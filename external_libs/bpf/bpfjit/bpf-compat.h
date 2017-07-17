/*-
 * Copyright (c) Alexander Nasonov, 2015.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NET_BPF_COMPAT_H_
#define _NET_BPF_COMPAT_H_

#include <stddef.h>
#include <stdint.h>

/* Compat definitions. */
#ifndef BPF_COP
#define BPF_COP  0x20
#define BPF_COPX 0x40
#endif

#ifndef BPF_MOD
#define BPF_MOD 0x90
#endif

#ifndef BPF_XOR
#define BPF_XOR 0xa0
#endif

#define	BPF_MAX_MEMWORDS	30

/*
 * bpf_memword_init_t: bits indicate which words in the external memory
 * store will be initialised by the caller before BPF program execution.
 */
typedef uint32_t bpf_memword_init_t;
#define	BPF_MEMWORD_INIT(k)	(UINT32_C(1) << (k))

typedef struct bpf_args {
	const uint8_t *	pkt;
	size_t		wirelen;
	size_t		buflen;
	/*
	 * The following arguments are used only by some kernel
	 * subsystems.
	 * They aren't required for classical bpf filter programs.
	 * For such programs, bpfjit generated code doesn't read
	 * those arguments at all. Note however that bpf interpreter
	 * always needs a pointer to memstore.
	 */
	uint32_t *	mem; /* pointer to external memory store */
	void *		arg; /* auxiliary argument for a copfunc */
} bpf_args_t;

#if defined(_KERNEL) || defined(__BPF_PRIVATE)
typedef uint32_t (*bpf_copfunc_t)(const bpf_ctx_t *, bpf_args_t *, uint32_t);

struct bpf_ctx {
	/*
	 * BPF coprocessor functions and the number of them.
	 */
	const bpf_copfunc_t *	copfuncs;
	size_t			nfuncs;

	/*
	 * The number of memory words in the external memory store.
	 * There may be up to BPF_MAX_MEMWORDS words; if zero is set,
	 * then the internal memory store is used which has a fixed
	 * number of words (BPF_MEMWORDS).
	 */
	size_t			extwords;

	/*
	 * The bitmask indicating which words in the external memstore
	 * will be initialised by the caller.
	 */
	bpf_memword_init_t	preinited;
};
#endif

#endif
