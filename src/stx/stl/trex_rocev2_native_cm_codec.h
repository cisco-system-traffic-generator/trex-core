/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#ifndef TREX_ROCEV2_NATIVE_CM_CODEC_H
#define TREX_ROCEV2_NATIVE_CM_CODEC_H

#include "trex_rocev2_native_codec.h"
#include "trex_rocev2_native_types.h"
#include <array>

namespace TrexRoceV2 { namespace Native {

static const size_t NATIVE_CM_MAD_SIZE = 256;
enum class CmMessageType : uint16_t {
    REQ = 0x0010, MRA = 0x0011, REJ = 0x0012, REP = 0x0013,
    RTU = 0x0014, DREQ = 0x0015, DREP = 0x0016
};

/* Values use host byte order. GIDs are the exact network-order 16 bytes.
 * RC and the primary path are the advertised profile; unsupported alternate
 * paths, ECE, XRC and SRQ are rejected rather than silently negotiated. */
struct CmParameters {
    CmParameters();
    uint32_t qpn;
    uint32_t psn;                   // wire Starting PSN: selects peer SQ / local RQ
    uint8_t responder_resources;
    uint8_t initiator_depth;
    uint8_t mtu;                    // IB MTU enumeration 1..5
    uint8_t retry_count;
    uint8_t rnr_retry_count;
    uint8_t ack_timeout;
    uint8_t response_timeout;       // REQ Remote CM Response Timeout, byte 43
    uint8_t local_response_timeout; // REQ Local CM Response Timeout, byte 47
    uint8_t max_cm_retries;
    uint16_t pkey;
    uint64_t ca_guid;
};

struct CmMessage {
    CmMessage();
    CmMessageType type;
    uint64_t transaction_id;
    uint32_t local_id;
    uint32_t remote_id;
    uint64_t service_id;
    CmParameters parameters;
    std::array<uint8_t,16> local_gid;
    std::array<uint8_t,16> remote_gid;
    uint8_t traffic_class;
    uint8_t hop_limit;
    uint32_t flow_label;
    uint8_t message_mraed;          // 0 REQ, 1 REP
    uint8_t service_timeout;
    uint16_t reject_reason;
    uint32_t remote_qpn;           // DREQ target
    std::array<uint8_t,72> reject_info;
    size_t reject_info_length;
    std::array<uint8_t,224> private_data;
    size_t private_data_length;    // decode returns fixed message field length
};

struct CmaIpHeader {
    CmaIpHeader();
    bool ipv6;
    uint16_t source_port;
    std::array<uint8_t,16> source_ip;
    std::array<uint8_t,16> destination_ip;
};

class NativeCmCodec {
public:
    static size_t private_data_limit(CmMessageType type);
    static bool encode(const CmMessage &message, uint8_t *output, size_t capacity,
                       Status *status = NULL);
    static bool decode(ByteSpan mad, CmMessage &message, Status *status = NULL);
    static bool encode_frame(const NativeEndpoint &path, const CmMessage &message,
                             uint32_t ud_psn, uint8_t *output, size_t capacity,
                             size_t &written, Status *status = NULL);
    static bool decode_frame(ByteSpan frame, NativeEndpoint &path, CmMessage &message,
                             Status *status = NULL);
    static bool encode_ip_private(const CmaIpHeader &header, ByteSpan application,
                                  CmMessage &message, Status *status = NULL);
    static bool decode_ip_private(const CmMessage &message, CmaIpHeader &header,
                                  ByteSpan &application, Status *status = NULL);
    static uint64_t ip_service_id(uint16_t destination_port, uint16_t port_space = 0x0106);
    static void ip_gid(bool ipv6, const uint8_t *ip, std::array<uint8_t,16> &gid);
};

} }
#endif
