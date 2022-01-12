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


#include "sys_inet.h"
#ifdef _DEBUG
#define TCPSTATES
#define TCPTIMERS
#define TANAMES
#endif
#include "tcp_var.h"

#include "tcp_debug.h"


#ifdef TCPDEBUG
// <sys/protosw.h>
static const char *prurequests[] = {
	"ATTACH",	"DETACH",	"BIND",		"LISTEN",
	"CONNECT",	"ACCEPT",	"DISCONNECT",	"SHUTDOWN",
	"RCVD",		"SEND",		"ABORT",	"CONTROL",
	"SENSE",	"RCVOOB",	"SENDOOB",	"SOCKADDR",
	"PEERADDR",	"CONNECT2",	"FASTTIMO",	"SLOWTIMO",
	"PROTORCV",	"PROTOSEND",	"SEND_EOF",	"SOSETLABEL",
	"CLOSE",	"FLUSH",
};

static const int tcpconsdebug = 1;
#endif


/*
 * Save TCP state at a given moment; optionally, both tcpcb and TCP packet
 * header state will be saved.
 */
void
tcp_trace(short act, short ostate, struct tcpcb *tp, void *ipgen,
    struct tcphdr *th, int req)
{
#ifdef TCPDEBUG
	tcp_seq seq, ack;
	int len, flags;

	if (tp != NULL) {
#define TF2_SERVER_ROLE 0x80000000
		if (tp->t_state == TCPS_LISTEN)
			tp->t_flags2 |= TF2_SERVER_ROLE;
		printf("\n(%3.3f) ", tcp_timer_ticks_to_msec(tcp_getticks(tp))/1000.0f);
	}
	if (act == TA_USER)
		printf("--- ");
	if (tcpconsdebug == 0)
		return;
	if (tp != NULL)
	{
		printf("[%s] ", tp->t_flags2 & TF2_SERVER_ROLE ? "S": "C");
		printf("%p %s:", tp, ostate >= TCP_NSTATES ? "-": tcpstates[ostate]);
	}
	else
		printf("???????? ");
	printf("%s ", tanames[act]);
	switch (act) {
	case TA_RESPOND:
		act = TA_OUTPUT;
	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:
		if (/*ipgen == NULL || */th == NULL)
			break;
		seq = th->th_seq;
		ack = th->th_ack;
		len = req;
		if (act == TA_OUTPUT) {
			seq = ntohl(seq);
			ack = ntohl(ack);
		}

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

        /* to print out relative seq value */
	tcp_seq irs = 0, iss = 0;
	if (tp->t_state >= TCPS_SYN_SENT || tp->t_starttime) {
		irs = tp->irs;
		iss = tp->iss;
	}
	printf("\trcv_(nxt,wnd,up) (%lx,%lx,%lx) snd_(una,nxt,max) (%lx,%lx,%lx)\n",
	    (u_long)(tp->rcv_nxt-irs), (u_long)tp->rcv_wnd, (u_long)(tp->rcv_up-irs),
	    (u_long)(tp->snd_una-iss), (u_long)(tp->snd_nxt-iss), (u_long)(tp->snd_max-iss));
	printf("\tsnd_(wl1,wl2,wnd) (%lx,%lx,%lx)\n",
	    (u_long)(tp->snd_wl1-irs), (u_long)(tp->snd_wl2-iss), (u_long)tp->snd_wnd);
#endif /* TCPDEBUG */
}
