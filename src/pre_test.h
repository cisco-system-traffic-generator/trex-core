/*
  Ido Barnea
  Cisco Systems, Inc.
*/

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

#ifndef __PRE_TEST_H__
#define __PRE_TEST_H__

#include <iostream>
#include <common/Network/Packet/Arp.h>
#include <common/Network/Packet/MacAddress.h>
#include "bp_sim.h"
#include "trex_defs.h"

class CPreTestStats {
 public:
    uint32_t m_rx_arp; // how many ARP packets we received
    uint32_t m_tx_arp; // how many ARP packets we sent

 public:
    void clear() {
        m_rx_arp = 0;
        m_tx_arp = 0;
    }
};

class CPretestOnePortInfo {
    friend class CPretest;
    enum CPretestOnePortInfoStates {
        RESOLVE_NEEDED,
        RESOLVE_NOT_NEEDED,
    };

 public:
    CPretestOnePortInfo();
    void add_src(uint32_t ip, uint16_t vlan, MacAddress mac);
    void add_dst(uint32_t ip, uint16_t vlan);
    void add_src(uint16_t ip[8], uint16_t vlan, MacAddress mac);
    void add_dst(uint16_t ip[8], uint16_t vlan);
    bool get_mac(uint32_t ip, uint16_t vlan, uint8_t *mac);
    bool get_mac(uint16_t ip[8], uint16_t vlan, uint8_t *mac);
    bool get_mac(COneIPInfo *ip, uint8_t *mac);
    COneIPInfo *get_src(uint16_t vlan, uint8_t ip_ver);
    void set_port_id(uint16_t port_id)  {m_port_id = port_id;}
    void dump(FILE *fd, char *offset);
    bool is_loopback() {return m_is_loopback;}
    CPreTestStats get_stats() {return m_stats;}
    bool resolve_needed();
    void send_grat_arp_all();
    void send_arp_req_all();

 private:
    COneIPv4Info *find_ip(uint32_t ip, uint16_t vlan);
    COneIPv4Info *find_next_hop(uint32_t ip, uint16_t vlan);
    COneIPv6Info *find_ipv6(uint16_t *ip, uint16_t vlan);
    bool get_mac(COneIPInfo *ip, uint16_t vlan, uint8_t *mac, uint8_t ip_ver);

 private:
    bool m_is_loopback;
    CPretestOnePortInfoStates m_state;
    CPreTestStats m_stats;
    uint16_t m_port_id;
    std::vector<COneIPInfo *> m_src_info;
    std::vector<COneIPInfo *> m_dst_info;
};

class CPretest {
 public:
    CPretest(uint16_t max_ports) {
        m_max_ports = max_ports;
        for (int i =0; i < max_ports; i++) {
            m_port_info[i].set_port_id(i);
        }
    }
    void add_ip(uint16_t port, uint32_t ip, uint16_t vlan, MacAddress src_mac);
    void add_ip(uint16_t port, uint32_t ip, MacAddress src_mac);
    void add_next_hop(uint16_t port, uint32_t ip, uint16_t vlan);
    void add_next_hop(uint16_t port, uint32_t ip);
    void add_ip(uint16_t port, uint16_t ip[8], uint16_t vlan, MacAddress src_mac);
    void add_ip(uint16_t port, uint16_t ip[8], MacAddress src_mac);
    void add_next_hop(uint16_t port, uint16_t ip[8], uint16_t vlan);
    void add_next_hop(uint16_t port, uint16_t ip[8]);
    bool get_mac(uint16_t port, uint32_t ip, uint16_t vlan, uint8_t *mac);
    bool get_mac(uint16_t port, uint16_t ip[8], uint16_t vlan, uint8_t *mac);
    CPreTestStats get_stats(uint16_t port_id);
    bool is_loopback(uint16_t port);
    bool resolve_all();
    void send_arp_req_all();
    void send_grat_arp_all();
    bool is_arp(const uint8_t *p, uint16_t pkt_size, ArpHdr *&arp, uint16_t &vlan_tag);
    void get_results(CManyIPInfo &resolved_ips);
    void dump(FILE *fd);
    void test();

 private:
    int handle_rx(int port, int queue_id);

 private:
    CPretestOnePortInfo m_port_info[TREX_MAX_PORTS];
    uint16_t m_max_ports;
};

#endif
