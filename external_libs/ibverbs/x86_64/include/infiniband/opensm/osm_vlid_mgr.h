/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2016 Mellanox Technologies LTD. All rights reserved.
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
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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
 *      Virtual Lid Manager
 */

#pragma once

#include <iba/ib_types.h>
#include <opensm/osm_log.h>

struct osm_sm;

/****s* VlidMgr/osm_vlid_mgr_t
* NAME
*  osm_vlid_mgr_t
*
* DESCRIPTION
*	This type represents a virtual lid manager.
*
*	The manager keeps data about all virtual lids and the lids of the
*	physical port that they belong to.
*
* SYNOPSIS
*/
typedef struct osm_vlid_mgr {
	uint16_t vlids[IB_LID_UCAST_END_HO + 1];
	uint16_t next[IB_LID_UCAST_END_HO + 1];
	uint16_t last[IB_LID_UCAST_END_HO + 1];
	uint16_t lids[IB_LID_UCAST_END_HO + 1];
	uint16_t lids_num;
	boolean_t iter_constructed;
	boolean_t update_since_apply;
	uint16_t min_vlid;
	uint16_t max_vlid;
} osm_vlid_mgr_t;
/*
* FIELDS
*	vlids
*		vlid to lid mapping
*		Each index represent vlid and contain the lid of the physical
*		port this vlid belong to
*
*	next
*		The value of the next vlid which belong to the same physical
*		port
*
*	last
*		The value of the last vport belong to this physical port
*		This is updated only in indexes of physical port lid
*
*	lids
*		An array which keeps in its beginning the list of physical
*		lids which has virtual lids belong to them
*
*	lids_num
*		Number of lids which has vlids belong to it (length of lids
*		array)
*
*	iter_constructed
*		The vlid mgr offers iterators over lids and their vlids but
*		the iterators needs to be constructoed after updates in the db
*		so this indicates whether iterator were constructed since last
*		update of the db
*
*	update_since_apply
*		Indication whether the db was updated since the last time that
*		it was applied to lft tables
*
*	min_vlid
*		The minimal vlid value ever set in the db.
*		NOTICE: This value is used to optimize performance of iter
*			construction, this vlid might already be cleared and
*			not in the db anymore but it give us a minimal value
*			to start construction from even if it is no longer in
*			the db.
*			One must not relate to this value as the current
*			minimal vlid value!
*
*	max_vlid
*		The maximal vlid value ever set in the db.
*		NOTICE: This value is used to optimize performance of iter
*			construction, this vlid might already be cleared and
*			not in the db anymore but it give us a maximal value
*			to stop construction in even if it is no longer in
*			the db.
*			One must not relate to this value as the current
*			maximal vlid value!
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_init
* NAME osm_vlid_mgr_init
*
* DESCRIPTION
*	Init the vlid manager
*
* SYNOPSIS
*
*/
static inline void osm_vlid_mgr_init(osm_vlid_mgr_t *p_mgr)
{
	memset(p_mgr, 0, sizeof(osm_vlid_mgr_t));
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	None
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_set_vlid
* NAME osm_vlid_mgr_set_vlid
*
* DESCRIPTION
*	Set vlid with the lid it belong to in the manager db
*
* SYNOPSIS
*
*/
static inline void osm_vlid_mgr_set_vlid(osm_vlid_mgr_t *p_mgr, uint16_t vlid,
					 uint16_t lid)
{
	p_mgr->vlids[vlid] = lid;
	p_mgr->iter_constructed = FALSE;
	p_mgr->update_since_apply = TRUE;
	if (!p_mgr->min_vlid || (vlid < p_mgr->min_vlid))
		p_mgr->min_vlid = vlid;
	if (vlid > p_mgr->max_vlid)
		p_mgr->max_vlid = vlid;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
*	vlid
*		virtual lid to set
*
*	lid
*		lid of physical port this virtual lid belong to
*
* RETURN VALUE
*	None
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_clear_vlid
* NAME osm_vlid_mgr_clear_vlid
*
* DESCRIPTION
*	Clear vlid from the manager db
*
* SYNOPSIS
*
*/
static inline void osm_vlid_mgr_clear_vlid(osm_vlid_mgr_t *p_mgr, uint16_t vlid)
{
	p_mgr->vlids[vlid] = 0;
	p_mgr->iter_constructed = FALSE;
	p_mgr->update_since_apply = TRUE;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
*	vlid
*		virtual lid to set
*
*	lid
*		lid of physical port this virtual lid belong to
*
* RETURN VALUE
*	None
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_construct_iter
* NAME osm_vlid_mgr_construct_iter
*
* DESCRIPTION
*	Construct LID to VLID iterator if needed
*
* SYNOPSIS
*
*/
void osm_vlid_mgr_construct_iter(osm_vlid_mgr_t *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	None
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_first_vlid
* NAME osm_vlid_mgr_get_first_vlid
*
* DESCRIPTION
*	This function gets the lid of a physical port and return the first
*	(lowest) vlid which belong to this port.
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_first_vlid
		       (osm_vlid_mgr_t *p_mgr, uint16_t lid)
{
	if (!p_mgr->iter_constructed)
		osm_vlid_mgr_construct_iter(p_mgr);
	return p_mgr->next[lid];
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
*	vlid
*		A virtual lid number
*
* RETURN VALUE
*	the next vlid in the sorted list of vlids which belong to the same
*	port.
*
*
*	Zero - In case that this is the last (highest) vlid in the list or in
*		case that the given lid is not a vlid.
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_next_vlid
* NAME osm_vlid_mgr_get_next_vlid
*
* DESCRIPTION
*	This function gets the number of a vlid and return the next vlid which
*	belongs to the same physical port as the given lid.
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_next_vlid
		       (osm_vlid_mgr_t *p_mgr, uint16_t vlid)
{
	if (!p_mgr->iter_constructed)
		osm_vlid_mgr_construct_iter(p_mgr);
	return p_mgr->next[vlid];
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
*	vlid
*		A virtual lid number
*
* RETURN VALUE
*	the next vlid in the sorted list of vlids which belong to the same
*	port.
*
*
*	Zero - In case that this is the last (highest) vlid in the list or in
*		case that the given lid is not a vlid.
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_port_count
* NAME osm_vlid_mgr_get_port_count
*
* DESCRIPTION
*	This function return the number of ports which has virtual lids which
*	belongs to them
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_port_count(osm_vlid_mgr_t *p_mgr)
{
	if (!p_mgr->iter_constructed)
		osm_vlid_mgr_construct_iter(p_mgr);
	return p_mgr->lids_num;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	The number of physical ports which has virtual lids belong to them.
*
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_port_lid_by_index
* NAME osm_vlid_mgr_get_port_lid_by_index
*
* DESCRIPTION
*	This function returns a port lid in a given index from the list of ports
*	which has vports belongs to them.
*	This function is to be used in order to iterate over these ports.
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_port_lid_by_index
		       (osm_vlid_mgr_t *p_mgr, uint16_t index)
{
	if (!p_mgr->iter_constructed)
		osm_vlid_mgr_construct_iter(p_mgr);
	return p_mgr->lids[index];
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
*	index
*		Index of the port in the physical port list (start with 0)
*
* RETURN VALUE
*	the lid of the port in the given index within the list of physical
*	ports which has virtual lids belong to them.
*
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_max_vlid
* NAME osm_vlid_mgr_get_max_vlid
*
* DESCRIPTION
*	Return the max virtual lid value
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_max_vlid(osm_vlid_mgr_t *p_mgr)
{
	if (!p_mgr->iter_constructed)
		osm_vlid_mgr_construct_iter(p_mgr);
	return p_mgr->max_vlid;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	Maximal virtual lid value (0 if not virtual lid exist)
*
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_reset_update_since_apply
* NAME osm_vlid_mgr_reset_update_since_apply
*
* DESCRIPTION
*	Reset the update_since_apply indication (to FALSE)
*
* SYNOPSIS
*
*/
static inline void osm_vlid_mgr_reset_update_since_apply(osm_vlid_mgr_t *p_mgr)
{
	p_mgr->update_since_apply = FALSE;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	None
*
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_is_update_since_apply
* NAME osm_vlid_mgr_is_update_since_apply
*
* DESCRIPTION
*	Check if vlid mgr db was updated since last time it was applied to lfts
*
* SYNOPSIS
*
*/
static inline boolean_t osm_vlid_mgr_is_update_since_apply(osm_vlid_mgr_t *p_mgr)
{
	return p_mgr->update_since_apply;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	Indication whether changes done to vlid mgr db since the last time
*	it was applied to lfts
*
*/

/****f* VlidMgr: VlidMgr/osm_vlid_mgr_get_lid_by_vlid
* NAME osm_vlid_mgr_get_lid_by_vlid
*
* DESCRIPTION
*	Get lid of port for which a given virtual port lid belongs to
*
* SYNOPSIS
*
*/
static inline uint16_t osm_vlid_mgr_get_lid_by_vlid(osm_vlid_mgr_t *p_mgr,
						     uint16_t vlid)
{
	if (IB_LID_UCAST_START_HO <= vlid && vlid <= IB_LID_UCAST_END_HO)
		return p_mgr->vlids[vlid];

	return 0;
}
/*
* PARAMETERS
*	p_mgr
*		Pointer to vlid manager
*
* RETURN VALUE
*	Lid of port or zero if not found
*
*/
