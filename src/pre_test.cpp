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

void CPretestPortInfo::set_params(CPerPortIPCfg port_cfg, const uint8_t *src_mac, bool resolve_needed) {
    m_ip = port_cfg.get_ip();
    m_def_gw = port_cfg.get_def_gw();
    m_vlan = port_cfg.get_vlan();
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

/*
  put in mac relevant dest MAC for port/ip pair.
  return false if no relevant info exists, true otherwise.
 */
bool CPretest::get_mac(uint16_t port_id, uint32_t ip, uint8_t *mac) {
    assert(port_id < TREX_MAX_PORTS);

    if (m_port_info[port_id].m_state != CPretestPortInfo::RESOLVE_DONE) {
        return false;
    }

    memcpy(mac, &m_port_info[port_id].m_dst_mac, sizeof(m_port_info[port_id].m_dst_mac));

    return true;
}

CPreTestStats CPretest::get_stats(uint16_t port_id) {
    assert(port_id < TREX_MAX_PORTS);

    return m_port_info[port_id].m_stats;
}

bool CPretest::is_loopback(uint16_t port) {
    assert(port < TREX_MAX_PORTS);

    return m_port_info[port].m_is_loopback;
}

void CPretest::set_port_params(uint16_t port_id, const CPerPortIPCfg &port_cfg, const uint8_t *src_mac, bool resolve_needed) {
    if (port_id >= m_max_ports)
        return;

    m_port_info[port_id].set_params(port_cfg, src_mac, resolve_needed);
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

        for (i = 0; i < cnt; i++) {
            rte_mbuf_t * m = rx_pkts[i];
            int pkt_size = rte_pktmbuf_pkt_len(m);
            uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);
            ArpHdr *arp;
            CPretestPortInfo *port = &m_port_info[port_id];
            if (is_arp(p, pkt_size, arp)) {
                m_port_info[port_id].m_stats.m_rx_arp++;
                if (arp->m_arp_op == htons(ArpHdr::ARP_HDR_OP_REQUEST)) {
                    if (verbose >= 3) {
                        fprintf(stdout, "RX ARP request on port %d queue %d sip:0x%08x tip:0x%08x\n", port_id, queue_id
                                , ntohl(arp->m_arp_sip)
                                , ntohl(arp->m_arp_tip));
                    }
                    // is this request for our IP?
                    if (ntohl(arp->m_arp_tip) == port->m_ip) {
                        // If our request(i.e. we are connected in loopback)
                        // , do a shortcut, and write info directly to asking port
                        uint8_t magic[5] = {0x1, 0x3, 0x5, 0x7, 0x9};
                        if (! memcmp((uint8_t *)&arp->m_arp_tha.data, magic, 5)) {
                            uint8_t sent_port_id = arp->m_arp_tha.data[5];
                            if ((sent_port_id < m_max_ports) &&
                                (m_port_info[sent_port_id].m_def_gw == port->m_ip)) {
                                memcpy(m_port_info[sent_port_id].m_dst_mac, port->m_src_mac, ETHER_ADDR_LEN);
                                m_port_info[sent_port_id].m_state = CPretestPortInfo::RESOLVE_DONE;
                                m_port_info[sent_port_id].m_is_loopback = true;
                            }
                        }
                    } else {
                        // ARP request not to our IP. At the moment, we ignore this.
                    }
                } else {
                    if (arp->m_arp_op == htons(ArpHdr::ARP_HDR_OP_REPLY)) {
                        if (verbose >= 3) {
                            fprintf(stdout, "RX ARP response on port %d queue %d sip:0x%08x tip:0x%08x\n", port_id, queue_id
                                    , ntohl(arp->m_arp_sip)
                                    , ntohl(arp->m_arp_tip));
                        }
                        // If this is response to our request, update our tables
                        if (port->m_def_gw == ntohl(arp->m_arp_sip)) {
                            port->set_dst_mac((uint8_t *)&arp->m_arp_sha);
                        }
                    }
                }
            }
            rte_pktmbuf_free(m);
        }
    } while ((cnt != 0) && (tries < 1000));

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

// Send ARP request for our default gateway on port
// If is_grat is true - send gratuitous ARP.
void CPretest::send_arp_req(uint16_t port_id, bool is_grat) {
    rte_mbuf_t *m[1];
    int num_sent;
    int verbose = CGlobalInfo::m_options.preview.getVMode();

    m[0] = CGlobalInfo::pktmbuf_alloc_small(0);
    if ( unlikely(m[0] == 0) )  {
        fprintf(stderr, "ERROR: Could not allocate mbuf for sending ARP to port:%d\n", port_id);
        exit(1);
    }

    uint32_t tip;
    uint8_t *p = (uint8_t *)rte_pktmbuf_append(m[0], 60); // ARP packet is shorter than 60
    uint32_t sip = m_port_info[port_id].m_ip;
    uint8_t *src_mac = m_port_info[port_id].m_src_mac;
    uint16_t vlan = m_port_info[port_id].m_vlan;
    if (is_grat) {
        tip = sip;
    } else {
        tip = m_port_info[port_id].m_def_gw;
    }

    if (verbose >= 3) {
        fprintf(stdout, "TX %s port:%d sip:0x%08x, tip:0x%08x\n"
                , is_grat ? "grat ARP": "ARP request", port_id ,sip, tip);
    }

    CTestPktGen::create_arp_req(p, sip, tip, src_mac, vlan, port_id);
    num_sent = rte_eth_tx_burst(port_id, 0, m, 1);
    if (num_sent < 1) {
        fprintf(stderr, "Failed sending ARP to port:%d\n", port_id);
        exit(1);
    } else {
        m_port_info[port_id].m_stats.m_tx_arp++;
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

bool CPretest::is_arp(const uint8_t *p, uint16_t pkt_size, ArpHdr *&arp) {
    EthernetHeader *m_ether = (EthernetHeader *)p;

    if ((pkt_size < 60) ||
        ((m_ether->getNextProtocol() != EthernetHeader::Protocol::ARP)
         && (m_ether->getNextProtocol() != EthernetHeader::Protocol::VLAN)))
        return false;

    if (m_ether->getNextProtocol() == EthernetHeader::Protocol::ARP) {
        arp = (ArpHdr *)(p + 14);
    } else {
        if (m_ether->getVlanProtocol() != EthernetHeader::Protocol::ARP) {
            return false;
        } else {
            arp = (ArpHdr *)(p + 18);
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

    CPerPortIPCfg port_cfg0;
    CPerPortIPCfg port_cfg1;
    port_cfg0.set_ip(ip0);
    port_cfg0.set_def_gw(ip1);
    port_cfg0.set_vlan(0);
    port_cfg1.set_ip(ip1);
    port_cfg1.set_def_gw(ip0);
    port_cfg1.set_vlan(0);

    set_port_params(0, port_cfg0, mac0, true);
    set_port_params(1, port_cfg1, mac1, true);
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
