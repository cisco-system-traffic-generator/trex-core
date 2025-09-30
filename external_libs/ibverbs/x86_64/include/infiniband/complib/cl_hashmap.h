/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <complib/cl_qlist.h>
#include <complib/cl_qhashmap.h>

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
*	Provides hashmap functionality.
*
*       Hashmap is not thread safe, and users must provide serialization.
*
*       The hashmap functions operate on a cl_hashmap_t structure which should
*       be treated as opaque and should be manipulated only through the provided
*       functions.
*
* SEE ALSO
*       Structures:
*               cl_hashmap_t
*               cl_hashmap_item_t
*               cl_hashmap_bucket_t
*
*       Callbacks:
*		cl_hashmap_apply_t
*
*       Initialization/Destruction:
*               cl_hashmap_init, cl_hashmap_destroy
*
*       Iteration:
*               cl_hashmap_apply_func
*
*       Search:
*               cl_hashmap_find
*
*       Manipulation:
*               cl_hashmap_insert, cl_hashmap_remove, cl_hashmap_remove_all
*********/

/* ---------------------------------------------------------------------------------------------- */

/****s* Component Library: Hashmap/cl_hashmap_t
* NAME
*       cl_hashmap_t
*
* DESCRIPTION
*       Hashmap structure.
*
*       The cl_hashmap_t structure should be treated as opaque and should be
*       manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_hashmap {
	cl_qhashmap_t		hashmap;
	cl_qhashmap_item_t	*free;
	size_t			free_count;
} cl_hashmap_t;
/*
* FIELDS
*       hashmap
*               The underlying qhashmap.
*
*       free
*               Free items for insertion.
*
*       free_count
*               Number of free items.
*********/


/****d* Component Library: Hashmap/cl_hashmap_apply_t
* NAME
*       cl_hashmap_apply_t
*
* DESCRIPTION
*       The cl_hashmap_apply_t function type defines the prototype for
*	functions used to iterate objects in a hashmap.
*
* SYNOPSIS
*/
typedef void (*cl_hashmap_apply_t) (IN const cl_qhashmap_item_t *p_item,
                                    IN void *context);
/*
* PARAMETERS
*       p_item
*               [in] Hashmap item containing key/value pair.
*
*       context
*               [in] Context provided in a call to cl_hashmap_apply_func.
*
* RETURN VALUE
*       This function does not return a value.
*
* NOTES
*       This function type is provided as function prototype reference for the
*       function provided by users as a parameter to the cl_hashmap_apply_func
*       function.
*
* SEE ALSO
*       Hashmap, cl_hashmap_apply_func
*********/


/****f* Component Library: Hashmap/cl_hashmap_init_uint64
* NAME
*       cl_hashmap_init_uint64
*
* DESCRIPTION
*       Initialize a hashmap for numeric keys.
*
* SYNOPSIS
*/
cl_status_t cl_hashmap_init_uint64(IN cl_hashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure to initialize.
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
*       cl_hashmap_destroy, cl_hashmap_insert, cl_hashmap_remove
*********/

/****f* Component Library: Hashmap/cl_hashmap_init_memory
* NAME
*       cl_hashmap_init_memory
*
* DESCRIPTION
*       Initialize a hashmap for memory buffer keys.
*
* SYNOPSIS
*/
cl_status_t cl_hashmap_init_memory(IN cl_hashmap_t * const p_hash, size_t length);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure to initialize.
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
*       cl_hashmap_destroy, cl_hashmap_insert, cl_hashmap_remove
*********/

/****f* Component Library: Hashmap/cl_hashmap_init_string
* NAME
*       cl_hashmap_init_string
*
* DESCRIPTION
*       Initialize a hashmap for string buffer keys.
*
* SYNOPSIS
*/
cl_status_t cl_hashmap_init_string(IN cl_hashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure to initialize.
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
*       cl_hashmap_destroy, cl_hashmap_insert, cl_hashmap_remove
*********/

/****f* Component Library: Hashmap/cl_hashmap_destroy
* NAME
*       cl_hashmap_destroy
*
* DESCRIPTION
*       Destroy the hashmap.
*
* SYNOPSIS
*/
void cl_hashmap_destroy(IN cl_hashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a hashmap to destroy.
*
* RETURN VALUE
*       None
*
* NOTES
*       Performs any necessary cleanup of the specified hashmap. Further
*       operations should not be attempted on the hashmap. cl_hashmap_destroy does
*       not affect any of the objects stored in the hashmap.  It is the callers
*	responsibility to harvest the objects stored in the hashmap.
*
* SEE ALSO
*       cl_hashmap_init
*********/


/****f* Component Library: Hashmap/cl_hashmap_insert
* NAME
*       cl_hashmap_insert
*
* DESCRIPTION
*       Inserts the hashmap item with the give key into the hashmap.
*
* SYNOPSIS
*/
cl_status_t cl_hashmap_insert(IN cl_hashmap_t *p_hash,
			      IN uintptr_t key,
			      IN uintptr_t value);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a hashmap into which to add the item.
*
*       key
*               [in] Hashmap item key for the value.
*
*       value
*               [in] Pointer to an object to insert into the hashmap.
*
* RETURN VALUES
*	CL_SUCCESS		If the key/value pair is inserted into the hashmap.
*
*	CL_DUPLICATE		If the key is already in the hashmap.
*
*	CL_INSUFFICIENT_MEMORY	If the hashmap_item can't be alloced.
*
* NOTES
*       Insertion operations may cause the hashmap to resize.
*
* SEE ALSO
*       cl_hashmap_remove, cl_hashmap_item_t
*********/


/****f* Component Library: Hashmap/cl_hashmap_remove
* NAME
*       cl_hashmap_remove
*
* DESCRIPTION
*       Remove the hashmap item with the give key from the hashmap.
*
* SYNOPSIS
*/
cl_status_t cl_hashmap_remove(IN cl_hashmap_t * const p_hash,
			      IN uintptr_t key);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure from which to remove the
*               item with the specified key.
*
*       key
*               [in] Key to search for and remove.
*
* RETURN VALUES
*       CL_SUCCESS if item was removed.
*
*	CL_INVALID_PARAMETER if either parameter is NULL.
*
*       CL_ERROR otherwise.
*
* SEE ALSO
*       cl_hashmap_remove_all, cl_hashmap_insert
*********/


/****f* Component Library: Hashmap/cl_hashmap_remove_all
* NAME
*       cl_hashmap_remove_all
*
* DESCRIPTION
*       Remove all items from the hashmap.
*
* SYNOPSIS
*/
void cl_hashmap_remove_all(IN cl_hashmap_t * const p_hash);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure from which to remove all
*               the items.
*
* RETURN VALUES
*       None
*
* SEE ALSO
*       cl_hashmap_remove
*********/


/****f* Component Library: Hashmap/cl_hashmap_apply_func
* NAME
*       cl_hashmap_apply_func
*
* DESCRIPTION
*       The cl_hashmap_apply_func function executes a specified function for every
*       object stored in the hashmap.
*
* SYNOPSIS
*/
void cl_hashmap_apply_func(IN cl_hashmap_t * const p_hash,
                           IN const cl_qhashmap_apply_t p_func,
                           IN void * const context);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure to iterate.
*
*       p_func
*               [in] Function invoked for every item in a hashmap.
*               See the cl_hashmap_apply_t function type declaration for details
*               about the callback function.
*
*       context
*               [in] Value to pass to the callback functions to provide context.
*
* RETURN VALUE
*       This function does not return a value.
*
* NOTES
*       cl_hashmap_apply_func invokes the specified callback function for every
*       object stored in the hashmap, starting from the head.  The function specified
*       by the func parameter must not perform any hashmap operations as these
*       would corrupt the hashmap.
*
* SEE ALSO
*       Hashmap, cl_hashmap_find, cl_hashmap_apply_t
*********/


/****f* Component Library: Hashmap/cl_hashmap_find
* NAME
*       cl_hashmap_find
*
* DESCRIPTION
*       Find the hashmap item associated with the given key.
*
* SYNOPSIS
*/
uintptr_t cl_hashmap_find(IN cl_hashmap_t * const p_hash,
			  IN uintptr_t key);
/*
* PARAMETERS
*       p_hash
*               [in] Pointer to a cl_hashmap_t structure from which to find the item.
*
*       key
*               [in] Key value to search for.
*
* RETURN VALUES
*       uintptr_t of the value associated with this key.
*
*       0 if no object with the specified key exists in the hashmap.
*
* SEE ALSO
*       Hashmap
*********/

END_C_DECLS
