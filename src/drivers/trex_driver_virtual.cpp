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

#include "trex_driver_virtual.h"
#include "trex_driver_defines.h"


std::string CTRexExtendedDriverMlnx4::mlx4_so_str = "";

std::string& get_mlx4_so_string(void) {
    return CTRexExtendedDriverMlnx4::mlx4_so_str;
}


int CTRexExtendedDriverVirtBase::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE_1G);
}

void CTRexExtendedDriverVirtBase::get_dpdk_drv_params(CTrexDpdkParams &p) {
    p.rx_data_q_num = 1;
    p.rx_drop_q_num = 0;
    p.rx_desc_num_data_q = RX_DESC_NUM_DATA_Q_VM;
    p.rx_desc_num_drop_q = RX_DESC_NUM_DROP_Q;
    p.tx_desc_num = TX_DESC_NUM;
    p.rx_mbuf_type = MBUF_2048;
}

void CTRexExtendedDriverAfPacket::get_dpdk_drv_params(CTrexDpdkParams &p) {
    CTRexExtendedDriverVirtBase::get_dpdk_drv_params(p);
    p.rx_mbuf_type = MBUF_9k;
}

void CTRexExtendedDriverVirtBase::update_configuration(port_cfg_t * cfg) {
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
}

int CTRexExtendedDriverVirtBase::configure_rx_filter_rules(CPhyEthIF * _if){
    return (0);
}

void CTRexExtendedDriverVirtBase::clear_extended_stats(CPhyEthIF * _if){
    repid_t repid =_if->get_repid();
    rte_eth_stats_reset(repid);
}

int CTRexExtendedDriverVirtBase::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    return (0);
}

int CTRexExtendedDriverVirtBase::wait_for_stable_link(){
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverVirtBase::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
    assert (parser);
    return parser;
}

void CTRexExtendedDriverMlnx4::update_configuration(port_cfg_t * cfg) {
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

void CTRexExtendedDriverVirtio::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    rte_eth_rxmode *rxmode = &cfg->m_port_conf.rxmode;
    rxmode->offloads &= ~DEV_RX_OFFLOAD_SCATTER;
    if ( get_is_tcp_mode() ) {
        rxmode->offloads |= DEV_RX_OFFLOAD_TCP_LRO;
    }
}


bool CTRexExtendedDriverVirtio::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

bool CTRexExtendedDriverVmxnet3::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

bool CTRexExtendedDriverAfPacket::get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

bool CTRexExtendedDriverBaseE1000::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 0, 4);
}

void CTRexExtendedDriverBaseE1000::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    // We configure hardware not to strip CRC. Then DPDK driver removes the CRC.
    // If configuring "hardware" to remove CRC, due to bug in ESXI e1000 emulation, we got packets with CRC.
    cfg->m_port_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_CRC_STRIP;
}

void CTRexExtendedDriverVmxnet3::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
    if ( get_is_tcp_mode() ) {
        cfg->m_port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_TCP_LRO;
    }
    cfg->m_port_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_CRC_STRIP;
}

void CTRexExtendedDriverAfPacket::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_port_conf.rxmode.max_rx_pkt_len = 1514;
    cfg->m_port_conf.rxmode.offloads = 0;
}

///////////////////////////////////////////////////////// VF
void CTRexExtendedDriverI40evf::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

