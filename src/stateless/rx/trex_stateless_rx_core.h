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
#include "os_time.h"
#include "pal/linux/sanb_atomic.h"
#include "utl_cpuu.h"

class TrexStatelessCpToRxMsgBase;

class CCPortLatencyStl {
 public:
    void reset();

 public:
    rx_per_flow_t m_rx_pg_stat[MAX_FLOW_STATS];
    rx_per_flow_t m_rx_pg_stat_payload[MAX_FLOW_STATS_PAYLOAD];
};

class CLatencyManagerPerPortStl {
public:
     CCPortLatencyStl     m_port;
     CPortLatencyHWBase * m_io;
};

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

class CRFC2544Info {
 public:
    void create();
    void stop();
    void reset();
    void export_data(rfc2544_info_t_ &obj);
    inline void add_sample(double stime) {
        m_latency.Add(stime);
        m_jitter.calc(stime);
    }
    inline void sample_period_end() {
        m_latency.update();
    }
    inline uint32_t get_seq() {return m_seq;}
    inline void set_seq(uint32_t val) {m_seq = val;}
    inline void inc_seq_err(uint64_t val) {m_seq_err += val;}
    inline void dec_seq_err() {if (m_seq_err >0) {m_seq_err--;}}
    inline void inc_seq_err_too_big() {m_seq_err_events_too_big++;}
    inline void inc_seq_err_too_low() {m_seq_err_events_too_low++;}
    inline void inc_dup() {m_dup++;}
    inline void inc_ooo() {m_ooo++;}
    inline uint16_t get_exp_flow_seq() {return m_exp_flow_seq;}
    inline void set_exp_flow_seq(uint16_t flow_seq) {m_exp_flow_seq = flow_seq;}
    inline uint16_t get_prev_flow_seq() {return m_prev_flow_seq;}
    inline bool no_flow_seq() {return (m_exp_flow_seq == FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ) ? true : false;}
 private:
    uint32_t m_seq; // expected next seq num
    CTimeHistogram  m_latency; // latency info
    CJitter         m_jitter;
    uint64_t m_seq_err; // How many packet seq num gaps we saw (packets lost or out of order)
    uint64_t m_seq_err_events_too_big; // How many packet seq num greater than expected events we had
    uint64_t m_seq_err_events_too_low; // How many packet seq num lower than expected events we had
    uint64_t m_ooo; // Packets we got with seq num lower than expected (We guess they are out of order)
    uint64_t m_dup; // Packets we got with same seq num
    uint16_t m_exp_flow_seq; // flow sequence number we should see in latency header
    // flow sequence number previously used with this id. We use this to catch packets arriving late from an old flow
    uint16_t m_prev_flow_seq;
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
    int get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset
                     , TrexPlatformApi::driver_stat_cap_e type);
    int get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset);
    void work() {m_state = STATE_WORKING;}
    void idle() {m_state = STATE_IDLE;}
    void quit() {m_state = STATE_QUIT;}
    bool is_working() const {return (m_ack_start_work_msg == true);}
    void set_working_msg_ack(bool val);
    double get_cpu_util();
    void update_cpu_util();


 private:
    void handle_cp_msg(TrexStatelessCpToRxMsgBase *msg);
    bool periodic_check_for_cp_messages();
    void tickle();
    void idle_state_loop();
    void handle_rx_pkt(CLatencyManagerPerPortStl * lp, rte_mbuf_t * m);
    void handle_rx_queue_msgs(uint8_t thread_id, CNodeRing * r);
    void flush_rx();
    int try_rx();
    void try_rx_queues();
    bool is_flow_stat_id(uint16_t id);
    bool is_flow_stat_payload_id(uint16_t id);
    uint16_t get_hw_id(uint16_t id);

 private:

    TrexMonitor     m_monitor;

    uint32_t m_max_ports;
    bool m_has_streams;
    CLatencyManagerPerPortStl m_ports[TREX_MAX_PORTS];
    state_e   m_state;
    CNodeRing *m_ring_from_cp;
    CNodeRing *m_ring_to_cp;
    CCpuUtlDp m_cpu_dp_u;
    CCpuUtlCp m_cpu_cp_u;
    // Used for acking "work" (go out of idle) messages from cp
    volatile bool m_ack_start_work_msg __rte_cache_aligned;
    CRFC2544Info m_rfc2544[MAX_FLOW_STATS_PAYLOAD];
};
#endif
