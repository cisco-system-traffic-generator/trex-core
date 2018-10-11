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


#include "rpc-server/trex_rpc_cmds_table.h"
#include "common/trex_rpc_cmds_common.h"
#include "publisher/trex_publisher.h"
#include "trex_messaging.h"
#include "utl_sync_barrier.h"
#include "md5.h"

#include "astf/astf_db.h"
#include "trex_astf.h"
#include "trex_astf_dp_core.h"
#include "trex_astf_messaging.h"
#include "trex_astf_port.h"
#include "trex_astf_rpc_cmds.h"
#include "trex_astf_rx_core.h"

#include "stt_cp.h"

using namespace std;

TrexAstf::TrexAstf(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 1;
    const int API_VER_MINOR = 0;

    /* init the RPC table */
    TrexRpcCommandsTable::get_instance().init("ASTF", API_VER_MAJOR, API_VER_MINOR);

    /* load the RPC components for ASTF */
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsCommon());
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsASTF());
    const TrexPlatformApi &api = get_platform_api();

     /* create ASTF ports */
    for (int i = 0; i < api.get_port_count(); i++) {
        if ( !CGlobalInfo::m_options.m_dummy_port_map[i] ) {
            m_ports[i] = (TrexPort *)new TrexAstfPort(i);
        }
    }

    m_profile_buffer = "";
    states_names = {"Idle", "Transmitting"};
    assert(states_names.size()==AMOUNT_OF_STATES);

    m_cur_state = STATE_IDLE;
    m_active_cores = 0;

    /* create RX core */
    CRxCore *rx = (CRxCore*) new CRxAstfCore();
    rx->create(cfg.m_rx_cfg);
    m_sync_b = api.get_sync_barrier();
    m_fl = api.get_fl();

    m_rx = rx;
    m_wd = nullptr;
}


TrexAstf::~TrexAstf() {
    for (auto &port_pair : m_ports) {
        delete port_pair.second;
    }

    delete m_rx;
    m_rx = nullptr;
}


TrexAstfPort * TrexAstf::get_port_by_id(uint8_t port_id) {
    return (TrexAstfPort *)TrexSTX::get_port_by_id(port_id);
}


void TrexAstf::launch_control_plane() {
    /* start RPC server */
    m_rpc_server.start();
}

void TrexAstf::shutdown() {
    /* stop RPC server */
    m_rpc_server.stop();

    /* shutdown all DP cores */
    send_msg_to_all_dp(new TrexDpQuit());

    /* shutdown RX */
    send_msg_to_rx(new TrexRxQuit());
}

void TrexAstf::dp_core_finished(int thread_id) {
    m_active_cores--;
    assert(m_active_cores>=0);

    if ( m_active_cores || m_cur_state == STATE_IDLE ) {
        return;
    }

    set_barrier(0.5);
    TrexCpToDpMsgBase *msg = new TrexAstfDpDeleteTcp();
    send_message_to_all_dp(msg);
    m_sync_b->listen(true);

    for (auto &port_pair : get_port_map()) {
        port_pair.second->stop();
    }

    m_cur_state = STATE_IDLE;
}

TrexDpCore* TrexAstf::create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) {
    return new TrexAstfDpCore(thread_id, core);
}

void TrexAstf::publish_async_data(void) {
}

void TrexAstf::set_barrier(double timeout_sec) {
    m_sync_b->reset(timeout_sec);
}


void TrexAstf::check_whitelist_states(const states_t &whitelist) {
    assert(whitelist.size());
    for ( auto &state : whitelist ) {
        if ( m_cur_state == state ) {
            return;
        }
    }

    string err = "Invalid state: " + states_names[m_cur_state] + ", should be";
    if ( whitelist.size() > 1 ) {
        err += " one of following";
    }
    bool first = true;
    for ( auto &state : whitelist ) {
        if ( first ) {
            err += ": " + states_names[state];
            first = false;
        } else {
            err += "," + states_names[state];
        }
    }
    throw TrexException(err);
}

void TrexAstf::acquire_context(const string &user, bool force) {
    if ( get_owner().is_free() || force ) {
        get_owner().own("", 0);
        for (auto &port: m_ports) {
            port.second->get_owner().own(user, 0);
        }
    } else {
        throw TrexException("context is already taken (use 'force' argument to override)");
    }
}

void TrexAstf::release_context(void) {
    for (auto &port: m_ports) {
        port.second->get_owner().release();
    }
    get_owner().release();
}

bool TrexAstf::profile_check(uint32_t total_size) {
    return total_size == m_profile_buffer.size();
}

bool TrexAstf::profile_check(const string &hash) {
    return md5(m_profile_buffer) == hash;
}

void TrexAstf::profile_clear(void) {
    check_whitelist_states({STATE_IDLE});
    m_profile_buffer.clear();
}

void TrexAstf::profile_append(const string &fragment) {
    check_whitelist_states({STATE_IDLE});
    m_profile_buffer += fragment;
}


void TrexAstf::profile_load(void) {
    check_whitelist_states({STATE_IDLE});

    CAstfDB *db = CAstfDB::instance();

    string err;
    bool rc;

    rc = db->set_profile_one_msg(m_profile_buffer, err);
    if ( !rc ) {
        throw TrexException(err);
    }

    int num_dp_cores = CGlobalInfo::m_options.preview.getCores() * CGlobalInfo::m_options.get_expected_dual_ports();
    CJsonData_err err_obj = db->verify_data(num_dp_cores);

    if (err_obj.is_error()) {
        throw TrexException(err_obj.description());
    }
}

void TrexAstf::start_transmit(double duration, double mult,bool nc) {
    if ( unlikely(!m_wd) ) {
        m_wd = TrexWatchDog::getInstance().get_current_monitor();
    }

    check_whitelist_states({STATE_IDLE});

    CGlobalInfo::m_options.m_factor = mult;

    m_fl->m_stt_cp->clear_counters();

    set_barrier(0.5);
    TrexCpToDpMsgBase *msg = new TrexAstfDpStart(duration,nc);
    send_message_to_all_dp(msg);
    m_sync_b->listen(true);

    for (auto &port_pair : get_port_map()) {
        port_pair.second->start();
    }

    m_active_cores += m_dp_core_count;
    m_cur_state = STATE_WORK;
}

bool TrexAstf::stop_transmit(void) {
    if ( m_cur_state == STATE_IDLE ) {
        return true;
    }

    set_barrier(0.5);
    TrexCpToDpMsgBase *msg = new TrexAstfDpStop();
    send_message_to_all_dp(msg);
    if ( m_sync_b->listen(false) != 0 ) {
        return false;
    }

    return true;
}


void TrexAstf::send_message_to_all_dp(TrexCpToDpMsgBase *msg) {
    for ( uint8_t core_id = 0; core_id < m_dp_core_count; core_id++ ) {
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
        ring->Enqueue((CGenNode *)msg->clone());
    }
    delete msg;
    msg = nullptr;
}


