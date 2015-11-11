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
TrexStatelessPort::TrexStatelessPort(uint8_t port_id) : m_port_id(port_id) {
    m_port_state = PORT_STATE_IDLE;
    clear_owner();
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
TrexStatelessPort::start_traffic(double mul) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_STREAMS);

    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);

    /* compiler it */
    TrexStreamsCompiler compiler;
    TrexStreamsCompiledObj *compiled_obj = new TrexStreamsCompiledObj(m_port_id, mul);

    bool rc = compiler.compile(streams, *compiled_obj);
    if (!rc) {
        throw TrexRpcException("Failed to compile streams");
    }

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *start_msg = new TrexStatelessDpStart(compiled_obj);

    send_message_to_dp(start_msg);

    change_state(PORT_STATE_TX);
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
        return "down";

    case PORT_STATE_IDLE:
        return  "no streams";

    case PORT_STATE_STREAMS:
        return "with streams, idle";

    case PORT_STATE_TX:
        return "transmitting";

    case PORT_STATE_PAUSE:
        return "paused";
    }

    return "unknown";
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

    std::vector<std::pair<uint8_t, uint8_t>> cores_id_list;

    get_stateless_obj()->get_platform_api()->port_id_to_cores(m_port_id, cores_id_list);

    for (auto core_pair : cores_id_list) {

        /* send the message to the core */
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_pair.first);
        ring->Enqueue((CGenNode *)msg->clone());
    }

}
