/*
 * Copyright (c) 1982, 1986, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)tcp_debug.c	8.1 (Berkeley) 6/10/93
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
                         

struct	tcp_debug {
	dsec_t 	td_time;
	short	td_act;
	short	td_ostate;
	void *	td_tcb;
	struct	tcpiphdr td_ti;
	short	td_req;
	struct	tcpcb td_cb;
};

              

const char *tcptimers[] =
    { "REXMT", "PERSIST", "KEEP", "2MSL" };

const char	*tanames[] =
    { "input", "output", "user", "respond", "drop" };


#ifdef TCPDEBUG
/* one thread debug */
int	tcpconsdebug = 1;
#define	TCP_NDEBUG 100
struct	tcp_debug tcp_debug[TCP_NDEBUG];
int	tcp_debx;


const char *prurequests[] = {
	"ATTACH",	"DETACH",	"BIND",		"LISTEN",
	"CONNECT",	"ACCEPT",	"DISCONNECT",	"SHUTDOWN",
	"RCVD",		"SEND",		"ABORT",	"CONTROL",
	"SENSE",	"RCVOOB",	"SENDOOB",	"SOCKADDR",
	"PEERADDR",	"CONNECT2",	"FASTTIMO",	"SLOWTIMO",
	"PROTORCV",	"PROTOSEND",	"SEND_EOF",	"SOSETLABEL",
	"CLOSE",	"FLUSH",
};

/*
 * Tcp debug routines
 */
void tcp_trace(CTcpPerThreadCtx * ctx,
          short act, 
          short ostate, 
          struct tcpcb * tp, 
          struct tcpiphdr * ti, 
          TCPHeader * tio,
          int req){
	tcp_seq seq, ack;
	int len, flags;
    struct tcp_debug local_td;
    const char ** tcpstates= tcp_get_tcpstate();

    struct tcp_debug *td = &local_td;
    if (tp->m_flow==0){
        printf(" NO TRACE info \n");
        return;
    }
    CFlowTemplate * ftp=&tp->m_flow->m_template;
    printf("\n");
    uint32_t src_ipv4=ftp->get_src_ipv4();
    uint32_t dst_ipv4=ftp->get_dst_ipv4();

	if (tcp_debx == TCP_NDEBUG)
		tcp_debx = 0;
	td->td_time = now_sec();
	td->td_act = act;
	td->td_ostate = ostate;
	td->td_tcb = (caddr_t)tp;
	if (tp)
		td->td_cb = *tp;
	else
		memset((void *)&td->td_cb, 0,sizeof (*tp));
	if (ti)
		td->td_ti = *ti;
	else
		memset((void *)&td->td_ti,0, sizeof (*ti));
    if (act==TA_USER) {
        td->td_req = req;
    }else{
        td->td_req = 0;
    }
	if (tcpconsdebug == 0)
		return;
	if (tp)
		printf("%p %s:", tp, tcpstates[ostate]);
	else
		printf("???????? ");
	printf("%s ", tanames[act]);

    if (act == TA_OUTPUT) {
        seq = tio->getSeqNumber();
        ack = tio->getAckNumber();
        len = req;
        flags = tio->getFlags();
    }else{
        if (ti) {
            seq = ti->ti_seq;
            ack = ti->ti_ack;
            len = ti->ti_len;
            flags = ti->ti_flags;
        }
    }

	switch (act) {

	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:

        if (tp)
            printf(" (%x) -> (%x) %s :", src_ipv4,dst_ipv4,  tcpstates[tp->t_state]);

        if (act==TA_OUTPUT) {
            if (len)
                printf("[%lx..%lx)", (ulong)seq-tp->iss, (ulong)seq+len-tp->iss);
            else
                printf("%lx", (ulong)seq-tp->iss);
            printf("@%lx, urp=%lx", (ulong)ack-tp->irs, (ulong)0);
        }else{
            if (act==TA_INPUT) {
                if ((flags&TH_SYN)==0) {
                    if (len)
                        printf("[%lx..%lx)", (ulong)seq-tp->irs, (ulong)seq+len-tp->irs);
                    else
                        printf("%lx", (ulong)seq-tp->irs);
                    printf("@%lx, urp=%lx", (ulong)ack-tp->iss, (ulong)0);
                }
            }
        }

        if (flags) {
#ifndef lint
			char *cp = (char *)"<";
#define pf(f) { if (flags&TH_##f) { printf("%s%s", cp, #f); cp = (char*)","; } }
			pf(SYN); pf(ACK); pf(FIN); pf(RST); pf(PUSH); pf(URG);
#endif
			printf(">");
		}
		break;

	case TA_USER:
		printf("%s", prurequests[req&0xff]);
		if ((req & 0xff) == PRU_SLOWTIMO)
			printf("<%s>", tcptimers[req>>8]);
		break;
	}
	/* print out internal state of tp !?! */
	printf("\n");
	if (tp == 0)
		return;

#define sw(f) ((ulong)(tp->f - tp->iss))
#define rw(f) ((ulong)(tp->f - tp->irs))
#define ff(f) ((ulong)(tp->f))

	printf("\trcv_(nxt,wnd,up) (%lx,%lu,%lx) snd_(una,nxt,max) (%lx,%lx,%lx)\n",
	    rw(rcv_nxt), ff(rcv_wnd), rw(rcv_up), sw(snd_una), sw(snd_nxt),sw(snd_max));
	//printf("\tsnd_(wl1,wl2,wnd) (%lu,%lu,%lu)\n",
	   //ff(snd_wl1), ff(snd_wl2), ff(snd_wnd));

    printf("\tsnd_(wnd,cwnd) (%lu,%lu)\n", ff(snd_wnd),ff(snd_cwnd));
}


#else

void tcp_trace(CTcpPerThreadCtx * ctx,
          short act, 
          short ostate, 
          struct tcpcb * tp, 
          struct tcpiphdr * ti, 
          TCPHeader * tio,
          int req){
}



#endif

