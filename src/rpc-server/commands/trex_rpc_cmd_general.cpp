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
#include <../linux_dpdk/version.h>
#include <trex_rpc_server_api.h>

using namespace std;

/**
 * get status
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetStatus::_run(const Json::Value &params, Json::Value &result) {

    /* validate count */
    check_param_count(params, 0, result);

    Json::Value &section = result["result"];

    section["general"]["version"]       = VERSION_BUILD_NUM;
    section["general"]["build_date"]    = get_build_date();
    section["general"]["build_time"]    = get_build_time();
    section["general"]["version_user"]  = VERSION_USER;
    section["general"]["uptime"]        = TrexRpcServer::get_server_uptime();

    return (TREX_RPC_CMD_OK);
}

