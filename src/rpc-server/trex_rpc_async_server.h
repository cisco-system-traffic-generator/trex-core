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

#ifndef __TREX_RPC_ASYNC_SERVER_H__
#define __TREX_RPC_ASYNC_SERVER_H__

#include <trex_rpc_server_api.h>
#include <trex_stateless_port.h>

/**
 * async RPC server
 * 
 * @author imarom (11-Aug-15)
 */
class TrexRpcServerAsync : public TrexRpcServerInterface  {
public:

    TrexRpcServerAsync(const TrexRpcServerConfig &cfg);

protected:
    void _prepare();
    void _rpc_thread_cb();
    void _stop_rpc_thread();

private:

    void handle_server_error(const std::string &specific_err);

    static const int    RPC_MAX_MSG_SIZE = (20 * 1024);
    void               *m_context;
    void               *m_socket;
    uint8_t             m_msg_buffer[RPC_MAX_MSG_SIZE];
};


#endif /* __TREX_RPC_ASYNC_SERVER_H__ */

