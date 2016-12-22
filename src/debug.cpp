/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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

// DPDK c++ issue
#define UINT8_MAX 255
#define UINT16_MAX 0xFFFF
// DPDK c++ issue

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <rte_mbuf.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <common/basic_utils.h>
#include "pkt_gen.h"
#include "main_dpdk.h"
#include "debug.h"

const uint8_t udp_pkt[] = {
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x08,0x00,

    0x45,0x00,0x00,0x81,
    0xaf,0x7e,0x00,0x00,
    0xfe,0x06,0xd9,0x23,
    0x01,0x01,0x01,0x01,
    0x3d,0xad,0x72,0x1b,

    0x11,0x11,
    0x11,0x11,
    0x00,0x6d,
    0x00,0x00,

    0x64,0x31,0x3a,0x61,
    0x64,0x32,0x3a,0x69,0x64,
    0x32,0x30,0x3a,0xd0,0x0e,
    0xa1,0x4b,0x7b,0xbd,0xbd,
    0x16,0xc6,0xdb,0xc4,0xbb,0x43,
    0xf9,0x4b,0x51,0x68,0x33,0x72,
    0x20,0x39,0x3a,0x69,0x6e,0x66,0x6f,
    0x5f,0x68,0x61,0x73,0x68,0x32,0x30,0x3a,0xee,0xc6,0xa3,
    0xd3,0x13,0xa8,0x43,0x06,0x03,0xd8,0x9e,0x3f,0x67,0x6f,
    0xe7,0x0a,0xfd,0x18,0x13,0x8d,0x65,0x31,0x3a,0x71,0x39,
    0x3a,0x67,0x65,0x74,0x5f,0x70,0x65,0x65,0x72,0x73,0x31,
    0x3a,0x74,0x38,0x3a,0x3d,0xeb,0x0c,0xbf,0x0d,0x6a,0x0d,
    0xa5,0x31,0x3a,0x79,0x31,0x3a,0x71,0x65,0x87,0xa6,0x7d,
    0xe7
};

CTrexDebug::CTrexDebug(CPhyEthIF m_ports_arg[12], int max_ports) {
    m_test = NULL;
    m_ports = m_ports_arg;
    m_max_ports = max_ports;
}

int CTrexDebug::rcv_send(int port, int queue_id) {
    CPhyEthIF * lp = &m_ports[port];
    rte_mbuf_t * rx_pkts[32];
    printf(" test rx port:%d queue:%d \n",port,queue_id);
    printf(" --------------\n");
    uint16_t cnt = lp->rx_burst(queue_id,rx_pkts,32);
    int i;

    for (i=0; i < (int)cnt; i++) {
        rte_mbuf_t * m = rx_pkts[i];
        int pkt_size = rte_pktmbuf_pkt_len(m);
        char *p = rte_pktmbuf_mtod(m, char*);
        utl_DumpBuffer(stdout, p, pkt_size, 0);
        rte_pktmbuf_free(m);
    }
    return 0;
}

// receive packets on queue_id
int CTrexDebug::rcv_send_all(int queue_id) {
    int i;
    for (i=0; i<m_max_ports; i++) {
        rcv_send(i,queue_id);
    }
    return 0;
}

#if 0
rte_mbuf_t *CTrexDebug::create_test_pkt(int ip_ver, uint16_t l4_proto, uint8_t ttl, uint16_t ip_id) {
    uint8_t test_pkt[] =
    {0x74, 0xa2, 0xe6, 0xd5, 0x39, 0x25, 0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x02, 0x86, 0xDD, 0x60, 0x00,
     0xff, 0x7f, 0x00, 0x14, 0x06, 0xff, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x01, 0x30, 0x00,
     //     0x00, 0x01, 0x10, 0x00, 0x00, 0x01, 0x20, 0x01, 0x00, 0x00, 0x41, 0x37, 0x93, 0x50, 0x80, 0x00,
     0x00, 0x01, 0x10, 0x00, 0x00, 0x01, /* TCP: */ 0xab, 0xcd, 0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
     0x07, 0x08, 0x50, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x08, 0x0a, 0x01, 0x02, 0x03, 0x04,
     //     bad - 0x03, 0x04, 0x06, 0x02, 0x20, 0x00, 0xBB, 0x79, 0x00, 0x00};
    0x03, 0x04, 0x50, 0x02, 0x20, 0x00, 0xBB, 0x79, 0x00, 0x00};
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_by_port(0, sizeof(test_pkt));
    char *p = rte_pktmbuf_append(m, sizeof(test_pkt));
    assert(p);

    /* set pkt data */
    memcpy(p, test_pkt, sizeof(test_pkt));
    return m;

}

#else
rte_mbuf_t *CTrexDebug::create_test_pkt(int ip_ver, uint16_t l4_proto, uint8_t ttl
                                        , uint32_t ip_id, uint16_t flags) {
    int pkt_size;
    char *pkt;
    rte_mbuf_t *m;
    char *p;
    uint16_t l3_type;

    switch (ip_ver) {
    case 4:
        l3_type = EthernetHeader::Protocol::IP;
        break;
    case 6:
        l3_type = EthernetHeader::Protocol::IPv6;
        break;
    case 1:
        l3_type = EthernetHeader::Protocol::ARP;
        break;
    default:
        return NULL;
        break;
    }

    pkt = CTestPktGen::create_test_pkt(l3_type, l4_proto, ttl, ip_id, flags, 1000, pkt_size);

    /* DEBUG print the packet
    utl_k12_pkt_format(stdout,pkt,  pkt_size) ;
    */

    m = CGlobalInfo::pktmbuf_alloc_by_port(0, pkt_size);
    if ( unlikely(m == 0) )  {
        printf("ERROR no packets \n");
        return (NULL);
    }
    p = rte_pktmbuf_append(m, pkt_size);
    memcpy(p, pkt, pkt_size);
    free(pkt);

    return m;
}

#endif

rte_mbuf_t *CTrexDebug::create_pkt(uint8_t *pkt, int pkt_size) {
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_by_port(0, pkt_size);
    if ( unlikely(m == 0) ) {
        printf("ERROR no packets \n");
        return 0;
    }

    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* set pkt data */
    memcpy(p, pkt, pkt_size);
    return m;
}

rte_mbuf_t *CTrexDebug::create_pkt_indirect(rte_mbuf_t *m, uint32_t new_pkt_size){
    rte_mbuf_t *d = CGlobalInfo::pktmbuf_alloc_by_port(0, 60);
    assert(d);

    rte_pktmbuf_attach(d, m);
    d->data_len = new_pkt_size;
    d->pkt_len = new_pkt_size;
    return d;
}

rte_mbuf_t *CTrexDebug::create_udp_9k_pkt() {
    rte_mbuf_t *m;
    uint16_t pkt_size = 9*1024+21;
    uint8_t *p = (uint8_t *)malloc(9*1024+22);
    assert(p);
    memset(p, 0x55, pkt_size);
    memcpy(p, (uint8_t*)udp_pkt, sizeof(udp_pkt));
    m = create_pkt(p, pkt_size);
    free(p);
    return m;
}

int CTrexDebug::test_send_pkts(rte_mbuf_t *m, uint16_t queue_id, int num_pkts, int port) {
    CPhyEthIF * lp = &m_ports[port];
    rte_mbuf_t * tx_pkts[32];
    if (num_pkts > 32) {
        num_pkts = 32;
    }

    int i;
    for (i=0; i < num_pkts; i++) {
        rte_mbuf_refcnt_update(m, 1);
        tx_pkts[i] = m;
    }
    uint16_t res = lp->tx_burst(queue_id, tx_pkts, num_pkts);
    if ((num_pkts - res) > 0) {
        m_test_drop += (num_pkts - res);
    }
    return (0);
}

int  CTrexDebug::set_promisc_all(bool enable) {
    int i;
    for (i=0; i < m_max_ports; i++) {
        if (enable) {
            rte_eth_promiscuous_enable(i);
        }else{
            rte_eth_promiscuous_disable(i);
        }
    }

    return 0;
}

static void rte_stat_dump_array(const uint64_t *c, const char *name, int size) {
    int i;

    // dont print anything if all values are 0
    for (i = 0; i < size; i++) {
        if (c[i] != 0)
            break;
    }
    if (i == size)
        return;

    printf("%s:", name);
    for (i = 0; i < size; i++) {
        if (((i % 32) == 0) && (size > 32)) {
            printf ("\n  %4d:", i);
        }
        printf(" %2ld", c[i]);
    }
    printf("\n");
}

static void rte_stat_dump_one(uint64_t c, const char *name) {
    if (c != 0)
        printf("%s:%ld\n", name, c);
}

static void rte_stats_dump(const struct rte_eth_stats &stats) {
    rte_stat_dump_one(stats.ipackets, "ipackets");
    rte_stat_dump_one(stats.opackets, "opackets");
    rte_stat_dump_one(stats.ibytes, "ibytes");
    rte_stat_dump_one(stats.obytes, "obytes");
    rte_stat_dump_one(stats.imissed, "imissed");
    rte_stat_dump_one(stats.ierrors, "ierrors");
    rte_stat_dump_one(stats.oerrors, "oerrors");
    rte_stat_dump_one(stats.rx_nombuf, "rx_nombuf");
    rte_stat_dump_array(stats.q_ipackets, "queue rx", RTE_ETHDEV_QUEUE_STAT_CNTRS);
    rte_stat_dump_array(stats.q_opackets, "queue tx", RTE_ETHDEV_QUEUE_STAT_CNTRS);
    rte_stat_dump_array(stats.q_ibytes, "queue rx bytes", RTE_ETHDEV_QUEUE_STAT_CNTRS);
    rte_stat_dump_array(stats.q_obytes, "queue tx bytes", RTE_ETHDEV_QUEUE_STAT_CNTRS);
    rte_stat_dump_array(stats.q_errors, "queue dropped", RTE_ETHDEV_QUEUE_STAT_CNTRS);
}

extern const uint32_t FLOW_STAT_PAYLOAD_IP_ID;

typedef enum debug_expected_q_t_ {
    ZERO, // always queue 0
    ONE, // always queue 1
    STL, // queue 1 on stateless. 0 on stateful
    STF // queue 1 on stateful. 0 on stateless
} debug_expected_q_t;

struct pkt_params {
    char name[100];
    uint8_t ip_ver;
    uint16_t l4_proto;
    uint8_t ttl;
    uint32_t ip_id;
    uint16_t pkt_flags;
    debug_expected_q_t expected_q;
};

struct pkt_params test_pkts[] = {
    {"ARP", 1, IPPROTO_UDP, 255, 5, 0, ZERO},
    {"VLAN ARP", 1, IPPROTO_UDP, 255, 5, DPF_VLAN, ZERO},
    {"ipv4 TCP ttl 255", 4, IPPROTO_TCP, 255, 5, 0, STF},
    {"ipv4 TCP ttl 246", 4, IPPROTO_TCP, 246, 5, 0, STF},
    {"ipv4 TCP ttl 245", 4, IPPROTO_TCP, 245, 5, 0, ZERO},
    {"ipv4 UDP ttl 255", 4, IPPROTO_UDP, 255, 5, 0, STF},
    {"ipv4 UDP ttl 246", 4, IPPROTO_UDP, 246, 5, 0, STF},
    {"ipv4 UDP ttl 245", 4, IPPROTO_UDP, 245, 5, 0, ZERO},
    {"ipv4 ICMP ttl 255", 4, IPPROTO_ICMP, 255, 5, 0, STF},
    {"ipv4 ICMP ttl 246", 4, IPPROTO_ICMP, 246, 5, 0, STF},
    {"ipv4 ICMP ttl 245", 4, IPPROTO_ICMP, 245, 5, 0, ZERO},
    {"ipv4 TCP latency flow stat", 4, IPPROTO_TCP, 240, FLOW_STAT_PAYLOAD_IP_ID, 0, STL},
    {"ipv4 UDP latency flow stat", 4, IPPROTO_UDP, 240, FLOW_STAT_PAYLOAD_IP_ID, 0, STL},
    {"vlan ipv4 TCP ttl 255", 4, IPPROTO_TCP, 255, 5, DPF_VLAN, STF},
    {"vlan ipv4 TCP ttl 246", 4, IPPROTO_TCP, 246, 5, DPF_VLAN, STF},
    {"vlan ipv4 TCP ttl 245", 4, IPPROTO_TCP, 245, 5, DPF_VLAN, ZERO},
    {"vlan ipv4 UDP ttl 255", 4, IPPROTO_UDP, 255, 5, DPF_VLAN, STF},
    {"vlan ipv4 UDP ttl 246", 4, IPPROTO_UDP, 246, 5, DPF_VLAN, STF},
    {"vlan ipv4 UDP ttl 245", 4, IPPROTO_UDP, 245, 5, DPF_VLAN, ZERO},
    {"vlan ipv4 ICMP ttl 255", 4, IPPROTO_ICMP, 255, 5, DPF_VLAN, STF},
    {"vlan ipv4 ICMP ttl 246", 4, IPPROTO_ICMP, 246, 5, DPF_VLAN, STF},
    {"vlan ipv4 ICMP ttl 245", 4, IPPROTO_ICMP, 245, 5, DPF_VLAN, ZERO},
    {"vlan ipv4 TCP latency flow stat", 4, IPPROTO_TCP, 240, FLOW_STAT_PAYLOAD_IP_ID, DPF_VLAN, STL},
    {"vlan ipv4 UDP latency flow stat", 4, IPPROTO_UDP, 240, FLOW_STAT_PAYLOAD_IP_ID, DPF_VLAN, STL},
    {"ipv6 TCP ttl 255", 6, IPPROTO_TCP, 255, 5, DPF_RXCHECK, STF},
    {"ipv6 TCP ttl 246", 6, IPPROTO_TCP, 246, 5, DPF_RXCHECK, STF},
    {"ipv6 TCP ttl 245", 6, IPPROTO_TCP, 245, 5, DPF_RXCHECK, ZERO},
    {"ipv6 UDP ttl 255", 6, IPPROTO_UDP, 255, 5, DPF_RXCHECK, STF},
    {"ipv6 UDP ttl 246", 6, IPPROTO_UDP, 246, 5, DPF_RXCHECK, STF},
    {"ipv6 UDP ttl 245", 6, IPPROTO_UDP, 245, 5, DPF_RXCHECK, ZERO},
    {"ipv6 ICMP ttl 255", 6, IPPROTO_ICMP, 255, 5, DPF_RXCHECK, STF},
    {"ipv6 ICMP ttl 246", 6, IPPROTO_ICMP, 246, 5, DPF_RXCHECK, STF},
    {"ipv6 ICMP ttl 245", 6, IPPROTO_ICMP, 245, 5, DPF_RXCHECK, ZERO},
    {"ipv6 TCP latency flow stat", 6, IPPROTO_TCP, 240, FLOW_STAT_PAYLOAD_IP_ID, 0, STL},
    {"ipv6 UDP latency flow stat", 6, IPPROTO_UDP, 240, FLOW_STAT_PAYLOAD_IP_ID, 0, STL},
    {"vlan ipv6 TCP ttl 255", 6, IPPROTO_TCP, 255, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 TCP ttl 246", 6, IPPROTO_TCP, 246, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 TCP ttl 245", 6, IPPROTO_TCP, 245, 5, DPF_VLAN | DPF_RXCHECK, ZERO},
    {"vlan ipv6 UDP ttl 255", 6, IPPROTO_UDP, 255, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 UDP ttl 246", 6, IPPROTO_UDP, 246, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 UDP ttl 245", 6, IPPROTO_UDP, 245, 5, DPF_VLAN | DPF_RXCHECK, ZERO},
    {"vlan ipv6 ICMP ttl 255", 6, IPPROTO_ICMP, 255, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 ICMP ttl 246", 6, IPPROTO_ICMP, 246, 5, DPF_VLAN | DPF_RXCHECK, STF},
    {"vlan ipv6 ICMP ttl 245", 6, IPPROTO_ICMP, 245, 5, DPF_VLAN | DPF_RXCHECK, ZERO},
    {"vlan ipv6 TCP latency flow stat", 6, IPPROTO_TCP, 240, FLOW_STAT_PAYLOAD_IP_ID, DPF_VLAN, STL},
    {"vlan ipv6 UDP latency flow stat", 6, IPPROTO_UDP, 240, FLOW_STAT_PAYLOAD_IP_ID, DPF_VLAN, STL},
};

// unit test for verifying hw queues rule configuration. Can be run by:
// for stateful: --send-debug-pkt 100 -f cap2/dns.yaml -l 1
// for stateless: --setnd-debug-pkt 100 -i
int CTrexDebug::verify_hw_rules(bool recv_all) {
    rte_mbuf_t *m = NULL;
    CPhyEthIF * lp;
    rte_mbuf_t * rx_pkts[32];
    int sent_num = 8;   /* reduce the size, there are driver that can handle only burst of 8 in QUEUE 0 */
    int ret = 0;

    for (int pkt_num = 0; pkt_num < sizeof(test_pkts) / sizeof (test_pkts[0]); pkt_num++) {
        uint8_t ip_ver = test_pkts[pkt_num].ip_ver;
        uint16_t l4_proto = test_pkts[pkt_num].l4_proto;
        uint8_t ttl = test_pkts[pkt_num].ttl;
        uint32_t ip_id = test_pkts[pkt_num].ip_id;
        uint8_t exp_q;
        uint16_t pkt_flags = test_pkts[pkt_num].pkt_flags;
        debug_expected_q_t expected_q = test_pkts[pkt_num].expected_q;
        if (recv_all) {
            exp_q = MAIN_DPDK_RX_Q;
        } else {
            switch (expected_q) {
            case ZERO:
                exp_q = MAIN_DPDK_DATA_Q;
                break;
            case ONE:
                exp_q = MAIN_DPDK_RX_Q;
                break;
            case STL:
                if ( CGlobalInfo::m_options.is_stateless() ) {
                    exp_q = MAIN_DPDK_RX_Q;
                    pkt_flags |= DPF_TOS_1;
                } else {
                    exp_q = MAIN_DPDK_DATA_Q;
                }
                break;
            case STF:
                if ( CGlobalInfo::m_options.is_stateless() ) {
                    exp_q = MAIN_DPDK_DATA_Q;
                } else {
                    exp_q = MAIN_DPDK_RX_Q;
                    pkt_flags |= DPF_TOS_1;
                }
                break;
            default:
                exp_q = MAIN_DPDK_DATA_Q;
                break;
            }
        }

        m = create_test_pkt(ip_ver, l4_proto, ttl, ip_id, pkt_flags);
        assert(m);
        test_send_pkts(m, 0, sent_num, 0);

        delay(100);

        int pkt_per_q[2];
        memset(pkt_per_q, 0, sizeof(pkt_per_q));
        // We don't know which interfaces connected where, so sum all queue 1 and all queue 0
        for (int port = 0; port < m_max_ports; port++) {
            for(int queue_id = 0; queue_id <= 1; queue_id++) {
                lp = &m_ports[port];
                uint16_t cnt = lp->rx_burst(queue_id, rx_pkts, 32);
                pkt_per_q[queue_id] += cnt;

                for (int i = 0; i < (int)cnt; i++) {
                    rte_mbuf_t * m = rx_pkts[i];
                    rte_pktmbuf_free(m);
                }
            }
        }

        if (pkt_per_q[exp_q] != sent_num) {
            printf("Error:");
            ret = 1;
        } else {
            printf ("OK:");
        }
        printf("%s q0: %d, q1:%d\n", test_pkts[pkt_num].name, pkt_per_q[0], pkt_per_q[1]);

    }
    return ret;
}

int CTrexDebug::test_send(uint pkt_type) {
    int port_id;

    set_promisc_all(true);
    rte_mbuf_t *m, *d;

    if (pkt_type == D_PKT_TYPE_HW_VERIFY) {
        return verify_hw_rules(false);
    } else if (pkt_type == D_PKT_TYPE_HW_VERIFY_RCV_ALL) {
        return verify_hw_rules(true);
    }

    if (! (pkt_type >= 1 && pkt_type <= 4) && !(pkt_type >= 61 && pkt_type <= 63)) {
        printf("Unsupported packet type %d\n", pkt_type);
        printf("Supported packet types are: %d(ICMP), %d(UDP), %d(TCP) %d(9k UDP)\n", 1, 2, 3, 4);
        printf("                            IPv6: %d(ICMP), %d(UDP), %d(TCP)\n", 61, 62, 63);
        exit(-1);
    }

    if (pkt_type == D_PKT_TYPE_9k_UDP) {
        m = create_udp_9k_pkt();
        assert (m);
        d = create_pkt_indirect(m, 9*1024+18);
    } else {
        uint16_t l4_proto;
        int ip_ver;

        if (pkt_type > D_PKT_TYPE_IPV6) {
            ip_ver = 6;
            pkt_type -= D_PKT_TYPE_IPV6;
        } else {
            ip_ver = 4;
        }
        if (pkt_type > D_PKT_TYPE_ARP) {
            printf("Packet type not supported\n");
            exit(1);
        }

        switch(pkt_type) {
        default:
        case D_PKT_TYPE_ICMP:
            l4_proto = IPPROTO_ICMP;
            break;
        case D_PKT_TYPE_UDP:
            l4_proto = IPPROTO_UDP;
            break;
        case D_PKT_TYPE_TCP:
            l4_proto = IPPROTO_TCP;
            break;
        case D_PKT_TYPE_ARP:
            ip_ver = 1;
            l4_proto = IPPROTO_TCP; //just to prevenet compilation warning. Not used in this case.
            break;
        }
        d = create_test_pkt(ip_ver, l4_proto, 254, FLOW_STAT_PAYLOAD_IP_ID, 0);
    }
    if (d == NULL) {
        printf("Packet creation failed\n");
        exit(-1);
    }

    for (port_id = 0; port_id < m_max_ports; port_id++) {
        CPhyEthIF * lp=&m_ports[port_id];
        lp->reset_hw_flow_stats();
    }

    printf("Sending packet:\n");
    utl_DumpBuffer(stdout, rte_pktmbuf_mtod(d, char *), 64, 0);

    test_send_pkts(d, 0, 2, 0);
    test_send_pkts(d, 0, 1, 1);

    delay(1000);

    int j=0;
    for (j = 0; j < 2; j++) {
        printf(" =========\n");
        printf(" rx queue %d \n", j);
        printf(" =========\n");
        rcv_send_all(j);
        printf("\n\n");
    }

    delay(1000);

    struct rte_eth_stats stats;
    for (port_id = 0; port_id < m_max_ports; port_id++) {
        CPhyEthIF * lp=&m_ports[port_id];
        std::cout << "=====================\n";
        std::cout << "Statistics for port " << port_id << std::endl;
        std::cout << "=====================\n";

        if (rte_eth_stats_get(port_id, &stats) == 0) {
            rte_stats_dump(stats);
        } else {
            // For NICs which does not support rte_eth_stats_get, we have our own implementation.
            lp->update_counters();
            lp->get_stats().Dump(stdout);
        }

        lp->dump_stats_extended(stdout);
    }
    for (port_id = 0; port_id < m_max_ports; port_id++) {
        rx_per_flow_t fdir_stat[MAX_FLOW_STATS];
        uint64_t fdir_stat_64[MAX_FLOW_STATS];
        CPhyEthIF *lp = &m_ports[port_id];
        if (lp->get_flow_stats(fdir_stat, NULL, 0, MAX_FLOW_STATS, false) == 0) {
            for (int i = 0; i < MAX_FLOW_STATS; i++) {
                fdir_stat_64[i] = fdir_stat[i].get_pkts();
            }
            rte_stat_dump_array(fdir_stat_64, "FDIR stat", MAX_FLOW_STATS);
        }
    }


    return (0);
}
