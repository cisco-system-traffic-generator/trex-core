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
#ifndef _ARP_H_
#define _ARP_H_
#include "MacAddress.h"

#pragma pack(push, 1)
class ArpHdr {
 public:
    enum arp_hdr_enum_e {
        ARP_HDR_HRD_ETHER = 1,
        ARP_HDR_OP_REQUEST = 1, /* request to resolve address */
        ARP_HDR_OP_REPLY = 2, /* response to previous request */
        ARP_HDR_OP_REVREQUEST = 3, /* request proto addr given hardware */
        ARP_HDR_OP_REVREPLY = 4, /* response giving protocol address */
        ARP_HDR_OP_INVREQUEST = 5, /* request to identify peer */
        ARP_HDR_OP_INVREPLY = 6, /* response identifying peer */
    };

 public:
	uint16_t m_arp_hrd;    /* format of hardware address */
	uint16_t m_arp_pro;    /* format of protocol address */
	uint8_t  m_arp_hln;    /* length of hardware address */
	uint8_t  m_arp_pln;    /* length of protocol address */
	uint16_t m_arp_op;     /* ARP opcode (command) */
	MacAddress m_arp_sha;  /**< sender hardware address */
	uint32_t m_arp_sip;  /**< sender IP address */
	MacAddress m_arp_tha;  /**< target hardware address */
	uint32_t m_arp_tip;  /**< target IP address */
};
#pragma pack(pop)

#endif
