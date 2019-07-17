#ifndef __TREX_LATENCY_COUNTERS_H__
#define __TREX_LATENCY_COUNTERS_H__

#include "stl/trex_stl_fs.h"
#include "time_histogram.h"
#include "utl_jitter.h"

/**************************************
 * CRFC2544Info
 *************************************/
class CRFC2544Info {
 public:
    void create();
    void stop();
    void reset();
    void export_data(rfc2544_info_t_ &obj);
    inline void add_sample(double stime) {
        m_latency.Add(stime);
        m_jitter.calc(stime);
    }
    inline void sample_period_end() {
        m_latency.update();
    }
    inline uint32_t get_seq() {return m_seq;}
    inline void set_seq(uint32_t val) {m_seq = val;}
    inline void inc_seq_err(uint64_t val) {m_seq_err += val;}
    inline void dec_seq_err() {if (m_seq_err >0) {m_seq_err--;}}
    inline void inc_seq_err_too_big() {m_seq_err_events_too_big++;}
    inline void inc_seq_err_too_low() {m_seq_err_events_too_low++;}
    inline void inc_dup() {m_dup++;}
    inline void inc_ooo() {m_ooo++;}
    inline uint16_t get_exp_flow_seq() {return m_exp_flow_seq;}
    inline void set_exp_flow_seq(uint16_t flow_seq) {m_exp_flow_seq = flow_seq;}
    inline uint16_t get_prev_flow_seq() {return m_prev_flow_seq;}
    inline bool no_flow_seq() {return (m_exp_flow_seq == FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ) ? true : false;}
    CRFC2544Info operator+= (const CRFC2544Info& in);
    friend std::ostream& operator<<(std::ostream& os, const CRFC2544Info& in);

 private:
    uint32_t m_seq; // expected next seq num
    CTimeHistogram  m_latency; // latency info
    CJitter         m_jitter;
    uint64_t m_seq_err; // How many packet seq num gaps we saw (packets lost or out of order)
    uint64_t m_seq_err_events_too_big; // How many packet seq num greater than expected events we had
    uint64_t m_seq_err_events_too_low; // How many packet seq num lower than expected events we had
    uint64_t m_ooo; // Packets we got with seq num lower than expected (We guess they are out of order)
    uint64_t m_dup; // Packets we got with same seq num
    uint16_t m_exp_flow_seq; // flow sequence number we should see in latency header
    // flow sequence number previously used with this id. We use this to catch packets arriving late from an old flow
    uint16_t m_prev_flow_seq;
};

std::ostream& operator<<(std::ostream& os, const CRFC2544Info& in);

/**************************************
 * CRxCoreErrCntrs
 *************************************/
class CRxCoreErrCntrs {

 public:
    CRxCoreErrCntrs();
    uint64_t get_bad_header();
    uint64_t get_old_flow();
    void reset();
    CRxCoreErrCntrs operator+= (const CRxCoreErrCntrs& in);
    friend std::ostream& operator<<(std::ostream& os, const CRxCoreErrCntrs& in);

 public:
    uint64_t m_bad_header;
    uint64_t m_old_flow;
};

std::ostream& operator<<(std::ostream& os, const CRxCoreErrCntrs& in);

/**************************************
 * RXLatency
 *************************************/
class RXLatency {
public:

    RXLatency();

    void create(CRFC2544Info *rfc2544, CRxCoreErrCntrs *err_cntrs);

    void handle_pkt(const rte_mbuf_t *m);
    void update_stats_for_pkt(flow_stat_payload_header *fsp_head,
                              uint32_t pkt_len,
                              hr_time_t hr_time_now);

    Json::Value to_json() const;

    void get_stats(rx_per_flow_t *rx_stats,
                   int min,
                   int max,
                   bool reset,
                   TrexPlatformApi::driver_stat_cap_e type);

    void reset_stats();
    void reset_stats_partial(int min, int max, TrexPlatformApi::driver_stat_cap_e type);

    RXLatency operator+=(const RXLatency& in);
    friend std::ostream& operator<<(std::ostream& os, const RXLatency& in);

private:
    // below functions for both IP v4 and v6, so they need uint32_t id
    bool is_flow_stat_id(uint32_t id) {
        if ((uint16_t) id >= m_ip_id_base)
            return true;
        else
            return false;
    }

    bool is_flow_stat_payload_id(uint32_t id) {
        if (id == FLOW_STAT_PAYLOAD_IP_ID) return true;
        return false;
    }

    uint16_t get_hw_id(uint16_t id) {
        return (~m_ip_id_base & (uint16_t )id);
}

public:

    rx_per_flow_t         m_rx_pg_stat[MAX_FLOW_STATS];
    rx_per_flow_t         m_rx_pg_stat_payload[MAX_FLOW_STATS_PAYLOAD];

    bool                  m_rcv_all;
    CRFC2544Info         *m_rfc2544;
    CRxCoreErrCntrs      *m_err_cntrs;
    uint16_t              m_ip_id_base;
};

std::ostream& operator<<(std::ostream& os, const RXLatency& in);

#endif /* __TREX_LATENCY_COUNTERS_H__ */

