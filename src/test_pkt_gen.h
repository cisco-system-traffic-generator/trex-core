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

#ifndef __TEST_PKT_GEN_H__
#define __TEST_PKT_GEN_H__

enum {
    D_PKT_TYPE_ICMP = 1,
    D_PKT_TYPE_UDP = 2,
    D_PKT_TYPE_TCP = 3,
    D_PKT_TYPE_9k_UDP = 4,
    D_PKT_TYPE_IPV6 = 60,
    D_PKT_TYPE_HW_VERIFY = 100,
    D_PKT_TYPE_HW_VERIFY_RCV_ALL = 101,
    D_PKT_TYPE_HW_TOGGLE_TEST = 102,
};

enum {
    DPF_VLAN = 0x1,
    DPF_QINQ = 0X2,
    DPF_RXCHECK = 0x4
};

class CTestPktGen {
 public:
    char *create_test_pkt(uint16_t l3_type, uint16_t l4_proto, uint8_t ttl, uint32_t ip_id, uint16_t flags
                          , uint16_t max_payload, int &pkt_size);
};

#endif
