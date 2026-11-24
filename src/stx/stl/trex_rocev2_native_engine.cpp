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

#include "trex_rocev2_native_engine.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <string.h>
#include <utility>

namespace TrexRoceV2 {
namespace Native {

namespace {

static const uint64_t FNV1A_OFFSET_BASIS = 1469598103934665603ULL;
static const uint64_t FNV1A_PRIME = 1099511628211ULL;

static uint64_t registry_key(uint32_t pd, uint32_t key) {
    return (uint64_t(pd) << 32) | key;
}

static bool add_u64_overflows(uint64_t left, uint64_t right) {
    return right > std::numeric_limits<uint64_t>::max() - left;
}

static bool add_storage_reservation(uint64_t count,
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

static bool size_fits_platform(uint64_t value) {
    return value <= static_cast<uint64_t>(std::numeric_limits<size_t>::max());
}

static bool qpid_generation_matches(const QpId &id,
                                     const SessionGeneration &generation) {
    return id.valid() && id.generation() == generation;
}

static bool endpoint_filter_matches(uint32_t filter, uint32_t endpoint) {
    return filter == ENGINE_ANY_ENDPOINT || filter == endpoint;
}

static bool is_rc_request_stream_opcode(IbOpcode opcode) {
    return opcode == IB_SEND_FIRST || opcode == IB_SEND_MIDDLE || opcode == IB_SEND_LAST ||
           opcode == IB_SEND_ONLY || opcode == IB_SEND_ONLY_WITH_IMMEDIATE ||
           opcode == IB_SEND_LAST_WITH_IMMEDIATE || opcode == IB_SEND_ONLY_WITH_INVALIDATE ||
           opcode == IB_SEND_LAST_WITH_INVALIDATE || opcode == IB_COMPARE_SWAP || opcode == IB_FETCH_ADD ||
           opcode == IB_RDMA_WRITE_FIRST ||
           opcode == IB_RDMA_WRITE_MIDDLE ||
           opcode == IB_RDMA_WRITE_LAST ||
           opcode == IB_RDMA_WRITE_LAST_WITH_IMMEDIATE ||
           opcode == IB_RDMA_WRITE_ONLY ||
           opcode == IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE ||
           opcode == IB_RDMA_READ_REQUEST;
}

static bool is_rc_response_stream_opcode(IbOpcode opcode) {
    return opcode == IB_ATOMIC_ACKNOWLEDGE || opcode == IB_ACKNOWLEDGE ||
           opcode == IB_RDMA_READ_RESPONSE_FIRST ||
           opcode == IB_RDMA_READ_RESPONSE_MIDDLE ||
           opcode == IB_RDMA_READ_RESPONSE_LAST ||
           opcode == IB_RDMA_READ_RESPONSE_ONLY;
}

static bool is_rc_read_response_opcode(IbOpcode opcode) {
    return opcode == IB_ATOMIC_ACKNOWLEDGE || opcode == IB_RDMA_READ_RESPONSE_FIRST ||
           opcode == IB_RDMA_READ_RESPONSE_MIDDLE ||
           opcode == IB_RDMA_READ_RESPONSE_LAST ||
           opcode == IB_RDMA_READ_RESPONSE_ONLY;
}

static bool is_fatal_aeth(const DecodedPacket &packet) {
    return packet.opcode == IB_ACKNOWLEDGE && packet.has_aeth &&
           packet.aeth.syndrome >= 0x61U && packet.aeth.syndrome <= 0x63U;
}

} // namespace

EngineMemoryConfig::EngineMemoryConfig() :
    memory_id(0),
    initial_value(0) {
}

EngineMemorySnapshot::EngineMemorySnapshot() :
    memory_id(0),
    bytes(0),
    checksum(0),
    read_operations(0),
    write_operations(0),
    bytes_read(0),
    bytes_written(0) {
}

NativeMemoryBuffer::NativeMemoryBuffer() :
    m_memory_id(0),
    m_checksum_cache(FNV1A_OFFSET_BASIS),
    m_checksum_dirty(false),
    m_read_operations(0),
    m_write_operations(0),
    m_bytes_read(0),
    m_bytes_written(0) {
}

bool NativeMemoryBuffer::configure(const EngineMemoryConfig &config,
                                   uint64_t maximum_bytes,
                                   Status *status) {
    if (configured()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "memory buffer is already configured");
    }
    if (config.memory_id == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "memory ID zero is reserved");
    }
    if (maximum_bytes == 0 || config.window.length > maximum_bytes) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory buffer exceeds the configured byte limit");
    }
    if (!size_fits_platform(config.window.length)) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory buffer cannot be represented on this platform");
    }

    MemoryWindow candidate;
    if (!candidate.configure(config.window, status)) {
        return false;
    }
    std::vector<uint8_t> backing;
    try {
        backing.assign(static_cast<size_t>(config.window.length), config.initial_value);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory buffer allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory buffer length exceeds vector capacity");
    }

    try { m_retired_keys.reserve(128); } catch (const std::bad_alloc &) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "registration key history allocation failed");
    }
    m_memory_id = config.memory_id;
    m_window = candidate;
    m_bytes.swap(backing);
    m_checksum_cache = FNV1A_OFFSET_BASIS;
    m_checksum_dirty = true;
    m_read_operations = 0;
    m_write_operations = 0;
    m_bytes_read = 0;
    m_bytes_written = 0;
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::view(uint64_t address,
                              uint64_t length,
                              MemoryAccess required,
                              ByteSpan &span,
                              Status *status) const {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (!window.configured || !window.active) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "memory window is not active");
    }
    if (length == 0) {
        /* A zero-byte work request consumes protocol state but no backing. */
        const bool one_past =
            window.last_address != std::numeric_limits<uint64_t>::max() &&
            address == window.last_address + 1;
        if (address < window.base_address ||
            (address > window.last_address && !one_past)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "zero-length memory address is outside the registered window");
        }
        if (!has_memory_access(window.access, required)) {
            return status_error(status,
                                ErrorCode::ACCESS_DENIED,
                                "memory window does not grant the requested access");
        }
        span = ByteSpan(NULL, 0);
        m_read_operations++;
        status_ok(status);
        return true;
    }
    if (length > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory view length cannot be represented on this platform");
    }
    uint64_t offset = 0;
    if (!m_window.validate(window.rkey,
                           address,
                           length,
                           required,
                           &offset,
                           status)) {
        return false;
    }
    if (!size_fits_platform(offset) || !size_fits_platform(length)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory view cannot be represented on this platform");
    }
    const size_t native_offset = static_cast<size_t>(offset);
    const size_t native_length = static_cast<size_t>(length);
    if (native_offset > m_bytes.size() || native_length > m_bytes.size() - native_offset) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "memory window metadata exceeds backing storage");
    }
    span = ByteSpan(&m_bytes[native_offset], native_length);
    m_read_operations++;
    m_bytes_read += length;
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::read(uint64_t address,
                              uint64_t length,
                              MemoryAccess required,
                              std::vector<uint8_t> &output,
                              Status *status) const {
    ByteSpan source;
    if (!view(address, length, required, source, status)) {
        return false;
    }
    if (source.size == 0) {
        output.clear();
        status_ok(status);
        return true;
    }
    try {
        output.assign(source.data, source.data + source.size);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory read result allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory read result exceeds vector capacity");
    }
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::write(uint64_t address,
                               ByteSpan value,
                               MemoryAccess required,
                               Status *status) {
    if (value.size == 0) {
        uint64_t unused_offset = 0;
        if (!offset_of(address, 0, required, unused_offset, status)) {
            return false;
        }
        m_write_operations++;
        status_ok(status);
        return true;
    }
    if (value.data == NULL) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "non-empty memory write requires a value pointer");
    }
    const MemoryWindowSnapshot window = m_window.snapshot();
    uint64_t offset = 0;
    if (!m_window.validate(window.rkey,
                           address,
                           static_cast<uint64_t>(value.size),
                           required,
                           &offset,
                           status)) {
        return false;
    }
    if (!size_fits_platform(offset)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory write offset cannot be represented");
    }
    const size_t native_offset = static_cast<size_t>(offset);
    if (native_offset > m_bytes.size() || value.size > m_bytes.size() - native_offset) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "memory write exceeds backing storage");
    }
    memmove(&m_bytes[native_offset], value.data, value.size);
    m_checksum_dirty = true;
    m_write_operations++;
    m_bytes_written += static_cast<uint64_t>(value.size);
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::fill(uint64_t address,
                              uint64_t length,
                              uint8_t value,
                              MemoryAccess required,
                              Status *status) {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (length == 0) {
        uint64_t unused_offset = 0;
        if (!offset_of(address, 0, required, unused_offset, status)) {
            return false;
        }
        m_write_operations++;
        status_ok(status);
        return true;
    }
    uint64_t offset = 0;
    if (!m_window.validate(window.rkey,
                           address,
                           length,
                           required,
                           &offset,
                           status)) {
        return false;
    }
    if (!size_fits_platform(offset) || !size_fits_platform(length)) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "memory fill range cannot be represented");
    }
    const size_t native_offset = static_cast<size_t>(offset);
    const size_t native_length = static_cast<size_t>(length);
    if (native_offset > m_bytes.size() || native_length > m_bytes.size() - native_offset) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "memory fill exceeds backing storage");
    }
    memset(&m_bytes[native_offset], value, native_length);
    m_checksum_dirty = true;
    m_write_operations++;
    m_bytes_written += length;
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::offset_of(uint64_t address,
                                   uint64_t length,
                                   MemoryAccess required,
                                   uint64_t &offset,
                                   Status *status) const {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (!window.configured || !window.active) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "memory window is not active");
    }
    if (length == 0) {
        const bool one_past =
            window.last_address != std::numeric_limits<uint64_t>::max() &&
            address == window.last_address + 1;
        if (address < window.base_address ||
            (address > window.last_address && !one_past)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "zero-length memory address is outside the registered window");
        }
        if (!has_memory_access(window.access, required)) {
            return status_error(status,
                                ErrorCode::ACCESS_DENIED,
                                "memory window does not grant the requested access");
        }
        offset = address - window.base_address;
        status_ok(status);
        return true;
    }
    if (!m_window.validate(window.rkey,
                           address,
                           length,
                           required,
                           &offset,
                           status)) {
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::validate_local_key(uint32_t lkey, uint32_t protection_domain,
                                               uint64_t address, uint64_t length,
                                               Status *status) const {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (!window.active || lkey != window.lkey || protection_domain != window.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "local key or protection domain does not match active memory");
    }
    uint64_t offset = 0;
    return offset_of(address, length, MemoryAccess::LOCAL_WRITE, offset, status);
}

bool NativeMemoryBuffer::validate_remote_access(uint32_t rkey, uint64_t address,
                                                 uint64_t length, MemoryAccess required,
                                                 Status *status) const {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (!window.remote_key_active || rkey != window.rkey) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "remote key is invalid or inactive");
    }
    if (length) return m_window.validate(rkey, address, length, required, NULL, status);
    uint64_t offset = 0;
    return offset_of(address, length, required, offset, status);
}

bool NativeMemoryBuffer::view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                                         MemoryAccess required, ByteSpan &out, Status *status) const {
    return NativeTransportMemory::view_remote_key(rkey, address, length, required, out, status);
}
bool NativeMemoryBuffer::write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value, Status *status) {
    return NativeTransportMemory::write_remote_key(rkey, address, value, status);
}

bool NativeMemoryBuffer::rebind_keys(uint32_t lkey, uint32_t rkey, Status *status) {
    const MemoryWindowSnapshot old = m_window.snapshot();
    if (!old.active || !lkey || !rkey || lkey == old.lkey || rkey == old.rkey) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT, "rebind requires fresh nonzero local and remote keys");
    }
    if (m_retired_keys.size() > 126) return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "registration key generation bound is exhausted");
    for (size_t i = 0; i < m_retired_keys.size(); ++i) {
        if (m_retired_keys[i] == lkey || m_retired_keys[i] == rkey) {
            return status_error(status, ErrorCode::STALE_GENERATION, "retired registration key cannot be reused");
        }
    }
    MemoryWindowConfig config;
    config.base_address = old.base_address;
    config.length = old.length;
    config.lkey = lkey;
    config.rkey = rkey;
    config.protection_domain = old.protection_domain;
    config.access = old.access;
    if (!m_window.configure(config, status)) return false;
    m_retired_keys.push_back(old.lkey);
    m_retired_keys.push_back(old.rkey);
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::invalidate_remote_key(uint32_t rkey, bool apply, Status *status) {
    const MemoryWindowSnapshot window = m_window.snapshot();
    if (!window.active || !window.remote_key_active || window.rkey != rkey) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "remote invalidation key is inactive or does not match");
    }
    if (apply) return m_window.invalidate_remote_key(rkey, status);
    status_ok(status);
    return true;
}

bool NativeMemoryBuffer::atomic(uint64_t address, uint32_t rkey, bool fetch_add,
                                uint64_t compare, uint64_t swap_add,
                                uint64_t &original, Status *status) {
    if ((address & 7U) != 0) return status_error(status, ErrorCode::INVALID_ARGUMENT, "atomic address is not aligned");
    uint64_t offset = 0;
    if (!m_window.validate(rkey, address, 8, MemoryAccess::REMOTE_ATOMIC, &offset, status)) return false;
    /* The owning dataplane worker serializes all access to this registration.
     * Never cast packet or vector storage to an atomic pointer: it may be unaligned.
     * An emulated 64-bit word is represented in native host byte order, as verbs
     * application memory is. AtomicETH and response headers use network order. */
    memcpy(&original, m_bytes.data() + offset, sizeof(original));
    const uint64_t replacement = fetch_add ? original + swap_add :
                                             (original == compare ? swap_add : original);
    memcpy(m_bytes.data() + offset, &replacement, sizeof(replacement));
    ++m_read_operations;
    ++m_write_operations;
    m_bytes_read += 8;
    m_bytes_written += 8;
    m_checksum_dirty = true;
    status_ok(status);
    return true;
}

void NativeMemoryBuffer::reset() {
    if (!m_bytes.empty()) {
        std::fill(m_bytes.begin(), m_bytes.end(), 0);
    }
    std::vector<uint8_t>().swap(m_bytes);
    m_window.reset();
    m_memory_id = 0;
    m_retired_keys.clear();
    m_checksum_cache = FNV1A_OFFSET_BASIS;
    m_checksum_dirty = false;
    m_read_operations = 0;
    m_write_operations = 0;
    m_bytes_read = 0;
    m_bytes_written = 0;
}

bool NativeMemoryBuffer::configured() const {
    return m_window.configured() && m_window.active();
}

uint32_t NativeMemoryBuffer::memory_id() const {
    return m_memory_id;
}

uint64_t NativeMemoryBuffer::checksum() const {
    if (!m_checksum_dirty) {
        return m_checksum_cache;
    }
    uint64_t value = FNV1A_OFFSET_BASIS;
    for (size_t index = 0; index < m_bytes.size(); index++) {
        value ^= static_cast<uint64_t>(m_bytes[index]);
        value *= FNV1A_PRIME;
    }
    m_checksum_cache = value;
    m_checksum_dirty = false;
    return m_checksum_cache;
}

EngineMemorySnapshot NativeMemoryBuffer::snapshot() const {
    EngineMemorySnapshot result;
    result.memory_id = m_memory_id;
    result.window = m_window.snapshot();
    result.bytes = static_cast<uint64_t>(m_bytes.size());
    result.checksum = checksum();
    result.read_operations = m_read_operations;
    result.write_operations = m_write_operations;
    result.bytes_read = m_bytes_read;
    result.bytes_written = m_bytes_written;
    return result;
}

EngineWireFrame::EngineWireFrame() :
    source_endpoint(ENGINE_ANY_ENDPOINT),
    destination_endpoint(ENGINE_ANY_ENDPOINT),
    source_qp_index(INVALID_QP_INDEX),
    egress_port(ENGINE_ANY_PORT),
    ingress_port(ENGINE_ANY_PORT),
    sequence(0),
    ready_ns(0),
    ingress_token(0) {
}

EngineWriteRequest::EngineWriteRequest() :
    source_memory_id(0),
    source_address(0),
    length(0),
    remote_address(0),
    remote_rkey(0),
    with_immediate(false),
    immediate_data(0),
    signalled(true), fenced(false) {
}

EngineReadRequest::EngineReadRequest() :
    destination_memory_id(0),
    destination_address(0),
    length(0),
    remote_address(0),
    remote_rkey(0),
    signalled(true), fenced(false) {
}

EngineTransportSnapshot::EngineTransportSnapshot() :
    attached(false),
    pending(false),
    queued_tx_frames(0),
    tx_packets(0),
    tx_bytes(0),
    rx_packets(0),
    rx_bytes(0),
    malformed_packets(0),
    retransmit_packets(0) {
}

NativeRcEngineTransport::NativeRcEngineTransport(
    const NativeTransportConfig &config,
    uint32_t max_completion_records) :
    m_config(config),
    m_max_completion_records(max_completion_records),
    m_qp(NULL),
    m_memory(NULL),
    m_engine(NULL) {
}

NativeRcEngineTransport::~NativeRcEngineTransport() {
    detach();
}

bool NativeRcEngineTransport::estimate_storage_bytes(
    const QpConfig &qp_config,
    const NativeTransportConfig &transport_config,
    uint32_t max_completion_records,
    uint64_t &bytes,
    Status *status) {
    bytes = 0;
    if (max_completion_records == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "RC completion storage bound must be non-zero");
    }
    if (max_completion_records < qp_config.sq_depth_messages) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "RC completion queue bound is smaller than SQ message depth");
    }
    if (transport_config.max_tx_frames >
        std::numeric_limits<uint32_t>::max() - 4U) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "RC adapter frame reservation overflowed");
    }

    uint64_t qp_bytes = 0;
    uint64_t transport_bytes = 0;
    if (!QueuePair::estimate_storage_bytes(qp_config, qp_bytes, status) ||
        !NativeRcTransport::estimate_storage_bytes(
            qp_config, transport_config, transport_bytes, status)) {
        return false;
    }
    if (!add_storage_reservation(1,
                                 sizeof(NativeRcEngineTransport),
                                 bytes,
                                 status,
                                 "RC adapter object") ||
        !add_storage_reservation(1,
                                 qp_bytes,
                                 bytes,
                                 status,
                                 "RC QP") ||
        !add_storage_reservation(1,
                                 transport_bytes,
                                 bytes,
                                 status,
                                 "RC transport")) {
        return false;
    }

    const uint64_t frame_slots =
        static_cast<uint64_t>(transport_config.max_tx_frames) + 4U;
    if (!add_storage_reservation(frame_slots,
                                 sizeof(NativeTransportFrame),
                                 bytes,
                                 status,
                                 "RC adapter pending-frame ring") ||
        !add_storage_reservation(frame_slots,
                                 sizeof(NativeTransportFrame),
                                 bytes,
                                 status,
                                 "RC adapter output frame vector") ||
        !add_storage_reservation(max_completion_records,
                                 sizeof(CompletionRecord),
                                 bytes,
                                 status,
                                 "RC adapter completion ring") ||
        !add_storage_reservation(max_completion_records,
                                 sizeof(CompletionRecord),
                                 bytes,
                                 status,
                                 "RC adapter output completion vector")) {
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::attach(QueuePair *qp,
                                     NativeMemoryBuffer *local_memory,
                                     Status *status) {
    if (m_transport.get() != NULL || m_qp != NULL || m_memory != NULL) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC engine transport is already attached");
    }
    if (qp == NULL || local_memory == NULL) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "RC engine transport requires a QP and memory buffer");
    }
    if (m_max_completion_records == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "RC completion queue bound must be non-zero");
    }
    const QpSnapshot qp_snapshot = qp->snapshot();
    if (qp_snapshot.has_config &&
        m_max_completion_records < qp_snapshot.config.sq_depth_messages) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "RC completion queue bound is smaller than SQ message depth");
    }
    if ((qp->state() != QpState::RTS && qp->state() != QpState::RTR) || !local_memory->configured()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC engine transport resources must be active");
    }
    if (m_config.max_tx_frames > std::numeric_limits<uint32_t>::max() - 4U) {
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "RC transmit queue bound cannot reserve control-frame slack");
    }
    if (!m_pending_frames.configure(
            static_cast<size_t>(m_config.max_tx_frames) + 4U, status) ||
        !m_pending_completions.configure(m_max_completion_records, status) ||
        !m_output.configure(static_cast<size_t>(m_config.max_tx_frames) + 4U,
                            m_max_completion_records,
                            status)) {
        return false;
    }
    std::unique_ptr<NativeRcTransport> transport(
        new (std::nothrow) NativeRcTransport(*qp));
    if (!transport.get()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC transport allocation failed");
    }
    m_qp = qp;
    m_memory = local_memory;
    if (!transport->configure(m_config, status) ||
        !transport->bind_memory(this, status)) {
        transport->detach();
        m_qp = NULL;
        m_memory = NULL;
        return false;
    }
    m_qp = qp;
    m_memory = local_memory;
    m_transport = std::move(transport);
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::absorb_output(NativeTransportOutput &output,
                                            Status *status) {
    const size_t max_frames = m_pending_frames.capacity();
    if (m_pending_frames.size() > max_frames ||
        output.frames.size() > max_frames - m_pending_frames.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC adapter pending-frame queue is full");
    }
    if (m_pending_completions.size() > m_max_completion_records ||
        output.completions.size() >
        m_max_completion_records - m_pending_completions.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC adapter completion queue is full");
    }
    for (size_t index = 0; index < output.frames.size(); index++) {
        if (!m_pending_frames.push(std::move(output.frames[index]), status)) {
            return false;
        }
    }
    for (size_t index = 0; index < output.completions.size(); index++) {
        if (!m_pending_completions.push_copy(output.completions[index], status)) {
            return false;
        }
    }
    output.clear();
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::preflight_completion_slot(Status *status) const {
    if (m_qp == NULL) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    const QpRuntimeView qp = m_qp->runtime_view();
    const NativeTransportRuntimeView transport = m_transport->runtime_view();
    const uint64_t reserved =
        static_cast<uint64_t>(m_pending_completions.size()) +
        qp.ledger.ready_completions + qp.ledger.outstanding_work + transport.posted_receives + transport.receive_cqes;
    if (reserved >= m_pending_completions.capacity()) {
        return status_error(
            status,
            ErrorCode::RESOURCE_EXHAUSTED,
            "RC completion queue must be drained before posting more work");
    }
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::post_write(const EngineWriteRequest &request,
                                         ByteSpan payload,
                                         uint64_t now_ns,
                                         PostedWork &posted,
                                         Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    if (payload.size != request.length) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "write payload length disagrees with the engine request");
    }
    const uint16_t pmtu = m_qp->runtime_view().negotiated_pmtu;
    const uint64_t needed_frames = payload.size == 0
                                       ? 1
                                       : 1 + ((payload.size - 1) / pmtu);
    if (m_pending_frames.size() > m_config.max_tx_frames ||
        needed_frames > m_config.max_tx_frames - m_pending_frames.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "write cannot fit the bounded RC transmit queue");
    }
    if (!preflight_completion_slot(status)) {
        return false;
    }
    NativeTransportWriteRequest native;
    native.remote_address = request.remote_address;
    native.remote_rkey = request.remote_rkey;
    native.with_immediate = request.with_immediate;
    native.immediate_data = request.immediate_data;
    native.signalled = request.signalled;
    native.fenced = request.fenced;
    m_output.clear();
    if (!m_transport->post_write(native,
                                 payload,
                                 now_ns,
                                 posted,
                                 m_output,
                                 status)) {
        return false;
    }
    return absorb_output(m_output, status);
}

bool NativeRcEngineTransport::post_read(const EngineReadRequest &request,
                                        uint64_t now_ns,
                                        PostedWork &posted,
                                        Status *status) {
    if (!m_transport.get() || m_memory == NULL) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    if (m_pending_frames.size() >= m_config.max_tx_frames) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "read request cannot fit the bounded RC transmit queue");
    }
    NativeTransportReadRequest native;
    native.remote_address = request.remote_address;
    native.remote_rkey = request.remote_rkey;
    uint64_t local_offset = 0;
    NativeMemoryBuffer *destination = m_engine ? m_engine->memory(request.destination_memory_id) : m_memory;
    if (!destination) return status_error(status, ErrorCode::NOT_FOUND, "read destination memory was not found");
    if (!destination->offset_of(request.destination_address,
                             request.length,
                             MemoryAccess::LOCAL_WRITE,
                             local_offset,
                             status)) {
        return false;
    }
    (void)local_offset;
    if (!preflight_completion_slot(status)) {
        return false;
    }
    native.local_address = request.destination_address;
    native.local_lkey = m_engine ? destination->window().lkey : 0;
    native.length = request.length;
    native.signalled = request.signalled;
    native.fenced = request.fenced;
    m_output.clear();
    if (!m_transport->post_read(native,
                                now_ns,
                                posted,
                                m_output,
                                status)) {
        return false;
    }
    return absorb_output(m_output, status);
}

bool NativeRcEngineTransport::read(uint64_t address, uint64_t length, MemoryAccess access,
                                   std::vector<uint8_t> &out, Status *status) const {
    return m_memory && m_memory->read(address, length, access, out, status);
}
bool NativeRcEngineTransport::view(uint64_t address, uint64_t length, MemoryAccess access,
                                   ByteSpan &out, Status *status) const {
    return m_memory && m_memory->view(address, length, access, out, status);
}
bool NativeRcEngineTransport::write(uint64_t address, ByteSpan value, MemoryAccess access, Status *status) {
    return m_memory && m_memory->write(address, value, access, status);
}
bool NativeRcEngineTransport::validate_remote_access(uint32_t rkey, uint64_t address,
                                                      uint64_t length, MemoryAccess required,
                                                      Status *status) const {
    return m_engine ? m_engine->validate_remote_access(rkey, m_qp->runtime_view().config.protection_domain,
                                                        address, length, required, status) :
                      (m_memory && m_memory->validate_remote_access(rkey, address, length, required, status));
}
bool NativeRcEngineTransport::view_remote_key(uint32_t rkey, uint64_t address, uint64_t length,
                                              MemoryAccess required, ByteSpan &out, Status *status) const {
    return m_engine ? m_engine->view_remote_key(rkey, m_qp->runtime_view().config.protection_domain,
                                                address, length, required, out, status) :
                      (m_memory && m_memory->view_remote_key(rkey, address, length, required, out, status));
}
bool NativeRcEngineTransport::write_remote_key(uint32_t rkey, uint64_t address, ByteSpan value, Status *status) {
    return m_engine ? m_engine->write_remote_key(rkey, m_qp->runtime_view().config.protection_domain,
                                                 address, value, status) :
                      (m_memory && m_memory->write_remote_key(rkey, address, value, status));
}

bool NativeRcEngineTransport::validate_local_key(uint32_t lkey, uint32_t pd,
                                                 uint64_t address, uint64_t length, Status *status) const {
    return m_engine ? m_engine->validate_local_key(lkey, pd, address, length, status) :
                     (m_memory && m_memory->validate_local_key(lkey, pd, address, length, status));
}
bool NativeRcEngineTransport::write_local_key(uint32_t lkey, uint32_t pd,
                                              uint64_t address, ByteSpan value, Status *status) {
    if (m_engine) return m_engine->write_local_key(lkey, pd, address, value, status);
    return NativeTransportMemory::write_local_key(lkey, pd, address, value, status);
}
bool NativeRcEngineTransport::invalidate_remote_key(uint32_t rkey, bool apply, Status *status) {
    return m_engine ? m_engine->invalidate_remote_key(rkey, m_qp->runtime_view().config.protection_domain, apply, status) :
                     (m_memory && m_memory->invalidate_remote_key(rkey, apply, status));
}
bool NativeRcEngineTransport::atomic(uint64_t address, uint32_t rkey, bool fetch_add,
                                     uint64_t compare, uint64_t swap_add, uint64_t &original,
                                     Status *status) {
    return m_engine ? m_engine->atomic_remote_key(rkey, m_qp->runtime_view().config.protection_domain,
                                                   address, fetch_add, compare, swap_add, original, status) :
                      (m_memory && m_memory->atomic(address, rkey, fetch_add, compare, swap_add, original, status));
}

bool NativeRcEngineTransport::post_receive(const EngineReceiveRequest &request, Status *status) {
    if (!m_transport) return status_error(status, ErrorCode::INVALID_STATE, "RC transport is not attached");
    if (!preflight_completion_slot(status)) return false;
    return m_transport->post_receive(request, status);
}

bool NativeRcEngineTransport::post_send(const EngineSendRequest &request, ByteSpan payload,
                                        uint64_t now_ns, PostedWork &posted, Status *status) {
    if (!m_transport || !preflight_completion_slot(status)) return false;
    const uint16_t pmtu = m_qp->runtime_view().negotiated_pmtu;
    const uint64_t frames = payload.size == 0 ? 1 : 1 + (payload.size - 1) / pmtu;
    if (payload.size != request.length || frames > m_config.max_tx_frames -
        std::min<size_t>(m_pending_frames.size(), m_config.max_tx_frames)) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "SEND does not fit bounded adapter output");
    }
    m_output.clear();
    if (!m_transport->post_send(request, payload, now_ns, posted, m_output, status)) return false;
    return absorb_output(m_output, status);
}

bool NativeRcEngineTransport::post_atomic(const EngineAtomicRequest &request,
                                          uint64_t now_ns, PostedWork &posted, Status *status) {
    if (!m_transport || !m_memory || !preflight_completion_slot(status)) return false;
    if (m_pending_frames.size() >= m_config.max_tx_frames) {
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "atomic does not fit bounded adapter output");
    }
    uint64_t offset = 0;
    NativeMemoryBuffer *destination = m_engine ? m_engine->memory(request.destination_memory_id) : m_memory;
    if (!destination) return status_error(status, ErrorCode::NOT_FOUND, "atomic destination memory was not found");
    if (!destination->offset_of(request.destination_address, 8, MemoryAccess::LOCAL_WRITE, offset, status)) return false;
    if (destination->window().protection_domain != m_qp->runtime_view().config.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "atomic destination belongs to another protection domain");
    }
    NativeTransportAtomicRequest native = request;
    native.local_address = request.destination_address;
    native.local_lkey = m_engine ? destination->window().lkey : 0;
    m_output.clear();
    if (!m_transport->post_atomic(native, now_ns, posted, m_output, status)) return false;
    return absorb_output(m_output, status);
}

bool NativeRcEngineTransport::on_frame(const EngineWireFrame &frame,
                                       const DecodedPacket &packet,
                                       uint64_t now_ns,
                                       Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    m_output.clear();
    if (!m_transport->on_frame(packet,
                               ByteSpan(frame.bytes.data(), frame.bytes.size()),
                               now_ns,
                               m_output,
                               status)) {
        return false;
    }
    return absorb_output(m_output, status);
}

bool NativeRcEngineTransport::on_frame_accepted(uint32_t psn,
                                                uint64_t accepted_ns,
                                                Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    return m_transport->on_frame_accepted(psn, accepted_ns, status);
}

bool NativeRcEngineTransport::flush_pending_frames(
    uint32_t budget,
    std::vector<NativeFrameBuffer> &frames,
    Status *status) {
    const size_t count = std::min<size_t>(budget, m_pending_frames.size());
    if (count > frames.capacity() - frames.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC adapter poll scratch capacity is exhausted");
    }
    for (size_t index = 0; index < count; index++) {
        frames.push_back(std::move(m_pending_frames.front().bytes));
        m_pending_frames.pop();
    }
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::poll(
    uint64_t now_ns,
    uint32_t frame_budget,
    std::vector<NativeFrameBuffer> &frames,
    Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    if (frame_budget == 0) {
        status_ok(status);
        return true;
    }
    if (!flush_pending_frames(frame_budget, frames, status)) {
        return false;
    }
    if (frames.size() >= frame_budget) {
        status_ok(status);
        return true;
    }
    m_output.clear();
    if (!m_transport->poll(now_ns,
                           frame_budget - frames.size(),
                           m_output,
                           status) ||
        !absorb_output(m_output, status)) {
        return false;
    }
    return flush_pending_frames(frame_budget - frames.size(), frames, status);
}

bool NativeRcEngineTransport::drain_completions(
    std::vector<CompletionRecord> &output,
    Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    m_output.clear();
    const size_t admitted = m_pending_completions.capacity() -
                            m_pending_completions.size();
    if (!m_transport->drain_completions_bounded(m_output.completions,
                                                 std::min(admitted,
                                                          m_output.completion_limit),
                                                 status) ||
        !absorb_output(m_output, status)) {
        return false;
    }
    try {
        output.reserve(output.size() + m_pending_completions.size());
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC completion result allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC completion result exceeds vector capacity");
    }
    try {
        while (!m_pending_completions.empty()) {
            output.push_back(m_pending_completions.front());
            m_pending_completions.pop();
        }
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC completion result allocation failed");
    }
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::drain_completions_bounded(
    std::vector<CompletionRecord> &output,
    size_t max_records,
    Status *status) {
    if (!m_transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "RC transport is not attached");
    }
    if (output.size() > output.capacity() ||
        max_records > output.capacity() - output.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "RC completion result exceeds admitted capacity");
    }
    m_output.clear();
    const size_t admitted = m_pending_completions.capacity() -
                            m_pending_completions.size();
    if (!m_transport->drain_completions_bounded(m_output.completions,
                                                 std::min(admitted,
                                                          m_output.completion_limit),
                                                 status) ||
        !absorb_output(m_output, status)) {
        return false;
    }
    const size_t count = std::min(max_records, m_pending_completions.size());
    for (size_t index = 0; index < count; ++index) {
        output.push_back(m_pending_completions.front());
        m_pending_completions.pop();
    }
    status_ok(status);
    return true;
}

bool NativeRcEngineTransport::has_pending_work() const {
    /* Ready completions are terminal evidence, not wire work. */
    return !m_pending_frames.empty() ||
           (m_transport.get() != NULL && m_transport->has_pending_work());
}

NativeTransportRuntimeView NativeRcEngineTransport::runtime_view() const {
    if (m_transport.get() == NULL) {
        return NativeTransportRuntimeView();
    }
    NativeTransportRuntimeView result = m_transport->runtime_view();
    const uint64_t queued = static_cast<uint64_t>(result.queued_tx_frames) +
                            m_pending_frames.size();
    result.queued_tx_frames = static_cast<uint32_t>(std::min<uint64_t>(
        queued, std::numeric_limits<uint32_t>::max()));
    return result;
}

EngineTransportSnapshot NativeRcEngineTransport::snapshot() const {
    EngineTransportSnapshot result;
    result.attached = m_transport.get() != NULL;
    result.queued_tx_frames = static_cast<uint32_t>(m_pending_frames.size());
    if (!m_transport.get()) {
        result.pending = !m_pending_frames.empty();
        return result;
    }
    const NativeTransportSnapshot native = m_transport->snapshot();
    result.native = native;
    result.qp = native.qp;
    result.pending = native.pending || !m_pending_frames.empty();
    result.queued_tx_frames += native.queued_tx_frames;
    result.tx_packets = native.counters.tx_packets;
    result.tx_bytes = native.counters.tx_bytes;
    result.rx_packets = native.counters.rx_packets;
    result.rx_bytes = native.counters.rx_bytes;
    result.malformed_packets = native.counters.malformed_packets;
    result.retransmit_packets = native.counters.retransmit_packets;
    return result;
}

void NativeRcEngineTransport::abort(ErrorCode error,
                                    const std::string &reason) {
    if (m_transport.get()) {
        m_transport->abort(error, reason);
    }
    m_pending_frames.clear();
}

void NativeRcEngineTransport::detach() {
    /* Adapter queues retain intrusive handles into m_transport's frame slab;
     * release every handle before destroying the pool owner. */
    m_output.clear();
    m_pending_frames.clear();
    m_pending_completions.clear();
    if (m_transport.get()) {
        m_transport->detach();
    }
    m_transport.reset();
    m_qp = NULL;
    m_memory = NULL;
}

EngineQpBinding::EngineQpBinding() :
    memory_id(0),
    peer_endpoint(ENGINE_ANY_ENDPOINT),
    egress_port(ENGINE_ANY_PORT),
    peer_ingress_port(ENGINE_ANY_PORT),
    has_local_endpoint(false) {
}

EngineQpRuntimeView::EngineQpRuntimeView() {
}

const char *engine_state_to_string(EngineState state) {
    switch (state) {
    case EngineState::NEW:
        return "NEW";
    case EngineState::CONFIGURED:
        return "CONFIGURED";
    case EngineState::RUNNING:
        return "RUNNING";
    case EngineState::ERROR:
        return "ERROR";
    case EngineState::STOPPED:
        return "STOPPED";
    }
    return "UNKNOWN";
}

EngineConfig::EngineConfig() :
    endpoint_id(ENGINE_ANY_ENDPOINT),
    max_qps(1024),
    max_memory_regions(1024),
    max_total_memory_bytes(1ULL << 30),
    max_completion_records(8192),
    max_ingress_frames(8192),
    max_tx_frames(8192),
    max_frame_bytes(TREX_MAX_ROCEV2_FRAME_SIZE),
    ingress_budget_per_poll(256),
    tx_budget_per_poll(256) {
}

bool EngineConfig::validate(Status *status) const {
    if (!generation.valid()) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine generation must be non-zero");
    }
    if (endpoint_id == ENGINE_ANY_ENDPOINT) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine endpoint ID is reserved");
    }
    if (max_qps == 0 || max_memory_regions == 0 || max_total_memory_bytes == 0 ||
        max_completion_records == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine resource limits must be non-zero");
    }
    if (max_ingress_frames == 0 || max_tx_frames == 0 || max_frame_bytes == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine queue and frame limits must be non-zero");
    }
    if (max_frame_bytes > TREX_MAX_ROCEV2_FRAME_SIZE) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine frame limit exceeds the RoCEv2 frame bound");
    }
    if (ingress_budget_per_poll == 0 || tx_budget_per_poll == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine poll budgets must be non-zero");
    }
    status_ok(status);
    return true;
}

EngineCounters::EngineCounters() :
    polls(0),
    ingress_enqueued(0),
    ingress_processed(0),
    ingress_stale_generation(0),
    ingress_wrong_endpoint(0),
    ingress_malformed(0),
    ingress_unknown_qp(0),
    ingress_backpressure(0),
    tx_enqueued(0),
    tx_drained(0),
    tx_backpressure(0),
    post_write_calls(0),
    post_read_calls(0),
    work_completions(0),
    monotonic_time_errors(0),
    teardown_calls(0) {
}

EngineQpSnapshot::EngineQpSnapshot() {
}

EngineSnapshot::EngineSnapshot() :
    state(EngineState::NEW),
    publication_epoch(0),
    last_poll_ns(0),
    has_polled(false),
    ingress_queued(0),
    tx_queued(0),
    pending_transports(0),
    total_memory_bytes(0),
    ingress_queue_capacity(0),
    ingress_queue_high_watermark(0),
    tx_queue_capacity(0),
    tx_queue_high_watermark(0),
    hot_path_growth_events(0),
    error_code(ErrorCode::OK) {
}

EnginePollResult::EnginePollResult() :
    ingress_processed(0),
    frames_produced(0),
    transports_polled(0),
    made_progress(false) {
}

NativeEndpointEngine::QpSlot::QpSlot() {
}

NativeEndpointEngine::QpSlot::QpSlot(QpSlot &&other) :
    qp(std::move(other.qp)),
    transport(std::move(other.transport)),
    binding(other.binding) {
}

NativeEndpointEngine::QpSlot &
NativeEndpointEngine::QpSlot::operator=(QpSlot &&other) {
    if (this != &other) {
        qp = std::move(other.qp);
        transport = std::move(other.transport);
        binding = other.binding;
    }
    return *this;
}

NativeEndpointEngine::NativeEndpointEngine() :
    m_state(EngineState::NEW),
    m_total_memory_bytes(0),
    m_next_frame_sequence(1),
    m_next_ingress_token(1),
    m_rr_cursor(0),
    m_last_poll_ns(0),
    m_has_polled(false),
    m_publication_epoch(0),
    m_error_code(ErrorCode::OK) {
}

NativeEndpointEngine::~NativeEndpointEngine() {
    unwind_reverse(ErrorCode::ABORTED, "native endpoint engine destroyed");
}

bool NativeEndpointEngine::estimate_storage_bytes(const EngineConfig &config,
                                                  uint64_t &bytes,
                                                  Status *status) {
    bytes = 0;
    if (!config.validate(status)) {
        return false;
    }

    uint64_t ingress_pool_bytes = 0;
    if (!NativeFramePool::estimate_storage_bytes(config.max_ingress_frames,
                                                  config.max_frame_bytes,
                                                  ingress_pool_bytes,
                                                  status) ||
        !add_storage_reservation(1,
                                 sizeof(NativeEndpointEngine),
                                 bytes,
                                 status,
                                 "native engine object") ||
        !add_storage_reservation(1,
                                 ingress_pool_bytes,
                                 bytes,
                                 status,
                                 "native engine ingress pool") ||
        !add_storage_reservation(config.max_ingress_frames,
                                 sizeof(EngineWireFrame),
                                 bytes,
                                 status,
                                 "native engine ingress ring") ||
        !add_storage_reservation(config.max_tx_frames,
                                 sizeof(EngineWireFrame),
                                 bytes,
                                 status,
                                 "native engine TX ring") ||
        !add_storage_reservation(config.max_qps,
                                 sizeof(QpSlot),
                                 bytes,
                                 status,
                                 "native engine QP table") ||
        !add_storage_reservation(config.max_qps, 512, bytes, status,
                                 "native engine QP identity indexes") ||
        !add_storage_reservation(config.max_memory_regions, 384, bytes, status,
                                 "native engine memory identity indexes") ||
        !add_storage_reservation(config.max_memory_regions,
                                 sizeof(std::unique_ptr<NativeMemoryBuffer>) +
                                     sizeof(NativeMemoryBuffer),
                                 bytes,
                                 status,
                                 "native engine memory table") ||
        !add_storage_reservation(config.max_memory_regions,
                                 128U * sizeof(uint32_t),
                                 bytes, status, "native registration retired-key history") ||
        !add_storage_reservation(config.max_qps,
                                 3U * sizeof(uint64_t) + sizeof(uint32_t),
                                 bytes,
                                 status,
                                 "native engine ingress barriers") ||
        !add_storage_reservation(1,
                                 sizeof(NativeFrameBuffer),
                                 bytes,
                                 status,
                                 "native engine poll scratch")) {
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::configure(const EngineConfig &config, Status *status) {
    if (m_state != EngineState::NEW && m_state != EngineState::STOPPED) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "engine configure requires NEW or STOPPED state");
    }
    if (!config.validate(status)) {
        return false;
    }
    if (!m_memories.empty() || !m_qps.empty() || !m_ingress.empty() || !m_tx.empty()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "stopped engine retained resources");
    }

    m_config = config;
    m_total_memory_bytes = 0;
    m_next_frame_sequence = 1;
    m_next_ingress_token = 1;
    m_rr_cursor = 0;
    m_last_poll_ns = 0;
    m_has_polled = false;
    m_counters = EngineCounters();
    m_error_code = ErrorCode::OK;
    m_error_reason.clear();
    if (!m_ingress.configure(config.max_ingress_frames, status) ||
        !m_tx.configure(config.max_tx_frames, status) ||
        !m_ingress_frames.configure(config.max_ingress_frames,
                                    config.max_frame_bytes,
                                    status)) {
        unwind_reverse(ErrorCode::RESOURCE_EXHAUSTED,
                       "engine admitted storage allocation failed");
        return false;
    }
    try {
        m_poll_frames.reserve(1);
        m_memories.reserve(config.max_memory_regions);
        m_qps.reserve(config.max_qps);
        m_qp_index.reserve(config.max_qps);
        m_wire_qps.reserve(config.max_qps);
        m_legacy_wire_qps.reserve(config.max_qps);
        m_wire_qpn_counts.reserve(config.max_qps);
        m_memory_index.reserve(config.max_memory_regions);
        m_local_keys.reserve(config.max_memory_regions);
        m_remote_keys.reserve(config.max_memory_regions);
        m_ingress_request_barrier.assign(config.max_qps, 0);
        m_ingress_response_barrier.assign(config.max_qps, 0);
        m_ingress_terminal_nak_barrier.assign(config.max_qps, 0);
        m_qp_tx_queued.assign(config.max_qps, 0);
    } catch (const std::bad_alloc &) {
        unwind_reverse(ErrorCode::RESOURCE_EXHAUSTED,
                       "engine admitted storage allocation failed");
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine admitted storage allocation failed");
    } catch (const std::length_error &) {
        unwind_reverse(ErrorCode::OUT_OF_RANGE,
                       "engine admitted storage exceeds container capacity");
        return status_error(status,
                            ErrorCode::OUT_OF_RANGE,
                            "engine admitted storage exceeds container capacity");
    }
    m_state = EngineState::CONFIGURED;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

NativeMemoryBuffer *NativeEndpointEngine::find_memory(uint32_t memory_id) {
    const auto found = m_memory_index.find(memory_id);
    return found == m_memory_index.end() ? NULL : found->second;
}

const NativeMemoryBuffer *
NativeEndpointEngine::find_memory(uint32_t memory_id) const {
    const auto found = m_memory_index.find(memory_id);
    return found == m_memory_index.end() ? NULL : found->second;
}

bool NativeEndpointEngine::add_memory(const EngineMemoryConfig &config,
                                      Status *status) {
    if (m_state != EngineState::CONFIGURED) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "memory can only be added before the engine starts");
    }
    if (m_memories.size() >= m_config.max_memory_regions) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine memory-region limit is exhausted");
    }
    if (find_memory(config.memory_id) != NULL) {
        return status_error(status,
                            ErrorCode::DUPLICATE,
                            "engine memory ID already exists");
    }
    const uint32_t lkey = config.window.lkey ? config.window.lkey : config.window.rkey;
    const uint64_t local_key = registry_key(config.window.protection_domain, lkey);
    const uint64_t remote_key = registry_key(config.window.protection_domain, config.window.rkey);
    if (m_local_keys.count(local_key) || m_remote_keys.count(remote_key)) {
        return status_error(status, ErrorCode::DUPLICATE, "memory key already registered in protection domain");
    }
    if (add_u64_overflows(m_total_memory_bytes, config.window.length) ||
        m_total_memory_bytes + config.window.length > m_config.max_total_memory_bytes) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine total memory limit is exhausted");
    }

    std::unique_ptr<NativeMemoryBuffer> memory(new (std::nothrow) NativeMemoryBuffer());
    if (!memory.get()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "memory-buffer object allocation failed");
    }
    if (!memory->configure(config,
                           m_config.max_total_memory_bytes - m_total_memory_bytes,
                           status)) {
        return false;
    }
    try {
        m_memory_index.emplace(config.memory_id, memory.get());
        m_local_keys.emplace(local_key, memory.get());
        m_remote_keys.emplace(remote_key, memory.get());
        m_memories.push_back(std::move(memory));
    } catch (const std::bad_alloc &) {
        m_memory_index.erase(config.memory_id);
        m_local_keys.erase(local_key);
        m_remote_keys.erase(remote_key);
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine memory table allocation failed");
    }
    m_total_memory_bytes += config.window.length;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::remove_memory(uint32_t memory_id, Status *status) {
    if (m_state != EngineState::CONFIGURED && m_state != EngineState::RUNNING) {
        return status_error(status, ErrorCode::INVALID_STATE, "memory removal requires configured or drained engine");
    }
    for (size_t i = 0; i < m_qps.size(); ++i) {
        if (m_qps[i].binding.memory_id == memory_id) return status_error(status, ErrorCode::INVALID_STATE, "memory is bound to a QP");
        const NativeTransportRuntimeView runtime = m_qps[i].transport->runtime_view();
        if (runtime.posted_receives || runtime.receive_cqes || runtime.outstanding_work ||
            runtime.qp.ledger.ready_completions || (!m_qps[i].qp->drained() && m_state == EngineState::RUNNING)) {
            return status_error(status, ErrorCode::INVALID_STATE, "registered memory may still be referenced by QP work");
        }
    }
    if (!m_ingress.empty() || !m_tx.empty()) return status_error(status, ErrorCode::INVALID_STATE, "engine wire queues are not empty");
    for (size_t i = 0; i < m_memories.size(); ++i) {
        if (m_memories[i]->memory_id() == memory_id) {
            const auto window = m_memories[i]->window();
            m_total_memory_bytes -= window.length;
            m_memory_index.erase(memory_id);
            m_local_keys.erase(registry_key(window.protection_domain, window.lkey));
            m_remote_keys.erase(registry_key(window.protection_domain, window.rkey));
            m_memories.erase(m_memories.begin() + i);
            ++m_publication_epoch;
            status_ok(status);
            return true;
        }
    }
    return status_error(status, ErrorCode::NOT_FOUND, "memory registration was not found");
}

bool NativeEndpointEngine::rebind_memory_keys(uint32_t memory_id, uint32_t lkey, uint32_t rkey, Status *status) {
    if (m_state != EngineState::CONFIGURED && m_state != EngineState::RUNNING) {
        return status_error(status, ErrorCode::INVALID_STATE, "memory rebind requires configured or drained engine");
    }
    NativeMemoryBuffer *memory = find_memory(memory_id);
    if (!memory) return status_error(status, ErrorCode::NOT_FOUND, "memory registration was not found");
    const MemoryWindowSnapshot old = memory->window();
    if (!m_ingress.empty() || !m_tx.empty()) return status_error(status, ErrorCode::INVALID_STATE, "engine wire queues are not empty");
    for (size_t i = 0; i < m_qps.size(); ++i) {
        const NativeTransportRuntimeView runtime = m_qps[i].transport->runtime_view();
        if (runtime.posted_receives || runtime.receive_cqes || runtime.outstanding_work ||
            runtime.qp.ledger.ready_completions || m_qps[i].transport->has_pending_work() ||
            (m_qps[i].qp->state() != QpState::RTR && !m_qps[i].qp->drained())) {
            return status_error(status, ErrorCode::INVALID_STATE, "memory rebind requires all QPs unstarted or drained without posted receives");
        }
    }
    const uint64_t local_key = registry_key(old.protection_domain, lkey);
    const uint64_t remote_key = registry_key(old.protection_domain, rkey);
    const auto local = m_local_keys.find(local_key), remote = m_remote_keys.find(remote_key);
    if ((local != m_local_keys.end() && local->second != memory) ||
        (remote != m_remote_keys.end() && remote->second != memory)) {
        return status_error(status, ErrorCode::DUPLICATE, "new memory key already registered in protection domain");
    }
    const bool add_local = local == m_local_keys.end(), add_remote = remote == m_remote_keys.end();
    try {
        if (add_local) m_local_keys.emplace(local_key, memory);
        if (add_remote) m_remote_keys.emplace(remote_key, memory);
    } catch (const std::bad_alloc &) {
        if (add_local) m_local_keys.erase(local_key);
        if (add_remote) m_remote_keys.erase(remote_key);
        return status_error(status, ErrorCode::RESOURCE_EXHAUSTED, "memory-key index allocation failed");
    }
    if (!memory->rebind_keys(lkey, rkey, status)) {
        if (add_local) m_local_keys.erase(local_key);
        if (add_remote) m_remote_keys.erase(remote_key);
        return false;
    }
    if (old.lkey != lkey) m_local_keys.erase(registry_key(old.protection_domain, old.lkey));
    if (old.rkey != rkey) m_remote_keys.erase(registry_key(old.protection_domain, old.rkey));
    MemoryWindowConfig config;
    config.base_address = old.base_address;
    config.length = old.length;
    config.access = old.access;
    config.protection_domain = old.protection_domain;
    config.lkey = lkey;
    config.rkey = rkey;
    for (size_t i = 0; i < m_qps.size(); ++i) {
        if (m_qps[i].binding.memory_id == memory_id && !m_qps[i].qp->update_local_window(config, status)) return false;
    }
    ++m_publication_epoch;
    status_ok(status);
    return true;
}

NativeEndpointEngine::QpIterator
NativeEndpointEngine::find_qp(const QpId &qp) {
    if (!qpid_generation_matches(qp, m_config.generation)) return m_qps.end();
    const auto found = m_qp_index.find(qp.qp_index());
    return found == m_qp_index.end() ? m_qps.end() : m_qps.begin() + found->second;
}

NativeEndpointEngine::ConstQpIterator
NativeEndpointEngine::find_qp(const QpId &qp) const {
    if (!qpid_generation_matches(qp, m_config.generation)) return m_qps.end();
    const auto found = m_qp_index.find(qp.qp_index());
    return found == m_qp_index.end() ? m_qps.end() : m_qps.begin() + found->second;
}

size_t NativeEndpointEngine::WireQpHash::operator()(const WireQpKey &key) const {
    uint64_t hash = FNV1A_OFFSET_BASIS;
    for (size_t i = 0; i < key.size(); ++i) { hash = (hash ^ key[i]) * FNV1A_PRIME; }
    return static_cast<size_t>(hash);
}

NativeEndpointEngine::WireQpKey NativeEndpointEngine::wire_key(
    uint32_t qpn, const NativeEndpoint &endpoint, bool incoming) {
    WireQpKey key = {{0}};
    key[0] = qpn >> 16; key[1] = qpn >> 8; key[2] = qpn;
    key[3] = endpoint.ipv6; key[4] = endpoint.has_vlan;
    if (endpoint.has_vlan) { key[5] = endpoint.vlan >> 8; key[6] = endpoint.vlan; }
    memcpy(key.data() + 7, incoming ? endpoint.dst_mac : endpoint.src_mac, 6);
    memcpy(key.data() + 13, incoming ? endpoint.dst_ip : endpoint.src_ip, endpoint.ipv6 ? 16 : 4);
    return key;
}

NativeEndpointEngine::QpIterator
NativeEndpointEngine::find_wire_qpn(const DecodedPacket &packet) {
    const auto qualified = m_wire_qps.find(wire_key(packet.dest_qpn, packet.endpoint, true));
    if (qualified != m_wire_qps.end()) return m_qps.begin() + qualified->second;
    const auto legacy = m_legacy_wire_qps.find(packet.dest_qpn);
    return legacy == m_legacy_wire_qps.end() ? m_qps.end() : m_qps.begin() + legacy->second;
}

bool NativeEndpointEngine::add_qp(std::unique_ptr<QueuePair> qp,
                                  std::unique_ptr<EngineTransport> transport,
                                  const EngineQpBinding &binding,
                                  Status *status) {
    if (m_state != EngineState::CONFIGURED && m_state != EngineState::RUNNING) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "QP can only be added before the engine starts");
    }
    if (!qp.get() || !transport.get()) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "QP and transport ownership must both be supplied");
    }
    if (m_qps.size() >= m_config.max_qps) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine QP limit is exhausted");
    }
    if (!qpid_generation_matches(qp->id(), m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "QP belongs to a different session generation");
    }
    if (qp->state() != QpState::RTS && qp->state() != QpState::RTR) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "engine only accepts an established RTS QP");
    }
    if (find_qp(qp->id()) != m_qps.end()) {
        return status_error(status, ErrorCode::DUPLICATE, "QP ID already exists");
    }
    const QpSnapshot incoming = qp->snapshot();
    if (!incoming.has_config) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT, "local wire QPN is missing");
    }
    const uint32_t qpn = incoming.config.local_qpn;
    const WireQpKey wire = wire_key(qpn, binding.local_endpoint, false);
    if ((binding.has_local_endpoint &&
         (m_legacy_wire_qps.count(qpn) || m_wire_qps.count(wire))) ||
        (!binding.has_local_endpoint && m_wire_qpn_counts.count(qpn))) {
        return status_error(status, ErrorCode::DUPLICATE,
                            "local endpoint identity and QPN are already bound or ambiguous");
    }
    NativeMemoryBuffer *local_memory = find_memory(binding.memory_id);
    if (local_memory == NULL) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "QP binding references an unknown memory buffer");
    }
    if (incoming.config.protection_domain != local_memory->window().protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED,
                            "QP and bound memory belong to different protection domains");
    }
    if (binding.peer_endpoint == ENGINE_ANY_ENDPOINT ||
        binding.egress_port == ENGINE_ANY_PORT ||
        binding.peer_ingress_port == ENGINE_ANY_PORT) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "QP link binding is incomplete");
    }
    transport->bind_registry(this);
    if (!transport->attach(qp.get(), local_memory, status)) {
        return false;
    }

    QpSlot slot;
    slot.qp = std::move(qp);
    slot.transport = std::move(transport);
    slot.binding = binding;
    const uint32_t index = slot.qp->id().qp_index();
    const size_t position = m_qps.size();
    try {
        m_qp_index.emplace(index, position);
        if (binding.has_local_endpoint) m_wire_qps.emplace(wire, position);
        else m_legacy_wire_qps.emplace(qpn, position);
        const auto count = m_wire_qpn_counts.emplace(qpn, 0);
        m_qps.push_back(std::move(slot));
        ++count.first->second;
    } catch (const std::bad_alloc &) {
        m_qp_index.erase(index);
        if (binding.has_local_endpoint) m_wire_qps.erase(wire);
        else m_legacy_wire_qps.erase(qpn);
        const auto count = m_wire_qpn_counts.find(qpn);
        if (count != m_wire_qpn_counts.end() && !count->second) m_wire_qpn_counts.erase(count);
        if (slot.transport.get()) {
            slot.transport->detach();
        }
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine QP table allocation failed");
    }
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::add_native_qp(
    std::unique_ptr<QueuePair> qp,
    const NativeTransportConfig &transport_config,
    const EngineQpBinding &binding,
    Status *status) {
    EngineQpBinding qualified = binding;
    if (binding.has_local_endpoint &&
        wire_key(0, binding.local_endpoint, false) != wire_key(0, transport_config.endpoint, false)) {
        return status_error(status, ErrorCode::INVALID_ARGUMENT,
                            "QP binding local identity differs from native transport endpoint");
    }
    qualified.has_local_endpoint = true;
    qualified.local_endpoint = transport_config.endpoint;
    std::unique_ptr<EngineTransport> transport(
        new (std::nothrow) NativeRcEngineTransport(transport_config,
                                                  m_config.max_completion_records));
    if (!transport.get()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "native RC adapter allocation failed");
    }
    return add_qp(std::move(qp), std::move(transport), qualified, status);
}

bool NativeEndpointEngine::start(Status *status) {
    if (m_state != EngineState::CONFIGURED) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "engine start requires CONFIGURED state");
    }
    m_state = EngineState::RUNNING;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::validate_local_key(uint32_t lkey, uint32_t pd,
                                              uint64_t address, uint64_t length,
                                              Status *status) const {
    const auto found = m_local_keys.find(registry_key(pd, lkey));
    if (found != m_local_keys.end()) return found->second->validate_local_key(lkey, pd, address, length, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "local key is not registered in QP protection domain");
}

bool NativeEndpointEngine::write_local_key(uint32_t lkey, uint32_t pd,
                                           uint64_t address, ByteSpan value, Status *status) {
    const auto found = m_local_keys.find(registry_key(pd, lkey));
    if (found != m_local_keys.end()) {
        return found->second->validate_local_key(lkey, pd, address, value.size, status) &&
               found->second->write(address, value, MemoryAccess::LOCAL_WRITE, status);
    }
    return status_error(status, ErrorCode::ACCESS_DENIED, "local write key is not registered in QP protection domain");
}

bool NativeEndpointEngine::invalidate_remote_key(uint32_t rkey, uint32_t pd,
                                                 bool apply, Status *status) {
    const auto found = m_remote_keys.find(registry_key(pd, rkey));
    if (found != m_remote_keys.end()) return found->second->invalidate_remote_key(rkey, apply, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "remote invalidation key is not registered in QP protection domain");
}

bool NativeEndpointEngine::validate_remote_access(uint32_t rkey, uint32_t pd, uint64_t address,
                                                   uint64_t length, MemoryAccess required,
                                                   Status *status) const {
    const auto found = m_remote_keys.find(registry_key(pd, rkey));
    if (found != m_remote_keys.end()) return found->second->validate_remote_access(rkey, address, length, required, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "remote key is not registered in QP protection domain");
}

bool NativeEndpointEngine::view_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                                           uint64_t length, MemoryAccess required,
                                           ByteSpan &out, Status *status) const {
    const auto found = m_remote_keys.find(registry_key(pd, rkey));
    if (found != m_remote_keys.end()) return found->second->view_remote_key(rkey, address, length, required, out, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "remote read key is not registered in QP protection domain");
}

bool NativeEndpointEngine::write_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                                            ByteSpan value, Status *status) {
    const auto found = m_remote_keys.find(registry_key(pd, rkey));
    if (found != m_remote_keys.end()) return found->second->write_remote_key(rkey, address, value, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "remote write key is not registered in QP protection domain");
}

bool NativeEndpointEngine::atomic_remote_key(uint32_t rkey, uint32_t pd, uint64_t address,
                                             bool fetch_add, uint64_t compare, uint64_t swap_add,
                                             uint64_t &original, Status *status) {
    const auto found = m_remote_keys.find(registry_key(pd, rkey));
    if (found != m_remote_keys.end()) return found->second->atomic(address, rkey, fetch_add, compare, swap_add, original, status);
    return status_error(status, ErrorCode::ACCESS_DENIED, "remote atomic key is not registered in QP protection domain");
}

bool NativeEndpointEngine::activate_qp(const QpId &qp, Status *status) {
    if (m_state != EngineState::CONFIGURED && m_state != EngineState::RUNNING) {
        return status_error(status, ErrorCode::INVALID_STATE, "QP activation requires configured or running engine");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "activation QP was not found");
    if (!slot->qp->modify_to_rts(status)) return false;
    ++m_publication_epoch;
    return true;
}

bool NativeEndpointEngine::drain_qp(const QpId &qp, Status *status) {
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "drain QP was not found");
    if (!slot->qp->modify_to_sqd(status)) return false;
    ++m_publication_epoch;
    return true;
}
bool NativeEndpointEngine::resume_qp(const QpId &qp, Status *status) {
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "resume QP was not found");
    if (!slot->qp->resume_from_sqd(status)) return false;
    ++m_publication_epoch;
    return true;
}

bool NativeEndpointEngine::close_qp(const QpId &qp, ErrorCode error, const std::string &reason, Status *status) {
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "close QP was not found");
    const size_t slot_index = static_cast<size_t>(slot - m_qps.begin());
    slot->transport->abort(error == ErrorCode::OK ? ErrorCode::ABORTED : error,
                            reason.empty() ? "QP closed" : reason);
    /* The engine is the sole owner of published TX and admitted RX handles.
     * Retire only this QP's queued frames before exposing another peek_tx;
     * unrelated QPs preserve FIFO order and their accepted-prefix accounting. */
    const size_t tx_count = m_tx.size();
    for (size_t i = 0; i < tx_count; ++i) {
        EngineWireFrame frame = std::move(m_tx.front());
        m_tx.pop();
        if (frame.source_qp_index != qp.qp_index()) {
            if (!m_tx.push(std::move(frame), status)) return false;
        }
    }
    m_qp_tx_queued[slot_index] = 0;
    const size_t ingress_count = m_ingress.size();
    DecodeOptions options;
    options.verify_icrc = false;
    options.verify_ipv4_checksum = false;
    options.verify_udp_checksum = false;
    for (size_t i = 0; i < ingress_count; ++i) {
        EngineWireFrame frame = std::move(m_ingress.front());
        m_ingress.pop();
        DecodedPacket packet;
        const bool closed = NativePacketCodec::decode(ByteSpan(frame.bytes.data(), frame.bytes.size()),
                                                      options, packet) && find_wire_qpn(packet) == slot;
        if (!closed && !m_ingress.push(std::move(frame), status)) return false;
    }
    m_ingress_request_barrier[slot_index] = 0;
    m_ingress_response_barrier[slot_index] = 0;
    m_ingress_terminal_nak_barrier[slot_index] = 0;
    ++m_publication_epoch;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::require_running(const char *operation,
                                           Status *status) const {
    if (m_state == EngineState::RUNNING) {
        return true;
    }
    std::ostringstream message;
    message << operation << " requires RUNNING state, current state is "
            << engine_state_to_string(m_state);
    return status_error(status, ErrorCode::INVALID_STATE, message.str());
}

bool NativeEndpointEngine::validate_now(uint64_t now_ns, Status *status) {
    if (m_has_polled && now_ns < m_last_poll_ns) {
        m_counters.monotonic_time_errors++;
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "engine clock moved backwards");
    }
    if (!m_has_polled || now_ns > m_last_poll_ns) {
        m_last_poll_ns = now_ns;
        m_has_polled = true;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::post_write(const QpId &qp,
                                      const EngineWriteRequest &request,
                                      uint64_t now_ns,
                                      PostedWork &posted,
                                      Status *status) {
    if (!require_running("post_write", status) || !validate_now(now_ns, status)) {
        return false;
    }
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "post_write QP belongs to a stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) {
        return status_error(status, ErrorCode::NOT_FOUND, "post_write QP was not found");
    }
    NativeMemoryBuffer *source = find_memory(request.source_memory_id);
    if (source == NULL) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "post_write source memory was not found");
    }
    if (source->window().protection_domain != slot->qp->runtime_view().config.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "write source belongs to another protection domain");
    }
    ByteSpan payload;
    if (!source->view(request.source_address,
                      request.length,
                      MemoryAccess::LOCAL_READ,
                      payload,
                      status)) {
        return false;
    }
    if (!slot->transport->post_write(request, payload, now_ns, posted, status)) {
        return false;
    }
    m_counters.post_write_calls++;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::post_read(const QpId &qp,
                                     const EngineReadRequest &request,
                                     uint64_t now_ns,
                                     PostedWork &posted,
                                     Status *status) {
    if (!require_running("post_read", status) || !validate_now(now_ns, status)) {
        return false;
    }
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "post_read QP belongs to a stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) {
        return status_error(status, ErrorCode::NOT_FOUND, "post_read QP was not found");
    }
    NativeMemoryBuffer *destination = find_memory(request.destination_memory_id);
    if (destination == NULL) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "post_read destination memory was not found");
    }
    if (destination->window().protection_domain != slot->qp->runtime_view().config.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "read destination belongs to another protection domain");
    }
    uint64_t offset = 0;
    if (!destination->offset_of(request.destination_address,
                                request.length,
                                MemoryAccess::LOCAL_WRITE,
                                offset,
                                status)) {
        return false;
    }
    if (!slot->transport->post_read(request, now_ns, posted, status)) {
        return false;
    }
    m_counters.post_read_calls++;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::post_receive(const QpId &qp, const EngineReceiveRequest &request, Status *status) {
    if (m_state != EngineState::CONFIGURED && m_state != EngineState::RUNNING) {
        return status_error(status, ErrorCode::INVALID_STATE, "receive posting requires configured or running engine");
    }
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status, ErrorCode::STALE_GENERATION, "receive QP belongs to stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "receive QP was not found");
    if (!slot->transport->post_receive(request, status)) return false;
    ++m_publication_epoch;
    return true;
}

bool NativeEndpointEngine::post_send(const QpId &qp, const EngineSendRequest &request,
                                     uint64_t now_ns, PostedWork &posted, Status *status) {
    if (!require_running("post_send", status) || !validate_now(now_ns, status)) return false;
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status, ErrorCode::STALE_GENERATION, "SEND QP belongs to stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "SEND QP was not found");
    NativeMemoryBuffer *source = find_memory(request.source_memory_id);
    ByteSpan payload;
    if (!source) return status_error(status, ErrorCode::NOT_FOUND, "SEND source memory was not found");
    if (source->window().protection_domain != slot->qp->runtime_view().config.protection_domain) {
        return status_error(status, ErrorCode::ACCESS_DENIED, "SEND source belongs to another protection domain");
    }
    if (!source->view(request.source_address, request.length, MemoryAccess::LOCAL_READ, payload, status)) return false;
    if (!slot->transport->post_send(request, payload, now_ns, posted, status)) return false;
    ++m_publication_epoch;
    return true;
}

bool NativeEndpointEngine::post_atomic(const QpId &qp, const EngineAtomicRequest &request,
                                       uint64_t now_ns, PostedWork &posted, Status *status) {
    if (!require_running("post_atomic", status) || !validate_now(now_ns, status)) return false;
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status, ErrorCode::STALE_GENERATION, "atomic QP belongs to stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) return status_error(status, ErrorCode::NOT_FOUND, "atomic QP was not found");
    if (!slot->transport->post_atomic(request, now_ns, posted, status)) return false;
    ++m_publication_epoch;
    return true;
}

bool NativeEndpointEngine::drain_completions(
    const QpId &qp,
    std::vector<CompletionRecord> &output,
    Status *status) {
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "completion QP belongs to a stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) {
        return status_error(status, ErrorCode::NOT_FOUND, "completion QP was not found");
    }
    const size_t before = output.size();
    if (!slot->transport->drain_completions(output, status)) {
        return false;
    }
    m_counters.work_completions += static_cast<uint64_t>(output.size() - before);
    if (output.size() != before) {
        m_publication_epoch++;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::drain_completions_bounded(
    const QpId &qp,
    std::vector<CompletionRecord> &output,
    size_t max_records,
    Status *status) {
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "completion QP belongs to a stale generation");
    }
    QpIterator slot = find_qp(qp);
    if (slot == m_qps.end()) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "completion QP was not found");
    }
    const size_t before = output.size();
    if (!slot->transport->drain_completions_bounded(output,
                                                     max_records,
                                                     status)) {
        return false;
    }
    m_counters.work_completions += static_cast<uint64_t>(output.size() - before);
    if (output.size() != before) {
        m_publication_epoch++;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::enqueue_ingress(EngineWireFrame frame,
                                           Status *status) {
    if (!require_running("enqueue_ingress", status)) {
        return false;
    }
    if (frame.generation != m_config.generation) {
        m_counters.ingress_stale_generation++;
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "ingress frame belongs to a stale generation");
    }
    if (frame.destination_endpoint != ENGINE_ANY_ENDPOINT &&
        frame.destination_endpoint != m_config.endpoint_id) {
        m_counters.ingress_wrong_endpoint++;
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "ingress frame targets another endpoint");
    }
    if (frame.bytes.empty() || frame.bytes.size() > m_config.max_frame_bytes) {
        m_counters.ingress_malformed++;
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "ingress frame length is outside configured bounds");
    }
    if (m_ingress.size() >= m_config.max_ingress_frames) {
        m_counters.ingress_backpressure++;
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine ingress queue is full");
    }
    if (m_next_ingress_token == 0 ||
        m_next_ingress_token == std::numeric_limits<uint64_t>::max()) {
        m_counters.ingress_backpressure++;
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine ingress identity space is exhausted");
    }
    frame.ingress_token = m_next_ingress_token++;
    if (!m_ingress.push(std::move(frame), status)) {
        m_counters.ingress_backpressure++;
        return false;
    }
    m_counters.ingress_enqueued++;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::enqueue_ingress_copy(
    const EngineWireFrame &metadata,
    const uint8_t *frame,
    size_t length,
    Status *status) {
    EngineWireFrame owned = metadata;
    if (!m_ingress_frames.copy(frame, length, owned.bytes, status)) {
        m_counters.ingress_backpressure++;
        return false;
    }
    return enqueue_ingress(std::move(owned), status);
}

bool NativeEndpointEngine::route_one_ingress(DecodedPacket &packet,
                                             QpSlot *&slot,
                                             bool &consumed,
                                             Status *status) {
    slot = NULL;
    consumed = false;
    if (m_ingress.empty()) {
        status_ok(status);
        return true;
    }

    const EngineWireFrame &frame = m_ingress.front();
    CodecError codec_error;
    DecodeOptions route_options;
    route_options.verify_icrc = false;
    route_options.verify_ipv4_checksum = false;
    route_options.verify_udp_checksum = false;
    if (!NativePacketCodec::decode(ByteSpan(frame.bytes.data(), frame.bytes.size()),
                                   route_options,
                                   packet,
                                   &codec_error)) {
        m_ingress.pop();
        m_counters.ingress_processed++;
        m_counters.ingress_malformed++;
        m_publication_epoch++;
        consumed = true;
        status_ok(status);
        return true;
    }
    QpIterator routed = find_wire_qpn(packet);
    if (routed == m_qps.end()) {
        m_ingress.pop();
        m_counters.ingress_processed++;
        m_counters.ingress_unknown_qp++;
        m_publication_epoch++;
        consumed = true;
        status_ok(status);
        return true;
    }
    if (routed->qp->state() == QpState::ERR) {
        /* A terminal QP drops late wire traffic.  In particular, a delayed
         * positive ACK must not resurrect work after a fatal NAK, and should
         * not escalate the otherwise inspectable QP failure to engine ERROR. */
        const size_t slot_index =
            static_cast<size_t>(&(*routed) - &m_qps.front());
        m_ingress_request_barrier[slot_index] = 0;
        m_ingress_response_barrier[slot_index] = 0;
        m_ingress_terminal_nak_barrier[slot_index] = 0;
        m_ingress.pop();
        m_counters.ingress_processed++;
        m_publication_epoch++;
        consumed = true;
        status_ok(status);
        return true;
    }
    slot = &(*routed);
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::process_one_ingress(QpSlot &slot,
                                               const DecodedPacket &packet,
                                               uint64_t now_ns,
                                               Status *status) {
    if (m_ingress.empty()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "routed engine ingress frame disappeared");
    }
    const EngineWireFrame &frame = m_ingress.front();
    m_counters.ingress_processed++;
    m_publication_epoch++;
    if (!slot.transport->on_frame(frame, packet, now_ns, status)) {
        const ErrorCode error = status == NULL ? ErrorCode::PROTOCOL_ERROR : status->code();
        const std::string reason = status == NULL || status->message().empty()
                                       ? "transport rejected ingress frame"
                                       : status->message();
        m_ingress.pop();
        enter_error(error == ErrorCode::OK ? ErrorCode::PROTOCOL_ERROR : error, reason);
        return false;
    }
    m_ingress.pop();
    status_ok(status);
    return true;
}

EngineIngressAdmission NativeEndpointEngine::ingress_admission(
    const QpSlot &slot,
    const DecodedPacket &packet) const {
    /* CNP and positive ACK packets only update requester state and/or publish
     * CQ records.  They must remain consumable while congestion-paced responder
     * frames are waiting, otherwise ACK/CNP control traffic can sit behind its
     * own data backlog for seconds.  Completion capacity is reserved when each
     * WQE is posted, so these transitions cannot overflow the adapter CQ after
     * mutating the QP. */
    if (packet.opcode == IB_CNP ||
        (packet.opcode == IB_ACKNOWLEDGE && packet.has_aeth &&
         packet.aeth.syndrome <= 0x1fU)) {
        return EngineIngressAdmission(true, false);
    }

    const NativeTransportRuntimeView transport =
        slot.transport->runtime_view();
    if (transport.paced_tx_frames > transport.queued_tx_frames) {
        return EngineIngressAdmission(false, true);
    }
    const uint32_t adapter_frames =
        transport.queued_tx_frames - transport.paced_tx_frames;
    const bool nak = packet.opcode == IB_ACKNOWLEDGE && packet.has_aeth &&
                     packet.aeth.syndrome > 0x1fU;
    const bool sequence_nak = nak && packet.aeth.syndrome == 0x60U;

    /* A read request or sequence NAK can atomically publish a full response or
     * replay batch through the congestion-paced FIFO.  RNR and fatal NAKs do
     * not publish frames, but they pause or terminate requester TX and must be
     * ordered after every frame already retained by this QP and by the engine.
     * Other data packets emit direct controls and need only the adapter FIFO. */
    const bool uses_paced_batch =
        packet.opcode == IB_RDMA_READ_REQUEST || packet.opcode == IB_COMPARE_SWAP ||
        packet.opcode == IB_FETCH_ADD || nak;
    const bool ready = adapter_frames == 0 &&
        (!uses_paced_batch || transport.paced_tx_frames == 0);
    return EngineIngressAdmission(ready,
                                  !nak || sequence_nak,
                                  nak || is_rc_read_response_opcode(packet.opcode) || packet.opcode == IB_ATOMIC_ACKNOWLEDGE);
}

bool NativeEndpointEngine::defer_one_ingress(Status *status) {
    if (m_ingress.empty()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "cannot defer an empty engine ingress queue");
    }
    EngineWireFrame deferred = std::move(m_ingress.front());
    m_ingress.pop();
    if (!m_ingress.push(std::move(deferred), status)) {
        enter_error(ErrorCode::INTERNAL_ERROR,
                    "failed to rotate bounded engine ingress");
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::enqueue_transport_frames(
    QpSlot &slot,
    std::vector<NativeFrameBuffer> &frames,
    uint64_t now_ns,
    Status *status) {
    if (frames.empty()) {
        status_ok(status);
        return true;
    }
    const size_t slot_index =
        static_cast<size_t>(&slot - &m_qps.front());
    if (slot_index >= m_qps.size() ||
        slot_index >= m_qp_tx_queued.size()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "transport frame owner is outside the QP table");
    }
    if (frames.size() > m_config.max_tx_frames - m_tx.size()) {
        m_counters.tx_backpressure++;
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine TX queue has insufficient capacity");
    }
    for (size_t index = 0; index < frames.size(); index++) {
        if (frames[index].empty() || frames[index].size() > m_config.max_frame_bytes) {
            return status_error(status,
                                ErrorCode::PROTOCOL_ERROR,
                                "transport emitted a frame outside configured bounds");
        }
    }
    if (m_next_frame_sequence == 0 ||
        static_cast<uint64_t>(frames.size()) >
            std::numeric_limits<uint64_t>::max() - m_next_frame_sequence) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "engine frame sequence is exhausted");
    }

    for (size_t index = 0; index < frames.size(); index++) {
        EngineWireFrame frame;
        frame.generation = m_config.generation;
        frame.source_endpoint = m_config.endpoint_id;
        frame.destination_endpoint = slot.binding.peer_endpoint;
        frame.source_qp_index = slot.qp->id().qp_index();
        frame.egress_port = slot.binding.egress_port;
        frame.ingress_port = slot.binding.peer_ingress_port;
        frame.sequence = m_next_frame_sequence++;
        frame.ready_ns = now_ns;
        frame.bytes = std::move(frames[index]);
        if (!m_tx.push(std::move(frame), status)) {
            m_counters.tx_backpressure++;
            return false;
        }
        m_qp_tx_queued[slot_index]++;
    }
    m_counters.tx_enqueued += static_cast<uint64_t>(frames.size());
    m_publication_epoch++;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::poll_transport_once(QpSlot &slot,
                                               uint64_t now_ns,
                                               uint32_t &budget,
                                               EnginePollResult &result,
                                               bool &produced,
                                               Status *status) {
    produced = false;
    if (budget == 0 || m_tx.size() >= m_config.max_tx_frames) {
        status_ok(status);
        return true;
    }
    const size_t slot_index =
        static_cast<size_t>(&slot - &m_qps.front());
    if (slot_index >= m_qps.size() ||
        slot_index >= m_qp_tx_queued.size()) {
        return status_error(status,
                            ErrorCode::INTERNAL_ERROR,
                            "transport poll owner is outside the QP table");
    }
    /* Once the adapter FIFO is drained into the engine ring, the transport
     * must not advance timers, completion failure, coalesced controls, or a
     * new replay chunk until this QP's published frames are accepted.  It may
     * continue flushing its already-admitted adapter/pacing queue so a burst
     * can still fill the bounded engine ring. */
    if (m_qp_tx_queued[slot_index] != 0 &&
        slot.transport->runtime_view().queued_tx_frames == 0) {
        status_ok(status);
        return true;
    }

    m_poll_frames.clear();
    if (!slot.transport->poll(now_ns, 1, m_poll_frames, status)) {
        const ErrorCode error = status == NULL ? ErrorCode::INTERNAL_ERROR : status->code();
        const std::string reason = status == NULL || status->message().empty()
                                       ? "transport poll failed"
                                       : status->message();
        enter_error(error == ErrorCode::OK ? ErrorCode::INTERNAL_ERROR : error, reason);
        return false;
    }
    result.transports_polled++;
    if (m_poll_frames.size() > 1) {
        enter_error(ErrorCode::PROTOCOL_ERROR,
                    "transport exceeded the requested one-frame fair-share budget");
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "transport exceeded its TX budget");
    }
    if (m_poll_frames.empty()) {
        status_ok(status);
        return true;
    }
    if (!enqueue_transport_frames(slot, m_poll_frames, now_ns, status)) {
        enter_error(status == NULL ? ErrorCode::RESOURCE_EXHAUSTED : status->code(),
                    status == NULL ? "failed to enqueue transport frame" : status->message());
        return false;
    }
    budget--;
    result.frames_produced++;
    result.made_progress = true;
    produced = true;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::poll(uint64_t now_ns,
                                EnginePollResult &result,
                                Status *status) {
    result = EnginePollResult();
    if (!require_running("poll", status) || !validate_now(now_ns, status)) {
        return false;
    }
    m_counters.polls++;

    uint32_t budget = std::min<uint32_t>(
        m_config.tx_budget_per_poll,
        m_config.max_tx_frames - static_cast<uint32_t>(m_tx.size()));

    /* Route in FIFO order, but never mutate requester/responder state unless
     * the addressed adapter can retain the complete response batch.  Generated
     * ACK/NAK/read/replay frames are interleaved into the bounded engine TX
     * ring; when that ring is full the unconsumed ingress head remains owned by
     * the engine and resumes on a later poll. */
    const uint32_t ingress_count = std::min<uint32_t>(
        m_config.ingress_budget_per_poll,
        static_cast<uint32_t>(m_ingress.size()));
    for (uint32_t index = 0; index < ingress_count; index++) {
        DecodedPacket packet;
        QpSlot *slot = NULL;
        bool consumed = false;
        if (!route_one_ingress(packet, slot, consumed, status)) {
            return false;
        }
        if (consumed) {
            result.ingress_processed++;
            result.made_progress = true;
            continue;
        }
        if (slot == NULL) {
            enter_error(ErrorCode::INTERNAL_ERROR,
                        "routable engine ingress frame has no QP slot");
            return status_error(status,
                                ErrorCode::INTERNAL_ERROR,
                                "routable engine ingress frame has no QP slot");
        }

        const size_t slot_index =
            static_cast<size_t>(slot - &m_qps.front());
        const uint64_t ingress_token = m_ingress.front().ingress_token;
        const bool request_stream =
            is_rc_request_stream_opcode(packet.opcode);
        const bool response_stream =
            is_rc_response_stream_opcode(packet.opcode);
        const bool terminal_nak = is_fatal_aeth(packet);
        const bool request_barrier_owner =
            request_stream &&
            m_ingress_request_barrier[slot_index] == ingress_token;
        const bool response_barrier_owner =
            response_stream &&
            m_ingress_response_barrier[slot_index] == ingress_token;
        const bool terminal_barrier_owner =
            terminal_nak &&
            m_ingress_terminal_nak_barrier[slot_index] == ingress_token;
        const bool qp_order_blocked =
            (m_ingress_terminal_nak_barrier[slot_index] != 0 &&
             !terminal_barrier_owner) ||
            (request_stream &&
             m_ingress_request_barrier[slot_index] != 0 &&
             !request_barrier_owner) ||
            (response_stream &&
             m_ingress_response_barrier[slot_index] != 0 &&
             !response_barrier_owner);
        EngineIngressAdmission admission =
            qp_order_blocked
                ? EngineIngressAdmission(false, true)
                : ingress_admission(*slot, packet);
        bool produced = false;
        size_t idle_slots = 0;
        while (!admission.ready && !qp_order_blocked &&
               budget != 0 &&
               m_tx.size() < m_config.max_tx_frames &&
               idle_slots < m_qps.size()) {
            if (m_rr_cursor >= m_qps.size()) {
                m_rr_cursor = 0;
            }
            QpSlot &scheduled = m_qps[m_rr_cursor];
            m_rr_cursor = (m_rr_cursor + 1) % m_qps.size();
            if (!poll_transport_once(scheduled,
                                     now_ns,
                                     budget,
                                     result,
                                     produced,
                                     status)) {
                return false;
            }
            idle_slots = produced ? 0 : idle_slots + 1;
            admission = ingress_admission(*slot, packet);
        }
        if (!admission.ready ||
            (admission.requires_empty_engine_tx &&
             m_qp_tx_queued[slot_index] != 0) ||
            (admission.may_emit_frames &&
             (budget == 0 || m_tx.size() >= m_config.max_tx_frames))) {
            /* Inspect every frame that was present at poll entry exactly once.
             * Rotating a blocked data frame preserves FIFO order among other
             * blocked data while allowing independent QPs and no-wire ACK/CNP
             * control transitions to make bounded progress. */
            if (!qp_order_blocked) {
                if (request_stream &&
                    m_ingress_request_barrier[slot_index] == 0) {
                    m_ingress_request_barrier[slot_index] = ingress_token;
                }
                if (response_stream &&
                    m_ingress_response_barrier[slot_index] == 0) {
                    m_ingress_response_barrier[slot_index] = ingress_token;
                }
                if (terminal_nak &&
                    m_ingress_terminal_nak_barrier[slot_index] == 0) {
                    m_ingress_terminal_nak_barrier[slot_index] = ingress_token;
                }
            }
            if (!defer_one_ingress(status)) {
                return false;
            }
            continue;
        }
        if (!process_one_ingress(*slot, packet, now_ns, status)) {
            return false;
        }
        if (request_barrier_owner) {
            m_ingress_request_barrier[slot_index] = 0;
        }
        if (response_barrier_owner) {
            m_ingress_response_barrier[slot_index] = 0;
        }
        if (terminal_barrier_owner || terminal_nak) {
            m_ingress_request_barrier[slot_index] = 0;
            m_ingress_response_barrier[slot_index] = 0;
            m_ingress_terminal_nak_barrier[slot_index] = 0;
        }
        result.ingress_processed++;
        result.made_progress = true;
    }

    if (m_qps.empty() || m_tx.size() >= m_config.max_tx_frames) {
        status_ok(status);
        return true;
    }

    size_t idle_slots = 0;
    while (budget != 0 && idle_slots < m_qps.size()) {
        if (m_rr_cursor >= m_qps.size()) {
            m_rr_cursor = 0;
        }
        QpSlot &slot = m_qps[m_rr_cursor];
        m_rr_cursor = (m_rr_cursor + 1) % m_qps.size();

        bool produced = false;
        if (!poll_transport_once(slot,
                                 now_ns,
                                 budget,
                                 result,
                                 produced,
                                 status)) {
            return false;
        }
        if (!produced) {
            idle_slots++;
            continue;
        }
        idle_slots = 0;
    }
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::drain_tx(uint32_t frame_budget,
                                    std::vector<EngineWireFrame> &output,
                                    Status *status) {
    if (frame_budget == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "TX drain budget must be non-zero");
    }
    const size_t count = std::min<size_t>(frame_budget, m_tx.size());
    if (count > output.max_size() - output.size()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "TX result vector cannot hold the requested frames");
    }
    try {
        output.reserve(output.size() + count);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "TX result vector allocation failed");
    } catch (const std::length_error &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "TX result vector exceeds capacity");
    }
    for (size_t index = 0; index < count; index++) {
        QpIterator owner = find_qp(QpId(
            m_config.generation, m_tx.front().source_qp_index));
        const size_t owner_index = owner == m_qps.end()
                                       ? m_qps.size()
                                       : static_cast<size_t>(owner - m_qps.begin());
        if (owner_index >= m_qps.size() ||
            owner_index >= m_qp_tx_queued.size() ||
            m_qp_tx_queued[owner_index] == 0) {
            enter_error(ErrorCode::INTERNAL_ERROR,
                        "engine TX owner accounting is inconsistent");
            return status_error(status,
                                ErrorCode::INTERNAL_ERROR,
                                "engine TX owner accounting is inconsistent");
        }
        output.push_back(std::move(m_tx.front()));
        m_tx.pop();
        m_qp_tx_queued[owner_index]--;
    }
    m_counters.tx_drained += static_cast<uint64_t>(count);
    if (count != 0) {
        m_publication_epoch++;
    }
    status_ok(status);
    return true;
}

const EngineWireFrame *NativeEndpointEngine::peek_tx() const {
    return peek_tx_at(0);
}

const EngineWireFrame *NativeEndpointEngine::peek_tx_at(size_t index) const {
    return m_tx.at(index);
}

bool NativeEndpointEngine::accept_tx(uint64_t sequence,
                                     uint64_t accepted_ns,
                                     Status *status) {
    if (!require_running("accept_tx", status) ||
        !validate_now(accepted_ns, status)) {
        return false;
    }
    if (m_tx.empty() || sequence == 0 || m_tx.front().sequence != sequence) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "TX acceptance does not match the engine queue head");
    }
    DecodedPacket packet;
    DecodeOptions options;
    options.verify_icrc = false;
    options.verify_ipv4_checksum = false;
    options.verify_udp_checksum = false;
    CodecError codec_error;
    if (!NativePacketCodec::decode(
            ByteSpan(m_tx.front().bytes.data(), m_tx.front().bytes.size()),
            options,
            packet,
            &codec_error)) {
        return status_error(status,
                            ErrorCode::PROTOCOL_ERROR,
                            "accepted engine TX frame cannot be decoded");
    }
    QpIterator slot = find_qp(
        QpId(m_config.generation, m_tx.front().source_qp_index));
    if (slot == m_qps.end()) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "accepted engine TX frame has no owning QP");
    }
    const size_t slot_index = static_cast<size_t>(slot - m_qps.begin());
    if (slot_index >= m_qp_tx_queued.size() ||
        m_qp_tx_queued[slot_index] == 0) {
        enter_error(ErrorCode::INTERNAL_ERROR,
                    "accepted engine TX owner accounting is inconsistent");
        return status_error(
            status,
            ErrorCode::INTERNAL_ERROR,
            "accepted engine TX owner accounting is inconsistent");
    }
    if (is_rc_request_stream_opcode(packet.opcode) &&
        !slot->transport->on_frame_accepted(packet.psn, accepted_ns, status)) {
        return false;
    }
    m_tx.pop();
    m_qp_tx_queued[slot_index]--;
    m_counters.tx_drained++;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

NativeMemoryBuffer *NativeEndpointEngine::memory(uint32_t memory_id) {
    return find_memory(memory_id);
}

const NativeMemoryBuffer *
NativeEndpointEngine::memory(uint32_t memory_id) const {
    return find_memory(memory_id);
}

bool NativeEndpointEngine::resolve_memory(uint64_t address,
                                          uint64_t length,
                                          MemoryAccess required,
                                          uint32_t &memory_id,
                                          Status *status) const {
    Status candidate_status;
    for (size_t index = 0; index < m_memories.size(); ++index) {
        uint64_t ignored_offset = 0;
        if (m_memories[index]->offset_of(address,
                                         length,
                                         required,
                                         ignored_offset,
                                         &candidate_status)) {
            memory_id = m_memories[index]->memory_id();
            status_ok(status);
            return true;
        }
    }
    return status_error(status,
                        ErrorCode::OUT_OF_RANGE,
                        "address is outside the native rank memory windows");
}

QueuePair *NativeEndpointEngine::queue_pair(const QpId &qp) {
    QpIterator current = find_qp(qp);
    return current == m_qps.end() ? NULL : current->qp.get();
}

const QueuePair *NativeEndpointEngine::queue_pair(const QpId &qp) const {
    ConstQpIterator current = find_qp(qp);
    return current == m_qps.end() ? NULL : current->qp.get();
}

bool NativeEndpointEngine::qp_runtime_view(const QpId &qp,
                                           EngineQpRuntimeView &view,
                                           Status *status) const {
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "QP runtime view belongs to a stale generation");
    }
    ConstQpIterator current = find_qp(qp);
    if (current == m_qps.end()) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "QP runtime view was not found");
    }
    view.qp = current->qp->runtime_view();
    view.binding = current->binding;
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::transport_runtime_view(
    const QpId &qp,
    NativeTransportRuntimeView &view,
    Status *status) const {
    if (!qpid_generation_matches(qp, m_config.generation)) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "transport runtime view belongs to a stale generation");
    }
    ConstQpIterator current = find_qp(qp);
    if (current == m_qps.end()) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "transport runtime view was not found");
    }
    view = current->transport->runtime_view();
    status_ok(status);
    return true;
}

bool NativeEndpointEngine::has_pending_work() const {
    if (!m_ingress.empty() || !m_tx.empty()) {
        return true;
    }
    for (ConstQpIterator current = m_qps.begin(); current != m_qps.end(); ++current) {
        if (current->transport->has_pending_work()) {
            return true;
        }
    }
    return false;
}

EngineState NativeEndpointEngine::state() const {
    return m_state;
}

uint32_t NativeEndpointEngine::endpoint_id() const {
    return m_config.endpoint_id;
}

const SessionGeneration &NativeEndpointEngine::generation() const {
    return m_config.generation;
}

EngineSnapshot NativeEndpointEngine::snapshot() const {
    EngineSnapshot result;
    result.state = m_state;
    result.config = m_config;
    result.publication_epoch = m_publication_epoch;
    result.last_poll_ns = m_last_poll_ns;
    result.has_polled = m_has_polled;
    result.ingress_queued = static_cast<uint32_t>(m_ingress.size());
    result.tx_queued = static_cast<uint32_t>(m_tx.size());
    result.total_memory_bytes = m_total_memory_bytes;
    result.ingress_storage = m_ingress_frames.snapshot();
    result.ingress_queue_capacity = static_cast<uint32_t>(m_ingress.capacity());
    result.ingress_queue_high_watermark = m_ingress.high_watermark();
    result.tx_queue_capacity = static_cast<uint32_t>(m_tx.capacity());
    result.tx_queue_high_watermark = m_tx.high_watermark();
    result.counters = m_counters;
    result.error_code = m_error_code;
    result.error_reason = m_error_reason;
    result.memories.reserve(m_memories.size());
    for (size_t index = 0; index < m_memories.size(); index++) {
        result.memories.push_back(m_memories[index]->snapshot());
    }
    result.qps.reserve(m_qps.size());
    for (ConstQpIterator current = m_qps.begin(); current != m_qps.end(); ++current) {
        EngineQpSnapshot qp;
        qp.qp = current->qp->snapshot();
        qp.binding = current->binding;
        qp.transport = current->transport->snapshot();
        if (qp.transport.pending) {
            result.pending_transports++;
        }
        result.qps.push_back(qp);
    }
    return result;
}

void NativeEndpointEngine::enter_error(ErrorCode error,
                                       const std::string &reason) {
    if (m_state == EngineState::STOPPED) {
        return;
    }
    if (error == ErrorCode::OK) {
        error = ErrorCode::INTERNAL_ERROR;
    }
    m_error_code = error;
    m_error_reason = reason.empty() ? "native endpoint engine error" : reason;
    for (std::vector<QpSlot>::reverse_iterator current = m_qps.rbegin();
         current != m_qps.rend();
         ++current) {
        current->transport->abort(error, m_error_reason);
        if (current->qp->state() == QpState::RTS) {
            current->qp->enter_error(error, m_error_reason, NULL);
        }
    }
    m_state = EngineState::ERROR;
    m_publication_epoch++;
}

void NativeEndpointEngine::unwind_reverse(ErrorCode error,
                                          const std::string &reason) {
    /* Engine TX frames refer into per-QP transport slabs.  They must leave
     * before reverse QP teardown destroys those slab owners. */
    m_ingress.reset();
    m_tx.reset();
    std::vector<NativeFrameBuffer>().swap(m_poll_frames);
    m_ingress_frames.release_storage();
    for (std::vector<QpSlot>::reverse_iterator current = m_qps.rbegin();
         current != m_qps.rend();
         ++current) {
        if (current->transport.get()) {
            current->transport->abort(error, reason);
            current->transport->detach();
        }
        if (current->qp.get()) {
            current->qp->destroy(reason, NULL);
        }
    }
    std::vector<QpSlot>().swap(m_qps);
    decltype(m_qp_index)().swap(m_qp_index);
    decltype(m_wire_qps)().swap(m_wire_qps);
    decltype(m_legacy_wire_qps)().swap(m_legacy_wire_qps);
    decltype(m_wire_qpn_counts)().swap(m_wire_qpn_counts);
    for (std::vector<std::unique_ptr<NativeMemoryBuffer> >::reverse_iterator current =
             m_memories.rbegin();
         current != m_memories.rend();
         ++current) {
        (*current)->reset();
    }
    std::vector<std::unique_ptr<NativeMemoryBuffer> >().swap(m_memories);
    decltype(m_memory_index)().swap(m_memory_index);
    decltype(m_local_keys)().swap(m_local_keys);
    decltype(m_remote_keys)().swap(m_remote_keys);
    std::vector<uint64_t>().swap(m_ingress_request_barrier);
    std::vector<uint64_t>().swap(m_ingress_response_barrier);
    std::vector<uint64_t>().swap(m_ingress_terminal_nak_barrier);
    std::vector<uint32_t>().swap(m_qp_tx_queued);
    m_total_memory_bytes = 0;
    m_rr_cursor = 0;
}

bool NativeEndpointEngine::teardown(const SessionGeneration &generation,
                                    Status *status) {
    m_counters.teardown_calls++;
    if (!m_config.generation.valid() || generation != m_config.generation) {
        return status_error(status,
                            ErrorCode::STALE_GENERATION,
                            "teardown generation does not own this engine");
    }
    if (m_state == EngineState::STOPPED) {
        status_ok(status);
        return true;
    }
    unwind_reverse(ErrorCode::ABORTED, "session teardown");
    m_state = EngineState::STOPPED;
    m_publication_epoch++;
    status_ok(status);
    return true;
}

LinkFaultRule::LinkFaultRule() :
    frame_ordinal(0),
    source_endpoint(ENGINE_ANY_ENDPOINT),
    destination_endpoint(ENGINE_ANY_ENDPOINT),
    source_qp_index(INVALID_QP_INDEX),
    psn(0xffffffffU),
    opcode(-1),
    match_occurrence(0),
    every_n(0),
    probability_ppm(1000000),
    probability_seed(0),
    one_shot(true),
    kind(LinkFaultKind::DROP),
    delay_ns(0),
    duplicate_count(1),
    corrupt_offset(0),
    corrupt_xor(1) {
}

InMemoryLinkConfig::InMemoryLinkConfig() :
    max_endpoints(16),
    max_routes(1024),
    max_in_flight_frames(65536),
    max_fault_rules(1024),
    max_frames_per_step(1024),
    propagation_delay_ns(0),
    idle_step_ns(1000) {
}

bool InMemoryLinkConfig::validate(Status *status) const {
    if (max_endpoints < 2 || max_routes == 0 || max_in_flight_frames == 0 ||
        max_fault_rules == 0 || max_frames_per_step == 0 || idle_step_ns == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "in-memory link limits and idle step must be non-zero");
    }
    status_ok(status);
    return true;
}

InMemoryRoute::InMemoryRoute() :
    source_endpoint(ENGINE_ANY_ENDPOINT),
    egress_port(ENGINE_ANY_PORT),
    destination_endpoint(ENGINE_ANY_ENDPOINT),
    ingress_port(ENGINE_ANY_PORT) {
}

InMemoryLinkCounters::InMemoryLinkCounters() :
    pump_steps(0),
    frames_captured(0),
    frames_delivered(0),
    frames_dropped(0),
    frames_blackholed(0),
    frames_duplicated(0),
    frames_delayed(0),
    frames_corrupted(0),
    frames_reordered(0),
    frames_unroutable(0),
    endpoint_backpressure(0) {
}

InMemoryLinkSnapshot::InMemoryLinkSnapshot() :
    now_ns(0),
    endpoints(0),
    routes(0),
    in_flight_frames(0),
    held_reorder_frames(0) {
}

NativeInMemoryLink::PendingFrame::PendingFrame() :
    delivery_ns(0),
    order(0) {
}

NativeInMemoryLink::HeldFrame::HeldFrame() :
    source_endpoint(ENGINE_ANY_ENDPOINT),
    destination_endpoint(ENGINE_ANY_ENDPOINT) {
}

NativeInMemoryLink::NativeInMemoryLink() :
    m_configured(false),
    m_now_ns(0),
    m_frame_ordinal(0),
    m_delivery_order(0) {
}

bool NativeInMemoryLink::configure(const InMemoryLinkConfig &config,
                                   Status *status) {
    if (m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is already configured");
    }
    if (!config.validate(status)) {
        return false;
    }
    m_config = config;
    m_now_ns = 0;
    m_frame_ordinal = 0;
    m_delivery_order = 0;
    m_counters = InMemoryLinkCounters();
    m_configured = true;
    status_ok(status);
    return true;
}

NativeEndpointEngine *
NativeInMemoryLink::find_endpoint(uint32_t endpoint_id) const {
    for (size_t index = 0; index < m_endpoints.size(); index++) {
        if (m_endpoints[index]->endpoint_id() == endpoint_id) {
            return m_endpoints[index];
        }
    }
    return NULL;
}

bool NativeInMemoryLink::attach(NativeEndpointEngine *endpoint,
                                Status *status) {
    if (!m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is not configured");
    }
    if (endpoint == NULL) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "cannot attach a null endpoint");
    }
    if (endpoint->state() != EngineState::RUNNING) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "attached endpoint must be running");
    }
    if (find_endpoint(endpoint->endpoint_id()) != NULL) {
        return status_error(status,
                            ErrorCode::DUPLICATE,
                            "in-memory link endpoint ID already exists");
    }
    if (m_endpoints.size() >= m_config.max_endpoints) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory link endpoint limit is exhausted");
    }
    try {
        m_endpoints.push_back(endpoint);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory endpoint table allocation failed");
    }
    status_ok(status);
    return true;
}

const InMemoryRoute *
NativeInMemoryLink::find_route(uint32_t source_endpoint,
                               uint32_t egress_port) const {
    for (size_t index = 0; index < m_routes.size(); index++) {
        if (m_routes[index].source_endpoint == source_endpoint &&
            m_routes[index].egress_port == egress_port) {
            return &m_routes[index];
        }
    }
    return NULL;
}

bool NativeInMemoryLink::add_route(const InMemoryRoute &route,
                                   Status *status) {
    if (!m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is not configured");
    }
    if (route.source_endpoint == ENGINE_ANY_ENDPOINT ||
        route.destination_endpoint == ENGINE_ANY_ENDPOINT ||
        route.egress_port == ENGINE_ANY_PORT ||
        route.ingress_port == ENGINE_ANY_PORT) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "in-memory route is incomplete");
    }
    if (find_endpoint(route.source_endpoint) == NULL ||
        find_endpoint(route.destination_endpoint) == NULL) {
        return status_error(status,
                            ErrorCode::NOT_FOUND,
                            "in-memory route references an unattached endpoint");
    }
    if (find_route(route.source_endpoint, route.egress_port) != NULL) {
        return status_error(status,
                            ErrorCode::DUPLICATE,
                            "source endpoint and egress port already have a route");
    }
    if (m_routes.size() >= m_config.max_routes) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory link route limit is exhausted");
    }
    try {
        m_routes.push_back(route);
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory route table allocation failed");
    }
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::add_fault(const LinkFaultRule &fault,
                                   Status *status) {
    if (!m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is not configured");
    }
    if (m_faults.size() >= m_config.max_fault_rules) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory link fault-rule limit is exhausted");
    }
    if (fault.probability_ppm > 1000000) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "fault probability exceeds one million ppm");
    }
    if (fault.opcode > 255 || fault.opcode < -1) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "fault opcode filter is invalid");
    }
    if (fault.psn != 0xffffffffU && !psn24_valid(fault.psn)) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "fault PSN filter exceeds 24 bits");
    }
    if (fault.kind == LinkFaultKind::DUPLICATE && fault.duplicate_count == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "duplicate fault requires at least one extra copy");
    }
    if (fault.kind == LinkFaultKind::CORRUPT && fault.corrupt_xor == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "corrupt fault requires a non-zero XOR mask");
    }
    try {
        m_faults.push_back(fault);
        m_fault_consumed.push_back(false);
        m_fault_match_counts.push_back(0);
    } catch (const std::bad_alloc &) {
        if (m_faults.size() > m_fault_consumed.size()) {
            m_faults.pop_back();
        }
        if (m_fault_consumed.size() > m_fault_match_counts.size()) {
            m_fault_consumed.pop_back();
        }
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory fault table allocation failed");
    }
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::fault_matches(size_t fault_index,
                                       const EngineWireFrame &frame,
                                       uint64_t ordinal) {
    if (fault_index >= m_faults.size()) {
        return false;
    }
    const LinkFaultRule &fault = m_faults[fault_index];
    if (m_fault_consumed[fault_index] ||
        (fault.frame_ordinal != 0 && fault.frame_ordinal != ordinal) ||
        !endpoint_filter_matches(fault.source_endpoint, frame.source_endpoint) ||
        !endpoint_filter_matches(fault.destination_endpoint, frame.destination_endpoint) ||
        (fault.source_qp_index != INVALID_QP_INDEX &&
         fault.source_qp_index != frame.source_qp_index)) {
        return false;
    }

    if (fault.psn != 0xffffffffU || fault.opcode >= 0) {
        DecodedPacket packet;
        CodecError error;
        DecodeOptions options;
        options.verify_icrc = false;
        options.verify_ipv4_checksum = false;
        options.verify_udp_checksum = false;
        if (!NativePacketCodec::decode(ByteSpan(frame.bytes.data(), frame.bytes.size()),
                                       options,
                                       packet,
                                       &error)) {
            return false;
        }
        if (fault.psn != 0xffffffffU && packet.psn != fault.psn) {
            return false;
        }
        if (fault.opcode >= 0 &&
            static_cast<int32_t>(packet.opcode) != fault.opcode) {
            return false;
        }
    }

    const uint64_t occurrence = ++m_fault_match_counts[fault_index];
    if (fault.match_occurrence != 0 && occurrence != fault.match_occurrence) {
        return false;
    }
    if (fault.every_n != 0 && occurrence % fault.every_n != 0) {
        return false;
    }
    if (fault.probability_ppm == 0) {
        return false;
    }
    if (fault.probability_ppm < 1000000) {
        uint64_t mixed = fault.probability_seed ^ ordinal ^ frame.sequence;
        mixed ^= mixed >> 30;
        mixed *= 0xbf58476d1ce4e5b9ULL;
        mixed ^= mixed >> 27;
        mixed *= 0x94d049bb133111ebULL;
        mixed ^= mixed >> 31;
        if (mixed % 1000000ULL >= fault.probability_ppm) {
            return false;
        }
    }
    return true;
}

bool NativeInMemoryLink::queue_pending(EngineWireFrame frame,
                                       uint64_t delivery_ns,
                                       Status *status) {
    if (m_pending.size() + m_held.size() >= m_config.max_in_flight_frames) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory link in-flight limit is exhausted");
    }
    if (m_delivery_order == std::numeric_limits<uint64_t>::max()) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory delivery order is exhausted");
    }
    PendingFrame pending;
    pending.delivery_ns = delivery_ns;
    pending.order = ++m_delivery_order;
    pending.frame = std::move(frame);
    pending.frame.ready_ns = delivery_ns;
    try {
        m_pending.push_back(std::move(pending));
    } catch (const std::bad_alloc &) {
        return status_error(status,
                            ErrorCode::RESOURCE_EXHAUSTED,
                            "in-memory delivery queue allocation failed");
    }
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::apply_faults_and_queue(EngineWireFrame frame,
                                                uint64_t now_ns,
                                                Status *status) {
    size_t selected = m_faults.size();
    for (size_t index = 0; index < m_faults.size(); index++) {
        if (fault_matches(index, frame, m_frame_ordinal)) {
            selected = index;
            break;
        }
    }
    if (selected == m_faults.size()) {
        if (add_u64_overflows(now_ns, m_config.propagation_delay_ns)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "in-memory propagation deadline overflows");
        }
        return queue_pending(std::move(frame),
                             now_ns + m_config.propagation_delay_ns,
                             status);
    }

    const LinkFaultRule fault = m_faults[selected];
    if (fault.one_shot && fault.kind != LinkFaultKind::BLACKHOLE) {
        m_fault_consumed[selected] = true;
    }
    switch (fault.kind) {
    case LinkFaultKind::DROP:
        m_counters.frames_dropped++;
        status_ok(status);
        return true;

    case LinkFaultKind::BLACKHOLE:
        /* A blackhole remains active even when one_shot was accidentally set. */
        m_counters.frames_dropped++;
        m_counters.frames_blackholed++;
        status_ok(status);
        return true;

    case LinkFaultKind::DELAY: {
        if (add_u64_overflows(now_ns, m_config.propagation_delay_ns) ||
            add_u64_overflows(now_ns + m_config.propagation_delay_ns, fault.delay_ns)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "fault delay deadline overflows");
        }
        m_counters.frames_delayed++;
        return queue_pending(std::move(frame),
                             now_ns + m_config.propagation_delay_ns + fault.delay_ns,
                             status);
    }

    case LinkFaultKind::CORRUPT: {
        if (fault.corrupt_offset >= frame.bytes.size()) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "fault corruption offset exceeds frame length");
        }
        /* Fault injection is a simulation-only copy-on-write boundary.  Live
         * requester/replay bytes remain immutable in their native slab. */
        NativeFrameBuffer corrupted;
        corrupted.assign(frame.bytes.data(),
                         frame.bytes.data() + frame.bytes.size());
        corrupted.mutable_data()[fault.corrupt_offset] ^= fault.corrupt_xor;
        frame.bytes = std::move(corrupted);
        m_counters.frames_corrupted++;
        if (add_u64_overflows(now_ns, m_config.propagation_delay_ns)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "corrupt-frame deadline overflows");
        }
        return queue_pending(std::move(frame),
                             now_ns + m_config.propagation_delay_ns,
                             status);
    }

    case LinkFaultKind::DUPLICATE: {
        if (m_pending.size() + m_held.size() +
                static_cast<size_t>(fault.duplicate_count) + 1 >
            m_config.max_in_flight_frames) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "duplicate fault exceeds the in-flight frame limit");
        }
        if (add_u64_overflows(now_ns, m_config.propagation_delay_ns)) {
            return status_error(status,
                                ErrorCode::OUT_OF_RANGE,
                                "duplicate-frame deadline overflows");
        }
        const uint64_t delivery = now_ns + m_config.propagation_delay_ns;
        for (uint32_t copy = 0; copy < fault.duplicate_count; copy++) {
            if (!queue_pending(frame, delivery, status)) {
                return false;
            }
        }
        m_counters.frames_duplicated += fault.duplicate_count;
        return queue_pending(std::move(frame), delivery, status);
    }

    case LinkFaultKind::REORDER_AFTER_NEXT: {
        if (m_pending.size() + m_held.size() >= m_config.max_in_flight_frames) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "reorder fault exceeds the in-flight frame limit");
        }
        HeldFrame held;
        held.source_endpoint = frame.source_endpoint;
        held.destination_endpoint = frame.destination_endpoint;
        held.frame = std::move(frame);
        try {
            m_held.push_back(std::move(held));
        } catch (const std::bad_alloc &) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "reorder hold allocation failed");
        }
        m_counters.frames_reordered++;
        status_ok(status);
        return true;
    }
    }
    return status_error(status, ErrorCode::INTERNAL_ERROR, "unknown link fault kind");
}

bool NativeInMemoryLink::capture_tx(NativeEndpointEngine &endpoint,
                                    uint64_t now_ns,
                                    uint32_t &budget,
                                    bool &made_progress,
                                    Status *status) {
    if (budget == 0) {
        status_ok(status);
        return true;
    }
    uint32_t captured = 0;
    while (captured < budget) {
        const EngineWireFrame *queued = endpoint.peek_tx();
        if (queued == NULL) {
            break;
        }
        EngineWireFrame frame = *queued;
        const InMemoryRoute *route = find_route(frame.source_endpoint, frame.egress_port);
        if (route == NULL) {
            m_counters.frames_unroutable++;
            return status_error(status,
                                ErrorCode::NOT_FOUND,
                                "no route for endpoint egress port");
        }
        if (frame.destination_endpoint != ENGINE_ANY_ENDPOINT &&
            frame.destination_endpoint != route->destination_endpoint) {
            m_counters.frames_unroutable++;
            return status_error(status,
                                ErrorCode::PROTOCOL_ERROR,
                                "transport peer endpoint disagrees with link route");
        }
        frame.destination_endpoint = route->destination_endpoint;
        frame.ingress_port = route->ingress_port;
        if (m_frame_ordinal == std::numeric_limits<uint64_t>::max()) {
            return status_error(status,
                                ErrorCode::RESOURCE_EXHAUSTED,
                                "in-memory frame ordinal is exhausted");
        }
        m_frame_ordinal++;
        m_counters.frames_captured++;
        if (!endpoint.accept_tx(frame.sequence, now_ns, status)) {
            return false;
        }
        captured++;

        /*
         * A held frame is released after the next frame in that direction is
         * queued, making the swap deterministic without relying on a random
         * scheduler or container iteration order.
         */
        size_t held_index = m_held.size();
        for (size_t current = 0; current < m_held.size(); current++) {
            if (m_held[current].source_endpoint == frame.source_endpoint &&
                m_held[current].destination_endpoint == frame.destination_endpoint) {
                held_index = current;
                break;
            }
        }
        const bool had_held = held_index < m_held.size();
        if (!apply_faults_and_queue(std::move(frame), now_ns, status)) {
            return false;
        }
        if (had_held) {
            EngineWireFrame held = std::move(m_held[held_index].frame);
            m_held.erase(m_held.begin() + held_index);
            if (add_u64_overflows(now_ns, m_config.propagation_delay_ns) ||
                !queue_pending(std::move(held),
                               now_ns + m_config.propagation_delay_ns,
                               status)) {
                return false;
            }
        }
    }
    budget -= captured;
    made_progress = made_progress || captured != 0;
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::deliver_due(uint64_t now_ns,
                                     uint32_t &budget,
                                     bool &made_progress,
                                     Status *status) {
    while (budget != 0) {
        size_t selected = m_pending.size();
        for (size_t index = 0; index < m_pending.size(); index++) {
            if (m_pending[index].delivery_ns > now_ns) {
                continue;
            }
            if (selected == m_pending.size() ||
                m_pending[index].delivery_ns < m_pending[selected].delivery_ns ||
                (m_pending[index].delivery_ns == m_pending[selected].delivery_ns &&
                 m_pending[index].order < m_pending[selected].order)) {
                selected = index;
            }
        }
        if (selected == m_pending.size()) {
            break;
        }
        NativeEndpointEngine *destination =
            find_endpoint(m_pending[selected].frame.destination_endpoint);
        if (destination == NULL) {
            m_counters.frames_unroutable++;
            return status_error(status,
                                ErrorCode::NOT_FOUND,
                                "pending frame destination is detached");
        }
        Status enqueue_status;
        if (!destination->enqueue_ingress(m_pending[selected].frame, &enqueue_status)) {
            if (enqueue_status.code() == ErrorCode::RESOURCE_EXHAUSTED) {
                m_counters.endpoint_backpressure++;
                break;
            }
            return status_error(status, enqueue_status.code(), enqueue_status.message());
        }
        m_pending.erase(m_pending.begin() + selected);
        m_counters.frames_delivered++;
        budget--;
        made_progress = true;
    }
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::step(uint64_t now_ns,
                              bool &made_progress,
                              Status *status) {
    made_progress = false;
    if (!m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is not configured");
    }
    if (now_ns < m_now_ns) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "in-memory link clock moved backwards");
    }
    m_now_ns = now_ns;
    m_counters.pump_steps++;
    uint32_t budget = m_config.max_frames_per_step;

    /* Deliver first so every endpoint's own poll remains ingress-first. */
    if (!deliver_due(now_ns, budget, made_progress, status)) {
        return false;
    }
    for (size_t index = 0; index < m_endpoints.size(); index++) {
        EnginePollResult result;
        if (!m_endpoints[index]->poll(now_ns, result, status)) {
            return false;
        }
        made_progress = made_progress || result.made_progress;
    }
    for (size_t index = 0; index < m_endpoints.size() && budget != 0; index++) {
        if (!capture_tx(*m_endpoints[index], now_ns, budget, made_progress, status)) {
            return false;
        }
    }
    /* Zero-delay frames become visible now and are processed next step. */
    if (!deliver_due(now_ns, budget, made_progress, status)) {
        return false;
    }
    status_ok(status);
    return true;
}

bool NativeInMemoryLink::all_quiescent() const {
    if (!m_pending.empty() || !m_held.empty()) {
        return false;
    }
    for (size_t index = 0; index < m_endpoints.size(); index++) {
        if (m_endpoints[index]->has_pending_work()) {
            return false;
        }
    }
    return true;
}

bool NativeInMemoryLink::pump_until_quiescent(uint64_t start_ns,
                                              uint64_t max_steps,
                                              uint64_t &finish_ns,
                                              Status *status) {
    if (!m_configured) {
        return status_error(status,
                            ErrorCode::INVALID_STATE,
                            "in-memory link is not configured");
    }
    if (max_steps == 0) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "pump step limit must be non-zero");
    }
    if (start_ns < m_now_ns) {
        return status_error(status,
                            ErrorCode::INVALID_ARGUMENT,
                            "pump start time moves backwards");
    }
    uint64_t now_ns = start_ns;
    for (uint64_t index = 0; index < max_steps; index++) {
        bool progress = false;
        if (!step(now_ns, progress, status)) {
            finish_ns = now_ns;
            return false;
        }
        if (all_quiescent()) {
            finish_ns = now_ns;
            status_ok(status);
            return true;
        }
        if (!progress) {
            if (add_u64_overflows(now_ns, m_config.idle_step_ns)) {
                finish_ns = now_ns;
                return status_error(status,
                                    ErrorCode::OUT_OF_RANGE,
                                    "pump clock overflowed");
            }
            now_ns += m_config.idle_step_ns;
        }
    }
    finish_ns = now_ns;
    return status_error(status,
                        ErrorCode::TIMEOUT,
                        "in-memory link did not quiesce within the step limit");
}

InMemoryLinkSnapshot NativeInMemoryLink::snapshot() const {
    InMemoryLinkSnapshot result;
    result.now_ns = m_now_ns;
    result.endpoints = static_cast<uint32_t>(m_endpoints.size());
    result.routes = static_cast<uint32_t>(m_routes.size());
    result.in_flight_frames = static_cast<uint32_t>(m_pending.size());
    result.held_reorder_frames = static_cast<uint32_t>(m_held.size());
    result.counters = m_counters;
    return result;
}

void NativeInMemoryLink::reset() {
    m_pending.clear();
    m_held.clear();
    m_faults.clear();
    m_fault_consumed.clear();
    m_fault_match_counts.clear();
    m_routes.clear();
    m_endpoints.clear();
    m_config = InMemoryLinkConfig();
    m_configured = false;
    m_now_ns = 0;
    m_frame_ordinal = 0;
    m_delivery_order = 0;
    m_counters = InMemoryLinkCounters();
}

} // namespace Native
} // namespace TrexRoceV2
