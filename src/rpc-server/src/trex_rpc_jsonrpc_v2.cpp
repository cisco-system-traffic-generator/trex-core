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
#include <trex_rpc_exception_api.h>
#include <trex_rpc_jsonrpc_v2.h>
#include <trex_rpc_commands.h>

#include <json/json.h>

#include <iostream>

/**
 * error as described in the RFC
 */
enum {
    JSONRPC_V2_ERR_PARSE              = -32700,
    JSONRPC_V2_ERR_INVALID_REQ        = -32600,
    JSONRPC_V2_ERR_METHOD_NOT_FOUND   = -32601,
    JSONRPC_V2_ERR_INVALID_PARAMS     = -32602,
    JSONRPC_V2_ERR_INTERNAL_ERROR     = -32603
};

/* dummy command */
class DumymCommand : public TrexJsonRpcV2Command {
public:
    virtual void _execute(Json::Value &response) {
    }
};

/*************** JSON RPC command base type ************/

TrexJsonRpcV2Command::TrexJsonRpcV2Command(const Json::Value &msg_id) : m_msg_id(msg_id) {

}

void TrexJsonRpcV2Command::execute(Json::Value &response) {
    /* common fields */
    response["jsonrpc"] = "2.0";
    response["id"]      = m_msg_id;

    /* call the underlying implementation to add the reset of the sections */
    _execute(response);
}

/**
 * describes the parser error 
 * 
 */
class JsonRpcError : public TrexJsonRpcV2Command {
public:

    JsonRpcError(const Json::Value &msg_id, int code, const std::string &msg) : TrexJsonRpcV2Command(msg_id), m_code(code), m_msg(msg) {

    }

    virtual void _execute(Json::Value &response) {

        response["error"]["code"]    = m_code;
        response["error"]["message"] = m_msg;
        
    }

private:
    int           m_code;
    std::string   m_msg;
};


TrexJsonRpcV2Parser::TrexJsonRpcV2Parser(const std::string &msg) : m_msg(msg) {

}

void TrexJsonRpcV2Parser::parse(std::vector<TrexJsonRpcV2Command *> &commands) {

    Json::Reader reader;
    Json::Value  request;

    /* basic JSON parsing */
    bool rc = reader.parse(m_msg, request, false);
    if (!rc) {
        commands.push_back(new JsonRpcError(Json::Value::null, JSONRPC_V2_ERR_PARSE, "Bad JSON Format"));
        return;
    }

    /* request can be an array of requests */
    if (request.isArray()) {
        /* handle each command */
        for (auto single_request : request) {
            parse_single_request(single_request, commands);
        }
    } else {
        /* handle single command */
        parse_single_request(request, commands);
    }

  
}


void TrexJsonRpcV2Parser::parse_single_request(Json::Value &request, 
                                               std::vector<TrexJsonRpcV2Command *> &commands) {

    Json::Value msg_id = request["id"];

    /* check version */
    if (request["jsonrpc"] != "2.0") {
        commands.push_back(new JsonRpcError(msg_id, JSONRPC_V2_ERR_INVALID_REQ, "Invalid JSONRPC Version"));
        return;
    }

    /* check method name */
    std::string method_name = request["method"].asString();
    if (method_name == "") {
        commands.push_back(new JsonRpcError(msg_id, JSONRPC_V2_ERR_INVALID_REQ, "Missing Method Name"));
        return;
    }

    TrexRpcCommand * rpc_cmd = TrexRpcCommandsTable::get_instance().lookup(method_name);
    if (!rpc_cmd) {
        commands.push_back(new JsonRpcError(msg_id, JSONRPC_V2_ERR_METHOD_NOT_FOUND, "Method not registered"));
        return;
    }

    /* TODO - add commands */
}

