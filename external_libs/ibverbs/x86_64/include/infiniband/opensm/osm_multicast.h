/*
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2012 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2013 Oracle and/or its affiliates. All rights reserved.
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
 * 	Declaration of osm_mgrp_t.
 *	This object represents an IBA Multicast Group.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_fleximap.h>
#include <complib/cl_qlist.h>
#include <complib/cl_spinlock.h>
#include <opensm/osm_base.h>
#include <opensm/osm_mtree.h>
#include <opensm/osm_mcm_port.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Multicast Group
* NAME
*	Multicast Group
*
* DESCRIPTION
*	The Multicast Group encapsulates the information needed by the
*	OpenSM to manage Multicast Groups.  The OpenSM allocates one
*	Multicast Group object per Multicast Group in the IBA subnet.
*
*	The Multicast Group is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****s* OpenSM: Multicast Group/osm_mgrp_t
* NAME
*	osm_mgrp_t
*
* DESCRIPTION
*	Multicast Group structure.
*
*	The osm_mgrp_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_mgrp {
	cl_fmap_item_t map_item;
	cl_list_item_t list_item;
	cl_list_item_t prtn_list_item;
	struct osm_prtn *p_prtn;
	ib_net16_t mlid;
	cl_qmap_t mcm_port_tbl;
	cl_qmap_t mcm_alias_port_tbl;
	ib_member_rec_t mcmember_rec;
	boolean_t well_known;
	unsigned full_members;
} osm_mgrp_t;
/*
* FIELDS
*	map_item
*		Map Item for fmap linkage.  Must be first element!!
*
*	list_item
*		List item for linkage in osm_mgrp_box's mgrp_list qlist.
*
*	prtn_list_item
*		List item for linkage in osm_prtn_t's mgrp_list qlist.
*
*	p_prtn
*		Pointer to osm_prtn_t object of the partition of the group.
*
*	mlid
*		The network ordered LID of this Multicast Group (must be
*		>= 0xC000).
*
*	mcm_port_tbl
*		Table (sorted by port GUID) of osm_mcm_port_t objects
*		representing the member ports of this multicast group.
*
*	mcm_alias_port_tbl
*		Table (sorted by port alias GUID) of osm_mcm_port_t
*		objects representing the member ports of this multicast
*		group.
*
*	mcmember_rec
*		Holds the parameters of the Multicast Group.
*
*	well_known
*		Indicates that this is the wellknown multicast group which
*		is created during the initialization of SM/SA and will be
*		present even if there are no ports for this group
*
* 	full_members
*		Number of full members in the Multicast Group.
*
* SEE ALSO
*********/

/****d* OpenSM: OpenSM/osm_mgrp_box_imr_enum
* NAME
*       osm_mgrp_box_imr_enum
*
* DESCRIPTION
*       Enumerates the possible states of osm_mgrp_box_t.imr_state.
*
* SYNOPSIS
*/
typedef enum _osm_mgrp_box_imr_enum {
	OSM_MGRP_BOX_IMR_SUPPORTED = 0,
	OSM_MGRP_BOX_IMR_ON_IDLE,
	OSM_MGRP_BOX_IMR_NOT_SUPPORTED
} osm_mgrp_box_imr_enum;
/***********/

/****s* OpenSM: Multicast Group/osm_mgrp_box_t
* NAME
*	osm_mgrp_box_t
*
* DESCRIPTION
*	Multicast structure which holds all multicast groups with same MLID.
*
* SYNOPSIS
*/
typedef struct osm_mgrp_box {
	uint16_t mlid;
	cl_qlist_t mgrp_list;
	osm_mtree_node_t *root;
	uint64_t root_guid;
	osm_mgrp_box_imr_enum imr_state;
	boolean_t imr_tree_changed_flag;
	cl_qmap_t mtree_nodes_tbl;
	int num_failed_routes;
} osm_mgrp_box_t;
/*
* FIELDS
*	mlid
*		The host ordered LID of this Multicast Group (must be
*		>= 0xC000).
*
*	mgrp_list
*		List of multicast groups (mpgr object) having same MLID value.
*
*	root
*		Pointer to the root "tree node" in the single spanning tree
*		for this multicast group.  The nodes of the tree represent
*		switches.  Member ports are not represented in the tree.
*
*	imr_state
*		Indicates whether this object supports incremental multicast
*		routing.
*
*	imr_tree_changed_flag
*		Indicates that tree has changed, and needs to be rerouted.
*
*	mtree_nodes_tbl
*		Table of mtree_node_t object of the multicast tree.
*
* 	num_failed_routes
* 		Number of members that could not be added to the multicast tree.
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_new
* NAME
*	osm_mgrp_new
*
* DESCRIPTION
*	Allocates and initializes a Multicast Group for use.
*
* SYNOPSIS
*/
osm_mgrp_t *osm_mgrp_new(IN osm_subn_t * subn, IN ib_net16_t mlid,
			 IN ib_member_rec_t * mcmr);
/*
* PARAMETERS
*	subn
*		[in] Pointer to osm_subn_t object.
*
*	mlid
*		[in] Multicast LID for this multicast group.
*
*	mcmr
*		[in] MCMember Record for this multicast group.
*
* RETURN VALUES
*	IB_SUCCESS if initialization was successful.
*
* NOTES
*	Allows calling other Multicast Group methods.
*
* SEE ALSO
*	Multicast Group, osm_mgrp_delete
*********/

/*
 * Need a forward declaration to work around include loop:
 * osm_sm.h <- osm_multicast.h
 */
struct osm_sm;

/****f* OpenSM: Multicast Tree/osm_purge_mtree
* NAME
*	osm_purge_mtree
*
* DESCRIPTION
*	Frees all the nodes in a multicast spanning tree
*
* SYNOPSIS
*/
void osm_purge_mtree(IN osm_subn_t * p_subn, IN osm_mgrp_box_t * mgb);
/*
* PARAMETERS
*	sm
*		[in] Pointer to osm_subn_t object.
*	mgb
*		[in] Pointer to an osm_mgrp_box_t object.
*
* RETURN VALUES
*	None.
*
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_is_guid
* NAME
*	osm_mgrp_is_guid
*
* DESCRIPTION
*	Indicates if the specified port GUID is a member of the Multicast Group.
*
* SYNOPSIS
*/
static inline boolean_t osm_mgrp_is_guid(IN const osm_mgrp_t * p_mgrp,
					 IN ib_net64_t port_guid)
{
	return (cl_qmap_get(&p_mgrp->mcm_port_tbl, port_guid) !=
		cl_qmap_end(&p_mgrp->mcm_port_tbl));
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Port GUID.
*
* RETURN VALUES
*	TRUE if the port GUID is a member of the group,
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_is_empty
* NAME
*	osm_mgrp_is_empty
*
* DESCRIPTION
*	Indicates if the multicast group has any member ports.
*
* SYNOPSIS
*/
static inline boolean_t osm_mgrp_is_empty(IN const osm_mgrp_t * p_mgrp)
{
	return (cl_qmap_count(&p_mgrp->mcm_port_tbl) == 0);
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
* RETURN VALUES
*	TRUE if there are no ports in the multicast group.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_get_mlid
* NAME
*	osm_mgrp_get_mlid
*
* DESCRIPTION
*	The osm_mgrp_get_mlid function returns the multicast LID of this group.
*
* SYNOPSIS
*/
static inline ib_net16_t osm_mgrp_get_mlid(IN const osm_mgrp_t * p_mgrp)
{
	return p_mgrp->mlid;
}

/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
* RETURN VALUES
*	MLID of the Multicast Group.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

/****d* OpenSM: SA/osm_sa_state_t
* NAME
*	osm_sa_record_set_status
*
* DESCRIPTION
*	Enumerates the possible statuses of the sa record set.
*
* SYNOPSIS
*/
typedef enum _osm_sa_record_set_status {
	OSM_SA_RECORD_SET_SUCCESS = 0,
	OSM_SA_RECORD_SET_INSUFFICIENT_MEMORY,
	OSM_SA_RECORD_SET_PLANE_ASYMMETRIC
} osm_sa_record_set_status_t;
/***********/

/****f* OpenSM: SA/osm_sa_get_sa_record_set_status_str
* NAME
*	osm_sa_get_sa_record_set_status_str
*
* DESCRIPTION
*	Returns a string for the specified SA Record set status value.
*
* SYNOPSIS
*/
const char *osm_sa_get_sa_record_set_status_str(IN osm_sa_record_set_status_t sa_record_set_status);
/*
* PARAMETERS
*	sa_record_set_status
*		[in] SA record set status.
*
* RETURN VALUES
*	Pointer to the status string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_add_port
* NAME
*	osm_mgrp_add_port
*
* DESCRIPTION
*	Adds a port to the multicast group.
*
* SYNOPSIS
*/
osm_sa_record_set_status_t osm_mgrp_add_port(osm_subn_t *subn, osm_log_t *log,
					     IN osm_mgrp_t * mgrp, IN osm_port_t *port,
					     IN ib_member_rec_t *mcmr, IN boolean_t proxy);
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet object
*
*	log
*		[in] The log object pointer
*
*	mgrp
*		[in] Pointer to an osm_mgrp_t object to initialize.
*
*	port
*		[in] Pointer to an osm_port_t object
*
*	mcmr
*		[in] Pointer to MCMember record received for the join
*
*	proxy
*		[in] The proxy join state for this port in the group.
*
* RETURN VALUES
*	OSM_SA_RECORD_SET_SUCCESS
*	OSM_SA_RECORD_SET_INSUFFICIENT_MEMORY
*	OSM_SA_RECORD_SET_PLANE_ASYMMETRIC
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_get_mcm_port
* NAME
*	osm_mgrp_get_mcm_port
*
* DESCRIPTION
*	Finds a port in the multicast group.
*
* SYNOPSIS
*/
osm_mcm_port_t *osm_mgrp_get_mcm_port(IN const osm_mgrp_t * p_mgrp,
				      IN ib_net64_t port_guid);
/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Port guid.
*
* RETURN VALUES
*	Pointer to the mcm port object when present or NULL otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_get_mcm_alias_guid
* NAME
*	osm_mgrp_get_mcm_alias_guid
*
* DESCRIPTION
*	Finds an mcm alias GUID in the multicast group based on an alias GUID.
*
* SYNOPSIS
*/
osm_mcm_alias_guid_t *osm_mgrp_get_mcm_alias_guid(IN const osm_mgrp_t * p_mgrp,
						  IN ib_net64_t port_guid);
/*
* PARAMETERS
*	p_mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port_guid
*		[in] Alias port guid.
*
* RETURN VALUES
*	Pointer to the mcm alias GUID object when present or NULL otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_delete_port
* NAME
*	osm_mgrp_delete_port
*
* DESCRIPTION
*	Removes a port from the multicast group.
*
* SYNOPSIS
*/
void osm_mgrp_delete_port(IN osm_subn_t * subn, IN osm_log_t * log,
			  IN osm_mgrp_t * mgrp, IN osm_port_t * port);
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet object
*
*	log
*		[in] The log object pointer
*
*	mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	port
*		[in] Pointer to an osm_port_t object for the the departing port.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Multicast Group/osm_mgrp_remove_mcm_alias_guid
* NAME
*	osm_mgrp_remove_mcm_alias_guid
*
* DESCRIPTION
*	Removes a multicast member record from the multicast group.
*
* SYNOPSIS
*/
boolean_t osm_mgrp_remove_mcm_alias_guid(IN osm_subn_t * subn, osm_log_t * log,
					 IN osm_mgrp_t * mgrp,
					 IN osm_mcm_alias_guid_t * mcm_alias_guid);
/*
* PARAMETERS
*
*	subn
*		[in] Pointer to the subnet object
*
*	log
*		[in] The log object pointer
*
*	mgrp
*		[in] Pointer to an osm_mgrp_t object.
*
*	mcm_alias_guid
*		[in] Pointer to a mcm_alias_guid_t object of the departing member.
*
* RETURN VALUES
*	TRUE if the multicast group was closed as a result of the operation.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

boolean_t osm_mgrp_remove_port(osm_subn_t * subn, osm_log_t * log, osm_mgrp_t * mgrp,
			  osm_mcm_alias_guid_t * mcm_alias_guid,
			  ib_member_rec_t * mcmr);
void osm_mgrp_cleanup(osm_subn_t * subn, osm_mgrp_t * mpgr);
void osm_mgrp_box_delete(osm_subn_t * subn, osm_mgrp_box_t *mbox);
void osm_mgrp_box_clear_changes_tbl(osm_mgrp_box_t *mbox);

/****f* OpenSM: Multicast Group/osm_mgrp_box_clear_mtree
* NAME
*	osm_mgrp_box_clear_mtree
*
* DESCRIPTION
* 	Clears multicast tree information from osm_mgrp_box_t object.
*
* SYNOPSIS
*/
void osm_mgrp_box_clear_mtree(osm_subn_t * p_subn, osm_mgrp_box_t * mbox);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object.
*
*	mbox
*		[in] Pointer to an osm_mgrp_box_t object.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Multicast Group
*********/

END_C_DECLS
