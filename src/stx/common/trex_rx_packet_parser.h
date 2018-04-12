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

#ifndef __TREX_RX_PACKET_PARSER_H__
#define __TREX_RX_PACKET_PARSER_H__

#include "trex_pkt.h"
#include "common/Network/Packet/Arp.h"
#include "common/Network/Packet/VLANHeader.h"
#include "common/Network/Packet/IcmpHeader.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/VLANHeader.h"


/**************************************
 * RX feature server (ARP, ICMP) and etc.
 * 
 *************************************/

class RXPktParser {
public:
    RXPktParser(const rte_mbuf_t *mbuf);
    const rte_mbuf_t         *m_mbuf;
    EthernetHeader           *m_ether;
    ArpHdr                   *m_arp;
    IPHeader                 *m_ipv4;
    ICMPHeader               *m_icmp;
    std::vector<uint16_t>     m_vlan_ids;
    
protected:
    uint16_t parse_l2(void);
    const uint8_t *parse_bytes(uint32_t size);
    void parse_arp(void);
    void parse_ipv4(void);
    void parse_icmp(void);    
    void parse_err(void);

    const uint8_t *m_current;
    uint16_t       m_size_left;
};

#endif /* __TREX_RX_PACKET_PARSER_H__ */

