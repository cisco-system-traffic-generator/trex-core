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

using namespace std;

/**
 * add command
 * 
 */

TestRpcAddMethod::TestRpcAddMethod() : TrexRpcCommand("test_rpc_add") {

}

TrexRpcCommand::rpc_cmd_rc_e 
TestRpcAddMethod::_run(const Json::Value &params, Json::Value &result) {

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

TestRpcSubMethod::TestRpcSubMethod() : TrexRpcCommand("test_rpc_sub") {

}

TrexRpcCommand::rpc_cmd_rc_e 
TestRpcSubMethod::_run(const Json::Value &params, Json::Value &result) {

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

