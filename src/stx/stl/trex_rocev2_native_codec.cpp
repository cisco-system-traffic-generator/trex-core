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

#include "trex_rocev2_native_codec.h"

#include <limits>
#include <sstream>
#include <string.h>

/* Keep this translation unit independent of DPDK headers. The shared ICRC
 * implementation in trex_rocev2_icrc.cpp operates on a contiguous byte span,
 * which is exactly the contract of this codec. */
uint32_t trex_roce_v2_calc_icrc_bytes(const uint8_t *pkt,
                                      uint32_t pkt_len,
                                      uint16_t l2_len,
                                      uint16_t l3_len,
                                      uint16_t bth_offset);

namespace TrexRoceV2 {
namespace Native {

namespace {

static const uint16_t ETHERTYPE_IPV4 = 0x0800;
static const uint16_t ETHERTYPE_VLAN = 0x8100;
static const uint16_t ETHERTYPE_QINQ = 0x88a8;
static const uint16_t ETHERTYPE_IPV6 = 0x86dd;
static const uint8_t IPPROTO_UDP_VALUE = 17;
static const size_t ETHERNET_LEN = 14;
static const size_t VLAN_LEN = 4;
static const size_t IPV4_MIN_LEN = 20;
static const size_t IPV6_LEN = 40;
static const size_t UDP_LEN = 8;
static const size_t BTH_LEN = 12;
static const size_t RETH_LEN = 16;
static const size_t AETH_LEN = 4;
static const size_t IMMEDIATE_LEN = 4;
static const size_t DETH_LEN = 8;
static const size_t INVALIDATE_LEN = 4;
static const size_t ATOMIC_ETH_LEN = 28;
static const size_t ATOMIC_ACK_LEN = 8;
static const size_t CNP_RESERVED_LEN = 16;

struct OpcodeLayout {
    OpcodeLayout()
        : has_reth(false),
          has_aeth(false),
          has_immediate(false),
          has_deth(false),
          has_invalidate(false),
          has_atomic_eth(false),
          has_atomic_ack(false),
          has_cnp_reserved(false),
          payload_allowed(true),
          full_word_payload(false),
          extended_length(0) {
    }

    bool has_reth;
    bool has_aeth;
    bool has_immediate;
    bool has_deth;
    bool has_invalidate;
    bool has_atomic_eth;
    bool has_atomic_ack;
    bool has_cnp_reserved;
    bool payload_allowed;
    bool full_word_payload;
    size_t extended_length;
};

static bool fail(CodecError *error,
                 CodecErrorCode code,
                 size_t offset,
                 const std::string &message) {
    if (error != NULL) {
        error->code = code;
        error->offset = offset;
        error->message = message;
    }
    return false;
}

static void clear_error(CodecError *error) {
    if (error != NULL) {
        error->clear();
    }
}

static uint16_t read_be16(const uint8_t *data) {
    return static_cast<uint16_t>((static_cast<uint32_t>(data[0]) << 8) |
                                 static_cast<uint32_t>(data[1]));
}

static uint32_t read_be24(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 16) |
           (static_cast<uint32_t>(data[1]) << 8) |
           static_cast<uint32_t>(data[2]);
}

static uint32_t read_be32(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

static uint64_t read_be64(const uint8_t *data) {
    return (static_cast<uint64_t>(read_be32(data)) << 32) |
           static_cast<uint64_t>(read_be32(data + 4));
}

static uint32_t read_le32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static void write_be16(uint8_t *data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

static void write_be24(uint8_t *data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value >> 16);
    data[1] = static_cast<uint8_t>(value >> 8);
    data[2] = static_cast<uint8_t>(value);
}

static void write_be32(uint8_t *data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

static void write_be64(uint8_t *data, uint64_t value) {
    write_be32(data, static_cast<uint32_t>(value >> 32));
    write_be32(data + 4, static_cast<uint32_t>(value));
}

static void write_le32(uint8_t *data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8);
    data[2] = static_cast<uint8_t>(value >> 16);
    data[3] = static_cast<uint8_t>(value >> 24);
}

static uint32_t checksum_add(uint32_t sum, const uint8_t *data, size_t length) {
    while (length >= 2) {
        sum += (static_cast<uint32_t>(data[0]) << 8) |
               static_cast<uint32_t>(data[1]);
        data += 2;
        length -= 2;
    }
    if (length != 0) {
        sum += static_cast<uint32_t>(data[0]) << 8;
    }
    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffffU) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

enum HeaderMask {
    HDR_RETH = 1U << 0,
    HDR_AETH = 1U << 1,
    HDR_IMMEDIATE = 1U << 2,
    HDR_DETH = 1U << 3,
    HDR_INVALIDATE = 1U << 4,
    HDR_ATOMIC_ETH = 1U << 5,
    HDR_ATOMIC_ACK = 1U << 6,
    HDR_CNP_RESERVED = 1U << 7
};

struct OpcodeDefinition {
    uint8_t opcode;
    unsigned headers;
    bool payload_allowed;
    bool full_word_payload;
};

/* Explicit legality table, independently checked against the protocol wire
 * definitions in Linux v6.12 ib_pack.h and rxe_{opcode.c,hdr.h}. Keep header
 * selection explicit: an unknown operation must never become a Read/Write. */
static const OpcodeDefinition OPCODE_DEFINITIONS[] = {
    {IB_SEND_FIRST, 0, true, true},
    {IB_SEND_MIDDLE, 0, true, true},
    {IB_SEND_LAST, 0, true, false},
    {IB_SEND_LAST_WITH_IMMEDIATE, HDR_IMMEDIATE, true, false},
    {IB_SEND_ONLY, 0, true, false},
    {IB_SEND_ONLY_WITH_IMMEDIATE, HDR_IMMEDIATE, true, false},
    {IB_RDMA_WRITE_FIRST, HDR_RETH, true, false},
    {IB_RDMA_WRITE_MIDDLE, 0, true, false},
    {IB_RDMA_WRITE_LAST, 0, true, false},
    {IB_RDMA_WRITE_LAST_WITH_IMMEDIATE, HDR_IMMEDIATE, true, false},
    {IB_RDMA_WRITE_ONLY, HDR_RETH, true, false},
    {IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE, HDR_RETH | HDR_IMMEDIATE, true, false},
    {IB_RDMA_READ_REQUEST, HDR_RETH, false, false},
    {IB_RDMA_READ_RESPONSE_FIRST, HDR_AETH, true, false},
    {IB_RDMA_READ_RESPONSE_MIDDLE, 0, true, false},
    {IB_RDMA_READ_RESPONSE_LAST, HDR_AETH, true, false},
    {IB_RDMA_READ_RESPONSE_ONLY, HDR_AETH, true, false},
    {IB_ACKNOWLEDGE, HDR_AETH, false, false},
    {IB_ATOMIC_ACKNOWLEDGE, HDR_AETH | HDR_ATOMIC_ACK, false, false},
    {IB_COMPARE_SWAP, HDR_ATOMIC_ETH, false, false},
    {IB_FETCH_ADD, HDR_ATOMIC_ETH, false, false},
    {IB_SEND_LAST_WITH_INVALIDATE, HDR_INVALIDATE, true, false},
    {IB_SEND_ONLY_WITH_INVALIDATE, HDR_INVALIDATE, true, false},
    {IB_UD_SEND_ONLY, HDR_DETH, true, false},
    {IB_CNP, HDR_CNP_RESERVED, false, false}
};

static bool opcode_layout(uint8_t raw_opcode, OpcodeLayout &layout) {
    for (size_t i = 0; i < sizeof(OPCODE_DEFINITIONS) / sizeof(OPCODE_DEFINITIONS[0]); ++i) {
        const OpcodeDefinition &definition = OPCODE_DEFINITIONS[i];
        if (definition.opcode != raw_opcode) {
            continue;
        }
        layout = OpcodeLayout();
        layout.has_reth = (definition.headers & HDR_RETH) != 0;
        layout.has_aeth = (definition.headers & HDR_AETH) != 0;
        layout.has_immediate = (definition.headers & HDR_IMMEDIATE) != 0;
        layout.has_deth = (definition.headers & HDR_DETH) != 0;
        layout.has_invalidate = (definition.headers & HDR_INVALIDATE) != 0;
        layout.has_atomic_eth = (definition.headers & HDR_ATOMIC_ETH) != 0;
        layout.has_atomic_ack = (definition.headers & HDR_ATOMIC_ACK) != 0;
        layout.has_cnp_reserved = (definition.headers & HDR_CNP_RESERVED) != 0;
        layout.payload_allowed = definition.payload_allowed;
        layout.full_word_payload = definition.full_word_payload;
        layout.extended_length =
            (layout.has_reth ? RETH_LEN : 0) +
            (layout.has_aeth ? AETH_LEN : 0) +
            (layout.has_immediate ? IMMEDIATE_LEN : 0) +
            (layout.has_deth ? DETH_LEN : 0) +
            (layout.has_invalidate ? INVALIDATE_LEN : 0) +
            (layout.has_atomic_eth ? ATOMIC_ETH_LEN : 0) +
            (layout.has_atomic_ack ? ATOMIC_ACK_LEN : 0) +
            (layout.has_cnp_reserved ? CNP_RESERVED_LEN : 0);
        return true;
    }
    return false;
}

static bool is_ipv6_extension_header(uint8_t next_header) {
    switch (next_header) {
    case 0:   /* Hop-by-hop */
    case 43:  /* Routing */
    case 44:  /* Fragment */
    case 50:  /* ESP */
    case 51:  /* AH */
    case 60:  /* Destination options */
    case 135: /* Mobility */
        return true;
    default:
        return false;
    }
}

static bool validate_encode_spec(const PacketEncodeSpec &spec,
                                 OpcodeLayout &layout,
                                 uint8_t &pad_count,
                                 size_t &frame_size,
                                 CodecError *error) {
    if ((static_cast<unsigned>(spec.opcode) > 0xffU) ||
        !opcode_layout(static_cast<uint8_t>(spec.opcode), layout)) {
        std::ostringstream message;
        message << "unsupported BTH opcode 0x" << std::hex
                << static_cast<unsigned>(spec.opcode);
        return fail(error, CodecErrorCode::UNSUPPORTED_OPCODE, 0, message.str());
    }
    if (spec.endpoint.udp_dport != ROCEV2_UDP_PORT) {
        return fail(error,
                    CodecErrorCode::WRONG_UDP_PORT,
                    0,
                    "RoCEv2 UDP destination port must be 4791");
    }
    if ((spec.endpoint.udp_sport == 0) || (spec.endpoint.ttl == 0) ||
        (spec.endpoint.dscp > 63) || (spec.endpoint.ecn > 3) ||
        (spec.endpoint.pcp > 7) ||
        (spec.endpoint.has_vlan && (spec.endpoint.vlan > 4094))) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "endpoint contains a value outside its wire range");
    }
    if ((spec.dest_qpn > NATIVE_MAX_U24) || (spec.psn > NATIVE_MAX_U24)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "BTH destination QPN and PSN must fit in 24 bits");
    }
    if ((spec.payload.size != 0) && (spec.payload.data == NULL)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "non-empty payload has a null data pointer");
    }
    if (spec.payload.size > TREX_MAX_ROCEV2_FRAME_SIZE) {
        return fail(error,
                    CodecErrorCode::FRAME_TOO_LARGE,
                    0,
                    "payload exceeds the maximum TRex RoCEv2 frame size");
    }
    if (!layout.payload_allowed && (spec.payload.size != 0)) {
        return fail(error,
                    CodecErrorCode::INVALID_PAYLOAD,
                    0,
                    "this opcode does not permit an inline payload");
    }
    if (layout.full_word_payload &&
        ((spec.payload.size == 0) || ((spec.payload.size & 3U) != 0))) {
        return fail(error, CodecErrorCode::INVALID_PAYLOAD, 0,
                    "SEND FIRST/MIDDLE requires non-empty, whole-word payload");
    }
    if ((spec.has_reth != layout.has_reth) ||
        (spec.has_aeth != layout.has_aeth) ||
        (spec.has_immediate_data != layout.has_immediate) ||
        (spec.has_deth != layout.has_deth) ||
        (spec.has_invalidate != layout.has_invalidate) ||
        (spec.has_atomic_eth != layout.has_atomic_eth) ||
        (spec.has_atomic_ack != layout.has_atomic_ack)) {
        return fail(error,
                    CodecErrorCode::ILLEGAL_EXTENDED_HEADERS,
                    0,
                    "extended-header selection does not match the opcode");
    }
    if (spec.has_atomic_eth && ((spec.atomic_eth.virtual_address & 7U) != 0)) {
        return fail(error, CodecErrorCode::INVALID_ARGUMENT, 0,
                    "atomic remote address must be aligned to eight bytes");
    }
    if ((spec.opcode == IB_FETCH_ADD) && (spec.atomic_eth.compare != 0)) {
        return fail(error, CodecErrorCode::INVALID_RESERVED_FIELD, 0,
                    "FETCH_ADD compare field is reserved and must be zero");
    }
    if (spec.has_aeth && (spec.aeth.msn > NATIVE_MAX_U24)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "AETH MSN must fit in 24 bits");
    }
    if (spec.has_deth && (spec.deth.source_qpn > NATIVE_MAX_U24)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "DETH source QPN must fit in 24 bits");
    }

    pad_count = static_cast<uint8_t>((4 - (spec.payload.size & 3U)) & 3U);
    const size_t l2_length = spec.endpoint.has_vlan ? ETHERNET_LEN + VLAN_LEN : ETHERNET_LEN;
    const size_t l3_length = spec.endpoint.ipv6 ? IPV6_LEN : IPV4_MIN_LEN;
    const size_t fixed_length = l2_length + l3_length + UDP_LEN + BTH_LEN +
                                layout.extended_length + ROCEV2_ICRC_LEN;
    if (spec.payload.size > std::numeric_limits<size_t>::max() - fixed_length - pad_count) {
        return fail(error,
                    CodecErrorCode::FRAME_TOO_LARGE,
                    0,
                    "encoded frame length overflows size_t");
    }
    frame_size = fixed_length + spec.payload.size + pad_count;
    const size_t udp_length = frame_size - l2_length - l3_length;
    const size_t ip_length = frame_size - l2_length;
    if ((udp_length > 0xffffU) || (!spec.endpoint.ipv6 && (ip_length > 0xffffU)) ||
        (frame_size > TREX_MAX_ROCEV2_FRAME_SIZE)) {
        return fail(error,
                    CodecErrorCode::FRAME_TOO_LARGE,
                    0,
                    "encoded packet exceeds a protocol or TRex frame limit");
    }
    return true;
}

static bool compute_udp_checksum(ByteSpan frame,
                                 const DecodedPacket &packet,
                                 uint16_t &checksum,
                                 CodecError *error) {
    if ((frame.data == NULL) ||
        (packet.frame_length > frame.size) ||
        (packet.udp_header.length != UDP_LEN) ||
        (packet.udp_header.offset + UDP_LEN > packet.frame_length)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    packet.udp_header.offset,
                    "packet offsets do not describe a complete UDP datagram");
    }

    const size_t udp_length = packet.frame_length - packet.udp_header.offset;
    if (udp_length > 0xffffU) {
        return fail(error,
                    CodecErrorCode::INVALID_UDP_LENGTH,
                    packet.udp_header.offset + 4,
                    "UDP length exceeds 65535 bytes");
    }

    uint32_t sum = 0;
    if (packet.endpoint.ipv6) {
        sum = checksum_add(sum, packet.endpoint.src_ip, 16);
        sum = checksum_add(sum, packet.endpoint.dst_ip, 16);
        uint8_t pseudo_length[4];
        write_be32(pseudo_length, static_cast<uint32_t>(udp_length));
        sum = checksum_add(sum, pseudo_length, sizeof(pseudo_length));
        const uint8_t next_header[4] = {0, 0, 0, IPPROTO_UDP_VALUE};
        sum = checksum_add(sum, next_header, sizeof(next_header));
    } else {
        sum = checksum_add(sum, packet.endpoint.src_ip, 4);
        sum = checksum_add(sum, packet.endpoint.dst_ip, 4);
        const uint8_t protocol_and_length[4] = {
            0,
            IPPROTO_UDP_VALUE,
            static_cast<uint8_t>(udp_length >> 8),
            static_cast<uint8_t>(udp_length)
        };
        sum = checksum_add(sum, protocol_and_length, sizeof(protocol_and_length));
    }

    const size_t udp_offset = packet.udp_header.offset;
    sum = checksum_add(sum, frame.data + udp_offset, 6);
    const uint8_t zero_checksum[2] = {0, 0};
    sum = checksum_add(sum, zero_checksum, sizeof(zero_checksum));
    sum = checksum_add(sum,
                       frame.data + udp_offset + UDP_LEN,
                       udp_length - UDP_LEN);
    checksum = checksum_finish(sum);
    if (checksum == 0) {
        checksum = 0xffff;
    }
    return true;
}

} // namespace

ByteSpan::ByteSpan() : data(NULL), size(0) {
}

ByteSpan::ByteSpan(const uint8_t *data_value, size_t size_value)
    : data(data_value), size(size_value) {
}

WireSpan::WireSpan() : offset(0), length(0) {
}

WireSpan::WireSpan(size_t offset_value, size_t length_value)
    : offset(offset_value), length(length_value) {
}

size_t WireSpan::end() const {
    return offset + length;
}

bool WireSpan::empty() const {
    return length == 0;
}

CodecError::CodecError() : code(CodecErrorCode::OK), offset(0), message() {
}

void CodecError::clear() {
    code = CodecErrorCode::OK;
    offset = 0;
    message.clear();
}

bool CodecError::ok() const {
    return code == CodecErrorCode::OK;
}

const char *codec_error_code_to_string(CodecErrorCode code) {
    switch (code) {
    case CodecErrorCode::OK: return "ok";
    case CodecErrorCode::INVALID_ARGUMENT: return "invalid_argument";
    case CodecErrorCode::BUFFER_TOO_SMALL: return "buffer_too_small";
    case CodecErrorCode::FRAME_TOO_LARGE: return "frame_too_large";
    case CodecErrorCode::TRUNCATED: return "truncated";
    case CodecErrorCode::UNSUPPORTED_ETHERNET: return "unsupported_ethernet";
    case CodecErrorCode::UNSUPPORTED_VLAN: return "unsupported_vlan";
    case CodecErrorCode::INVALID_IPV4: return "invalid_ipv4";
    case CodecErrorCode::INVALID_IPV4_CHECKSUM: return "invalid_ipv4_checksum";
    case CodecErrorCode::IPV4_FRAGMENTED: return "ipv4_fragmented";
    case CodecErrorCode::INVALID_IPV6: return "invalid_ipv6";
    case CodecErrorCode::IPV6_EXTENSION_UNSUPPORTED: return "ipv6_extension_unsupported";
    case CodecErrorCode::NOT_UDP: return "not_udp";
    case CodecErrorCode::WRONG_UDP_PORT: return "wrong_udp_port";
    case CodecErrorCode::INVALID_UDP_LENGTH: return "invalid_udp_length";
    case CodecErrorCode::INVALID_UDP_CHECKSUM: return "invalid_udp_checksum";
    case CodecErrorCode::UNSUPPORTED_OPCODE: return "unsupported_opcode";
    case CodecErrorCode::INVALID_BTH: return "invalid_bth";
    case CodecErrorCode::ILLEGAL_EXTENDED_HEADERS: return "illegal_extended_headers";
    case CodecErrorCode::INVALID_RESERVED_FIELD: return "invalid_reserved_field";
    case CodecErrorCode::INVALID_PAYLOAD: return "invalid_payload";
    case CodecErrorCode::INVALID_PADDING: return "invalid_padding";
    case CodecErrorCode::BAD_ICRC: return "bad_icrc";
    }
    return "unknown";
}

NativeEndpoint::NativeEndpoint()
    : has_next_hop(false),
      ipv6(false),
      has_vlan(false),
      vlan(0),
      pcp(0),
      vlan_dei(false),
      dscp(0),
      ecn(0),
      ttl(64),
      udp_sport(49152),
      udp_dport(ROCEV2_UDP_PORT) {
    memset(src_mac, 0, sizeof(src_mac));
    memset(dst_mac, 0, sizeof(dst_mac));
    memset(src_ip, 0, sizeof(src_ip));
    memset(dst_ip, 0, sizeof(dst_ip));
}

NativeReth::NativeReth() : virtual_address(0), rkey(0), dma_length(0) {
}

NativeAeth::NativeAeth() : syndrome(0), msn(0) {
}

NativeDeth::NativeDeth() : qkey(NATIVE_GSI_QKEY), source_qpn(1) {
}

NativeAtomicEth::NativeAtomicEth()
    : virtual_address(0), rkey(0), swap_add(0), compare(0) {
}

DecodedPacket::DecodedPacket()
    : opcode(IB_RDMA_WRITE_ONLY),
      pkey(0),
      dest_qpn(0),
      psn(0),
      solicited(false),
      migration_request(false),
      ack_request(false),
      fecn(false),
      becn(false),
      pad_count(0),
      has_reth(false),
      has_aeth(false),
      has_immediate_data(false),
      immediate_data(0),
      has_deth(false),
      has_invalidate(false),
      invalidate_rkey(0),
      has_atomic_eth(false),
      has_atomic_ack(false),
      atomic_original_value(0),
      has_cnp_reserved(false),
      ipv4_identification(0),
      ipv4_dont_fragment(false),
      udp_checksum(0),
      icrc(0),
      frame_length(0) {
    memset(cnp_reserved, 0, sizeof(cnp_reserved));
}

DecodeOptions::DecodeOptions()
    : verify_icrc(true),
      verify_ipv4_checksum(true),
      verify_udp_checksum(true) {
}

PacketEncodeSpec::PacketEncodeSpec()
    : opcode(IB_RDMA_WRITE_ONLY),
      pkey(0xffff),
      dest_qpn(0),
      psn(0),
      solicited(false),
      migration_request(false),
      ack_request(false),
      fecn(false),
      becn(false),
      has_reth(true),
      has_aeth(false),
      has_immediate_data(false),
      immediate_data(0),
      has_deth(false),
      has_invalidate(false),
      invalidate_rkey(0),
      has_atomic_eth(false),
      has_atomic_ack(false),
      atomic_original_value(0),
      payload(),
      ipv4_identification(0),
      ipv4_dont_fragment(false) {
}

bool NativePacketCodec::decode(ByteSpan frame,
                               DecodedPacket &packet,
                               CodecError *error) {
    DecodeOptions options;
    return decode(frame, options, packet, error);
}

bool NativePacketCodec::decode(ByteSpan frame,
                               const DecodeOptions &options,
                               DecodedPacket &packet,
                               CodecError *error) {
    clear_error(error);
    packet = DecodedPacket();
    if ((frame.data == NULL) && (frame.size != 0)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "frame has a null data pointer");
    }
    if (frame.size < ETHERNET_LEN) {
        return fail(error,
                    CodecErrorCode::TRUNCATED,
                    frame.size,
                    "Ethernet header is truncated");
    }
    if (frame.size > TREX_MAX_ROCEV2_FRAME_SIZE) {
        return fail(error,
                    CodecErrorCode::FRAME_TOO_LARGE,
                    frame.size,
                    "frame exceeds the TRex RoCEv2 frame limit");
    }

    memcpy(packet.endpoint.dst_mac, frame.data, 6);
    memcpy(packet.endpoint.src_mac, frame.data + 6, 6);
    uint16_t ethertype = read_be16(frame.data + 12);
    size_t l2_length = ETHERNET_LEN;
    packet.vlan_header = WireSpan();
    if (ethertype == ETHERTYPE_VLAN) {
        if (frame.size < ETHERNET_LEN + VLAN_LEN) {
            return fail(error,
                        CodecErrorCode::TRUNCATED,
                        frame.size,
                        "802.1Q header is truncated");
        }
        const uint16_t tci = read_be16(frame.data + 14);
        packet.endpoint.has_vlan = true;
        packet.endpoint.pcp = static_cast<uint8_t>((tci >> 13) & 7);
        packet.endpoint.vlan_dei = (tci & 0x1000U) != 0;
        packet.endpoint.vlan = tci & 0x0fffU;
        packet.vlan_header = WireSpan(12, VLAN_LEN);
        ethertype = read_be16(frame.data + 16);
        l2_length += VLAN_LEN;
        if ((ethertype == ETHERTYPE_VLAN) || (ethertype == ETHERTYPE_QINQ)) {
            return fail(error,
                        CodecErrorCode::UNSUPPORTED_VLAN,
                        16,
                        "stacked VLAN headers are not supported");
        }
    } else if (ethertype == ETHERTYPE_QINQ) {
        return fail(error,
                    CodecErrorCode::UNSUPPORTED_VLAN,
                    12,
                    "802.1ad/QinQ is not supported by the native codec");
    }
    packet.ethernet = WireSpan(0, l2_length);

    size_t l3_length = 0;
    size_t ip_packet_length = 0;
    uint8_t ip_protocol = 0;
    if (ethertype == ETHERTYPE_IPV4) {
        packet.endpoint.ipv6 = false;
        if (frame.size < l2_length + IPV4_MIN_LEN) {
            return fail(error,
                        CodecErrorCode::TRUNCATED,
                        frame.size,
                        "IPv4 header is truncated");
        }
        const uint8_t version_ihl = frame.data[l2_length];
        if ((version_ihl >> 4) != 4) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV4,
                        l2_length,
                        "IPv4 version field is not 4");
        }
        l3_length = static_cast<size_t>(version_ihl & 0x0fU) * 4U;
        if ((l3_length < IPV4_MIN_LEN) || (frame.size < l2_length + l3_length)) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV4,
                        l2_length,
                        "IPv4 IHL is invalid or truncated");
        }
        ip_packet_length = read_be16(frame.data + l2_length + 2);
        if ((ip_packet_length < l3_length + UDP_LEN) ||
            (ip_packet_length != frame.size - l2_length)) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV4,
                        l2_length + 2,
                        "IPv4 total length does not match the frame");
        }
        const uint16_t fragment = read_be16(frame.data + l2_length + 6);
        if ((fragment & 0x8000U) || (fragment & 0x2000U) || (fragment & 0x1fffU)) {
            return fail(error,
                        CodecErrorCode::IPV4_FRAGMENTED,
                        l2_length + 6,
                        "fragmented IPv4 RoCEv2 packets are not supported");
        }
        packet.ipv4_dont_fragment = (fragment & 0x4000U) != 0;
        packet.ipv4_identification = read_be16(frame.data + l2_length + 4);
        ip_protocol = frame.data[l2_length + 9];
        packet.endpoint.ttl = frame.data[l2_length + 8];
        const uint8_t traffic = frame.data[l2_length + 1];
        packet.endpoint.dscp = static_cast<uint8_t>(traffic >> 2);
        packet.endpoint.ecn = traffic & 3;
        memcpy(packet.endpoint.src_ip, frame.data + l2_length + 12, 4);
        memcpy(packet.endpoint.dst_ip, frame.data + l2_length + 16, 4);
        if (options.verify_ipv4_checksum &&
            !verify_ipv4_checksum(ByteSpan(frame.data + l2_length, l3_length))) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV4_CHECKSUM,
                        l2_length + 10,
                        "IPv4 header checksum validation failed");
        }
    } else if (ethertype == ETHERTYPE_IPV6) {
        packet.endpoint.ipv6 = true;
        l3_length = IPV6_LEN;
        if (frame.size < l2_length + l3_length) {
            return fail(error,
                        CodecErrorCode::TRUNCATED,
                        frame.size,
                        "IPv6 base header is truncated");
        }
        const uint32_t first_word = read_be32(frame.data + l2_length);
        if ((first_word >> 28) != 6) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV6,
                        l2_length,
                        "IPv6 version field is not 6");
        }
        const uint16_t payload_length = read_be16(frame.data + l2_length + 4);
        ip_packet_length = l3_length + payload_length;
        if ((payload_length < UDP_LEN) ||
            (ip_packet_length != frame.size - l2_length)) {
            return fail(error,
                        CodecErrorCode::INVALID_IPV6,
                        l2_length + 4,
                        "IPv6 payload length does not match the frame");
        }
        ip_protocol = frame.data[l2_length + 6];
        if (is_ipv6_extension_header(ip_protocol)) {
            return fail(error,
                        CodecErrorCode::IPV6_EXTENSION_UNSUPPORTED,
                        l2_length + 6,
                        "IPv6 extension headers are not supported for RoCEv2");
        }
        packet.endpoint.ttl = frame.data[l2_length + 7];
        const uint8_t traffic = static_cast<uint8_t>((first_word >> 20) & 0xffU);
        packet.endpoint.dscp = static_cast<uint8_t>(traffic >> 2);
        packet.endpoint.ecn = traffic & 3;
        memcpy(packet.endpoint.src_ip, frame.data + l2_length + 8, 16);
        memcpy(packet.endpoint.dst_ip, frame.data + l2_length + 24, 16);
    } else {
        return fail(error,
                    CodecErrorCode::UNSUPPORTED_ETHERNET,
                    packet.endpoint.has_vlan ? 16 : 12,
                    "EtherType is not IPv4 or IPv6");
    }
    packet.ip_header = WireSpan(l2_length, l3_length);

    if (ip_protocol != IPPROTO_UDP_VALUE) {
        return fail(error,
                    CodecErrorCode::NOT_UDP,
                    l2_length + (packet.endpoint.ipv6 ? 6 : 9),
                    "RoCEv2 network protocol is not UDP");
    }
    const size_t udp_offset = l2_length + l3_length;
    if (frame.size < udp_offset + UDP_LEN) {
        return fail(error,
                    CodecErrorCode::TRUNCATED,
                    frame.size,
                    "UDP header is truncated");
    }
    const uint16_t udp_length = read_be16(frame.data + udp_offset + 4);
    const size_t ip_payload_length = ip_packet_length - l3_length;
    if ((udp_length < UDP_LEN + BTH_LEN + ROCEV2_ICRC_LEN) ||
        (udp_length != ip_payload_length) ||
        (udp_offset + udp_length != frame.size)) {
        return fail(error,
                    CodecErrorCode::INVALID_UDP_LENGTH,
                    udp_offset + 4,
                    "UDP length does not match the IP payload and frame");
    }
    packet.endpoint.udp_sport = read_be16(frame.data + udp_offset);
    packet.endpoint.udp_dport = read_be16(frame.data + udp_offset + 2);
    packet.udp_checksum = read_be16(frame.data + udp_offset + 6);
    if (packet.endpoint.udp_dport != ROCEV2_UDP_PORT) {
        return fail(error,
                    CodecErrorCode::WRONG_UDP_PORT,
                    udp_offset + 2,
                    "UDP destination port is not 4791");
    }
    packet.udp_header = WireSpan(udp_offset, UDP_LEN);

    const size_t bth_offset = udp_offset + UDP_LEN;
    const size_t roce_end = udp_offset + udp_length;
    const size_t icrc_offset = roce_end - ROCEV2_ICRC_LEN;
    if (bth_offset + BTH_LEN > icrc_offset) {
        return fail(error,
                    CodecErrorCode::TRUNCATED,
                    bth_offset,
                    "BTH or ICRC is truncated");
    }

    OpcodeLayout layout;
    const uint8_t raw_opcode = frame.data[bth_offset];
    if (!opcode_layout(raw_opcode, layout)) {
        std::ostringstream message;
        message << "unsupported BTH opcode 0x" << std::hex
                << static_cast<unsigned>(raw_opcode);
        return fail(error,
                    CodecErrorCode::UNSUPPORTED_OPCODE,
                    bth_offset,
                    message.str());
    }
    packet.opcode = static_cast<IbOpcode>(raw_opcode);
    const uint8_t bth_flags = frame.data[bth_offset + 1];
    if ((bth_flags & 0x0fU) != 0) {
        return fail(error,
                    CodecErrorCode::INVALID_BTH,
                    bth_offset + 1,
                    "BTH transport-header version/reserved bits must be zero");
    }
    const uint8_t congestion = frame.data[bth_offset + 4];
    if ((congestion & 0x3fU) != 0) {
        return fail(error,
                    CodecErrorCode::INVALID_RESERVED_FIELD,
                    bth_offset + 4,
                    "BTH reserved bits beside FECN/BECN must be zero");
    }
    const uint8_t ack_flags = frame.data[bth_offset + 8];
    if ((ack_flags & 0x7fU) != 0) {
        return fail(error,
                    CodecErrorCode::INVALID_RESERVED_FIELD,
                    bth_offset + 8,
                    "BTH reserved bits beside AckReq must be zero");
    }
    packet.solicited = (bth_flags & 0x80U) != 0;
    packet.migration_request = (bth_flags & 0x40U) != 0;
    packet.pad_count = static_cast<uint8_t>((static_cast<uint32_t>(bth_flags) >> 4) & 3U);
    packet.pkey = read_be16(frame.data + bth_offset + 2);
    packet.fecn = (congestion & 0x80U) != 0;
    packet.becn = (congestion & 0x40U) != 0;
    packet.dest_qpn = read_be24(frame.data + bth_offset + 5);
    packet.ack_request = (ack_flags & 0x80U) != 0;
    packet.psn = read_be24(frame.data + bth_offset + 9);
    packet.bth = WireSpan(bth_offset, BTH_LEN);

    size_t cursor = bth_offset + BTH_LEN;
    if (layout.extended_length > icrc_offset - cursor) {
        return fail(error,
                    CodecErrorCode::TRUNCATED,
                    cursor,
                    "opcode-specific extended header is truncated");
    }
    const size_t extended_offset = cursor;
    if (layout.has_reth) {
        packet.has_reth = true;
        packet.reth.virtual_address = read_be64(frame.data + cursor);
        packet.reth.rkey = read_be32(frame.data + cursor + 8);
        packet.reth.dma_length = read_be32(frame.data + cursor + 12);
        cursor += RETH_LEN;
    }
    if (layout.has_aeth) {
        packet.has_aeth = true;
        packet.aeth.syndrome = frame.data[cursor];
        packet.aeth.msn = read_be24(frame.data + cursor + 1);
        cursor += AETH_LEN;
    }
    if (layout.has_immediate) {
        packet.has_immediate_data = true;
        packet.immediate_data = read_be32(frame.data + cursor);
        cursor += IMMEDIATE_LEN;
    }
    if (layout.has_deth) {
        packet.has_deth = true;
        packet.deth.qkey = read_be32(frame.data + cursor);
        if (frame.data[cursor + 4] != 0) {
            return fail(error,
                        CodecErrorCode::INVALID_RESERVED_FIELD,
                        cursor + 4,
                        "DETH reserved byte must be zero");
        }
        packet.deth.source_qpn = read_be24(frame.data + cursor + 5);
        cursor += DETH_LEN;
    }
    if (layout.has_invalidate) {
        packet.has_invalidate = true;
        packet.invalidate_rkey = read_be32(frame.data + cursor);
        cursor += INVALIDATE_LEN;
    }
    if (layout.has_atomic_eth) {
        packet.has_atomic_eth = true;
        packet.atomic_eth.virtual_address = read_be64(frame.data + cursor);
        packet.atomic_eth.rkey = read_be32(frame.data + cursor + 8);
        packet.atomic_eth.swap_add = read_be64(frame.data + cursor + 12);
        packet.atomic_eth.compare = read_be64(frame.data + cursor + 20);
        /* Address alignment is a remote-access check at the responder, which
         * must send a NAK for an unaligned request. Preserve that address here
         * rather than turning a well-formed request into a silent wire drop. */
        if ((packet.opcode == IB_FETCH_ADD) && (packet.atomic_eth.compare != 0)) {
            return fail(error, CodecErrorCode::INVALID_RESERVED_FIELD, cursor + 20,
                        "FETCH_ADD compare field is reserved and must be zero");
        }
        cursor += ATOMIC_ETH_LEN;
    }
    if (layout.has_atomic_ack) {
        packet.has_atomic_ack = true;
        packet.atomic_original_value = read_be64(frame.data + cursor);
        cursor += ATOMIC_ACK_LEN;
    }
    if (layout.has_cnp_reserved) {
        packet.has_cnp_reserved = true;
        memcpy(packet.cnp_reserved, frame.data + cursor, CNP_RESERVED_LEN);
        for (size_t i = 0; i < CNP_RESERVED_LEN; ++i) {
            if (packet.cnp_reserved[i] != 0) {
                return fail(error,
                            CodecErrorCode::INVALID_RESERVED_FIELD,
                            cursor + i,
                            "CNP reserved bytes must be zero");
            }
        }
        cursor += CNP_RESERVED_LEN;
    }
    packet.extended_headers = WireSpan(extended_offset, cursor - extended_offset);

    if ((layout.has_atomic_eth || layout.has_atomic_ack || layout.full_word_payload) &&
        packet.pad_count != 0) {
        return fail(error, CodecErrorCode::INVALID_PADDING, bth_offset + 1,
                    "atomic and nonterminal SEND packets cannot contain padding");
    }
    if (packet.pad_count > icrc_offset - cursor) {
        return fail(error,
                    CodecErrorCode::INVALID_PADDING,
                    bth_offset + 1,
                    "BTH pad count exceeds the remaining RoCEv2 body");
    }
    const size_t payload_end = icrc_offset - packet.pad_count;
    packet.payload = WireSpan(cursor, payload_end - cursor);
    packet.padding = WireSpan(payload_end, packet.pad_count);
    for (size_t i = payload_end; i < icrc_offset; ++i) {
        if (frame.data[i] != 0) {
            return fail(error,
                        CodecErrorCode::INVALID_PADDING,
                        i,
                        "RoCEv2 padding bytes must be zero");
        }
    }
    if (!layout.payload_allowed && !packet.payload.empty()) {
        return fail(error,
                    CodecErrorCode::INVALID_PAYLOAD,
                    packet.payload.offset,
                    "opcode contains an unexpected inline payload");
    }
    if (layout.full_word_payload &&
        (packet.payload.empty() || ((packet.payload.length & 3U) != 0))) {
        return fail(error, CodecErrorCode::INVALID_PAYLOAD, packet.payload.offset,
                    "SEND FIRST/MIDDLE requires non-empty, whole-word payload");
    }
    packet.icrc_field = WireSpan(icrc_offset, ROCEV2_ICRC_LEN);
    packet.icrc = read_le32(frame.data + icrc_offset);
    packet.frame_length = roce_end;

    if (options.verify_udp_checksum && !verify_udp_checksum(frame, packet, error)) {
        return false;
    }
    if (options.verify_icrc && !verify_icrc(frame, packet, error)) {
        return false;
    }
    return true;
}

bool NativePacketCodec::encoded_size(const PacketEncodeSpec &spec,
                                     size_t &size,
                                     CodecError *error) {
    clear_error(error);
    size = 0;
    OpcodeLayout layout;
    uint8_t pad_count = 0;
    return validate_encode_spec(spec, layout, pad_count, size, error);
}

bool NativePacketCodec::encode(const PacketEncodeSpec &spec,
                               uint8_t *output,
                               size_t capacity,
                               size_t &written,
                               CodecError *error) {
    clear_error(error);
    written = 0;
    OpcodeLayout layout;
    uint8_t pad_count = 0;
    size_t frame_size = 0;
    if (!validate_encode_spec(spec, layout, pad_count, frame_size, error)) {
        return false;
    }
    if (output == NULL) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    0,
                    "output buffer is null");
    }
    if (capacity < frame_size) {
        return fail(error,
                    CodecErrorCode::BUFFER_TOO_SMALL,
                    capacity,
                    "output buffer is smaller than encoded_size");
    }

    memset(output, 0, frame_size);
    memcpy(output, spec.endpoint.dst_mac, 6);
    memcpy(output + 6, spec.endpoint.src_mac, 6);
    size_t cursor = 12;
    const uint16_t ip_ethertype = spec.endpoint.ipv6 ? ETHERTYPE_IPV6 : ETHERTYPE_IPV4;
    if (spec.endpoint.has_vlan) {
        write_be16(output + cursor, ETHERTYPE_VLAN);
        cursor += 2;
        const uint16_t tci = static_cast<uint16_t>(
            (static_cast<uint32_t>(spec.endpoint.pcp) << 13) |
            (spec.endpoint.vlan_dei ? 0x1000U : 0U) |
            spec.endpoint.vlan);
        write_be16(output + cursor, tci);
        cursor += 2;
    }
    write_be16(output + cursor, ip_ethertype);
    cursor += 2;
    const size_t l2_length = cursor;
    const size_t l3_length = spec.endpoint.ipv6 ? IPV6_LEN : IPV4_MIN_LEN;
    const size_t udp_offset = l2_length + l3_length;
    const size_t udp_length = frame_size - udp_offset;
    const uint8_t traffic = static_cast<uint8_t>((spec.endpoint.dscp << 2) |
                                                 spec.endpoint.ecn);

    if (spec.endpoint.ipv6) {
        const uint32_t first_word = (6U << 28) |
                                    (static_cast<uint32_t>(traffic) << 20);
        write_be32(output + l2_length, first_word);
        write_be16(output + l2_length + 4, static_cast<uint16_t>(udp_length));
        output[l2_length + 6] = IPPROTO_UDP_VALUE;
        output[l2_length + 7] = spec.endpoint.ttl;
        memcpy(output + l2_length + 8, spec.endpoint.src_ip, 16);
        memcpy(output + l2_length + 24, spec.endpoint.dst_ip, 16);
    } else {
        output[l2_length] = 0x45;
        output[l2_length + 1] = traffic;
        write_be16(output + l2_length + 2,
                   static_cast<uint16_t>(frame_size - l2_length));
        write_be16(output + l2_length + 4, spec.ipv4_identification);
        write_be16(output + l2_length + 6,
                   spec.ipv4_dont_fragment ? 0x4000U : 0U);
        output[l2_length + 8] = spec.endpoint.ttl;
        output[l2_length + 9] = IPPROTO_UDP_VALUE;
        memcpy(output + l2_length + 12, spec.endpoint.src_ip, 4);
        memcpy(output + l2_length + 16, spec.endpoint.dst_ip, 4);
        const uint16_t checksum = ipv4_checksum(ByteSpan(output + l2_length,
                                                         l3_length));
        write_be16(output + l2_length + 10, checksum);
    }

    write_be16(output + udp_offset, spec.endpoint.udp_sport);
    write_be16(output + udp_offset + 2, spec.endpoint.udp_dport);
    write_be16(output + udp_offset + 4, static_cast<uint16_t>(udp_length));

    const size_t bth_offset = udp_offset + UDP_LEN;
    output[bth_offset] = static_cast<uint8_t>(spec.opcode);
    output[bth_offset + 1] = static_cast<uint8_t>(
        (spec.solicited ? 0x80U : 0U) |
        (spec.migration_request ? 0x40U : 0U) |
        (static_cast<uint32_t>(pad_count) << 4));
    write_be16(output + bth_offset + 2, spec.pkey);
    output[bth_offset + 4] = static_cast<uint8_t>(
        (spec.fecn ? 0x80U : 0U) | (spec.becn ? 0x40U : 0U));
    write_be24(output + bth_offset + 5, spec.dest_qpn);
    output[bth_offset + 8] = spec.ack_request ? 0x80U : 0U;
    write_be24(output + bth_offset + 9, spec.psn);
    cursor = bth_offset + BTH_LEN;

    if (layout.has_reth) {
        write_be64(output + cursor, spec.reth.virtual_address);
        write_be32(output + cursor + 8, spec.reth.rkey);
        write_be32(output + cursor + 12, spec.reth.dma_length);
        cursor += RETH_LEN;
    }
    if (layout.has_aeth) {
        output[cursor] = spec.aeth.syndrome;
        write_be24(output + cursor + 1, spec.aeth.msn);
        cursor += AETH_LEN;
    }
    if (layout.has_immediate) {
        write_be32(output + cursor, spec.immediate_data);
        cursor += IMMEDIATE_LEN;
    }
    if (layout.has_deth) {
        write_be32(output + cursor, spec.deth.qkey);
        output[cursor + 4] = 0;
        write_be24(output + cursor + 5, spec.deth.source_qpn);
        cursor += DETH_LEN;
    }
    if (layout.has_invalidate) {
        write_be32(output + cursor, spec.invalidate_rkey);
        cursor += INVALIDATE_LEN;
    }
    if (layout.has_atomic_eth) {
        write_be64(output + cursor, spec.atomic_eth.virtual_address);
        write_be32(output + cursor + 8, spec.atomic_eth.rkey);
        write_be64(output + cursor + 12, spec.atomic_eth.swap_add);
        write_be64(output + cursor + 20, spec.atomic_eth.compare);
        cursor += ATOMIC_ETH_LEN;
    }
    if (layout.has_atomic_ack) {
        write_be64(output + cursor, spec.atomic_original_value);
        cursor += ATOMIC_ACK_LEN;
    }
    if (layout.has_cnp_reserved) {
        memset(output + cursor, 0, CNP_RESERVED_LEN);
        cursor += CNP_RESERVED_LEN;
    }
    if (spec.payload.size != 0) {
        memcpy(output + cursor, spec.payload.data, spec.payload.size);
        cursor += spec.payload.size;
    }
    memset(output + cursor, 0, pad_count);
    cursor += pad_count;
    const size_t icrc_offset = cursor;
    cursor += ROCEV2_ICRC_LEN;
    if (cursor != frame_size) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    cursor,
                    "internal encoded-size mismatch");
    }

    const uint32_t icrc = trex_roce_v2_calc_icrc_bytes(
        output,
        static_cast<uint32_t>(frame_size),
        static_cast<uint16_t>(l2_length),
        static_cast<uint16_t>(l3_length),
        static_cast<uint16_t>(bth_offset));
    write_le32(output + icrc_offset, icrc);

    DecodedPacket checksum_packet;
    checksum_packet.endpoint = spec.endpoint;
    checksum_packet.frame_length = frame_size;
    checksum_packet.udp_header = WireSpan(udp_offset, UDP_LEN);
    uint16_t udp_checksum_value = 0;
    if (!compute_udp_checksum(ByteSpan(output, frame_size),
                              checksum_packet,
                              udp_checksum_value,
                              error)) {
        return false;
    }
    write_be16(output + udp_offset + 6, udp_checksum_value);
    written = frame_size;
    return true;
}

bool NativePacketCodec::encode(const PacketEncodeSpec &spec,
                               std::vector<uint8_t> &output,
                               CodecError *error) {
    size_t size = 0;
    if (!encoded_size(spec, size, error)) {
        output.clear();
        return false;
    }
    output.resize(size);
    size_t written = 0;
    if (!encode(spec, output.data(), output.size(), written, error)) {
        output.clear();
        return false;
    }
    output.resize(written);
    return true;
}

uint16_t NativePacketCodec::ipv4_checksum(ByteSpan ipv4_header) {
    if ((ipv4_header.data == NULL) || (ipv4_header.size < IPV4_MIN_LEN) ||
        ((ipv4_header.size & 1U) != 0)) {
        return 0xffff;
    }
    return checksum_finish(checksum_add(0,
                                        ipv4_header.data,
                                        ipv4_header.size));
}

bool NativePacketCodec::verify_ipv4_checksum(ByteSpan ipv4_header) {
    return ipv4_checksum(ipv4_header) == 0;
}

bool NativePacketCodec::udp_checksum(ByteSpan frame,
                                     const DecodedPacket &packet,
                                     uint16_t &checksum,
                                     CodecError *error) {
    clear_error(error);
    checksum = 0;
    return compute_udp_checksum(frame, packet, checksum, error);
}

bool NativePacketCodec::verify_udp_checksum(ByteSpan frame,
                                            const DecodedPacket &packet,
                                            CodecError *error) {
    clear_error(error);
    if (!packet.endpoint.ipv6 && (packet.udp_checksum == 0)) {
        return true;
    }
    if (packet.endpoint.ipv6 && (packet.udp_checksum == 0)) {
        return fail(error,
                    CodecErrorCode::INVALID_UDP_CHECKSUM,
                    packet.udp_header.offset + 6,
                    "IPv6 UDP checksum must not be zero");
    }
    uint16_t expected = 0;
    if (!compute_udp_checksum(frame, packet, expected, error)) {
        return false;
    }
    if (expected != packet.udp_checksum) {
        return fail(error,
                    CodecErrorCode::INVALID_UDP_CHECKSUM,
                    packet.udp_header.offset + 6,
                    "UDP checksum validation failed");
    }
    return true;
}

bool NativePacketCodec::verify_icrc(ByteSpan frame,
                                    const DecodedPacket &packet,
                                    CodecError *error) {
    clear_error(error);
    if ((frame.data == NULL) || (packet.frame_length > frame.size) ||
        (packet.frame_length > std::numeric_limits<uint32_t>::max()) ||
        (packet.ethernet.length > std::numeric_limits<uint16_t>::max()) ||
        (packet.ip_header.length > std::numeric_limits<uint16_t>::max()) ||
        (packet.bth.offset > std::numeric_limits<uint16_t>::max()) ||
        (packet.icrc_field.length != ROCEV2_ICRC_LEN) ||
        (packet.icrc_field.end() != packet.frame_length)) {
        return fail(error,
                    CodecErrorCode::INVALID_ARGUMENT,
                    packet.icrc_field.offset,
                    "packet offsets do not describe a complete ICRC field");
    }
    const uint32_t expected = trex_roce_v2_calc_icrc_bytes(
        frame.data,
        static_cast<uint32_t>(packet.frame_length),
        static_cast<uint16_t>(packet.ethernet.length),
        static_cast<uint16_t>(packet.ip_header.length),
        static_cast<uint16_t>(packet.bth.offset));
    if (expected != packet.icrc) {
        return fail(error,
                    CodecErrorCode::BAD_ICRC,
                    packet.icrc_field.offset,
                    "RoCEv2 invariant CRC validation failed");
    }
    return true;
}

} // namespace Native
} // namespace TrexRoceV2
