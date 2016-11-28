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

#include "common/captureFile.h"


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

    void reset_stats();

    void handle_pkt(const rte_mbuf_t *m);

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

/**                
 * describes a single saved RX packet
 * 
 */
class RXPacket {
public:

    RXPacket(const rte_mbuf_t *m) {
        /* assume single part packet */
        assert(m->nb_segs == 1);

        m_size = m->pkt_len;
        const uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);

        m_raw = new uint8_t[m_size];
        memcpy(m_raw, p, m_size);
        
        /* generate a packet timestamp */
        m_timestamp = now_sec();
    }

    /* slow path and also RVO - pass by value is ok */
    Json::Value to_json() {
        Json::Value output;
        output["ts"]      = m_timestamp;
        output["binary"]  = base64_encode(m_raw, m_size);
        return output;
    }

    ~RXPacket() {
        if (m_raw) {
            delete [] m_raw;
        }
    }

private:

    uint8_t   *m_raw;
    uint16_t   m_size;
    dsec_t     m_timestamp;
};


/**************************************
 * RX feature queue 
 * 
 *************************************/

class RXPacketBuffer {
public:

    RXPacketBuffer(uint64_t size, uint64_t *shared_counter);
    ~RXPacketBuffer();

    /**
     * push a packet to the buffer
     * 
     */
    void push(const rte_mbuf_t *m);
    
    /**
     * freezes the queue and clones a new one
     * 
     */
    RXPacketBuffer * freeze_and_clone();

    /**
     * generate a JSON output of the queue
     * 
     */
    Json::Value to_json() const;


    bool is_empty() const {
        return (m_head == m_tail);
    }

    bool is_full() const {
        return ( next(m_head) == m_tail);
    }

private:
    int next(int v) const {
        return ( (v + 1) % m_size );
    }

    /* pop in case of full queue - internal usage */
    RXPacket * pop();

    int             m_head;
    int             m_tail;
    int             m_size;
    RXPacket      **m_buffer;
    uint64_t       *m_shared_counter;
    bool            m_is_enabled;
};


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
    void start(uint64_t size, uint64_t *shared_counter);
    
    /**
     * fetch the current buffer
     * 
     */
    RXPacketBuffer * fetch();
    
    /**
     * stop RX queue
     * 
     */
    void stop();
    
    void handle_pkt(const rte_mbuf_t *m);
    
private:
    RXPacketBuffer  *m_pkt_buffer;
};

/**************************************
 * RX feature PCAP recorder 
 * 
 *************************************/

class RXPacketRecorder {
public:
    RXPacketRecorder();
    
    ~RXPacketRecorder() {
        stop();
    }
    
    void start(const std::string &pcap, uint64_t limit, uint64_t *shared_counter);
    void stop();
    void handle_pkt(const rte_mbuf_t *m);

    /**
     * flush any cached packets to disk
     * 
     */
    void flush_to_disk();
    
private:
    CFileWriterBase  *m_writer;
    CCapPktRaw        m_pkt;
    dsec_t            m_epoch;
    uint64_t          m_limit;
    uint64_t         *m_shared_counter;
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
        RECORDER     = 0x2,
        QUEUE        = 0x4
    };

    RXPortManager();

    void create(CPortLatencyHWBase *io,
                CRFC2544Info *rfc2544,
                CRxCoreErrCntrs *err_cntrs,
                CCpuUtlDp *cpu_util);

    void clear_stats() {
        m_latency.reset_stats();
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

    /* recorder */
    void start_recorder(const std::string &pcap, uint64_t limit_pkts, uint64_t *shared_counter) {
        m_recorder.start(pcap, limit_pkts, shared_counter);
        set_feature(RECORDER);
    }

    void stop_recorder() {
        m_recorder.stop();
        unset_feature(RECORDER);
    }

    /* queue */
    void start_queue(uint32_t size, uint64_t *shared_counter) {
        m_queue.start(size, shared_counter);
        set_feature(QUEUE);
    }

    void stop_queue() {
        m_queue.stop();
        unset_feature(QUEUE); 
    }

    RXPacketBuffer *get_pkt_buffer() {
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
     * handle a single packet
     * 
     */
    void handle_pkt(const rte_mbuf_t *m);

    /**
     * maintenance
     * 
     * @author imarom (11/24/2016)
     */
    void tick();
    
    bool has_features_set() {
        return (m_features != NO_FEATURES);
    }


    bool no_features_set() {
        return (!has_features_set());
    }

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

    bool is_feature_set(feature_t feature) const {
        return ( (m_features & feature) == feature );
    }

    uint32_t                     m_features;
    
    RXLatency                    m_latency;
    RXPacketRecorder             m_recorder;
    RXQueue                      m_queue;
    
    
    CCpuUtlDp                   *m_cpu_dp_u;
    CPortLatencyHWBase          *m_io;
};



#endif /* __TREX_STATELESS_RX_PORT_MNGR_H__ */

