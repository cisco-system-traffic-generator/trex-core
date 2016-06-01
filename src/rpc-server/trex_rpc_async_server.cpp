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

/* required for sleep_for c++ 2011
   https://bugs.launchpad.net/ubuntu/+source/gcc-4.4/+bug/608145
*/
#define _GLIBCXX_USE_NANOSLEEP

#include <trex_stateless.h>
#include <trex_stateless_port.h>
#include <trex_rpc_async_server.h>
#include <zmq.h>
#include <json/json.h>
#include <string>
#include <iostream>

/**
 * ZMQ based publisher server
 * 
 */
TrexRpcServerAsync::TrexRpcServerAsync(const TrexRpcServerConfig &cfg) : TrexRpcServerInterface(cfg, "publisher") {
    /* ZMQ is not thread safe - this should be outside */
    m_context = zmq_ctx_new();
}

void
TrexRpcServerAsync::_prepare() {
}

/**
 * publisher thread
 * 
 */
void 
TrexRpcServerAsync::_rpc_thread_cb() {
/* disabled, using the main publisher */
#if 0
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
        Json::Value snapshot;
        Json::FastWriter writer;

        /* if lock was provided - take it */
        if (m_lock) {
            m_lock->lock();
        }

        /* trigger a full update for stats */
        //get_stateless_obj()->update_stats();

        /* done with the lock */
        if (m_lock) {
            m_lock->unlock();
        }

        /* encode them to JSON */
        get_stateless_obj()->encode_stats(snapshot);

        /* write to string and publish */
        std::string snapshot_str = writer.write(snapshot);

        zmq_send(m_socket, snapshot_str.c_str(), snapshot_str.size(), 0);
        //std::cout << "sending " << snapshot_str << "\n";

        /* relax for some time */
        std::this_thread::sleep_for (std::chrono::milliseconds(1000));
    }

    /* must be closed from the same thread */
    zmq_close(m_socket);
#endif
}

void 
TrexRpcServerAsync::_stop_rpc_thread() {
    zmq_term(m_context);
}
