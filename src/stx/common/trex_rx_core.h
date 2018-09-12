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
#ifndef __TREX_RX_CORE_H__
#define __TREX_RX_CORE_H__
#include <stdint.h>
#include <map>

#include "stateful_rx_core.h"
#include "os_time.h"
#include "utl_cpuu.h"
#include "trex_rx_port_mngr.h"
#include "trex_capture.h"
#include "trex_rx_tx.h"

class TrexCpToRxMsgBase;

typedef std::map<uint8_t, RXPortManager> rx_port_mngr_t;

class CCPortLatencyStl {
 public:
    void reset();

 public:
    rx_per_flow_t m_rx_pg_stat[MAX_FLOW_STATS];
    rx_per_flow_t m_rx_pg_stat_payload[MAX_FLOW_STATS_PAYLOAD];
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

class CRxCoreErrCntrs {
    friend CRxCore;

 public:
    uint64_t get_bad_header() {return m_bad_header;}
    uint64_t get_old_flow() {return m_old_flow;}
    CRxCoreErrCntrs() {
        reset();
    }
    void reset() {
        m_bad_header = 0;
        m_old_flow = 0;
    }

 public:
    uint64_t m_bad_header;
    uint64_t m_old_flow;
};

/**
 * RX core
 * 
 */
class CRxCore : public TrexRxCore {
    
    /**
     * core states 
     *  
     * STATE_COLD - will sleep until a packet arrives 
     *              then it will move to a faster pace
     *              until no packet arrives for some time
     *  
     * STATE_HOT  - 100% checking for packets (latency check)
     */
    enum state_e {
        STATE_COLD,
        STATE_HOT,
        STATE_QUIT
    };

 public:
     
    CRxCore() {
        m_is_active = false;
    }
    ~CRxCore();
    
    void start();
    void create(const CRxSlCfg &cfg);
    void reset_rx_stats(uint8_t port_id);
    int get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset
                     , TrexPlatformApi::driver_stat_cap_e type);
    int get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch);
    int get_rx_err_cntrs(CRxCoreErrCntrs *rx_err);


    void quit() {m_state = STATE_QUIT;}
    bool is_working() const {return (m_state != STATE_QUIT);}
    double get_cpu_util();
    void update_cpu_util();

    
    const TrexPktBuffer *get_rx_queue_pkts(uint8_t port_id);

    bool tx_pkt(rte_mbuf_t *m, uint8_t tx_port_id);
    bool tx_pkt(const std::string &pkt, uint8_t tx_port_id);

    /**
     * start RX queueing of packets
     * 
     */
    void start_queue(uint8_t port_id, uint64_t size);
    void stop_queue(uint8_t port_id);

    /**
     * start proxifying of CAPWAP traffic between WLC and STF TRex
     * 
     */
    bool start_capwap_proxy(uint8_t port_id, uint8_t pair_port_id, bool is_wireless_side, Json::Value capwap_map);
    void stop_capwap_proxy(uint8_t port_id);

    /**
     * enable latency feature for RX packets
     * will be apply to all ports
     */
    void enable_latency();
    void disable_latency();

    RXPortManager &get_rx_port_mngr(uint8_t port_id);
    bool has_port(const std::string &mac_buf);

    bool is_cfg_state(uint8_t port_id);

    /**
     * fetch the ignored stats for a port
     * 
     */
    void get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff);

    /**
     * returns the current rate that 
     * the RX core handles packets 
     * 
     */
    float get_pps_rate() {
        return m_rx_pps.add(m_rx_pkts);
    }
    
    /**
     * sends a list of packets using a queue (delayed) with IPG
     */
    uint32_t tx_pkts(int port_id, const std::vector<std::string> &pkts, uint32_t ipg_usec);
    
    /**
     * sends a packet through the TX queue assigned to the RX core
     */
    bool tx_pkt(int port_id, const std::string &pkt);
    
    /**
     * returns true if the RX core is active
     * 
     */
    bool is_active() {
        return m_is_active;
    }

 private:
    void handle_cp_msg(TrexCpToRxMsgBase *msg);

    bool periodic_check_for_cp_messages();

    void tickle();

    /* states */
    void hot_state_loop();
    void cold_state_loop();
    void init_work_stage();
    bool work_tick();
    
    void recalculate_next_state();
    bool is_latency_active();
    bool should_be_hot();

    void handle_rx_queue_msgs(uint8_t thread_id, CNodeRing * r);
    void handle_work_stage();

    int process_all_pending_pkts(bool flush_rx = false);

    void flush_all_pending_pkts() {
        process_all_pending_pkts(true);
    }

    void try_rx_queues();
    
  
    
 private:
    TrexMonitor      m_monitor;
    uint32_t         m_max_ports;
    uint32_t         m_tx_cores;
    bool             m_capture;
    state_e          m_state;
    CNodeRing       *m_ring_from_cp;
    CCpuUtlDp        m_cpu_dp_u;
    CCpuUtlCp        m_cpu_cp_u;

    dsec_t           m_sync_time_sec;
    dsec_t           m_grat_arp_sec;
    
    uint64_t         m_rx_pkts;

    CRxCoreErrCntrs  m_err_cntrs;
    CRFC2544Info     m_rfc2544[MAX_FLOW_STATS_PAYLOAD];

    rx_port_mngr_t   m_rx_port_mngr;
    
    CPPSMeasure      m_rx_pps;
    
    TXQueue          m_tx_queue;
    
    /* accessed from control core */
    volatile bool    m_is_active;
};
#endif
