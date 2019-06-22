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

#include "trex_rpc_server_api.h"
#include "trex_rpc_req_resp_server.h"
#include "trex_rpc_jsonrpc_v2_parser.h"

#include <unistd.h>
#include <zmq.h>
#include <sstream>
#include <iostream>
#include <ctime>

#include "trex_watchdog.h"

/************** RPC server interface ***************/

TrexRpcServerInterface::TrexRpcServerInterface(const TrexRpcServerConfig &cfg, const std::string &name) : m_cfg(cfg) {
    m_name = name;

    m_lock = cfg.m_lock;

    m_is_running = false;
    m_is_verbose = cfg.is_verbose();

    if (m_lock == NULL) {
        m_lock = &m_dummy_lock;
    }

    std::string logfile_name = cfg.get_logfile_name();
    if ( logfile_name.size() ) {
        m_logfile.open(logfile_name);
        if ( !m_logfile.is_open() ) {
            printf("ERROR: Could not open rpc-log file.\n");
            printf("Check path correctness and permissions: %s", logfile_name.c_str());
            exit(1);
        }
    }

}

TrexRpcServerInterface::~TrexRpcServerInterface() {
    if (m_is_running) {
        stop();
    }

    if ( m_logfile.is_open() ) {
        m_logfile.close();
    }
}

void TrexRpcServerInterface::verbose_msg(const std::string &msg) {
    if ( !m_is_verbose && !m_logfile.is_open() ) {
        return;
    }

    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    char time_buffer[80];
    strftime (time_buffer, 80, "%d-%m-%Y %H:%M:%S", tm);

    std::stringstream msg_ss;
    msg_ss << "[" << time_buffer << "] [" << m_name << "] " << msg << "\n";

    if ( m_is_verbose ) {
        std::cout << msg_ss.str() << std::flush;
    }
    if ( m_logfile.is_open() ) {
        m_logfile << msg_ss.str();
        m_logfile.flush();
    }
}

void TrexRpcServerInterface::verbose_json(const std::string &msg, const std::string &json_str) {
    verbose_msg(msg + "\n\n" + TrexJsonRpcV2Parser::pretty_json_str(json_str));
}

/**
 * starts a RPC specific server
 * 
 * @author imarom (17-Aug-15)
 */
void TrexRpcServerInterface::start() {
    m_is_running = true;

    verbose_msg("Starting RPC Server");

    /* prepare for run */
    _prepare();

    m_thread = new std::thread(&TrexRpcServerInterface::_rpc_thread_cb, this);
    if (!m_thread) {
        throw TrexRpcException("unable to create RPC thread");
    }
}

void TrexRpcServerInterface::stop() {
    m_is_running = false;

    verbose_msg("Attempting To Stop RPC Server");

    /* call the dynamic type class stop */
    _stop_rpc_thread();
    
    /* hold until thread has joined */    
    m_thread->join();

    verbose_msg("Server Stopped");

    delete m_thread;
}

void TrexRpcServerInterface::set_verbose(bool verbose) {
    m_is_verbose = verbose;
}

bool TrexRpcServerInterface::is_verbose() {
    return m_is_verbose;
}

bool TrexRpcServerInterface::is_running() {
    return m_is_running;
}


/************** RPC server *************/

static const std::string 
get_current_date_time() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%b %d %Y @ %X", &tstruct);

    return buf;
}

const std::string TrexRpcServer::s_server_uptime = get_current_date_time();

TrexRpcServer::TrexRpcServer(const TrexRpcServerConfig &req_resp_cfg) {

    m_req_resp = NULL;

    /* add the request response server */
    if (req_resp_cfg.get_protocol() == TrexRpcServerConfig::RPC_PROT_MOCK) {
        m_req_resp = new TrexRpcServerReqResMock(req_resp_cfg);
    } else {
        m_req_resp = new TrexRpcServerReqRes(req_resp_cfg);
    }

    m_servers.push_back(m_req_resp);
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

void TrexRpcServer::set_verbose(bool verbose) {
    for (auto server : m_servers) {
        server->set_verbose(verbose);
    }
}


std::string
TrexRpcServer::test_inject_request(const std::string &req_str) {
    if (m_req_resp) {
        return m_req_resp->test_inject_request(req_str);
    } else {
        return "";
    }
}

/**
 * MOCK req resp server
 */
TrexRpcServerReqResMock::TrexRpcServerReqResMock(const TrexRpcServerConfig &cfg) : TrexRpcServerReqRes(cfg) {
}

/**
 * override start
 * 
 */
void
TrexRpcServerReqResMock::start() {

}


/**
 * override stop
 */
void
TrexRpcServerReqResMock::stop() {

}
