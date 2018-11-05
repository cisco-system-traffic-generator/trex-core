/*
  Itay Marom
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
#include "trex_rx_port_mngr.h"
#include "trex_rx_core.h"
#include "common/Network/Packet/Arp.h"
#include "common/Network/Packet/VLANHeader.h"
#include "pkt_gen.h"
#include "trex_rx_packet_parser.h"


RXPktParser::RXPktParser(const rte_mbuf_t *mbuf) {
    m_mbuf = mbuf;

    /* start point */
    m_current   = rte_pktmbuf_mtod(mbuf, uint8_t *);
    m_size_left = rte_pktmbuf_pkt_len(mbuf);

    m_ether    = NULL;
    m_arp      = NULL;
    m_ipv4     = NULL;
    m_icmp     = NULL;

    /* parse L2 (stripping VLANs) */
    uint16_t next_proto = parse_l2();

    /**
     * support only for ARP or IPv4 based protocols
     */
    switch (next_proto) {
    case EthernetHeader::Protocol::ARP:
        parse_arp();
        return;

    case EthernetHeader::Protocol::IP:
        parse_ipv4();
        return;

    default:
        return;
    }
}
    
    
/**
 * parse L2 header 
 * returns the payload protocol (VLANs stripped)
 */
uint16_t RXPktParser::parse_l2(void) {
    /* ethernet */
    m_ether = (EthernetHeader *)parse_bytes(14);
    
    uint16_t next_proto = m_ether->getNextProtocol();
 
    while (true) {
        switch (next_proto) {
        case EthernetHeader::Protocol::QINQ:
        case EthernetHeader::Protocol::VLAN:
            {
                VLANHeader *vlan_hdr = (VLANHeader *)parse_bytes(4);
                m_vlan_ids.push_back(vlan_hdr->getTagID());

                next_proto = vlan_hdr->getNextProtocolHostOrder();
                
            }
            break;
            
        default:
            /* break */
            return next_proto;
        }
    }
    
}
    
    
const uint8_t *RXPktParser::parse_bytes(uint32_t size) {
    if (m_size_left < size) {
        parse_err();
    }
    
    const uint8_t *p = m_current;
    m_current    += size;
    m_size_left  -= size;
    
    return p;
}

void RXPktParser::parse_arp(void) {
    m_arp = (ArpHdr *)parse_bytes(sizeof(ArpHdr));
}

void RXPktParser::parse_ipv4(void) {
    m_ipv4 = (IPHeader *)parse_bytes(IPHeader::DefaultSize);
    
    /* advance over IP options if exists */
    parse_bytes(m_ipv4->getOptionLen());
    
    switch (m_ipv4->getNextProtocol()) {
    case IPHeader::Protocol::ICMP:
        parse_icmp();
        return;
        
    default:
        return;
    }
}

void RXPktParser::parse_icmp(void) {
    m_icmp = (ICMPHeader *)parse_bytes(sizeof(ICMPHeader));
}

void RXPktParser::parse_err(void) {
    throw TrexException(TrexException::T_RX_PKT_PARSE_ERR);
}

