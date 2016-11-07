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

/************************* latency ***********************/

class CPortLatencyHWBase;
class CRFC2544Info;
class CRxCoreErrCntrs;

class RXLatency {
public:

    RXLatency() {
        m_rcv_all    = false;
        m_rfc2544    = NULL;
        m_err_cntrs  = NULL;

        for (int i = 0; i < MAX_FLOW_STATS; i++) {
            m_rx_pg_stat[i].clear();
            m_rx_pg_stat_payload[i].clear();
        }
    }

    void create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs) {
        m_rfc2544 = rfc2544;
        m_err_cntrs = err_cntrs;
    }

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

/************************ queue ***************************/

/**                
 * describes a single saved RX packet
 * 
 */
class RxPacket {
public:

    RxPacket(const rte_mbuf_t *m) {
        /* assume single part packet */
        assert(m->nb_segs == 1);

        m_size = m->pkt_len;
        const uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);

        m_raw = (uint8_t *)malloc(m_size);
        memcpy(m_raw, p, m_size);
    }

    /* RVO here - no performance impact */
    const std::string to_base64_str() const {
        return base64_encode(m_raw, m_size);
    }

    ~RxPacket() {
        if (m_raw) {
            delete m_raw;
        }
    }

private:

    uint8_t *m_raw;
    uint16_t m_size;
};

/**
 * a simple cyclic buffer to hold RX packets
 * 
 */
class RxPacketBuffer {
public:

    RxPacketBuffer(uint64_t size, uint64_t *shared_counter) {
        m_buffer           = nullptr;
        m_head             = 0;
        m_tail             = 0;
        m_size             = (size + 1); // for the empty/full difference 1 slot reserved
        m_shared_counter   = shared_counter;

        *m_shared_counter = 0;

        m_buffer = new RxPacket*[m_size](); // zeroed

        m_is_enabled = true;
    }

    ~RxPacketBuffer() {
        assert(m_buffer);

        while (!is_empty()) {
            RxPacket *pkt = pop();
            delete pkt;
        }
        delete [] m_buffer;
    }

    /* freeze the data structure - no more packets can be pushed / poped */
    RxPacketBuffer * freeze_and_clone() {
        /* create a new one */
        RxPacketBuffer *new_buffer = new RxPacketBuffer(m_size, m_shared_counter);

        /* freeze the current */
        m_shared_counter = NULL;
        m_is_enabled = false;

        return new_buffer;
    }

    bool is_empty() const {
        return (m_head == m_tail);
    }

    bool is_full() const {
        return ( next(m_head) == m_tail);
    }

    void handle_pkt(const rte_mbuf_t *m) {
        assert(m_is_enabled);

        /* if full - pop the oldest */
        if (is_full()) {
            delete pop();
        }

        (*m_shared_counter)++;

        m_buffer[m_head] = new RxPacket(m);
        m_head = next(m_head);
    }

    /**
     * generate a JSON output of the queue
     * 
     */
    Json::Value to_json() const {

        Json::Value output = Json::arrayValue;

        int tmp = m_tail;
        while (tmp != m_head) {
            RxPacket *pkt = m_buffer[tmp];
            output.append(pkt->to_base64_str());
            tmp = next(tmp);
        }

        return output;
    }

private:
    int next(int v) const {
        return ( (v + 1) % m_size );
    }

    /* pop in case of full queue - internal usage */
    RxPacket * pop() {
        assert(m_is_enabled);
        assert(!is_empty());

        RxPacket *pkt = m_buffer[m_tail];
        m_tail = next(m_tail);
        (*m_shared_counter)--;
        return pkt;
    }

    int             m_head;
    int             m_tail;
    int             m_size;
    RxPacket      **m_buffer;
    uint64_t       *m_shared_counter;
    bool            m_is_enabled;
};

/************************ recoder ***************************/

/**
 * RX packet recorder to PCAP file
 * 
 */
class RXPacketRecorder {
public:
    RXPacketRecorder();
    ~RXPacketRecorder();
    void start(const std::string &pcap, uint64_t limit, uint64_t *shared_counter);
    void stop();
    void handle_pkt(const rte_mbuf_t *m);

private:
    CFileWriterBase  *m_writer;
    CCapPktRaw        m_pkt;
    dsec_t            m_epoch;
    uint64_t          m_limit;
    uint64_t          *m_shared_counter;
};


/************************ manager ***************************/

/**
 * per port RX features manager
 * 
 * @author imarom (10/30/2016)
 */
class RXPortManager {
public:
    enum features_t {
        LATENCY = 0x1,
        CAPTURE  = 0x2,
        QUEUE   = 0x4
    };

    RXPortManager() {
        m_features    = 0;
        m_pkt_buffer  = NULL;
        m_io          = NULL;
        m_cpu_dp_u    = NULL;
    }

    void create(CPortLatencyHWBase *io,
                CRFC2544Info *rfc2544,
                CRxCoreErrCntrs *err_cntrs,
                CCpuUtlDp *cpu_util) {
        m_io = io;
        m_cpu_dp_u = cpu_util;
        m_latency.create(rfc2544, err_cntrs);
    }

    void clear_stats() {
        m_latency.reset_stats();
    }

    RXLatency & get_latency() {
        return m_latency;
    }

    void enable_latency() {
        set_feature(LATENCY);
    }

    void disable_latency() {
        unset_feature(LATENCY);
    }

    /**
     * capturing of RX packets
     * 
     * @author imarom (11/2/2016)
     * 
     * @param pcap 
     * @param limit_pkts 
     */
    void start_capture(const std::string &pcap, uint64_t limit_pkts, uint64_t *shared_counter) {
        m_recorder.start(pcap, limit_pkts, shared_counter);
        set_feature(CAPTURE);
    }

    void stop_capture() {
        m_recorder.stop();
        unset_feature(CAPTURE);
    }

    /**
     * queueing packets
     * 
     */
    void start_queue(uint32_t size, uint64_t *shared_counter) {
        if (m_pkt_buffer) {
            delete m_pkt_buffer;
        }
        m_pkt_buffer = new RxPacketBuffer(size, shared_counter);
        set_feature(QUEUE);
    }

    void stop_queue() {
        if (m_pkt_buffer) {
            delete m_pkt_buffer;
            m_pkt_buffer = NULL;
        }
        unset_feature(QUEUE);
    }

    RxPacketBuffer *get_pkt_buffer() {
        if (!is_feature_set(QUEUE)) {
            return NULL;
        }

        assert(m_pkt_buffer);

        /* hold a pointer to the old one */
        RxPacketBuffer *old_buffer = m_pkt_buffer;

        /* replace the old one with a new one and freeze the old */
        m_pkt_buffer = old_buffer->freeze_and_clone();

        return old_buffer;
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


    bool has_features_set() {
        return (m_features != 0);
    }


    bool no_features_set() {
        return (!has_features_set());
    }

private:
    

    void set_feature(features_t feature) {
        m_features |= feature;
    }

    void unset_feature(features_t feature) {
        m_features &= (~feature);
    }

    bool is_feature_set(features_t feature) {
        return ( (m_features & feature) == feature );
    }

    uint32_t                     m_features;
    RXPacketRecorder             m_recorder;
    RXLatency                    m_latency;
    RxPacketBuffer              *m_pkt_buffer;
    CCpuUtlDp                   *m_cpu_dp_u;
    CPortLatencyHWBase          *m_io;
};



#endif /* __TREX_STATELESS_RX_PORT_MNGR_H__ */

