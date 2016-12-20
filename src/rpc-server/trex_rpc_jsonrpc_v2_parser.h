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

#ifndef __TREX_RPC_JSONRPC_V2_PARSER_H__
#define __TREX_RPC_JSONRPC_V2_PARSER_H__

#include <string>
#include <vector>
#include <json/json.h>

/**
 * JSON RPC V2 parsed object
 * 
 * @author imarom (12-Aug-15)
 */
class TrexJsonRpcV2ParsedObject {
public:

    TrexJsonRpcV2ParsedObject(const Json::Value &msg_id, bool force);
    virtual ~TrexJsonRpcV2ParsedObject() {}

    /**
     * main function to execute the command
     * 
     */
    void execute(Json::Value &response);

protected:

    /**
     * instance private implementation
     */
    virtual void _execute(Json::Value &response) = 0;

    Json::Value   m_msg_id;
    bool          m_respond;
};

/**
 * JSON RPC V2 parser
 * 
 * @author imarom (12-Aug-15)
 */
class TrexJsonRpcV2Parser {

public:

    /**
     * creates a JSON-RPC object from a string
     * 
     * @author imarom (12-Aug-15)
     * 
     * @param msg 
     */
    TrexJsonRpcV2Parser(const std::string &msg);

    /**
     * parses the string to a executable commands vector
     * 
     * @author imarom (12-Aug-15)
     */
    void parse(std::vector<TrexJsonRpcV2ParsedObject *> &commands);

    /**
     * will generate a valid JSON RPC v2 error message with 
     * generic error code and message 
     * 
     * @author imarom (16-Sep-15)
     * 
     */
    static void generate_common_error(Json::Value &json, const std::string &specific_err);

    /**
     * will generate a valid JSON RPC v2 error message with 
     * generic error code and message 
     * 
     * @author imarom (16-Sep-15)
     * 
     */
    static void generate_common_error(std::string &response, const std::string &specific_err);

    /**
     * *tries* to generate a pretty string from JSON 
     * if json_str is not a valid JSON string 
     * it will duplicate the source
     * 
     */
    static std::string pretty_json_str(const std::string &json_str);

private:

    /**
     * handle a single request
     * 
     */
    void parse_single_request(Json::Value &request, std::vector<TrexJsonRpcV2ParsedObject *> &commands);

    std::string m_msg;
};

#endif /* __TREX_RPC_JSONRPC_V2_PARSER_H__ */

