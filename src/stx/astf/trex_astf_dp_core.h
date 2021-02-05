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
#include "trex_astf_mbuf_redirect.h"
#include "trex_dp_core.h"
#include "trex_messaging.h"

class CAstfDB;

typedef struct client_tunnel_data {
    uint32_t client_ip;
    uint32_t teid;
    uint32_t version;
    uint8_t  type;
    union
    {
      uint32_t src_ipv4;
      uint8_t  src_ip[16];
    }u1;

    union
    {
      uint32_t dst_ipv4;
      uint8_t  dst_ip[16];
    }u2;
}client_tunnel_data_t;

struct profile_param {
    profile_id_t    m_profile_id;
    double          m_duration;
    bool            m_nc_flow_close;
};

class TrexAstfDpCore : public TrexDpCore {

public:

    TrexAstfDpCore(uint8_t thread_id, CFlowGenListPerThread *core);
    ~TrexAstfDpCore();

    /**
     * This stop is used to abort gracefully.
     */
    virtual void stop() override;

    /**
     * return true if all the ports are idle
     */
    virtual bool are_all_ports_idle();

    /**
     * return true if a specific port is active
     */
    virtual bool is_port_active(uint8_t port_id);

    void start_transmit(profile_id_t profile_id, double duration, bool nc);
    void stop_transmit(profile_id_t profile_id, uint32_t stop_id, bool set_nc);
    void stop_transmit(profile_id_t profile_id);
    void update_rate(profile_id_t profile_id, double ratio);
    void create_tcp_batch(profile_id_t profile_id, double factor, CAstfDB* astf_db);
    void delete_tcp_batch(profile_id_t profile_id, bool do_remove, CAstfDB* astf_db);
    void parse_astf_json(profile_id_t profile_id, std::string *profile_buffer, std::string *topo_buffer, CAstfDB* astf_db);
    void remove_astf_json(profile_id_t profile_id, CAstfDB* astf_db);

    void scheduler(bool activate);

    void set_service_mode(bool enabled, bool filtered, uint8_t mask);
    void activate_client(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool activate, bool is_range);
    void client_lookup_and_activate(uint32_t client, bool activate);
    bool get_client_stats(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool is_range, MsgReply<Json::Value> &reply);
    Json::Value client_data_to_json(void *ip_info);
    void update_tunnel_for_client(CAstfDB* astf_db, std::vector<client_tunnel_data_t> msg_data, uint8_t tunnel_type);

protected:
    virtual bool rx_for_idle();
    void report_finished(profile_id_t profile_id = 0);
    void report_finished_partial(profile_id_t profile_id);
    void report_profile_ctx(profile_id_t profile_id = 0);
    void report_error(profile_id_t profile_id, const std::string &error);
    bool sync_barrier();
    void report_dp_state();
    CFlowGenListPerThread *m_flow_gen;

    virtual void start_scheduler() override;

    void add_profile_duration(profile_id_t profile_id, double duration);
    bool is_profile_stop_id(profile_id_t profile_id, uint32_t stop_id);
    void set_profile_stop_id(profile_id_t profile_id, uint32_t stop_id);

    void get_scheduler_options(profile_id_t profile_id, bool& disable_client, double& d_time_flow, double& d_phase);
    void start_profile_ctx(profile_id_t profile_id, double duration, bool nc);
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
    int                                               m_active_profile_cnt;
    uint32_t                                          m_profile_stop_id;
    MbufRedirectCache*                                m_mbuf_redirect_cache; // Mbuf redirect cache only in case RSS ASTF software mode.

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
    void set_profile_stopping(profile_id_t profile_id, bool stop_all=true);
    int active_profile_cnt() { return m_active_profile_cnt; }
    int profile_cnt() { return m_profile_states.size(); }

    void set_profile_nc(profile_id_t profile_id, bool nc);
    bool get_profile_nc(profile_id_t profile_id);

    uint32_t profile_stop_id() {
        if (++m_profile_stop_id == 0) {
            ++m_profile_stop_id;
        }
        return m_profile_stop_id;
    }

    void set_profile_stop_event(profile_id_t profile_id);
    void clear_profile_stop_event_all();

    /**
     * Get mbuf redirect cache.
     * @return MbufRedirectCache
     *    Mbuf caching object to redirect packets to the correct DP.
     */
    MbufRedirectCache* get_mbuf_redirect_cache() override {
        return m_mbuf_redirect_cache;
    }

public:
    void on_profile_stop_event(profile_id_t profile_id);
};

#endif /* __TREX_ASTF_DP_CORE_H__ */

