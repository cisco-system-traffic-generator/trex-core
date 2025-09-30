/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005,2008, 2015 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_router_t.
 *	This object represents an IBA router.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <iba/ib_types.h>
#include <opensm/osm_base.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_node.h>
#include <opensm/osm_port.h>
#include <opensm/osm_mcast_tbl.h>
#include <opensm/osm_port_profile.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Router
* NAME
*	Router
*
* DESCRIPTION
*	The Router object encapsulates the information needed by the
*	OpenSM to manage routers.  The OpenSM allocates one router object
*	per router in the IBA subnet.
*
*	The Router object is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Hal Rosenstock, Voltaire
*
*********/

/****s* OpenSM: osm_router_tbl_t
*NAME
*       osm_router_tbl_t
*
* DESCRIPTION
*  This object stores Router Table blocks (AdjacentSiteLocalSubnetsTable MADs)
*
* The osm_router_tbl_t object should be treated as opaque and should
* be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_router_tbl {
        cl_ptr_vector_t blocks;
} osm_router_tbl_t;

/*
* FIELDS
*  blocks
*   The VS defined blocks of router table blocks, updated from the subnet
*
**********/

/****f* OpenSM: osm_router_tbl_construct
* NAME
*  osm_router_tbl_construct
*
* DESCRIPTION
*  Constructs the Router table object
*
* SYNOPSIS
*/
void osm_router_tbl_construct(IN osm_router_tbl_t * p_router_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_delete
* NAME
*  osm_router_tbl_delete
*
* DESCRIPTION
*  Deletes all blocks from the Router table
*
* SYNOPSIS
*/
void osm_router_tbl_delete(IN osm_router_tbl_t * p_router_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_destroy
* NAME
*  osm_router_tbl_destroy
*
* DESCRIPTION
*  Destroys the Router table object
*
* SYNOPSIS
*/
void osm_router_tbl_destroy(IN osm_router_tbl_t * p_router_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_init
* NAME
*  osm_router_tbl_init
*
* DESCRIPTION
*  Inits the Router table object
*
* SYNOPSIS
*/
ib_api_status_t osm_router_tbl_init(IN osm_router_tbl_t * p_router_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_get_num_blocks
* NAME
*  osm_router_tbl_get_num_blocks
*
* DESCRIPTION
*  Obtain the number of blocks in the router table
*
* SYNOPSIS
*/
static inline uint16_t
osm_router_tbl_get_num_blocks(IN const osm_router_tbl_t * p_router_tbl)
{
	return ((uint16_t) (cl_ptr_vector_get_size(&p_router_tbl->blocks)));
}

/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
* RETURN VALUES
*  The number of blocks in the router table
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_adj_block_get
* NAME
*  osm_router_tbl_adj_block_get
*
* DESCRIPTION
*  Obtain the pointer for a specific adj block
*
* SYNOPSIS
*/
static inline ib_rtr_adj_table_block_t *
osm_router_tbl_adj_block_get(const osm_router_tbl_t * p_router_tbl, uint16_t block)
{
	return ((block < cl_ptr_vector_get_size(&p_router_tbl->blocks)) ?
		(ib_rtr_adj_table_block_t *)cl_ptr_vector_get(
		&p_router_tbl->blocks, block) : NULL);
};

/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
*  block
*     [in] The block number to get
*
* RETURN VALUES
*  The IB router table, which is one block in the osm router table object.
*  Returns NULL in case of invalid block number.
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_tbl_nh_block_get
* NAME
*  osm_router_tbl_nh_block_get
*
* DESCRIPTION
*  Obtain the pointer for a specific next hop block
*
* SYNOPSIS
*/
static inline ib_rtr_next_hop_table_block_t *
osm_router_tbl_nh_block_get(const osm_router_tbl_t * p_router_tbl, uint16_t block)
{
	return ((block < cl_ptr_vector_get_size(&p_router_tbl->blocks)) ?
		(ib_rtr_next_hop_table_block_t *)cl_ptr_vector_get(
		&p_router_tbl->blocks, block) : NULL);
};

/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object.
*
*  block
*     [in] The block number to get
*
* RETURN VALUES
*  The IB router table, which is one block in the osm router table object.
*  Returns NULL in case of invalid block number.
*
* NOTES
*
*********/

/****f* OpenSM: osm_adj_router_tbl_set
* NAME
*  osm_adj_router_tbl_set
*
* DESCRIPTION
*  Set the adjacent Router table block provided in the Router object.
*
* SYNOPSIS
*/
ib_api_status_t
osm_adj_router_tbl_set(IN osm_router_tbl_t * p_router_tbl,
		       IN uint16_t block,
		       IN ib_rtr_adj_table_block_t * p_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object
*
*  block
*     [in] The block number to set
*
*  p_tbl
*     [in] The IB Router adjacent table block to copy to the object
*
* RETURN VALUES
*  IB_SUCCESS or IB_ERROR
*
* NOTES
*
*********/

/****f* OpenSM: osm_next_hop_router_tbl_set
* NAME
*  osm_next_hop_router_tbl_set
*
* DESCRIPTION
*  Set the next hop Router table block provided in the Router object.
*
* SYNOPSIS
*/
ib_api_status_t
osm_next_hop_router_tbl_set(IN osm_router_tbl_t * p_router_tbl,
			    IN uint16_t block,
			    IN ib_rtr_next_hop_table_block_t * p_tbl);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object
*
*  block
*     [in] The block number to set
*
*  p_tbl
*     [in] IB Router next hop table block to copy to the router table object
*
* RETURN VALUES
*  IB_SUCCESS or IB_ERROR
*
* NOTES
*
*********/

/********************************* END OF ROUTER TABLE ***********************/
/****s* OpenSM: Subnet/osm_router_tbl_data_t
* NAME
*	osm_router_tbl_data_t
*
* DESCRIPTION
*	This struct defines the data to keep on router in routers table
*	This will be used for both adjacent and next hop routers
* SYNOPSIS
*/
typedef struct osm_router_tbl_data {
	cl_fmap_item_t map_item;
	ib_net64_t subnet_prefix;
	ib_net16_t pkey;
	ib_net16_t master_sm_lid;
	boolean_t is_adjacent;
} osm_router_tbl_data_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_fmap.  MUST BE FIRST MEMBER!
*
*	subnet_prefix
*		subnet pefix in which router has a "leg"
*
*	pkey
*		pkey on the port of the router in that subnet
*
*	master_sm_lid
*		lid of master sm in that subnet
*
*	is_adjacent
*		An indication if this is adjacent router (if not then this is
*		a next hop router)
*
* SEE ALSO
*********/

/****s* OpenSM: Router/osm_router_t
* NAME
*	osm_router_t
*
* DESCRIPTION
*	Router structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_router {
	cl_map_item_t map_item;
	osm_port_t   *p_port;
	ib_router_info_t router_info;
	osm_router_tbl_t adj_router_tbl;
	osm_router_tbl_t next_hop_router_tbl;
	cl_fmap_t subnet_prefix_tbl;
	uint8_t adj_router_lid_info_top;//adjacent_subnets_router_lid_info_table_top;
	ib_rtr_router_lid_tbl_block_t *adj_router_lid_info;
	uint16_t global_flid_start;
	uint16_t global_flid_end;
	uint16_t local_flid_start;
	uint16_t local_flid_end;
	ib_rtr_router_lid_tbl_block_t *router_lid_tbl;
	ib_rtr_router_lid_tbl_block_t *router_lid_tbl_locals;
	ib_rtr_router_lid_tbl_block_t *new_router_lid_tbl;
	boolean_t is_router_info_set_error;
	uint16_t *enabled_flids;
	uint16_t num_ar_group_to_lid_entries;
	ib_rtr_ar_group_to_lid_tbl_block_t *subn_ar_group_to_lid;
} osm_router_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	p_port
*		Pointer to the Port object for this router.
*
*	router_info
*		router info for this router.
*
*	adj_router_tbl
*		adjacent router table blocks for this router.
*
*	next_hop_router_tbl
*		next hop router table blocks for this router.
*
*	subnet_prefix_tbl
*		Subnet prefixes and their pkeys of this router.
*		The key of the table is a pair <subnet prefix, pkey>.
*
*	global_flid_start
*		The start of global flid range, that is configured in the subnet
*
*	global_flid_end
*		The end of global flid range, that is configured in the subnet
*
*	local_flid_start
*		The start of local flid range, that is configured in the subnet
*
*	local_flid_end
*		The end of local flid range, that is configured in the subnet
*
*	router_lid_tbl
*		router lid table blocks as read from subnet for this router
*
*	router_lid_tbl_locals
*		router lid table blocks as read from subnet for this router
*
*	new_router_lid_tbl
*		router lid table blocks as calculated with respect to setting local flids
*
*	is_router_info_set_error
*		Indicates if router info set failed.
*
*	enabled_flids
*		Array of foreign FLIDs enabled on this router
*
*	num_ar_group_to_lid_entries
*		Number of AR group to router LID entries on this router.
*
*	subn_ar_group_to_lid
*		Configured AR group to router LID table on this router.
*
* SEE ALSO
*	Router object
*********/

/****f* OpenSM: Router/osm_router_delete
* NAME
*	osm_router_delete
*
* DESCRIPTION
*	Destroys and deallocates the object.
*
* SYNOPSIS
*/
void osm_router_delete(IN OUT osm_router_t ** pp_rtr);
/*
* PARAMETERS
*	p_rtr
*		[in][out] Pointer to a pointer to the object to destroy.
*		The pointer will be set to NULL on return.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*	Router object, osm_router_new
*********/

/****f* OpenSM: Router/osm_router_new
* NAME
*	osm_router_new
*
* DESCRIPTION
*	The osm_router_new function initializes a Router object for use.
*
* SYNOPSIS
*/
osm_router_t *osm_router_new(IN struct osm_sm * sm, IN osm_port_t * p_port);
/*
* PARAMETERS
* 	sm
* 		[in] Pointer to the sm object
*
*	p_port
*		[in] Pointer to the port object of this router
*
* RETURN VALUES
*	Pointer to the new initialized router object.
*	If could not initialize, return NULL.
*
* NOTES
*
* SEE ALSO
*	Router object, osm_router_new
*********/

/****f* OpenSM: Router/osm_router_get_port_ptr
* NAME
*	osm_router_get_port_ptr
*
* DESCRIPTION
*	Returns a pointer to the Port object for this router.
*
* SYNOPSIS
*/
static inline osm_port_t *osm_router_get_port_ptr(IN const osm_router_t * p_rtr)
{
	return p_rtr->p_port;
}

/*
* PARAMETERS
*	p_rtr
*		[in] Pointer to an osm_router_t object.
*
* RETURN VALUES
*	Returns a pointer to the Port object for this router.
*
* NOTES
*
* SEE ALSO
*	Router object
*********/

/****f* OpenSM: Router/osm_router_get_node_ptr
* NAME
*	osm_router_get_node_ptr
*
* DESCRIPTION
*	Returns a pointer to the Node object for this router.
*
* SYNOPSIS
*/
static inline osm_node_t *osm_router_get_node_ptr(IN const osm_router_t * p_rtr)
{
	return p_rtr->p_port->p_node;
}

/*
* PARAMETERS
*	p_rtr
*		[in] Pointer to an osm_router_t object.
*
* RETURN VALUES
*	Returns a pointer to the Node object for this router.
*
* NOTES
*
* SEE ALSO
*	Router object
*********/

/****f* OpenSM: Switch/osm_router_set_router_info
* NAME
*  osm_router_set_router_info
*
* DESCRIPTION
*  Updates the router info attribute of this router.
*
* SYNOPSIS
*/
static inline void osm_router_set_router_info(IN osm_router_t * p_rtr,
					      IN const ib_router_info_t * p_ri)
{
	OSM_ASSERT(p_rtr);
	OSM_ASSERT(p_ri);
	p_rtr->router_info = *p_ri;
}
/*
* PARAMETERS
*	p_rtr
*		[in] Pointer to an osm_router_t object.
*
*	p_ri
*		[in] Pointer to router info.
*
* RETURN VALUES
*
* NOTES
*
* SEE ALSO
*	Router object
*********/

/****f* OpenSM: Switch/osm_router_find_pkey
* NAME
*  osm_router_find_pkey
*
* DESCRIPTION
*  Finds pkey that matches the remote gid subnet in the routing table
*  and is configured both on source and on the subnet matching router.
*
* SYNOPSIS
*/
ib_api_status_t osm_router_find_pkey(
	IN osm_subn_t *p_subn,
	IN const osm_alias_guid_t *p_router_alias_guid,
	IN const ib_gid_t *p_remote_gid,
	IN const osm_pkey_tbl_t *pkey_tbl_src,
	IN const osm_pkey_tbl_t *pkey_tbl_dest,
	OUT ib_net16_t *pkey);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to opensm subnet object.
*
*	p_router_alias_guid
*		[in] Pointer to router alias guid object.
*
*	p_remote_gid
*		[in] pointer to gid in the remote subnet, i.e the destination
*
* 	pkey_tbl_src
* 		[in] the pkey table of the source in the path record
*
* 	pkey_tbl_dest
* 		[i] the pkey table of the inspected router
*
*	pkey
*		[out] pointer to pkey to return
*
* RETURN VALUES
*	Returns success or not found, if the given router is not in the the db.
* NOTES
*
* SEE ALSO
*	Router object
*********/

/****f* OpenSM: osm_adj_router_lid_info_set
* NAME
*  osm_adj_router_lid_info_set
*
* DESCRIPTION
*  Set the adjacent subnet router lid info block provided in the Router object.
*
* SYNOPSIS
*/
void osm_adj_router_lid_info_set(IN osm_router_t *p_router,
				 IN uint32_t block,
				 IN ib_rtr_adj_subnets_lid_info_block_t * p_block);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object
*
*  block
*     [in] The block number to set
*
*  p_block
*     [in] The IB Router adjacent subnet router lid info block to copy to the object
*
* RETURN VALUES
*
* NOTES
*
*********/

/****f* OpenSM: osm_router_lid_table_set
* NAME
*  osm_router_lid_table_set
*
* DESCRIPTION
*  Set the router lid table block provided in the Router object.
*
* SYNOPSIS
*/
void osm_router_lid_table_set(IN osm_router_t *p_router,
			      IN uint32_t block,
			      IN ib_rtr_router_lid_tbl_block_t * p_block);
/*
*  p_router_tbl
*     [in] Pointer to osm_router_tbl_t object
*
*  block
*     [in] The block number to set
*
*  p_block
*     [in] The IB Router router lid table block to copy to the object
*
* RETURN VALUES
*
* NOTES
*
*********/


#define RTR_TBL_NUM_BLOCKS(top, num_elements_in_block) \
	(((top) + ((num_elements_in_block) - 1)) / (num_elements_in_block))

#define RTR_TBL_ENTRY_IDX_TO_BLOCK(entry, num_elements_in_block) \
	((entry) / (num_elements_in_block))

#define RTR_TBL_NUM_BLOCKS_RANGE(start_idx, end_idx, num_elements_in_block) \
	(RTR_TBL_ENTRY_IDX_TO_BLOCK((end_idx), (num_elements_in_block)) - \
	 RTR_TBL_ENTRY_IDX_TO_BLOCK((start_idx), (num_elements_in_block)) + 1)

#define RTR_RTR_LID_TBL_BLOCK_IDX_IN_RECORD(lid) \
	((IB_ROUTER_LID_TBL_BLOCK_RECORD_LIDS_NUM - 1) - \
	 ((lid) % IB_ROUTER_LID_TBL_BLOCK_RECORD_LIDS_NUM))

#define RTR_RTR_LID_TBL_BLOCK_RECORD_IDX(lid) \
	(((lid) % IB_ROUTER_LID_TBL_BLOCK_LIDS_NUM) / IB_ROUTER_LID_TBL_BLOCK_RECORD_LIDS_NUM)

#define RTR_RTR_LID_TBL_BLOCK_LAST_RECORD_IDX RTR_RTR_LID_TBL_BLOCK_RECORD_IDX(\
		IB_ROUTER_LID_TBL_BLOCK_LIDS_NUM - 1)

/*
 * This is currently the number of subnets supported.
 */
#define MAX_ROUTER_SUPPORTED_SUBNETS		8

typedef struct osm_sw_rtrs_list {
	cl_map_item_t map_item;
	ib_net64_t sw_guid;
	struct osm_switch *p_sw;
	cl_list_t rtrs_list;
} osm_sw_rtrs_list_t;

typedef struct osm_flid_rtrs_list {
	cl_fmap_item_t map_item;
	uint16_t flid;
	cl_list_t rtrs_list;
} osm_flid_rtrs_list_t;

void osm_router_set_flid(IN osm_subn_t *p_subn, IN OUT osm_router_t *p_router, uint16_t flid);

uint32_t osm_router_lid_tbl_offset_to_block(IN osm_router_t *p_router, IN uint32_t offset);

typedef enum osm_rtr_flid_routed_type {
	OSM_RTR_FLID_NONE = 0,
	OSM_RTR_FLID_ROUTED = 1,
	OSM_RTR_FLID_STATIC = 2
} osm_rtr_flid_routed_type;

typedef enum osm_rtr_lid_tbl_type_enum {
	OSM_RTR_LID_TBL_SUBNET = 0,
	OSM_RTR_LID_TBL_LOCAL_CALCULATION,
	OSM_RTR_LID_TBL_SUBNET_LOCALS_ONLY
} osm_rtr_lid_tbl_type_enum;

typedef enum osm_rtr_lid_tbl_action_enum {
	OSM_RTR_LID_TBL_GET = 0,
	OSM_RTR_LID_TBL_SET_ENABLE,
	OSM_RTR_LID_TBL_SET_DISABLE
} osm_rtr_lid_tbl_action_enum;

uint32_t osm_router_rtr_lid_tbl_get_set(IN OUT osm_router_t *p_router,
					IN uint16_t flid,
					IN osm_rtr_lid_tbl_type_enum rtr_lid_tbl_type,
					IN osm_rtr_lid_tbl_action_enum rtr_rtr_lid_tbl_action);

static inline
uint32_t osm_router_lid_tbl_block_to_global_offset(osm_router_t *p_router, uint32_t block) {
	return block - RTR_TBL_ENTRY_IDX_TO_BLOCK(p_router->global_flid_start,
						  IB_ROUTER_LID_TBL_BLOCK_LIDS_NUM);
}

static inline
uint32_t osm_router_lid_tbl_block_to_local_offset(osm_router_t *p_router, uint32_t block) {
	return block - RTR_TBL_ENTRY_IDX_TO_BLOCK(p_router->local_flid_start,
						  IB_ROUTER_LID_TBL_BLOCK_LIDS_NUM);
}

boolean_t osm_router_is_router_in_the_list(osm_router_t *p_router, cl_list_t *p_rtrs_list);

/*
 * This function is called after Router Info SET.
 * If the SET failed, then router is not flid enabled.
 */
static inline
boolean_t osm_router_is_router_flid_enabled(IN osm_subn_t *p_subn, IN osm_router_t *p_router)
{
	return
	   (ib_router_info_get_global_router_lid_start(&p_router->router_info) == p_subn->opt.global_flid_start &&
	    ib_router_info_get_global_router_lid_end(&p_router->router_info) == p_subn->opt.global_flid_end &&
	    ib_router_info_get_local_router_lid_start(&p_router->router_info) == p_subn->opt.local_flid_start &&
	    ib_router_info_get_local_router_lid_end(&p_router->router_info) == p_subn->opt.local_flid_end);
}

/* This function returns the amount of FLIDs foreign to this local subnet */
static inline int osm_router_get_foreign_flid_count(IN osm_subn_t *p_subn)
{
	return p_subn->opt.global_flid_end - p_subn->opt.global_flid_start -
	       p_subn->opt.local_flid_end + p_subn->opt.local_flid_start + 1;
}

/* Release router AR group to LID table */
void osm_router_ar_group_to_lid_tbl_clear(IN osm_router_t *p_router);

/* Release and realloate AR group to LID table */
void osm_router_ar_group_to_lid_tbl_reinit(IN osm_subn_t *p_subn, IN osm_router_t *p_router, IN uint16_t num_entries);

END_C_DECLS
