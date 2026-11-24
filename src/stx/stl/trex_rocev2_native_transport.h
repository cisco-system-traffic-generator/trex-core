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

#ifndef __TREX_ROCEV2_NATIVE_TRANSPORT_H__
#define __TREX_ROCEV2_NATIVE_TRANSPORT_H__

#include "trex_rocev2_native_codec.h"
#include "trex_rocev2_native_qp.h"
#include "trex_rocev2_native_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

namespace TrexRoceV2 {
namespace Native {

/*
 * Transport-owned memory contract.  The production endpoint engine adapts its
 * registered backing store to this interface; tests can use a bounded vector
 * implementation without depending on DPDK or Python.
 */
class NativeTransportMemory {
public:
    virtual ~NativeTransportMemory() {}

    virtual bool read(uint64_t address,
                      uint64_t length,
                      MemoryAccess access,
                      std::vector<uint8_t> &output,
                      Status *status = NULL) const = 0;
    virtual bool view(uint64_t address,
                      uint64_t length,
                      MemoryAccess access,
                      ByteSpan &output,
                      Status *status = NULL) const = 0;
    virtual bool write(uint64_t address,
                       ByteSpan value,
                       MemoryAccess access,
                       Status *status = NULL) = 0;
    virtual bool validate_local_key(uint32_t lkey, uint32_t protection_domain,
                                    uint64_t address, uint64_t length,
                                    Status *status = NULL) const;
    virtual bool write_local_key(uint32_t lkey, uint32_t protection_domain,
                                 uint64_t address, ByteSpan value,
                                 Status *status = NULL);
    virtual bool validate_remote_access(uint32_t rkey, uint64_t address,
                                        uint64_t length, MemoryAccess required,
                                        Status *status = NULL) const;
    virtual bool view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                                 MemoryAccess required, ByteSpan &out,
                                 Status *status = NULL) const;
    virtual bool write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value,
                                  Status *status = NULL);
    virtual bool invalidate_remote_key(uint32_t rkey, bool apply,
                                       Status *status = NULL);
    virtual bool atomic(uint64_t address, uint32_t rkey, bool fetch_add,
                        uint64_t compare, uint64_t swap_add,
                        uint64_t &original, Status *status = NULL);
};

struct NativeScatterGatherEntry {
    uint64_t address;
    uint64_t length;
    uint32_t lkey;
    NativeScatterGatherEntry(uint64_t addr = 0, uint64_t len = 0, uint32_t key = 0) :
        address(addr), length(len), lkey(key) {}
};

struct NativeReceiveRequest {
    uint64_t wr_id;
    std::vector<NativeScatterGatherEntry> sges;
    bool signalled;
    NativeReceiveRequest() : wr_id(0), signalled(true) {}
};

struct NativeTransportSendRequest {
    bool with_immediate;
    uint32_t immediate_data;
    bool with_invalidate;
    uint32_t invalidate_rkey;
    bool signalled;
    bool fenced;
    NativeTransportSendRequest() : with_immediate(false), immediate_data(0),
        with_invalidate(false), invalidate_rkey(0), signalled(true), fenced(false) {}
};

struct NativeTransportAtomicRequest {
    uint64_t remote_address;
    uint32_t remote_rkey;
    uint64_t local_address;
    uint32_t local_lkey;
    uint64_t compare;
    uint64_t swap_add;
    bool fetch_add;
    bool signalled;
    bool fenced;
    NativeTransportAtomicRequest() : remote_address(0), remote_rkey(0),
        local_address(0), local_lkey(0), compare(0), swap_add(0), fetch_add(false),
        signalled(true), fenced(false) {}
};

enum class NativeTransportSignalMode {
    PLAIN_LAST = 0,
    IMMEDIATE
};

struct NativeTransportConfig {
    NativeEndpoint endpoint;
    uint16_t pkey;
    uint8_t control_dscp;
    NativeTransportSignalMode signal_mode;
    uint32_t ack_request_interval;
    uint32_t duplicate_window;
    uint32_t ack_coalesce_packets;
    uint64_t ack_coalesce_ns;
    uint32_t receive_credits;
    bool unlimited_receive_credits;
    bool require_receive_wqes;
    /* Diagnostic compatibility for original draft fixtures only. Production
     * RC profiles leave false and provision negotiated responder replay slots. */
    bool legacy_response_acks;
    bool cnp_enabled;
    uint64_t cnp_min_interval_ns;
    double cnp_reaction_factor;
    uint64_t cnp_recovery_ns;
    uint32_t max_tx_frames;
    uint64_t max_staged_write_bytes;
    uint32_t max_read_cache_entries;
    uint64_t max_read_cache_bytes;

    NativeTransportConfig();
    bool validate(Status *status = NULL) const;
};

struct NativeTransportWriteRequest {
    uint64_t remote_address;
    uint32_t remote_rkey;
    bool with_immediate;
    uint32_t immediate_data;
    bool signalled;
    bool fenced;

    NativeTransportWriteRequest();
};

struct NativeTransportReadRequest {
    uint64_t remote_address;
    uint32_t remote_rkey;
    uint64_t local_address;
    uint32_t local_lkey;
    uint64_t length;
    bool signalled;
    bool fenced;

    NativeTransportReadRequest();
};

/* A frame is immutable after publication; retry queues retain a slab handle. */
struct NativeTransportFrame {
    WorkId work_id;
    uint32_t psn;
    IbOpcode opcode;
    bool retransmission;
    NativeFrameBuffer bytes;

    NativeTransportFrame();
};

struct NativeTransportOutput {
    std::vector<NativeTransportFrame> frames;
    std::vector<CompletionRecord> completions;
    size_t frame_limit;
    size_t completion_limit;
    bool bounded;

    NativeTransportOutput();
    bool configure(size_t max_frames,
                   size_t max_completions,
                   Status *status = NULL);
    void clear();
};

struct NativeTransportCounters {
    uint64_t write_messages_posted;
    uint64_t read_messages_posted;
    uint64_t send_messages_posted;
    uint64_t receive_wqes_posted;
    uint64_t receive_completions;
    uint64_t atomic_messages_posted;
    uint64_t atomic_requests_received;
    uint64_t atomic_replay_hits;
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t acks_sent;
    uint64_t acks_received;
    uint64_t duplicate_acks;
    uint64_t naks_sent;
    uint64_t naks_received;
    uint64_t rnr_naks_sent;
    uint64_t rnr_naks_received;
    uint64_t sequence_naks_sent;
    uint64_t sequence_naks_received;
    uint64_t fatal_naks_sent;
    uint64_t fatal_naks_received;
    uint64_t duplicate_packets;
    uint64_t out_of_order_packets;
    uint64_t psn_gap_events;
    uint64_t packets_lost_detected;
    uint64_t packets_recovered;
    uint64_t retransmit_packets;
    uint64_t retransmit_events;
    uint64_t timeout_retries;
    uint64_t nak_retries;
    uint64_t rnr_retries;
    uint64_t retry_exhausted;
    uint64_t rnr_retry_exhausted;
    uint64_t read_requests_received;
    uint64_t read_response_packets;
    uint64_t read_response_bytes;
    uint64_t read_cache_hits;
    uint64_t read_cache_misses;
    uint64_t receive_credits_consumed;
    uint64_t receive_credit_stalls;
    uint64_t deliveries;
    uint64_t delivered_bytes;
    uint64_t ecn_ce_packets;
    uint64_t cnp_sent;
    uint64_t cnp_received;
    uint64_t malformed_packets;
    uint64_t unknown_qp_packets;
    uint64_t icrc_errors;
    uint64_t endpoint_mismatch_packets;
    uint64_t stale_generation_packets;

    NativeTransportCounters();
};

struct NativeTransportSnapshot {
    bool configured;
    bool memory_bound;
    bool pending;
    QpSnapshot qp;
    uint32_t outstanding_work;
    uint32_t outstanding_packets;
    uint32_t queued_tx_frames;
    uint32_t receive_credits;
    bool unlimited_receive_credits;
    uint32_t responder_msn;
    uint32_t read_cache_entries;
    uint64_t read_cache_bytes;
    uint32_t deferred_read_frames;
    uint64_t deferred_read_bytes;
    uint64_t oldest_deadline_ns;
    uint64_t rnr_resume_ns;
    uint64_t next_send_ns;
    double rate_scale;
    NativeStorageSnapshot frame_storage;
    uint32_t retained_frame_capacity;
    uint32_t retained_frames;
    uint32_t paced_frame_capacity;
    uint64_t hot_path_growth_events;
    NativeTransportCounters counters;

    NativeTransportSnapshot();
};

/* Allocation-free counter/QP view used by the owning dataplane worker. */
struct NativeTransportRuntimeView {
    bool configured;
    bool memory_bound;
    bool pending;
    QpRuntimeView qp;
    uint32_t outstanding_work;
    uint32_t posted_receives;
    uint32_t receive_cqes;
    uint32_t queued_tx_frames;
    uint32_t paced_tx_frames;
    NativeTransportCounters counters;

    NativeTransportRuntimeView();
};

/*
 * One concrete reliable-connected requester/responder per QueuePair.  All
 * methods are single-owner and poll-driven; no method creates a worker thread,
 * blocks, or invokes a Python callback.
 */
class NativeRcTransport {
public:
    explicit NativeRcTransport(QueuePair &qp);
    ~NativeRcTransport();

    static bool estimate_storage_bytes(
        const QpConfig &qp_config,
        const NativeTransportConfig &transport_config,
        uint64_t &bytes,
        Status *status = NULL);

    NativeRcTransport(const NativeRcTransport &) = delete;
    NativeRcTransport &operator=(const NativeRcTransport &) = delete;

    bool configure(const NativeTransportConfig &config,
                   Status *status = NULL);
    bool bind_memory(NativeTransportMemory *memory,
                     Status *status = NULL);

    bool post_receive(uint32_t credits, Status *status = NULL);
    bool post_receive(const NativeReceiveRequest &request, Status *status = NULL);
    bool post_send(const NativeTransportSendRequest &request, ByteSpan payload,
                   uint64_t now_ns, PostedWork &posted,
                   NativeTransportOutput &output, Status *status = NULL);
    bool post_atomic(const NativeTransportAtomicRequest &request,
                     uint64_t now_ns, PostedWork &posted,
                     NativeTransportOutput &output, Status *status = NULL);
    bool post_write(const NativeTransportWriteRequest &request,
                    ByteSpan payload,
                    uint64_t now_ns,
                    PostedWork &posted,
                    NativeTransportOutput &output,
                    Status *status = NULL);
    bool post_read(const NativeTransportReadRequest &request,
                   uint64_t now_ns,
                   PostedWork &posted,
                   NativeTransportOutput &output,
                   Status *status = NULL);

    bool on_frame(const DecodedPacket &packet,
                  ByteSpan wire_frame,
                  uint64_t now_ns,
                  NativeTransportOutput &output,
                  Status *status = NULL);
    bool poll(uint64_t now_ns,
              uint32_t frame_budget,
              NativeTransportOutput &output,
              Status *status = NULL);
    /* Arms requester retry timing only after the adapter accepted a frame. */
    bool on_frame_accepted(uint32_t psn,
                           uint64_t accepted_ns,
                           Status *status = NULL);
    bool drain_completions(std::vector<CompletionRecord> &output,
                           Status *status = NULL);
    bool drain_completions_bounded(std::vector<CompletionRecord> &output,
                                   size_t max_records,
                                   Status *status = NULL);

    bool has_pending_work() const;
    NativeTransportRuntimeView runtime_view() const;
    NativeTransportSnapshot snapshot() const;
    void abort(ErrorCode error, const std::string &reason);
    void detach();

private:
    struct Impl;
    Impl *m_impl;
};

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_TRANSPORT_H__ */
