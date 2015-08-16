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
#include <trex_rpc_commands.h>
#include <iostream>
#include <sstream>

using namespace std;

/***************** commands **********/
class TestRpcAddRpcMethod : public TrexRpcCommand {
public:

    TestRpcAddRpcMethod() : TrexRpcCommand("test_rpc_add") {
    }

    virtual rpc_cmd_rc_e _run(const Json::Value &params, std::string &output) {
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

        std::stringstream ss;
        ss << (x.asInt() + y.asInt());
        output = ss.str();
         
        return (RPC_CMD_OK);
    }
};

/*************** generic command methods *********/

TrexRpcCommand::TrexRpcCommand(const string &method_name) : m_name(method_name) {
}

TrexRpcCommand::rpc_cmd_rc_e 
TrexRpcCommand::run(const Json::Value &params, std::string &output) {
    return _run(params, output);
}

const string & TrexRpcCommand::get_name() {
    return m_name;
}


/************* table related methods ***********/
TrexRpcCommandsTable::TrexRpcCommandsTable() {
    /* add the test command (for gtest) */
    register_command(new TestRpcAddRpcMethod());

    TrexRpcCommand *cmd = m_rpc_cmd_table["test_rpc_add"];
}

TrexRpcCommand * TrexRpcCommandsTable::lookup(const string &method_name) {
    return m_rpc_cmd_table[method_name];
}


void TrexRpcCommandsTable::register_command(TrexRpcCommand *command) {

    m_rpc_cmd_table[command->get_name()] = command;
}
