/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007-2008,2010
 *	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart,
 * James Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 */


#include "sys_inet.h"
#include "tcp_var.h"

#include "tcp_mbuf.h"
#include "tcp_seq.h"
#include "tcp_debug.h"

/* tcp_debug.c */
extern void tcp_trace(short, short, struct tcpcb *, void *, struct tcphdr *, int);
/* tcp_sack.c */
extern void tcp_clean_sackreport(struct tcpcb *tp);
extern void tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_laststart, tcp_seq rcv_lastend);
extern int tcp_sack_doack(struct tcpcb *, struct tcpopt *, tcp_seq);
extern void tcp_sack_partialack(struct tcpcb *, struct tcphdr *);
extern void tcp_update_dsack_list(struct tcpcb *, tcp_seq, tcp_seq);
/* tcp_output.c */
extern int tcp_output(struct tcpcb *);

/* tcp_timer.c */
extern void tcp_timer_activate(struct tcpcb *, uint32_t, u_int);
extern int tcp_timer_active(struct tcpcb *, uint32_t);
extern void tcp_cancel_timers(struct tcpcb *);
/* tcp_subr.c */
extern u_int tcp_maxseg(const struct tcpcb *);
extern void tcp_state_change(struct tcpcb *, int);
extern struct tcpcb * tcp_drop(struct tcpcb *, int);
extern struct tcpcb * tcp_close(struct tcpcb *);
extern void tcp_respond(struct tcpcb *, void *, struct tcphdr *, struct mbuf *, tcp_seq, tcp_seq, int);

/* defined functions */
void cc_conn_init(struct tcpcb *tp);
void cc_cong_signal(struct tcpcb *tp, struct tcphdr *th, uint32_t type);
void cc_ecnpkt_handler(struct tcpcb *tp, struct tcphdr *th, uint8_t iptos);
void tcp_handle_wakeup(struct tcpcb *tp, struct socket *so);
void tcp_dropwithreset(struct mbuf *, struct tcphdr *, struct tcpcb *, int, int);
void tcp_dooptions(struct tcpcb *, struct tcpopt *, u_char *, int, int);
int tcp_compute_pipe(struct tcpcb *);
uint32_t tcp_compute_initwnd(struct tcpcb *, uint32_t);
void tcp_mss(struct tcpcb *, int);
int tcp_mssopt(struct tcpcb *);

static void cc_ack_received(struct tcpcb *tp, struct tcphdr *th, uint16_t nsegs, uint16_t type);
static void cc_post_recovery(struct tcpcb *tp, struct tcphdr *th);
static void tcp_xmit_timer(struct tcpcb *, int);
static void tcp_prr_partialack(struct tcpcb *, struct tcphdr *);
static void tcp_newreno_partial_ack(struct tcpcb *, struct tcphdr *);

// for badport_bandlim rstreason, from netinet/icmp_var.h
#define BANDLIM_UNLIMITED -1
#define BANDLIM_RST_CLOSEDPORT 3 /* No connection, and no listeners */
#define BANDLIM_RST_OPENPORT 4   /* No connection, listener */

#define ticks               tcp_getticks(tp)
#define tcp_ts_getticks()   tcp_getticks(tp)


/*
 * CC wrapper hook functions
 */
void
cc_ack_received(struct tcpcb *tp, struct tcphdr *th, uint16_t nsegs,
    uint16_t type)
{
	tp->ccv->nsegs = nsegs;
	tp->ccv->bytes_this_ack = BYTES_THIS_ACK(tp, th);
	if ((!V_tcp_do_newcwv && (tp->snd_cwnd <= tp->snd_wnd)) ||
	    (V_tcp_do_newcwv && (tp->snd_cwnd <= tp->snd_wnd) &&
	     (tp->snd_cwnd < (tcp_compute_pipe(tp) * 2))))
		tp->ccv->flags |= CCF_CWND_LIMITED;
	else
		tp->ccv->flags &= ~CCF_CWND_LIMITED;

	if (type == CC_ACK) {
		if (tp->snd_cwnd > tp->snd_ssthresh) {
			tp->t_bytes_acked += tp->ccv->bytes_this_ack;
			if (tp->t_bytes_acked >= tp->snd_cwnd) {
				tp->t_bytes_acked -= tp->snd_cwnd;
				tp->ccv->flags |= CCF_ABC_SENTAWND;
			}
		} else {
				tp->ccv->flags &= ~CCF_ABC_SENTAWND;
				tp->t_bytes_acked = 0;
		}
	}

	if (CC_ALGO(tp)->ack_received != NULL) {
		/* XXXLAS: Find a way to live without this */
		tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->ack_received(tp->ccv, type);
	}
}

void
cc_conn_init(struct tcpcb *tp)
{
	u_int maxseg;
	maxseg = tcp_maxseg(tp);

	/*
	 * Set the initial slow-start flight size.
	 *
	 * If a SYN or SYN/ACK was lost and retransmitted, we have to
	 * reduce the initial CWND to one segment as congestion is likely
	 * requiring us to be cautious.
	 */
	if (tp->snd_cwnd == 1)
		tp->snd_cwnd = maxseg;		/* SYN(-ACK) lost */
	else
		tp->snd_cwnd = tcp_compute_initwnd(tp, maxseg);

	if (CC_ALGO(tp)->conn_init != NULL)
		CC_ALGO(tp)->conn_init(tp->ccv);
}

void inline
cc_cong_signal(struct tcpcb *tp, struct tcphdr *th, uint32_t type)
{
	switch(type) {
	case CC_NDUPACK:
		if (!IN_FASTRECOVERY(tp->t_flags)) {
			tp->snd_recover = tp->snd_max;
			if (tp->t_flags2 & TF2_ECN_PERMIT)
				tp->t_flags2 |= TF2_ECN_SND_CWR;
		}
		break;
	case CC_ECN:
		if (!IN_CONGRECOVERY(tp->t_flags) ||
		    /*
		     * Allow ECN reaction on ACK to CWR, if
		     * that data segment was also CE marked.
		     */
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_CONGRECOVERY(tp->t_flags);
			TCPSTAT_INC(tcps_ecn_rcwnd);
			tp->snd_recover = tp->snd_max + 1;
			if (tp->t_flags2 & TF2_ECN_PERMIT)
				tp->t_flags2 |= TF2_ECN_SND_CWR;
		}
		break;
	case CC_RTO:
		tp->t_dupacks = 0;
		tp->t_bytes_acked = 0;
		EXIT_RECOVERY(tp->t_flags);
		if (tp->t_flags2 & TF2_ECN_PERMIT)
			tp->t_flags2 |= TF2_ECN_SND_CWR;
		break;
	case CC_RTO_ERR:
		TCPSTAT_INC(tcps_sndrexmitbad);
		/* RTO was unnecessary, so reset everything. */
		tp->snd_cwnd = tp->snd_cwnd_prev;
		tp->snd_ssthresh = tp->snd_ssthresh_prev;
		tp->snd_recover = tp->snd_recover_prev;
		if (tp->t_flags & TF_WASFRECOVERY)
			ENTER_FASTRECOVERY(tp->t_flags);
		if (tp->t_flags & TF_WASCRECOVERY)
			ENTER_CONGRECOVERY(tp->t_flags);
		tp->snd_nxt = tp->snd_max;
		tp->t_flags &= ~TF_PREVVALID;
		tp->t_badrxtwin = 0;
		break;
	}

	if (CC_ALGO(tp)->cong_signal != NULL) {
		if (th != NULL)
			tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->cong_signal(tp->ccv, type);
	}
}

void inline
cc_post_recovery(struct tcpcb *tp, struct tcphdr *th)
{
	/* XXXLAS: KASSERT that we're in recovery? */

	if (CC_ALGO(tp)->post_recovery != NULL) {
		tp->ccv->curack = th->th_ack;
		CC_ALGO(tp)->post_recovery(tp->ccv);
	}
	/* XXXLAS: EXIT_RECOVERY ? */
	tp->t_bytes_acked = 0;
	tp->sackhint.prr_out = 0;
}

/*
 * Indicate whether this ack should be delayed.  We can delay the ack if
 * following conditions are met:
 *	- There is no delayed ack timer in progress.
 *	- Our last ack wasn't a 0-sized window. We never want to delay
 *	  the ack that opens up a 0-sized window.
 *	- LRO wasn't used for this segment. We make sure by checking that the
 *	  segment size is not larger than the MSS.
 */
#ifndef TREX_FBSD
#define DELAY_ACK(tp, tlen)						\
	((!tcp_timer_active(tp, TT_DELACK) &&				\
	    (tp->t_flags & TF_RXWIN0SENT) == 0) &&			\
	    (tlen <= tp->t_maxseg) &&					\
	    (V_tcp_delack_enabled || (tp->t_flags & TF_NEEDSYN)))
#else /* TREX_FBSD */
#define DELAY_ACK(tp, tlen)						\
	(((tp->t_flags & TF_RXWIN0SENT) == 0) &&			\
	    !(thflags & TH_PUSH) &&					\
	    (tlen <= tp->t_maxseg) &&					\
	    (V_tcp_delack_enabled || (tp->t_flags & TF_NEEDSYN)) &&	\
            !tcp_check_no_delay(tp, tlen))
/* !tcp_timer_active(tp, TT_DELACK) forces ACK on every other packet */
/* !(thflags & TH_PUSH) for trex-core compatible */
#endif /* TREX_FBSD */

void inline
cc_ecnpkt_handler(struct tcpcb *tp, struct tcphdr *th, uint8_t iptos)
{
	if (CC_ALGO(tp)->ecnpkt_handler != NULL) {
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->ccv->flags |= CCF_IPHDR_CE;
			break;
		case IPTOS_ECN_ECT0:
			/* FALLTHROUGH */
		case IPTOS_ECN_ECT1:
			/* FALLTHROUGH */
		case IPTOS_ECN_NOTECT:
			tp->ccv->flags &= ~CCF_IPHDR_CE;
			break;
		}

		if (th->th_flags & TH_CWR)
			tp->ccv->flags |= CCF_TCPHDR_CWR;
		else
			tp->ccv->flags &= ~CCF_TCPHDR_CWR;

		CC_ALGO(tp)->ecnpkt_handler(tp->ccv);

		if (tp->ccv->flags & CCF_ACKNOW) {
			tcp_timer_activate(tp, TT_DELACK, V_tcp_delacktime);
			tp->t_flags |= TF_ACKNOW;
		}
	}
}

static inline void
tcp_fields_to_host(struct tcphdr *th)
{
	th->th_seq = ntohl(th->th_seq);
	th->th_ack = ntohl(th->th_ack);
	th->th_win = ntohs(th->th_win);
	th->th_urp = ntohs(th->th_urp);
}

void
tcp_input(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th, int toff, int tlen, uint8_t iptos)
{
	struct ip *ip = NULL;
	struct socket *so = NULL;
	u_char *optp = NULL;
	int optlen = 0;
	int off;
	int drop_hdrlen;
	int thflags;
	int rstreason = 0;	/* For badport_bandlim accounting purposes */
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
	int isipv6;
#else
	const void *ip6 = NULL;
#endif /* INET6 */
	struct tcpopt to;		/* options in this segment */
#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp;
	short ostate = 0;
#endif

#ifdef INET6
        isipv6 = tcp_isipv6(tp);
#endif

	to.to_flags = 0;
	TCPSTAT_INC(tcps_rcvtotal);

#ifdef INET6
	if (isipv6) {
		ip6 = (struct ip6_hdr *)(((void *)th) - sizeof(struct ip6_hdr));
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		ip = (struct ip *)(((void *)th) - sizeof(struct ip));
	}
#endif /* INET */

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = th->th_off << 2;
	if (off > sizeof (struct tcphdr)) {
		optlen = off - sizeof (struct tcphdr);
		optp = (u_char *)(th + 1);
	}
	thflags = th->th_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	tcp_fields_to_host(th);

	/*
	 * Delay dropping TCP, IP headers, IPv6 ext headers, and TCP options.
	 */
	drop_hdrlen = toff;

	/*
	 * The TCPCB may no longer exist if the connection is winding
	 * down or it is in the CLOSED state.  Either way we drop the
	 * segment and send an appropriate response.
	 */
	so = tcp_getsocket(tp);
	KASSERT(so != NULL, ("%s: so == NULL", __func__));
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG) {
		ostate = tp->t_state;
#ifdef INET6
		if (isipv6) {
			bcopy((char *)ip6, (char *)tcp_saveipgen, sizeof(*ip6));
		} else
#endif
			bcopy((char *)ip, (char *)tcp_saveipgen, sizeof(*ip));
		tcp_savetcp = *th;
	}
#else
        (void) ip;
        (void) ip6;
#endif /* TCPDEBUG */
	/*
	 * When the socket is accepting connections (the INPCB is in LISTEN
	 * state) we look into the SYN cache if this is a new connection
	 * attempt or the completion of a previous one.
	 */
	KASSERT(tp->t_state == TCPS_LISTEN || !(so->so_options & SO_ACCEPTCONN),
	    ("%s: so accepting but tp %p not listening", __func__, tp));
	if (tp->t_state == TCPS_LISTEN) {

		/*
		 * Check for an existing connection attempt in syncache if
		 * the flag is only ACK.  A successful lookup creates a new
		 * socket appended to the listen queue in SYN_RECEIVED state.
		 */
		if ((thflags & (TH_RST|TH_ACK|TH_SYN)) == TH_ACK) {
			/*
			 * Parse the TCP options here because
			 * syncookies need access to the reflected
			 * timestamp.
			 */
			//tcp_dooptions(tp, &to, optp, optlen, 0); //?? check TS ...

			tcp_timer_activate(tp, TT_REXMT, 0);
			tcp_state_change(tp, TCPS_SYN_RECEIVED);

                        tcp_rcvseqinit(tp);
                        tcp_sendseqinit(tp);

                        tp->snd_wl1 = tp->irs;
                        tp->snd_max = tp->iss + 1;
                        tp->snd_nxt = tp->iss + 1;
                        tp->rcv_wnd = imax(sbspace(&so->so_rcv), 0);
                        tp->rcv_adv += tp->rcv_wnd;
                        tp->last_ack_sent = tp->rcv_nxt;

			//tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
			//tp->t_flags |= TF_ACKNOW;
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));

			TCPSTAT_INC(tcps_accepts);
			KASSERT(tp->t_state == TCPS_SYN_RECEIVED,
			    ("%s: ", __func__));
			/*
			 * Process the segment and the data it
			 * contains.  tcp_do_segment() consumes
			 * the mbuf chain and unlocks the inpcb.
			 */
			tp->t_fb->tfb_tcp_do_segment(m, th, so, tp, drop_hdrlen, tlen,
			    iptos);
			return;
		}
		/*
		 * Segment flag validation for new connection attempts:
		 *
		 * Our (SYN|ACK) response was rejected.
		 * Check with syncache and remove entry to prevent
		 * retransmits.
		 *
		 * NB: syncache_chkrst does its own logging of failure
		 * causes.
		 */
		if (thflags & TH_RST) {
			tp = tcp_drop(tp, ECONNREFUSED);
			goto dropunlock;
		}
		/*
		 * We can't do anything without SYN.
		 */
		if ((thflags & TH_SYN) == 0) {
			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * (SYN|ACK) is bogus on a listen socket.
		 */
		if (thflags & TH_ACK) {
			TCPSTAT_INC(tcps_badsyn);
			rstreason = BANDLIM_RST_OPENPORT;
			goto dropwithreset;
		}
		/*
		 * If the drop_synfin option is enabled, drop all
		 * segments with both the SYN and FIN bits set.
		 * This prevents e.g. nmap from identifying the
		 * TCP/IP stack.
		 * XXX: Poor reasoning.  nmap has other methods
		 * and is constantly refining its stack detection
		 * strategies.
		 * XXX: This is a violation of the TCP specification
		 * and was used by RFC1644.
		 */
		if ((thflags & TH_FIN) && V_drop_synfin) {
			TCPSTAT_INC(tcps_badsyn);
			goto dropunlock;
		}
		/*
		 * Segment's flags are (SYN) or (SYN|FIN).
		 *
		 * TH_PUSH, TH_URG, TH_ECE, TH_CWR are ignored
		 * as they do not affect the state of the TCP FSM.
		 * The data pointed to by TH_URG and th_urp is ignored.
		 */
		KASSERT((thflags & (TH_RST|TH_ACK)) == 0,
		    ("%s: Listen socket: TH_RST or TH_ACK set", __func__));
		KASSERT(thflags & (TH_SYN),
		    ("%s: Listen socket: TH_SYN not set", __func__));
		/*
		 * SYN appears to be valid.  Create compressed TCP state
		 * for syncache.
		 */
#ifdef TCPDEBUG
		if (so->so_options & SO_DEBUG)
			tcp_trace(TA_INPUT, ostate, tp,
			    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif
		tcp_dooptions(tp, &to, optp, optlen, TO_SYN);

		/* TREX_FBSD: update tcp options immediately due to syncache removed */
#define tcp_new_ts_offset(x)    0
                if (V_tcp_do_rfc1323) {
                        if (to.to_flags & TOF_TS) {
                                tp->t_flags |= TF_REQ_TSTMP|TF_RCVD_TSTMP;
                                tp->ts_recent = to.to_tsval;
                                tp->ts_recent_age = tcp_ts_getticks();
                                tp->ts_offset = tcp_new_ts_offset(inc);
                        }
                        if (to.to_flags & TOF_SCALE) {
                                int wscale = 0;
                                while (wscale < TCP_MAX_WINSHIFT &&
                                    (TCP_MAXWIN << wscale) < so->so_rcv.sb_hiwat)
                                        wscale++;
                                tp->t_flags |= TF_REQ_SCALE|TF_RCVD_SCALE;
                                tp->snd_scale = to.to_wscale;
                                tp->request_r_scale = wscale;
                        }
                }
                if (to.to_flags & TOF_SACKPERM)
                        tp->t_flags |= TF_SACK_PERMIT;
                if (((th->th_flags & (TH_ECE|TH_CWR)) == (TH_ECE|TH_CWR)) && V_tcp_do_ecn) {
                        tp->t_flags2 |= TF2_ECN_PERMIT;
                        //tp->t_flags2 |= TF2_ECN_SND_ECE; // ??
                        TCPSTAT_INC(tcps_ecn_shs);
                }

		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);

                if (!tcp_timer_active(tp, TT_REXMT))
                        tp->iss = tcp_new_isn(tp);
		tp->irs = th->th_seq;

                /* send SYN+ACK, tp->iss should be intialized already */
		tcp_respond(tp, NULL, NULL, m, tp->irs + 1, tp->iss, TH_SYN|TH_ACK);
                tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);

		/*
		 * Entry added to syncache and mbuf consumed.
		 * Only the listen socket is unlocked by syncache_add().
		 */
		return;
	}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (tp->t_flags & TF_SIGNATURE) {
		tcp_dooptions(&to, optp, optlen, thflags);
		if ((to.to_flags & TOF_SIGNATURE) == 0) {
			TCPSTAT_INC(tcps_sig_err_nosigopt);
			goto dropunlock;
		}
		if (!TCPMD5_ENABLED() ||
		    TCPMD5_INPUT(m, th, to.to_signature) != 0)
			goto dropunlock;
	}
#endif

	/*
	 * Segment belongs to a connection in SYN_SENT, ESTABLISHED or later
	 * state.  tcp_do_segment() always consumes the mbuf chain, unlocks
	 * the inpcb, and unlocks pcbinfo.
	 */
	tp->t_fb->tfb_tcp_do_segment(m, th, so, tp, drop_hdrlen, tlen, iptos);
	return;

dropwithreset:
	tcp_dropwithreset(m, th, tp, tlen, rstreason);
	m = NULL;	/* mbuf chain got consumed. */

dropunlock:
	if (m != NULL)
		m_freem(m);
	return;
}


void
tcp_handle_wakeup(struct tcpcb *tp, struct socket *so)
{
	if (tp->t_flags & TF_WAKESOR) {
		tp->t_flags &= ~TF_WAKESOR;
		sorwakeup(so);
	}
	if (tp->t_flags & TF_WAKESOW) {
		tp->t_flags &= ~TF_WAKESOW;
		sowwakeup(so);
	}
}

void
tcp_do_segment(struct mbuf *m, struct tcphdr *th, struct socket *so,
    struct tcpcb *tp, int drop_hdrlen, int tlen, uint8_t iptos)
{
	int thflags, acked, ourfinisacked, needoutput = 0, sack_changed;
	int rstreason, todrop, win, incforsyn = 0;
	uint32_t tiwin;
	uint16_t nsegs;
	struct tcpopt to;
	int tfo_syn;

#ifdef TCPDEBUG
	/*
	 * The size of tcp_saveipgen must be the size of the max ip header,
	 * now IPv6.
	 */
	u_char tcp_saveipgen[IP6_HDR_LEN];
	struct tcphdr tcp_savetcp = *th;
	short ostate = tp->t_state;
#ifdef INET6
	if (tcp_isipv6(tp))
		bcopy(((char *)th)-sizeof(struct ip6_hdr), (char *)tcp_saveipgen, sizeof(struct ip6_hdr));
	else
#endif
		bcopy(((char *)th)-sizeof(struct ip), (char *)tcp_saveipgen, sizeof(struct ip));
#endif
	thflags = th->th_flags;
	tp->sackhint.last_sack_ack = 0;
	sack_changed = 0;
	nsegs = 1;      /* TREX_FBSD: LRO not supported */

	KASSERT(tp->t_state > TCPS_LISTEN, ("%s: TCPS_LISTEN",
	    __func__));
	KASSERT(tp->t_state != TCPS_TIME_WAIT, ("%s: TCPS_TIME_WAIT",
	    __func__));

	if ((thflags & TH_SYN) && (thflags & TH_FIN) && V_drop_synfin) {
		goto drop;
	}

	/*
	 * If a segment with the ACK-bit set arrives in the SYN-SENT state
	 * check SEQ.ACK first.
	 */
	if ((tp->t_state == TCPS_SYN_SENT) && (thflags & TH_ACK) &&
	    (SEQ_LEQ(th->th_ack, tp->iss) || SEQ_GT(th->th_ack, tp->snd_max))) {
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 * XXX: This should be done after segment
	 * validation to ignore broken/spoofed segs.
	 */
	tp->t_rcvtime = ticks;

	/*
	 * Scale up the window into a 32-bit value.
	 * For the SYN_SENT state the scale is zero.
	 */
	tiwin = th->th_win << tp->snd_scale;

	/*
	 * TCP ECN processing.
	 */
	if (tp->t_flags2 & TF2_ECN_PERMIT) {
		if (thflags & TH_CWR) {
			tp->t_flags2 &= ~TF2_ECN_SND_ECE;
			tp->t_flags |= TF_ACKNOW;
		}
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			tp->t_flags2 |= TF2_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}

		/* Process a packet differently from RFC3168. */
		cc_ecnpkt_handler(tp, th, iptos);

		/* Congestion experienced. */
		if (thflags & TH_ECE) {
			cc_cong_signal(tp, th, CC_ECN);
		}
	}

	/*
	 * Parse options on any incoming segment.
	 */
	tcp_dooptions(tp, &to, (u_char *)(th + 1),
	    (th->th_off << 2) - sizeof(struct tcphdr),
	    (thflags & TH_SYN) ? TO_SYN : 0);

#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if ((tp->t_flags & TF_SIGNATURE) != 0 &&
	    (to.to_flags & TOF_SIGNATURE) == 0) {
		TCPSTAT_INC(tcps_sig_err_sigopt);
		/* XXX: should drop? */
	}
#endif
	/*
	 * If echoed timestamp is later than the current time,
	 * fall back to non RFC1323 RTT calculation.  Normalize
	 * timestamp if syncookies were used when this connection
	 * was established.
	 */
	if ((to.to_flags & TOF_TS) && (to.to_tsecr != 0)) {
		to.to_tsecr -= tp->ts_offset;
		if (TSTMP_GT(to.to_tsecr, tcp_ts_getticks()))
			to.to_tsecr = 0;
		else if (tp->t_flags & TF_PREVVALID &&
			 tp->t_badrxtwin != 0 && SEQ_LT(to.to_tsecr, tp->t_badrxtwin))
			cc_cong_signal(tp, th, CC_RTO_ERR);
	}
	/*
	 * Process options only when we get SYN/ACK back. The SYN case
	 * for incoming connections is handled in tcp_syncache.
	 * According to RFC1323 the window field in a SYN (i.e., a <SYN>
	 * or <SYN,ACK>) segment itself is never scaled.
	 * XXX this is traditional behavior, may need to be cleaned up.
	 */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
		/* Handle parallel SYN for ECN */
		if (!(thflags & TH_ACK) &&
		    ((thflags & (TH_CWR | TH_ECE)) == (TH_CWR | TH_ECE)) &&
		    ((V_tcp_do_ecn == 1) || (V_tcp_do_ecn == 2))) {
			tp->t_flags2 |= TF2_ECN_PERMIT;
			tp->t_flags2 |= TF2_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_shs);
		}
		if ((to.to_flags & TOF_SCALE) &&
		    (tp->t_flags & TF_REQ_SCALE)) {
			tp->t_flags |= TF_RCVD_SCALE;
			tp->snd_scale = to.to_wscale;
		} else
			tp->t_flags &= ~TF_REQ_SCALE;
		/*
		 * Initial send window.  It will be updated with
		 * the next incoming segment to the scaled value.
		 */
		tp->snd_wnd = th->th_win;
		if ((to.to_flags & TOF_TS) &&
		    (tp->t_flags & TF_REQ_TSTMP)) {
			tp->t_flags |= TF_RCVD_TSTMP;
			tp->ts_recent = to.to_tsval;
			tp->ts_recent_age = tcp_ts_getticks();
		} else
			tp->t_flags &= ~TF_REQ_TSTMP;
		if (to.to_flags & TOF_MSS)
			tcp_mss(tp, to.to_mss);
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    (to.to_flags & TOF_SACKPERM) == 0)
			tp->t_flags &= ~TF_SACK_PERMIT;
	}

	/*
	 * If timestamps were negotiated during SYN/ACK and a
	 * segment without a timestamp is received, silently drop
	 * the segment, unless it is a RST segment or missing timestamps are
	 * tolerated.
	 * See section 3.2 of RFC 7323.
	 */
	if ((tp->t_flags & TF_RCVD_TSTMP) && !(to.to_flags & TOF_TS)) {
		if (((thflags & TH_RST) != 0) || V_tcp_tolerate_missing_ts) {
		} else {
			goto drop;
		}
	}
	/*
	 * If timestamps were not negotiated during SYN/ACK and a
	 * segment with a timestamp is received, ignore the
	 * timestamp and process the packet normally.
	 * See section 3.2 of RFC 7323.
	 */
	if (!(tp->t_flags & TF_RCVD_TSTMP) && (to.to_flags & TOF_TS)) {
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
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED first, it can only
	 * be TH_NEEDSYN.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    th->th_seq == tp->rcv_nxt &&
	    (thflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    tp->snd_nxt == tp->snd_max &&
	    tiwin && tiwin == tp->snd_wnd &&
	    ((tp->t_flags & (TF_NEEDSYN|TF_NEEDFIN)) == 0) &&
	    SEGQ_EMPTY(tp) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	     TSTMP_GEQ(to.to_tsval, tp->ts_recent)) ) {
		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		if ((to.to_flags & TOF_TS) != 0 &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_ts_getticks();
			tp->ts_recent = to.to_tsval;
		}

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    !IN_RECOVERY(tp->t_flags) &&
			    (to.to_flags & TOF_SACK) == 0 &&
			    TAILQ_EMPTY(&tp->snd_holes)) {
				/*
				 * This is a pure ack for outstanding data.
				 */
				TCPSTAT_INC(tcps_predack);

				/*
				 * "bad retransmit" recovery without timestamps.
				 */
				if ((to.to_flags & TOF_TS) == 0 &&
				    tp->t_rxtshift == 1 &&
				    tp->t_flags & TF_PREVVALID &&
				    (int)(ticks - tp->t_badrxtwin) < 0) {
					cc_cong_signal(tp, th, CC_RTO_ERR);
				}

				/*
				 * Recalculate the transmit timer / rtt.
				 *
				 * Some boxes send broken timestamp replies
				 * during the SYN+ACK phase, ignore
				 * timestamps of 0 or we could calculate a
				 * huge RTT and blow up the retransmit timer.
				 */
				if ((to.to_flags & TOF_TS) != 0 &&
				    to.to_tsecr) {
					uint32_t t;

					t = tcp_ts_getticks() - to.to_tsecr;
					if (!tp->t_rttlow || tp->t_rttlow > t)
						tp->t_rttlow = t;
					tcp_xmit_timer(tp,
					    TCP_TS_TO_TICKS(t) + 1);
				} else if (tp->t_rtttime &&
				    SEQ_GT(th->th_ack, tp->t_rtseq)) {
					if (!tp->t_rttlow ||
					    tp->t_rttlow > ticks - tp->t_rtttime)
						tp->t_rttlow = ticks - tp->t_rtttime;
					tcp_xmit_timer(tp,
							ticks - tp->t_rtttime);
				}
				acked = BYTES_THIS_ACK(tp, th);

				TCPSTAT_ADD(tcps_rcvackpack, nsegs);
				TCPSTAT_ADD(tcps_rcvackbyte, acked);
				sbdrop(&so->so_snd, acked, so);
				if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
				    SEQ_LEQ(th->th_ack, tp->snd_recover))
					tp->snd_recover = th->th_ack - 1;

				/*
				 * Let the congestion control algorithm update
				 * congestion control related information. This
				 * typically means increasing the congestion
				 * window.
				 */
				cc_ack_received(tp, th, nsegs, CC_ACK);

				tp->snd_una = th->th_ack;
				/*
				 * Pull snd_wl2 up to prevent seq wrap relative
				 * to th_ack.
				 */
				tp->snd_wl2 = th->th_ack;
				tp->t_dupacks = 0;
				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
#ifdef TCPDEBUG
				if (so->so_options & SO_DEBUG)
					tcp_trace(TA_INPUT, ostate, tp,
					    (void *)tcp_saveipgen,
					    &tcp_savetcp, 0);
#endif
				if (tp->snd_una == tp->snd_max)
					tcp_timer_activate(tp, TT_REXMT, 0);
				else if (!tcp_timer_active(tp, TT_PERSIST))
					tcp_timer_activate(tp, TT_REXMT,
						      tp->t_rxtcur);
				tp->t_flags |= TF_WAKESOW;
				if (sbavail(&so->so_snd))
					(void) tp->t_fb->tfb_tcp_output(tp);
				goto check_delack;
			}
		} else if (th->th_ack == tp->snd_una &&
		    tlen <= sbspace(&so->so_rcv)) {
			/*
			 * This is a pure, in-sequence data packet with
			 * nothing on the reassembly queue and we have enough
			 * buffer space to take it.
			 */
			/* Clean receiver SACK report if present */
			if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks)
				tcp_clean_sackreport(tp);
			TCPSTAT_INC(tcps_preddat);
			tp->rcv_nxt += tlen;
			if (tlen &&
			    ((tp->t_flags2 & TF2_FBYTES_COMPLETE) == 0) &&
			    (tp->t_fbyte_in == 0)) {
				tp->t_fbyte_in = ticks;
				if (tp->t_fbyte_in == 0)
					tp->t_fbyte_in = 1;
				if (tp->t_fbyte_out && tp->t_fbyte_in)
					tp->t_flags2 |= TF2_FBYTES_COMPLETE;
			}
			/*
			 * Pull snd_wl1 up to prevent seq wrap relative to
			 * th_seq.
			 */
			tp->snd_wl1 = th->th_seq;
			/*
			 * Pull rcv_up up to prevent seq wrap relative to
			 * rcv_nxt.
			 */
			tp->rcv_up = tp->rcv_nxt;
			TCPSTAT_ADD(tcps_rcvpack, nsegs);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
#ifdef TCPDEBUG
			if (so->so_options & SO_DEBUG)
				tcp_trace(TA_INPUT, ostate, tp,
				    (void *)tcp_saveipgen, &tcp_savetcp, 0);
#endif

			/* Add data to socket buffer. */
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
				m_freem(m);
			} else {
				m_adj(m, drop_hdrlen);	/* delayed header drop */
				sbappendstream_locked(&so->so_rcv, m, 0, so);
			}
			tp->t_flags |= TF_WAKESOR;
			if (DELAY_ACK(tp, tlen)) {
				tp->t_flags |= TF_DELACK;
			} else {
				tp->t_flags |= TF_ACKNOW;
				tp->t_fb->tfb_tcp_output(tp);
			}
			goto check_delack;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));

	switch (tp->t_state) {
	/*
	 * If the state is SYN_RECEIVED:
	 *	if seg contains an ACK, but not for our SYN/ACK, send a RST.
	 */
	case TCPS_SYN_RECEIVED:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
		     SEQ_GT(th->th_ack, tp->snd_max))) {
				rstreason = BANDLIM_RST_OPENPORT;
				goto dropwithreset;
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains a RST with valid ACK (SEQ.ACK has already
	 *	    been verified), then drop the connection.
	 *	if seg contains a RST without an ACK, drop the seg.
	 *	if seg does not contain SYN, then drop the seg.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if seg contains an ECE and ECN support is enabled, the stream
	 *	    is ECN capable.
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((thflags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) {
			tp = tcp_drop(tp, ECONNREFUSED);
		}
		if (thflags & TH_RST)
			goto drop;
		if (!(thflags & TH_SYN))
			goto drop;

		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
			int tfo_partial_ack = 0;

			TCPSTAT_INC(tcps_connects);
			soisconnected(so);
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
			}
			tp->rcv_adv += min(tp->rcv_wnd,
			    TCP_MAXWIN << tp->rcv_scale);
			tp->snd_una++;		/* SYN is acked */
			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
			if (DELAY_ACK(tp, tlen) && tlen != 0 && !tfo_partial_ack)
				tcp_timer_activate(tp, TT_DELACK,
				    V_tcp_delacktime);
			else
				tp->t_flags |= TF_ACKNOW;

			if (((thflags & (TH_CWR | TH_ECE)) == TH_ECE) &&
			    (V_tcp_do_ecn == 1)) {
				tp->t_flags2 |= TF2_ECN_PERMIT;
				TCPSTAT_INC(tcps_ecn_shs);
			}

			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = ticks;
			if (tp->t_flags & TF_NEEDFIN) {
				tcp_state_change(tp, TCPS_FIN_WAIT_1);
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;
			} else {
				tcp_state_change(tp, TCPS_ESTABLISHED);
				cc_conn_init(tp);
				tcp_timer_activate(tp, TT_KEEP,
				    TP_KEEPIDLE(tp));
			}
		} else {
			/*
			 * Received initial SYN in SYN-SENT[*] state =>
			 * simultaneous open.
			 * If it succeeds, connection is * half-synchronized.
			 * Otherwise, do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 */
			tp->t_flags |= (TF_ACKNOW | TF_NEEDSYN);
			tcp_timer_activate(tp, TT_REXMT, 0);
			tcp_state_change(tp, TCPS_SYN_RECEIVED);
		}

		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			thflags &= ~TH_FIN;
			TCPSTAT_INC(tcps_rcvpackafterwin);
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		/*
		 * Client side of transaction: already sent SYN and data.
		 * If the remote host used T/TCP to validate the SYN,
		 * our data will be ACK'd; if so, enter normal data segment
		 * processing in the middle of step 5, ack processing.
		 * Otherwise, goto step 6.
		 */
		if (thflags & TH_ACK)
			goto process_ACK;

		goto step6;

	/*
	 * If the state is LAST_ACK or CLOSING or TIME_WAIT:
	 *      do normal processing.
	 *
	 * NB: Leftover from RFC1644 T/TCP.  Cases to be reused later.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
		break;  /* continue normal processing */
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check the RST flag and sequence number since reset segments
	 * are exempt from the timestamp and connection count tests.  This
	 * fixes a bug introduced by the Stevens, vol. 2, p. 960 bugfix
	 * below which allowed reset segments in half the sequence space
	 * to fall though and be processed (which gives forged reset
	 * segments with a random sequence number a 50 percent chance of
	 * killing a connection).
	 * Then check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 */
	if (thflags & TH_RST) {
		/*
		 * RFC5961 Section 3.2
		 *
		 * - RST drops connection only if SEG.SEQ == RCV.NXT.
		 * - If RST is in window, we send challenge ACK.
		 *
		 * Note: to take into account delayed ACKs, we should
		 *   test against last_ack_sent instead of rcv_nxt.
		 * Note 2: we handle special case of closed window, not
		 *   covered by the RFC.
		 */
		if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
		    (tp->rcv_wnd == 0 && tp->last_ack_sent == th->th_seq)) {
			KASSERT(tp->t_state != TCPS_SYN_SENT,
			    ("%s: TH_RST for TCPS_SYN_SENT th %p tp %p",
			    __func__, th, tp));

			if (V_tcp_insecure_rst ||
			    tp->last_ack_sent == th->th_seq) {
				TCPSTAT_INC(tcps_drops);
				/* Drop the connection. */
				switch (tp->t_state) {
				case TCPS_SYN_RECEIVED:
					so->so_error = ECONNREFUSED;
					goto close;
				case TCPS_ESTABLISHED:
				case TCPS_FIN_WAIT_1:
				case TCPS_FIN_WAIT_2:
				case TCPS_CLOSE_WAIT:
				case TCPS_CLOSING:
				case TCPS_LAST_ACK:
					so->so_error = ECONNRESET;
				close:
					/* FALLTHROUGH */
				default:
					tp = tcp_close(tp);
				}
			} else {
				TCPSTAT_INC(tcps_badrst);
				/* Send challenge ACK. */
				tcp_respond(tp, NULL, th, m,
				    tp->rcv_nxt, tp->snd_nxt, TH_ACK);
				tp->last_ack_sent = tp->rcv_nxt;
				m = NULL;
			}
		}
		goto drop;
	}

	/*
	 * RFC5961 Section 4.2
	 * Send challenge ACK for any SYN in synchronized state.
	 */
	if ((thflags & TH_SYN) && tp->t_state != TCPS_SYN_SENT &&
	    tp->t_state != TCPS_SYN_RECEIVED) {
		TCPSTAT_INC(tcps_badsyn);
		if (V_tcp_insecure_syn &&
		    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
			tp = tcp_drop(tp, ECONNRESET);
			rstreason = BANDLIM_UNLIMITED;
		} else {
			/* Send challenge ACK. */
			tcp_respond(tp, NULL, th, m, tp->rcv_nxt,
			    tp->snd_nxt, TH_ACK);
			tp->last_ack_sent = tp->rcv_nxt;
			m = NULL;
		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {
		/* Check to see if ts_recent is over 24 days old.  */
		if (tcp_ts_getticks() - tp->ts_recent_age > TCP_PAWS_IDLE) {
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
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, tlen);
			TCPSTAT_INC(tcps_pawsdrop);
			if (tlen)
				goto dropafterack;
			goto drop;
		}
	}

	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know
	 * the sequence numbers haven't wrapped.  This is a partial fix
	 * for the "LAND" DoS attack.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && SEQ_LT(th->th_seq, tp->irs)) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}

	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		if (thflags & TH_SYN) {
			thflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1)
				th->th_urp--;
			else
				thflags &= ~TH_URG;
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 */
		if (todrop > tlen
		    || (todrop == tlen && (thflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			thflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tlen;
			TCPSTAT_INC(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
			TCPSTAT_INC(tcps_rcvpartduppack);
			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		/*
		 * DSACK - add SACK block for dropped range
		 */
		if ((todrop > 0) && (tp->t_flags & TF_SACK_PERMIT)) {
			tcp_update_sack_list(tp, th->th_seq,
			    th->th_seq + todrop);
			/*
			 * ACK now, as the next in-sequence segment
			 * will clear the DSACK block again
			 */
			tp->t_flags |= TF_ACKNOW;
		}
		drop_hdrlen += todrop;	/* drop from the top afterwards */
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop)
			th->th_urp -= todrop;
		else {
			thflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tlen) {
		tp = tcp_close(tp);
		TCPSTAT_INC(tcps_rcvafterclose);
		rstreason = BANDLIM_UNLIMITED;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
	if (todrop > 0) {
		TCPSTAT_INC(tcps_rcvpackafterwin);
		if (todrop >= tlen) {
			TCPSTAT_ADD(tcps_rcvbyteafterwin, tlen);
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT_INC(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		} else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		tlen -= todrop;
		thflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE:
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to.to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
		((thflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_ts_getticks();
		tp->ts_recent = to.to_tsval;
	}

	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN
	 * flag is on (half-synchronized state), then queue data for
	 * later processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_state == TCPS_SYN_RECEIVED ||
		    (tp->t_flags & TF_NEEDSYN)) {
			goto step6;
		} else if (tp->t_flags & TF_ACKNOW)
			goto dropafterack;
		else
			goto drop;
	}

	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {
	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:

		TCPSTAT_INC(tcps_connects);
		soisconnected(so);
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->rcv_scale = tp->request_r_scale;
		}
		tp->snd_wnd = tiwin;
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = ticks;
		if (tp->t_flags & TF_NEEDFIN) {
			tcp_state_change(tp, TCPS_FIN_WAIT_1);
			tp->t_flags &= ~TF_NEEDFIN;
		} else {
			tcp_state_change(tp, TCPS_ESTABLISHED);
			/*
			 * TFO connections call cc_conn_init() during SYN
			 * processing.  Calling it again here for such
			 * connections is not harmless as it would undo the
			 * snd_cwnd reduction that occurs when a TFO SYN|ACK
			 * is retransmitted.
			 */
			cc_conn_init(tp);
			tcp_timer_activate(tp, TT_KEEP, TP_KEEPIDLE(tp));
		}
		/*
		 * Account for the ACK of our SYN prior to
		 * regular ACK processing below, except for
		 * simultaneous SYN, which is handled later.
		 */
		if (SEQ_GT(th->th_ack, tp->snd_una) && !(tp->t_flags & TF_NEEDSYN))
			incforsyn = 1;
		/*
		 * If segment contains data or ACK, will call tcp_reass()
		 * later; if not, do so now to pass queued data to user.
		 */
		if (tlen == 0 && (thflags & TH_FIN) == 0)
			(void) tcp_reass(tp, (struct tcphdr *)0, NULL, 0,
			    (struct mbuf *)0);
		tp->snd_wl1 = th->th_seq - 1;
		/* FALLTHROUGH */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			TCPSTAT_INC(tcps_rcvacktoomuch);
			goto dropafterack;
		}
		if ((tp->t_flags & TF_SACK_PERMIT) &&
		    ((to.to_flags & TOF_SACK) ||
		     !TAILQ_EMPTY(&tp->snd_holes)))
			sack_changed = tcp_sack_doack(tp, &to, th->th_ack);
		else
			/*
			 * Reset the value so that previous (valid) value
			 * from the last ack with SACK doesn't get used.
			 */
			tp->sackhint.sacked_bytes = 0;

		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			u_int maxseg;

			maxseg = tcp_maxseg(tp);
			if (tlen == 0 &&
			    (tiwin == tp->snd_wnd ||
			    (tp->t_flags & TF_SACK_PERMIT))) {
				/*
				 * If this is the first time we've seen a
				 * FIN from the remote, this is not a
				 * duplicate and it needs to be processed
				 * normally.  This happens during a
				 * simultaneous close.
				 */
				if ((thflags & TH_FIN) &&
				    (TCPS_HAVERCVDFIN(tp->t_state) == 0)) {
					tp->t_dupacks = 0;
					break;
				}
				/* TREX_FBSD: we can get ack on FIN-ACK and it should not considered dup */
				if (tp->t_state != TCPS_FIN_WAIT_2)
					TCPSTAT_INC(tcps_rcvdupack);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change and FIN isn't set),
				 * the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshold of them, assume a packet
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
				 *
				 * When using TCP ECN, notify the peer that
				 * we reduced the cwnd.
				 */
				/*
				 * Following 2 kinds of acks should not affect
				 * dupack counting:
				 * 1) Old acks
				 * 2) Acks with SACK but without any new SACK
				 * information in them. These could result from
				 * any anomaly in the network like a switch
				 * duplicating packets or a possible DoS attack.
				 */
				if (th->th_ack != tp->snd_una ||
				    ((tp->t_flags & TF_SACK_PERMIT) &&
				    !sack_changed))
					break;
				else if (!tcp_timer_active(tp, TT_REXMT))
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks > V_tcprexmtthresh ||
				     IN_FASTRECOVERY(tp->t_flags)) {
					cc_ack_received(tp, th, nsegs,
					    CC_DUPACK);
					if (V_tcp_do_prr &&
					    IN_FASTRECOVERY(tp->t_flags) &&
					    (tp->t_flags & TF_SACK_PERMIT)) {
						int snd_cnt = 0, limit = 0;
						int del_data = 0, pipe = 0;
						/*
						 * In a duplicate ACK del_data is only the
						 * diff_in_sack. If no SACK is used del_data
						 * will be 0. Pipe is the amount of data we
						 * estimate to be in the network.
						 */
						del_data = tp->sackhint.delivered_data;
						if (V_tcp_do_rfc6675_pipe)
							pipe = tcp_compute_pipe(tp);
						else
							pipe = (tp->snd_nxt - tp->snd_fack) +
								tp->sackhint.sack_bytes_rexmit;
						tp->sackhint.prr_delivered += del_data;
						if (pipe >= tp->snd_ssthresh) {
							if (tp->sackhint.recover_fs == 0)
								tp->sackhint.recover_fs =
								    imax(1, tp->snd_nxt - tp->snd_una);
							snd_cnt = howmany((long)tp->sackhint.prr_delivered *
							    tp->snd_ssthresh, tp->sackhint.recover_fs) -
							    tp->sackhint.prr_out;
						} else {
							if (V_tcp_do_prr_conservative)
								limit = tp->sackhint.prr_delivered -
									tp->sackhint.prr_out;
							else
								limit = imax(tp->sackhint.prr_delivered -
									    tp->sackhint.prr_out,
									    del_data) + maxseg;
							snd_cnt = imin(tp->snd_ssthresh - pipe, limit);
						}
						snd_cnt = imax(snd_cnt, 0) / maxseg;
						/*
						 * Send snd_cnt new data into the network in
						 * response to this ACK. If there is a going
						 * to be a SACK retransmission, adjust snd_cwnd
						 * accordingly.
						 */
						tp->snd_cwnd = imax(maxseg, tp->snd_nxt - tp->snd_recover +
						    tp->sackhint.sack_bytes_rexmit + (snd_cnt * maxseg));
					} else if ((tp->t_flags & TF_SACK_PERMIT) &&
					    IN_FASTRECOVERY(tp->t_flags)) {
						int awnd;

						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff
						 * we have less than 1/2 the original window's
						 * worth of data in flight.
						 */
						if (V_tcp_do_rfc6675_pipe)
							awnd = tcp_compute_pipe(tp);
						else
							awnd = (tp->snd_nxt - tp->snd_fack) +
								tp->sackhint.sack_bytes_rexmit;

						if (awnd < tp->snd_ssthresh) {
							tp->snd_cwnd += maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh)
								tp->snd_cwnd = tp->snd_ssthresh;
						}
					} else
						tp->snd_cwnd += maxseg;
					(void) tp->t_fb->tfb_tcp_output(tp);
					goto drop;
				} else if (tp->t_dupacks == V_tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;

					/*
					 * If we're doing sack, or prr, check
					 * to see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (V_tcp_do_prr ||
					    (tp->t_flags & TF_SACK_PERMIT)) {
						if (IN_FASTRECOVERY(tp->t_flags)) {
							tp->t_dupacks = 0;
							break;
						}
					} else {
						if (SEQ_LEQ(th->th_ack,
						    tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					/* Congestion signal before ack. */
					cc_cong_signal(tp, th, CC_NDUPACK);
					cc_ack_received(tp, th, nsegs,
					    CC_DUPACK);
					tcp_timer_activate(tp, TT_REXMT, 0);
					tp->t_rtttime = 0;
					if (V_tcp_do_prr) {
						/*
						 * snd_ssthresh is already updated by
						 * cc_cong_signal.
						 */
						tp->sackhint.prr_delivered =
						    tp->sackhint.sacked_bytes;
						tp->sackhint.recover_fs = max(1,
						    tp->snd_nxt - tp->snd_una);
					}
					if (tp->t_flags & TF_SACK_PERMIT) {
						TCPSTAT_INC(
						    tcps_sack_recovery_episode);
						tp->snd_recover = tp->snd_nxt;
						tp->snd_cwnd = maxseg;
						(void) tp->t_fb->tfb_tcp_output(tp);
						goto drop;
					}
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = maxseg;
					(void) tp->t_fb->tfb_tcp_output(tp);
					KASSERT(tp->snd_limited <= 2,
					    ("%s: tp->snd_limited too big",
					    __func__));
					tp->snd_cwnd = tp->snd_ssthresh +
					     maxseg *
					     (tp->t_dupacks - tp->snd_limited);
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (V_tcp_do_rfc3042) {
					/*
					 * Process first and second duplicate
					 * ACKs. Each indicates a segment
					 * leaving the network, creating room
					 * for more. Make sure we can send a
					 * packet on reception of each duplicate
					 * ACK by increasing snd_cwnd by one
					 * segment. Restore the original
					 * snd_cwnd after packet transmission.
					 */
					cc_ack_received(tp, th, nsegs,
					    CC_DUPACK);
					uint32_t oldcwnd = tp->snd_cwnd;
					tcp_seq oldsndmax = tp->snd_max;
					u_int sent;
					int avail;

					KASSERT(tp->t_dupacks == 1 ||
					    tp->t_dupacks == 2,
					    ("%s: dupacks not 1 or 2",
					    __func__));
					if (tp->t_dupacks == 1)
						tp->snd_limited = 0;
					tp->snd_cwnd =
					    (tp->snd_nxt - tp->snd_una) +
					    (tp->t_dupacks - tp->snd_limited) *
					    maxseg;
					/*
					 * Only call tcp_output when there
					 * is new data available to be sent.
					 * Otherwise we would send pure ACKs.
					 */
					avail = sbavail(&so->so_snd) -
					    (tp->snd_nxt - tp->snd_una);
					if (avail > 0)
						(void) tp->t_fb->tfb_tcp_output(tp);
					sent = tp->snd_max - oldsndmax;
					if (sent > maxseg) {
						KASSERT((tp->t_dupacks == 2 &&
						    tp->snd_limited == 0) ||
						   (sent == maxseg + 1 &&
						    tp->t_flags & TF_SENTFIN),
						    ("%s: sent too much",
						    __func__));
						tp->snd_limited = 2;
					} else if (sent > 0)
						++tp->snd_limited;
					tp->snd_cwnd = oldcwnd;
					goto drop;
				}
			}
			break;
		} else {
			/*
			 * This ack is advancing the left edge, reset the
			 * counter.
			 */
			tp->t_dupacks = 0;
			/*
			 * If this ack also has new SACK info, increment the
			 * counter as per rfc6675. The variable
			 * sack_changed tracks all changes to the SACK
			 * scoreboard, including when partial ACKs without
			 * SACK options are received, and clear the scoreboard
			 * from the left side. Such partial ACKs should not be
			 * counted as dupacks here.
			 */
			if ((tp->t_flags & TF_SACK_PERMIT) &&
			    (to.to_flags & TOF_SACK) &&
			    sack_changed)
				tp->t_dupacks++;
		}

		KASSERT(SEQ_GT(th->th_ack, tp->snd_una),
		    ("%s: th_ack <= snd_una", __func__));

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (IN_FASTRECOVERY(tp->t_flags)) {
			if (SEQ_LT(th->th_ack, tp->snd_recover)) {
				if (tp->t_flags & TF_SACK_PERMIT)
					if (V_tcp_do_prr)
						tcp_prr_partialack(tp, th);
					else
						tcp_sack_partialack(tp, th);
				else
					tcp_newreno_partial_ack(tp, th);
			} else
				cc_post_recovery(tp, th);
		}
		/*
		 * If we reach this point, ACK is not a duplicate,
		 *     i.e., it ACKs something we sent.
		 */
		if (tp->t_flags & TF_NEEDSYN) {
			/*
			 * T/TCP: Connection was half-synchronized, and our
			 * SYN has been ACK'd (so connection is now fully
			 * synchronized).  Go to non-starred state,
			 * increment snd_una for ACK of SYN, and check if
			 * we can do window scaling.
			 */
			tp->t_flags &= ~TF_NEEDSYN;
			tp->snd_una++;
			/* Do window scaling? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->rcv_scale = tp->request_r_scale;
				/* Send window already scaled. */
			}
		}

process_ACK:
		/*
		 * Adjust for the SYN bit in sequence space,
		 * but don't account for it in cwnd calculations.
		 * This is for the SYN_RECEIVED, non-simultaneous
		 * SYN case. SYN_SENT and simultaneous SYN are
		 * treated elsewhere.
		 */
		if (incforsyn)
			tp->snd_una++;
		acked = BYTES_THIS_ACK(tp, th);
		KASSERT(acked >= 0, ("%s: acked unexepectedly negative "
		    "(tp->snd_una=%u, th->th_ack=%u, tp=%p, m=%p)", __func__,
		    tp->snd_una, th->th_ack, tp, m));
		TCPSTAT_ADD(tcps_rcvackpack, nsegs);

		/*
		 * If we just performed our first retransmit, and the ACK
		 * arrives within our recovery window, then it was a mistake
		 * to do the retransmit in the first place.  Recover our
		 * original cwnd and ssthresh, and proceed to transmit where
		 * we left off.
		 */
		if (tp->t_rxtshift == 1 &&
		    tp->t_flags & TF_PREVVALID &&
		    tp->t_badrxtwin &&
		    SEQ_LT(to.to_tsecr, tp->t_badrxtwin))
			cc_cong_signal(tp, th, CC_RTO_ERR);

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 *
		 * Some boxes send broken timestamp replies
		 * during the SYN+ACK phase, ignore
		 * timestamps of 0 or we could calculate a
		 * huge RTT and blow up the retransmit timer.
		 */
		if ((to.to_flags & TOF_TS) != 0 && to.to_tsecr) {
			uint32_t t;

			t = tcp_ts_getticks() - to.to_tsecr;
			if (!tp->t_rttlow || tp->t_rttlow > t)
				tp->t_rttlow = t;
			tcp_xmit_timer(tp, TCP_TS_TO_TICKS(t) + 1);
		} else if (tp->t_rtttime && SEQ_GT(th->th_ack, tp->t_rtseq)) {
			if (!tp->t_rttlow || tp->t_rttlow > ticks - tp->t_rtttime)
				tp->t_rttlow = ticks - tp->t_rtttime;
			tcp_xmit_timer(tp, ticks - tp->t_rtttime);
		}

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (th->th_ack == tp->snd_max) {
			tcp_timer_activate(tp, TT_REXMT, 0);
			needoutput = 1;
		} else if (!tcp_timer_active(tp, TT_PERSIST))
			tcp_timer_activate(tp, TT_REXMT, tp->t_rxtcur);

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * Let the congestion control algorithm update congestion
		 * control related information. This typically means increasing
		 * the congestion window.
		 */
		cc_ack_received(tp, th, nsegs, CC_ACK);

		if (acked > sbavail(&so->so_snd)) {
			TCPSTAT_ADD(tcps_rcvackbyte, sbavail(&so->so_snd));
			TCPSTAT_ADD(tcps_rcvackbyte_of, acked-sbavail(&so->so_snd));
			if (tp->snd_wnd >= sbavail(&so->so_snd))
				tp->snd_wnd -= sbavail(&so->so_snd);
			else
				tp->snd_wnd = 0;
			sbdrop(&so->so_snd, (int)sbavail(&so->so_snd), so);
			ourfinisacked = 1;
		} else {
			TCPSTAT_ADD(tcps_rcvackbyte, acked);
			sbdrop(&so->so_snd, acked, so);
			if (tp->snd_wnd >= (uint32_t) acked)
				tp->snd_wnd -= acked;
			else
				tp->snd_wnd = 0;
			ourfinisacked = 0;
		}
		tp->t_flags |= TF_WAKESOW;
		/* Detect una wraparound. */
		if (!IN_RECOVERY(tp->t_flags) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover))
			tp->snd_recover = th->th_ack - 1;
		/* XXXLAS: Can this be moved up into cc_post_recovery? */
		if (IN_RECOVERY(tp->t_flags) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_RECOVERY(tp->t_flags);
		}
		tp->snd_una = th->th_ack;
		if (tp->t_flags & TF_SACK_PERMIT) {
			if (SEQ_GT(tp->snd_una, tp->snd_recover))
				tp->snd_recover = tp->snd_una;
		}
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
				 *
				 * XXXjl:
				 * we should release the tp also, and use a
				 * compressed state.
				 */
				if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
					soisdisconnected(so);
					tcp_timer_activate(tp, TT_2MSL, TCPTV_2MSL);
				}
				tcp_state_change(tp, TCPS_FIN_WAIT_2);
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
				tcp_state_change(tp, TCPS_TIME_WAIT);
				soisdisconnected(so);
				tcp_cancel_timers(tp);
				tcp_timer_activate(tp, TT_2MSL, TCPTV_2MSL);
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
				tp = tcp_close(tp);
				goto drop;
			}
			break;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	     (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
			TCPSTAT_INC(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * If no out of band data is expected,
	 * pull receive urgent pointer along
	 * with the receive window.
	 */
	if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
		tp->rcv_up = tp->rcv_nxt;

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	tfo_syn = false;
	if ((tlen || (thflags & TH_FIN) || (tfo_syn && tlen > 0)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		tcp_seq save_start = th->th_seq;
		tcp_seq save_rnxt  = tp->rcv_nxt;
		int     save_tlen  = tlen;
		m_adj(m, drop_hdrlen);	/* delayed header drop */
		/*
		 * Insert segment which includes th into TCP reassembly queue
		 * with control block tp.  Set thflags to whether reassembly now
		 * includes a segment with FIN.  This handles the common case
		 * inline (segment is the next to be received on an established
		 * connection, and the queue is empty), avoiding linkage into
		 * and removal from the queue and repetition of various
		 * conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order (so
		 * fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt &&
		    SEGQ_EMPTY(tp) &&
		    (TCPS_HAVEESTABLISHED(tp->t_state) ||
		     tfo_syn)) {
			if (DELAY_ACK(tp, tlen) || tfo_syn)
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt += tlen;
			if (tlen &&
			    ((tp->t_flags2 & TF2_FBYTES_COMPLETE) == 0) &&
			    (tp->t_fbyte_in == 0)) {
				tp->t_fbyte_in = ticks;
				if (tp->t_fbyte_in == 0)
					tp->t_fbyte_in = 1;
				if (tp->t_fbyte_out && tp->t_fbyte_in)
					tp->t_flags2 |= TF2_FBYTES_COMPLETE;
			}
			thflags = th->th_flags & TH_FIN;
			TCPSTAT_INC(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tlen);
			if (so->so_rcv.sb_state & SBS_CANTRCVMORE)
				m_freem(m);
			else
				sbappendstream_locked(&so->so_rcv, m, 0, so);
			tp->t_flags |= TF_WAKESOR;
		} else {
			/*
			 * XXX: Due to the header drop above "th" is
			 * theoretically invalid by now.  Fortunately
			 * m_adj() doesn't actually frees any mbufs
			 * when trimming from the head.
			 */
			tcp_seq temp = save_start;
			thflags = tcp_reass(tp, th, &temp, &tlen, m);
			tp->t_flags |= TF_ACKNOW;
		}
		if ((tp->t_flags & TF_SACK_PERMIT) && (save_tlen > 0)) {
			if ((tlen == 0) && (SEQ_LT(save_start, save_rnxt))) {
				/*
				 * DSACK actually handled in the fastpath
				 * above.
				 */
				tcp_update_sack_list(tp, save_start,
				    save_start + save_tlen);
			} else if ((tlen > 0) && SEQ_GT(tp->rcv_nxt, save_rnxt)) {
				if ((tp->rcv_numsacks >= 1) &&
				    (tp->sackblks[0].end == save_start)) {
					/*
					 * Partial overlap, recorded at todrop
					 * above.
					 */
					tcp_update_sack_list(tp,
					    tp->sackblks[0].start,
					    tp->sackblks[0].end);
				} else {
					tcp_update_dsack_list(tp, save_start,
					    save_start + save_tlen);
				}
			} else if (tlen >= save_tlen) {
				/* Update of sackblks. */
				tcp_update_dsack_list(tp, save_start,
				    save_start + save_tlen);
			} else if (tlen > 0) {
				tcp_update_dsack_list(tp, save_start,
				    save_start + tlen);
			}
		}
#if 0
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 * XXX: Unused.
		 */
		if (SEQ_GT(tp->rcv_adv, tp->rcv_nxt))
			len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
		else
			len = so->so_rcv.sb_hiwat;
#endif
	} else {
		m_freem(m);
		thflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (thflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			/* The socket upcall is handled by socantrcvmore. */
			tp->t_flags &= ~TF_WAKESOR;
			/*
			 * If connection is half-synchronized
			 * (ie NEEDSYN flag on) then delay ACK,
			 * so it may be piggybacked when SYN is sent.
			 * Otherwise, since we received a FIN then no
			 * more input can be expected, send ACK now.
			 */
			if (tp->t_flags & TF_NEEDSYN)
				tp->t_flags |= TF_DELACK;
			else
				tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {
		/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = ticks;
			/* FALLTHROUGH */
		case TCPS_ESTABLISHED:
			tcp_state_change(tp, TCPS_CLOSE_WAIT);
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tcp_state_change(tp, TCPS_CLOSING);
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			tcp_state_change(tp, TCPS_TIME_WAIT);
			tcp_cancel_timers(tp);
			tcp_timer_activate(tp, TT_2MSL, TCPTV_2MSL);
			soisdisconnected(so);
			break;
		}
	}
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tp->t_fb->tfb_tcp_output(tp);

check_delack:
	if (tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tcp_timer_activate(tp, TT_DELACK, V_tcp_delacktime);
	}
	tcp_handle_wakeup(tp, so);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all
	 * paths to this code happen after packets containing
	 * RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the
	 * segment we received passes the SYN-RECEIVED ACK test.
	 * If it fails send a RST.  This breaks the loop in the
	 * "LAND" DoS attack, and also prevents an ACK storm
	 * between two listening ports that have been sent forged
	 * SYN segments, each with the source address of the other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	     SEQ_GT(th->th_ack, tp->snd_max)) ) {
		rstreason = BANDLIM_RST_OPENPORT;
		goto dropwithreset;
	}
#ifdef TCPDEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	tp->t_flags |= TF_ACKNOW;
	(void) tp->t_fb->tfb_tcp_output(tp);
	tcp_handle_wakeup(tp, so);
	m_freem(m);
	return;

dropwithreset:
	if (tp != NULL) {
		tcp_dropwithreset(m, th, tp, tlen, rstreason);
		tcp_handle_wakeup(tp, so);
	} else
		tcp_dropwithreset(m, th, NULL, tlen, rstreason);
	return;

drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef TCPDEBUG
	if (tp == NULL || (tcp_getsocket(tp)->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, (void *)tcp_saveipgen,
			  &tcp_savetcp, 0);
#endif
	if (tp != NULL) {
		tcp_handle_wakeup(tp, so);
	}
	m_freem(m);
}

/*
 * Issue RST and make ACK acceptable to originator of segment.
 * The mbuf must still include the original packet header.
 * tp may be NULL.
 */
void
tcp_dropwithreset(struct mbuf *m, struct tcphdr *th, struct tcpcb *tp,
    int tlen, int rstreason)
{
	if (tp != NULL) {
	}

	/* Don't bother if destination was broadcast/multicast. */
	if (th->th_flags & TH_RST)
		goto drop;

	/* tcp_respond consumes the mbuf chain. */
	if (th->th_flags & TH_ACK) {
		tcp_respond(tp, NULL, th, m, (tcp_seq)0,
		    th->th_ack, TH_RST);
	} else {
		if (th->th_flags & TH_SYN)
			tlen++;
		if (th->th_flags & TH_FIN)
			tlen++;
		tcp_respond(tp, NULL, th, m, th->th_seq+tlen,
		    (tcp_seq)0, TH_RST|TH_ACK);
	}
	return;
drop:
	m_freem(m);
}

/*
 * Parse TCP options and place in tcpopt.
 */
void
tcp_dooptions(struct tcpcb *tp, struct tcpopt *to, u_char *cp, int cnt, int flags)
{
	int opt, optlen;

	to->to_flags = 0;
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(flags & TO_SYN))
				continue;
			to->to_flags |= TOF_MSS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_mss, sizeof(to->to_mss));
			to->to_mss = ntohs(to->to_mss);
			break;
		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(flags & TO_SYN))
				continue;
			to->to_flags |= TOF_SCALE;
			to->to_wscale = min(cp[2], TCP_MAX_WINSHIFT);
			break;
		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			to->to_flags |= TOF_TS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_tsval, sizeof(to->to_tsval));
			to->to_tsval = ntohl(to->to_tsval);
			bcopy((char *)cp + 6,
			    (char *)&to->to_tsecr, sizeof(to->to_tsecr));
			to->to_tsecr = ntohl(to->to_tsecr);
			break;
		case TCPOPT_SIGNATURE:
			/*
			 * In order to reply to a host which has set the
			 * TCP_SIGNATURE option in its initial SYN, we have
			 * to record the fact that the option was observed
			 * here for the syncache code to perform the correct
			 * response.
			 */
			if (optlen != TCPOLEN_SIGNATURE)
				continue;
			to->to_flags |= TOF_SIGNATURE;
			to->to_signature = cp + 2;
			break;
		case TCPOPT_SACK_PERMITTED:
			if (optlen != TCPOLEN_SACK_PERMITTED)
				continue;
			if (!(flags & TO_SYN))
				continue;
			if (!V_tcp_do_sack)
				continue;
			to->to_flags |= TOF_SACKPERM;
			break;
		case TCPOPT_SACK:
			if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
				continue;
			if (flags & TO_SYN)
				continue;
			to->to_flags |= TOF_SACK;
			to->to_nsacks = (optlen - 2) / TCPOLEN_SACK;
			to->to_sacks = cp + 2;
			TCPSTAT_INC(tcps_sack_rcv_blocks);
			break;
		default:
			continue;
		}
	}
}


/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void
tcp_xmit_timer(struct tcpcb *tp, int rtt)
{
	int delta;

	TCPSTAT_INC(tcps_rttupdated);
	tp->t_rttupdated++;
	if ((tp->t_srtt != 0) && (tp->t_rxtshift <= TCP_RTT_INVALIDATE)) {
		/*
		 * srtt is stored as fixed point with 5 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = ((rtt - 1) << TCP_DELTA_SHIFT)
			- (tp->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT));

		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;

		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 4 bits after the
		 * binary point (scaled by 16).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= tp->t_rttvar >> (TCP_RTTVAR_SHIFT - TCP_DELTA_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
		if (tp->t_rttbest > tp->t_srtt + tp->t_rttvar)
		    tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
		tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
	}
	tp->t_rtttime = 0;
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
		      max(tp->t_rttmin, rtt + 2), TCPTV_REXMTMAX);

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}


void
tcp_mss(struct tcpcb *tp, int offer)
{
#if 0 /* TREX_FBSD: do nothing for old stack compatibility */
	int mss;
	uint32_t bufsize;
	struct socket *so;

	mss = tp->t_maxseg;

	/*
	 * If there's a pipesize, change the socket buffer to that size,
	 * don't change if sb_hiwat is different than default (then it
	 * has been changed on purpose with setsockopt).
	 * Make the socket buffers an integral number of mss units;
	 * if the mss is larger than the socket buffer, decrease the mss.
	 */
	so = tcp_getsocket(tp);
	bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss)
		mss = bufsize;
	else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > so->so_snd.sb_hiwat)
			(void)sbreserve_locked(&so->so_snd, bufsize, so, NULL);
	}
	/*
	 * Sanity check: make sure that maxseg will be large
	 * enough to allow some data on segments even if the
	 * all the option space is used (40bytes).  Otherwise
	 * funny things may happen in tcp_output.
	 *
	 * XXXGL: shouldn't we reserve space for IP/IPv6 options?
	 */
	tp->t_maxseg = max(mss, 64);

	bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = roundup(bufsize, mss);
		if (bufsize > so->so_rcv.sb_hiwat)
			(void)sbreserve_locked(&so->so_rcv, bufsize, so, NULL);
	}
#endif
}

/*
 * Determine the MSS option to send on an outgoing SYN.
 */
int
tcp_mssopt(struct tcpcb *tp)
{
	int mss = 0;
	mss = V_tcp_mssdflt;

	return (mss);
}

void
tcp_prr_partialack(struct tcpcb *tp, struct tcphdr *th)
{
	int snd_cnt = 0, limit = 0, del_data = 0, pipe = 0;
	int maxseg = tcp_maxseg(tp);

	tcp_timer_activate(tp, TT_REXMT, 0);
	tp->t_rtttime = 0;
	/*
	 * Compute the amount of data that this ACK is indicating
	 * (del_data) and an estimate of how many bytes are in the
	 * network.
	 */
	del_data = tp->sackhint.delivered_data;
	if (V_tcp_do_rfc6675_pipe)
		pipe = tcp_compute_pipe(tp);
	else
		pipe = (tp->snd_nxt - tp->snd_fack) + tp->sackhint.sack_bytes_rexmit;
	tp->sackhint.prr_delivered += del_data;
	/*
	 * Proportional Rate Reduction
	 */
	if (pipe >= tp->snd_ssthresh) {
		if (tp->sackhint.recover_fs == 0)
			tp->sackhint.recover_fs =
			    imax(1, tp->snd_nxt - tp->snd_una);
		snd_cnt = howmany((long)tp->sackhint.prr_delivered *
			    tp->snd_ssthresh, tp->sackhint.recover_fs) -
			    tp->sackhint.prr_out;
	} else {
		if (V_tcp_do_prr_conservative)
			limit = tp->sackhint.prr_delivered -
			    tp->sackhint.prr_out;
		else
			limit = imax(tp->sackhint.prr_delivered -
				    tp->sackhint.prr_out, del_data) +
				    maxseg;
		snd_cnt = imin((tp->snd_ssthresh - pipe), limit);
	}
	snd_cnt = imax(snd_cnt, 0) / maxseg;
	/*
	 * Send snd_cnt new data into the network in response to this ack.
	 * If there is going to be a SACK retransmission, adjust snd_cwnd
	 * accordingly.
	 */
	tp->snd_cwnd = imax(maxseg, tp->snd_nxt - tp->snd_recover +
		tp->sackhint.sack_bytes_rexmit + (snd_cnt * maxseg));
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
}

/*
 * On a partial ack arrives, force the retransmission of the
 * next unacknowledged segment.  Do not clear tp->t_dupacks.
 * By setting snd_nxt to ti_ack, this forces retransmission timer to
 * be started again.
 */
void
tcp_newreno_partial_ack(struct tcpcb *tp, struct tcphdr *th)
{
	tcp_seq onxt = tp->snd_nxt;
	uint32_t ocwnd = tp->snd_cwnd;
	u_int maxseg = tcp_maxseg(tp);

	tcp_timer_activate(tp, TT_REXMT, 0);
	tp->t_rtttime = 0;
	tp->snd_nxt = th->th_ack;
	/*
	 * Set snd_cwnd to one segment beyond acknowledged offset.
	 * (tp->snd_una has not yet been updated when this function is called.)
	 */
	tp->snd_cwnd = maxseg + BYTES_THIS_ACK(tp, th);
	tp->t_flags |= TF_ACKNOW;
	(void) tp->t_fb->tfb_tcp_output(tp);
	tp->snd_cwnd = ocwnd;
	if (SEQ_GT(onxt, tp->snd_nxt))
		tp->snd_nxt = onxt;
	/*
	 * Partial window deflation.  Relies on fact that tp->snd_una
	 * not updated yet.
	 */
	if (tp->snd_cwnd > BYTES_THIS_ACK(tp, th))
		tp->snd_cwnd -= BYTES_THIS_ACK(tp, th);
	else
		tp->snd_cwnd = 0;
	tp->snd_cwnd += maxseg;
}

int
tcp_compute_pipe(struct tcpcb *tp)
{
	return (tp->snd_max - tp->snd_una +
		tp->sackhint.sack_bytes_rexmit -
		tp->sackhint.sacked_bytes);
}

uint32_t
tcp_compute_initwnd(struct tcpcb *tp, uint32_t maxseg)
{
	/*
	 * Calculate the Initial Window, also used as Restart Window
	 *
	 * RFC5681 Section 3.1 specifies the default conservative values.
	 * RFC3390 specifies slightly more aggressive values.
	 * RFC6928 increases it to ten segments.
	 * Support for user specified value for initial flight size.
	 */
	if (V_tcp_initcwnd_segments)
		return min(V_tcp_initcwnd_segments * maxseg,
		    max(2 * maxseg, V_tcp_initcwnd_segments * 1460));
	else if (V_tcp_do_rfc3390)
		return min(4 * maxseg, max(2 * maxseg, 4380));
	else {
		/* Per RFC5681 Section 3.1 */
		if (maxseg > 2190)
			return (2 * maxseg);
		else if (maxseg > 1095)
			return (3 * maxseg);
		else
			return (4 * maxseg);
	}
}
