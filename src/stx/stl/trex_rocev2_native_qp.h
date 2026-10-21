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

#ifndef __TREX_ROCEV2_NATIVE_QP_H__
#define __TREX_ROCEV2_NATIVE_QP_H__

#include "trex_rocev2_native_types.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace TrexRoceV2 {
namespace Native {

enum class QpState {
    RESET = 0,
    INIT,
    RTR,
    RTS,
    SQD,
    ERR
};

const char *qp_state_to_string(QpState state);

enum class MemoryAccess : uint8_t {
    NONE = 0,
    LOCAL_READ = 1U << 0,
    LOCAL_WRITE = 1U << 1,
    REMOTE_READ = 1U << 2,
    REMOTE_WRITE = 1U << 3,
    REMOTE_ATOMIC = 1U << 4
};

MemoryAccess operator|(MemoryAccess left, MemoryAccess right);
MemoryAccess operator&(MemoryAccess left, MemoryAccess right);
bool has_memory_access(MemoryAccess configured, MemoryAccess required);

struct MemoryWindowConfig {
    uint64_t base_address;
    uint64_t length;
    uint32_t rkey;
    uint32_t lkey;
    uint32_t protection_domain;
    bool remote_key_active;
    MemoryAccess access;

    MemoryWindowConfig();
};

struct MemoryWindowSnapshot {
    bool configured;
    bool active;
    uint64_t base_address;
    uint64_t length;
    uint64_t last_address;
    uint32_t rkey;
    uint32_t lkey;
    uint32_t protection_domain;
    bool remote_key_active;
    MemoryAccess access;

    MemoryWindowSnapshot();
};

/*
 * Metadata-only emulated memory registration.  Payload storage belongs to the
 * requester/responder engine; this object is the single authority for rkey,
 * access, and overflow-safe address-range admission.
 */
class MemoryWindow {
public:
    MemoryWindow();

    bool configure(const MemoryWindowConfig &config, Status *status = NULL);
    bool validate(uint32_t rkey,
                  uint64_t address,
                  uint64_t length,
                  MemoryAccess required,
                  uint64_t *offset = NULL,
                  Status *status = NULL) const;
    bool invalidate_remote_key(uint32_t rkey, Status *status = NULL);
    void deactivate();
    void reset();

    bool configured() const;
    bool active() const;
    MemoryWindowSnapshot snapshot() const;

private:
    MemoryWindowConfig m_config;
    bool m_configured;
    bool m_active;
    bool m_remote_key_active;
};

struct QpConfig {
    uint32_t local_qpn;
    uint32_t starting_sq_psn;
    uint16_t pmtu;
    uint32_t sq_depth_messages;
    uint32_t sq_depth_packets;
    uint32_t rq_depth_messages;
    uint32_t max_receive_sge;
    uint32_t protection_domain;
    bool allow_remote_registry;
    uint8_t timeout_exp;
    uint8_t retry_count;
    uint8_t rnr_retry_count;
    uint8_t min_rnr_timer;
    uint8_t max_rd_atomic;
    uint8_t max_dest_rd_atomic;
    uint8_t service_level;
    uint8_t traffic_class;

    QpConfig();
    bool validate(Status *status = NULL) const;
};

struct QpPeerConfig {
    uint32_t remote_qpn;
    uint32_t starting_psn;
    uint16_t pmtu;
    uint32_t rkey;
    uint64_t virtual_address;
    uint64_t window_size;
    MemoryAccess window_access;
    uint8_t max_rd_atomic;
    uint8_t service_level;
    uint8_t traffic_class;
    std::string gid;
    std::string ip;
    std::string mac;

    QpPeerConfig();
    bool validate(Status *status = NULL) const;
};

enum class WorkKind {
    WRITE = 0,
    READ,
    SEND,
    RECEIVE,
    RECEIVE_RDMA_WITH_IMMEDIATE,
    COMPARE_SWAP,
    FETCH_ADD
};

bool work_requires_response(WorkKind kind);
bool work_completes_by_ack(WorkKind kind);
bool work_is_receive(WorkKind kind);

enum class CompletionState {
    SUCCESS = 0,
    ERROR,
    FLUSHED
};

struct WorkDescriptor {
    WorkId id;
    WorkKind kind;
    uint32_t first_psn;
    uint32_t terminal_psn;
    /* Sequence slots occupied by the operation. For RDMA Read this is the
     * response-packet count, even though only one request frame is sent. */
    uint32_t reserved_psns;
    uint64_t payload_bytes;
    uint64_t expected_read_bytes;
    uint32_t expected_read_psns;
    bool signalled;

    WorkDescriptor();
};

struct PostedWork {
    WorkId id;
    WorkKind kind;
    uint32_t first_psn;
    uint32_t terminal_psn;
    uint32_t reserved_psns;
    uint64_t payload_bytes;
    uint64_t expected_read_bytes;
    uint32_t expected_read_psns;
    bool signalled;

    PostedWork();
};

struct CompletionRecord {
    WorkId id;
    WorkKind kind;
    CompletionState state;
    ErrorCode error;
    uint32_t terminal_psn;
    uint64_t completed_bytes;
    uint32_t completed_psns;
    uint64_t wr_id;
    bool has_immediate;
    uint32_t immediate_data;
    bool has_invalidate;
    uint32_t invalidated_rkey;
    uint64_t atomic_original_value;

    CompletionRecord();
};

struct CompletionLedgerCounters {
    uint64_t work_posted;
    uint64_t sq_psns_reserved;
    uint64_t payload_bytes_posted;
    uint64_t writes_completed;
    uint64_t reads_completed;
    uint64_t sends_completed;
    uint64_t atomics_completed;
    uint64_t work_errored;
    uint64_t work_flushed;
    uint64_t acks_received;
    uint64_t duplicate_acks;
    uint64_t invalid_acks;
    uint64_t read_response_packets;
    uint64_t read_response_bytes;

    CompletionLedgerCounters();
};

struct CompletionLedgerSnapshot {
    bool configured;
    QpId owner;
    uint32_t max_outstanding_work;
    uint32_t max_outstanding_packets;
    uint32_t outstanding_work;
    uint32_t outstanding_packets;
    uint32_t outstanding_responses;
    uint32_t ready_completions;
    uint32_t initial_sq_psn;
    uint32_t next_post_psn;
    uint32_t last_acked_psn;
    bool has_posted_packets;
    CompletionLedgerCounters counters;

    CompletionLedgerSnapshot();
};

/*
 * Allocation-free live QP view for the owning dataplane thread.  Unlike the
 * publication snapshot, this intentionally excludes diagnostic strings and
 * peer address strings so workload admission and execution checks cannot allocate.
 */
struct QpRuntimeView {
    QpId id;
    QpState state;
    bool has_config;
    QpConfig config;
    bool has_peer;
    uint32_t remote_qpn;
    uint16_t negotiated_pmtu;
    uint32_t next_sq_psn;
    uint32_t expected_rx_psn;
    uint32_t last_acked_psn;
    uint64_t next_work_sequence;
    MemoryWindowSnapshot local_window;
    MemoryWindowSnapshot remote_window;
    CompletionLedgerSnapshot ledger;

    QpRuntimeView();
};

/*
 * Bounded internal completion bookkeeping.  This is intentionally not a
 * verbs CQ API.  A write succeeds only when a cumulative protocol ACK covers
 * its terminal PSN; a read succeeds only after its exact response byte and PSN
 * counts have arrived contiguously.
 */
class CompletionLedger {
public:
    CompletionLedger();

    static bool estimate_storage_bytes(uint32_t max_outstanding_work,
                                       uint64_t &bytes,
                                       Status *status = NULL);

    bool configure(const QpId &owner,
                   uint32_t initial_sq_psn,
                   uint32_t max_outstanding_work,
                   uint32_t max_outstanding_packets,
                   Status *status = NULL);
    bool post(const WorkDescriptor &work, Status *status = NULL);
    bool acknowledge(uint32_t ack_psn, Status *status = NULL);
    bool record_read_response(const WorkId &id,
                              uint32_t response_psn,
                              uint32_t payload_bytes,
                              bool last,
                              Status *status = NULL);
    bool set_atomic_result(const WorkId &id, uint64_t value, Status *status = NULL);
    bool fail(const WorkId &id, ErrorCode error, Status *status = NULL);

    /* Flushes newest work first, mirroring reverse resource unwinding. */
    void flush_reverse(ErrorCode error = ErrorCode::INVALID_STATE);
    void release_runtime();
    void reset();

    bool drain_completions(std::vector<CompletionRecord> &out, Status *status = NULL);
    bool drain_completions_bounded(std::vector<CompletionRecord> &out,
                                   size_t max_records,
                                   Status *status = NULL);
    CompletionLedgerSnapshot snapshot() const;

private:
    struct Entry {
        WorkDescriptor work;
        bool read_started;
        bool acknowledged;
        bool response_complete;
        uint64_t atomic_original_value;
        uint32_t next_read_psn;
        uint32_t read_psns_received;
        uint64_t read_bytes_received;

        Entry();
    };

    typedef std::vector<Entry>::iterator EntryIterator;

    bool validate_work(const WorkDescriptor &work, Status *status) const;
    bool acknowledge_internal(uint32_t ack_psn, bool count_ack, Status *status);
    EntryIterator find_entry(const WorkId &id);
    bool append_completion(const Entry &entry,
                           CompletionState state,
                           ErrorCode error,
                           Status *status);
    void erase_entry(EntryIterator entry);
    bool retire_ready(Status *status);

private:
    bool m_configured;
    QpId m_owner;
    uint32_t m_initial_sq_psn;
    uint32_t m_next_post_psn;
    uint32_t m_last_acked_psn;
    uint32_t m_last_posted_psn;
    uint32_t m_max_outstanding_work;
    uint32_t m_max_outstanding_packets;
    uint32_t m_outstanding_packets;
    uint32_t m_outstanding_responses;
    bool m_has_posted_packets;
    uint64_t m_last_work_sequence;
    std::vector<Entry> m_entries;
    std::vector<CompletionRecord> m_ready;
    CompletionLedgerCounters m_counters;
};

enum class RxPsnDisposition {
    EXPECTED = 0,
    DUPLICATE,
    GAP,
    AMBIGUOUS,
    INVALID
};

struct QpCounters {
    uint64_t state_transitions;
    uint64_t invalid_transitions;
    uint64_t write_messages_posted;
    uint64_t read_messages_posted;
    uint64_t receive_packets_accepted;
    uint64_t receive_psns_advanced;
    uint64_t receive_duplicates;
    uint64_t receive_gaps;
    uint64_t error_transitions;
    uint64_t destroy_calls;

    QpCounters();
};

struct QpSnapshot {
    QpId id;
    QpState state;
    bool has_config;
    QpConfig config;
    bool has_peer;
    QpPeerConfig peer;
    uint16_t negotiated_pmtu;
    uint32_t next_sq_psn;
    uint32_t expected_rx_psn;
    uint32_t last_acked_psn;
    uint64_t next_work_sequence;
    bool retransmit_timer_armed;
    ErrorCode error_code;
    std::string error_reason;
    MemoryWindowSnapshot local_window;
    MemoryWindowSnapshot remote_window;
    CompletionLedgerSnapshot ledger;
    QpCounters counters;

    QpSnapshot();
};

/*
 * Single-worker QP state.  Synchronization belongs to the owning session;
 * snapshots are detached value copies and cannot mutate live state.
 */
class QueuePair {
public:
    explicit QueuePair(const QpId &id);
    ~QueuePair();

    static bool estimate_storage_bytes(const QpConfig &config,
                                       uint64_t &bytes,
                                       Status *status = NULL);

    QueuePair(const QueuePair &) = delete;
    QueuePair &operator=(const QueuePair &) = delete;

    bool create(const QpConfig &config,
                const MemoryWindowConfig &local_window,
                Status *status = NULL);
    bool modify_to_rtr(const QpPeerConfig &peer, Status *status = NULL);
    bool modify_to_rts(Status *status = NULL);
    bool modify_to_sqd(Status *status = NULL);
    bool resume_from_sqd(Status *status = NULL);
    bool drained() const;
    bool update_local_window(const MemoryWindowConfig &window, Status *status = NULL);

    bool post_write(uint64_t remote_address,
                    uint32_t remote_rkey,
                    uint64_t payload_bytes,
                    bool signalled,
                    PostedWork &posted,
                    Status *status = NULL);
    bool post_read(uint64_t remote_address,
                   uint32_t remote_rkey,
                   uint64_t requested_bytes,
                   bool signalled,
                   PostedWork &posted,
                   Status *status = NULL);

    bool post_send(uint64_t payload_bytes, bool signalled,
                   PostedWork &posted, Status *status = NULL);
    bool post_atomic(uint64_t remote_address, uint32_t remote_rkey,
                     bool fetch_add, bool signalled,
                     PostedWork &posted, Status *status = NULL);
    bool acknowledge(uint32_t ack_psn, Status *status = NULL);
    bool record_read_response(const WorkId &id,
                              uint32_t response_psn,
                              uint32_t payload_bytes,
                              bool last,
                              Status *status = NULL);
    bool set_atomic_result(const WorkId &id, uint64_t value, Status *status = NULL);
    bool fail_work(const WorkId &id, ErrorCode error, Status *status = NULL);
    bool drain_completions(std::vector<CompletionRecord> &out, Status *status = NULL);
    bool drain_completions_bounded(std::vector<CompletionRecord> &out,
                                   size_t max_records,
                                   Status *status = NULL);

    RxPsnDisposition classify_rx_psn(uint32_t psn) const;
    bool accept_rx_psn(uint32_t psn, Status *status = NULL);
    /* RDMA Read consumes one wire request but reserves the complete response
     * PSN span. Other RC packets pass logical_psns == 1. */
    bool accept_rx_span(uint32_t first_psn,
                        uint32_t logical_psns,
                        Status *status = NULL);

    bool enter_error(ErrorCode error,
                     const std::string &reason,
                     Status *status = NULL);
    bool destroy(const std::string &flush_reason = "QP destroyed", Status *status = NULL);

    const QpId &id() const;
    QpState state() const;
    QpRuntimeView runtime_view() const;
    QpSnapshot snapshot() const;

private:
    bool require_state(QpState expected, const char *operation, Status *status);
    bool make_work(WorkKind kind,
                   uint64_t payload_bytes,
                   uint32_t reserved_psns,
                   uint64_t expected_read_bytes,
                   uint32_t expected_read_psns,
                   bool signalled,
                   PostedWork &posted,
                   Status *status);
    void unwind_runtime_reverse(ErrorCode error);
    void transition(QpState next);

private:
    QpId m_id;
    QpState m_state;
    bool m_has_config;
    QpConfig m_config;
    bool m_has_peer;
    QpPeerConfig m_peer;
    uint16_t m_negotiated_pmtu;
    uint32_t m_next_sq_psn;
    uint32_t m_expected_rx_psn;
    uint64_t m_next_work_sequence;
    bool m_retransmit_timer_armed;
    ErrorCode m_error_code;
    std::string m_error_reason;
    MemoryWindow m_local_window;
    MemoryWindow m_remote_window;
    CompletionLedger m_ledger;
    QpCounters m_counters;
};

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_QP_H__ */
