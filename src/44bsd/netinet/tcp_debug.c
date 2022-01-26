/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)tcp_debug.c	8.1 (Berkeley) 6/10/93
 */

#ifdef TREX_FBSD

#include "sys_inet.h"

#ifdef _DEBUG
#define TCPSTATES
#define TCPTIMERS
#define TANAMES
#endif
#include "tcp_int.h"

#ifdef TCPDEBUG
// <sys/protosw.h>
static const char *prurequests[] = {
        "ATTACH",       "DETACH",       "BIND",         "LISTEN",
        "CONNECT",      "ACCEPT",       "DISCONNECT",   "SHUTDOWN",
        "RCVD",         "SEND",         "ABORT",        "CONTROL",
        "SENSE",        "RCVOOB",       "SENDOOB",      "SOCKADDR",
        "PEERADDR",     "CONNECT2",     "FASTTIMO",     "SLOWTIMO",
        "PROTORCV",     "PROTOSEND",    "SEND_EOF",     "SOSETLABEL",
        "CLOSE",        "FLUSH",
};

static const int tcpconsdebug = 1;
#endif

void tcp_trace(short act, short ostate, struct tcpcb *tp, void *ipgen, struct tcphdr *th, int req);

#else /* !TREX_FBSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#ifdef TCPDEBUG
/* load symbolic names */
#define PRUREQUESTS
#define TCPSTATES
#define	TCPTIMERS
#define	TANAMES
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#ifdef TCPDEBUG
static int		tcpconsdebug = 0;
#endif

/*
 * Global ring buffer of TCP debugging state.  Each entry captures a snapshot
 * of TCP connection state at any given moment.  tcp_debx addresses at the
 * next available slot.  There is no explicit export of this data structure;
 * it will be read via /dev/kmem by debugging tools.
 */
static struct tcp_debug	tcp_debug[TCP_NDEBUG];
static int		tcp_debx;

/*
 * All global state is protected by tcp_debug_mtx; tcp_trace() is split into
 * two parts, one of which saves connection and other state into the global
 * array (locked by tcp_debug_mtx).
 */
struct mtx		tcp_debug_mtx;
MTX_SYSINIT(tcp_debug_mtx, &tcp_debug_mtx, "tcp_debug_mtx", MTX_DEF);

#endif /* !TREX_FBSD */

/*
 * Save TCP state at a given moment; optionally, both tcpcb and TCP packet
 * header state will be saved.
 */
void
tcp_trace(short act, short ostate, struct tcpcb *tp, void *ipgen,
    struct tcphdr *th, int req)
{
#ifndef TREX_FBSD
#ifdef INET6
	int isipv6;
#endif /* INET6 */
	tcp_seq seq, ack;
	int len, flags;
	struct tcp_debug *td;

	mtx_lock(&tcp_debug_mtx);
	td = &tcp_debug[tcp_debx++];
	if (tcp_debx == TCP_NDEBUG)
		tcp_debx = 0;
	bzero(td, sizeof(*td));
#ifdef INET6
	isipv6 = (ipgen != NULL && ((struct ip *)ipgen)->ip_v == 6) ? 1 : 0;
#endif /* INET6 */
	td->td_family =
#ifdef INET6
	    (isipv6 != 0) ? AF_INET6 :
#endif
	    AF_INET;
#ifdef INET
	td->td_time = iptime();
#endif
	td->td_act = act;
	td->td_ostate = ostate;
	td->td_tcb = (caddr_t)tp;
	if (tp != NULL)
		td->td_cb = *tp;
	if (ipgen != NULL) {
		switch (td->td_family) {
#ifdef INET
		case AF_INET:
			bcopy(ipgen, &td->td_ti.ti_i, sizeof(td->td_ti.ti_i));
			break;
#endif
#ifdef INET6
		case AF_INET6:
			bcopy(ipgen, td->td_ip6buf, sizeof(td->td_ip6buf));
			break;
#endif
		}
	}
	if (th != NULL) {
		switch (td->td_family) {
#ifdef INET
		case AF_INET:
			td->td_ti.ti_t = *th;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			td->td_ti6.th = *th;
			break;
#endif
		}
	}
	td->td_req = req;
	mtx_unlock(&tcp_debug_mtx);
#endif  /* !TREX_FBSD */
#ifdef TCPDEBUG
#ifdef TREX_FBSD
	tcp_seq seq, ack;
	int len, flags;
#ifdef INET6
	int isipv6;
	isipv6 = (ipgen != NULL && ((struct ip *)ipgen)->ip_v == 6) ? 1 : 0;
#endif /* INET6 */
#define TF2_SERVER_ROLE 0x80000000
        if (tp->t_state == TCPS_LISTEN)
                tp->t_flags2 |= TF2_SERVER_ROLE;
        printf("\n(%3.3f) ", tcp_timer_ticks_to_msec(tcp_getticks(tp))/1000.0f);
        if (act == TA_USER)
                printf("--- ");
#endif /* TREX_FBSD */
	if (tcpconsdebug == 0)
		return;
	if (tp != NULL)
#ifndef TREX_FBSD
		printf("%p %s:", tp, tcpstates[ostate]);
#else
	{
		printf("[%s] ", tp->t_flags2 & TF2_SERVER_ROLE ? "S": "C");
		printf("%p %s:", tp, ostate >= TCP_NSTATES ? "-": tcpstates[ostate]);
	}
#endif
	else
		printf("???????? ");
	printf("%s ", tanames[act]);
	switch (act) {
#ifdef TREX_FBSD
	case TA_RESPOND:
                act = TA_OUTPUT;
#endif
	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:
		if (ipgen == NULL || th == NULL)
			break;
		seq = th->th_seq;
		ack = th->th_ack;
		len =
#ifdef INET6
		    isipv6 ? ntohs(((struct ip6_hdr *)ipgen)->ip6_plen) :
#endif
		    ntohs(((struct ip *)ipgen)->ip_len) - sizeof(struct ip);
		if (act == TA_OUTPUT) {
			seq = ntohl(seq);
			ack = ntohl(ack);
		}
#ifndef TREX_FBSD
		if (act == TA_OUTPUT)
			len -= sizeof (struct tcphdr);
#else
			len -= th->th_off * 4;
#endif
#ifndef TREX_FBSD
		if (len)
			printf("[%x..%x)", seq, seq+len);
		else
			printf("%x", seq);
		printf("@%x, urp=%x", ack, th->th_urp);
#else /* TREX_FBSD */
                tcp_seq iseq = 0, iack = 0;
                if (tp != NULL) {
                        switch(act) {
                        case TA_OUTPUT:
                            iseq = tp->iss;
                            iack = tp->irs;
                            break;
                        case TA_INPUT:
                        case TA_DROP:
                            iseq = tp->irs;
                            iack = tp->iss;
                            break;
                        }
                }
#if 0
                if (act == TA_INPUT && th->th_flags & TH_SYN)
                        ack = iack;
#endif
		if (len)
			printf("[%x..%x)", seq-iseq, seq+len-iseq);
		else
			printf("%x", seq-iseq);
		printf("@%x, urp=%x", ack-iack, th->th_urp);
#endif /* TREX_FBSD */
		flags = th->th_flags;
		if (flags) {
			char *cp = "<";
#define pf(f) {					\
	if (th->th_flags & TH_##f) {		\
		printf("%s%s", cp, #f);		\
		cp = ",";			\
	}					\
}
			pf(SYN); pf(ACK); pf(FIN); pf(RST); pf(PUSH); pf(URG);
			printf(">");
		}
		break;

	case TA_USER:
		printf("%s", prurequests[req&0xff]);
		if ((req & 0xff) == PRU_SLOWTIMO)
			printf("<%s>", tcptimers[req>>8]);
		break;
	}
	if (tp != NULL)
		printf(" -> %s", tcpstates[tp->t_state]);
	/* print out internal state of tp !?! */
	printf("\n");
	if (tp == NULL)
		return;
#ifdef TREX_FBSD
    if (tp->t_state < TCPS_SYN_SENT && !tp->t_starttime) {
#endif
	printf(
	"\trcv_(nxt,wnd,up) (%lx,%lx,%lx) snd_(una,nxt,max) (%lx,%lx,%lx)\n",
	    (u_long)tp->rcv_nxt, (u_long)tp->rcv_wnd, (u_long)tp->rcv_up,
	    (u_long)tp->snd_una, (u_long)tp->snd_nxt, (u_long)tp->snd_max);
#ifdef TREX_FBSD
    } else {
	printf("\trcv_(nxt,wnd,up) (%lx,%lx,%lx) snd_(una,nxt,max) (%lx,%lx,%lx)\n",
	    (u_long)(tp->rcv_nxt-tp->irs), (u_long)tp->rcv_wnd, (u_long)(tp->rcv_up-tp->irs),
	    (u_long)(tp->snd_una-tp->iss), (u_long)(tp->snd_nxt-tp->iss), (u_long)(tp->snd_max-tp->iss));
    }
#endif
#ifdef TREX_FBSD
    if (tp->t_state < TCPS_ESTABLISHED && !tp->t_starttime) {
#endif
	printf("\tsnd_(wl1,wl2,wnd) (%lx,%lx,%lx)\n",
	    (u_long)tp->snd_wl1, (u_long)tp->snd_wl2, (u_long)tp->snd_wnd);
#ifdef TREX_FBSD
    } else {
	printf("\tsnd_(wl1,wl2,wnd) (%lx,%lx,%lx)\n",
	    (u_long)(tp->snd_wl1-tp->irs), (u_long)(tp->snd_wl2-tp->iss), (u_long)tp->snd_wnd);
    }
#endif
#endif /* TCPDEBUG */
}
