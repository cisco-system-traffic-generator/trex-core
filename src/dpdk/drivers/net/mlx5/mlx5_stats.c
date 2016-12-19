/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"


#include <linux/ethtool.h>
#include <linux/sockios.h>

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 */


static void
mlx5_stats_read_hw(struct rte_eth_dev *dev,
                struct rte_eth_stats *stats){
    struct priv *priv = mlx5_get_priv(dev);
    struct mlx5_stats_priv * lps = &priv->m_stats;
    unsigned int i;

    struct rte_eth_stats tmp = {0};
    struct ethtool_stats    *et_stats   = (struct ethtool_stats    *)lps->et_stats;
    struct ifreq ifr;

    et_stats->cmd = ETHTOOL_GSTATS;
    et_stats->n_stats = lps->n_stats;

    ifr.ifr_data = (caddr_t) et_stats;

    if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) { 
        WARN("unable to get statistic values for mlnx5 "); 
    }

    tmp.ibytes += et_stats->data[lps->inx_rx_vport_unicast_bytes] +
                  et_stats->data[lps->inx_rx_vport_multicast_bytes] +
                  et_stats->data[lps->inx_rx_vport_broadcast_bytes];

    tmp.ipackets += et_stats->data[lps->inx_rx_vport_unicast_packets] +
                et_stats->data[lps->inx_rx_vport_multicast_packets] +
                et_stats->data[lps->inx_rx_vport_broadcast_packets];

    tmp.ierrors += 	(et_stats->data[lps->inx_rx_wqe_err] +
                    et_stats->data[lps->inx_rx_crc_errors_phy] +
                    et_stats->data[lps->inx_rx_in_range_len_errors_phy] +
                    et_stats->data[lps->inx_rx_symbol_err_phy]);

    tmp.obytes += et_stats->data[lps->inx_tx_vport_unicast_bytes] +
                  et_stats->data[lps->inx_tx_vport_multicast_bytes] +
                  et_stats->data[lps->inx_tx_vport_broadcast_bytes];

    tmp.opackets += (et_stats->data[lps->inx_tx_vport_unicast_packets] +
                     et_stats->data[lps->inx_tx_vport_multicast_packets] +
                     et_stats->data[lps->inx_tx_vport_broadcast_packets]);

    tmp.oerrors += et_stats->data[lps->inx_tx_errors_phy];

    /* SW Rx */
    for (i = 0; (i != priv->rxqs_n); ++i) {
        struct rxq *rxq = (*priv->rxqs)[i];
        if (rxq) {
            tmp.imissed += rxq->stats.idropped;
            tmp.rx_nombuf += rxq->stats.rx_nombuf;
        }
    }

    /*SW Tx */
    for (i = 0; (i != priv->txqs_n); ++i) {
        struct txq *txq = (*priv->txqs)[i];
        if (txq) {
            tmp.oerrors += txq->stats.odropped;
        }
    }

    *stats =tmp;
}

void
mlx5_stats_free(struct rte_eth_dev *dev)
{
    struct priv *priv = mlx5_get_priv(dev);
    struct mlx5_stats_priv * lps = &priv->m_stats;

    if ( lps->et_stats ){
        free(lps->et_stats);
        lps->et_stats=0;
    }
}


static void
mlx5_stats_init(struct rte_eth_dev *dev)
{
    struct priv *priv = mlx5_get_priv(dev);
    struct mlx5_stats_priv * lps = &priv->m_stats;
    struct rte_eth_stats tmp = {0};

    unsigned int i;
    unsigned int idx;
    char ifname[IF_NAMESIZE];
    struct ifreq ifr;

    struct ethtool_stats    *et_stats   = NULL;
    struct ethtool_drvinfo drvinfo;
    struct ethtool_gstrings *strings = NULL;
    unsigned int n_stats, sz_str, sz_stats;

    if (priv_get_ifname(priv, &ifname)) {
            WARN("unable to get interface name");
            return;
    }
    /* How many statistics are available ? */
    drvinfo.cmd = ETHTOOL_GDRVINFO;
    ifr.ifr_data = (caddr_t) &drvinfo;
    if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) {
            WARN("unable to get driver info for %s", ifname);
            return;
    }

    n_stats = drvinfo.n_stats;
    if (n_stats < 1) {
            WARN("no statistics available for %s", ifname);
            return;
    }
    lps->n_stats = n_stats;

    /* Allocate memory to grab stat names and values */ 
    sz_str = n_stats * ETH_GSTRING_LEN; 
    sz_stats = n_stats * sizeof(uint64_t); 
    strings = calloc(1, sz_str + sizeof(struct ethtool_gstrings)); 
    if (!strings) { 
        WARN("unable to allocate memory for strings"); 
        return;
    } 

    et_stats = calloc(1, sz_stats + sizeof(struct ethtool_stats)); 
    if (!et_stats) { 
        free(strings);
        WARN("unable to allocate memory for stats"); 
    } 

    strings->cmd = ETHTOOL_GSTRINGS; 
    strings->string_set = ETH_SS_STATS; 
    strings->len = n_stats; 
    ifr.ifr_data = (caddr_t) strings; 
    if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) { 
        WARN("unable to get statistic names for %s", ifname); 
        free(strings);
        free(et_stats);
        return;
    } 

    for (i = 0; (i != n_stats); ++i) {

            const char * curr_string = (const char*) &(strings->data[i * ETH_GSTRING_LEN]);

            if (!strcmp("rx_vport_unicast_bytes", curr_string)) lps->inx_rx_vport_unicast_bytes = i;
            if (!strcmp("rx_vport_multicast_bytes", curr_string)) lps->inx_rx_vport_multicast_bytes = i;
            if (!strcmp("rx_vport_broadcast_bytes", curr_string)) lps->inx_rx_vport_broadcast_bytes = i;

            if (!strcmp("rx_vport_unicast_packets", curr_string)) lps->inx_rx_vport_unicast_packets = i;
            if (!strcmp("rx_vport_multicast_packets", curr_string)) lps->inx_rx_vport_multicast_packets = i;
            if (!strcmp("rx_vport_broadcast_packets", curr_string)) lps->inx_rx_vport_broadcast_packets = i;

            if (!strcmp("tx_vport_unicast_bytes", curr_string)) lps->inx_tx_vport_unicast_bytes = i;
            if (!strcmp("tx_vport_multicast_bytes", curr_string)) lps->inx_tx_vport_multicast_bytes = i;
            if (!strcmp("tx_vport_broadcast_bytes", curr_string)) lps->inx_tx_vport_broadcast_bytes = i;

            if (!strcmp("tx_vport_unicast_packets", curr_string)) lps->inx_tx_vport_unicast_packets = i;
            if (!strcmp("tx_vport_multicast_packets", curr_string)) lps->inx_tx_vport_multicast_packets = i;
            if (!strcmp("tx_vport_broadcast_packets", curr_string)) lps->inx_tx_vport_broadcast_packets = i;

            if (!strcmp("rx_wqe_err", curr_string)) lps->inx_rx_wqe_err = i;
            if (!strcmp("rx_crc_errors_phy", curr_string)) lps->inx_rx_crc_errors_phy = i;
            if (!strcmp("rx_in_range_len_errors_phy", curr_string)) lps->inx_rx_in_range_len_errors_phy = i;
            if (!strcmp("rx_symbol_err_phy", curr_string)) lps->inx_rx_symbol_err_phy = i;

            if (!strcmp("tx_errors_phy", curr_string)) lps->inx_tx_errors_phy = i;
    }

    lps->et_stats =(void *)et_stats;

    if (!lps->inx_rx_vport_unicast_bytes ||
    !lps->inx_rx_vport_multicast_bytes ||
    !lps->inx_rx_vport_broadcast_bytes || 
    !lps->inx_rx_vport_unicast_packets ||
    !lps->inx_rx_vport_multicast_packets ||
    !lps->inx_rx_vport_broadcast_packets ||
    !lps->inx_tx_vport_unicast_bytes || 
    !lps->inx_tx_vport_multicast_bytes ||
    !lps->inx_tx_vport_broadcast_bytes ||
    !lps->inx_tx_vport_unicast_packets ||
    !lps->inx_tx_vport_multicast_packets ||
    !lps->inx_tx_vport_broadcast_packets ||
    !lps->inx_rx_wqe_err ||
    !lps->inx_rx_crc_errors_phy ||
    !lps->inx_rx_in_range_len_errors_phy) {
        WARN("Counters are not recognized %s", ifname);
        return;
    }

    mlx5_stats_read_hw(dev,&tmp);

    /* copy yo shadow at first time */
    lps->m_shadow = tmp;

    free(strings);
}


static void
mlx5_stats_diff(struct rte_eth_stats *a,
                struct rte_eth_stats *b,
                struct rte_eth_stats *c){
    #define MLX5_DIFF(cnt) { a->cnt = (b->cnt - c->cnt);  }

    MLX5_DIFF(ipackets);
    MLX5_DIFF(opackets); 
    MLX5_DIFF(ibytes); 
    MLX5_DIFF(obytes);
    MLX5_DIFF(imissed);

    MLX5_DIFF(ierrors); 
    MLX5_DIFF(oerrors); 
    MLX5_DIFF(rx_nombuf);
}

 
void
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = mlx5_get_priv(dev);

    struct mlx5_stats_priv * lps = &priv->m_stats;
    priv_lock(priv);

    if (lps->et_stats == NULL) {
        mlx5_stats_init(dev);
    }
    struct rte_eth_stats tmp = {0};

    mlx5_stats_read_hw(dev,&tmp);

    mlx5_stats_diff(stats,
                    &tmp,
                    &lps->m_shadow);

	priv_unlock(priv);
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_stats_reset(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
    struct mlx5_stats_priv * lps = &priv->m_stats;

    priv_lock(priv);

    if (lps->et_stats == NULL) {
        mlx5_stats_init(dev);
    }
    struct rte_eth_stats tmp = {0};


    mlx5_stats_read_hw(dev,&tmp);

    /* copy to shadow */
    lps->m_shadow = tmp;

	priv_unlock(priv);
}
