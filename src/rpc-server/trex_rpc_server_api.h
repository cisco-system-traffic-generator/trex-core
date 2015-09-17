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
#include <trex_rpc_exception_api.h>

class TrexRpcServerInterface;

/**
 * defines a configuration of generic RPC server
 * 
 * @author imarom (17-Aug-15)
 */
class TrexRpcServerConfig {
public:

    enum rpc_prot_e {
        RPC_PROT_TCP
    };

    TrexRpcServerConfig(rpc_prot_e protocol, uint16_t port) : m_protocol(protocol), m_port(port) {

    }

    uint16_t get_port() {
        return m_port;
    }

    rpc_prot_e get_protocol() {
        return m_protocol;
    }

private:
    rpc_prot_e       m_protocol;
    uint16_t         m_port;
};

/**
 * generic type RPC server instance
 * 
 * @author imarom (12-Aug-15)
 */
class TrexRpcServerInterface {
public:
  
    TrexRpcServerInterface(const TrexRpcServerConfig &cfg, const std::string &name);
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
     * set verbose on or off
     * 
     */
    void set_verbose(bool verbose);

    /**
     * return TRUE if server is active
     * 
     */
    bool is_running();

    /**
     * is the server verbose or not
     * 
     */
    bool is_verbose();

protected:
    /**
     * instances implement this
     * 
     */
    virtual void _rpc_thread_cb()                   = 0;
    virtual void _stop_rpc_thread()                 = 0;

    /**
     * prints a verbosed message (if enabled)
     * 
     */
    void verbose_msg(const std::string &msg);

    /**
     * prints a verbose message with a JSON to be converted to 
     * string 
     * 
     */
    void verbose_json(const std::string &msg, const std::string &json_str);

    TrexRpcServerConfig                  m_cfg;
    bool                                 m_is_running;
    bool                                 m_is_verbose;
    std::thread                          *m_thread;
    std::string                          m_name;
};

/**
 * TREX RPC server 
 * may contain serveral types of RPC servers 
 * (request response, async and etc.) 
 * 
 * @author imarom (12-Aug-15)
 */
class TrexRpcServer {
public:
  
    /* currently only request response server config is required */
    TrexRpcServer(const TrexRpcServerConfig &req_resp_cfg);
    ~TrexRpcServer();

    /**
     * starts the RPC server
     * 
     * @author imarom (19-Aug-15)
     */
    void start();

    /**
     * stops the RPC server
     * 
     * @author imarom (19-Aug-15)
     */
    void stop();

    void set_verbose(bool verbose);

    static const std::string &get_server_uptime() {
        return s_server_uptime;
    }


   

private:
    static std::string generate_handler();

    std::vector<TrexRpcServerInterface *>   m_servers;
    bool                                    m_verbose;
    static const std::string                s_server_uptime;

    static std::string                      s_owner;
    static std::string                      s_owner_handler;
};

#endif /* __TREX_RPC_SERVER_API_H__ */
