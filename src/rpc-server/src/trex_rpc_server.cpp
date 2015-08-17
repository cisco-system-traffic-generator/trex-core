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

#include <trex_rpc_server_api.h>
#include <trex_rpc_req_resp_server.h>
#include <unistd.h>
#include <zmq.h>
#include <sstream>


/************** RPC server interface ***************/

TrexRpcServerInterface::TrexRpcServerInterface(const TrexRpcServerConfig &cfg) : m_cfg(cfg)  {
    m_is_running = false;
}

TrexRpcServerInterface::~TrexRpcServerInterface() {
    if (m_is_running) {
        stop();
    }
}

/**
 * starts a RPC specific server
 * 
 * @author imarom (17-Aug-15)
 */
void TrexRpcServerInterface::start() {
    m_is_running = true;

    m_thread = new std::thread(&TrexRpcServerInterface::_rpc_thread_cb, this);
    if (!m_thread) {
        throw TrexRpcException("unable to create RPC thread");
    }
}

void TrexRpcServerInterface::stop() {
    m_is_running = false;

    /* call the dynamic type class stop */
    _stop_rpc_thread();
    
    /* hold until thread has joined */    
    m_thread->join();
    delete m_thread;
}

bool TrexRpcServerInterface::is_running() {
    return m_is_running;
}


/************** RPC server *************/

TrexRpcServer::TrexRpcServer(const TrexRpcServerConfig &req_resp_cfg) {

    /* add the request response server */
    m_servers.push_back(new TrexRpcServerReqRes(req_resp_cfg));
}

TrexRpcServer::~TrexRpcServer() {

    /* make sure they are all stopped */
    stop();

    for (auto server : m_servers) {
        delete server;
    }
}

/**
 * start the server array
 * 
 */
void TrexRpcServer::start() {
    for (auto server : m_servers) {
        server->start();
    }
}

/**
 * stop the server array
 * 
 */
void TrexRpcServer::stop() {
    for (auto server : m_servers) {
        if (server->is_running()) {
            server->stop();
        }
    }
}

