/* Copyright (c) 2026 Cisco Systems, Inc.
 * Licensed under the Apache License, Version 2.0. See LICENSE.
 * Standalone wire exerciser for the production native RC endpoint engine.
 * AF_PACKET sends real Ethernet frames; UDP is an explicitly framed carrier
 * for hosts without raw-socket privileges. Neither mode measures DPDK speed.
 */
#include "trex_rocev2_native_engine.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;

namespace {
volatile sig_atomic_t stopped = 0;
void stop_handler(int) { stopped = 1; }

uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void check(bool result, const Status &status, const char *operation) {
    if (!result) {
        throw std::runtime_error(std::string(operation) + ": " + status.message());
    }
}

struct Options {
    bool requester = false;
    std::string interface;
    std::string fault = "none";
    uint16_t local_port = 0;
    uint16_t peer_port = 0;
    uint32_t payload_bytes = 65549;
    uint32_t deadline_ms = 5000;
};

static const uint32_t WIRE_MAX_FRAMES = 256;
static const uint16_t WIRE_PMTU = 1024;

uint32_t integer(const std::string &value, uint32_t maximum) {
    size_t consumed = 0;
    const unsigned long n = std::stoul(value, &consumed);
    if (consumed != value.size() || n == 0 || n > maximum) {
        throw std::runtime_error("numeric argument outside supported range");
    }
    return n;
}

Options options(int argc, char **argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string key(argv[i]);
        if (key == "--requester") { result.requester = true; continue; }
        if (++i == argc) { throw std::runtime_error("missing option value"); }
        const std::string value(argv[i]);
        if (key == "--interface") { result.interface = value; }
        else if (key == "--local-port") { result.local_port = integer(value, 65535); }
        else if (key == "--peer-port") { result.peer_port = integer(value, 65535); }
        else if (key == "--bytes") {
            result.payload_bytes = integer(value, WIRE_MAX_FRAMES * WIRE_PMTU);
        }
        else if (key == "--deadline-ms") { result.deadline_ms = integer(value, 60000); }
        else if (key == "--fault") { result.fault = value; }
        else { throw std::runtime_error("unknown option " + key); }
    }
    if (result.interface.empty() && (!result.local_port || !result.peer_port)) {
        throw std::runtime_error("provide --interface or both UDP carrier ports");
    }
    if (result.fault != "none" && result.fault != "drop" &&
        result.fault != "corrupt" && result.fault != "malformed" &&
        result.fault != "blackhole") {
        throw std::runtime_error("unknown fault scenario");
    }
    return result;
}

class Wire {
public:
    explicit Wire(const Options &opts) : m_raw(!opts.interface.empty()) {
        m_fd = socket(m_raw ? AF_PACKET : AF_INET,
                      m_raw ? SOCK_RAW : SOCK_DGRAM,
                      m_raw ? htons(ETH_P_IP) : 0);
        if (m_fd < 0) { fail("socket"); }
        if (m_raw) {
            sockaddr_ll address = {};
            address.sll_family = AF_PACKET;
            address.sll_protocol = htons(ETH_P_IP);
            address.sll_ifindex = if_nametoindex(opts.interface.c_str());
            if (!address.sll_ifindex || bind(m_fd, reinterpret_cast<sockaddr *>(&address),
                                             sizeof(address))) { fail("bind interface"); }
            m_packet_peer = address;
            m_packet_peer.sll_halen = ETH_ALEN;
            const uint8_t peer_mac[6] = {2, 0, 0, 0, 0,
                                         static_cast<uint8_t>(opts.requester ? 20 : 10)};
            memcpy(m_packet_peer.sll_addr, peer_mac, 6);
        } else {
            sockaddr_in address = {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(opts.local_port);
            if (bind(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address))) {
                fail("bind UDP carrier");
            }
            m_udp_peer = address;
            m_udp_peer.sin_port = htons(opts.peer_port);
        }
        if (fcntl(m_fd, F_SETFL, O_NONBLOCK) < 0) { fail("nonblocking socket"); }
    }
    ~Wire() { if (m_fd >= 0) { close(m_fd); } }
    Wire(const Wire &) = delete;
    Wire &operator=(const Wire &) = delete;

    bool send(const uint8_t *bytes, size_t length) {
        const sockaddr *peer = m_raw ? reinterpret_cast<const sockaddr *>(&m_packet_peer) :
                                      reinterpret_cast<const sockaddr *>(&m_udp_peer);
        const socklen_t size = m_raw ? sizeof(m_packet_peer) : sizeof(m_udp_peer);
        const ssize_t sent = sendto(m_fd, bytes, length, 0, peer, size);
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)) {
            return false;
        }
        if (sent < 0 || static_cast<size_t>(sent) != length) { fail("send frame"); }
        return true;
    }

    ssize_t receive(uint8_t *bytes, size_t capacity) {
        sockaddr_storage source = {};
        socklen_t source_length = sizeof(source);
        const ssize_t length = recvfrom(m_fd, bytes, capacity, 0,
                                         reinterpret_cast<sockaddr *>(&source), &source_length);
        if (length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { return 0; }
        if (length < 0) { fail("receive frame"); }
        if (m_raw) {
            const sockaddr_ll *packet = reinterpret_cast<const sockaddr_ll *>(&source);
            // Reflected egress is not peer ingress on AF_PACKET.
            if (packet->sll_pkttype == PACKET_OUTGOING ||
                packet->sll_pkttype == PACKET_LOOPBACK) { return 0; }
        } else {
            const sockaddr_in *udp = reinterpret_cast<const sockaddr_in *>(&source);
            if (udp->sin_addr.s_addr != m_udp_peer.sin_addr.s_addr ||
                udp->sin_port != m_udp_peer.sin_port) { return 0; }
        }
        return length;
    }
    void wait() { pollfd fd = {m_fd, POLLIN, 0}; ::poll(&fd, 1, 1); }

private:
    [[noreturn]] void fail(const char *what) {
        const int saved_errno = errno;
        if (m_fd >= 0) { close(m_fd); m_fd = -1; }
        throw std::runtime_error(std::string(what) + ": " + strerror(saved_errno));
    }
    int m_fd = -1;
    bool m_raw;
    sockaddr_ll m_packet_peer = {};
    sockaddr_in m_udp_peer = {};
};

const uint64_t LEFT_VA = 0x100000000ULL;
const uint64_t RIGHT_VA = 0x200000000ULL;
const uint32_t LEFT_RKEY = 0x11223344;
const uint32_t RIGHT_RKEY = 0x55667788;

MemoryAccess memory_access() {
    return MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
           MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
}

std::vector<uint8_t> pattern(size_t length) {
    std::vector<uint8_t> result(length);
    uint32_t seed = 0x9e3779b9;
    for (size_t i = 0; i < length; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        result[i] = static_cast<uint8_t>(seed ^ i ^ (i >> 8));
    }
    return result;
}

void configure(NativeEndpointEngine &engine, const Options &opts) {
    Status status;
    const bool left = opts.requester;
    EngineConfig config;
    config.generation = SessionGeneration(1);
    config.endpoint_id = left ? 1 : 2;
    config.max_qps = 1;
    config.max_memory_regions = 1;
    config.max_total_memory_bytes = 3ULL * opts.payload_bytes;
    config.max_completion_records = 16;
    config.max_ingress_frames = WIRE_MAX_FRAMES;
    config.max_tx_frames = WIRE_MAX_FRAMES;
    config.ingress_budget_per_poll = 128;
    config.tx_budget_per_poll = 128;
    check(engine.configure(config, &status), status, "configure engine");
    EngineMemoryConfig memory;
    memory.memory_id = 1;
    memory.window.base_address = left ? LEFT_VA : RIGHT_VA;
    memory.window.length = config.max_total_memory_bytes;
    memory.window.rkey = left ? LEFT_RKEY : RIGHT_RKEY;
    memory.window.access = memory_access();
    check(engine.add_memory(memory, &status), status, "add memory");

    std::unique_ptr<QueuePair> qp(new QueuePair(QpId(SessionGeneration(1), 0)));
    QpConfig local;
    local.local_qpn = left ? 0x1000 : 0x2000;
    local.starting_sq_psn = left ? 0x100 : 0x200;
    local.pmtu = WIRE_PMTU;
    local.sq_depth_messages = 4;
    local.sq_depth_packets = 4096;
    local.timeout_exp = 12; // 16.8 ms accommodates scheduler delays, finite retry budget.
    local.retry_count = 3;
    local.rnr_retry_count = 3;
    local.max_rd_atomic = 1;
    check(qp->create(local, memory.window, &status), status, "create QP");
    QpPeerConfig peer;
    peer.remote_qpn = left ? 0x2000 : 0x1000;
    peer.starting_psn = left ? 0x200 : 0x100;
    peer.pmtu = WIRE_PMTU;
    peer.rkey = left ? RIGHT_RKEY : LEFT_RKEY;
    peer.virtual_address = left ? RIGHT_VA : LEFT_VA;
    peer.window_size = memory.window.length;
    peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE;
    peer.max_rd_atomic = 1;
    peer.ip = left ? "192.0.2.20" : "192.0.2.10";
    peer.mac = left ? "02:00:00:00:00:14" : "02:00:00:00:00:0a";
    check(qp->modify_to_rtr(peer, &status), status, "QP RTR");
    check(qp->modify_to_rts(&status), status, "QP RTS");
    NativeTransportConfig transport;
    // Legacy isolated recovery runner; production ASTF uses standard response replay.
    transport.legacy_response_acks = true;
    NativeEndpoint &endpoint = transport.endpoint;
    const uint8_t local_mac[6] = {2, 0, 0, 0, 0, static_cast<uint8_t>(left ? 10 : 20)};
    const uint8_t peer_mac[6] = {2, 0, 0, 0, 0, static_cast<uint8_t>(left ? 20 : 10)};
    memcpy(endpoint.src_mac, local_mac, 6);
    memcpy(endpoint.dst_mac, peer_mac, 6);
    inet_pton(AF_INET, left ? "192.0.2.10" : "192.0.2.20", endpoint.src_ip);
    inet_pton(AF_INET, left ? "192.0.2.20" : "192.0.2.10", endpoint.dst_ip);
    endpoint.dscp = 26;
    endpoint.udp_sport = left ? 49162 : 49172;
    endpoint.udp_dport = ROCEV2_UDP_PORT;
    transport.unlimited_receive_credits = true;
    transport.max_tx_frames = WIRE_MAX_FRAMES;
    transport.max_read_cache_entries = 4;
    transport.max_read_cache_bytes = memory.window.length;
    transport.cnp_enabled = false;
    EngineQpBinding binding;
    binding.memory_id = 1;
    binding.peer_endpoint = left ? 2 : 1;
    binding.egress_port = 0;
    binding.peer_ingress_port = 0;
    check(engine.add_native_qp(std::move(qp), transport, binding, &status), status, "add QP");
    check(engine.start(&status), status, "start engine");
}

int run(const Options &opts) {
    Wire wire(opts);
    NativeEndpointEngine engine;
    configure(engine, opts);
    Status status;
    const QpId qp(SessionGeneration(1), 0);
    const std::vector<uint8_t> expected = pattern(opts.payload_bytes);
    const uint64_t start = now_ns();
    unsigned completed = 0;
    bool success = false;
    bool failed_completion = false;
    ErrorCode completion_error = ErrorCode::OK;
    uint64_t tx = 0, rx = 0, dropped = 0, modified = 0;
    if (opts.requester) {
        check(engine.memory(1)->write(LEFT_VA, ByteSpan(expected.data(), expected.size()),
                                      MemoryAccess::LOCAL_WRITE, &status), status, "seed payload");
        EngineWriteRequest request;
        request.source_memory_id = 1;
        request.source_address = LEFT_VA;
        request.length = opts.payload_bytes;
        request.remote_address = RIGHT_VA;
        request.remote_rkey = RIGHT_RKEY;
        PostedWork posted;
        check(engine.post_write(qp, request, now_ns(), posted, &status), status, "post Write");
    }
    std::cout << "{\"ready\":true}" << std::endl;
    uint8_t buffer[16384];
    while (!stopped && now_ns() - start < opts.deadline_ms * 1000000ULL) {
        for (unsigned i = 0; i < 128; ++i) {
            const ssize_t length = wire.receive(buffer, sizeof(buffer));
            if (!length) { break; }
            EngineWireFrame metadata;
            metadata.generation = SessionGeneration(1);
            metadata.source_endpoint = opts.requester ? 2 : 1;
            metadata.destination_endpoint = opts.requester ? 1 : 2;
            metadata.egress_port = 0;
            metadata.ingress_port = 0;
            check(engine.enqueue_ingress_copy(metadata, buffer, length, &status), status, "ingress");
            ++rx;
        }
        EnginePollResult result;
        check(engine.poll(now_ns(), result, &status), status, "poll engine");
        for (unsigned i = 0; i < 128; ++i) {
            const EngineWireFrame *frame = engine.peek_tx();
            if (!frame) { break; }
            bool accepted = true;
            if (opts.requester && (opts.fault == "blackhole" ||
                                  (opts.fault == "drop" && tx == 2))) {
                ++dropped;
            } else if (opts.requester && !modified &&
                       (opts.fault == "corrupt" || opts.fault == "malformed")) {
                std::vector<uint8_t> changed(frame->bytes.data(),
                                              frame->bytes.data() + frame->bytes.size());
                if (opts.fault == "corrupt") {
                    // IPv4 UDP checksum zero is legal. Isolate ICRC rejection.
                    changed[40] = changed[41] = 0;
                    changed.back() ^= 1;
                } else {
                    changed.resize(18);
                }
                accepted = wire.send(changed.data(), changed.size());
                if (accepted) { ++modified; }
            } else {
                accepted = wire.send(frame->bytes.data(), frame->bytes.size());
            }
            if (!accepted) { break; }
            const uint64_t sequence = frame->sequence;
            check(engine.accept_tx(sequence, now_ns(), &status), status, "accept TX");
            ++tx;
        }
        std::vector<CompletionRecord> completions;
        check(engine.drain_completions(qp, completions, &status), status, "drain CQ");
        for (const auto &completion : completions) {
            if (completion.state != CompletionState::SUCCESS) {
                failed_completion = true;
                completion_error = completion.error;
                break;
            }
            if (completion.completed_bytes != opts.payload_bytes) {
                throw std::runtime_error("completion byte count mismatch");
            }
            ++completed;
            if (completed == 1) {
                if (completion.kind != WorkKind::WRITE) { throw std::runtime_error("expected Write CQ"); }
                EngineReadRequest read;
                read.destination_memory_id = 1;
                read.destination_address = LEFT_VA + opts.payload_bytes;
                read.length = opts.payload_bytes;
                read.remote_address = RIGHT_VA;
                read.remote_rkey = RIGHT_RKEY;
                PostedWork posted;
                check(engine.post_read(qp, read, now_ns(), posted, &status), status, "post Read");
            } else if (completed == 2) {
                if (completion.kind != WorkKind::READ) { throw std::runtime_error("expected Read CQ"); }
                std::vector<uint8_t> actual;
                check(engine.memory(1)->read(LEFT_VA + opts.payload_bytes, opts.payload_bytes,
                                             MemoryAccess::LOCAL_READ, actual, &status), status, "readback");
                if (actual != expected) { throw std::runtime_error("readback payload mismatch"); }
                success = true;
            }
        }
        if (opts.requester && (success || failed_completion)) { break; }
        wire.wait();
    }
    bool remote_payload = false;
    if (!opts.requester) {
        std::vector<uint8_t> actual;
        check(engine.memory(1)->read(RIGHT_VA, opts.payload_bytes,
                                     MemoryAccess::LOCAL_READ, actual, &status), status, "responder memory");
        remote_payload = actual == expected;
        success = opts.fault == "blackhole" ?
            actual == std::vector<uint8_t>(opts.payload_bytes, 0) : remote_payload;
    } else if (opts.fault == "blackhole") {
        success = failed_completion && completion_error == ErrorCode::RETRY_EXHAUSTED && completed == 0;
    }
    const EngineSnapshot snapshot = engine.snapshot();
    const NativeTransportCounters &counters = snapshot.qps.at(0).transport.native.counters;
    std::cout << "{\"success\":" << (success ? "true" : "false")
              << ",\"role\":\"" << (opts.requester ? "requester" : "responder")
              << "\",\"backend\":\"" << (opts.interface.empty() ? "udp-carrier" : "af-packet")
              << "\",\"fault\":\"" << opts.fault << "\",\"payload_bytes\":" << opts.payload_bytes
              << ",\"completions\":" << completed << ",\"completion_error\":\""
              << error_code_to_string(completion_error) << "\",\"tx_frames\":" << tx
              << ",\"rx_frames\":" << rx << ",\"dropped_frames\":" << dropped
              << ",\"modified_frames\":" << modified
              << ",\"retransmit_packets\":" << counters.retransmit_packets
              << ",\"read_requests_received\":" << counters.read_requests_received
              << ",\"malformed_frames\":" << snapshot.counters.ingress_malformed + counters.malformed_packets
              << ",\"icrc_errors\":" << counters.icrc_errors
              << ",\"remote_payload_verified\":" << (remote_payload ? "true" : "false")
              << ",\"elapsed_ms\":" << (now_ns() - start) / 1000000 << "}" << std::endl;
    return success ? 0 : 1;
}
} // namespace

int main(int argc, char **argv) {
    signal(SIGTERM, stop_handler);
    signal(SIGINT, stop_handler);
    try { return run(options(argc, argv)); }
    catch (const std::exception &error) {
        std::cerr << "RoCEv2 wire exerciser: " << error.what() << std::endl;
        return 2;
    }
}
