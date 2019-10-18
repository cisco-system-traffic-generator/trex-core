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
    os << "m_latency" << in.m_latency << std::endl;
    os << "m_seq_err_events_too_big = " << in.m_seq_err_events_too_big << std::endl;
    os << "m_seq_err_events_too_low = " << in.m_seq_err_events_too_low << std::endl;
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

uint64_t CRxCoreErrCntrs::get_bad_header() {
    return m_bad_header;
}

uint64_t CRxCoreErrCntrs::get_old_flow() {
    return m_old_flow;
}

void CRxCoreErrCntrs::reset() {
        m_bad_header = 0;
        m_old_flow = 0;
}

CRxCoreErrCntrs CRxCoreErrCntrs::operator+= (const CRxCoreErrCntrs& in) {
    this->m_bad_header += in.m_bad_header;
    this->m_old_flow += in.m_old_flow;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const CRxCoreErrCntrs& in) {
    os << "m_bad_header = " << in.m_bad_header << std::endl;
    os << "m_old_flow = " << in.m_old_flow << std::endl;
    return os;
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

    uint16_t num_counters, cap, ip_id_base;

    const TrexPlatformApi &api = get_platform_api();
    api.get_port_stat_info(0, num_counters, cap, ip_id_base);

    m_ip_id_base = ip_id_base;
}

void RXLatency::handle_pkt(const rte_mbuf_t *m, int port) {
  uint8_t tmp_buf[sizeof(struct flow_stat_payload_header)];
  CFlowStatParser parser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
  parser.set_vxlan_skip(CGlobalInfo::m_options.m_ip_cfg[port].get_vxlan_fs());
  int ret = parser.parse(rte_pktmbuf_mtod(m, uint8_t *), m->pkt_len);

  if (m_rcv_all || (ret == 0)) {
    uint32_t ip_id = 0;
    int ret2 = parser.get_ip_id(ip_id);
    if (m_rcv_all || (ret2 == 0)) {
      if (m_rcv_all || is_flow_stat_id(ip_id)) {
        if (is_flow_stat_payload_id(ip_id) ||
            ((!is_flow_stat_id(ip_id)) && m_rcv_all)) {
          struct flow_stat_payload_header *fsp_head =
              (flow_stat_payload_header *)utl_rte_pktmbuf_get_last_bytes(
                  m, sizeof(struct flow_stat_payload_header), tmp_buf);
          hr_time_t hr_time_now = os_get_hr_tick_64();
          update_stats_for_pkt(fsp_head, m->pkt_len, hr_time_now);
        } else {
          uint16_t hw_id = get_hw_id((uint16_t)ip_id);
          if (hw_id < MAX_FLOW_STATS) {
            m_rx_pg_stat[hw_id].add_pkts(1);
            m_rx_pg_stat[hw_id].add_bytes(m->pkt_len +
                                          4); // +4 for ethernet CRC
          }
        }
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

    // [nanos] Use nanoseconds instead of ticks
    //uint64_t d = (hr_time_now - fsp_head->time_stamp );
    //dsec_t ctime = ptime_convert_hr_dsec(d);
    uint64_t receiver_time_nanoseconds = get_time_epoch_nanoseconds();
    // received_pkt_with_future_timestamp == true means that time synchronization error is bigger than latency.
    bool received_pkt_with_future_timestamp = receiver_time_nanoseconds < fsp_head->time_stamp;
    uint64_t abs_diff = received_pkt_with_future_timestamp ?
        fsp_head->time_stamp - receiver_time_nanoseconds
        : receiver_time_nanoseconds - fsp_head->time_stamp;
    dsec_t ctime = (received_pkt_with_future_timestamp ? -1.d : 1.d) * abs_diff / (1000.d * 1000.d * 1000.d);
    curr_rfc2544->add_sample(ctime);
}

void
RXLatency::check_seq_number_and_update_stats(
        flow_stat_payload_header *fsp_head,
        CRFC2544Info *curr_rfc2544) {
    uint32_t pkt_seq = fsp_head->seq;
    uint32_t exp_seq = curr_rfc2544->get_seq();
    if (unlikely(pkt_seq != exp_seq)) {
        if (pkt_seq < exp_seq) {
            handle_seq_number_smaller_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        } else {
            handle_seq_number_bigger_than_expected(
                curr_rfc2544, pkt_seq, exp_seq);
        }
    } else {
        curr_rfc2544->set_seq(pkt_seq + 1);
    }
}

void
RXLatency::handle_seq_number_smaller_than_expected(
        CRFC2544Info *curr_rfc2544,
        uint32_t &pkt_seq,
        uint32_t &exp_seq) {
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
}

void
RXLatency::handle_seq_number_bigger_than_expected(
        CRFC2544Info *curr_rfc2544,
        uint32_t &pkt_seq,
        uint32_t &exp_seq) {
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
