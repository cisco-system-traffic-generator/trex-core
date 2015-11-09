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
#include <trex_rpc_cmds_table.h>
#include <iostream>

#include "commands/trex_rpc_cmds.h"

using namespace std;

/************* table related methods ***********/
TrexRpcCommandsTable::TrexRpcCommandsTable() {
    /* add the test command (for gtest) */
    register_command(new TrexRpcCmdTestAdd());
    register_command(new TrexRpcCmdTestSub());
    

    /* general */
    register_command(new TrexRpcCmdPing());
    register_command(new TrexRpcCmdGetCmds());
    register_command(new TrexRpcCmdGetVersion());
    register_command(new TrexRpcCmdGetSysInfo());
    register_command(new TrexRpcCmdGetOwner());
    register_command(new TrexRpcCmdAcquire());
    register_command(new TrexRpcCmdRelease());
    register_command(new TrexRpcCmdGetPortStats());

    register_command(new TrexRpcCmdSyncUser());

    /* stream commands */
    register_command(new TrexRpcCmdAddStream());
    register_command(new TrexRpcCmdRemoveStream());
    register_command(new TrexRpcCmdRemoveAllStreams());
    register_command(new TrexRpcCmdGetStreamList());
    register_command(new TrexRpcCmdGetStream());
    register_command(new TrexRpcCmdGetAllStreams());

    register_command(new TrexRpcCmdStartTraffic());
    register_command(new TrexRpcCmdStopTraffic());
    register_command(new TrexRpcCmdPauseTraffic());
    register_command(new TrexRpcCmdResumeTraffic());
}

TrexRpcCommandsTable::~TrexRpcCommandsTable() {
    for (auto cmd : m_rpc_cmd_table) {
        delete cmd.second;
    }
}

TrexRpcCommand * TrexRpcCommandsTable::lookup(const string &method_name) {
    auto search = m_rpc_cmd_table.find(method_name);

    if (search != m_rpc_cmd_table.end()) {
        return search->second;
    } else {
        return NULL;
    }
}


void TrexRpcCommandsTable::register_command(TrexRpcCommand *command) {

    m_rpc_cmd_table[command->get_name()] = command;
}

void TrexRpcCommandsTable::query(vector<string> &cmds) {
    for (auto cmd : m_rpc_cmd_table) {
        cmds.push_back(cmd.first);
    }
}

