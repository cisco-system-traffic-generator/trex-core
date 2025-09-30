/*
 * FILE:	cl_sha2.h
 * AUTHOR:	Aaron D. Gifford - http://www.aarongifford.com/
 * 
 * Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2000-2001, Aaron D. Gifford
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
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: sha2.h,v 1.1 2001/11/08 00:02:01 adg Exp adg $
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>

#define SHA512_BLOCK_LENGTH		128
#define SHA512_DIGEST_LENGTH		64
#define SHA512_DIGEST_STRING_LENGTH	(SHA512_DIGEST_LENGTH * 2 + 1)

/****h* Component Library/SHA512
* NAME
*	SHA512
*
* DESCRIPTION
* 	SHA512 implements SHA-512 algorithm.
*
* SEE ALSO
*	Structures:
*		cl_sha512_ctxt_t
*
*	Initialization:
*		cl_sha512_init
*
*	Item Manipulation:
*		cl_sha512_update
*
*	Attributes:
*		cl_sha512_digest
*		cl_sha512_data
*********/

/****s* Component Library: SHA512/cl_sha512_ctxt_t
* NAME
*	cl_sha512_ctxt_t
*
* DESCRIPTION
*	Context for SHA512
*
*	The cl_sha512_ctxt_t structure should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct cl_sha512_ctxt {
	uint64_t state[8];
	uint64_t bitcount[2];
	uint8_t	buffer[SHA512_BLOCK_LENGTH];
} cl_sha512_ctxt_t;
/*
* SEE ALSO
*	SHA512, cl_sha512_init, cl_sha512_update, cl_sha512_digest, cl_sha512_data
*********/

/****f* Component Library: SHA512/cl_sha512_init
* NAME
*	cl_sha512_init
*
* DESCRIPTION
* 	Initialize SHA512 context object.
* 	This function should be called before invoking cl_sha512_update, cl_sha512_digest.
*
* SYNOPSIS
*/
void cl_sha512_init(struct cl_sha512_ctxt * p_ctxt);
/*
* SEE ALSO
*	SHA512, cl_sha512_update, cl_sha512_digest.
*********/

/****f* Component Library: SHA512/cl_sha512_update
* NAME
*	cl_sha512_update
*
* DESCRIPTION
* 	Calculate SHA512 on input buffer, and add result to SHA512 context object.
*
* SYNOPSIS
*/
void cl_sha512_update(struct cl_sha512_ctxt * p_ctxt, const uint8_t * buffer, size_t len);
/*
* PARAMETERS
*	[in/out] p_ctxt
*		Pointer to SHA512 context object.
*
*	[in] buffer
*		Buffer to add to SHA512 calculation.
*
*	[in] len
*		Buffer length.
*
* SEE ALSO
*	SHA512, cl_sha512_init, cl_sha512_digest.
*********/

/****f* Component Library: SHA512/cl_sha512_digest
* NAME
*	cl_sha512_digest
*
* DESCRIPTION
* 	Calculate and return SHA512 digest of the input buffers.
*
* SYNOPSIS
*/
void cl_sha512_digest(uint8_t result[SHA512_DIGEST_LENGTH], struct cl_sha512_ctxt * p_ctxt);
/*
* PARAMETERS
*	[out] result
*		Pointer to SHA512 result buffer.
*
*	[in/out] p_ctxt
*		Pointer to SHA512 context object.
*
* SEE ALSO
*	SHA512, cl_sha512_init, cl_sha512_update.
*********/

/****f* Component Library: SHA512/cl_sha512_data
* NAME
*	cl_sha512_data
*
* DESCRIPTION
* 	Calculate and return SHA512 digest of an input buffer.
*
* SYNOPSIS
*/
uint8_t * cl_sha512_data(const uint8_t * data, size_t len, uint8_t digest[SHA512_DIGEST_LENGTH]);
/*
* PARAMETERS
*	[in] data
*		Pointer to the data to hash.
*
*	[in] len
*		Length of data buffer.
*
*	[out] digest
*		Buffer to hold the calculated hash.
*********/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

