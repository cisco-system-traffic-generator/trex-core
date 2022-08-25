#include "trex_latency_counters.h"
#include "bp_sim.h"

/*******************************************************************
CRFC2544Info
*******************************************************************/
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
    m_seqblks.clear();
}

bool CRFC2544Info::SeqBlk::set_map(uint32_t pkt_seq) {
    assert(has_map() && includes(pkt_seq));
    int seq_idx = pkt_seq - m_begin;

    if (!m_seq_map[seq_idx]) {
        m_seq_map[seq_idx] = true;
        m_seq_cnt++;
        return true;
    }

    return false;
}

bool CRFC2544Info::SeqBlk::set_seq(uint32_t pkt_seq) {
    assert(includes(pkt_seq));

    if (likely(!has_map())) {
        if (likely(is_border(pkt_seq))) {
            if (pkt_seq == m_begin) {
                m_begin++;
            } else if (pkt_seq == m_end) {
                m_end--;
            }
            return true;
        }
        m_seq_map.resize(size());
        m_seq_cnt = 0;
    }
    return set_map(pkt_seq);
}

void CRFC2544Info::add_seqblk(uint32_t begin, uint32_t end) {
    int size = end - begin + 1;
    assert(size > 0);

    if (unlikely(m_seqblks.empty())) {
        m_seqblk_base = begin;
    } else if (size > CHECK_BLK_SIZE && (int)(begin - m_seqblk_base) <= 0) {
        // remove overlapped old seqblk when new seqblk size is large.
        bool is_overlap = (int)(m_seqblk_base - end) <= 0;
        prune_old_seqblks(is_overlap ? m_seqblk_base - 1: end);
    }

    if (end < begin) {
        m_seqblks.emplace(0, SeqBlk(0, end));
        end = UINT32_MAX;
    }
    m_seqblks.emplace(begin, SeqBlk(begin, end));
}

#define SEQ_MAP_SIZE    0x400

bool CRFC2544Info::set_seqblk_seq(uint32_t pkt_seq) {
    auto it = m_seqblks.lower_bound(pkt_seq);
    if (it != m_seqblks.end() && it->second.includes(pkt_seq)) {
        auto& seqblk = it->second;

        if (seqblk.size() > SEQ_MAP_SIZE && seqblk.is_split(pkt_seq)) {
            uint32_t seq_end = seqblk.get_end();
            seqblk.set_end(pkt_seq - 1);
            add_seqblk(pkt_seq + 1, seq_end);
            return true;
        }

        if (!seqblk.set_seq(pkt_seq)) {
            return false;
        }

        if (seqblk.empty()) {
            m_seqblks.erase(it);
        }
        return true;
    }
    return false;
}

void CRFC2544Info::prune_old_seqblks(uint32_t cur_seq) {
    if (m_seqblks.empty() || (int)(m_seqblk_base - cur_seq) < 0) {
        return;
    }

    auto it = m_seqblks.find(m_seqblk_base);
    if (it == m_seqblks.end()) {
        it = m_seqblks.upper_bound(m_seqblk_base);
    }

    do {
        if (it == m_seqblks.end()) {
            it = m_seqblks.begin();
        }
        m_seqblk_base = it->first;

        uint32_t seq_end = it->second.get_end();
        if ((int)(seq_end - cur_seq) < 0) {
            // large size seqblk can be left easily and overlapped by new seqblk.
            // So, the seqblk need to remove old sequences to prevent the overlap.
            auto& seqblk = it->second;
            if (seqblk.size() > CHECK_BLK_SIZE) {
                uint32_t seq_begin = seq_end - seqblk.size() + 1; // it->first != seq_begin
                if ((int)(seq_begin - cur_seq) > 0) {
                    m_seqblks.erase(it);
                    m_seqblk_base = cur_seq + INT32_MAX + 1;
                    assert((int)(seq_end - m_seqblk_base) >= 0);
                    add_seqblk(m_seqblk_base, seq_end);
                }
            }
            break;
        }
        m_seqblks.erase(it++);
    } while (!m_seqblks.empty());
}

void CRFC2544Info::check_seqblks() {
    if (unlikely(!m_seqblks.empty() && (int)(m_seqblk_base - m_seq) > CHECK_BLK_SIZE)) {
        prune_old_seqblks(m_seq);
    }
}

void CRFC2544Info::export_data(rfc2544_info_t_ &obj) {
    std::string json_str;
    Json::Reader reader;
    Json::Value json;

    obj.set_err_cntrs(m_seq_err, m_ooo, m_dup, m_seq_err_events_too_big, m_seq_err_events_too_low);
    obj.set_jitter(m_jitter.get_jitter());
    m_latency.dump_json(json);
    obj.set_latency_json(json);
}

CRFC2544Info CRFC2544Info::operator+= (const CRFC2544Info& in) {
    this->m_seq += in.m_seq;
    this->m_latency += in.m_latency;
    this->m_jitter = std::max(this->m_jitter, in.m_jitter);
    this->m_seq_err += in.m_seq_err;
    this->m_seq_err_events_too_big += in.m_seq_err_events_too_big;
    this->m_seq_err_events_too_low += in.m_seq_err_events_too_low;
    this->m_ooo += in.m_ooo;
    this->m_dup += in.m_dup;
    this->m_exp_flow_seq += in.m_exp_flow_seq;
    this->m_prev_flow_seq += in.m_prev_flow_seq;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const CRFC2544Info& in) {
    os << "m_seq = " << in.m_seq << std::endl;
    os << "m_jitter = " << in.m_jitter << std::endl;
    os << "m_latency" << std::endl << in.m_latency << std::endl;
    os << "m_seq_err_events_too_big = " << in.m_seq_err_events_too_big << std::endl;
    os << "m_seq_err_events_too_low = " << in.m_seq_err_events_too_low << std::endl;
    os << "m_seq_err = " << in.m_seq_err << std::endl;
    os << "m_ooo = " << in.m_ooo << std::endl;
    os << "m_dup = " << in.m_dup << std::endl;
    os << "m_exp_flow_seq = " << in.m_exp_flow_seq << std::endl;
    os << "m_prev_flow_seq = " << in.m_prev_flow_seq << std::endl;
    return os;
}

/*******************************************************************
CRxCoreErrCntrs
*******************************************************************/
CRxCoreErrCntrs::CRxCoreErrCntrs() {
    reset();
}

#define ADD_CNT(f)  { if (m_##f) {(*data_section)[#f] = Json::Value::UInt64(m_##f);  } } 
#define PLUS_CNT(f)  { this->m_##f += in.m_##f;}

void CRxCoreErrCntrs::ToJson(Json::Value *data_section){
    ADD_CNT(bad_header);
    ADD_CNT(old_flow);
    ADD_CNT(ezmq_tx_to_emu);
    ADD_CNT(ezmq_tx_to_emu_err);
    ADD_CNT(ezmq_tx_to_emu_restart);
    ADD_CNT(ezmq_tx_fe_dropped_no_mbuf);
    ADD_CNT(ezmq_tx_fe_wrong_vport);
    ADD_CNT(ezmq_tx_fe_err_send);
    ADD_CNT(ezmq_tx_fe_ok_send);
    ADD_CNT(ezmq_rx_fe_err_buffer_len_high);
    ADD_CNT(ezmq_rx_fe_err_len_zero);
}


void CRxCoreErrCntrs::reset() {
    m_bad_header = 0;
    m_old_flow = 0;
    m_ezmq_tx_to_emu =0;
    m_ezmq_tx_to_emu_err =0;
    m_ezmq_tx_to_emu_restart =0;
    m_ezmq_tx_fe_dropped_no_mbuf =0;
    m_ezmq_tx_fe_wrong_vport =0;
    m_ezmq_tx_fe_err_send =0;
    m_ezmq_tx_fe_ok_send =0;
    m_ezmq_rx_fe_err_buffer_len_high =0;
    m_ezmq_rx_fe_err_len_zero =0;
}

CRxCoreErrCntrs CRxCoreErrCntrs::operator+= (const CRxCoreErrCntrs& in) {
    PLUS_CNT(bad_header);
    PLUS_CNT(old_flow);
    PLUS_CNT(ezmq_tx_to_emu);
    PLUS_CNT(ezmq_tx_to_emu_err);
    PLUS_CNT(ezmq_tx_to_emu_restart);
    PLUS_CNT(ezmq_tx_fe_dropped_no_mbuf);
    PLUS_CNT(ezmq_tx_fe_wrong_vport);
    PLUS_CNT(ezmq_tx_fe_err_send);
    PLUS_CNT(ezmq_tx_fe_ok_send);
    PLUS_CNT(ezmq_rx_fe_err_buffer_len_high);
    PLUS_CNT(ezmq_rx_fe_err_len_zero);
    return *this;
}


/**************************************
 * RXLatency
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

    m_dump_err_pkts = false;
    m_dump_pkt = nullptr;
    m_dump_fsp_head = nullptr;
    m_is_ieee_ref_cnt_set = false;
    m_diag_dup_pkts = false;
}

void
RXLatency::create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs) {
    m_rfc2544   = rfc2544;
    m_err_cntrs = err_cntrs;
    if ( !get_dpdk_mode()->is_hardware_filter_needed() ) {
        m_rcv_all    = true;
    } else {
        m_rcv_all    = false;
    }

    m_dump_err_pkts = isVerbose(6); // be enabled by -v 7
    m_diag_dup_pkts = CGlobalInfo::m_options.preview.get_latency_diag();

    uint16_t num_counters, cap, ip_id_base;

    const TrexPlatformApi &api = get_platform_api();
    api.get_port_stat_info(0, num_counters, cap, ip_id_base);
    
    m_ip_id_base = ip_id_base;
}

COLD_FUNC void RXLatency::set_dump_info(const rte_mbuf_t *m, struct flow_stat_payload_header *fsp_head) {
    m_dump_pkt = m;
    m_dump_fsp_head = fsp_head;
}

COLD_FUNC void RXLatency::dump_err_pkt(const char* info, bool dump_latency) {
    if (m_dump_fsp_head) {
        std::cout << info;
        if (dump_latency) {
            uint64_t d = os_get_hr_tick_64() - m_dump_fsp_head->time_stamp;
            std::cout << ", seq = " << m_dump_fsp_head->seq;
            std::cout << ", lat = " << ptime_convert_hr_dsec(d);
        }
        std::cout << ", hw_id = " << m_dump_fsp_head->hw_id;
        std::cout << "(" << unsigned(m_dump_fsp_head->flow_seq) << ")";
        std::cout << std::endl;
    }
    if (m_dump_pkt) {
#ifndef TREX_SIM
        rte_pktmbuf_dump(stdout, m_dump_pkt, rte_pktmbuf_pkt_len(m_dump_pkt));
#else
        rte_pktmbuf_dump(m_dump_pkt, rte_pktmbuf_pkt_len(m_dump_pkt));
#endif
    }
}

void
RXLatency::save_timestamps_for_sync_pkt(const rte_mbuf_t *m, flow_stat_payload_header_ieee_1588 *fsp_head, int port) {

    struct timespec timestamp = {0, 0};

    m_fup_seq_exp = fsp_head->fsp_hdr.flow_seq;

    if (rte_eth_timesync_read_rx_timestamp(port, &timestamp, m->timesync) < 0) {
        printf("Port %u RX timestamp registers not valid\n", port);
    }
#ifdef LAT_1588_DEBUG
    printf("Port %u RX timestamp value %lu s %lu ns\n",
            port, timestamp.tv_sec, timestamp.tv_nsec);
#endif
    m_sync_arrival_time_sec = timestamp.tv_sec;
    m_sync_arrival_time_nsec = timestamp.tv_nsec;
}

void
RXLatency::update_timestamps_for_fup_pkt(flow_stat_payload_header_ieee_1588 *fsp_head) {

    if(fsp_head->fsp_hdr.flow_seq == m_fup_seq_exp) {

        struct timespec ts_sync;

        ts_sync.tv_nsec = ntohl(fsp_head->ptp_message.origin_tstamp.ns);
        ts_sync.tv_sec =
            ((uint64_t)ntohl(fsp_head->ptp_message.origin_tstamp.sec_lsb)) |
            (((uint64_t)ntohs(fsp_head->ptp_message.origin_tstamp.sec_msb)) << 32);

        fsp_head->fsp_hdr.time_stamp =  ts_sync.tv_nsec;
#ifdef LAT_1588_DEBUG
         printf("timestamp in FUP %lu s %lu ns\n",
                             ts_sync.tv_sec, ts_sync.tv_nsec);
#endif
    } else {
        printf(" Wrong flow seq in FUP \n");
    }
}

bool flow_stat_payload_header::is_valid_ts(hr_time_t now){
    int64_t d = (now - time_stamp);
    uint64_t d1  = std::abs(d) >>2;
    return (d1 < os_get_hr_freq()?true:false);
}

bool RXLatency::handle_flow_latency_stats_ieee_1588(const rte_mbuf_t *m, uint32_t& ip_id, int port) {
    uint8_t tmp_buf_ieee_1588[sizeof(struct flow_stat_payload_header_ieee_1588)];
    struct flow_stat_payload_header_ieee_1588 *fsp_head_ieee_1588 = 0;

    if (m->ol_flags & RTE_MBUF_F_RX_IEEE1588_TMST) {  /* IEEE-1588 - SYNC Pkt */
        m_is_ieee_ref_cnt_set = true; /* Expect a FUP pkt. Set ref cnt */
        fsp_head_ieee_1588 = (flow_stat_payload_header_ieee_1588 *)utl_rte_pktmbuf_get_last_bytes(
            m, sizeof(struct flow_stat_payload_header_ieee_1588), tmp_buf_ieee_1588);

        save_timestamps_for_sync_pkt(m, fsp_head_ieee_1588, port);

    } else if (m_is_ieee_ref_cnt_set == true) { /* IEEE-1588 FUP Pkt */
        m_is_ieee_ref_cnt_set = false; /* Got FUP pkt. Clear the ref cnt */
        fsp_head_ieee_1588 = (flow_stat_payload_header_ieee_1588 *)utl_rte_pktmbuf_get_last_bytes(
            m, sizeof(struct flow_stat_payload_header_ieee_1588), tmp_buf_ieee_1588);

        update_timestamps_for_fup_pkt(fsp_head_ieee_1588);

    } else {
        printf(" ERR: Not expected to reach here\n");
        return false;
    }

    update_stats_for_pkt_ieee_1588 (fsp_head_ieee_1588, m->pkt_len, m_sync_arrival_time_nsec);
    return true;

}

bool RXLatency::handle_flow_latency_stats(const rte_mbuf_t *m, uint32_t& ip_id,bool check_non_ip) {
    uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
    struct flow_stat_payload_header *fsp_head = 0;
    bool readtime=false;
    hr_time_t hr_time_now =0;

    if (check_non_ip) {
        hr_time_now = os_get_hr_tick_64();
        readtime = true;
        fsp_head = (flow_stat_payload_header *)utl_rte_pktmbuf_get_last_bytes(
                m, sizeof(struct flow_stat_payload_header), tmp_buf);

        /* check if flow-latency-stat packet by payload magic.
         * for the non-IP protocol (invalid ip_id) and IP proxy (changed ip_id) cases.
         */
        if ( (fsp_head->magic == FLOW_STAT_PAYLOAD_MAGIC) &&
             (fsp_head->hw_id < MAX_FLOW_STATS_PAYLOAD)&& 
             fsp_head->is_valid_ts(hr_time_now) ) {
            ip_id = FLOW_STAT_PAYLOAD_IP_ID;
        }
    }

    if (is_flow_stat_payload_id(ip_id)) {
        if (!fsp_head){
            fsp_head = (flow_stat_payload_header *)utl_rte_pktmbuf_get_last_bytes(
                m, sizeof(struct flow_stat_payload_header), tmp_buf);
        }
        if (!readtime){
            hr_time_now = os_get_hr_tick_64();
            readtime = true;
        }
        update_stats_for_pkt(fsp_head, m->pkt_len, hr_time_now);
        return true;
    }
    return false;
}

void RXLatency::update_flow_stats(const rte_mbuf_t *m, uint32_t ip_id) {
    uint16_t hw_id = get_hw_id((uint16_t)ip_id);
    if (hw_id < MAX_FLOW_STATS) {
        m_rx_pg_stat[hw_id].add_pkts(1);
        m_rx_pg_stat[hw_id].add_bytes(m->pkt_len + 4); // +4 for ethernet CRC
    }
}

void RXLatency::handle_pkt(const rte_mbuf_t *m, int port) {
    CFlowStatParser parser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
    parser.set_vxlan_skip(CGlobalInfo::m_options.m_ip_cfg[port].get_vxlan_fs());
    int res = parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len);
    uint32_t ip_id = 0;
    parser.get_ip_id(ip_id);

    // in case of software mode, give priorty to meta-data checked 
    if (m_rcv_all) {
        if ( handle_flow_latency_stats(m, ip_id,true) ) {
            return;
        }
    }
    // valid IP 
    if (res == FSTAT_PARSER_E_OK){
        if (is_flow_stat_id(ip_id)) {
            if (is_flow_stat_payload_id(ip_id)) {
                if (unlikely( (m->ol_flags & RTE_MBUF_F_RX_IEEE1588_TMST) || (m_is_ieee_ref_cnt_set == true) )) {
                    handle_flow_latency_stats_ieee_1588(m, ip_id, port);
                } else {
                    handle_flow_latency_stats(m, ip_id,false);
                }
            } else {
                update_flow_stats(m, ip_id);
            }
        }
    }

}

void
RXLatency::update_stats_for_pkt(
        flow_stat_payload_header *fsp_head,
        uint32_t pkt_len,
        hr_time_t hr_time_now) {
    uint16_t hw_id = fsp_head->hw_id;
    if (unlikely(fsp_head->magic != FLOW_STAT_PAYLOAD_MAGIC)
            || hw_id >= MAX_FLOW_STATS_PAYLOAD) {
        if (!m_rcv_all) {
            m_err_cntrs->m_bad_header++;

            if (unlikely(m_dump_err_pkts)) {
                dump_err_pkt("bad_header: unexpected packet");
            }
        }
    } else {
        bool good_packet = true;
        CRFC2544Info *curr_rfc2544 = &m_rfc2544[hw_id];

        if (fsp_head->flow_seq != curr_rfc2544->get_exp_flow_seq()) {
            good_packet = handle_unexpected_flow(fsp_head, curr_rfc2544);
        }

        if (good_packet) {
            handle_correct_flow(fsp_head, curr_rfc2544, pkt_len, hr_time_now);
        }
    }
}

void
RXLatency::update_stats_for_pkt_ieee_1588(
        flow_stat_payload_header_ieee_1588 *fsp_head,
        uint32_t pkt_len,
        hr_time_t hr_time_now) {
    uint16_t hw_id = fsp_head->fsp_hdr.hw_id;
    if (unlikely(fsp_head->fsp_hdr.magic != FLOW_STAT_PAYLOAD_MAGIC)
            || hw_id >= MAX_FLOW_STATS_PAYLOAD) {
        if (!m_rcv_all) {
            m_err_cntrs->m_bad_header++;

            if (unlikely(m_dump_err_pkts)) {
                dump_err_pkt("bad_header: unexpected packet");
            }
        }
    } else {
        bool good_packet = true;
        CRFC2544Info *curr_rfc2544 = &m_rfc2544[hw_id];

        if (fsp_head->fsp_hdr.flow_seq != curr_rfc2544->get_exp_flow_seq()) {
            good_packet = handle_unexpected_flow_ieee_1588(fsp_head, curr_rfc2544);
        }

        if (good_packet) {
            handle_correct_flow_ieee_1588(fsp_head, curr_rfc2544, pkt_len, hr_time_now);
        }
    }
}

bool
RXLatency::handle_unexpected_flow(
        flow_stat_payload_header *fsp_head,
        CRFC2544Info *curr_rfc2544) {
    bool good_packet = true;
    // bad flow seq num
    // Might be the first packet of a new flow, packet from an old flow, or garbage.
    if (fsp_head->flow_seq == curr_rfc2544->get_prev_flow_seq()) {
        // packet from previous flow using this hw_id that arrived late
        good_packet = false;
        m_err_cntrs->m_old_flow++;
    } else {
        if (curr_rfc2544->no_flow_seq()) {
            // first packet we see from this flow
            curr_rfc2544->set_exp_flow_seq(fsp_head->flow_seq);
        } else {
            // garbage packet
            good_packet = false;
            m_err_cntrs->m_bad_header++;

            if (unlikely(m_dump_err_pkts)) {
                dump_err_pkt("bad_header: invalid hw_id");
            }
        }
    }

    return good_packet;
}

bool
RXLatency::handle_unexpected_flow_ieee_1588(
        flow_stat_payload_header_ieee_1588 *fsp_head,
        CRFC2544Info *curr_rfc2544) {
    bool good_packet = true;
    // bad flow seq num
    // Might be the first packet of a new flow, packet from an old flow, or garbage.
    if (fsp_head->fsp_hdr.flow_seq == curr_rfc2544->get_prev_flow_seq()) {
        // packet from previous flow using this hw_id that arrived late
        good_packet = false;
        m_err_cntrs->m_old_flow++;
    } else {
        if (curr_rfc2544->no_flow_seq()) {
            // first packet we see from this flow
            curr_rfc2544->set_exp_flow_seq(fsp_head->fsp_hdr.flow_seq);
        } else {
            // garbage packet
            good_packet = false;
            m_err_cntrs->m_bad_header++;

            if (unlikely(m_dump_err_pkts)) {
                dump_err_pkt("bad_header: invalid hw_id");
            }
        }
    }

    return good_packet;
}

void
RXLatency::handle_correct_flow(
        flow_stat_payload_header *fsp_head,
        CRFC2544Info *curr_rfc2544,
        uint32_t pkt_len,
        hr_time_t hr_time_now) {
    check_seq_number_and_update_stats(fsp_head, curr_rfc2544);
    uint16_t hw_id = fsp_head->hw_id;
    m_rx_pg_stat_payload[hw_id].add_pkts(1);
    m_rx_pg_stat_payload[hw_id].add_bytes(pkt_len + 4); // +4 for ethernet CRC
    uint64_t d = (hr_time_now - fsp_head->time_stamp );
    dsec_t ctime = ptime_convert_hr_dsec(d);
    curr_rfc2544->add_sample(ctime);
}

void
RXLatency::handle_correct_flow_ieee_1588(
        flow_stat_payload_header_ieee_1588 *fsp_head,
        CRFC2544Info *curr_rfc2544,
        uint32_t pkt_len,
        hr_time_t hr_time_now) {
    check_seq_number_and_update_stats_ieee_1588(fsp_head, curr_rfc2544);
    uint16_t hw_id = fsp_head->fsp_hdr.hw_id;
    m_rx_pg_stat_payload[hw_id].add_pkts(1);
    m_rx_pg_stat_payload[hw_id].add_bytes(pkt_len + 4); // +4 for ethernet CRC

    if (fsp_head->ptp_message.hdr.msg_type == FOLLOW_UP ) {
        uint64_t d = (hr_time_now - fsp_head->fsp_hdr.time_stamp );
        if (d > NSEC_PER_SEC) {
            /*
             * Expected at times of second transition.
             * Nothing to worry!. Just subtract the timestamp from NSEC_PER_SEC
             * and add the new time in nsec (hr_time_now) to get the proper delta.
             */
            struct timespec ts_sync;

            ts_sync.tv_sec =
                ((uint64_t)ntohl(fsp_head->ptp_message.origin_tstamp.sec_lsb)) |
                (((uint64_t)ntohs(fsp_head->ptp_message.origin_tstamp.sec_msb)) << 32);

            if ( likely (ts_sync.tv_sec != m_sync_arrival_time_sec)) {
                d =  hr_time_now + (NSEC_PER_SEC - fsp_head->fsp_hdr.time_stamp);
            } else {
                d = (fsp_head->fsp_hdr.time_stamp - hr_time_now);
            }
        }
        dsec_t ctime = ptime_convert_hr_dsec(d);
        curr_rfc2544->add_sample(ctime);
    }
}

void
RXLatency::check_seq_number_and_update_stats(
        flow_stat_payload_header *fsp_head,
        CRFC2544Info *curr_rfc2544) {
    uint32_t pkt_seq = fsp_head->seq;
    uint32_t exp_seq = curr_rfc2544->get_seq();
    if (unlikely(pkt_seq != exp_seq)) {
        if ((int32_t)(pkt_seq - exp_seq) < 0) {
            handle_seq_number_smaller_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        } else {
            handle_seq_number_bigger_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        }
    } else {
        curr_rfc2544->set_seq(pkt_seq + 1);
        if (unlikely(m_diag_dup_pkts)) {
            curr_rfc2544->check_seqblks();
        }
    }
}

void
RXLatency::check_seq_number_and_update_stats_ieee_1588(
        flow_stat_payload_header_ieee_1588 *fsp_head,
        CRFC2544Info *curr_rfc2544) {
    uint32_t pkt_seq = fsp_head->fsp_hdr.seq;
    uint32_t exp_seq = curr_rfc2544->get_seq();
    if (unlikely(pkt_seq != exp_seq)) {
        if ((int32_t)(pkt_seq - exp_seq) < 0) {
            handle_seq_number_smaller_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        } else {
            handle_seq_number_bigger_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        }
    } else {
        if(fsp_head->ptp_message.hdr.msg_type == FOLLOW_UP) {
            curr_rfc2544->set_seq(pkt_seq + 1);
            if (unlikely(m_diag_dup_pkts)) {
                curr_rfc2544->check_seqblks();
            }
        }
    }
}

void
RXLatency::handle_seq_number_smaller_than_expected(
        CRFC2544Info *curr_rfc2544,
        uint32_t &pkt_seq,
        uint32_t &exp_seq) {
    bool is_dup = pkt_seq == (exp_seq - 1);
    if (unlikely(!is_dup && m_diag_dup_pkts)) {
        is_dup = !curr_rfc2544->set_seqblk_seq(pkt_seq);
    }
    if (is_dup) {
        curr_rfc2544->inc_dup();

        if (unlikely(m_dump_err_pkts)) {
            dump_err_pkt("dup: unexpected seq", true);
        }
    } else {
        curr_rfc2544->inc_ooo();
        // We thought it was lost, but it was just out of order
        curr_rfc2544->dec_seq_err();
    }
    curr_rfc2544->inc_seq_err_too_low();
}

void
RXLatency::handle_seq_number_bigger_than_expected(
        CRFC2544Info *curr_rfc2544,
        uint32_t &pkt_seq,
        uint32_t &exp_seq) {
    // seq > curr_rfc2544->seq. Assuming lost packets
    curr_rfc2544->inc_seq_err(pkt_seq - exp_seq);
    curr_rfc2544->inc_seq_err_too_big();
    if (unlikely(m_diag_dup_pkts)) {
        curr_rfc2544->add_seqblk(exp_seq, pkt_seq - 1);
    }
    curr_rfc2544->set_seq(pkt_seq + 1);
}

void
RXLatency::reset_stats() {
    for (int hw_id = 0; hw_id < MAX_FLOW_STATS; hw_id++) {
        m_rx_pg_stat[hw_id].clear();
    }
}

void
RXLatency::reset_stats_partial(int min, int max, TrexPlatformApi::driver_stat_cap_e type) {
    if (type == TrexPlatformApi::IF_STAT_PAYLOAD) {
        for (int hw_id = min; hw_id <= max; hw_id++) {
            m_rx_pg_stat_payload[hw_id].clear();
        }
    } else {
        for (int hw_id = min; hw_id <= max; hw_id++) {
            m_rx_pg_stat[hw_id].clear();
        }
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

RXLatency
RXLatency::operator+= (const RXLatency& in) {
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        this->m_rx_pg_stat[i] += in.m_rx_pg_stat[i];
    }
    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        this->m_rx_pg_stat_payload[i] += in.m_rx_pg_stat_payload[i];
    }
    return *this;
}

std::ostream& operator<<(std::ostream& os, const RXLatency& in) {
    os << "m_rx_stats = <";
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        os << in.m_rx_pg_stat[i] << ", ";
    } 
    os << ">" << std::endl;
    os << "m_rx_pg_stat_payload = < ";
    for (int i = 0; i< MAX_FLOW_STATS_PAYLOAD; i++) {
        os << in.m_rx_pg_stat_payload[i] << ", ";
    }
    os << ">" << std::endl;
    return os;
}
