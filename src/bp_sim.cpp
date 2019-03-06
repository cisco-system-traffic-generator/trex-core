/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#include "bp_sim.h"
#include "stateful_rx_core.h"
#include "utl_json.h"
#include "utl_yaml.h"
#include "msg_manager.h"
#include "trex_watchdog.h"
#include "utl_ipg_bucket.h"
#include "stt_cp.h"

#include <common/utl_gcc_diag.h>

#include <common/basic_utils.h>

#include "trex_stx.h"
#include "utl_mbuf.h"

/* stateless includes */
#include "stl/trex_stl_stream_node.h"
#include "stl/trex_stl_messaging.h"


#undef VALG

#ifdef VALG
#include <valgrind/callgrind.h>
#endif

CPluginCallback * CPluginCallback::callback;


uint32_t getDualPortId(uint32_t thread_id){
    return  ( thread_id % (CGlobalInfo::m_options.get_expected_dual_ports()) );
}




void CFlowYamlInfo::Dump(FILE *fd){
    fprintf(fd,"name        : %s \n",m_name.c_str());
    fprintf(fd,"cps         : %f \n",m_k_cps);
    fprintf(fd,"ipg         : %f \n",m_ipg_sec);
    fprintf(fd,"rtt         : %f \n",m_rtt_sec);
    fprintf(fd,"w           : %d \n",m_w);
    fprintf(fd,"limit       : %d \n",m_limit);
    fprintf(fd,"cap_mode    : %d \n",m_cap_mode?1:0);
    fprintf(fd,"plugin_id   : %d \n",m_plugin_id);
    fprintf(fd,"one_server  : %d \n",m_one_app_server?1:0);
    fprintf(fd,"one_server_was_set  : %d \n",m_one_app_server_was_set?1:0);
    if (m_dpPkt) {
        m_dpPkt->Dump(fd);
    }
}



void CFlowGenStats::clear(){
   m_nat_lookup_no_flow_id=0;
   m_total_bytes=0;
   m_total_pkt=0;
   m_total_open_flows =0;
   m_total_close_flows =0;
   m_nat_lookup_no_flow_id=0;
   m_nat_lookup_remove_flow_id=0;
   m_nat_lookup_wait_ack_state = 0;
   m_nat_lookup_add_flow_id=0;
   m_nat_flow_timeout=0;
   m_nat_flow_timeout_wait_ack = 0;
   m_nat_flow_learn_error=0;
}

void CFlowGenStats::dump(FILE *fd){
    std::string s_bytes=double_to_human_str((double )(m_total_bytes),
                                    "bytes",
                                    KBYE_1024);

    std::string s_pkt=double_to_human_str((double )(m_total_pkt),
                                    "pkt",
                                    KBYE_1000);

    std::string s_flows=double_to_human_str((double )(m_total_open_flows),
                                    "flows",
                                    KBYE_1000);

   DP_S(m_total_bytes,s_bytes);
   DP_S(m_total_pkt,s_pkt);
   DP_S(m_total_open_flows,s_flows);
   DP(m_total_pkt);
   DP(m_total_open_flows);
   DP(m_total_close_flows);
   DP_name("active",(m_total_open_flows-m_total_close_flows));
   DP(m_total_bytes);
   DP(m_nat_lookup_no_flow_id);

   DP(m_nat_lookup_no_flow_id);
   DP(m_nat_lookup_remove_flow_id);
   DP(m_nat_lookup_wait_ack_state);
   DP(m_nat_lookup_add_flow_id);
   DP(m_nat_flow_timeout);
   DP(m_nat_flow_timeout_wait_ack);
   DP_name("active_nat",(m_nat_lookup_add_flow_id-m_nat_lookup_remove_flow_id));
   DP_name("active_nat_wait_syn", (m_nat_lookup_add_flow_id - m_nat_lookup_wait_ack_state));
   DP(m_nat_flow_learn_error);
}



int CErfIF::open_file(std::string file_name){
    BP_ASSERT(m_writer==0);

    if ( m_preview_mode->getFileWrite() ){
        capture_type_e file_type=ERF;
        if ( m_preview_mode->get_pcap_mode_enable() ){
            file_type=LIBPCAP;
        }
        m_writer = CCapWriterFactory::CreateWriter(file_type,(char *)file_name.c_str());
        if (m_writer == NULL) {
            fprintf(stderr,"ERROR can't create cap file %s ",(char *)file_name.c_str());
            return (-1);
        }
    }
    m_raw = new CCapPktRaw();
    return (0);
}


int CErfIF::write_pkt(CCapPktRaw *pkt_raw){

    BP_ASSERT(m_writer);

    if ( m_preview_mode->getFileWrite() ){
        BP_ASSERT(m_writer);
        bool res=m_writer->write_packet(pkt_raw);
        if (res != true) {
            fprintf(stderr,"ERROR can't write to cap file");
            return (-1);
        }
    }
    return (0);
}


int CErfIF::close_file(void){

    if (m_raw) {
        delete m_raw;
        m_raw = NULL;
    }

    if ( m_preview_mode->getFileWrite() ){
        if (m_writer) {
            delete m_writer;
            m_writer = NULL;
        }
    }

    return (0);
}



void CFlowKey::Clean(){
    m_ipaddr1=0;
    m_ipaddr2=0;
    m_port1=0;
    m_port2=0;
    m_ip_proto=0;
    m_l2_proto=0;
    m_vrfid=0;
}

void CFlowKey::Dump(FILE *fd){
    fprintf(fd," %x:%x:%x:%x:%x:%x:%x\n",m_ipaddr1,m_ipaddr2,m_port1,m_port2,m_ip_proto,m_l2_proto,m_vrfid);
}



void CPacketDescriptor::Dump(FILE *fd){
    fprintf(fd," IsSwapTuple : %d \n",IsSwapTuple()?1:0);
    fprintf(fd," IsSInitDir  : %d \n",IsInitSide()?1:0);
    fprintf(fd," Isvalid : %d ",IsValidPkt()?1:0);
    fprintf(fd," IsRtt   : %d ",IsRtt()?1:0);
    fprintf(fd," IsLearn   : %d ",IsLearn()?1:0);

    if (IsTcp() ) {
        fprintf(fd," TCP ");
    }else{
        fprintf(fd," UDP ");
    }
    fprintf(fd," IsLast Pkt   : %d ", IsLastPkt() ?1:0);
    fprintf(fd," id           : %d \n",getId() );

    fprintf(fd," flow_ID      : %d , max_pkts : %u, max_aging: %d sec , pkt_id : %u, init: %d   ( dir:%d  dir_max :%d  ) bid:%d \n",getFlowId(),
            GetMaxPktsPerFlow(),
            GetMaxFlowTimeout() ,
            getFlowPktNum(),
            IsInitSide(),
            GetDirInfo()->GetPktNum(),
            GetDirInfo()->GetMaxPkts(),
            IsBiDirectionalFlow()?1:0

            );
    fprintf(fd,"\n");
}


void CPacketIndication::UpdateMbufSize(){

    uint8_t offset = m_udp_tcp_offset;
    assert(offset>0);

    if ( m_desc.IsTcp() ) {
        offset+=20; 
    }else{
        if ( m_desc.IsUdp() ) {
            offset+=8;
        }else{
            /* ICMP only ports */
            offset+=8;
        }
    }

    if (offset<=64) {
        m_rw_mbuf_size=64;
    }else{
        if (offset<=128) {
            m_rw_mbuf_size=128;
        }else{
            printf(" ERROR packet r/w is more than 128 bytes. it is not supported \n");
            exit(1);
        }
    }

    uint16_t pkt_len = m_packet->getTotalLen();

    if ( pkt_len > m_rw_mbuf_size ) {
        m_ro_mbuf_size = pkt_len - m_rw_mbuf_size;
    }else{
        m_ro_mbuf_size=0;
    }
}

void CPacketIndication::UpdateOffsets(){
    m_ether_offset   = getEtherOffset();
    m_ip_offset      = getIpOffset();
    m_udp_tcp_offset = getTcpOffset();
    m_payload_offset = getPayloadOffset();
    UpdateMbufSize();
}

void CPacketIndication::UpdatePacketPadding(){
    m_packet_padding = m_packet->getTotalLen() - (l3.m_ipv4->getTotalLength()+ getIpOffset());
}


void CPacketIndication::RefreshPointers(){

    char *pobase=getBasePtr();

    m_ether = (EthernetHeader *) (pobase + m_ether_offset);
    l3.m_ipv4  = (IPHeader       *) (pobase + m_ip_offset);
    l4.m_tcp=  (TCPHeader *)(pobase + m_udp_tcp_offset);
    if ( m_payload_offset ){
        m_payload =(uint8_t *)(pobase + m_payload_offset);
    }else{
        m_payload =(uint8_t *)(0);
    }
}

// copy ref assume pkt point to a fresh
void CPacketIndication::Clone(CPacketIndication * obj,CCapPktRaw * pkt){
    Clean();
    m_cap_ipg = obj->m_cap_ipg;
    m_packet  = pkt;
    char *pobase=getBasePtr();
    m_flow = obj->m_flow;

    m_ether = (EthernetHeader *) (pobase + obj->getEtherOffset());
    l3.m_ipv4  = (IPHeader       *) (pobase + obj->getIpOffset());
    m_is_ipv6 = obj->m_is_ipv6;
    m_is_ipv6_converted = obj->m_is_ipv6_converted;

    l4.m_tcp=  (TCPHeader *)(pobase + obj->getTcpOffset());
    if ( obj->getPayloadOffset() ){
        m_payload =(uint8_t *)(pobase + obj->getPayloadOffset());
    }else{
        m_payload =(uint8_t *)(0);
    }
    m_payload_len = obj->m_payload_len;
    m_flow_key    = obj->m_flow_key;
    m_desc        = obj->m_desc;

    m_packet_padding = obj->m_packet_padding;
    /* copy offsets*/
    m_ether_offset   = obj->m_ether_offset;
    m_ip_offset      = obj->m_ip_offset;
    m_udp_tcp_offset = obj->m_udp_tcp_offset;;
    m_payload_offset = obj->m_payload_offset;
    UpdateMbufSize();
}



void CPacketIndication::Dump(FILE *fd,int verbose){
    fprintf(fd," ipg : %f \n",m_cap_ipg);
    fprintf(fd," key \n");
    fprintf(fd," ------\n");
    m_flow_key.Dump(fd);

    fprintf(fd," L2 info \n");
    fprintf(fd," ------\n");
    m_packet->Dump(fd,verbose);

    fprintf(fd," Descriptor \n");
    fprintf(fd," ------\n");
    m_desc.Dump(fd);

    if ( m_desc.IsValidPkt() ) {
        fprintf(fd," ipv4 \n");
        l3.m_ipv4->dump(fd);
        if ( m_desc.IsUdp() ) {
            l4.m_udp->dump(fd);
        }else{
            l4.m_tcp->dump(fd);
        }
        fprintf(fd," payload len : %d \n",m_payload_len);
    }else{
        fprintf(fd," not valid packet \n");
    }

#ifdef DEBUG
    fprintf(fd," offsets \n");
    fprintf(fd," ------\n");
    fprintf(fd," eth       : %d \n",(int)m_ether_offset);
    fprintf(fd," ip        : %d \n",(int)m_ip_offset);
    fprintf(fd," udp/tcp   : %d \n",(int)m_udp_tcp_offset);
    fprintf(fd," payload   : %d \n",(int)m_payload_offset);
    fprintf(fd," l3_size   : %d \n",(int)m_udp_tcp_offset-m_ip_offset);
    fprintf(fd," r/w       : %d \n",(int)m_rw_mbuf_size);
    fprintf(fd," r/o       : %d \n",(int)m_ro_mbuf_size);
    fprintf(fd,"----------------\n");
    fprintf(fd,"\n");
#endif
}

void CPacketIndication::Clean(){
    m_desc.Clear();
    m_ether=0;
    l3.m_ipv4=0;
    l4.m_tcp=0;
    m_payload=0;
    m_payload_len=0;
    m_rw_mbuf_size=0;
    m_ro_mbuf_size=0;
}



uint64_t CCPacketParserCounters::getTotalErrors(){
    uint64_t res=
    m_non_ip+
    m_arp+
    m_mpls+
    m_non_valid_ipv4_ver+
    m_ip_checksum_error+
    m_ip_length_error+
    m_ip_not_first_fragment_error+
    m_ip_ttl_is_zero_error+
    m_ip_multicast_error+

    m_non_tcp_udp_ah+
    m_non_tcp_udp_esp+
    m_non_tcp_udp_icmp+
    m_non_tcp_udp_gre+
    m_non_tcp_udp_ip+
    m_tcp_udp_pkt_length_error;
    return (res);
}

void CCPacketParserCounters::Clear(){
    m_pkt=0;
    m_non_ip=0;
    m_vlan=0;
    m_arp=0;
    m_mpls=0;

    m_non_valid_ipv4_ver=0;
    m_ip_checksum_error=0;
    m_ip_length_error=0;
    m_ip_not_first_fragment_error=0;
    m_ip_ttl_is_zero_error=0;
    m_ip_multicast_error=0;
    m_ip_header_options=0;

    m_non_tcp_udp=0;
    m_non_tcp_udp_ah=0;
    m_non_tcp_udp_esp=0;
    m_non_tcp_udp_icmp=0;
    m_non_tcp_udp_gre=0;
    m_non_tcp_udp_ip=0;
    m_tcp_header_options=0;
    m_tcp_udp_pkt_length_error=0;
    m_tcp=0;
    m_udp=0;
    m_valid_udp_tcp=0;
}


void CCPacketParserCounters::Dump(FILE *fd){

    DP (m_pkt);
    DP (m_non_ip);
    DP (m_vlan);
    DP (m_arp);
    DP (m_mpls);

    DP (m_non_valid_ipv4_ver);
    DP (m_ip_checksum_error);
    DP (m_ip_length_error);
    DP (m_ip_not_first_fragment_error);
    DP (m_ip_ttl_is_zero_error);
    DP (m_ip_multicast_error);
    DP (m_ip_header_options);

    DP (m_non_tcp_udp);
    DP (m_non_tcp_udp_ah);
    DP (m_non_tcp_udp_esp);
    DP (m_non_tcp_udp_icmp);
    DP (m_non_tcp_udp_gre);
    DP (m_non_tcp_udp_ip);
    DP (m_tcp_header_options);
    DP (m_tcp_udp_pkt_length_error);
    DP (m_tcp);
    DP (m_udp);
    DP (m_valid_udp_tcp);
}


bool CPacketParser::Create(){
    m_counter.Clear();
    return (true);
}

void CPacketParser::Delete(){
}


bool CPacketParser::ProcessPacket(CPacketIndication * pkt_indication,
                                  CCapPktRaw * raw_packet){
    BP_ASSERT(pkt_indication);
    pkt_indication->ProcessPacket(this,raw_packet);
    if (pkt_indication->m_desc.IsValidPkt()) {
        return (true);
    }
    return (false);
}

void CPacketParser::Dump(FILE *fd){
    fprintf(fd," parser statistics \n");
    fprintf(fd," ===================== \n");
    m_counter.Dump(fd);
}


void CPacketIndication::SetKey(void){
    uint32_t ip_src, ip_dst;

    m_desc.SetIsValidPkt(true);
    if (is_ipv6()){
        uint16_t ipv6_src[8];
        uint16_t ipv6_dst[8];

        l3.m_ipv6->getSourceIpv6(&ipv6_src[0]);
        l3.m_ipv6->getDestIpv6(&ipv6_dst[0]);
        ip_src=(ipv6_src[6] << 16) | ipv6_src[7];
        ip_dst=(ipv6_dst[6] << 16) | ipv6_dst[7];
        m_flow_key.m_ip_proto   = l3.m_ipv6->getNextHdr();
    }else{
        ip_src=l3.m_ipv4->getSourceIp();
        ip_dst=l3.m_ipv4->getDestIp();
        m_flow_key.m_ip_proto   = l3.m_ipv4->getProtocol();
    }

    /* UDP/TCP has same place */
    uint16_t src_port = l4.m_udp->getSourcePort();
    uint16_t dst_port = l4.m_udp->getDestPort();
    if (ip_src > ip_dst ) {
       m_flow_key.m_ipaddr1 =ip_dst;
       m_flow_key.m_ipaddr2 =ip_src;
       m_flow_key.m_port1   = dst_port;
       m_flow_key.m_port2   = src_port;
    }else{
        m_desc.SetSwapTuple(true);
        m_flow_key.m_ipaddr1 = ip_src;
        m_flow_key.m_ipaddr2 = ip_dst;
        m_flow_key.m_port1   = src_port;
        m_flow_key.m_port2   = dst_port;
    }
    m_flow_key.m_l2_proto   = 0;
    m_flow_key.m_vrfid      = 0;
}

uint8_t CPacketIndication::ProcessIpPacketProtocol(CCPacketParserCounters *m_cnt,
                                  uint8_t protocol, int *offset){

    char * packetBase = m_packet->raw;
    TCPHeader * tcp=0;
    UDPHeader * udp=0;
    uint16_t tcp_header_len=0;

    switch (protocol) {
    case IPHeader::Protocol::TCP :
        m_desc.SetIsTcp(true);
        tcp =(TCPHeader *)(packetBase +*offset);
        l4.m_tcp = tcp;

        tcp_header_len = tcp->getHeaderLength();
        if ( tcp_header_len  > (5*4) ){
            // we have ip option
            m_cnt->m_tcp_header_options++;
        }
        *offset += tcp_header_len;
        m_cnt->m_tcp++;
        break;
    case IPHeader::Protocol::UDP :
        m_desc.SetIsUdp(true);
        udp =(UDPHeader *)(packetBase +*offset);
        l4.m_udp = udp;
        *offset += 8;
        m_cnt->m_udp++;
        break;
    case         IPHeader::Protocol::AH:
        m_cnt->m_non_tcp_udp_ah++;
        return (1);
        break;
    case         IPHeader::Protocol::ESP:
        m_cnt->m_non_tcp_udp_esp++;
        return (1);
        break;
    case         IPHeader::Protocol::ICMP:
    case         IPHeader::Protocol::IPV6_ICMP:
        m_cnt->m_non_tcp_udp_icmp++;
        return (1);
        break;
    case         IPHeader::Protocol::GRE:
        m_cnt->m_non_tcp_udp_gre++;
        return (1);
        break;
    case         IPHeader::Protocol::IP:
        m_cnt->m_non_ip++;
        return (1);
        break;

    default:
        m_cnt->m_non_tcp_udp++;
        return (1);
        break;
    }

    /* out of packet */
    if ( *offset > m_packet->getTotalLen() ) {
        m_cnt->m_tcp_udp_pkt_length_error++;
        return (1);
    }
    return (0);
}


void CPacketIndication::ProcessIpPacket(CPacketParser *parser,
                                        int offset){

    char * packetBase;
    CCPacketParserCounters * m_cnt=&parser->m_counter;
    packetBase = m_packet->raw;
    uint8_t protocol;
    BP_ASSERT(l3.m_ipv4);

    parser->m_counter.m_pkt++;

    if ( l3.m_ipv4->getVersion() == 4 ){
        m_cnt->m_ipv4++;
    }else{
        m_cnt->m_non_valid_ipv4_ver++;
        return;
    }
    // check the IP Length
    if( (uint32_t)(l3.m_ipv4->getTotalLength()+offset) > (uint32_t)( m_packet->getTotalLen())   ){
        m_cnt->m_ip_length_error++;
        return;
    }

    uint16_t ip_offset=offset;
    uint16_t ip_header_length = l3.m_ipv4->getHeaderLength();

    if ( ip_header_length >(5*4) ){
        m_cnt->m_ip_header_options++;
    }

    if ( (uint32_t)(ip_header_length + offset) > (uint32_t)m_packet->getTotalLen() ) {
        m_cnt->m_ip_length_error++;
        return;
    }
    offset += ip_header_length;


    if( l3.m_ipv4->getTimeToLive() ==0 ){
        m_cnt->m_ip_ttl_is_zero_error++;
        return;
    }

    if( l3.m_ipv4->isNotFirstFragment() ) {
        m_cnt->m_ip_not_first_fragment_error++;
        return;
    }

    protocol = l3.m_ipv4->getProtocol();
    if (ProcessIpPacketProtocol(m_cnt,protocol,&offset) != 0) {
        return;
    };

    uint16_t payload_offset_from_ip = offset-ip_offset;
    if ( payload_offset_from_ip >  l3.m_ipv4->getTotalLength() ) {
        m_cnt->m_tcp_udp_pkt_length_error++;
        return;
    }

    if ( m_packet->pkt_len > MAX_PKT_SIZE ){
        m_cnt->m_tcp_udp_pkt_length_error++;
        printf("Error: packet too big. Max supported packet size is %d \n",MAX_PKT_SIZE);
        return;
    }

    // Set packet length and include padding if needed
    m_packet->pkt_len = l3.m_ipv4->getTotalLength() + getIpOffset();
    if (m_packet->pkt_len < 60) { m_packet->pkt_len = 60; }

    m_cnt->m_valid_udp_tcp++;
    m_payload_len = l3.m_ipv4->getTotalLength() - (payload_offset_from_ip);
    m_payload     = (uint8_t *)(packetBase +offset);
    UpdatePacketPadding();
    SetKey();
}




void CPacketIndication::PostProcessIpv6Packet(){

    /* if it was converted we have the information of init */
    if (m_is_ipv6_converted) {
        uint16_t idx;
        uint16_t src_ipv6[6];
        uint16_t dst_ipv6[6];

        for (idx=0; idx<6; idx++){
            src_ipv6[idx] = CGlobalInfo::m_options.m_src_ipv6[idx];
            dst_ipv6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
        }
        if ( m_desc.IsInitSide() ){
            l3.m_ipv6->updateMSBIpv6Src(&src_ipv6[0]);
            l3.m_ipv6->updateMSBIpv6Dst(&dst_ipv6[0]);
        }else{
            l3.m_ipv6->updateMSBIpv6Src(&dst_ipv6[0]);
            l3.m_ipv6->updateMSBIpv6Dst(&src_ipv6[0]);
        }
    }
}


void CPacketIndication::ProcessIpv6Packet(CPacketParser *parser,
                                        int offset){

    char * packetBase = m_packet->raw;
    CCPacketParserCounters * m_cnt=&parser->m_counter;
    uint8_t protocol;
    BP_ASSERT(l3.m_ipv6);

    parser->m_counter.m_pkt++;

    if ( l3.m_ipv6->getVersion() == 6 ){
        m_cnt->m_ipv6++;
    }else{
        m_cnt->m_non_valid_ipv6_ver++;
        return;
    }

     // Check length
    if ((uint32_t)(l3.m_ipv6->getPayloadLen()+offset+l3.m_ipv6->getHeaderLength()) >
        (uint32_t)( m_packet->getTotalLen())   ){
        m_cnt->m_ipv6_length_error++;
        return;
    }

    offset += l3.m_ipv6->getHeaderLength();
    protocol = l3.m_ipv6->getNextHdr();
    if (ProcessIpPacketProtocol(m_cnt,protocol,&offset) != 0) {
        return;
    };

    // Set packet length and include padding if needed
    uint16_t real_pkt_size = l3.m_ipv6->getPayloadLen()+ getIpOffset() + l3.m_ipv6->getHeaderLength();
    m_packet->pkt_len = real_pkt_size;
    if (m_packet->pkt_len < 60) { m_packet->pkt_len = 60; }

    m_cnt->m_valid_udp_tcp++;
    m_payload_len = l3.m_ipv6->getPayloadLen();
    m_payload     = (uint8_t *)(packetBase +offset);

    m_packet_padding = m_packet->getTotalLen() -  real_pkt_size;
    assert( m_packet->getTotalLen()>= real_pkt_size );
    SetKey();
}


static uint8_t cbuff[MAX_PKT_SIZE];

bool CPacketIndication::ConvertPacketToIpv6InPlace(CCapPktRaw * pkt,
                                                       int offset){

    // Copy l2 data and set l2 type to ipv6
    memcpy(cbuff, pkt->raw, offset);
    *(uint16_t*)(cbuff+12) = PKT_HTONS(EthernetHeader::Protocol::IPv6);

    // Create the ipv6 header
    IPHeader *ipv4 = (IPHeader *) (pkt->raw+offset);
    IPv6Header *ipv6 = (IPv6Header *) (cbuff+offset);
    uint8_t ipv6_hdrlen = ipv6->getHeaderLength();
    memset((char *)ipv6,0,ipv6_hdrlen);
    ipv6->setVersion(6);
    if (ipv4->getTotalLength() < ipv4->getHeaderLength()) {
        return(false);
    }
    // Calculate the payload length
    uint16_t p_len = ipv4->getTotalLength() - ipv4->getHeaderLength();
    ipv6->setPayloadLen(p_len);
    uint8_t l4_proto = ipv4->getProtocol();
    ipv6->setNextHdr(l4_proto);
    ipv6->setHopLimit(64);

    // Update the least signficant 32-bits of ipv6 address
    //   using the ipv4 address
    ipv6->updateLSBIpv6Src(ipv4->getSourceIp());
    ipv6->updateLSBIpv6Dst(ipv4->getDestIp());

    // Copy rest of packet
    uint16_t ipv4_offset = offset + ipv4->getHeaderLength();
    uint16_t ipv6_offset = offset + ipv6_hdrlen;
    memcpy(cbuff+ipv6_offset,pkt->raw+ipv4_offset,p_len);

    ipv6_offset+=p_len;
    memcpy(pkt->raw,cbuff,ipv6_offset);

    // Set packet length
    pkt->pkt_len = ipv6_offset;
    m_is_ipv6 = true;
    m_is_ipv6_converted =true;
    return (true);
}

void CPacketIndication::ProcessPacket(CPacketParser *parser,
                                      CCapPktRaw * pkt){
    _ProcessPacket(parser,pkt);
    if ( m_desc.IsValidPkt() ){
        UpdateOffsets(); /* update fast offsets */
    }
}

/* process packet */
void CPacketIndication::_ProcessPacket(CPacketParser *parser,
                                      CCapPktRaw * pkt){

    BP_ASSERT(pkt);
    m_packet =pkt;
    Clean();
    CCPacketParserCounters * m_cnt=&parser->m_counter;

    int offset = 0;
    char * packetBase;
    packetBase = m_packet->raw;
    BP_ASSERT(packetBase);
    m_ether = (EthernetHeader *)packetBase;
    m_is_ipv6 = false;
    m_is_ipv6_converted =false;


    // IP
    switch( m_ether->getNextProtocol() ) {
    case EthernetHeader::Protocol::IP :
        offset = 14;
        l3.m_ipv4 =(IPHeader *)(packetBase+offset);
        break;
    case EthernetHeader::Protocol::IPv6 :
        offset = 14;
        l3.m_ipv6 =(IPv6Header *)(packetBase+offset);
        m_is_ipv6 = true;
        break;
    case EthernetHeader::Protocol::VLAN :
        m_cnt->m_vlan++;
        switch ( m_ether->getVlanProtocol() ){
          case EthernetHeader::Protocol::IP:
               offset = 18;
               l3.m_ipv4 =(IPHeader *)(packetBase+offset);
              break;
          case EthernetHeader::Protocol::IPv6 :
              offset = 18;
              l3.m_ipv6 =(IPv6Header *)(packetBase+offset);
              m_is_ipv6 = true;
              break;
          case EthernetHeader::Protocol::MPLS_Multicast   :
          case EthernetHeader::Protocol::MPLS_Unicast  :
              m_cnt->m_mpls++;
              return;

        case EthernetHeader::Protocol::ARP :
            m_cnt->m_arp++;
            return;

        default:
            m_cnt->m_non_ip++;
            return ; /* Non IP */
            }
        break;
    case EthernetHeader::Protocol::ARP  :
        m_cnt->m_arp++;
        return; /* Non IP */
        break;

    case EthernetHeader::Protocol::MPLS_Multicast   :
    case EthernetHeader::Protocol::MPLS_Unicast  :
        m_cnt->m_mpls++;
        return; /* Non IP */
        break;

    default:
        m_cnt->m_non_ip++;
        return; /* Non IP */
    }

    if (is_ipv6() == false) {
        if( (14+20) > (uint32_t)( m_packet->getTotalLen())   ){
            m_cnt->m_ip_length_error++;
            return;
        }
    }

    // For now, we can not mix ipv4 and ipv4 packets
    // so we require --ipv6 option be set for ipv6 packets
    if ((m_is_ipv6) && (CGlobalInfo::is_ipv6_enable() == false)){
        fprintf(stderr,"Error: --ipv6 command line option must be set in order to process ipv6 packets\n");
        exit(-1);
    }

    // Convert to Ipv6 if requested and not already Ipv6
    if ((CGlobalInfo::is_ipv6_enable()) && (is_ipv6() == false )) {
        if (ConvertPacketToIpv6InPlace(pkt, offset) == false){
            /* Move to next packet as this was erroneous */
            printf(" Unable to convert packet to IPv6, skipping...\n");
            return;
        }
    }

    if (is_ipv6()){
        ProcessIpv6Packet(parser,offset);
    }else{
        ProcessIpPacket(parser,offset);
    }
}



void CFlowTableStats::Clear(){
  m_lookup=0;
  m_found=0;
  m_fif=0;
  m_add=0;
  m_remove=0;
  m_fif_err=0;
  m_active=0;
}

void CFlowTableStats::Dump(FILE *fd){
    DP (m_lookup);
    DP (m_found);
    DP (m_fif);
    DP (m_add);
    DP (m_remove);
    DP (m_fif_err);
    DP (m_active);
}


void CFlow::Dump(FILE *fd){
    fprintf(fd," fif is swap : %d \n",is_fif_swap);
}


void CFlowTableManagerBase::Dump(FILE *fd){
    m_stats.Dump(fd);
}

// Return flow that has given key. If flow does not exist, create one, and add to CFlow data structure.
// key - key to lookup by.
// is_fif - return: true if flow did not exist (This is the first packet we see in this flow).
//                  false if flow already existed
CFlow * CFlowTableManagerBase::process(const CFlowKey & key, bool & is_fif) {
    m_stats.m_lookup++;
    is_fif=false;
    CFlow * lp=lookup(key);
    if ( lp  ) {
        m_stats.m_found++;
        return (lp);
    }else{
        m_stats.m_fif++;
        m_stats.m_active++;
        m_stats.m_add++;
        is_fif=true;
        lp= add(key );
        if (lp) {
        }else{
            m_stats.m_fif_err++;
        }
    }
    return (lp);
}

bool CFlowTableMap::Create(int max_size){
    m_stats.Clear();
    return (true);
}

void CFlowTableMap::Delete(){
    remove_all();
}

void CFlowTableMap::remove(const CFlowKey & key ) {
    CFlow *lp=lookup(key);
    if ( lp ) {
        delete lp;
        m_stats.m_remove++;
        m_stats.m_active--;
        m_map.erase(key);
    }else{
        BP_ASSERT(0);
    }
}


CFlow * CFlowTableMap::lookup(const CFlowKey & key ) {
    flow_map_t::iterator iter;
    iter = m_map.find(key);
    if (iter != m_map.end() ) {
        return ( (*iter).second );
    }else{
        return (( CFlow*)0);
    }
}

CFlow * CFlowTableMap::add(const CFlowKey & key ) {
    CFlow * flow = new CFlow();
    m_map.insert(flow_map_t::value_type(key,flow));
    return (flow);
}

void CFlowTableMap::remove_all() {
    for (auto &it : m_map) {
        delete it.second;
    }
    m_map.clear();
}

uint64_t CFlowTableMap::count(){
    return ( m_map.size());
}


char * CFlowPktInfo::push_ipv4_option_offline(uint8_t bytes){
    /* must be align by 4*/
    assert( (bytes % 4)== 0 );
    assert(m_pkt_indication.is_ipv6()==false);
    if ( m_pkt_indication.l3.m_ipv4->getHeaderLength()+bytes>60 ){
        printf(" ERROR ipv4 options size is too big, should be able to add %d bytes for internal info \n",bytes);
        return((char *)0);
    }
    /* now we can do that !*/

    /* add more bytes to the packet */
    m_packet->append(bytes);
    uint8_t ip_offset_to_move= m_pkt_indication.getFastIpOffsetFast()+IPHeader::DefaultSize;
    char *p=m_packet->raw+ip_offset_to_move;
    uint16_t bytes_to_move= m_packet->pkt_len - ip_offset_to_move -bytes;

    /* move the start of ipv4 options */
    memmove(p+bytes ,p, bytes_to_move);

    /* fix all other stuff */
    if ( m_pkt_indication.m_udp_tcp_offset ){
        m_pkt_indication.m_udp_tcp_offset+=bytes;
    }
    if ( m_pkt_indication.m_payload_offset ) {
        m_pkt_indication.m_payload_offset+=bytes;
    }

    m_pkt_indication.RefreshPointers();
    /* now pointer are updated we can manipulate ipv4 header */
    IPHeader       * ipv4=m_pkt_indication.l3.m_ipv4;

    ipv4->setTotalLength(ipv4->getTotalLength()+bytes);
    ipv4->setHeaderLength(ipv4->getHeaderLength()+(bytes));

    m_pkt_indication.UpdatePacketPadding();
    m_pkt_indication.UpdateMbufSize();

    /* refresh the global mbuf */
    free_const_mbuf();
    alloc_const_mbuf();
    return (p);
}

void CFlowPktInfo::mark_as_learn(){
    CNatOption *lpNat;
    if ( m_pkt_indication.is_ipv6() ) {
        lpNat=(CNatOption *)push_ipv6_option_offline(CNatOption::noOPTION_LEN);
        lpNat->set_init_ipv6_header();
        lpNat->set_fid(0);
        lpNat->set_thread_id(0);
    } else {
        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION)) {
            // Make space in IPv4 header for NAT option
            lpNat=(CNatOption *)push_ipv4_option_offline(CNatOption::noOPTION_LEN);
            lpNat->set_init_ipv4_header();
            lpNat->set_fid(0);
            lpNat->set_thread_id(0);
            m_pkt_indication.l3.m_ipv4->updateCheckSum();
        }
        /* learn is true */
        m_pkt_indication.m_desc.SetLearn(true);
    }
}

char * CFlowPktInfo::push_ipv6_option_offline(uint8_t bytes){

    /* must be align by 8*/
    assert( (bytes % 8)== 0 );
    assert(m_pkt_indication.is_ipv6()==true);

    /* add more bytes to the packet */
    m_packet->append(bytes);
    uint8_t ip_offset_to_move= m_pkt_indication.getFastIpOffsetFast()+IPv6Header::DefaultSize;
    char *p=m_packet->raw+ip_offset_to_move;
    uint16_t bytes_to_move= m_packet->pkt_len - ip_offset_to_move -bytes;

    /* move the start of ipv4 options */
    memmove(p+bytes ,p, bytes_to_move);

    /* fix all other stuff */
    if ( m_pkt_indication.m_udp_tcp_offset ){
        m_pkt_indication.m_udp_tcp_offset+=bytes;
    }
    if ( m_pkt_indication.m_payload_offset ) {
        m_pkt_indication.m_payload_offset+=bytes;
    }

    m_pkt_indication.RefreshPointers();
    /* now pointer are updated we can manipulate ipv6 header */
    IPv6Header       * ipv6=m_pkt_indication.l3.m_ipv6;

    ipv6->setPayloadLen(ipv6->getPayloadLen()+bytes);
    uint8_t save_header= ipv6->getNextHdr();
    *p=save_header; /* copy next header */
    ipv6->setNextHdr(CNatOption::noIPV6_OPTION);

    m_pkt_indication.UpdatePacketPadding();
    m_pkt_indication.UpdateMbufSize();

    /* refresh the global mbuf */
    free_const_mbuf();
    alloc_const_mbuf();
    return (p);
}


void CFlowPktInfo::alloc_const_mbuf(){

    uint16_t pkt_s = m_pkt_indication.get_cons_mbuf_size();

    if ( pkt_s  > 0 ) {
        uint8_t rw_mbuf_size = m_pkt_indication.get_rw_mbuf_size() ;
        int i;
        for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
            if ( CGlobalInfo::m_socket.is_sockets_enable(i) ){

                rte_mbuf_t        * m;

                m = CGlobalInfo::pktmbuf_alloc(i,pkt_s);
                rte_mbuf_set_as_core_const(m);
                BP_ASSERT(m);
                char *p=rte_pktmbuf_append(m, pkt_s);
                rte_memcpy(p,(m_packet->raw+rw_mbuf_size),pkt_s);

                assert(m_big_mbuf[i]==NULL);
                m_big_mbuf[i]=m;
            }
        }
    }
}

void CFlowPktInfo::free_const_mbuf(){
    int i;
    for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
        rte_mbuf_t   * m=m_big_mbuf[i];
        if (m) {
            rte_mbuf_set_as_core_multi(m);
            assert(rte_mbuf_refcnt_read(m)==1);
            rte_pktmbuf_free(m);
            m_big_mbuf[i]=NULL;
        }
    }
}


bool CFlowPktInfo::Create(CPacketIndication  * pkt_ind){
    /* clone the packet*/
    m_packet = new CCapPktRaw(pkt_ind->m_packet);
    /* clone of the offsets */
    m_pkt_indication.Clone(pkt_ind,m_packet);

    int i;
    for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
        m_big_mbuf[i] = NULL;
    }
    alloc_const_mbuf();
    return (true);
}

void CFlowPktInfo::Delete(){
    free_const_mbuf();
    delete m_packet;
}

void CFlowPktInfo::Dump(FILE *fd){
    m_pkt_indication.Dump(fd,0);
}



void CCapFileFlowInfo::generate_flow(CTupleTemplateGeneratorSmart   * tuple_gen,
                                            CNodeGenerator * gen,
                                            dsec_t time,
                                            uint64_t flow_id,
                                            CFlowYamlInfo *  template_info,
                                            CGenNode *     node){
    dsec_t c_time = time;

    node->m_type=CGenNode::FLOW_PKT;
    CTupleBase  tuple;
    tuple_gen->GenerateTuple(tuple);

    CFlowGenListPerThread  * lpThread=gen->Parent();

    /* add the first packet of the flow */
    CFlowPktInfo *  lp=GetPacket((uint32_t)0);

    node->set_socket_id(gen->m_socket_id);

    node->m_thread_id = tuple_gen->GetThreadId();
    node->m_flow_id = (flow_id & (0x000fffffffffffffULL)) |
                      ( ((uint64_t)(tuple_gen->GetThreadId()& 0xff)) <<56 ) ;

    node->m_time     = c_time;
    node->m_pkt_info = lp;
    node->m_flow_info = this;
    node->m_flags=0;
    node->m_template_info =template_info;
    node->m_tuple_gen = tuple_gen->get_gen();
    node->m_src_ip= tuple.getClient();
    node->m_dest_ip = tuple.getServer();
    node->m_src_idx = tuple.getClientId();
    node->m_dest_idx = tuple.getServerId();
    node->m_src_port = tuple.getClientPort();
    node->m_client_cfg = tuple.getClientCfg();
    node->set_nat_tcp_seq_diff_client(0);
    node->set_nat_tcp_seq_diff_server(0);

    node->m_plugin_info =(void *)0;

    if ( unlikely( CGlobalInfo::is_learn_mode()  ) ){
        // check if flow is two direction
        if ( lp->m_pkt_indication.m_desc.IsBiDirectionalFlow() ) {
            /* we are in learn mode */
            bool is_ok;
            if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION) ||
                node->m_pkt_info->m_pkt_indication.l3.m_ipv4->getProtocol() == IPPROTO_TCP) {
                is_ok = lpThread->associate(((uint32_t)flow_id) & NAT_FLOW_ID_MASK_TCP_ACK, node);  /* associate flow_id=>node */
                assert(is_ok);
            } else {
               is_ok = lpThread->associate(((uint32_t)flow_id) & NAT_FLOW_ID_MASK_IP_ID, node);  /* associate flow_id=>node */
               if (! is_ok) {
                   fprintf(stderr, "With given learn mode, max supported rate of UDP flows is limited. Please reduce the rate\n");
                   exit(1);
               }
            }
            node->set_nat_first_state();
        }
    }

    if ( unlikely(  get_is_rx_check_mode()) ) {
        if  ( (CGlobalInfo::m_options.m_rx_check_sample == 1 ) ||
            ( ( rte_rand() % CGlobalInfo::m_options.m_rx_check_sample ) == 1 )){
           if (unlikely(!node->is_repeat_flow() )) {
               node->set_rx_check();
           }
        }
    }

    if ( unlikely( CGlobalInfo::m_options.preview.getClientServerFlowFlipAddr() ) ){
        node->set_initiator_start_from_server_side_with_server_addr(node->is_eligible_from_server_side());
    }else{
        /* -p */
        if ( likely( CGlobalInfo::m_options.preview.getClientServerFlowFlip() ) ){
            node->set_initiator_start_from_server(node->is_eligible_from_server_side());
            node->set_all_flow_from_same_dir(true);
        }else{
            /* --flip */
            if ( unlikely( CGlobalInfo::m_options.preview.getClientServerFlip() ) ){
                node->set_initiator_start_from_server(node->is_eligible_from_server_side());
            }
        }
    }


    /* in case of plugin we need to call the callback */
    if ( template_info->m_plugin_id ) {
        /* alloc the info , generate the ports */
        on_node_first(template_info->m_plugin_id,node,template_info,tuple_gen,gen->Parent() );
    }

    node->m_tmr.reset();

    /* in  case of noraml flow  use TW */
    if (likely(node->m_type == CGenNode::FLOW_PKT)){
        lpThread->on_flow_tick<false>(node); /* tick packet */
    }else{
        gen->add_node(node);
    }
}


void CCapFileFlowInfo::save_to_erf(std::string cap_file_name,int pcap){
    if (Size() ==0) {
        fprintf(stderr,"ERROR no info for this flow ");
        return ;
    }
    capture_type_e file_type=ERF;
    if ( pcap ){
        file_type=LIBPCAP;
    }


    CFileWriterBase * lpWriter=CCapWriterFactory::CreateWriter(file_type,(char *)cap_file_name.c_str());
    if (lpWriter == NULL) {
        fprintf(stderr,"ERROR can't create cap file %s ",(char *)cap_file_name.c_str());
        return ;
    }
    int i;

    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo *  lp=GetPacket((uint32_t)i);
        bool res=lpWriter->write_packet(lp->m_packet);
        BP_ASSERT(res);
    }
    delete lpWriter;
}



struct CTmpFlowPerDirInfo {
    CTmpFlowPerDirInfo(){
        m_pkt_id=0;
    }

    uint16_t    m_pkt_id;
};

class CTmpFlowInfo {
public:
    CTmpFlowInfo(){
        m_max_pkts=0;
        m_max_aging_sec=0.0;
        m_last_pkt=0.0;

    }
    ~CTmpFlowInfo(){
       }
public:
    uint32_t  m_max_pkts;
    dsec_t    m_max_aging_sec;
    dsec_t    m_last_pkt;

    CTmpFlowPerDirInfo  m_per_dir[CS_NUM];
};

typedef CTmpFlowInfo * flow_tmp_t;
typedef std::map<uint16_t, flow_tmp_t> flow_tmp_map_t;
typedef flow_tmp_map_t::iterator flow_tmp_map_iter_t;

enum CCapFileFlowInfo::load_cap_file_err CCapFileFlowInfo::is_valid_template_load_time(){
   int i;
   bool is_tcp = false;

    for (i=0; i<Size(); i++) {
        CFlowPktInfo * lp= GetPacket((uint32_t)i);
        CPacketIndication * lpd=&lp->m_pkt_indication;
        if ( lpd->getEtherOffset() !=0 ){
            fprintf(stderr, "Error: Bad CAP file. Ether offset start is not 0 in packet %d \n", i+1);
            return kPktNotSupp;
        }

        if  ( CGlobalInfo::is_learn_mode() ) {
            // We change TCP ports. Need them to be in first 64 byte mbuf.
            // Since we also add IP option, and rx-check feature might add another IP option, better not allow
            // OP options in this mode. If needed this limitation can be refined a bit.
            if ( lpd->getTcpOffset() - lpd->getIpOffset() != 20 ) {
                fprintf(stderr, "Error: Bad CAP file. In learn (NAT) mode, no IP options allowed \n");
                return kIPOptionNotAllowed;
            }
        }
    }

    if  ( CGlobalInfo::is_learn_mode() ) {
        CPacketIndication &pkt_0_indication = GetPacket(0)->m_pkt_indication;

        if ( pkt_0_indication.m_desc.IsPluginEnable() ) {
            fprintf(stderr, "Error: plugins are not supported with --learn mode \n");
            return kPlugInWithLearn;
        }

        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP) && (pkt_0_indication.getIpProto() == IPPROTO_TCP)) {
            is_tcp = true;
            if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
                if (Size() < 3) {
                    fprintf(stderr
                            , "Error: In the chosen learn mode, need at least the 3 TCP handshake packets.\n");
                    fprintf(stderr
                            , "       Please give different CAP file, or try different --learn-mode\n");
                    return kTCPLearnModeBadFlow;
                }
            }
            CPacketIndication &pkt_1_indication = GetPacket(1)->m_pkt_indication;


            // verify first packet is TCP SYN from client
            TCPHeader *tcp = (TCPHeader *)(pkt_0_indication.getBasePtr() + pkt_0_indication.getTcpOffset());
            if ( (! pkt_0_indication.m_desc.IsInitSide()) || (! tcp->getSynFlag()) ) {
                fprintf(stderr, "Error: In the chosen learn mode, first TCP packet should be SYN from client side.\n");
                fprintf(stderr, "       In cap file, first packet side direction is %s. TCP header is:\n"
                        , pkt_0_indication.m_desc.IsInitSide() ? "outside":"inside");
                tcp->dump(stderr);
                fprintf(stderr, "       Please give different CAP file, or try different --learn-mode\n");
                return kNoSyn;
            }

            if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK) && is_tcp) {
                // To support TCP seq randomization from server to client, we need second packet in flow to be the server SYN+ACK
                bool error = false;
                if (pkt_1_indication.getIpProto() != IPPROTO_TCP) {
                    error = true;
                } else {
                    TCPHeader *tcp = (TCPHeader *)(pkt_1_indication.getBasePtr() + pkt_1_indication.getTcpOffset());
                    if ( (! tcp->getSynFlag()) ||  (! tcp->getAckFlag()) || ( pkt_1_indication.m_desc.IsInitSide())) {
                        error = true;
                    }
                }
                if (error) {
                    fprintf(stderr, "Error: In the chosen learn mode, second packet should be SYN+ACK from server.\n");
                    fprintf(stderr, "       Please give different CAP file, or try different --learn-mode\n");
                    return kNoTCPSynAck;
                }

                CPacketIndication &pkt_2_indication = GetPacket(2)->m_pkt_indication;
                if ( (! pkt_2_indication.m_desc.IsInitSide()) ) {
                    fprintf(stderr
                            , "Error: Wrong third packet. In the chosen learn mode, need at least the 3 TCP handshake packets.\n");
                    fprintf(stderr
                            , "       Please give different CAP file, or try different --learn-mode\n");
                    return kTCPLearnModeBadFlow;
                }
                if ((pkt_0_indication.m_cap_ipg < (double)LEARN_MODE_MIN_IPG / 1000)
                    || (pkt_1_indication.m_cap_ipg < (double)LEARN_MODE_MIN_IPG / 1000)) {
                    fprintf(stderr
                            , "Error: Bad cap file timings. In the chosen learn mode");
                    fprintf(stderr, "IPG between TCP handshake packets should be at least %d msec.\n", LEARN_MODE_MIN_IPG);
                    fprintf(stderr, "       Current delay is %f between second and first, %f between third and second"
                            , pkt_0_indication.m_cap_ipg, pkt_1_indication.m_cap_ipg);
                    fprintf(stderr
                            , "       Please give different CAP file, try different --learn-mode, or edit ipg parameters in template file\n");
                    return kTCPIpgTooLow;
                }
            }
        }
    }

    return(kOK);
}


/**
 * update global info
 * 1. maximum aging
 * 2. per sub-flow pkt_num/max-pkt per dir and per global
 */
void CCapFileFlowInfo::update_info(CFlowYamlInfo *  flow_info){
    flow_tmp_map_iter_t iter;
    flow_tmp_map_t      ft;
    CTmpFlowInfo *      lpFlow;
    int i;
    dsec_t ctime=0.0;

    CCalcIpgDiff dtick_util(BUCKET_TIME_SEC);


    // first iteration, lern all the info into a temp flow table
    for (i=0; i<Size(); i++) {
        CFlowPktInfo * lp= GetPacket((uint32_t)i);
        // extract flow_id
        CPacketDescriptor * desc=&lp->m_pkt_indication.m_desc;
        uint16_t flow_id = desc->getFlowId();
        CPacketDescriptorPerDir * lpCurPacket = desc->GetDirInfo();
        pkt_dir_t dir=desc->IsInitSide()?CLIENT_SIDE:SERVER_SIDE; // with respect to the first sub-flow in the template

        //update lpFlow
        iter = ft.find(flow_id);
        if (iter != ft.end() ) {
            lpFlow=(*iter).second;
        }else{
            lpFlow = new CTmpFlowInfo();
            assert(lpFlow);
            ft.insert(flow_tmp_map_t::value_type(flow_id,lpFlow));
            //add it

        }

        // main info
        lpCurPacket->SetPktNum(lpFlow->m_per_dir[dir].m_pkt_id);
        lpFlow->m_max_pkts++;
        lpFlow->m_per_dir[dir].m_pkt_id++;

        dsec_t delta = ctime - lpFlow->m_last_pkt ;
        lpFlow->m_last_pkt = ctime;
        if (delta > lpFlow->m_max_aging_sec) {
            lpFlow->m_max_aging_sec = delta;
        }
        // per direction info

        if (i<Size()) {
             ctime += lp->m_pkt_indication.m_cap_ipg;
        }
    }


    for (i=0; i<Size(); i++) {
        CFlowPktInfo * lp= GetPacket((uint32_t)i);

        CPacketDescriptor * desc=&lp->m_pkt_indication.m_desc;
        uint16_t flow_id = desc->getFlowId();
        CPacketDescriptorPerDir * lpCurPacket = desc->GetDirInfo();
        pkt_dir_t dir=desc->IsInitSide()?CLIENT_SIDE:SERVER_SIDE; // with respect to the first sub-flow in the template

        iter = ft.find(flow_id);
        assert( iter != ft.end() );
        lpFlow=(*iter).second;

        if ( (lpFlow->m_per_dir[0].m_pkt_id >0) &&
             (lpFlow->m_per_dir[1].m_pkt_id >0) ) {
            /* we have both dir */
            lp->m_pkt_indication.m_desc.SetBiPluginEnable(true);
        }

        if (CGlobalInfo::is_learn_mode()) {
            lp->m_pkt_indication.setIpIdNat(false);
        }
        lpCurPacket->SetMaxPkts(lpFlow->m_per_dir[dir].m_pkt_id);
        lp->m_pkt_indication.m_desc.SetMaxPktsPerFlow(lpFlow->m_max_pkts);
        lp->m_pkt_indication.m_desc.SetMaxFlowTimeout(lpFlow->m_max_aging_sec);



        /* update dtick from ipg */
        double  dtime=0;

        if ( likely ( lp->m_pkt_indication.m_desc.IsPcapTiming()) ){
            dtime     = lp->m_pkt_indication.m_cap_ipg ;
        }else{
            if ( lp->m_pkt_indication.m_desc.IsRtt() ){
                dtime     = flow_info->m_rtt_sec ;
            }else{
                dtime     = flow_info->m_ipg_sec;
            }
            lp->m_pkt_indication.m_cap_ipg = dtime;
        }
        lp->m_pkt_indication.m_ticks = dtick_util.do_calc(dtime);
    }


    /* in case of learn mode , we need to mark the first packet */
    if ( CGlobalInfo::is_learn_mode() ) {
        CFlowPktInfo * lp= GetPacket(0);
        assert(lp);
        /* only for bi directionl traffic, first packet, put learn flag */
        if ( lp->m_pkt_indication.m_desc.IsBiDirectionalFlow() ){
            lp->m_pkt_indication.setTTL(TTL_RESERVE_DUPLICATE);
            lp->m_pkt_indication.setTOSReserve();
            lp->mark_as_learn();
            lp->m_pkt_indication.setIpIdNat(true);
        }

        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
            if (Size() > 1) {
                // In this mode, we need to see the SYN+ACK as well.
                lp = GetPacket(1);
                assert(lp);
                if (lp->m_pkt_indication.getIpProto() == IPPROTO_TCP) {
                    lp->m_pkt_indication.setTTL(TTL_RESERVE_DUPLICATE);
                    lp->m_pkt_indication.setTOSReserve();
                }
            }
        }
    }

    if ( ft.empty() )
        return;

    flow_tmp_map_iter_t it;
    for (it= ft.begin(); it != ft.end(); ++it) {
        CTmpFlowInfo *lp = it->second;
        assert(lp);
        delete lp;
    }
    ft.clear();
}


enum CCapFileFlowInfo::load_cap_file_err CCapFileFlowInfo::load_cap_file(std::string cap_file, uint16_t _id, uint8_t plugin_id) {
    RemoveAll();

    fprintf(stdout," -- loading cap file %s \n",cap_file.c_str());
    CPacketParser parser;
    CPacketIndication pkt_indication;
    CCapReaderBase * lp=CCapReaderFactory::CreateReader((char *)cap_file.c_str(),0);

    if (lp == 0) {
        printf(" ERROR file %s does not exist or not supported \n",(char *)cap_file.c_str());
        return kFileNotExist;
    }
    bool multi_flow_enable =( (plugin_id!=0)?true:false);


    CFlowTableMap flow;

    parser.Create();
    flow.Create(0);
    CFlow * first_flow=0;
    bool first_flow_fif_is_swap=false;
    bool time_was_set=false;
    bool is_init_side=true;
    double last_time=0.0;
    CCapPktRaw raw_packet;
    int cnt=0;
    while ( true ) {
        /* read packet */
        if ( lp->ReadPacket(&raw_packet) ==false ){
            break;
        }
        cnt++;

        if ( !time_was_set ){
            last_time=raw_packet.get_time();
            time_was_set=true;
        }else{
            if (raw_packet.get_time()<last_time) {
                fprintf(stderr, "Error: Non valid pcap file. Timestamp is negative at packet %d\n", cnt);
                return kNegTimestamp;
            }
            last_time=raw_packet.get_time();
        }

        if ( parser.ProcessPacket(&pkt_indication, &raw_packet) ){

            if ( pkt_indication.m_desc.IsValidPkt() ) {
                pkt_indication.m_desc.SetPluginEnable(multi_flow_enable);
                pkt_indication.m_desc.SetPluginId(plugin_id);

                pkt_indication.m_desc.SetId(_id);
                bool is_fif;
                CFlow * lpflow=flow.process(pkt_indication.m_flow_key,is_fif);
                m_total_bytes += (pkt_indication.m_packet->pkt_len+4); /* L2 include CRC*/
                pkt_indication.m_cap_ipg = raw_packet.get_time();

                pkt_indication.m_flow =lpflow;
                pkt_indication.m_desc.SetFlowPktNum(lpflow->pkt_id);
                /* inc pkt_id inside the flow */
                lpflow->pkt_id++;

                /* check that we don't have reserved TTL */
                uint8_t ttl = pkt_indication.getTTL();
                if ( ttl > 128) {
                    pkt_indication.setTTL(128);
                }

                pkt_indication.clearTOSReserve();


                // Validation for first packet in flow
                if (is_fif) {
                    lpflow->flow_id = m_total_flows;
                    pkt_indication.m_desc.SetFlowId(lpflow->flow_id);

                    if (m_total_flows == 0) {
                        /* first flow */
                        first_flow =lpflow;/* save it for single flow support , to signal error */
                        lpflow->is_fif_swap =pkt_indication.m_desc.IsSwapTuple();
                        first_flow_fif_is_swap = pkt_indication.m_desc.IsSwapTuple();
                        is_init_side = true;
                        m_total_flows++;
                    } else {
                        if ( multi_flow_enable ) {
                            lpflow->is_fif_swap = pkt_indication.m_desc.IsSwapTuple();
                            /* in respect to the first flow */
                            is_init_side = ((first_flow_fif_is_swap?true:false) == lpflow->is_fif_swap)?true:false;
                            m_total_flows++;
                        } else {
                            printf("More than one flow in this cap. Ignoring it !! \n");
                            pkt_indication.m_flow_key.Dump(stderr);
                            m_total_errors++;
                        }
                    }
                }else{ /* no FIF */
                    pkt_indication.m_desc.SetFlowId(lpflow->flow_id);

                    if ( multi_flow_enable ==false ){
                        if (lpflow == first_flow) {
                            // add to
                            is_init_side = ((lpflow->is_fif_swap?true:false) == pkt_indication.m_desc.IsSwapTuple())?true:false;
                        }else{
                            //printf(" more than one flow in this cap ignot it !! \n");
                            m_total_errors++;
                        }
                    }else{
                        /* support multi-flow,  */

                        /* work in respect to first flow */
                        is_init_side = ((first_flow_fif_is_swap?true:false) == pkt_indication.m_desc.IsSwapTuple())?true:false;
                    }
                }
            }else{
                fprintf(stderr, "ERROR packet %d is not supported, should be Ethernet/IP(0x0800)/(TCP|UDP) format try to convert it using Wireshark !\n",cnt);
                return kPktNotSupp;
            }
            pkt_indication.m_desc.SetInitSide(is_init_side);
            Append(&pkt_indication);
        }else{
            fprintf(stderr, "ERROR packet %d is not supported, should be Ethernet/IP(0x0800)/(TCP|UDP) format try to convert it using Wireshark !\n",cnt);
            return kPktProcessFail;
        }
    }

    if (Size() == 0) {
        fprintf(stderr, "Error: Cap file '%s' is empty. Please remove it from the config file and try again.\n"
                , cap_file.c_str());
        return kEmptyFile;
    }
    /* set the last */
    CFlowPktInfo * last_pkt =GetPacket((uint32_t)(Size()-1));
    last_pkt->m_pkt_indication.m_desc.SetIsLastPkt(true);

    int i;

    for (i=0; i<Size(); i++) {
        CFlowPktInfo * lp= GetPacket((uint32_t)i);
        lp->m_pkt_indication.PostProcessIpv6Packet();
    }


    for (i=1; i<Size(); i++) {
        CFlowPktInfo * lp_prev= GetPacket((uint32_t)i-1);
        CFlowPktInfo * lp= GetPacket((uint32_t)i);

        lp_prev->m_pkt_indication.m_cap_ipg = lp->m_pkt_indication.m_cap_ipg-
                                              lp_prev->m_pkt_indication.m_cap_ipg;
        if ( lp->m_pkt_indication.m_desc.IsInitSide() !=
             lp_prev->m_pkt_indication.m_desc.IsInitSide()) {
            lp_prev->m_pkt_indication.m_desc.SetRtt(true);
        }
    }

    GetPacket((uint32_t)Size()-1)->m_pkt_indication.m_cap_ipg=0.0;
    m_total_errors += parser.m_counter.getTotalErrors();


    /* dump the flow */
    //Dump(stdout);

    //flow.Dump(stdout);
    flow.Delete();
    //parser.Dump(stdout);
    parser.Delete();
    //fprintf(stdout," -- finish loading cap file \n");
    //fprintf(stdout,"\n");
    delete lp;
    if ( m_total_errors > 0 ) {
        parser.m_counter.Dump(stdout);
        fprintf(stderr, " ERORR in one of the cap file, you should have one flow per cap file or valid plugin \n");
        return kCapFileErr;
    }
    return kOK;
}



void CCapFileFlowInfo::update_pcap_mode(){
    int i;
    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo * lp=GetPacket((uint32_t)i);
        lp->m_pkt_indication.m_desc.SetPcapTiming(true);
    }
}

void CCapFileFlowInfo::get_total_memory(CCCapFileMemoryUsage & memory){
    memory.clear();
    int i;
    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo * lp=GetPacket((uint32_t)i);
        uint16_t const_size = lp->m_pkt_indication.get_cons_mbuf_size();
        if ( const_size ) {
            memory.add_size(const_size);
        }
    }
}


double CCapFileFlowInfo::get_cap_file_length_sec(){
    dsec_t sum=0.0;
    int i;
    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo * lp=GetPacket((uint32_t)i);
        sum+=lp->m_pkt_indication.m_cap_ipg;
    }
    return (sum);
}



void CCapFileFlowInfo::update_ipg_by_factor(double factor,
                                            CFlowYamlInfo *  flow_info){
    int i;

    CCalcIpgDiff dtick_util(BUCKET_TIME_SEC);

    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo * lp=GetPacket((uint32_t)i);

        /* update dtick from ipg */
        double  dtime=0;

        if ( likely ( lp->m_pkt_indication.m_desc.IsPcapTiming()) ){
            dtime     = lp->m_pkt_indication.m_cap_ipg ;
        }else{
            if ( lp->m_pkt_indication.m_desc.IsRtt() ){
                dtime     = flow_info->m_rtt_sec ;
            }else{
                dtime     = flow_info->m_ipg_sec;
            }
        }
        lp->m_pkt_indication.m_cap_ipg = dtime*factor;
        lp->m_pkt_indication.m_ticks = dtick_util.do_calc(dtime*factor);
    }
}

void CCapFileFlowInfo::update_min_ipg(dsec_t min_ipg,
                                      dsec_t override_ipg){

    int i;
    for (i=0; i<(int)Size(); i++) {
        CFlowPktInfo * lp=GetPacket((uint32_t)i);
        if ( lp->m_pkt_indication.m_cap_ipg  < min_ipg ){
            lp->m_pkt_indication.m_cap_ipg=override_ipg;
        }
        if ( lp->m_pkt_indication.m_cap_ipg  < override_ipg ){
            lp->m_pkt_indication.m_cap_ipg=override_ipg;
        }
    }
}


void CCapFileFlowInfo::Dump(FILE *fd){


    int i;
    //CCapPacket::DumpHeader(fd);
    for (i=0; i<(int)Size(); i++) {
        fprintf(fd,"pkt_id : %d \n",i+1);
        fprintf(fd,"-----------\n");
        CFlowPktInfo * lp=GetPacket((uint32_t)i);
        lp->Dump(fd);
    }
}

// add pkt indication
void CCapFileFlowInfo::Append(CPacketIndication * pkt_indication){

    CFlowPktInfo * lp;
    lp = new CFlowPktInfo();
    lp->Create( pkt_indication );
    m_flow_pkts.push_back(lp);
}



void CCCapFileMemoryUsage::Add(const CCCapFileMemoryUsage & obj){
    int i;
    for (i=0; i<CCCapFileMemoryUsage::MASK_SIZE; i++) {
        m_buf[i] += obj.m_buf[i];
    }
    m_total_bytes +=obj.m_total_bytes;

}


void CCCapFileMemoryUsage::dump(FILE *fd){
    fprintf(fd, " Memory usage \n");
    int i;
    int c_size=CCCapFileMemoryUsage::SIZE_MIN;
    int c_total=0;

    for (i=0; i<CCCapFileMemoryUsage::MASK_SIZE; i++) {
        fprintf(fd," size_%-7d   : %lu \n",c_size, (ulong)m_buf[i]);
        c_total +=m_buf[i]*c_size;
        c_size = c_size*2;
    }
    fprintf(fd," Total    : %s  %.0f%% util due to buckets \n",double_to_human_str(c_total,"bytes",KBYE_1024).c_str(),100.0*float(c_total)/float(m_total_bytes) );
}


void CCapFileFlowInfo::clear(void) {
    m_total_bytes = 0;
    m_total_errors = 0;
    m_total_flows = 0;
}

bool CCapFileFlowInfo::Create(){
    clear();
    return (true);
}


void CCapFileFlowInfo::dump_pkt_sizes(void){
    int i;
    for (i=0; i<(int)Size(); i++) {
        flow_pkt_info_t lp=GetPacket((uint32_t)i);
        CGenNode node;
        node.m_dest_ip  = 0x10000110;
        node.m_src_ip   = 0x20000110;
        node.m_src_port = 12;
        rte_mbuf_t * buf=lp->generate_new_mbuf(&node);
        //rte_pktmbuf_dump(buf, buf->pkt_len);
        rte_pktmbuf_free(buf);
    }
}

void CCapFileFlowInfo::RemoveAll(){
    int i;
    clear();
    for (i=0; i<(int)Size(); i++) {
        flow_pkt_info_t lp=GetPacket((uint32_t)i);
        lp->Delete();
        delete lp;
    }
    // free all the pointers
    m_flow_pkts.clear();
}

void CCapFileFlowInfo::Delete(){
    RemoveAll();
}

void operator >> (const YAML::Node& node, CFlowYamlDpPkt & fi) {
    uint32_t val;
    node["pkt_id"]      >> val;
    fi.m_pkt_id =(uint8_t)val;
    node["pyld_offset"] >>  val;
    fi.m_pyld_offset =(uint8_t)val;
    node["type"]        >>  val;
    fi.m_type =(uint8_t)val;
    node["len"]         >>  val;
    fi.m_len =(uint8_t)val;
    node["mask"]        >>  val;
    fi.m_pkt_mask =val;
}

void operator >> (const YAML::Node& node, CVlanYamlInfo & fi) {

    uint32_t tmp;
    if ( node.FindValue("enable") ){
        node["enable"] >> tmp ;
        fi.m_enable=tmp;
        node["vlan0"] >>  tmp;
        fi.m_vlan_per_port[0] = tmp;
        node["vlan1"] >>  tmp;
        fi.m_vlan_per_port[1] = tmp;
    }
}



void operator >> (const YAML::Node& node, CFlowYamlInfo & fi) {
   node["name"] >> fi.m_name;

   if ( node.FindValue("client_pool") ){
       node["client_pool"] >> fi.m_client_pool_name;
   }else{
       fi.m_client_pool_name = "default";
   }
   if ( node.FindValue("server_pool") ){
       node["server_pool"] >> fi.m_server_pool_name;
   }else{
       fi.m_server_pool_name = "default";
   }

   node["cps"] >>  fi.m_k_cps;
   fi.m_k_cps = fi.m_k_cps/1000.0;
   double t;
   node["ipg"] >>  t;
   fi.m_ipg_sec =t/1000000.0;
   node["rtt"] >>  t;
   fi.m_rtt_sec = t/1000000.0;
   node["w"] >>  fi.m_w;

   if ( node.FindValue("cap_ipg") ){
       node["cap_ipg"] >> fi.m_cap_mode;
       fi.m_cap_mode_was_set =true;
   }else{
       fi.m_cap_mode_was_set =false;
   }


   if ( node.FindValue("limit") ){
       node["limit"] >>  fi.m_limit;
       fi.m_limit_was_set = true;
   }else{
       fi.m_limit_was_set = false;
       fi.m_limit = 0;
   }

   if ( node.FindValue("plugin_id") ){
       uint32_t plugin_val;
       node["plugin_id"] >> plugin_val;
       fi.m_plugin_id=plugin_val;
   }else{
       fi.m_plugin_id=0;
   }


   fi.m_one_app_server_was_set = false;
   fi.m_one_app_server = false;
   if ( utl_yaml_read_ip_addr(node,
                         "server_addr",
                         fi.m_server_addr) ){
       try {
           node["one_app_server"] >> fi.m_one_app_server;
           fi.m_one_app_server_was_set=true;
       } catch ( const std::exception& e ) {
           fi.m_one_app_server_was_set = false;
           fi.m_one_app_server = false;
       }
   }



   if ( ( fi.m_limit_was_set ) && (fi.m_plugin_id !=0) ){
       fprintf(stderr," limit can't be non zero when plugin is set, you must have only one of the options set");
       exit(-1);
   }


    if ( node.FindValue("dyn_pyload") ){
        const YAML::Node& dyn_pyload = node["dyn_pyload"];
        for(unsigned i=0;i<dyn_pyload.size();i++) {
            CFlowYamlDpPkt fd;
            dyn_pyload[i] >> fd;
            if ( fi.m_dpPkt == 0 ){
                fi.m_dpPkt = new CFlowYamlDynamicPyloadPlugin();
                if (fi.m_plugin_id == 0) {
                    fi.m_plugin_id = mpDYN_PYLOAD;
                }else{
                    fprintf(stderr," plugin should be zero with dynamic pyload program");
                    exit(-1);
                }
            }
            fi.m_dpPkt->Add(fd);
        }
    }else{
        fi.m_dpPkt=0;
    }
}



void operator >> (const YAML::Node& node, CFlowsYamlInfo & flows_info) {

   node["duration"] >> flows_info.m_duration_sec;

   if ( node.FindValue("generator") ) {
       node["generator"] >> flows_info.m_tuple_gen;
       flows_info.m_tuple_gen_was_set =true;
   }else{
       flows_info.m_tuple_gen_was_set =false;
   }

   // m_ipv6_set will be true if and only if both src_ipv6
   // and dst_ipv6 are provided.  These are used to set
   // the most significant 96-bits of the IPv6 address; the
   // least significant 32-bits come from the ipv4 address
   // (what is set above).
   //
   // If the IPv6 src/dst is not provided in the yaml file,
   // then the  most significant 96-bits will be set to 0
   // which represents an IPv4-compatible IPv6 address.
   //
   // If desired, an IPv4-mapped IPv6 address can be
   // formed by providing src_ipv6,dst_ipv6 and specifying
   // {0,0,0,0,0,0xffff}
   flows_info.m_ipv6_set=true;

   if ( node.FindValue("src_ipv6") ) {
       const YAML::Node& src_ipv6_info = node["src_ipv6"];
       if (src_ipv6_info.size() == 6 ){
            for(unsigned i=0;i<src_ipv6_info.size();i++) {
               uint32_t fi;
               const YAML::Node & node =src_ipv6_info;
               node[i]  >> fi;
               flows_info.m_src_ipv6.push_back(fi);
           }
       }
   }else{
       flows_info.m_ipv6_set=false;
   }


   if ( node.FindValue("dst_ipv6") ) {
       const YAML::Node& dst_ipv6_info = node["dst_ipv6"];
       if (dst_ipv6_info.size() == 6 ){
            for(unsigned i=0;i<dst_ipv6_info.size();i++) {
               uint32_t fi;
               const YAML::Node & node =dst_ipv6_info;
               node[i]  >> fi;
               flows_info.m_dst_ipv6.push_back(fi);
           }
       }
   }else{
       flows_info.m_ipv6_set=false;
   }

   if ( node.FindValue("cap_ipg") ) {
       node["cap_ipg"] >> flows_info.m_cap_mode;
       flows_info.m_cap_mode_set=true;
   }else{
       flows_info.m_cap_mode=false;
       flows_info.m_cap_mode_set=false;
   }

   double t=0.0;

   if ( node.FindValue("cap_ipg_min") ) {
       node["cap_ipg_min"] >>  t  ;
       flows_info.m_cap_ipg_min = t/1000000.0;
       flows_info.m_cap_ipg_min_set=true;
   }else{
       flows_info.m_cap_ipg_min_set=false;
       flows_info.m_cap_ipg_min = 20;
   }

   if ( node.FindValue("cap_override_ipg") ) {
       node["cap_override_ipg"] >> t;
       flows_info.m_cap_overide_ipg = t/1000000.0;
       flows_info.m_cap_overide_ipg_set = true;
   }else{
       flows_info.m_cap_overide_ipg_set = false;
       flows_info.m_cap_overide_ipg = 0;
   }


   if (node.FindValue("one_app_server")) {
       printf("one_app_server should be configured per template. \n"
              "Will ignore this configuration\n");
   }
   flows_info.m_one_app_server =false;
   flows_info.m_one_app_server_was_set=false;

   if (node.FindValue("vlan")) {
       node["vlan"] >> flows_info.m_vlan_info;
   }

   if (node.FindValue("mac_override_by_ip")) {
       node["mac_override_by_ip"] >> flows_info.m_mac_replace_by_ip;
   }else{
       flows_info.m_mac_replace_by_ip =false;
   }

   const YAML::Node& cap_info = node["cap_info"];
   for(unsigned i=0;i<cap_info.size();i++) {
       CFlowYamlInfo fi;
       cap_info[i] >> fi;
       fi.m_client_pool_idx =
           flows_info.m_tuple_gen.get_client_pool_id(fi.m_client_pool_name);
       fi.m_server_pool_idx =
           flows_info.m_tuple_gen.get_server_pool_id(fi.m_server_pool_name);
       flows_info.m_vec.push_back(fi);
   }

   if ( node.FindValue("tw") ){
       node["tw"] >> flows_info.m_tw;
   }

}

void CVlanYamlInfo::Dump(FILE *fd){
    fprintf(fd," vlan enable : %d  \n",m_enable);
    fprintf(fd," vlan val    : %d ,%d \n",m_vlan_per_port[0],m_vlan_per_port[1]);
}


void CFlowsYamlInfo::Dump(FILE *fd){
    fprintf(fd," duration : %f sec \n",m_duration_sec);

    fprintf(fd,"\n");
    if (CGlobalInfo::is_ipv6_enable()) {
        int idx;
        fprintf(fd," src_ipv6 : ");
        for (idx=0; idx<5; idx++){
            fprintf(fd,"%04x:", CGlobalInfo::m_options.m_src_ipv6[idx]);
        }
        fprintf(fd,"%04x\n", CGlobalInfo::m_options.m_src_ipv6[5]);
        fprintf(fd," dst_ipv6 : ");
        for (idx=0; idx<5; idx++){
            fprintf(fd,"%04x:", CGlobalInfo::m_options.m_dst_ipv6[idx]);
        }
        fprintf(fd,"%04x\n", CGlobalInfo::m_options.m_dst_ipv6[5]);
    }
    if ( !m_cap_mode_set ) {
        fprintf(fd," cap_ipg : wasn't set  \n");
    }else{
        fprintf(fd," cap_ipg : %d \n",m_cap_mode?1:0);
    }

    if ( !m_cap_ipg_min_set ){
        fprintf(fd," cap_ipg_min  : wasn't set  \n");
    }else{
        fprintf(fd," cap_ipg_min       : %f \n",m_cap_ipg_min);
    }

    if ( !m_cap_overide_ipg_set ){
        fprintf(fd," cap_override_ipg  : wasn't set  \n");
    }else{
        fprintf(fd," cap_override_ipg  : %f \n",m_cap_overide_ipg);
    }

    fprintf(fd," one_server_for_application : %d  \n",m_one_app_server?1:0);
    fprintf(fd," one_server_for_application_was_set : %d  \n",m_one_app_server_was_set?1:0);

    m_vlan_info.Dump(fd);

    fprintf(fd," cap file info \n");
    fprintf(fd," ------------- \n");
    for (int i=0; i<(int)m_vec.size(); i++) {
        m_vec[i].Dump(fd);
    }
    m_tw.Dump(fd);
}


/*

example for YAML file

- duration : 10.0
  cap_info :
     - name: hey1.pcap
       cps : 12.0
       ipg : 0.0001
     - name: hey2.pcap
       cps : 11.0
       ipg : 0.0001


*/

bool CFlowsYamlInfo::verify_correctness(uint32_t num_threads) {
    if ( m_tuple_gen_was_set ==false ){
        printf(" ERROR there must be a generator field in YAML , the old format is deprecated \n");
        printf(" This is not supported : \n");
        printf("       min_src_ip : 0x10000001  \n");
        printf("       max_src_ip : 0x50000001  \n");
        printf("       min_dst_ip : 0x60000001  \n");
        printf("       max_dst_ip : 0x60000010  \n");
        printf(" This is  supported : \n");
        printf("generator :  \n");
        printf("   distribution : \"seq\"         \n");
        printf("   clients_start : \"16.0.0.1\"   \n");
        printf("   clients_end   : \"16.0.1.255\" \n");
        printf("   servers_start : \"48.0.0.1\"   \n");
        printf("   servers_end   : \"48.0.0.255\" \n");
        printf("   clients_per_gb : 201           \n");
        printf("   min_clients    : 101           \n");
        printf("   dual_port_mask : \"1.0.0.0\"   \n");
        printf("   tcp_aging      : 1             \n");
        printf("   udp_aging      : 1             \n");
        return(false);
    }
    if ( !m_tuple_gen.is_valid(num_threads,is_any_plugin_configured()) ){
        return (false);
    }
    /* patch defect trex-54 */
    if ( is_any_plugin_configured() ){
         /*Plugin is configured. in that case due to a limitation ( defect trex-54 )
          the number of servers should be bigger than number of clients   */

        int i;
        for (i=0; i<(int)m_vec.size(); i++) {
            CFlowYamlInfo * lp=&m_vec[i];
            if ( lp->m_plugin_id ){
                uint8_t c_idx = lp->m_client_pool_idx;
                uint8_t s_idx = lp->m_server_pool_idx;
                uint32_t total_clients = m_tuple_gen.m_client_pool[c_idx].getTotalIps();
                uint32_t total_servers = m_tuple_gen.m_server_pool[s_idx].getTotalIps();
                if ( total_servers < total_clients ){
                    printf(" Plugin is configured. in that case due to a limitation ( defect trex-54 ) \n");
                    printf(" the number of servers should be bigger than number of clients  \n");
                    printf(" client_pool_name : %s \n", lp->m_client_pool_name.c_str());
                    printf(" server_pool_name : %s \n", lp->m_server_pool_name.c_str());
                    return (false);
                }
            uint32_t mul = total_servers / total_clients;
            uint32_t new_server_num = mul * total_clients;
            if ( new_server_num != total_servers ) {
                printf(" Plugin is configured. in that case due to a limitation ( defect trex-54 ) \n");
                printf(" the number of servers should be exact multiplication of the number of clients  \n");
                printf(" client_pool_name : %s  clients %d \n", lp->m_client_pool_name.c_str(),total_clients);
                printf(" server_pool_name : %s  servers %d should be %d \n", lp->m_server_pool_name.c_str(),total_servers,new_server_num);
                return (false);
            }
          }
        }
    }

    return(true);
}



void CFlowsYamlInfo::set_astf_mode(){
    m_vec.clear();
    m_tw.reset();

    if (CGlobalInfo::m_options.m_duration > 0.1) {
        m_duration_sec = CGlobalInfo::m_options.m_duration;
    } else {
        m_duration_sec = 0;
    }
    
    m_is_plugin_configured=false;
}


int CFlowsYamlInfo::load_from_yaml_file(std::string file_name){
    m_vec.clear();
    m_tw.reset();

    if ( !utl_is_file_exists (file_name)  ){
        printf(" ERROR file %s does not exist \n",file_name.c_str());
        exit(-1);
    }

    try {
       std::ifstream fin((char *)file_name.c_str());
       YAML::Parser parser(fin);
       YAML::Node doc;

       parser.GetNextDocument(doc);
       for(unsigned i=0;i<doc.size();i++) {
          doc[i] >> *this;
          break;
       }
    } catch ( const std::exception& e ) {
        printf("Error parsing file %s as YAML:\n", file_name.c_str());
        std::cout << e.what() << "\n";
        exit(-1);
    }

    /* update from user input */
    if (CGlobalInfo::m_options.m_duration > 0.1) {
        m_duration_sec = CGlobalInfo::m_options.m_duration;
    }
    int i;
    m_is_plugin_configured=false;
    for (i=0; i<(int)m_vec.size(); i++) {
      m_vec[i].m_k_cps =m_vec[i].m_k_cps*CGlobalInfo::m_options.m_factor;
      if (( ! m_vec[i].m_cap_mode_was_set  ) && (m_cap_mode_set ) ){
          m_vec[i].m_cap_mode = m_cap_mode;
      }

      if (( ! m_vec[i].m_one_app_server_was_set  ) && (m_one_app_server_was_set ) ){
          m_vec[i].m_one_app_server = m_one_app_server;
      }

      if ( m_cap_overide_ipg_set ){
          m_vec[i].m_ipg_sec  = m_cap_overide_ipg;
          m_vec[i].m_rtt_sec  = m_cap_overide_ipg;
      }

      if ( m_vec[i].m_plugin_id ){
          m_is_plugin_configured=true;
      }
    }

    if ( m_tw.m_info_exist ){

        CTimerWheelYamlInfo *lp=&m_tw;
        std::string  err;
        if (!lp->Verify(err)){
            std::cout << err << "\n";
            exit(-1);
        }

        CParserOption* po = &CGlobalInfo::m_options;
        po->set_tw_bucket_time_in_usec(lp->m_bucket_time_usec);
        po->set_tw_buckets(lp->m_buckets);
        po->set_tw_levels(lp->m_levels);
    }
   return 0;
}



void CFlowStats::Clear(){

    m_id=0;
    m_name="";
    m_pkt=0.0;
    m_bytes=0.0;
    m_cps=0.0;
    m_mb_sec=0.0;
    m_mB_sec=0.0;
    m_c_flows=0.0;
    m_pps =0.0;
    m_errors =0;
    m_flows =0 ;
    m_memory.clear();
    duration_sec=0.0;
}

void CFlowStats::Add(const CFlowStats & obj){

    m_pkt     += obj.m_pkt     ;
    m_bytes   += obj.m_bytes   ;
    m_cps     += obj.m_cps     ;
    m_mb_sec  += obj.m_mb_sec  ;
    m_mB_sec  += obj.m_mB_sec  ;
    m_c_flows += obj.m_c_flows ;
    m_pps     += obj.m_pps     ;
    m_errors  +=obj.m_errors;
    m_flows   +=obj.m_flows ;
    duration_sec += obj.duration_sec;

    m_memory.Add(obj.m_memory);
}


void CFlowStats::DumpHeader(FILE *fd){
    fprintf(fd," %2s %-40s %-8s %-9s %-6s %-8s %-8s %-7s %-6s %-7s %-6s %-4s %-4s\n",
                 "id","name","tps","cps","f-pkts","f-bytes","duration","Mb/sec","MB/sec","c-flows","PPS","errors","flows");
}
void CFlowStats::Dump(FILE *fd){
    fprintf(fd," %02d %-40s %8.2f %9.2f %6.0f %8.0f %8.2f %7.2f %6.2f %7.0f %6.0f %4llu %4llu\n",
            m_id,
            m_name.c_str(),
            m_cps,
            get_normal_cps(),
            m_pkt,
            m_bytes,
            duration_sec,
            m_mb_sec,
            m_mB_sec,
            m_c_flows,
            m_pps,
            (unsigned long long)m_errors,
            (unsigned long long)m_flows);
}

bool CFlowGeneratorRecPerThread::Create(CTupleGeneratorSmart  * global_gen,
                                        CFlowYamlInfo *         info,
                                        CFlowsYamlInfo *        yaml_flow_info,
                                        CCapFileFlowInfo *      flow_info,
                                        uint16_t _id,
                                        uint32_t thread_id){

    BP_ASSERT(info);
    m_thread_id =thread_id ;

    tuple_gen.Create(global_gen, info->m_client_pool_idx,
                     info->m_server_pool_idx);
    CTupleGenYamlInfo * lpt;
    lpt = &yaml_flow_info->m_tuple_gen;

    tuple_gen.SetSingleServer(info->m_one_app_server,
                              info->m_server_addr,
                              getDualPortId(thread_id),
                              lpt->m_client_pool[info->m_client_pool_idx].getDualMask()
                              );

    tuple_gen.SetW(info->m_w);



    m_id   =_id;
    m_info =info;
    m_flows_info = yaml_flow_info;
    // set policer give bucket size for bursts
    m_policer.set_cir(info->m_k_cps*1000.0);
    m_policer.set_level(0.0);
    m_policer.set_bucket_size(100.0);
    /* pointer to global */
    m_flow_info = flow_info;
    return (true);
}


void CFlowGeneratorRecPerThread::Delete(){
    tuple_gen.Delete();
}




void CFlowGeneratorRecPerThread::Dump(FILE *fd){
    fprintf(fd," configuration info ");
    fprintf(fd," -----------------");
    m_info->Dump(fd);
    fprintf(fd," -----------------");
    m_flow_info->Dump(fd);
}


void CFlowGeneratorRecPerThread::getFlowStats(CFlowStats * stats){

    double t_pkt=(double)m_flow_info->Size();
    double t_bytes=(double)m_flow_info->get_total_bytes();
    double cps=m_info->m_k_cps *1000.0;
    double mb_sec   = (cps*t_bytes*8.0)/(_1Mb_DOUBLE);
    double mB_sec   = (cps*t_bytes)/(_1Mb_DOUBLE);

    double c_flow_windows_sec=0.0;

    if (m_info->m_cap_mode) {
        c_flow_windows_sec  = m_flow_info->get_cap_file_length_sec();
    }else{
        c_flow_windows_sec  = t_pkt * m_info->m_ipg_sec;
    }


    double c_flows  = cps*c_flow_windows_sec*m_flow_info->get_total_flows();
    double pps =cps*t_pkt;
    uint64_t errors = m_flow_info->get_total_errors();
    uint64_t flows  = m_flow_info->get_total_flows();


    stats->m_id     = m_id;
    stats->m_pkt    = t_pkt;
    stats->m_bytes  = t_bytes;
    stats->duration_sec =  c_flow_windows_sec;
    stats->m_name   = m_info->m_name.c_str();
    stats->m_cps    =  cps;
    stats->m_mb_sec =  mb_sec;
    stats->m_mB_sec =  mB_sec;
    stats->m_c_flows  =  c_flows;
    stats->m_pps    =  pps;
    stats->m_errors    =  errors;
    stats->m_flows    =  flows;
}



void CFlowGeneratorRec::Dump(FILE *fd){
    fprintf(fd," configuration info ");
    fprintf(fd," -----------------");
    m_info->Dump(fd);
    fprintf(fd," -----------------");
    m_flow_info.Dump(fd);
}


void CFlowGeneratorRec::updateIpg(double factor){
    m_flow_info.update_ipg_by_factor(factor,m_info);
}



void CFlowGeneratorRec::getFlowStats(CFlowStats * stats){

    double t_pkt=(double)m_flow_info.Size();
    double t_bytes=(double)m_flow_info.get_total_bytes();
    double cps=m_info->m_k_cps *1000.0;
    double mb_sec   = (cps*t_bytes*8.0)/(_1Mb_DOUBLE);
    double mB_sec   = (cps*t_bytes)/(_1Mb_DOUBLE);

    double c_flow_windows_sec;

    c_flow_windows_sec  = m_flow_info.get_cap_file_length_sec();

    m_flow_info.get_total_memory(stats->m_memory);


    double c_flows  = cps*c_flow_windows_sec;
    double pps =cps*t_pkt;
    uint64_t errors = m_flow_info.get_total_errors();
    uint64_t flows  = m_flow_info.get_total_flows();


    stats->m_id     = m_id;
    stats->m_pkt    = t_pkt;
    stats->m_bytes  = t_bytes;
    stats->duration_sec =  c_flow_windows_sec;
    stats->m_name   = m_info->m_name.c_str();
    stats->m_cps    =  cps;
    stats->m_mb_sec =  mb_sec;
    stats->m_mB_sec =  mB_sec;
    stats->m_c_flows  =  c_flows;
    stats->m_pps    =  pps;
    stats->m_errors    =  errors;
    stats->m_flows    =  flows;
}


void CFlowGeneratorRec::fixup_ipg_if_needed(void){
    if  ( m_flows_info->m_cap_mode ) {
        m_flow_info.update_pcap_mode();
    }

    if ( (m_flows_info->m_cap_mode) &&
         (m_flows_info->m_cap_ipg_min_set) &&
         (m_flows_info->m_cap_overide_ipg_set)
         ){
        m_flow_info.update_min_ipg(m_flows_info->m_cap_ipg_min,
                                   m_flows_info->m_cap_overide_ipg);
    }
}


bool CFlowGeneratorRec::Create(CFlowYamlInfo * info,
                               CFlowsYamlInfo * flows_info,
                               uint16_t _id){
    BP_ASSERT(info);
    m_id=_id;
    m_info=info;
    m_flows_info=flows_info;
    m_flow_info.Create();

    // set policer give bucket size for bursts
    m_policer.set_cir(info->m_k_cps*1000.0);
    m_policer.set_level(0.0);
    m_policer.set_bucket_size(100.0);

    int res=m_flow_info.load_cap_file(info->m_name.c_str(),_id,m_info->m_plugin_id);
    if ( res==0 ) {
        fixup_ipg_if_needed();

        if (m_flow_info.is_valid_template_load_time() != 0) {
            return (false);
        }
        m_flow_info.update_info(m_info);
        return (true);
    }else{
        return (false);
    }
}

void CFlowGeneratorRec::Delete(){
    m_flow_info.Delete();
}


void CGenNode::DumpHeader(FILE *fd){
    fprintf(fd," pkt_id,time,fid,pkt_info,pkt,len,type,is_init,is_last,type,thread_id,src_ip,dest_ip,src_port \n");
}


void CGenNode::free_gen_node(){
    rte_mbuf_t * m=get_cache_mbuf();
    if ( unlikely(m != NULL) ) {
        rte_pktmbuf_free(m);
        m_plugin_info=0;
    }
}


void CGenNode::Dump(FILE *fd){
    fprintf(fd,"%.6f,%llx,%p,%llu,%d,%d,%d,%d,%d,%d,%x,%x,%d\n",
            m_time,
            (unsigned long long)m_flow_id,
            m_pkt_info,
            (unsigned long long)m_pkt_info->m_pkt_indication.m_packet->pkt_cnt,
            m_pkt_info->m_pkt_indication.m_packet->pkt_len,
            m_pkt_info->m_pkt_indication.m_desc.getId(),
            (m_pkt_info->m_pkt_indication.m_desc.IsInitSide()?1:0),
            m_pkt_info->m_pkt_indication.m_desc.IsLastPkt(),
            m_type,
            m_thread_id,
            m_src_ip,
            m_dest_ip,
            m_src_port);

}

void  CNodeGenerator::set_vif(CVirtualIF * v_if){
    m_v_if = v_if;
}

bool  CNodeGenerator::Create(CFlowGenListPerThread  *  parent){
    m_v_if =0;
    m_parent=parent;
    m_socket_id =0;
    m_realtime_his.Create();
    m_last_sync_time_sec = 0;
    m_tw_level1_next_sec = 0;

    if ( CGlobalInfo::m_options.m_is_sleepy_scheduler ) { // measure nanosleep overhead
        int measure_times = 20;
        int start_measure_time = now_sec();
        for (int i = 1; i <= measure_times; i++) {
            delay_sec(0);
        }
        nanosleep_overhead = (now_sec() - start_measure_time) / measure_times;
    }

    return(true);
}

void  CNodeGenerator::Delete(){
    m_realtime_his.Delete();
}


void  CNodeGenerator::add_node(CGenNode * mynode){
    m_p_queue.push(mynode);
}



void CNodeGenerator::remove_all(CFlowGenListPerThread * thread){
    CGenNode *node;
    while (!m_p_queue.empty()) {
        node = m_p_queue.top();
        m_p_queue.pop();
        /* sanity check */
        if (node->m_type == CGenNode::STATELESS_PKT) {
            CGenNodeStateless * p=(CGenNodeStateless *)node;
            /* need to be changed in Pause support */
            assert(p->is_mask_for_free());
        }

        thread->free_node( node);
    }
}

int CNodeGenerator::open_file(std::string file_name,
                              CPreviewMode * preview_mode){
    BP_ASSERT(m_v_if);
    m_preview_mode =*preview_mode;
    /* ser preview mode */
    m_v_if->set_review_mode(preview_mode);
    m_v_if->open_file(file_name);
    m_cnt   = 0;
    m_non_active = 0;
    m_limit = 0;
    return (0);
}


int CNodeGenerator::close_file(CFlowGenListPerThread * thread){
    remove_all(thread);
    BP_ASSERT(m_v_if);
    m_v_if->close_file();
    return (0);
}

int CNodeGenerator::update_stl_stats(CGenNodeStateless *node_sl){
    m_cnt++;
    if (!node_sl->is_node_active()) {
        m_non_active++;
    }
    #ifdef _DEBUG
    if ( m_preview_mode.getVMode() >2 ){
        fprintf(stdout," %4lu ,", (ulong)m_cnt);
        fprintf(stdout," %4lu ,", (ulong)m_non_active);
        node_sl->Dump(stdout);
    }
    #endif

    return (0);
}


int  CNodeGenerator::update_stats(CGenNode * node){
    if ( m_preview_mode.getVMode() >2 ){
        fprintf(stdout," %llu ,", (unsigned long long)m_cnt);
        node->Dump(stdout);
        m_cnt++;
    }
    return (0);
}


bool CNodeGenerator::has_limit_reached() {
    /* do we have a limit and has it passed ? */
    return ( (m_limit > 0) && (m_cnt >= m_limit) );
}

uint8_t CFlowGenListPerThread::get_memory_socket_id(){ 
    uint8_t p1;
    uint8_t p2;
    get_port_ids(p1,p2);
    return (CGlobalInfo::m_socket.port_to_socket((port_id_t)p1));
}

/* the expected number of flows for TCP mode, this is taken from configuration file and div by threads*/
uint32_t CFlowGenListPerThread::get_max_active_flows_per_core_tcp(){ 

    double active_flows_per_core = (double)CGlobalInfo::m_memory_cfg.get_each_core_dp_flows()/(double)m_max_threads;
    /* can't have more than 20M flows per core ..*/
    if (active_flows_per_core>20*1e6) {
        printf("ERROR something went wrong here, more than 20M flows per core does not make sense (%f) \n",active_flows_per_core);
        assert(0);
    }
    return((uint32_t)active_flows_per_core);
}


bool CFlowGenListPerThread::Create(uint32_t           thread_id,
                                   CFlowGenList  *    flow_list,
                                   uint32_t           max_threads){


    m_sync_b= (CSyncBarrier *)0;
    m_non_active_nodes = 0;
    m_terminated_by_master=false;
    m_flow_list =flow_list;
    m_tcp_dpc= 0;
    m_udp_dpc=0;
    m_max_threads=max_threads;
    m_thread_id=thread_id;

    m_c_tcp=0;
    m_c_tcp_io =0;
    m_s_tcp=0;
    m_s_tcp_io=0;
    m_tcp_terminate=false;
    m_tcp_terminate_cnt=0;
    m_sched_accurate=false;

    m_cpu_cp_u.Create(&m_cpu_dp_u);

    uint8_t socket_id = get_memory_socket_id();

    char name[100];
    sprintf(name,"nodes-%d",m_thread_id);

    unsigned flow_nodes = CGlobalInfo::m_memory_cfg.get_each_core_dp_flows() ;
    if (get_is_tcp_mode()) {
        flow_nodes = 1024; /* No need for many nodes, it handles in different ways */
    }

    bool use_hugepages = CGlobalInfo::m_options.m_is_vdev ? false : true;
    m_node_pool = utl_rte_mempool_create_non_pkt(name,
                                                 flow_nodes,
                                                 sizeof(CGenNode),
                                                 128,
                                                 socket_id,
                                                 false,
                                                 use_hugepages);

     RC_HTW_t tw_res=m_tw.Create(TW_BUCKETS,TW_BUCKETS_LEVEL1_DIV); 
     if (tw_res != RC_HTW_OK){
         CHTimerWheelErrorStr err(tw_res);
         printf("Timer wheel configuration error,please look into the manual for details  \n");
         printf("ERROR  %-30s  - %s \n",err.get_str(),err.get_help_str());
         exit(1);  
     }

    m_node_gen.Create(this);
    m_node_gen.m_socket_id =socket_id;
    m_flow_id_to_node_lookup.Create();

    /* split the clients to threads */

    if (! get_is_tcp_mode()) {

        CTupleGenYamlInfo * tuple_gen = &m_flow_list->m_yaml_info.m_tuple_gen;

        double active_flows_per_core = flow_list->get_worse_case_active_flows()/(double)m_max_threads;

        m_smart_gen.Create(0,m_thread_id);

        /* split the clients to threads using the mask */
        CIpPortion  portion;
        for (int i=0;i<tuple_gen->m_client_pool.size();i++) {
            split_ips(m_thread_id, m_max_threads, getDualPortId(),
                      tuple_gen->m_client_pool[i],
                      portion);

            m_smart_gen.add_client_pool(tuple_gen->m_client_pool[i].m_dist,
                                        portion.m_ip_start,
                                        portion.m_ip_end,
                                        active_flows_per_core,
                                        m_flow_list->m_client_config_info,
                                        tuple_gen->m_client_pool[i].m_tcp_aging_sec,
                                        tuple_gen->m_client_pool[i].m_udp_aging_sec
                                        );
        }
        for (int i=0;i<tuple_gen->m_server_pool.size();i++) {
            split_ips(m_thread_id, m_max_threads, getDualPortId(),
                      tuple_gen->m_server_pool[i],
                      portion);
            m_smart_gen.add_server_pool(tuple_gen->m_server_pool[i].m_dist,
                                        portion.m_ip_start,
                                        portion.m_ip_end,
                                        active_flows_per_core,
                                        tuple_gen->m_server_pool[i].m_is_bundling);
        }

        init_from_global();
    }else{
        m_yaml_info =m_flow_list->m_yaml_info;
    }

    CMessagingManager * rx_dp=CMsgIns::Ins()->getRxDp();

    m_ring_from_rx = rx_dp->getRingCpToDp(thread_id);
    m_ring_to_rx   = rx_dp->getRingDpToCp(thread_id);

    assert(m_ring_from_rx);
    assert(m_ring_to_rx);

    /* will be set by start */
    m_dp_core = get_stx()->create_dp_core(thread_id, this);
    
    return (true);
}

/* return  the client ip , port */
FORCE_NO_INLINE void CFlowGenListPerThread::handler_defer_job(CGenNode *p){
    CGenNodeDeferPort     *   defer=(CGenNodeDeferPort     *)p;
    int i;
    for (i=0; i<defer->m_cnt; i++) {
        m_smart_gen.FreePort(defer->m_pool_idx[i],
                                 defer->m_clients[i],defer->m_ports[i]);
    }
}



void CFlowGenListPerThread::defer_client_port_free(bool is_tcp,
                                                   uint32_t c_idx,
                                                   uint16_t port,
                                                   pool_index_t c_pool_idx,
                                                   CTupleGeneratorSmart * gen){
    /* free is not required in this case */
    if (!gen->IsFreePortRequired(c_pool_idx) ){
        return;
    }
    CGenNodeDeferPort     *   defer;
    if (is_tcp) {
        if (gen->get_tcp_aging(c_pool_idx)==0) {
            gen->FreePort(c_pool_idx,c_idx,port);
            return;
        }
        defer=get_tcp_defer();
    }else{
        if (gen->get_udp_aging(c_pool_idx)==0) {
            gen->FreePort(c_pool_idx, c_idx,port);
            return;
        }
        defer=get_udp_defer();
    }
    if ( defer->add_client(c_pool_idx, c_idx,port) ){
        if (is_tcp) {
            m_node_gen.schedule_node((CGenNode *)defer,gen->get_tcp_aging(c_pool_idx));
            m_tcp_dpc=0;
        }else{
            m_node_gen.schedule_node((CGenNode *)defer,gen->get_udp_aging(c_pool_idx));
            m_udp_dpc=0;
        }
    }
}


void CFlowGenListPerThread::defer_client_port_free(CGenNode *p){
    defer_client_port_free(p->m_pkt_info->m_pkt_indication.m_desc.IsTcp(),
                           p->m_src_idx,p->m_src_port,p->m_template_info->m_client_pool_idx,
                           p->m_tuple_gen);
}



/* copy all info from global and div by num of threads */
void CFlowGenListPerThread::init_from_global(){
    /* copy generator , it is the same */
    m_yaml_info =m_flow_list->m_yaml_info;

    /* copy first the flow info */
    int i;
    for (i=0; i<(int)m_flow_list->m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_flow_list->m_cap_gen[i];
        CFlowGeneratorRecPerThread * lp_thread=new CFlowGeneratorRecPerThread();
        /* TBD leak of memory */
        CFlowYamlInfo *         yaml_info =new CFlowYamlInfo();

        yaml_info->m_name    = lp->m_info->m_name;
        yaml_info->m_k_cps   = lp->m_info->m_k_cps/(double)m_max_threads;
        yaml_info->m_ipg_sec = lp->m_info->m_ipg_sec;
        yaml_info->m_rtt_sec = lp->m_info->m_rtt_sec;
        yaml_info->m_w   = lp->m_info->m_w;
        yaml_info->m_cap_mode =lp->m_info->m_cap_mode;
        yaml_info->m_plugin_id = lp->m_info->m_plugin_id;
        yaml_info->m_one_app_server = lp->m_info->m_one_app_server;
        yaml_info->m_server_addr = lp->m_info->m_server_addr;
        yaml_info->m_dpPkt          =lp->m_info->m_dpPkt;
        yaml_info->m_server_pool_idx=lp->m_info->m_server_pool_idx;
        yaml_info->m_client_pool_idx=lp->m_info->m_client_pool_idx;
        yaml_info->m_server_pool_name=lp->m_info->m_server_pool_name;
        yaml_info->m_client_pool_name=lp->m_info->m_client_pool_name;
        /* fix this */
        assert(m_max_threads>0);
        if ( m_max_threads == 1 ) {
            /* we have one thread the limit */
            yaml_info->m_limit         = lp->m_info->m_limit;
        }else{
            yaml_info->m_limit = lp->m_info->m_limit/m_max_threads;
            /* thread is zero base */
            if ( m_thread_id == 0){
                yaml_info->m_limit += lp->m_info->m_limit % m_max_threads;
            }
            if (yaml_info->m_limit==0) {
                yaml_info->m_limit=1;
            }
        }

        yaml_info->m_limit_was_set = lp->m_info->m_limit_was_set;
        yaml_info->m_flowcnt       = 0;
        yaml_info->m_restart_time = ( yaml_info->m_limit_was_set ) ?
            (yaml_info->m_limit / (yaml_info->m_k_cps * 1000.0)) : 0;

        lp_thread->Create(&m_smart_gen,
                           yaml_info,
                           lp->m_flows_info,
                           &lp->m_flow_info,
                           lp->m_id,
                           m_thread_id);

        m_cap_gen.push_back(lp_thread);
    }
}

static void free_map_flow_id_to_node(CGenNode *p){
    CGlobalInfo::free_node(p);
}


void CFlowGenListPerThread::Delete(){
    // free all current maps
    m_flow_id_to_node_lookup.remove_all(free_map_flow_id_to_node);
    // free object
    m_flow_id_to_node_lookup.Delete();

    m_smart_gen.Delete();
    m_node_gen.Delete();
    Clean();
    m_cpu_cp_u.Delete();
    m_tw.Delete();

    utl_rte_mempool_delete(m_node_pool);
    
    if (m_dp_core) {
        delete m_dp_core;
        m_dp_core = nullptr;
    }
}



void CFlowGenListPerThread::Clean(){
    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread * lp=m_cap_gen[i];
        if (lp->m_tuple_gen_was_set) {
            CTupleGeneratorSmart *gen;
            gen = lp->tuple_gen.get_gen();
            gen->Delete();
            delete gen;
        }
        lp->Delete();
        delete lp;
    }
    m_cap_gen.clear();
}

//uint64_t _start_time;

void CNodeGenerator::dump_json(std::string & json){

    json="{\"name\":\"tx-gen\",\"type\":0,\"data\":{";
    m_realtime_his.dump_json("realtime-hist",json);
    json+="\"unknown\":0}}" ;
}

void CNodeGenerator::add_exit_node(CFlowGenListPerThread * thread,
                                  dsec_t max_time){

    if ( max_time > 0 ) {
        CGenNode *exit_node = thread->create_node();
        exit_node->m_type = CGenNode::EXIT_SCHED;
        exit_node->m_time = max_time;
        add_node(exit_node);
    }
}

inline bool CNodeGenerator::handle_stl_node(CGenNode * node,
                                             CFlowGenListPerThread * thread){
    uint8_t type=node->m_type;

    if ( likely( type == CGenNode::STATELESS_PKT ) ) {
       m_p_queue.pop();
       CGenNodeStateless *node_sl = (CGenNodeStateless *)node;
        /* if the stream has been deactivated - end */
        if ( unlikely( node_sl->is_mask_for_free() ) ) {
            thread->free_node(node);
        } else {
            /* count before handle - node might be destroyed */
            #ifdef TREX_SIM
            uint8_t port_id = node_sl->get_port_id();
            uint32_t profile_id = node_sl->get_profile_id();
            update_stl_stats(node_sl);
            #endif

            /**** WARNING - after this call the node_sl might be destroyed *****/
            node_sl->handle(thread);

            #ifdef TREX_SIM
            if (has_limit_reached()) {
                ((TrexStatelessDpCore *)thread->m_dp_core)->stop_traffic(port_id, profile_id, false, 0);
            }
            #endif
        }
        return (true);
    }
    return(false);
}



#define unsafe_container_of(var,ptr, type, member)              \
    ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))


/*TEARDOWN is true for stateful in second phase we wait for all the flow to finish
with --nc there is no TEARDOWN

first phase ==> TEARDOWN =false
last phase ==> TEARDOWN  =true

this is relevant for repeatable flows
*/

template<bool TEARDOWN>
inline void CFlowGenListPerThread::on_flow_tick(CGenNode *node){

    #ifdef TREX_SIM
    /* for sim, we need to update the node from tick time*/
    if ( likely (!node->is_repeat_flow() ) ){
        node->m_time=m_cur_time_sec; 
    }
    #endif
    #ifdef _DEBUG
    m_node_gen.update_stats(node);
    #endif
    m_node_gen.flush_one_node_to_file(node);

    if ( likely (!node->is_repeat_flow()) ) {
        if ( likely (!node->is_last_in_flow()) ) {
            m_tw.timer_start(&node->m_tmr,node->update_next_pkt_in_flow_tw() );
        }else{                              
            free_last_flow_node(node);
        }
    }else{
        /* repeatable flow, we need to stop it in case of repeat */
        if ( node->is_last_in_flow() ) {

            if ( TEARDOWN == false ){
                reschedule_flow(node); 
            }else{
                free_last_flow_node(node);
            }

        }else{
            /* need to update both accurate time and bucket time in this case update_next_pkt_in_flow_both() for reschedule to be accurate */
            m_tw.timer_start(&node->m_tmr,node->update_next_pkt_in_flow_both() );
        }
    }
}



static void tw_free_node(void *userdata,
                       CHTimerObj *tmr){
    CFlowGenListPerThread * thread=(CFlowGenListPerThread * )userdata;
    UNSAFE_CONTAINER_OF_PUSH;
    CGenNode * node=unsafe_container_of(node,tmr,CGenNode,m_tmr);
    UNSAFE_CONTAINER_OF_POP;
    if (node->is_flow_node()) {
        /* this is a flow*/
        thread->free_last_flow_node(node);
    }else{
        thread->free_node(node);
    }
}


static void tw_on_tick_per_thread_cb_always(void *userdata,
                                            CHTimerObj *tmr){
    CFlowGenListPerThread * thread=(CFlowGenListPerThread * )userdata;
    UNSAFE_CONTAINER_OF_PUSH;
    CGenNode * node=unsafe_container_of(node,tmr,CGenNode,m_tmr);
    UNSAFE_CONTAINER_OF_POP;

    thread->on_flow_tick<true>(node);
}


void tw_on_tick_per_thread_cb(void *userdata,
                                     CHTimerObj *tmr){
    CFlowGenListPerThread * thread=(CFlowGenListPerThread * )userdata;

    UNSAFE_CONTAINER_OF_PUSH;
    CGenNode * node=unsafe_container_of(node,tmr,CGenNode,m_tmr);
    UNSAFE_CONTAINER_OF_POP;

    thread->on_flow_tick<false>(node);
}


FORCE_NO_INLINE void CFlowGenListPerThread::handler_defer_job_flush(void){

    /* free the objects left in TW */
    m_tw.detach_all((void *)this,tw_free_node);

    /* flush the pending job of free ports */
    if (m_tcp_dpc) {
        handler_defer_job((CGenNode *)m_tcp_dpc);
        free_node((CGenNode *)m_tcp_dpc);
        m_tcp_dpc=0;
    }
    if (m_udp_dpc) {
        handler_defer_job((CGenNode *)m_udp_dpc);
        free_node((CGenNode *)m_udp_dpc);
        m_udp_dpc=0;
    }
}



inline bool CNodeGenerator::do_work_stl(CGenNode * node,
                                        CFlowGenListPerThread * thread,
                                        bool on_terminate){

    if ( handle_stl_node(node,thread)){
        return (false);
    }else{
        return (handle_slow_messages(node->m_type,node,thread,on_terminate));
    }
}




template<bool ON_TERMINATE>
inline bool CNodeGenerator::do_work_both(CGenNode * node,
                                         CFlowGenListPerThread * thread,
                                         dsec_t d_time){

    bool exit_scheduler=false;
    uint8_t type=node->m_type;
    bool done;

    if ( handle_stl_node (node,thread) ){
    }else{
        if ( likely( type == CGenNode::TW_SYNC )  ) {
            m_p_queue.pop();
            /* update bucket time */
            thread->m_cur_time_sec = node->m_time;
            if ( ON_TERMINATE ) {
                thread->m_tw.on_tick_level0((void*)thread,tw_on_tick_per_thread_cb_always);
                if ( thread->m_tw.is_any_events_left() ){
                    node->m_time += BUCKET_TIME_SEC;
                    m_p_queue.push(node);
                }else{
                    thread->free_node(node);
                }
            }else{
                thread->m_tw.on_tick_level0((void*)thread,tw_on_tick_per_thread_cb);
                node->m_time += BUCKET_TIME_SEC;;
                m_p_queue.push(node);
            }

        }else{

            if ( likely( type == CGenNode::FLOW_PKT ) ) {
                /* PKT */
                m_p_queue.pop();
                thread->on_flow_tick<ON_TERMINATE>(node);
            }else{
                if ((type == CGenNode::FLOW_FIF)) {
                   /* callback to our method */
                    m_p_queue.pop();
                    if ( ON_TERMINATE == false) {
                        thread->m_cur_time_sec = node->m_time ;

                        thread->generate_flows_roundrobin(&done);

                        if (!done) {
                            node->m_time +=d_time;
                            m_p_queue.push(node);
                        }else{
                            thread->free_node(node);
                        }
                    }else{
                        thread->free_node(node);
                    }

                }else{
                    exit_scheduler = handle_slow_messages(type,node,thread,ON_TERMINATE);
                }
            }
        }
    }

    return (exit_scheduler);
}



template<int SCH_MODE,bool ON_TERMINATE>
inline bool CNodeGenerator::do_work(CGenNode * node,
                                    CFlowGenListPerThread * thread,
                                    dsec_t d_time){
    /* template filter in compile time */
    if ( SCH_MODE == smSTATELESS  ) {
        return ( do_work_stl(node,thread,ON_TERMINATE) );
    }else{
        /* smSTATEFUL */
        return ( do_work_both<ON_TERMINATE>(node,thread,d_time) );
    }
}


inline void CNodeGenerator::do_sleep(dsec_t & cur_time,
                                     CFlowGenListPerThread * thread,
                                     dsec_t n_time){

    thread->m_cpu_dp_u.commit1();

    if (unlikely( CGlobalInfo::m_options.m_is_sleepy_scheduler )) {
        delay_sec(n_time - now_sec() - nanosleep_overhead);
        cur_time = now_sec();
    } else {
        dsec_t dt;
        while ( true ) {
            cur_time = now_sec();
            dt = cur_time - n_time ;
            if ( dt > WAIT_WINDOW_SIZE ) {
                break;
            }
            rte_pause();
        }
    }

    thread->m_cpu_dp_u.start_work1();
}


inline int CNodeGenerator::teardown(CFlowGenListPerThread * thread,
                                    bool on_terminate,
                                    double &old_offset,
                                    double offset){

    thread->m_cpu_dp_u.commit1();

        /* to do */
    if ( thread->is_terminated_by_master() ) {
        return (0);
    }

    if (!on_terminate) {
        old_offset =offset;
    }else{
        // free the left other
        thread->handler_defer_job_flush();

    }
    return (0);
}



template<int SCH_MODE,bool ON_TERIMATE> HOT_FUNC
inline int CNodeGenerator::flush_file_realtime(dsec_t max_time,
                                               dsec_t d_time,
                                               CFlowGenListPerThread * thread,
                                               double &old_offset) {
    CGenNode * node;
    dsec_t offset=0.0;
    dsec_t cur_time;
    dsec_t n_time;
    if (ON_TERIMATE) {
         offset=old_offset;
    }else{
        add_exit_node(thread,max_time);
    }
    m_scheduler_offset = offset;

    thread->m_cpu_dp_u.start_work1();

    sch_state_t state = scINIT;
    node = m_p_queue.top();
    n_time = node->m_time + offset;
    cur_time = now_sec();

    while (state!=scTERMINATE) {

         switch (state) {
         case scINIT:
            cur_time = now_sec();
            {
                dsec_t dt = cur_time - n_time ;

                if (dt > BURST_OFFSET_DTIME) {
                    state = scSTRECH;
                } else if (dt > 0) {
                    state = scWORK;
                } else {
                    state = scWAIT;
                }

            }
            break;

         case scWORK:
            {
                int node_count = 0;
                do {

                    bool s=do_work<SCH_MODE,ON_TERIMATE>(node,thread,d_time);
                    if (s) { // can we remove this IF ?
                        state=scTERMINATE;
                        break;
                    }
                    node = m_p_queue.top();
                    n_time = node->m_time + offset;
                    node_count++;

                    /* we either out of the time frame or every 1024 nodes we get out for time checking */
                    if ( ( (n_time - cur_time) > EAT_WINDOW_DTIME ) || (node_count > 1024) ) {
                        state = scINIT;
                        break;
                    }

                } while (true);
                break;
            }

         case scWAIT:
                do_sleep(cur_time,thread,n_time); // estimate  loop
                state=scWORK;
                break;


         default:
             handle_slow_operations(state, node, cur_time, n_time, offset, thread);
             break;
        } /* switch */

    }/* while*/

    return (teardown(thread,ON_TERIMATE,old_offset,offset));
}


FORCE_NO_INLINE void CNodeGenerator::handle_slow_operations(sch_state_t &state,
                                                            CGenNode * &node,
                                                            dsec_t &cur_time,
                                                            dsec_t &n_time,
                                                            dsec_t &offset,
                                                            CFlowGenListPerThread *thread) {
    switch (state) {
    case scSTRECH:
        {
            handle_time_strech(node, cur_time, n_time, offset, thread);

            /* go back to work */
            state = scWORK;

        }
        break;

    default:
        assert(0);
    }

}

/**
 * when time is streched - the flow_sync node
 * might be postpond too much
 * this can result a watchdog crash and lack
 * of responsivness from the DP core
 * (no handling of messages)
 *
 * @author imarom (7/31/2016)
 *
 */
void CNodeGenerator::handle_time_strech(CGenNode * &node,
                                        dsec_t &cur_time,
                                        dsec_t &n_time,
                                        dsec_t &offset,
                                        CFlowGenListPerThread *thread) {


    /* fix the time offset */
    dsec_t dt = cur_time - n_time;
    offset += dt;
    /* set new offset */
    m_scheduler_offset = offset;

    /* check if flow sync message was delayed too much */
    if ( (cur_time - m_last_sync_time_sec) > SYNC_TIME_OUT ) {
        handle_maintenance(thread);

        /* re-read the top of the queue - it might have changed with messaging */
        node = m_p_queue.top();
        n_time = node->m_time + offset;
    }

}

int CNodeGenerator::flush_file_sim(dsec_t max_time,
                                   dsec_t d_time,
                                   bool on_terminate,
                                   CFlowGenListPerThread * thread,
                                   double &old_offset){
    CGenNode * node;

    if (!on_terminate) {
        add_exit_node(thread,max_time);
    }

    while (true) {
        node = m_p_queue.top();

        bool do_exit;
        if (on_terminate) {
            if ( get_is_stateless() ) {
                do_exit=do_work<smSTATELESS,true>(node,thread,d_time);
            }else{
                do_exit=do_work<smSTATEFUL,true>(node,thread,d_time);
            }
        }else{
            if ( get_is_stateless() ) {
                do_exit=do_work<smSTATELESS,false>(node,thread,d_time);
            }else{
                do_exit=do_work<smSTATEFUL,false>(node,thread,d_time);
            }
        }
        if ( do_exit ){
            break;
        }
    }
    return (teardown(thread,on_terminate,old_offset,0));
}

int CNodeGenerator::flush_file(dsec_t max_time,
                               dsec_t d_time,
                               bool on_terminate,
                               CFlowGenListPerThread * thread,
                               double &old_offset) {
    #ifdef TREX_SIM
      return ( flush_file_sim(max_time, d_time,on_terminate,thread,old_offset) );
    #else
      if (on_terminate) {
          if ( get_is_stateless() ) {
              return ( flush_file_realtime<smSTATELESS,true>(max_time, d_time,thread,old_offset) );
          }else{
              return ( flush_file_realtime<smSTATEFUL,true>(max_time, d_time,thread,old_offset) );
          }
      }else{
          if ( get_is_stateless() ) {
              return ( flush_file_realtime<smSTATELESS,false>(max_time, d_time,thread,old_offset) );
          }else{
              return ( flush_file_realtime<smSTATEFUL,false>(max_time, d_time,thread,old_offset) );
          }
      }

    #endif
}


void CNodeGenerator::handle_batch_tw_level1(CGenNode *node, 
                                            CFlowGenListPerThread *thread,
                                            bool &exit_scheduler,
                                            bool on_terminate) {

    m_p_queue.pop();
    /* update bucket time */
    thread->m_cur_time_sec = node->m_time;

    bool stop_loop=false;

    while (!stop_loop) {
        na_htw_state_num_t tw_state = thread->m_tw.on_tick_level1((void*)thread,tw_on_tick_per_thread_cb);
        if ( (tw_state == TW_FIRST_FINISH) || (tw_state == TW_FIRST_FINISH_ANY)){
            node->m_time += BUCKET_TIME_SEC_LEVEL1;
            stop_loop=true;
        }else{
            switch (tw_state) {
            case TW_FIRST_BATCH:
                m_tw_level1_next_sec = node->m_time + BUCKET_TIME_SEC_LEVEL1;
                node->m_time = now_sec()-m_scheduler_offset; /* spread if we can */
                if (m_tw_level1_next_sec+m_scheduler_offset > now_sec() ) {
                    stop_loop=true;
                }
                break;
            case TW_NEXT_BATCH :
                node->m_time = now_sec()-m_scheduler_offset; /* spread if we can */
                if (m_tw_level1_next_sec+m_scheduler_offset > now_sec() ) {
                    stop_loop=true;
                }
                break;
            case TW_END_BATCH:
                if (m_tw_level1_next_sec+m_scheduler_offset > now_sec() ) {
                    node->m_time = m_tw_level1_next_sec;
                }else{
                    node->m_time = m_tw_level1_next_sec;  /* too late but we don't have anyting to do  */
                }
                stop_loop=true;
              break;
            default:
                assert(0);
            };
        }
    }

    if ( on_terminate &&  
         (thread->m_tw.is_any_events_left()==false) ){
        thread->free_node(node);
    }else{
        m_p_queue.push(node);
    }
}


void CNodeGenerator::handle_flow_pkt(CGenNode *node, CFlowGenListPerThread *thread) {

    /*repeat and NAT is not supported together */
    if ( node->is_nat_first_state()  ) {
        node->set_nat_wait_state();
        flush_one_node_to_file(node);
        UPDATE_STATS(node);
    } else {
        if ( node->is_nat_wait_state() ) {
            if (node->is_responder_pkt()) {
                m_p_queue.pop();
                /* time out, need to free the flow and remove the association , we didn't get conversion yet*/
                thread->terminate_nat_flows(node);
                return;

            } else {
                flush_one_node_to_file(node);
                UPDATE_STATS(node);
            }
        } else {
            if ( node->is_nat_wait_ack_state() ) {
                if (node->is_initiator_pkt()) {
                    m_p_queue.pop();
                    /* time out, need to free the flow and remove the association , we didn't get conversion yet*/
                    thread->terminate_nat_flows(node);
                    return;

                } else {
                    flush_one_node_to_file(node);
                    UPDATE_STATS(node);
                }
            } else {
                assert(0);
            }
        }
    }
    m_p_queue.pop();
    if ( node->is_last_in_flow() ) {
        thread->free_last_flow_node(node);
    } else {
        node->update_next_pkt_in_flow_as();
        m_p_queue.push(node);
    }
}

void CNodeGenerator::handle_flow_sync(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler) {


    /* flow sync message is a sync point for time */
    thread->m_cur_time_sec = node->m_time;

    /* first pop the node */
    m_p_queue.pop();

    /* call all the maintenance required */
    handle_maintenance(thread);

    /* exit in case this is the last node*/
    if ( m_p_queue.size() == m_parent->m_non_active_nodes ) {
        thread->free_node(node);
        exit_scheduler = true;
    } else {
        /* schedule for next maintenace */
        node->m_time += SYNC_TIME_OUT;
        m_p_queue.push(node);
    }

}

void
CNodeGenerator::handle_maintenance(CFlowGenListPerThread *thread) {

    /* tickle and check messages */
    thread->tickle();
    thread->check_msgs();

    m_v_if->flush_tx_queue(); /* flush pkt each timeout */

    /* save last sync time as realtime */
    m_last_sync_time_sec = now_sec();
}


void CNodeGenerator::handle_command(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler) {
    m_p_queue.pop();
    CGenNodeCommand *node_cmd = (CGenNodeCommand *)node;
    TrexCpToDpMsgBase * cmd=node_cmd->m_cmd;
    cmd->handle(thread->m_dp_core);
    exit_scheduler = cmd->is_quit();
    thread->free_node((CGenNode *)node_cmd);/* free the node */
}

void CNodeGenerator::handle_pcap_pkt(CGenNode *node, CFlowGenListPerThread *thread) {
    m_p_queue.pop();

    CGenNodePCAP *node_pcap = (CGenNodePCAP *)node;

    /* might have been marked for free */
    if ( unlikely( node_pcap->is_marked_for_free() ) ) {
        thread->free_node(node);
    } else {
        node_pcap->handle(thread);
    }
}

bool
CNodeGenerator::handle_slow_messages(uint8_t type,
                                     CGenNode * node,
                                     CFlowGenListPerThread * thread,
                                     bool on_terminate){

    /* should we continue after */
    bool exit_scheduler = false;

    switch (type) {
    case CGenNode::PCAP_PKT:
        handle_pcap_pkt(node, thread);
        break;

    case CGenNode::FLOW_DEFER_PORT_RELEASE:
        m_p_queue.pop();
        thread->handler_defer_job(node);
        thread->free_node(node);
        break;

    case CGenNode::FLOW_PKT_NAT:
        handle_flow_pkt(node, thread);
        break;

    case CGenNode::FLOW_SYNC:
        handle_flow_sync(node, thread, exit_scheduler);
        break;

    case CGenNode::EXIT_SCHED:
        m_p_queue.pop();
        thread->free_node(node);
        exit_scheduler = true;
        break;


    case CGenNode::COMMAND:
        handle_command(node, thread, exit_scheduler);
        break;

    case CGenNode::TW_SYNC1:
        handle_batch_tw_level1(node, thread, exit_scheduler,on_terminate);
        break;

    case CGenNode::TCP_RX_FLUSH:
        thread->handle_rx_flush(node,on_terminate);
        break;

    case CGenNode::TCP_TX_FIF:
        thread->handle_tx_fif(node,on_terminate);
        break;

    case CGenNode::TCP_TW:
        thread->handle_tw(node,on_terminate);
        break;

    case CGenNode::STL_RX_FLUSH:
        thread->handle_stl_rx(node,on_terminate);
        break;

    default:
        assert(0);
    }

    return (exit_scheduler);

}

void CFlowGenListPerThread::Dump(FILE *fd){
    fprintf(fd,"yaml info ");
    m_yaml_info.Dump(fd);

    fprintf(fd,"\n");
    fprintf(fd,"cap file info");
    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread  * lp=m_cap_gen[i];
        lp->Dump(stdout);
    }
}




void CFlowGenListPerThread::DumpStats(FILE *fd){
    m_stats.dump(fd);
}


void CFlowGenListPerThread::DumpCsv(FILE *fd){
    CFlowStats::DumpHeader(fd);

    CFlowStats stats;
    CFlowStats sum;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread  * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        stats.Dump(fd);
        sum.Add(stats);
    }
    fprintf(fd,"\n");
    sum.m_name= "sum";
    sum.Dump(fd);
}


uint32_t CFlowGenListPerThread::getDualPortId(){
    return ( ::getDualPortId(m_thread_id) );
}

double CFlowGenListPerThread::get_longest_flow(pool_index_t pool_idx, bool is_client){
    int i;
    double longest_flow = 0.0;
    for (i=0;i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread * lp=m_cap_gen[i];
        if (is_client &&
            lp->m_info->m_client_pool_idx != pool_idx)
            continue;
        if (!is_client &&
            lp->m_info->m_server_pool_idx != pool_idx)
            continue;
        double tmp_len;
        tmp_len = lp->m_flow_info->get_cap_file_length_sec();
        if (longest_flow < tmp_len ) {
            longest_flow = tmp_len;
        }
    }
    return longest_flow;
}


double CFlowGenListPerThread::get_longest_flow(){
    int i;
    double longest_flow = 0.0;
    for (i=0;i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread * lp=m_cap_gen[i];
        double tmp_len;
        tmp_len = lp->m_flow_info->get_cap_file_length_sec();
        if (longest_flow < tmp_len ) {
            longest_flow = tmp_len;
        }
    }
    return longest_flow;
}

double CFlowGenListPerThread::get_total_kcps(pool_index_t pool_idx, bool is_client){
    int i;
    double total=0.0;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread * lp=m_cap_gen[i];
        if (is_client &&
            lp->m_info->m_client_pool_idx != pool_idx)
            continue;
        if (!is_client &&
            lp->m_info->m_server_pool_idx != pool_idx)
            continue;
        total +=lp->m_info->m_k_cps;
    }
    return (total);
}

double CFlowGenListPerThread::get_total_kcps(){
    int i;
    double total=0.0;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRecPerThread * lp=m_cap_gen[i];
        total +=lp->m_info->m_k_cps;
    }
    return (total);
}

double CFlowGenListPerThread::get_delta_flow_is_sec(){
    return (1.0/(1000.0*get_total_kcps()));
}


void CFlowGenListPerThread::inc_current_template(void){
    m_cur_template++;
    if (m_cur_template == m_cap_gen.size()) {
        m_cur_template=0;
    }
}


int CFlowGenListPerThread::generate_flows_roundrobin(bool *done){
    // round robin

    CFlowGeneratorRecPerThread * cur;
    bool found=false;
    // try current
    int i;
    *done = true;
    for (i=0;i<(int)m_cap_gen.size();i++ ) {
        cur=m_cap_gen[m_cur_template];
        if (!(cur->m_info->m_limit_was_set) ||
            (cur->m_info->m_flowcnt < cur->m_info->m_limit)) {
            *done = false;
            if ( cur->m_policer.update(1.0,m_cur_time_sec) ){
                cur->m_info->m_flowcnt++;
                found=true;
                break;
            }
        }
        inc_current_template();
    }

    if (found) {
        /* generate the flow into the generator*/
        CGenNode * node= create_node() ;

        cur->generate_flow(&m_node_gen,m_cur_time_sec,m_cur_flow_id,node);
        m_cur_flow_id++;

        /* this is estimation */
        m_stats.m_total_open_flows += cur->m_flow_info->get_total_flows();
        m_stats.m_total_bytes += cur->m_flow_info->get_total_bytes();
        m_stats.m_total_pkt   += cur->m_flow_info->Size();
        inc_current_template();
    }
    return (0);
}


int CFlowGenListPerThread::reschedule_flow(CGenNode *node){

    // Re-schedule the node
    node->reset_pkt_in_flow();
    node->m_time += node->m_template_info->m_restart_time;
    m_node_gen.add_node(node);

    m_stats.m_total_bytes += node->m_flow_info->get_total_bytes();
    m_stats.m_total_pkt   += node->m_flow_info->Size();

    return (0);
}

void CFlowGenListPerThread::terminate_nat_flows(CGenNode *p){
    m_stats.m_nat_flow_timeout++;
    m_stats.m_nat_lookup_remove_flow_id++;
    if (p->is_nat_wait_ack_state()) {
        m_stats.m_nat_flow_timeout_wait_ack++;
    } else {
        m_stats.m_nat_lookup_wait_ack_state++;
    }

    if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION) ||
        p->m_pkt_info->m_pkt_indication.l3.m_ipv4->getProtocol() == IPPROTO_TCP) {
        m_flow_id_to_node_lookup.remove_no_lookup(p->get_short_fid() & NAT_FLOW_ID_MASK_TCP_ACK);
    } else {
        m_flow_id_to_node_lookup.remove_no_lookup(p->get_short_fid() & NAT_FLOW_ID_MASK_IP_ID);
    }
    free_last_flow_node(p);
}


void CFlowGenListPerThread::handle_latency_pkt_msg(CGenNodeLatencyPktInfo * msg){
    /* send the packet */
    #ifdef RX_QUEUE_TRACE_
    printf(" latency  msg dir %d\n",msg->m_dir);
    struct rte_mbuf * m;
    m=msg->m_pkt;
    rte_pktmbuf_dump(stdout,m, rte_pktmbuf_pkt_len(m));
    #endif

    /* update timestamp */
    if (msg->m_update_ts) {
        struct rte_mbuf *m = msg->m_pkt;
        uint8_t *p = rte_pktmbuf_mtod(m, uint8_t*);
        latency_header * h = (latency_header *)(p+msg->m_latency_offset);
        h->time_stamp = os_get_hr_tick_64();
    }


    m_node_gen.m_v_if->send_one_pkt((pkt_dir_t)msg->m_dir,msg->m_pkt);
}

void CFlowGenListPerThread::handle_nat_msg(CGenNodeNatInfo * msg){
    int i;
    bool first = true, second = true;

    for (i=0; i<msg->m_cnt; i++) {
        first = true;
        second = true;
        CNatFlowInfo * nat_msg=&msg->m_data[i];
        CGenNode * node=m_flow_id_to_node_lookup.lookup(nat_msg->m_fid);
        if (!node) {
            /* this should be moved to a notification module */
#ifdef NAT_TRACE_
            printf(" ERORR not valid flow_id %d probably flow was aged  \n",nat_msg->m_fid);
#endif
            m_stats.m_nat_lookup_no_flow_id++;
            continue;
        }

        // Calculate diff between tcp seq of SYN packet, and TCP ack of SYN+ACK packet
        // For supporting firewalls who do TCP seq num randomization
        if ( CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP) ) {

            if (node->m_pkt_info->m_pkt_indication.getIpProto() == IPPROTO_TCP) {
                if (node->is_nat_wait_state()) {
                    char *syn_pkt = node->m_flow_info->GetPacket(0)->m_packet->raw;
                    TCPHeader *tcp = (TCPHeader *)(syn_pkt + node->m_pkt_info->m_pkt_indication.getFastTcpOffset());
                    node->set_nat_tcp_seq_diff_client(nat_msg->m_tcp_seq - tcp->getSeqNumber());
                    if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
                        node->set_nat_wait_ack_state();
                        m_stats.m_nat_lookup_wait_ack_state++;
                        second = false;
                    } else {
                        node->set_nat_learn_state();
                    }
                } else {
                    char *syn_ack_pkt = node->m_flow_info->GetPacket(1)->m_packet->raw;
                    TCPHeader *tcp = (TCPHeader *)(syn_ack_pkt + node->m_pkt_info->m_pkt_indication.getFastTcpOffset());
                    node->set_nat_tcp_seq_diff_server(nat_msg->m_tcp_seq - tcp->getSeqNumber());
                    assert(node->is_nat_wait_ack_state());
                    node->set_nat_learn_state();
                    first = false;
                }
          }else{
              /* UDP */
              assert(node->is_nat_wait_state());
              node->set_nat_learn_state();
              m_stats.m_nat_lookup_wait_ack_state++;
          }
        } else {
            assert(node->is_nat_wait_state());
            node->set_nat_learn_state();
        }

        if (first) {
#ifdef NAT_TRACE_
            printf(" %.03f  RX :set node %p:%x  %x:%x TCP diff %x\n"
                   , now_sec(), node,nat_msg->m_fid, nat_msg->m_external_ip, nat_msg->m_external_port
                   , node->get_nat_tcp_seq_diff_client());
#endif

            node->set_nat_ipv4_addr(nat_msg->m_external_ip);
            node->set_nat_ipv4_port(nat_msg->m_external_port);

            if ( CGlobalInfo::is_learn_verify_mode() ){
                if (!node->is_external_is_eq_to_internal_ip() ||
                    node->get_nat_tcp_seq_diff_client() != 0) {
                    m_stats.m_nat_flow_learn_error++;
                }
            }
        }

        if (second) {
            /* remove from the hash */
            m_flow_id_to_node_lookup.remove_no_lookup(nat_msg->m_fid);
            m_stats.m_nat_lookup_remove_flow_id++;
        }
    }
}

void CFlowGenListPerThread::handle_stl_rx(CGenNode * node,
                                          bool on_terminate){

    double dtime=STL_RX_FLUSH_SEC;
    int drop=0;
    m_node_gen.m_p_queue.pop();
    /* in case the ports are idle for more than time ticks stop */
    if (m_dp_core->are_all_ports_idle()){
        m_tcp_terminate_cnt++;
        if ( m_tcp_terminate_cnt>STL_RX_DELAY_TICKS ) {
           drop=1;
        }
    }else{
        m_tcp_terminate_cnt=0;
    }
    
    if ( on_terminate ){
           drop=1;
    }
    if (drop) {
        free_node(node);
    }else{
        node->m_time += dtime;
        m_node_gen.m_p_queue.push(node);
    }
    handle_stl_pkts(false);
}

uint16_t CFlowGenListPerThread::handle_stl_pkts(bool is_idle) {
    CVirtualIF * v_if=m_node_gen.m_v_if;
    rte_mbuf_t * rx_pkts[64];
    int dir;
    uint16_t cnt;
    tvpid_t   ports_id[2];
    uint16_t sum;
    uint16_t sum_both_dir = 0;
    get_port_ids(ports_id[0], ports_id[1]);

    for (dir=0; dir<CS_NUM; dir++) {
        sum=0;
        while (true) {
            cnt=v_if->rx_burst(dir,rx_pkts,64);
            if (cnt==0) {
                break;
            }
            int i;
            for (i=0; i<(int)cnt;i++) {
                rte_mbuf_t * m=rx_pkts[i];
#ifdef _DEBUG
                if ( CGlobalInfo::m_options.preview.getVMode() ==7 ){
                    fprintf(stdout,"RX---> dir %d (tid:%d) \n",dir,(int)m_thread_id);
                    utl_rte_pktmbuf_dump_k12(stdout,m);
                }
#endif
                m_dp_core->rx_handle_packet(dir,m,is_idle,ports_id[dir]);
            }
            sum+=cnt;
            /* support up to 25MPPS per core (for 20usec tick it would be 512 
               suggested by @jsmoon 
            */
            if (sum>512) {
                break;
            }
        }
        /*if (m_sched_accurate && sum){
            v_if->flush_tx_queue();
        }*/
        sum_both_dir += sum;
    }
    return sum_both_dir;
}



void   CFlowGenListPerThread::no_memory_error(){
    printf("--------\n");
    printf("\n");
    printf("\n");
    printf("ERROR, not enough flow objects, try to enlarge the number of objects in trex_cfg file or reduce the bandwidth \n");
    printf("See in the manual how to enlarge the number of objects.\n");
    printf("\n");
    printf("\n");
    printf(" Check your active flows, 'Active-flows    :  6771863', If it too high reduce the multiplier \n");
    printf(" or use --active-flows directive to reduce the number of flows\n");
    printf(" If you don't have enough memory for flows you should add something like that in your config file            \n");                                
    printf("\n");
    printf(" memory    :             \n");                                
    printf("      dp_flows    : 4048576 \n");
    printf("--------\n");
    exit(1);
}


bool CFlowGenListPerThread::check_msgs_from_rx() {
    if ( likely ( m_ring_from_rx->isEmpty() ) ) {
        return false;
    }

    #ifdef  NAT_TRACE_
    printf(" %.03f got message from RX \n",now_sec());
    #endif
    while ( true ) {
        CGenNode * node;
        if ( m_ring_from_rx->Dequeue(node)!=0 ){
            break;
        }
        assert(node);
        //printf ( " message: thread %d,  node->m_flow_id : %d \n", m_thread_id,node->m_flow_id);
        /* only one message is supported right now */

        CGenNodeMsgBase * msg=(CGenNodeMsgBase *)node;

        uint8_t   msg_type =  msg->m_msg_type;
        switch (msg_type ) {
        case CGenNodeMsgBase::NAT_FIRST:
            handle_nat_msg((CGenNodeNatInfo * )msg);
            break;

        case CGenNodeMsgBase::LATENCY_PKT:
            handle_latency_pkt_msg((CGenNodeLatencyPktInfo *) msg);
            break;

        default:
            printf("ERROR pkt-thread message type is not valid %d \n",msg_type);
            assert(0);
        }

        CGlobalInfo::free_node(node);
    }

    return true;
}

bool CFlowGenListPerThread::check_msgs() {

    bool had_msg = false;

    /* inlined for performance */
    if (m_dp_core->periodic_check_for_cp_messages()) {
        had_msg = true;
    }

    if (check_msgs_from_rx()) {
        had_msg = true;
    }

    return had_msg;
}



void CFlowGenListPerThread::start(std::string &erf_file_name, CPreviewMode &preview) {
    
    /* reset the time */
    m_cur_time_sec = 0;
    
    /* set per thread global info, for performance */
    m_preview_mode = preview;
    
    m_node_gen.open_file(erf_file_name, &m_preview_mode);
    
    /* start the core */
    m_dp_core->start();
}

void CFlowGenListPerThread::start_sim(const std::string &erf_file_name, CPreviewMode &preview, uint64_t limit) {
    m_preview_mode = preview;
    m_node_gen.open_file(erf_file_name,&m_preview_mode);
    m_node_gen.set_packet_limit(limit);
    
    m_cur_time_sec = 0;
    m_dp_core->start_once();
    m_node_gen.m_v_if->close_file();
}




void CFlowGenList::Delete(){
    clean_p_thread_info();
    Clean();
    if (CPluginCallback::callback) {
        delete  CPluginCallback::callback;
        CPluginCallback::callback = NULL;
    }
    if (m_stt_cp) {
        m_stt_cp->Delete();
        delete m_stt_cp;
        m_stt_cp=0;
    }
}


bool CFlowGenList::Create(){
    check_objects_sizes();
    CPluginCallback::callback=  new CPluginCallbackSimple();
    m_stt_cp =0;
    if (get_is_tcp_mode()) {
      m_stt_cp = new CSTTCp();
      m_stt_cp->Create();
    }
    return (true);
}


void CFlowGenList::generate_p_thread_info(uint32_t num_threads){
    clean_p_thread_info();
    BP_ASSERT(num_threads < 64);
    int i;
    for (i=0; i<(int)num_threads; i++) {
        CFlowGenListPerThread   * lp= new CFlowGenListPerThread();
        lp->Create(i,this,num_threads);
        m_threads_info.push_back(lp);
    }
}


void CFlowGenList::clean_p_thread_info(void){
    int i;
    for (i=0; i<(int)m_threads_info.size(); i++) {
        CFlowGenListPerThread * lp=m_threads_info[i];
        lp->Delete();
        delete lp;
    }
    m_threads_info.clear();
}

int CFlowGenList::load_client_config_file(std::string file_name) {
    m_client_config_info.load_yaml_file(file_name);
    return (0);
}

void CFlowGenList::set_client_config_tuple_gen_info(CTupleGenYamlInfo * tg) {
    m_client_config_info.set_tuple_gen_info(tg);
}

void CFlowGenList::get_client_cfg_ip_list(std::vector<ClientCfgCompactEntry *> &ret) {
    m_client_config_info.get_entry_list(ret);
}

void CFlowGenList::set_client_config_resolved_macs(CManyIPInfo *pretest_result) {
    m_client_config_info.set_resolved_macs(pretest_result);
}

void CFlowGenList::dump_client_config(FILE *fd) {
    m_client_config_info.dump(fd);
}

/* take the total CPS and multi by the longest flow */
double CFlowGenList::get_worse_case_active_flows(){
    CFlowStats sum;
    CFlowStats stats;
    int i;
    double max_duration=-1.0;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        
        if ( stats.duration_sec > max_duration){
            max_duration=stats.duration_sec;
        }
        sum.Add(stats);
    }
    return (sum.m_cps*max_duration);
}

int CFlowGenList::update_active_flows(uint32_t active_flows){
    double d_active_flow=(double)active_flows;
    CFlowStats stats;
    CFlowStats sum;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        sum.Add(stats);
    }

    if (sum.m_c_flows <10) {
        /* nothing to do */
        return (0);
    }
    double ipg_factor = d_active_flow/sum.m_c_flows;

    /* calc it again */
    sum.Clear();
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->updateIpg(ipg_factor);
        lp->getFlowStats(&stats);
        sum.Add(stats);
    }

    return(0);
}


int CFlowGenList::load_astf(){

    m_yaml_info.set_astf_mode();

    uint8_t idx;
    for (idx=0; idx<6; idx++){
        CGlobalInfo::m_options.m_src_ipv6[idx] = 0;
        CGlobalInfo::m_options.m_dst_ipv6[idx] = 0;
    }

    return(0);
}

int CFlowGenList::load_from_yaml(std::string file_name,
                                 uint32_t num_threads){
    uint8_t idx;
    m_yaml_info.load_from_yaml_file(file_name);
    if (m_yaml_info.verify_correctness(num_threads) ==false){
        exit(0);
    }

    /* move it to global info, better CPU D-cache  usage */
    if (m_yaml_info.m_vlan_info.m_enable) {
        CGlobalInfo::m_options.preview.set_vlan_mode_verify(CPreviewMode::VLAN_MODE_LOAD_BALANCE);
        CGlobalInfo::m_options.m_vlan_port[0] = m_yaml_info.m_vlan_info.m_vlan_per_port[0];
        CGlobalInfo::m_options.m_vlan_port[1] = m_yaml_info.m_vlan_info.m_vlan_per_port[1];
    } else {
        CGlobalInfo::m_options.m_vlan_port[0] = 0;
        CGlobalInfo::m_options.m_vlan_port[1] = 0;
    }
    CGlobalInfo::m_options.preview.set_mac_ip_overide_enable(m_yaml_info.m_mac_replace_by_ip);

    if (m_yaml_info.m_ipv6_set == true) {
        // Copy the most significant 96-bits from yaml data
        for (idx=0; idx<6; idx++){
            CGlobalInfo::m_options.m_src_ipv6[idx] = m_yaml_info.m_src_ipv6[idx];
            CGlobalInfo::m_options.m_dst_ipv6[idx] = m_yaml_info.m_dst_ipv6[idx];
        }
    }else{
        // Set the most signifcant 96-bits to zero which represents an
        // IPv4-compatible IPv6 address
        for (idx=0; idx<6; idx++){
            CGlobalInfo::m_options.m_src_ipv6[idx] = 0;
            CGlobalInfo::m_options.m_dst_ipv6[idx] = 0;
        }
    }

    int i=0;
    Clean();
    bool all_template_has_one_direction=true;
    for (i=0; i<(int)m_yaml_info.m_vec.size(); i++) {
        CFlowGeneratorRec * lp=new CFlowGeneratorRec();
        if ( lp->Create(&m_yaml_info.m_vec[i],&m_yaml_info,i) == false){
            fprintf(stderr, "Error processing file: '%s'. Please correct the errors and try again.\n", file_name.c_str());
            exit(-1);
        }
        m_cap_gen.push_back(lp);

        if (lp->m_flow_info.GetPacket(0)->m_pkt_indication.m_desc.IsBiDirectionalFlow() ) {
            all_template_has_one_direction=false;
        }
    }

    if ( CGlobalInfo::is_learn_mode() && all_template_has_one_direction ) {
        fprintf(stdout,"\n Warning --learn mode has nothing to do when all templates are one directional, please remove it  \n");
    }
    return (0);
}



void CFlowGenList::Clean(){
    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->Delete();
        delete lp;
    }
    m_cap_gen.clear();
}

double CFlowGenList::GetCpuUtil(){
    int i;
    double c=0.0;
    for (i=0; i<(int)m_threads_info.size(); i++) {
        CFlowGenListPerThread * lp=m_threads_info[i];
        c+=lp->m_cpu_cp_u.GetVal();
    }
    return (c/m_threads_info.size());
}

double CFlowGenList::GetCpuUtilRaw(){
    int i;
    double c=0.0;
    for (i=0; i<(int)m_threads_info.size(); i++) {
        CFlowGenListPerThread * lp=m_threads_info[i];
        c+=lp->m_cpu_cp_u.GetValRaw();
    }
    return (c/m_threads_info.size());
}


void CFlowGenList::UpdateFast(){

    for (int i=0; i<(int)m_threads_info.size(); i++) {
        CFlowGenListPerThread * lp=m_threads_info[i];
        lp->Update();
    }
}



void CFlowGenList::Dump(FILE *fd){
    fprintf(fd,"yaml info \n");
    fprintf(fd,"--------------\n");
    m_yaml_info.Dump(fd);

    fprintf(fd,"\n");
    fprintf(fd,"cap file info \n");
    fprintf(fd,"----------------------\n");
    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->Dump(fd);
    }
}


void CFlowGenList::DumpPktSize(){

    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->m_flow_info.dump_pkt_sizes();
    }
}


void CFlowGenList::DumpCsv(FILE *fd){
    CFlowStats::DumpHeader(fd);

    CFlowStats stats;
    CFlowStats sum;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        stats.Dump(fd);
        sum.Add(stats);
    }
    fprintf(fd,"\n");
    sum.m_name= "sum";
    sum.Dump(fd);
    sum.m_memory.dump(fd);
}


uint32_t CFlowGenList::get_total_repeat_flows(){
    uint32_t flows=0;
    int i;
    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        flows+=lp->m_info->m_limit ;
    }
    return (flows);
}


double CFlowGenList::get_total_tx_bps(){
    CFlowStats stats;
    double total=0.0;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        total+=(stats.m_mb_sec);
    }
    return (_1Mb_DOUBLE*total);
}

double CFlowGenList::get_total_pps(){

    CFlowStats stats;
    double total=0.0;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        total+=stats.m_pps;
    }
    return (total);
}


double CFlowGenList::get_total_kcps(){

    CFlowStats stats;
    double total=0.0;
    int i;

    for (i=0; i<(int)m_cap_gen.size(); i++) {
        CFlowGeneratorRec * lp=m_cap_gen[i];
        lp->getFlowStats(&stats);
        total+= stats.get_normal_cps();
    }
    return ((total/1000.0));
}

double CFlowGenList::get_delta_flow_is_sec(){
    return (1.0/(1000.0*get_total_kcps()));
}



#if 0

void CTupleGlobalGenerator::Dump(FILE *fd){
    fprintf(fd," src:%x  dest: %x \n",m_result_src_ip,m_result_dest_ip);
}

bool CTupleGlobalGenerator::Create(){
    was_generated=false;
    return (true);
}


void CTupleGlobalGenerator::Copy(CTupleGlobalGenerator * gen){
   was_generated=false;
   m_min_src_ip  = gen->m_min_src_ip;
   m_max_src_ip  = gen->m_max_src_ip;
   m_min_dest_ip = gen->m_min_dest_ip;
   m_max_dest_ip = gen->m_max_dest_ip;
}


void CTupleGlobalGenerator::Delete(){
    was_generated=false;
}

#endif

static uint32_t  get_rand_32(uint32_t MinimumRange ,
                      uint32_t MaximumRange );



static uint32_t get_rand_32(uint32_t MinimumRange,
                            uint32_t MaximumRange) __attribute__ ((unused));

static uint32_t get_rand_32(uint32_t MinimumRange,
                            uint32_t MaximumRange) {
    enum {RANDS_NUM = 2 , RAND_MAX_BITS = 0xf , UNSIGNED_INT_BITS = 0x20 , TWO_BITS_MASK = 0x3};
    const double TWO_POWER_32_BITS = 0x10000000 * (double)0x10;
    uint32_t RandomNumber = 0;

    for (int i = 0 ; i < RANDS_NUM;i++) {
        RandomNumber = (RandomNumber<<RAND_MAX_BITS) + rand();
    }
    RandomNumber = (RandomNumber<<(UNSIGNED_INT_BITS - RAND_MAX_BITS * RANDS_NUM)) + (rand() | TWO_BITS_MASK);

    uint32_t Range;
    if ((Range = MaximumRange - MinimumRange) == 0xffffffff) {
        return RandomNumber;
    }
    return (uint32_t)(((Range + 1) / TWO_POWER_32_BITS * RandomNumber) + MinimumRange );
}



int CNullIF::send_node(CGenNode * node){
    #if 0
    CFlowPktInfo * lp=node->m_pkt_info;
    rte_mbuf_t * buf=lp->generate_new_mbuf(node);
    //rte_pktmbuf_dump(buf, buf->pkt_len);
    //sending it ??
    // free it here as if driver does
    rte_pktmbuf_free(buf);
    #endif
    return (0);
}



void CErfIF::fill_raw_packet(rte_mbuf_t * m,CGenNode * node,pkt_dir_t dir){

    fill_pkt(m_raw,m);

    CPktNsecTimeStamp t_c(node->m_time);
    m_raw->time_nsec = t_c.m_time_nsec;
    m_raw->time_sec  = t_c.m_time_sec;
    uint8_t p_id = (uint8_t)dir;
    m_raw->setInterface(p_id);
}


pkt_dir_t CErfIFStl::port_id_to_dir(uint8_t port_id) {
     return ((pkt_dir_t)(port_id&1));
}


int CErfIFStl::update_mac_addr_from_global_cfg(pkt_dir_t  dir, uint8_t * p){
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(dir),12);
    return (0);
}

int CErfIFStl::send_sl_node(CGenNodeStateless *node_sl) {
    pkt_dir_t dir=(pkt_dir_t)node_sl->get_mbuf_cache_dir();

    rte_mbuf_t *    m;
    if ( likely(node_sl->is_cache_mbuf_array()) ) {
        m=node_sl->cache_mbuf_array_get_cur();
        fill_raw_packet(m,(CGenNode *)node_sl,dir);
    }else{
        m=node_sl->get_cache_mbuf();
        bool is_const = false;
        if (m) {
            is_const = true;
            rte_pktmbuf_refcnt_update(m,1);
        }else{
            m=node_sl->alloc_node_with_vm();
            assert(m);
        }

        if (node_sl->is_stat_needed() && (node_sl->get_stat_hw_id() >= MAX_FLOW_STATS) ) {
            /* latency packet. flow stat without latency handled like normal packet in simulation */
            uint16_t hw_id = node_sl->get_stat_hw_id();
            rte_mbuf_t *mi;
            struct flow_stat_payload_header *fsp_head;
            mi = node_sl->alloc_flow_stat_mbuf(m, fsp_head, is_const);
            fsp_head->seq = 0x12345678;
            fsp_head->hw_id = hw_id - MAX_FLOW_STATS;
            fsp_head->magic = FLOW_STAT_PAYLOAD_MAGIC;
            fsp_head->flow_seq = FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ;
            fsp_head->time_stamp = 0x8899aabbccddeeff;
            fill_raw_packet(mi, (CGenNode *)node_sl, dir);
            rte_pktmbuf_free(mi);
        } else {
            fill_raw_packet(m,(CGenNode *)node_sl,dir);
            rte_pktmbuf_free(m);
        }
    }
        /* check that we have mbuf  */
    int rc = write_pkt(m_raw);
    BP_ASSERT(rc == 0);

    return (rc);
}

int CErfIFStl::send_pcap_node(CGenNodePCAP *pcap_node) {
    rte_mbuf_t *m = pcap_node->get_pkt();
    if (!m) {
        return (-1);
    }

    pkt_dir_t dir = (pkt_dir_t)pcap_node->get_mbuf_dir();
    fill_raw_packet(m, (CGenNode*)pcap_node, dir);
    rte_pktmbuf_free(m);

    int rc = write_pkt(m_raw);
    BP_ASSERT(rc == 0);

    return (rc);
}

/*
 * This is the simulation stateless send_node.
 * in simulation (bp-sim-64) it is called instead of CCoreEthIFStateless::send_node
 * Purpose is to test the mbuf manipulation functions which are the same in simulation and "real" code
 */
int CErfIFStl::send_node(CGenNode * _no_to_use){

    if ( m_preview_mode->getFileWrite() ) {

        switch (_no_to_use->m_type) {
        case CGenNode::STATELESS_PKT:
            return send_sl_node((CGenNodeStateless *) _no_to_use);

        case CGenNode::PCAP_PKT:
            return send_pcap_node((CGenNodePCAP *) _no_to_use);

        default:
            assert(0);
        }
    }
    return (0);
}



void CErfIF::add_vlan(uint16_t vlan_id) {
    uint8_t *buffer =(uint8_t *)m_raw->raw;

    uint16_t vlan_protocol = EthernetHeader::Protocol::VLAN;
    uint32_t vlan_tag = (vlan_protocol << 16) | vlan_id;
    vlan_tag = PKT_HTONL(vlan_tag);

    /* insert vlan tag and adjust packet size */
    memcpy(cbuff+4, buffer + 12, m_raw->pkt_len - 12);
    memcpy(cbuff, &vlan_tag, 4);
    memcpy(buffer + 12, cbuff, m_raw->pkt_len - 8);

    m_raw->pkt_len += 4;
}

void CErfIF::apply_client_config(const ClientCfgBase *cfg, pkt_dir_t dir) {
    assert(cfg);
    uint8_t *p = (uint8_t *)m_raw->raw;

    const ClientCfgDirBase &cfg_dir = ( (dir == CLIENT_SIDE) ? cfg->m_initiator : cfg->m_responder);

    /* dst mac */
    if (cfg_dir.has_dst_mac_addr()) {
        memcpy(p, cfg_dir.get_dst_mac_addr(), 6);
    }

    /* src mac */
    if (cfg_dir.has_src_mac_addr()) {
        memcpy(p + 6, cfg_dir.get_src_mac_addr(), 6);
    }

    /* VLAN */
    if (cfg_dir.has_vlan()) {
        add_vlan(cfg_dir.get_vlan());
    }
}

int CErfIF::send_node(CGenNode *node){

    if (!m_preview_mode->getFileWrite()) {
        return (0);
    }

    CFlowPktInfo *lp = node->m_pkt_info;
    rte_mbuf_t   *m  = lp->generate_new_mbuf(node);
    pkt_dir_t    dir = node->cur_interface_dir();

    fill_raw_packet(m, node, dir);

    /* update mac addr dest/src 12 bytes */
    uint8_t *p=(uint8_t *)m_raw->raw;
    int p_id=(int)dir;
    memcpy(p,CGlobalInfo::m_options.get_dst_src_mac_addr(p_id),12);

    /* if a client configuration was provided - apply the config */
    if (CGlobalInfo::m_options.preview.get_is_client_cfg_enable()) {
        apply_client_config(node->m_client_cfg, dir);

        // for simulation, VLAN_MODE_NORMAL is not relevant, since it uses vlan_id set in platform config file
    } else if (CGlobalInfo::m_options.preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_LOAD_BALANCE) {
        uint8_t vlan_port = (node->m_src_ip & 1);
        uint16_t vlan_id = CGlobalInfo::m_options.m_vlan_port[vlan_port];

        add_vlan(vlan_id);
    }

    //utl_DumpBuffer(stdout,p,  12,0);

    int rc = write_pkt(m_raw);
    BP_ASSERT(rc == 0);

    rte_pktmbuf_free(m);

    return (0);
}

int CErfIF::flush_tx_queue(void){
    return (0);
}

void CTcpSeq::update(uint8_t *p, CFlowPktInfo *pkt_info, int16_t s_size){
    TCPHeader *tcp= (TCPHeader *)(p+pkt_info->m_pkt_indication.getFastTcpOffset());
    uint32_t seqnum, acknum;

    // This routine will adjust the TCP segment size for packets
    // based on the modifications made by the plugins.
    // Basically it will keep track of the size changes
    // and adjust the TCP sequence numbers accordingly.

    bool is_init=pkt_info->m_pkt_indication.m_desc.IsInitSide();

    // Update TCP seq number
    seqnum = tcp->getSeqNumber();
    acknum = tcp->getAckNumber();
    if (is_init) {
        // Packet is from client
        seqnum += client_seq_delta;
        acknum += server_seq_delta;
    } else {
        // Packet is from server
        seqnum += server_seq_delta;
        acknum += client_seq_delta;
    }
    tcp->setSeqNumber(seqnum);
    tcp->setAckNumber(acknum);

    // Adjust delta being tracked
    if (is_init) {
        client_seq_delta += s_size;
    } else {
        server_seq_delta += s_size;
    }
}


void on_node_first(uint8_t plugin_id,CGenNode *     node,
                   CFlowYamlInfo *  template_info,
                   CTupleTemplateGeneratorSmart * tuple_gen,
                   CFlowGenListPerThread  * flow_gen){

    if (CPluginCallback::callback) {
        CPluginCallback::callback->on_node_first(plugin_id,node,template_info, tuple_gen,flow_gen);
    }
}

void on_node_last(uint8_t plugin_id,CGenNode *     node){
    if (CPluginCallback::callback) {
        CPluginCallback::callback->on_node_last(plugin_id,node);
    }

}

rte_mbuf_t * on_node_generate_mbuf(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info){
    rte_mbuf_t * m;
    assert(CPluginCallback::callback);
    m=CPluginCallback::callback->on_node_generate_mbuf(plugin_id,node,pkt_info);
    assert(m);
    return(m);
}


class CPlugin_rtsp : public CTcpSeq {
public:
    void *   m_gen;
    uint16_t rtp_client_0;
    uint16_t rtp_client_1;
};


void CPluginCallbackSimple::on_node_first(uint8_t plugin_id,
                                          CGenNode *     node,
                                          CFlowYamlInfo *  template_info,
                                          CTupleTemplateGeneratorSmart * tuple_gen,
                                          CFlowGenListPerThread  * flow_gen ){
    //printf(" on on_node_first callback  %d  node %x! \n",(int)plugin_id,node);
    /* generate 2 ports from client side */

    if ( (plugin_id == mpRTSP) || (plugin_id == mpSIP_VOICE) ) {
        CPlugin_rtsp * lpP=new CPlugin_rtsp();
        assert(lpP);

        /* TBD need to be fixed using new API */
        lpP->rtp_client_0 = tuple_gen->GenerateOneSourcePort();
        lpP->rtp_client_1 = tuple_gen->GenerateOneSourcePort();
        lpP->m_gen=flow_gen;
        node->m_plugin_info = (void *)lpP;
    }else{
        if (plugin_id ==mpDYN_PYLOAD) {
            /* nothing to do */
        }else{
            if (plugin_id ==mpAVL_HTTP_BROWSIN) {
                CTcpSeq * lpP=new CTcpSeq();
                assert(lpP);
                node->m_plugin_info = (void *)lpP;
            }else{
                /* do not support this */
                assert(0);
            }
        }
    }
}

void CPluginCallbackSimple::on_node_last(uint8_t plugin_id,CGenNode *     node){
    //printf(" on on_node_last callback  %d  %x! \n",(int)plugin_id,node);
    if ( (plugin_id == mpRTSP) || (plugin_id == mpSIP_VOICE) ) {
        CPlugin_rtsp * lpP=(CPlugin_rtsp * )node->m_plugin_info;
        /* free the ports */
        CFlowGenListPerThread  * flow_gen=(CFlowGenListPerThread  *) lpP->m_gen;
        bool is_tcp=node->m_pkt_info->m_pkt_indication.m_desc.IsTcp();
        flow_gen->defer_client_port_free(is_tcp,node->m_src_idx,lpP->rtp_client_0,
                                node->m_template_info->m_client_pool_idx,node->m_tuple_gen);
        flow_gen->defer_client_port_free(is_tcp,node->m_src_idx,lpP->rtp_client_1,
                                node->m_template_info->m_client_pool_idx,  node->m_tuple_gen);

        assert(lpP);
        delete lpP;
        node->m_plugin_info=0;
    }else{
        if (plugin_id ==mpDYN_PYLOAD) {
            /* nothing to do */
        }else{
            if (plugin_id ==mpAVL_HTTP_BROWSIN) {
                /* nothing to do */
                CTcpSeq * lpP=(CTcpSeq * )node->m_plugin_info;
                delete lpP;
                node->m_plugin_info=0;
            }else{
                /* do not support this */
                assert(0);
            }
        }
    }
}

rte_mbuf_t * CPluginCallbackSimple::http_plugin(uint8_t plugin_id,
                                                CGenNode *     node,
                                                CFlowPktInfo * pkt_info){
    CPacketDescriptor * lpd=&pkt_info->m_pkt_indication.m_desc;
    assert(lpd->getFlowId()==0); /* only one flow */
    CMiniVMCmdBase * program[2];
    CMiniVMReplaceIP  replace_cmd;
    CMiniVMCmdBase    eop_cmd;
    CTcpSeq * lpP=(CTcpSeq * )node->m_plugin_info;
    assert(lpP);
    rte_mbuf_t *mbuf;
    int16_t s_size=0;

    if ( likely (lpd->getFlowPktNum() != 3) ){
        if (unlikely (CGlobalInfo::is_ipv6_enable()) ) {
            // Request a larger initial segment for IPv6
            mbuf = pkt_info->do_generate_new_mbuf_big(node);
        }else{
            mbuf = pkt_info->do_generate_new_mbuf(node);
        }

    }else{
        CFlowInfo flow_info;
        flow_info.vm_program=0;

        flow_info.client_ip = node->m_src_ip;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = node->m_src_port;
        flow_info.server_port = 0;
        flow_info.replace_server_port =false;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);


        replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
        replace_cmd.m_flags   = 0;

        // Determine how much larger the packet needs to be to
        // handle the largest IP address. There is a single address
        // string of 8 bytes that needs to be replaced.
        if (CGlobalInfo::is_ipv6_enable() ) {
            // For IPv6, accomodate use of brackets (+2 bytes)
            replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 8;

            // Mark as IPv6 and set the upper 96-bits
            replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
            for (uint8_t idx=0; idx<6; idx++){
                replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
            }
        } else {
           replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 8;
        }

        // Set m_start_0/m_stop_1 at start/end of IP address to be replaced.
        // For this packet we know the IP addr string length is 8 bytes.
        replace_cmd.m_start_0 = 10+16;
        replace_cmd.m_stop_1  = replace_cmd.m_start_0 + 8;

        replace_cmd.m_server_ip.v4 = flow_info.server_ip;

        eop_cmd.m_cmd = VM_EOP;

        program[0] = &replace_cmd;
        program[1] = &eop_cmd;

        flow_info.vm_program = program;

        mbuf = pkt_info->do_generate_new_mbuf_ex_vm(node,&flow_info, &s_size);
    }

    // Fixup the TCP sequence numbers
    uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);

    // Update TCP sequence numbers
    lpP->update(p, pkt_info, s_size);

    return(mbuf);
}

rte_mbuf_t * CPluginCallbackSimple::dyn_pyload_plugin(uint8_t plugin_id,
                                                      CGenNode *     node,
                                                      CFlowPktInfo * pkt_info){

    CMiniVMCmdBase * program[2];

    CMiniVMDynPyload  dyn_cmd;
    CMiniVMCmdBase    eop_cmd;

    CPacketDescriptor * lpd=&pkt_info->m_pkt_indication.m_desc;
    CFlowYamlDynamicPyloadPlugin   *  lpt = node->m_template_info->m_dpPkt;
    assert(lpt);
    CFlowInfo flow_info;
    flow_info.vm_program=0;
    int16_t s_size=0;

    // IPv6 packets are not supported
    if (CGlobalInfo::is_ipv6_enable() ) {
         fprintf (stderr," IPv6 is not supported for dynamic pyload change\n");
         exit(-1);
    }

    if ( lpd->getFlowId() == 0 ) {

        flow_info.client_ip = node->m_src_ip;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = node->m_src_port;
        flow_info.server_port = 0;
        flow_info.replace_server_port =false;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);

        uint32_t pkt_num = lpd->getFlowPktNum();
        if (pkt_num < 253) {
            int i;
            /* fast filter */
            for (i=0; i<lpt->m_num; i++) {
                if (lpt->m_pkt_ids[i] == pkt_num ) {
                     //add a program here
                    dyn_cmd.m_cmd     = VM_DYN_PYLOAD;
                    dyn_cmd.m_ptr= &lpt->m_program[i];
                    dyn_cmd.m_flags   = 0;
                    dyn_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 8;
                    dyn_cmd.m_ip.v4=node->m_src_ip;

                    eop_cmd.m_cmd = VM_EOP;
                    program[0] = &dyn_cmd;
                    program[1] = &eop_cmd;

                    flow_info.vm_program = program;
                }
            }
        }
        // only for the first flow
     }else{
         fprintf (stderr," only one flow is allowed for dynamic pyload change \n");
         exit(-1);
     }/* only for the first flow */

    if ( unlikely( flow_info.vm_program != 0 ) ) {

        return (  pkt_info->do_generate_new_mbuf_ex_vm(node,&flow_info, &s_size) );
    }else{
        return (  pkt_info->do_generate_new_mbuf_ex(node,&flow_info) );
    }
}

rte_mbuf_t * CPluginCallbackSimple::sip_voice_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info){
    CMiniVMCmdBase * program[2];

    CMiniVMReplaceIP_PORT_IP_IP_Port  via_replace_cmd;
    CMiniVMCmdBase    eop_cmd;

    CPacketDescriptor * lpd=&pkt_info->m_pkt_indication.m_desc;
    CPlugin_rtsp * lpP=(CPlugin_rtsp * )node->m_plugin_info;
    assert(lpP);
  //  printf(" %d %d \n",lpd->getFlowId(),lpd->getFlowPktNum());
    CFlowInfo flow_info;
    flow_info.vm_program=0;
    int16_t s_size=0;

    switch ( lpd->getFlowId() ) {
    /* flow - SIP , packet #0,#1 control  */
    case 0:
        flow_info.client_ip = node->m_src_ip;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = node->m_src_port;
        flow_info.server_port = 0;
        flow_info.replace_server_port =false;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);


        /* program to replace ip server */
        switch ( lpd->getFlowPktNum() ) {
        case 0:
            {
                via_replace_cmd.m_cmd     = VM_REPLACE_IPVIA_IP_IP_PORT;
                via_replace_cmd.m_flags   = 0;
                via_replace_cmd.m_start_0 = 0;
                via_replace_cmd.m_stop_1  = 0;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There are 3 address
                // strings (each 9 bytes) that needs to be replaced.
                // We also need to accomodate IPv6 use of brackets
                // (+2 bytes) in a URI.
                // There are also 2 port strings that needs to be
                //  replaced (1 is 4 bytes the other is 5 bytes).
                if (CGlobalInfo::is_ipv6_enable() ) {
                    via_replace_cmd.m_add_pkt_len = (((INET6_ADDRSTRLEN + 2) - 9)  * 3) +
                         ((INET_PORTSTRLEN * 2) - 9);

                    // Mark as IPv6 and set the upper 96-bits
                    via_replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        via_replace_cmd.m_ip.v6[idx] = CGlobalInfo::m_options.m_src_ipv6[idx];
                        via_replace_cmd.m_ip_via.v6[idx] = CGlobalInfo::m_options.m_src_ipv6[idx];
                    }
                } else {
                    via_replace_cmd.m_add_pkt_len = ((INET_ADDRSTRLEN - 9)  * 3) +
                         ((INET_PORTSTRLEN * 2) - 9);
                }
                via_replace_cmd.m_ip.v4      =node->m_src_ip;
                via_replace_cmd.m_ip0_start  = 377;
                via_replace_cmd.m_ip0_stop   = 377+9;

                via_replace_cmd.m_ip1_start  = 409;
                via_replace_cmd.m_ip1_stop   = 409+9;


                via_replace_cmd.m_port       =lpP->rtp_client_0;
                via_replace_cmd.m_port_start = 435;
                via_replace_cmd.m_port_stop  = 435+5;

                via_replace_cmd.m_ip_via.v4     =  node->m_src_ip;
                via_replace_cmd.m_port_via   =  node->m_src_port;

                via_replace_cmd.m_ip_via_start = 208;
                via_replace_cmd.m_ip_via_stop  = 208+9+5;


                eop_cmd.m_cmd = VM_EOP;

                program[0] = &via_replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;
        case 1:
            {
                via_replace_cmd.m_cmd     = VM_REPLACE_IPVIA_IP_IP_PORT;
                via_replace_cmd.m_flags   = 0;
                via_replace_cmd.m_start_0 = 0;
                via_replace_cmd.m_stop_1  = 0;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There are 3 address
                // strings (each 9 bytes) that needs to be replaced.
                // We also need to accomodate IPv6 use of brackets
                // (+2 bytes) in a URI.
                // There are also 2 port strings that needs to be
                //  replaced (1 is 4 bytes the other is 5 bytes).
                if (CGlobalInfo::is_ipv6_enable() ) {
                    via_replace_cmd.m_add_pkt_len = (((INET6_ADDRSTRLEN + 2) - 9)  * 3) +
                         ((INET_PORTSTRLEN * 2) - 9);

                    // Mark as IPv6 and set the upper 96-bits
                    via_replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        via_replace_cmd.m_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                        via_replace_cmd.m_ip_via.v6[idx] = CGlobalInfo::m_options.m_src_ipv6[idx];
                   }
                } else {
                    via_replace_cmd.m_add_pkt_len = ((INET_ADDRSTRLEN - 9)  * 3) +
                         ((INET_PORTSTRLEN * 2) - 9);
                }

                via_replace_cmd.m_ip.v4      =node->m_dest_ip;
                via_replace_cmd.m_ip0_start  = 370;
                via_replace_cmd.m_ip0_stop   = 370+8;

                via_replace_cmd.m_ip1_start  = 401;
                via_replace_cmd.m_ip1_stop   = 401+8;


                via_replace_cmd.m_port       =lpP->rtp_client_0;
                via_replace_cmd.m_port_start = 426;
                via_replace_cmd.m_port_stop  = 426+5;


                via_replace_cmd.m_ip_via.v4  =  node->m_src_ip;
                via_replace_cmd.m_port_via   =  node->m_src_port;

                via_replace_cmd.m_ip_via_start = 207;
                via_replace_cmd.m_ip_via_stop  = 207+9+5;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &via_replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;


        }/* end of big switch on packet */
        break;

    case 1:
        flow_info.client_ip = node->m_src_ip ;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = lpP->rtp_client_0;
        /* this is tricky ..*/
        flow_info.server_port = lpP->rtp_client_0;
        flow_info.replace_server_port = true;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);

        break;
    default:
        assert(0);
        break;
    };

    //printf(" c_ip:%x s_ip:%x c_po:%x s_po:%x  init:%x  replace:%x \n",flow_info.client_ip,flow_info.server_ip,flow_info.client_port,flow_info.server_port,flow_info.is_init_dir,flow_info.replace_server_port);

    //printf(" program %p  \n",flow_info.vm_program);
    if ( unlikely( flow_info.vm_program != 0 ) ) {

        return (  pkt_info->do_generate_new_mbuf_ex_vm(node,&flow_info, &s_size) );
    }else{
        return (  pkt_info->do_generate_new_mbuf_ex(node,&flow_info) );
    }
}

rte_mbuf_t * CPluginCallbackSimple::rtsp_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info){

    CMiniVMCmdBase * program[2];

    CMiniVMReplaceIP  replace_cmd;
    CMiniVMCmdBase    eop_cmd;
    CMiniVMReplaceIPWithPort  replace_port_cmd;
    rte_mbuf_t *mbuf;

    CPacketDescriptor * lpd=&pkt_info->m_pkt_indication.m_desc;
    CPlugin_rtsp * lpP=(CPlugin_rtsp * )node->m_plugin_info;

    assert(lpP);
  //  printf(" %d %d \n",lpd->getFlowId(),lpd->getFlowPktNum());
    CFlowInfo flow_info;
    flow_info.vm_program=0;
    int16_t s_size=0;

    switch ( lpd->getFlowId() ) {
    /* flow - control  */
    case 0:
        flow_info.client_ip = node->m_src_ip;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = node->m_src_port;
        flow_info.server_port = 0;
        flow_info.replace_server_port =false;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);


        /* program to replace ip server */
        switch ( lpd->getFlowPktNum() ) {
        case 3:
            {
                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 16;
                replace_cmd.m_stop_1  = 16+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;
        case 4:
            {
                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 46;
                replace_cmd.m_stop_1  = 46+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        case 5:
            {

                replace_port_cmd.m_cmd     = VM_REPLACE_IP_PORT_OFFSET;
                replace_port_cmd.m_flags   = 0;
                replace_port_cmd.m_start_0 = 13;
                replace_port_cmd.m_stop_1  = 13+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                // There are also 2 port strings (8 bytes) that needs to be
                //  replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_port_cmd.m_add_pkt_len = ((INET6_ADDRSTRLEN + 2) - 9) +
                         ((INET_PORTSTRLEN * 2) - 8);

                    // Mark as IPv6 and set the upper 96-bits
                    replace_port_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_port_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_port_cmd.m_add_pkt_len = (INET_ADDRSTRLEN - 9) +
                         ((INET_PORTSTRLEN * 2) - 8);
                }
                replace_port_cmd.m_server_ip.v4 = flow_info.server_ip;
                replace_port_cmd.m_start_port =  164;
                replace_port_cmd.m_stop_port  =  164+(4*2)+1;
                replace_port_cmd.m_client_port = lpP->rtp_client_0;
                replace_port_cmd.m_server_port =0;


                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_port_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        case 6:
            {

                replace_port_cmd.m_cmd     = VM_REPLACE_IP_PORT_RESPONSE_OFFSET;
                replace_port_cmd.m_flags   = 0;
                replace_port_cmd.m_start_0 = 0;
                replace_port_cmd.m_stop_1  = 0;

                // Determine how much larger the packet needs to be to
                // handle the largest port addresses. There are 4 port address
                // strings (16 bytes) that needs to be replaced.
                replace_port_cmd.m_add_pkt_len = ((INET_PORTSTRLEN * 4) - 16);

                replace_port_cmd.m_server_ip.v4 = flow_info.server_ip;
                replace_port_cmd.m_start_port =  247;
                replace_port_cmd.m_stop_port  = 247+(4*4)+2+13;
                replace_port_cmd.m_client_port = lpP->rtp_client_0;
                replace_port_cmd.m_server_port = lpP->rtp_client_0;


                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_port_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;


        case 7:
            {

                replace_port_cmd.m_cmd     = VM_REPLACE_IP_PORT_OFFSET;
                replace_port_cmd.m_flags   = 0;
                replace_port_cmd.m_start_0 = 13;
                replace_port_cmd.m_stop_1  = 13+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                // There are also 2 port strings (8 bytes) that needs to be
                //  replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_port_cmd.m_add_pkt_len = ((INET6_ADDRSTRLEN + 2) - 9) +
                         ((INET_PORTSTRLEN * 2) - 8);

                    // Mark as IPv6 and set the upper 96-bits
                    replace_port_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_port_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_port_cmd.m_add_pkt_len = (INET_ADDRSTRLEN - 9) +
                         ((INET_PORTSTRLEN * 2) - 8);
                }
                replace_port_cmd.m_server_ip.v4 = flow_info.server_ip;
                replace_port_cmd.m_start_port =  164;
                replace_port_cmd.m_stop_port  =  164+(4*2)+1;
                replace_port_cmd.m_client_port = lpP->rtp_client_1;
                replace_port_cmd.m_server_port =0;


                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_port_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        case 8:

            {

                replace_port_cmd.m_cmd     = VM_REPLACE_IP_PORT_RESPONSE_OFFSET;
                replace_port_cmd.m_flags   = 0;
                replace_port_cmd.m_start_0 = 0;
                replace_port_cmd.m_stop_1  = 0;

                // Determine how much larger the packet needs to be to
                // handle the largest port addresses. There are 4 port address
                // strings (16 bytes) that needs to be replaced.
                replace_port_cmd.m_add_pkt_len = ((INET_PORTSTRLEN * 4) - 16);

                replace_port_cmd.m_server_ip.v4 = flow_info.server_ip;
                replace_port_cmd.m_start_port =  247;
                replace_port_cmd.m_stop_port  = 247+(4*4)+2+13;
                replace_port_cmd.m_client_port = lpP->rtp_client_1;
                replace_port_cmd.m_server_port = lpP->rtp_client_1;


                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_port_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        /* PLAY */
        case 9:
            {

                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 12;
                replace_cmd.m_stop_1  = 12+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        /*OPTION 0*/
        case 12:
            {

                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 15;
                replace_cmd.m_stop_1  = 15+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        /* option #2*/
        case 15:
            {

                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 15;
                replace_cmd.m_stop_1  = 15+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;

        case 18:
            {

                replace_cmd.m_cmd     = VM_REPLACE_IP_OFFSET;
                replace_cmd.m_flags   = 0;
                replace_cmd.m_start_0 = 16;
                replace_cmd.m_stop_1  = 16+9;

                // Determine how much larger the packet needs to be to
                // handle the largest IP address. There is a single address
                // string of 9 bytes that needs to be replaced.
                if (CGlobalInfo::is_ipv6_enable() ) {
                    // For IPv6, accomodate use of brackets (+2 bytes)
                    replace_cmd.m_add_pkt_len = (INET6_ADDRSTRLEN + 2) - 9;

                    // Mark as IPv6 and set the upper 96-bits
                    replace_cmd.m_flags |= CMiniVMCmdBase::MIN_VM_V6;
                    for (uint8_t idx=0; idx<6; idx++){
                        replace_cmd.m_server_ip.v6[idx] = CGlobalInfo::m_options.m_dst_ipv6[idx];
                    }
                } else {
                    replace_cmd.m_add_pkt_len = INET_ADDRSTRLEN - 9;
                }
                replace_cmd.m_server_ip.v4 = flow_info.server_ip;

                eop_cmd.m_cmd = VM_EOP;

                program[0] = &replace_cmd;
                program[1] = &eop_cmd;

                flow_info.vm_program = program;
            }
            break;


        }/* end of big switch on packet */
        break;

    case 1:
        flow_info.client_ip = node->m_src_ip ;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = lpP->rtp_client_0;
        /* this is tricky ..*/
        flow_info.server_port = lpP->rtp_client_0;
        flow_info.replace_server_port = true;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);

        break;
    case 2:
        flow_info.client_ip = node->m_src_ip ;
        flow_info.server_ip = node->m_dest_ip;
        flow_info.client_port = lpP->rtp_client_1;
        /* this is tricky ..*/
        flow_info.server_port = lpP->rtp_client_1;
        flow_info.replace_server_port =true;
        flow_info.is_init_ip_dir = (node->cur_pkt_ip_addr_dir() == CLIENT_SIDE?true:false);
        flow_info.is_init_port_dir = (node->cur_pkt_port_addr_dir() ==CLIENT_SIDE?true:false);

        break;
    default:
        assert(0);
        break;
    };

    //printf(" c_ip:%x s_ip:%x c_po:%x s_po:%x  init:%x  replace:%x \n",flow_info.client_ip,flow_info.server_ip,flow_info.client_port,flow_info.server_port,flow_info.is_init_dir,flow_info.replace_server_port);

    //printf(" program %p  \n",flow_info.vm_program);
    if ( unlikely( flow_info.vm_program != 0 ) ) {

        mbuf = pkt_info->do_generate_new_mbuf_ex_vm(node,&flow_info, &s_size);
    }else{
        if (unlikely (CGlobalInfo::is_ipv6_enable()) ) {
            // Request a larger initial segment for IPv6
            mbuf = pkt_info->do_generate_new_mbuf_ex_big(node,&flow_info);
        }else{
            mbuf = pkt_info->do_generate_new_mbuf_ex(node,&flow_info);
        }
    }

    // Fixup the TCP sequence numbers for the TCP flow
    if ( lpd->getFlowId() == 0 ) {
        uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);

        // Update TCP sequence numbers
        lpP->update(p, pkt_info, s_size);
    }

    return(mbuf);
}


/* replace the tuples */
rte_mbuf_t * CPluginCallbackSimple::on_node_generate_mbuf(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info){

    rte_mbuf_t * m=NULL;
    switch (plugin_id) {
    case mpRTSP:
        m=rtsp_plugin(plugin_id,node,pkt_info);
        break;
    case mpSIP_VOICE:
        m=sip_voice_plugin(plugin_id,node,pkt_info);
        break;
    case  mpDYN_PYLOAD:
        m=dyn_pyload_plugin(plugin_id,node,pkt_info);
        break;
    case mpAVL_HTTP_BROWSIN:
        m=http_plugin(plugin_id,node,pkt_info);
        break;
    default:
        assert(0);
    }
    return (m);
}


int CMiniVM::mini_vm_run(CMiniVMCmdBase * cmds[]){

    m_new_pkt_size=0;
    bool need_to_stop=false;
    int cnt=0;
    
    while (! need_to_stop) {
        CMiniVMCmdBase * cmd = cmds[cnt];
        switch (cmd->m_cmd) {
        case VM_REPLACE_IP_OFFSET:
            mini_vm_replace_ip((CMiniVMReplaceIP *)cmd);
            break;
        case VM_REPLACE_IP_PORT_OFFSET:
            mini_vm_replace_port_ip((CMiniVMReplaceIPWithPort *)cmd);
            break;
        case  VM_REPLACE_IP_PORT_RESPONSE_OFFSET:
            mini_vm_replace_ports((CMiniVMReplaceIPWithPort *)cmd);
            break;

        case  VM_REPLACE_IP_IP_PORT:
            mini_vm_replace_ip_ip_ports((CMiniVMReplaceIP_IP_Port * )cmd);
            break;

        case  VM_REPLACE_IPVIA_IP_IP_PORT:
            mini_vm_replace_ip_via_ip_ip_ports((CMiniVMReplaceIP_PORT_IP_IP_Port *)cmd);
            break;

        case VM_DYN_PYLOAD:
            mini_vm_dyn_payload((CMiniVMDynPyload *)cmd);
            break;

        case VM_EOP:
            need_to_stop=true;
            break;
        default:
            printf(" vm cmd %d does not exist \n",cmd->m_cmd);
            assert(0);
        }
        cnt++;
    }
    return (0);
}

inline int cp_pkt_len(char *to,char *from,uint16_t from_offset,uint16_t len){
    memcpy(to, from+from_offset , len);
    return (len);
}

/* not including the to_offset

 0 1
 x

*/
inline int cp_pkt_to_from(char *to,char *from,uint16_t from_offset,uint16_t to_offset){
    memcpy(to, from+from_offset , to_offset-from_offset) ;
    return (to_offset-from_offset);
}


int CMiniVM::mini_vm_dyn_payload( CMiniVMDynPyload * cmd){
    /* copy all the packet */
    CFlowYamlDpPkt * dyn=(CFlowYamlDpPkt *)cmd->m_ptr;
    uint16_t l7_offset = m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len = m_pkt_info->m_packet->pkt_len - l7_offset ;
    char * original_l7_ptr=m_pkt_info->m_packet->raw+l7_offset;
    char * p=m_pyload_mbuf_ptr;
    /* copy payload */
    memcpy(p,original_l7_ptr,len);
    if ( ( dyn->m_pyld_offset+ (dyn->m_len*4)) < ( len-4) ){
        // we can change the packet
        int i;
        uint32_t *l=(uint32_t *)(p+dyn->m_pyld_offset);
        for (i=0; i<dyn->m_len; i++) {
            if ( dyn->m_type==0 ) {
                *l=(rand() & dyn->m_pkt_mask);
            }else if (dyn->m_type==1){
                *l=(PKT_NTOHL(cmd->m_ip.v4) & dyn->m_pkt_mask);
            }
            l++;
        }

    }

    // Return packet size which hasn't changed
    m_new_pkt_size = m_pkt_info->m_packet->pkt_len;

    return (0);
}


int CMiniVM::mini_vm_replace_ip_via_ip_ip_ports(CMiniVMReplaceIP_PORT_IP_IP_Port * cmd){
    uint16_t l7_offset = m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len = m_pkt_info->m_packet->pkt_len - l7_offset;
    char * original_l7_ptr=m_pkt_info->m_packet->raw+l7_offset;
    char * p=m_pyload_mbuf_ptr;

    p+=cp_pkt_to_from(p,original_l7_ptr,
                      0,
                      cmd->m_ip_via_start);

    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p+=ipv6_to_str(&cmd->m_ip_via,p);
    } else {
        p+=ip_to_str(cmd->m_ip_via.v4,p);
    }
    p+=sprintf(p,":%u",cmd->m_port_via);

    /* up to the IP */
    p+=cp_pkt_to_from(p,original_l7_ptr,
                      cmd->m_ip_via_stop,
                      cmd->m_ip0_start);
    /*IP */
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p[-2] = '6';
        p+=ipv6_to_str(&cmd->m_ip,p);
    } else {
        p+=ip_to_str(cmd->m_ip.v4,p);
    }
    /* up to IP 2 */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_ip0_stop,
                   cmd->m_ip1_start);
    /* IP2 */
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p[-2] = '6';
        p+=ipv6_to_str(&cmd->m_ip,p);
    } else {
        p+=ip_to_str(cmd->m_ip.v4,p);
    }

    /* up to port */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_ip1_stop,
                   cmd->m_port_start);
    /* port */
    p+=sprintf(p,"%u",cmd->m_port);

    /* up to end */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_port_stop,
                   len);

    // Determine new packet size
    m_new_pkt_size= ((p+l7_offset) - m_pyload_mbuf_ptr);

    return (0);
}


int CMiniVM::mini_vm_replace_ip_ip_ports(CMiniVMReplaceIP_IP_Port * cmd){
    uint16_t l7_offset = m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len = m_pkt_info->m_packet->pkt_len - l7_offset;
    char * original_l7_ptr=m_pkt_info->m_packet->raw+l7_offset;
    char * p=m_pyload_mbuf_ptr;

    /* up to the IP */
    p+=cp_pkt_to_from(p,original_l7_ptr,
                      0,
                      cmd->m_ip0_start);
    /*IP */
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p+=ipv6_to_str(&cmd->m_ip,p);
    } else {
        p+=ip_to_str(cmd->m_ip.v4,p);
    }
    /* up to IP 2 */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_ip0_stop,
                   cmd->m_ip1_start);
    /* IP2 */
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p+=ipv6_to_str(&cmd->m_ip,p);
    } else {
        p+=ip_to_str(cmd->m_ip.v4,p);
    }

    /* up to port */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_ip1_stop,
                   cmd->m_port_start);
    /* port */
    p+=sprintf(p,"%u",cmd->m_port);

    /* up to end */
    p+=cp_pkt_to_from(p, original_l7_ptr ,
                   cmd->m_port_stop,
                   len);

    // Determine new packet size
    m_new_pkt_size= ((p+l7_offset) - m_pyload_mbuf_ptr);

    return (0);
}

int CMiniVM::mini_vm_replace_ports(CMiniVMReplaceIPWithPort * cmd){
    uint16_t l7_offset = m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len = m_pkt_info->m_packet->pkt_len - l7_offset;
    char * original_l7_ptr=m_pkt_info->m_packet->raw+l7_offset;

    memcpy(m_pyload_mbuf_ptr, original_l7_ptr,cmd->m_start_port);
    char * p=m_pyload_mbuf_ptr+cmd->m_start_port;
    p+=sprintf(p,"%u-%u;server_port=%u-%u",cmd->m_client_port,cmd->m_client_port+1,cmd->m_server_port,cmd->m_server_port+1);
    memcpy(p, original_l7_ptr+cmd->m_stop_port,len-cmd->m_stop_port);
    p+=(len-cmd->m_stop_port);

    // Determine new packet size
    m_new_pkt_size= ((p+l7_offset) - m_pyload_mbuf_ptr);

    return (0);
}


int CMiniVM::mini_vm_replace_port_ip(CMiniVMReplaceIPWithPort * cmd){
    uint16_t l7_offset=m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len = m_pkt_info->m_packet->pkt_len - l7_offset - 0;
    char * original_l7_ptr=m_pkt_info->m_packet->raw+l7_offset;

    memcpy(m_pyload_mbuf_ptr, original_l7_ptr,cmd->m_start_0);
    char *p=m_pyload_mbuf_ptr+cmd->m_start_0;
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        p+=ipv6_to_str(&cmd->m_server_ip,p);
    } else {
        p+=ip_to_str(cmd->m_server_ip.v4,p);
    }
    /* copy until the port start offset */
    int len1=cmd->m_start_port-cmd->m_stop_1 ;
    memcpy(p, original_l7_ptr+cmd->m_stop_1,len1);
    p+=len1;
    p+=sprintf(p,"%u-%u",cmd->m_client_port,cmd->m_client_port+1);
    memcpy(p, original_l7_ptr+cmd->m_stop_port,len-cmd->m_stop_port);
    p+=len-cmd->m_stop_port;

    // Determine new packet size
    m_new_pkt_size= ((p+l7_offset) - m_pyload_mbuf_ptr);

    return (0);
}

int CMiniVM::mini_vm_replace_ip(CMiniVMReplaceIP * cmd){
    uint16_t l7_offset = m_pkt_info->m_pkt_indication.getFastPayloadOffset();
    uint16_t len       = m_pkt_info->m_packet->pkt_len - l7_offset;
    char * original_l7_ptr = m_pkt_info->m_packet->raw+l7_offset;

    memcpy(m_pyload_mbuf_ptr, original_l7_ptr,cmd->m_start_0);
    char *p=m_pyload_mbuf_ptr+cmd->m_start_0;

    int n_size=0;
    if (cmd->m_flags & CMiniVMCmdBase::MIN_VM_V6) {
        n_size=ipv6_to_str(&cmd->m_server_ip,p);
    } else {
        n_size=ip_to_str(cmd->m_server_ip.v4,p);
    }
    p+=n_size;
    memcpy(p, original_l7_ptr+cmd->m_stop_1,len-cmd->m_stop_1);

    // Determine new packet size
    m_new_pkt_size= ((p+l7_offset+(len-cmd->m_stop_1)) - m_pyload_mbuf_ptr);

    return (0);
}


void CFlowYamlDpPkt::Dump(FILE *fd){
    fprintf(fd," pkt_id   : %d \n",(int)m_pkt_id);
    fprintf(fd," offset   : %d \n",(int)m_pyld_offset);
    fprintf(fd," offset   : %d \n",(int)m_type);
    fprintf(fd," len      : %d \n",(int)m_len);
    fprintf(fd," mask     : 0x%x \n",(int)m_pkt_mask);
}


void CFlowYamlDynamicPyloadPlugin::Add(CFlowYamlDpPkt & fd){
    if (m_num ==MAX_PYLOAD_PKT_CHANGE) {
        fprintf (stderr,"ERROR can set only %d rules \n",MAX_PYLOAD_PKT_CHANGE);
        exit(-1);
    }
    m_pkt_ids[m_num]=fd.m_pkt_id;
    m_program[m_num]=fd;
    m_num+=1;
}

void CFlowYamlDynamicPyloadPlugin::Dump(FILE *fd){
    int i;
    fprintf(fd," pkts :");
    for (i=0; i<m_num; i++) {
        fprintf(fd," %d ",m_pkt_ids[i]);
    }
    fprintf(fd,"\n");
    for (i=0; i<m_num; i++) {
        fprintf(fd," program : %d \n",i);
        fprintf(fd,"---------------- \n");
        m_program[i].Dump(fd);
    }
}

/* free the right object.
   it is classic to use virtual function but we can't do it here and we don't even want to use callback function
   as we want to save space and in most cases there is nothing to free.
   this might be changed in the future
 */
void CGenNodeBase::free_base(){
    if ( m_type == FLOW_PKT ) {
         CGenNode* p=(CGenNode*)this;
         p->free_gen_node();
        return;
    }
    if (m_type==STATELESS_PKT) {
         CGenNodeStateless* p=(CGenNodeStateless*)this;
         p->free_stl_node();
        return;
    }

    if (m_type == PCAP_PKT) {
        CGenNodePCAP *p = (CGenNodePCAP *)this;
        p->destroy();
        return;
    }

    if ( m_type == COMMAND ) {
         CGenNodeCommand* p=(CGenNodeCommand*)this;
         p->free_command();
    }

}

