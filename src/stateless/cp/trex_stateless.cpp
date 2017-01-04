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

//#include <sched.h>
#include <iostream>
#include <unistd.h>

#include "trex_stateless.h"
#include "trex_stateless_port.h"
#include "trex_stateless_messaging.h"

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
    const int API_VER_MAJOR = 3;
    const int API_VER_MINOR = 0;
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

capture_id_t
TrexStateless::start_capture(const CaptureFilter &filter, uint64_t limit) {
    static MsgReply<capture_id_t> reply;
    
    reply.reset();
    
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    TrexStatelessRxStartCapture *msg = new TrexStatelessRxStartCapture(filter, limit, reply);
     
    ring->Enqueue((CGenNode *)msg);
    
    capture_id_t new_id = reply.wait_for_reply();
    
    return (new_id);
}

capture_id_t
TrexStateless::stop_capture(capture_id_t capture_id) {
    static MsgReply<capture_id_t> reply;
    
    reply.reset();
    
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    TrexStatelessRxStopCapture *msg = new TrexStatelessRxStopCapture(capture_id, reply);
     
    ring->Enqueue((CGenNode *)msg);
    
    capture_id_t rc = reply.wait_for_reply();
    
    return (rc);
}

