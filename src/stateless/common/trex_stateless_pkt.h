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

#ifndef __TREX_STATELESS_PKT_H__
#define __TREX_STATELESS_PKT_H__

#include <stdint.h>
#include <json/json.h>
#include "mbuf.h"
#include "common/base64.h"
#include "os_time.h"


/**
 * copies MBUF to a flat buffer
 * 
 */
void mbuf_to_buffer(uint8_t *dest, const rte_mbuf_t *m);

/**************************************
 * TRex packet
 * 
 *************************************/
class TrexPkt {
public:

    /**
     * origin of the created packet
     */
    enum origin_e {
        ORIGIN_NONE = 1,
        ORIGIN_TX,
        ORIGIN_RX
    };
    
    /**
     * generate a packet from MBUF
     */
    TrexPkt(const rte_mbuf_t *m, int port = -1, origin_e origin = ORIGIN_NONE, uint64_t index = 0);
    
    /**
     * duplicate an existing packet
     */
    TrexPkt(const TrexPkt &other);
 
    
    /**
     * sets a packet index 
     * used by a buffer of packets 
     */
    void set_index(uint64_t index) {
        m_index = index;
    }
    
    
    /* slow path and also RVO - pass by value is ok */
    Json::Value to_json() const {
        Json::Value output;
        output["ts"]      = m_timestamp;
        output["binary"]  = base64_encode(m_raw, m_size);
        output["port"]    = m_port;
        output["index"]   = Json::UInt64(m_index);
        
        switch (m_origin) {
        case ORIGIN_TX:
            output["origin"]  = "TX";
            break;
        case ORIGIN_RX:
            output["origin"]  = "RX";
            break;
        default:
            output["origin"]  = "NONE";
            break;
        }
        
        return output;
    }

    ~TrexPkt() {
        if (m_raw) {
            delete [] m_raw;
        }
    }

    origin_e get_origin() const {
        return m_origin;
    }
    
    int get_port() const {
        return m_port;
    }
 
    uint16_t get_size() const {
        return m_size;   
    }
    
    dsec_t get_ts() const {
        return m_timestamp;
    }
    
private:

    uint8_t   *m_raw;
    uint16_t   m_size;
    dsec_t     m_timestamp;
    origin_e   m_origin;
    int        m_port;
    uint64_t   m_index;
};


/**************************************
 * TRex packet buffer
 * 
 *************************************/
class TrexPktBuffer {
public:

    /**
     * two modes for operations: 
     *  
     * MODE_DROP_HEAD - when the buffer is full, packets will be 
     * dropped from the head (the oldest packet) 
     *  
     * MODE_DROP_TAIL - when the buffer is full, packets will be 
     * dropped from the tail (the current packet) 
     */
    enum mode_e {
        MODE_DROP_HEAD = 1,
        MODE_DROP_TAIL = 2,
    };
    
    TrexPktBuffer(uint64_t size, mode_e mode = MODE_DROP_TAIL);
    ~TrexPktBuffer();

    /**
     * push a packet to the buffer 
     * packet will be generated from a MBUF 
     *  
     */
    void push(const rte_mbuf_t *m,
              int port = -1,
              TrexPkt::origin_e origin = TrexPkt::ORIGIN_NONE,
              uint64_t pkt_index = 0);
    
    /**
     * push an existing packet structure 
     * packet will *not* be duplicated 
     *  
     * after calling this function 
     * the packet is no longer usable 
     * from caller prespective 
     */
    void push(const TrexPkt *pkt);
    
    /**
     * pops a packet from the buffer
     * usually for internal usage
     */
    const TrexPkt * pop();
    
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

    /**
     * return the total amount of space possible
     */
    uint32_t get_capacity() const {
        /* one slot is used for diff between full/empty */
        return (m_size - 1);
    }
    
    /**
     * see mode_e
     * 
     */
    mode_e get_mode() const {
        return m_mode;
    }
    
    /**
     * returns how many elements are in the queue
     */
    uint32_t get_element_count() const;
    
    /**
     * current bytes holded by the buffer
     */
    uint32_t get_bytes() const {
        return m_bytes; 
    }
    
private:
    int next(int v) const {
        return ( (v + 1) % m_size );
    }

    mode_e          m_mode;
    int             m_head;
    int             m_tail;
    int             m_size;
    uint32_t        m_bytes;
    const TrexPkt **m_buffer;
};


#endif /* __TREX_STATELESS_PKT_H__*/
