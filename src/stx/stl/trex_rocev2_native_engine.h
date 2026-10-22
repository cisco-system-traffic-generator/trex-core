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

#ifndef __TREX_ROCEV2_NATIVE_ENGINE_H__
#define __TREX_ROCEV2_NATIVE_ENGINE_H__

#include "trex_rocev2_native_codec.h"
#include "trex_rocev2_native_qp.h"
#include "trex_rocev2_native_transport.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace TrexRoceV2 {
namespace Native {

class NativeEndpointEngine;

static const uint32_t ENGINE_ANY_ENDPOINT = 0xffffffffU;
static const uint32_t ENGINE_ANY_PORT = 0xffffffffU;

/*
 * Payload storage for an emulated registered memory window.  The backing
 * vector is sized once during configuration and never resized while active,
 * so ByteSpan views handed to a transport remain stable until teardown.
 */
struct EngineMemoryConfig {
    uint32_t memory_id;
    MemoryWindowConfig window;
    uint8_t initial_value;

    EngineMemoryConfig();
};

struct EngineMemorySnapshot {
    uint32_t memory_id;
    MemoryWindowSnapshot window;
    uint64_t bytes;
    uint64_t checksum;
    uint64_t read_operations;
    uint64_t write_operations;
    uint64_t bytes_read;
    uint64_t bytes_written;

    EngineMemorySnapshot();
};

class NativeMemoryBuffer : public NativeTransportMemory {
public:
    NativeMemoryBuffer();

    NativeMemoryBuffer(const NativeMemoryBuffer &) = delete;
    NativeMemoryBuffer &operator=(const NativeMemoryBuffer &) = delete;

    bool configure(const EngineMemoryConfig &config,
                   uint64_t maximum_bytes,
                   Status *status = NULL);
    bool view(uint64_t address,
              uint64_t length,
              MemoryAccess required,
              ByteSpan &span,
              Status *status = NULL) const;
    bool read(uint64_t address,
              uint64_t length,
              MemoryAccess required,
              std::vector<uint8_t> &output,
              Status *status = NULL) const;
    bool write(uint64_t address,
               ByteSpan value,
               MemoryAccess required,
               Status *status = NULL);
    bool fill(uint64_t address,
              uint64_t length,
              uint8_t value,
              MemoryAccess required,
              Status *status = NULL);
    bool offset_of(uint64_t address,
                   uint64_t length,
                   MemoryAccess required,
                   uint64_t &offset,
                   Status *status = NULL) const;
    bool validate_local_key(uint32_t lkey, uint32_t protection_domain,
                            uint64_t address, uint64_t length,
                            Status *status = NULL) const;
    bool invalidate_remote_key(uint32_t rkey, bool apply,
                               Status *status = NULL);
    bool atomic(uint64_t address, uint32_t rkey, bool fetch_add,
                uint64_t compare, uint64_t swap_add,
                uint64_t &original, Status *status = NULL);
    bool validate_remote_access(uint32_t rkey, uint64_t address,
                                uint64_t length, MemoryAccess required,
                                Status *status = NULL) const;
    bool rebind_keys(uint32_t lkey, uint32_t rkey, Status *status = NULL);
    bool view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                         MemoryAccess required, ByteSpan &out, Status *status = NULL) const;
    bool write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value,
                          Status *status = NULL);
    void reset();

    bool configured() const;
    uint32_t memory_id() const;
    MemoryWindowSnapshot window() const { return m_window.snapshot(); }
    uint64_t checksum() const;
    EngineMemorySnapshot snapshot() const;

private:
    uint32_t m_memory_id;
    MemoryWindow m_window;
    std::vector<uint8_t> m_bytes;
    mutable uint64_t m_checksum_cache;
    mutable bool m_checksum_dirty;
    mutable uint64_t m_read_operations;
    uint64_t m_write_operations;
    mutable uint64_t m_bytes_read;
    uint64_t m_bytes_written;
    std::vector<uint32_t> m_retired_keys;
};

/*
 * A complete Ethernet frame has one value owner at every boundary.  Logical
 * endpoint and port fields are simulation/routing metadata and never replace
 * validation of the generation or destination QPN in the wire bytes.
 */
struct EngineWireFrame {
    SessionGeneration generation;
    uint32_t source_endpoint;
    uint32_t destination_endpoint;
    uint32_t source_qp_index;
    uint32_t egress_port;
    uint32_t ingress_port;
    uint64_t sequence;
    uint64_t ready_ns;
    /* Engine-local identity assigned at ingress admission.  Unlike sequence,
     * this is unique even when a control-plane test or external RX producer
     * supplies no source sequence. */
    uint64_t ingress_token;
    NativeFrameBuffer bytes;

    EngineWireFrame();
};

struct EngineWriteRequest {
    uint32_t source_memory_id;
    uint64_t source_address;
    uint64_t length;
    uint64_t remote_address;
    uint32_t remote_rkey;
    bool with_immediate;
    uint32_t immediate_data;
    bool signalled;
    bool fenced;

    EngineWriteRequest();
};

struct EngineReadRequest {
    uint32_t destination_memory_id;
    uint64_t destination_address;
    uint64_t length;
    uint64_t remote_address;
    uint32_t remote_rkey;
    bool signalled;
    bool fenced;

    EngineReadRequest();
};

struct EngineSendRequest : public NativeTransportSendRequest {
    uint32_t source_memory_id;
    uint64_t source_address;
    uint64_t length;
    EngineSendRequest() : source_memory_id(0), source_address(0), length(0) {}
};

struct EngineAtomicRequest : public NativeTransportAtomicRequest {
    uint32_t destination_memory_id;
    uint64_t destination_address;
    EngineAtomicRequest() : destination_memory_id(0), destination_address(0) {}
};

typedef NativeReceiveRequest EngineReceiveRequest;

struct EngineTransportSnapshot {
    QpSnapshot qp;
    NativeTransportSnapshot native;
    bool attached;
    bool pending;
    uint32_t queued_tx_frames;
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t malformed_packets;
    uint64_t retransmit_packets;

    EngineTransportSnapshot();
};

struct EngineIngressAdmission {
    bool ready;
    bool may_emit_frames;
    bool requires_empty_engine_tx;

    EngineIngressAdmission(bool is_ready = true,
                           bool emits_frames = true,
                           bool requires_empty_tx = false) :
        ready(is_ready),
        may_emit_frames(emits_frames),
        requires_empty_engine_tx(requires_empty_tx) {
    }
};

/*
 * The reliable requester/responder implements this endpoint directly.  This
 * small engine-owned contract keeps the session scheduler DPDK-free while the
 * production transport remains a concrete native C++ object, not a callback
 * into Python or Open Architecture.
 */
class EngineTransport {
public:
    virtual ~EngineTransport() {}
    virtual void bind_registry(NativeEndpointEngine *) {}
    virtual bool post_receive(const EngineReceiveRequest &, Status *status = NULL) {
        return status_error(status, ErrorCode::UNSUPPORTED, "transport has no receive queue");
    }
    virtual bool post_send(const EngineSendRequest &, ByteSpan, uint64_t,
                           PostedWork &, Status *status = NULL) {
        return status_error(status, ErrorCode::UNSUPPORTED, "transport has no SEND operation");
    }
    virtual bool post_atomic(const EngineAtomicRequest &, uint64_t,
                             PostedWork &, Status *status = NULL) {
        return status_error(status, ErrorCode::UNSUPPORTED, "transport has no atomic operation");
    }

    virtual bool attach(QueuePair *qp,
                        NativeMemoryBuffer *local_memory,
                        Status *status = NULL) = 0;
    virtual bool post_write(const EngineWriteRequest &request,
                            ByteSpan payload,
                            uint64_t now_ns,
                            PostedWork &posted,
                            Status *status = NULL) = 0;
    virtual bool post_read(const EngineReadRequest &request,
                           uint64_t now_ns,
                           PostedWork &posted,
                           Status *status = NULL) = 0;
    virtual bool on_frame(const EngineWireFrame &frame,
                          const DecodedPacket &packet,
                          uint64_t now_ns,
                          Status *status = NULL) = 0;
    virtual bool on_frame_accepted(uint32_t psn,
                                   uint64_t accepted_ns,
                                   Status *status = NULL) = 0;
    virtual bool poll(uint64_t now_ns,
                      uint32_t frame_budget,
                      std::vector<NativeFrameBuffer> &frames,
                      Status *status = NULL) = 0;
    virtual bool drain_completions(std::vector<CompletionRecord> &output,
                                   Status *status = NULL) = 0;
    virtual bool drain_completions_bounded(
        std::vector<CompletionRecord> &output,
        size_t max_records,
        Status *status = NULL) = 0;
    virtual bool has_pending_work() const = 0;
    virtual NativeTransportRuntimeView runtime_view() const = 0;
    virtual EngineTransportSnapshot snapshot() const = 0;
    virtual void abort(ErrorCode error, const std::string &reason) = 0;
    virtual void detach() = 0;
};

/*
 * Production adapter for the concrete requester/responder.  Initial post and
 * ingress outputs are retained in a bounded FIFO and participate in the same
 * fair engine poll as timer/retry output; callers never bypass engine queues.
 */
class NativeRcEngineTransport : public EngineTransport, public NativeTransportMemory {
public:
    explicit NativeRcEngineTransport(const NativeTransportConfig &config,
                                     uint32_t max_completion_records = 8192);
    virtual ~NativeRcEngineTransport();

    static bool estimate_storage_bytes(
        const QpConfig &qp_config,
        const NativeTransportConfig &transport_config,
        uint32_t max_completion_records,
        uint64_t &bytes,
        Status *status = NULL);

    NativeRcEngineTransport(const NativeRcEngineTransport &) = delete;
    NativeRcEngineTransport &operator=(const NativeRcEngineTransport &) = delete;
    void bind_registry(NativeEndpointEngine *engine) { m_engine = engine; }
    bool read(uint64_t address, uint64_t length, MemoryAccess access,
              std::vector<uint8_t> &out, Status *status = NULL) const;
    bool view(uint64_t address, uint64_t length, MemoryAccess access,
              ByteSpan &out, Status *status = NULL) const;
    bool write(uint64_t address, ByteSpan value, MemoryAccess access,
               Status *status = NULL);
    bool validate_local_key(uint32_t lkey, uint32_t protection_domain,
                            uint64_t address, uint64_t length, Status *status = NULL) const;
    bool write_local_key(uint32_t lkey, uint32_t protection_domain,
                         uint64_t address, ByteSpan value, Status *status = NULL);
    bool validate_remote_access(uint32_t rkey, uint64_t address,
                                uint64_t length, MemoryAccess required,
                                Status *status = NULL) const;
    bool view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                         MemoryAccess required, ByteSpan &out, Status *status = NULL) const;
    bool write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value, Status *status = NULL);
    bool invalidate_remote_key(uint32_t rkey, bool apply, Status *status = NULL);
    bool atomic(uint64_t address, uint32_t rkey, bool fetch_add,
                uint64_t compare, uint64_t swap_add, uint64_t &original,
                Status *status = NULL);
    bool post_receive(const EngineReceiveRequest &request, Status *status = NULL);
    bool post_send(const EngineSendRequest &request, ByteSpan payload,
                   uint64_t now_ns, PostedWork &posted, Status *status = NULL);
    bool post_atomic(const EngineAtomicRequest &request, uint64_t now_ns,
                     PostedWork &posted, Status *status = NULL);

    virtual bool attach(QueuePair *qp,
                        NativeMemoryBuffer *local_memory,
                        Status *status = NULL);
    virtual bool post_write(const EngineWriteRequest &request,
                            ByteSpan payload,
                            uint64_t now_ns,
                            PostedWork &posted,
                            Status *status = NULL);
    virtual bool post_read(const EngineReadRequest &request,
                           uint64_t now_ns,
                           PostedWork &posted,
                           Status *status = NULL);
    virtual bool on_frame(const EngineWireFrame &frame,
                          const DecodedPacket &packet,
                          uint64_t now_ns,
                          Status *status = NULL);
    virtual bool on_frame_accepted(uint32_t psn,
                                   uint64_t accepted_ns,
                                   Status *status = NULL);
    virtual bool poll(uint64_t now_ns,
                      uint32_t frame_budget,
                      std::vector<NativeFrameBuffer> &frames,
                      Status *status = NULL);
    virtual bool drain_completions(std::vector<CompletionRecord> &output,
                                   Status *status = NULL);
    virtual bool drain_completions_bounded(
        std::vector<CompletionRecord> &output,
        size_t max_records,
        Status *status = NULL);
    virtual bool has_pending_work() const;
    virtual NativeTransportRuntimeView runtime_view() const;
    virtual EngineTransportSnapshot snapshot() const;
    virtual void abort(ErrorCode error, const std::string &reason);
    virtual void detach();

private:
    bool absorb_output(NativeTransportOutput &output, Status *status);
    bool preflight_completion_slot(Status *status) const;
    bool flush_pending_frames(uint32_t budget,
                              std::vector<NativeFrameBuffer> &frames,
                              Status *status);

private:
    NativeTransportConfig m_config;
    uint32_t m_max_completion_records;
    QueuePair *m_qp;
    NativeMemoryBuffer *m_memory;
    NativeEndpointEngine *m_engine;
    std::unique_ptr<NativeRcTransport> m_transport;
    NativeBoundedRing<NativeTransportFrame> m_pending_frames;
    NativeBoundedRing<CompletionRecord> m_pending_completions;
    NativeTransportOutput m_output;
};

struct EngineQpBinding {
    uint32_t memory_id;
    uint32_t peer_endpoint;
    uint32_t egress_port;
    uint32_t peer_ingress_port;
    /* Local TX orientation; RX demultiplexes destination MAC/IP/VLAN/QPN.
     * add_native_qp derives this automatically from its transport endpoint. */
    bool has_local_endpoint;
    NativeEndpoint local_endpoint;

    EngineQpBinding();
};

struct EngineQpRuntimeView {
    QpRuntimeView qp;
    EngineQpBinding binding;

    EngineQpRuntimeView();
};

enum class EngineState {
    NEW = 0,
    CONFIGURED,
    RUNNING,
    ERROR,
    STOPPED
};

const char *engine_state_to_string(EngineState state);

struct EngineConfig {
    SessionGeneration generation;
    uint32_t endpoint_id;
    uint32_t max_qps;
    uint32_t max_memory_regions;
    uint64_t max_total_memory_bytes;
    uint32_t max_completion_records;
    uint32_t max_ingress_frames;
    uint32_t max_tx_frames;
    uint32_t max_frame_bytes;
    uint32_t ingress_budget_per_poll;
    uint32_t tx_budget_per_poll;

    EngineConfig();
    bool validate(Status *status = NULL) const;
};

struct EngineCounters {
    uint64_t polls;
    uint64_t ingress_enqueued;
    uint64_t ingress_processed;
    uint64_t ingress_stale_generation;
    uint64_t ingress_wrong_endpoint;
    uint64_t ingress_malformed;
    uint64_t ingress_unknown_qp;
    uint64_t ingress_backpressure;
    uint64_t tx_enqueued;
    uint64_t tx_drained;
    uint64_t tx_backpressure;
    uint64_t post_write_calls;
    uint64_t post_read_calls;
    uint64_t work_completions;
    uint64_t monotonic_time_errors;
    uint64_t teardown_calls;

    EngineCounters();
};

struct EngineQpSnapshot {
    QpSnapshot qp;
    EngineQpBinding binding;
    EngineTransportSnapshot transport;

    EngineQpSnapshot();
};

struct EngineSnapshot {
    EngineState state;
    EngineConfig config;
    uint64_t publication_epoch;
    uint64_t last_poll_ns;
    bool has_polled;
    uint32_t ingress_queued;
    uint32_t tx_queued;
    uint32_t pending_transports;
    uint64_t total_memory_bytes;
    NativeStorageSnapshot ingress_storage;
    uint32_t ingress_queue_capacity;
    uint32_t ingress_queue_high_watermark;
    uint32_t tx_queue_capacity;
    uint32_t tx_queue_high_watermark;
    uint64_t hot_path_growth_events;
    std::vector<EngineMemorySnapshot> memories;
    std::vector<EngineQpSnapshot> qps;
    EngineCounters counters;
    ErrorCode error_code;
    std::string error_reason;

    EngineSnapshot();
};

struct EnginePollResult {
    uint32_t ingress_processed;
    uint32_t frames_produced;
    uint32_t transports_polled;
    bool made_progress;

    EnginePollResult();
};

class NativeEndpointEngine {
public:
    NativeEndpointEngine();
    ~NativeEndpointEngine();

    static bool estimate_storage_bytes(const EngineConfig &config,
                                       uint64_t &bytes,
                                       Status *status = NULL);

    NativeEndpointEngine(const NativeEndpointEngine &) = delete;
    NativeEndpointEngine &operator=(const NativeEndpointEngine &) = delete;

    bool configure(const EngineConfig &config, Status *status = NULL);
    bool add_memory(const EngineMemoryConfig &config, Status *status = NULL);
    bool remove_memory(uint32_t memory_id, Status *status = NULL);
    bool rebind_memory_keys(uint32_t memory_id, uint32_t lkey, uint32_t rkey,
                             Status *status = NULL);
    bool add_qp(std::unique_ptr<QueuePair> qp,
                std::unique_ptr<EngineTransport> transport,
                const EngineQpBinding &binding,
                Status *status = NULL);
    bool add_native_qp(std::unique_ptr<QueuePair> qp,
                       const NativeTransportConfig &transport_config,
                       const EngineQpBinding &binding,
                       Status *status = NULL);
    bool start(Status *status = NULL);
    bool validate_local_key(uint32_t lkey, uint32_t protection_domain,
                            uint64_t address, uint64_t length, Status *status = NULL) const;
    bool write_local_key(uint32_t lkey, uint32_t protection_domain,
                         uint64_t address, ByteSpan value, Status *status = NULL);
    bool invalidate_remote_key(uint32_t rkey, uint32_t protection_domain,
                               bool apply, Status *status = NULL);
    bool validate_remote_access(uint32_t rkey, uint32_t pd, uint64_t address,
                                uint64_t length, MemoryAccess required,
                                Status *status = NULL) const;
    bool view_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                         uint64_t length, MemoryAccess required,
                         ByteSpan &out, Status *status = NULL) const;
    bool write_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                          ByteSpan value, Status *status = NULL);
    bool atomic_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                           bool fetch_add, uint64_t compare, uint64_t swap_add,
                           uint64_t &original, Status *status = NULL);
    bool activate_qp(const QpId &qp, Status *status = NULL);
    bool drain_qp(const QpId &qp, Status *status = NULL);
    bool resume_qp(const QpId &qp, Status *status = NULL);
    bool close_qp(const QpId &qp, ErrorCode error, const std::string &reason,
                   Status *status = NULL);
    bool post_receive(const QpId &qp, const EngineReceiveRequest &request,
                      Status *status = NULL);
    bool post_send(const QpId &qp, const EngineSendRequest &request,
                   uint64_t now_ns, PostedWork &posted, Status *status = NULL);
    bool post_atomic(const QpId &qp, const EngineAtomicRequest &request,
                     uint64_t now_ns, PostedWork &posted, Status *status = NULL);

    bool post_write(const QpId &qp,
                    const EngineWriteRequest &request,
                    uint64_t now_ns,
                    PostedWork &posted,
                    Status *status = NULL);
    bool post_read(const QpId &qp,
                   const EngineReadRequest &request,
                   uint64_t now_ns,
                   PostedWork &posted,
                   Status *status = NULL);
    bool drain_completions(const QpId &qp,
                           std::vector<CompletionRecord> &output,
                           Status *status = NULL);
    bool drain_completions_bounded(const QpId &qp,
                                   std::vector<CompletionRecord> &output,
                                   size_t max_records,
                                   Status *status = NULL);

    bool enqueue_ingress(EngineWireFrame frame, Status *status = NULL);
    bool enqueue_ingress_copy(const EngineWireFrame &metadata,
                              const uint8_t *frame,
                              size_t length,
                              Status *status = NULL);
    bool poll(uint64_t now_ns,
              EnginePollResult &result,
              Status *status = NULL);
    bool drain_tx(uint32_t frame_budget,
                  std::vector<EngineWireFrame> &output,
                  Status *status = NULL);
    const EngineWireFrame *peek_tx() const;
    /* Inspect a pending burst without publishing acceptance.  Pointers remain
     * valid until the owning worker mutates the queue; only accept_tx retires
     * a frame and arms its transport deadline after device acceptance. */
    const EngineWireFrame *peek_tx_at(size_t index) const;
    bool accept_tx(uint64_t sequence,
                   uint64_t accepted_ns,
                   Status *status = NULL);

    NativeMemoryBuffer *memory(uint32_t memory_id);
    const NativeMemoryBuffer *memory(uint32_t memory_id) const;
    bool resolve_memory(uint64_t address,
                        uint64_t length,
                        MemoryAccess required,
                        uint32_t &memory_id,
                        Status *status = NULL) const;
    QueuePair *queue_pair(const QpId &qp);
    const QueuePair *queue_pair(const QpId &qp) const;
    bool qp_runtime_view(const QpId &qp,
                         EngineQpRuntimeView &view,
                         Status *status = NULL) const;
    bool transport_runtime_view(const QpId &qp,
                                NativeTransportRuntimeView &view,
                                Status *status = NULL) const;

    bool has_pending_work() const;
    EngineState state() const;
    uint32_t endpoint_id() const;
    const SessionGeneration &generation() const;
    EngineSnapshot snapshot() const;
    void enter_error(ErrorCode error, const std::string &reason);
    bool teardown(const SessionGeneration &generation, Status *status = NULL);

private:
    struct QpSlot {
        std::unique_ptr<QueuePair> qp;
        std::unique_ptr<EngineTransport> transport;
        EngineQpBinding binding;

        QpSlot();
        QpSlot(QpSlot &&other);
        QpSlot &operator=(QpSlot &&other);

    private:
        QpSlot(const QpSlot &);
        QpSlot &operator=(const QpSlot &);
    };

    typedef std::vector<QpSlot>::iterator QpIterator;
    typedef std::vector<QpSlot>::const_iterator ConstQpIterator;

    QpIterator find_qp(const QpId &qp);
    ConstQpIterator find_qp(const QpId &qp) const;
    typedef std::array<uint8_t, 32> WireQpKey;
    struct WireQpHash {
        size_t operator()(const WireQpKey &key) const;
    };
    static WireQpKey wire_key(uint32_t qpn, const NativeEndpoint &endpoint, bool incoming);
    QpIterator find_wire_qpn(const DecodedPacket &packet);
    NativeMemoryBuffer *find_memory(uint32_t memory_id);
    const NativeMemoryBuffer *find_memory(uint32_t memory_id) const;
    bool require_running(const char *operation, Status *status) const;
    bool validate_now(uint64_t now_ns, Status *status);
    bool route_one_ingress(DecodedPacket &packet,
                           QpSlot *&slot,
                           bool &consumed,
                           Status *status);
    bool process_one_ingress(QpSlot &slot,
                             const DecodedPacket &packet,
                             uint64_t now_ns,
                             Status *status);
    EngineIngressAdmission ingress_admission(
        const QpSlot &slot,
        const DecodedPacket &packet) const;
    bool defer_one_ingress(Status *status);
    bool poll_transport_once(QpSlot &slot,
                             uint64_t now_ns,
                             uint32_t &budget,
                             EnginePollResult &result,
                             bool &produced,
                             Status *status);
    bool enqueue_transport_frames(QpSlot &slot,
                                  std::vector<NativeFrameBuffer> &frames,
                                  uint64_t now_ns,
                                  Status *status);
    void unwind_reverse(ErrorCode error, const std::string &reason);

private:
    EngineState m_state;
    EngineConfig m_config;
    std::vector<std::unique_ptr<NativeMemoryBuffer> > m_memories;
    std::vector<QpSlot> m_qps;
    /* Allocated only when configuring/adding registrations or QPs. Lookups
     * never insert entries or allocate on the dataplane path. Indices remain
     * stable because QP slots are retained until session teardown. */
    std::unordered_map<uint32_t, size_t> m_qp_index;
    std::unordered_map<WireQpKey, size_t, WireQpHash> m_wire_qps;
    std::unordered_map<uint32_t, size_t> m_legacy_wire_qps;
    std::unordered_map<uint32_t, size_t> m_wire_qpn_counts;
    std::unordered_map<uint32_t, NativeMemoryBuffer *> m_memory_index;
    std::unordered_map<uint64_t, NativeMemoryBuffer *> m_local_keys;
    std::unordered_map<uint64_t, NativeMemoryBuffer *> m_remote_keys;
    /* Declared before every holder so intrusive frame handles are destroyed
     * before their backing slab. */
    NativeFramePool m_ingress_frames;
    NativeBoundedRing<EngineWireFrame> m_ingress;
    NativeBoundedRing<EngineWireFrame> m_tx;
    /* Reused single-frame poll result; avoids an outer-vector allocation on
     * every QP visit while retaining transport fair sharing. */
    std::vector<NativeFrameBuffer> m_poll_frames;
    /* Persistent owner tokens used when blocked ingress is rotated.  The
     * barrier survives poll-budget boundaries so independent QPs can bypass
     * without reordering one QP's responder PSNs or allowing later traffic to
     * overtake a terminal NAK. */
    std::vector<uint64_t> m_ingress_request_barrier;
    std::vector<uint64_t> m_ingress_response_barrier;
    std::vector<uint64_t> m_ingress_terminal_nak_barrier;
    std::vector<uint32_t> m_qp_tx_queued;
    uint64_t m_total_memory_bytes;
    uint64_t m_next_frame_sequence;
    uint64_t m_next_ingress_token;
    size_t m_rr_cursor;
    uint64_t m_last_poll_ns;
    bool m_has_polled;
    uint64_t m_publication_epoch;
    EngineCounters m_counters;
    ErrorCode m_error_code;
    std::string m_error_reason;
};

enum class LinkFaultKind {
    DROP = 0,
    BLACKHOLE,
    DUPLICATE,
    DELAY,
    CORRUPT,
    REORDER_AFTER_NEXT
};

struct LinkFaultRule {
    /* Zero means any global frame ordinal. */
    uint64_t frame_ordinal;
    uint32_t source_endpoint;
    uint32_t destination_endpoint;
    /* INVALID_QP_INDEX, 0xffffffff PSN, and -1 opcode mean any. */
    uint32_t source_qp_index;
    uint32_t psn;
    int32_t opcode;
    /* Apply to one matching occurrence, every Nth match, or all matches. */
    uint64_t match_occurrence;
    uint64_t every_n;
    /* Deterministic probability in parts per million; no global RNG state. */
    uint32_t probability_ppm;
    uint64_t probability_seed;
    bool one_shot;
    LinkFaultKind kind;
    uint64_t delay_ns;
    uint32_t duplicate_count;
    uint32_t corrupt_offset;
    uint8_t corrupt_xor;

    LinkFaultRule();
};

struct InMemoryLinkConfig {
    uint32_t max_endpoints;
    uint32_t max_routes;
    uint32_t max_in_flight_frames;
    uint32_t max_fault_rules;
    uint32_t max_frames_per_step;
    uint64_t propagation_delay_ns;
    uint64_t idle_step_ns;

    InMemoryLinkConfig();
    bool validate(Status *status = NULL) const;
};

struct InMemoryRoute {
    uint32_t source_endpoint;
    uint32_t egress_port;
    uint32_t destination_endpoint;
    uint32_t ingress_port;

    InMemoryRoute();
};

struct InMemoryLinkCounters {
    uint64_t pump_steps;
    uint64_t frames_captured;
    uint64_t frames_delivered;
    uint64_t frames_dropped;
    uint64_t frames_blackholed;
    uint64_t frames_duplicated;
    uint64_t frames_delayed;
    uint64_t frames_corrupted;
    uint64_t frames_reordered;
    uint64_t frames_unroutable;
    uint64_t endpoint_backpressure;

    InMemoryLinkCounters();
};

struct InMemoryLinkSnapshot {
    uint64_t now_ns;
    uint32_t endpoints;
    uint32_t routes;
    uint32_t in_flight_frames;
    uint32_t held_reorder_frames;
    InMemoryLinkCounters counters;

    InMemoryLinkSnapshot();
};

class NativeInMemoryLink {
public:
    NativeInMemoryLink();

    NativeInMemoryLink(const NativeInMemoryLink &) = delete;
    NativeInMemoryLink &operator=(const NativeInMemoryLink &) = delete;

    bool configure(const InMemoryLinkConfig &config, Status *status = NULL);
    bool attach(NativeEndpointEngine *endpoint, Status *status = NULL);
    bool add_route(const InMemoryRoute &route, Status *status = NULL);
    bool add_fault(const LinkFaultRule &fault, Status *status = NULL);

    /* Poll, route, and deliver at most the configured frame budget once. */
    bool step(uint64_t now_ns, bool &made_progress, Status *status = NULL);

    /*
     * Drive both sides until no engine/link work remains.  When a transport is
     * waiting on a deadline, time advances by idle_step_ns.  max_steps makes a
     * broken retry machine a deterministic test failure instead of a hang.
     */
    bool pump_until_quiescent(uint64_t start_ns,
                              uint64_t max_steps,
                              uint64_t &finish_ns,
                              Status *status = NULL);

    InMemoryLinkSnapshot snapshot() const;
    void reset();

private:
    struct PendingFrame {
        uint64_t delivery_ns;
        uint64_t order;
        EngineWireFrame frame;

        PendingFrame();
    };

    struct HeldFrame {
        uint32_t source_endpoint;
        uint32_t destination_endpoint;
        EngineWireFrame frame;

        HeldFrame();
    };

    NativeEndpointEngine *find_endpoint(uint32_t endpoint_id) const;
    const InMemoryRoute *find_route(uint32_t source_endpoint, uint32_t egress_port) const;
    bool capture_tx(NativeEndpointEngine &endpoint,
                    uint64_t now_ns,
                    uint32_t &budget,
                    bool &made_progress,
                    Status *status);
    bool apply_faults_and_queue(EngineWireFrame frame,
                                uint64_t now_ns,
                                Status *status);
    bool queue_pending(EngineWireFrame frame,
                       uint64_t delivery_ns,
                       Status *status);
    bool deliver_due(uint64_t now_ns,
                     uint32_t &budget,
                     bool &made_progress,
                     Status *status);
    bool fault_matches(size_t fault_index,
                       const EngineWireFrame &frame,
                       uint64_t ordinal);
    bool all_quiescent() const;

private:
    bool m_configured;
    InMemoryLinkConfig m_config;
    std::vector<NativeEndpointEngine *> m_endpoints;
    std::vector<InMemoryRoute> m_routes;
    std::vector<LinkFaultRule> m_faults;
    std::vector<bool> m_fault_consumed;
    std::vector<uint64_t> m_fault_match_counts;
    std::vector<PendingFrame> m_pending;
    std::vector<HeldFrame> m_held;
    uint64_t m_now_ns;
    uint64_t m_frame_ordinal;
    uint64_t m_delivery_order;
    InMemoryLinkCounters m_counters;
};

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_ENGINE_H__ */
