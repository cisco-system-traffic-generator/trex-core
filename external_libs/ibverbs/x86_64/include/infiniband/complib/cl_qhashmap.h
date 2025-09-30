/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#pragma once

#include <complib/cl_qlist.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* Component Library/Hashmap
* NAME
*       Hashmap
*
* DESCRIPTION
*	Provides quick hashmap functionality.
*
*       Quick hashmap is not thread safe, and users must provide serialization.
*
*       The quick hashmap functions operate on a cl_qhashmap_t structure which should
*       be treated as opaque and should be manipulated only through the provided
*       functions.
*
* SEE ALSO
*       Structures:
*               cl_qhashmap_t
*               cl_qhashmap_item_t
*               cl_qhashmap_bucket_t
*
*       Callbacks:
*		cl_qhashmap_apply_t
*
*       Initialization/Destruction:
*               cl_qhashmap_init_uint64, cl_qhashmap_init_memory, cl_qhashmap_init_string,
*		cl_qhashmap_destroy
*
*       Iteration:
*               cl_qhashmap_apply_func
*
*       Search:
*               cl_qhashmap_find
*
*       Manipulation:
*               cl_qhashmap_insert, cl_qhashmap_remove, cl_qhashmap_remove_all
*********/

/* ---------------------------------------------------------------------------------------------- */

/****s* Component Library: Hashmap/cl_qhashmap_type_t
* NAME
*       cl_qhashmap_type_t
*
* DESCRIPTION
*       Hashmap type.
*
*       The cl_qhashmap_type_t enum determines the hashing function for
*	key lookup.
*
* SYNOPSIS
*/
typedef enum {
	cl_qhashmap_type_uint64,
	cl_qhashmap_type_memory,
	cl_qhashmap_type_string,
} cl_qhashmap_type_t;
/*
* VALUES
*       cl_qhashmap_type_uint64
*               The hashmap item keys are 64-bit unsigned numbers.
*
*       cl_qhashmap_type_memory
*               The hashmap item keys are constant size buffers.
*
*       cl_qhashmap_type_string
*               The hashmap item keys are char *.
*********/


/****s* Component Library: Hashmap/cl_qhashmap_item_t
* NAME
*       cl_qhashmap_item_t
*
* DESCRIPTION
*       Hashmap item structure.
*
*       The cl_qhashmap_item_t structure should be treated as opaque and
*	should be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_qhashmap_item cl_qhashmap_item_t;

struct _cl_qhashmap_item {
	/*
	 * Internal use only.
	 */
	cl_qhashmap_item_t	*prev;		// linked list chaining
	cl_qhashmap_item_t	*next;		// linked list chaining
	uint64_t		b_index;	// bucket index

	/*
	 * User provided.
	 */
	uintptr_t		key;
	uintptr_t		value;
};
/*
* FIELDS
*       prev
*               cl_list_item_t for chaining in the buckets.
*
*       next
*               cl_list_item_t for chaining in the buckets.
*
*       b_index
*               Bucket index.
*
*       key
*               User specified key.
*
*       value
*               User specified value.
*********/


/****s* Component Library: Hashmap/cl_qhashmap_bucket_t
* NAME
*       cl_qhashmap_bucket_t
*
* DESCRIPTION
*       List structure.
*
*       The cl_qhashmap_bucket_t structure should be treated as opaque and
*	should be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_qhashmap_bucket {
	cl_qhashmap_item_t	*list;
	uint64_t		index;
	size_t			count;
} cl_qhashmap_bucket_t;
/*
* FIELDS
*       list
*               cl_qlist_t of items in the bucket.
*
*       index
*               Index into the buckets array for this bucket.
*
*       count
*               Count of items in this bucket.
*********/


/****s* Component Library: Hashmap/cl_qhashmap_t
* NAME
*       cl_qhashmap_t
*
* DESCRIPTION
*       Hashmap structure.
*
*       The cl_qhashmap_t structure should be treated as opaque and should be
*       manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_qhashmap {
	cl_qhashmap_type_t	type;
	size_t			count;
	uint64_t		(*f_hash)(uintptr_t, size_t);
	int			(*f_cmp)(uintptr_t, uintptr_t, size_t);
	size_t			num_buckets;
	size_t			key_length;
	cl_qhashmap_bucket_t	*buckets;
} cl_qhashmap_t;
/*
* FIELDS
*       state
*               The state of the hashmap.
*
*       type
*               Hash type (number of string).
*
*       count
*               Number of items in the hashmap.
*
*       num_buckets
*               Count of buckets in the hashmap.
*
* 	key_length
* 		Key length in bytes (when using memory type keys).
*
*       f_hash
*               Function applied to keys for indexing into the buckets.
*
*       f_cmp
*               Function to compare keys.
*
*       buckets
*               An array of buckets for hashing.
*********/


/****d* Component Library: Hashmap/cl_pfn_hashmap_apply_t
* NAME
*       cl_pfn_hashmap_apply_t
*
* DESCRIPTION
*       The cl_pfn_hashmap_apply_t function type defines the prototype for
*	functions used to iterate objects in a hashmap.
*
* SYNOPSIS
*/
typedef void (*cl_qhashmap_apply_t)(IN cl_qhashmap_item_t *p_item,
                                    IN void *context);
/*
* PARAMETERS
*       p_item
*               [in] Hashmap item containing key/value pair.
*
*       context
*               [in] Context provided in a call to cl_qhashmap_apply_func.
*
* RETURN VALUE
*       This function does not return a value.
*
* NOTES
*       This function type is provided as function prototype reference for the
*       function provided by users as a parameter to the cl_qhashmap_apply_func
*       function.
*
* SEE ALSO
*       Hashmap, cl_qhashmap_apply_func
*********/


/****f* Component Library: Hashmap/cl_qhashmap_init_uint64
* NAME
*       cl_qhashmap_init_uint64
*
* DESCRIPTION
*       Initialize a hashmap for numeric keys.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_init_uint64(IN cl_qhashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure to initialize.
*
* RETURN VALUES
*       CL_SUCCESS if the hashmap was initialized successfully.
*
*       CL_INVALID_PARAMETER if p_hash is NULL or type is invalid.
*
*       CL_INSUFFICIENT_MEMORY if the hashmap could not allocate buckets.
*
* NOTES
*       Allocates buckets for use of the hashmap.
*
* SEE ALSO
*       cl_qhashmap_destroy, cl_qhashmap_insert, cl_qhashmap_remove
*********/

/****f* Component Library: Hashmap/cl_qhashmap_init_memory
* NAME
*       cl_qhashmap_init_memory
*
* DESCRIPTION
*       Initialize a hashmap for memory buffer keys.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_init_memory(IN cl_qhashmap_t * const p_hash,
				    IN size_t length);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure to initialize.
*
*	length
*		[in] Key size in bytes.
*
* RETURN VALUES
*       CL_SUCCESS if the hashmap was initialized successfully.
*
*       CL_INVALID_PARAMETER if p_hash is NULL or type is invalid.
*
*       CL_INSUFFICIENT_MEMORY if the hashmap could not allocate buckets.
*
* NOTES
*       Allocates buckets for use of the hashmap.
*
* SEE ALSO
*       cl_qhashmap_destroy, cl_qhashmap_insert, cl_qhashmap_remove
*********/

/****f* Component Library: Hashmap/cl_qhashmap_init_string
* NAME
*       cl_qhashmap_init_string
*
* DESCRIPTION
*       Initialize a hashmap for string buffer keys.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_init_string(IN cl_qhashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure to initialize.
*
* RETURN VALUES
*       CL_SUCCESS if the hashmap was initialized successfully.
*
*       CL_INVALID_PARAMETER if p_hash is NULL or type is invalid.
*
*       CL_INSUFFICIENT_MEMORY if the hashmap could not allocate buckets.
*
* NOTES
*       Allocates buckets for use of the hashmap.
*
* SEE ALSO
*       cl_qhashmap_destroy, cl_qhashmap_insert, cl_qhashmap_remove
*********/

/****f* Component Library: Hashmap/cl_qhashmap_destroy
* NAME
*       cl_qhashmap_destroy
*
* DESCRIPTION
*       Destroy the hashmap.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_destroy(IN cl_qhashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a hashmap to destroy.
*
* RETURN VALUE
*       CL_SUCCESS if the hashmap was destroyed successfully.
*
* NOTES
*       Performs any necessary cleanup of the specified hashmap. Further
*       operations should not be attempted on the hashmap. cl_qhashmap_destroy does
*       not affect any of the objects stored in the hashmap.  It is the callers
*	responsibility to harvest the objects stored in the hashmap.
*
* SEE ALSO
*       cl_qhashmap_init
*********/


/****f* Component Library: Hashmap/cl_qhashmap_insert
* NAME
*       cl_qhashmap_insert
*
* DESCRIPTION
*       Inserts the hashmap item with the give key into the hashmap.
*
* SYNOPSIS
*/
cl_qhashmap_item_t *cl_qhashmap_insert(IN cl_qhashmap_t *p_hash,
				       IN cl_qhashmap_item_t *p_item);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a hashmap into which to add the item.
*
*       p_item
*               [in] Hashmap item containing key/value pair.
*
*       value
*               [in] Pointer to an object to insert into the hashmap.
*
* RETURN VALUES
*	If the key is not in the hashmap, p_item is returned.
*
*	If the key in p_item was already in the hashmap, a pointer to the
*	item containing the key is returned.
*
*	It is the user's responsibility to check the return item.
*
* NOTES
*       Insertion operations may cause the hashmap to resize.
*
* SEE ALSO
*       cl_qhashmap_remove, cl_qhashmap_item_t
*********/


/****f* Component Library: Hashmap/cl_qhashmap_remove
* NAME
*       cl_qhashmap_remove
*
* DESCRIPTION
*       Remove the hashmap item with the give key from the hashmap.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_remove(IN cl_qhashmap_t * const p_hash,
			       IN cl_qhashmap_item_t *p_item);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure from which to remove the
*               item with the specified key.
*
*       p_item
*               [in] Item to remove.
*
* RETURN VALUES
*       CL_SUCCESS if item was removed.
*
*	CL_INVALID_PARAMETER if either parameter is NULL.
*
*       CL_ERROR otherwise.
*
* SEE ALSO
*       cl_qhashmap_remove_all, cl_qhashmap_insert
*********/


/****f* Component Library: Hashmap/cl_qhashmap_remove_all
* NAME
*       cl_qhashmap_remove_all
*
* DESCRIPTION
*       Remove all items from the hashmap.
*
* SYNOPSIS
*/
cl_status_t cl_qhashmap_remove_all(IN cl_qhashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure from which to remove all
*               the items.
*
* RETURN VALUES
*       CL_SUCCESS if the operation succeeded.
*
*	CL_INVALID_PARAMETER if p_hash is NULL.
*
* SEE ALSO
*       cl_qhashmap_remove
*********/


/****f* Component Library: Hashmap/cl_qhashmap_apply_func
* NAME
*       cl_qhashmap_apply_func
*
* DESCRIPTION
*       The cl_qhashmap_apply_func function executes a specified function for every
*       object stored in the hashmap.
*
* SYNOPSIS
*/
void cl_qhashmap_apply_func(IN const cl_qhashmap_t * const p_hash,
			    IN cl_qhashmap_apply_t p_func,
			    IN void * const context);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure to iterate.
*
*       p_func
*               [in] Function invoked for every item in a hashmap.
*               See the cl_pfn_hashmap_apply_t function type declaration for details
*               about the callback function.
*
*       context
*               [in] Value to pass to the callback functions to provide context.
*
* RETURN VALUE
*       This function does not return a value.
*
* NOTES
*       cl_qhashmap_apply_func invokes the specified callback function for every
*       object stored in the hashmap, starting from the head.  The function specified
*       by the pfn_func parameter must not perform any hashmap operations as these
*       would corrupt the hashmap.
*
* SEE ALSO
*       Hashmap, cl_qhashmap_find, cl_pfn_hashmap_apply_t
*********/


/****f* Component Library: Hashmap/cl_qhashmap_find
* NAME
*       cl_qhashmap_find
*
* DESCRIPTION
*       Find the hashmap item associated with the given key.
*
* SYNOPSIS
*/
cl_qhashmap_item_t *cl_qhashmap_find(IN const cl_qhashmap_t * const p_hash,
				     IN uintptr_t key);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_qhashmap_t structure from which to find the item.
*
*       key
*               [in] Key value to search for.
*
* RETURN VALUES
*       Pointer to the object associated with the specified key if it was found.
*
*       NULL if no object with the specified key exists in the hashmap.
*
* SEE ALSO
*       Hashmap
*********/

END_C_DECLS
