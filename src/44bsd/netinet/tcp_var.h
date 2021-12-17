/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_


#include "tcp.h"
#include "tcp_fsm.h"

#define _WANT_TCPCB

#include <sys/queue.h>      // TAILQ_HEAD, TAILQ_ENTRY, ...
#include "tcp_socket.h"     // struct socket
#include "tcp_timer.h"      // struct tcp_timer
#include "cc/cc.h"          // struct cc_var


#define TCP_END_BYTE_INFO 8	/* Bytes that makeup the "end information array" */
/* Types of ending byte info */
#define TCP_EI_EMPTY_SLOT	0
#define TCP_EI_STATUS_CLIENT_FIN	0x1
#define TCP_EI_STATUS_CLIENT_RST	0x2
#define TCP_EI_STATUS_SERVER_FIN	0x3
#define TCP_EI_STATUS_SERVER_RST	0x4
#define TCP_EI_STATUS_RETRAN		0x5
#define TCP_EI_STATUS_PROGRESS		0x6
#define TCP_EI_STATUS_PERSIST_MAX	0x7
#define TCP_EI_STATUS_KEEP_MAX		0x8
#define TCP_EI_STATUS_DATA_A_CLOSE	0x9
#define TCP_EI_STATUS_RST_IN_FRONT	0xa
#define TCP_EI_STATUS_2MSL		0xb
#define TCP_EI_STATUS_MAX_VALUE		0xb

/************************************************/
/* Status bits we track to assure no duplicates,
 * the bits here are not used by the code but
 * for human representation. To check a bit we
 * take and shift over by 1 minus the value (1-8).
 */
/************************************************/
#define TCP_EI_BITS_CLIENT_FIN	0x001
#define TCP_EI_BITS_CLIENT_RST	0x002
#define TCP_EI_BITS_SERVER_FIN	0x004
#define TCP_EI_BITS_SERVER_RST	0x008
#define TCP_EI_BITS_RETRAN	0x010
#define TCP_EI_BITS_PROGRESS	0x020
#define TCP_EI_BITS_PRESIST_MAX	0x040
#define TCP_EI_BITS_KEEP_MAX	0x080
#define TCP_EI_BITS_DATA_A_CLO  0x100
#define TCP_EI_BITS_RST_IN_FR	0x200	/* a front state reset */
#define TCP_EI_BITS_2MS_TIMER	0x400	/* 2 MSL timer expired */

#if defined(_KERNEL) || defined(_WANT_TCPCB)
#if 0
/* TCP segment queue entry */
struct tseg_qent {
	TAILQ_ENTRY(tseg_qent) tqe_q;
	struct	mbuf   *tqe_m;		/* mbuf contains packet */
	struct  mbuf   *tqe_last;	/* last mbuf in chain */
	tcp_seq tqe_start;		/* TCP Sequence number start */
	int	tqe_len;		/* TCP segment data length */
	uint32_t tqe_flags;		/* The flags from the th->th_flags */
	uint32_t tqe_mbuf_cnt;		/* Count of mbuf overhead */
};
TAILQ_HEAD(tsegqe_head, tseg_qent);
#endif

struct sackblk {
	tcp_seq start;		/* start seq no. of sack block */
	tcp_seq end;		/* end seq no. */
};

struct sackhole {
	tcp_seq start;		/* start seq no. of hole */
	tcp_seq end;		/* end seq no. */
	tcp_seq rxmit;		/* next seq. no in hole to be retransmitted */
	TAILQ_ENTRY(sackhole) scblink;	/* scoreboard linkage */
};

struct sackhint {
	struct sackhole	*nexthole;
	int32_t		sack_bytes_rexmit;
	tcp_seq		last_sack_ack;	/* Most recent/largest sacked ack */

	int32_t		delivered_data; /* Newly acked data from last SACK */

	int32_t		sacked_bytes;	/* Total sacked bytes reported by the
					 * receiver via sack option
					 */
	uint32_t	recover_fs;	/* Flight Size at the start of Loss recovery */
	uint32_t	prr_delivered;	/* Total bytes delivered using PRR */
	uint32_t	prr_out;	/* Bytes sent during IN_RECOVERY */
};

#define SEGQ_EMPTY(tp)  tcp_reass_is_empty(tp)

/*
 * Tcp control block, one per tcp; fields:
 * Organized for 64 byte cacheline efficiency based
 * on common tcp_input/tcp_output processing.
 */
struct tcpcb {
        /* data structures per tcpcb */
	struct tcp_timer m_timer;	/* TCP timer */
	struct cc_var m_ccv;		/* congestion control specific vars */
	struct socket m_socket;

	struct tcp_tune *t_tune;	/* pointer to TCP tunable values */
	struct tcpstat *t_stat;		/* pointer to TCP counters */
	struct tcpstat *t_stat_ex;	/* extra pointer to TCP counters */

	/* Cache line 1 */
	struct tcp_function_block *t_fb;/* TCP function call block */
	u_short t_maxseg;		/* maximum segment size */
	u_short t_state;		/* state of this connection */
	u_int	t_flags;
	tcp_seq	snd_una;		/* sent but unacknowledged */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	uint32_t  snd_wnd;		/* send window */
	uint32_t  snd_cwnd;		/* congestion-controlled window */
	/* Cache line 2 */
	u_int32_t  ts_offset;		/* our timestamp offset */
	int	rcv_numsacks;		/* # distinct sack blks present */
	u_int	t_tsomax;		/* TSO total burst length limit in bytes */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_adv;		/* advertised window */
	uint32_t  rcv_wnd;		/* receive window */
	u_int	t_flags2;		/* More tcpcb flags storage */
	int	t_srtt;			/* smoothed round-trip time */
	int	t_rttvar;		/* variance in round-trip time */
	u_int32_t  ts_recent;		/* timestamp echo data */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	snd_limited;		/* segments limited transmitted */
	u_char	request_r_scale;	/* pending window scaling */
	tcp_seq	last_ack_sent;
	u_int	t_rcvtime;		/* inactivity time */
	/* Cache line 3 */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	void *	t_segq;			/* segment reassembly queue */
	uint32_t  snd_ssthresh;		/* snd_cwnd size threshold for
					 * for slow start exponential to
					 * linear switch
					 */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	/* Cache line 4 */
	tcp_seq	snd_wl2;		/* window update seg ack number */

	tcp_seq	irs;			/* initial receive sequence number */
	tcp_seq	iss;			/* initial send sequence number */
	u_int	t_sndtime;		/* time last data was sent */
	u_int	ts_recent_age;		/* when last updated */
	tcp_seq	snd_recover;		/* for use in NewReno Fast Recovery */
	int	t_rxtcur;		/* current retransmit value (ticks) */

	int	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	u_int	t_rtttime;		/* RTT measurement start time */

	tcp_seq	t_rtseq;		/* sequence number being timed */
	u_int	t_starttime;		/* time connection was established */
	u_int	t_fbyte_in;		/* ticks time when first byte queued in */
	u_int	t_fbyte_out;		/* ticks time when first byte queued out */

	u_int	t_rttmin;		/* minimum rtt allowed */

	u_int	t_rttbest;		/* best rtt we've seen */

	int	t_softerror;		/* possible error not yet reported */
	uint32_t  max_sndwnd;		/* largest window peer has offered */
	/* Cache line 5 */
	uint32_t  snd_cwnd_prev;	/* cwnd prior to retransmit */
	uint32_t  snd_ssthresh_prev;	/* ssthresh prior to retransmit */
	tcp_seq	snd_recover_prev;	/* snd_recover prior to retransmit */
	int	t_sndzerowin;		/* zero-window updates sent */
	u_long	t_rttupdated;		/* number of times rtt sampled */
	int	snd_numholes;		/* number of holes seen by sender */
	u_int	t_badrxtwin;		/* window for retransmit recovery */
	TAILQ_HEAD(sackhole_head, sackhole) snd_holes;
					/* SACK scoreboard (sorted) */
	tcp_seq	snd_fack;		/* last seq number(+1) sack'd by rcv'r*/
	struct sackblk sackblks[MAX_SACK_BLKS]; /* seq nos. of sack blocks */
	struct sackhint	sackhint;	/* SACK scoreboard hint */
	int	t_rttlow;		/* smallest observerved RTT */
	int	t_sndrexmitpack;	/* retransmit packets sent */
	struct cc_algo	*cc_algo;	/* congestion control algorithm */
	struct cc_var	*ccv;		/* congestion control specific vars */
	int	t_bytes_acked;		/* # bytes acked during current RTT */
	int	t_dupacks;		/* consecutive dup acks recd */

};
#endif	/* _KERNEL || _WANT_TCPCB */

#if defined(_KERNEL) || defined(TREX_FBSD)

/*
 * If defining the optional tcp_timers, in the
 * tfb_tcp_timer_stop call you must use the
 * callout_async_drain() function with the
 * tcp_timer_discard callback. You should check
 * the return of callout_async_drain() and if 0
 * increment tt_draincnt. Since the timer sub-system
 * does not know your callbacks you must provide a
 * stop_all function that loops through and calls
 * tcp_timer_stop() with each of your defined timers.
 * Adding a tfb_tcp_handoff_ok function allows the socket
 * option to change stacks to query you even if the
 * connection is in a later stage. You return 0 to
 * say you can take over and run your stack, you return
 * non-zero (an error number) to say no you can't.
 * If the function is undefined you can only change
 * in the early states (before connect or listen).
 * tfb_tcp_fb_fini is changed to add a flag to tell
 * the old stack if the tcb is being destroyed or
 * not. A one in the flag means the TCB is being
 * destroyed, a zero indicates its transitioning to
 * another stack (via socket option).
 */
struct tcp_function_block {
	char tfb_tcp_block_name[TCP_FUNCTION_NAME_LEN_MAX];
	int	(*tfb_tcp_output)(struct tcpcb *);
	void	(*tfb_tcp_do_segment)(struct mbuf *, struct tcphdr *,
			    struct socket *, struct tcpcb *,
		        int, int, uint8_t);
	/* Optional memory allocation/free routine */
	int	(*tfb_tcp_fb_init)(struct tcpcb *);
	void	(*tfb_tcp_fb_fini)(struct tcpcb *, int);
	/* Optional timers, must define all if you define one */
	int	(*tfb_tcp_timer_stop_all)(struct tcpcb *);
	void	(*tfb_tcp_rexmit_tmr)(struct tcpcb *);
};

#endif	/* _KERNEL || TREX_FBSD */

/*
 * Flags and utility macros for the t_flags field.
 */
#define	TF_ACKNOW	0x00000001	/* ack peer immediately */
#define	TF_DELACK	0x00000002	/* ack, but try to delay it */
#define	TF_NODELAY	0x00000004	/* don't delay packets to coalesce */
#define	TF_NOOPT	0x00000008	/* don't use tcp options */
#define	TF_SENTFIN	0x00000010	/* have sent FIN */
#define	TF_REQ_SCALE	0x00000020	/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x00000040	/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x00000080	/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x00000100	/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x00000200	/* other side said I could SACK */
#define	TF_NEEDSYN	0x00000400	/* send SYN (implicit state) */
#define	TF_NEEDFIN	0x00000800	/* send FIN (implicit state) */
#define	TF_NOPUSH	0x00001000	/* don't push */
#define	TF_PREVVALID	0x00002000	/* saved values for bad rxmit valid */
#define	TF_WAKESOR	0x00004000	/* wake up receive socket */
#define	TF_GPUTINPROG	0x00008000	/* Goodput measurement in progress */
#define	TF_MORETOCOME	0x00010000	/* More data to be appended to sock */
#define	TF_LQ_OVERFLOW	0x00020000	/* listen queue overflow */
#define	TF_LASTIDLE	0x00040000	/* connection was previously idle */
#define	TF_RXWIN0SENT	0x00080000	/* sent a receiver win 0 in response */
#define	TF_FASTRECOVERY	0x00100000	/* in NewReno Fast Recovery */
#define	TF_WASFRECOVERY	0x00200000	/* was in NewReno Fast Recovery */
#define	TF_SIGNATURE	0x00400000	/* require MD5 digests (RFC2385) */
#define	TF_FORCEDATA	0x00800000	/* force out a byte */
#define	TF_TSO		0x01000000	/* TSO enabled on this connection */
#define	TF_TOE		0x02000000	/* this connection is offloaded */
#define	TF_WAKESOW	0x04000000	/* wake up send socket */
#define	TF_UNUSED1	0x08000000	/* unused */
#define	TF_UNUSED2	0x10000000	/* unused */
#define	TF_CONGRECOVERY	0x20000000	/* congestion recovery mode */
#define	TF_WASCRECOVERY	0x40000000	/* was in congestion recovery */
#define	TF_FASTOPEN	0x80000000	/* TCP Fast Open indication */

#define	IN_FASTRECOVERY(t_flags)	(t_flags & TF_FASTRECOVERY)
#define	ENTER_FASTRECOVERY(t_flags)	t_flags |= TF_FASTRECOVERY
#define	EXIT_FASTRECOVERY(t_flags)	t_flags &= ~TF_FASTRECOVERY

#define	IN_CONGRECOVERY(t_flags)	(t_flags & TF_CONGRECOVERY)
#define	ENTER_CONGRECOVERY(t_flags)	t_flags |= TF_CONGRECOVERY
#define	EXIT_CONGRECOVERY(t_flags)	t_flags &= ~TF_CONGRECOVERY

#define	IN_RECOVERY(t_flags) (t_flags & (TF_CONGRECOVERY | TF_FASTRECOVERY))
#define	ENTER_RECOVERY(t_flags) t_flags |= (TF_CONGRECOVERY | TF_FASTRECOVERY)
#define	EXIT_RECOVERY(t_flags) t_flags &= ~(TF_CONGRECOVERY | TF_FASTRECOVERY)

#define	BYTES_THIS_ACK(tp, th)	(th->th_ack - tp->snd_una)

/*
 * Flags for the t_oobflags field.
 */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02

/*
 * Flags for the extended TCP flags field, t_flags2
 */
#define	TF2_PLPMTU_BLACKHOLE	0x00000001 /* Possible PLPMTUD Black Hole. */
#define	TF2_PLPMTU_PMTUD	0x00000002 /* Allowed to attempt PLPMTUD. */
#define	TF2_PLPMTU_MAXSEGSNT	0x00000004 /* Last seg sent was full seg. */
#define	TF2_LOG_AUTO		0x00000008 /* Session is auto-logging. */
#define TF2_DROP_AF_DATA 	0x00000010 /* Drop after all data ack'd */
#define	TF2_ECN_PERMIT		0x00000020 /* connection ECN-ready */
#define	TF2_ECN_SND_CWR		0x00000040 /* ECN CWR in queue */
#define	TF2_ECN_SND_ECE		0x00000080 /* ECN ECE in queue */
#define	TF2_ACE_PERMIT		0x00000100 /* Accurate ECN mode */
#define TF2_FBYTES_COMPLETE	0x00000400 /* We have first bytes in and out */
/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions and tcp_addoptions.
 * The binary order of the to_flags is relevant for packing of the
 * options in tcp_addoptions.
 */
struct tcpopt {
	u_int32_t	to_flags;	/* which options are present */
#define	TOF_MSS		0x0001		/* maximum segment size */
#define	TOF_SCALE	0x0002		/* window scaling */
#define	TOF_SACKPERM	0x0004		/* SACK permitted */
#define	TOF_TS		0x0010		/* timestamp */
#define	TOF_SIGNATURE	0x0040		/* TCP-MD5 signature option (RFC2385) */
#define	TOF_SACK	0x0080		/* Peer sent SACK option */
#define	TOF_FASTOPEN	0x0100		/* TCP Fast Open (TFO) cookie */
#define	TOF_MAXOPT	0x0200
	u_int32_t	to_tsval;	/* new timestamp */
	u_int32_t	to_tsecr;	/* reflected timestamp */
	u_char		*to_sacks;	/* pointer to the first SACK blocks */
	u_char		*to_signature;	/* pointer to the TCP-MD5 signature */
	u_int8_t	*to_tfo_cookie; /* pointer to the TFO cookie */
	u_int16_t	to_mss;		/* maximum segment size */
	u_int8_t	to_wscale;	/* window scaling */
	u_int8_t	to_nsacks;	/* number of SACK blocks */
	u_int8_t	to_tfo_len;	/* TFO cookie length */
	u_int32_t	to_spare;	/* UTO */
};

/*
 * Flags for tcp_dooptions.
 */
#define	TO_SYN		0x01		/* parse SYN-only options */

#if 0
struct hc_metrics_lite {	/* must stay in sync with hc_metrics */
	uint32_t	rmx_mtu;	/* MTU for this path */
	uint32_t	rmx_ssthresh;	/* outbound gateway buffer limit */
	uint32_t	rmx_rtt;	/* estimated round trip time */
	uint32_t	rmx_rttvar;	/* estimated rtt variance */
	uint32_t	rmx_cwnd;	/* congestion window */
	uint32_t	rmx_sendpipe;   /* outbound delay-bandwidth product */
	uint32_t	rmx_recvpipe;   /* inbound delay-bandwidth product */
};

/*
 * Used by tcp_maxmtu() to communicate interface specific features
 * and limits at the time of connection setup.
 */
struct tcp_ifcap {
	int	ifcap;
	u_int	tsomax;
	u_int	tsomaxsegcount;
	u_int	tsomaxsegsize;
};

#ifndef _NETINET_IN_PCB_H_
struct in_conninfo;
#endif /* _NETINET_IN_PCB_H_ */

struct tcptw {
	struct inpcb	*tw_inpcb;	/* XXX back pointer to internet pcb */
	tcp_seq		snd_nxt;
	tcp_seq		rcv_nxt;
	tcp_seq		iss;
	tcp_seq		irs;
	u_short		last_win;	/* cached window value */
	short		tw_so_options;	/* copy of so_options */
	struct ucred	*tw_cred;	/* user credentials */
	u_int32_t	t_recent;
	u_int32_t	ts_offset;	/* our timestamp offset */
	u_int		t_starttime;
	int		tw_time;
	TAILQ_ENTRY(tcptw) tw_2msl;
	void		*tw_pspare;	/* TCP_SIGNATURE */
	u_int		*tw_spare;	/* TCP_SIGNATURE */
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	intotw(ip)	((struct tcptw *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))
#endif

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		32	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		5	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	16	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	4	/* shift for rttvar; 2 bits */
#define	TCP_DELTA_SHIFT		2	/* see tcp_input.c */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define	TCP_REXMTVAL(tp) \
	max((tp)->t_rttmin, (((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	uint64_t tcps_connattempt;	/* connections initiated */
	uint64_t tcps_accepts;		/* connections accepted */
	uint64_t tcps_connects;		/* connections established */
	uint64_t tcps_drops;		/* connections dropped */
	uint64_t tcps_conndrops;	/* embryonic connections dropped */
	uint64_t tcps_closed;		/* conn. closed (includes drops) */
	uint64_t tcps_segstimed;	/* segs where we tried to get rtt */
	uint64_t tcps_rttupdated;	/* times we succeeded */
	uint64_t tcps_delack;		/* delayed acks sent */
	uint64_t tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	uint64_t tcps_rexmttimeo;	/* retransmit timeouts */
	uint64_t tcps_rexmttimeo_syn;	/* TREX_FBSD: retransmit SYN timeouts */
	uint64_t tcps_persisttimeo;	/* persist timeouts */
	uint64_t tcps_keeptimeo;	/* keepalive timeouts */
	uint64_t tcps_keepprobe;	/* keepalive probes sent */
	uint64_t tcps_keepdrops;	/* connections dropped in keepalive */

	uint64_t tcps_sndtotal;		/* total packets sent */
	uint64_t tcps_sndpack;		/* data packets sent */
	uint64_t tcps_sndbyte;		/* data bytes sent (TREX_FBSD: by application layer) */
	uint64_t tcps_sndbyte_ok;	/* TREX_FBSD: data bytes sent by TCP */
	uint64_t tcps_sndrexmitpack;	/* data packets retransmitted */
	uint64_t tcps_sndrexmitbyte;	/* data bytes retransmitted */
	uint64_t tcps_sndrexmitbad;	/* unnecessary packet retransmissions */
	uint64_t tcps_sndacks;		/* ack-only packets sent */
	uint64_t tcps_sndprobe;		/* window probes sent */
	uint64_t tcps_sndurg;		/* packets sent with URG only */
	uint64_t tcps_sndwinup;		/* window update-only packets sent */
	uint64_t tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	uint64_t tcps_rcvtotal;		/* total packets received */
	uint64_t tcps_rcvpack;		/* packets received in sequence */
	uint64_t tcps_rcvbyte;		/* bytes received in sequence */
	uint64_t tcps_rcvbadsum;	/* packets received with ccksum errs */
	uint64_t tcps_rcvbadoff;	/* packets received with bad offset */
	uint64_t tcps_rcvreassfull;	/* packets dropped for no reass space */
	uint64_t tcps_rcvshort;		/* packets received too short */
	uint64_t tcps_rcvduppack;	/* duplicate-only packets received */
	uint64_t tcps_rcvdupbyte;	/* duplicate-only bytes received */
	uint64_t tcps_rcvpartduppack;	/* packets with some duplicate data */
	uint64_t tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	uint64_t tcps_rcvoopack;	/* out-of-order packets received */
	uint64_t tcps_rcvoobyte;	/* out-of-order bytes received */
	uint64_t tcps_rcvpackafterwin;	/* packets with data after window */
	uint64_t tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	uint64_t tcps_rcvafterclose;	/* packets rcvd after "close" */
	uint64_t tcps_rcvwinprobe;	/* rcvd window probe packets */
	uint64_t tcps_rcvdupack;	/* rcvd duplicate acks */
	uint64_t tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	uint64_t tcps_rcvackpack;	/* rcvd ack packets */
	uint64_t tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	uint64_t tcps_rcvackbyte_of;	/* TREX_FBSD: bytes acked by rcvd acks (overflowed) */
	uint64_t tcps_rcvwinupd;	/* rcvd window update packets */
	uint64_t tcps_pawsdrop;		/* segments dropped due to PAWS */
	uint64_t tcps_predack;		/* times hdr predict ok for acks */
	uint64_t tcps_preddat;		/* times hdr predict ok for data pkts */
	uint64_t tcps_persistdrop;	/* timeout in persist state */
	uint64_t tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
	uint64_t tcps_badrst;		/* ignored RSTs in the window */

	/* SACK related stats */
	uint64_t tcps_sack_recovery_episode; /* SACK recovery episodes */
	uint64_t tcps_sack_rexmits;	    /* SACK rexmit segments   */
	uint64_t tcps_sack_rexmit_bytes;    /* SACK rexmit bytes      */
	uint64_t tcps_sack_rcv_blocks;	    /* SACK blocks (options) received */
	uint64_t tcps_sack_send_blocks;	    /* SACK blocks (options) sent     */
	uint64_t tcps_sack_sboverflow;	    /* times scoreboard overflowed */

	/* ECN related stats */
	uint64_t tcps_ecn_ce;		/* ECN Congestion Experienced */
	uint64_t tcps_ecn_ect0;		/* ECN Capable Transport */
	uint64_t tcps_ecn_ect1;		/* ECN Capable Transport */
	uint64_t tcps_ecn_shs;		/* ECN successful handshakes */
	uint64_t tcps_ecn_rcwnd;	/* # times ECN reduced the cwnd */

};

#define	tcps_rcvmemdrop	tcps_rcvreassfull	/* compat */

#define TCPSTAT_ADD(name, val)                  \
    do {                                        \
        tp->t_stat->name += val;                \
        if (tp->t_stat_ex)                      \
            tp->t_stat_ex->name += val;         \
    } while(0)
#define TCPSTAT_INC(name)           TCPSTAT_ADD(name, 1)


/* localized global TCP tunable values */
struct tcp_tune {
        int tcp_do_rfc1323;     /* (1) Enable rfc1323 (high performance TCP) extensions */
        int tcp_do_sack;        /* (1) Enable/Disable TCP SACK support */
        int tcp_mssdflt;        /* (TCP_MSS) Default TCP Maximum Segment Size */
        //int tcp_v6mssdflt;      /* (TCP6_MSS) Default TCP Maximum Segment Size for IPv6 */
        int tcp_initcwnd_segments;  /* (10) Slow-start flight size (initial congestion window) in number of segments (10) */
        int tcprexmtthresh;     /* (3) number of duplicate ack to trigger retransmission */

        int tcp_keepinit;       /* (TCPTV_KEEP_INIT) time to establish connection */
        int tcp_keepidle;       /* (TCPTV_KEEP_IDLE) time before keepalive probes begin */
        int tcp_keepintvl;      /* (TCPTV_KEEPINTVL) time between keepalive probes */
        int tcp_keepcnt;        /* (TCPTV_KEEPCNT) Number of keepalive probes to send */
        int tcp_delacktime;     /* (TCPTV_DELACK) Time before a delayed ACK is sent */
};

#define TCP_TUNE(name)              tp->t_tune->name

//#define V_path_mtu_discovery        0
#define V_tcp_insecure_rst          0
#define V_tcp_insecure_syn          0
#define V_drop_synfin               0

#define V_tcp_do_tso                1
#define V_tcp_delack_enabled        1

#define V_tcp_do_sack               TCP_TUNE(tcp_do_sack)
#define V_tcp_sack_maxholes         128
//#define V_tcp_sack_globalmaxholes   65536

#define V_tcp_do_prr                1
#define V_tcp_do_prr_conservative   0
#define V_tcp_do_rfc6675_pipe       0

#define V_tcp_do_rfc3390            1
#define V_tcp_do_rfc3042            1

#define V_tcp_do_ecn                2
#define V_tcp_ecn_maxretries        1

#define V_tcp_minmss                216
#define V_tcp_tolerate_missing_ts   0

#define V_tcp_do_newcwv             0

#define V_tcp_mssdflt               TCP_TUNE(tcp_mssdflt)
#define V_tcp_v6mssdflt             TCP_TUNE(tcp_mssdflt)
#define V_tcp_initcwnd_segments     TCP_TUNE(tcp_initcwnd_segments)    // 10
#define V_tcp_do_rfc1323            TCP_TUNE(tcp_do_rfc1323)           // 1
#define V_tcprexmtthresh            TCP_TUNE(tcprexmtthresh)           // 3
#define V_tcp_delacktime            TCP_TUNE(tcp_delacktime)


#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* provided functions */
#define tcp_int_output(tp)  (tp)->t_fb->tfb_tcp_output(tp)
int tcp_output(struct tcpcb *tp);
#define tcp_int_respond(tp,ack,seq,flags)   tcp_respond(tp, NULL, NULL, NULL, ack, seq, flags)
void tcp_respond(struct tcpcb *tp, void *, struct tcphdr *, struct mbuf *, tcp_seq ack, tcp_seq seq, int flags);
#define tcp_int_input(tp,m,th,toff,tlen,iptos)  tcp_input(tp, m, th, toff, tlen, iptos)
void tcp_input(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th, int toff, int tlen, uint8_t iptos);
void tcp_handle_timers(struct tcpcb *tp);
void tcp_timer_activate(struct tcpcb *, uint32_t, u_int);
struct tcpcb* tcp_inittcpcb(struct tcpcb *tp, struct tcp_function_block *fb, struct cc_algo *cc_algo, struct tcp_tune *tune, struct tcpstat *stat);
void tcp_discardcb(struct tcpcb *tp);
struct tcpcb * tcp_drop(struct tcpcb *, int res);
struct tcpcb * tcp_close(struct tcpcb *);
int tcp_connect(struct tcpcb *);
int tcp_listen(struct tcpcb *);
void tcp_disconnect(struct tcpcb *);
void tcp_usrclosed(struct tcpcb *);

/* required functions */
uint32_t tcp_getticks(struct tcpcb *tp);
int tcp_build_pkt(struct tcpcb *tp, uint32_t off, uint32_t len, uint16_t hdrlen, uint16_t optlen, struct mbuf **mp, struct tcphdr **thp);
int tcp_ip_output(struct tcpcb *tp, struct mbuf *m);
int tcp_reass(struct tcpcb *tp, struct tcphdr *th, tcp_seq *seq_start, int *tlenp, struct mbuf *m);
bool tcp_reass_is_empty(struct tcpcb *tp);
bool tcp_check_no_delay(struct tcpcb *, int);
bool tcp_isipv6(struct tcpcb *);
//struct socket* tcp_getsocket(struct tcpcb *);
#define tcp_getsocket(tp)       (&(tp)->m_socket)
uint32_t tcp_new_isn(struct tcpcb *);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* _NETINET_TCP_VAR_H_ */
