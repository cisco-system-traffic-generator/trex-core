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
#include <iostream>
#include <sstream>
#include <trex_rpc_cmds_table.h>

using namespace std;

/**
 * add command
 * 
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdTestAdd::_run(const Json::Value &params, Json::Value &result) {

    const Json::Value &x = params["x"];
    const Json::Value &y = params["y"];
    
    check_param_count(params, 2, result);
    check_field_type(params, "x", FIELD_TYPE_INT, result);
    check_field_type(params, "y", FIELD_TYPE_INT, result);

    result["result"] = x.asInt() + y.asInt();
    return (TREX_RPC_CMD_OK);
}

/**
 * sub command
 * 
 * @author imarom (16-Aug-15)
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdTestSub::_run(const Json::Value &params, Json::Value &result) {

    const Json::Value &x = params["x"];
    const Json::Value &y = params["y"];
        
    check_param_count(params, 2, result);
    check_field_type(params, "x", TrexRpcCommand::FIELD_TYPE_INT, result);
    check_field_type(params, "y", TrexRpcCommand::FIELD_TYPE_INT, result);

    result["result"] = x.asInt() - y.asInt();
    return (TREX_RPC_CMD_OK);
}

/**
 * ping command
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    /* validate count */
    check_param_count(params, 0, result);

    result["result"] = "ACK";
    return (TREX_RPC_CMD_OK);
}

/**
 * query command
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdGetReg::_run(const Json::Value &params, Json::Value &result) {
    vector<string> cmds;

    /* validate count */
    check_param_count(params, 0, result);

    TrexRpcCommandsTable::get_instance().query(cmds);

    Json::Value test = Json::arrayValue;
    for (auto cmd : cmds) {
        test.append(cmd);
    }

    result["result"] = test;

    return (TREX_RPC_CMD_OK);
}

