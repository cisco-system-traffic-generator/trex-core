/*/
  Ido Barnea
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#ifndef __FLOW_STAT_H__
#define __FLOW_STAT_H__
#include <stdio.h>
#include <string>
#include <map>
#include <json/json.h>
#include "trex_defs.h"
#include "trex_exception.h"
#include "trex_stream.h"
#include "msg_manager.h"
#include "internal_api/trex_platform_api.h"

// range reserved for rx stat measurement is from IP_ID_RESERVE_BASE to 0xffff
// Do not change this value. In i350 cards, we filter according to first byte of IP ID
// In other places, we identify packets by if (ip_id > IP_ID_RESERVE_BASE)
#define IP_ID_RESERVE_BASE 0xff00
#define FLOW_STAT_PAYLOAD_MAGIC 0xAB
#define FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ 0x01
extern const uint32_t FLOW_STAT_PAYLOAD_IP_ID;

typedef std::map<uint32_t, uint16_t> flow_stat_map_t;
typedef std::map<uint32_t, uint16_t>::iterator flow_stat_map_it_t;

class CRxCoreStateless;

struct flow_stat_payload_header {
    uint8_t magic;
    uint8_t flow_seq;
    uint16_t hw_id;
    uint32_t seq;
    uint64_t time_stamp;
};


class TrexFStatEx : public TrexException {
 public:
    TrexFStatEx(const std::string &what, enum TrexExceptionTypes_t type): TrexException(what, type) {
    }
};


class rfc2544_info_t_ {
 friend class CFlowStatUserIdInfoPayload;

 public:
    rfc2544_info_t_() {
        clear();
    }

    inline void get_latency_json(Json::Value & json) const {
        json = m_latency;
    }

    inline void set_latency_json(Json::Value json) {
        m_latency = json;
    }

    inline void set_err_cntrs(uint64_t seq, uint64_t ooo, uint64_t dup, uint64_t seq_big, uint64_t seq_low) {
        m_seq_err = seq;
        m_out_of_order = ooo;
        m_dup = dup;
        m_seq_err_ev_big = seq_big;
        m_seq_err_ev_low = seq_low;
    }

    inline uint64_t get_seq_err_cnt() {
        return m_seq_err;
    }

    inline uint64_t get_ooo_cnt() {
        return m_out_of_order;
    }

    inline uint64_t get_dup_cnt() {
        return m_dup;
    }

    inline uint64_t get_seq_err_ev_big() {
        return m_seq_err_ev_big;
    }

    inline uint64_t get_seq_err_ev_low() {
        return m_seq_err_ev_low;
    }

    inline double get_jitter() const {
        return m_jitter;
    }

    inline void set_jitter(double jitter) {
        m_jitter = jitter;
    }

    uint32_t get_jitter_usec(){
        return (uint32_t)(m_jitter * 1000000.0);
    }

    inline void set_last_max(dsec_t val) {
        m_last_max_latency = val;
    }

    inline dsec_t get_last_max() {
        return m_last_max_latency;
    }

    inline uint32_t get_last_max_usec() {
        return (uint32_t)(m_last_max_latency * 1000000.0);
    }

    inline void clear() {
        m_seq_err = 0;
        m_out_of_order = 0;
        m_dup = 0;
        m_seq_err_ev_big = 0;
        m_seq_err_ev_low = 0;
        m_jitter = 0;
        m_latency = Json::nullValue;
        m_last_max_latency = 0;
    }

    inline rfc2544_info_t_ operator+ (const rfc2544_info_t_ &t_in) {
        rfc2544_info_t_ t_out;
        t_out.m_seq_err = this->m_seq_err + t_in.m_seq_err;
        t_out.m_out_of_order = this->m_out_of_order + t_in.m_out_of_order;
        t_out.m_dup = this->m_dup + t_in.m_dup;
        t_out.m_seq_err_ev_big = this->m_seq_err_ev_big + t_in.m_seq_err_ev_big;
        t_out.m_seq_err_ev_low = this->m_seq_err_ev_low + t_in.m_seq_err_ev_low;
        return t_out;
    }

    inline rfc2544_info_t_ operator- (const rfc2544_info_t_ &t_in) {
        rfc2544_info_t_ t_out;
        t_out.m_seq_err = this->m_seq_err - t_in.m_seq_err;
        t_out.m_out_of_order = this->m_out_of_order - t_in.m_out_of_order;
        t_out.m_dup = this->m_dup - t_in.m_dup;
        t_out.m_seq_err_ev_big = this->m_seq_err_ev_big - t_in.m_seq_err_ev_big;
        t_out.m_seq_err_ev_low = this->m_seq_err_ev_low - t_in.m_seq_err_ev_low;
        return t_out;
    }

    inline rfc2544_info_t_ operator+= (const rfc2544_info_t_ &t_in) {
        m_seq_err += t_in.m_seq_err;
        m_out_of_order += t_in.m_out_of_order;
        m_dup += t_in.m_dup;
        m_seq_err_ev_big += t_in.m_seq_err_ev_big;
        m_seq_err_ev_low += t_in.m_seq_err_ev_low;
        return *this;
    }

    inline bool operator!= (const rfc2544_info_t_ &t_in) {
        if ((m_jitter != t_in.m_jitter) || (m_seq_err != t_in.m_seq_err)
            || (m_out_of_order != t_in.m_out_of_order) || (m_dup != t_in.m_dup)
            || (m_seq_err_ev_big != t_in.m_seq_err_ev_big) || (m_seq_err_ev_low != t_in.m_seq_err_ev_low))
            return true;
        return false;
    }

    friend std::ostream& operator<<(std::ostream& os, const rfc2544_info_t_ &t) {
        os  << "jitter:" << t.m_jitter << " errors(seq:"
            << t.m_seq_err << " out of order:" << t.m_out_of_order
            << " dup:" << t.m_dup << ")";
        return os;
    }

 private:
    uint64_t m_seq_err;
    uint64_t m_out_of_order;
    uint64_t m_dup;
    uint64_t m_seq_err_ev_big;
    uint64_t m_seq_err_ev_low;
    double   m_jitter;
    dsec_t   m_last_max_latency;
    // json latency object. In case of stop/start, we calculate latency graph from scratch,
    // so when stopping, we just "freeze" state for reporting by saving the json string
    Json::Value m_latency;
};

class tx_per_flow_t_ {
 public:
    tx_per_flow_t_() {
        clear();
    }
    inline uint64_t get_bytes() {
        return m_bytes;
    }
    inline uint64_t get_pkts() {
        return m_pkts;
    }
    inline void set_bytes(uint64_t bytes) {
        m_bytes = bytes;;
    }
    inline void set_pkts(uint64_t pkts) {
        m_pkts = pkts;
    }
    inline void add_bytes(uint64_t bytes) {
        m_bytes += bytes;;
    }
    inline void add_pkts(uint64_t pkts) {
        m_pkts += pkts;
    }
    inline void clear() {
        m_bytes = 0;
        m_pkts = 0;
    }
    inline tx_per_flow_t_ operator+ (const tx_per_flow_t_ &t_in) {
        tx_per_flow_t_ t_out;
        t_out.m_bytes = this->m_bytes + t_in.m_bytes;
        t_out.m_pkts = this->m_pkts + t_in.m_pkts;
        return t_out;
    }

    inline tx_per_flow_t_ operator- (const tx_per_flow_t_ &t_in) {
        tx_per_flow_t_ t_out;
        t_out.m_bytes = this->m_bytes - t_in.m_bytes;
        t_out.m_pkts = this->m_pkts - t_in.m_pkts;
        return t_out;
    }

    inline tx_per_flow_t_ operator+= (const tx_per_flow_t_ &t_in) {
        m_bytes += t_in.m_bytes;
        m_pkts += t_in.m_pkts;
        return *this;
    }

    inline bool operator!= (const tx_per_flow_t_ &t_in) {
        if ((m_bytes != t_in.m_bytes) || (m_pkts != t_in.m_pkts))
            return true;
        return false;
    }

    friend std::ostream& operator<<(std::ostream& os, const tx_per_flow_t_ &t) {
        os  << "p:" << t.m_pkts << " b:" << t.m_bytes;
        return os;
    }

 private:
    uint64_t m_bytes;
    uint64_t m_pkts;
};

typedef class rfc2544_info_t_ rfc2544_info_t;
typedef class tx_per_flow_t_ tx_per_flow_t;
typedef class tx_per_flow_t_ rx_per_flow_t;

class CPhyEthIF;
class CFlowStatParser;

class CFlowStatUserIdInfo {
 public:
    CFlowStatUserIdInfo(uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h);
    virtual ~CFlowStatUserIdInfo() {};
    friend std::ostream& operator<<(std::ostream& os, const CFlowStatUserIdInfo& cf);
    void set_rx_cntr(uint8_t port, rx_per_flow_t val) {m_rx_cntr[port] = val;}
    rx_per_flow_t get_rx_cntr(uint8_t port) {return m_rx_cntr[port] + m_rx_cntr_base[port];}
    void set_tx_cntr(uint8_t port, tx_per_flow_t val) {m_tx_cntr[port] = val;}
    tx_per_flow_t get_tx_cntr(uint8_t port) {return m_tx_cntr[port] + m_tx_cntr_base[port];}
    void set_hw_id(uint16_t hw_id) {m_hw_id = hw_id;}
    uint16_t get_hw_id() {return m_hw_id;}
    virtual void reset_hw_id();
    bool is_hw_id() {return (m_hw_id != UINT16_MAX);}
    uint16_t get_l3_proto() {return m_l3_proto;}
    uint8_t get_l4_proto() {return m_l4_proto;}
    uint8_t get_ipv6_next_h() {return m_ipv6_next_h;}
    uint8_t get_ref_count() {return m_ref_count;}
    virtual void add_stream(uint8_t proto);
    int del_stream() {m_ref_count--; return m_ref_count;}
    void add_started_stream() {m_trans_ref_count++;}
    int stop_started_stream() {m_trans_ref_count--; return m_trans_ref_count;}
    bool is_started() {return (m_trans_ref_count != 0);}
    bool need_to_send_rx(uint8_t port) {return m_rx_changed[port];}
    bool need_to_send_tx(uint8_t port) {return m_tx_changed[port];}
    void set_no_need_to_send_rx(uint8_t port) {m_rx_changed[port] = false;}
    void set_no_need_to_send_tx(uint8_t port) {m_tx_changed[port] = false;}
    void set_need_to_send_rx(uint8_t port) {m_rx_changed[port] = true;}
    void set_need_to_send_tx(uint8_t port) {m_tx_changed[port] = true;}
    bool was_sent() {return m_was_sent == true;}
    void set_was_sent(bool val) {m_was_sent = val;}
    bool rfc2544_support() {return m_rfc2544_support;}

 protected:
    bool m_rfc2544_support;
    uint16_t m_hw_id;     // Associated hw id. UINT16_MAX if no associated hw id.

 private:
    bool m_rx_changed[TREX_MAX_PORTS]; // Which RX counters changed since we last published
    bool m_tx_changed[TREX_MAX_PORTS]; // Which TX counters changed since we last published
    rx_per_flow_t m_rx_cntr[TREX_MAX_PORTS]; // How many packets received with this user id since stream start
    // How many packets received with this user id, since stream creation, before stream start.
    rx_per_flow_t m_rx_cntr_base[TREX_MAX_PORTS];
    tx_per_flow_t m_tx_cntr[TREX_MAX_PORTS]; // How many packets transmitted with this user id since stream start
    // How many packets transmitted with this user id, since stream creation, before stream start.
    tx_per_flow_t m_tx_cntr_base[TREX_MAX_PORTS];
    uint16_t m_l3_proto;      // L3 protocol (IPv4, IPv6), associated with this user id.
    uint8_t m_l4_proto;      // L4 protocol (UDP, TCP, other), associated with this user id.
    uint8_t m_ipv6_next_h;   // In case of IPv6, what is the type of the first extension header
    uint8_t m_ref_count;  // How many streams with this user id exists
    uint8_t m_trans_ref_count;  // How many streams with this user id currently transmit
    bool m_was_sent; // Did we send this info to clients once?
};

typedef std::map<uint32_t, class CFlowStatUserIdInfo *> flow_stat_user_id_map_t;
typedef std::map<uint32_t, class CFlowStatUserIdInfo *>::iterator flow_stat_user_id_map_it_t;

class CFlowStatUserIdInfoPayload : public CFlowStatUserIdInfo {
 public:
    CFlowStatUserIdInfoPayload(uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h)
                               : CFlowStatUserIdInfo(l3_proto, l4_proto, ipv6_next_h)
        {m_rfc2544_support = true; clear();}
    virtual void add_stream(uint8_t proto);

    void clear() {
        m_rfc2544_info.clear();
        m_seq_err_base = 0;
        m_out_of_order_base = 0;
        m_dup_base = 0;
        m_seq_err_ev_big_base = 0;
        m_seq_err_ev_low_base = 0;
    }
    inline void get_latency_json(Json::Value & json) const {
        json = m_rfc2544_info.m_latency;
    }

    inline void set_latency_json(const Json::Value &json) {
        m_rfc2544_info.m_latency = json;
    }

    inline double get_jitter() const {
        return m_rfc2544_info.get_jitter();
    }

    inline void set_jitter(double jitter) {
        m_rfc2544_info.set_jitter(jitter);
    }

    uint32_t get_jitter_usec(){
        return m_rfc2544_info.get_jitter_usec();
    }

    inline void set_seq_err_cnt(uint64_t cnt) {
        m_rfc2544_info.m_seq_err = cnt;
    }

    inline uint64_t get_seq_err_cnt() const {
        return m_rfc2544_info.m_seq_err + m_seq_err_base;
    }

    inline void set_ooo_cnt(uint64_t cnt) {
        m_rfc2544_info.m_out_of_order = cnt;
    }

    inline uint64_t get_ooo_cnt() const {
        return m_rfc2544_info.m_out_of_order + m_out_of_order_base;
    }

    inline void set_dup_cnt(uint64_t cnt) {
        m_rfc2544_info.m_dup = cnt;
    }

    inline uint64_t get_dup_cnt() const {
        return m_rfc2544_info.m_dup + m_dup_base;
    }

    inline void set_seq_err_big_cnt(uint64_t cnt) {
        m_rfc2544_info.m_seq_err_ev_big = cnt;
    }

    inline uint64_t get_seq_err_big_cnt() const {
        return m_rfc2544_info.m_seq_err_ev_big + m_seq_err_ev_big_base;
    }

    inline void set_seq_err_low_cnt(uint64_t cnt) {
        m_rfc2544_info.m_seq_err_ev_low = cnt;
    }

    inline uint64_t get_seq_err_low_cnt() const {
        return m_rfc2544_info.m_seq_err_ev_low + m_seq_err_ev_low_base;
    }

    inline void reset_hw_id();
 private:
    rfc2544_info_t m_rfc2544_info;
    uint64_t m_seq_err_base;
    uint64_t m_out_of_order_base;
    uint64_t m_dup_base;
    uint64_t m_seq_err_ev_big_base;
    uint64_t m_seq_err_ev_low_base;
};

class CFlowStatUserIdMap {
 public:
    CFlowStatUserIdMap();
    friend std::ostream& operator<<(std::ostream& os, const CFlowStatUserIdMap& cf);
    bool is_empty() {return (m_map.empty() == true);};
    uint16_t get_hw_id(uint32_t user_id);
    CFlowStatUserIdInfo * find_user_id(uint32_t user_id);
    CFlowStatUserIdInfo * add_user_id(uint32_t user_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h);
    void add_stream(uint32_t user_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h);
    int del_stream(uint32_t user_id);
    int start_stream(uint32_t user_id, uint16_t hw_id);
    int start_stream(uint32_t user_id);
    int stop_stream(uint32_t user_id);
    bool is_started(uint32_t user_id);
    uint16_t unmap(uint32_t user_id);
    flow_stat_user_id_map_it_t begin() {return m_map.begin();}
    flow_stat_user_id_map_it_t end() {return m_map.end();}
 private:
    flow_stat_user_id_map_t m_map;
};

class CFlowStatHwIdMap {
 public:
    CFlowStatHwIdMap();
    ~CFlowStatHwIdMap();
    void create(uint16_t size);
    friend std::ostream& operator<<(std::ostream& os, const CFlowStatHwIdMap& cf);
    uint16_t find_free_hw_id();
    void map(uint16_t hw_id, uint32_t user_id);
    void unmap(uint16_t hw_id);
    uint32_t get_user_id(uint16_t hw_id) {return m_map[hw_id];};
 private:
    uint32_t *m_map; // translation from hw id to user id
    uint16_t m_num_free; // How many free entries in the m_rules array
};

class CFlowStatRuleMgr {
 public:
    enum flow_stat_rule_types_e {
        FLOW_STAT_RULE_TYPE_NONE,
        FLOW_STAT_RULE_TYPE_IPV4_ID,
        FLOW_STAT_RULE_TYPE_PAYLOAD,
        FLOW_STAT_RULE_TYPE_IPV6_FLOW_LABEL,
    };

    enum flow_stat_mode_e {
        FLOW_STAT_MODE_NORMAL,
        FLOW_STAT_MODE_PASS_ALL,
    };

    CFlowStatRuleMgr();
    ~CFlowStatRuleMgr();
    friend std::ostream& operator<<(std::ostream& os, const CFlowStatRuleMgr& cf);
    void copy_state(TrexStream * from, TrexStream * to);
    void init_stream(TrexStream * stream);
    int verify_stream(TrexStream * stream);
    int add_stream(TrexStream * stream);
    int del_stream(TrexStream * stream);
    int start_stream(TrexStream * stream);
    int stop_stream(TrexStream * stream);
    int get_active_pgids(flow_stat_active_t &result);
    int set_mode(enum flow_stat_mode_e mode);
    bool dump_json(std::string & s_json, std::string & l_json, bool baseline);

 private:
    void create();
    int compile_stream(const TrexStream * stream, CFlowStatParser *parser);
    int add_stream_internal(TrexStream * stream, bool do_action);
    int add_hw_rule(uint16_t hw_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h);
    void send_start_stop_msg_to_rx(bool is_start);

 private:
    CFlowStatHwIdMap m_hw_id_map; // map hw ids to user ids
    CFlowStatHwIdMap m_hw_id_map_payload; // map hw id numbers of payload rules to user ids
    CFlowStatUserIdMap m_user_id_map; // map user ids to hw ids
    uint8_t m_num_ports; // How many ports are being used
    const TrexPlatformApi *m_api;
    const CRxCoreStateless *m_rx_core;
    int m_max_hw_id; // max hw id we ever used
    int m_max_hw_id_payload; // max hw id we ever used for payload rules
    int m_num_started_streams; // How many started (transmitting) streams we have
    CNodeRing *m_ring_to_rx; // handle for sending messages to Rx core
    CFlowStatParser *m_parser_ipid; // for IP_ID rules (flow stat)
    CFlowStatParser *m_parser_pl;  // for payload rules (latency)
    enum flow_stat_mode_e m_mode;
    uint16_t m_cap; // capabilities of the NIC driver we are using
    uint32_t m_rx_cant_count_err[TREX_MAX_PORTS];
    uint32_t m_tx_cant_count_err[TREX_MAX_PORTS];
};

#endif
