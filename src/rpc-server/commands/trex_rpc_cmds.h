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

class TrexStream;

/* all the RPC commands decl. goes here */

/******************* test section ************/

/**
 * syntactic sugar for creating a simple command
 */

#define TREX_RPC_CMD_DEFINE_EXTENDED(class_name, cmd_name, param_count, needs_ownership, ext)              \
    class class_name : public TrexRpcCommand {                                                            \
    public:                                                                                               \
        class_name () : TrexRpcCommand(cmd_name, param_count, needs_ownership) {}                         \
    protected:                                                                                            \
        virtual trex_rpc_cmd_rc_e _run(const Json::Value &params, Json::Value &result);                   \
        ext                                                                                               \
    }

#define TREX_RPC_CMD_DEFINE(class_name, cmd_name, param_count, needs_ownership) TREX_RPC_CMD_DEFINE_EXTENDED(class_name, cmd_name, param_count, needs_ownership, ;)

/**
 * test cmds
 */
TREX_RPC_CMD_DEFINE(TrexRpcCmdTestAdd,    "test_add", 2, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdTestSub,    "test_sub", 2, false);

/**
 * general cmds
 */
TREX_RPC_CMD_DEFINE(TrexRpcCmdPing,       "ping",                 0, false);
TREX_RPC_CMD_DEFINE(TrexRpcPublishNow,    "publish_now",          1, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetCmds,    "get_supported_cmds",   0, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetVersion, "get_version",          0, false);

TREX_RPC_CMD_DEFINE_EXTENDED(TrexRpcCmdGetSysInfo, "get_system_info", 0, false,

std::string get_cpu_model();
void get_hostname(std::string &hostname);

);

/**
 * ownership
 */
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetOwner,   "get_owner",       1, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdAcquire,    "acquire",         4, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdRelease,    "release",         1, true);


/**
 * port commands
 */
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetPortStats, "get_port_stats", 1, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetPortStatus, "get_port_status", 1, false);


/**
 * stream cmds
 */
TREX_RPC_CMD_DEFINE(TrexRpcCmdRemoveAllStreams,   "remove_all_streams",   1, true);
TREX_RPC_CMD_DEFINE(TrexRpcCmdRemoveStream,       "remove_stream",        2, true);

TREX_RPC_CMD_DEFINE_EXTENDED(TrexRpcCmdAddStream, "add_stream", 3, true, 

/* extended part */
TrexStream * allocate_new_stream(const Json::Value &section, uint8_t port_id, uint32_t stream_id, Json::Value &result);
void validate_stream(const TrexStream *stream, Json::Value &result);
void parse_vm(const Json::Value &vm, TrexStream *stream, Json::Value &result);
void parse_vm_instr_checksum(const Json::Value &inst, TrexStream *stream, Json::Value &result);
void parse_vm_instr_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result);
void parse_vm_instr_tuple_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result);
void parse_vm_instr_trim_pkt_size(const Json::Value &inst, TrexStream *stream, Json::Value &result);

void parse_vm_instr_write_flow_var(const Json::Value &inst, TrexStream *stream, Json::Value &result);
);


TREX_RPC_CMD_DEFINE(TrexRpcCmdGetStreamList, "get_stream_list", 1, false);
TREX_RPC_CMD_DEFINE(TrexRpcCmdGetAllStreams, "get_all_streams", 2, false);

TREX_RPC_CMD_DEFINE(TrexRpcCmdGetStream, "get_stream", 3, false);



TREX_RPC_CMD_DEFINE(TrexRpcCmdStartTraffic,  "start_traffic", 4, true);
TREX_RPC_CMD_DEFINE(TrexRpcCmdStopTraffic,   "stop_traffic", 1, true);
TREX_RPC_CMD_DEFINE(TrexRpcCmdPauseTraffic,  "pause_traffic", 1, true);
TREX_RPC_CMD_DEFINE(TrexRpcCmdResumeTraffic, "resume_traffic", 1, true);

TREX_RPC_CMD_DEFINE(TrexRpcCmdUpdateTraffic, "update_traffic", 3, true);

TREX_RPC_CMD_DEFINE(TrexRpcCmdValidate, "validate", 2, false);

#endif /* __TREX_RPC_CMD_H__ */
