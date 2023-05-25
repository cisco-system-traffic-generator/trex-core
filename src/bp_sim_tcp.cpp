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
#include <astf/astf_db.h>
#include <astf/astf_template_db.h>

#undef DEBUG_TX_PACKET

class CTcpIOCb : public CTcpCtxCb {
public:
   int on_tx(CTcpPerThreadCtx *ctx,
             struct CTcpCb * tp,
             rte_mbuf_t *m);

   void on_flush_tx();

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CFlowBase * flow);

   int on_redirect_rx(CTcpPerThreadCtx *ctx,
                      rte_mbuf_t *m);


public:
    uint8_t                 m_dir;
    CFlowGenListPerThread * m_p;
};


int CTcpIOCb::on_flow_close(CTcpPerThreadCtx *ctx,
                            CFlowBase * flow){
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

    CAstfPerTemplateRW * cur = flow->m_pctx->m_template_rw->get_template_by_id(c_template_id);

    cur->try_inc_limit(); /* allow new flow when flows are limited */

    CTupleGeneratorSmart * lpgen= cur->m_tuple_gen.get_gen();
    if ( lpgen->IsFreePortRequired(c_pool_idx) ){
        lpgen->FreePort(c_pool_idx,c_idx,flow->m_template.m_src_port);
    }

    flow->resume_base_flow();
    flow->clear_exec_flow();
    return(0);
}


int CTcpIOCb::on_redirect_rx(CTcpPerThreadCtx *ctx,
                               rte_mbuf_t *m){
    pkt_dir_t   dir = ctx->m_ft.is_client_side()?CLIENT_SIDE:SERVER_SIDE;
    return(m_p->m_node_gen.m_v_if->redirect_to_rx_core(dir,m)?0:-1);
}

void CTcpIOCb::on_flush_tx(){
    m_p->m_node_gen.m_v_if->flush_tx_queue();
}

int CTcpIOCb::on_tx(CTcpPerThreadCtx *ctx,
                      struct CTcpCb * tp,
                      rte_mbuf_t *m){
    CNodeTcp node_tcp;
    node_tcp.m_type = 0xFF;
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


void CFlowGenListPerThread::handle_rx_flush(CGenNode * node,
                                                bool on_terminate){

#ifdef TREX_SIM
    m_cur_time_sec =node->m_time;
    m_node_gen.m_v_if->set_rx_burst_time(m_cur_time_sec);
#endif
    double dtime=m_sched_accurate?TCP_RX_FLUSH_ACCURATE_SEC:TCP_RX_FLUSH_SEC;
    int drop=0;
    m_node_gen.m_p_queue.pop();
    if ( on_terminate ){
        if (m_tcp_terminate){
            if (m_tcp_terminate_cnt>(uint32_t)(2.0/dtime)) {
                drop=1;
                m_tcp_terminate=false;
            }else{
                m_tcp_terminate_cnt++;
            }
        }
    }
    if (drop) {
        free_node(node);
    }else{
        node->m_time += dtime;
        m_node_gen.m_p_queue.push(node);
    }
    handle_rx_pkts(false);
}

uint16_t CFlowGenListPerThread::handle_rx_pkts(bool is_idle) {
    if (CGlobalInfo::m_options.m_astf_best_effort_mode) {
        return handle_rx_pkts<true>(is_idle);
    } else {
        return handle_rx_pkts<false>(is_idle);
    }
}

/**
 * rx_burst_astf_software_rss_ring attempts to read *nb_pkts* packets into the *rx_pkts* array.
 * The ring from which to read, the redirected packets ring or the physical ring is specified in the boolean
 * parameter.
 *
 * @param dir
 *   The direction of the traffic.
 * @param rx_pkts
 *   The address of an array of pointers to *rte_mbuf* structures that
 *   must be large enough to store *nb_pkts* pointers in it.
 * @param nb_pkts
 *   The maximum number of packets to retrieve.
 * @param redirect_ring
 *   Read packets from the redirect ring if this is true, else read them from the physical ring.
 * @return
 *   The number of packets actually read. It returns the smaller between *nb_pkts* and the ring size.
*/
uint16_t CFlowGenListPerThread::rx_burst_astf_software_rss_ring(pkt_dir_t dir, rte_mbuf_t** rx_pkts, uint16_t nb_pkts, bool redirect_ring) {
    if (redirect_ring) {
        MbufRedirectCache* mbuf_redirect_cache = m_dp_core->get_mbuf_redirect_cache();
        return mbuf_redirect_cache->retrieve_redirected_packets(rx_pkts, nb_pkts);
    } else {
        CVirtualIF* v_if = m_node_gen.m_v_if;
        return v_if->rx_burst(dir, rx_pkts, nb_pkts);
    }
}

/**
 * rx_burst_astf_software_rss attempts to read *nb_pkts* packets into the *rx_pkts* array.
 * Since the mode is ASTF software RSS, the packets need to be read from 2 rings. The first one being,
 * the classical physical interface, and the second one being the redirect ring between the DPs.
 * This is done using the Round Robin (RR) algorithm.
 *
 * @param dir
 *   The direction of the traffic.
 * @param rx_pkts
 *   The address of an array of pointers to *rte_mbuf* structures that
 *   must be large enough to store *nb_pkts* pointers in it.
 * @param nb_pkts
 *   The maximum number of packets to retrieve.
 * @return
 *   The number of packets actually read. It returns the smaller between *nb_pkts* to the ring size.
*/
uint16_t CFlowGenListPerThread::rx_burst_astf_software_rss(pkt_dir_t dir, rte_mbuf_t** rx_pkts, uint16_t nb_pkts) {
    if (dir == SERVER_SIDE) {
        // Nothing to do on server
        return rx_burst_astf_software_rss_ring(dir, rx_pkts, nb_pkts, false);
    }
    uint16_t cnt = rx_burst_astf_software_rss_ring(dir, rx_pkts, nb_pkts, m_read_from_redirect_ring);
    if (cnt == 0) {
        // The ring that we tried to read from was empty, let's try the other one.
        cnt = rx_burst_astf_software_rss_ring(dir, rx_pkts, nb_pkts, !m_read_from_redirect_ring);
        // Read from the not intended ring, don't change the boolean flag indicating which ring to read from next time.
    } else {
        // Round Robin, next time read from the other ring.
        m_read_from_redirect_ring = !m_read_from_redirect_ring;
    }
    return cnt;
}

/**
 * handle_astf_software_rss handles a packet (mbuf) when we are running in software mode
 * with multicore. Since the packets can arrive to cores that don't have the relevant flow table,
 * they need to be redirected to the correct core. This is achieved by aligning the
 * source port to the correct core upon generation, hence using the port on the
 * returning packet we can calculate the correct destination core.
 *
 * @param mbuf
 *   The received packet.
 * @param ctx
 *   The TCP context of this thread.
 * @param port_id
 *   The port identifier.
 * @param is_idle
 *   Boolean flag indicating that the rx is idle.
 * @return
 *   Was the packet redirected.
 *    - true: Packet was redirected.
 *    - false: Packet was not redirected.
**/
bool CFlowGenListPerThread::handle_astf_software_rss(pkt_dir_t dir,
                                                     rte_mbuf_t* mbuf,
                                                     CTcpPerThreadCtx* ctx,
                                                     tvpid_t port_id) {
    if (dir == SERVER_SIDE) {
        // Need to handle like all the packets.
        return false;
    }

    CFlowKeyTuple tuple;
    CFlowKeyFullTuple ftuple;

    CSimplePacketParser parser(mbuf);

    tcp_rx_pkt_action_t action;

    ctx->m_ft.parse_packet(mbuf,
                parser,
                tuple,
                ftuple,
                ctx->get_rx_checksum_check(),
                action,
                port_id);

    if (action != tPROCESS) {
        // Shouldn't process this packet. Usual handle.
        return false;
    }

    uint16_t src_port = (uint16_t)tuple.get_sport();
    /* The source port is aligned to the core id upon creation, and the dedicated core can be found using
    the destination port. The parser automatically understands source/destination per dir. */

    /* The rss_thread_id can be retrieved from the source port but this is not enough in case of multiple
       dual ports. We need to redirect the packet to the correct thread_id (not the correct rss_thread_id).
       The thread_ids per dual port are i, i + num_dual_ports, i + 2*num_dual_ports ...for the dual port number i.
       However the rss_thread_id are 0, 1, 2, ... num_cores_per_dual_port for each dual port.
       The following code calculates the correct thread id from the source port.
    */
    uint8_t reta_mask = CGlobalInfo::m_options.m_reta_mask;
    uint8_t num_cores_per_dual_port = CGlobalInfo::m_options.preview.getCores();
    uint8_t rss_thread_id = rss_get_thread_id_aligned(true, src_port, num_cores_per_dual_port, reta_mask);
    uint8_t num_dual_ports = uint8_t(CGlobalInfo::m_options.get_expected_dual_ports());
    uint8_t dedicated_thread_id = rss_thread_id * num_dual_ports + (m_thread_id % num_dual_ports);

    if (dedicated_thread_id != m_thread_id) {
        // redirect packet (cache it first) and redirect only when flushed.
        MbufRedirectCache* mbuf_redirect_cache = m_dp_core->get_mbuf_redirect_cache();
        mbuf_redirect_cache->redirect_packet(mbuf, dedicated_thread_id);
        return true;
    }
    return false;
}

template<bool ASTF_SOFTWARE_RSS>
uint16_t CFlowGenListPerThread::handle_rx_pkts(bool is_idle) {
    CVirtualIF * v_if=m_node_gen.m_v_if;
    rte_mbuf_t * rx_pkts[64];
    int dir;
    uint16_t cnt;
    bool redirected = false;

    CTcpPerThreadCtx * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    tvpid_t   ports_id[2];

    uint16_t sum;
    uint16_t sum_both_dir = 0;
    get_port_ids(ports_id[0], ports_id[1]);
    bool tunnel_loopback = CGlobalInfo::m_options.m_tunnel_loopback;

    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx = mctx_dir[dir];
        sum=0;
        while (true) {
            if (unlikely(ASTF_SOFTWARE_RSS)) {
                cnt = rx_burst_astf_software_rss(dir, rx_pkts, 64);
            } else {
                cnt = v_if->rx_burst(dir, rx_pkts, 64);
            }
            if (cnt==0) {
                break;
            }
            m_cpu_dp_u.start_work1();   // for TrexDpCore::idle_state_loop()
            int i;
            for (i=0; i<(int)cnt;i++) {
                rte_mbuf_t * m=rx_pkts[i];

#ifdef _DEBUG
                if ( CGlobalInfo::m_options.preview.getVMode() ==7 ){
                    fprintf(stdout,"RX---> dir %d \n",dir);
                    utl_rte_pktmbuf_dump_k12(stdout,m);

                    #ifdef RSS_DEBUG
                        if (dir==SERVER_SIDE) {
                            uint8_t *p1=rte_pktmbuf_mtod(m, uint8_t*);
                            uint32_t rss_hash=rss_calc(p1+14+12,12);
                            uint16_t c=CGlobalInfo::m_options.preview.getCores();
                            uint16_t r=(m->hash.rss&CGlobalInfo::m_options.m_reta_mask) %c;
                            uint16_t rss_thread_id = (m_thread_id/(CGlobalInfo::m_options.get_expected_dual_ports() ));
                            fprintf(stdout,"RX---> dir %d tid:%d(%d) hash:%08x sw:%08x RSS: %x, rss_thread %d -> (%d:%d)%s\n",dir,m_thread_id,rss_thread_id,m->hash.rss,rss_hash,((m->ol_flags&0x2)?1:0),r,r,rss_thread_id,((r==rss_thread_id)?"OK":"ERROR"));
                        }
                    #endif
                }
#endif          
                redirected = false;
                if (unlikely(ASTF_SOFTWARE_RSS)) {
                    redirected = handle_astf_software_rss(dir, m, ctx, ports_id[dir]);
                }
                if (!redirected) {
                    //remove the tunnel_ctx in server side in case of loopback mode
                    int res=0;
                    if (dir == SERVER_SIDE && tunnel_loopback) {
                        res=ctx->m_tunnel_handler->on_rx(SERVER_SIDE, m);
                        if (res) {
                            rte_pktmbuf_free(m);
                        }
                    }
                    if (!res) {
                        ctx->m_ft.rx_handle_packet(ctx, m, is_idle,ports_id[dir]);
                    }
                }
            }
            sum+=cnt;
            if (sum>NUM_RX_OFFLOAD) {
                ctx->m_ft.inc_rx_throttled_cnt();
                break;
            }
        }
        ctx->m_ft.flush_software_lro(ctx);
        if (m_sched_accurate && sum){
            v_if->flush_tx_queue();
        }
        sum_both_dir += sum;
    }
    return sum_both_dir;
}

static CEmulAppApiImpl     m_tcp_bh_api_impl_c;
static CEmulAppApiUdpImpl  m_udp_bh_api_impl_c;

#ifndef TREX_SIM
uint16_t get_client_side_vlan(CVirtualIF * _ifs);
tunnel_cfg_data_t get_client_side_tunnel_cfg_data(CVirtualIF * _ifs);
#endif
void CFlowGenListPerThread::generate_flow(CPerProfileCtx * pctx, uint16_t _tg_id, CFlowBase* in_flow){

    if ( m_c_tcp->is_open_flow_enabled()==false ){
        m_c_tcp->m_ft.inc_err_c_new_flow_throttled_cnt();
        return;
    }

    CAstfTemplatesRW * c_rw = pctx->m_template_rw;

    /* choose template index */
    uint16_t template_id = c_rw->do_schedule_template(_tg_id);

    CAstfPerTemplateRW * cur = c_rw->get_template_by_id(template_id);
    CAstfDbRO    *   cur_tmp_ro = pctx->m_template_ro;

    CTupleBase  tuple;
    if (in_flow) {
        CAstfPerTemplateRW * in_tmp_rw = c_rw->get_template_by_id(in_flow->m_c_template_idx);

        CClientPool * client_gen = in_tmp_rw->m_tuple_gen.get_client_gen();
        client_gen->GenerateTuple(tuple, client_gen->get_ip_info_by_idx(in_flow->m_c_idx));

        CServerPoolBase * server_gen = cur->m_tuple_gen.get_server_gen();
        server_gen->GenerateTuple(tuple);
    } else {
        if (cur->check_limit() || !(cur->m_tuple_gen.has_active_clients())){
            /* we can't generate a flow, there is a limit  or Clients are not active yet*/
            return;
        }

        cur->m_tuple_gen.GenerateTuple(tuple);
    }

    if ( tuple.getClientPort() == ILLEGAL_PORT ){
        /* we can't allocate tuple, too much */
        m_c_tcp->m_ft.inc_err_c_new_tuple_err_cnt();
        return;
    }

    if (unlikely(CGlobalInfo::m_options.m_tunnel_enabled)) {
        if (!tuple.getTunnelCtx()){
            CTupleGeneratorSmart * lpgen = cur->m_tuple_gen.get_gen();
            if (lpgen->IsFreePortRequired(cur->m_client_pool_idx) ){
                lpgen->FreePort(cur->m_client_pool_idx,tuple.getClientId(), tuple.getClientPort());
            }
            return;
        }
    }

    /* it is not set by generator, need to take it from the pcap file */
    tuple.setServerPort(cur_tmp_ro->get_dport(template_id));

    ClientCfgBase * lpc=tuple.getClientCfg();

    tunnel_cfg_data_t tunnel_data;

    if (lpc) {
        if (lpc->m_initiator.has_vlan()){
            tunnel_data.m_vlan = lpc->m_initiator.get_vlan();
        }
    }else{
        if ( unlikely(CGlobalInfo::m_options.preview.get_vlan_mode() != CPreviewMode::VLAN_MODE_NONE) ) {
    #ifndef TREX_SIM
            tunnel_data = get_client_side_tunnel_cfg_data(m_node_gen.m_v_if);
    #endif
        }
    }

    /* priorty to global */
    bool is_ipv6 = CGlobalInfo::is_ipv6_enable() || 
                   c_rw->get_c_tuneables()->is_valid_field(CTcpTuneables::ipv6_enable) ||
                   cur->get_c_tune()->is_valid_field(CTcpTuneables::ipv6_enable);


    bool is_udp = cur->is_udp();

    CFlowBase * c_flow;
    uint16_t tg_id = cur_tmp_ro->get_template_tg_id(template_id);
    if (is_udp) {
        c_flow = m_c_tcp->m_ft.alloc_flow_udp(pctx,
                                               tuple.getClient(),
                                               tuple.getServer(),

                                               tuple.getClientPort(),
                                               tuple.getServerPort(),

                                               tunnel_data,
                                               is_ipv6,
                                               tuple.getTunnelCtx(),
                                               true,
                                               tg_id,
                                               template_id);
    }else{
        c_flow = m_c_tcp->m_ft.alloc_flow(pctx,
                                          tuple.getClient(),
                                          tuple.getServer(),
                                          tuple.getClientPort(),
                                          tuple.getServerPort(),
                                          tunnel_data,
                                          is_ipv6,
                                          tuple.getTunnelCtx(),
                                          tg_id,
                                          template_id);
    }

    #ifdef RSS_DEBUG 
    printf(" (%s) (%d) generated tuple %x:%x:%x:%x \n",__func__,m_thread_id,tuple.getClient(),tuple.getServer(),tuple.getClientPort(),tuple.getServerPort());
    #endif
    if (c_flow == (CFlowBase *)0) {
        return;
    }

    /* save tuple generator information into the flow */
    c_flow->set_tuple_generator(tuple.getClientId(),
                                cur->m_client_pool_idx, 
                                template_id,
                                true);

    /* set parent flow to continue after c_flow closed */
    if (in_flow) {
        c_flow->setup_base_flow(in_flow);
    }

    /* update default mac addrees, dir is zero client side  */
    m_node_gen.m_v_if->update_mac_addr_from_global_cfg(CLIENT_SIDE,c_flow->m_template.m_template_pkt);
    /* override by client config, if exists */
    if (lpc) {
        ClientCfgDirBase *b=&lpc->m_initiator;
        char * p =(char *)c_flow->m_template.m_template_pkt;
        if (b->has_src_mac_addr()){
            memcpy(p+6,b->get_src_mac_addr(),6);
        }
        if (b->has_dst_mac_addr()){
            memcpy(p,b->get_dst_mac_addr(),6);
        }
    }

    CFlowKeyTuple   c_tuple;
    c_tuple.set_src_ip(tuple.getClient());
    c_tuple.set_sport(tuple.getClientPort());
    c_tuple.set_dst_ip(tuple.getServer());
    c_tuple.set_dport(tuple.getServerPort());
    if (is_udp){
        c_tuple.set_proto(IPHeader::Protocol::UDP);
    }else{
        c_tuple.set_proto(IPHeader::Protocol::TCP);
    }
    c_tuple.set_ipv4(is_ipv6?false:true);

    m_stats.m_total_open_flows += 1;

    if (!m_c_tcp->m_ft.insert_new_flow(c_flow,c_tuple)){
        cur->try_dec_limit();   // flow close will increase value by try_inc_limit()
        /* need to free the tuple */
        m_c_tcp->m_ft.handle_close(m_c_tcp,c_flow,false);
        return;
    }

    cur->dec_limit();

    CEmulApp * app_c;

    app_c = &c_flow->m_app;

    app_c->set_program(cur_tmp_ro->get_client_prog(template_id));
    app_c->set_peer_id(cur_tmp_ro->get_template_server_id(template_id));

    if (is_udp){
        CUdpFlow * udp_flow=(CUdpFlow*)c_flow;;
        app_c->set_bh_api(&m_udp_bh_api_impl_c);
        app_c->set_udp_flow_ctx(udp_flow->m_pctx,udp_flow);
        app_c->set_udp_flow();
        if (CGlobalInfo::m_options.preview.getEmulDebug() ){
            app_c->set_log_enable(true);
        }
        /* start connect */
        app_c->start(false);
        /* in UDP there are case that we need to open and close the flow in the first packet */
        //udp_flow->set_c_udp_info(cur, template_id);

        if (udp_flow->is_can_closed()) {
            m_c_tcp->m_ft.handle_close(m_c_tcp,c_flow,true);
        }

    }else{
        CTcpFlow * tcp_flow=(CTcpFlow*)c_flow;
        app_c->set_bh_api(&m_tcp_bh_api_impl_c);
        app_c->set_flow_ctx(tcp_flow->m_pctx,tcp_flow);
        if (CGlobalInfo::m_options.preview.getEmulDebug() ){
            app_c->set_log_enable(true);
            app_c->set_debug_id(0);
            tcp_flow->m_tcp.m_socket.so_options |= US_SO_DEBUG;
            tcp_set_debug_flow(&tcp_flow->m_tcp);
        }

        tcp_flow->set_app(app_c);
        tcp_flow->set_c_tcp_info(cur, template_id);

        /* start connect */
        tcp_flow->m_pctx->set_flow_info(tcp_flow);
        app_c->start(true);
        tcp_connect(&tcp_flow->m_tcp);
    }
    /* WARNING -- flow might be not valid here !!!! */
}

void CFlowGenListPerThread::handle_tx_fif(CGenNodeTXFIF * node,
                                              bool on_terminate){
    #ifdef TREX_SIM
    m_cur_time_sec =node->m_time;
    #endif

    m_node_gen.m_p_queue.pop();
    if ((on_terminate == false) && node->m_pctx) {
        m_cur_time_sec = node->m_time ;

        /* when e_duration is given, stop flow generation when it's over. */
        if (node->m_time_established) {
            if (node->m_pctx->get_time_connects()) {
                node->m_time_established = 0;   // clear establishment timeout
            }
            else if (m_cur_time_sec >= node->m_time_established) {
                node->m_time_stop = m_cur_time_sec;
                node->m_set_nc = true;
                node->m_terminate_duration = 0; // prevent waiting
            }
        }

        /* when t_duration is given without duration, stop when the traffic duration is over. */
        if (node->m_terminate_duration && !node->m_time_stop && node->m_pctx->get_time_connects()) {
            double traffic_time = node->m_pctx->get_time_lap();
            if (traffic_time >= node->m_terminate_duration) {
                node->m_time_stop = m_cur_time_sec;
                node->m_set_nc = true;  // stop immediately
                node->m_terminate_duration = 0;
            }
            else if (node->m_terminate_duration - traffic_time < node->m_pctx->m_fif_d_time) {
                node->m_pctx->m_fif_d_time = node->m_terminate_duration - traffic_time;
            }
        }

        if (node->m_time_stop && m_cur_time_sec >= node->m_time_stop) {
            if (get_is_interactive()) {
                bool stop_traffic = true;
                if (node->m_terminate_duration) {
                    node->m_time_stop += node->m_terminate_duration;
                    node->m_set_nc = true;
                    node->m_terminate_duration = 0;

                    stop_traffic = false;
                }

                TrexAstfDpCore* astf = (TrexAstfDpCore*)m_dp_core;
                astf->stop_transmit(node->m_pctx->m_profile_id, stop_traffic ? node->m_set_nc: false);

                if (stop_traffic) {
                    node->m_pctx->m_tx_node = nullptr; // for the safe profile stop
                    node->m_pctx = nullptr;
                }
            }
        }
        else if (node->m_pctx && node->m_pctx->is_active()) {
            generate_flow(node->m_pctx);
            /* when there is no flow to generate, profile stop can be triggered */
            if (!node->m_pctx->m_template_rw->has_flow_gen() && get_is_interactive()) {
                TrexAstfDpCore* astf = (TrexAstfDpCore*)m_dp_core;
                astf->stop_transmit(node->m_pctx->m_profile_id, false);

                if (!(node->m_terminate_duration || (node->m_time_stop && node->m_set_nc))) {
                    node->m_pctx->m_tx_node = nullptr; // for the safe profile stop
                    node->m_pctx = nullptr;
                }
            }
        }

        if (m_sched_accurate){
            CVirtualIF * v_if=m_node_gen.m_v_if;
            v_if->flush_tx_queue();
        }

        if (node->m_pctx) {
            node->m_time += node->m_pctx->m_fif_d_time;
            if (node->m_time_stop && node->m_time > node->m_time_stop) {
                node->m_time = node->m_time_stop;
            }
            m_node_gen.m_p_queue.push((CGenNode*)node);
        }else{
            free_node((CGenNode*)node);
        }
    }else{
        free_node((CGenNode*)node);
    }
}

void CFlowGenListPerThread::handle_tw(CGenNode * node,
                                          bool on_terminate){
    #ifdef TREX_SIM
    m_cur_time_sec = node->m_time;
    #endif

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += tcp_get_tw_tick_in_sec();
        m_node_gen.m_p_queue.push(node);
    }

    CTcpPerThreadCtx * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    int dir;
    bool any_event=false;
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx = mctx_dir[dir];
        ctx->maintain_resouce();
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
            m_tcp_terminate_cnt=0;
        }
    }
    
}


double CFlowGenListPerThread::tcp_get_tw_tick_in_sec(){
    return(TCP_TIME_TICK_SEC);
}

void CFlowGenListPerThread::Create_tcp_ctx(void) {
    assert(!m_c_tcp);
    assert(!m_s_tcp);

    m_c_tcp = new CTcpPerThreadCtx();
    m_s_tcp = new CTcpPerThreadCtx();

    CTcpIOCb * c_tcp_io = new CTcpIOCb();
    CTcpIOCb * s_tcp_io = new CTcpIOCb();

    c_tcp_io->m_dir =0;
    c_tcp_io->m_p   = this;
    s_tcp_io->m_dir =1;
    s_tcp_io->m_p   = this;

    uint32_t active_flows = get_max_active_flows_per_core_tcp()/2 ;
    if (active_flows<100000) {
        active_flows=100000;
    }

    m_c_tcp->Create(active_flows,true);
    m_s_tcp->Create(active_flows,false);
    m_c_tcp->set_thread(this);
    m_s_tcp->set_thread(this);

    m_c_tcp->set_cb(c_tcp_io);
    m_s_tcp->set_cb(s_tcp_io);

    uint8_t mem_socket_id = get_memory_socket_id();

    m_c_tcp->set_memory_socket(mem_socket_id);
    m_s_tcp->set_memory_socket(mem_socket_id);

    /* set dev flags */
    CPreviewMode * lp = &CGlobalInfo::m_options.preview;

    uint8_t dev_offload_flags=0;
    if (lp->getChecksumOffloadEnable()) {
        dev_offload_flags |= (OFFLOAD_RX_CHKSUM | OFFLOAD_TX_CHKSUM);
    }else{
        dev_offload_flags |= (OFFLOAD_TX_CHKSUM);
    }
    if (lp->get_dev_tso_support()) {
        dev_offload_flags |= TCP_OFFLOAD_TSO;
    }

    m_c_tcp->set_offload_dev_flags(dev_offload_flags);
    m_s_tcp->set_offload_dev_flags(dev_offload_flags);

    uint8_t dmode= m_preview_mode.getVMode();
    if ( (dmode > 2) && (dmode < 7) ){
        m_c_tcp->m_ft.set_debug(true);
        m_s_tcp->m_ft.set_debug(true);
    }

    m_s_tcp->m_ft.set_tcp_api(&m_tcp_bh_api_impl_c);
    m_s_tcp->m_ft.set_udp_api(&m_udp_bh_api_impl_c);
}

void CFlowGenListPerThread::load_tcp_profile(profile_id_t profile_id, bool is_first, CAstfDB* astf_db) {
    /* clear global statistics when new profile is started only */
    if (is_first) {
        m_stats.clear();    // moved from TrexAstfDpCore::create_tcp_batch()

        m_c_tcp->m_ft.m_sts.Clear();
        m_s_tcp->m_ft.m_sts.Clear();
    }
    m_c_tcp->create_profile_ctx(profile_id);
    m_s_tcp->create_profile_ctx(profile_id);

    uint8_t mem_socket_id = get_memory_socket_id();
    CAstfDbRO *template_db = astf_db->get_db_ro(mem_socket_id);
    if ( !template_db ) {
        throw TrexException("Could not create RO template database");
    }
    m_c_tcp->set_template_ro(template_db, profile_id);
    m_c_tcp->resize_stats(profile_id);
    m_s_tcp->set_template_ro(template_db, profile_id);
    m_s_tcp->resize_stats(profile_id);
    m_s_tcp->append_server_ports(profile_id);
    CAstfTemplatesRW * rw = astf_db->get_db_template_rw(
            mem_socket_id,
            nullptr, /* use CAstfDB's internal generator */
            m_thread_id,
            m_max_threads,
            getDualPortId());
    if (!rw) {
        throw TrexException("Could not create RW per-thread database");
    }

    m_c_tcp->set_template_rw(rw, profile_id);
    m_s_tcp->set_template_rw(rw, profile_id);

    m_c_tcp->append_active_profile(profile_id);
    m_s_tcp->append_active_profile(profile_id);

    m_c_tcp->get_profile_ctx(profile_id)->update_tuneables(rw->get_c_tuneables());
    m_s_tcp->get_profile_ctx(profile_id)->update_tuneables(rw->get_s_tuneables());

    /* call startup for client side */
    m_c_tcp->call_startup(profile_id);
    m_s_tcp->call_startup(profile_id);
}
void CFlowGenListPerThread::load_tcp_profile() {
    load_tcp_profile(0, true, CAstfDB::instance());
}

void CFlowGenListPerThread::unload_tcp_profile(profile_id_t profile_id, bool is_last, CAstfDB* astf_db) {
    m_s_tcp->remove_server_ports(profile_id);

    assert(m_c_tcp->profile_flow_cnt(profile_id) == 0);
    assert(m_s_tcp->profile_flow_cnt(profile_id) == 0);

    m_c_tcp->remove_active_profile(profile_id);
    m_s_tcp->remove_active_profile(profile_id);

    if (astf_db) {
        astf_db->clear_db_ro_rw(nullptr, m_thread_id);
    }

    m_c_tcp->set_template_ro(nullptr, profile_id);
    m_c_tcp->set_template_rw(nullptr, profile_id);
    m_s_tcp->set_template_ro(nullptr, profile_id);
    m_s_tcp->set_template_rw(nullptr, profile_id);

    if (is_last) {
        m_sched_accurate = false;
    }
}
void CFlowGenListPerThread::unload_tcp_profile() {
    unload_tcp_profile(0, true, CAstfDB::instance());
}

void CFlowGenListPerThread::remove_tcp_profile(profile_id_t profile_id) {
    m_s_tcp->remove_profile_ctx(profile_id);
    m_c_tcp->remove_profile_ctx(profile_id);
}

void CFlowGenListPerThread::Delete_tcp_ctx(){
    if (m_c_tcp) {
        CTcpIOCb * c_tcp_io = (CTcpIOCb *)m_c_tcp->get_cb();
        if (c_tcp_io) {
            delete c_tcp_io;
        }
        m_c_tcp->Delete();
        delete m_c_tcp;
        m_c_tcp=0;
    }
    if (m_s_tcp) {
        CTcpIOCb * s_tcp_io = (CTcpIOCb *)m_s_tcp->get_cb();
        if (s_tcp_io) {
            delete s_tcp_io;
        }
        m_s_tcp->Delete();
        delete m_s_tcp;
        m_s_tcp=0;
    }
}

