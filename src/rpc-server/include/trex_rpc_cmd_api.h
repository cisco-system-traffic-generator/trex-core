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

#ifndef __TREX_RPC_CMD_API_H__
#define __TREX_RPC_CMD_API_H__

#include <string>
#include <vector>
#include <json/json.h>

/**
 * interface for RPC command
 * 
 * @author imarom (13-Aug-15)
 */
class TrexRpcCommand {
public:

    /**
     * describe different types of rc for run()
     */
    enum rpc_cmd_rc_e {
        RPC_CMD_OK,
        RPC_CMD_PARAM_COUNT_ERR = 1,
        RPC_CMD_PARAM_PARSE_ERR,
        RPC_CMD_INTERNAL_ERR
    };

    /**
     * method name and params
     */
    TrexRpcCommand(const std::string &method_name) : m_name(method_name) {

    }

    rpc_cmd_rc_e run(const Json::Value &params, Json::Value &result) {
        return _run(params, result);
    }

    const std::string &get_name() {
        return m_name;
    }

    virtual ~TrexRpcCommand() {}

protected:

    /**
     * implemented by the dervied class
     * 
     */
    virtual rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result) = 0;

    std::string m_name;
};

#endif /* __TREX_RPC_CMD_API_H__ */

