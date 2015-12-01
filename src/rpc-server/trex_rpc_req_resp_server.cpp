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

#include <unistd.h>
#include <sstream>
#include <iostream>
#include <assert.h>

#include <zmq.h>
#include <json/json.h>

/**
 * ZMQ based request-response server
 * 
 */
TrexRpcServerReqRes::TrexRpcServerReqRes(const TrexRpcServerConfig &cfg, std::mutex *lock) : TrexRpcServerInterface(cfg, "req resp", lock) {
    /* ZMQ is not thread safe - this should be outside */
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

    /* create a socket based on the configuration */

    m_socket  = zmq_socket (m_context, ZMQ_REP);

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
}

bool
TrexRpcServerReqRes::fetch_one_request(std::string &msg) {

    zmq_msg_t zmq_msg;
    int rc;

    rc = zmq_msg_init(&zmq_msg);
    assert(rc == 0);

    rc = zmq_msg_recv (&zmq_msg, m_socket, 0);

    if (rc == -1) {
        zmq_msg_close(&zmq_msg);
        /* normal shutdown and zmq_term was called */
        if (errno == ETERM) {
            return false;
        } else {
            throw TrexRpcException("Unhandled error of zmq_recv");
        }
    }

    const char *data =  (const char *)zmq_msg_data(&zmq_msg);
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
    zmq_term(m_context);

}

/**
 * handles a request given to the server
 * respondes to the request
 */
void TrexRpcServerReqRes::handle_request(const std::string &request) {
    std::vector<TrexJsonRpcV2ParsedObject *> commands;

    Json::FastWriter writer;
    Json::Value response;

    /* first parse the request using JSON RPC V2 parser */
    TrexJsonRpcV2Parser rpc_request(request);
    rpc_request.parse(commands);

    int index = 0;

    /* if lock was provided, take it  */
    if (m_lock) {
        m_lock->lock();
    }

    /* for every command parsed - launch it */
    for (auto command : commands) {
        Json::Value single_response;

        command->execute(single_response);
        delete command;

        response[index++] = single_response;

    }

    /* done with the lock */
    if (m_lock) {
        m_lock->unlock();
    }

    /* write the JSON to string and sever on ZMQ */
    std::string response_str;

    if (response.size() == 1) {
        response_str = writer.write(response[0]);
    } else {
        response_str = writer.write(response);
    }
    
    verbose_json("Server Replied:  ", response_str);

    zmq_send(m_socket, response_str.c_str(), response_str.size(), 0);
    
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
