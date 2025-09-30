/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * 	Declaration of osm_virt_mgr_t.
 *	This object represents the Virtualization Manager object.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <complib/cl_passivelock.h>
#include <complib/cl_qlist.h>
#include <complib/cl_u64_range_vector.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_log.h>
#include <stdlib.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****h* OpenSM/Virtualization Manager
* NAME
*	Virtualization Manager
*
* DESCRIPTION
* 	The Virtualization Manager object encapsulate the data required
* 	to configure virtual ports in the subnet.
*
*	The Virtualization Manager object is thread safe.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Daniel Klein, Mellanox
*
*********/


/****d* OpenSM: Virtualization Manager/osm_virt_mgr_sweep_stage_enum
* NAME
*	osm_virt_mgr_sweep_stage_enum
*
* DESCRIPTION
*	Virtual ports processing stages during heavy sweep.
*	DISCOVERY - Discover virtual ports.
*	SETUP - Configure P_Keys and LIDs.
*	ACTIVATION - Configure QoS, router alias GUID and activate virtual port.
*
* SYNOPSIS
*/
typedef enum _osm_virt_mgr_sweep_stage {
	OSM_VIRT_MGR_SWEEP_STAGE_DISCOVERY = 0,
	OSM_VIRT_MGR_SWEEP_STAGE_SETUP,
	OSM_VIRT_MGR_SWEEP_STAGE_ACTIVATION,
} osm_virt_mgr_sweep_stage_enum;
/***********/

/****s* OpenSM: Virtualization Manager/osm_vnode_t
* NAME
*	osm_vnode_t
*
* DESCRIPTION
*	Object used to represent vnode (VHCA)
*
* SYNOPSIS
*/
typedef struct osm_vnode {
	cl_map_item_t map_item;
	ib_vnode_info_t vnode_info;
	ib_node_desc_t vnode_desc;
	cl_qmap_t vports;
	ib_net64_t node_guid;
	cl_qlist_t migration_ports;
} osm_vnode_t;

/****s* OpenSM: Virtualization Manager/osm_virt_mgr_partition_t
* NAME
*	osm_virt_mgr_partition_t
*
* DESCRIPTION
*	Object used to keep partiotion data in virt mgr
*
* SYNOPSIS
*/
typedef struct osm_virt_partition {
	cl_map_item_t map_item;
	ib_net16_t pkey;
	boolean_t full;
	boolean_t indx0;
} osm_virt_mgr_partition_t;

/****s* OpenSM: Virtualization Manager/osm_virt_mgr_partitions_t
* NAME
*	osm_virt_mgr_partitions_t
*
* DESCRIPTION
*	Object used to keep a list of partitions data in virt mgr
*
* SYNOPSIS
*/
typedef struct osm_virt_mgr_partitions {
	cl_map_item_t map_item;
	cl_qmap_t limited_prtns;
	cl_qmap_t full_prtns;
} osm_virt_mgr_partitions_t;
/*
* FIELDS
*	map_item
*		cl map item
*
*	limited_prtns
*		map of partitions with limited membership
*
*	full_prtns
*		map of partitions with full membership
*
* SEE ALSO
*	Virtualization Manager object
*********/

typedef struct osm_virt_mgr_prtn_ranges_ {
	cl_map_item_t map_item;
	ib_net16_t pkey;
	boolean_t indx0;
	cl_u64_range_vector_t limited_guid_ranges_vector;
	cl_u64_range_vector_t full_guid_ranges_vector;
} osm_virt_mgr_prtn_ranges_t;

/****s* OpenSM: Virtualization Manager/osm_virt_partitions_data_t
* NAME
*	osm_virt_partitions_data_t
*
* DESCRIPTION
*	Object used to keep all partitions related data in virt mgr
*
*	Whenever a vport is set to ACTIVE it should check this data in order
*	to know to which partitions this vport belongs
*
* SYNOPSIS
*/
typedef struct osm_virt_partitions_data {
	cl_qmap_t limited_prtns_for_all;
	cl_qmap_t full_prtns_for_all;
	cl_qmap_t prtns_for_guid_ranges;
	cl_qmap_t prtns_per_guid;
} osm_virt_partitions_data_t;
/*
* FIELDS
*	limited_prtns_for_all
*		map of partitions configured for all ports with limited
*		membership
*
*	full_prtns_for_all
*		map of partitions configured for all ports with full
*		membership
*
*	prtns_per_guid
*		map of osm_virt_mgr_partitions_t objects
*		Indexed per guids (guids which were parsed from partitions conf
*		file and not found in port_guid_tbl of the subnet)
*
* SEE ALSO
*	Virtualization Manager object
*********/

typedef struct osm_guid_list_item {
        cl_list_item_t list_item;
        ib_net64_t port_guid;
} osm_guid_list_item_t;

/****s* OpenSM: Virtualization Manager/osm_virt_mgr_t
* NAME
*	osm_virt_mgr_t
*
* DESCRIPTION
*	Virtualization Manager structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_virt_mgr {
	cl_qlist_t queue;
	boolean_t dump_required;
	osm_virt_partitions_data_t partitions_data;
	cl_qmap_t vnodes;
} osm_virt_mgr_t;

/*
* FIELDS
*	queue
*		Queue of ports that need to be processed by Virtualization
*		Manager.
*
*	dump_required
*		Indication if virtualization data changed since last dump
*
*	partitions_data
*		All data relevant for vports partitions
*
*	vnodes
*		Map of vnodes indexed by their guid
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_init
* NAME
*	osm_virt_mgr_init
*
* DESCRIPTION
*	Initializes Virtualization Manger object.
*
* SYNOPSIS
*/
ib_api_status_t osm_virt_mgr_init(IN osm_virt_mgr_t * p_mgr,
				  IN struct osm_sm * sm);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to Virtualization Manger object.
*
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
*	IB_SUCCESS if the Virtualization Manager object was initialized successfully.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_destroy
* NAME
*	osm_virt_mgr_destroy
*
* DESCRIPTION
*	Cleans resources that were allocated for Virtualization Manger object.
*
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_virt_mgr_init
*
* SYNOPSIS
*/
void osm_virt_mgr_destroy(IN osm_virt_mgr_t * p_mgr, IN struct osm_sm * sm);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to Virtualization Manger object.
*
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
* 	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_process
* NAME
*	osm_virt_mgr_process
*
* DESCRIPTION
*	Process and configure the subnet's virtual ports.
*
* SYNOPSIS
*/
void osm_virt_mgr_process(IN struct osm_sm * sm, IN boolean_t process_all_ports);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	process_all_ports
*		[in] If TRUE virt_mgr should process all ports with virt enabled,
*		     else process only the ones in the queue.
*
* RETURN VALUES
* 	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_enqueue_port
* NAME
*	osm_virt_mgr_enqueue_port
*
* DESCRIPTION
*	Add port to queue of ports to be processed by Virtualization Manager.
*
* SYNOPSIS
*/
void osm_virt_mgr_enqueue_port(IN struct osm_sm * sm, IN osm_port_t * p_port);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
* 	p_port
* 		[in] Pointer to osm_port_t object to add to the queue.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_dequeue_port
* NAME
*	osm_virt_mgr_dequeue_port
*
* DESCRIPTION
*	Remove port to queue of ports to be processed by Virtualization Manager.
*
* SYNOPSIS
*/
void osm_virt_mgr_dequeue_port(IN struct osm_sm * sm, IN osm_port_t * p_port);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
* 	p_port
* 		[in] Pointer to osm_port_t object to remove from the queue.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_empty_queue
* NAME
*	osm_virt_mgr_empty_queue
*
* DESCRIPTION
*	Empty the Virtualization Manager's queue of the VPorts.
*
* SYNOPSIS
*/
void osm_virt_mgr_empty_queue(IN osm_virt_mgr_t * p_mgr);
/*
* PARAMETERS
* 	p_mgr
* 		[in] Pointer to osm_virt_mgr_t object to clear queue.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_is_dump_required
* NAME
*	osm_virt_mgr_is_dump_required
*
* DESCRIPTION
*	Return indication if virtualization data dump is required (change
*	occurred since last dump).
*
* SYNOPSIS
*/
static inline boolean_t osm_virt_mgr_is_dump_required(IN osm_virt_mgr_t *p_mgr)
{
	return p_mgr->dump_required;
}
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to osm_virt_mgr_t object.
*
* RETURN VALUES
*	Return indication if virtualization data dump is required.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_set_dump_required
* NAME
*	osm_virt_mgr_set_dump_required
*
* DESCRIPTION
*	Set indication if virtualization data dump is required.
*
* SYNOPSIS
*/
static inline void osm_virt_mgr_set_dump_required(IN osm_virt_mgr_t *p_mgr,
						  IN boolean_t value)
{
	p_mgr->dump_required = value;
}
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to osm_virt_mgr_t object.
*
* RETURN VALUES
*	Does not return a value
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_vport_state_change_check
* NAME
*	osm_virt_mgr_vport_state_change_check
*
* DESCRIPTION
*	Enqueues all ports that support virtualization, in order to check
*	port's VirtualizationInfo.VPortStateChange.
*
* SYNOPSIS
*/
void osm_virt_mgr_vport_state_change_check(IN struct osm_sm * sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_clear_prtn_data
* NAME
*	osm_virt_mgr_clear_prtn_data
*
* DESCRIPTION
*	Clear all data in partitions_data object
*
* SYNOPSIS
*/
void osm_virt_mgr_clear_prtn_data(IN struct osm_sm *sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_save_partition
* NAME
*	osm_virt_mgr_save_partition
*
* DESCRIPTION
*	Save the partition information in the partitions data so if a vport
*	will turn active we can check if this partition is relevant for it
*
* SYNOPSIS
*/
boolean_t osm_virt_mgr_save_partition(IN struct osm_sm *sm, IN ib_net64_t min_guid, IN ib_net64_t max_guid,
				      IN ib_net16_t pkey, IN boolean_t full,
				      IN boolean_t indx0);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	min_guid
*		[in] Minimal guid to which this partition is relevant (For a specific range).
*		     zero means it is relevant for all guids.
*
*	max_guid
*		[in] Maximal guid to which this partition is relevant (For a specific range).
*		     zero means it is relevant for all guids.
*
*	pkey
*		[in] Partition key.
*
*	full
*		[in] Indicate whether this is a full membership.
*
*	indx0
*		[in] Indicate whether this pkey defined to be in indx0.
*
* RETURN VALUES
*	Indication whether finished successfully.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_apply_partitions
* NAME
*	osm_virt_mgr_apply_partitions
*
* DESCRIPTION
*	Apply relevant partitions from partitions data to a given vport
*
* SYNOPSIS
*/
void osm_virt_mgr_apply_partitions(IN struct osm_sm *sm, struct osm_vport *p_vport,
				   IN cl_map_t * pkey_map);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	p_vport
*		pointer to osm_vport_t object to apply partitions for
*
*	pkey_map
*		pointer to the map aggregating all pkeys in all VPORTs
*		in the same Host
*
* RETURN VALUES
*	This method does not return a value
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_add_vnode
* NAME
*	osm_virt_mgr_add_vnode
*
* DESCRIPTION
*	Keep this vnode info in the virt mgr vnodes data
*
* SYNOPSIS
*/
boolean_t osm_virt_mgr_add_vnode(struct osm_sm *sm, IN ib_vnode_info_t *p_vni,
				 IN osm_port_t *p_port, IN uint16_t vport_index);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	p_vni
*		pointer to osm_vnode_info_t object to keep
*
*	p_port
*		[in] pointer to osm_port_t object this vnode belongs to
*
*	vport_index
*		[in] vport index for which this vnode info arrived
*
* RETURN VALUES
*	Boolean indication if the vnode info was kept successfully of not
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_get_vnode
* NAME
*	osm_virt_mgr_get_vnode
*
* DESCRIPTION
*	Get vnode object for specified vnode_guid from virt mgr data
*
* SYNOPSIS
*/
osm_vnode_t *osm_virt_mgr_get_vnode(IN struct osm_sm *sm,
				    IN ib_net64_t vnode_guid);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	vnode_guid
*		[in] guid of requested vnode object
*
* RETURN VALUES
*	Pointer to vnode object if found, NULL otherwise
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_register_to_vnode
* NAME
*	osm_virt_mgr_register_to_vnode
*
* DESCRIPTION
*	Register VPort to a VNode
*
* SYNOPSIS
*/
boolean_t osm_virt_mgr_register_to_vnode(IN struct osm_sm *sm,
					 IN ib_net64_t vnode_guid,
					 IN struct osm_vport *p_vport);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	vnode_guid
*		[in] guid of VNode to register into
*
*	p_vport
*		[in] Pointer to VPort object to register
*
* RETURN VALUES
*	Boolean indication whether the registration succeeded
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_unregister_from_vnode
* NAME
*	osm_virt_mgr_unregister_from_vnode
*
* DESCRIPTION
*	Unregister VPort from a VNode
*
* SYNOPSIS
*/
void osm_virt_mgr_unregister_from_vnode(IN struct osm_sm *sm,
					IN ib_net64_t vnode_guid,
				        IN struct osm_vport *p_vport);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	vnode_guid
*		[in] guid of VNode to register into
*
*	p_vport
*		[in] Pointer to VPort object to unregister
*
* RETURN VALUES
*	This method does not return any value
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_vport_add_migration_port
* NAME
*	osm_vport_add_migration_port
*
* DESCRIPTION
*	Add a port to the list of possible migration ports
*
* SYNOPSIS
* /
void osm_vport_add_migration_port(IN struct osm_sm *sm,
				  IN osm_vnode_t *p_vnode,
				  IN osm_port_t *p_port);
/ *
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	p_vnode
*		[in] Pointer to vnode
*
*	p_port
*		[in] Pointer to port object to add to the list
*
* RETURN VALUES
*	This method does not return any value
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_reload_prtn_data
* NAME
*	osm_virt_mgr_reload_prtn_data
*
* DESCRIPTION
*	This method iterate all vports in the fabric and re-apply their
*	partitions data.
*	The function expects that the sm lock is already acquired.
*
* SYNOPSIS
*/
void osm_virt_mgr_reload_prtn_data(IN struct osm_sm *sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
* RETURN VALUES
*	This method does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_handle_vnd_change
* NAME
*	osm_virt_mgr_handle_vnd_change
*
* DESCRIPTION
*	This method handles VNode Description (vnd) change
*
* SYNOPSIS
*/
void osm_virt_mgr_handle_vnd_change(IN struct osm_sm *sm,
				   IN struct osm_port *p_port,
				   IN uint16_t vport_index);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*	p_port
*		[in] Pointer to osm_port_t object
*	vport_index
*		[in] VPort index to which VNode description changed
*
* RETURN VALUES
*	This method does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/virt_mgr_handle_empty_vnodes
* NAME
*	virt_mgr_handle_empty_vnodes
*
* DESCRIPTION
*	This function calls the function virt_mgr_handle_empty_vnode for
*	every vnode
*
* SYNOPSIS
*/
void virt_mgr_handle_empty_vnodes(struct osm_sm * sm, boolean_t migrate_ports,
				  boolean_t del_empty_vnodes);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	migrate_ports
*		[in] Boolean indication whether to migrate vports
*
*	del_empty_vnodes
*		[in] Boolean indication whether to delete empty vnodes
*
* RETURN VALUES
*	This method does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/virt_mgr_handle_empty_vnode
* NAME
*	virt_mgr_handle_empty_vnode
*
* DESCRIPTION
*	This function receives a vnode, checks if no vports attached to it
*	and deletes it if required, if the vnode has ports in its migration
*	ports list the function add these ports to the virt mgr queue
*
* SYNOPSIS
*/
void virt_mgr_handle_empty_vnode(IN struct osm_sm * sm, OUT osm_vnode_t *p_vnode,
				 IN boolean_t migrate_ports,
				 IN boolean_t del_if_empty);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	p_vnode
*		[out] Pointer to vnode structure to handle
*
*	migrate_ports
*		[in] Boolean indication whether to migrate vports
*
*	del_if_empty
*		[in] Boolean indication whether to delete the vnode if empty
*
* RETURN VALUES
*	This method does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_sweep
* NAME
*	osm_virt_mgr_sweep
*
* DESCRIPTION
* 	Process virtual ports in the subnet, during sweep.
*
* SYNOPSIS
*/
void osm_virt_mgr_sweep(IN struct osm_sm * sm, uint8_t stage);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*	stage
*		[in] Heavy seep stage indicator.
*
* RETURN VALUES
* 	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

/****f* OpenSM: Virtualization Manager/osm_virt_mgr_revalidate_all_vports
* NAME
*	osm_virt_mgr_revalidate_all_vports
*
* DESCRIPTION
*	Revalidate the physical guid -> virtual guid mapping is authorized
*	on all fabric ports.
*
* SYNOPSIS
*/
void osm_virt_mgr_revalidate_all_vports(struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_sm
*             [in] Pointer to the SM object.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Virtualization Manager object
*********/

END_C_DECLS
