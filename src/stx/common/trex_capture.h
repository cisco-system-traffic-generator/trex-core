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
#ifndef __TREX_CAPTURE_H__
#define __TREX_CAPTURE_H__

#include <stdint.h>
#include <assert.h>
#include <mutex>

#include "trex_pkt.h"
#include "trex_capture_rc.h"
#include "bpf_api.h"

#include <common/pcap.h>

/**
 * a class to handle BPF filter 
 *  
 */
class BPFFilter {
public:
    
    /* CTOR */
    BPFFilter() {
        m_bpf_h = BPF_H_NONE;
    }
    
    
    /* DTOR */    
    ~BPFFilter() {
        release();
    }
    
    
    /* Copy CTOR - do not copy object, only pattern */
    BPFFilter(const BPFFilter &other) {
        m_bpf_filter = other.m_bpf_filter;
        m_bpf_h      = BPF_H_NONE;
    }
  
   
    /**
     * relase compiled object
     * 
     */
    void release() {
        if (m_bpf_h != BPF_H_NONE) {
            
            #ifdef TREX_USE_BPFJIT
                bpfjit_destroy(m_bpf_h);
            #else
                bpf_destroy(m_bpf_h);
            #endif
            
            m_bpf_h = BPF_H_NONE;
        }
    }
    
    /**
     * set a BPF filter
     * 
     */
    void set_filter(const std::string &bpf_filter) {
        release();
        m_bpf_filter = bpf_filter;
    }
    
    
     /**
     * returns the pattern asso
     * 
     * @author imarom (7/13/2017)
     * 
     * @return const std::string& 
     */
    const std::string & get_filter() const {
        return m_bpf_filter;
    }
    
    
    /**
     * compile the capture BPF filter
     */
    void compile() {
        /* cleanup if recompiled */
        release();
        
        /* JIT or regular */
        #ifdef TREX_USE_BPFJIT
            m_bpf_h = bpfjit_compile(m_bpf_filter.c_str());
        #else
            m_bpf_h = bpf_compile(m_bpf_filter.c_str());
        #endif
        
        
        /* should never fail - caller should verify */
        assert(m_bpf_h);
    }
    
    

    /* assignment operator */
    BPFFilter& operator=(const BPFFilter &other) {

        release();
        m_bpf_filter = other.m_bpf_filter;

        return *this;
    }

    
    /**
     * combine BPF filters
     * 
     */
    BPFFilter& operator +=(const BPFFilter &other) {

        /* if any filter is empty the result is an empty filter (OR operator) */
        if ( (m_bpf_filter == "") || (other.m_bpf_filter == "") ) {
            /* match everything */
            set_filter("");
        } else {
            const std::string new_filter = "(" + m_bpf_filter + ") or (" + other.m_bpf_filter + ")";
            set_filter(new_filter);
        }
        
        return *this;
    }
    
    inline bool match(const rte_mbuf_t *m) const {
        assert(m_bpf_h);

        const char *buffer = rte_pktmbuf_mtod(m, char *);
        uint32_t len       = rte_pktmbuf_pkt_len(m);
        
        #ifdef TREX_USE_BPFJIT
            int rc = bpfjit_run(m_bpf_h, buffer, len);
        #else
            int rc = bpf_run(m_bpf_h, buffer, len);
        #endif
        
        return (rc != 0);
    }
    
private:
    /* BPF pattern and a BPF compiled object handler */
    std::string  m_bpf_filter;
    bpf_h        m_bpf_h;
    
};


/**************************************
 * Capture Filter 
 *  
 * specify which ports to capture and if TX/RX or both 
 *************************************/
class CaptureFilter {
public:
    
    /* CTOR */
    CaptureFilter() {
        m_tx_active  = 0;
        m_rx_active  = 0;
    }
    
    /* Copy CTOR */
    CaptureFilter(const CaptureFilter &other) {
        /* copy those fields */
        m_tx_active   = other.m_tx_active;
        m_rx_active   = other.m_rx_active;
        m_bpf         = other.m_bpf;
    }
    
    /**
     * set a BPF filter
     *  
     * by default, match all 
     */
    void set_bpf_filter(const std::string &bpf_filter) {
        m_bpf.set_filter(bpf_filter);
    }
    
    /**
     * compile the capture BPF filter
     */
    void compile() {
        m_bpf.compile();
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
    
    /**
     * add a port to both directions
     * 
     */
    void add(uint8_t port_id) {
        add_tx(port_id);
        add_rx(port_id);
    }
    
    /**
     * return true if 'port_id' is being captured 
     * as RX port 
     */
    bool in_rx(uint8_t port_id) const {
        uint64_t bit = (1LL << port_id);
        return ((m_rx_active & bit) == bit);
    }
    
    /**
     * return true if 'port_id' is being captured 
     * as TX port 
     */
    bool in_tx(uint8_t port_id) const {
        uint64_t bit = (1LL << port_id);
        return ((m_tx_active & bit) == bit);
    }
    
    /**
     * return true if 'port_id' is being monitored at all
     */
    bool in_any(uint8_t port_id) const {
        return ( in_tx(port_id) || in_rx(port_id) );
    }
    
    /**
     * match a packet against the filter
     * 
     */
    inline bool match(uint8_t port_id, TrexPkt::origin_e origin, const rte_mbuf_t *m) const {
        if (origin == TrexPkt::ORIGIN_RX) {
            return (in_rx(port_id) && m_bpf.match(m));
        } else {
            return (in_tx(port_id) && m_bpf.match(m));
        }
    }
    
    /**
     * updates the current filter with another filter 
     * the result is the aggregation of TX /RX active lists 
     */
    CaptureFilter& operator +=(const CaptureFilter &other) {
        m_tx_active |= other.m_tx_active;
        m_rx_active |= other.m_rx_active;
        
        /* combine BPF filters */
        m_bpf += other.m_bpf;
        
        return *this;
    }
    
    
    Json::Value to_json() const {
        Json::Value output = Json::objectValue;
        output["tx"]     = Json::UInt64(m_tx_active);
        output["rx"]     = Json::UInt64(m_rx_active);
        output["bpf"]    = m_bpf.get_filter();
        return output;
    }

    uint64_t get_tx_active_map() const {
        return m_tx_active;
    }
    
    uint64_t get_rx_active_map() const {
        return m_rx_active;
    }
    
private:

    
    uint64_t     m_tx_active;
    uint64_t     m_rx_active;
    
    BPFFilter    m_bpf;
};


/**************************************
 * Capture Endpoint
 *
 * An endpoint to save captured packets
 *************************************/
class EndpointWriter {
public:
    virtual ~EndpointWriter() {}
    virtual bool Create(std::string &endpoint, std::string &err) = 0;
    virtual bool is_open() const = 0;
    virtual int write(const void *buf, size_t nbyte) = 0;
    virtual void close() = 0;
};

class EndpointBuffer {
    uint8_t* m_base;
    uint32_t m_size;
    uint32_t m_reserved;
    uint16_t m_count;   /* packet count */

public:
    EndpointBuffer();

    ~EndpointBuffer();

    uint8_t* get_base() const { return m_base; }
    uint32_t get_size() const { return m_size; }
    uint32_t get_count() const { return m_count; }

    uint32_t available_size() const;
    uint8_t * reserve_block(uint32_t size);
    void consume_block(uint32_t size);

    bool in_use() const { return m_size != m_reserved; }
    bool has_reserved(uint8_t *block) const {
        return (block - m_base) < m_reserved;
    }
};

class CaptureEndpoint {
    std::string m_endpoint;
    enum format {
        PCAP = 0,
        PCAPNG = 1,
    } m_format;

    uint16_t m_snaplen;
    std::map<uint8_t,uint8_t> m_ports_map;
    EndpointWriter *m_writer;

    uint64_t m_timestamp_ns;
    uint64_t m_timestamp_cycles;
    uint64_t m_timestamp_last;

    uint64_t get_timestamp_ns() const;

public:
    CaptureEndpoint(const std::string& endpoint, uint16_t snaplen);

    virtual ~CaptureEndpoint();

    bool Init(const CaptureFilter& filter, std::string &err);

    bool writer_is_active() const {
        return m_writer && m_writer->is_open();
    }

    std::string get_name() const {
        return m_endpoint;
    }

    uint16_t get_snaplen() const {
        return m_snaplen;
    }

    bool write_header();

    void write_packet(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin, uint64_t index);

    void stop();

    void to_json_status(Json::Value &output) const;

    double get_start_ts() const;

private:
    mutable std::mutex m_lock;
    std::vector<EndpointBuffer*> m_buffers;

    uint64_t m_packet_limit;    // limit count of saved packets to endpoint
    uint64_t m_packet_count;    // total count of saved packets to endpoint
    uint64_t m_packet_bytes;    // total bytes of saved packets to endpoint

    uint8_t* allocate_block(uint32_t size);
    void finalize_block(uint8_t *block, uint32_t size, uint32_t bytes);

public:
    void set_packet_limit(uint64_t limit) { m_packet_limit = limit; }
    uint64_t get_packet_limit() const { return m_packet_limit; }
    uint64_t get_packet_count() const { return m_packet_count; }
    uint64_t get_packet_bytes() const { return m_packet_bytes; }

    int flush_buffer(bool all);

    bool is_empty() const { return m_buffers.empty(); }

    uint32_t get_element_count() const;
};


/**************************************
 * Capture
 *  
 * A single instance of a capture
 *************************************/
class TrexCapture {
public:
    
    enum state_e {
        STATE_ACTIVE,
        STATE_STOPPED,
    };
    
    TrexCapture(capture_id_t id,
                         uint64_t limit,
                         const CaptureFilter &filter,
                         TrexPktBuffer::mode_e mode,
                         CaptureEndpoint *endpoint);
    
    ~TrexCapture();
    
    /**
     * handles a packet
     */
    void handle_pkt(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin);
    
    
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
        if (m_endpoint) {
            return m_endpoint->get_element_count();
        } else {
            return m_pkt_buffer->get_element_count();
        }
    }

    uint64_t get_pkt_lost() const {
        return m_pkt_index - (m_pkt_count + get_pkt_count());
    }
    
    dsec_t get_start_ts() const {
        return m_start_ts;
    }

    bool has_endpoint() const {
        return m_endpoint;
    }

    bool has_active_endpoint() const {
        return m_endpoint && m_endpoint->writer_is_active();
    }

    std::string get_endpoint_name() const {
        return m_endpoint ? m_endpoint->get_name(): "";
    }

    int flush_endpoint();

    Json::Value to_json() const;
    
private:
    state_e          m_state;
    TrexPktBuffer   *m_pkt_buffer;
    dsec_t           m_start_ts;
    CaptureFilter    m_filter;
    uint64_t         m_id;
    uint64_t         m_pkt_index;   // matched packets
    uint64_t         m_pkt_count;   // delivered packets
    CaptureEndpoint *m_endpoint;
};


/**************************************
 * Capture Manager
 * Handles all the captures in 
 * the system 
 *  
 * the design is a singleton 
 *************************************/
class TrexCaptureMngr {
    
public:
    
    static TrexCaptureMngr g_instance;
    static TrexCaptureMngr& getInstance() {
        return g_instance;
    }

    
    ~TrexCaptureMngr() {
        reset();
    }
    
    /**
     * starts a new capture
     */
    void start(const CaptureFilter &filter,
               uint64_t limit,
               TrexPktBuffer::mode_e mode,
               uint16_t snaplen,
               const std::string& endpoint,
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
     * return true if port is being monitored
     * on rx only
     */
    bool is_rx_active(uint8_t port) const {
        return m_global_filter.in_rx(port);
    }
    
    /**
     * handle packet on TX side 
     * always with a lock 
     */
    inline void handle_pkt_tx(const rte_mbuf_t *m, int port) {

        /* fast path */
        if (likely(!m_global_filter.in_tx(port))) {
            return;
        }
        
        /* TX core always locks */
        std::unique_lock<std::mutex> ulock(m_lock);
        
        /* check again the global filter (because of RX fast path might not lock) */
        if (m_global_filter.in_tx(port)) {
            handle_pkt_slow_path(m, port, TrexPkt::ORIGIN_TX);
        }
        
        ulock.unlock();
        
    }

    /* handle ASTF case where packets are in dp core */
    inline void handle_pkt_rx_dp(const rte_mbuf_t *m, int port) {

        /* fast path */
        if (likely(!m_global_filter.in_rx(port))) {
            return;
        }

        /* TX core always locks */
        std::unique_lock<std::mutex> ulock(m_lock);

        /* check again the global filter (because of RX fast path might not lock) */
        if (m_global_filter.in_rx(port)) {
            handle_pkt_slow_path(m, port, TrexPkt::ORIGIN_RX);
        }

        ulock.unlock();

    }

    /** 
     * handle packet on RX side 
     * RX side might or might not use a lock - depends if there are 
     * other TX cores being captured 
     */
    inline void handle_pkt_rx(const rte_mbuf_t *m, int port) {
        
        /* fast path - use the global filter */
        if (likely(!m_global_filter.match(port, TrexPkt::ORIGIN_RX, m))) {
            return;
        }
        
        /* create a RAII object lock but do not lock yet */
        std::unique_lock<std::mutex> ulock(m_lock, std::defer_lock);
        
        /* if we are not alone - lock */
        if (m_global_filter.get_tx_active_map() != 0) {
            ulock.lock();
        }
        
        handle_pkt_slow_path(m, port, TrexPkt::ORIGIN_RX);
        
    }
    
    Json::Value to_json();

    bool is_flush_needed();
    bool tick();    /* periodic flush for live captures */
        
private:
    
    TrexCaptureMngr() {
        /* init this to 1 */
        m_id_counter = 1;
        m_tick_time_sec = 0.0;
    }
    
    
    TrexCapture * lookup(capture_id_t capture_id) const;
    int lookup_index(capture_id_t capture_id) const;
    
    void handle_pkt_slow_path(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin);
   
    void update_global_filter();
    
    std::vector<TrexCapture *> m_captures;
    
    capture_id_t m_id_counter;
    
    /* a union of all the filters curently active */
    CaptureFilter m_global_filter;
    
    /* slow path lock*/
    std::mutex    m_lock;
    
    static const int MAX_CAPTURE_SIZE = 10;
    
    dsec_t m_tick_time_sec;
};

#endif /* __TREX_CAPTURE_H__ */

