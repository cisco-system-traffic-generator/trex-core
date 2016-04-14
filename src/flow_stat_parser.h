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

#ifndef __FLOW_STAT_PARSER_H__
#define __FLOW_STAT_PARSER_H__

// Basic flow stat parser. Relevant for xl710/x710/x350 cards
#include "common/Network/Packet/IPHeader.h"

class CFlowStatParser {
 public:
    virtual ~CFlowStatParser() {}
    virtual void reset();
    virtual int parse(uint8_t *pkt, uint16_t len);
    virtual bool is_stat_supported() {return m_stat_supported == true;}
    virtual int get_ip_id(uint16_t &ip_id);
    virtual int set_ip_id(uint16_t ip_id);
    virtual int get_l4_proto(uint8_t &proto);
    virtual int get_payload_len(uint8_t *p, uint16_t len, uint16_t &payload_len);
    virtual int test();

 protected:
    IPHeader *m_ipv4;
    bool m_stat_supported;
    uint8_t m_l4_proto;
};

class C82599Parser : public CFlowStatParser {
 public:
    C82599Parser(bool vlan_supported) {m_vlan_supported = vlan_supported;}
    ~C82599Parser() {}
    int parse(uint8_t *pkt, uint16_t len);

 private:
    bool m_vlan_supported;
};

#endif
