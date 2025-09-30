/*
 * Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2004 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else                           /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif                          /* __cplusplus */

BEGIN_C_DECLS

/*
 * Internal fraction macros.
 */
#define F_lhs(F1,F2)		(F1.num * F2.den)
#define F_rhs(F1,F2)		(F1.den * F2.num)


/****h* Component Library/Fraction
* NAME
*Fraction
*
* DESCRIPTION
*	Provides fraction functionality.
*
*	Fraction is thread safe.
*
*	The fraction functions operate on a cl_fraction_t structure which should
*	be treated as opaque and should be manipulated only through the provided
*	functions.
*
*	Initialization/Destruction:
*		cl_fraction, cl_fraction_from_string
*
*	Manipulation:
*		cl_fraction_add, cl_fraction_sub, cl_fraction_mul, cl_fraction_div
*		cl_fraction_scale, cl_fraction_floor, cl_fraction_ceil
*
*	Comparison:
*		cl_fraction_eq, cl_fraction_ne,
*		cl_fraction_gt, cl_fraction_ge
*		cl_fraction_lt, cl_fraction_le
*
*	Miscellaneous:
*		cl_fraction_to_string
*********/

/****s* Component Library: Fraction/cl_fraction_t
* NAME
*       cl_fraction_t
*
* DESCRIPTION
*       Fraction structure.
*
*       The cl_fraction_t structure should be treated as opaque and should be
*       manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct {
	boolean_t		nan;
	int64_t			num;
	int64_t			den;
} cl_fraction_t;

/*
* FIELDS
*       num
*               Fraction numerator.
*
*       den
*               Fraction denominator.
*********/

/* ==============================================================================================================
 *
 * API functions.
 */

/****f* Component Library: Fraction/cl_fraction_is_valid
* NAME
*       cl_fraction_is_valid
*
* DESCRIPTION
*       Validate a fraction.
*
* SYNOPSIS
*/
boolean_t cl_fraction_is_valid(IN cl_fraction_t f);
/*
* PARAMETERS
*       f
*               [in] Fraction to validate.
*
* RETURN VALUES
*	TRUE if fraction is valid
*	FALSE otherwise
*********/

/****f* Component Library: Fraction/cl_fraction
* NAME
*       cl_fraction
*
* DESCRIPTION
*       Initialize a fraction.
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction(IN int64_t a, IN int64_t b);
/*
* PARAMETERS
*       a
*               [in] Numerator.
*       b
*               [in] Denominator.
*
* NOTES
*	'b' MUST not be zero.
*
* RETURN VALUES
*	nan if b == 0
*	a/b otherwise
*********/

/****f* Component Library: Fraction/cl_fraction_add
* NAME
*       cl_fraction_add
*
* DESCRIPTION
*       Add two fractions.
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction_add(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*       f1 + f2
*
* SEE ALSO
*       cl_fraction_sub, cl_fraction_mul, cl_fraction_div
*********/

/****f* Component Library: Fraction/cl_fraction_sub
* NAME
*       cl_fraction_sub
*
* DESCRIPTION
*       Subtracts two fractions.
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction_sub(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*       f1 - f2
*
* SEE ALSO
*       cl_fraction_add, cl_fraction_mul, cl_fraction_div
*********/

/****f* Component Library: Fraction/cl_fraction_mul
* NAME
*       cl_fraction_mul
*
* DESCRIPTION
*       Multiply two fractions.
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction_mul(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*       f1 * f2
*
* SEE ALSO
*       cl_fraction_add, cl_fraction_sub, cl_fraction_div
*********/

/****f* Component Library: Fraction/cl_fraction_div
* NAME
*       cl_fraction_div
*
* DESCRIPTION
*       Divide two fractions.
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction_div(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*       f1 / f2
*
* NOTES
*	The second fraction (f2) must not be zero.
*
* SEE ALSO
*       cl_fraction_add, cl_fraction_sub, cl_fraction_mul
*********/

/****f* Component Library: Fraction/cl_fraction_eq
* NAME
*       cl_fraction_eq
*
* DESCRIPTION
*       Compares two fractions for equality.
*
* SYNOPSIS
*/
boolean_t cl_fraction_eq(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 == f2
*	FALSE	if f1 != f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_ne
* NAME
*       cl_fraction_ne
*
* DESCRIPTION
*       Compares two fractions for non-equality.
*
* SYNOPSIS
*/
boolean_t cl_fraction_ne(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 != f2
*	FALSE	if f1 == f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_gt
* NAME
*       cl_fraction_gt
*
* DESCRIPTION
*       Compares two fractions for greater than.
*
* SYNOPSIS
*/
boolean_t cl_fraction_gt(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 >  f2
*	FALSE	if f1 <= f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_ge
* NAME
*       cl_fraction_ge
*
* DESCRIPTION
*       Compares two fractions for greater than or equality.
*
* SYNOPSIS
*/
boolean_t cl_fraction_ge(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 >= f2
*	FALSE	if f1 <  f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_lt
* NAME
*       cl_fraction_lt
*
* DESCRIPTION
*       Compares two fractions for less than.
*
* SYNOPSIS
*/
boolean_t cl_fraction_lt(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 <  f2
*	FALSE	if f1 >= f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_le
* NAME
*       cl_fraction_le
*
* DESCRIPTION
*       Compares two fractions for less than or equality.
*
* SYNOPSIS
*/
boolean_t cl_fraction_le(IN cl_fraction_t f1, IN cl_fraction_t f2);
/*
* PARAMETERS
*       f1
*               [in] First fraction.
*       f2
*               [in] Second fraction.
*
* RETURN VALUES
*	TRUE	if f1 <= f2
*	FALSE	if f1 >  f2
*
* SEE ALSO
*       cl_fraction_eq, cl_fraction_ne
*	cl_fraction_gt, cl_fraction_ge
*	cl_fraction_lt, cl_fraction_le
*********/

/****f* Component Library: Fraction/cl_fraction_scale
* NAME
*       cl_fraction_scale
*
* DESCRIPTION
*       Scale a fraction by an integer multiple
*
* SYNOPSIS
*/
cl_fraction_t cl_fraction_scale(IN cl_fraction_t f, IN int64_t value);
/*
* PARAMETERS
*       f
*               [in] Fraction to scale.
*       value
*               [in] Scaling factor.
*
* RETURN VALUES
*	The scaled fraction (ie, f * v)
*
* SEE ALSO
*       cl_fraction_floor, cl_fraction_ceil
*********/

/****f* Component Library: Fraction/cl_fraction_floor
* NAME
*       cl_fraction_floor
*
* DESCRIPTION
*       Calculate the floor of the fraction.
*
* SYNOPSIS
*/
int64_t cl_fraction_floor(IN cl_fraction_t f);
/*
* PARAMETERS
*       f
*               [in] Fraction.
*
* RETURN VALUES
*	floor(f) if fraction is valid
*	INT64_MIN otherwise
*
* SEE ALSO
*       cl_fraction_scale, cl_fraction_ceil
*********/

/****f* Component Library: Fraction/cl_fraction_ceil
* NAME
*       cl_fraction_ceil
*
* DESCRIPTION
*       Calculate the ceil of the fraction.
*
* SYNOPSIS
*/
int64_t cl_fraction_ceil(IN cl_fraction_t f);
/*
* PARAMETERS
*       f
*               [in] Fraction.
*
* RETURN VALUES
*	ceil(f) if fraction is valid
*	INT64_MAX otherwise
*
* SEE ALSO
*       cl_fraction_scale, cl_fraction_floor
*********/

/****f* Component Library: Fraction/cl_fraction_from_string
* NAME
*       cl_fraction_from_string
*
* DESCRIPTION
*       Initialize a fraction from a string.
*
* SYNOPSIS
*	The input string can be either of the following:
*		"a"	- a is an int64_t.
*		"a/b"	- a and b are int64_t.
*/
cl_fraction_t cl_fraction_from_string(IN char *string);
/*
* PARAMETERS
*       string
*               [in] Character string to parse.
*
* NOTES
*	The return value will be incorrect for invalid input values.
*
* RETURN VALUES
*	nan fraction if string == NULL
*	nan fraction if invalid input
*	a/1 if string is of the form "a"
*	a/b otherwise
*
* SEE ALSO
*       cl_fraction_to_string
*********/

/****f* Component Library: Fraction/cl_fraction
* NAME
*       cl_fraction_to_string
*
* DESCRIPTION
*       Create an ASCII representation of the fraction.
*
* SYNOPSIS
*/
char *cl_fraction_to_string(cl_fraction_t f, IN char *buffer);
/*
* PARAMETERS
*       f
*               [in] Fraction to represent.
*       string
*               [in] Resulting character string buffer.
*
* NOTES
*	The buffer MUST be at least 64 bytes long.
*
* RETURN VALUES
*	NULL if buffer == NULL
*	"NAN" if invalid fraction
*	"a/b" otherwise
*
* SEE ALSO
*       cl_fraction_from_string
*********/

END_C_DECLS

