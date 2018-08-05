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

#include "trex_driver_vic.h"
#include "trex_driver_defines.h"

CTRexExtendedDriverBaseVIC::CTRexExtendedDriverBaseVIC() {
    if (get_is_tcp_mode()) {
        m_cap = TREX_DRV_CAP_DROP_Q  | TREX_DRV_CAP_MAC_ADDR_CHG | TREX_DRV_DEFAULT_ASTF_MULTI_CORE;
    }else{
        m_cap = TREX_DRV_CAP_DROP_Q  | TREX_DRV_CAP_MAC_ADDR_CHG ;
    }
}

int CTRexExtendedDriverBaseVIC::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}


void CTRexExtendedDriverBaseVIC::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.rxmode.max_rx_pkt_len =9*1000-10;
    cfg->m_port_conf.fdir_conf.mask.ipv4_mask.tos = 0x01;
    cfg->m_port_conf.fdir_conf.mask.ipv6_mask.tc  = 0x01;
    if (get_is_tcp_mode()){
       cfg->m_port_conf.fdir_conf.mask.ipv6_mask.proto  = 0xff;
    }
}

void CTRexExtendedDriverBaseVIC::add_del_rules(enum rte_filter_op op, repid_t  repid, uint16_t type
                                               , uint16_t id, uint8_t l4_proto, uint8_t tos, int queue) {
    int ret=rte_eth_dev_filter_supported(repid, RTE_ETH_FILTER_FDIR);

    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, repid);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

#if 0
    printf("VIC add_del_rules::%s rules: port:%d type:%d id:%d l4:%d tod:%d, q:%d\n"
           , (op == RTE_ETH_FILTER_ADD) ?  "add" : "del"
           , port_id, type, id, l4_proto, tos, queue);
#endif

    filter.action.rx_queue = queue;
    filter.action.behavior = RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status = RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.soft_id = id;
    filter.input.flow_type = type;

    switch (type) {
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
        filter.input.flow.ip4_flow.tos = tos;
        filter.input.flow.ip4_flow.proto = l4_proto;
        break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
        filter.input.flow.ipv6_flow.tc = tos;
        filter.input.flow.ipv6_flow.proto = l4_proto;
        break;
    }

    ret = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_FDIR, op, (void*)&filter);
    if ( ret != 0 ) {
        if (((op == RTE_ETH_FILTER_ADD) && (ret == -EEXIST)) || ((op == RTE_ETH_FILTER_DELETE) && (ret == -ENOENT)))
            return;

        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl: err=%d, port=%u\n",
                 ret, repid);
    }
}

int CTRexExtendedDriverBaseVIC::add_del_eth_type_rule(repid_t  repid, enum rte_filter_op op, uint16_t eth_type) {
    int ret;
    struct rte_eth_ethertype_filter filter;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = eth_type;
    filter.flags = 0;
    filter.queue = MAIN_DPDK_RX_Q;
    ret = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_ETHERTYPE, op, (void *) &filter);

    return ret;
}

int CTRexExtendedDriverBaseVIC::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    repid_t  repid = _if->get_repid();

    set_rcv_all(_if, false);

    // Rules to direct all IP packets with tos lsb bit 1 to RX Q.
    // IPv4
    add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 1, 17, 0x1, MAIN_DPDK_RX_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 1, 6,  0x1, MAIN_DPDK_RX_Q);
    add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_SCTP, 1, 132,  0x1, MAIN_DPDK_RX_Q); /*SCTP*/
    add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 1, 1,  0x1, MAIN_DPDK_RX_Q);  /*ICMP*/
    // Ipv6
    if (get_is_tcp_mode()==0){
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 6,  0x1, MAIN_DPDK_RX_Q);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17,  0x1, MAIN_DPDK_RX_Q);
    }else{
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 1,  0x1, MAIN_DPDK_RX_Q);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17,  0x1, MAIN_DPDK_RX_Q);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 1, 6,  0x1, MAIN_DPDK_RX_Q);
    }

    // Because of some issue with VIC firmware, IPv6 UDP and ICMP go by default to q 1, so we
    // need these rules to make them go to q 0.
    // rule appply to all packets with 0 on tos lsb.
    if (get_is_tcp_mode()==0){
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 1, 6,  0, MAIN_DPDK_DROP_Q);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 1, 17,  0, MAIN_DPDK_DROP_Q);
    }

    return 0;
}


int CTRexExtendedDriverBaseVIC::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    repid_t repid=_if->get_repid();

    // soft ID 100 tells VIC driver to add rule for all ether types.
    // Added with highest priority (implicitly in the driver), so if it exists, it applies before all other rules
    if (set_on) {
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 100, 30, 0, MAIN_DPDK_RX_Q);
    } else {
        add_del_rules(RTE_ETH_FILTER_DELETE, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 100, 30, 0, MAIN_DPDK_RX_Q);
    }

    return 0;

}

void CTRexExtendedDriverBaseVIC::clear_extended_stats(CPhyEthIF * _if){
    repid_t repid=_if->get_repid();
    rte_eth_stats_reset(repid);
}

bool CTRexExtendedDriverBaseVIC::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    // In VIC, we need to reduce 4 bytes from the amount reported for each incoming packet
    return get_extended_stats_fixed(_if, stats, -4, 0);
}

int CTRexExtendedDriverBaseVIC::verify_fw_ver(tvpid_t   tvpid) {

    repid_t repid = CTVPort(tvpid).get_repid();

    if (CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_ONE_QUEUE
        || CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_RSS) {
        return 0;
    }

    struct rte_eth_fdir_info fdir_info;

    if ( rte_eth_dev_filter_ctrl(repid,RTE_ETH_FILTER_FDIR, RTE_ETH_FILTER_INFO,(void *)&fdir_info) == 0 ){
        if ( fdir_info.flow_types_mask[0] & (1<< RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) ) {
           /* support new features */
            if (CGlobalInfo::m_options.preview.getVMode() >= 1) {
                printf("VIC port %d: FW support advanced filtering \n", repid);
            }
            return 0;
        }
    }

    printf("Warning: In order to fully utilize the VIC NIC, firmware should be upgraded to support advanced filtering \n");
    printf("  Please refer to %s for upgrade instructions\n",
           "https://trex-tgn.cisco.com/trex/doc/trex_manual.html");
    printf("If this is an unsupported card, or you do not want to upgrade, you can use --software command line arg\n");
    printf("This will work without hardware support (meaning reduced performance)\n");
    exit(1);
}

int CTRexExtendedDriverBaseVIC::configure_rx_filter_rules(CPhyEthIF * _if) {

    if (get_is_stateless()) {
        /* both stateless and stateful work in the same way, might changed in the future TOS */
        return configure_rx_filter_rules_statefull(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }
}

void CTRexExtendedDriverBaseVIC::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {
}

int CTRexExtendedDriverBaseVIC::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    printf(" NOT supported yet \n");
    return 0;
}

// if fd != NULL, dump fdir stats of _if
// return num of filters
int CTRexExtendedDriverBaseVIC::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
 //printf(" NOT supported yet \n");
 return (0);
}

CFlowStatParser *CTRexExtendedDriverBaseVIC::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

void CTRexExtendedDriverBaseVIC::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
    num_counters = MAX_FLOW_STATS;
    base_ip_id = IP_ID_RESERVE_BASE;
}

