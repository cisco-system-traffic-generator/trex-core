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

/***********************************************************
 * TrexAstf
 ***********************************************************/
TrexAstf::TrexAstf(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 1;
    const int API_VER_MINOR = 7;
    m_l_state = STATE_L_IDLE;
    m_latency_pps = 0;
    m_lat_with_traffic = false;
    m_lat_profile_id = 0;

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

    m_topo_buffer = "";
    m_topo_hash = "";
    m_topo_parsed = false;

    m_state = STATE_IDLE;
    m_epoch = 0;

    m_dp_states.resize(api.get_dp_core_count());
    m_stopping_dp = false;

    /* create RX core */
    CRxCore *rx = (CRxCore*) new CRxAstfCore();
    rx->create(cfg.m_rx_cfg);
    m_sync_b = api.get_sync_barrier();
    m_fl = api.get_fl();

    m_rx = rx;

    /* Create default profile for backward compatibility */
    TrexAstfPerProfile* instance = new TrexAstfPerProfile(this);
    m_profile_list.insert(map<string, TrexAstfPerProfile *>::value_type(
                          DEFAULT_ASTF_PROFILE_ID, instance));

    m_profile_id_map[instance->get_dp_profile_id()] = DEFAULT_ASTF_PROFILE_ID;
    m_states_cnt[instance->get_profile_state()]++;
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

bool TrexAstf::is_trans_state() {
    return m_state == STATE_PARSE || m_state == STATE_BUILD || m_state == STATE_CLEANUP;
}

bool TrexAstf::topo_needs_parsing() {
    return m_topo_hash.size() && !m_topo_parsed;
}

void TrexAstf::change_state(state_e new_state) {
    m_state = new_state;
    TrexPort::port_state_e port_state = TrexPort::PORT_STATE_IDLE;

    switch ( m_state ) {
        case STATE_IDLE:
            port_state = TrexPort::PORT_STATE_IDLE;
            break;
        case STATE_LOADED:
            port_state = TrexPort::PORT_STATE_ASTF_LOADED;
            break;
        case STATE_PARSE:
            port_state = TrexPort::PORT_STATE_ASTF_PARSE;
            break;
        case STATE_BUILD:
            port_state = TrexPort::PORT_STATE_ASTF_BUILD;
            break;
        case STATE_TX:
            port_state = TrexPort::PORT_STATE_TX;
            break;
        case STATE_CLEANUP:
            port_state = TrexPort::PORT_STATE_ASTF_CLEANUP;
            break;
        case STATE_DELETE:
            break;
        case AMOUNT_OF_STATES:
            assert(0);
    }

    for (auto &port: get_port_map()) {
        port.second->change_state(port_state);
    }
}

void TrexAstf::update_astf_state() {
    int temp_state = 0;

    for (int i = 0; i < m_states_cnt.size(); i++) {
        if (m_states_cnt[i]) {
            temp_state |= (0x01 << i);
        }
    }

    if (temp_state & (0x01 << STATE_TX)) {
        change_state(STATE_TX);
    } else if (temp_state & (0x01 << STATE_BUILD)) {
        change_state(STATE_BUILD);
    } else if (temp_state & (0x01 << STATE_PARSE)) {
        change_state(STATE_PARSE);
    } else if (temp_state & (0x01 << STATE_CLEANUP)) {
        change_state(STATE_CLEANUP);
    } else if (temp_state & (0x01 << STATE_LOADED)) {
        change_state(STATE_LOADED);
    } else if (temp_state & (0x01 << STATE_DELETE)) {
        change_state(STATE_LOADED); // ASTFClient does not support STATE_DELETE.
    } else if (temp_state & (0x01 << STATE_IDLE)) {
        change_state(STATE_IDLE);
    }
}

void TrexAstf::get_profiles_status(Json::Value &result) {
    vector<string> profile_id_list = get_profile_id_list();
    Json::Value get_profiles_status_json = Json::objectValue;

    for (auto profile_id : profile_id_list) {
        state_e j = get_profile(profile_id)->get_profile_state();
        stringstream ss;
        ss << profile_id;
        get_profiles_status_json[ss.str()] = j;
    }

    result = get_profiles_status_json;
}

void TrexAstf::dp_core_finished(int thread_id, uint32_t dp_profile_id) {
    get_profile(get_profile_id(dp_profile_id))->dp_core_finished();
}

void TrexAstf::dp_core_error(int thread_id, uint32_t dp_profile_id, const string &err) {
    get_profile(get_profile_id(dp_profile_id))->dp_core_error(err);
}

bool TrexAstf::is_dp_core_state(int state, bool any) {
    int states_mask = 0;
    for (auto dp_state: m_dp_states) {
        states_mask |= 1 << dp_state;
    }

    int s_mask = 1 << state;
    return any ? (states_mask & s_mask): (states_mask == s_mask);
}

void TrexAstf::dp_core_state(int thread_id, int state) {
    m_dp_states[thread_id] = state;

    if (m_stopping_dp && is_dp_core_state(TrexAstfDpCore::STATE_IDLE)) {
        m_stopping_dp = false;
        for (auto msg: m_suspended_msgs) {
            send_message_to_all_dp(msg);    // TrexAstfDpCreateTcp
        }
        m_suspended_msgs.clear();
    }
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

bool TrexAstf::topo_cmp_hash(const string &hash) {
    return m_topo_hash == hash;
}

void TrexAstf::topo_clear() {
    check_whitelist_states({STATE_IDLE, STATE_LOADED});
    m_topo_buffer.clear();
    m_topo_hash.clear();
    m_topo_parsed = false;
    ClientCfgDB &m_cc_db = m_fl->m_client_config_info;
    m_cc_db.clear();
    CAstfDB::instance()->get_topo()->clear();
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

bool TrexAstf::is_state_build() {
    return m_state == STATE_BUILD;
}

void TrexAstf::start_transmit(cp_profile_id_t profile_id, const start_params_t &args) {
    TrexAstfPerProfile* pid = get_profile(profile_id);
    pid->profile_check_whitelist_states({STATE_LOADED});

    if ( args.latency_pps ) {
        if (m_l_state != STATE_L_IDLE) {
            throw TrexException("Latency state is not idle, should stop latency first");
        }
        m_latency_pps = args.latency_pps;
        m_lat_with_traffic = true;
        m_lat_profile_id = pid->get_dp_profile_id();
    }

    pid->set_factor(args.mult);
    pid->set_duration(args.duration);
    pid->set_nc_flow_close(args.nc);

    m_opts->m_astf_client_mask = args.client_mask;
    m_opts->preview.set_ipv6_mode_enable(args.ipv6);

    if ( pid->profile_needs_parsing() || topo_needs_parsing() ) {
        pid->parse();
    } else {
        pid->build();
    }
}

void TrexAstf::stop_transmit(cp_profile_id_t profile_id) {
    TrexAstfPerProfile* pid = get_profile(profile_id);

    /* check already stopping or not transmitting */
    if (pid->get_profile_stopping() || pid->get_profile_state() != STATE_TX) {
        return;
    }

    pid->set_profile_stopping(true);

    TrexCpToDpMsgBase *msg = new TrexAstfDpStop(pid->get_dp_profile_id());
    send_message_to_all_dp(msg, true);
}

void TrexAstf::stop_dp_scheduler() {
    if (!m_stopping_dp && is_dp_core_state(TrexAstfDpCore::STATE_TRANSMITTING)) {
        TrexCpToDpMsgBase *msg = new TrexAstfDpScheduler(false);
        send_message_to_all_dp(msg);
        m_stopping_dp = true;
    }
}

void TrexAstf::profile_clear(cp_profile_id_t profile_id){
    TrexAstfPerProfile* pid = get_profile(profile_id);
    pid->profile_check_whitelist_states({STATE_IDLE, STATE_LOADED});

    if (profile_id == DEFAULT_ASTF_PROFILE_ID) {
        pid->profile_init();
        return;
    }

    pid->profile_change_state(STATE_DELETE);

    TrexCpToDpMsgBase *msg = new TrexAstfDeleteDB(pid->get_dp_profile_id());
    send_message_to_dp(0, msg);
}

void TrexAstf::update_rate(cp_profile_id_t profile_id, double mult) {
    TrexAstfPerProfile* pid = get_profile(profile_id);
    pid->profile_check_whitelist_states({STATE_TX});

    // time interval for opening new flow will be multiplied by old_new_ratio
    // new mult higher => time is shorter
    double old_new_ratio = pid->get_factor() / mult;
    if ( std::isnan(old_new_ratio) || std::isinf(old_new_ratio) ) {
        throw TrexException("Ratio between current rate and new one is invalid.");
    }

    pid->set_factor(mult);
    TrexCpToDpMsgBase *msg = new TrexAstfDpUpdate(pid->get_dp_profile_id(), old_new_ratio);
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
        throw TrexException("Latency is not active, can't update rate");
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

string TrexAstf::handle_start_latency(int32_t dp_profile_id) {
    if ( m_lat_profile_id == dp_profile_id && m_lat_with_traffic && m_l_state == STATE_L_IDLE ) {
        CAstfDB *db = CAstfDB::instance(dp_profile_id);
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
            return ex.what();
        }
    }

    return "";
}

void TrexAstf::handle_stop_latency() {
    if (m_lat_with_traffic && m_l_state == STATE_L_WORK) {
        m_latency_pps = 0;
        m_lat_with_traffic = false;
        m_lat_profile_id = 0;
        stop_transmit_latency();
    }
}

void TrexAstf::send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg, bool clone) {
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    if ( clone ) {
        ring->SecureEnqueue((CGenNode *)msg->clone(), true);
    } else {
        ring->SecureEnqueue((CGenNode *)msg, true);
    }
}

void TrexAstf::send_message_to_all_dp(TrexCpToDpMsgBase *msg, bool suspend) {
    /* some messages should not be delivered during DP is stopping */
    if (suspend && (m_suspended_msgs.size() || m_stopping_dp)) {
        m_suspended_msgs.push_back(msg);
        return;
    }

    for ( uint8_t core_id = 0; core_id < m_dp_core_count; core_id++ ) {
        send_message_to_dp(core_id, msg, true);
    }
    delete msg;
    msg = nullptr;
}

void TrexAstf::inc_epoch() {
    if ( is_trans_state() ) {
        throw TrexException("Can't increase epoch in current state: " + m_states_names[m_state]);
    }
    m_epoch++;
}

void TrexAstf::publish_astf_state() {
    /* Publish the state change of all profiles */
    int old_state = m_state;

    update_astf_state();
    if (old_state == m_state) {
        return;
    }

    if (old_state == STATE_TX) {
        /* trigger DP core exit from its scheduler */
        if (m_state != STATE_BUILD && m_state != STATE_PARSE) {
            stop_dp_scheduler();
        }
    }

    Json::Value data;
    data["state"] = m_state;
    data["epoch"] = m_epoch;

    get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_STATE_CHG, data);
}

/***********************************************************
 * TrexAstfProfile
 ***********************************************************/
TrexAstfProfile::TrexAstfProfile() {
    m_states_names = {"Idle", "Loaded profile", "Parsing profile", "Setup traffic", "Transmitting", "Cleanup traffic", "Delete profile"};
    assert(m_states_names.size()==AMOUNT_OF_STATES);
    m_states_cnt = std::vector<uint32_t> (AMOUNT_OF_STATES);

    m_dp_profile_last_id = 0;
}

TrexAstfProfile::~TrexAstfProfile() {
    for (auto mprofile : m_profile_list) {
        delete mprofile.second;
    }
}

void TrexAstfProfile::add_profile(cp_profile_id_t profile_id) {
    if (is_valid_profile(profile_id)) {
        return;
    }

    uint32_t dp_profile_id;
    do {
        dp_profile_id = ++m_dp_profile_last_id;
    } while (get_profile_id(dp_profile_id) != "");

    TrexAstfPerProfile* instance = new TrexAstfPerProfile(get_astf_object(),
                                                          dp_profile_id,
                                                          profile_id);
    m_profile_list.insert(map<string, TrexAstfPerProfile *>::value_type(
                          profile_id, instance));

    m_profile_id_map[instance->get_dp_profile_id()] = profile_id;
    m_states_cnt[instance->get_profile_state()]++;

    int i;
    CSTTCp* lpstt = instance->get_stt_cp();
    if (!lpstt->m_init){
        CFlowGenList *m_fl = get_platform_api().get_fl();
        CFlowGenListPerThread* lpt;
        bool all_init=true;
        for (i = 0; i < m_fl->m_threads_info.size(); i++) {
            lpt = m_fl->m_threads_info[i];
            if (lpt->m_c_tcp == 0 || lpt->m_s_tcp == 0) {
                all_init=false;
                break;
            }
        }
        if (all_init) {
            for (i = 0; i < m_fl->m_threads_info.size(); i++) {
                lpt = m_fl->m_threads_info[i];
                lpstt->Add(TCP_CLIENT_SIDE, lpt->m_c_tcp);
                lpstt->Add(TCP_SERVER_SIDE, lpt->m_s_tcp);
            }
            lpstt->Init();
            lpstt->m_init=true;
        }
    }
}

bool TrexAstfProfile::delete_profile(cp_profile_id_t profile_id) {
    if (is_valid_profile(profile_id)) {
        auto profile = m_profile_list[profile_id];
        m_profile_id_map.erase(profile->get_dp_profile_id());

        auto state = profile->get_profile_state();
        assert(m_states_cnt[state]>0);
        m_states_cnt[state]--;

        delete m_profile_list.find(profile_id)->second;
        m_profile_list.erase(profile_id);
        return true;
    }

    return false;
}

bool TrexAstfProfile::is_valid_profile(cp_profile_id_t profile_id) {
    return m_profile_list.count(profile_id) ? true : false;
}

uint32_t TrexAstfProfile::get_num_profiles() {
    return m_profile_list.size();
}

TrexAstfPerProfile* TrexAstfProfile::get_profile(cp_profile_id_t profile_id) {
    if (is_valid_profile(profile_id)) {
        return m_profile_list[profile_id];
    }
    else {
        throw TrexException("ASTF profile_id " + profile_id + " does not exist");
    }
}

cp_profile_id_t TrexAstfProfile::get_profile_id(uint32_t dp_profile_id)
{
    if (m_profile_id_map.find(dp_profile_id) != m_profile_id_map.end()) {
        return m_profile_id_map[dp_profile_id];
    }

    return "";
}

vector<string> TrexAstfProfile::get_profile_id_list() {
    vector<cp_profile_id_t> profile_id_list;

    for (auto mprofile : m_profile_list) {
        profile_id_list.push_back(mprofile.first);
    }

    return profile_id_list;
}

vector<CSTTCp *> TrexAstfProfile::get_sttcp_list() {
    vector<CSTTCp *> sttcp_list;

    for (auto mprofile : m_profile_list) {
        sttcp_list.push_back(mprofile.second->get_stt_cp());
    }

    return sttcp_list;
}

bool TrexAstfProfile::is_another_profile_transmitting(cp_profile_id_t profile_id) {
    uint32_t cnt = m_states_cnt[STATE_TX];
    if ((cnt > 0) && get_profile(profile_id)->get_profile_state() == STATE_TX) {
        cnt--;
    }
    return cnt ? true : false;
}

bool TrexAstfProfile::is_safe_update_stats() {
    states_t states = { STATE_BUILD, STATE_CLEANUP, STATE_DELETE };
    uint32_t cnt = 0;
    for (auto state : states) {
        cnt += m_states_cnt[state];
    }
    return cnt ? false : true;
}


/***********************************************************
 * TrexAstfPerProfile
 ***********************************************************/
TrexAstfPerProfile::TrexAstfPerProfile(TrexAstf* astf_obj,
                                       uint32_t dp_profile_id,
                                       cp_profile_id_t cp_profile_id) {
    m_dp_profile_id = dp_profile_id;
    m_cp_profile_id = cp_profile_id;

    m_profile_state = STATE_IDLE;
    m_profile_buffer = "";
    m_profile_hash = "";
    m_profile_parsed = false;
    m_profile_stopping = false;

    m_active_cores = 0;
    m_duration = 0.0;
    m_factor = 1.0;
    m_error = "";

    m_stt_cp = new CSTTCp();
    m_stt_cp->Create(dp_profile_id);

    m_astf_obj = astf_obj;
}

TrexAstfPerProfile::~TrexAstfPerProfile() {
    m_stt_cp->Delete();
    delete m_stt_cp;
    m_stt_cp=0;
}

trex_astf_hash_e TrexAstfPerProfile::profile_cmp_hash(const string &hash) {
    for (auto mprofile : m_astf_obj->get_profile_list()) {
        if (mprofile.second->m_profile_hash == hash) {
            if (mprofile.second->m_dp_profile_id == m_dp_profile_id) {
                return HASH_ON_SAME_PROFILE;
            }
            else {
                return HASH_ON_OTHER_PROFILE;
            }
        }
    }

    return HASH_OK;
}

void TrexAstfPerProfile::profile_init() {
    profile_check_whitelist_states({STATE_IDLE, STATE_LOADED});
    if ( m_profile_state == STATE_LOADED ) {
        profile_change_state(STATE_IDLE);
    }
    m_profile_buffer.clear();
    m_profile_hash.clear();
    m_profile_parsed = false;
}

void TrexAstfPerProfile::profile_append(const string &fragment) {
    profile_check_whitelist_states({STATE_IDLE});
    m_profile_buffer += fragment;
}

void TrexAstfPerProfile::profile_set_loaded() {
    profile_check_whitelist_states({STATE_IDLE});
    profile_change_state(STATE_LOADED);
    m_profile_hash = md5(m_profile_buffer);
}

void TrexAstfPerProfile::profile_change_state(state_e new_state) {
    auto old_state = m_profile_state;

    switch ( new_state ) {
        case STATE_IDLE:
            m_active_cores = 0;
            break;
        case STATE_LOADED:
            m_stt_cp->m_update = true;
            m_profile_stopping = false;
            m_active_cores = 0;
            break;
        case STATE_PARSE:
            m_active_cores = 1;
            break;
        case STATE_BUILD:
            m_stt_cp->m_update = false;
            m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_TX:
            if (m_astf_obj->is_safe_update_stats()) {
                m_stt_cp->update_profile_ctx();
            }
            m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_CLEANUP:
            m_stt_cp->Update();
            m_stt_cp->m_update = false;
            m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_DELETE:
            m_stt_cp->m_update = false;
            m_active_cores = 1;
            break;
        case AMOUNT_OF_STATES:
            assert(0);
    }

    m_profile_state = new_state;
    m_astf_obj->m_states_cnt[old_state]--;
    m_astf_obj->m_states_cnt[new_state]++;

    publish_astf_profile_state();
    m_astf_obj->publish_astf_state();
    m_error = "";
}

void TrexAstfPerProfile::profile_check_whitelist_states(const states_t &whitelist) {
    assert(whitelist.size());
    for ( auto &state : whitelist ) {
        if ( m_profile_state == state ) {
            return;
        }
    }

    string err = "Invalid state: " + m_astf_obj->m_states_names[m_profile_state] + ", should be";
    if ( whitelist.size() > 1 ) {
        err += " one of following";
    }
    bool first = true;
    for ( auto &state : whitelist ) {
        if ( first ) {
            err += ": " + m_astf_obj->m_states_names[state];
            first = false;
        } else {
            err += "," + m_astf_obj->m_states_names[state];
        }
    }
    throw TrexException(err);
}

bool TrexAstfPerProfile::is_profile_state_build() {
    return m_profile_state == STATE_BUILD;
}

bool TrexAstfPerProfile::profile_needs_parsing() {
    return m_profile_hash.size() && !(m_profile_parsed) && (m_profile_state != STATE_PARSE);
}

void TrexAstfPerProfile::parse() {
    string *prof = profile_needs_parsing() ? &(m_profile_buffer) : nullptr;
    string *topo = m_astf_obj->topo_needs_parsing() ? m_astf_obj->get_topo_buffer() : nullptr;
    assert(prof||topo);

    profile_change_state(STATE_PARSE);

    TrexCpToDpMsgBase *msg = new TrexAstfLoadDB(m_dp_profile_id, prof, topo);
    m_astf_obj->send_message_to_dp(0, msg);
}

void TrexAstfPerProfile::build() {
    profile_change_state(STATE_BUILD);

    TrexCpToDpMsgBase *msg = new TrexAstfDpCreateTcp(m_dp_profile_id, m_factor);
    m_astf_obj->send_message_to_all_dp(msg);
}

void TrexAstfPerProfile::transmit() {
    /* Resize the statistics vector depending on the number of template groups */
    CSTTCp* lpstt = m_stt_cp;
    lpstt->Resize(CAstfDB::instance(m_dp_profile_id)->get_num_of_tg_ids());

    string err = m_astf_obj->handle_start_latency(m_dp_profile_id);
    if (err != "") {
        m_error = err;
        cleanup();
        return;
    }

    if (!m_astf_obj->is_another_profile_transmitting(m_cp_profile_id)) {
        m_astf_obj->set_barrier(0.5);
    }

    profile_change_state(STATE_TX);

    TrexCpToDpMsgBase *msg = new TrexAstfDpStart(m_dp_profile_id, m_duration, m_nc_flow_close);

    m_astf_obj->send_message_to_all_dp(msg, true);
}

void TrexAstfPerProfile::cleanup() {
    profile_change_state(STATE_CLEANUP);
    m_profile_stopping = true;

    m_astf_obj->handle_stop_latency();

    TrexCpToDpMsgBase *msg = new TrexAstfDpDeleteTcp(m_dp_profile_id, false);
    m_astf_obj->send_message_to_all_dp(msg);
}

void TrexAstfPerProfile::all_dp_cores_finished() {
    switch ( m_profile_state ) {
        case STATE_PARSE:
            if ( is_error() || m_profile_stopping ) {
                profile_change_state(STATE_LOADED);
            } else {
                m_profile_parsed = true;
                m_astf_obj->set_topo_parsed(true);
                build();
            }
            break;
        case STATE_BUILD:
            if ( is_error() || m_profile_stopping ) {
                cleanup();
            } else {
                transmit();
            }
            break;
        case STATE_TX:
            cleanup();
            break;
        case STATE_CLEANUP:
            profile_change_state(STATE_LOADED);
            break;
        case STATE_DELETE:
            publish_astf_profile_clear();
            {
                auto astf = m_astf_obj;
                m_astf_obj->delete_profile(m_cp_profile_id);
                astf->publish_astf_state();     // ASTF state should be updated
            }
            break;
        default:
            printf("DP cores should not report in state: %s", m_astf_obj->m_states_names[m_profile_state].c_str());
            exit(1);
    }
}

void TrexAstfPerProfile::dp_core_finished() {
    m_active_cores--;
    if ( m_active_cores == 0 ) {
        all_dp_cores_finished();
    } else {
        assert(m_active_cores>0);
    }
}

void TrexAstfPerProfile::dp_core_error(const string &err) {
    switch ( m_profile_state ) {
        case STATE_PARSE:
            m_error = err;
            break;
        case STATE_BUILD:
            m_error = err;
            break;
        default:
            printf("DP core should not report error in state: %s\n", m_astf_obj->m_states_names[m_profile_state].c_str());
            printf("Error is: %s\n", err.c_str());
            exit(1);
    }
    dp_core_finished();
}

void TrexAstfPerProfile::publish_astf_profile_state() {
    if (m_profile_state == STATE_DELETE) {
        return;
    }

    /* Publish the state change of each profile */
    Json::Value data;
    data["profile_id"] = m_cp_profile_id;
    data["state"] = m_profile_state;
    data["epoch"] = m_astf_obj->get_epoch();
    if ( is_error() ) {
        data["error"] = m_error;
    }

    m_astf_obj->get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_PROFILE_STATE_CHG, data);
}

void TrexAstfPerProfile::publish_astf_profile_clear() {
    Json::Value data;
    data["profile_id"] = m_cp_profile_id;
    data["epoch"] = m_astf_obj->get_epoch();

    m_astf_obj->get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_PROFILE_CLEARED, data);
}

