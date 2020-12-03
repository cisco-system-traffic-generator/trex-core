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
 *  @(#)tcp_timer.c 8.2 (Berkeley) 5/24/95
 */



#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"


const int   tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32 };
/*64, 64, 64, 64, 64, 64, 64 };*/

const int	tcp_syn_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 1, 1, 2, 2 , 3 };
    /*, 4, 8, 16, 32, 64, 64, 64 };*/


int tcp_totbackoff = 511;   /* sum of tcp_backoff[] */


/*
 * Fast timeout routine for processing delayed acks
 */
void tcp_fasttimo(CPerProfileCtx * pctx, struct tcpcb *tp){

        if (tp->t_flags & TF_DELACK) {
            tp->t_flags &= ~TF_DELACK;
            tp->t_flags |= TF_ACKNOW;
            tp->t_pkts_cnt = 0;
            INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_delack);
            (void) tcp_output(pctx,tp);
        }
}





/*
 * Cancel all timers for TCP tp.
 */
void tcp_canceltimers(struct tcpcb *tp){
    int i;

    for (i = 0; i < TCPT_NTIMERS; i++){
        tp->t_timer[i] = 0;
    }
}





/*
 * TCP timer processing.
 */
struct tcpcb *
tcp_timers(CPerProfileCtx * pctx,struct tcpcb *tp, int timer){
    register int rexmt;
    uint16_t tg_id = tp->m_flow->m_tg_id;
    CTcpTunableCtx * tctx = &pctx->m_tunable_ctx;

    switch (timer) {

    /*
     * 2 MSL timeout in shutdown went off.  If we're closed but
     * still waiting for peer to close and connection has been idle
     * too long, or if 2MSL time is up from TIME_WAIT, delete connection
     * control block.  Otherwise, check again in a bit.
     */
    case TCPT_2MSL:
        if (tp->t_state != TCPS_TIME_WAIT &&
            tp->t_idle <= tctx->tcp_maxidle)
            tp->t_timer[TCPT_2MSL] = tctx->tcp_keepintvl;
        else
            tp = tcp_close(pctx,tp);
        break;

    /*
     * Retransmission timer went off.  Message has not
     * been acked within retransmit interval.  Back off
     * to a longer retransmit interval and retransmit one segment.
     */
    case TCPT_REXMT:
        if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
            tp->t_rxtshift = TCP_MAXRXTSHIFT;
            INC_STAT(pctx, tg_id, tcps_timeoutdrop);
            tp = tcp_drop_now(pctx,tp, tp->t_softerror ?
                tp->t_softerror : TCP_US_ETIMEDOUT);
            break;
        }
        if (tp->t_state < TCPS_ESTABLISHED){
            rexmt = TCP_REXMTVAL(tp) * tcp_syn_backoff[tp->t_rxtshift];
            INC_STAT(pctx, tg_id, tcps_rexmttimeo_syn);
        }else{
            INC_STAT(pctx, tg_id, tcps_rexmttimeo);
            rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
        }

        TCPT_RANGESET(tp->t_rxtcur, rexmt,
            tp->t_rttmin, TCPTV_REXMTMAX);
        tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
        /*
         * If losing, let the lower level know and try for
         * a better route.  Also, if we backed off this far,
         * our srtt estimate is probably bogus.  Clobber it
         * so we'll take the next rtt measurement as our srtt;
         * move the current srtt into rttvar to keep the current
         * retransmit times until then.
         */
        if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
            //in_losing(tp->t_inpcb); # no need that 
            tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
            tp->t_srtt = 0;
        }
        tp->snd_nxt = tp->snd_una;
        /*
         * If timing a segment in this window, stop the timer.
         */
        tp->t_rtt = 0;
        /*
         * Close the congestion window down to one segment
         * (we'll open it by one segment for each ack we get).
         * Since we probably have a window's worth of unacked
         * data accumulated, this "slow start" keeps us from
         * dumping all that data as back-to-back packets (which
         * might overwhelm an intermediate gateway).
         *
         * There are two phases to the opening: Initially we
         * open by one mss on each ack.  This makes the window
         * size increase exponentially with time.  If the
         * window is larger than the path can handle, this
         * exponential growth results in dropped packet(s)
         * almost immediately.  To get more time between 
         * drops but still "push" the network to take advantage
         * of improving conditions, we switch from exponential
         * to linear window opening at some threshhold size.
         * For a threshhold, we use half the current window
         * size, truncated to a multiple of the mss.
         *
         * (the minimum cwnd that will give us exponential
         * growth is 2 mss.  We don't allow the threshhold
         * to go below this.)
         */
        {
        u_int win = bsd_umin(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
        if (win < 2)
            win = 2;
        tp->snd_cwnd = tp->t_maxseg;
        tp->snd_ssthresh = win * tp->t_maxseg;
        tp->t_dupacks = 0;
        }
        (void) tcp_output(pctx,tp);
        break;

    /*
     * Persistance timer into zero window.
     * Force a byte to be output, if possible.
     */
    case TCPT_PERSIST:
        INC_STAT(pctx, tg_id, tcps_persisttimeo);
        /*
         * Hack: if the peer is dead/unreachable, we do not
         * time out if the window is closed.  After a full
         * backoff, drop the connection if the idle time
         * (no responses to probes) reaches the maximum
         * backoff that we would use if retransmitting.
         */
        if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
            (tp->t_idle >= tctx->tcp_maxpersistidle ||
            tp->t_idle >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
            INC_STAT(pctx, tg_id, tcps_persistdrop);
            tp = tcp_drop_now(pctx,tp, TCP_US_ETIMEDOUT);
            break;
        }
        tcp_setpersist(pctx,tp);
        tp->t_force = 1;
        (void) tcp_output(pctx,tp);
        tp->t_force = 0;
        break;

    /*
     * Keep-alive timer went off; send something
     * or drop connection if idle for too long.
     */
    case TCPT_KEEP:
        INC_STAT(pctx, tg_id, tcps_keeptimeo);
        if (tp->t_state < TCPS_ESTABLISHED)
            goto dropit;
        if (tp->m_socket.so_options & US_SO_KEEPALIVE) {
                if (tp->t_idle >= tctx->tcp_keepidle + tctx->tcp_maxidle)
                goto dropit;
            /*
             * Send a packet designed to force a response
             * if the peer is up and reachable:
             * either an ACK if the connection is still alive,
             * or an RST if the peer has closed the connection
             * due to timeout or reboot.
             * Using sequence number tp->snd_una-1
             * causes the transmitted zero-length segment
             * to lie outside the receive window;
             * by the protocol spec, this requires the
             * correspondent TCP to respond.
             */
            INC_STAT(pctx, tg_id, tcps_keepprobe);
            tcp_respond(pctx,tp,
                tp->rcv_nxt, tp->snd_una - 1, TH_ACK);
            tp->t_timer[TCPT_KEEP] = tctx->tcp_keepintvl;
        } else
            tp->t_timer[TCPT_KEEP] = tctx->tcp_keepidle;
        break;
    dropit:
        INC_STAT(pctx, tg_id, tcps_keepdrops);
        tp = tcp_drop_now(pctx,tp, TCP_US_ETIMEDOUT);
        break;
    }
    return (tp);
}



/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates the timers in all active tcb's and
 * causes finite state machine actions if timers expire.
 */
void tcp_slowtimo(CPerProfileCtx * pctx, struct tcpcb *tp)
{
    int i;

    if (tp->t_state == TCPS_LISTEN)
        return ;

    for (i = 0; i < TCPT_NTIMERS; i++) {
        if (tp->t_timer[i] && --tp->t_timer[i] == 0) {
            tcp_timers(pctx,tp, i);
            if (tp->t_state == TCPS_CLOSED) {
                return;
            }
        }
    }
    tp->t_idle++;
    if (tp->t_rtt){
        tp->t_rtt++;
    }
}

