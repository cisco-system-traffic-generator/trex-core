/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)tcp_timer.c	8.2 (Berkeley) 5/24/95
 */


#include "sys_inet.h"
#include "tcp_var.h"

#include "tcp_debug.h"

/* tcp_debug.c */
extern void tcp_trace(short, short, struct tcpcb *, void *, struct tcphdr *, int);
/* tcp_sack.c */
extern void tcp_free_sackholes(struct tcpcb *);
/* tcp_output.c */
extern void tcp_setpersist(struct tcpcb *);
/* tcp_input.c */
extern void cc_cong_signal(struct tcpcb *tp, struct tcphdr *th, uint32_t type);
/* tcp_subr.c */
extern struct tcpcb * tcp_drop(struct tcpcb *, int);
extern struct tcpcb * tcp_close(struct tcpcb *);
extern void tcp_respond(struct tcpcb *, void *, struct tcphdr *, struct mbuf *, tcp_seq, tcp_seq, int);
extern void tcp_timer_discard(void *);

/* defined functions */
#if 0
void tcp_timer_2msl(struct tcpcb *tp);
void tcp_timer_keep(struct tcpcb *tp);
void tcp_timer_persist(struct tcpcb *tp);
void tcp_timer_rexmt(struct tcpcb *tp);
void tcp_timer_delack(struct tcpcb *tp);

void tcp_timer_activate(struct tcpcb *tp, uint32_t timer_type, u_int delta);
int tcp_timer_active(struct tcpcb *tp, uint32_t timer_type);
void tcp_cancel_timers(struct tcpcb *tp);
#else
bool tcp_handle_timers(struct tcpcb *tp);
#endif

#define tcp_maxpersistidle  TCPTV_KEEP_IDLE
#define ticks   tcp_getticks(tp)



const int tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32/*, 64, 128, 256, 512, 512, 512, 512*/ };

const int tcp_syn_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 1, 1, 2, 2 , 3/*, 4, 8, 16, 32, 64, 64, 64*/ };

const int tcp_totbackoff = 511;	/* sum of tcp_backoff[] */

/*
 * TCP timer processing.
 */

static inline void
tcp_timer_delack(struct tcpcb *tp)
{
	tp->t_flags |= TF_ACKNOW;
	TCPSTAT_INC(tcps_delack);
	(void) tp->t_fb->tfb_tcp_output(tp);
}

static inline void
tcp_timer_2msl(struct tcpcb *tp)
{
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	KASSERT(inp != NULL, ("%s: tp %p tp->t_inpcb == NULL", __func__, tp));
	tcp_free_sackholes(tp);
	if (ticks - tp->t_rcvtime < TCPTV_2MSL) {
		tcp_timer_activate(tp, TT_2MSL, TCPTV_2MSL - (ticks - tp->t_rcvtime));
	} else {
		tcp_close(tp);
		return;
	}

#ifdef TCPDEBUG
	if (tp != NULL && (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0, PRU_SLOWTIMO|(TT_2MSL<<8));
#endif
}

static inline void
tcp_timer_keep(struct tcpcb *tp)
{
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif

	/*
	 * Because we don't regularly reset the keepalive callout in
	 * the ESTABLISHED state, it may be that we don't actually need
	 * to send a keepalive yet. If that occurs, schedule another
	 * call for the next time the keepalive timer might expire.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		u_int idletime;

		idletime = ticks - tp->t_rcvtime;
		if (idletime < TP_KEEPIDLE(tp)) {
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp) - idletime);
			return;
		}
	}

	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	TCPSTAT_INC(tcps_keeptimeo);
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((V_tcp_always_keepalive ||
	    tcp_getsocket(tp)->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if (ticks - tp->t_rcvtime >= TP_KEEPIDLE(tp) + TP_MAXIDLE(tp))
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
		TCPSTAT_INC(tcps_keepprobe);
		tcp_respond(tp, NULL, (struct tcphdr *)NULL, (struct mbuf *)NULL,
			    tp->rcv_nxt, tp->snd_una - 1, TH_ACK);
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPINTVL(tp));
	} else
		tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));

#ifdef TCPDEBUG
	if (tcp_getsocket(tp)->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0, PRU_SLOWTIMO|(TT_KEEP<<8));
#endif
	return;

dropit:
	TCPSTAT_INC(tcps_keepdrops);
	tp = tcp_drop(tp, ETIMEDOUT);

#ifdef TCPDEBUG
	if (tp != NULL && (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0, PRU_SLOWTIMO|(TT_KEEP<<8));
#endif
}

static inline void
tcp_timer_persist(struct tcpcb *tp)
{
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	/*
	 * Persistence timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	TCPSTAT_INC(tcps_persisttimeo);
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
	    (ticks - tp->t_rcvtime >= tcp_maxpersistidle ||
	     ticks - tp->t_rcvtime >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
		TCPSTAT_INC(tcps_persistdrop);
		goto out;
	}
	/*
	 * If the user has closed the socket then drop a persisting
	 * connection after a much reduced timeout.
	 */
	if (tp->t_state > TCPS_CLOSE_WAIT &&
	    (ticks - tp->t_rcvtime) >= TCPTV_PERSMAX) {
		TCPSTAT_INC(tcps_persistdrop);
		tp = tcp_drop(tp, ETIMEDOUT);
		goto out;
	}
	tcp_setpersist(tp);
	tp->t_flags |= TF_FORCEDATA;
	(void) tp->t_fb->tfb_tcp_output(tp);
	tp->t_flags &= ~TF_FORCEDATA;

out:
	tp = tcp_drop(tp, ETIMEDOUT);
#ifdef TCPDEBUG
	if (tp != NULL && (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO|(TT_PERSIST<<8));
#endif
	return;
}

static inline void
tcp_timer_rexmt(struct tcpcb *tp)
{
	int rexmt;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	tcp_free_sackholes(tp);
	if (tp->t_fb->tfb_tcp_rexmit_tmr) {
		/* The stack has a timer action too. */
		(*tp->t_fb->tfb_tcp_rexmit_tmr)(tp);
	}
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		TCPSTAT_INC(tcps_timeoutdrop);
		tp = tcp_drop(tp, ETIMEDOUT);
		goto out;
	}
	if (tp->t_state == TCPS_SYN_SENT) {
		/*
		 * If the SYN was retransmitted, indicate CWND to be
		 * limited to 1 segment in cc_conn_init().
		 */
		tp->snd_cwnd = 1;
	} else if (tp->t_rxtshift == 1) {
		/*
		 * first retransmit; record ssthresh and cwnd so they can
		 * be recovered if this turns out to be a "bad" retransmit.
		 * A retransmit is considered "bad" if an ACK for this
		 * segment is received within RTT/2 interval; the assumption
		 * here is that the ACK was already in flight.  See
		 * "On Estimating End-to-End Network Path Properties" by
		 * Allman and Paxson for more details.
		 */
		tp->snd_cwnd_prev = tp->snd_cwnd;
		tp->snd_ssthresh_prev = tp->snd_ssthresh;
		tp->snd_recover_prev = tp->snd_recover;
		if (IN_FASTRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASFRECOVERY;
		else
			tp->t_flags &= ~TF_WASFRECOVERY;
		if (IN_CONGRECOVERY(tp->t_flags))
			tp->t_flags |= TF_WASCRECOVERY;
		else
			tp->t_flags &= ~TF_WASCRECOVERY;
		if ((tp->t_flags & TF_RCVD_TSTMP) == 0)
			tp->t_badrxtwin = ticks + (tp->t_srtt >> (TCP_RTT_SHIFT + 1));
		/* In the event that we've negotiated timestamps
		 * badrxtwin will be set to the value that we set
		 * the retransmitted packet's to_tsval to by tcp_output
		 */
		tp->t_flags |= TF_PREVVALID;
	} else
		tp->t_flags &= ~TF_PREVVALID;
	if ((tp->t_state == TCPS_SYN_SENT) ||
	    (tp->t_state == TCPS_LISTEN) ||
	    (tp->t_state == TCPS_SYN_RECEIVED)) {
		rexmt = tcp_rexmit_initial * tcp_syn_backoff[tp->t_rxtshift];
		TCPSTAT_INC(tcps_rexmttimeo_syn);
	} else {
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
		TCPSTAT_INC(tcps_rexmttimeo);
	}
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);

	/* TREX_FBSD: to alter the syncache_timer */
	if (tp->t_state == TCPS_LISTEN) {
		tcp_respond(tp, NULL, (struct tcphdr *)NULL, (struct mbuf *)NULL, tp->irs + 1, tp->iss, TH_ACK|TH_SYN);
		tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);
#ifdef TCPDEBUG
		if (tp != NULL && (tcp_getsocket(tp)->so_options & SO_DEBUG))
			tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0, PRU_SLOWTIMO|(TT_REXMT<<8));
#endif
		return;
	}

	tp->snd_nxt = tp->snd_una;
	tp->snd_recover = tp->snd_max;
	/*
	 * Force a segment to be sent.
	 */
	tp->t_flags |= TF_ACKNOW;
	/*
	 * If timing a segment in this window, stop the timer.
	 */
	tp->t_rtttime = 0;

	cc_cong_signal(tp, NULL, CC_RTO);
	(void) tp->t_fb->tfb_tcp_output(tp);

out:
#ifdef TCPDEBUG
	if (tp != NULL && (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (void *)0, (struct tcphdr *)0, PRU_SLOWTIMO|(TT_REXMT<<8));
#endif
	return;
}


void
tcp_timer_activate(struct tcpcb *tp, uint32_t timer_type, u_int delta)
{
	if (delta == 0) {
		tp->m_timer.tt_flags &= ~(1 << timer_type);
	} else {
		tp->m_timer.tt_flags |= (1 << timer_type);
		delta += (ticks - tp->m_timer.last_tick);
	}
	tp->m_timer.tt_timer[timer_type] = delta;
}

int
tcp_timer_active(struct tcpcb *tp, uint32_t timer_type)
{
	return (tp->m_timer.tt_flags & (1 << timer_type));
}

void
tcp_timer_stop(struct tcpcb *tp, uint32_t timer_type)
{
	tp->m_timer.tt_flags &= ~(1 << timer_type);
}

void
tcp_cancel_timers(struct tcpcb *tp)
{
	tp->m_timer.tt_flags = 0;
}


bool
tcp_handle_timers(struct tcpcb *tp)
{
	uint32_t tt_flags = tp->m_timer.tt_flags & ((1 << TCPT_NTIMERS) - 1);
	uint32_t now_tick = ticks;
	uint32_t tick_passed = now_tick - tp->m_timer.last_tick;
	bool is_delack = false;

	/* handle fast timer: TT_DELACK */
	if ((tt_flags & TT_FLAG_DELACK)) {
		tt_flags &= ~TT_FLAG_DELACK;
		if (tp->m_timer.tt_timer[TT_DELACK] <= tick_passed) {
			tp->m_timer.tt_flags &= ~TT_FLAG_DELACK;
			tp->m_timer.tt_timer[TT_DELACK] = 0;
			tcp_timer_delack(tp);
			is_delack = true;
		}
	}

	if (tick_passed < TCPTV_SLOWTIMO) {
		return is_delack;
	}
	/* handle slow timer only */
	tp->m_timer.last_tick = now_tick;

	for (int i = TT_REXMT; tt_flags && i < TCPT_NTIMERS; i++ ) {
		if ((tt_flags & (1 << i)) == 0)
			continue;
		tt_flags &= ~(1 << i);

		if (tp->m_timer.tt_timer[i] <= tick_passed) {
			tp->m_timer.tt_flags &= ~(1 << i);
			tp->m_timer.tt_timer[i] = 0;
			switch(i) {
			case TT_REXMT: tcp_timer_rexmt(tp); break;
			case TT_PERSIST: tcp_timer_persist(tp); break;
			case TT_KEEP: tcp_timer_keep(tp); break;
			case TT_2MSL: tcp_timer_2msl(tp); break;
			}
		} else {
			tp->m_timer.tt_timer[i] -= tick_passed;
		}
	}
	return is_delack;
}

