/*
 Bes Dollma
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2021-2022 Cisco Systems, Inc.

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

#include "trex_stl_tpg.h"
#include "flow_stat_parser.h"
#include "trex_stl.h"
#include "trex_stl_port.h"

// Pure Abstract Destructor needs implementation.
BasePacketGroupTag::~BasePacketGroupTag() {}

/**************************************
 * Packet Group Tag Manager
 *************************************/
PacketGroupTagMgr::PacketGroupTagMgr(const PacketGroupTagMgr* tag_mgr) {
    // Copy Dot1Q Map
    for (auto& iter_pair : tag_mgr->m_dot1q_map) {
        uint16_t vlan = iter_pair.second->get_vlan();
        uint16_t tag = iter_pair.second->get_tag();
        m_dot1q_map[iter_pair.first] = new Dot1QTag(vlan, tag);
    }

    // Copy QinQ Map
    for (auto& iter_pair : tag_mgr->m_qinq_map) {
        uint16_t inner_vlan = iter_pair.second->get_inner_vlan();
        uint16_t outter_vlan = iter_pair.second->get_outter_vlan();
        uint16_t tag = iter_pair.second->get_tag();
        m_qinq_map[iter_pair.first] = new QinQTag(inner_vlan, outter_vlan, tag);
    }

    m_num_tags = tag_mgr->m_num_tags;
}

PacketGroupTagMgr::~PacketGroupTagMgr() {
    for (auto& itr: m_dot1q_map) {
        delete itr.second;
    }

    for (auto& itr: m_qinq_map) {
        delete itr.second;
    }
}

uint16_t PacketGroupTagMgr::get_dot1q_tag(uint16_t vlan) {
    if (!dot1q_tag_exists(vlan)) {
        std::string err_msg = "Vlan " + std::to_string(vlan) + " not in Dot1Q Map";
        throw TrexException(err_msg);
    }
    return m_dot1q_map[vlan]->get_tag();
}

uint16_t PacketGroupTagMgr::get_qinq_tag(uint16_t inner_vlan, uint16_t outter_vlan) {
    if (!qinq_tag_exists(inner_vlan, outter_vlan)) {
        std::string err_msg = "QinQ " + std::to_string(inner_vlan) + ", " + std::to_string(outter_vlan) + " not in QinQ Map";
        throw TrexException(err_msg);
    }
    return m_qinq_map[get_qinq_key(inner_vlan, outter_vlan)]->get_tag();
}

bool PacketGroupTagMgr::add_dot1q_tag(uint16_t vlan, uint16_t tag) {
    if (dot1q_tag_exists(vlan) || !valid_vlan(vlan)) {
        return false;
    }
    Dot1QTag* dot1q_tag = new Dot1QTag(vlan, tag);
    m_dot1q_map[vlan] = dot1q_tag;
    m_num_tags++;
    return true;
}

bool PacketGroupTagMgr::add_qinq_tag(uint16_t inner_vlan, uint16_t outter_vlan, uint16_t tag) {
    if (qinq_tag_exists(inner_vlan, outter_vlan) || !valid_vlan(inner_vlan) || !valid_vlan(outter_vlan)) {
        return false;
    }
    QinQTag* qinq_tag = new QinQTag(inner_vlan, outter_vlan, tag);
    m_qinq_map[get_qinq_key(inner_vlan, outter_vlan)] = qinq_tag;
    m_num_tags++;
    return true;
}

/**************************************
 * TPG State
 *************************************/

TPGState::TPGState(TPGState::Value val) : m_val(val) {}

bool TPGState::isError() {
    return m_val == Value::RX_ALLOC_FAILED || m_val == Value::DP_ALLOC_FAILED;
}

int TPGState::getValue() {
    return static_cast<int>(m_val);
}

bool operator==(const TPGState& lhs, const TPGState& rhs) {
    return lhs.m_val == rhs.m_val;
}

bool operator!=(const TPGState& lhs, const TPGState& rhs) {
    return !(lhs==rhs);
}

/**************************************
 * Tagged Packet Group Control Plane Manager
 *************************************/
TPGCpCtx::TPGCpCtx(const std::vector<uint8_t>& acquired_ports,
                   const std::vector<uint8_t>& rx_ports,
                   const std::unordered_map<uint8_t, bool>& core_map,
                   const uint32_t num_tpgids,
                   const std::string& username) :
                   m_acquired_ports(acquired_ports), m_rx_ports(rx_ports), m_cores_map(core_map), m_num_tpgids(num_tpgids), m_username(username), m_tpg_state(TPGState::DISABLED) {
    m_tag_mgr = new PacketGroupTagMgr();
}

TPGCpCtx::~TPGCpCtx() {
    delete m_tag_mgr;
}

TPGState TPGCpCtx::handle_rx_state_update(TPGStateUpdate rx_state) {
    if (!is_awaiting_rx()) {
        return m_tpg_state;
    }

    // Awaiting ENABLE OR DISABLE
    if (m_tpg_state == TPGState::ENABLED_CP) {
        // Awaiting Enable
        switch (rx_state) {
            case TPGStateUpdate::RX_ENABLED:
                // RX Enabled Successfully
                m_tpg_state = TPGState::ENABLED_CP_RX;
                break;
            case TPGStateUpdate::RX_ALLOC_FAIL:
                // Allocation failed
                m_tpg_state = TPGState::RX_ALLOC_FAILED;
                break;
            default:
                break;
        }
    } else {
        // Awaiting Disable - Rx should return NO_EXIST
        assert(m_tpg_state == TPGState::DISABLED_DP);
        if (rx_state == TPGStateUpdate::RX_NO_EXIST) {
            m_tpg_state = TPGState::DISABLED_DP_RX;
        }
    }
    return m_tpg_state;
}

void TPGCpCtx::add_dp_mgr_ptr(uint8_t port_id, TPGDpMgrPerSide* dp_mgr) {
    m_dp_tpg_mgr[port_id] = dp_mgr;
}

void TPGCpCtx::get_tpg_tx_stats(Json::Value& stats, uint8_t port_id, uint32_t tpgid) {
    if (m_dp_tpg_mgr.find(port_id) != m_dp_tpg_mgr.end()) {
        Json::Value& port_stats = stats[std::to_string(port_id)];
        auto dp_mgr = m_dp_tpg_mgr[port_id];
        if (dp_mgr) {
            dp_mgr->get_tpg_tx_stats(port_stats, tpgid);
        }
    }
}

/**************************************
 * Tagged Packet Group Stream Manager
 *************************************/

// Instantiate the static variable */
TPGStreamMgr* TPGStreamMgr::m_instance = nullptr;

TPGStreamMgr::TPGStreamMgr() {
    m_parser = new CFlowStatParser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW);
    m_software_mode = !CGlobalInfo::m_dpdk_mode.get_mode()->is_hardware_filter_needed();
}

void TPGStreamMgr::compile_stream(const TrexStream * stream) {

    CFlowStatParser_err_t ret = m_parser->parse(stream->m_pkt.binary, stream->m_pkt.len);

    // if we could not parse the packet.
    if (ret != FSTAT_PARSER_E_OK) {
        throw TrexFStatEx(m_parser->get_error_str(ret), TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
    }

    if (!stream->m_rx_check.m_vxlan_skip) {
        return;
    }

    uint16_t vxlan_skip = m_parser->get_vxlan_payload_offset(stream->m_pkt.binary, stream->m_pkt.len);

    ret = m_parser->parse(stream->m_pkt.binary + vxlan_skip, stream->m_pkt.len - vxlan_skip);

    if (ret != FSTAT_PARSER_E_OK) {
        throw TrexFStatEx(m_parser->get_error_str(ret), TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
    }

}

bool TPGStreamMgr::stream_exists(const TrexStream* stream) {
    return m_stream_states.find(stream_key(stream)) != m_stream_states.end();
}

void TPGStreamMgr::add_stream(TrexStream* stream) {

    if (!stream->is_tpg_stream()) {
        return;
    }

    // We are in stateless mode, let's check that we are in software mode
    if (!m_software_mode) {
        throw TrexFStatEx("TPG is supported only in software mode!", TrexException::T_FLOW_STAT_BAD_RULE_TYPE_FOR_IF);
    }

    uint8_t port_id = stream->m_port_id;

    TrexStatelessPort* stream_port = get_stateless_obj()->get_port_by_id(port_id);
    TPGCpCtx* tpg_ctx = stream_port->get_tpg_ctx();

    // Let's check if TPG is enabled
    if (tpg_ctx == nullptr || tpg_ctx->get_tpg_state() != TPGState::ENABLED) {
        throw TrexFStatEx("Trying to add a TPG stream but TPG isn't enabled in this port.", TrexException::T_FLOW_STAT_TPG_NOT_ENABLED);
    }

    if (stream_exists(stream)) {
        TPGStreamState state = m_stream_states[stream_key(stream)];
        if (state == TPGStreamAdded || state == TPGStreamStarted) {
            throw TrexFStatEx("Trying to add a stream that is already started/added.", TrexException::T_FLOW_STAT_ALREADY_EXIST);
        }
    }

    /** Verify TPGID validity **/
    uint32_t tpgid = stream->m_rx_check.m_pg_id;
    uint32_t num_tpgids = tpg_ctx->get_num_tpgids();

    if (tpgid >= num_tpgids) {
        throw TrexFStatEx("Invalid tpgid " + std::to_string(tpgid) + ". The maximal tpgid allowed for this port is " + std::to_string(num_tpgids-1),
                          TrexException::T_FLOW_STAT_INVALID_TPGID);
    }

    const bool tpgid_in_use = m_active_tpgids.find(tpgid_key(stream->m_port_id, tpgid)) != m_active_tpgids.end();
    if (tpgid_in_use) {
        throw TrexFStatEx("Tagged Packet Group ID " + std::to_string(tpgid) + " already in use. Can't use the same id for two streams.", 
                          TrexException::T_FLOW_STAT_DUP_PG_ID);
    }

    uint16_t payload_header_size = sizeof(struct tpg_payload_header);
    uint16_t payload_len;

    // compile_stream throws exception if something goes wrong
    compile_stream(stream);

    /* Verify the size of the payload is enough to install the TPG header */
    TrexStreamPktLenData* pkt_len_data = stream->get_pkt_size();
    if (m_parser->get_payload_len(stream->m_pkt.binary, stream->m_pkt.len, payload_len) < 0) {
        throw TrexFStatEx("Failed getting payload len", TrexException::T_FLOW_STAT_BAD_PKT_FORMAT);
    }

    if (payload_len < payload_header_size) {
        throw TrexFStatEx("Need at least " + std::to_string(payload_header_size)
                          + " payload bytes for TPG Payload Header. Packet only has " + std::to_string(payload_len) + " bytes"
                          , TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
    }

    if (pkt_len_data->m_min_pkt_len != pkt_len_data->m_max_pkt_len) {
        // We have field engine which changes packets size
        uint16_t min_pkt_len = stream->m_pkt.len - payload_len + payload_header_size;
        if (min_pkt_len < 60 + payload_header_size) {
            // We have first mbuf of 60, and then we need the payload data to be contiguous.
            min_pkt_len = 60 + payload_header_size;
        }
        if (pkt_len_data->m_min_pkt_len < min_pkt_len) {
            throw TrexFStatEx("You specified field engine with minimum packet size of " + std::to_string(pkt_len_data->m_min_pkt_len) + " bytes."
                            + " For TPG together with random packet size, minimum packet size should be at least "
                            + std::to_string(min_pkt_len)
                            , TrexException::T_FLOW_STAT_PAYLOAD_TOO_SHORT);
        }
    }

    // Add the TPGID to set of active tpgids
    m_active_tpgids.insert(tpgid_key(port_id, tpgid));

    // Keep stream state
    m_stream_states[stream_key(stream)] = TPGStreamAdded;
}

void TPGStreamMgr::del_stream(TrexStream* stream) {
    if (!stream->is_tpg_stream()) {
        return;
    }

    if (!stream_exists(stream)) {
        throw TrexFStatEx("Trying to delete TPG stream that was not added with tpgid " + std::to_string(stream->m_rx_check.m_pg_id)
                          , TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    stop_stream(stream); // Stop Stream, this will not harm us even if it is already stopped.

    m_stream_states.erase(stream_key(stream));

    /** Remove the stream from set of active tpgids **/
    uint32_t tpgid = stream->m_rx_check.m_pg_id;
    m_active_tpgids.erase(tpgid_key(stream->m_port_id, tpgid));

}

void TPGStreamMgr::start_stream(TrexStream* stream) {

    if (!stream->is_tpg_stream()) {
        return;
    }

    if (!stream_exists(stream)) {
        add_stream(stream);
    }

    m_stream_states[stream_key(stream)] = TPGStreamStarted;
}

void TPGStreamMgr::stop_stream(TrexStream* stream) {

    if (!stream->is_tpg_stream()) {
        return;
    }

    if (!stream_exists(stream)) {
        throw TrexFStatEx("Trying to stop TPG stream that was not added with tpgid " + std::to_string(stream->m_rx_check.m_pg_id)
                          , TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    m_stream_states[stream_key(stream)] = TPGStreamStopped;

    /**
    NOTE: Even when removing the last stream, we don't notify the RX to move to idle.
    The Rx core will move to idle only when the TPG feature is disabled.
    **/
}

void TPGStreamMgr::reset_stream(TrexStream* stream) {
    if (!stream->is_tpg_stream()) {
        return;
    }

    if (!stream_exists(stream)) {
        throw TrexFStatEx("Trying to reset TPG stream that was not added with tpgid " + std::to_string(stream->m_rx_check.m_pg_id)
                          , TrexException::T_FLOW_STAT_NON_EXIST_ID);
    }

    m_stream_states[stream_key(stream)] = TPGStreamAdded;
}

void TPGStreamMgr::copy_state(TrexStream* from, TrexStream* to) {
    // We haven't instilled a state in the TrexStream.
}


/**************************************
 * Tagged Packet Group Tx Counters
 * per group (tpgid).
 *************************************/
TPGTxGroupCounters::TPGTxGroupCounters() {
    static_assert(!std::is_polymorphic<TPGTxGroupCounters>::value, "TPGTxGroupCounters can't be polymorphic!");
    memset(this, 0, sizeof(TPGTxGroupCounters));
}

void TPGTxGroupCounters::update_cntr(uint64_t pkts, uint64_t bytes) {
    m_pkts += pkts;
    m_bytes += bytes;
}

void TPGTxGroupCounters::dump_json(Json::Value& stats) {
    stats["pkts"] = m_pkts;
    stats["bytes"] = m_bytes;
}

bool operator==(const TPGTxGroupCounters& lhs, const TPGTxGroupCounters& rhs) {
    return (lhs.m_pkts == rhs.m_pkts && lhs.m_bytes == rhs.m_bytes);
}

bool operator!=(const TPGTxGroupCounters& lhs, const TPGTxGroupCounters& rhs) {
    return !(lhs==rhs);
}

std::ostream& operator<<(std::ostream& os, const TPGTxGroupCounters& tag) {
    os << "\n{" << std::endl;
    os << "Pkts: " << tag.m_pkts << std::endl;
    os << "Bytes: " << tag.m_bytes << std::endl;
    os << "}" << std::endl;
    return os;
}

void TPGTxGroupCounters::set_cntrs(uint64_t pkts, uint64_t bytes) {
    m_pkts = pkts;
    m_bytes = bytes;
}

/**************************************
 * Tagged Packet Group Data Plane
 * Manager Per Side
 *************************************/
TPGDpMgrPerSide::TPGDpMgrPerSide(uint32_t num_tpgids) : m_num_tpgids(num_tpgids) {}

TPGDpMgrPerSide::~TPGDpMgrPerSide() {
    free(m_seq_array);
    free(m_cntrs);
}

bool TPGDpMgrPerSide::allocate() {
    static_assert(!std::is_polymorphic<TPGTxGroupCounters>::value, "TPGTxGroupCounters can't be polymorphic!");
    m_seq_array = (uint32_t*)calloc(m_num_tpgids, sizeof(uint32_t));
    m_cntrs = (TPGTxGroupCounters*)calloc(m_num_tpgids, sizeof(TPGTxGroupCounters));
    return (m_seq_array != nullptr && m_cntrs != nullptr);
}

uint32_t TPGDpMgrPerSide::get_seq(uint32_t tpgid) {
    assert(m_seq_array);
    if (tpgid >= m_num_tpgids) {
        return 0;
    }
    return m_seq_array[tpgid];
}

void TPGDpMgrPerSide::inc_seq(uint32_t tpgid) {
    assert(m_seq_array);
    if (tpgid >= m_num_tpgids) {
        return;
    }
    m_seq_array[tpgid]++;
}

void TPGDpMgrPerSide::update_tx_cntrs(uint32_t tpgid, uint64_t pkts, uint64_t bytes) {
    assert(m_cntrs);
    if (tpgid >= m_num_tpgids) {
        return;
    }
    m_cntrs[tpgid].update_cntr(pkts, bytes);
}

void TPGDpMgrPerSide::get_tpg_tx_stats(Json::Value& stats, uint32_t tpgid) {
    if (tpgid >= m_num_tpgids || m_cntrs == nullptr) {
        return;
    }
    Json::Value& tpgid_stats = stats[std::to_string(tpgid)];
    m_cntrs[tpgid].dump_json(tpgid_stats);
}