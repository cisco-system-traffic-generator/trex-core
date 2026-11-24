/*
 * Copyright (c) 2026 Cisco Systems, Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "trex_astf_rocev2.h"
#include "trex_astf_dp_core.h"
#include "bp_sim.h"
#include "stt_cp.h"
#include "stl/trex_rocev2_native_engine.h"
#include "stl/trex_rocev2_native_cm.h"
#include "stl/trex_rocev2_native_neighbor.h"

#include <arpa/inet.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>

using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;

namespace {
const uint32_t MAX_FRAME = 4352; // 4096-byte PMTU plus Ethernet/IP/RDMA headers.
const size_t BURST = 32;
const uint64_t DEFAULT_NATIVE_STORAGE = 512ULL << 20;
const uint64_t MAX_NATIVE_STORAGE = 16ULL << 30;

uint64_t monotonic_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint64_t number(const Json::Value &v, const char *key, uint64_t fallback,
                uint64_t low, uint64_t high) {
    const Json::Value &entry = v[key];
    if (!v.isMember(key)) { return fallback; }
    if (!entry.isUInt64() || entry.isBool()) {
        throw std::runtime_error(std::string(key) + " must be an unsigned integer");
    }
    const uint64_t value = entry.asUInt64();
    if (value < low || value > high) {
        throw std::runtime_error(std::string(key) + " is outside its supported range");
    }
    return value;
}

void checked(bool ok, const Status &status) {
    if (!ok) { throw std::runtime_error(status.message()); }
}

void known_fields(const Json::Value &value, std::initializer_list<const char *> fields,
                  const char *context) {
    if (!value.isObject()) { throw std::runtime_error(std::string(context) + " must be an object"); }
    std::set<std::string> allowed(fields.begin(), fields.end());
    for (const auto &key : value.getMemberNames()) {
        if (!allowed.count(key)) { throw std::runtime_error(std::string("Unknown ") + context + " field: " + key); }
    }
}

MemoryAccess all_access() {
    return MemoryAccess::LOCAL_READ | MemoryAccess::LOCAL_WRITE |
           MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
}

void parse_mac(const std::string &value, uint8_t *output) {
    if (value.size() != 17) { throw std::runtime_error("Invalid Ethernet address"); }
    for (size_t i = 0; i < 6; ++i) {
        if (i && value[3 * i - 1] != ':') {
            throw std::runtime_error("Invalid Ethernet address separator");
        }
        unsigned result = 0;
        for (size_t digit = 0; digit < 2; ++digit) {
            const char c = value[3 * i + digit];
            const int n = c >= '0' && c <= '9' ? c - '0' :
                          c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                          c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (n < 0) { throw std::runtime_error("Invalid Ethernet address digit"); }
            result = result * 16 + n;
        }
        output[i] = result;
    }
}

std::vector<uint8_t> unhex(const Json::Value &value, size_t limit) {
    if (!value.isString()) { throw std::runtime_error("private_data must be a hexadecimal string"); }
    const std::string hex = value.asString();
    if (hex.size() % 2 || hex.size() / 2 > limit) { throw std::runtime_error("private_data exceeds the CM message limit"); }
    std::vector<uint8_t> data; data.reserve(hex.size() / 2);
    for (size_t n = 0; n < hex.size(); n += 2) {
        unsigned byte = 0;
        for (size_t k = 0; k < 2; ++k) {
            const char c = hex[n + k];
            const int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (v < 0) { throw std::runtime_error("private_data contains a non-hexadecimal character"); }
            byte = byte * 16 + v;
        }
        data.push_back(byte);
    }
    return data;
}

std::string hex_data(const uint8_t *data, size_t length) {
    const char digits[] = "0123456789abcdef";
    std::string result; result.reserve(length * 2);
    for (size_t n = 0; n < length; ++n) { result.push_back(digits[data[n] >> 4]); result.push_back(digits[data[n] & 15]); }
    return result;
}

void pattern(std::vector<uint8_t> &output, size_t size, uint32_t seed) {
    output.resize(size);
    uint32_t state = seed ? seed : 1;
    for (size_t i = 0; i < size; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        output[i] = static_cast<uint8_t>(state ^ i ^ (i >> 8));
    }
}

bool pattern_matches(ByteSpan bytes, uint32_t seed) {
    uint32_t state = seed ? seed : 1;
    for (size_t i = 0; i < bytes.size; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        if (bytes.data[i] != static_cast<uint8_t>(state ^ i ^ (i >> 8))) { return false; }
    }
    return true;
}

/* PAL-compatible bounded read: Linux simulator mbufs do not provide
 * rte_pktmbuf_read. Never assume the first segment contains all headers. */
bool mbuf_copy(const rte_mbuf_t *m, size_t offset, uint8_t *out, size_t length) {
    while (m && offset >= rte_pktmbuf_data_len(m)) {
        offset -= rte_pktmbuf_data_len(m);
        m = m->next;
    }
    while (m && length) {
        const size_t count = std::min<size_t>(length, rte_pktmbuf_data_len(m) - offset);
        memcpy(out, rte_pktmbuf_mtod(m, const uint8_t *) + offset, count);
        out += count; length -= count; offset = 0; m = m->next;
    }
    return length == 0;
}

/* Classification is intentionally cheaper than codec validation. The owning
 * engine still checks IP/UDP lengths, endpoint identity, ICRC and the opcode.
 * Only UDP/4791 is claimed; other traffic retains ASTF's existing RX path. */
bool wire_qpn(const rte_mbuf_t *m, uint32_t &qpn, NativeEndpoint &local) {
    uint8_t bytes[128];
    const size_t length = std::min<size_t>(sizeof(bytes), rte_pktmbuf_pkt_len(m));
    if (length < 14 || !mbuf_copy(m, 0, bytes, length)) { return false; }
    size_t ip = 14;
    uint16_t type = (bytes[12] << 8) | bytes[13];
    if (type == 0x8100 || type == 0x88a8) {
        if (length < 18) { return false; }
        local.has_vlan = true; local.vlan = ((bytes[14] << 8) | bytes[15]) & 4095;
        type = (bytes[16] << 8) | bytes[17]; ip = 18;
    }
    size_t udp;
    if (type == 0x0800) {
        if (length < ip + 20 || bytes[ip + 9] != 17 || (bytes[ip] >> 4) != 4) {
            return false;
        }
        const size_t ihl = (bytes[ip] & 15) * 4;
        if (ihl < 20) { return false; }
        local.ipv6 = false; memcpy(local.src_ip, bytes + ip + 16, 4);
        udp = ip + ihl;
    } else if (type == 0x86dd) {
        if (length < ip + 40 || bytes[ip + 6] != 17 || (bytes[ip] >> 4) != 6) {
            return false;
        }
        local.ipv6 = true; memcpy(local.src_ip, bytes + ip + 24, 16);
        udp = ip + 40;
    } else { return false; }
    if (length < udp + 16 || ((bytes[udp + 2] << 8) | bytes[udp + 3]) != ROCEV2_UDP_PORT) {
        return false;
    }
    qpn = (uint32_t(bytes[udp + 13]) << 16) |
          (uint32_t(bytes[udp + 14]) << 8) | bytes[udp + 15];
    memcpy(local.src_mac, bytes, 6);
    return true;
}

std::string neighbor_key(uint8_t port, const NativeEndpoint &endpoint) {
    std::string key;
    key.push_back(port); key.push_back(endpoint.ipv6); key.push_back(endpoint.has_vlan);
    key.push_back(endpoint.vlan >> 8); key.push_back(endpoint.vlan);
    key.append(reinterpret_cast<const char *>(endpoint.src_ip), endpoint.ipv6 ? 16 : 4);
    return key;
}

// All routing keys use the local endpoint in TX orientation. On RX the
// classifier copies destination fields into src_* before looking them up.
std::string endpoint_key(uint8_t port, const NativeEndpoint &endpoint) {
    std::string key = neighbor_key(port, endpoint);
    key.append(reinterpret_cast<const char *>(endpoint.src_mac), 6);
    return key;
}

std::string identity_key(uint8_t port, const NativeEndpoint &endpoint, uint64_t identity) {
    std::string key = endpoint_key(port, endpoint);
    for (unsigned shift = 0; shift < 64; shift += 8) { key.push_back(identity >> shift); }
    return key;
}

NativeEndpoint received_local(const NativeEndpoint &wire) {
    NativeEndpoint local = wire;
    memcpy(local.src_mac, wire.dst_mac, 6);
    memcpy(local.src_ip, wire.dst_ip, sizeof(local.src_ip));
    return local;
}

bool neighbor_target(const rte_mbuf_t *m, NativeEndpoint &endpoint) {
    uint8_t b[128]; const size_t size = std::min<size_t>(sizeof(b), rte_pktmbuf_pkt_len(m));
    if (size < 14 || !mbuf_copy(m, 0, b, size)) { return false; }
    size_t l3 = 14; uint16_t type = (b[12] << 8) | b[13];
    if (type == 0x8100 || type == 0x88a8) {
        if (size < 18) { return false; }
        endpoint.has_vlan = true; endpoint.vlan = ((b[14] << 8) | b[15]) & 4095;
        type = (b[16] << 8) | b[17]; l3 = 18;
    }
    if (type == 0x0806 && size >= l3 + 28) {
        endpoint.ipv6 = false; memcpy(endpoint.src_ip, b + l3 + 24, 4); return true;
    }
    if (type == 0x86dd && size >= l3 + 64 && b[l3 + 6] == 58) {
        if (b[l3 + 40] != 135 && b[l3 + 40] != 136) { return false; }
        endpoint.ipv6 = true;
        memcpy(endpoint.src_ip, b + l3 + (b[l3 + 40] == 135 ? 48 : 24), 16); return true;
    }
    return false;
}

struct RedirectedPacket {
    rte_mbuf_t *mbuf;
    uint64_t generation;
    uint8_t port;
    RedirectedPacket() : mbuf(nullptr), generation(0), port(0) {}
};

/* Separate bounded queues avoid sharing native frame slabs, whose intrusive
 * references are deliberately non-atomic. Mutexes are taken only for cross-
 * worker packets; direct-owner ingress does not take a global lock. */
struct Mailbox {
    std::mutex lock;
    std::atomic<size_t> pending{0};
    uint64_t generation = 0;
    size_t head = 0, tail = 0, count = 0;
    std::vector<RedirectedPacket> packets;
    void clear_locked() {
        while (count) {
            rte_pktmbuf_free(packets[head].mbuf);
            head = (head + 1) % packets.size(); --count;
        }
        head = tail = 0;
        pending.store(0, std::memory_order_release);
    }
};
std::array<Mailbox, 256> mailboxes;

bool redirect(uint8_t owner, uint64_t generation, uint8_t port, rte_mbuf_t *m) {
    Mailbox &box = mailboxes[owner];
    std::lock_guard<std::mutex> guard(box.lock);
    if (box.generation != generation || box.packets.empty() || box.count == box.packets.size()) {
        return false;
    }
    RedirectedPacket &entry = box.packets[box.tail];
    entry.mbuf = m; entry.generation = generation; entry.port = port;
    box.tail = (box.tail + 1) % box.packets.size(); ++box.count;
    box.pending.store(box.count, std::memory_order_release);
    return true;
}

enum class ProgramOperation { WRITE, WRITE_IMM, READ, SEND, SEND_IMM, SEND_INV, CAS, FAA };

struct WorkSpec {
    ProgramOperation operation;
    uint64_t local_offset, remote_offset, length, compare, swap_add, expected_original;
    uint32_t repeat, immediate_data, invalidate_rkey, expected_seed;
    bool fenced, has_expected_original;
};

ProgramOperation operation(const std::string &name) {
    if (name == "write") { return ProgramOperation::WRITE; }
    if (name == "write_imm") { return ProgramOperation::WRITE_IMM; }
    if (name == "read") { return ProgramOperation::READ; }
    if (name == "send") { return ProgramOperation::SEND; }
    if (name == "send_imm") { return ProgramOperation::SEND_IMM; }
    if (name == "send_inv") { return ProgramOperation::SEND_INV; }
    if (name == "compare_swap") { return ProgramOperation::CAS; }
    if (name == "fetch_add") { return ProgramOperation::FAA; }
    throw std::runtime_error("Unsupported RoCEv2 work operation");
}

bool is_atomic(ProgramOperation op) {
    return op == ProgramOperation::CAS || op == ProgramOperation::FAA;
}

struct DeviceSpec {
    uint32_t id = 0, ordinal = 0;
    uint8_t port = 0;
    uint64_t node_guid = 0;
    bool has_ipv4 = false, has_ipv6 = false;
    NativeNeighborInterface ipv4, ipv6;
    std::map<uint16_t, NativeEndpoint> gids;
};

void host_address(const Json::Value &host, const char *key, bool ipv6, uint8_t *output) {
    if (!host[key].isString() || inet_pton(ipv6 ? AF_INET6 : AF_INET,
            host[key].asString().c_str(), output) != 1) {
        throw std::runtime_error(std::string("Invalid device host ") + key);
    }
    const size_t bytes = ipv6 ? 16 : 4;
    if (std::all_of(output, output + bytes, [](uint8_t b) { return b == 0; }) ||
        (ipv6 ? output[0] == 0xff : output[0] >= 224)) {
        throw std::runtime_error(std::string("Device host ") + key + " must be a unicast address");
    }
}

DeviceSpec parse_device(const Json::Value &j, uint32_t ordinal, uint8_t pairs) {
    known_fields(j, {"device_id", "port", "host", "node_guid", "gids"}, "device");
    if (!j.isMember("device_id") || !j.isMember("port") || !j.isMember("node_guid")) {
        throw std::runtime_error("A device requires device_id, port and node_guid");
    }
    DeviceSpec d;
    d.id = number(j, "device_id", 0, 1, UINT32_MAX); d.ordinal = ordinal;
    d.port = number(j, "port", 256, 0, uint32_t(pairs) * 2 - 1);
    d.node_guid = number(j, "node_guid", 0, 1, UINT64_MAX);
    const Json::Value &host = j["host"];
    known_fields(host, {"mac", "ipv4", "ipv4_prefix", "ipv4_gateway", "ipv6",
                       "ipv6_prefix", "ipv6_gateway", "vlan"}, "device host");
    if (!host["mac"].isString()) { throw std::runtime_error("Device host requires a MAC address"); }
    parse_mac(host["mac"].asString(), d.ipv4.endpoint.src_mac);
    if ((d.ipv4.endpoint.src_mac[0] & 1) ||
        std::all_of(d.ipv4.endpoint.src_mac, d.ipv4.endpoint.src_mac + 6, [](uint8_t b) { return b == 0; })) {
        throw std::runtime_error("Device host MAC must be nonzero unicast");
    }
    d.ipv4.port = d.port; d.ipv4.interface_id = uint64_t(ordinal) * 2 + 1;
    d.ipv4.endpoint.has_vlan = host.isMember("vlan");
    d.ipv4.endpoint.vlan = number(host, "vlan", 0, 0, 4095);
    d.ipv6 = d.ipv4; d.ipv6.endpoint.ipv6 = true; ++d.ipv6.interface_id;
    d.has_ipv4 = host.isMember("ipv4"); d.has_ipv6 = host.isMember("ipv6");
    if (!d.has_ipv4 && !d.has_ipv6) { throw std::runtime_error("Device host requires IPv4 or IPv6"); }
    for (bool ipv6 : {false, true}) {
        const char *ip_key = ipv6 ? "ipv6" : "ipv4";
        const char *prefix_key = ipv6 ? "ipv6_prefix" : "ipv4_prefix";
        const char *gateway_key = ipv6 ? "ipv6_gateway" : "ipv4_gateway";
        const bool present = ipv6 ? d.has_ipv6 : d.has_ipv4;
        if (!present) {
            if (host.isMember(prefix_key) || host.isMember(gateway_key)) {
                throw std::runtime_error("Device host prefix/gateway requires its address family");
            }
            continue;
        }
        NativeNeighborInterface &network = ipv6 ? d.ipv6 : d.ipv4;
        host_address(host, ip_key, ipv6, network.endpoint.src_ip);
        network.prefix_length = number(host, prefix_key, ipv6 ? 128 : 32, 0, ipv6 ? 128 : 32);
        network.has_gateway = host.isMember(gateway_key);
        if (network.has_gateway) { host_address(host, gateway_key, ipv6, network.gateway.data()); }
    }
    if (!j["gids"].isArray() || j["gids"].empty() || j["gids"].size() > 256) {
        throw std::runtime_error("Device gids must be a nonempty bounded array");
    }
    for (const auto &gid : j["gids"]) {
        known_fields(gid, {"index", "ip", "gid"}, "GID");
        if (!gid.isMember("index") || !gid["ip"].isString() || !gid["gid"].isString()) {
            throw std::runtime_error("A GID requires index, ip and gid");
        }
        const uint16_t index = number(gid, "index", 0, 0, UINT16_MAX);
        const bool ipv6 = gid["ip"].asString().find(':') != std::string::npos;
        const NativeEndpoint &host_endpoint = ipv6 ? d.ipv6.endpoint : d.ipv4.endpoint;
        uint8_t ip[16] = {}, wire_gid[16] = {}, expected[16] = {};
        if (!(ipv6 ? d.has_ipv6 : d.has_ipv4) ||
            inet_pton(ipv6 ? AF_INET6 : AF_INET, gid["ip"].asString().c_str(), ip) != 1 ||
            memcmp(ip, host_endpoint.src_ip, ipv6 ? 16 : 4) ||
            inet_pton(AF_INET6, gid["gid"].asString().c_str(), wire_gid) != 1) {
            throw std::runtime_error("GID IP must match an owned device host address");
        }
        if (ipv6) { memcpy(expected, ip, 16); }
        else { expected[10] = expected[11] = 0xff; memcpy(expected + 12, ip, 4); }
        for (const auto &existing : d.gids) {
            if (existing.second.ipv6 == ipv6) { throw std::runtime_error("GID IP addresses must be unique"); }
        }
        if (memcmp(expected, wire_gid, 16) || !d.gids.emplace(index, host_endpoint).second) {
            throw std::runtime_error("GID must match its IP and have a unique index");
        }
    }
    for (bool ipv6 : {false, true}) {
        if (!(ipv6 ? d.has_ipv6 : d.has_ipv4)) { continue; }
        bool found = false;
        for (const auto &gid : d.gids) { found |= gid.second.ipv6 == ipv6; }
        if (!found) { throw std::runtime_error("Every device host address requires a GID"); }
    }
    return d;
}

void validate_device_qp_fields(const Json::Value &j) {
    known_fields(j, {"port", "device_id", "local_gid_index", "local_qpn", "remote_qpn", "message_size",
        "memory_size", "local_address", "remote_address", "local_rkey", "remote_rkey", "seed", "peer_seed",
        "iterations", "outstanding", "operation", "memory_id", "receive_repost", "expected_receives",
        "initial_u64", "expected_u64", "initial_u64_offset", "program", "immediate_data", "invalidate_rkey",
        "atomic_compare", "atomic_swap_add", "expected_original", "expect_writes", "protection_domain",
        "receive_depth", "receives", "lkey", "local_psn", "mtu", "timeout_exp", "retry_count",
        "max_rd_atomic", "max_dest_rd_atomic", "remote_psn", "src_mac", "dst_mac", "src_ip", "dst_ip",
        "dscp", "udp_sport", "vlan", "connection", "prefix_length", "gateway"}, "device QP");
    if (j["program"].isArray()) {
        for (const auto &work : j["program"]) {
            known_fields(work, {"operation", "length", "local_offset", "remote_offset", "repeat", "immediate_data",
                "invalidate_rkey", "compare", "swap_add", "expected_original", "expected_seed", "fenced"}, "work");
        }
    }
    if (j["receives"].isArray()) {
        for (const auto &receive : j["receives"]) {
            known_fields(receive, {"wr_id", "sges"}, "receive");
            if (receive["sges"].isArray()) {
                for (const auto &sge : receive["sges"]) { known_fields(sge, {"address", "length", "lkey"}, "SGE"); }
            }
        }
    }
    if (j.isMember("connection")) {
        known_fields(j["connection"], {"mode", "service_id", "source_port", "destination_port", "ip_cma",
            "auto_accept", "private_data", "backlog"}, "connection");
    }
}

struct QpSpec {
    uint8_t port, owner;
    uint32_t index, memory_id, local_qpn, remote_qpn, iterations, outstanding;
    uint32_t device_id = 0;
    uint16_t local_gid_index = 0;
    uint64_t node_guid = 0;
    uint32_t size, memory_size, seed, peer_seed;
    uint64_t local_address, remote_address;
    uint32_t local_rkey, remote_rkey, local_lkey;
    bool expect_writes;
    bool managed = false, active_connection = false, ip_cma = false, auto_accept = true;
    std::vector<uint8_t> private_data;
    uint64_t service_id = 0;
    uint16_t source_port = 0;
    uint32_t backlog = 1;
    NativeNeighborInterface network;
    bool receive_repost, has_initial_u64, has_expected_u64;
    uint64_t initial_u64, expected_u64, initial_u64_offset, expected_receives, work_per_iteration;
    std::string operation;
    std::vector<WorkSpec> program;
    std::vector<NativeReceiveRequest> receives;
    QpConfig local;
    QpPeerConfig peer;
    NativeTransportConfig transport;
};

QpSpec parse_qp(const Json::Value &j, uint32_t index, uint8_t pairs, uint8_t workers_per_pair) {
    if (!j.isObject()) { throw std::runtime_error("Every QP entry must be an object"); }
    QpSpec s;
    s.index = index;
    // NativeEndpointEngine reserves memory-region ID zero. Keep the public QP
    // index zero-based while assigning each QP a nonzero backing-region ID.
    s.memory_id = index + 1;
    s.port = number(j, "port", 256, 0, uint32_t(pairs) * 2 - 1);
    // A missing required numeric value must not wrap before admission.
    if (!j.isMember("port") || !j.isMember("local_qpn") || !j.isMember("remote_qpn")) {
        throw std::runtime_error("QP requires port, local_qpn and remote_qpn");
    }
    s.local_qpn = number(j, "local_qpn", 0, 2, PSN24_MASK);
    s.remote_qpn = number(j, "remote_qpn", 0, 2, PSN24_MASK);
    s.size = number(j, "message_size", 4096, 1, 16U << 20);
    s.memory_size = number(j, "memory_size", uint64_t(s.size) * 2, 2, 64U << 20);
    if (s.memory_size < uint64_t(s.size) * 2) {
        throw std::runtime_error("memory_size must hold both source and target messages");
    }
    s.local_address = number(j, "local_address", 0x100000, 0, UINT64_MAX);
    s.remote_address = number(j, "remote_address", 0x100000, 0, UINT64_MAX);
    if (s.local_address > UINT64_MAX - s.memory_size ||
        s.remote_address > UINT64_MAX - s.memory_size) {
        throw std::runtime_error("QP memory address range overflows");
    }
    s.local_rkey = number(j, "local_rkey", 1, 1, UINT32_MAX);
    s.remote_rkey = number(j, "remote_rkey", 1, 1, UINT32_MAX);
    s.seed = number(j, "seed", 1, 0, UINT32_MAX);
    s.peer_seed = number(j, "peer_seed", 1, 0, UINT32_MAX);
    s.iterations = number(j, "iterations", 1, 0, UINT32_MAX);
    s.outstanding = number(j, "outstanding", 1, 1, 1024);
    s.operation = j.get("operation", "write").asString();
    s.memory_id = number(j, "memory_id", s.memory_id, 1, UINT32_MAX);
    s.owner = 0; // Assigned from the final protection domain below.
    s.receive_repost = j.get("receive_repost", false).asBool();
    if (j.isMember("receive_repost") && !j["receive_repost"].isBool()) {
        throw std::runtime_error("receive_repost must be boolean");
    }
    s.expected_receives = number(j, "expected_receives", 0, 0, UINT32_MAX);
    s.has_initial_u64 = j.isMember("initial_u64");
    s.has_expected_u64 = j.isMember("expected_u64");
    s.initial_u64 = number(j, "initial_u64", 0, 0, UINT64_MAX);
    s.expected_u64 = number(j, "expected_u64", 0, 0, UINT64_MAX);
    s.initial_u64_offset = number(j, "initial_u64_offset", s.size, 0, s.memory_size);
    if ((s.has_initial_u64 || s.has_expected_u64) &&
        (s.initial_u64_offset > s.memory_size || s.memory_size - s.initial_u64_offset < 8)) {
        throw std::runtime_error("64-bit memory value lies outside the registered region");
    }
    s.work_per_iteration = 0;
    Json::Value program = j["program"];
    if (program.isNull() && s.operation != "none") {
        Json::Value step(Json::objectValue);
        step["operation"] = s.operation;
        step["immediate_data"] = j.get("immediate_data", 0);
        step["invalidate_rkey"] = j.get("invalidate_rkey", 0);
        step["compare"] = j.get("atomic_compare", Json::UInt64(0));
        step["swap_add"] = j.get("atomic_swap_add", Json::UInt64(0));
        if (j.isMember("expected_original")) { step["expected_original"] = j["expected_original"]; }
        program = Json::arrayValue; program.append(step);
    }
    if (!program.isNull() && (!program.isArray() || program.empty() || program.size() > 4096)) {
        throw std::runtime_error("program must contain between 1 and 4096 work entries");
    }
    for (Json::ArrayIndex n = 0; n < program.size(); ++n) {
        const Json::Value &step = program[n];
        if (!step.isObject() || !step["operation"].isString()) {
            throw std::runtime_error("A program entry requires an operation");
        }
        WorkSpec work;
        work.operation = operation(step["operation"].asString());
        const bool source = work.operation == ProgramOperation::WRITE ||
                            work.operation == ProgramOperation::WRITE_IMM ||
                            work.operation == ProgramOperation::SEND ||
                            work.operation == ProgramOperation::SEND_IMM ||
                            work.operation == ProgramOperation::SEND_INV;
        work.length = number(step, "length", is_atomic(work.operation) ? 8 : s.size, 0, 16U << 20);
        work.local_offset = number(step, "local_offset", source ? 0 : s.size, 0, s.memory_size);
        work.remote_offset = number(step, "remote_offset",
            work.operation == ProgramOperation::WRITE || work.operation == ProgramOperation::WRITE_IMM ||
            is_atomic(work.operation) ? s.size : 0, 0, s.memory_size);
        if (work.length > s.memory_size - work.local_offset ||
            work.length > s.memory_size - work.remote_offset ||
            (is_atomic(work.operation) && (work.length != 8 || (s.remote_address + work.remote_offset) % 8))) {
            throw std::runtime_error("Work range or atomic alignment is invalid");
        }
        work.repeat = number(step, "repeat", 1, 1, UINT32_MAX);
        work.immediate_data = number(step, "immediate_data", 0, 0, UINT32_MAX);
        work.invalidate_rkey = number(step, "invalidate_rkey", 0, 0, UINT32_MAX);
        if (work.operation == ProgramOperation::SEND_INV && !work.invalidate_rkey) {
            throw std::runtime_error("send_inv requires a nonzero invalidate_rkey");
        }
        work.compare = number(step, "compare", 0, 0, UINT64_MAX);
        work.swap_add = number(step, "swap_add", 0, 0, UINT64_MAX);
        work.has_expected_original = step.isMember("expected_original");
        work.expected_original = number(step, "expected_original", 0, 0, UINT64_MAX);
        work.expected_seed = number(step, "expected_seed", s.peer_seed, 0, UINT32_MAX);
        if (step.isMember("fenced") && !step["fenced"].isBool()) {
            throw std::runtime_error("fenced must be boolean");
        }
        work.fenced = step.get("fenced", false).asBool();
        s.program.push_back(work);
        s.work_per_iteration += work.repeat;
    }
    if (s.iterations && s.work_per_iteration > UINT64_MAX / s.iterations) {
        throw std::runtime_error("Work program iteration count overflows");
    }
    if (j.isMember("expect_writes") && !j["expect_writes"].isBool()) {
        throw std::runtime_error("expect_writes must be boolean");
    }
    s.expect_writes = j.get("expect_writes", false).asBool();
    s.local.local_qpn = s.local_qpn;
    s.local.protection_domain = number(j, "protection_domain", s.memory_id, 1, UINT32_MAX);
    s.local.max_receive_sge = 16;
    const uint32_t receive_depth = number(j, "receive_depth", 0, 0, 1024);
    const Json::Value &receives = j["receives"];
    if (!receives.isNull() && (!receives.isArray() || receives.size() > 1024)) {
        throw std::runtime_error("receives must contain at most 1024 descriptors");
    }
    std::set<uint64_t> receive_ids;
    const uint32_t lkey = number(j, "lkey", 1, 1, UINT32_MAX);
    s.local_lkey = lkey;
    for (Json::ArrayIndex n = 0; n < receives.size(); ++n) {
        const Json::Value &entry = receives[n];
        if (!entry.isObject() || !entry["sges"].isArray() || entry["sges"].empty() ||
            entry["sges"].size() > s.local.max_receive_sge) {
            throw std::runtime_error("Receive descriptor requires bounded SGEs");
        }
        NativeReceiveRequest request;
        request.wr_id = number(entry, "wr_id", n, 0, UINT64_MAX);
        if (!receive_ids.insert(request.wr_id).second) { throw std::runtime_error("Duplicate receive work ID"); }
        for (Json::ArrayIndex k = 0; k < entry["sges"].size(); ++k) {
            const Json::Value &sge = entry["sges"][k];
            NativeScatterGatherEntry segment;
            segment.address = number(sge, "address", s.local_address + s.size, 0, UINT64_MAX);
            segment.length = number(sge, "length", s.size, 0, 16U << 20);
            segment.lkey = number(sge, "lkey", lkey, 1, UINT32_MAX);
            request.sges.push_back(segment);
        }
        s.receives.push_back(request);
    }
    if (uint64_t(receive_depth + 1) * s.size > s.memory_size) {
        throw std::runtime_error("Receive depth requires disjoint buffers within memory_size");
    }
    for (uint32_t n = 0; n < receive_depth; ++n) {
        NativeReceiveRequest request;
        request.wr_id = uint64_t(n) + 1;
        if (!receive_ids.insert(request.wr_id).second) { throw std::runtime_error("Duplicate receive work ID"); }
        NativeScatterGatherEntry segment;
        segment.address = s.local_address + uint64_t(n + 1) * s.size;
        segment.length = s.size; segment.lkey = lkey;
        request.sges.push_back(segment); s.receives.push_back(request);
    }
    s.local.rq_depth_messages = std::max<size_t>(1, s.receives.size());
    s.local.starting_sq_psn = number(j, "local_psn", 0, 0, PSN24_MASK);
    s.local.pmtu = number(j, "mtu", 1024, 256, 4096);
    if (s.local.pmtu != 256 && s.local.pmtu != 512 && s.local.pmtu != 1024 &&
        s.local.pmtu != 2048 && s.local.pmtu != 4096) {
        throw std::runtime_error("mtu must be an RDMA path MTU");
    }
    uint64_t maximum_length = s.size;
    for (const auto &work : s.program) { maximum_length = std::max(maximum_length, work.length); }
    const uint64_t packets = std::max<uint64_t>(1, (maximum_length + s.local.pmtu - 1) / s.local.pmtu) * s.outstanding;
    if (packets > 65536) { throw std::runtime_error("QP requires more than 65536 outstanding packets"); }
    s.local.sq_depth_messages = s.outstanding;
    s.local.sq_depth_packets = std::max<uint64_t>(32, packets);
    s.local.timeout_exp = number(j, "timeout_exp", 12, 0, 31);
    s.local.retry_count = number(j, "retry_count", 3, 0, 7);
    s.local.rnr_retry_count = s.local.retry_count;
    s.local.max_rd_atomic = number(j, "max_rd_atomic", std::min<uint32_t>(16, s.outstanding), 0, 16);
    s.local.max_dest_rd_atomic = number(j, "max_dest_rd_atomic", std::min<uint32_t>(16, s.outstanding), 0, 16);
    s.peer.remote_qpn = s.remote_qpn;
    s.peer.starting_psn = number(j, "remote_psn", 0, 0, PSN24_MASK);
    s.peer.pmtu = s.local.pmtu;
    s.peer.rkey = s.remote_rkey;
    s.peer.virtual_address = s.remote_address;
    s.peer.window_size = s.memory_size;
    s.peer.window_access = MemoryAccess::REMOTE_READ | MemoryAccess::REMOTE_WRITE | MemoryAccess::REMOTE_ATOMIC;
    s.peer.max_rd_atomic = s.local.max_rd_atomic;
    const char *required[] = {"src_mac", "dst_mac", "src_ip", "dst_ip"};
    for (const char *key : required) {
        if (!j[key].isString()) { throw std::runtime_error(std::string("QP requires string ") + key); }
    }
    NativeEndpoint &endpoint = s.transport.endpoint;
    parse_mac(j["src_mac"].asString(), endpoint.src_mac);
    parse_mac(j["dst_mac"].asString(), endpoint.dst_mac);
    const std::string src = j["src_ip"].asString(), dst = j["dst_ip"].asString();
    endpoint.ipv6 = src.find(':') != std::string::npos;
    const int family = endpoint.ipv6 ? AF_INET6 : AF_INET;
    if (inet_pton(family, src.c_str(), endpoint.src_ip) != 1 ||
        inet_pton(family, dst.c_str(), endpoint.dst_ip) != 1) {
        throw std::runtime_error("Invalid endpoint IP address or mismatched IP families");
    }
    s.peer.ip = dst; s.peer.mac = j["dst_mac"].asString();
    endpoint.dscp = number(j, "dscp", 26, 0, 63);
    endpoint.udp_sport = number(j, "udp_sport", 49152, 1, 65535);
    endpoint.udp_dport = ROCEV2_UDP_PORT;
    endpoint.has_vlan = j.isMember("vlan");
    endpoint.vlan = number(j, "vlan", 0, 0, 4095);
    if (j.isMember("connection")) {
        const Json::Value &cm = j["connection"];
        if (!cm.isObject() || !cm["mode"].isString() ||
            (cm["mode"].asString() != "active" && cm["mode"].asString() != "passive")) {
            throw std::runtime_error("connection requires active or passive mode");
        }
        s.managed = true; s.active_connection = cm["mode"].asString() == "active";
        s.service_id = number(cm, "service_id", 0, 1, UINT64_MAX);
        if (!s.service_id) { throw std::runtime_error("connection requires a nonzero service_id"); }
        s.source_port = number(cm, "source_port", 0, 0, 65535);
        const uint16_t destination_port = number(cm, "destination_port", 0, 0, 65535);
        if (destination_port && s.service_id != NativeCmCodec::ip_service_id(destination_port)) {
            throw std::runtime_error("IP CM destination_port must match service_id 0x0106:<port>");
        }
        if ((cm.isMember("ip_cma") && !cm["ip_cma"].isBool()) ||
            (cm.isMember("auto_accept") && !cm["auto_accept"].isBool())) {
            throw std::runtime_error("ip_cma and auto_accept must be boolean");
        }
        s.ip_cma = cm.get("ip_cma", s.source_port != 0 || destination_port != 0).asBool();
        s.auto_accept = cm.get("auto_accept", true).asBool();
        if (s.active_connection && s.ip_cma && !s.source_port) {
            throw std::runtime_error("IP CMA active connect requires a source_port");
        }
        s.private_data = unhex(cm.get("private_data", ""), s.active_connection ? (s.ip_cma ? 56 : 92) : 196);
        s.backlog = number(cm, "backlog", 1, 1, 4096);
    }
    s.network.interface_id = uint64_t(s.index) + 1; s.network.port = s.port;
    s.network.endpoint = endpoint;
    s.network.prefix_length = number(j, "prefix_length", endpoint.ipv6 ? 128 : 32, 0, endpoint.ipv6 ? 128 : 32);
    s.network.has_gateway = j.isMember("gateway");
    if (s.network.has_gateway && (!j["gateway"].isString() ||
        inet_pton(family, j["gateway"].asString().c_str(), s.network.gateway.data()) != 1)) {
        throw std::runtime_error("gateway must match the endpoint IP family");
    }
    if (!s.managed && std::all_of(endpoint.dst_mac, endpoint.dst_mac + 6, [](uint8_t b) { return b == 0; })) {
        throw std::runtime_error("Dynamic neighbor resolution requires a managed CM connection");
    }
    s.owner = (s.local.protection_domain % workers_per_pair) * pairs + s.port / 2;
    s.local.allow_remote_registry = true;
    s.transport.cnp_enabled = false;
    s.transport.signal_mode = NativeTransportSignalMode::PLAIN_LAST;
    s.transport.require_receive_wqes = true;
    s.transport.legacy_response_acks = false;
    s.transport.unlimited_receive_credits = true;
    s.transport.max_tx_frames = std::max<uint64_t>(64, packets + 8);
    s.transport.max_staged_write_bytes = maximum_length * s.outstanding;
    s.transport.max_read_cache_entries = std::max<unsigned>(1, s.local.max_dest_rd_atomic);
    s.transport.max_read_cache_bytes = maximum_length * s.transport.max_read_cache_entries;
    Status status;
    checked(s.local.validate(&status), status);
    checked(s.peer.validate(&status), status);
    checked(s.transport.validate(&status), status);
    return s;
}

struct PendingWork {
    bool active = false;
    uint64_t sequence = 0, expected_original = 0;
    WorkSpec work;
};

struct RuntimeQp {
    QpSpec spec;
    QpId id;
    NativeEndpointEngine *engine = nullptr;
    std::vector<uint8_t> expected;
    uint64_t posted = 0, completed = 0, failed = 0, completed_bytes = 0;
    uint64_t data_tx_packets = 0;
    uint64_t verification_failures = 0, verified_bytes = 0;
    uint64_t received = 0, received_bytes = 0, receive_failures = 0, total_work = 0;
    uint64_t unverified_results = 0, connection_failures = 0, submission_failures = 0;
    uint32_t last_immediate = 0, last_invalidated_rkey = 0;
    bool has_immediate = false, has_invalidate = false;
    size_t next_work = 0;
    uint32_t work_repeat = 0;
    std::vector<uint64_t> step_posts;
    std::vector<PendingWork> pending;
    bool terminal = false;
    bool transport_failed = false;
    bool attached = false, connected = false, closed = false, ever_established = false;
    uint32_t connection_id = 0;
    std::string connection_state = "static";
};
}

class TrexAstfRoceV2Worker::Impl {
public:
    Impl(uint8_t id, CFlowGenListPerThread *core) : id(id), core(core) {}
    ~Impl() { release(); }

    uint8_t id;
    CFlowGenListPerThread *core;
    bool configured = false, running = false, participating = false, started = false;
    uint64_t generation = 0, deadline = 0, duration_ns = 0;
    uint64_t rx_packets = 0, tx_packets = 0, redirected = 0, redirect_drops = 0;
    uint64_t ingress_drops = 0, malformed = 0, worker_errors = 0;
    uint64_t tx_backpressure = 0, allocation_failures = 0;
    std::string error;
    std::array<std::unique_ptr<NativeEndpointEngine>, 2> engines;
    std::vector<RuntimeQp> qps;
    std::unique_ptr<NativeCmManager> cm;
    std::unique_ptr<NativeNeighborManager> neighbors;
    std::map<std::string, uint8_t> listener_routes;
    std::map<std::string, uint8_t> neighbor_routes;
    std::map<std::string, uint8_t> routes;
    std::map<uint64_t, size_t> operation_qps;
    std::map<uint32_t, size_t> connection_qps;
    Json::Value admission, scale, device_info;
    std::vector<CompletionRecord> completions;
    std::vector<CmEvent> cm_events;
    std::array<uint8_t, MAX_FRAME> scratch;
    Json::Value final_stats;

    void release() {
        running = false;
        Mailbox &box = mailboxes[id];
        {
            std::lock_guard<std::mutex> guard(box.lock);
            box.generation = 0;
            box.clear_locked();
            std::vector<RedirectedPacket>().swap(box.packets);
        }
        std::vector<RuntimeQp>().swap(qps); std::vector<CmEvent>().swap(cm_events);
        cm.reset(); neighbors.reset(); listener_routes.clear(); neighbor_routes.clear();
        for (auto &engine : engines) { engine.reset(); }
        routes.clear();
        operation_qps.clear(); connection_qps.clear(); admission = Json::Value(); scale = Json::Value();
        device_info = Json::Value();
        configured = participating = false;
    }

    EngineConfig engine_config(uint32_t count, uint64_t memory_bytes, uint32_t depth,
                               uint32_t ingress_capacity) const {
        EngineConfig c;
        c.generation = SessionGeneration(generation);
        c.endpoint_id = 0; // Metadata is local-only; actual peers are checked in wire bytes.
        c.max_qps = count; c.max_memory_regions = count;
        c.max_total_memory_bytes = memory_bytes;
        c.max_completion_records = std::max<uint32_t>(32, depth);
        c.max_frame_bytes = MAX_FRAME;
        c.max_ingress_frames = ingress_capacity;
        c.max_tx_frames = std::max<uint32_t>(64, std::min<uint32_t>(4096, ingress_capacity));
        c.ingress_budget_per_poll = BURST;
        c.tx_budget_per_poll = BURST;
        return c;
    }

    void configure(const Json::Value &config) {
        if (configured) { throw std::runtime_error("Stop the configured endpoint before reconfiguration"); }
        started = false;
        // STOP snapshots stay readable until the next configuration attempt.
        // A failed new attempt must not report an older session as successful.
        final_stats = Json::Value();
        const uint32_t schema = number(config, "schema_version", 1, 1, 3);
        if (schema == 3) {
            known_fields(config, {"schema_version", "generation", "duration", "qps", "devices",
                "max_memory_bytes", "max_native_storage_bytes", "ingress_queue_size", "scale"}, "profile");
        } else if (config.isMember("devices") || config.isMember("scale")) {
            throw std::runtime_error("Device metadata requires schema_version 3");
        }
        rx_packets = tx_packets = redirected = redirect_drops = 0;
        ingress_drops = malformed = worker_errors = 0;
        tx_backpressure = allocation_failures = 0;
        error.clear();
        if (!config.isObject() || !config["qps"].isArray() || config["qps"].empty() ||
            config["qps"].size() > 4096) {
            throw std::runtime_error("qps must contain between 1 and 4096 QP entries");
        }
        const unsigned pairs_value = CGlobalInfo::m_options.get_expected_dual_ports();
        const unsigned cores_value = CGlobalInfo::m_options.preview.getCores();
        if (!pairs_value || !cores_value || pairs_value * cores_value > 255) {
            throw std::runtime_error("Invalid ASTF worker topology for native RoCEv2");
        }
        const uint8_t pairs = pairs_value, per_pair = cores_value;
        generation = number(config, "generation", 0, 1, UINT64_MAX);
        if (!generation) { throw std::runtime_error("Session generation must be nonzero"); }
        const uint64_t memory_limit = number(config, "max_memory_bytes", (schema == 3 ? 128ULL : 64ULL) << 20,
                                             1, 256ULL << 20);
        const uint64_t storage_limit = number(config, "max_native_storage_bytes", DEFAULT_NATIVE_STORAGE,
                                              1, MAX_NATIVE_STORAGE);
        const uint32_t capacity = number(config, "ingress_queue_size", 1024, 1, 65536);
        if (config.isMember("duration") && !config["duration"].isNumeric()) {
            throw std::runtime_error("duration must be numeric");
        }
        const double duration = config.get("duration", 10.0).asDouble();
        if (!std::isfinite(duration) || duration <= 0 || duration > 3600) {
            throw std::runtime_error("duration must be finite and in (0, 3600] seconds");
        }
        duration_ns = uint64_t(duration * 1e9);
        std::map<uint32_t, DeviceSpec> devices;
        device_info = schema == 3 ? Json::Value(Json::arrayValue) : Json::Value();
        std::set<uint64_t> node_guids;
        std::set<std::string> owned_addresses;
        uint64_t metadata_storage = 0;
        if (schema == 3) {
            if (!config["devices"].isArray() || config["devices"].empty() || config["devices"].size() > 4096) {
                throw std::runtime_error("devices must contain between 1 and 4096 device entries");
            }
            for (Json::ArrayIndex n = 0; n < config["devices"].size(); ++n) {
                DeviceSpec device = parse_device(config["devices"][n], n, pairs);
                if (!node_guids.insert(device.node_guid).second || devices.count(device.id)) {
                    throw std::runtime_error("device_id and node_guid must be globally unique");
                }
                for (bool ipv6 : {false, true}) {
                    if (!(ipv6 ? device.has_ipv6 : device.has_ipv4)) { continue; }
                    const NativeEndpoint &endpoint = ipv6 ? device.ipv6.endpoint : device.ipv4.endpoint;
                    if (!owned_addresses.insert(neighbor_key(device.port, endpoint)).second) {
                        throw std::runtime_error("Devices cannot share a port/VLAN/IP ownership identity");
                    }
                }
                metadata_storage += sizeof(DeviceSpec) + 4096 + device.gids.size() * (sizeof(NativeEndpoint) + 64);
                if (metadata_storage > storage_limit) {
                    throw std::runtime_error("Device metadata exceeds max_native_storage_bytes");
                }
                devices.emplace(device.id, std::move(device));
            }
        }
        scale = Json::Value();
        if (config.isMember("scale")) {
            known_fields(config["scale"], {"devices_per_port", "qps_per_device"}, "scale");
            if (!config["scale"].isMember("devices_per_port") || !config["scale"].isMember("qps_per_device")) {
                throw std::runtime_error("scale requires devices_per_port and qps_per_device");
            }
            number(config["scale"], "devices_per_port", 0, 1, 4096);
            number(config["scale"], "qps_per_device", 0, 1, 4096);
            scale = config["scale"];
        }
        std::vector<QpSpec> specs;
        std::set<std::string> identities;
        std::set<std::pair<uint32_t, uint32_t>> logical_qp_ids;
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> protection_domains, region_ids;
        std::map<uint32_t, uint32_t> device_qps;
        struct Admission { uint32_t count = 0, depth = 0; uint64_t bytes = 0; };
        std::map<std::pair<uint8_t, uint8_t>, Admission> admissions;
        std::set<uint8_t> participants;
        std::set<uint8_t> qp_owners;
        std::map<uint32_t, QpSpec> memory_regions;
        uint64_t total_memory = 0, total_storage = metadata_storage;
        for (Json::ArrayIndex n = 0; n < config["qps"].size(); ++n) {
            const Json::Value &qp_json = config["qps"][n];
            if (schema == 3) { validate_device_qp_fields(qp_json); }
            QpSpec s = parse_qp(qp_json, n, pairs, per_pair);
            if (schema == 3) {
                if (!qp_json.isMember("device_id")) { throw std::runtime_error("Device QP requires device_id"); }
                s.device_id = number(qp_json, "device_id", 0, 1, UINT32_MAX);
                const auto device = devices.find(s.device_id);
                if (device == devices.end()) { throw std::runtime_error("QP refers to an unknown device_id"); }
                const DeviceSpec &d = device->second;
                const NativeEndpoint &endpoint = s.transport.endpoint;
                const NativeNeighborInterface &host = endpoint.ipv6 ? d.ipv6 : d.ipv4;
                if (s.port != d.port || !(endpoint.ipv6 ? d.has_ipv6 : d.has_ipv4) ||
                    endpoint_key(s.port, endpoint) != endpoint_key(d.port, host.endpoint)) {
                    throw std::runtime_error("QP endpoint must match its device host identity");
                }
                auto gid = d.gids.end();
                if (qp_json.isMember("local_gid_index")) {
                    gid = d.gids.find(number(qp_json, "local_gid_index", 0, 0, UINT16_MAX));
                } else {
                    for (auto candidate = d.gids.begin(); candidate != d.gids.end(); ++candidate) {
                        if (candidate->second.ipv6 == endpoint.ipv6) { gid = candidate; break; }
                    }
                }
                if (gid == d.gids.end() || endpoint_key(s.port, gid->second) != endpoint_key(s.port, endpoint)) {
                    throw std::runtime_error("QP local_gid_index must identify its source IP");
                }
                s.local_gid_index = gid->first; s.node_guid = d.node_guid;
                if ((qp_json.isMember("prefix_length") && s.network.prefix_length != host.prefix_length) ||
                    (qp_json.isMember("gateway") && (!host.has_gateway || s.network.gateway != host.gateway))) {
                    throw std::runtime_error("QP route must match its device host route");
                }
                s.network.prefix_length = host.prefix_length;
                s.network.has_gateway = host.has_gateway; s.network.gateway = host.gateway;
                // Wire keys and virtual addresses remain unchanged. The native
                // registry sees disjoint internal PD/MR IDs for each device.
                const auto pd_key = std::make_pair(d.id, s.local.protection_domain);
                const auto mr_key = std::make_pair(d.id, s.memory_id);
                auto pd = protection_domains.emplace(pd_key, protection_domains.size() + 1);
                auto mr = region_ids.emplace(mr_key, region_ids.size() + 1);
                s.local.protection_domain = pd.first->second; s.memory_id = mr.first->second;
                s.owner = (d.ordinal % per_pair) * pairs + s.port / 2;
                ++device_qps[d.id];
            } else if (qp_json.isMember("device_id") || qp_json.isMember("local_gid_index")) {
                throw std::runtime_error("Device QP fields require schema_version 3");
            }
            if (!logical_qp_ids.emplace(schema == 3 ? s.device_id : s.port, s.local_qpn).second) {
                throw std::runtime_error("local_qpn must be unique within its device or legacy port");
            }
            if (!identities.insert(identity_key(s.port, s.transport.endpoint, s.local_qpn)).second) {
                throw std::runtime_error("Duplicate endpoint/local_qpn identity");
            }
            const auto memory = memory_regions.find(s.memory_id);
            if (memory != memory_regions.end()) {
                const QpSpec &other = memory->second;
                if (s.port != other.port || s.local_address != other.local_address ||
                    s.memory_size != other.memory_size || s.local_rkey != other.local_rkey ||
                    s.local_lkey != other.local_lkey || s.seed != other.seed ||
                    s.local.protection_domain != other.local.protection_domain ||
                    s.has_initial_u64 != other.has_initial_u64 || s.initial_u64 != other.initial_u64 ||
                    s.initial_u64_offset != other.initial_u64_offset) {
                    throw std::runtime_error("Shared memory_id requires one port, owner and identical region configuration");
                }
            } else {
                memory_regions[s.memory_id] = s;
                total_memory += s.memory_size;
            }
            if (total_memory > memory_limit) { throw std::runtime_error("Registered memory exceeds max_memory_bytes"); }
            Admission &a = admissions[{s.owner, s.port}];
            qp_owners.insert(s.owner);
            ++a.count;
            if (memory == memory_regions.end()) { a.bytes += s.memory_size; }
            a.depth = std::max<uint32_t>(a.depth, s.outstanding + s.receives.size());
            uint64_t bytes = 0; Status status;
            checked(NativeRcEngineTransport::estimate_storage_bytes(s.local, s.transport,
                      std::max<uint32_t>(32, s.outstanding + s.receives.size()), bytes, &status), status);
            total_storage += bytes + s.memory_size + s.size +
                             uint64_t(s.outstanding) * sizeof(PendingWork) +
                             uint64_t(s.program.size()) * (sizeof(WorkSpec) + sizeof(uint64_t));
            // Every worker attached to a configured port pair must drain RX;
            // RSS may deliver a QP to a worker which owns no QPs itself.
            for (uint8_t worker = 0; worker < per_pair; ++worker) {
                participants.insert(worker * pairs + s.port / 2);
            }
            if (total_storage > storage_limit) {
                throw std::runtime_error("Native endpoint storage estimate exceeds max_native_storage_bytes");
            }
            specs.push_back(std::move(s));
        }
        std::map<uint8_t, uint32_t> port_devices;
        for (const auto &device : devices) {
            if (!device_qps.count(device.first)) { throw std::runtime_error("Every device requires at least one QP"); }
            ++port_devices[device.second.port];
            if (!scale.isNull() && device_qps[device.first] != scale["qps_per_device"].asUInt()) {
                throw std::runtime_error("scale qps_per_device does not match the configured QPs");
            }
        }
        for (const auto &port : port_devices) {
            if (!scale.isNull() && port.second != scale["devices_per_port"].asUInt()) {
                throw std::runtime_error("scale devices_per_port does not match the configured devices");
            }
        }
        for (const auto &entry : admissions) {
            const Admission &a = entry.second;
            uint64_t bytes = 0; Status status;
            checked(NativeEndpointEngine::estimate_storage_bytes(
                        engine_config(a.count, a.bytes, a.depth, capacity), bytes, &status), status);
            total_storage += bytes;
        }
        total_storage += uint64_t(participants.size()) * capacity * sizeof(RedirectedPacket);
        // Managers reserve at least one slot on every worker, including
        // workers which only forward RSS ingress or are not participating.
        const uint64_t control_slots = specs.size() + uint64_t(pairs) * per_pair - qp_owners.size();
        total_storage += control_slots * (16 * sizeof(CmEvent) + 16 * sizeof(CmTxFrame) +
                                           16 * sizeof(NativeNeighborEvent) + 8192);
        // Maps and runtime specs are private copies on each participating
        // worker. Include bounded routing/index metadata in the admission.
        total_storage += uint64_t(pairs) * per_pair * specs.size() * (sizeof(QpSpec) + 512);
        if (total_storage > storage_limit) {
            throw std::runtime_error("Native endpoint storage estimate exceeds max_native_storage_bytes");
        }
        admission = Json::Value(Json::objectValue);
        admission["estimated_storage_bytes"] = Json::UInt64(total_storage);
        admission["registered_memory_bytes"] = Json::UInt64(total_memory);
        admission["max_native_storage_bytes"] = Json::UInt64(storage_limit);
        admission["max_memory_bytes"] = Json::UInt64(memory_limit);
        admission["qp_count"] = Json::UInt64(specs.size());
        admission["device_count"] = Json::UInt64(devices.size());
        uint8_t ports[2]; core->get_port_ids(ports[0], ports[1]);
        for (const auto &entry : admissions) {
            if (entry.first.first != id) { continue; }
            const uint8_t port = entry.first.second;
            if (port != ports[0] && port != ports[1]) {
                throw std::runtime_error("QP owner is not attached to its configured port");
            }
            const size_t dir = port == ports[0] ? 0 : 1;
            const Admission &a = entry.second;
            engines[dir].reset(new NativeEndpointEngine()); Status status;
            checked(engines[dir]->configure(engine_config(a.count, a.bytes, a.depth, capacity), &status), status);
        }
        cm.reset(new NativeCmManager()); neighbors.reset(new NativeNeighborManager());
        NativeCmConfig cm_config;
        size_t local_connections = 0;
        for (const auto &s : specs) { if (s.owner == id) { ++local_connections; } }
        cm_config.max_connections = std::max<size_t>(1, local_connections);
        cm_config.max_listeners = cm_config.max_connections;
        cm_config.max_events = cm_config.max_connections * 4;
        cm_config.max_tx_frames = cm_config.max_connections * 4;
        cm_config.connection_id_base = (uint32_t(id + 1) << 24) | 1;
        cm_config.connection_id_limit = (uint32_t(id + 1) << 24) | 0xffffff;
        NativeNeighborConfig neighbor_config;
        size_t local_interfaces = schema == 3 ? 0 : local_connections;
        for (const auto &device : devices) {
            const DeviceSpec &d = device.second;
            const uint8_t owner = (d.ordinal % per_pair) * pairs + d.port / 2;
            if (owner == id) { local_interfaces += unsigned(d.has_ipv4) + unsigned(d.has_ipv6); }
        }
        neighbor_config.max_interfaces = std::max<size_t>(1, local_interfaces);
        neighbor_config.max_entries = std::max<size_t>(1, local_connections);
        neighbor_config.max_events = neighbor_config.max_entries * 2;
        neighbor_config.max_tx_frames = std::max(neighbor_config.max_entries, neighbor_config.max_interfaces) * 2;
        Status manager_status;
        checked(cm->configure(cm_config, &manager_status), manager_status);
        checked(neighbors->configure(neighbor_config, &manager_status), manager_status);
        qps.reserve(local_connections); cm_events.reserve(3 * local_connections);
        std::set<uint32_t> initialized_memory;
        std::map<std::string, NativeNeighborInterface> registered_interfaces;
        // A dual-stack host owns both addresses even when all current QPs use
        // only one GID. Host ARP/ND ownership is independent of QP allocation.
        for (const auto &device : devices) {
            const DeviceSpec &d = device.second;
            const uint8_t owner = (d.ordinal % per_pair) * pairs + d.port / 2;
            if (owner == id) { device_info.append(config["devices"][d.ordinal]); }
            for (bool ipv6 : {false, true}) {
                if (!(ipv6 ? d.has_ipv6 : d.has_ipv4)) { continue; }
                const NativeNeighborInterface &network = ipv6 ? d.ipv6 : d.ipv4;
                const std::string address = neighbor_key(d.port, network.endpoint);
                neighbor_routes[address] = owner;
                if (owner == id) {
                    registered_interfaces[address] = network;
                    Status status; checked(neighbors->register_interface(network, &status), status);
                }
            }
        }
        for (const QpSpec &s : specs) {
            routes[identity_key(s.port, s.transport.endpoint, s.local_qpn)] = s.owner;
            const std::string address = neighbor_key(s.port, s.transport.endpoint);
            const auto network_owner = neighbor_routes.emplace(address, s.owner);
            const bool dynamic = std::all_of(s.transport.endpoint.dst_mac, s.transport.endpoint.dst_mac + 6,
                                             [](uint8_t b) { return b == 0; });
            if (!network_owner.second && network_owner.first->second != s.owner && dynamic) {
                throw std::runtime_error("Dynamic neighbor QPs sharing an IP must share a memory owner");
            }
            if (s.managed && !s.active_connection) {
                if (!listener_routes.emplace(identity_key(s.port, s.transport.endpoint, s.service_id), s.owner).second) {
                    throw std::runtime_error("An endpoint/service_id may have only one passive QP");
                }
            }
            if (s.owner != id) { continue; }
            const size_t dir = s.port == ports[0] ? 0 : 1;
            NativeEndpointEngine &engine = *engines[dir];
            const auto network = registered_interfaces.find(address);
            if (network == registered_interfaces.end()) {
                registered_interfaces[address] = s.network;
                Status status; checked(neighbors->register_interface(s.network, &status), status);
            } else if (memcmp(network->second.endpoint.src_mac, s.network.endpoint.src_mac, 6) ||
                       network->second.prefix_length != s.network.prefix_length ||
                       network->second.has_gateway != s.network.has_gateway ||
                       network->second.gateway != s.network.gateway) {
                throw std::runtime_error("Shared local IP requires identical MAC, prefix and gateway");
            }
            EngineMemoryConfig memory;
            memory.memory_id = s.memory_id;
            memory.window.base_address = s.local_address;
            memory.window.length = s.memory_size;
            memory.window.rkey = s.local_rkey;
            memory.window.lkey = s.local_lkey;
            memory.window.protection_domain = s.local.protection_domain;
            memory.window.access = all_access();
            Status status;
            if (initialized_memory.insert(s.memory_id).second) {
                checked(engine.add_memory(memory, &status), status);
                std::vector<uint8_t> source;
                pattern(source, s.size, s.seed);
                checked(engine.memory(s.memory_id)->write(s.local_address,
                            ByteSpan(source.data(), source.size()), MemoryAccess::LOCAL_WRITE, &status), status);
                if (s.has_initial_u64) {
                    checked(engine.memory(s.memory_id)->write(s.local_address + s.initial_u64_offset,
                        ByteSpan(reinterpret_cast<const uint8_t *>(&s.initial_u64), 8),
                        MemoryAccess::LOCAL_WRITE, &status), status);
                }
            }
        }
        // Register every region before attaching any receive queue. SGEs may
        // refer to another region in the same owner/protection domain.
        for (const QpSpec &s : specs) {
            if (s.owner != id) { continue; }
            const size_t dir = s.port == ports[0] ? 0 : 1;
            NativeEndpointEngine &engine = *engines[dir]; Status status;
            RuntimeQp runtime;
            runtime.spec = s; runtime.id = QpId(SessionGeneration(generation), s.index);
            runtime.engine = &engine;
            runtime.total_work = uint64_t(s.iterations) * s.work_per_iteration;
            runtime.pending.resize(s.outstanding);
            runtime.step_posts.resize(s.program.size(), 0);
            pattern(runtime.expected, s.size, s.peer_seed);
            runtime.connection_state = s.managed ? "configured" : "static";
            if (!s.managed) { attach(runtime, s.peer, s.transport.endpoint, true); }
            else if (!s.active_connection) {
                CmListenRequest listener;
                listener.listener_id = uint64_t(s.index) + 1; listener.service_id = s.service_id;
                listener.port = s.port; listener.endpoint = s.transport.endpoint; listener.backlog = s.backlog;
                listener.ip_cma = s.ip_cma;
                checked(cm->listen(listener, &status), status);
                runtime.connection_state = "listening";
            }
            operation_qps[uint64_t(s.index) + 1] = qps.size();
            qps.push_back(std::move(runtime));
        }
        completions.reserve(BURST);
        participating = participants.count(id) != 0;
        if (participating) {
            Mailbox &box = mailboxes[id]; std::lock_guard<std::mutex> guard(box.lock);
            box.clear_locked(); box.packets.resize(capacity); box.generation = generation;
        }
        configured = true;
        final_stats = Json::Value();
    }

    CmParameters cm_parameters(const QpSpec &s) const {
        CmParameters p;
        if (s.node_guid) { p.ca_guid = s.node_guid; }
        p.qpn = s.local_qpn; p.psn = s.local.starting_sq_psn;
        p.responder_resources = s.local.max_dest_rd_atomic; p.initiator_depth = s.local.max_rd_atomic;
        p.mtu = 1; for (uint16_t mtu = 256; mtu < s.local.pmtu; mtu *= 2) { ++p.mtu; }
        p.retry_count = s.local.retry_count; p.rnr_retry_count = s.local.rnr_retry_count;
        p.ack_timeout = s.local.timeout_exp;
        return p;
    }

    void attach(RuntimeQp &q, const QpPeerConfig &peer, const NativeEndpoint &endpoint, bool activate) {
        Status status;
        if (!q.attached) {
            std::unique_ptr<QueuePair> qp(new QueuePair(q.id));
            MemoryWindowConfig window;
            window.base_address = q.spec.local_address; window.length = q.spec.memory_size;
            window.rkey = q.spec.local_rkey; window.lkey = q.spec.local_lkey;
            window.protection_domain = q.spec.local.protection_domain; window.access = all_access();
            QpConfig local = q.spec.local;
            if (q.spec.managed) {
                CmConnectionInfo connection;
                if (cm->connection(q.connection_id, connection)) {
                    // IB CM Starting PSN advertises the sender's expected RQ,
                    // so the received value initializes our local SQ.
                    local.starting_sq_psn = connection.send_psn;
                    local.retry_count = connection.retry_count;
                    local.rnr_retry_count = connection.rnr_retry_count;
                    local.max_rd_atomic = std::min(local.max_rd_atomic, connection.remote.responder_resources);
                    local.max_dest_rd_atomic = std::min(q.spec.local.max_dest_rd_atomic, connection.remote.initiator_depth);
                }
            }
            checked(qp->create(local, window, &status), status);
            checked(qp->modify_to_rtr(peer, &status), status);
            if (activate) { checked(qp->modify_to_rts(&status), status); }
            EngineQpBinding binding;
            binding.memory_id = q.spec.memory_id; binding.peer_endpoint = 1;
            binding.egress_port = q.spec.port; binding.peer_ingress_port = q.spec.port;
            NativeTransportConfig transport = q.spec.transport; transport.endpoint = endpoint;
            checked(q.engine->add_native_qp(std::move(qp), transport, binding, &status), status);
            for (const auto &receive : q.spec.receives) { checked(q.engine->post_receive(q.id, receive, &status), status); }
            q.attached = true;
        } else if (activate) { checked(q.engine->activate_qp(q.id, &status), status); }
        q.connected = activate;
    }

    void connect(RuntimeQp &q, const NativeEndpoint &path, uint64_t now) {
        CmConnectRequest request;
        request.operation_id = uint64_t(q.spec.index) + 1;
        request.service_id = q.spec.service_id; request.port = q.spec.port;
        request.source_port = q.spec.source_port; request.ip_cma = q.spec.ip_cma;
        request.endpoint = path; request.parameters = cm_parameters(q.spec);
        request.private_data_length = q.spec.private_data.size();
        std::copy(q.spec.private_data.begin(), q.spec.private_data.end(), request.private_data.begin());
        Status status;
        checked(cm->connect(request, now, q.connection_id, &status), status);
        connection_qps[q.connection_id] = &q - qps.data();
        q.connection_state = "req_sent";
    }

    RuntimeQp *operation_qp(uint64_t operation_id) {
        const auto found = operation_qps.find(operation_id);
        return found == operation_qps.end() ? nullptr : &qps[found->second];
    }

    RuntimeQp *event_qp(const CmEvent &event) {
        return operation_qp(event.connection.active ? event.connection.operation_id : event.connection.listener_id);
    }

    void service_neighbors(uint64_t now) {
        neighbors->poll(now);
        NativeNeighborEvent event;
        while (neighbors->pop_event(event)) {
            RuntimeQp *q = operation_qp(event.operation_id);
            if (!q || q->terminal) { continue; }
            if (event.type == NeighborEventType::RESOLVED) { connect(*q, event.endpoint, now); }
            else { q->terminal = true; ++q->connection_failures; error = "Neighbor resolution timed out or was cancelled"; }
        }
    }

    QpPeerConfig cm_peer(const RuntimeQp &q, const CmConnectionInfo &info) const {
        QpPeerConfig peer = q.spec.peer;
        peer.remote_qpn = info.remote.qpn;
        peer.starting_psn = q.spec.local.starting_sq_psn;
        peer.pmtu = uint16_t(128U << (info.negotiated_mtu ? info.negotiated_mtu : info.remote.mtu));
        peer.max_rd_atomic = info.remote.responder_resources;
        return peer;
    }

    void accept(RuntimeQp &q, const CmConnectionInfo &info, ByteSpan private_data, uint64_t now) {
        if (cm_parameters(q.spec).mtu < info.remote.mtu) {
            Status status; checked(cm->reject(q.connection_id, 26, ByteSpan(), now, &status), status);
            q.connection_state = "rejected"; return;
        }
        attach(q, cm_peer(q, info), info.endpoint, false);
        CmParameters parameters = cm_parameters(q.spec);
        parameters.initiator_depth = std::min(parameters.initiator_depth, info.remote.responder_resources);
        parameters.responder_resources = std::min(parameters.responder_resources, info.remote.initiator_depth);
        Status status; checked(cm->accept(q.connection_id, parameters, private_data, now, &status), status);
        q.connection_state = "rep_sent";
    }

    void service_cm(uint64_t now) {
        cm->poll(now);
        CmEvent event;
        while (cm->pop_event(event)) {
            RuntimeQp *q = event_qp(event);
            if (!q) { ++worker_errors; error = "CM event has no endpoint owner"; continue; }
            if (q->connection_id && q->connection_id != event.connection.connection_id) {
                if (event.type == CmEventType::CONNECT_REQUEST) {
                    Status status; cm->reject(event.connection.connection_id, 8, ByteSpan(), now, &status);
                }
                continue;
            }
            q->connection_id = event.connection.connection_id;
            connection_qps[q->connection_id] = q - qps.data();
            q->connection_state = cm_state_to_string(event.connection.state);
            if (cm_events.size() < cm_events.capacity()) { cm_events.push_back(event); }
            else { ++worker_errors; error = "CM public event queue overflow"; }
            if (event.type == CmEventType::CONNECT_REQUEST) {
                q->connection_state = "pending_accept";
                if (q->spec.auto_accept) {
                    accept(*q, event.connection, ByteSpan(q->spec.private_data.data(), q->spec.private_data.size()), now);
                }
            } else if (event.type == CmEventType::CONNECT_RESPONSE) {
                attach(*q, cm_peer(*q, event.connection), event.connection.endpoint, false);
                Status status; checked(cm->confirm(q->connection_id, now, &status), status);
                q->connection_state = "rtu_pending";
            } else if (event.type == CmEventType::ESTABLISHED) {
                attach(*q, cm_peer(*q, event.connection), event.connection.endpoint, true);
                q->ever_established = true; q->connection_state = "established";
            } else if (event.type == CmEventType::DISCONNECTED) {
                q->connected = false; q->closed = true; q->connection_state = "disconnected";
                if (q->attached) {
                    Status status; checked(q->engine->close_qp(q->id, ErrorCode::ABORTED,
                        "CM connection disconnected", &status), status);
                }
            } else {
                q->connected = false; q->closed = true; q->terminal = true; ++q->connection_failures;
                error = std::string("CM ") + cm_event_to_string(event.type);
                if (q->attached) {
                    Status status; checked(q->engine->close_qp(q->id, ErrorCode::ABORTED, error, &status), status);
                }
            }
        }
    }

    bool verify_memory(RuntimeQp &q) {
        ByteSpan bytes; Status status;
        const uint64_t offset = q.spec.has_expected_u64 ? q.spec.initial_u64_offset : q.spec.size;
        const uint64_t length = q.spec.has_expected_u64 ? 8 : q.spec.size;
        if (!q.engine->memory(q.spec.memory_id)->view(q.spec.local_address + offset,
                    length, MemoryAccess::LOCAL_READ, bytes, &status)) { return false; }
        if (q.spec.has_expected_u64) {
            uint64_t value = 0; memcpy(&value, bytes.data, 8);
            return value == q.spec.expected_u64;
        }
        return pattern_matches(bytes, q.spec.peer_seed);
    }

    bool verify_receive(RuntimeQp &q, const CompletionRecord &completion,
                        const NativeReceiveRequest &request) {
        if (completion.kind == WorkKind::RECEIVE_RDMA_WITH_IMMEDIATE) { return true; }
        uint64_t left = completion.completed_bytes;
        uint32_t state = q.spec.peer_seed ? q.spec.peer_seed : 1;
        uint64_t offset = 0;
        Status status;
        for (const auto &sge : request.sges) {
            const uint64_t length = std::min(left, sge.length);
            if (!length) { continue; }
            ByteSpan bytes;
            if (!q.engine->memory(q.spec.memory_id)->view(sge.address, length,
                        MemoryAccess::LOCAL_READ, bytes, &status)) { return false; }
            for (size_t n = 0; n < bytes.size; ++n) {
                state ^= state << 13; state ^= state >> 17; state ^= state << 5;
                if (bytes.data[n] != uint8_t(state ^ offset ^ (offset >> 8))) { return false; }
                ++offset;
            }
            left -= length;
        }
        return left == 0;
    }

    void drain_completions(bool all = false, bool shutdown = false) {
        for (auto &q : qps) {
            if (!q.attached) { continue; }
            NativeTransportRuntimeView view; Status status;
            if (!shutdown && !q.closed && q.engine->transport_runtime_view(q.id, view, &status) &&
                view.qp.state == QpState::ERR) { q.transport_failed = q.terminal = true; }
            do {
                completions.clear();
                if (!q.engine->drain_completions_bounded(q.id, completions, BURST, &status)) {
                    ++worker_errors; error = status.message(); break;
                }
                for (const CompletionRecord &c : completions) {
                    const bool success = c.state == CompletionState::SUCCESS && c.error == ErrorCode::OK;
                    if (work_is_receive(c.kind)) {
                        if (q.closed && c.state == CompletionState::FLUSHED) { continue; }
                        const NativeReceiveRequest *request = nullptr;
                        for (const auto &r : q.spec.receives) { if (r.wr_id == c.wr_id) { request = &r; break; } }
                        if (!success || !request) { ++q.receive_failures; q.terminal = true; continue; }
                        ++q.received; q.received_bytes += c.completed_bytes;
                        if (!verify_receive(q, c, *request)) { ++q.verification_failures; q.terminal = true; }
                        else if (c.kind == WorkKind::RECEIVE) { q.verified_bytes += c.completed_bytes; }
                        q.has_immediate = c.has_immediate; q.last_immediate = c.immediate_data;
                        q.has_invalidate = c.has_invalidate; q.last_invalidated_rkey = c.invalidated_rkey;
                        if (q.spec.receive_repost && !shutdown && !q.terminal &&
                            (!q.spec.expected_receives || q.received < q.spec.expected_receives) &&
                            !q.engine->post_receive(q.id, *request, &status)) {
                            ++q.receive_failures; q.terminal = true; error = status.message();
                        }
                        continue;
                    }
                    PendingWork *pending = nullptr;
                    for (auto &p : q.pending) { if (p.active && p.sequence == c.id.sequence()) { pending = &p; break; } }
                    if (!pending) { ++worker_errors; q.terminal = true; error = "Completion has no matching submitted work"; continue; }
                    if (!success) { ++q.failed; q.terminal = true; }
                    else {
                        ++q.completed; q.completed_bytes += c.completed_bytes;
                        bool valid = true;
                        if (c.kind == WorkKind::READ) {
                            ByteSpan bytes;
                            valid = q.engine->memory(q.spec.memory_id)->view(
                                q.spec.local_address + pending->work.local_offset,
                                pending->work.length, MemoryAccess::LOCAL_READ, bytes, &status) &&
                                pattern_matches(bytes, pending->work.expected_seed);
                            if (valid) { q.verified_bytes += c.completed_bytes; }
                        } else if (c.kind == WorkKind::COMPARE_SWAP || c.kind == WorkKind::FETCH_ADD) {
                            if (pending->work.has_expected_original) {
                                valid = c.atomic_original_value == pending->expected_original;
                                if (valid) { q.verified_bytes += 8; }
                            } else { ++q.unverified_results; }
                        }
                        if (!valid) { ++q.verification_failures; q.terminal = true; }
                    }
                    pending->active = false;
                }
            } while (all && completions.size() == BURST);
        }
    }

    void submit(uint64_t now) {
        for (auto &q : qps) {
            if (q.terminal || !q.connected || q.spec.program.empty()) { continue; }
            uint32_t admitted = 0;
            while (q.posted - q.completed - q.failed < q.spec.outstanding &&
                   (!q.spec.iterations || q.posted < q.total_work) && admitted++ < BURST) {
                const WorkSpec &work = q.spec.program[q.next_work];
                PendingWork *pending = nullptr;
                for (auto &p : q.pending) { if (!p.active) { pending = &p; break; } }
                if (!pending) { break; }
                PostedWork posted; Status status; bool ok = false;
                if (work.operation == ProgramOperation::WRITE || work.operation == ProgramOperation::WRITE_IMM) {
                    EngineWriteRequest request;
                    request.source_memory_id = q.spec.memory_id;
                    request.source_address = q.spec.local_address + work.local_offset;
                    request.length = work.length;
                    request.remote_address = q.spec.remote_address + work.remote_offset;
                    request.remote_rkey = q.spec.remote_rkey;
                    request.with_immediate = work.operation == ProgramOperation::WRITE_IMM;
                    request.immediate_data = work.immediate_data;
                    request.signalled = true; request.fenced = work.fenced;
                    ok = q.engine->post_write(q.id, request, now, posted, &status);
                } else if (work.operation == ProgramOperation::READ) {
                    EngineReadRequest request;
                    request.destination_memory_id = q.spec.memory_id;
                    request.destination_address = q.spec.local_address + work.local_offset;
                    request.length = work.length;
                    request.remote_address = q.spec.remote_address + work.remote_offset;
                    request.remote_rkey = q.spec.remote_rkey;
                    request.signalled = true; request.fenced = work.fenced;
                    ok = q.engine->post_read(q.id, request, now, posted, &status);
                } else if (is_atomic(work.operation)) {
                    EngineAtomicRequest request;
                    request.destination_memory_id = q.spec.memory_id;
                    request.destination_address = q.spec.local_address + work.local_offset;
                    request.remote_address = q.spec.remote_address + work.remote_offset;
                    request.remote_rkey = q.spec.remote_rkey;
                    request.fetch_add = work.operation == ProgramOperation::FAA;
                    request.compare = work.compare; request.swap_add = work.swap_add;
                    request.signalled = true; request.fenced = work.fenced;
                    ok = q.engine->post_atomic(q.id, request, now, posted, &status);
                } else {
                    EngineSendRequest request;
                    request.source_memory_id = q.spec.memory_id;
                    request.source_address = q.spec.local_address + work.local_offset;
                    request.length = work.length;
                    request.with_immediate = work.operation == ProgramOperation::SEND_IMM;
                    request.immediate_data = work.immediate_data;
                    request.with_invalidate = work.operation == ProgramOperation::SEND_INV;
                    request.invalidate_rkey = work.invalidate_rkey;
                    request.signalled = true; request.fenced = work.fenced;
                    ok = q.engine->post_send(q.id, request, now, posted, &status);
                }
                if (!ok) {
                    if (status.code() != ErrorCode::RESOURCE_EXHAUSTED) {
                        ++q.submission_failures; q.terminal = true; error = status.message();
                    }
                    break;
                }
                pending->active = true; pending->sequence = posted.id.sequence(); pending->work = work;
                pending->expected_original = work.expected_original;
                if (work.operation == ProgramOperation::FAA) {
                    pending->expected_original += q.step_posts[q.next_work] * work.swap_add;
                } else if (work.operation == ProgramOperation::CAS && q.step_posts[q.next_work] &&
                           work.expected_original == work.compare) {
                    pending->expected_original = work.swap_add;
                }
                ++q.step_posts[q.next_work]; ++q.posted;
                if (++q.work_repeat == work.repeat) {
                    q.work_repeat = 0; q.next_work = (q.next_work + 1) % q.spec.program.size();
                }
            }
        }
    }

    void ingress(rte_mbuf_t *m, uint8_t port) {
        uint8_t ports[2]; core->get_port_ids(ports[0], ports[1]);
        const size_t dir = port == ports[0] ? 0 : 1;
        if ((port != ports[0] && port != ports[1]) ||
            rte_pktmbuf_pkt_len(m) > scratch.size()) {
            ++ingress_drops; rte_pktmbuf_free(m); return;
        }
        const size_t size = rte_pktmbuf_pkt_len(m);
        const uint8_t *data = rte_pktmbuf_mtod(m, const uint8_t *);
        if (rte_pktmbuf_data_len(m) != size) {
            if (!mbuf_copy(m, 0, scratch.data(), size)) {
                ++malformed; rte_pktmbuf_free(m); return;
            }
            data = scratch.data();
        }
        NativeEndpoint target;
        if (neighbor_target(m, target) && neighbor_routes.count(neighbor_key(port, target))) {
            Status status;
            if (!neighbors->receive(port, ByteSpan(data, size), monotonic_ns(), &status)) { ++malformed; }
            rte_pktmbuf_free(m); return;
        }
        NativeEndpoint local; uint32_t qpn = 0;
        if (wire_qpn(m, qpn, local) && qpn == 1) {
            Status control_status;
            if (!cm->receive(port, ByteSpan(data, size), monotonic_ns(), &control_status)) { ++malformed; }
            rte_pktmbuf_free(m); return;
        }
        EngineWireFrame metadata;
        metadata.generation = SessionGeneration(generation);
        metadata.source_endpoint = 1; metadata.destination_endpoint = 0;
        metadata.ingress_port = port;
        Status status;
        if (!engines[dir] || !engines[dir]->enqueue_ingress_copy(metadata, data, size, &status)) {
            ++ingress_drops;
        }
        rte_pktmbuf_free(m);
    }

    void receive() {
        RedirectedPacket pending[BURST]; size_t count = 0;
        Mailbox &box = mailboxes[id];
        if (box.pending.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> guard(box.lock);
            while (count < BURST && box.count) {
                pending[count++] = box.packets[box.head];
                box.head = (box.head + 1) % box.packets.size(); --box.count;
            }
            box.pending.store(box.count, std::memory_order_release);
        }
        for (size_t n = 0; n < count; ++n) {
            if (pending[n].generation == generation) { ingress(pending[n].mbuf, pending[n].port); }
            else { ++redirect_drops; rte_pktmbuf_free(pending[n].mbuf); }
        }
        CVirtualIF *vif = core->m_node_gen.m_v_if;
        uint8_t ports[2]; core->get_port_ids(ports[0], ports[1]);
        for (int dir = 0; dir < 2; ++dir) {
            rte_mbuf_t *packets[BURST];
            const uint16_t count = vif->rx_burst(dir, packets, BURST);
            for (uint16_t n = 0; n < count; ++n) {
                rte_mbuf_t *m = packets[n]; uint32_t qpn = 0; NativeEndpoint local;
                if (!wire_qpn(m, qpn, local)) {
                    NativeEndpoint target;
                    const auto route = neighbor_target(m, target) ?
                        neighbor_routes.find(neighbor_key(ports[dir], target)) : neighbor_routes.end();
                    if (route != neighbor_routes.end()) {
                        TrexCaptureMngr::getInstance().handle_pkt_rx_dp(m, ports[dir]); ++rx_packets;
                        if (route->second == id) { ingress(m, ports[dir]); }
                        else if (redirect(route->second, generation, ports[dir], m)) { ++redirected; }
                        else { ++redirect_drops; rte_pktmbuf_free(m); }
                        continue;
                    }
                    CTcpPerThreadCtx *ctx = dir ? core->m_s_tcp : core->m_c_tcp;
                    ctx->m_ft.rx_handle_packet(ctx, m, true, ports[dir]);
                    continue;
                }
                TrexCaptureMngr::getInstance().handle_pkt_rx_dp(m, ports[dir]);
                ++rx_packets;
                uint8_t owner = 255;
                if (qpn == 1) {
                    const size_t size = rte_pktmbuf_pkt_len(m);
                    NativeEndpoint path; CmMessage message; Status status;
                    if (size <= scratch.size() && mbuf_copy(m, 0, scratch.data(), size) &&
                        NativeCmCodec::decode_frame(ByteSpan(scratch.data(), size), path, message, &status)) {
                        if (message.remote_id) { owner = (message.remote_id >> 24) - 1; }
                        else {
                            const auto listener = listener_routes.find(identity_key(
                                ports[dir], received_local(path), message.service_id));
                            owner = listener == listener_routes.end() ? ports[dir] / 2 : listener->second;
                        }
                    }
                } else {
                    const auto route = routes.find(identity_key(ports[dir], local, qpn));
                    if (route != routes.end()) { owner = route->second; }
                }
                if (owner == 255) { ++ingress_drops; rte_pktmbuf_free(m); }
                else if (owner == id) { ingress(m, ports[dir]); }
                else if (redirect(owner, generation, ports[dir], m)) { ++redirected; }
                else { ++redirect_drops; rte_pktmbuf_free(m); }
            }
        }
    }

    void transmit(uint64_t now) {
        CVirtualIF *vif = core->m_node_gen.m_v_if;
        for (size_t dir = 0; dir < engines.size(); ++dir) {
            if (!engines[dir] || engines[dir]->state() != EngineState::RUNNING) { continue; }
            NativeEndpointEngine &engine = *engines[dir];
            rte_mbuf_t *packets[BURST]; uint64_t sequences[BURST]; RuntimeQp *owners[BURST];
            uint16_t count = 0; bool valid_owners = true;
            for (; count < BURST; ++count) {
                const EngineWireFrame *frame = engine.peek_tx_at(count);
                if (!frame) { break; }
                RuntimeQp *owner = operation_qp(uint64_t(frame->source_qp_index) + 1);
                if (!owner || owner->engine != &engine || owner->spec.port != frame->egress_port) {
                    valid_owners = false; ++worker_errors; error = "Engine TX frame has no owning QP"; break;
                }
                rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_local(core->m_node_gen.m_socket_id, frame->bytes.size());
                if (!m) { ++allocation_failures; break; }
                char *data = rte_pktmbuf_append(m, frame->bytes.size());
                if (!data) { rte_pktmbuf_free(m); ++allocation_failures; break; }
                memcpy(data, frame->bytes.data(), frame->bytes.size());
                packets[count] = m; sequences[count] = frame->sequence; owners[count] = owner;
            }
            if (!valid_owners) {
                for (uint16_t n = 0; n < count; ++n) { rte_pktmbuf_free(packets[n]); }
                engine.enter_error(ErrorCode::INTERNAL_ERROR, error); continue;
            }
            if (!count) { continue; }
            const uint16_t accepted = vif->tx_burst_nonblocking(dir, packets, count);
            // The PMD owns accepted mbufs already. Retain engine frames until
            // this point, so descriptor backpressure cannot arm retries early.
            Status status;
            bool healthy = true;
            for (uint16_t n = 0; n < accepted; ++n) {
                // Count PMD acceptance even if a subsequent internal publish
                // fails: these mbufs already belong to the carrier.
                ++owners[n]->data_tx_packets;
                if (healthy && !engine.accept_tx(sequences[n], now, &status)) {
                    healthy = false; ++worker_errors; error = status.message();
                    engine.enter_error(status.code(), error);
                }
            }
            tx_packets += accepted;
            if (accepted < count) { ++tx_backpressure; }
            for (uint16_t n = accepted; n < count; ++n) { rte_pktmbuf_free(packets[n]); }
        }
    }

    template <class Manager> void transmit_control(Manager &manager, uint64_t now) {
        CmTxFrame frame;
        uint8_t ports[2]; core->get_port_ids(ports[0], ports[1]);
        for (size_t n = 0; n < BURST && manager.peek_tx(frame); ++n) {
            const uint8_t dir = frame.port == ports[0] ? 0 : 1;
            rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_local(core->m_node_gen.m_socket_id, frame.length);
            if (!m) { ++allocation_failures; break; }
            char *data = rte_pktmbuf_append(m, frame.length);
            if (!data) { rte_pktmbuf_free(m); ++allocation_failures; break; }
            memcpy(data, frame.data.data(), frame.length);
            const bool accepted = core->m_node_gen.m_v_if->tx_burst_nonblocking(dir, &m, 1) == 1;
            Status status; checked(manager.complete_tx(frame.token, accepted, now, &status), status);
            if (!accepted) { rte_pktmbuf_free(m); ++tx_backpressure; break; }
            ++tx_packets;
        }
    }

    void abort(ErrorCode code, const std::string &reason) {
        for (auto &engine : engines) {
            if (engine && engine->state() == EngineState::RUNNING) { engine->enter_error(code, reason); }
        }
        drain_completions(true, true);
        running = false;
    }

    bool poll() {
        if (!configured || !started || !participating) { return false; }
        const uint64_t now = monotonic_ns();
        receive();
        service_neighbors(now); service_cm(now);
        for (auto &engine : engines) {
            if (!engine || engine->state() != EngineState::RUNNING) { continue; }
            EnginePollResult progress; Status status;
            if (!engine->poll(now, progress, &status)) {
                ++worker_errors; error = status.message();
                engine->enter_error(status.code(), error);
            }
        }
        drain_completions();
        if (running) { submit(now); }
        transmit(now); transmit_control(*neighbors, now); transmit_control(*cm, now);
        if (running && now >= deadline) {
            bool unfinished = false;
            for (const auto &q : qps) {
                if ((!q.spec.program.empty() &&
                    ((q.spec.iterations && q.completed < q.total_work) ||
                     q.posted != q.completed + q.failed)) ||
                    q.received < q.spec.expected_receives) { unfinished = true; }
            }
            if (unfinished) { ++worker_errors; error = "RoCEv2 workload deadline expired before completion"; }
            if (unfinished) { abort(ErrorCode::TIMEOUT, error); }
            else { running = false; }
        }
        // A running responder must stay hot even when it has no local work.
        // The caller still checks CP messages before every poll iteration.
        return true;
    }

    Json::Value stats(bool verify) {
        if (!configured && !final_stats.isNull()) { return final_stats; }
        Json::Value result(Json::objectValue);
        result["thread_id"] = id; result["configured"] = configured;
        result["running"] = running; result["done"] = true;
        result["passed"] = started && worker_errors == 0;
        result["qps"] = Json::arrayValue;
        if (!admission.isNull()) { result["admission"] = admission; }
        if (!scale.isNull()) { result["scale"] = scale; }
        if (!device_info.isNull()) { result["device_info"] = device_info; }
        uint64_t errors = worker_errors;
        for (auto &q : qps) {
            Json::Value entry(Json::objectValue);
            NativeTransportRuntimeView view; Status status;
            const bool have_view = q.attached && q.engine->transport_runtime_view(q.id, view, &status);
            const uint64_t deliveries = have_view ? view.counters.deliveries : 0;
            const uint64_t served_reads = have_view ? view.counters.read_requests_received : 0;
            const bool finite_done = q.terminal ||
                ((!q.spec.managed || q.ever_established) && (q.spec.program.empty() || (q.spec.iterations && q.completed == q.total_work)) &&
                 q.received >= q.spec.expected_receives);
            bool passed = started && !q.failed && !q.receive_failures && !q.verification_failures &&
                          !q.transport_failed && !q.unverified_results && !q.connection_failures && !q.submission_failures &&
                          q.received >= q.spec.expected_receives &&
                          (!q.spec.managed || q.ever_established);
            if (!q.spec.program.empty()) {
                passed = passed && (q.spec.iterations ? q.completed == q.total_work :
                                                     !running && q.completed > 0);
            } else if (!q.spec.expect_writes && !q.spec.expected_receives) {
                // A passive Read responder proves it actually served work;
                // an unused endpoint cannot validate as successful traffic.
                passed = passed && (served_reads > 0 ||
                    (have_view && view.counters.atomic_requests_received > 0 && q.spec.has_expected_u64));
            }
            bool memory_ok = true;
            if (q.spec.expect_writes || q.spec.has_expected_u64) {
                memory_ok = (deliveries > 0 || (have_view && view.counters.atomic_requests_received > 0)) &&
                            (!verify || verify_memory(q));
                passed = passed && memory_ok;
            }
            entry["port"] = q.spec.port; entry["local_qpn"] = q.spec.local_qpn;
            entry["device_id"] = q.spec.device_id;
            entry["local_gid_index"] = q.spec.local_gid_index;
            entry["node_guid"] = Json::UInt64(q.spec.node_guid);
            entry["operation"] = q.spec.operation;
            entry["connection_state"] = q.connection_state;
            entry["connection_id"] = q.connection_id;
            entry["ever_established"] = q.ever_established;
            entry["state"] = have_view ? qp_state_to_string(view.qp.state) : "UNKNOWN";
            entry["posted"] = Json::UInt64(q.posted);
            entry["completed"] = Json::UInt64(q.completed);
            entry["failed"] = Json::UInt64(q.failed);
            entry["connection_failures"] = Json::UInt64(q.connection_failures);
            entry["submission_failures"] = Json::UInt64(q.submission_failures);
            entry["completed_bytes"] = Json::UInt64(q.completed_bytes);
            entry["verification_failures"] = Json::UInt64(q.verification_failures + (verify && !memory_ok ? 1 : 0));
            entry["verified_bytes"] = Json::UInt64(q.verified_bytes +
                (verify && (q.spec.expect_writes || q.spec.has_expected_u64) && memory_ok ?
                    (q.spec.has_expected_u64 ? 8 : q.spec.size) : 0));
            entry["received_writes"] = Json::UInt64(deliveries);
            entry["served_reads"] = Json::UInt64(served_reads);
            entry["expected_work"] = Json::UInt64(q.total_work);
            entry["received"] = Json::UInt64(q.received);
            entry["received_bytes"] = Json::UInt64(q.received_bytes);
            entry["receive_failures"] = Json::UInt64(q.receive_failures);
            entry["has_immediate"] = q.has_immediate; entry["last_immediate"] = q.last_immediate;
            entry["has_invalidate"] = q.has_invalidate; entry["last_invalidated_rkey"] = q.last_invalidated_rkey;
            entry["unverified_atomic_results"] = Json::UInt64(q.unverified_results);
            entry["atomic_requests_received"] = Json::UInt64(have_view ? view.counters.atomic_requests_received : 0);
            entry["atomic_replay_hits"] = Json::UInt64(have_view ? view.counters.atomic_replay_hits : 0);
            entry["retransmissions"] = Json::UInt64(have_view ? view.counters.retransmit_packets : 0);
            entry["icrc_errors"] = Json::UInt64(have_view ? view.counters.icrc_errors : 0);
            entry["endpoint_mismatch_packets"] = Json::UInt64(have_view ? view.counters.endpoint_mismatch_packets : 0);
            entry["malformed_rc_packets"] = Json::UInt64(have_view ? view.counters.malformed_packets : 0);
            entry["data_tx_packets"] = Json::UInt64(q.data_tx_packets);
            entry["data_tx_packets_queued"] = Json::UInt64(have_view ? view.counters.tx_packets : 0);
            entry["data_rx_packets"] = Json::UInt64(have_view ? view.counters.rx_packets : 0);
            entry["done"] = finite_done || !running;
            entry["passed"] = passed;
            const uint64_t qp_errors = q.failed + q.receive_failures + q.verification_failures + q.unverified_results +
                q.connection_failures + q.submission_failures +
                (q.transport_failed && !q.failed ? 1 : 0) + (verify && !memory_ok ? 1 : 0);
            entry["errors"] = Json::UInt64(qp_errors);
            errors += qp_errors;
            result["done"] = result["done"].asBool() && entry["done"].asBool();
            result["passed"] = result["passed"].asBool() && passed;
            result["qps"].append(entry);
        }
        if (cm) {
            const CmStatistics &c = cm->statistics();
            Json::Value state(Json::objectValue);
            state["established"] = Json::UInt64(c.established); state["disconnected"] = Json::UInt64(c.disconnected);
            state["retries"] = Json::UInt64(c.retries); state["timeouts"] = Json::UInt64(c.timeouts);
            state["rejected"] = Json::UInt64(c.rejected); state["malformed"] = Json::UInt64(c.malformed);
            result["cm"] = state;
        }
        result["errors"] = Json::UInt64(errors);
        result["error"] = error;
        result["rx_packets"] = Json::UInt64(rx_packets);
        result["tx_packets"] = Json::UInt64(tx_packets);
        result["redirected_packets"] = Json::UInt64(redirected);
        result["redirect_drops"] = Json::UInt64(redirect_drops);
        result["ingress_drops"] = Json::UInt64(ingress_drops);
        result["malformed_packets"] = Json::UInt64(malformed);
        result["tx_backpressure"] = Json::UInt64(tx_backpressure);
        result["allocation_failures"] = Json::UInt64(allocation_failures);
        return result;
    }

    Json::Value command(const std::string &name, const Json::Value &config) {
        if (name == "configure") {
            // Reject reconfiguration without releasing the live session. A
            // failed first-time configuration still rolls back partial state.
            if (configured) {
                throw std::runtime_error("Stop the configured endpoint before reconfiguration");
            }
            try { configure(config); }
            catch (...) { release(); throw; }
        } else if (name == "start") {
            if (!configured || started) { throw std::runtime_error("Endpoint must be freshly configured before start"); }
            Status status;
            for (auto &engine : engines) { if (engine) { checked(engine->start(&status), status); } }
            started = true; running = participating;
            const uint64_t now = monotonic_ns();
            for (auto &q : qps) {
                if (q.spec.managed && q.spec.active_connection) {
                    checked(neighbors->resolve(uint64_t(q.spec.index) + 1, q.spec.port,
                        q.spec.transport.endpoint, now, &status), status);
                    q.connection_state = "resolving";
                }
            }
            deadline = now + duration_ns;
        } else if (name == "cm_control") {
            const uint32_t connection_id = number(config, "connection_id", 0, 1, UINT32_MAX);
            if (!connection_id || !config["action"].isString()) { throw std::runtime_error("CM control requires a connection_id and action"); }
            if ((connection_id >> 24) == uint32_t(id + 1)) {
                const auto found = connection_qps.find(connection_id);
                RuntimeQp *qp = found == connection_qps.end() ? nullptr : &qps[found->second];
                if (!qp) { throw std::runtime_error("Unknown CM connection_id"); }
                CmConnectionInfo info;
                if (!cm->connection(connection_id, info) || info.state != CmState::REQUEST_RECEIVED) {
                    throw std::runtime_error("CM accept/reject requires a pending request");
                }
                const std::string action = config["action"].asString();
                const auto data = unhex(config.get("private_data", ""), action == "accept" ? 196 : 148);
                if (action == "accept") { accept(*qp, info, ByteSpan(data.data(), data.size()), monotonic_ns()); }
                else if (action == "reject") {
                    Status status; checked(cm->reject(connection_id, number(config, "reason", 28, 1, 65535),
                        ByteSpan(data.data(), data.size()), monotonic_ns(), &status), status);
                } else { throw std::runtime_error("Unsupported CM control action"); }
            }
        } else if (name == "disconnect" || name == "cancel") {
            if (!configured || !started) { throw std::runtime_error("Start a managed RoCEv2 session first"); }
            const uint64_t now = monotonic_ns(); Status status;
            for (auto &q : qps) {
                if (!q.spec.managed) { continue; }
                if (name == "cancel" && q.spec.active_connection && !q.connection_id && !q.terminal) {
                    checked(neighbors->cancel(uint64_t(q.spec.index) + 1, now, &status), status);
                    q.terminal = true; q.connection_state = "cancelled"; ++q.connection_failures;
                    continue;
                }
                if (!q.connection_id) { continue; }
                if (name == "disconnect" && q.connected) {
                    if (q.posted != q.completed + q.failed) { throw std::runtime_error("Drain work before CM disconnect"); }
                    checked(cm->disconnect(q.connection_id, now, &status), status);
                    q.connection_state = "dreq_sent"; q.connected = false;
                } else if (name == "cancel" && !q.connected) {
                    checked(cm->cancel(q.connection_id, now, &status), status);
                }
            }
        } else if (name == "stop") {
            if (configured) {
                if (running) {
                    bool outstanding = false;
                    for (const auto &q : qps) { outstanding |= q.posted != q.completed + q.failed; }
                    if (outstanding) { abort(ErrorCode::ABORTED, "Endpoint session stopped with outstanding work"); }
                    else { running = false; }
                }
                final_stats = stats(true);
                final_stats["configured"] = false;
                final_stats["running"] = false;
                final_stats["done"] = true;
                release();
            }
        } else if (name != "stats" && name != "verify" && name != "get_events") {
            throw std::runtime_error("Unknown native RoCEv2 worker command");
        }
        Json::Value result = stats(name == "verify");
        if (name == "get_events") {
            result["events"] = Json::arrayValue;
            for (const auto &event : cm_events) {
                Json::Value entry(Json::objectValue);
                entry["sequence"] = Json::UInt64(&event - cm_events.data() + 1);
                entry["type"] = cm_event_to_string(event.type); entry["connection_id"] = event.connection.connection_id;
                entry["operation_id"] = Json::UInt64(event.connection.operation_id);
                entry["listener_id"] = Json::UInt64(event.connection.listener_id);
                entry["service_id"] = Json::UInt64(event.connection.service_id); entry["port"] = event.connection.port;
                entry["remote_qpn"] = event.connection.remote.qpn; entry["reason"] = event.reason;
                const RuntimeQp *q = event_qp(event);
                entry["device_id"] = q ? q->spec.device_id : 0;
                entry["local_gid_index"] = q ? q->spec.local_gid_index : 0;
                entry["wire_private_data"] = hex_data(event.private_data.data(), event.private_data_length);
                const size_t offset = q && q->spec.ip_cma && event.type == CmEventType::CONNECT_REQUEST ? 36 : 0;
                entry["ip_cma_header"] = offset != 0;
                entry["private_data"] = hex_data(event.private_data.data() + offset,
                    event.private_data_length >= offset ? event.private_data_length - offset : 0);
                result["events"].append(entry);
            }
            // Snapshot the bounded history. Retrying a read-only RPC must not
            // consume an event before the client has received its response.
        }
        return result;
    }
};

TrexAstfRoceV2Worker::TrexAstfRoceV2Worker(uint8_t thread_id, CFlowGenListPerThread *core) :
    m_impl(new Impl(thread_id, core)) {}
TrexAstfRoceV2Worker::~TrexAstfRoceV2Worker() = default;
bool TrexAstfRoceV2Worker::configured() const { return m_impl->configured; }
bool TrexAstfRoceV2Worker::active() const {
    return m_impl->configured && m_impl->started && m_impl->participating;
}
bool TrexAstfRoceV2Worker::poll() {
    try { return m_impl->poll(); }
    catch (const std::exception &ex) {
        ++m_impl->worker_errors;
        m_impl->error = ex.what();
        m_impl->abort(ErrorCode::INTERNAL_ERROR, m_impl->error);
        return false;
    }
}
Json::Value TrexAstfRoceV2Worker::command(const std::string &name, const Json::Value &config) {
    return m_impl->command(name, config);
}

TrexAstfDpRoceV2::TrexAstfDpRoceV2(const std::string &command, const Json::Value &config,
                               std::shared_ptr<TrexAstfRoceV2Reply> reply) :
    m_command(command), m_config(config), m_reply(std::move(reply)) {}
TrexCpToDpMsgBase *TrexAstfDpRoceV2::clone() {
    return new TrexAstfDpRoceV2(m_command, m_config, m_reply);
}
bool TrexAstfDpRoceV2::handle(TrexDpCore *core) {
    static_cast<TrexAstfDpCore *>(core)->handle_rocev2_command(m_command, m_config, m_reply);
    return true;
}
