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
#ifndef __TREX_ASTF_H__
#define __TREX_ASTF_H__

#include <stdint.h>
#include <vector>

#include "common/trex_stx.h"

class TrexAstfPort;
class CSyncBarrier;

typedef std::unordered_map<uint8_t, TrexAstfPort*> astf_port_map_t;

/**
 * TRex adavanced stateful interactive object
 * 
 */
class TrexAstf : public TrexSTX {
public:

    enum state_e {
        STATE_IDLE,
        STATE_WORK,
        AMOUNT_OF_STATES,
    };
    typedef std::vector<state_e> states_t;

    /** 
     * create ASTF object 
     */
    TrexAstf(const TrexSTXCfg &cfg);
    virtual ~TrexAstf();

    
    /**
     * ASTF control plane
     * 
     */
    void launch_control_plane();
    
    
    /**
     * shutdown ASTF
     * 
     */
    void shutdown();

    void dp_core_finished(int thread_id);

    /**
     * async data sent for ASTF
     * 
     */
    void publish_async_data();
    
    
    /**
     * create a ASTF DP core
     * 
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);

    /**
     * fetch astf port object by port ID
     *
     */
    TrexAstfPort *get_port_by_id(uint8_t port_id);

    /**
     * returns the port list as astf port objects
     */
    const astf_port_map_t &get_port_map() const {
        return *(astf_port_map_t *)&m_ports;
    }

    /**
     * Acquire context (and all ports)
     */
    void acquire_context(const std::string &user, bool force);

    /**
     * Release context (and all ports)
     */
    void release_context(void);

    /**
     * Profile-related:
     * check - check hash of loaded profile, true = matches (heavy profile will not be uploaded twice)
     * clear - clears profile JSON string (if any)
     * append - appends fragment to profile JSON string
     * load - loads JSON profile string to DB
     */
    bool profile_check(uint32_t total_size);
    bool profile_check(const std::string &hash);
    void profile_clear(void);
    void profile_append(const std::string &fragment);
    void profile_load(void);

    /**
     * Start transmit
     */
    void start_transmit(double duration, double mult,bool nc);

    /**
     * Stop transmit
     */
    bool stop_transmit(void);

    TrexOwner& get_owner(void) {
        return m_owner;
    }
    CSyncBarrier* get_barrier(void) {
        return m_sync_b;
    }

protected:
    bool stop_actions(void);
    void set_barrier(double timeout_sec);
    void send_message_to_all_dp(TrexCpToDpMsgBase *msg);

    void check_whitelist_states(const states_t &whitelist);
    //void check_blacklist_states(const states_t &blacklist);

    TrexOwner m_owner;
    uint8_t m_cur_state;
    int16_t m_active_cores;
    std::vector<std::string> states_names;
    std::string m_profile_buffer;
    CSyncBarrier *m_sync_b;
    CFlowGenList *m_fl;
    TrexMonitor * m_wd;
};


static inline TrexAstf * get_astf_object() {
    return (TrexAstf *)get_stx();
}


#endif /* __TREX_STL_H__ */

