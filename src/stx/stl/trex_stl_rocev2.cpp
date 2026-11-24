/* Copyright (c) 2026 Cisco Systems, Inc.
 * Licensed under the Apache License, Version 2.0. See LICENSE.
 * Packet encoding/checksum and segmentation logic adapted from the reference
 * RoCEv2 builder (59604573de024b85eed1901452dc3166e0d87297), without CCL.
 */
#include "trex_stl_rocev2.h"
#include "trex_stl_port.h"
#include <arpa/inet.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace TrexRoceV2 {
namespace {
using Bytes = std::vector<uint8_t>;

void require(bool condition, const std::string &message) {
    if (!condition) { throw std::invalid_argument("RoCEv2: " + message); }
}

uint64_t number(const Json::Value &c, const char *key, uint64_t fallback, uint64_t maximum) {
    if (!c.isMember(key)) { return fallback; }
    const auto &v = c[key];
    require(!v.isBool() && v.isIntegral() && v.isUInt64() && v.asUInt64() <= maximum,
            std::string(key) + " must be an unsigned integer in range 0.." + std::to_string(maximum));
    return v.asUInt64();
}

std::string text(const Json::Value &c, const char *key, const std::string &fallback) {
    if (!c.isMember(key)) { return fallback; }
    require(c[key].isString(), std::string(key) + " must be a string");
    return c[key].asString();
}

bool boolean(const Json::Value &c, const char *key, bool fallback = false) {
    if (!c.isMember(key)) { return fallback; }
    require(c[key].isBool(), std::string(key) + " must be a boolean");
    return c[key].asBool();
}

void append(Bytes &out, uint64_t value, unsigned size) {
    for (int i = size - 1; i >= 0; --i) { out.push_back((value >> (8 * i)) & 0xff); }
}

void store16(Bytes &out, unsigned offset, uint16_t value) {
    out[offset] = value >> 8; out[offset + 1] = value & 0xff;
}

uint32_t ones_sum(const uint8_t *data, size_t length) {
    uint32_t sum = 0;
    while (length > 1) {
        sum += (uint16_t(data[0]) << 8) | data[1]; data += 2; length -= 2;
    }
    if (length) { sum += uint16_t(data[0]) << 8; }
    return sum;
}

uint16_t checksum(uint32_t sum) {
    while (sum >> 16) { sum = (sum & 0xffff) + (sum >> 16); }
    return uint16_t(~sum);
}

unsigned hex_digit(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    throw std::invalid_argument("RoCEv2: invalid hexadecimal digit");
}

Bytes mac(const std::string &value) {
    Bytes out;
    std::istringstream stream(value);
    std::string octet;
    while (std::getline(stream, octet, ':')) {
        require(!octet.empty() && octet.size() <= 2, "MAC octets must have one or two hex digits");
        unsigned v = 0;
        for (char c : octet) { v = (v << 4) | hex_digit(c); }
        out.push_back(v);
    }
    require(out.size() == 6 && !value.empty() && value.back() != ':', "MAC must contain six octets");
    return out;
}

Bytes address(const Json::Value &c, const char *side, bool ipv6) {
    std::string key = std::string(side) + "_ip";
    std::string gid_key = std::string(side) + "_gid";
    std::string fallback = ipv6 ? (std::string(side) == "src" ? "2001:db8:16::1" : "2001:db8:48::1") :
                                  (std::string(side) == "src" ? "16.0.0.1" : "48.0.0.1");
    Bytes out(ipv6 ? 16 : 4);
    std::string value = text(c, key.c_str(), fallback);
    require(inet_pton(ipv6 ? AF_INET6 : AF_INET, value.c_str(), out.data()) == 1, key + " is invalid");
    if (c.isMember(gid_key)) {
        uint8_t gid[16];
        value = text(c, gid_key.c_str(), "");
        require(inet_pton(AF_INET6, value.c_str(), gid) == 1, gid_key + " is invalid");
        const uint8_t mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        require(ipv6 || memcmp(gid, mapped, 12) == 0, "IPv4 GIDs must be IPv4-mapped IPv6 addresses");
        Bytes resolved(gid + (ipv6 ? 0 : 12), gid + 16);
        require(!c.isMember(key) || resolved == out, gid_key + " disagrees with " + key);
        out = resolved;
    }
    return out;
}

Bytes increment(Bytes base, uint64_t offset) {
    for (int i = base.size() - 1; i >= 0 && offset; --i) {
        offset += base[i]; base[i] = offset & 0xff; offset >>= 8;
    }
    require(offset == 0, "IP increment overflows address range");
    return base;
}

struct Spec {
    uint8_t opcode, ext; // ext is its wire length: RETH=16, AETH=4.
    uint32_t payload, dma, psn;
    uint64_t offset;
    Spec(uint8_t op, uint8_t ex, uint32_t length, uint32_t dma_length = 0,
         uint32_t sequence = 0, uint64_t address_offset = 0) :
        opcode(op), ext(ex), payload(length), dma(dma_length), psn(sequence), offset(address_offset) {}
};

struct Config {
    bool ipv6, swap, ackreq, fecn, becn, flow_stats;
    Bytes src_mac, dst_mac, src_ip, dst_ip, pattern;
    uint32_t qps, qp_base, psn, psn_step, message_size, pgid_base, total_pkts;
    uint32_t src_step, dst_step, udp_sport, sport_step, rkey, msn;
    uint64_t va;
    uint16_t mtu, pkey, vlan;
    uint8_t pcp, dscp, ecn, ttl, syndrome;
    bool has_vlan;
    std::string operation, mode;
    double pps;
    unsigned l2() const { return has_vlan ? 18 : 14; }
    unsigned l3() const { return ipv6 ? 40 : 20; }
    uint32_t capacity(unsigned ext) const {
        unsigned header = l2() + l3() + 8 + 12 + ext + 4 + 4;
        require(mtu >= header, "MTU is too small for protocol headers");
        return ((mtu - header) / 4) * 4;
    }
};

Config parse(const Json::Value &c) {
    require(c.isObject(), "config must be an object");
    const std::set<std::string> keys = {
        "operation", "ip_version", "src_mac", "dst_mac", "src_ip", "dst_ip", "src_gid", "dst_gid",
        "src_ip_step", "dst_ip_step", "no_direction_swap", "direction", "vlan", "pcp", "dscp", "ecn", "ttl",
        "udp_sport", "udp_sport_step", "qp_base", "qps", "psn", "psn_step", "pkey", "ackreq", "fecn", "becn",
        "virtual_address", "rkey", "message_size", "mtu", "payload_pattern", "aeth_syndrome", "msn",
        "stream_mode", "pps", "total_pkts", "flow_stats", "pg_id_base"};
    for (const auto &key : c.getMemberNames()) { require(keys.count(key), "unknown config field: " + key); }
    Config cfg;
    unsigned version = number(c, "ip_version", 4, 6);
    require(version == 4 || version == 6, "ip_version must be 4 or 6"); cfg.ipv6 = version == 6;
    bool no_swap = boolean(c, "no_direction_swap");
    cfg.swap = number(c, "direction", 0, 1) && !no_swap;
    cfg.src_mac = mac(text(c, "src_mac", "00:00:00:00:01:00"));
    cfg.dst_mac = mac(text(c, "dst_mac", "00:00:00:00:02:00"));
    cfg.src_ip = address(c, "src", cfg.ipv6); cfg.dst_ip = address(c, "dst", cfg.ipv6);
    cfg.src_step = number(c, "src_ip_step", 0, UINT32_MAX); cfg.dst_step = number(c, "dst_ip_step", 0, UINT32_MAX);
    cfg.qps = number(c, "qps", 1, 0xffffff); require(cfg.qps, "qps must be positive");
    cfg.qp_base = number(c, "qp_base", 0x100, 0xffffff);
    require(uint64_t(cfg.qp_base) + cfg.qps - 1 <= 0xffffff, "QP range exceeds 24 bits");
    cfg.psn = number(c, "psn", 0, 0xffffff); cfg.psn_step = number(c, "psn_step", 1024, 0xffffff);
    cfg.message_size = number(c, "message_size", 4096, UINT32_MAX); require(cfg.message_size, "message_size must be positive");
    cfg.mtu = number(c, "mtu", 9000, 9216); require(cfg.mtu >= 64, "mtu must be in range 64..9216, including FCS");
    cfg.has_vlan = c.isMember("vlan"); cfg.vlan = number(c, "vlan", 0, 4094);
    cfg.pcp = number(c, "pcp", 3, 7); cfg.dscp = number(c, "dscp", 26, 63); cfg.ecn = number(c, "ecn", 1, 3);
    cfg.ttl = number(c, "ttl", 64, 255); cfg.pkey = number(c, "pkey", 0xffff, 0xffff);
    cfg.ackreq = boolean(c, "ackreq"); cfg.fecn = boolean(c, "fecn"); cfg.becn = boolean(c, "becn");
    cfg.udp_sport = number(c, "udp_sport", 49152, 65535); cfg.sport_step = number(c, "udp_sport_step", 1, 65535);
    require(uint64_t(cfg.udp_sport) + uint64_t(cfg.sport_step) * (cfg.qps - 1) <= 65535, "UDP source port range exceeds 16 bits");
    increment(cfg.src_ip, uint64_t(cfg.src_step) * (cfg.qps - 1));
    increment(cfg.dst_ip, uint64_t(cfg.dst_step) * (cfg.qps - 1));
    cfg.va = number(c, "virtual_address", 0x100000000ULL, UINT64_MAX);
    cfg.rkey = number(c, "rkey", 0x10101010, UINT32_MAX);
    cfg.syndrome = number(c, "aeth_syndrome", 0, 255); cfg.msn = number(c, "msn", 0, 0xffffff);
    cfg.operation = text(c, "operation", "write");
    require(cfg.operation == "write" || cfg.operation == "read-request" || cfg.operation == "read-response" || cfg.operation == "read", "invalid operation");
    cfg.mode = text(c, "stream_mode", "continuous");
    require(cfg.mode == "continuous" || cfg.mode == "burst" || cfg.mode == "chain", "invalid stream_mode");
    require(!c.isMember("pps") || (c["pps"].isNumeric() && !c["pps"].isBool()), "pps must be numeric");
    cfg.pps = c.get("pps", 1.0).asDouble(); require(std::isfinite(cfg.pps) && cfg.pps > 0, "pps must be finite and positive");
    cfg.total_pkts = number(c, "total_pkts", 1, UINT32_MAX); require(cfg.total_pkts, "total_pkts must be positive");
    cfg.flow_stats = boolean(c, "flow_stats", true); cfg.pgid_base = number(c, "pg_id_base", 0, UINT32_MAX);
    require(!cfg.flow_stats || (cfg.qps <= 4095 && uint64_t(cfg.pgid_base) + cfg.qps - 1 <= UINT32_MAX), "invalid flow-stat QP/PGID range");
    std::string pattern = text(c, "payload_pattern", "GPU");
    if (pattern.substr(0, 2) == "0x") {
        require(pattern.size() % 2 == 0, "hex payload must contain complete bytes");
        for (size_t i = 2; i < pattern.size(); i += 2) { cfg.pattern.push_back((hex_digit(pattern[i]) << 4) | hex_digit(pattern[i + 1])); }
    } else {
        for (unsigned char value : pattern) { require(value < 128, "payload_pattern must be ASCII or hex"); cfg.pattern.push_back(value); }
    }
    require(!cfg.pattern.empty(), "payload_pattern must not be empty");
    return cfg;
}

std::vector<Spec> specs(const Config &c, bool request, bool response) {
    if (request) { c.capacity(16); return {Spec(12, 16, 0, c.message_size)}; }
    uint8_t ext = response ? 4 : 16;
    uint8_t first = response ? 13 : 6, middle = response ? 14 : 7, last = response ? 15 : 8;
    uint32_t only_cap = c.capacity(ext);
    if (c.message_size <= only_cap) { return {Spec(response ? 16 : 10, ext, c.message_size, response ? 0 : c.message_size)}; }
    uint32_t first_cap = only_cap, middle_cap = c.capacity(0), last_cap = c.capacity(response ? 4 : 0);
    require(first_cap && middle_cap && last_cap, "MTU cannot fit segmented payload");
    std::vector<Spec> out{Spec(first, ext, first_cap, response ? 0 : c.message_size)};
    uint32_t remaining = c.message_size - first_cap, sequence = 1;
    uint64_t offset = first_cap;
    while (remaining > last_cap) {
        require(out.size() < TrexStatelessProfile::get_max_streams(), "generated stream count exceeds the port limit");
        uint32_t chunk = std::min(middle_cap, remaining);
        if (chunk == remaining) { chunk = remaining - last_cap; }
        out.emplace_back(middle, 0, chunk, 0, sequence++, offset);
        remaining -= chunk; offset += chunk;
    }
    out.emplace_back(last, response ? 4 : 0, remaining, 0, sequence, offset);
    return out;
}

Bytes packet(const Config &c, uint32_t qp, const Spec &s) {
    Bytes src = increment(c.src_ip, uint64_t(c.src_step) * qp), dst = increment(c.dst_ip, uint64_t(c.dst_step) * qp);
    const Bytes &smac = c.swap ? c.dst_mac : c.src_mac, &dmac = c.swap ? c.src_mac : c.dst_mac;
    if (c.swap) { src.swap(dst); }
    uint8_t pad = (-s.payload) & 3;
    uint16_t udp_len = 8 + 12 + s.ext + s.payload + pad + 4;
    Bytes out(dmac); out.insert(out.end(), smac.begin(), smac.end());
    if (c.has_vlan) { append(out, 0x8100, 2); append(out, (uint16_t(c.pcp) << 13) | c.vlan, 2); }
    append(out, c.ipv6 ? 0x86dd : 0x0800, 2);
    uint8_t tos = (c.dscp << 2) | c.ecn;
    if (c.ipv6) {
        append(out, 0x60000000 | (uint32_t(tos) << 20), 4); append(out, udp_len, 2); append(out, 17, 1); append(out, c.ttl, 1);
    } else {
        append(out, 0x45, 1); append(out, tos, 1); append(out, 20 + udp_len, 2);
        append(out, 1, 2); append(out, 0, 2); append(out, c.ttl, 1); append(out, 17, 1); append(out, 0, 2);
    }
    out.insert(out.end(), src.begin(), src.end()); out.insert(out.end(), dst.begin(), dst.end());
    append(out, c.udp_sport + c.sport_step * qp, 2); append(out, 4791, 2); append(out, udp_len, 2); append(out, 0, 2);
    append(out, s.opcode, 1); append(out, pad << 4, 1); append(out, c.pkey, 2);
    append(out, (c.fecn ? 0x80 : 0) | (c.becn ? 0x40 : 0), 1); append(out, c.qp_base + qp, 3);
    append(out, c.ackreq ? 0x80 : 0, 1); append(out, (uint64_t(c.psn) + uint64_t(c.psn_step) * qp + s.psn) & 0xffffff, 3);
    if (s.ext == 16) {
        require(s.offset <= UINT64_MAX - c.va, "virtual_address plus segment offset overflows");
        append(out, c.va + s.offset, 8); append(out, c.rkey, 4); append(out, s.dma, 4);
    } else if (s.ext == 4) { append(out, c.syndrome, 1); append(out, (c.msn + qp) & 0xffffff, 3); }
    for (uint32_t i = 0; i < s.payload; ++i) { out.push_back(c.pattern[i % c.pattern.size()]); }
    for (unsigned i = 0; i < pad + 4U; ++i) { out.push_back(0); }
    uint32_t crc = trex_roce_v2_calc_icrc_bytes(out.data(), out.size(), c.l2(), c.l3(), c.l2() + c.l3() + 8);
    for (unsigned i = 0; i < 4; ++i) { out[out.size() - 4 + i] = (crc >> (8 * i)) & 0xff; }
    unsigned udp = c.l2() + c.l3();
    uint32_t sum = ones_sum(src.data(), src.size()) + ones_sum(dst.data(), dst.size()) + 17 + udp_len;
    uint16_t udp_checksum = checksum(sum + ones_sum(out.data() + udp, udp_len));
    store16(out, udp + 6, udp_checksum ? udp_checksum : 0xffff);
    if (!c.ipv6) { store16(out, c.l2() + 10, checksum(ones_sum(out.data() + c.l2(), 20))); }
    require(out.size() + 4 <= c.mtu, "packet exceeds MTU");
    return out;
}
} // namespace

Json::Value build_streams(const Json::Value &config, uint32_t first_stream_id) {
    Config c = parse(config);
    std::vector<std::pair<std::string, std::vector<Spec>>> operations;
    if (c.operation == "write") { operations.emplace_back("write", specs(c, false, false)); }
    if (c.operation == "read" || c.operation == "read-request") { operations.emplace_back("read_req", specs(c, true, false)); }
    if (c.operation == "read" || c.operation == "read-response") { operations.emplace_back("read_rsp", specs(c, false, true)); }
    uint64_t count = 0;
    for (const auto &op : operations) { count += uint64_t(op.second.size()) * c.qps; }
    require(count && count <= TrexStatelessProfile::get_max_streams(), "generated stream count exceeds the port limit");
    require(uint64_t(first_stream_id) + count - 1 <= INT32_MAX, "stream IDs exceed the chain ID range");
    Json::Value result(Json::objectValue);
    result["streams"] = Json::arrayValue; result["stream_ids"] = Json::arrayValue;
    result["pgids"] = Json::arrayValue; result["expected_stats"] = Json::objectValue;
    if (c.flow_stats) { for (uint32_t q = 0; q < c.qps; ++q) { result["pgids"].append(c.pgid_base + q); } }
    uint32_t index = 0;
    for (const auto &op : operations) {
        for (uint32_t qp = 0; qp < c.qps; ++qp) {
            for (size_t seg = 0; seg < op.second.size(); ++seg, ++index) {
                Bytes bytes = packet(c, qp, op.second[seg]);
                Json::Value stream;
                std::ostringstream name; name << "rocev2_" << op.first << "_qp" << std::hex << c.qp_base + qp << "_seg" << std::dec << seg;
                stream["name"] = name.str(); stream["enabled"] = true; stream["self_start"] = c.mode != "chain" || index == 0;
                stream["start_paused"] = false; stream["isg"] = 0.0; stream["flags"] = 3; stream["action_count"] = 0;
                stream["stream_id"] = first_stream_id + index;
                stream["next_stream_id"] = c.mode == "chain" && index + 1 < count ? int(first_stream_id + index + 1) : -1;
                stream["mode"]["type"] = c.mode == "continuous" ? "continuous" : "single_burst";
                if (c.mode != "continuous") { stream["mode"]["total_pkts"] = c.total_pkts; }
                stream["mode"]["rate"]["type"] = "pps"; stream["mode"]["rate"]["value"] = c.pps;
                stream["packet"]["binary"] = base64_encode(bytes.data(), bytes.size()); stream["packet"]["meta"] = name.str();
                Json::Value fix;
                fix["type"] = "fix_checksum_hw"; fix["l2_len"] = c.l2(); fix["l3_len"] = c.l3(); fix["l4_type"] = 11;
                stream["vm"]["instructions"].append(fix);
                fix.removeMember("l4_type"); fix["type"] = "fix_checksum_rocev2_icrc"; fix["bth_offset"] = c.l2() + c.l3() + 8;
                stream["vm"]["instructions"].append(fix); stream["vm"]["cache"] = 0;
                stream["flow_stats"]["enabled"] = c.flow_stats;
                if (c.flow_stats) {
                    stream["flow_stats"]["stream_id"] = c.pgid_base + qp; stream["flow_stats"]["rule_type"] = "stats";
                    stream["flow_stats"]["vxlan"] = false; stream["flow_stats"]["ieee_1588"] = false;
                    if (c.mode != "continuous") {
                        auto &expected = result["expected_stats"][std::to_string(c.pgid_base + qp)];
                        expected["tx_pkts"] = Json::UInt64(expected.get("tx_pkts", Json::UInt64(0)).asUInt64() + c.total_pkts);
                        expected["tx_bytes"] = Json::UInt64(expected.get("tx_bytes", Json::UInt64(0)).asUInt64() + uint64_t(c.total_pkts) * (bytes.size() + 4));
                    }
                }
                result["streams"].append(stream); result["stream_ids"].append(first_stream_id + index);
            }
        }
    }
    if (c.mode == "chain") {
        for (Json::ArrayIndex i = 0; i + 1 < result["streams"].size(); ++i) {
            result["streams"][i]["next"] = result["streams"][i + 1]["name"];
        }
    }
    return result;
}
} // namespace TrexRoceV2
