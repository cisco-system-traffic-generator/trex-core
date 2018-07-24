/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <inttypes.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_ethdev_driver.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

struct mlx5_counter_ctrl {
	/* Name of the counter. */
	char dpdk_name[RTE_ETH_XSTATS_NAME_SIZE];
	/* Name of the counter on the device table. */
	char ctr_name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t ib:1; /**< Nonzero for IB counters. */
};

static const struct mlx5_counter_ctrl mlx5_counters_init[] = {
	{
		.dpdk_name = "rx_port_unicast_bytes",
		.ctr_name = "rx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "rx_port_multicast_bytes",
		.ctr_name = "rx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "rx_port_broadcast_bytes",
		.ctr_name = "rx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "rx_port_unicast_packets",
		.ctr_name = "rx_vport_unicast_packets",
	},
	{
		.dpdk_name = "rx_port_multicast_packets",
		.ctr_name = "rx_vport_multicast_packets",
	},
	{
		.dpdk_name = "rx_port_broadcast_packets",
		.ctr_name = "rx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "tx_port_unicast_bytes",
		.ctr_name = "tx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "tx_port_multicast_bytes",
		.ctr_name = "tx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "tx_port_broadcast_bytes",
		.ctr_name = "tx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "tx_port_unicast_packets",
		.ctr_name = "tx_vport_unicast_packets",
	},
	{
		.dpdk_name = "tx_port_multicast_packets",
		.ctr_name = "tx_vport_multicast_packets",
	},
	{
		.dpdk_name = "tx_port_broadcast_packets",
		.ctr_name = "tx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "rx_wqe_err",
		.ctr_name = "rx_wqe_err",
	},
	{
		.dpdk_name = "rx_crc_errors_phy",
		.ctr_name = "rx_crc_errors_phy",
	},
	{
		.dpdk_name = "rx_in_range_len_errors_phy",
		.ctr_name = "rx_in_range_len_errors_phy",
	},
	{
		.dpdk_name = "rx_symbol_err_phy",
		.ctr_name = "rx_symbol_err_phy",
	},
	{
		.dpdk_name = "tx_errors_phy",
		.ctr_name = "tx_errors_phy",
	},
	{
		.dpdk_name = "rx_out_of_buffer",
		.ctr_name = "out_of_buffer",
		.ib = 1,
	},
	{
		.dpdk_name = "tx_packets_phy",
		.ctr_name = "tx_packets_phy",
	},
	{
		.dpdk_name = "rx_packets_phy",
		.ctr_name = "rx_packets_phy",
	},
	{
		.dpdk_name = "tx_bytes_phy",
		.ctr_name = "tx_bytes_phy",
	},
	{
		.dpdk_name = "rx_bytes_phy",
		.ctr_name = "rx_bytes_phy",
	},
};

static const unsigned int xstats_n = RTE_DIM(mlx5_counters_init);

/**
 * Read device counters table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Counters table output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_read_dev_counters(struct rte_eth_dev *dev, uint64_t *stats)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	struct ifreq ifr;
	unsigned int stats_sz = xstats_ctrl->stats_n * sizeof(uint64_t);
	unsigned char et_stat_buf[sizeof(struct ethtool_stats) + stats_sz];
	struct ethtool_stats *et_stats = (struct ethtool_stats *)et_stat_buf;
	int ret;

	et_stats->cmd = ETHTOOL_GSTATS;
	et_stats->n_stats = xstats_ctrl->stats_n;
	ifr.ifr_data = (caddr_t)et_stats;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u unable to read statistic values from device",
			dev->data->port_id);
		return ret;
	}
	for (i = 0; i != xstats_n; ++i) {
		if (mlx5_counters_init[i].ib) {
			FILE *file;
			MKSTR(path, "%s/ports/1/hw_counters/%s",
			      priv->ibdev_path,
			      mlx5_counters_init[i].ctr_name);

			file = fopen(path, "rb");
			if (file) {
				int n = fscanf(file, "%" SCNu64, &stats[i]);

				fclose(file);
				if (n != 1)
					stats[i] = 0;
			}
		} else {
			stats[i] = (uint64_t)
				et_stats->data[xstats_ctrl->dev_table_idx[i]];
		}
	}
	return 0;
}

/**
 * Query the number of statistics provided by ETHTOOL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Number of statistics on success, negative errno value otherwise and
 *   rte_errno is set.
 */
static int
mlx5_ethtool_get_stats_n(struct rte_eth_dev *dev) {
	struct ethtool_drvinfo drvinfo;
	struct ifreq ifr;
	int ret;

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to query number of statistics",
			dev->data->port_id);
		return ret;
	}
	return drvinfo.n_stats;
}

/**
 * Init the structures to read device counters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_xstats_init(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	unsigned int j;
	struct ifreq ifr;
	struct ethtool_gstrings *strings = NULL;
	unsigned int dev_stats_n;
	unsigned int str_sz;
	int ret;

	ret = mlx5_ethtool_get_stats_n(dev);
	if (ret < 0) {
		DRV_LOG(WARNING, "port %u no extended statistics available",
			dev->data->port_id);
		return;
	}
	dev_stats_n = ret;
	xstats_ctrl->stats_n = dev_stats_n;
	/* Allocate memory to grab stat names and values. */
	str_sz = dev_stats_n * ETH_GSTRING_LEN;
	strings = (struct ethtool_gstrings *)
		  rte_malloc("xstats_strings",
			     str_sz + sizeof(struct ethtool_gstrings), 0);
	if (!strings) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for xstats",
		     dev->data->port_id);
		return;
	}
	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_STATS;
	strings->len = dev_stats_n;
	ifr.ifr_data = (caddr_t)strings;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get statistic names",
			dev->data->port_id);
		goto free;
	}
	for (j = 0; j != xstats_n; ++j)
		xstats_ctrl->dev_table_idx[j] = dev_stats_n;
	for (i = 0; i != dev_stats_n; ++i) {
		const char *curr_string = (const char *)
			&strings->data[i * ETH_GSTRING_LEN];

		for (j = 0; j != xstats_n; ++j) {
			if (!strcmp(mlx5_counters_init[j].ctr_name,
				    curr_string)) {
				xstats_ctrl->dev_table_idx[j] = i;
				break;
			}
		}
	}
	for (j = 0; j != xstats_n; ++j) {
		if (mlx5_counters_init[j].ib)
			continue;
		if (xstats_ctrl->dev_table_idx[j] >= dev_stats_n) {
			DRV_LOG(WARNING,
				"port %u counter \"%s\" is not recognized",
				dev->data->port_id,
				mlx5_counters_init[j].dpdk_name);
			goto free;
		}
	}
	/* Copy to base at first time. */
	assert(xstats_n <= MLX5_MAX_XSTATS);
	ret = mlx5_read_dev_counters(dev, xstats_ctrl->base);
	if (ret)
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
free:
	rte_free(strings);
}

/**
 * DPDK callback to get extended device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] stats
 *   Pointer to rte extended stats table.
 * @param n
 *   The size of the stats table.
 *
 * @return
 *   Number of extended stats on success and stats is filled,
 *   negative on error and rte_errno is set.
 */
int
mlx5_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
		unsigned int n)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	uint64_t counters[n];

	if (n >= xstats_n && stats) {
		struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
		int stats_n;
		int ret;

		stats_n = mlx5_ethtool_get_stats_n(dev);
		if (stats_n < 0)
			return stats_n;
		if (xstats_ctrl->stats_n != stats_n)
			mlx5_xstats_init(dev);
		ret = mlx5_read_dev_counters(dev, counters);
		if (ret)
			return ret;
		for (i = 0; i != xstats_n; ++i) {
			stats[i].id = i;
			stats[i].value = (counters[i] - xstats_ctrl->base[i]);
		}
	}
	return xstats_n;
}

#ifndef TREX_PATCH

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_eth_stats tmp = {0};
	unsigned int i;
	unsigned int idx;

	/* Add software counters. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		idx = rxq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_ipackets[idx] += rxq->stats.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes;
#endif
			tmp.q_errors[idx] += (rxq->stats.idropped +
					      rxq->stats.rx_nombuf);
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.ipackets += rxq->stats.ipackets;
		tmp.ibytes += rxq->stats.ibytes;
#endif
		tmp.ierrors += rxq->stats.idropped;
		tmp.rx_nombuf += rxq->stats.rx_nombuf;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct mlx5_txq_data *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
#endif
			tmp.q_errors[idx] += txq->stats.oerrors;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		tmp.opackets += txq->stats.opackets;
		tmp.obytes += txq->stats.obytes;
#endif
		tmp.oerrors += txq->stats.oerrors;
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: retrieve and add hardware counters. */
#endif
	*stats = tmp;
	return 0;
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
	unsigned int i;
	unsigned int idx;

	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		idx = (*priv->rxqs)[i]->stats.idx;
		(*priv->rxqs)[i]->stats =
			(struct mlx5_rxq_stats){ .idx = idx };
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		if ((*priv->txqs)[i] == NULL)
			continue;
		idx = (*priv->txqs)[i]->stats.idx;
		(*priv->txqs)[i]->stats =
			(struct mlx5_txq_stats){ .idx = idx };
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: reset hardware counters. */
#endif
}

#endif

#ifdef TREX_PATCH

static void
mlx5_stats_read_hw(struct rte_eth_dev *dev,
                   struct rte_eth_stats *stats,
                   int init){
    struct priv *priv = dev->data->dev_private;
    struct mlx5_stats_priv * lps = &priv->m_stats;
    unsigned int i;

    struct rte_eth_stats tmp = {0};
    struct ethtool_stats    *et_stats   = (struct ethtool_stats    *)lps->et_stats;
    struct ifreq ifr;

    et_stats->cmd = ETHTOOL_GSTATS;
    et_stats->n_stats = lps->n_stats;

    ifr.ifr_data = (caddr_t) et_stats;

    if (mlx5_ifreq(dev, SIOCETHTOOL, &ifr) != 0) { 
        WARN("unable to get statistic values for mlnx5 "); 
    }

    tmp.ibytes += et_stats->data[lps->inx_rx_vport_unicast_bytes] +
                  et_stats->data[lps->inx_rx_vport_multicast_bytes] +
                  et_stats->data[lps->inx_rx_vport_broadcast_bytes];

    if (init) {
        tmp.ipackets += et_stats->data[lps->inx_rx_vport_unicast_packets] +
                    et_stats->data[lps->inx_rx_vport_multicast_packets] +
                    et_stats->data[lps->inx_rx_vport_broadcast_packets];

        /* woprkaround CX-4 Lx does not implement tx_vport_unicast_packets and return always ZERO */
        if ( et_stats->data[lps->inx_tx_vport_unicast_packets]==0 ) {
            lps->cx_4_workaround=1;
        }

        if ( lps->cx_4_workaround == 0 ) {
            tmp.opackets += (et_stats->data[lps->inx_tx_vport_unicast_packets] +
                             et_stats->data[lps->inx_tx_vport_multicast_packets] +
                             et_stats->data[lps->inx_tx_vport_broadcast_packets]);
        }else{
            tmp.opackets += (et_stats->data[lps->inx_tx_packets_phy] );
        }

        lps->m_t_opackets =0;
        lps->m_t_ipackets =0;

        lps->m_old_ipackets = (uint32_t)tmp.ipackets;
        lps->m_old_opackets = (uint32_t)tmp.opackets;


    }else{

        /* diff of 32bit */
        uint32_t ipackets = (uint32_t)(et_stats->data[lps->inx_rx_vport_unicast_packets] +
                                       et_stats->data[lps->inx_rx_vport_multicast_packets] +
                                       et_stats->data[lps->inx_rx_vport_broadcast_packets]);

        lps->m_t_ipackets +=(ipackets - lps->m_old_ipackets);

        tmp.ipackets = lps->m_t_ipackets;

        lps->m_old_ipackets = ipackets;


        uint32_t opackets;
        if ( lps->cx_4_workaround == 0 ) {

        /* diff of 32bit */ 
          opackets = (uint32_t)(et_stats->data[lps->inx_tx_vport_unicast_packets] +
                                       et_stats->data[lps->inx_tx_vport_multicast_packets] +
                                       et_stats->data[lps->inx_tx_vport_broadcast_packets]);
        }else{

          opackets = (uint32_t)(et_stats->data[lps->inx_tx_packets_phy] );

        }

        lps->m_t_opackets +=(opackets - lps->m_old_opackets);
        tmp.opackets = lps->m_t_opackets;
        lps->m_old_opackets =opackets;

    }

    tmp.ierrors += 	(et_stats->data[lps->inx_rx_wqe_err] +
                    et_stats->data[lps->inx_rx_crc_errors_phy] +
                    et_stats->data[lps->inx_rx_in_range_len_errors_phy] +
                    et_stats->data[lps->inx_rx_symbol_err_phy]);

    if ( lps->cx_4_workaround == 0 ) {
        tmp.obytes += et_stats->data[lps->inx_tx_vport_unicast_bytes] +
                      et_stats->data[lps->inx_tx_vport_multicast_bytes] +
                      et_stats->data[lps->inx_tx_vport_broadcast_bytes];
    }else{
        tmp.obytes += (et_stats->data[lps->inx_tx_bytes_phy] - (tmp.opackets*4)) ;
    }


    tmp.oerrors += et_stats->data[lps->inx_tx_errors_phy];

    /* SW Rx */
    for (i = 0; (i != priv->rxqs_n); ++i) {
        struct mlx5_rxq_data *rxq = (*priv->rxqs)[i];
        if (rxq) {
            tmp.imissed += rxq->stats.idropped;
            tmp.rx_nombuf += rxq->stats.rx_nombuf;
        }
    }

    /*SW Tx */
    for (i = 0; (i != priv->txqs_n); ++i) {
        struct mlx5_txq_data *txq = (*priv->txqs)[i];
        if (txq) {
            tmp.oerrors += txq->stats.odropped;
        }
    }

    *stats =tmp;
}

void
mlx5_stats_free(struct rte_eth_dev *dev)
{
    struct priv *priv = dev->data->dev_private;
    struct mlx5_stats_priv * lps = &priv->m_stats;

    if ( lps->et_stats ){
        free(lps->et_stats);
        lps->et_stats=0;
    }
}


static void
mlx5_stats_init(struct rte_eth_dev *dev)
{
    struct priv *priv = dev->data->dev_private;
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

    if (mlx5_get_ifname(dev, &ifname)) {
            WARN("unable to get interface name");
            return;
    }
    /* How many statistics are available ? */
    drvinfo.cmd = ETHTOOL_GDRVINFO;
    ifr.ifr_data = (caddr_t) &drvinfo;
    if (mlx5_ifreq(dev, SIOCETHTOOL, &ifr) != 0) {
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
    if (mlx5_ifreq(dev, SIOCETHTOOL, &ifr) != 0) { 
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

            if (!strcmp("tx_packets_phy", curr_string)) lps->inx_tx_packets_phy = i;
            if (!strcmp("tx_bytes_phy", curr_string)) lps->inx_tx_bytes_phy = i;
            lps->cx_4_workaround=0;

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
    !lps->inx_rx_in_range_len_errors_phy||
    !lps->inx_tx_packets_phy||
    !lps->inx_tx_bytes_phy) {
        WARN("Counters are not recognized %s", ifname);
        return;
    }

    mlx5_stats_read_hw(dev,&tmp,1);



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


int mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = dev->data->dev_private;

    struct mlx5_stats_priv * lps = &priv->m_stats;

    if (lps->et_stats == NULL) {
        mlx5_stats_init(dev);
    }
    struct rte_eth_stats tmp = {0};

    mlx5_stats_read_hw(dev,&tmp,0);

    mlx5_stats_diff(stats,
                    &tmp,
                    &lps->m_shadow);

    return(0);
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


    if (lps->et_stats == NULL) {
        mlx5_stats_init(dev);
    }
    struct rte_eth_stats tmp = {0};


    mlx5_stats_read_hw(dev,&tmp,0);

    /* copy to shadow */
    lps->m_shadow = tmp;
}



#endif



/**
 * DPDK callback to clear device extended statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_xstats_reset(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	int stats_n;
	unsigned int i;
	unsigned int n = xstats_n;
	uint64_t counters[n];
	int ret;

	stats_n = mlx5_ethtool_get_stats_n(dev);
	if (stats_n < 0) {
		DRV_LOG(ERR, "port %u cannot get stats: %s", dev->data->port_id,
			strerror(-stats_n));
		return;
	}
	if (xstats_ctrl->stats_n != stats_n)
		mlx5_xstats_init(dev);
	ret = mlx5_read_dev_counters(dev, counters);
	if (ret) {
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
		return;
	}
	for (i = 0; i != n; ++i)
		xstats_ctrl->base[i] = counters[i];
}

/**
 * DPDK callback to retrieve names of extended device statistics
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 * @param n
 *   Number of names.
 *
 * @return
 *   Number of xstats names.
 */
int
mlx5_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
		      struct rte_eth_xstat_name *xstats_names, unsigned int n)
{
	unsigned int i;

	if (n >= xstats_n && xstats_names) {
		for (i = 0; i != xstats_n; ++i) {
			strncpy(xstats_names[i].name,
				mlx5_counters_init[i].dpdk_name,
				RTE_ETH_XSTATS_NAME_SIZE);
			xstats_names[i].name[RTE_ETH_XSTATS_NAME_SIZE - 1] = 0;
		}
	}
	return xstats_n;
}
