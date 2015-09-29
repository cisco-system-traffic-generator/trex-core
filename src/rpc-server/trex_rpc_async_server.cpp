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
#include <trex_rpc_async_server.h>
#include <zmq.h>
#include <json/json.h>

/**
 * ZMQ based publisher server
 * 
 */
TrexRpcServerAsync::TrexRpcServerAsync(const TrexRpcServerConfig &cfg) : TrexRpcServerInterface(cfg, "publisher") {
    /* ZMQ is not thread safe - this should be outside */
    m_context = zmq_ctx_new();
}

void 
TrexRpcServerAsync::_rpc_thread_cb() {
    std::stringstream ss;

    /* create a socket based on the configuration */
    m_socket  = zmq_socket (m_context, ZMQ_PUB);

    switch (m_cfg.get_protocol()) {
    case TrexRpcServerConfig::RPC_PROT_TCP:
        ss << "tcp://*:";
        break;
    default:
        throw TrexRpcException("unknown protocol for RPC");
    }

    ss << m_cfg.get_port();

    /* bind the scoket */
    int rc = zmq_bind (m_socket, ss.str().c_str());
    if (rc != 0) {
        throw TrexRpcException("Unable to start ZMQ server at: " + ss.str());
    }

    /* while the server is running - publish results */
    while (m_is_running) {
        /* update all ports for their stats */
        uint8_t port_count = TrexStateless::get_instance().get_port_count();
        for (uint8_t i = 0; i < port_count; i++) {
            TrexStateless::get_instance().get_port_by_id(i).update_stats();
            const TrexPortStats &stats = TrexStateless::get_instance().get_port_by_id(i).get_stats();



        }
    }
}

void 
TrexRpcServerAsync::_stop_rpc_thread() {
    m_is_running = false;
    this->m_thread.join();
    zmq_term(m_context);
}
