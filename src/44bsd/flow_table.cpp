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

#include "astf/astf_db.h"
#include "tcp_var.h"
#include "flow_stat_parser.h"
#include "flow_table.h"
#include "../astf/trex_astf.h"
#include "trex_global.h"
#include "trex_capture.h"
#include "trex_port.h"
#include <common/Network/Packet/MPLSHeader.h>

void CSttFlowTableStats::Clear(){
    memset(&m_sts,0,sizeof(m_sts));

}

#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)

void CSttFlowTableStats::Dump(FILE *fd){
    MYC(m_err_no_syn);
    MYC(m_err_len_err);
    MYC(m_err_fragments_ipv4_drop);
    MYC(m_err_no_tcp_udp);
    MYC(m_err_client_pkt_without_flow);
    MYC(m_err_no_template);
    MYC(m_err_no_memory);
    MYC(m_err_duplicate_client_tuple);
    MYC(m_err_l3_cs);
    MYC(m_err_l4_cs);
    MYC(m_err_redirect_rx);
    MYC(m_redirect_rx_ok);
    MYC(m_err_rx_throttled);
    MYC(m_err_flow_overflow);
    MYC(m_err_c_nf_throttled);
    MYC(m_err_s_nf_throttled);
    MYC(m_defer_template);
    MYC(m_err_defer_no_template);
    MYC(m_rss_redirect_rx);
    MYC(m_rss_redirect_tx);
    MYC(m_rss_redirect_drops);
    MYC(m_rss_redirect_queue_full);
    MYC(m_ignored_macs);
    MYC(m_ignored_ips);
}


void CFlowKeyTuple::dump(FILE *fd){
    fprintf(fd,"m_src_ip   : %lu \n",(ulong)get_src_ip());
    fprintf(fd,"m_sport    : %lu \n",(ulong)get_sport());
    fprintf(fd,"m_dst_ip   : %lu \n",(ulong)get_dst_ip());
    fprintf(fd,"m_dport    : %lu \n",(ulong)get_dport());
    fprintf(fd,"m_proto    : %lu \n",(ulong)get_proto());
    fprintf(fd,"m_ipv4     : %lu \n",(ulong)get_is_ipv4());
    fprintf(fd,"hash       : %u \n",get_hash());
}


bool CFlowTable::Create(uint32_t size,
                        bool client_side){
    m_verbose =false;
    m_client_side = client_side;
    if (!m_ft.Create(size) ){
        printf("ERROR can't allocate flow-table \n");
        return(false);
    }
    reset_stats();
    m_tcp_api=(CEmulAppApi    *)0;
    m_udp_api=(CEmulAppApi    *)0;

    m_service_status        = SERVICE_OFF;
    m_service_filtered_mask = 0;

    m_tcp_lro = nullptr;
    if (!CGlobalInfo::m_options.preview.get_dev_lro_support() &&
        !CGlobalInfo::m_options.preview.getLroOffloadDisable()) {
        using namespace std::placeholders;
        auto cb = std::bind(&CFlowTable::process_tcp_packet, this, _1, _2, _3, _4, _5);

        m_tcp_lro = new CTcpRxOffload;
        m_tcp_lro->Create(cb);
    }
    m_black_list = 0;
    return(true);
}

void CFlowTable::Delete(){
    m_ft.Delete();
    if (m_tcp_lro) {
        m_tcp_lro->Delete();
        delete m_tcp_lro;
    }
}


void CFlowTable::dump(FILE *fd){
    m_ft.Dump(stdout);
    m_sts.Dump(stdout);
}


void CFlowTable::reset_stats(){
    m_sts.Clear();
}


void CFlowTable::redirect_to_rx_core(CTcpPerThreadCtx * ctx,
                                     struct rte_mbuf * mbuf){
    if (ctx->m_cb->on_redirect_rx(ctx,mbuf)!=0){
        FT_INC_SCNT(m_err_redirect_rx);
    }else{
        FT_INC_SCNT(m_redirect_rx_ok);
    }
}


void CFlowTable::parse_packet(struct rte_mbuf * mbuf,
                              CSimplePacketParser & parser,
                              CFlowKeyTuple & tuple,
                              CFlowKeyFullTuple & ftuple,
                              bool rx_l4_check,
                              tcp_rx_pkt_action_t & action,
                              tvpid_t port_id){
    action=tDROP;

    if (!parser.Parse()){
        action=tREDIRECT_RX_CORE;
        return;
    }
    uint32_t pkt_len=0;

    if ( (parser.m_protocol != IPHeader::Protocol::TCP) && 
         (parser.m_protocol != IPHeader::Protocol::UDP) ){
        FT_INC_SCNT(m_err_no_tcp_udp);
        action=tREDIRECT_RX_CORE;
        return;
    }
    /* TCP/UDP, only supported right now */
    
    /* Service filtered check, default is SERVICE OFF */
    if ( m_service_status != SERVICE_OFF ) {
        check_service_filter(parser, action);
        if ( action != tPROCESS ) {
            return;
        }
    }

    uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);
    uint32_t mbuf_pkt_len=rte_pktmbuf_pkt_len(mbuf);
    /* check packet length and checksum in case of TCP */

    CFlowKeyFullTuple *lpf= &ftuple;

    if (parser.m_ipv4) {
        /* IPv4 */
        lpf->m_ipv4      =true;
        lpf->m_l3_offset = (uintptr_t)parser.m_ipv4 - (uintptr_t)p;
        IPHeader *   ipv4= parser.m_ipv4;
        TCPUDPHeaderBase    * lpL4 = (TCPUDPHeaderBase *)parser.m_l4;
        if ( m_client_side ) {
            tuple.set_src_ip(ipv4->getDestIp());
            tuple.set_sport(lpL4->getDestPort());
            tuple.set_dst_ip(ipv4->getSourceIp());
            tuple.set_dport(lpL4->getSourcePort());
        }else{
            tuple.set_dst_ip(ipv4->getDestIp());
            tuple.set_dport(lpL4->getDestPort());
            tuple.set_src_ip(ipv4->getSourceIp());
            tuple.set_sport(lpL4->getSourcePort());
        }
        if (ipv4->getTotalLength()<IPV4_HDR_LEN) {
            FT_INC_SCNT(m_err_len_err);
            return;
        }
        /* fragments is supported right now */
        if (ipv4->isFragmented()){
            FT_INC_SCNT(m_err_fragments_ipv4_drop);
            return;
        }
        pkt_len = ipv4->getTotalLength() + lpf->m_l3_offset;
    }else{
        lpf->m_ipv4      =false;
        lpf->m_l3_offset = (uintptr_t)parser.m_ipv6 - (uintptr_t)p;

        IPv6Header *   ipv6= parser.m_ipv6;
        TCPUDPHeaderBase    * lpL4 = (TCPUDPHeaderBase *)parser.m_l4;

        if ( m_client_side ) {
            tuple.set_src_ip(ipv6->getDestIpv6LSB());
            tuple.set_sport(lpL4->getDestPort());
            tuple.set_dst_ip(ipv6->getSourceIpv6LSB());
            tuple.set_dport(lpL4->getSourcePort());
        }else{
            tuple.set_src_ip(ipv6->getSourceIpv6LSB());
            tuple.set_sport(lpL4->getSourcePort());
            tuple.set_dst_ip(ipv6->getDestIpv6LSB());
            tuple.set_dport(lpL4->getDestPort());
        }
        /* TBD need to find the last IPv6 header and skip  */
        pkt_len = ipv6->getPayloadLen()+ lpf->m_l3_offset + IPV6_HDR_LEN;
    }

    /* the reported packet (from headers) is bigger than real one */
    if (pkt_len > mbuf_pkt_len){
        FT_INC_SCNT(m_err_len_err);
        return;
    }

    lpf->m_proto     =   parser.m_protocol;
    lpf->m_l4_offset = (uintptr_t)parser.m_l4 - (uintptr_t)p;

    uint8_t l4_header_len=0;
    if ( lpf->m_proto == IPHeader::Protocol::UDP){
        l4_header_len=UDP_HEADER_LEN;
    }else{
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        l4_header_len = lpTcp->getHeaderLength();
        if ( l4_header_len < TCP_HEADER_LEN ) {
            FT_INC_SCNT(m_err_len_err);
            return;
        }
    }

    lpf->m_l7_offset = lpf->m_l4_offset + l4_header_len;

    /* check TCP header size */
    if (pkt_len < lpf->m_l7_offset ) {
        FT_INC_SCNT(m_err_len_err);
        return;
    }
    lpf->m_l7_total_len  =  pkt_len - lpf->m_l7_offset;

    if ( (mbuf->ol_flags & RTE_MBUF_F_RX_IP_CKSUM_MASK) ==  RTE_MBUF_F_RX_IP_CKSUM_BAD ){
        FT_INC_SCNT(m_err_l3_cs);
        return;
    }

    if ( rx_l4_check &&  ((mbuf->ol_flags & RTE_MBUF_F_RX_L4_CKSUM_BAD) ==  RTE_MBUF_F_RX_L4_CKSUM_BAD) ){
        FT_INC_SCNT(m_err_l4_cs);
        return;
    }

    action=tPROCESS;
    tuple.set_proto(lpf->m_proto);
    tuple.set_ipv4(lpf->m_ipv4);
}

void CFlowTable::check_service_filter(CSimplePacketParser & parser, tcp_rx_pkt_action_t & action) {
    if ( (get_astf_object()->get_state() == AstfProfileState::STATE_IDLE) && (m_service_status == SERVICE_ON) ) {
        action = tREDIRECT_RX_CORE;
        return;
    }
    if ( m_service_status == SERVICE_ON ){
                action = tREDIRECT_RX_CORE;
                return;
    }

    if (m_service_filtered_mask & TrexPort::BGP) {
        if ( parser.m_protocol == IPPROTO_TCP ) {
            TCPHeader *l4_header = (TCPHeader *)parser.m_l4;
            uint16_t src_port = l4_header->getSourcePort();
            uint16_t dst_port = l4_header->getDestPort();
            if ( src_port == BGP_PORT || dst_port == BGP_PORT ) {
                action = tREDIRECT_RX_CORE;
                return;
            }
        }
    }

    if (m_service_filtered_mask & TrexPort::DHCP) {
        if ( parser.m_protocol == IPPROTO_UDP ) {
            UDPHeader *l4_header = (UDPHeader *)parser.m_l4;
            uint16_t src_port = l4_header->getSourcePort();
            uint16_t dst_port = l4_header->getDestPort();
            if ( (( src_port == DHCPv4_PORT || dst_port == DHCPv4_PORT ))  ||
                 (( src_port == DHCPv6_PORT || dst_port == DHCPv6_PORT ))) {
                action = tREDIRECT_RX_CORE;
                return;
            }
        }
    }

    if ( (m_service_filtered_mask & TrexPort::TRANSPORT) && 
            ((parser.m_protocol == IPPROTO_UDP) || (parser.m_protocol == IPPROTO_TCP)) ) {
        UDPHeader *l4_header = (UDPHeader *)parser.m_l4;
        uint16_t src_port = l4_header->getSourcePort();
        uint16_t dst_port = l4_header->getDestPort();
        if ( (dst_port & 0xff00) == 0xff00 || (src_port & 0xff00) == 0xff00 ) {
            action = tREDIRECT_RX_CORE;
            return;
        }
    }

    if ( (m_service_filtered_mask & TrexPort::MDNS) && (parser.m_protocol == IPPROTO_UDP) ) {
        UDPHeader *l4_header = (UDPHeader *)parser.m_l4;
        uint16_t src_port = l4_header->getSourcePort();
        uint16_t dst_port = l4_header->getDestPort();
        if (dst_port == MDNS_PORT && src_port == MDNS_PORT) {
            // In MDNS both ports should equal.
            action = tREDIRECT_RX_CORE;
            return;
        }
    }

    action = tPROCESS;

}

static void on_flow_free_cb(void *userdata,void  *obh){
    CFlowTable * ft = (CFlowTable *)userdata;
    CFlowBase * flow = CFlowBase::cast_from_hash_obj((flow_hash_ent_t *)obh);
    ft->terminate_flow(flow->m_pctx->m_ctx, flow, false);
}

void CFlowTable::terminate_flow(CTcpPerThreadCtx * ctx,
                                CFlowBase * flow,
                                bool remove_from_ft){
    uint16_t tg_id = flow->m_tg_id;
    if ( !flow->is_udp() ){
        if ( !((CTcpFlow*)flow)->is_activated() ) {
            FT_INC_SCNT(m_err_defer_no_template);
        }
        else {
            INC_STAT(flow->m_pctx, tg_id, tcps_testdrops);
            INC_STAT(flow->m_pctx, tg_id, tcps_closed);
            flow->m_pctx->set_time_closed();
        }
    }
    handle_close(ctx, flow, remove_from_ft);
}

void CFlowTable::terminate_all_flows(){
    m_ft.detach_all(this,on_flow_free_cb);
}

HOT_FUNC void CFlowTable::handle_close(CTcpPerThreadCtx * ctx,
                              CFlowBase * flow,
                              bool remove_from_ft){
    ctx->m_cb->on_flow_close(ctx,flow);
    if ( remove_from_ft ){
        m_ft.remove(&flow->m_hash);
    }
    free_flow(flow);
}

void tcp_respond_rst(CTcpFlow* flow, TCPHeader* lpTcp, CFlowKeyFullTuple &ftuple) {
    if ( lpTcp->getAckFlag() ){
        tcp_int_respond(&flow->m_tcp,
                     0,
                     lpTcp->getAckNumber(),
                     TH_RST);
    }else{
        int tlen=ftuple.m_l7_total_len;
        if (lpTcp->getSynFlag()) {
            tlen++;
        }
        if (lpTcp->getFinFlag()) {
            tlen++;
        }
        tcp_seq seq=0;
        if (tlen==0 && (lpTcp->getFlags()==0)) {
            /* keep-alive packet  */
            seq = lpTcp->getAckNumber();
        }
        tcp_int_respond(&flow->m_tcp,
                     lpTcp->getSeqNumber()+tlen,
                     seq,
                     TH_RST|TH_ACK);
    }
}


bool CFlowTable::update_new_template(CTcpPerThreadCtx * ctx,
                                    struct rte_mbuf * mbuf,
                                    CTcpFlow *  flow,
                                    TCPHeader    * lpTcp,
                                    CFlowKeyFullTuple &ftuple){
    assert(!flow->is_activated());
    if (flow->m_template_info) {
        flow->update_new_template_assoc_info();
        assert(flow->is_activated()); /* flow is now activated */
        flow->m_pctx->set_flow_info(flow);
        flow->m_app.start(false);
    } else {    /* no matched template found, now mbuf should be freed */
        CPerProfileCtx* pctx = FALLBACK_PROFILE_CTX(ctx);
        uint8_t mask = 3;
        // if gtpu is enable ignore
        bool ignore = CGlobalInfo::m_options.m_tunnel_enabled;
        // black_list is not empty
        if (m_black_list & mask && !ignore) {
            ignore = ignore_packet(ctx, mbuf, ftuple);
        }
        if (pctx->m_tunable_ctx.tcp_blackhole == 0 && !ignore) {
            tcp_respond_rst(flow, lpTcp, ftuple);
        }
        FT_INC_SCNT(m_err_defer_no_template);
        handle_close(ctx,flow,true);
        return false;   /* flow is not available */
    }
    return true;
}

static inline void tcp_flow_input(CTcpFlow *  flow,
                                  struct rte_mbuf * mbuf,
                                  TCPHeader    * lpTcp,
                                  CFlowKeyFullTuple &ftuple){

    uint8_t iptos;
    if (ftuple.m_ipv4) {
        iptos = (rte_pktmbuf_mtod_offset(mbuf, IPHeader*, ftuple.m_l3_offset))->getTOS();
    } else {
        iptos = (rte_pktmbuf_mtod_offset(mbuf, IPv6Header*, ftuple.m_l3_offset))->getTrafficClass();
    }

    tcp_int_input(&flow->m_tcp, (struct mbuf*)mbuf, (struct tcphdr*)lpTcp, ftuple.m_l7_offset, ftuple.m_l7_total_len, iptos);

    flow->check_defer_function();
}

void HOT_FUNC CFlowTable::process_tcp_packet(CTcpPerThreadCtx * ctx,
                                    CTcpFlow *  flow,
                                    struct rte_mbuf * mbuf,
                                    TCPHeader    * lpTcp,
                                    CFlowKeyFullTuple &ftuple){
    TCPHeader tcphdr;
    if (unlikely(!flow->is_activated())) {
        tcphdr = *lpTcp;
    }

    tcp_flow_input(flow, mbuf, lpTcp, ftuple);

    if (unlikely(flow->new_template_identified())) {
        /* identifying new template has been done */
        if (!update_new_template(ctx, mbuf, flow, &tcphdr, ftuple)) {
            return; /* when update is failed, flow is not available also. */
        }
    }

    if (flow->is_can_close() && ctx) {
        handle_close(ctx,flow,true);
    }
}

void       CFlowTable::generate_rst_pkt(CPerProfileCtx * pctx,
                                         uint32_t src,
                                         uint32_t dst,
                                         uint16_t src_port,
                                         uint16_t dst_port,
                                         tunnel_cfg_data_t tunnel_data,
                                         bool is_ipv6,
                                        TCPHeader    * lpTcp,
                                        uint8_t *   pkt,
                                        IPv6Header *    ipv6,
                                        CFlowKeyFullTuple &ftuple,
                                        tvpid_t port_id){
   /* TBD could be done much faster, but this is a corner case and there is no need to improve this 
      allocate flow, 
      fill information
      generate RST
      free flow 
   */

    if ( lpTcp->getResetFlag() ){
        /* do not reply with RST to RST */
        return;
    }


    // This is intentionally left without tg_id
    CTcpFlow * flow=alloc_flow(pctx,
                                 src,
                                 dst,
                                 src_port,
                                 dst_port,
                                 tunnel_data,
                                 is_ipv6,
                                 NULL);
    if (flow==0) {
        return;
    }

    flow->m_template.server_update_mac(pkt, pctx, port_id);
    if (is_ipv6) {
        /* learn the ipv6 headers */
        flow->m_template.learn_ipv6_headers_from_network(ipv6);
    }

    tcp_respond_rst(flow, lpTcp, ftuple);

    free_flow(flow);
}

CUdpFlow * CFlowTable::alloc_flow_udp(CPerProfileCtx * pctx,
                                  uint32_t src,
                                  uint32_t dst,
                                  uint16_t src_port,
                                  uint16_t dst_port,
                                  tunnel_cfg_data_t tunnel_data,
                                  bool is_ipv6,
                                  void *tunnel_ctx,
                                  bool client,
                                  uint16_t tg_id,
                                  uint16_t template_id){
    CUdpFlow * flow = new (std::nothrow) CUdpFlow();
    if (flow == 0 ) {
        FT_INC_SCNT(m_err_no_memory);
        return((CUdpFlow *)0);
    }
    flow->Create(pctx, client, tg_id);
    flow->m_c_template_idx = template_id;
    flow->m_template.set_tuple(src,dst,src_port,dst_port,tunnel_data,IPHeader::Protocol::UDP,tunnel_ctx,is_ipv6);
    flow->init();
    flow->m_pctx->m_flow_cnt++;
    return(flow);
}

CTcpFlow * CFlowTable::alloc_flow(CPerProfileCtx * pctx,
                                  uint32_t src,
                                  uint32_t dst,
                                  uint16_t src_port,
                                  uint16_t dst_port,
                                  tunnel_cfg_data_t tunnel_data,
                                  bool is_ipv6,
                                  void *tunnel_ctx,
                                  uint16_t tg_id,
                                  uint16_t template_id){
    CTcpFlow * flow = new (std::nothrow) CTcpFlow();
    if (flow == 0 ) {
        FT_INC_SCNT(m_err_no_memory);
        return((CTcpFlow *)0);
    }
    flow->Create(pctx, tg_id);
    flow->m_c_template_idx = template_id;
    flow->m_template.set_tuple(src,dst,src_port,dst_port,tunnel_data,IPHeader::Protocol::TCP,tunnel_ctx,is_ipv6);
    flow->init();
    flow->m_pctx->m_flow_cnt++;
    return(flow);
}

HOT_FUNC void CFlowTable::free_flow(CFlowBase * flow){
    assert(flow);
    CPerProfileCtx* pctx = flow->m_pctx;
    flow->m_pctx->m_flow_cnt--;
    flow->m_pctx->on_flow_close();

    if ( flow->is_udp() ){
        CUdpFlow * udp_flow=(CUdpFlow *)flow;
        udp_flow->Delete();
        delete udp_flow;
    }else{
        CTcpFlow * tcp_flow=(CTcpFlow *)flow;
        flow->m_pctx->clear_flow_info(tcp_flow);
        tcp_flow->Delete();
        delete tcp_flow;
    }

    if (pctx->is_on_flow()) {
        assert(pctx->m_flow_cnt == 0);
        delete pctx;
    }
}

bool CFlowTable::insert_new_flow(CFlowBase *  flow,
                                 CFlowKeyTuple  & tuple){

    flow_key_t key=tuple.get_flow_key();
    uint32_t  hash=tuple.get_hash();
    flow->m_hash.key =key;

    HASH_STATUS  res=m_ft.insert(&flow->m_hash,hash);
    if (res!=hsOK) {
        m_sts.m_sts.m_err_duplicate_client_tuple++;
        /* duplicate */
        return(false);
    }
    return(true);
}

void CFlowTable::rx_non_process_packet(tcp_rx_pkt_action_t action,
                                       CTcpPerThreadCtx * ctx,
                                       struct rte_mbuf * mbuf){
    if (action == tDROP) {
        rte_pktmbuf_free(mbuf);
    }else{
        assert(action == tREDIRECT_RX_CORE);
        redirect_to_rx_core(ctx,mbuf);
    }
}

#undef FLOW_TABLE_DEBUG

#ifdef FLOW_TABLE_DEBUG
static int pkt_cnt=0;
#endif



void CFlowTable::process_udp_packet(CTcpPerThreadCtx * ctx,
                                    CUdpFlow *  flow,
                                    struct rte_mbuf * mbuf,
                                    UDPHeader    * lpUDP,
                                    CFlowKeyFullTuple &ftuple){

    flow->on_rx_packet(mbuf,
                       lpUDP,
                       ftuple.m_l7_offset,
                       ftuple.m_l7_total_len);

    flow->check_defer_function();

    if (flow->is_can_closed()) {
        handle_close(ctx,flow,true);
    }
}

bool CFlowTable::ignore_packet(CTcpPerThreadCtx * ctx,
                               struct rte_mbuf * mbuf,
                               CFlowKeyFullTuple &ftuple) {

    if (!ctx->is_ignored_macs_empty()) {
        uint8_t* p = rte_pktmbuf_mtod(mbuf, uint8_t*);
        uint64_t mac_addr = 0;
        uint8_t* mac_p = (uint8_t*)&mac_addr;
        memcpy((void*)(mac_p + 2), (void*)p, 6);
        if (ctx->mac_lookup(mac_addr)) {
            FT_INC_SCNT(m_ignored_macs);
            return true;
        }
    }
    if (!ctx->is_ignored_ips_empty()) {
        IPHeader *iph = rte_pktmbuf_mtod_offset (mbuf, IPHeader *, ftuple.m_l3_offset);
        if (iph->getVersion() == IPHeader::Protocol::IP) {
            if (ctx->ip_lookup(iph->getSourceIp())) {
                FT_INC_SCNT(m_ignored_ips);
                return true;
            }
        }
    }
    return false;
}

bool CFlowTable::rx_handle_stateless_packet(CTcpPerThreadCtx * ctx,
                                            struct rte_mbuf * mbuf,
                                            tvpid_t port_id){
    TrexStatelessDpCore* dp_core = (TrexStatelessDpCore*)ctx->m_thread->get_dp_core();
    if (!dp_core->is_feature_set(TrexStatelessDpCore::LATENCY))
        return false;

    RXLatency* fs_latency = dp_core->get_fs_latency_object_ptr(!m_client_side);

    fs_latency->handle_pkt(mbuf, port_id);

    return true;
}

bool CFlowTable::rx_handle_packet_udp_no_flow(CTcpPerThreadCtx * ctx,
                                              struct rte_mbuf * mbuf,
                                              flow_hash_ent_t * lpflow,
                                              CSimplePacketParser & parser,
                                              CFlowKeyTuple & tuple,
                                              CFlowKeyFullTuple & ftuple,
                                              uint32_t  hash,
                                              tvpid_t port_id
                                              ){
    CUdpFlow * flow;

    /* first in flow */
    UDPHeader    * lpUDP = (UDPHeader *)parser.m_l4;

    /* not found in flowtable , we are generating the flows*/
    if ( m_client_side ){
        rx_handle_stateless_packet(ctx, mbuf, port_id);
        FT_INC_SCNT(m_err_client_pkt_without_flow);
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    /* Patch */
    uint32_t dest_ip;
    bool is_ipv6=false;
    if (parser.m_ipv4){
        IPHeader *  ipv4 = (IPHeader *)parser.m_ipv4;    
        dest_ip=ipv4->getDestIp();
    }else{
        IPv6Header *   ipv6= parser.m_ipv6;
        dest_ip =ipv6->getDestIpv6LSB();
        is_ipv6=true;
    }

    uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t*);

    /* TBD Parser need to be fixed */
    tunnel_cfg_data_t tunnel_data;
    if (parser.m_vlan_offset) {
        VLANHeader * lpVlan=(VLANHeader *)(pkt+14+parser.m_mpls_offset);
        tunnel_data.m_vlan = lpVlan->getVlanTag();
    }

    if (parser.m_mpls_offset !=0) {
        MPLSHeader *lpMpls = (MPLSHeader *)(pkt + 14);
        tunnel_data.m_mpls.label = lpMpls->getLabel();
        tunnel_data.m_mpls.tc = lpMpls->getTc();
        tunnel_data.m_mpls.s = lpMpls->getBottomOfStack();
        tunnel_data.m_mpls.ttl = lpMpls->getTtl();
    }

    uint16_t dst_port = lpUDP->getDestPort();

    if (!ctx->is_any_profile()) {
        rx_handle_stateless_packet(ctx, mbuf, port_id);
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }

    CServerTemplateInfo *temp = ctx->get_template_info_by_port(dst_port,false);
    if (!temp) {
        uint8_t *l7_data = pkt + ftuple.m_l7_offset;
        uint16_t l7_len = ftuple.m_l7_total_len;
        temp = ctx->get_template_info(dst_port,false,dest_ip, l7_data,l7_len);
    }
    CPerProfileCtx *pctx = temp ? temp->get_profile_ctx(): nullptr;
    CTcpServerInfo *server_info = temp ? temp->get_server_info(): nullptr;

    if (!server_info || !pctx->is_open_flow_allowed()){
        if (!server_info) {
            rx_handle_stateless_packet(ctx, mbuf, port_id);
        }
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }

    CAstfDbRO *tcp_data_ro = pctx->m_template_ro;
    uint16_t c_template_idx = server_info->get_temp_idx();
    uint16_t tg_id = tcp_data_ro->get_template_tg_id(c_template_idx);

    if ( ctx->is_open_flow_enabled()==false ){
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_s_nf_throttled);
        return(false);
    }

    CEmulAppProgram *server_prog = server_info->get_prog();
    //CTcpTuneables *s_tune = server_info->get_tuneables();

    flow = ctx->m_ft.alloc_flow_udp(pctx, dest_ip, tuple.get_src_ip(),
                                    dst_port, tuple.get_sport(),
                                    tunnel_data, is_ipv6, NULL, false, tg_id,
                                    c_template_idx);

    if (flow == 0 ) {
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    //set the tunnel contexts to the server side in gtpu-loopback mode
    flow->set_tunnel_ctx();

    flow->m_template.server_update_mac(pkt, pctx, port_id);
    if (is_ipv6) {
        /* learn the ipv6 headers */
        flow->m_template.learn_ipv6_headers_from_network(parser.m_ipv6);
    }

    flow->m_c_template_idx = c_template_idx;

    flow_key_t key=tuple.get_flow_key();
    /* add to flow-table */
    flow->m_hash.key = key;

    /* no need to check, we just checked */
    m_ft.insert_nc(&flow->m_hash,hash);

    CEmulApp * app =&flow->m_app;

    app->set_program(server_prog);
    app->set_bh_api(m_udp_api);
    app->set_udp_flow_ctx(flow->m_pctx,flow);
    app->set_udp_flow();
    //flow->set_s_udp_info(tcp_data_ro, s_tune);  NOT needed for now 


    app->start(true); /* start the application */

    process_udp_packet(ctx,flow,mbuf,lpUDP,ftuple);
    return(true);
}


bool CFlowTable::rx_handle_packet_tcp_no_flow(CTcpPerThreadCtx * ctx,
                                              struct rte_mbuf * mbuf,
                                              flow_hash_ent_t * lpflow,
                                              CSimplePacketParser & parser,
                                              CFlowKeyTuple & tuple,
                                              CFlowKeyFullTuple & ftuple,
                                              uint32_t  hash,
                                              tvpid_t port_id
                                      ){
    CTcpFlow * lptflow;

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    /* server with SYN packet, it is OK 
      we need to build the flow and add it to the table */
    /* Patch */
    uint32_t dest_ip;
    bool is_ipv6=false;
    if (parser.m_ipv4){
        IPHeader *  ipv4 = (IPHeader *)parser.m_ipv4;    
        dest_ip=ipv4->getDestIp();
    }else{
        IPv6Header *   ipv6= parser.m_ipv6;
        dest_ip =ipv6->getDestIpv6LSB();
        is_ipv6=true;
    }

    uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t*);

    /* TBD Parser need to be fixed */
    tunnel_cfg_data_t tunnel_data;
    if (parser.m_vlan_offset) {
        VLANHeader * lpVlan=(VLANHeader *)(pkt+14+parser.m_mpls_offset);
        tunnel_data.m_vlan = lpVlan->getVlanTag();
    }

    if (parser.m_mpls_offset !=0) {
        MPLSHeader *lpMpls = (MPLSHeader *)(pkt + 14);
        tunnel_data.m_mpls.label = lpMpls->getLabel();
        tunnel_data.m_mpls.tc = lpMpls->getTc();
        tunnel_data.m_mpls.s = lpMpls->getBottomOfStack();
        tunnel_data.m_mpls.ttl = lpMpls->getTtl();
    }

    uint16_t dst_port = lpTcp->getDestPort();

    if (!ctx->is_any_profile()) {
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }
        /* not found in flowtable , we are generating the flows*/
    if ( m_client_side ){
        CPerProfileCtx* pctx = FALLBACK_PROFILE_CTX(ctx);
        uint8_t mask = 3;
        // if gtpu is enable ignore
        bool ignore = CGlobalInfo::m_options.m_tunnel_enabled;
        // black_list is not empty
        if (m_black_list & mask && !ignore) {
            ignore = ignore_packet(ctx, mbuf, ftuple);
        }
        if (pctx->m_tunable_ctx.tcp_blackhole == 0 && !ignore) {

            uint32_t source_ip;
            if (parser.m_ipv4){
                IPHeader *  ipv4 = (IPHeader *)parser.m_ipv4;    
                source_ip=ipv4->getSourceIp();
            }else{
                IPv6Header *   ipv6= parser.m_ipv6;
                source_ip =ipv6->getSourceIpv6LSB();
            }

            generate_rst_pkt(pctx,
                           dest_ip,
                           source_ip,
                           dst_port,
                           lpTcp->getSourcePort(),
                           tunnel_data,
                           is_ipv6,
                           lpTcp,
                           pkt,parser.m_ipv6,
                           ftuple,
                           port_id);
        }

        FT_INC_SCNT(m_err_client_pkt_without_flow);
        rte_pktmbuf_free(mbuf);
        return(false);
    }


    /* server side */
    if (  (lpTcp->getFlags() & TCPHeader::Flag::SYN) ==0 ) {
        /* no syn */
        CPerProfileCtx* pctx = FALLBACK_PROFILE_CTX(ctx);
        uint8_t mask = 3;
        // if gtpu is enable ignore
        bool ignore = CGlobalInfo::m_options.m_tunnel_enabled;
        // black_list is not empty
        if (m_black_list & mask && !ignore) {
            ignore = ignore_packet(ctx, mbuf, ftuple);
        }
        if (pctx->m_tunable_ctx.tcp_blackhole != 2 && !ignore) {
            generate_rst_pkt(pctx,
                             dest_ip,
                             tuple.get_src_ip(),
                             dst_port,
                             tuple.get_sport(),
                             tunnel_data,
                             is_ipv6,
                             lpTcp,
                             pkt,parser.m_ipv6,
                             ftuple,
                             port_id);

        }
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_syn);
        return(false);
    }

    CServerIpPayloadInfo *payload_info = nullptr;
    CServerTemplateInfo *temp = ctx->get_template_info(dst_port,true,dest_ip, &payload_info);
    CPerProfileCtx *pctx = temp ? temp->get_profile_ctx(): nullptr;
    CTcpServerInfo *server_info = temp ? temp->get_server_info(): nullptr;

    if (!server_info || !pctx->is_open_flow_allowed()) {
        if (!pctx) {
            pctx = FALLBACK_PROFILE_CTX(ctx);
        }
        uint8_t mask = 3;
        // if gtpu is enable ignore
        bool ignore = CGlobalInfo::m_options.m_tunnel_enabled;
        // black_list is not empty
        if (m_black_list & mask && !ignore) {
            ignore = ignore_packet(ctx, mbuf, ftuple);
        }
        if (pctx->m_tunable_ctx.tcp_blackhole == 0 && !ignore) {
          generate_rst_pkt(pctx,
                         dest_ip,
                         tuple.get_src_ip(),
                         dst_port,
                         tuple.get_sport(),
                         tunnel_data,
                         is_ipv6,
                         lpTcp,
                         pkt,parser.m_ipv6,
                         ftuple,
                         port_id);
        }

        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }

    CAstfDbRO *tcp_data_ro = pctx->m_template_ro;
    uint16_t c_template_idx = server_info->get_temp_idx();
    uint16_t tg_id = tcp_data_ro->get_template_tg_id(c_template_idx);

    if ( ctx->is_open_flow_enabled()==false ){
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_s_nf_throttled);
        return(false);
    }

    CEmulAppProgram *server_prog = server_info->get_prog();
    CTcpTuneables *s_tune = server_info->get_tuneables();

    lptflow = ctx->m_ft.alloc_flow(pctx,
                                   dest_ip,
                                   tuple.get_src_ip(),
                                   dst_port,
                                   tuple.get_sport(),
                                   tunnel_data,
                                   is_ipv6,
                                   NULL,
                                   tg_id,
                                   c_template_idx);



    if (lptflow == 0 ) {
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    //set the tunnel contexts to the server side in gtpu-loopback mode
    lptflow->set_tunnel_ctx();

    if (unlikely(temp->has_payload_params())) {
        /* payload params can be checked after it is received */
        pctx = lptflow->create_on_flow_profile();
        if (pctx == nullptr) {
            FT_INC_SCNT(m_err_no_memory);
            rte_pktmbuf_free(mbuf);
            return(false);
        }
        FT_INC_SCNT(m_defer_template);
    }

    lptflow->set_s_tcp_info(tcp_data_ro, s_tune);
    lptflow->m_template.server_update_mac(pkt, lptflow->m_pctx, port_id);
    if (is_ipv6) {
        /* learn the ipv6 headers */
        lptflow->m_template.learn_ipv6_headers_from_network(parser.m_ipv6);
    }

    lptflow->m_c_template_idx = c_template_idx;

    flow_key_t key=tuple.get_flow_key();
    /* add to flow-table */
    lptflow->m_hash.key = key;

    /* no need to check, we just checked */
    m_ft.insert_nc(&lptflow->m_hash,hash);

    CEmulApp * app =&lptflow->m_app;

    app->set_program(server_prog);
    app->set_bh_api(m_tcp_api);
    app->set_flow_ctx(lptflow->m_pctx,lptflow);
    if (CGlobalInfo::m_options.preview.getEmulDebug() ){
        app->set_log_enable(true);
    }
    app->set_debug_id(1);

    lptflow->set_app(app);

    if (unlikely(temp->has_payload_params())) {
        lptflow->start_identifying_template(pctx, payload_info);
    } else {
        lptflow->m_pctx->set_flow_info(lptflow);
        app->start(true); /* start the application */
    }

    /* start listen */
    tcp_listen(&lptflow->m_tcp);

    /* process SYN packet */
    process_tcp_packet(ctx,lptflow,mbuf,lpTcp,ftuple);

    return(true);
}


void CTcpRxOffloadBuf::update_mbuf(struct rte_mbuf* mbuf, TCPHeader* lpTcp, CFlowKeyFullTuple& ftuple) {
    m_mbuf = mbuf;
    m_lpTcp = lpTcp;
    m_ftuple = ftuple;
}

void CTcpRxOffloadBuf::set(CTcpFlow* flow, struct rte_mbuf* mbuf, TCPHeader* lpTcp, CFlowKeyFullTuple& ftuple) {
    update_mbuf(mbuf, lpTcp, ftuple);
    m_flow = flow;
    flow->m_lro_buf = this;
}

void CTcpRxOffloadBuf::clear_flow() {
    if (m_flow) {
        if (m_flow->m_lro_buf == this) {
            m_flow->m_lro_buf = nullptr;
        }
        m_flow = nullptr;
    }
}

void CTcpRxOffloadBuf::reset() {
    if (m_flow) {
        if (m_mbuf) {
            rte_pktmbuf_free(m_mbuf);
            m_mbuf = nullptr;
        }
        clear_flow();
    }
}

bool CTcpRxOffloadBuf::reassemble_mbuf(struct rte_mbuf * mbuf, TCPHeader* lpTcp, CFlowKeyFullTuple& ftuple) {
#ifndef TREX_SIM
    /* merge if the two packets are neighbors */
    if (m_ftuple.m_l7_total_len + ftuple.m_l7_total_len <= TCP_TSO_MAX_DEFAULT &&
        lpTcp->getHeaderLength() == m_lpTcp->getHeaderLength()) {

        if (likely(lpTcp->getSeqNumber() == m_lpTcp->getSeqNumber() + m_ftuple.m_l7_total_len)) {
            m_ftuple.m_l7_total_len += ftuple.m_l7_total_len;
            if (rte_pktmbuf_adj(mbuf, ftuple.m_l7_offset)) {
                rte_pktmbuf_chain(m_mbuf, mbuf);

                INC_STAT(m_flow->m_pctx, m_flow->m_tg_id, tcps_rcvoffloads);
                return true;
            }
        }
        else if (m_lpTcp->getSeqNumber() == lpTcp->getSeqNumber() + ftuple.m_l7_total_len) {
            ftuple.m_l7_total_len += m_ftuple.m_l7_total_len;
            if (rte_pktmbuf_adj(m_mbuf, m_ftuple.m_l7_offset)) {
                rte_pktmbuf_chain(mbuf, m_mbuf);

                update_mbuf(mbuf, lpTcp, ftuple);
                INC_STAT(m_flow->m_pctx, m_flow->m_tg_id, tcps_rcvoffloads);
                return true;
            }
        }
    }
#endif
    return false;
}


void CTcpRxOffload::Create(rx_offload_cb_t cb) {
    m_size = NUM_RX_OFFLOAD;
    m_bufs = new CTcpRxOffloadBuf[m_size];
    m_head = 0;
    m_tail = 0;

    m_cb = cb;
}

void CTcpRxOffload::Delete() {
    for (; m_tail < m_head; ++m_tail) {
        m_bufs[m_tail % m_size].reset();
    }
    delete[] m_bufs;
}

HOT_FUNC CTcpRxOffloadBuf* CTcpRxOffload::new_buf() {

    assert(m_head >= m_tail);
    if (m_head > m_tail) {
        uint32_t index = (m_head - 1) % m_size;
        if (!m_bufs[index].is_active()) {
            return &m_bufs[index];
        }
    }
    if (m_head - m_tail >= m_size) {
        if (m_bufs[m_tail % m_size].is_active()) {
            return nullptr;
        }
        ++m_tail;
    }

    uint32_t index = m_head % m_size;
    ++m_head;
    if (m_head > 2*m_size) {
        m_head -= m_size;
        m_tail -= m_size;
    }
    return &m_bufs[index];
}

HOT_FUNC CTcpRxOffloadBuf* CTcpRxOffload::find_buf(CTcpFlow* flow) {
    return flow->m_lro_buf;
}

HOT_FUNC bool CTcpRxOffload::append_buf(CTcpFlow* flow,
                                        struct rte_mbuf* mbuf,
                                        TCPHeader* lpTcp,
                                        CFlowKeyFullTuple& ftuple) {
    bool lro_done = false;
    bool do_lro = (lpTcp->getFlags() == TCPHeader::Flag::ACK) && (ftuple.m_l7_total_len > 0);
    CTcpRxOffloadBuf* buf = find_buf(flow);

    if (buf) {
        if (do_lro && buf->reassemble_mbuf(mbuf, lpTcp, ftuple)) {
            lro_done = true;
        } else {
            uint8_t flags = lpTcp->getFlags();
            if ((flags & TCPHeader::Flag::PSH) && (ftuple.m_l7_total_len > 0)) {
                if (buf->reassemble_mbuf(mbuf, lpTcp, ftuple)) {
                    buf->m_lpTcp->setFlag(flags);
                    lro_done = true;
                }
            }

            /* nullptr ctx prevents unexpected flow closing */
            m_cb(nullptr, flow, buf->m_mbuf, buf->m_lpTcp, buf->m_ftuple);
            buf->clear_flow();
        }
    }

    if (do_lro && !lro_done) {
        if (!buf) {
            buf = new_buf();
        }

        if (likely(buf)) {
            buf->set(flow, mbuf, lpTcp, ftuple);
            lro_done = true;
        }
    }

    return lro_done;
}

bool CTcpRxOffload::is_empty() {
    return (m_head == m_tail);
}

inline void CTcpRxOffload::flush_bufs(CTcpPerThreadCtx * ctx) {

    assert(m_head >= m_tail);

    for (; m_tail < m_head; ++m_tail) {
        CTcpRxOffloadBuf* buf = &m_bufs[m_tail % m_size];
        CTcpFlow* flow = buf->m_flow;

        if (flow) {
            buf->clear_flow(); /* to prevent mbuf double-free from flow closing. */
            m_cb(ctx, flow, buf->m_mbuf, buf->m_lpTcp, buf->m_ftuple);
        }
    }
}


HOT_FUNC bool CFlowTable::process_software_lro(CTcpPerThreadCtx * ctx,
                                    CTcpFlow *  flow,
                                    struct rte_mbuf * mbuf,
                                    TCPHeader    * lpTcp,
                                    CFlowKeyFullTuple &ftuple) {

    return m_tcp_lro && m_tcp_lro->append_buf(flow, mbuf, lpTcp, ftuple);
}

void CFlowTable::flush_software_lro(CTcpPerThreadCtx * ctx) {
    if (m_tcp_lro && !m_tcp_lro->is_empty()) {
        m_tcp_lro->flush_bufs(ctx);
    }
}

HOT_FUNC bool CFlowTable::rx_handle_packet(CTcpPerThreadCtx * ctx,
                                  struct rte_mbuf * mbuf,
                                  bool is_idle,
                                  tvpid_t port_id) {

    CFlowKeyTuple tuple;
    CFlowKeyFullTuple ftuple;

    #ifdef _DEBUG
    if ( m_verbose ){
        uint8_t *p1=rte_pktmbuf_mtod(mbuf, uint8_t*);
        uint16_t pkt_size1=rte_pktmbuf_pkt_len(mbuf);
        utl_k12_pkt_format(stdout,p1 ,pkt_size1) ;
    }
    #endif

    #ifdef FLOW_TABLE_DEBUG
    printf ("-- \n");
    printf (" client:%d process packet %d \n",m_client_side,pkt_cnt);
    pkt_cnt++;
    #endif

    CSimplePacketParser parser(mbuf);

    tcp_rx_pkt_action_t action;

    parse_packet(mbuf,
                parser,
                tuple,
                ftuple,
                ctx->get_rx_checksum_check(),
                action,
                port_id);

    if ( action != tREDIRECT_RX_CORE ) {
        TrexCaptureMngr::getInstance().handle_pkt_rx_dp(mbuf, port_id);
    }

    if ( action != tPROCESS ) {
        rx_handle_stateless_packet(ctx, mbuf, port_id);
        rx_non_process_packet(action, ctx, mbuf);
        return false;
    }

    if ( is_idle ) {
        rte_pktmbuf_free(mbuf);
        return false;
    }

    /* it is local mbuf, no need to atomic ref count */
    rte_mbuf_set_as_core_local(mbuf);


    flow_key_t key=tuple.get_flow_key();
    uint32_t  hash=tuple.get_hash();
   #ifdef FLOW_TABLE_DEBUG
    tuple.dump(stdout);
    printf ("-- \n");
   #endif


    flow_hash_ent_t * lpflow;
    lpflow = m_ft.find(key,hash);

    if (lpflow) {
        if ( tuple.get_proto() == IPHeader::Protocol::UDP ){
            CUdpFlow *flow;
            flow = CUdpFlow::cast_from_hash_obj(lpflow);
            UDPHeader    * lpUDP = (UDPHeader *)parser.m_l4;
            process_udp_packet(ctx,flow,mbuf,lpUDP,ftuple);
        }else{
            CTcpFlow *flow;
            flow = CTcpFlow::cast_from_hash_obj(lpflow);
            TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
#ifndef TREX_SIM
            if (process_software_lro(ctx, flow, mbuf, lpTcp, ftuple)) {
                /* mbuf is consumed by software LRO */
                return(true);
            }
#endif
            process_tcp_packet(ctx,flow,mbuf,lpTcp,ftuple);
        }
        return(true);
    }

    if ( tuple.get_proto() == IPHeader::Protocol::UDP ){
        return (rx_handle_packet_udp_no_flow(ctx,mbuf,lpflow,parser,tuple,ftuple,hash,port_id));
    }else{
        /* TCP */
        return (rx_handle_packet_tcp_no_flow(ctx,mbuf,lpflow,parser,tuple,ftuple,hash,port_id));
    }
}



