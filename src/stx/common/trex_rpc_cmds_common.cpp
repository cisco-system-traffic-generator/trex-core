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
 * holds all the common RPC commands used by stateless and 
 * advanced stateful
 */

#include <fstream>
#include <iostream>
#include <unistd.h>

#include "trex_rpc_cmds_common.h"

#include "os_time.h"
#include "common/captureFile.h"

#include "rpc-server/trex_rpc_cmds_table.h"
#include "rpc-server/trex_rpc_server_api.h"

#include "internal_api/trex_platform_api.h"

#include "trex_stx.h"
#include "trex_pkt.h"
#include "trex_capture.h"
#include "trex_messaging.h"

/* this is a hack for get_port_status */
#include "stl/trex_stl_port.h"

#ifdef RTE_DPDK
    #include <../linux_dpdk/version.h>
#endif

using namespace std;
using namespace placeholders;

/****************************** commands declerations ******************************/

/**
 * API sync
 */
TREX_RPC_CMD_NOAPI(TrexRpcCmdGetVersion,   "get_version");
TREX_RPC_CMD_NOAPI(TrexRpcCmdAPISync,      "api_sync");
TREX_RPC_CMD_NOAPI(TrexRpcCmdAPISyncV2,    "api_sync_v2");


/**
 * ownership
 */
TREX_RPC_CMD(TrexRpcCmdAcquire,          "acquire");
TREX_RPC_CMD_OWNED(TrexRpcCmdRelease,    "release");   


/**
 * port status
 */
TREX_RPC_CMD(TrexRpcCmdGetPortStatus,    "get_port_status");

/**
 * ping - the most basic command
 */
TREX_RPC_CMD_NOAPI(TrexRpcCmdPing, "ping");

/**
 * general cmds
 */
TREX_RPC_CMD(TrexRpcCmdShutdown,              "shutdown");
TREX_RPC_CMD(TrexRpcCmdGetCmds,               "get_supported_cmds");
TREX_RPC_CMD(TrexRpcCmdGetUtilization,        "get_utilization");
TREX_RPC_CMD(TrexRpcPublishNow,               "publish_now"); 
TREX_RPC_CMD(TrexRpcCmdGetAsyncResults,       "get_async_results");
TREX_RPC_CMD(TrexRpcCmdCancelAsyncTask,       "cancel_async_task");

TREX_RPC_CMD_EXT(TrexRpcCmdGetSysInfo,        "get_system_info",

string get_cpu_model();
void get_hostname(string &hostname);

);

TREX_RPC_CMD(TrexRpcCmdGetGlobalStats,        "get_global_stats"); 


/**
 * port commands
 */
TREX_RPC_CMD(TrexRpcCmdGetPortStats,          "get_port_stats");
TREX_RPC_CMD(TrexRpcCmdGetPortXStatsValues,   "get_port_xstats_values");
TREX_RPC_CMD(TrexRpcCmdGetPortXStatsNames,    "get_port_xstats_names");
TREX_RPC_CMD(TrexRpcCmdSetPortAttr,           "set_port_attr");
TREX_RPC_CMD_EXT(TrexRpcCmdSetL2,             "set_l2",
    void process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result);
);
TREX_RPC_CMD_EXT(TrexRpcCmdSetL3,             "set_l3",
    void process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result);
);
TREX_RPC_CMD_EXT(TrexRpcCmdConfIPv6,          "conf_ipv6",
    void process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result);
);
TREX_RPC_CMD(TrexRpcCmdStartCapturePort,      "start_capture_port");
TREX_RPC_CMD(TrexRpcCmdStopCapturePort,       "stop_capture_port");
TREX_RPC_CMD(TrexRpcCmdSetCapturePortBPF,     "set_capture_port_bpf");

TREX_RPC_CMD_EXT(TrexRpcCmdSetVLAN,           "set_vlan",
    void validate_vlan(uint16_t vlan, Json::Value &result);
    void process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result);;
);


/**
 * query for ownership
 */
TREX_RPC_CMD(TrexRpcCmdGetOwner,   "get_owner");


/**
 * service mode & capture
 */
TREX_RPC_CMD(TrexRpcCmdSetServiceMode, "service");

TREX_RPC_CMD_EXT(TrexRpcCmdCapture,  "capture",
    void parse_cmd_start(const Json::Value &msg, Json::Value &result);
    void parse_cmd_stop(const Json::Value &msg, Json::Value &result);
    void parse_cmd_status(const Json::Value &msg, Json::Value &result);
    void parse_cmd_fetch(const Json::Value &msg, Json::Value &result);
    void parse_cmd_remove(const Json::Value &params, Json::Value &result);
);


/**
 * rx features
 */
TREX_RPC_CMD_EXT(TrexRpcCmdSetRxFeature, "set_rx_feature",
    void parse_queue_msg(const Json::Value &msg,  TrexPort *port, Json::Value &result);
    void parse_server_msg(const Json::Value &msg, TrexPort *port, Json::Value &result);

);

TREX_RPC_CMD(TrexRpcCmdGetRxQueuePkts, "get_rx_queue_pkts");

/**
 * push packets through RX core
 */
TREX_RPC_CMD(TrexRpcCmdTXPkts, "push_pkts");

/****************************** commands implementation ******************************/

/**
 * old API sync command (for backward compatability of GUI)
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAPISync::_run(const Json::Value &params, Json::Value &result) {
    const Json::Value &api_vers = parse_array(params, "api_vers", result);

    if (api_vers.size() != 1) {
        generate_parse_err(result, "'api_vers' should be a list of size 1");
    }

    const Json::Value &api_ver = api_vers[0];
    const string type = parse_choice(api_ver, "type",{"core"}, result);
    int major = parse_int(api_ver, "major", result);
    int minor = parse_int(api_ver, "minor", result);

    Json::Value api_ver_rc = Json::arrayValue;
    Json::Value single_rc;

    single_rc["type"] = type;

    try {
        single_rc["api_h"] = m_component->get_rpc_api_ver()->verify("STL", major, minor);

    } catch (const TrexRpcException &e) {
        generate_execute_err(result, e.what());
    }

    api_ver_rc.append(single_rc);


    result["result"]["api_vers"] = api_ver_rc;

    return(TREX_RPC_CMD_OK);
}


/**
 * API sync command V2
 *  
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAPISyncV2::_run(const Json::Value &params, Json::Value &result) {
    string name     = parse_string(params, "name", result);
    int major       = parse_int(params, "major", result);
    int minor       = parse_int(params, "minor", result);

    try {
        result["result"]["api_h"] = m_component->get_rpc_api_ver()->verify(name, major, minor);

    } catch (const TrexRpcException &e) {
        generate_execute_err(result, e.what());
    }

    return(TREX_RPC_CMD_OK);
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
    verify_fast_stack(params, result, port_id);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    Json::Value &res = result["result"];

    if ( port->is_rx_running_cfg_tasks() ) {
        generate_try_again(result);
    }

    res["owner"]         = (port->get_owner().is_free() ? "" : port->get_owner().get_name());
    res["state"]         = port->get_state_as_string();
    res["service"]       = port->is_service_mode_on();
    
    /* yes, this is an ugly hack... for now */
    TrexStatelessPort *stl_port = dynamic_cast<TrexStatelessPort *>(port);
    if (stl_port) {
        result["result"]["max_stream_id"] = stl_port->get_max_stream_id();
    }

    // promisc, speed etc.
    get_platform_api().getPortAttrObj(port_id)->to_json(res["attr"]);

    /* RX info */
    try {
        port->port_attr_to_json(res["attr"]); // mac/vlan/ip etc.
        port->rx_features_to_json(res["rx_info"]);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}




/**
 * acquire device
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAcquire::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    const std::string  &new_owner  = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);
    uint32_t session_id = parse_uint32(params, "session_id", result);

    /* if not free and not you and not force - fail */
    TrexPort *port = get_stx()->get_port_by_id(port_id);

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

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    try {
        port->release();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;

    return (TREX_RPC_CMD_OK);
}



/**
 * ping command
 */
trex_rpc_cmd_rc_e
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    result["result"]["ts"] = now_sec();
    return (TREX_RPC_CMD_OK);
}


/**
 * publish async data now (fast flush)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcPublishNow::_run(const Json::Value &params, Json::Value &result) {

    uint32_t key  = parse_uint32(params, "key", result);
    bool baseline = parse_bool(params, "baseline", result);

    get_platform_api().publish_async_data_now(key, baseline);

    result["result"] = Json::objectValue;
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
    section["mode"]          = TrexRpcCommandsTable::get_instance().get_config_name();
    
    #else

    section["version"]       = "v1.75";
    section["build_date"]    = __DATE__;
    section["build_time"]    = __TIME__;
    section["built_by"]      = "MOCK";
    section["mode"]          = "MOCK";
    
    #endif

    return (TREX_RPC_CMD_OK);
}


/**
 * get the CPU model
 *
 */
string
TrexRpcCmdGetSysInfo::get_cpu_model() {

    static const string cpu_prefix = "model name";
    ifstream cpuinfo("/proc/cpuinfo");

    if (cpuinfo.is_open()) {
        while (cpuinfo.good()) {

            string line;
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

    TrexPlatformApi &api = get_platform_api();
    
    Json::Value &section = result["result"];

    get_hostname(hostname);
    section["hostname"]  = hostname;

    section["uptime"] = TrexRpcServer::get_server_uptime();

    /* FIXME: core count */
    section["dp_core_count"] = api.get_dp_core_count();
    section["dp_core_count_per_port"] = api.get_dp_core_count() / (api.get_port_count() / 2);
    section["core_type"] = get_cpu_model();

    /* ports */
    const stx_port_map_t &stx_port_map = get_stx()->get_port_map();
    section["port_count"] = get_stx()->get_port_count();
    section["ports"] = Json::arrayValue;

    for (auto &port : stx_port_map) {
        int i = port.first;
        TrexPlatformApi::intf_info_st port_info;
        supp_speeds_t supp_speeds;
        uint16_t rx_count_num;
        uint16_t rx_caps;
        uint16_t ip_id_base;
        
        /* query the port info through the platform API */
        api.get_port_info(i, port_info);

        get_platform_api().getPortAttrObj(i)->get_supported_speeds(supp_speeds);
        Json::Value port_json;

        port_json["index"]   = i;

        port_json["driver"]       = port_info.driver_name;
        port_json["pci_addr"]     = port_info.pci_addr;
        port_json["numa"]         = port_info.numa_node;
        port_json["hw_mac"]       = utl_macaddr_to_str(port_info.hw_macaddr);

        port_json["description"]  = api.getPortAttrObj(i)->get_description();
        
        api.get_port_stat_info(i, rx_count_num, rx_caps, ip_id_base);
        
        port_json["rx"]["caps"]      = Json::arrayValue;
        
        if (rx_caps & TrexPlatformApi::IF_STAT_IPV4_ID) {
            port_json["rx"]["caps"].append("flow_stats");
        }
        if (rx_caps & TrexPlatformApi::IF_STAT_PAYLOAD) {
            port_json["rx"]["caps"].append("latency");
        }
        if (rx_caps & TrexPlatformApi::IF_STAT_RX_BYTES_COUNT) {
            port_json["rx"]["caps"].append("rx_bytes");
        }
        
        port_json["rx"]["counters"]     = rx_count_num;
        port_json["is_fc_supported"]    = api.getPortAttrObj(i)->is_fc_change_supported();
        port_json["is_led_supported"]   = api.getPortAttrObj(i)->is_led_change_supported();
        port_json["is_link_supported"]  = api.getPortAttrObj(i)->is_link_change_supported();
        port_json["is_prom_supported"]  = api.getPortAttrObj(i)->is_prom_change_supported();
        port_json["is_virtual"]         = api.getPortAttrObj(i)->is_virtual();
        
        port_json["supp_speeds"] = Json::arrayValue;
        for (int speed_id=0; speed_id<supp_speeds.size(); speed_id++) {
            port_json["supp_speeds"].append(supp_speeds[speed_id]);
        }
        section["ports"].append(port_json);

    }

    return (TREX_RPC_CMD_OK);
}


/**
 * get global stats
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetGlobalStats::_run(const Json::Value &params, Json::Value &result) {
    get_platform_api().global_stats_to_json(result["result"]);
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
 * shutdown command
 */
trex_rpc_cmd_rc_e
TrexRpcCmdShutdown::_run(const Json::Value &params, Json::Value &result) {

    const string &user = parse_string(params, "user", result);
    bool force = parse_bool(params, "force", result);

    /* verify every port is either free or owned by the issuer */
    for (auto &port : get_stx()->get_port_map()) {
        TrexPortOwner &owner = port.second->get_owner();
        if ( (!owner.is_free()) && (!owner.is_owned_by(user)) && !force) {
            stringstream ss;
            ss << "port " << int(port.first) << " is owned by '" << owner.get_name() << "' - specify 'force' for override";
            generate_execute_err(result, ss.str());
        }
    }

    /* signal that we got a shutdown request */
    get_platform_api().mark_for_shutdown();

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}



// get utilization of CPU per thread with up to 20 latest values + mbufs per socket
trex_rpc_cmd_rc_e
TrexRpcCmdGetUtilization::_run(const Json::Value &params, Json::Value &result) {
    cpu_util_full_t cpu_util_full;

    Json::Value &section = result["result"];

    if (get_platform_api().get_mbuf_util(section) != 0) {
        return TREX_RPC_CMD_INTERNAL_ERR;
    }

    if (get_platform_api().get_cpu_util_full(cpu_util_full) != 0) {
        return TREX_RPC_CMD_INTERNAL_ERR;
    }

    for (int thread_id = 0; thread_id < cpu_util_full.size(); thread_id++) {

        /* history */
        for (int history_id = 0; history_id < cpu_util_full[thread_id].m_history.size(); history_id++) {
            section["cpu"][thread_id]["history"].append(cpu_util_full[thread_id].m_history[history_id]);
        }

        /* ports */
        section["cpu"][thread_id]["ports"] = Json::arrayValue;
        section["cpu"][thread_id]["ports"].append(cpu_util_full[thread_id].m_port1);
        section["cpu"][thread_id]["ports"].append(cpu_util_full[thread_id].m_port2);
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

    const Json::Value &attr = parse_object(params, "attr", result);

    int ret = 0;

    /* iterate over all attributes in the dict */
    for (const string &name : attr.getMemberNames()) {

        if (name == "promiscuous") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            ret = get_platform_api().getPortAttrObj(port_id)->set_promiscuous(enabled);
        }

        else if (name == "multicast") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            ret = get_platform_api().getPortAttrObj(port_id)->set_multicast(enabled);
        }

        else if (name == "link_status") {
            bool up = parse_bool(attr[name], "up", result);
            ret = get_platform_api().getPortAttrObj(port_id)->set_link_up(up);
        }

        else if (name == "led_status") {
            bool on = parse_bool(attr[name], "on", result);
            ret = get_platform_api().getPortAttrObj(port_id)->set_led(on);
        }

        else if (name == "flow_ctrl_mode") {
            int mode = parse_int(attr[name], "mode", result);
            ret = get_platform_api().getPortAttrObj(port_id)->set_flow_ctrl(mode);
        }

        /* unknown attribute */
        else {
            generate_execute_err(result, "unknown attribute type: '" + name + "'");
            break;
        }

        /* check error code */
        if ( ret == -ENOTSUP ) {
            generate_execute_err(result, "Error applying " + name + ": operation is not supported for this NIC.");
        } else if (ret) {
            generate_execute_err(result, "Error applying " + name + " attribute, return value: " + to_string(ret));
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

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    section["owner"] = port->get_owner().get_name();

    return (TREX_RPC_CMD_OK);
}




/**
 * get port extended stats names (keys of dict)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortXStatsNames::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    xstats_names_t xstats_names;

    int ret = get_platform_api().get_xstats_names(port_id, xstats_names);
    if (ret < 0) {
        if ( ret == -ENOTSUP ) {
            generate_execute_err(result, "Operation not supported");
        }
        else if (ret) {
            generate_execute_err(result, "Operation failed, error code: " + to_string(ret));
        }
    } else {
        for (int i=0; i<xstats_names.size(); i++) {
            result["result"]["xstats_names"].append(xstats_names[i]);
        }
    }

    return (TREX_RPC_CMD_OK);
}


/**
 * get port extended stats (values of dict)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortXStatsValues::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    xstats_values_t xstats_values;

    int ret = get_platform_api().get_xstats_values(port_id, xstats_values);
    if (ret < 0) {
        if ( ret == -ENOTSUP ) {
            generate_execute_err(result, "Operation not supported");
        }
        else if (ret) {
            generate_execute_err(result, "Operation failed, error code: " + to_string(ret));
        }
    } else {
        for (int i=0; i<xstats_values.size(); i++) {
            result["result"]["xstats_values"].append((Json::Value::UInt64) xstats_values[i]);
        }
    }

    return (TREX_RPC_CMD_OK);
}



/**
 * get port stats
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortStats::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    try {
        port->encode_stats(result["result"]);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    return (TREX_RPC_CMD_OK);
}



/**
 * set on/off RX software receive mode
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetRxFeature::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);
    TrexPort *port  = get_stx()->get_port_by_id(port_id);

    /* decide which feature is being set */
    const string type = parse_choice(params, "type", {"queue", "server"}, result);

    if (type == "queue") {
        parse_queue_msg(params, port, result);
    } else if (type == "server") {
        parse_server_msg(params, port, result);
    } else {
        assert(0);
    }

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
   
}

void 
TrexRpcCmdSetRxFeature::parse_queue_msg(const Json::Value &msg, TrexPort *port, Json::Value &result) {
    bool enabled = parse_bool(msg, "enabled", result);

    if (enabled) {
        
        if (!port->is_service_mode_on()) {
            generate_execute_err(result, "setting RX queue is only available under service mode");
        }

        uint64_t size = parse_uint32(msg, "size", result);

        if (size == 0) {
            generate_parse_err(result, "queue size cannot be zero");
        }

        try {
            port->start_rx_queue(size);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }

    } else {

        try {
            port->stop_rx_queue();
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }

    }

}

void 
TrexRpcCmdSetRxFeature::parse_server_msg(const Json::Value &msg, TrexPort *port, Json::Value &result) {
}


trex_rpc_cmd_rc_e
TrexRpcCmdGetRxQueuePkts::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    if (!port->is_service_mode_on()) {
        generate_execute_err(result, "fetching RX queue packets is only available under service mode");
    }

    
    try {
        const TrexPktBuffer *pkt_buffer = port->get_rx_queue_pkts();
        if (pkt_buffer) {
            result["result"]["pkts"] = pkt_buffer->to_json();
            delete pkt_buffer;
            
        } else {
            result["result"]["pkts"] = Json::arrayValue;
        }

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    
    return (TREX_RPC_CMD_OK);
}


/**
 * configures a port in L2 mode
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetL2::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    verify_fast_stack(params, result, port_id);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    const string dst_mac_str  = parse_string(params, "dst_mac", result);

    uint8_t dst_mac[6];
    if (!utl_str_to_macaddr(dst_mac_str, dst_mac)) {
        stringstream ss;
        ss << "'invalid MAC address: '" << dst_mac_str << "'";
        generate_parse_err(result, ss.str());
    }
    string dst_mac_buf((char*)dst_mac, 6);

    try {
        port->set_l2_mode_async(dst_mac_buf);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    uint64_t ticket_id = port->run_rx_cfg_tasks_async();
    process_results(ticket_id, params, result);

    return (TREX_RPC_CMD_OK);
}

void TrexRpcCmdSetL2::process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    stack_result_t results;
    bool found = get_stx()->get_port_by_id(port_id)->get_rx_cfg_tasks_results(ticket_id, results);
    if ( !found ) {
        generate_async_no_results(result, "Could not find L2 results, probably timeout on aging.");
    }
    if ( !results.is_ready ) {
        async_ticket_task_t task;
        task.result_func = bind(&TrexRpcCmdSetL2::process_results, this, ticket_id, params, _1);
        task.cancel_func = bind(&TrexPort::cancel_rx_cfg_tasks, get_stx()->get_port_by_id(port_id));
        get_stx()->add_task_by_ticket(ticket_id, task);
        generate_async_wip(result, ticket_id);
    }
    if ( results.err_per_mac.size() ) {
        assert(results.err_per_mac.size() == 1);
        generate_execute_err(result, "Could not configure L2: " + results.err_per_mac.begin()->second);
    }
    result["result"] = Json::objectValue;
}

/**
 * handles VLAN config message
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetVLAN::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    verify_fast_stack(params, result, port_id);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    const Json::Value &vlans = parse_array(params, "vlan", result);

    if ( vlans.size() > 2 ) {
        generate_parse_err(result, "Maximal number of stacked VLANs is 2, got: " + to_string(vlans.size()));
    }

    vlan_list_t vlan_list;

    for (int i=0; i<vlans.size(); i++) {
        uint16_t vlan = parse_uint16(vlans, i, result);
        validate_vlan(vlan, result);
        vlan_list.push_back(vlan);
    }

    try {
        port->set_vlan_cfg_async(vlan_list);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    uint64_t ticket_id = port->run_rx_cfg_tasks_async();
    process_results(ticket_id, params, result);

    return (TREX_RPC_CMD_OK);
}


void TrexRpcCmdSetVLAN::process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    stack_result_t results;
    bool found = get_stx()->get_port_by_id(port_id)->get_rx_cfg_tasks_results(ticket_id, results);
    if ( !found ) {
        generate_async_no_results(result, "Could not find VLAN config results, probably timeout on aging.");
    }
    if ( !results.is_ready ) {
        async_ticket_task_t task;
        task.result_func = bind(&TrexRpcCmdSetVLAN::process_results, this, ticket_id, params, _1);
        task.cancel_func = bind(&TrexPort::cancel_rx_cfg_tasks, get_stx()->get_port_by_id(port_id));
        get_stx()->add_task_by_ticket(ticket_id, task);
        generate_async_wip(result, ticket_id);
    }
    if ( results.err_per_mac.size() ) {
        assert(results.err_per_mac.size() == 1);
        generate_execute_err(result, "Could not configure VLAN(s): " + results.err_per_mac.begin()->second);
    }
    result["result"] = Json::objectValue;
}


void
TrexRpcCmdSetVLAN::validate_vlan(uint16_t vlan, Json::Value &result) {
    /* validate VLAN */
    if ( (vlan == 0) || (vlan > 4095) ) {
        generate_parse_err(result, "invalid VLAN tag: '" + to_string(vlan) + "'");
    }
}


/**
 * configures a port in L3 mode
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetL3::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    verify_fast_stack(params, result, port_id);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    const string src_ipv4_str  = parse_string(params, "src_addr", result);
    const string dst_ipv4_str  = parse_string(params, "dst_addr", result);

    char buf[4];

    if ( inet_pton(AF_INET, src_ipv4_str.c_str(), buf) != 1 ) {
        stringstream ss;
        ss << "invalid source IPv4 address: '" << src_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }
    string ip4_buf(buf, 4);

    if ( inet_pton(AF_INET, dst_ipv4_str.c_str(), buf) != 1 ) {
        stringstream ss;
        ss << "invalid destination IPv4 address: '" << dst_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }
    string gw4_buf(buf, 4);

    /* did we get a resolved MAC as well ? */
    if (params["resolved_mac"] != Json::Value::null) {
        const string resolved_mac  = parse_string(params, "resolved_mac", result);

        uint8_t dst_mac[6];
        if (!utl_str_to_macaddr(resolved_mac, dst_mac)) {
            stringstream ss;
            ss << "'invalid MAC address: '" << resolved_mac << "'";
            generate_parse_err(result, ss.str());
        }
        string dst_mac_buf((char*)dst_mac, 6);

        try {
            port->set_l3_mode_async(ip4_buf, gw4_buf, &dst_mac_buf);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }

    } else {
        try {
            port->set_l3_mode_async(ip4_buf, gw4_buf, nullptr);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }
    }

    uint64_t ticket_id = port->run_rx_cfg_tasks_async();
    process_results(ticket_id, params, result);

    return (TREX_RPC_CMD_OK);
}

void TrexRpcCmdSetL3::process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    stack_result_t results;
    bool found = get_stx()->get_port_by_id(port_id)->get_rx_cfg_tasks_results(ticket_id, results);
    if ( !found ) {
        generate_async_no_results(result, "Could not find L3 results, probably timeout on aging.");
    }
    if ( !results.is_ready ) {
        async_ticket_task_t task;
        task.result_func = bind(&TrexRpcCmdSetL3::process_results, this, ticket_id, params, _1);
        task.cancel_func = bind(&TrexPort::cancel_rx_cfg_tasks, get_stx()->get_port_by_id(port_id));
        get_stx()->add_task_by_ticket(ticket_id, task);
        generate_async_wip(result, ticket_id);
    }
    if ( results.err_per_mac.size() ) {
        assert(results.err_per_mac.size() == 1);
        generate_execute_err(result, "Could not configure L3: " + results.err_per_mac.begin()->second);
    }
    result["result"] = Json::objectValue;
}


/**
 * configures IPv6 of a port
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdConfIPv6::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    verify_fast_stack(params, result, port_id);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    const bool enabled = parse_bool(params, "enabled", result);
    string src_ipv6 = parse_string(params, "src_ipv6", result);

    if ( enabled && src_ipv6.size() ) {
        char buf[16];
        if ( inet_pton(AF_INET6, src_ipv6.c_str(), buf) != 1 ) {
            stringstream ss;
            ss << "invalid source IPv6 address: '" << src_ipv6 << "'";
            generate_parse_err(result, ss.str());
        }
        src_ipv6.assign(buf, 16);
    }

    try {
        port->conf_ipv6_async(enabled, src_ipv6);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    uint64_t ticket_id = port->run_rx_cfg_tasks_async();
    process_results(ticket_id, params, result);

    return (TREX_RPC_CMD_OK);
}

void TrexRpcCmdConfIPv6::process_results(uint64_t ticket_id, const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    stack_result_t results;
    bool found = get_stx()->get_port_by_id(port_id)->get_rx_cfg_tasks_results(ticket_id, results);
    if ( !found ) {
        generate_async_no_results(result, "Could not find IPv6 results, probably timeout on aging.");
    }
    if ( !results.is_ready ) {
        async_ticket_task_t task;
        task.result_func = bind(&TrexRpcCmdConfIPv6::process_results, this, ticket_id, params, _1);
        task.cancel_func = bind(&TrexPort::cancel_rx_cfg_tasks, get_stx()->get_port_by_id(port_id));
        get_stx()->add_task_by_ticket(ticket_id, task);
        generate_async_wip(result, ticket_id);
    }
    if ( results.err_per_mac.size() ) {
        assert(results.err_per_mac.size() == 1);
        generate_execute_err(result, "Could not configure IPv6: " + results.err_per_mac.begin()->second);
    }
    result["result"] = Json::objectValue;
}

 /**
+ * Start capture port
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdStartCapturePort::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    TrexPort *port = get_stx()->get_port_by_id(port_id);
    const std::string filter  = parse_string(params, "bpf_filter", result, "");

    /* compile BPF to verify */
    if (filter.size() > 0 && !bpf_verify(filter.c_str())) {
        generate_parse_err(result, "BPF filter: '" + filter + "' is not a valid pattern");
    }

    const std::string endpoint  = parse_string(params, "endpoint", result);
    if (endpoint.size() < 5) {
        std::stringstream ss;
        ss << "invalid endpoint '" << endpoint << "'";
        generate_parse_err(result, ss.str());
    }

    port->start_capture_port(filter, endpoint);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}

/**
 * Stop capture port
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdStopCapturePort::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    port->stop_capture_port();

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}

/**
 * Set capture port BPF Filter
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetCapturePortBPF::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    const std::string filter  = parse_string(params, "bpf_filter", result, "");

    /* compile BPF to verify */
    if (filter.size() > 0 && !bpf_verify(filter.c_str())) {
        generate_parse_err(result, "BPF filter: '" + filter + "' is not a valid pattern");
    }

    TrexPort *port = get_stx()->get_port_by_id(port_id);

    port->set_capture_port_bpf_filter(filter);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}

/**
 * capture command tree
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdCapture::_run(const Json::Value &params, Json::Value &result) {
    const string cmd = parse_choice(params, "command", {"start", "stop", "fetch", "status", "remove"}, result);
    
    if (cmd == "start") {
        parse_cmd_start(params, result);
    } else if (cmd == "stop") {
        parse_cmd_stop(params, result);
    } else if (cmd == "fetch") {
        parse_cmd_fetch(params, result);
    } else if (cmd == "status") {
        parse_cmd_status(params, result);
    } else if (cmd == "remove") {
        parse_cmd_remove(params, result);
    } else {
        /* can't happen */
        assert(0);
    }
    
    return TREX_RPC_CMD_OK;
}

/**
 * starts PCAP capturing
 * 
 */
void
TrexRpcCmdCapture::parse_cmd_start(const Json::Value &params, Json::Value &result) {
    
    uint32_t limit              = parse_uint32(params, "limit", result);
    
    /* parse mode type */
    const string mode_str  = parse_choice(params, "mode", {"fixed", "cyclic"}, result);
    TrexPktBuffer::mode_e mode  = ( (mode_str == "fixed") ? TrexPktBuffer::MODE_DROP_TAIL : TrexPktBuffer::MODE_DROP_HEAD);
    
    /* parse filters */
    const Json::Value &tx_json  = parse_array(params, "tx", result);
    const Json::Value &rx_json  = parse_array(params, "rx", result);
    CaptureFilter filter;
 
    /* parse a BPF format filter for the capture */
    const string filter_str = parse_string(params, "filter", result, "");
    
    /* compile it to verify */
    if (!bpf_verify(filter_str.c_str())) {
        generate_parse_err(result, "BPF filter: '" + filter_str + "' is not a valid pattern");
    }
    
    /* set the BPF filter to the capture filter */
    filter.set_bpf_filter(filter_str);
    
    set<uint8_t> ports;
    
    /* populate the filter */
    for (int i = 0; i < tx_json.size(); i++) {
        uint8_t tx_port = parse_byte(tx_json, i, result);
        validate_port_id(tx_port, result);
        
        filter.add_tx(tx_port);
        ports.insert(tx_port);
    }
    
    for (int i = 0; i < rx_json.size(); i++) {
        uint8_t rx_port = parse_byte(rx_json, i, result);
        validate_port_id(rx_port, result);
        
        filter.add_rx(rx_port);
        ports.insert(rx_port);
    }
    
    /* check that all ports are under service mode */
    for (uint8_t port_id : ports) {
        TrexPort *port = get_stx()->get_port_by_id(port_id);
        if (!port->is_service_mode_on()) {
            generate_parse_err(result, "start_capture is available only under service mode");
        }
    }
    
    static MsgReply<TrexCaptureRCStart> reply;
    reply.reset();
  
    /* send a start message to RX core */
    TrexRxCaptureStart *start_msg = new TrexRxCaptureStart(filter, limit, mode, reply);
    get_stx()->send_msg_to_rx(start_msg);
    
      /* wait for reply - might get a timeout */
    try {
        
        TrexCaptureRCStart rc = reply.wait_for_reply();
        if (!rc) {
            generate_execute_err(result, rc.get_err());
        }
        
        result["result"]["capture_id"] = rc.get_new_id();
        result["result"]["start_ts"]   = rc.get_start_ts();
    
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
}

/**
 * stops PCAP capturing
 * 
 */
void
TrexRpcCmdCapture::parse_cmd_stop(const Json::Value &params, Json::Value &result) {
    
    uint32_t capture_id = parse_uint32(params, "capture_id", result);
    
    static MsgReply<TrexCaptureRCStop> reply;
    reply.reset();
    
    TrexRxCaptureStop *stop_msg = new TrexRxCaptureStop(capture_id, reply);
    get_stx()->send_msg_to_rx(stop_msg);
    
    
    /* wait for reply - might get a timeout */
    try {
        
        TrexCaptureRCStop rc = reply.wait_for_reply();
        if (!rc) {
            generate_execute_err(result, rc.get_err());
        }
        result["result"]["pkt_count"] = rc.get_pkt_count();
    
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
   
}

/**
 * gets the status of all captures in the system
 * 
 */
void
TrexRpcCmdCapture::parse_cmd_status(const Json::Value &params, Json::Value &result) {
    
    /* generate a status command */
    
    static MsgReply<TrexCaptureRCStatus> reply;
    reply.reset();
    
    TrexRxCaptureStatus *status_msg = new TrexRxCaptureStatus(reply);
    get_stx()->send_msg_to_rx(status_msg);
    
      /* wait for reply - might get a timeout */
    try {
        
        TrexCaptureRCStatus rc = reply.wait_for_reply();
        if (!rc) {
            generate_execute_err(result, rc.get_err());
        }
        
        result["result"] = rc.get_status();
    
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
}

/**
 * fetch packets from a capture
 * 
 */
void
TrexRpcCmdCapture::parse_cmd_fetch(const Json::Value &params, Json::Value &result) {
    
    uint32_t capture_id = parse_uint32(params, "capture_id", result);
    uint32_t pkt_limit  = parse_uint32(params, "pkt_limit", result);
    
    /* generate a fetch command */
    
    static MsgReply<TrexCaptureRCFetch> reply;
    reply.reset();
    
    TrexRxCaptureFetch *fetch_msg = new TrexRxCaptureFetch(capture_id, pkt_limit, reply);
    get_stx()->send_msg_to_rx(fetch_msg);
    
     /* wait for reply - might get a timeout */
    try {
        
        TrexCaptureRCFetch rc = reply.wait_for_reply();
        if (!rc) {
            generate_execute_err(result, rc.get_err());
        }
    
        const TrexPktBuffer *pkt_buffer = rc.get_pkt_buffer();
            
        result["result"]["pending"]     = rc.get_pending();
        result["result"]["start_ts"]    = rc.get_start_ts();
        result["result"]["pkts"]        = pkt_buffer->to_json();
 
        /* delete the buffer */
        delete pkt_buffer;
       
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
 
}


void
TrexRpcCmdCapture::parse_cmd_remove(const Json::Value &params, Json::Value &result) {
    
    uint32_t capture_id = parse_uint32(params, "capture_id", result);
 
    /* generate a remove command */
    
    static MsgReply<TrexCaptureRCRemove> reply;
    reply.reset();
    
    TrexRxCaptureRemove *remove_msg = new TrexRxCaptureRemove(capture_id, reply);
    get_stx()->send_msg_to_rx(remove_msg);
    
    
    /* wait for reply - might get a timeout */
    try {
        
        TrexCaptureRCRemove rc = reply.wait_for_reply();
        if (!rc) {
            generate_execute_err(result, rc.get_err());
        }
        
        result["result"] = Json::objectValue;
        
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
}


/**
 * sends packets through the RX core
 */
trex_rpc_cmd_rc_e
TrexRpcCmdTXPkts::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
 
    TrexPort *port = get_stx()->get_port_by_id(port_id);

    if ( port->is_rx_running_cfg_tasks() ) {
        generate_execute_err(result, "Interface is in the middle of configuration");
    }

    /* IPG in usec */
    uint32_t ipg_usec = parse_uint32(params, "ipg_usec", result, 0);
    
    const Json::Value &pkts_json = parse_array(params, "pkts", result);
    
    /* do not allow batch of more than 1 seconds to be added - this can cause a non ending TX queue */
    if ( (usec_to_sec(pkts_json.size() * ipg_usec)) > 1) {
        generate_parse_err(result, "TX batch total transmit time exceeds the limit of 1 second");
    }
    
    vector<string> pkts;
    CNodeBase port_node;
    bool port_node_updated = false;
    
    for (int i = 0; i < pkts_json.size(); i++) {
        const Json::Value &pkt = parse_object(pkts_json, i, result);

        bool use_port_dst_mac = parse_bool(pkt, "use_port_dst_mac", result);
        bool use_port_src_mac = parse_bool(pkt, "use_port_src_mac", result);
        
        string pkt_binary = base64_decode(parse_string(pkt, "binary", result));
        
        /* check packet size */
        if ( (pkt_binary.size() < MIN_PKT_SIZE) || (pkt_binary.size() > MAX_PKT_SIZE) ) {
            stringstream ss;
            ss << "Bad packet size provided: " << pkt_binary.size() <<  ". Should be between " << MIN_PKT_SIZE << " and " << MAX_PKT_SIZE;
            generate_execute_err(result, ss.str()); 
        }

        if ( use_port_dst_mac || use_port_src_mac ) {
            if ( !port_node_updated ) {
                try {
                    port->get_port_node(port_node);
                    port_node_updated = true;
                } catch (const TrexException &ex) {
                    generate_execute_err(result, ex.what());
                }
            }

            if ( use_port_dst_mac ) {
                /* replace dst MAC if needed*/
                pkt_binary.replace(0, 6, port_node.get_dst_mac());
            }
            if ( use_port_src_mac ) {
                /* replace src MAC if needed */
                pkt_binary.replace(6, 6, port_node.get_src_mac());
            }
        }

        pkts.push_back(pkt_binary);
    }
    
    /* send packets to the RX core for TX'ing */
    static MsgReply<uint32_t> reply;
    reply.reset();
    
    dsec_t now = now_sec();
    
    TrexRxTXPkts *tx_pkts_msg = new TrexRxTXPkts(port_id, pkts, ipg_usec, reply);
    get_stx()->send_msg_to_rx(tx_pkts_msg);
    
      /* wait for reply - might get a timeout */
    try {
        
        result["result"]["sent"] = reply.wait_for_reply();
        
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    result["result"]["ts"]   = now;
    
    return TREX_RPC_CMD_OK;
}

/**
 * Get results of async task
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetAsyncResults::_run(const Json::Value &params, Json::Value &result) {
    uint64_t ticket_id = parse_uint64(params, "ticket_id", result);
    async_ticket_task_t task;
    bool found = get_stx()->get_task_by_ticket(ticket_id, task);

    if ( !found ) {
        generate_async_no_results(result);
    }

    task.result_func(result);

    return (TREX_RPC_CMD_OK);
}

/**
 * Cancel async task
 */
trex_rpc_cmd_rc_e
TrexRpcCmdCancelAsyncTask::_run(const Json::Value &params, Json::Value &result) {
    uint64_t ticket_id = parse_uint64(params, "ticket_id", result);

    async_ticket_task_t task;
    bool found = get_stx()->get_task_by_ticket(ticket_id, task);
    if ( found ) {
        task.cancel_func();
    }

    return (TREX_RPC_CMD_OK);
}


/****************************** component implementation ******************************/

/**
 * common RPC component
 * 
 */
TrexRpcCmdsCommon::TrexRpcCmdsCommon() : TrexRpcComponent("common") {
    
    m_cmds.push_back(new TrexRpcCmdAPISync(this));
    m_cmds.push_back(new TrexRpcCmdAPISyncV2(this));
    
    m_cmds.push_back(new TrexRpcCmdAcquire(this));
    m_cmds.push_back(new TrexRpcCmdRelease(this));
        
    m_cmds.push_back(new TrexRpcCmdGetPortStatus(this));
        
    m_cmds.push_back(new TrexRpcCmdPing(this));

    m_cmds.push_back(new TrexRpcCmdSetL2(this));
    m_cmds.push_back(new TrexRpcCmdSetL3(this));
    m_cmds.push_back(new TrexRpcCmdConfIPv6(this));
    m_cmds.push_back(new TrexRpcCmdSetVLAN(this));
    
    m_cmds.push_back(new TrexRpcPublishNow(this));
    m_cmds.push_back(new TrexRpcCmdGetVersion(this));
    m_cmds.push_back(new TrexRpcCmdGetSysInfo(this));
    m_cmds.push_back(new TrexRpcCmdGetGlobalStats(this));
    m_cmds.push_back(new TrexRpcCmdGetCmds(this));
    m_cmds.push_back(new TrexRpcCmdShutdown(this));
    m_cmds.push_back(new TrexRpcCmdGetUtilization(this));
    m_cmds.push_back(new TrexRpcCmdSetPortAttr(this));
    m_cmds.push_back(new TrexRpcCmdGetOwner(this));
    m_cmds.push_back(new TrexRpcCmdGetPortXStatsNames(this));
    m_cmds.push_back(new TrexRpcCmdGetPortXStatsValues(this));
    m_cmds.push_back(new TrexRpcCmdGetPortStats(this));
    m_cmds.push_back(new TrexRpcCmdCapture(this));
    m_cmds.push_back(new TrexRpcCmdTXPkts(this));
    m_cmds.push_back(new TrexRpcCmdGetRxQueuePkts(this));
    m_cmds.push_back(new TrexRpcCmdSetRxFeature(this));
    m_cmds.push_back(new TrexRpcCmdGetAsyncResults(this));
    m_cmds.push_back(new TrexRpcCmdCancelAsyncTask(this));

    m_cmds.push_back(new TrexRpcCmdStartCapturePort(this));
    m_cmds.push_back(new TrexRpcCmdStopCapturePort(this));
    m_cmds.push_back(new TrexRpcCmdSetCapturePortBPF(this));
 
}


