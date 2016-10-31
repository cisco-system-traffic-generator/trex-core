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
#include "bp_sim.h"
#include "trex_stateless_rx_port_mngr.h"
#include "common/captureFile.h"

RXPacketRecorder::RXPacketRecorder() {
    m_writer = NULL;
    m_limit  = 0;
    m_epoch  = -1;
}

RXPacketRecorder::~RXPacketRecorder() {
    stop();
}

void
RXPacketRecorder::start(const std::string &pcap, uint32_t limit) {
    m_writer = CCapWriterFactory::CreateWriter(LIBPCAP, (char *)pcap.c_str());
    if (m_writer == NULL) {
        std::stringstream ss;
        ss << "unable to create PCAP file: " << pcap;
        throw TrexException(ss.str());
    }

    assert(limit > 0);
    m_limit = limit;
}

void
RXPacketRecorder::stop() {
    if (m_writer) {
        delete m_writer;
        m_writer = NULL;
    }
}

void
RXPacketRecorder::handle_pkt(const rte_mbuf_t *m) {
    if (!m_writer) {
        return;
    }

    dsec_t now = now_sec();
    if (m_epoch < 0) {
        m_epoch = now;
    }

    dsec_t dt = now - m_epoch;

    CPktNsecTimeStamp t_c(dt);
    m_pkt.time_nsec = t_c.m_time_nsec;
    m_pkt.time_sec  = t_c.m_time_sec;

    const uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);
    m_pkt.pkt_len = m->pkt_len;
    memcpy(m_pkt.raw, p, m->pkt_len);
    
    m_writer->write_packet(&m_pkt);

    m_limit--;
    if (m_limit == 0) {
        stop();
    }
}
