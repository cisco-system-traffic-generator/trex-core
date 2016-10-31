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

#include "trex_stateless_rx_core.h"

#include <fstream>
#include <iostream>
#include <unistd.h>

#ifdef RTE_DPDK
    #include <../linux_dpdk/version.h>
#endif

using namespace std;

/**
 * API sync
 */
trex_rpc_cmd_rc_e
TrexRpcCmdAPISync::_run(const Json::Value &params, Json::Value &result) {
    const Json::Value &api_vers = parse_array(params, "api_vers", result);

    Json::Value api_ver_rc = Json::arrayValue;

    /* for every element in the list - generate the appropirate API handler */
    for (const auto api_ver : api_vers) {
        Json::Value single_rc;

        /* only those are supported */
        const std::string type = parse_choice(api_ver, "type", {"core"}, result);

        int major = parse_int(api_ver, "major", result);
        int minor = parse_int(api_ver, "minor", result);
        APIClass::type_e api_type;

        /* decode type of API */
        if (type == "core") {
            api_type = APIClass::API_CLASS_TYPE_CORE;
        }

        single_rc["type"]    = type;

        /* this section might throw exception in case versions do not match */
        try {
            single_rc["api_h"] = get_stateless_obj()->verify_api(api_type, major, minor);

        } catch (const TrexAPIException &e) {
            generate_execute_err(result, e.what());
        }

        /* add to the response */
        api_ver_rc.append(single_rc);
    }

    result["result"]["api_vers"] = api_ver_rc;

    return (TREX_RPC_CMD_OK);
}

/**
 * ping command
 */
trex_rpc_cmd_rc_e
TrexRpcCmdPing::_run(const Json::Value &params, Json::Value &result) {

    result["result"] = Json::objectValue;
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
    for (auto port : get_stateless_obj()->get_port_list()) {
        TrexPortOwner &owner = port->get_owner();
        if ( (!owner.is_free()) && (!owner.is_owned_by(user)) && !force) {
            std::stringstream ss;
            ss << "port " << int(port->get_port_id()) << " is owned by '" << owner.get_name() << "' - specify 'force' for override";
            generate_execute_err(result, ss.str());
        }
    }

    /* signal that we got a shutdown request */
    get_stateless_obj()->get_platform_api()->mark_for_shutdown();

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

// get utilization of CPU per thread with up to 20 latest values + mbufs per socket
trex_rpc_cmd_rc_e
TrexRpcCmdGetUtilization::_run(const Json::Value &params, Json::Value &result) {
    cpu_util_full_t cpu_util_full;

    Json::Value &section = result["result"];

    if (get_stateless_obj()->get_platform_api()->get_mbuf_util(section) != 0) {
        return TREX_RPC_CMD_INTERNAL_ERR;
    }

    if (get_stateless_obj()->get_platform_api()->get_cpu_util_full(cpu_util_full) != 0) {
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
    section["dp_core_count_per_port"] = main->get_dp_core_count() / (main->get_port_count() / 2);
    section["core_type"] = get_cpu_model();

    /* ports */


    section["port_count"] = main->get_port_count();

    section["ports"] = Json::arrayValue;

    for (int i = 0; i < main->get_port_count(); i++) {
        uint32_t speed;
        string driver;
        string hw_macaddr;
        string src_macaddr;
        string dst_macaddr;
        string pci_addr;
        string description;
        supp_speeds_t supp_speeds;
        int numa;

        TrexStatelessPort *port = main->get_port_by_id(i);
        port->get_properties(driver, speed);
        port->get_macaddr(hw_macaddr, src_macaddr, dst_macaddr);

        port->get_pci_info(pci_addr, numa);
        main->get_platform_api()->getPortAttrObj(i)->get_description(description);
        main->get_platform_api()->getPortAttrObj(i)->get_supported_speeds(supp_speeds);

        section["ports"][i]["index"]   = i;

        section["ports"][i]["driver"]       = driver;
        section["ports"][i]["description"]  = description;
        section["ports"][i]["hw_macaddr"]   = hw_macaddr;
        section["ports"][i]["src_macaddr"]  = src_macaddr;
        section["ports"][i]["dst_macaddr"]  = dst_macaddr;

        section["ports"][i]["pci_addr"]     = pci_addr;
        section["ports"][i]["numa"]         = numa;

        uint16_t caps = port->get_rx_caps();
        section["ports"][i]["rx"]["caps"]      = Json::arrayValue;
        if (caps & TrexPlatformApi::IF_STAT_IPV4_ID) {
            section["ports"][i]["rx"]["caps"].append("flow_stats");
        }
        if (caps & TrexPlatformApi::IF_STAT_PAYLOAD) {
            section["ports"][i]["rx"]["caps"].append("latency");
        }
        if (caps & TrexPlatformApi::IF_STAT_RX_BYTES_COUNT) {
            section["ports"][i]["rx"]["caps"].append("rx_bytes");
        }
        section["ports"][i]["rx"]["counters"]  = port->get_rx_count_num();
        section["ports"][i]["speed"] = (uint16_t) speed / 1000;
        section["ports"][i]["is_fc_supported"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(i)->is_fc_change_supported();
        section["ports"][i]["is_led_supported"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(i)->is_led_change_supported();
        section["ports"][i]["is_link_supported"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(i)->is_link_change_supported();
        section["ports"][i]["is_virtual"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(i)->is_virtual();
        section["ports"][i]["supp_speeds"] = Json::arrayValue;
        for (int speed_id=0; speed_id<supp_speeds.size(); speed_id++) {
            section["ports"][i]["supp_speeds"].append(supp_speeds[speed_id]);
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

    const Json::Value &attr = parse_object(params, "attr", result);
    int ret = 0;
    bool changed = false;
    /* iterate over all attributes in the dict */
    for (const std::string &name : attr.getMemberNames()) {
        if (name == "promiscuous") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_promiscuous(enabled);
        }
        else if (name == "link_status") {
            bool up = parse_bool(attr[name], "up", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_link_up(up);
        }
        else if (name == "led_status") {
            bool on = parse_bool(attr[name], "on", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_led(on);
        } else if (name == "flow_ctrl_mode") {
            int mode = parse_int(attr[name], "mode", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_flow_ctrl(mode);
        } else {
            generate_execute_err(result, "Not recognized attribute: " + name);
            break;
        }
        if (ret != 0){
            if ( ret == -ENOTSUP ) {
                generate_execute_err(result, "Error applying " + name + ": operation is not supported for this NIC.");
            }
            else if (ret) {
                generate_execute_err(result, "Error applying " + name + " attribute, return value: " + to_string(ret));
            }
            break;
        } else {
            changed = true;
        }
    }
    if (changed) {
        get_stateless_obj()->get_platform_api()->publish_async_port_attr_changed(port_id);
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
 * get port extended stats names (keys of dict)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdGetPortXStatsNames::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    xstats_names_t xstats_names;

    int ret = get_stateless_obj()->get_platform_api()->get_xstats_names(port_id, xstats_names);
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

    int ret = get_stateless_obj()->get_platform_api()->get_xstats_values(port_id, xstats_values);
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
    result["result"]["speed"]         = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->get_link_speed();

    /* attributes */
    result["result"]["attr"]["promiscuous"]["enabled"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->get_promiscuous();
    result["result"]["attr"]["link"]["up"] = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->is_link_up();
    int mode;
    int ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->get_flow_ctrl(mode);
    if (ret != 0) {
        mode = -1;
    }
    result["result"]["attr"]["fc"]["mode"] = mode;

    return (TREX_RPC_CMD_OK);
}

/**
 * publish async data now (fast flush)
 *
 */
trex_rpc_cmd_rc_e
TrexRpcPublishNow::_run(const Json::Value &params, Json::Value &result) {
    TrexStateless *main = get_stateless_obj();

    uint32_t key  = parse_uint32(params, "key", result);
    bool baseline = parse_bool(params, "baseline", result);

    main->get_platform_api()->publish_async_data_now(key, baseline);

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}


/**
 * push a remote PCAP on a port
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdPushRemote::_run(const Json::Value &params, Json::Value &result) {

    uint8_t port_id = parse_port(params, result);
    std::string  pcap_filename  = parse_string(params, "pcap_filename", result);
    double       ipg_usec       = parse_double(params, "ipg_usec", result);
    double       speedup        = parse_double(params, "speedup", result);
    uint32_t     count          = parse_uint32(params, "count", result);
    double       duration       = parse_double(params, "duration", result);
    bool         is_dual        = parse_bool(params,   "is_dual", result, false);
    std::string  slave_handler  = parse_string(params, "slave_handler", result, "");

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    /* for dual mode - make sure slave_handler matches */
    if (is_dual) {
        TrexStatelessPort *slave = get_stateless_obj()->get_port_by_id(port_id ^ 0x1);
        if (!slave->get_owner().verify(slave_handler)) {
            generate_execute_err(result, "incorrect or missing slave port handler");
        }
    }


    try {
        port->push_remote(pcap_filename, ipg_usec, speedup, count, duration, is_dual);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}

/**
 * set on/off RX software receive mode
 *
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetRxFeature::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);
	TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    /* decide which feature is being set */
    const std::string type = parse_choice(params, "type", {"filter_mode", "record", "queue", "server"}, result);

    if (type == "filter_mode") {
        parse_filter_mode_msg(params, port, result);
    } else if (type == "record") {
        parse_record_msg(params, port, result);
    } else if (type == "queue") {
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
TrexRpcCmdSetRxFeature::parse_filter_mode_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
	const std::string type = parse_choice(msg, "filter_type", {"hw", "all"}, result);

	rx_filter_mode_e filter_mode;
	if (type == "hw") {
		filter_mode = RX_FILTER_MODE_HW;
	} else if (type == "all") {
		filter_mode = RX_FILTER_MODE_ALL;
	} else {
		assert(0);
	}

	try {
        port->set_rx_filter_mode(filter_mode);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
}

void 
TrexRpcCmdSetRxFeature::parse_record_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
}

void 
TrexRpcCmdSetRxFeature::parse_queue_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
}

void 
TrexRpcCmdSetRxFeature::parse_server_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
}


trex_rpc_cmd_rc_e
TrexRpcCmdGetRxSwPkts::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

    try {
        RxPacketBuffer *pkt_buffer = port->get_rx_sw_pkts();
        result["result"]["pkts"] = pkt_buffer->to_json();

    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    
    return (TREX_RPC_CMD_OK);
}
