/* 
?????
- read also from q 0
- read periodically - not wait 1 sec
flush q 0,1 at end
close q 0 at end (not close it at start)
test in trex-dan router for long time. remove static arp from router.
test 10g, vm
add also per profile


remove stuff in ???
make 10G work
documentation

 */

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

#include <rte_arp.h>
#include <rte_ethdev.h>
#include <arpa/inet.h>
#include <common/Network/Packet/EthernetHeader.h>
#include "common/basic_utils.h"
#include "bp_sim.h"
#include "main_dpdk.h"
#include "pre_test.h"


void CPretestPortInfo::set_params(uint32_t ip, const uint8_t *src_mac, uint32_t def_gw
                                  , bool resolve_needed) {
    m_ip = ip;
    m_def_gw = def_gw;
    memcpy(&m_src_mac, src_mac, sizeof(m_src_mac));
    if (resolve_needed) {
        m_state = CPretestPortInfo::RESOLVE_NEEDED;
    } else {
        m_state = CPretestPortInfo::RESOLVE_NOT_NEEDED;
    }
}

void CPretestPortInfo::set_dst_mac(const uint8_t *dst_mac) {
    memcpy(&m_dst_mac, dst_mac, sizeof(m_dst_mac));
    m_state = CPretestPortInfo::RESOLVE_DONE;
}

void CPretestPortInfo::dump(FILE *fd) {
    if (m_state == INIT_NEEDED) {
        return;
    }

    uint32_t ip = htonl(m_ip);
    
    fprintf(fd, "  ip:%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    ip = htonl(m_def_gw);
    fprintf(fd, "  default gw:%d.%d.%d.%d\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);

    printf("  src MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", m_src_mac[0], m_src_mac[1], m_src_mac[2], m_src_mac[3]
           , m_src_mac[4], m_src_mac[5]);
    printf(  "  dst MAC: ");
    if (m_state == RESOLVE_DONE) {
        printf("%02x:%02x:%02x:%02x:%02x:%02x\n", m_dst_mac[0], m_dst_mac[1], m_dst_mac[2], m_dst_mac[3]
               , m_dst_mac[4], m_dst_mac[5]);
    } else {
        printf("Not resolved\n");
    }
}

// Create ARP request for our default gateway.
// If is_grat is true - create gratuitous ARP.
// pkt_size will contain the size of buffer returned.
// Return pointer to buffer containing the packet.
uint8_t *CPretestPortInfo::create_arp_req(uint16_t &pkt_size, uint8_t port, bool is_grat) {
    pkt_size = 14 + sizeof (struct arp_hdr);
    uint16_t l2_proto = htons(EthernetHeader::Protocol::ARP);
    
    uint8_t *pkt = (uint8_t *)malloc(pkt_size);
    assert(pkt != NULL);   
    uint8_t *p = pkt;
    
    // dst MAC
    memset(p, 0xff, ETHER_ADDR_LEN);
    p += ETHER_ADDR_LEN;
    // src MAC
    memcpy(p, m_src_mac, ETHER_ADDR_LEN);
    p += ETHER_ADDR_LEN;
    // l3 type
    memcpy(p, &l2_proto, sizeof(l2_proto));
    p += 2;

    struct arp_hdr *arp = (struct arp_hdr *)p;
    arp->arp_hrd = htons(ARP_HRD_ETHER); // Format of hardware address
    arp->arp_pro = htons(EthernetHeader::Protocol::IP); // Format of protocol address
    arp->arp_hln = ETHER_ADDR_LEN; // Length of hardware address
    arp->arp_pln = 4; // Length of protocol address
    arp->arp_op = htons(ARP_OP_REQUEST); // ARP opcode (command)

    memcpy(&arp->arp_data.arp_sha, m_src_mac, ETHER_ADDR_LEN); // Sender MAC address
    arp->arp_data.arp_sip = htonl(m_ip); // Sender IP address
    uint8_t magic[5] = {0x1, 0x3, 0x5, 0x7, 0x9};
    memcpy(&arp->arp_data.arp_tha, magic, 5); // Target MAC address
    arp->arp_data.arp_tha.addr_bytes[5] = port;
    if (is_grat)
        arp->arp_data.arp_tip = htonl(m_ip); // gratuitous ARP is request for your own IP.
    else
        arp->arp_data.arp_tip = htonl(m_def_gw); // Target IP address
    
    return pkt;
}

/*
  put in mac relevant dest MAC for port/ip pair.
  return false if no relevant info exists, true otherwise.
 */
bool CPretest::get_mac(uint16_t port, uint32_t ip, uint8_t *mac) {
    if (port >= TREX_MAX_PORTS) {
        return false;
    }

    if (m_port_info[port].m_state != CPretestPortInfo::RESOLVE_DONE) {
        return false;
    }

    memcpy(mac, &m_port_info[port].m_dst_mac, sizeof(m_port_info[port].m_dst_mac));
        
    return true;    
}

void CPretest::set_port_params(uint16_t port, uint32_t ip, const uint8_t *src_mac, uint32_t def_gw,
                               bool resolve_needed) {
    if (port >= m_max_ports)
        return;

    m_port_info[port].set_params(ip, src_mac, def_gw, resolve_needed);
}


int CPretest::handle_rx(int port_id, int queue_id) {
    rte_mbuf_t * rx_pkts[32];
    uint16_t cnt;
    int i;

    cnt = rte_eth_rx_burst(port_id, queue_id, rx_pkts, sizeof(rx_pkts)/sizeof(rx_pkts[0]));
 
    for (i = 0; i < cnt; i++) {
        rte_mbuf_t * m = rx_pkts[i];
        int pkt_size = rte_pktmbuf_pkt_len(m);
        uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);
        struct arp_hdr *arp;
        CPretestPortInfo *port = &m_port_info[port_id];
        if (is_arp(p, pkt_size, arp)) {
                if (arp->arp_op == htons(ARP_OP_REQUEST)) {
                    // is this request for our IP?
                    if (ntohl(arp->arp_data.arp_tip) == port->m_ip) {
                        // If our request, do a shortcut, and write info directly to asking port
                        uint8_t magic[5] = {0x1, 0x3, 0x5, 0x7, 0x9};
                        if (! memcmp((uint8_t *)&arp->arp_data.arp_tha, magic, 5)) {
                            uint8_t sent_port_id = arp->arp_data.arp_tha.addr_bytes[5];
                            if ((sent_port_id < m_max_ports) &&
                                (m_port_info[sent_port_id].m_def_gw == port->m_ip)) {
                                memcpy(m_port_info[sent_port_id].m_dst_mac, port->m_src_mac, ETHER_ADDR_LEN);
                                m_port_info[sent_port_id].m_state = CPretestPortInfo::RESOLVE_DONE;
                            }
                        }
                    } else {
                        // ARP request not to our IP. At the moment, we ignore this.
                    }
                } else {
                    if (arp->arp_op == htons(ARP_OP_REPLY)) {
                        // If this is response to our request, update our tables
                        if (port->m_def_gw == ntohl(arp->arp_data.arp_sip)) {
                            port->set_dst_mac((uint8_t *)&arp->arp_data.arp_sha);
                        }
                    }
                }
        }       

        printf("port %d, queue:%d\n", port_id, queue_id);
        utl_DumpBuffer(stdout, p, pkt_size, 0); //??? remove
        rte_pktmbuf_free(m);
    }
    return 0;
}

/*
  Try to resolve def_gw address on all ports marked as needed.
  Return false if failed to resolve on one of the ports
 */
bool CPretest::resolve_all() {
    uint16_t port;

    // send ARP request on all ports
    for (port = 0; port < m_max_ports; port++) {
        if (m_port_info[port].m_state == CPretestPortInfo::RESOLVE_NEEDED) {
            send_arp_req(port, false);
        }
    }

    int max_tries = 1000;
    int i;
    for (i = 0; i < max_tries; i++) {
        bool all_resolved = true;
        for (port = 0; port < m_max_ports; port++) {
            if (m_port_info[port].m_state == CPretestPortInfo::RESOLVE_NEEDED) {
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
}

void CPretest::dump(FILE *fd) {
    for (int port = 0; port < m_max_ports; port++) {
        if (m_port_info[port].m_state != CPretestPortInfo::INIT_NEEDED) {
            fprintf(fd, "port %d:\n", port);
            m_port_info[port].dump(fd);
        }
    }
}

void CPretest::send_arp_req(uint16_t port, bool is_grat) {
    uint16_t pkt_size;
    uint8_t *pkt;
    rte_mbuf_t *m[1];
    char *p;
    int num_sent;

    pkt = m_port_info[port].create_arp_req(pkt_size, port, is_grat);
    m[0] = CGlobalInfo::pktmbuf_alloc(0, pkt_size);
    if ( unlikely(m[0] == 0) )  {
        fprintf(stderr, "ERROR: Could not allocate mbuf for sending ARP to port:%d\n", port);
        exit(1);
    }
    p = rte_pktmbuf_append(m[0], pkt_size);
    memcpy(p, pkt, pkt_size);
    free(pkt);

    num_sent = rte_eth_tx_burst(port, 0, m, 1);
    if (num_sent < 1) {
        fprintf(stderr, "Failed sending ARP to port:%d\n", port);
        exit(1);
    }
}

/*
  Send gratuitous ARP on all ports
 */
void CPretest::send_grat_arp_all() {
    for (uint16_t port = 0; port < m_max_ports; port++) {
        if (m_port_info[port].m_state == CPretestPortInfo::RESOLVE_NEEDED) {
            send_arp_req(port, true);
        }
    }
}

bool CPretest::is_arp(uint8_t *p, uint16_t pkt_size, struct arp_hdr *&arp) {
    EthernetHeader *m_ether = (EthernetHeader *)p;

    if ((pkt_size < 60) ||
        ((m_ether->getNextProtocol() != EthernetHeader::Protocol::ARP)
         && (m_ether->getNextProtocol() != EthernetHeader::Protocol::VLAN)))
        return false;

    if (m_ether->getNextProtocol() == EthernetHeader::Protocol::ARP) {
        arp = (struct arp_hdr *)(p + 14);
    } else {
        if (m_ether->getVlanProtocol() != EthernetHeader::Protocol::ARP) {
            return false;
        } else {
            arp = (struct arp_hdr *)(p + 18);
        }
    }

    return true;
}

// Should be run on setup with two interfaces connected by loopback.
// Before running, should put ports on receive all mode. 
void CPretest::test() {
    uint8_t found_mac[ETHER_ADDR_LEN];
    uint8_t mac0[ETHER_ADDR_LEN] = {0x90, 0xe2, 0xba, 0xae, 0x87, 0xd0};
    uint8_t mac1[ETHER_ADDR_LEN] = {0x90, 0xe2, 0xba, 0xae, 0x87, 0xd1};
    uint32_t ip0 = 0x0f000003;
    uint32_t ip1 = 0x0f000001;
    
    set_port_params(0, ip0, mac0, ip1, true);
    set_port_params(1, ip1, mac1, ip0, true);
    dump(stdout);
    resolve_all();
    dump(stdout);
    get_mac(0, ip1, found_mac);
    if (memcmp(found_mac, mac1, ETHER_ADDR_LEN)) {
        fprintf(stderr, "Test failed: Could not resolve def gw on port 0\n");
        exit(1);
    }

    get_mac(1, ip0, found_mac);
    if (memcmp(found_mac, mac0, ETHER_ADDR_LEN)) {
        fprintf(stderr, "Test failed: Could not resolve def gw on port 1\n");
        exit(1);
    }

    printf("Test passed\n");
    exit(0);
}
