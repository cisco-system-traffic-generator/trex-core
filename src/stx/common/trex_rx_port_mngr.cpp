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
#include "trex_rx_port_mngr.h"
#include "trex_rx_core.h"
#include "pkt_gen.h"
#include "trex_capture.h"

#include <zmq.h>


/**************************************
 * latency RX feature
 * 
 *************************************/
RXLatency::RXLatency() {
    m_rcv_all    = false;
    m_rfc2544    = NULL;
    m_err_cntrs  = NULL;

    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        m_rx_pg_stat[i].clear();
    }
    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rx_pg_stat_payload[i].clear();
    }
}

void
RXLatency::create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs) {
    m_rfc2544   = rfc2544;
    m_err_cntrs = err_cntrs;
    if ((CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_ONE_QUEUE)
        || (CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_RSS)) {
        m_rcv_all    = true;
    } else {
        m_rcv_all    = false;
    }

    uint16_t num_counters, cap, ip_id_base;

    const TrexPlatformApi &api = get_platform_api();
    api.get_port_stat_info(0, num_counters, cap, ip_id_base);
    
    m_ip_id_base = ip_id_base;
}

void 
RXLatency::handle_pkt(const rte_mbuf_t *m) {
    uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
    CFlowStatParser parser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
    int ret = parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len);

    if (m_rcv_all ||  (ret == 0)) {
        uint32_t ip_id = 0;
        int ret2 = parser.get_ip_id(ip_id);
        if (m_rcv_all || ( ret2 == 0)) {
            if (m_rcv_all || is_flow_stat_id(ip_id)) {
                uint16_t hw_id;
                if (is_flow_stat_payload_id(ip_id) || ((! is_flow_stat_id(ip_id)) && m_rcv_all)) {
                    bool good_packet = true;
                    struct flow_stat_payload_header *fsp_head = (flow_stat_payload_header *)
                        utl_rte_pktmbuf_get_last_bytes(m, sizeof(struct flow_stat_payload_header), tmp_buf);
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
#ifdef LAT_DEBUG
                                    printf("magic:%x flow_seq:%x hw_id:%x seq %x exp %x\n"
                                           , fsp_head->magic, fsp_head->flow_seq, fsp_head->hw_id, pkt_seq
                                           , exp_seq);
                                    utl_DumpBuffer(stdout, rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len, 0);
#endif
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
#ifdef LAT_DEBUG
                                    printf("2:seq %d exp %d\n", exp_seq, pkt_seq);
#endif
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
                    hw_id = get_hw_id((uint16_t)ip_id);
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


void
RXLatency::get_stats(rx_per_flow_t *rx_stats,
                     int min,
                     int max,
                     bool reset,
                     TrexPlatformApi::driver_stat_cap_e type) {
    
    for (int hw_id = min; hw_id <= max; hw_id++) {
        if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
            rx_stats[hw_id - min] = m_rx_pg_stat_payload[hw_id];
        } else {
            rx_stats[hw_id - min] = m_rx_pg_stat[hw_id];
        }
        if (reset) {
            if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                m_rx_pg_stat_payload[hw_id].clear();
            } else {
                m_rx_pg_stat[hw_id].clear();
            }
        }
    }
}


Json::Value
RXLatency::to_json() const {
    return Json::objectValue;
}

/**************************************
 * RX feature queue 
 * 
 *************************************/

void
RXQueue::start(uint64_t size) {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
    }
    m_pkt_buffer = new TrexPktBuffer(size, TrexPktBuffer::MODE_DROP_HEAD);
}

void
RXQueue::stop() {
    if (m_pkt_buffer) {
        delete m_pkt_buffer;
        m_pkt_buffer = NULL;
    }
}

const TrexPktBuffer *
RXQueue::fetch() {

    /* if no buffer or the buffer is empty - give a NULL one */
    if ( (!m_pkt_buffer) || (m_pkt_buffer->get_element_count() == 0) ) {
        return nullptr;
    }
    
    /* hold a pointer to the old one */
    TrexPktBuffer *old_buffer = m_pkt_buffer;

    /* replace the old one with a new one and freeze the old */
    m_pkt_buffer = new TrexPktBuffer(old_buffer->get_capacity(), old_buffer->get_mode());

    return old_buffer;
}

void
RXQueue::handle_pkt(const rte_mbuf_t *m) {
    m_pkt_buffer->push(m);
}

Json::Value
RXQueue::to_json() const {
    assert(m_pkt_buffer != NULL);
    
    Json::Value output = Json::objectValue;
    
    output["size"]    = Json::UInt64(m_pkt_buffer->get_capacity());
    output["count"]   = Json::UInt64(m_pkt_buffer->get_element_count());
    
    return output;
}


/**************************************
 * Capture Port
 *
 *************************************/
void
RXCapturePort::create(RXFeatureAPI *api) {
    m_api = api;
}

Json::Value
RXCapturePort::to_json() const {
    Json::Value output = Json::objectValue;
    output["bpf_filter"] = m_bpf_filter ? m_bpf_filter->get_filter() : "";
    output["endpoint"] = m_endpoint;
    output["socket_open"] = m_zeromq_socket != nullptr;
    return output;
}


void
RXCapturePort::handle_pkt(const rte_mbuf_t *m) {
    if(likely(m_bpf_filter != nullptr)) {
        if (!m_bpf_filter->match(m)) {
            return;
        }
    }

    uint8_t* ptr = rte_pktmbuf_mtod(m, uint8_t *);
    uint32_t len = rte_pktmbuf_pkt_len(m);
    zmq_send(m_zeromq_socket, ptr, len, ZMQ_DONTWAIT);
}

void
RXCapturePort::set_bpf_filter(const std::string& filter) {
   delete m_bpf_filter;
   m_bpf_filter = nullptr;
   if (filter.size() > 0) {
       m_bpf_filter = new BPFFilter();
       m_bpf_filter->set_filter(filter);
       m_bpf_filter->compile();
   }
}

bool RXCapturePort::start(std::string &err) {
    m_zeromq_ctx = zmq_ctx_new();
    if ( !m_zeromq_ctx ) {
        err = "Could not create ZMQ context";
        return false;
    }
    m_zeromq_socket = zmq_socket(m_zeromq_ctx, ZMQ_PAIR);
    if ( !m_zeromq_socket ) {
        zmq_ctx_term(m_zeromq_ctx);
        m_zeromq_ctx = nullptr;
        err = "Could not create ZMQ socket";
        return false;
    }
    int linger = 0;
    zmq_setsockopt(m_zeromq_socket, ZMQ_LINGER, &linger, sizeof(linger));
    if ( zmq_connect(m_zeromq_socket, m_endpoint.c_str()) != 0 ) {
        zmq_close(m_zeromq_socket);
        zmq_ctx_term(m_zeromq_ctx);
        m_zeromq_socket = nullptr;
        m_zeromq_ctx = nullptr;
        err = "Could not connect to ZMQ socket";
        return false;
    }
    return true;
}

void RXCapturePort::stop() {
    if ( m_zeromq_socket ) {
        zmq_close(m_zeromq_socket);
        m_zeromq_socket = nullptr;
    }
    if ( m_zeromq_ctx ) {
        zmq_ctx_term(m_zeromq_ctx);
        m_zeromq_ctx = nullptr;
    }
}

uint32_t
RXCapturePort::do_tx() {
    uint32_t cnt = 0;
    uint8_t port_id = m_api->get_port_id();
    int len = 0;
    while(cnt < 32 && (len = zmq_recv(m_zeromq_socket, m_buffer_zmq,
                               sizeof(m_buffer_zmq), ZMQ_NOBLOCK)) > 0) {
        /* Allocate a mbuf */
        rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(CGlobalInfo::m_socket.port_to_socket(port_id), len);
        assert(m);

        /* allocate */
        uint8_t *p = (uint8_t *)rte_pktmbuf_append(m, len);
        assert(p);

        memcpy(p, m_buffer_zmq, len);
        if (!m_api->tx_pkt(m)) {
            rte_pktmbuf_free(m);
            break;
        }

        cnt++;
    }

    return cnt;
}

RXCapturePort::~RXCapturePort() {
    delete m_bpf_filter;
    stop();
    m_bpf_filter = nullptr;
}


/**************************************
 * Port manager 
 * 
 *************************************/

RXPortManager::RXPortManager() : m_feature_api(this) {
    clear_all_features();
    m_stack          = nullptr;
    m_io             = nullptr;
    m_port_id        = UINT8_MAX;
}

// workaround for inserting this object to map (GCC 4.7 does copy)
RXPortManager::RXPortManager(const RXPortManager &other) : m_feature_api(this) {
    clear_all_features();
    m_stack          = nullptr;
    m_io             = nullptr;
    m_port_id        = UINT8_MAX;
}

RXPortManager::~RXPortManager(void) {
    if ( m_stack != nullptr) {
        delete m_stack;
        m_stack = nullptr;
    }
}

struct CPlatformYamlInfo;

void
RXPortManager::create_async(uint32_t port_id,
                      CPortLatencyHWBase *io,
                      CRFC2544Info *rfc2544,
                      CRxCoreErrCntrs *err_cntrs,
                      CCpuUtlDp *cpu_util) {
    
    m_port_id = port_id;
    m_io = io;

    /* create a predicator for CPU util. */
    m_cpu_pred.create(cpu_util);
    
    /* init features */
    m_latency.create(rfc2544, err_cntrs);

    m_capture_port.create(&m_feature_api);

    /* by default, stack is always on */
    std::string &stack_type = CGlobalInfo::m_options.m_stack_type;
    m_stack = CStackFactory::create(stack_type, &m_feature_api, &m_ign_stats);
    m_stack->add_port_node_async();
    m_stack->run_pending_tasks_async(0);
    set_feature(STACK);
}

std::string wait_stack_tasks(CStackBase *stack, double timeout_sec) {
    stack_result_t results;
    try {
        stack->wait_on_tasks(0, results, timeout_sec);
    } catch (const TrexException &ex) {
        return ex.what();
    }
    if ( results.err_per_mac.size() ) {
        assert(results.err_per_mac.size()==1);
        return results.err_per_mac.begin()->second;
    }
    return "";
}

void RXPortManager::wait_for_create_done(void) {
    std::string err = wait_stack_tasks(get_stack(), 5);
    if ( err.size() ) {
        throw TrexException("Error: cound not create stack with following reason:\n" + err);
    }
}

void RXPortManager::cleanup_async(void) {
    m_stack->cleanup_async();
    m_stack->run_pending_tasks_async(0);
}

void RXPortManager::wait_for_cleanup_done(void) {
    wait_stack_tasks(get_stack(), -1);
}

bool RXPortManager::start_capture_port(const std::string& filter,
                                       const std::string& endpoint,
                                       std::string &err) {
    if ( is_feature_set(CAPTURE_PORT) ) {
        err = "Capture port is already active";
        return false;
    }

    /* Set the BPF filter */
    m_capture_port.set_bpf_filter(filter);

    /* Set the endpoint */
    m_capture_port.set_endpoint(endpoint);

    /* Start the zeromq socket */
    if ( !m_capture_port.start(err) ) {
        return false;
    }

    set_feature(CAPTURE_PORT);
    return true;
}

bool RXPortManager::stop_capture_port(std::string &err) {

    if ( !is_feature_set(CAPTURE_PORT) ) {
        return true;
        /* allow stopping stopped
        err = "Capture port is not active";
        return false;
        */
    }

    m_capture_port.stop();
    unset_feature(CAPTURE_PORT);
    return true;
}

void RXPortManager::handle_pkt(const rte_mbuf_t *m) {

    /* handle features */

    /* capture should be first */
    TrexCaptureMngr::getInstance().handle_pkt_rx(m, m_port_id);

    if (is_feature_set(LATENCY)) {
        m_latency.handle_pkt(m);
    }

    if (is_feature_set(QUEUE)) {
        m_queue.handle_pkt(m);
    }
    
    if ( is_feature_set(STACK) && !m_stack->is_running_tasks() ) {
        m_stack->handle_pkt(m);
    }

    if (is_feature_set(CAPTURE_PORT)) {
        m_capture_port.handle_pkt(m);
    }
}

uint16_t RXPortManager::handle_tx(void) {
    uint16_t cnt_pkts = 0;
    uint16_t limit = 32;

    if ( is_feature_set(STACK) && !m_stack->is_running_tasks() ) {
        cnt_pkts += m_stack->handle_tx(limit);
    }

    return cnt_pkts;
}

int RXPortManager::process_all_pending_pkts(bool flush_rx) {

    rte_mbuf_t *rx_pkts[64];
    uint16_t cnt_tx = 0;
    
    /* start CPU util. with heuristics
       this may or may not start the CPU util.
       measurement
     */
    m_cpu_pred.start_heur();
    cnt_tx = handle_tx();

    if ( cnt_tx ) {
        m_cpu_pred.update(true);
    }

    /* Do TX on capture port if needed */
    if (is_feature_set(CAPTURE_PORT)) {
        cnt_tx = m_capture_port.do_tx();
    }

    /* try to read 64 packets clean up the queue */
    uint16_t cnt_rx = m_io->rx_burst(rx_pkts, 64);

    if (cnt_tx + cnt_rx == 0) {
        /* update the predicator that no packets have arrived
           will stop the predictor if it was started
         */
        m_cpu_pred.update(false);
        return 0;
    }

    /* for next time mark the predictor as true
       will start the predictor in case it did not
     */
    m_cpu_pred.update(true);

    for (int j = 0; j < cnt_rx; j++) {
        rte_mbuf_t *m = rx_pkts[j];

        if (!flush_rx) {
            handle_pkt(m);
        }

        rte_pktmbuf_free(m);
    }

    /* done */
    m_cpu_pred.commit();
    

    return cnt_tx + cnt_rx;
}


bool
RXPortManager::tx_pkt(const std::string &pkt) {
    /* allocate MBUF */
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(CGlobalInfo::m_socket.port_to_socket(m_port_id), pkt.size());
    assert(m);
    
    /* allocate */
    uint8_t *p = (uint8_t *)rte_pktmbuf_append(m, pkt.size());
    assert(p);
    
    /* copy */
    memcpy(p, pkt.c_str(), pkt.size());
    
    /* send */
    bool rc = tx_pkt(m);
    if (!rc) {
        rte_pktmbuf_free(m);
    }
    
    return rc;
}


bool
RXPortManager::tx_pkt(rte_mbuf_t *m) {
    TrexCaptureMngr::getInstance().handle_pkt_tx(m, m_port_id);
    return (m_io->tx_raw(m) == 0);
}


void RXPortManager::to_json(Json::Value &feat_res) const {
    if (is_feature_set(LATENCY)) {
        feat_res["latency"] = m_latency.to_json();
        feat_res["latency"]["is_active"] = true;
    } else {
        feat_res["latency"]["is_active"] = false;
    }

    if (is_feature_set(QUEUE)) {
        feat_res["queue"] = m_queue.to_json();
        feat_res["queue"]["is_active"] = true;
    } else {
        feat_res["queue"]["is_active"] = false;
    }

    if (is_feature_set(STACK)) {
        feat_res["stack"]["is_active"] = true;
        m_stack->grat_to_json(feat_res["grat_arp"]);
    } else {
        feat_res["stack"]["is_active"] = false;
        feat_res["grat_arp"]["is_active"] = false;
    }

    if (is_feature_set(CAPTURE_PORT)) {
        feat_res["capture_port"] = m_capture_port.to_json();
        feat_res["capture_port"]["is_active"] = true;
    } else {
        feat_res["capture_port"]["is_active"] = false;
    }

}


void RXPortManager::get_ignore_stats(CRXCoreIgnoreStat &stat, bool get_diff) {
    if (get_diff) {
        stat = m_ign_stats - m_ign_stats_prev;
        m_ign_stats_prev = m_ign_stats;
    } else {
        stat = m_ign_stats;
    }
}


/**************************************
 * RX feature API
 * exposes a subset of commands 
 * from the port manager object 
 *************************************/

bool
RXFeatureAPI::tx_pkt(const std::string &pkt) {
    return m_port_mngr->tx_pkt(pkt);
}

bool
RXFeatureAPI::tx_pkt(rte_mbuf_t *m) {
    return m_port_mngr->tx_pkt(m);
}

uint8_t
RXFeatureAPI::get_port_id() {
    return m_port_mngr->m_port_id;
}
