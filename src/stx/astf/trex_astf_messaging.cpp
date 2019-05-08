/*
 Itay Marom
 Hanoch Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include "trex_astf_dp_core.h"
#include "trex_astf_messaging.h"
#include "trex_rx_core.h"

using namespace std;

TrexAstfDpCore* astf_core(TrexDpCore *dp_core) {
    return (TrexAstfDpCore *)dp_core;
}


/*************************
  start traffic message
 ************************/
TrexAstfDpStart::TrexAstfDpStart(uint32_t profile_id, double duration) {
    m_profile_id = profile_id;
    m_duration = duration;
}


bool TrexAstfDpStart::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->start_transmit(m_profile_id, m_duration);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStart::clone() {
    return new TrexAstfDpStart(m_profile_id, m_duration);
}

/*************************
  stop traffic message
 ************************/
TrexAstfDpStop::TrexAstfDpStop(uint32_t profile_id, uint32_t stop_id) {
    m_profile_id = profile_id;
    m_core = NULL;
    m_stop_id = stop_id;
}

bool TrexAstfDpStop::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->stop_transmit(m_profile_id, m_stop_id);
    return true;
}

void TrexAstfDpStop::on_node_remove() {
    if (m_core) {
        assert(m_core->m_non_active_nodes>0);
        m_core->m_non_active_nodes--;
    }
}

TrexCpToDpMsgBase* TrexAstfDpStop::clone() {
    return new TrexAstfDpStop(m_profile_id, m_stop_id);
}

/*************************
  update traffic message
 ************************/
TrexAstfDpUpdate::TrexAstfDpUpdate(uint32_t profile_id, double old_new_ratio) {
    m_profile_id    = profile_id;
    m_old_new_ratio = old_new_ratio;
}

bool TrexAstfDpUpdate::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->update_rate(m_profile_id, m_old_new_ratio);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpUpdate::clone() {
    return new TrexAstfDpUpdate(m_profile_id, m_old_new_ratio);
}

/*************************
  create tcp batch
 ************************/
TrexAstfDpCreateTcp::TrexAstfDpCreateTcp(uint32_t profile_id) {
    m_profile_id = profile_id;
}

bool TrexAstfDpCreateTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->create_tcp_batch(m_profile_id);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpCreateTcp::clone() {
    return new TrexAstfDpCreateTcp(m_profile_id);
}

/*************************
  delete tcp batch
 ************************/
TrexAstfDpDeleteTcp::TrexAstfDpDeleteTcp(uint32_t profile_id) {
    m_profile_id = profile_id;
}

bool TrexAstfDpDeleteTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->delete_tcp_batch(m_profile_id);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpDeleteTcp::clone() {
    return new TrexAstfDpDeleteTcp(m_profile_id);
}


/*************************
  parse ASTF JSON from string
 ************************/
TrexAstfLoadDB::TrexAstfLoadDB(uint32_t profile_id, string *profile_buffer, string *topo_buffer) {
    m_profile_id     = profile_id;
    m_profile_buffer = profile_buffer;
    m_topo_buffer    = topo_buffer;
}

bool TrexAstfLoadDB::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->parse_astf_json(m_profile_id, m_profile_buffer, m_topo_buffer);
    return true;
}

TrexCpToDpMsgBase* TrexAstfLoadDB::clone() {
    assert(0); // should not be cloned [and sent to several cores]
    return nullptr;
}

