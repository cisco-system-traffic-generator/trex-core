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

#include "trex_dp_core.h"

class TrexAstfDpCore : public TrexDpCore {

public:

    TrexAstfDpCore(uint8_t thread_id, CFlowGenListPerThread *core);
    ~TrexAstfDpCore(void);

    /**
     * return true if all the ports are idle
     */
    virtual bool are_all_ports_idle();

    /**
     * return true if a specific port is active
     */
    virtual bool is_port_active(uint8_t port_id);

    void start_transmit(double duration);
    void stop_transmit(void);
    void create_tcp_batch(void);
    void delete_tcp_batch(void);

protected:
    void report_finished(void);
    bool sync_barrier(void);
    CFlowGenListPerThread *m_flow_gen;
    bool no_close;
    bool sync_stop;

    virtual void start_scheduler() override;
};

#endif /* __TREX_ASTF_DP_CORE_H__ */

