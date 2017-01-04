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
}

TrexStatelessCapture::~TrexStatelessCapture() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
    }
}

void
TrexStatelessCapture::handle_pkt_tx(const TrexPkt *pkt) {
    
    /* if not in filter - back off */
    if (!m_filter.in_filter(pkt)) {
        return;
    }
    
    m_pkt_buffer->push(pkt);
}

void
TrexStatelessCapture::handle_pkt_rx(const rte_mbuf_t *m, int port) {
    if (!m_filter.in_rx(port)) {
        return;
    }
    
    m_pkt_buffer->push(m);
}

capture_id_t
TrexStatelessCaptureMngr::add(uint64_t limit, const CaptureFilter &filter) {
    
    if (m_captures.size() > MAX_CAPTURE_SIZE) {
        throw TrexException(TrexException::T_CAPTURE_MAX_INSTANCES);
    }
    

    int new_id = m_id_counter++;
    TrexStatelessCapture *new_buffer = new TrexStatelessCapture(new_id, limit, filter);
    m_captures.push_back(new_buffer);
    
    return new_id;
}

void
TrexStatelessCaptureMngr::remove(capture_id_t id) {
    
    int index = -1;
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i]->get_id() == id) {
            index = i;
            break;
        }
    }
    
    /* does not exist */
    if (index == -1) {
        throw TrexException(TrexException::T_CAPTURE_INVALID_ID);
    }
    
    TrexStatelessCapture *capture =  m_captures[index];
    m_captures.erase(m_captures.begin() + index);
    delete capture; 
}

void
TrexStatelessCaptureMngr::reset() {
    while (m_captures.size() > 0) {
        remove(m_captures[0]->get_id());
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

