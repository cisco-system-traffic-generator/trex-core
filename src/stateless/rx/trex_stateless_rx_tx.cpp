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

#include "trex_stateless_rx_tx.h"
#include "trex_stateless_rx_feature_api.h"

void
TXQueue::create(RXFeatureAPI *api, uint32_t capacity) {
    m_api        = api;
    m_capacity   = capacity;
}


/**
 * remove all packets from the queue
 */
void
TXQueue::destroy() {
    while (!m_heap.empty()) {

        TXPacket *pkt = m_heap.top();
        delete pkt;
        
        m_heap.pop();
    }
}

/**
 * add a packet to the queue
 * 
 */
bool
TXQueue::push(const std::string &raw, double ts_sec) {
    
    /* do we have space ? */
    if (is_full()) {
        return false;
    }
    
    /* add the packet to the heap */
    m_heap.push(new TXPacket(raw, ts_sec));
    
    return true;
}

void
TXQueue::tick() {

    /* fast path */
    if (m_heap.empty()) {
        return;
    }
    
    int pkts_sent = 0;
    
    while (!m_heap.empty() && (pkts_sent < 100)) {
        TXPacket *pkt = m_heap.top();
       
        if (pkt->get_time() <= now_sec()) {
            /* pop */
            m_heap.pop();
            
            /* send the packet */
            m_api->tx_pkt(pkt->get_raw());
            pkts_sent++;
            
        } else {
            break;
        }
   }
}

