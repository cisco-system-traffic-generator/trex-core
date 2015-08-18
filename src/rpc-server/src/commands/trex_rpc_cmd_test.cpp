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
TrexRpcCommand::rpc_cmd_rc_e 
TrexRpcCmdTestAdd::_run(const Json::Value &params, Json::Value &result) {

    const Json::Value &x = params["x"];
    const Json::Value &y = params["y"];
        
    /* validate count */
    if (params.size() != 2) {
        return (TrexRpcCommand::RPC_CMD_PARAM_COUNT_ERR);
    }

    /* check we have all the required paramters */
    if (!x.isInt() || !y.isInt()) {
        return (TrexRpcCommand::RPC_CMD_PARAM_PARSE_ERR);
    }

    result["result"] = x.asInt() + y.asInt();
    return (RPC_CMD_OK);
}

/**
 * sub command
 * 
 * @author imarom (16-Aug-15)
 */
TrexRpcCommand::rpc_cmd_rc_e 
TrexRpcCmdTestSub::_run(const Json::Value &params, Json::Value &result) {

    const Json::Value &x = params["x"];
    const Json::Value &y = params["y"];
        
    /* validate count */
    if (params.size() != 2) {
        return (TrexRpcCommand::RPC_CMD_PARAM_COUNT_ERR);
    }

    /* check we have all the required paramters */
    if (!x.isInt() || !y.isInt()) {
        return (TrexRpcCommand::RPC_CMD_PARAM_PARSE_ERR);
    }

    result["result"] = x.asInt() - y.asInt();
    return (RPC_CMD_OK);
}

/**
 * ping command
 */
TrexRpcCommand::rpc_cmd_rc_e 
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    /* validate count */
    if (params.size() != 0) {
        return (TrexRpcCommand::RPC_CMD_PARAM_COUNT_ERR);
    }

    result["result"] = "ACK";
    return (RPC_CMD_OK);
}

/**
 * query command
 */
TrexRpcCommand::rpc_cmd_rc_e 
TrexRpcCmdGetReg::_run(const Json::Value &params, Json::Value &result) {
    vector<string> cmds;
    stringstream ss;

    /* validate count */
    if (params.size() != 0) {
        return (TrexRpcCommand::RPC_CMD_PARAM_COUNT_ERR);
    }


    TrexRpcCommandsTable::get_instance().query(cmds);
    for (auto cmd : cmds) {
        ss << cmd << "\n";
    }

    result["result"] = ss.str();
    return (RPC_CMD_OK);
}

