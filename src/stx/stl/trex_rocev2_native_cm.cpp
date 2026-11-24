/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#include "trex_rocev2_native_cm.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace TrexRoceV2 { namespace Native {
namespace {
uint64_t after(uint64_t now,uint64_t delay) {
    return delay>std::numeric_limits<uint64_t>::max()-now?std::numeric_limits<uint64_t>::max():now+delay;
}
uint64_t cm_timeout(uint8_t encoded) { return uint64_t(4096)<<encoded; }
bool same_vlan(const NativeEndpoint &a,const NativeEndpoint &b) {
    return a.has_vlan==b.has_vlan && (!a.has_vlan || a.vlan==b.vlan);
}
bool same_ip(bool v6,const uint8_t *a,const uint8_t *b) { return !std::memcmp(a,b,v6?16:4); }
NativeEndpoint reverse_path(const NativeEndpoint &p) {
    NativeEndpoint r=p;std::memcpy(r.src_mac,p.dst_mac,6);std::memcpy(r.dst_mac,p.src_mac,6);
    std::memcpy(r.src_ip,p.dst_ip,16);std::memcpy(r.dst_ip,p.src_ip,16);
    r.udp_dport=ROCEV2_UDP_PORT;r.udp_sport=ROCEV2_UDP_PORT;r.ttl=64;return r;
}
bool copy_private(CmMessage &m,ByteSpan data,Status *s) {
    if(data.size>NativeCmCodec::private_data_limit(m.type)||(data.size&&!data.data))
        return status_error(s,ErrorCode::OUT_OF_RANGE,"CM private data exceeds message field");
    m.private_data_length=data.size;if(data.size)std::memcpy(m.private_data.data(),data.data,data.size);return true;
}
}
NativeCmConfig::NativeCmConfig():max_connections(256),max_listeners(32),max_events(768),
    max_tx_frames(256),retry_timeout_ns(100000000),pending_accept_timeout_ns(5000000000ULL),
    timewait_ns(1000000000),handshake_timeout_ns(30000000000ULL),max_retries(7),connection_id_base(1),connection_id_limit(0xffffffffU) {}
CmListenRequest::CmListenRequest():listener_id(0),service_id(0),port(0),backlog(64),ip_cma(true) {}
CmConnectRequest::CmConnectRequest():operation_id(0),service_id(0),port(0),source_port(0),
    ip_cma(true),private_data_length(0) {private_data.fill(0);}
CmConnectionInfo::CmConnectionInfo():connection_id(0),remote_id(0),operation_id(0),listener_id(0),
    service_id(0),port(0),active(false),state(CmState::FREE),send_psn(0),receive_psn(0),retry_count(0),rnr_retry_count(0),
    negotiated_mtu(0),requester_depth(0),responder_depth(0) {}
CmEvent::CmEvent():type(CmEventType::TIMEOUT),reason(0),private_data_length(0) {private_data.fill(0);}
CmTxFrame::CmTxFrame():token(0),connection_id(0),port(0),length(0) {data.fill(0);}
CmStatistics::CmStatistics():received(0),transmitted(0),retries(0),malformed(0),unknown(0),duplicates(0),
    established(0),disconnected(0),rejected(0),timeouts(0),resource_exhaustion(0) {}
NativeCmManager::Slot::Slot():transaction_id(0),deadline(0),timewait_deadline(0),overall_deadline(0),close_transaction_id(0),retries(0),event_budget(0),
    awaiting_tx(false),retry_message(false),emitted_terminal(false),established_event(false) {}
NativeCmManager::NativeCmManager():m_configured(false),m_next_id(1),m_ud_psn(0),
    m_next_transaction(1),m_next_token(1) {}
bool NativeCmManager::configure(const NativeCmConfig &c,Status *s) {
    if(m_configured && live_connections())return status_error(s,ErrorCode::INVALID_STATE,"CM connections still live");
    if(!c.max_connections || c.max_connections>65536 || !c.max_listeners || c.max_listeners>65536 ||
       c.max_events<c.max_connections*3 || c.max_events>1048576 || !c.max_tx_frames || c.max_tx_frames>65536 ||
       !c.retry_timeout_ns || !c.pending_accept_timeout_ns || !c.timewait_ns || !c.handshake_timeout_ns || c.max_retries>15 ||
       !c.connection_id_base || c.connection_id_base>c.connection_id_limit)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid bounded CM resources or timers");
    m_config=c;m_slots.assign(c.max_connections,Slot());m_listeners.clear();m_events.clear();m_tx.clear();
    m_listeners.reserve(c.max_listeners);m_events.reserve(c.max_events);m_tx.reserve(c.max_tx_frames);
    m_statistics=CmStatistics();m_next_id=c.connection_id_base;m_configured=true;status_ok(s);return true;
}
NativeCmManager::Slot *NativeCmManager::find(uint32_t id) {
    for(auto &x:m_slots)if(x.info.state!=CmState::FREE && x.info.connection_id==id)return &x;
    return NULL;
}
const NativeCmManager::Slot *NativeCmManager::find(uint32_t id) const {
    for(const auto &x:m_slots)if(x.info.state!=CmState::FREE && x.info.connection_id==id)return &x;
    return NULL;
}
NativeCmManager::Slot *NativeCmManager::allocate(uint64_t) {
    size_t reserved=m_events.size()+3;
    for(const auto &x:m_slots)reserved+=x.event_budget;
    if(reserved>m_config.max_events){++m_statistics.resource_exhaustion;return NULL;}
    for(auto &x:m_slots)if(x.info.state==CmState::FREE) {
        if(!m_next_id || m_next_id>m_config.connection_id_limit){++m_statistics.resource_exhaustion;return NULL;}
        x=Slot();x.info.connection_id=m_next_id++;x.event_budget=3;return &x;
    }
    ++m_statistics.resource_exhaustion;return NULL;
}
bool NativeCmManager::listen(const CmListenRequest &r,Status *s) {
    if(!m_configured || !r.listener_id || !r.service_id || (r.ip_cma && !(r.service_id&0xffff)) ||
       !r.backlog || r.backlog>m_config.max_connections)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid CM listener");
    for(const auto &x:m_listeners)if(x.listener_id==r.listener_id ||
       (x.service_id==r.service_id && x.port==r.port && x.endpoint.ipv6==r.endpoint.ipv6 &&
        same_vlan(x.endpoint,r.endpoint)&&same_ip(r.endpoint.ipv6,x.endpoint.src_ip,r.endpoint.src_ip)))
        return status_error(s,ErrorCode::DUPLICATE,"duplicate CM listener");
    if(m_listeners.size()==m_config.max_listeners)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"CM listener bound reached");
    m_listeners.push_back(r);status_ok(s);return true;
}
bool NativeCmManager::unlisten(uint64_t id,Status *s) {
    for(size_t i=0;i<m_listeners.size();++i)if(m_listeners[i].listener_id==id) {
        m_listeners.erase(m_listeners.begin()+i);status_ok(s);return true;
    }
    return status_error(s,ErrorCode::NOT_FOUND,"CM listener not found");
}
CmMessage NativeCmManager::response(const Slot &x,CmMessageType type) const {
    CmMessage m;m.type=type;m.transaction_id=x.transaction_id;m.local_id=x.info.connection_id;
    m.remote_id=x.info.remote_id;m.parameters=x.info.local;m.service_id=x.info.service_id;
    NativeCmCodec::ip_gid(x.info.endpoint.ipv6,x.info.endpoint.src_ip,m.local_gid);
    NativeCmCodec::ip_gid(x.info.endpoint.ipv6,x.info.endpoint.dst_ip,m.remote_gid);
    m.traffic_class=(x.info.endpoint.dscp<<2)|x.info.endpoint.ecn;m.hop_limit=x.info.endpoint.ttl;return m;
}
bool NativeCmManager::queue(Slot &x,const CmMessage &m,bool arm,Status *s) {
    if(x.awaiting_tx) {status_ok(s);return true;}
    if(m_tx.size()==m_config.max_tx_frames || !m_next_token)
        return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"CM TX queue full");
    CmTxFrame f;f.token=m_next_token++;f.connection_id=x.info.connection_id;f.port=x.info.port;
    if(!NativeCmCodec::encode_frame(x.info.endpoint,m,m_ud_psn++,f.data.data(),f.data.size(),f.length,s))return false;
    m_tx.push_back(f);x.awaiting_tx=true;x.retry_message=arm;x.saved=m;
    if(arm)x.deadline=0;
    status_ok(s);return true;
}
bool NativeCmManager::event(Slot &x,CmEventType type,const CmMessage *m,uint16_t reason) {
    if(!x.event_budget || m_events.size()==m_config.max_events)return false;
    CmEvent e;e.type=type;e.connection=x.info;e.reason=reason;
    if(m){e.private_data=m->private_data;e.private_data_length=m->private_data_length;}
    m_events.push_back(e);--x.event_budget;return true;
}
void NativeCmManager::terminal(Slot &x,CmEventType type,uint64_t now,uint16_t reason,const CmMessage *message) {
    if(x.awaiting_tx && x.saved.type!=CmMessageType::DREP && x.saved.type!=CmMessageType::REJ) {
        m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[&x](const CmTxFrame &f){return f.connection_id==x.info.connection_id;}),m_tx.end());
        x.awaiting_tx=false;
    }
    x.info.state=CmState::TIME_WAIT;x.deadline=0;x.timewait_deadline=after(now,m_config.timewait_ns);
    if(!x.emitted_terminal){event(x,type,message,reason);x.emitted_terminal=true;
        if(type==CmEventType::TIMEOUT)++m_statistics.timeouts;
        if(type==CmEventType::DISCONNECTED)++m_statistics.disconnected;
        if(type==CmEventType::REJECTED)++m_statistics.rejected;}
}
bool NativeCmManager::reject_request(uint16_t port,const NativeEndpoint &path,const CmMessage &request,
    uint16_t reason,Status *s) {
    if(m_tx.size()==m_config.max_tx_frames || !m_next_token)
        return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"CM rejection TX queue full");
    CmMessage reject;reject.type=CmMessageType::REJ;reject.transaction_id=request.transaction_id;
    reject.local_id=0;reject.remote_id=request.local_id;reject.reject_reason=reason;
    CmTxFrame f;f.token=m_next_token++;f.port=port;
    if(!NativeCmCodec::encode_frame(reverse_path(path),reject,m_ud_psn++,f.data.data(),f.data.size(),f.length,s))return false;
    m_tx.push_back(f);++m_statistics.rejected;status_ok(s);return true;
}
void NativeCmManager::negotiate(Slot &x) {
    x.info.send_psn=x.info.remote.psn;
    x.info.receive_psn=x.info.local.psn;
    x.info.retry_count=x.info.active?x.info.local.retry_count:x.info.remote.retry_count;
    x.info.rnr_retry_count=x.info.remote.rnr_retry_count;
    x.info.negotiated_mtu=std::min(x.info.local.mtu,x.info.remote.mtu);
    x.info.requester_depth=std::min(x.info.local.initiator_depth,x.info.remote.responder_resources);
    x.info.responder_depth=std::min(x.info.local.responder_resources,x.info.remote.initiator_depth);
}
uint64_t NativeCmManager::response_deadline(const Slot &x,uint64_t now,uint8_t exponent) const {
    // Configured carrier round-trip allowance plus the peer's advertised
    // processing time. The hard handshake deadline bounds hostile exponents.
    uint64_t deadline=after(now,after(m_config.retry_timeout_ns,cm_timeout(exponent)));
    return x.overall_deadline?std::min(deadline,x.overall_deadline):deadline;
}
bool NativeCmManager::connect(const CmConnectRequest &r,uint64_t now,uint32_t &id,Status *s) {
    if(!m_configured || !r.operation_id || !r.service_id || (r.ip_cma && !(r.service_id&0xffff)) ||
       r.private_data_length>r.private_data.size())
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid CM connect request");
    Slot *x=allocate(now);if(!x)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"CM connection or event capacity exhausted");
    x->info.operation_id=r.operation_id;x->info.service_id=r.service_id;x->info.port=r.port;
    x->info.endpoint=r.endpoint;x->info.local=r.parameters;x->info.active=true;x->info.state=CmState::REQ_SENT;
    x->overall_deadline=after(now,m_config.handshake_timeout_ns);
    x->transaction_id=m_next_transaction++;CmMessage m=response(*x,CmMessageType::REQ);
    if(r.ip_cma) {
        CmaIpHeader h;h.ipv6=r.endpoint.ipv6;h.source_port=r.source_port;
        std::memcpy(h.source_ip.data(),r.endpoint.src_ip,r.endpoint.ipv6?16:4);
        std::memcpy(h.destination_ip.data(),r.endpoint.dst_ip,r.endpoint.ipv6?16:4);
        if(!NativeCmCodec::encode_ip_private(h,ByteSpan(r.private_data.data(),r.private_data_length),m,s)){*x=Slot();return false;}
    } else if(!copy_private(m,ByteSpan(r.private_data.data(),r.private_data_length),s)){*x=Slot();return false;}
    if(!queue(*x,m,true,s)){*x=Slot();return false;}
    x->request=m;id=x->info.connection_id;status_ok(s);return true;
}
bool NativeCmManager::accept(uint32_t id,const CmParameters &local,ByteSpan data,uint64_t,Status *s) {
    Slot *x=find(id);if(!x || x->info.state!=CmState::REQUEST_RECEIVED)
        return status_error(s,ErrorCode::INVALID_STATE,"accept requires pending CM request");
    CmMessage m=response(*x,CmMessageType::REP);m.parameters=local;
    if(local.initiator_depth>x->info.remote.responder_resources || local.responder_resources>x->info.remote.initiator_depth ||
       local.mtu<x->info.remote.mtu)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"passive CM resource/path parameters cannot satisfy request");
    if(!copy_private(m,data,s))return false;
    uint8_t check[256];if(!NativeCmCodec::encode(m,check,sizeof(check),s))return false;
    /* Remove an unsent MRA before replacing it with REP. */
    m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[id](const CmTxFrame &f){return f.connection_id==id;}),m_tx.end());
    x->awaiting_tx=false;if(!queue(*x,m,true,s))return false;
    x->info.local=local;x->info.local.mtu=x->info.remote.mtu;negotiate(*x);
    x->info.state=CmState::REP_SENT;x->retries=0;x->last_reply=m;status_ok(s);return true;
}
bool NativeCmManager::reject(uint32_t id,uint16_t reason,ByteSpan data,uint64_t now,Status *s) {
    Slot *x=find(id);if(!x || x->info.state!=CmState::REQUEST_RECEIVED)
        return status_error(s,ErrorCode::INVALID_STATE,"reject requires pending CM request");
    CmMessage m=response(*x,CmMessageType::REJ);m.reject_reason=reason;
    if(!copy_private(m,data,s))return false;
    m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[id](const CmTxFrame &f){return f.connection_id==id;}),m_tx.end());
    x->awaiting_tx=false;if(!queue(*x,m,false,s))return false;
    x->last_reply=m;terminal(*x,CmEventType::REJECTED,now,reason,&m);status_ok(s);return true;
}
bool NativeCmManager::confirm(uint32_t id,uint64_t,Status *s) {
    Slot *x=find(id);
    if(!x || x->info.state!=CmState::REP_RECEIVED)
        return status_error(s,ErrorCode::INVALID_STATE,"confirm requires received CM REP and provisioned QP/RQ");
    m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[id](const CmTxFrame &f){return f.connection_id==id;}),m_tx.end());
    x->awaiting_tx=false;
    CmMessage r=response(*x,CmMessageType::RTU);
    if(!queue(*x,r,false,s))return false;
    x->last_reply=r;x->info.state=CmState::ESTABLISHED;status_ok(s);return true;
}
bool NativeCmManager::disconnect(uint32_t id,uint64_t now,Status *s) {
    Slot *x=find(id);if(!x || x->info.state!=CmState::ESTABLISHED || !x->established_event)
        return status_error(s,ErrorCode::INVALID_STATE,"disconnect requires established CM connection");
    if(x->awaiting_tx)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"CM response still queued");
    x->transaction_id=m_next_transaction++;CmMessage m=response(*x,CmMessageType::DREQ);m.remote_qpn=x->info.remote.qpn;
    x->close_transaction_id=x->transaction_id;
    x->overall_deadline=after(now,m_config.handshake_timeout_ns);
    if(!queue(*x,m,true,s))return false;
    x->info.state=CmState::DREQ_SENT;x->retries=0;status_ok(s);return true;
}
bool NativeCmManager::cancel(uint32_t id,uint64_t now,Status *s) {
    Slot *x=find(id);if(!x)return status_error(s,ErrorCode::NOT_FOUND,"CM connection not found");
    m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[id](const CmTxFrame &f){return f.connection_id==id;}),m_tx.end());
    x->awaiting_tx=false;terminal(*x,CmEventType::CANCELLED,now);status_ok(s);return true;
}
bool NativeCmManager::matching_peer(const Slot &x,uint16_t port,const NativeEndpoint &p) const {
    return x.info.port==port && x.info.endpoint.ipv6==p.ipv6 && same_vlan(x.info.endpoint,p) &&
        same_ip(p.ipv6,x.info.endpoint.src_ip,p.dst_ip) && same_ip(p.ipv6,x.info.endpoint.dst_ip,p.src_ip) &&
        !std::memcmp(x.info.endpoint.src_mac,p.dst_mac,6) && !std::memcmp(x.info.endpoint.dst_mac,p.src_mac,6);
}
bool NativeCmManager::receive(uint16_t port,ByteSpan frame,uint64_t now,Status *s) {
    NativeEndpoint path;CmMessage m;if(!NativeCmCodec::decode_frame(frame,path,m,s)){++m_statistics.malformed;return false;}
    ++m_statistics.received;
    if(m.type==CmMessageType::REQ) {
        for(auto &x:m_slots)if(x.info.state!=CmState::FREE && !x.info.active && x.info.remote_id==m.local_id && matching_peer(x,port,path)) {
            if(x.transaction_id!=m.transaction_id || x.request.parameters.qpn!=m.parameters.qpn ||
               x.request.parameters.psn!=m.parameters.psn || x.request.service_id!=m.service_id ||
               x.request.parameters.responder_resources!=m.parameters.responder_resources ||
               x.request.parameters.initiator_depth!=m.parameters.initiator_depth ||
               x.request.parameters.mtu!=m.parameters.mtu || x.request.local_gid!=m.local_gid ||
               x.request.remote_gid!=m.remote_gid || x.request.private_data!=m.private_data)
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"conflicting CM REQ duplicate");
            ++m_statistics.duplicates;
            if(x.info.state==CmState::TIME_WAIT && x.last_reply.type!=CmMessageType::REJ)
                return reject_request(port,path,m,10,s);
            if(x.last_reply.local_id && !queue(x,x.last_reply,x.info.state==CmState::REP_SENT,s))return false;
            status_ok(s);return true;
        }
        const CmListenRequest *listener=NULL;
        for(const auto &l:m_listeners)if(l.service_id==m.service_id && l.port==port && l.endpoint.ipv6==path.ipv6 &&
            same_vlan(l.endpoint,path)&&same_ip(path.ipv6,l.endpoint.src_ip,path.dst_ip) &&
            !std::memcmp(l.endpoint.src_mac,path.dst_mac,6)){listener=&l;break;}
        if(!listener){++m_statistics.unknown;return reject_request(port,path,m,8,s);}
        size_t pending=0;for(const auto &x:m_slots)if(x.info.listener_id==listener->listener_id &&
            (x.info.state==CmState::REQUEST_RECEIVED||x.info.state==CmState::REP_SENT))++pending;
        if(pending>=listener->backlog)return reject_request(port,path,m,3,s);
        std::array<uint8_t,16> sgid,dgid;NativeCmCodec::ip_gid(path.ipv6,path.src_ip,sgid);NativeCmCodec::ip_gid(path.ipv6,path.dst_ip,dgid);
        if(m.local_gid!=sgid || m.remote_gid!=dgid)return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM primary GIDs differ from IP path");
        if(listener->ip_cma) {
            CmaIpHeader h;ByteSpan app;if(!NativeCmCodec::decode_ip_private(m,h,app,s))return false;
            if(h.ipv6!=path.ipv6 || !same_ip(h.ipv6,h.source_ip.data(),path.src_ip)||!same_ip(h.ipv6,h.destination_ip.data(),path.dst_ip))
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"IP CMA private addresses differ from packet path");
        }
        Slot *x=allocate(now);if(!x)return reject_request(port,path,m,3,s);
        x->info.remote_id=m.local_id;x->info.service_id=m.service_id;x->info.port=port;x->info.listener_id=listener->listener_id;
        x->info.remote=m.parameters;x->info.endpoint=reverse_path(path);x->info.state=CmState::REQUEST_RECEIVED;
        x->info.send_psn=m.parameters.psn;x->info.retry_count=m.parameters.retry_count;
        x->info.rnr_retry_count=m.parameters.rnr_retry_count;
        x->info.endpoint.dscp=m.traffic_class>>2;x->info.endpoint.ecn=m.traffic_class&3;
        x->info.endpoint.ttl=m.hop_limit;
        x->transaction_id=m.transaction_id;x->request=m;x->deadline=after(now,m_config.pending_accept_timeout_ns);
        x->overall_deadline=after(now,m_config.handshake_timeout_ns);
        CmMessage reply=response(*x,CmMessageType::MRA);reply.message_mraed=0;reply.service_timeout=20;
        if(!queue(*x,reply,false,s)){*x=Slot();return false;}
        x->deadline=after(now,m_config.pending_accept_timeout_ns);x->last_reply=reply;
        event(*x,CmEventType::CONNECT_REQUEST,&m);status_ok(s);return true;
    }
    Slot *x=find(m.remote_id);
    if(!x || !matching_peer(*x,port,path) || (x->info.remote_id && x->info.remote_id!=m.local_id)) {
        ++m_statistics.unknown;return status_error(s,ErrorCode::NOT_FOUND,"CM response peer or communication IDs do not match");
    }
    if(m.type==CmMessageType::DREP && x->info.state==CmState::TIME_WAIT &&
       m.transaction_id==x->close_transaction_id) {
        ++m_statistics.duplicates;status_ok(s);return true;
    }
    if(m.type!=CmMessageType::DREQ && m.transaction_id!=x->transaction_id)
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM transaction ID mismatch");
    if(m.type==CmMessageType::MRA) {
        if(x->info.state==CmState::ESTABLISHED) {
            ++m_statistics.duplicates;status_ok(s);return true;
        }
        if((x->info.state!=CmState::REQ_SENT || m.message_mraed!=0) &&
           (x->info.state!=CmState::REP_SENT || m.message_mraed!=1))
            return status_error(s,ErrorCode::INVALID_STATE,"unexpected CM MRA");
        x->info.remote_id=m.local_id;x->deadline=response_deadline(*x,now,m.service_timeout);status_ok(s);return true;
    }
    if(m.type==CmMessageType::REP && x->info.active &&
        (x->info.state==CmState::REQ_SENT || x->info.state==CmState::REP_RECEIVED || x->info.state==CmState::ESTABLISHED)) {
        if(x->info.state!=CmState::REQ_SENT) {
            if(x->info.remote.qpn!=m.parameters.qpn||x->info.remote.psn!=m.parameters.psn)
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"conflicting CM REP duplicate");
            ++m_statistics.duplicates;return queue(*x,x->last_reply,false,s);
        }
        if(m.parameters.initiator_depth>x->info.local.responder_resources || m.parameters.responder_resources>x->info.local.initiator_depth)
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM REP exceeds requested Read/Atomic limits");
        x->info.remote_id=m.local_id;x->info.remote=m.parameters;x->info.remote.mtu=x->info.local.mtu;negotiate(*x);
        m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[x](const CmTxFrame &f){return f.connection_id==x->info.connection_id;}),m_tx.end());x->awaiting_tx=false;
        CmMessage r=response(*x,CmMessageType::MRA);r.message_mraed=1;
        if(!queue(*x,r,false,s))return false;
        x->last_reply=r;x->request=m;x->info.state=CmState::REP_RECEIVED;
        x->deadline=after(now,m_config.pending_accept_timeout_ns);
        event(*x,CmEventType::CONNECT_RESPONSE,&m);status_ok(s);return true;
    }
    if(m.type==CmMessageType::RTU && !x->info.active &&
        (x->info.state==CmState::REP_SENT || x->info.state==CmState::ESTABLISHED)) {
        if(x->info.state==CmState::ESTABLISHED){++m_statistics.duplicates;status_ok(s);return true;}
        x->info.state=CmState::ESTABLISHED;x->deadline=0;
        m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[x](const CmTxFrame &f){return f.connection_id==x->info.connection_id;}),m_tx.end());x->awaiting_tx=false;
        if(!x->established_event){event(*x,CmEventType::ESTABLISHED,&m);x->established_event=true;++m_statistics.established;}
        status_ok(s);return true;
    }
    if(m.type==CmMessageType::REJ && (x->info.state==CmState::REQ_SENT || x->info.state==CmState::REP_SENT || x->info.state==CmState::REP_RECEIVED)) {
        x->info.remote_id=m.local_id;terminal(*x,CmEventType::REJECTED,now,m.reject_reason,&m);status_ok(s);return true;
    }
    if(m.type==CmMessageType::DREQ && (x->info.state==CmState::ESTABLISHED||x->info.state==CmState::DREQ_SENT||x->info.state==CmState::TIME_WAIT)) {
        if(m.remote_qpn!=x->info.local.qpn)return status_error(s,ErrorCode::PROTOCOL_ERROR,"CM DREQ target QPN mismatch");
        if(x->info.state==CmState::TIME_WAIT) {
            if(x->last_reply.type!=CmMessageType::DREP || x->last_reply.transaction_id!=m.transaction_id)
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"stale DREQ in timewait");
            ++m_statistics.duplicates;return queue(*x,x->last_reply,false,s);
        }
        m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[x](const CmTxFrame &f){return f.connection_id==x->info.connection_id;}),m_tx.end());x->awaiting_tx=false;
        x->transaction_id=m.transaction_id;CmMessage r=response(*x,CmMessageType::DREP);
        if(!queue(*x,r,false,s))return false;
        x->last_reply=r;
        terminal(*x,CmEventType::DISCONNECTED,now,0,&m);status_ok(s);return true;
    }
    if(m.type==CmMessageType::DREP && x->info.state==CmState::DREQ_SENT) {
        terminal(*x,CmEventType::DISCONNECTED,now,0,&m);status_ok(s);return true;
    }
    return status_error(s,ErrorCode::INVALID_STATE,"CM message not legal in current state");
}
void NativeCmManager::poll(uint64_t now) {
    for(auto &x:m_slots) {
        if(x.info.state==CmState::FREE)continue;
        if(x.info.state==CmState::TIME_WAIT) {
            if(now>=x.timewait_deadline) {
                m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[&x](const CmTxFrame &f){return f.connection_id==x.info.connection_id;}),m_tx.end());
                x=Slot();
            }
            continue;
        }
        if((x.info.state!=CmState::ESTABLISHED || !x.established_event) &&
           x.overall_deadline && now>=x.overall_deadline) {
            terminal(x,CmEventType::TIMEOUT,now);continue;
        }
        if(!x.deadline || now<x.deadline)continue;
        if(x.info.state==CmState::REQUEST_RECEIVED || x.info.state==CmState::REP_RECEIVED){terminal(x,CmEventType::TIMEOUT,now);continue;}
        if(x.info.state!=CmState::REQ_SENT && x.info.state!=CmState::REP_SENT && x.info.state!=CmState::DREQ_SENT)continue;
        const uint8_t negotiated_retries=x.info.active?x.info.local.max_cm_retries:x.info.remote.max_cm_retries;
        if(x.retries>=std::min<uint32_t>(m_config.max_retries,negotiated_retries)) {
            terminal(x,CmEventType::TIMEOUT,now);continue;
        }
        if(!x.awaiting_tx && queue(x,x.saved,true,NULL)){++x.retries;++m_statistics.retries;}
    }
}
bool NativeCmManager::peek_tx(CmTxFrame &f) const {if(m_tx.empty())return false;f=m_tx.front();return true;}
bool NativeCmManager::complete_tx(uint64_t token,bool accepted,uint64_t now,Status *s) {
    if(m_tx.empty()||m_tx.front().token!=token)return status_error(s,ErrorCode::STALE_GENERATION,"CM TX token not current");
    if(!accepted){status_ok(s);return true;}
    Slot *x=find(m_tx.front().connection_id);m_tx.erase(m_tx.begin());++m_statistics.transmitted;
    if(x) {
        x->awaiting_tx=false;
        if(x->retry_message && x->info.state!=CmState::TIME_WAIT) {
            uint8_t timeout=x->info.active?x->info.local.response_timeout:x->info.remote.local_response_timeout;
            x->deadline=response_deadline(*x,now,timeout);
        }
        if(x->info.active && x->info.state==CmState::ESTABLISHED && !x->established_event) {
            event(*x,CmEventType::ESTABLISHED,&x->request);x->established_event=true;++m_statistics.established;
        }
    }
    status_ok(s);return true;
}
bool NativeCmManager::pop_event(CmEvent &e){if(m_events.empty())return false;e=m_events.front();m_events.erase(m_events.begin());return true;}
bool NativeCmManager::connection(uint32_t id,CmConnectionInfo &i) const {const Slot *x=find(id);if(!x)return false;i=x->info;return true;}
const CmStatistics &NativeCmManager::statistics() const {return m_statistics;}
size_t NativeCmManager::live_connections() const {size_t n=0;for(const auto &x:m_slots)if(x.info.state!=CmState::FREE)++n;return n;}
const char *cm_state_to_string(CmState s) {
    switch(s) {case CmState::FREE:return "free";case CmState::REQ_SENT:return "req_sent";
    case CmState::REQUEST_RECEIVED:return "request_received";case CmState::REP_SENT:return "rep_sent";
    case CmState::REP_RECEIVED:return "rep_received";
    case CmState::ESTABLISHED:return "established";case CmState::DREQ_SENT:return "dreq_sent";
    case CmState::TIME_WAIT:return "time_wait";case CmState::REJECTED:return "rejected";case CmState::FAILED:return "failed";}
    return "unknown";
}
const char *cm_event_to_string(CmEventType t) {
    switch(t) {case CmEventType::CONNECT_REQUEST:return "connect_request";case CmEventType::CONNECT_RESPONSE:return "connect_response";
    case CmEventType::ESTABLISHED:return "established";
    case CmEventType::DISCONNECTED:return "disconnected";case CmEventType::REJECTED:return "rejected";
    case CmEventType::TIMEOUT:return "timeout";case CmEventType::CANCELLED:return "cancelled";}return "unknown";
}
} }
