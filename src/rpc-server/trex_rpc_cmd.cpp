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

trex_rpc_cmd_rc_e 
TrexRpcCommand::run(const Json::Value &params, Json::Value &result) {
    trex_rpc_cmd_rc_e rc;

    /* the internal run can throw a parser error / other error */
    try {
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
        generate_err(result, ss.str());
        throw TrexRpcCommandException(TREX_RPC_CMD_PARAM_COUNT_ERR);
    }
}

const char *
TrexRpcCommand::type_to_str(field_type_e type) {
    switch (type) {
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
    case FILED_TYPE_ARRAY:
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
void 
TrexRpcCommand::check_field_type(const Json::Value &parent, const std::string &name, field_type_e type, Json::Value &result) {
    std::stringstream ss;

    /* check if field exists , does not add the field because its const */
    const Json::Value &field = parent[name];

    /* first check if field exists */
    if (field == Json::Value::null) {
        ss << "field '" << name << "' missing";
        generate_err(result, ss.str());
        throw (TrexRpcCommandException(TREX_RPC_CMD_PARAM_PARSE_ERR));
    }

    bool rc = true;

    switch (type) {
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
    }

    if (!rc) {
        ss << "error at offset: " << field.getOffsetStart() << " - '" << name << "' is '" << json_type_to_name(field) << "', expecting '" << type_to_str(type) << "'";
        generate_err(result, ss.str());
        throw (TrexRpcCommandException(TREX_RPC_CMD_PARAM_PARSE_ERR));
    }

}

void 
TrexRpcCommand::generate_err(Json::Value &result, const std::string &msg) {
    result["specific_err"] = msg;
}
