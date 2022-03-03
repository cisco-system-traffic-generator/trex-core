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
#include <zmq.h>


CRxCore::~CRxCore(void) {
    for (auto &iter_pair : m_rx_port_mngr_map) {
        iter_pair.second->cleanup_async();
    }
    for (auto &iter_pair : m_rx_port_mngr_map) {
        iter_pair.second->wait_for_cleanup_done();
        delete iter_pair.second;
        iter_pair.second = nullptr;
    }

    // Remove active TPG contexts, no need to erase from map since the object is getting destroyed.
    for (auto& iter_pair : m_tpg_ctx) {
        delete iter_pair.second;
    }
}


/* check for messages from DP cores */
uint32_t CRxCore::handle_msg_packets(void) {
    uint32_t pkts = 0;
    for (uint8_t thread_id = 0; thread_id < m_tx_cores; thread_id++) {
        m_rx_dp->getRingCpToDp(thread_id)->Reschedule();

        CNodeRing* r = m_rx_dp->getRingDpToCp(thread_id);
        if (r->Size() > 0) {
            pkts += handle_rx_one_queue(thread_id, r);
        }
    }
    return pkts;
}

uint32_t CRxCore::handle_rx_one_queue(uint8_t thread_id, CNodeRing* r) {

    uint32_t ring_size = r->Size();

    if (ring_size == 0) {
        return 0;
    }

    // Work to do, let's update the CPU
    m_cpu_dp_u.start_work1();

    constexpr uint32_t MAX_BULK_SIZE = 256;
    uint32_t bulk_size = std::min(ring_size, MAX_BULK_SIZE);
    CGenNode* nodes[MAX_BULK_SIZE];
    assert(r->DequeueBulk(nodes, bulk_size)); // It must succeed, since bulk_size <= ring_size.
    for (int i = 0; i < bulk_size; i++) {
        handle_dp_msg(thread_id, nodes[i]);
    }

    // Work was done.
    m_cpu_dp_u.commit1();

    return bulk_size;
}

void CRxCore::handle_dp_msg(uint8_t thread_id, CGenNode* node) {
    assert(node);

    CFlowGenListPerThread* lpt = get_platform_api().get_fl()->m_threads_info[thread_id];
    uint8_t port1, port2, port_pair_offset;
    lpt->get_port_ids(port1, port2);
    port_pair_offset = port1 & port2;

    // For now only Latency packets are passed here.
    CGenNodeLatencyPktInfo* pkt_info = (CGenNodeLatencyPktInfo*)node;
    assert(pkt_info->m_msg_type==CGenNodeMsgBase::LATENCY_PKT);
    assert(pkt_info->m_latency_offset==0xdead);
    uint8_t dir = pkt_info->m_dir & 1;
    m_rx_port_mngr_vec[port_pair_offset + dir]->handle_pkt((rte_mbuf_t *)pkt_info->m_pkt);

    CGlobalInfo::free_node(node);
}


void CRxCore::create(const CRxSlCfg &cfg) {
    m_rx_dp = CMsgIns::Ins()->getRxDp();
    m_capture                 = false;
    m_tx_cores                = cfg.m_tx_cores;
    m_rx_pkts                 = 0;
    m_sync_time_period        = 1.0 / 1000;   // 1 ms
    m_sync_time_dp_msg_period = 20.0 / 1e6;   // 20 us

    CMessagingManager * cp_rx = CMsgIns::Ins()->getCpRx();

    m_ring_from_cp = cp_rx->getRingCpToDp(0);
    m_state = STATE_COLD;

    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544[i].create();
    }

    m_cpu_cp_u.Create(&m_cpu_dp_u);

    CParserOption * po =&CGlobalInfo::m_options;

    m_ex_zmq_enabled = po->m_ezmq_ch_enabled;
    m_ezmq_use_tcp = po->m_emzq_ch_tcp;

    if (m_ex_zmq_enabled) {
        create_zmq();
    }

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
        if (m_ex_zmq_enabled) {
            mngr->set_zmq_cn(&m_zmq_wr,&m_zmq_rd);
            mngr->set_feature(RXPortManager::EZMQ); /* enable the feature*/
        }
    }


    /* create a TX queue */
    m_tx_queue.create(this, 5000);

}

void  CRxCore::restart_zmq()  {
    delete_zmq();
    create_zmq();
}

void  CRxCore::create_zmq()  {
    // create zmq ch
    m_zmq_ctx = zmq_ctx_new();
    CParserOption * po =&CGlobalInfo::m_options;

    char buffer[100];
    if (m_ezmq_use_tcp) {
        sprintf(buffer,"tcp://*:%d",po->m_ezmq_ch_port);
    } else {
        sprintf(buffer,"ipc://%s-%d.ipc", po->m_emzq_ipc_file_path.c_str(), po->m_ezmq_ch_port);
    }

    create_zmq(m_zmq_rx_socket,buffer);

    if (m_ezmq_use_tcp) {
        sprintf(buffer,"tcp://*:%d",po->m_ezmq_ch_port+1);
    } else {
        sprintf(buffer,"ipc://%s-%d.ipc", po->m_emzq_ipc_file_path.c_str(), po->m_ezmq_ch_port + 1);
    }

    create_zmq(m_zmq_tx_socket,buffer);

    m_zmq_wr.Create(m_zmq_rx_socket,&m_err_cntrs);
    m_zmq_rd.Create(m_zmq_tx_socket,this,&m_err_cntrs);
}


void  CRxCore::delete_zmq()  {

    if (m_zmq_rx_socket) {
        zmq_close(m_zmq_rx_socket);
        m_zmq_rx_socket=0;
    }
    if (m_zmq_tx_socket) {
        zmq_close(m_zmq_tx_socket);
        m_zmq_tx_socket=0;
    }

    if (m_zmq_ctx){
        zmq_ctx_term(m_zmq_ctx);
        m_zmq_ctx=0;
    }
}


bool  CRxCore::create_zmq(void *   &socket,std::string server)  {
    int rc;
    socket = zmq_socket(m_zmq_ctx, ZMQ_PAIR);

    if (!socket) {
      printf("unable to create socket server at:%s \n", server.c_str());
      return false;
    }
    int linger = 0;
    rc = zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
    
    if (rc != 0) {
      printf("unable to set linger server at:%s \n", server.c_str());
      return false;
    }

    rc = zmq_bind(socket, server.c_str());

    if (rc != 0) {
      printf("unable to bind ZMQ server at:%s \n", server.c_str());
      return false;
    }

    return true; 
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
            || mngr_pair.second->is_feature_set(RXPortManager::CAPTURE_PORT)
            || mngr_pair.second->is_feature_set(RXPortManager::TPG)) {
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
    m_sync_time_sec        = now + m_sync_time_period;
    m_sync_time_dp_msg_sec = now + m_sync_time_dp_msg_period;
    m_grat_arp_sec         = now + (double)CGlobalInfo::m_options.m_arp_ref_per;
}

/**
 * performs once tick of work
 * return true if anything was done
 */
bool CRxCore::work_tick() {

    bool did_something = false;
    int pkt_cnt = 0;
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

    /**
     * DP can redirect packets as messages in a high rate, as such we check if there
     * are any packets each *m_sync_time_dp_msg_period* or if we got stopped last time when
     * we still had packets from DP but the *limit* passed.
     * 
     * We attempt reading packets in bulks.
     */
    constexpr uint8_t DP_MSG_ITERATIONS = 10;
    limit = DP_MSG_ITERATIONS;
    pkt_cnt = 0;

    if (m_dp_msg_hot || (now - m_sync_time_dp_msg_sec) > 0) {
        constexpr uint8_t NUM_PACKETS_THRESHOLD = 16;
        do {
            pkt_cnt = handle_msg_packets();
            m_dp_msg_hot = (pkt_cnt >= NUM_PACKETS_THRESHOLD);
            limit--;
            did_something |= (pkt_cnt > 0);
        } while ( m_dp_msg_hot && limit );

        if (unlikely(limit != DP_MSG_ITERATIONS - 1)) {
            // Might have been a while in the loop.
            now = now_sec();
        }

        m_sync_time_dp_msg_sec = now + m_sync_time_dp_msg_period;
    }

    /**
     * Following is the slow path. It happens once in *m_sync_time_period*.
     **/

    if ( (now - m_sync_time_sec) > 0 ) {

        if (periodic_check_for_cp_messages()) {
            did_something = true;
        }

        if (unlikely(m_working_tpg_threads != 0)) {
            did_something = handle_tpg_threads();
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

    if (m_ex_zmq_enabled){
       delete_zmq();
    }
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
    if (m_ex_zmq_enabled){
        if (m_zmq_wr.flush() ) {
            restart_zmq();
        }
        m_zmq_rd.pool_msg();
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


void CRxCore::start_queue(uint8_t port_id, uint64_t size) {
    m_rx_port_mngr_vec[port_id]->start_queue(size);
    recalculate_next_state();
}

void CRxCore::stop_queue(uint8_t port_id) {
    m_rx_port_mngr_vec[port_id]->stop_queue();
    recalculate_next_state();
}

bool CRxCore::start_capwap_proxy(uint8_t port_id, uint8_t pair_port_id, bool is_wireless_side, Json::Value capwap_map, uint32_t wlc_ip) {
    bool rc;
    rc = m_rx_port_mngr_vec[port_id]->start_capwap_proxy(pair_port_id, is_wireless_side, capwap_map, wlc_ip);
    recalculate_next_state();
    return rc;
}

bool CRxCore::add_to_capwap_proxy(uint8_t port_id, Json::Value capwap_map) {
    bool rc;
    rc = m_rx_port_mngr_vec[port_id]->add_to_capwap_proxy(capwap_map);
    recalculate_next_state();
    return rc;
}

void CRxCore::stop_capwap_proxy(uint8_t port_id) {
    m_rx_port_mngr_vec[port_id]->stop_capwap_proxy();
    recalculate_next_state();
}

void CRxCore::enable_latency() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        mngr_pair.second->enable_latency();
    }

    recalculate_next_state();
}

void CRxCore::enable_astf_latency_fia(bool enable) {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
        mngr_pair.second->enable_astf_latency(enable);
    }

    recalculate_next_state();
}


void CRxCore::disable_latency() {
    for (auto &mngr_pair : m_rx_port_mngr_map) {
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

void CRxCore::get_ignore_stats(int port_id, CRXCoreIgnoreStat &stat, bool get_diff) {
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
uint32_t CRxCore::tx_pkts(int port_id, const std::vector<std::string> &pkts, uint32_t ipg_usec) {

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
bool CRxCore::tx_pkt(int port_id, const std::string &pkt) {
    return m_rx_port_mngr_vec[port_id]->tx_pkt(pkt);
}

/**************************************
 * Tagged Packet Group
*************************************/

int CRxCore::get_tpg_state(const std::string& username) {

    if (tpg_ctx_exists(username)) {
        // Context doesn't exist.
        assert(m_tpg_ctx[username]);

        TPGRxState state = m_tpg_ctx[username]->get_state(); // Get state safely
        switch (state) {
            case TPGRxState::ALLOC_FAIL:
                return static_cast<int>(TPGStateUpdate::RX_ALLOC_FAIL);

            case TPGRxState::ENABLED:
            case TPGRxState::AWAITING_DISABLE:
                return static_cast<int>(TPGStateUpdate::RX_ENABLED);

            case TPGRxState::DISABLED:
            case TPGRxState::AWAITING_ENABLE:
                return static_cast<int>(TPGStateUpdate::RX_DISABLED);
        }
    }

    return static_cast<int>(TPGStateUpdate::RX_NO_EXIST);
}

bool CRxCore::tpg_ctx_exists(const std::string& username) {
    return (m_tpg_ctx.find(username) != m_tpg_ctx.end());
}

void CRxCore::enable_tpg_ctx(TPGCpCtx* tpg_cp_ctx) {
    /**
     * NOTE: All the statistics are allocated here.
     * We do this is another thread so that this function will be non blocking.
     * NOTE: The TPG context is *not* enabled after this function, since this is async.
     **/
    const std::string username = tpg_cp_ctx->get_username();
    if (tpg_ctx_exists(username)) {
        // TPG context already exists for this username
        return;
    }

    // Create a new context
    TPGRxCtx* tpg_rx_ctx = new TPGRxCtx(tpg_cp_ctx->get_rx_ports(),
                                        tpg_cp_ctx->get_num_tpgids(),
                                        username,
                                        tpg_cp_ctx->get_tag_mgr());

    m_tpg_ctx[username] = tpg_rx_ctx;          // Add entry to map

    // Start allocating
    m_working_tpg_threads++;        // Increase the ref counter
    tpg_rx_ctx->allocate();         // Starts allocating in another thread
}

void CRxCore::disable_tpg_ctx(const std::string& username) {
    /**
     * NOTE: All the statistics are de-allocated here. We do this is another thread so that
     * this function will be non blocking.
     **/
    if (!tpg_ctx_exists(username)) {
        // TPG context doesn't exist for this user.
        return;
    }
    TPGRxCtx* tpg_rx_ctx = m_tpg_ctx[username];

    // Stop the port handlers before freeing the counters.
    const std::vector<uint8_t>& rx_ports_vec = tpg_rx_ctx->get_rx_ports();
    for (uint8_t port: rx_ports_vec) {
        m_rx_port_mngr_vec[port]->disable_tpg();
    }

    m_working_tpg_threads++;             // Increase the ref counter
    tpg_rx_ctx->deallocate();            // Start deallocating in another thread
}

bool CRxCore::update_tpg_ctx_tags(const std::string& username, PacketGroupTagMgr* tag_mgr) {
    if (!tpg_ctx_exists(username)) {
        // TPG context doesn't exist for this user.
        return false;
    }

    TPGRxCtx* tpg_rx_ctx = m_tpg_ctx[username];
    assert(tpg_rx_ctx);

    // Update in Context.
    PacketGroupTagMgr* new_tag_mgr = tpg_rx_ctx->update_tag_mgr(tag_mgr);

    // Distribute to ports.
    const std::vector<uint8_t>& rx_ports_vec = tpg_rx_ctx->get_rx_ports();
    for (uint8_t port: rx_ports_vec) {
        RxTPGPerPort* rx_tpg = m_rx_port_mngr_vec[port]->get_rx_tpg();
        assert(rx_tpg);
        rx_tpg->set_tag_mgr(new_tag_mgr); // Set the pointer to the new allocated tag manager in Rx.
    }
    return true;
}

bool CRxCore::handle_tpg_threads() {
    /**
     * NOTE: Called by the scheduler periodically if there is an allocating/deallocating thread working.
     **/

    bool work_done = false;
    std::vector<std::string> to_erase;
    for (auto& tpg_ctx : m_tpg_ctx) {
            TPGRxState tpg_rx_state = tpg_ctx.second->get_state();
            if (unlikely(tpg_rx_state == TPGRxState::AWAITING_ENABLE)) {
                // Post allocation - Awaiting context enablement.
                _enable_tpg_ctx(tpg_ctx.second);
                work_done = true;
            }
            if (unlikely(tpg_rx_state == TPGRxState::AWAITING_DISABLE)) {
                // Post deallocation - Awaiting context disablement
                _disable_tpg_ctx(tpg_ctx.second);
                to_erase.push_back(tpg_ctx.first);
                work_done = true;
            }
    }
    for (std::string& username : to_erase) {
        m_tpg_ctx.erase(username);
    }
    return work_done;
}

void CRxCore::_enable_tpg_ctx(TPGRxCtx* tpg_rx_ctx) {
    m_working_tpg_threads--;
    tpg_rx_ctx->destroy_thread(); // Thread has finished working

    if (unlikely(tpg_rx_ctx->get_cntrs() == nullptr)) {
        tpg_rx_ctx->set_state(TPGRxState::ALLOC_FAIL);
    } else {
        const std::vector<uint8_t>& rx_ports_vec = tpg_rx_ctx->get_rx_ports();
        uint32_t num_tpgids = tpg_rx_ctx->get_num_tpgids();

        for (uint8_t port: rx_ports_vec) {
            CTPGTagCntr* port_cntr = tpg_rx_ctx->get_port_cntr(port);
            m_rx_port_mngr_vec[port]->enable_tpg(num_tpgids, tpg_rx_ctx->get_tag_mgr(), port_cntr);
        }

        tpg_rx_ctx->set_state(TPGRxState::ENABLED);
    }

    recalculate_next_state();
}

void CRxCore::_disable_tpg_ctx(TPGRxCtx* tpg_rx_ctx) {
    m_working_tpg_threads--;
    tpg_rx_ctx->destroy_thread(); // Thread has finished working

    delete tpg_rx_ctx;
    /**
     Theoretically we should remove the username from the map here.
     However we are looping the map right now, hence the caller removes it
     **/
    recalculate_next_state();
}

void CRxCore::get_tpg_stats(Json::Value& stats, uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag, bool untagged) {
    Json::Value& port_stats = stats[std::to_string(port_id)];
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->get_tpg_stats(port_stats, tpgid, min_tag, max_tag, unknown_tag, untagged);
}

void CRxCore::clear_tpg_stats(uint8_t port_id, uint32_t min_tpgid, uint32_t max_tpgid, const std::vector<uint16_t>& tag_list) {
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->clear_tpg_stats(min_tpgid, max_tpgid, tag_list);
}

void CRxCore::clear_tpg_stats(uint8_t port_id, uint32_t tpgid, const std::vector<uint16_t>& tag_list, bool unknown_tag, bool untagged) {
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->clear_tpg_stats(tpgid, tag_list, unknown_tag, untagged);
}

void CRxCore::clear_tpg_stats(uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag, bool untagged) {
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->clear_tpg_stats(tpgid, min_tag, max_tag, unknown_tag, untagged);
}

void CRxCore::get_tpg_unknown_tags(Json::Value& tags, uint8_t port_id) {
    tags[std::to_string(port_id)] = Json::arrayValue;
    Json::Value& port_tags = tags[std::to_string(port_id)];
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->get_tpg_unknown_tags(port_tags);
}


void CRxCore::clear_tpg_unknown_tags(uint8_t port_id) {
    RxTPGPerPort* rx_tpg = m_rx_port_mngr_map[port_id]->get_rx_tpg(); // This is not nullptr, validated that we are collecting on this port.
    rx_tpg->clear_tpg_unknown_tags();
}
