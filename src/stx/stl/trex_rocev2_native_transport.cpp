/*
Copyright (c) 2026 Cisco Systems, Inc.

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

#include "trex_rocev2_native_transport.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <string.h>

namespace TrexRoceV2 {
namespace Native {

namespace {

static const uint8_t AETH_ACK = 0x1f;
static const uint8_t AETH_RNR_BASE = 0x20;
static const uint8_t AETH_SEQUENCE_NAK = 0x60;
static const uint8_t AETH_INVALID_REQUEST = 0x61;
static const uint8_t AETH_REMOTE_ACCESS = 0x62;
static const uint8_t AETH_REMOTE_OPERATION = 0x63;

static const uint64_t RNR_TIMEOUT_NS[32] = {
    655360000ULL, 10000ULL, 20000ULL, 30000ULL,
    40000ULL, 60000ULL, 80000ULL, 120000ULL,
    160000ULL, 240000ULL, 320000ULL, 480000ULL,
    640000ULL, 960000ULL, 1280000ULL, 1920000ULL,
    2560000ULL, 3840000ULL, 5120000ULL, 7680000ULL,
    10240000ULL, 15360000ULL, 20480000ULL, 30720000ULL,
    40960000ULL, 61440000ULL, 81920000ULL, 122880000ULL,
    163840000ULL, 245760000ULL, 327680000ULL, 491520000ULL
};

bool add_storage_reservation(uint64_t count,
                             uint64_t bytes_per_item,
                             uint64_t &total,
                             Status *status,
                             const char *description) {
    if ((count != 0 &&
         bytes_per_item > std::numeric_limits<uint64_t>::max() / count) ||
        count * bytes_per_item >
            std::numeric_limits<uint64_t>::max() - total) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            std::string(description) +
                                " storage reservation overflowed");
    }
    total += count * bytes_per_item;
    return true;
}

bool is_write_opcode(IbOpcode opcode) {
    return opcode == IB_RDMA_WRITE_FIRST ||
           opcode == IB_RDMA_WRITE_MIDDLE ||
           opcode == IB_RDMA_WRITE_LAST ||
           opcode == IB_RDMA_WRITE_LAST_WITH_IMMEDIATE ||
           opcode == IB_RDMA_WRITE_ONLY ||
           opcode == IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE;
}

bool is_send_opcode(IbOpcode opcode) {
    return opcode == IB_SEND_FIRST || opcode == IB_SEND_MIDDLE || opcode == IB_SEND_LAST ||
           opcode == IB_SEND_LAST_WITH_IMMEDIATE || opcode == IB_SEND_ONLY ||
           opcode == IB_SEND_ONLY_WITH_IMMEDIATE || opcode == IB_SEND_LAST_WITH_INVALIDATE ||
           opcode == IB_SEND_ONLY_WITH_INVALIDATE;
}

bool is_read_response_opcode(IbOpcode opcode) {
    return opcode == IB_RDMA_READ_RESPONSE_FIRST ||
           opcode == IB_RDMA_READ_RESPONSE_MIDDLE ||
           opcode == IB_RDMA_READ_RESPONSE_LAST ||
           opcode == IB_RDMA_READ_RESPONSE_ONLY;
}

bool has_immediate(IbOpcode opcode) {
    return opcode == IB_RDMA_WRITE_LAST_WITH_IMMEDIATE ||
           opcode == IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE;
}

bool endpoint_reverse_matches(const NativeEndpoint &local,
                              const NativeEndpoint &incoming) {
    const size_t ip_bytes = local.ipv6 ? 16 : 4;
    bool source_mac_valid = (incoming.src_mac[0] & 1U) == 0;
    bool source_mac_nonzero = false;
    for (size_t index = 0; index < sizeof(incoming.src_mac); ++index) {
        source_mac_nonzero = source_mac_nonzero || incoming.src_mac[index] != 0;
    }
    const bool vlan_matches =
        incoming.has_vlan == local.has_vlan &&
        (!local.has_vlan ||
         (incoming.vlan == local.vlan &&
          (local.has_next_hop ||
           (incoming.pcp == local.pcp &&
            incoming.vlan_dei == local.vlan_dei))));
    const bool source_mac_matches = local.has_next_hop
        ? source_mac_valid && source_mac_nonzero
        : memcmp(incoming.src_mac,
                 local.dst_mac,
                 sizeof(local.src_mac)) == 0;
    return incoming.ipv6 == local.ipv6 &&
           vlan_matches && source_mac_matches &&
           memcmp(incoming.dst_mac, local.src_mac, sizeof(local.src_mac)) == 0 &&
           memcmp(incoming.src_ip, local.dst_ip, ip_bytes) == 0 &&
           memcmp(incoming.dst_ip, local.src_ip, ip_bytes) == 0 &&
           incoming.udp_sport != 0 &&
           incoming.udp_dport == ROCEV2_UDP_PORT;
}

uint64_t retry_timeout_ns(uint8_t exponent) {
    return 4096ULL << exponent;
}

bool add_u64(uint64_t left, uint64_t right, uint64_t &result) {
    if (right > std::numeric_limits<uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

uint64_t saturated_deadline(uint64_t now_ns, uint64_t delay_ns) {
    uint64_t deadline = 0;
    return add_u64(now_ns, delay_ns, deadline)
               ? deadline
               : std::numeric_limits<uint64_t>::max();
}

bool psn_interval_contains(uint32_t value, uint32_t first, uint32_t last) {
    bool contains = false;
    return psn24_in_closed_interval(value, first, last, contains, NULL) && contains;
}

bool reserve_output_frames(NativeTransportOutput &output,
                           size_t count,
                           Status *status) {
    if (output.bounded) {
        if (output.frames.size() > output.frame_limit ||
            count > output.frame_limit - output.frames.size()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "bounded transport output frame queue is full");
        }
        status_ok(status);
        return true;
    }
    if (count > output.frames.max_size() - output.frames.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport output frame count exceeds container capacity");
    }
    try {
        output.frames.reserve(output.frames.size() + count);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport output frame allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport output frame count exceeds container capacity");
    }
    return true;
}

bool reserve_output_completions(NativeTransportOutput &output,
                                size_t count,
                                Status *status) {
    if (output.bounded) {
        if (output.completions.size() > output.completion_limit ||
            count > output.completion_limit - output.completions.size()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "bounded transport completion output is full");
        }
        status_ok(status);
        return true;
    }
    if (count > output.completions.max_size() - output.completions.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport completion count exceeds container capacity");
    }
    try {
        output.completions.reserve(output.completions.size() + count);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport completion allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport completion count exceeds container capacity");
    }
    return true;
}

} // namespace

bool NativeTransportMemory::validate_local_key(uint32_t, uint32_t, uint64_t, uint64_t, Status *status) const {
    return status_error(status, ErrorCode::UNSUPPORTED, "memory adapter has no keyed receive support");
}
bool NativeTransportMemory::write_local_key(uint32_t lkey, uint32_t pd, uint64_t address,
                                            ByteSpan value, Status *status) {
    return validate_local_key(lkey, pd, address, value.size, status) &&
           write(address, value, MemoryAccess::LOCAL_WRITE, status);
}
bool NativeTransportMemory::validate_remote_access(uint32_t, uint64_t, uint64_t, MemoryAccess,
                                                   Status *status) const {
    status_ok(status);
    return true;
}
bool NativeTransportMemory::view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                                            MemoryAccess required, ByteSpan &out, Status *status) const {
    return validate_remote_access(rkey, address, length, required, status) &&
           view(address, length, required, out, status);
}
bool NativeTransportMemory::write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value, Status *status) {
    return validate_remote_access(rkey, address, value.size, MemoryAccess::REMOTE_WRITE, status) &&
           write(address, value, MemoryAccess::REMOTE_WRITE, status);
}
bool NativeTransportMemory::invalidate_remote_key(uint32_t, bool, Status *status) {
    return status_error(status, ErrorCode::UNSUPPORTED, "memory adapter has no invalidation support");
}
bool NativeTransportMemory::atomic(uint64_t, uint32_t, bool, uint64_t, uint64_t, uint64_t &, Status *status) {
    return status_error(status, ErrorCode::UNSUPPORTED, "memory adapter has no atomic support");
}

NativeTransportConfig::NativeTransportConfig() :
    pkey(0xffff),
    control_dscp(48),
    signal_mode(NativeTransportSignalMode::IMMEDIATE),
    ack_request_interval(0),
    duplicate_window(4096),
    ack_coalesce_packets(0),
    ack_coalesce_ns(0),
    receive_credits(0),
    unlimited_receive_credits(true),
    require_receive_wqes(false),
    legacy_response_acks(false),
    cnp_enabled(true),
    cnp_min_interval_ns(50000),
    cnp_reaction_factor(0.5),
    cnp_recovery_ns(1000000),
    max_tx_frames(32768),
    max_staged_write_bytes(64ULL << 20),
    max_read_cache_entries(64),
    max_read_cache_bytes(64ULL << 20) {
}

bool NativeTransportConfig::validate(Status *status) const {
    if (pkey == 0 || control_dscp > 63 || endpoint.dscp > 63 ||
        endpoint.ecn > 3 || endpoint.ttl == 0 || endpoint.udp_sport == 0 ||
        endpoint.udp_dport != ROCEV2_UDP_PORT || endpoint.pcp > 7 ||
        (endpoint.has_vlan && endpoint.vlan > 4094)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "native transport endpoint or P_Key is invalid");
    }
    if (duplicate_window == 0 || duplicate_window > PSN24_MAX_SERIAL_SPAN ||
        max_tx_frames == 0 || max_staged_write_bytes == 0 ||
        max_read_cache_entries == 0 || max_read_cache_bytes == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "native transport queue and cache bounds must be non-zero");
    }
    if (cnp_reaction_factor <= 0.0 || cnp_reaction_factor > 1.0 ||
        (cnp_enabled && (cnp_min_interval_ns == 0 || cnp_recovery_ns == 0))) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "native transport congestion parameters are invalid");
    }
    status_ok(status);
    return true;
}

NativeTransportWriteRequest::NativeTransportWriteRequest() :
    remote_address(0),
    remote_rkey(0),
    with_immediate(false),
    immediate_data(0),
    signalled(true), fenced(false) {
}

NativeTransportReadRequest::NativeTransportReadRequest() :
    remote_address(0),
    remote_rkey(0),
    local_address(0),
    local_lkey(0),
    length(0),
    signalled(true), fenced(false) {
}

NativeTransportFrame::NativeTransportFrame() :
    psn(0),
    opcode(IB_RDMA_WRITE_ONLY),
    retransmission(false) {
}

NativeTransportOutput::NativeTransportOutput()
    : frame_limit(0), completion_limit(0), bounded(false) {
}

bool NativeTransportOutput::configure(size_t max_frames,
                                      size_t max_completions,
                                      Status *status) {
    if (max_frames == 0 || max_completions == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "transport output bounds must be non-zero");
    }
    try {
        frames.clear();
        completions.clear();
        frames.reserve(max_frames);
        completions.reserve(max_completions);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "transport output allocation failed during admission");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "transport output bounds exceed container capacity");
    }
    frame_limit = max_frames;
    completion_limit = max_completions;
    bounded = true;
    status_ok(status);
    return true;
}

void NativeTransportOutput::clear() {
    frames.clear();
    completions.clear();
}

NativeTransportCounters::NativeTransportCounters() :
    write_messages_posted(0), read_messages_posted(0),
    send_messages_posted(0), receive_wqes_posted(0), receive_completions(0),
    atomic_messages_posted(0), atomic_requests_received(0), atomic_replay_hits(0), tx_packets(0),
    tx_bytes(0), rx_packets(0), rx_bytes(0), acks_sent(0), acks_received(0),
    duplicate_acks(0), naks_sent(0), naks_received(0), rnr_naks_sent(0),
    rnr_naks_received(0), sequence_naks_sent(0), sequence_naks_received(0),
    fatal_naks_sent(0), fatal_naks_received(0), duplicate_packets(0),
    out_of_order_packets(0), psn_gap_events(0), packets_lost_detected(0),
    packets_recovered(0), retransmit_packets(0), retransmit_events(0),
    timeout_retries(0), nak_retries(0), rnr_retries(0), retry_exhausted(0),
    rnr_retry_exhausted(0), read_requests_received(0), read_response_packets(0),
    read_response_bytes(0), read_cache_hits(0), read_cache_misses(0),
    receive_credits_consumed(0), receive_credit_stalls(0), deliveries(0),
    delivered_bytes(0), ecn_ce_packets(0), cnp_sent(0), cnp_received(0),
    malformed_packets(0), unknown_qp_packets(0), icrc_errors(0),
    endpoint_mismatch_packets(0), stale_generation_packets(0) {
}

NativeTransportSnapshot::NativeTransportSnapshot() :
    configured(false), memory_bound(false), pending(false), outstanding_work(0),
    outstanding_packets(0), queued_tx_frames(0), receive_credits(0),
    unlimited_receive_credits(false), responder_msn(0), read_cache_entries(0),
    read_cache_bytes(0), deferred_read_frames(0), deferred_read_bytes(0),
    oldest_deadline_ns(0), rnr_resume_ns(0), next_send_ns(0),
    rate_scale(1.0), retained_frame_capacity(0), retained_frames(0),
    paced_frame_capacity(0), hot_path_growth_events(0) {
}

NativeTransportRuntimeView::NativeTransportRuntimeView() :
    configured(false),
    memory_bound(false),
    pending(false),
    outstanding_work(0),
    posted_receives(0),
    receive_cqes(0),
    queued_tx_frames(0),
    paced_tx_frames(0) {
}

struct NativeRcTransport::Impl {
    static const uint32_t INVALID_FRAME = 0xffffffffU;

    struct TxWork {
        PostedWork posted;
        uint32_t frame_head;
        uint32_t frame_tail;
        uint32_t frame_count;
        uint64_t posted_ns;
        uint64_t deadline_ns;
        uint64_t remote_address;
        uint32_t remote_rkey;
        uint64_t local_address;
        uint32_t local_lkey;
        uint64_t length;
        uint64_t read_bytes;
        uint32_t read_psns;
        uint32_t next_read_psn;
        bool has_accepted_frame;
        uint64_t last_accepted_ns;

        TxWork() : frame_head(INVALID_FRAME), frame_tail(INVALID_FRAME),
                   frame_count(0), posted_ns(0), deadline_ns(0), remote_address(0),
                   remote_rkey(0), local_address(0), local_lkey(0), length(0), read_bytes(0),
                   read_psns(0), next_read_psn(0), has_accepted_frame(false),
                   last_accepted_ns(0) {}
    };

    struct ReceiveSlot {
        uint64_t wr_id;
        uint64_t sequence;
        uint64_t capacity;
        uint32_t count;
        bool signalled;
        NativeScatterGatherEntry sges[16];
        ReceiveSlot() : wr_id(0), sequence(0), capacity(0), count(0), signalled(true) {}
    };

    struct StagedWrite {
        bool active;
        bool send;
        ReceiveSlot receive;
        uint64_t address;
        uint32_t rkey;
        uint64_t declared_length;
        size_t used;
        std::vector<uint8_t> bytes;

        StagedWrite() : active(false), send(false), address(0), rkey(0), declared_length(0),
                        used(0) {}
        void clear() {
            active = false;
            send = false;
            address = 0;
            rkey = 0;
            declared_length = 0;
            used = 0;
        }
    };

    struct ReadCacheEntry {
        uint32_t first_psn;
        uint32_t terminal_psn;
        uint64_t address;
        uint32_t rkey;
        uint64_t length;
        uint64_t payload_bytes;
        uint32_t frame_head;
        uint32_t frame_tail;
        uint32_t frame_count;

        ReadCacheEntry() : first_psn(0), terminal_psn(0), address(0), rkey(0),
                           length(0), payload_bytes(0), frame_head(INVALID_FRAME),
                           frame_tail(INVALID_FRAME), frame_count(0) {}
    };

    struct DeferredReadFrame {
        uint32_t psn;
        NativeFrameBuffer bytes;

        DeferredReadFrame() : psn(0) {}
    };

    struct ReplayCursor {
        bool active;
        size_t work_index;
        uint32_t frame_index;
        bool adjusted_read;
        uint32_t start_psn;

        ReplayCursor() : active(false), work_index(0),
                         frame_index(INVALID_FRAME),
                         adjusted_read(false), start_psn(0) {}
        void clear() {
            active = false;
            work_index = 0;
            frame_index = INVALID_FRAME;
            adjusted_read = false;
            start_psn = 0;
        }
    };

    struct RetainedFrame {
        NativeTransportFrame frame;
        uint32_t next;
        bool used;

        RetainedFrame() : next(INVALID_FRAME), used(false) {}
    };

    explicit Impl(QueuePair &queue_pair) :
        qp(queue_pair), memory(NULL), configured(false), detached(false),
        receive_credits(0), responder_msn(0), next_receive_sequence(1), has_last_time(false), last_time_ns(0),
        retry_remaining(0), rnr_retry_remaining(0), rnr_waiting(false),
        rnr_psn(0), rnr_resume_ns(0), oldest_deadline_ns(0), has_gap_nak(false),
        gap_nak_epsn(0), has_read_gap_nak(false), read_gap_psn(0),
        recovery_remaining(0), pending_ack(false),
        pending_ack_psn(0), pending_ack_msn(0), pending_ack_deadline_ns(0),
        accepted_since_ack(0), has_last_cnp(false), last_cnp_ns(0),
        rate_scale(1.0), congestion_recovery_ns(0), next_send_ns(0),
        pending_frame_head(0), retained_free_head(INVALID_FRAME),
        retained_in_use(0), read_cache_bytes(0), deferred_read_bytes(0),
        hot_path_growth_events(0) {}

    QueuePair &qp;
    NativeTransportMemory *memory;
    NativeTransportConfig config;
    bool configured;
    bool detached;
    uint32_t receive_credits;
    uint32_t responder_msn;
    uint64_t next_receive_sequence;
    NativeBoundedRing<ReceiveSlot> receives;
    std::vector<CompletionRecord> receive_completions;
    bool has_last_time;
    uint64_t last_time_ns;
    uint8_t retry_remaining;
    uint8_t rnr_retry_remaining;
    bool rnr_waiting;
    uint32_t rnr_psn;
    uint64_t rnr_resume_ns;
    uint64_t oldest_deadline_ns;
    bool has_gap_nak;
    uint32_t gap_nak_epsn;
    bool has_read_gap_nak;
    uint32_t read_gap_psn;
    uint32_t recovery_remaining;
    bool pending_ack;
    uint32_t pending_ack_psn;
    uint32_t pending_ack_msn;
    uint64_t pending_ack_deadline_ns;
    uint32_t accepted_since_ack;
    bool has_last_cnp;
    uint64_t last_cnp_ns;
    double rate_scale;
    uint64_t congestion_recovery_ns;
    uint64_t next_send_ns;
    /* Declared before all handle owners so the slab is destroyed last. */
    NativeFramePool frame_pool;
    StagedWrite staged_write;
    std::vector<TxWork> tx_works;
    std::vector<NativeTransportFrame> pending_frames;
    size_t pending_frame_head;
    std::vector<RetainedFrame> retained_frames;
    uint32_t retained_free_head;
    uint32_t retained_in_use;
    std::vector<NativeTransportFrame> frame_scratch;
    ReplayCursor replay;
    std::vector<ReadCacheEntry> read_cache;
    uint64_t read_cache_bytes;
    std::vector<DeferredReadFrame> deferred_reads;
    uint64_t deferred_read_bytes;
    uint64_t hot_path_growth_events;
    NativeTransportCounters counters;

    bool check_active(Status *status) const {
        if (!configured || detached || (qp.state() != QpState::RTS && qp.state() != QpState::RTR && qp.state() != QpState::SQD)) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport is not active");
        }
        return true;
    }

    bool response_outstanding() const {
        for (size_t i = 0; i < tx_works.size(); ++i) {
            if (work_requires_response(tx_works[i].posted.kind)) return true;
        }
        return false;
    }
    bool check_fence(bool fenced, Status *status) const {
        if (fenced && response_outstanding()) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "fence is waiting for preceding Read/Atomic work");
        }
        return true;
    }

    bool receive_available() const {
        return !receives.empty() || (!config.require_receive_wqes &&
               (config.unlimited_receive_credits || receive_credits != 0));
    }

    bool complete_receive(const ReceiveSlot &receive, WorkKind kind,
                           const DecodedPacket &packet, uint64_t bytes,
                           CompletionState state, ErrorCode error, Status *status) {
        if (receive_completions.size() >= receive_completions.capacity()) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "receive CQ is full");
        }
        if (state == CompletionState::SUCCESS && !receive.signalled) return true;
        CompletionRecord record;
        record.id = WorkId(qp.id(), receive.sequence);
        record.wr_id = receive.wr_id;
        record.kind = kind;
        record.state = state;
        record.error = error;
        record.terminal_psn = packet.psn;
        record.completed_bytes = state == CompletionState::SUCCESS ? bytes : 0;
        record.completed_psns = state == CompletionState::SUCCESS ? 1 : 0;
        if (state == CompletionState::SUCCESS) {
            record.has_immediate = packet.has_immediate_data;
            record.immediate_data = packet.immediate_data;
            record.has_invalidate = packet.has_invalidate;
            record.invalidated_rkey = packet.invalidate_rkey;
        }
        receive_completions.push_back(record);
        ++counters.receive_completions;
        return true;
    }

    bool consume_write_immediate(const DecodedPacket &packet, uint64_t bytes, Status *status) {
        if (!receives.empty()) {
            const ReceiveSlot receive = receives.front();
            if (!complete_receive(receive, WorkKind::RECEIVE_RDMA_WITH_IMMEDIATE,
                                  packet, bytes, CompletionState::SUCCESS, ErrorCode::OK, status)) return false;
            receives.pop();
        } else if (!config.unlimited_receive_credits) {
            --receive_credits;
        }
        ++counters.receive_credits_consumed;
        return true;
    }

    bool scatter_receive(const ReceiveSlot &receive, ByteSpan bytes, Status *status) {
        size_t offset = 0;
        for (uint32_t i = 0; i < receive.count; ++i) {
            const NativeScatterGatherEntry &sge = receive.sges[i];
            const size_t count = std::min<uint64_t>(sge.length, bytes.size - offset);
            if (!memory->validate_local_key(sge.lkey, qp.runtime_view().config.protection_domain,
                                            sge.address, count, status)) return false;
            if (count && !memory->write_local_key(sge.lkey, qp.runtime_view().config.protection_domain,
                                                   sge.address, ByteSpan(bytes.data + offset, count), status)) return false;
            offset += count;
            if (offset == bytes.size) break;
        }
        return offset == bytes.size || status_error(status, ErrorCode::OUT_OF_RANGE, "receive SGEs are too small");
    }

    void flush_receives(ErrorCode error) {
        DecodedPacket packet;
        if (staged_write.active && staged_write.send) {
            complete_receive(staged_write.receive, WorkKind::RECEIVE, packet, 0,
                             CompletionState::FLUSHED, error, NULL);
        }
        while (!receives.empty()) {
            complete_receive(receives.front(), WorkKind::RECEIVE, packet, 0,
                             CompletionState::FLUSHED, error, NULL);
            receives.pop();
        }
    }

    bool check_time(uint64_t now_ns, Status *status) {
        if (has_last_time && now_ns < last_time_ns) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native RC transport clock moved backwards");
        }
        has_last_time = true;
        last_time_ns = now_ns;
        return true;
    }

    PacketEncodeSpec base_spec(IbOpcode opcode, uint32_t psn) const {
        const QpRuntimeView runtime = qp.runtime_view();
        PacketEncodeSpec spec;
        spec.endpoint = config.endpoint;
        spec.opcode = opcode;
        spec.pkey = config.pkey;
        spec.dest_qpn = runtime.remote_qpn;
        spec.psn = psn;
        spec.has_reth = false;
        spec.has_aeth = false;
        spec.has_immediate_data = false;
        spec.has_deth = false;
        return spec;
    }

    bool encode_frame(const PacketEncodeSpec &spec,
                      const WorkId &work_id,
                      bool retransmission,
                      NativeTransportFrame &frame,
                      Status *status) {
        CodecError error;
        NativeTransportFrame candidate;
        if (!frame_pool.acquire(candidate.bytes, status)) {
            return false;
        }
        size_t written = 0;
        if (!NativePacketCodec::encode(spec,
                                       candidate.bytes.mutable_data(),
                                       frame_pool.stride(),
                                       written,
                                       &error) ||
            !frame_pool.set_length(candidate.bytes, written, status)) {
            return status_error(status,
                                ErrorCode::PROTOCOL_ERROR,
                                std::string("could not encode native RC packet: ") +
                                    error.message);
        }
        candidate.work_id = work_id;
        candidate.psn = spec.psn;
        candidate.opcode = spec.opcode;
        candidate.retransmission = retransmission;
        frame = std::move(candidate);
        return true;
    }

    bool retained_available(uint32_t count, Status *status) const {
        if (count > retained_frames.size() - retained_in_use) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native retained-frame descriptor pool is exhausted");
        }
        status_ok(status);
        return true;
    }

    uint32_t retain_frame(const NativeTransportFrame &frame) {
        const uint32_t index = retained_free_head;
        RetainedFrame &slot = retained_frames[index];
        retained_free_head = slot.next;
        slot.frame = frame;
        slot.next = INVALID_FRAME;
        slot.used = true;
        ++retained_in_use;
        return index;
    }

    template <typename Owner>
    bool append_retained(Owner &owner,
                         const NativeTransportFrame &frame,
                         Status *status) {
        if (!retained_available(1, status)) {
            return false;
        }
        const uint32_t index = retain_frame(frame);
        if (owner.frame_tail == INVALID_FRAME) {
            owner.frame_head = index;
        } else {
            retained_frames[owner.frame_tail].next = index;
        }
        owner.frame_tail = index;
        ++owner.frame_count;
        status_ok(status);
        return true;
    }

    void release_retained(uint32_t index) {
        RetainedFrame &slot = retained_frames[index];
        slot.frame = NativeTransportFrame();
        slot.used = false;
        slot.next = retained_free_head;
        retained_free_head = index;
        --retained_in_use;
    }

    template <typename Owner>
    void release_owner_frames(Owner &owner) {
        uint32_t current = owner.frame_head;
        while (current != INVALID_FRAME) {
            const uint32_t next = retained_frames[current].next;
            release_retained(current);
            current = next;
        }
        owner.frame_head = INVALID_FRAME;
        owner.frame_tail = INVALID_FRAME;
        owner.frame_count = 0;
    }

    uint32_t owner_frame_at(uint32_t head, size_t ordinal) const {
        uint32_t current = head;
        while (current != INVALID_FRAME && ordinal != 0) {
            current = retained_frames[current].next;
            --ordinal;
        }
        return current;
    }

    bool retain_scratch(TxWork &owner, Status *status) {
        if (!retained_available(static_cast<uint32_t>(frame_scratch.size()),
                                status)) {
            return false;
        }
        for (size_t index = 0; index < frame_scratch.size(); ++index) {
            if (!append_retained(owner, frame_scratch[index], status)) {
                release_owner_frames(owner);
                return false;
            }
        }
        return true;
    }

    bool retain_scratch(ReadCacheEntry &owner, Status *status) {
        if (!retained_available(static_cast<uint32_t>(frame_scratch.size()),
                                status)) {
            return false;
        }
        for (size_t index = 0; index < frame_scratch.size(); ++index) {
            if (!append_retained(owner, frame_scratch[index], status)) {
                release_owner_frames(owner);
                return false;
            }
        }
        return true;
    }

    void count_tx(const NativeTransportFrame &frame) {
        counters.tx_packets++;
        counters.tx_bytes += frame.bytes.size();
        if (frame.retransmission) {
            counters.retransmit_packets++;
        }
    }

    size_t pending_frame_count() const {
        return pending_frames.size() - pending_frame_head;
    }

    void clear_pending_frames() {
        pending_frames.clear();
        pending_frame_head = 0;
    }

    void compact_pending_frames() {
        if (pending_frame_head == 0) {
            return;
        }
        pending_frames.erase(pending_frames.begin(),
                             pending_frames.begin() + pending_frame_head);
        pending_frame_head = 0;
    }

    void update_congestion(uint64_t now_ns) {
        if (congestion_recovery_ns != 0 && now_ns >= congestion_recovery_ns) {
            rate_scale = 1.0;
            congestion_recovery_ns = 0;
            next_send_ns = 0;
        }
    }

    uint64_t pacing_interval_ns() const {
        if (rate_scale >= 1.0) {
            return 0;
        }
        const long double interval =
            static_cast<long double>(config.cnp_min_interval_ns) /
            static_cast<long double>(rate_scale);
        if (interval >= static_cast<long double>(
                            std::numeric_limits<uint64_t>::max())) {
            return std::numeric_limits<uint64_t>::max();
        }
        const uint64_t truncated = static_cast<uint64_t>(interval);
        return std::max<uint64_t>(1, truncated);
    }

    bool pacing_active(uint64_t now_ns) {
        update_congestion(now_ns);
        return rate_scale < 1.0;
    }

    bool preflight_publication(size_t count,
                               uint64_t now_ns,
                               NativeTransportOutput &output,
                               Status *status) {
        if (!pacing_active(now_ns)) {
            return reserve_output_frames(output, count, status);
        }
        if (pending_frame_count() > config.max_tx_frames ||
            count > config.max_tx_frames - pending_frame_count()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "congestion-paced transmit queue is full");
        }
        status_ok(status);
        return true;
    }

    bool publish_or_queue(std::vector<NativeTransportFrame> &frames,
                          uint64_t now_ns,
                          NativeTransportOutput &output,
                          Status *status) {
        if (frames.empty()) {
            status_ok(status);
            return true;
        }
        if (!pacing_active(now_ns)) {
            if (!reserve_output_frames(output, frames.size(), status)) {
                return false;
            }
            for (size_t index = 0; index < frames.size(); ++index) {
                count_tx(frames[index]);
                output.frames.push_back(std::move(frames[index]));
            }
            status_ok(status);
            return true;
        }
        if (pending_frame_count() > config.max_tx_frames ||
            frames.size() > config.max_tx_frames - pending_frame_count()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "congestion-paced transmit queue is full");
        }
        compact_pending_frames();
        for (size_t index = 0; index < frames.size(); ++index) {
            pending_frames.push_back(std::move(frames[index]));
        }
        if (next_send_ns == 0 || next_send_ns <= now_ns) {
            next_send_ns = saturated_deadline(now_ns, pacing_interval_ns());
        }
        status_ok(status);
        return true;
    }

    bool drain_paced_frames(uint64_t now_ns,
                            uint32_t &frame_budget,
                            NativeTransportOutput &output,
                            Status *status) {
        update_congestion(now_ns);
        while (frame_budget != 0 && pending_frame_count() != 0) {
            if (rate_scale < 1.0 &&
                (next_send_ns == 0 || now_ns < next_send_ns)) {
                break;
            }
            if (!reserve_output_frames(output, 1, status)) {
                return false;
            }
            NativeTransportFrame &frame = pending_frames[pending_frame_head++];
            count_tx(frame);
            output.frames.push_back(std::move(frame));
            frame_budget--;
            if (rate_scale < 1.0) {
                next_send_ns = saturated_deadline(next_send_ns,
                                                  pacing_interval_ns());
            }
        }
        if (pending_frame_count() == 0) {
            clear_pending_frames();
            if (rate_scale >= 1.0) {
                next_send_ns = 0;
            }
        }
        status_ok(status);
        return true;
    }

    bool publish_frames(std::vector<NativeTransportFrame> &frames,
                        NativeTransportOutput &output,
                        Status *status) {
        if (!reserve_output_frames(output, frames.size(), status)) {
            return false;
        }
        for (size_t index = 0; index < frames.size(); index++) {
            count_tx(frames[index]);
            output.frames.push_back(std::move(frames[index]));
        }
        status_ok(status);
        return true;
    }

    bool collect_completions(NativeTransportOutput &output, Status *status) {
        const size_t sq_ready = qp.runtime_view().ledger.ready_completions;
        const size_t ready = sq_ready + receive_completions.size();
        if (!reserve_output_completions(output, ready, status)) {
            return false;
        }
        if (!qp.drain_completions_bounded(output.completions, sq_ready, status)) return false;
        output.completions.insert(output.completions.end(), receive_completions.begin(), receive_completions.end());
        receive_completions.clear();
        status_ok(status);
        return true;
    }

    void refresh_deadline(uint64_t now_ns) {
        if (tx_works.empty() || rnr_waiting || replay.active ||
            pending_frame_count() != 0 ||
            !tx_works.front().has_accepted_frame) {
            oldest_deadline_ns = 0;
            return;
        }
        oldest_deadline_ns = saturated_deadline(
            now_ns, retry_timeout_ns(qp.runtime_view().config.timeout_exp));
        tx_works.front().deadline_ns = oldest_deadline_ns;
    }

    bool work_owns_live_psn(const TxWork &work, uint32_t psn) const {
        if (work.posted.kind == WorkKind::READ) {
            return psn_interval_contains(psn,
                                         work.next_read_psn,
                                         work.posted.terminal_psn);
        }
        for (uint32_t index = work.frame_head; index != INVALID_FRAME;
             index = retained_frames[index].next) {
            if (retained_frames[index].frame.psn == psn) {
                return true;
            }
        }
        return false;
    }

    void prune_acked_writes(uint32_t old_ack, uint32_t new_ack) {
        size_t work_index = 0;
        while (work_index < tx_works.size()) {
            TxWork &work = tx_works[work_index];
            if (!work_completes_by_ack(work.posted.kind)) {
                work_index++;
                continue;
            }
            uint32_t previous = INVALID_FRAME;
            uint32_t frame_index = work.frame_head;
            while (frame_index != INVALID_FRAME) {
                RetainedFrame &retained = retained_frames[frame_index];
                const uint32_t next = retained.next;
                const uint32_t frame_psn = retained.frame.psn;
                if (frame_psn != old_ack &&
                    psn_interval_contains(frame_psn, old_ack, new_ack)) {
                    if (previous == INVALID_FRAME) {
                        work.frame_head = next;
                    } else {
                        retained_frames[previous].next = next;
                    }
                    if (work.frame_tail == frame_index) {
                        work.frame_tail = previous;
                    }
                    release_retained(frame_index);
                    --work.frame_count;
                } else {
                    previous = frame_index;
                }
                frame_index = next;
            }
            if (work.frame_count == 0 && work.posted.terminal_psn != old_ack &&
                psn_interval_contains(work.posted.terminal_psn,
                                      old_ack,
                                      new_ack)) {
                tx_works.erase(tx_works.begin() + work_index);
            } else {
                work_index++;
            }
        }
    }

    bool validate_local_window(uint64_t address,
                               uint64_t length,
                               uint32_t rkey,
                               MemoryAccess access,
                               Status *status) const {
        if (qp.runtime_view().config.allow_remote_registry) {
            return memory != NULL && memory->validate_remote_access(rkey, address, length, access, status);
        }
        const MemoryWindowSnapshot window = qp.runtime_view().local_window;
        if (!window.configured || !window.active || window.rkey != rkey ||
            !has_memory_access(window.access, access)) {
            return status_error(status,
                                ErrorCode::ACCESS_DENIED,
                                "remote request does not match the registered memory window");
        }
        if (memory && !memory->validate_remote_access(rkey, address, length, access, status)) return false;
        uint64_t end = 0;
        if (length == 0) {
            const bool one_past = window.last_address !=
                                       std::numeric_limits<uint64_t>::max() &&
                                   address == window.last_address + 1;
            if (address < window.base_address ||
                (address > window.last_address && !one_past)) {
                return status_error(status,
                                    ErrorCode::OUT_OF_RANGE,
                                    "zero-byte remote request is outside the memory window");
            }
            status_ok(status);
            return true;
        }
        if (!add_u64(address, length - 1, end) || address < window.base_address ||
            end > window.last_address) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "remote request exceeds the registered memory window");
        }
        status_ok(status);
        return true;
    }

    bool emit_control(uint8_t syndrome,
                      uint32_t psn,
                      NativeTransportOutput &output,
                      Status *status) {
        PacketEncodeSpec spec = base_spec(IB_ACKNOWLEDGE, psn);
        spec.has_aeth = true;
        spec.aeth.syndrome = syndrome;
        spec.aeth.msn = responder_msn;
        NativeTransportFrame frame;
        if (!encode_frame(spec, WorkId(), false, frame, status) ||
            !reserve_output_frames(output, 1, status)) {
            return false;
        }
        count_tx(frame);
        output.frames.push_back(std::move(frame));
        if (syndrome <= 0x1f) {
            counters.acks_sent++;
        } else {
            counters.naks_sent++;
            if (syndrome >= 0x20 && syndrome <= 0x3f) {
                counters.rnr_naks_sent++;
            } else if (syndrome == AETH_SEQUENCE_NAK) {
                counters.sequence_naks_sent++;
            } else {
                counters.fatal_naks_sent++;
            }
        }
        status_ok(status);
        return true;
    }

    bool maybe_emit_cnp(const DecodedPacket &packet,
                        uint64_t now_ns,
                        NativeTransportOutput &output,
                        Status *status) {
        if (packet.endpoint.ecn != 3) {
            return true;
        }
        counters.ecn_ce_packets++;
        if (!config.cnp_enabled ||
            (has_last_cnp && now_ns - last_cnp_ns < config.cnp_min_interval_ns)) {
            return true;
        }
        PacketEncodeSpec spec = base_spec(IB_CNP, 0);
        spec.endpoint.dscp = config.control_dscp;
        spec.endpoint.ecn = 0;
        spec.becn = true;
        NativeTransportFrame frame;
        if (!encode_frame(spec, WorkId(), false, frame, status) ||
            !reserve_output_frames(output, 1, status)) {
            return false;
        }
        count_tx(frame);
        output.frames.push_back(std::move(frame));
        has_last_cnp = true;
        last_cnp_ns = now_ns;
        counters.cnp_sent++;
        return true;
    }

    bool fail_all(ErrorCode error,
                  const std::string &reason,
                  NativeTransportOutput &output,
                  Status *status) {
        for (size_t index = 0; index < tx_works.size(); index++) {
            qp.fail_work(tx_works[index].posted.id, error, NULL);
            release_owner_frames(tx_works[index]);
        }
        tx_works.clear();
        clear_pending_frames();
        replay.clear();
        deferred_reads.clear();
        deferred_read_bytes = 0;
        flush_receives(error);
        staged_write.clear();
        pending_ack = false;
        rnr_waiting = false;
        rnr_resume_ns = 0;
        oldest_deadline_ns = 0;
        if (qp.state() == QpState::RTS || qp.state() == QpState::RTR || qp.state() == QpState::SQD) {
            qp.enter_error(error, reason, NULL);
        }
        return collect_completions(output, status);
    }

    bool emit_cached_from(uint32_t psn,
                          uint64_t now_ns,
                          NativeTransportOutput &output,
                          Status *status) {
        for (size_t entry_index = 0; entry_index < read_cache.size();
             entry_index++) {
            ReadCacheEntry &entry = read_cache[entry_index];
            if (!psn_interval_contains(psn,
                                       entry.first_psn,
                                       entry.terminal_psn)) {
                continue;
            }
            uint32_t frame_index = entry.frame_head;
            while (frame_index != INVALID_FRAME &&
                   retained_frames[frame_index].frame.psn != psn) {
                frame_index = retained_frames[frame_index].next;
            }
            if (frame_index == INVALID_FRAME) {
                counters.read_cache_misses++;
                return true;
            }
            size_t count = 0;
            for (uint32_t current = frame_index; current != INVALID_FRAME;
                 current = retained_frames[current].next) {
                ++count;
            }
            if (count > config.max_tx_frames ||
                !preflight_publication(count, now_ns, output, status)) {
                return false;
            }
            counters.read_cache_hits++;
            counters.retransmit_events++;
            frame_scratch.clear();
            for (; frame_index != INVALID_FRAME;
                 frame_index = retained_frames[frame_index].next) {
                NativeTransportFrame replay = retained_frames[frame_index].frame;
                replay.retransmission = true;
                frame_scratch.push_back(std::move(replay));
            }
            return publish_or_queue(frame_scratch, now_ns, output, status);
        }
        counters.read_cache_misses++;
        status_ok(status);
        return true;
    }

    bool cache_contains(uint32_t psn) const {
        for (size_t index = 0; index < read_cache.size(); index++) {
            if (psn_interval_contains(psn,
                                      read_cache[index].first_psn,
                                      read_cache[index].terminal_psn)) {
                return true;
            }
        }
        return false;
    }

    void acknowledge_read_cache(uint32_t psn) {
        size_t index = 0;
        while (index < read_cache.size()) {
            if (psn_interval_contains(read_cache[index].terminal_psn,
                                      read_cache[index].first_psn,
                                      psn)) {
                read_cache_bytes -= read_cache[index].payload_bytes;
                release_owner_frames(read_cache[index]);
                read_cache.erase(read_cache.begin() + index);
            } else {
                index++;
            }
        }
    }

    bool build_adjusted_read_retry(const TxWork &work,
                                   uint32_t start_psn,
                                   NativeTransportFrame &frame,
                                   Status *status) {
        uint32_t psn_offset = 0;
        if (!psn24_forward_distance(work.posted.first_psn,
                                    start_psn,
                                    psn_offset,
                                    status) ||
            psn_offset >= work.posted.expected_read_psns) {
            return status_error(status,
                                ErrorCode::PROTOCOL_ERROR,
                                "read retry PSN is outside the response span");
        }
        const uint64_t byte_offset = std::min<uint64_t>(
            work.length,
            static_cast<uint64_t>(psn_offset) *
                qp.runtime_view().negotiated_pmtu);
        uint64_t adjusted_address = 0;
        if (!add_u64(work.remote_address, byte_offset, adjusted_address)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "adjusted read retry address overflowed");
        }
        const uint64_t remaining = work.length - byte_offset;
        PacketEncodeSpec spec = base_spec(IB_RDMA_READ_REQUEST, start_psn);
        spec.has_reth = true;
        spec.reth.virtual_address = adjusted_address;
        spec.reth.rkey = work.remote_rkey;
        spec.reth.dma_length = static_cast<uint32_t>(remaining);
        spec.ack_request = true;
        spec.ipv4_identification = static_cast<uint16_t>(start_psn);
        return encode_frame(spec, work.posted.id, true, frame, status);
    }

    void restart_active_replay() {
        if (!replay.active) {
            return;
        }
        if (tx_works.empty()) {
            replay.clear();
            return;
        }
        replay.work_index = 0;
        replay.adjusted_read = tx_works.front().posted.kind == WorkKind::READ;
        replay.frame_index = replay.adjusted_read
                                 ? INVALID_FRAME
                                 : tx_works.front().frame_head;
        replay.start_psn = replay.adjusted_read
                               ? tx_works.front().next_read_psn
                               : retained_frames[tx_works.front().frame_head].frame.psn;
    }

    bool begin_replay(uint32_t start_psn, Status *status) {
        replay.clear();
        for (size_t work_index = 0; work_index < tx_works.size(); work_index++) {
            const TxWork &work = tx_works[work_index];
            if (work.posted.kind == WorkKind::READ &&
                psn_interval_contains(start_psn,
                                      work.posted.first_psn,
                                      work.posted.terminal_psn)) {
                replay.active = true;
                replay.work_index = work_index;
                replay.frame_index = INVALID_FRAME;
                replay.adjusted_read = true;
                replay.start_psn = start_psn;
                tx_works[work_index].has_accepted_frame = false;
                counters.retransmit_events++;
                status_ok(status);
                return true;
            }
            for (uint32_t frame_index = work.frame_head;
                 frame_index != INVALID_FRAME;
                 frame_index = retained_frames[frame_index].next) {
                if (retained_frames[frame_index].frame.psn == start_psn) {
                    replay.active = true;
                    replay.work_index = work_index;
                    replay.frame_index = frame_index;
                    replay.adjusted_read = false;
                    replay.start_psn = start_psn;
                    tx_works[work_index].has_accepted_frame = false;
                    counters.retransmit_events++;
                    status_ok(status);
                    return true;
                }
            }
        }
        counters.malformed_packets++;
        status_ok(status);
        return true;
    }

    bool emit_replay_chunk(uint64_t now_ns,
                           uint32_t frame_budget,
                           NativeTransportOutput &output,
                           Status *status) {
        if (!replay.active || frame_budget == 0) {
            status_ok(status);
            return true;
        }
        size_t capacity = frame_budget;
        if (pacing_active(now_ns)) {
            capacity = std::min<size_t>(capacity,
                config.max_tx_frames - pending_frame_count());
        }
        capacity = std::min<size_t>(capacity, config.max_tx_frames);
        if (capacity == 0) {
            status_ok(status);
            return true;
        }
        frame_scratch.clear();
        while (replay.active && frame_scratch.size() < capacity) {
            if (replay.work_index >= tx_works.size()) {
                replay.clear();
                break;
            }
            const TxWork &work = tx_works[replay.work_index];
            if (replay.adjusted_read) {
                NativeTransportFrame adjusted;
                if (!build_adjusted_read_retry(work,
                                               replay.start_psn,
                                               adjusted,
                                               status)) {
                    return false;
                }
                frame_scratch.push_back(std::move(adjusted));
                replay.adjusted_read = false;
                replay.frame_index = INVALID_FRAME;
            } else if (replay.frame_index != INVALID_FRAME) {
                NativeTransportFrame frame =
                    retained_frames[replay.frame_index].frame;
                replay.frame_index = retained_frames[replay.frame_index].next;
                frame.retransmission = true;
                frame_scratch.push_back(std::move(frame));
            }
            if (replay.frame_index == INVALID_FRAME) {
                replay.work_index++;
                if (replay.work_index >= tx_works.size()) {
                    replay.clear();
                } else {
                    replay.frame_index = tx_works[replay.work_index].frame_head;
                }
            }
        }
        if (!preflight_publication(frame_scratch.size(), now_ns, output, status) ||
            !publish_or_queue(frame_scratch, now_ns, output, status)) {
            return false;
        }
        if (!replay.active) {
            if (pending_frame_count() == 0) {
                refresh_deadline(now_ns);
            } else {
                oldest_deadline_ns = 0;
            }
        }
        status_ok(status);
        return true;
    }

    bool retransmit_from(uint32_t start_psn,
                         uint64_t now_ns,
                         uint32_t frame_budget,
                         NativeTransportOutput &output,
                         Status *status) {
        if (!begin_replay(start_psn, status)) {
            return false;
        }
        return emit_replay_chunk(now_ns, frame_budget, output, status);
    }

    bool schedule_ack(uint32_t accepted_psn,
                      bool ack_request,
                      uint64_t now_ns,
                      NativeTransportOutput &output,
                      Status *status) {
        accepted_since_ack++;
        const bool threshold = config.ack_coalesce_packets != 0 &&
                               accepted_since_ack >= config.ack_coalesce_packets;
        /* AckReq is a protocol fence, not merely a hint.  In particular a
         * terminal packet must not wait forever when packet-only coalescing is
         * configured and no further packet will reach the threshold. */
        if (ack_request) {
            pending_ack = false;
            pending_ack_deadline_ns = 0;
            accepted_since_ack = 0;
            return emit_control(AETH_ACK, accepted_psn, output, status);
        }
        if (!ack_request && !threshold && !pending_ack) {
            return true;
        }
        if (config.ack_coalesce_packets == 0 && config.ack_coalesce_ns == 0) {
            accepted_since_ack = 0;
            return emit_control(AETH_ACK, accepted_psn, output, status);
        }
        pending_ack = true;
        pending_ack_psn = accepted_psn;
        pending_ack_msn = responder_msn;
        if (config.ack_coalesce_ns != 0 && pending_ack_deadline_ns == 0) {
            pending_ack_deadline_ns = saturated_deadline(
                now_ns, config.ack_coalesce_ns);
        }
        if (threshold) {
            pending_ack = false;
            pending_ack_deadline_ns = 0;
            accepted_since_ack = 0;
            return emit_control(AETH_ACK, pending_ack_psn, output, status);
        }
        status_ok(status);
        return true;
    }

    bool classify_request_psn(uint32_t psn,
                              RxPsnDisposition &disposition,
                              uint32_t &distance,
                              Status *status) {
        disposition = qp.classify_rx_psn(psn);
        distance = 0;
        const uint32_t expected = qp.runtime_view().expected_rx_psn;
        if (disposition == RxPsnDisposition::DUPLICATE) {
            if (!psn24_forward_distance(psn, expected, distance, status)) {
                return false;
            }
            if (distance > config.duplicate_window) {
                disposition = RxPsnDisposition::AMBIGUOUS;
            }
        } else if (disposition == RxPsnDisposition::GAP) {
            if (!psn24_forward_distance(expected, psn, distance, status)) {
                return false;
            }
        }
        status_ok(status);
        return true;
    }

    bool accept_write(const DecodedPacket &packet,
                      ByteSpan wire_frame,
                      uint64_t now_ns,
                      NativeTransportOutput &output,
                      Status *status) {
        const IbOpcode opcode = packet.opcode;
        if (memory == NULL || packet.payload.end() > wire_frame.size) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC write has no bounded memory or payload view");
        }
        const ByteSpan payload(packet.payload.length == 0
                                   ? NULL
                                   : wire_frame.data + packet.payload.offset,
                               packet.payload.length);
        const uint16_t pmtu = qp.runtime_view().negotiated_pmtu;
        if (payload.size > pmtu || ((opcode == IB_RDMA_WRITE_FIRST || opcode == IB_RDMA_WRITE_MIDDLE) && payload.size != pmtu)) {
            return emit_control(AETH_REMOTE_OPERATION, packet.psn, output, status);
        }
        if (opcode == IB_RDMA_WRITE_ONLY ||
            opcode == IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE) {
            if (staged_write.active || !packet.has_reth ||
                packet.reth.dma_length != packet.payload.length) {
                return emit_control(AETH_REMOTE_OPERATION,
                                    qp.runtime_view().expected_rx_psn,
                                    output,
                                    status);
            }
            Status access;
            if (!validate_local_window(packet.reth.virtual_address,
                                       packet.reth.dma_length,
                                       packet.reth.rkey,
                                       MemoryAccess::REMOTE_WRITE,
                                       &access)) {
                return emit_control(AETH_REMOTE_ACCESS,
                                    qp.runtime_view().expected_rx_psn,
                                    output,
                                    status);
            }
            if (has_immediate(opcode) && !receive_available()) {
                counters.receive_credit_stalls++;
                return emit_control(static_cast<uint8_t>(
                                        AETH_RNR_BASE |
                                        qp.runtime_view().config.min_rnr_timer),
                                    packet.psn,
                                    output,
                                    status);
            }
            if (!memory->write_remote_key(packet.reth.rkey, packet.reth.virtual_address,
                                          payload, status) ||
                !qp.accept_rx_psn(packet.psn, status)) {
                return false;
            }
            if (has_immediate(opcode) && !consume_write_immediate(packet, payload.size, status)) return false;
            psn24_add(responder_msn, 1, responder_msn, NULL);
            counters.deliveries++;
            counters.delivered_bytes += payload.size;
            if (recovery_remaining != 0) {
                recovery_remaining--;
                counters.packets_recovered++;
            }
            has_gap_nak = false;
            return schedule_ack(packet.psn,
                                packet.ack_request,
                                now_ns,
                                output,
                                status);
        }

        if (opcode == IB_RDMA_WRITE_FIRST) {
            if (staged_write.active || !packet.has_reth ||
                packet.reth.dma_length <= payload.size ||
                packet.reth.dma_length > config.max_staged_write_bytes) {
                return emit_control(AETH_REMOTE_OPERATION,
                                    qp.runtime_view().expected_rx_psn,
                                    output,
                                    status);
            }
            Status access;
            if (!validate_local_window(packet.reth.virtual_address,
                                       packet.reth.dma_length,
                                       packet.reth.rkey,
                                       MemoryAccess::REMOTE_WRITE,
                                       &access)) {
                return emit_control(AETH_REMOTE_ACCESS,
                                    qp.runtime_view().expected_rx_psn,
                                    output,
                                    status);
            }
            staged_write.address = packet.reth.virtual_address;
            staged_write.rkey = packet.reth.rkey;
            staged_write.declared_length = packet.reth.dma_length;
            staged_write.used = 0;
            if (payload.size != 0) {
                memcpy(staged_write.bytes.data(), payload.data, payload.size);
                staged_write.used = payload.size;
            }
            staged_write.active = true;
            if (!qp.accept_rx_psn(packet.psn, status)) {
                staged_write.clear();
                return false;
            }
            if (recovery_remaining != 0) {
                recovery_remaining--;
                counters.packets_recovered++;
            }
            has_gap_nak = false;
            return schedule_ack(packet.psn,
                                packet.ack_request,
                                now_ns,
                                output,
                                status);
        }

        if (!staged_write.active || staged_write.send) {
            return emit_control(AETH_REMOTE_OPERATION,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        if (opcode == IB_RDMA_WRITE_MIDDLE) {
            if (payload.size == 0 ||
                payload.size >= staged_write.declared_length -
                                    staged_write.used) {
                staged_write.clear();
                return emit_control(AETH_REMOTE_OPERATION,
                                    qp.runtime_view().expected_rx_psn,
                                    output,
                                    status);
            }
            memcpy(staged_write.bytes.data() + staged_write.used,
                   payload.data,
                   payload.size);
            staged_write.used += payload.size;
            if (!qp.accept_rx_psn(packet.psn, status)) {
                return false;
            }
            if (recovery_remaining != 0) {
                recovery_remaining--;
                counters.packets_recovered++;
            }
            has_gap_nak = false;
            return schedule_ack(packet.psn,
                                packet.ack_request,
                                now_ns,
                                output,
                                status);
        }
        if (opcode != IB_RDMA_WRITE_LAST &&
            opcode != IB_RDMA_WRITE_LAST_WITH_IMMEDIATE) {
            staged_write.clear();
            return emit_control(AETH_REMOTE_OPERATION,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        if (payload.size > staged_write.declared_length -
                               staged_write.used ||
            staged_write.used + payload.size !=
                staged_write.declared_length) {
            staged_write.clear();
            return emit_control(AETH_REMOTE_OPERATION,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        if (has_immediate(opcode) && !receive_available()) {
            counters.receive_credit_stalls++;
            return emit_control(static_cast<uint8_t>(
                                    AETH_RNR_BASE |
                                    qp.runtime_view().config.min_rnr_timer),
                                packet.psn,
                                output,
                                status);
        }
        memcpy(staged_write.bytes.data() + staged_write.used,
               payload.data,
               payload.size);
        staged_write.used += payload.size;
        if (!memory->write_remote_key(staged_write.rkey, staged_write.address,
                           ByteSpan(staged_write.used == 0
                                        ? NULL
                                        : staged_write.bytes.data(),
                                    staged_write.used), status) ||
            !qp.accept_rx_psn(packet.psn, status)) {
            staged_write.clear();
            return false;
        }
        const uint64_t delivered = staged_write.used;
        staged_write.clear();
        if (has_immediate(opcode) && !consume_write_immediate(packet, delivered, status)) return false;
        psn24_add(responder_msn, 1, responder_msn, NULL);
        counters.deliveries++;
        counters.delivered_bytes += delivered;
        if (recovery_remaining != 0) {
            recovery_remaining--;
            counters.packets_recovered++;
        }
        has_gap_nak = false;
        return schedule_ack(packet.psn,
                            packet.ack_request,
                            now_ns,
                            output,
                            status);
    }

    bool accept_send(const DecodedPacket &packet, ByteSpan wire_frame,
                      uint64_t now_ns, NativeTransportOutput &output, Status *status) {
        const bool only = packet.opcode == IB_SEND_ONLY || packet.opcode == IB_SEND_ONLY_WITH_IMMEDIATE ||
                          packet.opcode == IB_SEND_ONLY_WITH_INVALIDATE;
        const bool first = packet.opcode == IB_SEND_FIRST;
        const bool middle = packet.opcode == IB_SEND_MIDDLE;
        const bool last = !first && !middle;
        const QpRuntimeView runtime = qp.runtime_view();
        if (packet.payload.end() > wire_frame.size || packet.payload.length > runtime.negotiated_pmtu ||
            ((!last) && packet.payload.length != runtime.negotiated_pmtu) ||
            ((only || first) ? staged_write.active : (!staged_write.active || !staged_write.send))) {
            return emit_control(AETH_REMOTE_OPERATION, packet.psn, output, status);
        }
        if (only || first) {
            if (receives.empty()) {
                ++counters.receive_credit_stalls;
                return emit_control(static_cast<uint8_t>(AETH_RNR_BASE | runtime.config.min_rnr_timer), packet.psn, output, status);
            }
            staged_write.clear();
            staged_write.active = true;
            staged_write.send = true;
            staged_write.receive = receives.front();
            staged_write.declared_length = staged_write.receive.capacity;
            receives.pop();
        }
        if (packet.payload.length > staged_write.declared_length - staged_write.used) {
            const ReceiveSlot receive = staged_write.receive;
            staged_write.clear();
            if (!complete_receive(receive, WorkKind::RECEIVE, packet, 0,
                                  CompletionState::ERROR, ErrorCode::OUT_OF_RANGE, status) ||
                !emit_control(AETH_REMOTE_OPERATION, packet.psn, output, status)) return false;
            return fail_all(ErrorCode::REMOTE_OPERATION_ERROR, "SEND exceeds posted receive capacity", output, status);
        }
        if (packet.payload.length) {
            memcpy(staged_write.bytes.data() + staged_write.used,
                   wire_frame.data + packet.payload.offset, packet.payload.length);
        }
        staged_write.used += packet.payload.length;
        if (last) {
            Status access;
            const ReceiveSlot receive = staged_write.receive;
            if (packet.has_invalidate && !memory->invalidate_remote_key(packet.invalidate_rkey, false, &access)) {
                staged_write.clear();
                if (!complete_receive(receive, WorkKind::RECEIVE, packet, 0,
                                      CompletionState::ERROR, ErrorCode::ACCESS_DENIED, status) ||
                    !emit_control(AETH_REMOTE_ACCESS, packet.psn, output, status)) return false;
                return fail_all(ErrorCode::REMOTE_ACCESS_ERROR, "SEND invalidation key is invalid", output, status);
            }
            if (!scatter_receive(receive, ByteSpan(staged_write.used ? staged_write.bytes.data() : NULL,
                                                  staged_write.used), &access)) {
                staged_write.clear();
                if (!complete_receive(receive, WorkKind::RECEIVE, packet, 0,
                                      CompletionState::ERROR, access.code(), status) ||
                    !emit_control(AETH_REMOTE_ACCESS, packet.psn, output, status)) return false;
                return fail_all(ErrorCode::REMOTE_ACCESS_ERROR, "SEND receive key is invalid", output, status);
            }
            if (packet.has_invalidate && !memory->invalidate_remote_key(packet.invalidate_rkey, true, status)) return false;
            const uint64_t delivered = staged_write.used;
            staged_write.clear();
            if (!complete_receive(receive, WorkKind::RECEIVE, packet, delivered,
                                  CompletionState::SUCCESS, ErrorCode::OK, status)) return false;
            ++counters.deliveries;
            counters.delivered_bytes += delivered;
            ++counters.receive_credits_consumed;
            psn24_add(responder_msn, 1, responder_msn, NULL);
        }
        if (!qp.accept_rx_psn(packet.psn, status)) return false;
        has_gap_nak = false;
        if (recovery_remaining) { --recovery_remaining; ++counters.packets_recovered; }
        return schedule_ack(packet.psn, packet.ack_request, now_ns, output, status);
    }

    void recycle_response_slot() {
        if (config.legacy_response_acks || read_cache.empty() ||
            read_cache.size() < qp.runtime_view().config.max_dest_rd_atomic) return;
        read_cache_bytes -= read_cache.front().payload_bytes;
        release_owner_frames(read_cache.front());
        read_cache.erase(read_cache.begin());
    }

    bool accept_atomic_request(const DecodedPacket &packet, uint64_t now_ns,
                                NativeTransportOutput &output, Status *status) {
        const QpRuntimeView runtime = qp.runtime_view();
        if (runtime.config.max_dest_rd_atomic == 0) return emit_control(AETH_INVALID_REQUEST, packet.psn, output, status);
        if (staged_write.active || !packet.has_atomic_eth ||
            (packet.atomic_eth.virtual_address & 7U) ||
            (packet.opcode == IB_FETCH_ADD && packet.atomic_eth.compare)) {
            return emit_control(AETH_INVALID_REQUEST, packet.psn, output, status);
        }
        Status access;
        if (!validate_local_window(packet.atomic_eth.virtual_address, 8, packet.atomic_eth.rkey,
                                    MemoryAccess::REMOTE_ATOMIC, &access)) {
            return emit_control(AETH_REMOTE_ACCESS, packet.psn, output, status);
        }
        recycle_response_slot();
        if (read_cache.size() >= config.max_read_cache_entries ||
            read_cache.size() >= runtime.config.max_dest_rd_atomic ||
            8 > config.max_read_cache_bytes - std::min<uint64_t>(read_cache_bytes, config.max_read_cache_bytes) ||
            !retained_available(1, &access)) {
            ++counters.receive_credit_stalls;
            return emit_control(static_cast<uint8_t>(AETH_RNR_BASE | runtime.config.min_rnr_timer), packet.psn, output, status);
        }
        if (!preflight_publication(1, now_ns, output, status)) return false;
        PacketEncodeSpec spec = base_spec(IB_ATOMIC_ACKNOWLEDGE, packet.psn);
        spec.has_aeth = true;
        spec.aeth.syndrome = AETH_ACK;
        spec.aeth.msn = responder_msn;
        spec.has_atomic_ack = true;
        spec.ack_request = config.legacy_response_acks;
        NativeTransportFrame response;
        if (!encode_frame(spec, WorkId(), false, response, status)) return false;
        uint64_t original = 0;
        if (!memory->atomic(packet.atomic_eth.virtual_address, packet.atomic_eth.rkey,
                             packet.opcode == IB_FETCH_ADD, packet.atomic_eth.compare,
                             packet.atomic_eth.swap_add, original, &access)) {
            return emit_control(AETH_REMOTE_ACCESS, packet.psn, output, status);
        }
        /* All output/cache resources and fixed-size header legality were proved
         * before mutation. Re-encoding only replaces the eight result bytes. */
        spec.atomic_original_value = original;
        CodecError error;
        size_t written = 0;
        if (!NativePacketCodec::encode(spec, response.bytes.mutable_data(), frame_pool.stride(), written, &error) ||
            !frame_pool.set_length(response.bytes, written, status)) {
            return fail_all(ErrorCode::INTERNAL_ERROR, "reserved atomic response encode failed", output, status);
        }
        ReadCacheEntry cache;
        cache.first_psn = cache.terminal_psn = packet.psn;
        cache.address = packet.atomic_eth.virtual_address;
        cache.rkey = packet.atomic_eth.rkey;
        cache.length = cache.payload_bytes = 8;
        if (!append_retained(cache, response, status)) return false;
        read_cache.push_back(std::move(cache));
        read_cache_bytes += 8;
        frame_scratch.clear();
        frame_scratch.push_back(std::move(response));
        if (!qp.accept_rx_psn(packet.psn, status)) return false;
        psn24_add(responder_msn, 1, responder_msn, NULL);
        ++counters.atomic_requests_received;
        has_gap_nak = false;
        return publish_or_queue(frame_scratch, now_ns, output, status);
    }

    bool handle_atomic_response(const DecodedPacket &packet, uint64_t now_ns,
                                 NativeTransportOutput &output, Status *status) {
        size_t owner = tx_works.size();
        for (size_t i = 0; i < tx_works.size(); ++i) {
            if ((tx_works[i].posted.kind == WorkKind::COMPARE_SWAP || tx_works[i].posted.kind == WorkKind::FETCH_ADD) &&
                tx_works[i].posted.first_psn == packet.psn) { owner = i; break; }
        }
        if (owner == tx_works.size()) {
            ++counters.duplicate_packets;
            if (!config.legacy_response_acks) { status_ok(status); return true; }
            return emit_control(AETH_ACK, packet.psn, output, status);
        }
        if (!packet.has_atomic_ack || !packet.has_aeth || packet.aeth.syndrome > 0x1f) {
            return fail_all(ErrorCode::REMOTE_OPERATION_ERROR, "invalid atomic response", output, status);
        }
        const TxWork work = tx_works[owner];
        const uint64_t original = packet.atomic_original_value;
        const ByteSpan value(reinterpret_cast<const uint8_t *>(&original), 8);
        if (work.local_lkey ? !memory->write_local_key(work.local_lkey, qp.runtime_view().config.protection_domain,
                                                      work.local_address, value, status)
                           : !memory->write(work.local_address, value, MemoryAccess::LOCAL_WRITE, status)) return false;
        const uint32_t old_ack = qp.runtime_view().last_acked_psn;
        if (!qp.set_atomic_result(work.posted.id, original, status) ||
            !qp.record_read_response(work.posted.id, packet.psn, 8, true, status)) return false;
        release_owner_frames(tx_works[owner]);
        tx_works.erase(tx_works.begin() + owner);
        prune_acked_writes(old_ack, packet.psn);
        restart_active_replay();
        retry_remaining = qp.runtime_view().config.retry_count;
        const PsnOrder rnr_order = rnr_waiting ? psn24_compare(packet.psn, rnr_psn) : PsnOrder::EQUAL;
        if (!rnr_waiting || rnr_order == PsnOrder::EQUAL || rnr_order == PsnOrder::AFTER) {
            rnr_retry_remaining = qp.runtime_view().config.rnr_retry_count;
            rnr_waiting = false;
            rnr_resume_ns = 0;
        }
        refresh_deadline(now_ns);
        if (config.legacy_response_acks && !emit_control(AETH_ACK, packet.psn, output, status)) return false;
        const size_t before = output.completions.size();
        if (!collect_completions(output, status)) return false;
        for (size_t i = before; i < output.completions.size(); ++i) {
            if (output.completions[i].id == work.posted.id) output.completions[i].atomic_original_value = original;
        }
        return true;
    }

    bool accept_read_request(const DecodedPacket &packet,
                             uint64_t now_ns,
                             NativeTransportOutput &output,
                             Status *status) {
        if (memory == NULL || !packet.has_reth || staged_write.active) {
            return emit_control(AETH_REMOTE_OPERATION,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        Status access;
        if (!validate_local_window(packet.reth.virtual_address,
                                   packet.reth.dma_length,
                                   packet.reth.rkey,
                                   MemoryAccess::REMOTE_READ,
                                   &access)) {
            return emit_control(AETH_REMOTE_ACCESS,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        const QpRuntimeView snapshot = qp.runtime_view();
        if (snapshot.config.max_dest_rd_atomic == 0) return emit_control(AETH_INVALID_REQUEST, packet.psn, output, status);
        const uint64_t count64 = packet.reth.dma_length == 0
                                     ? 1
                                     : 1 + ((packet.reth.dma_length - 1) /
                                            snapshot.negotiated_pmtu);
        if (count64 > config.max_tx_frames ||
            count64 > PSN24_MAX_SERIAL_SPAN ||
            packet.reth.dma_length > config.max_read_cache_bytes) {
            return emit_control(AETH_REMOTE_OPERATION,
                                qp.runtime_view().expected_rx_psn,
                                output,
                                status);
        }
        if (!config.legacy_response_acks &&
            (packet.reth.dma_length > config.max_read_cache_bytes / snapshot.config.max_dest_rd_atomic ||
             count64 > config.max_tx_frames / snapshot.config.max_dest_rd_atomic)) {
            return emit_control(AETH_REMOTE_OPERATION, packet.psn, output, status);
        }
        recycle_response_slot();
        if (read_cache.size() >= config.max_read_cache_entries ||
            read_cache.size() >= snapshot.config.max_dest_rd_atomic ||
            packet.reth.dma_length > config.max_read_cache_bytes -
                                         read_cache_bytes) {
            counters.receive_credit_stalls++;
            return emit_control(static_cast<uint8_t>(
                                    AETH_RNR_BASE |
                                    snapshot.config.min_rnr_timer),
                                packet.psn,
                                output,
                                status);
        }
        if (!retained_available(static_cast<uint32_t>(count64), status)) {
            counters.receive_credit_stalls++;
            return emit_control(static_cast<uint8_t>(
                                    AETH_RNR_BASE |
                                    snapshot.config.min_rnr_timer),
                                packet.psn,
                                output,
                                status);
        }
        ByteSpan value;
        if (!memory->view_remote_key(packet.reth.rkey, packet.reth.virtual_address,
                          packet.reth.dma_length,
                          MemoryAccess::REMOTE_READ,
                          value,
                          status)) {
            return false;
        }
        ReadCacheEntry candidate;
        candidate.first_psn = packet.psn;
        if (!psn24_add(packet.psn,
                       static_cast<int64_t>(count64) - 1,
                       candidate.terminal_psn,
                       status)) {
            return false;
        }
        candidate.address = packet.reth.virtual_address;
        candidate.rkey = packet.reth.rkey;
        candidate.length = packet.reth.dma_length;
        candidate.payload_bytes = packet.reth.dma_length;
        frame_scratch.clear();
        size_t offset = 0;
        for (uint32_t index = 0; index < static_cast<uint32_t>(count64); index++) {
            const size_t chunk = std::min<size_t>(
                value.size - offset, snapshot.negotiated_pmtu);
            IbOpcode opcode = IB_RDMA_READ_RESPONSE_MIDDLE;
            if (count64 == 1) {
                opcode = IB_RDMA_READ_RESPONSE_ONLY;
            } else if (index == 0) {
                opcode = IB_RDMA_READ_RESPONSE_FIRST;
            } else if (index + 1 == count64) {
                opcode = IB_RDMA_READ_RESPONSE_LAST;
            }
            uint32_t psn = 0;
            if (!psn24_add(packet.psn, index, psn, status)) {
                return false;
            }
            PacketEncodeSpec spec = base_spec(opcode, psn);
            if (opcode != IB_RDMA_READ_RESPONSE_MIDDLE) {
                spec.has_aeth = true;
                spec.aeth.syndrome = AETH_ACK;
                spec.aeth.msn = responder_msn;
            }
            spec.ack_request = config.legacy_response_acks && index + 1 == count64;
            spec.payload = ByteSpan(chunk == 0 ? NULL : value.data + offset,
                                    chunk);
            spec.ipv4_identification = static_cast<uint16_t>(psn);
            NativeTransportFrame frame;
            if (!encode_frame(spec, WorkId(), false, frame, status)) {
                return false;
            }
            frame_scratch.push_back(std::move(frame));
            offset += chunk;
        }
        if (!preflight_publication(frame_scratch.size(),
                                   now_ns,
                                   output,
                                   status) ||
            !retain_scratch(candidate, status)) {
            return false;
        }
        read_cache.push_back(std::move(candidate));
        read_cache_bytes += packet.reth.dma_length;
        if (!qp.accept_rx_span(packet.psn,
                               static_cast<uint32_t>(count64),
                               status)) {
            read_cache_bytes -= read_cache.back().payload_bytes;
            release_owner_frames(read_cache.back());
            read_cache.pop_back();
            return false;
        }
        has_gap_nak = false;
        counters.read_requests_received++;
        counters.read_response_packets += frame_scratch.size();
        counters.read_response_bytes += packet.reth.dma_length;
        return publish_or_queue(frame_scratch, now_ns, output, status);
    }

    size_t oldest_read_work_index() const {
        for (size_t index = 0; index < tx_works.size(); index++) {
            if (tx_works[index].posted.kind == WorkKind::READ) {
                return index;
            }
        }
        return tx_works.size();
    }

    size_t read_work_index(uint32_t psn) const {
        for (size_t index = 0; index < tx_works.size(); index++) {
            if (tx_works[index].posted.kind == WorkKind::READ &&
                psn_interval_contains(psn,
                                      tx_works[index].posted.first_psn,
                                      tx_works[index].posted.terminal_psn)) {
                return index;
            }
        }
        return tx_works.size();
    }

    size_t deferred_read_index(uint32_t psn) const {
        for (size_t index = 0; index < deferred_reads.size(); index++) {
            if (deferred_reads[index].psn == psn) {
                return index;
            }
        }
        return deferred_reads.size();
    }

    bool emit_read_gap(uint32_t expected_psn,
                       uint32_t received_psn,
                       NativeTransportOutput &output,
                       Status *status) {
        counters.out_of_order_packets++;
        if (!has_read_gap_nak || read_gap_psn != expected_psn) {
            uint32_t distance = 0;
            if (!psn24_forward_distance(expected_psn,
                                        received_psn,
                                        distance,
                                        status)) {
                return false;
            }
            counters.psn_gap_events++;
            counters.packets_lost_detected += distance;
            has_read_gap_nak = true;
            read_gap_psn = expected_psn;
            if (!config.legacy_response_acks) { status_ok(status); return true; }
            return emit_control(AETH_SEQUENCE_NAK, expected_psn, output, status);
        }
        status_ok(status);
        return true;
    }

    bool buffer_read_response(const DecodedPacket &packet,
                              ByteSpan wire_frame,
                              uint32_t expected_psn,
                              NativeTransportOutput &output,
                              Status *status) {
        const size_t existing = deferred_read_index(packet.psn);
        if (existing != deferred_reads.size()) {
            const NativeFrameBuffer &bytes = deferred_reads[existing].bytes;
            if (bytes.size() == wire_frame.size &&
                memcmp(bytes.data(), wire_frame.data, wire_frame.size) == 0) {
                counters.duplicate_packets++;
            } else {
                counters.malformed_packets++;
            }
            return emit_read_gap(expected_psn, packet.psn, output, status);
        }
        if (retained_in_use + deferred_reads.size() >= config.max_tx_frames ||
            wire_frame.size > config.max_read_cache_bytes -
                                  std::min<uint64_t>(deferred_read_bytes,
                                                     config.max_read_cache_bytes)) {
            counters.receive_credit_stalls++;
            return emit_read_gap(expected_psn, packet.psn, output, status);
        }
        DeferredReadFrame deferred;
        deferred.psn = packet.psn;
        if (!frame_pool.copy(wire_frame.data,
                             wire_frame.size,
                             deferred.bytes,
                             status)) {
            return false;
        }
        deferred_read_bytes += wire_frame.size;
        deferred_reads.push_back(std::move(deferred));
        return emit_read_gap(expected_psn, packet.psn, output, status);
    }

    bool process_expected_read_response(const DecodedPacket &packet,
                                        ByteSpan wire_frame,
                                        uint64_t now_ns,
                                        NativeTransportOutput &output,
                                        Status *status) {
        const size_t work_index = oldest_read_work_index();
        if (work_index == tx_works.size()) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        TxWork &work = tx_works[work_index];
        if (packet.psn != work.next_read_psn ||
            packet.payload.end() > wire_frame.size) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        const uint32_t ordinal = work.read_psns;
        IbOpcode expected_opcode = IB_RDMA_READ_RESPONSE_MIDDLE;
        if (work.posted.expected_read_psns == 1) {
            expected_opcode = IB_RDMA_READ_RESPONSE_ONLY;
        } else if (ordinal == 0) {
            expected_opcode = IB_RDMA_READ_RESPONSE_FIRST;
        } else if (ordinal + 1 == work.posted.expected_read_psns) {
            expected_opcode = IB_RDMA_READ_RESPONSE_LAST;
        }
        const uint64_t remaining = work.length - work.read_bytes;
        const size_t expected_bytes = static_cast<size_t>(std::min<uint64_t>(
            remaining, qp.runtime_view().negotiated_pmtu));
        if (packet.opcode != expected_opcode ||
            packet.payload.length != expected_bytes ||
            ((packet.opcode == IB_RDMA_READ_RESPONSE_FIRST ||
              packet.opcode == IB_RDMA_READ_RESPONSE_LAST ||
              packet.opcode == IB_RDMA_READ_RESPONSE_ONLY) &&
             (!packet.has_aeth || packet.aeth.syndrome > 0x1f))) {
            return fail_all(ErrorCode::REMOTE_OPERATION_ERROR,
                            "malformed RDMA Read response sequence",
                            output,
                            status);
        }
        uint64_t destination = 0;
        if (!add_u64(work.local_address, work.read_bytes, destination)) {
            return fail_all(ErrorCode::OUT_OF_RANGE,
                            "RDMA Read destination address overflowed",
                            output,
                            status);
        }
        const ByteSpan payload(expected_bytes == 0
                                   ? NULL
                                   : wire_frame.data + packet.payload.offset,
                               expected_bytes);
        if (memory == NULL || (work.local_lkey
                ? !memory->write_local_key(work.local_lkey, qp.runtime_view().config.protection_domain,
                                           destination, payload, status)
                : !memory->write(destination, payload, MemoryAccess::LOCAL_WRITE, status))) {
            return false;
        }
        const bool last = ordinal + 1 == work.posted.expected_read_psns;
        const WorkId id = work.posted.id;
        const uint32_t old_ack = qp.runtime_view().last_acked_psn;
        if (!qp.record_read_response(id,
                                     packet.psn,
                                     static_cast<uint32_t>(expected_bytes),
                                     last,
                                     status)) {
            return false;
        }
        work.read_bytes += expected_bytes;
        work.read_psns++;
        if (!psn24_add(packet.psn, 1, work.next_read_psn, status)) {
            return false;
        }
        counters.read_response_packets++;
        counters.read_response_bytes += expected_bytes;
        has_read_gap_nak = false;
        retry_remaining = qp.runtime_view().config.retry_count;
        const PsnOrder rnr_order =
            rnr_waiting ? psn24_compare(packet.psn, rnr_psn) :
                          PsnOrder::EQUAL;
        const bool response_covers_rnr =
            !rnr_waiting || rnr_order == PsnOrder::EQUAL ||
            rnr_order == PsnOrder::AFTER;
        if (response_covers_rnr) {
            rnr_retry_remaining = qp.runtime_view().config.rnr_retry_count;
            rnr_waiting = false;
            rnr_resume_ns = 0;
        }
        if (last) {
            release_owner_frames(work);
            tx_works.erase(tx_works.begin() + work_index);
        }
        prune_acked_writes(old_ack, packet.psn);
        restart_active_replay();
        refresh_deadline(now_ns);
        if (config.legacy_response_acks && packet.ack_request &&
            !emit_control(AETH_ACK, packet.psn, output, status)) {
            return false;
        }
        return collect_completions(output, status);
    }

    bool drain_deferred_read_responses(uint64_t now_ns,
                                       NativeTransportOutput &output,
                                       Status *status) {
        while (true) {
            const size_t work_index = oldest_read_work_index();
            if (work_index == tx_works.size()) {
                status_ok(status);
                return true;
            }
            const size_t deferred_index =
                deferred_read_index(tx_works[work_index].next_read_psn);
            if (deferred_index == deferred_reads.size()) {
                status_ok(status);
                return true;
            }
            NativeFrameBuffer bytes =
                std::move(deferred_reads[deferred_index].bytes);
            deferred_read_bytes -= bytes.size();
            deferred_reads.erase(deferred_reads.begin() + deferred_index);
            DecodedPacket packet;
            CodecError error;
            if (!NativePacketCodec::decode(ByteSpan(bytes.data(), bytes.size()),
                                           packet,
                                           &error)) {
                counters.malformed_packets++;
                continue;
            }
            if (!process_expected_read_response(packet,
                                                ByteSpan(bytes.data(),
                                                         bytes.size()),
                                                now_ns,
                                                output,
                                                status)) {
                return false;
            }
        }
    }

    bool handle_read_response(const DecodedPacket &packet,
                              ByteSpan wire_frame,
                              uint64_t now_ns,
                              NativeTransportOutput &output,
                              Status *status) {
        const size_t oldest = oldest_read_work_index();
        if (oldest == tx_works.size()) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        const PsnOrder oldest_order =
            psn24_compare(packet.psn, tx_works[oldest].next_read_psn);
        if (oldest_order == PsnOrder::BEFORE) {
            counters.duplicate_packets++;
            uint32_t ack_psn = 0;
            if (!psn24_add(tx_works[oldest].next_read_psn,
                           -1,
                           ack_psn,
                           status)) {
                return false;
            }
            if (!config.legacy_response_acks) { status_ok(status); return true; }
            return emit_control(AETH_ACK, ack_psn, output, status);
        }
        const size_t owner = read_work_index(packet.psn);
        if (owner == tx_works.size()) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        if (owner != oldest || oldest_order == PsnOrder::AFTER) {
            return buffer_read_response(packet,
                                        wire_frame,
                                        tx_works[oldest].next_read_psn,
                                        output,
                                        status);
        }
        if (oldest_order != PsnOrder::EQUAL) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        if (!process_expected_read_response(packet,
                                            wire_frame,
                                            now_ns,
                                            output,
                                            status)) {
            return false;
        }
        return drain_deferred_read_responses(now_ns, output, status);
    }

    bool handle_ack(const DecodedPacket &packet,
                    uint64_t now_ns,
                    NativeTransportOutput &output,
                    Status *status) {
        const uint32_t old_ack = qp.runtime_view().last_acked_psn;
        const PsnOrder order = psn24_compare(packet.psn, old_ack);
        if (order == PsnOrder::EQUAL || order == PsnOrder::BEFORE) {
            counters.duplicate_acks++;
        }
        if (!qp.acknowledge(packet.psn, status)) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        counters.acks_received++;
        if (order == PsnOrder::AFTER) {
            const PsnOrder rnr_order =
                rnr_waiting ? psn24_compare(packet.psn, rnr_psn) :
                              PsnOrder::EQUAL;
            const bool ack_covers_rnr =
                !rnr_waiting || rnr_order == PsnOrder::EQUAL ||
                rnr_order == PsnOrder::AFTER;
            prune_acked_writes(old_ack, packet.psn);
            restart_active_replay();
            retry_remaining = qp.runtime_view().config.retry_count;
            /* An advancing cumulative ACK can still precede the PSN rejected
             * by an already-observed RNR NAK.  Preserve that later WQE's RNR
             * pause and consumed retry budget until an ACK actually covers
             * the rejected PSN. */
            if (ack_covers_rnr) {
                rnr_retry_remaining =
                    qp.runtime_view().config.rnr_retry_count;
                rnr_waiting = false;
                rnr_resume_ns = 0;
            }
            refresh_deadline(now_ns);
        }
        return collect_completions(output, status);
    }

    bool handle_aeth(const DecodedPacket &packet,
                     uint64_t now_ns,
                     NativeTransportOutput &output,
                     Status *status) {
        if (!packet.has_aeth) {
            counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        const uint8_t syndrome = packet.aeth.syndrome;
        if (syndrome <= 0x1f) {
            bool requester_ack = false;
            for (size_t index = 0; index < tx_works.size(); index++) {
                if (work_completes_by_ack(tx_works[index].posted.kind) &&
                    psn_interval_contains(packet.psn,
                                          tx_works[index].posted.first_psn,
                                          tx_works[index].posted.terminal_psn)) {
                    requester_ack = true;
                    break;
                }
            }
            /* ACK direction is resolved against active requester WRITE work,
             * never by PSN alone.  A responder read-cache entry can legally
             * use the same PSN as this endpoint's SQ. */
            if (requester_ack) {
                return handle_ack(packet, now_ns, output, status);
            }
            if (config.legacy_response_acks && cache_contains(packet.psn)) {
                counters.acks_received++;
                acknowledge_read_cache(packet.psn);
                status_ok(status);
                return true;
            }
            counters.duplicate_acks++;
            status_ok(status);
            return true;
        }
        counters.naks_received++;
        if (syndrome >= 0x20 && syndrome <= 0x3f) {
            counters.rnr_naks_received++;
            if (tx_works.empty()) {
                counters.malformed_packets++;
                status_ok(status);
                return true;
            }
            bool owns_live_psn = false;
            for (size_t index = 0; index < tx_works.size(); index++) {
                if (work_owns_live_psn(tx_works[index], packet.psn)) {
                    owns_live_psn = true;
                    break;
                }
            }
            if (!owns_live_psn) {
                /* Never replace an active retry deadline with an RNR timer
                 * that cannot replay any admitted requester work. */
                counters.malformed_packets++;
                status_ok(status);
                return true;
            }
            if (rnr_retry_remaining == 0) {
                counters.rnr_retry_exhausted++;
                return fail_all(ErrorCode::RNR_RETRY_EXHAUSTED,
                                "RNR retry budget exhausted",
                                output,
                                status);
            }
            /* The all-ones verbs retry value means unlimited retries. */
            if (rnr_retry_remaining != 7) {
                rnr_retry_remaining--;
            }
            /* RNR suspends the complete requester send stream, including a
             * replay that still has chunks not yet published.  Engine
             * admission has already drained this QP's published frames; the
             * resume timer reconstructs replay from rnr_psn. */
            replay.clear();
            rnr_waiting = true;
            rnr_psn = packet.psn;
            rnr_resume_ns = saturated_deadline(
                now_ns, RNR_TIMEOUT_NS[syndrome & 0x1f]);
            oldest_deadline_ns = 0;
            status_ok(status);
            return true;
        }
        if (syndrome == AETH_SEQUENCE_NAK) {
            counters.sequence_naks_received++;
            if (config.legacy_response_acks && cache_contains(packet.psn)) {
                return emit_cached_from(packet.psn, now_ns, output, status);
            }
            bool owns_live_psn = false;
            for (size_t index = 0; index < tx_works.size(); index++) {
                if (work_owns_live_psn(tx_works[index], packet.psn)) {
                    owns_live_psn = true;
                    break;
                }
            }
            if (!owns_live_psn) {
                /* A stale or future NAK must not consume the retry budget or
                 * cancel a replay belonging to currently outstanding work. */
                counters.malformed_packets++;
                status_ok(status);
                return true;
            }
            if (retry_remaining == 0) {
                counters.retry_exhausted++;
                return fail_all(ErrorCode::RETRY_EXHAUSTED,
                                "sequence retry budget exhausted",
                                output,
                                status);
            }
            if (retry_remaining != 7) {
                retry_remaining--;
            }
            counters.nak_retries++;
            return retransmit_from(packet.psn,
                                   now_ns,
                                   config.max_tx_frames,
                                   output,
                                   status);
        }
        if (syndrome == AETH_INVALID_REQUEST ||
            syndrome == AETH_REMOTE_ACCESS ||
            syndrome == AETH_REMOTE_OPERATION) {
            counters.fatal_naks_received++;
            ErrorCode error = ErrorCode::INVALID_REQUEST;
            if (syndrome == AETH_REMOTE_ACCESS) {
                error = ErrorCode::REMOTE_ACCESS_ERROR;
            } else if (syndrome == AETH_REMOTE_OPERATION) {
                error = ErrorCode::REMOTE_OPERATION_ERROR;
            }
            return fail_all(error,
                            "fatal AETH NAK received from peer",
                            output,
                            status);
        }
        counters.malformed_packets++;
        status_ok(status);
        return true;
    }
};

NativeRcTransport::NativeRcTransport(QueuePair &qp) :
    m_impl(new (std::nothrow) Impl(qp)) {
}

NativeRcTransport::~NativeRcTransport() {
    detach();
    delete m_impl;
    m_impl = NULL;
}

bool NativeRcTransport::estimate_storage_bytes(
    const QpConfig &qp_config,
    const NativeTransportConfig &transport_config,
    uint64_t &bytes,
    Status *status) {
    bytes = 0;
    if (!qp_config.validate(status) || !transport_config.validate(status)) {
        return false;
    }

    const uint64_t retained_capacity =
        static_cast<uint64_t>(qp_config.sq_depth_packets) +
        static_cast<uint64_t>(transport_config.max_tx_frames);
    const uint64_t pool_capacity = retained_capacity + 8U;
    const uint64_t stride = static_cast<uint64_t>(qp_config.pmtu) + 128U;
    if (retained_capacity > std::numeric_limits<uint32_t>::max() ||
        pool_capacity > std::numeric_limits<uint32_t>::max() ||
        stride > TREX_MAX_ROCEV2_FRAME_SIZE ||
        transport_config.max_staged_write_bytes >
            static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "native RC preallocated storage bounds overflow");
    }

    uint64_t pool_bytes = 0;
    if (!NativeFramePool::estimate_storage_bytes(
            static_cast<uint32_t>(pool_capacity),
            static_cast<uint32_t>(stride),
            pool_bytes,
            status) ||
        !add_storage_reservation(1,
                                 sizeof(NativeRcTransport),
                                 bytes,
                                 status,
                                 "native RC transport object") ||
        !add_storage_reservation(1,
                                 sizeof(Impl),
                                 bytes,
                                 status,
                                 "native RC transport implementation") ||
        !add_storage_reservation(qp_config.rq_depth_messages,
                                 sizeof(Impl::ReceiveSlot) + sizeof(CompletionRecord),
                                 bytes, status, "native RC receive descriptors and CQ") ||
        !add_storage_reservation(qp_config.sq_depth_messages,
                                 sizeof(Impl::TxWork),
                                 bytes,
                                 status,
                                 "native RC transmit-work vector") ||
        !add_storage_reservation(transport_config.max_tx_frames,
                                 sizeof(NativeTransportFrame),
                                 bytes,
                                 status,
                                 "native RC pending-frame vector") ||
        !add_storage_reservation(transport_config.max_read_cache_entries,
                                 sizeof(Impl::ReadCacheEntry),
                                 bytes,
                                 status,
                                 "native RC read-cache vector") ||
        !add_storage_reservation(transport_config.max_tx_frames,
                                 sizeof(Impl::DeferredReadFrame),
                                 bytes,
                                 status,
                                 "native RC deferred-read vector") ||
        !add_storage_reservation(transport_config.max_tx_frames,
                                 sizeof(NativeTransportFrame),
                                 bytes,
                                 status,
                                 "native RC frame-scratch vector") ||
        !add_storage_reservation(retained_capacity,
                                 sizeof(Impl::RetainedFrame),
                                 bytes,
                                 status,
                                 "native RC retained-frame vector") ||
        !add_storage_reservation(1,
                                 transport_config.max_staged_write_bytes,
                                 bytes,
                                 status,
                                 "native RC staged-write vector") ||
        !add_storage_reservation(1,
                                 pool_bytes,
                                 bytes,
                                 status,
                                 "native RC frame pool")) {
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeRcTransport::configure(const NativeTransportConfig &config,
                                  Status *status) {
    try {
        if (m_impl == NULL) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native RC transport allocation failed");
        }
        if (m_impl->configured) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport is already configured");
        }
        if ((m_impl->qp.state() != QpState::RTS && m_impl->qp.state() != QpState::RTR) || !config.validate(status)) {
            if (m_impl->qp.state() != QpState::RTS && m_impl->qp.state() != QpState::RTR) {
                return status_error(status,
                                    ErrorCode::INVALID_STATE,
                                    "native RC transport requires an RTS QP");
            }
            return false;
        }
        const QpSnapshot qp = m_impl->qp.snapshot();
        if (!config.legacy_response_acks && qp.config.max_dest_rd_atomic &&
            (config.max_read_cache_entries < qp.config.max_dest_rd_atomic ||
             config.max_tx_frames < qp.config.max_dest_rd_atomic ||
             config.max_read_cache_bytes / qp.config.max_dest_rd_atomic < 8)) {
            return status_error(status, ErrorCode::INVALID_ARGUMENT, "replay cache is smaller than advertised responder depth");
        }
        if (!qp.has_config || !qp.has_peer || qp.negotiated_pmtu == 0 ||
            config.max_tx_frames < 1) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport QP is incomplete");
        }
        const uint64_t retained_capacity64 =
            static_cast<uint64_t>(qp.config.sq_depth_packets) +
            static_cast<uint64_t>(config.max_tx_frames);
        const uint64_t pool_capacity64 = retained_capacity64 + 8U;
        const uint64_t stride64 =
            static_cast<uint64_t>(qp.negotiated_pmtu) + 128U;
        if (retained_capacity64 > std::numeric_limits<uint32_t>::max() ||
            pool_capacity64 > std::numeric_limits<uint32_t>::max() ||
            stride64 > TREX_MAX_ROCEV2_FRAME_SIZE ||
            config.max_staged_write_bytes >
                static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "native RC preallocated storage bounds overflow");
        }
        m_impl->tx_works.reserve(qp.config.sq_depth_messages);
        m_impl->receive_completions.reserve(qp.config.rq_depth_messages);
        if (!m_impl->receives.configure(qp.config.rq_depth_messages, status)) return false;
        m_impl->pending_frames.reserve(config.max_tx_frames);
        m_impl->read_cache.reserve(config.max_read_cache_entries);
        m_impl->deferred_reads.reserve(config.max_tx_frames);
        m_impl->frame_scratch.reserve(config.max_tx_frames);
        const uint32_t retained_capacity =
            static_cast<uint32_t>(retained_capacity64);
        m_impl->retained_frames.resize(retained_capacity);
        m_impl->retained_free_head = retained_capacity == 0
                                         ? Impl::INVALID_FRAME
                                         : 0;
        for (uint32_t index = 0; index < retained_capacity; ++index) {
            m_impl->retained_frames[index].next =
                index + 1U == retained_capacity
                    ? Impl::INVALID_FRAME
                    : index + 1U;
        }
        m_impl->staged_write.bytes.resize(
            static_cast<size_t>(config.max_staged_write_bytes));
        if (!m_impl->frame_pool.configure(
                static_cast<uint32_t>(pool_capacity64),
                static_cast<uint32_t>(stride64),
                status)) {
            return false;
        }
        m_impl->config = config;
        m_impl->receive_credits = config.receive_credits;
        m_impl->retry_remaining = qp.config.retry_count;
        m_impl->rnr_retry_remaining = qp.config.rnr_retry_count;
        m_impl->configured = true;
        m_impl->detached = false;
        status_ok(status);
        return true;
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC transport configuration allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC transport configuration exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC transport configuration failure");
    }
}

bool NativeRcTransport::bind_memory(NativeTransportMemory *memory,
                                    Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status)) {
            return false;
        }
        if (memory == NULL) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native RC transport memory is null");
        }
        if (m_impl->memory != NULL) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport memory is already bound");
        }
        m_impl->memory = memory;
        status_ok(status);
        return true;
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC memory binding failure");
    }
}

bool NativeRcTransport::post_receive(uint32_t credits, Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status)) {
            return false;
        }
        if (credits == 0) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "receive credit count must be non-zero");
        }
        if (m_impl->config.unlimited_receive_credits) {
            status_ok(status);
            return true;
        }
        if (credits > std::numeric_limits<uint32_t>::max() -
                          m_impl->receive_credits) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "receive credit count overflowed");
        }
        m_impl->receive_credits += credits;
        status_ok(status);
        return true;
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected receive-credit failure");
    }
}

bool NativeRcTransport::post_receive(const NativeReceiveRequest &request, Status *status) {
    try {
        if (!m_impl || !m_impl->check_active(status) || !m_impl->memory) return false;
        if (!request.signalled) return status_error(status, ErrorCode::INVALID_ARGUMENT, "receive WQEs always produce completion records");
        const QpConfig config = m_impl->qp.runtime_view().config;
        const size_t active_receive = m_impl->staged_write.active && m_impl->staged_write.send ? 1 : 0;
        if (m_impl->receives.size() + m_impl->receive_completions.size() + active_receive >= config.rq_depth_messages ||
            request.sges.size() > config.max_receive_sge || m_impl->next_receive_sequence == 0) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "receive queue, SGE or identity bound is exhausted");
        }
        Impl::ReceiveSlot receive;
        receive.wr_id = request.wr_id;
        receive.signalled = request.signalled;
        receive.sequence = m_impl->next_receive_sequence;
        receive.count = static_cast<uint32_t>(request.sges.size());
        for (size_t i = 0; i < request.sges.size(); ++i) {
            const NativeScatterGatherEntry &sge = request.sges[i];
            if (sge.length > m_impl->config.max_staged_write_bytes - receive.capacity) {
                return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "receive exceeds staged message bound");
            }
            if (!m_impl->memory->validate_local_key(sge.lkey, config.protection_domain,
                                                    sge.address, sge.length, status)) return false;
            receive.sges[i] = sge;
            receive.capacity += sge.length;
        }
        if (!m_impl->receives.push(receive, status)) return false;
        ++m_impl->next_receive_sequence;
        ++m_impl->counters.receive_wqes_posted;
        status_ok(status);
        return true;
    } catch (...) {
        return status_error(status, ErrorCode::INTERNAL_ERROR, "unexpected receive posting failure");
    }
}

bool NativeRcTransport::post_send(const NativeTransportSendRequest &request,
                                  ByteSpan payload, uint64_t now_ns,
                                  PostedWork &posted, NativeTransportOutput &output,
                                  Status *status) {
    try {
        if (!m_impl || !m_impl->check_active(status) || !m_impl->check_time(now_ns, status) ||
            !m_impl->check_fence(request.fenced, status)) return false;
        if ((payload.size && !payload.data) || (request.with_immediate && request.with_invalidate) ||
            (request.with_invalidate && request.invalidate_rkey == 0)) {
            return status_error(status, ErrorCode::INVALID_ARGUMENT, "SEND payload or terminal variants are invalid");
        }
        const QpRuntimeView qp = m_impl->qp.runtime_view();
        const uint64_t count64 = payload.size == 0 ? 1 : 1 + (payload.size - 1) / qp.negotiated_pmtu;
        if (count64 > m_impl->config.max_tx_frames || count64 > PSN24_MAX_SERIAL_SPAN ||
            m_impl->tx_works.size() >= qp.config.sq_depth_messages) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SEND exceeds bounded SQ or transmit batch");
        }
        const uint32_t count = static_cast<uint32_t>(count64);
        if (!m_impl->retained_available(count, status)) return false;
        Impl::TxWork candidate;
        candidate.posted.id = WorkId(qp.id, qp.next_work_sequence);
        candidate.posted.kind = WorkKind::SEND;
        candidate.posted.first_psn = qp.next_sq_psn;
        candidate.posted.reserved_psns = count;
        candidate.posted.payload_bytes = payload.size;
        candidate.posted.signalled = request.signalled;
        candidate.posted_ns = now_ns;
        candidate.length = payload.size;
        if (!psn24_add(qp.next_sq_psn, count - 1, candidate.posted.terminal_psn, status)) return false;
        m_impl->frame_scratch.clear();
        size_t offset = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const bool last = i + 1 == count;
            IbOpcode opcode = count == 1 ? IB_SEND_ONLY : (i == 0 ? IB_SEND_FIRST : (last ? IB_SEND_LAST : IB_SEND_MIDDLE));
            if (last && request.with_immediate) opcode = count == 1 ? IB_SEND_ONLY_WITH_IMMEDIATE : IB_SEND_LAST_WITH_IMMEDIATE;
            if (last && request.with_invalidate) opcode = count == 1 ? IB_SEND_ONLY_WITH_INVALIDATE : IB_SEND_LAST_WITH_INVALIDATE;
            uint32_t psn = 0;
            if (!psn24_add(qp.next_sq_psn, i, psn, status)) return false;
            PacketEncodeSpec spec = m_impl->base_spec(opcode, psn);
            spec.ack_request = last || (m_impl->config.ack_request_interval &&
                                         (i + 1) % m_impl->config.ack_request_interval == 0);
            spec.has_immediate_data = last && request.with_immediate;
            spec.immediate_data = request.immediate_data;
            spec.has_invalidate = last && request.with_invalidate;
            spec.invalidate_rkey = request.invalidate_rkey;
            const size_t chunk = std::min<size_t>(payload.size - offset, qp.negotiated_pmtu);
            spec.payload = ByteSpan(chunk ? payload.data + offset : NULL, chunk);
            NativeTransportFrame frame;
            if (!m_impl->encode_frame(spec, candidate.posted.id, false, frame, status)) return false;
            m_impl->frame_scratch.push_back(std::move(frame));
            offset += chunk;
        }
        if (!m_impl->preflight_publication(count, now_ns, output, status) ||
            !m_impl->retain_scratch(candidate, status)) return false;
        m_impl->tx_works.push_back(std::move(candidate));
        if (!m_impl->qp.post_send(payload.size, request.signalled, posted, status)) {
            m_impl->release_owner_frames(m_impl->tx_works.back());
            m_impl->tx_works.pop_back();
            return false;
        }
        m_impl->tx_works.back().posted = posted;
        ++m_impl->counters.send_messages_posted;
        return m_impl->publish_or_queue(m_impl->frame_scratch, now_ns, output, status);
    } catch (const std::bad_alloc &) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SEND allocation failed");
    }
}

bool NativeRcTransport::post_atomic(const NativeTransportAtomicRequest &request,
                                    uint64_t now_ns, PostedWork &posted,
                                    NativeTransportOutput &output, Status *status) {
    try {
        if (!m_impl || !m_impl->check_active(status) || !m_impl->check_time(now_ns, status) ||
            !m_impl->check_fence(request.fenced, status)) return false;
        const QpRuntimeView qp = m_impl->qp.runtime_view();
        const uint32_t outstanding = qp.ledger.outstanding_responses;
        if (outstanding >= qp.config.max_rd_atomic || m_impl->tx_works.size() >= qp.config.sq_depth_messages) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "shared Read/Atomic requester limit is exhausted");
        }
        if ((request.remote_address & 7U) || (request.local_address & 7U) ||
            (request.fetch_add && request.compare != 0)) {
            return status_error(status, ErrorCode::INVALID_ARGUMENT, "atomic addresses or reserved compare are invalid");
        }
        ByteSpan local;
        if (!m_impl->memory || (request.local_lkey
            ? !m_impl->memory->validate_local_key(request.local_lkey, qp.config.protection_domain, request.local_address, 8, status)
            : !m_impl->memory->view(request.local_address, 8, MemoryAccess::LOCAL_WRITE, local, status))) return false;
        Impl::TxWork candidate;
        candidate.posted.id = WorkId(qp.id, qp.next_work_sequence);
        candidate.posted.kind = request.fetch_add ? WorkKind::FETCH_ADD : WorkKind::COMPARE_SWAP;
        candidate.posted.first_psn = candidate.posted.terminal_psn = qp.next_sq_psn;
        candidate.posted.reserved_psns = candidate.posted.expected_read_psns = 1;
        candidate.posted.payload_bytes = candidate.posted.expected_read_bytes = 8;
        candidate.posted.signalled = request.signalled;
        candidate.local_address = request.local_address;
        candidate.local_lkey = request.local_lkey;
        candidate.length = 8;
        candidate.next_read_psn = qp.next_sq_psn;
        candidate.posted_ns = now_ns;
        PacketEncodeSpec spec = m_impl->base_spec(request.fetch_add ? IB_FETCH_ADD : IB_COMPARE_SWAP, qp.next_sq_psn);
        spec.has_atomic_eth = true;
        spec.atomic_eth.virtual_address = request.remote_address;
        spec.atomic_eth.rkey = request.remote_rkey;
        spec.atomic_eth.compare = request.compare;
        spec.atomic_eth.swap_add = request.swap_add;
        spec.ack_request = true;
        NativeTransportFrame frame;
        if (!m_impl->encode_frame(spec, candidate.posted.id, false, frame, status)) return false;
        m_impl->frame_scratch.clear();
        m_impl->frame_scratch.push_back(std::move(frame));
        if (!m_impl->preflight_publication(1, now_ns, output, status) ||
            !m_impl->retain_scratch(candidate, status)) return false;
        m_impl->tx_works.push_back(std::move(candidate));
        if (!m_impl->qp.post_atomic(request.remote_address, request.remote_rkey, request.fetch_add,
                                     request.signalled, posted, status)) {
            m_impl->release_owner_frames(m_impl->tx_works.back());
            m_impl->tx_works.pop_back();
            return false;
        }
        m_impl->tx_works.back().posted = posted;
        ++m_impl->counters.atomic_messages_posted;
        return m_impl->publish_or_queue(m_impl->frame_scratch, now_ns, output, status);
    } catch (const std::bad_alloc &) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "atomic posting allocation failed");
    }
}

bool NativeRcTransport::post_write(const NativeTransportWriteRequest &request,
                                   ByteSpan payload,
                                   uint64_t now_ns,
                                   PostedWork &posted,
                                   NativeTransportOutput &output,
                                   Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status) ||
            !m_impl->check_time(now_ns, status)) {
            return false;
        }
        if ((payload.size != 0 && payload.data == NULL) ||
            payload.size > std::numeric_limits<uint32_t>::max()) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native RC write payload is invalid or exceeds RETH length");
        }
        if (!m_impl->check_fence(request.fenced, status)) return false;
        const QpRuntimeView snapshot = m_impl->qp.runtime_view();
        const uint64_t count64 = payload.size == 0
                                     ? 1
                                     : 1 + ((payload.size - 1) /
                                            snapshot.negotiated_pmtu);
        if (count64 > m_impl->config.max_tx_frames ||
            count64 > PSN24_MAX_SERIAL_SPAN) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native RC write cannot fit the bounded transmit batch");
        }
        const uint32_t count = static_cast<uint32_t>(count64);
        if (m_impl->tx_works.size() >= snapshot.config.sq_depth_messages) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "bounded transmit work queue is full");
        }
        Impl::TxWork candidate;
        candidate.posted.id = WorkId(snapshot.id, snapshot.next_work_sequence);
        candidate.posted.kind = WorkKind::WRITE;
        candidate.posted.first_psn = snapshot.next_sq_psn;
        candidate.posted.reserved_psns = count;
        candidate.posted.payload_bytes = payload.size;
        candidate.posted.signalled = request.signalled;
        if (!psn24_add(snapshot.next_sq_psn,
                       static_cast<int64_t>(count) - 1,
                       candidate.posted.terminal_psn,
                       status)) {
            return false;
        }
        candidate.posted_ns = now_ns;
        candidate.remote_address = request.remote_address;
        candidate.remote_rkey = request.remote_rkey;
        candidate.length = payload.size;
        if (!m_impl->retained_available(count, status)) {
            return false;
        }
        m_impl->frame_scratch.clear();
        const bool immediate = request.with_immediate ||
                               m_impl->config.signal_mode ==
                                   NativeTransportSignalMode::IMMEDIATE;
        size_t offset = 0;
        for (uint32_t index = 0; index < count; index++) {
            const size_t remaining = payload.size - offset;
            const size_t chunk_size = std::min<size_t>(
                remaining, snapshot.negotiated_pmtu);
            IbOpcode opcode = IB_RDMA_WRITE_MIDDLE;
            if (count == 1) {
                opcode = immediate ? IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE
                                   : IB_RDMA_WRITE_ONLY;
            } else if (index == 0) {
                opcode = IB_RDMA_WRITE_FIRST;
            } else if (index + 1 == count) {
                opcode = immediate ? IB_RDMA_WRITE_LAST_WITH_IMMEDIATE
                                   : IB_RDMA_WRITE_LAST;
            }
            uint32_t psn = 0;
            if (!psn24_add(snapshot.next_sq_psn, index, psn, status)) {
                return false;
            }
            PacketEncodeSpec spec = m_impl->base_spec(opcode, psn);
            spec.ack_request = index + 1 == count ||
                (m_impl->config.ack_request_interval != 0 &&
                 ((index + 1) % m_impl->config.ack_request_interval) == 0);
            if (opcode == IB_RDMA_WRITE_FIRST || opcode == IB_RDMA_WRITE_ONLY ||
                opcode == IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE) {
                spec.has_reth = true;
                spec.reth.virtual_address = request.remote_address;
                spec.reth.rkey = request.remote_rkey;
                spec.reth.dma_length = static_cast<uint32_t>(payload.size);
            }
            if (has_immediate(opcode)) {
                spec.has_immediate_data = true;
                spec.immediate_data = request.immediate_data;
            }
            spec.payload = ByteSpan(chunk_size == 0 ? NULL : payload.data + offset,
                                    chunk_size);
            spec.ipv4_identification = static_cast<uint16_t>(psn);
            NativeTransportFrame frame;
            if (!m_impl->encode_frame(spec,
                                      candidate.posted.id,
                                      false,
                                      frame,
                                      status)) {
                return false;
            }
            m_impl->frame_scratch.push_back(std::move(frame));
            offset += chunk_size;
        }
        if (!m_impl->preflight_publication(m_impl->frame_scratch.size(),
                                           now_ns,
                                           output,
                                           status) ||
            !m_impl->retain_scratch(candidate, status)) {
            return false;
        }
        m_impl->tx_works.push_back(std::move(candidate));
        PostedWork actual;
        if (!m_impl->qp.post_write(request.remote_address,
                                   request.remote_rkey,
                                   payload.size,
                                   request.signalled,
                                   actual,
                                   status)) {
            m_impl->release_owner_frames(m_impl->tx_works.back());
            m_impl->tx_works.pop_back();
            return false;
        }
        Impl::TxWork &stored = m_impl->tx_works.back();
        if (actual.id != stored.posted.id ||
            actual.first_psn != stored.posted.first_psn ||
            actual.terminal_psn != stored.posted.terminal_psn) {
            m_impl->qp.enter_error(ErrorCode::INTERNAL_ERROR,
                                   "native write preflight diverged from QP ledger",
                                   NULL);
            for (size_t index = 0; index < m_impl->tx_works.size(); ++index) {
                m_impl->release_owner_frames(m_impl->tx_works[index]);
            }
            m_impl->tx_works.clear();
            return status_error(status,
                                ErrorCode::INTERNAL_ERROR,
                                "native write preflight diverged from QP ledger");
        }
        stored.posted = actual;
        posted = actual;
        if (m_impl->tx_works.size() == 1 &&
            !m_impl->pacing_active(now_ns)) {
            m_impl->refresh_deadline(now_ns);
        }
        m_impl->counters.write_messages_posted++;
        return m_impl->publish_or_queue(m_impl->frame_scratch,
                                        now_ns,
                                        output,
                                        status);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC write allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC write exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC write failure");
    }
}

bool NativeRcTransport::post_read(const NativeTransportReadRequest &request,
                                  uint64_t now_ns,
                                  PostedWork &posted,
                                  NativeTransportOutput &output,
                                  Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status) ||
            !m_impl->check_time(now_ns, status)) {
            return false;
        }
        if (request.length > std::numeric_limits<uint32_t>::max()) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "native RC read exceeds RETH length");
        }
        if (!m_impl->check_fence(request.fenced, status)) return false;
        const QpRuntimeView snapshot = m_impl->qp.runtime_view();
        const uint32_t outstanding_reads = snapshot.ledger.outstanding_responses;
        if (outstanding_reads >= snapshot.config.max_rd_atomic ||
            m_impl->config.max_tx_frames < 1) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native RC max_rd_atomic or transmit bound is exhausted");
        }
        const uint64_t response_count64 = request.length == 0
                                              ? 1
                                              : 1 + ((request.length - 1) /
                                                     snapshot.negotiated_pmtu);
        if (response_count64 > PSN24_MAX_SERIAL_SPAN) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "native RC read response span is serially ambiguous");
        }
        if (m_impl->tx_works.size() >= snapshot.config.sq_depth_messages) {
            return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "bounded transmit work queue is full");
        }
        Impl::TxWork candidate;
        candidate.posted.id = WorkId(snapshot.id, snapshot.next_work_sequence);
        candidate.posted.kind = WorkKind::READ;
        candidate.posted.first_psn = snapshot.next_sq_psn;
        candidate.posted.reserved_psns = static_cast<uint32_t>(response_count64);
        candidate.posted.payload_bytes = request.length;
        candidate.posted.expected_read_bytes = request.length;
        candidate.posted.expected_read_psns =
            static_cast<uint32_t>(response_count64);
        candidate.posted.signalled = request.signalled;
        if (!psn24_add(snapshot.next_sq_psn,
                       static_cast<int64_t>(response_count64) - 1,
                       candidate.posted.terminal_psn,
                       status)) {
            return false;
        }
        candidate.posted_ns = now_ns;
        candidate.remote_address = request.remote_address;
        candidate.remote_rkey = request.remote_rkey;
        candidate.local_address = request.local_address;
        candidate.local_lkey = request.local_lkey;
        candidate.length = request.length;
        candidate.next_read_psn = candidate.posted.first_psn;
        PacketEncodeSpec spec = m_impl->base_spec(IB_RDMA_READ_REQUEST,
                                                  candidate.posted.first_psn);
        spec.has_reth = true;
        spec.reth.virtual_address = request.remote_address;
        spec.reth.rkey = request.remote_rkey;
        spec.reth.dma_length = static_cast<uint32_t>(request.length);
        spec.ack_request = true;
        spec.ipv4_identification = static_cast<uint16_t>(candidate.posted.first_psn);
        NativeTransportFrame frame;
        if (!m_impl->encode_frame(spec,
                                  candidate.posted.id,
                                  false,
                                  frame,
                                  status)) {
            return false;
        }
        m_impl->frame_scratch.clear();
        m_impl->frame_scratch.push_back(std::move(frame));
        if (!m_impl->preflight_publication(1, now_ns, output, status) ||
            !m_impl->retain_scratch(candidate, status)) {
            return false;
        }
        m_impl->tx_works.push_back(std::move(candidate));
        PostedWork actual;
        if (!m_impl->qp.post_read(request.remote_address,
                                  request.remote_rkey,
                                  request.length,
                                  request.signalled,
                                  actual,
                                  status)) {
            m_impl->release_owner_frames(m_impl->tx_works.back());
            m_impl->tx_works.pop_back();
            return false;
        }
        Impl::TxWork &stored = m_impl->tx_works.back();
        if (actual.id != stored.posted.id ||
            actual.first_psn != stored.posted.first_psn ||
            actual.terminal_psn != stored.posted.terminal_psn) {
            m_impl->qp.enter_error(ErrorCode::INTERNAL_ERROR,
                                   "native read preflight diverged from QP ledger",
                                   NULL);
            for (size_t index = 0; index < m_impl->tx_works.size(); ++index) {
                m_impl->release_owner_frames(m_impl->tx_works[index]);
            }
            m_impl->tx_works.clear();
            return status_error(status,
                                ErrorCode::INTERNAL_ERROR,
                                "native read preflight diverged from QP ledger");
        }
        stored.posted = actual;
        posted = actual;
        if (m_impl->tx_works.size() == 1 &&
            !m_impl->pacing_active(now_ns)) {
            m_impl->refresh_deadline(now_ns);
        }
        m_impl->counters.read_messages_posted++;
        return m_impl->publish_or_queue(m_impl->frame_scratch,
                                        now_ns,
                                        output,
                                        status);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC read allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC read exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC read failure");
    }
}

bool NativeRcTransport::on_frame(const DecodedPacket &packet,
                                 ByteSpan wire_frame,
                                 uint64_t now_ns,
                                 NativeTransportOutput &output,
                                 Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status) ||
            !m_impl->check_time(now_ns, status)) {
            return false;
        }
        if (m_impl->memory == NULL) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport memory is not bound");
        }
        DecodedPacket verified;
        CodecError codec_error;
        if (!NativePacketCodec::decode(wire_frame, verified, &codec_error)) {
            m_impl->counters.malformed_packets++;
            CodecError icrc_error;
            if (!NativePacketCodec::verify_icrc(wire_frame,
                                                packet,
                                                &icrc_error) &&
                icrc_error.code == CodecErrorCode::BAD_ICRC) {
                m_impl->counters.icrc_errors++;
            }
            status_ok(status);
            return true;
        }
        if (verified.opcode != packet.opcode || verified.psn != packet.psn ||
            verified.dest_qpn != packet.dest_qpn ||
            verified.frame_length != packet.frame_length) {
            m_impl->counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        m_impl->counters.rx_packets++;
        m_impl->counters.rx_bytes += wire_frame.size;
        const QpRuntimeView qp = m_impl->qp.runtime_view();
        if (!endpoint_reverse_matches(m_impl->config.endpoint,
                                      verified.endpoint)) {
            m_impl->counters.endpoint_mismatch_packets++;
            status_ok(status);
            return true;
        }
        if (verified.dest_qpn != qp.config.local_qpn) {
            m_impl->counters.unknown_qp_packets++;
            status_ok(status);
            return true;
        }
        if (verified.pkey != m_impl->config.pkey) {
            m_impl->counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        if (verified.opcode == IB_CNP) {
            m_impl->counters.cnp_received++;
            if (!m_impl->config.cnp_enabled) {
                status_ok(status);
                return true;
            }
            m_impl->rate_scale = std::max(0.01,
                m_impl->rate_scale * m_impl->config.cnp_reaction_factor);
            m_impl->congestion_recovery_ns = saturated_deadline(
                now_ns, m_impl->config.cnp_recovery_ns);
            const uint64_t gate = saturated_deadline(
                now_ns, m_impl->pacing_interval_ns());
            m_impl->next_send_ns = std::max(m_impl->next_send_ns, gate);
            status_ok(status);
            return true;
        }
        if (verified.opcode == IB_ACKNOWLEDGE) {
            return m_impl->handle_aeth(verified, now_ns, output, status);
        }
        if (is_read_response_opcode(verified.opcode)) {
            if (!m_impl->maybe_emit_cnp(verified, now_ns, output, status)) {
                return false;
            }
            return m_impl->handle_read_response(verified,
                                                wire_frame,
                                                now_ns,
                                                output,
                                                status);
        }
        if (verified.opcode == IB_ATOMIC_ACKNOWLEDGE) {
            return m_impl->handle_atomic_response(verified, now_ns, output, status);
        }
        if (!is_write_opcode(verified.opcode) && !is_send_opcode(verified.opcode) &&
            verified.opcode != IB_RDMA_READ_REQUEST && verified.opcode != IB_COMPARE_SWAP &&
            verified.opcode != IB_FETCH_ADD) {
            m_impl->counters.malformed_packets++;
            status_ok(status);
            return true;
        }
        if (!m_impl->maybe_emit_cnp(verified, now_ns, output, status)) {
            return false;
        }
        RxPsnDisposition disposition = RxPsnDisposition::INVALID;
        uint32_t distance = 0;
        if (!m_impl->classify_request_psn(verified.psn,
                                          disposition,
                                          distance,
                                          status)) {
            return false;
        }
        if (disposition == RxPsnDisposition::DUPLICATE) {
            m_impl->counters.duplicate_packets++;
            if ((verified.opcode == IB_RDMA_READ_REQUEST || verified.opcode == IB_COMPARE_SWAP ||
                 verified.opcode == IB_FETCH_ADD) && m_impl->cache_contains(verified.psn)) {
                if (verified.opcode != IB_RDMA_READ_REQUEST) ++m_impl->counters.atomic_replay_hits;
                return m_impl->emit_cached_from(verified.psn,
                                                now_ns,
                                                output,
                                                status);
            }
            uint32_t ack_psn = 0;
            if (!psn24_add(qp.expected_rx_psn, -1, ack_psn, status)) {
                return false;
            }
            return m_impl->emit_control(AETH_ACK, ack_psn, output, status);
        }
        if (disposition == RxPsnDisposition::GAP) {
            m_impl->counters.out_of_order_packets++;
            if (!m_impl->has_gap_nak ||
                m_impl->gap_nak_epsn != qp.expected_rx_psn) {
                m_impl->has_gap_nak = true;
                m_impl->gap_nak_epsn = qp.expected_rx_psn;
                m_impl->counters.psn_gap_events++;
                m_impl->counters.packets_lost_detected += distance;
                m_impl->recovery_remaining = std::max(
                    m_impl->recovery_remaining, distance);
                return m_impl->emit_control(AETH_SEQUENCE_NAK,
                                            qp.expected_rx_psn,
                                            output,
                                            status);
            }
            status_ok(status);
            return true;
        }
        if (disposition != RxPsnDisposition::EXPECTED) {
            m_impl->counters.out_of_order_packets++;
            status_ok(status);
            return true;
        }
        bool accepted = false;
        if (is_write_opcode(verified.opcode)) {
            accepted = m_impl->accept_write(verified, wire_frame, now_ns, output, status);
        } else if (is_send_opcode(verified.opcode)) {
            accepted = m_impl->accept_send(verified, wire_frame, now_ns, output, status);
        } else if (verified.opcode == IB_COMPARE_SWAP || verified.opcode == IB_FETCH_ADD) {
            accepted = m_impl->accept_atomic_request(verified, now_ns, output, status);
        } else {
            accepted = m_impl->accept_read_request(verified, now_ns, output, status);
        }
        return accepted && m_impl->collect_completions(output, status);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC ingress allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC ingress exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC ingress failure");
    }
}

bool NativeRcTransport::poll(uint64_t now_ns,
                             uint32_t frame_budget,
                             NativeTransportOutput &output,
                             Status *status) {
    try {
        if (m_impl == NULL) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport is unavailable");
        }
        /* ERR is terminal but its final CQ evidence must remain drainable.
         * A poll after the RNR/fatal threshold therefore succeeds without
         * publishing wire work instead of masking the completion with an
         * INVALID_STATE return. */
        if (m_impl->qp.state() == QpState::ERR && !m_impl->detached) {
            return m_impl->collect_completions(output, status);
        }
        if (!m_impl->check_active(status) || !m_impl->check_time(now_ns, status)) {
            return false;
        }
        if (!m_impl->drain_paced_frames(now_ns,
                                        frame_budget,
                                        output,
                                        status)) {
            return false;
        }
        if (frame_budget != 0 && m_impl->pending_ack &&
            m_impl->pending_ack_deadline_ns != 0 &&
            now_ns >= m_impl->pending_ack_deadline_ns) {
            const uint32_t psn = m_impl->pending_ack_psn;
            m_impl->pending_ack = false;
            m_impl->pending_ack_deadline_ns = 0;
            m_impl->accepted_since_ack = 0;
            if (!m_impl->emit_control(AETH_ACK, psn, output, status)) {
                return false;
            }
            frame_budget--;
        }
        const bool replay_was_active = m_impl->replay.active;
        const size_t replay_frames_before = output.frames.size();
        if (frame_budget != 0 && replay_was_active &&
            !m_impl->emit_replay_chunk(now_ns,
                                       frame_budget,
                                       output,
                                       status)) {
            return false;
        }
        frame_budget -= std::min<uint32_t>(
            frame_budget,
            static_cast<uint32_t>(output.frames.size() -
                                  replay_frames_before));
        if (!replay_was_active && !m_impl->replay.active &&
            m_impl->rnr_waiting && now_ns >= m_impl->rnr_resume_ns) {
            m_impl->rnr_waiting = false;
            m_impl->rnr_resume_ns = 0;
            m_impl->counters.rnr_retries++;
            /* The synthetic poll can legitimately jump far beyond the RNR
             * resume point; arm the replay deadline from the poll timestamp. */
            m_impl->oldest_deadline_ns = 0;
            if (!m_impl->retransmit_from(m_impl->rnr_psn,
                                         now_ns,
                                         frame_budget,
                                         output,
                                         status)) {
                return false;
            }
        } else if (!replay_was_active && !m_impl->replay.active &&
                   m_impl->pending_frame_count() == 0 &&
                   !m_impl->rnr_waiting &&
                   m_impl->oldest_deadline_ns != 0 &&
                   now_ns >= m_impl->oldest_deadline_ns &&
                   !m_impl->tx_works.empty()) {
            if (m_impl->retry_remaining == 0) {
                m_impl->counters.retry_exhausted++;
                return m_impl->fail_all(ErrorCode::RETRY_EXHAUSTED,
                                        "retransmit retry budget exhausted",
                                        output,
                                        status);
            }
            if (m_impl->retry_remaining != 7) {
                m_impl->retry_remaining--;
            }
            m_impl->counters.timeout_retries++;
            uint32_t start_psn = m_impl->tx_works.front().posted.first_psn;
            if (m_impl->tx_works.front().posted.kind == WorkKind::READ) {
                start_psn = m_impl->tx_works.front().next_read_psn;
            } else if (m_impl->tx_works.front().frame_head !=
                       Impl::INVALID_FRAME) {
                start_psn = m_impl->retained_frames[
                    m_impl->tx_works.front().frame_head].frame.psn;
            }
            if (!m_impl->retransmit_from(start_psn,
                                         now_ns,
                                         frame_budget,
                                         output,
                                         status)) {
                return false;
            }
        }
        status_ok(status);
        return true;
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC poll allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC poll exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected native RC poll failure");
    }
}

bool NativeRcTransport::on_frame_accepted(uint32_t psn,
                                          uint64_t accepted_ns,
                                          Status *status) {
    try {
        if (m_impl == NULL || !m_impl->check_active(status) ||
            !m_impl->check_time(accepted_ns, status)) {
            return false;
        }
        if (!psn24_valid(psn)) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "accepted requester PSN exceeds 24 bits");
        }
        for (size_t index = 0; index < m_impl->tx_works.size(); index++) {
            Impl::TxWork &work = m_impl->tx_works[index];
            if (!m_impl->work_owns_live_psn(work, psn)) {
                continue;
            }
            /* WRITE retry timing starts only after the live terminal packet,
             * which carries AckReq, is accepted.  A READ has one request
             * packet (including an adjusted retry request) at its live PSN. */
            if (work_completes_by_ack(work.posted.kind) &&
                (work.frame_tail == Impl::INVALID_FRAME ||
                 psn != m_impl->retained_frames[work.frame_tail].frame.psn)) {
                status_ok(status);
                return true;
            }
            work.has_accepted_frame = true;
            work.last_accepted_ns = accepted_ns;
            if (index == 0) {
                m_impl->oldest_deadline_ns = saturated_deadline(
                    accepted_ns,
                    retry_timeout_ns(
                        m_impl->qp.runtime_view().config.timeout_exp));
                work.deadline_ns = m_impl->oldest_deadline_ns;
            }
            status_ok(status);
            return true;
        }
        /* Control, responder, stale, and already-retired frames do not own a
         * requester retry timer and are intentionally harmless here. */
        status_ok(status);
        return true;
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected requester-frame acceptance failure");
    }
}

bool NativeRcTransport::drain_completions(std::vector<CompletionRecord> &output,
                                          Status *status) {
    try {
        if (m_impl == NULL) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport is unavailable");
        }
        const size_t ready = m_impl->qp.runtime_view().ledger.ready_completions + m_impl->receive_completions.size();
        if (ready > output.max_size() - output.size()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "completion result exceeds container capacity");
        }
        output.reserve(output.size() + ready);
        return drain_completions_bounded(output, ready, status);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "completion result allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "completion result exceeds container capacity");
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected completion drain failure");
    }
}

bool NativeRcTransport::drain_completions_bounded(
    std::vector<CompletionRecord> &output,
    size_t max_records,
    Status *status) {
    try {
        if (m_impl == NULL) {
            return status_error(status,
                                ErrorCode::INVALID_STATE,
                                "native RC transport is unavailable");
        }
        const size_t sq_ready = std::min<size_t>(max_records, m_impl->qp.runtime_view().ledger.ready_completions);
        if (!m_impl->qp.drain_completions_bounded(output, sq_ready, status)) return false;
        const size_t count = std::min<size_t>(max_records - sq_ready, m_impl->receive_completions.size());
        if (count > output.capacity() - output.size()) return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "receive result exceeds capacity");
        output.insert(output.end(), m_impl->receive_completions.begin(), m_impl->receive_completions.begin() + count);
        m_impl->receive_completions.erase(m_impl->receive_completions.begin(), m_impl->receive_completions.begin() + count);
        status_ok(status);
        return true;
    } catch (...) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "unexpected bounded completion drain failure");
    }
}

bool NativeRcTransport::has_pending_work() const {
    return m_impl != NULL &&
           (!m_impl->tx_works.empty() ||
            m_impl->pending_frame_count() != 0 || m_impl->replay.active ||
            !m_impl->deferred_reads.empty() || m_impl->pending_ack ||
            m_impl->rnr_waiting);
}

NativeTransportRuntimeView NativeRcTransport::runtime_view() const {
    NativeTransportRuntimeView result;
    if (m_impl == NULL) {
        return result;
    }
    result.configured = m_impl->configured && !m_impl->detached;
    result.memory_bound = m_impl->memory != NULL;
    result.pending = has_pending_work();
    result.qp = m_impl->qp.runtime_view();
    result.outstanding_work = result.qp.ledger.outstanding_work;
    result.posted_receives = m_impl->receives.size() + (m_impl->staged_write.active && m_impl->staged_write.send ? 1 : 0);
    result.receive_cqes = m_impl->receive_completions.size();
    result.queued_tx_frames = static_cast<uint32_t>(
        m_impl->pending_frame_count());
    result.paced_tx_frames = result.queued_tx_frames;
    result.counters = m_impl->counters;
    return result;
}

NativeTransportSnapshot NativeRcTransport::snapshot() const {
    NativeTransportSnapshot result;
    if (m_impl == NULL) {
        return result;
    }
    result.configured = m_impl->configured && !m_impl->detached;
    result.memory_bound = m_impl->memory != NULL;
    result.pending = has_pending_work();
    result.qp = m_impl->qp.snapshot();
    result.outstanding_work = result.qp.ledger.outstanding_work;
    result.outstanding_packets = result.qp.ledger.outstanding_packets;
    result.queued_tx_frames = static_cast<uint32_t>(
        m_impl->pending_frame_count());
    result.receive_credits = m_impl->receive_credits;
    result.unlimited_receive_credits = m_impl->config.unlimited_receive_credits;
    result.responder_msn = m_impl->responder_msn;
    result.read_cache_entries = static_cast<uint32_t>(m_impl->read_cache.size());
    result.read_cache_bytes = m_impl->read_cache_bytes;
    result.deferred_read_frames = static_cast<uint32_t>(
        m_impl->deferred_reads.size());
    result.deferred_read_bytes = m_impl->deferred_read_bytes;
    result.oldest_deadline_ns = m_impl->oldest_deadline_ns;
    result.rnr_resume_ns = m_impl->rnr_resume_ns;
    result.next_send_ns = m_impl->next_send_ns;
    result.rate_scale = m_impl->rate_scale;
    result.frame_storage = m_impl->frame_pool.snapshot();
    result.retained_frame_capacity = static_cast<uint32_t>(
        m_impl->retained_frames.size());
    result.retained_frames = m_impl->retained_in_use;
    result.paced_frame_capacity = static_cast<uint32_t>(
        m_impl->pending_frames.capacity());
    result.hot_path_growth_events = m_impl->hot_path_growth_events;
    result.counters = m_impl->counters;
    return result;
}

void NativeRcTransport::abort(ErrorCode error, const std::string &reason) {
    if (m_impl == NULL || (m_impl->qp.state() != QpState::RTS && m_impl->qp.state() != QpState::RTR && m_impl->qp.state() != QpState::SQD)) {
        return;
    }
    const ErrorCode actual = error == ErrorCode::OK ? ErrorCode::ABORTED : error;
    const std::string actual_reason = reason.empty() ? "native RC transport aborted" : reason;
    m_impl->qp.enter_error(actual, actual_reason, NULL);
    for (size_t index = 0; index < m_impl->tx_works.size(); ++index) {
        m_impl->release_owner_frames(m_impl->tx_works[index]);
    }
    m_impl->tx_works.clear();
    m_impl->clear_pending_frames();
    m_impl->replay.clear();
    for (size_t index = 0; index < m_impl->read_cache.size(); ++index) {
        m_impl->release_owner_frames(m_impl->read_cache[index]);
    }
    m_impl->read_cache.clear();
    m_impl->read_cache_bytes = 0;
    m_impl->deferred_reads.clear();
    m_impl->deferred_read_bytes = 0;
    m_impl->flush_receives(actual);
    m_impl->staged_write.clear();
    m_impl->pending_ack = false;
    m_impl->rnr_waiting = false;
    m_impl->oldest_deadline_ns = 0;
}

void NativeRcTransport::detach() {
    if (m_impl == NULL || m_impl->detached) {
        return;
    }
    if (m_impl->qp.state() == QpState::RTS || m_impl->qp.state() == QpState::RTR || m_impl->qp.state() == QpState::SQD) {
        m_impl->qp.enter_error(ErrorCode::ABORTED,
                               "native RC transport detached",
                               NULL);
    }
    for (size_t index = 0; index < m_impl->tx_works.size(); ++index) {
        m_impl->release_owner_frames(m_impl->tx_works[index]);
    }
    m_impl->tx_works.clear();
    m_impl->clear_pending_frames();
    m_impl->replay.clear();
    for (size_t index = 0; index < m_impl->read_cache.size(); ++index) {
        m_impl->release_owner_frames(m_impl->read_cache[index]);
    }
    m_impl->read_cache.clear();
    m_impl->read_cache_bytes = 0;
    m_impl->deferred_reads.clear();
    m_impl->deferred_read_bytes = 0;
    m_impl->flush_receives(ErrorCode::ABORTED);
    m_impl->staged_write.clear();
    m_impl->frame_scratch.clear();
    m_impl->memory = NULL;
    m_impl->detached = true;
}

} // namespace Native
} // namespace TrexRoceV2
