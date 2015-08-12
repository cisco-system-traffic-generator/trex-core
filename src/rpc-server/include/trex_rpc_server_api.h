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

#ifndef __TREX_RPC_SERVER_API_H__
#define __TREX_RPC_SERVER_API_H__

#include <stdint.h>
#include <vector>
#include <thread>
#include <string>
#include <stdexcept>

/**
 * generic exception for RPC errors
 * 
 */
class TrexRpcException : public std::runtime_error 
{
public:
    TrexRpcException(const std::string &what) : std::runtime_error(what) {
    }
};

/* forward decl. of class */
class TrexRpcServerInterface;

/**
 * servers array
 * 
 * @author imarom (12-Aug-15)
 */
class TrexRpcServerArray {
public:
   /**
    * different types the RPC server supports
    */
    enum protocol_type_e {
        RPC_PROT_TCP
    };

    TrexRpcServerArray(protocol_type_e protocol, uint16_t port);
    ~TrexRpcServerArray();

    void start();
    void stop();

private:
    std::vector<TrexRpcServerInterface *>  m_servers;
    protocol_type_e                        m_protocol;
    uint16_t                               m_port;
};

/**
 * generic type RPC server instance
 * 
 * @author imarom (12-Aug-15)
 */
class TrexRpcServerInterface {
public:
  
    TrexRpcServerInterface(TrexRpcServerArray::protocol_type_e protocol, uint16_t port);
    virtual ~TrexRpcServerInterface();

    /**
     * starts the server
     * 
     */
    void start();

    /**
     * stops the server
     * 
     */
    void stop();

    /**
     * return TRUE if server is active
     * 
     */
    bool is_running();

protected:
    /**
     * instances implement this
     * 
     */
    virtual void _rpc_thread_cb() = 0;
    virtual void _stop_rpc_thread()  = 0;

    TrexRpcServerArray::protocol_type_e  m_protocol;
    uint16_t                             m_port;
    bool                                 m_is_running;
    std::thread                          *m_thread;
};

#endif /* __TREX_RPC_SERVER_API_H__ */
