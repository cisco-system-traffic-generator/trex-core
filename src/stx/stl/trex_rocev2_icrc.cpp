/*
 Itay Marom
 Hanoch Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

#include "trex_rocev2_icrc.h"

uint32_t trex_roce_v2_calc_icrc_bytes(const uint8_t *pkt, uint32_t pkt_len,
                                    uint16_t l2_len, uint16_t l3_len, uint16_t bth_offset) {
    if (!pkt || pkt_len < 4 || l2_len >= pkt_len) { return 0; }
    bool ipv6 = (pkt[l2_len] >> 4) == 6;
    uint32_t crc = 0xffffffff;
    for (unsigned i = 0; i < 8; ++i) { crc = roce_crc32_update_byte(crc, 0xff); }
    for (uint32_t i = l2_len; i < pkt_len - 4; ++i) {
        crc = roce_crc32_update_byte(crc, roce_icrc_invariant_byte(i, pkt[i], ipv6,
                                                                l2_len, l2_len + l3_len, bth_offset));
    }
    return crc ^ 0xffffffff;
}
