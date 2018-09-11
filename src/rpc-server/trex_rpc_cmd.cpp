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

#include "trex_rpc_cmd_api.h"
#include "trex_rpc_server_api.h"

#include "trex_stx.h"
#include "trex_port.h"

TrexRpcComponent::~TrexRpcComponent() {
    for (auto cmd : m_cmds) {
        delete cmd;
    }
    
    m_cmds.clear();
}


/**
 * method name and params
 * 
 */
TrexRpcCommand::TrexRpcCommand(const std::string &method_name,
                               TrexRpcComponent *component,
                               bool needs_api,
                               bool needs_ownership) {

    m_name            = method_name;
    m_component       = component;
    m_needs_api       = needs_api;
    m_needs_ownership = needs_ownership;
}


trex_rpc_cmd_rc_e 
TrexRpcCommand::run(const Json::Value &params, Json::Value &result) {
    trex_rpc_cmd_rc_e rc;

    /* the internal run can throw a parser error / other error */
    try {

        /* verify API handler is correct (version mismatch) */
        if (m_needs_api && !g_test_override_api) {
            verify_api_handler(params, result);
        }

        /* verify ownership */
        if (m_needs_ownership && !g_test_override_ownership) {
            verify_ownership(params, result);
        }

        /* run the command itself*/
        rc = _run(params, result);

    } catch (TrexRpcCommandException &e) {
        return e.get_rc();
    }

    return (rc);
}

void
TrexRpcCommand::verify_ownership(const Json::Value &params, Json::Value &result) {
    std::string handler = parse_string(params, "handler", result);
    uint8_t port_id = parse_port(params, result);

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    if (port->get_owner().is_free()) {
        generate_execute_err(result, "please acquire the port before modifying port state");
    }

    if (!port->get_owner().verify(handler)) {
        generate_execute_err(result, "port is not owned by you or your current executing session");
    }
}

void TrexRpcCommand::verify_fast_stack(const Json::Value &params, Json::Value &result, uint8_t port_id) {
    if ( !get_stx()->get_rx()->is_active() ) { // we don't have RX core
        return;
    }
    TrexPort *port = get_stx()->get_port_by_id(port_id);
    const bool block = parse_bool(params, "block", result, true);

    if ( block && !port->has_fast_stack() ) {
        generate_parse_err(result, "Current stack \"" + port->get_stack_name() + "\" does not support blocking operations. Switch to another or use newer client");
    }
}

void
TrexRpcCommand::verify_api_handler(const Json::Value &params, Json::Value &result) {
    
    std::string api_handler = parse_string(params, "api_h", result);

    if (api_handler != m_component->get_rpc_api_ver()->get_api_handler()) {
        std::stringstream ss;
        ss << "API verification failed - API handler provided mismatch for class: '" << m_component->get_name() << "'";
        generate_execute_err(result, ss.str());
    }
}

uint8_t 
TrexRpcCommand::parse_port(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_byte(params, "port_id", result);
    validate_port_id(port_id, result);

    return (port_id);
}

void 
TrexRpcCommand::validate_port_id(uint8_t port_id, Json::Value &result) {
    const stx_port_map_t &port_map = get_stx()->get_port_map();
    if ( port_map.find(port_id) == port_map.end() ) {
        std::stringstream ss;
        ss << "invalid port id - should be:";
        for ( auto &port:port_map ) {
            ss << " " << port.first;
        }
        generate_execute_err(result, ss.str());
    }
}

const char *
TrexRpcCommand::type_to_str(field_type_e type) {
    switch (type) {
    case FIELD_TYPE_BYTE:
        return "byte";
    case FIELD_TYPE_UINT16:
        return "uint16";
    case FIELD_TYPE_UINT32:
        return "uint32";
    case FIELD_TYPE_UINT64:
        return "uint64";
    case FIELD_TYPE_BOOL:
        return "bool";
    case FIELD_TYPE_INT:
        return "int";
    case FIELD_TYPE_DOUBLE:
        return "double";
    case FIELD_TYPE_UDOUBLE:
        return "unsigned double";
    case FIELD_TYPE_OBJ:
        return "object";
    case FIELD_TYPE_STR:
        return "string";
    case FIELD_TYPE_ARRAY:
        return "array";

    default:
        return "UNKNOWN";
    }
}

const char *
TrexRpcCommand::json_type_to_name(const Json::Value &value) {

    switch(value.type()) {
    case Json::nullValue:
        return "null";
    case Json::intValue:
        return "int";
    case Json::uintValue:
        return "uint";
    case Json::realValue:
        return "double";
    case Json::stringValue:
        return "string";
    case Json::booleanValue:
        return "boolean";
    case Json::arrayValue:
        return "array";
    case Json::objectValue:
        return "object";

    default:
        return "UNKNOWN";
    }

}

/**
 * for index element (array)
 */
void 
TrexRpcCommand::check_field_type(const Json::Value &parent, int index, field_type_e type, Json::Value &result) {

    /* should never get here without parent being array */
    if (!parent.isArray()) {
        throw TrexRpcException("internal parsing error");
    }

    const Json::Value &field = parent[index];

    std::stringstream ss;
    ss << "array element: " << (index + 1) << " ";
    check_field_type_common(field, ss.str(), type, result);
}

void 
TrexRpcCommand::check_field_type(const Json::Value &parent, const std::string &name, field_type_e type, Json::Value &result) {

    /* is the parent missing ? */
    if (parent == Json::Value::null) {
        generate_parse_err(result, "field '" + name + "' is missing");
    }
    
     /* should never get here without parent being object */
    if (!parent.isObject()) {
        throw TrexRpcException("internal parsing error");
    }

    const Json::Value &field = parent[name];
    check_field_type_common(field, name, type, result);
}

void 
TrexRpcCommand::check_field_type_common(const Json::Value &field, const std::string &name, field_type_e type, Json::Value &result) {
    std::string specific_err;

    /* first check if field exists */
    if (field == Json::Value::null) {
        specific_err = "field '" + name + "' value is null";
        generate_parse_err(result, specific_err);
    }

    bool rc = true;
    specific_err = "is '" + std::string(json_type_to_name(field)) + "', expecting '" + std::string(type_to_str(type)) + "'";

    switch (type) {
    case FIELD_TYPE_BYTE:
        if (!field.isUInt64()) {
            rc = false;
        } else if (field.asUInt64() > 0xFF) {
            specific_err = "has size bigger than uint8.";
            rc = false;
        }
        break;

    case FIELD_TYPE_UINT16:
        if (!field.isUInt64()) {
            rc = false;
        } else if (field.asUInt64() > 0xFFFF) {
            specific_err = "has size bigger than uint16.";
            rc = false;
        }
        break;

    case FIELD_TYPE_UINT32:
        if (!field.isUInt64()) {
            rc = false;
        } else if (field.asUInt64() > 0xFFFFFFFF) {
            specific_err = "has size bigger than uint32.";
            rc = false;
        }
        break;

    case FIELD_TYPE_UINT64:
        if (!field.isUInt64()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_BOOL:
        if (!field.isBool()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_INT:
        if (!field.isInt()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_DOUBLE:
        if (!field.isDouble()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_UDOUBLE:
        if (!field.isDouble()) {
            rc = false;
        } else if (field.asDouble() < 0) {
            specific_err =  "has negative value.";
            rc = false;
        }
        break;

    case FIELD_TYPE_OBJ:
        if (!field.isObject()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_STR:
        if (!field.isString()) {
            rc = false;
        }
        break;

    case FIELD_TYPE_ARRAY:
        if (!field.isArray()) {
            rc = false;
        }
        break;

    default:
        throw TrexRpcException("unhandled type");
        break;

    }
    if (!rc) {
        generate_parse_err(result, "error at offset: " + std::to_string(field.getOffsetStart()) + " - '" + name + "' " + specific_err);
    }

}

void 
TrexRpcCommand::generate_parse_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_PARSE_ERR,msg));
}

void 
TrexRpcCommand::generate_internal_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_INTERNAL_ERR,msg));
}

void 
TrexRpcCommand::generate_execute_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_EXECUTE_ERR,msg));
}

void TrexRpcCommand::generate_try_again(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_TRY_AGAIN_ERR, ""));
}

void TrexRpcCommand::generate_try_again(Json::Value &result) {
    result["specific_err"] = "";
    throw (TrexRpcCommandException(TREX_RPC_CMD_TRY_AGAIN_ERR, ""));
}

void TrexRpcCommand::generate_async_wip(Json::Value &result, uint64_t ticket_id) {
    result["specific_err"] = std::to_string(ticket_id);
    throw (TrexRpcCommandException(TREX_RPC_CMD_ASYNC_WIP_ERR, ""));
}

void TrexRpcCommand::generate_async_no_results(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_ASYNC_NO_RESULTS_ERR, ""));
}

void TrexRpcCommand::generate_async_no_results(Json::Value &result) {
    result["specific_err"] = "";
    throw (TrexRpcCommandException(TREX_RPC_CMD_ASYNC_NO_RESULTS_ERR, ""));
}

/**
 * by default this is off
 */
bool TrexRpcCommand::g_test_override_ownership = false;
bool TrexRpcCommand::g_test_override_api = false;

