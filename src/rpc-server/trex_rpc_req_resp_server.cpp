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
#include <trex_rpc_jsonrpc_v2_parser.h>
#include <trex_rpc_zip.h>

#include <unistd.h>
#include <sstream>
#include <iostream>
#include <assert.h>

#include <zmq.h>
#include <json/json.h>

#include "trex_watchdog.h"

/**
 * ZMQ based request-response server
 * 
 */
TrexRpcServerReqRes::TrexRpcServerReqRes(const TrexRpcServerConfig &cfg) : TrexRpcServerInterface(cfg, "ZMQ sync request-response") {

}

void TrexRpcServerReqRes::_prepare() {
    m_context = zmq_ctx_new();
}

/**
 * main entry point for the server 
 * this function will be created on a different thread 
 * 
 * @author imarom (17-Aug-15)
 */
void TrexRpcServerReqRes::_rpc_thread_cb() {
    std::stringstream ss;
    int zmq_rc;

    m_monitor.create(m_name, 1);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    /* create a socket based on the configuration */

    m_socket  = zmq_socket (m_context, ZMQ_REP);

    /* to make sure the watchdog gets tickles form time to time we give a timeout of 500ms */
    int timeout = 500;
    zmq_rc = zmq_setsockopt (m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(int));
    assert(zmq_rc == 0);

    switch (m_cfg.get_protocol()) {
    case TrexRpcServerConfig::RPC_PROT_TCP:
        ss << "tcp://*:";
        break;
    default:
        throw TrexRpcException("unknown protocol for RPC");
    }

    ss << m_cfg.get_port();

    /* bind the scoket */
    zmq_rc = zmq_bind (m_socket, ss.str().c_str());
    if (zmq_rc != 0) {
        throw TrexRpcException("Unable to start ZMQ server at: " + ss.str());
    }

    /* server main loop */
    while (m_is_running) {
        std::string request;

        /* get the next request */
        bool rc = fetch_one_request(request);
        if (!rc) {
            break;
        }

        verbose_json("Server Received: ", TrexJsonRpcV2Parser::pretty_json_str(request));

        handle_request(request);
    }

    /* must be done from the same thread */
    zmq_close(m_socket);

    /* done */
    m_monitor.disable();
}

bool
TrexRpcServerReqRes::fetch_one_request(std::string &msg) {

    zmq_msg_t zmq_msg;
    int rc;

    rc = zmq_msg_init(&zmq_msg);
    assert(rc == 0);

    while (true) {
        m_monitor.tickle();

        rc = zmq_msg_recv (&zmq_msg, m_socket, 0);
        if (rc != -1) {
            break;
        }

        /* timeout ? */
        if (errno == EAGAIN) {
            continue;
        }

        /* error ! */
        zmq_msg_close(&zmq_msg);

        /* normal shutdown and zmq_term was called */
        if (errno == ETERM) {
            return false;
        } else {
            throw TrexRpcException("Unhandled error of zmq_recv");
        }
    }

  

    const char *data = (const char *)zmq_msg_data(&zmq_msg);
    size_t len = zmq_msg_size(&zmq_msg);
    msg.append(data, len);

    zmq_msg_close(&zmq_msg);
    return true;
}

/**
 * stops the ZMQ based RPC server
 * 
 */
void TrexRpcServerReqRes::_stop_rpc_thread() {
    /* by calling zmq_term we signal the blocked thread to exit */
    if (m_context) {
        zmq_term(m_context);
    }

}


/**
 * handles a request given to the server
 * respondes to the request
 */
void TrexRpcServerReqRes::handle_request(const std::string &request) {
    std::string response;

    process_request(request, response);

    zmq_send(m_socket, response.c_str(), response.size(), 0);
}

void TrexRpcServerReqRes::process_request(const std::string &request, std::string &response) {

    if (TrexRpcZip::is_compressed(request)) {
        process_zipped_request(request, response);
    } else {
        process_request_raw(request, response);
    }

}

/**
 * main processing of the request
 * 
 */
void TrexRpcServerReqRes::process_request_raw(const std::string &request, std::string &response) {

    std::vector<TrexJsonRpcV2ParsedObject *> commands;

    Json::FastWriter writer;
    Json::Value response_json;

    /* first parse the request using JSON RPC V2 parser */
    TrexJsonRpcV2Parser rpc_request(request);
    rpc_request.parse(commands);

    int index = 0;

    /* for every command parsed - launch it */
    for (auto command : commands) {
        Json::Value single_response;

        /* the command itself should be protected */
        std::unique_lock<std::mutex> lock(*m_lock);
        command->execute(single_response);
        lock.unlock();

        delete command;

        response_json[index++] = single_response;

        /* batch is like getting all the messages one by one - it should not be considered as stuck thread */
        /* need to think if this is a good thing */
        //m_monitor.tickle();
    }

    /* write the JSON to string and sever on ZMQ */

    if (response.size() == 1) {
        response = writer.write(response_json[0]);
    } else {
        response = writer.write(response_json);
    }
    
    verbose_json("Server Replied:  ", response);

}

void TrexRpcServerReqRes::process_zipped_request(const std::string &request, std::string &response) {
    std::string unzipped;

    /* try to uncomrpess - if fails, last shot is the JSON RPC */
    bool rc = TrexRpcZip::uncompress(request, unzipped);
    if (!rc) {
        return process_request_raw(request, response);
    }

    /* process the request */
    std::string raw_response;
    process_request_raw(unzipped, raw_response);

    TrexRpcZip::compress(raw_response, response);

}

/**
 * handles a server error
 * 
 */
void 
TrexRpcServerReqRes::handle_server_error(const std::string &specific_err) {
    Json::FastWriter writer;
    Json::Value response;

    /* generate error */
    TrexJsonRpcV2Parser::generate_common_error(response, specific_err);

     /* write the JSON to string and sever on ZMQ */
    std::string response_str = writer.write(response);
    
    verbose_json("Server Replied:  ", response_str);

    zmq_send(m_socket, response_str.c_str(), response_str.size(), 0);
}



std::string
TrexRpcServerReqRes::test_inject_request(const std::string &req) {
    std::string response;

    process_request(req, response);

    return response;
}

