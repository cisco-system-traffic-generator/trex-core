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
#include "trex_stateless_rx_core.h"

/************************** latency feature ************/
void RXLatency::handle_pkt(const rte_mbuf_t *m) {
    CFlowStatParser parser;

    if (m_rcv_all || parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len) == 0) {
        uint32_t ip_id;
        if (m_rcv_all || (parser.get_ip_id(ip_id) == 0)) {
            if (m_rcv_all || is_flow_stat_id(ip_id)) {
                uint16_t hw_id;
                if (m_rcv_all || is_flow_stat_payload_id(ip_id)) {
                    bool good_packet = true;
                    uint8_t *p = rte_pktmbuf_mtod(m, uint8_t*);
                    struct flow_stat_payload_header *fsp_head = (struct flow_stat_payload_header *)
                        (p + m->pkt_len - sizeof(struct flow_stat_payload_header));
                    hw_id = fsp_head->hw_id;
                    CRFC2544Info *curr_rfc2544;

                    if (unlikely(fsp_head->magic != FLOW_STAT_PAYLOAD_MAGIC) || hw_id >= MAX_FLOW_STATS_PAYLOAD) {
                        good_packet = false;
                        if (!m_rcv_all)
                            m_err_cntrs->m_bad_header++;
                    } else {
                        curr_rfc2544 = &m_rfc2544[hw_id];

                        if (fsp_head->flow_seq != curr_rfc2544->get_exp_flow_seq()) {
                            // bad flow seq num
                            // Might be the first packet of a new flow, packet from an old flow, or garbage.

                            if (fsp_head->flow_seq == curr_rfc2544->get_prev_flow_seq()) {
                                // packet from previous flow using this hw_id that arrived late
                                good_packet = false;
                                m_err_cntrs->m_old_flow++;
                            } else {
                                if (curr_rfc2544->no_flow_seq()) {
                                    // first packet we see from this flow
                                    good_packet = true;
                                    curr_rfc2544->set_exp_flow_seq(fsp_head->flow_seq);
                                } else {
                                    // garbage packet
                                    good_packet = false;
                                    m_err_cntrs->m_bad_header++;
                                }
                            }
                        }
                    }

                    if (good_packet) {
                        uint32_t pkt_seq = fsp_head->seq;
                        uint32_t exp_seq = curr_rfc2544->get_seq();
                        if (unlikely(pkt_seq != exp_seq)) {
                            if (pkt_seq < exp_seq) {
                                if (exp_seq - pkt_seq > 100000) {
                                    // packet loss while we had wrap around
                                    curr_rfc2544->inc_seq_err(pkt_seq - exp_seq);
                                    curr_rfc2544->inc_seq_err_too_big();
                                    curr_rfc2544->set_seq(pkt_seq + 1);
                                } else {
                                    if (pkt_seq == (exp_seq - 1)) {
                                        curr_rfc2544->inc_dup();
                                    } else {
                                        curr_rfc2544->inc_ooo();
                                        // We thought it was lost, but it was just out of order
                                        curr_rfc2544->dec_seq_err();
                                    }
                                    curr_rfc2544->inc_seq_err_too_low();
                                }
                            } else {
                                if (unlikely (pkt_seq - exp_seq > 100000)) {
                                    // packet reorder while we had wrap around
                                    if (pkt_seq == (exp_seq - 1)) {
                                        curr_rfc2544->inc_dup();
                                    } else {
                                        curr_rfc2544->inc_ooo();
                                        // We thought it was lost, but it was just out of order
                                        curr_rfc2544->dec_seq_err();
                                    }
                                    curr_rfc2544->inc_seq_err_too_low();
                                } else {
                                // seq > curr_rfc2544->seq. Assuming lost packets
                                    curr_rfc2544->inc_seq_err(pkt_seq - exp_seq);
                                    curr_rfc2544->inc_seq_err_too_big();
                                    curr_rfc2544->set_seq(pkt_seq + 1);
                                }
                            }
                        } else {
                            curr_rfc2544->set_seq(pkt_seq + 1);
                        }
                        m_rx_pg_stat_payload[hw_id].add_pkts(1);
                        m_rx_pg_stat_payload[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                        uint64_t d = (os_get_hr_tick_64() - fsp_head->time_stamp );
                        dsec_t ctime = ptime_convert_hr_dsec(d);
                        curr_rfc2544->add_sample(ctime);
                    }
                } else {
                    hw_id = get_hw_id(ip_id);
                    if (hw_id < MAX_FLOW_STATS) {
                        m_rx_pg_stat[hw_id].add_pkts(1);
                        m_rx_pg_stat[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                    }
                }
            }
        }
    }
}

void
RXLatency::reset_stats() {
    for (int hw_id = 0; hw_id < MAX_FLOW_STATS; hw_id++) {
        m_rx_pg_stat[hw_id].clear();
    }
}

/****************************** packet recorder ****************************/

RXPacketRecorder::RXPacketRecorder() {
    m_writer = NULL;
    m_shared_counter = NULL;
    m_limit  = 0;
    m_epoch  = -1;
}

RXPacketRecorder::~RXPacketRecorder() {
    stop();
}

void
RXPacketRecorder::start(const std::string &pcap, uint64_t limit, uint64_t *shared_counter) {
    m_writer = CCapWriterFactory::CreateWriter(LIBPCAP, (char *)pcap.c_str());
    if (m_writer == NULL) {
        std::stringstream ss;
        ss << "unable to create PCAP file: " << pcap;
        throw TrexException(ss.str());
    }

    assert(limit > 0);
    m_limit = limit;
    m_shared_counter = shared_counter;
    (*m_shared_counter) = 0;
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
    (*m_shared_counter)++;

    if (m_limit == 0) {
        stop();
    }
}


void RXPortManager::handle_pkt(const rte_mbuf_t *m) {

    /* handle features */

    if (is_feature_set(LATENCY)) {
        m_latency.handle_pkt(m);
    }

    if (is_feature_set(CAPTURE)) {
        m_recorder.handle_pkt(m);
    }

    if (is_feature_set(QUEUE)) {
        m_pkt_buffer->push(new RxPacket(m));
    }
}


int RXPortManager::process_all_pending_pkts(bool flush_rx) {

    rte_mbuf_t *rx_pkts[64];

    /* try to read 64 packets clean up the queue */
    uint16_t cnt_p = m_io->rx_burst(rx_pkts, 64);
    if (cnt_p == 0) {
        return cnt_p;
    }


    m_cpu_dp_u->start_work1();

    for (int j = 0; j < cnt_p; j++) {
        rte_mbuf_t *m = rx_pkts[j];

        if (!flush_rx) {
            handle_pkt(m);
        }

        rte_pktmbuf_free(m);
    }

    /* commit only if there was work to do ! */
    m_cpu_dp_u->commit1();


    return cnt_p;
}

