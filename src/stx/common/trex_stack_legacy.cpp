/*
  TRex team
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

#include "bp_sim.h"
#include "trex_stack_legacy.h"
#include "pkt_gen.h"


/***************************************
*            CStackLegacy              *
***************************************/

CStackLegacy::CStackLegacy(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) : CStackBase(api, ignore_stats) {
    debug("Legacy stack ctor");
}

CStackLegacy::~CStackLegacy(void) {
    debug("Legacy stack dtor");
}

CNodeBase* CStackLegacy::add_node_internal(const std::string &mac_buf) {
    assert(m_nodes.size()==0);
    CLegacyNode *node = new CLegacyNode(mac_buf);
    if (node == nullptr) {
        string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
        throw TrexException("Could not create node " + mac_str + "!");
    }
    m_nodes[mac_buf] = node;
    return node;
}

void CStackLegacy::del_node_internal(const std::string &mac_buf) {
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair == m_nodes.end() ) {
        string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
        throw TrexException("Node with MAC " + mac_str + " does not exist!");
    }
    delete iter_pair->second;
    m_nodes.erase(iter_pair->first);
}

uint16_t CStackLegacy::get_capa(void) {
    return (FAST_OPS);
}

void CStackLegacy::grat_to_json(Json::Value &res) {
    assert(!m_is_running_tasks); // CP should check this
    bool is_active = is_grat_active();
    res["is_active"] = is_active;
    if ( is_active ) {
        res["interval_sec"] = CGlobalInfo::m_options.m_arp_ref_per;
    }
}

void CStackLegacy::handle_pkt(const rte_mbuf_t *m) {
    try {
        RXPktParser parser(m);
        // verify that packet matches the port VLAN config
        if ( m_port_node->get_vlan() != parser.m_vlan_ids ) {
            return;
        }
        if (parser.m_icmp) {
            handle_icmp(parser);
        } else if (parser.m_arp) {
            handle_arp(parser);
        } else {
            return;
        }
    } catch (const TrexException &e) {
        return;
    }
}

void CStackLegacy::handle_icmp(RXPktParser &parser) {
    
    /* maybe not for us... */
    uint32_t sip = *(uint32_t *)m_port_node->get_src_ip4().c_str();
    if (parser.m_ipv4->myDestination != sip) {
        return;
    }
    
    /* we handle only echo request */
    if (parser.m_icmp->getType() != ICMPHeader::TYPE_ECHO_REQUEST) {
        return;
    }
    
    /* duplicate the MBUF */
    rte_mbuf_t *response = duplicate_mbuf(parser.m_mbuf);
    if (!response) {
        return;
    }
    
    /* reparse the cloned packet */
    RXPktParser response_parser(response);
    
    /* swap MAC */
    MacAddress tmp = response_parser.m_ether->mySource;
    response_parser.m_ether->mySource = response_parser.m_ether->myDestination;
    response_parser.m_ether->myDestination = tmp;
    
    /* swap IP */
    swap(response_parser.m_ipv4->mySource, response_parser.m_ipv4->myDestination);
    
    /* new packet - new TTL */
    response_parser.m_ipv4->setTimeToLive(128);
    response_parser.m_ipv4->updateCheckSum();
    
    /* update type and fix checksum */
    response_parser.m_icmp->setType(ICMPHeader::TYPE_ECHO_REPLY);
    response_parser.m_icmp->updateCheckSum(response_parser.m_ipv4->getTotalLength() - response_parser.m_ipv4->getHeaderLength());
    
    /* send */
    bool rc = m_api->tx_pkt(response);
    if (!rc) {
        rte_pktmbuf_free(response);
    }
}

void CStackLegacy::handle_arp(RXPktParser &parser) {
    MacAddress src_mac;
    
    /* only ethernet format supported */
    if (parser.m_arp->getHrdType() != ArpHdr::ARP_HDR_HRD_ETHER) {
        return;
    }
    
    /* IPV4 only */    
    if (parser.m_arp->getProtocolType() != ArpHdr::ARP_HDR_PROTO_IPV4) {
        return;
    }

    /* support only for ARP request */
    if (parser.m_arp->getOp() != ArpHdr::ARP_HDR_OP_REQUEST) {
        return;
    }
    
    /* are we the target ? if not - go home */
    if (parser.m_arp->m_arp_tip != *(uint32_t *) m_port_node->get_src_ip4().c_str()) {
        return;
    }

    /* duplicate the MBUF */
    rte_mbuf_t *response = duplicate_mbuf(parser.m_mbuf);
    if (!response) {
        return;
    }

    /* reparse the cloned packet */
    RXPktParser response_parser(response);
    
    /* reply */
    response_parser.m_arp->setOp(ArpHdr::ARP_HDR_OP_REPLY);
    
    /* fix the MAC addresses */
    src_mac.set((uint8_t *)m_port_node->get_src_mac().c_str());
    response_parser.m_ether->mySource = src_mac;
    response_parser.m_ether->myDestination = parser.m_ether->mySource;
    
    /* fill up the fields */
    
    /* src */
    response_parser.m_arp->m_arp_sha = src_mac;
    response_parser.m_arp->setSip(parser.m_arp->getTip());
    
    /* dst */
    response_parser.m_arp->m_arp_tha = parser.m_arp->m_arp_sha;
    response_parser.m_arp->m_arp_tip = parser.m_arp->m_arp_sip;
    
    /* send */
    bool rc = m_api->tx_pkt(response);
    if (!rc) {
        rte_pktmbuf_free(response);
    }
}

bool CStackLegacy::is_grat_active(void) {
    return m_port_node->get_src_ip4().size() && !m_port_node->is_loopback() && m_port_node->is_dst_mac_valid();
}

uint16_t CStackLegacy::handle_tx(uint16_t limit) {
    uint16_t sent = 0;

    if ( is_grat_active() && now_sec() >= m_next_grat_arp_sec ) {
        m_next_grat_arp_sec += (double)CGlobalInfo::m_options.m_arp_ref_per;
        debug("Sending GARP");
        send_grat_arp();
        sent++;
    }

    return sent;
}

uint16_t CStackLegacy::send_grat_arp(void) {
    uint8_t port_id = m_api->get_port_id();
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_small(CGlobalInfo::m_socket.port_to_socket(port_id));
    assert(m);
    uint8_t *p = (uint8_t *)rte_pktmbuf_append(m, 60);

    uint8_t *src_mac = (uint8_t *)m_port_node->get_src_mac().c_str();
    uint32_t sip = PKT_HTONL(*(uint32_t *)m_port_node->get_src_ip4().c_str());
    const vlan_list_t &vlans = m_port_node->get_vlan();

    switch (vlans.size()) {
    case 0:
        /* no VLAN */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, port_id);
        break;
    case 1:
        /* single VLAN */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, port_id, vlans[0]);
        break;
    case 2:
        /* QinQ */
        CTestPktGen::create_arp_req(p, sip, sip, src_mac, port_id, vlans[0], vlans[1]);
    }

    if (m_api->tx_pkt(m)) {
        m_ignore_stats->m_tx_arp++;
        m_ignore_stats->m_tot_bytes += 64;
        return 1;
    }
    rte_pktmbuf_free(m);
    return 0;
}

rte_mbuf_t* CStackLegacy::duplicate_mbuf(const rte_mbuf_t *m) {
    
    /* allocate */
    rte_mbuf_t *clone_mbuf = CGlobalInfo::pktmbuf_alloc_by_port(m_api->get_port_id(), rte_pktmbuf_pkt_len(m));
    assert(clone_mbuf);
    
    /* append data - should always succeed */
    uint8_t *dest = (uint8_t *)rte_pktmbuf_append(clone_mbuf, rte_pktmbuf_pkt_len(m));
    assert(dest);
    
    /* copy data */
    mbuf_to_buffer(dest, m);
    
    return clone_mbuf;
}


/***************************************
*             CLegacyNode              *
***************************************/

CLegacyNode::CLegacyNode(const string &mac_buf) {
    debug("Legacy node ctor");
    m_src_mac = mac_buf;
}

CLegacyNode::~CLegacyNode() {
    debug("Legacy node dtor");
}

void CLegacyNode::conf_vlan_internal(const vlan_list_t &vlans) {
    debug("Legacy stack: conf vlan internal");
    m_vlan_tags = vlans;
}

void CLegacyNode::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    debug("Legacy stack: conf IPv4 internal");
    m_ip4 = ip4_buf;
    m_gw4 = gw4_buf;
}

