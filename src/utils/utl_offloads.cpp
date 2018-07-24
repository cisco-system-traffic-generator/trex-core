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

#include "utl_offloads.h"
#include <string>
#include <vector>

using namespace std;

typedef vector<pair<string, int64_t>> offload_names_t;

bool _check_offloads(uint64_t supported_offloads, uint64_t requested_offloads, const offload_names_t &offload_names, const string &type) {
    bool success = true;
    for ( auto &offloads : offload_names) {
        uint64_t req = offloads.second & requested_offloads;
        uint64_t sup = offloads.second & supported_offloads;
        if ( req && !sup ) {
            success = false;
            printf("Requested %s offload %s is not supported\n", type.c_str(), offloads.first.c_str());
        }
    }
    return success;
}

void check_offloads(const struct rte_eth_dev_info *dev_info, const struct rte_eth_conf *m_port_conf) {
    offload_names_t offload_names;
    bool success = true;

    // TX offloads

    offload_names = {
        {"VLAN_INSERT",      DEV_TX_OFFLOAD_VLAN_INSERT},
        {"IPV4_CKSUM",       DEV_TX_OFFLOAD_IPV4_CKSUM},
        {"UDP_CKSUM",        DEV_TX_OFFLOAD_UDP_CKSUM},
        {"TCP_CKSUM",        DEV_TX_OFFLOAD_TCP_CKSUM},
        {"SCTP_CKSUM",       DEV_TX_OFFLOAD_SCTP_CKSUM},
        {"TCP_TSO",          DEV_TX_OFFLOAD_TCP_TSO},
        {"UDP_TSO",          DEV_TX_OFFLOAD_UDP_TSO},
        {"OUTER_IPV4_CKSUM", DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM},
        {"QINQ_INSERT",      DEV_TX_OFFLOAD_QINQ_INSERT},
        {"VXLAN_TNL_TSO",    DEV_TX_OFFLOAD_VXLAN_TNL_TSO},
        {"GRE_TNL_TSO",      DEV_TX_OFFLOAD_GRE_TNL_TSO},
        {"IPIP_TNL_TSO",     DEV_TX_OFFLOAD_IPIP_TNL_TSO},
        {"GENEVE_TNL_TSO",   DEV_TX_OFFLOAD_GENEVE_TNL_TSO},
        {"MACSEC_INSERT",    DEV_TX_OFFLOAD_MACSEC_INSERT},
        {"MT_LOCKFREE",      DEV_TX_OFFLOAD_MT_LOCKFREE},
        {"MULTI_SEGS",       DEV_TX_OFFLOAD_MULTI_SEGS},
        {"MBUF_FAST_FREE",   DEV_TX_OFFLOAD_MBUF_FAST_FREE},
        {"SECURITY",         DEV_TX_OFFLOAD_SECURITY},
        {"UDP_TNL_TSO",      DEV_TX_OFFLOAD_UDP_TNL_TSO},
        {"IP_TNL_TSO",       DEV_TX_OFFLOAD_IP_TNL_TSO},
    };

    success &= _check_offloads(dev_info->tx_offload_capa, m_port_conf->txmode.offloads, offload_names, "TX");

    // RX offloads

    offload_names = {
        {"VLAN_STRIP",       DEV_RX_OFFLOAD_VLAN_STRIP},
        {"IPV4_CKSUM",       DEV_RX_OFFLOAD_IPV4_CKSUM},
        {"UDP_CKSUM",        DEV_RX_OFFLOAD_UDP_CKSUM},
        {"TCP_CKSUM",        DEV_RX_OFFLOAD_TCP_CKSUM},
        {"TCP_LRO",          DEV_RX_OFFLOAD_TCP_LRO},
        {"QINQ_STRIP",       DEV_RX_OFFLOAD_QINQ_STRIP},
        {"OUTER_IPV4_CKSUM", DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM},
        {"MACSEC_STRIP",     DEV_RX_OFFLOAD_MACSEC_STRIP},
        {"HEADER_SPLIT",     DEV_RX_OFFLOAD_HEADER_SPLIT},
        {"VLAN_FILTER",      DEV_RX_OFFLOAD_VLAN_FILTER},
        {"VLAN_EXTEND",      DEV_RX_OFFLOAD_VLAN_EXTEND},
        {"JUMBO_FRAME",      DEV_RX_OFFLOAD_JUMBO_FRAME},
        {"CRC_STRIP",        DEV_RX_OFFLOAD_CRC_STRIP},
        {"SCATTER",          DEV_RX_OFFLOAD_SCATTER},
        {"TIMESTAMP",        DEV_RX_OFFLOAD_TIMESTAMP},
        {"SECURITY",         DEV_RX_OFFLOAD_SECURITY},
    };

    success &= _check_offloads(dev_info->rx_offload_capa, m_port_conf->rxmode.offloads, offload_names, "RX");

    // RSS offloads

    offload_names = {
        {"IPV4",                ETH_RSS_IPV4},
        {"FRAG_IPV4",           ETH_RSS_FRAG_IPV4},
        {"NONFRAG_IPV4_TCP",    ETH_RSS_NONFRAG_IPV4_TCP},
        {"NONFRAG_IPV4_UDP",    ETH_RSS_NONFRAG_IPV4_UDP},
        {"NONFRAG_IPV4_SCTP",   ETH_RSS_NONFRAG_IPV4_SCTP},
        {"NONFRAG_IPV4_OTHER",  ETH_RSS_NONFRAG_IPV4_OTHER},
        {"IPV6",                ETH_RSS_IPV6},
        {"FRAG_IPV6",           ETH_RSS_FRAG_IPV6},
        {"NONFRAG_IPV6_TCP",    ETH_RSS_NONFRAG_IPV6_TCP},
        {"NONFRAG_IPV6_UDP",    ETH_RSS_NONFRAG_IPV6_UDP},
        {"NONFRAG_IPV6_SCTP",   ETH_RSS_NONFRAG_IPV6_SCTP},
        {"NONFRAG_IPV6_OTHER",  ETH_RSS_NONFRAG_IPV6_OTHER},
        {"L2_PAYLOAD",          ETH_RSS_L2_PAYLOAD},
        {"IPV6_EX",             ETH_RSS_IPV6_EX},
        {"IPV6_TCP_EX",         ETH_RSS_IPV6_TCP_EX},
        {"IPV6_UDP_EX",         ETH_RSS_IPV6_UDP_EX},
        {"PORT",                ETH_RSS_PORT},
        {"VXLAN",               ETH_RSS_VXLAN},
        {"GENEVE",              ETH_RSS_GENEVE},
        {"NVGRE",               ETH_RSS_NVGRE},
        {"INNER_IPV4",          ETH_RSS_INNER_IPV4},
        {"INNER_IPV4_TCP",      ETH_RSS_INNER_IPV4_TCP},
        {"INNER_IPV4_UDP",      ETH_RSS_INNER_IPV4_UDP},
        {"INNER_IPV4_SCTP",     ETH_RSS_INNER_IPV4_SCTP},
        {"INNER_IPV4_OTHER",    ETH_RSS_INNER_IPV4_OTHER},
        {"INNER_IPV6",          ETH_RSS_INNER_IPV6},
        {"INNER_IPV6_TCP",      ETH_RSS_INNER_IPV6_TCP},
        {"INNER_IPV6_UDP",      ETH_RSS_INNER_IPV6_UDP},
        {"INNER_IPV6_SCTP",     ETH_RSS_INNER_IPV6_SCTP},
        {"INNER_IPV6_OTHER",    ETH_RSS_INNER_IPV6_OTHER},
    };

    success &= _check_offloads(dev_info->flow_type_rss_offloads, m_port_conf->rx_adv_conf.rss_conf.rss_hf, offload_names, "RSS");


    if ( !success ) {
        exit(1);
    }
}
