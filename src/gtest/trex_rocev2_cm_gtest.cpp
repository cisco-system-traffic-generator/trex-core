/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#include <common/gtest.h>
#include "stx/stl/trex_rocev2_native_cm.h"
#include "stx/stl/trex_rocev2_native_neighbor.h"
#include <cstring>

using namespace TrexRoceV2;
using namespace TrexRoceV2::Native;
namespace TrexRoceV2 { namespace Native {
std::ostream &operator<<(std::ostream &out,CmMessageType t) {return out<<static_cast<int>(t);}
std::ostream &operator<<(std::ostream &out,CmEventType t) {return out<<cm_event_to_string(t);}
std::ostream &operator<<(std::ostream &out,NeighborEventType t) {return out<<static_cast<int>(t);}
} }
namespace {
NativeEndpoint cm_path(bool reverse=false,bool v6=false,bool vlan=false) {
    NativeEndpoint p;p.ipv6=v6;p.has_vlan=vlan;p.vlan=vlan?123:0;
    p.src_mac[0]=p.dst_mac[0]=2;p.src_mac[5]=reverse?2:1;p.dst_mac[5]=reverse?1:2;
    if(v6){p.src_ip[0]=p.dst_ip[0]=0x20;p.src_ip[1]=p.dst_ip[1]=1;p.src_ip[2]=p.dst_ip[2]=0xd;p.src_ip[3]=p.dst_ip[3]=0xb8;
        p.src_ip[15]=reverse?2:1;p.dst_ip[15]=reverse?1:2;}
    else{p.src_ip[0]=p.dst_ip[0]=192;p.src_ip[1]=p.dst_ip[1]=0;p.src_ip[2]=p.dst_ip[2]=2;
        p.src_ip[3]=reverse?2:1;p.dst_ip[3]=reverse?1:2;}
    p.udp_sport=4791;p.udp_dport=4791;return p;
}
CmMessage cm_message(CmMessageType type) {
    CmMessage m;m.type=type;m.transaction_id=0x0102030405060708ULL;m.local_id=0x11223344;m.remote_id=0x55667788;
    m.service_id=NativeCmCodec::ip_service_id(9999);m.parameters.qpn=0x123456;m.parameters.psn=0xabcdef;
    m.parameters.ca_guid=0x1020304050607080ULL;m.remote_qpn=0x654321;m.reject_reason=28;
    auto p=cm_path();NativeCmCodec::ip_gid(false,p.src_ip,m.local_gid);NativeCmCodec::ip_gid(false,p.dst_ip,m.remote_gid);
    return m;
}
bool transfer(NativeCmManager &from,NativeCmManager &to,uint64_t now,bool drop=false) {
    CmTxFrame f;if(!from.peek_tx(f))return false;
    if(!from.complete_tx(f.token,true,now))return false;
    if(drop)return true;
    Status s;
    bool ok=to.receive(f.port,ByteSpan(f.data.data(),f.length),now,&s);
    EXPECT_TRUE(ok)<<s.message();return ok;
}
struct CmPair {
    NativeCmManager a,b;
    uint32_t active,passive;
    uint64_t now;
    CmPair():active(0),passive(0),now(100) {}
    void configure(bool v6=false,bool vlan=false) {
        NativeCmConfig ac,bc;ac.connection_id_base=0x01000001;ac.connection_id_limit=0x01ffffff;
        bc.connection_id_base=0x02000001;bc.connection_id_limit=0x02ffffff;
        ASSERT_TRUE(a.configure(ac));ASSERT_TRUE(b.configure(bc));
        CmListenRequest l;l.listener_id=123;l.service_id=NativeCmCodec::ip_service_id(9999);l.endpoint=cm_path(true,v6,vlan);
        ASSERT_TRUE(b.listen(l));CmConnectRequest c;c.operation_id=42;c.service_id=l.service_id;c.source_port=40000;
        c.endpoint=cm_path(false,v6,vlan);c.parameters.qpn=20;c.parameters.psn=0xfffffe;
        Status status;
        ASSERT_TRUE(a.connect(c,now,active,&status))<<status.message();ASSERT_TRUE(transfer(a,b,now));
        CmEvent e;ASSERT_TRUE(b.pop_event(e));ASSERT_EQ(CmEventType::CONNECT_REQUEST,e.type);passive=e.connection.connection_id;
    }
    void establish(bool drop_rtu=false) {
        ASSERT_TRUE(transfer(b,a,now)); // MRA
        CmParameters p;p.qpn=21;p.psn=123;
        ASSERT_TRUE(b.accept(passive,p,ByteSpan(),now));ASSERT_TRUE(transfer(b,a,now));
        CmEvent response;ASSERT_TRUE(a.pop_event(response));EXPECT_EQ(CmEventType::CONNECT_RESPONSE,response.type);
        ASSERT_TRUE(a.confirm(active,now));
        ASSERT_TRUE(transfer(a,b,now,drop_rtu));
    }
};
}
TEST(rocev2_cm, req_exact_field_offsets) {
    CmMessage m=cm_message(CmMessageType::REQ);m.parameters.responder_resources=9;m.parameters.initiator_depth=7;
    m.parameters.mtu=4;m.parameters.retry_count=5;m.parameters.rnr_retry_count=3;
    m.parameters.response_timeout=19;m.parameters.local_response_timeout=17;m.parameters.max_cm_retries=6;
    m.private_data_length=2;m.private_data[0]=0xfa;m.private_data[1]=0xce;
    uint8_t bytes[256];ASSERT_TRUE(NativeCmCodec::encode(m,bytes,sizeof(bytes)));
    const uint8_t header[24]={1,7,2,3,0,0,0,0,1,2,3,4,5,6,7,8,0,0x10,0,0,0,0,0,0};
    EXPECT_EQ(0,std::memcmp(bytes,header,24));const uint8_t *b=bytes+24;
    EXPECT_EQ(0x11,b[0]);EXPECT_EQ(0x44,b[3]);EXPECT_EQ(0x12,b[32]);EXPECT_EQ(0x56,b[34]);
    EXPECT_EQ(9,b[35]);EXPECT_EQ(7,b[39]);EXPECT_EQ(19<<3,b[43]);EXPECT_EQ(0xab,b[44]);
    EXPECT_EQ((17<<3)|5,b[47]);EXPECT_EQ(0x43,b[50]);EXPECT_EQ(0x60,b[51]);
    EXPECT_EQ(0xfa,b[140]);EXPECT_EQ(0xce,b[141]);EXPECT_EQ(0,b[142]);
    CmMessage decoded;ASSERT_TRUE(NativeCmCodec::decode(ByteSpan(bytes,sizeof(bytes)),decoded));
    EXPECT_EQ(19,decoded.parameters.response_timeout);EXPECT_EQ(17,decoded.parameters.local_response_timeout);
}
TEST(rocev2_cm, all_mad_messages_roundtrip_and_bounds) {
    const CmMessageType types[]={CmMessageType::REQ,CmMessageType::REP,CmMessageType::RTU,CmMessageType::MRA,
        CmMessageType::REJ,CmMessageType::DREQ,CmMessageType::DREP};
    for(auto t:types){CmMessage m=cm_message(t);m.private_data_length=NativeCmCodec::private_data_limit(t);
        m.private_data.fill(0xa5);uint8_t bytes[256];ASSERT_TRUE(NativeCmCodec::encode(m,bytes,sizeof(bytes)));
        CmMessage decoded;ASSERT_TRUE(NativeCmCodec::decode(ByteSpan(bytes,sizeof(bytes)),decoded));
        EXPECT_EQ(t,decoded.type);EXPECT_EQ(m.local_id,decoded.local_id);EXPECT_EQ(m.transaction_id,decoded.transaction_id);
        EXPECT_EQ(m.private_data_length,decoded.private_data_length);EXPECT_EQ(0,std::memcmp(m.private_data.data(),decoded.private_data.data(),m.private_data_length));
        ++m.private_data_length;EXPECT_FALSE(NativeCmCodec::encode(m,bytes,sizeof(bytes)));
        EXPECT_FALSE(NativeCmCodec::decode(ByteSpan(bytes,255),decoded));
    }
}
TEST(rocev2_cm, ip_cma_layout_matches_standard_padding_and_service_id) {
    CmMessage m=cm_message(CmMessageType::REQ);CmaIpHeader h;h.source_port=40000;
    h.source_ip[0]=192;h.source_ip[3]=1;h.destination_ip[0]=192;h.destination_ip[3]=2;
    const uint8_t app[]={1,2,3};ASSERT_TRUE(NativeCmCodec::encode_ip_private(h,ByteSpan(app,3),m));
    EXPECT_EQ(0,m.private_data[0]);EXPECT_EQ(0x40,m.private_data[1]);EXPECT_EQ(0x9c,m.private_data[2]);EXPECT_EQ(0x40,m.private_data[3]);
    EXPECT_EQ(192,m.private_data[16]);EXPECT_EQ(192,m.private_data[32]);EXPECT_EQ(1,m.private_data[36]);
    EXPECT_EQ(0x0106270fULL,NativeCmCodec::ip_service_id(9999));CmaIpHeader decoded;ByteSpan application;
    ASSERT_TRUE(NativeCmCodec::decode_ip_private(m,decoded,application));EXPECT_EQ(3U,application.size);
    m.private_data[4]=1;EXPECT_FALSE(NativeCmCodec::decode_ip_private(m,decoded,application));
}
TEST(rocev2_cm, invalid_transport_and_ece_rejected) {
    CmMessage m=cm_message(CmMessageType::REQ),d;uint8_t bytes[256];ASSERT_TRUE(NativeCmCodec::encode(m,bytes,sizeof(bytes)));
    bytes[24+43]|=2;EXPECT_FALSE(NativeCmCodec::decode(ByteSpan(bytes,256),d));bytes[24+43]&=~2;
    bytes[23]=1;EXPECT_FALSE(NativeCmCodec::decode(ByteSpan(bytes,256),d));
}
TEST(rocev2_cm, active_passive_ipv4_and_ipv6_vlan_establishment) {
    for(unsigned i=0;i<2;++i){CmPair p;p.configure(i,i);p.establish();CmEvent ae,be;
        ASSERT_TRUE(p.a.pop_event(ae));ASSERT_TRUE(p.b.pop_event(be));EXPECT_EQ(CmEventType::ESTABLISHED,ae.type);
        EXPECT_EQ(CmEventType::ESTABLISHED,be.type);EXPECT_EQ(21U,ae.connection.remote.qpn);EXPECT_EQ(20U,be.connection.remote.qpn);
        EXPECT_EQ(16,ae.connection.requester_depth);EXPECT_EQ(3,ae.connection.negotiated_mtu);
        EXPECT_EQ(123U,ae.connection.send_psn);EXPECT_EQ(0xfffffeU,ae.connection.receive_psn);
        EXPECT_EQ(0xfffffeU,be.connection.send_psn);EXPECT_EQ(123U,be.connection.receive_psn);
        EXPECT_EQ(0x01000001U,p.active);EXPECT_EQ(0x02000001U,p.passive);
    }
}
TEST(rocev2_cm, lost_rtu_recovers_from_replayed_rep_once) {
    CmPair p;p.configure();p.establish(true);p.now+=5000000000ULL;p.b.poll(p.now);
    ASSERT_TRUE(transfer(p.b,p.a,p.now));ASSERT_TRUE(transfer(p.a,p.b,p.now));
    CmEvent e;ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::ESTABLISHED,e.type);EXPECT_FALSE(p.a.pop_event(e));
    ASSERT_TRUE(p.b.pop_event(e));EXPECT_EQ(CmEventType::ESTABLISHED,e.type);EXPECT_FALSE(p.b.pop_event(e));
}
TEST(rocev2_cm, tx_backpressure_does_not_arm_retry_timer) {
    CmPair p;p.configure();CmTxFrame before,after;ASSERT_TRUE(p.b.peek_tx(before));
    ASSERT_TRUE(p.b.complete_tx(before.token,false,p.now));p.b.poll(p.now+1000000);
    ASSERT_TRUE(p.b.peek_tx(after));EXPECT_EQ(before.token,after.token);EXPECT_EQ(0U,p.b.statistics().retries);
}
TEST(rocev2_cm, dreq_drep_and_duplicate_replay) {
    CmPair p;p.configure();p.establish();CmEvent e;ASSERT_TRUE(p.a.pop_event(e));ASSERT_TRUE(p.b.pop_event(e));
    ASSERT_TRUE(p.a.disconnect(p.active,p.now));CmTxFrame dreq;ASSERT_TRUE(p.a.peek_tx(dreq));
    ASSERT_TRUE(transfer(p.a,p.b,p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now,true));
    ASSERT_TRUE(p.b.receive(0,ByteSpan(dreq.data.data(),dreq.length),p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now));
    ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.a.pop_event(e));
    ASSERT_TRUE(p.b.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.b.pop_event(e));
}
TEST(rocev2_cm, pending_accept_times_out_without_qp_start) {
    CmPair p;p.configure();ASSERT_TRUE(transfer(p.b,p.a,p.now));p.b.poll(6000000000ULL);
    CmEvent e;ASSERT_TRUE(p.b.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);
    CmParameters params;params.qpn=21;EXPECT_FALSE(p.b.accept(p.passive,params,ByteSpan(),6000000000ULL));
}
TEST(rocev2_cm, consumer_rejection_reaches_active_peer) {
    CmPair p;p.configure();const uint8_t private_data[]={0xa5,0x5a};
    ASSERT_TRUE(p.b.reject(p.passive,28,ByteSpan(private_data,2),p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now));
    CmEvent e;ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::REJECTED,e.type);EXPECT_EQ(28,e.reason);
    EXPECT_EQ(0xa5,e.private_data[0]);EXPECT_EQ(0x5a,e.private_data[1]);
}
TEST(rocev2_cm, ids_not_reused_after_timewait) {
    NativeCmManager a;NativeCmConfig c;c.max_connections=1;c.max_events=3;c.connection_id_base=c.connection_id_limit=99;
    ASSERT_TRUE(a.configure(c));CmConnectRequest r;r.operation_id=1;r.source_port=1234;r.service_id=NativeCmCodec::ip_service_id(9999);
    r.parameters.qpn=2;r.endpoint=cm_path();uint32_t id;ASSERT_TRUE(a.connect(r,0,id));ASSERT_TRUE(a.cancel(id,0));
    a.poll(2000000000ULL);CmEvent e;ASSERT_TRUE(a.pop_event(e));EXPECT_FALSE(a.connect(r,2000000000ULL,id));
}
TEST(rocev2_cm, unknown_service_is_rejected_without_allocating_connection) {
    NativeCmManager a,b;ASSERT_TRUE(a.configure(NativeCmConfig()));ASSERT_TRUE(b.configure(NativeCmConfig()));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=NativeCmCodec::ip_service_id(9999);
    r.endpoint=cm_path();r.parameters.qpn=20;uint32_t id;ASSERT_TRUE(a.connect(r,0,id));
    ASSERT_TRUE(transfer(a,b,0));EXPECT_EQ(0U,b.live_connections());ASSERT_TRUE(transfer(b,a,0));
    CmEvent e;ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(CmEventType::REJECTED,e.type);EXPECT_EQ(8,e.reason);
}
TEST(rocev2_cm, both_sides_disconnect_once_under_simultaneous_close) {
    CmPair p;p.configure();p.establish();CmEvent e;ASSERT_TRUE(p.a.pop_event(e));ASSERT_TRUE(p.b.pop_event(e));
    ASSERT_TRUE(p.a.disconnect(p.active,p.now));ASSERT_TRUE(p.b.disconnect(p.passive,p.now));
    CmTxFrame ad,bd;ASSERT_TRUE(p.a.peek_tx(ad));ASSERT_TRUE(p.b.peek_tx(bd));
    ASSERT_TRUE(p.a.complete_tx(ad.token,true,p.now));ASSERT_TRUE(p.b.complete_tx(bd.token,true,p.now));
    ASSERT_TRUE(p.a.receive(0,ByteSpan(bd.data.data(),bd.length),p.now));
    ASSERT_TRUE(p.b.receive(0,ByteSpan(ad.data.data(),ad.length),p.now));
    ASSERT_TRUE(transfer(p.a,p.b,p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now));
    ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.a.pop_event(e));
    ASSERT_TRUE(p.b.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.b.pop_event(e));
}
TEST(rocev2_cm, lost_req_and_rep_retransmit_from_accepted_tx) {
    NativeCmManager a,b;ASSERT_TRUE(a.configure(NativeCmConfig()));ASSERT_TRUE(b.configure(NativeCmConfig()));
    CmListenRequest l;l.listener_id=1;l.service_id=NativeCmCodec::ip_service_id(9999);l.endpoint=cm_path(true);ASSERT_TRUE(b.listen(l));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=l.service_id;r.endpoint=cm_path();r.parameters.qpn=20;
    uint32_t id;ASSERT_TRUE(a.connect(r,0,id));ASSERT_TRUE(transfer(a,b,0,true));a.poll(5000000000ULL);
    ASSERT_TRUE(transfer(a,b,5000000000ULL));CmEvent e;ASSERT_TRUE(b.pop_event(e));ASSERT_TRUE(transfer(b,a,5000000000ULL));
    CmParameters p;p.qpn=21;ASSERT_TRUE(b.accept(e.connection.connection_id,p,ByteSpan(),5000000000ULL));
    ASSERT_TRUE(transfer(b,a,5000000000ULL,true));b.poll(10000000000ULL);ASSERT_TRUE(transfer(b,a,10000000000ULL));
    ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(CmEventType::CONNECT_RESPONSE,e.type);ASSERT_TRUE(a.confirm(id,10000000000ULL));
    ASSERT_TRUE(transfer(a,b,10000000000ULL));ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(CmEventType::ESTABLISHED,e.type);
}
TEST(rocev2_cm, repeated_mra_cannot_extend_overall_handshake_deadline) {
    CmPair p;p.configure();CmTxFrame mra;ASSERT_TRUE(p.b.peek_tx(mra));ASSERT_TRUE(p.b.complete_tx(mra.token,true,p.now));
    for(uint64_t now=1;now<30;++now)ASSERT_TRUE(p.a.receive(0,ByteSpan(mra.data.data(),mra.length),now*1000000000ULL));
    p.a.poll(31000000000ULL);CmEvent e;ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);
}
TEST(rocev2_cm, event_pressure_prevents_accepting_unreportable_work) {
    NativeCmManager a;NativeCmConfig c;c.max_connections=1;c.max_events=3;ASSERT_TRUE(a.configure(c));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=NativeCmCodec::ip_service_id(9999);
    r.endpoint=cm_path();r.parameters.qpn=20;uint32_t id;ASSERT_TRUE(a.connect(r,0,id));ASSERT_TRUE(a.cancel(id,0));
    a.poll(2000000000ULL);EXPECT_FALSE(a.connect(r,2000000000ULL,id));CmEvent e;ASSERT_TRUE(a.pop_event(e));
    ASSERT_TRUE(a.connect(r,2000000000ULL,id));EXPECT_EQ(2U,id);
}
TEST(rocev2_cm, wrong_port_and_unknown_remote_id_do_not_establish) {
    CmPair p;p.configure();ASSERT_TRUE(transfer(p.b,p.a,p.now));CmParameters params;params.qpn=21;
    ASSERT_TRUE(p.b.accept(p.passive,params,ByteSpan(),p.now));CmTxFrame rep;ASSERT_TRUE(p.b.peek_tx(rep));
    EXPECT_FALSE(p.a.receive(1,ByteSpan(rep.data.data(),rep.length),p.now));CmEvent e;EXPECT_FALSE(p.a.pop_event(e));
    CmMessage m;NativeEndpoint path;ASSERT_TRUE(NativeCmCodec::decode_frame(ByteSpan(rep.data.data(),rep.length),path,m));
    m.remote_id+=1;size_t n;ASSERT_TRUE(NativeCmCodec::encode_frame(path,m,0,rep.data.data(),rep.data.size(),n));
    EXPECT_FALSE(p.a.receive(0,ByteSpan(rep.data.data(),n),p.now));EXPECT_FALSE(p.a.pop_event(e));
}
TEST(rocev2_cm, lost_dreq_retries_and_disconnects_without_duplicate_events) {
    CmPair p;p.configure();p.establish();CmEvent e;ASSERT_TRUE(p.a.pop_event(e));ASSERT_TRUE(p.b.pop_event(e));
    ASSERT_TRUE(p.a.disconnect(p.active,p.now));ASSERT_TRUE(transfer(p.a,p.b,p.now,true));
    p.a.poll(5000000000ULL);ASSERT_TRUE(transfer(p.a,p.b,5000000000ULL));ASSERT_TRUE(transfer(p.b,p.a,5000000000ULL));
    ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.a.pop_event(e));
    ASSERT_TRUE(p.b.pop_event(e));EXPECT_EQ(CmEventType::DISCONNECTED,e.type);EXPECT_FALSE(p.b.pop_event(e));
}
TEST(rocev2_cm, terminal_timeout_reclaims_unsent_packet_and_never_emits_established) {
    NativeCmManager a;NativeCmConfig c;c.handshake_timeout_ns=100;c.timewait_ns=100;ASSERT_TRUE(a.configure(c));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=NativeCmCodec::ip_service_id(9999);
    r.endpoint=cm_path();r.parameters.qpn=20;uint32_t id;ASSERT_TRUE(a.connect(r,0,id));a.poll(101);
    CmTxFrame f;EXPECT_FALSE(a.peek_tx(f));CmEvent e;ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);
    a.poll(202);EXPECT_EQ(0U,a.live_connections());EXPECT_FALSE(a.pop_event(e));
}
TEST(rocev2_cm, active_qp_preparation_gates_rtu_and_actual_tx_gates_established) {
    CmPair p;p.configure();ASSERT_TRUE(transfer(p.b,p.a,p.now));CmParameters params;params.qpn=21;
    ASSERT_TRUE(p.b.accept(p.passive,params,ByteSpan(),p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now));
    CmEvent e;ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::CONNECT_RESPONSE,e.type);EXPECT_FALSE(p.a.pop_event(e));
    CmTxFrame f;ASSERT_TRUE(p.a.peek_tx(f));NativeEndpoint path;CmMessage m;
    ASSERT_TRUE(NativeCmCodec::decode_frame(ByteSpan(f.data.data(),f.length),path,m));
    EXPECT_EQ(CmMessageType::MRA,m.type);EXPECT_EQ(1,m.message_mraed);ASSERT_TRUE(transfer(p.a,p.b,p.now));
    ASSERT_TRUE(p.a.confirm(p.active,p.now));ASSERT_TRUE(p.a.peek_tx(f));
    ASSERT_TRUE(NativeCmCodec::decode_frame(ByteSpan(f.data.data(),f.length),path,m));EXPECT_EQ(CmMessageType::RTU,m.type);
    ASSERT_TRUE(p.a.complete_tx(f.token,false,p.now));EXPECT_FALSE(p.a.pop_event(e));
    ASSERT_TRUE(transfer(p.a,p.b,p.now));ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::ESTABLISHED,e.type);
}
TEST(rocev2_cm, active_confirmation_timeout_does_not_publish_established) {
    CmPair p;p.configure();ASSERT_TRUE(transfer(p.b,p.a,p.now));CmParameters params;params.qpn=21;
    ASSERT_TRUE(p.b.accept(p.passive,params,ByteSpan(),p.now));ASSERT_TRUE(transfer(p.b,p.a,p.now));
    CmEvent e;ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::CONNECT_RESPONSE,e.type);
    p.a.poll(6000000000ULL);ASSERT_TRUE(p.a.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);
    EXPECT_FALSE(p.a.confirm(p.active,6000000000ULL));
}
TEST(rocev2_cm, active_req_timer_uses_remote_timeout_field) {
    NativeCmManager a;NativeCmConfig c;c.retry_timeout_ns=1;c.handshake_timeout_ns=1000000000;
    ASSERT_TRUE(a.configure(c));CmConnectRequest r;r.operation_id=1;r.source_port=40000;
    r.service_id=NativeCmCodec::ip_service_id(9999);r.endpoint=cm_path();r.parameters.qpn=20;
    r.parameters.response_timeout=2;r.parameters.local_response_timeout=8;
    uint32_t id;ASSERT_TRUE(a.connect(r,0,id));CmTxFrame f;ASSERT_TRUE(a.peek_tx(f));
    ASSERT_TRUE(a.complete_tx(f.token,true,0));a.poll(16000);EXPECT_FALSE(a.peek_tx(f));
    a.poll(16385);EXPECT_TRUE(a.peek_tx(f));EXPECT_EQ(1U,a.statistics().retries);
}
TEST(rocev2_cm, passive_rep_timer_uses_distinct_local_timeout_and_req_retry_limit) {
    for(unsigned retries=0;retries<2;++retries){
        NativeCmManager a,b;NativeCmConfig c;c.retry_timeout_ns=1;c.handshake_timeout_ns=1000000000;
        ASSERT_TRUE(a.configure(c));ASSERT_TRUE(b.configure(c));CmListenRequest l;
        l.listener_id=1;l.service_id=NativeCmCodec::ip_service_id(9999);l.endpoint=cm_path(true);ASSERT_TRUE(b.listen(l));
        CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=l.service_id;r.endpoint=cm_path();
        r.parameters.qpn=20;r.parameters.response_timeout=0;r.parameters.local_response_timeout=4;r.parameters.max_cm_retries=retries;
        uint32_t id;ASSERT_TRUE(a.connect(r,0,id));ASSERT_TRUE(transfer(a,b,0));CmEvent e;ASSERT_TRUE(b.pop_event(e));
        CmParameters p;p.qpn=21;ASSERT_TRUE(b.accept(e.connection.connection_id,p,ByteSpan(),0));
        ASSERT_TRUE(transfer(b,a,0,true));b.poll(5000);CmTxFrame f;EXPECT_FALSE(b.peek_tx(f));
        b.poll(65538);
        if(retries){EXPECT_TRUE(b.peek_tx(f));EXPECT_EQ(1U,b.statistics().retries);}
        else{EXPECT_FALSE(b.peek_tx(f));ASSERT_TRUE(b.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);}
    }
}
TEST(rocev2_cm, huge_peer_timeout_is_bounded_by_configured_handshake_deadline) {
    NativeCmManager a;NativeCmConfig c;c.retry_timeout_ns=1;c.handshake_timeout_ns=1000000;ASSERT_TRUE(a.configure(c));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=NativeCmCodec::ip_service_id(9999);
    r.endpoint=cm_path();r.parameters.qpn=20;r.parameters.response_timeout=31;uint32_t id;ASSERT_TRUE(a.connect(r,0,id));
    CmTxFrame f;ASSERT_TRUE(a.peek_tx(f));ASSERT_TRUE(a.complete_tx(f.token,true,0));a.poll(1000001);
    CmEvent e;ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(CmEventType::TIMEOUT,e.type);EXPECT_FALSE(a.peek_tx(f));
}
TEST(rocev2_cm, normalized_retry_and_psn_values_follow_receiver_direction) {
    NativeCmManager a,b;ASSERT_TRUE(a.configure(NativeCmConfig()));ASSERT_TRUE(b.configure(NativeCmConfig()));
    CmListenRequest l;l.listener_id=1;l.service_id=NativeCmCodec::ip_service_id(9999);l.endpoint=cm_path(true);ASSERT_TRUE(b.listen(l));
    CmConnectRequest r;r.operation_id=1;r.source_port=40000;r.service_id=l.service_id;r.endpoint=cm_path();
    r.parameters.qpn=20;r.parameters.psn=101;r.parameters.retry_count=2;r.parameters.rnr_retry_count=5;
    uint32_t id;ASSERT_TRUE(a.connect(r,0,id));ASSERT_TRUE(transfer(a,b,0));CmEvent request;ASSERT_TRUE(b.pop_event(request));
    EXPECT_EQ(101U,request.connection.send_psn);EXPECT_EQ(2,request.connection.retry_count);EXPECT_EQ(5,request.connection.rnr_retry_count);
    CmParameters p;p.qpn=21;p.psn=202;p.rnr_retry_count=6;ASSERT_TRUE(b.accept(request.connection.connection_id,p,ByteSpan(),0));
    ASSERT_TRUE(transfer(b,a,0));CmEvent response;ASSERT_TRUE(a.pop_event(response));EXPECT_EQ(CmEventType::CONNECT_RESPONSE,response.type);
    EXPECT_EQ(202U,response.connection.send_psn);EXPECT_EQ(101U,response.connection.receive_psn);
    EXPECT_EQ(2,response.connection.retry_count);EXPECT_EQ(6,response.connection.rnr_retry_count);
    ASSERT_TRUE(a.confirm(id,0));ASSERT_TRUE(transfer(a,b,0));CmEvent established;ASSERT_TRUE(b.pop_event(established));
    EXPECT_EQ(101U,established.connection.send_psn);EXPECT_EQ(202U,established.connection.receive_psn);
    EXPECT_EQ(2,established.connection.retry_count);EXPECT_EQ(5,established.connection.rnr_retry_count);
}
TEST(rocev2_neighbor, arp_and_ipv6_ns_na_resolve_owned_vlan_address) {
    for(unsigned v=0;v<2;++v){NativeNeighborManager a,b;NativeNeighborConfig config;
        ASSERT_TRUE(a.configure(config));ASSERT_TRUE(b.configure(config));NativeNeighborInterface ai,bi;
        ai.interface_id=1;bi.interface_id=2;ai.endpoint=cm_path(false,v,true);bi.endpoint=cm_path(true,v,true);
        ai.prefix_length=bi.prefix_length=v?64:24;ASSERT_TRUE(a.register_interface(ai));ASSERT_TRUE(b.register_interface(bi));
        NativeEndpoint route=ai.endpoint;std::memset(route.dst_mac,0,6);ASSERT_TRUE(a.resolve(44,0,route,0));
        CmTxFrame ns;ASSERT_TRUE(a.peek_tx(ns));ASSERT_TRUE(a.complete_tx(ns.token,true,0));ASSERT_TRUE(b.receive(0,ByteSpan(ns.data.data(),ns.length),0));
        CmTxFrame na;ASSERT_TRUE(b.peek_tx(na));ASSERT_TRUE(b.complete_tx(na.token,true,0));ASSERT_TRUE(a.receive(0,ByteSpan(na.data.data(),na.length),0));
        NativeNeighborEvent event;ASSERT_TRUE(a.pop_event(event));EXPECT_EQ(NeighborEventType::RESOLVED,event.type);
        EXPECT_EQ(44U,event.operation_id);EXPECT_EQ(0,std::memcmp(event.endpoint.dst_mac,bi.endpoint.src_mac,6));
        ASSERT_TRUE(a.resolve(45,0,route,1));EXPECT_FALSE(a.peek_tx(ns));ASSERT_TRUE(a.pop_event(event));EXPECT_EQ(45U,event.operation_id);
    }
}
TEST(rocev2_neighbor, route_uses_gateway_without_replacing_peer_ip) {
    NativeNeighborManager a;ASSERT_TRUE(a.configure(NativeNeighborConfig()));NativeNeighborInterface i;i.interface_id=1;
    i.endpoint=cm_path();i.prefix_length=24;i.has_gateway=true;i.gateway[0]=192;i.gateway[2]=2;i.gateway[3]=254;
    ASSERT_TRUE(a.register_interface(i));NativeEndpoint p=i.endpoint;p.dst_ip[0]=198;std::memset(p.dst_mac,0,6);
    ASSERT_TRUE(a.resolve(1,0,p,0));CmTxFrame f;ASSERT_TRUE(a.peek_tx(f));
    EXPECT_EQ(192,f.data[14+24]);EXPECT_EQ(254,f.data[14+27]);
}
TEST(rocev2_neighbor, retries_expire_and_unregistration_cancels) {
    NativeNeighborManager a;NativeNeighborConfig c;c.max_retries=0;ASSERT_TRUE(a.configure(c));
    NativeNeighborInterface i;i.interface_id=1;i.endpoint=cm_path();i.prefix_length=24;ASSERT_TRUE(a.register_interface(i));
    NativeEndpoint p=i.endpoint;std::memset(p.dst_mac,0,6);ASSERT_TRUE(a.resolve(1,0,p,0));CmTxFrame f;
    ASSERT_TRUE(a.peek_tx(f));ASSERT_TRUE(a.complete_tx(f.token,true,0));a.poll(1000000000ULL);
    NativeNeighborEvent e;ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(NeighborEventType::TIMEOUT,e.type);
    ASSERT_TRUE(a.resolve(2,0,p,0));ASSERT_TRUE(a.unregister_interface(1));EXPECT_FALSE(a.peek_tx(f));
    ASSERT_TRUE(a.pop_event(e));EXPECT_EQ(NeighborEventType::CANCELLED,e.type);
}
TEST(rocev2_neighbor, wrong_port_or_vlan_does_not_answer) {
    NativeNeighborManager a,b;ASSERT_TRUE(a.configure(NativeNeighborConfig()));ASSERT_TRUE(b.configure(NativeNeighborConfig()));
    NativeNeighborInterface ai,bi;ai.interface_id=1;bi.interface_id=2;ai.prefix_length=bi.prefix_length=24;
    ai.endpoint=cm_path();bi.endpoint=cm_path(true);ASSERT_TRUE(a.register_interface(ai));ASSERT_TRUE(b.register_interface(bi));
    NativeEndpoint p=ai.endpoint;std::memset(p.dst_mac,0,6);ASSERT_TRUE(a.resolve(1,0,p,0));CmTxFrame f;ASSERT_TRUE(a.peek_tx(f));
    EXPECT_FALSE(b.receive(1,ByteSpan(f.data.data(),f.length),0));EXPECT_FALSE(b.peek_tx(f));
}
TEST(rocev2_neighbor, corrupted_ipv6_checksum_and_wrong_mac_are_rejected) {
    NativeNeighborManager a,b;ASSERT_TRUE(a.configure(NativeNeighborConfig()));ASSERT_TRUE(b.configure(NativeNeighborConfig()));
    NativeNeighborInterface ai,bi;ai.interface_id=1;bi.interface_id=2;ai.prefix_length=bi.prefix_length=64;
    ai.endpoint=cm_path(false,true);bi.endpoint=cm_path(true,true);ASSERT_TRUE(a.register_interface(ai));ASSERT_TRUE(b.register_interface(bi));
    NativeEndpoint p=ai.endpoint;std::memset(p.dst_mac,0,6);ASSERT_TRUE(a.resolve(1,0,p,0));CmTxFrame f;ASSERT_TRUE(a.peek_tx(f));
    f.data[14+40+2]^=1;EXPECT_FALSE(b.receive(0,ByteSpan(f.data.data(),f.length),0));
    f.data[14+40+2]^=1;f.data[6+5]^=1;EXPECT_FALSE(b.receive(0,ByteSpan(f.data.data(),f.length),0));
}
TEST(rocev2_neighbor, operation_cancel_removes_only_its_unsent_probe_and_event) {
    NativeNeighborManager manager;ASSERT_TRUE(manager.configure(NativeNeighborConfig()));
    NativeNeighborInterface interface;interface.interface_id=1;interface.endpoint=cm_path();interface.prefix_length=24;
    ASSERT_TRUE(manager.register_interface(interface));
    NativeEndpoint first=interface.endpoint;std::memset(first.dst_mac,0,6);
    NativeEndpoint second=first;second.dst_ip[3]=3;
    ASSERT_TRUE(manager.resolve(101,0,first,0));ASSERT_TRUE(manager.resolve(102,0,second,0));
    ASSERT_TRUE(manager.cancel(101,10));Status status;
    EXPECT_FALSE(manager.cancel(101,11,&status));EXPECT_TRUE(status.code()==ErrorCode::NOT_FOUND);
    NativeNeighborEvent event;ASSERT_TRUE(manager.pop_event(event));EXPECT_EQ(101U,event.operation_id);
    EXPECT_EQ(NeighborEventType::CANCELLED,event.type);EXPECT_FALSE(manager.pop_event(event));
    CmTxFrame frame;ASSERT_TRUE(manager.peek_tx(frame));EXPECT_EQ(3,frame.data[14+27]);
    ASSERT_TRUE(manager.complete_tx(frame.token,true,12));EXPECT_FALSE(manager.peek_tx(frame));
    ASSERT_TRUE(manager.cancel(102,13));ASSERT_TRUE(manager.pop_event(event));EXPECT_EQ(102U,event.operation_id);
    EXPECT_EQ(NeighborEventType::CANCELLED,event.type);manager.poll(10000000000ULL);EXPECT_FALSE(manager.peek_tx(frame));
}
