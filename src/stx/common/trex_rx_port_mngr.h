/*
  Itay Marom
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

#ifndef __TREX_RX_PORT_MNGR_H__
#define __TREX_RX_PORT_MNGR_H__

#include <stdint.h>
#include "common/base64.h"

#include "trex_pkt.h"
#include "trex_rx_feature_api.h"
#include "trex_stack_base.h"

class CPortLatencyHWBase;
class CRFC2544Info;
class CRxCoreErrCntrs;


/**************************************
 * RX feature latency
 * 
 *************************************/
class RXLatency {
public:

    RXLatency();

    void create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs);

    void handle_pkt(const rte_mbuf_t *m);

    Json::Value to_json() const;
    
    void get_stats(rx_per_flow_t *rx_stats,
                   int min,
                   int max,
                   bool reset,
                   TrexPlatformApi::driver_stat_cap_e type);
    
    void reset_stats();
    
private:
    // below functions for both IP v4 and v6, so they need uint32_t id
    bool is_flow_stat_id(uint32_t id) {
        if ((uint16_t) id >= m_ip_id_base)
            return true;
        else
            return false;
    }

    bool is_flow_stat_payload_id(uint32_t id) {
        if (id == FLOW_STAT_PAYLOAD_IP_ID) return true;
        return false;
    }

    uint16_t get_hw_id(uint16_t id) {
        return (~m_ip_id_base & (uint16_t )id);
}

public:

    rx_per_flow_t        m_rx_pg_stat[MAX_FLOW_STATS];
    rx_per_flow_t        m_rx_pg_stat_payload[MAX_FLOW_STATS_PAYLOAD];

    bool                 m_rcv_all;
    CRFC2544Info         *m_rfc2544;
    CRxCoreErrCntrs      *m_err_cntrs;
    uint16_t             m_ip_id_base;
};


/**************************************
 * RX feature queue 
 * 
 *************************************/

class RXQueue {
public:
    RXQueue() {
        m_pkt_buffer = nullptr;
    }
 
    ~RXQueue() {
        stop();
    }
    
    /**
     * start RX queue
     * 
     */
    void start(uint64_t size);
    
    /**
     * fetch the current buffer
     * return NULL if no packets
     */
    const TrexPktBuffer * fetch();
    
    /**
     * stop RX queue
     * 
     */
    void stop();
    
    void handle_pkt(const rte_mbuf_t *m);
    
    Json::Value to_json() const;
    
private:
    TrexPktBuffer  *m_pkt_buffer;
};


/************************ manager ***************************/

/**
 * per port RX features manager
 * 
 * @author imarom (10/30/2016)
 */
class RXPortManager {
    friend class RXFeatureAPI;
    
public:
    enum feature_t {
        NO_FEATURES  = 0,
        LATENCY      = 1,
        QUEUE        = 1 << 1,
        STACK        = 1 << 2,
    };

    RXPortManager();
    RXPortManager(const RXPortManager &other);
    ~RXPortManager(void);

    void create_async(uint32_t port_id,
                CPortLatencyHWBase *io,
                CRFC2544Info *rfc2544,
                CRxCoreErrCntrs *err_cntrs,
                CCpuUtlDp *cpu_util);
    void wait_for_create_done(void);
    void cleanup_async(void);
    void wait_for_cleanup_done(void);

    void clear_stats() {
        m_latency.reset_stats();
    }

    void get_latency_stats(rx_per_flow_t *rx_stats,
                           int min,
                           int max,
                           bool reset,
                           TrexPlatformApi::driver_stat_cap_e type) {
        
        return m_latency.get_stats(rx_stats, min, max, reset, type);
    }
    
    RXLatency & get_latency() {
        return m_latency;
    }

    CStackBase *get_stack() {
        return m_stack;
    }

    /* latency */
    void enable_latency() {
        set_feature(LATENCY);
    }

    void disable_latency() {
        unset_feature(LATENCY);
    }

    /* queue */
    void start_queue(uint32_t size) {
        m_queue.start(size);
        set_feature(QUEUE);
    }

    void stop_queue() {
        m_queue.stop();
        unset_feature(QUEUE); 
    }

    const TrexPktBuffer *get_pkt_buffer() {
        if (!is_feature_set(QUEUE)) {
            return nullptr;
        }
        
        return m_queue.fetch();
    }

    /**
     * fetch and process all packets
     * 
     */
    int process_all_pending_pkts(bool flush_rx = false);


    /**
     * flush all pending packets without processing them
     * 
     */
    void flush_all_pending_pkts() {
        process_all_pending_pkts(true);
    }


    /**
     * TX packets immediately (no queue)
     *  
     * returns true in case packets was transmitted succesfully 
     */
    bool tx_pkt(const std::string &pkt);
    bool tx_pkt(rte_mbuf_t *m);
    
    
    bool has_features_set() {
        return (m_features != NO_FEATURES);
    }

    bool no_features_set() {
        return (!has_features_set());
    }

    bool is_feature_set(feature_t feature) const {
        return ( (m_features & feature) == feature );
    }

    /**
     * returns ignored set of stats
     * (grat ARP, PING response and etc.)
     */
    void get_ignore_stats(CRXCoreIgnoreStat &stat, bool get_diff);
    
    /**
     * write the status to a JSON format
     */
    void to_json(Json::Value &feat_res) const;

private:

    // currently read from Linux sockets and send through port
    // might be extended later to other features
    uint16_t handle_tx(void);

    /**
     * handle a single packet
     * 
     */
    void handle_pkt(const rte_mbuf_t *m);


    void clear_all_features() {
        m_features = NO_FEATURES;
    }

    void set_feature(feature_t feature) {
        m_features |= feature;
    }

    void unset_feature(feature_t feature) {
        m_features &= (~feature);
    }

    uint32_t                     m_features;
    uint8_t                      m_port_id;
    RXLatency                    m_latency;
    RXQueue                      m_queue;

    CStackBase                  *m_stack;
    CCpuUtlDpPredict             m_cpu_pred;
    CPortLatencyHWBase          *m_io;

    /* stats to ignore (ARP and etc.) */
    CRXCoreIgnoreStat            m_ign_stats;
    CRXCoreIgnoreStat            m_ign_stats_prev;
    
    RXFeatureAPI                 m_feature_api;
};



#endif /* __TREX_RX_PORT_MNGR_H__ */

