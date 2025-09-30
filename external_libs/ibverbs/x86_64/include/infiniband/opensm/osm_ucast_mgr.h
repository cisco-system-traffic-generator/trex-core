/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2009 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_ucast_mgr_t.
 *	This object represents the Unicast Manager object.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <complib/cl_passivelock.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_log.h>
#include <opensm/osm_ucast_cache.h>
#include <opensm/osm_graph.h>
#include <opensm/osm_routing_v2.h>
#include <complib/cl_mpthreadpool.h>
#include <stdlib.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/* possible values are 32, 64, 128, 256. Default is 128 */
#define SCATTER_BUF_SIZE 128

#define UCAST_MGR_MAX_GROUP_ID		0xFFFE
#define UCAST_MGR_INVALID_GROUP_ID	0xFFFF
#define UCAST_MGR_MAX_PLFT		3
#define UCAST_MGR_MAX_DFP2_UNIQUE_PLFT	2
#define UCAST_MGR_MAX_PORT_NUM		254

#define ucast_start_plane(P_MGR)	((P_MGR)->p_subn->is_planarized ? 1 : 0)
#define ucast_end_plane(P_MGR)		((P_MGR)->p_subn->is_planarized ? IB_MAX_NUM_OF_PLANES_XDR : 0)

#define foreach_ucast_plane(P_MGR, PLANE)				\
	for (PLANE = ucast_start_plane(P_MGR);				\
	     PLANE <= ucast_end_plane(P_MGR);				\
	     PLANE++)

/****h* OpenSM/Unicast Manager
* NAME
*	Unicast Manager
*
* DESCRIPTION
*	The Unicast Manager object encapsulates the information
*	needed to control unicast LID forwarding on the subnet.
*
*	The Unicast Manager object is thread safe.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/
struct osm_sm;
/****s* OpenSM: Unicast Manager/osm_ucast_mgr_t
* NAME
*	osm_ucast_mgr_t
*
* DESCRIPTION
*	Unicast Manager structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_ucast_mgr {
	char *scatter_statebufs;
	struct random_data *scatter_bufs;
	struct osm_sm *sm;
	osm_subn_t *p_subn;
	osm_log_t *p_log;
	cl_plock_t *p_lock;
	cl_mp_thread_pool_t thread_pool;
	uint16_t max_lid;
	cl_qlist_t port_order_list;
	cl_qlist_t *port_order_lists;
	cl_qlist_t *sw_lists;
	boolean_t is_dor;
	boolean_t some_hop_count_set;
	cl_qmap_t cache_sw_tbl;
	cl_qmap_t cache_vlid_tbl;
	boolean_t cache_valid;
	boolean_t vlid_cache_valid;
	boolean_t variables_invalidate_cache;
	boolean_t variables_reinit_thread_pool;
	boolean_t rebalancing_required;
	struct timeval rerouting_timestamp;
	boolean_t ar_configured;
	int guid_routing_order_count;
	/* Adaptive routig group IDs */
	osm_db_domain_t *p_g2groupid;
	uint64_t used_group_ids[UCAST_MGR_MAX_GROUP_ID + 1];
	uint16_t lid_to_group_id[IB_LID_UCAST_END_HO + 1][UCAST_MGR_MAX_PLFT];
	uint16_t port_num_to_group_id[UCAST_MGR_MAX_PORT_NUM + 1];
	uint16_t free_global_group_id_idx;
	uint16_t free_local_group_id_idx;
	uint16_t num_reserved_local_ar_groups;
	osm_db_domain_t *p_flid2groupid;
	osm_db_domain_t *p_alid2groupid;
	osm_db_domain_t *p_portnum2groupid;
	void *re_context;
	unsigned num_roots;
	uint8_t num_planes;
	osm_graph_t *graphs[IB_MAX_NUM_OF_PLANES_XDR + 1];
	osm_route_context_t *v2_context[IB_MAX_NUM_OF_PLANES_XDR + 1];
} osm_ucast_mgr_t;
/*
* FIELDS
*	sm
*		Pointer to the SM object.
*
*	p_subn
*		Pointer to the Subnet object for this subnet.
*
*	p_log
*		Pointer to the log object.
*
*	p_lock
*		Pointer to the serializing lock.
*
*	max_lid
*		Max LID of all the switches in the subnet.
*
*	port_order_list
*		List of ports ordered for routing.
*
*	is_dor
*		Dimension Order Routing (DOR) will be done
*
*	some_hop_count_set
*		Initialized to FALSE at the beginning of each the min hop
*		tables calculation iteration cycle, set to TRUE to indicate
*		that some hop count changes were done.
*
*	cache_sw_tbl
*		Cached switches table.
*
*	cache_vlid_tbl
*		Cached vLIDs table
*
*	cache_valid
*		TRUE if the unicast cache is valid.
*
*	vlid_cache_valid
*		TRUE if the vLID cache is valid
*
*	variables_invalidate_cache
*		TRUE if one of the variables from config file, relevant to
*		the routing calculations, has changed.
*
*	variables_reinit_thread_pool
*		TRUE if one of the variables from config file, relevant to
*		the ucast_mgr_t thread pool, has changed.
*
*	rebalancing_required
*		TRUE if current routing is unbalanced
*
*	rerouting_timestamp
*		Last time rerouting happen during heavy sweep
*
*	ar_configured
*		Indicates that Adaptive Routing is configured on at least one
*		switch in the subnet.
*		Note: SHIELD does not affect this indicator.
*
*	guid_routing_order_count
*		Number of nodes in guid_routing_order_file
*
*	p_g2groupid
*		GUID to AR group ID persistent database.
*
* 	used_group_ids
*		 An array of used adaptive routing group ids.
*		 keeps track of used and free groups ids.
*		 Should only be used as a boolean array.
*
*	port_num_to_group_id
*		A helper array that translates between port_num to group id.
*		See p_portnum2groupid.
*
*	free_local_group_id_idx
*		Iterator for searching unused adaptive routing group id for
*		local swithces..
*
*	free_global_group_id_idx
*		Iterator for searching unused adaptive routing group id for
*		remote FLIDs and for planarized HCAs.
*
*	 num_reserved_local_ar_groups
*		Number of AR groups reserved for local AR groups.
*		The value of this field is used to control AR group allocations.
*
*	p_flid2groupid
*		Switch GUID, that was assigned with an FLID and needs a group id.
*
*	p_alid2groupid
*		Mapping from ALID, which is assigned to Node GPU, to a group id.
*
*	p_portnum2groupid
*		A persistent database linking port number to AR group ID.
*		Used only for routing from a leaf that is a prism switch to planarized hosts
*		connected to it.
*		Between multiple physical ports in the same APort,
*		the lowest port number is chosen as the representative, and is used in the database.
*		e.g. if the APort to the host includes ports 4-7, the AR group ID stored in the db
*		for port num 4 should be used when routing from the prism switch.
*		NOTE: Multiple hosts (connected to different prism switches) can use the same AR
*		group ID, if they have the same representative port number.
*
* SEE ALSO
*	Unicast Manager object
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_construct
* NAME
*	osm_ucast_mgr_construct
*
* DESCRIPTION
*	This function constructs a Unicast Manager object.
*
* SYNOPSIS
*/
void osm_ucast_mgr_construct(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to a Unicast Manager object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows osm_ucast_mgr_destroy
*
*	Calling osm_ucast_mgr_construct is a prerequisite to calling any other
*	method except osm_ucast_mgr_init.
*
* SEE ALSO
*	Unicast Manager object, osm_ucast_mgr_init,
*	osm_ucast_mgr_destroy
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_destroy
* NAME
*	osm_ucast_mgr_destroy
*
* DESCRIPTION
*	The osm_ucast_mgr_destroy function destroys the object, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_ucast_mgr_destroy(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified
*	Unicast Manager object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to
*	osm_ucast_mgr_construct or osm_ucast_mgr_init.
*
* SEE ALSO
*	Unicast Manager object, osm_ucast_mgr_construct,
*	osm_ucast_mgr_init
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_init
* NAME
*	osm_ucast_mgr_init
*
* DESCRIPTION
*	The osm_ucast_mgr_init function initializes a
*	Unicast Manager object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_ucast_mgr_init(IN osm_ucast_mgr_t * p_mgr,
				   IN struct osm_sm * sm);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object to initialize.
*
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
*	IB_SUCCESS if the Unicast Manager object was initialized
*	successfully.
*
* NOTES
*	Allows calling other Unicast Manager methods.
*
* SEE ALSO
*	Unicast Manager object, osm_ucast_mgr_construct,
*	osm_ucast_mgr_destroy
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_set_fwd_tables
* NAME
*	osm_ucast_mgr_set_fwd_tables
*
* DESCRIPTION
*	Setup forwarding table for the switch (from prepared new_lft).
*
* SYNOPSIS
*/
void osm_ucast_mgr_set_fwd_tables(IN osm_ucast_mgr_t * p_mgr, IN osm_node_filter_t filter);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
*	filter
*		[in] Filter to select which switches to setup.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/ucast_mgr_setup_all_switches
* NAME
*	ucast_mgr_setup_all_switches
*
* DESCRIPTION
*
* SYNOPSIS
*/
int ucast_mgr_setup_all_switches(osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] pointer to subnet object
*
* RETURN VALUE
*	Returns zero on success and negative value on failure.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/alloc_ports_priv
* NAME
*	alloc_ports_priv
*
* DESCRIPTION
*
* SYNOPSIS
*/
int alloc_ports_priv(osm_ucast_mgr_t * mgr, cl_qlist_t *port_list);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
*	port_list
*		[in] List of ports to alloc priv objects in.
*		     init_ports_priv should be called after
*		     alloc_ports_priv to reset allocated priv objects
*
* RETURN VALUE
*	Returns zero on success and negative value on failure.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/init_ports_priv
* NAME
*	init_ports_priv
*
* DESCRIPTION
*
* SYNOPSIS
*/
void init_ports_priv(osm_ucast_mgr_t * mgr, cl_qlist_t *port_list);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
*	port_list
*		[in] List of ports to initialize priv objects in
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/free_ports_priv
* NAME
*	free_ports_priv
*
* DESCRIPTION
*
* SYNOPSIS
*/
void free_ports_priv(osm_ucast_mgr_t * mgr, uint32_t i);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*	i
*		[in] Index of compute thread. It should be zero
*			for non-parallel routing engines
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_build_lid_matrices
* NAME
*	osm_ucast_mgr_build_lid_matrices
*
* DESCRIPTION
*	Build switches's lid matrices.
*
* SYNOPSIS
*/
int osm_ucast_mgr_build_lid_matrices(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
* NOTES
*	This function processes the subnet, configuring switches'
*	min hops tables (aka lid matrices).
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_build_lft_tables
* NAME
*	osm_ucast_mgr_build_lft_tables
*
* DESCRIPTION
*	Build switches's lft tables.
*
* SYNOPSIS
*/
int osm_ucast_mgr_build_lft_tables(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
* NOTES
*	This function processes minhop tables, configuring switches
*	lft tables.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_create_graphs
* NAME
*	osm_ucast_mgr_create_graphs
*
* DESCRIPTION
*	Build the routing graphs for all planes.
*
* SYNOPSIS
*/
int osm_ucast_mgr_create_graphs(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
* NOTES
*	This function creates the graphs which are needed
*	by the routing engines.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_destroy_graphs
* NAME
*	osm_ucast_mgr_destroy_graphs
*
* DESCRIPTION
*	Destroy the routing graphs for all planes.
*
* SYNOPSIS
*/
void osm_ucast_mgr_destroy_graphs(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
* NOTES
*	This function destroys the graphs.
*
* SEE ALSO
*	Unicast Manager
*********/

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_process
* NAME
*	osm_ucast_mgr_process
*
* DESCRIPTION
*	Process and configure the subnet's unicast forwarding tables.
*
* SYNOPSIS
*/
int osm_ucast_mgr_process(IN osm_ucast_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
* RETURN VALUES
*	Returns zero on success and negative value on failure.
*
* NOTES
*	This function processes the subnet, configuring switch
*	unicast forwarding tables.
*
* SEE ALSO
*	Unicast Manager, Node Info Response Controller
*********/

int osm_ucast_add_guid_to_order_list(void *, uint64_t, char *);
void osm_ucast_clear_prof_ignore_flag(cl_map_item_t * const, void *);
int osm_ucast_mark_ignored_port(void *, uint64_t, char *);
void osm_ucast_add_port_to_order_list(cl_map_item_t * const, void *);
void osm_ucast_process_tbl(IN cl_map_item_t * const p_map_item, IN void *context);
int alloc_random_bufs(osm_ucast_mgr_t * p_mgr, int num_of_bufs);
int ucast_dummy_build_lid_matrices(void *context);
int osm_ucast_mgr_calculate_missing_routes(IN OUT osm_ucast_mgr_t * p_mgr);
int osm_ucast_mgr_init_port_order_lists(IN osm_ucast_mgr_t * p_mgr,
					IN uint32_t number_of_thread);
void osm_ucast_mgr_release_port_order_lists(IN osm_ucast_mgr_t * p_mgr,
					    IN uint32_t number_of_thread);

void osm_ucast_mgr_reset_lfts_all_switches(osm_subn_t * p_subn);

void osm_ucast_mgr_update_groupid_maps(osm_ucast_mgr_t *p_mgr);
uint16_t osm_ucast_mgr_get_groupid(osm_ucast_mgr_t * p_mgr, uint16_t lid, uint8_t plft);
uint16_t osm_ucast_mgr_make_groupid(osm_ucast_mgr_t * p_mgr, uint16_t lid, uint8_t plft);
int osm_ucast_mgr_set_groupid(osm_ucast_mgr_t * p_mgr, uint16_t lid, uint16_t group_id, uint8_t plft);
int osm_ucast_mgr_remove_groupid(osm_ucast_mgr_t * p_mgr, uint16_t lid);

/****f* OpenSM: Unicast Manager/osm_ucast_mgr_make_port_groupid
* NAME
*	osm_ucast_mgr_make_port_num_groupid
*
* DESCRIPTION
*	Get AR group ID for this port number, or allocate a new one if one isn't allocated already
*
* SYNOPSIS
*/
uint16_t osm_ucast_mgr_make_port_num_groupid(IN osm_ucast_mgr_t * p_mgr, IN uint8_t port_num);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to an osm_ucast_mgr_t object.
*
*	port_num
*		[in] The lowest port number in an aport leading from a prism switch to a planarized
*		     HCA.
*
* RETURN VALUES
*	Returns the group ID on sucess, and UCAST_MGR_INVALID_GROUP_ID on failure.
*
* NOTES
*	This function is used to to get existing or allocate new AR group IDs for
*	planarized HCAs connected to prism switches.
*	To spare AR group IDs, these AR groups will be used only for routing from a leaf to
*	hosts connected to it, which allows them to be reused for different HCAs, as long as these
*	HCAs are connected to different leaf switches.
*	To allocate an AR group ID for a planarized HCA, we take the port number of the
*	lowest-numbered port on the APort from the switch to the host.
*	e.g. if the APort (on the switch side) to the host includes ports 4-7, the AR group ID
*	for this APort should be osm_ucast_mgr_make_port_num_groupid(p_mgr, 4).
*
* SEE ALSO
*	osm_ucast_mgr_t.p_portnum2groupid
*********/

void osm_ucast_mgr_group_id_db_store(IN osm_ucast_mgr_t * p_mgr);
void osm_ucast_mgr_thread_pool_init(IN osm_ucast_mgr_t * p_mgr);

void osm_ucast_mgr_mp_thread_pool_init(IN osm_ucast_mgr_t * p_mgr,
				       IN cl_mp_thread_pool_t * mp_thread_pool,
				       IN const char * name,
				       OUT uint32_t * number_of_threads);

int osm_ucast_mgr_routing_preprocess(IN osm_ucast_mgr_t *p_mgr);
int osm_ucast_mgr_routing_postprocess(IN osm_ucast_mgr_t *p_mgr);
void osm_ucast_mgr_post_routing(IN osm_ucast_mgr_t *p_mgr, IN osm_routing_engine_t *p_re, IN uint8_t plane);

osm_graph_t *osm_ucast_mgr_plane_to_graph(osm_ucast_mgr_t *p_mgr, uint8_t plane);

void osm_get_port_nums_used_in_prism_aports_to_hcas(IN osm_ucast_mgr_t * p_mgr,
                                                    OUT uint8_t * used_port_numbers);

int osm_ucast_mgr_route_v2(struct osm_routing_engine *r, osm_opensm_t * osm);

END_C_DECLS
