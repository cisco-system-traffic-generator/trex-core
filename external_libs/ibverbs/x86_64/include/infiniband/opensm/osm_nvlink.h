/*
 * Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *	Module for NVLink discovery
 *
 * Author:
 *	Or Nechemia, NVIDIA
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <complib/cl_hashmap.h>
#include <opensm/osm_log.h>
#include <opensm/osm_gpu.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/* Some hardcoded values for Single Node (SN) case of NVLink */
#define OSM_NVLINK_NUM_ALIDS_PER_GPU		1
#define OSM_NVLINK_MAX_NUM_ALIDS_PER_GPU	2
/* For SN topologies, LID ranges of all LID types are of predefined length */
#define OSM_NVLINK_GLOBAL_LID_RANGE_LEN		0x400
#define OSM_NVLINK_NVLID_RANGE_LEN		0x800
#define OSM_NVLINK_GPU_LMC			0
#define OSM_NVLINK_SN_NUM_RAILS			18
#define OSM_NVLINK_NO_PLANE			0
#define OSM_NVLINK_INVALID_PLANE_INDEX		OSM_NVLINK_MAX_PLANES
#define OSM_NVLINK_NO_RAIL			0xFF
#define OSM_NVLINK_MAX_PORT_PARTITIONS	        2

#define OSM_RAIL_FILTER_SUBN			0
#define OSM_RAIL_FILTER_CALC			1

#define OSM_DEFAULT_NVL_HBF_HASH_TYPE		OSM_HBF_HASH_TYPE_CRC
#define OSM_DEFAULT_NVL_HBF_PACKET_HASH_BITMASK	0
#define OSM_DEFAULT_NVL_HBF_SEED		0
#define OSM_DEFAULT_NVL_HBF_FIELDS_ENABLE	IB_NVL_HBF_CONFIG_FIELDS_ENABLE_ALL
#define OSM_DEFAULT_NVL_PRTN_REROUTE_WAIT	2
#define OSM_DEFAULT_NVL_PRTN_REROUTE_WAIT_MAX	100

/* In Umbriel, all ports get the same port profile for NVL HBF Config*/
#define OSM_NVLINK_NVL_HBF_PORTS_PROFILE	1

/* Some hardcoded values for Single Node (SN) case of NVLink */
#define OSM_NVLINK_MIN_RAIL			0
#define OSM_NVLINK_MAX_RAIL			OSM_NVLINK_SN_NUM_RAILS - 1

#define OSM_NVL_ADD_PORT			TRUE
#define OSM_NVL_REMOVE_PORT			FALSE

#define OSM_RAIL_FILTER_INGRESS_NUM_BLOCKS	2
#define OSM_RAIL_FILTER_EGRESS_NUM_BLOCKS	1

typedef struct osm_nvlink_mgr {
	osm_subn_t *p_subn;
	cl_disp_reg_handle_t nvlink_mad_disp_h;
	osm_db_domain_t *p_g2alid;
	osm_db_domain_t *p_g2nvlid;
	cl_qlist_t free_alid_ranges;
	uint16_t alid_range_start;
	uint16_t alid_range_top;
	uint16_t num_alids_per_gpu;
	uint16_t p_start_ranges_per_plane[OSM_NVLINK_MAX_PLANES + 1];
	cl_qlist_t p_free_ranges_per_plane[OSM_NVLINK_MAX_PLANES + 1];
	uint16_t global_lid_range_top;
	uint16_t lid_range_top_per_plane[OSM_NVLINK_MAX_PLANES + 1];
	osm_db_domain_t *p_guid2planes;
	osm_db_domain_t *p_port2rail;
	cl_hashmap_t gpu_to_pkey_map;
	osm_db_domain_t *p_port2nvl_prtn;
	cl_qmap_t nvl_prtn_tbl;
	cl_fmap_t nvl_admin_states;
	cl_fmap_t nvl_clear_cnd_map;
} osm_nvlink_mgr_t;
/*
* FIELDS
* 	p_subn
*		Pointer to the Subnet object for this subnet.
*
* 	nvlink_mad_disp_h
*		Dispatcher that handles NVLink related MADs
*
*	p_g2alid
*		Pointer to the database domain storing GPU guid to anycast lid mapping.
*		Data is stored by guid, first alid, second alid.
*		As number of ALIDs is known to SM (currently hardcoded for Single Node),
*		and is planned to be up to 2 ALIDs per GPU.
*
* 	p_g2nvlid
* 		Pointer to the database domain storing GPU port guid to lid mapping.
*
* 	free_alid_ranges
*		A list of available free Anycast LID ranges.
*
* 	alid_range_start
*		Anycast LID range start LID.
*
*	alid_range_top
*		Maximal Anycast LID configured on the subnet. Set to switches by LFTSplit MAD.
*
* 	num_alids_per_gpu
*		Number of Anycast LIDs to be configured for each GPU.
* 
* 	p_start_ranges_per_plane
*		Array of LIDs, each refers to start LID range of a plane.
*
* 	p_free_ranges_per_plane
*		Array of range lists, indexed by plane.
*		Each list holds available free GPU port LID ranges.
*
* 	global_lid_range_top
*		Maximal LID used of global LIDs. This refers to devices other than GPUs.
*		Used for LFTSplit to set the LID space division for each switch.
*
* 	lid_range_top_per_plane
*		Array of maximal LID used for each plane. This refers to the GPU ports.
*		Used for LFTSplit to set the LID space division for each switch.
*
*	p_guid2planes
*		Pointer to the database domain storing switch GUID to plane mapping.
*
*	p_port2rail
*		Pointer to the database domain storing switch port (GUID and port) to rail mapping.
*		For each link, rails are identical for both ports, hence no need to cache GPU port,
*		as it holds the same rail as its remote switch port.
*
*	gpu_to_pkey_map
*		Map GPU GUID to PKey, that needs to be configured on all the ports of that GPU.
*
*	p_port2nvl_prtn
*		Pointer to the database domain storing switch trunk ports (switch GUID, port number)
*		to LAAS pkey mapping.
*
*	nvl_prtn_tbl
*		Container of pointers to all NVLink Partition objects in the subnet.
*		Indexed by PKey in network order, with membership bit off (15bits PKey).
*
* 	nvl_admin_states
* 		Map from switch port (GUID in host order, port number) to admin state.
* 		Used for admin state persistency in case of port or switch reset on SM runtime.
* 		In case of SM reset, admin states are expected to be part of partition sync.
*
* 	nvl_clear_cnd_map
* 		Map of all the switch links
* 		(GUID in host order, port number, peer GUID in host order, peer port number) that are
* 		needed to be cleared from CND state.
*********/

/****d* OpenSM: NVLink/osm_nvlink_lftsplit_state_t
* NAME
*       osm_nvlink_lftsplit_state_t
*
* DESCRIPTION
* 	Enumerates LFTSplit state for NVLink
*	LFTSplit should be initialized before setting its new values to the switch.
*	Thus, use this state machine for correct flow.
*
* SYNOPSIS
*/
typedef enum _osm_nvlink_lftsplit_state
{
	IB_LFT_SPLIT_STATE_NONE,
	IB_LFT_SPLIT_STATE_NEED_UPDATE,
	IB_LFT_SPLIT_STATE_SWITCH_INFO_RESET,
	IB_LFT_SPLIT_STATE_LFT_SPLIT_RESET,
	IB_LFT_SPLIT_STATE_SWITCH_INFO_SET,
	IB_LFT_SPLIT_STATE_UPDATED,
} osm_nvlink_lftsplit_state_t;
/*
 * IB_LFT_SPLIT_STATE_NONE
 * 	Before entring LFTSplit flow.
 * 	LFTSplit GET is expected to be sent first in order to determine next steps.
 *
 * IB_LFT_SPLIT_STATE_NEED_UPDATE
 * 	LFTSplit values of the subnet should be updated (according to LFTSplit GETResp).
 * 	First, should set SwitchInfo.lid_top to 0, and only then initialize the LFTSplit values.
 * 	This order is important due to FW behavior!
 *
 * IB_LFT_SPLIT_STATE_SWITCH_INFO_RESET
 * 	SwitchInfo.lid_top was initialized to zero successfully by SwitchInfo SET.
 *
 * IB_LFT_SPLIT_STATE_LFT_SPLIT_RESET
 *	LFTSplit ranges were initialized successfully.
 *	Now can update to new values. Again, first update SwitchInfo.lid_top and only
 *	then LFTSplit. This order is important due to FW requirements.
 *
 * IB_LFT_SPLIT_STATE_SWITCH_INFO_SET
 * 	SwitchInfo.lid_top was updated successfully by SwitchInfo SET.
 *
 * IB_LFT_SPLIT_STATE_UPDATED
 * 	Subnet is updated with the required LFTSplit values.
 * 	Can get to this state by either:
 * 	1. LFTSplit values were updated successfully at the end of a whole flow.
 * 	2. Following LFTSplit GETResp, it was found subnet is configured with the expected values,
 * 	   hence no need for an update (meaning skipped from state IB_LFT_SPLIT_STATE_NONE to this state).
 *
 ***********/

typedef struct osm_nvlink_sw_data {
	uint8_t plane;
	uint8_t plane_index;
	uint16_t rail_used_mask;
	uint8_t max_ingress_block;
	uint8_t max_egress_block;
	ib_profiles_config_t ports_profiles[IB_PROFILE_CONFIG_NUM_BLOCKS];
	uint8_t nvl_hbf_profiles_configured;
	ib_nvl_hbf_config_t subn_nvl_hbf_cfg;
	ib_nvl_hbf_config_t calc_nvl_hbf_cfg;
	ib_linear_forwarding_table_split_t subn_lft_split;
	ib_linear_forwarding_table_split_t calc_lft_split;
	osm_nvlink_lftsplit_state_t lft_split_state;
	ib_contain_and_drain_info_t subn_cnd_info[IB_CND_NUM_BLOCKS];
	uint8_t cnd_info_updated;
	ib_contain_and_drain_port_state_t subn_cnd_port_state[IB_CND_NUM_BLOCKS];
	ib_rail_filter_config_t subn_non_allocated_ports_rail_filter;
	boolean_t is_non_allocated_updated;
} osm_nvlink_sw_data_t;
/*
* PARAMETERS
* 	rail_used_mask
*		Bitmask used for marking which rails were used for this switch.
*		RailFilterConfig will be sent according to this mask.
*
*	ports_profiles
*		ProfilesConfig blocks data as latest received from subnet
*
*	nvl_hbf_profiles_configured
*		Bitmask indicates whether profiles were configured for this feature.
*		Each bit represents ProfilesConfig BLOCK, which may contain multiple profiles.
*		hence to make sure all profile blocks were set successfully before sending
*		profiles parameters by NVL HBF Config MADs.
*
*	subn_nvl_hbf_cfg
*		NVLHBFConfig configured for port profile 1 of this switch.
*		For current implementation, set a single profile to all ports.
*		Hence, need for a single MAD, for profile 1 (profile 0 is reserved).
*
* 	calc_nvl_hbf_cfg
*		NVLHBFConfig calculated for port profile 1 of this switch.
*		For current implementation, set a single profile to all ports.
*		Hence, need for a single MAD, for profile 1 (profile 0 is reserved).
*
* 	lft_split_state
* 		Indicates at which stage of setting LFTSplit this switch is.
* 		please see osm_nvlink_lftsplit_state_t for more informaion
* 		regarding LFTSplit flow.
*
* 	subn_cnd_info
* 		ContainAndDrainInfo configured on the subnet for this switch.
*
* 	cnd_info_updated
* 		Bitmask indicates whether all ContainAndDrainInfo blocks were set successfully and subn_cnd_info is updated.
*
* 	subn_cnd_port_state
* 		ContainAndDrainPortState configured on the subnet for this switch.
*
*********/

/****d* OpenSM: NVLink/osm_nvlink_port_type_t
* NAME
*       osm_nvlink_port_type_t
*
* DESCRIPTION
* 	Enumerates port type for NVLink, in which same port can be referred
* 	in differnt point of view.
*
* SYNOPSIS
*/
typedef enum _osm_nvlink_port_type
{
	IB_PORT_TYPE_INGRESS,
	IB_PORT_TYPE_EGRESS,
	IB_PORT_TYPE_MAX,
} osm_nvlink_port_type_t;
/***********/

void osm_nvlink_sw_data_init(struct osm_switch * p_sw);

/****f* OpenSM: NVLink/osm_nvlink_send_set_alid_info
* NAME
*	osm_nvlink_send_set_alid_info
*
* DESCRIPTION
*	Send calculated ALIDInfo to input GPU.
*	Call this function AFTER ALIDs are already configured into GPU's ALIDInfo structure
*	Returns status indicates if MAD was sent successfully.
*
* SYNOPSIS
*/
ib_api_status_t osm_nvlink_send_set_alid_info(struct osm_sm * sm, struct osm_gpu * p_gpu);
/*
* PARAMETERS
*	p_sm
*		Pointer to an osm_sm_t object
*
*	p_gpu
*		Pointer to the GPU.
*
* RETURN VALUES
*	IB_SUCCESS status, if MAD was sent successfully. Otherwise, relevant error status.
*
* SEE ALSO
* 	ib_alid_info_t
*********/


/****f* OpenSM: NVLink/osm_nvlink_set_gpu_extended_node_info
* NAME
*	osm_nvlink_set_gpu_extended_node_info
*
* DESCRIPTION
*	Send calculated ExtendedNodeInfo to input GPU.
*	Returns status indicates if MAD was sent successfully.
*
* SYNOPSIS
*/
ib_api_status_t osm_nvlink_set_gpu_extended_node_info(struct osm_sm * sm, struct osm_gpu * p_gpu);
/*
* PARAMETERS
*	p_sm
*		Pointer to an osm_sm_t object
*
*	p_gpu
*		Pointer to the GPU.
*
* RETURN VALUES
*	IB_SUCCESS status, if MAD was sent successfully. Otherwise, relevant error status.
*
* SEE ALSO
* 	ib_mlnx_ext_node_info_t
*********/

/****f* OpenSM: NVLink/osm_nvlink_init
* NAME
*	osm_nvlink_init
*
* DESCRIPTION
*	Function for initialization of NVLink manager structure.
*	Returns status indicates if was initialized successfully.
*
* SYNOPSIS
*/
ib_api_status_t osm_nvlink_init(struct osm_sm * sm);
/*
* PARAMETERS
*	p_sm
*		Pointer to an osm_sm_t object
*
* RETURN VALUES
*	IB_SUCCESS status, if initialized successfully. Otherwise, relevant error status.
*********/

/****f* OpenSM: NVLink/osm_nvlink_mgr_destroy
* NAME
*	osm_nvlink_mgr_destroy
*
* DESCRIPTION
*	Function for destroying NVLink manager structure.
*
* SYNOPSIS
*/
void osm_nvlink_mgr_destroy(osm_nvlink_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to an osm_nvlink_mgr_t object
*
* RETURN VALUES
* 	None
*********/

/****f* OpenSM: NVLink/osm_nvlink_destroy_disp
* NAME
*	osm_nvlink_destroy_disp
*
* DESCRIPTION
*	Function for destorying NVLink manager structure.
*
* SYNOPSIS
*/
void osm_nvlink_destroy_disp(osm_nvlink_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_sm
*		Pointer to an osm_sm_t object
*
* RETURN VALUES
* 	None
*********/

/****f* OpenSM: NVLink/osm_nvlink_configuration
* NAME
*	osm_nvlink_configuration
*
* DESCRIPTION
*	Function for post discovery required for NVLink configuration.
*	Returns status indicates if was successfully done.
*
* SYNOPSIS
*/
ib_api_status_t osm_nvlink_configuration(osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	Status indicates if NVLink configuration was done successfully.
*
* SEE ALSO
*********/

/****f* OpenSM: NVLink/osm_nvlink_nvl_hbf_send_profiles_config
* NAME
*	osm_nvlink_nvl_hbf_send_profiles_config
*
* DESCRIPTION
* 	Send ProfileConfig MAD for setting NVL HBF feature port profiles
*
* SYNOPSIS
*/
void osm_nvlink_nvl_hbf_send_profiles_config(struct osm_sm * sm, struct osm_switch * p_sw);
/*
* PARAMETERS
*	p_sm
*		Pointer to an osm_sm_t object
*
*	p_sw
*		Pointer to a Switch object
*
* RETURN VALUES
* 	None.
*
* SEE ALSO
* 	ib_profiles_config_t, ib_nvl_hbf_config_t
*********/

/****f* OpenSM: NVLink/osm_nvlink_lft_split_process
* NAME
*	osm_nvlink_lft_split_process
*
* DESCRIPTION
* 	Configuration of LFTSplit ranges, starting from getting current values of LFTSplit,
* 	and configuration of new values if required.
*
* SYNOPSIS
*/
ib_api_status_t osm_nvlink_lft_split_process(struct osm_sm * sm);
/*
* PARAMETERS
*	sm
*		Pointer to an osm_sm_t object
*
* RETURN VALUES
* 	None
*
* SEE ALSO
* 	ib_linear_forwarding_table_split_t, osm_nvlink_lftsplit_state_t
*********/

void osm_nvlink_build_alids_ar_groups(osm_subn_t * p_subn);

/****f* OpenSM: NVLink/osm_nvlink_get_port_pkeys
* NAME
*	osm_nvlink_get_port_pkeys
*
* DESCRIPTION
* 	The function populates the 'pkeys_array' with the NVL PKeys of the given port.
* 	All filled PKeys in 'pkeys_array' are non-zero values.
*
* SYNOPSIS
*/
int osm_nvlink_get_port_pkeys(IN osm_physp_t *p_physp, OUT ib_net16_t *pkeys_array, IN int array_size);
/*
* PARAMETERS
*	p_physp
*		Pointer to an osm_physp_t object
*
*	pkeys_array
*		Pointer to an empty array for this function to fill
*
*	array_size
*		pkeys_array size. Note: array_size should be >= OSM_NVLINK_MAX_PORT_PARTITIONS 
*
* RETURN VALUES
* 	Return 0 in case of success
*
*
*********/

/****f* OpenSM: NVLink/osm_nvlink_calc_cnd_block_range
 * NAME
 *	osm_nvlink_calc_cnd_block_range
 *
 * DESCRIPTION
 *	Calculate the start and end port numbers for the given ContainAndDrain block number.
 *
 * SYNOPSIS
 */
void osm_nvlink_calc_cnd_block_range(IN uint8_t block_num,
				     IN uint8_t num_ports,
				     OUT uint16_t *port_num_start,
				     OUT uint16_t *port_num_end);
/*
* PARAMETERS
*	block_num
*		[in] Block number.
*
*	num_ports
*		[in] Number of ports.
*
*	port_num_start
*		[out] Pointer to the start port number.
*
*	port_num_end
*		[out] Pointer to the end port number.
*
* RETURN VALUE
*	None.
*
*
*********/

/****f* OpenSM: NVLink/osm_nvlink_calc_num_of_cnd_blocks
* NAME
*	osm_nvlink_calc_num_of_cnd_blocks
*
* DESCRIPTION
* 	Calculate the number of blocks needed for ContainAndDrain.
*
* SYNOPSIS
*/
uint8_t osm_nvlink_calc_num_of_cnd_blocks(struct osm_sm * sm, osm_switch_t * p_sw);
/*
* PARAMETERS
*	sm
*		Pointer to an osm_sm_t object.
*
*	p_sw
*		Pointer to an osm_switch_t object.
*
* RETURN VALUES
* 	Number of blocks needed for ContainAndDrain.
*
*
*********/

/****f* OpenSM: NVLink/osm_nvlink_set_cnd_info_updated
 * NAME
 *	osm_nvlink_set_cnd_info_updated
 *
 * DESCRIPTION
 *	Mark ContainAndDrainInfo block of the feature as updated successfully, or as not
 *	updated yet, according to is_updated.
 *
 * SYNOPSIS
 */
void osm_nvlink_set_cnd_info_updated(osm_switch_t * p_sw,
				     uint8_t block,
				     boolean_t is_updated);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
*	block
*		Block number
*
*	is_updated
*		Boolean indicates whether to mark ContainAndDrainInfo block of this feature
*		as updated or as not updated for this switch.
*
* RETURN VALUE
*	None
*
*
*********/

/****f* OpenSM: NVLink/osm_nvlink_is_cnd_info_updated
 * NAME
 *	osm_nvlink_is_cnd_info_updated
 *
 * DESCRIPTION
*	Returns boolean indicates whether all ContainAndDrainInfo blocks were set successfully.
 *
 * SYNOPSIS
 */
boolean_t osm_nvlink_is_cnd_info_updated(struct osm_sm * sm, osm_switch_t * p_sw);
/*
* PARAMETERS
*	sm
*		Pointer to sm.
*
*	p_sw
*		Pointer to switch.
*
* RETURN VALUE
*	Boolean indicates whether all ContainAndDrainInfo blocks were set successfully.
*
*
*********/

/****f* OpenSM: NVLink/osm_nvlink_calc_port_cnd_info
 * NAME
 *	osm_nvlink_calc_port_cnd_info
 *
 * DESCRIPTION
*	Calculate the ContainAndDrainInfo port state values for the given port.
 *
 * SYNOPSIS
 */
uint8_t osm_nvlink_calc_port_cnd_info(osm_subn_t * p_subn, osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet data structure.
*
*	p_physp
*		Pointer to a physical port object.
*
* RETURN VALUE
*	ContainAndDrainInfo port state value.
*
*
*********/

void osm_nvlink_send_get_contain_and_drain_port_state(struct osm_sm * sm,
						      struct osm_switch * p_sw,
						      uint8_t block_num);

typedef struct osm_nvlink_prtn_sw_data {
	ib_rail_filter_config_t subn_rail_filter[OSM_NVLINK_SN_NUM_RAILS][OSM_RAIL_FILTER_INGRESS_NUM_BLOCKS][OSM_RAIL_FILTER_EGRESS_NUM_BLOCKS];
	ib_rail_filter_config_t calc_rail_filter[OSM_NVLINK_SN_NUM_RAILS][OSM_RAIL_FILTER_INGRESS_NUM_BLOCKS][OSM_RAIL_FILTER_EGRESS_NUM_BLOCKS];
	boolean_t is_laas_set;
} osm_nvlink_prtn_sw_data_t;
/*
 * Note rail filter holds both trunk and access links of the partition
 *
 * is_laas_set
 * 	Boolean indicates whether switch was set to LAAS.
 */


typedef enum osm_nvlink_prtn_state {
	NVL_PRTN_STATE_MAINTAIN,
	NVL_PRTN_STATE_REROUTE_REQUESTED,
	NVL_PRTN_STATE_REROUTE,
	NVL_PRTN_STATE_ERROR,
} osm_nvlink_prtn_state_t;
/*
 * NVL_PRTN_STATE_MAINTAIN
 * 	Partition created, no issue detected (consider what about error status?)
 *
 * NVL_PRTN_STATE_REROUTE_REQUESTED
 *	Partition is within reroute process due to partition reroute request received from GFM.
 *
 * NVL_PRTN_STATE_REROUTE
 * 	All partition ports are set to Contain and Drain as part of reroute request, meaning
 * 	SM can update partition routing.
 *
 * NVL_PRTN_STATE_ERROR
 *	Partition defined in error state if any partition configuration MAD was not set successfully.
 *	SM detect it by routing validation step.
 */

typedef struct osm_nvlink_prtn {
	cl_map_item_t map_item;
	ib_net16_t pkey;
	cl_map_t sw_tbl;
	osm_nvlink_prtn_state_t state;
	uint8_t max_trunk_links;
	uint8_t num_trunk_used;
	boolean_t reroute_requested;
	uint8_t num_req_guids;
} osm_nvlink_prtn_t;
/*
* FIELDS
*	sw_tbl
*		Container of switch GUIDs of all the switchs that are part of the partition (used for
*		connecting between partition members).
*
* 	state
* 		Partition state indicates whether partition is in error state, maintained, or on
* 		any step of rerouting flow.
*
* SEE ALSO
*	osm_prtn_reroute_partition, osm_prtn_reroute_partition_finish
*********/

void osm_nvlink_rail_filter_config_mark_port_mask(ib_rail_filter_config_t * p_rail_filter_config,
						  osm_nvlink_port_type_t port_type,
						  uint8_t port_num,
						  uint8_t mask_block_num,
						  boolean_t is_add_request);

boolean_t osm_nvlink_switch_is_rail_used(osm_nvlink_sw_data_t * p_nvlink_sw_data, uint8_t rail);
void osm_nvlink_switch_mark_rail_used(osm_nvlink_sw_data_t * p_nvlink_sw_data, uint8_t rail);

void osm_nvlink_rail_assignment(osm_subn_t * p_subn);

void osm_nvlink_done_process_new_gpus(struct osm_sm * sm);

END_C_DECLS
