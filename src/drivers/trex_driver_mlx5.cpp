/*
  TRex team
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2015-2017 Cisco Systems, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "trex_driver_mlx5.h"
#include "trex_driver_defines.h"

std::string CTRexExtendedDriverBaseMlnx5G::mlx5_so_str = "";

std::string& get_mlx5_so_string(void) {
    return CTRexExtendedDriverBaseMlnx5G::mlx5_so_str;
}

int CTRexExtendedDriverBaseMlnx5G::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}

void CTRexExtendedDriverBaseMlnx5G::get_dpdk_drv_params(CTrexDpdkParams &p) {
    CTRexExtendedDriverBase::get_dpdk_drv_params(p);
    if (get_is_tcp_mode()){
        p.rx_mbuf_type = MBUF_9k; /* due to trex-481*/
    }
}


void CTRexExtendedDriverBaseMlnx5G::clear_extended_stats(CPhyEthIF * _if){
    repid_t repid=_if->get_repid();
    rte_eth_stats_reset(repid);
}

void CTRexExtendedDriverBaseMlnx5G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
    cfg->m_port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
    cfg->m_port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
}

void CTRexExtendedDriverBaseMlnx5G::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
    for (int i =0; i < len; i++) {
        stats[i] = 0;
    }
}

int CTRexExtendedDriverBaseMlnx5G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    /* not supported yet */
    return 0;
}

int CTRexExtendedDriverBaseMlnx5G::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
    repid_t repid=_if->get_repid();
    struct rte_eth_fdir_stats stat;
    int ret;

    ret = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_STATS, (void*)&stat);
    if (ret == 0) {
        if (fd)
            fprintf(fd, "Num filters on guarant poll:%d, best effort poll:%d\n", stat.guarant_cnt, stat.best_cnt);
        return (stat.guarant_cnt + stat.best_cnt);
    } else {
        if (fd)
            fprintf(fd, "Failed reading fdir statistics\n");
        return -1;
    }
}

bool CTRexExtendedDriverBaseMlnx5G::get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
    enum { // start of xstats array
        rx_good_packets,
        tx_good_packets,
        rx_good_bytes,
        tx_good_bytes,
        rx_missed_errors,
        rx_errors,
        tx_errors,
        rx_mbuf_allocation_errors,
        COUNT
    };
    enum { // end of xstats array
        rx_port_unicast_bytes,
        rx_port_multicast_bytes,
        rx_port_broadcast_bytes,
        rx_port_unicast_packets,
        rx_port_multicast_packets,
        rx_port_broadcast_packets,
        tx_port_unicast_bytes,
        tx_port_multicast_bytes,
        tx_port_broadcast_bytes,
        tx_port_unicast_packets,
        tx_port_multicast_packets,
        tx_port_broadcast_packets,
        rx_wqe_err,
        rx_crc_errors_phy,
        rx_in_range_len_errors_phy,
        rx_symbol_err_phy,
        tx_errors_phy,
        rx_out_of_buffer,
        tx_packets_phy,
        rx_packets_phy,
        tx_bytes_phy,
        rx_bytes_phy,
        XCOUNT
    };

    uint16_t repid = _if->get_repid();
    xstats_struct* xstats_struct = &m_port_xstats[repid];

    if ( !xstats_struct->init ) {
        // total_count = COUNT + per queue stats count + XCOUNT
        xstats_struct->total_count = rte_eth_xstats_get(repid, NULL, 0);
        assert(xstats_struct->total_count>=COUNT+XCOUNT);
    }

    struct rte_eth_xstat xstats_array[xstats_struct->total_count];
    struct rte_eth_xstat *xstats = &xstats_array[xstats_struct->total_count - XCOUNT];
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;

    /* fetch stats */
    int ret;
    ret = rte_eth_xstats_get(repid, xstats_array, xstats_struct->total_count);
    assert(ret==xstats_struct->total_count);

    uint32_t opackets = xstats[tx_port_unicast_packets].value +
                        xstats[tx_port_multicast_packets].value +
                        xstats[tx_port_broadcast_packets].value;
    uint32_t ipackets = xstats[rx_port_unicast_packets].value +
                        xstats[rx_port_multicast_packets].value +
                        xstats[rx_port_broadcast_packets].value;
    uint64_t obytes = xstats[tx_port_unicast_bytes].value +
                      xstats[tx_port_multicast_bytes].value +
                      xstats[tx_port_broadcast_bytes].value;
    uint64_t ibytes = xstats[rx_port_unicast_bytes].value +
                      xstats[rx_port_multicast_bytes].value +
                      xstats[rx_port_broadcast_bytes].value;
    uint64_t &imissed = xstats_array[rx_missed_errors].value;
    uint64_t &rx_nombuf = xstats_array[rx_mbuf_allocation_errors].value;
    uint64_t ierrors = xstats[rx_wqe_err].value +
                       xstats[rx_crc_errors_phy].value +
                       xstats[rx_in_range_len_errors_phy].value +
                       xstats[rx_symbol_err_phy].value +
                       xstats[rx_out_of_buffer].value;
    uint64_t &oerrors = xstats[tx_errors_phy].value;

    if ( !xstats_struct->init ) {
        xstats_struct->init = true;
    } else {
        // Packet counter on Connect5 is 40 bits, use 32 bit diffs
        uint32_t packet_diff;
        packet_diff = opackets - (uint32_t)prev_stats->opackets;
        stats->opackets += packet_diff;
        stats->obytes += obytes - prev_stats->obytes +
                         packet_diff * 4; // add FCS

        packet_diff = ipackets - (uint32_t)prev_stats->ipackets;
        stats->ipackets += packet_diff;
        stats->ibytes += ibytes - prev_stats->ibytes +
                         packet_diff * 4; // add FCS

        stats->ierrors += imissed - prev_stats->imissed +
                          rx_nombuf - prev_stats->rx_nombuf +
                          ierrors - prev_stats->ierrors;
        stats->rx_nombuf += rx_nombuf - prev_stats->rx_nombuf;
        stats->oerrors += oerrors - prev_stats->oerrors;
    }

    prev_stats->ipackets  = ipackets;
    prev_stats->opackets  = opackets;
    prev_stats->ibytes    = ibytes;
    prev_stats->obytes    = obytes;
    prev_stats->imissed   = imissed;
    prev_stats->rx_nombuf = rx_nombuf;
    prev_stats->ierrors   = ierrors;
    prev_stats->oerrors   = oerrors;

    return true;
}

int CTRexExtendedDriverBaseMlnx5G::wait_for_stable_link(){
    delay(20);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverBaseMlnx5G::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

