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
#include "trex_stateless_capture.h"
#include "trex_exception.h"

TrexStatelessCapture::TrexStatelessCapture(capture_id_t id, uint64_t limit, const CaptureFilter &filter) {
    m_id         = id;
    m_pkt_buffer = new TrexPktBuffer(limit, TrexPktBuffer::MODE_DROP_TAIL);
    m_filter     = filter;
    m_state      = STATE_ACTIVE;
}

TrexStatelessCapture::~TrexStatelessCapture() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
    }
}

void
TrexStatelessCapture::handle_pkt_tx(const TrexPkt *pkt) {

    if (m_state != STATE_ACTIVE) {
        delete pkt;
        return;
    }
    
    /* if not in filter - back off */
    if (!m_filter.in_filter(pkt)) {
        delete pkt;
        return;
    }
    
    m_pkt_buffer->push(pkt);
}

void
TrexStatelessCapture::handle_pkt_rx(const rte_mbuf_t *m, int port) {

    if (m_state != STATE_ACTIVE) {
        return;
    }
    
    if (!m_filter.in_rx(port)) {
        return;
    }
    
    m_pkt_buffer->push(m);
}


Json::Value
TrexStatelessCapture::to_json() const {
    Json::Value output = Json::objectValue;

    output["id"]     = Json::UInt64(m_id);
    output["filter"] = m_filter.to_json();
    output["count"]  = m_pkt_buffer->get_element_count();
    output["bytes"]  = m_pkt_buffer->get_bytes();
    output["limit"]  = m_pkt_buffer->get_capacity();
    
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
    
    return output;
}

TrexPktBuffer *
TrexStatelessCapture::fetch(uint32_t pkt_limit, uint32_t &pending) {
    
    /* if the total sum of packets is within the limit range - take it */
    if (m_pkt_buffer->get_element_count() <= pkt_limit) {
        TrexPktBuffer *current = m_pkt_buffer;
        m_pkt_buffer = new TrexPktBuffer(m_pkt_buffer->get_capacity(), m_pkt_buffer->get_mode());
        pending = 0;
        return current;
    }
    
    /* harder part - partial fetch */
    TrexPktBuffer *partial = new TrexPktBuffer(pkt_limit);
    for (int i = 0; i < pkt_limit; i++) {
        const TrexPkt *pkt = m_pkt_buffer->pop();
        partial->push(pkt);
    }
    
    pending = m_pkt_buffer->get_element_count();
    return partial;
}

void
TrexStatelessCaptureMngr::update_global_filter() {
    CaptureFilter new_filter;
    
    for (TrexStatelessCapture *capture : m_captures) {
        new_filter += capture->get_filter();
    }
    
    m_global_filter = new_filter;
}

TrexStatelessCapture *
TrexStatelessCaptureMngr::lookup(capture_id_t capture_id) {
    
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            return m_captures[i];
        }
    }
    
    /* does not exist */
    return nullptr;
}

void
TrexStatelessCaptureMngr::start(const CaptureFilter &filter, uint64_t limit, TrexCaptureRCStart &rc) {

    if (m_captures.size() > MAX_CAPTURE_SIZE) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_LIMIT_REACHED);
        return;
    }
    

    int new_id = m_id_counter++;
    TrexStatelessCapture *new_buffer = new TrexStatelessCapture(new_id, limit, filter);
    m_captures.push_back(new_buffer);
 
    /* update global filter */
    update_global_filter();
    
    /* result */
    rc.set_new_id(new_id);
}

void
TrexStatelessCaptureMngr::stop(capture_id_t capture_id, TrexCaptureRCStop &rc) {
    TrexStatelessCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    capture->stop();
    rc.set_count(capture->get_pkt_count());
}

void
TrexStatelessCaptureMngr::fetch(capture_id_t capture_id, uint32_t pkt_limit, TrexCaptureRCFetch &rc) {
    TrexStatelessCapture *capture = lookup(capture_id);
    if (!capture) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    if (capture->is_active()) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_FETCH_UNDER_ACTIVE);
        return;
    }
    
    uint32_t pending = 0;
    TrexPktBuffer *pkt_buffer = capture->fetch(pkt_limit, pending);
    
    rc.set_pkt_buffer(pkt_buffer, pending);
}

void
TrexStatelessCaptureMngr::remove(capture_id_t capture_id, TrexCaptureRCRemove &rc) {

    int index = -1;
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == capture_id) {
            index = i;
            break;
        }
    }
    
    /* does not exist */
    if (index == -1) {
        rc.set_err(TrexCaptureRC::RC_CAPTURE_NOT_FOUND);
        return;
    }
    
    TrexStatelessCapture *capture =  m_captures[index];
    m_captures.erase(m_captures.begin() + index);
    
    /* free memory */
    delete capture;
    
    /* update global filter */
    update_global_filter();
}

void
TrexStatelessCaptureMngr::reset() {
    TrexCaptureRCRemove dummy;
    
    while (m_captures.size() > 0) {
        remove(m_captures[0]->get_id(), dummy);
    }
}

void 
TrexStatelessCaptureMngr::handle_pkt_tx(const TrexPkt *pkt) {
    for (TrexStatelessCapture *capture : m_captures) {
        capture->handle_pkt_tx(pkt);
    }
}

void 
TrexStatelessCaptureMngr::handle_pkt_rx_slow_path(const rte_mbuf_t *m, int port) {
    for (TrexStatelessCapture *capture : m_captures) {
        capture->handle_pkt_rx(m, port);
    }
}

Json::Value
TrexStatelessCaptureMngr::to_json() const {
    Json::Value lst = Json::arrayValue;

    for (TrexStatelessCapture *capture : m_captures) {
        lst.append(capture->to_json());
    }

    return lst;
}

