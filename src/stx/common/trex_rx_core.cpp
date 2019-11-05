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

#include <thread>
#include <stdio.h>
#include "bp_sim.h"
#include "flow_stat_parser.h"
#include "stateful_rx_core.h"
#include "trex_rx_core.h"
#include "trex_messaging.h"

CRxCore::~CRxCore(void) {
    for (auto &iter_pair : m_rx_port_mngr_map) {
        iter_pair.second->cleanup_async();
    }
    for (auto &iter_pair : m_rx_port_mngr_map) {
        iter_pair.second->wait_for_cleanup_done();
        delete iter_pair.second;
        iter_pair.second = nullptr;
    }
}


/* check for messages from DP cores */
uint32_t CRxCore::handle_msg_packets(void) {
    uint32_t pkts = 0;
    for (uint8_t thread_id=0; thread_id<m_tx_cores; thread_id++) {
        m_rx_dp->getRingCpToDp(thread_id)->Reschedule();

        CNodeRing *r = m_rx_dp->getRingDpToCp(thread_id);
        pkts += handle_rx_one_queue(thread_id, r);
    }
    return pkts;
}

uint32_t CRxCore::handle_rx_one_queue(uint8_t thread_id, CNodeRing *r) {
    CGenNode * node;
    uint32_t got_pkts;
    CFlowGenListPerThread* lpt = get_platform_api().get_fl()->m_threads_info[thread_id];
    uint8_t port1, port2, port_pair_offset;
    lpt->get_port_ids(port1, port2);
    port_pair_offset = port1 & port2;

    for ( got_pkts=0; got_pkts<64; got_pkts++ ) { // read at most 64 packets
        if ( r->Dequeue(node)!=0 ){
            break;
        }
        assert(node);

        CGenNodeLatencyPktInfo *pkt_info = (CGenNodeLatencyPktInfo *) node;
        assert(pkt_info->m_msg_type==CGenNodeMsgBase::LATENCY_PKT);
        assert(pkt_info->m_latency_offset==0xdead);
        uint8_t dir = pkt_info->m_dir & 1;
        m_rx_port_mngr_vec[port_pair_offset + dir]->handle_pkt((rte_mbuf_t *)pkt_info->m_pkt);

        CGlobalInfo::free_node(node);
    }
    return got_pkts;
}


void CRxCore::create(const CRxSlCfg &cfg) {
    m_rx_dp = CMsgIns::Ins()->getRxDp();
    m_capture          = false;
    m_tx_cores         = cfg.m_tx_cores;
    m_rx_pkts          = 0;
    m_sync_time_period = 1.0 / 1000;

    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_state = STATE_COLD;

    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544[i].create();
    }

    m_cpu_cp_u.Create(&m_cpu_dp_u);

    uint8_t max_port_id = 0;
    for (auto &port : cfg.m_ports) {
        max_port_id = std::max(max_port_id, port.first);
    }

    uint8_t ports_count = (max_port_id | 1) + 1;
    m_rx_port_mngr_vec.resize(ports_count, nullptr);

    /* create per port manager */
    for (auto &port : cfg.m_ports) {
        RXPortManager *mngr = new RXPortManager();
        mngr->create_async(port.first,
                           this,
                           port.second,
                           m_rfc2544,
                           &m_err_cntrs,
                           &m_cpu_dp_u);
        m_rx_port_mngr_map[port.first] = mngr;
        m_rx_port_mngr_vec[port.first] = mngr;
    }

    for (auto &iter_pair : m_rx_port_mngr_map) {
        iter_pair.second->wait_for_create_done();
    }

    /* create a TX queue */
    m_tx_queue.create(this, 5000);
}

void CRxCore::handle_cp_msg(TrexCpToRxMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

void CRxCore::tickle() {
    m_monitor.tickle();
}

bool CRxCore::periodic_check_for_cp_messages() {

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
        TrexCpToRxMsgBase * msg = (TrexCpToRxMsgBase *)node;
        handle_cp_msg(msg);
    }

    /* a message might result in a change of state */
    recalculate_next_state();
    return true;
}


void CRxCore::recalculate_next_state() {
    if (m_state == STATE_QUIT) {
        return;
    }

    /* only latency requires the 'hot' state */
    m_state = (should_be_hot() ? STATE_HOT : STATE_COLD);
}


/**
 * return true if any port has latency enabled
 */
bool CRxCore::is_latency_active() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        if (mngr_pair.second->is_feature_set(RXPortManager::LATENCY)) {
            return true;
        }
    }

    return false;
}

/**
 * return true if features requiring quick response are enabled
 * (latency, capture, capwap proxy)
 */
bool CRxCore::should_be_hot() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        if ( TrexCaptureMngr::getInstance().is_active(mngr_pair.first)
            || mngr_pair.second->is_feature_set(RXPortManager::LATENCY)
            || mngr_pair.second->is_feature_set(RXPortManager::CAPWAP_PROXY)
            || mngr_pair.second->is_feature_set(RXPortManager::CAPTURE_PORT) ) {
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
void CRxCore::hot_state_loop() {
    
    while (m_state == STATE_HOT) {
        if (!work_tick()) {
            rte_pause_or_delay_lowend();
        } else {
            delay_lowend();
        }
    }
}


/**
 * on cold state loop we adapt to work actually done
 * 
 */
void CRxCore::cold_state_loop() {
    const uint32_t COLD_LIMIT_ITER = 10000000;
    const uint32_t COLD_SLEEP_MS  = 10;

    int counter = 0;
 
    while (m_state == STATE_COLD) {
        bool did_something = work_tick();
        if (did_something) {
            counter = 0;
            /* we are hot - continue with no delay (unless lowend)*/
            delay_lowend();
            continue;
        }
        
        if (counter < COLD_LIMIT_ITER) {
            /* hot stage */
            counter++;
            rte_pause_or_delay_lowend();
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
void CRxCore::init_work_stage() {
    
    /* set the next sync time to */
    dsec_t now = now_sec();
    m_sync_time_sec = now + m_sync_time_period;
    m_grat_arp_sec  = now + (double)CGlobalInfo::m_options.m_arp_ref_per;
}

/**
 * performs once tick of work
 * return true if anything was done
 */
bool CRxCore::work_tick() {
    
    bool did_something = false;
    int pkt_cnt;
    int limit = 30;

    /* TODO: add a scheduler - will solve all problems here */
    
    /* continue while pending packets are waiting or limit reached */
    do {
        pkt_cnt = process_all_pending_pkts();
        m_rx_pkts += pkt_cnt;
        limit--;
        did_something |= (pkt_cnt > 0);
    } while ( pkt_cnt && limit );
    
    dsec_t now = now_sec();

    /* until a scheduler is added here - dirty IFs */

    if ( (now - m_sync_time_sec) > 0 ) {

        did_something |= handle_msg_packets();

        if (periodic_check_for_cp_messages()) {
            did_something = true;
        }
        
        m_sync_time_sec = now + m_sync_time_period;
    }

    /* pass a tick to the TX queue
     
       */
    m_tx_queue.tick();
    
    return did_something;
}


bool CRxCore::tx_pkt(rte_mbuf_t *m, uint8_t tx_port_id) {
    return m_rx_port_mngr_vec[tx_port_id]->tx_pkt(m);
}


bool CRxCore::tx_pkt(const std::string &pkt, uint8_t tx_port_id) {
    return m_rx_port_mngr_vec[tx_port_id]->tx_pkt(pkt);
}


int CRxCore::_do_start(void){

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
    return (0);
}


void CRxCore::start() {
    
    /* mark the core as active (used for accessing from other cores) */
    m_is_active = true;
       
    /* register a watchdog handle on current core */
    m_monitor.create("STL RX CORE", 1);
    TrexWatchDog::getInstance().register_monitor(&m_monitor);

    recalculate_next_state();
    
    init_work_stage();

    (void)_do_start();

    m_monitor.disable();

    m_is_active = false;
    
}

void CRxCore::handle_astf_latency_pkt(const rte_mbuf_t *m,
                                      uint8_t port_id){
    /* nothing to do */

}

int CRxCore::process_all_pending_pkts(bool flush_rx) {

    int total_pkts = 0;
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        total_pkts += mngr_pair.second->process_all_pending_pkts(flush_rx);
    }

    return total_pkts;

}

void CRxCore::reset_rx_stats(uint8_t port_id) {
    m_rx_port_mngr_vec[port_id]->clear_stats();
}

int CRxCore::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max
                                   , bool reset, TrexPlatformApi::driver_stat_cap_e type) {

    /* for now only latency stats */
    m_rx_port_mngr_vec[port_id]->get_latency_stats(rx_stats, min, max, reset, type);
    
    return (0);
    
}

/*
 * Get RFC 2544 information from histogram.
 * If just need to force average and jitter calculation, can send with rfc2544_info = NULL
 *  and period_switch = True.
 */
int CRxCore::get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset
                                       , bool period_switch) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        CRFC2544Info &curr_rfc2544 = m_rfc2544[hw_id];

        if (reset) {
            // need to stop first, so count will be consistent
            curr_rfc2544.stop();
        }

        if (period_switch) {
            curr_rfc2544.sample_period_end();
        }
        if (rfc2544_info != NULL) {
            curr_rfc2544.export_data(rfc2544_info[hw_id - min]);
        }

        if (reset) {
            curr_rfc2544.reset();
        }
    }
    return 0;
}

int CRxCore::get_rx_err_cntrs(CRxCoreErrCntrs *rx_err) {
    *rx_err = m_err_cntrs;
    return 0;
}

void CRxCore::update_cpu_util(){
    m_cpu_cp_u.Update();
}

double CRxCore::get_cpu_util() {
    return m_cpu_cp_u.GetVal();
}


void
CRxCore::start_queue(uint8_t port_id, uint64_t size) {
    m_rx_port_mngr_vec[port_id]->start_queue(size);
    recalculate_next_state();
}

void
CRxCore::stop_queue(uint8_t port_id) {
    m_rx_port_mngr_vec[port_id]->stop_queue();
    recalculate_next_state();
}

bool
CRxCore::start_capwap_proxy(uint8_t port_id, uint8_t pair_port_id, bool is_wireless_side, Json::Value capwap_map, uint32_t wlc_ip) {
    bool rc;
    rc = m_rx_port_mngr_vec[port_id]->start_capwap_proxy(pair_port_id, is_wireless_side, capwap_map, wlc_ip);
    recalculate_next_state();
    return rc;
}

void
CRxCore::stop_capwap_proxy(uint8_t port_id) {
    m_rx_port_mngr_vec[port_id]->stop_capwap_proxy();
    recalculate_next_state();
}

void
CRxCore::enable_latency() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        mngr_pair.second->enable_latency();
        // This is a sending part for time synchronisation.  Enable it only if `timesync-method`
        // is set and `timesync-interval` equals 0.
        if ((CGlobalInfo::m_options.m_timesync_method != CParserOption::TIMESYNC_NONE) &&
            (CGlobalInfo::m_options.m_timesync_interval == 0)) {
            mngr_pair.second->enable_timesync(CGlobalInfo::m_options.m_timesync_method);
        }
    }
    
    recalculate_next_state();
}

void
CRxCore::enable_astf_latency_fia(bool enable) {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        mngr_pair.second->enable_astf_latency(enable);
    }
    
    recalculate_next_state();
}


void
CRxCore::disable_latency() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        if (CGlobalInfo::m_options.m_timesync_method != CParserOption::TIMESYNC_NONE) {
            mngr_pair.second->disable_timesync();
        }
        mngr_pair.second->disable_latency();
    }
    
    recalculate_next_state();
}

const TrexPktBuffer *CRxCore::get_rx_queue_pkts(uint8_t port_id) {
    return m_rx_port_mngr_vec[port_id]->get_pkt_buffer();
}

RXPortManager &CRxCore::get_rx_port_mngr(uint8_t port_id) {
    return *m_rx_port_mngr_vec[port_id];
}

void
CRxCore::get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff) {
    m_rx_port_mngr_vec[port_id]->get_ignore_stats(stat, get_diff);
}


bool CRxCore::has_port(const std::string &mac_buf) {
    assert(mac_buf.size()==6);
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        if ( mngr_pair.second->get_stack()->has_port(mac_buf) ) {
            return true;
        }
    }
    return false;
}

/**
 * sends packets through the RX core TX queue
 * 
*/
uint32_t
CRxCore::tx_pkts(int port_id, const std::vector<std::string> &pkts, uint32_t ipg_usec) {

    double time_to_send_sec = now_sec();
    uint32_t pkts_sent      = 0;
    
    for (const auto &pkt : pkts) {
        bool rc = m_tx_queue.push(port_id, pkt, time_to_send_sec);
        if (!rc) {
            break;
            
        }
        
        pkts_sent++;
        time_to_send_sec += usec_to_sec(ipg_usec);
    }
    
    return pkts_sent;
}

/**
 * forward the actual send to the port manager
 */
bool
CRxCore::tx_pkt(int port_id, const std::string &pkt) {
    return m_rx_port_mngr_vec[port_id]->tx_pkt(pkt);
}

