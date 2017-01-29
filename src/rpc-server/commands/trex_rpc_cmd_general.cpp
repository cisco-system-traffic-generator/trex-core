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
#include "trex_stateless_capture.h"
#include "trex_stateless_messaging.h"

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
        string driver;
        string pci_addr;
        string description;
        string hw_mac;
        supp_speeds_t supp_speeds;
        int numa;

        TrexStatelessPort *port = main->get_port_by_id(i);

        port->get_properties(driver);
        port->get_hw_mac(hw_mac);

        port->get_pci_info(pci_addr, numa);
        main->get_platform_api()->getPortAttrObj(i)->get_description(description);
        main->get_platform_api()->getPortAttrObj(i)->get_supported_speeds(supp_speeds);

        section["ports"][i]["index"]   = i;

        section["ports"][i]["driver"]       = driver;
        section["ports"][i]["description"]  = description;

        section["ports"][i]["pci_addr"]     = pci_addr;
        section["ports"][i]["numa"]         = numa;
        section["ports"][i]["hw_mac"]       = hw_mac;

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
    
    /* iterate over all attributes in the dict */
    for (const std::string &name : attr.getMemberNames()) {

        if (name == "promiscuous") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_promiscuous(enabled);
        }

        else if (name == "multicast") {
            bool enabled = parse_bool(attr[name], "enabled", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_multicast(enabled);
        }

        else if (name == "link_status") {
            bool up = parse_bool(attr[name], "up", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_link_up(up);
        }

        else if (name == "led_status") {
            bool on = parse_bool(attr[name], "on", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_led(on);
        }

        else if (name == "flow_ctrl_mode") {
            int mode = parse_int(attr[name], "mode", result);
            ret = get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->set_flow_ctrl(mode);
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
    result["result"]["service"]       = port->is_service_mode_on();
    
    /* attributes */
    get_stateless_obj()->get_platform_api()->getPortAttrObj(port_id)->to_json(result["result"]["attr"]);
    
    /* RX info */
    try {
        result["result"]["rx_info"] = port->rx_features_to_json();
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
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
    double       min_ipg_sec    = usec_to_sec(parse_udouble(params, "min_ipg_usec", result, 0));
    double       speedup        = parse_udouble(params, "speedup", result);
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


    /* IO might take time, increase timeout of WD */
    TrexMonitor * cur_monitor = TrexWatchDog::getInstance().get_current_monitor();
    if (cur_monitor != NULL) {
        cur_monitor->io_begin();
    }

    try {
        port->push_remote(pcap_filename, ipg_usec, min_ipg_sec, speedup, count, duration, is_dual);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }

    /* revert timeout of WD */
    if (cur_monitor != NULL) {
        cur_monitor->io_end();
    }

    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);

}


/**
 * set service mode on/off
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetServiceMode::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);
    bool enabled = parse_bool(params, "enabled", result);
    
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    try {
        port->set_service_mode(enabled);
    } catch (TrexException &ex) {
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
    const std::string type = parse_choice(params, "type", {"queue", "server"}, result);

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
TrexRpcCmdSetRxFeature::parse_queue_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
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
TrexRpcCmdSetRxFeature::parse_server_msg(const Json::Value &msg, TrexStatelessPort *port, Json::Value &result) {
}


trex_rpc_cmd_rc_e
TrexRpcCmdGetRxQueuePkts::_run(const Json::Value &params, Json::Value &result) {
    
    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);

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

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
    
    const std::string dst_mac_str  = parse_string(params, "dst_mac", result);
 
    uint8_t dst_mac[6];
    if (!utl_str_to_macaddr(dst_mac_str, dst_mac)) {
        std::stringstream ss;
        ss << "'invalid MAC address: '" << dst_mac_str << "'";
        generate_parse_err(result, ss.str());
    }
    
    try {
        port->set_l2_mode(dst_mac);
    } catch (const TrexException &ex) {
        generate_execute_err(result, ex.what());
    }
    
    result["result"] = Json::objectValue;
    return (TREX_RPC_CMD_OK);
}

/**
 * configures a port in L3 mode
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdSetL3::_run(const Json::Value &params, Json::Value &result) {
    uint8_t port_id = parse_port(params, result);

    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
 
    const std::string src_ipv4_str  = parse_string(params, "src_addr", result);
    const std::string dst_ipv4_str  = parse_string(params, "dst_addr", result);
    
    uint32_t src_ipv4;
    if (!utl_ipv4_to_uint32(src_ipv4_str.c_str(), src_ipv4)) {
        std::stringstream ss;
        ss << "invalid source IPv4 address: '" << src_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }
 
    uint32_t dst_ipv4;
    if (!utl_ipv4_to_uint32(dst_ipv4_str.c_str(), dst_ipv4)) {
        std::stringstream ss;
        ss << "invalid destination IPv4 address: '" << dst_ipv4_str << "'";
        generate_parse_err(result, ss.str());
    }
     
   
    
    /* did we get a resolved MAC as well ? */
    if (params["resolved_mac"] != Json::Value::null) {
        const std::string resolved_mac  = parse_string(params, "resolved_mac", result);
        
        uint8_t mac[6];
        if (!utl_str_to_macaddr(resolved_mac, mac)) {
            std::stringstream ss;
            ss << "'invalid MAC address: '" << resolved_mac << "'";
            generate_parse_err(result, ss.str());
        } 
    
        try {
            port->set_l3_mode(src_ipv4, dst_ipv4, mac);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }
        
    } else {
        try {
            port->set_l3_mode(src_ipv4, dst_ipv4);
        } catch (const TrexException &ex) {
            generate_execute_err(result, ex.what());
        }
        
    }
   
    result["result"] = Json::objectValue; 
    return (TREX_RPC_CMD_OK);    
    
}


/**
 * capture command tree
 * 
 */
trex_rpc_cmd_rc_e
TrexRpcCmdCapture::_run(const Json::Value &params, Json::Value &result) {
    const std::string cmd = parse_choice(params, "command", {"start", "stop", "fetch", "status", "remove"}, result);
    
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
    const std::string mode_str  = parse_choice(params, "mode", {"fixed", "cyclic"}, result);
    TrexPktBuffer::mode_e mode  = ( (mode_str == "fixed") ? TrexPktBuffer::MODE_DROP_TAIL : TrexPktBuffer::MODE_DROP_HEAD);
    
    /* parse filters */
    const Json::Value &tx_json  = parse_array(params, "tx", result);
    const Json::Value &rx_json  = parse_array(params, "rx", result);
    CaptureFilter filter;
    
    std::set<uint8_t> ports;
    
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
        TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(port_id);
        if (!port->is_service_mode_on()) {
            generate_parse_err(result, "start_capture is available only under service mode");
        }
    }
    
    static MsgReply<TrexCaptureRCStart> reply;
    reply.reset();
  
    /* send a start message to RX core */
    TrexStatelessRxCaptureStart *start_msg = new TrexStatelessRxCaptureStart(filter, limit, mode, reply);
    get_stateless_obj()->send_msg_to_rx(start_msg);
    
    TrexCaptureRCStart rc = reply.wait_for_reply();
    if (!rc) {
        generate_execute_err(result, rc.get_err());
    }
    
    result["result"]["capture_id"] = rc.get_new_id();
    result["result"]["start_ts"]   = rc.get_start_ts();
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
    
    TrexStatelessRxCaptureStop *stop_msg = new TrexStatelessRxCaptureStop(capture_id, reply);
    get_stateless_obj()->send_msg_to_rx(stop_msg);
    
    TrexCaptureRCStop rc = reply.wait_for_reply();
    if (!rc) {
        generate_execute_err(result, rc.get_err());
    }
    
    result["result"]["pkt_count"] = rc.get_pkt_count();
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
    
    TrexStatelessRxCaptureStatus *status_msg = new TrexStatelessRxCaptureStatus(reply);
    get_stateless_obj()->send_msg_to_rx(status_msg);
    
    TrexCaptureRCStatus rc = reply.wait_for_reply();
    if (!rc) {
        generate_execute_err(result, rc.get_err());
    }
    
    result["result"] = rc.get_status();
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
    
    TrexStatelessRxCaptureFetch *fetch_msg = new TrexStatelessRxCaptureFetch(capture_id, pkt_limit, reply);
    get_stateless_obj()->send_msg_to_rx(fetch_msg);
    
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
}

void
TrexRpcCmdCapture::parse_cmd_remove(const Json::Value &params, Json::Value &result) {
    
    uint32_t capture_id = parse_uint32(params, "capture_id", result);
 
    /* generate a remove command */
    
    static MsgReply<TrexCaptureRCRemove> reply;
    reply.reset();
    
    TrexStatelessRxCaptureRemove *remove_msg = new TrexStatelessRxCaptureRemove(capture_id, reply);
    get_stateless_obj()->send_msg_to_rx(remove_msg);
    
    TrexCaptureRCRemove rc = reply.wait_for_reply();
    if (!rc) {
        generate_execute_err(result, rc.get_err());
    }
    
    result["result"] = Json::objectValue;
}

