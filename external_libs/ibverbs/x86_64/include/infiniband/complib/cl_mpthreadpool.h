/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <complib/cl_qmap.h>

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
*	Thread Pool for multi purpose parallel computing
*
* DESCRIPTION
*	The Thread Pool for multi purpose Parallel Computing manages a user specified number
*	of threads allocated from regular thread pool
*
*	Each thread in the thread pool is placed on separate processor to speed
*	up processing. Each time the user provides a function and a collection.
*	All threads in the MP thread pool invoke these
*	callback function on all the members of the collection and performs parallel computations.
*
*	The thread pool functions operate on a cl_pc_thread_pool_t structure which
*	should be treated as opaque, and should be manipulated only through the
*	provided functions.
*
* SEE ALSO
*	Structures:
*		cl_MP_thread_pool_t
*
*	Initialization:
*		cl_mp_thread_pool_init, cl_mp_thread_pool_destroy
*
*	Manipulation
*		cl_mp_thread_pool_array_apply, cl_mp_thread_pool_xarray_apply,
*		cl_mp_thread_pool_qmap_apply
*
*	Attributes:
*		cl_mp_thread_pool_thread_id, cl_mp_thread_pool_state,
*		cl_mp_thread_pool_num_threads
*
*	Callbacks:
*		cl_pfn_mp_thread_pool_apply_t
*
*********/

/****d* Component Library: Data Types/cl_mp_thread_pool_mode_t
* NAME
*	cl_mp_thread_pool_mode_t
*
* DESCRIPTION
* 	The cl_mp_thread_pool_mode_t controls mode of operation of
* 	cl_mp_thread_pool_t object when distributing tasks for parallel
* 	computation to different threads.
*
* SYNOPSIS
*/
#define CL_MP_THREAD_POOL_BATCH_PER_THREAD		(0x00000000L)
#define CL_MP_THREAD_POOL_BATCH_PER_TASK		(0x00000001L)
#define CL_MP_THREAD_POOL_BATCH_SIZE(n)			(0x00000000L | (n))
#define CL_MP_THREAD_POOL_BATCH_NUM(n)			(0x80000000L | (n))

typedef unsigned long cl_mp_thread_pool_mode_t;
/*
* SEE ALSO
*	Thread pool, cl_mp_thread_pool_array_apply, cl_mp_thread_pool_qmap_apply
*********/

/****d* Component Library: MP Thread Pool/cl_mp_thread_pool_t
* NAME
*	cl_pfn_mp_thread_pool_apply_t
*
* DESCRIPTION
*	The cl_pfn_mp_thread_pool_apply_t function type defines the prototype
*	for functions used to run by MP thread pool on tasks in parallel.
*
* SYNOPSIS
*/
typedef int
    (*cl_pfn_mp_thread_pool_apply_t) (IN void **tasks,
				      IN unsigned num_tasks,
				      IN void *context,
				      IN unsigned thread_id);
/*
 * PARAMETERS
 *	tasks
 *		[in] Pointer to array of tasks.
 *
 *	num_tasks
 *		[in] Length of array of tasks.
 *
 *	context
 *		[in] Context.
 *
 *	thread_id
 *		[in] ID of thread pool's thread.
 *
 * RETURN VALUE
 *	Returns 0 on success.
 *
 * NOTES
 *	This function type is provided as function prototype reference for the
 *	function provided by users as a parameter to the
 *	cl_mp_thread_pool_array_apply and cl_mp_thread_pool_qmap_apply functions.
 *
 * SEE ALSO
 *	Thread Pool, cl_mp_thread_pool_array_apply, cl_mp_thread_pool_qmap_apply
 *********/

/****s* Component Library: MP Thread Pool/cl_mp_thread_pool_t
* NAME
*	cl_mp_thread_pool_t
*
* DESCRIPTION
*	Thread pool structure for multi purpose use.
*	After one init can be used multiple times for Parallel computing.
*
*	The cl_mp_thread_pool_t structure should be treated as opaque, and should be
*	manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct cl_mp_thread_pool {
	cl_thread_pool_t thread_pool;
	cl_state_t state;
	cl_pfn_mp_thread_pool_apply_t pfn_callback;
	void *context;
	unsigned num_threads;
	void **tasks;
	unsigned num_tasks;
	unsigned num_pending_tasks;
	unsigned next_task;
	unsigned batch_size;
	unsigned num_batches;
	unsigned result;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} cl_mp_thread_pool_t;
/*
* FIELDS
*	thread_pool
*		Thread pool struct.
*
*	state
*		State of the thread pool, used to verify that operations are permitted.
*
*	pfn_callback
*		Thread callback function for parallel computing
*
*	context
*		Context to pass to the thread callback function.
*
*	num_threads
*		Number of threads in the thread pool.
*
*	tasks
*		An array of for the threads to complete.
*
*	num_tasks
*		Number of tasks in the task_queue.
*
*	next_task
*		Index of next task to be processed by the threads
*
*	batch_size
*		Maximal number of tasks to pass to the callback function on
*		each call.
*
*	num_batches
*		Number of batches in which thread pool should complete processing
*		all the tasks.
*
*	result
*		Result of the thread pool processing,
*
*	mutex
*		Mutex for threadpool use.
*
*	cond
*		Conditional variable to signal an event to thread
*
* SEE ALSO
*	Thread Pool
*********/


/****f* Component Library: Thread Pool/cl_mp_thread_pool_init
* NAME
*	cl_mp_thread_pool_init
*
* DESCRIPTION
*	The cl_mp_thread_pool_init function creates the threads
*	for parallel computing.
*
* SYNOPSIS
*/
cl_status_t cl_mp_thread_pool_init(IN cl_mp_thread_pool_t * const p_mp_thread_pool,
				   IN unsigned num_threads,
				   IN uint16_t max_threads_per_core,
				   IN const char *const name);

/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a MP thread pool structure to initialize.
*
*	num_threads
*		[in] Number of threads to be managed by the thread pool.
*
*	max_threads_per_core
*		[in] indicates maximal number of threads allowed
*		to be placed on separate processor
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
* 	cl_mp_thread_pool_init initializes the thread pool and allows calling
* 	manipulation functions.
*
*	If thread_count is zero, the thread pool creates as many threads as
*	there are processors in the system the process is allowed to use.
*
*	If max_threads_per_core is not zero, the thread pool will limit number
*	of threads in the thread pool according to number of threads.
*	In addition, the thread pool will configure affinity per thread to
*	limit it to single core accordingly.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_destroy,
*	cl_mp_thread_pool_array_apply, cl_mp_thread_pool_qmap_apply
*********/

/****f* Component Library: Thread Pool/cl_mp_thread_pool_destroy
* NAME
*	cl_mp_thread_pool_destroy
*
* DESCRIPTION
*	The cl_mp_thread_pool_destroy function performs any necessary cleanup
*	for a thread pool.
*
* SYNOPSIS
*/
void cl_mp_thread_pool_destroy(IN cl_mp_thread_pool_t * const p_mp_thread_pool);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

/****f* Component Library: Thread Pool/cl_mp_thread_pool_state
* NAME
*	cl_mp_thread_pool_state
*
* DESCRIPTION
*	Returns state of cl_mp_thread_pool_t object.
*
* SYNOPSIS
*/
cl_status_t cl_mp_thread_pool_state(IN cl_mp_thread_pool_t * const p_mp_thread_pool);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure.
*
* RETURN VALUE
*	This function returns state of cl_mp_thread_pool_t object.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

/****f* Component Library: Thread Pool/cl_mp_thread_num_threads
* NAME
*	cl_mp_thread_pool_num_threads
*
* DESCRIPTION
*	Returns number of threads of cl_mp_thread_pool_t object.
*
* SYNOPSIS
*/
unsigned cl_mp_thread_pool_num_threads(IN cl_mp_thread_pool_t * const p_mp_thread_pool);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure.
*
* RETURN VALUE
*	This function returns number of threads of cl_mp_thread_pool_t object.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

/****f* Component Library: Thread Pool/cl_mp_thread_pool_array_apply
* NAME
*	cl_mp_thread_pool_array_apply
*
* DESCRIPTION
* 	Run user provided function on elements of array of pointers in parallel.
*
* SYNOPSIS
*/
unsigned cl_mp_thread_pool_array_apply(IN cl_mp_thread_pool_t * const p_mp_thread_pool,
				      IN void * tasks[],
				      IN unsigned num_tasks,
				      IN cl_mp_thread_pool_mode_t mode,
				      IN cl_pfn_mp_thread_pool_apply_t pfn_callback,
				      IN void *context);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure to destroy.
*
* 	tasks
* 		[in] Array of pointers.
*
* 	num_tasks
* 		[in] Length of array of tasks.
*
* 	mode
* 		[in] Mode of distributing tasks to threads.
*
* 	pfn_callback
* 		[in] Callback function.
*
* 	context
* 		[in] Context for callback function.
*
*
* RETURN VALUE
* 	This function returns number of failed tasks.
* 	Task result is determined by the return value of pfn_callback.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

/****f* Component Library: Thread Pool/cl_mp_thread_pool_xarray_apply
* NAME
*	cl_mp_thread_pool_xarray_apply
*
* DESCRIPTION
* 	Run user provided function on elements of array in parallel.
*
* SYNOPSIS
*/
unsigned cl_mp_thread_pool_xarray_apply(IN cl_mp_thread_pool_t * const p_mp_thread_pool,
					IN void *tasks,
					IN unsigned num_tasks,
					IN unsigned size,
					IN cl_mp_thread_pool_mode_t mode,
					IN cl_pfn_mp_thread_pool_apply_t pfn_callback,
					IN void *context);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure to destroy.
*
* 	tasks
* 		[in] Array of tasks.
*
* 	num_tasks
* 		[in] Length of array of tasks.
*
* 	size
* 		[in] Size in bytes of each task
*
* 	mode
* 		[in] Mode of distributing tasks to threads.
*
* 	pfn_callback
* 		[in] Callback function.
*
* 	context
* 		[in] Context for callback function.
*
*
* RETURN VALUE
* 	This function returns number of failed tasks.
* 	Task result is determined by the return value of pfn_callback.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

unsigned cl_mp_thread_pool_qmap_apply(IN cl_mp_thread_pool_t * const p_mp_thread_pool,
				      IN cl_qmap_t * const p_map,
				      IN cl_mp_thread_pool_mode_t mode,
				      IN cl_pfn_mp_thread_pool_apply_t pfn_callback,
				      IN void *context);
/*
* PARAMETERS
*	p_mp_thread_pool
*		[in] Pointer to a thread pool structure to destroy.
*
*	p_map
*		[in] Point to cl_qmap_t object of tasks.
*
* 	mode
* 		[in] Mode of distributing tasks to threads.
*
* 	pfn_callback
* 		[in] Callback function.
*
* 	context
* 		[in] Context for callback function.
*
*
* RETURN VALUE
* 	This function returns number of failed tasks.
* 	Task result is determined by the return value of pfn_callback.
*
* NOTES
*	This function should only be called after a call to
*	cl_mp_thread_pool_init.
*
* SEE ALSO
*	Thread Pool, cl_mp_thread_pool_init
*********/

unsigned cl_mp_thread_pool_thread_id();

END_C_DECLS
