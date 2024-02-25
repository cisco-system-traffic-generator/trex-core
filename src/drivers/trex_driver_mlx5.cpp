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

CTRexExtendedDriverBaseMlnx5G::CTRexExtendedDriverBaseMlnx5G() {
    //m_cap = tdCAP_MULTI_QUE | tdCAP_ONE_QUE  | TREX_DRV_CAP_MAC_ADDR_CHG ;
    m_cap = tdCAP_ALL  | TREX_DRV_CAP_MAC_ADDR_CHG ;
    for ( int i=0; i<TREX_MAX_PORTS; i++ ) {
        m_port_xstats[i] = {0};
    }
}

TRexPortAttr* CTRexExtendedDriverBaseMlnx5G::create_port_attr(tvpid_t tvpid,repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, false, false, true, false, true);
}

std::string& get_mlx5_so_string(void) {
    return CTRexExtendedDriverBaseMlnx5G::mlx5_so_str;
}

bool CTRexExtendedDriverBaseMlnx5G::is_support_for_rx_scatter_gather(){
    return false;
}


int CTRexExtendedDriverBaseMlnx5G::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
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
    cfg->m_port_conf.fdir_conf.pballoc = RTE_ETH_FDIR_PBALLOC_64K;
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
    return(0);
}

bool CTRexExtendedDriverBaseMlnx5G::get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
    uint16_t repid = _if->get_repid();
    xstats_struct* xstats_struct = &m_port_xstats[repid];


    if ( !xstats_struct->init ) {
        // Get total count of stats
        xstats_struct->total_count = rte_eth_xstats_get(repid, NULL, 0);
        assert(xstats_struct->total_count>0);
        const auto xstats_names = new struct rte_eth_xstat_name[xstats_struct->total_count];
        assert(xstats_names != 0);
        rte_eth_xstats_get_names(repid, xstats_names, xstats_struct->total_count);

        for (uint16_t i = 0; i < xstats_struct->total_count; i++) {
            xstats_struct->name_to_id[std::string(xstats_names[i].name)] = i;
        }

        // Assert that DPDK name for the required counters haven't changed
        // opackets
        assert(xstats_struct->name_to_id.count("tx_unicast_packets") == 1);
        assert(xstats_struct->name_to_id.count("tx_multicast_packets") == 1);
        assert(xstats_struct->name_to_id.count("tx_broadcast_packets") == 1);

        // ipackets
        assert(xstats_struct->name_to_id.count("rx_unicast_packets") == 1);
        assert(xstats_struct->name_to_id.count("rx_multicast_packets") == 1);
        assert(xstats_struct->name_to_id.count("rx_broadcast_packets") == 1);

        // obytes
        assert(xstats_struct->name_to_id.count("tx_unicast_bytes") == 1);
        assert(xstats_struct->name_to_id.count("tx_multicast_bytes") == 1);
        assert(xstats_struct->name_to_id.count("tx_broadcast_bytes") == 1);

        // ibytes
        assert(xstats_struct->name_to_id.count("rx_unicast_bytes") == 1);
        assert(xstats_struct->name_to_id.count("rx_multicast_bytes") == 1);
        assert(xstats_struct->name_to_id.count("rx_broadcast_bytes") == 1);

        // imissed
        assert(xstats_struct->name_to_id.count("rx_missed_errors") == 1);

        // rx_nombuf
        assert(xstats_struct->name_to_id.count("rx_mbuf_allocation_errors") == 1);

        // ierrors
        assert(xstats_struct->name_to_id.count("rx_wqe_errors") == 1);
        assert(xstats_struct->name_to_id.count("rx_phy_crc_errors") == 1);
        assert(xstats_struct->name_to_id.count("rx_phy_in_range_len_errors") == 1);
        assert(xstats_struct->name_to_id.count("rx_phy_symbol_errors") == 1);
        assert(xstats_struct->name_to_id.count("rx_out_of_buffer") == 1);

        // oerrors
        assert(xstats_struct->name_to_id.count("tx_phy_errors") == 1);

        delete[] xstats_names;
    }

    rte_eth_xstat xstats[xstats_struct->total_count];
    rte_eth_stats *prev_stats = &stats->m_prev_stats;

    /* fetch stats */
    const int ret = rte_eth_xstats_get(repid, xstats, xstats_struct->total_count);
    assert(ret<=xstats_struct->total_count);


    uint64_t opackets = xstats[xstats_struct->name_to_id.at("tx_unicast_packets")].value +
                            xstats[xstats_struct->name_to_id.at("tx_multicast_packets")].value +
                            xstats[xstats_struct->name_to_id.at("tx_broadcast_packets")].value;
    uint64_t ipackets = xstats[xstats_struct->name_to_id.at("rx_unicast_packets")].value +
                            xstats[xstats_struct->name_to_id.at("rx_multicast_packets")].value +
                            xstats[xstats_struct->name_to_id.at("rx_broadcast_packets")].value;
    uint64_t obytes = xstats[xstats_struct->name_to_id.at("tx_unicast_bytes")].value +
                            xstats[xstats_struct->name_to_id.at("tx_multicast_bytes")].value +
                            xstats[xstats_struct->name_to_id.at("tx_broadcast_bytes")].value;
    uint64_t ibytes = xstats[xstats_struct->name_to_id.at("rx_unicast_bytes")].value +
                            xstats[xstats_struct->name_to_id.at("rx_multicast_bytes")].value +
                            xstats[xstats_struct->name_to_id.at("rx_broadcast_bytes")].value;
    uint64_t imissed = xstats[xstats_struct->name_to_id.at("rx_missed_errors")].value;
    uint64_t rx_nombuf = xstats[xstats_struct->name_to_id.at("rx_mbuf_allocation_errors")].value;
    uint64_t ierrors = xstats[xstats_struct->name_to_id.at("rx_wqe_errors")].value +
                           xstats[xstats_struct->name_to_id.at("rx_phy_crc_errors")].value +
                           xstats[xstats_struct->name_to_id.at("rx_phy_in_range_len_errors")].value +
                           xstats[xstats_struct->name_to_id.at("rx_phy_symbol_errors")].value +
                           xstats[xstats_struct->name_to_id.at("rx_out_of_buffer")].value;
    uint64_t oerrors = xstats[xstats_struct->name_to_id.at("tx_phy_errors")].value;

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

void CTRexExtendedDriverBaseMlnx5G::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
    num_counters = 127; //With MAX_FLOW_STATS we saw packet failures in rx_test. Need to check.
    base_ip_id = IP_ID_RESERVE_BASE;
}

