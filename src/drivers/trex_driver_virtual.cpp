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

TRexPortAttr* CTRexExtendedDriverVirtBase::create_port_attr(tvpid_t tvpid, repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, true, true, true, false, true);
}

TRexPortAttr* CTRexExtendedDriverIavf::create_port_attr(tvpid_t tvpid, repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, true, true, true, false, true);
}

TRexPortAttr* CTRexExtendedDriverIxgbevf::create_port_attr(tvpid_t tvpid, repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, true, true, false, false, true);
}

TRexPortAttr* CTRexExtendedDriverAfPacket::create_port_attr(tvpid_t tvpid, repid_t repid) {
    return new DpdkTRexPortAttr(tvpid, repid, true, true, true, false, false);
}



CTRexExtendedDriverMlnx4::CTRexExtendedDriverMlnx4() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE | TREX_DRV_CAP_MAC_ADDR_CHG ;
}

int CTRexExtendedDriverVirtBase::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE_1G);
}


void CTRexExtendedDriverVirtBase::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
    num_counters = MAX_FLOW_STATS;
    base_ip_id = IP_ID_RESERVE_BASE;
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


bool CTRexExtendedDriverVirtBase::is_override_dpdk_params(CTrexDpdkParamsOverride & dpdk_p){
    dpdk_p.rx_desc_num_data_q = RX_DESC_NUM_DATA_Q_VM;
    dpdk_p.rx_desc_num_drop_q = 0; /* zero means don't change */
    dpdk_p.rx_desc_num_dp_q   = RX_DESC_NUM_DATA_Q_VM;
    dpdk_p.tx_desc_num        = 0; /* zero means don't change */
    return (true);
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


CTRexExtendedDriverVirtio::CTRexExtendedDriverVirtio() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

void CTRexExtendedDriverVirtio::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    rte_eth_rxmode *rxmode = &cfg->m_port_conf.rxmode;
    rxmode->mtu = 2018;
    rxmode->offloads &= ~RTE_ETH_RX_OFFLOAD_SCATTER;
    if ( get_is_tcp_mode() ) {
        rxmode->offloads |= RTE_ETH_RX_OFFLOAD_TCP_LRO;
    }
}

bool CTRexExtendedDriverVirtio::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}


CTRexExtendedDriverVmxnet3::CTRexExtendedDriverVmxnet3() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

bool CTRexExtendedDriverVmxnet3::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

bool CTRexExtendedDriverAfPacket::get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

CTRexExtendedDriverBaseE1000::CTRexExtendedDriverBaseE1000() {
    // E1000 driver is only relevant in VM in our case
    m_cap = tdCAP_ONE_QUE;
}

bool CTRexExtendedDriverBaseE1000::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 0, 4);
}

void CTRexExtendedDriverBaseE1000::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    // We configure hardware not to strip CRC. Then DPDK driver removes the CRC.
    // If configuring "hardware" to remove CRC, due to bug in ESXI e1000 emulation, we got packets with CRC.
    //cfg->m_port_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_CRC_STRIP;
    // E1000 does not claim as supporting multi-segment send.
    cfg->tx_offloads.common_required &= ~RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
}

void CTRexExtendedDriverVmxnet3::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
    if ( get_is_tcp_mode() ) {
        cfg->m_port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TCP_LRO;
    }
}

CTRexExtendedDriverAfPacket::CTRexExtendedDriverAfPacket(){
    m_cap = tdCAP_ONE_QUE;
}

void CTRexExtendedDriverAfPacket::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_port_conf.rxmode.offloads = 0;
    // AF Packet does not claim as supporting multi-segment send.
    cfg->tx_offloads.common_required &= ~RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
}

CTRexExtendedDriverMemif::CTRexExtendedDriverMemif() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

TRexPortAttr* CTRexExtendedDriverMemif::create_port_attr(tvpid_t tvpid,repid_t repid){
    return new DpdkTRexPortAttr(tvpid, repid, true, false, true, false, false);
}

bool CTRexExtendedDriverMemif::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

void CTRexExtendedDriverMemif::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_port_conf.rxmode.offloads = 0;
    // Memif does not claim as supporting multi-segment send.
    cfg->tx_offloads.common_required &= ~RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
}

///////////////////////////////////////////////////////// VF

CTRexExtendedDriverIavf::CTRexExtendedDriverIavf() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

void CTRexExtendedDriverIavf::update_configuration(port_cfg_t * cfg) {
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

CTRexExtendedDriverIxgbevf::CTRexExtendedDriverIxgbevf() {
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

CTRexExtendedDriverNetvsc::CTRexExtendedDriverNetvsc(){
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
}

TRexPortAttr* CTRexExtendedDriverNetvsc::create_port_attr(tvpid_t tvpid,repid_t repid){
    return new DpdkTRexPortAttr(tvpid, repid, true, false, true, false, false);
}

bool CTRexExtendedDriverNetvsc::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

void CTRexExtendedDriverNetvsc::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_port_conf.rxmode.offloads = 0;
    cfg->tx_offloads.common_required |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
    cfg->tx_offloads.common_required |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
    cfg->tx_offloads.common_best_effort = 0;
}

CTRexExtendedDriverAzure::CTRexExtendedDriverAzure(){
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE;
    //m_cap = tdCAP_ONE_QUE;
}

TRexPortAttr* CTRexExtendedDriverAzure::create_port_attr(tvpid_t tvpid,repid_t repid){
    return new DpdkTRexPortAttr(tvpid, repid, true, false, true, false, false);
}

bool CTRexExtendedDriverAzure::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

void CTRexExtendedDriverAzure::update_configuration(port_cfg_t * cfg){
    CTRexExtendedDriverVirtBase::update_configuration(cfg);
    cfg->m_port_conf.rxmode.offloads = 0;
    cfg->tx_offloads.common_required &= ~RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
#if 0
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
    cfg->m_port_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_JUMBO_FRAME;
    cfg->tx_offloads.common_required |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
#endif
    cfg->tx_offloads.common_best_effort = 0;
}


#include "net/bonding/rte_eth_bond.h"

CTRexExtendedDriverBonding::CTRexExtendedDriverBonding(){
    m_cap = tdCAP_ONE_QUE | tdCAP_MULTI_QUE | TREX_DRV_CAP_MAC_ADDR_CHG;
    m_slave_drv = nullptr;
}

TRexPortAttr* CTRexExtendedDriverBonding::create_port_attr(tvpid_t tvpid, repid_t repid){
    /* In LACP mode, need to call TX/RX burst at least every 100ms. */
    bool flush_needed = false;
    if (rte_eth_bond_mode_get(repid) == BONDING_MODE_8023AD) {
        flush_needed = true;
    }
    return new DpdkTRexPortAttr(tvpid, repid, true, true, true, false, false, flush_needed);
}

bool CTRexExtendedDriverBonding::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

void CTRexExtendedDriverBonding::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
}

int CTRexExtendedDriverBonding::wait_for_stable_link(){
    /* In LACP mode, should wait enough time for DISTRIBUTING */
    wait_x_sec(3 + CGlobalInfo::m_options.m_wait_before_traffic, true);
    return 0;
}

void CTRexExtendedDriverBonding::wait_after_link_up(){
    wait_for_stable_link();
}

void CTRexExtendedDriverBonding::set_slave_driver(tvpid_t tvpid){
    /* some slave drivers need to be called by bonding driver. */
    if (m_slave_drv == nullptr) {
        uint16_t slaves[RTE_MAX_ETHPORTS];
        uint8_t slave_cnt = rte_eth_bond_members_get(CTVPort(tvpid).get_repid(), slaves, RTE_MAX_ETHPORTS);
        if (slave_cnt > 0) {
            struct rte_eth_dev_info dev_info;
            rte_eth_dev_info_get(slaves[0], &dev_info);
            if (CTRexExtendedDriverDb::Ins()->is_driver_exists(dev_info.driver_name)) {
                m_slave_drv = CTRexExtendedDriverDb::Ins()->create_driver(dev_info.driver_name);
                printf(" set slave driver = %s\n", dev_info.driver_name);
            }
        }
    }
}

static std::vector<tvpid_t> get_bond_slave_devs(tvpid_t tvpid){
    std::vector<tvpid_t> slave_devs;
    uint16_t slaves[RTE_MAX_ETHPORTS];
    uint8_t slave_cnt = rte_eth_bond_members_get(CTVPort(tvpid).get_repid(), slaves, RTE_MAX_ETHPORTS);

    for (int i = 0; i < slave_cnt; i++) {
        slave_devs.push_back(CREPort(slaves[i]).get_tvpid());
    }

    return slave_devs;
}

bool CTRexExtendedDriverBonding::extra_tx_queues_requires(tvpid_t tvpid){
    set_slave_driver(tvpid);
    if (m_slave_drv) {
        for (auto slave_id: get_bond_slave_devs(tvpid)) {
            if (m_slave_drv->extra_tx_queues_requires(slave_id)) {
                return true;
            }
        }
    }
    return false;
}

int CTRexExtendedDriverBonding::verify_fw_ver(tvpid_t tvpid){
    set_slave_driver(tvpid);
    if (m_slave_drv) {
        for (auto slave_id: get_bond_slave_devs(tvpid)) {
            if (m_slave_drv->verify_fw_ver(slave_id) < 0) {
                return -1;
            }
        }
    }
    return 0;
}
