/*
 * Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/*
 * Abstract:
 *	provides byteswapping utilities. Basic functions are obtained from
 *  platform specific implementations from byteswap_osd.h.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else	/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif	/* __cplusplus */

BEGIN_C_DECLS
/****h* Component Library/Bitmap
* NAME
*       Bitmap
*
* DESCRIPTION
*       Provides bitmap functionality.  There is no cap on the maximum number
*	of bits in the bitmap.
*
*       Bitmap is not thread safe, and users must provide serialization.
*
*       The cl_bitmap_t structure should be treated as an opaque object and
*       only accessed via the provided API functions.  Each bitmap which is
*	initialized MUST be destroyed in order to avoid a memory leak.
*
* SEE ALSO
*       Structures:
*               cl_bitmap_t
*
*       Initialization/Destruction:
*               cl_bitmap_init, cl_bitmap_clone, cl_bitmap_destroy
*
*       Manipulation:
*               cl_bitmap_reset, cl_bitmap_set, cl_bitmap_clear,
*               cl_bitmap_and, cl_bitmap_or, cl_bitmap_xor,
*
*       Comparison
*               cl_bitmap_all, cl_bitmap_any, cl_bitmap_equal,
*               cl_bitmap_is_set, cl_bitmap_is_clear
*
*       Attributes:
*               cl_bitmap_get
*********/

typedef	struct {
	boolean_t	inited;		// TRUE if initialized
	int		max_bits;	// number of bits in the bitmap
	int		len;		// number of words in the bitmap
	uint64_t	mask;		// mask for upper word in the bitmap
	uint64_t	*bits;		// the data
} cl_bitmap_t;


/****f* Component Library: Bitmap/cl_bitmap_init
* NAME
*       cl_bitmap_init
*
* DESCRIPTION
*       The first function to call for cl_bitmap use.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_init(IN cl_bitmap_t *p_bitmap,
			       IN unsigned bits);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*       bits		[in] Number of bits in the bitmap.  This value
*			     must be a multiple of 8.
*
* RETURN VALUES
*       CL_SUCCESS		the bitmap was initialized successfully.
*	CL_INSUFFICIENT_MEMORY	memory could not be allocated.
*	CL_INVALID_PARAMETER	bits not a multiple of 8
*********/

/****f* Component Library: Bitmap/cl_bitmap_clone
* NAME
*       cl_bitmap_clone
*
* DESCRIPTION
*       Makes a duplicate of the input bitmap.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_clone(IN cl_bitmap_t *p_dst_bitmap,
			        IN cl_bitmap_t *p_src_bitmap);
/*
* PARAMETERS
*       p_dst_bitmap	[in] Pointer to a bitmap.
*       p_src_bitmap	[in] Pointer to a bitmap.
*
* RETURN VALUES
*       CL_SUCCESS	the bitmap was cloned successfully.
*	CL_ERROR	the source bitmap was not initialized
*			or memory could not be allocated.
*********/

/****f* Component Library: Bitmap/cl_bitmap_destroy
* NAME
*       cl_bitmap_destroy
*
* DESCRIPTION
*       Destroy the bitmap
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_destroy(IN cl_bitmap_t *p_bitmap);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*
* RETURN VALUES
*       CL_SUCCESS	the bitmap was destroyed successfully.
*	CL_ERROR	the bitmap was not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_reset
* NAME
*       cl_bitmap_reset
*
* DESCRIPTION
*       Reset all bits to 0.
*
* SYNOPSIS
*/
void		cl_bitmap_reset(IN cl_bitmap_t *p_bitmap);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*
* RETURN VALUES
*       None
*********/

/****f* Component Library: Bitmap/cl_bitmap_get
* NAME
*       cl_bitmap_get
*
* DESCRIPTION
*       Retrieve the value of the specified bit number.
*
* SYNOPSIS
*/
int		cl_bitmap_get(IN cl_bitmap_t *p_bitmap,
			      IN unsigned bit_number);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*       bit_number	[in] Bit number to retrieve.
*
* RETURN VALUES
*       -1		bitmap not initialized.
*			bit number was out of range.
*	0 or 1		bit value.
*********/

/****f* Component Library: Bitmap/cl_bitmap_set
* NAME
*       cl_bitmap_get
*
* DESCRIPTION
*       Set the specified bit number.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_set(IN cl_bitmap_t *p_bitmap,
			      IN unsigned bit_number);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*       bit_number	[in] Bit number to set.
*
* RETURN VALUES
*	CL_SUCCESS	bit was successfully set.
*       CL_ERROR	bitmap not initialized.
*			bit_number out of range.
*********/

/****f* Component Library: Bitmap/cl_bitmap_clear
* NAME
*       cl_bitmap_clear
*
* DESCRIPTION
*       Set the specified bit number to 0.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_clear(IN cl_bitmap_t *p_bitmap,
				IN unsigned bit_number);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to a bitmap.
*       bit_number	[in] Bit number to clear.
*
* RETURN VALUES
*	CL_SUCCESS	bit was successfully cleared.
*       CL_ERROR	bitmap not initialized.
*			bit_number out of range.
*********/

/****f* Component Library: Bitmap/cl_bitmap_and
* NAME
*       cl_bitmap_and
*
* DESCRIPTION
*       Perform an AND operation on the first bitmap using the
*	second bitmap.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_and(IN cl_bitmap_t *p_bitmap,
			      IN cl_bitmap_t *p_other);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to the dst bitmap.
*       p_other		[in] Pointer to the src bitmap.
*
* RETURN VALUES
*	CL_SUCCESS	and operation was successful.
*       CL_ERROR	a bitmap not initialized.
*			bitmap are of different sizes.
*********/

/****f* Component Library: Bitmap/cl_bitmap_or
* NAME
*       cl_bitmap_or
*
* DESCRIPTION
*       Perform an OR operation on the first bitmap using the
*	second bitmap.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_or(IN cl_bitmap_t *p_bitmap,
			     IN cl_bitmap_t *p_other);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to the dst bitmap.
*       p_other		[in] Pointer to the src bitmap.
*
* RETURN VALUES
*	CL_SUCCESS	or operation was successful.
*       CL_ERROR	a bitmap not initialized.
*			bitmap are of different sizes.
*********/

/****f* Component Library: Bitmap/cl_bitmap_xor
* NAME
*       cl_bitmap_xor
*
* DESCRIPTION
*       Perform an XOR operation on the first bitmap using the
*	second bitmap.
*       Set the specified bit number to 0.
*
* SYNOPSIS
*/
cl_status_t	cl_bitmap_xor(IN cl_bitmap_t *p_bitmap,
			      IN cl_bitmap_t *p_other);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to the dst bitmap.
*       p_other		[in] Pointer to the src bitmap.
*
* RETURN VALUES
*	CL_SUCCESS	xor operation was successful.
*       CL_ERROR	a bitmap not initialized.
*			bitmap are of different sizes.
*********/

/****f* Component Library: Bitmap/cl_bitmap_count
* NAME
*       cl_bitmap_xcount
*
* DESCRIPTION
*       Count the number of '1' bits in the map.
*
* SYNOPSIS
*/
int		cl_bitmap_count(IN cl_bitmap_t *p_bitmap);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to the bitmap.
*
* RETURN VALUES
*	-1		bit map not initialized.
*       >= 0		count of '1' bits.
*********/

/****f* Component Library: Bitmap/cl_bitmap_all
* NAME
*       cl_bitmap_all
*
* DESCRIPTION
*       Check if all bits are set to 1.
*
* SYNOPSIS
*/
boolean_t	cl_bitmap_all(IN cl_bitmap_t *p_bitmap);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*
* RETURN VALUES
*	TRUE		all bits were set.
*       FALSE		no bits were set or
*       		bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_any
* NAME
*       cl_bitmap_any
*
* DESCRIPTION
*       Check if any bit is set to 1.
*
* SYNOPSIS
*/
boolean_t	cl_bitmap_any(IN cl_bitmap_t *p_bitmap);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*
* RETURN VALUES
*	TRUE		some bit was set.
*       FALSE		no bits were set or
*       		bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_is_set
* NAME
*       cl_bitmap_is_set
*
* DESCRIPTION
*       Check if the specified bit is set to 1.
*
* SYNOPSIS
*/
boolean_t	cl_bitmap_is_set(IN cl_bitmap_t *p_bitmap,
			         IN unsigned bit_number);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*
* RETURN VALUES
*	TRUE		specified bit was 1.
*       FALSE		no bits were set or
*       		bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_is_clear
* NAME
*       cl_bitmap_is_clear
*
* DESCRIPTION
*       Check if the specified bit is set to 0.
*
* SYNOPSIS
*/
boolean_t	cl_bitmap_is_clear(IN cl_bitmap_t *p_bitmap,
				   IN unsigned bit_number);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*
* RETURN VALUES
*	TRUE		specified bit was 1.
*       FALSE		no bits were set or
*       		bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_equal
* NAME
*       cl_bitmap_equal
*
* DESCRIPTION
*       Check if the specified bitmaps have the same value.
*
* SYNOPSIS
*/
boolean_t	cl_bitmap_equal(IN cl_bitmap_t *p_bitmap,
				IN cl_bitmap_t *p_other);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*       p_other		[in] Pointer to bitmap.
*
* RETURN VALUES
*	TRUE		specified bit was 1.
*       FALSE		no bits were set or
*       		bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_to_array
* NAME
*       cl_bitmap_to_array
*
* DESCRIPTION
*       Copy the bitmap data to the supplied array.
*
* SYNOPSIS
*/
cl_status_t cl_bitmap_to_array(IN cl_bitmap_t *p_bitmap,
                               OUT uint8_t *p_data,
                               IN size_t length);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*       p_data		[out] Pointer to destination data.
*       length		[in] Length to copy (in bits) - must be a multiple of 8.
*
* RETURN VALUES
*	CL_SUCCESS	operation completed successfully.
*       CL_ERROR	bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_from_array
* NAME
*       cl_bitmap_from_array
*
* DESCRIPTION
*       Copy the supplied array to the bitmap data.
*
* SYNOPSIS
*/
cl_status_t cl_bitmap_from_array(IN cl_bitmap_t *p_bitmap,
                                 OUT uint8_t *p_data,
                                 IN size_t length);
/*
* PARAMETERS
*       p_bitmap	[in] Pointer to bitmap.
*       p_data		[out] Pointer to destination data.
*       length		[in] Length to copy (in bits) - must be a multiple of 8.
*
* RETURN VALUES
*	CL_SUCCESS	operation completed successfully.
*       CL_ERROR	bitmap not initialized.
*********/

/****f* Component Library: Bitmap/cl_bitmap_get_set_positions
* NAME
*       cl_bitmap_get_set_positions
*
* DESCRIPTION
*       List the bit numbers of set bits.
*
* SYNOPSIS
*/
int cl_bitmap_get_set_positions(IN cl_bitmap_t *p_bitmap,
				OUT int *p_data,
				IN size_t length);
/*
* PARAMETERS
*       p_bitmap		[in] Pointer to bitmap.
*       p_data			[out] Pointer to destination data.
*       length			[in] Length of destination.
*
* RETURN VALUES
*	-1			Parameter error.
*       >= 0			Number of entries returned in p_data.
*********/

/****f* Component Library: Bitmap/cl_bitmap_get_cleared_positions
* NAME
*       cl_bitmap_get_cleared_positions
*
* DESCRIPTION
*       List the bit numbers of cleared bits.
*
* SYNOPSIS
*/
int cl_bitmap_get_cleared_positions(IN cl_bitmap_t *p_bitmap,
				    OUT int *p_data,
				    IN size_t length);
/*
* PARAMETERS
*       p_bitmap		[in] Pointer to bitmap.
*       p_data			[out] Pointer to destination data.
*       length			[in] Length of destination.
*
* RETURN VALUES
*	-1			Parameter error.
*       >= 0			Number of entries returned in p_data.
*********/

END_C_DECLS
