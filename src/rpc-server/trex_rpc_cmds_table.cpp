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
#include "trex_rpc_cmds_table.h"
#include <iostream>


using namespace std;


/************* table related methods ***********/

TrexRpcCommandsTable::TrexRpcCommandsTable() {
    m_is_init = false;
}


TrexRpcCommandsTable::~TrexRpcCommandsTable() {
    reset();
}


/**
 * init the RPC table with configuration name and version
 * 
 */
void
TrexRpcCommandsTable::init(const std::string &config_name, int major, int minor) {
    
    /* reset any previous data */
    reset();
    
    /* init the API version */
    m_api_ver.init(config_name, major, minor);
    
    m_is_init = true;
}


void
TrexRpcCommandsTable::reset() {
    /* delete all components loaded */
    for (auto comp : m_rpc_components) {
        delete comp;
    }
    
    m_rpc_components.clear();
    m_is_init = false;
}


/**
 * load a component to the table
 */
void
TrexRpcCommandsTable::load_component(TrexRpcComponent *comp) {
    
    /* set the component to the right API version / config */
    comp->set_rpc_api_ver(&m_api_ver);
    
    /* add the component */
    m_rpc_components.push_back(comp);
     
    /* register all the RPC commands */
    for (auto cmd : comp->get_rpc_cmds()) {
        register_command(cmd);
    }
}


/**
 * register a new command to the table
 * 
 */
void
TrexRpcCommandsTable::register_command(TrexRpcCommand *command) {

    m_rpc_cmd_table[command->get_name()] = command;
}


/**
 * query for all the commands
 * @param cmds 
 */
void TrexRpcCommandsTable::query(vector<string> &cmds) {
    for (auto cmd : m_rpc_cmd_table) {
        cmds.push_back(cmd.first);
    }
}


TrexRpcCommand *
TrexRpcCommandsTable::lookup(const string &method_name) {
    auto search = m_rpc_cmd_table.find(method_name);

    if (search != m_rpc_cmd_table.end()) {
        return search->second;
    } else {
        return NULL;
    }
}


