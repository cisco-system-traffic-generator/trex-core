/*
  Hanoch Haim
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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

#ifndef __TREX_RX_RPC_TUNNEL_H
#define __TREX_RX_RPC_TUNNEL_H

#include <stdlib.h>
#include <json/json.h>
#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <vector>
#include <functional>
#include <stdio.h>
#include "rpc-server/trex_rpc_cmd_api.h"

using namespace std;


typedef std::function<trex_rpc_cmd_rc_e(const Json::Value &params, Json::Value &result)> rpc_method_cb_t;

typedef std::map<const string,rpc_method_cb_t> int_rpc_map_t;
typedef std::map<const string,rpc_method_cb_t>::iterator int_rpc_map_it_t;
typedef std::pair<const string,rpc_method_cb_t> int_rpc_map_pair_t;

class CRpcDispatch {
public:
    virtual ~CRpcDispatch();
    void register_func(const string & json_name,rpc_method_cb_t cb);
    rpc_method_cb_t find_func_by_name(const string & json_name);
private:
    int_rpc_map_t m_map;
};


/* this class derive from  TrexRpcCommand  to use the template function to parse json in the command */
class CRpcTunnelBatch : public TrexRpcCommand {

public:
    CRpcTunnelBatch():TrexRpcCommand("dummy",NULL,false,false){
    }
    virtual ~CRpcTunnelBatch(){
    }
    void register_func(const string & json_name,rpc_method_cb_t cb);

    trex_rpc_cmd_rc_e run_batch(const Json::Value &commands,Json::Value &results);

protected:
    /* dummy */
    virtual trex_rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result);
    /* callback, call by batch to update number of commands */
    virtual void update_cmd_count(uint32_t total_commands,
                                  uint32_t err_commands);

private:
    CRpcDispatch  m_rpc_dispatch;
};



#endif

