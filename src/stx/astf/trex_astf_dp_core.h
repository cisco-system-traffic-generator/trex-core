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
#ifndef __TREX_ASTF_DP_CORE_H__
#define __TREX_ASTF_DP_CORE_H__

#include <string>
#include <algorithm>
#include "trex_dp_core.h"

struct profile_param {
    profile_id_t    m_profile_id;
    double          m_duration;
};

class TrexAstfDpCore : public TrexDpCore {

public:

    TrexAstfDpCore(uint8_t thread_id, CFlowGenListPerThread *core);
    ~TrexAstfDpCore();

    /**
     * return true if all the ports are idle
     */
    virtual bool are_all_ports_idle();

    /**
     * return true if a specific port is active
     */
    virtual bool is_port_active(uint8_t port_id);

    void start_transmit(profile_id_t profile_id, double duration);
    void stop_transmit(profile_id_t profile_id, uint32_t stop_id);
    void update_rate(profile_id_t profile_id, double ratio);
    void create_tcp_batch(profile_id_t profile_id, double factor);
    void delete_tcp_batch(profile_id_t profile_id, bool do_remove);
    void parse_astf_json(profile_id_t profile_id, std::string *profile_buffer, std::string *topo_buffer);
    void remove_astf_json(profile_id_t profile_id);

protected:
    virtual bool rx_for_idle();
    void report_finished(profile_id_t profile_id = 0);
    void report_error(profile_id_t profile_id, const std::string &error);
    bool sync_barrier();
    void report_dp_state();
    CFlowGenListPerThread *m_flow_gen;

    virtual void start_scheduler() override;

    void add_profile_duration(profile_id_t profile_id, double duration);
    bool is_profile_stop_id(profile_id_t profile_id, uint32_t stop_id);
    void set_profile_stop_id(profile_id_t profile_id, uint32_t stop_id);

    void get_scheduler_options(profile_id_t profile_id, bool& disable_client, double& d_time_flow, double& d_phase);
    void start_profile_ctx(profile_id_t profile_id, double duration);
    void stop_profile_ctx(profile_id_t profile_id, uint32_t stop_id);

    std::vector<struct profile_param> m_sched_param;

    enum profile_state_e {
        pSTATE_FREE,    // by create or delete_tcp_batch
        pSTATE_LOADED,  // by create_tcp_batch or stop_transmit
        pSTATE_STARTING,// by start_transmit
        pSTATE_ACTIVE,  // by start_profile_ctx
        pSTATE_WAIT     // by stop_transmit
    };

    std::unordered_map<profile_id_t, profile_state_e> m_profile_states;
    int m_active_profile_cnt;
    uint32_t m_profile_stop_id;

    bool is_profile(profile_id_t profile_id) {
        return m_profile_states.find(profile_id) != m_profile_states.end();
    }
    void create_profile_state(profile_id_t profile_id) {
        assert(!is_profile(profile_id));
        m_profile_states[profile_id] = pSTATE_FREE;
    }
    void remove_profile_state(profile_id_t profile_id) {
        if (is_profile(profile_id)) {
            m_profile_states.erase(profile_id);
        }
    }
    void set_profile_state(profile_id_t profile_id, profile_state_e state) {
        assert(is_profile(profile_id));

        if (m_profile_states[profile_id] != pSTATE_ACTIVE && state == pSTATE_ACTIVE) {
            m_active_profile_cnt++;
        }
        if (m_profile_states[profile_id] == pSTATE_ACTIVE && state != pSTATE_ACTIVE) {
            m_active_profile_cnt--;
        }

        m_profile_states[profile_id] = state;
    }
    profile_state_e get_profile_state(profile_id_t profile_id) {
        assert(is_profile(profile_id));
        return m_profile_states[profile_id];
    }
    void set_profile_active(profile_id_t profile_id);
    void set_profile_stopping(profile_id_t profile_id);
    int active_profile_cnt() { return m_active_profile_cnt; }
    int profile_cnt() { return m_profile_states.size(); }

    uint32_t profile_stop_id() {
        if (++m_profile_stop_id == 0) {
            ++m_profile_stop_id;
        }
        return m_profile_stop_id;
    }

    void set_profile_stop_event(profile_id_t profile_id);
    void clear_profile_stop_event_all();

public:
    void on_profile_stop_event(profile_id_t profile_id);
};

#endif /* __TREX_ASTF_DP_CORE_H__ */

