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

#include "trex_rocev2_native_qp.h"

#include <limits>
#include <new>
#include <sstream>

namespace TrexRoceV2 {
namespace Native {

namespace {

static const uint8_t MEMORY_ACCESS_MASK =
    static_cast<uint8_t>(MemoryAccess::LOCAL_READ) |
    static_cast<uint8_t>(MemoryAccess::LOCAL_WRITE) |
    static_cast<uint8_t>(MemoryAccess::REMOTE_READ) |
    static_cast<uint8_t>(MemoryAccess::REMOTE_WRITE) |
    static_cast<uint8_t>(MemoryAccess::REMOTE_ATOMIC);

bool valid_pmtu(uint16_t pmtu) {
    return pmtu == 256 || pmtu == 512 || pmtu == 1024 || pmtu == 2048 || pmtu == 4096;
}

bool valid_wire_qpn(uint32_t qpn) {
    return qpn != 0 && qpn <= PSN24_MASK;
}

bool add_u64_overflows(uint64_t left, uint64_t right) {
    return right > std::numeric_limits<uint64_t>::max() - left;
}

bool add_storage_reservation(uint64_t count,
                             uint64_t bytes_per_item,
                             uint64_t &total,
                             Status *status,
                             const char *description) {
    if ((count != 0 &&
         bytes_per_item > std::numeric_limits<uint64_t>::max() / count) ||
        add_u64_overflows(total, count * bytes_per_item)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            std::string(description) +
                                " storage reservation overflowed");
    }
    total += count * bytes_per_item;
    return true;
}

} // namespace

const char *qp_state_to_string(QpState state) {
    switch (state) {
    case QpState::RESET:
        return "RESET";
    case QpState::INIT:
        return "INIT";
    case QpState::RTR:
        return "RTR";
    case QpState::RTS:
        return "RTS";
    case QpState::SQD:
        return "SQD";
    case QpState::ERR:
        return "ERR";
    }
    return "UNKNOWN";
}

MemoryAccess operator|(MemoryAccess left, MemoryAccess right) {
    return static_cast<MemoryAccess>(static_cast<uint8_t>(left) |
                                     static_cast<uint8_t>(right));
}

MemoryAccess operator&(MemoryAccess left, MemoryAccess right) {
    return static_cast<MemoryAccess>(static_cast<uint8_t>(left) &
                                     static_cast<uint8_t>(right));
}

bool has_memory_access(MemoryAccess configured, MemoryAccess required) {
    const uint8_t configured_bits = static_cast<uint8_t>(configured);
    const uint8_t required_bits = static_cast<uint8_t>(required);
    return required_bits != 0 && (configured_bits & required_bits) == required_bits;
}

MemoryWindowConfig::MemoryWindowConfig() :
    base_address(0),
    length(0),
    rkey(0),
    lkey(0),
    protection_domain(1),
    remote_key_active(true),
    access(MemoryAccess::NONE) {
}

MemoryWindowSnapshot::MemoryWindowSnapshot() :
    configured(false),
    active(false),
    base_address(0),
    length(0),
    last_address(0),
    rkey(0),
    lkey(0),
    protection_domain(1),
    remote_key_active(true),
    access(MemoryAccess::NONE) {
}

MemoryWindow::MemoryWindow() :
    m_configured(false),
    m_active(false),
    m_remote_key_active(false) {
}

bool MemoryWindow::configure(const MemoryWindowConfig &config, Status *status) {
    if (config.length == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "memory window length must be greater than zero");
    }
    if (config.rkey == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "memory window rkey must be non-zero");
    }
    const uint8_t access = static_cast<uint8_t>(config.access);
    if (access == 0 || (access & ~MEMORY_ACCESS_MASK) != 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "memory window access flags are invalid");
    }

    /* The inclusive last address must be representable in uint64_t. */
    if (config.length - 1 > std::numeric_limits<uint64_t>::max() - config.base_address) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory window address range overflows uint64");
    }

    m_config = config;
    if (m_config.lkey == 0) m_config.lkey = m_config.rkey;
    if (m_config.protection_domain == 0) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "memory protection domain is zero");
    }
    m_remote_key_active = config.remote_key_active;
    m_configured = true;
    m_active = true;
    status_ok(status);
    return true;
}

bool MemoryWindow::validate(uint32_t rkey,
                            uint64_t address,
                            uint64_t length,
                            MemoryAccess required,
                            uint64_t *offset,
                            Status *status) const {
    if (!m_configured || !m_active) {
        return status_error(status, ErrorCode::INVALID_STATE, "memory window is not active");
    }
    if (length == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "memory access length must be greater than zero");
    }
    const uint8_t required_bits = static_cast<uint8_t>(required);
    if (required_bits == 0 || (required_bits & ~MEMORY_ACCESS_MASK) != 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "required memory access flags are invalid");
    }
    const MemoryAccess remote = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
    if (static_cast<uint8_t>(required & remote) != 0 && !m_remote_key_active) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "remote memory key was invalidated");
    }
    if (rkey != m_config.rkey) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "memory rkey does not match");
    }
    if (!has_memory_access(m_config.access, required)) {
        return status_error(status,
                            ErrorCode::ACCESS_DENIED,
                            "memory window does not grant the requested access");
    }
    if (address < m_config.base_address) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory address precedes the registered window");
    }

    const uint64_t relative = address - m_config.base_address;
    if (relative >= m_config.length || length - 1 > (m_config.length - 1) - relative) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory access exceeds the registered window");
    }
    if (add_u64_overflows(address, length - 1)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory access address overflows uint64");
    }

    if (offset != NULL) {
        *offset = relative;
    }
    status_ok(status);
    return true;
}

bool MemoryWindow::invalidate_remote_key(uint32_t rkey, Status *status) {
    if (!m_active || !m_remote_key_active || rkey != m_config.rkey) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "invalid or inactive remote invalidation key");
    }
    m_remote_key_active = false;
    status_ok(status);
    return true;
}

void MemoryWindow::deactivate() {
    m_active = false;
    m_remote_key_active = false;
}

void MemoryWindow::reset() {
    m_config = MemoryWindowConfig();
    m_configured = false;
    m_active = false;
    m_remote_key_active = false;
}

bool MemoryWindow::configured() const {
    return m_configured;
}

bool MemoryWindow::active() const {
    return m_active;
}

MemoryWindowSnapshot MemoryWindow::snapshot() const {
    MemoryWindowSnapshot result;
    result.configured = m_configured;
    result.active = m_active;
    if (m_configured) {
        result.base_address = m_config.base_address;
        result.length = m_config.length;
        result.last_address = m_config.base_address + m_config.length - 1;
        result.rkey = m_config.rkey;
        result.lkey = m_config.lkey;
        result.protection_domain = m_config.protection_domain;
        result.remote_key_active = m_remote_key_active;
        result.access = m_config.access;
    }
    return result;
}

QpConfig::QpConfig() :
    local_qpn(0),
    starting_sq_psn(0),
    pmtu(4096),
    sq_depth_messages(128),
    sq_depth_packets(4096),
    rq_depth_messages(128),
    max_receive_sge(16),
    protection_domain(1),
    allow_remote_registry(false),
    timeout_exp(14),
    retry_count(7),
    rnr_retry_count(7),
    min_rnr_timer(12),
    max_rd_atomic(16),
    max_dest_rd_atomic(16),
    service_level(0),
    traffic_class(0) {
}

bool QpConfig::validate(Status *status) const {
    if (!valid_wire_qpn(local_qpn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "local QPN must be a non-zero 24-bit value");
    }
    if (!psn24_valid(starting_sq_psn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "starting SQ PSN exceeds 24 bits");
    }
    if (!valid_pmtu(pmtu)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "PMTU is not an IBA path MTU");
    }
    if (rq_depth_messages == 0 || max_receive_sge == 0 || max_receive_sge > 16 || protection_domain == 0) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "RQ/SGE/protection-domain limits are invalid");
    }
    if (sq_depth_messages == 0 || sq_depth_packets == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "SQ message and packet depths must be greater than zero");
    }
    if (sq_depth_packets > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "SQ packet depth must remain below half the PSN space");
    }
    if (timeout_exp > 31) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "timeout exponent exceeds 31");
    }
    if (retry_count > 7 || rnr_retry_count > 7) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "retry count exceeds 7");
    }
    if (min_rnr_timer > 31) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "minimum RNR timer exceeds 31");
    }
    if (service_level > 15) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "service level exceeds 15");
    }
    status_ok(status);
    return true;
}

QpPeerConfig::QpPeerConfig() :
    remote_qpn(0),
    starting_psn(0),
    pmtu(4096),
    rkey(0),
    virtual_address(0),
    window_size(0),
    window_access(MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE),
    max_rd_atomic(16),
    service_level(0),
    traffic_class(0) {
}

bool QpPeerConfig::validate(Status *status) const {
    if (!valid_wire_qpn(remote_qpn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote QPN must be a non-zero 24-bit value");
    }
    if (!psn24_valid(starting_psn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote starting PSN exceeds 24 bits");
    }
    if (!valid_pmtu(pmtu)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote PMTU is not an IBA path MTU");
    }
    if (rkey == 0 || window_size == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote rkey and window size must be non-zero");
    }
    if (window_size - 1 > std::numeric_limits<uint64_t>::max() - virtual_address) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "remote memory window overflows uint64");
    }
    const MemoryAccess allowed = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
    const uint8_t access = static_cast<uint8_t>(window_access);
    if (access == 0 || (access & ~static_cast<uint8_t>(allowed)) != 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote window access must contain only remote read/write flags");
    }
    if (service_level > 15) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "remote service level exceeds 15");
    }
    status_ok(status);
    return true;
}

WorkDescriptor::WorkDescriptor() :
    kind(WorkKind::WRITE),
    first_psn(0),
    terminal_psn(0),
    reserved_psns(0),
    payload_bytes(0),
    expected_read_bytes(0),
    expected_read_psns(0),
    signalled(true) {
}

PostedWork::PostedWork() :
    kind(WorkKind::WRITE),
    first_psn(0),
    terminal_psn(0),
    reserved_psns(0),
    payload_bytes(0),
    expected_read_bytes(0),
    expected_read_psns(0),
    signalled(true) {
}

CompletionRecord::CompletionRecord() :
    kind(WorkKind::WRITE),
    state(CompletionState::ERROR),
    error(ErrorCode::INTERNAL_ERROR),
    terminal_psn(0),
    completed_bytes(0),
    completed_psns(0),
    wr_id(0), has_immediate(false), immediate_data(0),
    has_invalidate(false), invalidated_rkey(0), atomic_original_value(0) {
}

CompletionLedgerCounters::CompletionLedgerCounters() :
    work_posted(0),
    sq_psns_reserved(0),
    payload_bytes_posted(0),
    writes_completed(0),
    reads_completed(0),
    sends_completed(0),
    atomics_completed(0),
    work_errored(0),
    work_flushed(0),
    acks_received(0),
    duplicate_acks(0),
    invalid_acks(0),
    read_response_packets(0),
    read_response_bytes(0) {
}

CompletionLedgerSnapshot::CompletionLedgerSnapshot() :
    configured(false),
    max_outstanding_work(0),
    max_outstanding_packets(0),
    outstanding_work(0),
    outstanding_packets(0),
    outstanding_responses(0),
    ready_completions(0),
    initial_sq_psn(0),
    next_post_psn(0),
    last_acked_psn(0),
    has_posted_packets(false) {
}

QpRuntimeView::QpRuntimeView() :
    state(QpState::RESET),
    has_config(false),
    has_peer(false),
    remote_qpn(0),
    negotiated_pmtu(0),
    next_sq_psn(0),
    expected_rx_psn(0),
    last_acked_psn(0),
    next_work_sequence(0) {
}

CompletionLedger::Entry::Entry() :
    read_started(false),
    acknowledged(false),
    response_complete(false),
    atomic_original_value(0),
    next_read_psn(0),
    read_psns_received(0),
    read_bytes_received(0) {
}

CompletionLedger::CompletionLedger() :
    m_configured(false),
    m_initial_sq_psn(0),
    m_next_post_psn(0),
    m_last_acked_psn(0),
    m_last_posted_psn(0),
    m_max_outstanding_work(0),
    m_max_outstanding_packets(0),
    m_outstanding_packets(0),
    m_outstanding_responses(0),
    m_has_posted_packets(false),
    m_last_work_sequence(0) {
}

bool CompletionLedger::estimate_storage_bytes(uint32_t max_outstanding_work,
                                              uint64_t &bytes,
                                              Status *status) {
    bytes = 0;
    if (max_outstanding_work == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "completion-ledger capacity must be greater than zero");
    }
    if (!add_storage_reservation(max_outstanding_work,
                                 sizeof(Entry),
                                 bytes,
                                 status,
                                 "completion-ledger entry vector") ||
        !add_storage_reservation(max_outstanding_work,
                                 sizeof(CompletionRecord),
                                 bytes,
                                 status,
                                 "completion-ledger ready vector")) {
        return false;
    }
    status_ok(status);
    return true;
}

bool CompletionLedger::configure(const QpId &owner,
                                 uint32_t initial_sq_psn,
                                 uint32_t max_outstanding_work,
                                 uint32_t max_outstanding_packets,
                                 Status *status) {
    if (!owner.valid()) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "completion owner is invalid");
    }
    if (!psn24_valid(initial_sq_psn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "initial completion-ledger PSN exceeds 24 bits");
    }
    if (max_outstanding_work == 0 || max_outstanding_packets == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "completion-ledger capacities must be greater than zero");
    }
    if (max_outstanding_packets > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "completion-ledger packet capacity exceeds serial safety limit");
    }

    try {
        m_entries.reserve(max_outstanding_work);
        m_ready.reserve(max_outstanding_work);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "could not reserve completion-ledger storage");
    }

    reset();
    m_owner = owner;
    m_initial_sq_psn = initial_sq_psn;
    m_next_post_psn = initial_sq_psn;
    if (!psn24_add(initial_sq_psn, -1, m_last_acked_psn, status)) {
        return false;
    }
    m_last_posted_psn = m_last_acked_psn;
    m_max_outstanding_work = max_outstanding_work;
    m_max_outstanding_packets = max_outstanding_packets;
    m_configured = true;
    status_ok(status);
    return true;
}

bool CompletionLedger::validate_work(const WorkDescriptor &work, Status *status) const {
    if (!m_configured) {
        return status_error(status, ErrorCode::INVALID_STATE, "completion ledger is not configured");
    }
    if (!work.id.valid()) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "work ID is invalid");
    }
    if (!work.id.belongs_to(m_owner)) {
        return status_error(status, ErrorCode::STALE_GENERATION, "work ID belongs to another QP generation");
    }
    if (work.id.sequence() <= m_last_work_sequence) {
        return status_error(status,
                            ErrorCode::DUPLICATE,
                            "work sequence is not newer than the previously posted sequence");
    }
    if (!psn24_valid(work.first_psn) || !psn24_valid(work.terminal_psn)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "work PSN exceeds 24 bits");
    }
    if (work.reserved_psns == 0 || work.reserved_psns > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "work packet count is invalid");
    }
    if (work.first_psn != m_next_post_psn) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "work does not begin at the next SQ PSN");
    }
    uint32_t expected_terminal = 0;
    if (!psn24_add(work.first_psn,
                   static_cast<int64_t>(work.reserved_psns) - 1,
                   expected_terminal,
                   status)) {
        return false;
    }
    if (work.terminal_psn != expected_terminal) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "work terminal PSN does not match its packet count");
    }

    if (work_completes_by_ack(work.kind)) {
        if (work.expected_read_bytes != 0 || work.expected_read_psns != 0) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "write work cannot declare read-response expectations");
        }
    } else if (work_requires_response(work.kind)) {
        if (work.expected_read_psns == 0 ||
            work.expected_read_psns > PSN24_MAX_SERIAL_SPAN) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "read work requires non-zero bounded response expectations");
        }
        if (work.reserved_psns != work.expected_read_psns) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "read work must reserve one SQ PSN for each response packet");
        }
        if (work.payload_bytes != work.expected_read_bytes) {
            return status_error(status,
                                ErrorCode::INVALID_ARGUMENT,
                                "read payload size must equal expected response bytes");
        }
    }

    if (!work_completes_by_ack(work.kind) && !work_requires_response(work.kind)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "unsupported SQ work kind");
    }

    if (m_entries.size() + m_ready.size() >= m_max_outstanding_work) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "completion queue must be drained before posting more work");
    }
    if (work.reserved_psns > m_max_outstanding_packets - m_outstanding_packets) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SQ packet depth is exhausted");
    }

    uint32_t unacked_span = 0;
    if (!psn24_forward_distance(m_last_acked_psn, work.terminal_psn, unacked_span, status)) {
        return false;
    }
    if (unacked_span == 0 || unacked_span > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "posting would make cumulative ACK ordering ambiguous");
    }
    status_ok(status);
    return true;
}

bool CompletionLedger::post(const WorkDescriptor &work, Status *status) {
    if (!validate_work(work, status)) {
        return false;
    }

    Entry entry;
    entry.work = work;
    /* Storage was reserved by configure and capacity was checked above. */
    m_entries.push_back(entry);
    m_outstanding_packets += work.reserved_psns;
    if (work_requires_response(work.kind)) ++m_outstanding_responses;
    m_last_posted_psn = work.terminal_psn;
    if (!psn24_add(work.terminal_psn, 1, m_next_post_psn, status)) {
        m_entries.pop_back();
        m_outstanding_packets -= work.reserved_psns;
        if (work_requires_response(work.kind)) --m_outstanding_responses;
        return false;
    }
    m_has_posted_packets = true;
    m_last_work_sequence = work.id.sequence();
    m_counters.work_posted++;
    m_counters.sq_psns_reserved += work.reserved_psns;
    m_counters.payload_bytes_posted += work.payload_bytes;
    status_ok(status);
    return true;
}

bool CompletionLedger::append_completion(const Entry &entry,
                                         CompletionState state,
                                         ErrorCode error,
                                         Status *status) {
    /* A successful unsignalled WQE retires from the SQ and contributes to
     * completion accounting, but it must not consume a CQ slot.  Errors and
     * flushes are always reported so callers cannot silently lose failures. */
    const bool publish = state != CompletionState::SUCCESS ||
                         entry.work.signalled;
    if (publish && m_ready.size() >= m_ready.capacity()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "completion queue capacity is exhausted");
    }

    CompletionRecord completion;
    completion.id = entry.work.id;
    completion.kind = entry.work.kind;
    completion.atomic_original_value = entry.atomic_original_value;
    completion.wr_id = entry.work.id.sequence();
    completion.state = state;
    completion.error = error;
    completion.terminal_psn = entry.work.terminal_psn;
    if (work_requires_response(entry.work.kind)) {
        completion.completed_bytes = entry.read_bytes_received;
        completion.completed_psns = entry.read_psns_received;
    } else if (state == CompletionState::SUCCESS) {
        completion.completed_bytes = entry.work.payload_bytes;
        completion.completed_psns = entry.work.reserved_psns;
    }
    if (publish) {
        m_ready.push_back(completion);
    }

    if (state == CompletionState::SUCCESS) {
        if (entry.work.kind == WorkKind::WRITE) {
            m_counters.writes_completed++;
        } else if (entry.work.kind == WorkKind::READ) {
            m_counters.reads_completed++;
        } else if (entry.work.kind == WorkKind::SEND) {
            m_counters.sends_completed++;
        } else {
            m_counters.atomics_completed++;
        }
    } else if (state == CompletionState::FLUSHED) {
        m_counters.work_flushed++;
    } else {
        m_counters.work_errored++;
    }
    status_ok(status);
    return true;
}

void CompletionLedger::erase_entry(EntryIterator entry) {
    m_outstanding_packets -= entry->work.reserved_psns;
    if (work_requires_response(entry->work.kind)) --m_outstanding_responses;
    m_entries.erase(entry);
}

bool CompletionLedger::acknowledge_internal(uint32_t ack_psn,
                                            bool count_ack,
                                            Status *status) {
    if (!m_configured) {
        return status_error(status, ErrorCode::INVALID_STATE, "completion ledger is not configured");
    }
    if (count_ack) {
        m_counters.acks_received++;
    }
    if (!psn24_valid(ack_psn)) {
        m_counters.invalid_acks++;
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "ACK PSN exceeds 24 bits");
    }

    const PsnOrder order = psn24_compare(ack_psn, m_last_acked_psn);
    if (order == PsnOrder::EQUAL || order == PsnOrder::BEFORE) {
        if (count_ack) {
            m_counters.duplicate_acks++;
        }
        status_ok(status);
        return true;
    }
    if (order == PsnOrder::AMBIGUOUS || order == PsnOrder::INVALID) {
        m_counters.invalid_acks++;
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "ACK cannot be ordered in the active PSN serial window");
    }
    if (!m_has_posted_packets) {
        m_counters.invalid_acks++;
        return status_error(status, ErrorCode::PROTOCOL_ERROR, "ACK received before any work was posted");
    }

    uint32_t ack_distance = 0;
    uint32_t newest_distance = 0;
    if (!psn24_forward_distance(m_last_acked_psn, ack_psn, ack_distance, status) ||
        !psn24_forward_distance(m_last_acked_psn,
                                m_last_posted_psn,
                                newest_distance,
                                status)) {
        m_counters.invalid_acks++;
        return false;
    }
    if (ack_distance > newest_distance) {
        m_counters.invalid_acks++;
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "ACK advances beyond the newest posted SQ PSN");
    }

    EntryIterator entry = m_entries.begin();
    while (entry != m_entries.end()) {
        bool covered = false;
        if (!psn24_in_closed_interval(entry->work.terminal_psn,
                                      m_last_acked_psn,
                                      ack_psn,
                                      covered,
                                      status)) {
            m_counters.invalid_acks++;
            return false;
        }
        /* The interval includes the old ACK itself; it cannot name pending work. */
        covered = covered && entry->work.terminal_psn != m_last_acked_psn;
        if (covered && work_completes_by_ack(entry->work.kind)) {
            entry->acknowledged = true;
        }
        ++entry;
    }

    m_last_acked_psn = ack_psn;
    return retire_ready(status);
}

bool CompletionLedger::acknowledge(uint32_t ack_psn, Status *status) {
    return acknowledge_internal(ack_psn, true, status);
}

CompletionLedger::EntryIterator CompletionLedger::find_entry(const WorkId &id) {
    for (EntryIterator entry = m_entries.begin(); entry != m_entries.end(); ++entry) {
        if (entry->work.id == id) {
            return entry;
        }
    }
    return m_entries.end();
}

bool CompletionLedger::record_read_response(const WorkId &id,
                                            uint32_t response_psn,
                                            uint32_t payload_bytes,
                                            bool last,
                                            Status *status) {
    if (!m_configured) {
        return status_error(status, ErrorCode::INVALID_STATE, "completion ledger is not configured");
    }
    if (!id.valid() || !id.belongs_to(m_owner)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "read response work ID belongs to another QP generation");
    }
    if (!psn24_valid(response_psn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "read response PSN is invalid");
    }

    EntryIterator entry = find_entry(id);
    if (entry == m_entries.end()) {
        return status_error(status, ErrorCode::NOT_FOUND, "read response work ID is not outstanding");
    }
    if (!work_requires_response(entry->work.kind)) {
        return status_error(status, ErrorCode::PROTOCOL_ERROR, "response was attributed to non-read work");
    }

    const uint32_t expected_psn = entry->read_started
                                      ? entry->next_read_psn
                                      : entry->work.first_psn;
    if (response_psn != expected_psn) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "read response PSN is not contiguous");
    }
    if (entry->read_psns_received >= entry->work.expected_read_psns) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "read response contains more PSNs than expected");
    }
    if (payload_bytes > entry->work.expected_read_bytes - entry->read_bytes_received) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "read response contains more bytes than expected");
    }

    const uint32_t new_psn_count = entry->read_psns_received + 1;
    const uint64_t new_byte_count = entry->read_bytes_received + payload_bytes;
    const bool exactly_complete =
        new_psn_count == entry->work.expected_read_psns &&
        new_byte_count == entry->work.expected_read_bytes;
    if (last != exactly_complete) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            last ? "read response ended before its declared size"
                                 : "read response reached its declared size without LAST");
    }

    /*
     * An RC Read Request reserves the response PSN range.  Each accepted
     * response packet cumulatively advances that range; the request is not
     * considered acknowledged all the way to terminal_psn when FIRST arrives.
     * acknowledge_internal may erase preceding write entries, so reacquire the
     * read entry before mutating its assembly state.
     */
    if (!acknowledge_internal(response_psn, false, status)) {
        return false;
    }
    entry = find_entry(id);
    if (entry == m_entries.end()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "read work disappeared while advancing response PSN");
    }

    entry->read_started = true;
    entry->read_psns_received = new_psn_count;
    entry->read_bytes_received = new_byte_count;
    m_counters.read_response_packets++;
    m_counters.read_response_bytes += payload_bytes;
    if (!psn24_add(response_psn, 1, entry->next_read_psn, status)) {
        return false;
    }

    if (last) entry->response_complete = true;
    return retire_ready(status);
}

bool CompletionLedger::retire_ready(Status *status) {
    while (!m_entries.empty()) {
        const Entry &entry = m_entries.front();
        if (work_requires_response(entry.work.kind) ? !entry.response_complete : !entry.acknowledged) break;
        if (!append_completion(entry, CompletionState::SUCCESS, ErrorCode::OK, status)) return false;
        erase_entry(m_entries.begin());
    }
    status_ok(status);
    return true;
}

bool CompletionLedger::set_atomic_result(const WorkId &id, uint64_t value, Status *status) {
    EntryIterator entry = find_entry(id);
    if (entry == m_entries.end() || (entry->work.kind != WorkKind::COMPARE_SWAP && entry->work.kind != WorkKind::FETCH_ADD)) {
        return status_error(status, ErrorCode::NOT_FOUND, "atomic result does not name outstanding atomic work");
    }
    entry->atomic_original_value = value;
    status_ok(status);
    return true;
}

bool CompletionLedger::fail(const WorkId &id, ErrorCode error, Status *status) {
    if (!m_configured) {
        return status_error(status, ErrorCode::INVALID_STATE, "completion ledger is not configured");
    }
    if (error == ErrorCode::OK) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "failed work requires an error code");
    }
    if (!id.valid() || !id.belongs_to(m_owner)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "failed work ID belongs to another QP generation");
    }
    EntryIterator entry = find_entry(id);
    if (entry == m_entries.end()) {
        return status_error(status, ErrorCode::NOT_FOUND, "failed work ID is not outstanding");
    }
    if (!append_completion(*entry, CompletionState::ERROR, error, status)) {
        return false;
    }
    erase_entry(entry);
    status_ok(status);
    return true;
}

void CompletionLedger::flush_reverse(ErrorCode error) {
    for (std::vector<Entry>::reverse_iterator entry = m_entries.rbegin();
         entry != m_entries.rend();
         ++entry) {
        /* Capacity is invariant: ready + outstanding never exceeds max work. */
        CompletionRecord completion;
        completion.id = entry->work.id;
        completion.kind = entry->work.kind;
        completion.state = CompletionState::FLUSHED;
        completion.error = (error == ErrorCode::OK) ? ErrorCode::INVALID_STATE : error;
        completion.terminal_psn = entry->work.terminal_psn;
        completion.completed_bytes = entry->read_bytes_received;
        completion.completed_psns = entry->read_psns_received;
        m_ready.push_back(completion);
        m_counters.work_flushed++;
    }
    m_entries.clear();
    m_outstanding_packets = 0;
    m_outstanding_responses = 0;
}

void CompletionLedger::release_runtime() {
    m_entries.clear();
    m_outstanding_packets = 0;
    m_outstanding_responses = 0;
    m_configured = false;
}

void CompletionLedger::reset() {
    m_entries.clear();
    m_ready.clear();
    m_configured = false;
    m_owner = QpId();
    m_initial_sq_psn = 0;
    m_next_post_psn = 0;
    m_last_acked_psn = 0;
    m_last_posted_psn = 0;
    m_max_outstanding_work = 0;
    m_max_outstanding_packets = 0;
    m_outstanding_packets = 0;
    m_outstanding_responses = 0;
    m_has_posted_packets = false;
    m_last_work_sequence = 0;
    m_counters = CompletionLedgerCounters();
}

bool CompletionLedger::drain_completions(std::vector<CompletionRecord> &out,
                                         Status *status) {
    try {
        out.reserve(out.size() + m_ready.size());
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "could not reserve caller completion storage");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "caller completion storage exceeds container capacity");
    }
    return drain_completions_bounded(out, m_ready.size(), status);
}

bool CompletionLedger::drain_completions_bounded(
    std::vector<CompletionRecord> &out,
    size_t max_records,
    Status *status) {
    const size_t count = std::min(max_records, m_ready.size());
    if (out.size() > out.capacity() ||
        count > out.capacity() - out.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "caller completion storage has no admitted capacity");
    }
    out.insert(out.end(), m_ready.begin(), m_ready.begin() + count);
    if (count != 0) {
        m_ready.erase(m_ready.begin(), m_ready.begin() + count);
    }
    status_ok(status);
    return true;
}

CompletionLedgerSnapshot CompletionLedger::snapshot() const {
    CompletionLedgerSnapshot result;
    result.configured = m_configured;
    result.owner = m_owner;
    result.max_outstanding_work = m_max_outstanding_work;
    result.max_outstanding_packets = m_max_outstanding_packets;
    result.outstanding_work = static_cast<uint32_t>(m_entries.size());
    result.outstanding_packets = m_outstanding_packets;
    result.outstanding_responses = m_outstanding_responses;
    result.ready_completions = static_cast<uint32_t>(m_ready.size());
    result.initial_sq_psn = m_initial_sq_psn;
    result.next_post_psn = m_next_post_psn;
    result.last_acked_psn = m_last_acked_psn;
    result.has_posted_packets = m_has_posted_packets;
    result.counters = m_counters;
    return result;
}

QpCounters::QpCounters() :
    state_transitions(0),
    invalid_transitions(0),
    write_messages_posted(0),
    read_messages_posted(0),
    receive_packets_accepted(0),
    receive_psns_advanced(0),
    receive_duplicates(0),
    receive_gaps(0),
    error_transitions(0),
    destroy_calls(0) {
}

QpSnapshot::QpSnapshot() :
    state(QpState::RESET),
    has_config(false),
    has_peer(false),
    negotiated_pmtu(0),
    next_sq_psn(0),
    expected_rx_psn(0),
    last_acked_psn(0),
    next_work_sequence(0),
    retransmit_timer_armed(false),
    error_code(ErrorCode::OK) {
}

QueuePair::QueuePair(const QpId &id) :
    m_id(id),
    m_state(QpState::RESET),
    m_has_config(false),
    m_has_peer(false),
    m_negotiated_pmtu(0),
    m_next_sq_psn(0),
    m_expected_rx_psn(0),
    m_next_work_sequence(1),
    m_retransmit_timer_armed(false),
    m_error_code(ErrorCode::OK) {
}

QueuePair::~QueuePair() {
    if (m_state == QpState::RTS || m_state == QpState::RTR || m_state == QpState::SQD) {
        unwind_runtime_reverse(ErrorCode::INVALID_STATE);
    }
    m_remote_window.deactivate();
    m_local_window.deactivate();
}

bool QueuePair::estimate_storage_bytes(const QpConfig &config,
                                       uint64_t &bytes,
                                       Status *status) {
    bytes = 0;
    if (!config.validate(status)) {
        return false;
    }
    uint64_t ledger_bytes = 0;
    if (!CompletionLedger::estimate_storage_bytes(config.sq_depth_messages,
                                                  ledger_bytes,
                                                  status) ||
        !add_storage_reservation(1,
                                 sizeof(QueuePair),
                                 bytes,
                                 status,
                                 "queue-pair object") ||
        !add_storage_reservation(1,
                                 ledger_bytes,
                                 bytes,
                                 status,
                                 "queue-pair completion ledger")) {
        return false;
    }
    status_ok(status);
    return true;
}

bool QueuePair::require_state(QpState expected, const char *operation, Status *status) {
    if (m_state == expected) {
        return true;
    }
    m_counters.invalid_transitions++;
    std::ostringstream message;
    message << operation << " requires " << qp_state_to_string(expected)
            << ", current state is " << qp_state_to_string(m_state);
    return status_error(status, ErrorCode::INVALID_STATE, message.str());
}

void QueuePair::transition(QpState next) {
    m_state = next;
    m_counters.state_transitions++;
}

bool QueuePair::create(const QpConfig &config,
                       const MemoryWindowConfig &local_window,
                       Status *status) {
    if (!require_state(QpState::RESET, "create", status)) {
        return false;
    }
    if (!m_id.valid()) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "QP identity is invalid");
    }
    if (!config.validate(status)) {
        return false;
    }
    if (config.protection_domain != local_window.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "QP and local memory belong to different protection domains");
    }
    MemoryWindow candidate;
    if (!candidate.configure(local_window, status)) {
        return false;
    }

    m_config = config;
    m_has_config = true;
    m_has_peer = false;
    m_peer = QpPeerConfig();
    m_negotiated_pmtu = config.pmtu;
    m_next_sq_psn = config.starting_sq_psn;
    m_expected_rx_psn = 0;
    m_retransmit_timer_armed = false;
    m_error_code = ErrorCode::OK;
    m_error_reason.clear();
    m_local_window = candidate;
    m_remote_window.reset();
    m_ledger.reset();
    transition(QpState::INIT);
    status_ok(status);
    return true;
}

bool QueuePair::modify_to_rtr(const QpPeerConfig &peer, Status *status) {
    if (!require_state(QpState::INIT, "modify_to_rtr", status)) {
        return false;
    }
    if (!peer.validate(status)) {
        return false;
    }
    MemoryWindowConfig remote_config;
    remote_config.base_address = peer.virtual_address;
    remote_config.length = peer.window_size;
    remote_config.rkey = peer.rkey;
    remote_config.access = peer.window_access;
    MemoryWindow candidate;
    if (!candidate.configure(remote_config, status)) {
        return false;
    }

    if (!m_ledger.configure(m_id,
                            m_config.starting_sq_psn,
                            m_config.sq_depth_messages,
                            m_config.sq_depth_packets,
                            status)) {
        return false;
    }
    m_config.max_rd_atomic = std::min(m_config.max_rd_atomic, peer.max_rd_atomic);
    m_peer = peer;
    m_has_peer = true;
    m_remote_window = candidate;
    m_negotiated_pmtu = (m_config.pmtu < peer.pmtu) ? m_config.pmtu : peer.pmtu;
    m_expected_rx_psn = peer.starting_psn;
    transition(QpState::RTR);
    status_ok(status);
    return true;
}

bool QueuePair::modify_to_rts(Status *status) {
    if (!require_state(QpState::RTR, "modify_to_rts", status)) {
        return false;
    }
    m_next_sq_psn = m_config.starting_sq_psn;
    /* An RTS QP has no retransmission deadline until work is posted. */
    m_retransmit_timer_armed = false;
    transition(QpState::RTS);
    status_ok(status);
    return true;
}

bool QueuePair::modify_to_sqd(Status *status) {
    if (!require_state(QpState::RTS, "modify_to_sqd", status)) return false;
    transition(QpState::SQD);
    status_ok(status);
    return true;
}
bool QueuePair::drained() const {
    return m_state == QpState::SQD && m_ledger.snapshot().outstanding_work == 0;
}
bool QueuePair::resume_from_sqd(Status *status) {
    if (!require_state(QpState::SQD, "resume_from_sqd", status)) return false;
    if (!drained()) return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SQ is still draining");
    transition(QpState::RTS);
    status_ok(status);
    return true;
}

bool QueuePair::update_local_window(const MemoryWindowConfig &window, Status *status) {
    if (m_state != QpState::INIT && m_state != QpState::RTR && !drained()) {
        return status_error(status, ErrorCode::INVALID_STATE, "local registration update requires unstarted or drained QP");
    }
    if (window.protection_domain != m_config.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "updated registration belongs to another protection domain");
    }
    return m_local_window.configure(window, status);
}

bool QueuePair::make_work(WorkKind kind,
                          uint64_t payload_bytes,
                          uint32_t reserved_psns,
                          uint64_t expected_read_bytes,
                          uint32_t expected_read_psns,
                          bool signalled,
                          PostedWork &posted,
                          Status *status) {
    if (!require_state(QpState::RTS, "post work", status)) {
        return false;
    }
    if (m_next_work_sequence == 0 ||
        m_next_work_sequence == std::numeric_limits<uint64_t>::max()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "work ID sequence is exhausted for this QP generation");
    }
    if (reserved_psns == 0) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "reserved PSN count must be non-zero");
    }

    uint32_t terminal_psn = 0;
    if (!psn24_add(m_next_sq_psn,
                   static_cast<int64_t>(reserved_psns) - 1,
                   terminal_psn,
                   status)) {
        return false;
    }

    WorkDescriptor work;
    work.id = WorkId(m_id, m_next_work_sequence);
    work.kind = kind;
    work.first_psn = m_next_sq_psn;
    work.terminal_psn = terminal_psn;
    work.reserved_psns = reserved_psns;
    work.payload_bytes = payload_bytes;
    work.expected_read_bytes = expected_read_bytes;
    work.expected_read_psns = expected_read_psns;
    work.signalled = signalled;
    if (!m_ledger.post(work, status)) {
        return false;
    }

    if (!psn24_add(terminal_psn, 1, m_next_sq_psn, status)) {
        /* Ledger already proved this operation for a valid PSN; unreachable. */
        m_ledger.fail(work.id, ErrorCode::INTERNAL_ERROR, NULL);
        return false;
    }
    m_next_work_sequence++;
    m_retransmit_timer_armed = true;

    posted.id = work.id;
    posted.kind = work.kind;
    posted.first_psn = work.first_psn;
    posted.terminal_psn = work.terminal_psn;
    posted.reserved_psns = work.reserved_psns;
    posted.payload_bytes = work.payload_bytes;
    posted.expected_read_bytes = work.expected_read_bytes;
    posted.expected_read_psns = work.expected_read_psns;
    posted.signalled = work.signalled;
    status_ok(status);
    return true;
}

bool QueuePair::post_write(uint64_t remote_address,
                           uint32_t remote_rkey,
                           uint64_t payload_bytes,
                           bool signalled,
                           PostedWork &posted,
                           Status *status) {
    if (!require_state(QpState::RTS, "post_write", status)) {
        return false;
    }
    if (!m_config.allow_remote_registry && payload_bytes != 0 &&
        !m_remote_window.validate(remote_rkey,
                                  remote_address,
                                  payload_bytes,
                                  MemoryAccess::REMOTE_WRITE,
                                  NULL,
                                  status)) {
        return false;
    }
    const uint64_t packet_count64 = payload_bytes == 0
                                        ? 1
                                        : 1 + ((payload_bytes - 1) / m_negotiated_pmtu);
    if (packet_count64 > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "write occupies too many PSNs");
    }
    if (!make_work(WorkKind::WRITE,
                   payload_bytes,
                   static_cast<uint32_t>(packet_count64),
                   0,
                   0,
                   signalled,
                   posted,
                   status)) {
        return false;
    }
    m_counters.write_messages_posted++;
    return true;
}

bool QueuePair::post_read(uint64_t remote_address,
                          uint32_t remote_rkey,
                          uint64_t requested_bytes,
                          bool signalled,
                          PostedWork &posted,
                          Status *status) {
    if (!require_state(QpState::RTS, "post_read", status)) {
        return false;
    }
    if (!m_config.allow_remote_registry && requested_bytes != 0 &&
        !m_remote_window.validate(remote_rkey,
                                  remote_address,
                                  requested_bytes,
                                  MemoryAccess::REMOTE_READ,
                                  NULL,
                                  status)) {
        return false;
    }
    const uint64_t response_psns64 = requested_bytes == 0
                                         ? 1
                                         : 1 + ((requested_bytes - 1) / m_negotiated_pmtu);
    if (response_psns64 > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "read response occupies too many PSNs");
    }
    const uint32_t expected_response_psns = static_cast<uint32_t>(response_psns64);
    if (!make_work(WorkKind::READ,
                   requested_bytes,
                   expected_response_psns,
                   requested_bytes,
                   expected_response_psns,
                   signalled,
                   posted,
                   status)) {
        return false;
    }
    m_counters.read_messages_posted++;
    return true;
}

bool work_requires_response(WorkKind kind) {
    return kind == WorkKind::READ || kind == WorkKind::COMPARE_SWAP || kind == WorkKind::FETCH_ADD;
}
bool work_completes_by_ack(WorkKind kind) {
    return kind == WorkKind::WRITE || kind == WorkKind::SEND;
}
bool work_is_receive(WorkKind kind) {
    return kind == WorkKind::RECEIVE || kind == WorkKind::RECEIVE_RDMA_WITH_IMMEDIATE;
}

bool QueuePair::post_send(uint64_t payload_bytes, bool signalled,
                          PostedWork &posted, Status *status) {
    if (!require_state(QpState::RTS, "post_send", status)) return false;
    const uint64_t count = payload_bytes == 0 ? 1 : 1 + (payload_bytes - 1) / m_negotiated_pmtu;
    if (count > PSN24_MAX_SERIAL_SPAN) return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SEND PSN span is too large");
    return make_work(WorkKind::SEND, payload_bytes, static_cast<uint32_t>(count), 0, 0, signalled, posted, status);
}

bool QueuePair::post_atomic(uint64_t remote_address, uint32_t remote_rkey,
                            bool fetch_add, bool signalled,
                            PostedWork &posted, Status *status) {
    if (!require_state(QpState::RTS, "post_atomic", status)) return false;
    if ((remote_address & 7U) != 0) return status_error(status, ErrorCode::INVALID_ARGUMENT, "atomic address is not 8-byte aligned");
    if (!m_config.allow_remote_registry && !m_remote_window.validate(remote_rkey, remote_address, 8, MemoryAccess::REMOTE_ATOMIC, NULL, status)) return false;
    return make_work(fetch_add ? WorkKind::FETCH_ADD : WorkKind::COMPARE_SWAP, 8, 1, 8, 1, signalled, posted, status);
}

bool QueuePair::acknowledge(uint32_t ack_psn, Status *status) {
    if (m_state != QpState::RTS && m_state != QpState::SQD) {
        return status_error(status, ErrorCode::INVALID_STATE, "requester result requires RTS or SQD");
    }
    if (!m_ledger.acknowledge(ack_psn, status)) {
        return false;
    }
    m_retransmit_timer_armed = m_ledger.snapshot().outstanding_work != 0;
    return true;
}

bool QueuePair::record_read_response(const WorkId &id,
                                     uint32_t response_psn,
                                     uint32_t payload_bytes,
                                     bool last,
                                     Status *status) {
    if (m_state != QpState::RTS && m_state != QpState::SQD) {
        return status_error(status, ErrorCode::INVALID_STATE, "requester result requires RTS or SQD");
    }
    if (!m_ledger.record_read_response(id, response_psn, payload_bytes, last, status)) {
        return false;
    }
    m_retransmit_timer_armed = m_ledger.snapshot().outstanding_work != 0;
    return true;
}

bool QueuePair::set_atomic_result(const WorkId &id, uint64_t value, Status *status) {
    return m_ledger.set_atomic_result(id, value, status);
}

bool QueuePair::fail_work(const WorkId &id, ErrorCode error, Status *status) {
    if (m_state != QpState::RTS && m_state != QpState::SQD) {
        return status_error(status, ErrorCode::INVALID_STATE, "requester result requires RTS or SQD");
    }
    if (!m_ledger.fail(id, error, status)) {
        return false;
    }
    m_retransmit_timer_armed = m_ledger.snapshot().outstanding_work != 0;
    return true;
}

bool QueuePair::drain_completions(std::vector<CompletionRecord> &out, Status *status) {
    return m_ledger.drain_completions(out, status);
}

bool QueuePair::drain_completions_bounded(
    std::vector<CompletionRecord> &out,
    size_t max_records,
    Status *status) {
    return m_ledger.drain_completions_bounded(out, max_records, status);
}

RxPsnDisposition QueuePair::classify_rx_psn(uint32_t psn) const {
    if ((m_state != QpState::RTS && m_state != QpState::RTR && m_state != QpState::SQD) || !psn24_valid(psn)) {
        return RxPsnDisposition::INVALID;
    }
    switch (psn24_compare(psn, m_expected_rx_psn)) {
    case PsnOrder::EQUAL:
        return RxPsnDisposition::EXPECTED;
    case PsnOrder::BEFORE:
        return RxPsnDisposition::DUPLICATE;
    case PsnOrder::AFTER:
        return RxPsnDisposition::GAP;
    case PsnOrder::AMBIGUOUS:
        return RxPsnDisposition::AMBIGUOUS;
    case PsnOrder::INVALID:
        return RxPsnDisposition::INVALID;
    }
    return RxPsnDisposition::INVALID;
}

bool QueuePair::accept_rx_psn(uint32_t psn, Status *status) {
    return accept_rx_span(psn, 1, status);
}

bool QueuePair::accept_rx_span(uint32_t first_psn,
                               uint32_t logical_psns,
                               Status *status) {
    if (m_state != QpState::RTS && m_state != QpState::RTR && m_state != QpState::SQD) {
        return status_error(status, ErrorCode::INVALID_STATE, "receive requires RTR or RTS");
    }
    if (logical_psns == 0 || logical_psns > PSN24_MAX_SERIAL_SPAN) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "receive PSN span is zero or serially ambiguous");
    }
    const RxPsnDisposition disposition = classify_rx_psn(first_psn);
    if (disposition == RxPsnDisposition::DUPLICATE) {
        m_counters.receive_duplicates++;
        return status_error(status, ErrorCode::DUPLICATE, "receive PSN is a duplicate");
    }
    if (disposition == RxPsnDisposition::GAP) {
        m_counters.receive_gaps++;
        return status_error(status, ErrorCode::PROTOCOL_ERROR, "receive PSN is ahead of EPSN");
    }
    if (disposition != RxPsnDisposition::EXPECTED) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "receive PSN cannot be ordered against EPSN");
    }
    if (!psn24_add(m_expected_rx_psn,
                   static_cast<int64_t>(logical_psns),
                   m_expected_rx_psn,
                   status)) {
        return false;
    }
    m_counters.receive_packets_accepted++;
    m_counters.receive_psns_advanced += logical_psns;
    status_ok(status);
    return true;
}

void QueuePair::unwind_runtime_reverse(ErrorCode error) {
    /* Acquisition order is local window, remote binding, then SQ ledger. */
    m_ledger.flush_reverse(error);
    m_ledger.release_runtime();
    m_remote_window.deactivate();
    m_local_window.deactivate();
    m_retransmit_timer_armed = false;
}

bool QueuePair::enter_error(ErrorCode error,
                            const std::string &reason,
                            Status *status) {
    if (m_state != QpState::RTS && m_state != QpState::RTR && m_state != QpState::SQD) {
        ++m_counters.invalid_transitions;
        return status_error(status, ErrorCode::INVALID_STATE, "error transition requires RTR, RTS or SQD");
    }
    if (error == ErrorCode::OK || reason.empty()) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "ERR transition requires an error code and reason");
    }
    unwind_runtime_reverse(error);
    m_error_code = error;
    m_error_reason = reason;
    m_counters.error_transitions++;
    transition(QpState::ERR);
    status_ok(status);
    return true;
}

bool QueuePair::destroy(const std::string &flush_reason, Status *status) {
    (void)flush_reason;
    m_counters.destroy_calls++;
    if (m_state == QpState::RESET) {
        status_ok(status);
        return true;
    }

    if (m_state == QpState::RTS || m_state == QpState::RTR || m_state == QpState::SQD) {
        unwind_runtime_reverse(ErrorCode::INVALID_STATE);
    } else {
        /* Reverse the resources that exist in INIT/RTR/ERR. */
        m_ledger.release_runtime();
        m_remote_window.deactivate();
        m_local_window.deactivate();
        m_retransmit_timer_armed = false;
    }
    m_remote_window.reset();
    m_local_window.reset();
    m_has_peer = false;
    m_peer = QpPeerConfig();
    m_has_config = false;
    m_config = QpConfig();
    m_negotiated_pmtu = 0;
    m_next_sq_psn = 0;
    m_expected_rx_psn = 0;
    m_error_code = ErrorCode::OK;
    m_error_reason.clear();
    transition(QpState::RESET);
    status_ok(status);
    return true;
}

const QpId &QueuePair::id() const {
    return m_id;
}

QpState QueuePair::state() const {
    return m_state;
}

QpRuntimeView QueuePair::runtime_view() const {
    QpRuntimeView result;
    result.id = m_id;
    result.state = m_state;
    result.has_config = m_has_config;
    result.config = m_config;
    result.has_peer = m_has_peer;
    result.remote_qpn = m_has_peer ? m_peer.remote_qpn : 0;
    result.negotiated_pmtu = m_negotiated_pmtu;
    result.next_sq_psn = m_next_sq_psn;
    result.expected_rx_psn = m_expected_rx_psn;
    result.next_work_sequence = m_next_work_sequence;
    result.ledger = m_ledger.snapshot();
    result.last_acked_psn = result.ledger.last_acked_psn;
    result.local_window = m_local_window.snapshot();
    result.remote_window = m_remote_window.snapshot();
    return result;
}

QpSnapshot QueuePair::snapshot() const {
    QpSnapshot result;
    result.id = m_id;
    result.state = m_state;
    result.has_config = m_has_config;
    result.config = m_config;
    result.has_peer = m_has_peer;
    result.peer = m_peer;
    result.negotiated_pmtu = m_negotiated_pmtu;
    result.next_sq_psn = m_next_sq_psn;
    result.expected_rx_psn = m_expected_rx_psn;
    const CompletionLedgerSnapshot ledger = m_ledger.snapshot();
    result.last_acked_psn = ledger.last_acked_psn;
    result.next_work_sequence = m_next_work_sequence;
    result.retransmit_timer_armed = m_retransmit_timer_armed;
    result.error_code = m_error_code;
    result.error_reason = m_error_reason;
    result.local_window = m_local_window.snapshot();
    result.remote_window = m_remote_window.snapshot();
    result.ledger = ledger;
    result.counters = m_counters;
    return result;
}

} // namespace Native
} // namespace TrexRoceV2
