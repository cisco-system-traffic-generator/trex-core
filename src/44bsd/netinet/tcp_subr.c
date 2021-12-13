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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */


#include "sys_inet.h"
#include "tcp_int.h"

/* tcp_debug.c */
extern void tcp_trace(short, short, struct tcpcb *, void *, struct tcphdr *, int);
/* tcp_output.c */
extern int tcp_output(struct tcpcb *);
extern int tcp_addoptions(struct tcpcb *, struct tcpopt *, u_char *);
extern void tcp_setpersist(struct tcpcb *);
/* tcp_inpput.c */
extern void tcp_do_segment(struct mbuf *, struct tcphdr *, struct socket *, struct tcpcb *, int, int, uint8_t);
extern int tcp_mssopt(struct tcpcb *);
/* tcp_timer.c */
extern int tcp_timer_active(struct tcpcb *, uint32_t);
extern void tcp_timer_activate(struct tcpcb *, uint32_t, u_int);
extern void tcp_timer_stop(struct tcpcb *, uint32_t);
/* tcp_sack.c */
void tcp_free_sackholes(struct tcpcb *tp);

/* defined functions */
void tcp_state_change(struct tcpcb *tp, int newstate);
struct tcpcb * tcp_close(struct tcpcb *tp);
void tcp_discardcb(struct tcpcb *tp);
struct tcpcb * tcp_drop(struct tcpcb *tp, int errno);
u_int tcp_maxseg(const struct tcpcb *tp);
void tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m, tcp_seq ack, tcp_seq seq, int flags);
void tcp_timer_discard(void *ptp);

#define tcp_ts_getticks()   tcp_getticks(tp)
#define ticks               tcp_getticks(tp)


static int	tcp_default_fb_init(struct tcpcb *tp);
static void	tcp_default_fb_fini(struct tcpcb *tp, int tcb_is_purged);

static struct tcp_function_block tcp_def_funcblk = {
	.tfb_tcp_block_name = "freebsd",
	.tfb_tcp_output = tcp_output,
	.tfb_tcp_do_segment = tcp_do_segment,
	.tfb_tcp_fb_init = tcp_default_fb_init,
	.tfb_tcp_fb_fini = tcp_default_fb_fini,
};


/*
 * tfb_tcp_fb_init() function for the default stack.
 *
 * This handles making sure we have appropriate timers set if you are
 * transitioning a socket that has some amount of setup done.
 *
 * The init() fuction from the default can *never* return non-zero i.e.
 * it is required to always succeed since it is the stack of last resort!
 */
static int
tcp_default_fb_init(struct tcpcb *tp)
{

	struct socket *so;

	KASSERT(tp->t_state >= 0 && tp->t_state < TCPS_TIME_WAIT,
	    ("%s: connection %p in unexpected state %d", __func__, tp,
	    tp->t_state));

	/*
	 * Nothing to do for ESTABLISHED or LISTEN states. And, we don't
	 * know what to do for unexpected states (which includes TIME_WAIT).
	 */
	if (tp->t_state <= TCPS_LISTEN || tp->t_state >= TCPS_TIME_WAIT)
		return (0);

	/*
	 * Make sure some kind of transmission timer is set if there is
	 * outstanding data.
	 */
	so = tcp_getsocket(tp);
	if ((!TCPS_HAVEESTABLISHED(tp->t_state) || sbavail(&so->so_snd) ||
	    tp->snd_una != tp->snd_max) && !(tcp_timer_active(tp, TT_REXMT) ||
	    tcp_timer_active(tp, TT_PERSIST))) {
		/*
		 * If the session has established and it looks like it should
		 * be in the persist state, set the persist timer. Otherwise,
		 * set the retransmit timer.
		 */
		if (TCPS_HAVEESTABLISHED(tp->t_state) && tp->snd_wnd == 0 &&
		    (int32_t)(tp->snd_nxt - tp->snd_una) <
		    (int32_t)sbavail(&so->so_snd))
			tcp_setpersist(tp);
		else
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);
	}

	/* All non-embryonic sessions get a keepalive timer. */
	if (!tcp_timer_active(tp, TT_KEEP))
		tcp_timer_activate(tp, TT_KEEP,
		    TCPS_HAVEESTABLISHED(tp->t_state) ? TP_KEEPIDLE(tp) :
		    TP_KEEPINIT(tp));

	/*
	 * Make sure critical variables are initialized
	 * if transitioning while in Recovery.
	 */
	if IN_FASTRECOVERY(tp->t_flags) {
		if (tp->sackhint.recover_fs == 0)
			tp->sackhint.recover_fs = max(1,
			    tp->snd_nxt - tp->snd_una);
	}

	return (0);
}

/*
 * tfb_tcp_fb_fini() function for the default stack.
 *
 * This changes state as necessary (or prudent) to prepare for another stack
 * to assume responsibility for the connection.
 */
static void
tcp_default_fb_fini(struct tcpcb *tp, int tcb_is_purged)
{
	return;
}


#ifdef TREX_FBSD
void
tcp_int_respond(struct tcpcb *tp, tcp_seq ack, tcp_seq seq, int flags)
{
	tcp_respond(tp, NULL, NULL, NULL, ack, seq, flags);
}
#endif
/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == NULL, then we make a copy
 * of the tcpiphdr at th and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection.  If flags are given then we send
 * a message back to the TCP which originated the segment th,
 * and discard the mbuf containing it and any other attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * NOTE: If m != NULL, then th must point to *inside* the mbuf.
 */
void
tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m,
    tcp_seq ack, tcp_seq seq, int flags)
{
	struct tcpopt to;
	struct tcphdr *nth;
	u_char *optp;
#ifdef INET6
#ifdef TCPDEBUG
	int isipv6;
#endif
#endif /* INET6 */
	int optlen, tlen, win;
	bool incl_opts;

	KASSERT(tp != NULL || m != NULL, ("tcp_respond: tp and m both NULL"));

#ifdef TCPDEBUG
	isipv6 = tcp_isipv6(tp);
#endif

	incl_opts = false;
	win = 0;
	if (tp != NULL) {
		{
			win = sbspace(&tcp_getsocket(tp)->so_rcv);
			if (win > TCP_MAXWIN << tp->rcv_scale)
				win = TCP_MAXWIN << tp->rcv_scale;
		}
		if ((tp->t_flags & TF_NOOPT) == 0)
			incl_opts = true;
	}
	if (m) {
		m_freem(m);
	}
	if (flags & TH_ACK)
		TCPSTAT_INC(tcps_sndacks);
	else if (flags & (TH_SYN|TH_FIN|TH_RST))
		TCPSTAT_INC(tcps_sndctrl);
	TCPSTAT_INC(tcps_sndtotal);

	tlen = sizeof (struct tcphdr);
	to.to_flags = 0;

        u_char opt[TCP_MAXOLEN];
        optp = &opt[0];
	if (incl_opts) {
#ifdef TREX_FBSD /* support SYN respose, from syncache_respond */
                if (flags & TH_SYN) {
                        to.to_mss = tcp_mssopt(tp);
                        to.to_flags = TOF_MSS;
                        if (tp->t_flags & TF_REQ_SCALE) {
                                to.to_wscale = tp->request_r_scale;
                                to.to_flags |= TOF_SCALE;
                        }
                        if (tp->t_flags & TF_SACK_PERMIT)
                                to.to_flags |= TOF_SACKPERM;
                }
#endif
		/* Timestamps. */
		if (tp->t_flags & TF_RCVD_TSTMP) {
			to.to_tsval = tcp_ts_getticks() + tp->ts_offset;
			to.to_tsecr = tp->ts_recent;
			to.to_flags |= TOF_TS;
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		/* TCP-MD5 (RFC2385). */
		if (tp->t_flags & TF_SIGNATURE)
			to.to_flags |= TOF_SIGNATURE;
#endif
		/* Add the options. */
		tlen += optlen = tcp_addoptions(tp, &to, optp);
	} else
		optlen = 0;

        if (tcp_build_pkt(tp, 0, 0, tlen, optlen, &m, &nth) != 0) {
                return;
        }
	if (optlen) {
		bcopy(opt, nth + 1, optlen);
	}

	nth->th_seq = htonl(seq);
	nth->th_ack = htonl(ack);
	nth->th_x2 = 0;
	nth->th_off = (sizeof (struct tcphdr) + optlen) >> 2;
	nth->th_flags = flags;
	if (tp != NULL)
		nth->th_win = htons((u_short) (win >> tp->rcv_scale));
	else
		nth->th_win = htons((u_short)win);
	nth->th_urp = 0;

#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (to.to_flags & TOF_SIGNATURE) {
		if (!TCPMD5_ENABLED() ||
		    TCPMD5_OUTPUT(m, nth, to.to_signature) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

#ifdef TCPDEBUG
        ipgen = ((void *)nth) - (isipv6 ? sizeof(struct ip6_hdr) : sizeof(struct ip));
	if (tp == NULL || (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_RESPOND, tp->t_state, tp, ipgen, nth, 0);
#endif
	tcp_ip_output(tp, m);
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.  The `inp' parameter must have
 * come from the zone allocator set up in tcp_init().
 */
struct tcpcb *
tcp_inittcpcb(struct tcpcb *tp, struct tcp_function_block *fb, struct cc_algo *cc_algo, struct tcp_tune *tune, struct tcpstat *stat)
{
#ifdef INET6
	int isipv6 = tcp_isipv6(tp);
#endif /* INET6 */

	/* Initialise cc_var struct for this tcpcb. */
	tp->ccv = &tp->m_ccv;
	tp->ccv->type = IPPROTO_TCP;
	tp->ccv->ccvc.tcp = tp;

	tp->t_fb = fb ? fb : &tcp_def_funcblk;
        tp->t_tune = tune;
        tp->t_stat = stat;
	/*
	 * Use the current system default CC algorithm.
	 */
	CC_ALGO(tp) = cc_algo;

	if (CC_ALGO(tp)->cb_init != NULL)
		if (CC_ALGO(tp)->cb_init(tp->ccv) > 0) {
			if (tp->t_fb->tfb_tcp_fb_fini)
				(*tp->t_fb->tfb_tcp_fb_fini)(tp, 1);
			return (NULL);
		}

	tp->t_maxseg =
#ifdef INET6
		isipv6 ? V_tcp_v6mssdflt :
#endif /* INET6 */
		V_tcp_mssdflt;

        tcp_handle_timers(tp);  /* initial update of last_tick */

	if (V_tcp_do_rfc1323)
		tp->t_flags = (TF_REQ_SCALE|TF_REQ_TSTMP);
	if (V_tcp_do_sack)
		tp->t_flags |= TF_SACK_PERMIT;
	TAILQ_INIT(&tp->snd_holes);

	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 4 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = ((tcp_rexmit_initial - TCPTV_SRTTBASE) << TCP_RTTVAR_SHIFT) / 4;
	tp->t_rttmin = tcp_rexmit_min;
	tp->t_rxtcur = tcp_rexmit_initial;
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->t_rcvtime = ticks;
	if (tp->t_fb->tfb_tcp_fb_init) {
		if ((*tp->t_fb->tfb_tcp_fb_init)(tp)) {
			return (NULL);
		}
	}
	return (tp);		/* XXX */
}


/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tcp_getsocket(tp);

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tcp_state_change(tp, TCPS_CLOSED);
		(void) tp->t_fb->tfb_tcp_output(tp);
		TCPSTAT_INC(tcps_drops);
	} else
		TCPSTAT_INC(tcps_conndrops);
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

void
tcp_discardcb(struct tcpcb *tp)
{
	int released __unused;

#ifdef TREX_FBSD
        if (tp->t_fb == NULL) {
                return;
        }
#endif

	/*
	 * Make sure that all of our timers are stopped before we delete the
	 * PCB.
	 *
	 * If stopping a timer fails, we schedule a discard function in same
	 * callout, and the last discard function called will take care of
	 * deleting the tcpcb.
	 */
	tcp_timer_stop(tp, TT_REXMT);
	tcp_timer_stop(tp, TT_PERSIST);
	tcp_timer_stop(tp, TT_KEEP);
	tcp_timer_stop(tp, TT_2MSL);
	tcp_timer_stop(tp, TT_DELACK);
	if (tp->t_fb->tfb_tcp_timer_stop_all) {
		/*
		 * Call the stop-all function of the methods,
		 * this function should call the tcp_timer_stop()
		 * method with each of the function specific timeouts.
		 * That stop will be called via the tfb_tcp_timer_stop()
		 * which should use the async drain function of the
		 * callout system (see tcp_var.h).
		 */
		tp->t_fb->tfb_tcp_timer_stop_all(tp);
	}

	tcp_free_sackholes(tp);

	/* Allow the CC algorithm to clean up after itself. */
	if (CC_ALGO(tp)->cb_destroy != NULL)
		CC_ALGO(tp)->cb_destroy(tp->ccv);
	CC_DATA(tp) = NULL;
	CC_ALGO(tp) = NULL;
}

/*
 * Attempt to close a TCP control block, marking it as dropped, and freeing
 * the socket if we hold the only reference.
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct socket *so;

#ifdef TCP_OFFLOAD
	if (tp->t_state == TCPS_LISTEN)
		tcp_offload_listen_stop(tp);
#endif
	TCPSTAT_INC(tcps_closed);
	if (tp->t_state != TCPS_CLOSED)
		tcp_state_change(tp, TCPS_CLOSED);
	so = tcp_getsocket(tp);
	soisdisconnected(so);
	return (tp);
}


/*
 * Calculate effective SMSS per RFC5681 definition for a given TCP
 * connection at its current state, taking into account SACK and etc.
 */
u_int
tcp_maxseg(const struct tcpcb *tp)
{
	u_int optlen;

	if (tp->t_flags & TF_NOOPT)
		return (tp->t_maxseg);

	/*
	 * Here we have a simplified code from tcp_addoptions(),
	 * without a proper loop, and having most of paddings hardcoded.
	 * We might make mistakes with padding here in some edge cases,
	 * but this is harmless, since result of tcp_maxseg() is used
	 * only in cwnd and ssthresh estimations.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (tp->t_flags & TF_RCVD_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = 0;
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PADTCPOLEN(TCPOLEN_SIGNATURE);
#endif
		if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks > 0) {
			optlen += TCPOLEN_SACKHDR;
			optlen += tp->rcv_numsacks * TCPOLEN_SACK;
			optlen = PADTCPOLEN(optlen);
		}
	} else {
		if (tp->t_flags & TF_REQ_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = PADTCPOLEN(TCPOLEN_MAXSEG);
		if (tp->t_flags & TF_REQ_SCALE)
			optlen += PADTCPOLEN(TCPOLEN_WINDOW);
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PADTCPOLEN(TCPOLEN_SIGNATURE);
#endif
		if (tp->t_flags & TF_SACK_PERMIT)
			optlen += PADTCPOLEN(TCPOLEN_SACK_PERMITTED);
	}
#undef PAD
	optlen = min(optlen, TCP_MAXOLEN);
	return (tp->t_maxseg - optlen);
}


/*
 * A subroutine which makes it easy to track TCP state changes with DTrace.
 * This function shouldn't be called for t_state initializations that don't
 * correspond to actual TCP state transitions.
 */
void
tcp_state_change(struct tcpcb *tp, int newstate)
{
	tp->t_state = newstate;
}

