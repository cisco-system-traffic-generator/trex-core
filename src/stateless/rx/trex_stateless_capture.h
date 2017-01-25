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
#include <assert.h>

#include "trex_stateless_pkt.h"
#include "trex_stateless_capture_rc.h"


/**************************************
 * Capture Filter 
 *  
 * specify which ports to capture and if TX/RX or both 
 *************************************/
class CaptureFilter {
public:
    CaptureFilter() {
        m_tx_active = 0;
        m_rx_active = 0;
    }
    
    /**
     * add a port to the active TX port list
     */
    void add_tx(uint8_t port_id) {
        m_tx_active |= (1LL << port_id);
    }

    /**
     * add a port to the active RX port list
     */
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
    
    /**
     * updates the current filter with another filter 
     * the result is the aggregation of TX /RX active lists 
     */
    CaptureFilter& operator +=(const CaptureFilter &other) {
        m_tx_active |= other.m_tx_active;
        m_rx_active |= other.m_rx_active;
        
        return *this;
    }
    
    Json::Value to_json() const {
        Json::Value output = Json::objectValue;
        output["tx"] = Json::UInt64(m_tx_active);
        output["rx"] = Json::UInt64(m_rx_active);

        return output;
    }

private:
    
    uint64_t  m_tx_active;
    uint64_t  m_rx_active;
};


/**************************************
 * Capture
 *  
 * A single instance of a capture
 *************************************/
class TrexStatelessCapture {
public:
    
    enum state_e {
        STATE_ACTIVE,
        STATE_STOPPED,
    };
    
    TrexStatelessCapture(capture_id_t id,
                         uint64_t limit,
                         const CaptureFilter &filter,
                         TrexPktBuffer::mode_e mode);
    
    ~TrexStatelessCapture();
    
    /**
     * handles a packet from the TX side
     */
    void handle_pkt_tx(const TrexPkt *pkt);
    
    /**
     * handles a packet from the RX side
     */
    void handle_pkt_rx(const rte_mbuf_t *m, int port);
    
    
    uint64_t get_id() const {
        return m_id;
    }
    
    const CaptureFilter & get_filter() const {
        return m_filter;
    }
    

    /**
     * stop the capture - from now on all packets will be ignored
     * 
     * @author imarom (1/24/2017)
     */
    void stop() {
        m_state = STATE_STOPPED;
    }
    
    TrexPktBuffer * fetch(uint32_t pkt_limit, uint32_t &pending);
    
    bool is_active() const {
        return m_state == STATE_ACTIVE;
    }
    
    uint32_t get_pkt_count() const {
        return m_pkt_buffer->get_element_count();
    }
    
    dsec_t get_start_ts() const {
        return m_start_ts;
    }
    
    
    Json::Value to_json() const;
    
private:
    state_e          m_state;
    TrexPktBuffer   *m_pkt_buffer;
    dsec_t           m_start_ts;
    CaptureFilter    m_filter;
    uint64_t         m_id;
    uint64_t         m_pkt_index;
};


/**************************************
 * Capture Manager
 * Handles all the captures in 
 * the system 
 *  
 * the design is a singleton 
 *************************************/
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
     * starts a new capture
     */
    void start(const CaptureFilter &filter,
               uint64_t limit,
               TrexPktBuffer::mode_e mode,
               TrexCaptureRCStart &rc);
   
    /**
     * stops an existing capture
     * 
     */
    void stop(capture_id_t capture_id, TrexCaptureRCStop &rc);
    
    /**
     * fetch packets from an existing capture
     * 
     */
    void fetch(capture_id_t capture_id, uint32_t pkt_limit, TrexCaptureRCFetch &rc);

    /** 
     * removes an existing capture 
     * all packets captured will be detroyed 
     */
    void remove(capture_id_t capture_id, TrexCaptureRCRemove &rc);
    
    
    /**
     * removes all captures
     * 
     */
    void reset();
    
    
    /**
     * return true if any filter is active 
     * on a specific port 
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
    void handle_pkt_tx(const TrexPkt *pkt) {
        if (!m_global_filter.in_filter(pkt)) {
            return;
        }
        
        handle_pkt_tx_slow_path(pkt);
    }
    
    /** 
     * handle packet from RX 
     */
    void handle_pkt_rx(const rte_mbuf_t *m, int port) {
        /* fast path - check the global filter */
        if (!m_global_filter.in_rx(port)) {
            return;
        }
        
        /* slow path */
        handle_pkt_rx_slow_path(m, port);
    }
    
    Json::Value to_json() const;
        
private:
    
    TrexStatelessCaptureMngr() {
        /* init this to 1 */
        m_id_counter = 1;
    }
    
    
    TrexStatelessCapture * lookup(capture_id_t capture_id);
    int lookup_index(capture_id_t capture_id);
    
    void handle_pkt_rx_slow_path(const rte_mbuf_t *m, int port);
    void handle_pkt_tx_slow_path(const TrexPkt *pkt);
    
    void update_global_filter();
    
    std::vector<TrexStatelessCapture *> m_captures;
    
    capture_id_t m_id_counter;
    
    /* a union of all the filters curently active */
    CaptureFilter m_global_filter;
    
    static const int MAX_CAPTURE_SIZE = 10;
};

#endif /* __TREX_STATELESS_CAPTURE_H__ */

