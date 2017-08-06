/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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
#include <common/utl_gcc_diag.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <cmath>
#include "utl_mbuf.h"

#include "44bsd/tcp.h"
#include "44bsd/tcp_var.h"
#include "44bsd/tcp.h"
#include "44bsd/tcp_fsm.h"
#include "44bsd/tcp_seq.h"
#include "44bsd/tcp_timer.h"
#include "44bsd/tcp_socket.h"
#include "44bsd/tcpip.h"
#include "44bsd/tcp_dpdk.h"
#include "44bsd/flow_table.h"

#include "mbuf.h"
#include <stdlib.h>
#include <common/c_common.h>

static char http_req[] = {
0x47, 0x45, 0x54, 0x20, 0x2f, 0x32, 0x31, 0x30, 
0x30, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 
0x2e, 0x31, 0x0d, 0x0a, 0x48, 0x6f, 0x73, 0x74, 
0x3a, 0x20, 0x32, 0x32, 0x2e, 0x30, 0x2e, 0x30, 
0x2e, 0x33, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x6e, 
0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 
0x4b, 0x65, 0x65, 0x70, 0x2d, 0x41, 0x6c, 0x69, 
0x76, 0x65, 0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 
0x2d, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20, 
0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0x2f, 
0x34, 0x2e, 0x30, 0x20, 0x28, 0x63, 0x6f, 0x6d, 
0x70, 0x61, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x3b, 
0x20, 0x4d, 0x53, 0x49, 0x45, 0x20, 0x37, 0x2e, 
0x30, 0x3b, 0x20, 0x57, 0x69, 0x6e, 0x64, 0x6f, 
0x77, 0x73, 0x20, 0x4e, 0x54, 0x20, 0x35, 0x2e, 
0x31, 0x3b, 0x20, 0x53, 0x56, 0x31, 0x3b, 0x20, 
0x2e, 0x4e, 0x45, 0x54, 0x20, 0x43, 0x4c, 0x52, 
0x20, 0x31, 0x2e, 0x31, 0x2e, 0x34, 0x33, 0x32, 
0x32, 0x3b, 0x20, 0x2e, 0x4e, 0x45, 0x54, 0x20, 
0x43, 0x4c, 0x52, 0x20, 0x32, 0x2e, 0x30, 0x2e, 
0x35, 0x30, 0x37, 0x32, 0x37, 0x29, 0x0d, 0x0a, 
0x41, 0x63, 0x63, 0x65, 0x70, 0x74, 0x3a, 0x20, 
0x2a, 0x2f, 0x2a, 0x0d, 0x0a, 0x41, 0x63, 0x63, 
0x65, 0x70, 0x74, 0x2d, 0x4c, 0x61, 0x6e, 0x67, 
0x75, 0x61, 0x67, 0x65, 0x3a, 0x20, 0x65, 0x6e, 
0x2d, 0x75, 0x73, 0x0d, 0x0a, 0x41, 0x63, 0x63, 
0x65, 0x70, 0x74, 0x2d, 0x45, 0x6e, 0x63, 0x6f, 
0x64, 0x69, 0x6e, 0x67, 0x3a, 0x20, 0x67, 0x7a, 
0x69, 0x70, 0x2c, 0x20, 0x64, 0x65, 0x66, 0x6c, 
0x61, 0x74, 0x65, 0x2c, 0x20, 0x63, 0x6f, 0x6d, 
0x70, 0x72, 0x65, 0x73, 0x73, 0x0d, 0x0a, 0x0d, 
0x0a };

static char http_res[] = {
0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 
0x20, 0x32, 0x30, 0x30, 0x20, 0x4f, 0x4b, 0x0d, 
0x0a, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3a, 
0x20, 0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 
0x66, 0x74, 0x2d, 0x49, 0x49, 0x53, 0x2f, 0x36, 
0x2e, 0x30, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 
0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 
0x3a, 0x20, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 
0x74, 0x6d, 0x6c, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 
0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65, 0x6e, 
0x67, 0x74, 0x68, 0x3a, 0x20, 0x33, 0x32, 0x30, 
0x30, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x3c, 0x68, 
0x74, 0x6d, 0x6c, 0x3e, 0x3c, 0x70, 0x72, 0x65, 
0x3e, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a
};

static char http_res_post[] ={
0x3c, 0x2f, 0x70, 0x72, 0x65, 0x3e, 0x3c, 0x2f, 0x68, 0x74, 0x6d, 
0x6c, 0x3e
};

static char * allocate_http_res(uint32_t &new_size){
    uint16_t min_size=sizeof(http_res)+sizeof(http_res_post);
    if (new_size<min_size) {
        new_size=min_size;
    }
    char *p=(char *)malloc(new_size);
    char *s=p;
    uint32_t fill=new_size-min_size;
    memcpy(p,http_res,sizeof(http_res));
    p+=sizeof(http_res);
    if (fill) {
        memset(p,'*',fill);
    }
    p+=fill;
    memcpy(p,http_res_post,sizeof(http_res_post));
    return(s);
}

static void free_http_res(char *p){
    assert(p);
    free(p);
}


                                            
#undef DEBUG_TX_PACKET
                                          
class CTcpDpdkCb : public CTcpCtxCb {
public:
   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * tp,
             rte_mbuf_t *m);

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CTcpFlow * flow);

public:
    uint8_t                 m_dir;
    CFlowGenListPerThread * m_p;
};


int CTcpDpdkCb::on_flow_close(CTcpPerThreadCtx *ctx,
                              CTcpFlow * flow){
    uint32_t   c_idx;
    uint16_t   c_pool_idx;
    uint16_t   c_template_id;
    bool       enable;


    if (ctx->m_ft.is_client_side() == false) {
        /* nothing to do, flow ports was allocated by client */
        return(0);
    }
    m_p->m_stats.m_total_close_flows +=1;

    flow->get_tuple_generator(c_idx,c_pool_idx,c_template_id,enable);
    assert(enable==true); /* all flows should have tuple generator */

    CFlowGeneratorRecPerThread * cur = m_p->m_cap_gen[c_template_id];
    CTupleGeneratorSmart * lpgen=cur->tuple_gen.get_gen();
    if ( lpgen->IsFreePortRequired(c_pool_idx) ){
        lpgen->FreePort(c_pool_idx,c_idx,flow->m_tcp.src_port);
    }
    return(0);
}


int CTcpDpdkCb::on_tx(CTcpPerThreadCtx *ctx,
                      struct tcpcb * tp,
                      rte_mbuf_t *m){
    CNodeTcp node_tcp;
    node_tcp.dir  = m_dir;
    node_tcp.mbuf = m;

#ifdef DEBUG_TX_PACKET
     //fprintf(stdout,"TX---> dir %d \n",m_dir);
     //utl_rte_pktmbuf_dump_k12(stdout,m);
#endif
    
    m_p->m_node_gen.m_v_if->send_node((CGenNode *) &node_tcp);
    return(0);
}


void CFlowGenListPerThread::tcp_handle_rx_flush(CGenNode * node,
                                                bool on_terminate){

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += TCP_RX_FLUSH_SEC;
        m_node_gen.m_p_queue.push(node);
    }else{
        free_node(node);
    }

    CVirtualIF * v_if=m_node_gen.m_v_if;
    rte_mbuf_t * rx_pkts[64];
    int dir;
    uint16_t cnt;

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };
    
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
        while (true) {
            cnt=v_if->rx_burst(dir,rx_pkts,64);
            if (cnt==0) {
                break;
            }
            int i;
            for (i=0; i<(int)cnt;i++) {
                rte_mbuf_t * m=rx_pkts[i];

#ifdef DEBUG_TX_PACKET
                fprintf(stdout,"RX---> dir %d \n",dir);
                utl_rte_pktmbuf_dump_k12(stdout,m);
#endif
                ctx->m_ft.rx_handle_packet(ctx,m);
            }
        }
    }
}

static CTcpAppApiImpl     m_tcp_bh_api_impl_c;

#ifndef TREX_SIM
uint16_t get_client_side_vlan(CVirtualIF * _ifs);
#endif

void CFlowGenListPerThread::tcp_generate_flows_roundrobin(bool &done){

    done=false;
    /* TBD need to scan the vector of template .. for now we have one template */

    CFlowGeneratorRecPerThread * cur;
    uint8_t template_id=0; /* TBD take template zero */

    cur=m_cap_gen[template_id]; /* first template TBD need to fix this */


    CTupleBase  tuple;
    cur->tuple_gen.GenerateTuple(tuple);
    /* it is not set by generator, need to take it from the pcap file */
    tuple.setServerPort(80) ;

    /* TBD this include an information on the client/vlan/destination */ 
    /*ClientCfgBase * lpc=tuple.getClientCfg(); */

    
    uint16_t vlan=0;

    if ( unlikely(CGlobalInfo::m_options.preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_NORMAL) ) {
 /* TBD need to fix , should be taken from right port */
#ifndef TREX_SIM
        vlan= get_client_side_vlan(m_node_gen.m_v_if);
#endif
    }

    /* TBD set the tuple */
    CTcpFlow * c_flow = m_c_tcp->m_ft.alloc_flow(m_c_tcp,
                                                 tuple.getClient(),
                                                 tuple.getServer(),
                                                 tuple.getClientPort(),
                                                 tuple.getServerPort(),
                                                 vlan,
                                                 false);
    if (c_flow == (CTcpFlow *)0) {
        return;
    }

    /* save tuple generator information into the flow */
    c_flow->set_tuple_generator(tuple.getClientId(), 
                                cur->m_info->m_client_pool_idx,
                                template_id,
                                true);

    /* update default mac addrees, dir is zero client side  */
    m_node_gen.m_v_if->update_mac_addr_from_global_cfg(CLIENT_SIDE,c_flow->m_tcp.template_pkt);

    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(tuple.getClient());
    c_tuple.set_port(tuple.getClientPort());
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

    if (!m_c_tcp->m_ft.insert_new_flow(c_flow,c_tuple)){
        /* need to free the tuple */
        m_c_tcp->m_ft.handle_close(m_c_tcp,c_flow,false);
        return;
    }

    CTcpApp * app_c;

    app_c = &c_flow->m_app;

    app_c->set_program(m_prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(m_c_tcp,c_flow);
    c_flow->set_app(app_c);

    /* start connect */
    app_c->start(true);
    tcp_connect(m_c_tcp,&c_flow->m_tcp);

    m_stats.m_total_open_flows += 1;
}

void CFlowGenListPerThread::tcp_handle_tx_fif(CGenNode * node,
                                              bool on_terminate){
    bool done;
    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ) {
        m_cur_time_sec = node->m_time ;

        tcp_generate_flows_roundrobin(done);

        if (!done) {
            node->m_time += m_tcp_fif_d_time;
            m_node_gen.m_p_queue.push(node);
        }else{
            free_node(node);
        }
    }else{
        free_node(node);
    }



}

void CFlowGenListPerThread::tcp_handle_tw(CGenNode * node,
                                          bool on_terminate){

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += tcp_get_tw_tick_in_sec();
        m_node_gen.m_p_queue.push(node);
    }else{
        free_node(node);
    }

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    int dir;
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
        ctx->timer_w_on_tick();
    }
}


double CFlowGenListPerThread::tcp_get_tw_tick_in_sec(){
    return((double)TCP_TIMER_W_TICK/((double)TCP_TIMER_W_DIV* 1000.0));
}


bool CFlowGenListPerThread::Create_tcp(){

    m_c_tcp = new CTcpPerThreadCtx();
    m_s_tcp = new CTcpPerThreadCtx();

    uint8_t mem_socket_id=get_memory_socket_id();

    m_c_tcp->set_memory_socket(mem_socket_id);
    m_s_tcp->set_memory_socket(mem_socket_id);
    

    CTcpDpdkCb * c_tcp_io = new CTcpDpdkCb();
    CTcpDpdkCb * s_tcp_io = new CTcpDpdkCb();

    c_tcp_io->m_dir =0;
    c_tcp_io->m_p   = this;
    s_tcp_io->m_dir =1;
    s_tcp_io->m_p   = this;

    m_c_tcp_io =c_tcp_io;
    m_s_tcp_io =s_tcp_io;

    m_c_tcp->Create(10000,true);
    m_c_tcp->set_cb(m_c_tcp_io);
    
    m_s_tcp->Create(10000,false);
    m_s_tcp->set_cb(m_s_tcp_io);

    uint32_t http_r_size= CGlobalInfo::m_options.m_tcp_http_res;

    m_req = new CMbufBuffer();
    m_res = new CMbufBuffer();

    m_prog_c = new CTcpAppProgram();
    m_prog_s = new CTcpAppProgram();

    uint8_t* http_r=(uint8_t*)allocate_http_res(http_r_size);

    utl_mbuf_buffer_create_and_copy(mem_socket_id,m_req,2048,(uint8_t*)http_req,sizeof(http_req));
    utl_mbuf_buffer_create_and_copy(mem_socket_id,m_res,2048,(uint8_t*)http_r,http_r_size);

    free_http_res((char *)http_r);


    CTcpAppCmd cmd;


    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =m_req;
    m_prog_c->add_cmd(cmd);

    cmd.m_cmd =tcRX_BUFFER ;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm  =http_r_size;
    m_prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = sizeof(http_req);
    m_prog_s->add_cmd(cmd);

    cmd.m_cmd = tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =m_res;
    m_prog_s->add_cmd(cmd);



    m_s_tcp->m_ft.set_tcp_api(&m_tcp_bh_api_impl_c);
    m_s_tcp->m_ft.set_tcp_program(m_prog_s);


    return(true);
}

void CFlowGenListPerThread::Delete_tcp(){

    delete m_prog_c;
    delete m_prog_s;

    m_req->Delete();
    delete m_req;

    m_res->Delete();
    delete m_res;

    m_c_tcp->Delete();
    m_s_tcp->Delete();

    delete m_c_tcp;
    delete m_s_tcp;

    CTcpDpdkCb * c_tcp_io = (CTcpDpdkCb *)m_c_tcp_io;
    CTcpDpdkCb * s_tcp_io = (CTcpDpdkCb *)m_s_tcp_io;
    delete c_tcp_io;
    delete s_tcp_io;
}

