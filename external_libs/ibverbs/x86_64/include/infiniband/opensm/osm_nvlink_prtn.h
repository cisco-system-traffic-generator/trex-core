/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *	Module for Link as a Service configuration for NVLink systems
 *
 * Author:
 *	Or Nechemia, NVIDIA
 *	Sasha Minchiu, NVIDIA
 */

#pragma once

#include <opensm/osm_sm.h>
#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_log.h>
#include <opensm/osm_nvlink.h>
#include <opensm/osm_gpu.h>
#include <opensm/osm_partition.h>
#include <complib/cl_fleximap.h>
#include <stdint.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS }
#else                           /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif                          /* __cplusplus */

BEGIN_C_DECLS

#define OSM_NVL_INVALID_PKEY	0x0

#define NVL_ADMIN_STATE_MASK_UP   (1 << IB_NMX_ADMINSTATE_UP)
#define NVL_ADMIN_STATE_MASK_DOWN (1 << IB_NMX_ADMINSTATE_DOWN)
#define NVL_ADMIN_STATE_MASK_DIAG (1 << IB_NMX_ADMINSTATE_DIAG)

/* Bitmask to define configuration for multiple port types */
typedef enum {
	NVL_PORT_TYPE_MASK_ACCESS = 1,
	NVL_PORT_TYPE_MASK_TRUNK = 2,
	NVL_PORT_TYPE_MASK_FNM = 4,
	NVL_PORT_TYPE_MASK_ALL = 7
} osm_nvlink_port_type_mask_t;

static inline void osm_nvl_prtn_clear_gpu2pkey_db(osm_nvlink_mgr_t *p_mgr)
{
	cl_hashmap_remove_all(&p_mgr->gpu_to_pkey_map);
}

/****f* OpenSM: NVLink/osm_nvl_prtn_destroy_prtn
* NAME
*	osm_nvl_prtn_destroy_prtn
*
* DESCRIPTION
* 	Delete NVL partition structure
*
* SYNOPSIS
*/
void osm_nvl_prtn_destroy_prtn(osm_nvlink_mgr_t * p_mgr, osm_nvlink_prtn_t * p_prtn);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_prtn
*		Pointer to an osm_nvlink_prtn_t object
*
* RETURN VALUES
* 	None
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_destroy_prtn_by_pkey
* NAME
*	osm_nvl_prtn_destroy_prtn_by_pkey
*
* DESCRIPTION
* 	Delete NVL partition structure according to pkey
*
* SYNOPSIS
*/
void osm_nvl_prtn_destroy_prtn_by_pkey(osm_nvlink_mgr_t * p_mgr, ib_net16_t pkey);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	pkey
*		PKey of which NVL partition should be destroyed
*
* RETURN VALUES
* 	None
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_make_new
* NAME
* 	osm_nvl_prtn_make_new
*
* DESCRIPTION
* 	The function creates both partition for partition manager and NVL partition
* 	for nvlink manager.
* 	Returns the NVL partition as a return value, and the partition for PKey manger in **p.
* 	NULL if could not be created.
*
* SYNOPSIS
*/
osm_nvlink_prtn_t * osm_nvl_prtn_make_new(IN osm_nvlink_mgr_t * p_mgr,
					  IN const char *name,
					  IN ib_net16_t pkey,
					  IN uint8_t max_trunk_links,
					  IN boolean_t is_reroute_req,
					  OUT osm_prtn_t ** pp_prtn);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
* 	name
*		String name for PKey manager partition
*
*	pkey
*		PKey for both structures
*
*	max_trunk_links
*		Maximal number of trunk links to assign for the nvlink partition
*
*	is_reroute_req
*		Boolean indicates whether SM is allowed to reroute the nvlink partition
*
*	pp_prtn
*		Pointer to return PKey manager partition
*
* RETURN VALUES
* 	Returns the NVL partition as a return value, and the partition for PKey manger in **p.
* 	NULL if could not be created.
*
* SEE ALSO
* 	osm_nvlink_prtn_t, osm_prtn_t
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_is_trunk_in_cnd
* NAME
*	osm_nvl_prtn_is_trunk_in_cnd
*
* DESCRIPTION
* 	The function returns TRUE if p_physp is a switch trunk port that is
* 	in Contain and Drain state. Otherwise, FALSE.
*
* SYNOPSIS
*/
boolean_t osm_nvl_prtn_is_trunk_in_cnd(osm_nvlink_mgr_t * p_mgr, osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_physp
*		Pointer to physical port
*
* RETURN VALUES
* 	The function returns TRUE if p_physp is a switch trunk port that is
* 	in Contain and Drain state. Otherwise, FALSE.
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_add_gpu
* NAME
*	osm_nvl_prtn_add_gpu
*
* DESCRIPTION
* 	The function adds all the ports of the GPU to the partition.
*
* SYNOPSIS
*/
int osm_nvl_prtn_add_gpu(osm_nvlink_mgr_t *p_mgr, osm_nvlink_prtn_t *p_prtn, ib_net64_t node_guid);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_prtn
*		Pointer to an osm_nvlink_prtn_t object
*
*	node_guid
*		The node GUID of the GPU
*
* RETURN VALUES
* 	Return 0 in case of success
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_get_gpu_ptrn
* NAME
*	osm_nvl_prtn_get_gpu_ptrn
*
* DESCRIPTION
* 	Returns Partition key of the GPU.
*
* SYNOPSIS
*/
ib_net16_t osm_nvl_prtn_get_gpu_prtn(IN osm_nvlink_mgr_t * p_mgr, IN osm_gpu_t * p_gpu);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object.
*
* 	p_gpu
*		Pointer to GPU object.
*
* RETURN VALUES
* 	Returns Partition key of the GPU in network order.
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_update_num_req_guids
* NAME
*	osm_nvl_prtn_update_num_req_guids
*
* DESCRIPTION
*   The function updates the number of requested GPUs to the partition.
*
* SYNOPSIS
*/
void osm_nvl_prtn_update_num_req_guids(osm_nvlink_prtn_t * p_prtn, uint8_t num_req_guids);
/*
* PARAMETERS
*	p_prtn
*		Pointer to an osm_nvlink_prtn_t object.
*
*	num_req_guids
*		The number of requested GPUs.
*
* RETURN VALUES
* 	This function does not return a value.
*
*********/

typedef enum _osm_nvlink_link_type {
	IB_NVLINK_LINK_UNDEFINED,
	IB_NVLINK_LINK_TRUNK,
	IB_NVLINK_LINK_ACCESS_SW,
	IB_NVLINK_LINK_ACCESS_GPU,
} osm_nvlink_link_type_t;

typedef struct osm_nvlink_link {
	osm_physp_t *p_physp;
	uint8_t type;
} osm_nvlink_link_t;
/*
* FIELDS
* 	p_physp
*		Pointer to osm_physp_t object.
* 	type
* 		Link type (osm_nvlink_link_type_t)
*
* SEE ALSO
* 	osm_nvlink_link_t, osm_nvl_prtn_get_links, osm_nvl_prtn_release_links
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_get_links
* NAME
*	osm_nvl_prtn_get_links
*
* DESCRIPTION
* 	The function returns array of osm_nvlink_link_type_t objects associate by
* 	specified partition.
*
* SYNOPSIS
*/
osm_nvlink_link_t * osm_nvl_prtn_get_links(osm_subn_t * p_subn, uint16_t pkey);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a Subnet object to construct.
*
* 	pkey
* 		Partition
*
* RETURN VALUES
* 	The function return NULL in case of failure.
* 	Otherwise, returns array of osm_nvlink_link_type_t, last element is zero.
*
* NOTES
* 	Returned array of osm_nvlink_link_type_t objects should be released using
* 	osm_nvl_prtn_release_links function.
*
* SEE ALSO
* 	osm_nvlink_link_t, osm_nvl_prtn_get_links, osm_nvl_prtn_release_links
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_release_links
* NAME
*	osm_nvl_prtn_release_links
*
* DESCRIPTION
* 	The function releases array of osm_nvlink_link_type_t objects previously
* 	allocated by osm_nvl_prtn_get_links.
*
* SYNOPSIS
*/
void osm_nvl_prtn_release_links(IN osm_nvlink_link_t * links);
/*
* PARAMETERS
* 	links
* 		Array of osm_nvl_prtn_get_links objects.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
* 	osm_nvlink_link_t, osm_nvl_prtn_get_links, osm_nvl_prtn_release_links
*/

void osm_nvl_prtn_preprocess(osm_subn_t * p_subn);

void osm_nvl_prtn_laas_process(osm_subn_t * p_subn);

void osm_nvl_prtn_send_rail_filter_of_prtn(osm_subn_t * p_subn, osm_switch_t * p_sw,
					   ib_net16_t pkey, uint8_t rail, uint8_t ingress_block,
					   uint8_t egress_block);

void osm_nvl_prtn_cpy_subn_rail_filter_to_prtn(osm_subn_t * p_subn, osm_switch_t * p_sw,
					       ib_net16_t pkey, uint8_t rail, uint8_t ingress_block,
					       uint8_t egress_block,
					       ib_rail_filter_config_t * p_rail_filter);

void osm_nvl_prtn_reroute_finish(osm_subn_t * p_subn);

/****f* OpenSM: NVLink/osm_nvl_prtn_get_gpu_reroute_allowed
* NAME
*	osm_nvl_prtn_get_gpu_reroute_allowed
*
* DESCRIPTION
*	Returns TRUE if should reroute GPU ALID for switches that belong to input plane.
*
* SYNOPSIS
*/
boolean_t osm_nvl_prtn_get_gpu_reroute_allowed(osm_nvlink_mgr_t * p_mgr, osm_gpu_t * p_gpu,
					       uint8_t plane);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_gpu
*		Pointer to GPU to validate
*
*	plane
*		Plane number
*
* RETURN VALUE
*	Returns TRUE if should reroute GPU ALID for switches that belong to input plane.
*	Otherwise, FALSE.
*
* SEE ALSO
*/

/****f* OpenSM: NVLink/osm_nvl_prtn_init
* NAME
*	osm_nvl_prtn_init
*
* DESCRIPTION
* 	The function initialize the stuctures required for NVLink partition configuration
*
* SYNOPSIS
*/
ib_api_status_t osm_nvl_prtn_init(IN osm_nvlink_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
* RETURN VALUES
*	IB_SUCCESS status if structures were initialized successfully.
*	Otherwise, relevant error status.
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_get_prtn_state
* NAME
*	osm_nvl_prtn_get_prtn_state
*
* DESCRIPTION
* 	Returns the partition state for NVL partition of input pkey
*
* SYNOPSIS
*/
uint8_t osm_nvl_prtn_get_prtn_state(osm_nvlink_mgr_t * p_mgr, ib_net16_t pkey);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	pkey
*		PKey of which NVL partition should be destroyed
*
* RETURN VALUES
* 	Partition state for NVL partition of input pkey
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_get_prtn_state_str
* NAME
*	osm_nvl_prtn_get_prtn_state_str
*
* DESCRIPTION
* 	Returns string of partition state of NVL partition of input pkey
*
* SYNOPSIS
*/
char * osm_nvl_prtn_get_prtn_state_str(osm_nvlink_mgr_t * p_mgr, ib_net16_t pkey);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	pkey
*		PKey of which NVL partition should be destroyed
*
* RETURN VALUES
* 	Partition state string for NVL partition of input pkey
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_init_sw_subn_rail_filter
* NAME
*	osm_nvl_prtn_init_sw_subn_rail_filter
*
* DESCRIPTION
* 	Iterates over all partitions and Initialize subnet rail filter of input switch
*
* SYNOPSIS
*/
void osm_nvl_prtn_init_sw_subn_rail_filter(osm_nvlink_mgr_t * p_mgr, osm_switch_t * p_sw);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_sw
*		Pointer to switch of which ti initialize subnet rail filter
*
* RETURN VALUES
* 	None
*
*********/

char * osm_nvl_prtn_get_admin_state_str(uint8_t admin_state);

typedef struct osm_sw_port {
	uint64_t guid;
	uint8_t port_num;
} osm_sw_port_t;

typedef struct osm_nvl_admin_state_map_item {
	cl_fmap_item_t fmap_item;
	osm_sw_port_t sw_port_key;
	uint8_t admin_state;
} osm_nvl_admin_state_map_item_t;

uint8_t osm_nvl_prtn_get_port_subn_admin_state(osm_nvlink_mgr_t * p_mgr, uint64_t node_guid, uint8_t port_num);

ib_api_status_t osm_nvl_prtn_set_port_admin_state(osm_nvlink_mgr_t * p_mgr,
						  uint64_t guid,
						  uint8_t port_num,
						  uint8_t admin_state);

char * osm_nvl_prtn_get_admin_state_str(uint8_t admin_state);

boolean_t osm_nvl_prtn_is_port_in_cnd(osm_physp_t *p_physp);

/****f* OpenSM: NVLink/osm_nvl_prtn_set_mlnx_extended_port_info
* NAME
*	osm_nvl_prtn_set_mlnx_extended_port_info
*
* DESCRIPTION
* 	Set Admin State for switch trunk ports only
*
* SYNOPSIS
*/
void osm_nvl_prtn_set_mlnx_extended_port_info(osm_nvlink_mgr_t * p_mgr, osm_physp_t * p_physp, uint8_t admin_states_bitmask_to_apply);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_physp
*		Pointer to an osm_physp_t object
*
*	admin_states_bitmask_to_apply
*		Bitmask of admin states to apply
*
* RETURN VALUES
* 	None
*
*********/

ib_api_status_t osm_nvl_prtn_set_all_sw_init_ports_cnd_state(osm_subn_t * p_subn,
							     uint8_t port_type_mask,
							     ib_cnd_port_state_t cnd_port_state);
/*
* PARAMETERS
*	p_subn
*		Pointer to an osm_subn_t object
*
*	port_type_mask
*		Port type mask to set the state on
*
*	cnd_port_state
*		Contain and Drain port state to set on all switch ports in INIT state
*
*********/

typedef struct _osm_link {
	cl_fmap_item_t map_item; /* Must be first */
	uint64_t guid;		 /* Node GUID */
	uint8_t port_num;	 /* Port number */
	uint64_t peer_guid;	 /* Peer node GUID */
	uint8_t peer_port_num;	 /* Peer port number */
} osm_link_t;

boolean_t osm_nvl_prtn_is_link_in_cnd_map(osm_nvlink_mgr_t *p_mgr, osm_physp_t *p_physp);

void osm_nvl_prtn_cnd_map_destroy(osm_nvlink_mgr_t *p_mgr);

osm_link_t *osm_nvl_prtn_cnd_map_insert(osm_nvlink_mgr_t *p_mgr,
                                        uint64_t guid, uint8_t port_num,
                                        uint64_t peer_guid, uint8_t peer_port_num);

void osm_nvl_prtn_cnd_map_remove(osm_nvlink_mgr_t *p_mgr, osm_physp_t *p_physp);

void osm_nvl_prtn_clear_all_switch_cnd_links(osm_subn_t *p_subn);

boolean_t osm_nvl_prtn_cnd_can_activate_link(osm_sm_t *sm, osm_physp_t *p_physp);

/****f* OpenSM: NVLink/osm_nvl_prtn_validate_admin_state_ports
* NAME
*	osm_nvl_prtn_validate_admin_state_ports
*
* DESCRIPTION
* 	Validate Admin State of switch ports
*
* SYNOPSIS
*/
void osm_nvl_prtn_validate_admin_state_ports(osm_subn_t * p_subn, osm_switch_t * p_sw, uint8_t admin_states_bitmask);
/*
* PARAMETERS
*	p_subn
*		Pointer to an osm_subn_t object
*
*	p_sw
*		Pointer to an osm_switch_t object
*
*	admin_states_bitmask
*		Bitmask of admin states to validate
*
* RETURN VALUES
* 	None
*
*********/

/****f* OpenSM: NVLink/osm_nvl_prtn_is_port_in_admin_state
* NAME
*	osm_nvl_prtn_is_port_in_admin_state
*
* DESCRIPTION
* 	In case requested admin state could not be configured to subnet,
* 	port may still be in admin state.
* 	Thus, port is considered in admin state when it is either configured to admin state or
* 	requested to be configured to admin state.
*
* SYNOPSIS
*/
boolean_t osm_nvl_prtn_is_port_in_admin_state(osm_nvlink_mgr_t * p_mgr, osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
*	p_physp
*		Pointer to an osm_physp_t object
*
* RETURN VALUES
*	TRUE if port is in admin state or requested to be configured to admin state.
*	Otherwise, FALSE.
*
*********/

END_C_DECLS
