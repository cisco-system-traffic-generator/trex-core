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

#include "trex_astf.h"
#include "trex_astf_defs.h"
#include "trex_astf_port.h"
#include "trex_astf_rpc_cmds.h"
#include "trex_astf_rx_core.h"
#include "stt_cp.h"
#include <set>

using namespace std;

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

TREX_RPC_CMD(TrexRpcCmdAstfAcquire, "acquire");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfRelease, "release");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfProfileFragment, "profile_fragment");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfProfileClear, "profile_clear");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStart, "start");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStop, "stop");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfUpdate, "update");

TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStartLatency, "start_latency");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStopLatency, "stop_latency");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfUpdateLatency, "update_latency");

TREX_RPC_CMD(TrexRpcCmdAstfCountersDesc, "get_counter_desc");
TREX_RPC_CMD(TrexRpcCmdAstfCountersValues, "get_counter_values");
TREX_RPC_CMD(TrexRpcCmdAstfGetLatencyStats, "get_latency_stats");

TREX_RPC_CMD(TrexRpcCmdAstfTopoGet, "topo_get");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfTopoFragment, "topo_fragment");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfTopoClear, "topo_clear");

/****************************** commands implementation ******************************/

trex_rpc_cmd_rc_e
TrexRpcCmdAstfAcquire::_run(const Json::Value &params, Json::Value &result) {
    const string user = parse_string(params, "user", result);
    const bool force = parse_bool(params, "force", result);

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

    if ( stx->profile_needs_parsing() ) {
        stx->profile_clear();
    }

    if ( stx->topo_needs_parsing() ) {
        stx->topo_clear();
    }

    res["handler"] = stx->get_owner().get_handler();
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfRelease::_run(const Json::Value &params, Json::Value &result) {
    get_astf_object()->release_context();

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfProfileFragment::_run(const Json::Value &params, Json::Value &result) {
    const bool frag_first = parse_bool(params, "frag_first", result, false);
    const bool frag_last = parse_bool(params, "frag_last", result, false);

    TrexAstf *stx = get_astf_object();

    if ( frag_first && !frag_last) {
        const string hash = parse_string(params, "md5", result);
        if ( stx->profile_cmp_hash(hash) ) {
            result["result"]["matches_loaded"] = true;
            return TREX_RPC_CMD_OK;
        }
    }

    const string fragment = parse_string(params, "fragment", result);

    try {
        if ( frag_first ) {
            stx->profile_clear();
        }

        stx->profile_append(fragment);

        if ( frag_last ) {
            stx->profile_set_loaded();
        }

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfProfileClear::_run(const Json::Value &params, Json::Value &result) {
    try {
        get_astf_object()->profile_clear();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStart::_run(const Json::Value &params, Json::Value &result) {
    start_params_t args;
    args.duration = parse_double(params, "duration", result);
    args.mult = parse_double(params, "mult", result);
    args.nc = parse_bool(params, "nc", result);
    args.latency_pps = parse_uint32(params, "latency_pps", result);
    args.ipv6 = parse_bool(params, "ipv6", result);
    args.client_mask = parse_uint32(params, "client_mask", result);

    try {
        get_astf_object()->start_transmit(args);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStop::_run(const Json::Value &params, Json::Value &result) {
    bool stopped = true;
    try {
        stopped = get_astf_object()->stop_transmit();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    result["result"]["stopped"] = stopped;

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfUpdate::_run(const Json::Value &params, Json::Value &result) {
    const double mult = parse_double(params, "mult", result);

    try {
        get_astf_object()->update_rate(mult);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

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
TrexRpcCmdAstfCountersDesc::_run(const Json::Value &params, Json::Value &result) {
    CSTTCp *lpstt = get_platform_api().get_fl()->m_stt_cp;
    if (lpstt) {
        if (lpstt->m_init) {
            lpstt->m_dtbl.dump_meta("counter desc", result["result"]);
        }
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfCountersValues::_run(const Json::Value &params, Json::Value &result) {
    CSTTCp *lpstt = get_platform_api().get_fl()->m_stt_cp;
    if (lpstt) {
        if (lpstt->m_init) {
            lpstt->m_dtbl.dump_values("counter vals", false, result["result"]);
        }
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


/****************************** component implementation ******************************/

/**
 * ASTF RPC component
 * 
 */
TrexRpcCmdsASTF::TrexRpcCmdsASTF() : TrexRpcComponent("ASTF") {
    m_cmds.push_back(new TrexRpcCmdAstfAcquire(this));
    m_cmds.push_back(new TrexRpcCmdAstfRelease(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileFragment(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileClear(this));
    m_cmds.push_back(new TrexRpcCmdAstfStart(this));
    m_cmds.push_back(new TrexRpcCmdAstfStop(this));
    m_cmds.push_back(new TrexRpcCmdAstfUpdate(this));
    m_cmds.push_back(new TrexRpcCmdAstfStartLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfStopLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfUpdateLatency(this));
    m_cmds.push_back(new TrexRpcCmdAstfGetLatencyStats(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersDesc(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersValues(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoGet(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoFragment(this));
    m_cmds.push_back(new TrexRpcCmdAstfTopoClear(this));
}
