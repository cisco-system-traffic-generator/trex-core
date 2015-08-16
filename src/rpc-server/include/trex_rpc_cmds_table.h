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

class TrexRpcCommand;

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

    void register_command(TrexRpcCommand *command);

    TrexRpcCommand * lookup(const std::string &method_name);

private:
    TrexRpcCommandsTable();
    ~TrexRpcCommandsTable();

    /* c++ 2011 style singleton */
    TrexRpcCommandsTable(TrexRpcCommandsTable const&)  = delete;  
    void operator=(TrexRpcCommandsTable const&)        = delete;

    /**
     * holds all the registered RPC commands
     * 
     */
    std::unordered_map<std::string, TrexRpcCommand *> m_rpc_cmd_table;
};

#endif /* __TREX_RPC_CMDS_TABLE_H__ */
