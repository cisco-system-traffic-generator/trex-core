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

#include <json/json.h>

#include <iostream>

/* dummy command */
class DumymCommand : public TrexJsonRpcV2Command {
public:
    virtual void execute(std::string &response) {
        std::cout << "dummy here\n";
    }
};

/**
 * describes the parser error 
 * 
 */
class JsonRpcError : public TrexJsonRpcV2Command {
public:

    JsonRpcError(const Json::Value &msg_id, int code, const std::string &msg) : m_msg_id(msg_id), m_code(code), m_msg(msg) {

    }

    virtual void execute(std::string &response) {
        Json::Value response_json;
        Json::FastWriter writer;

        response_json["jsonrpc"] = "2.0";
        response_json["id"]      = m_msg_id;

        response_json["error"]["code"]    = m_code;
        response_json["error"]["message"] = m_msg;

        /* encode to string */
        response = writer.write(response_json);
        
    }

private:
    Json::Value   m_msg_id;
    int           m_code;
    std::string   m_msg;
};


TrexJsonRpcV2Parser::TrexJsonRpcV2Parser(const std::string &msg) : m_msg(msg) {

}

TrexJsonRpcV2Command * TrexJsonRpcV2Parser::parse() {
    Json::Reader reader;
    Json::Value  request;

    /* basic JSON parsing */
    bool rc = reader.parse(m_msg, request, false);
    if (!rc) {
        return new JsonRpcError(Json::Value::null, -32700, "Bad JSON Format");
    }

    Json::Value msg_id = request["id"];

    /* check version */
    if (request["jsonrpc"] != "2.0") {
        return new JsonRpcError(msg_id, -32600, "Invalid JSONRPC Version");
    }

    /* check method name */
    std::string method_name = request["method"].asString();
    if (method_name == "") {
        return new JsonRpcError(msg_id, -32600, "Missing Method Name");
    }

    std::cout << "method name: " << method_name << "\n";

    TrexJsonRpcV2Command *command = new DumymCommand();

    return (command);
}

