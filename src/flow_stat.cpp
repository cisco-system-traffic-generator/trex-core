/*
  Ido Barnea
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2015-2017 Cisco Systems, Inc.

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

/*
Important classes in this file:
CFlowStatUserIdInfo - Information about one packet group id
CFlowStatUserIdMap - Mapping between packet group id (chosen by user) and hardware counter id
CFlowStatHwIdMap - Mapping between hardware id and packet group id
CFlowStatRuleMgr - API to users of the file

General idea of operation:
For each stream needing flow statistics, the user provides packet group id (pg_id). Few streams can have the same pg_id.
We maintain reference count. With latency streams (payload rules), pg_id is unique per stream.
When doing start_stream, for the first stream in pg_id, hw_id is associated with the pg_id, and relevant hardware rules are
inserted (on supported hardware). When removing all streams with the pg_id, the hw_id <--> pg_id mapping is removed, hw_id is
returned to the free hw_id pool, and hardware rules are removed. Counters for the pg_id are kept.

For each stream we keep state in the m_rx_check.m_hw_id field. Since we keep reference count for certain structs, we want to
protect from illegal operations, like starting stream while it is already started, stopping when it is stopped...
State machine is:
init_stream: HW_ID_INIT
add_stream: HW_ID_FREE
start_stream: legal hw_id (range is 0..MAX_FLOW_STATS)
stop_stream: HW_ID_STOPPED
del_stream: HW_ID_INIT
reset_stream: HW_ID_FREE - putting it back as if it was just added
 */
#include <sstream>
#include <string>
#include <iostream>
#include <assert.h>
#include <os_time.h>
#include <stdarg.h>
#include "internal_api/trex_platform_api.h"
#include "trex_stateless.h"
#include "trex_stateless_messaging.h"
#include "trex_stateless_rx_core.h"
#include "trex_stream.h"
#include "flow_stat_parser.h"
#include "flow_stat.h"


#define FLOW_STAT_ADD_ALL_PORTS 255

static const uint16_t HW_ID_INIT = UINT16_MAX;
static const uint16_t HW_ID_FREE = UINT16_MAX - 1;
static const uint16_t HW_ID_STOPPED = UINT16_MAX - 2;
static const uint8_t PAYLOAD_RULE_PROTO = 255;
static const uint32_t MAX_ALLOWED_PGID_LIST_LEN = 1024+128;
const uint32_t FLOW_STAT_PAYLOAD_IP_ID = UINT16_MAX;
// make the class singelton
CFlowStatRuleMgr* CFlowStatRuleMgr::m_pInstance = NULL;

inline std::string methodName(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return prettyFunction.substr(begin,end) + "()";
}

void dbg_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#define EXTRA_DEBUG 0
#define DEBUG_PRINT(...) \
    do { if (EXTRA_DEBUG) fprintf(stderr, __VA_ARGS__); } while (0);

#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)
#ifdef __DEBUG_FUNC_ENTRY__
#define FUNC_ENTRY (std::cout << __METHOD_NAME__ << std::endl);
#ifdef __STREAM_DUMP__
#define stream_dump(stream) stream->Dump(stderr)
#else
#define stream_dump(stream)
#endif
#else
#define FUNC_ENTRY
#endif

/************** class CFlowStatUserIdInfo ***************/
CFlowStatUserIdInfo::CFlowStatUserIdInfo(uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h) {
    memset(m_rx_cntr, 0, sizeof(m_rx_cntr));
    memset(m_tx_cntr, 0, sizeof(m_tx_cntr));
    m_hw_id = HW_ID_INIT;
    m_l3_proto = l3_proto;
    m_l4_proto = l4_proto;
    m_ipv6_next_h = ipv6_next_h;
    m_ref_count = 1;
    m_trans_ref_count = 0;
    m_was_sent = false;
    m_rfc2544_support = false;
}

std::ostream& operator<<(std::ostream& os, const CFlowStatUserIdInfo& cf) {
    os << "hw_id:" << cf.m_hw_id << " l3 proto:" << (uint16_t) cf.m_l3_proto << " ref("
       << (uint16_t) cf.m_ref_count << "," << (uint16_t) cf.m_trans_ref_count << ")";
    os << " rx count (";
    os << cf.m_rx_cntr[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_rx_cntr[i];
    }

    os << " tx count (";
    os << cf.m_tx_cntr[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_tx_cntr[i];
    }

    return os;
}

void CFlowStatUserIdInfo::add_stream(uint8_t proto) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " proto:" << (uint16_t)proto << std::endl;
#endif

    if (proto != m_l4_proto)
        throw TrexFStatEx("Can't use same pg_id for streams with different l4 protocol",
                                    TrexException::T_FLOW_STAT_PG_ID_DIFF_L4);

    m_ref_count++;
}

void CFlowStatUserIdInfo::reset_hw_id() {
    FUNC_ENTRY;

    m_hw_id = HW_ID_INIT;
    for (int i = 0; i < TREX_MAX_PORTS; i++) {
        memset(&m_rx_cntr[i], 0, sizeof(m_rx_cntr[0]));
        memset(&m_tx_cntr[i], 0, sizeof(m_tx_cntr[0]));
    }
}

void CFlowStatUserIdInfo::update_vals(const rx_per_flow_t val, tx_per_flow_with_rate_t & to_update
                                      , bool is_last, hr_time_t curr_time, hr_time_t freq, bool rate_update) {
    if (is_last) {
        to_update.set_p_rate(0);
        to_update.set_b_rate(0);
        to_update.set_last_rate_calc_time(0);
        to_update.set_pkt_base(val.get_pkts());
        to_update.set_byte_base(val.get_bytes());
    } else {
        if (to_update.get_last_rate_calc_time() != 0) {
            if (rate_update) {
                double time_diff = ((double)curr_time - (double)to_update.get_last_rate_calc_time()) / (double)freq;
                uint64_t pkt_diff = val.get_pkts() - to_update.get_pkt_base();
                uint64_t byte_diff = val.get_bytes() - to_update.get_byte_base();
                float curr_p_rate = pkt_diff / time_diff;
                float curr_b_rate = byte_diff * 8 / time_diff;

                if (to_update.get_p_rate() != 0) {
                    to_update.set_p_rate(to_update.get_p_rate() * 0.5 + curr_p_rate * 0.5);
                    to_update.set_b_rate(to_update.get_b_rate() * 0.5 + curr_b_rate * 0.5);
                } else {
                    to_update.set_p_rate(curr_p_rate);
                    to_update.set_b_rate(curr_b_rate);
                }
                to_update.set_last_rate_calc_time(curr_time);
                to_update.set_pkt_base(val.get_pkts());
                to_update.set_byte_base(val.get_bytes());
            }
        } else {
            to_update.set_last_rate_calc_time(curr_time);
            to_update.set_pkt_base(val.get_pkts());
            to_update.set_byte_base(val.get_bytes());
        }
    }

    to_update.set_bytes(val.get_bytes());
    to_update.set_pkts(val.get_pkts());
}


/************** class CFlowStatUserIdInfoPayload ***************/
void CFlowStatUserIdInfoPayload::add_stream(uint8_t proto) {
    throw TrexFStatEx("For payload rules: Can't have two streams with same pg_id, or same stream on more than one port"
                      , TrexException::T_FLOW_STAT_DUP_PG_ID);
}

void CFlowStatUserIdInfoPayload::reset_hw_id() {
    CFlowStatUserIdInfo::reset_hw_id();
    m_rfc2544_info.m_seq_err = 0;
    m_rfc2544_info.m_out_of_order = 0;
    m_rfc2544_info.m_dup = 0;
    m_rfc2544_info.m_seq_err_ev_big = 0;
    m_rfc2544_info.m_seq_err_ev_low = 0;
}

/************** class CFlowStatUserIdMap ***************/
CFlowStatUserIdMap::CFlowStatUserIdMap() {
  m_ver_id_pool = 0;
}

std::ostream& operator<<(std::ostream& os, const CFlowStatUserIdMap& cf) {
    std::map<unsigned int, CFlowStatUserIdInfo*>::const_iterator it;
    for (it = cf.m_map.begin(); it != cf.m_map.end(); it++) {
        CFlowStatUserIdInfo *user_id_info = it->second;
        uint32_t user_id = it->first;
        os << "Flow stat user id info:\n";
        os << "  " << user_id << ":" << *user_id_info << std::endl;
    }
    return os;
}

uint16_t CFlowStatUserIdMap::get_hw_id(uint32_t user_id) {
    CFlowStatUserIdInfo *cf = find_user_id(user_id);

    if (cf == NULL) {
        return HW_ID_FREE;
    } else {
        return cf->get_hw_id();
    }
}

CFlowStatUserIdInfo *
CFlowStatUserIdMap::find_user_id(uint32_t user_id) {
    flow_stat_user_id_map_it_t it = m_map.find(user_id);

    if (it == m_map.end()) {
        return NULL;
    } else {
        return it->second;
    }
}

CFlowStatUserIdInfo *
CFlowStatUserIdMap::add_user_id(uint32_t user_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " l3 proto:" << (uint16_t)l3_proto
              << std::endl;
#endif

    CFlowStatUserIdInfo *new_id;

    if (l4_proto == PAYLOAD_RULE_PROTO) {
        new_id = new CFlowStatUserIdInfoPayload(l3_proto, l4_proto, ipv6_next_h);
    } else {
        new_id = new CFlowStatUserIdInfo(l3_proto, l4_proto, ipv6_next_h);
    }
    if (new_id != NULL) {
        std::pair<flow_stat_user_id_map_it_t, bool> ret;
        ret = m_map.insert(std::pair<uint32_t, CFlowStatUserIdInfo *>(user_id, new_id));
        if (ret.second == false) {
            delete new_id;
            throw TrexFStatEx("packet group id " + std::to_string(user_id) + " already exists"
                              , TrexException::T_FLOW_STAT_ALREADY_EXIST);
        }
        return new_id;
    } else {
        throw TrexFStatEx("Failed allocating memory for new statistic counter"
                          , TrexException::T_FLOW_STAT_ALLOC_FAIL);
    }
}

void CFlowStatUserIdMap::add_stream(uint32_t user_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " l3 proto:" << (uint16_t)l3_proto
              << " l4 proto:" << (uint16_t)l4_proto << " IPv6 next header:" << (uint16_t)ipv6_next_h
              << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        // throws exception on error
        c_user_id = add_user_id(user_id, l3_proto, l4_proto, ipv6_next_h);
        c_user_id->set_ver_id(m_ver_id_pool++);
    } else {
        c_user_id->add_stream(l4_proto);
    }

#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << "exit:" << __METHOD_NAME__ << " user id:" << user_id << " ver_id:" << c_user_id->get_ver_id()
              << std::endl;
#endif

}

int CFlowStatUserIdMap::del_stream(uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        throw TrexFStatEx("Trying to delete stream which does not exist"
                          , TrexException::T_FLOW_STAT_DEL_NON_EXIST);
    }

    int ret = c_user_id->del_stream();
    if (ret == 0) {
        // ref count of this entry became 0. can release this entry.
        m_map.erase(user_id);
        delete c_user_id;
    }

    return ret;
}

int CFlowStatUserIdMap::start_stream(uint32_t user_id, uint16_t hw_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " hw_id:" << hw_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        throw TrexFStatEx("Internal error: Trying to associate non exist group id " + std::to_string(user_id)
                          + " to hardware id " + std::to_string(hw_id)
                          , TrexException::T_FLOW_STAT_ASSOC_NON_EXIST_ID);
    }

    if (c_user_id->is_hw_id()) {
        throw TrexFStatEx("Internal error: Trying to associate hw id " + std::to_string(hw_id) + " to user_id "
                          + std::to_string(user_id) + ", but it is already associated to "
                          + std::to_string(c_user_id->get_hw_id())
                          , TrexException::T_FLOW_STAT_ASSOC_OCC_ID);
    }
    c_user_id->set_hw_id(hw_id);
    c_user_id->add_started_stream();

    return 0;
}

int CFlowStatUserIdMap::start_stream(uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        throw TrexFStatEx("Trying to start stream with non exist packet group id " + std::to_string(user_id)
                          , TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    c_user_id->add_started_stream();

    return 0;
}

// return: negative number in case of error.
//         Number of started streams attached to used_id otherwise.
int CFlowStatUserIdMap::stop_stream(uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        throw TrexFStatEx("Trying to stop stream with non exist packet group id" + std::to_string(user_id)
                          , TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    return c_user_id->stop_started_stream();
}

bool CFlowStatUserIdMap::is_started(uint32_t user_id) {
    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        return false;
    }

    return c_user_id->is_started();
}

uint16_t CFlowStatUserIdMap::unmap(uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        return HW_ID_INIT;
    }
    uint16_t old_hw_id = c_user_id->get_hw_id();
    c_user_id->reset_hw_id();

    return old_hw_id;
}

/************** class CFlowStatHwIdMap ***************/
CFlowStatHwIdMap::CFlowStatHwIdMap() {
    m_map = NULL; // must call create in order to work with the class
    m_num_free = 0; // to make coverity happy, init this here too.
    m_size = 0;
}

CFlowStatHwIdMap::~CFlowStatHwIdMap() {
    delete[] m_map;
}

void CFlowStatHwIdMap::create(uint16_t size) {
    m_map = new uint32_t[size];
    assert (m_map != NULL);
    m_num_free = size;
    for (int i = 0; i < size; i++) {
        m_map[i] = HW_ID_FREE;
    }
    m_size = size;
}

std::ostream& operator<<(std::ostream& os, const CFlowStatHwIdMap& cf) {
    int count = 0;

    os << "HW id map:\n";
    os << "  num free:" << cf.m_num_free << std::endl;
    for (int i = 0; i < cf.m_size; i++) {
        if (cf.m_map[i] != HW_ID_FREE) {
            count++;
            os << "(" << i << ":" << cf.m_map[i] << ")";
            if (count == 10) {
                os << std::endl;
                count = 0;
            }
        }
    }

    return os;
}

uint16_t CFlowStatHwIdMap::find_free_hw_id() {
    for (int i = 0; i < m_size; i++) {
        if (m_map[i] == HW_ID_FREE)
            return i;
    }

    return HW_ID_FREE;
}

void CFlowStatHwIdMap::map(uint16_t hw_id, uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " hw id:" << hw_id << " user id:" << user_id << std::endl;
#endif

    m_map[hw_id] = user_id;
    m_num_free--;
}

void CFlowStatHwIdMap::unmap(uint16_t hw_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " hw id:" << hw_id << std::endl;
#endif

    m_map[hw_id] = HW_ID_FREE;
    m_num_free++;
}

/************** class CFlowStatRuleMgr ***************/
CFlowStatRuleMgr::CFlowStatRuleMgr() {
    m_api = NULL;
    m_max_hw_id = -1;
    m_max_hw_id_payload = -1;
    m_num_started_streams = 0;
    m_ring_to_rx = NULL;
    m_cap = 0;
    m_parser_ipid = NULL;
    m_parser_pl = NULL;
    m_rx_core = NULL;
    memset(m_rx_cant_count_err, 0, sizeof(m_rx_cant_count_err));
    memset(m_tx_cant_count_err, 0, sizeof(m_tx_cant_count_err));
    m_num_ports = 0; // need to call create to init
    m_mode = FLOW_STAT_MODE_NORMAL;
}

CFlowStatRuleMgr::~CFlowStatRuleMgr() {
    delete m_parser_ipid;
    delete m_parser_pl;
#ifdef TREX_SIM
    // In simulator, nobody handles the messages to RX, so need to free them to have clean valgrind run.
    if (m_ring_to_rx) {
        CGenNode *msg = NULL;
        while (! m_ring_to_rx->isEmpty()) {
            m_ring_to_rx->Dequeue(msg);
            delete msg;
        }
    }
#endif
}

void CFlowStatRuleMgr::create() {
    uint16_t num_counters, cap, ip_id_base;
    TrexStateless *tstateless = get_stateless_obj();
    assert(tstateless);

    m_api = tstateless->get_platform_api();
    assert(m_api);
    m_api->get_port_stat_info(0, num_counters, cap, ip_id_base);
    m_api->get_port_num(m_num_ports); // This initialize m_num_ports
    for (uint8_t port = 0; port < m_num_ports; port++) {
        assert(m_api->reset_hw_flow_stats(port) == 0);
    }
    m_hw_id_map.create(num_counters);
    m_hw_id_map_payload.create(MAX_FLOW_STATS_PAYLOAD);
    m_ring_to_rx = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    assert(m_ring_to_rx);
    m_rx_core = get_rx_sl_core_obj();
    m_cap = cap;
    m_ip_id_reserve_base = ip_id_base;

    if ((CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_ONE_QUEUE)
        || (CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_RSS)) {
        set_mode(FLOW_STAT_MODE_PASS_ALL);
        m_parser_ipid = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
        m_parser_pl = new CPassAllParser;
    } else {
        m_parser_ipid = m_api->get_flow_stat_parser();
        m_parser_pl = m_api->get_flow_stat_parser();
    }
    assert(m_parser_ipid);
    assert(m_parser_pl);
}

std::ostream& operator<<(std::ostream& os, const CFlowStatRuleMgr& cf) {
    os << "Flow stat rule mgr (" << cf.m_num_ports << ") ports:" << std::endl;
    os << cf.m_hw_id_map;
    os << cf.m_hw_id_map_payload;
    os << cf.m_user_id_map;
    return os;
}

int CFlowStatRuleMgr::compile_stream(const TrexStream * stream, CFlowStatParser *parser) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << " en:";
    std::cout << stream->m_rx_check.m_enabled << std::endl;
#endif
    CFlowStatParser_err_t ret = parser->parse(stream->m_pkt.binary, stream->m_pkt.len);

    if (ret != FSTAT_PARSER_E_OK) {
        // if we could not parse the packet, but no stat count needed, it is probably OK.
        if (stream->m_rx_check.m_enabled) {
            throw TrexFStatEx(parser->get_error_str(ret), TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
        } else {
            return 0;
        }
    }

    return 0;
}

void CFlowStatRuleMgr::copy_state(TrexStream * from, TrexStream * to) {
    to->m_rx_check.m_hw_id = from->m_rx_check.m_hw_id;
}
void CFlowStatRuleMgr::init_stream(TrexStream * stream) {
    stream->m_rx_check.m_hw_id = HW_ID_INIT;
}

int CFlowStatRuleMgr::verify_stream(TrexStream * stream) {
    return add_stream_internal(stream, false);
}

int CFlowStatRuleMgr::add_stream(TrexStream * stream) {
    return add_stream_internal(stream, true);
}

/*
 * Helper function for adding/verifying streams
 * stream - stream to act on
 * do_action - if false, just verify. Do not change any state, or add to database.
 */
int CFlowStatRuleMgr::add_stream_internal(TrexStream * stream, bool do_action) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << " action:" << do_action << std::endl;
    stream_dump(stream);
#endif

    if (! stream->m_rx_check.m_enabled) {
        return 0;
    }

    // Init everything here, and not in the constructor, since we relay on other objects
    // By the time a stream is added everything else is initialized.
    if (! m_api ) {
        create();
    }

    TrexPlatformApi::driver_stat_cap_e rule_type = (TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type;

    if ((m_cap & rule_type) == 0) {
        throw TrexFStatEx("Interface does not support given rule type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE_FOR_IF);
    }

    TrexStreamPktLenData *pkt_len_data = stream->get_pkt_size();
    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        uint16_t l3_proto;
        // compile_stream throws exception if something goes wrong
        compile_stream(stream, m_parser_ipid);

        if (m_parser_ipid->get_l3_proto(l3_proto) < 0) {
            throw TrexFStatEx("Failed determining l3 proto for packet", TrexException::T_FLOW_STAT_FAILED_FIND_L3);
        }
        uint8_t l4_proto;
        if (m_parser_ipid->get_l4_proto(l4_proto) < 0) {
            throw TrexFStatEx("Failed determining l4 proto for packet", TrexException::T_FLOW_STAT_FAILED_FIND_L4);
        }


        // throws exception if there is error
        if (do_action) {
            // passing 0 in ipv6_next_h. This is not used currently in stateless.
            m_user_id_map.add_stream(stream->m_rx_check.m_pg_id, l3_proto, l4_proto, 0);
        }
        break;
    case TrexPlatformApi::IF_STAT_PAYLOAD:
        uint16_t payload_len;
        // compile_stream throws exception if something goes wrong
        compile_stream(stream, m_parser_pl);

        if (m_parser_pl->get_payload_len(stream->m_pkt.binary, stream->m_pkt.len, payload_len) < 0) {
            throw TrexFStatEx("Failed getting payload len", TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
        }
        if (payload_len < sizeof(struct flow_stat_payload_header)) {
            throw TrexFStatEx("Need at least " + std::to_string(sizeof(struct latency_header))
                              + " payload bytes for payload rules. Packet only has " + std::to_string(payload_len) + " bytes"
                              , TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
        }

        if (pkt_len_data->m_min_pkt_len != pkt_len_data->m_max_pkt_len) {
            // We have field engine which changes packets size
            uint16_t min_pkt_len = stream->m_pkt.len - payload_len + sizeof(struct flow_stat_payload_header);
            if (min_pkt_len < 60 + sizeof(struct flow_stat_payload_header)) {
                // We have first mbuf of 60, and then we need the payload data to be contiguous.
                min_pkt_len = 60 + sizeof(struct flow_stat_payload_header);
            }
            if (pkt_len_data->m_min_pkt_len < min_pkt_len) {
                throw TrexFStatEx("You specified field engine with minimum packet size of " + std::to_string(pkt_len_data->m_min_pkt_len) + " bytes."
                                  + " For latency together with random packet size, minimum packet size should be at least "
                                  + std::to_string(min_pkt_len)
                                  , TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
            }
        }

        if (do_action) {
            m_user_id_map.add_stream(stream->m_rx_check.m_pg_id, 0, PAYLOAD_RULE_PROTO, 0);
        }
        break;
    default:
        throw TrexFStatEx("Wrong rule_type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
        break;
    }
    if (do_action) {
        stream->m_rx_check.m_hw_id = HW_ID_FREE;
    }
    return 0;
}


// Bring stream back to HW_ID_INIT state.
int CFlowStatRuleMgr::reset_stream(TrexStream * stream) {
    return del_stream_internal(stream, false);
}

// delete stream
int CFlowStatRuleMgr::del_stream(TrexStream * stream) {
    return del_stream_internal(stream, true);
}

// if need_to_delete == true, delete the stream
// if it equals false, do the same actions, but do not really delete the stream.
// Just bring it back to HW_ID_INIT state.
int CFlowStatRuleMgr::del_stream_internal(TrexStream * stream, bool need_to_delete) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id
              << " hw_id:" << stream->m_rx_check.m_hw_id << " delete:"<< need_to_delete <<std::endl;
    stream_dump(stream);
#endif

    if (! stream->m_rx_check.m_enabled) {
        return 0;
    }

    if (! m_api)
        throw TrexFStatEx("Called del_stream, but no stream was added", TrexException::T_FLOW_STAT_NO_STREAMS_EXIST);

    TrexPlatformApi::driver_stat_cap_e rule_type = (TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type;
    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        break;
    case TrexPlatformApi::IF_STAT_PAYLOAD:
        break;
    default:
        throw TrexFStatEx("Wrong rule_type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
        break;
    }

    bool need_to_unmap = false;

    // calling del for same stream twice, or for a stream which was never "added"
    if(stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        return 0;
    }

    // we got del_stream command for a stream which has valid hw_id.
    // Probably someone forgot to call stop
    if(stream->m_rx_check.m_hw_id < MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        // if last transmitting stream, need to free hw_id
        internal_stop_stream(stream);
        stream->m_rx_check.m_hw_id = HW_ID_STOPPED;
    }

    if  (stream->m_rx_check.m_hw_id == HW_ID_STOPPED) {
        CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(stream->m_rx_check.m_pg_id);
        if (p_user_id && (! p_user_id->is_started()) ) {
            need_to_unmap = true;
        }
        stream->m_rx_check.m_hw_id = HW_ID_FREE;
    }

    // stream->m_rx_check.m_hw_id is HW_ID_FREE

    uint16_t hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id);
    // no mapping found. Stream already unmapped
    if (hw_id == HW_ID_FREE || hw_id == HW_ID_INIT) {
        need_to_unmap = false;
    }

    // stopped last stream with this hw_id <-->pd_id association. Need to unmap
    if (need_to_unmap) {
        CFlowStatUserIdInfo *p_user_id = NULL;
        if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
            p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(hw_id));
            if (p_user_id) {
                m_hw_id_map.unmap(hw_id);
            }
        } else {
            p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(hw_id));
            if (p_user_id) {
                m_hw_id_map_payload.unmap(hw_id);
            }
        }
        if (p_user_id) {
            for (uint8_t port = 0; port < m_num_ports; port++) {
                if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                    m_api->del_rx_flow_stat_rule(port, p_user_id->get_l3_proto(), p_user_id->get_l4_proto()
                                                 , p_user_id->get_ipv6_next_h(), hw_id);
                }
            }
            m_user_id_map.unmap(stream->m_rx_check.m_pg_id);
        }

        rx_per_flow_t rx_cntr;
        tx_per_flow_t tx_cntr;
        // clean stat for hw_id
        for (uint8_t port = 0; port < m_num_ports; port++) {
            m_api->get_flow_stats(port, &rx_cntr, (void *)&tx_cntr, hw_id, hw_id, true, rule_type);
        }
    }

    if (need_to_delete) {
        stream->m_rx_check.m_hw_id = HW_ID_INIT;
        m_user_id_map.del_stream(stream->m_rx_check.m_pg_id); // Throws exception in case of error
    } else {
        stream->m_rx_check.m_hw_id = HW_ID_FREE;
    }

    if (m_user_id_map.is_empty()) {
        m_max_hw_id = -1;
        m_max_hw_id_payload = -1;
    }

    return 0;
}

// called on all streams, when stream start to transmit
// If stream need flow stat counting, make sure the type of packet is supported, and
// embed needed info in packet.
// If stream does not need flow stat counting, make sure it does not interfere with
// other streams that do need stat counting.
// Might change the IP ID of the stream packet
int CFlowStatRuleMgr::start_stream(TrexStream * stream) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << " hw id:" << stream->m_rx_check.m_hw_id << std::endl;
    stream_dump(stream);
#endif

    int ret;
    // Streams which does not need statistics might be started, before any stream that do
    // need statistcs, so start_stream might be called before add_stream
    if (! m_api ) {
        create();
    }

    // first handle streams that do not need rx stat
    if (! stream->m_rx_check.m_enabled) {
        try {
            compile_stream(stream, m_parser_ipid);
        } catch (TrexFStatEx) {
            // If no statistics needed, and we can't parse the stream, that's OK.
            DEBUG_PRINT("  No rx check needed. Compilation failed - return 0\n");
            return 0;
        }

        uint32_t ip_id; // 32 bit, since this also supports IPv6
        if (m_parser_ipid->get_ip_id(ip_id) < 0) {
            DEBUG_PRINT("  No rx check needed. Failed reading IP ID - return 0\n");
            return 0; // if we could not find the ip id, no need to fix
        }
        // verify no reserved IP_ID used, and change if needed
        if (ip_id >= m_ip_id_reserve_base) {
            m_parser_ipid->set_ip_id(ip_id & 0x0000efff);
        }
        DEBUG_PRINT("  No rx check needed. - return 0\n");
        return 0;
    }

    // from here, we know the stream need rx stat

    TrexPlatformApi::driver_stat_cap_e rule_type = (TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type;

    if ((m_cap & rule_type) == 0) {
        throw TrexFStatEx("Interface does not support given rule type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE_FOR_IF);
    }

    // Starting a stream which was never added
    if (stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        add_stream(stream);
    }

    if (stream->m_rx_check.m_hw_id < MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        // stream already started
        return 0;
    }

    /// Stream is either HW_ID_STOPPED or HW_ID_FREE

    uint16_t hw_id;
    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        // compile_stream throws exception if something goes wrong
        if ((ret = compile_stream(stream, m_parser_ipid)) < 0)
            return ret;
        break;
    case TrexPlatformApi::IF_STAT_PAYLOAD:
        // compile_stream throws exception if something goes wrong
        if ((ret = compile_stream(stream, m_parser_pl)) < 0)
            return ret;
        break;
    default:
        throw TrexFStatEx("Wrong rule_type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
        break;
    }

    if (m_user_id_map.is_started(stream->m_rx_check.m_pg_id)) {
        m_user_id_map.start_stream(stream->m_rx_check.m_pg_id); // just increase ref count;
        hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id); // can't fail if we got here
        DEBUG_PRINT("  Restarting stream pg_id:%d, hw_id:%d\n", stream->m_rx_check.m_pg_id, hw_id);
    } else {

        uint32_t user_id = stream->m_rx_check.m_pg_id;
        bool new_hw_id = false;
        // if stream started and then stopped, it will have hw_id. If first time start, need to allocate.
        hw_id = m_user_id_map.get_hw_id(user_id);
        if (hw_id >= HW_ID_FREE) {
            new_hw_id = true;
            if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                hw_id = m_hw_id_map.find_free_hw_id();
            } else {
                hw_id = m_hw_id_map_payload.find_free_hw_id();
            }
        }

        if (stream->m_rx_check.m_hw_id == HW_ID_STOPPED) {
            m_user_id_map.start_stream(user_id);
            hw_id = m_user_id_map.get_hw_id(user_id);
            new_hw_id = false;
        }

        DEBUG_PRINT("  pg_id:%d, hw_id:%d - %s\n", user_id, hw_id, new_hw_id ? "new": "old");

        if (hw_id == HW_ID_FREE) {
            throw TrexFStatEx("Failed allocating statistic counter. Probably all are used for this rule type."
                              , TrexException::T_FLOW_STAT_NO_FREE_HW_ID);
        } else {
            if (new_hw_id) {
                m_user_id_map.start_stream(user_id, hw_id);
                if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                    if (hw_id > m_max_hw_id) {
                        m_max_hw_id = hw_id;
                    }
                    m_hw_id_map.map(hw_id, user_id);
                    CFlowStatUserIdInfo *uid_info = m_user_id_map.find_user_id(user_id);
                    if (uid_info != NULL) {
                        add_hw_rule(hw_id, uid_info->get_l3_proto(), uid_info->get_l4_proto(), uid_info->get_ipv6_next_h());
                    }
                } else {
                    if (hw_id > m_max_hw_id_payload) {
                        m_max_hw_id_payload = hw_id;
                    }
                    m_hw_id_map_payload.map(hw_id, user_id);
                }
                // clear hardware counters. Just in case we have garbage from previous iteration
                rx_per_flow_t rx_cntr;
                tx_per_flow_t tx_cntr;
                rfc2544_info_t rfc2544_info;
                for (uint8_t port = 0; port < m_num_ports; port++) {
                    m_api->get_flow_stats(port, &rx_cntr, (void *)&tx_cntr, hw_id, hw_id, true, rule_type);
                }
                if (rule_type == TrexPlatformApi::IF_STAT_PAYLOAD) {
                    m_api->get_rfc2544_info(&rfc2544_info, hw_id, hw_id, true, true);
                }
            }
        }
    }

    // saving given hw_id on stream for use by tx statistics count
    if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
        m_parser_ipid->set_ip_id(m_ip_id_reserve_base + hw_id);
        m_parser_ipid->set_tos_to_cpu();
        stream->m_rx_check.m_hw_id = hw_id;
    } else {
        m_parser_pl->set_ip_id(FLOW_STAT_PAYLOAD_IP_ID);
        m_parser_pl->set_tos_to_cpu();
        // for payload rules, we use the range right after ip id rules
        stream->m_rx_check.m_hw_id = hw_id + MAX_FLOW_STATS;
    }

#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << "exit:" << __METHOD_NAME__ << " hw_id:" << hw_id << std::endl;
    stream_dump(stream);
#endif

    if (m_num_started_streams == 0) {

        send_start_stop_msg_to_rx(true); // First transmitting stream. Rx core should start reading packets;

        //also good time to zero global counters
        memset(m_rx_cant_count_err, 0, sizeof(m_rx_cant_count_err));
        memset(m_tx_cant_count_err, 0, sizeof(m_tx_cant_count_err));

#if 0
        // wait to make sure that message is acknowledged. RX core might be in deep sleep mode, and we want to
        // start transmitting packets only after it is working, otherwise, packets will get lost.
        if (m_rx_core) { // in simulation, m_rx_core will be NULL
            int count = 0;
            while (!m_rx_core->is_working()) {
                delay(1);
                count++;
                if (count == 100) {
                    throw TrexFStatEx("Critical error!! - RX core failed to start", TrexException::T_FLOW_STAT_RX_CORE_START_FAIL);
                }
            }
        }
#endif

    } else {
        // make sure rx core is working. If not, we got really confused somehow.
        if (m_rx_core)
            assert(m_rx_core->is_working());
    }
    m_num_started_streams++;
    DEBUG_PRINT("  Increased num started streams to:%d\n", m_num_started_streams);
    return 0;
}

int CFlowStatRuleMgr::add_hw_rule(uint16_t hw_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h) {
    for (int port = 0; port < m_num_ports; port++) {
        m_api->add_rx_flow_stat_rule(port, l3_proto, l4_proto, ipv6_next_h, hw_id);
    }

    return 0;
}

int CFlowStatRuleMgr::stop_stream(TrexStream * stream) {
    internal_stop_stream(stream);
    return 0;
}

int CFlowStatRuleMgr::internal_stop_stream(TrexStream * stream) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id
              << " hw id:" << stream->m_rx_check.m_hw_id << std::endl;
    stream_dump(stream);
#endif

    int ret = -1;

    if (! stream->m_rx_check.m_enabled) {
        return -1;
    }

    if (! m_api)
        throw TrexFStatEx("Called stop_stream, but no stream was added", TrexException::T_FLOW_STAT_NO_STREAMS_EXIST);

    if (stream->m_rx_check.m_hw_id >= MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        // We allow stopping while already stopped. Will not hurt us.
        return -1;
    }

    TrexPlatformApi::driver_stat_cap_e rule_type = (TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type;
    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        break;
    case TrexPlatformApi::IF_STAT_PAYLOAD:
        break;
    default:
        throw TrexFStatEx("Wrong rule_type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
        break;
    }

    stream->m_rx_check.m_hw_id = HW_ID_STOPPED;

    rx_per_flow_t rx_cntr;
    tx_per_flow_t tx_cntr;
    CFlowStatUserIdInfo *p_user_id;
    uint16_t hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id);

    if (hw_id >= MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        return -1;
    }

    if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
        p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(hw_id));
    } else {
        p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(hw_id));
    }
    if (p_user_id == NULL) {
        printf ("hw_id:%d. No mapping found. Supposed to be attached to %d\n", hw_id, stream->m_rx_check.m_pg_id);

    } else {
        ret = m_user_id_map.stop_stream(stream->m_rx_check.m_pg_id);

        if (ret == 0) {
            // If we stopped the last stream trasnimitting on this hw_id,
            // read counters one last time to make sure everything is in sync
            if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                update_counters(false, hw_id, hw_id, HW_ID_INIT, HW_ID_INIT, true);
            } else {
                update_counters(false, HW_ID_INIT, HW_ID_INIT, hw_id, hw_id, true);
            }
        }
    }
    assert(p_user_id != NULL);

    m_num_started_streams--;
    DEBUG_PRINT("  Decreased num started streams to:%d\n", m_num_started_streams);
    assert (m_num_started_streams >= 0);
    if (m_num_started_streams == 0) {
        DEBUG_PRINT("  Sending stop message to rx\n");
        send_start_stop_msg_to_rx(false); // No more transmittig streams. Rx core should get into idle loop.
    }

    return ret;
}

// return list of active pgids with their type (latency of flow_stat)

int CFlowStatRuleMgr::get_active_pgids(flow_stat_active_t_new &result) {
    flow_stat_user_id_map_it_t it;
    active_pgid one_active;
    CFlowStatUserIdInfo *user_id_info;

    for (it = m_user_id_map.begin(); it != m_user_id_map.end(); it++) {
        one_active.m_pg_id = it->first;
        user_id_info = it->second;
        one_active.m_type = user_id_info->rfc2544_support() ? PGID_LATENCY : PGID_FLOW_STAT;
        result.push_back(one_active);
    }

    return 0;
}

int CFlowStatRuleMgr::set_mode(enum flow_stat_mode_e mode) {
    if ( ! m_user_id_map.is_empty() )
        return -1;

    if (! m_api ) {
        create();
    }

    switch (mode) {
    case FLOW_STAT_MODE_PASS_ALL:
        delete m_parser_ipid;
        delete m_parser_pl;
        m_parser_ipid = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
        m_parser_pl = new CPassAllParser;
        break;
    case FLOW_STAT_MODE_NORMAL:
        delete m_parser_ipid;
        delete m_parser_pl;
        m_parser_ipid = m_api->get_flow_stat_parser();
        m_parser_pl = m_api->get_flow_stat_parser();
        break;
    default:
        return -1;

    }
    assert(m_parser_ipid);
    assert(m_parser_pl);

    m_mode = mode;

    return 0;
}

extern bool rx_should_stop;
void CFlowStatRuleMgr::send_start_stop_msg_to_rx(bool is_start) {
    TrexStatelessCpToRxMsgBase *msg;

    if (is_start) {
        static MsgReply<bool> reply;
        reply.reset();

        msg = new TrexStatelessRxEnableLatency(reply);
        m_ring_to_rx->Enqueue((CGenNode *)msg);

        /* hold until message was ack'ed - otherwise we might lose packets */
        if (m_rx_core) {
            reply.wait_for_reply();
            assert(m_rx_core->is_working());
        }

    } else {
        msg = new TrexStatelessRxDisableLatency();
        m_ring_to_rx->Enqueue((CGenNode *)msg);
    }
}

void CFlowStatRuleMgr::periodic_update() {
    uint16_t min_f = (m_max_hw_id >= 0) ? 0 : HW_ID_INIT;
    uint16_t min_l = (m_max_hw_id_payload >= 0) ? 0 : HW_ID_INIT;

    internal_periodic_update(min_f, m_max_hw_id, min_l, m_max_hw_id_payload);
}

void CFlowStatRuleMgr::internal_periodic_update(uint16_t min_f, uint16_t max_f
                                       , uint16_t min_l, uint16_t max_l) {
    if (! m_api ) {
        create();
    }

    // toggle period in rfc2544 info
    if (min_l != HW_ID_INIT) {
        m_api->get_rfc2544_info(NULL, min_l, max_l, false, true);
    }
    update_counters(true, min_f, max_f, min_l, max_l, false);
}

// read hw counters, and update internal counters
void CFlowStatRuleMgr::update_counters(bool update_rate, uint16_t min_f, uint16_t max_f
                                       , uint16_t min_l, uint16_t max_l, bool is_last) {
    hr_time_t now = os_get_hr_tick_64();
    hr_time_t freq = os_get_hr_freq();
#ifdef __DEBUG_FUNC_ENTRY__
        printf("CFlowStatRuleMgr::update_counters: time:%ld, flow(%d-%d) payload(%d-%d)\n"
               , now, min_f, max_f, min_l, max_l);
#endif
    for (uint8_t port = 0; port < m_num_ports; port++) {
        if (min_f != HW_ID_INIT) {
            m_api->get_flow_stats(port, m_rx_stats, (void *)m_tx_stats, min_f, max_f, false, TrexPlatformApi::IF_STAT_IPV4_ID);
            for (int i = min_f; i <= max_f; i++) {
                if (m_rx_stats[i - min_f].get_pkts() != 0) {
                    rx_per_flow_t rx_pkts = m_rx_stats[i - min_f];
                    CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                    if (likely(p_user_id != NULL)) {
                        p_user_id->update_rx_vals(port, rx_pkts, is_last, now, freq, update_rate);
                    } else {
                        m_rx_cant_count_err[port] += rx_pkts.get_pkts();
                        std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << rx_pkts << " rx packets, on port "
                                  << (uint16_t)port << ", because no mapping was found." << std::endl;
                    }
                }

                if (m_tx_stats[i - min_f].get_pkts() != 0) {
                    tx_per_flow_t tx_pkts = m_tx_stats[i - min_f];
                    CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                    if (likely(p_user_id != NULL)) {
                        DEBUG_PRINT("port: %d, hw_id:%d tx_pkts:%lu\n", port, i, tx_pkts.get_pkts());
                        p_user_id->update_tx_vals(port, tx_pkts, is_last, now, freq, update_rate);
                    } else {
                        m_tx_cant_count_err[port] += tx_pkts.get_pkts();;
                        std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << tx_pkts <<  " tx packets on port "
                                  << (uint16_t)port << ", because no mapping was found." << std::endl;
                    }
                }
            }
        }
        // payload rules
        if (min_l != HW_ID_INIT) {
            m_api->get_flow_stats(port, m_rx_stats_payload, (void *)m_tx_stats_payload, min_l, max_l
                                  , false, TrexPlatformApi::IF_STAT_PAYLOAD);
            for (int i = min_l; i <= max_l; i++) {
                if (m_rx_stats_payload[i - min_l].get_pkts() != 0) {
                    rx_per_flow_t rx_pkts = m_rx_stats_payload[i - min_l];
                    CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(i));
                    if (likely(p_user_id != NULL)) {
                        p_user_id->update_rx_vals(port, rx_pkts, is_last, now, freq, update_rate);
                    } else {
                        m_rx_cant_count_err[port] += rx_pkts.get_pkts();;
                        std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << rx_pkts << " rx payload packets, on port "
                                  << (uint16_t)port << ", because no mapping was found." << std::endl;
                    }
                }

                if (m_tx_stats_payload[i - min_l].get_pkts() != 0) {
                    tx_per_flow_t tx_pkts = m_tx_stats_payload[i - min_l];
                    CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(i));
                    if (likely(p_user_id != NULL)) {
                        DEBUG_PRINT("payload - port: %d, hw_id:%d tx_pkts:%lu\n", port, i, tx_pkts.get_pkts());
                        p_user_id->update_tx_vals(port, tx_pkts, is_last, now, freq, update_rate);
                    } else {
                        m_tx_cant_count_err[port] += tx_pkts.get_pkts();;
                        std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << tx_pkts <<  " tx packets on port "
                                  << (uint16_t)port << ", because no mapping was found." << std::endl;
                    }
                }
            }
        }
    }
}

// return false if no counters changed since last run. true otherwise
// s_json - flow statistics json
// l_json - latency data json
bool CFlowStatRuleMgr::dump_json(Json::Value &json, std::vector<uint32> pgids) {
    FUNC_ENTRY;
    CRxCoreErrCntrs rx_err_cntrs;
    Json::FastWriter writer;
    Json::Value s_data_section;
    Json::Value l_data_section;
    Json::Value v_data_section;
    bool send_all = true;

    if (! m_api ) {
        create();
    }

    uint16_t min_f = (m_max_hw_id >= 0) ? 0 : HW_ID_INIT;
    uint16_t min_l = (m_max_hw_id_payload >= 0) ? 0 : HW_ID_INIT;
    update_counters(false, min_f, m_max_hw_id, min_l, m_max_hw_id_payload, false);

    if (pgids.size() != 0) {
        if (pgids.size() > MAX_ALLOWED_PGID_LIST_LEN) {
            throw TrexFStatEx("Asking information for too many packet group IDs. Max allowed is "
                              + std::to_string(MAX_ALLOWED_PGID_LIST_LEN)
                              , TrexException::T_FLOW_STAT_TOO_MANY_PG_IDS_FOR_DUMP);
        }
        send_all = false;
    } else {
        if (m_user_id_map.size() > MAX_ALLOWED_PGID_LIST_LEN) {
            throw TrexFStatEx("Asking information for all packet group IDs. There are currently "
                              + std::to_string(m_user_id_map.size())
                              + " active IDs . Max allowed is "
                              + std::to_string(MAX_ALLOWED_PGID_LIST_LEN)
                              + ". Need to limit query size."
                              , TrexException::T_FLOW_STAT_TOO_MANY_PG_IDS_FOR_DUMP);
        }
    }


    m_api->get_rfc2544_info(m_rfc2544_info, 0, m_max_hw_id_payload, false, false);
    m_api->get_rx_err_cntrs(&rx_err_cntrs);

    // build json report
    // general per port data
    for (uint8_t port = 0; port < m_num_ports; port++) {
            std::string str_port = static_cast<std::ostringstream*>( &(std::ostringstream() << int(port) ) )->str();
            if (m_rx_cant_count_err[port] != 0)
                s_data_section["g"]["rx_err"][str_port] = m_rx_cant_count_err[port];
            if (m_tx_cant_count_err[port] != 0)
                s_data_section["g"]["tx_err"][str_port] = m_tx_cant_count_err[port];
    }

    // payload rules rx errors
    uint64_t tmp_cnt;
    tmp_cnt = rx_err_cntrs.get_bad_header();
    if (tmp_cnt) {
        l_data_section["g"]["bad_hdr"] = Json::Value::UInt64(tmp_cnt);
    }
    tmp_cnt = rx_err_cntrs.get_old_flow();
    if (tmp_cnt) {
        l_data_section["g"]["old_flow"] = Json::Value::UInt64(tmp_cnt);
    }

    // If given vector of pgids, create json containing only them, otherwise, use all pgids
    flow_stat_user_id_map_it_t it_all;
    std::vector<uint32_t>::iterator it_some;
    bool is_finished = false;
    if (send_all) {
        it_all = m_user_id_map.begin();
    } else {
        it_some = pgids.begin();
    }

    while (! is_finished) {
        uint32_t user_id;
        CFlowStatUserIdInfo *user_id_info;

        if (send_all) {
            if (it_all == m_user_id_map.end()) {
                break;
            }
            user_id = it_all->first;
            user_id_info = it_all->second;
        } else {
            if (it_some == pgids.end()) {
                break;
            }
            user_id = *it_some;
            user_id_info = m_user_id_map.find_user_id(user_id);
            if (user_id_info == NULL) {
                // ID does not exist
                it_some++;
                continue;
            }
        }

        std::string str_user_id = static_cast<std::ostringstream*>( &(std::ostringstream() << user_id) )->str();
        v_data_section[str_user_id] = user_id_info->get_ver_id();
        // flow stat json
        for (uint8_t port = 0; port < m_num_ports; port++) {
            std::string str_port = static_cast<std::ostringstream*>( &(std::ostringstream() << int(port) ) )->str();
            s_data_section[str_user_id]["rp"][str_port] = Json::Value::UInt64(user_id_info->get_rx_cntr(port).get_pkts());
            if (m_cap & TrexPlatformApi::IF_STAT_RX_BYTES_COUNT)
                s_data_section[str_user_id]["rb"][str_port] = Json::Value::UInt64(user_id_info->get_rx_cntr(port).get_bytes());
            s_data_section[str_user_id]["tp"][str_port] = Json::Value::UInt64(user_id_info->get_tx_cntr(port).get_pkts());
            s_data_section[str_user_id]["tb"][str_port] = Json::Value::UInt64(user_id_info->get_tx_cntr(port).get_bytes());

            // truncate floats to two digits after decimal point
            char temp_str[100];
            snprintf(temp_str, sizeof(temp_str), "%.2f", user_id_info->get_rx_bps(port));
            s_data_section[str_user_id]["rbs"][str_port] = Json::Value(atof(temp_str));
            snprintf(temp_str, sizeof(temp_str), "%.2f", user_id_info->get_rx_pps(port));
            s_data_section[str_user_id]["rps"][str_port] = Json::Value(atof(temp_str));
            snprintf(temp_str, sizeof(temp_str), "%.2f", user_id_info->get_tx_bps(port));
            s_data_section[str_user_id]["tbs"][str_port] = Json::Value(atof(temp_str));
            snprintf(temp_str, sizeof(temp_str), "%.2f", user_id_info->get_tx_pps(port));
            s_data_section[str_user_id]["tps"][str_port] = Json::Value(atof(temp_str));
        }

        // latency info json
        if (user_id_info->rfc2544_support()) {
            CFlowStatUserIdInfoPayload *user_id_info_p = (CFlowStatUserIdInfoPayload *)user_id_info;
            // payload object. Send also latency, jitter...
            Json::Value lat_hist = Json::arrayValue;
            if (user_id_info->is_hw_id()) {
                // if mapped to hw_id, take info from what we just got from rx core
                uint16_t hw_id = user_id_info->get_hw_id();
                m_rfc2544_info[hw_id].get_latency_json(lat_hist);
                user_id_info_p->set_seq_err_cnt(m_rfc2544_info[hw_id].get_seq_err_cnt());
                user_id_info_p->set_ooo_cnt(m_rfc2544_info[hw_id].get_ooo_cnt());
                user_id_info_p->set_dup_cnt(m_rfc2544_info[hw_id].get_dup_cnt());
                user_id_info_p->set_seq_err_big_cnt(m_rfc2544_info[hw_id].get_seq_err_ev_big());
                user_id_info_p->set_seq_err_low_cnt(m_rfc2544_info[hw_id].get_seq_err_ev_low());
                l_data_section[str_user_id]["lat"] = lat_hist;
                l_data_section[str_user_id]["lat"]["jit"] = m_rfc2544_info[hw_id].get_jitter_usec();
            } else {
                // Not mapped to hw_id. Get saved info.
                user_id_info_p->get_latency_json(lat_hist);
                if (lat_hist != Json::nullValue) {
                    l_data_section[str_user_id]["lat"] = lat_hist;
                    l_data_section[str_user_id]["lat"]["jit"] = user_id_info_p->get_jitter_usec();
                }
            }
            if (user_id_info_p->get_seq_err_cnt() != 0) {
                l_data_section[str_user_id]["er"]["drp"]
                    = Json::Value::UInt64(user_id_info_p->get_seq_err_cnt());
            }
            if (user_id_info_p->get_ooo_cnt() != 0) {
                l_data_section[str_user_id]["er"]["ooo"]
                    = Json::Value::UInt64(user_id_info_p->get_ooo_cnt());
            }
            if (user_id_info_p->get_dup_cnt() != 0) {
                l_data_section[str_user_id]["er"]["dup"]
                    = Json::Value::UInt64(user_id_info_p->get_dup_cnt());
            }
            if (user_id_info_p->get_seq_err_big_cnt() != 0) {
                l_data_section[str_user_id]["er"]["sth"]
                    = Json::Value::UInt64(user_id_info_p->get_seq_err_big_cnt());
            }
            if (user_id_info_p->get_seq_err_low_cnt() != 0) {
                l_data_section[str_user_id]["er"]["stl"]
                    = Json::Value::UInt64(user_id_info_p->get_seq_err_low_cnt());
            }
        }


        if (send_all) {
            it_all++;
        } else {
            it_some++;
        }
    }

    json["flow_stats"] = s_data_section;
    json["latency"] = l_data_section;
    json["ver_id"] = v_data_section;

    return true;
}
