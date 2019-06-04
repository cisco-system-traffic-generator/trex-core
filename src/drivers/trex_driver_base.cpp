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

#include "trex_driver_base.h"
#include "trex_driver_defines.h"
#include "trex_driver_i40e.h"
#include "trex_driver_igb.h"
#include "trex_driver_ixgbe.h"
#include "trex_driver_mlx5.h"
#include "trex_driver_ntacc.h"
#include "trex_driver_vic.h"
#include "trex_driver_virtual.h"



port_cfg_t::port_cfg_t() {
    memset(&m_port_conf,0,sizeof(m_port_conf));
    memset(&m_rx_conf,0,sizeof(m_rx_conf));
    memset(&m_tx_conf,0,sizeof(m_tx_conf));
    memset(&m_rx_drop_conf,0,sizeof(m_rx_drop_conf));

    m_rx_conf.rx_thresh.pthresh = RX_PTHRESH;
    m_rx_conf.rx_thresh.hthresh = RX_HTHRESH;
    m_rx_conf.rx_thresh.wthresh = RX_WTHRESH;
    m_rx_conf.rx_free_thresh =32;

    m_rx_drop_conf.rx_thresh.pthresh = 0;
    m_rx_drop_conf.rx_thresh.hthresh = 0;
    m_rx_drop_conf.rx_thresh.wthresh = 0;
    m_rx_drop_conf.rx_free_thresh =32;
    m_rx_drop_conf.rx_drop_en=1;

    m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;

    m_port_conf.rxmode.max_rx_pkt_len = 9*1024+22;

    m_port_conf.rxmode.offloads =
        DEV_RX_OFFLOAD_JUMBO_FRAME |
        DEV_RX_OFFLOAD_CRC_STRIP |
        DEV_RX_OFFLOAD_SCATTER;

    tx_offloads.common_best_effort =
        DEV_TX_OFFLOAD_IPV4_CKSUM |
        DEV_TX_OFFLOAD_UDP_CKSUM |
        DEV_TX_OFFLOAD_TCP_CKSUM;

    tx_offloads.common_required =
        DEV_TX_OFFLOAD_MULTI_SEGS;

    tx_offloads.astf_best_effort =
        DEV_TX_OFFLOAD_TCP_TSO |
        DEV_TX_OFFLOAD_UDP_TSO;

}

void port_cfg_t::update_var(void) {
    get_ex_drv()->update_configuration(this);
    if ( (m_port_conf.rxmode.offloads & DEV_RX_OFFLOAD_TCP_LRO) && 
        CGlobalInfo::m_options.preview.getLroOffloadDisable() ) {
        m_port_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_TCP_LRO;
        printf("Warning LRO is supported and asked to be disabled by user \n");
    }
}

void port_cfg_t::update_global_config_fdir(void) {
    get_ex_drv()->update_global_config_fdir(this);
}



void CTRexExtendedDriverDb::register_driver(std::string name,
                                            create_object_t func){
    CTRexExtendedDriverRec * rec;
    rec = new CTRexExtendedDriverRec();
    rec->m_driver_name=name;
    rec->m_constructor=func;
    m_list.push_back(rec);
}


bool CTRexExtendedDriverDb::is_driver_exists(std::string name){
    int i;
    for (i=0; i<(int)m_list.size(); i++) {
        if (m_list[i]->m_driver_name == name) {
            return (true);
        }
    }
    return (false);
}

void CTRexExtendedDriverDb::set_driver_name(std::string name) {
    m_driver_was_set=true;
    m_driver_name=name;
    printf(" set driver name %s \n",name.c_str());
    m_drv=create_driver(m_driver_name);
    assert(m_drv);
}

void CTRexExtendedDriverDb::create_dummy() {
    if ( ! m_dummy_selector_created ) {
        m_dummy_selector_created = true;
        m_drv = new CTRexExtendedDriverDummySelector(get_drv());
    }
}

CTRexExtendedDriverBase* CTRexExtendedDriverDb::get_drv() {
    if (!m_driver_was_set) {
        printf(" ERROR too early to use this object !\n");
        printf(" need to set the right driver \n");
        assert(0);
    }
    assert(m_drv);
    return (m_drv);
}

CTRexExtendedDriverDb::CTRexExtendedDriverDb() {
    register_driver(std::string("net_ixgbe"),CTRexExtendedDriverBase10G::create);
    register_driver(std::string("net_e1000_igb"),CTRexExtendedDriverBase1G::create);
    register_driver(std::string("net_i40e"),CTRexExtendedDriverBase40G::create);
    register_driver(std::string("net_enic"),CTRexExtendedDriverBaseVIC::create);
    register_driver(std::string("net_mlx5"),CTRexExtendedDriverBaseMlnx5G::create);
    register_driver(std::string("net_mlx4"),CTRexExtendedDriverMlnx4::create);
    register_driver(std::string("net_ntacc"), CTRexExtendedDriverBaseNtAcc::create);


    /* virtual devices */
    register_driver(std::string("net_e1000_em"), CTRexExtendedDriverBaseE1000::create);
    register_driver(std::string("net_vmxnet3"), CTRexExtendedDriverVmxnet3::create);
    register_driver(std::string("net_virtio"), CTRexExtendedDriverVirtio::create);
    register_driver(std::string("net_ena"),CTRexExtendedDriverVirtio::create);
    register_driver(std::string("net_i40e_vf"), CTRexExtendedDriverI40evf::create);
    register_driver(std::string("net_ixgbe_vf"), CTRexExtendedDriverIxgbevf::create);

    /* raw socket */
    register_driver(std::string("net_af_packet"), CTRexExtendedDriverAfPacket::create);

    m_driver_was_set=false;
    m_dummy_selector_created=false;
    m_drv=NULL;
    m_driver_name="";
}


CTRexExtendedDriverBase * CTRexExtendedDriverDb::create_driver(std::string name){
    int i;
    for (i=0; i<(int)m_list.size(); i++) {
        if (m_list[i]->m_driver_name == name) {
            return ( m_list[i]->m_constructor() );
        }
    }
    return( (CTRexExtendedDriverBase *)0);
}


CTRexExtendedDriverDb * CTRexExtendedDriverDb::Ins(){
    if (!m_ins) {
        m_ins = new CTRexExtendedDriverDb();
    }
    return (m_ins);
}



int CTRexExtendedDriverBase::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    repid_t repid =_if->get_repid();

    return (rte_eth_dev_rx_queue_stop(repid, q_num));
}

int CTRexExtendedDriverBase::wait_for_stable_link() {
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
    return 0;
}

void CTRexExtendedDriverBase::wait_after_link_up() {
    wait_x_sec(CGlobalInfo::m_options.m_wait_before_traffic);
}

CFlowStatParser *CTRexExtendedDriverBase::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

bool CTRexExtendedDriverBase::get_extended_stats_fixed(CPhyEthIF * _if, CPhyEthIFStats *stats, int fix_i, int fix_o) {
    struct rte_eth_stats stats1;
    struct rte_eth_stats *prev_stats = &stats->m_prev_stats;
    int res;
    
    /* fetch stats */
    res=rte_eth_stats_get(_if->get_repid(), &stats1);
    
    /* check the error flag */
    if (res!=0) {
        /* error (might happen on i40e_vf ) */
        return false;
    }

    stats->ipackets   += stats1.ipackets - prev_stats->ipackets;
    // Some drivers report input byte counts without Ethernet FCS (4 bytes), we need to fix the reported numbers
    stats->ibytes += stats1.ibytes - prev_stats->ibytes + (stats1.ipackets - prev_stats->ipackets) * fix_i;
    stats->opackets   += stats1.opackets - prev_stats->opackets;
    // Some drivers report output byte counts without Ethernet FCS (4 bytes), we need to fix the reported numbers
    stats->obytes += stats1.obytes - prev_stats->obytes + (stats1.opackets - prev_stats->opackets) * fix_o;
    stats->f_ipackets += 0;
    stats->f_ibytes   += 0;
    stats->ierrors    += stats1.imissed + stats1.ierrors + stats1.rx_nombuf
        - prev_stats->imissed - prev_stats->ierrors - prev_stats->rx_nombuf;
    stats->oerrors    += stats1.oerrors - prev_stats->oerrors;
    stats->imcasts    += 0;
    stats->rx_nombuf  += stats1.rx_nombuf - prev_stats->rx_nombuf;

    prev_stats->ipackets = stats1.ipackets;
    prev_stats->ibytes = stats1.ibytes;
    prev_stats->opackets = stats1.opackets;
    prev_stats->obytes = stats1.obytes;
    prev_stats->imissed = stats1.imissed;
    prev_stats->oerrors = stats1.oerrors;
    prev_stats->ierrors = stats1.ierrors;
    prev_stats->rx_nombuf = stats1.rx_nombuf;
    
    return true;
}


CTRexExtendedDriverBase * get_ex_drv() {
    return ( CTRexExtendedDriverDb::Ins()->get_drv());
}


// CTRexExtendedDriverDummySelector

CTRexExtendedDriverDummySelector::CTRexExtendedDriverDummySelector(CTRexExtendedDriverBase *original_driver) {
    m_real_drv = original_driver;
    m_cap = m_real_drv->get_capabilities();
}

int CTRexExtendedDriverDummySelector::configure_rx_filter_rules(CPhyEthIF *_if) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->configure_rx_filter_rules(_if);
    }
}

int CTRexExtendedDriverDummySelector::add_del_rx_flow_stat_rule(CPhyEthIF *_if, enum rte_filter_op op, uint16_t l3, uint8_t l4, uint8_t ipv6_next_h, uint16_t id) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->add_del_rx_flow_stat_rule(_if, op, l3, l4, ipv6_next_h, id);
    }
}

int CTRexExtendedDriverDummySelector::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->stop_queue(_if, q_num);
    }
}

bool CTRexExtendedDriverDummySelector::get_extended_stats_fixed(CPhyEthIF * _if, CPhyEthIFStats *stats, int fix_i, int fix_o) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->get_extended_stats_fixed(_if, stats, fix_i, fix_o);
    }
}

bool CTRexExtendedDriverDummySelector::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->get_extended_stats(_if, stats);
    }
}

void CTRexExtendedDriverDummySelector::clear_extended_stats(CPhyEthIF * _if) {
    if ( ! _if->is_dummy() ) {
        m_real_drv->clear_extended_stats(_if);
    }
}

int CTRexExtendedDriverDummySelector::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->get_rx_stats(_if, pkts, prev_pkts, bytes, prev_bytes, min, max);
    }
}

void CTRexExtendedDriverDummySelector::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
    if ( ! _if->is_dummy() ) {
        m_real_drv->reset_rx_stats(_if, stats, min, len);
    }
}

int CTRexExtendedDriverDummySelector::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->dump_fdir_global_stats(_if, fd);
    }
}

int CTRexExtendedDriverDummySelector::verify_fw_ver(tvpid_t tvpid) {
    if ( CTVPort(tvpid).is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->verify_fw_ver(tvpid);
    }
}

int CTRexExtendedDriverDummySelector::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    if ( _if->is_dummy() ) {
        return 0;
    } else {
        return m_real_drv->set_rcv_all(_if, set_on);
    }
}

TRexPortAttr * CTRexExtendedDriverDummySelector::create_port_attr(tvpid_t tvpid, repid_t repid) {
    if ( CTVPort(tvpid).is_dummy() ) {
        return new SimTRexPortAttr();
    } else {
        return m_real_drv->create_port_attr(tvpid, repid);
    }
}

