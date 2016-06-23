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
#include <mutex>
#include <thread>
#include <string>
#include <stdexcept>
#include <trex_rpc_exception_api.h>
#include <json/json.h>
#include "trex_watchdog.h"

class TrexRpcServerInterface;
class TrexRpcServerReqRes;

/**
 * defines a configuration of generic RPC server
 * 
 * @author imarom (17-Aug-15)
 */
class TrexRpcServerConfig {
public:

    enum rpc_prot_e {
        RPC_PROT_TCP,
        RPC_PROT_MOCK
    };

    TrexRpcServerConfig(rpc_prot_e protocol, uint16_t port, std::mutex *lock) {
        m_protocol  = protocol;
        m_port      = port;
        m_lock      = lock;
    }

    uint16_t get_port() const {
        return m_port;
    }

    rpc_prot_e get_protocol() const {
        return m_protocol;
    }

private:
    rpc_prot_e       m_protocol;
    uint16_t         m_port;

public:
    std::mutex       *m_lock;
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
    virtual void start();

    /**
     * stops the server
     * 
     */
    virtual void stop();

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
    virtual void _prepare()                         = 0;
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
    std::mutex                           *m_lock;
    std::mutex                           m_dummy_lock;
    TrexMonitor                          m_monitor;
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
  
    /* creates the collection of servers using configurations */
    TrexRpcServer(const TrexRpcServerConfig *req_resp_cfg);

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


    /**
     * allow injecting of a JSON and get a response
     * 
     * @author imarom (27-Dec-15)
     * 
     * @return std::string 
     */
    std::string test_inject_request(const std::string &request_str);
   

private:
    static std::string generate_handler();

    std::vector<TrexRpcServerInterface *>   m_servers;
    // an alias to the req resp server
    TrexRpcServerReqRes                     *m_req_resp;

    bool                                    m_verbose;
    static const std::string                s_server_uptime;

    static std::string                      s_owner;
    static std::string                      s_owner_handler;
};

#endif /* __TREX_RPC_SERVER_API_H__ */
