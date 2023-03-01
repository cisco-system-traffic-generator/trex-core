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
#include "trex_astf.h"
#include "trex_astf_messaging.h"
#include "trex_rx_core.h"

using namespace std;

TrexAstfDpCore* astf_core(TrexDpCore *dp_core) {
    return (TrexAstfDpCore *)dp_core;
}


/*************************
  start traffic message
 ************************/
TrexAstfDpStart::TrexAstfDpStart(profile_id_t profile_id, double duration, bool nc, double establish_timeout, double terminate_duration, double dump_interval) {
    m_profile_id = profile_id;
    m_duration = duration;
    m_nc_flow_close = nc;
    m_establish_timeout = establish_timeout;
    m_terminate_duration = terminate_duration;
    m_dump_interval = dump_interval;
}


bool TrexAstfDpStart::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->start_transmit(m_profile_id, m_duration, m_nc_flow_close, m_establish_timeout, m_terminate_duration, m_dump_interval);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpStart::clone() {
    return new TrexAstfDpStart(m_profile_id, m_duration, m_nc_flow_close, m_establish_timeout, m_terminate_duration, m_dump_interval);
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
    if (m_core) {   // from add_profile_duration()
        astf_core(dp_core)->stop_transmit(m_profile_id, m_stop_id, false);
    }
    else {
        bool set_nc = (m_stop_id != 0); // override nc when CP requests
        astf_core(dp_core)->stop_transmit(m_profile_id, m_stop_id, set_nc);
    }
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
TrexAstfLoadDB::TrexAstfLoadDB(profile_id_t profile_id, string *profile_buffer, string *topo_buffer, CAstfDB* astf_db, const string* tunnel_topo_buffer) {
    m_profile_id         = profile_id;
    m_profile_buffer     = profile_buffer;
    m_topo_buffer        = topo_buffer;
    m_astf_db            = astf_db;
    m_tunnel_topo_buffer = tunnel_topo_buffer;
}

bool TrexAstfLoadDB::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->parse_astf_json(m_profile_id, m_profile_buffer, m_topo_buffer, m_astf_db, m_tunnel_topo_buffer);
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
TrexAstfDpActivateClient::TrexAstfDpActivateClient(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool activate, bool is_range) {
    m_astf_db = astf_db;
    m_msg_data =  msg_data;
    m_activate = activate;
    m_is_range = is_range;
}

bool TrexAstfDpActivateClient::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->activate_client(m_astf_db, m_msg_data, m_activate, m_is_range);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpActivateClient::clone() {
    return new TrexAstfDpActivateClient(m_astf_db, m_msg_data, m_activate, m_is_range);
}

/*************************
+  Get Client Stats MSG
+ ************************/

bool TrexAstfDpGetClientStats::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->get_client_stats(m_msg_data, m_is_range);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpGetClientStats::clone() {
    return new TrexAstfDpGetClientStats(m_msg_data, m_is_range);
}

/*************************
+  Set ignored mac addresses
+ ************************/

bool TrexAstfDpIgnoredMacAddrs::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->insert_ignored_mac_addresses(m_mac_addresses);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpIgnoredMacAddrs::clone() {
    return new TrexAstfDpIgnoredMacAddrs(m_mac_addresses);
}

/*************************
+  Set ignored IPv4 addresses
+ ************************/

bool TrexAstfDpIgnoredIpAddrs::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->insert_ignored_ip_addresses(m_ip_addresses);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpIgnoredIpAddrs::clone() {
    return new TrexAstfDpIgnoredIpAddrs(m_ip_addresses);
}

/*************************
+  Sending Client Stats MSG
+ ************************/

bool TrexAstfDpSentClientStats::handle(void) {
    ((TrexAstf*)get_stx())->add_clients_info(m_client_sts);
    return true;
}

/*************************
*  Update tunnel info to Client MSG
*************************/
TrexAstfDpUpdateTunnelClient::TrexAstfDpUpdateTunnelClient(CAstfDB* astf_db, std::vector<client_tunnel_data_t> msg_data) {
    m_astf_db = astf_db;
    m_msg_data =  msg_data;
}

bool TrexAstfDpUpdateTunnelClient::handle(TrexDpCore *dp_core) {
    astf_core(dp_core)->update_tunnel_for_client(m_astf_db, m_msg_data);
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpUpdateTunnelClient::clone() {
    return new TrexAstfDpUpdateTunnelClient(m_astf_db, m_msg_data);
}

/*************************
*  Init dps tunnel handler
*************************/
bool TrexAstfDpInitTunnelHandler::handle(TrexDpCore *dp_core) {
    m_reply.set_reply(astf_core(dp_core)->activate_tunnel_handler(m_activate, m_tunnel_type, m_loopback_mode));
    return true;
}

TrexCpToDpMsgBase* TrexAstfDpInitTunnelHandler::clone() {
    return new TrexAstfDpInitTunnelHandler(m_activate, m_tunnel_type, m_loopback_mode, m_reply);
}

/*************************
+  Report flows per profile
+ ************************/

bool TrexAstfDpFlowInfo::handle(void) {
    ((TrexAstf*)get_stx())->add_flows_info(m_dp_profile_id, m_flows);
    return true;
}
