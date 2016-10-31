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

    RxPacketBuffer(int limit) {
        m_buffer = nullptr;
        m_head   = 0;
        m_tail   = 0;
        m_limit  = limit;

        m_buffer = new RxPacket*[limit](); // zeroed
    }

    ~RxPacketBuffer() {
        assert(m_buffer);

        while (!is_empty()) {
            RxPacket *pkt = pop();
            delete pkt;
        }
        delete [] m_buffer;
    }

    bool is_empty() const {
        return (m_head == m_tail);
    }

    bool is_full() const {
        return ( next(m_head) == m_tail);
    }

    int get_limit() const {
        return m_limit;
    }

    void push(RxPacket *pkt) {
        if (is_full()) {
            delete pop();
        }
        m_buffer[m_head] = pkt;
        m_head = next(m_head);
    }

    /**
     * generate a JSON output of the queue
     * 
     */
    Json::Value to_json() {

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
        return ( (v + 1) % m_limit );
    }

    /* pop in case of full queue - internal usage */
    RxPacket * pop() {
        assert(!is_empty());
        RxPacket *pkt = m_buffer[m_tail];
        m_tail = next(m_tail);
        return pkt;
    }

    int m_head;
    int m_tail;
    int m_limit;
    RxPacket **m_buffer;
};

/**
 * RX packet recorder to PCAP file
 * 
 */
class RXPacketRecorder {
public:
    RXPacketRecorder();
    ~RXPacketRecorder();
    void start(const std::string &pcap, uint32_t limit);
    void stop();
    void handle_pkt(const rte_mbuf_t *m);

private:
    CFileWriterBase  *m_writer;
    CCapPktRaw        m_pkt;
    dsec_t            m_epoch;
    uint32_t          m_limit;
};


/**
 * per port RX features manager
 * 
 * @author imarom (10/30/2016)
 */
class RXPortManager {
public:
    enum features_t {
        LATENCY = 0x1,
        RECORD  = 0x2,
        QUEUE   = 0x4
    };

    RXPortManager() {
        m_features = 0;
        m_pkt_buffer = NULL;
        set_feature(LATENCY);
    }

    void start_recorder(const std::string &pcap, uint32_t limit_pkts) {
        m_recorder.start(pcap, limit_pkts);
        set_feature(RECORD);
    }

    void stop_recorder() {
        m_recorder.stop();
        unset_feature(RECORD);
    }

    void start_queue(uint32_t limit) {
        if (m_pkt_buffer) {
            delete m_pkt_buffer;
        }
        m_pkt_buffer = new RxPacketBuffer(limit);
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

        /* take the current */
        RxPacketBuffer *old_buffer = m_pkt_buffer;

        /* allocate a new buffer */
        m_pkt_buffer = new RxPacketBuffer(old_buffer->get_limit());

        return old_buffer;
    }

    void handle_pkt(const rte_mbuf_t *m) {
        /* fast path */
        if (no_features_set()) {
            return;
        }

        if (is_feature_set(RECORD)) {
            m_recorder.handle_pkt(m);
        }

        if (is_feature_set(QUEUE)) {
            m_pkt_buffer->push(new RxPacket(m));
        }
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

    bool no_features_set() {
        return (m_features == 0);
    }

    uint32_t            m_features;
    RXPacketRecorder    m_recorder;
    RxPacketBuffer     *m_pkt_buffer;
};



#endif /* __TREX_STATELESS_RX_PORT_MNGR_H__ */

