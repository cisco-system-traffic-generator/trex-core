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

           

//extern    struct inpcb *tcp_last_inpcb;

#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)


void tcpstat::Clear(){
    memset(&m_sts,0,sizeof(tcpstat_int_t));
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


/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
int tcp_connect(CTcpPerThreadCtx * ctx,
                struct tcpcb *tp) {
    int error;

    /* Compute window scaling to request.  */
    while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
        (TCP_MAXWIN << tp->request_r_scale) < tp->m_socket.so_rcv.sb_hiwat){
        tp->request_r_scale++;
    }

    soisconnecting(&tp->m_socket);

    INC_STAT(ctx,tcps_connattempt);
    tp->t_state = TCPS_SYN_SENT;
    tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
    tp->iss = ctx->tcp_iss; 
    ctx->tcp_iss += TCP_ISSINCR/4;
    tcp_sendseqinit(tp);
    error = tcp_output(ctx,tp);
    return (error);
}

int tcp_listen(CTcpPerThreadCtx * ctx,
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
struct tcpcb * tcp_usrclosed(CTcpPerThreadCtx * ctx,
                             struct tcpcb *tp){

    switch (tp->t_state) {

    case TCPS_CLOSED:
    case TCPS_LISTEN:
    case TCPS_SYN_SENT:
        tp->t_state = TCPS_CLOSED;
        tp = tcp_close(ctx,tp);
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
struct tcpcb * tcp_disconnect(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp){

    struct tcp_socket *so = &tp->m_socket;

    if (tp->t_state < TCPS_ESTABLISHED)
        tp = tcp_close(ctx,tp);
    else if (1 /*(so->so_options & SO_LINGER) && so->so_linger == 0*/){
        tp = tcp_drop_now(ctx,tp, 0);
    } else {
        soisdisconnecting(so);
        sbflush(&so->so_rcv);
        tp = tcp_usrclosed(ctx,tp);
        if (tp->t_state != TCPS_CLOSED)
            (void) tcp_output(ctx,tp);
    }
    return (tp);
}






void CTcpFlow::init(){
    /* build template */
    m_tcp.m_offload_flags =m_ctx->m_offload_flags;

    if (m_tcp.is_tso()){
        if (m_tcp.t_maxseg >m_tcp.m_max_tso ){
            m_tcp.m_max_tso=m_tcp.t_maxseg;
        }
    }else{
        m_tcp.m_max_tso=m_tcp.t_maxseg;
    }

    tcp_template(&m_tcp);

    /* default keepalive */
    m_tcp.m_socket.so_options = US_SO_KEEPALIVE;

    /* register the timer */

    m_ctx->timer_w_start(this);
}


void CTcpFlow::Create(CTcpPerThreadCtx *ctx){
    m_tick=0;

    /* TCP_OPTIM  */
    tcpcb *tp=&m_tcp;
    memset((char *) tp, 0,sizeof(struct tcpcb));

    m_pad[0]=0;
    m_pad[1]=0;
    m_c_idx_enable =0;
    m_c_idx=0;
    m_c_pool_idx=0;
    m_c_template_idx=0;

    m_ctx=ctx;
    m_timer.reset();

    tp->m_socket.so_snd.Create(ctx->tcp_tx_socket_bsize);
    tp->m_socket.so_rcv.sb_hiwat = ctx->tcp_rx_socket_bsize;

    tp->t_maxseg = ctx->tcp_mssdflt;
    tp->m_max_tso = ctx->tcp_max_tso;

    tp->mbuf_socket = ctx->m_mbuf_socket;

    tp->t_flags = ctx->tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;

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

}


void CTcpFlow::server_update_mac_from_packet(uint8_t *pkt){
    /* copy thr MAC from the packet in reverse order */
    memcpy(m_tcp.template_pkt+6,pkt,6);
    memcpy(m_tcp.template_pkt,pkt+6,6);
}


void CTcpFlow::on_slow_tick(){
    tcp_slowtimo(m_ctx, &m_tcp);
}


void CTcpFlow::on_fast_tick(){
    tcp_fasttimo(m_ctx, &m_tcp);
}


void CTcpFlow::Delete(){
    struct tcpcb *tp=&m_tcp;
    tcp_reass_clean(m_ctx,tp);
    m_ctx->timer_w_stop(this);
}


#define my_unsafe_container_of(ptr, type, member)              \
    ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))


static void tcp_timer(void *userdata,
                       CHTimerObj *tmr){
    CTcpPerThreadCtx * tcp_ctx=(CTcpPerThreadCtx * )userdata;
    UNSAFE_CONTAINER_OF_PUSH;
    CTcpFlow * tcp_flow=my_unsafe_container_of(tmr,CTcpFlow,m_timer);
    UNSAFE_CONTAINER_OF_POP;
    tcp_flow->on_tick();
    tcp_ctx->timer_w_restart(tcp_flow);
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
    uint32_t left;
    m_timer_w.on_tick_level_count(0,(void*)this,tcp_timer,16,left);
#else
    m_timer_w.on_tick_level0((void*)this,tcp_timer);
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



bool CTcpPerThreadCtx::Create(uint32_t size,
                              bool is_client){

    tcp_tx_socket_bsize=32*1024;
    tcp_rx_socket_bsize=32*1024 ;
    sb_max = SB_MAX;        /* patchable */
    m_mbuf_socket=0;
    m_offload_flags=0;
    tcprexmtthresh = 3 ;
    tcp_mssdflt = TCP_MSS;
    tcp_initwnd_factor=TCP_INITWND_FACTOR;
    tcp_initwnd = tcp_initwnd_factor * tcp_mssdflt;
    tcp_max_tso = TCP_TSO_MAX_DEFAULT;
    tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
    tcp_do_rfc1323 = 1;
    tcp_keepinit = TCPTV_KEEP_INIT;
    tcp_keepidle = TCPTV_KEEP_IDLE;
    tcp_keepintvl = TCPTV_KEEPINTVL;
    tcp_keepcnt = TCPTV_KEEPCNT;        /* max idle probes */
    tcp_maxpersistidle = TCPTV_KEEP_IDLE;   /* max idle time in persist */
    tcp_maxidle=0;
    tcp_ttl=0;
    m_disable_new_flow=0;
    m_pad=0;
    tcp_iss = rand();   /* wrong, but better than a constant */
    m_tcpstat.Clear();
    m_tick=0;
    tcp_now=0;
    m_cb = NULL;
    m_template_rw = NULL;
    m_template_ro = NULL;
    memset(&tcp_saveti,0,sizeof(tcp_saveti));

    RC_HTW_t tw_res;
    tw_res = m_timer_w.Create(1024,1);

    if (tw_res != RC_HTW_OK ){
        CHTimerWheelErrorStr err(tw_res);
        printf("Timer wheel configuration error,please look into the manual for details  \n");
        printf("ERROR  %-30s  - %s \n",err.get_str(),err.get_help_str());
        return(false);
    }
#ifndef TREX_SIM
    m_timer_w.set_level1_cnt_div(TCP_TIMER_W_DIV);
#endif

    if (!m_ft.Create(size,is_client)){
        printf("ERROR  can't create flow table \n");
        return(false);
    }
    return(true);
}



void CTcpPerThreadCtx::Delete(){
    m_timer_w.Delete();
    m_ft.Delete();
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
void tcp_template(struct tcpcb *tp){
    /* TCPIPV6*/

    const uint8_t default_ipv4_header[] = {
        0x00,0x00,0x00,0x01,0x0,0x0,  // Ethr
        0x00,0x00,0x00,0x02,0x0,0x0,
        0x08,00,


        0x45,0x00,0x00,0x00,          //Ipv4
        0x00,0x00,0x40,0x00,
        0x7f,0x06,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,

        0x00, 0x00, 0x00, 0x00, // src, dst ports  //TCP
        0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, // seq num, ack num
        0x50, 0x00, 0x00, 0x00, // Header size, flags, window size
        0x00, 0x00, 0x00, 0x00 // checksum ,urgent pointer
    };


    const uint8_t default_ipv6_header[] = {
        0x00,0x00,0x00,0x01,0x0,0x0,  // Ethr
        0x00,0x00,0x00,0x02,0x0,0x0,
        0x86,0xdd,


        0x60,0x00,0x00,0x00,          //Ipv6
        0x00,0x00,0x06,0x40,

        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, // src IP

        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, // dst IP

        0x00, 0x00, 0x00, 0x00, // src, dst ports  //TCP
        0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, // seq num, ack num
        0x50, 0x00, 0x00, 0x00, // Header size, flags, window size
        0x00, 0x00, 0x00, 0x00 // checksum ,urgent pointer
    };

    if (!tp->is_ipv6) {
        uint8_t vlan_offset=0;
        if (tp->vlan){
            vlan_offset=4;
        }

        tp->offset_ip  = 14+vlan_offset;
        tp->offset_tcp = tp->offset_ip + 20;
        tp->is_ipv6    = 0;

        uint8_t *p=tp->template_pkt;
        if (vlan_offset==0){
            memcpy(p,default_ipv4_header,sizeof(default_ipv4_header) );
        }else{
            memcpy(p,default_ipv4_header,sizeof(12));
            const uint8_t next_vlan[2]={0x81,00};
            memcpy(p+12,next_vlan,2);
            VLANHeader vlan_head;
            vlan_head.setVlanTag(tp->vlan);
            vlan_head.setNextProtocolFromHostOrder(0x0800);
            memcpy(p+14,vlan_head.getPointer(),4);
            memcpy(p+18,default_ipv4_header+14,sizeof(default_ipv4_header)-14);
        }
        /* set default value */
        IPHeader *lpIpv4=(IPHeader *)(p+tp->offset_ip);
        lpIpv4->setTotalLength(20); /* important for PH calculation */
        lpIpv4->setDestIp(tp->dst_ipv4);
        lpIpv4->setSourceIp(tp->src_ipv4);
        lpIpv4->ClearCheckSum();
        TCPHeader *lpTCP=(TCPHeader *)(p+tp->offset_tcp);
        lpTCP->setSourcePort(tp->src_port);
        lpTCP->setDestPort(tp->dst_port);

        if (tp->m_offload_flags & TCP_OFFLOAD_TX_CHKSUM){
            tp->l4_pseudo_checksum = rte_ipv4_phdr_cksum((struct ipv4_hdr *)lpIpv4,(PKT_TX_IPV4 |PKT_TX_IP_CKSUM|PKT_TX_TCP_CKSUM));
        }else{
            tp->l4_pseudo_checksum=0;
        }
    }else{
        uint8_t vlan_offset=0;
        if (tp->vlan){
            vlan_offset=4;
        }

        tp->offset_ip  = 14+vlan_offset;
        tp->offset_tcp = tp->offset_ip + IPV6_HDR_LEN;
        tp->is_ipv6    = 1;

        uint8_t *p=tp->template_pkt;
        if (vlan_offset==0){
            memcpy(p,default_ipv6_header,sizeof(default_ipv6_header) );
        }else{
            memcpy(p,default_ipv6_header,sizeof(12));
            const uint8_t next_vlan[2]={0x81,00};
            memcpy(p+12,next_vlan,2);
            VLANHeader vlan_head;
            vlan_head.setVlanTag(tp->vlan);
            vlan_head.setNextProtocolFromHostOrder(0x86dd);
            memcpy(p+14,vlan_head.getPointer(),4);
            memcpy(p+18,default_ipv6_header+14,sizeof(default_ipv6_header)-14);
        }
        /* set default value */
        IPv6Header *ipv6=(IPv6Header *)(p+tp->offset_ip);
        ipv6->updateLSBIpv6Dst(tp->dst_ipv4);
        ipv6->updateLSBIpv6Src(tp->src_ipv4);
        ipv6->setPayloadLen(0);  /* important for PH calculation */
        TCPHeader *lpTCP=(TCPHeader *)(p+tp->offset_tcp);
        lpTCP->setSourcePort(tp->src_port);
        lpTCP->setDestPort(tp->dst_port);

        if (tp->m_offload_flags & TCP_OFFLOAD_TX_CHKSUM){
            tp->l4_pseudo_checksum = rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv6,(PKT_TX_IPV6 | PKT_TX_TCP_CKSUM));
        }else{
            tp->l4_pseudo_checksum=0;
        }
    }
}



/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb * tcp_drop_now(CTcpPerThreadCtx * ctx,
                            struct tcpcb *tp, 
                            int res){
    struct tcp_socket *so = &tp->m_socket;

    if (TCPS_HAVERCVDSYN(tp->t_state)) {
        tp->t_state = TCPS_CLOSED;
        (void) tcp_output(ctx,tp);
        INC_STAT(ctx,tcps_drops);
    } else{
        INC_STAT(ctx,tcps_conndrops);
    }
    if (res == ETIMEDOUT && tp->t_softerror){
        res = tp->t_softerror;
    }
    so->so_error = res;
    return (tcp_close(ctx,tp));
}


/*
 * Close a TCP control block:
 *  discard all space held by the tcp
 *  discard internet protocol block
 *  wake up any sleepers
 */
struct tcpcb *  
tcp_close(CTcpPerThreadCtx * ctx,
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
    INC_STAT(ctx,tcps_closed);
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




