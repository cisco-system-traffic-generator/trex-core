/*
 * Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009 HNR Consulting. All rights reserved.
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

#include <string.h>
#include <opensm/osm_base.h>
#include <vendor/osm_vendor_api.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/*
 * Abstract:
 * 	Declaration of path related objects.
 *	These objects are part of the OpenSM family of objects.
 */
/****h* OpenSM/DR Path
* NAME
*	DR Path
*
* DESCRIPTION
*	The DR Path structure encapsulates a directed route through the subnet.
*
*	This structure allows direct access to member variables.
*
* AUTHOR
*	Steve King, Intel
*
*********/
/****s* OpenSM: DR Path/osm_dr_path_t
* NAME
*	osm_dr_path_t
*
* DESCRIPTION
*	Directed Route structure.
*
*	This structure allows direct access to member variables.
*
* SYNOPSIS
*/
typedef struct osm_dr_path {
	uint8_t hop_count;
	uint8_t path[IB_SUBNET_PATH_HOPS_MAX];
	uint8_t exit_port_num;
} osm_dr_path_t;
/*
* FIELDS
*	hop_count
*		The number of hops in this path.
*
*	path
*		The array of port numbers that comprise this path.
*
*	exit_port_num
*		The port number the path starts on. Should usually be equal to path[1].
*		Used for picking which bind to use on an aggregated port.
*		a value of 0 means it is unspecified.
*
* SEE ALSO
*	DR Path structure
*********/
/****f* OpenSM: DR Path/osm_dr_path_construct
* NAME
*	osm_dr_path_construct
*
* DESCRIPTION
*	This function constructs a directed route path object.
*
* SYNOPSIS
*/
static inline void osm_dr_path_construct(IN osm_dr_path_t * p_path)
{
	memset(p_path, 0, sizeof(*p_path));
}

/****f* OpenSM: DR Path/osm_dr_path_array_construct
* NAME
*	osm_dr_path_array_construct
*
* DESCRIPTION
*	This function constructs an array of directe route path objects.
*
* SYNOPSIS
*/
static inline void osm_dr_path_array_construct(IN osm_dr_path_t * p_path, int size)
{
	memset(p_path, 0, sizeof(*p_path) * size);
}

/*
* PARAMETERS
*	p_path
*		[in] Pointer to a directed route path object to initialize.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: DR Path/osm_dr_path_init
* NAME
*	osm_dr_path_init
*
* DESCRIPTION
*	This function initializes a directed route path object.
*
* SYNOPSIS
*/
static inline void
osm_dr_path_init(IN osm_dr_path_t * p_path, IN uint8_t hop_count,
		 IN const uint8_t path[IB_SUBNET_PATH_HOPS_MAX], IN uint8_t exit_port_num)
{
	memset(p_path, 0, sizeof(*p_path));

	/* The first location in the path array is reserved. */
	OSM_ASSERT(path[0] == 0);
	OSM_ASSERT(hop_count < IB_SUBNET_PATH_HOPS_MAX);

	if (path[0] == 0) {
		if (hop_count >= IB_SUBNET_PATH_HOPS_MAX)
			hop_count = IB_SUBNET_PATH_HOPS_MAX - 1;

		p_path->hop_count = hop_count;
		memcpy(p_path->path, path, hop_count + 1);
		p_path->exit_port_num = exit_port_num;
	}
}

/*
* PARAMETERS
*	p_path
*		[in] Pointer to a directed route path object to initialize.
*
*	hop_count
*		[in] Hop count needed to reach this node.
*
*	path
*		[in] Directed route path to reach this node.
*
*	exit_port_num
*		[in] When the SM binds to a planarized port, the bind includes more than one
*		physical port, indexed by libvendor.
*		This exit_port_num is the physical port number that should be used on the bind, and
*		is used by libvendor as a hint for which index to use.
*		Legacy behavior of using the default physical port for that planarized bind port is
*		kept by giving port number 0.
*		On paths with length >= 1 that start at a planarized port, exit_port_num should have
*		the same value as path[1].
*		When SM binds to switch, the switch bind port is not planarized, and thus no need
*		for this parameter. Also on switches the bind port number is 0, so a value of 0
*		can be put here.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*********/
/****f* OpenSM: DR Path/osm_dr_path_extend
* NAME
*	osm_dr_path_extend
*
* DESCRIPTION
*	Adds a new hop to a path.
*
* SYNOPSIS
*/
static inline int osm_dr_path_extend(IN osm_dr_path_t * p_path,
				     IN uint8_t port_num)
{
	p_path->hop_count++;

	if (p_path->hop_count >= IB_SUBNET_PATH_HOPS_MAX)
		return -1;
	/*
	   Location 0 in the path array is reserved per IB spec.
	 */
	p_path->path[p_path->hop_count] = port_num;
	return 0;
}

/*
* PARAMETERS
*	p_path
*		[in] Pointer to a directed route path object to initialize.
*
*	port_num
*		[in] Additional port to add to the DR path.
*
* RETURN VALUES
*	0 indicates path was extended.
*	Other than 0 indicates path was not extended.
*
* NOTES
*
* SEE ALSO
*********/
/****f* OpenSM: DR Path/osm_dr_path_is_equal
* NAME
*	osm_dr_path_is_equal
*
* DESCRIPTION
*	Compares DR path to another one.
*
* SYNOPSIS
*/
static inline boolean_t osm_dr_path_is_equal(IN osm_dr_path_t * p_dr_path,
				   IN uint8_t * p_path,
				   IN uint8_t hops)
{
	OSM_ASSERT(p_dr_path);
	OSM_ASSERT(p_path);

	if (p_dr_path->hop_count != hops)
		return FALSE;

	if (memcmp(p_dr_path->path, p_path, hops + 1))
		return FALSE;

	return TRUE;
}

/*
* PARAMETERS
*	p_dr_path
*		[in] A pointer to first DR path.
*
*	p_path
*		[in] A pointer to array of ports that comprise
*		     the second DR path.
*
*	hops
*		[in] Number of hops in second DR path.
*
* RETURN VALUES
*	TRUE	indicates paths were equal.
*	FALSE	indicates paths were different.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: MAD Wrapper/osm_madw_get_smp_dr
* DESCRIPTION
*	Initializes an osm_dr_path_t struct corresponding to the SMP mad's initial path
*
* SYNOPSIS
*/
static inline void osm_madw_get_smp_dr(IN const osm_madw_t * p_madw,
				       OUT osm_dr_path_t * p_dr_path)
{
	const ib_mad_t *p_mad = osm_madw_get_mad_ptr(p_madw);
	const ib_smp_t *p_smp;

	OSM_ASSERT(p_mad->mgmt_class == IB_MCLASS_SUBN_DIR ||
		   p_mad->mgmt_class == IB_MCLASS_SUBN_LID);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	osm_dr_path_init(p_dr_path, p_smp->hop_count, p_smp->initial_path,
			 p_madw->mad_addr.addr_type.smi.port_num);
}
/*
* PARAMETERS
*	p_madw
*		[in] Pointer to an osm_madw_t object containing an SMP MAD.
*
*	p_dr_path
*		[out] Pointer to an osm_dr_path_t object to write the path to.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
