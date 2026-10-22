/* Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License,
 * Version 2.0. See http://www.apache.org/licenses/LICENSE-2.0 */
#include "trex_rocev2_native_neighbor.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace TrexRoceV2 { namespace Native {
namespace {
uint16_t nget16(const uint8_t *p){return (uint16_t(p[0])<<8)|p[1];}
void nput16(uint8_t *p,uint16_t v){p[0]=v>>8;p[1]=v;}
uint64_t later(uint64_t n,uint64_t d){return d>std::numeric_limits<uint64_t>::max()-n?std::numeric_limits<uint64_t>::max():n+d;}
bool zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;++i)if(p[i])return false;return true;}
bool vlan_equal(const NativeEndpoint &a,const NativeEndpoint &b){return a.has_vlan==b.has_vlan&&(!a.has_vlan||a.vlan==b.vlan);}
bool ip_equal(const NativeEndpoint &a,const uint8_t *x,const uint8_t *y){return !std::memcmp(x,y,a.ipv6?16:4);}
bool on_link(const uint8_t *a,const uint8_t *b,unsigned prefix){
    for(unsigned i=0;i<prefix/8;++i)if(a[i]!=b[i])return false;
    unsigned rest=prefix%8;return !rest || ((a[prefix/8]^b[prefix/8])&(0xff<<(8-rest)))==0;
}
uint32_t sum16(uint32_t sum,const uint8_t *p,size_t n){
    for(size_t i=0;i+1<n;i+=2)sum+=nget16(p+i);
    if(n&1)sum+=uint16_t(p[n-1])<<8;
    return sum;
}
uint16_t icmp6_checksum(const uint8_t *ip,const uint8_t *icmp,size_t length){
    uint32_t sum=sum16(0,ip+8,32);sum+=length>>16;sum+=length&0xffff;sum+=58;
    sum=sum16(sum,icmp,length);while(sum>>16)sum=(sum&0xffff)+(sum>>16);return ~sum;
}
size_t eth_header(uint8_t *out,const NativeEndpoint &p,const uint8_t *dst,uint16_t type){
    std::memcpy(out,dst,6);std::memcpy(out+6,p.src_mac,6);
    if(p.has_vlan){nput16(out+12,0x8100);nput16(out+14,(uint16_t(p.pcp)<<13)|(p.vlan_dei?0x1000:0)|p.vlan);nput16(out+16,type);return 18;}
    nput16(out+12,type);return 14;
}
size_t ipv6_header(uint8_t *out,const uint8_t *src,const uint8_t *dst,size_t payload){
    std::memset(out,0,40);out[0]=0x60;nput16(out+4,payload);out[6]=58;out[7]=255;
    std::memcpy(out+8,src,16);std::memcpy(out+24,dst,16);return 40;
}
}
NativeNeighborConfig::NativeNeighborConfig():max_interfaces(256),max_entries(512),max_events(1024),
    max_tx_frames(512),retry_timeout_ns(1000000000),cache_lifetime_ns(30000000000ULL),max_retries(3) {}
NativeNeighborInterface::NativeNeighborInterface():interface_id(0),port(0),prefix_length(0),has_gateway(false){gateway.fill(0);}
NativeNeighborEvent::NativeNeighborEvent():operation_id(0),type(NeighborEventType::TIMEOUT),port(0) {}
NativeNeighborManager::Entry::Entry():operation_id(0),interface_id(0),deadline(0),expires(0),port(0),retries(0),
    used(false),pending(false),awaiting_tx(false){next_hop.fill(0);}
NativeNeighborManager::NativeNeighborManager():m_configured(false),m_next_token(1) {}
bool NativeNeighborManager::configure(const NativeNeighborConfig &c,Status *s){
    if(!c.max_interfaces||c.max_interfaces>65536||!c.max_entries||c.max_entries>65536||
       c.max_events<c.max_entries||c.max_events>1048576||!c.max_tx_frames||c.max_tx_frames>65536||
       !c.retry_timeout_ns||!c.cache_lifetime_ns||c.max_retries>15)
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid bounded neighbor resources");
    for(const auto &e:m_entries)if(e.pending)return status_error(s,ErrorCode::INVALID_STATE,"neighbor resolutions still pending");
    m_config=c;m_interfaces.clear();m_entries.assign(c.max_entries,Entry());m_events.clear();m_tx.clear();
    m_interfaces.reserve(c.max_interfaces);m_events.reserve(c.max_events);m_tx.reserve(c.max_tx_frames);
    m_configured=true;status_ok(s);return true;
}
bool NativeNeighborManager::register_interface(const NativeNeighborInterface &i,Status *s){
    if(!m_configured||!i.interface_id||i.prefix_length>(i.endpoint.ipv6?128:32)||
       zero(i.endpoint.src_mac,6)||(i.endpoint.src_mac[0]&1)||zero(i.endpoint.src_ip,i.endpoint.ipv6?16:4)||
       (i.endpoint.has_vlan&&i.endpoint.vlan>4094)||
       (i.has_gateway&&(zero(i.gateway.data(),i.endpoint.ipv6?16:4)||
        (!on_link(i.endpoint.src_ip,i.gateway.data(),i.prefix_length)&&
         !(i.endpoint.ipv6&&i.gateway[0]==0xfe&&(i.gateway[1]&0xc0)==0x80)))))
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"invalid owned neighbor interface/address/route");
    for(const auto &x:m_interfaces)if(x.interface_id==i.interface_id||
       (x.port==i.port&&x.endpoint.ipv6==i.endpoint.ipv6&&vlan_equal(x.endpoint,i.endpoint)&&ip_equal(i.endpoint,x.endpoint.src_ip,i.endpoint.src_ip)))
        return status_error(s,ErrorCode::DUPLICATE,"neighbor address already owned");
    if(m_interfaces.size()==m_config.max_interfaces)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"neighbor interface bound reached");
    m_interfaces.push_back(i);status_ok(s);return true;
}
bool NativeNeighborManager::unregister_interface(uint64_t id,Status *s){
    for(size_t i=0;i<m_interfaces.size();++i)if(m_interfaces[i].interface_id==id){
        const NativeNeighborInterface removed=m_interfaces[i];
        for(auto &e:m_entries)if(e.used&&e.interface_id==id){if(e.pending)finish(e,NeighborEventType::CANCELLED,0);e=Entry();}
        m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[&removed](const CmTxFrame &f){
            if(f.port!=removed.port || f.length<14 || std::memcmp(f.data.data()+6,removed.endpoint.src_mac,6))return false;
            bool vlan=nget16(f.data.data()+12)==0x8100;
            if(vlan!=removed.endpoint.has_vlan || (vlan &&
               (f.length<18 || (nget16(f.data.data()+14)&4095)!=removed.endpoint.vlan)))return false;
            size_t l2=vlan?18:14;
            size_t source=l2+(removed.endpoint.ipv6?8:14);
            size_t width=removed.endpoint.ipv6?16:4;
            return f.length>=source+width && !std::memcmp(f.data.data()+source,removed.endpoint.src_ip,width);
        }),m_tx.end());
        m_interfaces.erase(m_interfaces.begin()+i);status_ok(s);return true;
    }
    return status_error(s,ErrorCode::NOT_FOUND,"neighbor interface not found");
}
const NativeNeighborInterface *NativeNeighborManager::interface(uint16_t port,const NativeEndpoint &p)const{
    for(const auto &i:m_interfaces)if(i.port==port&&i.endpoint.ipv6==p.ipv6&&vlan_equal(i.endpoint,p)&&ip_equal(p,i.endpoint.src_ip,p.src_ip))return &i;
    return NULL;
}
bool NativeNeighborManager::queue_reply(uint16_t port,const uint8_t *data,size_t len,Status *s){
    if(m_tx.size()==m_config.max_tx_frames||!m_next_token)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"neighbor TX queue full");
    CmTxFrame f;f.token=m_next_token++;f.port=port;f.length=len;std::memcpy(f.data.data(),data,len);m_tx.push_back(f);return true;
}
bool NativeNeighborManager::queue_probe(Entry &e,Status *s){
    if(e.awaiting_tx)return true;
    uint8_t bytes[128]={0};size_t n=0;
    if(!e.path.ipv6){
        const uint8_t broadcast[6]={255,255,255,255,255,255};n=eth_header(bytes,e.path,broadcast,0x0806);
        uint8_t *arp=bytes+n;nput16(arp,1);nput16(arp+2,0x0800);arp[4]=6;arp[5]=4;nput16(arp+6,1);
        std::memcpy(arp+8,e.path.src_mac,6);std::memcpy(arp+14,e.path.src_ip,4);std::memcpy(arp+24,e.next_hop.data(),4);n+=28;
        n=std::max<size_t>(n,60);
    }else{
        uint8_t dip[16]={0xff,0x02,0,0,0,0,0,0,0,0,0,1,0xff,0,0,0};
        std::memcpy(dip+13,e.next_hop.data()+13,3);uint8_t dmac[6]={0x33,0x33,0xff,0,0,0};std::memcpy(dmac+3,dip+13,3);
        n=eth_header(bytes,e.path,dmac,0x86dd);uint8_t *ip=bytes+n;n+=ipv6_header(ip,e.path.src_ip,dip,32);
        uint8_t *icmp=bytes+n;icmp[0]=135;std::memcpy(icmp+8,e.next_hop.data(),16);icmp[24]=1;icmp[25]=1;
        std::memcpy(icmp+26,e.path.src_mac,6);nput16(icmp+2,icmp6_checksum(ip,icmp,32));n+=32;
    }
    if(!queue_reply(e.port,bytes,n,s))return false;
    // operation_id is 64-bit; TX connection_id stores stable entry index+1.
    m_tx.back().connection_id=uint32_t(&e-m_entries.data())+1;e.awaiting_tx=true;e.deadline=0;return true;
}
void NativeNeighborManager::finish(Entry &e,NeighborEventType type,uint64_t now,const uint8_t *mac){
    NativeNeighborEvent event;event.operation_id=e.operation_id;event.type=type;event.port=e.port;event.endpoint=e.path;
    if(mac){std::memcpy(e.path.dst_mac,mac,6);std::memcpy(event.endpoint.dst_mac,mac,6);event.endpoint.has_next_hop=true;}
    // Every pending entry reserves one event slot in resolve().
    m_events.push_back(event);e.pending=false;e.deadline=0;e.awaiting_tx=false;
    const uint32_t slot=uint32_t(&e-m_entries.data())+1;
    m_tx.erase(std::remove_if(m_tx.begin(),m_tx.end(),[slot](const CmTxFrame &f){return f.connection_id==slot;}),m_tx.end());
    if(type==NeighborEventType::RESOLVED)e.expires=later(now,m_config.cache_lifetime_ns);else e.used=false;
}
bool NativeNeighborManager::resolve(uint64_t op,uint16_t port,const NativeEndpoint &p,uint64_t now,Status *s){
    const NativeNeighborInterface *i=interface(port,p);
    if(!m_configured||!op||!i||std::memcmp(p.src_mac,i->endpoint.src_mac,6))
        return status_error(s,ErrorCode::INVALID_ARGUMENT,"neighbor resolution requires owned local IP/MAC interface");
    size_t reserve=m_events.size()+1;for(const auto &e:m_entries)if(e.pending){++reserve;if(e.operation_id==op)return status_error(s,ErrorCode::DUPLICATE,"neighbor operation ID already active");}
    if(reserve>m_config.max_events)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"neighbor event capacity exhausted");
    std::array<uint8_t,16> hop;hop.fill(0);bool direct=on_link(p.src_ip,p.dst_ip,i->prefix_length);
    if(!direct&&!i->has_gateway&&zero(p.dst_mac,6))return status_error(s,ErrorCode::NOT_FOUND,"no on-link or gateway route for neighbor resolution");
    std::memcpy(hop.data(),direct?p.dst_ip:i->gateway.data(),p.ipv6?16:4);
    if(!zero(p.dst_mac,6)){
        if(p.dst_mac[0]&1)return status_error(s,ErrorCode::INVALID_ARGUMENT,"static next-hop MAC must be unicast");
        NativeNeighborEvent event;event.operation_id=op;event.type=NeighborEventType::RESOLVED;event.port=port;event.endpoint=p;
        event.endpoint.has_next_hop=true;m_events.push_back(event);status_ok(s);return true;
    }
    for(const auto &x:m_entries)if(x.used&&!x.pending&&now<x.expires&&x.interface_id==i->interface_id&&x.next_hop==hop){
        NativeNeighborEvent event;event.operation_id=op;event.type=NeighborEventType::RESOLVED;event.port=port;event.endpoint=p;
        std::memcpy(event.endpoint.dst_mac,x.path.dst_mac,6);event.endpoint.has_next_hop=true;m_events.push_back(event);status_ok(s);return true;
    }
    Entry *entry=NULL;for(auto &x:m_entries)if(!x.used||(!x.pending&&now>=x.expires)){entry=&x;break;}
    if(!entry)return status_error(s,ErrorCode::RESOURCE_EXHAUSTED,"neighbor cache/pending bound reached");
    *entry=Entry();entry->used=true;entry->pending=true;entry->operation_id=op;entry->interface_id=i->interface_id;
    entry->port=port;entry->path=p;entry->next_hop=hop;
    if(!queue_probe(*entry,s)){*entry=Entry();return false;}status_ok(s);return true;
}
bool NativeNeighborManager::cancel(uint64_t operation_id,uint64_t now,Status *status) {
    for (auto &entry : m_entries) {
        if (entry.pending && entry.operation_id == operation_id) {
            finish(entry, NeighborEventType::CANCELLED, now);
            status_ok(status);
            return true;
        }
    }
    return status_error(status, ErrorCode::NOT_FOUND, "pending neighbor operation not found");
}
bool NativeNeighborManager::receive(uint16_t port,ByteSpan frame,uint64_t now,Status *s){
    if(!frame.data||frame.size<14)return status_error(s,ErrorCode::PROTOCOL_ERROR,"truncated neighbor Ethernet frame");
    const uint8_t *bytes=frame.data;NativeEndpoint p;size_t offset=14;uint16_t type=nget16(bytes+12);
    if(type==0x8100||type==0x88a8){
        if(frame.size<18)return status_error(s,ErrorCode::PROTOCOL_ERROR,"truncated neighbor VLAN");
        p.has_vlan=true;p.vlan=nget16(bytes+14)&4095;type=nget16(bytes+16);offset=18;
    }
    if((bytes[6]&1)||zero(bytes+6,6))return status_error(s,ErrorCode::PROTOCOL_ERROR,"neighbor source MAC is not unicast");
    if(type==0x0806){
        if(frame.size<offset+28)return status_error(s,ErrorCode::PROTOCOL_ERROR,"truncated ARP");
        const uint8_t *arp=bytes+offset;
        if(nget16(arp)!=1||nget16(arp+2)!=0x0800||arp[4]!=6||arp[5]!=4||
           (nget16(arp+6)!=1&&nget16(arp+6)!=2)||std::memcmp(arp+8,bytes+6,6))
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid Ethernet/IPv4 ARP");
        p.ipv6=false;std::memcpy(p.src_ip,arp+24,4);const auto *i=interface(port,p);
        if(!i)return status_error(s,ErrorCode::NOT_FOUND,"ARP target not locally owned on port/VLAN");
        if(nget16(arp+6)==1){
            const uint8_t broadcast[6]={255,255,255,255,255,255};
            if(std::memcmp(bytes,i->endpoint.src_mac,6)&&std::memcmp(bytes,broadcast,6))
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"ARP request Ethernet destination is not local/broadcast");
            uint8_t reply[64]={0};size_t n=eth_header(reply,i->endpoint,bytes+6,0x0806);uint8_t *a=reply+n;
            nput16(a,1);nput16(a+2,0x0800);a[4]=6;a[5]=4;nput16(a+6,2);
            std::memcpy(a+8,i->endpoint.src_mac,6);std::memcpy(a+14,i->endpoint.src_ip,4);
            std::memcpy(a+18,arp+8,6);std::memcpy(a+24,arp+14,4);
            if(!queue_reply(port,reply,std::max<size_t>(60,n+28),s))return false;
        }else{
            if(std::memcmp(bytes,i->endpoint.src_mac,6)||std::memcmp(arp+18,i->endpoint.src_mac,6))
                return status_error(s,ErrorCode::PROTOCOL_ERROR,"ARP reply destination MAC differs from local owner");
            for(auto &e:m_entries)if(e.pending&&e.interface_id==i->interface_id&&!std::memcmp(e.next_hop.data(),arp+14,4))finish(e,NeighborEventType::RESOLVED,now,arp+8);
        }
        status_ok(s);return true;
    }
    if(type!=0x86dd||frame.size<offset+40)return status_error(s,ErrorCode::UNSUPPORTED,"not ARP/IPv6 ND");
    const uint8_t *ip=bytes+offset;size_t payload=nget16(ip+4);
    if((ip[0]>>4)!=6||ip[6]!=58||ip[7]!=255||payload<24||frame.size<offset+40+payload)
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid ND IPv6 header/length/hop limit");
    const uint8_t *icmp=ip+40;
    if((icmp[0]!=135&&icmp[0]!=136)||icmp[1]||icmp6_checksum(ip,icmp,payload)||icmp[8]==0xff||ip[8]==0xff||
       (icmp[0]==135&&!zero(icmp+4,4)) || (icmp[0]==136&&((icmp[4]&0x1f)||!zero(icmp+5,3))))
        return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid ICMPv6 ND message/checksum/target");
    p.ipv6=true;
    const uint8_t *option_mac=NULL;
    for(size_t o=24;o<payload;){
        if(o+2>payload||!icmp[o+1]||o+size_t(icmp[o+1])*8>payload)return status_error(s,ErrorCode::PROTOCOL_ERROR,"malformed ND option");
        if((icmp[o]==1&&icmp[0]==135)||(icmp[o]==2&&icmp[0]==136)){
            if(icmp[o+1]!=1||option_mac)return status_error(s,ErrorCode::PROTOCOL_ERROR,"invalid/duplicate ND link-layer option");
            option_mac=icmp+o+2;if(std::memcmp(option_mac,bytes+6,6))return status_error(s,ErrorCode::PROTOCOL_ERROR,"ND link-layer option differs from Ethernet source");
        }
        o+=size_t(icmp[o+1])*8;
    }
    if(icmp[0]==135){
        std::memcpy(p.src_ip,icmp+8,16);const auto *i=interface(port,p);
        if(!i)return status_error(s,ErrorCode::NOT_FOUND,"NS target not locally owned on port/VLAN");
        bool dad=zero(ip+8,16);
        if(dad&&option_mac)return status_error(s,ErrorCode::PROTOCOL_ERROR,"DAD NS contains source link-layer option");
        uint8_t solicited[16]={0xff,2,0,0,0,0,0,0,0,0,0,1,0xff,0,0,0};
        std::memcpy(solicited+13,icmp+21,3);
        if((dad||std::memcmp(ip+24,i->endpoint.src_ip,16))&&std::memcmp(ip+24,solicited,16))
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"NS destination does not match local/solicited-node address");
        const uint8_t allnodes[16]={0xff,2,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
        const uint8_t allmac[6]={0x33,0x33,0,0,0,1};uint8_t reply[128]={0};
        size_t n=eth_header(reply,i->endpoint,dad?allmac:bytes+6,0x86dd);uint8_t *rip=reply+n;
        n+=ipv6_header(rip,i->endpoint.src_ip,dad?allnodes:ip+8,32);uint8_t *na=reply+n;
        na[0]=136;na[4]=dad?0x20:0x60;std::memcpy(na+8,i->endpoint.src_ip,16);na[24]=2;na[25]=1;
        std::memcpy(na+26,i->endpoint.src_mac,6);nput16(na+2,icmp6_checksum(rip,na,32));n+=32;
        if(!queue_reply(port,reply,n,s))return false;
    }else{
        if(!option_mac||zero(ip+8,16)||!(icmp[4]&0x40)||ip[24]==0xff)
            return status_error(s,ErrorCode::PROTOCOL_ERROR,"neighbor resolution requires solicited unicast NA with target MAC");
        std::memcpy(p.src_ip,ip+24,16);const auto *i=interface(port,p);
        if(!i||std::memcmp(bytes,i->endpoint.src_mac,6))return status_error(s,ErrorCode::NOT_FOUND,"NA not addressed to owned local interface");
        for(auto &e:m_entries)if(e.pending&&e.interface_id==i->interface_id&&!std::memcmp(e.next_hop.data(),icmp+8,16))finish(e,NeighborEventType::RESOLVED,now,option_mac);
    }
    status_ok(s);return true;
}
void NativeNeighborManager::poll(uint64_t now){
    for(auto &e:m_entries)if(e.pending&&!e.awaiting_tx&&e.deadline&&now>=e.deadline){
        if(e.retries>=m_config.max_retries)finish(e,NeighborEventType::TIMEOUT,now);
        else if(queue_probe(e,NULL))++e.retries;
    }
}
bool NativeNeighborManager::pop_event(NativeNeighborEvent &e){if(m_events.empty())return false;e=m_events.front();m_events.erase(m_events.begin());return true;}
bool NativeNeighborManager::peek_tx(CmTxFrame &f)const{if(m_tx.empty())return false;f=m_tx.front();return true;}
bool NativeNeighborManager::complete_tx(uint64_t token,bool accepted,uint64_t now,Status *s){
    if(m_tx.empty()||m_tx.front().token!=token)return status_error(s,ErrorCode::STALE_GENERATION,"neighbor TX token not current");
    if(!accepted){status_ok(s);return true;}uint32_t slot=m_tx.front().connection_id;m_tx.erase(m_tx.begin());
    if(slot&&slot<=m_entries.size()){Entry &e=m_entries[slot-1];if(e.pending){e.awaiting_tx=false;e.deadline=later(now,m_config.retry_timeout_ns);}}
    status_ok(s);return true;
}
} }
