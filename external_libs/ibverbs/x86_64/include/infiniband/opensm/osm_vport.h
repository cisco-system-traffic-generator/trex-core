/*
 * Copyright (c) 2014 Mellanox Technologies LTD. All rights reserved.
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
 *	Declaration of virtual ports (vports) related objects.
 */

#pragma once

#include <complib/cl_qmap.h>
#include <opensm/osm_pkey.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/*
	Forward references.
*/
struct osm_port;
struct osm_pkeybl;
struct osm_sm;
struct osm_subn;
struct osm_alias_guid;

/****s* OpenSM: Virtual Port/osm_vport_t
* NAME
*	osm_vport_t
*
* DESCRIPTION
*	This object represents a virtual port on a logical port.
*
* SYNOPSIS
*/
typedef struct osm_vport {
	cl_map_item_t map_item;
	ib_vport_info_t vport_info;
	uint16_t index;
	ib_net64_t (*p_guids)[];
	struct osm_port *p_port;
	struct osm_pkeybl pkeys;
	ib_net64_t vnode_guid;
	uint8_t vnode_port;
	boolean_t report_new_vport;
	qos_config_sl_t qos_config_sl;
	boolean_t query_vport_info;
} osm_vport_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	vport_info
*		The IBA defined VPortInfo data for this Virtual Port.
*
*	index
*		The index of this Virtual Port within the logical port
*
*	p_guids
*		Pointer to array of GUIDs obtained from GUIDInfo.
*
*	p_port
*		Pointer to the parent Port object of this Virtual Port.
*
*	pkeys
*		osm_pkey_tbl_t object holding the port PKeys.
*
*	vnode_guid
*		guid of VNode which this VPort is connected to
*
*	vnode_port
*		port number in VNode to which this VPort is connected
*
*	query_vport_info
*		Flag indicating that OpenSM should requery VPortInfo of this
*		virtual port.
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Virtual Port/osm_vport_get_guid
* NAME
*	osm_vport_get_guid
*
* DESCRIPTION
*	Getter for the virtual port guid
*
* SYNOPSIS
*/
static inline ib_net64_t osm_vport_get_guid(IN osm_vport_t * p_vport) {
	OSM_ASSERT(p_vport);
	return p_vport->vport_info.port_guid;
}

/****f* OpenSM: Virtual Port/osm_vport_get_lid
* NAME
*	osm_vport_get_lid
*
* DESCRIPTION
*	Getter for the virtual port lid
*
* SYNOPSIS
*/
static inline ib_net16_t osm_vport_get_lid(IN osm_vport_t * p_vport) {
	OSM_ASSERT(p_vport);
	return p_vport->vport_info.vport_lid;
}

/*
* PARAMETERS
*	p_vport
*		[in] Pointer to an osm_vport_t object.
*
* RETURN VALUES
*	Guid of given virtual port.
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_get_max_pkey_block
* NAME
*	osm_vport_get_max_pkey_block
*
* DESCRIPTION
*	Return the max pkey block number for a given vport object
*
* SYNOPSIS
*/
uint16_t osm_vport_get_max_pkey_block(IN struct osm_sm *p_sm,
				      IN osm_vport_t *p_vport);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*	p_vport
*		[in] Pointer to an osm_vport_t object.
*
* RETURN VALUES
*	Max pkey block number for the given vport.
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_get_max_pkeys_and_blocks
* NAME
*	osm_vport_get_max_pkeys_and_blocks
*
* DESCRIPTION
*	Return the max number of pkeys and blocks for a given vport object
*
* SYNOPSIS
*/
void osm_vport_get_max_pkeys_and_blocks(IN struct osm_sm *p_sm,
					IN osm_vport_t *p_vport,
					OUT uint16_t *p_max_pkeys,
					OUT uint16_t *p_max_blocks);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*	p_vport
*		[in] Pointer to an osm_vport_t object.
*	p_max_pkeys
*		[out] Pointer to use in order to fill max pkeys
*	p_max_blocks
*		[out] Pointer to use in order to fill max blocks
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_set_pkey_tbl
* NAME
*	osm_vport_set_pkey_tbl
*
* DESCRIPTION
*	Set pkey table block within a vport pkey table
*
* SYNOPSIS
*/
void osm_vport_set_pkey_tbl(IN osm_log_t * p_log, IN struct osm_subn *p_subn,
			    IN osm_vport_t * p_vport,
			    IN ib_pkey_table_t * p_pkey_tbl,
			    IN uint16_t block_num,
			    IN boolean_t is_set);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to log object
*	p_subn
*		[in] Pointer to osm_subn_t object
*	p_vport
*		[in] Pointer to vport object to set
*	p_pkey_tbl
*		[in] Pointer to pkey table from which the pkey block should
*		     be taken
*	block_num
*		[in] Number of pkey block to set
*	is_set
*		[in] Indication if this is driven by vpkey tbl set response
*
* RETURN VALUES
*	This method does not return a value
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_update_pkey_tbl
* NAME
*	osm_vport_update_pkey_tbl
*
* DESCRIPTION
*	This method sends pkey table set to all blocks (which changed) for
*	vport pkey table
*
* SYNOPSIS
*/
void osm_vport_update_pkey_tbl(IN struct osm_sm * sm, IN osm_vport_t *p_vport,
			       IN char *owner_desc);
/*
* PARAMETERS
*	sm
*		[in] Pointer to osm_sm_t object
*	p_vport
*		[in] Pointer to vport object
*	owner_desc
*		[in] Description of the owner of the pkey table
*
* RETURN VALUES
*	This method does not return a value
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_calc_lid
* NAME
*	osm_vport_calc_lid
*
* DESCRIPTION
*	Calculates the lid used by a vport.
*	- If the vport requires a lid the function return its lid.
*	- If the vport depends on the lid of a different vport the function return
*	  the lid of the other vport which this vport depands on.
*	- If Vport not requires a lid and not depended on other VPort lid the
*	  function returns the port base lid
*
* SYNOPSIS
*/
ib_net16_t osm_vport_calc_lid(IN osm_vport_t *p_vport);
/*
* PARAMETERS
*	p_vport
*		[in] Pointer to virtual port object
*
* RETURN VALUES
*	Lid used by the vport (or 0 if lid not found)
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_get_lid_by_aguid
* NAME
*	osm_vport_get_lid_by_aguid
*
* DESCRIPTION
*	Get the lid used by a vport by its alias GUID.
*	- If the vport requires a lid the function return its lid.
*	- If the vport depends on the lid of a different vport the function return
*	  the lid of the other vport which this vport depands on.
*	- If Vport not requires a lid and not depended on other VPort lid the
*	  function returns the port base lid
*
* SYNOPSIS
*/
ib_net16_t osm_vport_get_lid_by_aguid(IN const struct osm_alias_guid *p_aguid);
/*
* PARAMETERS
*	p_aguid
*		[in] Pointer to alias guid object
*
* RETURN VALUES
*	Lid used by the vport (or 0 if lid not found)
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_get_aguid
* NAME
*	osm_vport_get_aguid
*
* DESCRIPTION
*	Get the alias guid of a given VPort
*
* SYNOPSIS
*/
struct osm_alias_guid *osm_vport_get_aguid(IN struct osm_subn *p_subn,
					   IN osm_vport_t *p_vport);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object
*
*	p_vport
*		[in] Pointer to VPort
*
* RETURN VALUES
*	Pointer to the alias guid of the VPort.
*	NULL if alias guid of the VPort is not found.
*
* NOTES
*
*********/

/****f* OpenSM: Virtual Port/osm_vport_is_lid_required
* NAME
*	osm_vport_is_lid_required
*
* DESCRIPTION
*	Check if lid is required for given vport
*
* SYNOPSIS
*/
static inline boolean_t osm_vport_is_lid_required(IN osm_vport_t *p_vport)
{
	if (ib_vport_info_get_lid_required(&p_vport->vport_info))
		return TRUE;
	else
		return FALSE;
}
/*
* PARAMETERS
*	p_vport
*		[in] Pointer to VPort
*
* RETURN VALUES
*	Boolean indication whether lidRequired bit is enabled for the given
*	vport
*
* NOTES
*
*********/
END_C_DECLS
