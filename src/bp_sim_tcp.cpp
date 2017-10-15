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
#include <astf/json_reader.h>
#include <astf/astf_template_db.h>

#undef DEBUG_TX_PACKET

class CTcpIOCb : public CTcpCtxCb {
public:
   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * tp,
             rte_mbuf_t *m);

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CTcpFlow * flow);

   int on_redirect_rx(CTcpPerThreadCtx *ctx,
                      rte_mbuf_t *m);


public:
    uint8_t                 m_dir;
    CFlowGenListPerThread * m_p;
};


int CTcpIOCb::on_flow_close(CTcpPerThreadCtx *ctx,
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

    CAstfPerTemplateRW * cur = ctx->m_template_rw->get_template_by_id(c_template_id);
    CTupleGeneratorSmart * lpgen= cur->m_tuple_gen.get_gen();

    if ( lpgen->IsFreePortRequired(c_pool_idx) ){
        lpgen->FreePort(c_pool_idx,c_idx,flow->m_tcp.src_port);
    }
    return(0);
}


int CTcpIOCb::on_redirect_rx(CTcpPerThreadCtx *ctx,
                               rte_mbuf_t *m){
    pkt_dir_t   dir = ctx->m_ft.is_client_side()?CLIENT_SIDE:SERVER_SIDE;
    return(m_p->m_node_gen.m_v_if->redirect_to_rx_core(dir,m)?0:-1);
}

int CTcpIOCb::on_tx(CTcpPerThreadCtx *ctx,
                      struct tcpcb * tp,
                      rte_mbuf_t *m){
    CNodeTcp node_tcp;
    node_tcp.dir  = m_dir;
    node_tcp.mbuf = m;
#ifdef TREX_SIM
    node_tcp.sim_time =m_p->m_cur_time_sec;
#endif


#ifdef _DEBUG
    if ( CGlobalInfo::m_options.preview.getVMode() == 6){
        fprintf(stdout,"TX---> dir %d \n",m_dir);
        utl_rte_pktmbuf_dump_k12(stdout,m);
    }
#endif



    m_p->m_node_gen.m_v_if->send_node((CGenNode *) &node_tcp);
    return(0);
}


void CFlowGenListPerThread::tcp_handle_rx_flush(CGenNode * node,
                                                bool on_terminate){

#ifdef TREX_SIM
    m_cur_time_sec =node->m_time;
    m_node_gen.m_v_if->set_rx_burst_time(m_cur_time_sec);
#endif

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += TCP_RX_FLUSH_SEC;
        m_node_gen.m_p_queue.push(node);
    }else{
        if (m_tcp_terminate){
            free_node(node);
        }else{
            node->m_time += TCP_RX_FLUSH_SEC;
            m_node_gen.m_p_queue.push(node);
        }
    }

    CVirtualIF * v_if=m_node_gen.m_v_if;
    rte_mbuf_t * rx_pkts[64];
    int dir;
    uint16_t cnt;

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    int sum;
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
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
                    fprintf(stdout,"RX---> dir %d \n",dir);
                    utl_rte_pktmbuf_dump_k12(stdout,m);
                }
#endif
                ctx->m_ft.rx_handle_packet(ctx,m);
            }
            sum+=cnt;
            if (sum>127) {
                ctx->m_ft.inc_rx_throttled_cnt();
                break;
            }
        }
    }
}

static CTcpAppApiImpl     m_tcp_bh_api_impl_c;

#ifndef TREX_SIM
uint16_t get_client_side_vlan(CVirtualIF * _ifs);
#endif

void CFlowGenListPerThread::tcp_generate_flow(bool &done){

    done=false;

    CAstfTemplatesRW * c_rw = m_c_tcp->m_template_rw;

    /* choose template index */
    uint16_t template_id = c_rw->do_schedule_template();

    CAstfPerTemplateRW * cur = c_rw->get_template_by_id(template_id);
    CTcpData    *   cur_tmp_ro = m_c_tcp->m_template_ro;


    CTupleBase  tuple;
    cur->m_tuple_gen.GenerateTuple(tuple);

    /* it is not set by generator, need to take it from the pcap file */
    tuple.setServerPort(cur_tmp_ro->get_dport(template_id));

    /* TBD this include an information on the client/vlan/destination */ 
    /*ClientCfgBase * lpc=tuple.getClientCfg(); */

    
    uint16_t vlan=0;

    if ( unlikely(CGlobalInfo::m_options.preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_NORMAL) ) {
 /* TBD need to fix , should be taken from right port */
#ifndef TREX_SIM
        vlan= get_client_side_vlan(m_node_gen.m_v_if);
#endif
    }

    bool is_ipv6 = CGlobalInfo::is_ipv6_enable();

    /* TBD set the tuple */
    CTcpFlow * c_flow = m_c_tcp->m_ft.alloc_flow(m_c_tcp,
                                                 tuple.getClient(),
                                                 tuple.getServer(),
                                                 tuple.getClientPort(),
                                                 tuple.getServerPort(),
                                                 vlan,
                                                 is_ipv6);
    if (c_flow == (CTcpFlow *)0) {
        return;
    }

    /* save tuple generator information into the flow */
    c_flow->set_tuple_generator(tuple.getClientId(),
                                cur->m_client_pool_idx, 
                                template_id,
                                true);

    /* update default mac addrees, dir is zero client side  */
    m_node_gen.m_v_if->update_mac_addr_from_global_cfg(CLIENT_SIDE,c_flow->m_tcp.template_pkt);

    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(tuple.getClient());
    c_tuple.set_port(tuple.getClientPort());
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(is_ipv6?false:true);

    if (!m_c_tcp->m_ft.insert_new_flow(c_flow,c_tuple)){
        /* need to free the tuple */
        m_c_tcp->m_ft.handle_close(m_c_tcp,c_flow,false);
        return;
    }

    CTcpApp * app_c;

    app_c = &c_flow->m_app;

    app_c->set_program(cur_tmp_ro->get_client_prog(template_id));
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
    #ifdef TREX_SIM
    m_cur_time_sec =node->m_time;
    #endif

    bool done;
    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ) {
        m_cur_time_sec = node->m_time ;

        tcp_generate_flow(done);

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
    #ifdef TREX_SIM
    m_cur_time_sec = node->m_time;
    #endif

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += tcp_get_tw_tick_in_sec();
        m_node_gen.m_p_queue.push(node);
    }

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    int dir;
    bool any_event=false;
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
        ctx->timer_w_on_tick();
        if(ctx->timer_w_any_events()){
            any_event=true;
        } 
    }

    if ( on_terminate == true ){
        if (any_event){
            node->m_time += tcp_get_tw_tick_in_sec();
            m_node_gen.m_p_queue.push(node);
        }else{
            free_node(node);
            m_tcp_terminate=true;
        }
    }
    
}


double CFlowGenListPerThread::tcp_get_tw_tick_in_sec(){
    return(TCP_TIME_TICK_SEC);
}


bool CFlowGenListPerThread::Create_tcp(){
    m_tcp_terminate = false;
    m_c_tcp = new CTcpPerThreadCtx();
    m_s_tcp = new CTcpPerThreadCtx();

    uint8_t mem_socket_id=get_memory_socket_id();
    CTcpData *template_db = CJsonData::instance()->get_tcp_data_handle(mem_socket_id);
    CTcpIOCb * c_tcp_io = new CTcpIOCb();
    CTcpIOCb * s_tcp_io = new CTcpIOCb();

    m_tcp_fif_d_time = template_db->get_delta_tick_sec_thread(m_max_threads);

    #if 0
    printf("socket id:%d pointer:%p\n", mem_socket_id, template_db); //??? remove
    template_db->dump(stdout);
    #endif

    c_tcp_io->m_dir =0;
    c_tcp_io->m_p   = this;
    s_tcp_io->m_dir =1;
    s_tcp_io->m_p   = this;

    m_c_tcp_io =c_tcp_io;
    m_s_tcp_io =s_tcp_io;

    uint32_t active_flows = get_max_active_flows_per_core_tcp()/2 ;
    if (active_flows<100000) {
        active_flows=100000;
    }
    m_c_tcp->Create(active_flows,true);
    m_c_tcp->set_cb(m_c_tcp_io);

    m_c_tcp->set_template_ro(template_db);
    CAstfTemplatesRW * rw= CJsonData::instance()->get_tcp_data_handle_rw(mem_socket_id, &m_smart_gen
                                                                           , m_thread_id, m_max_threads
                                                                           , getDualPortId());
    m_c_tcp->set_template_rw(rw);
 
    m_s_tcp->Create(active_flows,false);
    m_s_tcp->set_cb(m_s_tcp_io);
    m_s_tcp->set_template_ro(template_db);

    m_c_tcp->set_memory_socket(mem_socket_id);
    m_s_tcp->set_memory_socket(mem_socket_id);

    /* set dev flags */
    CPreviewMode * lp=&CGlobalInfo::m_options.preview;

    uint8_t dev_offload_flags=0;
    if (lp->getChecksumOffloadEnable()) {
        dev_offload_flags |= (TCP_OFFLOAD_RX_CHKSUM | TCP_OFFLOAD_TX_CHKSUM);
    }
    if (lp->get_dev_tso_support()) {
        dev_offload_flags |= TCP_OFFLOAD_TSO;
    }

    m_c_tcp->set_offload_dev_flags(dev_offload_flags);
    m_s_tcp->set_offload_dev_flags(dev_offload_flags);


    if ( m_preview_mode.getVMode() >2 ){
        m_c_tcp->m_ft.set_debug(true);
        m_s_tcp->m_ft.set_debug(true);
    }

    m_s_tcp->m_ft.set_tcp_api(&m_tcp_bh_api_impl_c);

    return(true);
}

void CFlowGenListPerThread::Delete_tcp(){
    if (m_c_tcp) {
        m_c_tcp->Delete();
        delete m_c_tcp;
        m_c_tcp=0;
    }
    if (m_s_tcp) {
        m_s_tcp->Delete();
        delete m_s_tcp;
        m_s_tcp=0;
    }

    CTcpIOCb * c_tcp_io = (CTcpIOCb *)m_c_tcp_io;
    if (c_tcp_io) {
        delete c_tcp_io;
        m_c_tcp_io=0;
    }
    CTcpIOCb * s_tcp_io = (CTcpIOCb *)m_s_tcp_io;
    if (s_tcp_io) {
        delete s_tcp_io;
        m_s_tcp_io=0;
    }
}

