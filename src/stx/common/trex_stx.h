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
#ifndef __TREX_STX_H__
#define __TREX_STX_H__

#include "rpc-server/trex_rpc_server_api.h"
#include "trex_port.h"
#include "trex_rx_core.h"

class TrexCpToRxMsgBase;
class TrexPublisher;
class TrexDpCore;
class CRxCore;
class CFlowGenListPerThread;

/**
 *  
 * STX object related information 
 *  
 * STX is a polymorphic object used by: 
 * 1. STF 
 * 2. STL 
 * 3. ASTF 
 * 4. ASTF batch
 *  
 * all those objects provide common interface with 
 * more specific functions in the specific object 
 *  
 * but the common interface is always STX 
 *  
 */


/**
 * config object for STX object
 */
class TrexSTXCfg {
public:
    
    TrexSTXCfg() {
        m_publisher = nullptr;
    }
    
    TrexSTXCfg(const TrexRpcServerConfig &rpc_cfg,
               const CRxSlCfg &rx_cfg,
               TrexPublisher *publisher) : m_rpc_req_resp_cfg(rpc_cfg), m_rx_cfg(rx_cfg), m_publisher(publisher) {
    }

    TrexRpcServerConfig           m_rpc_req_resp_cfg;
    CRxSlCfg                      m_rx_cfg;
    TrexPublisher                *m_publisher;
};


/**
 * abstract class (meant for derived implementation) 
 *  
 * dervied by STL, STF, ASTF and ASTF batch
 * 
 * @author imarom (8/30/2017)
 */
class TrexSTX {
public:

    /**
     * create a STX object with configuration
     * 
     */
    TrexSTX(const TrexSTXCfg &cfg);
    
    
    /**
     * pure virtual DTOR to enforce deriviation
     */
    virtual ~TrexSTX();
    

    /**
     * starts the control plane side
     */
    virtual void launch_control_plane() = 0;
    
    
    /**
     * shutdown the server 
     */
    virtual void shutdown() = 0;
    

    /**
     * create a DP core object
     * 
     */
    virtual TrexDpCore * create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) = 0;

    
    /**
     * publish config-specific data
     * 
     */
    virtual void publish_async_data() = 0;


    /**
     * fast path tick
     * 
     */
    void fastpath_tick() {
        check_for_dp_messages();
        
        /* update RX core util. */
        for (int i = 0; i < 1000; i++) {
            m_rx->update_cpu_util();
            rte_pause();
        }
    }
    
    
    /**
     * can be implemented by the derived class for slowpath tick
     * 
     */
    virtual void slowpath_tick() {}
    
 

    /**
     * fills ignored stats on 'stat'
     * 
     */
    void get_ignore_stats(int port_id, CRXCoreIgnoreStat &ign_stats, bool get_diff) {
        get_rx()->get_ignore_stats(port_id, ign_stats, get_diff);
    }
    
    
    /**
     * send a message to the RX core
     */
    void send_msg_to_rx(TrexCpToRxMsgBase *msg) const;
    
      
    /**
     * returns the port count
     */
    uint8_t get_port_count() const;
    
    
   /**
    * returns the TRex port (interactive)
    * by ID
    */
    TrexPort * get_port_by_id(uint8_t port_id);
    
    
    /**
     * returns a list of CP ports
     */
    const std::vector <TrexPort *> get_port_list() {
        return m_ports;
    }

    
    TrexRpcServer * get_rpc_server() {
        return &m_rpc_server;
    }
    

    TrexPublisher * get_publisher() {
        return m_cfg.m_publisher;
    }
    

    TrexRxCore *get_rx() {
        return m_rx;
    }

 
    /**
     * check for messages from any core
     * 
     */
    void check_for_dp_messages();
    
protected:

    void check_for_dp_message_from_core(int thread_id);
    void send_msg_to_all_dp(TrexCpToDpMsgBase *msg);
    
    /* no copy or assignment */
    TrexSTX(TrexSTX const&)              = delete;  
    void operator=(TrexSTX const&)       = delete;

    /* RPC server array */
    TrexRpcServer               m_rpc_server;

    /* ports */
    std::vector<TrexPort *>     m_ports;
    
    /* RX */
    TrexRxCore                 *m_rx;
    const TrexSTXCfg            m_cfg;
    
    uint8_t                     m_dp_core_count;

};


/**
 * get the STX object - implemented by main_dpdk or 
 * simulator 
 */
TrexSTX * get_stx();

#endif /* __TREX_STX_H__ */
