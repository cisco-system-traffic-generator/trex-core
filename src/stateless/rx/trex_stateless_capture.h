/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#ifndef __TREX_STATELESS_CAPTURE_H__
#define __TREX_STATELESS_CAPTURE_H__

#include <stdint.h>
#include "trex_stateless_pkt.h"

/**
 * capture filter 
 * specify which ports to capture and if TX/RX or both 
 */
class CaptureFilter {
public:
    CaptureFilter() {
        m_tx_active = 0;
        m_rx_active = 0;
    }
    
    void add_tx(uint8_t port_id) {
        m_tx_active |= (1LL << port_id);
    }

    void add_rx(uint8_t port_id) {
        m_rx_active |= (1LL << port_id);
    }
    
    void add(uint8_t port_id) {
        add_tx(port_id);
        add_rx(port_id);
    }
    
    bool in_filter(const TrexPkt *pkt) {
        switch (pkt->get_origin()) {
        case TrexPkt::ORIGIN_TX:
            return in_tx(pkt->get_port());
            
        case TrexPkt::ORIGIN_RX:
            return in_rx(pkt->get_port());
            
        default:
            return false;
        }
    }
    
    bool in_rx(uint8_t port_id) const {
        uint64_t bit = (1LL << port_id);
        return ((m_rx_active & bit) == bit);
    }
    
    bool in_tx(uint8_t port_id) const {
        uint64_t bit = (1LL << port_id);
        return ((m_tx_active & bit) == bit);
    }
    
    bool in_any(uint8_t port_id) const {
        return ( in_tx(port_id) || in_rx(port_id) );
    }
    
    CaptureFilter& operator +=(const CaptureFilter &other) {
        m_tx_active |= other.m_tx_active;
        m_rx_active |= other.m_rx_active;
        
        return *this;
    }
    
private:
    
    uint64_t  m_tx_active;
    uint64_t  m_rx_active;
};

typedef int64_t capture_id_t;
enum {
    CAPTURE_ID_NOT_FOUND = -1,
    CAPTURE_TOO_MANY_CAPTURES = -2,
};

class TrexStatelessCapture {
public:
    
    TrexStatelessCapture(capture_id_t id, uint64_t limit, const CaptureFilter &filter);
    
    void handle_pkt_tx(const TrexPkt *pkt);
    void handle_pkt_rx(const rte_mbuf_t *m, int port);
    
    ~TrexStatelessCapture();
    
    uint64_t get_id() const {
        return m_id;
    }
    
    const CaptureFilter & get_filter() const {
        return m_filter;
    }
    
private:
    TrexPktBuffer   *m_pkt_buffer;
    CaptureFilter    m_filter;
    uint64_t         m_id;
};

class TrexStatelessCaptureMngr {
    
public:
    
    static TrexStatelessCaptureMngr& getInstance() {
        static TrexStatelessCaptureMngr instance;

        return instance;
    }

    
    ~TrexStatelessCaptureMngr() {
        reset();
    }
    
    /**
     * adds a capture buffer
     * returns ID 
     */
    capture_id_t add(uint64_t limit, const CaptureFilter &filter);
   
     
    /**
     * stops capture mode 
     * on success, will return the ID of the removed one 
     * o.w it will be an error 
     */
    capture_id_t remove(capture_id_t id);
    
    /**
     * removes all captures
     * 
     */
    void reset();
    
    /**
     * return true if any filter is active
     * 
     * @author imarom (1/3/2017)
     * 
     * @return bool 
     */
    bool is_active(uint8_t port) const {
        return m_global_filter.in_any(port);
    }
    
    /**
     *  handle packet from TX
     */
    void handle_pkt_tx(const TrexPkt *pkt);
    
    /** 
     * handle packet from RX 
     */
    void handle_pkt_rx(const rte_mbuf_t *m, int port) {
        /* fast path */
        if (!is_active(port)) {
            return;
        }
        
        /* slow path */
        handle_pkt_rx_slow_path(m, port);
    }
    
private:
    
    TrexStatelessCaptureMngr() {
        /* init this to 1 */
        m_id_counter = 1;
    }
    
    void handle_pkt_rx_slow_path(const rte_mbuf_t *m, int port);
    void update_global_filter();
    
    std::vector<TrexStatelessCapture *> m_captures;
    
    capture_id_t m_id_counter;
    
    /* a union of all the filters curently active */
    CaptureFilter m_global_filter;
    
    static const int MAX_CAPTURE_SIZE = 10;
};

#endif /* __TREX_STATELESS_CAPTURE_H__ */

