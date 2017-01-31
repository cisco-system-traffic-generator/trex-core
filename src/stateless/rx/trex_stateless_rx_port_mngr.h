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

#ifndef __TREX_STATELESS_RX_PORT_MNGR_H__
#define __TREX_STATELESS_RX_PORT_MNGR_H__

#include <stdint.h>
#include "common/base64.h"

#include "trex_stateless_pkt.h"

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
    bool is_flow_stat_id(uint32_t id) {
        if ((id & 0x000fff00) == IP_ID_RESERVE_BASE) return true;
        return false;
    }

    bool is_flow_stat_payload_id(uint32_t id) {
        if (id == FLOW_STAT_PAYLOAD_IP_ID) return true;
        return false;
    }

    uint16_t get_hw_id(uint16_t id) {
    return (0x00ff & id);
}

public:

    rx_per_flow_t        m_rx_pg_stat[MAX_FLOW_STATS];
    rx_per_flow_t        m_rx_pg_stat_payload[MAX_FLOW_STATS_PAYLOAD];

    bool                 m_rcv_all;
    CRFC2544Info         *m_rfc2544;
    CRxCoreErrCntrs      *m_err_cntrs;
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


/**************************************
 * RX server (ping, ARP and etc.)
 * 
 *************************************/
class RXPktParser;
class RXServer {
public:
    
    RXServer();
    void create(uint8_t port_id, CPortLatencyHWBase *io, const CManyIPInfo *src_addr);
    void handle_pkt(const rte_mbuf_t *m);
    
private:
    void handle_icmp(RXPktParser &parser);
    void handle_arp(RXPktParser &parser);
    rte_mbuf_t *duplicate_mbuf(const rte_mbuf_t *m);
    
    CPortLatencyHWBase  *m_io;
    uint8_t              m_port_id;
    const CManyIPInfo   *m_src_addr;
};

/**************************************
 * Gratidious ARP
 * 
 *************************************/
class RXGratARP {
public:
    RXGratARP() {
        m_io        = NULL;
        m_port_id   = UINT8_MAX;
        m_src_addr  = NULL;
        m_ign_stats = NULL;
    }
    
    void create(uint8_t port_id,
                CPortLatencyHWBase *io,
                CManyIPInfo *src_addr,
                CRXCoreIgnoreStat *ignore_stats);

    
    /**
     * the main 'tick' of the service
     * 
     */
    void send_next_grat_arp();
    
    Json::Value to_json() const;
    
private:
    uint8_t               m_port_id;
    CPortLatencyHWBase   *m_io;
    CManyIPInfo          *m_src_addr;
    CRXCoreIgnoreStat    *m_ign_stats;
};

/************************ manager ***************************/

/**
 * per port RX features manager
 * 
 * @author imarom (10/30/2016)
 */
class RXPortManager {
public:
    enum feature_t {
        NO_FEATURES  = 0x0,
        LATENCY      = 0x1,
        QUEUE        = 0x4,
        SERVER       = 0x8,
        GRAT_ARP     = 0x10,
    };

    RXPortManager();

    void create(const TRexPortAttr *port_attr,
                CPortLatencyHWBase *io,
                CRFC2544Info *rfc2544,
                CRxCoreErrCntrs *err_cntrs,
                CCpuUtlDp *cpu_util,
                uint8_t crc_bytes_num);

    
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

    void start_grat_arp() {
        set_feature(GRAT_ARP);
    }
    
    void stop_grat_arp() {
        unset_feature(GRAT_ARP);
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
     * handle a single packet
     * 
     */
    void handle_pkt(const rte_mbuf_t *m);

    /**
     * send next grat arp (if on)
     * 
     * @author imarom (12/13/2016)
     */
    void send_next_grat_arp();

    /**
     * set port mode to L2
     */
    void set_l2_mode();
    
    /**
     * set port mode to L3
     * 
     * @author imarom (12/13/2016)
     */
    void set_l3_mode(const CManyIPInfo &ip_info, bool is_grat_arp_needed);
  
      
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
    Json::Value to_json() const;
    
private:
    
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
    RXServer                     m_server;
    RXGratARP                    m_grat_arp;
    
    // compensate for the fact that hardware send us packets without Ethernet CRC, and we report with it
    uint8_t m_num_crc_fix_bytes;
    
    CCpuUtlDp                   *m_cpu_dp_u;
    CPortLatencyHWBase          *m_io;
    CManyIPInfo                  m_src_addr;
    
    /* stats to ignore (ARP and etc.) */
    CRXCoreIgnoreStat            m_ign_stats;
    CRXCoreIgnoreStat            m_ign_stats_prev;
};



#endif /* __TREX_STATELESS_RX_PORT_MNGR_H__ */

