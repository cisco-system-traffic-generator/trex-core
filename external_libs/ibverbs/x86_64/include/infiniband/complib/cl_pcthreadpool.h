/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
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
 *	Declaration of thread pool for parallel computing.
 */

#pragma once

#include <pthread.h>
#include <complib/cl_types.h>
#include <complib/cl_threadpool.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* Component Library/Thread Pool
* NAME
*	Thread Pool for Parallel computing
*
* DESCRIPTION
*	The Thread Pool for Parallel Computing manages a user specified number
*	of threads allocated from regular thread pool
*
*	Each thread in the thread pool is placed on separate processor to speed
*	up processing. All threads in the PC thread pool invoke the same
*	callback function provided by user that performs parallel computations.
*
*	The thread pool functions operate on a cl_pc_thread_pool_t structure which
*	should be treated as opaque, and should be manipulated only through the
*	provided functions.
*
* SEE ALSO
*	Structures:
*		cl_pc_thread_pool_t
*
*	Initialization:
*		cl_pc_thread_pool_construct, cl_pc_thread_pool_destroy
*
*	Manipulation
*		cl_pc_thread_pool_start, cl_pc_thread_pool_wait
*********/

/****s* Component Library: PC Thread Pool/cl_pc_thread_pool_t
* NAME
*	cl_pc_thread_pool_t
*
* DESCRIPTION
*	Thread pool structure for Parallel computing.
*
*	The cl_pc_thread_pool_t structure should be treated as opaque, and should be
*	manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_pc_thread_pool {
	cl_thread_pool_t *p_thread_pool;
	void (*pfn_callback) (void *, uint32_t);
	void *context;
	cl_event_t signal;
	pthread_mutex_t mutex;
	uint16_t *active_cores;
	uint16_t max_threads_per_core;
	uint32_t max_id;
	uint32_t finished_threads;
} cl_pc_thread_pool_t;
/*
* FIELDS
*	p_thread_pool
*		pointer to the thread pool
*
*	pfn_callback
*		thread callback function for parallel computing
*
*	context
*		context to pass to the thread callback function.
*
*	signal
*		signal sent when all threads complete the job
*
*	mutex
*		mutex for max_id and finished_threads variables protection
*
*	cond
*		conditional variable to signal an event to thread
*
*	active_cores
*		array indicating current number of active threads per core
*
*	max_threads_per_core
*		indicates maximal number of threads allowed
*		to be placed on separate processor
*
*	max_id
*		maximal unique id allocated per thread
*
*	finished_threads
*		number of threads already completed the job
*
* SEE ALSO
*	Thread Pool
*********/

/****f* Component Library: Thread Pool/cl_pc_thread_pool_init
* NAME
*	cl_pc_thread_pool_init
*
* DESCRIPTION
*	The cl_pc_thread_pool_init function creates the threads
*	for parallel computing. The threads will start actual
*	computing only as a result of cl_pc_thread_pool_start invocation.
*
* SYNOPSIS
*/
cl_status_t
cl_pc_thread_pool_init(IN cl_pc_thread_pool_t * const p_pc_thread_pool,
		    IN unsigned count,
		    IN void (*pfn_callback) (void *, uint32_t),
		    IN uint16_t max_threads_per_core,
		    IN void *context, IN const char *const name);
/*
* PARAMETERS
*	p_pc_thread_pool
*		[in] Pointer to a PC thread pool structure to initialize.
*
*	count
*		[in] Number of threads to be managed by the thread pool.
*
*	pfn_callback
*		[in] Address of a function to be invoked by a thread.
*
*	max_threads_per_core
*		[in] indicates maximal number of threads allowed
*		to be placed on separate processor
*
*	context
*		[in] Value to pass to the callback function.
*
*	name
*		[in] Name to associate with the threads.  The name may be up to 16
*		characters, including a terminating null character.  All threads
*		created in the pool have the same name.
*
* RETURN VALUES
*	CL_SUCCESS if the thread pool creation succeeded.
*
*	CL_INSUFFICIENT_MEMORY if there was not enough memory to initialize
*	the thread pool.
*
*	CL_ERROR if the threads could not be created.
*
* NOTES
*	cl_pc_thread_pool_init creates and starts the specified number of CL threads.
*	The actual parallel computation will be invoked by cl_pc_thread_pool_start
*	routine.
*	If thread_count is zero, the thread pool creates as many threads as there
*	are processors in the system.
*
* SEE ALSO
*	Thread Pool, cl_pc_thread_pool_destroy,
*	cl_pc_thread_pool_wait, cl_pc_thread_pool_start
*********/

/****f* Component Library: Thread Pool/cl_pc_thread_pool_destroy
* NAME
*	cl_pc_thread_pool_destroy
*
* DESCRIPTION
*	The cl_pc_thread_pool_destroy function performs any necessary cleanup
*	for a PC thread pool.
*
* SYNOPSIS
*/
void cl_pc_thread_pool_destroy(IN cl_pc_thread_pool_t * const p_pc_thread_pool);
/*
* PARAMETERS
*	p_pc_thread_pool
*		[in] Pointer to a thread pool structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	This function blocks until all CL threads exit, and must therefore not
*	be called from any of the thread pool's threads. Because of its blocking
*	nature, callers of cl_pc_thread_pool_destroy must ensure that entering a wait
*	state is valid from the calling thread context.
*
*	This function should only be called after a call to cl_pc_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_pc_thread_pool_init
*********/

/****f* Component Library: Thread Pool/cl_pc_thread_pool_start
* NAME
*	cl_pc_thread_pool_start
*
* DESCRIPTION
*	The cl_pc_thread_pool_start function starts parallel computations
*
* SYNOPSIS
*/
void  cl_pc_thread_pool_start(IN cl_pc_thread_pool_t * const p_pc_thread_pool);
/*
* PARAMETERS
*	p_pc_thread_pool
*		[in] Pointer to a PC thread pool structure to start.
*
* RETURN VALUES
*	CL_SUCCESS if the parallel computations were successfully started.
*
*	CL_ERROR otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* Component Library: Thread Pool/cl_pc_thread_pool_wait
* NAME
*	cl_pc_thread_pool_wait
*
* DESCRIPTION
*	The cl_pc_thread_pool_wait function blocks
*	until parallel computation ends
*
*SYNOPSIS
*/

cl_status_t cl_pc_thread_pool_wait(IN cl_pc_thread_pool_t * p_pc_thread_pool,
				   IN const uint32_t wait_us);
/*
* PARAMETERS
*	p_pc_thread_pool
*		[in] Pointer to a PC thread pool structure to start.
*
*	wait_us
*		[in] Number of microseconds to wait.
*
* RETURN VALUES
*	CL_SUCCESS if the parallel computations were successfully ended
*
*	CL_TIMEOUT if the specified time period elapses.
*
*	CL_ERROR if the wait operation failed.
*
* NOTES
*	If wait_us is set to EVENT_NO_TIMEOUT, the function will wait until the
*	all threads complete the computation.
*
*	Zero timeout value is not allowed
*
* SEE ALSO
********/

END_C_DECLS
