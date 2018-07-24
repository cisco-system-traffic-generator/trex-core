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

bool CTRexExtendedDriverBaseMlnx5G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    return get_extended_stats_fixed(_if, stats, 4, 4);
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

