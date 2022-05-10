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
#include "netinet/tcp_mbuf.h"
#include "utl_mbuf.h"
#include "astf/astf_db.h"
#include "astf/astf_template_db.h"




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
static inline void tcp_pktmbuf_fix_mbuf(struct rte_mbuf *& m,
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


/* tcp_mbuf.h */

struct mbuf *
m_adj_fix(struct mbuf *m, int req_len, int l7_len)
{
    tcp_pktmbuf_fix_mbuf((struct rte_mbuf *&)m, req_len, l7_len);
    return m;
}

void
m_trim(struct mbuf *m, int req_len)
{
    tcp_pktmbuf_trim((struct rte_mbuf *)m, req_len);
}

void
m_freem(struct mbuf *m)
{
    if (m != NULL) {
        rte_pktmbuf_free((struct rte_mbuf *)m);
    }
}

uint32_t
m_pktlen(struct mbuf *m)
{
    return m->pkt_len;
}


bool CTcpReass::expect(vec_tcp_reas_t & lpkts,FILE * fd){
    if (lpkts.size() != m_blocks.size()) {
        fprintf(fd,"ERROR not the same size");
        return(false);
    }
    int i = 0;
    for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it, ++i) {
        auto& block = it->second;
        if ( !(block == lpkts[i] )){
            fprintf(fd,"ERROR object are not the same \n");
            fprintf(fd,"Exists :\n");
            block.Dump(fd);
            fprintf(fd,"Expected :\n");
            lpkts[i].Dump(fd);
        }
    }
    return(true);
}

void CTcpReass::Dump(FILE *fd){
    fprintf(fd,"active blocks : %d \n", get_active_blocks());
    for (auto it: m_blocks) {
        auto& block = it.second;
        block.Dump(fd);
    }
}


int CTcpReass::tcp_reass_no_data(CPerProfileCtx * pctx,
                                 struct CTcpCb *tp){
    int flags=0;

    if (TCPS_HAVERCVDSYN(tp->t_state) == 0){
        return (0);
    }

    auto first = m_blocks.begin();
    auto& block = first->second;

    if (block.m_seq != tp->rcv_nxt) {
        return(0);
    }

    tp->rcv_nxt += block.m_len;
    uint16_t tg_id = tp->m_flow->m_tg_id;
    INC_STAT(pctx, tg_id, tcps_rcvpack);
    INC_STAT_CNT(pctx, tg_id, tcps_rcvbyte, block.m_len);

    flags = (block.m_flags==1) ? TH_FIN :0;

    sbappend_bytes((struct tcp_socket*)&tp->m_socket,
                   &tp->m_socket.so_rcv ,
                   block.m_len);

    /* remove the first block */
    m_blocks.erase(first);

    sorwakeup(&tp->m_socket);
    return (flags);
}


int CTcpReass::tcp_reass(CPerProfileCtx * pctx,
                         struct CTcpCb *tp, 
                         struct tcpiphdr *ti, 
                         struct rte_mbuf *m){
    pre_tcp_reass(pctx,tp,ti,m);
    return (tcp_reass_no_data(pctx,tp));
}


bool tcp_reass_merge_block(CPerProfileCtx * pctx,
                           struct CTcpCb *tp,
                           CTcpReassBlock& base,
                           CTcpReassBlock& next)
{
    int32_t diff = base.m_seq + base.m_len - next.m_seq;

    if (diff < 0) { /* no overlap */
        return false;
    }

    if (diff > 0) {
        uint16_t tg_id = tp->m_flow->m_tg_id;
        if (diff >= next.m_len) {
            /* all packet is duplicate */
            INC_STAT(pctx, tg_id, tcps_rcvduppack);
            INC_STAT_CNT(pctx, tg_id, tcps_rcvdupbyte, next.m_len);
        } else {
            /* part of it duplicate */
            INC_STAT(pctx, tg_id, tcps_rcvpartduppack);
            INC_STAT_CNT(pctx, tg_id, tcps_rcvpartdupbyte, next.m_len - diff);
        }
    }
    if (next.m_len > diff) {
        base.m_len += (next.m_len - diff);
        base.m_flags |= next.m_flags; /* or the overlay FIN */
    }

    return true;
}


int CTcpReass::pre_tcp_reass(CPerProfileCtx * pctx,
                         struct CTcpCb *tp, 
                         struct tcpiphdr *ti, 
                         struct rte_mbuf *m){

    assert(ti);
    if (m) {
        rte_pktmbuf_free(m);
    }

    uint16_t tg_id = tp->m_flow->m_tg_id;
    INC_STAT(pctx, tg_id, tcps_rcvoopack);
    INC_STAT_CNT(pctx, tg_id, tcps_rcvoobyte, ti->ti_len);
    tp->t_rcvoopack++;

    CTcpReassBlock cur{ti->ti_seq, ti->ti_len, !!(ti->ti_flags & TH_FIN)};
    CTcpSeqKey cur_seq{cur.m_seq};

    if (m_blocks.empty()) {
        /* first one - just add it to the list */
        m_blocks.insert(std::pair<CTcpSeqKey,CTcpReassBlock>(cur_seq, cur));
        m_max_used = std::max(m_max_used, get_active_blocks());
        return 0;
    }

    auto it = m_blocks.upper_bound(cur_seq);
    /*
     * 'it' points the block next to the current block(cur).
     * so, the current block should be merged to the previous block(--it).
     */
    if ((it == m_blocks.begin()) || !tcp_reass_merge_block(pctx, tp, (--it)->second, cur)) {
        it = m_blocks.insert(it, std::pair<CTcpSeqKey,CTcpReassBlock>(cur_seq, cur));
    }

    /* merge following overlapped blocks: [next..last), 'it' points current block */
    auto next = std::next(it, 1);
    if (next != m_blocks.end()) {
        auto last = next;

        while (last != m_blocks.end()) {
            if (!tcp_reass_merge_block(pctx, tp, it->second, last->second)) {
                break;
            }
            ++last;
        }

        /* remove merged blocks */
        if (next != last) {
            m_blocks.erase(next, last);
        }
    }

    ti->ti_len = it->second.m_len;

    if (m_max_size && get_active_blocks() > m_max_size) {
        /* drop last segment */
        auto last = std::prev(m_blocks.end(), 1);
        if (it == last) {
            ti->ti_len = 0;    /* to update ti_len as dropped */
        }
        INC_STAT(pctx, tg_id, tcps_rcvoopackdrop);
        INC_STAT_CNT(pctx, tg_id, tcps_rcvoobytesdrop, last->second.m_len);

        m_blocks.erase(last);
    }

    m_max_used = std::max(m_max_used, get_active_blocks());
    return 0;
}



int tcp_reass_no_data(CPerProfileCtx * pctx,
                      struct CTcpCb *tp){

    if ( TCPS_HAVERCVDSYN(tp->t_state) == 0 )
        return (0);

    if ( tcp_reass_is_exists(tp)==false ) {
        return (0);
    }

    int res=tp->m_tpc_reass->tcp_reass_no_data(pctx,tp);
    if (tp->m_tpc_reass->get_active_blocks()==0) {
        tcp_reass_free(pctx,tp);
    }
    return (res);
}



int tcp_reass(CPerProfileCtx * pctx,
              struct CTcpCb *tp, 
              struct tcpiphdr *ti, 
              struct rte_mbuf *m){

    if (tcp_reass_is_exists(tp)==false ){
        /* in case of FIN in order with no data there is nothing to do */
        if ( (ti->ti_flags & TH_FIN) &&
             (ti->ti_seq == tp->rcv_nxt) && 
             (tp->t_state > TCPS_ESTABLISHED) && 
             (ti->ti_len==0) ) {
            INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_rcvpack);
            if (m) {
                rte_pktmbuf_free(m);
            }
            return(ti->ti_flags & TH_FIN);
        }
        tcp_reass_alloc(pctx,tp);
    }

    int res=tp->m_tpc_reass->tcp_reass(pctx,tp,ti,m);
    if (tp->m_tpc_reass->get_active_blocks()==0) {
        tcp_reass_free(pctx,tp);
    }
    return (res);
}
#ifdef  TREX_SIM
int tcp_reass(CTcpPerThreadCtx * ctx,
              struct CTcpCb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m) { return tcp_reass(DEFAULT_PROFILE_CTX(ctx), tp, ti, m); }
#endif


bool tcp_reass_is_empty(struct tcpcb *tp) {
    return !tcp_reass_is_exists((CTcpCb*)tp);
}

bool tcp_check_no_delay(struct tcpcb *_tp,int bytes) {
    CTcpCb *tp = static_cast<CTcpCb*>(_tp);

    if ( tp->m_delay_limit == 0 )
        return false;

    if (bytes < 0) {
        tp->t_pkts_cnt = 0;
        return true;
    }

    int pkts_cnt = bytes + tp->t_pkts_cnt;

    if (pkts_cnt >= tp->m_delay_limit) {
        tp->t_pkts_cnt = uint16(pkts_cnt - tp->m_delay_limit);
        if (tp->t_pkts_cnt >= tp->m_delay_limit) {
            tp->t_pkts_cnt = 0;
        }
        return true;
    }

    tp->t_pkts_cnt = uint16(pkts_cnt);
    return false;
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

int
tcp_reass(struct tcpcb *_tp, struct tcphdr *th, tcp_seq *seq_start, int *tlenp, struct mbuf *m)
{
    CTcpCb *tp = static_cast<CTcpCb*>(_tp);
    CPerProfileCtx *pctx = tp->m_flow->m_pctx;
    tcpiphdr pheader;
    tcpiphdr *ti = &pheader;
    int tiflags = 0;

    if (th == nullptr) {
        return tcp_reass_no_data(pctx, tp);
    }
    assert(tlenp != nullptr);

    ti->ti_t     = *th;
    ti->ti_len   = *tlenp; /* L7 len */

    if (tp->m_reass_disabled) {
        uint16_t tg_id = tp->m_flow->m_tg_id;
        INC_STAT(pctx, tg_id, tcps_rcvoopackdrop);
        INC_STAT_CNT(pctx, tg_id, tcps_rcvoobytesdrop,ti->ti_len);
        if (m) {
            rte_pktmbuf_free(m);
        }
        *tlenp = 0;
    } else {
        tiflags = tcp_reass(pctx,tp, ti, m);
        *tlenp = ti->ti_len; /* updated by reassembled data length */
    }
    return tiflags;
}







#ifdef TCPDEBUG

struct CTcpCb *debug_flow; 

#endif


CTcpTuneables * tcp_get_parent_tunable(CPerProfileCtx * pctx,
                                      struct CTcpCb *tp){

        CTcpPerThreadCtx * ctx = pctx->m_ctx;

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
        CAstfTemplatesRW *ctx_temp_rw = pctx->m_template_rw;
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


