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

#ifndef __TREX_ROCEV2_NATIVE_CODEC_H__
#define __TREX_ROCEV2_NATIVE_CODEC_H__

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace TrexRoceV2 {

/* Wire constants shared by the endpoint codec and transport.  Keep these
 * independent of workload builders and orchestration. */
static const uint16_t ROCEV2_UDP_PORT = 4791;
static const uint8_t ROCEV2_ICRC_LEN = 4;
static const uint8_t ETH_FCS_LEN = 4;
static const uint16_t TREX_MAX_ROCEV2_FRAME_SIZE = 9216;

enum IbOpcode {
    IB_SEND_FIRST = 0x00,
    IB_SEND_MIDDLE = 0x01,
    IB_SEND_LAST = 0x02,
    IB_SEND_LAST_WITH_IMMEDIATE = 0x03,
    IB_SEND_ONLY = 0x04,
    IB_SEND_ONLY_WITH_IMMEDIATE = 0x05,
    IB_RDMA_WRITE_FIRST = 0x06,
    IB_RDMA_WRITE_MIDDLE = 0x07,
    IB_RDMA_WRITE_LAST = 0x08,
    IB_RDMA_WRITE_LAST_WITH_IMMEDIATE = 0x09,
    IB_RDMA_WRITE_ONLY = 0x0a,
    IB_RDMA_WRITE_ONLY_WITH_IMMEDIATE = 0x0b,
    IB_RDMA_READ_REQUEST = 0x0c,
    IB_RDMA_READ_RESPONSE_FIRST = 0x0d,
    IB_RDMA_READ_RESPONSE_MIDDLE = 0x0e,
    IB_RDMA_READ_RESPONSE_LAST = 0x0f,
    IB_RDMA_READ_RESPONSE_ONLY = 0x10,
    IB_ACKNOWLEDGE = 0x11,
    IB_ATOMIC_ACKNOWLEDGE = 0x12,
    IB_COMPARE_SWAP = 0x13,
    IB_FETCH_ADD = 0x14,
    IB_SEND_LAST_WITH_INVALIDATE = 0x16,
    IB_SEND_ONLY_WITH_INVALIDATE = 0x17,
    IB_UD_SEND_ONLY = 0x64,
    IB_CNP = 0x81
};

namespace Native {

static const uint32_t NATIVE_MAX_U24 = 0x00ffffffU;
static const uint32_t NATIVE_GSI_QKEY = 0x80010000U;

struct ByteSpan {
    ByteSpan();
    ByteSpan(const uint8_t *data_value, size_t size_value);

    const uint8_t *data;
    size_t size;
};

struct WireSpan {
    WireSpan();
    WireSpan(size_t offset_value, size_t length_value);

    size_t end() const;
    bool empty() const;

    size_t offset;
    size_t length;
};

enum class CodecErrorCode {
    OK = 0,
    INVALID_ARGUMENT,
    BUFFER_TOO_SMALL,
    FRAME_TOO_LARGE,
    TRUNCATED,
    UNSUPPORTED_ETHERNET,
    UNSUPPORTED_VLAN,
    INVALID_IPV4,
    INVALID_IPV4_CHECKSUM,
    IPV4_FRAGMENTED,
    INVALID_IPV6,
    IPV6_EXTENSION_UNSUPPORTED,
    NOT_UDP,
    WRONG_UDP_PORT,
    INVALID_UDP_LENGTH,
    INVALID_UDP_CHECKSUM,
    UNSUPPORTED_OPCODE,
    INVALID_BTH,
    ILLEGAL_EXTENDED_HEADERS,
    INVALID_RESERVED_FIELD,
    INVALID_PAYLOAD,
    INVALID_PADDING,
    BAD_ICRC
};

struct CodecError {
    CodecError();

    void clear();
    bool ok() const;

    CodecErrorCode code;
    size_t offset;
    std::string message;
};

const char *codec_error_code_to_string(CodecErrorCode code);

struct NativeEndpoint {
    NativeEndpoint();

    uint8_t src_mac[6];
    uint8_t dst_mac[6];
    /* Configuration-only marker: dst_mac is an egress next hop rather than
     * the peer endpoint MAC.  It is not encoded as a separate wire field. */
    bool has_next_hop;
    bool ipv6;
    uint8_t src_ip[16];
    uint8_t dst_ip[16];
    bool has_vlan;
    uint16_t vlan;
    uint8_t pcp;
    bool vlan_dei;
    uint8_t dscp;
    uint8_t ecn;
    uint8_t ttl;
    uint16_t udp_sport;
    uint16_t udp_dport;
};

struct NativeReth {
    NativeReth();

    uint64_t virtual_address;
    uint32_t rkey;
    uint32_t dma_length;
};

struct NativeAeth {
    NativeAeth();

    uint8_t syndrome;
    uint32_t msn;
};

struct NativeDeth {
    NativeDeth();

    uint32_t qkey;
    uint32_t source_qpn;
};

/* Host-order fields. The wire AtomicETH is exactly 28 bytes, without the
 * alignment padding a C++ structure may contain between rkey and swap_add. */
struct NativeAtomicEth {
    NativeAtomicEth();

    uint64_t virtual_address;
    uint32_t rkey;
    uint64_t swap_add;
    uint64_t compare;
};

struct DecodedPacket {
    DecodedPacket();

    NativeEndpoint endpoint;
    IbOpcode opcode;
    uint16_t pkey;
    uint32_t dest_qpn;
    uint32_t psn;
    bool solicited;
    bool migration_request;
    bool ack_request;
    bool fecn;
    bool becn;
    uint8_t pad_count;

    bool has_reth;
    NativeReth reth;
    bool has_aeth;
    NativeAeth aeth;
    bool has_immediate_data;
    uint32_t immediate_data;
    bool has_deth;
    NativeDeth deth;
    bool has_invalidate;
    uint32_t invalidate_rkey;
    bool has_atomic_eth;
    NativeAtomicEth atomic_eth;
    bool has_atomic_ack;
    uint64_t atomic_original_value;
    bool has_cnp_reserved;
    uint8_t cnp_reserved[16];

    uint16_t ipv4_identification;
    bool ipv4_dont_fragment;
    uint16_t udp_checksum;
    uint32_t icrc;

    size_t frame_length;
    WireSpan ethernet;
    WireSpan vlan_header;
    WireSpan ip_header;
    WireSpan udp_header;
    WireSpan bth;
    WireSpan extended_headers;
    WireSpan payload;
    WireSpan padding;
    WireSpan icrc_field;
};

struct DecodeOptions {
    DecodeOptions();

    bool verify_icrc;
    bool verify_ipv4_checksum;
    bool verify_udp_checksum;
};

struct PacketEncodeSpec {
    PacketEncodeSpec();

    NativeEndpoint endpoint;
    IbOpcode opcode;
    uint16_t pkey;
    uint32_t dest_qpn;
    uint32_t psn;
    bool solicited;
    bool migration_request;
    bool ack_request;
    bool fecn;
    bool becn;

    bool has_reth;
    NativeReth reth;
    bool has_aeth;
    NativeAeth aeth;
    bool has_immediate_data;
    uint32_t immediate_data;
    bool has_deth;
    NativeDeth deth;
    bool has_invalidate;
    uint32_t invalidate_rkey;
    bool has_atomic_eth;
    NativeAtomicEth atomic_eth;
    bool has_atomic_ack;
    uint64_t atomic_original_value;

    ByteSpan payload;
    uint16_t ipv4_identification;
    bool ipv4_dont_fragment;
};

class NativePacketCodec {
public:
    static bool decode(ByteSpan frame,
                       DecodedPacket &packet,
                       CodecError *error = NULL);

    static bool decode(ByteSpan frame,
                       const DecodeOptions &options,
                       DecodedPacket &packet,
                       CodecError *error = NULL);

    static bool encoded_size(const PacketEncodeSpec &spec,
                             size_t &size,
                             CodecError *error = NULL);

    static bool encode(const PacketEncodeSpec &spec,
                       uint8_t *output,
                       size_t capacity,
                       size_t &written,
                       CodecError *error = NULL);

    /* Convenience for control-plane/tests. Hot paths should use the bounded
     * caller-owned buffer overload above. */
    static bool encode(const PacketEncodeSpec &spec,
                       std::vector<uint8_t> &output,
                       CodecError *error = NULL);

    static uint16_t ipv4_checksum(ByteSpan ipv4_header);
    static bool verify_ipv4_checksum(ByteSpan ipv4_header);

    /* Return the checksum value to place on the wire. A computed zero is
     * represented as 0xffff per UDP. */
    static bool udp_checksum(ByteSpan frame,
                             const DecodedPacket &packet,
                             uint16_t &checksum,
                             CodecError *error = NULL);

    static bool verify_udp_checksum(ByteSpan frame,
                                    const DecodedPacket &packet,
                                    CodecError *error = NULL);

    static bool verify_icrc(ByteSpan frame,
                            const DecodedPacket &packet,
                            CodecError *error = NULL);
};

} // namespace Native
} // namespace TrexRoceV2

#endif /* __TREX_ROCEV2_NATIVE_CODEC_H__ */
