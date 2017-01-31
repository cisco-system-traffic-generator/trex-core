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

#include "trex_stateless_pkt.h"
#include <assert.h>


/**
 * copy MBUF to a flat buffer
 * 
 * @author imarom (12/20/2016)
 * 
 * @param dest - buffer with at least rte_pktmbuf_pkt_len(m) 
 *               bytes
 * @param m - MBUF to copy 
 * 
 * @return uint8_t* 
 */
void mbuf_to_buffer(uint8_t *dest, const rte_mbuf_t *m) {
    
    int index = 0;
    for (const rte_mbuf_t *it = m; it != NULL; it = it->next) {
        const uint8_t *src = rte_pktmbuf_mtod(it, const uint8_t *);
        memcpy(dest + index, src, it->data_len);
        index += it->data_len;
    }
}

/**************************************
 * TRex packet
 * 
 *************************************/
TrexPkt::TrexPkt(const rte_mbuf_t *m, int port, origin_e origin, uint64_t index) {

    /* allocate buffer */
    m_size = m->pkt_len;
    m_raw = new uint8_t[m_size];

    /* copy data */
    mbuf_to_buffer(m_raw, m);

    /* generate a packet timestamp */
    m_timestamp = now_sec();
    
    m_port   = port;
    m_origin = origin;
    m_index  = index;
}

TrexPkt::TrexPkt(const TrexPkt &other) {
    m_size = other.m_size;
    memcpy(m_raw, other.m_raw, m_size);
    
    m_timestamp = other.m_timestamp;
    
    m_port   = other.m_port;
    m_origin = other.m_origin;
    m_index  = other.m_index;
}


/**************************************
 * TRex packet buffer
 * 
 *************************************/

TrexPktBuffer::TrexPktBuffer(uint64_t size, mode_e mode) {
    m_mode             = mode;
    m_buffer           = nullptr;
    m_head             = 0;
    m_tail             = 0;
    m_bytes            = 0;
    m_size             = (size + 1); // for the empty/full difference 1 slot reserved
    
    /* generate queue */
    m_buffer = new const TrexPkt*[m_size](); // zeroed
}

TrexPktBuffer::~TrexPktBuffer() {
    assert(m_buffer);

    while (!is_empty()) {
        const TrexPkt *pkt = pop();
        delete pkt;
    }
    delete [] m_buffer;
}

/**
 * packet will be copied to an internal object
 */
void 
TrexPktBuffer::push(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin, uint64_t pkt_index) {
    
    /* if full - decide by the policy */
    if (is_full()) {
        if (m_mode == MODE_DROP_HEAD) {
            delete pop();
        } else {
            /* drop the tail (current packet) */
            return;
        }
    }

    push_internal(new TrexPkt(m, port, origin, pkt_index));
}

/**
 * packet will be handled internally 
 * packet pointer is invalid after this call 
 */
void 
TrexPktBuffer::push(const TrexPkt *pkt, uint64_t pkt_index) {
    /* if full - decide by the policy */
    if (is_full()) {
        if (m_mode == MODE_DROP_HEAD) {
            delete pop();
        } else {
            /* drop the tail (current packet) */
            return;
        }
    }

    /* duplicate packet */
    TrexPkt *dup = new TrexPkt(*pkt);
    
    /* update packet index if given */
    if (pkt_index != 0) {
        dup->set_index(pkt_index);
    }
    
    push_internal(dup);
}


void 
TrexPktBuffer::push_internal(const TrexPkt *pkt) {
    /* push the packet */
    m_buffer[m_head] = pkt;
    m_bytes += pkt->get_size();
    
    m_head = next(m_head);
}

const TrexPkt *
TrexPktBuffer::pop() {
    assert(!is_empty());
    
    const TrexPkt *pkt = m_buffer[m_tail];
    m_tail = next(m_tail);
    
    m_bytes -= pkt->get_size();
    
    return pkt;
}

uint32_t
TrexPktBuffer::get_element_count() const {
    if (m_head >= m_tail) {
        return (m_head - m_tail);
    } else {
        return ( get_capacity() - (m_tail - m_head - 1) );
    }
}

Json::Value
TrexPktBuffer::to_json() const {

    Json::Value output = Json::arrayValue;

    int tmp = m_tail;
    while (tmp != m_head) {
        const TrexPkt *pkt = m_buffer[tmp];
        output.append(pkt->to_json());
        tmp = next(tmp);
    }

    return output;
}

TrexPktBuffer *
TrexPktBuffer::pop_n(uint32_t count) {
    /* can't pop more than total */
    assert(count <= get_element_count());
    
    // TODO: consider returning NULL if no packets exists
    //       to avoid mallocing
 
    TrexPktBuffer *partial = new TrexPktBuffer(count);
    
    for (int i = 0; i < count; i++) {
        const TrexPkt *pkt = pop();
        partial->push_internal(pkt);
    }
    
    return partial;
}
