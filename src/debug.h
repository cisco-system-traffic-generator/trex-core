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

#ifndef _TREX_DEBUG_H
#define _TREX_DEBUG_H
#include "mbuf.h"

class CTrexDebug {
    rte_mbuf_t *m_test;
    uint64_t m_test_drop;
    CPhyEthIF *m_ports;
    uint32_t m_max_ports;

    int rcv_send(int port,int queue_id);
    int rcv_send_all(int queue_id);
    rte_mbuf_t *create_pkt(uint8_t *pkt,int pkt_size);
    rte_mbuf_t *create_pkt_indirect(rte_mbuf_t *m, uint32_t new_pkt_size);
    rte_mbuf_t *create_udp_pkt();
    rte_mbuf_t *create_udp_9k_pkt();
    int  set_promisc_all(bool enable);
    int test_send_pkts(rte_mbuf_t *, uint16_t queue_id, int pkt, int port);
    rte_mbuf_t *create_test_pkt(int proto, uint8_t ttl, uint16_t ip_id);

 public:
    CTrexDebug(CPhyEthIF *m_ports_arg, int max_ports);
    int test_send(uint pkt_type);
};

#endif
