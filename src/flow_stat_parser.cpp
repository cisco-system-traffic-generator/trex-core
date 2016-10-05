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

#include <netinet/in.h>
#include "common/basic_utils.h"
#include "common/Network/Packet/EthernetHeader.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "common/Network/Packet/TcpHeader.h"
#include "test_pkt_gen.h"
#include "flow_stat_parser.h"

void CFlowStatParser::reset() {
    m_start = 0;
    m_len = 0;
    m_ipv4 = 0;
    m_ipv6 = 0;
    m_l4_proto = 0;
    m_l4 = 0;
    m_vlan_offset = 0;
    m_stat_supported = false;
}

int CFlowStatParser::parse(uint8_t *p, uint16_t len) {
    EthernetHeader *ether = (EthernetHeader *)p;
    int min_len = ETH_HDR_LEN;
    reset();

    if (len < min_len)
        return -1;

    m_start = p;
    m_len = len;
    switch( ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        min_len += IPV4_HDR_LEN;
        if (len < min_len)
            return -1;
        m_ipv4 = (IPHeader *)(p + ETH_HDR_LEN);
        m_l4 = ((uint8_t *)m_ipv4) + m_ipv4->getHeaderLength();
        m_l4_proto = m_ipv4->getProtocol();
        m_stat_supported = true;
        break;
    case EthernetHeader::Protocol::IPv6 :
        min_len += IPV6_HDR_LEN;
        if (len < min_len)
            return -1;
        m_ipv6 = (IPv6Header *)(p + ETH_HDR_LEN);
        m_stat_supported = true;
        break;
    case EthernetHeader::Protocol::VLAN :
        m_vlan_offset = 4;
        min_len += 4;
        if (len < min_len)
            return -1;
        switch ( ether->getVlanProtocol() ){
        case EthernetHeader::Protocol::IP:
            min_len += IPV4_HDR_LEN;
            if (len < min_len)
                    return -1;
            m_ipv4 = (IPHeader *)(p + ETH_HDR_LEN + 4);
            m_l4 = ((uint8_t *)m_ipv4) + m_ipv4->getHeaderLength();
            m_l4_proto = m_ipv4->getProtocol();
            m_stat_supported = true;
            break;
        case EthernetHeader::Protocol::IPv6 :
            min_len += IPV6_HDR_LEN;
            if (len < min_len)
                return -1;
            m_ipv6 = (IPv6Header *)(p + ETH_HDR_LEN + 4);
            m_stat_supported = true;
            break;
        default:
            m_stat_supported = false;
            return -1;
        }

        break;
    default:
        m_stat_supported = false;
        return -1;
        break;
    }

    return 0;
}

int CFlowStatParser::get_ip_id(uint32_t &ip_id) {
    if (m_ipv4) {
        ip_id = m_ipv4->getId();
        return 0;
    }

    if (m_ipv6) {
        ip_id = m_ipv6->getFlowLabel();
        return 0;
    }

    return -1;
}

int CFlowStatParser::set_ip_id(uint32_t new_id) {
    if (m_ipv4) {
        // Updating checksum, not recalculating, so if someone put bad checksum on purpose, it will stay bad
        m_ipv4->updateCheckSum(PKT_NTOHS(m_ipv4->getId()), PKT_NTOHS(new_id));
        m_ipv4->setId(new_id);
        return 0;
    }

    if (m_ipv6) {
        m_ipv6->setFlowLabel(new_id);
        return 0;
    }
    return -1;
}

int CFlowStatParser::get_l3_proto(uint16_t &proto) {
    if (m_ipv4) {
        proto = EthernetHeader::Protocol::IP;
        return 0;
    }

    if (m_ipv6) {
        proto = EthernetHeader::Protocol::IPv6;
        return 0;
    }

    return -1;
}

int CFlowStatParser::get_l4_proto(uint8_t &proto) {
    if (m_ipv4) {
        proto = m_ipv4->getProtocol();
        return 0;
    }

    if (m_ipv6) {
        if (!m_l4) {
            uint16_t payload_len;
            // in IPv6 we calculate l4 proto only when running get_payload_len
            get_payload_len(m_start, m_len, payload_len);
        }
        proto = m_l4_proto;
        return 0;
    }

    return -1;
}

uint16_t CFlowStatParser::get_pkt_size() {
    uint16_t ip_len=0;

    if (m_ipv4) {
        ip_len = m_ipv4->getTotalLength();
    } else if (m_ipv6) {
        ip_len = m_ipv6->getHeaderLength() + m_ipv6->getPayloadLen();
    }
    return ( ip_len + m_vlan_offset + ETH_HDR_LEN);
}

uint8_t CFlowStatParser::get_ttl(){
    if (m_ipv4) {
        return ( m_ipv4->getTimeToLive() );
    }
    if (m_ipv6) {
        return ( m_ipv6->getHopLimit() );
    }
    return (0);
}

// calculate the payload len. Do not want to do this in parse(), since this is required only in
// specific cases, while parse is used in many places (including on packet RX path, where we want to be as fast as possible)
int CFlowStatParser::get_payload_len(uint8_t *p, uint16_t len, uint16_t &payload_len) {
    uint16_t l2_header_len;
    uint16_t l4_header_len;
    uint8_t *p_l3 = NULL;
    uint8_t *p_l4 = NULL;
    TCPHeader *p_tcp = NULL;
    if (!m_ipv4 && !m_ipv6) {
        payload_len = 0;
        return -1;
    }

    if (m_ipv4) {
        l2_header_len = ((uint8_t *)m_ipv4) - p;
        m_l4_proto = m_ipv4->getProtocol();
        p_l3 = (uint8_t *)m_ipv4;
        p_l4 = p_l3 + m_ipv4->getHeaderLength();
    } else if (m_ipv6) {
        l2_header_len = ((uint8_t *)m_ipv6) - p;
        m_l4_proto = m_ipv6->getl4Proto((uint8_t *)m_ipv6, len - l2_header_len, p_l4);
    }

    switch (m_l4_proto) {
    case IPPROTO_UDP:
        l4_header_len = 8;
        break;
    case IPPROTO_TCP:
        if ((p_l4 + TCP_HEADER_LEN) > (p + len)) {
            //Not enough space for TCP header
            payload_len = 0;
            return -2;
        }
        p_tcp = (TCPHeader *)p_l4;
        l4_header_len = p_tcp->getHeaderLength();
        break;
    case IPPROTO_ICMP:
        l4_header_len = 8;
        break;
    default:
        l4_header_len = 0;
        break;
    }

    payload_len = len - (p_l4 - p) - l4_header_len;

    if (payload_len <= 0) {
        payload_len = 0;
        return -3;
    }

    return 0;
}

static const uint16_t TEST_IP_ID = 0xabcd;
static const uint16_t TEST_IP_ID2 = 0xabcd;
static const uint8_t TEST_L4_PROTO = IPPROTO_UDP;


int CFlowStatParserTest::verify_pkt_one_parser(uint8_t * p, uint16_t pkt_size, uint16_t payload_len, uint32_t ip_id
                                               , uint8_t l4_proto, CFlowStatParser &parser, bool sup_pkt) {
    int ret;
    uint32_t pkt_ip_id = 0;
    uint8_t pkt_l4_proto;
    uint16_t pkt_payload_len;

    ret = parser.parse(p, pkt_size);
    if (sup_pkt) {
        assert (ret == 0);
        parser.get_ip_id(pkt_ip_id);
        assert(pkt_ip_id == ip_id);
        parser.set_ip_id(TEST_IP_ID2);
        // utl_DumpBuffer(stdout, test_pkt, sizeof(test_pkt), 0);
        parser.get_ip_id(ip_id);
        assert(ip_id == TEST_IP_ID2);
        if (parser.m_ipv4)
            assert(parser.m_ipv4->isChecksumOK() == true);
        assert(parser.get_l4_proto(pkt_l4_proto) == 0);
        assert(pkt_l4_proto == l4_proto);
        assert(parser.m_stat_supported == true);
        ret = parser.get_payload_len(p, pkt_size, pkt_payload_len);
        assert(ret == 0);
        assert(pkt_payload_len == payload_len);
    } else {
        assert(ret != 0);
        assert(parser.m_stat_supported == false);
    }

    return 0;
}

int CFlowStatParserTest::verify_pkt(uint8_t *p, uint16_t pkt_size, uint16_t payload_len, uint32_t ip_id, uint8_t l4_proto
                                    , uint16_t flags) {
    int ret, ret_val;
    CFlowStatParser parser;
    C82599Parser parser82599(false);
    C82599Parser parser82599_vlan(true);

    printf ("  ");
    if ((flags & P_OK) || (flags & P_BAD)) {
        printf("general parser");
        ret = verify_pkt_one_parser(p, pkt_size, payload_len, ip_id, l4_proto, parser, flags & P_OK);
        ret_val = ret;
        if (ret == 0)
            printf("-OK");
        else {
            printf("-BAD");
        }
    }
    if ((flags & P82599_OK) || (flags & P82599_BAD)) {
        printf(", 82599 parser");
        ret = verify_pkt_one_parser(p, pkt_size, payload_len, ip_id, l4_proto, parser82599, flags & P82599_OK);
        ret_val |= ret;
        if (ret == 0)
            printf("-OK");
        else {
            printf("-BAD");
        }
    }
    if ((flags & P82599_VLAN_OK) || (flags & P82599_VLAN_BAD)) {
        printf(", 82599 vlan parser");
        ret = verify_pkt_one_parser(p, pkt_size, payload_len, ip_id, l4_proto, parser82599_vlan, flags & P82599_VLAN_OK);
        ret_val |= ret;
        if (ret == 0)
            printf("-OK");
        else {
            printf("-BAD");
        }
    }
    printf("\n");

    return 0;
}

int CFlowStatParserTest::test_one_pkt(const char *name, uint16_t ether_type, uint8_t l4_proto, bool is_vlan
                                      , uint16_t verify_flags) {
    CTestPktGen gen;
    uint8_t *p;
    int pkt_size;
    uint16_t payload_len = 16;
    uint16_t pkt_flags;
    int ret = 0;

    printf("%s - ", name);

    // in case of IPv6, we add rx_check header, just to make sure we now how to parse with multiple headers
    if (is_vlan) {
        pkt_flags = DPF_VLAN | DPF_RXCHECK;
    } else {
        pkt_flags = DPF_RXCHECK;
    }

    p = (uint8_t *)gen.create_test_pkt(ether_type, l4_proto, 255, TEST_IP_ID, pkt_flags, payload_len, pkt_size);
    ret = verify_pkt(p, pkt_size, payload_len, TEST_IP_ID, l4_proto, verify_flags);
    free(p);

    return ret;
}

int CFlowStatParserTest::test() {
    bool vlan = true;
    uint8_t tcp = IPPROTO_TCP, udp = IPPROTO_UDP, icmp = IPPROTO_ICMP;
    uint16_t ipv4 = EthernetHeader::Protocol::IP, ipv6 = EthernetHeader::Protocol::IPv6;

    test_one_pkt("IPv4 TCP", ipv4, tcp, !vlan, P_OK | P82599_OK | P82599_VLAN_BAD);
    test_one_pkt("IPv4 TCP VLAN", ipv4, tcp, vlan, P_OK | P82599_BAD | P82599_VLAN_OK);
    test_one_pkt("IPv4 UDP", ipv4, udp, !vlan, P_OK | P82599_OK | P82599_VLAN_BAD);
    test_one_pkt("IPv4 UDP VLAN", ipv4, udp, vlan, P_OK | P82599_BAD | P82599_VLAN_OK);
    test_one_pkt("IPv4 ICMP", ipv4, icmp, !vlan, P_OK | P82599_OK | P82599_VLAN_BAD);
    test_one_pkt("IPv4 ICMP VLAN", ipv4, icmp, vlan, P_OK | P82599_BAD | P82599_VLAN_OK);
    test_one_pkt("IPv6 TCP", ipv6, tcp, !vlan, P_OK | P82599_BAD | P82599_VLAN_BAD);
    test_one_pkt("IPv6 TCP VLAN", ipv6, tcp, vlan, P_OK | P82599_BAD | P82599_VLAN_BAD);
    test_one_pkt("IPv6 UDP", ipv6, udp, !vlan, P_OK | P82599_BAD | P82599_VLAN_BAD);
    test_one_pkt("IPv6 UDP VLAN", ipv6, udp, vlan, P_OK | P82599_BAD | P82599_VLAN_BAD);
    test_one_pkt("IPv4 IGMP", ipv4, IPPROTO_IGMP, !vlan, P_OK | P82599_OK | P82599_VLAN_BAD);
    test_one_pkt("BAD l3 type", 0xaa, icmp, !vlan, P_BAD | P82599_BAD | P82599_VLAN_BAD);
    test_one_pkt("VLAN + BAD l3 type", 0xaa, icmp, vlan, P_BAD | P82599_BAD | P82599_VLAN_BAD);

    return 0;
}

// In 82599 10G card we do not support VLANs
int C82599Parser::parse(uint8_t *p, uint16_t len) {
    EthernetHeader *ether = (EthernetHeader *)p;
    int min_len = ETH_HDR_LEN + IPV4_HDR_LEN;
    reset();

    if (len < min_len)
        return -1;

    switch( ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        // In 82599 all streams should be with vlan, or without. Can't mix
        if (m_vlan_supported)
            return -1;
        m_ipv4 = (IPHeader *)(p + ETH_HDR_LEN);
        m_stat_supported = true;
        break;
    case EthernetHeader::Protocol::VLAN :
        if (!m_vlan_supported)
            return -1;
        min_len += 4;
        if (len < min_len)
            return -1;
        switch ( ether->getVlanProtocol() ){
        case EthernetHeader::Protocol::IP:
            m_ipv4 = (IPHeader *)(p + 18);
            m_stat_supported = true;
            break;
        default:
            m_stat_supported = false;
            return -1;
        }
        break;
    default:
        m_stat_supported = false;
        return -1;
        break;
    }

    return 0;
}

int CPassAllParser::parse(uint8_t *pkt, uint16_t len) {
    reset();

    if (len < ETH_HDR_LEN)
        return -1;

    m_len = len;

    return 0;
}

bool CSimplePacketParser::Parse(){

    rte_mbuf_t * m=m_m;
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    EthernetHeader *m_ether = (EthernetHeader *)p;
    IPHeader * ipv4=0;
    IPv6Header * ipv6=0;
    m_vlan_offset=0;
    uint8_t protocol = 0;

    // Retrieve the protocol type from the packet
    switch( m_ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        // IPv4 packet
        ipv4=(IPHeader *)(p+14);
        m_l4 = (uint8_t *)ipv4 + ipv4->getHeaderLength();
        protocol = ipv4->getProtocol();
        break;
    case EthernetHeader::Protocol::IPv6 :
        // IPv6 packet
        ipv6=(IPv6Header *)(p+14);
        m_l4 = (uint8_t *)ipv6 + ipv6->getHeaderLength();
        protocol = ipv6->getNextHdr();
        break;
    case EthernetHeader::Protocol::VLAN :
        m_vlan_offset = 4;
        switch ( m_ether->getVlanProtocol() ){
        case EthernetHeader::Protocol::IP:
            // IPv4 packet
            ipv4=(IPHeader *)(p+18);
            m_l4 = (uint8_t *)ipv4 + ipv4->getHeaderLength();
            protocol = ipv4->getProtocol();
            break;
        case EthernetHeader::Protocol::IPv6 :
            // IPv6 packet
            ipv6=(IPv6Header *)(p+18);
            m_l4 = (uint8_t *)ipv6 + ipv6->getHeaderLength();
            protocol = ipv6->getNextHdr();
            break;
        default:
        break;
        }
        default:
        break;
    }
    m_protocol =protocol;
    m_ipv4=ipv4;
    m_ipv6=ipv6;

    if ( protocol == 0 ){
        return (false);
    }
    return (true);
}
