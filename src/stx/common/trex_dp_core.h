/*
 Itay Marom
 Hanoch Haim
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

#ifndef __TREX_DP_CORE_H__
#define __TREX_DP_CORE_H__

#include <stdint.h>
#include "msg_manager.h"
#include "pal_utl.h"
#include "mbuf.h"
#include "dpdk_port_map.h"


class CFlowGenListPerThread;
class TrexCpToDpMsgBase;
class MbufRedirectCache;

#define NUM_PORTS_PER_CORE 2

/**
 * DP core abstract class 
 * derived and implemented by STL and ASTF 
 * 
 */
class TrexDpCore {
public:

    /* states */
    enum state_e {
        STATE_IDLE,
        STATE_STARTING,
        STATE_TRANSMITTING,
        STATE_STOPPING,
        STATE_PCAP_TX,
        STATE_TERMINATE
    };

    
    TrexDpCore(uint32_t thread_id, CFlowGenListPerThread *core, state_e init_state);
    virtual ~TrexDpCore() {}
    
    /**
     * launch the DP core 
     * if once is true,  
     * 
     */
    void start();
    
    
    /**
     * launch the DP core but 
     * exit after one iteration 
     * (for simulation) 
     *  
     */
    void start_once();
    
    
    /**
     * stop the DP core
     */
    virtual void stop();
    
    
    /**
     * a barrier for the DP core
     * 
     */
    void barrier(uint8_t port_id, uint32_t profile_id, int event_id);
    

    /**
     * return true if core has any pending messages from CP
     * 
     */
    bool are_any_pending_cp_messages() {
        return (!m_ring_from_cp->isEmpty());
    }

    
    /**
     * check for and handle messages from CP
     * 
     * @author imarom (27-Oct-15)
     */
    bool periodic_check_for_cp_messages() {
        // doing this inline for performance reasons

        /* fast path */
        m_ring_to_cp->Reschedule();

        if ( likely ( m_ring_from_cp->isEmpty() ) ) {
            return false;
        }

        while ( true ) {
            CGenNode * node = NULL;
            if (m_ring_from_cp->Dequeue(node) != 0) {
                break;
            }
            assert(node);

            TrexCpToDpMsgBase * msg = (TrexCpToDpMsgBase *)node;
            handle_cp_msg(msg);
        }

        return true;

    }
    
    
    state_e get_state() {
        return m_state;
    }
    
    
    /**
     * return true if all the ports are idle
     */
    virtual bool are_all_ports_idle() = 0;
  
      
    /**
     * return true if a specific port is active
     */
    virtual bool is_port_active(uint8_t port_id) = 0;

    
    virtual void rx_handle_packet(int dir,rte_mbuf_t * m,bool is_idle,
                                  tvpid_t port_id){
        assert(0);
    }

    /**
     * Get the mbuf redirect cache for software RSS.
     */
    virtual MbufRedirectCache* get_mbuf_redirect_cache() {
        /* Only ASTF implements this function but the core object is polymorphic
        hence the base class needs to declare */
        assert(0);
    }

protected:

    /**
     * per implementation start scheduler
     */
    virtual void start_scheduler() = 0;

    /* read from RX  */
    virtual bool rx_for_idle(void);

    virtual bool is_hot_state();

    void idle_state_loop();
    
    /**
     * handles a CP to DP message
     * 
     * @author imarom (27-Oct-15)
     * 
     * @param msg 
     */
    void handle_cp_msg(TrexCpToDpMsgBase *msg);

    
    void add_global_duration(double duration);
    
    
    /* thread id */
    uint8_t                  m_thread_id;


    /* global state */
    state_e                  m_state;
    
    /* pointer to the main object */
    CFlowGenListPerThread   *m_core;
    
    /* messaging */    
    CNodeRing               *m_ring_from_cp;
    CNodeRing               *m_ring_to_cp;

};


#endif /* __TREX_DP_CORE_H__ */

