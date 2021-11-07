/*
 Bes Dollma
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2021-2021 Cisco Systems, Inc.

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
    if (dot1q_tag_exists(vlan)) {
        return false;
    }
    Dot1QTag* dot1q_tag = new Dot1QTag(vlan, tag);
    m_dot1q_map[vlan] = dot1q_tag;
    m_num_tags++;
    return true;
}

bool PacketGroupTagMgr::add_qinq_tag(uint16_t inner_vlan, uint16_t outter_vlan, uint16_t tag) {
    if (qinq_tag_exists(inner_vlan, outter_vlan)) {
        return false;
    }
    QinQTag* qinq_tag = new QinQTag(inner_vlan, outter_vlan, tag);
    m_qinq_map[get_qinq_key(inner_vlan, outter_vlan)] = qinq_tag;
    m_num_tags++;
    return true;
}

/**************************************
 * Tagged Packet Group Control Plane Manager
 *************************************/
TPGCpMgr::TPGCpMgr(const std::vector<uint8_t>& ports, uint32_t num_tpgids) : m_ports(ports), m_num_tpgids(num_tpgids) {
    m_tag_mgr = new PacketGroupTagMgr();
}

TPGCpMgr::~TPGCpMgr() {
    delete m_tag_mgr;
}


/**************************************
 * Tagged Packet Group Stream Manager
 *************************************/

// Instantiate the static variable */
TPGStreamMgr* TPGStreamMgr::m_instance = nullptr;

TPGStreamMgr::TPGStreamMgr() : m_num_tpgids(0) {
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

    // Let's check if TPG is enabled
    TPGState tpg_state = get_stateless_obj()->get_tpg_state();
    if (tpg_state != TPGState::ENABLED) {
        throw TrexFStatEx("Trying to add a TPG stream but TPG isn't enabled", TrexException::T_FLOW_STAT_TPG_NOT_ENABLED);
    }

    if (stream_exists(stream)) {
        TPGStreamState state = m_stream_states[stream_key(stream)];
        if (state == TPGStreamAdded || state == TPGStreamStarted) {
            throw TrexFStatEx("Trying to add a stream that is already started/added.", TrexException::T_FLOW_STAT_ALREADY_EXIST);
        }
    }

    /** Verify TPGID validity **/
    uint32_t tpgid = stream->m_rx_check.m_pg_id;

    if (tpgid >= m_num_tpgids) {
        throw TrexFStatEx("Invalid tpgid " + std::to_string(tpgid) + ". The maximal tpgid allowed is " + std::to_string(m_num_tpgids-1), 
                          TrexException::T_FLOW_STAT_INVALID_TPGID);
    }

    const bool tpgid_in_use = m_active_tpgids.find(tpgid) != m_active_tpgids.end();
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
    m_active_tpgids.insert(tpgid);

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
    m_active_tpgids.erase(tpgid);

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
 * Tagged Packet Group Data Plane Manager
 *************************************/
TPGDpMgr::TPGDpMgr(uint32_t num_tpgids) : m_num_tpgids(num_tpgids) {
    m_seq_array = new uint32_t[m_num_tpgids];
    for (int i = 0; i < m_num_tpgids; i++) {
        m_seq_array[i] = 0;
    }
}

TPGDpMgr::~TPGDpMgr() {
    delete[] m_seq_array;
}

uint32_t TPGDpMgr::get_seq(uint32_t tpgid) {
    if (tpgid >= m_num_tpgids) {
        return 0;
    }
    return m_seq_array[tpgid];
}

void TPGDpMgr::inc_seq(uint32_t tpgid) {
    if (tpgid >= m_num_tpgids) {
        return;
    }
    m_seq_array[tpgid]++;
}