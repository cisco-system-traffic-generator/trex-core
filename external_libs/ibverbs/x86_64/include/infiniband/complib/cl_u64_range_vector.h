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
 *	This file contains U64 Range Vector definitions.
 *	U64 Range Vector provides dynamically resizable array functionality.
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
/****h* Component Library/U64 Range Vector
* NAME
*	U64 Range Vector
*
* DESCRIPTION
*	The U64_t Vector is a self-sizing array of uint64_t elements.
*	Like a traditional array, a U64 range vector allows efficient constant
*	time access to elements  with a specified index.
*	A U64 range vector grows transparently as the user adds elements to the
*	array.
*
*	In addition, this vector supports sorting array elements and performing
*	lookup in logarithmic time.
*
*	The cl_u64 vector_t structure should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* SEE ALSO
*	Structures:
*		cl_u64_range_vector_t
*
*	Callbacks:
*		cl_pfn_u64_range_vec_apply_t
*
*	Item Manipulation:
*		cl_u64_range_vector_set
*
*	Initialization:
*		cl_u64_range_vector_construct, cl_u64_range_vector_init, cl_u64_range_vector_destroy
*
*	Manipulation:
*		cl_u64_range_vector_get_capacity, cl_u64_range_vector_set_capacity,
*		cl_u64_range_vector_get_size, cl_u64_range_vector_set_size, cl_u64_range_vector_set_min_size
*		cl_u64_range_vector_get, cl_u64_range_vector_set, cl_u64_range_vector_remove_range, cl_u64_range_vector_insert
*		cl_u64_range_vector_remove_all, cl_u64_range_vector_sort
*
*	Search:
*		cl_u64_range_vector_find_from_start, cl_u64_range_vector_find_from_end
*		cl_u64_range_vector_apply_func, cl_u64_lookup
*********/

typedef struct _cl_u64_range {
	uint64_t min;
	uint64_t max;
} cl_u64_range_t;

/****s* Component Library: U64 Range Vector/cl_u64_range_vector_t
* NAME
*	cl_u64_range_vector_t
*
* DESCRIPTION
*	U64 Range Vector structure.
*
*	The cl_u64_range_vector_t structure should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_u64_range_vector {
	size_t size;
	boolean_t allow_automatic_growth;
	size_t capacity;
	cl_u64_range_t *p_u64_range_array;
	cl_state_t state;
	boolean_t sorted;
	boolean_t ranges_combined;
} cl_u64_range_vector_t;
/*
* FIELDS
*	size
*		 Number of elements successfully initialized in the U64 range vector.
*
*	allow_automatic_growth
*		 Boolean indicator for whether automatic growth of the vector is allowed.
*
*	capacity
*		 total # of elements allocated.
*
*	alloc_list
*		 List of allocations.
*
*	p_u64_range_array
*		 Internal array of U64 range elements.
*
*	state
*		State of the U64 range vector.
*
*	sorted
*		The U64 range vector is sorted.  (For lookup)
*
*	ranges_combined
*		The ranges in the U64 range vector are combined
*		together to be mutually exclusive.  (For lookup/remove)
*
* SEE ALSO
*	U64 Range Vector
*********/

/****d* Component Library: U64 Range Vector/cl_pfn_u64_range_vec_apply_t
* NAME
*	cl_pfn_u64_range_vec_apply_t
*
* DESCRIPTION
*	The cl_pfn_u64_range_vec_apply_t function type defines the prototype for
*	functions used to iterate elements in a U64 range vector.
*
* SYNOPSIS
*/
typedef void
 (*cl_pfn_u64_range_vec_apply_t) (IN const size_t index, IN cl_u64_range_t *element, IN void *context);
/*
* PARAMETERS
*	index
*		[in] Index of the element.
*
*	element
*		[in] Element at the specified index in the U64 range vector.
*
*	context
*		[in] Context provided in a call to cl_u64_range_vector_apply_func.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	This function type is provided as function prototype reference for
*	the function passed by users as a parameter to the cl_u64_range_vector_apply_func
*	function.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_apply_func
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_construct
* NAME
*	cl_u64_range_vector_construct
*
* DESCRIPTION
*	The cl_u64_range_vector_construct function constructs a U64 range vector.
*
* SYNOPSIS
*/
void cl_u64_range_vector_construct(IN cl_u64_range_vector_t * const p_vector);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling cl_u64_range_vector_destroy without first calling
*	cl_u64_range_vector_init.
*
*	Calling cl_u64_range_vector_construct is a prerequisite to calling any other
*	U64 range vector function except cl_u64_range_vector_init.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_init, cl_u64_range_vector_destroy
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_init
* NAME
*	cl_u64_range_vector_init
*
* DESCRIPTION
*	The cl_u64_range_vector_init function initializes a U64 range vector for use.
*
* SYNOPSIS
*/
cl_status_t cl_u64_range_vector_init(IN cl_u64_range_vector_t * const p_vector,
				     IN const size_t min_size,
				     IN boolean_t allow_automatic_growth);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to initialize.
*
*	min_size
*		[in] Initial number of elements.
*
*	allow_automatic_growth
*		[in] Boolean indicator for whether automatic growth of the vector is allowed.
*
* RETURN VALUES
*	CL_SUCCESS if the U64 range vector was initialized successfully.
*
*	CL_INSUFFICIENT_MEMORY if the initialization failed.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_construct, cl_u64_range_vector_destroy,
*	cl_u64_range_vector_set, cl_u64_range_vector_get
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_destroy
* NAME
*	cl_u64_range_vector_destroy
*
* DESCRIPTION
*	The cl_u64_range_vector_destroy function destroys a U64 range vector.
*
* SYNOPSIS
*/
void cl_u64_range_vector_destroy(IN cl_u64_range_vector_t * const p_vector);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_u64_range_vector_destroy frees all memory allocated for the U64 range vector.
*
*	This function should only be called after a call to cl_u64_range_vector_construct
*	or cl_u64_range_vector_init.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_construct, cl_u64_range_vector_init
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_get_capacity
* NAME
*	cl_u64_range_vector_get_capacity
*
* DESCRIPTION
*	The cl_u64_range_vector_get_capacity function returns the capacity of
*	a U64 range vector.
*
* SYNOPSIS
*/
static inline size_t
cl_u64_range_vector_get_capacity(IN const cl_u64_range_vector_t * const p_vector)
{
	CL_ASSERT(p_vector);
	CL_ASSERT(p_vector->state == CL_INITIALIZED);

	return (p_vector->capacity);
}

/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose capacity to return.
*
* RETURN VALUE
*	Capacity, in elements, of the U64 range vector.
*
* NOTES
*	The capacity is the number of elements that the U64 range vector can store,
*	and can be greater than the number of elements stored. To get the number
*	of elements stored in the U64 range vector, use cl_u64_range_vector_get_size.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_set_capacity, cl_u64_range_vector_get_size
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_get_size
* NAME
*	cl_u64_range_vector_get_size
*
* DESCRIPTION
*	The cl_u64_range_vector_get_size function returns the size of a U64 range vector.
*
* SYNOPSIS
*/
static inline uint32_t
cl_u64_range_vector_get_size(IN const cl_u64_range_vector_t * const p_vector)
{
	CL_ASSERT(p_vector);
	CL_ASSERT(p_vector->state == CL_INITIALIZED);
	return ((uint32_t) p_vector->size);

}

/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose size to return.
*
* RETURN VALUE
*	Size, in elements, of the U64 range vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_set_size, cl_u64_range_vector_get_capacity
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_get
* NAME
*	cl_u64_range_vector_get
*
* DESCRIPTION
*	The cl_u64_range_vector_get function returns a pointer to the element stored in a
*	U64 range vector at a specified index.
*
* SYNOPSIS
*/
static inline cl_u64_range_t *cl_u64_range_vector_get(IN const cl_u64_range_vector_t * const p_vector,
						      IN const size_t index)
{
	CL_ASSERT(p_vector);
	CL_ASSERT(p_vector->state == CL_INITIALIZED);
	CL_ASSERT(p_vector->size > index);

	return (&p_vector->p_u64_range_array[index]);
}

/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure from which to get an
*		element.
*
*	index
*		[in] Index of the element.
*
* RETURN VALUE
*	Pointer to the range stored at the specified index.
*
* NOTES
*	cl_u64_range_vector_get provides constant access times regardless of the index.
*
*	cl_u64_range_vector_get does not perform boundary checking. Callers are
*	responsible for providing an index that is within the range of the U64 range
*	vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_set, cl_u64_range_vector_get_size
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_set
* NAME
*	cl_u64_range_vector_set
*
* DESCRIPTION
*	The cl_u64_range_vector_set function sets the element at the specified index.
*
* SYNOPSIS
*/
cl_status_t cl_u64_range_vector_set(IN cl_u64_range_vector_t * const p_vector,
				    IN size_t index, IN uint64_t range_min, IN uint64_t range_max);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure into which to store
*		an element.
*
*	index
*		[in] Index of the element.
*
*	range_min
*		[in] range_min of the element to store in the U64 range vector.
*
*	range_max
*		[in] range_max of the element to store in the U64 range vector.
*
* RETURN VALUES
*	CL_SUCCESS if the element was successfully set.
*
*	CL_INSUFFICIENT_MEMORY if the U64 range vector could not be resized to
*	accommodate the new element.
*
* NOTES
*	cl_u64_range_vector_set grows the U64 range vector as needed to accommodate
*	the new element, unless the allow_automatic_growth parameter passed into the
*	cl_u64_range_vector_init function was FALSE.
*
*	cl_u64_range_vector_set sets U64 range vector status to unsorted.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_get
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_insert
* NAME
*	cl_u64_range_vector_insert
*
* DESCRIPTION
*	The cl_u64_range_vector_insert function inserts a range element into a U64 range vector.
*
* SYNOPSIS
*/
static inline cl_status_t
cl_u64_range_vector_insert(IN cl_u64_range_vector_t * const p_vector,
			   IN uint64_t range_min,
			   IN uint64_t range_max,
			   OUT size_t * const p_index OPTIONAL)
{
	cl_status_t status;

	CL_ASSERT(p_vector);
	CL_ASSERT(p_vector->state == CL_INITIALIZED);

	status = cl_u64_range_vector_set(p_vector, p_vector->size, range_min, range_max);
	if (status == CL_SUCCESS && p_index)
		*p_index = p_vector->size - 1;

	return (status);
}

/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure into which to store
*		an element.
*
*	range_min
*		[in] Range min to store in the U64 range vector.
*
*	range_max
*		[in] Range max to store in the U64 range vector.
*
*	p_index
*		[out] Pointer to the index of the element.  Valid only if
*		insertion was successful.
*
* RETURN VALUES
*	CL_SUCCESS if the element was successfully inserted.
*
*	CL_INSUFFICIENT_MEMORY if the U64 range vector could not be resized to
*	accommodate the new element.
*
* NOTES
*	cl_u64_range_vector_insert places the new element at the end of
*	the U64 range vector.
*
*	cl_u64_range_vector_insert grows the U64 range vector as needed to accommodate
*	the new element, unless the allow_automatic_growth parameter passed into the
*	cl_u64_range_vector_init function was FALSE.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_remove_range, cl_u64_range_vector_set
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_remove_range
* NAME
*	cl_u64_range_vector_remove_range
*
* DESCRIPTION
*	The cl_u64_range_vector_remove_range function removes all the ranges in the vector that are between
*	the min and the max values and updates the ranges that are partially in the provided range.
*	In the case where one of the ranges in the vector fully contains the range to be removed then the larger
*	range will be devided into two ranges with updated min/max and the ranges in the vector will be shifted right.
*	In that case the U64 range vector size will grow by 1. If there is no such case the vector size
*	can be decreased and all ranges after the removed ranges will be shifted left.
*
* SYNOPSIS
*/
cl_status_t cl_u64_range_vector_remove_range(IN cl_u64_range_vector_t * const p_vector,
					     IN uint64_t range_min, IN uint64_t range_max);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure from which to get an
*		element.
*
*	range_min
*		[in] The minimum of the range to be removed.
*
*	range_max
*		[in] The maximum of the range to be removed.
*
* RETURN VALUE
*	Value of the element stored at the specified index.
*
* NOTES
*	cl_u64_range_vector_remove_range does not perform boundary checking. Callers are
*	responsible for providing a vector with size < capacity or allowing automatic growth vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_insert, cl_u64_range_vector_get_size,
*	cl_u64_range_vector_remove_all
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_remove_all
* NAME
*	cl_u64_range_vector_remove_all
*
* DESCRIPTION
*	The cl_u64_range_vector_remove_all function removes all elements from a
*	U64 range vector.
*
* SYNOPSIS
*/
void
cl_u64_range_vector_remove_all(IN cl_u64_range_vector_t * const p_vector);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose size to set.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
* 	cl_u64_range_vector_remove_all removes all elements from the specified U64
* 	vector, and sets its size to 0.
* 	The function does not release the memory, and does not change vector
* 	capacity.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_get_size, cl_u64_range_vector_set_min_size,
*	cl_u64_range_vector_set_capacity, cl_u64_range_vector_remove_range
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_set_capacity
* NAME
*	cl_u64_range_vector_set_capacity
*
* DESCRIPTION
*	The cl_u64_range_vector_set_capacity function reserves memory in a
*	U64 range vector for a specified number of elements.
*
* SYNOPSIS
*/
cl_status_t
cl_u64_range_vector_set_capacity(IN cl_u64_range_vector_t * const p_vector,
			   IN const size_t new_capacity);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose capacity to set.
*
*	new_capacity
*		[in] Total number of elements for which the U64 range vector should
*		allocate memory.
*
* RETURN VALUES
*	CL_SUCCESS if the capacity was successfully set.
*
*	CL_INSUFFICIENT_MEMORY if there was not enough memory to satisfy the
*	operation. The U64 range vector is left unchanged.
*
* NOTES
*	cl_u64_range_vector_set_capacity increases the capacity of the U64 range vector.
*	It does not change the size of the U64 range vector. If the requested
*	capacity is less than the current capacity, the U64 range vector is left
*	unchanged.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_get_capacity, cl_u64_range_vector_set_size,
*	cl_u64_range_vector_set_min_size
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_set_size
* NAME
*	cl_u64_range_vector_set_size
*
* DESCRIPTION
*	The cl_u64_range_vector_set_size function resizes a U64 range vector, either
*	increasing or decreasing its size.
*
* SYNOPSIS
*/
cl_status_t
cl_u64_range_vector_set_size(IN cl_u64_range_vector_t * const p_vector,
		       IN const size_t size);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose size to set.
*
*	size
*		[in] Number of elements desired in the U64 range vector.
*
* RETURN VALUES
*	CL_SUCCESS if the size of the U64 range vector was set successfully.
*
*	CL_INSUFFICIENT_MEMORY if there was not enough memory to complete the
*	operation. The U64 range vector is left unchanged.
*
* NOTES
*	cl_u64_range_vector_set_size sets the U64 range vector to the specified size.
*	If size is smaller than the current size of the U64 range vector, the size
*	is reduced.
*
*	This function can only fail if size is larger than the current capacity.
*
*	When increasing vector size, cl_u64_range_vector_set_size sets U64 range vector
*	status to unsorted.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_get_size, cl_u64_range_vector_set_min_size,
*	cl_u64_range_vector_set_capacity
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_set_min_size
* NAME
*	cl_u64_range_vector_set_min_size
*
* DESCRIPTION
*	The cl_u64_range_vector_set_min_size function resizes a U64 range vector to a
*	specified size if the U64 range vector is smaller than the specified size.
*
* SYNOPSIS
*/
cl_status_t cl_u64_range_vector_set_min_size(IN cl_u64_range_vector_t * const p_vector,
					     IN const size_t min_size);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose minimum size to set.
*
*	min_size
*		[in] Minimum number of elements that the U64 range vector should contain.
*
* RETURN VALUES
*	CL_SUCCESS if the U64 range vector size is greater than or equal to min_size.
*	This could indicate that the U64 range vector's capacity was increased to
*	min_size or that the U64 range vector was already of sufficient size.
*
*	CL_INSUFFICIENT_MEMORY if there was not enough memory to resize the
*	U64 range vector.  The U64 range vector is left unchanged.
*
* NOTES
*	If min_size is smaller than the current size of the U64 range vector,
*	the U64 range vector is unchanged. The U64 range vector is unchanged if the
*	size could not be changed due to insufficient memory being available to
*	perform the operation.
*
*	When increasing vector size, cl_u64_range_vector_set_min_size sets U64 range vector
*	status to unsorted.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_get_size, cl_u64_range_vector_set_size,
*	cl_u64_range_vector_set_capacity
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_apply_func
* NAME
*	cl_u64_range_vector_apply_func
*
* DESCRIPTION
*	The cl_u64_range_vector_apply_func function invokes a specified function for
*	every element in a U64 range vector.
*
* SYNOPSIS
*/
void cl_u64_range_vector_apply_func(IN const cl_u64_range_vector_t * const p_vector,
				    IN cl_pfn_u64_range_vec_apply_t pfn_callback,
				    IN const void *const context);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure whose elements to iterate.
*
*	pfn_callback
*		[in] Function invoked for every element in the array.
*		See the cl_pfn_u64_vec_apply_t function type declaration for details
*		about the callback function.
*
*	context
*		[in] Value to pass to the callback function.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	cl_u64_range_vector_apply_func invokes the specified function for every element
*	in the U64 range vector, starting from the beginning of the U64 range vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_find_from_start, cl_u64_range_vector_find_from_end,
*	cl_pfn_u64_vec_apply_t
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_find_from_start
* NAME
*	cl_u64_range_vector_find_from_start
*
* DESCRIPTION
*	The cl_u64_range_vector_find_from_start function searches elements in a U64
*	vector starting from the lowest index.
*
* SYNOPSIS
*/
size_t cl_u64_range_vector_find_from_start(IN const cl_u64_range_vector_t * const p_vector,
					   IN uint64_t value);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to search.
*
*	value
*		[in] Value to search.
*
* RETURN VALUES
*	Index of the range element that containes the value, if found.
*
*	Size of the U64 range vector if the element containing the value was not found.
*
* NOTES
*	cl_u64_range_vector_find_from_start does not remove the found element from
*	the U64 range vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_find_from_end, cl_u64_range_vector_apply_func,
*	cl_u64_range_vector_sort, cl_u64_range_vector_lookup
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_find_from_end
* NAME
*	cl_u64_range_vector_find_from_end
*
* DESCRIPTION
*	The cl_u64_range_vector_find_from_end function searches elements in a U64
*	vector starting from the highest index.
*
* SYNOPSIS
*/
size_t cl_u64_range_vector_find_from_end(IN const cl_u64_range_vector_t * const p_vector,
					 IN uint64_t value);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to search.
*
*	Value
*		[in] Value to search.
*
* RETURN VALUES
*	Index of the range element that containes the value, if found.
*
*	Size of the U64 range vector if the element containing the value was not found.
*
* NOTES
*	cl_u64_range_vector_find_from_end does not remove the found element from
*	the U64 range vector.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_find_from_start, cl_u64_range_vector_apply_func,
*	cl_u64_range_vector_sort, cl_u64_range_vector_lookup
*********/

/****f* Component Library: U64 Range Vector/cl_u64_range_vector_lookup
* NAME
*	cl_u64_range_vector_lookup
*
* DESCRIPTION
*	The cl_u64_range_vector_lookup function searches for a range that contains the value
*	in a sorted U64 range vector in logarithmic time complexity.
*
* SYNOPSIS
*/
size_t cl_u64_range_vector_lookup(IN cl_u64_range_vector_t * const p_vector,
				  IN const uint64_t value);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to search.
*
*	value
*		[in] Value to search.
*
* RETURN VALUES
*	Index of the range that contains the value, if found.
*
*	Size of the U64 range vector if no range containing the value was found.
*
* NOTES
*	cl_u64_range_vector_lookup does not remove the found range element from
*	the U64 range vector.
*
* 	U64 Range Vector must be sorted before calling cl_u64_range_vector_lookup.
*
* SEE ALSO
*	U64 Range Vector, cl_u64_range_vector_find_from_start, cl_u64_range_vector_apply_func,
*	cl_u64_range_vector_sort.
*********/

/****f* Component Library: U64 range Vector/cl_u64_range_vector_qcontains
* NAME
*	cl_u64_range_vector_qcontains
*
* DESCRIPTION
*	The cl_u64_range_vector_qcontains function returns whether a sorted
*	U64 range vector has a range that contains the value.
*
* SYNOPSIS
*/
boolean_t cl_u64_range_vector_qcontains(IN cl_u64_range_vector_t * const p_vector,
					IN const uint64_t value);
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_vector_t structure to query.
*
*	value
*		[in] value to search.
*
* RETURN VALUES
*	TRUE if the value is found, FALSE otherwise.
*
* NOTES
*	1. cl_u64_range_vector_qcontains does not remove the found value from
*	the U64 range vector.
*	2. The vector must be sorted when calling this function.
*
* SEE ALSO
*	U64 range Vector, cl_u64_range_vector_find_from_start,
*	cl_u64_range_vector_find_from_end, cl_u64_range_vector_sort.
*********/

/****f* Component Library: U64 range Vector/cl_u64_range_vector_is_empty
* NAME
*	cl_u64_range_vector_is_empty
*
* DESCRIPTION
*	The cl_u64_range_vector_is_empty function returns whether a U64 range vector is empty.
*
* SYNOPSIS
*/
static inline
boolean_t cl_u64_range_vector_is_empty(IN const cl_u64_range_vector_t * const p_vector)
{
	return cl_u64_range_vector_get_size(p_vector) == 0;
}
/*
* PARAMETERS
*	p_vector
*		[in] Pointer to a cl_u64_range_vector_t structure to query.
*
* RETURN VALUES
*	TRUE if the vector contains no elements, FALSE otherwise.
*
* SEE ALSO
*	U64 range Vector, cl_u64_range_vector_get_size.
*********/

END_C_DECLS
