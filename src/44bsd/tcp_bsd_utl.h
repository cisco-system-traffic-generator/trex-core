#ifndef BSD44_TCP_UTL
#define BSD44_TCP_UTL

#include <stdint.h>

/*
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *  @(#)tcp_var.h   8.4 (Berkeley) 5/24/95
 */

/*
 * Kernel variables for tcp.
 */

/*
 * Tcp control block, one per tcp; fields:
 */

#include "pal_utl.h"

static inline uint32_t bsd_ntohl(uint32_t a){
    return (PAL_NTOHL(a));
}

static inline uint32_t bsd_htonl(uint32_t a){
    return (PAL_NTOHL(a));
}

static inline uint16_t bsd_ntohs(uint16_t a){
    return (PAL_NTOHS(a));
}

static inline uint16_t bsd_htons(uint16_t a){
    return (PAL_NTOHS(a));
}

inline uint16_t in_cksum(struct rte_mbuf * m, int len ){
    return (0);
}

static inline int32_t bsd_imax(int32_t a, int32_t b){
    return (a > b ? a : b);
}

static inline uint32_t bsd_umax(uint32_t a, uint32_t b){
    return (a > b ? a : b);
}

static inline uint32_t bsd_umin(uint32_t a, uint32_t b){
    return (a < b ? a : b);
}



#endif
