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

#ifndef __TREX_STATELESS_RX_TX_H__
#define __TREX_STATELESS_RX_TX_H__

#include <string>
#include <queue>
#include "os_time.h"

class CRxCoreStateless;

/**
 * TX packet - an object to hold a packet to be sent
 */
class TXPacket {
public:
    
    /**
     * create a new packet 
     *  
     */
    TXPacket(int port_id, const std::string &raw, double ts_sec) : m_raw(raw) {
        m_port_id = port_id;
        m_time    = ts_sec;
    }
    
    /**
     * returns the timestamp of the packet
     */
    double get_time() const {
        return m_time;
    }
    
    /**
     * returns the port id on which the packet should be sent
     */
    int get_port_id() const {
        return m_port_id;
    }
    
    /**
     * returns the binary raw of the packet (string)
     * 
     */
    const std::string &get_raw() const {
        return m_raw;
    }
    
private:
    
    const std::string   m_raw;
    double              m_time;
    int                 m_port_id;
};


/**
 * comparator for TX packets (for timing)
 */
struct TXPacketCompare
{
   bool operator() (const TXPacket *lhs, const TXPacket *rhs)
   {
       return lhs->get_time() > rhs->get_time();
   }
};


/**
 * holds a heap of to-be-sent packets
 * 
 * @author imarom (8/15/2017)
 */
class TXQueue {
public:

    TXQueue() {
        m_rx = NULL;
        m_capacity  = 0;
    }
    
    /**
     * create a TX queue
     */
    void create(CRxCoreStateless *rx, uint32_t capacity);
    
    /**
     * release all resources
     */
    void destroy();
    
    /**
     * pushes a new packet to the send queue
     *  
     * raw    - a string of the packet 
     * ts_sec - when to send the packet ( in respect to now_sec() )
     */
    bool push(int port_id, const std::string &raw, double ts_sec);
    
    /**
     * return true if the queue is full
     * 
     */
    bool is_full() const {
        /* should never be bigger, but... */
        return m_heap.size() >= m_capacity;
    }
    
    
    /**
     * a constant tick to the queue
     */
    inline void tick() {
        /* fast path */
        if (m_heap.empty()) {
            return;
        }
        
        /* slow path */
        _tick();
    }
    
private:

    void _tick();
    
    std::priority_queue<TXPacket *, std::vector<TXPacket *>, TXPacketCompare> m_heap;
    
    CRxCoreStateless        *m_rx;
    uint32_t                 m_capacity;
};



#endif /* __TREX_STATELESS_RX_TX_H__ */

