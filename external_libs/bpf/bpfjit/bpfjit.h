/*-
 * Copyright (c) 2011-2013 Alexander Nasonov.
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

#ifndef _NET_BPFJIT_H_
#define _NET_BPFJIT_H_

#ifndef _KERNEL
#include <stddef.h>
#include <stdint.h>
#endif

#include <sys/types.h>

#ifdef __linux
#include "pcap-bpf.h"
#else
#include <net/bpf.h>
#endif

struct bpf_ctx;
typedef struct bpf_ctx bpf_ctx_t;

#ifndef BPF_COP_EXTMEM_RELEASE
#include "bpf-compat.h"
#endif

typedef unsigned int (*bpfjit_func_t)(const bpf_ctx_t *, bpf_args_t *);

bpfjit_func_t bpfjit_generate_code(const bpf_ctx_t *,
    const struct bpf_insn *, size_t);
void bpfjit_free_code(bpfjit_func_t);

#ifdef _KERNEL
struct bpfjit_ops {
	bpfjit_func_t (*bj_generate_code)(const bpf_ctx_t *,
	    const struct bpf_insn *, size_t);
	void (*bj_free_code)(bpfjit_func_t);
};

extern struct bpfjit_ops bpfjit_module_ops;
#endif

#endif /* !_NET_BPFJIT_H_ */
