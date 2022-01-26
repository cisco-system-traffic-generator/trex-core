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
#include "tcp_var.h"

#include "tcp_mbuf.h"
#include "tcp_debug.h"

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
extern void tcp_free_sackholes(struct tcpcb *tp);

/* defined functions */
void tcp_state_change(struct tcpcb *tp, int newstate);
struct tcpcb * tcp_close(struct tcpcb *tp);
void tcp_discardcb(struct tcpcb *tp);
struct tcpcb * tcp_drop(struct tcpcb *tp, int res);
u_int tcp_maxseg(const struct tcpcb *tp);
void tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m, tcp_seq ack, tcp_seq seq, int flags);


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
	int optlen, tlen, win;
	bool incl_opts;

	KASSERT(tp != NULL || m != NULL, ("tcp_respond: tp and m both NULL"));

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
		/* TREX_FBSD: to support SYN response, from syncache_respond */
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
		/* Timestamps. */
		if (tp->t_flags & TF_RCVD_TSTMP) {
			to.to_tsval = tcp_ts_getticks() + tp->ts_offset;
			to.to_tsecr = tp->ts_recent;
			to.to_flags |= TOF_TS;
		}
		/* Add the options. */
		tlen += optlen = tcp_addoptions(tp, &to, optp);
	} else
		optlen = 0;

	struct tcp_pkt pkt;
	pkt.m_optlen = optlen;
	if (tcp_build_cpkt(tp, tlen, &pkt) != 0) {
		return;
	}
	m = (struct mbuf *)pkt.m_buf;
	nth = (struct tcphdr *)pkt.lpTcp;

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

#ifdef TCPDEBUG
	if (tp == NULL || (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_RESPOND, tp->t_state, tp, NULL, nth, 0);
#endif
	tcp_ip_output(tp, m, 0);
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.  The `inp' parameter must have
 * come from the zone allocator set up in tcp_init().
 */
struct tcpcb *
tcp_inittcpcb(struct tcpcb *tp, struct tcpcb_param *param)
{
	/* Initialise cc_var struct for this tcpcb. */
	tp->ccv = &tp->m_ccv;
	tp->ccv->type = IPPROTO_TCP;
	tp->ccv->ccvc.tcp = tp;

	tp->t_fb = param->fb ? param->fb : &tcp_def_funcblk;
	tp->t_tune = param->tune;
	tp->t_stat = param->stat;
	tp->t_stat_ex = param->stat_ex;

	tp->m_timer.now_tick = param->tcp_ticks;
	tp->m_timer.last_tick = tcp_getticks(tp);

	/*
	 * Use the current system default CC algorithm.
	 */
	CC_ALGO(tp) = param->cc_algo;

	if (CC_ALGO(tp)->cb_init != NULL)
		if (CC_ALGO(tp)->cb_init(tp->ccv) > 0) {
			if (tp->t_fb->tfb_tcp_fb_fini)
				(*tp->t_fb->tfb_tcp_fb_fini)(tp, 1);
			return (NULL);
		}

	tp->t_maxseg = V_tcp_mssdflt;

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
	tp->t_rcvtime = tcp_getticks(tp);
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
tcp_drop(struct tcpcb *tp, int res)
{
	struct socket *so = tcp_getsocket(tp);

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tcp_state_change(tp, TCPS_CLOSED);
		(void) tp->t_fb->tfb_tcp_output(tp);
		TCPSTAT_INC(tcps_drops);
	} else
		TCPSTAT_INC(tcps_conndrops);
	if (res == ETIMEDOUT && tp->t_softerror)
		res = tp->t_softerror;
	so->so_error = res;
	return (tcp_close(tp));
}

void
tcp_discardcb(struct tcpcb *tp)
{
	if (tp->t_fb == NULL) {
		return;
	}

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
		if (tp->t_flags & TF_SACK_PERMIT)
			optlen += PADTCPOLEN(TCPOLEN_SACK_PERMITTED);
	}
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


/* TREX_FBSD: from tcp_usrreq.c */

#include "tcp_seq.h"

/*
 * TCP protocol interface to socket abstraction.
 */


int
tcp_listen(struct tcpcb *tp)
{
	assert(tp->t_state == TCPS_CLOSED);
	tcp_state_change(tp, TCPS_LISTEN);
	return(0);
}


/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct sockaddr_in in mbuf *nam.  Call in_pcbbind to assign a local
 * port number if needed.  Call in_pcbconnect_setup to do the routing and
 * to choose a local host address (interface).  If there is an existing
 * incarnation of the same connection in TIME-WAIT state and if the remote
 * host was sending CC options and if the connection duration was < MSL, then
 * truncate the previous TIME-WAIT state and proceed.
 * Initialize connection parameters and enter SYN-SENT state.
 */
int
tcp_connect(struct tcpcb *tp)
{
	struct socket *so = tcp_getsocket(tp);
	int error;

	/*
	 * Compute window scaling to request:
	 * Scale to fit into sweet spot.  See tcp_syncache.c.
	 * XXX: This should move to tcp_output().
	 */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT_INC(tcps_connattempt);
	tcp_state_change(tp, TCPS_SYN_SENT);
	tp->iss = tcp_new_isn(tp);
	if (tp->t_flags & TF_REQ_TSTMP)
		tp->ts_offset = 0;
	tcp_sendseqinit(tp);

	/* TREX_FBSD: added for old stack comapatibility */
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	error = tp->t_fb->tfb_tcp_output(tp);

	return error;
}


/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
void
tcp_disconnect(struct tcpcb *tp)
{
#if 0
	struct socket *so = tcp_getsocket(tp);
#endif

	/*
	 * Neither tcp_close() nor tcp_drop() should return NULL, as the
	 * socket is still open.
	 */
	if (tp->t_state < TCPS_ESTABLISHED &&
	    !(tp->t_state > TCPS_LISTEN /*&& IS_FASTOPEN(tp->t_flags)*/)) {
		tp = tcp_close(tp);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_close() returned NULL"));
	} else /*if ((so->so_options & SO_LINGER) && so->so_linger == 0)*/ {
		tp = tcp_drop(tp, 0);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_drop() returned NULL"));
#if 0
	} else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tcp_usrclosed(tp);
		if (tp->t)state != TCPS_CLOSED)
			tp->t_fb->tfb_tcp_output(tp);
#endif
	}
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
void
tcp_usrclosed(struct tcpcb *tp)
{
	switch (tp->t_state) {
	case TCPS_LISTEN:
		tcp_state_change(tp, TCPS_CLOSED);
		/* FALLTHROUGH */
	case TCPS_CLOSED:
		tp = tcp_close(tp);
		/*
		 * tcp_close() should never return NULL here as the socket is
		 * still open.
		 */
		KASSERT(tp != NULL,
		    ("tcp_usrclosed: tcp_close() returned NULL"));
		break;

	case TCPS_SYN_SENT:
	case TCPS_SYN_RECEIVED:
		tp->t_flags |= TF_NEEDFIN;
		break;

	case TCPS_ESTABLISHED:
		tcp_state_change(tp, TCPS_FIN_WAIT_1);
		break;

	case TCPS_CLOSE_WAIT:
		tcp_state_change(tp, TCPS_LAST_ACK);
		break;
	}
	if (tp->t_state >= TCPS_FIN_WAIT_2) {
		soisdisconnected(tcp_getsocket(tp));
		/* Prevent the connection hanging in FIN_WAIT_2 forever. */
		if (tp->t_state == TCPS_FIN_WAIT_2) {
			tcp_timer_activate(tp, TT_2MSL, TCPTV_2MSL);
		}
	}
}

