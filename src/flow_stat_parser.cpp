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
#include <common/basic_utils.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>
#include <flow_stat_parser.h>

void CFlowStatParser::reset() {
    m_ipv4 = 0;
    m_l4_proto = 0;
    m_stat_supported = false;
}

int CFlowStatParser::parse(uint8_t *p, uint16_t len) {
    EthernetHeader *ether = (EthernetHeader *)p;
    int min_len = ETH_HDR_LEN + IPV4_HDR_LEN;
    reset();

    if (len < min_len)
        return -1;

    switch( ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        m_ipv4 = (IPHeader *)(p + ETH_HDR_LEN);
        m_stat_supported = true;
        break;
    case EthernetHeader::Protocol::VLAN :
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

int CFlowStatParser::get_ip_id(uint16_t &ip_id) {
    if (! m_ipv4)
        return -1;

    ip_id = m_ipv4->getId();

    return 0;
}

int CFlowStatParser::set_ip_id(uint16_t new_id) {
    if (! m_ipv4)
        return -1;

    // Updating checksum, not recalculating, so if someone put bad checksum on purpose, it will stay bad
    m_ipv4->updateCheckSum(PKT_NTOHS(m_ipv4->getId()), PKT_NTOHS(new_id));
    m_ipv4->setId(new_id);

    return 0;
}

int CFlowStatParser::get_l4_proto(uint8_t &proto) {
    if (! m_ipv4)
        return -1;

    proto = m_ipv4->getProtocol();

    return 0;
}

// calculate the payload len. Do not want to do this in parse(), since this is required only in
// specific cases, while parse is used in many places (including on packet RX path, where we want to bo as fast as possible)
int CFlowStatParser::get_payload_len(uint8_t *p, uint16_t len, uint16_t &payload_len) {
    uint16_t l2_header_len;
    uint16_t l3_header_len;
    uint16_t l4_header_len;
    uint8_t *p_l4 = NULL;
    TCPHeader *p_tcp = NULL;
    if (!m_ipv4) {
        payload_len = 0;
        return -1;
    }

    l2_header_len = ((uint8_t *)m_ipv4) - p;
    l3_header_len = m_ipv4->getHeaderLength();
    switch (m_ipv4->getProtocol()) {
    case IPPROTO_UDP:
        l4_header_len = 8;
        break;
    case IPPROTO_TCP:
        p_l4 = ((uint8_t *)m_ipv4) + l3_header_len;
        if ((p_l4 + TCP_HEADER_LEN) > (p + len)) {
            //Not enough space for TCP header
            payload_len = 0;
            return -1;
        }
        p_tcp = (TCPHeader *)p_l4;
        l4_header_len = p_tcp->getHeaderLength();
        break;
    case IPPROTO_ICMP:
    l4_header_len = 8;
    break;
    default:
        l4_header_len = 0;
    }

    if (len < l2_header_len + l3_header_len + l4_header_len) {
        payload_len = 0;
        return -1;
    }

    payload_len = len - l2_header_len - l3_header_len - l4_header_len;

    return 0;
}

static const uint16_t TEST_IP_ID = 0xabcd;
static const uint8_t TEST_L4_PROTO = IPPROTO_UDP;

int CFlowStatParser::test() {
    uint16_t ip_id = 0;
    uint8_t l4_proto;
    uint8_t test_pkt[] = {
        // ether header
        0x74, 0xa2, 0xe6, 0xd5, 0x39, 0x25,
        0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x02,
        0x81, 0x00,
        0x0a, 0xbc, 0x08, 0x00, // vlan
        // IP header
        0x45,0x02,0x00,0x30,
        0x01,0x02,0x40,0x00,
        0xff, TEST_L4_PROTO, 0xbd,0x04,
        0x10,0x0,0x0,0x1,
        0x30,0x0,0x0,0x1,
        // TCP heaader
        0xab, 0xcd, 0x00, 0x80, // src, dst ports
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // seq num, ack num
        0x50, 0x00, 0xff, 0xff, // Header size, flags, window size
        0x00, 0x00, 0x00, 0x00, // checksum ,urgent pointer
        // some extra bytes
        0x1, 0x2, 0x3, 0x4
    };

    // good packet
    assert (parse(test_pkt, sizeof(test_pkt)) == 0);
    m_ipv4->updateCheckSum();
    assert(m_ipv4->isChecksumOK() == true);
    set_ip_id(TEST_IP_ID);
    // utl_DumpBuffer(stdout, test_pkt, sizeof(test_pkt), 0);
    get_ip_id(ip_id);
    assert(ip_id == TEST_IP_ID);
    assert(m_ipv4->isChecksumOK() == true);
    assert(get_l4_proto(l4_proto) == 0);
    assert(l4_proto == TEST_L4_PROTO);
    assert(m_stat_supported == true);

    // payload len test
    uint16_t payload_len;
    int ret;
    ret = get_payload_len(test_pkt, sizeof(test_pkt), payload_len);
    // UDP packet.
    assert(ret == 0);
    assert(payload_len == 16);
    reset();
    // ICMP packet
    test_pkt[27] = IPPROTO_ICMP;
    assert (parse(test_pkt, sizeof(test_pkt)) == 0);
    ret = get_payload_len(test_pkt, sizeof(test_pkt), payload_len);
    assert(ret == 0);
    assert(payload_len == 16);
    // TCP packet
    test_pkt[27] = IPPROTO_TCP;
    assert (parse(test_pkt, sizeof(test_pkt)) == 0);
    ret = get_payload_len(test_pkt, sizeof(test_pkt), payload_len);
    assert(ret == 0);
    assert(payload_len == 4);
    // Other protocol
    test_pkt[27] = 0xaa;
    assert (parse(test_pkt, sizeof(test_pkt)) == 0);
    ret = get_payload_len(test_pkt, sizeof(test_pkt), payload_len);
    assert(ret == 0);
    assert(payload_len == 24);

    reset();

    // bad packet. change eth protocol
    test_pkt[16] = 0xaa;
    assert (parse(test_pkt, sizeof(test_pkt)) == -1);
    assert(m_stat_supported == false);

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
        m_ipv4 = (IPHeader *)(p + ETH_HDR_LEN);
        m_stat_supported = true;
        break;
    default:
        m_stat_supported = false;
        return -1;
        break;
    }

    return 0;
}
