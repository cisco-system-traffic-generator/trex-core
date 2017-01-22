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
#include "trex_stateless_capture_msg.h"

typedef int32_t capture_id_t;

class TrexCaptureRC {
public:

    TrexCaptureRC() {
        m_rc = RC_INVALID;
        m_pkt_buffer = NULL;
    }

    enum rc_e {
        RC_INVALID = 0,
        RC_OK = 1,
        RC_CAPTURE_NOT_FOUND,
        RC_CAPTURE_LIMIT_REACHED,
        RC_CAPTURE_FETCH_UNDER_ACTIVE
    };

    bool operator !() const {
        return (m_rc != RC_OK);
    }
    
    std::string get_err() const {
        assert(m_rc != RC_INVALID);
        
        switch (m_rc) {
        case RC_OK:
            return "";
        case RC_CAPTURE_LIMIT_REACHED:
            return "capture limit has reached";
        case RC_CAPTURE_NOT_FOUND:
            return "capture ID not found";
        case RC_CAPTURE_FETCH_UNDER_ACTIVE:
            return "fetch command cannot be executed on an active capture";
        default:
            assert(0);
        }
    }
    
    void set_err(rc_e rc) {
        m_rc = rc;
    }
    
    Json::Value get_json() const {
        return m_json_rc;
    }
    
public:
    rc_e              m_rc;
    capture_id_t      m_capture_id;
    TrexPktBuffer    *m_pkt_buffer;
    Json::Value       m_json_rc;
};

class TrexCaptureRCStart : public TrexCaptureRC {
public:

    void set_new_id(capture_id_t new_id) {
        m_capture_id = new_id;
        m_rc = RC_OK;
    }
    
    capture_id_t get_new_id() const {
        return m_capture_id;
    }
    
private:
    capture_id_t  m_capture_id;
};


class TrexCaptureRCStop : public TrexCaptureRC {
public:
    void set_count(uint32_t pkt_count) {
        m_pkt_count = pkt_count;
        m_rc = RC_OK;
    }
    
    uint32_t get_pkt_count() const {
        return m_pkt_count;
    }
    
private:
    uint32_t m_pkt_count;
};

class TrexCaptureRCFetch : public TrexCaptureRC {
public:

    TrexCaptureRCFetch() {
        m_pkt_buffer = nullptr;
        m_pending    = 0;
    }
    
    void set_pkt_buffer(const TrexPktBuffer *pkt_buffer, uint32_t pending, dsec_t start_ts) {
        m_pkt_buffer  = pkt_buffer;
        m_pending     = pending;
        m_start_ts    = start_ts;
        m_rc          = RC_OK;
    }
    
    const TrexPktBuffer *get_pkt_buffer() const {
        return m_pkt_buffer;
    }
    
    uint32_t get_pending() const {
        return m_pending;
    }
    
    dsec_t get_start_ts() const {
        return m_start_ts;
    }
    
private:
    const TrexPktBuffer *m_pkt_buffer;
    uint32_t             m_pending;
    dsec_t               m_start_ts;
};

class TrexCaptureRCRemove : public TrexCaptureRC {
public:
    void set_ok() {
        m_rc = RC_OK;
    }
};

class TrexCaptureRCStatus : public TrexCaptureRC {
public:
    
    void set_status(const Json::Value &json) {
        m_json = json;
        m_rc   = RC_OK;
    }
    
    const Json::Value & get_status() const {
        return m_json;
    }
    
private:
    Json::Value m_json;
};

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


class TrexStatelessCapture {
public:
    enum state_e {
        STATE_ACTIVE,
        STATE_STOPPED,
    };
    
    TrexStatelessCapture(capture_id_t id, uint64_t limit, const CaptureFilter &filter);
    
    void handle_pkt_tx(TrexPkt *pkt);
    void handle_pkt_rx(const rte_mbuf_t *m, int port);
    
    ~TrexStatelessCapture();
    
    uint64_t get_id() const {
        return m_id;
    }
    
    const CaptureFilter & get_filter() const {
        return m_filter;
    }
    
    Json::Value to_json() const;

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
    
private:
    state_e          m_state;
    TrexPktBuffer   *m_pkt_buffer;
    dsec_t           m_start_ts;
    CaptureFilter    m_filter;
    uint64_t         m_id;
    uint64_t         m_pkt_index;
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
     * starts a new capture
     */
    void start(const CaptureFilter &filter, uint64_t limit, TrexCaptureRCStart &rc);
   
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
    void handle_pkt_tx(TrexPkt *pkt);
    
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
    
    Json::Value to_json() const;
        
private:
    
    TrexStatelessCaptureMngr() {
        /* init this to 1 */
        m_id_counter = 1;
    }
    
    
    TrexStatelessCapture * lookup(capture_id_t capture_id);
    
    void handle_pkt_rx_slow_path(const rte_mbuf_t *m, int port);
    void update_global_filter();
    
    std::vector<TrexStatelessCapture *> m_captures;
    
    capture_id_t m_id_counter;
    
    /* a union of all the filters curently active */
    CaptureFilter m_global_filter;
    
    static const int MAX_CAPTURE_SIZE = 10;
};

#endif /* __TREX_STATELESS_CAPTURE_H__ */

