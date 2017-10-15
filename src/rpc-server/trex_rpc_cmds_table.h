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

#ifndef __TREX_RPC_CMDS_TABLE_H__
#define __TREX_RPC_CMDS_TABLE_H__

#include <unordered_map>
#include <string>
#include <vector>
#include <json/json.h>

#include "trex_rpc_cmd_api.h"

/**
 * holds all the commands registered
 * 
 * @author imarom (13-Aug-15)
 */
class TrexRpcCommandsTable {

public:

    static TrexRpcCommandsTable& get_instance() {
        static TrexRpcCommandsTable instance;
        return instance;
    }

    /**
     * init the RPC table
     *  
     * @param config_name - how the RPC configuration will be 
     *                      presented.
     *                      in api_sync RPC command, the client
     *                      should provide the correct config name
     *                      and version
     *  
     * @param major         configuration major version
     * @param minor         configuration minor version
     */
    void init(const std::string &config_name, int major, int minor);
    
    /**
     * loads a new component to the RPC table
     * 
     */
    void load_component(TrexRpcComponent *component);
    
    /**
     * register a new command
     * 
     */
    void register_command(TrexRpcCommand *command);

    /**
     * lookup for a command
     * 
     */
    TrexRpcCommand * lookup(const std::string &method_name);

    /**
     * query all commands registered
     * 
     */
    void query(std::vector<std::string> &cmds);
    
    
    /**
     * returns the config name
     */
    const std::string & get_config_name() const {
        return m_api_ver.get_name();
    }
    
private:
    TrexRpcCommandsTable();
    ~TrexRpcCommandsTable();

    /**
     * clears the RPC table
     * 
     */
    void reset();
    
    /* c++ 2011 style singleton */
    TrexRpcCommandsTable(TrexRpcCommandsTable const&)  = delete;  
    void operator=(TrexRpcCommandsTable const&)        = delete;

    
    /**
     * holds all the loaded RPC components
     * 
     */
    std::vector<TrexRpcComponent *> m_rpc_components;
    
    /**
     * holds all the registered RPC commands in a flat hash table
     * 
     */
    std::unordered_map<std::string, TrexRpcCommand *> m_rpc_cmd_table;
    
    TrexRpcAPIVersion   m_api_ver;
    bool                m_is_init;
};

#endif /* __TREX_RPC_CMDS_TABLE_H__ */
