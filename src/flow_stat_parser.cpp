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

#include <common/basic_utils.h>
#include <common/Network/Packet/IPHeader.h>
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
    int min_len = 14 + IPV4_HDR_LEN;

    reset();

    switch( ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        if (len < min_len)
            return -1;
        m_ipv4 = (IPHeader *)(p + 14);
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
    m_ipv4->updateCheckSum(m_ipv4->getId(), PKT_NTOHS(new_id));
    m_ipv4->setId(new_id);

    return 0;
}

int CFlowStatParser::get_l4_proto(uint8_t &proto) {
    if (! m_ipv4)
        return -1;

    proto = m_ipv4->getProtocol();

    return 0;
}

static const uint16_t TEST_IP_ID = 0xabcd;
static const uint8_t TEST_L4_PROTO = 0x11;

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
        0x00,0x00,0x40,0x00,
        0xff, TEST_L4_PROTO, 0xbd,0x04,
        0x10,0x0,0x0,0x1,
        0x30,0x0,0x0,0x1,
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

    reset();

    // bad packet
    test_pkt[16] = 0xaa;
    assert (parse(test_pkt, sizeof(test_pkt)) == -1);
    assert(m_stat_supported == false);

    return 0;
}

// In 82599 10G card we do not support VLANs
int C82599Parser::parse(uint8_t *p, uint16_t len) {
    EthernetHeader *ether = (EthernetHeader *)p;
    int min_len = 14 + IPV4_HDR_LEN;

    reset();

    switch( ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        if (len < min_len)
            return -1;
        m_ipv4 = (IPHeader *)(p + 14);
        m_stat_supported = true;
        break;
    default:
        m_stat_supported = false;
        return -1;
        break;
    }

    return 0;
}
