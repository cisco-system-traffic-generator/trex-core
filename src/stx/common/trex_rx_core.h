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
#include "stl/trex_stl_tpg.h"

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
        m_is_active           = false;
        m_ex_zmq_enabled      = false;
        m_ezmq_use_tcp        = false;
        m_dp_msg_hot          = false;
        m_working_tpg_threads = 0;
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
    bool add_to_capwap_proxy(uint8_t port_id, Json::Value capwap_map);
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

    /**
     * Indicate if Tagged Packet Grouping is enabled/disabled in RX.
     * Enabling/Disabling TPG is Rx can take a while since we might allocate/deallocate a lot of memory.
     * However, the memory is allocated in another thread, hence this function
     * can be called even when the allocation/deallocation is happening.
     *
     * @param username
     *   Username for which we are checking if TPG is enabled.
     *
     * @return int
     *  State casted to int so it can pass easily through messages.
     **/

    int get_tpg_state(const std::string& username);

    /**
     * Does the TPG context for this username exist?
     *
     * @param username
     *   Username for which we are checking if TPG context exists
     *
     * @return bool
     *  True iff TPG context exists for this context.
     **/
    bool tpg_ctx_exists(const std::string& username);

    /**
     * Enable Tagged Packet Grouping asynchronously.
     * Dynamically allocate counters for tagged packets.
     * NOTE: TPG is not necessarily enabled after this function returns because of async.
     *       Use @is_tpg_enabled to verify the enablement has finished.
     * @param tpg_ctx
     *   Tagged Packet Group Control Plane Context.
     **/
    void enable_tpg_ctx(TPGCpCtx* tpg_ctx);

    /**
     * Disable Tagged Packet Grouping.
     * Remove all dynamically allocated counters for this user context.
     * NOTE: TPG is not necessarily disabled after this function returns because of async.
     *       Use @is_tpg_enabled to verify the disablement has finished.
     *
     * @param username
     *   Username whose context we are disabling.
     */
    void disable_tpg_ctx(const std::string& username);

    /**
     * Get Tagged Packet Group Statistics
     *
     * @param stats
     *   Json on which we dump the statistics.
     *
     * @param port_id
     *   Port on which we collect the stats
     *
     * @param tpgid
     *   Tagged Packet Group Identifier for which we collect the stats
     *
     * @param min_tag
     *   Minimal Tag to collect stats for. Inclusive.
     *
     * @param max_tag
     *   Maximal Tag for which to collect stats for. Exclusive.
     *
     * @param unknown_tag
     *   Dump the stats collected on unknown tags aswell.
     **/
    void get_tpg_stats(Json::Value& stats, uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag);

 protected:

    /**
     * Handle message packets from DPs.
     *
     * @return uint32_t
     *   Number of packets/messages handled.
     **/
    uint32_t handle_msg_packets();

    /**
     * Handle message packets from one single DP.
     *
     * @param thread_id
     *   Identifier of the DP.
     *
     * @param r
     *   Ring on which the messages/packets are sent.
     *
     * @return uint32_t
     *   Number of packets/messages handled.
     **/
    uint32_t handle_rx_one_queue(uint8_t thread_id, CNodeRing* r);

    /**
     * Handle one message/packet from DP.
     *
     * @param thread_id
     *   Thread on which the message is received
     *
     * @param node
     *   The message to handle.
     **/
    void handle_dp_msg(uint8_t thread_id, CGenNode* node);
    void delete_zmq();
    void create_zmq();
    void restart_zmq();
    bool create_zmq(void *   &socket,std::string port);
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
    void handle_rx_queue_msgs(uint8_t thread_id, CNodeRing* r);
    void handle_work_stage();
    int process_all_pending_pkts(bool flush_rx = false);
    void flush_all_pending_pkts() {
        process_all_pending_pkts(true);
    }
    void try_rx_queues();

    /**
     * In case there are any working TPG threads, this function is called by the main loop.
     * Handles TPG allocating/deallocating threads.
     *
     * @return bool
     *   Return True iff some work was done.
     **/
    bool handle_tpg_threads();

    /**
     * This function is called if a TPG context has finished allocating counters
     * in a separate thread and is awaiting enable.
     * Here we distribute the counters to the ports.
     *
     * @param tpg_rx_ctx
     *   Rx TPG context that finished allocating.
     **/
    void _enable_tpg_ctx(TPGRxCtx* tpg_rx_ctx);

    /**
     * This function is called if a TPG context has finished freeing counters
     * in a separate thread and is awaiting disable.
     * It is responsible to invalidate the counters to the ports.
     *
     * @param tpg_rx_ctx
     *   Rx TPG context that finished deallocating.
     **/
    void _disable_tpg_ctx(TPGRxCtx* tpg_rx_ctx);


 protected:
    TrexMonitor             m_monitor;
    uint32_t                m_tx_cores;
    bool                    m_capture;
    state_e                 m_state;
    CNodeRing*              m_ring_from_cp;
    CMessagingManager*      m_rx_dp;
    CCpuUtlDp               m_cpu_dp_u;
    CCpuUtlCp               m_cpu_cp_u;
    dsec_t                  m_sync_time_sec;
    dsec_t                  m_sync_time_period;
    dsec_t                  m_sync_time_dp_msg_sec;             // Next timestamp when DP messages should be read
    dsec_t                  m_sync_time_dp_msg_period;          // Period in between two consequent DP message reads.
    dsec_t                  m_grat_arp_sec;
    bool                    m_dp_msg_hot;                       // Is DP messaging redirection hot?
    uint64_t                m_rx_pkts;
    uint8_t                 m_working_tpg_threads;              // Number of working TPG threads (Ref Counter)
    CRxCoreErrCntrs         m_err_cntrs;
    CRFC2544Info            m_rfc2544[MAX_FLOW_STATS_PAYLOAD];
    rx_port_mg_map_t        m_rx_port_mngr_map;
    rx_port_mg_vec_t        m_rx_port_mngr_vec;
    CPPSMeasure             m_rx_pps;
    TXQueue                 m_tx_queue;

    /* accessed from control core */
    volatile bool                               m_is_active;
    std::unordered_map<std::string, TPGRxCtx*>  m_tpg_ctx;       // Rx TPG Contexts
    void*                                       m_zmq_ctx;
    void*                                       m_zmq_rx_socket; // in respect to TRex interface (rx->emu)
    void*                                       m_zmq_tx_socket; // in respect to TRex interface (emu->tx)
    CZmqPacketWriter                            m_zmq_wr;
    CZmqPacketReader                            m_zmq_rd;
    bool                                        m_ex_zmq_enabled;
    bool                                        m_ezmq_use_tcp;

};
#endif
