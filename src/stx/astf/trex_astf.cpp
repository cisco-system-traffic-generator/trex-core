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
    const int API_VER_MINOR = 6;
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

    m_topo_buffer = "";
    m_topo_hash = "";
    m_topo_parsed = false;

    m_state = STATE_IDLE;
    m_epoch = 0;

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

void TrexAstf::parse(string profile_id) {
    if (is_another_profile_busy(profile_id)) {
        add_cmd_in_wait_list(profile_id, CMD_PARSE);
        return;
    } else {
        del_cmd_in_wait_list(profile_id);
    }

    string *prof = profile_needs_parsing(profile_id) ? &(get_profile_by_id(profile_id)->m_profile_buffer) : nullptr;
    string *topo = topo_needs_parsing() ? &m_topo_buffer : nullptr;
    assert(prof||topo);

    profile_change_state(profile_id, STATE_PARSE);

    uint32_t profile_index = get_profile_index_by_id(profile_id);
    TrexCpToDpMsgBase *msg = new TrexAstfLoadDB(profile_index, prof, topo);
    send_message_to_dp(0, msg);
}

void TrexAstf::build(string profile_id) {
    if (is_another_profile_busy(profile_id)) {
        add_cmd_in_wait_list(profile_id, CMD_BUILD);
        return;
    } else {
        del_cmd_in_wait_list(profile_id);
    }

    profile_change_state(profile_id, STATE_BUILD);

    double factor          = get_profile_by_id(profile_id)->m_factor;
    uint32_t profile_index = get_profile_index_by_id(profile_id);
    TrexCpToDpMsgBase *msg = new TrexAstfDpCreateTcp(profile_index, factor);
    send_message_to_all_dp(msg);
}

void TrexAstf::transmit(string profile_id) {
    uint32_t profile_index = get_profile_index_by_id(profile_id);

    /* Resize the statistics vector depending on the number of template groups */
    CSTTCp* lpstt = get_sttcp_by_id(profile_id);
    lpstt->Resize(CAstfDB::instance(profile_index)->get_num_of_tg_ids());

    if ( m_lat_with_traffic ) {
        CAstfDB *db = CAstfDB::instance(profile_index);
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
            get_profile_by_id(profile_id)->m_error = ex.what();
            cleanup(profile_id);
            return;
        }
    }

    if (!is_another_profile_transmitting(profile_id)) {
        set_barrier(0.5);
    }

    profile_change_state(profile_id, STATE_TX);

    double duration = get_profile_by_id(profile_id)->m_duration;
    TrexCpToDpMsgBase *msg = new TrexAstfDpStart(profile_index, duration);

    send_message_to_all_dp(msg);
}


void TrexAstf::cleanup(string profile_id) {
    if (is_another_profile_busy(profile_id)) {
        add_cmd_in_wait_list(profile_id, CMD_CLEANUP);
        return;
    } else {
        del_cmd_in_wait_list(profile_id);
    }

    profile_change_state(profile_id, STATE_CLEANUP);
    get_profile_by_id(profile_id)->m_profile_stopping = true;

    if (m_lat_with_traffic && (m_l_state==STATE_L_WORK)) {
        m_latency_pps = 0;
        m_lat_with_traffic = false;
        stop_transmit_latency();
    }

    uint32_t profile_index = get_profile_index_by_id(profile_id);
    TrexCpToDpMsgBase *msg = new TrexAstfDpDeleteTcp(profile_index);
    send_message_to_all_dp(msg);
}

void TrexAstf::profile_clear(string profile_id){
    profile_check_whitelist_states(profile_id, {STATE_IDLE, STATE_LOADED});
    if (profile_id == DEFAULT_ASTF_PROFILE_ID) {
        profile_init(profile_id);
        return;
    }

    profile_change_state(profile_id, STATE_DELETE);

    uint32_t profile_index = get_profile_index_by_id(profile_id);
    TrexCpToDpMsgBase *msg = new TrexAstfDeleteDB(profile_index);
    send_message_to_dp(0, msg);
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
    vector<state_e> states = get_profile_state_list();

    for (auto it : states) {
        temp_state |= (0x01 << it);
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
    } else if (temp_state & (0x01 << STATE_IDLE)) {
        change_state(STATE_IDLE);
    }
}

void TrexAstf::publish_astf_state(string profile_id) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    if (mprof->m_profile_state == STATE_DELETE) {
        return;
    }

    /* Publish the state change of each profile */
    Json::Value data;
    data["profile_id"] = profile_id;
    data["state"] = mprof->m_profile_state;
    data["epoch"] = m_epoch;
    if ( is_error(profile_id) ) {
        data["error"] = mprof->m_error;
    }

    get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_PROFILE_STATE_CHG, data);


    /* Publish the state change of all profiles */
    int old_state = m_state;

    update_astf_state();
    if (old_state == m_state) {
        mprof->m_error = "";
        return;
    }

    data.clear();
    data["state"] = m_state;
    data["epoch"] = m_epoch;
    if ( is_error(profile_id) && !is_trans_state() ) {
        data["error"] = mprof->m_error;
    }

    mprof->m_error = "";

    get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_STATE_CHG, data);
}

void TrexAstf::publish_astf_profile_clear(string profile_id) {
     Json::Value data;
     data["profile_id"] = profile_id;
     data["epoch"] = m_epoch;

     get_publisher()->publish_event(TrexPublisher::EVENT_ASTF_PROFILE_CLEARED, data);
}

void TrexAstf::get_profiles_status(Json::Value &result) {
    vector<string> profile_id_list = get_profile_id_list();
    Json::Value get_profiles_status_json = Json::objectValue;

    for (auto profile_id : profile_id_list) {
        state_e j = get_profile_state_by_id(profile_id);
        stringstream ss;
        ss << profile_id;
        get_profiles_status_json[ss.str()] = j;
    }

    result = get_profiles_status_json;
}

void TrexAstf::all_dp_cores_finished(uint32_t profile_index) {
    string profile_id = get_profile_id_by_index(profile_index);
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    switch ( mprof->m_profile_state ) {
        case STATE_PARSE:
            if ( is_error(profile_id) || mprof->m_profile_stopping ) {
                profile_change_state(profile_id, STATE_LOADED);
                del_cmd_in_wait_list(profile_id);
                run_cmd_in_wait_list();
            } else {
                mprof->m_profile_parsed = true;
                m_topo_parsed = true;
                build(profile_id);
            }
            break;
        case STATE_BUILD:
            if ( is_error(profile_id) || mprof->m_profile_stopping ) {
                del_cmd_in_wait_list(profile_id);
                cleanup(profile_id);
            } else {
                transmit(profile_id);
                run_cmd_in_wait_list();
            }
            break;
        case STATE_TX:
            cleanup(profile_id);
            break;
        case STATE_CLEANUP:
            profile_change_state(profile_id, STATE_LOADED);
            run_cmd_in_wait_list();
            break;
        case STATE_DELETE:
            delete_profile(profile_id);
            publish_astf_profile_clear(profile_id);
            break;
        default:
            printf("DP cores should not report in state: %s", m_states_names[mprof->m_profile_state].c_str());
            exit(1);
    }
}

void TrexAstf::dp_core_finished(int thread_id, uint32_t profile_index) {
    TrexAstfPerProfile* mprof = get_profile_by_id(get_profile_id_by_index(profile_index));

    mprof->m_active_cores--;
    if ( mprof->m_active_cores == 0 ) {
        all_dp_cores_finished(profile_index);
    } else {
        assert(mprof->m_active_cores>0);
    }
}

void TrexAstf::dp_core_error(int thread_id, uint32_t profile_index, const string &err) {
    TrexAstfPerProfile* mprof = get_profile_by_id(get_profile_id_by_index(profile_index));

    switch ( mprof->m_profile_state ) {
        case STATE_PARSE:
            mprof->m_error = err;
            break;
        case STATE_BUILD:
            mprof->m_error = err;
            break;
        default:
            printf("DP core should not report error in state: %s\n", m_states_names[mprof->m_profile_state].c_str());
            printf("Error is: %s\n", err.c_str());
            exit(1);
    }
    dp_core_finished(thread_id, profile_index);
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

void TrexAstf::start_transmit(string profile_id, const start_params_t &args) {
    profile_check_whitelist_states(profile_id, {STATE_LOADED});

    if ( args.latency_pps ) {
        if (m_l_state != STATE_L_IDLE) {
            throw TrexException("Latency state is not idle, should stop latency first");
        }
        m_latency_pps = args.latency_pps;
        m_lat_with_traffic = true;
    }

    TrexAstfPerProfile* mprof  = get_profile_by_id(profile_id);
    mprof->m_factor            = args.mult;
    mprof->m_duration          = args.duration;
    mprof->m_nc_flow_close     = args.nc;

    m_opts->m_astf_client_mask = args.client_mask;
    m_opts->preview.setNoCleanFlowClose(args.nc);
    m_opts->preview.set_ipv6_mode_enable(args.ipv6);

    if ( profile_needs_parsing(profile_id) || topo_needs_parsing() ) {
        parse(profile_id);
    } else {
        build(profile_id);
    }
}

void TrexAstf::stop_transmit(string profile_id) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    /* check already stopping */
    if (mprof->m_profile_stopping) {
        return;
    }

    /* check cmd in waiting list */
    cmd_wait_e cmd = get_cmd_in_wait_list(profile_id);
    if (cmd != AMOUNT_OF_CMDS) {
        if (cmd != CMD_STOP) {
            del_cmd_in_wait_list(profile_id);
            profile_change_state(profile_id, STATE_LOADED);
        }
        return;
    }

    /* check state */
    state_e state = get_profile_state_by_id(profile_id);
    if (state == STATE_IDLE || state == STATE_LOADED) {
        return;
    } else if (state == STATE_TX && is_another_profile_busy(profile_id)) {
        add_cmd_in_wait_list(profile_id, CMD_STOP);
        return;
    } else {
        del_cmd_in_wait_list(profile_id);
    }

    mprof->m_profile_stopping = true;
    m_opts->preview.setNoCleanFlowClose(mprof->m_nc_flow_close);

    if ( state == STATE_TX ) {
        uint32_t profile_index = get_profile_index_by_id(profile_id);
        TrexCpToDpMsgBase *msg = new TrexAstfDpStop(profile_index);
        send_message_to_all_dp(msg);
    }
}

void TrexAstf::update_rate(string profile_id, double mult) {
    profile_check_whitelist_states(profile_id, {STATE_TX});

    // time interval for opening new flow will be multiplied by old_new_ratio
    // new mult higher => time is shorter
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);
    double old_new_ratio = mprof->m_factor / mult;
    if ( std::isnan(old_new_ratio) || std::isinf(old_new_ratio) ) {
        throw TrexException("Ratio between current rate and new one is invalid.");
    }

    mprof->m_factor = mult;
    TrexCpToDpMsgBase *msg = new TrexAstfDpUpdate(mprof->m_profile_index, old_new_ratio);
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

void TrexAstf::inc_epoch() {
    if ( is_trans_state() ) {
        throw TrexException("Can't increase epoch in current state: " + m_states_names[m_state]);
    }
    m_epoch++;
}

void TrexAstf::run_cmd_in_wait_list()
{
    /* Execute the first one in command waiting list */
    if (!m_cmd_wait_list.empty()) {
        string id = m_cmd_wait_list.begin()->first;
        cmd_wait_e cmd = m_cmd_wait_list.begin()->second;
        del_cmd_in_wait_list(id);
        if (cmd == CMD_PARSE) {
            parse(id);
        } else if (cmd == CMD_BUILD) {
            build(id);
        } else if (cmd == CMD_STOP) {
            stop_transmit(id);
        } else if (cmd == CMD_CLEANUP) {
            cleanup(id);
        }
    }
}

void TrexAstf::add_cmd_in_wait_list(string profile_id, cmd_wait_e cmd)
{
    for (auto i = m_cmd_wait_list.begin(); i != m_cmd_wait_list.end(); i++) {
        if (i->first == profile_id) {
            if (i->second == cmd) {
                return;
            }
            m_cmd_wait_list.erase(i);
            break;
        }
    }
    m_cmd_wait_list.push_back(make_pair(profile_id, cmd));
}

void TrexAstf::del_cmd_in_wait_list(string profile_id)
{
    for (auto i = m_cmd_wait_list.begin(); i != m_cmd_wait_list.end(); i++) {
        if (i->first == profile_id) {
            m_cmd_wait_list.erase(i);
            break;
        }
    }
}

TrexAstf::cmd_wait_e TrexAstf::get_cmd_in_wait_list(string profile_id)
{
    for (auto i = m_cmd_wait_list.begin(); i != m_cmd_wait_list.end(); i++) {
        if (i->first == profile_id) {
            return i->second;
        }
    }
    return AMOUNT_OF_CMDS;
}


/***********************************************************
 * TrexAstfProfile
 ***********************************************************/
TrexAstfProfile::TrexAstfProfile() {
    m_states_names = {"Idle", "Loaded profile", "Parsing profile", "Setup traffic", "Transmitting", "Cleanup traffic", "Delete profile"};
    assert(m_states_names.size()==AMOUNT_OF_STATES);

    m_profile_last_index = 0;
    /* Create default profile for backward compatibility */
    TrexAstfPerProfile* m_instance = new TrexAstfPerProfile(m_profile_last_index++);
    m_profile_list.insert(map<string, TrexAstfPerProfile *>::value_type(
                          DEFAULT_ASTF_PROFILE_ID, m_instance));
}

TrexAstfProfile::~TrexAstfProfile() {
    for (auto mprofile : m_profile_list) {
        delete mprofile.second;
    }
}

void TrexAstfProfile::add_profile(string profile_id) {
    if (is_valid_profile(profile_id)) {
        return;
    }

    TrexAstfPerProfile* m_instance = new TrexAstfPerProfile(m_profile_last_index++);
    m_profile_list.insert(map<string, TrexAstfPerProfile *>::value_type(
                          profile_id, m_instance));

    int i;
    CSTTCp* lpstt = m_instance->m_stt_cp;
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

bool TrexAstfProfile::delete_profile(string profile_id)
{
    if (is_valid_profile(profile_id)) {
        delete m_profile_list.find(profile_id)->second;
        m_profile_list.erase(profile_id);
        return true;
    }

    return false;
}

bool TrexAstfProfile::is_valid_profile(string profile_id) {
    return m_profile_list.count(profile_id) ? true : false;
}

uint32_t TrexAstfProfile::get_num_profiles() {
    return m_profile_list.size();
}

TrexAstfPerProfile* TrexAstfProfile::get_profile_by_id(string profile_id) {
    if (is_valid_profile(profile_id)) {
        return m_profile_list[profile_id];
    }
    else {
        throw TrexException("ASTF profile_id " + profile_id + " does not exist");
    }
}

uint32_t TrexAstfProfile::get_profile_index_by_id(string profile_id) {
    return get_profile_by_id(profile_id)->m_profile_index;
}

TrexAstfProfile::state_e TrexAstfProfile::get_profile_state_by_id(string profile_id) {
    return get_profile_by_id(profile_id)->m_profile_state;
}

string TrexAstfProfile::get_profile_id_by_index(uint32_t profile_index)
{
    for (auto mprofile : m_profile_list) {
        if (mprofile.second->m_profile_index == profile_index) {
            return mprofile.first;
        }
    }

    return "";
}

CSTTCp* TrexAstfProfile::get_sttcp_by_id(string profile_id) {
    return is_valid_profile(profile_id) ? get_profile_by_id(profile_id)->m_stt_cp : 0;
}

vector<string> TrexAstfProfile::get_profile_id_list()
{
    vector<string> profile_id_list;

    for (auto mprofile : m_profile_list) {
        profile_id_list.push_back(mprofile.first);
    }

    return profile_id_list;
}

vector<TrexAstfProfile::state_e> TrexAstfProfile::get_profile_state_list()
{
    vector<state_e> profile_state_list;

    for (auto mprofile : m_profile_list) {
        profile_state_list.push_back(mprofile.second->m_profile_state);
    }

    return profile_state_list;
}

vector<CSTTCp *> TrexAstfProfile::get_sttcp_list()
{
    vector<CSTTCp *> sttcp_list;

    for (auto mprofile : m_profile_list) {
        sttcp_list.push_back(mprofile.second->m_stt_cp);
    }

    return sttcp_list;
}

bool TrexAstfProfile::profile_needs_parsing(string profile_id) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    return mprof->m_profile_hash.size() && !(mprof->m_profile_parsed) && (mprof->m_profile_state != STATE_PARSE);
}

trex_astf_hash_e TrexAstfProfile::profile_cmp_hash(string profile_id, const string &hash) {
    for (auto mprofile : m_profile_list) {
        if (mprofile.second->m_profile_hash == hash) {
            if (mprofile.first == profile_id) {
                return HASH_ON_SAME_PROFILE;
            }
            else {
                return HASH_ON_OTHER_PROFILE;
            }
        }
    }

    return HASH_OK;
}

void TrexAstfProfile::profile_init(string profile_id){
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    profile_check_whitelist_states(profile_id, {STATE_IDLE, STATE_LOADED});
    if ( mprof->m_profile_state == STATE_LOADED ) {
        del_cmd_in_wait_list(profile_id);
        profile_change_state(profile_id, STATE_IDLE);
    }
    mprof->m_profile_buffer.clear();
    mprof->m_profile_hash.clear();
    mprof->m_profile_parsed = false;
}

void TrexAstfProfile::profile_append(string profile_id, const string &fragment) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    profile_check_whitelist_states(profile_id, {STATE_IDLE});
    mprof->m_profile_buffer += fragment;
}

void TrexAstfProfile::profile_set_loaded(string profile_id) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);

    profile_check_whitelist_states(profile_id, {STATE_IDLE});
    profile_change_state(profile_id, STATE_LOADED);
    mprof->m_profile_hash = md5(mprof->m_profile_buffer);
}

void TrexAstfProfile::profile_change_state(string profile_id, state_e new_state) {
    TrexAstfPerProfile* mprof = get_profile_by_id(profile_id);
    mprof->m_profile_state = new_state;

    switch ( new_state ) {
        case STATE_IDLE:
            mprof->m_active_cores = 0;
            break;
        case STATE_LOADED:
            mprof->m_profile_stopping = false;
            mprof->m_active_cores = 0;
            break;
        case STATE_PARSE:
            mprof->m_active_cores = 1;
            break;
        case STATE_BUILD:
            mprof->m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_TX:
            mprof->m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_CLEANUP:
            mprof->m_active_cores = get_platform_api().get_dp_core_count();
            break;
        case STATE_DELETE:
            mprof->m_active_cores = 1;
            break;
        case AMOUNT_OF_STATES:
            assert(0);
    }

    publish_astf_state(profile_id);
}

void TrexAstfProfile::profile_check_whitelist_states(string profile_id, const states_t &whitelist) {
    state_e profile_state = get_profile_state_by_id(profile_id);

    assert(whitelist.size());
    for ( auto &state : whitelist ) {
        if ( profile_state == state ) {
            return;
        }
    }

    string err = "Invalid state: " + m_states_names[profile_state] + ", should be";
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

bool TrexAstfProfile::is_profile_state_build(string profile_id) {
    return get_profile_state_by_id(profile_id) == STATE_BUILD;
}

bool TrexAstfProfile::is_another_profile_transmitting(string profile_id)
{
    for (auto id : get_profile_id_list()) {
        if (id == profile_id) {
            continue;
        }
        if (get_profile_state_by_id(id) == STATE_TX) {
            return true;
        }
    }
    return false;
}

bool TrexAstfProfile::is_another_profile_busy(string profile_id)
{
    for (auto id : get_profile_id_list()) {
        if (id == profile_id) {
            continue;
        }
        TrexAstfPerProfile* mprof = get_profile_by_id(id);
        if (mprof->m_profile_state == STATE_PARSE || mprof->m_profile_state == STATE_BUILD ||
            mprof->m_profile_stopping == true) {
            return true;
        }
    }
    return false;
}

