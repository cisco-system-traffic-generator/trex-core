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

#include <assert.h>
#include <netinet/in.h>
#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/Network/Packet/IcmpHeader.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>
#include <common/Network/Packet/Arp.h>
#include "rx_check_header.h"
#include "pkt_gen.h"
#include "bp_sim.h"
// For use in tests
char *CTestPktGen::create_test_pkt(uint16_t l3_type, uint16_t l4_proto, uint8_t ttl, uint32_t ip_id, uint16_t flags
                                   , uint16_t max_payload, int &pkt_size) {
    // ASA 2
    uint8_t dst_mac[6] = {0x74, 0xa2, 0xe6, 0xd5, 0x39, 0x25};
    uint8_t src_mac[6] = {0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x02};
    uint8_t vlan_header[4] = {0x0a, 0xbc, 0x00, 0x00}; // we set the type below according to if pkt is ipv4 or 6
    uint8_t vlan_header2[4] = {0x0a, 0xbc, 0x88, 0xa8};
    uint16_t l2_proto;
    uint16_t payload_len;

    // ASA 1
    //        uint8_t dst_mac[6] = {0xd4, 0x8c, 0xb5, 0xc9, 0x54, 0x2b};
    //      uint8_t src_mac[6] = {0xa0, 0x36, 0x9f, 0x38, 0xa4, 0x0};
    if (flags & DPF_VLAN) {
        l2_proto = 0x0081;
    } else {
        l2_proto = htons(l3_type);
    }

    uint8_t ip_header[] = {
        0x45,0x03,0x00,0x30,
        0x00,0x00,0x40,0x00,
        0xff,0x01,0xbd,0x04,
        0x10,0x0,0x0,0x1, //SIP
        0x30,0x0,0x0,0x1, //DIP
        //                      0x82, 0x0b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // IP option. change 45 to 48 (header len) if using it.
    };
    uint8_t ipv6_header[] = {
        0x60,0x10,0xff,0x30, // traffic class + flow label
        0x00,0x00,0x40,0x00, // payload len + next header + hop limit
        0x10,0x0,0x0,0x1,0x10,0x0,0x0,0x1,0x10,0x0,0x0,0x1,0x10,0x0,0x0,0x1, //SIP
        0x30,0x0,0x0,0x1,0x10,0x0,0x0,0x1,0x30,0x0,0x0,0x1,0x10,0x0,0x0,0x1, //DIP
    };
    uint8_t udp_header[] =  {0x11, 0x11, 0x11,0x11, 0x00, 0x6d, 0x00, 0x00};
    uint8_t udp_data[] = {0x64,0x31,0x3a,0x61,
                          0x64,0x32,0x3a,0x69,0x64,
                          0x32,0x30,0x3a,0xd0,0x0e,
                          0xa1,0x4b,0x7b,0xbd,0xbd,
                          0x16,0xc6,0xdb,0xc4,0xbb,0x43,
                          0xf9,0x4b,0x51,0x68,0x33,0x72,
                          0x20,0x39,0x3a,0x69,0x6e,0x66,0x6f,
                          0x5f,0x68,0x61,0x73,0x68,0x32,0x30,0x3a,0xee,0xc6,0xa3,
                          0xd3,0x13,0xa8,0x43,0x06,0x03,0xd8,0x9e,0x3f,0x67,0x6f,
                          0xe7,0x0a,0xfd,0x18,0x13,0x8d,0x65,0x31,0x3a,0x71,0x39,
                          0x3a,0x67,0x65,0x74,0x5f,0x70,0x65,0x65,0x72,0x73,0x31,
                          0x3a,0x74,0x38,0x3a,0x3d,0xeb,0x0c,0xbf,0x0d,0x6a,0x0d,
                          0xa5,0x31,0x3a,0x79,0x31,0x3a,0x71,0x65,0x87,0xa6,0x7d,
                          0xe7
    };

    uint8_t tcp_header[] = {0xab, 0xcd, 0x00, 0x80, // src, dst ports
                            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // seq num, ack num
                            0x50, 0x00, 0xff, 0xff, // Header size, flags, window size
                            0x00, 0x00, 0x00, 0x00, // checksum ,urgent pointer
    };

    uint8_t tcp_data[] = {0x8, 0xa, 0x1, 0x2, 0x3, 0x4, 0x3, 0x4, 0x6, 0x5,
                          0x8, 0xa, 0x1, 0x2, 0x3, 0x4, 0x3, 0x4, 0x6, 0x5};

    uint8_t icmp_header[] = {
        0x08, 0x00,
        0xb8, 0x21,  //checksum
        0xaa, 0xbb,  // id
        0x00, 0x01,  // Sequence number
    };
    uint8_t icmp_data[] = {
        0xd6, 0x6e, 0x64, 0x34, // magic
        0x6a, 0xad, 0x0f, 0x00, //64 bit counter
        0x00, 0x56, 0x34, 0x12,
        0x78, 0x56, 0x34, 0x12, 0x00, 0x00 // seq
    };

    pkt_size = 14;

    if (flags & DPF_VLAN) {
        pkt_size += 4;
        if (flags & DPF_QINQ) {
            pkt_size += 4;
        }
    }

    switch(l3_type) {
    case EthernetHeader::Protocol::IP:
        pkt_size += sizeof(ip_header);
        break;
    case EthernetHeader::Protocol::IPv6:
        pkt_size += sizeof(ipv6_header);
        if (flags & DPF_RXCHECK) {
            pkt_size += sizeof(struct CRx_check_header);
        }
        break;
    }

    switch (l4_proto) {
    case IPPROTO_ICMP:
        pkt_size += sizeof(icmp_header);
        payload_len = (max_payload < sizeof(icmp_data)) ? max_payload:sizeof(icmp_data);
        pkt_size += payload_len;
        break;
    case IPPROTO_UDP:
        pkt_size += sizeof(udp_header);
        payload_len = (max_payload < sizeof(udp_data)) ? max_payload:sizeof(udp_data);
        pkt_size += payload_len;
        break;
    case IPPROTO_TCP:
        pkt_size += sizeof(tcp_header);
        payload_len = (max_payload < sizeof(tcp_data)) ? max_payload:sizeof(tcp_data);
        pkt_size += payload_len;
        break;
    default:
        payload_len = (max_payload < sizeof(udp_data)) ? max_payload:sizeof(udp_data);
        pkt_size += payload_len;
        break;
    }

    char *p_start = (char *)malloc(pkt_size);
    assert(p_start);
    char *p = p_start;

    /* set pkt data */
    memcpy(p, dst_mac, sizeof(dst_mac)); p += sizeof(dst_mac);
    memcpy(p, src_mac, sizeof(src_mac)); p += sizeof(src_mac);
    memcpy(p, &l2_proto, sizeof(l2_proto)); p += sizeof(l2_proto);

    if (flags & DPF_VLAN) {
        if (flags & DPF_QINQ) {
            memcpy(p, &vlan_header2, sizeof(vlan_header2)); p += sizeof(vlan_header2);
        }

        uint16_t l3_type_htons = htons(l3_type);
        memcpy(&vlan_header[2], &l3_type_htons, sizeof(l3_type));
        memcpy(p, &vlan_header, sizeof(vlan_header)); p += sizeof(vlan_header);
    }

    struct IPHeader *ip = (IPHeader *)p;
    struct IPv6Header *ipv6 = (IPv6Header *)p;
    switch(l3_type) {
    case EthernetHeader::Protocol::IP:
        memcpy(p, ip_header, sizeof(ip_header)); p += sizeof(ip_header);
        ip->setProtocol(l4_proto);
        ip->setTotalLength(pkt_size - 14);
        ip->setId(ip_id);
        break;
    case EthernetHeader::Protocol::IPv6:
        memcpy(p, ipv6_header, sizeof(ipv6_header)); p += sizeof(ipv6_header);
        if (flags & DPF_RXCHECK) {
            // rx check header
            ipv6->setNextHdr(RX_CHECK_V6_OPT_TYPE);
            if (flags & DPF_RXCHECK) {
                struct CRx_check_header *rxch = (struct CRx_check_header *)p;
                p += sizeof(CRx_check_header);
                rxch->m_option_type = l4_proto;
                rxch->m_option_len = RX_CHECK_V6_OPT_LEN;
            }
        } else {
            ipv6->setNextHdr(l4_proto);
        }
        ipv6->setPayloadLen(pkt_size - 14 - sizeof(ipv6_header));
        ipv6->setFlowLabel(ip_id);
        break;
    }


    struct TCPHeader *tcp = (TCPHeader *)p;
    struct ICMPHeader *icmp= (ICMPHeader *)p;
    switch (l4_proto) {
    case IPPROTO_ICMP:
        memcpy(p, icmp_header, sizeof(icmp_header)); p += sizeof(icmp_header);
        memcpy(p, icmp_data, payload_len); p += payload_len;
        icmp->updateCheckSum(sizeof(icmp_header) + sizeof(icmp_data));
        break;
    case IPPROTO_UDP:
        memcpy(p, udp_header, sizeof(udp_header)); p += sizeof(udp_header);
        memcpy(p, udp_data, payload_len); p += payload_len;
        break;
    case IPPROTO_TCP:
        memcpy(p, tcp_header, sizeof(tcp_header)); p += sizeof(tcp_header);
        memcpy(p, tcp_data, payload_len); p += payload_len;
        tcp->setSynFlag(false);
        // printf("Sending TCP header:");
        //tcp->dump(stdout);
        break;
    default:
        memcpy(p, udp_data, payload_len); p += payload_len;
        break;
    }

    switch(l3_type) {
    case EthernetHeader::Protocol::IP:
        ip->setTimeToLive(ttl);
        if (flags & DPF_TOS_1) {
            ip->setTOS(TOS_TTL_RESERVE_DUPLICATE);
        }else{
            ip->setTOS(0x2);
        }

        ip->updateCheckSum();
        break;
    case EthernetHeader::Protocol::IPv6:
        ipv6->setHopLimit(ttl);
        if (flags & DPF_TOS_1) {
            ipv6->setTrafficClass(TOS_TTL_RESERVE_DUPLICATE);
        }else{
            ipv6->setTrafficClass(0x2);
        }

        break;
    }

    return p_start;
}

/*
 * Create ARP request packet
 * Parameters:
 *  pkt - Buffer to fill the packet in. Size should be big enough to contain the packet (60 is a good value).
 *  sip - Our source IP
 *  tip - Target IP for which we need resolution (In case of gratuitous ARP, should be equal sip).
 *  src_mac - Our source MAC
 *  vlan - VLAN tag to send the packet on. If set to 0, no vlan will be sent.
 *  port - Port we intended to send packet on. This is needed since we put some "magic" number with the port, so
 *         we can identify if we are connected in loopback, which ports are connected.
 */
void CTestPktGen::create_arp_req(uint8_t *pkt, uint32_t sip, uint32_t tip, uint8_t *src_mac, uint16_t vlan
                                 , uint16_t port) {
    uint16_t l2_proto = htons(EthernetHeader::Protocol::ARP);

    // dst MAC
    memset(pkt, 0xff, ETHER_ADDR_LEN);
    pkt += ETHER_ADDR_LEN;
    // src MAC
    memcpy(pkt, src_mac, ETHER_ADDR_LEN);
    pkt += ETHER_ADDR_LEN;

    if (vlan != 0) {
        uint16_t htons_vlan = htons(vlan);
        uint16_t vlan_proto = htons(0x8100);
        memcpy(pkt, &vlan_proto, sizeof(vlan_proto));
        pkt += 2;
        memcpy(pkt, &htons_vlan, sizeof(uint16_t));
        pkt += 2;
    }

    // l3 type
    memcpy(pkt, &l2_proto, sizeof(l2_proto));
    pkt += 2;

    ArpHdr *arp = (ArpHdr *)pkt;
    arp->m_arp_hrd = htons(ArpHdr::ARP_HDR_HRD_ETHER); // Format of hardware address
    arp->m_arp_pro = htons(EthernetHeader::Protocol::IP); // Format of protocol address
    arp->m_arp_hln = ETHER_ADDR_LEN; // Length of hardware address
    arp->m_arp_pln = 4; // Length of protocol address
    arp->m_arp_op = htons(ArpHdr::ARP_HDR_OP_REQUEST); // ARP opcode (command)

    memcpy(&arp->m_arp_sha.data, src_mac, ETHER_ADDR_LEN); // Sender MAC address
    arp->m_arp_sip = htonl(sip); // Sender IP address

    uint8_t magic[5] = {0x1, 0x3, 0x5, 0x7, 0x9};
    memcpy(&arp->m_arp_tha.data, magic, 5); // Target MAC address
    arp->m_arp_tha.data[5] = port;
    arp->m_arp_tip = htonl(tip);
}
