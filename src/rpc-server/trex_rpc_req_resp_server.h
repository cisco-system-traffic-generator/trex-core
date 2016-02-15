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

#ifndef __TREX_RPC_REQ_RESP_API_H__
#define __TREX_RPC_REQ_RESP_API_H__

#include <trex_rpc_server_api.h>

/**
 * request-response RPC server
 * 
 * @author imarom (11-Aug-15)
 */
class TrexRpcServerReqRes : public TrexRpcServerInterface  {
public:

    TrexRpcServerReqRes(const TrexRpcServerConfig &cfg, std::mutex *lock = NULL);

    /* for test purposes - bypass the ZMQ and inject a message */
    std::string test_inject_request(const std::string &req);

protected:

    void _prepare();
    void _rpc_thread_cb();
    void _stop_rpc_thread();

    bool fetch_one_request(std::string &msg);
    void handle_request(const std::string &request);
    void process_request(const std::string &request, std::string &response);
    void process_request_raw(const std::string &request, std::string &response);
    void process_zipped_request(const std::string &request, std::string &response);

    void handle_server_error(const std::string &specific_err);

    void               *m_context;
    void               *m_socket;
};


/**
 * a mock req resp server (for tests)
 * 
 * @author imarom (03-Jan-16)
 */
class TrexRpcServerReqResMock : public TrexRpcServerReqRes {

public:
    TrexRpcServerReqResMock(const TrexRpcServerConfig &cfg);

    /* override the interface functions */
    virtual void start();
    virtual void stop();


};

#endif /* __TREX_RPC_REQ_RESP_API_H__ */

