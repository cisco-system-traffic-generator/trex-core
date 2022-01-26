#ifndef BSD44_BR_TCP_SEQ
#define BSD44_BR_TCP_SEQ

/*
 * Copyright (c) 1982, 1986, 1993, 1995
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
 *  @(#)tcp_seq.h   8.3 (Berkeley) 6/21/95
 */

#ifdef TREX_FBSD
#include "netinet/tcp_seq.h"
#else /* !TREX_FBSD */

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define SEQ_LT(a,b) ((int)((a)-(b)) < 0)
#define SEQ_LEQ(a,b)    ((int)((a)-(b)) <= 0)
#define SEQ_GT(a,b) ((int)((a)-(b)) > 0)
#define SEQ_GEQ(a,b)    ((int)((a)-(b)) >= 0)

/*
 * Macros to initialize tcp sequence numbers for
 * send and receive from initial send and receive
 * sequence numbers.
 */
#define tcp_rcvseqinit(tp) \
    (tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define tcp_sendseqinit(tp) \
    (tp)->snd_una = (tp)->snd_nxt = (tp)->snd_max = (tp)->snd_up = \
        (tp)->iss
#endif /* !TREX_FBSD */

/*
 * Increment for tcp_iss each second.
 * This is designed to increment at the standard 250 KB/s,
 * but with a random component averaging 128 KB.
 * We also increment tcp_iss by a quarter of this amount
 * each time we use the value for a new connection.
 * If defined, the tcp_random18() macro should produce a
 * number in the range [0-0x3ffff] that is hard to predict.
 */
//TBDFIXME
#define tcp_random18()  (0)

#define TCP_ISSINCR (122*1024 + tcp_random18())


#endif


