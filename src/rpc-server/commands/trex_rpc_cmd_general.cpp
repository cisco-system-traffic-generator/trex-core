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
#include "trex_rpc_cmds.h"
#include <trex_rpc_server_api.h>
#include <trex_stateless_api.h>

#ifndef TREX_RPC_MOCK_SERVER
    #include <../linux_dpdk/version.h>
#endif

using namespace std;

/**
 * get status
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetStatus::_run(const Json::Value &params, Json::Value &result) {

    Json::Value &section = result["result"];

    #ifndef TREX_RPC_MOCK_SERVER

    section["general"]["version"]       = VERSION_BUILD_NUM;
    section["general"]["build_date"]    = get_build_date();
    section["general"]["build_time"]    = get_build_time();
    section["general"]["built_by"]      = VERSION_USER;

    #else

    section["general"]["version"]       = "v0.0";
    section["general"]["build_date"]    = __DATE__;
    section["general"]["build_time"]    = __TIME__;
    section["general"]["version_user"]  = "MOCK";

    #endif

    section["general"]["uptime"]        = TrexRpcServer::get_server_uptime();
    section["general"]["owner"]         = TrexRpcServer::get_owner();

    // ports

    section["ports"]["count"]           = TrexStateless::get_instance().get_port_count();

    return (TREX_RPC_CMD_OK);
}

/**
 * returns the current owner of the device
 * 
 * @author imarom (08-Sep-15)
 * 
 * @param params 
 * @param result 
 * 
 * @return trex_rpc_cmd_rc_e 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetOwner::_run(const Json::Value &params, Json::Value &result) {
    Json::Value &section = result["result"];

    section["owner"] = TrexRpcServer::get_owner();

    return (TREX_RPC_CMD_OK);
}

/**
 * acquire device
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAcquire::_run(const Json::Value &params, Json::Value &result) {

    const string &new_owner = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);

    /* if not free and not you and not force - fail */
    if ( (!TrexRpcServer::is_free_to_aquire()) && (TrexRpcServer::get_owner() != new_owner) && (!force)) {
        generate_execute_err(result, "device is already taken by '" + TrexRpcServer::get_owner() + "'");
    }

    string handle = TrexRpcServer::set_owner(new_owner);

    result["result"] = handle;

    return (TREX_RPC_CMD_OK);
}

/**
 * release device
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdRelease::_run(const Json::Value &params, Json::Value &result) {
    TrexRpcServer::clear_owner();

    result["result"] = "ACK";

    return (TREX_RPC_CMD_OK);
}
