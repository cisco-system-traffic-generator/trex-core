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

#include <rte_ethdev.h>
#include <arpa/inet.h>
#include <common/Network/Packet/EthernetHeader.h>
#include <common/Network/Packet/Arp.h>
#include "common/basic_utils.h"
#include "bp_sim.h"
#include "main_dpdk.h"
#include "pkt_gen.h"
#include "pre_test.h"

CPretestOnePortInfo::CPretestOnePortInfo() {
    m_state = RESOLVE_NOT_NEEDED;
    m_is_loopback = false;
    m_stats.clear();
}

CPretestOnePortInfo::~CPretestOnePortInfo() {
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        delete *it;
    }
    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        delete *it;
    }
}

void CPretestOnePortInfo::add_src(uint32_t ip, uint16_t vlan, MacAddress mac) {
    COneIPv4Info *one_ip = new COneIPv4Info(ip, vlan, mac);
    assert(one_ip);
    m_src_info.push_back(one_ip);
}

void CPretestOnePortInfo::add_dst(uint32_t ip, uint16_t vlan) {
    MacAddress default_mac;
    COneIPv4Info *one_ip = new COneIPv4Info(ip, vlan, default_mac);
    assert(one_ip);
    m_dst_info.push_back(one_ip);
    m_state = RESOLVE_NEEDED;
}

void CPretestOnePortInfo::add_src(uint16_t ip[8], uint16_t vlan, MacAddress mac) {
    COneIPv6Info *one_ip = new COneIPv6Info(ip, vlan, mac);
    assert(one_ip);
    m_src_info.push_back(one_ip);
}

void CPretestOnePortInfo::add_dst(uint16_t ip[8], uint16_t vlan) {
    MacAddress default_mac;
    COneIPv6Info *one_ip = new COneIPv6Info(ip, vlan, default_mac);
    assert(one_ip);
    m_dst_info.push_back(one_ip);
    m_state = RESOLVE_NEEDED;
}

void CPretestOnePortInfo::dump(FILE *fd, char *offset) {
    std::string new_offset = std::string(offset) + "  ";

    if (m_is_loopback) {
        fprintf(fd, "%sPort connected in loopback\n", offset);
    }
    fprintf(fd, "%sSources:\n", offset);
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        (*it)->dump(fd, new_offset.c_str());
    }
    fprintf(fd, "%sDestinations:\n", offset);
    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        (*it)->dump(fd, new_offset.c_str());
    }
}

/*
 * Get appropriate source for given vlan and ip version.
 */
COneIPInfo *CPretestOnePortInfo::get_src(uint16_t vlan, uint8_t ip_ver) {
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        if ((ip_ver == (*it)->ip_ver()) && (vlan == (*it)->get_vlan()))
            return (*it);
    }

    return NULL;
}

COneIPv4Info *CPretestOnePortInfo::find_ip(uint32_t ip, uint16_t vlan) {
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        if (((*it)->ip_ver() == COneIPInfo::IP4_VER) && ((*it)->get_vlan() == vlan) && (((COneIPv4Info *)(*it))->get_ip() == ip))
            return (COneIPv4Info *) *it;
    }

    return NULL;
}

COneIPv4Info *CPretestOnePortInfo::find_next_hop(uint32_t ip, uint16_t vlan) {

    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        if (((*it)->ip_ver() == COneIPInfo::IP4_VER) && ((*it)->get_vlan() == vlan) && (((COneIPv4Info *)(*it))->get_ip() == ip))
            return (COneIPv4Info *) *it;
    }

    return NULL;
}

COneIPv6Info *CPretestOnePortInfo::find_ipv6(uint16_t ip[8], uint16_t vlan) {
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        if (((*it)->ip_ver() == COneIPInfo::IP6_VER) && ((*it)->get_vlan() == vlan)
            && (! memcmp((uint8_t *) ((COneIPv6Info *) (*it))->get_ipv6(), (uint8_t *)ip, 2*8 /* ???*/ ) ) )
            return (COneIPv6Info *) *it;
    }

    return NULL;
}

bool CPretestOnePortInfo::get_mac(COneIPInfo *ip, uint8_t *mac) {
    MacAddress defaultmac;

    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        if (ip->ip_ver() != (*it)->ip_ver())
            continue;

        switch(ip->ip_ver()) {
        case 4:
            if (*((COneIPv4Info *) (*it)) != *((COneIPv4Info *) ip))
                continue;
            break;
        case 6:
            if (*((COneIPv6Info *) (*it)) != *((COneIPv6Info *) ip))
                continue;
            break;
        default:
            assert(0);
        }

        (*it)->get_mac(mac);
        if (! memcmp(mac, defaultmac.GetConstBuffer(), ETHER_ADDR_LEN)) {
            return false;
        } else {
            return true;
        }
    }

    return false;
}

bool CPretestOnePortInfo::get_mac(uint32_t ip, uint16_t vlan, uint8_t *mac) {
    COneIPv4Info one_ip(ip, vlan);

    return get_mac(&one_ip, mac);
}

bool CPretestOnePortInfo::get_mac(uint16_t ip[8], uint16_t vlan, uint8_t *mac) {
    COneIPv6Info one_ip(ip, vlan);

    return get_mac(&one_ip, mac);
}

// return true if there are still any addresses to resolve on this port
bool CPretestOnePortInfo::resolve_needed() {
    if (m_state == RESOLVE_NOT_NEEDED)
        return false;

    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        if ((*it)->resolve_needed())
            return true;
    }

    m_state = RESOLVE_NOT_NEEDED;
    return false;
}

void CPretestOnePortInfo::send_arp_req_all() {
    for (std::vector<COneIPInfo *>::iterator it = m_dst_info.begin(); it != m_dst_info.end(); ++it) {
        rte_mbuf_t *m[1];
        int num_sent;
        int verbose = CGlobalInfo::m_options.preview.getVMode();

        m[0] = CGlobalInfo::pktmbuf_alloc_small_by_port(m_port_id);
        if ( unlikely(m[0] == 0) )  {
            fprintf(stderr, "ERROR: Could not allocate mbuf for sending ARP to port:%d\n", m_port_id);
            exit(1);
        }

        uint8_t *p = (uint8_t *)rte_pktmbuf_append(m[0], (*it)->get_arp_req_len());
        // We need source on the same VLAN of the dest in order to send
        COneIPInfo *sip = get_src((*it)->get_vlan(), (*it)->ip_ver());
        if (sip == NULL) {
            fprintf(stderr, "Failed finding matching source for - ");
            (*it)->dump(stderr);
            exit(1);
        }
        (*it)->fill_arp_req_buf(p, m_port_id, sip);

        if (verbose >= 3) {
            fprintf(stdout, "TX ARP request on port %d - " , m_port_id);
            (*it)->dump(stdout, "");
        }

        num_sent = rte_eth_tx_burst(m_port_id, 0, m, 1);
        if (num_sent < 1) {
            fprintf(stderr, "Failed sending ARP to port:%d\n", m_port_id);
            exit(1);
        } else {
            m_stats.m_tx_arp++;
        }
    }
}

void CPretestOnePortInfo::send_grat_arp_all() {
    for (std::vector<COneIPInfo *>::iterator it = m_src_info.begin(); it != m_src_info.end(); ++it) {
        rte_mbuf_t *m[1];
        int num_sent;
        int verbose = CGlobalInfo::m_options.preview.getVMode();

        m[0] = CGlobalInfo::pktmbuf_alloc_small_by_port(m_port_id);
        if ( unlikely(m[0] == 0) )  {
            fprintf(stderr, "ERROR: Could not allocate mbuf for sending grat ARP on port:%d\n", m_port_id);
            exit(1);
        }

        uint8_t *p = (uint8_t *)rte_pktmbuf_append(m[0], (*it)->get_grat_arp_len());
        (*it)->fill_grat_arp_buf(p);


        if (verbose >= 3) {
            fprintf(stdout, "TX grat ARP on port %d - " , m_port_id);
            (*it)->dump(stdout, "");
        }

        num_sent = rte_eth_tx_burst(m_port_id, 0, m, 1);
        if (num_sent < 1) {
            fprintf(stderr, "Failed sending grat ARP on port:%d\n", m_port_id);
            exit(1);
        } else {
            m_stats.m_tx_arp++;
        }
    }
}

// IPv4 functions
void CPretest::add_ip(uint16_t port, uint32_t ip, uint16_t vlan, MacAddress src_mac) {
    assert(port < m_max_ports);
    m_port_info[port].add_src(ip, vlan, src_mac);
}

void CPretest::add_ip(uint16_t port, uint32_t ip, MacAddress src_mac) {
    assert(port < m_max_ports);
    add_ip(port, ip, 0, src_mac);
}

void CPretest::add_next_hop(uint16_t port, uint32_t ip, uint16_t vlan) {
    assert(port < m_max_ports);
    m_port_info[port].add_dst(ip, vlan);
}

void CPretest::add_next_hop(uint16_t port, uint32_t ip) {
    assert(port < m_max_ports);
    add_next_hop(port, ip, 0);
}

// IPv6 functions
void CPretest::add_ip(uint16_t port, uint16_t ip[8], uint16_t vlan, MacAddress src_mac) {
    assert(port < m_max_ports);
    m_port_info[port].add_src(ip, vlan, src_mac);
}

void CPretest::add_ip(uint16_t port, uint16_t ip[8], MacAddress src_mac) {
    assert(port < m_max_ports);
    add_ip(port, ip, 0, src_mac);
}

void CPretest::add_next_hop(uint16_t port, uint16_t ip[8], uint16_t vlan) {
    assert(port < m_max_ports);
    m_port_info[port].add_dst(ip, vlan);
}

void CPretest::add_next_hop(uint16_t port, uint16_t ip[8]) {
    assert(port < m_max_ports);
    add_next_hop(port, ip, 0);
}

// put in mac, the relevant mac address for the tupple port_id, ip, vlan
bool CPretest::get_mac(uint16_t port_id, uint32_t ip, uint16_t vlan, uint8_t *mac) {
    assert(port_id < m_max_ports);

    return m_port_info[port_id].get_mac(ip, vlan, mac);
}

// IPv6 version of above
bool CPretest::get_mac(uint16_t port_id, uint16_t ip[8], uint16_t vlan, uint8_t *mac) {
    assert(port_id < m_max_ports);

    return m_port_info[port_id].get_mac(ip, vlan, mac);
}

CPreTestStats CPretest::get_stats(uint16_t port_id) {
    assert(port_id < m_max_ports);

    return m_port_info[port_id].get_stats();
}

bool CPretest::is_loopback(uint16_t port) {
    assert(port < m_max_ports);

    return m_port_info[port].is_loopback();
}

bool CPretest::resolve_all() {
    uint16_t port;

    // send ARP request on all ports
    for (port = 0; port < m_max_ports; port++) {
        m_port_info[port].send_arp_req_all();
    }

    int max_tries = 1000;
    int i;
    for (i = 0; i < max_tries; i++) {
        bool all_resolved = true;
        for (port = 0; port < m_max_ports; port++) {
            if (m_port_info[port].resolve_needed()) {
                // We need to stop reading packets only if all ports are resolved.
                // If we are on loopback, We might get requests on port even after it is in RESOLVE_DONE state
                all_resolved = false;
            }
            handle_rx(port, MAIN_DPDK_DATA_Q);
            if (! CGlobalInfo::m_options.preview.get_vm_one_queue_enable())
                handle_rx(port, MAIN_DPDK_RX_Q);
        }
        if (all_resolved) {
            break;
        } else {
            delay(1);
        }
    }

    if (i == max_tries) {
        return false;
    } else {
        return true;
    }

    return true;
}

void CPretest::send_arp_req_all() {
    for (uint16_t port = 0; port < m_max_ports; port++) {
        m_port_info[port].send_arp_req_all();
    }
}

void CPretest::send_grat_arp_all() {
    for (uint16_t port = 0; port < m_max_ports; port++) {
        m_port_info[port].send_grat_arp_all();
    }
}

bool CPretest::is_arp(const uint8_t *p, uint16_t pkt_size, ArpHdr *&arp, uint16_t &vlan_tag) {
    EthernetHeader *m_ether = (EthernetHeader *)p;
    vlan_tag = 0;

    if ((pkt_size < sizeof(EthernetHeader)) ||
        ((m_ether->getNextProtocol() != EthernetHeader::Protocol::ARP)
         && (m_ether->getNextProtocol() != EthernetHeader::Protocol::VLAN)))
        return false;

    if (m_ether->getNextProtocol() == EthernetHeader::Protocol::ARP) {
        arp = (ArpHdr *)(p + 14);
    } else {
        if (m_ether->getVlanProtocol() != EthernetHeader::Protocol::ARP) {
            return false;
        } else {
            vlan_tag = m_ether->getVlanTag();
            arp = (ArpHdr *)(p + 18);
        }
    }

    return true;
}

int CPretest::handle_rx(int port_id, int queue_id) {
    rte_mbuf_t * rx_pkts[32];
    uint16_t cnt;
    int i;
    int verbose = CGlobalInfo::m_options.preview.getVMode();
    int tries = 0;

    do {
        cnt = rte_eth_rx_burst(port_id, queue_id, rx_pkts, sizeof(rx_pkts)/sizeof(rx_pkts[0]));
        tries++;
        bool free_pkt;
        for (i = 0; i < cnt; i++) {
            rte_mbuf_t * m = rx_pkts[i];
            free_pkt = true;
            int pkt_size = rte_pktmbuf_pkt_len(m);
            uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);
            ArpHdr *arp;
            uint16_t vlan_tag;
            CPretestOnePortInfo *port = &m_port_info[port_id];
            if (is_arp(p, pkt_size, arp, vlan_tag)) {
                port->m_stats.m_rx_arp++;
                if (arp->m_arp_op == htons(ArpHdr::ARP_HDR_OP_REQUEST)) {
                    if (verbose >= 3) {
                        bool is_grat = false;
                        if (arp->m_arp_sip == arp->m_arp_tip) {
                            is_grat = true;
                        }
                        fprintf(stdout, "RX %s on port %d queue %d sip:%s tip:%s vlan:%d\n"
                                , is_grat ? "grat ARP" : "ARP request"
                                , port_id, queue_id
                                , ip_to_str(ntohl(arp->m_arp_sip)).c_str()
                                , ip_to_str(ntohl(arp->m_arp_tip)).c_str()
                                , vlan_tag);
                    }
                    // is this request for our IP?
                    COneIPv4Info *src_addr;
                    COneIPv4Info *rcv_addr;
                    if ((src_addr = port->find_ip(ntohl(arp->m_arp_tip), vlan_tag))) {
                        // If our request(i.e. we are connected in loopback)
                        // , do a shortcut, and write info directly to asking port
                        uint8_t magic[5] = {0x1, 0x3, 0x5, 0x7, 0x9};
                        if (! memcmp((uint8_t *)&arp->m_arp_tha.data, magic, 5)) {
                            uint8_t sent_port_id = arp->m_arp_tha.data[5];
                            if ((sent_port_id < m_max_ports) &&
                                (rcv_addr = m_port_info[sent_port_id].find_next_hop(ntohl(arp->m_arp_tip), vlan_tag))) {
                                uint8_t mac[ETHER_ADDR_LEN];
                                src_addr->get_mac(mac);
                                rcv_addr->set_mac(mac);
                                port->m_is_loopback = true;
                                m_port_info[sent_port_id].m_is_loopback = true;
                            }
                        } else {
                            // Not our request. Answer.
                            uint8_t src_mac[ETHER_ADDR_LEN];
                            free_pkt = false; // We use the same mbuf to send response. Don't free it twice.
                            arp->m_arp_op = htons(ArpHdr::ARP_HDR_OP_REPLY);
                            uint32_t tmp_ip = arp->m_arp_sip;
                            arp->m_arp_sip = arp->m_arp_tip;
                            arp->m_arp_tip = tmp_ip;
                            memcpy((uint8_t *)&arp->m_arp_tha, (uint8_t *)&arp->m_arp_sha, ETHER_ADDR_LEN);
                            src_addr->get_mac(src_mac);
                            memcpy((uint8_t *)&arp->m_arp_sha, src_mac, ETHER_ADDR_LEN);
                            EthernetHeader *m_ether = (EthernetHeader *)p;
                            memcpy((uint8_t *)&m_ether->myDestination, (uint8_t *)&m_ether->mySource, ETHER_ADDR_LEN);
                            memcpy((uint8_t *)&m_ether->mySource, src_mac, ETHER_ADDR_LEN);
                            int num_sent = rte_eth_tx_burst(port_id, 0, &m, 1);
                            if (num_sent < 1) {
                                fprintf(stderr, "Failed sending ARP reply to port:%d\n", port_id);
                                rte_pktmbuf_free(m);
                            } else {
                                if (verbose >= 3) {
                                    fprintf(stdout, "TX ARP reply on port:%d sip:%s, tip:%s\n"
                                            , port_id
                                            , ip_to_str(ntohl(arp->m_arp_sip)).c_str()
                                            , ip_to_str(ntohl(arp->m_arp_tip)).c_str());

                                }
                                m_port_info[port_id].m_stats.m_tx_arp++;
                            }
                        }
                    } else {
                        // ARP request not to our IP. Check if this is gratitues ARP for something we need.
                        if ((arp->m_arp_tip == arp->m_arp_sip)
                            && (rcv_addr = port->find_next_hop(ntohl(arp->m_arp_tip), vlan_tag))) {
                            rcv_addr->set_mac((uint8_t *)&arp->m_arp_sha);
                        }
                    }
                } else {
                    if (arp->m_arp_op == htons(ArpHdr::ARP_HDR_OP_REPLY)) {
                        if (verbose >= 3) {
                            fprintf(stdout, "RX ARP reply on port %d queue %d sip:%s tip:%s\n"
                                    , port_id, queue_id
                                    , ip_to_str(ntohl(arp->m_arp_sip)).c_str()
                                    , ip_to_str(ntohl(arp->m_arp_tip)).c_str());
                        }

                        // If this is response to our request, update our tables
                        COneIPv4Info *addr;
                        if ((addr = port->find_next_hop(ntohl(arp->m_arp_sip), vlan_tag))) {
                            addr->set_mac((uint8_t *)&arp->m_arp_sha);
                        }
                    }
                }
            }
            if (free_pkt)
                rte_pktmbuf_free(m);
        }
    } while ((cnt != 0) && (tries < 1000));

    return 0;
}

void CPretest::get_results(CManyIPInfo &resolved_ips) {
    for (int port = 0; port < m_max_ports; port++) {
        for (std::vector<COneIPInfo *>::iterator it = m_port_info[port].m_dst_info.begin()
                 ; it != m_port_info[port].m_dst_info.end(); ++it) {
            uint8_t ip_type = (*it)->ip_ver();
            (*it)->set_port(port);
            switch(ip_type) {
            case COneIPInfo::IP4_VER:
                resolved_ips.insert(*(COneIPv4Info *)(*it));
                break;
#if 0
                //??? fix for ipv6
            case COneIPInfo::IP6_VER:
                ipv6_tmp = (uint8_t *)((COneIPv6Info *)(*it))->get_ipv6();
                memcpy((uint8_t *)ipv6, (uint8_t *)ipv6_tmp, 16);
                v6_list.insert(std::pair<std::pair<uint16_t[8], uint16_t>, COneIPv6Info>
                               (std::pair<uint16_t[8], uint16_t>(ipv6, vlan), *(COneIPv6Info *)(*it)));
                break;
#endif
            default:
                break;
            }
        }
    }
}

void CPretest::dump(FILE *fd) {
    fprintf(fd, "Pre test info start ===================\n");
    for (int port = 0; port < m_max_ports; port++) {
        fprintf(fd, "Port %d:\n", port);
        m_port_info[port].dump(fd, (char *)"  ");
    }
    fprintf(fd, "Pre test info end ===================\n");
}

void CPretest::test() {
    uint8_t found_mac[ETHER_ADDR_LEN];
    uint8_t mac0[ETHER_ADDR_LEN] = {0x90, 0xe2, 0xba, 0xae, 0x87, 0xd0};
    uint8_t mac1[ETHER_ADDR_LEN] = {0x90, 0xe2, 0xba, 0xae, 0x87, 0xd1};
    uint8_t mac2[ETHER_ADDR_LEN] = {0x90, 0xe2, 0xba, 0xae, 0x87, 0xd2};
    uint32_t ip0  = 0x0f000002;
    uint32_t ip01 = 0x0f000003;
    uint32_t ip1  = 0x0f000001;
    uint16_t ipv6_0[8] = {0x1234, 0x5678, 0xabcd, 0x0, 0x0, 0x0, 0x1111, 0x2220};
    uint16_t ipv6_1[8] = {0x1234, 0x5678, 0xabcd, 0x0, 0x0, 0x0, 0x1111, 0x2221};
    uint16_t vlan=1;
    uint8_t port_0 = 0;
    uint8_t port_1 = 3;

    add_ip(port_0, ip0, vlan, mac0);
    add_ip(port_0, ip01, vlan, mac1);
    add_ip(port_0, ipv6_0, vlan, mac1);
    add_next_hop(port_0, ip1, vlan);
    add_next_hop(port_0, ipv6_1, vlan);

    add_ip(port_1, ip1, vlan, mac2);
    add_ip(port_1, ipv6_1, vlan, mac2);
    add_next_hop(port_1, ip0, vlan);
    add_next_hop(port_1, ip01, vlan);
    add_next_hop(port_1, ipv6_0, vlan);

    dump(stdout);
    send_grat_arp_all();
    resolve_all();
    dump(stdout);

    if (!get_mac(port_0, ip1, vlan, found_mac)) {
        fprintf(stderr, "Test failed: Could not find %x on port %d\n", ip1, port_0);
        exit(1);
    }
    if (memcmp(found_mac, mac2, ETHER_ADDR_LEN)) {
        fprintf(stderr, "Test failed: dest %x on port %d badly resolved\n", ip1, port_0);
        exit(1);
    }

    if (!get_mac(port_1, ip0, vlan, found_mac)) {
        fprintf(stderr, "Test failed: Could not find %x on port %d\n", ip0, port_1);
        exit(1);
    }
    if (memcmp(found_mac, mac0, ETHER_ADDR_LEN)) {
        fprintf(stderr, "Test failed: dest %x on port %d badly resolved\n", ip0, port_1);
        exit(1);
    }

    printf("Test passed\n");
    exit(0);
}
