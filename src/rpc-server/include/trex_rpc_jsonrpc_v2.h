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

#ifndef __TREX_RPC_JSONRPC_V2_H__
#define __TREX_RPC_JSONRPC_V2_H__

#include <string>
#include <vector>
#include <json/json.h>

/**
 * JSON RPC V2 command
 * 
 * @author imarom (12-Aug-15)
 */
class TrexJsonRpcV2Command {
public:

    TrexJsonRpcV2Command(const Json::Value &msg_id);

    /**
     * main function to execute the command
     * 
     */
    void execute(Json::Value &response);

protected:

    /**
     * instance private implementation
     * 
     */
    virtual void _execute(Json::Value &response) = 0;

    Json::Value   m_msg_id;
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
    void parse(std::vector<TrexJsonRpcV2Command *> &commands);

private:

    /**
     * handle a single request
     * 
     */
    void parse_single_request(Json::Value &request, std::vector<TrexJsonRpcV2Command *> &commands);

    std::string m_msg;
};

#endif /* __TREX_RPC_JSONRPC_V2_H__ */

