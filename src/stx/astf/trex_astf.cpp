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
#include "trex_astf_topo.h"

#include "stt_cp.h"

using namespace std;

TrexAstf::TrexAstf(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 1;
    const int API_VER_MINOR = 2;
    m_l_state = STATE_L_IDLE;
    m_latency_pps = 0;
    m_lat_with_traffic = false;

    TrexRpcCommandsTable &rpc_table = TrexRpcCommandsTable::get_instance();

    /* init the RPC table */
    rpc_table.init("ASTF", API_VER_MAJOR, API_VER_MINOR);

    /* load the RPC components for ASTF */
    rpc_table.load_component(new TrexRpcCmdsCommon());
    rpc_table.load_component(new TrexRpcCmdsASTF());
    const TrexPlatformApi &api = get_platform_api();
    m_opts = &CGlobalInfo::m_options;

     /* create ASTF ports */
    for (int i = 0; i < api.get_port_count(); i++) {
        if ( !m_opts->m_dummy_port_map[i] ) {
            m_ports[i] = (TrexPort *)new TrexAstfPort(i);
        }
    }

    m_opts->m_astf_mode = CParserOption::OP_ASTF_MODE_CLIENT_MASK;

    m_profile_buffer = "";
    m_profile_hash = "";
    m_profile_parsed = false;

    m_topo_buffer = "";
    m_topo_hash = "";
    m_topo_parsed = false;

    m_states_names = {"Idle", "Loaded profile", "Parsing profile", "Setup traffic", "Transmitting", "Cleanup traffic"};
    assert(m_states_names.size()==AMOUNT_OF_STATES);

    m_state = STATE_IDLE;
    m_active_cores = 0;

    /* create RX core */
    CRxCore *rx = (CRxCore*) new CRxAstfCore();
    rx->create(cfg.m_rx_cfg);
    m_sync_b = api.get_sync_barrier();
    m_fl = api.get_fl();

    m_rx = rx;
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

void TrexAstf::parse() {
    change_state(STATE_PARSE);

    string *prof = profile_needs_parsing() ? &m_profile_buffer : nullptr;
    string *topo = topo_needs_parsing() ? &m_topo_buffer : nullptr;
    assert(prof||topo);

    TrexCpToDpMsgBase *msg = new TrexAstfLoadDB(prof, topo);
    send_message_to_dp(0, msg);
}

void TrexAstf::build() {
    change_state(STATE_BUILD);

    TrexCpToDpMsgBase *msg = new TrexAstfDpCreateTcp();
    send_message_to_all_dp(msg);
}

void TrexAstf::transmit() {
    if ( m_lat_with_traffic ) {
        CAstfDB *db = CAstfDB::instance();
        lat_start_params_t args;

        try {
            if ( !db->get_latency_info(args.client_ip.v4,
                                       args.server_ip.v4,
                                       args.dual_ip) ) {
                throw TrexException("No valid ip range for latency");
            }

            args.cps = m_latency_pps;
            args.ports_mask = 0xffffffff;
            start_transmit_latency(args);
        } catch (const TrexException &ex) {
            m_error = ex.what();
            cleanup();
            return;
        }
    }

    change_state(STATE_TX);
    set_barrier(0.5);

    TrexCpToDpMsgBase *msg = new TrexAstfDpStart();
    send_message_to_all_dp(msg);
}


void TrexAstf::cleanup() {
    change_state(STATE_CLEANUP);

    if (m_lat_with_traffic && (m_l_state==STATE_L_WORK)) {
        m_latency_pps = 0;
        m_lat_with_traffic = false;
        stop_transmit_latency();
    }

    TrexCpToDpMsgBase *msg = new TrexAstfDpDeleteTcp();
    send_message_to_all_dp(msg);
}

bool TrexAstf::profile_needs_parsing() {
    return m_profile_hash.size() && !m_profile_parsed;
}

bool TrexAstf::topo_needs_parsing() {
    return m_topo_hash.size() && !m_topo_parsed;
}

void TrexAstf::change_state(state_e new_state) {
    m_state = new_state;
    TrexPort::port_state_e port_state = TrexPort::PORT_STATE_IDLE;

    switch ( m_state ) {
        case STATE_IDLE:
            m_active_cores = 0;
            port_state = TrexPort::PORT_STATE_IDLE;
            break;
        case STATE_LOADED:
            m_active_cores = 0;
            port_state = TrexPort::PORT_STATE_ASTF_LOADED;
            break;
        case STATE_PARSE:
            m_active_cores = 1;
            port_state = TrexPort::PORT_STATE_ASTF_PARSE;
            break;
        case STATE_BUILD:
            m_active_cores = m_dp_core_count;
            port_state = TrexPort::PORT_STATE_ASTF_BUILD;
            break;
        case STATE_TX:
            m_active_cores = m_dp_core_count;
            port_state = TrexPort::PORT_STATE_TX;
            break;
        case STATE_CLEANUP:
            m_active_cores = m_dp_core_count;
            port_state = TrexPort::PORT_STATE_ASTF_CLEANUP;
            break;
        case AMOUNT_OF_STATES:
            assert(0);
    }

    Json::Value data;
    data["state"] = m_state;
    if ( is_error() ) {
        data["error"] = m_error;
        m_error = "";
    }
    get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_STATE_CHG, data);
    for (auto &port: get_port_map()) {
        port.second->change_state(port_state);
    }
}

void TrexAstf::all_dp_cores_finished() {
    switch ( m_state ) {
        case STATE_PARSE:
            if ( is_error() ) {
                change_state(STATE_LOADED);
            } else {
                m_profile_parsed = true;
                m_topo_parsed = true;
                build();
            }
            break;
        case STATE_BUILD:
            if ( is_error() ) {
                cleanup();
            } else {
                transmit();
            }
            break;
        case STATE_TX:
            cleanup();
            break;
        case STATE_CLEANUP:
            change_state(STATE_LOADED);
            break;
        default:
            printf("DP cores should not report in state: %s", m_states_names[m_state].c_str());
            exit(1);
    }
}

void TrexAstf::dp_core_finished(int thread_id) {
    m_active_cores--;
    if ( m_active_cores == 0 ) {
        all_dp_cores_finished();
    } else {
        assert(m_active_cores>0);
    }
}

void TrexAstf::dp_core_error(int thread_id, const string &err) {
    switch ( m_state ) {
        case STATE_PARSE:
            m_error = err;
            break;
        case STATE_BUILD:
            m_error = err;
            break;
        default:
            printf("DP core should not report error in state: %s\n", m_states_names[m_state].c_str());
            printf("Error is: %s\n", err.c_str());
            exit(1);
    }
    dp_core_finished(thread_id);
}


TrexDpCore* TrexAstf::create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) {
    return new TrexAstfDpCore(thread_id, core);
}

void TrexAstf::publish_async_data() {
    CRxAstfCore * rx= get_rx();
    rx->cp_update_stats();
}

void TrexAstf::set_barrier(double timeout_sec) {
    m_sync_b->reset(timeout_sec);
}


void TrexAstf::check_whitelist_states(const states_t &whitelist) {
    assert(whitelist.size());
    for ( auto &state : whitelist ) {
        if ( m_state == state ) {
            return;
        }
    }

    string err = "Invalid state: " + m_states_names[m_state] + ", should be";
    if ( whitelist.size() > 1 ) {
        err += " one of following";
    }
    bool first = true;
    for ( auto &state : whitelist ) {
        if ( first ) {
            err += ": " + m_states_names[state];
            first = false;
        } else {
            err += "," + m_states_names[state];
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

void TrexAstf::release_context() {
    for (auto &port: m_ports) {
        port.second->get_owner().release();
    }
    get_owner().release();
}

bool TrexAstf::profile_cmp_hash(const string &hash) {
    return m_profile_hash == hash;
}

void TrexAstf::profile_clear() {
    check_whitelist_states({STATE_IDLE, STATE_LOADED});
    if ( m_state == STATE_LOADED ) {
        change_state(STATE_IDLE);
    }
    m_profile_buffer.clear();
    m_profile_hash.clear();
    m_profile_parsed = false;
}

void TrexAstf::profile_append(const string &fragment) {
    check_whitelist_states({STATE_IDLE});
    m_profile_buffer += fragment;
}

void TrexAstf::profile_set_loaded() {
    check_whitelist_states({STATE_IDLE});
    change_state(STATE_LOADED);
    m_profile_hash = md5(m_profile_buffer);
}

bool TrexAstf::topo_cmp_hash(const string &hash) {
    return m_topo_hash == hash;
}

void TrexAstf::topo_clear() {
    check_whitelist_states({STATE_IDLE, STATE_LOADED});
    m_topo_buffer.clear();
    m_topo_hash.clear();
    m_topo_parsed = false;
}

void TrexAstf::topo_append(const string &fragment) {
    check_whitelist_states({STATE_IDLE, STATE_LOADED});
    m_topo_buffer += fragment;
}

void TrexAstf::topo_set_loaded() {
    check_whitelist_states({STATE_IDLE, STATE_LOADED});
    m_topo_hash = md5(m_topo_buffer);
}

void TrexAstf::topo_get(Json::Value &result) {
    CAstfDB::instance()->get_topo()->to_json(result["topo_data"]);
}

void TrexAstf::start_transmit(const start_params_t &args) {
    check_whitelist_states({STATE_LOADED});

    if ( args.latency_pps ) {
        if (m_l_state != STATE_L_IDLE) {
            throw TrexException("Latency state is not idle, should stop latency first");
        }
        m_latency_pps = args.latency_pps;
        m_lat_with_traffic = true;
    }

    m_opts->m_factor           = args.mult;
    m_opts->m_duration         = args.duration;
    m_opts->m_astf_client_mask = args.client_mask;
    m_opts->preview.setNoCleanFlowClose(args.nc);
    m_opts->preview.set_ipv6_mode_enable(args.ipv6);
    ClientCfgDB &m_cc_db = m_fl->m_client_config_info;
    m_cc_db.clear();

    m_fl->m_stt_cp->clear_counters();

    if ( profile_needs_parsing() || topo_needs_parsing() ) {
        parse();
    } else {
        build();
    }
}

bool TrexAstf::stop_transmit() {
    if ( m_state == STATE_IDLE ) {
        return true;
    }

    m_opts->preview.setNoCleanFlowClose(true);

    TrexCpToDpMsgBase *msg = new TrexAstfDpStop();
    send_message_to_all_dp(msg);
    return false;
}

void TrexAstf::update_rate(double mult) {
    check_whitelist_states({STATE_TX});

    // time interval for opening new flow will be multiplied by old_new_ratio
    // new mult higher => time is shorter
    double old_new_ratio = m_opts->m_factor / mult;
    if ( std::isnan(old_new_ratio) || std::isinf(old_new_ratio) ) {
        throw TrexException("Ratio between current rate and new one is invalid.");
    }

    m_opts->m_factor = mult;
    TrexCpToDpMsgBase *msg = new TrexAstfDpUpdate(old_new_ratio);
    send_message_to_all_dp(msg);
}


void TrexAstf::start_transmit_latency(const lat_start_params_t &args) {

    if (m_l_state != STATE_L_IDLE){
        throw TrexException("Latency state is not idle, should stop latency first");
    }

    m_l_state = STATE_L_WORK;

    TrexRxStartLatency *msg = new TrexRxStartLatency(args);
    send_msg_to_rx(msg);
}

bool TrexAstf::stop_transmit_latency() {

    if (m_l_state != STATE_L_WORK){
        return true;
    }

    TrexRxStopLatency *msg = new TrexRxStopLatency();
    send_msg_to_rx(msg);
    m_l_state = STATE_L_IDLE;
    return (true);
}

void TrexAstf::update_latency_rate(double mult) {

    if ( m_l_state != STATE_L_WORK ) {
        string err = "Latency is not active, can't update rate";
        throw TrexException(err);
    }

    TrexRxUpdateLatency *msg = new TrexRxUpdateLatency(mult);
    send_msg_to_rx(msg);
}

void TrexAstf::get_latency_stats(Json::Value & obj) {
    CRxAstfCore * rx= get_rx();
    std::string  json_str;
    /* to do covert this function  to native object */
    rx->cp_get_json(json_str);
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(json_str,obj);
    assert(parsingSuccessful==true);
}

void TrexAstf::send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg, bool clone) {
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    if ( clone ) {
        ring->Enqueue((CGenNode *)msg->clone());
    } else {
        ring->Enqueue((CGenNode *)msg);
    }
}

void TrexAstf::send_message_to_all_dp(TrexCpToDpMsgBase *msg) {
    for ( uint8_t core_id = 0; core_id < m_dp_core_count; core_id++ ) {
        send_message_to_dp(core_id, msg, true);
    }
    delete msg;
    msg = nullptr;
}


