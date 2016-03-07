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
#include <trex_stateless.h>
#include <trex_stateless_port.h>
#include <trex_rpc_cmds_table.h>

#include <internal_api/trex_platform_api.h>

#include <fstream>
#include <iostream>
#include <unistd.h>

#ifdef RTE_DPDK
    #include <../linux_dpdk/version.h>
#endif

using namespace std;

/**
 * ping command
 */
trex_rpc_cmd_rc_e
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    result["result"] = Json::objectValue;
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

    #ifdef RTE_DPDK

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

trex_rpc_cmd_rc_e
TrexRpcCmdGetActivePGIds::_run(const Json::Value &params, Json::Value &result) {
    flow_stat_active_t active_flow_stat;
    flow_stat_active_it_t it;
    int i = 0;

    Json::Value &section = result["result"];
    section["ids"] = Json::arrayValue;

    if (get_stateless_obj()->get_platform_api()->get_active_pgids(active_flow_stat) < 0)
        return TREX_RPC_CMD_INTERNAL_ERR;

    for (it = active_flow_stat.begin(); it != active_flow_stat.end(); it++) {
        section["ids"][i++] = *it;
    }

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

    TrexStateless * main = get_stateless_obj();

    Json::Value &section = result["result"];

    get_hostname(hostname);
    section["hostname"]  = hostname;

    section["uptime"] = TrexRpcServer::get_server_uptime();

    /* FIXME: core count */
    section["dp_core_count"] = main->get_dp_core_count();
    section["core_type"] = get_cpu_model();

    /* ports */


    section["port_count"] = main->get_port_count();

    section["ports"] = Json::arrayValue;

    for (int i = 0; i < main->get_port_count(); i++) {
        TrexPlatformApi::driver_speed_e speed;
        string driver;
        string hw_macaddr;
        string src_macaddr;
        string dst_macaddr;
        string pci_addr;
        int numa;

        TrexStatelessPort *port = main->get_port_by_id(i);
        port->get_properties(driver, speed);
        port->get_macaddr(hw_macaddr, src_macaddr, dst_macaddr);

        port->get_pci_info(pci_addr, numa);

        section["ports"][i]["index"]   = i;

        section["ports"][i]["driver"]       = driver;
        section["ports"][i]["hw_macaddr"]   = hw_macaddr;
        section["ports"][i]["src_macaddr"]  = src_macaddr;
        section["ports"][i]["dst_macaddr"]  = dst_macaddr;

        section["ports"][i]["pci_addr"]     = pci_addr;
        section["ports"][i]["numa"]         = numa;

        section["ports"][i]["rx"]["caps"]      = port->get_rx_caps();
        section["ports"][i]["rx"]["counters"]  = port->get_rx_count_num();


        switch (speed) {
        case TrexPlatformApi::SPEED_1G:
            section["ports"][i]["speed"]   = 1;
            break;

        case TrexPlatformApi::SPEED_10G:
            section["ports"][i]["speed"]   = 10;
            break;

        case TrexPlatformApi::SPEED_40G:
            section["ports"][i]["speed"]   = 40;
            break;

        default:
            /* unknown value */
            section["ports"][i]["speed"]   = 0;
            break;
        }


    }

    return (TREX_RPC_CMD_OK);
}

/**
 * set port commands
 *
 * @author imarom (24-Feb-16)
 *
 * @param params
 * @param result
 *
 * @return trex_rpc_cmd_rc_e
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetPortAttr::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    const Json::Value &attr = parse_object(params, "attr", result);

    /* iterate over all attributes in the dict */
    for (const std::string &name : attr.getMemberNames()) {

        /* handle promiscuous */
        if (name == "promiscuous") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            port->set_promiscuous(enabled);
        }
    }

    result["result"] = Json::objectValue;
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

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    section["owner"] = port->get_owner().get_name();

    return (TREX_RPC_CMD_OK);
}

/**
 * acquire device
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAcquire::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    const string  &new_owner  = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);
    uint32_t session_id = parse_uint32(params, "session_id", result);

    /* if not free and not you and not force - fail */
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->acquire(new_owner, session_id, force);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = port->get_owner().get_handler();

    return (TREX_RPC_CMD_OK);
}

/**
 * release device
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdRelease::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->release();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}

/**
 * get port stats
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortStats::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        port->encode_stats(result["result"]);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}

/**
 * fetch the port status
 *
 * @author imarom (09-Dec-15)
 *
 * @param params
 * @param result
 *
 * @return trex_rpc_cmd_rc_e
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortStatus::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    result["result"]["owner"]         = (port->get_owner().is_free() ? "" : port->get_owner().get_name());
    result["result"]["state"]         = port->get_state_as_string();
    result["result"]["max_stream_id"] = port->get_max_stream_id();

    /* attributes */
    result["result"]["attr"]["promiscuous"]["enabled"] = port->get_promiscuous();

    return (TREX_RPC_CMD_OK);
}

/**
 * publish async data now (fast flush)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcPublishNow::_run(const Json::Value &params, Json::Value &result) {
    TrexStateless *main = get_stateless_obj();

    uint32_t key = parse_uint32(params, "key", result);

    main->get_platform_api()->publish_async_data_now(key);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}
