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

#include "trex_tpg_stats.h"
#include "stl/trex_stl_fs.h"
#include <pthread.h>
#include "common/Network/Packet/VLANHeader.h"

/*******************************************************************
 CTPGTagCntr
*******************************************************************/
CTPGTagCntr::CTPGTagCntr() {
    static_assert(!std::is_polymorphic<CTPGTagCntr>::value, "CTPGTagCntr can't be polymorphic!");
    memset(this, 0, sizeof(CTPGTagCntr));
}

void CTPGTagCntr::update_cntrs(uint32_t rcv_seq, uint32_t pkt_len) {
    if (rcv_seq > m_exp_seq) {
        _update_cntr_seq_too_big(rcv_seq);
        m_exp_seq = rcv_seq + 1;
    } else if (rcv_seq == m_exp_seq) {
        m_exp_seq = rcv_seq + 1;
    } else {
        _update_cntr_seq_too_small(rcv_seq);
    }
    m_pkts++;
    m_bytes += (pkt_len + 4); // +4 for Ethernet FCS
}

void CTPGTagCntr::_update_cntr_seq_too_big(uint32_t rcv_seq) {
    m_seq_err += rcv_seq - m_exp_seq;
    m_seq_err_too_big++;
}

void CTPGTagCntr::_update_cntr_seq_too_small(uint32_t rcv_seq) {
    if (rcv_seq == m_exp_seq - 1) {
        // We always calculate: m_exp_seq = rcv_seq + 1
        // Hence if this is the case, this is a duplicate.
        m_dup++;
    } else {
        // Out of Order
        m_ooo++;
        if (m_seq_err > 0) {
            m_seq_err--;
        }
    }
    m_seq_err_too_small++;
}

void CTPGTagCntr::dump_json(Json::Value& stats) {
    stats["pkts"] = m_pkts;
    stats["bytes"] = m_bytes;
    stats["seq_err"] = m_seq_err;
    stats["seq_err_too_big"] = m_seq_err_too_big;
    stats["seq_err_too_small"] = m_seq_err_too_small;
    stats["ooo"] = m_ooo;
    stats["dup"] = m_dup;
}

void CTPGTagCntr::set_cntrs(uint64_t pkts,
                            uint64_t bytes,
                            uint64_t seq_err,
                            uint64_t seq_err_too_big,
                            uint64_t seq_err_too_small,
                            uint64_t dup,
                            uint64_t ooo) {

        m_pkts = pkts;
        m_bytes = bytes;
        m_seq_err = seq_err;
        m_seq_err_too_big = seq_err_too_big;
        m_seq_err_too_small = seq_err_too_small;
        m_dup = dup;
        m_ooo = ooo;
}

bool operator==(const CTPGTagCntr& lhs, const CTPGTagCntr& rhs) {
    return (
        lhs.m_pkts == rhs.m_pkts &&
        lhs.m_bytes == rhs.m_bytes &&
        lhs.m_seq_err == rhs.m_seq_err &&
        lhs.m_seq_err_too_big == rhs.m_seq_err_too_big &&
        lhs.m_seq_err_too_small == rhs.m_seq_err_too_small &&
        lhs.m_ooo == rhs.m_ooo &&
        lhs.m_dup == rhs.m_dup
    );
}

bool operator!=(const CTPGTagCntr& lhs, const CTPGTagCntr& rhs) {
    return !(lhs==rhs);
}

std::ostream& operator<<(std::ostream& os, const CTPGTagCntr& tag) {
    os << "\n{" << std::endl;
    os << "Pkts: " << tag.m_pkts << std::endl;
    os << "Bytes: " << tag.m_bytes << std::endl;
    os << "Seq Err: " << tag.m_seq_err << std::endl;
    os << "Seq Err Too Big: " << tag.m_seq_err_too_big << std::endl;
    os << "Seq Err Too Small: " << tag.m_seq_err_too_small << std::endl;
    os << "OOO: " << tag.m_ooo << std::endl;
    os << "Dup: " << tag.m_dup << std::endl;
    os << "}" << std::endl;
    return os;
}

/**************************************
 * RxTPGPerPort
 *************************************/
RxTPGPerPort::RxTPGPerPort(uint8_t port_id, uint32_t num_tpgids, PacketGroupTagMgr* tag_mgr, CTPGTagCntr* cntrs)
                          : m_port_id(port_id), m_num_tpgids(num_tpgids), m_tag_mgr(tag_mgr), m_cntrs(cntrs) {

    m_num_tags = m_tag_mgr->get_num_tags();
}

void RxTPGPerPort::handle_pkt(const rte_mbuf_t* m) {
    // If we got here the feature is set.

    uint16_t header_size = sizeof(struct tpg_payload_header);
    uint8_t tmp_buf[header_size];
    memset(tmp_buf, 0, header_size);
    struct tpg_payload_header* tpg_header = (tpg_payload_header *)utl_rte_pktmbuf_get_last_bytes(m, header_size, tmp_buf);

    if (tpg_header->magic != TPG_PAYLOAD_MAGIC) {
        // Not a TPG Packet, return.
        return;
    }

    // This is a TPG packet
    uint8_t* pkt_ptr = rte_pktmbuf_mtod(m, uint8_t*);

    CFlowStatParser parser(CFlowStatParser::FLOW_STAT_PARSER_MODE_SW); // Support Dot1Q, QinQ.
    parser.set_vxlan_skip(false); // Ignore VxLAN for now.
    // Intentionally ignore parser return value, since parser doesn't support non IP packets.
    parser.parse(pkt_ptr, m->pkt_len);

    uint16_t tag_id = 0;
    bool tag_exists = false;
    uint8_t vlan_offset = parser.get_vlan_offset();
    if (vlan_offset != 0) {
        // Packet has Dot1Q or QinQ.
        pkt_ptr += vlan_offset;
        VLANHeader* vlan = (VLANHeader*)pkt_ptr;
        if (vlan->getNextProtocolHostOrder() == EthernetHeader::Protocol::VLAN) {
            // QinQ
            pkt_ptr += sizeof(VLANHeader);
            VLANHeader* secondVlan = (VLANHeader*)pkt_ptr;
            tag_exists = m_tag_mgr->qinq_tag_exists(vlan->getTagID(), secondVlan->getTagID());
            if (tag_exists) {
                tag_id = m_tag_mgr->get_qinq_tag(vlan->getTagID(), secondVlan->getTagID());
            }
        } else {
            // Dot1Q
            tag_exists = m_tag_mgr->dot1q_tag_exists(vlan->getTagID());
            if (tag_exists) {
                tag_id = m_tag_mgr->get_dot1q_tag(vlan->getTagID());
            }
        }
    }

    update_cntrs(tpg_header->tpgid, tpg_header->seq, m->pkt_len, tag_id, tag_exists);
}

void RxTPGPerPort::update_cntrs(uint32_t tpgid, uint32_t rcv_seq, uint32_t pkt_len, uint16_t tag_id, bool tag_exists) {
    if (tpgid >= m_num_tpgids) {
        // Invalid tpgid, ignore.
        return;
    }
    CTPGTagCntr* tag_cntr = nullptr;
    if (tag_exists) {
        tag_cntr = get_tag_cntr(tpgid, tag_id);
    } else {
        /**
         * Counters based on sequencing are irrelevant in case the tag is not known.
         * For example, we can receive seq = 3 in Vlan 3, and seq = 5 in Vlan 5.
         * We will count this as seq_err even though it isn't necessarily.
         */
        tag_cntr = get_unknown_tag_cntr(tpgid);
    }

    tag_cntr->update_cntrs(rcv_seq, pkt_len);
}

CTPGTagCntr* RxTPGPerPort::get_tag_cntr(uint32_t tpgid, uint16_t tag) {
    if (tpgid >= m_num_tpgids || tag >= m_num_tags) {
        return nullptr;
    }
    return m_cntrs + tpgid * (m_num_tags + 1) + tag;
}

CTPGTagCntr* RxTPGPerPort::get_unknown_tag_cntr(uint32_t tpgid) {
    // Unkown tag is last at each tpgid
    if (tpgid >= m_num_tpgids) {
        return nullptr;
    }
    return m_cntrs +  tpgid * (m_num_tags + 1) + m_num_tags;
}

void RxTPGPerPort::get_tpg_stats(Json::Value& stats, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag) {
    Json::Value& tpgid_stats = stats[std::to_string(tpgid)];

    CTPGTagCntr* ref_cntr = get_tag_cntr(tpgid, min_tag);
    uint16_t i = min_tag + 1;
    while (i < max_tag) {
        if (*ref_cntr != *get_tag_cntr(tpgid, i)) {
            break;
        }
        i++;
    }

    std::string key = "";
    if (i != min_tag + 1) {
        key = std::to_string(min_tag) + "-" + std::to_string(i-1); // i-1 to make it inclusive
    } else {
        key = std::to_string(min_tag);
    }

    Json::Value& key_stats = tpgid_stats[key];
    ref_cntr->dump_json(key_stats);

    if (unknown_tag) {
        Json::Value& unknown_tag_stats = tpgid_stats["unknown_tag"];
        get_unknown_tag_cntr(tpgid)->dump_json(unknown_tag_stats);
    }
}


/**************************************
 * TPGRxCtx
 *************************************/

TPGRxCtx::TPGRxCtx(const std::vector<uint8_t>& rx_ports,
                   const uint32_t num_tpgids,
                   const std::string& username,
                   PacketGroupTagMgr* tag_mgr) :
                   m_rx_ports(rx_ports), m_num_tpgids(num_tpgids), m_username(username), m_cntrs(nullptr), m_state(TPGRxState::DISABLED), m_thread(nullptr) {

    m_tag_mgr = new PacketGroupTagMgr(tag_mgr); // Clone this for locality on NUMA
}

TPGRxCtx::~TPGRxCtx() {
    delete m_thread;    // Delete detached thread
    delete m_tag_mgr;   // Remove the clone
    free(m_cntrs);      // Free the counters if needed
}

void TPGRxCtx::allocate() {
    spawn_thread("TPG Rx Counter Allocator", &TPGRxCtx::_allocate);
}

void TPGRxCtx::deallocate() {
    spawn_thread("TPG Rx Counter Deallocator", &TPGRxCtx::_deallocate);
}

void TPGRxCtx::spawn_thread(const std::string& name, std::function<void(TPGRxCtx*)> func) {
    if (m_thread) {
        throw TrexException("Context is already allocating/deallocating.");
    }

    m_thread = new std::thread(func, this); // This will throw if it fails
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(sched_getcpu(), &cpuset);
    auto thread_handle = m_thread->native_handle();
    int rc = pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset); // Run with same affinity as master
    if (rc != 0) {
        // Return Code is non zero
        throw TrexException("Error setting affinity for allocating thread");
    }
    pthread_setname_np(thread_handle, name.c_str());
    m_thread->detach();
}

void TPGRxCtx::destroy_thread() {
    delete m_thread;
    m_thread = nullptr;
}

void TPGRxCtx::_allocate() {
    uint8_t num_ports = m_rx_ports.size();
    uint16_t num_tags = m_tag_mgr->get_num_tags() + 1; // + 1 for unkown tags
    uint64_t num_cntrs = num_ports * m_num_tpgids * num_tags;
    /**
     * NOTE: Use calloc to distribute the workload based on COW (Copy on Write).
     * NOTE: The Rx thread validates that the counters were allocated correctly.
     **/
    static_assert(!std::is_polymorphic<CTPGTagCntr>::value,  "CTPGTagCntr can't be polymorphic!");
    m_cntrs = (CTPGTagCntr*)calloc(num_cntrs, sizeof(CTPGTagCntr));

    // Update state safely
    set_state(TPGRxState::AWAITING_ENABLE);
}

void TPGRxCtx::_deallocate() {
    free(m_cntrs);
    m_cntrs = nullptr;

    // Update state safely
    set_state(TPGRxState::AWAITING_DISABLE);
}

CTPGTagCntr* TPGRxCtx::get_port_cntr(uint8_t port_id) {
    uint16_t num_tags = m_tag_mgr->get_num_tags() + 1; // + 1 for unknown tag
    for (int i = 0; i < m_rx_ports.size(); i++) {
        if (m_rx_ports[i] == port_id) {
            // Found it
            uint64_t offset = i * m_num_tpgids * num_tags;
            return m_cntrs + offset;
        }
    }
    // Couldn't find port
    return nullptr;
}

void TPGRxCtx::set_state(TPGRxState state) {
    std::lock_guard<std::mutex> l(m_state_mutex);
    m_state = state;
} // auto unlock (lock_guard, RAII)

TPGRxState TPGRxCtx::get_state() {
    std::lock_guard<std::mutex> l(m_state_mutex);
    return m_state;
} // auto unlock (lock_guard, RAII)
