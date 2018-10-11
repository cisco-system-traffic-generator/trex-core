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
#include "trex_astf_port.h"
#include "trex_astf_rpc_cmds.h"
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
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStart, "start");
TREX_RPC_CMD_ASTF_OWNED(TrexRpcCmdAstfStop, "stop");

TREX_RPC_CMD(TrexRpcCmdAstfCountersDesc, "get_counter_desc");
TREX_RPC_CMD(TrexRpcCmdAstfCountersValues, "get_counter_values");

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
        const uint32_t total_size = parse_int(params, "total_size", result);
        if ( stx->profile_check(total_size) ) {
            const string hash = parse_string(params, "md5", result);
            if ( stx->profile_check(hash) ) {
                result["result"]["matches_loaded"] = true;
                return TREX_RPC_CMD_OK;
            }
        }
    }

    const string fragment = parse_string(params, "fragment", result);

    try {
        if ( frag_first ) {
            stx->profile_clear();
        }

        stx->profile_append(fragment);

        if ( frag_last ) {
            stx->profile_load();
        }

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStart::_run(const Json::Value &params, Json::Value &result) {
    const double duration = parse_double(params, "duration", result);
    const double mult = parse_double(params, "mult", result);
    const bool nc = parse_bool(params, "nc", result);

    try {
        get_astf_object()->start_transmit(duration, mult,nc);
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

/****************************** component implementation ******************************/

/**
 * ASTF RPC component
 * 
 */
TrexRpcCmdsASTF::TrexRpcCmdsASTF() : TrexRpcComponent("ASTF") {
    m_cmds.push_back(new TrexRpcCmdAstfAcquire(this));
    m_cmds.push_back(new TrexRpcCmdAstfRelease(this));
    m_cmds.push_back(new TrexRpcCmdAstfProfileFragment(this));
    m_cmds.push_back(new TrexRpcCmdAstfStart(this));
    m_cmds.push_back(new TrexRpcCmdAstfStop(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersDesc(this));
    m_cmds.push_back(new TrexRpcCmdAstfCountersValues(this));
}
