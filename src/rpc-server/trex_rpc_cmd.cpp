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
#include <trex_rpc_cmd_api.h>
#include <trex_rpc_server_api.h>
#include <trex_stateless_api.h>
#include <trex_stateless_port.h>

trex_rpc_cmd_rc_e 
TrexRpcCommand::run(const Json::Value &params, Json::Value &result) {
    trex_rpc_cmd_rc_e rc;

    /* the internal run can throw a parser error / other error */
    try {

        check_param_count(params, m_param_count, result);

        if (m_needs_ownership) {
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
TrexRpcCommand::check_param_count(const Json::Value &params, int expected, Json::Value &result) {

    if (params.size() != expected) {
        std::stringstream ss;
        ss << "method expects '" << expected << "' paramteres, '" << params.size() << "' provided";
        generate_parse_err(result, ss.str());
    }
}

void
TrexRpcCommand::verify_ownership(const Json::Value &params, Json::Value &result) {
    std::string handler = parse_string(params, "handler", result);
    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    if (!port->verify_owner_handler(handler)) {
        generate_execute_err(result, "invalid handler provided. please pass the handler given when calling 'acquire' or take ownership");
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
    if (port_id >= TrexStateless::get_instance().get_port_count()) {
        std::stringstream ss;
        ss << "invalid port id - should be between 0 and " << (int)TrexStateless::get_instance().get_port_count() - 1;
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
    case FIELD_TYPE_BOOL:
        return "bool";
    case FIELD_TYPE_INT:
        return "int";
    case FIELD_TYPE_DOUBLE:
        return "double";
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
        return "real";
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

uint8_t  
TrexRpcCommand::parse_byte(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_BYTE, result);
    return parent[name].asUInt();
}

uint8_t  
TrexRpcCommand::parse_byte(const Json::Value &parent, int index, Json::Value &result) {
    check_field_type(parent, index, FIELD_TYPE_BYTE, result);
    return parent[index].asUInt();
}

uint16_t 
TrexRpcCommand::parse_uint16(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_UINT16, result);
    return parent[name].asUInt();
}

uint16_t  
TrexRpcCommand::parse_uint16(const Json::Value &parent, int index, Json::Value &result) {
    check_field_type(parent, index, FIELD_TYPE_UINT16, result);
    return parent[index].asUInt();
}

int
TrexRpcCommand::parse_int(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_INT, result);
    return parent[name].asInt();
}

bool
TrexRpcCommand::parse_bool(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_BOOL, result);
    return parent[name].asBool();
}

double
TrexRpcCommand::parse_double(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_DOUBLE, result);
    return parent[name].asDouble();
}

const std::string
TrexRpcCommand::parse_string(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_STR, result);
    return parent[name].asString();
}

const Json::Value &
TrexRpcCommand::parse_object(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_OBJ, result);
    return parent[name];
}

const Json::Value &
TrexRpcCommand::parse_array(const Json::Value &parent, const std::string &name, Json::Value &result) {
    check_field_type(parent, name, FIELD_TYPE_ARRAY, result);
    return parent[name];
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
     /* should never get here without parent being object */
    if (!parent.isObject()) {
        throw TrexRpcException("internal parsing error");
    }

    const Json::Value &field = parent[name];
    check_field_type_common(field, name, type, result);
}
void 
TrexRpcCommand::check_field_type_common(const Json::Value &field, const std::string &name, field_type_e type, Json::Value &result) {
    std::stringstream ss;

    /* first check if field exists */
    if (field == Json::Value::null) {
        ss << "field '" << name << "' is missing";
        generate_parse_err(result, ss.str());
    }

    bool rc = true;

    switch (type) {
    case FIELD_TYPE_BYTE:
        if ( (!field.isUInt()) || (field.asInt() > 0xFF)) {
            rc = false;
        }
        break;

    case FIELD_TYPE_UINT16:
        if ( (!field.isUInt()) || (field.asInt() > 0xFFFF)) {
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
        ss << "error at offset: " << field.getOffsetStart() << " - '" << name << "' is '" << json_type_to_name(field) << "', expecting '" << type_to_str(type) << "'";
        generate_parse_err(result, ss.str());
    }

}

void 
TrexRpcCommand::generate_parse_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_PARSE_ERR));
}

void 
TrexRpcCommand::generate_internal_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_INTERNAL_ERR));
}

void 
TrexRpcCommand::generate_execute_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
    throw (TrexRpcCommandException(TREX_RPC_CMD_EXECUTE_ERR));
}

