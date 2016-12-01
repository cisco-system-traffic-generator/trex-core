/*
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

/*
Important classes in this file:
CFlowStatUserIdInfo - Information about one packet group id
CFlowStatUserIdMap - Mapping between packet group id (chosen by user) and hardware counter id
CFlowStatHwIdMap - Mapping between hardware id and packet group id
CFlowStatRuleMgr - API to users of the file

General idea of operation:
For each stream needing flow statistics, the user provides packet group id (pg_id). Few streams can have the same pg_id.
We maintain reference count.
When doing start_stream, for the first stream in pg_id, hw_id is associated with the pg_id, and relevant hardware rules are
inserted (on supported hardware). When stopping all streams with the pg_id, the hw_id <--> pg_id mapping is removed, hw_id is
returned to the free hw_id pool, and hardware rules are removed. Counters for the pg_id are kept.
If starting streams again, new hw_id will be assigned, and counters will continue from where they stopped. Only When deleting
all streams using certain pg_id, infromation about this pg_id will be freed.

For each stream we keep state in the m_rx_check.m_hw_id field. Since we keep reference count for certain structs, we want to
protect from illegal operations, like starting stream while it is already starting, stopping when it is stopped...
State machine is:
stream_init: HW_ID_INIT
stream_add: HW_ID_FREE
stream_start: legal hw_id (range is 0..MAX_FLOW_STATS)
stream_stop: HW_ID_FREE
stream_del: HW_ID_INIT
 */
#include <sstream>
#include <string>
#include <iostream>
#include <assert.h>
#include <os_time.h>
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
static const uint8_t PAYLOAD_RULE_PROTO = 255;
const uint32_t FLOW_STAT_PAYLOAD_IP_ID = IP_ID_RESERVE_BASE + MAX_FLOW_STATS;

inline std::string methodName(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return prettyFunction.substr(begin,end) + "()";
}

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
    memset(m_rx_cntr_base, 0, sizeof(m_rx_cntr));
    memset(m_tx_cntr, 0, sizeof(m_tx_cntr));
    memset(m_tx_cntr_base, 0, sizeof(m_tx_cntr));
    m_hw_id = UINT16_MAX;
    m_l3_proto = l3_proto;
    m_l4_proto = l4_proto;
    m_ipv6_next_h = ipv6_next_h;
    m_ref_count = 1;
    m_trans_ref_count = 0;
    m_was_sent = false;
    for (int i = 0; i < TREX_MAX_PORTS; i++) {
        m_rx_changed[i] = false;
        m_tx_changed[i] = false;
    }
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
    os << ")";
    os << " rx count base(";
    os << cf.m_rx_cntr_base[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_rx_cntr_base[i];
    }
    os << ")";

    os << " tx count (";
    os << cf.m_tx_cntr[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_tx_cntr[i];
    }
    os << ")";
    os << " tx count base(";
    os << cf.m_tx_cntr_base[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_tx_cntr_base[i];
    }
    os << ")";

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

    m_hw_id = UINT16_MAX;
    // we are not attached to hw. Save packet count of session.
    // Next session will start counting from 0.
    for (int i = 0; i < TREX_MAX_PORTS; i++) {
        m_rx_cntr_base[i] += m_rx_cntr[i];
        memset(&m_rx_cntr[i], 0, sizeof(m_rx_cntr[0]));
        m_tx_cntr_base[i] += m_tx_cntr[i];
        memset(&m_tx_cntr[i], 0, sizeof(m_tx_cntr[0]));
    }
}

/************** class CFlowStatUserIdInfoPayload ***************/
void CFlowStatUserIdInfoPayload::add_stream(uint8_t proto) {
    throw TrexFStatEx("For payload rules: Can't have two streams with same pg_id, or same stream on more than one port"
                      , TrexException::T_FLOW_STAT_DUP_PG_ID);
}

void CFlowStatUserIdInfoPayload::reset_hw_id() {
    CFlowStatUserIdInfo::reset_hw_id();

    m_seq_err_base += m_rfc2544_info.m_seq_err;
    m_out_of_order_base += m_rfc2544_info.m_out_of_order;
    m_dup_base += m_rfc2544_info.m_dup;
    m_seq_err_ev_big_base += m_rfc2544_info.m_seq_err_ev_big;
    m_seq_err_ev_low_base += m_rfc2544_info.m_seq_err_ev_low;
    m_rfc2544_info.m_seq_err = 0;
    m_rfc2544_info.m_out_of_order = 0;
    m_rfc2544_info.m_dup = 0;
    m_rfc2544_info.m_seq_err_ev_big = 0;
    m_rfc2544_info.m_seq_err_ev_low = 0;
}

/************** class CFlowStatUserIdMap ***************/
CFlowStatUserIdMap::CFlowStatUserIdMap() {

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
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " proto:" << (uint16_t)proto
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
    } else {
        c_user_id->add_stream(l4_proto);
    }
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

    if (c_user_id->del_stream() == 0) {
        // ref count of this entry became 0. can release this entry.
        m_map.erase(user_id);
        delete c_user_id;
    }

    return 0;
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
        return UINT16_MAX;
    }
    uint16_t old_hw_id = c_user_id->get_hw_id();
    c_user_id->reset_hw_id();

    return old_hw_id;
}

/************** class CFlowStatHwIdMap ***************/
CFlowStatHwIdMap::CFlowStatHwIdMap() {
    m_map = NULL; // must call create in order to work with the class
    m_num_free = 0; // to make coverity happy, init this here too.
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
}

std::ostream& operator<<(std::ostream& os, const CFlowStatHwIdMap& cf) {
    int count = 0;

    os << "HW id map:\n";
    os << "  num free:" << cf.m_num_free << std::endl;
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
        if (cf.m_map[i] != 0) {
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
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
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
    m_parser = NULL;
    m_rx_core = NULL;
    m_hw_id_map.create(MAX_FLOW_STATS);
    m_hw_id_map_payload.create(MAX_FLOW_STATS_PAYLOAD);
    memset(m_rx_cant_count_err, 0, sizeof(m_rx_cant_count_err));
    memset(m_tx_cant_count_err, 0, sizeof(m_tx_cant_count_err));
    m_num_ports = 0; // need to call create to init
}

CFlowStatRuleMgr::~CFlowStatRuleMgr() {
    delete m_parser;
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
    uint16_t num_counters, cap;
    TrexStateless *tstateless = get_stateless_obj();
    assert(tstateless);

    m_api = tstateless->get_platform_api();
    assert(m_api);
    m_api->get_interface_stat_info(0, num_counters, cap);
    m_api->get_port_num(m_num_ports); // This initialize m_num_ports
    for (uint8_t port = 0; port < m_num_ports; port++) {
        assert(m_api->reset_hw_flow_stats(port) == 0);
    }
    m_ring_to_rx = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    assert(m_ring_to_rx);
    m_rx_core = get_rx_sl_core_obj();
    m_parser = m_api->get_flow_stat_parser();
    assert(m_parser);
    m_cap = cap;
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

    if (parser->parse(stream->m_pkt.binary, stream->m_pkt.len) != 0) {
        // if we could not parse the packet, but no stat count needed, it is probably OK.
        if (stream->m_rx_check.m_enabled) {
            throw TrexFStatEx("Failed parsing given packet for flow stat. Please consult the manual for supported packet types for flow stat."
                              , TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
        } else {
            return 0;
        }
    }

    if (!parser->is_stat_supported()) {
        if (! stream->m_rx_check.m_enabled) {
            // flow stat not needed. Do nothing.
            return 0;
        } else {
            // flow stat needed, but packet format is not supported
            throw TrexFStatEx("Unsupported packet format for flow stat on given interface type"
                              , TrexException::T_FLOW_STAT_UNSUPP_PKT_FORMAT);
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
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << std::endl;
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

    // compile_stream throws exception if something goes wrong
    compile_stream(stream, m_parser);

    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        uint16_t l3_proto;

        if (m_mode == FLOW_STAT_MODE_PASS_ALL) {
            throw TrexFStatEx("Can not add flow stat stream in 'receive all' mode", TrexException::T_FLOW_STAT_BAD_RULE_TYPE_FOR_MODE);
        }

        if (m_parser->get_l3_proto(l3_proto) < 0) {
            throw TrexFStatEx("Failed determining l3 proto for packet", TrexException::T_FLOW_STAT_FAILED_FIND_L3);
        }
        uint8_t l4_proto;
        if (m_parser->get_l4_proto(l4_proto) < 0) {
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
        if (m_parser->get_payload_len(stream->m_pkt.binary, stream->m_pkt.len, payload_len) < 0) {
            throw TrexFStatEx("Failed getting payload len", TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
        }
        if (payload_len < sizeof(struct flow_stat_payload_header)) {
            throw TrexFStatEx("Need at least " + std::to_string(sizeof(struct latency_header))
                              + " payload bytes for payload rules. Packet only has " + std::to_string(payload_len) + " bytes"
                              , TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
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

int CFlowStatRuleMgr::del_stream(TrexStream * stream) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << std::endl;
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

    // we got del_stream command for a stream which has valid hw_id.
    // Probably someone forgot to call stop
    if(stream->m_rx_check.m_hw_id < MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        stop_stream(stream);
    }

    // calling del for same stream twice, or for a stream which was never "added"
    if(stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        return 0;
    }
    m_user_id_map.del_stream(stream->m_rx_check.m_pg_id); // Throws exception in case of error
    stream->m_rx_check.m_hw_id = HW_ID_INIT;

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
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << std::endl;
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
            compile_stream(stream, m_parser);
        } catch (TrexFStatEx) {
            // If no statistics needed, and we can't parse the stream, that's OK.
            return 0;
        }

        uint32_t ip_id;
        if (m_parser->get_ip_id(ip_id) < 0) {
            return 0; // if we could not find the ip id, no need to fix
        }
        // verify no reserved IP_ID used, and change if needed
        if (ip_id >= IP_ID_RESERVE_BASE) {
            if (m_parser->set_ip_id(ip_id & 0xefff) < 0) {
                throw TrexFStatEx("Stream IP ID in reserved range. Failed changing it"
                                  , TrexException::T_FLOW_STAT_FAILED_CHANGE_IP_ID);
            }
        }
        return 0;
    }

    // from here, we know the stream need rx stat

    // Starting a stream which was never added
    if (stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        add_stream(stream);
    }

    if (stream->m_rx_check.m_hw_id < MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        throw TrexFStatEx("Starting a stream which was already started"
                          , TrexException::T_FLOW_STAT_ALREADY_STARTED);
    }

    TrexPlatformApi::driver_stat_cap_e rule_type = (TrexPlatformApi::driver_stat_cap_e)stream->m_rx_check.m_rule_type;

    if ((m_cap & rule_type) == 0) {
        throw TrexFStatEx("Interface does not support given rule type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE_FOR_IF);
    }

    // compile_stream throws exception if something goes wrong
    if ((ret = compile_stream(stream, m_parser)) < 0)
        return ret;

    uint16_t hw_id;

    switch(rule_type) {
    case TrexPlatformApi::IF_STAT_IPV4_ID:
        break;
    case TrexPlatformApi::IF_STAT_PAYLOAD:
        break;
    default:
        throw TrexFStatEx("Wrong rule_type", TrexException::T_FLOW_STAT_BAD_RULE_TYPE);
        break;
    }

    if (m_user_id_map.is_started(stream->m_rx_check.m_pg_id)) {
        m_user_id_map.start_stream(stream->m_rx_check.m_pg_id); // just increase ref count;
        hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id); // can't fail if we got here
    } else {
        if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
            hw_id = m_hw_id_map.find_free_hw_id();
        } else {
            hw_id = m_hw_id_map_payload.find_free_hw_id();
        }
        if (hw_id == HW_ID_FREE) {
            throw TrexFStatEx("Failed allocating statistic counter. Probably all are used for this rule type."
                              , TrexException::T_FLOW_STAT_NO_FREE_HW_ID);
        } else {
            uint32_t user_id = stream->m_rx_check.m_pg_id;
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
                m_api->get_rfc2544_info(&rfc2544_info, hw_id, hw_id, true);
            }
        }
    }

    // saving given hw_id on stream for use by tx statistics count
    if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
        m_parser->set_ip_id(IP_ID_RESERVE_BASE + hw_id);
        stream->m_rx_check.m_hw_id = hw_id;
    } else {
        m_parser->set_ip_id(FLOW_STAT_PAYLOAD_IP_ID);
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
    return 0;
}

int CFlowStatRuleMgr::add_hw_rule(uint16_t hw_id, uint16_t l3_proto, uint8_t l4_proto, uint8_t ipv6_next_h) {
    for (int port = 0; port < m_num_ports; port++) {
        m_api->add_rx_flow_stat_rule(port, l3_proto, l4_proto, ipv6_next_h, hw_id);
    }

    return 0;
}

int CFlowStatRuleMgr::stop_stream(TrexStream * stream) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << stream->m_rx_check.m_pg_id << std::endl;
    stream_dump(stream);
#endif
    if (! stream->m_rx_check.m_enabled) {
        return 0;
    }

    if (! m_api)
        throw TrexFStatEx("Called stop_stream, but no stream was added", TrexException::T_FLOW_STAT_NO_STREAMS_EXIST);

    if (stream->m_rx_check.m_hw_id >= MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD) {
        // We allow stopping while already stopped. Will not hurt us.
        return 0;
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

    stream->m_rx_check.m_hw_id = HW_ID_FREE;

    if (m_user_id_map.stop_stream(stream->m_rx_check.m_pg_id) == 0) {
        // last stream associated with the entry stopped transmittig.
        // remove user_id <--> hw_id mapping
        uint16_t hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id);
        if (hw_id >= MAX_FLOW_STATS) {
            throw TrexFStatEx("Internal error in stop_stream. Got bad hw_id" + std::to_string(hw_id)
                              , TrexException::T_FLOW_STAT_BAD_HW_ID);
        } else {
            CFlowStatUserIdInfo *p_user_id;
            // update counters, and reset before unmapping
            if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(hw_id));
            } else {
                p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(hw_id));
            }
            assert(p_user_id != NULL);
            rx_per_flow_t rx_cntr;
            tx_per_flow_t tx_cntr;
            rfc2544_info_t rfc2544_info;
            for (uint8_t port = 0; port < m_num_ports; port++) {
                if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                    m_api->del_rx_flow_stat_rule(port, p_user_id->get_l3_proto(), p_user_id->get_l4_proto()
                                                 , p_user_id->get_ipv6_next_h(), hw_id);
                }
                m_api->get_flow_stats(port, &rx_cntr, (void *)&tx_cntr, hw_id, hw_id, true, rule_type);
                // when stopping, always send counters for stopped stream one last time
                p_user_id->set_rx_cntr(port, rx_cntr);
                p_user_id->set_need_to_send_rx(port);
                p_user_id->set_tx_cntr(port, tx_cntr);
                p_user_id->set_need_to_send_tx(port);
            }

            if (rule_type == TrexPlatformApi::IF_STAT_IPV4_ID) {
                m_hw_id_map.unmap(hw_id);
            } else {
                CFlowStatUserIdInfoPayload *p_user_id_p = (CFlowStatUserIdInfoPayload *)p_user_id;
                Json::Value json;
                m_api->get_rfc2544_info(&rfc2544_info, hw_id, hw_id, true);
                p_user_id_p->set_jitter(rfc2544_info.get_jitter());
                rfc2544_info.get_latency_json(json);
                p_user_id_p->set_latency_json(json);
                p_user_id_p->set_seq_err_cnt(rfc2544_info.get_seq_err_cnt());
                p_user_id_p->set_ooo_cnt(rfc2544_info.get_ooo_cnt());
                p_user_id_p->set_dup_cnt(rfc2544_info.get_dup_cnt());
                p_user_id_p->set_seq_err_big_cnt(rfc2544_info.get_seq_err_ev_big());
                p_user_id_p->set_seq_err_low_cnt(rfc2544_info.get_seq_err_ev_low());
                m_hw_id_map_payload.unmap(hw_id);
            }
            m_user_id_map.unmap(stream->m_rx_check.m_pg_id);
        }
    }
    m_num_started_streams--;
    assert (m_num_started_streams >= 0);
    if (m_num_started_streams == 0) {
        send_start_stop_msg_to_rx(false); // No more transmittig streams. Rx core should get into idle loop.
    }
    return 0;
}

int CFlowStatRuleMgr::get_active_pgids(flow_stat_active_t &result) {
    flow_stat_user_id_map_it_t it;

    for (it = m_user_id_map.begin(); it != m_user_id_map.end(); it++) {
        result.insert(it->first);
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
        delete m_parser;
        m_parser = new CPassAllParser;
        break;
    case FLOW_STAT_MODE_NORMAL:
        delete m_parser;
        m_parser = m_api->get_flow_stat_parser();
        assert(m_parser);
        break;
    default:
        return -1;

    }

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

// return false if no counters changed since last run. true otherwise
// s_json - flow statistics json
// l_json - latency data json
// baseline - If true, send flow statistics fields even if they were not changed since last run
bool CFlowStatRuleMgr::dump_json(std::string & s_json, std::string & l_json, bool baseline) {
    rx_per_flow_t rx_stats[MAX_FLOW_STATS];
    rx_per_flow_t rx_stats_payload[MAX_FLOW_STATS];
    tx_per_flow_t tx_stats[MAX_FLOW_STATS];
    tx_per_flow_t tx_stats_payload[MAX_FLOW_STATS_PAYLOAD];
    rfc2544_info_t rfc2544_info[MAX_FLOW_STATS_PAYLOAD];
    CRxCoreErrCntrs rx_err_cntrs;
    Json::FastWriter writer;
    Json::Value s_root;
    Json::Value l_root;

    s_root["name"] = "flow_stats";
    s_root["type"] = 0;
    l_root["name"] = "latency_stats";
    l_root["type"] = 0;

    if (baseline) {
        s_root["baseline"] = true;
        l_root["baseline"] = true;
    }

    Json::Value &s_data_section = s_root["data"];
    Json::Value &l_data_section = l_root["data"];
    s_data_section["ts"]["value"] = Json::Value::UInt64(os_get_hr_tick_64());
    s_data_section["ts"]["freq"] = Json::Value::UInt64(os_get_hr_freq());

    if (m_user_id_map.is_empty()) {
        s_json = writer.write(s_root);
        l_json = writer.write(l_root);
        return true;
    }

    m_api->get_rfc2544_info(rfc2544_info, 0, m_max_hw_id_payload, false);
    m_api->get_rx_err_cntrs(&rx_err_cntrs);

    // read hw counters, and update
    for (uint8_t port = 0; port < m_num_ports; port++) {
        m_api->get_flow_stats(port, rx_stats, (void *)tx_stats, 0, m_max_hw_id, false, TrexPlatformApi::IF_STAT_IPV4_ID);
        for (int i = 0; i <= m_max_hw_id; i++) {
            if (rx_stats[i].get_pkts() != 0) {
                rx_per_flow_t rx_pkts = rx_stats[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_rx_cntr(port) != rx_pkts) {
                        p_user_id->set_rx_cntr(port, rx_pkts);
                        p_user_id->set_need_to_send_rx(port);
                    }
                } else {
                    m_rx_cant_count_err[port] += rx_pkts.get_pkts();
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << rx_pkts << " rx packets, on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
            if (tx_stats[i].get_pkts() != 0) {
                tx_per_flow_t tx_pkts = tx_stats[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_tx_cntr(port) != tx_pkts) {
                        p_user_id->set_tx_cntr(port, tx_pkts);
                        p_user_id->set_need_to_send_tx(port);
                    }
                } else {
                    m_tx_cant_count_err[port] += tx_pkts.get_pkts();;
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << tx_pkts <<  " tx packets on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
        }
        // payload rules
        m_api->get_flow_stats(port, rx_stats_payload, (void *)tx_stats_payload, 0, m_max_hw_id_payload
                              , false, TrexPlatformApi::IF_STAT_PAYLOAD);
        for (int i = 0; i <= m_max_hw_id_payload; i++) {
            if (rx_stats_payload[i].get_pkts() != 0) {
                rx_per_flow_t rx_pkts = rx_stats_payload[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_rx_cntr(port) != rx_pkts) {
                        p_user_id->set_rx_cntr(port, rx_pkts);
                        p_user_id->set_need_to_send_rx(port);
                    }
                } else {
                    m_rx_cant_count_err[port] += rx_pkts.get_pkts();;
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << rx_pkts << " rx payload packets, on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
            if (tx_stats_payload[i].get_pkts() != 0) {
                tx_per_flow_t tx_pkts = tx_stats_payload[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map_payload.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_tx_cntr(port) != tx_pkts) {
                        p_user_id->set_tx_cntr(port, tx_pkts);
                        p_user_id->set_need_to_send_tx(port);
                    }
                } else {
                    m_tx_cant_count_err[port] += tx_pkts.get_pkts();;
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << tx_pkts <<  " tx packets on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
        }
    }

    // build json report
    // general per port data
    for (uint8_t port = 0; port < m_num_ports; port++) {
            std::string str_port = static_cast<std::ostringstream*>( &(std::ostringstream() << int(port) ) )->str();
            if ((m_rx_cant_count_err[port] != 0) || baseline)
                s_data_section["global"]["rx_err"][str_port] = m_rx_cant_count_err[port];
            if ((m_tx_cant_count_err[port] != 0) || baseline)
                s_data_section["global"]["tx_err"][str_port] = m_tx_cant_count_err[port];
    }

    // payload rules rx errors
    uint64_t tmp_cnt;
    tmp_cnt = rx_err_cntrs.get_bad_header();
    if (tmp_cnt || baseline) {
        l_data_section["global"]["bad_hdr"] = Json::Value::UInt64(tmp_cnt);
    }
    tmp_cnt = rx_err_cntrs.get_old_flow();
    if (tmp_cnt || baseline) {
        l_data_section["global"]["old_flow"] = Json::Value::UInt64(tmp_cnt);
    }

    flow_stat_user_id_map_it_t it;
    for (it = m_user_id_map.begin(); it != m_user_id_map.end(); it++) {
        bool send_empty = true;
        CFlowStatUserIdInfo *user_id_info = it->second;
        uint32_t user_id = it->first;
        std::string str_user_id = static_cast<std::ostringstream*>( &(std::ostringstream() << user_id) )->str();
        if (! user_id_info->was_sent()) {
            s_data_section[str_user_id]["first_time"] = true;
            user_id_info->set_was_sent(true);
            send_empty = false;
        }
        // flow stat json
        for (uint8_t port = 0; port < m_num_ports; port++) {
            std::string str_port = static_cast<std::ostringstream*>( &(std::ostringstream() << int(port) ) )->str();
            if (user_id_info->need_to_send_rx(port) || baseline) {
                user_id_info->set_no_need_to_send_rx(port);
                s_data_section[str_user_id]["rx_pkts"][str_port] = Json::Value::UInt64(user_id_info->get_rx_cntr(port).get_pkts());
                if (m_cap & TrexPlatformApi::IF_STAT_RX_BYTES_COUNT)
                    s_data_section[str_user_id]["rx_bytes"][str_port] = Json::Value::UInt64(user_id_info->get_rx_cntr(port).get_bytes());
                send_empty = false;
            }
            if (user_id_info->need_to_send_tx(port) || baseline) {
                user_id_info->set_no_need_to_send_tx(port);
                s_data_section[str_user_id]["tx_pkts"][str_port] = Json::Value::UInt64(user_id_info->get_tx_cntr(port).get_pkts());
                s_data_section[str_user_id]["tx_bytes"][str_port] = Json::Value::UInt64(user_id_info->get_tx_cntr(port).get_bytes());
                send_empty = false;
            }
        }
        if (send_empty) {
            s_data_section[str_user_id] = Json::objectValue;
        }

        // latency info json
        if (user_id_info->rfc2544_support()) {
            CFlowStatUserIdInfoPayload *user_id_info_p = (CFlowStatUserIdInfoPayload *)user_id_info;
            // payload object. Send also latency, jitter...
            Json::Value lat_hist = Json::arrayValue;
            if (user_id_info->is_hw_id()) {
                // if mapped to hw_id, take info from what we just got from rx core
                uint16_t hw_id = user_id_info->get_hw_id();
                rfc2544_info[hw_id].get_latency_json(lat_hist);
                user_id_info_p->set_seq_err_cnt(rfc2544_info[hw_id].get_seq_err_cnt());
                user_id_info_p->set_ooo_cnt(rfc2544_info[hw_id].get_ooo_cnt());
                user_id_info_p->set_dup_cnt(rfc2544_info[hw_id].get_dup_cnt());
                user_id_info_p->set_seq_err_big_cnt(rfc2544_info[hw_id].get_seq_err_ev_big());
                user_id_info_p->set_seq_err_low_cnt(rfc2544_info[hw_id].get_seq_err_ev_low());
                l_data_section[str_user_id]["latency"] = lat_hist;
                l_data_section[str_user_id]["latency"]["jitter"] = rfc2544_info[hw_id].get_jitter_usec();
            } else {
                // Not mapped to hw_id. Get saved info.
                user_id_info_p->get_latency_json(lat_hist);
                if (lat_hist != Json::nullValue) {
                    l_data_section[str_user_id]["latency"] = lat_hist;
                    l_data_section[str_user_id]["latency"]["jitter"] = user_id_info_p->get_jitter_usec();
                }
            }
            l_data_section[str_user_id]["err_cntrs"]["dropped"]
                = Json::Value::UInt64(user_id_info_p->get_seq_err_cnt());
            l_data_section[str_user_id]["err_cntrs"]["out_of_order"]
                = Json::Value::UInt64(user_id_info_p->get_ooo_cnt());
            l_data_section[str_user_id]["err_cntrs"]["dup"]
                = Json::Value::UInt64(user_id_info_p->get_dup_cnt());
            l_data_section[str_user_id]["err_cntrs"]["seq_too_high"]
                = Json::Value::UInt64(user_id_info_p->get_seq_err_big_cnt());
            l_data_section[str_user_id]["err_cntrs"]["seq_too_low"]
                = Json::Value::UInt64(user_id_info_p->get_seq_err_low_cnt());
        }
    }

    s_json = writer.write(s_root);
    l_json = writer.write(l_root);
    // We always want to publish, even only the timestamp.
    return true;
}
