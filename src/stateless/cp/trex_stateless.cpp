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
#include <trex_stateless.h>
#include <trex_stateless_port.h>

#include <sched.h>
#include <iostream>
#include <unistd.h>

using namespace std;

/***********************************************************
 * Trex stateless object
 * 
 **********************************************************/

/**
 * 
 */
TrexStateless::TrexStateless(const TrexStatelessCfg &cfg) {

    /* create RPC servers */

    /* set both servers to mutex each other */
    m_rpc_server = new TrexRpcServer(cfg.m_rpc_req_resp_cfg);
    m_rpc_server->set_verbose(cfg.m_rpc_server_verbose);

    /* configure ports */
    m_port_count = cfg.m_port_count;

    for (int i = 0; i < m_port_count; i++) {
        m_ports.push_back(new TrexStatelessPort(i, cfg.m_platform_api));
    }

    m_platform_api = cfg.m_platform_api;
    m_publisher    = cfg.m_publisher;

    /* API core version */
    const int API_VER_MAJOR = 2;
    const int API_VER_MINOR = 3;
    m_api_classes[APIClass::API_CLASS_TYPE_CORE].init(APIClass::API_CLASS_TYPE_CORE,
                                                      API_VER_MAJOR,
                                                      API_VER_MINOR);
}

/** 
 * release all memory 
 * 
 * @author imarom (08-Oct-15)
 */
TrexStateless::~TrexStateless() {

    shutdown();

    /* release memory for ports */
    for (auto port : m_ports) {
        delete port;
    }
    m_ports.clear();

    /* stops the RPC server */
    if (m_rpc_server) {
        delete m_rpc_server;
        m_rpc_server = NULL;
    }

    if (m_platform_api) {
        delete m_platform_api;
        m_platform_api = NULL;
    }
}

/**
* shutdown the server
*/
void TrexStateless::shutdown() {

    /* stop ports */
    for (TrexStatelessPort *port : m_ports) {
        /* safe to call stop even if not active */
        port->stop_traffic();
    }

    /* shutdown the RPC server */
    if (m_rpc_server) {
        m_rpc_server->stop();
    }
}

/**
 * starts the control plane side
 * 
 */
void
TrexStateless::launch_control_plane() {

    /* pin this process to the current running CPU
       any new thread will be called on the same CPU
       (control plane restriction)
     */
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(sched_getcpu(), &mask);
    sched_setaffinity(0, sizeof(mask), &mask);

    /* start RPC server */
    m_rpc_server->start();
}


/**
 * fetch a port by ID
 * 
 */
TrexStatelessPort * TrexStateless::get_port_by_id(uint8_t port_id) {
    if (port_id >= m_port_count) {
        throw TrexException("index out of range");
    }

    return m_ports[port_id];

}

uint8_t 
TrexStateless::get_port_count() {
    return m_port_count;
}

uint8_t 
TrexStateless::get_dp_core_count() {
    return m_platform_api->get_dp_core_count();
}

void
TrexStateless::encode_stats(Json::Value &global) {

    const TrexPlatformApi *api = get_stateless_obj()->get_platform_api();

    TrexPlatformGlobalStats stats;
    api->get_global_stats(stats);

    global["cpu_util"] = stats.m_stats.m_cpu_util;
    global["rx_cpu_util"] = stats.m_stats.m_rx_cpu_util;

    global["tx_bps"]   = stats.m_stats.m_tx_bps;
    global["rx_bps"]   = stats.m_stats.m_rx_bps;

    global["tx_pps"]   = stats.m_stats.m_tx_pps;
    global["rx_pps"]   = stats.m_stats.m_rx_pps;

    global["total_tx_pkts"] = Json::Value::UInt64(stats.m_stats.m_total_tx_pkts);
    global["total_rx_pkts"] = Json::Value::UInt64(stats.m_stats.m_total_rx_pkts);

    global["total_tx_bytes"] = Json::Value::UInt64(stats.m_stats.m_total_tx_bytes);
    global["total_rx_bytes"] = Json::Value::UInt64(stats.m_stats.m_total_rx_bytes);

    global["tx_rx_errors"]    = Json::Value::UInt64(stats.m_stats.m_tx_rx_errors);

    for (uint8_t i = 0; i < m_port_count; i++) {
        std::stringstream ss;

        ss << "port " << i;
        Json::Value &port_section = global[ss.str()];

        m_ports[i]->encode_stats(port_section);
    }
}

/**
 * generate a snapshot for publish (async publish)
 * 
 */
void
TrexStateless::generate_publish_snapshot(std::string &snapshot) {
    Json::FastWriter writer;
    Json::Value root;

    root["name"] = "trex-stateless-info";
    root["type"] = 0;

    /* stateless specific info goes here */
    root["data"] = Json::nullValue;

    snapshot = writer.write(root);
}

