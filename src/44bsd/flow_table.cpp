#include "flow_table.h"
#include "tcp_var.h"
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


#include "flow_stat_parser.h"

void CSttFlowTableStats::Clear(){
    memset(&m_sts,0,sizeof(m_sts));

}

#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)

void CSttFlowTableStats::Dump(FILE *fd){
    MYC(m_err_no_syn);                  
    MYC(m_err_len_err);                 
    MYC(m_err_no_tcp);                  
    MYC(m_err_client_pkt_without_flow); 
    MYC(m_err_no_template);             
    MYC(m_err_no_memory);               
    MYC(m_err_duplicate_client_tuple);  
    MYC(m_err_l3_cs);                   
    MYC(m_err_l4_cs);                   
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
    m_prog =(CTcpAppProgram *)0;
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


bool CFlowTable::parse_packet(struct rte_mbuf * mbuf,
                              CSimplePacketParser & parser,
                              CFlowKeyTuple & tuple,
                              CFlowKeyFullTuple & ftuple,
                              bool rx_l4_check){

    if (!parser.Parse()){
        return(false);
    }
    uint16_t l3_pkt_len=0;

    /* TBD fix, should support UDP/IPv6 */

    if ( parser.m_protocol != IPHeader::Protocol::TCP ){
        FT_INC_SCNT(m_err_no_tcp);
        return(false);
    }
    /* it is TCP, only supported right now */

    uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);
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

        l3_pkt_len = ipv4->getTotalLength() + lpf->m_l3_offset;

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
        /* TBD need to find the last header here */

        l3_pkt_len = ipv6->getPayloadLen();
    }

    lpf->m_proto     =   parser.m_protocol;
    lpf->m_l4_offset = (uintptr_t)parser.m_l4 - (uintptr_t)p;

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    lpf->m_l7_offset = lpf->m_l4_offset + lpTcp->getHeaderLength();

    if (l3_pkt_len < lpf->m_l7_offset ) {
        FT_INC_SCNT(m_err_len_err);
        return(false);
    }
    lpf->m_l7_total_len  =  l3_pkt_len - lpf->m_l7_offset;

    if ( (mbuf->ol_flags & PKT_RX_IP_CKSUM_MASK) ==  PKT_RX_IP_CKSUM_BAD ){
        FT_INC_SCNT(m_err_l3_cs);
        return(false);
    }

    if ( rx_l4_check &&  ((mbuf->ol_flags & PKT_RX_L4_CKSUM_BAD) ==  PKT_RX_L4_CKSUM_BAD) ){
        FT_INC_SCNT(m_err_l4_cs);
        return(false);
    }

    tuple.set_proto(lpf->m_proto);
    tuple.set_ipv4(lpf->m_ipv4);
    return(true);
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
                                        TCPHeader    * lpTcp){
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
    

    CSimplePacketParser parser(mbuf);


    if ( !parse_packet(mbuf,
                       parser,
                       tuple,
                       ftuple,
                       ctx->get_rx_checksum_check() ) ){
        /* free memory */
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    flow_key_t key=tuple.get_as_uint64();
    uint32_t  hash=tuple.get_hash();

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

    /* server side */
    if (  (lpTcp->getFlags() & TCPHeader::Flag::SYN) ==0 ) {
        /* no syn */
        /* TBD need to generate RST packet in this case?? need to check what are the conditions in the old code ??? */
        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_syn);
        return(false);
    }

    /* server with SYN packet, it is OK 
    we need to build the flow and add it to the table */


    IPHeader *  ipv4 = (IPHeader *)parser.m_ipv4;
    uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t*);

    /* TBD Parser need to be fixed */
    uint16_t vlan=0;
    if (parser.m_vlan_offset) {
        VLANHeader * lpVlan=(VLANHeader *)(pkt+14);
        vlan = lpVlan->getVlanTag();
    }

    /* TBD template port, need to do somthing better  */
    if (lpTcp->getDestPort() != 80) {
        generate_rst_pkt(ctx,
                         ipv4->getDestIp(),
                         tuple.get_ip(),
                         lpTcp->getDestPort(),
                         tuple.get_port(),
                         vlan,
                         false,
                         lpTcp);

        rte_pktmbuf_free(mbuf);
        FT_INC_SCNT(m_err_no_template);
        return(false);
    }


    lptflow = ctx->m_ft.alloc_flow(ctx,
                                   ipv4->getDestIp(),
                                   tuple.get_ip(),
                                   lpTcp->getDestPort(),
                                   tuple.get_port(),
                                   vlan,
                                   false);


    if (lptflow == 0 ) {
        rte_pktmbuf_free(mbuf);
        return(false);
    }


    lptflow->server_update_mac_from_packet(pkt);

    /* add to flow-table */
    lptflow->m_hash.key = key;

    /* no need to check, we just checked */
    m_ft.insert_nc(&lptflow->m_hash,hash);

    CTcpApp * app =&lptflow->m_app;

    app->set_program(m_prog);
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



