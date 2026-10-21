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

#include "stx/stl/trex_rocev2_native_engine.h"
#include "stx/stl/trex_rocev2_native_transport.h"

#include <common/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string.h>
#include <vector>

namespace rocev2_native_n2_tests {

using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;

static const uint64_t N2_GENERATION = 0x123456789ULL;
static const uint32_t N2_LEFT_ENDPOINT = 10;
static const uint32_t N2_RIGHT_ENDPOINT = 20;
static const uint32_t N2_LEFT_PORT = 100;
static const uint32_t N2_RIGHT_PORT = 200;
static const uint64_t N2_LEFT_VA = 0x0000000100000000ULL;
static const uint64_t N2_RIGHT_VA = 0x0000000200000000ULL;
static const uint32_t N2_LEFT_RKEY = 0x11112222U;
static const uint32_t N2_RIGHT_RKEY = 0x33334444U;
static const uint64_t N2_DEFAULT_MEMORY = 8ULL << 20;

static NativeEndpoint n2_endpoint(bool left, uint8_t ecn = 0) {
    NativeEndpoint endpoint;
    const uint8_t left_mac[6] = {0x02, 0, 0, 0, 0, 0x0a};
    const uint8_t right_mac[6] = {0x02, 0, 0, 0, 0, 0x14};
    const uint8_t left_ip[4] = {192, 0, 2, 10};
    const uint8_t right_ip[4] = {192, 0, 2, 20};
    memcpy(endpoint.src_mac, left ? left_mac : right_mac, sizeof(left_mac));
    memcpy(endpoint.dst_mac, left ? right_mac : left_mac, sizeof(left_mac));
    memcpy(endpoint.src_ip, left ? left_ip : right_ip, sizeof(left_ip));
    memcpy(endpoint.dst_ip, left ? right_ip : left_ip, sizeof(left_ip));
    endpoint.ipv6 = false;
    endpoint.has_vlan = false;
    endpoint.dscp = 26;
    endpoint.ecn = ecn;
    endpoint.ttl = 64;
    endpoint.udp_sport = left ? 49162 : 49172;
    endpoint.udp_dport = ROCEV2_UDP_PORT;
    return endpoint;
}

static MemoryAccess n2_memory_access() {
    return MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
           MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
}

static std::vector<uint8_t> n2_pattern(size_t length, uint32_t seed) {
    std::vector<uint8_t> bytes(length);
    uint32_t value = seed ? seed : 1;
    for (size_t index = 0; index < length; index++) {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        bytes[index] = static_cast<uint8_t>(value ^ index ^ (index >> 8));
    }
    return bytes;
}

static uint64_t n2_digest(const std::vector<uint8_t> &bytes) {
    uint64_t digest = 1469598103934665603ULL;
    for (size_t index = 0; index < bytes.size(); index++) {
        digest ^= bytes[index];
        digest *= 1099511628211ULL;
    }
    return digest;
}

static std::unique_ptr<QueuePair> n2_qp(uint32_t endpoint_id,
                                       uint32_t qp_index,
                                       uint32_t local_qpn,
                                       uint32_t remote_qpn,
                                       uint32_t local_psn,
                                       uint32_t remote_psn,
                                       uint16_t pmtu,
                                       uint64_t local_va,
                                       uint32_t local_rkey,
                                       uint64_t remote_va,
                                       uint32_t remote_rkey,
                                       uint64_t memory_bytes,
                                       uint8_t retry_count = 7,
                                       uint8_t rnr_retry_count = 7,
                                       uint8_t timeout_exp = 0,
                                       uint8_t min_rnr_timer = 1,
                                       uint32_t sq_depth_messages = 1024) {
    (void)endpoint_id;
    std::unique_ptr<QueuePair> qp(
        new QueuePair(QpId(SessionGeneration(N2_GENERATION), qp_index)));
    QpConfig local;
    local.local_qpn = local_qpn;
    local.starting_sq_psn = local_psn;
    local.pmtu = pmtu;
    local.sq_depth_messages = sq_depth_messages;
    local.sq_depth_packets = 32768;
    local.timeout_exp = timeout_exp;
    local.retry_count = retry_count;
    local.rnr_retry_count = rnr_retry_count;
    local.min_rnr_timer = min_rnr_timer;
    local.max_rd_atomic = 16;
    MemoryWindowConfig window;
    window.base_address = local_va;
    window.length = memory_bytes;
    window.rkey = local_rkey;
    window.access = n2_memory_access();
    EXPECT_TRUE(qp->create(local, window));
    QpPeerConfig peer;
    peer.remote_qpn = remote_qpn;
    peer.starting_psn = remote_psn;
    peer.pmtu = pmtu;
    peer.rkey = remote_rkey;
    peer.virtual_address = remote_va;
    peer.window_size = memory_bytes;
    peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
    peer.max_rd_atomic = 16;
    peer.ip = endpoint_id == N2_LEFT_ENDPOINT ? "192.0.2.20" : "192.0.2.10";
    peer.mac = endpoint_id == N2_LEFT_ENDPOINT ? "02:00:00:00:00:14" :
                                                 "02:00:00:00:00:0a";
    EXPECT_TRUE(qp->modify_to_rtr(peer));
    EXPECT_TRUE(qp->modify_to_rts());
    return qp;
}

static NativeTransportConfig n2_transport_config(bool left,
                                                 uint32_t max_tx_frames = 32768,
                                                 uint8_t ecn = 0) {
    NativeTransportConfig config;
    // Original draft recovery fixtures retain explicit synthetic response ACKs.
    config.legacy_response_acks = true;
    config.endpoint = n2_endpoint(left, ecn);
    config.signal_mode = NativeTransportSignalMode::IMMEDIATE;
    config.ack_request_interval = 0;
    config.duplicate_window = 4096;
    config.ack_coalesce_packets = 0;
    config.ack_coalesce_ns = 0;
    config.receive_credits = 1024;
    config.unlimited_receive_credits = true;
    config.cnp_enabled = true;
    config.cnp_min_interval_ns = 50000;
    config.cnp_reaction_factor = 0.5;
    config.cnp_recovery_ns = 1000000;
    config.max_tx_frames = max_tx_frames;
    config.max_read_cache_entries = 64;
    config.max_read_cache_bytes = 64ULL << 20;
    return config;
}

class N2Pair {
public:
    N2Pair() :
        memory_bytes(N2_DEFAULT_MEMORY),
        pmtu(4096),
        qps(1),
        left_start_psn(0x100),
        right_start_psn(0x200),
        retry_count(7),
        rnr_retry_count(7),
        timeout_exp(0),
        min_rnr_timer(1),
        left_ecn(0),
        right_ecn(0),
        sq_depth_messages(1024),
        max_completion_records(4096),
        max_tx_frames(32768),
        max_ingress_frames(32768),
        ingress_budget_per_poll(4096),
        idle_step_ns(4096),
        configured(false) {
    }

    void configure() {
        ASSERT_FALSE(configured);
        configure_engine(left, true);
        configure_engine(right, false);
        ASSERT_TRUE(left.start());
        ASSERT_TRUE(right.start());

        InMemoryLinkConfig link_config;
        link_config.max_endpoints = 2;
        link_config.max_routes = 2;
        link_config.max_in_flight_frames = 131072;
        link_config.max_fault_rules = 64;
        link_config.max_frames_per_step = 4096;
        link_config.propagation_delay_ns = 0;
        link_config.idle_step_ns = idle_step_ns;
        ASSERT_TRUE(link.configure(link_config));
        ASSERT_TRUE(link.attach(&left));
        ASSERT_TRUE(link.attach(&right));
        InMemoryRoute left_to_right;
        left_to_right.source_endpoint = N2_LEFT_ENDPOINT;
        left_to_right.egress_port = N2_LEFT_PORT;
        left_to_right.destination_endpoint = N2_RIGHT_ENDPOINT;
        left_to_right.ingress_port = N2_RIGHT_PORT;
        ASSERT_TRUE(link.add_route(left_to_right));
        InMemoryRoute right_to_left;
        right_to_left.source_endpoint = N2_RIGHT_ENDPOINT;
        right_to_left.egress_port = N2_RIGHT_PORT;
        right_to_left.destination_endpoint = N2_LEFT_ENDPOINT;
        right_to_left.ingress_port = N2_LEFT_PORT;
        ASSERT_TRUE(link.add_route(right_to_left));
        configured = true;
    }

    QpId left_qp(uint32_t index = 0) const {
        return QpId(SessionGeneration(N2_GENERATION), index);
    }

    QpId right_qp(uint32_t index = 0) const {
        return QpId(SessionGeneration(N2_GENERATION), index);
    }

    uint32_t left_qpn(uint32_t index) const { return 0x1000U + index; }
    uint32_t right_qpn(uint32_t index) const { return 0x2000U + index; }

    void write_left_memory(uint64_t offset, const std::vector<uint8_t> &value) {
        ASSERT_TRUE(left.memory(1)->write(N2_LEFT_VA + offset,
                                          ByteSpan(value.data(), value.size()),
                                          MemoryAccess::LOCAL_WRITE));
    }

    void write_right_memory(uint64_t offset, const std::vector<uint8_t> &value) {
        ASSERT_TRUE(right.memory(1)->write(N2_RIGHT_VA + offset,
                                           ByteSpan(value.data(), value.size()),
                                           MemoryAccess::LOCAL_WRITE));
    }

    std::vector<uint8_t> read_left_memory(uint64_t offset, uint64_t length) const {
        std::vector<uint8_t> value;
        EXPECT_TRUE(left.memory(1)->read(N2_LEFT_VA + offset,
                                         length,
                                         MemoryAccess::LOCAL_READ,
                                         value));
        return value;
    }

    std::vector<uint8_t> read_right_memory(uint64_t offset, uint64_t length) const {
        std::vector<uint8_t> value;
        EXPECT_TRUE(right.memory(1)->read(N2_RIGHT_VA + offset,
                                          length,
                                          MemoryAccess::LOCAL_READ,
                                          value));
        return value;
    }

    bool pump(uint64_t start_ns, uint64_t max_steps, uint64_t &finish_ns,
              Status *status = NULL) {
        return link.pump_until_quiescent(start_ns, max_steps, finish_ns, status);
    }

    std::vector<CompletionRecord> left_completions(uint32_t index = 0) {
        std::vector<CompletionRecord> result;
        EXPECT_TRUE(left.drain_completions(left_qp(index), result));
        return result;
    }

    std::vector<CompletionRecord> right_completions(uint32_t index = 0) {
        std::vector<CompletionRecord> result;
        EXPECT_TRUE(right.drain_completions(right_qp(index), result));
        return result;
    }

    uint64_t memory_bytes;
    uint16_t pmtu;
    uint32_t qps;
    uint32_t left_start_psn;
    uint32_t right_start_psn;
    uint8_t retry_count;
    uint8_t rnr_retry_count;
    uint8_t timeout_exp;
    uint8_t min_rnr_timer;
    uint8_t left_ecn;
    uint8_t right_ecn;
    uint32_t sq_depth_messages;
    uint32_t max_completion_records;
    uint32_t max_tx_frames;
    uint32_t max_ingress_frames;
    uint32_t ingress_budget_per_poll;
    uint64_t idle_step_ns;
    NativeEndpointEngine left;
    NativeEndpointEngine right;
    NativeInMemoryLink link;
    bool configured;

private:
    void configure_engine(NativeEndpointEngine &engine, bool is_left) {
        EngineConfig engine_config;
        engine_config.generation = SessionGeneration(N2_GENERATION);
        engine_config.endpoint_id = is_left ? N2_LEFT_ENDPOINT : N2_RIGHT_ENDPOINT;
        engine_config.max_qps = qps;
        engine_config.max_memory_regions = 1;
        engine_config.max_total_memory_bytes = memory_bytes;
        engine_config.max_completion_records = max_completion_records;
        engine_config.max_ingress_frames = max_ingress_frames;
        engine_config.max_tx_frames = max_tx_frames;
        engine_config.ingress_budget_per_poll = ingress_budget_per_poll;
        engine_config.tx_budget_per_poll = 4096;
        ASSERT_TRUE(engine.configure(engine_config));
        EngineMemoryConfig memory;
        memory.memory_id = 1;
        memory.window.base_address = is_left ? N2_LEFT_VA : N2_RIGHT_VA;
        memory.window.length = memory_bytes;
        memory.window.rkey = is_left ? N2_LEFT_RKEY : N2_RIGHT_RKEY;
        memory.window.access = n2_memory_access();
        ASSERT_TRUE(engine.add_memory(memory));
        for (uint32_t index = 0; index < qps; index++) {
            std::unique_ptr<QueuePair> qp = n2_qp(
                is_left ? N2_LEFT_ENDPOINT : N2_RIGHT_ENDPOINT,
                index,
                is_left ? left_qpn(index) : right_qpn(index),
                is_left ? right_qpn(index) : left_qpn(index),
                is_left ? left_start_psn + index * 0x100 : right_start_psn + index * 0x100,
                is_left ? right_start_psn + index * 0x100 : left_start_psn + index * 0x100,
                pmtu,
                is_left ? N2_LEFT_VA : N2_RIGHT_VA,
                is_left ? N2_LEFT_RKEY : N2_RIGHT_RKEY,
                is_left ? N2_RIGHT_VA : N2_LEFT_VA,
                is_left ? N2_RIGHT_RKEY : N2_LEFT_RKEY,
                memory_bytes,
                retry_count,
                rnr_retry_count,
                timeout_exp,
                min_rnr_timer,
                sq_depth_messages);
            EngineQpBinding binding;
            binding.memory_id = 1;
            binding.peer_endpoint = is_left ? N2_RIGHT_ENDPOINT : N2_LEFT_ENDPOINT;
            binding.egress_port = is_left ? N2_LEFT_PORT : N2_RIGHT_PORT;
            binding.peer_ingress_port = is_left ? N2_RIGHT_PORT : N2_LEFT_PORT;
            ASSERT_TRUE(engine.add_native_qp(
                std::move(qp),
                n2_transport_config(is_left,
                                    max_tx_frames,
                                    is_left ? left_ecn : right_ecn),
                binding));
        }
    }
};

static void n2_expect_success(const std::vector<CompletionRecord> &completions,
                              size_t count,
                              WorkKind kind) {
    ASSERT_EQ(count, completions.size());
    for (size_t index = 0; index < completions.size(); index++) {
        EXPECT_TRUE(kind == completions[index].kind);
        EXPECT_TRUE(CompletionState::SUCCESS == completions[index].state);
        EXPECT_TRUE(ErrorCode::OK == completions[index].error);
    }
}

static EngineWireFrame n2_control_ingress(bool left_to_right,
                                          IbOpcode opcode,
                                          uint32_t destination_qpn,
                                          uint32_t psn,
                                          uint64_t sequence,
                                          uint8_t syndrome = 0) {
    PacketEncodeSpec spec;
    spec.endpoint = n2_endpoint(left_to_right);
    spec.opcode = opcode;
    spec.dest_qpn = destination_qpn;
    spec.psn = psn;
    spec.has_reth = false;
    if (opcode == IB_ACKNOWLEDGE) {
        spec.has_aeth = true;
        spec.aeth.syndrome = syndrome;
        spec.aeth.msn = 1;
    } else if (opcode == IB_CNP) {
        spec.becn = true;
    }

    std::vector<uint8_t> encoded;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::encode(spec, encoded, &error))
        << error.message;
    EngineWireFrame frame;
    frame.generation = SessionGeneration(N2_GENERATION);
    frame.source_endpoint = left_to_right ? N2_LEFT_ENDPOINT : N2_RIGHT_ENDPOINT;
    frame.destination_endpoint = left_to_right ? N2_RIGHT_ENDPOINT : N2_LEFT_ENDPOINT;
    frame.source_qp_index = 0;
    frame.egress_port = left_to_right ? N2_LEFT_PORT : N2_RIGHT_PORT;
    frame.ingress_port = left_to_right ? N2_RIGHT_PORT : N2_LEFT_PORT;
    frame.sequence = sequence;
    frame.bytes.assign(encoded.data(), encoded.data() + encoded.size());
    return frame;
}

static EngineWireFrame n2_read_response_ingress(uint32_t destination_qpn,
                                                uint32_t psn,
                                                uint64_t sequence,
                                                const std::vector<uint8_t> &payload) {
    PacketEncodeSpec spec;
    spec.endpoint = n2_endpoint(false);
    spec.opcode = IB_RDMA_READ_RESPONSE_ONLY;
    spec.dest_qpn = destination_qpn;
    spec.psn = psn;
    spec.has_reth = false;
    spec.has_aeth = true;
    spec.aeth.syndrome = 0x1f;
    spec.aeth.msn = 1;
    spec.ack_request = true;
    spec.payload = ByteSpan(payload.empty() ? NULL : payload.data(),
                            payload.size());

    std::vector<uint8_t> encoded;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::encode(spec, encoded, &error))
        << error.message;
    EngineWireFrame frame;
    frame.generation = SessionGeneration(N2_GENERATION);
    frame.source_endpoint = N2_RIGHT_ENDPOINT;
    frame.destination_endpoint = N2_LEFT_ENDPOINT;
    frame.source_qp_index = 0;
    frame.egress_port = N2_RIGHT_PORT;
    frame.ingress_port = N2_LEFT_PORT;
    frame.sequence = sequence;
    frame.bytes.assign(encoded.data(), encoded.data() + encoded.size());
    return frame;
}

class N2DirectTransport {
public:
    N2DirectTransport() : qp(), transport(), pmtu(256) {}

    bool configure(uint32_t start_psn = 0,
                   uint8_t retry_count = 7,
                   uint8_t rnr_retry_count = 7,
                   uint8_t timeout_exp = 0,
                   uint32_t tx_limit = 32768,
                   bool unlimited_credits = true,
                   uint32_t ack_coalesce_packets = 0,
                   uint64_t ack_coalesce_ns = 0,
                   bool routed = false) {
        EngineMemoryConfig memory_config;
        memory_config.memory_id = 1;
        memory_config.window.base_address = N2_LEFT_VA;
        memory_config.window.length = 1ULL << 20;
        memory_config.window.rkey = N2_LEFT_RKEY;
        memory_config.window.access = n2_memory_access();
        if (!memory.configure(memory_config, 1ULL << 20)) {
            return false;
        }
        qp = n2_qp(N2_LEFT_ENDPOINT,
                   0,
                   0x1000,
                   0x2000,
                   start_psn,
                   0x800,
                   pmtu,
                   N2_LEFT_VA,
                   N2_LEFT_RKEY,
                   N2_RIGHT_VA,
                   N2_RIGHT_RKEY,
                   1ULL << 20,
                   retry_count,
                   rnr_retry_count,
                   timeout_exp,
                   1);
        transport.reset(new NativeRcTransport(*qp));
        NativeTransportConfig config = n2_transport_config(true, tx_limit);
        if (routed) {
            const uint8_t gateway[6] = {0x02, 0xaa, 0, 0, 0, 1};
            memcpy(config.endpoint.dst_mac, gateway, sizeof(gateway));
            config.endpoint.has_next_hop = true;
            config.endpoint.has_vlan = true;
            config.endpoint.vlan = 321;
            config.endpoint.pcp = 3;
            config.endpoint.vlan_dei = false;
        }
        config.unlimited_receive_credits = unlimited_credits;
        config.receive_credits = unlimited_credits ? 0 : 1;
        config.ack_coalesce_packets = ack_coalesce_packets;
        config.ack_coalesce_ns = ack_coalesce_ns;
        return transport->configure(config) && transport->bind_memory(&memory);
    }

    std::vector<uint8_t> encode_aeth(uint32_t psn, uint8_t syndrome) const {
        PacketEncodeSpec spec;
        spec.endpoint = n2_endpoint(false);
        spec.opcode = IB_ACKNOWLEDGE;
        spec.dest_qpn = 0x1000;
        spec.psn = psn;
        spec.has_reth = false;
        spec.has_aeth = true;
        spec.aeth.syndrome = syndrome;
        spec.aeth.msn = 1;
        std::vector<uint8_t> frame;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame));
        return frame;
    }

    std::vector<uint8_t> encode_read_request(uint32_t psn,
                                             uint64_t address,
                                             uint32_t rkey,
                                             uint32_t length) const {
        PacketEncodeSpec spec;
        spec.endpoint = n2_endpoint(false);
        spec.opcode = IB_RDMA_READ_REQUEST;
        spec.dest_qpn = 0x1000;
        spec.psn = psn;
        spec.has_reth = true;
        spec.reth.virtual_address = address;
        spec.reth.rkey = rkey;
        spec.reth.dma_length = length;
        std::vector<uint8_t> frame;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame));
        return frame;
    }

    std::vector<uint8_t> encode_read_response(
        uint32_t psn,
        const std::vector<uint8_t> &payload,
        bool ack_request = true) const {
        PacketEncodeSpec spec;
        spec.endpoint = n2_endpoint(false);
        spec.opcode = IB_RDMA_READ_RESPONSE_ONLY;
        spec.dest_qpn = 0x1000;
        spec.psn = psn;
        spec.has_reth = false;
        spec.has_aeth = true;
        spec.aeth.syndrome = 0x1f;
        spec.aeth.msn = 1;
        spec.ack_request = ack_request;
        spec.payload = ByteSpan(payload.empty() ? NULL : payload.data(),
                                payload.size());
        spec.ipv4_identification = static_cast<uint16_t>(psn);
        std::vector<uint8_t> frame;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame));
        return frame;
    }

    std::vector<uint8_t> encode_write_only(
        uint32_t psn,
        uint64_t address,
        uint32_t rkey,
        const std::vector<uint8_t> &payload,
        bool ack_request) const {
        PacketEncodeSpec spec;
        spec.endpoint = n2_endpoint(false);
        spec.opcode = IB_RDMA_WRITE_ONLY;
        spec.dest_qpn = 0x1000;
        spec.psn = psn;
        spec.has_reth = true;
        spec.reth.virtual_address = address;
        spec.reth.rkey = rkey;
        spec.reth.dma_length = static_cast<uint32_t>(payload.size());
        spec.ack_request = ack_request;
        spec.payload = ByteSpan(payload.empty() ? NULL : payload.data(),
                                payload.size());
        spec.ipv4_identification = static_cast<uint16_t>(psn);
        std::vector<uint8_t> frame;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame));
        return frame;
    }

    std::vector<uint8_t> encode_cnp() const {
        PacketEncodeSpec spec;
        spec.endpoint = n2_endpoint(false);
        spec.opcode = IB_CNP;
        spec.dest_qpn = 0x1000;
        spec.psn = 0;
        spec.has_reth = false;
        spec.becn = true;
        std::vector<uint8_t> frame;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame));
        return frame;
    }

    bool deliver(const std::vector<uint8_t> &frame,
                 uint64_t now_ns,
                 NativeTransportOutput &output,
                 Status *status = NULL) {
        DecodedPacket packet;
        CodecError error;
        if (!NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()),
                                       packet,
                                       &error)) {
            return status_error(status, ErrorCode::PROTOCOL_ERROR, error.message);
        }
        return transport->on_frame(packet,
                                   ByteSpan(frame.data(), frame.size()),
                                   now_ns,
                                   output,
                                   status);
    }

    NativeMemoryBuffer memory;
    std::unique_ptr<QueuePair> qp;
    std::unique_ptr<NativeRcTransport> transport;
    uint16_t pmtu;
};

TEST(RoceV2NativeN2,
     RoutedIngressAcceptsRouterMacAndRejectsWrongIdentityQpnAndIcrc) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0, 7, 7, 0, 32768, true, 0, 0, true));
    NativeEndpoint incoming = n2_endpoint(false);
    const uint8_t ingress_router[6] = {0x02, 0xbb, 0, 0, 0, 2};
    memcpy(incoming.src_mac, ingress_router, sizeof(ingress_router));
    incoming.has_vlan = true;
    incoming.vlan = 321;
    /* Routed matching owns VID, while PCP/DEI can be rewritten per hop. */
    incoming.pcp = 7;
    incoming.vlan_dei = true;

    const auto encode = [](const NativeEndpoint &endpoint,
                           uint32_t qpn) {
        PacketEncodeSpec spec;
        spec.endpoint = endpoint;
        spec.opcode = IB_ACKNOWLEDGE;
        spec.dest_qpn = qpn;
        spec.psn = 0;
        spec.has_reth = false;
        spec.has_aeth = true;
        spec.aeth.syndrome = 0x1f;
        spec.aeth.msn = 1;
        std::vector<uint8_t> frame;
        CodecError error;
        EXPECT_TRUE(NativePacketCodec::encode(spec, frame, &error))
            << error.message;
        return frame;
    };

    NativeTransportOutput output;
    Status status;
    const std::vector<uint8_t> valid = encode(incoming, 0x1000);
    ASSERT_TRUE(fixture.deliver(valid, 1000, output, &status))
        << status.message();
    EXPECT_EQ(0U,
              fixture.transport->snapshot().counters.endpoint_mismatch_packets);

    NativeEndpoint wrong_dst = incoming;
    wrong_dst.dst_mac[5] ^= 1U;
    ASSERT_TRUE(fixture.deliver(encode(wrong_dst, 0x1000),
                                1001, output, &status));
    NativeEndpoint wrong_ip = incoming;
    wrong_ip.dst_ip[3] ^= 1U;
    ASSERT_TRUE(fixture.deliver(encode(wrong_ip, 0x1000),
                                1002, output, &status));
    NativeEndpoint wrong_source_ip = incoming;
    wrong_source_ip.src_ip[3] ^= 1U;
    ASSERT_TRUE(fixture.deliver(encode(wrong_source_ip, 0x1000),
                                1003, output, &status));
    NativeEndpoint wrong_vlan = incoming;
    wrong_vlan.vlan++;
    ASSERT_TRUE(fixture.deliver(encode(wrong_vlan, 0x1000),
                                1004, output, &status));
    NativeEndpoint zero_source = incoming;
    memset(zero_source.src_mac, 0, sizeof(zero_source.src_mac));
    ASSERT_TRUE(fixture.deliver(encode(zero_source, 0x1000),
                                1005, output, &status));
    NativeEndpoint multicast_source = incoming;
    multicast_source.src_mac[0] |= 1U;
    ASSERT_TRUE(fixture.deliver(encode(multicast_source, 0x1000),
                                1006, output, &status));
    ASSERT_TRUE(fixture.deliver(encode(incoming, 0x1001),
                                1007, output, &status));

    std::vector<uint8_t> corrupt = valid;
    corrupt.back() ^= 1U;
    DecodeOptions relaxed;
    relaxed.verify_icrc = false;
    relaxed.verify_ipv4_checksum = false;
    relaxed.verify_udp_checksum = false;
    DecodedPacket decoded;
    CodecError codec_error;
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(corrupt.data(), corrupt.size()),
        relaxed,
        decoded,
        &codec_error));
    ASSERT_TRUE(fixture.transport->on_frame(
        decoded,
        ByteSpan(corrupt.data(), corrupt.size()),
        1008,
        output,
        &status));

    const NativeTransportCounters counters =
        fixture.transport->snapshot().counters;
    EXPECT_EQ(6U, counters.endpoint_mismatch_packets);
    EXPECT_EQ(1U, counters.unknown_qp_packets);
    EXPECT_EQ(1U, counters.icrc_errors);
}

TEST(RoceV2NativeN2, TransportRejectsReservedVlan4095) {
    NativeTransportConfig config = n2_transport_config(true);
    config.endpoint.has_vlan = true;
    config.endpoint.vlan = 4094;
    Status status;
    EXPECT_TRUE(config.validate(&status)) << status.message();
    config.endpoint.vlan = 4095;
    EXPECT_FALSE(config.validate(&status));
    EXPECT_TRUE(status.code() == ErrorCode::INVALID_ARGUMENT);
}

TEST(RoceV2NativeN2, EngineTeardownReleasesQueuesBeforeSmallerReconfigure) {
    NativeEndpointEngine engine;
    EngineConfig large;
    large.generation = SessionGeneration(N2_GENERATION);
    large.endpoint_id = N2_LEFT_ENDPOINT;
    large.max_qps = 64;
    large.max_memory_regions = 32;
    large.max_total_memory_bytes = 1ULL << 20;
    large.max_completion_records = 1024;
    large.max_ingress_frames = 128;
    large.max_tx_frames = 96;
    large.max_frame_bytes = 512;
    Status status;
    ASSERT_TRUE(engine.configure(large, &status)) << status.message();
    EngineSnapshot snapshot = engine.snapshot();
    EXPECT_EQ(128U, snapshot.ingress_storage.capacity);
    EXPECT_EQ(128U, snapshot.ingress_queue_capacity);
    EXPECT_EQ(96U, snapshot.tx_queue_capacity);

    ASSERT_TRUE(engine.teardown(large.generation, &status)) << status.message();
    snapshot = engine.snapshot();
    EXPECT_TRUE(snapshot.state == EngineState::STOPPED);
    EXPECT_EQ(0U, snapshot.ingress_storage.capacity);
    EXPECT_EQ(0U, snapshot.ingress_queue_capacity);
    EXPECT_EQ(0U, snapshot.tx_queue_capacity);

    EngineConfig small = large;
    small.generation = SessionGeneration(N2_GENERATION + 1U);
    small.max_qps = 1;
    small.max_memory_regions = 1;
    small.max_completion_records = 8;
    small.max_ingress_frames = 2;
    small.max_tx_frames = 3;
    ASSERT_TRUE(engine.configure(small, &status)) << status.message();
    snapshot = engine.snapshot();
    EXPECT_TRUE(snapshot.state == EngineState::CONFIGURED);
    EXPECT_EQ(2U, snapshot.ingress_storage.capacity);
    EXPECT_EQ(2U, snapshot.ingress_queue_capacity);
    EXPECT_EQ(3U, snapshot.tx_queue_capacity);
}

TEST(RoceV2NativeN2, WriteReadRoundTripAcrossTwoNativeEngines) {
    N2Pair pair;
    pair.configure();
    const uint64_t source_offset = 0x10000;
    const uint64_t remote_offset = 0x30000;
    const uint64_t readback_offset = 0x50000;
    const std::vector<uint8_t> expected = n2_pattern(3 * pair.pmtu + 17, 0x1234);
    pair.write_left_memory(source_offset, expected);

    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + source_offset;
    write.length = expected.size();
    write.remote_address = N2_RIGHT_VA + remote_offset;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.with_immediate = true;
    write.immediate_data = 0xfeedbeef;
    PostedWork posted_write;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted_write));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EXPECT_TRUE(expected == pair.read_right_memory(remote_offset, expected.size()));

    ASSERT_TRUE(pair.left.memory(1)->fill(N2_LEFT_VA + readback_offset,
                                          expected.size(),
                                          0,
                                          MemoryAccess::LOCAL_WRITE));
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + readback_offset;
    read.length = expected.size();
    read.remote_address = N2_RIGHT_VA + remote_offset;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted_read;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read,
                                    finish_ns + 1, posted_read));
    ASSERT_TRUE(pair.pump(finish_ns + 1, 200000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::READ);
    EXPECT_TRUE(expected == pair.read_left_memory(readback_offset, expected.size()));
}

TEST(RoceV2NativeN2, FourMiBWriteAndReadPreserveExactDigest) {
    N2Pair pair;
    pair.memory_bytes = 16ULL << 20;
    pair.configure();
    const size_t transfer_bytes = 4U << 20;
    const uint64_t source_offset = 0;
    const uint64_t remote_offset = 5ULL << 20;
    const uint64_t readback_offset = 8ULL << 20;
    const std::vector<uint8_t> expected = n2_pattern(transfer_bytes, 0xc001d00dU);
    const uint64_t expected_digest = n2_digest(expected);
    pair.write_left_memory(source_offset, expected);

    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + source_offset;
    write.length = transfer_bytes;
    write.remote_address = N2_RIGHT_VA + remote_offset;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    EXPECT_EQ(1024U, posted.reserved_psns);
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 500000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    const std::vector<uint8_t> remote =
        pair.read_right_memory(remote_offset, transfer_bytes);
    EXPECT_EQ(expected_digest, n2_digest(remote));
    EXPECT_TRUE(expected == remote);

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + readback_offset;
    read.length = transfer_bytes;
    read.remote_address = N2_RIGHT_VA + remote_offset;
    read.remote_rkey = N2_RIGHT_RKEY;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read,
                                    finish_ns + 1, posted));
    ASSERT_TRUE(pair.pump(finish_ns + 1, 500000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::READ);
    const std::vector<uint8_t> readback =
        pair.read_left_memory(readback_offset, transfer_bytes);
    EXPECT_EQ(expected_digest, n2_digest(readback));
    EXPECT_TRUE(expected == readback);
}

TEST(RoceV2NativeN2, SegmentationBoundariesChooseExactOpcodesAndAckReq) {
    struct Boundary {
        size_t length;
        size_t frames;
        IbOpcode first;
        IbOpcode last;
    };
    const Boundary cases[] = {
        {0, 1, IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE,
               IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE},
        {1, 1, IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE,
               IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE},
        {256, 1, IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE,
                 IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE},
        {257, 2, IB_RDMA_WRITE_FIRST, IB_RDMA_WRITE_LAST_WITH_IMMEDIATE},
        {512, 2, IB_RDMA_WRITE_FIRST, IB_RDMA_WRITE_LAST_WITH_IMMEDIATE},
        {513, 3, IB_RDMA_WRITE_FIRST, IB_RDMA_WRITE_LAST_WITH_IMMEDIATE}
    };
    for (size_t test = 0; test < sizeof(cases) / sizeof(cases[0]); test++) {
        N2DirectTransport fixture;
        ASSERT_TRUE(fixture.configure(0x100));
        const std::vector<uint8_t> payload = n2_pattern(cases[test].length, 7);
        NativeTransportWriteRequest request;
        request.remote_address = N2_RIGHT_VA;
        request.remote_rkey = N2_RIGHT_RKEY;
        request.with_immediate = true;
        request.immediate_data = 0x55aa55aa;
        PostedWork posted;
        NativeTransportOutput output;
        ASSERT_TRUE(fixture.transport->post_write(
            request,
            ByteSpan(payload.empty() ? NULL : payload.data(), payload.size()),
            0,
            posted,
            output));
        ASSERT_EQ(cases[test].frames, output.frames.size());
        EXPECT_EQ(cases[test].first, output.frames.front().opcode);
        EXPECT_EQ(cases[test].last, output.frames.back().opcode);
        EXPECT_EQ(cases[test].frames, posted.reserved_psns);
        for (size_t index = 0; index < output.frames.size(); index++) {
            DecodedPacket decoded;
            ASSERT_TRUE(NativePacketCodec::decode(
                ByteSpan(output.frames[index].bytes.data(),
                         output.frames[index].bytes.size()),
                decoded));
            EXPECT_EQ(index + 1 == output.frames.size(), decoded.ack_request);
        }
    }
}

TEST(RoceV2NativeN2, SegmentationWrapsPsnWithoutChangingWorkIdentity) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x00fffffeU));
    const std::vector<uint8_t> payload = n2_pattern(3 * fixture.pmtu, 99);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0,
                                               posted,
                                               output));
    ASSERT_EQ(3U, output.frames.size());
    EXPECT_EQ(0x00fffffeU, output.frames[0].psn);
    EXPECT_EQ(0x00ffffffU, output.frames[1].psn);
    EXPECT_EQ(0U, output.frames[2].psn);
    EXPECT_EQ(0x00fffffeU, posted.first_psn);
    EXPECT_EQ(0U, posted.terminal_psn);
    EXPECT_TRUE(posted.id == output.frames[0].work_id);
    EXPECT_TRUE(posted.id == output.frames[1].work_id);
    EXPECT_TRUE(posted.id == output.frames[2].work_id);
}

TEST(RoceV2NativeN2, OneCumulativeAckCompletesAllCoveredWrites) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x400));
    uint32_t terminal_psn = 0;
    std::vector<WorkId> work_ids;
    for (uint32_t index = 0; index < 3; index++) {
        const std::vector<uint8_t> payload(1, static_cast<uint8_t>(index));
        NativeTransportWriteRequest request;
        request.remote_address = N2_RIGHT_VA + index;
        request.remote_rkey = N2_RIGHT_RKEY;
        PostedWork posted;
        NativeTransportOutput output;
        ASSERT_TRUE(fixture.transport->post_write(request,
                                                   ByteSpan(payload.data(), payload.size()),
                                                   index,
                                                   posted,
                                                   output));
        terminal_psn = posted.terminal_psn;
        work_ids.push_back(posted.id);
    }
    NativeTransportOutput ack_output;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(terminal_psn, 0x1f),
                                100,
                                ack_output));
    ASSERT_EQ(3U, ack_output.completions.size());
    for (size_t index = 0; index < ack_output.completions.size(); index++) {
        EXPECT_TRUE(work_ids[index] == ack_output.completions[index].id);
        EXPECT_TRUE(CompletionState::SUCCESS == ack_output.completions[index].state);
    }
    const NativeTransportSnapshot snapshot = fixture.transport->snapshot();
    EXPECT_EQ(0U, snapshot.outstanding_work);
    EXPECT_EQ(0U, snapshot.outstanding_packets);
    EXPECT_EQ(1U, snapshot.counters.acks_received);
}

TEST(RoceV2NativeN2, ReadyCompletionDoesNotPreventLinkQuiescence) {
    N2Pair pair;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(64, 1);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 10000, finish_ns));
    EXPECT_FALSE(pair.left.has_pending_work());
    /* The engine adapter owns terminal records until the client drains them;
     * the lower QP ledger must therefore already be empty here. */
    EXPECT_EQ(0U, pair.left.snapshot().qps[0].transport.native.qp.ledger.ready_completions);
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
}

TEST(RoceV2NativeN2, OneDroppedPacketRecoversWithGoBackN) {
    N2Pair pair;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.opcode = IB_RDMA_WRITE_FIRST;
    fault.kind = LinkFaultKind::DROP;
    ASSERT_TRUE(pair.link.add_fault(fault));

    const std::vector<uint8_t> payload = n2_pattern(2 * pair.pmtu + 19, 0x11);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EXPECT_TRUE(payload == pair.read_right_memory(0, payload.size()));
    const EngineSnapshot left = pair.left.snapshot();
    const EngineSnapshot right = pair.right.snapshot();
    EXPECT_GT(left.qps[0].transport.native.counters.retransmit_packets, 0U);
    EXPECT_GT(right.qps[0].transport.native.counters.psn_gap_events, 0U);
    EXPECT_EQ(1U, pair.link.snapshot().counters.frames_dropped);
}

TEST(RoceV2NativeN2, DuplicatePacketIsNotDeliveredTwice) {
    N2Pair pair;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.opcode = IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE;
    fault.kind = LinkFaultKind::DUPLICATE;
    fault.duplicate_count = 2;
    ASSERT_TRUE(pair.link.add_fault(fault));
    const std::vector<uint8_t> payload = n2_pattern(128, 2);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.with_immediate = true;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 100000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EXPECT_TRUE(payload == pair.read_right_memory(0, payload.size()));
    const NativeTransportCounters counters =
        pair.right.snapshot().qps[0].transport.native.counters;
    EXPECT_GE(counters.duplicate_packets, 2U);
    EXPECT_EQ(1U, counters.deliveries);
    EXPECT_EQ(2U, pair.link.snapshot().counters.frames_duplicated);
}

TEST(RoceV2NativeN2, ReorderedFirstPacketTriggersNakThenRecovers) {
    N2Pair pair;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.opcode = IB_RDMA_WRITE_FIRST;
    fault.kind = LinkFaultKind::REORDER_AFTER_NEXT;
    ASSERT_TRUE(pair.link.add_fault(fault));
    const std::vector<uint8_t> payload = n2_pattern(pair.pmtu + 37, 3);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EXPECT_TRUE(payload == pair.read_right_memory(0, payload.size()));
    const NativeTransportCounters right =
        pair.right.snapshot().qps[0].transport.native.counters;
    EXPECT_GT(right.sequence_naks_sent, 0U);
    EXPECT_GT(right.psn_gap_events, 0U);
    EXPECT_EQ(1U, pair.link.snapshot().counters.frames_reordered);
}

TEST(RoceV2NativeN2, CorruptFrameFailsIcrcAndRecoversByTimeout) {
    N2Pair pair;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.opcode = IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE;
    fault.kind = LinkFaultKind::CORRUPT;
    fault.corrupt_offset = 50;
    fault.corrupt_xor = 0x80;
    ASSERT_TRUE(pair.link.add_fault(fault));
    const std::vector<uint8_t> payload = n2_pattern(100, 4);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.with_immediate = true;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EXPECT_TRUE(payload == pair.read_right_memory(0, payload.size()));
    EXPECT_EQ(1U, pair.link.snapshot().counters.frames_corrupted);
    EXPECT_GT(pair.left.snapshot().qps[0].transport.native.counters.timeout_retries, 0U);
    const EngineSnapshot right = pair.right.snapshot();
    EXPECT_EQ(0U, right.counters.ingress_malformed);
    EXPECT_EQ(1U, right.qps[0].transport.native.counters.icrc_errors);
    EXPECT_EQ(1U, right.qps[0].transport.native.counters.malformed_packets);
}

TEST(RoceV2NativeN2, PersistentBlackholeExhaustsRetryAndErrorsQp) {
    N2Pair pair;
    pair.retry_count = 1;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.kind = LinkFaultKind::BLACKHOLE;
    fault.one_shot = false;
    ASSERT_TRUE(pair.link.add_fault(fault));
    const std::vector<uint8_t> payload = n2_pattern(64, 5);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(CompletionState::ERROR == completions[0].state);
    EXPECT_TRUE(ErrorCode::RETRY_EXHAUSTED == completions[0].error);
    const EngineSnapshot snapshot = pair.left.snapshot();
    EXPECT_TRUE(QpState::ERR == snapshot.qps[0].qp.state);
    EXPECT_EQ(1U, snapshot.qps[0].transport.native.counters.retry_exhausted);
    EXPECT_GE(pair.link.snapshot().counters.frames_blackholed, 2U);
}

TEST(RoceV2NativeN2, RepeatedSequenceNakExhaustsBoundedRetryBudget) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x500, 1));
    const std::vector<uint8_t> payload = n2_pattern(2 * fixture.pmtu, 6);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput post_output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0,
                                               posted,
                                               post_output));
    std::vector<CompletionRecord> completions;
    for (uint32_t attempt = 0; attempt < 8 && fixture.qp->state() == QpState::RTS;
         attempt++) {
        NativeTransportOutput output;
        ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x60),
                                    attempt + 1,
                                    output));
        completions.insert(completions.end(),
                           output.completions.begin(),
                           output.completions.end());
    }
    EXPECT_TRUE(QpState::ERR == fixture.qp->state());
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(ErrorCode::RETRY_EXHAUSTED == completions[0].error);
    const NativeTransportCounters counters = fixture.transport->snapshot().counters;
    EXPECT_GT(counters.sequence_naks_received, 0U);
    EXPECT_GT(counters.nak_retries, 0U);
    EXPECT_EQ(1U, counters.retry_exhausted);
}

TEST(RoceV2NativeN2, RepeatedRnrNakExhaustsIndependentRnrBudget) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x600, 7, 1));
    const std::vector<uint8_t> payload = n2_pattern(8, 7);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    request.with_immediate = true;
    PostedWork posted;
    NativeTransportOutput post_output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0,
                                               posted,
                                               post_output));
    uint64_t now_ns = 100;
    std::vector<CompletionRecord> completions;
    for (uint32_t attempt = 0; attempt < 8 && fixture.qp->state() == QpState::RTS;
         attempt++) {
        NativeTransportOutput rnr_output;
        ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x21),
                                    now_ns,
                                    rnr_output));
        completions.insert(completions.end(),
                           rnr_output.completions.begin(),
                           rnr_output.completions.end());
        now_ns += 1000000000ULL;
        NativeTransportOutput poll_output;
        Status poll_status;
        ASSERT_TRUE(fixture.transport->poll(now_ns, 32768, poll_output,
                                             &poll_status))
            << error_code_to_string(poll_status.code()) << ": "
            << poll_status.message();
        completions.insert(completions.end(),
                           poll_output.completions.begin(),
                           poll_output.completions.end());
    }
    EXPECT_TRUE(QpState::ERR == fixture.qp->state());
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(ErrorCode::RNR_RETRY_EXHAUSTED == completions[0].error);
    const NativeTransportCounters counters = fixture.transport->snapshot().counters;
    EXPECT_GT(counters.rnr_naks_received, 0U);
    EXPECT_GT(counters.rnr_retries, 0U);
    EXPECT_EQ(1U, counters.rnr_retry_exhausted);
}

TEST(RoceV2NativeN2, FatalRemoteAccessNakFailsImmediatelyWithoutReplay) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x700));
    const std::vector<uint8_t> payload = n2_pattern(16, 8);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput post_output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0,
                                               posted,
                                               post_output));
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x62),
                                1,
                                output));
    EXPECT_TRUE(output.frames.empty());
    ASSERT_EQ(1U, output.completions.size());
    EXPECT_TRUE(ErrorCode::REMOTE_ACCESS_ERROR == output.completions[0].error);
    EXPECT_TRUE(QpState::ERR == fixture.qp->state());
    EXPECT_EQ(1U, fixture.transport->snapshot().counters.fatal_naks_received);
}

TEST(RoceV2NativeN2, PartialReadResponseLossUsesResponderCacheAndRecovers) {
    N2Pair pair;
    pair.configure();
    const size_t length = 4 * pair.pmtu + 11;
    const uint64_t remote_offset = 0x10000;
    const uint64_t local_offset = 0x40000;
    const std::vector<uint8_t> expected = n2_pattern(length, 9);
    pair.write_right_memory(remote_offset, expected);
    LinkFaultRule fault;
    fault.source_endpoint = N2_RIGHT_ENDPOINT;
    fault.destination_endpoint = N2_LEFT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.opcode = IB_RDMA_READ_RESPONSE_MIDDLE;
    fault.match_occurrence = 1;
    fault.kind = LinkFaultKind::DROP;
    ASSERT_TRUE(pair.link.add_fault(fault));
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + local_offset;
    read.length = length;
    read.remote_address = N2_RIGHT_VA + remote_offset;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 300000, finish_ns));
    n2_expect_success(pair.left_completions(), 1, WorkKind::READ);
    EXPECT_TRUE(expected == pair.read_left_memory(local_offset, length));
    const NativeTransportCounters requester =
        pair.left.snapshot().qps[0].transport.native.counters;
    const NativeTransportCounters responder =
        pair.right.snapshot().qps[0].transport.native.counters;
    EXPECT_GT(requester.sequence_naks_sent, 0U);
    EXPECT_GT(responder.read_cache_hits, 0U);
    EXPECT_GT(responder.retransmit_packets, 0U);
    EXPECT_EQ(1U, pair.link.snapshot().counters.frames_dropped);
}

TEST(RoceV2NativeN2, MultipleOutstandingReadsCompleteToDistinctDestinations) {
    N2Pair pair;
    pair.configure();
    const size_t first_length = 2 * pair.pmtu + 7;
    const size_t second_length = pair.pmtu + 19;
    const std::vector<uint8_t> first = n2_pattern(first_length, 10);
    const std::vector<uint8_t> second = n2_pattern(second_length, 11);
    pair.write_right_memory(0x10000, first);
    pair.write_right_memory(0x50000, second);
    EngineReadRequest read1;
    read1.destination_memory_id = 1;
    read1.destination_address = N2_LEFT_VA + 0x100000;
    read1.length = first_length;
    read1.remote_address = N2_RIGHT_VA + 0x10000;
    read1.remote_rkey = N2_RIGHT_RKEY;
    EngineReadRequest read2;
    read2.destination_memory_id = 1;
    read2.destination_address = N2_LEFT_VA + 0x200000;
    read2.length = second_length;
    read2.remote_address = N2_RIGHT_VA + 0x50000;
    read2.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted1;
    PostedWork posted2;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read1, 0, posted1));
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read2, 0, posted2));
    EXPECT_TRUE(posted1.id != posted2.id);
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 300000, finish_ns));
    n2_expect_success(pair.left_completions(), 2, WorkKind::READ);
    EXPECT_TRUE(first == pair.read_left_memory(0x100000, first_length));
    EXPECT_TRUE(second == pair.read_left_memory(0x200000, second_length));
}

TEST(RoceV2NativeN2, CnpGenerationIsPacedAndRequesterRecoversRate) {
    N2Pair pair;
    pair.left_ecn = 3;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(4 * pair.pmtu, 12);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    for (uint32_t step = 0; step < 32; step++) {
        bool progress = false;
        ASSERT_TRUE(pair.link.step(0, progress));
    }
    n2_expect_success(pair.left_completions(), 1, WorkKind::WRITE);
    EngineSnapshot left = pair.left.snapshot();
    const EngineSnapshot right = pair.right.snapshot();
    EXPECT_EQ(4U, right.qps[0].transport.native.counters.ecn_ce_packets);
    EXPECT_EQ(1U, right.qps[0].transport.native.counters.cnp_sent);
    EXPECT_EQ(1U, left.qps[0].transport.native.counters.cnp_received);
    EXPECT_DOUBLE_EQ(0.5, left.qps[0].transport.native.rate_scale);
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(1000001, poll));
    left = pair.left.snapshot();
    EXPECT_DOUBLE_EQ(1.0, left.qps[0].transport.native.rate_scale);
}

TEST(RoceV2NativeN2, BoundedAdapterRejectsOversizedPostWithoutLedgerMutation) {
    N2Pair pair;
    pair.max_tx_frames = 2;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(2 * pair.pmtu + 1, 13);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    Status status;
    EXPECT_FALSE(pair.left.post_write(pair.left_qp(), write, 0, posted, &status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == status.code());
    const QpSnapshot qp = pair.left.queue_pair(pair.left_qp())->snapshot();
    EXPECT_EQ(0U, qp.ledger.outstanding_work);
    EXPECT_EQ(0U, qp.ledger.outstanding_packets);
    EXPECT_EQ(pair.left_start_psn, qp.next_sq_psn);
}

TEST(RoceV2NativeN2, TimerRetryExhaustionPreservesPendingCompletions) {
    N2Pair pair;
    pair.sq_depth_messages = 2;
    pair.max_completion_records = 2;
    pair.max_tx_frames = 2;
    pair.retry_count = 0;
    pair.timeout_exp = 0;
    pair.configure();

    const std::vector<uint8_t> payload = n2_pattern(3, 0xc0deU);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.length = 1;
    write.remote_rkey = N2_RIGHT_RKEY;

    write.source_address = N2_LEFT_VA;
    write.remote_address = N2_RIGHT_VA;
    PostedWork completed;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, completed));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 10000, finish_ns));
    const EngineSnapshot after_success = pair.left.snapshot();
    EXPECT_EQ(0U,
              after_success.qps[0].transport.native.qp.ledger.outstanding_work);
    EXPECT_EQ(0U,
              after_success.qps[0].transport.native.qp.ledger.ready_completions);
    EXPECT_FALSE(pair.left.has_pending_work());

    const uint64_t post_ns = finish_ns + 1;
    write.source_address = N2_LEFT_VA + 1;
    write.remote_address = N2_RIGHT_VA + 1;
    PostedWork timed_out;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, post_ns, timed_out));
    const uint32_t next_sq_psn =
        pair.left.queue_pair(pair.left_qp())->snapshot().next_sq_psn;

    write.source_address = N2_LEFT_VA + 2;
    write.remote_address = N2_RIGHT_VA + 2;
    PostedWork rejected;
    Status rejected_status;
    ASSERT_FALSE(pair.left.post_write(pair.left_qp(),
                                      write,
                                      post_ns,
                                      rejected,
                                      &rejected_status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == rejected_status.code());
    const QpSnapshot after_rejection =
        pair.left.queue_pair(pair.left_qp())->snapshot();
    EXPECT_EQ(1U, after_rejection.ledger.outstanding_work);
    EXPECT_EQ(0U, after_rejection.ledger.ready_completions);
    EXPECT_EQ(next_sq_psn, after_rejection.next_sq_psn);

    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(post_ns, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    const uint64_t sequence = pair.left.peek_tx()->sequence;
    ASSERT_TRUE(pair.left.accept_tx(sequence, post_ns));
    const uint64_t deadline = pair.left.snapshot()
                                  .qps[0]
                                  .transport
                                  .native
                                  .oldest_deadline_ns;
    ASSERT_GT(deadline, post_ns);

    Status timeout_status;
    ASSERT_TRUE(pair.left.poll(deadline, poll, &timeout_status))
        << timeout_status.message();
    const EngineSnapshot exhausted = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == exhausted.state);
    EXPECT_TRUE(ErrorCode::OK == exhausted.error_code);
    EXPECT_TRUE(QpState::ERR == exhausted.qps[0].qp.state);
    EXPECT_EQ(1U,
              exhausted.qps[0].transport.native.counters.retry_exhausted);
    EXPECT_EQ(0U,
              exhausted.qps[0].transport.native.qp.ledger.outstanding_work);
    EXPECT_EQ(0U,
              exhausted.qps[0].transport.native.qp.ledger.ready_completions);

    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(2U, completions.size());
    EXPECT_TRUE(completed.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);
    EXPECT_TRUE(ErrorCode::OK == completions[0].error);
    EXPECT_TRUE(timed_out.id == completions[1].id);
    EXPECT_TRUE(CompletionState::ERROR == completions[1].state);
    EXPECT_TRUE(ErrorCode::RETRY_EXHAUSTED == completions[1].error);
    EXPECT_TRUE(pair.left_completions().empty());
}

TEST(RoceV2NativeN2, OuterTxBacklogDefersTimeoutFailureUntilAcceptance) {
    N2Pair pair;
    pair.max_tx_frames = 2;
    pair.retry_count = 0;
    pair.configure();

    pair.write_left_memory(0, std::vector<uint8_t>(2, 0x73));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.length = 1;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.source_address = N2_LEFT_VA;
    write.remote_address = N2_RIGHT_VA;
    PostedWork first;
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, first));
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));
    const uint64_t first_deadline = pair.left.snapshot()
                                        .qps[0]
                                        .transport
                                        .native
                                        .oldest_deadline_ns;
    ASSERT_GT(first_deadline, 1U);

    write.source_address = N2_LEFT_VA + 1;
    write.remote_address = N2_RIGHT_VA + 1;
    PostedWork second;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 1, second));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_EQ(1U, pair.left.snapshot().tx_queued);

    /* The global ring has spare capacity, but the same QP still owns an
     * unaccepted frame.  Its timeout cannot fail the QP and strand that frame
     * in the outer ring. */
    Status status;
    ASSERT_TRUE(pair.left.poll(first_deadline, poll, &status))
        << status.message();
    EngineSnapshot requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(1U, requester.tx_queued);
    EXPECT_EQ(0U,
              requester.qps[0].transport.native.counters.retry_exhausted);
    EXPECT_TRUE(pair.left_completions().empty());

    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence,
                                    first_deadline,
                                    &status))
        << status.message();
    requester = pair.left.snapshot();
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(0U, requester.tx_queued);
    const uint64_t final_deadline =
        requester.qps[0].transport.native.oldest_deadline_ns;
    ASSERT_EQ(first_deadline, final_deadline);

    ASSERT_TRUE(pair.left.poll(final_deadline, poll, &status))
        << status.message();
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::ERR == requester.qps[0].qp.state);
    EXPECT_EQ(0U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.retry_exhausted);
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(2U, completions.size());
    EXPECT_TRUE(first.id == completions[0].id);
    EXPECT_TRUE(second.id == completions[1].id);
    EXPECT_TRUE(CompletionState::ERROR == completions[0].state);
    EXPECT_TRUE(CompletionState::ERROR == completions[1].state);
    EXPECT_TRUE(ErrorCode::RETRY_EXHAUSTED == completions[0].error);
    EXPECT_TRUE(ErrorCode::RETRY_EXHAUSTED == completions[1].error);
}

TEST(RoceV2NativeN2, FatalNakWaitsForQueuedRequesterFramesBeforeError) {
    N2Pair pair;
    pair.max_tx_frames = 3;
    pair.ingress_budget_per_poll = 1;
    pair.configure();

    const std::vector<uint8_t> payload =
        n2_pattern(static_cast<size_t>(3) * pair.pmtu, 0xfa7a1U);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    ASSERT_EQ(3U, posted.reserved_psns);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        posted.first_psn,
        1,
        0x62)));
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        posted.terminal_psn,
        2,
        0x1f)));
    EnginePollResult poll;
    Status status;
    ASSERT_TRUE(pair.left.poll(0, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(3U, poll.frames_produced);
    EngineSnapshot requester = pair.left.snapshot();
    ASSERT_TRUE(EngineState::RUNNING == requester.state);
    ASSERT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    ASSERT_EQ(2U, requester.ingress_queued);
    ASSERT_EQ(3U, requester.tx_queued);
    ASSERT_EQ(0U,
              requester.qps[0].transport.native.counters.fatal_naks_received);

    for (uint32_t index = 0; index < 3; ++index) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));
    }
    ASSERT_EQ(NULL, pair.left.peek_tx());

    /* The later ACK is now at the ring head.  A persistent fatal-NAK fence
     * must survive the poll boundary and rotate it behind the NAK owner. */
    ASSERT_TRUE(pair.left.poll(1, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    requester = pair.left.snapshot();
    ASSERT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    ASSERT_EQ(2U, requester.ingress_queued);
    ASSERT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);
    ASSERT_TRUE(pair.left_completions().empty());

    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(0U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::ERR == requester.qps[0].qp.state);
    EXPECT_EQ(1U, requester.ingress_queued);
    EXPECT_EQ(0U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.fatal_naks_received);
    EXPECT_TRUE(ErrorCode::OK == requester.error_code);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);

    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_EQ(0U, requester.ingress_queued);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);

    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::ERROR == completions[0].state);
    EXPECT_TRUE(ErrorCode::REMOTE_ACCESS_ERROR == completions[0].error);
}

TEST(RoceV2NativeN2, FatalNakIsNotBlockedByAnotherQpTxBacklog) {
    N2Pair pair;
    pair.qps = 2;
    pair.max_tx_frames = 3;
    pair.configure();

    const std::vector<uint8_t> payload =
        n2_pattern(static_cast<size_t>(3) * pair.pmtu + 1, 0xfa17U);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork qp0_work;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(0), write, 0, qp0_work));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));

    write.source_address = N2_LEFT_VA + 1;
    write.length = static_cast<uint64_t>(3) * pair.pmtu;
    write.remote_address = N2_RIGHT_VA + 1;
    PostedWork qp1_work;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(1), write, 1, qp1_work));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(3U, poll.frames_produced);
    ASSERT_EQ(3U, pair.left.snapshot().tx_queued);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        qp0_work.first_psn,
        1,
        0x62)));
    Status status;
    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    const EngineSnapshot requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    ASSERT_EQ(2U, requester.qps.size());
    EXPECT_TRUE(QpState::ERR == requester.qps[0].qp.state);
    EXPECT_TRUE(QpState::RTS == requester.qps[1].qp.state);
    EXPECT_EQ(3U, requester.tx_queued);
    EXPECT_EQ(0U, requester.ingress_queued);
    EXPECT_TRUE(ErrorCode::OK == requester.error_code);

    const std::vector<CompletionRecord> completions = pair.left_completions(0);
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(qp0_work.id == completions[0].id);
    EXPECT_TRUE(CompletionState::ERROR == completions[0].state);
    EXPECT_TRUE(ErrorCode::REMOTE_ACCESS_ERROR == completions[0].error);
}

TEST(RoceV2NativeN2, RnrNakWaitsForBacklogAndHonorsResumeTimer) {
    N2Pair pair;
    pair.max_tx_frames = 3;
    pair.configure();

    const std::vector<uint8_t> payload =
        n2_pattern(static_cast<size_t>(3) * pair.pmtu, 0x7a7aU);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.with_immediate = true;
    write.immediate_data = 0x7a7a7a7aU;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    ASSERT_EQ(3U, posted.reserved_psns);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        posted.first_psn,
        1,
        0x21)));
    EnginePollResult poll;
    Status status;
    ASSERT_TRUE(pair.left.poll(0, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(3U, poll.frames_produced);
    EngineSnapshot requester = pair.left.snapshot();
    ASSERT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    ASSERT_EQ(1U, requester.ingress_queued);
    ASSERT_EQ(3U, requester.tx_queued);
    ASSERT_EQ(0U,
              requester.qps[0].transport.native.counters.rnr_naks_received);

    for (uint32_t index = 0; index < 3; ++index) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));
    }
    ASSERT_EQ(NULL, pair.left.peek_tx());

    ASSERT_TRUE(pair.left.poll(1, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(0U, poll.frames_produced);
    requester = pair.left.snapshot();
    ASSERT_TRUE(EngineState::RUNNING == requester.state);
    ASSERT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    ASSERT_EQ(0U, requester.ingress_queued);
    ASSERT_EQ(0U, requester.tx_queued);
    ASSERT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_naks_received);
    const uint64_t resume_ns =
        requester.qps[0].transport.native.rnr_resume_ns;
    ASSERT_GT(resume_ns, 1U);

    ASSERT_TRUE(pair.left.poll(resume_ns - 1U, poll, &status))
        << status.message();
    EXPECT_EQ(0U, poll.frames_produced);
    EXPECT_EQ(0U, pair.left.snapshot().tx_queued);
    EXPECT_EQ(0U,
              pair.left.snapshot()
                  .qps[0]
                  .transport
                  .native
                  .counters
                  .rnr_retries);

    ASSERT_TRUE(pair.left.poll(resume_ns, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(1U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_retries);
    EXPECT_EQ(0U, requester.qps[0].transport.native.rnr_resume_ns);
    EXPECT_TRUE(pair.left_completions().empty());

    /* A resumed replay advances only after the frame already published to the
     * outer TX ring is accepted.  This keeps the QP retry timer from failing a
     * packet that the dataplane still owns. */
    for (uint32_t frame = 1; frame < 3; ++frame) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence,
                                        resume_ns + frame - 1U));
        ASSERT_TRUE(pair.left.poll(resume_ns + frame, poll, &status))
            << status.message();
        EXPECT_EQ(1U, poll.frames_produced);
        EXPECT_EQ(1U, pair.left.snapshot().tx_queued);
    }
}

TEST(RoceV2NativeN2, RnrFenceAndResumeSurvivePollBudgetAndEarlierAck) {
    N2Pair pair;
    pair.max_tx_frames = 2;
    pair.ingress_budget_per_poll = 1;
    pair.configure();

    pair.write_left_memory(0, std::vector<uint8_t>(2, 0x71));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.length = 1;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork first;
    PostedWork second;
    write.source_address = N2_LEFT_VA;
    write.remote_address = N2_RIGHT_VA;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, first));
    write.source_address = N2_LEFT_VA + 1;
    write.remote_address = N2_RIGHT_VA + 1;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, second));
    ASSERT_EQ((first.terminal_psn + 1U) & PSN24_MASK, second.first_psn);

    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(2U, poll.frames_produced);
    ASSERT_EQ(2U, pair.left.snapshot().tx_queued);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        second.first_psn,
        1,
        0x21)));
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        first.terminal_psn,
        2,
        0x1f)));

    Status status;
    ASSERT_TRUE(pair.left.poll(0, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(2U, pair.left.snapshot().ingress_queued);

    /* The ACK for the older WQE is at the ring head on a new poll, but must
     * remain behind the earlier RNR NAK for the next WQE. */
    ASSERT_TRUE(pair.left.poll(1, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EngineSnapshot requester = pair.left.snapshot();
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_EQ(0U,
              requester.qps[0].transport.native.counters.rnr_naks_received);

    for (uint32_t index = 0; index < 2; ++index) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 1));
    }
    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    const uint64_t resume_ns =
        requester.qps[0].transport.native.rnr_resume_ns;
    ASSERT_GT(resume_ns, 2U);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_naks_received);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_EQ(2U,
              requester.qps[0].transport.native.qp.ledger.outstanding_work);

    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_EQ(1U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.qp.ledger.outstanding_work);
    EXPECT_EQ(resume_ns, requester.qps[0].transport.native.rnr_resume_ns);
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(first.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);

    ASSERT_TRUE(pair.left.poll(resume_ns, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_EQ(1U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_retries);
    EXPECT_EQ(0U, requester.qps[0].transport.native.rnr_resume_ns);
}

TEST(RoceV2NativeN2, EarlierReadResponseCannotReplenishLaterRnrBudget) {
    N2Pair pair;
    pair.max_tx_frames = 2;
    pair.rnr_retry_count = 1;
    pair.configure();

    const std::vector<uint8_t> read_payload(1, 0x74);
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x500;
    read.length = read_payload.size();
    read.remote_address = N2_RIGHT_VA + 0x500;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork read_posted;
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, read_posted));
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));

    pair.write_left_memory(0x600, std::vector<uint8_t>(1, 0x75));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + 0x600;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA + 0x600;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork write_posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 1, write_posted));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 1));

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        write_posted.first_psn,
        1,
        0x21)));
    Status status;
    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    EngineSnapshot requester = pair.left.snapshot();
    const uint64_t resume_ns =
        requester.qps[0].transport.native.rnr_resume_ns;
    ASSERT_GT(resume_ns, 3U);
    ASSERT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_naks_received);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_read_response_ingress(
        pair.left_qpn(0), read_posted.first_psn, 2, read_payload)));
    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    requester = pair.left.snapshot();
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(resume_ns, requester.qps[0].transport.native.rnr_resume_ns);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.read_response_packets);
    std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(read_posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 3));

    ASSERT_TRUE(pair.left.poll(resume_ns, poll, &status)) << status.message();
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, resume_ns));

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        write_posted.first_psn,
        3,
        0x21)));
    ASSERT_TRUE(pair.left.poll(resume_ns + 1U, poll, &status))
        << status.message();
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::ERR == requester.qps[0].qp.state);
    EXPECT_EQ(2U,
              requester.qps[0].transport.native.counters.rnr_naks_received);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_retry_exhausted);
    EXPECT_EQ(0U, requester.qps[0].transport.native.rnr_resume_ns);
    completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(write_posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::ERROR == completions[0].state);
    EXPECT_TRUE(ErrorCode::RNR_RETRY_EXHAUSTED == completions[0].error);
}

TEST(RoceV2NativeN2, UnknownRnrPsnCannotCancelRequesterRetryDeadline) {
    N2Pair pair;
    pair.max_tx_frames = 1;
    pair.configure();

    pair.write_left_memory(0, std::vector<uint8_t>(1, 0x72));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));

    EngineSnapshot requester = pair.left.snapshot();
    const uint64_t retry_deadline =
        requester.qps[0].transport.native.oldest_deadline_ns;
    ASSERT_GT(retry_deadline, 1U);
    uint32_t unknown_psn = 0;
    ASSERT_TRUE(psn24_add(posted.terminal_psn, 0x100, unknown_psn));
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        unknown_psn,
        1,
        0x21)));

    Status status;
    ASSERT_TRUE(pair.left.poll(1, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.qp.ledger.outstanding_work);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.malformed_packets);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_naks_received);
    EXPECT_EQ(0U, requester.qps[0].transport.native.rnr_resume_ns);
    EXPECT_EQ(retry_deadline,
              requester.qps[0].transport.native.oldest_deadline_ns);

    ASSERT_TRUE(pair.left.poll(retry_deadline, poll, &status))
        << status.message();
    EXPECT_EQ(1U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_EQ(1U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.timeout_retries);
    EXPECT_EQ(0U,
              requester.qps[0].transport.native.counters.rnr_retries);
}

TEST(RoceV2NativeN2, RnrSuspendsActiveChunkedReplayUntilResumeTimer) {
    N2Pair pair;
    pair.max_tx_frames = 3;
    pair.configure();

    const size_t work_bytes = static_cast<size_t>(3) * pair.pmtu;
    pair.write_left_memory(0, n2_pattern(2 * work_bytes, 0x7272U));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.length = work_bytes;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork first;
    PostedWork second;
    EnginePollResult poll;
    for (uint32_t work_index = 0; work_index < 2; ++work_index) {
        const uint64_t offset = static_cast<uint64_t>(work_index) * work_bytes;
        write.source_address = N2_LEFT_VA + offset;
        write.remote_address = N2_RIGHT_VA + offset;
        PostedWork &posted = work_index == 0 ? first : second;
        ASSERT_TRUE(pair.left.post_write(pair.left_qp(),
                                         write,
                                         work_index,
                                         posted));
        ASSERT_TRUE(pair.left.poll(work_index, poll));
        ASSERT_EQ(3U, poll.frames_produced);
        for (uint32_t frame = 0; frame < 3; ++frame) {
            ASSERT_TRUE(pair.left.peek_tx() != NULL);
            ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence,
                                            work_index));
        }
    }
    ASSERT_EQ((first.terminal_psn + 1U) & PSN24_MASK, second.first_psn);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        first.first_psn,
        1,
        0x60)));
    Status status;
    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    ASSERT_EQ(1U, poll.ingress_processed);
    ASSERT_EQ(3U, poll.frames_produced);
    for (uint32_t frame = 0; frame < 3; ++frame) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 2));
    }

    /* The replay still owns the second three-frame chunk.  An RNR for the
     * first replayed PSN must cancel that active cursor and publish nothing
     * until the negotiated RNR timer expires. */
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        first.first_psn,
        2,
        0x21)));
    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(0U, poll.frames_produced);
    EngineSnapshot requester = pair.left.snapshot();
    const uint64_t resume_ns =
        requester.qps[0].transport.native.rnr_resume_ns;
    ASSERT_GT(resume_ns, 3U);
    EXPECT_EQ(0U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_naks_received);

    ASSERT_TRUE(pair.left.poll(resume_ns - 1U, poll, &status))
        << status.message();
    EXPECT_EQ(0U, poll.frames_produced);
    EXPECT_EQ(0U, pair.left.snapshot().tx_queued);

    ASSERT_TRUE(pair.left.poll(resume_ns, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_EQ(1U, requester.tx_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.rnr_retries);
    EXPECT_EQ(0U, requester.qps[0].transport.native.rnr_resume_ns);

    /* Remaining replay chunks advance only after each already-published frame
     * has left the engine ring, so a later timeout cannot invalidate it. */
    for (uint32_t frame = 1; frame < 3; ++frame) {
        ASSERT_TRUE(pair.left.peek_tx() != NULL);
        ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence,
                                        resume_ns + frame - 1U));
        ASSERT_TRUE(pair.left.poll(resume_ns + frame, poll, &status))
            << status.message();
        EXPECT_EQ(1U, poll.frames_produced);
        EXPECT_EQ(1U, pair.left.snapshot().tx_queued);
    }
}

TEST(RoceV2NativeN2, ResponderBackpressureRetainsIngressUntilAckQueueDrains) {
    N2Pair pair;
    pair.max_tx_frames = 17;
    pair.configure();

    const std::vector<uint8_t> burst = n2_pattern(18, 0x17U);
    pair.write_left_memory(0, burst);

    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    write.with_immediate = true;
    write.immediate_data = 0x17001700U;

    PostedWork posted;
    for (uint32_t index = 0; index < 17; ++index) {
        write.source_address = N2_LEFT_VA + index;
        write.remote_address = N2_RIGHT_VA + index;
        write.immediate_data = 0x17001700U + index;
        ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
        ASSERT_EQ(1U, posted.reserved_psns);
    }

    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(17U, poll.frames_produced);
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(17, requests));
    ASSERT_EQ(17U, requests.size());

    write.source_address = N2_LEFT_VA + 17;
    write.remote_address = N2_RIGHT_VA + 17;
    write.immediate_data = 0x17001711U;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 1, posted));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.drain_tx(1, requests));
    ASSERT_EQ(18U, requests.size());

    for (size_t index = 0; index < requests.size(); ++index) {
        ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[index])));
    }
    ASSERT_EQ(18U, pair.right.snapshot().ingress_queued);

    Status status;
    ASSERT_TRUE(pair.right.poll(0, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(17U, poll.ingress_processed);
    EXPECT_EQ(1U, pair.right.snapshot().ingress_queued);
    EXPECT_EQ(17U, pair.right.snapshot().tx_queued);
    ASSERT_TRUE(pair.right.peek_tx() != NULL);
    const uint64_t blocked_sequence = pair.right.peek_tx()->sequence;
    EXPECT_EQ((pair.left_start_psn + 17U) & PSN24_MASK,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);

    ASSERT_TRUE(pair.right.poll(1, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(1U, pair.right.snapshot().ingress_queued);
    EXPECT_EQ(17U, pair.right.snapshot().tx_queued);
    ASSERT_TRUE(pair.right.peek_tx() != NULL);
    EXPECT_EQ(blocked_sequence, pair.right.peek_tx()->sequence);

    std::vector<EngineWireFrame> acknowledgements;
    ASSERT_TRUE(pair.right.drain_tx(17, acknowledgements));
    ASSERT_EQ(17U, acknowledgements.size());
    ASSERT_TRUE(pair.right.poll(2, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(0U, pair.right.snapshot().ingress_queued);
    EXPECT_EQ(1U, pair.right.snapshot().tx_queued);
    ASSERT_TRUE(pair.right.drain_tx(1, acknowledgements));
    EXPECT_EQ(18U, acknowledgements.size());
    for (size_t index = 0; index < acknowledgements.size(); ++index) {
        DecodedPacket ack;
        CodecError error;
        ASSERT_TRUE(NativePacketCodec::decode(
            ByteSpan(acknowledgements[index].bytes.data(),
                     acknowledgements[index].bytes.size()),
            ack,
            &error)) << error.message;
        EXPECT_TRUE(IB_ACKNOWLEDGE == ack.opcode);
        EXPECT_EQ((pair.left_start_psn + static_cast<uint32_t>(index)) &
                      PSN24_MASK,
                  ack.psn);
    }

    const EngineSnapshot responder = pair.right.snapshot();
    EXPECT_EQ(18U, responder.counters.ingress_processed);
    EXPECT_EQ(18U, responder.qps[0].transport.rx_packets);
    EXPECT_EQ(18U, responder.qps[0].transport.tx_packets);
    EXPECT_EQ((pair.left_start_psn + 18U) & PSN24_MASK,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);
}

TEST(RoceV2NativeN2, PacedReadResponsesRetainLaterIngressUntilQueueDrains) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.configure();

    PacketEncodeSpec cnp;
    cnp.endpoint = n2_endpoint(true);
    cnp.opcode = IB_CNP;
    cnp.dest_qpn = pair.right_qpn(0);
    cnp.psn = 0;
    cnp.has_reth = false;
    cnp.becn = true;
    EngineWireFrame congestion;
    congestion.generation = SessionGeneration(N2_GENERATION);
    congestion.source_endpoint = N2_LEFT_ENDPOINT;
    congestion.destination_endpoint = N2_RIGHT_ENDPOINT;
    congestion.source_qp_index = 0;
    congestion.egress_port = N2_LEFT_PORT;
    congestion.ingress_port = N2_RIGHT_PORT;
    congestion.sequence = 1;
    std::vector<uint8_t> encoded_cnp;
    CodecError cnp_error;
    ASSERT_TRUE(NativePacketCodec::encode(cnp, encoded_cnp, &cnp_error))
        << cnp_error.message;
    congestion.bytes.assign(encoded_cnp.data(),
                            encoded_cnp.data() + encoded_cnp.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(congestion)));
    EnginePollResult poll;
    ASSERT_TRUE(pair.right.poll(0, poll));
    ASSERT_EQ(1U, poll.ingress_processed);
    EXPECT_DOUBLE_EQ(0.5,
                     pair.right.snapshot().qps[0].transport.native.rate_scale);

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.length = static_cast<uint64_t>(5) * pair.pmtu;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    for (uint32_t index = 0; index < 2; ++index) {
        read.destination_address = N2_LEFT_VA + index * 0x10000;
        read.remote_address = N2_RIGHT_VA + index * 0x10000;
        ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 1, posted));
        ASSERT_EQ(5U, posted.reserved_psns);
    }
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(2U, poll.frames_produced);
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(2, requests));
    ASSERT_EQ(2U, requests.size());
    for (size_t index = 0; index < requests.size(); ++index) {
        ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[index])));
    }

    Status status;
    ASSERT_TRUE(pair.right.poll(1, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(1U, poll.ingress_processed);
    EngineSnapshot responder = pair.right.snapshot();
    EXPECT_EQ(1U, responder.ingress_queued);
    EXPECT_EQ(5U, responder.qps[0].transport.native.queued_tx_frames);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.read_requests_received);

    ASSERT_TRUE(pair.right.poll(1000000, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(5U, pair.right.snapshot().tx_queued);
    std::vector<EngineWireFrame> responses;
    ASSERT_TRUE(pair.right.drain_tx(5, responses));
    ASSERT_EQ(5U, responses.size());

    ASSERT_TRUE(pair.right.poll(1000001, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(1U, poll.ingress_processed);
    responder = pair.right.snapshot();
    EXPECT_EQ(0U, responder.ingress_queued);
    EXPECT_EQ(5U, responder.tx_queued);
    EXPECT_EQ(2U,
              responder.qps[0].transport.native.counters.read_requests_received);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);
}

TEST(RoceV2NativeN2, AckBypassesCongestionPacedResponderBacklog) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.configure();

    const std::vector<uint8_t> write_payload(1, 0xa5);
    pair.write_left_memory(0, write_payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = write_payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork write_posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, write_posted));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));
    EXPECT_EQ(1U, pair.left.snapshot()
                      .qps[0]
                      .transport
                      .native
                      .qp
                      .ledger
                      .outstanding_work);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false, IB_CNP, pair.left_qpn(0), 0, 1)));
    ASSERT_TRUE(pair.left.poll(1, poll));
    EngineSnapshot requester = pair.left.snapshot();
    ASSERT_EQ(1U, requester.qps[0].transport.native.counters.cnp_received);
    ASSERT_DOUBLE_EQ(0.5, requester.qps[0].transport.native.rate_scale);

    const uint64_t remote_offset = 0x10000;
    const uint64_t local_offset = 0x40000;
    const size_t read_length = static_cast<size_t>(5) * pair.pmtu;
    pair.write_left_memory(remote_offset,
                           n2_pattern(read_length, 0xacce55U));
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_RIGHT_VA + local_offset;
    read.length = read_length;
    read.remote_address = N2_LEFT_VA + remote_offset;
    read.remote_rkey = N2_LEFT_RKEY;
    PostedWork read_posted;
    ASSERT_TRUE(pair.right.post_read(pair.right_qp(), read, 0, read_posted));
    ASSERT_TRUE(pair.right.poll(0, poll));
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.right.drain_tx(1, requests));
    ASSERT_EQ(1U, requests.size());
    ASSERT_TRUE(pair.left.enqueue_ingress(std::move(requests[0])));
    ASSERT_TRUE(pair.left.poll(2, poll));
    requester = pair.left.snapshot();
    ASSERT_EQ(0U, requester.tx_queued);
    ASSERT_EQ(5U, requester.qps[0].transport.native.queued_tx_frames);
    ASSERT_GT(requester.qps[0].transport.native.next_send_ns, 2U);
    ASSERT_EQ(1U, requester.qps[0]
                      .transport
                      .native
                      .qp
                      .ledger
                      .outstanding_work);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        write_posted.terminal_psn,
        2,
        0x1f)));
    Status status;
    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_EQ(0U, requester.ingress_queued);
    EXPECT_EQ(0U, requester.tx_queued);
    EXPECT_EQ(5U, requester.qps[0].transport.native.queued_tx_frames);
    EXPECT_EQ(1U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_EQ(0U, requester.qps[0]
                      .transport
                      .native
                      .qp
                      .ledger
                      .outstanding_work);
    EXPECT_TRUE(ErrorCode::OK == requester.error_code);
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(write_posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);
    EXPECT_TRUE(ErrorCode::OK == completions[0].error);
}

TEST(RoceV2NativeN2, ReadResponseFencePrecedesLaterWriteAckAcrossPollBudget) {
    N2Pair pair;
    pair.qps = 2;
    pair.max_tx_frames = 1;
    pair.ingress_budget_per_poll = 1;
    pair.configure();

    const std::vector<uint8_t> read_payload(1, 0xd1);
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x100;
    read.length = read_payload.size();
    read.remote_address = N2_RIGHT_VA + 0x100;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork read_posted;
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(0), read, 0, read_posted));
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));

    pair.write_left_memory(0x200, std::vector<uint8_t>(2, 0xd2));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + 0x200;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA + 0x200;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork write_posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(0), write, 1, write_posted));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 1));

    /* Keep the one-frame engine TX ring occupied by an independent QP. */
    write.source_address = N2_LEFT_VA + 0x201;
    write.remote_address = N2_RIGHT_VA + 0x201;
    PostedWork other_qp_work;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(1), write, 2, other_qp_work));
    ASSERT_TRUE(pair.left.poll(2, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_EQ(1U, pair.left.snapshot().tx_queued);

    ASSERT_TRUE(pair.left.enqueue_ingress(n2_read_response_ingress(
        pair.left_qpn(0), read_posted.first_psn, 1, read_payload)));
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_control_ingress(
        false,
        IB_ACKNOWLEDGE,
        pair.left_qpn(0),
        write_posted.terminal_psn,
        2,
        0x1f)));

    Status status;
    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EngineSnapshot requester = pair.left.snapshot();
    EXPECT_EQ(2U, requester.ingress_queued);
    EXPECT_EQ(2U,
              requester.qps[0].transport.native.qp.ledger.outstanding_work);
    EXPECT_EQ(0U,
              requester.qps[0].transport.native.counters.read_response_packets);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);

    /* The ACK is at the head on a new poll, but the deferred read response is
     * its persistent response-stream barrier owner. */
    ASSERT_TRUE(pair.left.poll(4, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_EQ(2U, requester.ingress_queued);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_TRUE(pair.left_completions(0).empty());

    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 4));
    ASSERT_TRUE(pair.left.poll(5, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(1U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_EQ(1U, requester.ingress_queued);
    EXPECT_EQ(1U,
              requester.qps[0].transport.native.counters.read_response_packets);
    EXPECT_EQ(0U, requester.qps[0].transport.native.counters.acks_received);
    std::vector<CompletionRecord> completions = pair.left_completions(0);
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(read_posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);

    ASSERT_TRUE(pair.left.poll(6, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(0U, poll.frames_produced);
    requester = pair.left.snapshot();
    EXPECT_EQ(0U, requester.ingress_queued);
    EXPECT_EQ(1U, requester.qps[0].transport.native.counters.acks_received);
    EXPECT_EQ(0U,
              requester.qps[0].transport.native.qp.ledger.outstanding_work);
    completions = pair.left_completions(0);
    ASSERT_EQ(1U, completions.size());
    EXPECT_TRUE(write_posted.id == completions[0].id);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);
    EXPECT_TRUE(ErrorCode::OK == requester.error_code);
}

TEST(RoceV2NativeN2, MalformedReadResponseWaitsForSameQpOuterTx) {
    N2Pair pair;
    pair.max_tx_frames = 2;
    pair.configure();

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x300;
    read.length = 2;
    read.remote_address = N2_RIGHT_VA + 0x300;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork read_posted;
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, read_posted));
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 0));

    pair.write_left_memory(0x400, std::vector<uint8_t>(1, 0xd4));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + 0x400;
    write.length = 1;
    write.remote_address = N2_RIGHT_VA + 0x400;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork write_posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 1, write_posted));
    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    ASSERT_EQ(1U, pair.left.snapshot().tx_queued);

    /* READ_RESPONSE_ONLY carries one byte for a two-byte READ and will fail
     * the requester QP once consumed.  It must wait until the same QP's outer
     * WRITE frame is accepted so no stale frame remains after ERR. */
    ASSERT_TRUE(pair.left.enqueue_ingress(n2_read_response_ingress(
        pair.left_qpn(0),
        read_posted.first_psn,
        1,
        std::vector<uint8_t>(1, 0xd3))));
    Status status;
    ASSERT_TRUE(pair.left.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EngineSnapshot requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::RTS == requester.qps[0].qp.state);
    EXPECT_EQ(1U, requester.ingress_queued);
    EXPECT_EQ(1U, requester.tx_queued);

    ASSERT_TRUE(pair.left.peek_tx() != NULL);
    ASSERT_TRUE(pair.left.accept_tx(pair.left.peek_tx()->sequence, 2, &status))
        << status.message();
    ASSERT_TRUE(pair.left.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    requester = pair.left.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == requester.state);
    EXPECT_TRUE(QpState::ERR == requester.qps[0].qp.state);
    EXPECT_EQ(0U, requester.ingress_queued);
    EXPECT_EQ(0U, requester.tx_queued);
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(2U, completions.size());
    for (size_t index = 0; index < completions.size(); ++index) {
        EXPECT_TRUE(CompletionState::ERROR == completions[index].state);
        EXPECT_TRUE(ErrorCode::REMOTE_OPERATION_ERROR == completions[index].error);
    }
}

TEST(RoceV2NativeN2, CnpBypassesCongestionPacedResponderBacklog) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.configure();

    ASSERT_TRUE(pair.right.enqueue_ingress(n2_control_ingress(
        true, IB_CNP, pair.right_qpn(0), 0, 1)));
    EnginePollResult poll;
    ASSERT_TRUE(pair.right.poll(0, poll));
    EngineSnapshot responder = pair.right.snapshot();
    ASSERT_EQ(1U, responder.qps[0].transport.native.counters.cnp_received);
    ASSERT_DOUBLE_EQ(0.5, responder.qps[0].transport.native.rate_scale);

    const size_t length = static_cast<size_t>(5) * pair.pmtu;
    pair.write_right_memory(0x10000, n2_pattern(length, 0xc0ffeeU));
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x40000;
    read.length = length;
    read.remote_address = N2_RIGHT_VA + 0x10000;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 1, posted));
    ASSERT_TRUE(pair.left.poll(1, poll));
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(1, requests));
    ASSERT_EQ(1U, requests.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[0])));
    ASSERT_TRUE(pair.right.poll(1, poll));
    responder = pair.right.snapshot();
    ASSERT_EQ(0U, responder.tx_queued);
    ASSERT_EQ(5U, responder.qps[0].transport.native.queued_tx_frames);
    const uint64_t first_send_ns =
        responder.qps[0].transport.native.next_send_ns;
    ASSERT_GT(first_send_ns, 1U);

    ASSERT_TRUE(pair.right.enqueue_ingress(n2_control_ingress(
        true, IB_CNP, pair.right_qpn(0), 0, 2)));
    Status status;
    ASSERT_TRUE(pair.right.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    responder = pair.right.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == responder.state);
    EXPECT_EQ(0U, responder.ingress_queued);
    EXPECT_EQ(0U, responder.tx_queued);
    EXPECT_EQ(5U, responder.qps[0].transport.native.queued_tx_frames);
    EXPECT_EQ(2U, responder.qps[0].transport.native.counters.cnp_received);
    EXPECT_DOUBLE_EQ(0.25, responder.qps[0].transport.native.rate_scale);
    EXPECT_GT(responder.qps[0].transport.native.next_send_ns,
              first_send_ns);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);
}

TEST(RoceV2NativeN2, RequestRotationLetsCnpBypassWithoutReorderingSameQpWrite) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.ingress_budget_per_poll = 2;
    pair.configure();

    ASSERT_TRUE(pair.right.enqueue_ingress(n2_control_ingress(
        true, IB_CNP, pair.right_qpn(0), 0, 1)));
    EnginePollResult poll;
    ASSERT_TRUE(pair.right.poll(0, poll));

    const size_t paced_length = static_cast<size_t>(5) * pair.pmtu;
    const std::vector<uint8_t> paced_payload =
        n2_pattern(paced_length, 0xfeceU);
    pair.write_right_memory(0, paced_payload);
    EngineWriteRequest paced_write;
    paced_write.source_memory_id = 1;
    paced_write.source_address = N2_RIGHT_VA;
    paced_write.length = paced_length;
    paced_write.remote_address = N2_LEFT_VA;
    paced_write.remote_rkey = N2_LEFT_RKEY;
    PostedWork paced_posted;
    ASSERT_TRUE(pair.right.post_write(pair.right_qp(),
                                      paced_write,
                                      1,
                                      paced_posted));
    EngineSnapshot responder = pair.right.snapshot();
    ASSERT_EQ(0U, responder.tx_queued);
    ASSERT_EQ(5U, responder.qps[0].transport.native.queued_tx_frames);

    const std::vector<uint8_t> one_byte(1, 0x5a);
    pair.write_left_memory(0x10000, one_byte);
    pair.write_right_memory(0x20000, one_byte);
    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x20000;
    read.length = one_byte.size();
    read.remote_address = N2_RIGHT_VA + 0x20000;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork read_posted;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 1, read_posted));

    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + 0x10000;
    write.length = one_byte.size();
    write.remote_address = N2_RIGHT_VA + 0x30000;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork write_posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 1, write_posted));
    ASSERT_EQ((read_posted.terminal_psn + 1U) & PSN24_MASK,
              write_posted.first_psn);

    ASSERT_TRUE(pair.left.poll(1, poll));
    ASSERT_EQ(2U, poll.frames_produced);
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(2, requests));
    ASSERT_EQ(2U, requests.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[0])));
    ASSERT_TRUE(pair.right.enqueue_ingress(n2_control_ingress(
        true, IB_CNP, pair.right_qpn(0), 0, 2)));
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[1])));

    Status status;
    ASSERT_TRUE(pair.right.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    responder = pair.right.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == responder.state);
    EXPECT_EQ(2U, responder.ingress_queued);
    EXPECT_EQ(0U, responder.tx_queued);
    EXPECT_EQ(5U, responder.qps[0].transport.native.queued_tx_frames);
    EXPECT_EQ(2U, responder.qps[0].transport.native.counters.cnp_received);
    EXPECT_EQ(0U,
              responder.qps[0].transport.native.counters.read_requests_received);
    EXPECT_EQ(0U, responder.qps[0].transport.native.counters.deliveries);
    EXPECT_EQ(0U,
              responder.qps[0].transport.native.counters.sequence_naks_sent);
    EXPECT_EQ(pair.left_start_psn,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);

    /* The later WRITE is at the ring head after the first two-entry poll.
     * Crossing another poll boundary must not clear the READ request's fence
     * or turn the WRITE into an artificial sequence NAK. */
    ASSERT_TRUE(pair.right.poll(3, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    responder = pair.right.snapshot();
    EXPECT_EQ(2U, responder.ingress_queued);
    EXPECT_EQ(0U, responder.tx_queued);
    EXPECT_EQ(0U, responder.qps[0].transport.native.counters.deliveries);
    EXPECT_EQ(0U,
              responder.qps[0].transport.native.counters.sequence_naks_sent);
    EXPECT_EQ(pair.left_start_psn,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);

    ASSERT_TRUE(pair.right.poll(1000002, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    EXPECT_EQ(5U, poll.frames_produced);
    std::vector<EngineWireFrame> paced_frames;
    ASSERT_TRUE(pair.right.drain_tx(5, paced_frames));
    ASSERT_EQ(5U, paced_frames.size());

    ASSERT_TRUE(pair.right.poll(1000003, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(1U, poll.frames_produced);
    responder = pair.right.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == responder.state);
    EXPECT_EQ(1U, responder.ingress_queued);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.read_requests_received);
    EXPECT_EQ(0U, responder.qps[0].transport.native.counters.deliveries);
    EXPECT_EQ(0U,
              responder.qps[0].transport.native.counters.sequence_naks_sent);
    EXPECT_EQ((pair.left_start_psn + 1U) & PSN24_MASK,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);

    ASSERT_TRUE(pair.right.poll(1000004, poll, &status)) << status.message();
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(1U, poll.frames_produced);
    responder = pair.right.snapshot();
    EXPECT_TRUE(EngineState::RUNNING == responder.state);
    EXPECT_EQ(0U, responder.ingress_queued);
    EXPECT_EQ(1U, responder.qps[0].transport.native.counters.deliveries);
    EXPECT_EQ(0U,
              responder.qps[0].transport.native.counters.sequence_naks_sent);
    EXPECT_EQ((pair.left_start_psn + 2U) & PSN24_MASK,
              pair.right.queue_pair(pair.right_qp())->runtime_view().expected_rx_psn);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);
}

TEST(RoceV2NativeN2, CachedReadReplayRetainsSecondNakUntilFirstReplayDrains) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.configure();

    const uint64_t remote_offset = 0x10000;
    const uint64_t local_offset = 0x40000;
    const size_t length = static_cast<size_t>(5) * pair.pmtu;
    const std::vector<uint8_t> expected = n2_pattern(length, 0xca5eU);
    pair.write_right_memory(remote_offset, expected);

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + local_offset;
    read.length = length;
    read.remote_address = N2_RIGHT_VA + remote_offset;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, posted));
    ASSERT_EQ(5U, posted.reserved_psns);

    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(1U, poll.frames_produced);
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(1, requests));
    ASSERT_EQ(1U, requests.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[0])));

    Status status;
    ASSERT_TRUE(pair.right.poll(0, poll, &status)) << status.message();
    ASSERT_TRUE(EngineState::RUNNING == pair.right.state());
    EngineSnapshot responder = pair.right.snapshot();
    ASSERT_EQ(0U, responder.ingress_queued);
    ASSERT_EQ(5U, responder.tx_queued);
    ASSERT_EQ(1U, responder.qps[0].transport.native.read_cache_entries);
    ASSERT_EQ(5U,
              responder.qps[0].transport.native.counters.read_response_packets);
    std::vector<EngineWireFrame> original_responses;
    ASSERT_TRUE(pair.right.drain_tx(5, original_responses));
    ASSERT_EQ(5U, original_responses.size());

    PacketEncodeSpec nak;
    nak.endpoint = n2_endpoint(true);
    nak.opcode = IB_ACKNOWLEDGE;
    nak.dest_qpn = pair.right_qpn(0);
    nak.psn = posted.first_psn;
    nak.has_reth = false;
    nak.has_aeth = true;
    nak.aeth.syndrome = 0x60;
    nak.aeth.msn = 1;
    std::vector<uint8_t> encoded_nak;
    CodecError nak_error;
    ASSERT_TRUE(NativePacketCodec::encode(nak, encoded_nak, &nak_error))
        << nak_error.message;
    for (uint64_t sequence = 1; sequence <= 2; ++sequence) {
        EngineWireFrame frame;
        frame.generation = SessionGeneration(N2_GENERATION);
        frame.source_endpoint = N2_LEFT_ENDPOINT;
        frame.destination_endpoint = N2_RIGHT_ENDPOINT;
        frame.source_qp_index = 0;
        frame.egress_port = N2_LEFT_PORT;
        frame.ingress_port = N2_RIGHT_PORT;
        frame.sequence = sequence;
        frame.bytes.assign(encoded_nak.data(),
                           encoded_nak.data() + encoded_nak.size());
        ASSERT_TRUE(pair.right.enqueue_ingress(std::move(frame)));
    }

    ASSERT_TRUE(pair.right.poll(1, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(5U, poll.frames_produced);
    responder = pair.right.snapshot();
    EXPECT_EQ(1U, responder.ingress_queued);
    EXPECT_EQ(5U, responder.tx_queued);
    EXPECT_EQ(0U, responder.qps[0].transport.queued_tx_frames);
    EXPECT_EQ(1U, responder.qps[0].transport.native.read_cache_entries);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.sequence_naks_received);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.read_cache_hits);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.retransmit_events);
    EXPECT_EQ(5U,
              responder.qps[0].transport.native.counters.retransmit_packets);
    ASSERT_TRUE(pair.right.peek_tx() != NULL);
    const uint64_t blocked_sequence = pair.right.peek_tx()->sequence;

    ASSERT_TRUE(pair.right.poll(2, poll, &status)) << status.message();
    EXPECT_EQ(0U, poll.ingress_processed);
    responder = pair.right.snapshot();
    EXPECT_EQ(1U, responder.ingress_queued);
    EXPECT_EQ(5U, responder.tx_queued);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.sequence_naks_received);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.read_cache_hits);
    EXPECT_EQ(1U,
              responder.qps[0].transport.native.counters.retransmit_events);
    EXPECT_EQ(5U,
              responder.qps[0].transport.native.counters.retransmit_packets);
    ASSERT_TRUE(pair.right.peek_tx() != NULL);
    EXPECT_EQ(blocked_sequence, pair.right.peek_tx()->sequence);

    std::vector<EngineWireFrame> first_replay;
    ASSERT_TRUE(pair.right.drain_tx(5, first_replay));
    ASSERT_EQ(5U, first_replay.size());
    ASSERT_TRUE(pair.right.poll(3, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    EXPECT_EQ(1U, poll.ingress_processed);
    EXPECT_EQ(5U, poll.frames_produced);
    responder = pair.right.snapshot();
    EXPECT_EQ(0U, responder.ingress_queued);
    EXPECT_EQ(5U, responder.tx_queued);
    EXPECT_EQ(1U, responder.qps[0].transport.native.read_cache_entries);
    EXPECT_EQ(2U,
              responder.qps[0].transport.native.counters.sequence_naks_received);
    EXPECT_EQ(2U,
              responder.qps[0].transport.native.counters.read_cache_hits);
    EXPECT_EQ(2U,
              responder.qps[0].transport.native.counters.retransmit_events);
    EXPECT_EQ(10U,
              responder.qps[0].transport.native.counters.retransmit_packets);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);

    std::vector<EngineWireFrame> second_replay;
    ASSERT_TRUE(pair.right.drain_tx(5, second_replay));
    ASSERT_EQ(5U, second_replay.size());
    for (size_t batch = 0; batch < 2; ++batch) {
        const std::vector<EngineWireFrame> &frames =
            batch == 0 ? first_replay : second_replay;
        for (size_t index = 0; index < frames.size(); ++index) {
            DecodedPacket response;
            CodecError error;
            ASSERT_TRUE(NativePacketCodec::decode(
                ByteSpan(frames[index].bytes.data(), frames[index].bytes.size()),
                response,
                &error)) << error.message;
            const IbOpcode expected_opcode =
                index == 0 ? IB_RDMA_READ_RESPONSE_FIRST :
                (index + 1 == frames.size() ? IB_RDMA_READ_RESPONSE_LAST :
                                              IB_RDMA_READ_RESPONSE_MIDDLE);
            EXPECT_TRUE(expected_opcode == response.opcode);
            EXPECT_EQ((posted.first_psn + static_cast<uint32_t>(index)) &
                          PSN24_MASK,
                      response.psn);
        }
    }
}

TEST(RoceV2NativeN2, CeMarkedMaximumReadRetainsCnpAndResponseBatch) {
    N2Pair pair;
    pair.max_tx_frames = 5;
    pair.left_ecn = 3;
    pair.configure();

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA;
    read.length = static_cast<uint64_t>(5) * pair.pmtu;
    read.remote_address = N2_RIGHT_VA;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, posted));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    std::vector<EngineWireFrame> requests;
    ASSERT_TRUE(pair.left.drain_tx(1, requests));
    ASSERT_EQ(1U, requests.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(requests[0])));

    Status status;
    ASSERT_TRUE(pair.right.poll(0, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    const EngineSnapshot responder = pair.right.snapshot();
    EXPECT_EQ(5U, responder.tx_queued);
    EXPECT_EQ(1U, responder.qps[0].transport.queued_tx_frames);
    EXPECT_EQ(1U, responder.qps[0].transport.native.counters.cnp_sent);
    EXPECT_EQ(5U,
              responder.qps[0].transport.native.counters.read_response_packets);
    EXPECT_EQ(6U, responder.qps[0].transport.tx_packets);
    EXPECT_TRUE(ErrorCode::OK == responder.error_code);
}

TEST(RoceV2NativeN2, IngressResponseBurstPreservesCrossQpTxFairness) {
    N2Pair pair;
    pair.qps = 2;
    pair.max_tx_frames = 4;
    pair.configure();

    const std::vector<uint8_t> payload = n2_pattern(1, 0xfa17U);
    pair.write_right_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_RIGHT_VA;
    write.length = payload.size();
    write.remote_address = N2_LEFT_VA;
    write.remote_rkey = N2_LEFT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.right.post_write(pair.right_qp(1), write, 0, posted));

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x10000;
    read.length = static_cast<uint64_t>(4) * pair.pmtu;
    read.remote_address = N2_RIGHT_VA + 0x10000;
    read.remote_rkey = N2_RIGHT_RKEY;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(0), read, 0, posted));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    std::vector<EngineWireFrame> request;
    ASSERT_TRUE(pair.left.drain_tx(1, request));
    ASSERT_EQ(1U, request.size());
    ASSERT_TRUE(pair.right.enqueue_ingress(std::move(request[0])));

    Status status;
    ASSERT_TRUE(pair.right.poll(0, poll, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == pair.right.state());
    ASSERT_EQ(4U, poll.frames_produced);
    std::vector<EngineWireFrame> frames;
    ASSERT_TRUE(pair.right.drain_tx(4, frames));
    ASSERT_EQ(4U, frames.size());
    size_t qp1_frames = 0;
    for (size_t index = 0; index < frames.size(); ++index) {
        if (frames[index].source_qp_index == 1) {
            qp1_frames++;
        }
    }
    EXPECT_EQ(1U, qp1_frames);
}

TEST(RoceV2NativeN2, GenerationAndMonotonicClockAreEnforcedAtEveryBoundary) {
    N2Pair pair;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(16, 14);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    Status status;
    const QpId stale(SessionGeneration(N2_GENERATION + 1), 0);
    EXPECT_FALSE(pair.left.post_write(stale, write, 100, posted, &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 100, posted));
    EnginePollResult poll;
    EXPECT_FALSE(pair.left.poll(99, poll, &status));
    EXPECT_TRUE(ErrorCode::INVALID_ARGUMENT == status.code());
    EXPECT_EQ(1U, pair.left.snapshot().counters.monotonic_time_errors);

    EngineWireFrame stale_frame;
    stale_frame.generation = SessionGeneration(N2_GENERATION + 1);
    stale_frame.source_endpoint = N2_RIGHT_ENDPOINT;
    stale_frame.destination_endpoint = N2_LEFT_ENDPOINT;
    stale_frame.bytes.assign(64, 0);
    EXPECT_FALSE(pair.left.enqueue_ingress(stale_frame, &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
    EXPECT_EQ(1U, pair.left.snapshot().counters.ingress_stale_generation);
    EXPECT_FALSE(pair.left.teardown(SessionGeneration(N2_GENERATION + 1), &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
}

TEST(RoceV2NativeN2, IngressAndLinkQueuesApplyBackpressureWithoutTakingOwnership) {
    N2Pair pair;
    pair.max_ingress_frames = 1;
    pair.configure();
    EngineWireFrame first;
    first.generation = SessionGeneration(N2_GENERATION);
    first.source_endpoint = N2_RIGHT_ENDPOINT;
    first.destination_endpoint = N2_LEFT_ENDPOINT;
    first.source_qp_index = 0;
    first.egress_port = N2_RIGHT_PORT;
    first.ingress_port = N2_LEFT_PORT;
    first.sequence = 1;
    first.bytes.assign(64, 0);
    EngineWireFrame second = first;
    second.sequence = 2;
    ASSERT_TRUE(pair.left.enqueue_ingress(first));
    Status status;
    EXPECT_FALSE(pair.left.enqueue_ingress(second, &status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == status.code());
    const EngineSnapshot snapshot = pair.left.snapshot();
    EXPECT_EQ(1U, snapshot.ingress_queued);
    EXPECT_EQ(1U, snapshot.counters.ingress_backpressure);
    EXPECT_EQ(1U, first.sequence);
    EXPECT_EQ(2U, second.sequence);
}

TEST(RoceV2NativeN2, FairSchedulerServicesEachQpBeforeReturningToFirst) {
    N2Pair pair;
    pair.qps = 3;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(32, 15);
    pair.write_left_memory(0, payload);
    for (uint32_t index = 0; index < pair.qps; index++) {
        EngineWriteRequest write;
        write.source_memory_id = 1;
        write.source_address = N2_LEFT_VA;
        write.length = payload.size();
        write.remote_address = N2_RIGHT_VA + index * 0x100;
        write.remote_rkey = N2_RIGHT_RKEY;
        PostedWork posted;
        ASSERT_TRUE(pair.left.post_write(pair.left_qp(index), write, 0, posted));
    }
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(pair.qps, poll.frames_produced);
    std::vector<EngineWireFrame> frames;
    ASSERT_TRUE(pair.left.drain_tx(pair.qps, frames));
    ASSERT_EQ(pair.qps, frames.size());
    std::set<uint32_t> observed;
    for (size_t index = 0; index < frames.size(); index++) {
        observed.insert(frames[index].source_qp_index);
        EXPECT_EQ(index, frames[index].source_qp_index);
    }
    EXPECT_EQ(pair.qps, observed.size());
}

TEST(RoceV2NativeN2, FaultRulesCanBlackholeOneQpWithoutStoppingAnother) {
    N2Pair pair;
    pair.qps = 2;
    pair.retry_count = 1;
    pair.configure();
    LinkFaultRule fault;
    fault.source_endpoint = N2_LEFT_ENDPOINT;
    fault.destination_endpoint = N2_RIGHT_ENDPOINT;
    fault.source_qp_index = 0;
    fault.kind = LinkFaultKind::BLACKHOLE;
    fault.one_shot = false;
    ASSERT_TRUE(pair.link.add_fault(fault));
    const std::vector<uint8_t> zero_payload = n2_pattern(64, 16);
    const std::vector<uint8_t> one_payload = n2_pattern(64, 17);
    pair.write_left_memory(0, zero_payload);
    pair.write_left_memory(0x1000, one_payload);
    EngineWriteRequest zero;
    zero.source_memory_id = 1;
    zero.source_address = N2_LEFT_VA;
    zero.length = zero_payload.size();
    zero.remote_address = N2_RIGHT_VA;
    zero.remote_rkey = N2_RIGHT_RKEY;
    EngineWriteRequest one = zero;
    one.source_address = N2_LEFT_VA + 0x1000;
    one.remote_address = N2_RIGHT_VA + 0x1000;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(0), zero, 0, posted));
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(1), one, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 200000, finish_ns));
    const std::vector<CompletionRecord> qp0 = pair.left_completions(0);
    const std::vector<CompletionRecord> qp1 = pair.left_completions(1);
    ASSERT_EQ(1U, qp0.size());
    EXPECT_TRUE(CompletionState::ERROR == qp0[0].state);
    n2_expect_success(qp1, 1, WorkKind::WRITE);
    EXPECT_TRUE(one_payload == pair.read_right_memory(0x1000, one_payload.size()));
    EXPECT_TRUE(std::vector<uint8_t>(zero_payload.size(), 0) ==
                pair.read_right_memory(0, zero_payload.size()));
}

TEST(RoceV2NativeN2, DuplexOverlappingPsnAckCannotCompleteWrongDirection) {
    N2Pair pair;
    pair.right_start_psn = pair.left_start_psn;
    pair.configure();
    const std::vector<uint8_t> remote = n2_pattern(7000, 0xd00d);
    const std::vector<uint8_t> local = n2_pattern(128, 0xbeef);
    pair.write_right_memory(0x10000, remote);
    pair.write_left_memory(0x20000, local);

    EngineReadRequest read;
    read.destination_memory_id = 1;
    read.destination_address = N2_LEFT_VA + 0x40000;
    read.length = remote.size();
    read.remote_address = N2_RIGHT_VA + 0x10000;
    read.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted_read;
    ASSERT_TRUE(pair.left.post_read(pair.left_qp(), read, 0, posted_read));

    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA + 0x20000;
    write.length = local.size();
    write.remote_address = N2_RIGHT_VA + 0x50000;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted_write;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted_write));

    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 300000, finish_ns));
    const std::vector<CompletionRecord> completions = pair.left_completions();
    ASSERT_EQ(2U, completions.size());
    EXPECT_TRUE(CompletionState::SUCCESS == completions[0].state);
    EXPECT_TRUE(CompletionState::SUCCESS == completions[1].state);
    EXPECT_TRUE(remote == pair.read_left_memory(0x40000, remote.size()));
    EXPECT_TRUE(local == pair.read_right_memory(0x50000, local.size()));
}

TEST(RoceV2NativeN2, ReadCacheAckWinsOverNumericallyStaleSqAck) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x100));
    const std::vector<uint8_t> payload(1, 0xa5);
    NativeTransportWriteRequest write;
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(
        write, ByteSpan(payload.data(), payload.size()), 0, posted, output));
    NativeTransportOutput ack_output;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.terminal_psn, 0x1f),
                                1,
                                ack_output));
    EXPECT_EQ(posted.terminal_psn,
              fixture.transport->snapshot().qp.last_acked_psn);

    /* Remote EPSN starts at 0x800, numerically unrelated to the local SQ ACK.
     * Cache retirement must still resolve by ownership, not serial order. */
    NativeTransportOutput read_output;
    ASSERT_TRUE(fixture.deliver(
        fixture.encode_read_request(0x800,
                                    N2_LEFT_VA,
                                    N2_LEFT_RKEY,
                                    64),
        2,
        read_output));
    ASSERT_EQ(1U, fixture.transport->snapshot().read_cache_entries);
    NativeTransportOutput cache_ack;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(0x800, 0x1f),
                                3,
                                cache_ack));
    EXPECT_EQ(0U, fixture.transport->snapshot().read_cache_entries);
    EXPECT_EQ(posted.terminal_psn,
              fixture.transport->snapshot().qp.last_acked_psn);
}

TEST(RoceV2NativeN2, ReadCacheCapacityReturnsRnrWithoutEviction) {
    NativeMemoryBuffer memory;
    EngineMemoryConfig memory_config;
    memory_config.memory_id = 1;
    memory_config.window.base_address = N2_LEFT_VA;
    memory_config.window.length = 1ULL << 20;
    memory_config.window.rkey = N2_LEFT_RKEY;
    memory_config.window.access = n2_memory_access();
    ASSERT_TRUE(memory.configure(memory_config, 1ULL << 20));
    std::unique_ptr<QueuePair> qp = n2_qp(N2_LEFT_ENDPOINT,
                                          0,
                                          0x1000,
                                          0x2000,
                                          0x100,
                                          0x800,
                                          256,
                                          N2_LEFT_VA,
                                          N2_LEFT_RKEY,
                                          N2_RIGHT_VA,
                                          N2_RIGHT_RKEY,
                                          1ULL << 20);
    NativeRcTransport transport(*qp);
    NativeTransportConfig config = n2_transport_config(true);
    config.max_read_cache_entries = 1;
    config.max_read_cache_bytes = 256;
    ASSERT_TRUE(transport.configure(config));
    ASSERT_TRUE(transport.bind_memory(&memory));
    N2DirectTransport encoder;
    ASSERT_TRUE(encoder.configure());

    NativeTransportOutput first;
    const std::vector<uint8_t> first_request = encoder.encode_read_request(
        0x800, N2_LEFT_VA, N2_LEFT_RKEY, 64);
    DecodedPacket packet;
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(first_request.data(), first_request.size()), packet));
    ASSERT_TRUE(transport.on_frame(packet,
                                   ByteSpan(first_request.data(), first_request.size()),
                                   0,
                                   first));
    ASSERT_EQ(1U, transport.snapshot().read_cache_entries);

    NativeTransportOutput second;
    const std::vector<uint8_t> second_request = encoder.encode_read_request(
        0x801, N2_LEFT_VA + 64, N2_LEFT_RKEY, 64);
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(second_request.data(), second_request.size()), packet));
    ASSERT_TRUE(transport.on_frame(packet,
                                   ByteSpan(second_request.data(), second_request.size()),
                                   1,
                                   second));
    ASSERT_EQ(1U, second.frames.size());
    DecodedPacket rnr;
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(second.frames[0].bytes.data(), second.frames[0].bytes.size()),
        rnr));
    ASSERT_TRUE(rnr.has_aeth);
    EXPECT_EQ(0x20U | qp->snapshot().config.min_rnr_timer,
              rnr.aeth.syndrome);
    EXPECT_EQ(1U, transport.snapshot().read_cache_entries);
    EXPECT_EQ(64U, transport.snapshot().read_cache_bytes);

    NativeTransportOutput replay;
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(first_request.data(), first_request.size()), packet));
    ASSERT_TRUE(transport.on_frame(packet,
                                   ByteSpan(first_request.data(), first_request.size()),
                                   2,
                                   replay));
    EXPECT_FALSE(replay.frames.empty());
    EXPECT_EQ(1U, transport.snapshot().read_cache_entries);
}

TEST(RoceV2NativeN2, AckReqForcesTerminalAckWithPacketOnlyCoalescing) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0, 7, 7, 0, 32, true, 8, 0));
    const std::vector<uint8_t> payload = n2_pattern(32, 0xacc0);
    const std::vector<uint8_t> frame = fixture.encode_write_only(
        0x800, N2_LEFT_VA, N2_LEFT_RKEY, payload, true);
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.deliver(frame, 0, output));
    ASSERT_EQ(1U, output.frames.size());
    DecodedPacket ack;
    ASSERT_TRUE(NativePacketCodec::decode(
        ByteSpan(output.frames[0].bytes.data(), output.frames[0].bytes.size()),
        ack));
    EXPECT_EQ(IB_ACKNOWLEDGE, ack.opcode);
    EXPECT_TRUE(ack.has_aeth);
    EXPECT_LE(ack.aeth.syndrome, 0x1f);
    EXPECT_EQ(0x800U, ack.psn);
}

TEST(RoceV2NativeN2, ReadResponsesRetireInOldestRequestOrder) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x100));
    NativeTransportReadRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    request.local_address = N2_LEFT_VA + 0x1000;
    request.length = 32;
    PostedWork first;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_read(request, 0, first, output));
    output.clear();
    request.remote_address += 0x100;
    request.local_address += 0x100;
    PostedWork second;
    ASSERT_TRUE(fixture.transport->post_read(request, 1, second, output));
    output.clear();

    const std::vector<uint8_t> second_value = n2_pattern(32, 0x2222);
    ASSERT_TRUE(fixture.deliver(
        fixture.encode_read_response(second.first_psn, second_value),
        2,
        output));
    EXPECT_TRUE(output.completions.empty());
    EXPECT_EQ(1U, fixture.transport->snapshot().deferred_read_frames);
    std::vector<uint8_t> observed;
    ASSERT_TRUE(fixture.memory.read(request.local_address,
                                    request.length,
                                    MemoryAccess::LOCAL_READ,
                                    observed));
    EXPECT_TRUE(std::vector<uint8_t>(32, 0) == observed);

    output.clear();
    const std::vector<uint8_t> first_value = n2_pattern(32, 0x1111);
    ASSERT_TRUE(fixture.deliver(
        fixture.encode_read_response(first.first_psn, first_value),
        3,
        output));
    ASSERT_EQ(2U, output.completions.size());
    EXPECT_TRUE(first.id == output.completions[0].id);
    EXPECT_TRUE(second.id == output.completions[1].id);
    EXPECT_EQ(0U, fixture.transport->snapshot().deferred_read_frames);
    ASSERT_TRUE(fixture.memory.read(N2_LEFT_VA + 0x1000,
                                    first_value.size(),
                                    MemoryAccess::LOCAL_READ,
                                    observed));
    EXPECT_TRUE(first_value == observed);
    ASSERT_TRUE(fixture.memory.read(N2_LEFT_VA + 0x1100,
                                    second_value.size(),
                                    MemoryAccess::LOCAL_READ,
                                    observed));
    EXPECT_TRUE(second_value == observed);
}

TEST(RoceV2NativeN2, GoBackNReplayIsIncrementalAcrossBoundedPolls) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x400, 7, 7, 0, 2));
    for (uint32_t index = 0; index < 3; index++) {
        const std::vector<uint8_t> payload = n2_pattern(
            fixture.pmtu + 1, 0x9000 + index);
        NativeTransportWriteRequest request;
        request.remote_address = N2_RIGHT_VA + index * 0x1000;
        request.remote_rkey = N2_RIGHT_RKEY;
        PostedWork posted;
        NativeTransportOutput posted_output;
        ASSERT_TRUE(fixture.transport->post_write(
            request,
            ByteSpan(payload.data(), payload.size()),
            index,
            posted,
            posted_output));
        ASSERT_EQ(2U, posted_output.frames.size());
    }

    NativeTransportOutput output;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(0x400, 0x60),
                                4,
                                output));
    ASSERT_EQ(2U, output.frames.size());
    size_t replayed = output.frames.size();
    for (uint64_t now_ns = 5; replayed < 6; now_ns++) {
        output.clear();
        ASSERT_TRUE(fixture.transport->poll(now_ns, 2, output));
        EXPECT_LE(output.frames.size(), 2U);
        replayed += output.frames.size();
    }
    EXPECT_EQ(6U, replayed);
    EXPECT_EQ(6U, fixture.transport->snapshot().counters.retransmit_packets);
    EXPECT_EQ(1U, fixture.transport->snapshot().counters.retransmit_events);
    EXPECT_EQ(3U, fixture.qp->snapshot().ledger.outstanding_work);
}

TEST(RoceV2NativeN2, HotPathStorageCapacityIsStableAcrossAckAndRetry) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x400, 7, 7, 0, 4));
    const NativeTransportSnapshot admitted = fixture.transport->snapshot();
    ASSERT_GT(admitted.frame_storage.capacity, 0U);
    ASSERT_GT(admitted.retained_frame_capacity, 0U);
    ASSERT_EQ(0U, admitted.frame_storage.in_use);
    ASSERT_EQ(0U, admitted.retained_frames);

    const std::vector<uint8_t> payload = n2_pattern(
        fixture.pmtu + 17, 0xa110ca7eU);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput publication;
    ASSERT_TRUE(fixture.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        0,
        posted,
        publication));
    ASSERT_EQ(2U, publication.frames.size());
    const uint32_t first_slot = publication.frames[0].bytes.slot_id();
    const uint32_t second_slot = publication.frames[1].bytes.slot_id();
    ASSERT_NE(first_slot, second_slot);

    publication.clear();
    NativeTransportOutput replay;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x60),
                                1,
                                replay));
    ASSERT_EQ(2U, replay.frames.size());
    EXPECT_EQ(first_slot, replay.frames[0].bytes.slot_id());
    EXPECT_EQ(second_slot, replay.frames[1].bytes.slot_id());
    replay.clear();

    NativeTransportOutput ack;
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.terminal_psn, 0x1f),
                                2,
                                ack));
    const NativeTransportSnapshot retired = fixture.transport->snapshot();
    EXPECT_EQ(admitted.frame_storage.capacity,
              retired.frame_storage.capacity);
    EXPECT_EQ(admitted.frame_storage.stride, retired.frame_storage.stride);
    EXPECT_EQ(admitted.retained_frame_capacity,
              retired.retained_frame_capacity);
    EXPECT_EQ(0U, retired.frame_storage.in_use);
    EXPECT_EQ(0U, retired.retained_frames);
    EXPECT_EQ(0U, retired.hot_path_growth_events);
}

TEST(RoceV2NativeN2, EngineIngressAndTxRingsRemainFixedCapacity) {
    N2Pair pair;
    pair.max_ingress_frames = 8;
    pair.max_tx_frames = 8;
    pair.configure();
    const EngineSnapshot admitted = pair.left.snapshot();
    ASSERT_EQ(8U, admitted.ingress_queue_capacity);
    ASSERT_EQ(8U, admitted.tx_queue_capacity);
    ASSERT_EQ(8U, admitted.ingress_storage.capacity);

    const std::vector<uint8_t> payload = n2_pattern(257, 0x51ab1eU);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    uint64_t finish_ns = 0;
    ASSERT_TRUE(pair.pump(0, 100000, finish_ns));

    const EngineSnapshot retired = pair.left.snapshot();
    EXPECT_EQ(admitted.ingress_queue_capacity,
              retired.ingress_queue_capacity);
    EXPECT_EQ(admitted.tx_queue_capacity, retired.tx_queue_capacity);
    EXPECT_EQ(admitted.ingress_storage.capacity,
              retired.ingress_storage.capacity);
    EXPECT_EQ(admitted.ingress_storage.stride,
              retired.ingress_storage.stride);
    EXPECT_EQ(0U, retired.ingress_storage.in_use);
    EXPECT_EQ(0U, retired.hot_path_growth_events);
}

TEST(RoceV2NativeN2, CnpRateScaleGatesPublicationUntilMonotonicSendTime) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x500));
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.deliver(fixture.encode_cnp(), 0, output));
    EXPECT_DOUBLE_EQ(0.5, fixture.transport->snapshot().rate_scale);
    const uint64_t gate = fixture.transport->snapshot().next_send_ns;
    ASSERT_GT(gate, 0U);

    const std::vector<uint8_t> payload = n2_pattern(16, 0xc0de);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    output.clear();
    ASSERT_TRUE(fixture.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        1,
        posted,
        output));
    EXPECT_TRUE(output.frames.empty());
    EXPECT_EQ(1U, fixture.transport->snapshot().queued_tx_frames);
    ASSERT_TRUE(fixture.transport->poll(gate - 1, 8, output));
    EXPECT_TRUE(output.frames.empty());
    ASSERT_TRUE(fixture.transport->poll(gate, 8, output));
    ASSERT_EQ(1U, output.frames.size());
    EXPECT_EQ(posted.first_psn, output.frames[0].psn);
    output.clear();
    ASSERT_TRUE(fixture.transport->poll(1000000, 8, output));
    EXPECT_DOUBLE_EQ(1.0, fixture.transport->snapshot().rate_scale);
}

TEST(RoceV2NativeN2, SuccessfulUnsignalledWorkRetiresWithoutCqEntries) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x600));
    PostedWork posted[3];
    for (uint32_t index = 0; index < 3; index++) {
        const std::vector<uint8_t> payload(1, static_cast<uint8_t>(index));
        NativeTransportWriteRequest request;
        request.remote_address = N2_RIGHT_VA + index;
        request.remote_rkey = N2_RIGHT_RKEY;
        request.signalled = index == 2;
        NativeTransportOutput post_output;
        ASSERT_TRUE(fixture.transport->post_write(
            request,
            ByteSpan(payload.data(), payload.size()),
            index,
            posted[index],
            post_output));
    }
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.deliver(
        fixture.encode_aeth(posted[2].terminal_psn, 0x1f),
        4,
        output));
    ASSERT_EQ(1U, output.completions.size());
    EXPECT_TRUE(posted[2].id == output.completions[0].id);
    const CompletionLedgerSnapshot ledger = fixture.qp->snapshot().ledger;
    EXPECT_EQ(0U, ledger.outstanding_work);
    EXPECT_EQ(0U, ledger.ready_completions);
    EXPECT_EQ(3U, ledger.counters.writes_completed);
}

TEST(RoceV2NativeN2, UnsignalledFailuresAndFlushesStillPublishCqEvidence) {
    N2DirectTransport failed;
    ASSERT_TRUE(failed.configure(0x680));
    const std::vector<uint8_t> payload(1, 0x68);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    request.signalled = false;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(failed.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        0,
        posted,
        output));
    output.clear();
    ASSERT_TRUE(failed.deliver(failed.encode_aeth(posted.first_psn, 0x62),
                               1,
                               output));
    ASSERT_EQ(1U, output.completions.size());
    EXPECT_TRUE(posted.id == output.completions[0].id);
    EXPECT_TRUE(CompletionState::ERROR == output.completions[0].state);
    EXPECT_TRUE(ErrorCode::REMOTE_ACCESS_ERROR == output.completions[0].error);

    N2DirectTransport flushed;
    ASSERT_TRUE(flushed.configure(0x690));
    PostedWork first;
    PostedWork second;
    output.clear();
    ASSERT_TRUE(flushed.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        0,
        first,
        output));
    output.clear();
    ASSERT_TRUE(flushed.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        1,
        second,
        output));
    flushed.transport->abort(ErrorCode::ABORTED, "test flush");
    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(flushed.transport->drain_completions(completions));
    ASSERT_EQ(2U, completions.size());
    EXPECT_TRUE(second.id == completions[0].id);
    EXPECT_TRUE(first.id == completions[1].id);
    EXPECT_TRUE(CompletionState::FLUSHED == completions[0].state);
    EXPECT_TRUE(CompletionState::FLUSHED == completions[1].state);
    /* Release slab handles before their owning transport fixture. */
    output.clear();
}

TEST(RoceV2NativeN2, RetryTimerArmsOnlyAfterAdapterAcceptance) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x700, 1, 1, 0));
    const std::vector<uint8_t> payload(1, 0x5a);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(
        request,
        ByteSpan(payload.data(), payload.size()),
        0,
        posted,
        output));
    EXPECT_EQ(0U, fixture.transport->snapshot().oldest_deadline_ns);
    output.clear();
    ASSERT_TRUE(fixture.transport->poll(1000000, 8, output));
    EXPECT_TRUE(output.frames.empty());
    ASSERT_TRUE(fixture.transport->on_frame_accepted(posted.terminal_psn,
                                                     1000001));
    const uint64_t deadline = fixture.transport->snapshot().oldest_deadline_ns;
    EXPECT_GT(deadline, 1000001U);
    ASSERT_TRUE(fixture.transport->poll(deadline, 8, output));
    ASSERT_EQ(1U, output.frames.size());
    EXPECT_TRUE(output.frames[0].retransmission);
}


TEST(RoceV2NativeN2, AllOnesRnrRetryBudgetRemainsUnlimited) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x600, 1, 7));
    const std::vector<uint8_t> payload = n2_pattern(8, 7);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    request.with_immediate = true;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0, posted, output));
    uint64_t now_ns = 100;
    for (uint32_t attempt = 0; attempt < 12; attempt++) {
        output.clear();
        ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x21),
                                    now_ns, output));
        ASSERT_TRUE(output.completions.empty());
        now_ns += 1000000000ULL;
        output.clear();
        ASSERT_TRUE(fixture.transport->poll(now_ns, 8, output));
        ASSERT_TRUE(output.completions.empty());
        ASSERT_TRUE(QpState::RTS == fixture.qp->state());
    }
    EXPECT_EQ(12U, fixture.transport->snapshot().counters.rnr_naks_received);
    EXPECT_EQ(12U, fixture.transport->snapshot().counters.rnr_retries);
    EXPECT_EQ(0U, fixture.transport->snapshot().counters.rnr_retry_exhausted);
    output.clear();
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.terminal_psn, 0),
                                now_ns + 1, output));
    n2_expect_success(output.completions, 1, WorkKind::WRITE);
}

TEST(RoceV2NativeN2, AllOnesSequenceRetryBudgetRemainsUnlimited) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x500, 7, 1));
    const std::vector<uint8_t> payload = n2_pattern(2 * fixture.pmtu, 6);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0, posted, output));
    for (uint32_t attempt = 0; attempt < 12; attempt++) {
        output.clear();
        ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.first_psn, 0x60),
                                    attempt + 1, output));
        ASSERT_TRUE(output.completions.empty());
        ASSERT_TRUE(QpState::RTS == fixture.qp->state());
    }
    EXPECT_EQ(12U, fixture.transport->snapshot().counters.nak_retries);
    EXPECT_EQ(0U, fixture.transport->snapshot().counters.retry_exhausted);
    output.clear();
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.terminal_psn, 0),
                                13, output));
    n2_expect_success(output.completions, 1, WorkKind::WRITE);
}

TEST(RoceV2NativeN2, AllOnesTimeoutRetryBudgetRemainsUnlimited) {
    N2DirectTransport fixture;
    ASSERT_TRUE(fixture.configure(0x500, 7, 1));
    const std::vector<uint8_t> payload = n2_pattern(8, 6);
    NativeTransportWriteRequest request;
    request.remote_address = N2_RIGHT_VA;
    request.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    NativeTransportOutput output;
    ASSERT_TRUE(fixture.transport->post_write(request,
                                               ByteSpan(payload.data(), payload.size()),
                                               0, posted, output));
    uint64_t now_ns = 1;
    ASSERT_TRUE(fixture.transport->on_frame_accepted(posted.terminal_psn, now_ns));
    for (uint32_t attempt = 0; attempt < 12; attempt++) {
        now_ns = fixture.transport->snapshot().oldest_deadline_ns;
        ASSERT_GT(now_ns, 0U);
        output.clear();
        ASSERT_TRUE(fixture.transport->poll(now_ns, 8, output));
        ASSERT_EQ(1U, output.frames.size());
        ASSERT_TRUE(output.frames[0].retransmission);
        ASSERT_TRUE(output.completions.empty());
        ASSERT_TRUE(QpState::RTS == fixture.qp->state());
        ASSERT_TRUE(fixture.transport->on_frame_accepted(posted.terminal_psn, now_ns));
    }
    EXPECT_EQ(12U, fixture.transport->snapshot().counters.timeout_retries);
    EXPECT_EQ(0U, fixture.transport->snapshot().counters.retry_exhausted);
    output.clear();
    ASSERT_TRUE(fixture.deliver(fixture.encode_aeth(posted.terminal_psn, 0),
                                now_ns + 1, output));
    n2_expect_success(output.completions, 1, WorkKind::WRITE);
}

TEST(RoceV2NativeN2, BurstPeekRetainsUnacceptedSuffixAndRequiresOrderedAcceptance) {
    N2Pair pair;
    pair.max_tx_frames = 4;
    pair.retry_count = 0;
    pair.configure();
    const std::vector<uint8_t> payload = n2_pattern(3 * pair.pmtu, 12);
    pair.write_left_memory(0, payload);
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = N2_LEFT_VA;
    write.length = payload.size();
    write.remote_address = N2_RIGHT_VA;
    write.remote_rkey = N2_RIGHT_RKEY;
    PostedWork posted;
    ASSERT_TRUE(pair.left.post_write(pair.left_qp(), write, 0, posted));
    EnginePollResult poll;
    ASSERT_TRUE(pair.left.poll(0, poll));
    ASSERT_EQ(3U, pair.left.snapshot().tx_queued);
    uint64_t sequences[3];
    for (size_t index = 0; index < 3; index++) {
        ASSERT_TRUE(pair.left.peek_tx_at(index) != NULL);
        sequences[index] = pair.left.peek_tx_at(index)->sequence;
    }
    EXPECT_TRUE(pair.left.peek_tx_at(3) == NULL);
    EXPECT_TRUE(pair.left.peek_tx_at(std::numeric_limits<size_t>::max()) == NULL);
    EXPECT_EQ(0U, pair.left.snapshot().qps[0].transport.native.oldest_deadline_ns);
    Status status;
    EXPECT_FALSE(pair.left.accept_tx(sequences[1], 1, &status));
    EXPECT_TRUE(ErrorCode::INVALID_ARGUMENT == status.code());
    EXPECT_EQ(3U, pair.left.snapshot().tx_queued);
    ASSERT_TRUE(pair.left.accept_tx(sequences[0], 1));
    EXPECT_EQ(2U, pair.left.snapshot().tx_queued);
    ASSERT_TRUE(pair.left.peek_tx_at(0) != NULL);
    EXPECT_EQ(sequences[1], pair.left.peek_tx_at(0)->sequence);
    EXPECT_EQ(sequences[2], pair.left.peek_tx_at(1)->sequence);
    /* A partial device burst must keep its suffix alive despite a timer jump. */
    ASSERT_TRUE(pair.left.poll(1000000000ULL, poll));
    EXPECT_TRUE(QpState::RTS == pair.left.queue_pair(pair.left_qp())->state());
    EXPECT_EQ(sequences[1], pair.left.peek_tx()->sequence);
    ASSERT_TRUE(pair.left.accept_tx(sequences[1], 1000000001ULL));
    ASSERT_TRUE(pair.left.accept_tx(sequences[2], 1000000001ULL));
    EXPECT_TRUE(pair.left.peek_tx_at(0) == NULL);
    EXPECT_EQ(3U, pair.left.snapshot().counters.tx_drained);
}

TEST(RoceV2NativeN2, BurstPeekHandlesWrappedRingAndOutOfRangeIndices) {
    NativeBoundedRing<uint32_t> ring;
    ASSERT_TRUE(ring.configure(3));
    ASSERT_TRUE(ring.push(10));
    ASSERT_TRUE(ring.push(20));
    ring.pop();
    ASSERT_TRUE(ring.push(30));
    ASSERT_TRUE(ring.push(40));
    ASSERT_TRUE(ring.at(0) != NULL);
    EXPECT_EQ(20U, *ring.at(0));
    EXPECT_EQ(30U, *ring.at(1));
    EXPECT_EQ(40U, *ring.at(2));
    EXPECT_TRUE(ring.at(3) == NULL);
    EXPECT_TRUE(ring.at(std::numeric_limits<size_t>::max()) == NULL);
    ring.clear();
    EXPECT_TRUE(ring.at(0) == NULL);
}

} // namespace rocev2_native_n2_tests
