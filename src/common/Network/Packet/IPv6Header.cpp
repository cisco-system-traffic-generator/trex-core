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

#include "IPv6Header.h"

/*
 * Return l4 type of Ipv6 packet
 * pkt - pointer to start of packet data (including header)
 * pkt_len - length of packet (including header)
 * p_l4 - return pointer to start of l4 header
 */
uint8_t IPv6Header::getl4Proto(uint8_t *pkt, uint16_t pkt_len, uint8_t *&p_l4) {
    bool stop = false;
    uint8_t *next_header = pkt + IPV6_HDR_LEN;;
    uint8_t next_header_type = myNextHdr;
    uint16_t len_left = pkt_len - IPV6_HDR_LEN;
    uint16_t curr_header_len;

    do {
        switch(next_header_type) {
        case IPPROTO_HOPOPTS:
        case IPPROTO_ROUTING:
        case IPPROTO_ENCAP_SEC:
        case IPPROTO_AUTH:
        case IPPROTO_FRAGMENT:
        case IPPROTO_DSTOPTS:
        case IPPROTO_MH:
            next_header_type = next_header[0];
            curr_header_len = (next_header[1] + 1) * 8;
            next_header += curr_header_len;
            len_left -= curr_header_len;
            break;
        default:
            stop = true;
            break;
        }
    } while ((! stop) && len_left >= 2);

    p_l4 = next_header;
    return next_header_type;
}
