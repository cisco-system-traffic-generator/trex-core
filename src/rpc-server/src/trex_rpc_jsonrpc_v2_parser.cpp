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
#include <trex_rpc_jsonrpc_v2_parser.h>
#include <trex_rpc_cmd_api.h>
#include <trex_rpc_cmds_table.h>

#include <json/json.h>

#include <iostream>

/**
 * error as described in the RFC 
 * http://www.jsonrpc.org/specification 
 */
enum {
    JSONRPC_V2_ERR_PARSE              = -32700,
    JSONRPC_V2_ERR_INVALID_REQ        = -32600,
    JSONRPC_V2_ERR_METHOD_NOT_FOUND   = -32601,
    JSONRPC_V2_ERR_INVALID_PARAMS     = -32602,
    JSONRPC_V2_ERR_INTERNAL_ERROR     = -32603
};


/*************** JSON RPC parsed object base type ************/

TrexJsonRpcV2ParsedObject::TrexJsonRpcV2ParsedObject(const Json::Value &msg_id, bool force = false) : m_msg_id(msg_id) {
    /* if we have msg_id or a force was issued - write resposne */
    m_respond = (msg_id != Json::Value::null) || force;
}

void TrexJsonRpcV2ParsedObject::execute(Json::Value &response) {

    /* common fields */
    if (m_respond) {
        response["jsonrpc"] = "2.0";
        response["id"]      = m_msg_id;
        _execute(response);
    } else {
        Json::Value dummy;
        _execute(dummy);
    }
}

/****************** valid method return value **************/
class JsonRpcMethod : public TrexJsonRpcV2ParsedObject {
public:
    JsonRpcMethod(const Json::Value &msg_id, TrexRpcCommand *cmd, const Json::Value &params) : TrexJsonRpcV2ParsedObject(msg_id), m_cmd(cmd), m_params(params) {

    }

    virtual void _execute(Json::Value &response) {
        Json::Value result;

        TrexRpcCommand::rpc_cmd_rc_e rc = m_cmd->run(m_params, result);

        switch (rc) {
        case TrexRpcCommand::RPC_CMD_OK:
            response["result"] = result["result"];
            break;

        case TrexRpcCommand::RPC_CMD_PARAM_COUNT_ERR:
        case TrexRpcCommand::RPC_CMD_PARAM_PARSE_ERR:
            response["error"]["code"]          = JSONRPC_V2_ERR_INVALID_PARAMS;
            response["error"]["message"]       = "Bad paramters for method";
            response["error"]["specific_err"]  = result["specific_err"];
            break;

        case TrexRpcCommand::RPC_CMD_INTERNAL_ERR:
            response["error"]["code"]          = JSONRPC_V2_ERR_INTERNAL_ERROR;
            response["error"]["message"]       = "Internal Server Error";
            response["error"]["specific_err"]  = result["specific_err"];
            break;
        }

    }

private:
    TrexRpcCommand *m_cmd;
    Json::Value m_params;
};

/******************* RPC error **************/

/**
 * describes the parser error 
 * 
 */
class JsonRpcError : public TrexJsonRpcV2ParsedObject {
public:

    JsonRpcError(const Json::Value &msg_id, int code, const std::string &msg, bool force = false) : TrexJsonRpcV2ParsedObject(msg_id, force), m_code(code), m_msg(msg) {

    }

    virtual void _execute(Json::Value &response) {
        response["error"]["code"]    = m_code;
        response["error"]["message"] = m_msg;
    }

private:
    int           m_code;
    std::string   m_msg;
};


/************** JSON RPC V2 parser implementation *************/

TrexJsonRpcV2Parser::TrexJsonRpcV2Parser(const std::string &msg) : m_msg(msg) {

}

/**
 * parse a batch of commands
 * 
 * @author imarom (17-Aug-15)
 * 
 * @param commands 
 */
void TrexJsonRpcV2Parser::parse(std::vector<TrexJsonRpcV2ParsedObject *> &commands) {

    Json::Reader reader;
    Json::Value  request;

    /* basic JSON parsing */
    bool rc = reader.parse(m_msg, request, false);
    if (!rc) {
        commands.push_back(new JsonRpcError(Json::Value::null, JSONRPC_V2_ERR_PARSE, "Bad JSON Format", true));
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
                                               std::vector<TrexJsonRpcV2ParsedObject *> &commands) {

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

    /* lookup the method in the DB */
    TrexRpcCommand * rpc_cmd = TrexRpcCommandsTable::get_instance().lookup(method_name);
    if (!rpc_cmd) {
        commands.push_back(new JsonRpcError(msg_id, JSONRPC_V2_ERR_METHOD_NOT_FOUND, "Method not registered"));
        return;
    }

    /* create a method object */
    commands.push_back(new JsonRpcMethod(msg_id, rpc_cmd, request["params"]));
}

