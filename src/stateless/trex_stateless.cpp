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

    delete m_ports;
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

