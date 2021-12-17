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


static inline void tcp_pkt_update_len(CFlowTemplate *ftp,
                                      struct CTcpCb *tp,
                                      CTcpPkt &pkt,
                                      uint32_t dlen,
                                      uint16_t tcphlen){

    uint32_t tcp_h_pyld=dlen+tcphlen;
    char *p=pkt.get_header_ptr();

    if (ftp->m_offload_flags & OFFLOAD_TX_CHKSUM){

        rte_mbuf_t   * m=pkt.m_buf;

        if (!ftp->m_is_ipv6){
            uint16_t tlen=ftp->m_offset_l4-ftp->m_offset_ip+tcp_h_pyld;
            m->l2_len = ftp->m_offset_ip;
            m->l3_len = ftp->m_offset_l4-ftp->m_offset_ip;
            m->ol_flags |= (PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM);
            IPHeader * ipv4=(IPHeader *)(p+ftp->m_offset_ip);
            ipv4->ClearCheckSum();
            TCPHeader *  tcp=(TCPHeader *)(p+ftp->m_offset_l4);
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
                tcp->setChecksumRaw(ftp->m_l4_pseudo_checksum);
            }else{
                ipv4->setTotalLength(tlen);
                tcp->setChecksumRaw(pkt_AddInetChecksumRaw(ftp->m_l4_pseudo_checksum ,PKT_NTOHS(tlen-20)));
            }


        }else{
            uint16_t tlen=tcp_h_pyld;
            m->l2_len = ftp->m_offset_ip;
            m->l3_len = ftp->m_offset_l4-ftp->m_offset_ip;
            m->ol_flags |= ( PKT_TX_IPV6 | PKT_TX_TCP_CKSUM);
            IPv6Header * ipv6=(IPv6Header *)(p+ftp->m_offset_ip);
            TCPHeader *  tcp=(TCPHeader *)(p+ftp->m_offset_l4);

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
                ipv6->setPayloadLen(0);
                tcp->setChecksumRaw(ftp->m_l4_pseudo_checksum);
            }else{
                ipv6->setPayloadLen(tlen);
                tcp->setChecksumRaw(pkt_AddInetChecksumRaw(ftp->m_l4_pseudo_checksum ,PKT_NTOHS(tlen)));
            }
        }
    }else{
        if (!ftp->m_is_ipv6){
            uint16_t tlen=ftp->m_offset_l4-ftp->m_offset_ip+tcp_h_pyld;
            IPHeader * lpv4=(IPHeader *)(p+ftp->m_offset_ip);
            lpv4->setTotalLength(tlen);
            lpv4->updateCheckSumFast();
        }else{
            uint16_t tlen = tcp_h_pyld;
            IPv6Header * Ipv6=(IPv6Header *)(p+ftp->m_offset_ip);
            Ipv6->setPayloadLen(tlen);
        }
    }

}

/**
 * build control packet without data
 * 
 * @param pctx
 * @param tp
 * @param tcphlen
 * @param pkt
 * 
 * @return 
 */
static inline int _tcp_build_cpkt(CPerProfileCtx * pctx,
                                  CFlowTemplate *ftp,
                                  struct CTcpCb *tp,
                                  uint16_t tcphlen,
                                  CTcpPkt &pkt){
    int len= ftp->m_offset_l4+tcphlen;
    rte_mbuf_t * m;
    m=tp->pktmbuf_alloc(len);
    pkt.m_buf=m;
    CTcpPerThreadCtx * ctx = pctx->m_ctx;

    if (ftp->is_tunnel()){
        /*Check if we need to add any error counter for the special case*/
        if (ctx->is_client_side()) {
            if (!ftp->is_tunnel_aware()){
                INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_notunnel);
            }
            pkt.m_buf->dynfield_ptr = ftp->m_tunnel_ctx;
        } else {
            //add the server tunnel ctx, works in gtpu-loopback mode
            pkt.m_buf->dynfield_ptr = ftp->m_tunnel_ctx;
        }
     }

    if (m==0) {
        INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_nombuf);
        /* drop the packet */
        return(-1);
    }

    rte_mbuf_set_as_core_local(m);
    char *p=rte_pktmbuf_append(m,len);

    /* copy template */
    memcpy(p,ftp->m_template_pkt,len);
    pkt.lpTcp =(TCPHeader    *)(p+ftp->m_offset_l4);

    return(0);
}

int tcp_build_cpkt(CPerProfileCtx * pctx,
                   struct CTcpCb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt){
    assert(tp->m_flow);
    CFlowTemplate *ftp=&tp->m_flow->m_template;

   int res=_tcp_build_cpkt(pctx,ftp,tp,tcphlen,pkt);
   if (res==0){
       tcp_pkt_update_len(ftp,tp,pkt,0,tcphlen) ;
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
 * @param pctx
 * @param tp
 * @param offset
 * @param dlen
 * @param tcphlen
 * @param pkt
 * 
 * @return 
 */
static inline int tcp_build_dpkt_(CPerProfileCtx * pctx,
                                  CFlowTemplate *ftp,
                                  struct CTcpCb *tp,
                                  uint32_t offset, 
                                  uint32_t dlen,
                                  uint16_t tcphlen, 
                                  CTcpPkt &pkt){

    int res=_tcp_build_cpkt(pctx,ftp,tp,tcphlen,pkt);
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

    CTcpSockBuf *lptxs=(CTcpSockBuf*)&tp->m_socket.so_snd;
    while (dlen>0) {
        /* get blocks from socket buffer */
        CBufMbufRef  rb;
        lptxs->get_by_offset((tcp_socket*)&tp->m_socket,offset,rb);
        assert(rb.get_mbuf_size()>0);

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
                    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_nombuf);
                    rte_pktmbuf_free(m);
                    return(-1);
                }
                rte_mbuf_set_as_core_local(mi);
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
int tcp_build_dpkt(CPerProfileCtx * pctx,
                   struct CTcpCb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t tcphlen, 
                   CTcpPkt &pkt){
    assert(tp->m_flow);
    CFlowTemplate *ftp=&tp->m_flow->m_template;

    int res = tcp_build_dpkt_(pctx,ftp,tp,offset,dlen,tcphlen,pkt);
    if (res==0){
        tcp_pkt_update_len(ftp,tp,pkt,dlen,tcphlen) ;
    }
    return(res);
}

int
tcp_build_pkt(struct tcpcb *_tp, uint32_t off, uint32_t len, uint16_t hdrlen, uint16_t optlen, struct mbuf **mp, struct tcphdr **thp)
{
    CTcpCb *tp = static_cast<CTcpCb*>(_tp);
    CTcpPkt pkt;
    int res;

    pkt.m_optlen = optlen;
    if (len) {
        res = tcp_build_dpkt(tp->m_flow->m_pctx, tp, off, len, hdrlen, pkt);
    } else {
        res = tcp_build_cpkt(tp->m_flow->m_pctx, tp, hdrlen, pkt);
    }

    if (res == 0) {
        *mp = (struct mbuf *)pkt.m_buf;
        if (thp) {
            *thp = (struct tcphdr *)pkt.lpTcp;
        }
    }

    return res;
}

