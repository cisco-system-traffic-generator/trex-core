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
#include "trex_dp_core.h"

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

    void start_transmit(uint32_t profile_id, double duration);
    void stop_transmit(uint32_t profile_id, uint32_t stop_id);
    void update_rate(uint32_t profile_id, double ratio);
    void create_tcp_batch(uint32_t profile_id);
    void delete_tcp_batch(uint32_t profile_id);
    void parse_astf_json(uint32_t profile_id, std::string *profile_buffer, std::string *topo_buffer);

protected:
    virtual bool rx_for_idle();
    void report_finished(uint32_t profile_id = 0);
    void report_error(uint32_t profile_id, const std::string &error);
    bool sync_barrier();
    CFlowGenListPerThread *m_flow_gen;

    virtual void start_scheduler() override;

    void add_profile_duration(uint32_t profile_id, double duration);
    void get_scheduler_options(uint32_t profile_id, bool& disable_client, double& d_time_flow, double& d_phase);
    void start_profile_ctx(uint32_t profile_id, double duration);
    void stop_profile_ctx(uint32_t profile_id, uint32_t stop_id);

    /* scheduling profile parameters */
    struct profile_param {
        uint32_t    m_profile_id;
        double      m_duration;
    };
    struct {
        bool        m_flag; /* starting */

        uint32_t    m_profile_id;
        double      m_duration;
        std::vector<struct profile_param> m_params;

        bool        m_stopping;
    } m_sched_param;
};

#endif /* __TREX_ASTF_DP_CORE_H__ */

