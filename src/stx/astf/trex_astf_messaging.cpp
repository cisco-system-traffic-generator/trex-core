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
TrexAstfDpStart::TrexAstfDpStart(profile_id_t profile_id, double duration, bool nc) {
    m_profile_id = profile_id;
    m_duration = duration;
    m_nc_flow_close = nc;
}


bool TrexAstfDpStart::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->start_transmit(m_profile_id, m_duration, m_nc_flow_close);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStart::clone() {
    return new TrexAstfDpStart(m_profile_id, m_duration, m_nc_flow_close);
}

/*************************
  stop traffic message
 ************************/
TrexAstfDpStop::TrexAstfDpStop(profile_id_t profile_id, uint32_t stop_id) {
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
  control DP scheduler
 ************************/
TrexAstfDpScheduler::TrexAstfDpScheduler(bool activate) {
    m_activate = activate;
}

bool TrexAstfDpScheduler::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->scheduler(m_activate);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpScheduler::clone() {
    return new TrexAstfDpScheduler(m_activate);
}

/*************************
  update traffic message
 ************************/
TrexAstfDpUpdate::TrexAstfDpUpdate(profile_id_t profile_id, double old_new_ratio) {
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
TrexAstfDpCreateTcp::TrexAstfDpCreateTcp(profile_id_t profile_id, double factor, CAstfDB* astf_db) {
    m_profile_id = profile_id;
    m_factor = factor;
    m_astf_db = astf_db;
}

bool TrexAstfDpCreateTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->create_tcp_batch(m_profile_id, m_factor, m_astf_db);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpCreateTcp::clone() {
    return new TrexAstfDpCreateTcp(m_profile_id, m_factor, m_astf_db);
}

/*************************
  delete tcp batch
 ************************/
TrexAstfDpDeleteTcp::TrexAstfDpDeleteTcp(profile_id_t profile_id, bool do_remove, CAstfDB* astf_db) {
    m_profile_id = profile_id;
    m_do_remove = do_remove;
    m_astf_db = astf_db;
}

bool TrexAstfDpDeleteTcp::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->delete_tcp_batch(m_profile_id, m_do_remove, m_astf_db);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpDeleteTcp::clone() {
    return new TrexAstfDpDeleteTcp(m_profile_id, m_do_remove, m_astf_db);
}


/*************************
  parse ASTF JSON from string
 ************************/
TrexAstfLoadDB::TrexAstfLoadDB(profile_id_t profile_id, string *profile_buffer, string *topo_buffer, CAstfDB* astf_db) {
    m_profile_id     = profile_id;
    m_profile_buffer = profile_buffer;
    m_topo_buffer    = topo_buffer;
    m_astf_db        = astf_db;
}

bool TrexAstfLoadDB::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->parse_astf_json(m_profile_id, m_profile_buffer, m_topo_buffer, m_astf_db);
    return true;
}

TrexCpToDpMsgBase* TrexAstfLoadDB::clone() {
    assert(0); // should not be cloned [and sent to several cores]
    return nullptr;
}

/*************************
  remove ASTF JSON and DB
 ************************/
TrexAstfDeleteDB::TrexAstfDeleteDB(profile_id_t profile_id, CAstfDB* astf_db) {
    m_profile_id     = profile_id;
    m_astf_db        = astf_db;
}

bool TrexAstfDeleteDB::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->remove_astf_json(m_profile_id, m_astf_db);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDeleteDB::clone() {
    assert(0); // should not be cloned [and sent to several cores]
    return nullptr;
}

/****************
 set service mode
****************/
TrexCpToDpMsgBase *
TrexAstfDpServiceMode::clone() {

    TrexCpToDpMsgBase *new_msg = new TrexAstfDpServiceMode(m_enabled, m_filtered, m_mask);

    return new_msg;
}

bool
TrexAstfDpServiceMode::handle(TrexDpCore *dp_core) {
    
    TrexAstfDpCore *astf_core = dynamic_cast<TrexAstfDpCore *>(dp_core);

    astf_core->set_service_mode(m_enabled, m_filtered, m_mask);
    return true;
}

/*************************
+  Activate/Deactivate  Client MSG
+ ************************/
TrexAstfDpActivateClient::TrexAstfDpActivateClient(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool activate) {
    m_astf_db = astf_db;
    m_msg_data =  msg_data;
    m_activate = activate;
}

bool TrexAstfDpActivateClient::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->activate_client(m_astf_db, m_msg_data, m_activate);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpActivateClient::clone() {
    return new TrexAstfDpActivateClient(m_astf_db, m_msg_data, m_activate);
}
