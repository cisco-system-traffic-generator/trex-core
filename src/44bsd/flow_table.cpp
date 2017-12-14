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

void CSttFlowTableStats::Clear(){
    memset(&m_sts,0,sizeof(m_sts));

}

#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)

void CSttFlowTableStats::Dump(FILE *fd){
    MYC(m_err_no_syn);                  
    MYC(m_err_len_err);                 
    MYC(m_err_fragments_ipv4_drop);
    MYC(m_err_no_tcp);                  
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
}


void CFlowKeyTuple::dump(FILE *fd){
    fprintf(fd,"m_ip       : %lu \n",(ulong)get_ip());
    fprintf(fd,"m_port     : %lu \n",(ulong)get_port());
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
    m_tcp_api=(CTcpAppApi    *)0;
    return(true);
}

void CFlowTable::Delete(){
    m_ft.Delete();
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
                              tcp_rx_pkt_action_t & action){
    action=tDROP;

    if (!parser.Parse()){
        action=tREDIRECT_RX_CORE;
        return;
    }
    uint32_t pkt_len=0;

    /* TBD fix, should support UDP/IPv6 */


    if ( parser.m_protocol != IPHeader::Protocol::TCP ){
        if (parser.m_protocol != IPHeader::Protocol::UDP) {
            action=tREDIRECT_RX_CORE;
        }else{
            FT_INC_SCNT(m_err_no_tcp);
        }
        return;
    }
    /* it is TCP, only supported right now */

    uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);
    uint32_t mbuf_pkt_len=rte_pktmbuf_pkt_len(mbuf);
    /* check packet length and checksum in case of TCP */

    CFlowKeyFullTuple *lpf= &ftuple;

    if (parser.m_ipv4) {
        /* IPv4 */
        lpf->m_ipv4      =true;
        lpf->m_l3_offset = (uintptr_t)parser.m_ipv4 - (uintptr_t)p;
        IPHeader *   ipv4= parser.m_ipv4;
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        if ( m_client_side ) {
            tuple.set_ip(ipv4->getDestIp());
            tuple.set_port(lpTcp->getDestPort());
        }else{
            tuple.set_ip(ipv4->getSourceIp());
            tuple.set_port(lpTcp->getSourcePort());
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
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

        
        if ( m_client_side ) {
            tuple.set_ip(ipv6->getDestIpv6LSB());
            tuple.set_port(lpTcp->getDestPort());
        }else{
            tuple.set_ip(ipv6->getSourceIpv6LSB());
            tuple.set_port(lpTcp->getSourcePort());
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

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    uint8_t tcp_header_len = lpTcp->getHeaderLength();

    if ( tcp_header_len < TCP_HEADER_LEN ) {
        FT_INC_SCNT(m_err_len_err);
        return;
    }

    lpf->m_l7_offset = lpf->m_l4_offset + tcp_header_len;

    /* check TCP header size */
    if (pkt_len < lpf->m_l7_offset ) {
        FT_INC_SCNT(m_err_len_err);
        return;
    }
    lpf->m_l7_total_len  =  pkt_len - lpf->m_l7_offset;

    if ( (mbuf->ol_flags & PKT_RX_IP_CKSUM_MASK) ==  PKT_RX_IP_CKSUM_BAD ){
        FT_INC_SCNT(m_err_l3_cs);
        return;
    }

    if ( rx_l4_check &&  ((mbuf->ol_flags & PKT_RX_L4_CKSUM_BAD) ==  PKT_RX_L4_CKSUM_BAD) ){
        FT_INC_SCNT(m_err_l4_cs);
        return;
    }

    action=tPROCESS;
    tuple.set_proto(lpf->m_proto);
    tuple.set_ipv4(lpf->m_ipv4);
}



void CFlowTable::handle_close(CTcpPerThreadCtx * ctx,
                              CTcpFlow * flow,
                              bool remove_from_ft){
    ctx->m_cb->on_flow_close(ctx,flow);
    if ( remove_from_ft ){
        m_ft.remove(&flow->m_hash);
    }
    free_flow(flow);
}

void CFlowTable::process_tcp_packet(CTcpPerThreadCtx * ctx,
                                    CTcpFlow *  flow,
                                    struct rte_mbuf * mbuf,
                                    TCPHeader    * lpTcp,
                                    CFlowKeyFullTuple &ftuple){

    tcp_flow_input(ctx,
                   &flow->m_tcp,
                   mbuf,
                   lpTcp,
                   ftuple.m_l7_offset,
                   ftuple.m_l7_total_len
                   );

    if (flow->is_can_close()) {
        handle_close(ctx,flow,true);
    }
}

void       CFlowTable::generate_rst_pkt(CTcpPerThreadCtx * ctx,
                                         uint32_t src,
                                         uint32_t dst,
                                         uint16_t src_port,
                                         uint16_t dst_port,
                                         uint16_t vlan,
                                         bool is_ipv6,
                                        TCPHeader    * lpTcp,
                                        uint8_t *   pkt,
                                        IPv6Header *    ipv6){
   /* TBD could be done much faster, but this is a corner case and there is no need to improve this 
      allocate flow, 
      fill information
      generate RST
      free flow 
   */
    CTcpFlow * flow=alloc_flow(ctx,
                                 src,
                                 dst,
                                 src_port,
                                 dst_port,
                                 vlan,
                                 is_ipv6);
    if (flow==0) {
        return;
    }

    flow->server_update_mac_from_packet(pkt);
    if (is_ipv6) {
        /* learn the ipv6 headers */
        flow->learn_ipv6_headers_from_network(ipv6);
    }

    tcp_respond(ctx,
                 &flow->m_tcp,
                 lpTcp->getSeqNumber()+1, 
                 0, 
                 TH_RST|TH_ACK);

    free_flow(flow);
}

CTcpFlow * CFlowTable::alloc_flow(CTcpPerThreadCtx * ctx,
                                  uint32_t src,
                                  uint32_t dst,
                                  uint16_t src_port,
                                  uint16_t dst_port,
                                  uint16_t vlan,
                                  bool is_ipv6){
    CTcpFlow * flow = new (std::nothrow) CTcpFlow();
    if (flow == 0 ) {
        FT_INC_SCNT(m_err_no_memory);
        return((CTcpFlow *)0);
    }
    flow->Create(ctx);
    flow->set_tuple(src,dst,src_port,dst_port,vlan,is_ipv6);
    flow->init();
    return(flow);
}

void       CFlowTable::free_flow(CTcpFlow * flow){
    assert(flow);
    flow->Delete();
    delete flow;

}


bool CFlowTable::insert_new_flow(CTcpFlow *  flow,
                                 CFlowKeyTuple  & tuple){

    flow_key_t key=tuple.get_as_uint64();
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

bool CFlowTable::rx_handle_packet(CTcpPerThreadCtx * ctx,
                                  struct rte_mbuf * mbuf){

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
                action);

    if (action !=tPROCESS ){
        rx_non_process_packet(action,ctx,mbuf);
        return(false);
    }

    /* it is local mbuf, no need to atomic ref count */
    rte_mbuf_set_as_core_local(mbuf);


    flow_key_t key=tuple.get_as_uint64();
    uint32_t  hash=tuple.get_hash();
   #ifdef FLOW_TABLE_DEBUG
    tuple.dump(stdout);
    printf ("-- \n");
   #endif


    flow_hash_ent_t * lpflow;
    lpflow = m_ft.find(key,hash);
    CTcpFlow * lptflow;

    if ( lpflow ) {
        lptflow = CTcpFlow::cast_from_hash_obj(lpflow);
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        process_tcp_packet(ctx,lptflow,mbuf,lpTcp,ftuple);
        return (true);;
    }

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    /* not found in flowtable , we are generating the flows*/
    if ( m_client_side ){
        FT_INC_SCNT(m_err_client_pkt_without_flow);
        rte_pktmbuf_free(mbuf);
        return(false);
    }

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
    uint16_t vlan=0;
    if (parser.m_vlan_offset) {
        VLANHeader * lpVlan=(VLANHeader *)(pkt+14);
        vlan = lpVlan->getVlanTag();
    }

    uint16_t dst_port = lpTcp->getDestPort();

    /* server side */
    if (  (lpTcp->getFlags() & TCPHeader::Flag::SYN) ==0 ) {
        /* no syn */
        generate_rst_pkt(ctx,
                         dest_ip,
                         tuple.get_ip(),
                         dst_port,
                         tuple.get_port(),
                         vlan,
                         is_ipv6,
                         lpTcp,
                         pkt,parser.m_ipv6);

        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_syn);
        return(false);
    }

    CAstfDbRO *tcp_data_ro = ctx->get_template_ro();
    CTcpServreInfo *server_info = tcp_data_ro->get_server_info_by_port(dst_port);

    if (! server_info) {
        generate_rst_pkt(ctx,
                         dest_ip,
                         tuple.get_ip(),
                         dst_port,
                         tuple.get_port(),
                         vlan,
                         is_ipv6,
                         lpTcp,
                         pkt,parser.m_ipv6);

        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }

    if ( ctx->is_open_flow_enabled()==false ){
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_s_nf_throttled);
        return(false);
    }

    CTcpAppProgram *server_prog = server_info->get_prog();
    CTcpTuneables *s_tune = server_info->get_tuneables();

    lptflow = ctx->m_ft.alloc_flow(ctx,
                                   dest_ip,
                                   tuple.get_ip(),
                                   dst_port,
                                   tuple.get_port(),
                                   vlan,
                                   is_ipv6);



    if (lptflow == 0 ) {
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    lptflow->set_s_tcp_info(tcp_data_ro, s_tune);
    lptflow->server_update_mac_from_packet(pkt);
    if (is_ipv6) {
        /* learn the ipv6 headers */
        lptflow->learn_ipv6_headers_from_network(parser.m_ipv6);
    }

    lptflow->m_c_template_idx = server_info->get_temp_idx();

    /* add to flow-table */
    lptflow->m_hash.key = key;

    /* no need to check, we just checked */
    m_ft.insert_nc(&lptflow->m_hash,hash);

    CTcpApp * app =&lptflow->m_app;

    app->set_program(server_prog);
    app->set_bh_api(m_tcp_api);
    app->set_flow_ctx(ctx,lptflow);

    lptflow->set_app(app);

    app->start(true); /* start the application */
    /* start listen */
    tcp_listen(ctx,&lptflow->m_tcp);

    /* process SYN packet */
    process_tcp_packet(ctx,lptflow,mbuf,lpTcp,ftuple);

    return(true);
}



