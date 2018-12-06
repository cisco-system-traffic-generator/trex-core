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

#include "trex_driver_i40e.h"
#include "trex_driver_defines.h"
#include "dpdk_funcs.h"



static uint16_t all_eth_types[]  = {
    0x0800, 0x0806, 0x0842, 0x22F3, 0x22EA, 0x6003, 0x8035, 0x809B, 0x80F3, 0x8100,
    0x8137, 0x8204, 0x86DD, 0x8808, 0x8809, 0x8819, 0x8847, 0x8848, 0x8863, 0x8864,
    0x886D, 0x8870, 0x887B, 0x888E, 0x8892, 0x889A, 0x88A2, 0x88A4, 0x88A8, 0x88AB,
    0x88B8, 0x88B9, 0x88BA, 0x88CC, 0x88CD, 0x88DC, 0x88E1, 0x88E3, 0x88E5, 0x88E7,
    0x88F7, 0x88FB, 0x8902, 0x8906, 0x8914, 0x8915, 0x891D, 0x892F, 0x9000, 0x9100,
};

CTRexExtendedDriverBase40G::CTRexExtendedDriverBase40G() {
    m_cap = tdCAP_ALL | TREX_DRV_CAP_MAC_ADDR_CHG | TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN ;
}

int CTRexExtendedDriverBase40G::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE);
}

void CTRexExtendedDriverBase40G::clear_extended_stats(CPhyEthIF * _if){
    rte_eth_stats_reset(_if->get_repid());
}


void CTRexExtendedDriverBase40G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = TX_WTHRESH;
    cfg->m_port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
    cfg->m_port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
    cfg->m_port_conf.fdir_conf.status = RTE_FDIR_NO_REPORT_STATUS;
}

// What is the type of the rule the respective hw_id counter counts.
struct fdir_hw_id_params_t {
    uint16_t rule_type;
    uint8_t l4_proto;
};

static struct fdir_hw_id_params_t fdir_hw_id_rule_params[512];

/* Add rule to send packets with protocol 'type', and ttl 'ttl' to rx queue 1 */
// ttl is used in statefull mode, and ip_id in stateless. We configure the driver registers so that only one of them applies.
// So, the rule will apply if packet has either the correct ttl or IP ID, depending if we are in statfull or stateless.
void CTRexExtendedDriverBase40G::add_del_rules(enum rte_filter_op op, repid_t  repid, uint16_t type, uint8_t ttl
                                               , uint16_t ip_id, uint8_t l4_proto, int queue, uint16_t stat_idx) {
    int ret=rte_eth_dev_filter_supported(repid, RTE_ETH_FILTER_FDIR);
    static int filter_soft_id = 0;

    if ( ret != 0 ){
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_supported "
                 "err=%d, port=%u \n",
                 ret, repid);
    }

    struct rte_eth_fdir_filter filter;

    memset(&filter,0,sizeof(struct rte_eth_fdir_filter));

#if 0
    printf("40g::%s rules: port:%d type:%d ttl:%d ip_id:%x l4:%d q:%d hw index:%d\n"
           , (op == RTE_ETH_FILTER_ADD) ?  "add" : "del"
           , repid, type, ttl, ip_id, l4_proto, queue, stat_idx);
#endif

    filter.action.rx_queue = queue;
    filter.action.behavior =RTE_ETH_FDIR_ACCEPT;
    filter.action.report_status =RTE_ETH_FDIR_NO_REPORT_STATUS;
    filter.action.stat_count_index = stat_idx;
    filter.soft_id = filter_soft_id++;
    filter.input.flow_type = type;

    if (op == RTE_ETH_FILTER_ADD) {
        fdir_hw_id_rule_params[stat_idx].rule_type = type;
        fdir_hw_id_rule_params[stat_idx].l4_proto = l4_proto;
    }

    switch (type) {
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
    case RTE_ETH_FLOW_IPV4:
    case RTE_ETH_FLOW_FRAG_IPV4:
        filter.input.flow.ip4_flow.ttl=ttl;
        filter.input.flow.ip4_flow.ip_id = ip_id;
        if (l4_proto != 0)
            filter.input.flow.ip4_flow.proto = l4_proto;
        break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
    case RTE_ETH_FLOW_IPV6:
    case RTE_ETH_FLOW_FRAG_IPV6:
        filter.input.flow.ipv6_flow.hop_limits=ttl;
        filter.input.flow.ipv6_flow.flow_label = ip_id;
        filter.input.flow.ipv6_flow.proto = l4_proto;
        break;
    }

    ret = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_FDIR, op, (void*)&filter);

#if 0
    //todo: fix
    if ( ret != 0 ) {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_filter_ctrl: err=%d, port=%u\n",
                 ret, repid);
    }
#endif
}

int CTRexExtendedDriverBase40G::add_del_eth_type_rule(repid_t  repid, enum rte_filter_op op, uint16_t eth_type) {
    int ret;
    struct rte_eth_ethertype_filter filter;

    memset(&filter, 0, sizeof(filter));
    filter.ether_type = eth_type;
    filter.flags = 0;
    filter.queue = MAIN_DPDK_RX_Q;
    ret = rte_eth_dev_filter_ctrl(repid, RTE_ETH_FILTER_ETHERTYPE, op, (void *) &filter);

    return ret;
}

uint32_t CTRexExtendedDriverBase40G::get_flow_stats_offset(repid_t repid) {
    uint8_t pf_id;
    int ret = i40e_trex_get_pf_id(repid, &pf_id);
    assert(ret >= 0);
    assert((pf_id >= 0) && (pf_id <= 3));
    return pf_id * m_max_flow_stats;
}


// type - rule type. Currently we only support rules in IP ID.
// proto - Packet protocol: UDP or TCP
// id - Counter id in HW. We assume it is in the range 0..m_max_flow_stats
int CTRexExtendedDriverBase40G::add_del_rx_flow_stat_rule(CPhyEthIF * _if, enum rte_filter_op op, uint16_t l3_proto
                                                          , uint8_t l4_proto, uint8_t ipv6_next_h, uint16_t id) {
    repid_t repid = _if->get_repid();

    uint32_t rule_id = get_flow_stats_offset(repid) + id;
    uint16_t rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
    uint8_t next_proto;

    if (l3_proto == EthernetHeader::Protocol::IP) {
        next_proto = l4_proto;
        switch(l4_proto) {
        case IPPROTO_TCP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_TCP;
            break;
        case IPPROTO_UDP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_UDP;
            break;
        default:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
            break;
        }
    } else {
        // IPv6
        next_proto = ipv6_next_h;
        switch(l4_proto) {
        case IPPROTO_TCP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_TCP;
            break;
        case IPPROTO_UDP:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_UDP;
            break;
        default:
            rte_type = RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
            break;
        }
    }

    // If we count flow stat in hardware, we want all packets to be dropped.
    // If we count in software, we want to receive them.
    uint16_t queue;
    if (CGlobalInfo::m_options.preview.get_disable_hw_flow_stat()) {
        queue = MAIN_DPDK_RX_Q;
    } else {
        queue = MAIN_DPDK_DROP_Q;
    }

    add_del_rules(op, repid, rte_type, 0, IP_ID_RESERVE_BASE + id, next_proto, queue, rule_id);
    return 0;
}

int CTRexExtendedDriverBase40G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    repid_t repid=_if->get_repid();
    uint16_t hops = get_rx_check_hops();
    int i;

    rte_eth_fdir_stats_reset(repid, NULL, 0, 1);
    for (i = 0; i < 10; i++) {
        uint8_t ttl = TTL_RESERVE_DUPLICATE - i - hops;
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, ttl, 0, RX_CHECK_V6_OPT_TYPE, MAIN_DPDK_RX_Q, 0);
        /* Rules for latency measurement packets */
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, ttl, 0, IPPROTO_ICMP, MAIN_DPDK_RX_Q, 0);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_SCTP, ttl, 0, 0, MAIN_DPDK_RX_Q, 0);
    }
    return 0;
}

const uint32_t FDIR_TEMP_HW_ID = 511;
const uint32_t FDIR_PAYLOAD_RULES_HW_ID = 510;
extern const uint32_t FLOW_STAT_PAYLOAD_IP_ID;
int CTRexExtendedDriverBase40G::configure_rx_filter_rules(CPhyEthIF * _if) {
    repid_t repid=_if->get_repid();

    if (get_is_stateless()) {
        i40e_trex_fdir_reg_init(repid, I40E_TREX_INIT_STL);

        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, IPPROTO_ICMP, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);

        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_FRAG_IPV4, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);

        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);
        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);

        add_del_rules(RTE_ETH_FILTER_ADD, repid, RTE_ETH_FLOW_FRAG_IPV6, 0
                      , FLOW_STAT_PAYLOAD_IP_ID, 0, MAIN_DPDK_RX_Q, FDIR_PAYLOAD_RULES_HW_ID);

        rte_eth_fdir_stats_reset(repid, NULL, FDIR_TEMP_HW_ID, 1);
        return 0; // Other rules are configured dynamically in stateless
    } else {
        i40e_trex_fdir_reg_init(repid, I40E_TREX_INIT_STF);
        return configure_rx_filter_rules_statefull(_if);
    }
}

void CTRexExtendedDriverBase40G::reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {

    repid_t repid = _if->get_repid();

    uint32_t rule_id = get_flow_stats_offset(repid) + min;

    // Since flow dir counters are not wrapped around as promised in the data sheet, but rather get stuck at 0xffffffff
    // we reset the HW value
    rte_eth_fdir_stats_reset(repid, NULL, rule_id, len);

    for (int i =0; i < len; i++) {
        stats[i] = 0;
    }
}


// get rx stats on _if, between min and max
// prev_pkts should be the previous values read from the hardware.
//            Getting changed to be equal to current HW values.
// pkts return the diff between prev_pkts and current hw values
// bytes and prev_bytes are not used. X710 fdir filters do not support byte count.
int CTRexExtendedDriverBase40G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                             ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    uint32_t hw_stats[MAX_FLOW_STATS_XL710];
    repid_t repid = _if->get_repid();

    uint32_t start = get_flow_stats_offset(repid) + min;
    uint32_t len = max - min + 1;

    rte_eth_fdir_stats_get(repid, hw_stats, start, len);
    for (int i = min; i <= max; i++) {
        if (unlikely(hw_stats[i - min] > CGlobalInfo::m_options.get_x710_fdir_reset_threshold())) {
            // When x710 fdir counters reach max of 32 bits (4G), they get stuck. To handle this, we temporarily
            // move to temp counter, reset the counter in danger, and go back to using it.
            // see trex-199 for more details
            uint32_t counter, temp_count=0;
            uint32_t hw_id = start - min + i;

            add_del_rules( RTE_ETH_FILTER_ADD, repid, fdir_hw_id_rule_params[hw_id].rule_type, 0
                           , IP_ID_RESERVE_BASE + i, fdir_hw_id_rule_params[hw_id].l4_proto, MAIN_DPDK_DROP_Q
                           , FDIR_TEMP_HW_ID);
            rte_eth_fdir_stats_reset(repid, &counter, hw_id, 1);
            add_del_rules( RTE_ETH_FILTER_ADD, repid, fdir_hw_id_rule_params[hw_id].rule_type, 0
                           , IP_ID_RESERVE_BASE + i, fdir_hw_id_rule_params[hw_id].l4_proto, MAIN_DPDK_DROP_Q, hw_id);
            rte_eth_fdir_stats_reset(repid, &temp_count, FDIR_TEMP_HW_ID, 1);
            pkts[i] = counter + temp_count - prev_pkts[i];
            prev_pkts[i] = 0;
        } else {
            pkts[i] = hw_stats[i - min] - prev_pkts[i];
            prev_pkts[i] = hw_stats[i - min];
        }
        bytes[i] = 0;
    }

    return 0;
}

// if fd != NULL, dump fdir stats of _if
// return num of filters
int CTRexExtendedDriverBase40G::dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd)
{
    repid_t repid = _if->get_repid();

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

bool CTRexExtendedDriverBase40G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
    return get_extended_stats_fixed(_if, stats, 4, 4);
}

int CTRexExtendedDriverBase40G::wait_for_stable_link(){
    wait_x_sec(1 + CGlobalInfo::m_options.m_wait_before_traffic);
    return (0);
}


int CTRexExtendedDriverBase40G::verify_fw_ver(tvpid_t   tvpid) {
    uint32_t version;
    int ret;

    repid_t repid=CTVPort(tvpid).get_repid();

    ret = rte_eth_get_fw_ver(repid, &version);

    if (ret == 0) {
        if (CGlobalInfo::m_options.preview.getVMode() >= 1) {
            printf("port %d: FW ver %02d.%02d.%02d\n", (int)repid, ((version >> 12) & 0xf), ((version >> 4) & 0xff)
                   ,(version & 0xf));
        }

        if ((((version >> 12) & 0xf) < 5)  || ((((version >> 12) & 0xf) == 5) && ((version >> 4 & 0xff) == 0)
                                               && ((version & 0xf) < 4))) {
            printf("Error: In this TRex version, X710 firmware must be at least 05.00.04\n");
            printf("  Please refer to %s for upgrade instructions\n",
                   "https://trex-tgn.cisco.com/trex/doc/trex_manual.html#_firmware_update_to_xl710_x710");
            exit(1);
        }
    }

    return ret;
}

CFlowStatParser *CTRexExtendedDriverBase40G::get_flow_stat_parser() {
    CFlowStatParser *parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_HW);
    assert (parser);
    return parser;
}

int CTRexExtendedDriverBase40G::set_rcv_all(CPhyEthIF * _if, bool set_on) {

    repid_t repid=_if->get_repid();

    enum rte_filter_op op = set_on ? RTE_ETH_FILTER_ADD : RTE_ETH_FILTER_DELETE;

    for (int i = 0; i < sizeof(all_eth_types)/sizeof(uint16_t); i++) {
        add_del_eth_type_rule(repid, op, all_eth_types[i]);
    }

    if (set_on) {
        i40e_trex_fdir_reg_init(repid, I40E_TREX_INIT_RCV_ALL);
    }

    // In order to receive packets, we also need to configure rules for each type.
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV4_UDP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV4_TCP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV4_SCTP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV4_OTHER, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_FRAG_IPV4, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    

    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV6_UDP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV6_TCP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV6_SCTP, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_NONFRAG_IPV6_OTHER, 10, 0, 0, MAIN_DPDK_RX_Q, 0);
    add_del_rules(op, repid, RTE_ETH_FLOW_FRAG_IPV6, 10, 0, 0, MAIN_DPDK_RX_Q, 0);


    if (! set_on) {
        configure_rx_filter_rules(_if);
    }

    return 0;
}

void CTRexExtendedDriverBase40G::get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
    flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_PAYLOAD;
    // HW counters on x710 do not support counting bytes.
    if ( !get_dpdk_mode()->is_hardware_filter_needed() ) {
        flags |= TrexPlatformApi::IF_STAT_RX_BYTES_COUNT;
        num_counters = MAX_FLOW_STATS;
    } else {
        // TODO: check if we could get amount of interfaces per NIC to enlarge this
        num_counters = MAX_FLOW_STATS_X710;
    }
    base_ip_id = IP_ID_RESERVE_BASE;
    m_max_flow_stats = num_counters;
}

bool CTRexExtendedDriverBase40G::hw_rx_stat_supported() {
    if (CGlobalInfo::m_options.preview.get_disable_hw_flow_stat() ||
        (!get_dpdk_mode()->is_hardware_filter_needed())) {
        return false;
    } else {
        return true;
    }
}

