/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
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
 *      topologies manager
 */
#include <opensm/osm_port_group.h>
#include <opensm/osm_routing_chain.h>

#ifndef OSM_TOPO_MGR_H
#define OSM_TOPO_MGR_H
#define MAX_TOPOLOGIES_NUMBER 50
#define MAIN_TOPO_ID 0
#define NO_TOPO_ID 99999
#define NON_ITR_TOPO_ID (MAX_TOPOLOGIES_NUMBER + 1)
#define TOPO_ARRAY_SIZE (NON_ITR_TOPO_ID + 1)
#define TOPO_ARRAY_LEN (8 * TOPO_ARRAY_SIZE)

typedef enum _osm_topo_arr_type {
	PRIMARY_ARRAY = 0,
	PREV_ARRAY
} osm_topo_arr_type_t;

typedef struct osm_topology {
	cl_map_item_t map_item;
	uint64_t id;
	osm_port_group_t *sw_grp;
	osm_port_group_t *hca_grp;
	struct osm_routing_chain *routing_engine;
	struct osm_subn *p_subn;
	cl_qmap_t *removed_hcas;
} osm_topology_t;

typedef struct osm_topo_mgr {
	osm_topology_t* topologies[TOPO_ARRAY_SIZE];
	osm_topology_t* prev_topologies[TOPO_ARRAY_SIZE];
	struct osm_opensm *p_osm;
	boolean_t initialized;
	ib_api_status_t creation_result;
} osm_topo_mgr_t;

/****f* OpenSM: Topologies mgr/osm_topo_mgr_create
* NAME
*	osm_topo_mgr_create
*
* DESCRIPTION
*	Create the topologies manager
*
* SYNOPSIS
*/
ib_api_status_t osm_topo_mgr_create(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm.
*
* RETURN VALUE
*	IB_SUCCESS on success
*	IB_ERROR on failure
*/

/****f* OpenSM: Topologies mgr/osm_topo_mgr_destroy
* NAME
*	osm_topo_mgr_destroy
*
* DESCRIPTION
*	Destroy the topologies manager.
*
* SYNOPSIS
*/
void osm_topo_mgr_destroy(IN osm_topo_mgr_t *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to topologies manager.
*/

/* API toward parser */

/****f* OpenSM: Topologies mgr/osm_topo_mgr_add_topo
* NAME
*	osm_topo_mgr_add_topo
*
* DESCRIPTION
*	Add topology to the topologies mgr
*
* SYNOPSIS
*/
ib_api_status_t osm_topo_mgr_add_topo(IN struct osm_opensm *p_osm, IN uint64_t id,
				      IN osm_port_group_t *sw_grp,
				      IN osm_port_group_t *hca_grp);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm.
*	id
*		id for new topology
*	sw_grp
*		pointer to switches ports group
*	hca_grp
*		pointer to hca's ports group
*
* RETURN VALUE
*	IB_SUCCESS on success
*	IB_ERROR on failure
*/

/****f* OpenSM: Topologies mgr/osm_topo_mgr_get_topo
* NAME
*	osm_topo_mgr_get_topo
*
* DESCRIPTION
*	Get topology by id
*
* SYNOPSIS
*/
osm_topology_t *osm_topo_mgr_get_topo(IN struct osm_opensm *p_osm, IN uint64_t id);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm.
*	id
*		id of topology to get
*
* RETURN VALUE
*	pointer to topology or NULL if not found
*/

/****f* OpenSM: Topologies mgr/osm_topologies_count
* NAME
*	osm_topologies_count
*
* DESCRIPTION
*	Get topologies count
*
* SYNOPSIS
*/
uint64_t osm_topologies_count(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm.
*
* RETURN VALUE
*	number of topologies in topology mgr
*/

/****f* OpenSM: Topologies mgr/clear_all_removed_hcas_maps
* NAME
*	clear_all_removed_hcas_maps
*
* DESCRIPTION
*	Clear all removed hca maps in all topologies
*
* SYNOPSIS
*/
void clear_all_removed_hcas_maps(IN struct osm_opensm * p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to opensm object.
*
* RETURN VALUE
*	None
*/

/****f* OpenSM: Topologies mgr/osm_topo_is_border_sw
* NAME
*	osm_topo_is_border_sw
*
* DESCRIPTION
*	Check whether provided switch is border switch
*
* SYNOPSIS
*
*/

boolean_t osm_topo_is_border_sw(IN osm_topology_t * p_topo,
                                IN ib_net64_t sw_guid);
/*
* PARAMETERS
*	p_topo
*		Pointer to the topology object
*	sw_guid
*		GUID of the switch
* RETURN VALUE
*	TRUE - switch is border switch in the topology
*	FALSE - otherwise
*/

/****f* OpenSM: Topologies mgr/osm_get_port_from_itr_topo
* NAME osm_get_port_from_itr_topo
*
* DESCRIPTION
*	Return port object from ITR topology the port belongs to
*
* SYNOPSIS
*
*/
struct osm_port *osm_get_port_from_itr_topo(IN struct osm_port *p_port,
					    OUT osm_topology_t **p_topo);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to the port object in MAIN subnet
*	p_topo
*		[out] Pointer to the found topology with ITR routing engine
* RETURN VALUE
*	Pointer to the port object in the subnet
*	NULL - no ITR subnet is found or port object is not found in the ITR subnet
*/


/****f* OpenSM: Topologies mgr/osm_topo_mgr_create_completer_topo
* NAME osm_topo_mgr_create_completer_topo
*
* DESCRIPTION
*	Creates completer topology of all physps which are not included in any
*	topology
*
* SYNOPSIS
*
*/
int osm_topo_mgr_create_completer_topo(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to opensm object.
*
* RETURN VALUE
*	0 = Success
*	1 = Failure
*/

/****f* OpenSM: Topologies mgr/osm_topo_is_engine_itr
* NAME osm_topo_is_engine_itr
*
* DESCRIPTION
*	Checks whether used routing engine in the topology is ITR engine
*
* SYNOPSIS
*
*/
boolean_t osm_topo_is_engine_itr(osm_topology_t *p_topo);
/*
* PARAMETERS
*	p_topo
*		Pointer to the topology object

* RETURN VALUE
*	TRUE - used routing engine is ITR engine
*	FALSE - otherwise
*/


/****f* OpenSM: Topologies mgr/osm_topo_mgr_create_non_itr_topo
* NAME osm_topo_mgr_create_non_itr_topo
*
* DESCRIPTION
*	Creata completer topology of all physps which are not included in itr
*	topology
*
* SYNOPSIS
*
*/
int osm_topo_mgr_create_non_itr_topo(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to opensm object.
*
* RETURN VALUE
*	0 = Success
*	1 = Failure
*/

/****f* OpenSM: Topologies mgr/osm_topo_physp_on_main
* NAME osm_topo_physp_on_main
*
* DESCRIPTION
*	Check if physical port is connected to physp on main topology
*
* SYNOPSIS
*
*/
boolean_t osm_topo_physp_on_main(struct osm_physp *p_physp);
/*
* PARAMETERS
*	p_physp
*		Pointer to physical port object.
*
* RETURN VALUE
*	0 = Not on Main
*	1 = On Main
*/

/****f* OpenSM: Topologies mgr/osm_topo_mgr_revert_prev
* NAME osm_topo_mgr_revert_prev
*
* DESCRIPTION
*	Revert to prev topologies
*
* SYNOPSIS
*
*/
void osm_topo_mgr_revert_prev(IN struct osm_opensm * p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to opensm object.
*/
#endif
