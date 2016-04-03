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
CFlowStatUserIdInfo::CFlowStatUserIdInfo(uint8_t proto) {
    memset(m_rx_counter, 0, sizeof(m_rx_counter));
    memset(m_rx_counter_base, 0, sizeof(m_rx_counter));
    memset(m_tx_counter, 0, sizeof(m_tx_counter));
    memset(m_tx_counter_base, 0, sizeof(m_tx_counter));
    m_hw_id = UINT16_MAX;
    m_proto = proto;
    m_ref_count = 1;
    m_trans_ref_count = 0;
    m_was_sent = false;
    for (int i = 0; i < TREX_MAX_PORTS; i++) {
        m_rx_changed[i] = false;
        m_tx_changed[i] = false;
    }
}

std::ostream& operator<<(std::ostream& os, const CFlowStatUserIdInfo& cf) {
    os << "hw_id:" << cf.m_hw_id << " proto:" << (uint16_t) cf.m_proto << " ref("
       << (uint16_t) cf.m_ref_count << "," << (uint16_t) cf.m_trans_ref_count << ")";
    os << " rx count (";
    os << cf.m_rx_counter[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_rx_counter[i];
    }
    os << ")";
    os << " rx count base(";
    os << cf.m_rx_counter_base[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_rx_counter_base[i];
    }
    os << ")";

    os << " tx count (";
    os << cf.m_tx_counter[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_tx_counter[i];
    }
    os << ")";
    os << " tx count base(";
    os << cf.m_tx_counter_base[0];
    for (int i = 1; i < TREX_MAX_PORTS; i++) {
        os << "," << cf.m_tx_counter_base[i];
    }
    os << ")";

    return os;
}

int CFlowStatUserIdInfo::add_stream(uint8_t proto) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " proto:" << (uint16_t)proto << std::endl;
#endif

    if (proto != m_proto)
        throw TrexException("Can't use same pg_id for streams with different l4 protocol");

    m_ref_count++;

    return 0;
}

void CFlowStatUserIdInfo::reset_hw_id() {
    FUNC_ENTRY;

    m_hw_id = UINT16_MAX;
    // we are not attached to hw. Save packet count of session.
    // Next session will start counting from 0.
    for (int i = 0; i < TREX_MAX_PORTS; i++) {
        m_rx_counter_base[i] += m_rx_counter[i];
        memset(&m_rx_counter[i], 0, sizeof(m_rx_counter[0]));
        m_tx_counter_base[i] += m_tx_counter[i];
        memset(&m_tx_counter[i], 0, sizeof(m_tx_counter[0]));
    }
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
CFlowStatUserIdMap::add_user_id(uint32_t user_id, uint8_t proto) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " proto:" << (uint16_t)proto
              << std::endl;
#endif

    CFlowStatUserIdInfo *new_id = new CFlowStatUserIdInfo(proto);
    if (new_id != NULL) {
        std::pair<flow_stat_user_id_map_it_t, bool> ret;
        ret = m_map.insert(std::pair<uint32_t, CFlowStatUserIdInfo *>(user_id, new_id));
        if (ret.second == false) {
            printf("%s Error: Trying to add user id %d which already exist\n", __func__, user_id);
            delete new_id;
            return NULL;
        }
        return new_id;
    } else {
        return NULL;
    }
}

int CFlowStatUserIdMap::add_stream(uint32_t user_id, uint8_t proto) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << " proto:" << (uint16_t)proto
              << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        c_user_id = add_user_id(user_id, proto);
        if (! c_user_id)
            throw TrexException("Failed adding statistic counter - Failure in add_stream");
        return 0;
    } else {
        return c_user_id->add_stream(proto);
    }
}

int CFlowStatUserIdMap::del_stream(uint32_t user_id) {
#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << __METHOD_NAME__ << " user id:" << user_id << std::endl;
#endif

    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        throw TrexException("Trying to delete stream which does not exist");
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
        fprintf(stderr, "%s Error: Trying to associate hw id %d to user_id %d but it does not exist\n"
                , __func__, hw_id, user_id);
        throw TrexException("Internal error: Trying to associate non exist group id");
    }

    if (c_user_id->is_hw_id()) {
        fprintf(stderr, "%s Error: Trying to associate hw id %d to user_id %d but it is already associated to %u\n"
                , __func__, hw_id, user_id, c_user_id->get_hw_id());
        throw TrexException("Internal error: Trying to associate used packet group id to different hardware counter");
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
        fprintf(stderr, "%s Error: Trying to start stream on pg_id %d but it does not exist\n"
                , __func__, user_id);
        throw TrexException("Trying to start stream with non exist packet group id");
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
        fprintf(stderr, "%s Error: Trying to stop stream on pg_id %d but it does not exist\n"
                , __func__, user_id);
        throw TrexException("Trying to stop stream with non exist packet group id");
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

uint8_t CFlowStatUserIdMap::l4_proto(uint32_t user_id) {
    CFlowStatUserIdInfo *c_user_id;

    c_user_id = find_user_id(user_id);
    if (! c_user_id) {
        return 0;
    }

    return c_user_id->get_proto();
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
    m_num_free = MAX_FLOW_STATS;
    for (int i = 0; i < MAX_FLOW_STATS; i++) {
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
    m_num_started_streams = 0;
    m_ring_to_rx = NULL;
    m_capabilities = 0;
    m_parser = NULL;
    m_rx_core = NULL;
}

CFlowStatRuleMgr::~CFlowStatRuleMgr() {
    if (m_parser)
        delete m_parser;
}

void CFlowStatRuleMgr::create() {
    uint16_t num_counters, capabilities;
    TrexStateless *tstateless = get_stateless_obj();
    assert(tstateless);

    m_api = tstateless->get_platform_api();
    assert(m_api);
    m_api->get_interface_stat_info(0, num_counters, capabilities);
    m_api->get_port_num(m_num_ports);
    for (uint8_t port = 0; port < m_num_ports; port++) {
        assert(m_api->reset_hw_flow_stats(port) == 0);
    }
    m_ring_to_rx = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    assert(m_ring_to_rx);
    m_rx_core = get_rx_sl_core_obj();
    m_parser = m_api->get_flow_stat_parser();
    assert(m_parser);
    m_capabilities = capabilities;
}

std::ostream& operator<<(std::ostream& os, const CFlowStatRuleMgr& cf) {
    os << "Flow stat rule mgr (" << cf.m_num_ports << ") ports:" << std::endl;
    os << cf.m_hw_id_map;
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
            fprintf(stderr, "Error: %s - Compilation failed\n", __func__);
            throw TrexException("Failed parsing given packet for flow stat. Probably bad packet format.");
        } else {
            return 0;
        }
    }

    if (!parser->is_stat_supported()) {
        if (stream->m_stream_id <= 0) {
            // flow stat not needed. Do nothing.
            return 0;
        } else {
            // flow stat needed, but packet format is not supported
            fprintf(stderr, "Error: %s - Unsupported packet format for flow stat\n", __func__);
            throw TrexException("Unsupported packet format for flow stat on given interface type");
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

int CFlowStatRuleMgr::add_stream(TrexStream * stream) {
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

    //??? put back    assert(stream->m_rx_check.m_hw_id == HW_ID_INIT);

    uint16_t rule_type = TrexPlatformApi::IF_STAT_IPV4_ID; // In the future need to get it from the stream;

    if ((m_capabilities & rule_type) == 0) {
        fprintf(stderr, "Error: %s - rule type not supported by interface\n", __func__);
        throw TrexException("Interface does not support given rule type");
    }

    // compile_stream throws exception if something goes wrong
    compile_stream(stream, m_parser);

    uint8_t l4_proto;
    if (m_parser->get_l4_proto(l4_proto) < 0) {
        fprintf(stderr, "Error: %s failed finding l4 proto\n", __func__);
        throw TrexException("Failed determining l4 proto for packet");
    }

    // throws exception if there is error
    m_user_id_map.add_stream(stream->m_rx_check.m_pg_id, l4_proto);

    stream->m_rx_check.m_hw_id = HW_ID_FREE;
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
        throw TrexException("Called del_stream, but no stream was added");

    // we got del_stream command for a stream which has valid hw_id.
    // Probably someone forgot to call stop
    if(stream->m_rx_check.m_hw_id < MAX_FLOW_STATS) {
        stop_stream(stream);
    }

    // calling del for same stream twice, or for a stream which was never "added"
    if(stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        return 0;
    }
    // Throws exception in case of error
    m_user_id_map.del_stream(stream->m_rx_check.m_pg_id);
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
        } catch (TrexException) {
            // If no statistics needed, and we can't parse the stream, that's OK.
            return 0;
        }

        uint16_t ip_id;
        if (m_parser->get_ip_id(ip_id) < 0) {
            return 0; // if we could not find the ip id, no need to fix
        }
        // verify no reserved IP_ID used, and change if needed
        if (ip_id >= IP_ID_RESERVE_BASE) {
            if (m_parser->set_ip_id(ip_id & 0xefff) < 0) {
                throw TrexException("Stream IP ID in reserved range. Failed changing it");
            }
        }
        return 0;
    }

    // from here, we know the stream need rx stat

    // Starting a stream which was never added
    if (stream->m_rx_check.m_hw_id == HW_ID_INIT) {
        add_stream(stream);
    }

    if (stream->m_rx_check.m_hw_id < MAX_FLOW_STATS) {
        throw TrexException("Starting a stream which was already started");
    }

    uint16_t rule_type = TrexPlatformApi::IF_STAT_IPV4_ID; // In the future, need to get it from the stream;

    if ((m_capabilities & rule_type) == 0) {
        fprintf(stderr, "Error: %s - rule type not supported by interface\n", __func__);
        throw TrexException("Interface does not support given rule type");
    }

    // compile_stream throws exception if something goes wrong
    if ((ret = compile_stream(stream, m_parser)) < 0)
        return ret;

    uint16_t hw_id;

    if (m_user_id_map.is_started(stream->m_rx_check.m_pg_id)) {
        m_user_id_map.start_stream(stream->m_rx_check.m_pg_id); // just increase ref count;
        hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id); // can't fail if we got here
    } else {
        hw_id = m_hw_id_map.find_free_hw_id();
        if (hw_id == HW_ID_FREE) {
            printf("Error: %s failed finding free hw_id\n", __func__);
            throw TrexException("Failed allocating statistic counter. Probably all are used.");
        } else {
            if (hw_id > m_max_hw_id) {
                m_max_hw_id = hw_id;
            }
            uint32_t user_id = stream->m_rx_check.m_pg_id;
            m_user_id_map.start_stream(user_id, hw_id);
            m_hw_id_map.map(hw_id, user_id);
            add_hw_rule(hw_id, m_user_id_map.l4_proto(user_id));
            // clear hardware counters. Just in case we have garbage from previous iteration
            rx_per_flow_t rx_counter;
            tx_per_flow_t tx_counter;
            for (uint8_t port = 0; port < m_num_ports; port++) {
                m_api->get_flow_stats(port, &rx_counter, (void *)&tx_counter, hw_id, hw_id, true);
            }
        }
    }

    m_parser->set_ip_id(IP_ID_RESERVE_BASE + hw_id);

    // saving given hw_id on stream for use by tx statistics count
    stream->m_rx_check.m_hw_id = hw_id;

#ifdef __DEBUG_FUNC_ENTRY__
    std::cout << "exit:" << __METHOD_NAME__ << " hw_id:" << hw_id << std::endl;
    stream_dump(stream);
#endif

    if (m_num_started_streams == 0) {
        send_start_stop_msg_to_rx(true); // First transmitting stream. Rx core should start reading packets;

        // wait to make sure that message is acknowledged. RX core might be in deep sleep mode, and we want to
        // start transmitting packets only after it is working, otherwise, packets will get lost.
        if (m_rx_core) { // in simulation, m_rx_core will be NULL
            int count = 0;
            while (!m_rx_core->is_working()) {
                delay(1);
                count++;
                if (count == 100) {
                    throw TrexException("Critical error!! - RX core failed to start");
                }
            }
        }
    } else {
        // make sure rx core is working. If not, we got really confused somehow.
        assert(m_rx_core->is_working());
    }
    m_num_started_streams++;
    return 0;
}

int CFlowStatRuleMgr::add_hw_rule(uint16_t hw_id, uint8_t proto) {
    for (int port = 0; port < m_num_ports; port++) {
        m_api->add_rx_flow_stat_rule(port, FLOW_STAT_RULE_TYPE_IPV4_ID, proto, hw_id);
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
        throw TrexException("Called stop_stream, but no stream was added");

    if (stream->m_rx_check.m_hw_id >= MAX_FLOW_STATS) {
        // We allow stopping while already stopped. Will not hurt us.
        return 0;
    }

    stream->m_rx_check.m_hw_id = HW_ID_FREE;

    if (m_user_id_map.stop_stream(stream->m_rx_check.m_pg_id) == 0) {
        // last stream associated with the entry stopped transmittig.
        // remove user_id <--> hw_id mapping
        uint8_t proto = m_user_id_map.l4_proto(stream->m_rx_check.m_pg_id);
        uint16_t hw_id = m_user_id_map.get_hw_id(stream->m_rx_check.m_pg_id);
        if (hw_id >= MAX_FLOW_STATS) {
            fprintf(stderr, "Error: %s got wrong hw_id %d from unmap\n", __func__, hw_id);
            throw TrexException("Internal error in stop_stream. Got bad hw_id");
        } else {
            // update counters, and reset before unmapping
            CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(hw_id));
            assert(p_user_id != NULL);
            rx_per_flow_t rx_counter;
            tx_per_flow_t tx_counter;
            for (uint8_t port = 0; port < m_num_ports; port++) {
                m_api->del_rx_flow_stat_rule(port, FLOW_STAT_RULE_TYPE_IPV4_ID, proto, hw_id);
                m_api->get_flow_stats(port, &rx_counter, (void *)&tx_counter, hw_id, hw_id, true);
                // when stopping, always send counters for stopped stream one last time
                p_user_id->set_rx_counter(port, rx_counter);
                p_user_id->set_need_to_send_rx(port);
                p_user_id->set_tx_counter(port, tx_counter);
                p_user_id->set_need_to_send_tx(port);
            }
            m_user_id_map.unmap(stream->m_rx_check.m_pg_id);
            m_hw_id_map.unmap(hw_id);
        }
    }
    m_num_started_streams--;
    assert (m_num_started_streams >= 0);
    if (m_num_started_streams == 0) {
        send_start_stop_msg_to_rx(false); // No more transmittig streams. Rx core shoulde get into idle loop.
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

extern bool rx_should_stop;
void CFlowStatRuleMgr::send_start_stop_msg_to_rx(bool is_start) {
    TrexStatelessCpToRxMsgBase *msg;

    if (is_start) {
        msg = new TrexStatelessRxStartMsg();
    } else {
        msg = new TrexStatelessRxStopMsg();
    }
    m_ring_to_rx->Enqueue((CGenNode *)msg);
}

// return false if no counters changed since last run. true otherwise
bool CFlowStatRuleMgr::dump_json(std::string & json, bool baseline) {
    rx_per_flow_t rx_stats[MAX_FLOW_STATS];
    tx_per_flow_t tx_stats[MAX_FLOW_STATS];
    Json::FastWriter writer;
    Json::Value root;

    root["name"] = "flow_stats";
    root["type"] = 0;

    if (baseline) {
        root["baseline"] = true;
    }

    Json::Value &data_section = root["data"];
    data_section["ts"]["value"] = Json::Value::UInt64(os_get_hr_tick_64());
    data_section["ts"]["freq"] = Json::Value::UInt64(os_get_hr_freq());

    if (m_user_id_map.is_empty()) {
        json = writer.write(root);
        return true;
    }

    // read hw counters, and update
    for (uint8_t port = 0; port < m_num_ports; port++) {
        m_api->get_flow_stats(port, rx_stats, (void *)tx_stats, 0, m_max_hw_id, false);
        for (int i = 0; i <= m_max_hw_id; i++) {
            if (rx_stats[i].get_pkts() != 0) {
                rx_per_flow_t rx_pkts = rx_stats[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_rx_counter(port) != rx_pkts) {
                        p_user_id->set_rx_counter(port, rx_pkts);
                        p_user_id->set_need_to_send_rx(port);
                    }
                } else {
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << rx_pkts << " rx packets, on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
            if (tx_stats[i].get_pkts() != 0) {
                tx_per_flow_t tx_pkts = tx_stats[i];
                CFlowStatUserIdInfo *p_user_id = m_user_id_map.find_user_id(m_hw_id_map.get_user_id(i));
                if (likely(p_user_id != NULL)) {
                    if (p_user_id->get_tx_counter(port) != tx_pkts) {
                        p_user_id->set_tx_counter(port, tx_pkts);
                        p_user_id->set_need_to_send_tx(port);
                    }
                } else {
                    std::cerr <<  __METHOD_NAME__ << i << ":Could not count " << tx_pkts <<  " tx packets on port "
                              << (uint16_t)port << ", because no mapping was found." << std::endl;
                }
            }
        }
    }

    // build json report
    flow_stat_user_id_map_it_t it;
    for (it = m_user_id_map.begin(); it != m_user_id_map.end(); it++) {
        bool send_empty = true;
        CFlowStatUserIdInfo *user_id_info = it->second;
        uint32_t user_id = it->first;
        std::string str_user_id = static_cast<std::ostringstream*>( &(std::ostringstream()
                                                                      << user_id) )->str();
        if (! user_id_info->was_sent()) {
            data_section[str_user_id]["first_time"] = true;
            user_id_info->set_was_sent(true);
            send_empty = false;
        }
        for (uint8_t port = 0; port < m_num_ports; port++) {
            std::string str_port = static_cast<std::ostringstream*>( &(std::ostringstream() << int(port) ) )->str();
            if (user_id_info->need_to_send_rx(port) || baseline) {
                user_id_info->set_no_need_to_send_rx(port);
                data_section[str_user_id]["rx_pkts"][str_port] = Json::Value::UInt64(user_id_info->get_rx_counter(port).get_pkts());
                data_section[str_user_id]["rx_bytes"][str_port] = Json::Value::UInt64(user_id_info->get_rx_counter(port).get_bytes());
                send_empty = false;
            }
            if (user_id_info->need_to_send_tx(port) || baseline) {
                user_id_info->set_no_need_to_send_tx(port);
                data_section[str_user_id]["tx_pkts"][str_port] = Json::Value::UInt64(user_id_info->get_tx_counter(port).get_pkts());
                data_section[str_user_id]["tx_bytes"][str_port] = Json::Value::UInt64(user_id_info->get_tx_counter(port).get_bytes());
                send_empty = false;
            }
        }
        if (send_empty) {
            data_section[str_user_id] = Json::objectValue;
        }
    }

    json = writer.write(root);
    // We always want to publish, even only the timestamp.
    return true;
}
