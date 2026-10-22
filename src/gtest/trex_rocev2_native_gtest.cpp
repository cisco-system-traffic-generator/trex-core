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

#include <common/gtest.h>

#include "stx/stl/trex_rocev2_native_codec.h"
#include "stx/stl/trex_rocev2_native_engine.h"
#include "stx/stl/trex_rocev2_native_qp.h"
#include "stx/stl/trex_rocev2_native_types.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

uint32_t trex_roce_v2_calc_icrc_bytes(const uint8_t *pkt,
                                      uint32_t pkt_len,
                                      uint16_t l2_len,
                                      uint16_t l3_len,
                                      uint16_t bth_offset);

using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;

/* Native transport tests are kept separate from the phase-1 workload-builder
 * tests so a packet-rate state-machine regression cannot be hidden by a
 * stateless builder pass. */

namespace {

void write_be16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 1] = static_cast<uint8_t>(value);
}

void write_le32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<uint8_t>(value >> 24);
}

uint16_t read_be16(const std::vector<uint8_t> &bytes, size_t offset) {
    return static_cast<uint16_t>((static_cast<uint32_t>(bytes[offset]) << 8) |
                                 bytes[offset + 1]);
}

NativeEndpoint codec_endpoint(bool ipv6 = false, bool vlan = false) {
    NativeEndpoint endpoint;
    const uint8_t src_mac[6] = {0x02, 0, 0, 0, 0, 1};
    const uint8_t dst_mac[6] = {0x02, 0, 0, 0, 0, 2};
    const uint8_t src_ipv4[4] = {192, 0, 2, 1};
    const uint8_t dst_ipv4[4] = {198, 51, 100, 2};
    const uint8_t src_ipv6[16] = {
        0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 1
    };
    const uint8_t dst_ipv6[16] = {
        0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 2
    };
    memcpy(endpoint.src_mac, src_mac, sizeof(src_mac));
    memcpy(endpoint.dst_mac, dst_mac, sizeof(dst_mac));
    endpoint.ipv6 = ipv6;
    if (ipv6) {
        memcpy(endpoint.src_ip, src_ipv6, sizeof(src_ipv6));
        memcpy(endpoint.dst_ip, dst_ipv6, sizeof(dst_ipv6));
    } else {
        memcpy(endpoint.src_ip, src_ipv4, sizeof(src_ipv4));
        memcpy(endpoint.dst_ip, dst_ipv4, sizeof(dst_ipv4));
    }
    endpoint.has_vlan = vlan;
    endpoint.vlan = vlan ? 4094 : 0;
    endpoint.pcp = vlan ? 7 : 0;
    endpoint.vlan_dei = vlan;
    endpoint.dscp = 26;
    endpoint.ecn = 3;
    endpoint.ttl = 63;
    endpoint.udp_sport = 49153;
    endpoint.udp_dport = ROCEV2_UDP_PORT;
    return endpoint;
}

PacketEncodeSpec codec_packet_spec(IbOpcode opcode,
                                   const std::vector<uint8_t> &payload) {
    PacketEncodeSpec spec;
    spec.endpoint = codec_endpoint();
    spec.opcode = opcode;
    spec.pkey = 0xbeef;
    spec.dest_qpn = 0x010203;
    spec.psn = 0xfefdfc;
    spec.solicited = true;
    spec.migration_request = true;
    spec.ack_request = true;
    spec.fecn = true;
    spec.becn = true;
    spec.has_reth = false;
    spec.has_aeth = false;
    spec.has_immediate_data = false;
    spec.has_deth = false;
    spec.payload = ByteSpan(payload.empty() ? NULL : &payload[0], payload.size());
    spec.ipv4_identification = 0x1234;
    spec.ipv4_dont_fragment = true;

    switch (opcode) {
    case IB_RDMA_WRITE_FIRST:
    case IB_RDMA_WRITE_ONLY:
    case IB_RDMA_READ_REQUEST:
        spec.has_reth = true;
        break;
    case IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE:
        spec.has_reth = true;
        spec.has_immediate_data = true;
        break;
    case IB_RDMA_WRITE_LAST_WITH_IMMEDIATE:
        spec.has_immediate_data = true;
        break;
    case IB_RDMA_READ_RESPONSE_FIRST:
    case IB_RDMA_READ_RESPONSE_LAST:
    case IB_RDMA_READ_RESPONSE_ONLY:
    case IB_ACKNOWLEDGE:
        spec.has_aeth = true;
        break;
    case IB_UD_SEND_ONLY:
        spec.has_deth = true;
        break;
    case IB_SEND_FIRST:
    case IB_SEND_MIDDLE:
    case IB_SEND_LAST:
    case IB_SEND_ONLY:
        break;
    case IB_SEND_LAST_WITH_IMMEDIATE:
    case IB_SEND_ONLY_WITH_IMMEDIATE:
        spec.has_immediate_data = true;
        break;
    case IB_SEND_LAST_WITH_INVALIDATE:
    case IB_SEND_ONLY_WITH_INVALIDATE:
        spec.has_invalidate = true;
        spec.invalidate_rkey = 0x1234;
        break;
    case IB_COMPARE_SWAP:
    case IB_FETCH_ADD:
        spec.has_atomic_eth = true;
        break;
    case IB_ATOMIC_ACKNOWLEDGE:
        spec.has_aeth = true;
        spec.has_atomic_ack = true;
        break;
    case IB_RDMA_WRITE_MIDDLE:
    case IB_RDMA_WRITE_LAST:
    case IB_RDMA_READ_RESPONSE_MIDDLE:
    case IB_CNP:
        break;
    }
    spec.reth.virtual_address = 0x0102030405060708ULL;
    spec.reth.rkey = 0x89abcdef;
    spec.reth.dma_length = 0x10203040;
    spec.aeth.syndrome = 0x60;
    spec.aeth.msn = 0x0a0b0c;
    spec.immediate_data = 0xa1b2c3d4;
    spec.deth.qkey = NATIVE_GSI_QKEY;
    spec.deth.source_qpn = 0x040506;
    return spec;
}

std::vector<uint8_t> encode_codec_packet(const PacketEncodeSpec &spec) {
    std::vector<uint8_t> frame;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::encode(spec, frame, &error))
        << codec_error_code_to_string(error.code) << ": " << error.message;
    return frame;
}

DecodedPacket decode_codec_packet(const std::vector<uint8_t> &frame) {
    DecodedPacket decoded;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()),
                                          decoded,
                                          &error))
        << codec_error_code_to_string(error.code) << ": " << error.message;
    return decoded;
}

void expect_decode_error(const std::vector<uint8_t> &frame,
                         CodecErrorCode expected,
                         const DecodeOptions *options = NULL) {
    DecodedPacket decoded;
    CodecError error;
    bool accepted = options == NULL
        ? NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()),
                                    decoded,
                                    &error)
        : NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()),
                                    *options,
                                    decoded,
                                    &error);
    EXPECT_FALSE(accepted);
    EXPECT_EQ(static_cast<int>(expected), static_cast<int>(error.code))
        << error.message;
}

MemoryWindowConfig local_window_config() {
    MemoryWindowConfig config;
    config.base_address = 0x10000000ULL;
    config.length = 0x2000;
    config.rkey = 0x10203040;
    config.access = MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
                    MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
    return config;
}

QpConfig local_qp_config(uint32_t starting_psn = 0xfffffe,
                         uint32_t message_depth = 8,
                         uint32_t packet_depth = 16) {
    QpConfig config;
    config.local_qpn = 0x010203;
    config.starting_sq_psn = starting_psn;
    config.pmtu = 1024;
    config.sq_depth_messages = message_depth;
    config.sq_depth_packets = packet_depth;
    return config;
}

QpPeerConfig peer_qp_config(uint32_t starting_psn = 0xfffffe) {
    QpPeerConfig peer;
    peer.remote_qpn = 0x040506;
    peer.starting_psn = starting_psn;
    peer.pmtu = 512;
    peer.rkey = 0xa1b2c3d4;
    peer.virtual_address = 0x20000000ULL;
    peer.window_size = 0x4000;
    peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
    return peer;
}

bool establish_qp(QueuePair &qp,
                  Status *status,
                  uint32_t starting_sq_psn = 0xfffffe,
                  uint32_t expected_rx_psn = 0xfffffe,
                  uint32_t message_depth = 8,
                  uint32_t packet_depth = 16) {
    const QpConfig local = local_qp_config(starting_sq_psn,
                                           message_depth,
                                           packet_depth);
    const QpPeerConfig peer = peer_qp_config(expected_rx_psn);
    const MemoryWindowConfig window = local_window_config();
    return qp.create(local, window, status) &&
           qp.modify_to_rtr(peer, status) &&
           qp.modify_to_rts(status);
}

WorkDescriptor write_work(const QpId &owner,
                          uint64_t sequence,
                          uint32_t first_psn,
                          uint32_t packet_count,
                          uint64_t bytes = 64) {
    WorkDescriptor work;
    work.id = WorkId(owner, sequence);
    work.kind = WorkKind::WRITE;
    work.first_psn = first_psn;
    work.reserved_psns = packet_count;
    work.payload_bytes = bytes;
    Status status;
    const bool added = psn24_add(first_psn,
                                 static_cast<int64_t>(packet_count) - 1,
                                 work.terminal_psn,
                                 &status);
    EXPECT_TRUE(added) << status.message();
    return work;
}

WorkDescriptor read_work(const QpId &owner,
                         uint64_t sequence,
                         uint32_t request_psn,
                         uint64_t bytes,
                         uint32_t response_psns) {
    WorkDescriptor work;
    work.id = WorkId(owner, sequence);
    work.kind = WorkKind::READ;
    work.first_psn = request_psn;
    Status status;
    const bool added = psn24_add(request_psn,
                                 static_cast<int64_t>(response_psns) - 1,
                                 work.terminal_psn,
                                 &status);
    EXPECT_TRUE(added) << status.message();
    work.reserved_psns = response_psns;
    work.payload_bytes = bytes;
    work.expected_read_bytes = bytes;
    work.expected_read_psns = response_psns;
    return work;
}

} // namespace

TEST(rocev2_native, psn_serial_arithmetic_wraps_without_losing_order) {
    Status status;
    uint32_t value = 0;
    ASSERT_TRUE(psn24_add(0xfffffe, 3, value, &status));
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(1U, value);

    uint32_t distance = 0;
    ASSERT_TRUE(psn24_forward_distance(0xfffffe, 1, distance, &status));
    EXPECT_EQ(3U, distance);
    EXPECT_EQ(static_cast<int>(PsnOrder::AFTER),
              static_cast<int>(psn24_compare(1, 0xfffffe)));
    EXPECT_EQ(static_cast<int>(PsnOrder::BEFORE),
              static_cast<int>(psn24_compare(0xfffffe, 1)));
    EXPECT_EQ(static_cast<int>(PsnOrder::EQUAL),
              static_cast<int>(psn24_compare(7, 7)));
}

TEST(rocev2_native, psn_serial_arithmetic_rejects_ambiguous_ranges) {
    EXPECT_EQ(static_cast<int>(PsnOrder::AMBIGUOUS),
              static_cast<int>(psn24_compare(0, PSN24_HALF_SPACE)));
    EXPECT_EQ(static_cast<int>(PsnOrder::INVALID),
              static_cast<int>(psn24_compare(PSN24_SPACE, 0)));

    Status status;
    bool contains = true;
    EXPECT_FALSE(psn24_in_closed_interval(1,
                                          0,
                                          PSN24_HALF_SPACE,
                                          contains,
                                          &status));
    EXPECT_FALSE(contains);
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_ARGUMENT),
              static_cast<int>(status.code()));
}

TEST(rocev2_native, generation_qualified_work_ids_reject_aba_reuse) {
    QpId first(SessionGeneration(41), 9);
    QpId replacement(SessionGeneration(42), 9);
    WorkId work(first, 1);

    ASSERT_TRUE(first.valid());
    ASSERT_TRUE(replacement.valid());
    ASSERT_TRUE(work.valid());
    EXPECT_TRUE(work.belongs_to(first));
    EXPECT_FALSE(work.belongs_to(replacement));
    EXPECT_EQ("41:9:1", work.to_string());
}

TEST(rocev2_native, memory_window_enforces_access_rkey_and_overflow_safe_bounds) {
    MemoryWindow window;
    MemoryWindowConfig config;
    config.base_address = 0xfffffffffffffff0ULL;
    config.length = 16;
    config.rkey = 0x12345678;
    config.access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;

    Status status;
    ASSERT_TRUE(window.configure(config, &status)) << status.message();
    const MemoryWindowSnapshot snapshot = window.snapshot();
    EXPECT_TRUE(snapshot.configured);
    EXPECT_TRUE(snapshot.active);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), snapshot.last_address);

    uint64_t offset = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(window.validate(config.rkey,
                                config.base_address,
                                config.length,
                                MemoryAccess::REMOTE_WRITE,
                                &offset,
                                &status)) << status.message();
    EXPECT_EQ(0U, offset);
    EXPECT_TRUE(window.validate(config.rkey,
                                std::numeric_limits<uint64_t>::max(),
                                1,
                                MemoryAccess::REMOTE_READ,
                                &offset,
                                &status)) << status.message();
    EXPECT_EQ(15U, offset);

    EXPECT_FALSE(window.validate(config.rkey + 1,
                                 config.base_address,
                                 1,
                                 MemoryAccess::REMOTE_READ,
                                 NULL,
                                 &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::ACCESS_DENIED),
              static_cast<int>(status.code()));
    EXPECT_FALSE(window.validate(config.rkey,
                                 config.base_address,
                                 1,
                                 MemoryAccess::LOCAL_WRITE,
                                 NULL,
                                 &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::ACCESS_DENIED),
              static_cast<int>(status.code()));
    EXPECT_FALSE(window.validate(config.rkey,
                                 config.base_address - 1,
                                 1,
                                 MemoryAccess::REMOTE_READ,
                                 NULL,
                                 &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::OUT_OF_RANGE),
              static_cast<int>(status.code()));
    EXPECT_FALSE(window.validate(config.rkey,
                                 std::numeric_limits<uint64_t>::max(),
                                 2,
                                 MemoryAccess::REMOTE_READ,
                                 NULL,
                                 &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::OUT_OF_RANGE),
              static_cast<int>(status.code()));

    window.deactivate();
    EXPECT_FALSE(window.validate(config.rkey,
                                 config.base_address,
                                 1,
                                 MemoryAccess::REMOTE_READ,
                                 NULL,
                                 &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_STATE),
              static_cast<int>(status.code()));

    MemoryWindow invalid;
    config.base_address = std::numeric_limits<uint64_t>::max();
    config.length = 2;
    EXPECT_FALSE(invalid.configure(config, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::OUT_OF_RANGE),
              static_cast<int>(status.code()));
    config.base_address = 0;
    config.length = 0;
    EXPECT_FALSE(invalid.configure(config, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_ARGUMENT),
              static_cast<int>(status.code()));
    config.length = 1;
    config.rkey = 0;
    EXPECT_FALSE(invalid.configure(config, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_ARGUMENT),
              static_cast<int>(status.code()));
}

TEST(rocev2_native, qp_accepts_every_legal_state_transition_and_rejects_illegal_ones) {
    const QpId id(SessionGeneration(7), 3);
    QueuePair qp(id);
    const QpConfig local = local_qp_config();
    const QpPeerConfig peer = peer_qp_config();
    const MemoryWindowConfig window = local_window_config();
    Status status;

    EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    EXPECT_FALSE(qp.modify_to_rtr(peer, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_STATE),
              static_cast<int>(status.code()));
    EXPECT_FALSE(qp.modify_to_rts(&status));
    EXPECT_FALSE(qp.enter_error(ErrorCode::PROTOCOL_ERROR, "reset", &status));

    ASSERT_TRUE(qp.create(local, window, &status)) << status.message();
    EXPECT_EQ(static_cast<int>(QpState::INIT), static_cast<int>(qp.state()));
    EXPECT_FALSE(qp.create(local, window, &status));
    EXPECT_FALSE(qp.modify_to_rts(&status));
    EXPECT_FALSE(qp.enter_error(ErrorCode::PROTOCOL_ERROR, "init", &status));

    ASSERT_TRUE(qp.modify_to_rtr(peer, &status)) << status.message();
    EXPECT_EQ(static_cast<int>(QpState::RTR), static_cast<int>(qp.state()));
    EXPECT_FALSE(qp.create(local, window, &status));
    EXPECT_FALSE(qp.modify_to_rtr(peer, &status));
    // RTR is a live responder state: receive-side failures may enter ERR.
    QueuePair passive(QpId(SessionGeneration(7), 4));
    ASSERT_TRUE(passive.create(local, window, &status));
    ASSERT_TRUE(passive.modify_to_rtr(peer, &status));
    EXPECT_TRUE(passive.enter_error(ErrorCode::PROTOCOL_ERROR, "rtr", &status));
    EXPECT_EQ(static_cast<int>(QpState::ERR), static_cast<int>(passive.state()));

    ASSERT_TRUE(qp.modify_to_rts(&status)) << status.message();
    EXPECT_EQ(static_cast<int>(QpState::RTS), static_cast<int>(qp.state()));
    EXPECT_FALSE(qp.create(local, window, &status));
    EXPECT_FALSE(qp.modify_to_rtr(peer, &status));
    EXPECT_FALSE(qp.modify_to_rts(&status));

    ASSERT_TRUE(qp.enter_error(ErrorCode::PROTOCOL_ERROR, "fatal", &status))
        << status.message();
    EXPECT_EQ(static_cast<int>(QpState::ERR), static_cast<int>(qp.state()));
    EXPECT_FALSE(qp.create(local, window, &status));
    EXPECT_FALSE(qp.modify_to_rtr(peer, &status));
    EXPECT_FALSE(qp.modify_to_rts(&status));
    EXPECT_FALSE(qp.enter_error(ErrorCode::PROTOCOL_ERROR, "again", &status));

    ASSERT_TRUE(qp.destroy("test teardown", &status)) << status.message();
    EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    EXPECT_EQ(5U, qp.snapshot().counters.state_transitions);
    EXPECT_EQ(15U, qp.snapshot().counters.invalid_transitions);
}

TEST(rocev2_native, qp_destroy_is_idempotent_and_legal_from_every_state) {
    Status status;
    {
        QueuePair qp(QpId(SessionGeneration(8), 0));
        EXPECT_TRUE(qp.destroy("already reset", &status));
        EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    }
    {
        QueuePair qp(QpId(SessionGeneration(8), 1));
        ASSERT_TRUE(qp.create(local_qp_config(), local_window_config(), &status));
        EXPECT_TRUE(qp.destroy("init", &status));
        EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    }
    {
        QueuePair qp(QpId(SessionGeneration(8), 2));
        ASSERT_TRUE(qp.create(local_qp_config(), local_window_config(), &status));
        ASSERT_TRUE(qp.modify_to_rtr(peer_qp_config(), &status));
        EXPECT_TRUE(qp.destroy("rtr", &status));
        EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    }
    {
        QueuePair qp(QpId(SessionGeneration(8), 3));
        ASSERT_TRUE(establish_qp(qp, &status));
        EXPECT_TRUE(qp.destroy("rts", &status));
        EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    }
    {
        QueuePair qp(QpId(SessionGeneration(8), 4));
        ASSERT_TRUE(establish_qp(qp, &status));
        ASSERT_TRUE(qp.enter_error(ErrorCode::PROTOCOL_ERROR, "err", &status));
        EXPECT_TRUE(qp.destroy("err", &status));
        EXPECT_EQ(static_cast<int>(QpState::RESET), static_cast<int>(qp.state()));
    }
}

TEST(rocev2_native, completion_ledger_bounds_work_and_packet_queues) {
    const QpId owner(SessionGeneration(9), 1);
    CompletionLedger ledger;
    Status status;
    ASSERT_TRUE(ledger.configure(owner, 10, 2, 3, &status)) << status.message();

    const WorkDescriptor first = write_work(owner, 1, 10, 2);
    const WorkDescriptor second = write_work(owner, 2, 12, 1);
    ASSERT_TRUE(ledger.post(first, &status)) << status.message();
    ASSERT_TRUE(ledger.post(second, &status)) << status.message();
    EXPECT_EQ(2U, ledger.snapshot().outstanding_work);
    EXPECT_EQ(3U, ledger.snapshot().outstanding_packets);

    const WorkDescriptor third = write_work(owner, 3, 13, 1);
    EXPECT_FALSE(ledger.post(third, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::RESOURCE_EXHAUSTED),
              static_cast<int>(status.code()));

    ASSERT_TRUE(ledger.acknowledge(first.terminal_psn, &status)) << status.message();
    EXPECT_EQ(1U, ledger.snapshot().outstanding_work);
    EXPECT_EQ(1U, ledger.snapshot().ready_completions);
    EXPECT_FALSE(ledger.post(third, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::RESOURCE_EXHAUSTED),
              static_cast<int>(status.code()));

    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(ledger.drain_completions(completions, &status));
    ASSERT_EQ(1U, completions.size());
    ASSERT_TRUE(ledger.post(third, &status)) << status.message();

    CompletionLedger packet_limited;
    ASSERT_TRUE(packet_limited.configure(owner, 100, 4, 3, &status));
    ASSERT_TRUE(packet_limited.post(write_work(owner, 1, 100, 2), &status));
    EXPECT_FALSE(packet_limited.post(write_work(owner, 2, 102, 2), &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::RESOURCE_EXHAUSTED),
              static_cast<int>(status.code()));
    ASSERT_TRUE(packet_limited.post(write_work(owner, 2, 102, 1), &status));
    EXPECT_EQ(3U, packet_limited.snapshot().outstanding_packets);
}

TEST(rocev2_native, cumulative_ack_covers_multiple_work_items_across_psn_wrap) {
    const QpId owner(SessionGeneration(10), 2);
    CompletionLedger ledger;
    Status status;
    ASSERT_TRUE(ledger.configure(owner, 0xfffffe, 4, 8, &status));
    ASSERT_TRUE(ledger.post(write_work(owner, 1, 0xfffffe, 2, 100), &status));
    ASSERT_TRUE(ledger.post(write_work(owner, 2, 0, 2, 200), &status));

    ASSERT_TRUE(ledger.acknowledge(1, &status)) << status.message();
    const CompletionLedgerSnapshot snapshot = ledger.snapshot();
    EXPECT_EQ(0U, snapshot.outstanding_work);
    EXPECT_EQ(0U, snapshot.outstanding_packets);
    EXPECT_EQ(2U, snapshot.ready_completions);
    EXPECT_EQ(1U, snapshot.last_acked_psn);
    EXPECT_EQ(2U, snapshot.counters.writes_completed);

    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(ledger.drain_completions(completions, &status));
    ASSERT_EQ(2U, completions.size());
    EXPECT_EQ(1U, completions[0].id.sequence());
    EXPECT_EQ(100U, completions[0].completed_bytes);
    EXPECT_EQ(2U, completions[1].id.sequence());
    EXPECT_EQ(200U, completions[1].completed_bytes);
}

TEST(rocev2_native, duplicate_and_stale_acks_are_idempotent_but_future_acks_fail) {
    const QpId owner(SessionGeneration(11), 3);
    CompletionLedger ledger;
    Status status;
    ASSERT_TRUE(ledger.configure(owner, 0xfffffe, 2, 4, &status));
    ASSERT_TRUE(ledger.post(write_work(owner, 1, 0xfffffe, 4), &status));
    ASSERT_TRUE(ledger.acknowledge(1, &status));

    EXPECT_TRUE(ledger.acknowledge(1, &status));
    EXPECT_TRUE(ledger.acknowledge(0xffffff, &status));
    EXPECT_EQ(2U, ledger.snapshot().counters.duplicate_acks);

    EXPECT_FALSE(ledger.acknowledge(2, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(status.code()));
    EXPECT_FALSE(ledger.acknowledge((1U + PSN24_HALF_SPACE) & PSN24_MASK,
                                    &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(status.code()));
    EXPECT_EQ(2U, ledger.snapshot().counters.invalid_acks);
}

TEST(rocev2_native, read_assembly_requires_exact_contiguous_bytes_psns_and_last) {
    const QpId owner(SessionGeneration(12), 4);
    CompletionLedger ledger;
    Status status;
    ASSERT_TRUE(ledger.configure(owner, 0xfffffe, 2, 3, &status));
    const WorkDescriptor read = read_work(owner, 1, 0xfffffe, 7, 3);
    ASSERT_TRUE(ledger.post(read, &status));

    ASSERT_TRUE(ledger.record_read_response(read.id, 0xfffffe, 3, false, &status));
    ASSERT_TRUE(ledger.record_read_response(read.id, 0xffffff, 2, false, &status));
    ASSERT_TRUE(ledger.record_read_response(read.id, 0, 2, true, &status));

    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(ledger.drain_completions(completions, &status));
    ASSERT_EQ(1U, completions.size());
    EXPECT_EQ(static_cast<int>(CompletionState::SUCCESS),
              static_cast<int>(completions[0].state));
    EXPECT_EQ(static_cast<int>(WorkKind::READ),
              static_cast<int>(completions[0].kind));
    EXPECT_EQ(7U, completions[0].completed_bytes);
    EXPECT_EQ(3U, completions[0].completed_psns);
    EXPECT_EQ(0U, ledger.snapshot().last_acked_psn);

    CompletionLedger invalid;
    ASSERT_TRUE(invalid.configure(owner, 200, 3, 3, &status));
    const WorkDescriptor early = read_work(owner, 1, 200, 4, 2);
    ASSERT_TRUE(invalid.post(early, &status));
    EXPECT_FALSE(invalid.record_read_response(early.id, 200, 2, true, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(status.code()));
    ASSERT_TRUE(invalid.record_read_response(early.id, 200, 2, false, &status));
    EXPECT_FALSE(invalid.record_read_response(early.id, 202, 2, true, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(status.code()));
    EXPECT_FALSE(invalid.record_read_response(early.id, 201, 2, false, &status));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(status.code()));
    ASSERT_TRUE(invalid.record_read_response(early.id, 201, 2, true, &status));
}

TEST(rocev2_native, qp_read_reserves_the_full_response_psn_range) {
    QueuePair qp(QpId(SessionGeneration(14), 6));
    Status status;
    ASSERT_TRUE(establish_qp(qp, &status, 0xfffffe, 700, 4, 8))
        << status.message();
    const QpPeerConfig peer = peer_qp_config(700);

    PostedWork read;
    ASSERT_TRUE(qp.post_read(peer.virtual_address,
                             peer.rkey,
                             1536,
                             true,
                             read,
                             &status));
    EXPECT_EQ(0xfffffeU, read.first_psn);
    EXPECT_EQ(0U, read.terminal_psn);
    EXPECT_EQ(3U, read.reserved_psns);
    EXPECT_EQ(1U, qp.snapshot().next_sq_psn);
    EXPECT_EQ(3U, qp.snapshot().ledger.outstanding_packets);
}

TEST(rocev2_native, responder_accepts_a_read_request_as_a_reserved_psn_span) {
    QueuePair qp(QpId(SessionGeneration(15), 7));
    Status status;
    ASSERT_TRUE(establish_qp(qp, &status, 10, 0xfffffe, 4, 8))
        << status.message();

    ASSERT_TRUE(qp.accept_rx_span(0xfffffe, 3, &status)) << status.message();
    const QpSnapshot snapshot = qp.snapshot();
    EXPECT_EQ(1U, snapshot.expected_rx_psn);
    EXPECT_EQ(1U, snapshot.counters.receive_packets_accepted);
    EXPECT_EQ(3U, snapshot.counters.receive_psns_advanced);
    EXPECT_EQ(static_cast<int>(RxPsnDisposition::DUPLICATE),
              static_cast<int>(qp.classify_rx_psn(0xfffffe)));
    EXPECT_EQ(static_cast<int>(RxPsnDisposition::EXPECTED),
              static_cast<int>(qp.classify_rx_psn(1)));
}

TEST(rocev2_native, zero_byte_rdma_operations_still_reserve_one_psn) {
    QueuePair qp(QpId(SessionGeneration(16), 8));
    Status status;
    ASSERT_TRUE(establish_qp(qp, &status, 900, 1200, 4, 8))
        << status.message();

    PostedWork write;
    ASSERT_TRUE(qp.post_write(0, 0, 0, true, write, &status))
        << status.message();
    EXPECT_EQ(1U, write.reserved_psns);
    EXPECT_EQ(900U, write.first_psn);
    EXPECT_EQ(900U, write.terminal_psn);

    PostedWork read;
    ASSERT_TRUE(qp.post_read(0, 0, 0, true, read, &status))
        << status.message();
    EXPECT_EQ(1U, read.reserved_psns);
    EXPECT_EQ(901U, read.first_psn);
    EXPECT_EQ(901U, read.terminal_psn);
    ASSERT_TRUE(qp.record_read_response(read.id, 901, 0, true, &status))
        << status.message();

    ASSERT_TRUE(qp.acknowledge(900, &status)) << status.message();
    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(qp.drain_completions(completions, &status));
    ASSERT_EQ(2U, completions.size());
    EXPECT_EQ(0U, completions[0].completed_bytes);
    EXPECT_EQ(0U, completions[1].completed_bytes);
}

TEST(rocev2_native, error_transition_flushes_work_newest_first_and_releases_resources) {
    QueuePair qp(QpId(SessionGeneration(13), 5));
    Status status;
    ASSERT_TRUE(establish_qp(qp, &status, 100, 500, 4, 8)) << status.message();
    const QpPeerConfig peer = peer_qp_config(500);

    PostedWork first;
    PostedWork second;
    PostedWork third;
    ASSERT_TRUE(qp.post_write(peer.virtual_address,
                              peer.rkey,
                              1025,
                              true,
                              first,
                              &status));
    ASSERT_TRUE(qp.post_read(peer.virtual_address + 1025,
                             peer.rkey,
                             64,
                             true,
                             second,
                             &status));
    ASSERT_TRUE(qp.post_write(peer.virtual_address + 1089,
                              peer.rkey,
                              32,
                              true,
                              third,
                              &status));

    ASSERT_TRUE(qp.enter_error(ErrorCode::PROTOCOL_ERROR,
                               "fatal transport error",
                               &status));
    const QpSnapshot snapshot = qp.snapshot();
    EXPECT_EQ(static_cast<int>(QpState::ERR), static_cast<int>(snapshot.state));
    EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
              static_cast<int>(snapshot.error_code));
    EXPECT_FALSE(snapshot.ledger.configured);
    EXPECT_FALSE(snapshot.local_window.active);
    EXPECT_FALSE(snapshot.remote_window.active);
    EXPECT_FALSE(snapshot.retransmit_timer_armed);

    std::vector<CompletionRecord> completions;
    ASSERT_TRUE(qp.drain_completions(completions, &status));
    ASSERT_EQ(3U, completions.size());
    EXPECT_TRUE(third.id == completions[0].id);
    EXPECT_TRUE(second.id == completions[1].id);
    EXPECT_TRUE(first.id == completions[2].id);
    for (size_t index = 0; index < completions.size(); index++) {
        EXPECT_EQ(static_cast<int>(CompletionState::FLUSHED),
                  static_cast<int>(completions[index].state));
        EXPECT_EQ(static_cast<int>(ErrorCode::PROTOCOL_ERROR),
                  static_cast<int>(completions[index].error));
    }
}

TEST(rocev2_native, codec_round_trips_every_supported_opcode_and_extended_header) {
    const IbOpcode opcodes[] = {
        IB_RDMA_WRITE_FIRST,
        IB_RDMA_WRITE_MIDDLE,
        IB_RDMA_WRITE_LAST,
        IB_RDMA_WRITE_LAST_WITH_IMMEDIATE,
        IB_RDMA_WRITE_ONLY,
        IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE,
        IB_RDMA_READ_REQUEST,
        IB_RDMA_READ_RESPONSE_FIRST,
        IB_RDMA_READ_RESPONSE_MIDDLE,
        IB_RDMA_READ_RESPONSE_LAST,
        IB_RDMA_READ_RESPONSE_ONLY,
        IB_ACKNOWLEDGE,
        IB_UD_SEND_ONLY,
        IB_CNP
    };
    const std::vector<uint8_t> payload = {0xde, 0xad, 0xbe, 0xef, 0x42};
    const std::vector<uint8_t> empty;

    for (size_t index = 0; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        const IbOpcode opcode = opcodes[index];
        const bool payload_forbidden = opcode == IB_RDMA_READ_REQUEST ||
                                       opcode == IB_ACKNOWLEDGE ||
                                       opcode == IB_CNP;
        const std::vector<uint8_t> &wire_payload = payload_forbidden ? empty : payload;
        const PacketEncodeSpec spec = codec_packet_spec(opcode, wire_payload);
        const std::vector<uint8_t> frame = encode_codec_packet(spec);
        ASSERT_FALSE(frame.empty()) << "opcode " << static_cast<unsigned>(opcode);
        const DecodedPacket decoded = decode_codec_packet(frame);

        EXPECT_EQ(static_cast<int>(opcode), static_cast<int>(decoded.opcode));
        EXPECT_EQ(spec.pkey, decoded.pkey);
        EXPECT_EQ(spec.dest_qpn, decoded.dest_qpn);
        EXPECT_EQ(spec.psn, decoded.psn);
        EXPECT_EQ(spec.solicited, decoded.solicited);
        EXPECT_EQ(spec.migration_request, decoded.migration_request);
        EXPECT_EQ(spec.ack_request, decoded.ack_request);
        EXPECT_EQ(spec.fecn, decoded.fecn);
        EXPECT_EQ(spec.becn, decoded.becn);
        EXPECT_EQ(spec.has_reth, decoded.has_reth);
        EXPECT_EQ(spec.has_aeth, decoded.has_aeth);
        EXPECT_EQ(spec.has_immediate_data, decoded.has_immediate_data);
        EXPECT_EQ(spec.has_deth, decoded.has_deth);
        EXPECT_EQ(static_cast<uint8_t>((4 - (wire_payload.size() & 3U)) & 3U),
                  decoded.pad_count);
        EXPECT_EQ(wire_payload.size(), decoded.payload.length);
        EXPECT_TRUE(std::equal(wire_payload.begin(),
                               wire_payload.end(),
                               frame.begin() + decoded.payload.offset));
        for (size_t pad = decoded.padding.offset;
             pad < decoded.padding.end();
             pad++) {
            EXPECT_EQ(0, frame[pad]);
        }
        if (spec.has_reth) {
            EXPECT_EQ(spec.reth.virtual_address, decoded.reth.virtual_address);
            EXPECT_EQ(spec.reth.rkey, decoded.reth.rkey);
            EXPECT_EQ(spec.reth.dma_length, decoded.reth.dma_length);
        }
        if (spec.has_aeth) {
            EXPECT_EQ(spec.aeth.syndrome, decoded.aeth.syndrome);
            EXPECT_EQ(spec.aeth.msn, decoded.aeth.msn);
        }
        if (spec.has_immediate_data) {
            EXPECT_EQ(spec.immediate_data, decoded.immediate_data);
        }
        if (spec.has_deth) {
            EXPECT_EQ(spec.deth.qkey, decoded.deth.qkey);
            EXPECT_EQ(spec.deth.source_qpn, decoded.deth.source_qpn);
        }
        EXPECT_EQ(opcode == IB_CNP, decoded.has_cnp_reserved);
        if (decoded.has_cnp_reserved) {
            for (size_t reserved = 0; reserved < 16; reserved++) {
                EXPECT_EQ(0, decoded.cnp_reserved[reserved]);
            }
        }
        EXPECT_NE(0, decoded.udp_checksum);
        CodecError error;
        EXPECT_TRUE(NativePacketCodec::verify_icrc(ByteSpan(frame.data(), frame.size()),
                                                   decoded,
                                                   &error))
            << error.message;
        EXPECT_TRUE(NativePacketCodec::verify_udp_checksum(
            ByteSpan(frame.data(), frame.size()), decoded, &error)) << error.message;
    }
}

TEST(rocev2_native, codec_round_trips_vlan_and_ipv6_with_required_udp_checksum) {
    const std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7};
    PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE,
                                               payload);
    spec.endpoint = codec_endpoint(true, true);
    const std::vector<uint8_t> frame = encode_codec_packet(spec);
    const DecodedPacket decoded = decode_codec_packet(frame);

    EXPECT_TRUE(decoded.endpoint.ipv6);
    EXPECT_TRUE(decoded.endpoint.has_vlan);
    EXPECT_EQ(4094, decoded.endpoint.vlan);
    EXPECT_EQ(7, decoded.endpoint.pcp);
    EXPECT_TRUE(decoded.endpoint.vlan_dei);
    EXPECT_EQ(18U, decoded.ethernet.length);
    EXPECT_EQ(40U, decoded.ip_header.length);
    EXPECT_EQ(0x86dd, read_be16(frame, 16));
    EXPECT_NE(0, decoded.udp_checksum);
    EXPECT_EQ(ROCEV2_UDP_PORT, decoded.endpoint.udp_dport);
    EXPECT_TRUE(std::equal(spec.endpoint.src_ip,
                           spec.endpoint.src_ip + 16,
                           decoded.endpoint.src_ip));
    EXPECT_TRUE(std::equal(spec.endpoint.dst_ip,
                           spec.endpoint.dst_ip + 16,
                           decoded.endpoint.dst_ip));
}

TEST(rocev2_native, codec_decodes_ipv4_options_with_valid_checksums_and_icrc) {
    const std::vector<uint8_t> payload = {0x10, 0x20, 0x30};
    const PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    std::vector<uint8_t> frame = encode_codec_packet(spec);
    const size_t l2_length = 14;
    const size_t old_ip_length = 20;
    const uint8_t options[4] = {1, 1, 0, 0};
    frame.insert(frame.begin() + l2_length + old_ip_length,
                 options,
                 options + sizeof(options));
    frame[l2_length] = 0x46;
    write_be16(frame,
               l2_length + 2,
               static_cast<uint16_t>(frame.size() - l2_length));
    frame[l2_length + 10] = 0;
    frame[l2_length + 11] = 0;
    const uint16_t ip_checksum = NativePacketCodec::ipv4_checksum(
        ByteSpan(frame.data() + l2_length, 24));
    write_be16(frame, l2_length + 10, ip_checksum);

    const size_t udp_offset = l2_length + 24;
    frame[udp_offset + 6] = 0;
    frame[udp_offset + 7] = 0;
    const size_t bth_offset = udp_offset + 8;
    const size_t icrc_offset = frame.size() - ROCEV2_ICRC_LEN;
    const uint32_t icrc = trex_roce_v2_calc_icrc_bytes(
        frame.data(),
        static_cast<uint32_t>(frame.size()),
        static_cast<uint16_t>(l2_length),
        24,
        static_cast<uint16_t>(bth_offset));
    write_le32(frame, icrc_offset, icrc);

    DecodeOptions without_udp;
    without_udp.verify_udp_checksum = false;
    DecodedPacket checksum_view;
    CodecError error;
    ASSERT_TRUE(NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()),
                                          without_udp,
                                          checksum_view,
                                          &error))
        << error.message;
    uint16_t udp_checksum = 0;
    ASSERT_TRUE(NativePacketCodec::udp_checksum(ByteSpan(frame.data(), frame.size()),
                                                checksum_view,
                                                udp_checksum,
                                                &error))
        << error.message;
    write_be16(frame, udp_offset + 6, udp_checksum);

    const DecodedPacket decoded = decode_codec_packet(frame);
    EXPECT_FALSE(decoded.endpoint.ipv6);
    EXPECT_EQ(24U, decoded.ip_header.length);
    EXPECT_EQ(udp_offset, decoded.udp_header.offset);
    EXPECT_EQ(bth_offset, decoded.bth.offset);
    EXPECT_TRUE(NativePacketCodec::verify_ipv4_checksum(
        ByteSpan(frame.data() + l2_length, decoded.ip_header.length)));
    EXPECT_EQ(payload.size(), decoded.payload.length);
}

TEST(rocev2_native, codec_rejects_bad_icrc_and_udp_checksum_independently) {
    const std::vector<uint8_t> payload = {0xaa, 0xbb, 0xcc, 0xdd};
    const PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    const std::vector<uint8_t> valid = encode_codec_packet(spec);

    std::vector<uint8_t> bad_icrc = valid;
    bad_icrc[bad_icrc.size() - 1] ^= 0x80;
    DecodeOptions ignore_udp;
    ignore_udp.verify_udp_checksum = false;
    expect_decode_error(bad_icrc, CodecErrorCode::BAD_ICRC, &ignore_udp);

    const DecodedPacket decoded = decode_codec_packet(valid);
    std::vector<uint8_t> bad_udp = valid;
    bad_udp[decoded.udp_header.offset + 6] ^= 0x01;
    expect_decode_error(bad_udp, CodecErrorCode::INVALID_UDP_CHECKSUM);

    std::vector<uint8_t> ipv4_zero_udp = valid;
    ipv4_zero_udp[decoded.udp_header.offset + 6] = 0;
    ipv4_zero_udp[decoded.udp_header.offset + 7] = 0;
    DecodedPacket zero_checksum;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::decode(
        ByteSpan(ipv4_zero_udp.data(), ipv4_zero_udp.size()),
        zero_checksum,
        &error)) << error.message;

    PacketEncodeSpec ipv6_spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    ipv6_spec.endpoint = codec_endpoint(true, false);
    std::vector<uint8_t> ipv6 = encode_codec_packet(ipv6_spec);
    const DecodedPacket ipv6_decoded = decode_codec_packet(ipv6);
    ipv6[ipv6_decoded.udp_header.offset + 6] = 0;
    ipv6[ipv6_decoded.udp_header.offset + 7] = 0;
    expect_decode_error(ipv6, CodecErrorCode::INVALID_UDP_CHECKSUM);
}

TEST(rocev2_native, codec_rejects_truncation_lengths_fragments_and_reserved_bits) {
    const std::vector<uint8_t> payload = {1, 2, 3, 4};
    const PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    const std::vector<uint8_t> valid = encode_codec_packet(spec);
    const DecodedPacket decoded = decode_codec_packet(valid);

    std::vector<uint8_t> truncated(valid.begin(), valid.begin() + 13);
    expect_decode_error(truncated, CodecErrorCode::TRUNCATED);

    std::vector<uint8_t> bad_length = valid;
    write_be16(bad_length,
               decoded.udp_header.offset + 4,
               static_cast<uint16_t>(valid.size()));
    expect_decode_error(bad_length, CodecErrorCode::INVALID_UDP_LENGTH);

    std::vector<uint8_t> fragmented = valid;
    fragmented[decoded.ip_header.offset + 6] |= 0x20;
    expect_decode_error(fragmented, CodecErrorCode::IPV4_FRAGMENTED);

    std::vector<uint8_t> bad_bth_version = valid;
    bad_bth_version[decoded.bth.offset + 1] |= 0x01;
    expect_decode_error(bad_bth_version, CodecErrorCode::INVALID_BTH);

    std::vector<uint8_t> bad_congestion_reserved = valid;
    bad_congestion_reserved[decoded.bth.offset + 4] |= 0x01;
    expect_decode_error(bad_congestion_reserved,
                        CodecErrorCode::INVALID_RESERVED_FIELD);

    const PacketEncodeSpec ud_spec = codec_packet_spec(IB_UD_SEND_ONLY, payload);
    std::vector<uint8_t> ud = encode_codec_packet(ud_spec);
    const DecodedPacket decoded_ud = decode_codec_packet(ud);
    ud[decoded_ud.extended_headers.offset + 4] = 1;
    expect_decode_error(ud, CodecErrorCode::INVALID_RESERVED_FIELD);

    const std::vector<uint8_t> empty;
    const PacketEncodeSpec cnp_spec = codec_packet_spec(IB_CNP, empty);
    std::vector<uint8_t> cnp = encode_codec_packet(cnp_spec);
    const DecodedPacket decoded_cnp = decode_codec_packet(cnp);
    cnp[decoded_cnp.extended_headers.offset + 7] = 1;
    expect_decode_error(cnp, CodecErrorCode::INVALID_RESERVED_FIELD);
}

TEST(rocev2_native, codec_rejects_padding_opcode_and_header_mismatches) {
    const std::vector<uint8_t> payload = {0x5a};
    const PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    const std::vector<uint8_t> valid = encode_codec_packet(spec);
    const DecodedPacket decoded = decode_codec_packet(valid);
    ASSERT_EQ(3U, decoded.padding.length);

    std::vector<uint8_t> bad_padding = valid;
    bad_padding[decoded.padding.offset + 1] = 1;
    expect_decode_error(bad_padding, CodecErrorCode::INVALID_PADDING);

    std::vector<uint8_t> unsupported_opcode = valid;
    unsupported_opcode[decoded.bth.offset] = 0xff;
    expect_decode_error(unsupported_opcode, CodecErrorCode::UNSUPPORTED_OPCODE);

    std::vector<uint8_t> wrong_wire_layout = valid;
    wrong_wire_layout[decoded.bth.offset] = IB_ACKNOWLEDGE;
    expect_decode_error(wrong_wire_layout, CodecErrorCode::INVALID_PAYLOAD);

    PacketEncodeSpec wrong_encode_layout = codec_packet_spec(IB_RDMA_WRITE_MIDDLE,
                                                              payload);
    wrong_encode_layout.has_reth = true;
    size_t encoded_size = 123;
    CodecError error;
    EXPECT_FALSE(NativePacketCodec::encoded_size(wrong_encode_layout,
                                                  encoded_size,
                                                  &error));
    EXPECT_EQ(0U, encoded_size);
    EXPECT_EQ(static_cast<int>(CodecErrorCode::ILLEGAL_EXTENDED_HEADERS),
              static_cast<int>(error.code));

    PacketEncodeSpec read_with_payload = codec_packet_spec(IB_RDMA_READ_REQUEST,
                                                            payload);
    EXPECT_FALSE(NativePacketCodec::encoded_size(read_with_payload,
                                                  encoded_size,
                                                  &error));
    EXPECT_EQ(static_cast<int>(CodecErrorCode::INVALID_PAYLOAD),
              static_cast<int>(error.code));
}

TEST(rocev2_native, codec_encode_honors_preflight_and_buffer_bounds) {
    const std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    const PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    CodecError error;
    size_t needed = 0;
    ASSERT_TRUE(NativePacketCodec::encoded_size(spec, needed, &error))
        << error.message;
    ASSERT_GT(needed, 1U);

    std::vector<uint8_t> storage(needed, 0xa5);
    size_t written = 999;
    EXPECT_FALSE(NativePacketCodec::encode(spec,
                                           storage.data(),
                                           needed - 1,
                                           written,
                                           &error));
    EXPECT_EQ(0U, written);
    EXPECT_EQ(static_cast<int>(CodecErrorCode::BUFFER_TOO_SMALL),
              static_cast<int>(error.code));
    EXPECT_TRUE(std::equal(storage.begin(), storage.end(),
                           std::vector<uint8_t>(needed, 0xa5).begin()));

    ASSERT_TRUE(NativePacketCodec::encode(spec,
                                          storage.data(),
                                          storage.size(),
                                          written,
                                          &error))
        << error.message;
    EXPECT_EQ(needed, written);
    EXPECT_FALSE(NativePacketCodec::encode(spec,
                                           NULL,
                                           needed,
                                           written,
                                           &error));
    EXPECT_EQ(0U, written);
    EXPECT_EQ(static_cast<int>(CodecErrorCode::INVALID_ARGUMENT),
              static_cast<int>(error.code));
}

TEST(rocev2_native, codec_accepts_vlan_4094_and_rejects_reserved_4095) {
    const std::vector<uint8_t> payload = {1};
    PacketEncodeSpec spec = codec_packet_spec(IB_RDMA_WRITE_ONLY, payload);
    spec.endpoint.has_vlan = true;
    spec.endpoint.vlan = 4094;
    CodecError error;
    size_t encoded_size = 0;
    ASSERT_TRUE(NativePacketCodec::encoded_size(spec, encoded_size, &error))
        << error.message;
    EXPECT_GT(encoded_size, 0U);

    spec.endpoint.vlan = 4095;
    EXPECT_FALSE(NativePacketCodec::encoded_size(spec, encoded_size, &error));
    EXPECT_EQ(0U, encoded_size);
    EXPECT_EQ(static_cast<int>(CodecErrorCode::INVALID_ARGUMENT),
              static_cast<int>(error.code));
}

namespace engine_device_tests {

static const uint64_t GENERATION = 0x12345678;
static const uint64_t VA = 0x1000;
static const uint32_t LKEY = 7;
static const uint32_t RKEY = 9;
static const uint32_t QPN = 123;

static EngineConfig engine_config(uint64_t generation = GENERATION) {
    EngineConfig config;
    config.generation = SessionGeneration(generation);
    config.endpoint_id = 42;
    config.max_qps = 8;
    config.max_memory_regions = 8;
    config.max_total_memory_bytes = 4096;
    config.max_completion_records = 4;
    config.max_ingress_frames = 16;
    config.max_tx_frames = 32;
    config.max_frame_bytes = 1024;
    config.ingress_budget_per_poll = 16;
    config.tx_budget_per_poll = 16;
    return config;
}

static EngineMemoryConfig memory_config(uint32_t id, uint32_t pd) {
    EngineMemoryConfig config;
    config.memory_id = id;
    config.window.base_address = VA;
    config.window.length = 256;
    config.window.lkey = LKEY;
    config.window.rkey = RKEY;
    config.window.protection_domain = pd;
    config.window.access = MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
                           MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE |
                           MemoryAccess::REMOTE_ATOMIC;
    config.initial_value = 0;
    return config;
}

static std::unique_ptr<QueuePair> make_qp(uint32_t index, uint32_t pd,
                                          bool ipv6 = false,
                                          uint64_t generation = GENERATION) {
    std::unique_ptr<QueuePair> qp(new QueuePair(QpId(SessionGeneration(generation), index)));
    QpConfig local;
    local.local_qpn = QPN;
    local.starting_sq_psn = 0;
    local.pmtu = 256;
    local.sq_depth_messages = 4;
    local.sq_depth_packets = 16;
    local.rq_depth_messages = 4;
    local.max_receive_sge = 2;
    local.protection_domain = pd;
    local.allow_remote_registry = true;
    local.max_rd_atomic = 1;
    local.max_dest_rd_atomic = 1;
    EXPECT_TRUE(qp->create(local, memory_config(0, pd).window));
    QpPeerConfig peer;
    peer.remote_qpn = 456;
    peer.starting_psn = 0;
    peer.pmtu = 256;
    peer.rkey = RKEY;
    peer.virtual_address = VA;
    peer.window_size = 256;
    peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE |
                         MemoryAccess::REMOTE_ATOMIC;
    peer.max_rd_atomic = 1;
    peer.ip = ipv6 ? "2001:db8::2" : "198.51.100.2";
    peer.mac = "02:00:00:00:00:02";
    EXPECT_TRUE(qp->modify_to_rtr(peer));
    EXPECT_TRUE(qp->modify_to_rts());
    return qp;
}

static NativeTransportConfig transport_config(const NativeEndpoint &endpoint) {
    NativeTransportConfig config;
    config.endpoint = endpoint;
    config.endpoint.ecn = 0;
    config.max_tx_frames = 8;
    config.max_staged_write_bytes = 256;
    config.max_read_cache_entries = 2;
    config.max_read_cache_bytes = 512;
    return config;
}

static EngineQpBinding binding(uint32_t id) {
    EngineQpBinding value;
    value.memory_id = id;
    value.peer_endpoint = 43;
    value.egress_port = 0;
    value.peer_ingress_port = 1;
    return value;
}

static std::vector<uint8_t> incoming_write(const NativeEndpoint &local,
                                            uint8_t value,
                                            uint32_t psn = 0) {
    PacketEncodeSpec packet;
    packet.endpoint = local;
    memcpy(packet.endpoint.src_mac, local.dst_mac, 6);
    memcpy(packet.endpoint.dst_mac, local.src_mac, 6);
    memcpy(packet.endpoint.src_ip, local.dst_ip, 16);
    memcpy(packet.endpoint.dst_ip, local.src_ip, 16);
    packet.endpoint.ecn = 0;
    packet.dest_qpn = QPN;
    packet.psn = psn;
    packet.ack_request = true;
    packet.reth.virtual_address = VA;
    packet.reth.rkey = RKEY;
    packet.reth.dma_length = 8;
    const std::vector<uint8_t> payload(8, value);
    packet.payload = ByteSpan(payload.data(), payload.size());
    size_t size = 0;
    CodecError error;
    EXPECT_TRUE(NativePacketCodec::encoded_size(packet, size, &error)) << error.message;
    std::vector<uint8_t> wire(size);
    size_t written = 0;
    EXPECT_TRUE(NativePacketCodec::encode(packet, wire.data(), wire.size(), written, &error))
        << error.message;
    EXPECT_EQ(size, written);
    return wire;
}

static void deliver(NativeEndpointEngine &engine,
                    const std::vector<uint8_t> &wire,
                    uint64_t now = 1) {
    EngineWireFrame metadata;
    metadata.generation = engine.generation();
    metadata.ingress_port = 0;
    metadata.destination_endpoint = engine.endpoint_id();
    Status status;
    ASSERT_TRUE(engine.enqueue_ingress_copy(metadata, wire.data(), wire.size(), &status))
        << status.message();
    EnginePollResult result;
    ASSERT_TRUE(engine.poll(now, result, &status)) << status.message();
}

static std::vector<uint8_t> read_memory(const NativeEndpointEngine &engine, uint32_t id) {
    std::vector<uint8_t> value;
    EXPECT_TRUE(engine.memory(id)->read(VA, 8, MemoryAccess::LOCAL_READ, value));
    return value;
}

/* Every invocation reuses all wire QPNs, keys and virtual addresses. The
 * destination identity alone must select the correct independent RC state. */
static void check_two_device_writes(const NativeEndpoint &first,
                                     const NativeEndpoint &second) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_memory(memory_config(9001, 22)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11, first.ipv6),
                                      transport_config(first), binding(1001)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(90001, 22, second.ipv6),
                                      transport_config(second), binding(9001)));
    ASSERT_TRUE(engine.start());
    deliver(engine, incoming_write(first, 0xa1), 1);
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xa1) == read_memory(engine, 1001));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0) == read_memory(engine, 9001));
    deliver(engine, incoming_write(second, 0xb2), 2);
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xa1) == read_memory(engine, 1001));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xb2) == read_memory(engine, 9001));
    EngineQpRuntimeView left, right;
    ASSERT_TRUE(engine.qp_runtime_view(QpId(SessionGeneration(GENERATION), 6001), left));
    ASSERT_TRUE(engine.qp_runtime_view(QpId(SessionGeneration(GENERATION), 90001), right));
    EXPECT_EQ(1U, left.qp.expected_rx_psn);
    EXPECT_EQ(1U, right.qp.expected_rx_psn);
    EXPECT_TRUE(left.binding.has_local_endpoint);
    EXPECT_TRUE(right.binding.has_local_endpoint);
    const EngineSnapshot snapshot = engine.snapshot();
    EXPECT_EQ(0U, snapshot.counters.ingress_unknown_qp);
    EXPECT_EQ(0U, snapshot.hot_path_growth_events);
    ASSERT_EQ(2U, snapshot.qps.size());
    EXPECT_EQ(1U, snapshot.qps[0].transport.native.counters.deliveries);
    EXPECT_EQ(1U, snapshot.qps[1].transport.native.counters.deliveries);
}

TEST(rocev2_native_engine_devices, same_qpn_routes_distinct_local_macs) {
    NativeEndpoint second = codec_endpoint();
    second.src_mac[5] = 3;
    check_two_device_writes(codec_endpoint(), second);
}

TEST(rocev2_native_engine_devices, same_qpn_routes_distinct_local_ipv4_addresses) {
    NativeEndpoint second = codec_endpoint();
    second.src_ip[3] = 3;
    check_two_device_writes(codec_endpoint(), second);
}

TEST(rocev2_native_engine_devices, same_qpn_routes_distinct_vlans_and_untagged_identity) {
    NativeEndpoint first = codec_endpoint(false, true);
    NativeEndpoint second = first;
    first.vlan = 100;
    second.vlan = 200;
    check_two_device_writes(first, second);
    second = first;
    second.has_vlan = false;
    second.vlan = 0;
    check_two_device_writes(first, second);
}

TEST(rocev2_native_engine_devices, same_qpn_routes_ipv6_and_distinguishes_ip_families) {
    NativeEndpoint first = codec_endpoint(true);
    NativeEndpoint second = first;
    second.src_ip[15] = 3;
    check_two_device_writes(first, second);
    check_two_device_writes(codec_endpoint(), first);
}

TEST(rocev2_native_engine_devices, unknown_destination_and_wrong_peer_do_not_reach_another_qp) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                      transport_config(codec_endpoint()), binding(1001)));
    ASSERT_TRUE(engine.start());
    NativeEndpoint unknown = codec_endpoint();
    unknown.src_mac[5] = 0x55;
    deliver(engine, incoming_write(unknown, 0xaa), 1);
    unknown = codec_endpoint();
    unknown.src_ip[3] = 0x55;
    deliver(engine, incoming_write(unknown, 0xaa), 2);
    unknown = codec_endpoint(false, true);
    deliver(engine, incoming_write(unknown, 0xaa), 3);
    NativeEndpoint wrong_peer = codec_endpoint();
    wrong_peer.dst_ip[3] = 0x55;
    deliver(engine, incoming_write(wrong_peer, 0xaa), 4);
    EXPECT_TRUE(std::vector<uint8_t>(8, 0) == read_memory(engine, 1001));
    const EngineSnapshot snapshot = engine.snapshot();
    EXPECT_EQ(3U, snapshot.counters.ingress_unknown_qp);
    ASSERT_EQ(1U, snapshot.qps.size());
    EXPECT_EQ(1U, snapshot.qps[0].transport.native.counters.endpoint_mismatch_packets);
    EXPECT_EQ(0U, snapshot.qps[0].qp.expected_rx_psn);
    deliver(engine, incoming_write(codec_endpoint(), 0xcc), 5);
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xcc) == read_memory(engine, 1001));
}

TEST(rocev2_native_engine_devices, duplicate_wire_identity_and_failed_attach_leave_existing_owner_usable) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_memory(memory_config(9001, 22)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                      transport_config(codec_endpoint()), binding(1001)));
    Status status;
    EXPECT_FALSE(engine.add_native_qp(make_qp(90001, 22),
                                       transport_config(codec_endpoint()), binding(9001), &status));
    EXPECT_TRUE(ErrorCode::DUPLICATE == status.code());
    NativeEndpoint second = codec_endpoint();
    second.src_mac[5] = 3;
    NativeTransportConfig invalid = transport_config(second);
    invalid.max_read_cache_entries = 0;
    EXPECT_FALSE(engine.add_native_qp(make_qp(90001, 22), invalid, binding(9001), &status));
    EXPECT_TRUE(ErrorCode::INVALID_ARGUMENT == status.code());
    EXPECT_EQ(NULL, engine.queue_pair(QpId(SessionGeneration(GENERATION), 90001)));
    EngineQpBinding mismatch = binding(9001);
    mismatch.has_local_endpoint = true;
    mismatch.local_endpoint = codec_endpoint();
    EXPECT_FALSE(engine.add_native_qp(make_qp(90001, 22), transport_config(second), mismatch, &status));
    EXPECT_TRUE(ErrorCode::INVALID_ARGUMENT == status.code());
    EXPECT_FALSE(engine.add_native_qp(make_qp(90001, 11),
                                       transport_config(second), binding(9001), &status));
    EXPECT_TRUE(ErrorCode::ACCESS_DENIED == status.code());
    ASSERT_TRUE(engine.add_native_qp(make_qp(90001, 22), transport_config(second), binding(9001)));
    ASSERT_TRUE(engine.start());
    deliver(engine, incoming_write(codec_endpoint(), 0xa1), 1);
    deliver(engine, incoming_write(second, 0xb2), 2);
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xa1) == read_memory(engine, 1001));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xb2) == read_memory(engine, 9001));
}

TEST(rocev2_native_engine_devices, memory_key_indexes_enforce_pd_and_follow_invalidate_rebind_removal) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_memory(memory_config(9001, 22)));
    Status status;
    EngineMemoryConfig duplicate_key = memory_config(7001, 11);
    duplicate_key.window.base_address += 512;
    EXPECT_FALSE(engine.add_memory(duplicate_key, &status));
    EXPECT_TRUE(ErrorCode::DUPLICATE == status.code());
    const uint8_t payload[] = {1, 2, 3};
    ASSERT_TRUE(engine.write_local_key(LKEY, 11, VA, ByteSpan(payload, 3)));
    EXPECT_FALSE(engine.write_local_key(LKEY, 33, VA, ByteSpan(payload, 3), &status));
    EXPECT_TRUE(ErrorCode::ACCESS_DENIED == status.code());
    ASSERT_TRUE(engine.validate_local_key(LKEY, 22, VA, 8));
    EXPECT_FALSE(engine.validate_local_key(LKEY, 22, VA + 255, 8));
    uint64_t original = 99;
    ASSERT_TRUE(engine.atomic_remote_key(RKEY, 22, VA, true, 0, 1, original));
    EXPECT_EQ(0U, original);
    ASSERT_TRUE(engine.invalidate_remote_key(RKEY, 11, true));
    EXPECT_FALSE(engine.validate_remote_access(RKEY, 11, VA, 8, MemoryAccess::REMOTE_READ));
    EXPECT_TRUE(engine.validate_remote_access(RKEY, 22, VA, 8, MemoryAccess::REMOTE_READ));
    EngineMemoryConfig collision = memory_config(7001, 11);
    collision.window.lkey = 17;
    collision.window.rkey = 19;
    collision.window.base_address += 512;
    ASSERT_TRUE(engine.add_memory(collision));
    EXPECT_FALSE(engine.rebind_memory_keys(1001, 17, 19, &status));
    EXPECT_TRUE(ErrorCode::DUPLICATE == status.code());
    EXPECT_TRUE(engine.validate_local_key(LKEY, 11, VA, 8));
    EXPECT_TRUE(engine.validate_remote_access(19, 11, VA + 512, 8, MemoryAccess::REMOTE_READ));
    ASSERT_TRUE(engine.remove_memory(7001));
    ASSERT_TRUE(engine.rebind_memory_keys(1001, 17, 19));
    EXPECT_FALSE(engine.validate_local_key(LKEY, 11, VA, 8));
    EXPECT_FALSE(engine.validate_remote_access(RKEY, 11, VA, 8, MemoryAccess::REMOTE_READ));
    EXPECT_TRUE(engine.validate_local_key(17, 11, VA, 8));
    EXPECT_TRUE(engine.validate_remote_access(19, 11, VA, 8, MemoryAccess::REMOTE_READ));
    EXPECT_FALSE(engine.rebind_memory_keys(1001, LKEY, 99, &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
    EXPECT_FALSE(engine.validate_remote_access(99, 11, VA, 8, MemoryAccess::REMOTE_READ));
    EXPECT_TRUE(engine.validate_remote_access(19, 11, VA, 8, MemoryAccess::REMOTE_READ));
    ASSERT_TRUE(engine.remove_memory(1001));
    EXPECT_EQ(NULL, engine.memory(1001));
    EXPECT_FALSE(engine.validate_local_key(17, 11, VA, 8));
    EXPECT_TRUE(engine.validate_local_key(LKEY, 22, VA, 8));
    /* Removing a vector element must not invalidate another registration's indexed pointer. */
    ASSERT_TRUE(engine.atomic_remote_key(RKEY, 22, VA, true, 0, 1, original));
    EXPECT_EQ(1U, original);
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    EXPECT_TRUE(engine.validate_local_key(LKEY, 11, VA, 8));
}

TEST(rocev2_native_engine_devices, key_rebind_updates_bound_qp_and_rejects_stale_keys) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                      transport_config(codec_endpoint()), binding(1001)));
    EXPECT_FALSE(engine.rebind_memory_keys(1001, 17, 19));
    EXPECT_TRUE(engine.validate_local_key(LKEY, 11, VA, 8));
    EXPECT_FALSE(engine.validate_local_key(17, 11, VA, 8));
    ASSERT_TRUE(engine.drain_qp(QpId(SessionGeneration(GENERATION), 6001)));
    ASSERT_TRUE(engine.rebind_memory_keys(1001, 17, 19));
    EngineQpRuntimeView view;
    ASSERT_TRUE(engine.qp_runtime_view(QpId(SessionGeneration(GENERATION), 6001), view));
    EXPECT_EQ(17U, view.qp.local_window.lkey);
    EXPECT_EQ(19U, view.qp.local_window.rkey);
    EXPECT_FALSE(engine.validate_local_key(LKEY, 11, VA, 8));
    EXPECT_TRUE(engine.validate_local_key(17, 11, VA, 8));
}

TEST(rocev2_native_engine_devices, closing_one_reused_qpn_preserves_another_devices_queued_ingress) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_memory(memory_config(9001, 22)));
    NativeEndpoint second = codec_endpoint();
    second.src_ip[3] = 3;
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                      transport_config(codec_endpoint()), binding(1001)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(90001, 22),
                                      transport_config(second), binding(9001)));
    ASSERT_TRUE(engine.start());
    EngineWireFrame metadata;
    metadata.generation = engine.generation();
    metadata.ingress_port = 0;
    for (unsigned i = 0; i != 2; ++i) {
        const std::vector<uint8_t> wire = incoming_write(i ? second : codec_endpoint(),
                                                        i ? 0xb2 : 0xa1);
        ASSERT_TRUE(engine.enqueue_ingress_copy(metadata, wire.data(), wire.size()));
    }
    ASSERT_TRUE(engine.close_qp(QpId(SessionGeneration(GENERATION), 6001), ErrorCode::ABORTED,
                                 "close device one"));
    EXPECT_EQ(1U, engine.snapshot().ingress_queued);
    EnginePollResult result;
    ASSERT_TRUE(engine.poll(1, result));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0) == read_memory(engine, 1001));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xb2) == read_memory(engine, 9001));
    const EngineSnapshot snapshot = engine.snapshot();
    ASSERT_EQ(2U, snapshot.qps.size());
    EXPECT_TRUE(snapshot.qps[0].qp.state == QpState::ERR);
    EXPECT_TRUE(snapshot.qps[1].qp.state == QpState::RTS);
}

TEST(rocev2_native_engine_devices, teardown_reconfigure_releases_identities_and_rejects_old_generation) {
    NativeEndpointEngine engine;
    ASSERT_TRUE(engine.configure(engine_config()));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                      transport_config(codec_endpoint()), binding(1001)));
    ASSERT_TRUE(engine.teardown(SessionGeneration(GENERATION)));
    EXPECT_EQ(NULL, engine.memory(1001));
    EXPECT_EQ(NULL, engine.queue_pair(QpId(SessionGeneration(GENERATION), 6001)));
    ASSERT_TRUE(engine.configure(engine_config(GENERATION + 1)));
    ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
    ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11, false, GENERATION + 1),
                                      transport_config(codec_endpoint()), binding(1001)));
    EXPECT_EQ(NULL, engine.queue_pair(QpId(SessionGeneration(GENERATION), 6001)));
    ASSERT_TRUE(NULL != engine.queue_pair(QpId(SessionGeneration(GENERATION + 1), 6001)));
    const NativeEndpointEngine &read_only = engine;
    EXPECT_EQ(NULL, read_only.queue_pair(QpId(SessionGeneration(GENERATION), 6001)));
    ASSERT_TRUE(NULL != read_only.queue_pair(QpId(SessionGeneration(GENERATION + 1), 6001)));
    EngineQpRuntimeView view;
    Status status;
    EXPECT_FALSE(engine.qp_runtime_view(QpId(SessionGeneration(GENERATION), 6001), view, &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
    ASSERT_TRUE(engine.start());
    deliver(engine, incoming_write(codec_endpoint(), 0xaa));
    EXPECT_TRUE(std::vector<uint8_t>(8, 0xaa) == read_memory(engine, 1001));
}

TEST(rocev2_native_engine_devices, legacy_unqualified_qpn_cannot_ambiguously_overlap_qualified_identity) {
    for (unsigned qualified_first = 0; qualified_first != 2; ++qualified_first) {
        NativeEndpointEngine engine;
        ASSERT_TRUE(engine.configure(engine_config()));
        ASSERT_TRUE(engine.add_memory(memory_config(1001, 11)));
        ASSERT_TRUE(engine.add_memory(memory_config(9001, 22)));
        NativeEndpoint second = codec_endpoint();
        second.src_mac[5] = 3;
        Status status;
        if (qualified_first) {
            ASSERT_TRUE(engine.add_native_qp(make_qp(6001, 11),
                                              transport_config(codec_endpoint()), binding(1001)));
            std::unique_ptr<EngineTransport> legacy(new NativeRcEngineTransport(
                transport_config(second), engine_config().max_completion_records));
            EXPECT_FALSE(engine.add_qp(make_qp(90001, 22), std::move(legacy), binding(9001), &status));
        } else {
            std::unique_ptr<EngineTransport> legacy(new NativeRcEngineTransport(
                transport_config(codec_endpoint()), engine_config().max_completion_records));
            ASSERT_TRUE(engine.add_qp(make_qp(6001, 11), std::move(legacy), binding(1001)));
            EXPECT_FALSE(engine.add_native_qp(make_qp(90001, 22),
                                               transport_config(second), binding(9001), &status));
        }
        EXPECT_TRUE(ErrorCode::DUPLICATE == status.code());
        ASSERT_TRUE(engine.start());
        deliver(engine, incoming_write(codec_endpoint(), 0xaa));
        EXPECT_TRUE(std::vector<uint8_t>(8, 0xaa) == read_memory(engine, 1001));
        EXPECT_TRUE(std::vector<uint8_t>(8, 0) == read_memory(engine, 9001));
    }
}

} // namespace engine_device_tests
