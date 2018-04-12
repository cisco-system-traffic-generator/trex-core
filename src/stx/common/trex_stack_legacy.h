/*
  TRex team
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

#ifndef __TREX_STACK_LEGACY_H__
#define __TREX_STACK_LEGACY_H__

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <set>
#include "trex_stack_base.h"
#include "trex_rx_packet_parser.h"

using namespace std;

class CLegacyNode : public CNodeBase {
public:
    CLegacyNode(const string &mac_buf);
    ~CLegacyNode();
    void conf_vlan_internal(const vlan_list_t &vlans);
    void conf_ip4_internal(const string &ip4_buf, const string &gw4_buf);

private:
    void set_mac(const string &addr);
    dsec_t                  m_grat_arp_sent_time_sec;
};


class CStackLegacy : public CStackBase {
public:
    CStackLegacy(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
    ~CStackLegacy(void);

    uint16_t get_capa(void);

    void grat_to_json(Json::Value &res);

    // TRex port -> node
    void handle_pkt(const rte_mbuf_t *m);

    // node -> TRex port
    uint16_t handle_tx(uint16_t limit);

private:
    bool is_grat_active(void);
    CNodeBase* add_node_internal(const std::string &mac_buf);
    void del_node_internal(const std::string &mac_buf);
    uint16_t send_grat_arp(void);
    void handle_icmp(RXPktParser &parser);
    void handle_arp(RXPktParser &parser);
    rte_mbuf_t *duplicate_mbuf(const rte_mbuf_t *m);

    dsec_t          m_next_grat_arp_sec;
};


#endif /* __TREX_STACK_LEGACY_H__ */
