/*
  Elad Aharon
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2017 Cisco Systems, Inc.

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

#ifndef __TREX_VLAN_FILTER_H__
#define __TREX_VLAN_FILTER_H__

#include "common/Network/Packet/EthernetHeader.h"
#include "common/Network/Packet/IPHeader.h"
#include "common/Network/Packet/IPv6Header.h"
#include "string"

typedef struct VlanFilterCFG {
    uint32_t m_vlans[2] = {0};
    uint8_t m_wanted_flags;
}VlanFilterCFG;

class VlanFilter {
public:
    enum FilterFlags {
        None = 0,
        NO_TCP_UDP = 1,  // This bit is mutually exclusive with all other bits
        TCP_UDP = 1 << 1,
        ALL = 3,

        // reserved bits for more flags DHCP e.t.c..
    };

    bool is_pkt_pass(const uint8_t *pkt);

    void parse_l4(uint8_t protocol);

    VlanFilterCFG get_cfg();
    void set_cfg(VlanFilterCFG &cfg);
    void set_cfg_flag(uint8_t flag);
    void mask_to_str(std::string &str);
private:
    VlanFilterCFG m_cfg;
    uint8_t m_total_vlans;
    uint8_t m_pkt_flags;
};

#endif
