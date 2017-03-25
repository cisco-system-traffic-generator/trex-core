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

#include "trex_api_class.h"

/**
 * describe different types of rc for run()
 */
typedef enum trex_rpc_cmd_rc_ {
    TREX_RPC_CMD_OK,
    TREX_RPC_CMD_PARSE_ERR,
    TREX_RPC_CMD_EXECUTE_ERR,
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
    TrexRpcCommand(const std::string &method_name,
                   int param_count,
                   bool needs_ownership,
                   APIClass::type_e type);

    /**
     * entry point for executing RPC command
     * 
     */
    trex_rpc_cmd_rc_e run(const Json::Value &params, Json::Value &result);

    const std::string &get_name() {
        return m_name;
    }

    /**
     * on test we enable this override
     * 
     * 
     * @param enable 
     */
    static void test_set_override_ownership(bool enable) {
        g_test_override_ownership = enable;
    }

    static void test_set_override_api(bool enable) {
        g_test_override_api = enable;
    }

    virtual ~TrexRpcCommand() {}

protected:
    static const int PARAM_COUNT_IGNORE = -1;

    /**
     * different types of fields
     */
    enum field_type_e {
        FIELD_TYPE_BYTE,
        FIELD_TYPE_UINT16,
        FIELD_TYPE_UINT32,
        FIELD_TYPE_UINT64,
        FIELD_TYPE_INT,
        FIELD_TYPE_DOUBLE,
        FIELD_TYPE_UDOUBLE,
        FIELD_TYPE_BOOL,
        FIELD_TYPE_STR,
        FIELD_TYPE_OBJ,
        FIELD_TYPE_ARRAY
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
     * verify API handler
     * 
     */
    void verify_api_handler(const Json::Value &params, Json::Value &result);

    /**
     * verify ownership
     * 
     */
    void verify_ownership(const Json::Value &params, Json::Value &result);

    
    /**
     * validate port id
     * 
     */
    void validate_port_id(uint8_t port_id, Json::Value &result);

    /**
     * parse functions
     * 
     */
    template<typename T> uint8_t parse_byte(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_BYTE, result);
        return parent[param].asUInt();
    }

    template<typename T> uint16_t parse_uint16(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_UINT16, result);
        return parent[param].asUInt();
    }

    template<typename T> uint32_t parse_uint32(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_UINT32, result);
        return parent[param].asUInt();
    }

    template<typename T> uint64_t parse_uint64(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_UINT64, result);
        return parent[param].asUInt64();
    }

    template<typename T> int parse_int(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_INT, result);
        return parent[param].asInt();
    }

    template<typename T> double parse_double(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_DOUBLE, result);
        return parent[param].asDouble();
    }

    template<typename T> double parse_udouble(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_UDOUBLE, result);
        return parent[param].asDouble();
    }

    template<typename T> bool parse_bool(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_BOOL, result);
        return parent[param].asBool();
    }

    template<typename T> const std::string  parse_string(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_STR, result);
        return parent[param].asString();
    }

    template<typename T> const Json::Value & parse_object(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_OBJ, result);
        return parent[param];
    }

    template<typename T> const Json::Value & parse_array(const Json::Value &parent, const T &param, Json::Value &result) {
        check_field_type(parent, param, FIELD_TYPE_ARRAY, result);
        return parent[param];
    }

   
    /**
     * parse with defaults
     */
    template<typename T> uint8_t parse_byte(const Json::Value &parent, const T &param, Json::Value &result, uint8_t def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_byte(parent, param, result);
    }

     template<typename T> uint16_t parse_uint16(const Json::Value &parent, const T &param, Json::Value &result, uint16_t def) {
         /* if not exists - default */
         if (parent[param] == Json::Value::null) {
             return def;
         }
         return parse_uint16(parent, param, result);
    }

    template<typename T> uint32_t parse_uint32(const Json::Value &parent, const T &param, Json::Value &result, uint32_t def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_uint32(parent, param, result);
    }

    template<typename T> uint64_t parse_uint64(const Json::Value &parent, const T &param, Json::Value &result, uint64_t def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_uint64(parent, param, result);
    }

    template<typename T> int parse_int(const Json::Value &parent, const T &param, Json::Value &result, int def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_int(parent, param, result);
    }

    template<typename T> double parse_double(const Json::Value &parent, const T &param, Json::Value &result, double def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_double(parent, param, result);
    }

    template<typename T> double parse_udouble(const Json::Value &parent, const T &param, Json::Value &result, double def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            if (def < 0) {
                std::stringstream ss;
                ss << "default value of '" << param << "' is negative (please report)";
                generate_parse_err(result, ss.str());
            } else {
                return def;
            }
        }
        return parse_udouble(parent, param, result);
    }

    template<typename T> bool parse_bool(const Json::Value &parent, const T &param, Json::Value &result, bool def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_bool(parent, param, result);
    }

    template<typename T> const std::string  parse_string(const Json::Value &parent, const T &param, Json::Value &result, const std::string &def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_string(parent, param, result);
    }

    template<typename T> const Json::Value & parse_object(const Json::Value &parent, const T &param, Json::Value &result, const Json::Value &def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_object(parent, param, result);
    }

    template<typename T> const Json::Value & parse_array(const Json::Value &parent, const T &param, Json::Value &result, const Json::Value &def) {
        /* if not exists - default */
        if (parent[param] == Json::Value::null) {
            return def;
        }
        return parse_array(parent, param, result);
    }

    /* shortcut for parsing port id */
    uint8_t parse_port(const Json::Value &params, Json::Value &result);

    /**
     * parse a field from choices 
     * 
     */
    template<typename T> T parse_choice(const Json::Value &params, const std::string &name, const std::initializer_list<T> choices, Json::Value &result) {
        const Json::Value &field = params[name];

        if (field == Json::Value::null) {
            std::stringstream ss;
            ss << "field '" << name << "' is missing";
            generate_parse_err(result, ss.str());
        }

        for (auto x : choices) {
            if (field == x) {
                return (x);
            }
        }

        std::stringstream ss;

        ss << "field '" << name << "' can only be one of [";
        for (auto x : choices) {
            ss << "'" << x << "' ,";
        }

        std::string s = ss.str();
        s.pop_back();
        s.pop_back();
        s += "]";
        generate_parse_err(result, s);

        /* dummy return value - does not matter, the above will throw exception */
        return (*choices.begin());
    }

    /**
     * check field type
     * 
     */
    void check_field_type(const Json::Value &parent, const std::string &name, field_type_e type, Json::Value &result);
    void check_field_type(const Json::Value &parent, int index, field_type_e type, Json::Value &result);
    void check_field_type_common(const Json::Value &field, const std::string &name, field_type_e type, Json::Value &result);

    /**
     * error generating functions
     * 
     */
    void generate_parse_err(Json::Value &result, const std::string &msg);


    /**
     * method execute error
     * 
     */
    void generate_execute_err(Json::Value &result, const std::string &msg);

    /**
     * internal error
     * 
     */
    void generate_internal_err(Json::Value &result, const std::string &msg);


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
    std::string        m_name;
    int                m_param_count;
    bool               m_needs_ownership;
    std::string        m_api_handler;
    APIClass::type_e   m_api_type; 
    static bool        g_test_override_ownership;
    static bool        g_test_override_api;
};

#endif /* __TREX_RPC_CMD_API_H__ */

