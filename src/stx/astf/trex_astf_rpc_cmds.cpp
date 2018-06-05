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

/**
 * ASTF specific RPC commands
 */

#include "trex_astf_rpc_cmds.h"


/****************************** commands declerations ******************************/

/**
 * test
 */
TREX_RPC_CMD(TrexRpcCmdAstfStart, "start_stf");


/****************************** commands implementation ******************************/

trex_rpc_cmd_rc_e
TrexRpcCmdAstfStart::_run(const Json::Value &params, Json::Value &result) {

    /* parsing of args  */
    uint32_t duration = parse_uint32(params, "duration", result, 0);
    printf("duration : %d \n",duration);

    Json::Value &section = result["result"];
    section["duration"] = duration;

    if (duration == 10) {
        generate_execute_err(result, "duration is not valid");
    }

    if (duration == 20) {
        return TREX_RPC_CMD_INTERNAL_ERR;
    }

    return (TREX_RPC_CMD_OK);
}


/****************************** component implementation ******************************/

/**
 * ASTF RPC component
 * 
 */
TrexRpcCmdsASTF::TrexRpcCmdsASTF() : TrexRpcComponent("ASTF") {
    
    m_cmds.push_back(new TrexRpcCmdAstfStart(this));
}
