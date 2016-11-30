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
#include "trex_stateless.h"


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

void CRxCoreStateless::create(const CRxSlCfg &cfg) {
    m_capture = false;
    m_max_ports = cfg.m_max_ports;

    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_ring_to_cp   = cp_rx->getRingDpToCp(0);
    m_state = STATE_IDLE;

    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544[i].create();
    }

    m_cpu_cp_u.Create(&m_cpu_dp_u);

    /* create per port manager */
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].create(cfg.m_ports[i],
                                 m_rfc2544,
                                 &m_err_cntrs,
                                 &m_cpu_dp_u);
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

    recalculate_next_state();
    return true;

}

void CRxCoreStateless::recalculate_next_state() {
    if (m_state == STATE_QUIT) {
        return;
    }

    /* next state is determine by the question are there any ports with active features ? */
    m_state = (are_any_features_active() ? STATE_WORKING : STATE_IDLE);
}

bool CRxCoreStateless::are_any_features_active() {
    for (int i = 0; i < m_max_ports; i++) {
        if (m_rx_port_mngr[i].has_features_set()) {
            return true;
        }
    }

    return false;
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
            flush_all_pending_pkts();
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

/**
 * for each port give a tick (for flushing if needed)
 * 
 */
void CRxCoreStateless::port_manager_tick() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].tick();
    }
}

void CRxCoreStateless::handle_work_stage(bool do_try_rx_queue) {
    int i = 0;
    int j = 0;
    
    while (m_state == STATE_WORKING) {

        if (do_try_rx_queue) {
            try_rx_queues();
        }

        process_all_pending_pkts();

        /* TODO: with scheduler, this should be solved better */
        i++;
        if (i == 100000) { // approx 10msec
            i = 0;
            periodic_check_for_cp_messages(); // m_state might change in here
            
            j++;
            if (j == 100) { // approx 1 sec
                j = 0;
                port_manager_tick();
            }
        }
        
        rte_pause();
    }
}

void CRxCoreStateless::start() {
    bool do_try_rx_queue = CGlobalInfo::m_options.preview.get_vm_one_queue_enable() ? true : false;

    /* register a watchdog handle on current core */
    m_monitor.create("STL RX CORE", 1);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    while (m_state != STATE_QUIT) {

        switch (m_state) {
        case STATE_IDLE:
            set_working_msg_ack(false);
            idle_state_loop();
            set_working_msg_ack(true);
            break;

        case STATE_WORKING:
            handle_work_stage(do_try_rx_queue);
            break;

        default:
            assert(0);
            break;
        }

    }

    m_monitor.disable();  
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


        switch (msg_type) {
        case CGenNodeMsgBase::LATENCY_PKT:
            l_msg = (CGenNodeLatencyPktInfo *)msg;
            assert(l_msg->m_latency_offset == 0xdead);
            rx_port_index = (thread_id << 1) + (l_msg->m_dir & 1);
            assert( rx_port_index < m_max_ports );

            m_rx_port_mngr[rx_port_index].handle_pkt((rte_mbuf_t *)l_msg->m_pkt);

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

int CRxCoreStateless::process_all_pending_pkts(bool flush_rx) {

    int total_pkts = 0;
    for (int i = 0; i < m_max_ports; i++) {
        total_pkts += m_rx_port_mngr[i].process_all_pending_pkts(flush_rx);
    }

    return total_pkts;

}


void CRxCoreStateless::reset_rx_stats(uint8_t port_id) {
    m_rx_port_mngr[port_id].clear_stats();
}

int CRxCoreStateless::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max
                                   , bool reset, TrexPlatformApi::driver_stat_cap_e type) {

    RXLatency &latency = m_rx_port_mngr[port_id].get_latency();

    for (int hw_id = min; hw_id <= max; hw_id++) {
        if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
            rx_stats[hw_id - min] = latency.m_rx_pg_stat_payload[hw_id];
        } else {
            rx_stats[hw_id - min] = latency.m_rx_pg_stat[hw_id];
        }
        if (reset) {
            if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                latency.m_rx_pg_stat_payload[hw_id].clear();
            } else {
                latency.m_rx_pg_stat[hw_id].clear();
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


void
CRxCoreStateless::start_recorder(uint8_t port_id, const std::string &pcap_filename, uint64_t limit) {
    m_rx_port_mngr[port_id].start_recorder(pcap_filename, limit);
}

void
CRxCoreStateless::stop_recorder(uint8_t port_id) {
    m_rx_port_mngr[port_id].stop_recorder();
}

void
CRxCoreStateless::start_queue(uint8_t port_id, uint64_t size) {
    m_rx_port_mngr[port_id].start_queue(size);
}

void
CRxCoreStateless::stop_queue(uint8_t port_id) {
    m_rx_port_mngr[port_id].stop_queue();
}

void
CRxCoreStateless::enable_latency() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].enable_latency();
    }
}

void
CRxCoreStateless::disable_latency() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].disable_latency();
    }
}

const RXPortManager &
CRxCoreStateless::get_rx_port_mngr(uint8_t port_id) {
    assert(port_id < m_max_ports);
    return m_rx_port_mngr[port_id];
    
}
