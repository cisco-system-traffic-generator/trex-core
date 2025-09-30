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
 * 	Declaration of osm_switch_t.
 *	This object represents an IBA switch.
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
#include <opensm/osm_advanced_routing.h>
#include <opensm/osm_nvlink.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Switch
* NAME
*	Switch
*
* DESCRIPTION
*	The Switch object encapsulates the information needed by the
*	OpenSM to manage switches.  The OpenSM allocates one switch object
*	per switch in the IBA subnet.
*
*	The Switch object is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

#define AR_MAX_GROUPS		4096

#define OSM_SW_NO_NEIGHBORHOOD_ID	0xffff

/****d* OpenSM: osm_fwd_state_enum
* NAME
*	osm_fwd_state_enum
*
* DESCRIPTION
*	States of osm_fwd_state_t object.
*
* SYNOPSIS
*/
typedef enum osm_fwd_state_enum {
	OSM_FWD_STATE_INIT,
	OSM_FWD_STATE_LFT,
	OSM_FWD_STATE_AR_GROUP_TABLE,
	OSM_FWD_STATE_AR_COPY_GROUP_TABLE,
	OSM_FWD_STATE_ARLFT,
	OSM_FWD_STATE_RN_SUB_GROUP_DIRECTION,
	OSM_FWD_STATE_WHBF,
	OSM_FWD_STATE_END,
} osm_fwd_state_enum;
/***********/

/****s* OpenSM: Switch/osm_fwd_state_t
* NAME
*	osm_fwd_state_t
*
* DESCRIPTION
*	Switch forwarding tables configuration state machine.
*
* 	This object holds state of forwarding tables configuration state and
* 	retrieve routing configuration MADs needed to be sent to the switch
* 	in the current state.
*
*	This object is used and manipulated by ucast manager and SMP receiver
*	workers handling routing table MADs.
*
* SYNOPSIS
*/
typedef struct osm_fwd_state {
	osm_fwd_state_enum state;
	union {
		struct lfts_ {
			uint16_t block_idx;
		} lfts;
		struct ar_group_table_ {
			uint16_t block_idx;
		} ar_group_table;
		struct ar_copy_group_table_ {
			uint16_t offset;
			uint16_t entry_offset;
			uint16_t from_group;
			uint16_t to_count[AR_MAX_GROUPS];
			uint16_t to_table[AR_MAX_GROUPS];
		} ar_copy_group_table;
		struct ar_lfts_ {
			uint16_t block_idx;
			uint8_t plft_idx;
		} ar_lfts;
		struct rn_sub_group_directions_ {
			uint16_t block_idx;
		} rn_sub_group_directions;
		struct whbf_ {
			uint16_t block_idx;
		} whbf;
	} modifier;
	uint8_t force_resend;
} osm_fwd_state_t;
/*
* FIELDS
*	state
*		Indicates current state.
*
*	modifier
*		Modifier in current state.
*
*	force_resend
*		Indicates that all routing information should be sent to the
*		switch.
*		otherwise, only changed blocks/configuration are sent.
*/

typedef struct osm_cc_switch {
	ib_cc_sw_general_settings_t calc_set_sw_general_settings;
	ib_cc_sw_general_settings_t subn_set_sw_general_settings;
	ib_cc_sw_general_settings_t subn_get_sw_general_settings;
	boolean_t is_vl_conf_for_all_ports;
	boolean_t is_sl_conf_for_all_ports;
	uint16_t resend_mask;
	boolean_t calculated;
} osm_cc_switch_data_t;

/****s* OpenSM: Switch/osm_fast_recovery_sw_data_t
* NAME
*	osm_fast_recovery_sw_data_t
*
* DESCRIPTION
*	Fast Recovery relevant parameters for switch
*
* SYNOPSIS
*/
typedef struct osm_fast_recovery_sw_data {
	ib_credit_watchdog_config_t subn_credit_watchdog[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_credit_watchdog_config_t calc_credit_watchdog[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_ber_config_t subn_ber_config[IB_BER_TYPE_MAX][IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_ber_config_t calc_ber_config[IB_BER_TYPE_MAX][IB_PROFILE_CONFIG_NUM_PROFILES];
	boolean_t subn_default_thr[IB_BER_TYPE_MAX][IB_PROFILE_CONFIG_NUM_PROFILES];
	boolean_t calc_default_thr[IB_BER_TYPE_MAX][IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_profiles_config_t ports_profiles[IB_PROFILE_CONFIG_NUM_BLOCKS];
	uint8_t profiles_configured;
	uint8_t max_profile;
} osm_fast_recovery_sw_data_t;
/*
* FIELDS
*	ports_profiles
*		ProfilesConfig blocks data as latest received from subnet
*
*	profiles_configured
*		Bitmask indicates whether profiles were configured for this feature.
*		If were not, avoids sending feature MADs that are sent per profile.
*
*/

/****s* OpenSM: Switch/osm_port_recovery_policy_sw_data_t
* NAME
*	osm_port_recovery_policy_sw_data_t
*
* DESCRIPTION
*	Port Recovery Policy relevant parameters for switch
*
* SYNOPSIS
*/
typedef struct osm_port_recovery_policy_sw_data {
	ib_port_recovery_policy_config_t subn_port_recovery_policy[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_port_recovery_policy_config_t calc_port_recovery_policy[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_profiles_config_t ports_profiles[IB_PROFILE_CONFIG_NUM_BLOCKS];
	uint8_t port_recovery_policy_updated;
	uint8_t subn_profiles_updated;
	uint8_t profiles_configured;
	uint8_t max_profile;
} osm_port_recovery_policy_sw_data_t;
/*
* FIELDS
*	subn_port_recovery_policy
*		Subnet port recovery policy configuration
*
*	calc_port_recovery_policy
*		Calculated port recovery policy configuration
*
*	ports_profiles
*		ProfilesConfig blocks data as latest received from subnet
*
*	port_recovery_policy_updated
*		Bitmask indicates whether configuration get MADs per profile were sent for this feature.
*
*	subn_profiles_updated
*		Bitmask indicates whether profiles get MADs were sent for this feature.
*
*	profiles_configured
*		Bitmask indicates whether profiles were configured for this feature.
*		If were not, avoids sending feature MADs that are sent per profile.
*
*	max_profile
*		The maximal port recovery policy profile to configure on the switch
*
*/

/****s* OpenSM: Switch/osm_qos_config_vl_sw_data_t
* NAME
*	osm_qos_config_vl_sw_data_t
*
* DESCRIPTION
*	QosConfigVL relevant parameters for switch
*
* SYNOPSIS
*/
typedef struct osm_qos_config_vl_sw_data {
	ib_qos_config_vl_t subn_qos_config_vl[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_qos_config_vl_t calc_qos_config_vl[IB_PROFILE_CONFIG_NUM_PROFILES];
	ib_profiles_config_t ports_profiles[IB_PROFILE_CONFIG_NUM_BLOCKS];
	uint8_t profiles_configured;
	boolean_t qos_config_vl_need_update;
	uint8_t max_profile;
} osm_qos_config_vl_sw_data_t;
/*
* FIELDS
*	ports_profiles
*		ProfilesConfig blocks data as latest received from subnet
*
*	profiles_configured
*		Bitmask indicates whether profiles were configured for this feature.
*		If were not, avoids sending feature MADs that are sent per profile.
*
*	qos_config_vl_need_update
*		Indicates whether QoSConfigValues values should be sent.
*
*/

typedef struct osm_conf_params_sw {
	ib_qos_config_vl_t qos_config_vl[IB_PROFILE_CONFIG_NUM_PROFILES];
	uint8_t qos_config_vl_max_profile;
	ib_port_recovery_policy_config_t port_recovery_policy_config[IB_PROFILE_CONFIG_NUM_PROFILES];
	uint8_t port_recovery_policy_max_profile;
} osm_conf_params_switch_t;

/****s* OpenSM: Switch/osm_switch_t
* NAME
*	osm_switch_t
*
* DESCRIPTION
*	Switch structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_switch {
	cl_map_item_t map_item;
	cl_list_item_t list_item;
	osm_node_t *p_node;
	ib_switch_info_t switch_info;
	ib_extended_switch_info_t subn_ext_switch_info;
	ib_extended_switch_info_t calc_ext_switch_info;
	boolean_t esi_updated;
	uint16_t max_lid_ho;
	uint8_t num_ports;
	uint16_t num_hops;
	uint8_t **hops;
	osm_port_profile_t *p_prof;
	uint8_t *search_ordering_ports;
	uint8_t *lft;
	uint8_t *new_lft;
	uint16_t lft_size;
	osm_mcast_tbl_t mcast_tbl;
	int32_t mft_block_num;
	uint32_t mft_position;
	unsigned endport_links;
	unsigned need_update;
	void *priv;
	int re_index;
	int graph_index[IB_MAX_NUM_OF_PLANES_XDR + 1];
	void *re_priv;
	void *multicast_priv;
	cl_map_item_t mgrp_item;
	uint32_t num_of_mcm;
	uint8_t is_mc_member;
	unsigned ar_configured;
	uint32_t mcast_route_weight;
	uint32_t mlid_root_num;
	uint8_t rank;
	uint8_t previous_rank;
	uint8_t an_port_num;
	uint16_t coord;
	osm_advanced_routing_data_t ar_data;
	osm_fwd_state_t fwd_state;
	osm_cc_switch_data_t cc_data;
	boolean_t send_port_info;
	uint64_t id;
	boolean_t pfrn_configured;
	osm_fast_recovery_sw_data_t fast_recovery_data;
	osm_port_recovery_policy_sw_data_t port_recovery_policy_data;
	osm_nvlink_sw_data_t nvlink_sw_data;
	osm_qos_config_vl_sw_data_t qos_config_vl_sw_data;
	uint8_t plane_mask;
	osm_conf_params_switch_t conf_params;
	boolean_t switch_reset;
} osm_switch_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	p_node
*		Pointer to the Node object for this switch.
*
*	switch_info
*		IBA defined SwitchInfo structure for this switch.
*
*	subn_ext_switch_info
*		Copy of latest ExtendedSwitchInfo received from the subnet for this switch.
*
*	calc_ext_switch_info
*		Latest ExtendedSwitchInfo calculated for this switch.
*
*	esi_updated
*		Indicator, indicates that the node holds an updated copy of extended switch info
*		received from subnet.
*
*	max_lid_ho
*		Max LID that is accessible from this switch.
*
*	num_ports
*		Number of ports for this switch.
*
*	num_hops
*		Size of hops table for this switch.
*
*	hops
*		LID Matrix for this switch containing the hop count to every LID from every port.
*		Note some routing engines fill hops array partially (e.g only hops towards other
*		switches), so later SM relies on remote switch lids for the missing CA/RTR lids.
*
*	p_prof
*		Pointer to array of Port Profile objects for this switch.
*
*	lft
*		This switch's linear forwarding table.
*
*	new_lft
*		This switch's linear forwarding table, as was
*		calculated by the last routing engine execution.
*
*	mcast_tbl
*		Multicast forwarding table for this switch.
*
*	need_update
*		When set indicates that switch was probably reset, so
*		fwd tables and rest cached data should be flushed
*
*	re_index
*		Switch index for routing engine usage.
*
*	re_priv
*		Private area for routing engines.
*
*	multicast_priv
*		Pointer for multicast routing algorithm to use to hold
*		temporary information about this switch.
*		This pointer should be set to NULL at the end of the routing.
*
*	mgrp_item
*		map item for switch in building mcast tree
*
*	num_of_mcm
*		number of mcast members(ports) connected to switch
*
*	is_mc_member
*		whether switch is a mcast member itself
*
*	ar_configured
*		indicates if adaptive routing is configured
*
*	mlid_root_num
*		number of MLIDs where the switch is a root
*
*	rank
*		rank of the switch as calculated by routing algorithm
*
*	previous_rank
*		Rank of the switch as caluclated in previous routing.
*
*	an_port_num
*		the port number through which sharp aggregation node is connected,
*		or OSM_NO_PATH (0xFF) if no sharp aggregation node exists.
*
* 	coord
* 		Coordinate of switch (applicable for hypercube and DFP )
*
* 	ar_data
* 		Advanced routing configuration and capabilities of the switch.
*
* 	fwd_state
* 		Forwarding tables configuration state machine of the switch.
*
* 	send_port_info
* 		Indicates if OpenSM needs to send portInfo to external ports and not SPST
* 	id
* 		The id from ids_guid_file file. If the file is not present, holds switch node GUID.
* 		Used by tree based routing engines to determine direction between two neighbor
* 		switches of the same rank.
*
*	pfrn_configured
*		Boolean indicates whether pFRNConfig was not configured, hence need to resend on
*		next sweep.
*
*	fast_recovery_data
*		Structure used for saving latest Fast Recovery MADs data that was configured on
*		this node, in order to check if should resend on next calculation.
*
*	port_recovery_policy_data
*		Structure used for saving latest Port Recovery Policy MADs data that was configured on
*		this node, in order to check if should resend on next calculation.
*
* 	nvlink_sw_data
*		Structure used for holding NVLink configuration data that is configured per switch.
*
* 	qos_config_vl_sw_data
*		Structure used for holding QoSConfigVL configuration data that is configured per switch.
*
*	plane_mask
*		A bitmask with all the planes that ports of this switch have
*
*	conf_params
*		Values for parameters that are prioritized for the ports belong
*		to this switch.
*
*	switch_reset
*		Flag indicating the switch was reset. Cleared every heavy sweep.
*
* SEE ALSO
*	Switch object
*********/

/****d* OpenSM: Switch/OSM_SW_NO_RANK
* NAME
*	OSM_SW_NO_RANK
*
* DESCRIPTION
*	Value indicating the switch rank was not calculated.
*
* SYNOPSIS
*/

#define OSM_SW_NO_RANK	0xFF
/**********/

/****d* OpenSM: Switch/OSM_SW_NO_COORD
* NAME
*	OSM_SW_NO_COORD
*
* DESCRIPTION
*	Value indicating the switch coordinate for was not calculated.
*
* SYNOPSIS
*/

#define OSM_SW_NO_COORD	0xFFFF
/**********/

/****s* OpenSM: Switch/struct osm_remote_guids_count
* NAME
*	struct osm_remote_guids_count
*
* DESCRIPTION
*	Stores array of pointers to remote node and the numbers of
*	times a switch has forwarded to it.
*
* SYNOPSIS
*/
struct osm_remote_guids_count {
	unsigned count;
	struct osm_remote_node {
		osm_node_t *node;
		unsigned forwarded_to;
		uint8_t port;
	} guids[0];
};
/*
* FIELDS
*	count
*		A number of used entries in array.
*
*	node
*		A pointer to node.
*
*	forwarded_to
*		A count of lids forwarded to this node.
*
*	port
*		Port number on the node.
*********/

/****s* OpenSM: Switch/osm_held_back_switch_t
* NAME
*	osm_held_back_switch_t
*
* DESCRIPTION
*	Represent held back switch.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
#define OSM_HB_STATUS_INITIAL		0
#define OSM_HB_STATUS_NEW		1
#define OSM_HB_STATUS_EXIST		2
typedef struct osm_held_back_switch {
	cl_map_item_t map_item;
	osm_node_t* p_node;
	uint8_t status;
} osm_held_back_switch_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	p_node
*		pointer to the switch node
*
*	status
*		Used when uploading held back configuration:
*		At load start we mark all held back switches in OSM_HB_STATUS_INITIAL state
*		Every new held back switch (found in configuration but not in table)
*			is added with status OSM_HB_STATUS_NEW
*		Every exist held back switch (found in configuration and in the table)
*			is marked in OSM_HB_STATUS_EXIST state
*		At the end of loading every held back switch that is left with
*			HB_STATUS_INITIAL state is recognized as one that was removed from
*			configuration and therefore we delete it from the table
*/

/****s* OpenSM: Switch/osm_sw_ar_configuration_type_t
* NAME
*	osm_sw_ar_configuration_type_t
*
* DESCRIPTION
* 	Enumeration of AR configuration types for osm_switch_t.ar_configured
*
* SYNOPSIS
*
*/
typedef enum osm_sw_ar_configuration_type {
	OSM_SW_AR_CONF_NONE = 0,
	OSM_SW_AR_CONF_PLUGIN = 1,
	OSM_SW_AR_CONF_DISABLED = 2,
	OSM_SW_AR_CONF_CONFIGURED = 3
} osm_sw_ar_configuration_type;
/*
* VALUES
*	OSM_SW_AR_CONF_NONE
*		No advanced routing configured (unicast routing using LFT).
*
* 	OSM_SW_AR_CONF_PLUGIN
* 		Routing tables configured and assigned to the switch by plug-in.
*
* 	OSM_SW_AR_DISABLED
* 		Indicates that advanced routing configuration disable, and need
* 		to set unicast routing configuration using LFT tables.
*
* 	OSM_SW_AR_CONFIGURED
* 		Advanced routing configured.
*****/

/****f* OpenSM: Switch/osm_switch_name
* NAME
*	osm_switch_name
*
* DESCRIPTION
*	Get the 'name' of the switch.
*
* SYNOPSIS
*/
static inline char * osm_switch_name(IN osm_switch_t * p_sw)
{
	return p_sw->p_node->print_desc;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the object.
*
* RETURN VALUE
*	The nodeInfo description.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_delete
* NAME
*	osm_switch_delete
*
* DESCRIPTION
*	Destroys and deallocates the object.
*
* SYNOPSIS
*/
void osm_switch_delete(IN OUT osm_switch_t ** pp_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*	Switch object, osm_switch_new
*********/

/****f* OpenSM: Switch/osm_switch_new
* NAME
*	osm_switch_new
*
* DESCRIPTION
*	The osm_switch_new function initializes a Switch object for use.
*
* SYNOPSIS
*/
osm_switch_t *osm_switch_new(IN osm_node_t * p_node,
			     IN const osm_madw_t * p_madw);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to the node object of this switch
*
*	p_madw
*		[in] Pointer to the MAD Wrapper containing the switch's
*		SwitchInfo attribute.
*
* RETURN VALUES
*	Pointer to the new initialized switch object.
*
* NOTES
*
* SEE ALSO
*	Switch object, osm_switch_delete
*********/

/****f* OpenSM: Switch/osm_switch_get_hop_count
* NAME
*	osm_switch_get_hop_count
*
* DESCRIPTION
*	Returns the hop count at the specified LID/Port intersection.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_hop_count(IN const osm_switch_t * p_sw,
					       IN uint16_t lid_ho,
					       IN uint8_t port_num)
{
	return (lid_ho > p_sw->max_lid_ho || !p_sw->hops[lid_ho]) ?
	    OSM_NO_PATH : p_sw->hops[lid_ho][port_num];
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to a Switch object.
*
*	lid_ho
*		[in] LID value (host order) for which to return the hop count
*
*	port_num
*		[in] Port number in the switch
*
* RETURN VALUES
*	Returns the hop count at the specified LID/Port intersection.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_set_hops
* NAME
*	osm_switch_set_hops
*
* DESCRIPTION
*	Sets the hop count at the specified LID/Port intersection.
*
* SYNOPSIS
*/
cl_status_t osm_switch_set_hops(IN osm_switch_t * p_sw, IN uint16_t lid_ho,
				IN uint8_t port_num, IN uint8_t num_hops);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to a Switch object.
*
*	lid_ho
*		[in] LID value (host order) for which to set the count.
*
*	port_num
*		[in] port number for which to set the count.
*
*	num_hops
*		[in] value to assign to this entry.
*
* RETURN VALUES
*	Returns 0 if successful. -1 if it failed
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_clear_hops
* NAME
*	osm_switch_clear_hops
*
* DESCRIPTION
*	Cleanup existing hops tables (lid matrix)
*
* SYNOPSIS
*/
void osm_switch_clear_hops(IN osm_switch_t * p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to a Switch object.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_clear_new_lfts
* NAME
*	osm_switch_clear_new_lfts
*
* DESCRIPTION
*	Cleanup existing hops tables (lid matrix)
*
* SYNOPSIS
*/
void osm_switch_clear_new_lfts(IN osm_switch_t * p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to a Switch object.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_least_hops
* NAME
*	osm_switch_get_least_hops
*
* DESCRIPTION
*	Returns the number of hops in the short path to this lid from
*	any port on the switch.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_least_hops(IN const osm_switch_t * p_sw,
						IN uint16_t lid_ho)
{
	return (lid_ho > p_sw->max_lid_ho || !p_sw->hops[lid_ho]) ?
	    OSM_NO_PATH : p_sw->hops[lid_ho][0];
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	lid_ho
*		[in] LID (host order) for which to retrieve the shortest hop count.
*
* RETURN VALUES
*	Returns the number of hops in the short path to this lid from
*	any port on the switch.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_get_port_least_hops
* NAME
*	osm_switch_get_port_least_hops
*
* DESCRIPTION
*	Returns the number of hops in the short path to this port from
*	any port on the switch.
*
* SYNOPSIS
*/
uint8_t osm_switch_get_port_least_hops(IN const osm_switch_t * p_sw,
				       IN const osm_port_t * p_port);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	p_port
*		[in] Pointer to an osm_port_t object for which to
*		retrieve the shortest hop count.
*
* RETURN VALUES
*	Returns the number of hops in the short path to this lid from
*	any port on the switch.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****d* OpenSM: osm_lft_type_enum
* NAME
*	osm_lft_type_enum
*
* DESCRIPTION
*	Enumerates LFT sets types of a switch.
*
* SYNOPSIS
*/
typedef enum osm_lft_type_enum {
	OSM_LFT = 0,
	OSM_NEW_LFT
} osm_lft_type_enum;
/***********/

/****f* OpenSM: Switch/osm_switch_get_port_by_lid
* NAME
*	osm_switch_get_port_by_lid
*
* DESCRIPTION
*	Returns the switch port number on which the specified LID is routed.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_port_by_lid(IN const osm_switch_t * p_sw,
						 IN uint16_t lid_ho,
						 IN osm_lft_type_enum lft_enum)
{
	uint16_t lft_size = (lid_ho / IB_SMP_DATA_SIZE + 1) * IB_SMP_DATA_SIZE;

	if (lid_ho == 0 || p_sw->lft_size < lft_size)
		return OSM_NO_PATH;
	return lft_enum == OSM_LFT ? p_sw->lft[lid_ho] : p_sw->new_lft[lid_ho];
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	lid_ho
*		[in] LID (host order) for which to retrieve the shortest hop count.
*
*	lft_enum
*		[in] Use LFT that was calculated by routing engine, or current
*		LFT on the switch.
*
* RETURN VALUES
*	Returns the switch port on which the specified LID is routed.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_get_route_by_lid
* NAME
*	osm_switch_get_route_by_lid
*
* DESCRIPTION
*	Gets the physical port object that routes the specified LID. as it was
*	calculated by the routing engine.
*
* SYNOPSIS
*/
static inline osm_physp_t *osm_switch_get_route_by_lid(IN const osm_switch_t *
						       p_sw, IN ib_net16_t lid)
{
	uint8_t port_num;

	OSM_ASSERT(p_sw);
	OSM_ASSERT(lid);

	port_num = osm_switch_get_port_by_lid(p_sw, cl_ntoh16(lid),
					      OSM_NEW_LFT);

	/*
	   In order to avoid holes in the subnet (usually happens when
	   running UPDN algorithm), i.e. cases where port is
	   unreachable through a switch (we put an OSM_NO_PATH value at
	   the port entry, we do not assert on unreachable lid entries
	   at the fwd table but return NULL
	 */
	if (port_num != OSM_NO_PATH)
		return (osm_node_get_physp_ptr(p_sw->p_node, port_num));
	else
		return NULL;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	lid
*		[in] LID for which to find a route.  This must be a unicast
*		LID value < 0xC000.
*
* RETURN VALUES
*	Returns a pointer to the Physical Port Object object that
*	routes the specified LID.  A return value of zero means
*	there is no route for the lid through this switch.
*	The lid value must be a unicast LID.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_sp0_is_lmc_capable
* NAME
*	osm_switch_sp0_is_lmc_capable
*
* DESCRIPTION
*	Returns whether switch port 0 (SP0) can support LMC
*
*/
static inline unsigned
osm_switch_sp0_is_lmc_capable(IN const osm_switch_t * p_sw,
			      IN const osm_subn_t * p_subn)
{
	return (p_subn->opt.lmc_esp0 &&
		ib_switch_info_is_enhanced_port0(&p_sw->switch_info)) ? 1 : 0;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
* RETURN VALUES
*	TRUE if SP0 is enhanced and globally enabled. FALSE otherwise.
*
* NOTES
*	This is workaround function, it takes into account user defined
*	p_subn->opt.lmc_esp0 parameter.
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_max_block_id_in_use
* NAME
*	osm_switch_get_max_block_id_in_use
*
* DESCRIPTION
*	Returns the maximum block ID (host order) of this switch that
*	is used for unicast routing.
*
* SYNOPSIS
*/
static inline uint16_t
osm_switch_get_max_block_id_in_use(IN const osm_switch_t * p_sw)
{
	return cl_ntoh16(p_sw->switch_info.lin_top) / IB_SMP_DATA_SIZE;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
* RETURN VALUES
*	Returns the maximum block ID (host order) of this switch.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_get_lft_block
* NAME
*	osm_switch_get_lft_block
*
* DESCRIPTION
*	Retrieve a linear forwarding table block, as it was calculated by
*	the routing engine.
*
* SYNOPSIS
*/
boolean_t osm_switch_get_lft_block(IN const osm_switch_t * p_sw,
				   IN uint16_t block_id, OUT uint8_t * p_block);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	block_ID
*		[in] The block_id to retrieve.
*
*	p_block
*		[out] Pointer to the 64 byte array to store the
*		forwarding table block specified by block_id.
*
* RETURN VALUES
*	Returns true if there are more blocks necessary to
*	configure all the LIDs reachable from this switch.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_supports_mcast
* NAME
*	osm_switch_supports_mcast
*
* DESCRIPTION
*	Indicates if a switch supports multicast.
*
* SYNOPSIS
*/
static inline boolean_t osm_switch_supports_mcast(IN const osm_switch_t * p_sw)
{
	return (p_sw->switch_info.mcast_cap != 0);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
* RETURN VALUES
*	Returns TRUE if the switch supports multicast.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_set_switch_info
* NAME
*	osm_switch_set_switch_info
*
* DESCRIPTION
*	Updates the switch info attribute of this switch.
*
* SYNOPSIS
*/
static inline void osm_switch_set_switch_info(IN osm_switch_t * p_sw,
					      IN const ib_switch_info_t * p_si)
{
	OSM_ASSERT(p_sw);
	OSM_ASSERT(p_si);
	p_sw->switch_info = *p_si;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to a Switch object.
*
*	p_si
*		[in] Pointer to the SwitchInfo attribute for this switch.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_count_path
* NAME
*	osm_switch_count_path
*
* DESCRIPTION
*	Counts this path in port profile.
*
* SYNOPSIS
*/
static inline void osm_switch_count_path(IN osm_switch_t * p_sw,
					 IN uint8_t port)
{
	osm_port_prof_path_count_inc(&p_sw->p_prof[port]);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	port
*		[in] Port to count path.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_set_lft_block
* NAME
*	osm_switch_set_lft_block
*
* DESCRIPTION
*	Copies in the specified block into
*	the switch's Linear Forwarding Table.
*
* SYNOPSIS
*/
static inline ib_api_status_t
osm_switch_set_lft_block(IN osm_switch_t * p_sw, IN const uint8_t * p_block,
			 IN uint32_t block_num)
{
	uint16_t lid_start =
		(uint16_t) (block_num * IB_SMP_DATA_SIZE);
	OSM_ASSERT(p_sw);

	if (lid_start + IB_SMP_DATA_SIZE > p_sw->lft_size)
		return IB_INVALID_PARAMETER;

	memcpy(&p_sw->lft[lid_start], p_block, IB_SMP_DATA_SIZE);
	return IB_SUCCESS;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	p_block
*		[in] Pointer to the forwarding table block.
*
*	block_num
*		[in] Block number for this block
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_set_mft_block
* NAME
*	osm_switch_set_mft_block
*
* DESCRIPTION
*	Sets a block of multicast port masks into the multicast table.
*
* SYNOPSIS
*/
static inline ib_api_status_t
osm_switch_set_mft_block(IN osm_switch_t * p_sw, IN const ib_net16_t * p_block,
			 IN uint16_t block_num, IN uint8_t position)
{
	OSM_ASSERT(p_sw);
	return osm_mcast_tbl_set_block(&p_sw->mcast_tbl, p_block, block_num,
				       position);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	p_block
*		[in] Pointer to the block of port masks to set.
*
*	block_num
*		[in] Block number (0-511) to set.
*
*	position
*		[in] Port mask position (0-15) to set.
*
* RETURN VALUE
*	IB_SUCCESS on success.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_mft_block
* NAME
*	osm_switch_get_mft_block
*
* DESCRIPTION
*	Retrieve a block of multicast port masks from the multicast table.
*
* SYNOPSIS
*/
static inline boolean_t osm_switch_get_mft_block(IN osm_switch_t * p_sw,
						 IN uint16_t block_num,
						 IN uint8_t position,
						 OUT ib_net16_t * p_block)
{
	OSM_ASSERT(p_sw);
	return osm_mcast_tbl_get_block(&p_sw->mcast_tbl, block_num, position,
				       OSM_MCAST_TBL_ALL_ENTRIES, p_block);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	block_num
*		[in] Block number (0-511) to set.
*
*	position
*		[in] Port mask position (0-15) to set.
*
*	p_block
*		[out] Pointer to the block of port masks stored.
*
* RETURN VALUES
*	Returns true if there are more blocks necessary to
*	configure all the MLIDs reachable from this switch.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_mft_max_block
* NAME
*	osm_switch_get_mft_max_block
*
* DESCRIPTION
*       Get the max_block from the associated multicast table.
*
* SYNOPSIS
*/
static inline uint16_t osm_switch_get_mft_max_block(IN osm_switch_t * p_sw)
{
	OSM_ASSERT(p_sw);
	return osm_mcast_tbl_get_max_block(&p_sw->mcast_tbl);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*/

/****f* OpenSM: Switch/osm_switch_get_mft_max_block_in_use
* NAME
*	osm_switch_get_mft_max_block_in_use
*
* DESCRIPTION
*	Get the max_block_in_use from the associated multicast table.
*
* SYNOPSIS
*/
static inline int16_t osm_switch_get_mft_max_block_in_use(IN osm_switch_t * p_sw)
{
	OSM_ASSERT(p_sw);
	return osm_mcast_tbl_get_max_block_in_use(&p_sw->mcast_tbl);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUES
*	Returns the maximum block ID in use in this switch's mcast table.
*	A value of -1 indicates no blocks are in use.
*
* NOTES
*
* SEE ALSO
*/

/****f* OpenSM: Switch/osm_switch_get_mft_max_position
* NAME
*	osm_switch_get_mft_max_position
*
* DESCRIPTION
*       Get the max_position from the associated multicast table.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_mft_max_position(IN osm_switch_t * p_sw)
{
	OSM_ASSERT(p_sw);
	return osm_mcast_tbl_get_max_position(&p_sw->mcast_tbl);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*/

/****f* OpenSM: Switch/osm_switch_get_dimn_port
* NAME
*	osm_switch_get_dimn_port
*
* DESCRIPTION
*	Get the routing ordered port. Can be used only
*	if p_sw provided is guaranteed not NULL.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_dimn_port(IN const osm_switch_t * p_sw,
					       IN uint8_t port_num)
{
	if (p_sw->search_ordering_ports == NULL)
		return port_num;
	return p_sw->search_ordering_ports[port_num];
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	port_num
*		[in] Port number in the switch
*
* RETURN VALUES
*	Returns the port number ordered for routing purposes.
*/

/****f* OpenSM: Switch/osm_switch_get_rev_dimn_port
* NAME
*	osm_switch_get_rev_dimn_port
*
* DESCRIPTION
*	Get the search order of a specified port number.
*	Can be used only if p_sw provided is guaranteed not NULL.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_rev_dimn_port(IN const osm_switch_t * p_sw,
						   IN uint8_t port_num)
{
	uint8_t port_order;
	if (p_sw->search_ordering_ports == NULL)
		return port_num;
	for (port_order = 0; port_order < p_sw->num_ports; port_order++) {
		if (p_sw->search_ordering_ports[port_order] == port_num)
				return port_order;
	}
	return OSM_NO_PATH;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	port_num
*		[in] Port number in the switch
*
* RETURN VALUES
*	Returns the search order enumeration for the specified port number.
*/

/****f* OpenSM: Switch/osm_switch_recommend_path
* NAME
*	osm_switch_recommend_path
*
* DESCRIPTION
*	Returns the recommended port on which to route this LID.
*	In cases where LMC > 0, the remote side system and node
*	used for the routing are tracked in the provided arrays
*	(and counts) such that other lid for the same port will
*	try and avoid going through the same remote system/node.
*
* SYNOPSIS
*/
uint8_t osm_switch_recommend_path(IN const osm_switch_t * p_sw,
				  IN osm_port_t * p_port, IN uint16_t lid_ho,
				  IN unsigned start_from,
				  IN boolean_t ignore_existing,
				  IN boolean_t routing_for_lmc,
				  IN boolean_t dor,
				  IN boolean_t port_shifting,
				  IN struct random_data *scatter_buf,
				  IN osm_lft_type_enum lft_enum,
				  IN boolean_t use_switch_weight);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	p_port
*		[in] Pointer to the port object for which to get a path
*		advisory.
*
*	lid_ho
*		[in] LID value (host order) for which to get a path advisory.
*
*	start_from
*		[in] Port number from where to start balance counting.
*
*	ignore_existing
*		[in] Set to cause the switch to choose the optimal route
*		regardless of existing paths.
*		If false, the switch will choose an existing route if one
*		exists, otherwise will choose the optimal route.
*
*	routing_for_lmc
*		[in] We support an enhanced LMC aware routing mode:
*		In the case of LMC > 0, we can track the remote side
*		system and node for all of the lids of the target
*		and try and avoid routing again through the same
*		system / node.
*
*		Assume if routing_for_lmc is TRUE that this procedure
*		was provided with the tracking array and counter via
*		p_port->priv, and we can conduct this algorithm.
*
*	dor
*		[in] If TRUE, Dimension Order Routing will be done.
*
*	port_shifting
*		[in] If TRUE, port_shifting will be done.
*
*	scatter_buf
*		[in] If not NULL, contains the data for randomization
*
*	lft_enum
*		[in] Use LFT that was calculated by routing engine, or current
*		LFT on the switch.
*
*	use_switch_weight
*		[in] No need to look for least loaded outport if switch weight
*		should not be taken into account
*
* RETURN VALUE
*	Returns the recommended port on which to route this LID.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_recommend_mcast_path
* NAME
*	osm_switch_recommend_mcast_path
*
* DESCRIPTION
*	Returns the recommended port on which to route this LID.
*
* SYNOPSIS
*/
uint8_t osm_switch_recommend_mcast_path(IN osm_switch_t * p_sw,
					IN osm_port_t * p_port,
					IN uint16_t mlid_ho,
					IN boolean_t ignore_existing);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
*	p_port
*		[in] Pointer to the port object for which to get
*		the multicast path.
*
*	mlid_ho
*		[in] MLID for the multicast group in question.
*
*	ignore_existing
*		[in] Set to cause the switch to choose the optimal route
*		regardless of existing paths.
*		If false, the switch will choose an existing route if one exists,
*		otherwise will choose the optimal route.
*
* RETURN VALUE
*	Returns the recommended port on which to route this LID.
*
* NOTES
*
* SEE ALSO
*********/

struct osm_mtree_node;
/****f* OpenSM: Switch/osm_switch_recommend_mcast_path_const
* NAME
*	osm_switch_recommend_mcast_path_const
*
* DESCRIPTION
*	Returns the recommended port on which to route this LID.
*
* SYNOPSIS
*/
uint8_t osm_switch_recommend_mcast_path_const(IN struct osm_mtree_node * p_mtn,
					IN osm_port_t * p_port,
					IN uint16_t mlid_ho,
					IN boolean_t ignore_existing);
/*
* PARAMETERS
*	p_mtn
*		[in] Pointer to the Mutical tree node object.
*
*	p_port
*		[in] Pointer to the port object for which to get
*		the multicast path.
*
*	mlid_ho
*		[in] MLID for the multicast group in question.
*
*	ignore_existing
*		[in] Set to cause the switch to choose the optimal route
*		regardless of existing paths.
*		If false, the switch will choose an existing route if one exists,
*		otherwise will choose the optimal route.
*
* RETURN VALUE
*	Returns the recommended port on which to route this LID.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_mcast_fwd_tbl_size
* NAME
*	osm_switch_get_mcast_fwd_tbl_size
*
* DESCRIPTION
*	Returns the number of entries available in the multicast forwarding table.
*
* SYNOPSIS
*/
static inline uint16_t
osm_switch_get_mcast_fwd_tbl_size(IN const osm_switch_t * p_sw)
{
	return cl_ntoh16(p_sw->switch_info.mcast_cap);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
* RETURN VALUE
*	Returns the number of entries available in the multicast forwarding table.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_path_count_get
* NAME
*	osm_switch_path_count_get
*
* DESCRIPTION
*	Returns the count of the number of paths going through this port.
*
* SYNOPSIS
*/
static inline uint32_t osm_switch_path_count_get(IN const osm_switch_t * p_sw,
						 IN uint8_t port_num)
{
	return osm_port_prof_path_count_get(&p_sw->p_prof[port_num]);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the Switch object.
*
*	port_num
*		[in] Port number for which to get path count.
*
* RETURN VALUE
*	Returns the count of the number of paths going through this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_prepare_path_rebuild
* NAME
*	osm_switch_prepare_path_rebuild
*
* DESCRIPTION
*	Prepares a switch to rebuild pathing information.
*
* SYNOPSIS
*/
int osm_switch_prepare_path_rebuild(IN osm_switch_t * p_sw,
				    IN uint16_t max_lids,
				    IN boolean_t clear_existing);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the Switch object.
*
*	max_lids
*		[in] Max number of lids in the subnet.
*
*	clear_existing
*		[in] Flag to clear existing data in hop tables
*		     Should be set to FALSE when adding vPORT LIDs
* RETURN VALUE
*	Returns zero on success, or negative value if an error occurred.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_mcast_tbl_ptr
* NAME
*	osm_switch_get_mcast_tbl_ptr
*
* DESCRIPTION
*	Returns a pointer to the switch's multicast table.
*
* SYNOPSIS
*/
static inline osm_mcast_tbl_t *osm_switch_get_mcast_tbl_ptr(IN const
							    osm_switch_t * p_sw)
{
	return (osm_mcast_tbl_t *) & p_sw->mcast_tbl;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
* RETURN VALUE
*	Returns a pointer to the switch's multicast table.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_is_in_mcast_tree
* NAME
*	osm_switch_is_in_mcast_tree
*
* DESCRIPTION
*	Returns true if this switch already belongs in the tree for the specified
*	multicast group.
*
* SYNOPSIS
*/
static inline boolean_t
osm_switch_is_in_mcast_tree(IN const osm_switch_t * p_sw, IN uint16_t mlid_ho)
{
	const osm_mcast_tbl_t *p_tbl;

	p_tbl = &p_sw->mcast_tbl;
	if (p_tbl)
		return osm_mcast_tbl_is_any_port(&p_sw->mcast_tbl, mlid_ho);
	else
		return FALSE;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	mlid_ho
*		[in] MLID (host order) of the multicast tree to check.
*
* RETURN VALUE
*	Returns true if this switch already belongs in the tree for the specified
*	multicast group.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_port0
* NAME
*	osm_switch_get_port0
*
* DESCRIPTION
*	Returns pointer to osm_port_t object of port 0 of the switch.
*
* SYNOPSIS
*/
static inline osm_port_t * osm_switch_get_port0(IN const osm_switch_t * p_sw,
						IN osm_subn_t * p_subn)
{
	osm_port_t * p_port =
	    osm_get_port_by_guid(p_subn,
				 p_sw->p_node->physp_table[0].port_guid);

	OSM_ASSERT(p_port);
	return p_port;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
* RETURN VALUE
*	Returns pointer to osm_port_t object of port 0 of the switch.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_is_isolated
* NAME
*	osm_switch_is_isolated
*
* DESCRIPTION
*	Returns true if the switch is isolated.
*
* SYNOPSIS
*/
static inline boolean_t osm_switch_is_isolated(IN const osm_switch_t * p_sw,
					      IN osm_subn_t * p_subn)
{
	return osm_port_is_isolated(osm_switch_get_port0(p_sw, p_subn));
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
* RETURN VALUE
*	Returns true if the switch is isolated.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_is_ar_supported
* NAME
*	osm_switch_is_ar_supported
*
* DESCRIPTION
*	Returns true if the switch is ar_supported.
*
* SYNOPSIS
*/
static inline boolean_t osm_switch_is_ar_supported(IN osm_switch_t *p_sw)
{
	return ib_general_info_is_cap_enabled(&p_sw->p_node->general_info, IB_GENERAL_INFO_ADAPTIVE_ROUTING_REV_SUPPORTED_BIT);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
* RETURN VALUE
*	Returns true if the switch is AR enabled.
*	Returns false if the switch is not AR enabled.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_apply_vlids_to_lft
* NAME
*	osm_switch_apply_vlids_to_lft
*
* DESCRIPTION
*	Add virtual lids to the lft table of the switch
*
* SYNOPSIS
*/
void osm_switch_apply_vlids_to_lft(osm_switch_t *p_sw, IN osm_subn_t *p_subn);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	p_mgr
*		[in] Pointer to osm_subn_t object.
*
* RETURN VALUE
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_has_cas
* NAME
*	osm_switch_has_cas
*
* DESCRIPTION
*	The function returns TRUE if there are any HCAs connected to the given switch,
*	FALSE otherwise.
*
* SYNOPSIS
*/
boolean_t osm_switch_has_cas(IN struct osm_switch *p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
* RETURN VALUE
*	TRUE if there are any HCAs connected to the given switch
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_minhop_port_count
* NAME
*	osm_switch_get_minhop_port_count
*
* DESCRIPTION
* 	The function gets the number of ports that have the same least number of hops to destination lid.
*
* SYNOPSIS
*/
uint8_t osm_switch_get_minhop_port_count(IN osm_switch_t *p_sw, IN uint16_t lid_ho);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*	lid_ho
*		[in] the destination lid to search for in the hops.
*
* RETURN VALUE
*	Number of ports that have the same least number of hops to destination lid.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_minhop_port_nums
* NAME
*	osm_switch_get_minhop_port_nums
*
* DESCRIPTION
* 	The function gets the port nums that have the same least number of hops to destination lid.
*
* SYNOPSIS
*/
uint8_t osm_switch_get_minhop_port_nums(IN osm_switch_t *p_sw, IN uint16_t lid_ho, OUT uint8_t *ports);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*	lid_ho
*		[in] the destination lid to search for in the hops.
*	ports
*		[out] array of ports
*
* RETURN VALUE
*	Number of ports that have the same least number of hops to destination lid.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_minhop_ports
* NAME
*	osm_switch_get_minhop_ports
*
* DESCRIPTION
* 	The function gets all the ports that have the same least number of hops to destination lid.
*
* SYNOPSIS
*/
uint8_t osm_switch_get_minhop_ports(IN osm_switch_t *p_sw, IN uint16_t lid_ho,
				    OUT osm_ar_subgroup_t *p_subgroup,
				    OUT uint8_t *port_from_group);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*	lid_ho
*		[in] the destination lid to search for in the hops.
*	p_subgroup
*		[out] the pointer to AR sub group to fill with ports.
*	port_from_group
*		[out] the pointer to return a port number of one of the ports that were added
*		to subgroup.
*
* RETURN VALUE
*	Number of ports that have the same least number of hops to destination lid.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_clear_and_get_minhop_ports
* NAME
*     osm_switch_clear_and_get_minhop_ports
*
* DESCRIPTION
*     The function clears the subgroup mask and then sets all the ports that have the same
*     least number of hops to destination lid.
*
* SYNOPSIS
*/
uint8_t osm_switch_clear_and_get_minhop_ports(IN osm_switch_t *p_sw,
					      IN uint16_t lid_ho,
					      OUT osm_ar_subgroup_t *p_subgroup,
					      OUT uint8_t *port_from_group);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*	lid_ho
*		[in] the destination lid to search for in the hops.
*	p_subgroup
*		[out] the pointer to AR sub group to fill with ports.
*	port_from_group
*		[out] the pointer to return a port number of one of the ports that were added
*		to subgroup.
*
* RETURN VALUE
*	Number of ports that have the same least number of hops to destination lid.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_match_filter
* NAME
*	osm_switch_match_filter
*
* DESCRIPTION
* 	Check if specified switch matche filter.
*
* SYNOPSIS
*/
static inline int osm_switch_match_filter(osm_switch_t *p_sw, osm_node_filter_t filter)
{
	return (filter == OSM_NODE_FILTER_ALL) ||
	       (filter == OSM_NODE_FILTER_CHANGED && p_sw->need_update) ||
	       (filter == OSM_NODE_FILTER_UNCHANGED && !p_sw->need_update);
}


/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	filter
*		[in] Filter
*
* RETURN VALUE
*	True if switch matches specified filter.
*
* NOTES
*
* SEE ALSO
*********/

boolean_t osm_switch_get_subn_profiles_updated(osm_switch_t * p_sw, uint8_t block,
					       ib_profiles_feature_t feature);

/****f* OpenSM: Switch/osm_switch_set_subn_profiles_updated
 * NAME
 *	osm_switch_set_subn_profiles_updated
 *
 * DESCRIPTION
 *	Mark ProfileConfig block of the feature as updated successfully, or as not
 *	updated yet, according to is_updated.
 *
 * SYNOPSIS
 */
void osm_switch_set_subn_profiles_updated(osm_switch_t * p_sw,
					  uint8_t block,
					  boolean_t is_updated,
					  ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
*	block
*		Block number
*
*	is_updated
*		Boolean indicates whether to mark subnet ProfilesConfig block of this feature
*		as updated or as not updated for this switch.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	None
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Switch/osm_switch_set_subn_profiles_updated
 * NAME
 *	osm_switch_set_subn_profiles_updated
 *
 * DESCRIPTION
 *	Mark ProfileConfig blocks of the feature as updated successfully, or as not
 *	updated yet, according to is_updated.
 *
 * SYNOPSIS
 */
void osm_switch_set_blocks_subn_profiles_updated(osm_switch_t * p_sw,
						 boolean_t is_updated,
						 ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
*	is_updated
*		Boolean indicates whether to mark subnet ProfilesConfig blocks of this feature
*		as updated or as not updated for this switch.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	None
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Switch/osm_switch_set_profiles_configured
 * NAME
 *	osm_switch_set_profiles_configured
 *
 * DESCRIPTION
 *	Mark ProfileConfig block of the feature as configured successfully, or as not
 *	configured yet, according to is_configured.
 *
 * SYNOPSIS
 */
void osm_switch_set_profiles_configured(osm_switch_t * p_sw,
					uint8_t block,
					boolean_t is_configured,
					ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
*	block
*		Block number
*
*	is_configured
*		Boolean indicates whether to mark ProfilesConfig block of this feature
*		as configured or as not configured for this switch.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	None
*
* NOTES
*
* SEE ALSO
*
*********/


/****f* OpenSM: Switch/osm_switch_are_profiles_configured
 * NAME
 *	osm_switch_are_profiles_configured
 *
 * DESCRIPTION
*	Returns boolean indicates whether all port profiles were configured by ProfilesConfig.
*	Meaning, whether all ProfilesConfig blocks were set successfully.
 *
 * SYNOPSIS
 */
boolean_t osm_switch_are_profiles_configured(osm_switch_t * p_sw,
					     ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	Boolean indicates whether all port profiles were configured by ProfilesConfig.
*	Meaning, whether all ProfilesConfig blocks were set successfully.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Switch/osm_switch_get_ports_profiles
 * NAME
 *	osm_switch_get_ports_profiles
 *
 * DESCRIPTION
*	Returns pointer to ProfilesConfig of input feature for this switch.
 *
 * SYNOPSIS
 */
ib_profiles_config_t * osm_switch_get_ports_profiles(osm_switch_t * p_sw,
						     ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	A pointer to ProfilesConfig of input feature for this switch.
*
* NOTES
*
* SEE ALSO
*
*********/


/****f* OpenSM: Switch/osm_switch_is_profile_set_on_switch
 * NAME
 *	osm_switch_is_profile_set_on_switch
 *
 * DESCRIPTION
 *	Returns if at least one port of the switch is set with the specified profile for the feature.
 *
 * SYNOPSIS
 */
boolean_t osm_switch_is_profile_set_on_switch(osm_switch_t * p_sw, uint8_t profile, ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to switch.
*
*	profile
* 		Profile number.
*
* 	feature
* 		Feature number that this PortsProfile MAD refers to.
*
* RETURN VALUE
*	Returns if at least one port of the switch is set with the specified profile for the feature.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Switch/osm_switch_is_prism_switch
* NAME
*	osm_switch_is_prism_switch
*
* DESCRIPTION
*	The function determines if the given switch is prism or single plane switch,
*	in case of single plane switch determines its plane,
*	for more than one plane on the switch, or for no planes (legacy switch, NDR or older)
*	return 0 in plane.
*
* SYNOPSIS
*/
boolean_t osm_switch_is_prism_switch(IN const osm_switch_t* p_sw, OUT uint8_t *p_plane);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
*	p_plane
*		[in] Pointer to the plane to return a single plane of single plane switch or
*		     plane bit mask, that consists of all the planes of the prism switch.
*
* RETURN VALUE
*	TRUE if the given switch is prism switch, FALSE otherwise
*
* NOTES
*
* SEE ALSO
* 	Planarization architecture
*********/

/****f* OpenSM: Switch/osm_switch_initialize_profiles_config
* NAME
*	osm_switch_initialize_profiles_config
*
* DESCRIPTION
*	Mark all ProfilesConfig MADs of input feature as not configured.
*
* SYNOPSIS
*/
void osm_switch_initialize_profiles_config(osm_switch_t * p_sw, ib_profiles_feature_t feature);
/*
* PARAMETERS
*	p_sw
*		Pointer to the switch.
*
* 	feature
* 		Feature identifier, refers to ProfilesConfig of which feature to initialize.
*
* RETURN VALUE
* 	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Switch/osm_switch_get_port_for_lid
* NAME
*       osm_switch_get_port_for_lid
*
* DESCRIPTION
*       Set the port number for routing to the given lid.
*
* SYNOPSIS
*/
static inline uint8_t osm_switch_get_port_for_lid(IN const osm_switch_t * p_sw, IN uint16_t lid_ho)
{
	if (lid_ho == 0 || lid_ho > p_sw->max_lid_ho) {
		return OSM_NO_PATH;
	}

	return p_sw->new_lft[lid_ho];
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	lid_ho
*		[in] LID (host order) for which to retrieve the shortest hop count.
*
* RETURN VALUES
*	Port number if lid_ho is in range.
*	OSM_NO_PATH otherwise.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_set_port_for_lid
* NAME
*       osm_switch_set_port_for_lid
*
* DESCRIPTION
*       Set the port number for routing to the given lid.
*
* SYNOPSIS
*/
static inline void osm_switch_set_port_for_lid(IN const osm_switch_t * p_sw,
					       IN uint16_t lid_ho,
					       IN uint8_t port_num,
					       IN osm_lft_type_enum lft_enum)
{
	uint16_t lft_size = (lid_ho / IB_SMP_DATA_SIZE + 1) * IB_SMP_DATA_SIZE;

	if (lid_ho == 0 || p_sw->lft_size < lft_size)
		return;

	if (lft_enum == OSM_LFT) {
		p_sw->lft[lid_ho] = port_num;
	} else if (lft_enum == OSM_NEW_LFT) {
		p_sw->new_lft[lid_ho] = port_num;
	}
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	lid_ho
*		[in] LID (host order) for which to retrieve the shortest hop count.
*
*	portnum
*		[in] Port for routing to lid
*
*	lft_enum
*		[in] Use LFT that was calculated by routing engine, or current
*		LFT on the switch.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_set_hops_to_remote_switch
* NAME
*       osm_switch_set_hops_to_remote_switch
*
* DESCRIPTION
*       Set the hop count from this switch/port to the remote switch.
*
* SYNOPSIS
*/
static inline void osm_switch_set_hops_to_remote_switch(IN osm_switch_t *p_sw,
							IN osm_switch_t *p_remote_sw,
							IN uint8_t port_num,
							IN uint8_t hop_count)
{
	uint16_t lid_ho = osm_node_get_base_lid_ho(p_remote_sw->p_node, 0);

	osm_switch_set_hops(p_sw, lid_ho, port_num, hop_count);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	p_remote_sw
*		[in] Pointer to an osm_switch_t object.
*
*	port_num
*		[in] Port for routing to remote switch.
*
*	hop_count
*		[in] The weighted hop count to the remote switch through this port.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_set_routing
* NAME
*       osm_switch_set_routing
*
* DESCRIPTION
*       Set the routing information on the switch.
*       The information is set based on whether switch is configured for adaptive routing or not.
*
* SYNOPSIS
*/
void osm_switch_set_routing(IN OUT osm_switch_t* p_sw,
			   IN osm_node_t *p_dest_node,
			   IN uint8_t dest_port_num,
			   IN uint8_t static_port,
			   IN uint8_t state,
			   IN uint16_t ar_group_id);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	p_dest_node
*		[in] Pointer to an osm_node_t object of the destination node.
*
*	dest_port_num
*		[in] Port number on the destination node for routing to destination port.
*	static_port
*		[in] The port to set the static port (as in adaptive routing table) on the switch,
*		through which routing goes to destination.
*	state
*		[in] The state (as in adaptive routing table) to set the state of the routing
*		to destination.
*	ar_group_id
*		[in] The group id (as in adaptive routing table) to set the group id of the routing
*		to destination.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_get_routing
* NAME
*       osm_switch_get_routing
*
* DESCRIPTION
*       Get the routing information from the switch.
*       The information is acquired based on whether switch is configured for adaptive routing or not.
*
* SYNOPSIS
*/
void osm_switch_get_routing(IN osm_switch_t* p_sw,
			    IN uint8_t plft_id,
			    IN uint16_t lid,
			    OUT uint8_t *static_port,
			    OUT uint8_t *state,
			    OUT uint16_t *ar_group_id);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	plft_id
*		[in] Private LFT ID.
*
*	lid
*		[in] LID of the destination of routing.
*
*	static_port
*		[out] The port to get the static port (as in adaptive routing table) on the switch,
*		through which routing goes to destination.
*	state
*		[out] The state (as in adaptive routing table) to get the state of the routing
*		to destination.
*	ar_group_id
*		[out] The group id (as in adaptive routing table) to get the group id of the routing
*		to destination.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Switch object
*********/

/****f* OpenSM: Switch/osm_switch_is_spst_supported
* NAME
*	osm_switch_is_spst_supported
*
* DESCRIPTION
* 	Returns TRUE if switch supports SwitchPortStateTable MAD.
*
* SYNOPSIS
*/
boolean_t osm_switch_is_spst_supported(osm_switch_t * p_sw);
/*
* PARAMETERS
*	p_sw
*		Pointer to the switch.
*
* RETURN VALUE
* 	Returns TRUE if switch supports SwitchPortStateTable MAD.
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
