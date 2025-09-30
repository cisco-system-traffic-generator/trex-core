/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019 Mellanox Technologies LTD. All rights reserved.
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * FIPS pub 180-1: Secure Hash Algorithm (SHA-1)
 * based on: http://csrc.nist.gov/fips/fip180-1.txt
 * implemented by Jun-ichiro itojun Itoh <itojun@itojun.org>
 */

#pragma once

#include <complib/cl_types.h>

/****h* Component Library/SHA1
* NAME
*	SHA1
*
* DESCRIPTION
* 	SHA1 implements SHA-1 algorithm.
*
* SEE ALSO
*	Structures:
*		cl_sha1_ctxt_t
*
*	Initialization:
*		cl_sha1_init
*
*	Item Manipulation:
*		cl_sha1_update
*
*	Attributes:
*		cl_sha1_digest
*********/

/****s* Component Library: SHA1/cl_sha1_ctxt_t
* NAME
*	cl_sha1_ctxt_t
*
* DESCRIPTION
*	Context for SHA1
*
*	The cl_sha1_ctxt_t structure should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct cl_sha1_ctxt {
	union {
		uint8_t		b8[20];
		uint32_t	b32[5];
	} h;
	union {
		uint8_t		b8[8];
		uint64_t	b64[1];
	} c;
	union {
		uint8_t		b8[64];
		uint32_t	b32[16];
	} m;
	uint8_t	count;
} cl_sha1_ctxt_t;
/*
* SEE ALSO
*	SHA1, cl_sha1_init, cl_sha1_update, cl_sha1_digest
*********/

#define	CL_SHA1_RESULTLEN	(160/8)

/****f* Component Library: SHA1/cl_sha1_init
* NAME
*	cl_sha1_init
*
* DESCRIPTION
* 	Initialize SHA1 context object.
* 	This function should be called before invoking cl_sha1_update, cl_sha1_digest.
*
* SYNOPSIS
*/
void cl_sha1_init(struct cl_sha1_ctxt *p_ctxt);
/*
* SEE ALSO
*	SHA1, cl_sha1_update, cl_sha1_digest.
*********/

/****f* Component Library: SHA1/cl_sha1_update
* NAME
*	cl_sha1_update
*
* PARAMETERS
*	[in/out] p_ctxt
*		Pointer to SHA1 context object.
*
* DESCRIPTION
* 	Calculate SHA1 on input buffer, and add result to SHA1 context object.
*
* SYNOPSIS
*/
void cl_sha1_update(struct cl_sha1_ctxt *p_ctxt, const uint8_t *buffer, size_t len);
/*
* PARAMETERS
*	[in/out] p_ctxt
*		Pointer to SHA1 context object.
*
*	[in] buffer
*		Buffer to add to SHA1 calculation.
*
*	[in] len
*		Buffer length.
*
* SEE ALSO
*	SHA1, cl_sha1_init, cl_sha1_digest.
*********/

/****f* Component Library: SHA1/cl_sha1_digest
* NAME
*	cl_sha1_digest
*
* DESCRIPTION
* 	Calculate and return SHA1 digest of the input buffers.
*
* SYNOPSIS
*/
void cl_sha1_digest(struct cl_sha1_ctxt *p_ctxt, uint8_t result[CL_SHA1_RESULTLEN]);
/*
* PARAMETERS
*	[in/out] p_ctxt
*		Pointer to SHA1 context object.
*
*	[out] result
*		Pointer to SHA1 result buffer.
*
* SEE ALSO
*	SHA1, cl_sha1_init, cl_sha1_update.
*********/

