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
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>

#include <string>

#ifndef TREX_RPC_MOCK_SERVER

// DPDK c++ issue 
#ifndef UINT8_MAX
    #define UINT8_MAX 255
#endif

#ifndef UINT16_MAX
    #define UINT16_MAX 0xFFFF
#endif

// DPDK c++ issue 
#endif

#include <rte_ethdev.h>
#include <os_time.h>

void
port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list);

using namespace std;

/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id, const TrexPlatformApi *api) {
    std::vector<std::pair<uint8_t, uint8_t>> core_pair_list;

    m_port_id = port_id;

    m_port_state = PORT_STATE_IDLE;
    clear_owner();

    /* get the DP cores belonging to this port */
    api->port_id_to_cores(m_port_id, core_pair_list);

    for (auto core_pair : core_pair_list) {

        /* send the core id */
        m_cores_id_list.push_back(core_pair.first);
    }

    /* init the events DP DB */
    m_dp_events.create(this);
}


/**
 * acquire the port
 * 
 * @author imarom (09-Nov-15)
 * 
 * @param user 
 * @param force 
 */
void 
TrexStatelessPort::acquire(const std::string &user, bool force) {
    if ( (!is_free_to_aquire()) && (get_owner() != user) && (!force)) {
        throw TrexRpcException("port is already taken by '" + get_owner() + "'");
    }

    set_owner(user);
}

void
TrexStatelessPort::release(void) {
    verify_state( ~(PORT_STATE_TX | PORT_STATE_PAUSE) );
    clear_owner();
}

/**
 * starts the traffic on the port
 * 
 */
void
TrexStatelessPort::start_traffic(double mul, double duration) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_STREAMS);

    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);

    /* split it per core */
    double per_core_mul = mul / m_cores_id_list.size();

    /* compiler it */
    TrexStreamsCompiler compiler;
    TrexStreamsCompiledObj *compiled_obj = new TrexStreamsCompiledObj(m_port_id, per_core_mul);

    bool rc = compiler.compile(streams, *compiled_obj);
    if (!rc) {
        throw TrexRpcException("Failed to compile streams");
    }

    /* generate a message to all the relevant DP cores to start transmitting */
    int event_id = m_dp_events.generate_event_id();
    /* mark that DP event of stoppped is possible */
    m_dp_events.wait_for_event(TrexDpPortEvent::EVENT_STOP, event_id);

    TrexStatelessCpToDpMsgBase *start_msg = new TrexStatelessDpStart(m_port_id, event_id, compiled_obj, duration);

    change_state(PORT_STATE_TX);

    send_message_to_dp(start_msg);
    
}

void
TrexStatelessPort::start_traffic_max_bps(double max_bps, double duration) {
    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);

    TrexStreamsGraph graph;
    const TrexStreamsGraphObj &obj = graph.generate(streams);
    double m = (max_bps / obj.get_max_bps());

    /* call the main function */
    start_traffic(m, duration);
}

void
TrexStatelessPort::start_traffic_max_pps(double max_pps, double duration) {
    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);

    TrexStreamsGraph graph;
    const TrexStreamsGraphObj &obj = graph.generate(streams);
    double m = (max_pps / obj.get_max_pps());

    /* call the main function */
    start_traffic(m, duration);
}

/**
 * stop traffic on port
 * 
 * @author imarom (09-Nov-15)
 * 
 * @return TrexStatelessPort::rc_e 
 */
void
TrexStatelessPort::stop_traffic(void) {

    if (m_port_state != PORT_STATE_TX) {
        return;
    }

    /* mask out the DP stop event */
    m_dp_events.disable(TrexDpPortEvent::EVENT_STOP);

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);

    send_message_to_dp(stop_msg);

    change_state(PORT_STATE_STREAMS);
    
}

void
TrexStatelessPort::pause_traffic(void) {

    verify_state(PORT_STATE_TX);

    #if 0
    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);

    send_message_to_dp(stop_msg);

    m_port_state = PORT_STATE_UP_IDLE;
    #endif
    change_state(PORT_STATE_PAUSE);
}

void
TrexStatelessPort::resume_traffic(void) {

    verify_state(PORT_STATE_PAUSE);

    #if 0
    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);

    send_message_to_dp(stop_msg);

    m_port_state = PORT_STATE_UP_IDLE;
    #endif
    change_state(PORT_STATE_TX);
}

void
TrexStatelessPort::update_traffic(double mul) {

    verify_state(PORT_STATE_STREAMS | PORT_STATE_TX | PORT_STATE_PAUSE);

    #if 0
    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);

    send_message_to_dp(stop_msg);

    m_port_state = PORT_STATE_UP_IDLE;
    #endif
}

std::string 
TrexStatelessPort::get_state_as_string() const {

    switch (get_state()) {
    case PORT_STATE_DOWN:
        return "DOWN";

    case PORT_STATE_IDLE:
        return  "IDLE";

    case PORT_STATE_STREAMS:
        return "STREAMS";

    case PORT_STATE_TX:
        return "TX";

    case PORT_STATE_PAUSE:
        return "PAUSE";
    }

    return "UNKNOWN";
}

void
TrexStatelessPort::get_properties(string &driver, string &speed) {

    /* take this from DPDK */
    driver = "e1000";
    speed  = "1 Gbps";
}

bool
TrexStatelessPort::verify_state(int state, bool should_throw) const {
    if ( (state & m_port_state) == 0 ) {
        if (should_throw) {
            throw TrexRpcException("command cannot be executed on current state: '" + get_state_as_string() + "'");
        } else {
            return false;
        }
    }

    return true;
}

void
TrexStatelessPort::change_state(port_state_e new_state) {

    m_port_state = new_state;
}

/**
 * generate a random connection handler
 * 
 */
std::string 
TrexStatelessPort::generate_handler() {
    std::stringstream ss;

    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    /* generate 8 bytes of random handler */
    for (int i = 0; i < 8; ++i) {
        ss << alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return (ss.str());
}


void
TrexStatelessPort::encode_stats(Json::Value &port) {

    const TrexPlatformApi *api = get_stateless_obj()->get_platform_api();

    TrexPlatformInterfaceStats stats;
    api->get_interface_stats(m_port_id, stats);

    port["tx_bps"]          = stats.m_stats.m_tx_bps;
    port["rx_bps"]          = stats.m_stats.m_rx_bps;

    port["tx_pps"]          = stats.m_stats.m_tx_pps;
    port["rx_pps"]          = stats.m_stats.m_rx_pps;

    port["total_tx_pkts"]   = Json::Value::UInt64(stats.m_stats.m_total_tx_pkts);
    port["total_rx_pkts"]   = Json::Value::UInt64(stats.m_stats.m_total_rx_pkts);

    port["total_tx_bytes"]  = Json::Value::UInt64(stats.m_stats.m_total_tx_bytes);
    port["total_rx_bytes"]  = Json::Value::UInt64(stats.m_stats.m_total_rx_bytes);
    
    port["tx_rx_errors"]    = Json::Value::UInt64(stats.m_stats.m_tx_rx_errors);
}

void 
TrexStatelessPort::send_message_to_dp(TrexStatelessCpToDpMsgBase *msg) {

    for (auto core_id : m_cores_id_list) {

        /* send the message to the core */
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
        ring->Enqueue((CGenNode *)msg->clone());
    }

}

/**
 * when a DP (async) event occurs - handle it
 * 
 */
void 
TrexStatelessPort::on_dp_event_occured(TrexDpPortEvent::event_e event_type) {
    Json::Value data;

    switch (event_type) {

    case TrexDpPortEvent::EVENT_STOP:
        /* set a stop event */
        change_state(PORT_STATE_STREAMS);
        /* send a ZMQ event */

        data["port_id"] = m_port_id;
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STOPPED, data);
        break;

    default:
        assert(0);

    }
}
