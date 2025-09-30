/*
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2018 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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
 * 	Common an2an function declarations.
 *
 * Author:
 *    Elad Weiss, Mellanox
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_spinlock.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_sa.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS


/****f* OpenSM: osm_is_same_physical_link
* NAME
*	osm_is_same_physical_link
*
* DESCRIPTION
*	Check that:
*	1. The link between two given switches through two given port numbers is
*	   a single physical link, i.e. <switch1>:<port1> is connected to
*	   <switch2>:<port2>.
*	2. The link is up.
*	This function is used to validate links taken from the an2an cache.
*
* SYNOPSIS
*/
boolean_t osm_is_same_physical_link(IN osm_sm_t *p_sm,
				    IN osm_switch_t *p_sw1, IN osm_switch_t *p_sw2,
				    IN uint8_t port_num1, IN uint8_t port_num2);

/*
* PARAMETERS
*	p_sm
*		[in] Pointer to a sm object
*
*	p_sw1
*		[in] Pointer to the first switch
*
*	p_sw2
*		[in] Pointer to the second switch
*
*	port_num1
*		[in] The first port number
*
*	port_num2
*		[in] The second port number
*
* RETURN VALUES
*	TRUE if both conditions are met.
*********/


/****f* OpenSM: osm_validate_an2an_links
* NAME
*	osm_validate_an2an_links
*
* DESCRIPTION
*	Iterate over an2an pairs, i.e. adjacent switches with aggregation nodes
*	enabled, and make sure the routing from each switch to its neighbor
*	switch's AN meets the following criteria:
*	1. The routing should change as rarely as possible. This is done by
*	keeping a persistent an2an cache file, and making sure that the routing
*	is always done according to the cache (if the cached link is still
*	valid).
*	2. The routing from switch-1 to AN-2 and from switch-2 to AN-1 should
*	be done on the same physical link.
*
* SYNOPSIS
*/
void osm_validate_an2an_links(IN osm_sm_t * p_sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to a sm object
*********/

END_C_DECLS
