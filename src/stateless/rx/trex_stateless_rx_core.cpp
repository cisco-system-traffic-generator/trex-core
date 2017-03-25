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
    if (m_exp_flow_seq != FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ) {
        m_prev_flow_seq = m_exp_flow_seq;
        m_exp_flow_seq = FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ;
    }
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
    m_tx_cores  = cfg.m_tx_cores;
    
    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_state = STATE_COLD;

    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544[i].create();
    }

    m_cpu_cp_u.Create(&m_cpu_dp_u);

    /* create per port manager */
    for (int i = 0; i < m_max_ports; i++) {
        const TRexPortAttr *port_attr = get_stateless_obj()->get_platform_api()->getPortAttrObj(i);
        m_rx_port_mngr[i].create(port_attr,
                                 cfg.m_ports[i],
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

    /* a message might result in a change of state */
    recalculate_next_state();
    return true;

}


void CRxCoreStateless::recalculate_next_state() {
    if (m_state == STATE_QUIT) {
        return;
    }

    /* only latency requires the 'hot' state */
    m_state = (is_latency_active() ? STATE_HOT : STATE_COLD);
}


/**
 * return true if any port has latency enabled
 */
bool CRxCoreStateless::is_latency_active() {
    for (int i = 0; i < m_max_ports; i++) {
        if (m_rx_port_mngr[i].is_feature_set(RXPortManager::LATENCY)) {
            return true;
        }
    }

    return false;
}


/**
 * when in hot state we busy poll as fast as possible 
 * because of latency packets 
 * 
 */
void CRxCoreStateless::hot_state_loop() {
    
    while (m_state == STATE_HOT) {
        work_tick();
        rte_pause();
    }
}


/**
 * on cold state loop we adapt to work actually done
 * 
 */
void CRxCoreStateless::cold_state_loop() {
    const uint32_t COLD_LIMIT_ITER = 10000000;
    const uint32_t COLD_SLEEP_MS  = 10;

    int counter = 0;
 
    while (m_state == STATE_COLD) {
        bool did_something = work_tick();
        if (did_something) {
            counter = 0;
            /* we are hot - continue with no delay */
            continue;
        }
        
        if (counter < COLD_LIMIT_ITER) {
            /* hot stage */
            counter++;
            rte_pause();
        } else {
            /* cold stage */
            delay(COLD_SLEEP_MS);
        }
        
    }
}

/**
 * init the 'IF' scheduler values
 * 
 * @author imarom (2/20/2017)
 */
void CRxCoreStateless::init_work_stage() {
    
    /* set the next sync time to */
    m_sync_time_sec = now_sec() + (1.0 / 1000);
    m_grat_arp_sec  = now_sec() + (double)CGlobalInfo::m_options.m_arp_ref_per;
}
    
/**
 * performs once tick of work
 * return true if anything was done
 */
bool CRxCoreStateless::work_tick() {
    bool did_something = false;
    
    /* if any packet arrived - mark as */
    if (process_all_pending_pkts()) {
        did_something = true;
    }
    
    dsec_t now = now_sec();
        
    /* until a scheduler is added here - dirty IFs */

    if ( (now - m_sync_time_sec) > 0 ) {

        if (periodic_check_for_cp_messages()) {
            did_something = true;
        }
        
        m_sync_time_sec = now + (1.0 / 1000);
    }
        
    if ( (now - m_grat_arp_sec) > 0) {
        handle_grat_arp();
        m_grat_arp_sec = now + (double)CGlobalInfo::m_options.m_arp_ref_per;
    }
    
    return did_something;
}

/**
 * for each port handle the grat ARP mechansim
 * 
 */
void CRxCoreStateless::handle_grat_arp() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].send_next_grat_arp();
    }
}


void CRxCoreStateless::start() {
    /* register a watchdog handle on current core */
    m_monitor.create("STL RX CORE", 1);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    recalculate_next_state();
    
    init_work_stage();
    
    while (m_state != STATE_QUIT) {

        switch (m_state) {

        case STATE_HOT:
            hot_state_loop();
            break;
            
        case STATE_COLD:
            cold_state_loop();
            break;

        default:
            assert(0);
            break;
        }

    }

    m_monitor.disable();  
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

    /* for now only latency stats */
    m_rx_port_mngr[port_id].get_latency_stats(rx_stats, min, max, reset, type);
    
    return (0);
    
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

void CRxCoreStateless::update_cpu_util(){
    m_cpu_cp_u.Update();
}

double CRxCoreStateless::get_cpu_util() {
    return m_cpu_cp_u.GetVal();
}


void
CRxCoreStateless::start_queue(uint8_t port_id, uint64_t size) {
    m_rx_port_mngr[port_id].start_queue(size);
    recalculate_next_state();
}

void
CRxCoreStateless::stop_queue(uint8_t port_id) {
    m_rx_port_mngr[port_id].stop_queue();
    recalculate_next_state();
}

void
CRxCoreStateless::enable_latency() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].enable_latency();
    }
    
    recalculate_next_state();
}

void
CRxCoreStateless::disable_latency() {
    for (int i = 0; i < m_max_ports; i++) {
        m_rx_port_mngr[i].disable_latency();
    }
    
    recalculate_next_state();
}

RXPortManager &
CRxCoreStateless::get_rx_port_mngr(uint8_t port_id) {
    assert(port_id < m_max_ports);
    return m_rx_port_mngr[port_id];
    
}

void
CRxCoreStateless::get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff) {
    get_rx_port_mngr(port_id).get_ignore_stats(stat, get_diff);
}
