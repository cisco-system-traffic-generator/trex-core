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

#include "trex_driver_ixgbe.h"
#include "trex_driver_defines.h"
#include "dpdk/drivers/net/ixgbe/base/ixgbe_type.h"

CTRexExtendedDriverBase10G::CTRexExtendedDriverBase10G() {
    m_cap = tdCAP_ALL | TREX_DRV_CAP_MAC_ADDR_CHG ;
}

int CTRexExtendedDriverBase10G::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}

void CTRexExtendedDriverBase10G::clear_extended_stats(CPhyEthIF * _if){
    _if->pci_reg_read(IXGBE_RXNFGPC);
}

void CTRexExtendedDriverBase10G::update_global_config_fdir(port_cfg_t * cfg) {
    cfg->m_port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT_MAC_VLAN;
    cfg->m_port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
    cfg->m_port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
    /* Offset of flexbytes field in RX packets (in 16-bit word units). */
    /* Note: divide by 2 to convert byte offset to word offset */
    if (get_is_stateless()) {
        cfg->m_port_conf.fdir_conf.flexbytes_offset = (14+4)/2;
        /* Increment offset 4 bytes for the case where we add VLAN */
        if (  CGlobalInfo::m_options.preview.get_vlan_mode() != CPreviewMode::VLAN_MODE_NONE) {
            cfg->m_port_conf.fdir_conf.flexbytes_offset += (4/2);
        }
    } else {
        if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ) {
            cfg->m_port_conf.fdir_conf.flexbytes_offset = (14+6)/2;
        } else {
            cfg->m_port_conf.fdir_conf.flexbytes_offset = (14+8)/2;
        }

        /* Increment offset 4 bytes for the case where we add VLAN */
        if (  CGlobalInfo::m_options.preview.get_vlan_mode() != CPreviewMode::VLAN_MODE_NONE ) {
            cfg->m_port_conf.fdir_conf.flexbytes_offset += (4/2);
        }
    }
    cfg->m_port_conf.fdir_conf.drop_queue = 1;
}

void CTRexExtendedDriverBase10G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    if ( get_is_tcp_mode() ) {
        cfg->m_port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_CHECKSUM;
        cfg->m_port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_TCP_LRO;
    }
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules(CPhyEthIF * _if) {
    set_rcv_all(_if, false);
    if ( get_is_stateless() ) {
        return configure_rx_filter_rules_stateless(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }

    return 0;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules_stateless(CPhyEthIF * _if) {
    repid_t repid =_if->get_repid();

    uint8_t  ip_id_lsb;

    // 0..128-1 is for rules using ip_id.
    // 128 rule is for the payload rules. Meaning counter value is in the payload
    for (ip_id_lsb = 0; ip_id_lsb <= 128; ip_id_lsb++ ) {
        struct rte_eth_fdir_filter fdir_filter;
        int res = 0;

        memset(&fdir_filter,0,sizeof(fdir_filter));
        fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
        fdir_filter.soft_id = ip_id_lsb; // We can use the ip_id_lsb also as filter soft_id
        if (ip_id_lsb == 128) {
            // payload rule is for 0xffff
            fdir_filter.input.flow_ext.flexbytes[0] = 0xff;
            fdir_filter.input.flow_ext.flexbytes[1] = 0xff;
        } else {
            // less than 255 flow stats, so only byte 1 changes
            fdir_filter.input.flow_ext.flexbytes[0] = 0xff & (IP_ID_RESERVE_BASE >> 8);
            fdir_filter.input.flow_ext.flexbytes[1] = ip_id_lsb;
        }
        fdir_filter.action.rx_queue = 1;
        fdir_filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
        fdir_filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
        res = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_ADD, &fdir_filter);

        if (res != 0) {
            rte_exit(EXIT_FAILURE, "Error: rte_eth_dev_filter_ctrl in configure_rx_filter_rules_stateless: %d\n",res);
        }
    }

    return 0;
}

int CTRexExtendedDriverBase10G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    repid_t repid=_if->get_repid();
    uint16_t base_hop = get_rx_check_hops();

    /* enable rule 0 SCTP -> queue 1 for latency  */
    /* 1 << 21 means send to queue */
    _if->pci_reg_write(IXGBE_L34T_IMIR(0),(1<<21));
    _if->pci_reg_write(IXGBE_FTQF(0),
                       IXGBE_FTQF_PROTOCOL_SCTP|
                       (IXGBE_FTQF_PRIORITY_MASK<<IXGBE_FTQF_PRIORITY_SHIFT)|
                       ((0x0f)<<IXGBE_FTQF_5TUPLE_MASK_SHIFT)|IXGBE_FTQF_QUEUE_ENABLE);

    // IPv4: bytes being compared are {TTL, Protocol}
    uint16_t ff_rules_v4[3] = {
        0xFF11,
        0xFF06,
        0xFF01,
    };
    // IPv6: bytes being compared are {NextHdr, HopLimit}
    uint16_t ff_rules_v6[1] = {
        0x3CFF
    };

    uint16_t *ff_rules;
    uint16_t num_rules;
    int  rule_id = 1;

    if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
        ff_rules = &ff_rules_v6[0];
        num_rules = sizeof(ff_rules_v6)/sizeof(ff_rules_v6[0]);
    }else{
        ff_rules = &ff_rules_v4[0];
        num_rules = sizeof(ff_rules_v4)/sizeof(ff_rules_v4[0]);
    }

    for (int rule_num = 0; rule_num < num_rules; rule_num++ ) {
        struct rte_eth_fdir_filter fdir_filter;
        uint16_t ff_rule = ff_rules[rule_num];
        int res = 0;
        uint16_t v4_hops;

        // configure rule sending packets to RX queue for 10 TTL values
        for (int hops = base_hop; hops < base_hop + 10; hops++) {
            memset(&fdir_filter, 0, sizeof(fdir_filter));
            /* TOS/PROTO */
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ) {
                fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
                fdir_filter.input.flow_ext.flexbytes[0] = (ff_rule >> 8) & 0xff;
                fdir_filter.input.flow_ext.flexbytes[1] = (ff_rule - hops) & 0xff;
            } else {
                v4_hops = hops << 8;
                fdir_filter.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
                fdir_filter.input.flow_ext.flexbytes[0] = ((ff_rule - v4_hops) >> 8) & 0xff;
                fdir_filter.input.flow_ext.flexbytes[1] = ff_rule & 0xff;
            }
            fdir_filter.soft_id = rule_id++;
            fdir_filter.action.rx_queue = 1;
            fdir_filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
            fdir_filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
            res = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_ADD, &fdir_filter);

            if (res != 0) {
                rte_exit(EXIT_FAILURE
                         , "Error: rte_eth_dev_filter_ctrl in configure_rx_filter_rules_statefull rule_id:%d: %d\n"
                         , rule_id, res);
            }
        }
    }
    return (0);
}

int CTRexExtendedDriverBase10G::add_del_eth_filter(CPhyEthIF * _if, bool is_add, uint16_t ethertype) {
    int res = 0;
    repid_t repid =_if->get_repid();
    struct rte_eth_ethertype_filter filter;
    enum rte_filter_op op;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = ethertype;
    res = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_ETHERTYPE, RTE_ETH_FILTER_GET, &filter);

    if (is_add && (res >= 0))
        return 0;
    if ((! is_add) && (res == -ENOENT))
        return 0;

    if (is_add) {
        op = RTE_ETH_FILTER_ADD;
    } else {
        op = RTE_ETH_FILTER_DELETE;
    }

    filter.queue = 1;
    res = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_ETHERTYPE, op, &filter);
    if (res != 0) {
        printf("Error: %s L2 filter for ethertype 0x%04x returned %d\n", is_add ? "Adding":"Deleting", ethertype, res);
        exit(1);
    }
    return 0;
}

int CTRexExtendedDriverBase10G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    int res = 0;
    res = add_del_eth_filter(_if, set_on, ETHER_TYPE_ARP);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_IPv4);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_IPv6);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_VLAN);
    res |= add_del_eth_filter(_if, set_on, ETHER_TYPE_QINQ);

    return res;
}

bool CTRexExtendedDriverBase10G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){

    int i;
    uint64_t t=0;

    if ( !get_is_stateless() ) {

        for (i=0; i<8;i++) {
            t+=_if->pci_reg_read(IXGBE_MPC(i));
        }
    }

    stats->ipackets     +=  _if->pci_reg_read(IXGBE_GPRC) ;

    stats->ibytes       +=  (_if->pci_reg_read(IXGBE_GORCL) +(((uint64_t)_if->pci_reg_read(IXGBE_GORCH))<<32));



    stats->opackets     +=  _if->pci_reg_read(IXGBE_GPTC);
    stats->obytes       +=  (_if->pci_reg_read(IXGBE_GOTCL) +(((uint64_t)_if->pci_reg_read(IXGBE_GOTCH))<<32));

    stats->f_ipackets   +=  _if->pci_reg_read(IXGBE_RXDGPC);
    stats->f_ibytes     += (_if->pci_reg_read(IXGBE_RXDGBCL) +(((uint64_t)_if->pci_reg_read(IXGBE_RXDGBCH))<<32));


    stats->ierrors      +=  ( _if->pci_reg_read(IXGBE_RLEC) +
                              _if->pci_reg_read(IXGBE_ERRBC) +
                              _if->pci_reg_read(IXGBE_CRCERRS) +
                              _if->pci_reg_read(IXGBE_ILLERRC ) +
                              _if->pci_reg_read(IXGBE_ROC)+
                              _if->pci_reg_read(IXGBE_RUC)+t);

    stats->oerrors      +=  0;
    stats->imcasts      =  0;
    stats->rx_nombuf    =  0;

    return true;
}

int CTRexExtendedDriverBase10G::wait_for_stable_link(){
    wait_x_sec(1 + CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}

CFlowStatParser *CTRexExtendedDriverBase10G::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser((CGlobalInfo::m_options.preview.get_vlan_mode()
                                                   != CPreviewMode::VLAN_MODE_NONE)
                                                  ? CFlowStatParser::FLOW_STAT_PARSER_MODE_82599_vlan
                                                  : CFlowStatParser::FLOW_STAT_PARSER_MODE_82599);
    assert (parser);
    return parser;
}

void CTRexExtendedDriverBase10G::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
    if ( !get_dpdk_mode()->is_hardware_filter_needed() ) {
        num_counters = MAX_FLOW_STATS;
    } else {
        num_counters = 127;
    }
    base_ip_id = IP_ID_RESERVE_BASE;
}

