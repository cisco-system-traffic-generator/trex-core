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
#include <trex_stateless_api.h>

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


/***************************
 * trex stateless port stats
 * 
 **************************/
TrexPortStats::TrexPortStats() {
    m_stats = {0};
}

/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id) : m_port_id(port_id) {
    m_port_state = PORT_STATE_UP_IDLE;
    clear_owner();
}


/**
 * starts the traffic on the port
 * 
 */
TrexStatelessPort::rc_e
TrexStatelessPort::start_traffic(void) {

    if (m_port_state != PORT_STATE_UP_IDLE) {
        return (RC_ERR_BAD_STATE_FOR_OP);
    }

    if (get_stream_table()->size() == 0) {
        return (RC_ERR_NO_STREAMS);
    }

    m_port_state = PORT_STATE_TRANSMITTING;

    /* real code goes here */
    return (RC_OK);
}

void 
TrexStatelessPort::stop_traffic(void) {

    /* real code goes here */
    if (m_port_state == PORT_STATE_TRANSMITTING) {
        m_port_state = PORT_STATE_UP_IDLE;
    }
}

/**
* access the stream table
* 
*/
TrexStreamTable * TrexStatelessPort::get_stream_table() {
    return &m_stream_table;
}


std::string 
TrexStatelessPort::get_state_as_string() {

    switch (get_state()) {
    case PORT_STATE_DOWN:
        return "down";

    case PORT_STATE_UP_IDLE:
        return  "idle";

    case PORT_STATE_TRANSMITTING:
        return "transmitting";
    }

    return "unknown";
}

void
TrexStatelessPort::get_properties(string &driver, string &speed) {

    /* take this from DPDK */
    driver = "e1000";
    speed  = "1 Gbps";
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
