/*
  Ido Barnea
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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
#ifndef __TREX_STATELESS_RX_CORE_H__
#define __TREX_STATELESS_RX_CORE_H__
#include <stdint.h>
#include "latency.h"
#include "utl_cpuu.h"

class TrexStatelessCpToRxMsgBase;

class CRxSlCfg {
 public:
    CRxSlCfg (){
        m_max_ports = 0;
        m_cps = 0.0;
    }

 public:
    uint32_t             m_max_ports;
    double               m_cps;
    CPortLatencyHWBase * m_ports[TREX_MAX_PORTS];
};

class CRxCoreStateless {
    enum state_e {
        STATE_IDLE,
        STATE_WORKING,
        STATE_QUIT
    };

 public:
    void start();
    void create(const CRxSlCfg &cfg);
    void reset_rx_stats(uint8_t port_id);
    int get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset);
    void work() {m_state = STATE_WORKING;}
    void idle() {m_state = STATE_IDLE;}
    void quit() {m_state = STATE_QUIT;}
    double get_cpu_util();

 private:
    void handle_cp_msg(TrexStatelessCpToRxMsgBase *msg);
    bool periodic_check_for_cp_messages();
    void idle_state_loop();
    void handle_rx_pkt(CLatencyManagerPerPort * lp, rte_mbuf_t * m);
    void handle_rx_queue_msgs(uint8_t thread_id, CNodeRing * r);
    int try_rx();
    void try_rx_queues();
    bool is_flow_stat_id(uint16_t id);
    uint16_t get_hw_id(uint16_t id);

 private:
    uint32_t m_max_ports;
    bool m_has_streams;
    CLatencyManagerPerPort m_ports[TREX_MAX_PORTS];
    state_e   m_state; /* state of all ports */
    CNodeRing *m_ring_from_cp;
    CNodeRing *m_ring_to_cp;
    CCpuUtlDp m_cpu_dp_u;
    CCpuUtlCp m_cpu_cp_u;
};
#endif
