/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0
 * Wire field locations independently implemented from the public IB CM MAD
 * layout; cross-checked against Linux v6.12 ibta_vol1_c12.h and cma.c.
 * No Linux or rdma-core implementation code is included. */
#include "trex_rocev2_native_cm_codec.h"
#include <algorithm>
#include <cstring>

namespace TrexRoceV2 { namespace Native {
namespace {
uint16_t get16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint32_t get24(const uint8_t *p) { return (uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|p[2]; }
uint32_t get32(const uint8_t *p) { return (uint32_t(get16(p))<<16)|get16(p+2); }
uint64_t get64(const uint8_t *p) { return (uint64_t(get32(p))<<32)|get32(p+4); }
void put16(uint8_t *p,uint16_t v) { p[0]=v>>8; p[1]=v; }
void put24(uint8_t *p,uint32_t v) { p[0]=v>>16; p[1]=v>>8; p[2]=v; }
void put32(uint8_t *p,uint32_t v) { put16(p,v>>16); put16(p+2,v); }
void put64(uint8_t *p,uint64_t v) { put32(p,v>>32); put32(p+4,v); }
size_t private_offset(CmMessageType t) {
    switch(t) {
    case CmMessageType::REQ:return 140; case CmMessageType::REP:return 36;
    case CmMessageType::MRA:return 10; case CmMessageType::REJ:return 84;
    case CmMessageType::DREQ:return 12; default:return 8;
    }
}
bool valid_parameters(const CmParameters &p, Status *s) {
    if(p.qpn<2 || p.qpn>NATIVE_MAX_U24 || p.psn>NATIVE_MAX_U24 || p.mtu<1 || p.mtu>5 ||
       p.retry_count>7 || p.rnr_retry_count>7 || p.ack_timeout>31 ||
       p.response_timeout>31 || p.local_response_timeout>31 || p.max_cm_retries>15 || p.pkey!=0xffff)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid RC CM parameters");
    return true;
}
bool all_zero(const uint8_t *p,size_t n) {
    for(size_t i=0;i<n;++i) if(p[i]) return false;
    return true;
}
}
CmParameters::CmParameters(): qpn(0),psn(0),responder_resources(16),initiator_depth(16),
    mtu(3),retry_count(7),rnr_retry_count(7),ack_timeout(14),response_timeout(20),local_response_timeout(20),
    max_cm_retries(7),pkey(0xffff),ca_guid(0) {}
CmMessage::CmMessage():type(CmMessageType::REQ),transaction_id(0),local_id(0),remote_id(0),
    service_id(0),traffic_class(104),hop_limit(64),flow_label(0),message_mraed(0),
    service_timeout(20),reject_reason(0),remote_qpn(0),reject_info_length(0),private_data_length(0) {
    local_gid.fill(0);remote_gid.fill(0);reject_info.fill(0);private_data.fill(0);
}
CmaIpHeader::CmaIpHeader():ipv6(false),source_port(0) { source_ip.fill(0);destination_ip.fill(0); }
size_t NativeCmCodec::private_data_limit(CmMessageType t) {
    switch(t) {
    case CmMessageType::REQ:return 92;case CmMessageType::REP:return 196;
    case CmMessageType::MRA:return 222;case CmMessageType::REJ:return 148;
    case CmMessageType::RTU:return 224;case CmMessageType::DREQ:return 220;
    case CmMessageType::DREP:return 224;
    }
    return 0;
}
bool NativeCmCodec::encode(const CmMessage &m,uint8_t *out,size_t cap,Status *s) {
    const size_t limit=private_data_limit(m.type);
    if(!out || cap<NATIVE_CM_MAD_SIZE || !limit || !m.transaction_id || (!m.local_id&&m.type!=CmMessageType::REJ) ||
       m.private_data_length>limit || m.reject_info_length>72 ||
       (m.type==CmMessageType::MRA&&m.message_mraed>1) || (m.type==CmMessageType::REJ&&m.message_mraed>2) ||
       m.service_timeout>31 || m.flow_label>0xfffff)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid CM MAD envelope or length");
    if((m.type==CmMessageType::REQ || m.type==CmMessageType::REP) && !valid_parameters(m.parameters,s)) return false;
    if(m.type!=CmMessageType::REQ && !m.remote_id)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"CM response requires remote communication ID");
    std::memset(out,0,NATIVE_CM_MAD_SIZE);
    out[0]=1;out[1]=7;out[2]=2;out[3]=3;
    put64(out+8,m.transaction_id);put16(out+16,static_cast<uint16_t>(m.type));
    uint8_t *b=out+24;
    put32(b,m.local_id);
    if(m.type!=CmMessageType::REQ)put32(b+4,m.remote_id);
    const CmParameters &p=m.parameters;
    switch(m.type) {
    case CmMessageType::REQ:
        put64(b+8,m.service_id);put64(b+16,p.ca_guid);
        put24(b+32,p.qpn);b[35]=p.responder_resources;b[39]=p.initiator_depth;
        b[43]=p.response_timeout<<3;put24(b+44,p.psn);
        b[47]=(p.local_response_timeout<<3)|p.retry_count;put16(b+48,p.pkey);
        b[50]=(p.mtu<<4)|p.rnr_retry_count;b[51]=p.max_cm_retries<<4;
        std::memcpy(b+56,m.local_gid.data(),16);std::memcpy(b+72,m.remote_gid.data(),16);
        b[88]=m.flow_label>>12;b[89]=m.flow_label>>4;b[90]=(m.flow_label&15)<<4;
        b[92]=m.traffic_class;b[93]=m.hop_limit;b[95]=p.ack_timeout<<3;
        break;
    case CmMessageType::REP:
        put24(b+12,p.qpn);put24(b+20,p.psn);b[24]=p.responder_resources;
        b[25]=p.initiator_depth;b[26]=p.ack_timeout<<3;b[27]=p.rnr_retry_count<<5;
        put64(b+28,p.ca_guid);break;
    case CmMessageType::MRA:b[8]=m.message_mraed<<6;b[9]=m.service_timeout<<3;break;
    case CmMessageType::REJ:
        b[8]=m.message_mraed<<6;b[9]=m.reject_info_length<<1;
        put16(b+10,m.reject_reason);std::memcpy(b+12,m.reject_info.data(),m.reject_info_length);break;
    case CmMessageType::DREQ:put24(b+8,m.remote_qpn);break;
    default:break;
    }
    std::memcpy(b+private_offset(m.type),m.private_data.data(),m.private_data_length);
    status_ok(s);return true;
}
bool NativeCmCodec::decode(ByteSpan mad,CmMessage &m,Status *s) {
    if(!mad.data || mad.size!=NATIVE_CM_MAD_SIZE)
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM MAD must contain exactly 256 bytes");
    const uint8_t *h=mad.data,*b=h+24;
    if(h[0]!=1 || h[1]!=7 || h[2]!=2 || h[3]!=3 || get16(h+4) || get16(h+6) ||
       get16(h+18) || get32(h+20))
        return status_error(s,ErrorCode::UNSUPPORTED,"unsupported MAD version/class/method/status or ECE");
    CmMessage d;d.type=static_cast<CmMessageType>(get16(h+16));
    size_t limit=private_data_limit(d.type);
    if(!limit)return status_error(s,ErrorCode::UNSUPPORTED,"unsupported CM attribute (RC primary path only)");
    d.transaction_id=get64(h+8);d.local_id=get32(b);
    if(d.type!=CmMessageType::REQ)d.remote_id=get32(b+4);
    if(!d.transaction_id || (!d.local_id&&d.type!=CmMessageType::REJ) || (d.type!=CmMessageType::REQ && !d.remote_id))
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"zero CM transaction/communication ID");
    CmParameters &p=d.parameters;
    switch(d.type) {
    case CmMessageType::REQ:
        /* Reject unsupported transports, alternate paths, SRQ, RDC and ECE.
         * Vendor fields are informational and need not be zero. */
        if((b[43]&7) || (b[50]&8) || (b[51]&15) || !all_zero(b+96,44) ||
           get24(b+36) || get24(b+40) || get32(b+28))
            return status_error(s,ErrorCode::UNSUPPORTED,"unsupported CM REQ capability");
        d.service_id=get64(b+8);p.ca_guid=get64(b+16);p.qpn=get24(b+32);
        p.responder_resources=b[35];p.initiator_depth=b[39];p.response_timeout=b[43]>>3;
        p.psn=get24(b+44);p.local_response_timeout=b[47]>>3;p.retry_count=b[47]&7;p.pkey=get16(b+48);
        p.mtu=b[50]>>4;p.rnr_retry_count=b[50]&7;p.max_cm_retries=b[51]>>4;
        std::memcpy(d.local_gid.data(),b+56,16);std::memcpy(d.remote_gid.data(),b+72,16);
        d.flow_label=(uint32_t(b[88])<<12)|(uint32_t(b[89])<<4)|(b[90]>>4);
        d.traffic_class=b[92];d.hop_limit=b[93];p.ack_timeout=b[95]>>3;
        if(d.flow_label || (b[94]&0xf0))
            return status_error(s,ErrorCode::UNSUPPORTED,"nonzero CM flow label/service level not supported by native carrier");
        if(!d.hop_limit || (b[90]&15) || (b[91]&0xc0) || (b[94]&7) || (b[95]&7) || b[4] || !all_zero(b+24,4))
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM REQ has invalid hop limit/reserved fields");
        if(!d.service_id)return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM REQ service ID zero");
        if(!valid_parameters(p,s))return false;
        break;
    case CmMessageType::REP:
        if(get32(b+8) || get24(b+16) || (b[26]&7) || (b[27]&31))
            return status_error(s,ErrorCode::UNSUPPORTED,"unsupported CM REP capability");
        p.qpn=get24(b+12);p.psn=get24(b+20);p.responder_resources=b[24];
        p.initiator_depth=b[25];p.ack_timeout=b[26]>>3;p.rnr_retry_count=b[27]>>5;
        p.ca_guid=get64(b+28);if(!valid_parameters(p,s))return false;break;
    case CmMessageType::MRA:
        if((b[8]&63)||(b[9]&7)||(b[8]>>6)>1)
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid MRA fields");
        d.message_mraed=b[8]>>6;d.service_timeout=b[9]>>3;break;
    case CmMessageType::REJ:
        d.message_mraed=b[8]>>6;d.reject_info_length=b[9]>>1;
        if((b[8]&63)||(b[9]&1)||d.message_mraed>2||d.reject_info_length>72)
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid REJ fields");
        d.reject_reason=get16(b+10);std::memcpy(d.reject_info.data(),b+12,d.reject_info_length);break;
    case CmMessageType::DREQ:
        if(b[11])return status_error(s,ErrorCode::PROTOCOL_ERROR,"DREQ reserved bits nonzero");
        d.remote_qpn=get24(b+8);break;
    default:break;
    }
    d.private_data_length=limit;
    std::memcpy(d.private_data.data(),b+private_offset(d.type),limit);
    m=d;status_ok(s);return true;
}
bool NativeCmCodec::encode_frame(const NativeEndpoint &path,const CmMessage &m,uint32_t psn,
    uint8_t *out,size_t cap,size_t &written,Status *s) {
    uint8_t mad[NATIVE_CM_MAD_SIZE];if(!encode(m,mad,sizeof(mad),s))return false;
    PacketEncodeSpec spec;spec.endpoint=path;spec.opcode=IB_UD_SEND_ONLY;
    spec.has_reth=false;
    spec.pkey=m.parameters.pkey;spec.dest_qpn=1;spec.psn=psn&NATIVE_MAX_U24;
    spec.has_deth=true;spec.deth.qkey=NATIVE_GSI_QKEY;spec.deth.source_qpn=1;
    spec.payload=ByteSpan(mad,sizeof(mad));CodecError e;
    if(!NativePacketCodec::encode(spec,out,cap,written,&e))
        return status_error(s,ErrorCode::PROTOCOL_ERROR,e.message);
    status_ok(s);return true;
}
bool NativeCmCodec::decode_frame(ByteSpan frame,NativeEndpoint &path,CmMessage &m,Status *s) {
    DecodedPacket p;CodecError e;
    if(!NativePacketCodec::decode(frame,p,&e))return status_error(s,ErrorCode::PROTOCOL_ERROR,e.message);
    if(p.opcode!=IB_UD_SEND_ONLY || p.dest_qpn!=1 || !p.has_deth ||
       p.deth.source_qpn!=1 || p.deth.qkey!=NATIVE_GSI_QKEY || (p.pkey&0x7fff)!=0x7fff)
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid QP1 GSI envelope");
    if(!decode(ByteSpan(frame.data+p.payload.offset,p.payload.length),m,s))return false;
    if(m.type==CmMessageType::REQ && m.parameters.pkey!=p.pkey)
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM REQ P_Key differs from BTH");
    path=p.endpoint;status_ok(s);return true;
}
uint64_t NativeCmCodec::ip_service_id(uint16_t port,uint16_t space) { return (uint64_t(space)<<16)|port; }
void NativeCmCodec::ip_gid(bool ipv6,const uint8_t *ip,std::array<uint8_t,16> &gid) {
    gid.fill(0);if(ipv6)std::memcpy(gid.data(),ip,16);
    else {gid[10]=gid[11]=0xff;std::memcpy(gid.data()+12,ip,4);}
}
bool NativeCmCodec::encode_ip_private(const CmaIpHeader &h,ByteSpan app,CmMessage &m,Status *s) {
    const size_t limit=private_data_limit(m.type);
    if(m.type!=CmMessageType::REQ || !h.source_port || app.size>limit-36 || (app.size&&!app.data))
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"IP CMA header requires REQ and at most 56 application bytes");
    m.private_data.fill(0);uint8_t *p=m.private_data.data();p[1]=h.ipv6?0x60:0x40;
    put16(p+2,h.source_port);
    if(h.ipv6) {std::memcpy(p+4,h.source_ip.data(),16);std::memcpy(p+20,h.destination_ip.data(),16);}
    else {std::memcpy(p+16,h.source_ip.data(),4);std::memcpy(p+32,h.destination_ip.data(),4);}
    if(app.size)std::memcpy(p+36,app.data,app.size);
    m.private_data_length=36+app.size;
    status_ok(s);return true;
}
bool NativeCmCodec::decode_ip_private(const CmMessage &m,CmaIpHeader &h,ByteSpan &app,Status *s) {
    if(m.type!=CmMessageType::REQ || m.private_data_length<36 || m.private_data[0] ||
       (m.private_data[1]!=0x40 && m.private_data[1]!=0x60))
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid IP CMA version/family");
    CmaIpHeader d;const uint8_t *p=m.private_data.data();d.ipv6=p[1]==0x60;d.source_port=get16(p+2);
    if(!d.source_port)return status_error(s,ErrorCode::PROTOCOL_ERROR,"zero IP CMA source port");
    if(!d.ipv6 && (!all_zero(p+4,12)||!all_zero(p+20,12)))
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"nonzero IPv4 CMA address padding");
    std::memcpy(d.source_ip.data(),p+(d.ipv6?4:16),d.ipv6?16:4);
    std::memcpy(d.destination_ip.data(),p+(d.ipv6?20:32),d.ipv6?16:4);
    h=d;app=ByteSpan(p+36,m.private_data_length-36);status_ok(s);return true;
}
} }
