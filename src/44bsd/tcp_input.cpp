/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *  @(#)tcp_input.c 8.12 (Berkeley) 5/24/95
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"
#include "tcp_socket.h"
#include "utl_mbuf.h"
#include "astf/astf_db.h"
#include "astf/astf_template_db.h"


#define TCP_PAWS_IDLE   (24 * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)   ((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)  ((int)((a)-(b)) >= 0)

int16_t tcp_du32(uint32_t a,
                 uint32_t b){
    int32_t d=a-b;
    if (d>0x7FFF) {
        d=0x7FFF;
    }else{
        if (d<0) {
            d=0;
        }
    }
    return((int16_t)d);
}


bool CTcpReassBlock::operator==(CTcpReassBlock& rhs)const{
    return  ((m_seq == rhs.m_seq) && (m_flags==rhs.m_flags) && (m_len ==rhs.m_len));
}


void CTcpReassBlock::Dump(FILE *fd){
    fprintf(fd,"seq : %lu(%lu)(%s) \n",(ulong)m_seq,(ulong)m_len,m_flags?"FIN":"");
}

static inline void tcp_pktmbuf_adj(struct rte_mbuf * & m, uint16_t len){
    assert(m->pkt_len>=len);
    assert(utl_rte_pktmbuf_adj_ex(m, len)!=NULL);
}

static inline void tcp_pktmbuf_trim(struct rte_mbuf *m, uint16_t len){
    assert(m->pkt_len>=len);
    assert(utl_rte_pktmbuf_trim_ex(m, len)==0);
}

/*
 * Drop TCP, IP headers and TCP options. go to L7 
   remove pad if exists
 */
static inline void tcp_pktmbuf_fix_mbuf(struct rte_mbuf *m, 
                                        uint16_t adj_len,
                                        uint16_t l7_len){

    /*
     * Drop TCP, IP headers and TCP options. go to L7 
     */
    tcp_pktmbuf_adj(m, adj_len);

    /*
     * remove padding if exists. 
     */
    if (unlikely(m->pkt_len > l7_len)) {
        uint32_t pad_size = m->pkt_len-(uint32_t)l7_len;
        assert(pad_size<0xffff);
        tcp_pktmbuf_trim(m, (uint16_t)pad_size);
    }

     #ifdef _DEBUG
         assert(m->pkt_len == l7_len);
         if (l7_len>0) {
             assert(utl_rte_pktmbuf_verify(m)==0);
         }
     #endif
}

bool CTcpReass::expect(vec_tcp_reas_t & lpkts,FILE * fd){
    int i; 
    if (lpkts.size() !=m_active_blocks) {
        fprintf(fd,"ERROR not the same size");
        return(false);
    }
    for (i=0;i<m_active_blocks;i++) {
        if ( !(m_blocks[i] == lpkts[i] )){
            fprintf(fd,"ERROR object are not the same \n");
            fprintf(fd,"Exists :\n");
            m_blocks[i].Dump(fd);
            fprintf(fd,"Expected :\n");
            lpkts[i].Dump(fd);
        }
    }
    return(true);
}

void CTcpReass::Dump(FILE *fd){
    int i; 
    fprintf(fd,"active blocks : %d \n",m_active_blocks);
    for (i=0;i<m_active_blocks;i++) {
        m_blocks[i].Dump(fd);
    }
}


int CTcpReass::tcp_reass_no_data(CTcpPerThreadCtx * ctx,
                                 struct tcpcb *tp){
    int flags=0;

    if (TCPS_HAVERCVDSYN(tp->t_state) == 0){
        return (0);
    }

    if (m_blocks[0].m_seq != tp->rcv_nxt) {
        return(0);
    }

    tp->rcv_nxt += m_blocks[0].m_len;

    INC_STAT(ctx,tcps_rcvpack);
    INC_STAT_CNT(ctx,tcps_rcvbyte, m_blocks[0].m_len);

    flags = (m_blocks[0].m_flags==1) ? TH_FIN :0;

    sbappend_bytes(&tp->m_socket,
                   &tp->m_socket.so_rcv ,
                   m_blocks[0].m_len);

    /* remove the first block */
    int i;
    for (i=0; i<m_active_blocks-1; i++) {
        m_blocks[i]=m_blocks[i+1];
    }

    m_active_blocks-=1;

    sorwakeup(&tp->m_socket);
    return (flags);
}


int CTcpReass::tcp_reass(CTcpPerThreadCtx * ctx,
                         struct tcpcb *tp, 
                         struct tcpiphdr *ti, 
                         struct rte_mbuf *m){
    pre_tcp_reass(ctx,tp,ti,m);
    return (tcp_reass_no_data(ctx,tp));
}


int CTcpReass::pre_tcp_reass(CTcpPerThreadCtx * ctx,
                         struct tcpcb *tp, 
                         struct tcpiphdr *ti, 
                         struct rte_mbuf *m){

    assert(ti);
    if (m) {
        rte_pktmbuf_free(m);
    }

    INC_STAT(ctx,tcps_rcvoopack);
    INC_STAT_CNT(ctx,tcps_rcvoobyte,ti->ti_len);

    if (m_active_blocks==0) {
        /* first one - just add it to the list */
        CTcpReassBlock * lpb=&m_blocks[0];
        lpb->m_seq = ti->ti_seq;
        lpb->m_len = (uint32_t)ti->ti_len;
        lpb->m_flags = (ti->ti_flags & TH_FIN) ?1:0;
        m_active_blocks=1;
        return(0);
    }
    /* we have at least 1 block */

    int ci=0;
    int li=0;
    CTcpReassBlock  tblocks[MAX_TCP_REASS_BLOCKS+1];
    bool save_ptr=false;


    CTcpReassBlock cur;

    while (true) {

        if (save_ptr==false) {
            if (li==(m_active_blocks)) {
                cur.m_seq = ti->ti_seq;
                cur.m_len = ti->ti_len;
                cur.m_flags = (ti->ti_flags & TH_FIN) ?1:0;
                save_ptr=true;
            }else{
                if (SEQ_GT(m_blocks[li].m_seq,ti->ti_seq)) {
                  cur.m_seq = ti->ti_seq;
                  cur.m_len = ti->ti_len;
                  cur.m_flags = (ti->ti_flags & TH_FIN) ?1:0;
                  save_ptr=true;
                }else{
                    cur=m_blocks[li];
                    li++;
                }
            }
        }else{
            cur=m_blocks[li];
            li++;
        }

        if (ci==0) {
            tblocks[ci]=cur;
            ci++;
        }else{
            int32 q = tblocks[ci-1].m_seq + tblocks[ci-1].m_len - cur.m_seq;
            if (q<0) {
                tblocks[ci]=cur;
                ci++;
            }else{

                if (q>0) {
                    if (q>=cur.m_len) {
                        /* all packet is duplicate */
                        INC_STAT(ctx,tcps_rcvduppack);
                        INC_STAT_CNT(ctx,tcps_rcvdupbyte,cur.m_len);
                    }else{
                        /* part of it duplicate */
                        INC_STAT(ctx,tcps_rcvpartduppack);
                        INC_STAT_CNT(ctx,tcps_rcvpartdupbyte,(cur.m_len-q));
                    }
                }
                if (cur.m_len>q) {
                    tblocks[ci-1].m_len+=(cur.m_len-q);
                    tblocks[ci-1].m_flags |=cur.m_flags; /* or the overlay FIN */
                }
            }

            if ( (save_ptr==true) && (li==m_active_blocks) ) {
                break;
            }
        }
    }

    if (ci>MAX_TCP_REASS_BLOCKS) {
        /* drop last segment */
        INC_STAT(ctx,tcps_rcvoopackdrop);
        INC_STAT_CNT(ctx,tcps_rcvoobytesdrop,tblocks[MAX_TCP_REASS_BLOCKS].m_len);
    }
    
    m_active_blocks = bsd_umin(ci,MAX_TCP_REASS_BLOCKS);
    for (li=0; li<m_active_blocks; li++ ) {
        m_blocks[li]= tblocks[li];
    }

    return(0);
}



int tcp_reass_no_data(CTcpPerThreadCtx * ctx,
                      struct tcpcb *tp){

    if ( TCPS_HAVERCVDSYN(tp->t_state) == 0 )
        return (0);

    if ( tcp_reass_is_exists(tp)==false ) {
        return (0);
    }

    int res=tp->m_tpc_reass->tcp_reass_no_data(ctx,tp);
    if (tp->m_tpc_reass->get_active_blocks()==0) {
        tcp_reass_free(ctx,tp);
    }
    return (res);
}



int tcp_reass(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp, 
              struct tcpiphdr *ti, 
              struct rte_mbuf *m){

    if (tcp_reass_is_exists(tp)==false ){
        /* in case of FIN in order with no data there is nothing to do */
        if ( (ti->ti_flags & TH_FIN) &&
             (ti->ti_seq == tp->rcv_nxt) && 
             (tp->t_state > TCPS_ESTABLISHED) && 
             (ti->ti_len==0) ) {
            INC_STAT(ctx,tcps_rcvpack);
            if (m) {
                rte_pktmbuf_free(m);
            }
            return(ti->ti_flags & TH_FIN);
        }
        tcp_reass_alloc(ctx,tp);
    }

    int res=tp->m_tpc_reass->tcp_reass(ctx,tp,ti,m);
    if (tp->m_tpc_reass->get_active_blocks()==0) {
        tcp_reass_free(ctx,tp);
    }
    return (res);
}


/*
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.  The macro form does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 */
inline void TCP_REASS(CTcpPerThreadCtx * ctx,
                      struct tcpcb *tp,
                      struct tcpiphdr *ti,
                      struct rte_mbuf *m,
                      struct tcp_socket *so,
                      int &tiflags) { 
    if (ti->ti_seq == tp->rcv_nxt && 
       (tcp_reass_is_exists(tp)==false) && 
        tp->t_state == TCPS_ESTABLISHED) { 
        if (tiflags & TH_PUSH) 
            tp->t_flags |= TF_ACKNOW; 
        else 
            tp->t_flags |= TF_DELACK; 
        tp->rcv_nxt += ti->ti_len; 
        tiflags = ti->ti_flags & TH_FIN; 
        INC_STAT(ctx,tcps_rcvpack);
        INC_STAT_CNT(ctx,tcps_rcvbyte,ti->ti_len);
        sbappend(so,
                 &so->so_rcv, m,ti->ti_len); 
        sorwakeup(so); 
    } else { 
        tiflags = tcp_reass(ctx,tp, ti, m); 
        tp->t_flags |= TF_ACKNOW; 
    }
}



void tcp_dooptions(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp, 
              uint8_t *cp, 
              int cnt, 
              struct tcpiphdr *ti, 
              int *ts_present,
              uint32_t *ts_val, 
              uint32_t * ts_ecr){
    u_short mss;
    int opt, optlen;

    for (; cnt > 0; cnt -= optlen, cp += optlen) {
        opt = cp[0];
        if (opt == TCPOPT_EOL)
            break;
        if (opt == TCPOPT_NOP)
            optlen = 1;
        else {
            optlen = cp[1];
            if (optlen <= 0)
                break;
        }
        switch (opt) {

        default:
            continue;

        case TCPOPT_MAXSEG:
            if (optlen != TCPOLEN_MAXSEG)
                continue;
            if (!(ti->ti_flags & TH_SYN))
                continue;
            memcpy((char *) &mss,(char *) cp + 2, sizeof(mss));
            mss=bsd_ntohs(mss);
            (void) tcp_mss(ctx,tp, mss);    /* sets t_maxseg */
            break;

        case TCPOPT_WINDOW:
            if (optlen != TCPOLEN_WINDOW)
                continue;
            if (!(ti->ti_flags & TH_SYN))
                continue;
            tp->t_flags |= TF_RCVD_SCALE;
            tp->requested_s_scale = bsd_umin(cp[2], TCP_MAX_WINSHIFT);
            break;

        case TCPOPT_TIMESTAMP:
            if (optlen != TCPOLEN_TIMESTAMP)
                continue;
            *ts_present = 1;
            memcpy((char *) ts_val,(char *)cp + 2, sizeof(*ts_val));
            *ts_val=bsd_ntohl(*ts_val);
            memcpy((char *) ts_ecr,(char *)cp + 6, sizeof(*ts_ecr));
            *ts_ecr=bsd_ntohl(*ts_ecr);

            /* 
             * A timestamp received in a SYN makes
             * it ok to send timestamp requests and replies.
             */
            if (ti->ti_flags & TH_SYN) {
                tp->t_flags |= TF_RCVD_TSTMP;
                tp->ts_recent = *ts_val;
                tp->ts_recent_age = ctx->tcp_now;
            }
            break;
        }
    }
}

#if SUPPORT_OF_URG

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(struct tcp_socket *so, 
                  struct tcpiphdr *ti, 
                  struct rte_mbuf *m){
    /* not supported */
    assert(0);
    int cnt = ti->ti_urp - 1;
    
    while (cnt >= 0) {
        if (m->m_len > cnt) {
            char *cp = mtod(m, caddr_t) + cnt;
            struct tcpcb *tp = sototcpcb(so);

            tp->t_iobc = *cp;
            tp->t_oobflags |= TCPOOB_HAVEDATA;
            bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
            m->m_len--;
            return;
        }
        cnt -= m->m_len;
        m = m->m_next;
        if (m == 0)
            break;
    }
    panic("tcp_pulloutofband");
}

#endif


#ifdef TCPDEBUG

struct tcpcb *debug_flow; 

#endif


/* assuming we found the flow */
int tcp_flow_input(CTcpPerThreadCtx * ctx,
                    struct tcpcb *tp, 
                    struct rte_mbuf *m,
                    TCPHeader *tcp,
                    int offset_l7,
                    int total_l7_len
                    ){
    struct tcpiphdr *ti;
    struct tcp_socket *so;
    tcpiphdr pheader;
    ti=&pheader;
    int tiflags;
    int iss = 0;
    uint32_t tiwin, ts_val, ts_ecr;
    int ts_present = 0;
    short ostate=0;
    int optlen;
    int todrop, acked, ourfinisacked, needoutput = 0;
    int off;

    uint8_t *optp;  // TCP option is exist  

#ifdef TCPDEBUG
  #if 0
    if ((tp == debug_flow) && (tp->t_state ==TCPS_FIN_WAIT_1 )) {
        printf(" break !! \n");
    }
  #endif
#endif

    if ( tcp->getHeaderLength()==TCP_HEADER_LEN ){
        optp=NULL;
        optlen=0;
    }else{
        optp=(uint8_t *)tcp->getOptionPtr();
        optlen=(tcp->getHeaderLength()-TCP_HEADER_LEN);
    }
    off=offset_l7;

    /* copy from packet to host */
    ti->ti_seq = tcp->getSeqNumber();
    ti->ti_ack = tcp->getAckNumber();
    ti->ti_win = tcp->getWindowSize();
    ti->ti_urp = tcp->getUrgentOffset();
    tiflags = tcp->getFlags();
    ti->ti_flags = tiflags;
    ti->ti_len   = total_l7_len; /* L7 len */


    /* Unscale the window into a 32-bit value. */
    if ((tiflags & TH_SYN) == 0)
        tiwin = ti->ti_win << tp->snd_scale;
    else
        tiwin = ti->ti_win;

    so = &tp->m_socket;

    ostate = tp->t_state;

    /* sanity check */
    if (off + ti->ti_len > m->pkt_len || (optlen<0)){
        /* somthing wrong here drop the packet */
        goto drop;
    }


    if (tp->t_state == TCPS_LISTEN) {

        if ((tiflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
                /*
                 * Note: dropwithreset makes sure we don't
                 * send a reset in response to a RST.
                 */
                if (tiflags & TH_ACK) {
                    INC_STAT(ctx,tcps_badsyn);
                    goto dropwithreset;
                }
                goto drop;
            }


        /* Compute proper scaling value from buffer space
         */
        while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
           TCP_MAXWIN << tp->request_r_scale < so->so_rcv.sb_hiwat)
            tp->request_r_scale++;
    }

    /*
     * Segment received on connection.
     * Reset idle time and keep-alive timer.
     */
    tp->t_idle = 0;
    tp->t_timer[TCPT_KEEP] = ctx->tcp_keepidle;

    /*
     * Process options if not in LISTEN state,
     * else do it below (after getting remote address).
     */
    if (optp && tp->t_state != TCPS_LISTEN){
        tcp_dooptions(ctx,tp, optp, optlen, ti,
            &ts_present, &ts_val, &ts_ecr);
    }

    /* 
     * Header prediction: check for the two common cases
     * of a uni-directional data xfer.  If the packet has
     * no control flags, is in-sequence, the window didn't
     * change and we're not retransmitting, it's a
     * candidate.  If the length is zero and the ack moved
     * forward, we're the sender side of the xfer.  Just
     * free the data acked & wake any higher level process
     * that was blocked waiting for space.  If the length
     * is non-zero and the ack didn't move, we're the
     * receiver side.  If we're getting packets in-order
     * (the reassembly queue is empty), add the data to
     * the socket buffer and note that we need a delayed ack.
     */
    if (tp->t_state == TCPS_ESTABLISHED &&
        (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
        (!ts_present || TSTMP_GEQ(ts_val, tp->ts_recent)) &&
        ti->ti_seq == tp->rcv_nxt &&
        tiwin && tiwin == tp->snd_wnd &&
        tp->snd_nxt == tp->snd_max) {

        /* 
         * If last ACK falls within this segment's sequence numbers,
         *  record the timestamp.
         */
        if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
           SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len)) {
            tp->ts_recent_age = ctx->tcp_now;
            tp->ts_recent = ts_val;
        }

        if (ti->ti_len == 0) {
            if (SEQ_GT(ti->ti_ack, tp->snd_una) &&
                SEQ_LEQ(ti->ti_ack, tp->snd_max) &&
                tp->snd_cwnd >= tp->snd_wnd) {
                /*
                 * this is a pure ack for outstanding data.
                 */
                INC_STAT(ctx,tcps_predack)
                if (ts_present)
                    tcp_xmit_timer(ctx,tp, tcp_du32(ctx->tcp_now,ts_ecr)+1 );
                else if (tp->t_rtt &&
                        SEQ_GT(ti->ti_ack, tp->t_rtseq))
                    tcp_xmit_timer(ctx,tp, tp->t_rtt);
                acked = ti->ti_ack - tp->snd_una;
                INC_STAT(ctx,tcps_rcvackpack);
                INC_STAT_CNT(ctx,tcps_rcvackbyte,acked);
                so->so_snd.sbdrop(so,acked);

                tp->snd_una = ti->ti_ack;
                rte_pktmbuf_free(m);

                /*
                 * If all outstanding data are acked, stop
                 * retransmit timer, otherwise restart timer
                 * using current (possibly backed-off) value.
                 * If process is waiting for space,
                 * wakeup/selwakeup/signal.  If data
                 * are ready to send, let tcp_output
                 * decide between more output or persist.
                 */
                if (tp->snd_una == tp->snd_max)
                    tp->t_timer[TCPT_REXMT] = 0;
                else if (tp->t_timer[TCPT_PERSIST] == 0)
                    tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

                if (so->so_snd.sb_flags & SB_NOTIFY){
                    sowwakeup(so);
                }
                if (so->so_snd.sb_cc)
                    (void) tcp_output(ctx,tp);
                return 0;
            }
        } else if ( (ti->ti_ack == tp->snd_una) &&
                    (tcp_reass_is_exists(tp)==false) &&
                    (ti->ti_len <= sbspace(&so->so_rcv))  ) {
            /*
             * this is a pure, in-sequence data packet
             * with nothing on the reassembly queue and
             * we have enough buffer space to take it.
             */
            INC_STAT(ctx,tcps_preddat);
            tp->rcv_nxt += ti->ti_len;
            INC_STAT(ctx,tcps_rcvpack);
            INC_STAT_CNT(ctx,tcps_rcvbyte, ti->ti_len);
            /*
             * Drop TCP, IP headers and TCP options then add data
             * to socket buffer. remove padding 
             */
            tcp_pktmbuf_fix_mbuf(m, off,ti->ti_len);

            sbappend(so,
                     &so->so_rcv, m,ti->ti_len);
            sorwakeup(so);

            /*
             * If this is a short packet, then ACK now - with Nagel
             *  congestion avoidance sender won't send more until
             *  he gets an ACK.
             */
            if (tiflags & TH_PUSH){
                tp->t_flags |= TF_ACKNOW;
                tcp_output(ctx,tp);
            }else{
                tp->t_flags |= TF_DELACK;
            }

            return 0;
        }
    }

    /*
     * Drop TCP, IP headers and TCP options. go to L7 
       remove padding
     */
    tcp_pktmbuf_fix_mbuf(m, off,ti->ti_len);

    /*
     * Calculate amount of space in receive window,
     * and then do TCP input processing.
     * Receive window is amount of space in rcv queue,
     * but not less than advertised window.
     */
    { 
        int win;
        win = sbspace(&so->so_rcv);
        if (win < 0)
          win = 0;
        tp->rcv_wnd = bsd_imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
    }

    switch (tp->t_state) {

    /*
     * If the state is LISTEN then ignore segment if it contains an RST.
     * If the segment contains an ACK then it is bad and send a RST.
     * If it does not contain a SYN then it is not interesting; drop it.
     * Don't bother responding if the destination was a broadcast.
     * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
     * tp->iss, and send a segment:
     *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
     * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
     * Fill in remote peer address fields if not previously specified.
     * Enter SYN_RECEIVED state, and process any other fields of this
     * segment in this state.
     */
    case TCPS_LISTEN: {

        if (optp)
            tcp_dooptions(ctx,tp, optp, optlen, ti,
                &ts_present, &ts_val, &ts_ecr);
        if (iss)
            tp->iss = iss;
        else
            tp->iss = ctx->tcp_iss;
        ctx->tcp_iss += TCP_ISSINCR/4;
        tp->irs = ti->ti_seq;
        tcp_sendseqinit(tp);
        tcp_rcvseqinit(tp);
        tp->t_flags |= TF_ACKNOW;
        tp->t_state = TCPS_SYN_RECEIVED;
        tp->t_timer[TCPT_KEEP] = ctx->tcp_keepinit;
        INC_STAT(ctx,tcps_accepts);
        goto trimthenstep6;
        }

    /*
     * If the state is SYN_SENT:
     *  if seg contains an ACK, but not for our SYN, drop the input.
     *  if seg contains a RST, then drop the connection.
     *  if seg does not contain SYN, then drop it.
     * Otherwise this is an acceptable SYN segment
     *  initialize tp->rcv_nxt and tp->irs
     *  if seg contains ack then advance tp->snd_una
     *  if SYN has been acked change to ESTABLISHED else SYN_RCVD state
     *  arrange for segment to be acked (eventually)
     *  continue processing rest of data/controls, beginning with URG
     */
    case TCPS_SYN_SENT:
        if ((tiflags & TH_ACK) &&
            (SEQ_LEQ(ti->ti_ack, tp->iss) ||
             SEQ_GT(ti->ti_ack, tp->snd_max)))
            goto dropwithreset;
        if (tiflags & TH_RST) {
            if (tiflags & TH_ACK){
                tp = tcp_drop_now(ctx,tp, TCP_US_ECONNREFUSED);
            }
            goto drop;
        }
        if ((tiflags & TH_SYN) == 0)
            goto drop;
        if (tiflags & TH_ACK) {
            tp->snd_una = ti->ti_ack;
            if (SEQ_LT(tp->snd_nxt, tp->snd_una))
                tp->snd_nxt = tp->snd_una;
        }
        tp->t_timer[TCPT_REXMT] = 0;
        tp->irs = ti->ti_seq;
        tcp_rcvseqinit(tp);
        tp->t_flags |= TF_ACKNOW;
        if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
            INC_STAT(ctx,tcps_connects);
            soisconnected(so);
            tp->t_state = TCPS_ESTABLISHED;
            /* Do window scaling on this connection? */
            if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
                (TF_RCVD_SCALE|TF_REQ_SCALE)) {
                tp->snd_scale = tp->requested_s_scale;
                tp->rcv_scale = tp->request_r_scale;
            }
            tcp_reass_no_data(ctx,tp);
            /*
             * if we didn't have to retransmit the SYN,
             * use its rtt as our initial srtt & rtt var.
             */
            if (tp->t_rtt)
                tcp_xmit_timer(ctx,tp, tp->t_rtt);
        } else {
            tp->t_state = TCPS_SYN_RECEIVED;
        }

trimthenstep6:
        /*
         * Advance ti->ti_seq to correspond to first data byte.
         * If data, trim to stay within window,
         * dropping FIN if necessary.
         */
        ti->ti_seq++;
        if (ti->ti_len > tp->rcv_wnd) {
            todrop = ti->ti_len - tp->rcv_wnd;
            tcp_pktmbuf_trim(m, todrop);
            ti->ti_len = tp->rcv_wnd;
            tiflags &= ~TH_FIN;
            INC_STAT(ctx,tcps_rcvpackafterwin);
            INC_STAT_CNT(ctx,tcps_rcvbyteafterwin,todrop);
        }
        tp->snd_wl1 = ti->ti_seq - 1;
        tp->rcv_up = ti->ti_seq;
        goto step6;
    }

    /*
     * States other than LISTEN or SYN_SENT.
     * First check timestamp, if present.
     * Then check that at least some bytes of segment are within 
     * receive window.  If segment begins before rcv_nxt,
     * drop leading data (and SYN); if nothing left, just ack.
     * 
     * RFC 1323 PAWS: If we have a timestamp reply on this segment
     * and it's less than ts_recent, drop it.
     */
    if (ts_present && ((tiflags & TH_RST) == 0) && tp->ts_recent &&
        TSTMP_LT(ts_val, tp->ts_recent)) {

        /* Check to see if ts_recent is over 24 days old.  */
        if ((int)(ctx->tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
            /*
             * Invalidate ts_recent.  If this segment updates
             * ts_recent, the age will be reset later and ts_recent
             * will get a valid value.  If it does not, setting
             * ts_recent to zero will at least satisfy the
             * requirement that zero be placed in the timestamp
             * echo reply when ts_recent isn't valid.  The
             * age isn't reset until we get a valid ts_recent
             * because we don't want out-of-order segments to be
             * dropped when ts_recent is old.
             */
            tp->ts_recent = 0;
        } else {
            INC_STAT(ctx,tcps_rcvduppack);
            INC_STAT_CNT(ctx,tcps_rcvdupbyte,ti->ti_len);
            INC_STAT(ctx,tcps_pawsdrop);
            goto dropafterack;
        }
    }

    todrop = tp->rcv_nxt - ti->ti_seq;
    if (todrop > 0) {
        if (tiflags & TH_SYN) {
            tiflags &= ~TH_SYN;
            ti->ti_seq++;
            if (ti->ti_urp > 1) 
                ti->ti_urp--;
            else
                tiflags &= ~TH_URG;
            todrop--;
        }
        if (todrop >= ti->ti_len) {
            INC_STAT(ctx,tcps_rcvduppack);
            INC_STAT_CNT(ctx,tcps_rcvdupbyte,ti->ti_len);
            /*
             * If segment is just one to the left of the window,
             * check two special cases:
             * 1. Don't toss RST in response to 4.2-style keepalive.
             * 2. If the only thing to drop is a FIN, we can drop
             *    it, but check the ACK or we will get into FIN
             *    wars if our FINs crossed (both CLOSING).
             * In either case, send ACK to resynchronize,
             * but keep on processing for RST or ACK.
             */
            if ((tiflags & TH_FIN && todrop == ti->ti_len + 1)
               ) {
                todrop = ti->ti_len;
                tiflags &= ~TH_FIN;
            } else {
                /*
                 * Handle the case when a bound socket connects
                 * to itself. Allow packets with a SYN and
                 * an ACK to continue with the processing.
                 */
                if (todrop != 0 || (tiflags & TH_ACK) == 0)
                    goto dropafterack;
            }
            tp->t_flags |= TF_ACKNOW;
        } else {
            INC_STAT(ctx,tcps_rcvpartduppack);
            INC_STAT_CNT(ctx,tcps_rcvpartdupbyte, todrop);
        }
        /* after this operation, it could be a mbuf with len==0 in  case of mbuf_len==todrop
           still need to free it */
        tcp_pktmbuf_adj(m, todrop);

        ti->ti_seq += todrop;
        ti->ti_len -= todrop;
        if (ti->ti_urp > todrop)
            ti->ti_urp -= todrop;
        else {
            tiflags &= ~TH_URG;
            ti->ti_urp = 0;
        }
    }

    /*
     * If new data are received on a connection after the
     * user processes are gone, then RST the other end.
     */
    if (tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) {
        tp = tcp_close(ctx,tp);
        INC_STAT(ctx,tcps_rcvafterclose);
        goto dropwithreset;
    }

    /*
     * If segment ends after window, drop trailing data
     * (and PUSH and FIN); if nothing left, just ACK.
     */
    todrop = (ti->ti_seq+ti->ti_len) - (tp->rcv_nxt+tp->rcv_wnd);
    if (todrop > 0) {
        INC_STAT(ctx,tcps_rcvpackafterwin);
        if (todrop >= ti->ti_len) {
            INC_STAT_CNT(ctx,tcps_rcvbyteafterwin,ti->ti_len);
            /*
             * If a new connection request is received
             * while in TIME_WAIT, drop the old connection
             * and start over if the sequence numbers
             * are above the previous ones.
             */
            if (tiflags & TH_SYN &&
                tp->t_state == TCPS_TIME_WAIT &&
                SEQ_GT(ti->ti_seq, tp->rcv_nxt)) {
                iss = tp->snd_nxt + TCP_ISSINCR;
                tp = tcp_close(ctx,tp);
                goto findpcb;
            }
            /*
             * If window is closed can only take segments at
             * window edge, and have to drop data and PUSH from
             * incoming segments.  Continue processing, but
             * remember to ack.  Otherwise, drop segment
             * and ack.
             */
            if (tp->rcv_wnd == 0 && ti->ti_seq == tp->rcv_nxt) {
                tp->t_flags |= TF_ACKNOW;
                INC_STAT(ctx,tcps_rcvwinprobe);
            } else
                goto dropafterack;
        } else{
            INC_STAT_CNT(ctx,tcps_rcvbyteafterwin, todrop);
        }
        tcp_pktmbuf_trim(m, todrop);
        ti->ti_len -= todrop;
        tiflags &= ~(TH_PUSH|TH_FIN);
    }

    /*
     * If last ACK falls within this segment's sequence numbers,
     * record its timestamp.
     */
    if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
        SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len +
           ((tiflags & (TH_SYN|TH_FIN)) != 0))) {
        tp->ts_recent_age = ctx->tcp_now;
        tp->ts_recent = ts_val;
    }

    /*
     * If the RST bit is set examine the state:
     *    SYN_RECEIVED STATE:
     *  If passive open, return to LISTEN state.
     *  If active open, inform user that connection was refused.
     *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
     *  Inform user that connection was reset, and close tcb.
     *    CLOSING, LAST_ACK, TIME_WAIT STATES
     *  Close the tcb.
     */
    if ( tiflags&TH_RST) {
        switch (tp->t_state) {

        case TCPS_SYN_RECEIVED:
            so->so_error = ECONNREFUSED;
            goto close;

        case TCPS_ESTABLISHED:
        case TCPS_FIN_WAIT_1:
        case TCPS_FIN_WAIT_2:
        case TCPS_CLOSE_WAIT:
            so->so_error = ECONNRESET;
        close:
            tp->t_state = TCPS_CLOSED;
            INC_STAT(ctx,tcps_drops);
            tp = tcp_close(ctx,tp);
            goto drop;

        case TCPS_CLOSING:
        case TCPS_LAST_ACK:
        case TCPS_TIME_WAIT:
            tp = tcp_close(ctx,tp);
            goto drop;
        }
    }

    /*
     * If a SYN is in the window, then this is an
     * error and we send an RST and drop the connection.
     */
    if (tiflags & TH_SYN) {
        tp = tcp_drop_now(ctx,tp, TCP_US_ECONNRESET);
        goto dropwithreset;
    }

    /*
     * If the ACK bit is off we drop the segment and return.
     */
    if ((tiflags & TH_ACK) == 0)
        goto drop;

    /*
     * Ack processing.
     */
    switch (tp->t_state) {

    /*
     * In SYN_RECEIVED state if the ack ACKs our SYN then enter
     * ESTABLISHED state and continue processing, otherwise
     * send an RST.
     */
    case TCPS_SYN_RECEIVED:
        if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
            SEQ_GT(ti->ti_ack, tp->snd_max))
            goto dropwithreset;
        INC_STAT(ctx,tcps_connects);
        soisconnected(so);
        tp->t_state = TCPS_ESTABLISHED;
        /* Do window scaling? */
        if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
            (TF_RCVD_SCALE|TF_REQ_SCALE)) {
            tp->snd_scale = tp->requested_s_scale;
            tp->rcv_scale = tp->request_r_scale;
        }
        (void) tcp_reass_no_data(ctx,tp);
        tp->snd_wl1 = ti->ti_seq - 1;
        /* fall into ... */

    /*
     * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
     * ACKs.  If the ack is in the range
     *  tp->snd_una < ti->ti_ack <= tp->snd_max
     * then advance tp->snd_una to ti->ti_ack and drop
     * data from the retransmission queue.  If this ACK reflects
     * more up to date window information we update our window information.
     */
    case TCPS_ESTABLISHED:
    case TCPS_FIN_WAIT_1:
    case TCPS_FIN_WAIT_2:
    case TCPS_CLOSE_WAIT:
    case TCPS_CLOSING:
    case TCPS_LAST_ACK:
    case TCPS_TIME_WAIT:

        if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) {
            if (ti->ti_len == 0 && tiwin == tp->snd_wnd) {
                INC_STAT(ctx,tcps_rcvdupack);
                /*
                 * If we have outstanding data (other than
                 * a window probe), this is a completely
                 * duplicate ack (ie, window info didn't
                 * change), the ack is the biggest we've
                 * seen and we've seen exactly our rexmt
                 * threshhold of them, assume a packet
                 * has been dropped and retransmit it.
                 * Kludge snd_nxt & the congestion
                 * window so we send only this one
                 * packet.
                 *
                 * We know we're losing at the current
                 * window size so do congestion avoidance
                 * (set ssthresh to half the current window
                 * and pull our congestion window back to
                 * the new ssthresh).
                 *
                 * Dup acks mean that packets have left the
                 * network (they're now cached at the receiver) 
                 * so bump cwnd by the amount in the receiver
                 * to keep a constant cwnd packets in the
                 * network.
                 */
                if (tp->t_timer[TCPT_REXMT] == 0 ||
                    ti->ti_ack != tp->snd_una)
                    tp->t_dupacks = 0;
                else if (++tp->t_dupacks == ctx->tcprexmtthresh) {
                    tcp_seq onxt = tp->snd_nxt;
                    u_int win =
                        bsd_umin(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;

                    if (win < 2)
                        win = 2;
                    tp->snd_ssthresh = win * tp->t_maxseg;
                    tp->t_timer[TCPT_REXMT] = 0;
                    tp->t_rtt = 0;
                    tp->snd_nxt = ti->ti_ack;
                    tp->snd_cwnd = tp->t_maxseg;
                    (void) tcp_output(ctx,tp);
                    tp->snd_cwnd = tp->snd_ssthresh +
                           tp->t_maxseg * tp->t_dupacks;
                    if (SEQ_GT(onxt, tp->snd_nxt))
                        tp->snd_nxt = onxt;
                    goto drop;
                } else if (tp->t_dupacks > ctx->tcprexmtthresh) {
                    tp->snd_cwnd += tp->t_maxseg;
                    (void) tcp_output(ctx,tp);
                    goto drop;
                }
            } else
                tp->t_dupacks = 0;
            break;
        }
        /*
         * If the congestion window was inflated to account
         * for the other side's cached packets, retract it.
         */
        if (tp->t_dupacks > ctx->tcprexmtthresh &&
            tp->snd_cwnd > tp->snd_ssthresh)
            tp->snd_cwnd = tp->snd_ssthresh;
        tp->t_dupacks = 0;
        if (SEQ_GT(ti->ti_ack, tp->snd_max)) {
            INC_STAT(ctx,tcps_rcvacktoomuch);
            goto dropafterack;
        }
        acked = ti->ti_ack - tp->snd_una;
        INC_STAT(ctx,tcps_rcvackpack);

        /*
         * If we have a timestamp reply, update smoothed
         * round trip time.  If no timestamp is present but
         * transmit timer is running and timed sequence
         * number was acked, update smoothed round trip time.
         * Since we now have an rtt measurement, cancel the
         * timer backoff (cf., Phil Karn's retransmit alg.).
         * Recompute the initial retransmit timer.
         */
        if (ts_present){
            tcp_xmit_timer(ctx,tp, tcp_du32(ctx->tcp_now,ts_ecr)+1);
        }else if (tp->t_rtt && SEQ_GT(ti->ti_ack, tp->t_rtseq)){
            tcp_xmit_timer(ctx,tp,tp->t_rtt);
        }

        /*
         * If all outstanding data is acked, stop retransmit
         * timer and remember to restart (more output or persist).
         * If there is more data to be acked, restart retransmit
         * timer, using current (possibly backed-off) value.
         */
        if (ti->ti_ack == tp->snd_max) {
            tp->t_timer[TCPT_REXMT] = 0;
            needoutput = 1;
        } else if (tp->t_timer[TCPT_PERSIST] == 0)
            tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
        /*
         * When new data is acked, open the congestion window.
         * If the window gives us less than ssthresh packets
         * in flight, open exponentially (maxseg per packet).
         * Otherwise open linearly: maxseg per window
         * (maxseg * (maxseg / cwnd) per packet).
         */
        {
        uint32_t cw = tp->snd_cwnd;
        uint32_t incr = tp->t_maxseg;

        if (cw > tp->snd_ssthresh)
            incr = incr * incr / cw;
        tp->snd_cwnd = bsd_umin(cw + incr, TCP_MAXWIN<<tp->snd_scale);
        }
        if (acked > so->so_snd.sb_cc) {
            INC_STAT_CNT(ctx,tcps_rcvackbyte,so->so_snd.sb_cc);
            INC_STAT_CNT(ctx,tcps_rcvackbyte_of,(acked-so->so_snd.sb_cc));
            tp->snd_wnd -= so->so_snd.sb_cc;
            so->so_snd.sbdrop_all(so);
            ourfinisacked = 1;
        } else {
            INC_STAT_CNT(ctx,tcps_rcvackbyte,acked);
            so->so_snd.sbdrop(so,acked);
            tp->snd_wnd -= acked;
            ourfinisacked = 0;
        }
        if (so->so_snd.sb_flags & SB_NOTIFY)
            sowwakeup(so);
        tp->snd_una = ti->ti_ack;
        if (SEQ_LT(tp->snd_nxt, tp->snd_una))
            tp->snd_nxt = tp->snd_una;

        switch (tp->t_state) {

        /*
         * In FIN_WAIT_1 STATE in addition to the processing
         * for the ESTABLISHED state if our FIN is now acknowledged
         * then enter FIN_WAIT_2.
         */
        case TCPS_FIN_WAIT_1:
            if (ourfinisacked) {
                /*
                 * If we can't receive any more
                 * data, then closing user can proceed.
                 * Starting the timer is contrary to the
                 * specification, but if we don't get a FIN
                 * we'll hang forever.
                 */
                if (so->so_state & US_SS_CANTRCVMORE) {
                    soisdisconnected(so);
                    tp->t_timer[TCPT_2MSL] = ctx->tcp_maxidle;
                }
                tp->t_state = TCPS_FIN_WAIT_2;
            }
            break;

        /*
         * In CLOSING STATE in addition to the processing for
         * the ESTABLISHED state if the ACK acknowledges our FIN
         * then enter the TIME-WAIT state, otherwise ignore
         * the segment.
         */
        case TCPS_CLOSING:
            if (ourfinisacked) {
                tp->t_state = TCPS_TIME_WAIT;
                tcp_canceltimers(tp);
                tp->t_timer[TCPT_2MSL] = TCPTV_2MSL;
                soisdisconnected(so);
            }
            break;

        /*
         * In LAST_ACK, we may still be waiting for data to drain
         * and/or to be acked, as well as for the ack of our FIN.
         * If our FIN is now acknowledged, delete the TCB,
         * enter the closed state and return.
         */
        case TCPS_LAST_ACK:
            if (ourfinisacked) {
                tp = tcp_close(ctx,tp);
                goto drop;
            }
            break;

        /*
         * In TIME_WAIT state the only thing that should arrive
         * is a retransmission of the remote FIN.  Acknowledge
         * it and restart the finack timer.
         */
        case TCPS_TIME_WAIT:
            tp->t_timer[TCPT_2MSL] = TCPTV_2MSL;
            goto dropafterack;
        }
    }

step6:
    /*
     * Update window information.
     * Don't look at window if no ACK: TAC's send garbage on first SYN.
     */
    if ( (tiflags & TH_ACK) &&
         (SEQ_LT(tp->snd_wl1, ti->ti_seq) || ( 
           (tp->snd_wl1 == ti->ti_seq) &&
           ( SEQ_LT(tp->snd_wl2, ti->ti_ack) || ((tp->snd_wl2 == ti->ti_ack) && (tiwin > tp->snd_wnd)) )
          ))) {
        /* keep track of pure window updates */
        if ( (ti->ti_len == 0) &&
            (tp->snd_wl2 == ti->ti_ack) && (tiwin > tp->snd_wnd)){
            INC_STAT(ctx,tcps_rcvwinupd);
        }
        tp->snd_wnd = tiwin;
        tp->snd_wl1 = ti->ti_seq;
        tp->snd_wl2 = ti->ti_ack;
        if (tp->snd_wnd > tp->max_sndwnd)
            tp->max_sndwnd = tp->snd_wnd;
        needoutput = 1;
    }

#if SUPPORT_OF_URG
    /*
     * Process segments with URG.
     */
    if ((tiflags & TH_URG) && ti->ti_urp &&
        TCPS_HAVERCVDFIN(tp->t_state) == 0) {
        /*
         * This is a kludge, but if we receive and accept
         * random urgent pointers, we'll crash in
         * soreceive.  It's hard to imagine someone
         * actually wanting to send this much urgent data.
         */
        if (ti->ti_urp + so->so_rcv.sb_cc > sb_max) {
            ti->ti_urp = 0;         /* XXX */
            tiflags &= ~TH_URG;     /* XXX */
            goto dodata;            /* XXX */
        }
        /*
         * If this segment advances the known urgent pointer,
         * then mark the data stream.  This should not happen
         * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
         * a FIN has been received from the remote side. 
         * In these states we ignore the URG.
         *
         * According to RFC961 (Assigned Protocols),
         * the urgent pointer points to the last octet
         * of urgent data.  We continue, however,
         * to consider it to indicate the first octet
         * of data past the urgent section as the original 
         * spec states (in one of two places).
         */
        if (SEQ_GT(ti->ti_seq+ti->ti_urp, tp->rcv_up)) {
            tp->rcv_up = ti->ti_seq + ti->ti_urp;
            so->so_oobmark = so->so_rcv.sb_cc +
                (tp->rcv_up - tp->rcv_nxt) - 1;
            if (so->so_oobmark == 0)
                so->so_state |= SS_RCVATMARK;
            sohasoutofband(so);
            tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
        }
        /*
         * Remove out of band data so doesn't get presented to user.
         * This can happen independent of advancing the URG pointer,
         * but if two URG's are pending at once, some out-of-band
         * data may creep in... ick.
         */
        if (ti->ti_urp <= ti->ti_len
#ifdef SO_OOBINLINE
             && (so->so_options & SO_OOBINLINE) == 0
#endif
             )
            tcp_pulloutofband(so, ti, m);
    } else
        /*
         * If no out of band data is expected,
         * pull receive urgent pointer along
         * with the receive window.
         */
        if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
            tp->rcv_up = tp->rcv_nxt;
#else
        /* no need to support URG for now */
        if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
            tp->rcv_up = tp->rcv_nxt;

#endif

//dodata:                           /* XXX */

    /*
     * Process the segment text, merging it into the TCP sequencing queue,
     * and arranging for acknowledgment of receipt if necessary.
     * This process logically involves adjusting tp->rcv_wnd as data
     * is presented to the user (this happens in tcp_usrreq.c,
     * case PRU_RCVD).  If a FIN has already been received on this
     * connection then we just ignore the text.
     */
    if ((ti->ti_len || (tiflags&TH_FIN)) &&
        TCPS_HAVERCVDFIN(tp->t_state) == 0) {
        TCP_REASS(ctx,tp, ti, m, so, tiflags);
        /*
         * Note the amount of data that peer has sent into
         * our window, in order to estimate the sender's
         * buffer size.
         */
        //len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
    } else {
        rte_pktmbuf_free(m);
        tiflags &= ~TH_FIN;
    }

    /*
     * If FIN is received ACK the FIN and let the user know
     * that the connection is closing.
     */
    if (tiflags & TH_FIN) {
        if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
            socantrcvmore(so);
            tp->t_flags |= TF_ACKNOW;
            tp->rcv_nxt++;
        }
        switch (tp->t_state) {

        /*
         * In SYN_RECEIVED and ESTABLISHED STATES
         * enter the CLOSE_WAIT state.
         */
        case TCPS_SYN_RECEIVED:
        case TCPS_ESTABLISHED:
            tp->t_state = TCPS_CLOSE_WAIT;
            break;

        /*
         * If still in FIN_WAIT_1 STATE FIN has not been acked so
         * enter the CLOSING state.
         */
        case TCPS_FIN_WAIT_1:
            tp->t_state = TCPS_CLOSING;
            break;

        /*
         * In FIN_WAIT_2 state enter the TIME_WAIT state,
         * starting the time-wait timer, turning off the other 
         * standard timers.
         */
        case TCPS_FIN_WAIT_2:
            tp->t_state = TCPS_TIME_WAIT;
            tcp_canceltimers(tp);
            tp->t_timer[TCPT_2MSL] = TCPTV_2MSL;
            soisdisconnected(so);
            break;

        /*
         * In TIME_WAIT state restart the 2 MSL time_wait timer.
         */
        case TCPS_TIME_WAIT:
            tp->t_timer[TCPT_2MSL] = TCPTV_2MSL;
            break;
        }
    }
    if (so->so_options & US_SO_DEBUG){
        tcp_trace(ctx,TA_INPUT, ostate, tp, ti, 0,0);
    }


    /*
     * Return any desired output.
     */
    if (needoutput || (tp->t_flags & TF_ACKNOW))
        (void) tcp_output(ctx,tp);
    return 0;

dropafterack:
    /*
     * Generate an ACK dropping incoming segment if it occupies
     * sequence space, where the ACK reflects our state.
     */
    if (tiflags & TH_RST)
        goto drop;
    rte_pktmbuf_free(m);
    tp->t_flags |= TF_ACKNOW;
    (void) tcp_output(ctx,tp);
    return 0;

dropwithreset:
    /*
     * Generate a RST, dropping incoming segment.
     * Make ACK acceptable to originator of segment.
     * Don't bother to respond if destination was broadcast/multicast.
     */
    if ((tiflags & TH_RST) )
        goto drop;

    /* want to use 64B mbuf for response and free big MBUF */
    rte_pktmbuf_free(m);

    if (tiflags & TH_ACK){
        tcp_respond(ctx,tp,   (tcp_seq)0, ti->ti_ack, TH_RST);
    }else {
        if (tiflags & TH_SYN){
            ti->ti_len++;
        }
        tcp_respond(ctx,tp,  ti->ti_seq+ti->ti_len, (tcp_seq)0,
            TH_RST|TH_ACK);
    }
    /* destroy temporarily created socket */
    return 0;

drop:
    /*
     * Drop space held by incoming segment and return.
     */
    if (tp && (so->so_options & US_SO_DEBUG)){
        tcp_trace(ctx,TA_DROP, ostate, tp, ti, 0, 0);
    }
    rte_pktmbuf_free(m);
    /* destroy temporarily created socket */

findpcb:
    /* SYN packet that need to reopen the flow as the flow was closed already and free .. */
    return 1;
}



/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void tcp_xmit_timer(CTcpPerThreadCtx * ctx,
                    struct tcpcb *tp,
                    int16_t rtt){
    short delta;

    INC_STAT(ctx,tcps_rttupdated);
    if (tp->t_srtt != 0) {
        /*
         * srtt is stored as fixed point with 3 bits after the
         * binary point (i.e., scaled by 8).  The following magic
         * is equivalent to the smoothing algorithm in rfc793 with
         * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
         * point).  Adjust rtt to origin 0.
         */
        delta = rtt - 1 - (tp->t_srtt >> TCP_RTT_SHIFT);
        if ((tp->t_srtt += delta) <= 0)
            tp->t_srtt = 1;
        /*
         * We accumulate a smoothed rtt variance (actually, a
         * smoothed mean difference), then set the retransmit
         * timer to smoothed rtt + 4 times the smoothed variance.
         * rttvar is stored as fixed point with 2 bits after the
         * binary point (scaled by 4).  The following is
         * equivalent to rfc793 smoothing with an alpha of .75
         * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
         * rfc793's wired-in beta.
         */
        if (delta < 0)
            delta = -delta;
        delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
        if ((tp->t_rttvar += delta) <= 0)
            tp->t_rttvar = 1;
    } else {
        /* 
         * No rtt measurement yet - use the unsmoothed rtt.
         * Set the variance to half the rtt (so our first
         * retransmit happens at 3*rtt).
         */
        tp->t_srtt = rtt << TCP_RTT_SHIFT;
        tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
    }
    tp->t_rtt = 0;
    tp->t_rxtshift = 0;


    /*
     * the retransmit should happen at rtt + 4 * rttvar.
     * Because of the way we do the smoothing, srtt and rttvar
     * will each average +1/2 tick of bias.  When we compute
     * the retransmit timer, we want 1/2 tick of rounding and
     * 1 extra tick because of +-1/2 tick uncertainty in the
     * firing of the timer.  The bias will give us exactly the
     * 1.5 tick we need.  But, because the bias is
     * statistical, we have to test that we don't drop below
     * the minimum feasible timer (which is 2 ticks).
     */
    TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
        tp->t_rttmin, TCPTV_REXMTMAX);
    
    /*
     * We received an ack for a packet that wasn't retransmitted;
     * it is probably safe to discard any error indications we've
     * received recently.  This isn't quite right, but close enough
     * for now (a route might have failed after we sent a segment,
     * and the return path might not be symmetrical).
     */
    tp->t_softerror = 0;
}


CTcpTuneables * tcp_get_parent_tunable(CTcpPerThreadCtx * ctx,
                                      struct tcpcb *tp){

        if (!(TUNE_HAS_PARENT_FLOW & tp->m_tuneable_flags)) {
            /* not part of a bigger CFlow*/
            return((CTcpTuneables *)NULL);
        }

        CTcpFlow temp_tcp_flow;
        uint16_t offset =  (char *)&temp_tcp_flow.m_tcp - (char *)&temp_tcp_flow;
        CTcpFlow *tcp_flow = (CTcpFlow *)((char *)tp - offset);
        uint16_t temp_id = tcp_flow->m_c_template_idx;
        CTcpTuneables *tcp_tune = NULL;
        CAstfPerTemplateRW *temp_rw = NULL;
        CAstfTemplatesRW *ctx_temp_rw = ctx->get_template_rw();
        if (ctx_temp_rw)
            temp_rw = ctx_temp_rw->get_template_by_id(temp_id);

        if (temp_rw) {
            if (ctx->m_ft.is_client_side())
                tcp_tune = temp_rw->get_c_tune();
            else
                tcp_tune = temp_rw->get_s_tune();
        }
        return(tcp_tune);

}


/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 */
int tcp_mss(CTcpPerThreadCtx * ctx,
        struct tcpcb *tp, 
        u_int offer){

    if (tp->m_tuneable_flags<2) {
        tp->snd_cwnd = ctx->tcp_initwnd;
        return ctx->tcp_mssdflt;
    } else {

        uint16_t init_win_factor;
        uint16_t mss;

        CTcpTuneables *tune = tcp_get_parent_tunable(ctx,tp);
        if (!tune) {
            tp->snd_cwnd = ctx->tcp_initwnd;
            return ctx->tcp_mssdflt;
        }

        if (tune->is_valid_field(CTcpTuneables::tcp_no_delay)){
            if (tune->m_tcp_no_delay) {
                tp->t_flags |= TF_NODELAY;
            }else{
                tp->t_flags &= ~TF_NODELAY;
            }
        }

        if (tune->is_valid_field(CTcpTuneables::tcp_mss_bit)){
            mss = tune->m_tcp_mss;
        }else{
            mss = ctx->tcp_mssdflt;
        }

        if (tune->is_valid_field(CTcpTuneables::tcp_initwnd_bit)){
            init_win_factor = tune->m_tcp_initwnd;
        }else{
            init_win_factor = ctx->tcp_initwnd_factor;
        }

        tp->snd_cwnd = _update_initwnd(mss,init_win_factor);
        return mss;
    }
}
