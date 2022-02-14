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

/**
 * ASTF specific RPC commands
 */

#include "astf/astf_db.h"
#include "trex_astf.h"
#include "trex_astf_defs.h"
#include "trex_astf_port.h"
#include "trex_astf_rpc_cmds.h"
#include "trex_astf_rx_core.h"
#include "../common/trex_port.h"
#include "stt_cp.h"
#include <set>
#include "trex_astf_dp_core.h"
#include "trex_astf_messaging.h"
#include "dyn_sts.h"

using namespace std;

#define MAX_TG_ALLOWED_AT_ONCE 10

TrexAstfPort* get_astf_port(uint8_t port_id) {
    return (TrexAstfPort*)get_stx()->get_port_by_id(port_id);
}

/**
 * interface for RPC ASTF command
 *
 * @author imarom (13-Aug-15)
 */
class TrexRpcAstfCommand : public TrexRpcCommand {
public:
    TrexRpcAstfCommand(const string &method_name, TrexRpcComponent *component, bool needs_api, bool needs_ownership) :
            TrexRpcCommand(method_name, component, needs_api, needs_ownership) {}

protected:

    virtual void verify_ownership(const Json::Value &params, Json::Value &result) {
        string handler = parse_string(params, "handler", result);

        if (!get_astf_object()->get_owner().verify(handler)) {
            generate_execute_err(result, "must acquire the context for this operation");
        }
    }
};

/**
 * syntactic sugar for creating a simple command
 */

#define TREX_RPC_CMD_ASTF_DEFINE_EXTENDED(class_name, cmd_name, needs_api, needs_ownership, ext)                           \
    class class_name : public TrexRpcAstfCommand {                                                                         \
    public:                                                                                                                \
        class_name (TrexRpcComponent *component) : TrexRpcAstfCommand(cmd_name, component, needs_api, needs_ownership) {}  \
    protected:                                                                                                             \
        virtual trex_rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result);                                    \
        ext                                                                                                                \
    }

/**
 * defines an owned RPC command (requires ownership at STX level)
 */
#define TREX_RPC_CMD_ASTF_OWNED(class_name, cmd_name)           TREX_RPC_CMD_ASTF_DEFINE_EXTENDED(class_name, cmd_name, true, true, ;)
#define TREX_RPC_CMD_ASTF_OWNED_EXT(class_name, cmd_name, ext)  TREX_RPC_CMD_ASTF_DEFINE_EXTENDED(class_name, cmd_name, true, true, ext)

typedef set<TrexAstfPort *> port_list_t;

/****************************** commands declarations ******************************/

TREX_RPC_CMD(TrexRpcCmdAstfSync, "sync");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfIncEpoch, "inc_epoch");

TREX_RPC_CMD(TrexRpcCmdAstfAcquire, "acquire");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfRelease, "release");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfProfileFragment, "profile_fragment");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfProfileClear, "profile_clear");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStart, "start");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStop, "stop");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfUpdate, "update");

TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfProfileList, "get_profile_list");

TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfServiceMode, "service");

TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStartLatency, "start_latency");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStopLatency, "stop_latency");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfUpdateLatency, "update_latency");

TREX_RPC_CMD(TrexRpcCmdAstfCountersDesc, "get_counter_desc");
TREX_RPC_CMD(TrexRpcCmdAstfCountersValues, "get_counter_values");
TREX_RPC_CMD(TrexRpcCmdAstfTotalCountersValues, "get_total_counter_values");
TREX_RPC_CMD(TrexRpcCmdAstfDynamicCountersDesc, "get_dynamic_counter_desc");
TREX_RPC_CMD(TrexRpcCmdAstfDynamicCountersValues, "get_dynamic_counter_values");
TREX_RPC_CMD(TrexRpcCmdAstfTotalDynamicCountersValues, "get_total_dynamic_counter_values");
TREX_RPC_CMD(TrexRpcCmdAstfGetTGNames, "get_tg_names");
TREX_RPC_CMD(TrexRpcCmdAstfGetTGStats, "get_tg_id_stats");
TREX_RPC_CMD(TrexRpcCmdAstfGetLatencyStats, "get_latency_stats");
TREX_RPC_CMD(TrexRpcCmdAstfGetTrafficDist, "get_traffic_dist");

TREX_RPC_CMD(TrexRpcCmdAstfTopoGet, "topo_get");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfTopoFragment, "topo_fragment");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfTopoClear, "topo_clear");
TREX_RPC_CMD(TrexRpcCmdEnableDisableClient,     "enable_disable_client");
TREX_RPC_CMD(TrexRpcCmdGetClientsInfo,          "get_clients_info");
TREX_RPC_CMD(TrexRpcCmdIsTunnelSupported,  "is_tunnel_supported");
TREX_RPC_CMD(TrexRpcCmdActivateTunnelMode,  "activate_tunnel_mode");
TREX_RPC_CMD(TrexRpcCmdUpdateTunnelClient,  "update_tunnel_client");
/****************************** commands implementation ******************************/

trex_rpc_cmd_rc_e
TrexRpcCmdAstfSync::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result, "");
    TrexAstf *stx = get_astf_object();

    Json::Value &res = result["result"];
    res["epoch"] = stx->get_epoch();
    res["state"] = stx->get_state();

    if ( profile_id != "" ) {
        Json::Value state_profile =  Json::objectValue;
        stx->get_profiles_status(state_profile);
        res["state_profile"] = state_profile;
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfIncEpoch::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    TrexAstf *stx = get_astf_object();
    try {
        stx->inc_epoch();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"]["epoch"] = stx->get_epoch();

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfAcquire::_run(const Json::Value &params, Json::Value &result) {
    const string user = parse_string(params, "user", result);
    const bool force = parse_bool(params, "force", result);
    uint32_t session_id = parse_uint32(params, "session_id", result);


    TrexAstf *stx = get_astf_object();
    try {
        stx->acquire_context(user, force);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    Json::Value &res = result["result"];
    res["ports"] = Json::objectValue;
    for (auto &port : stx->get_port_map()) {
        const string &handler = port.second->get_owner().get_handler();
        res["ports"][to_string(port.first)] = handler;
    }

    stringstream ss;
    vector<string> profile_list = stx->get_profile_id_list();
    for ( auto profile_id : profile_list ) {
        try {
            TrexAstfPerProfile *pid = stx->get_profile(profile_id);
            if ( pid->profile_needs_parsing() ) {
                pid->profile_init();
            }
        } catch (const TrexException &ex) {
            res["exception"][profile_id] = ex.what();
        }
    }

    if ( stx->topo_needs_parsing() ) {
        stx->topo_clear();
    }

    get_stx()->add_session_id(0,session_id); 

    res["handler"] = stx->get_owner().get_handler();
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfRelease::_run(const Json::Value &params, Json::Value &result) {
    get_astf_object()->release_context();
    get_stx()->remove_session_id(0); // one id 

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfProfileFragment::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    const bool frag_first = parse_bool(params, "frag_first", result, false);
    const bool frag_last = parse_bool(params, "frag_last", result, false);

    TrexAstf *stx = get_astf_object();
    stx->add_profile(profile_id);
    if (!stx->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = stx->get_profile(profile_id);

    if ( frag_first && !frag_last) {
        const string hash = parse_string(params, "md5", result);
        switch ( pid->profile_cmp_hash(hash) ) {
            case HASH_ON_SAME_PROFILE:
                result["result"]["matches_loaded"] = true;
                return TREX_RPC_CMD_OK;
            case HASH_ON_OTHER_PROFILE:
                generate_execute_err(result, "Fragment already added on other profile_id");
            default:
                break;
        }
    }

    const string fragment = parse_string(params, "fragment", result);

    try {
        if ( frag_first ) {
            pid->profile_init();
        }

        pid->profile_append(fragment);

        if ( frag_last ) {
            pid->profile_set_loaded();
        }

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfProfileClear::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    try {
        get_astf_object()->profile_clear(profile_id);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStart::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    start_params_t args;
    args.duration = parse_double(params, "duration", result);
    args.mult = parse_double(params, "mult", result);
    args.nc = parse_bool(params, "nc", result);
    args.latency_pps = parse_uint32(params, "latency_pps", result);
    args.ipv6 = parse_bool(params, "ipv6", result);
    args.client_mask = parse_uint32(params, "client_mask", result);
    args.e_duration = parse_double(params, "e_duration", result, 0.0);
    args.t_duration = parse_double(params, "t_duration", result, 0.0);

    try {
        get_astf_object()->start_transmit(profile_id, args);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStop::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    try {
        get_astf_object()->stop_transmit(profile_id);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfUpdate::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    const double mult = parse_double(params, "mult", result);

    try {
        get_astf_object()->update_rate(profile_id, mult);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfProfileList::_run(const Json::Value &params, Json::Value &result) {
    vector<string> profile_list = get_astf_object()->get_profile_id_list();
    Json::Value json_profile_list = Json::arrayValue;

    for (auto &profile_id : profile_list) {
        json_profile_list.append(profile_id);
    }

    result["result"] = json_profile_list;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfServiceMode::_run(const Json::Value &params, Json::Value &result) {
    /* no need for port id, always run on all ports */
    bool enabled = parse_bool(params, "enabled", result);
    bool filtered = parse_bool(params, "filtered", result, false);
    uint8_t mask = parse_byte(params, "mask", result, 0);

    if ( filtered ) {
        if ( (mask & TrexPort::NO_TCP_UDP) == 0 ) {
            generate_execute_err(result, "Cannot disable no_tcp_udp in ASTF!");
        }
        if ( CGlobalInfo::m_dpdk_mode.get_mode()->is_hardware_filter_needed() ) {
            generate_execute_err(result, "TRex must be at --software mode for filtered service!");
        }
    }

    get_astf_object()->set_service_mode(enabled, filtered, mask);

    /* Also notify ports for trex-console compatiblity */
    astf_port_map_t ports_map = get_astf_object()->get_port_map();
    try {
        for ( auto& port : ports_map ) {
            port.second->set_service_mode(enabled, filtered, mask);
        }
    } catch (TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStartLatency::_run(const Json::Value &params, Json::Value &result) {
    const string src_ipv4_str  = parse_string(params, "src_addr", result);
    const string dst_ipv4_str  = parse_string(params, "dst_addr", result);
    const string dual_ipv4_str  = parse_string(params, "dual_port_addr", result);

    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t dual_ip;
    if (!utl_ipv4_to_uint32(src_ipv4_str.c_str(), src_ip)){
        stringstream ss;
        ss << "invalid source IPv4 address: '" << src_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }

    if (!utl_ipv4_to_uint32(dst_ipv4_str.c_str(), dst_ip)){
        stringstream ss;
        ss << "invalid destination IPv4 address: '" << dst_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }

    if (!utl_ipv4_to_uint32(dual_ipv4_str.c_str(), dual_ip)){
        stringstream ss;
        ss << "invalid dual port ip IPv4 address: '" << dual_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }

    /* check that the dest IP (+dual_ip) of latency is not equal to src IP of any TRex interface */
    uint8_t max_port_id = 0;
    for (auto &port : get_astf_object()->get_port_map()) {
        max_port_id = max(max_port_id, port.first);
    }

    CNodeBase port_node;
    char ip_str[INET_ADDRSTRLEN];
    for (auto &port : get_astf_object()->get_port_map()) {
        try {
            port.second->get_port_node(port_node);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }

        string ip4_buf = port_node.get_src_ip4();
        inet_ntop(AF_INET, ip4_buf.c_str(), ip_str, INET_ADDRSTRLEN);
        uint32_t port_ip_num;
        utl_ipv4_to_uint32(ip_str, port_ip_num);

        for ( uint8_t dual_port_id=0; dual_port_id<=(max_port_id/2); dual_port_id++ ) {
            if ( port_ip_num == dst_ip + dual_port_id*dual_ip ) {
                string err = "Latency dst IP and dual_port might reach port " + to_string(port.first) + " with IP " + ip_str;
                generate_execute_err(result, err);
            }
        }
    }

    lat_start_params_t args;

    args.cps = parse_double(params, "mult", result);
    args.ports_mask  = parse_uint32(params, "mask", result);
    args.client_ip.v4 = src_ip;
    args.server_ip.v4 = dst_ip;
    args.dual_ip = dual_ip;

    try {
        get_astf_object()->start_transmit_latency(args);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStopLatency::_run(const Json::Value &params, Json::Value &result) {
    bool stopped = true;
    try {
        stopped = get_astf_object()->stop_transmit_latency();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    result["result"]["stopped"] = stopped;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfUpdateLatency::_run(const Json::Value &params, Json::Value &result) {

    const double mult = parse_double(params, "mult", result);

    try {
        get_astf_object()->update_latency_rate(mult);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfGetLatencyStats::_run(const Json::Value &params, Json::Value &result) {
    try {
        get_astf_object()->get_latency_stats(result["result"]);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfGetTrafficDist::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    if (!get_astf_object()->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = get_astf_object()->get_profile(profile_id);

    auto db = CAstfDB::instance(pid->get_dp_profile_id());
    if (!db) {
        generate_execute_err(result, "No DB found for profile : " + profile_id);
    }
    auto stx = get_astf_object();
    auto &api = get_platform_api();

    const string &start_ip = parse_string(params, "start_ip", result);
    const string &end_ip   = parse_string(params, "end_ip", result);
    const string &dual_ip  = parse_string(params, "dual_ip", result);
    bool seq_split         = parse_bool(params, "seq_split", result);

    Json::Value &res = result["result"];
    std::vector<std::pair<uint8_t, uint8_t>> cores_id_list;
    uint8_t max_threads = api.get_dp_core_count();
    try {
        for (auto &port : stx->get_port_map()) {
            if ( port.first & 1 ) {
                continue;
            }
            uint8_t dual_id = port.first/2;
            Json::Value cores_ranges;
            api.port_id_to_cores(port.first, cores_id_list);
            for (auto &core_pair : cores_id_list) {
                uint8_t thread_id = core_pair.first;
                CIpPortion portion;
                db->get_thread_ip_range(thread_id, max_threads, dual_id, start_ip, end_ip, dual_ip, seq_split, portion);
                Json::Value core_range;
                core_range["start"] = utl_uint32_to_ipv4(portion.m_ip_start);
                core_range["end"] = utl_uint32_to_ipv4(portion.m_ip_end);
                cores_ranges[to_string(thread_id)] = core_range;
            }
            res[to_string(port.first)] = cores_ranges;
        }
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfCountersDesc::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    TrexAstf *stx = get_astf_object();
    if (!stx->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = stx->get_profile(profile_id);
    CSTTCp *lpstt = pid->get_stt_cp();
    if (lpstt && lpstt->m_init) {
        lpstt->m_dtbl.dump_meta("counter desc", result["result"]);
    } else {
        generate_execute_err(result, "Statistics are not initialized yet");
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfCountersValues::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    TrexAstf *stx = get_astf_object();
    if (!stx->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = stx->get_profile(profile_id);
    CSTTCp *lpstt = pid->get_stt_cp();
    if (lpstt && lpstt->m_init) {
        lpstt->m_dtbl.dump_values("counter vals", false, result["result"]);
    } else {
        generate_execute_err(result, "Statistics are not initialized yet");
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfTotalCountersValues::_run(const Json::Value &params, Json::Value &result) {
    TrexAstf *stx = get_astf_object();
    vector<CSTTCp *> sttcp_total_list = stx->get_sttcp_total_list();

    for (auto lpstt : sttcp_total_list) {
        bool clear = false;
        if(lpstt == sttcp_total_list.front()) {
            clear = true;
        }

        bool calculate = false;
        if(lpstt == sttcp_total_list.back()) {
            calculate = true;
        }

        stx->Accumulate_total(clear, calculate, lpstt);
    }

    stx->m_stt_total_cp->m_dtbl.dump_values("counter vals", false, result["result"]);
    result["result"]["epoch"] = stx->get_epoch();

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfDynamicCountersDesc::_run(const Json::Value &params, Json::Value &result) {
    CCpDynStsInfra *infra = get_astf_object()->get_cp_sts_infra();
    bool is_sum = false;
    if (params.isMember("is_sum")) {
        is_sum = parse_bool(params, "is_sum", result);
    }
    CSTTCp *lpstt;
    if (is_sum) {
        lpstt = infra->m_total_sts;
    } else {
        string profile_id = parse_profile(params, result);
        if (!infra->is_valid_profile(profile_id)) {
            generate_execute_err(result, "Invalid profile : " + profile_id);
        }
        lpstt = infra->get_profile_sts(profile_id);
    }
    if (lpstt && lpstt->m_init) {
        lpstt->m_dtbl.dump_meta("counter desc", result["result"]);
    } else {
        generate_execute_err(result, "Statistics are not initialized yet");
    }
    result["result"]["epoch"] = lpstt->m_epoch;
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfDynamicCountersValues::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    CCpDynStsInfra *infra = get_astf_object()->get_cp_sts_infra();
    if (!infra->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    CSTTCp *lpstt = infra->get_profile_sts(profile_id);
    uint64_t epoch = parse_uint64(params, "epoch", result);
    if (epoch != lpstt->m_epoch) {
        result["result"]["epoch_err"] = 1;
        return (TREX_RPC_CMD_OK);
    }
    if (lpstt && lpstt->m_init) {
        lpstt->m_dtbl.dump_values("counter vals", false, result["result"]);
    } else {
        generate_execute_err(result, "Statistics are not initialized yet");
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfTotalDynamicCountersValues::_run(const Json::Value &params, Json::Value &result) {
    TrexAstf *stx = get_astf_object();
    CCpDynStsInfra *infra = stx->get_cp_sts_infra();
    uint64_t epoch = parse_uint64(params, "epoch", result);
    if (epoch != infra->m_total_sts->m_epoch) {
        result["result"]["epoch_err"] = "Try again: Epoch is not updated";
        return (TREX_RPC_CMD_OK);
    }
    vector<CSTTCp *> sttcp_total_list = infra->get_dyn_sts_list();
    for (auto lpstt : sttcp_total_list) {
        bool clear = false;
        if(lpstt == sttcp_total_list.front()) {
            clear = true;
        }
        infra->accumulate_total_dyn_sts(clear, lpstt);
    }
    infra->m_total_sts->m_dtbl.dump_values("counter vals", false, result["result"]);
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfGetTGNames::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    TrexAstf *stx = get_astf_object();
    if (!stx->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = stx->get_profile(profile_id);

    bool initialized = parse_bool(params, "initialized", result);
    uint64_t epoch = 0;
    if (initialized) {
        epoch = parse_uint64(params, "epoch", result);
    }
    CSTTCp *lpstt = pid->get_stt_cp();
    if (lpstt && lpstt->m_init) {
        CAstfDB *astf_db = CAstfDB::instance(pid->get_dp_profile_id());
        if (lpstt->m_update && astf_db) {
            lpstt->UpdateTGNames(astf_db->get_tg_names());
        }
        uint64_t server_epoch = lpstt->m_epoch;
        result["result"]["epoch"] = server_epoch;
        if ( (initialized && server_epoch != epoch) || !initialized)  {
            lpstt->DumpTGNames(result["result"]);
        }
    } else {
        generate_execute_err(result, "Statistics are not initialized yet for TGs");
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfGetTGStats::_run(const Json::Value &params, Json::Value &result) {
    string profile_id = parse_profile(params, result);
    vector<uint16_t> tgids_arr;
    uint64_t epoch = parse_uint64(params, "epoch", result);
    const Json::Value &tgids = parse_array(params, "tg_ids", result);
    TrexAstf *stx = get_astf_object();
    if (!stx->is_valid_profile(profile_id)) {
        generate_execute_err(result, "Invalid profile : " + profile_id);
    }
    TrexAstfPerProfile *pid = stx->get_profile(profile_id);
    CSTTCp *lpstt = pid->get_stt_cp();
    try {
        if (tgids.size() > MAX_TG_ALLOWED_AT_ONCE) {
            generate_execute_err(result, "Trying to get statistics for too many TGs. Max allowed is "
                                         + to_string(MAX_TG_ALLOWED_AT_ONCE));
        }
        if (lpstt && lpstt->m_init) {
            for (auto &itr: tgids) {
                uint16_t tg_id = itr.asUInt();
                if (tg_id > lpstt->m_num_of_tg_ids) {
                    generate_execute_err(result, "TG ID is too big. Max value is " +
                    to_string(lpstt->m_num_of_tg_ids) + ". Received " + to_string(tg_id));
                }
                if (tg_id == 0) {
                    generate_execute_err(result, "Invalid TG ID = 0");
                }
                tgids_arr.push_back(tg_id);
            }
            uint64_t server_epoch = lpstt->m_epoch;
            result["result"]["epoch"] = server_epoch;
            if (server_epoch == epoch) {
                if (lpstt->m_update) {
                    lpstt->UpdateTGStats(tgids_arr);
                }
                lpstt->DumpTGStats(result["result"], tgids_arr);
            }
        } else {
            generate_execute_err(result, "Statistics are not initialized yet for TGs");
        }
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfTopoGet::_run(const Json::Value &params, Json::Value &result) {
    try {
        get_astf_object()->topo_get(result["result"]);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    return TREX_RPC_CMD_OK;
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfTopoFragment::_run(const Json::Value &params, Json::Value &result) {
    const bool frag_first = parse_bool(params, "frag_first", result, false);
    const bool frag_last = parse_bool(params, "frag_last", result, false);

    TrexAstf *stx = get_astf_object();

    if ( frag_first && !frag_last) {
        const string hash = parse_string(params, "md5", result);
        if ( stx->topo_cmp_hash(hash) ) {
            result["result"]["matches_loaded"] = true;
            return TREX_RPC_CMD_OK;
        }
    }

    const string fragment = parse_string(params, "fragment", result);

    try {
        if ( frag_first ) {
            stx->topo_clear();
        }

        stx->topo_append(fragment);

        if ( frag_last ) {
            stx->topo_set_loaded();
        }

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return TREX_RPC_CMD_OK;
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfTopoClear::_run(const Json::Value &params, Json::Value &result) {
    try {
        get_astf_object()->topo_clear();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return TREX_RPC_CMD_OK;
}

trex_rpc_cmd_rc_e
TrexRpcCmdEnableDisableClient::_run(const Json::Value &params, Json::Value &result) {
    const Json::Value &attr = parse_array(params, "attr", result);
    bool is_enable = parse_bool(params, "is_enable", result);
    bool is_range = parse_bool(params, "is_range", result);

    std::vector<uint32_t> msg_data;
    auto astf_db = CAstfDB::get_instance(0);

    for (auto each_client : attr) {
        uint32_t client_start_ip = 0, client_end_ip = 0;
        
        if (!is_range){
             client_start_ip = parse_uint32(each_client, "client_ip", result);
             msg_data.push_back(client_start_ip);
        }
        else {
             client_start_ip = parse_uint32(each_client, "client_start_ip", result);
             client_end_ip = parse_uint32(each_client, "client_end_ip", result);
             msg_data.push_back(client_start_ip);
             msg_data.push_back(client_end_ip); 
        }
    }

    TrexCpToDpMsgBase *msg = new TrexAstfDpActivateClient(astf_db, msg_data, is_enable, is_range);
    get_astf_object()->send_message_to_all_dp(msg, false);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}

trex_rpc_cmd_rc_e
TrexRpcCmdGetClientsInfo::_run(const Json::Value &params, Json::Value &result) {

    const Json::Value &attr = parse_array(params, "attr", result);
    bool is_range = parse_bool(params, "is_range", result);

    auto astf_db = CAstfDB::get_instance(0);
    cpu_util_full_t cpu_util_full;

    if (get_platform_api().get_cpu_util_full(cpu_util_full) != 0) {
        return TREX_RPC_CMD_INTERNAL_ERR;
    }

    std::vector<Json::Value> dps_results;
    std::vector<uint32_t> msg_data;

    uint32_t client_start_ip = 0, client_end_ip = 0;
    for (auto each_client : attr) {
        if (!is_range){
             client_start_ip = parse_uint32(each_client, "client_ip", result);
             msg_data.push_back(client_start_ip);
        }
        else {
             client_start_ip = parse_uint32(each_client, "client_start_ip", result);
             client_end_ip = parse_uint32(each_client, "client_end_ip", result);
             msg_data.push_back(client_start_ip);
             msg_data.push_back(client_end_ip);
        }
    }

    uint8_t dp_core_count = get_astf_object()->get_dp_core_count();
    for (uint8_t core_id = 0; core_id < dp_core_count; core_id++) {
        static MsgReply<Json::Value> reply;
        reply.reset();
        TrexCpToDpMsgBase *msg = new TrexAstfDpGetClientStats(astf_db, msg_data, is_range, reply);
        get_astf_object()->send_msg_to_dp(core_id, msg);
        dps_results.push_back(reply.wait_for_reply());
    }

    assert(dps_results.size() != 0);
    result["result"] = dps_results[0];
    for (int i=0;i<dps_results.size();i++) {
        Json::Value::Members dp_clients = dps_results[i].getMemberNames();
        for (auto client : dp_clients) {
            if (dps_results[i][client]["Found"] == 1) {
                result["result"][client] = dps_results[i][client];
            }
        }
    }

    return (TREX_RPC_CMD_OK);

}



/****************************** component implementation ******************************/
/*
 * API      : Update tunnel information for a client
 * Param In : list of records. Each record contains
 *            1. Version   : IP Version
 *            2. Client_ip : Client IP 
 *            3. sip       : Tunnel source IP
 *            4. dip       : Tunnel dest ip
 */           


trex_rpc_cmd_rc_e
TrexRpcCmdUpdateTunnelClient::_run(const Json::Value &params, Json::Value &result) {
    uint8_t tunnel_type     = parse_uint16(params, "tunnel_type", result);
    std::vector<client_tunnel_data_t> all_msg_data;

    auto astf_db = CAstfDB::get_instance(0);
    if (!CGlobalInfo::m_options.m_tunnel_enabled) {
        generate_execute_err(result, "The tunnel mode needs to be activated");
    }
    CTunnelHandler* tunnel_handler = get_astf_object()->get_tunnel_handler();
    assert(tunnel_handler);
    if (tunnel_type == tunnel_handler->get_tunnel_type()) {
        tunnel_handler->parse_tunnel(params, result, all_msg_data);
    } else {
        generate_execute_err(result, "tunnel type is not supported");
    }

    TrexCpToDpMsgBase *msg = new TrexAstfDpUpdateTunnelClient(astf_db, all_msg_data);
    get_astf_object()->send_message_to_all_dp(msg, false);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}


trex_rpc_cmd_rc_e
TrexRpcCmdActivateTunnelMode::_run(const Json::Value &params, Json::Value &result) {
    TrexAstf *astf = get_astf_object();
    bool is_idle = (astf->get_state() == AstfProfileState::STATE_IDLE) ? true : false;

    if (!is_idle) {
        generate_execute_err(result, "The server state should be IDLE");
    }

    uint8_t tunnel_type = parse_uint16(params, "tunnel_type", result);
    bool loopback = parse_bool(params, "loopback", result);
    bool activate = parse_bool(params, "activate", result);
    bool is_tunnel_enabled = CGlobalInfo::m_options.m_tunnel_enabled;

    if (is_tunnel_enabled && activate) {
        generate_execute_err(result, "The tunnel mode have been already activated");
    }

    if (!is_tunnel_enabled && !activate) {
        generate_execute_err(result, "The tunnel mode is already off");
    }
    TrexAstf* stx = get_astf_object();
    CCpDynStsInfra* cp_infra = stx->get_cp_sts_infra();
    if (!activate) {
        CTunnelHandler* tunnel_handler = stx->get_tunnel_handler();
        cp_infra->remove_counters(tunnel_handler->get_meta_data());
    }
    astf->activate_tunnel_handler(activate, tunnel_type);
    CTunnelHandler* tunnel_handler = get_astf_object()->get_tunnel_handler();
    if (tunnel_handler == nullptr && activate) {
        generate_execute_err(result, "The tunnel handler is null on tunnel mode: tunnel type is not supported");
    }
    if (tunnel_handler && !activate) {
        generate_execute_err(result, "The tunnel handler is not null and the tunnel mode is off");
    }
    CGlobalInfo::m_options.m_tunnel_loopback = loopback && activate;
    CGlobalInfo::m_options.m_tunnel_enabled = activate;

    uint8_t dp_core_count = astf->get_dp_core_count();
    DpsDynSts dp_sts;
    for (uint8_t core_id = 0; core_id < dp_core_count; core_id++) {
        static MsgReply<dp_sts_t> reply;
        reply.reset();
        TrexCpToDpMsgBase *msg = new TrexAstfDpInitTunnelHandler(activate, tunnel_type, loopback, reply);
        astf->send_msg_to_dp(core_id, msg);
        dp_sts.add_dp_dyn_sts(reply.wait_for_reply());
    }
    if (activate) {
        assert(cp_infra->add_counters(tunnel_handler->get_meta_data(), &dp_sts));
    }

    return (TREX_RPC_CMD_OK);
}


trex_rpc_cmd_rc_e
TrexRpcCmdIsTunnelSupported::_run(const Json::Value &params, Json::Value &result) {
    uint8_t tunnel_type     = parse_uint16(params, "tunnel_type", result);
    CTunnelHandler *tunnel_handler = create_tunnel_handler(tunnel_type, (uint8_t)(TUNNEL_MODE_CP));
    assert(tunnel_handler);
    std::string error_msg = "";
    //there are certain flags that required for the tunnel mode to work for example in gtpu mode: --software --tso-disable --lro-disable are required
    bool is_tunnel_supported = tunnel_handler->is_tunnel_supported(error_msg);
    result["result"]["is_tunnel_supported"] = is_tunnel_supported;
    if (!is_tunnel_supported) {
        result["result"]["error_msg"] = "Tunnel is not supported: " + error_msg;
    }
    delete tunnel_handler;
    return (TREX_RPC_CMD_OK);
}

/****************************** component implementation ******************************/

/**
 * ASTF RPC component
 * 
 */
TrexRpcCmdsASTF::TrexRpcCmdsASTF() : TrexRpcComponent("ASTF") {
    m_cmds.push_back(new TrexRpcCmdAstfSync(this));
    m_cmds.push_back(new TrexRpcCmdAstfIncEpoch(this));
    m_cmds.push_back(new TrexRpcCmdAstfAcquire(this));
    m_cmds.push_back(new TrexRpcCmdAstfRelease(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileFragment(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileClear(this));
    m_cmds.push_back(new TrexRpcCmdAstfStart(this));
    m_cmds.push_back(new TrexRpcCmdAstfStop(this));
    m_cmds.push_back(new TrexRpcCmdAstfUpdate(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileList(this));
    m_cmds.push_back(new TrexRpcCmdAstfServiceMode(this));
    m_cmds.push_back(new TrexRpcCmdAstfStartLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfStopLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfUpdateLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfGetLatencyStats(this));
    m_cmds.push_back(new TrexRpcCmdAstfGetTrafficDist(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersDesc(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersValues(this));
    m_cmds.push_back(new TrexRpcCmdAstfTotalCountersValues(this));
    m_cmds.push_back(new TrexRpcCmdAstfGetTGNames(this));
    m_cmds.push_back(new TrexRpcCmdAstfGetTGStats(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoGet(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoFragment(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoClear(this));
    m_cmds.push_back(new TrexRpcCmdEnableDisableClient(this));
    m_cmds.push_back(new TrexRpcCmdGetClientsInfo(this));
    m_cmds.push_back(new TrexRpcCmdUpdateTunnelClient(this));
    m_cmds.push_back(new TrexRpcCmdActivateTunnelMode(this));
    m_cmds.push_back(new TrexRpcCmdIsTunnelSupported(this));
    m_cmds.push_back(new TrexRpcCmdAstfTotalDynamicCountersValues(this));
    m_cmds.push_back(new TrexRpcCmdAstfDynamicCountersDesc(this));
    m_cmds.push_back(new TrexRpcCmdAstfDynamicCountersValues(this));
}
