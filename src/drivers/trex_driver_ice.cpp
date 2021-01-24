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

#include "trex_driver_ice.h"
#include "trex_driver_defines.h"

std::string CTRexExtendedDriverIce::ice_so_str = "";

CTRexExtendedDriverIce::CTRexExtendedDriverIce() {
    //m_cap = tdCAP_ALL | TREX_DRV_CAP_MAC_ADDR_CHG | TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN ;
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE  | TREX_DRV_CAP_MAC_ADDR_CHG |TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN ;

    for ( int i=0; i<TREX_MAX_PORTS; i++ ) {
        m_port_xstats[i] = {0};
    }
    m_filter_manager.set_hw_mode(fhtINTEL_NO_DOT1Q);
}

TRexPortAttr* CTRexExtendedDriverIce::create_port_attr(tvpid_t tvpid,repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, false, false, true, false, true);
}

std::string& get_ice_so_string(void) {
    return CTRexExtendedDriverIce::ice_so_str;
}

bool CTRexExtendedDriverIce::is_support_for_rx_scatter_gather(){
    return (true);
}


int CTRexExtendedDriverIce::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}


void CTRexExtendedDriverIce::clear_extended_stats(CPhyEthIF * _if){
    repid_t repid=_if->get_repid();
    rte_eth_stats_reset(repid);
}

void CTRexExtendedDriverIce::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
    cfg->m_port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
    cfg->m_port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
}

void CTRexExtendedDriverIce::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
    for (int i =0; i < len; i++) {
        stats[i] = 0;
    }
}

int CTRexExtendedDriverIce::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    /* not supported yet */
    return 0;
}

int CTRexExtendedDriverIce::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
    return 0;
}

bool CTRexExtendedDriverIce::get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

int CTRexExtendedDriverIce::wait_for_stable_link(){
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverIce::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

void CTRexExtendedDriverIce::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
    num_counters = 127; //With MAX_FLOW_STATS we saw packet failures in rx_test. Need to check.
    base_ip_id = IP_ID_RESERVE_BASE;
}

