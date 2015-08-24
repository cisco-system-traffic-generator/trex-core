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
 * add new stream
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAddStream::_run(const Json::Value &params, Json::Value &result) {

    check_param_count(params, 1, result);
    check_field_type(params, "stream", FIELD_TYPE_OBJ, result);

    Json::Value &stream = result["stream"];

    return (TREX_RPC_CMD_OK);
}

