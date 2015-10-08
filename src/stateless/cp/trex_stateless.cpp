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

using namespace std;

/***********************************************************
 * Trex stateless object
 * 
 **********************************************************/
TrexStateless::TrexStateless() {
    m_is_configured = false;
}

/**
 * one time configuration of the stateless object
 * 
 */
void TrexStateless::configure(uint8_t port_count) {

    TrexStateless& instance = get_instance_internal();

    if (instance.m_is_configured) {
        throw TrexException("re-configuration of stateless object is not allowed");
    }

    instance.m_port_count = port_count;
    instance.m_ports = new TrexStatelessPort*[port_count];

    for (int i = 0; i < instance.m_port_count; i++) {
        instance.m_ports[i] = new TrexStatelessPort(i);
    }

    instance.m_is_configured = true;
}

TrexStateless::~TrexStateless() {
    for (int i = 0; i < m_port_count; i++) {
        delete m_ports[i];
    }

    delete [] m_ports;
}

TrexStatelessPort * TrexStateless::get_port_by_id(uint8_t port_id) {
    if (port_id >= m_port_count) {
        throw TrexException("index out of range");
    }

    return m_ports[port_id];

}

uint8_t TrexStateless::get_port_count() {
    return m_port_count;
}

void 
TrexStateless::update_stats() {

    /* update CPU util. */
    #ifdef TREX_RPC_MOCK_SERVER
        m_stats.m_stats.m_cpu_util = 0;
    #else
        m_stats.m_stats.m_cpu_util = 0;
    #endif

    /* for every port update and accumulate */
    for (uint8_t i = 0; i < m_port_count; i++) {
        m_ports[i]->update_stats();

        const TrexPortStats & port_stats = m_ports[i]->get_stats();

        m_stats.m_stats.m_tx_bps += port_stats.m_stats.m_tx_bps;
        m_stats.m_stats.m_rx_bps += port_stats.m_stats.m_rx_bps;

        m_stats.m_stats.m_tx_pps += port_stats.m_stats.m_tx_pps;
        m_stats.m_stats.m_rx_pps += port_stats.m_stats.m_rx_pps;

        m_stats.m_stats.m_total_tx_pkts += port_stats.m_stats.m_total_tx_pkts;
        m_stats.m_stats.m_total_rx_pkts += port_stats.m_stats.m_total_rx_pkts;

        m_stats.m_stats.m_total_tx_bytes += port_stats.m_stats.m_total_tx_bytes;
        m_stats.m_stats.m_total_rx_bytes += port_stats.m_stats.m_total_rx_bytes;

        m_stats.m_stats.m_tx_rx_errors += port_stats.m_stats.m_tx_rx_errors;
    }
}

void
TrexStateless::encode_stats(Json::Value &global) {

    global["cpu_util"] = m_stats.m_stats.m_cpu_util;

    global["tx_bps"]   = m_stats.m_stats.m_tx_bps;
    global["rx_bps"]   = m_stats.m_stats.m_rx_bps;

    global["tx_pps"]   = m_stats.m_stats.m_tx_pps;
    global["rx_pps"]   = m_stats.m_stats.m_rx_pps;

    global["total_tx_pkts"] = Json::Value::UInt64(m_stats.m_stats.m_total_tx_pkts);
    global["total_rx_pkts"] = Json::Value::UInt64(m_stats.m_stats.m_total_rx_pkts);

    global["total_tx_bytes"] = Json::Value::UInt64(m_stats.m_stats.m_total_tx_bytes);
    global["total_rx_bytes"] = Json::Value::UInt64(m_stats.m_stats.m_total_rx_bytes);

    global["tx_rx_errors"]    = Json::Value::UInt64(m_stats.m_stats.m_tx_rx_errors);

    for (uint8_t i = 0; i < m_port_count; i++) {
        std::stringstream ss;

        ss << "port " << i;
        Json::Value &port_section = global[ss.str()];

        m_ports[i]->encode_stats(port_section);
    }
}

