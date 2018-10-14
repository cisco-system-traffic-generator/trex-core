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
        STATE_LOADED,
        STATE_PARSE, // by DP core 0, not cancelable
        STATE_BUILD, // by DP cores, not cancelable
        STATE_TX,
        STATE_CLEANUP, // by DP cores, not cancelable
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

    void all_dp_cores_finished();
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
    void release_context();

    /**
     * Profile-related:
     * check - check hash of loaded profile, true = matches (heavy profile will not be uploaded twice)
     * clear - clears profile JSON string (if any)
     * append - appends fragment to profile JSON string
     * loaded - move state from idle to loaded
     */
    bool profile_check_size(uint32_t total_size);
    bool profile_check_hash(const std::string &hash);
    void profile_clear();
    void profile_append(const std::string &fragment);
    void profile_set_loaded();

    /**
     * Start transmit
     */
    void start_transmit(double duration, double mult, bool nc);

    /**
     * Stop transmit
     */
    bool stop_transmit();

    TrexOwner& get_owner() {
        return m_owner;
    }
    CSyncBarrier* get_barrier() {
        return m_sync_b;
    }

    std::string &get_last_error() {
        return m_error;
    }

    void dp_core_error(int thread_id, const std::string &err);

protected:
    // message to DP involved:
    void parse();
    void build();
    void transmit();
    void cleanup();

    bool is_error() {
        return m_error.size() > 0;
    }

    void change_state(state_e new_state);
    void set_barrier(double timeout_sec);
    void send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg, bool clone = false);
    void send_message_to_all_dp(TrexCpToDpMsgBase *msg);

    void check_whitelist_states(const states_t &whitelist);
    //void check_blacklist_states(const states_t &blacklist);

    void ports_report_state(state_e state);

    TrexOwner       m_owner;
    state_e         m_state;
    int16_t         m_active_cores;
    std::vector<std::string> m_states_names;
    std::string     m_profile_buffer;
    CSyncBarrier   *m_sync_b;
    CFlowGenList   *m_fl;
    CParserOption  *m_opts;
    std::string     m_error;
    std::string     m_profile_hash;
    std::string     m_parsed_hash;
};


static inline TrexAstf * get_astf_object() {
    return (TrexAstf *)get_stx();
}


#endif /* __TREX_STL_H__ */

