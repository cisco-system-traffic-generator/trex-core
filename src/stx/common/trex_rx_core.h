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
#include <vector>

#include "stateful_rx_core.h"
#include "os_time.h"
#include "utl_cpuu.h"
#include "trex_rx_port_mngr.h"
#include "trex_capture.h"
#include "trex_rx_tx.h"
#include "trex_latency_counters.h"

class TrexCpToRxMsgBase;

typedef std::map<uint8_t, RXPortManager*> rx_port_mg_map_t;
typedef std::vector<RXPortManager*> rx_port_mg_vec_t;

/**
 * RX core
 *
 */
class CRxCore : public TrexRxCore {

protected:
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
    bool start_capwap_proxy(uint8_t port_id, uint8_t pair_port_id, bool is_wireless_side, Json::Value capwap_map, uint32_t wlc_ip);
    void stop_capwap_proxy(uint8_t port_id);

    /* enable/disable astf fia */
    void enable_astf_latency_fia(bool enable);

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

    virtual void handle_astf_latency_pkt(const rte_mbuf_t *m,
                                         uint8_t port_id);

 protected:
    uint32_t handle_msg_packets(void);
    uint32_t handle_rx_one_queue(uint8_t thread_id, CNodeRing *r);

    void handle_cp_msg(TrexCpToRxMsgBase *msg);

    bool periodic_check_for_cp_messages();

    void tickle();

    virtual int _do_start(void);
    /* states */
    void hot_state_loop();
    void cold_state_loop();
    void init_work_stage();
    virtual bool work_tick();

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


 protected:
    TrexMonitor      m_monitor;
    uint32_t         m_tx_cores;
    bool             m_capture;
    state_e          m_state;
    CNodeRing       *m_ring_from_cp;
    CMessagingManager * m_rx_dp;
    CCpuUtlDp        m_cpu_dp_u;
    CCpuUtlCp        m_cpu_cp_u;

    dsec_t           m_sync_time_sec;
    dsec_t           m_sync_time_period;
    dsec_t           m_grat_arp_sec;

    uint64_t         m_rx_pkts;

    CRxCoreErrCntrs  m_err_cntrs;
    CRFC2544Info     m_rfc2544[MAX_FLOW_STATS_PAYLOAD];

    rx_port_mg_map_t m_rx_port_mngr_map;
    rx_port_mg_vec_t m_rx_port_mngr_vec;

    CPPSMeasure      m_rx_pps;

    TXQueue          m_tx_queue;

    /* accessed from control core */
    volatile bool    m_is_active;
};
#endif
