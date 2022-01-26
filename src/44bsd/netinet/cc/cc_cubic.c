/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2008-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Lawrence Stewart while studying at the Centre
 * for Advanced Internet Architectures, Swinburne University of Technology, made
 * possible in part by a grant from the Cisco University Research Program Fund
 * at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * An implementation of the CUBIC congestion control algorithm for FreeBSD,
 * based on the Internet Draft "draft-rhee-tcpm-cubic-02" by Rhee, Xu and Ha.
 * Originally released as part of the NewTCP research project at Swinburne
 * University of Technology's Centre for Advanced Internet Architectures,
 * Melbourne, Australia, which was made possible in part by a grant from the
 * Cisco University Research Program Fund at Community Foundation Silicon
 * Valley. More details are available at:
 *   http://caia.swin.edu.au/urp/newtcp/
 */

#ifdef  TREX_FBSD

#include "sys_inet.h"
#include "tcp_int.h"

#include <netinet/cc/cc_cubic.h>

/* tcp_input.c */
extern int tcp_compute_pipe(struct tcpcb *);
/* tcp_subr.c */
extern u_int tcp_maxseg(const struct tcpcb *);

#define ticks   tcp_getticks(ccv->ccvc.tcp)

#else   /* !TREX_FBSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/vnet.h>

#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/cc/cc.h>
#include <netinet/cc/cc_cubic.h>
#include <netinet/cc/cc_module.h>

#endif  /* !TREX_FBSD */

static void	cubic_ack_received(struct cc_var *ccv, uint16_t type);
static void	cubic_cb_destroy(struct cc_var *ccv);
static int	cubic_cb_init(struct cc_var *ccv);
static void	cubic_cong_signal(struct cc_var *ccv, uint32_t type);
static void	cubic_conn_init(struct cc_var *ccv);
#ifndef TREX_FBSD
static int	cubic_mod_init(void);
#endif
static void	cubic_post_recovery(struct cc_var *ccv);
static void	cubic_record_rtt(struct cc_var *ccv);
static void	cubic_ssthresh_update(struct cc_var *ccv, uint32_t maxseg);
static void	cubic_after_idle(struct cc_var *ccv);

struct cubic {
	/* Cubic K in fixed point form with CUBIC_SHIFT worth of precision. */
	int64_t		K;
	/* Sum of RTT samples across an epoch in ticks. */
	int64_t		sum_rtt_ticks;
	/* cwnd at the most recent congestion event. */
	unsigned long	max_cwnd;
	/* cwnd at the previous congestion event. */
	unsigned long	prev_max_cwnd;
	/* A copy of prev_max_cwnd. Used for CC_RTO_ERR */
	unsigned long	prev_max_cwnd_cp;
	/* various flags */
	uint32_t	flags;
#define CUBICFLAG_CONG_EVENT	0x00000001	/* congestion experienced */
#define CUBICFLAG_IN_SLOWSTART	0x00000002	/* in slow start */
#define CUBICFLAG_IN_APPLIMIT	0x00000004	/* application limited */
#define CUBICFLAG_RTO_EVENT	0x00000008	/* RTO experienced */
	/* Minimum observed rtt in ticks. */
	int		min_rtt_ticks;
	/* Mean observed rtt between congestion epochs. */
	int		mean_rtt_ticks;
	/* ACKs since last congestion event. */
	int		epoch_ack_count;
	/* Timestamp (in ticks) of arriving in congestion avoidance from last
	 * congestion event.
	 */
	int		t_last_cong;
	/* Timestamp (in ticks) of a previous congestion event. Used for
	 * CC_RTO_ERR.
	 */
	int		t_last_cong_prev;
};

#ifndef TREX_FBSD
static MALLOC_DEFINE(M_CUBIC, "cubic data",
    "Per connection data required for the CUBIC congestion control algorithm");
#endif

struct cc_algo cubic_cc_algo = {
	.name = "cubic",
	.ack_received = cubic_ack_received,
	.cb_destroy = cubic_cb_destroy,
	.cb_init = cubic_cb_init,
	.cong_signal = cubic_cong_signal,
	.conn_init = cubic_conn_init,
#ifndef TREX_FBSD
	.mod_init = cubic_mod_init,
#endif
	.post_recovery = cubic_post_recovery,
	.after_idle = cubic_after_idle,
};

static void
cubic_ack_received(struct cc_var *ccv, uint16_t type)
{
	struct cubic *cubic_data;
	unsigned long w_tf, w_cubic_next;
	int ticks_since_cong;

	cubic_data = ccv->cc_data;
	cubic_record_rtt(ccv);

	/*
	 * For a regular ACK and we're not in cong/fast recovery and
	 * we're cwnd limited, always recalculate cwnd.
	 */
	if (type == CC_ACK && !IN_RECOVERY(CCV(ccv, t_flags)) &&
	    (ccv->flags & CCF_CWND_LIMITED)) {
		 /* Use the logic in NewReno ack_received() for slow start. */
		if (CCV(ccv, snd_cwnd) <= CCV(ccv, snd_ssthresh) ||
		    cubic_data->min_rtt_ticks == TCPTV_SRTTBASE) {
			cubic_data->flags |= CUBICFLAG_IN_SLOWSTART;
			newreno_cc_algo.ack_received(ccv, type);
		} else {
			if ((cubic_data->flags & CUBICFLAG_RTO_EVENT) &&
			    (cubic_data->flags & CUBICFLAG_IN_SLOWSTART)) {
				/* RFC8312 Section 4.7 */
				cubic_data->flags &= ~(CUBICFLAG_RTO_EVENT |
						       CUBICFLAG_IN_SLOWSTART);
				cubic_data->max_cwnd = CCV(ccv, snd_cwnd);
				cubic_data->K = 0;
			} else if (cubic_data->flags & (CUBICFLAG_IN_SLOWSTART |
						 CUBICFLAG_IN_APPLIMIT)) {
				cubic_data->flags &= ~(CUBICFLAG_IN_SLOWSTART |
						       CUBICFLAG_IN_APPLIMIT);
				cubic_data->t_last_cong = ticks;
				cubic_data->K = cubic_k(cubic_data->max_cwnd /
							CCV(ccv, t_maxseg));
			}
			if ((ticks_since_cong =
			    ticks - cubic_data->t_last_cong) < 0) {
				/*
				 * dragging t_last_cong along
				 */
				ticks_since_cong = INT_MAX;
				cubic_data->t_last_cong = ticks - INT_MAX;
			}
			/*
			 * The mean RTT is used to best reflect the equations in
			 * the I-D. Using min_rtt in the tf_cwnd calculation
			 * causes w_tf to grow much faster than it should if the
			 * RTT is dominated by network buffering rather than
			 * propagation delay.
			 */
			w_tf = tf_cwnd(ticks_since_cong,
			    cubic_data->mean_rtt_ticks, cubic_data->max_cwnd,
			    CCV(ccv, t_maxseg));

			w_cubic_next = cubic_cwnd(ticks_since_cong +
			    cubic_data->mean_rtt_ticks, cubic_data->max_cwnd,
			    CCV(ccv, t_maxseg), cubic_data->K);

			ccv->flags &= ~CCF_ABC_SENTAWND;

			if (w_cubic_next < w_tf) {
				/*
				 * TCP-friendly region, follow tf
				 * cwnd growth.
				 */
				if (CCV(ccv, snd_cwnd) < w_tf)
					CCV(ccv, snd_cwnd) = ulmin(w_tf, INT_MAX);
			} else if (CCV(ccv, snd_cwnd) < w_cubic_next) {
				/*
				 * Concave or convex region, follow CUBIC
				 * cwnd growth.
				 * Only update snd_cwnd, if it doesn't shrink.
				 */
				CCV(ccv, snd_cwnd) = ulmin(w_cubic_next,
				    INT_MAX);
			}

			/*
			 * If we're not in slow start and we're probing for a
			 * new cwnd limit at the start of a connection
			 * (happens when hostcache has a relevant entry),
			 * keep updating our current estimate of the
			 * max_cwnd.
			 */
			if (((cubic_data->flags & CUBICFLAG_CONG_EVENT) == 0) &&
			    cubic_data->max_cwnd < CCV(ccv, snd_cwnd)) {
				cubic_data->max_cwnd = CCV(ccv, snd_cwnd);
				cubic_data->K = cubic_k(cubic_data->max_cwnd /
				    CCV(ccv, t_maxseg));
			}
		}
	} else if (type == CC_ACK && !IN_RECOVERY(CCV(ccv, t_flags)) &&
	    !(ccv->flags & CCF_CWND_LIMITED)) {
		cubic_data->flags |= CUBICFLAG_IN_APPLIMIT;
	}
}

/*
 * This is a Cubic specific implementation of after_idle.
 *   - Reset cwnd by calling New Reno implementation of after_idle.
 *   - Reset t_last_cong.
 */
static void
cubic_after_idle(struct cc_var *ccv)
{
	struct cubic *cubic_data;

	cubic_data = ccv->cc_data;

	cubic_data->max_cwnd = ulmax(cubic_data->max_cwnd, CCV(ccv, snd_cwnd));
	cubic_data->K = cubic_k(cubic_data->max_cwnd / CCV(ccv, t_maxseg));

	newreno_cc_algo.after_idle(ccv);
	cubic_data->t_last_cong = ticks;
}

static void
cubic_cb_destroy(struct cc_var *ccv)
{
#ifndef TREX_FBSD
	free(ccv->cc_data, M_CUBIC);
#else
	free(ccv->cc_data);
#endif
}

static int
cubic_cb_init(struct cc_var *ccv)
{
	struct cubic *cubic_data;

#ifndef TREX_FBSD
	cubic_data = malloc(sizeof(struct cubic), M_CUBIC, M_NOWAIT|M_ZERO);
#else
	cubic_data = malloc(sizeof(struct cubic));
#endif

	if (cubic_data == NULL)
		return (ENOMEM);

	/* Init some key variables with sensible defaults. */
	cubic_data->t_last_cong = ticks;
	cubic_data->min_rtt_ticks = TCPTV_SRTTBASE;
	cubic_data->mean_rtt_ticks = 1;

	ccv->cc_data = cubic_data;

	return (0);
}

/*
 * Perform any necessary tasks before we enter congestion recovery.
 */
static void
cubic_cong_signal(struct cc_var *ccv, uint32_t type)
{
	struct cubic *cubic_data;
	u_int mss;

	cubic_data = ccv->cc_data;
	mss = tcp_maxseg(ccv->ccvc.tcp);

	switch (type) {
	case CC_NDUPACK:
		if (!IN_FASTRECOVERY(CCV(ccv, t_flags))) {
			if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
				cubic_ssthresh_update(ccv, mss);
				cubic_data->flags |= CUBICFLAG_CONG_EVENT;
				cubic_data->t_last_cong = ticks;
				cubic_data->K = cubic_k(cubic_data->max_cwnd / mss);
			}
			ENTER_RECOVERY(CCV(ccv, t_flags));
		}
		break;

	case CC_ECN:
		if (!IN_CONGRECOVERY(CCV(ccv, t_flags))) {
			cubic_ssthresh_update(ccv, mss);
			cubic_data->flags |= CUBICFLAG_CONG_EVENT;
			cubic_data->t_last_cong = ticks;
			cubic_data->K = cubic_k(cubic_data->max_cwnd / mss);
			CCV(ccv, snd_cwnd) = CCV(ccv, snd_ssthresh);
			ENTER_CONGRECOVERY(CCV(ccv, t_flags));
		}
		break;

	case CC_RTO:
		/* RFC8312 Section 4.7 */
		if (CCV(ccv, t_rxtshift) == 1) {
			cubic_data->t_last_cong_prev = cubic_data->t_last_cong;
			cubic_data->prev_max_cwnd_cp = cubic_data->prev_max_cwnd;
		}
		cubic_data->flags |= CUBICFLAG_CONG_EVENT | CUBICFLAG_RTO_EVENT;
		cubic_data->prev_max_cwnd = cubic_data->max_cwnd;
		CCV(ccv, snd_ssthresh) = ((uint64_t)CCV(ccv, snd_cwnd) *
					  CUBIC_BETA) >> CUBIC_SHIFT;
		CCV(ccv, snd_cwnd) = mss;
		break;

	case CC_RTO_ERR:
		cubic_data->flags &= ~(CUBICFLAG_CONG_EVENT | CUBICFLAG_RTO_EVENT);
		cubic_data->max_cwnd = cubic_data->prev_max_cwnd;
		cubic_data->prev_max_cwnd = cubic_data->prev_max_cwnd_cp;
		cubic_data->t_last_cong = cubic_data->t_last_cong_prev;
		cubic_data->K = cubic_k(cubic_data->max_cwnd / mss);
		break;
	}
}

static void
cubic_conn_init(struct cc_var *ccv)
{
	struct cubic *cubic_data;

	cubic_data = ccv->cc_data;

	/*
	 * Ensure we have a sane initial value for max_cwnd recorded. Without
	 * this here bad things happen when entries from the TCP hostcache
	 * get used.
	 */
	cubic_data->max_cwnd = CCV(ccv, snd_cwnd);
}

#ifndef TREX_FBSD
static int
cubic_mod_init(void)
{
	return (0);
}
#endif /* !TREX_FBSD */

/*
 * Perform any necessary tasks before we exit congestion recovery.
 */
static void
cubic_post_recovery(struct cc_var *ccv)
{
	struct cubic *cubic_data;
	int pipe;

	cubic_data = ccv->cc_data;
	pipe = 0;

	if (IN_FASTRECOVERY(CCV(ccv, t_flags))) {
		/*
		 * If inflight data is less than ssthresh, set cwnd
		 * conservatively to avoid a burst of data, as suggested in
		 * the NewReno RFC. Otherwise, use the CUBIC method.
		 *
		 * XXXLAS: Find a way to do this without needing curack
		 */
		if (V_tcp_do_rfc6675_pipe)
			pipe = tcp_compute_pipe(ccv->ccvc.tcp);
		else
			pipe = CCV(ccv, snd_max) - ccv->curack;

		if (pipe < CCV(ccv, snd_ssthresh))
			/*
			 * Ensure that cwnd does not collapse to 1 MSS under
			 * adverse conditions. Implements RFC6582
			 */
			CCV(ccv, snd_cwnd) = max(pipe, CCV(ccv, t_maxseg)) +
			    CCV(ccv, t_maxseg);
		else
			/* Update cwnd based on beta and adjusted max_cwnd. */
			CCV(ccv, snd_cwnd) = max(((uint64_t)cubic_data->max_cwnd *
			    CUBIC_BETA) >> CUBIC_SHIFT,
			    2 * CCV(ccv, t_maxseg));
	}

	/* Calculate the average RTT between congestion epochs. */
	if (cubic_data->epoch_ack_count > 0 &&
	    cubic_data->sum_rtt_ticks >= cubic_data->epoch_ack_count) {
		cubic_data->mean_rtt_ticks = (int)(cubic_data->sum_rtt_ticks /
		    cubic_data->epoch_ack_count);
	}

	cubic_data->epoch_ack_count = 0;
	cubic_data->sum_rtt_ticks = 0;
}

/*
 * Record the min RTT and sum samples for the epoch average RTT calculation.
 */
static void
cubic_record_rtt(struct cc_var *ccv)
{
	struct cubic *cubic_data;
	int t_srtt_ticks;

	/* Ignore srtt until a min number of samples have been taken. */
	if (CCV(ccv, t_rttupdated) >= CUBIC_MIN_RTT_SAMPLES) {
		cubic_data = ccv->cc_data;
		t_srtt_ticks = CCV(ccv, t_srtt) / TCP_RTT_SCALE;

		/*
		 * Record the current SRTT as our minrtt if it's the smallest
		 * we've seen or minrtt is currently equal to its initialised
		 * value.
		 *
		 * XXXLAS: Should there be some hysteresis for minrtt?
		 */
		if ((t_srtt_ticks < cubic_data->min_rtt_ticks ||
		    cubic_data->min_rtt_ticks == TCPTV_SRTTBASE)) {
			cubic_data->min_rtt_ticks = max(1, t_srtt_ticks);

			/*
			 * If the connection is within its first congestion
			 * epoch, ensure we prime mean_rtt_ticks with a
			 * reasonable value until the epoch average RTT is
			 * calculated in cubic_post_recovery().
			 */
			if (cubic_data->min_rtt_ticks >
			    cubic_data->mean_rtt_ticks)
				cubic_data->mean_rtt_ticks =
				    cubic_data->min_rtt_ticks;
		}

		/* Sum samples for epoch average RTT calculation. */
		cubic_data->sum_rtt_ticks += t_srtt_ticks;
		cubic_data->epoch_ack_count++;
	}
}

/*
 * Update the ssthresh in the event of congestion.
 */
static void
cubic_ssthresh_update(struct cc_var *ccv, uint32_t maxseg)
{
	struct cubic *cubic_data;
	uint32_t ssthresh;
	uint32_t cwnd;

	cubic_data = ccv->cc_data;
	cwnd = CCV(ccv, snd_cwnd);

	/* Fast convergence heuristic. */
	if (cwnd < cubic_data->max_cwnd) {
		cwnd = ((uint64_t)cwnd * CUBIC_FC_FACTOR) >> CUBIC_SHIFT;
	}
	cubic_data->prev_max_cwnd = cubic_data->max_cwnd;
	cubic_data->max_cwnd = cwnd;

	/*
	 * On the first congestion event, set ssthresh to cwnd * 0.5
	 * and reduce max_cwnd to cwnd * beta. This aligns the cubic concave
	 * region appropriately. On subsequent congestion events, set
	 * ssthresh to cwnd * beta.
	 */
	if ((cubic_data->flags & CUBICFLAG_CONG_EVENT) == 0) {
		ssthresh = cwnd >> 1;
		cubic_data->max_cwnd = ((uint64_t)cwnd *
		    CUBIC_BETA) >> CUBIC_SHIFT;
	} else {
		ssthresh = ((uint64_t)cwnd *
		    CUBIC_BETA) >> CUBIC_SHIFT;
	}
	CCV(ccv, snd_ssthresh) = max(ssthresh, 2 * maxseg);
}

#ifndef TREX_FBSD
DECLARE_CC_MODULE(cubic, &cubic_cc_algo);
MODULE_VERSION(cubic, 1);
#endif
