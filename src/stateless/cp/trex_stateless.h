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
#ifndef __TREX_STATELESS_H__
#define __TREX_STATELESS_H__

#include <stdint.h>
#include <string>
#include <stdexcept>

#include <mutex>

#include "trex_stream.h"
#include "trex_stateless_port.h"
#include "trex_rpc_server_api.h"

#include "publisher/trex_publisher.h"
#include "internal_api/trex_platform_api.h"

#include "flow_stat.h"


#include "trex_exception.h"
#include "trex_api_class.h"

class TrexStatelessPort;
class TrexWatchDog;

/**
 * unified stats
 * 
 * @author imarom (06-Oct-15)
 */
class TrexStatelessStats {
public:
    TrexStatelessStats() {
        m_stats = {0};
    }

    struct {
        double m_cpu_util;

        double m_tx_bps;
        double m_rx_bps;
       
        double m_tx_pps;
        double m_rx_pps;
       
        uint64_t m_total_tx_pkts;
        uint64_t m_total_rx_pkts;
       
        uint64_t m_total_tx_bytes;
        uint64_t m_total_rx_bytes;
       
        uint64_t m_tx_rx_errors;
    } m_stats;
};


/**
 * config object for stateless object
 * 
 * @author imarom (08-Oct-15)
 */
class TrexStatelessCfg {
public:
    /* default values */
    TrexStatelessCfg() {
        m_port_count          = 0;
        m_rpc_req_resp_cfg    = NULL;
        m_rpc_server_verbose  = false;
        m_platform_api        = NULL;
        m_publisher           = NULL;
    }

    const TrexRpcServerConfig  *m_rpc_req_resp_cfg;
    const TrexPlatformApi      *m_platform_api;
    bool                        m_rpc_server_verbose;
    uint8_t                     m_port_count;
    TrexPublisher              *m_publisher;

};

/**
 * defines the TRex stateless operation mode
 * 
 */
class TrexStateless {
public:

    /**
     * configure the stateless object singelton 
     * reconfiguration is not allowed
     * an exception will be thrown
     */
    TrexStateless(const TrexStatelessCfg &cfg);
    ~TrexStateless();

    /**
     * starts the control plane side
     * 
     */
    void launch_control_plane();

    /**
     * launch on a single DP core
     * 
     */
    void launch_on_dp_core(uint8_t core_id);

    TrexStatelessPort * get_port_by_id(uint8_t port_id);
    uint8_t             get_port_count();

    uint8_t             get_dp_core_count();


    /**
     * shutdown the server
     */
    void shutdown();

    /**
     * fetch xstats names (keys of dict)
     * 
     */
    void encode_xstats_names(Json::Value &global);

    /**
     * fetch xstats values
     * 
     */
    void encode_xstats_values(Json::Value &global);

    /**
     * fetch all the stats
     * 
     */
    void               encode_stats(Json::Value &global);

    /**
     * generate a snapshot for publish
     */
    void generate_publish_snapshot(std::string &snapshot);

    const TrexPlatformApi * get_platform_api() {
        return (m_platform_api);
    }

    TrexPublisher * get_publisher() {
        return m_publisher;
    }

    const std::vector <TrexStatelessPort *> get_port_list() {
        return m_ports;
    }

    TrexRpcServer * get_rpc_server() {
        return m_rpc_server;
    }

    const std::string & verify_api(APIClass::type_e type, int major, int minor) {
        return m_api_classes[type].verify_api(major, minor);
    }

    const std::string & get_api_handler(APIClass::type_e type) {
        return m_api_classes[type].get_api_handler();
    }

    CFlowStatRuleMgr                     m_rx_flow_stat;

protected:

    /* no copy or assignment */
    TrexStateless(TrexStateless const&)      = delete;  
    void operator=(TrexStateless const&)     = delete;

    /* RPC server array */
    TrexRpcServer                        *m_rpc_server;

    /* ports */
    std::vector <TrexStatelessPort *>    m_ports;
    uint8_t                              m_port_count;

    /* platform API */
    const TrexPlatformApi                *m_platform_api;

    TrexPublisher                        *m_publisher;

    /* API */
    APIClass                              m_api_classes[APIClass::API_CLASS_TYPE_MAX];

};

/**
 * an anchor function
 * 
 * @author imarom (25-Oct-15)
 * 
 * @return TrexStateless& 
 */
TrexStateless * get_stateless_obj();
CRxCoreStateless * get_rx_sl_core_obj();

#endif /* __TREX_STATELESS_H__ */

