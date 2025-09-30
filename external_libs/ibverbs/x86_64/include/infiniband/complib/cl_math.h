/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004, 2005 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *	Defines standard math related macros and functions.
 */

#pragma once

#include <complib/cl_types.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

#define CL_BITS_PER_BYTE		8
#define CL_BYTES_PER_KB			1024
#define CL_SECS_TO_USECS		1000000

/****d* Component Library: Math/MAX
* NAME
*	MAX
*
* DESCRIPTION
*	The MAX macro returns the greater of two values.
*
* SYNOPSIS
*	MAX( x, y );
*
* PARAMETERS
*	x
*		[in] First of two values to compare.
*
*	y
*		[in] Second of two values to compare.
*
* RETURN VALUE
*	Returns the greater of the x and y parameters.
*
* SEE ALSO
*	MIN, ROUNDUP
*********/
#ifndef MAX
#define MAX(x,y)	((x) > (y) ? (x) : (y))
#endif
/****d* Component Library: Math/MIN
* NAME
*	MIN
*
* DESCRIPTION
*	The MIN macro returns the greater of two values.
*
* SYNOPSIS
*	MIN( x, y );
*
* PARAMETERS
*	x
*		[in] First of two values to compare.
*
*	y
*		[in] Second of two values to compare.
*
* RETURN VALUE
*	Returns the lesser of the x and y parameters.
*
* SEE ALSO
*	MAX, ROUNDUP
*********/
#ifndef MIN
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#endif
/****d* Component Library: Math/ROUNDDOWN
* NAME
*	ROUNDDOWN
*
* DESCRIPTION
*	The ROUNDDOWN macro rounds a value down to a given multiple.
*
* SYNOPSIS
*	ROUNDDOWN( val, align );
*
* PARAMETERS
*	val
*		[in] Value that is to be rounded down. The type of the value is
*		indeterminate, but must be at most the size of a natural integer
*		for the platform.
*
*	align
*		[in] Multiple to which the val parameter must be rounded down.
*
* RETURN VALUE
*	Returns a value that is the input value specified by val rounded down
*	to the nearest multiple of align.
*
* NOTES
*	The value provided must be of a type at most the size of a natural integer.
*********/
#ifndef ROUNDDOWN
#define ROUNDDOWN(val, align)	\
	(((val) / (align)) * (align))
#endif
/****d* Component Library: Math/ROUNDUP
* NAME
*	ROUNDUP
*
* DESCRIPTION
*	The ROUNDUP macro rounds a value up to a given multiple.
*
* SYNOPSIS
*	ROUNDUP( val, align );
*
* PARAMETERS
*	val
*		[in] Value that is to be rounded up. The type of the value is
*		indeterminate, but must be at most the size of a natural integer
*		for the platform.
*
*	align
*		[in] Multiple to which the val parameter must be rounded up.
*
* RETURN VALUE
*	Returns a value that is the input value specified by val rounded up to
*	the nearest multiple of align.
*
* NOTES
*	The value provided must be of a type at most the size of a natural integer.
*********/
#ifndef ROUNDUP
#define ROUNDUP(val, align)	\
	(ROUNDDOWN((val), (align)) + (((val) % (align)) ? (align) : 0))
#endif
/****d* Component Library: Math/ROUNDNEAREST
* NAME
*	ROUNDNEAREST
*
* DESCRIPTION
*	The ROUNDNEAREST macro rounds a value up to a given multiple.
*
* SYNOPSIS
*	ROUNDNEAREST( val, align );
*
* PARAMETERS
*	val
*		[in] Value that is to be rounded to nearest. The type of the
*		value is indeterminate, but must be at most the size of a
*		natural integer for the platform.
*
*	align
*		[in] Multiple to which the val parameter must be rounded.
*
* RETURN VALUE
*	Returns a value that is the input value specified by val rounded to the
*	nearest multiple of align.
*
* NOTES
*	The value provided must be of a type at most the size of a natural integer.
*********/
#ifndef ROUNDNEAREST
#define ROUNDNEAREST(val, align)	\
	((((val) % (align)) >= (ROUNDUP((align), 2) / 2)) ?	\
	 ROUNDUP((val), (align)) : ROUNDDOWN((val), (align)))
#endif
END_C_DECLS
