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

#include "trex_rpc_cmds.h"
#include <trex_rpc_server_api.h>
#include <trex_stateless_api.h>
#include <trex_rpc_cmds_table.h>

#include <fstream>
#include <iostream>
#include <unistd.h>

#ifndef TREX_RPC_MOCK_SERVER
    #include <../linux_dpdk/version.h>
#endif

using namespace std;

/**
 * ping command
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    result["result"] = "ACK";
    return (TREX_RPC_CMD_OK);
}

/**
 * query command
 */
trex_rpc_cmd_rc_e 
TrexRpcCmdGetCmds::_run(const Json::Value &params, Json::Value &result) {
    vector<string> cmds;

    TrexRpcCommandsTable::get_instance().query(cmds);

    Json::Value test = Json::arrayValue;
    for (auto cmd : cmds) {
        test.append(cmd);
    }

    result["result"] = test;

    return (TREX_RPC_CMD_OK);
}

/**
 * get version
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetVersion::_run(const Json::Value &params, Json::Value &result) {

    Json::Value &section = result["result"];

    #ifndef TREX_RPC_MOCK_SERVER

    section["version"]       = VERSION_BUILD_NUM;
    section["build_date"]    = get_build_date();
    section["build_time"]    = get_build_time();
    section["built_by"]      = VERSION_USER;

    #else

    section["version"]       = "v1.75";
    section["build_date"]    = __DATE__;
    section["build_time"]    = __TIME__;
    section["built_by"]      = "MOCK";

    #endif

    return (TREX_RPC_CMD_OK);
}

/**
 * get the CPU model
 * 
 */
std::string 
TrexRpcCmdGetSysInfo::get_cpu_model() {

    static const string cpu_prefix = "model name";
    std::ifstream cpuinfo("/proc/cpuinfo");

    if (cpuinfo.is_open()) {
        while (cpuinfo.good()) {

            std::string line;
            getline(cpuinfo, line);

            int pos = line.find(cpu_prefix);
            if (pos == string::npos) {
                continue;
            }

            /* trim it */
            int index = cpu_prefix.size() + 1;
            while ( (line[index] == ' ') || (line[index] == ':') ) {
                index++;
            }

            return line.substr(index);
        }
    }

    return "unknown";
}

void
TrexRpcCmdGetSysInfo::get_hostname(string &hostname) {
    char buffer[256];
    buffer[0] = 0;

    gethostname(buffer, sizeof(buffer));

    /* write hostname */
    hostname = buffer;
}

/**
 * get system info
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetSysInfo::_run(const Json::Value &params, Json::Value &result) {
    string hostname;

    TrexStateless & instance = TrexStateless::get_instance();

    Json::Value &section = result["result"];

    get_hostname(hostname);
    section["hostname"]  = hostname;

    section["uptime"] = TrexRpcServer::get_server_uptime();

    /* FIXME: core count */
    section["dp_core_count"] = 1;
    section["core_type"] = get_cpu_model();

    /* ports */
   

    section["port_count"] = instance.get_port_count();

    section["ports"] = Json::arrayValue;

    for (int i = 0; i < instance.get_port_count(); i++) {
        string driver;
        string speed;

        TrexStatelessPort *port = instance.get_port_by_id(i);
        port->get_properties(driver, speed);

        section["ports"][i]["index"]   = i;
        section["ports"][i]["driver"]  = driver;
        section["ports"][i]["speed"]   = speed;

        section["ports"][i]["owner"] = port->get_owner();

        section["ports"][i]["status"] = port->get_state_as_string();

    }

    return (TREX_RPC_CMD_OK);
}

/**
 * returns the current owner of the device
 * 
 * @author imarom (08-Sep-15)
 * 
 * @param params 
 * @param result 
 * 
 * @return trex_rpc_cmd_rc_e 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetOwner::_run(const Json::Value &params, Json::Value &result) {
    Json::Value &section = result["result"];

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);
    section["owner"] = port->get_owner();

    return (TREX_RPC_CMD_OK);
}

/**
 * acquire device
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAcquire::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    const string &new_owner = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);

    /* if not free and not you and not force - fail */
    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    if ( (!port->is_free_to_aquire()) && (port->get_owner() != new_owner) && (!force)) {
        generate_execute_err(result, "port is already taken by '" + port->get_owner() + "'");
    }

    port->set_owner(new_owner);

    result["result"] = port->get_owner_handler();

    return (TREX_RPC_CMD_OK);
}

/**
 * release device
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdRelease::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    if (port->get_state() == TrexStatelessPort::PORT_STATE_TRANSMITTING) {
        generate_execute_err(result, "cannot release a port during transmission");
    }

    port->clear_owner();

    result["result"] = "ACK";

    return (TREX_RPC_CMD_OK);
}

/**
 * get port stats
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortStats::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = TrexStateless::get_instance().get_port_by_id(port_id);

    if (port->get_state() == TrexStatelessPort::PORT_STATE_DOWN) {
        generate_execute_err(result, "cannot get stats - port is down");
    }

    result["result"]["status"] = port->get_state_as_string();

    result["result"]["tx_bps"]         = Json::Value::UInt64(port->get_port_stats().tx_bps);
    result["result"]["tx_pps"]         = Json::Value::UInt64(port->get_port_stats().tx_pps);
    result["result"]["total_tx_pkts"]  = Json::Value::UInt64(port->get_port_stats().total_tx_pkts);
    result["result"]["total_tx_bytes"] = Json::Value::UInt64(port->get_port_stats().total_tx_bytes);

    result["result"]["rx_bps"]         = Json::Value::UInt64(port->get_port_stats().rx_bps);
    result["result"]["rx_pps"]         = Json::Value::UInt64(port->get_port_stats().rx_pps);
    result["result"]["total_rx_pkts"]  = Json::Value::UInt64(port->get_port_stats().total_rx_pkts);
    result["result"]["total_rx_bytes"] = Json::Value::UInt64(port->get_port_stats().total_rx_bytes);

    result["result"]["tx_rx_error"]    = Json::Value::UInt64(port->get_port_stats().tx_rx_errors);

    return (TREX_RPC_CMD_OK);
}

