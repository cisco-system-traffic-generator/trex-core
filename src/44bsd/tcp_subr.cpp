/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)tcp_subr.c  8.2 (Berkeley) 5/24/95
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include <stdlib.h>
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"
#include "tcp_socket.h"
#include <common/utl_gcc_diag.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <stddef.h>
#include <common/Network/Packet/IPv6Header.h>
#include <astf/astf_template_db.h>
#include <astf/astf_db.h>
#include <cmath>

//extern    struct inpcb *tcp_last_inpcb;

#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)


tcpstat::tcpstat(uint16_t num_of_tg_ids){
    Init(num_of_tg_ids);
}

tcpstat::~tcpstat() {
    delete[] m_sts_tg_id;
}

void tcpstat::Init(uint16_t num_of_tg_ids) {
        m_sts_tg_id = new tcpstat_int_t[num_of_tg_ids];
        assert(m_sts_tg_id && "Operator new failed in tcp_subr.cpp/tcpstat::Init");
        m_num_of_tg_ids = num_of_tg_ids;
        Clear();
}

void tcpstat::Clear(){
    for (uint16_t tg_id = 0; tg_id < m_num_of_tg_ids; tg_id++){
        ClearPerTGID(tg_id);
    }
    memset(&m_sts,0,sizeof(tcpstat_int_t));
}

void tcpstat::ClearPerTGID(uint16_t tg_id){
    memset(m_sts_tg_id+tg_id,0,sizeof(tcpstat_int_t));
}

void tcpstat::Dump(FILE *fd){

    MYC(tcps_connattempt);
    MYC(tcps_accepts);
    MYC(tcps_connects);
    MYC(tcps_drops);
    MYC(tcps_conndrops);
    MYC(tcps_closed);
    MYC(tcps_segstimed);
    MYC(tcps_rttupdated);
    MYC(tcps_delack);
    MYC(tcps_timeoutdrop);
    MYC(tcps_rexmttimeo);
    MYC(tcps_rexmttimeo_syn);
    MYC(tcps_persisttimeo);
    MYC(tcps_keeptimeo);
    MYC(tcps_keepprobe);
    MYC(tcps_keepdrops);
    MYC(tcps_testdrops);

    MYC(tcps_sndtotal);
    MYC(tcps_sndpack);
    MYC(tcps_sndbyte);
    MYC(tcps_sndrexmitpack);
    MYC(tcps_sndrexmitbyte);
    MYC(tcps_sndacks);
    MYC(tcps_sndprobe);
    MYC(tcps_sndurg);
    MYC(tcps_sndwinup);
    MYC(tcps_sndctrl);

    MYC(tcps_rcvtotal);
    MYC(tcps_rcvpack);
    MYC(tcps_rcvbyte);
    MYC(tcps_rcvbadsum);
    MYC(tcps_rcvbadoff);
    MYC(tcps_rcvshort);
    MYC(tcps_rcvduppack);
    MYC(tcps_rcvdupbyte);
    MYC(tcps_rcvpartduppack);
    MYC(tcps_rcvpartdupbyte);
    MYC(tcps_rcvoopack);
    MYC(tcps_rcvoobyte);
    MYC(tcps_rcvoopackdrop);
    MYC(tcps_rcvoobytesdrop);
    MYC(tcps_rcvpackafterwin);
    MYC(tcps_rcvbyteafterwin);
    MYC(tcps_rcvafterclose);
    MYC(tcps_rcvwinprobe);
    MYC(tcps_rcvdupack);
    MYC(tcps_rcvacktoomuch);
    MYC(tcps_rcvackpack);
    MYC(tcps_rcvackbyte);
    MYC(tcps_rcvwinupd);
    MYC(tcps_pawsdrop);
    MYC(tcps_predack);
    MYC(tcps_preddat);
    MYC(tcps_persistdrop);
    MYC(tcps_badsyn);
    MYC(tcps_reasalloc);
    MYC(tcps_reasfree);
    MYC(tcps_nombuf);
}

void tcpstat::Resize(uint16_t new_num_of_tg_ids) {
    delete[] m_sts_tg_id;
    Init(new_num_of_tg_ids);
}

/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
int tcp_connect(CPerProfileCtx * pctx,
                struct tcpcb *tp) {
    int error;

    /* Compute window scaling to request.  */
    while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
        (TCP_MAXWIN << tp->request_r_scale) < tp->m_socket.so_rcv.sb_hiwat){
        tp->request_r_scale++;
    }

    soisconnecting(&tp->m_socket);

    CTcpPerThreadCtx * ctx = pctx->m_ctx;

    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_connattempt);
    tp->t_state = TCPS_SYN_SENT;
    tp->t_timer[TCPT_KEEP] = ctx->tcp_keepinit;
    tp->iss = ctx->tcp_iss;
    ctx->tcp_iss += TCP_ISSINCR/4;
    tcp_sendseqinit(tp);
    error = tcp_output(pctx,tp);
    return (error);
}
#ifdef  TREX_SIM
int tcp_connect(CTcpPerThreadCtx * ctx,struct tcpcb *tp) { return tcp_connect(DEFAULT_PROFILE_CTX(ctx), tp); }
#endif

int tcp_listen(CPerProfileCtx * pctx,
                struct tcpcb *tp) {
    assert( tp->t_state == TCPS_CLOSED);
    tp->t_state = TCPS_LISTEN;
    return(0);
}


/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb * tcp_usrclosed(CPerProfileCtx * pctx,
                             struct tcpcb *tp){

    switch (tp->t_state) {

    case TCPS_CLOSED:
    case TCPS_LISTEN:
    case TCPS_SYN_SENT:
        tp->t_state = TCPS_CLOSED;
        tp = tcp_close(pctx,tp);
        break;

    case TCPS_SYN_RECEIVED:
    case TCPS_ESTABLISHED:
        tp->t_state = TCPS_FIN_WAIT_1;
        break;

    case TCPS_CLOSE_WAIT:
        tp->t_state = TCPS_LAST_ACK;
        break;
    }
    if ((tp->t_state != TCPS_CLOSED) && tp->t_state >= TCPS_FIN_WAIT_2){
        soisdisconnected(&tp->m_socket);
    }
    return (tp);
}



/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb * tcp_disconnect(CPerProfileCtx * pctx,
                   struct tcpcb *tp){

    struct tcp_socket *so = &tp->m_socket;

    if (tp->t_state < TCPS_ESTABLISHED)
        tp = tcp_close(pctx,tp);
    else if (1 /*(so->so_options & SO_LINGER) && so->so_linger == 0*/){
        tp = tcp_drop_now(pctx,tp, 0);
    } else {
        soisdisconnecting(so);
        sbflush(&so->so_rcv);
        tp = tcp_usrclosed(pctx,tp);
        if (tp->t_state != TCPS_CLOSED)
            (void) tcp_output(pctx,tp);
    }
    return (tp);
}


void CTcpFlow::init(){
    /* build template */
    CFlowBase::init();

    if (m_template.is_tcp_tso()){
        /* to cache the info*/
        m_tcp.m_tuneable_flags |=TUNE_TSO_CACHE;
    }

    if (m_tcp.is_tso()){
        if (m_tcp.t_maxseg >m_tcp.m_max_tso ){
            m_tcp.m_max_tso=m_tcp.t_maxseg;
        }
    }else{
        m_tcp.m_max_tso=m_tcp.t_maxseg;
    }

    /* default keepalive */
    m_tcp.m_socket.so_options = US_SO_KEEPALIVE;

    /* register the timer */
    m_pctx->m_ctx->timer_w_start(this);
}


void CFlowBase::Create(CPerProfileCtx *pctx, uint16_t tg_id){
    m_pad[0]=0;
    m_pad[1]=0;
    m_c_idx_enable =0;
    m_c_idx=0;
    m_c_pool_idx=0;
    m_c_template_idx=0;
    m_pctx=pctx;
    m_tg_id=tg_id;
}

void CFlowBase::Delete(){

}

void CFlowBase::init(){
        /* build template */
    m_template.set_offload_mask(m_pctx->m_ctx->m_offload_flags);

    m_template.build_template(m_pctx,m_c_template_idx);
}

void CFlowBase::learn_ipv6_headers_from_network(IPv6Header * net_ipv6){
    m_template.learn_ipv6_headers_from_network(net_ipv6);
}



void CTcpFlow::Create(CPerProfileCtx *pctx, uint16_t tg_id){
    CFlowBase::Create(pctx, tg_id);
    m_tick=0;
    m_timer.reset();
    m_timer.m_type = 0;

    /* TCP_OPTIM  */
    tcpcb *tp=&m_tcp;
    memset((char *) tp, 0,sizeof(struct tcpcb));
    m_timer.m_type = ttTCP_FLOW;

    CTcpPerThreadCtx * ctx = pctx->m_ctx;

    tp->m_socket.so_snd.Create(ctx->tcp_tx_socket_bsize);
    tp->m_socket.so_rcv.sb_hiwat = ctx->tcp_rx_socket_bsize;

    tp->t_maxseg = ctx->tcp_mssdflt;
    tp->m_max_tso = ctx->tcp_max_tso;

    tp->mbuf_socket = ctx->m_mbuf_socket;

    tp->t_flags = ctx->tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
    tp->t_flags |= ctx->tcp_no_delay?(TF_NODELAY):0;

    /*
     * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
     * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
     * reasonable initial retransmit time.
     */
    tp->t_srtt = TCPTV_SRTTBASE;
    tp->t_rttvar = ctx->tcp_rttdflt * PR_SLOWHZ ;
    tp->t_rttmin = TCPTV_MIN;
    TCPT_RANGESET(tp->t_rxtcur,
        ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
        TCPTV_MIN, TCPTV_REXMTMAX);
    tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
    tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
    /* back pointer */
    tp->m_flow=this;
}
#ifdef  TREX_SIM
void CTcpFlow::Create(CTcpPerThreadCtx *ctx, uint16_t tg_id) {
    Create(DEFAULT_PROFILE_CTX(ctx), tg_id);
}
#endif



void CTcpFlow::set_c_tcp_info(const CAstfPerTemplateRW *rw_db, uint16_t temp_id) {
    m_tcp.m_tuneable_flags = 0;

    CTcpTuneables *tune = rw_db->get_c_tune();
    if (!tune)
        return;

    if (tune->is_empty())
        return;
    /* TCP object is part of a bigger object */
    m_tcp.m_tuneable_flags |= TUNE_HAS_PARENT_FLOW;

    if (tune->is_valid_field(CTcpTuneables::tcp_mss_bit) ) {
        m_tcp.m_tuneable_flags |= TUNE_MSS;
        m_tcp.t_maxseg = tune->m_tcp_mss;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_initwnd_bit) ) {
        m_tcp.m_tuneable_flags |= TUNE_INIT_WIN;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_no_delay) ) {
        m_tcp.m_tuneable_flags |= TUNE_NO_DELAY;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_rx_buf_size)) {
        m_tcp.m_socket.so_rcv.sb_hiwat = tune->m_tcp_rxbufsize;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_tx_buf_size)) {
        m_tcp.m_socket.so_snd.sb_hiwat = tune->m_tcp_txbufsize;
    }

}

void CTcpFlow::set_s_tcp_info(const CAstfDbRO * ro_db, CTcpTuneables *tune) {
    m_tcp.m_tuneable_flags = 0;

    if (!tune)
        return;

    if (tune->is_empty())
        return;

    /* TCP object is part of a bigger object */
    m_tcp.m_tuneable_flags |= TUNE_HAS_PARENT_FLOW;

    if (tune->is_valid_field(CTcpTuneables::tcp_mss_bit)) {
        m_tcp.m_tuneable_flags |= TUNE_MSS;
        m_tcp.t_maxseg = tune->m_tcp_mss;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_initwnd_bit)) {
        m_tcp.m_tuneable_flags |= TUNE_INIT_WIN;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_no_delay) ) {
        m_tcp.m_tuneable_flags |= TUNE_NO_DELAY;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_rx_buf_size)) {
        m_tcp.m_socket.so_rcv.sb_hiwat = tune->m_tcp_rxbufsize;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_tx_buf_size)) {
        m_tcp.m_socket.so_snd.sb_hiwat = tune->m_tcp_txbufsize;
    }

}


void CTcpFlow::on_slow_tick(){
    tcp_slowtimo(m_pctx, &m_tcp);
}


void CTcpFlow::on_fast_tick(){
    tcp_fasttimo(m_pctx, &m_tcp);
}


void CTcpFlow::Delete(){
    struct tcpcb *tp=&m_tcp;
    tcp_reass_clean(m_pctx,tp);
    m_pctx->m_ctx->timer_w_stop(this);
}


#define my_unsafe_container_of(ptr, type, member)              \
    ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))

#define my_unsafe_container_app(ptr, type,func)              \
    ((type *) ((uint8_t *)(ptr) - type::func()))


static void ctx_timer(void *userdata,
                       CHTimerObj *tmr){
    CTcpPerThreadCtx * ctx=(CTcpPerThreadCtx * )userdata;
    CEmulApp * app;
    if (likely(tmr->m_type==ttTCP_FLOW)) {
        /* most common */
        {
            CTcpFlow * tcp_flow;
            UNSAFE_CONTAINER_OF_PUSH;
            tcp_flow=my_unsafe_container_of(tmr,CTcpFlow,m_timer);
            UNSAFE_CONTAINER_OF_POP;
            tcp_flow->on_tick();
            ctx->timer_w_restart(tcp_flow);
        }
        return;
    }
    switch (tmr->m_type) {
    case ttTCP_APP:
        UNSAFE_CONTAINER_OF_PUSH;
        app=my_unsafe_container_app(tmr,CEmulApp,timer_offset);
        UNSAFE_CONTAINER_OF_POP;
        app->on_tick();
        break;
    case ttUDP_FLOW:
        {
        CUdpFlow * udp_flow;
        UNSAFE_CONTAINER_OF_PUSH;
        udp_flow=my_unsafe_container_of(tmr,CUdpFlow,m_keep_alive_timer);
        UNSAFE_CONTAINER_OF_POP;
        udp_flow->on_tick();
        ctx->handle_udp_timer(udp_flow);
        }
        break;
    case ttUDP_APP:
        UNSAFE_CONTAINER_OF_PUSH;
        app=my_unsafe_container_app(tmr,CEmulApp,timer_offset);
        UNSAFE_CONTAINER_OF_POP;
        app->on_tick();
        ctx->handle_udp_timer(app->get_udp_flow());
        break;
    case ttGen:
        {
            CAstfTimerObj * tobj=(CAstfTimerObj *)tmr;
            tobj->m_cb(userdata,tobj->m_userdata1,tobj);
        }
        break;
    case ttGenFunctor:
        {
            CAstfTimerFunctorObj * tobj=(CAstfTimerFunctorObj *)tmr;
            tobj->m_cb(tobj);
        }
        break;

    default:
        assert(0);
    };
}

void CTcpPerThreadCtx::cleanup_flows() {
    m_ft.terminate_all_flows();
    for (auto it: m_profiles) {
        delete_startup(it.first);
    }
    assert(m_timer_w.is_any_events_left()==0);
}

/*  this function is called every 20usec to see if we have an issue with resource */
void CTcpPerThreadCtx::maintain_resouce(){
    if ( m_ft.flow_table_resource_ok() ) {
        if (m_disable_new_flow==1) {
            m_disable_new_flow=0;
        }
    }else{
        if (m_disable_new_flow==0) {
            m_ft.inc_flow_overflow_cnt();
        }
        m_disable_new_flow=1;
    }
    /* TBD mode checks here */
}

bool CTcpPerThreadCtx::is_open_flow_enabled(){
    if (m_disable_new_flow==0) {
        return(true);
    }
    return(false);
}

/* tick every 50msec TCP_TIMER_W_TICK */
void CTcpPerThreadCtx::timer_w_on_tick(){
#ifndef TREX_SIM
    /* we have two levels on non-sim */
    uint32_t left;
    m_timer_w.on_tick_level0((void*)this,ctx_timer);
    m_timer_w.on_tick_level_count(1,(void*)this,ctx_timer,16,left);
#else
    m_timer_w.on_tick_level0((void*)this,ctx_timer);
#endif

    if ( m_tick==TCP_SLOW_RATIO_MASTER ) {
        tcp_maxidle = tcp_keepcnt * tcp_keepintvl;
        if (tcp_maxidle > (TCPTV_2MSL)) {
            tcp_maxidle = (TCPTV_2MSL);
        }

        tcp_iss += TCP_ISSINCR/PR_SLOWHZ;       /* increment iss */
        tcp_now++;                  /* for timestamps */
        m_tick=0;
    } else{
        m_tick++;
    }
}

#ifndef TREX_SIM
static uint16_t _update_slow_fast_ratio(uint16_t tcp_delay_ack_msec){
    double factor = round((double)TCP_TIMER_TICK_SLOW_MS/(double)tcp_delay_ack_msec);
    uint16_t res=(uint16_t)factor;
    if (res<1) {
        res=1;
    }
    return(res);
}
#endif

void CTcpPerThreadCtx::reset_tuneables() {
    tcp_blackhole = 0;
    tcp_do_rfc1323 = 1;
    tcp_fast_tick_msec = TCP_FAST_TICK_;
    tcp_initwnd = _update_initwnd(TCP_MSS, TCP_INITWND_FACTOR);
    tcp_initwnd_factor = TCP_INITWND_FACTOR;
    tcp_keepidle = TCPTV_KEEP_IDLE;
    tcp_keepinit = TCPTV_KEEP_INIT;
    tcp_keepintvl = TCPTV_KEEPINTVL;
    tcp_mssdflt = TCP_MSS;
    tcp_no_delay = 0;
    tcp_rx_socket_bsize = 32*1024;
    tcp_slow_fast_ratio = TCP_SLOW_FAST_RATIO_;
    tcp_tx_socket_bsize = 32*1024;
    tcprexmtthresh = 3;
}

void CTcpPerThreadCtx::update_tuneables(CTcpTuneables *tune) {
    if (tune == NULL)
        return;

    if (tune->is_empty())
        return;

    if (tune->is_valid_field(CTcpTuneables::tcp_mss_bit)) {
        tcp_mssdflt = tune->m_tcp_mss;
        tcp_initwnd = _update_initwnd(tcp_mssdflt,tcp_initwnd_factor);
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_initwnd_bit)) {
        tcp_initwnd_factor = tune->m_tcp_initwnd;
        tcp_initwnd = _update_initwnd(tcp_mssdflt,tcp_initwnd_factor);
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_rx_buf_size)) {
        tcp_rx_socket_bsize = tune->m_tcp_rxbufsize;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_tx_buf_size)) {
        tcp_tx_socket_bsize = tune->m_tcp_txbufsize;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_rexmtthresh)) {
        tcprexmtthresh = (int)tune->m_tcp_rexmtthresh;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_do_rfc1323)) {
        tcp_do_rfc1323 = (int)tune->m_tcp_do_rfc1323;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_no_delay)) {
        tcp_no_delay = (int)tune->m_tcp_no_delay;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_keepinit)) {
        tcp_keepinit = (int)tune->m_tcp_keepinit;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_keepidle)) {
        tcp_keepidle = (int)tune->m_tcp_keepidle;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_keepintvl)) {
        tcp_keepintvl = (int)tune->m_tcp_keepintvl;
    }

    if (tune->is_valid_field(CTcpTuneables::tcp_blackhole)) {
        tcp_blackhole  = (int)tune->m_tcp_blackhole;
    }

    #ifndef TREX_SIM
    if (tune->is_valid_field(CTcpTuneables::tcp_delay_ack)) {
        tcp_fast_tick_msec =  tw_time_msec_to_ticks(tune->m_tcp_delay_ack_msec);
        tcp_slow_fast_ratio = _update_slow_fast_ratio(tcp_fast_tick_msec);
    }
    #endif
}

void CTcpPerThreadCtx::resize_stats(profile_id_t profile_id) {
    uint16_t num_of_tg_ids = get_template_ro(profile_id)->get_num_of_tg_ids();
    get_tcpstat(profile_id)->Resize(num_of_tg_ids);
    get_udpstat(profile_id)->Resize(num_of_tg_ids);
}

bool CTcpPerThreadCtx::Create(uint32_t size,
                              bool is_client){
    uint32_t seed;
    uint32_t timestamp;
    #ifdef TREX_SIM
    seed=0x1234;
    timestamp=0;
    #else
    seed=rand();
    timestamp=seed;
    #endif
    m_rand = new KxuLCRand(seed);
    sb_max = SB_MAX;        /* patchable, not used  */
    m_mbuf_socket=0;
    m_offload_flags=0;
    tcp_max_tso = TCP_TSO_MAX_DEFAULT;
    tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
    tcp_keepcnt = TCPTV_KEEPCNT;        /* max idle probes */
    tcp_maxpersistidle = TCPTV_KEEP_IDLE;   /* max idle time in persist */
    tcp_maxidle=0;
    tcp_ttl=0;
    m_disable_new_flow=0;
    m_pad=0;
    tcp_iss = rand();   /* wrong, but better than a constant */
    m_tick=0;
    tcp_now=timestamp;
    m_cb = NULL;
    reset_tuneables();
    memset(&tcp_saveti,0,sizeof(tcp_saveti));

    RC_HTW_t tw_res;
    tw_res = m_timer_w.Create(1024,TCP_TIMER_LEVEL1_DIV);

    if (tw_res != RC_HTW_OK ){
        CHTimerWheelErrorStr err(tw_res);
        printf("Timer wheel configuration error,please look into the manual for details  \n");
        printf("ERROR  %-30s  - %s \n",err.get_str(),err.get_help_str());
        return(false);
    }
    if (TCP_TIMER_LEVEL1_DIV>1){
        /* on non-simulation we have two level active*/
        m_timer_w.set_level1_cnt_div();
    }

    if (!m_ft.Create(size,is_client)){
        printf("ERROR  can't create flow table \n");
        return(false);
    }

    return(true);
}


void CTcpPerThreadCtx::init_sch_rampup(profile_id_t profile_id){
        /* calc default fif rate*/
        astf_thread_id_t max_threads = get_template_rw(profile_id)->get_max_threads();
        set_fif_d_time(get_template_ro(profile_id)->get_delta_tick_sec_thread(max_threads), profile_id);

        /* get client tunables */
        CTcpTuneables * ctx_tune = get_template_rw(profile_id)->get_c_tuneables();

        if ( ctx_tune->is_valid_field(CTcpTuneables::sched_rampup) ){
            set_sch_rampup(new CAstfFifRampup(get_profile_ctx(profile_id),
                                              ctx_tune->m_scheduler_rampup,
                                              get_template_ro(profile_id)->get_total_cps_per_thread(max_threads)),
                           profile_id);
        }
}



void CTcpPerThreadCtx::call_startup(profile_id_t profile_id){
    if ( is_client_side() ){
        init_sch_rampup(profile_id);
    }
}

void CTcpPerThreadCtx::delete_startup(profile_id_t profile_id) {
    if (get_sch_rampup(profile_id)) {
        delete get_sch_rampup(profile_id);
        set_sch_rampup(nullptr, profile_id);
    }
}

void CTcpPerThreadCtx::Delete(){
    assert(m_rand);
    delete m_rand;
    m_rand=0;
    m_timer_w.Delete();
    m_ft.Delete();
    for (auto iter : m_profiles) {
        delete iter.second;
    }
}

void CTcpPerThreadCtx::append_server_ports(profile_id_t profile_id) {
    CPerProfileCtx * pctx = get_profile_ctx(profile_id);
    CAstfDbRO * template_db = pctx->m_template_ro;
    std::vector<uint16_t> server_ports;

    if (!template_db) {
        return;
    }

    server_ports.clear();
    template_db->enumerate_server_ports(server_ports, true);
    for (auto port: server_ports) {
        if (m_tcp_server_ports.find(port) != m_tcp_server_ports.end()) {
            throw TrexException("Two TCP servers with port " + std::to_string(port));
        }
        m_tcp_server_ports[port] = pctx;
    }
    server_ports.clear();
    template_db->enumerate_server_ports(server_ports, false);
    for (auto port: server_ports) {
        if (m_udp_server_ports.find(port) != m_udp_server_ports.end()) {
            throw TrexException("Two UDP servers with port " + std::to_string(port));
        }
        m_udp_server_ports[port] = pctx;
    }
}

void CTcpPerThreadCtx::remove_server_ports(profile_id_t profile_id) {
    CPerProfileCtx * pctx = get_profile_ctx(profile_id);
    CAstfDbRO * template_db = pctx->m_template_ro;
    std::vector<uint16_t> server_ports;

    if (!template_db) {
        return;
    }

    server_ports.clear();
    template_db->enumerate_server_ports(server_ports, true);
    for (auto port: server_ports) {
        auto it = m_tcp_server_ports.find(port);
        if ((it != m_tcp_server_ports.end()) && (it->second == pctx)) {
            m_tcp_server_ports.erase(port);
        }
    }
    server_ports.clear();
    template_db->enumerate_server_ports(server_ports, false);
    for (auto port: server_ports) {
        auto it = m_udp_server_ports.find(port);
        if ((it != m_udp_server_ports.end()) && (it->second == pctx)) {
            m_udp_server_ports.erase(port);
        }
    }
}

void CTcpPerThreadCtx::print_server_ports(bool stream) {
    if (stream) {
        for (auto it: m_tcp_server_ports) {
            std::cout << it.first << ": " << it.second << std::endl;
        }
        printf("[%p] TCP(%lu)\n", this, m_tcp_server_ports.size());
    }
    else {
        for (auto it: m_udp_server_ports) {
            std::cout << it.first << ": " << it.second << std::endl;
        }
        printf("[%p] UDP(%lu)\n", this, m_udp_server_ports.size());
    }
}

CPerProfileCtx * CTcpPerThreadCtx::get_profile_by_server_port(uint16_t port, bool stream) {
    if (stream) {
        if (m_tcp_server_ports.find(port) != m_tcp_server_ports.end()) {
            return m_tcp_server_ports[port];
        }
    }
    else {
        if (m_udp_server_ports.find(port) != m_udp_server_ports.end()) {
            return m_udp_server_ports[port];
        }
    }
    return FALLBACK_PROFILE_CTX(this);
}

static void tcp_template_ipv4_update(IPHeader *ipv4,
                                     CPerProfileCtx * pctx,
                                     uint16_t  template_id){

    CTcpPerThreadCtx * ctx = pctx->m_ctx;

    CAstfTemplatesRW * c_rw = pctx->m_template_rw;

    if (!c_rw){
        return;
    }

    CAstfPerTemplateRW * cur = c_rw->get_template_by_id(template_id);
    CTcpTuneables * ctx_tune;
    CTcpTuneables * t_tune;

    if (ctx->is_client_side()){
        t_tune   = cur->get_c_tune();
        ctx_tune = c_rw->get_c_tuneables();
    }else{
        t_tune = cur->get_s_tune();
        ctx_tune = c_rw->get_s_tuneables();
    }

    if (!ctx_tune || !t_tune){
       return;
    }

    if (ctx_tune->is_empty() && t_tune->is_empty()) {
        return;
    }

    if (t_tune->is_valid_field(CTcpTuneables::ip_ttl)) {
        ipv4->setTimeToLive(t_tune->m_ip_ttl);
    }else{
        if (ctx_tune->is_valid_field(CTcpTuneables::ip_ttl)){
            ipv4->setTimeToLive(ctx_tune->m_ip_ttl);
        }
    }

    if (t_tune->is_valid_field(CTcpTuneables::ip_tos)) {
        ipv4->setTOS(t_tune->m_ip_tos);
    }else{
        if (ctx_tune->is_valid_field(CTcpTuneables::ip_tos)){
            ipv4->setTOS(ctx_tune->m_ip_tos);
        }
    }
}

static void tcp_template_ipv6_update(IPv6Header *ipv6,
                                     CPerProfileCtx * pctx,
                                     uint16_t  template_id){

    CTcpPerThreadCtx * ctx = pctx->m_ctx;

    CAstfTemplatesRW * c_rw = pctx->m_template_rw;

    if (!c_rw){
        return;
    }
    CAstfPerTemplateRW * cur = c_rw->get_template_by_id(template_id);
    CTcpTuneables * ctx_tune;
    CTcpTuneables * t_tune;

    if (ctx->is_client_side()){
        t_tune   = cur->get_c_tune();
        ctx_tune = c_rw->get_c_tuneables();
    }else{
        t_tune = cur->get_s_tune();
        ctx_tune = c_rw->get_s_tuneables();
    }

    if (!ctx_tune || !t_tune){
       return;
    }

    if (ctx_tune->is_empty() && t_tune->is_empty()) {
        return;
    }

    if (t_tune->is_valid_field(CTcpTuneables::ip_ttl)) {
        ipv6->setHopLimit(t_tune->m_ip_ttl);
    }else{
        if (ctx_tune->is_valid_field(CTcpTuneables::ip_ttl)){
            ipv6->setHopLimit(ctx_tune->m_ip_ttl);
        }
    }

    if (t_tune->is_valid_field(CTcpTuneables::ip_tos)) {
        ipv6->setTrafficClass(t_tune->m_ip_tos);
    }else{
        if (ctx_tune->is_valid_field(CTcpTuneables::ip_tos)){
            ipv6->setTrafficClass(ctx_tune->m_ip_tos);
        }
    }

    if (!ctx->is_client_side()){
        /* in case of server side learn from the network */
        return;
    }

    if ( ctx_tune->is_valid_field(CTcpTuneables::ipv6_src_addr) ){
        ipv6->setSourceIpv6Raw(ctx_tune->m_ipv6_src);
    }else{
        if (t_tune->is_valid_field(CTcpTuneables::ipv6_src_addr)){
            ipv6->setSourceIpv6Raw(t_tune->m_ipv6_src);
        }
    }

    if ( ctx_tune->is_valid_field(CTcpTuneables::ipv6_dst_addr) ){
        ipv6->setDestIpv6Raw(ctx_tune->m_ipv6_dst);
    }else{
        if (t_tune->is_valid_field(CTcpTuneables::ipv6_dst_addr)){
            ipv6->setDestIpv6Raw(t_tune->m_ipv6_dst);
        }
    }
}



void CFlowTemplate::learn_ipv6_headers_from_network(IPv6Header * net_ipv6){

    assert(m_is_ipv6==1);
    uint8_t * p=m_template_pkt;
    IPv6Header *ipv6=(IPv6Header *)(p+m_offset_ip);
    ipv6->setSourceIpv6Raw((uint8_t*)&net_ipv6->myDestination[0]);
    ipv6->setDestIpv6Raw((uint8_t*)&net_ipv6->mySource[0]);

    /* recalculate */
    if (m_offload_flags & OFFLOAD_TX_CHKSUM){
        if (is_tcp()) {
            m_l4_pseudo_checksum = rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv6,(PKT_TX_IPV6 | PKT_TX_TCP_CKSUM));
        }else{
            m_l4_pseudo_checksum = rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv6,(PKT_TX_IPV6 | PKT_TX_UDP_CKSUM));
        }
    }else{
        m_l4_pseudo_checksum=0;
    }

}


void CFlowTemplate::server_update_mac_from_packet(uint8_t *pkt){
    /* copy thr MAC from the packet in reverse order */
    memcpy(m_template_pkt+6,pkt,6);
    memcpy(m_template_pkt,pkt+6,6);
}

void CFlowTemplate::build_template_ip(CPerProfileCtx * pctx,
                                      uint16_t  template_idx){

    const uint8_t default_ipv4_header[] = {
        0x00,0x00,0x00,0x01,0x0,0x0,  // Ethr
        0x00,0x00,0x00,0x02,0x0,0x0,
        0x08,00,


        0x45,0x00,0x00,0x00,          //Ipv4
        0x00,0x00,0x40,0x00,
        0x7f,0x11,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00
    };


    const uint8_t default_ipv6_header[] = {
        0x00,0x00,0x00,0x01,0x0,0x0,  // Ethr
        0x00,0x00,0x00,0x02,0x0,0x0,
        0x86,0xdd,


        0x60,0x00,0x00,0x00,          //Ipv6
        0x00,0x00,0x11,0x40,

        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, // src IP

        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, // dst IP
    };

    if (!m_is_ipv6) {
        uint8_t vlan_offset=0;
        if (m_vlan){
            vlan_offset=4;
        }
        m_offset_ip  = 14+vlan_offset;
        m_offset_l4 = m_offset_ip + 20;

        uint8_t *p=m_template_pkt;
        if (vlan_offset==0){
            memcpy(p,default_ipv4_header,sizeof(default_ipv4_header) );
        }else{
            memcpy(p,default_ipv4_header,sizeof(12));
            const uint8_t next_vlan[2]={0x81,00};
            memcpy(p+12,next_vlan,2);
            VLANHeader vlan_head;
            vlan_head.setVlanTag(m_vlan);
            vlan_head.setNextProtocolFromHostOrder(0x0800);
            memcpy(p+14,vlan_head.getPointer(),4);
            memcpy(p+18,default_ipv4_header+14,sizeof(default_ipv4_header)-14);
        }
        /* set default value */
        IPHeader *lpIpv4=(IPHeader *)(p+m_offset_ip);
        lpIpv4->setTotalLength(20); /* important for PH calculation */
        tcp_template_ipv4_update(lpIpv4,pctx,template_idx);
        lpIpv4->setDestIp(m_dst_ipv4);
        lpIpv4->setSourceIp(m_src_ipv4);
        lpIpv4->setProtocol(m_proto);
        lpIpv4->ClearCheckSum();
    }else{
        uint8_t vlan_offset=0;
        if (m_vlan){
            vlan_offset=4;
        }

        m_offset_ip  = 14+vlan_offset;
        m_offset_l4 = m_offset_ip + IPV6_HDR_LEN;

        uint8_t *p=m_template_pkt;
        if (vlan_offset==0){
            memcpy(p,default_ipv6_header,sizeof(default_ipv6_header) );
        }else{
            memcpy(p,default_ipv6_header,sizeof(12));
            const uint8_t next_vlan[2]={0x81,00};
            memcpy(p+12,next_vlan,2);
            VLANHeader vlan_head;
            vlan_head.setVlanTag(m_vlan);
            vlan_head.setNextProtocolFromHostOrder(0x86dd);
            memcpy(p+14,vlan_head.getPointer(),4);
            memcpy(p+18,default_ipv6_header+14,sizeof(default_ipv6_header)-14);
        }
        /* set default value */
        IPv6Header *ipv6=(IPv6Header *)(p+m_offset_ip);
        tcp_template_ipv6_update(ipv6,pctx,template_idx);
        ipv6->updateLSBIpv6Dst(m_dst_ipv4);
        ipv6->updateLSBIpv6Src(m_src_ipv4);
        ipv6->setNextHdr(m_proto);
        ipv6->setPayloadLen(0);  /* important for PH calculation */
    }
}


void CFlowTemplate::build_template_tcp(CPerProfileCtx * pctx){
       const uint8_t tcp_header[] = {
         0x00, 0x00, 0x00, 0x00, // src, dst ports  //TCP
         0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, // seq num, ack num
         0x50, 0x00, 0x00, 0x00, // Header size, flags, window size
         0x00, 0x00, 0x00, 0x00, // checksum ,urgent pointer
         0x00, 0x00, 0x00, 0x00
       };

       uint8_t *p=m_template_pkt;
       TCPHeader *lpTCP=(TCPHeader *)(p+m_offset_l4);
       memcpy(lpTCP,tcp_header,sizeof(tcp_header));

       lpTCP->setSourcePort(m_src_port);
       lpTCP->setDestPort(m_dst_port);
       if (m_offload_flags & OFFLOAD_TX_CHKSUM){
           if (m_is_ipv6) {
               m_l4_pseudo_checksum = rte_ipv6_phdr_cksum((struct ipv6_hdr *)(p+m_offset_ip),(PKT_TX_IPV6 | PKT_TX_TCP_CKSUM));
           }else{
               m_l4_pseudo_checksum = rte_ipv4_phdr_cksum((struct ipv4_hdr *)(p+m_offset_ip),(PKT_TX_IPV4 |PKT_TX_IP_CKSUM|PKT_TX_TCP_CKSUM));
           }
       }else{
            m_l4_pseudo_checksum=0;
       }
}


void CFlowTemplate::build_template_udp(CPerProfileCtx * pctx){

    const uint8_t udp_header[] = {
        0x00, 0x00, 0x00, 0x00, // src, dst ports  //UDP
        0x00, 0x00, 0x00, 0x00,
    };

    uint8_t *p=m_template_pkt;
    UDPHeader *lpUDP=(UDPHeader *)(p+m_offset_l4);
    memcpy(lpUDP,udp_header,sizeof(udp_header));
    lpUDP->setSourcePort(m_src_port);
    lpUDP->setDestPort(m_dst_port);
    if (m_offload_flags & OFFLOAD_TX_CHKSUM){
        if (m_is_ipv6) {
            m_l4_pseudo_checksum = rte_ipv6_phdr_cksum((struct ipv6_hdr *)(p+m_offset_ip),(PKT_TX_IPV6 | PKT_TX_UDP_CKSUM));
        }else{
            m_l4_pseudo_checksum = rte_ipv4_phdr_cksum((struct ipv4_hdr *)(p+m_offset_ip),(PKT_TX_IPV4 |PKT_TX_IP_CKSUM|PKT_TX_UDP_CKSUM));
        }
    }else{
        m_l4_pseudo_checksum=0;
    }
}


void CFlowTemplate::build_template(CPerProfileCtx * pctx,
                                   uint16_t  template_idx){

    build_template_ip(pctx,template_idx);
    if (is_tcp()) {
        build_template_tcp(pctx);
    }else{
        build_template_udp(pctx);
    }
}



/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb * tcp_drop_now(CPerProfileCtx * pctx,
                            struct tcpcb *tp,
                            int res){
    struct tcp_socket *so = &tp->m_socket;
    uint16_t tg_id = tp->m_flow->m_tg_id;
    if (TCPS_HAVERCVDSYN(tp->t_state)) {
        tp->t_state = TCPS_CLOSED;
        (void) tcp_output(pctx,tp);
        INC_STAT(pctx, tg_id, tcps_drops);
    } else{
        INC_STAT(pctx, tg_id, tcps_conndrops);
    }
    if (res == ETIMEDOUT && tp->t_softerror){
        res = tp->t_softerror;
    }
    so->so_error = res;
    return (tcp_close(pctx,tp));
}


/*
 * Close a TCP control block:
 *  discard all space held by the tcp
 *  discard internet protocol block
 *  wake up any sleepers
 */
struct tcpcb *
tcp_close(CPerProfileCtx * pctx,
          struct tcpcb *tp){


// no need for that , RTT will be calculated again for each flow
#ifdef RTV_RTT
    register struct rtentry *rt;

    /*
     * If we sent enough data to get some meaningful characteristics,
     * save them in the routing entry.  'Enough' is arbitrarily
     * defined as the sendpipesize (default 4K) * 16.  This would
     * give us 16 rtt samples assuming we only get one sample per
     * window (the usual case on a long haul net).  16 samples is
     * enough for the srtt filter to converge to within 5% of the correct
     * value; fewer samples and we could save a very bogus rtt.
     *
     * Don't update the default route's characteristics and don't
     * update anything that the user "locked".
     */
    if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
        (rt = inp->inp_route.ro_rt) &&
        ((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr != INADDR_ANY) {
        uint32_t  i;

        if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
            i = tp->t_srtt *
                (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
            if (rt->rt_rmx.rmx_rtt && i)
                /*
                 * filter this update to half the old & half
                 * the new values, converting scale.
                 * See route.h and tcp_var.h for a
                 * description of the scaling constants.
                 */
                rt->rt_rmx.rmx_rtt =
                    (rt->rt_rmx.rmx_rtt + i) / 2;
            else
                rt->rt_rmx.rmx_rtt = i;
        }
        if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
            i = tp->t_rttvar *
                (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
            if (rt->rt_rmx.rmx_rttvar && i)
                rt->rt_rmx.rmx_rttvar =
                    (rt->rt_rmx.rmx_rttvar + i) / 2;
            else
                rt->rt_rmx.rmx_rttvar = i;
        }
        /*
         * update the pipelimit (ssthresh) if it has been updated
         * already or if a pipesize was specified & the threshhold
         * got below half the pipesize.  I.e., wait for bad news
         * before we start updating, then update on both good
         * and bad news.
         */
        if ((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
            (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh ||
            i < (rt->rt_rmx.rmx_sendpipe / 2)) {
            /*
             * convert the limit from user data bytes to
             * packets then to packet data bytes.
             */
            i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
            if (i < 2)
                i = 2;
            i *= (uint32_t)(tp->t_maxseg + sizeof (struct tcpiphdr));
            if (rt->rt_rmx.rmx_ssthresh)
                rt->rt_rmx.rmx_ssthresh =
                    (rt->rt_rmx.rmx_ssthresh + i) / 2;
            else
                rt->rt_rmx.rmx_ssthresh = i;
        }
    }
#endif /* RTV_RTT */
    /* free the reassembly queue, if any */

    /* mark it as close and return zero */
    tp->t_state = TCPS_CLOSED;
    /* TBD -- back pointer to flow and delete it */
    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_closed);
    return((struct tcpcb *)tp);
}

void tcp_drain(){

}

#if 0
/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(inp, error)
    struct inpcb *inp;
    int error;
{
    register struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
    register struct socket *so = inp->inp_socket;

    /*
     * Ignore some errors if we are hooked up.
     * If connection hasn't completed, has retransmitted several times,
     * and receives a second error, give up now.  This is better
     * than waiting a long time to establish a connection that
     * can never complete.
     */
    if (tp->t_state == TCPS_ESTABLISHED &&
         (error == EHOSTUNREACH || error == ENETUNREACH ||
          error == EHOSTDOWN)) {
        return;
    } else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
        tp->t_softerror)
        so->so_error = error;
    else
        tp->t_softerror = error;
    wakeup((caddr_t) &so->so_timeo);
    sorwakeup(so);
    sowwakeup(so);
}

void
tcp_ctlinput(cmd, sa, ip)
    int cmd;
    struct sockaddr *sa;
    register struct ip *ip;
{
    register struct tcphdr *th;
    extern struct in_addr zeroin_addr;
    extern u_char inetctlerrmap[];
    void (*notify) __P((struct inpcb *, int)) = tcp_notify;

    if (cmd == PRC_QUENCH)
        notify = tcp_quench;
    else if (!PRC_IS_REDIRECT(cmd) &&
         ((unsigned)cmd > PRC_NCMDS || inetctlerrmap[cmd] == 0))
        return;
    if (ip) {
        th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
        in_pcbnotify(&tcb, sa, th->th_dport, ip->ip_src, th->th_sport,
            cmd, notify);
    } else
        in_pcbnotify(&tcb, sa, 0, zeroin_addr, 0, cmd, notify);
}
#endif
/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void tcp_quench(struct tcpcb *tp){

    tp->snd_cwnd = tp->t_maxseg;
}




