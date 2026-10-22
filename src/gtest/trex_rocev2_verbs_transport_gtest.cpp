/*
Copyright (c) 2026 Cisco Systems, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
*/

#include <common/gtest.h>
#include "stx/stl/trex_rocev2_native_engine.h"
#include <string.h>
#include <memory>

namespace {
using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;

class RoceVerbs : public ::testing::Test {
protected:
    NativeMemoryBuffer memory[2];
    std::unique_ptr<QueuePair> qp[2];
    std::unique_ptr<NativeRcTransport> transport[2];
    NativeTransportOutput output[2];
    NativeTransportConfig transport_config[2];
    std::vector<CompletionRecord> cq[2];
    uint64_t now;
    Status status;

    static uint64_t base(int side) { return 0x10000ULL + side * 0x10000ULL; }
    static uint32_t key(int side) { return 0x1234U + side; }
    static MemoryAccess access() {
        return MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
               MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
    }
    void SetUp() {
        now = 1;
        for (int side = 0; side < 2; ++side) {
            EngineMemoryConfig mc;
            mc.memory_id = side + 1;
            mc.window.base_address = base(side);
            mc.window.length = 4096;
            mc.window.rkey = key(side);
            mc.window.access = access();
            ASSERT_TRUE(memory[side].configure(mc, 4096, &status)) << status.message();
            qp[side].reset(new QueuePair(QpId(SessionGeneration(1), side)));
            QpConfig qc;
            qc.local_qpn = side + 2;
            qc.starting_sq_psn = side ? 0xfffffe : 0xfffffc;
            qc.pmtu = 256;
            qc.sq_depth_messages = 32;
            qc.sq_depth_packets = 64;
            qc.rq_depth_messages = 16;
            qc.timeout_exp = 0;
            qc.rnr_retry_count = 7;
            ASSERT_TRUE(qp[side]->create(qc, mc.window, &status)) << status.message();
            QpPeerConfig peer;
            peer.remote_qpn = 3 - side;
            peer.starting_psn = side ? 0xfffffc : 0xfffffe;
            peer.pmtu = 256;
            peer.rkey = key(1 - side);
            peer.virtual_address = base(1 - side);
            peer.window_size = 4096;
            peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
            ASSERT_TRUE(qp[side]->modify_to_rtr(peer, &status)) << status.message();
            ASSERT_TRUE(qp[side]->modify_to_rts(&status)) << status.message();
            NativeTransportConfig tc;
            tc.endpoint.src_mac[0] = tc.endpoint.dst_mac[0] = 2;
            tc.endpoint.src_mac[5] = side + 1;
            tc.endpoint.dst_mac[5] = 2 - side;
            tc.endpoint.src_ip[0] = tc.endpoint.dst_ip[0] = 192;
            tc.endpoint.src_ip[1] = tc.endpoint.dst_ip[1] = 0;
            tc.endpoint.src_ip[2] = tc.endpoint.dst_ip[2] = 2;
            tc.endpoint.src_ip[3] = side + 1;
            tc.endpoint.dst_ip[3] = 2 - side;
            tc.endpoint.udp_sport = 50000 + side;
            tc.signal_mode = NativeTransportSignalMode::PLAIN_LAST;
            tc.require_receive_wqes = true;
            tc.legacy_response_acks = false;
            tc.max_tx_frames = 64;
            tc.max_read_cache_entries = 16;
            tc.max_read_cache_bytes = 4096;
            tc.max_staged_write_bytes = 4096;
            tc.cnp_enabled = false;
            transport_config[side] = tc;
            transport[side].reset(new NativeRcTransport(*qp[side]));
            ASSERT_TRUE(transport[side]->configure(tc, &status)) << status.message();
            ASSERT_TRUE(transport[side]->bind_memory(&memory[side], &status)) << status.message();
        }
    }
    void collect(int side) {
        cq[side].insert(cq[side].end(), output[side].completions.begin(), output[side].completions.end());
        output[side].completions.clear();
    }
    bool deliver(int side, NativeTransportFrame frame) {
        DecodedPacket decoded;
        CodecError error;
        if (!NativePacketCodec::decode(ByteSpan(frame.bytes.data(), frame.bytes.size()), decoded, &error)) {
            ADD_FAILURE() << error.message;
            return false;
        }
        const bool requester = decoded.opcode <= IB_RDMA_READ_REQUEST || decoded.opcode == IB_COMPARE_SWAP ||
                               decoded.opcode == IB_FETCH_ADD || decoded.opcode == IB_SEND_LAST_WITH_INVALIDATE ||
                               decoded.opcode == IB_SEND_ONLY_WITH_INVALIDATE;
        if (requester && !transport[side]->on_frame_accepted(frame.psn, now, &status)) return false;
        return transport[1 - side]->on_frame(decoded, ByteSpan(frame.bytes.data(), frame.bytes.size()),
                                             now, output[1 - side], &status);
    }
    void pump() {
        for (int count = 0; count < 200; ++count) {
            bool progress = false;
            for (int side = 0; side < 2; ++side) {
                std::vector<NativeTransportFrame> frames;
                frames.swap(output[side].frames);
                collect(side);
                for (size_t i = 0; i < frames.size(); ++i) {
                    ASSERT_TRUE(deliver(side, frames[i])) << status.message();
                    progress = true;
                }
            }
            collect(0); collect(1);
            if (!progress && !transport[0]->has_pending_work() && !transport[1]->has_pending_work()) return;
            now += 1000;
            ASSERT_TRUE(transport[0]->poll(now, 64, output[0], &status)) << status.message();
            ASSERT_TRUE(transport[1]->poll(now, 64, output[1], &status)) << status.message();
        }
        GTEST_FAIL() << "verbs fixture failed to quiesce";
    }
    NativeReceiveRequest receive(uint64_t id, size_t bytes = 1024) {
        NativeReceiveRequest request;
        request.wr_id = id;
        request.sges.push_back(NativeScatterGatherEntry(base(1), bytes / 2, key(1)));
        request.sges.push_back(NativeScatterGatherEntry(base(1) + 2048, bytes - bytes / 2, key(1)));
        return request;
    }
};

TEST_F(RoceVerbs, SendSegmentsAcrossSgesWithImmediateAndDuplicateLast) {
    NativeReceiveRequest recv = receive(7001, 700);
    ASSERT_TRUE(transport[1]->post_receive(recv, &status)) << status.message();
    std::vector<uint8_t> payload(600);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = i * 13U;
    NativeTransportSendRequest send;
    send.with_immediate = true;
    send.immediate_data = 0x12345678;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(payload.data(), payload.size()), now, posted, output[0], &status)) << status.message();
    ASSERT_EQ(3U, output[0].frames.size());
    output[0].frames.push_back(output[0].frames.back());
    pump();
    ASSERT_EQ(1U, cq[0].size());
    ASSERT_EQ(1U, cq[1].size());
    EXPECT_TRUE(WorkKind::SEND == cq[0][0].kind);
    EXPECT_EQ(7001U, cq[1][0].wr_id);
    EXPECT_EQ(600U, cq[1][0].completed_bytes);
    EXPECT_TRUE(cq[1][0].has_immediate);
    EXPECT_EQ(0x12345678U, cq[1][0].immediate_data);
    ByteSpan left, right;
    ASSERT_TRUE(memory[1].view(base(1), 350, MemoryAccess::LOCAL_READ, left));
    ASSERT_TRUE(memory[1].view(base(1) + 2048, 250, MemoryAccess::LOCAL_READ, right));
    EXPECT_EQ(0, memcmp(left.data, payload.data(), 350));
    EXPECT_EQ(0, memcmp(right.data, payload.data() + 350, 250));
    EXPECT_EQ(1U, transport[1]->snapshot().counters.deliveries);
}

TEST_F(RoceVerbs, RnrUntilReceiveIsPostedDoesNotAdvancePsnOrConsumeReceive) {
    NativeTransportSendRequest send;
    const uint8_t payload[] = {1, 2, 3};
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(payload, sizeof(payload)), now, posted, output[0], &status));
    NativeTransportFrame request = output[0].frames.front();
    output[0].frames.clear();
    const uint32_t expected = qp[1]->runtime_view().expected_rx_psn;
    ASSERT_TRUE(deliver(0, request));
    EXPECT_EQ(expected, qp[1]->runtime_view().expected_rx_psn);
    EXPECT_EQ(1U, transport[1]->snapshot().counters.rnr_naks_sent);
    ASSERT_TRUE(transport[1]->post_receive(receive(9001), &status));
    ASSERT_TRUE(deliver(0, request));
    output[1].frames.erase(output[1].frames.begin()); // drop deliberately generated RNR, retain successful ACK
    pump();
    ASSERT_EQ(1U, cq[1].size());
    EXPECT_EQ(9001U, cq[1][0].wr_id);
}

TEST_F(RoceVerbs, WriteImmediateUsesReceiveDescriptorAndPlainWriteDoesNot) {
    ASSERT_TRUE(transport[1]->post_receive(receive(8001), &status));
    NativeTransportWriteRequest write;
    write.remote_address = base(1) + 512;
    write.remote_rkey = key(1);
    const uint8_t bytes[] = {8, 9, 10};
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_write(write, ByteSpan(bytes, sizeof(bytes)), now, posted, output[0], &status));
    pump();
    EXPECT_TRUE(cq[1].empty());
    write.with_immediate = true;
    write.immediate_data = 0xff00ee11;
    ASSERT_TRUE(transport[0]->post_write(write, ByteSpan(bytes, sizeof(bytes)), now, posted, output[0], &status));
    pump();
    ASSERT_EQ(1U, cq[1].size());
    EXPECT_TRUE(WorkKind::RECEIVE_RDMA_WITH_IMMEDIATE == cq[1][0].kind);
    EXPECT_EQ(8001U, cq[1][0].wr_id);
    EXPECT_EQ(0xff00ee11U, cq[1][0].immediate_data);
}

TEST_F(RoceVerbs, SendInvalidatesRemoteKeyWhilePreservingPostedLocalKey) {
    ASSERT_TRUE(transport[1]->post_receive(receive(2), &status));
    NativeTransportSendRequest send;
    send.with_invalidate = true;
    send.invalidate_rkey = key(1);
    const uint8_t bytes[] = {4, 5};
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(bytes, sizeof(bytes)), now, posted, output[0], &status));
    pump();
    ASSERT_EQ(1U, cq[1].size());
    EXPECT_TRUE(cq[1][0].has_invalidate);
    EXPECT_FALSE(memory[1].window().remote_key_active);
    EXPECT_TRUE(memory[1].validate_local_key(key(1), 1, base(1), 100, &status));
    EXPECT_FALSE(memory[1].validate_remote_access(key(1), base(1), 8, MemoryAccess::REMOTE_ATOMIC, &status));
}

TEST_F(RoceVerbs, CompareSwapAndFetchAddReplayMutateMemoryExactlyOnce) {
    uint64_t value = 41;
    ASSERT_TRUE(memory[1].write(base(1) + 64, ByteSpan(reinterpret_cast<uint8_t *>(&value), 8), MemoryAccess::LOCAL_WRITE));
    NativeTransportAtomicRequest atomic;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    atomic.compare = 41;
    atomic.swap_add = 99;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status)) << status.message();
    output[0].frames.push_back(output[0].frames.front());
    pump();
    ASSERT_EQ(1U, cq[0].size());
    EXPECT_EQ(41U, cq[0][0].atomic_original_value);
    ByteSpan view;
    ASSERT_TRUE(memory[1].view(base(1) + 64, 8, MemoryAccess::LOCAL_READ, view));
    memcpy(&value, view.data, 8);
    EXPECT_EQ(99U, value);
    atomic.fetch_add = true;
    atomic.compare = 0;
    atomic.swap_add = 7;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status)) << status.message();
    output[0].frames.push_back(output[0].frames.front());
    pump();
    ASSERT_EQ(2U, cq[0].size());
    EXPECT_EQ(99U, cq[0][1].atomic_original_value);
    ASSERT_TRUE(memory[1].view(base(1) + 64, 8, MemoryAccess::LOCAL_READ, view));
    memcpy(&value, view.data, 8);
    EXPECT_EQ(106U, value);
    EXPECT_EQ(2U, transport[1]->snapshot().counters.atomic_requests_received);
    EXPECT_EQ(2U, transport[1]->snapshot().counters.atomic_replay_hits);
}

TEST_F(RoceVerbs, FenceWaitsForAtomicResultAndWrongReceiveKeyIsRejected) {
    NativeTransportAtomicRequest atomic;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    NativeTransportSendRequest send;
    send.fenced = true;
    EXPECT_FALSE(transport[0]->post_send(send, ByteSpan(NULL, 0), now, posted, output[0], &status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == status.code());
    NativeReceiveRequest wrong = receive(4);
    wrong.sges[1].lkey ^= 0xff;
    EXPECT_FALSE(transport[1]->post_receive(wrong, &status));
    EXPECT_TRUE(ErrorCode::ACCESS_DENIED == status.code());
    EXPECT_EQ(0U, transport[1]->runtime_view().posted_receives);
    pump();
    ASSERT_TRUE(transport[1]->post_receive(receive(4), &status));
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(NULL, 0), now, posted, output[0], &status));
    pump();
    EXPECT_EQ(2U, cq[0].size());
}

TEST_F(RoceVerbs, AbortFlushesPostedReceivesWithApplicationIds) {
    ASSERT_TRUE(transport[1]->post_receive(receive(71), &status));
    ASSERT_TRUE(transport[1]->post_receive(receive(72), &status));
    transport[1]->abort(ErrorCode::ABORTED, "test stop");
    std::vector<CompletionRecord> records;
    ASSERT_TRUE(transport[1]->drain_completions(records, &status)) << status.message();
    ASSERT_EQ(2U, records.size());
    EXPECT_EQ(71U, records[0].wr_id);
    EXPECT_EQ(72U, records[1].wr_id);
    EXPECT_TRUE(CompletionState::FLUSHED == records[0].state);
}


TEST_F(RoceVerbs, AtomicResponseLossReplaysOriginalWithoutRepeatingMutation) {
    uint64_t value = 5;
    ASSERT_TRUE(memory[1].write(base(1) + 64, ByteSpan(reinterpret_cast<uint8_t *>(&value), 8), MemoryAccess::LOCAL_WRITE));
    NativeTransportAtomicRequest atomic;
    atomic.fetch_add = true;
    atomic.swap_add = 3;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    NativeTransportFrame request = output[0].frames.front();
    output[0].frames.clear();
    ASSERT_TRUE(deliver(0, request));
    ASSERT_EQ(1U, output[1].frames.size());
    output[1].frames.clear(); // lost atomic ACK, before requester received result
    now = 5000;
    ASSERT_TRUE(transport[0]->poll(now, 64, output[0], &status));
    ASSERT_EQ(1U, output[0].frames.size());
    EXPECT_TRUE(output[0].frames[0].retransmission);
    pump();
    ASSERT_EQ(1U, cq[0].size());
    EXPECT_EQ(5U, cq[0][0].atomic_original_value);
    ByteSpan view;
    ASSERT_TRUE(memory[1].view(base(1) + 64, 8, MemoryAccess::LOCAL_READ, view));
    memcpy(&value, view.data, 8);
    EXPECT_EQ(8U, value);
    EXPECT_EQ(1U, transport[1]->snapshot().counters.atomic_requests_received);
}

TEST_F(RoceVerbs, ReadThenAtomicResponsesCompleteInSqOrderEvenWhenReordered) {
    uint64_t value = 17;
    ASSERT_TRUE(memory[1].write(base(1) + 64, ByteSpan(reinterpret_cast<uint8_t *>(&value), 8), MemoryAccess::LOCAL_WRITE));
    NativeTransportReadRequest read;
    read.remote_address = base(1) + 64;
    read.remote_rkey = key(1);
    read.local_address = base(0);
    read.length = 8;
    PostedWork read_work, atomic_work;
    ASSERT_TRUE(transport[0]->post_read(read, now, read_work, output[0], &status));
    NativeTransportAtomicRequest atomic;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, atomic_work, output[0], &status));
    ASSERT_EQ(2U, output[0].frames.size());
    std::vector<NativeTransportFrame> requests;
    requests.swap(output[0].frames);
    ASSERT_TRUE(deliver(0, requests[0]));
    ASSERT_TRUE(deliver(0, requests[1]));
    ASSERT_EQ(2U, output[1].frames.size());
    std::vector<NativeTransportFrame> responses;
    responses.swap(output[1].frames);
    ASSERT_TRUE(deliver(1, responses[1]));
    EXPECT_TRUE(output[0].completions.empty());
    ASSERT_TRUE(deliver(1, responses[0]));
    collect(0);
    ASSERT_EQ(2U, cq[0].size());
    EXPECT_TRUE(cq[0][0].id == read_work.id);
    EXPECT_TRUE(cq[0][1].id == atomic_work.id);
    EXPECT_EQ(17U, cq[0][1].atomic_original_value);
    pump();
}

TEST_F(RoceVerbs, OversizedSendReportsLocalReceiveErrorAndFlushesRemainingWqes) {
    ASSERT_TRUE(transport[1]->post_receive(receive(20, 2), &status));
    ASSERT_TRUE(transport[1]->post_receive(receive(21), &status));
    const uint8_t bytes[] = {1, 2, 3};
    NativeTransportSendRequest send;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(bytes, 3), now, posted, output[0], &status));
    NativeTransportFrame request = output[0].frames.front();
    output[0].frames.clear();
    ASSERT_TRUE(deliver(0, request)) << status.message();
    collect(1);
    ASSERT_EQ(2U, cq[1].size());
    EXPECT_EQ(20U, cq[1][0].wr_id);
    EXPECT_TRUE(CompletionState::ERROR == cq[1][0].state);
    EXPECT_EQ(21U, cq[1][1].wr_id);
    EXPECT_TRUE(CompletionState::FLUSHED == cq[1][1].state);
    EXPECT_TRUE(QpState::ERR == qp[1]->state());
}

TEST_F(RoceVerbs, AtomicPermissionAndAlignmentAreCheckedBeforeMutation) {
    NativeMemoryBuffer restricted;
    EngineMemoryConfig config;
    config.memory_id = 5;
    config.window.base_address = 0x50000;
    config.window.length = 64;
    config.window.rkey = 77;
    config.window.access = MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE | MemoryAccess::REMOTE_WRITE;
    config.initial_value = 0x55;
    ASSERT_TRUE(restricted.configure(config, 64, &status));
    uint64_t original = 0;
    EXPECT_FALSE(restricted.atomic(0x50000, 77, true, 0, 1, original, &status));
    EXPECT_TRUE(ErrorCode::ACCESS_DENIED == status.code());
    EXPECT_FALSE(memory[1].atomic(base(1) + 1, key(1), true, 0, 1, original, &status));
    EXPECT_TRUE(ErrorCode::INVALID_ARGUMENT == status.code());
    ByteSpan view;
    ASSERT_TRUE(restricted.view(0x50000, 64, MemoryAccess::LOCAL_READ, view));
    for (size_t i = 0; i < view.size; ++i) EXPECT_EQ(0x55U, view.data[i]);
}

TEST_F(RoceVerbs, RegistryResolvesCrossRegionSgesAndRejectsDuplicateKeys) {
    NativeEndpointEngine registry;
    EngineConfig config;
    config.generation = SessionGeneration(1);
    config.endpoint_id = 1;
    config.max_qps = 4;
    config.max_memory_regions = 4;
    config.max_total_memory_bytes = 16384;
    config.max_ingress_frames = config.max_tx_frames = 16;
    config.max_frame_bytes = 512;
    config.ingress_budget_per_poll = config.tx_budget_per_poll = 16;
    ASSERT_TRUE(registry.configure(config, &status)) << status.message();
    EngineMemoryConfig first;
    first.memory_id = 10;
    first.window.base_address = base(1);
    first.window.length = 4096;
    first.window.rkey = key(1);
    first.window.access = access();
    ASSERT_TRUE(registry.add_memory(first, &status));
    EngineMemoryConfig second = first;
    second.memory_id = 11;
    second.window.base_address += 8192;
    EXPECT_FALSE(registry.add_memory(second, &status));
    EXPECT_TRUE(ErrorCode::DUPLICATE == status.code());
    second.window.rkey += 10;
    second.window.lkey = second.window.rkey;
    ASSERT_TRUE(registry.add_memory(second, &status));
    NativeTransportConfig native;
    native.max_tx_frames = 16;
    native.max_staged_write_bytes = native.max_read_cache_bytes = 4096;
    native.max_read_cache_entries = 16;
    native.require_receive_wqes = true;
    native.signal_mode = NativeTransportSignalMode::PLAIN_LAST;
    native.cnp_enabled = false;
    NativeTransportSendRequest send;
    const uint8_t bytes[] = {1, 2, 3, 4, 5, 6};
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_send(send, ByteSpan(bytes, 6), now, posted, output[0], &status));
    DecodedPacket incoming;
    ASSERT_TRUE(NativePacketCodec::decode(ByteSpan(output[0].frames[0].bytes.data(), output[0].frames[0].bytes.size()), incoming));
    native.endpoint = incoming.endpoint;
    std::swap_ranges(native.endpoint.src_mac, native.endpoint.src_mac + 6, native.endpoint.dst_mac);
    std::swap_ranges(native.endpoint.src_ip, native.endpoint.src_ip + 16, native.endpoint.dst_ip);
    native.endpoint.udp_sport = 50001;
    NativeRcEngineTransport adapter(native, 64);
    adapter.bind_registry(&registry);
    ASSERT_TRUE(adapter.attach(qp[1].get(), registry.memory(10), &status)) << status.message();
    NativeReceiveRequest recv;
    recv.wr_id = 100;
    recv.sges.push_back(NativeScatterGatherEntry(first.window.base_address, 3, first.window.rkey));
    recv.sges.push_back(NativeScatterGatherEntry(second.window.base_address, 3, second.window.lkey));
    ASSERT_TRUE(adapter.post_receive(recv, &status)) << status.message();
    EngineWireFrame frame;
    frame.bytes = output[0].frames[0].bytes;
    ASSERT_TRUE(adapter.on_frame(frame, incoming, now, &status)) << status.message();
    ByteSpan first_view, second_view;
    ASSERT_TRUE(registry.memory(10)->view(first.window.base_address, 3, MemoryAccess::LOCAL_READ, first_view));
    ASSERT_TRUE(registry.memory(11)->view(second.window.base_address, 3, MemoryAccess::LOCAL_READ, second_view));
    EXPECT_EQ(0, memcmp(bytes, first_view.data, 3));
    EXPECT_EQ(0, memcmp(bytes + 3, second_view.data, 3));
    std::vector<CompletionRecord> records;
    ASSERT_TRUE(adapter.drain_completions(records, &status));
    ASSERT_EQ(1U, records.size());
    EXPECT_EQ(100U, records[0].wr_id);
}

TEST_F(RoceVerbs, EngineSupportsDynamicPassiveRtrInstallAndActivation) {
    NativeEndpointEngine engine;
    EngineConfig config;
    config.generation = SessionGeneration(1);
    config.endpoint_id = 1;
    config.max_qps = 2;
    config.max_memory_regions = 1;
    config.max_total_memory_bytes = 4096;
    config.max_ingress_frames = config.max_tx_frames = 16;
    config.max_frame_bytes = 512;
    config.ingress_budget_per_poll = config.tx_budget_per_poll = 16;
    ASSERT_TRUE(engine.configure(config, &status));
    EngineMemoryConfig mc;
    mc.memory_id = 10;
    mc.window.base_address = base(1);
    mc.window.length = 4096;
    mc.window.rkey = key(1);
    mc.window.access = access();
    ASSERT_TRUE(engine.add_memory(mc, &status));
    ASSERT_TRUE(engine.start(&status));
    std::unique_ptr<QueuePair> passive(new QueuePair(QpId(SessionGeneration(1), 5)));
    QpConfig local = qp[1]->runtime_view().config;
    local.local_qpn = 20;
    ASSERT_TRUE(passive->create(local, mc.window, &status));
    ASSERT_TRUE(passive->modify_to_rtr(qp[1]->snapshot().peer, &status));
    NativeTransportConfig native;
    native.max_tx_frames = 16;
    native.max_staged_write_bytes = native.max_read_cache_bytes = 4096;
    native.max_read_cache_entries = 16;
    native.require_receive_wqes = true;
    native.cnp_enabled = false;
    EngineQpBinding binding;
    binding.memory_id = 10;
    binding.egress_port = 1;
    binding.peer_endpoint = 2;
    binding.peer_ingress_port = 0;
    ASSERT_TRUE(engine.add_native_qp(std::move(passive), native, binding, &status)) << status.message();
    ASSERT_TRUE(engine.post_receive(QpId(SessionGeneration(1), 5), receive(42), &status)) << status.message();
    EXPECT_TRUE(QpState::RTR == engine.queue_pair(QpId(SessionGeneration(1), 5))->state());
    ASSERT_TRUE(engine.activate_qp(QpId(SessionGeneration(1), 5), &status)) << status.message();
    EXPECT_TRUE(QpState::RTS == engine.queue_pair(QpId(SessionGeneration(1), 5))->state());
}


TEST_F(RoceVerbs, SqdRejectsNewWorkButRetiresExistingWorkAndCanResume) {
    NativeTransportAtomicRequest atomic;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    ASSERT_TRUE(qp[0]->modify_to_sqd(&status));
    EXPECT_FALSE(qp[0]->drained());
    EXPECT_FALSE(qp[0]->resume_from_sqd(&status));
    EXPECT_FALSE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    pump();
    EXPECT_TRUE(qp[0]->drained());
    ASSERT_TRUE(qp[0]->resume_from_sqd(&status));
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    pump();
    EXPECT_EQ(2U, cq[0].size());
}

TEST_F(RoceVerbs, RemoteRegistryReadWriteAtomicAndResultTargetSecondRegions) {
    NativeEndpointEngine engines[2];
    for (int side = 0; side < 2; ++side) {
        EngineConfig ec;
        ec.generation = SessionGeneration(1);
        ec.endpoint_id = side + 10;
        ec.max_qps = 1;
        ec.max_memory_regions = 2;
        ec.max_total_memory_bytes = 8192;
        ec.max_ingress_frames = ec.max_tx_frames = 64;
        ec.max_frame_bytes = 512;
        ec.ingress_budget_per_poll = ec.tx_budget_per_poll = 64;
        ASSERT_TRUE(engines[side].configure(ec, &status)) << status.message();
        EngineMemoryConfig mc;
        mc.memory_id = 1;
        mc.window.base_address = base(side);
        mc.window.length = 4096;
        mc.window.rkey = key(side);
        mc.window.access = access();
        ASSERT_TRUE(engines[side].add_memory(mc, &status));
        EngineMemoryConfig second = mc;
        second.memory_id = 2;
        second.window.base_address += 8192;
        second.window.rkey += 10;
        ASSERT_TRUE(engines[side].add_memory(second, &status));
        std::unique_ptr<QueuePair> registered(new QueuePair(QpId(SessionGeneration(1), side)));
        QpConfig qc = qp[side]->runtime_view().config;
        qc.allow_remote_registry = true;
        ASSERT_TRUE(registered->create(qc, mc.window, &status));
        ASSERT_TRUE(registered->modify_to_rtr(qp[side]->snapshot().peer, &status));
        ASSERT_TRUE(registered->modify_to_rts(&status));
        EngineQpBinding binding;
        binding.memory_id = 1;
        binding.peer_endpoint = 11 - side;
        binding.egress_port = side;
        binding.peer_ingress_port = 1 - side;
        ASSERT_TRUE(engines[side].add_native_qp(std::move(registered), transport_config[side], binding, &status)) << status.message();
        ASSERT_TRUE(engines[side].start(&status));
    }
    const auto pump_engines = [&]() {
        for (int loops = 0; loops < 200; ++loops) {
            for (int side = 0; side < 2; ++side) {
                EnginePollResult result;
                ASSERT_TRUE(engines[side].poll(now, result, &status)) << status.message();
                std::vector<EngineWireFrame> frames;
                ASSERT_TRUE(engines[side].drain_tx(64, frames, &status)) << status.message();
                for (size_t i = 0; i < frames.size(); ++i) {
                    frames[i].ingress_port = 1 - side;
                    ASSERT_TRUE(engines[1 - side].enqueue_ingress(std::move(frames[i]), &status)) << status.message();
                }
            }
            if (!engines[0].has_pending_work() && !engines[1].has_pending_work()) return;
            now += 1000;
        }
        GTEST_FAIL() << "registry engine pair did not quiesce";
    };
    uint64_t value = 91;
    ASSERT_TRUE(engines[0].memory(1)->write(base(0), ByteSpan(reinterpret_cast<uint8_t *>(&value), 8), MemoryAccess::LOCAL_WRITE));
    EngineWriteRequest write;
    write.source_memory_id = 1;
    write.source_address = base(0);
    write.length = 8;
    write.remote_address = base(1) + 8192;
    write.remote_rkey = key(1) + 10;
    PostedWork posted;
    const QpId id(SessionGeneration(1), 0);
    ASSERT_TRUE(engines[0].post_write(id, write, now, posted, &status)) << status.message();
    pump_engines();
    ByteSpan actual;
    ASSERT_TRUE(engines[1].memory(2)->view(write.remote_address, 8, MemoryAccess::LOCAL_READ, actual));
    memcpy(&value, actual.data, 8);
    EXPECT_EQ(91U, value);
    EngineReadRequest read;
    read.destination_memory_id = 2;
    read.destination_address = base(0) + 8192;
    read.length = 8;
    read.remote_address = write.remote_address;
    read.remote_rkey = write.remote_rkey;
    ASSERT_TRUE(engines[0].post_read(id, read, now, posted, &status)) << status.message();
    pump_engines();
    ASSERT_TRUE(engines[0].memory(2)->view(read.destination_address, 8, MemoryAccess::LOCAL_READ, actual));
    memcpy(&value, actual.data, 8);
    EXPECT_EQ(91U, value);
    EngineAtomicRequest atomic;
    atomic.destination_memory_id = 2;
    atomic.destination_address = read.destination_address + 16;
    atomic.remote_address = read.remote_address;
    atomic.remote_rkey = read.remote_rkey;
    atomic.compare = 91;
    atomic.swap_add = 100;
    ASSERT_TRUE(engines[0].post_atomic(id, atomic, now, posted, &status)) << status.message();
    pump_engines();
    ASSERT_TRUE(engines[0].memory(2)->view(atomic.destination_address, 8, MemoryAccess::LOCAL_READ, actual));
    memcpy(&value, actual.data, 8);
    EXPECT_EQ(91U, value);
    ASSERT_TRUE(engines[1].memory(2)->view(atomic.remote_address, 8, MemoryAccess::LOCAL_READ, actual));
    memcpy(&value, actual.data, 8);
    EXPECT_EQ(100U, value);
    std::vector<CompletionRecord> records;
    ASSERT_TRUE(engines[0].drain_completions(id, records, &status));
    ASSERT_EQ(3U, records.size());
    EXPECT_EQ(91U, records[2].atomic_original_value);
    ASSERT_TRUE(engines[1].close_qp(QpId(SessionGeneration(1), 1), ErrorCode::ABORTED, "closed", &status));
    EXPECT_TRUE(QpState::ERR == engines[1].queue_pair(QpId(SessionGeneration(1), 1))->state());
    // A late valid write to a closed QP must be dropped before registry access.
    write.source_address += 8;
    ASSERT_TRUE(engines[0].post_write(id, write, now, posted, &status));
    EnginePollResult result;
    ASSERT_TRUE(engines[0].poll(now, result, &status));
    std::vector<EngineWireFrame> late;
    ASSERT_TRUE(engines[0].drain_tx(64, late, &status));
    ASSERT_FALSE(late.empty());
    late[0].ingress_port = 1;
    ASSERT_TRUE(engines[1].enqueue_ingress(std::move(late[0]), &status));
    ASSERT_TRUE(engines[1].poll(now, result, &status));
    ASSERT_TRUE(engines[1].memory(2)->view(atomic.remote_address, 8, MemoryAccess::LOCAL_READ, actual));
    memcpy(&value, actual.data, 8);
    EXPECT_EQ(100U, value);
}

TEST_F(RoceVerbs, ZeroReadAtomicDepthKeepsSendOnlyQpUsable) {
    QueuePair send_only(QpId(SessionGeneration(1), 9));
    QpConfig local = qp[0]->runtime_view().config;
    local.max_rd_atomic = 0;
    local.max_dest_rd_atomic = 0;
    EngineMemoryConfig mc;
    mc.window.base_address = base(0);
    mc.window.length = 4096;
    mc.window.rkey = key(0);
    mc.window.access = access();
    ASSERT_TRUE(send_only.create(local, mc.window, &status));
    QpPeerConfig peer = qp[0]->snapshot().peer;
    peer.max_rd_atomic = 0;
    ASSERT_TRUE(send_only.modify_to_rtr(peer, &status));
    ASSERT_TRUE(send_only.modify_to_rts(&status));
    NativeTransportConfig tc = transport_config[0];
    NativeRcTransport native(send_only);
    ASSERT_TRUE(native.configure(tc, &status));
    ASSERT_TRUE(native.bind_memory(&memory[0], &status));
    NativeTransportOutput outgoing;
    PostedWork posted;
    NativeTransportReadRequest read;
    read.length = 8;
    EXPECT_FALSE(native.post_read(read, now, posted, outgoing, &status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == status.code());
    NativeTransportSendRequest send;
    ASSERT_TRUE(native.post_send(send, ByteSpan(NULL, 0), now, posted, outgoing, &status)) << status.message();
    EXPECT_TRUE(WorkKind::SEND == posted.kind);
}

TEST_F(RoceVerbs, RegistrationRebindRejectsStaleKeyReuseAndPreservesBytes) {
    NativeMemoryBuffer registration;
    EngineMemoryConfig mc;
    mc.memory_id = 1;
    mc.window.base_address = 0x90000;
    mc.window.length = 64;
    mc.window.rkey = 100;
    mc.window.lkey = 101;
    mc.window.access = access();
    mc.initial_value = 0x5a;
    ASSERT_TRUE(registration.configure(mc, 64, &status));
    ASSERT_TRUE(registration.rebind_keys(201, 200, &status));
    EXPECT_FALSE(registration.validate_local_key(101, 1, mc.window.base_address, 8, &status));
    EXPECT_FALSE(registration.validate_remote_access(100, mc.window.base_address, 8, MemoryAccess::REMOTE_READ, &status));
    ASSERT_TRUE(registration.validate_local_key(201, 1, mc.window.base_address, 8, &status));
    EXPECT_FALSE(registration.rebind_keys(101, 100, &status));
    EXPECT_TRUE(ErrorCode::STALE_GENERATION == status.code());
    ByteSpan view;
    ASSERT_TRUE(registration.view(mc.window.base_address, 64, MemoryAccess::LOCAL_READ, view));
    for (size_t i = 0; i < view.size; ++i) EXPECT_EQ(0x5aU, view.data[i]);
}


TEST_F(RoceVerbs, IndependentRequesterAndResponderDepthsRemainAsymmetric) {
    QueuePair receiver(QpId(SessionGeneration(1), 9));
    QpConfig local = qp[1]->runtime_view().config;
    local.max_rd_atomic = 1;
    local.max_dest_rd_atomic = 4;
    MemoryWindowConfig window;
    window.base_address = base(1);
    window.length = 4096;
    window.rkey = key(1);
    window.access = access();
    ASSERT_TRUE(receiver.create(local, window, &status));
    QpPeerConfig peer = qp[1]->snapshot().peer;
    peer.max_rd_atomic = 2;
    ASSERT_TRUE(receiver.modify_to_rtr(peer, &status));
    ASSERT_TRUE(receiver.modify_to_rts(&status));
    EXPECT_EQ(1U, receiver.runtime_view().config.max_rd_atomic);
    EXPECT_EQ(4U, receiver.runtime_view().config.max_dest_rd_atomic);
    NativeRcTransport responder(receiver);
    ASSERT_TRUE(responder.configure(transport_config[1], &status));
    ASSERT_TRUE(responder.bind_memory(&memory[1], &status));
    NativeTransportAtomicRequest atomic;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    PostedWork posted;
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    NativeTransportOutput responses;
    for (size_t i = 0; i < output[0].frames.size(); ++i) {
        DecodedPacket packet;
        const NativeFrameBuffer &frame = output[0].frames[i].bytes;
        ASSERT_TRUE(NativePacketCodec::decode(ByteSpan(frame.data(), frame.size()), packet));
        ASSERT_TRUE(responder.on_frame(packet, ByteSpan(frame.data(), frame.size()), now, responses, &status));
    }
    EXPECT_EQ(2U, responder.snapshot().counters.atomic_requests_received);
    EXPECT_EQ(2U, responder.snapshot().read_cache_entries);
    EXPECT_EQ(0U, responder.snapshot().counters.rnr_naks_sent);
}

TEST_F(RoceVerbs, OppositeAtomicResponseCannotAcknowledgeLostWriteAtSamePsn) {
    const QpConfig original[2] = {qp[0]->runtime_view().config, qp[1]->runtime_view().config};
    const QpPeerConfig peers[2] = {qp[0]->snapshot().peer, qp[1]->snapshot().peer};
    for (int side = 0; side < 2; ++side) {
        transport[side].reset();
        qp[side].reset(new QueuePair(QpId(SessionGeneration(1), side)));
        QpConfig local = original[side];
        local.starting_sq_psn = 100;
        MemoryWindowConfig window;
        window.base_address = base(side);
        window.length = 4096;
        window.rkey = key(side);
        window.access = access();
        ASSERT_TRUE(qp[side]->create(local, window, &status));
        QpPeerConfig peer = peers[side];
        peer.starting_psn = 100;
        ASSERT_TRUE(qp[side]->modify_to_rtr(peer, &status));
        ASSERT_TRUE(qp[side]->modify_to_rts(&status));
        transport[side].reset(new NativeRcTransport(*qp[side]));
        ASSERT_TRUE(transport[side]->configure(transport_config[side], &status));
        ASSERT_TRUE(transport[side]->bind_memory(&memory[side], &status));
    }
    NativeTransportWriteRequest write;
    write.remote_address = base(1);
    write.remote_rkey = key(1);
    const uint8_t bytes[] = {8, 7, 6};
    PostedWork write_work, atomic_work;
    ASSERT_TRUE(transport[0]->post_write(write, ByteSpan(bytes, 3), now, write_work, output[0], &status));
    NativeTransportFrame delayed_write = output[0].frames.front();
    output[0].frames.clear(); // Write is not yet delivered to its peer.
    NativeTransportAtomicRequest atomic;
    atomic.remote_address = base(0) + 64;
    atomic.remote_rkey = key(0);
    atomic.local_address = base(1) + 64;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    ASSERT_TRUE(transport[1]->post_atomic(atomic, now, atomic_work, output[1], &status));
    EXPECT_EQ(write_work.first_psn, atomic_work.first_psn);
    NativeTransportFrame request = output[1].frames.front();
    output[1].frames.clear();
    ASSERT_TRUE(deliver(1, request));
    ASSERT_EQ(1U, output[0].frames.size());
    DecodedPacket response;
    ASSERT_TRUE(NativePacketCodec::decode(ByteSpan(output[0].frames[0].bytes.data(), output[0].frames[0].bytes.size()), response));
    EXPECT_FALSE(response.ack_request);
    NativeTransportFrame atomic_response = output[0].frames.front();
    output[0].frames.clear();
    ASSERT_TRUE(deliver(0, atomic_response));
    EXPECT_TRUE(output[1].frames.empty()); // Atomic response has no requester ACK.
    EXPECT_TRUE(output[0].completions.empty());
    EXPECT_EQ(1U, qp[0]->runtime_view().ledger.outstanding_work);
    ASSERT_TRUE(deliver(0, delayed_write));
    pump();
    ASSERT_EQ(1U, cq[0].size());
    ASSERT_EQ(1U, cq[1].size());
    EXPECT_TRUE(cq[0][0].id == write_work.id);
    EXPECT_TRUE(cq[1][0].id == atomic_work.id);
}

TEST_F(RoceVerbs, LostOldestAtomicRetainsCreditsUntilSqOrderAllowsReplaySlotReuse) {
    NativeTransportAtomicRequest atomic;
    atomic.remote_address = base(1) + 64;
    atomic.remote_rkey = key(1);
    atomic.local_address = base(0) + 64;
    atomic.fetch_add = true;
    atomic.swap_add = 1;
    PostedWork posted;
    for (unsigned i = 0; i < 16; ++i) {
        ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status)) << status.message();
    }
    std::vector<NativeTransportFrame> requests;
    requests.swap(output[0].frames);
    for (size_t i = 0; i < requests.size(); ++i) ASSERT_TRUE(deliver(0, requests[i])) << status.message();
    ASSERT_EQ(16U, output[1].frames.size());
    std::vector<NativeTransportFrame> responses;
    responses.swap(output[1].frames);
    for (size_t i = 1; i < responses.size(); ++i) ASSERT_TRUE(deliver(1, responses[i])) << status.message();
    EXPECT_TRUE(output[0].completions.empty());
    EXPECT_EQ(16U, qp[0]->runtime_view().ledger.outstanding_responses);
    EXPECT_FALSE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    EXPECT_TRUE(ErrorCode::RESOURCE_EXHAUSTED == status.code());
    ASSERT_TRUE(deliver(1, responses[0])) << status.message();
    collect(0);
    ASSERT_EQ(16U, cq[0].size());
    for (size_t i = 0; i < cq[0].size(); ++i) EXPECT_EQ(i, cq[0][i].atomic_original_value);
    EXPECT_EQ(0U, qp[0]->runtime_view().ledger.outstanding_responses);
    ASSERT_TRUE(transport[0]->post_atomic(atomic, now, posted, output[0], &status));
    pump();
    EXPECT_EQ(16U, transport[1]->snapshot().read_cache_entries);
    EXPECT_EQ(17U, transport[1]->snapshot().counters.atomic_requests_received);
    ByteSpan value;
    ASSERT_TRUE(memory[1].view(base(1) + 64, 8, MemoryAccess::LOCAL_READ, value));
    uint64_t final_value = 0;
    memcpy(&final_value, value.data, 8);
    EXPECT_EQ(17U, final_value);
}

TEST_F(RoceVerbs, ClosingOneQpPurgesOnlyItsPublishedTxAndAdmittedIngress) {
    NativeEndpointEngine engine;
    EngineConfig ec;
    ec.generation = SessionGeneration(1);
    ec.endpoint_id = 10;
    ec.max_qps = 2;
    ec.max_memory_regions = 1;
    ec.max_total_memory_bytes = 4096;
    ec.max_ingress_frames = ec.max_tx_frames = 64;
    ec.max_frame_bytes = 512;
    ec.ingress_budget_per_poll = ec.tx_budget_per_poll = 64;
    ASSERT_TRUE(engine.configure(ec, &status));
    EngineMemoryConfig mc;
    mc.memory_id = 1;
    mc.window.base_address = base(0);
    mc.window.length = 4096;
    mc.window.rkey = key(0);
    mc.window.access = access();
    ASSERT_TRUE(engine.add_memory(mc, &status));
    for (unsigned index = 0; index < 2; ++index) {
        std::unique_ptr<QueuePair> owned(new QueuePair(QpId(SessionGeneration(1), index)));
        QpConfig config = qp[0]->runtime_view().config;
        config.local_qpn = 2 + 2 * index;
        config.starting_sq_psn = 100 + 10 * index;
        ASSERT_TRUE(owned->create(config, mc.window, &status));
        QpPeerConfig peer = qp[0]->snapshot().peer;
        peer.remote_qpn = 3 + 2 * index;
        peer.starting_psn = 200 + 10 * index;
        ASSERT_TRUE(owned->modify_to_rtr(peer, &status));
        ASSERT_TRUE(owned->modify_to_rts(&status));
        EngineQpBinding binding;
        binding.memory_id = 1;
        binding.peer_endpoint = 11;
        binding.egress_port = 0;
        binding.peer_ingress_port = 1;
        ASSERT_TRUE(engine.add_native_qp(std::move(owned), transport_config[0], binding, &status)) << status.message();
    }
    ASSERT_TRUE(engine.start(&status));
    EngineWriteRequest request;
    request.source_memory_id = 1;
    request.source_address = base(0);
    request.remote_address = base(1);
    request.remote_rkey = key(1);
    request.length = 8;
    PostedWork posted[2];
    ASSERT_TRUE(engine.post_write(QpId(SessionGeneration(1), 0), request, now, posted[0], &status));
    ASSERT_TRUE(engine.post_write(QpId(SessionGeneration(1), 1), request, now, posted[1], &status));
    EnginePollResult result;
    ASSERT_TRUE(engine.poll(now, result, &status));
    EXPECT_EQ(2U, engine.snapshot().tx_queued);
    for (unsigned index = 0; index < 2; ++index) {
        PacketEncodeSpec ack;
        ack.endpoint = transport_config[1].endpoint;
        ack.opcode = IB_ACKNOWLEDGE;
        ack.has_reth = false;
        ack.dest_qpn = 2 + 2 * index;
        ack.psn = posted[index].terminal_psn;
        ack.has_aeth = true;
        ack.aeth.syndrome = 0;
        ack.aeth.msn = 1;
        std::vector<uint8_t> bytes;
        ASSERT_TRUE(NativePacketCodec::encode(ack, bytes));
        EngineWireFrame metadata;
        metadata.generation = SessionGeneration(1);
        metadata.destination_endpoint = 10;
        metadata.ingress_port = 0;
        ASSERT_TRUE(engine.enqueue_ingress_copy(metadata, bytes.data(), bytes.size(), &status));
    }
    EXPECT_EQ(2U, engine.snapshot().ingress_queued);
    ASSERT_TRUE(engine.close_qp(QpId(SessionGeneration(1), 0), ErrorCode::ABORTED, "disconnect", &status));
    EXPECT_EQ(1U, engine.snapshot().tx_queued);
    EXPECT_EQ(1U, engine.snapshot().ingress_queued);
    ASSERT_TRUE(engine.peek_tx() != NULL);
    EXPECT_EQ(1U, engine.peek_tx()->source_qp_index);
    const uint64_t sequence = engine.peek_tx()->sequence;
    ASSERT_TRUE(engine.accept_tx(sequence, now, &status)) << status.message();
    ASSERT_TRUE(engine.poll(now, result, &status)) << status.message();
    EXPECT_TRUE(EngineState::RUNNING == engine.state());
    EXPECT_EQ(0U, engine.snapshot().tx_queued);
    EXPECT_EQ(0U, engine.snapshot().ingress_queued);
    std::vector<CompletionRecord> records;
    ASSERT_TRUE(engine.drain_completions(QpId(SessionGeneration(1), 1), records, &status));
    ASSERT_EQ(1U, records.size());
    EXPECT_TRUE(CompletionState::SUCCESS == records[0].state);
    EXPECT_TRUE(QpState::ERR == engine.queue_pair(QpId(SessionGeneration(1), 0))->state());
    EXPECT_TRUE(QpState::RTS == engine.queue_pair(QpId(SessionGeneration(1), 1))->state());
}

} // namespace
