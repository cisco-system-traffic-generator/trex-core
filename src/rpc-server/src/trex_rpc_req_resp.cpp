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
#include <trex_rpc_req_resp.h>

#include <unistd.h>
#include <sstream>
#include <iostream>

#include <zmq.h>
#include <json/json.h>


TrexRpcServerReqRes::TrexRpcServerReqRes(TrexRpcServerArray::protocol_type_e protocol, uint16_t port) : TrexRpcServerInterface(protocol, port) {
    /* ZMQ is not thread safe - this should be outside */
    m_context = zmq_ctx_new();
}

void TrexRpcServerReqRes::_rpc_thread_cb() {
    std::stringstream ss;

    //  Socket to talk to clients
    m_socket  = zmq_socket (m_context, ZMQ_REP);

    switch (m_protocol) {
    case TrexRpcServerArray::RPC_PROT_TCP:
        ss << "tcp://*:";
        break;
    default:
        throw TrexRpcException("unknown protocol for RPC");
    }

    ss << m_port;

    int rc = zmq_bind (m_socket, ss.str().c_str());
    if (rc != 0) {
        throw TrexRpcException("Unable to start ZMQ server at: " + ss.str());
    }

    printf("listening on %s\n", ss.str().c_str());

    /* server main loop */
    while (m_is_running) {
        int msg_size = zmq_recv (m_socket, m_msg_buffer, sizeof(m_msg_buffer), 0);

        if (msg_size == -1) {
            /* normal shutdown and zmq_term was called */
            if (errno == ETERM) {
                break;
            } else {
                throw TrexRpcException("Unhandled error of zmq_recv");
            }
        }

        handle_request(m_msg_buffer, msg_size);
    }

    /* must be done from the same thread */
    zmq_close(m_socket);
}

void TrexRpcServerReqRes::_stop_rpc_thread() {
    /* by calling zmq_term we signal the blocked thread to exit */
    zmq_term(m_context);

}

void TrexRpcServerReqRes::handle_request(const uint8_t *msg, uint32_t msg_size) {
    Json::Reader  reader;
    Json::Value   request;
    std::string   response;

    /* parse the json request */
    bool rc = reader.parse( (const char *)msg, (const char *)msg + msg_size, request, false);
    if (!rc) {
        throw TrexRpcException("Unable to decode JSON RPC request: " + std::string( (const char *)msg, msg_size));
    }
    std::cout << request << std::endl;

    #if 0
    TrexJsonRpcRequest rpc_request(msg, msg_size);

    rpc_request->parse();
    rpc_request->execute();

    rpc_request->get_response(response);

    zmq_send(m_socket, response, response.size(), 0);
    #endif

    zmq_send(m_socket, "ACK", 3 ,0);
    
}
