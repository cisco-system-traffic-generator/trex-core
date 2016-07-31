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

// For playing around, and testing packet sending in debug mode
rte_mbuf_t *CTrexDebug::create_test_pkt(int pkt_type, uint8_t ttl, uint16_t ip_id) {
    uint8_t proto;
    int pkt_size = 0;
    // ASA 2
    uint8_t dst_mac[6] = {0x74, 0xa2, 0xe6, 0xd5, 0x39, 0x25};
    uint8_t src_mac[6] = {0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x02};
    // ASA 1
    //        uint8_t dst_mac[6] = {0xd4, 0x8c, 0xb5, 0xc9, 0x54, 0x2b};
    //      uint8_t src_mac[6] = {0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x0};
    //#define VLAN
#ifdef VLAN
    uint16_t l2_proto = 0x0081;
    uint8_t vlan_header[4] = {0x0a, 0xbc, 0x08, 0x00};
#ifdef QINQ
    uint8_t vlan_header2[4] = {0x0a, 0xbc, 0x88, 0xa8};
#endif
#else
    uint16_t l2_proto = 0x0008;
#endif
    uint8_t ip_header[] = {
        0x45,0x02,0x00,0x30,
        0x00,0x00,0x40,0x00,
        0xff,0x01,0xbd,0x04,
        0x10,0x0,0x0,0x1, //SIP
        0x30,0x0,0x0,0x1, //DIP
        //                      0x82, 0x0b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // IP option. change 45 to 48 (header len) if using it.
    };
    uint8_t udp_header[] =  {0x11, 0x11, 0x11,0x11, 0x00, 0x6d, 0x00, 0x00};
    uint8_t udp_data[] = {0x64,0x31,0x3a,0x61,
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
    uint8_t tcp_header[] = {0xab, 0xcd, 0x00, 0x80, // src, dst ports
                            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // seq num, ack num
                            0x50, 0x00, 0xff, 0xff, // Header size, flags, window size
                            0x00, 0x00, 0x00, 0x00, // checksum ,urgent pointer
    };

    uint8_t tcp_data[] = {0x8, 0xa, 0x1, 0x2, 0x3, 0x4, 0x3, 0x4, 0x6, 0x5};

    uint8_t icmp_header[] = {
        0x08, 0x00,
        0xb8, 0x21,  //checksum
        0xaa, 0xbb,  // id
        0x00, 0x01,  // Sequence number
    };
    uint8_t icmp_data[] = {
        0xd6, 0x6e, 0x64, 0x34, // magic
        0x6a, 0xad, 0x0f, 0x00, //64 bit counter
        0x00, 0x56, 0x34, 0x12,
        0x78, 0x56, 0x34, 0x12, 0x00, 0x00 // seq
    };

    switch (pkt_type) {
    case 1:
        proto = IPPROTO_ICMP;
        pkt_size = 14 + sizeof(ip_header) + sizeof(icmp_header) + sizeof (icmp_data);
        break;
    case 2:
        proto = IPPROTO_UDP;
        pkt_size = 14 + sizeof(ip_header) + sizeof(udp_header) + sizeof (udp_data);
        break;
    case 3:
        proto = IPPROTO_TCP;
        pkt_size =  14 + sizeof(ip_header) + sizeof(tcp_header) + sizeof (tcp_data);
        break;
    default:
        return NULL;
    }

    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(0, pkt_size);
    if ( unlikely(m == 0) )  {
        printf("ERROR no packets \n");
        return (NULL);
    }
    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);

    /* set pkt data */
    memcpy(p, dst_mac, sizeof(dst_mac)); p += sizeof(dst_mac);
    memcpy(p, src_mac, sizeof(src_mac)); p += sizeof(src_mac);
    memcpy(p, &l2_proto, sizeof(l2_proto)); p += sizeof(l2_proto);
#ifdef VLAN
#ifdef QINQ
    memcpy(p, &vlan_header2, sizeof(vlan_header2)); p += sizeof(vlan_header2);
#endif
    memcpy(p, &vlan_header, sizeof(vlan_header)); p += sizeof(vlan_header);
#endif
    struct IPHeader *ip = (IPHeader *)p;
    memcpy(p, ip_header, sizeof(ip_header)); p += sizeof(ip_header);
    ip->setProtocol(proto);
    ip->setTotalLength(pkt_size - 14);
    ip->setId(ip_id);

    struct TCPHeader *tcp = (TCPHeader *)p;
    struct ICMPHeader *icmp= (ICMPHeader *)p;
    switch (pkt_type) {
    case 1:
        memcpy(p, icmp_header, sizeof(icmp_header)); p += sizeof(icmp_header);
        memcpy(p, icmp_data, sizeof(icmp_data)); p += sizeof(icmp_data);
        icmp->updateCheckSum(sizeof(icmp_header) + sizeof(icmp_data));
        break;
    case 2:
        memcpy(p, udp_header, sizeof(udp_header)); p += sizeof(udp_header);
        memcpy(p, udp_data, sizeof(udp_data)); p += sizeof(udp_data);
        break;
    case 3:
        memcpy(p, tcp_header, sizeof(tcp_header)); p += sizeof(tcp_header);
        memcpy(p, tcp_data, sizeof(tcp_data)); p += sizeof(tcp_data);
        tcp->setSynFlag(true);
        printf("Sending TCP header:");
        tcp->dump(stdout);
        break;
    default:
        return NULL;
    }

    ip->setTimeToLive(ttl);
    ip->updateCheckSum();
    return m;
}

rte_mbuf_t *CTrexDebug::create_pkt(uint8_t *pkt, int pkt_size) {
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(0, pkt_size);
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
    rte_mbuf_t *d = CGlobalInfo::pktmbuf_alloc(0, 60);
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
        CPhyEthIF *_if = &m_ports[i];
        _if->set_promiscuous(enable);
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

int CTrexDebug::test_send(uint pkt_type) {
    int port_id;

    set_promisc_all(true);
    rte_mbuf_t *m, *d, *d2=NULL, *d3=NULL;
    if (pkt_type < 1 || pkt_type > 4) {
        printf("Unsupported packet type %d\n", pkt_type);
        printf("Supported packet types are: %d(ICMP), %d(UDP), %d(TCP) %d(9k UDP)\n", 1, 2, 3, 4);
        exit(-1);
    }

    if (pkt_type == 4) {
        m = create_udp_9k_pkt();
        assert (m);
        d = create_pkt_indirect(m, 9*1024+18);
    } else {
        d = create_test_pkt(pkt_type, 255, 0xff35);
        //        d2 = create_test_pkt(pkt_type, 253, 0xfe01);
        //        d3 = create_test_pkt(pkt_type, 251, 0xfe02);
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
    if (d2) {
        test_send_pkts(d2, 0, 4, 0);
        test_send_pkts(d2, 0, 3, 1);
    }
    if (d3) {
        test_send_pkts(d3, 0, 6, 0);
        test_send_pkts(d3, 0, 5, 1);
    }

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
