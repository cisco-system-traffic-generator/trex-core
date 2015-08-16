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

#ifndef __TREX_RPC_CMD_H__
#define __TREX_RPC_CMD_H__

#include <trex_rpc_cmd_api.h>
#include <json/json.h>

/* all the RPC commands decl. goes here */

/******************* test section ************/
class TestRpcAddMethod : public TrexRpcCommand {
public:
     TestRpcAddMethod();
protected:
     virtual rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result);
};

class TestRpcSubMethod : public TrexRpcCommand {
public:
     TestRpcSubMethod();
protected:
     virtual rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result);
};

/**************** test section end *************/
#endif /* __TREX_RPC_CMD_H__ */
