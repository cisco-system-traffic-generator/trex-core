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
#include <trex_rpc_exception_api.h>

/**
 * describe different types of rc for run()
 */
typedef enum trex_rpc_cmd_rc_ {
    TREX_RPC_CMD_OK,
    TREX_RPC_CMD_PARAM_COUNT_ERR = 1,
    TREX_RPC_CMD_PARAM_PARSE_ERR,
    TREX_RPC_CMD_INTERNAL_ERR
} trex_rpc_cmd_rc_e;

/**
 * simple exception for RPC command processing
 * 
 * @author imarom (23-Aug-15)
 */
class TrexRpcCommandException : TrexRpcException {
public:
    TrexRpcCommandException(trex_rpc_cmd_rc_e rc) : m_rc(rc) {

    }

    trex_rpc_cmd_rc_e get_rc() {
        return m_rc;

    }

protected:
    trex_rpc_cmd_rc_e m_rc;
};

/**
 * interface for RPC command
 * 
 * @author imarom (13-Aug-15)
 */
class TrexRpcCommand {
public:

    /**
     * method name and params
     */
    TrexRpcCommand(const std::string &method_name) : m_name(method_name) {

    }

    /**
     * entry point for executing RPC command
     * 
     */
    trex_rpc_cmd_rc_e run(const Json::Value &params, Json::Value &result);

    const std::string &get_name() {
        return m_name;
    }

    virtual ~TrexRpcCommand() {}

protected:

    /**
     * different types of fields
     */
    enum field_type_e {
        FIELD_TYPE_INT,
        FIELD_TYPE_DOUBLE,
        FIELD_TYPE_BOOL,
        FIELD_TYPE_STR,
        FIELD_TYPE_OBJ,
        FILED_TYPE_ARRAY
    };

    /**
     * implemented by the dervied class
     * 
     */
    virtual trex_rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result) = 0;

    /**
     * check param count
     */
    void check_param_count(const Json::Value &params, int expected, Json::Value &result);

    /**
     * check field type
     * 
     */
    //void check_field_type(const Json::Value &field, field_type_e type, Json::Value &result);
    void check_field_type(const Json::Value &parent, const std::string &name, field_type_e type, Json::Value &result);

    /**
     * error generating functions
     * 
     */
    void generate_err(Json::Value &result, const std::string &msg);

    /**
     * translate enum to string
     * 
     */
    const char * type_to_str(field_type_e type);

    /**
     * translate JSON values to string
     * 
     */
    const char * json_type_to_name(const Json::Value &value);

    /* RPC command name */
    std::string m_name;
};

#endif /* __TREX_RPC_CMD_API_H__ */

