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

#ifndef __FLOW_STAT_PARSER_H__
#define __FLOW_STAT_PARSER_H__

#include <arpa/inet.h>
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "common/Network/Packet/TcpHeader.h"
#include "mbuf.h"

// Basic flow stat parser. Relevant for xl710/x710/x350 cards
class CFlowStatParser {
    friend class CFlowStatParserTest;
 public:
    virtual ~CFlowStatParser() {}
    virtual void reset();
    virtual int parse(uint8_t *pkt, uint16_t len);
    virtual bool is_stat_supported() {return m_stat_supported == true;}
    virtual int get_ip_id(uint32_t &ip_id);
    virtual int set_ip_id(uint32_t ip_id);
    virtual int get_l3_proto(uint16_t &proto);
    virtual int get_l4_proto(uint8_t &proto);
    virtual int get_payload_len(uint8_t *p, uint16_t len, uint16_t &payload_len);
    virtual uint16_t get_pkt_size();
    virtual uint8_t get_ttl();
    uint8_t *get_l3() {
        if (m_ipv4)
            return (uint8_t *)m_ipv4;
        else
            return (uint8_t *)m_ipv6;
    }
    uint8_t * get_l4() {return m_l4;}

    inline bool IsNatInfoPkt(bool &first) {
        if (!m_ipv4 || (m_l4_proto != IPPROTO_TCP)) {
            return false;
        }
        if ((m_l4 + TCP_HEADER_LEN) > (uint8_t *)m_ipv4 + get_pkt_size()) {
            return false;
        }
        // If we are here, relevant fields from tcp header are inside the packet boundaries
        // We want to handle SYN and SYN+ACK packets
        TCPHeader *tcp = (TCPHeader *)m_l4;
        if (! tcp->getSynFlag())
            return false;

        if (! tcp->getAckFlag()) {
            first = true;
        } else {
            first = false;
        }
        return true;
    }

 private:
    char *create_test_pkt(int ip_ver, uint16_t l4_proto, uint8_t ttl
                          , uint32_t ip_id, uint16_t flags, int &pkt_size);

 protected:
    IPHeader *m_ipv4;
    IPv6Header *m_ipv6;
    uint8_t *m_l4;
    bool m_stat_supported;
    uint8_t m_l4_proto;
    uint8_t m_vlan_offset;
};

// relevant for 82599 card
class C82599Parser : public CFlowStatParser {
 public:
    C82599Parser(bool vlan_supported) {m_vlan_supported = vlan_supported;}
    ~C82599Parser() {}
    int parse(uint8_t *pkt, uint16_t len);

 private:
    bool m_vlan_supported;
};

// Used for latency statefull packets. Need to be merged with above parser
class CSimplePacketParser {
 public:
    CSimplePacketParser(rte_mbuf_t * m){
        m_m = m;
        m_l4 = NULL;
    }
    bool Parse();

    // Check if this packet contains NAT info in TCP ack
    // first - set to true if this is the first packet of the flow. false otherwise.
    //         relevant only if return value is true
    inline bool IsNatInfoPkt(bool &first) {
        if (!m_ipv4 || (m_protocol != IPPROTO_TCP)) {
            return false;
        }
        if (! m_l4 || (m_l4 - rte_pktmbuf_mtod(m_m, uint8_t*) + TCP_HEADER_LEN) > m_m->data_len) {
            return false;
        }
        // If we are here, relevant fields from tcp header are guaranteed to be in first mbuf
        // We want to handle SYN and SYN+ACK packets
        TCPHeader *tcp = (TCPHeader *)m_l4;
        if (! tcp->getSynFlag())
            return false;

        if (! tcp->getAckFlag()) {
            first = true;
        } else {
            first = false;
        }
        return true;
    }

 public:
    IPHeader *      m_ipv4;
    IPv6Header *    m_ipv6;
    uint8_t         m_protocol;
    uint16_t        m_vlan_offset;
    uint8_t *       m_l4;
 private:
    rte_mbuf_t *    m_m;
};

class CFlowStatParserTest {
    enum {
        P_OK = 0x1,
        P_BAD = 0x2,
        P82599_OK = 0x4,
        P82599_BAD = 0x8,
        P82599_VLAN_OK = 0x10,
        P82599_VLAN_BAD = 0x20,
    };

 public:
    int test();

 private:
    int test_one_pkt(const char *name, uint16_t ether_type, uint8_t l4_proto, bool is_vlan, uint16_t verify_flags);
    int verify_pkt(uint8_t *p, uint16_t pkt_size, uint16_t payload_len, uint32_t ip_id, uint8_t l4_proto, uint16_t flags);
    int verify_pkt_one_parser(uint8_t * p, uint16_t pkt_size, uint16_t payload_len, uint32_t ip_id, uint8_t l4_proto
                              , CFlowStatParser &parser, bool sup_pkt);
};

#endif
