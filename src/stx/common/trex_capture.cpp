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
#include "trex_capture.h"
#include "trex_exception.h"

/**************************************
 * Capture
 *  
 * A single instance of a capture
 *************************************/
TrexCapture::TrexCapture(capture_id_t id,
                         uint64_t limit,
                         const CaptureFilter &filter,
                         TrexPktBuffer::mode_e mode) {
    m_id         = id;
    m_pkt_buffer = new TrexPktBuffer(limit, mode);
    m_filter     = filter;
    m_state      = STATE_ACTIVE;
    m_start_ts   = now_sec();
    m_pkt_index  = 0;
    
    m_filter.compile();
}

TrexCapture::~TrexCapture() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
    }
}

void
TrexCapture::handle_pkt(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin) {
    
    if (m_state != STATE_ACTIVE) {
        return;
    }
    
    /* if not in filter - back off */
    if (!m_filter.match(port, origin, m)) {
        return;
    }
    
    m_pkt_buffer->push(m, port, origin, ++m_pkt_index);
}


Json::Value
TrexCapture::to_json() const {
    Json::Value output = Json::objectValue;

    output["id"]       = Json::UInt64(m_id);
    output["matched"]  = Json::UInt64(m_pkt_index);
    output["filter"]   = m_filter.to_json();
    
    switch (m_state) {
    case STATE_ACTIVE:
        output["state"]  = "ACTIVE";
        break;
        
    case STATE_STOPPED:
        output["state"]  = "STOPPED";
        break;
        
    default:
        assert(0);
    }
    
    
    /* write the pkt buffer status */
    m_pkt_buffer->to_json_status(output);
    
    return output;
}

/**
 * fetch up to 'pkt_limit' from the capture
 * 
 */
TrexPktBuffer *
TrexCapture::fetch(uint32_t pkt_limit, uint32_t &pending) {

    /* if the total sum of packets is within the limit range - take it */
    if (m_pkt_buffer->get_element_count() <= pkt_limit) {
        TrexPktBuffer *current = m_pkt_buffer;
        m_pkt_buffer = new TrexPktBuffer(m_pkt_buffer->get_capacity(), m_pkt_buffer->get_mode());
        pending = 0;
        return current;
    }
    
    /* partial fetch - take a partial list */
    TrexPktBuffer *partial = m_pkt_buffer->pop_n(pkt_limit);
    pending  = m_pkt_buffer->get_element_count();
    
    return partial;
}


/**************************************
 * Capture Manager 
 * handles all the captures 
 * in the system 
 *************************************/

/**
 * holds the global filter in the capture manager 
 * which ports in the entire system are monitored 
 */
void
TrexCaptureMngr::update_global_filter() {
    
    /* if no captures - clear global filter */
    if (m_captures.size() == 0) {
        m_global_filter = CaptureFilter();
        return;
    }
    
    /* copy the first one */
    CaptureFilter new_filter = CaptureFilter();

    /* add the rest */
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->is_active()) {
            new_filter += m_captures[i]->get_filter();
        }
    }
  
    /* copy and compile */
    m_global_filter = new_filter;
    m_global_filter.compile();
}


/**
 * lookup a specific capture by ID
 */
TrexCapture *
TrexCaptureMngr::lookup(capture_id_t capture_id) const {
    
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            return m_captures[i];
        }
    }
    
    /* does not exist */
    return nullptr;
}


int
TrexCaptureMngr::lookup_index(capture_id_t capture_id) const {
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            return i;
        }
    }
    return -1;
}


/**
 * starts a new capture
 * 
 */
void
TrexCaptureMngr::start(const CaptureFilter &filter,
                                uint64_t limit,
                                TrexPktBuffer::mode_e mode,
                                TrexCaptureRCStart &rc) {

    /* check for maximum active captures */
    if (m_captures.size() >= MAX_CAPTURE_SIZE) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_LIMIT_REACHED);
        return;
    }
    
    /* create a new capture*/
    int new_id = m_id_counter++;
    TrexCapture *new_capture = new TrexCapture(new_id, limit, filter, mode);
    
    /**
     * add the new capture in a safe mode 
     * (TX might be active) 
     */
    std::unique_lock<std::mutex> ulock(m_lock);
    m_captures.push_back(new_capture);

    /* update global filter */
    update_global_filter();
    
    /* done with critical section */
    ulock.unlock();
    
    /* result */
    rc.set_rc(new_id, new_capture->get_start_ts());
}

void
TrexCaptureMngr::stop(capture_id_t capture_id, TrexCaptureRCStop &rc) {
    TrexCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    std::unique_lock<std::mutex> ulock(m_lock);
    capture->stop();
    /* update global filter under lock (for barrier) */
    update_global_filter();

    /* done with critical section */
    ulock.unlock();
    
    rc.set_rc(capture->get_pkt_count());
}


void
TrexCaptureMngr::fetch(capture_id_t capture_id, uint32_t pkt_limit, TrexCaptureRCFetch &rc) {
    TrexCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    uint32_t pending = 0;
    
    /* take a lock before fetching all the packets */
    std::unique_lock<std::mutex> ulock(m_lock);
    TrexPktBuffer *pkt_buffer = capture->fetch(pkt_limit, pending);
    ulock.unlock();
    
    rc.set_rc(pkt_buffer, pending, capture->get_start_ts());
}

void
TrexCaptureMngr::remove(capture_id_t capture_id, TrexCaptureRCRemove &rc) {
    
    /* lookup index */
    int index = lookup_index(capture_id);
    if (index == -1) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    TrexCapture *capture =  m_captures[index];
    
    /* remove from list under lock */
    std::unique_lock<std::mutex> ulock(m_lock);
    
    m_captures.erase(m_captures.begin() + index);
    
    /* update global filter under lock (for barrier) */
    update_global_filter();
    
    /* done with critical section */
    ulock.unlock();
    
    /* free memory */
    delete capture;

    rc.set_rc();
}

void
TrexCaptureMngr::reset() {
    TrexCaptureRCRemove dummy;
    
    while (m_captures.size() > 0) {
        remove(m_captures[0]->get_id(), dummy);
        assert(!!dummy);
    }
}

/* define this macro to stress test the critical section */
//#define STRESS_TEST

void 
TrexCaptureMngr::handle_pkt_slow_path(const rte_mbuf_t *m, int port, TrexPkt::origin_e origin) {
    
    #ifdef STRESS_TEST
        static int sanity = 0;
        assert(__sync_fetch_and_add(&sanity, 1) == 0);
    #endif
    
    for (TrexCapture *capture : m_captures) {
        capture->handle_pkt(m, port, origin);
    }
    
    #ifdef STRESS_TEST
        assert(__sync_fetch_and_sub(&sanity, 1) == 1);
    #endif
}


Json::Value
TrexCaptureMngr::to_json() {
    Json::Value lst = Json::arrayValue;
    
    std::unique_lock<std::mutex> ulock(m_lock);
    
    for (TrexCapture *capture : m_captures) {
        lst.append(capture->to_json());
    }

    ulock.unlock();
    
    return lst;
}

TrexCaptureMngr TrexCaptureMngr::g_instance;

