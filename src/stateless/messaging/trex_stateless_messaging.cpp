/*
 Itay Marom
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
#include <trex_stateless_messaging.h>
#include <trex_stateless_dp_core.h>
#include <string.h>

/*************************
  start traffic message
 ************************/ 
TrexStatelessDpStart::TrexStatelessDpStart(const uint8_t *pkt, uint16_t pkt_len, double pps) {
    assert(pkt);
    assert(pkt_len > 0);

    m_pkt = new uint8_t[pkt_len];
    memcpy(m_pkt, pkt, pkt_len);
    m_pkt_len = pkt_len;

    m_pps = pps;
}

TrexStatelessDpStart::~TrexStatelessDpStart() {
    if (m_pkt) {
        delete m_pkt;
    }
}

bool
TrexStatelessDpStart::handle(TrexStatelessDpCore *dp_core) {

    dp_core->start_const_traffic(m_pkt, m_pkt_len, m_pps);
    return true;
}

/*************************
  stop traffic message
 ************************/ 
bool
TrexStatelessDpStop::handle(TrexStatelessDpCore *dp_core) {
    dp_core->stop_traffic();
    return true;
}

