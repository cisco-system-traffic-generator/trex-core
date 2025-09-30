/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2008 Mellanox Technologies LTD. All rights reserved.
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
 * 	Header file that describes Unicast Cache functions.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.4 $
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <opensm/osm_switch.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

struct osm_ucast_mgr;

/****h* OpenSM/Unicast Manager/Unicast Cache
* NAME
*	Unicast Cache
*
* DESCRIPTION
*	The Unicast Cache object encapsulates the information
*	needed to cache and write unicast routing of the subnet.
*
*	The Unicast Cache object is NOT thread safe.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Yevgeny Kliteynik, Mellanox
*
*********/

/****f* OpenSM: Unicast Cache/osm_ucast_cache_invalidate
* NAME
*	osm_ucast_cache_invalidate
*
* DESCRIPTION
*	The osm_ucast_cache_invalidate function purges the
*	unicast cache and marks the cache as invalid.
*
* SYNOPSIS
*/
void osm_ucast_cache_invalidate(IN struct osm_ucast_mgr *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the ucast mgr object.
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*
* SEE ALSO
*	Unicast Manager object
*********/
/****f* OpenSM: Unicast Cache/osm_ucast_cache_vlid_invalidate
* NAME
*	osm_ucast_cache_vlid_invalidate
*
* DESCRIPTION
*	The osm_ucast_cache_vlid_invalidate function purges the
*	unicast vLID cache and marks it as invalid.
*
* SYNOPSIS
*/
void osm_ucast_cache_vlid_invalidate(struct osm_ucast_mgr *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the ucast mgr object.
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*
* SEE ALSO
*	Unicast Manager object
*********/


/****f* OpenSM: Unicast Cache/osm_ucast_cache_check_new_link
* NAME
*	osm_ucast_cache_check_new_link
*
* DESCRIPTION
*	The osm_ucast_cache_check_new_link checks whether
*	the newly discovered link still allows us to use
*	cached unicast routing.
*
* SYNOPSIS
*/
void osm_ucast_cache_check_new_link(IN struct osm_ucast_mgr *p_mgr,
				    IN osm_node_t * p_node_1,
				    IN uint8_t port_num_1,
				    IN osm_node_t * p_node_2,
				    IN uint8_t port_num_2);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
*	p_node_1
*		[in] Pointer to the first node of the link.
*
*	port_num_1
*		[in] Port number of the first node.
*
*	p_node_2
*		[in] Pointer to the second node of the link.
*
*	port_num_2
*		[in] Port number of the second node.
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*	The function checks whether the link was previously
*	cached/dropped or is this a completely new link.
*	If it decides that the new link makes cached routing
*	invalid, the cache is purged and marked as invalid.
*
* SEE ALSO
*	Unicast Cache object
*********/

/****f* OpenSM: Unicast Cache/osm_ucast_cache_add_link
* NAME
*	osm_ucast_cache_add_link
*
* DESCRIPTION
*	The osm_ucast_cache_add_link adds link to the cache.
*
* SYNOPSIS
*/
void osm_ucast_cache_add_link(IN struct osm_ucast_mgr *p_mgr,
			      IN osm_physp_t * physp1,
			      IN osm_physp_t * physp2);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
*	physp1
*		[in] Pointer to the first physical port of the link.
*
*	physp2
*		[in] Pointer to the second physical port of the link.
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*	Since the cache operates with ports and not links,
*	the function adds two port entries (both sides of the
*	link) to the cache.
*	If it decides that the dropped link makes cached routing
*	invalid, the cache is purged and marked as invalid.
*
* SEE ALSO
*	Unicast Manager object
*********/

/****f* OpenSM: Unicast Cache/osm_ucast_cache_add_node
* NAME
*	osm_ucast_cache_add_node
*
* DESCRIPTION
*	The osm_ucast_cache_add_node adds node and all
*	its links to the cache.
*
* SYNOPSIS
*/
void osm_ucast_cache_add_node(IN struct osm_ucast_mgr *p_mgr,
			      IN osm_node_t * p_node);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
*	p_node
*		[in] Pointer to the node object that should be cached.
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*	If the function decides that the dropped node makes cached
*	routing invalid, the cache is purged and marked as invalid.
*
* SEE ALSO
*	Unicast Manager object
*********/

/****f* OpenSM: Unicast Cache/osm_ucast_cache_process
* NAME
*	osm_ucast_cache_process
*
* DESCRIPTION
*	The osm_ucast_cache_process function writes the
*	cached unicast routing on the subnet switches.
*
* SYNOPSIS
*/
int osm_ucast_cache_process(IN struct osm_ucast_mgr *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
* RETURN VALUE
*	This function returns zero on success and non-zero
*	value otherwise.
*
* NOTES
*	Iterates through all the subnet switches and writes
*	the LFTs that were calculated during the last routing
*       engine execution to the switches.
*
* SEE ALSO
*	Unicast Manager object
*********/

/****f* OpenSM: Unicast Cache/osm_ucast_cache_sw_count
* NAME
*	osm_ucast_cache_sw_count
*
* DESCRIPTION
*	The osm_ucast_cache_sw_count function count the number
*	of cached switches
*
* SYNOPSIS
*/
uint32_t osm_ucast_cache_sw_count(struct osm_ucast_mgr *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
* RETURN VALUE
*	This function returns the number of cached switches
*
* NOTES
*
* SEE ALSO
*	Unicast Manager object
*********/
/****f* OpenSM: Unicast Cache/osm_ucast_cache_add_vport
* NAME
*	osm_ucast_cache_add_vport
*
* DESCRIPTION
*	The osm_ucast_cache_add_vport adds vLID of the vPort
*	to the vlid cache.
*
* SYNOPSIS
*/
void osm_ucast_cache_add_vport(struct osm_ucast_mgr *p_mgr,
			       osm_port_t * p_port,
			       uint16_t vport_index);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
*	p_port
*		[in] Pointer to the Port object
*
*	vport_index
*		[in] Index of the vPort
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*	If the function decides that vLID of the vPort
*	can't be added the vLID cache is purged
*	and marked as invalid.
*
* SEE ALSO
*	Unicast Manager object
*********/
/****f* OpenSM: Unicast Cache/osm_ucast_cache_remove_vport
* NAME
*	osm_ucast_cache_remove_vport
*
* DESCRIPTION
*	The osm_ucast_cache_remove_vport removes vLID record
*	from the vlid cache.
*
* SYNOPSIS
*/
void osm_ucast_cache_remove_vport(struct osm_ucast_mgr * p_mgr,
				  osm_port_t * p_port,
				  uint16_t vport_index,
				  ib_net16_t vlid);

/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the unicast manager object.
*
*	p_port
*		[in] Pointer to the Port object
*
*	vport_index
*		[in] Index of the vPort
*
*	vlid
*		[in] vLID to be removed
*		In some cases vlid can be different from
*		vport.vlid
*
* RETURN VALUE
*	This function does not return any value.
*
* NOTES
*	If the function decides that vLID can't be removed
*	the vLID cache is purged and marked as invalid.
*
* SEE ALSO
*	Unicast Manager object
*********/


END_C_DECLS
