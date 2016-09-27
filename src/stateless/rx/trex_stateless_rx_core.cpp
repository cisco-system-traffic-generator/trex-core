/*
  Ido Barnea
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

#include <stdio.h>
#include "bp_sim.h"
#include "flow_stat_parser.h"
#include "stateful_rx_core.h"
#include "pal/linux/sanb_atomic.h"
#include "trex_stateless_messaging.h"
#include "trex_stateless_rx_core.h"

void CRFC2544Info::create() {
    m_latency.Create();
    m_exp_flow_seq = 0;
    m_prev_flow_seq = 0;
    reset();
}

// after calling stop, packets still arriving will be considered error
void CRFC2544Info::stop() {
    m_prev_flow_seq = m_exp_flow_seq;
    m_exp_flow_seq = FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ;
}

void CRFC2544Info::reset() {
    // This is the seq num value we expect next packet to have.
    // Init value should match m_seq_num in CVirtualIFPerSideStats
    m_seq = UINT32_MAX - 1;  // catch wrap around issues early
    m_seq_err = 0;
    m_seq_err_events_too_big = 0;
    m_seq_err_events_too_low = 0;
    m_ooo = 0;
    m_dup = 0;
    m_latency.Reset();
    m_jitter.reset();
}

void CRFC2544Info::export_data(rfc2544_info_t_ &obj) {
    std::string json_str;
    Json::Reader reader;
    Json::Value json;

    obj.set_err_cntrs(m_seq_err, m_ooo, m_dup, m_seq_err_events_too_big, m_seq_err_events_too_low);
    obj.set_jitter(m_jitter.get_jitter());
    m_latency.dump_json(json);
    obj.set_latency_json(json);
};

void CCPortLatencyStl::reset() {
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        m_rx_pg_stat[i].clear();
        m_rx_pg_stat_payload[i].clear();
    }
}

void CRxCoreStateless::create(const CRxSlCfg &cfg) {
    m_rcv_all = false;
    m_capture = false;
    m_max_ports = cfg.m_max_ports;

    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_ring_to_cp   = cp_rx->getRingDpToCp(0);
    m_state = STATE_IDLE;

    for (int i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPortStl * lp = &m_ports[i];
        lp->m_io = cfg.m_ports[i];
        lp->m_port.reset();
    }
    m_cpu_cp_u.Create(&m_cpu_dp_u);

    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544[i].create();
    }
}

void CRxCoreStateless::handle_cp_msg(TrexStatelessCpToRxMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

void CRxCoreStateless::tickle() {
    m_monitor.tickle();
}

bool CRxCoreStateless::periodic_check_for_cp_messages() {

    /* tickle the watchdog */
    tickle();

    /* fast path */
    if ( likely ( m_ring_from_cp->isEmpty() ) ) {
        return false;
    }

    while ( true ) {
        CGenNode * node = NULL;

        if (m_ring_from_cp->Dequeue(node) != 0) {
            break;
        }
        assert(node);
        TrexStatelessCpToRxMsgBase * msg = (TrexStatelessCpToRxMsgBase *)node;
        handle_cp_msg(msg);
    }

    return true;

}

void CRxCoreStateless::idle_state_loop() {
    const int SHORT_DELAY_MS    = 2;
    const int LONG_DELAY_MS     = 50;
    const int DEEP_SLEEP_LIMIT  = 2000;

    int counter = 0;

    while (m_state == STATE_IDLE) {
        bool had_msg = periodic_check_for_cp_messages();
        if (had_msg) {
            counter = 0;
            continue;
        } else {
            flush_rx();
        }

        /* enter deep sleep only if enough time had passed */
        if (counter < DEEP_SLEEP_LIMIT) {
            delay(SHORT_DELAY_MS);
            counter++;
        } else {
            delay(LONG_DELAY_MS);
        }
    }
}

void CRxCoreStateless::start() {
    int count = 0;
    int i = 0;
    bool do_try_rx_queue =CGlobalInfo::m_options.preview.get_vm_one_queue_enable() ? true : false;

    /* register a watchdog handle on current core */
    m_monitor.create("STL RX CORE", 1);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    while (true) {
        if (m_state == STATE_WORKING) {
            i++;
            if (i == 100000) { // approx 10msec
                i = 0;
                periodic_check_for_cp_messages(); // m_state might change in here
            }
        } else {
            if (m_state == STATE_QUIT)
                break;
            count = 0;
            i = 0;
            set_working_msg_ack(false);
            idle_state_loop();
            set_working_msg_ack(true);
        }
        if (do_try_rx_queue) {
            try_rx_queues();
        }
        count += try_rx();
    }
    rte_pause();

    m_monitor.disable();
}

void CRxCoreStateless::handle_rx_pkt(CLatencyManagerPerPortStl *lp, rte_mbuf_t *m) {
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
                            m_err_cntrs.m_bad_header++;
                    } else {
                        curr_rfc2544 = &m_rfc2544[hw_id];

                        if (fsp_head->flow_seq != curr_rfc2544->get_exp_flow_seq()) {
                            // bad flow seq num
                            // Might be the first packet of a new flow, packet from an old flow, or garbage.

                            if (fsp_head->flow_seq == curr_rfc2544->get_prev_flow_seq()) {
                                // packet from previous flow using this hw_id that arrived late
                                good_packet = false;
                                m_err_cntrs.m_old_flow++;
                            } else {
                                if (curr_rfc2544->no_flow_seq()) {
                                    // first packet we see from this flow
                                    good_packet = true;
                                    curr_rfc2544->set_exp_flow_seq(fsp_head->flow_seq);
                                } else {
                                    // garbage packet
                                    good_packet = false;
                                    m_err_cntrs.m_bad_header++;
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
                        lp->m_port.m_rx_pg_stat_payload[hw_id].add_pkts(1);
                        lp->m_port.m_rx_pg_stat_payload[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                        uint64_t d = (os_get_hr_tick_64() - fsp_head->time_stamp );
                        dsec_t ctime = ptime_convert_hr_dsec(d);
                        curr_rfc2544->add_sample(ctime);
                    }
                } else {
                    hw_id = get_hw_id(ip_id);
                    if (hw_id < MAX_FLOW_STATS) {
                        lp->m_port.m_rx_pg_stat[hw_id].add_pkts(1);
                        lp->m_port.m_rx_pg_stat[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
                    }
                }
            }
        }
    }
}

void CRxCoreStateless::capture_pkt(rte_mbuf_t *m) {

}

// In VM setup, handle packets coming as messages from DP cores.
void CRxCoreStateless::handle_rx_queue_msgs(uint8_t thread_id, CNodeRing * r) {
    while ( true ) {
        CGenNode * node;
        if ( r->Dequeue(node) != 0 ) {
            break;
        }
        assert(node);

        CGenNodeMsgBase * msg = (CGenNodeMsgBase *)node;
        CGenNodeLatencyPktInfo * l_msg;
        uint8_t msg_type =  msg->m_msg_type;
        uint8_t rx_port_index;
        CLatencyManagerPerPortStl * lp;

        switch (msg_type) {
        case CGenNodeMsgBase::LATENCY_PKT:
            l_msg = (CGenNodeLatencyPktInfo *)msg;
            assert(l_msg->m_latency_offset == 0xdead);
            rx_port_index = (thread_id << 1) + (l_msg->m_dir & 1);
            assert( rx_port_index < m_max_ports );
            lp = &m_ports[rx_port_index];
            handle_rx_pkt(lp, (rte_mbuf_t *)l_msg->m_pkt);
            if (m_capture)
                capture_pkt((rte_mbuf_t *)l_msg->m_pkt);
            rte_pktmbuf_free((rte_mbuf_t *)l_msg->m_pkt);
            break;
        default:
            printf("ERROR latency-thread message type is not valid %d \n", msg_type);
            assert(0);
        }

        CGlobalInfo::free_node(node);
    }
}

// VM mode function. Handle messages from DP
void CRxCoreStateless::try_rx_queues() {

    CMessagingManager * rx_dp = CMsgIns::Ins()->getRxDp();
    uint8_t threads=CMsgIns::Ins()->get_num_threads();
    int ti;
    for (ti = 0; ti < (int)threads; ti++) {
        CNodeRing * r = rx_dp->getRingDpToCp(ti);
        if ( ! r->isEmpty() ) {
            handle_rx_queue_msgs((uint8_t)ti, r);
        }
    }
}

// exactly the same as try_rx, without the handle_rx_pkt
// purpose is to flush rx queues when core is in idle state
void CRxCoreStateless::flush_rx() {
    rte_mbuf_t * rx_pkts[64];
    int i, total_pkts = 0;
    for (i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPortStl * lp = &m_ports[i];
        rte_mbuf_t * m;
        /* try to read 64 packets clean up the queue */
        uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
        total_pkts += cnt_p;
        if (cnt_p) {
            m_cpu_dp_u.start_work1();
            int j;
            for (j = 0; j < cnt_p; j++) {
                m = rx_pkts[j];
                rte_pktmbuf_free(m);
            }
            /* commit only if there was work to do ! */
            m_cpu_dp_u.commit1();
        }/* if work */
    }// all ports
}

int CRxCoreStateless::try_rx() {
    rte_mbuf_t * rx_pkts[64];
    int i, total_pkts = 0;
    for (i = 0; i < m_max_ports; i++) {
        CLatencyManagerPerPortStl * lp = &m_ports[i];
        rte_mbuf_t * m;
        /* try to read 64 packets clean up the queue */
        uint16_t cnt_p = lp->m_io->rx_burst(rx_pkts, 64);
        total_pkts += cnt_p;
        if (cnt_p) {
            m_cpu_dp_u.start_work1();
            int j;
            for (j = 0; j < cnt_p; j++) {
                m = rx_pkts[j];
                handle_rx_pkt(lp, m);
                rte_pktmbuf_free(m);
            }
            /* commit only if there was work to do ! */
            m_cpu_dp_u.commit1();
        }/* if work */
    }// all ports
    return total_pkts;
}

bool CRxCoreStateless::is_flow_stat_id(uint32_t id) {
    if ((id & 0x000fff00) == IP_ID_RESERVE_BASE) return true;
    return false;
}

bool CRxCoreStateless::is_flow_stat_payload_id(uint32_t id) {
    if (id == FLOW_STAT_PAYLOAD_IP_ID) return true;
    return false;
}

uint16_t CRxCoreStateless::get_hw_id(uint16_t id) {
    return (0x00ff & id);
}

void CRxCoreStateless::reset_rx_stats(uint8_t port_id) {
    for (int hw_id = 0; hw_id < MAX_FLOW_STATS; hw_id++) {
        m_ports[port_id].m_port.m_rx_pg_stat[hw_id].clear();
    }
}

int CRxCoreStateless::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max
                                   , bool reset, TrexPlatformApi::driver_stat_cap_e type) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
            rx_stats[hw_id - min] = m_ports[port_id].m_port.m_rx_pg_stat_payload[hw_id];
        } else {
            rx_stats[hw_id - min] = m_ports[port_id].m_port.m_rx_pg_stat[hw_id];
        }
        if (reset) {
            if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                m_ports[port_id].m_port.m_rx_pg_stat_payload[hw_id].clear();
            } else {
                m_ports[port_id].m_port.m_rx_pg_stat[hw_id].clear();
            }
        }
    }
    return 0;
}

int CRxCoreStateless::get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        CRFC2544Info &curr_rfc2544 = m_rfc2544[hw_id];

        if (reset) {
            // need to stop first, so count will be consistent
            curr_rfc2544.stop();
        }

        curr_rfc2544.sample_period_end();
        curr_rfc2544.export_data(rfc2544_info[hw_id - min]);

        if (reset) {
            curr_rfc2544.reset();
        }
    }
    return 0;
}

int CRxCoreStateless::get_rx_err_cntrs(CRxCoreErrCntrs *rx_err) {
    *rx_err = m_err_cntrs;
    return 0;
}

void CRxCoreStateless::set_working_msg_ack(bool val) {
    sanb_smp_memory_barrier();
    m_ack_start_work_msg = val;
    sanb_smp_memory_barrier();
}


void CRxCoreStateless::update_cpu_util(){
    m_cpu_cp_u.Update();
}

double CRxCoreStateless::get_cpu_util() {
    return m_cpu_cp_u.GetVal();
}
