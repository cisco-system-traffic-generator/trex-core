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
 *  @(#)tcp_output.c    8.4 (Berkeley) 5/24/95
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
#include "mbuf.h"

#include <assert.h>

#define MAX_TCPOPTLEN   32  /* max # bytes that go in options */

                      
/*
 * Flags used when sending segments in tcp_output.
 * Basic flags (TH_RST,TH_ACK,TH_SYN,TH_FIN) are totally
 * determined by state, with the proviso that TH_FIN is sent only
 * if all data queued for output is included in the segment.
 */
const u_char    tcp_outflags[TCP_NSTATES] = {
    TH_RST|TH_ACK, 0, TH_SYN, TH_SYN|TH_ACK,
    TH_ACK, TH_ACK,
    TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_ACK, TH_ACK,
};

static const char *tcpstates[] = {
    "CLOSED",   "LISTEN",   "SYN_SENT", "SYN_RCVD",
    "ESTABLISHED",  "CLOSE_WAIT",   "FIN_WAIT_1",   "CLOSING",
    "LAST_ACK", "FIN_WAIT_2",   "TIME_WAIT",
};

const char ** tcp_get_tcpstate(){
    return (tcpstates);
}


static inline void tcp_pkt_update_len(struct tcpcb *tp,
                                      CTcpPkt &pkt,
                                      uint32_t dlen,
                                      uint16_t tcphlen){

    uint32_t tcp_h_pyld=dlen+tcphlen;
    char *p=pkt.get_header_ptr();

    if (tp->m_offload_flags & TCP_OFFLOAD_TX_CHKSUM){

        rte_mbuf_t   * m=pkt.m_buf;

        if (!tp->is_ipv6){
            uint16_t tlen=tp->offset_tcp-tp->offset_ip+tcp_h_pyld;
            m->l2_len = tp->offset_ip;
            m->l3_len = tp->offset_tcp-tp->offset_ip;
            m->ol_flags |= (PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM);
            IPHeader * ipv4=(IPHeader *)(p+tp->offset_ip);
            ipv4->ClearCheckSum();
            TCPHeader *  tcp=(TCPHeader *)(p+tp->offset_tcp);
            /* must be before checksum calculation */
            bool tso_done=false;
            if ( tp->is_tso() ) {
                uint16_t seg_size = tp->t_maxseg - pkt.m_optlen;
                if ( dlen>seg_size ){
                    m->ol_flags |=PKT_TX_TCP_SEG; 
                    m->tso_segsz = seg_size;
                    m->l4_len = pkt.m_optlen+TCP_HEADER_LEN;
                    tso_done=true;
                }
            }
            if (tso_done){
                /* in case of TSO the len is auto calculated */
                ipv4->setTotalLength(20);
                tcp->setChecksumRaw(tp->l4_pseudo_checksum);
            }else{
                ipv4->setTotalLength(tlen);
                tcp->setChecksumRaw(pkt_AddInetChecksumRaw(tp->l4_pseudo_checksum ,PKT_NTOHS(tlen-20)));
            }


        }else{
            /* TBD fix me IPV6 does not work */
            uint16_t tlen=tcp_h_pyld;
            m->l2_len = tp->offset_ip;
            m->l3_len = tp->offset_tcp-tp->offset_ip;
            m->ol_flags |= ( PKT_TX_IPV6 | PKT_TX_TCP_CKSUM);
            IPv6Header * Ipv6=(IPv6Header *)(p+tp->offset_ip);
            Ipv6->setPayloadLen(tlen);
            TCPHeader *  tcp=(TCPHeader *)(p+tp->offset_tcp);
            /* must be before checksum calculation */
            if ( tp->is_tso() && (dlen>tp->t_maxseg)){
                m->ol_flags |=PKT_TX_TCP_SEG; 
                m->tso_segsz = tp->t_maxseg;
            }
            tcp->setChecksumRaw(pkt_AddInetChecksumRaw(tp->l4_pseudo_checksum ,PKT_NTOHS(tlen-20)));
        }
    }else{
        if (!tp->is_ipv6){
            uint16_t tlen=tp->offset_tcp-tp->offset_ip+tcp_h_pyld;
            IPHeader * lpv4=(IPHeader *)(p+tp->offset_ip);
            lpv4->setTotalLength(tlen);
            lpv4->updateCheckSumFast();
        }else{
            uint16_t tlen = tcp_h_pyld;
            IPv6Header * Ipv6=(IPv6Header *)(p+tp->offset_ip);
            Ipv6->setPayloadLen(tlen);
        }
    }



}

/**
 * build control packet without data
 * 
 * @param ctx
 * @param tp
 * @param tcphlen
 * @param pkt
 * 
 * @return 
 */
static inline int _tcp_build_cpkt(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt){
    int len= tp->offset_tcp+tcphlen;
    rte_mbuf_t * m;
    m=tp->pktmbuf_alloc(len);
    pkt.m_buf=m;
    if (m==0) {
        INC_STAT(ctx,tcps_nombuf);
        /* drop the packet */
        return(-1);
    }

    char *p=rte_pktmbuf_append(m,len);

    /* copy template */
    memcpy(p,tp->template_pkt,len);
    pkt.lpTcp =(TCPHeader    *)(p+tp->offset_tcp);


    return(0);
}

int tcp_build_cpkt(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt){
   int res=_tcp_build_cpkt(ctx,tp,tcphlen,pkt);
   if (res==0){
       tcp_pkt_update_len(tp,pkt,0,tcphlen) ;
   }
   return(res);
}


static inline uint16_t update_next_mbuf(rte_mbuf_t   *mi,
                                        CBufMbufRef &rb,
                                        rte_mbuf_t * &lastm,
                                        uint16_t dlen){

    uint16_t bsize = bsd_umin(rb.get_mbuf_size(),dlen);
    uint16_t trim=rb.get_mbuf_size()-bsize;
    if (rb.m_offset) {
        rte_pktmbuf_adj(mi, rb.m_offset);
    }
    if (trim) {
        rte_pktmbuf_trim(mi, trim);
    }

    lastm->next =mi;
    lastm=mi;
    return(bsize);
}


/**
 * build packet from socket buffer 
 * 
 * @param ctx
 * @param tp
 * @param offset
 * @param dlen
 * @param tcphlen
 * @param pkt
 * 
 * @return 
 */
static inline int tcp_build_dpkt_(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t tcphlen, 
                   CTcpPkt &pkt){
    int res=_tcp_build_cpkt(ctx,tp,tcphlen,pkt);
    if (res<0) {
        return(res);
    }
    if (dlen==0) {
        return 0;
    }
    rte_mbuf_t   * m=pkt.m_buf;
    rte_mbuf_t   * lastm=m;

    uint16_t       bsize;

    m->pkt_len += dlen;

    CTcpSockBuf *lptxs=&tp->m_socket.so_snd;
    while (dlen>0) {
        /* get blocks from socket buffer */
        CBufMbufRef  rb;
        lptxs->get_by_offset(&tp->m_socket,offset,rb);

        rte_mbuf_t   * mn=rb.m_mbuf;
        if (rb.m_type==MO_CONST) {

            if (unlikely(!rb.need_indirect_mbuf(dlen))){
                /* last one */
                rte_pktmbuf_refcnt_update(mn,1);
                lastm->next =mn;
                bsize = rb.get_mbuf_size();
            }else{
                /* need to allocate indirect */
                rte_mbuf_t   * mi=tp->pktmbuf_alloc_small();
                if (mi==0) {
                    INC_STAT(ctx,tcps_nombuf);
                    rte_pktmbuf_free(m);
                    return(-1);
                }
                /* inc mn ref count */
                rte_pktmbuf_attach(mi,mn);

                bsize = update_next_mbuf(mi,rb,lastm,dlen);
            }
        }else{
            //rb.m_type==MO_RW not supported right now 
            if (rb.m_type==MO_RW) {
                /* assume mbuf with one seg */
                assert(mn->nb_segs==1);

                bsize = update_next_mbuf(mn,rb,lastm,dlen);

                m->nb_segs += 1;
            }else{
                assert(0);
            }
        }

        m->nb_segs += 1;
        dlen-=bsize;
        offset+=bsize;
    }

    return(0);
}

/* len : if TSO==true, it is the TSO packet size (before segmentation), 
         else it is the packet size */
int tcp_build_dpkt(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t tcphlen, 
                   CTcpPkt &pkt){

    int res = tcp_build_dpkt_(ctx,tp,offset,dlen,tcphlen,pkt);
    if (res==0){
        tcp_pkt_update_len(tp,pkt,dlen,tcphlen) ;
    }
    return(res);
}


/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
void tcp_respond(CTcpPerThreadCtx * ctx,
            struct tcpcb *tp, 
            tcp_seq ack, 
            tcp_seq seq, 
            int flags){
    assert(tp);
    uint32_t win = sbspace(&tp->m_socket.so_rcv);

    CTcpPkt pkt;
    if (tcp_build_cpkt(ctx,tp,TCP_HEADER_LEN,pkt)!=0){
        return;
    }
    TCPHeader * ti=pkt.lpTcp;

    ti->setSeqNumber(seq);
    ti->setAckNumber(ack);
    ti->setFlag(flags);
    ti->setWindowSize((uint16_t) (win >> tp->rcv_scale));

    ctx->m_cb->on_tx(ctx,tp,pkt.m_buf);
}




/*
 * Tcp output routine: figure out what should be sent and send it.
 */
int tcp_output(CTcpPerThreadCtx * ctx,struct tcpcb *tp) {

    struct tcp_socket *so = &tp->m_socket;
    int32_t len ;
    uint32_t win;
    int32_t off, flags, error=0;
    u_char opt[MAX_TCPOPTLEN];
    unsigned optlen, hdrlen;
    int idle, sendalot;

    /*
     * Determine length of data that should be transmitted,
     * and flags that will be used.
     * If there is some data or critical controls (SYN, RST)
     * to send, then transmit; otherwise, investigate further.
     */
    idle = (tp->snd_max == tp->snd_una);
    if (idle && tp->t_idle >= tp->t_rxtcur){
        /*
         * We have been idle for "a while" and no acks are
         * expected to clock out any data we send --
         * slow start to get ack "clock" running again.
         */
        tp->snd_cwnd = tp->t_maxseg;
    }
again:
    sendalot = 0;
    off = tp->snd_nxt - tp->snd_una;
    assert(off>=0);
    win = bsd_umin(tp->snd_wnd, tp->snd_cwnd);

    flags = tcp_outflags[tp->t_state];
    /*
     * If in persist timeout with window of 0, send 1 byte.
     * Otherwise, if window is small but nonzero
     * and timer expired, we will send what we can
     * and go to transmit state.
     */
    if (tp->t_force) {
        if (win == 0) {
            /*
             * If we still have some data to send, then
             * clear the FIN bit.  Usually this would
             * happen below when it realizes that we
             * aren't sending all the data.  However,
             * if we have exactly 1 byte of unset data,
             * then it won't clear the FIN bit below,
             * and if we are in persist state, we wind
             * up sending the packet without recording
             * that we sent the FIN bit.
             *
             * We can't just blindly clear the FIN bit,
             * because if we don't have any more data
             * to send then the probe will be the FIN
             * itself.
             */
            if (off < so->so_snd.sb_cc)
                flags &= ~TH_FIN;
            win = 1;
        } else {
            tp->t_timer[TCPT_PERSIST] = 0;
            tp->t_rxtshift = 0;
        }
    }

    len = ((int32_t)bsd_umin(so->so_snd.sb_cc, win)) - off;

    if (len < 0) {
        /*
         * If FIN has been sent but not acked,
         * but we haven't been called to retransmit,
         * len will be -1.  Otherwise, window shrank
         * after we sent into it.  If window shrank to 0,
         * cancel pending retransmit and pull snd_nxt
         * back to (closed) window.  We will enter persist
         * state below.  If the window didn't close completely,
         * just wait for an ACK.
         */
        len = 0;
        if (win == 0) {
            tp->t_timer[TCPT_REXMT] = 0;
            tp->snd_nxt = tp->snd_una;
        }
    }

    bool tso=false;
    uint16_t max_seg = tp->get_maxseg_tso(tso);
    if (len > max_seg) {
        len = max_seg;
        sendalot = 1;
    }
    if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
        flags &= ~TH_FIN;

    win = sbspace(&so->so_rcv);               

    /*
     * Sender silly window avoidance.  If connection is idle
     * and can send all data, a maximum segment,
     * at least a maximum default-size segment do it,
     * or are forced, do it; otherwise don't bother.
     * If peer's buffer is tiny, then send
     * when window is at least half open.
     * If retransmitting (possibly after persist timer forced us
     * to send into a small window), then must resend.
     */
    if (len) {
        if ((len >= tp->t_maxseg))
            goto send;
        if ((idle || tp->t_flags & TF_NODELAY) &&
            len + off >= so->so_snd.sb_cc)
            goto send;
        if (tp->t_force)
            goto send;
        if (len >= tp->max_sndwnd / 2)
            goto send;
        if (SEQ_LT(tp->snd_nxt, tp->snd_max))
            goto send;
    }

    /*
     * Compare available window to amount of window
     * known to peer (as advertised window less
     * next expected input).  If the difference is at least two
     * max size segments, or at least 50% of the maximum possible
     * window, then want to send a window update to peer.
     */
    if (win > 0) {
        /* 
         * "adv" is the amount we can increase the window,
         * taking into account that we are limited by
         * TCP_MAXWIN << tp->rcv_scale.
         */
        int32_t adv = bsd_umin(win, (int32_t)TCP_MAXWIN << tp->rcv_scale) -
            (tp->rcv_adv - tp->rcv_nxt);

        if (adv >= (int32_t) (2 * tp->t_maxseg))
            goto send;
        if ((int32_t)(2 * adv) >= (int32_t) so->so_rcv.sb_hiwat)
            goto send;
    }

    /*
     * Send if we owe peer an ACK.
     */
    if (tp->t_flags & TF_ACKNOW)
        goto send;
    if (flags & (TH_SYN|TH_RST))
        goto send;
    if (SEQ_GT(tp->snd_up, tp->snd_una))
        goto send;
    /*
     * If our state indicates that FIN should be sent
     * and we have not yet done so, or we're retransmitting the FIN,
     * then we need to send.
     */
    if (flags & TH_FIN &&
        ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
        goto send;

    /*
     * TCP window updates are not reliable, rather a polling protocol
     * using ``persist'' packets is used to insure receipt of window
     * updates.  The three ``states'' for the output side are:
     *  idle            not doing retransmits or persists
     *  persisting      to move a small or zero window
     *  (re)transmitting    and thereby not persisting
     *
     * tp->t_timer[TCPT_PERSIST]
     *  is set when we are in persist state.
     * tp->t_force
     *  is set when we are called to send a persist packet.
     * tp->t_timer[TCPT_REXMT]
     *  is set when we are retransmitting
     * The output side is idle when both timers are zero.
     *
     * If send window is too small, there is data to transmit, and no
     * retransmit or persist is pending, then go to persist state.
     * If nothing happens soon, send when timer expires:
     * if window is nonzero, transmit what we can,
     * otherwise force out a byte.
     */
    if (so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
        tp->t_timer[TCPT_PERSIST] == 0) {
        tp->t_rxtshift = 0;
        tcp_setpersist(ctx,tp);
    }

    /*
     * No reason to send a segment, just return.
     */
    return (0);

send:

    CTcpPkt     pkt;
    TCPHeader * ti;

    /*
     * Before ESTABLISHED, force sending of initial options
     * unless TCP set not to do any options.
     * NOTE: we assume that the IP/TCP header plus TCP options
     * always fit in a single mbuf, leaving room for a maximum
     * link header, i.e.
     *  max_linkhdr + sizeof (struct tcpiphdr) + optlen <= MHLEN
     */
    optlen = 0;
    hdrlen = TCP_HEADER_LEN;
    if (flags & TH_SYN) {
        tp->snd_nxt = tp->iss;
        if ((tp->t_flags & TF_NOOPT) == 0) {
            u_short mss;

            opt[0] = TCPOPT_MAXSEG;
            opt[1] = 4;
            mss = bsd_htons((u_short) tcp_mss(ctx,tp, 0));
            *(uint16_t*)(opt + 2)=mss;
            optlen = 4;
     
            if ((tp->t_flags & TF_REQ_SCALE) &&
                ((flags & TH_ACK) == 0 ||
                (tp->t_flags & TF_RCVD_SCALE))) {
                *((uint32_t *) (opt + optlen)) = bsd_htonl(
                    TCPOPT_NOP << 24 |
                    TCPOPT_WINDOW << 16 |
                    TCPOLEN_WINDOW << 8 |
                    tp->request_r_scale);
                optlen += 4;
            }
        }
    }
 
    /*
     * Send a timestamp and echo-reply if this is a SYN and our side 
     * wants to use timestamps (TF_REQ_TSTMP is set) or both our side
     * and our peer have sent timestamps in our SYN's.
     */
    if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
         (flags & TH_RST) == 0 &&
        ((flags & (TH_SYN|TH_ACK)) == TH_SYN ||
         (tp->t_flags & TF_RCVD_TSTMP))) {
        uint32_t *lp = (uint32_t *)(opt + optlen);
 
        /* Form timestamp option as shown in appendix A of RFC 1323. */
        *lp++ = bsd_htonl(TCPOPT_TSTAMP_HDR);
        *lp++ = bsd_htonl(ctx->tcp_now);
        *lp   = bsd_htonl(tp->ts_recent);
        optlen += TCPOLEN_TSTAMP_APPA;
    }

    hdrlen += optlen;

    /*
     * Adjust data length if insertion of options will
     * bump the packet length beyond the t_maxseg length.
     */
    pkt.m_optlen  = optlen; /* for TSO- current segment, will be replicate, so need to update max_seg  */
    if (!tso) {
        if (len > tp->t_maxseg - optlen) {
            len = tp->t_maxseg - optlen;
            sendalot = 1;
            flags &= ~TH_FIN;
        }
    }else {
        if (len > tp->t_maxseg - optlen ){
            if (optlen) {
                if ((len%tp->t_maxseg)==0) {
                    len -= (len/tp->t_maxseg)*optlen; /* reduce the optlen from all packets*/
                    sendalot = 1;
                    flags &= ~TH_FIN;
                }
            }
        }
    }


    /*
     * Grab a header mbuf, attaching a copy of data to
     * be transmitted, and initialize the header from
     * the template for sends on this connection.
     */
    if (len) {
        if (tp->t_force && len == 1){
            INC_STAT(ctx,tcps_sndprobe);
        } else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
            INC_STAT(ctx,tcps_sndrexmitpack);
            INC_STAT_CNT(ctx,tcps_sndrexmitbyte,len);
        } else {
            INC_STAT(ctx,tcps_sndpack);
            INC_STAT_CNT(ctx,tcps_sndbyte,len);
        }

        if (tcp_build_dpkt(ctx,tp,off,len,hdrlen,pkt)!=0){
            error = ENOBUFS;
            goto out;
        }

        /*
         * If we're sending everything we've got, set PUSH.
         * (This will keep happy those implementations which only
         * give data to the user when a buffer fills or
         * a PUSH comes in.)
         */
        if (off + len == so->so_snd.sb_cc)
            flags |= TH_PUSH;
    } else {
        if (tp->t_flags & TF_ACKNOW){
            INC_STAT(ctx,tcps_sndacks);
        } else if (flags & (TH_SYN|TH_FIN|TH_RST)){
            INC_STAT(ctx,tcps_sndctrl);
        } else if (SEQ_GT(tp->snd_up, tp->snd_una)){
            INC_STAT(ctx,tcps_sndurg);
        } else{
            INC_STAT(ctx,tcps_sndwinup);
        }

        if ( tcp_build_cpkt(ctx,tp,hdrlen,pkt)!=0){
            error = ENOBUFS;
            goto out;
        }
    }

    ti = pkt.lpTcp;

    /*
     * Fill in fields, remembering maximum advertised
     * window for use in delaying messages about window sizes.
     * If resending a FIN, be sure not to use a new sequence number.
     */
    if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && 
        tp->snd_nxt == tp->snd_max)
        tp->snd_nxt--;
    /*
     * If we are doing retransmissions, then snd_nxt will
     * not reflect the first unsent octet.  For ACK only
     * packets, we do not want the sequence number of the
     * retransmitted packet, we want the sequence number
     * of the next unsent octet.  So, if there is no data
     * (and no SYN or FIN), use snd_max instead of snd_nxt
     * when filling in ti_seq.  But if we are in persist
     * state, snd_max might reflect one byte beyond the
     * right edge of the window, so use snd_nxt in that
     * case, since we know we aren't doing a retransmission.
     * (retransmit and persist are mutually exclusive...)
     */
    if (len || (flags & (TH_SYN|TH_FIN)) || tp->t_timer[TCPT_PERSIST]){
        ti->setSeqNumber(tp->snd_nxt);
    }else{
        ti->setSeqNumber(tp->snd_max);
    }

    ti->setAckNumber(tp->rcv_nxt);
    if (optlen) {
        memcpy((char *)ti->getOptionPtr(),opt,  optlen);
        ti->setHeaderLength(TCP_HEADER_LEN+optlen);
    }
    ti->setFlag(flags);
    /*
     * Calculate receive window.  Don't shrink window,
     * but avoid silly window syndrome.
     */
    if ( (win < (so->so_rcv.sb_hiwat / 4)) && (win < (uint32_t)tp->t_maxseg) ){
        win = 0;
    }
    if (win < (uint32_t)(tp->rcv_adv - tp->rcv_nxt))
        win = (uint32_t)(tp->rcv_adv - tp->rcv_nxt);
    if (win > (uint32_t)TCP_MAXWIN << tp->rcv_scale)
        win = (uint32_t)TCP_MAXWIN << tp->rcv_scale;

    ti->setWindowSize( (u_short) (win>>tp->rcv_scale));
    if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
        /* not support this for now - hhaim*/
        //ti->ti_urp = bsd_htons((u_short)(tp->snd_up - tp->snd_nxt));
        //ti->ti_flags |= TH_URG;
    } else{
        /*
         * If no urgent pointer to send, then we pull
         * the urgent pointer to the left edge of the send window
         * so that it doesn't drift into the send window on sequence
         * number wraparound.
         */
        tp->snd_up = tp->snd_una;       /* drag it along */
    }


    /*
     * In transmit state, time the transmission and arrange for
     * the retransmit.  In persist state, just set snd_max.
     */
    if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) {
        tcp_seq startseq = tp->snd_nxt;

        /*
         * Advance snd_nxt over sequence space of this segment.
         */
        if (flags & (TH_SYN|TH_FIN)) {
            if (flags & TH_SYN)
                tp->snd_nxt++;
            if (flags & TH_FIN) {
                tp->snd_nxt++;
                tp->t_flags |= TF_SENTFIN;
            }
        }
        tp->snd_nxt += len;
        if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
            tp->snd_max = tp->snd_nxt;
            /*
             * Time this transmission if not a retransmission and
             * not currently timing anything.
             */
            if (tp->t_rtt == 0) {
                tp->t_rtt = 1;
                tp->t_rtseq = startseq;
                INC_STAT(ctx,tcps_segstimed);
            }
        }

        /*
         * Set retransmit timer if not currently set,
         * and not doing an ack or a keep-alive probe.
         * Initial value for retransmit timer is smoothed
         * round-trip time + 2 * round-trip time variance.
         * Initialize shift counter which is used for backoff
         * of retransmit time.
         */
        if (tp->t_timer[TCPT_REXMT] == 0 &&
            tp->snd_nxt != tp->snd_una) {
            tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
            if (tp->t_timer[TCPT_PERSIST]) {
                tp->t_timer[TCPT_PERSIST] = 0;
                tp->t_rxtshift = 0;
            }
        }
    } else
        if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
            tp->snd_max = tp->snd_nxt + len;

    /*
     * Trace.
     */
    if (so->so_options & US_SO_DEBUG){
        tcp_trace(ctx,TA_OUTPUT, tp->t_state, tp, (struct tcpiphdr *)0, ti,len);
    }

    error = ctx->m_cb->on_tx(ctx,tp,pkt.m_buf);

    if (error) {
out:
        if (error == ENOBUFS) {
            tcp_quench(tp);
            return (0);
        }
        if ((error == EHOSTUNREACH || error == ENETDOWN)
            && TCPS_HAVERCVDSYN(tp->t_state)) {
            tp->t_softerror = error;
            return (0);
        }
        return (error);
    }
    INC_STAT(ctx,tcps_sndtotal);

    /*
     * Data sent (as far as we can tell).
     * If this advertises a larger window than any other segment,
     * then remember the size of the advertised window.
     * Any pending ACK has now been sent.
     */
    if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
        tp->rcv_adv = tp->rcv_nxt + win;
    tp->last_ack_sent = tp->rcv_nxt;
    tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);
    if (sendalot)
        goto again;
    return (0);
}

void tcp_setpersist(CTcpPerThreadCtx * ctx,
                    struct tcpcb *tp){
    int16_t t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

    assert(tp->t_timer[TCPT_REXMT]==0);

    /*
     * Start/restart persistance timer.
     */
    TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
        t * tcp_backoff[tp->t_rxtshift],
        TCPTV_PERSMIN, TCPTV_PERSMAX);

    if (tp->t_rxtshift < TCP_MAXRXTSHIFT){
        tp->t_rxtshift++;
    }
}



