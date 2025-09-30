/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2009-2015 ZIH, TU Dresden, Federal Republic of Germany. All rights reserved.
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
 *	- Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
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
 *	Declaration of a d-ary heap. The caller must provide
 *	key/context and must provide a callback function to update
 *	the index of heap elements. Additionally, the caller can
 *	provide a compare function in case that the minimum d-ary heap
 *	based on uint64_t keys is not sufficient enough. The
 *	heap allocates internal structures and can be resized,
 *	which will not relocate or change existing elements.
 */

#pragma once

#include <complib/cl_types.h>

#ifdef __cplusplus
#define BEGIN_C_DECLS extern "C" {
#define END_C_DECLS   }
#else				/* !__cplusplus */
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* Component Library/d-ary (Min-)Heap
* NAME
*	Heap
*
* DESCRIPTION
*	The d-ary heap is stored implicitly in an array and parent/child nodes
*	are accessed via index calculations rather than using allocated
*	elements and pointer structures. It has been proven that implicit 4-ary
*	heaps are in many use cases the most efficient implementation (e.g.,
*	see "A Back-to-Basics Empirical Study of Priority Queues" by
*	Larkin et al.).
*
*	The heap has to be initialized to a certain size by the caller,
*	however a function to resize the heap is available. The resize function
*	will not delete or relocate existing elements in the heap.
*
*	Typical heap operations, such as insert, extract_[min | max],
*	decrease_key, and delete, are provided and referencing an element,
*	e.g., for deletion is done via indices. The implication is, that
*	the caller needs to be informed about index changes, e.g., after
*	internal reordering through heap_[up | down], to always know the
*	index of each element in the heap. Therefore, the caller has to
*	provide a callback function, which forwards the context of an element
*	and the new index in the heap back to the caller. The caller is
*	responsible for updating its internal sturctures/indices accordingly.
*
*	An implementation of heapify is omitted, because the caller should not
*	be able to allocate and provide an unsorted array. All heapify
*	operations with heap_up and heap_down are done internally after the
*	caller manipulated the heap with the provided functions.
*
*	Heaps are used extensively in some routing functions, such as [DF]SSSP
*	routing. Therefore, the [DF]SSSP implementation can be used as a
*	prototype to adapt and use the d-ary heap in other parts of OpenSM.
*
*	The cl_heap_t structure should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* SEE ALSO
*	Structures:
*		cl_heap_t
*
*	Callbacks:
*		cl_pfn_heap_apply_index_update_t, cl_pfn_heap_compare_keys_t
*
*	Initialization:
*		cl_heap_construct, cl_heap_init, cl_heap_destroy
*
*	Manipulation:
*		cl_heap_insert, cl_heap_delete, cl_heap_extract_root,
*		cl_heap_modify_key, cl_heap_resize
*
*	Attributes:
*		cl_heap_get_capacity, cl_heap_get_size, cl_heap_is_empty
*		cl_is_stored_in_heap, cl_is_heap_inited
*********/
/****d* Component Library: Heap/cl_pfn_heap_apply_index_update_t
* NAME
*	cl_pfn_heap_apply_index_update_t
*
* DESCRIPTION
*	The cl_pfn_heap_apply_index_update_t function type defines the prototype
*	to update the heap index, position of the element, in the user supplied
*	context. The index is the only information the user needs to store
*	somewhere in his/her structures and is essential for all munipulations
*	operations on the heap (except resize), since these will change the
*	position of elements through heap_up/heap_down.
*
* SYNOPSIS
*/
typedef void
 (*cl_pfn_heap_apply_index_update_t) (IN const void *context,
					IN const size_t new_index);
/*
* PARAMETERS
*	context
*		[in] Pointer to the user supplied context which is associated
*		with a heap element.
*
*	new_index
*		[in] The new index in the heap, i.e., position in the
*		element_array of cl_heap_t.
*
* RETURN VALUES
*	This callback function should not return any value.
*
* NOTES
*	The function is necessary to update the indices for the caller,
*	since the caller MUST keep track of the position of heap elements
*	to make changes, such as decrease_key or delete. For a working
*	reference implementation on how to define and handle this callback,
*	please refer to the [DF]SSSP routing in opensm/osm_ucast_dfsssp.c.
*
* SEE ALSO
*	Heap, cl_heap_modify_key, cl_heap_insert, cl_heap_delete,
*	cl_heap_extract_root
*********/

/****d* Component Library: Heap/cl_pfn_heap_compare_keys_t
* NAME
*	cl_pfn_heap_heap_compare_keys_t
*
* DESCRIPTION
*	The cl_pfn_heap_heap_compare_keys_t function type defines the prototype
*	to compare the keys of two heap elements.
*
* SYNOPSIS
*/
typedef int
 (*cl_pfn_heap_compare_keys_t) (IN const void *p_key_1, IN const void *p_key_2);
/*
* PARAMETERS
*	p_key_1
*		[in] Pointer to the first key.
*
*	p_key_2
*		[in] Pointer to the second key.
*
* RETURN VALUES
*	The function should return an integer less than, equal to, or greater
*	than zero indicating that key1 is "smaller", "equal" or "greater" than
*	key2.
*
* NOTES
*	If user does not provide a compare function, then the default behavior
*	is to assume all keys are uint64_t and the heap is a minimum d-ary
*	heap, i.e., the smallest key is stored at the root node.
*
* SEE ALSO
*	Heap
*********/

/****s* Component Library: Heap/cl_heap_t
* NAME
*	cl_heap_t
*
* DESCRIPTION
*	Heap structure.
*
*	The cl_heap_t structure should be treated as opaque and should be
*	manipulated only through the provided functions..
*
* SYNOPSIS
*
*/
struct _cl_heap_elem;
typedef struct _cl_heap {
	cl_state_t state;
	uint8_t branching_factor;
	struct _cl_heap_elem *element_array;
	size_t size;
	size_t capacity;
	cl_pfn_heap_apply_index_update_t pfn_index_update;
	cl_pfn_heap_compare_keys_t pfn_compare;
} cl_heap_t;
/*
* FIELDS
*	state
*		State of the heap.
*
*	branching_factor
*		Branching factor d for the d-ary heap, i.e., number of children
*		per node.
*
*	element_array
*		Array of elements for the heap. Each element consists of a key
*		and user supplied context pointer, i.e., usually very compact
*		storage with 16 bytes per element.
*
*	size
*		Number of elements successfully inserted into the heap.
*
*	capacity
*		Total number of elements allocated.
*
*	pfn_index_update
*		User supplied function to update position indices for
*		elements in the heap.
*
*	pfn_compare
*		User supplied comparison of element keys.
*
* SEE ALSO
*	Heap, cl_pfn_heap_apply_index_update_t, cl_pfn_heap_compare_keys_t
*********/

/****f* Component Library: Heap/cl_heap_construct
* NAME
*	cl_heap_construct
*
* DESCRIPTION
*	The cl_heap_construct function constructs a d-ary heap.
*	The result is a valid, but uninitialized, heap. The function
*	cl_heap_construct does not allocate any memory.
*
* SYNOPSIS
*/
void cl_heap_construct(IN cl_heap_t * const p_heap);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling cl_heap_destroy without first calling cl_heap_init.
*	Calling cl_heap_construct is a prerequisite to calling any other
*	heap function except cl_heap_init. The caller must allocate the
*	memory for the heap, i.e., p_heap==NULL results in an exception.
*
* SEE ALSO
*	Heap, cl_heap_init, cl_heap_destroy
*********/

/****f* Component Library: Heap/cl_heap_init
* NAME
*	cl_heap_init
*
* DESCRIPTION
*	The cl_heap_init function initializes a d-ary heap for use.
*
* SYNOPSIS
*/
cl_status_t
cl_heap_init(IN cl_heap_t * const p_heap,
	    IN const size_t max_size,
	    IN const uint8_t branching_factor,
	    IN cl_pfn_heap_apply_index_update_t pfn_index_update,
	    IN cl_pfn_heap_compare_keys_t pfn_compare OPTIONAL);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to initialize.
*
*	max_size
*		[in] Total number of elements the heap should be able to store.
*
*	branching_factor
*		[in] Branching factor d for the d-ary heap, i.e., number of
*		children. For example, using d=2 yields in a binary heap...
*
*	pfn_index_update
*		[in] User supplied callback to inform the user about index
*		changes of individual elements stored in the heap.
*
*	pfn_compare
*		[in] User supplied callback to compare two keys. This function
*		pointer is optional, i.e., if pfn_compare=NULL, then an internal
*		compare function is used to create a min-heap.
*
* RETURN VALUES
*	CL_SUCCESS if the heap was initialized successfully.
*
*       CL_INVALID_PARAMETER if max_size or branching_factor are less than or
*       equal to zero, or if pfn_index_update is NULL.
*
*	CL_INSUFFICIENT_MEMORY if the initialization failed.
*
* NOTES
*	Can be called without calling cl_heap_construct first. Calling
*	cl_heap_init on an already initialized heap will result in an internal
*	call to cl_heap_destroy prior to the memory allocation of the
*	element_array.
*
* SEE ALSO
*	Heap, cl_pfn_heap_apply_index_update_t, cl_pfn_heap_compare_keys_t,
*	cl_heap_construct
*********/

/****f* Component Library: Heap/cl_heap_destroy
* NAME
*	cl_heap_destroy
*
* DESCRIPTION
*	The cl_heap_destroy function destroys the heap. The heap is afterwards
*	in the CL_UNINITIALIZED state.
*
* SYNOPSIS
*/
void cl_heap_destroy(IN cl_heap_t * const p_heap);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_heap_destroy frees all memory allocated for the heap. The heap
*	is left valid, but uninitialized, with a zero capacity and size, and
*	must be re-initialized by calling cl_heap_init for further usage.
*
*	This function should only be called after a call to cl_heap_construct
*	or cl_heap_init.
*
* SEE ALSO
*	Heap, cl_heap_construct, cl_heap_init
*********/

/****f* Component Library: Heap/cl_heap_modify_key
* NAME
*	cl_heap_modify_key
*
* DESCRIPTION
*	The cl_heap_modify_key function changes the key of an element in the
*	heap identified by an index. The result could be invalidate the heap
*	property for the stored elements. Therefore, the cl_heap_modify_key
*	function calls heap_[up | down] internally to reconstruct a valid
*	heap.
*
* SYNOPSIS
*/
cl_status_t cl_heap_modify_key(IN cl_heap_t * const p_heap,
				IN const uint64_t key, IN const size_t index);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to modify.
*
*	key
*		[in] The new key for an existing heap element.
*
*	index
*		[in] Index of the heap element in element_array, which should
*		be modified.
*
* RETURN VALUE
*	CL_SUCCESS if the key of a heap element was modified successfully.
*
*	CL_INVALID_PARAMETER if the user supplied index is out of bounds.
*
* NOTES
*	This function is similar to the common decrease_key for minimum heaps,
*	however the naming scheme is more generic to support maximum heaps
*	as well.
*
*	This function should only be called after a call to cl_heap_init.
*
* SEE ALSO
*	Heap, cl_heap_init
*********/

/****f* Component Library: Heap/cl_heap_insert
* NAME
*	cl_heap_insert
*
* DESCRIPTION
*	The cl_heap_insert function adds a new element to the existing heap
*	and restores the heap property afterwards.
*
* SYNOPSIS
*/
cl_status_t cl_heap_insert(IN cl_heap_t * const p_heap, IN const uint64_t key,
			  IN const void *const context);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to modify.
*
*	key
*		[in] Initial key value to compare heap elements and reorder
*		the element_array according to the heap property.
*
*	context
*		[in] User supplied pointer to a context which "represents"
*		the heap element for the user.
*
* RETURN VALUE
*	CL_SUCCESS if the new heap element was stored successfully.
*
*	CL_INVALID_PARAMETER if the user has no association to the heap
*	element, i.e., the supplied context is NULL.
*
*	CL_INSUFFICIENT_RESOURCES if the heap is already full and no further
*	elements can be stored.
*
* NOTES
*	The user supplied context will be returned to the user in case the
*	user decides to remove either the root of the heap or any element
*	in the heap with cl_heap_extract_root or cl_heap_delete.
*	Furthermore, the context will be forwarded to the user through
*	cl_pfn_heap_apply_index_update_t to indicate for which element an index
*	change in the heap has happened.
*
* SEE ALSO
*	Heap, cl_pfn_heap_apply_index_update_t, cl_heap_get_capacity,
*	cl_heap_delete, cl_heap_extract_root
*********/

/****f* Component Library: Heap/cl_heap_delete
* NAME
*	cl_heap_delete
*
* DESCRIPTION
*	The cl_heap_delete function removes an arbitrary element, referenced
*	by index, from the heap. Afterwards, the heap property is restored.
*
* SYNOPSIS
*/
void *cl_heap_delete(IN cl_heap_t * const p_heap, IN const size_t index);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to modify.
*
*	index
*		[in] Index to an element in the element_array of the heap which
*		should be deleted.
*
* RETURN VALUE
*	The context pointer associated with the index (and key) in the heap,
*	(i.e., the context initially supplied by the user through
*	cl_heap_insert). If the index is invalid or the heap is empty, then
*	NULL is returned.
*
* NOTES
*	The function will move the element to a "out-of-bounds" position, i.e.,
*	relative to the size but not capacity of the heap, and update
*	the index for the caller via cl_pfn_heap_apply_index_update_t. This
*	ensures that later attempts to modify/delete this element can be
*	intercepted.
*
* SEE ALSO
*	Heap, cl_pfn_heap_apply_index_update_t, cl_heap_insert
*********/

/****f* Component Library: Heap/cl_heap_extract_root
* NAME
*	cl_heap_extract_root
*
* DESCRIPTION
*	The cl_heap_extract_root deletes the root of the heap and returns
*	the stored context to the user. The naming scheme is generalized
*	here to support minimum or maximum heaps with one function, e.g.,
*	the root refers to the element with the smallest key in a minimum
*	heap.
*
* SYNOPSIS
*/
void *cl_heap_extract_root(IN cl_heap_t * const p_heap);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to modify.
*
* RETURN VALUE
*	The context pointer associated with the smallest or largest key
*	in a minimum or maximum heap, respectively.
*
* NOTES
*	Internally, the cl_heap_extract_root function calls cl_heap_delete for
*	the element with index zero. Therefore, refer to cl_heap_delete for
*	further explanation.
*
* SEE ALSO
*	Heap, cl_heap_delete
*********/

/****f* Component Library: Heap/cl_heap_resize
* NAME
*	cl_heap_resize
*
* DESCRIPTION
*	The cl_heap_resize function changes the capacity of an existing heap.
*	The function will not delete or relocate existing elements in the heap,
*	consequently changing the capacity to a value smaller than the current
*	number of elements in the heap will result in an error.
*
* SYNOPSIS
*/
cl_status_t cl_heap_resize(IN cl_heap_t * const p_heap,
			  IN const size_t new_size);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure to resize.
*
*	new_size
*		[in] Total number of elements allocated for the heap.
*
* RETURN VALUE
*	CL_SUCCESS if the heap has been changed in size.
*
*	CL_INVALID_PARAMETER if either new_size is less than or equal to zero,
*	or if the requested size is smaller than the number of currently
*	stored heap elements (to prevent loss of data).
*
*	CL_INSUFFICIENT_MEMORY if resizing element_array failed due to an
*	insufficient amount of memory. The data stored in the heap is not lost
*	and can still be retrieved or modified.
*
* NOTES
*	Resizing the heap to a zero capacity is not supported. Please, use the
*	cl_heap_destroy function to free the memory allocated in cl_heap_t.
*
* SEE ALSO
*	Heap, cl_heap_get_capacity, cl_heap_destroy
*********/

/****f* Component Library: Heap/cl_heap_get_capacity
* NAME
*	cl_heap_get_capacity
*
* DESCRIPTION
*	The cl_heap_get_capacity function returns the capacity of a heap.
*
* SYNOPSIS
*/
static inline size_t cl_heap_get_capacity(IN const cl_heap_t * const p_heap)
{
	CL_ASSERT(p_heap);
	CL_ASSERT(p_heap->state == CL_INITIALIZED);

	return (p_heap->capacity);
}

/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure whose capacity to return.
*
* RETURN VALUE
*	Total number of elements which can be stored in the heap.
*
* NOTES
*	The capacity is the number of elements that the heap can store, and
*	can be greater than the number of elements stored. To get the number of
*	elements stored in the heap, use cl_heap_get_size.
*
* SEE ALSO
*	Heap, cl_heap_get_size
*********/

/****f* Component Library: Heap/cl_heap_get_size
* NAME
*	cl_heap_get_size
*
* DESCRIPTION
*	The cl_heap_get_size function returns the number of elements stored
*	in the heap.
*
* SYNOPSIS
*/
static inline size_t cl_heap_get_size(IN const cl_heap_t * const p_heap)
{
	CL_ASSERT(p_heap);
	CL_ASSERT(p_heap->state == CL_INITIALIZED);

	return (p_heap->size);
}

/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure whose size to return.
*
* RETURN VALUE
*	Number of elements in the heap.
*
* SEE ALSO
*	Heap, cl_heap_resize, cl_heap_get_capacity
*********/

/****f* Component Library: Heap/cl_heap_is_empty
* NAME
*	cl_heap_is_empty
*
* DESCRIPTION
*	The cl_heap_is_empty function checks whether elements are stored in the
*	heap, or not.
*
* SYNOPSIS
*/
static inline boolean_t cl_heap_is_empty(IN const cl_heap_t * const p_heap)
{
	CL_ASSERT(p_heap);
	CL_ASSERT(p_heap->state == CL_INITIALIZED);

	return (p_heap->size) ? FALSE : TRUE;
}

/*
* PARAMETERS
*	p_heap
*		[in] Pointer to an initialized cl_heap_t structure.
*
* RETURN VALUES
*	TRUE if no elements are stored in the heap, otherwise FALSE.
*
* SEE ALSO
*	Heap, cl_heap_get_size
*********/

/****f* Component Library: Heap/cl_is_stored_in_heap
* NAME
*	cl_is_stored_in_heap
*
* DESCRIPTION
*	The function cl_is_stored_in_heap can be used by the caller to verify
*	that a context, initially supplied via cl_heap_insert, is stored
*	in the heap at a index known by the caller.
*
* SYNOPSIS
*/
boolean_t cl_is_stored_in_heap(IN const cl_heap_t * const p_heap,
			       IN const void *const ctx,
			       IN const size_t index);
/*
* PARAMETERS
*	p_heap
*		[in] Pointer to an initialized cl_heap_t structure.
*
*       context
*		[in] Pointer to a context which "represents" the heap element
*		initially stored with the cl_heap_insert function.
*
*       index
*		[in] Index to the element in the element_array.
*
* RETURN VALUES
*	TRUE if the index is not out-of-bounds and the stored and requested
*	context matches, otherwise FALSE.
*
* SEE ALSO
*	Heap, cl_heap_insert
*********/

/****f* Component Library: Heap/cl_is_heap_inited
* NAME
*	cl_is_heap_inited
*
* DESCRIPTION
*	The function cl_is_heap_inited can be used to verify that the state
*	of a heap is CL_INITIALIZED.
*
* SYNOPSIS
*/
static inline uint32_t cl_is_heap_inited(IN const cl_heap_t * const p_heap)
{
	CL_ASSERT(p_heap);
	CL_ASSERT(cl_is_state_valid(p_heap->state));

	return (p_heap->state == CL_INITIALIZED);
}

/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure.
*
* RETURN VALUES
*	TRUE if the heap is initialized, otherwise FALSE.
*
* NOTES
*	Can be called for every allocaetd heap for which at least
*	cl_heap_construct or cl_heap_init was called beforehand.
*
* SEE ALSO
*	Heap, cl_heap_construct, cl_heap_init
*********/

/****f* Component Library: Heap/cl_verify_heap_property
* NAME
*       cl_verify_heap_property
*
* DESCRIPTION
*       The function cl_verify_heap_property validates the correctness of a
*       given heap.
*
* SYNOPSIS
*/
boolean_t cl_verify_heap_property(IN const cl_heap_t * const p_heap);

/*
* PARAMETERS
*	p_heap
*		[in] Pointer to a cl_heap_t structure.
*
* RETURN VALUES
*	TRUE if the heap complies with the heap property, otherwise FALSE.
*
* SEE ALSO
*	Heap, cl_heap_construct, cl_heap_init
*********/

END_C_DECLS
