/*
 * Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *	Declaration of last write register.
 */

#pragma once

#include <complib/cl_types.h>
#include <complib/cl_spinlock.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****h* Component Library/LWR
* NAME
*	LWR - Last Write Register
*
* DESCRIPTION
*	Last Write Register is a thread-safe queue for one element.
*
*	The lwr functions operate on a cl_lwr_t structure which should be
*	treated as opaque and should be manipulated only through the provided
*	functions.
*
* SEE ALSO
*	Structures:
*		cl_lwr_t
*
*	Callbacks:
*		cl_pfn_lwr_dispose_t
*		
*	Initialization/Destruction:
*		cl_lwr_construct, cl_lwr_init, cl_lwr_destroy
*
*	Manipulation:
*		cl_lwr_pop, cl_lwr_put
*
*	Access:
*		cl_lwr_pop, cl_lwr_get
*********/

/****d* Component Library: LWR/cl_pfn_lwr_dispose_t
* NAME
*	cl_pfn_lwr_dispose_t
*
* DESCRIPTION
*	The cl_pfn_lwr_dispose_t function type defines the prototype for functions
*	used to dispose objects in a Last Write Register.
*
* SYNOPSIS
*/
typedef void
(*cl_pfn_lwr_dispose_t) (IN void *const p_object, IN void *context);
/*
* PARAMETERS
*	p_object
*		[in] Pointer to an object to dispose.
*
*	context
*		[in] Context provided in a call to cl_pfn_lwr_dispose_t.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	This function is not assumed to be thread safe.
*	User must supply serialization mechanism if it's required.
*
*	This function type is provided as function prototype reference for the
*	function provided by users as a parameter to the cl_lwr_apply_func
*	function.
*
* SEE ALSO
*	LWR
*********/

/****s* Component Library: LWR/cl_lwr_t
* NAME
*	cl_lwr_t
*
* DESCRIPTION
*	Last Write Register structure.
*
*	The cl_lwr_t structure should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_lwr {
	cl_spinlock_t lock;
	cl_pfn_lwr_dispose_t dispose;
	void * dispose_ctx;
	boolean_t has_value;
	void * last_value;
	cl_state_t state;
} cl_lwr_t;
/*
* FIELDS
*	lock
*		A spinlock to serialize access to this register.
*
*	dispose
*		A function to free last stored item (whenever a new one is written).
*
*	dispose_ctx
*		Context to pass to dispose function.
*
*	has_value
*		An indicator whether the register contains a value.
*	
*	last_value
*		The last value written in the register.
*
*	state
*		The state of the register.
*
* SEE ALSO
*	cl_pfn_lwr_dispose_t
*********/

/****f* Component Library: LWR/cl_lwr_construct
* NAME
*	cl_lwr_construct
*
* DESCRIPTION
*	The cl_lwr_construct function constructs a Last Write Register.
*
* SYNOPSIS
*/
void cl_lwr_construct(IN cl_lwr_t * const p_lwr);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t object whose state to initialize.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling cl_lwr_init and cl_lwr_destroy.
*
*	Calling cl_lwr_construct is a prerequisite to calling any other
*	LWR function except cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_init, cl_lwr_destroy
*********/

/****f* Component Library: LWR/cl_lwr_init
* NAME
*	cl_lwr_init
*
* DESCRIPTION
*	The cl_lwr_init function initializes a Last Write Register for use.
*
* SYNOPSIS
*/
cl_status_t cl_lwr_init(IN cl_lwr_t * const p_lwr,
			IN cl_pfn_lwr_dispose_t dispose,
			IN void * dispose_context);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure to initialize.
*
*	dispose
*		[in] A callback for disposing elements.
*
*	dispose_context
*		[in] Context to pass to the dispose callback.
*
* RETURN VALUES
*	CL_SUCCESS if initialization succeeded.
*
*	CL_ERROR if initialization failed. Callers should call
*	cl_lwr_destroy to clean up any resources allocated during
*	initialization.
*
* SEE ALSO
*	LWR, cl_lwr_construct, cl_lwr_destroy,
*	cl_lwr_put, cl_lwr_get, cl_lwr_pop
*********/

/****f* Component Library: LWR/cl_lwr_get
* NAME
*	cl_lwr_get
*
* DESCRIPTION
*	The cl_lwr_get function retrieves a value from the Last Write Register, 
*	without clearing it.
*
* SYNOPSIS
*/
cl_status_t cl_lwr_get(IN cl_lwr_t * const p_lwr, OUT void ** p_value);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure to read from.
*
*	value
*		[out] A destination to write the retrieved value to.
*
* RETURN VALUE
*	cl_status_t:
*		CL_SUCCESS if operation was successful;
*		CL_NOT_FOUND if there was no element in the Last Write Register;
*		CL_INVALID_STATE if this LWR had been destroyed before invoking cl_lwr_get.
*
* NOTES
*	cl_lwr_get does not delete the currently written element in the register, if such exists.
*	Two subsequent calls to cl_lwr_get (when no other cl_lwr method is called in between)
*	will return the same value.
*	This function should only be called after a call to cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_get, cl_lwr_pop
*********/

/****f* Component Library: LWR/cl_lwr_pop
* NAME
*	cl_lwr_pop
*
* DESCRIPTION
*	The cl_lwr_pop function pops a value from the Last Write Register, clearing it.
*
* SYNOPSIS
*/
cl_status_t cl_lwr_pop(IN cl_lwr_t * const p_lwr, OUT void ** p_value);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure to pop from.
*
*	value
*		[out] A destination to write the popped value to.
*
* RETURN VALUE
*	cl_status_t:
*		CL_SUCCESS if operation was successful;
*		CL_NOT_FOUND if there was no element in the Last Write Register;
*		CL_INVALID_STATE if this LWR had been destroyed before invoking cl_lwr_pop.
*
* NOTES
*	cl_lwr_pop deletes the currently written element in the register, if such exists.
*	If deletion is not required, call cl_lwr_get.
*	After consuming `p_value`, the user must call cl_lwr_dispose_item to dispose it.
*	This function should only be called after a call to cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_get, cl_lwr_put, cl_lwr_dispose_item
*********/

/****f* Component Library: LWR/cl_lwr_put
* NAME
*	cl_lwr_put
*
* DESCRIPTION
*	The cl_lwr_put function writes a value to the Last Write Register.
*
* SYNOPSIS
*/
void cl_lwr_put(IN cl_lwr_t * const p_lwr, IN void * value);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure to put to.
*
*	value
*		[in] The value to write.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_lwr_put disposes the previously written element in the register, if such exists.
*	This function should only be called after a call to cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_get, cl_lwr_pop
*********/

/****f* Component Library: LWR/cl_lwr_dispose_item
* NAME
*	cl_lwr_dispose_item
*
* DESCRIPTION
*	The cl_lwr_dispose_item function disposes an item that was popped from
*	the Last Write Register. It must be called after a successful
*	cl_lwr_pop to avoid resource leaks.
*
* SYNOPSIS
*/
void cl_lwr_dispose_item(IN cl_lwr_t * const p_lwr, IN void * p_item);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure whose disposal function should be invoked.
*
*	p_item
*		[in] The item to dispose.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_lwr_dispose_item invokes the Last Write Register's dispose method with this element.
*	This function should only be called after a call to cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_pop
*********/

/****f* Component Library: LWR/cl_lwr_destroy
* NAME
*	cl_lwr_destroy
*
* DESCRIPTION
*	The cl_lwr_destroy function destroys a Last Write Register.
*
* SYNOPSIS
*/
void cl_lwr_destroy(IN cl_lwr_t * const p_lwr);
/*
* PARAMETERS
*	p_lwr
*		[in] Pointer to cl_lwr_t structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_lwr_destroy disposes the last written element in the register, if such exists.
*	Further operations should not be attempted on the LWR after cl_lwr_destroy is invoked.
*
*	This function should only be called after a call to cl_lwr_construct
*	or cl_lwr_init.
*
* SEE ALSO
*	LWR, cl_lwr_construct, cl_lwr_init
*********/

END_C_DECLS
