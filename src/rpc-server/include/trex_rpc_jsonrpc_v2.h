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

/**
 * JSON RPC V2 command
 * 
 * @author imarom (12-Aug-15)
 */
class TrexJsonRpcV2Command {
public:
    virtual void execute(std::string &response) = 0;
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
     * parses the string to a executable command
     * 
     * @author imarom (12-Aug-15)
     */
    TrexJsonRpcV2Command * parse();

private:
    std::string m_msg;
};

#endif /* __TREX_RPC_JSONRPC_V2_H__ */

