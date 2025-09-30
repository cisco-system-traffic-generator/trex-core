/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2010 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 Xsigo Systems Inc.  All rights reserved.
 * Copyright (c) 2009 System Fabric Works, Inc. All rights reserved.
 * Copyright (c) 2009 HNR Consulting. All rights reserved.
 * Copyright (c) 2009-2015 ZIH, TU Dresden, Federal Republic of Germany. All rights reserved.
 * Copyright (C) 2012-2017 Tokyo Institute of Technology. All rights reserved.
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
 *	Declaration of osm_subn_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_fleximap.h>
#include <complib/cl_map.h>
#include <complib/cl_ptr_vector.h>
#include <complib/cl_u64_vector.h>
#include <complib/cl_list.h>
#include <complib/cl_vector.h>
#include <complib/cl_spinlock.h>
#include <complib/cl_last_write_register.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_base.h>
#include <opensm/osm_prefix_route.h>
#include <opensm/osm_db.h>
#include <opensm/osm_log.h>
#include <opensm/osm_path.h>
#include <opensm/osm_planarized.h>
#include <opensm/osm_routing.h>
#include <opensm/osm_fabric_mode.h>
#include <opensm/st.h>
#include <stdio.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/*
 * Convenience macros.
 */
#define foreach_node(SUBN,PTR)                                                  \
	for (PTR  = (osm_node_t *)cl_qmap_head(&((SUBN)->node_guid_tbl));       \
	     PTR != (osm_node_t *)cl_qmap_end(&(SUBN)->node_guid_tbl);          \
	     PTR  = (osm_node_t *)cl_qmap_next((cl_map_item_t *)PTR))

#define foreach_switch(SUBN,P_SW)                                               \
	for (P_SW  = (osm_switch_t *)cl_qmap_head(&((SUBN)->sw_guid_tbl));      \
	     P_SW != (osm_switch_t *)cl_qmap_end(&((SUBN)->sw_guid_tbl));       \
	     P_SW  = (osm_switch_t *)cl_qmap_next((cl_map_item_t*)P_SW))

#define foreach_port(SUBN,PTR)                                                  \
	for (PTR  = (osm_port_t *)cl_qmap_head(&((SUBN)->port_guid_tbl));       \
	     PTR != (osm_port_t *)cl_qmap_end(&(SUBN)->port_guid_tbl);          \
	     PTR  = (osm_port_t *)cl_qmap_next((cl_map_item_t *)PTR))


#define foreach_leaf_iter(SUBN,ITER)                                            \
        for (ITER  = cl_list_head(&((SUBN)->leaf_switches_list));               \
             ITER != cl_list_end(&((SUBN)->leaf_switches_list));                \
             ITER  = cl_list_next(ITER))

#define	leaf_iter_to_switch(ITER)	(osm_switch_t *)cl_list_obj(ITER)

#define plane_is_valid(plane)		((0 <= plane) && (plane <= IB_MAX_NUM_OF_PLANES_XDR))

#define OSM_SUBNET_VECTOR_MIN_SIZE			0
#define OSM_SUBNET_VECTOR_GROW_SIZE			1
#define OSM_SUBNET_VECTOR_CAPACITY			256

#define OSM_PARTITION_ENFORCE_BOTH			"both"
#define OSM_PARTITION_ENFORCE_IN			"in"
#define OSM_PARTITION_ENFORCE_OUT			"out"
#define OSM_PARTITION_ENFORCE_OFF			"off"
#define OSM_DEFAULT_STATISTIC_DUMP_TIME_MINUTES		60
#define OSM_MAX_STATISTIC_DUMP_TIME			UINT32_MAX/60000
#define OSM_MAX_STATISTIC_DUMP_FILE_SIZE		4095
#define OSM_DEFAULT_STATISTIC_DUMP_FILE_SIZE		20
#define OSM_MAX_PERFLOG_DUMP_FILE_SIZE			4095
#define OSM_DEFAULT_PERFLOG_DUMP_FILE_SIZE		20
#define OSM_DEFAULT_AGUID_HOP_LIMIT			1
#define OSM_ACTIVITY_DEFAULT_SUBJECTS			OSM_ACTIVITY_SUBJECTS_NONE
#define OSM_KEY_DEFAULT_LEASE_PERIOD			60
#define OSM_KEY_DEFAULT_ENABLEMENT			OSM_CAPABILITY_IGNORED
#define OSM_MAX_SW_PEERS_NUM				254
#define OSM_SM_PORT_STATE_UP                            1
#define OSM_SM_PORT_STATE_DOWN                          0
#define OSM_KEY_SEED_RANDOM                             UINT64_MAX

/* By transport mask: Bit 0= UD, Bit 1= RC, Bit 2= UC, Bit 3= DCT, Bits 4-7 are reserved */
#define OSM_AR_CFG_DEFAULT_TRANSPORT_MASK		0xA /* RC, DC are enabled the rest are not */
#define OSM_AR_CFG_TRANSPORT_MASK_EN_ALL		0xF

typedef enum _osm_capability_type_enum {
	OSM_CAPABILITY_IGNORED = 0,
	OSM_CAPABILITY_DISABLED,
	OSM_CAPABILITY_ENABLED
} osm_capability_type_enum;

#define OSM_SHARP_CAP_DEFAULT	OSM_CAPABILITY_ENABLED
#define OSM_MLNX_CC_CAP_DEFAULT	OSM_CAPABILITY_IGNORED
#define OSM_FAST_RECOVERY_CAP_DEFAULT			OSM_CAPABILITY_IGNORED
#define OSM_PORT_RECOVERY_POLICY_CAP_DEFAULT		OSM_CAPABILITY_IGNORED

typedef enum _osm_partition_enforce_type_enum {
	OSM_PARTITION_ENFORCE_TYPE_BOTH,
	OSM_PARTITION_ENFORCE_TYPE_IN,
	OSM_PARTITION_ENFORCE_TYPE_OUT,
	OSM_PARTITION_ENFORCE_TYPE_OFF
} osm_partition_enforce_type_enum;

typedef enum _osm_asymmetric_tree_enum {
	OSM_ASYMMETRIC_TREE_DISABLED = 0,
	OSM_ASYMMETRIC_TREE_1_SUB_GROUP_ENABLED,
	OSM_ASYMMETRIC_TREE_2_SUB_GROUPS_ENABLED,
	OSM_ASYMMETRIC_TREE_ASYMTREE,
	OSM_ASYMMETRIC_TREE_MAX,			// Used for bounds checking
} osm_asymmetric_tree_enum;

typedef enum _osm_routing_flags_enum {
	OSM_ROUTING_FLAGS_RANK_ADJUSTMENT = 0x1,
	OSM_ROUTING_FLAGS_COMPLETE_ROOT_GUID_FILE = 0x2,
	OSM_ROUTING_FLAGS_ASYM3_LOCAL_BALANCING = 0x4,
	OSM_ROUTING_FLAGS_ASYM3_NO_BALANCING = 0x8,
} osm_routing_flags_enum;

typedef enum _osm_enhanced_missing_routes_mode {
	OSM_ENH_MISSING_ROUTES_DISABLED = 0,
	OSM_ENH_MISSING_ROUTES_SM_TO_ALL,
	OSM_ENH_MISSING_ROUTES_ALL,
} osm_enhanced_missing_routes_mode;


#define OSM_HM_COND_CREDIT_WATCHDOG_BIT			1 << OSM_HM_COND_CREDIT_WATCHDOG
#define OSM_HM_COND_RAW_BER_BIT				1 << OSM_HM_COND_RAW_BER
#define OSM_HM_COND_EFFECTIVE_BER_BIT			1 << OSM_HM_COND_EFFECTIVE_BER
#define OSM_HM_COND_SYMBOL_BER_BIT			1 << OSM_HM_COND_SYMBOL_BER
#define OSM_HM_COND_SW_DESICION_MASK			(OSM_HM_COND_CREDIT_WATCHDOG_BIT | \
							 OSM_HM_COND_RAW_BER_BIT | \
							 OSM_HM_COND_EFFECTIVE_BER_BIT | \
							 OSM_HM_COND_SYMBOL_BER_BIT)

typedef enum _osm_hm_cond_type_enum {
	OSM_HM_COND_REBOOT = 0,
	OSM_HM_COND_FLAPPING,
	OSM_HM_COND_UNRESPONSIVE,
	OSM_HM_COND_NOISY,
	OSM_HM_COND_SET_ERR,
	OSM_HM_COND_ILLEGAL,
	OSM_HM_COND_MANUAL,
	OSM_HM_COND_CREDIT_WATCHDOG,
	OSM_HM_COND_RAW_BER,
	OSM_HM_COND_EFFECTIVE_BER,
	OSM_HM_COND_SYMBOL_BER,
	OSM_HM_COND_MAX
} osm_hm_cond_type_enum;

typedef enum _osm_hm_action_type_enum {
	OSM_HM_ACTION_IGNORE = 0,
	OSM_HM_ACTION_REPORT,
	OSM_HM_ACTION_ISOLATE,
	OSM_HM_ACTION_NO_DISCOVER,
	OSM_HM_ACTION_DISABLE,
	OSM_HM_ACTION_MAX
} osm_hm_action_type_enum;

#define OSM_ALL_SL_VL_AR_ENABLED 0xFFFF   //all SLs or VLs are AR enabled

#define OSM_HBF_HASH_TYPE_CRC	0

#define OSM_HBF_SEED_TYPE_CONFIGURABLE	0
#define OSM_HBF_SEED_TYPE_RANDOM	1

/*
 * All HBF hash fields are enabled besides:
 * All Hash fields enabled by default except:
 * 	Ingress LAG ID
 * 	LNH
 * 	GRH.tclass.dscp
 * 	GRH.tclass.ecn
 * 	GRH.flow_label
 */
#define OSM_DEFAULT_HBF_HASH_FIELDS CL_HTON64(0x40F00C0F)

#define OSM_DEFAULT_HBF_SL_MASK 0xFFFF

/*
 * hbf_hash_seed configuration value 0xffffffff stands
 * for using 32 LSB of GUID as hbf seed. 
 */
#define OSM_HBF_HASH_SEED_GUID		0xFFFFFFFF

#define OSM_DEFAULT_WHBF_CONFIG		"auto"

extern const char *osm_hm_action_str[OSM_HM_ACTION_MAX];

/* XXX: not actual max, max we're currently going to support */
#define OSM_CCT_ENTRY_MAX        128
#define OSM_CCT_ENTRY_MAD_BLOCKS (OSM_CCT_ENTRY_MAX/64)
#define OSM_SUBNET_DEFAULT_PATH_BITS 0xFF

#define OSM_MAX_BINDING_PORTS	8

/*
 * The current router port HW supports 8 LIDs with 8 LID mask bits each
 * (maximum of 256 addresses per LID overall).
 * One address + LID mask is configured as the router LID using SubnSet(PortInfo),
 * and 7 addresses are used for FLID. This equals to 1792 available FLIDs.
 */
#define OSM_MAX_NUMBER_GLOBAL_FLIDS		1792

#define OSM_ROUTER_FLID_ALIGNMENT		256

struct osm_opensm;
struct osm_qos_policy;
struct osm_physp;
struct osm_mgrp_box;
struct osm_node;
struct osm_sm;

#define SECOND_TO_MILLIS (1000)
#define MINUTES_TO_SEC (60)
#define MINUTES_TO_MILLISEC (MINUTES_TO_SEC * SECOND_TO_MILLIS)

/****h* OpenSM/Subnet
* NAME
*	Subnet
*
* DESCRIPTION
*	The Subnet object encapsulates the information needed by the
*	OpenSM to manage a subnet.  The OpenSM allocates one Subnet object
*	per IBA subnet.
*
*	The Subnet object is not thread safe, thus callers must provide
*	serialization.
*
*	This object is essentially a container for the various components
*	of a subnet.  Callers may directly access the member variables.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****s* OpenSM: Subnet/osm_qos_options_t
* NAME
*	osm_qos_options_t
*
* DESCRIPTION
*	Subnet QoS options structure.  This structure contains the various
*	QoS specific configuration parameters for the subnet.
*
* SYNOPSIS
*/
typedef struct osm_qos_options {
	unsigned max_vls;
	int high_limit;
	char *vlarb_high;
	char *vlarb_low;
	char *sl2vl;
} osm_qos_options_t;
/*
* FIELDS
*
*	max_vls
*		The number of maximum VLs on the Subnet (0 == use default)
*
*	high_limit
*		The limit of High Priority component of VL Arbitration
*		table (IBA 7.6.9) (-1 == use default)
*
*	vlarb_high
*		High priority VL Arbitration table template. (NULL == use default)
*
*	vlarb_low
*		Low priority VL Arbitration table template. (NULL == use default)
*
*	sl2vl
*		SL2VL Mapping table (IBA 7.6.6) template. (NULL == use default)
*
*********/

/****s* OpenSM: Subnet/osm_cct_entry_t
* NAME
*	osm_cct_entry_t
*
* DESCRIPTION
*	Subnet Congestion Control Table entry.  See A10.2.2.1.1 for format details.
*
* SYNOPSIS
*/
typedef struct osm_cct_entry {
	uint8_t shift; //Alex: shift 2 bits
	uint16_t multiplier; //Alex multiplier 14 bits
} osm_cct_entry_t;
/*
* FIELDS
*
*	shift
*		shift field in CCT entry.  See A10.2.2.1.1.
*
*	multiplier
*		multiplier field in CCT entry.  See A10.2.2.1.1.
*
*********/

/****s* OpenSM: Subnet/osm_cacongestion_entry_t
* NAME
*	osm_cacongestion_entry_t
*
* DESCRIPTION
*	Subnet CA Congestion entry.  See A10.4.3.8.4 for format details.
*
* SYNOPSIS
*/
typedef struct osm_cacongestion_entry {
	ib_net16_t ccti_timer; //Alex: ccti_timer and ccti_increase should be replaced
	uint8_t ccti_increase;
	uint8_t trigger_threshold;
	uint8_t ccti_min;
} osm_cacongestion_entry_t;
/*
* FIELDS
*
*	ccti_timer
*		CCTI Timer
*
*	ccti_increase
*		CCTI Increase
*
*	trigger_threshold
*		CCTI trigger for log message
*
*	ccti_min
*		CCTI Minimum
*
*********/

/****s* OpenSM: Subnet/osm_cct_t
* NAME
*	osm_cct_t
*
* DESCRIPTION
*	Subnet CongestionControlTable.  See A10.4.3.9 for format details.
*
* SYNOPSIS
*/
typedef struct osm_cct {
	osm_cct_entry_t entries[OSM_CCT_ENTRY_MAX];
	unsigned int entries_len;
	char *input_str;
} osm_cct_t;
/*
* FIELDS
*
*	entries
*		Entries in CCT
*
*	entries_len
*		Length of entries
*
*	input_str
*		Original str input
*
*********/

/****d* OpenSM: OpenSM/osm_port_reset_type_enum
* NAME
*	osm_port_reset_type_enum
*
* DESCRIPTION
*	Types of port reset action.
*
* SYNOPSIS
*/
typedef enum _osm_port_reset_type_enum {
	OSM_PORT_RESET_TYPE_NONE = 0,
	OSM_PORT_RESET_TYPE_LOGICAL,
	OSM_PORT_RESET_TYPE_PHYSICAL,
	OSM_PORT_RESET_TYPE_MAX

} osm_port_reset_type_enum;
/***********/

/****f* OpenSM: OpenSM/osm_port_reset_type_str
* NAME
*	osm_port_reset_type_str
*
* DESCRIPTION
*	Returns a string for osm_port_reset_type_enum
*
* SYNOPSIS
*/
static inline const char *
osm_port_reset_type_str(IN osm_port_reset_type_enum port_reset_type)
{
	switch (port_reset_type) {
	case OSM_PORT_RESET_TYPE_NONE:
		return "no_reset";
	case OSM_PORT_RESET_TYPE_LOGICAL:
		return "logical";
	case OSM_PORT_RESET_TYPE_PHYSICAL:
		return "physical";
	default:
		return "unknown";
	}
}
/*
* PARAMETERS
*	port_reset_type
*		[in] Encoded port reset type.
*
* RETURN VALUES
*	Pointer to the port reset type string.
*
* NOTES
*/

/****d* OpenSM: OpenSM/osm_port_reset_reason_enum
* NAME
*	osm_port_reset_reason_enum
*
* DESCRIPTION
*	Types of port reset reasons.
*
* SYNOPSIS
*/
typedef enum _osm_port_reset_reason_enum {
	OSM_PORT_RESET_REASON_NONE = 0,
	OSM_PORT_RESET_REASON_SPEED,
	OSM_PORT_RESET_REASON_EXTSPEED,
	OSM_PORT_RESET_REASON_EXTSPEED2,
	OSM_PORT_RESET_REASON_MLNXSPEED,
	OSM_PORT_RESET_REASON_MTU,
	OSM_PORT_RESET_REASON_OPVLS,
	OSM_PORT_RESET_REASON_AME,
	OSM_PORT_RESET_REASON_PRP,
	OSM_PORT_RESET_REASON_MAX,
} osm_port_reset_reason_enum;
/***********/

/****f* OpenSM: OpenSM/osm_port_reset_reason_str
* NAME
*	osm_port_reset_reason_str
*
* DESCRIPTION
*	Returns a string for osm_port_reset_reason_enum
*
* SYNOPSIS
*/
static inline const char *
osm_port_reset_reason_str(IN osm_port_reset_reason_enum port_reset_reason)
{

	switch (port_reset_reason) {
	case OSM_PORT_RESET_REASON_NONE:
		return "none";
	case OSM_PORT_RESET_REASON_SPEED:
		return "speeds";
	case OSM_PORT_RESET_REASON_EXTSPEED:
		return "extended speeds";
	case OSM_PORT_RESET_REASON_EXTSPEED2:
		return "extended speeds 2";
	case OSM_PORT_RESET_REASON_MLNXSPEED:
		return "Mellanox speeds";
	case OSM_PORT_RESET_REASON_MTU:
		return "MTU";
	case OSM_PORT_RESET_REASON_OPVLS:
		return "operational VLs";
	case OSM_PORT_RESET_REASON_AME:
		return "AME bit";
	case OSM_PORT_RESET_REASON_PRP:
		return "Port recovery policy config";
	default:
		return "unknown";
	}
}
/*
* PARAMETERS
*	port_reset_reason
*		[in] Encoded port reset reason.
*
* RETURN VALUES
*	Pointer to the port reset reason string.
*
* NOTES
*/

/****d* OpenSM: OpenSM/osm_topology_type_enum
* NAME
*	osm_topology_type_enum
*
* DESCRIPTION
*	Types of port topology types supported by topology auto detection.
*
* SYNOPSIS
*/
typedef enum _osm_topology_type_enum {
	OSM_TOPOLOGY_TYPE_UNKNOWN = 0,
	OSM_TOPOLOGY_TYPE_TREE,
	OSM_TOPOLOGY_TYPE_DFP,
	OSM_TOPOLOGY_TYPE_MAX
} osm_topology_type_enum;
/***********/

/****d* OpenSM: OpenSM/osm_dfp_intermediate_down_path
* NAME
*	osm_dfp_intermediate_down_path
*
* DESCRIPTION
*	Different options of taking down paths in dfp groups.
*	OSM_DFP_DOWN_PATH_ENABLED - all down paths are enabled
*	OSM_DFP_DOWN_PATH_DISABLED - all down paths are disabled
*	OSM_DFP_DOWN_PATH_INTERMEDIATE_SPINE_NO_SHORTER_PATH - down paths
*	 are allowed through intermediate spine, if no shorter path exists.
*
* SYNOPSIS
*/
typedef enum _osm_dfp_intermediate_down_path {
	OSM_DFP_DOWN_PATH_ENABLED = 0,
	OSM_DFP_DOWN_PATH_DISABLED,
	OSM_DFP_DOWN_PATH_INTERMEDIATE_SPINE_NO_SHORTER_PATH
} osm_dfp_intermediate_down_path;
/***********/

/****d* OpenSM: OpenSM/osm_rtr_selection_function_t
* NAME
*	osm_rtr_selection_function_t
*
* DESCRIPTION
*	Types of functions for router selection.
*
* SYNOPSIS
*/
typedef enum _osm_rtr_selection_function_t {
	OSM_CRC32,
	OSM_FUNC_UNKNOWN
} osm_rtr_selection_function_t;
/***********/

/****d* OpenSM: Subnet/osm_rtr_selection_params_t
* NAME
*       osm_rtr_selection_params_t
*
* DESCRIPTION
* 	Indicators of which parameters to use in router selection function.
*
* SYNOPSIS
*/
typedef enum _osm_rtr_selection_params_t {
	OSM_RTR_SELECTION_PARAM_SGID = 1,
	OSM_RTR_SELECTION_PARAM_DGID = 2,
} osm_rtr_selection_params_t;
/***********/

/****d* OpenSM: Subnet/osm_client_rereg_mode_t
* NAME
*       osm_client_rereg_mode_t
*
* DESCRIPTION
* 	Client reregistration mode options.
* 	Supported values are:
* 	0 - Do not send client reregister.
* 	1 - Send client reregister in LID manager.
* 	2 - Send client reregister in link manager.
*
* SYNOPSIS
*/
typedef enum _osm_client_rereg_mode_t {
	OSM_CLIENT_REREG_MODE_DISABLE = 0,
	OSM_CLIENT_REREG_MODE_LID_MGR = 1,
	OSM_CLIENT_REREG_MODE_LINK_MGR = 2,
} osm_client_rereg_mode_t;
/***********/

#define OSM_DEFAULT_CLIENT_REREG_MODE	OSM_CLIENT_REREG_MODE_LINK_MGR

/****d* OpenSM: Subnet/osm_node_filter_t
* NAME
*       osm_node_filter_t
*
* DESCRIPTION
* 	0 - All nodes
* 	1 - Nodes that had topology changes.
* 	2 - Nodes that did not had topology change.
*
* SYNOPSIS
*/
typedef enum _osm_node_filter_t {
	OSM_NODE_FILTER_ALL = 0,
	OSM_NODE_FILTER_CHANGED = 1,
	OSM_NODE_FILTER_UNCHANGED = 2,
} osm_node_filter_t;
/***********/

/****d* OpenSM: OpenSM/planarized_routing_enum
* NAME
*	planarized_routing_enum_t
*
* DESCRIPTION
*	Planarized routing types
*
* SYNOPSIS
*/
typedef enum _osm_planarized_routing_enum {
	OSM_PLANARIZED_ROUTING_DISABLED = 0,
	OSM_PLANARIZED_ROUTING_PLANARIZED_ONLY = 1,
	OSM_PLANARIZED_ROUTING_ALL = 2,
} osm_planarized_routing_enum_t;

#define	planarized_routing_is_enabled(P_SUBN)										\
	(((P_SUBN)->opt.planarized_routing_enabled == OSM_PLANARIZED_ROUTING_ALL) ||					\
	(((P_SUBN)->opt.planarized_routing_enabled == OSM_PLANARIZED_ROUTING_PLANARIZED_ONLY) && (P_SUBN)->is_planarized))

#define	planarized_routing_is_disabled(P_SUBN)			\
	(!planarized_routing_is_enabled(P_SUBN))
/***********/

/****s* OpenSM: Subnet/osm_subn_opt_t
* NAME
*	osm_subn_opt_t
*
* DESCRIPTION
*	Subnet options structure.  This structure contains the various
*	site specific configuration parameters for the subnet.
*
* SYNOPSIS
*/
typedef struct osm_subn_opt {
	char *config_file;
	ib_net64_t guid[OSM_MAX_BINDING_PORTS];
	uint8_t num_guids;
	ib_net64_t m_key;
	ib_net64_t sm_key;
	ib_net64_t sa_key;
	ib_net64_t subnet_prefix;
	ib_net16_t m_key_lease_period;
	uint8_t m_key_protect_bits;
	boolean_t m_key_lookup;
	boolean_t m_key_per_port;
	uint32_t periodic_key_update;
	uint32_t sweep_interval;
	uint32_t max_wire_smps;
	uint32_t max_wire_smps2;
	uint32_t max_smps_timeout;
	uint32_t transaction_timeout;
	uint32_t transaction_retries;
	uint32_t long_transaction_timeout;
	uint32_t max_sa_reports_queued;
	uint32_t max_sa_reports_on_wire;
	boolean_t sa_enhanced_trust_model;
	boolean_t sa_etm_allow_untrusted_proxy_requests;
	boolean_t sa_etm_allow_untrusted_guidinfo_rec;
	boolean_t sa_etm_allow_guidinfo_rec_by_vf;
	uint32_t sa_etm_max_num_mcgs;
	uint32_t sa_etm_max_num_srvcs;
	uint32_t sa_etm_max_num_event_subs;
	uint32_t sa_rate_threshold;
	boolean_t sa_check_sgid_spoofing;
	uint8_t sm_priority;
	uint8_t master_sm_priority;
	uint16_t master_sm_lid;
	boolean_t enable_sm_port_failover;
	uint8_t lmc;
	uint8_t rtr_lmc;
	uint8_t prism_an_lmc;
	uint8_t lids_per_rtr;
	boolean_t lmc_esp0;
	uint8_t max_op_vls;
	boolean_t qos_config_vl_enabled;
	uint8_t max_neighbor_mtu;
	uint8_t suppress_mc_pkey_traps;
	uint8_t force_link_speed;
	uint8_t force_link_speed_ext;
	uint8_t force_link_speed_ext2;
	uint8_t force_link_width;
	uint8_t fdr10;
	boolean_t support_mepi_speeds;
	uint8_t mepi_enabled_speeds;
	boolean_t reassign_lids;
	boolean_t ignore_other_sm;
	boolean_t single_thread;
	boolean_t disable_multicast;
	boolean_t force_log_flush;
	uint8_t subnet_timeout;
	uint8_t packet_life_time;
	uint8_t vl_stall_count;
	uint8_t leaf_vl_stall_count;
	uint8_t head_of_queue_lifetime;
	uint8_t leaf_head_of_queue_lifetime;
	uint8_t local_phy_errors_threshold;
	uint8_t overrun_errors_threshold;
	boolean_t use_mfttop;
	uint32_t sminfo_polling_timeout;
	uint32_t polling_retry_number;
	uint32_t max_msg_fifo_timeout;
	boolean_t force_heavy_sweep;
	boolean_t light_sweep_spst;
	uint8_t log_flags;
	uint8_t syslog_log_flags;
	char *dump_files_dir;
	char *cache_files_dir;
	char *log_file;
	uint32_t log_max_size;
	uint8_t log_num_backlogs;
	char *partition_config_file;
	boolean_t no_partition_enforcement;
	char *part_enforce;
	osm_partition_enforce_type_enum part_enforce_enum;
	boolean_t allow_both_pkeys;
	boolean_t keep_pkey_indexes;
	uint8_t sm_assigned_guid;
	boolean_t qos;
	char *qos_policy_file;
	char *enhanced_qos_policy_file;
	boolean_t enhanced_qos_vport0_unlimit_default_rl;
	boolean_t suppress_sl2vl_mad_status_errors;
	uint8_t override_create_mcg_sl;
	boolean_t accum_log_file;
	char *console;
	uint16_t console_port;
	char *port_prof_ignore_file;
	char *hop_weights_file;
	char *port_search_ordering_file;
	boolean_t port_profile_switch_nodes;
	boolean_t sweep_on_trap;
	char *routing_engine_names;
	boolean_t enable_queries_during_routing;
	char *allowed_sm_guids;
	boolean_t avoid_throttled_links;
	boolean_t use_ucast_cache;
	boolean_t connect_roots;
	boolean_t calc_missing_routes;
	uint8_t enhanced_missing_routes;
	uint8_t max_cas_on_spine;
	boolean_t find_roots_color_algorithm;
	boolean_t dfp_find_roots_color_algorithm;
	char *lid_matrix_dump_file;
	char *lfts_file;
	char *root_guid_file;
	char *pgrp_policy_file;
	char *topo_policy_file;
	char *rch_policy_file;
	char *device_configuration_file;
	char *congestion_control_policy_file;
	char *ppcc_algo_dir;
	uint8_t port_recovery_policy_enabled;
	uint8_t fast_recovery_enabled;
	char *fast_recovery_conf_file;
	char *ftree_ca_order_dump_file;
	char *held_back_sw_file;
	char *cn_guid_file;
	char *io_guid_file;
	boolean_t port_shifting;
	uint32_t scatter_ports;
	boolean_t updn_lid_tracking_mode;
	boolean_t updn_lid_tracking_converge_routes;
	boolean_t updn_lid_tracking_prefer_total_routes;
	uint8_t dfp_max_cas_on_spine;
	uint16_t max_reverse_hops;
	uint16_t routing_threads_num;
	uint16_t max_threads_per_core;
	char *ids_guid_file;
	char *guid_routing_order_file;
	char *pqft_structure;
	boolean_t guid_routing_order_no_scatter;
	boolean_t use_scatter_for_switch_lid;
	boolean_t offsweep_balancing_enabled;
	uint32_t  offsweep_balancing_window;
	char *sa_db_file;
	boolean_t sa_db_dump;
	boolean_t sm_db_dump;
	char *torus_conf_file;
	char *pid_file;
	boolean_t do_mesh_analysis;
	boolean_t exit_on_fatal;
	boolean_t run_once;
	boolean_t honor_guid2lid_file;
	boolean_t const_multicast;
	boolean_t daemon;
	boolean_t sm_inactive;
	boolean_t babbling_port_policy;
	uint8_t reports;
	boolean_t use_optimized_slvl;
	boolean_t use_optimized_port_mask_slvl;
	boolean_t fsync_high_avail_files;
	uint8_t default_mcg_mtu;
	uint8_t default_mcg_rate;
	osm_qos_options_t qos_options, qos_options_prev;
	osm_qos_options_t qos_ca_options, qos_ca_options_prev;
	osm_qos_options_t qos_sw0_options, qos_sw0_options_prev;
	osm_qos_options_t qos_swe_options, qos_swe_options_prev;
	osm_qos_options_t qos_rtr_options, qos_rtr_options_prev;
	osm_qos_options_t qos_sw2sw_options, qos_sw2sw_options_prev;
	uint8_t mlnx_congestion_control;
	ib_net64_t cc_key;
	uint32_t cc_max_outstanding_mads;
	uint8_t cc_key_enable;
	uint16_t cc_key_lease_period;
	uint8_t cc_key_protect_bit;
	uint8_t vs_key_enable;
	uint16_t vs_key_lease_period;
	uint8_t vs_key_ci_protect_bits;
	uint32_t vs_max_outstanding_mads;
	uint8_t n2n_key_enable;
	uint16_t n2n_key_lease_period;
	uint8_t n2n_key_protect_bit;
	uint32_t n2n_max_outstanding_mads;
	ib_net64_t key_mgr_seed;
	boolean_t enable_quirks;
	boolean_t no_clients_rereg;
	uint8_t client_rereg_mode;
#ifdef ENABLE_OSM_PERF_MGR
	boolean_t perfmgr;
	boolean_t perfmgr_redir;
	uint16_t perfmgr_sweep_time_s;
	uint32_t perfmgr_max_outstanding_queries;
	boolean_t perfmgr_ignore_cas;
	char *event_db_dump_file;
	boolean_t perfmgr_rm_nodes;
	boolean_t perfmgr_log_errors;
	boolean_t perfmgr_query_cpi;
	boolean_t perfmgr_xmit_wait_log;
	uint32_t perfmgr_xmit_wait_threshold;
#endif				/* ENABLE_OSM_PERF_MGR */
	char *plugin_name;
	char *plugin_options;
	char *event_plugin_name;
	char *event_plugin_options;
	char *node_name_map_name;
	char *prefix_routes_file;
	char *log_prefix;
	boolean_t consolidate_ipv6_snm_req;
	uint32_t  consolidate_ipv4_mask;
	struct osm_subn_opt *file_opts; /* used for update */
	uint8_t lash_start_vl;			/* starting vl to use in lash */
	uint8_t sm_sl;			/* which SL to use for SM/SA communication */
	uint32_t max_seq_redisc;	/* Maximal sequential failed discovery */
	uint8_t rediscovery_max_link_penalty;
	uint8_t rediscovery_link_penalty_step;
	uint8_t rediscovery_max_node_penalty;
	uint8_t rediscovery_node_penalty_step;
	boolean_t rereg_on_guid_migr;
	boolean_t aguid_inout_notice;
	char *sm_assign_guid_func;
	ib_net64_t mc_primary_root_guid;
	ib_net64_t mc_secondary_root_guid;
	char *mc_roots_file;
	boolean_t drop_subscr_on_report_fail;
	boolean_t drop_event_subscriptions;
	boolean_t drop_unreachable_event_subscriptions;
	boolean_t ipoib_mcgroup_creation_validation;
	boolean_t mcgroup_join_validation;
	boolean_t use_original_extended_sa_rates_only;
	uint8_t max_rate_enum;
	char *per_module_logging_file;
	char *switch_name;
	uint32_t max_msg_fifo_len;
	uint32_t max_alt_dr_path_retries;
	uint32_t ca_port_soft_limit;
	uint32_t ca_port_num_limit;
	boolean_t sa_pr_full_world_queries_allowed;
	boolean_t enable_crashd;
	boolean_t hm_unhealthy_ports_checks;
	char *hm_ports_health_policy_file;
	char *hm_ca_reboot_action_s;
	char *hm_ca_unresponsive_action_s;
	char *hm_ca_noisy_action_s;
	char *hm_ca_seterr_action_s;
	char *hm_ca_flapping_action_s;
	char *hm_ca_illegal_action_s;
	char *hm_ca_manual_action_s;
	char *hm_sw_reboot_action_s;
	char *hm_sw_unresponsive_action_s;
	char *hm_sw_noisy_action_s;
	char *hm_sw_seterr_action_s;
	char *hm_sw_flapping_action_s;
	char *hm_sw_illegal_action_s;
	char *hm_sw_manual_action_s;
	osm_hm_action_type_enum hm_ca_reboot_action;
	osm_hm_action_type_enum hm_ca_unresponsive_action;
	osm_hm_action_type_enum hm_ca_noisy_action;
	osm_hm_action_type_enum hm_ca_seterr_action;
	osm_hm_action_type_enum hm_ca_flapping_action;
	osm_hm_action_type_enum hm_ca_illegal_action;
	osm_hm_action_type_enum hm_ca_manual_action;
	osm_hm_action_type_enum hm_sw_reboot_action;
	osm_hm_action_type_enum hm_sw_unresponsive_action;
	osm_hm_action_type_enum hm_sw_noisy_action;
	osm_hm_action_type_enum hm_sw_seterr_action;
	osm_hm_action_type_enum hm_sw_flapping_action;
	osm_hm_action_type_enum hm_sw_illegal_action;
	osm_hm_action_type_enum hm_sw_manual_action;
	uint32_t hm_num_reboots;
	uint32_t hm_reboots_period_secs;
	uint32_t hm_num_set_err_sweeps;
	uint32_t hm_num_set_err_sweeps_window;
	uint32_t hm_num_no_resp_sweeps;
	uint32_t hm_num_no_resp_sweeps_window;
	uint32_t hm_num_traps;
	uint32_t hm_num_traps_period_secs;
	uint32_t hm_num_flapping_sweeps;
	uint32_t hm_num_flapping_sweeps_window;
	uint32_t hm_num_illegal;
	int32_t force_heavy_sweep_window;
	boolean_t validate_smps;
	uint8_t max_topologies_per_sw;
	boolean_t auto_join_hca_wo_topo;
	boolean_t enable_inc_mc_routing;
	boolean_t allow_sm_port_reset;
	boolean_t prp_override_allow_sm_port_reset;
	boolean_t support_mlnx_enhanced_link;
	boolean_t mlnx_enhanced_link_enable;
	uint16_t adaptive_timeout_sl_mask;
	char *port_speed_change_action_s;
	char *port_ext_speed_change_action_s;
	char *port_ext_speed2_change_action_s;
	char *port_mepi_speed_change_action_s;
	char *port_mtu_change_action_s;
	char *port_vl_change_action_s;
	char *port_ame_bit_change_action_s;
	osm_port_reset_type_enum port_speed_change_action;
	osm_port_reset_type_enum port_ext_speed_change_action;
	osm_port_reset_type_enum port_ext_speed2_change_action;
	osm_port_reset_type_enum port_mepi_speed_change_action;
	osm_port_reset_type_enum port_mtu_change_action;
	osm_port_reset_type_enum port_vl_change_action;
	osm_port_reset_type_enum port_ame_bit_change_action;
	boolean_t mepi_cache_enabled;
	uint32_t virt_enabled;
	uint16_t virt_max_ports_in_process;
	uint8_t virt_default_hop_limit;
	boolean_t enable_virt_rec_ext;
	boolean_t improved_lmc_path_distribution;
	boolean_t sweep_every_hup_signal;
	uint32_t osm_stats_interval;
	unsigned long osm_stats_dump_limit;
	unsigned long osm_perflog_dump_limit;
	boolean_t osm_stats_dump_per_sm_port;
	boolean_t quasi_ftree_indexing;
	uint8_t sharp_enabled;
	uint8_t rtr_aguid_enable;
	uint32_t rtr_pr_flow_label;
	uint8_t rtr_pr_tclass;
	uint8_t rtr_pr_sl;
	uint8_t rtr_pr_mtu;
	uint8_t rtr_pr_rate;
	uint8_t aguid_default_hop_limit;
	char *topo_config_file;
	boolean_t topo_config_enabled;
	char *router_policy_file;
	boolean_t router_policy_enabled;
	boolean_t load_inform_info_from_sadb;
	char *verbose_bypass_policy_file;
	char *sa_db_dump_file;
	boolean_t enable_subnet_lst;
	char *subnet_lst_file;
	char *virt_dump_file;
	boolean_t dor_hyper_cube_mode;
	osm_sm_standalone_t stand_alone;
	char *additional_gi_supporting_devices;
	char *additional_mepi_force_devices;
	boolean_t disable_default_gi_support;
	char *activity_report_subjects;
	char *adv_routing_engine_name;
	uint8_t ar_mode;
	uint8_t shield_mode;
	uint8_t pfrn_sl;
	uint16_t pfrn_mask_clear_timeout;
	uint16_t pfrn_mask_force_clear_timeout;
	uint8_t pfrn_over_router_enabled;
	uint16_t ar_sl_mask;
	boolean_t enable_ar_by_device_cap;
	boolean_t enable_ar_group_copy;
	boolean_t enable_empty_arlft_optimization;
	uint8_t ar_transport_mask;
	uint8_t ar_tree_asymmetric_flow;
	uint32_t ar_tree_asymmetric_flow_threshold;
	uint32_t ar_tree_asymmetric_flow_threshold_limit;
	uint64_t routing_flags;
	uint16_t smp_threads;
	uint16_t gmp_threads;
	uint16_t gmp_traps_threads_num;
	boolean_t dump_ar;
	boolean_t cache_ar_group_id;
	osm_dfp_intermediate_down_path dfp_down_up_turns_mode;
	boolean_t enable_vl_packing;
	char *service_name2key_map_file;
	uint8_t max_wire_smps_per_device;
	struct osm_subn_opt *opts_def; /* Keep defaults */
	char *rtr_selection_function_s;
	osm_rtr_selection_function_t rtr_selection_function;
	ib_net64_t rtr_selection_seed;
	char *rtr_selection_algo_parameters_s;
	uint32_t rtr_selection_params_mask;
	uint16_t global_flid_start;
	uint16_t global_flid_end;
	uint16_t local_flid_start;
	uint16_t local_flid_end;
	uint16_t flid_compression;
	uint8_t max_op_vls_ca;
	uint8_t max_op_vls_sw;
	uint8_t max_op_vls_rtr;
	boolean_t get_mft_tables;
	uint16_t hbf_sl_mask;
	uint8_t  hbf_hash_type;
	uint8_t  hbf_seed_type;
	uint32_t hbf_seed;
	ib_net64_t hbf_hash_fields;
	char* hbf_weights;
	boolean_t enable_performance_logging;
	boolean_t nvlink_enable;
	boolean_t nvl_reassign_rails;
	uint8_t nvl_hbf_hash_type;
	uint8_t nvl_hbf_packet_hash_bitmask;
	uint32_t nvl_hbf_seed;
	uint64_t nvl_hbf_fields_enable;
	uint8_t nvl_partition_reroute_wait;
	boolean_t nvl_cage_allocation_mode;
	boolean_t nvl_cnd_for_access_links_enabled;
	boolean_t nvl_cnd_for_init_trunks_enabled;
	boolean_t tenants_policy_enabled;
	char *tenants_policy_file;
	boolean_t reply_lid_smps_in_dr;
	boolean_t respond_unknown_lid_traps;
	char *fabric_mode_profile;
	char *fabric_mode_policy_file;
	uint32_t issu_mode;
	uint32_t issu_timeout;
	uint32_t issu_pre_upgrade_time;
	char *conf_template;
	uint32_t dbg_lvl;
	boolean_t validate_conf_files;
	boolean_t enable_sa;
	boolean_t drop_unreachable_ports_enabled;
	osm_planarized_routing_enum_t planarized_routing_enabled;
} osm_subn_opt_t;
/*
* FIELDS
*
*	config_file
*		The name of the config file.
*
*	guid
*		The port guid or guids that the SM is binding to.
*
*	num_guids
*		The number of guids given in parameter 'guid'
*
*	m_key
*		M_Key value sent to all ports qualifying all Set(PortInfo).
*
*	sm_key
*		SM_Key value of the SM used for SM authentication.
*
*	sa_key
*		SM_Key value to qualify rcv SA queries as "trusted".
*
*	subnet_prefix
*		Subnet prefix used on this subnet.
*
*	m_key_lease_period
*		The lease period used for the M_Key on this subnet.
*
*	m_key_per_port
*		When TRUE enables different M_Key per port.
*
*	sweep_interval
*		The number of seconds between subnet sweeps.  A value of 0
*		disables sweeping.
*
*	max_wire_smps
*		The maximum number of SMPs sent in parallel.  Default is 4.
*
*	max_wire_smps2
*		The maximum number of timeout SMPs allowed to be outstanding.
*		Default is same as max_wire_smps which disables the timeout
*		mechanism.
*
*	max_smps_timeout
*		The wait time in usec for timeout based SMPs.  Default is
*		timeout * retries.
*
*	transaction_timeout
*		The maximum time in milliseconds allowed for a transaction
*		to complete.  Default is 200.
*
*	transaction_retries
*		The number of retries for a transaction. Default is 3.
*
*	long_transaction_timeout
*		The maximum time in milliseconds allowed for "long" transaction
*		to complete.  Default is 500.
*
*	sa_enhanced_trust_model
*		Is the enhanced trust model is active.
*		Defines a minimal set of sa operations, that are allowed for untrusted clients.
*		Default is FALSE.
*
*	sa_etm_allow_untrusted_proxy_requests
*		Indicate whether to allow or drop untrusted proxy request when SA enhanced
*		trust model is enabled.
*		Default is TRUE.
*
*	sa_etm_allow_untrusted_guidinfo_rec
*		If the sa_enhanced_trust_model active and sa_etm_allow_untrusted_guidinfo_rec
*		is set to FALSE, Opensm will drop guidInfoRecord requests if sm_key == 0 or incorrect.
*		Default is TRUE.
*
*	sa_etm_allow_guidinfo_rec_by_vf
*		If the sa_enhanced_trust_model active and sa_etm_allow_guidinfo_rec_by_vf
*		is set to FALSE, Opensm will silently drop guidInfoRecord
*		SET/DELETE requests from non-physical ports
*		(applies also to trusted clients).
*		Default is TRUE.
*
*	sa_etm_max_num_mcgs
*		Max number of multicast groups per port/vport that can be registered,
*		active only if the sa_enhanced_trust_model is set to TRUE.
*		Default: 0 (unlimited)
*
*	sa_etm_max_num_srvcs
*		Max number of services (ServiceRecords) per port/vport that can be registered,
*		active only if the sa_enhanced_trust_model is set to TRUE.
*		Default: 0 (unlimited)
*
*	sa_etm_max_num_event_subs
*		Max number of event subscriptions (InformInfo) per port/vport that can be registered,
*		active only if the sa_enhanced_trust_model is set to TRUE.
*		Default: 0 (unlimited)
*
*       sa_rate_threshold
*		Max number of SA requests per port/vport in 100 msec.
*		Default: 0 (unlimited)
*
*	sa_check_sgid_spoofing
*		If enabled every sa request is checked for gid spoofing.
*		Default is FALSE.
*
*	sm_priority
*		The priority of this SM as specified by the user.  This
*		value is made available in the SMInfo attribute.
*
*	master_sm_priority
*		The priority of this SM when it becomes MASTER.
*
* 	master_sm_lid
* 		Force master SM LID.
* 		Selected LID must match LMC that will be configured on SM device.
* 		Default: 0 (don't forces SM LID)
*
* 	enable_sm_port_failover
* 		Enable SM port failover.
* 		When SM runs on more than one GUID and,
* 		the main port goes down failover to one of the other ports.
* 		Default: FALSE (don't forces SM LID)
*
*	lmc
*		The LMC value used on this subnet.
*
*	rtr_lmc
*		The LMC value for Router in this subnet.
*		Valid only when global LMC is 0.
*		Otherwise overridden by global LMC.
*		This field is internal, and its value is calculated according
*		to the values of lids_per_rtr and lmc.
*
*	prism_an_lmc
*		The LMC value for aggregation nodes connected to prism switches in this subnet.
*		Each AN LID will be used in a different plane.
*		Note that this shouldn't be higher than log2(number of planes), since the PortInfo
*		might fail in such case.
*		Valid only when global and router LMC are 0. Otherwise overridden by global LMC.
*		HACK: Ideally the LMC needed per prism switch will be dependant on the number of
*		      planes in that specific prism switch, and not a global configuration.
*
*	lids_per_rtr
*		Sets maximum number of LIDs per router to support when calculating
*		inter-subnet path records, and sets LMC for routers (rtr_lmc) accordingly.
*		If global LMC is not zero, lids_per_rtr is limited by global LMC.
*		If set to 0, OpenSM will use number of lids according to global LMC.
*
*	lmc_esp0
*		Whether LMC value used on subnet should be used for
*		enhanced switch port 0 or not.  If TRUE, it is used.
*		Otherwise (the default), LMC is set to 0 for ESP0.
*
* 	flid_compression
* 		FLID compression ratio - how many leaf switches at max can share the same FLID.
*
*	max_op_vls
*		Limit the maximal operational VLs. default is 1.
*
*	max_neighbor_mtu
*		Limit the maximal neighbor MTU. (0 - no limit).
*
*	suppress_mc_pkey_traps
*		When bit 0 of this parameter is set the mc pkey trap
*		suppression is enabled.
*
*	reassign_lids
*		If TRUE cause all lids to be re-assigned.
*		Otherwise (the default),
*		OpenSM always tries to preserve as LIDs as much as possible.
*
*	ignore_other_sm_option
*		This flag is TRUE if other SMs on the subnet should be ignored.
*
*	disable_multicast
*		This flag is TRUE if OpenSM should disable multicast support.
*
*	light_sweep_spst
*		Flag indicating to query switch port states tables on light
*		sweeps.
*
*	max_msg_fifo_timeout
*		The maximal time a message can stay in the incoming message
*		queue. If there is more than one message in the queue and the
*		last message stayed in the queue more than this value the SA
*		request will be immediately returned with a BUSY status.
*
*	subnet_timeout
*		The subnet_timeout that will be set for all the ports in the
*		design SubnSet(PortInfo.vl_stall_life))
*
*	vl_stall_count
*		The number of sequential packets dropped that cause the port
*		to enter the VLStalled state.
*
*	leaf_vl_stall_count
*		The number of sequential packets dropped that cause the port
*		to enter the VLStalled state. This is for switch ports driving
*		a CA or router port.
*
*	head_of_queue_lifetime
*		The maximal time a packet can live at the head of a VL queue
*		on any port not driving a CA or router port.
*
*	leaf_head_of_queue_lifetime
*		The maximal time a packet can live at the head of a VL queue
*		on switch ports driving a CA or router.
*
*	local_phy_errors_threshold
*		Threshold of local phy errors for sending Trap 129
*
*	overrun_errors_threshold
*		Threshold of credits overrun errors for sending Trap 129
*
*	sminfo_polling_timeout
*		Specifies the polling timeout (in milliseconds) - the timeout
*		between one poll to another.
*
*	packet_life_time
*		The maximal time a packet can stay in a switch.
*		The value is send to all switches as
*		SubnSet(SwitchInfo.life_state)
*
*	dump_files_dir
*		The directory to be used for opensm-subnet.lst, opensm.fdbs,
*		opensm.mcfdbs, and default log file (the latter for Windows,
*		not Linux).
*
*	cache_files_dir
*		The directory to be used for SM cache files (such as guid2lid).
*
*	log_file
*		Name of the log file (or NULL) for stdout.
*
*	log_max_size
*		This option defines maximal log file size in MB. When
*		specified the log file will be truncated upon reaching
*		this limit.
*
*	qos
*		Boolean that specifies whether the OpenSM QoS functionality
*		should be off or on.
*
*	qos_policy_file
*		Name of the QoS policy file.
*
*	enhanced_qos_policy_file
*		Name of the enhanced QoS policy file.
*
*	enhanced_qos_vport0_unlimit_default_rl
*		Boolean that specifies whether default rate limit
*		of vport0 is unlimited for unspecified SLs in the
*		bandwidth rule
*
*	override_create_mcg_sl
*		Override multicast SL provided in join/create requests
*
*	accum_log_file
*		If TRUE (default) - the log file will be accumulated.
*		If FALSE - the log file will be erased before starting
*		current opensm run.
*
*	port_prof_ignore_file
*		Name of file with port guids to be ignored by port profiling.
*
*	port_profile_switch_nodes
*		If TRUE will count the number of switch nodes routed through
*		the link. If FALSE - only CA/RT nodes are counted.
*
*	sweep_on_trap
*		Received traps will initiate a new sweep.
*
*	routing_engine_names
*		Name of routing engine(s) to use.
*
*	enable_queries_during_routing
*		Boolean indicates whether to allow SM response for various SA
*		queries	during routing calculation.
*
*	allowed_sm_guids
*		String of allowed port GUIDs to run other SMs on them,
*		separated by commas.
*
*	avoid_throttled_links
*		This option will enforce that throttled switch-to-switch links
*		in the fabric are treated as 'broken' by the routing engines
*		(if they support it), and hence no path is assigned to these
*		underperforming links and a warning is logged instead.
*
*	connect_roots
*		The option which will enforce root to root connectivity with
*		up/down and fat-tree routing engines (even if this violates
*		"pure" deadlock free up/down or fat-tree algorithm)
*
*	max_cas_on_spine
*		Maximum number of CAs on switch to allow considering it
*		as spine instead of leaf by a routing algorithm.
*
*	find_roots_color_algorithm
*		Find root using coloring algorithm for tree based topologies.
*		Supported by updn and ar_updn routing engines.
*		Coloring algorithm uses BFS from leaf switches toward root switches.
*		Algorithm determine leaf switches according to number of hosts
*		connected to the switch. Number of host to determine that a switch
*		is a leaf, is specified by max_cas_on_spine parameter.
*
*	dfp_find_roots_color_algorithm
*		Find root using coloring algorithm for DF+ based topologies.
*		Supported by dfp and dfp2 routing engines.
*		Coloring algorithm uses BFS from leaf switches toward root switches.
*		Algorithm determine leaf switches according to number of hosts
*		connected to the switch. Number of host to determine that a switch
*		is a leaf, is specified by dfp_max_cas_on_spine parameter.
*
*	use_ucast_cache
*		When TRUE enables unicast routing cache.
*
*	lid_matrix_dump_file
*		Name of the lid matrix dump file from where switch
*		lid matrices (min hops tables) will be loaded
*
*	lfts_file
*		Name of the unicast LFTs routing file from where switch
*		forwarding tables will be loaded
*
*	root_guid_file
*		Name of the file that contains list of root guids that
*		will be used by fat-tree or up/dn routing (provided by User)
*
*	pgrp_policy_file
*		Name of the file that contains the port group policy data
*		will be used by port groups parser
*
*	topo_policy_file
*		Name of the file that contains the topology policy data
*		will be used by topology parser
*
*	rch_policy_file
*		Name of the file that contains the routing chain policy data
*		will be used by routing chain parser
*
*	device_configuration_file
*		Full path file name for device configuration file.
*
*	fast_recovery_enabled
*		Enable Fast Recovery feature (0 - ignore, 1 - disable, 2 - enable).
*
*	fast_recovery_conf_file
*		Full path file name for Fast Recovery configuration file.
*
*	routing_threads_num
*		Number of threads to be used for minhop/updn calculation
*
*	held_back_sw_file
*		Name of the file that contains list of node guids that
*		will be held-back (ignored) during discovery
*
*	cn_guid_file
*		Name of the file that contains list of compute node guids that
*		will be used by fat-tree routing (provided by User)
*
*	io_guid_file
*		Name of the file that contains list of I/O node guids that
*		will be used by fat-tree routing (provided by User)
*
*	port_shifting
*		This option will turn on port_shifting in routing.
*
*	ids_guid_file
*		Name of the file that contains list of ids which should be
*		used by Up/Down algorithm instead of node GUIDs
*
*	guid_routing_order_file
*		Name of the file that contains list of guids for routing order
*		that will be used by minhop and up/dn routing (provided by User).
*
*	pgft_structure
*		The definition of the expected Fat Tree topology:
*		PGFT(h; M1, M2,...Mh; W1,W2,...Wh; P1,P2,...Ph) or
*		QFT(h; M1, M2,...Mh; W1,W2,...Wh; P1,P2,...Ph)
*		Where:
*		h = height of tree (layers of switches)
*		Mi = Number of children for switches at level i (i=1 is leafs)
*		Wi = Number of different switch parents for switches at level i-1
*		Pi = For PGFT - number of parallel down links for switches at level i
*		     For QFT - number of additional children for switches at level i
*
*	offsweep_balancing_enabled
*		Rebalance the port load during heavy sweep routing
*
*	offsweep_balancing_window
*		Perform routing rebalancing offsweep_balancing_window
*		seconds after last heavy sweep if needed
*
*	sa_db_file
*		Name of the SA database file.
*
*	sa_db_dump
*		When TRUE causes OpenSM to dump SA DB at the end of every
*		light sweep regardless the current verbosity level.
*
*	sm_db_dump
*		When TRUE causes OpenSM to dump SM DB at the end of every
*		heavy sweep.
*
*	torus_conf_file
*		Name of the file with extra configuration info for torus-2QoS
*		routing engine.
*
*	exit_on_fatal
*		If TRUE (default) - SM will exit on fatal subnet initialization
*		issues.
*		If FALSE - SM will not exit.
*		Fatal initialization issues:
*		a. SM recognizes 2 different nodes with the same guid, or
*		   12x link with lane reversal badly configured.
*
*	honor_guid2lid_file
*		Always honor the guid2lid file if it exists and is valid. This
*		means that the file will be honored when SM is coming out of
*		STANDBY. By default this is FALSE.
*
*	daemon
*		OpenSM will run in daemon mode.
*
*	sm_inactive
*		OpenSM will start with SM in not active state.
*
*	babbling_port_policy
*		OpenSM will enforce its "babbling" port policy.
*
*	ipoib_mcgroup_creation_validation
*		OpenSM will validate IPoIB non-broadcast group parameters
*		against IPoIB broadcast group.
*
*	mcgroup_join_validation
*		OpenSM will validate multicast join parameters against
*		multicast group parameters when MC group already exists.
*
*	use_original_extended_sa_rates_only
*		Use only original extended SA rates (up through 300 Gbps
*		for 12x EDR). Option is needed for subnets with
*		old kernels/drivers that don't understand the
*		new SA rates for 2x link width and/or HDR link speed (19-22).
*
*	max_rate_enum
*		Enumeration of the maximal rate subnet supports. Option is
*		needed for subnets with old kernels/drivers that don't
*		understand new SA rates.
*		See also: use_original_extended_sa_rates_only.
*
*	use_optimized_slvl
*		Use optimized SLtoVLMappingTable programming (bit 0) if
*		device indicates it supports this.
*
*	use_optimized_port_mask_slvl
*		Use optimized SLtoVLMappingTable programming (bit 1) if
*		device indicates it supports this.
*
*	fsync_high_avail_files
*		Synchronize high availability in memory files
*		with storage.
*
*	perfmgr
*		Enable or disable the performance manager
*
*	perfmgr_redir
*		Enable or disable the saving of redirection by PerfMgr
*
*	perfmgr_sweep_time_s
*		Define the period (in seconds) of PerfMgr sweeps
*
*       event_db_dump_file
*               File to dump the event database to
*
*       event_plugin_name
*               Specify the name(s) of the event plugin(s)
*
*       event_plugin_options
*               Options string that would be passed to the plugin(s)
*
*	qos_options
*		Default set of QoS options
*
*	qos_ca_options
*		QoS options for CA ports
*
*	qos_sw0_options
*		QoS options for switches' port 0
*
*	qos_swe_options
*		QoS options for switches' external ports
*
*	qos_rtr_options
*		QoS options for router ports
*
*	congestion_control
*		Boolean that specifies whether OpenSM congestion control configuration
*		should be off or no.
*
*	cc_key
*		CCkey to use when configuring congestion control.
*
*	cc_max_outstanding_mads
*		Max number of outstanding CC mads that can be on the wire.
*
*	cc_key_enable
*		Enable CC key feature (0 - ignore, 1 - disable, 2 - enable).
*
*	cc_key_lease_period
*		The lease period used for CC keys.
*
*	cc_key_protect_bit
*		Protection level used for CC keys.
*
*	vs_key_enable
*		Enable VS key feature (0 - ignore, 1 - disable, 2 - enable).
*
*	vs_key_lease_period
*		The lease period used for VS keys. See also vs_key_protect_bit.
*
* 	vs_key_ci_protect_bits
*		Define protection level for VS keys.
*		    0: Protection is provided. However, VS managers are allowed
*		       to read the key by KeyInfo GET.
* 		    1: Protect both Configurational and Informational MADs. None
* 		       is allowed to read the key for lease period amount of time
* 		       (notice lease period 0 means infinite time).
*
*	vs_max_outstanding_mads
*		Max number of outstanding VS mads that can be on the wire.
*
*	key_mgr_seed
*		Seed used for key generation by the Key Manager.
*
*	enable_quirks
*		Enable high risk new features and not fully qualified
*		hardware specific work arounds
*
*	no_clients_rereg
*		When TRUE disables clients reregistration request.
*		Deprecated by client_rereg_mode.
*
*	client_rereg_mode
*		Control parameter for sending client reregisration options.
*		0 - Sending client reregistration disabled.
*		1 - Send client reregistration in LID manager.
*		2 - Send Client reregistration in link manager.
*
*	scatter_ports
*		When not zero, randomize best possible ports chosen
*		for a route. The value is used as a random key seed.
*
*	updn_lid_tracking_mode
*		Enable LID tracking mode for Up/Down.
*
*	updn_lid_tracking_converge_routes
*		With LID tracking mode:
*		when TRUE, consolidate routes to the same DLID
*		in the fabric as early as possible.
*		Otherwise, consolidate the routes as close
*		to destination as possible.
*
*	dfp_max_cas_on_spine
*		Maximum number of CAs on switch to allow considering it
*		as spine instead of leaf by DF+ routing algorithm.
*
*	max_seq_redisc
*		When not zero, defines the maximum number of sequential
*		failed discoveries that may be run, before SM will perform
*		the whole heavy sweep cycle.
*
*	rediscovery_max_link_penalty
*		If discovery MADs had timeouts, SM will attempt to rediscover these nodes using
*		different routes that avoid problematic links.
*		This parameter controls the maximum penalty a link can receive due to such timeouts.
*
*	rediscovery_link_penalty_step
*		The amount of penalty added to links per timeout of MAD going through them.
*
* 	mc_roots_file
*		Name of the file that contains mapping from MGID to switches
*		that OpenSM will try to use as roots for their multicast trees
*
*	per_module_logging_file
*		File name of per module logging configuration.
*
* 	max_alt_dr_path_retries
* 		Maximum number of attempts to find alternative direct route
* 		towards unresponsive ports.
*
* 	sa_pr_full_world_queries_allowed
* 		Enables SA to generate full World Path Records
* 		(for each pair of ports).
*
*	enable_crashd
*		When TRUE enables OpenSM crash daemon.
*
*	hm_unhealthy_ports_checks
*		When TRUE enable Unhealthy Ports feature.
*
*	hm_ports_health_policy_file
*		Unhealthy policy file.
*
*	hm_ca_reboot_action
*		Action to be taken for CAs/RTRs with Reboot condition.
*
*	hm_ca_unresponsive_action
*		Action to be taken for CAs/RTRs with Unresponsive condition.
*
*	hm_ca_noisy_action
*		Action to be taken for CAs/RTRs with Noisy condition.
*
*	hm_ca_seterr_action
*		Action to be taken for CAs/RTRs with SetErr condition.
*
*	hm_ca_flapping_action
*		Action to be taken for CAs/RTRs with Flapping link condition.
*
*	hm_ca_illegal_action
*		Action to be taken for CAs/RTRs with Illegal condition.
*
*	hm_ca_manual_action
*		Action to be taken for CAs/RTRs with manual condition.
*
*	hm_sw_reboot_action
*		Action to be taken for switch with Reboot condition.
*
*	hm_sw_unresponsive_action
*		Action to be taken for switch with Unresponsive condition.
*
*	hm_sw_noisy_action
*		Action to be taken for switch with Noisy condition.
*
*	hm_sw_seterr_action
*		Action to be taken for switch with SetErr condition.
*
*	hm_sw_flapping_action
*		Action to be taken for switch with Flapping link condition.
*
*	hm_sw_illegal_action
*		Action to be taken for switch with Illegal condition.
*
*	hm_sw_manual_action
*		Action to be taken for switch with manual condition.
*
*	hm_num_reboots
*		Number of reboots in a period to declare a node as unhealthy.
*
*	hm_reboots_period_secs
*		The period for counting number of reboots.
*
*	hm_num_set_err_sweeps
*		The number of sweeps that had that node report back an error
*		for a Set MAD.
*
*	hm_num_set_err_sweeps_window
*		The number of sweeps of which any node exceeding
*		hm_num_set_err_sweeps is declared unhealthy.
*
*	hm_num_no_resp_sweeps
*		The number of sweeps that had that node unresponsive.
*
*	hm_num_no_resp_sweeps_window
*		The number of sweeps of which any node exceeding
*		hm_num_no_resp_sweeps is declared unhealthy.
*
*	hm_num_traps
*		The number of received traps in a period to declare a node
*		as unhealthy.
*
*	hm_num_traps_period_secs
*		The period for counting received traps number.
*
*	hm_num_flapping_sweeps
*		The number of sweeps to declare a port as flapping.
*
*	hm_num_flapping_sweeps_window
*		The number of sweeps of which any port exceeding
*		hm_num_flapping_sweeps is declared unhealthy.
*
*	hm_num_illegal
*		Number of illegal SMPs a node may return to be declared
*		unhealthy.
*
*	force_heavy_sweep_window
*		Force heavy sweep after number of light sweep performed.
*
*	validate_smps
*		Enable SMPs validations
*
*	max_topologies_per_sw
*		Number of max topologies sw is allowed to be part of
*
*	auto_join_hca_wo_topo
*		Automatically join hca without topology to its sw topology
*
*	enable_inc_mc_routing
*		Enable incremental multicast routing
*
*	allow_sm_port_reset
*		Allow resetting SM port after changing port's properties
*
*	mepi_cache_enabled
*		Avoid sending MEPI to every port every heavy sweep.
*
*	virt_enabled
*		Enable virtualization support
*
*	virt_max_ports_in_process
*		Maximal number of ports to be processed simultaneously by
*		Virtualization Manager
*
*	virt_default_hop_limit
*		Default value for hop limit of path record where either source
*		or destination is virtual port.
*
*	enable_virt_rec_ext
*		Boolean indicates whether to extend PIR and NR to include virtual
*		records of vports with a unique lid.
*		If true, PIR and NR responses include VirtualPortRecords and
*		VirtualNodeRecords, respectively.
*
*	improved_lmc_path_distribution
*		Provides better local outgoing ports selection with LMC>0
*		for base LID + N (N in 1..2^LMC-1). Relevant for updn/minhop only
*
*	sweep_every_hup_signal
*		Flag indicating whether to perform heavy sweep in case of hup
*		signal received and no changes recognized in configuration
*
*	osm_stats_interval
*		Time interval [in min] between statistics dumps
*
*	osm_stats_dump_limit
*		The max size [in MB] of statisic dump file
*
*	osm_stats_dump_per_sm_port
*		Dump MADs statistics per sm port in different files
*
*	sharp_enabled
*		Enable sharp support
*	rtr_aguid_enable
*		Enable creation of alias guids for algo router.
*	rtr_pr_flow_label
*		Inter subnet PathRecord FlowLabel
*	rtr_pr_tclass
*		Inter subnet PathRecord TClass
*	rtr_pr_sl
*		Inter subnet PathRecord SL
*	rtr_pr_mtu
*		Inter subnet PathRecord MTU
*	rtr_pr_rate
*		Inter subnet PathRecord Rate
*	router_policy_file
*		Name of the route policy file
*	router_policy_enabled
*		Enable router policy feature support
*	aguid_default_hop_limit
*		Default value for hop limit of path record where either source
*		or destination is alias guid.
*	load_inform_info_from_sadb
*		Enable load inform info from sadb file
*	verbose_bypass_policy_file
*		File name for log verbosity bypass policy
*	sa_db_dump_file
*		Full path file name for sa db dump file
*	enable_subnet_lst
*		Enable dumping subnet lst dump file
*	subnet_lst_file
*		Full path file name for subnet lst dump file
*	virt_dump_file
*		Full path file name for virtualization dump file
*	dor_hyper_cube_mode
*		Perform Hyper-Cube topology analysis when using DOR routing
*		algorithm.
*	additional_gi_supporting_devices
*		Devices which supports general info mad in addition to the
*		devices which are defined by default
*
*	additional_mepi_force_devices
*		Devices which we force sending mepi for, in addition to the
*		devices which are defined by default
*
*	disable_default_gi_support
*		Disable general info support.
*
*	activity_report_subjects
*		Subjects to include in activity report
*
*	adv_routing_engine_name
*		Name of configured advanced routing engine.
*
*	ar_mode
*		Adaptive routing mode.
*
*	shield_mode
*		Fast link fault recovery mode.
*
*	pfrn_sl
* 		SL for pFRN communication
*
* 	pfrn_mask_force_clear_timeout
*		Maximal time (in seconds) since last mask clear, after which
*		mask must cleared. During that period, mask bits may have been
*		set.
*
* 	pfrn_mask_clear_timeout
* 		Time since latest pFRN for a specific sub group was received,
* 		after which the entire mask must be cleared
*
* 	pfrn_over_router_enabled
* 		Enable PFRN over router.
*
*	enable_ar_by_device_cap
*		Enable AR only for devices that support packet reordering.
*
*	enable_ar_group_copy
*		Enable AR group copy optimization.
*
*	enable_empty_arlft_optimization
*		Avoid sending empty ARLFT blocks optimization.
*
* 	ar_transport_mask
* 		AR trasnsport bitmask for AR enabled transport.
*
*	ar_tree_asymmetric_flow
*		AR Asymmetric trees max flow algorithm for calculating
*		AR sub groups for tree topologies
*		Supported values:
*		0 - Disable the algorithm.
*		1 - Enable with 1 sub group support.
*		2 - Enable with 2 sub groups on leaf switches.
*
* 	routing_flags
*		Bit mask of flags to control various options and behavior
*		of routing engines.
*		Supported values:
*		0x1 - Enable switch rank adjustments for tree based routing engines.
*
*	smp_threads
*		Number of threads to serve SMP requests
*
*	gmp_threads
*		Number of threads to serve GMP requests
*
*	gmp_traps_threads_num
*		Number of threads used for key violation traps of VS and CC
*		class managers.
*
*	dump_ar
*		dump ar data to file
*
*	cache_ar_group_id
*		Load GUID to AR group ID from cache file.
*		When enabled, it can reduce AR group configuration changes after restart.
*
*	dfp_down_up_turns_mode
*		Allow or forbid down up turns in intermediate dfp group.
*
*	enable_vl_packing
*		Use single VL per QoS on traffic between CA and switch.
*
*	service_name2key_map_file
*		Name of the file that maps service name to service key.
*
*	max_wire_smps_per_device
*		Maximum number of SMPs sent in parallel to the same port.
*		Currently, the supported MADs are:
*		portInfo/Extended portInfo, LFTs, AR LFTs,
*		AR group table, AR copy group table, RN sub group direction.
*
*	opts_def
*		Structure that keeps all option defaults
*
*	rtr_selection_function_s
*		Selection function used for router selection.
*
*	rtr_selection_function
*		Type of selection function used for router selection.
*
*	rtr_selection_seed
*		Parameter used for router selection randomization.
*
*	rtr_selection_algo_parameters_s
*		Indicates which parameters to use in router selection algorithm.
*
*	rtr_selection_params_mask
*		Bitmask for router selection algorithm, indicates which
*		parameters to use for randomization.
*
*	global_flid_start
*		A first Floating LID (FLID) in the global FLID range
*
*	global_flid_end
*		A last Floating LID (FLID) in the global FLID range
*
*	local_flid_start
*		A first Floating LID (FLID) in the local subnet FLID range
*
*	local_flid_end
*		A last Floating LID (FLID) in the local subnet FLID range
*
*	get_mft_tables
*		get MFT tables on first time master sweep
*
*	hbf_sl_mask
*		SL mask of enabled for HBF (Hash Based Forwarding) SLs.
*		This mask is used also for NVLink HBF:
*			In NVL5 systems, IB traffic runs on SLs 0,1, whereas
*			NVL traffic uses SLs 2-5. Hence, both HBFs can be
*			configured by the same mask while FW applies the relevant
*			HBF accordingly for each MAD received.
*
*	hbf_hash_type
*		HBF hash type (0 - CRC, 1 - XOR)
*
*	hbf_seed_type
*		HBF seed type (0 - configurable, 1 - random seed)
*
*	hbf_seed
*		HBF seed value
*
*	hbf_hash_fields
*		A bitmask of fields that configure HBF
*
*	enable_performance_logging
*		Enable performance logging.
*
*	nvlink_enable
*		 Enable NVLink subnet configuration.
*
* 	nvl_reassign_rails
*		If TRUE cause all NVLink rails to be re-assigned,
*		including for active ports.
*		Otherwise (the default), OpenSM always preserves rails from
*		port2rail cache and avoids configuring new rails for active ports.
*
*
* 	nvl_hbf_hash_type
*		NVL HBF hash type (0 - CRC, 1 - Packet Hash)
*
* 	nvl_hbf_packet_hash_bitmask
*		A bitmask of the packet hash field used for NVL HBF
*		when configured with hash type 1 (Packet Hash).
*
* 	nvl_hbf_seed
* 		NVL HBF seed value
*
* 	nvl_hbf_fields_enable
* 		A bitmask of fields that configure NVL HBF
*
* 	nvl_partition_reroute_wait
*		Indicates how many STO (0.5 sec) times to wait between configuring partition routing tables
*		to activating the partition.
*
*	nvl_cage_allocation_mode
*		Indicates whether to limit LAAS to allocate for each cage one partition at most.
*
*	nvl_cnd_for_access_links_enabled
*		Indicates whether to set access ports that are in INIT state to be in CND.
*		This is to enable access link resiliency.
*
*	nvl_cnd_for_init_trunks_enabled
*		Indicates whether to set trunk ports that are in INIT state to be in CND.
*		This parameter is relevant only for setting trunk links to CND
*		while they are in INIT state (not relevant to reroute CND flow).
*		This is to enable trunk link resiliency.
*
*	tenants_policy_file
*		 Physical-Virtual guid mapping file.
*
*	reply_lid_smps_in_dr
*		A boolean indicating whether SM should respond to LID-routed SMPs with DR.
*		If true, it will respond with DR. If false, it will respond with LID-routed.
*
*	respond_unknown_lid_traps
*		A boolean indicating whether SM should respond to LID-routed SMP traps sent by an
*		unknown sender.
*
* 	fabric_mode_profile
* 		String indicates which profile from fabric mode policy file to use
*
*	fabric_mode_policy_file
*		The file holding the Fabric Mode policy
*
*       issu_mode
*               ISSU operations mode.
*
*       issu_timeout
*               ISSU timeout (in seconds) to wait before sending MADs to upgrading switch.
#
#	issu_pre_upgrade_time;
*               ISSU timeout (in seconds) to wait between receiving ISSU request and approving it.
*
*	enable_sa
*		If set, allow SA requests.
*
*	drop_unreachable_ports_enabled
*		If set, the SM will drop all unreachable ports in each sweep.
*
* SEE ALSO
*	Subnet object
*********/

typedef struct osm_ar_configuration_params {
	uint8_t ar_mode;
	uint8_t shield_mode;
	boolean_t pfrn_mode;
	osm_adv_routing_type_enum ar_algorithm;
	unsigned int hbf_weights[OSM_AR_HBF_WEIGHT_SUBGROUP_NUM];
	unsigned int hbf_weights_configured;
} ar_configuration_params_t;

typedef struct osm_sm_state_info {
	uint8_t last_sm_port_state;
	uint8_t sm_state;
	boolean_t sm_port_manually_disabled;
} osm_sm_state_info_t;
/*
* FIELDS
*	last_sm_port_state
*		Last state of this SM's port.
*		OSM_SM_PORT_STATE_DOWN is down and OSM_SM_PORT_STATE_UP is up.
*
*	sm_state
*		The high-level state of the SM.  This value is made available
*		in the SMInfo attribute.
*
*	sm_port_manually_disabled
*		Boolean indicating whether SM has disabled its port.
*		SM disables its port during standalone sweep to avoid interference from other nodes.
*
* SEE ALSO
*	Subnet object
*********/

/****s* OpenSM: Subnet/osm_sm_port_data_t
* NAME
*	osm_sm_port_data_t
*
* DESCRIPTION
*	A struct storing information needed to identify whether a given node/physp is an SM
*	node/physp or its peer.
*	Note that the SM may have more than one physp/peer if the SM is on a planarized CA.
*
* SYNOPSIS
*/
typedef struct osm_sm_port_data {
	struct {
		ib_net64_t sm_node_guid;
		ib_net64_t peer_node_guid;
		uint8_t sm_port_num;
		uint8_t peer_port_num;
		boolean_t peer_found;
	} port[OSM_UMAD_MAX_PORTS_PER_CA];
	uint8_t num_ports;
} osm_sm_port_data_t;
/*
* FIELDS
*	port
*		An array that contains data about each port.
*
*	num_ports
*		Number of ports contained in the array.
*
* SEE ALSO
*********/

/****s* OpenSM: Subnet/osm_buffer_t
* NAME
*	osm_buffer_t
*
* DESCRIPTION
*	A struct holding a buffer and its size.
*
* SYNOPSIS
*/
typedef struct osm_buffer {
	void * data;
	size_t size;
} osm_buffer_t;
/*
* FIELDS
*	data
*		A pointer to the raw data.
*
*	size
*		The size of `data`, in bytes.
*
* SEE ALSO
*********/

/****s* OpenSM: Subnet/osm_subn_t
* NAME
*	osm_subn_t
*
* DESCRIPTION
*	Subnet structure.  Callers may directly access member components,
*	after grabbing a lock.
*
* TO DO
*	This structure should probably be volatile.
*
* SYNOPSIS
*/
typedef struct osm_subn {
	struct osm_opensm *p_osm;
	cl_qmap_t sw_guid_tbl;
	osm_aports_systems_tables_t aports_systems_tables;
	boolean_t is_planarized;
	cl_qmap_t node_guid_tbl;
	cl_qmap_t gpu_guid_tbl;
	cl_qmap_t port_guid_tbl;
	cl_qmap_t alias_port_guid_tbl;
	cl_qmap_t assigned_guids_tbl;
	cl_qmap_t rtr_guid_tbl;
	cl_qlist_t prefix_routes_list;
	cl_qmap_t prtn_pkey_tbl;
	cl_lwr_t prtn_data;
	cl_qmap_t sm_guid_tbl;
	cl_qlist_t sa_sr_list;
	cl_qlist_t sa_infr_list;
	cl_qlist_t alias_guid_list;
	cl_qlist_t perflog_list;
	cl_qmap_t held_back_sw_tbl;
	cl_fmap_t rcvd_traps_tbl;
	cl_qlist_t rcvd_traps_list;
	cl_qlist_t sa_reports_list;
	cl_fmap_t unhealthy_ports_tbl;
	cl_fmap_t healthy_ports_tbl;
	cl_list_t ignored_nodes_list;
	cl_u64_vector_t allowed_sm_guid_tbl;
	cl_ptr_vector_t port_lid_tbl;
	cl_ptr_vector_t gpu_alid_tbl;
	uint16_t max_lid;
	uint16_t max_mlid;
	ib_net16_t master_sm_base_lid;
	ib_net16_t sm_base_lid;
	ib_net64_t sm_port_guid;
	osm_sm_port_data_t sm_port_data;
	osm_sm_state_info_t sm_state_info;
	uint8_t sm_port_idx;
	uint8_t active_sm_priority;
	osm_subn_opt_t opt;
	struct osm_qos_policy *p_qos_policy;
	atomic32_t tid;
	uint16_t max_ucast_lid_ho;
	uint16_t max_mcast_lid_ho;
	uint8_t min_ca_mtu;
	uint8_t min_ca_rate;
	uint8_t min_data_vls;
	uint8_t min_sw_data_vls;
	boolean_t ignore_existing_lfts;
	boolean_t hbf_reroute_optimization;
	boolean_t subnet_initialization_error;
	boolean_t force_heavy_sweep;
	boolean_t force_reroute;
	boolean_t in_sweep_hop_0;
	boolean_t in_sweep_sm_remotes_guids;
	boolean_t force_first_time_master_sweep;
	boolean_t first_time_master_sweep;
	boolean_t coming_out_of_standby;
	boolean_t sm_port_changed;
	boolean_t sweeping_enabled;
	boolean_t in_heavy_sweep;
	boolean_t found_ignored_port;
	boolean_t had_mad_timeout;
	boolean_t restore_links;
	boolean_t is_ar_plugin_active;
	unsigned need_update;
	boolean_t force_client_rereg;
	cl_fmap_t mgrp_mgid_tbl;
	cl_fmap_t mc_roots_tbl;
	ib_net64_t global_mc_root_guid;
	osm_db_domain_t *p_g2m;
	osm_db_domain_t *p_neighbor;
	osm_db_domain_t *p_an2an;
	osm_db_domain_t *p_g2cckey;
	osm_db_domain_t *p_g2vskey;
	osm_db_domain_t *p_g2n2n_mgrkey;
	osm_db_domain_t *p_g2n2nkey;
	st_table *p_topoconfig_hash_tbl;
	void *mboxes[IB_LID_MCAST_END_HO - IB_LID_MCAST_START_HO + 1];
	struct osm_routing_engine *routing_engine_list;
	struct osm_routing_engine *last_routing_engine;
	struct osm_routing_engine *routing_engine_used;
	struct osm_routing_engine *default_routing_engine;
	struct osm_routing_engine *default_planarized_routing_engine;
	osm_adv_routing_engine_t *adv_routing_engine;
	ar_configuration_params_t ar_conf_params;
	int routing_chain_engine_id;
	boolean_t no_fallback_routing_engine;
	boolean_t is_auto_engine_used;
	osm_topology_type_enum detected_topo;
	uint8_t min_path_bit;
	uint8_t max_path_bit;
	uint32_t light_sweep_counter;
	cl_qmap_t border_sw_tbl;
	cl_list_t leaf_switches_list;
	ib_net64_t hub_switch_guid[IB_MAX_NUM_OF_PLANES_XDR + 1];
	boolean_t force_multicast_rerouting;
	boolean_t reroute_all_mlids;
	boolean_t found_link_for_reset;
	boolean_t hup_signal;
	cl_vector_t bad_mkey_context_list;
	cl_fmap_t service_keys_tbl;
	boolean_t active_sm_guid[OSM_MAX_BINDING_PORTS];
	boolean_t pfrn_supported;
	boolean_t pfrn_config_change;
	boolean_t topo_config_enabled;
	cl_qlist_t event_subscriber_list;
	cl_qlist_t routing_subscriber_list;
	cl_qlist_t partitions_subscriber_list;
	boolean_t all_healthy;
	boolean_t enable_sweep;
	boolean_t enable_access_link_retraining;
	osm_fabric_mode_data_t fabric_mode_data;
	uint8_t key_seed[64];
	cl_spinlock_t seed_lock;
} osm_subn_t;
/*
* FIELDS
*	sw_guid_tbl
*		Container of pointers to all Switch objects in the subnet.
*		Indexed by node GUID.
*
*	aports_systems_tables
*		Container of all the planarized aports system objects in the subnet.
*
* 	is_planarized
* 		This flag is set to TRUE when there is any planarized system in the subnet
*
*	node_guid_tbl
*		Container of pointers to all Node objects in the subnet.
*		Indexed by node GUID.
*
*	gpu_guid_tbl
*		Container of pointers to all GPU objects in the subnet.
*		Indexed by node GUID.
*
*	port_guid_tbl
*		Container of pointers to all Port objects in the subnet.
*		Indexed by port GUID.
*
*	rtr_guid_tbl
*		Container of pointers to all Router objects in the subnet.
*		Indexed by node GUID.
*
*	prtn_pkey_tbl
*		Container of pointers to all Partition objects in the subnet.
*		Indexed by P_KEY.
*
*	prtn_data
*		Partition configuration data.
*		Produced by UFM plugin, consumed by SM.
*
*	sm_guid_tbl
*		Container of pointers to SM objects representing other SMs
*		on the subnet.
*
*	held_back_sw_tbl
*		Container of empty items indexed by node guid.
*		The node guid keys are used to store info about switches
*		configured as held-back
*
*	unhealthy_ports_tbl
*		Table of all unhealthy ports of the subnet.
*
*	healthy_ports_tbl
*		Table of all the ports that are marked as healthy by unhealthy
*		configuration file.
*
*	allowed_sm_guid_tbl
*		Container of all the allowed port GUIDs to run other SMs on them
*
*	port_lid_tbl
*		Container of pointers to all Port objects in the subnet.
*		Indexed by port LID.
*
*	max_lid
*		The max lid value in the subnet.
*
*	max_mlid
*		The max mlid value in the subnet.
*
*	master_sm_base_lid
*		The base LID owned by the subnet's master SM.
*
*	sm_base_lid
*		The base LID of the local port where the SM is.
*
*	sm_port_guid
*		This SM's own port GUID.
*
*	sm_port_data
*		Array of minimal information used to identify nodes/physical ports that belong to
*		the SM or are its peers.
*		Used to avoid setting SM node/ports and its peers as unhealthy/heldback.
*
*	sm_state_info
*		This SM's state information.
*
*	active_sm_priority
*		Active SM priority, In case SM in MASTER state it will be
*		MAX(sm_priority, master_sm_priority) and sm_priority otherwise.
*
*	opt
*		Subnet options structure contains site specific configuration.
*
*	p_qos_policy
*		Subnet QoS policy structure.
*
*	max_ucast_lid_ho
*		The minimal max unicast lid reported by all switches
*
*	max_mcast_lid_ho
*		The minimal max multicast lid reported by all switches
*
*	min_ca_mtu
*		The minimal MTU reported by all CAs ports on the subnet
*
*	min_ca_rate
*		The minimal rate reported by all CA ports on the subnet
*
*	ignore_existing_lfts
*		This flag is a dynamic flag to instruct the LFT assignment to
*		ignore existing legal LFT settings.
*		The value will be set according to :
*		- Any change to the list of switches will set it to high
*		- Coming out of STANDBY it will be cleared (other SM worked)
*		- Set to FALSE upon end of all lft assignments.
*
*	hbf_reroute_optimization
*		This flag indicates that routing engine can reuse existing LFTs
*		when HBF is enabled on all SLs on all switches.
*		This optimization can be used to to minimize changes
*		in forwarding tables.
*
*	subnet_initialization_error
*		Similar to the force_heavy_sweep flag. If TRUE - means that
*		we had errors during initialization (due to SubnSet requests
*		that failed). We want to declare the subnet as unhealthy, and
*		force another heavy sweep.
*
*	force_heavy_sweep
*		If TRUE - we want to force a heavy sweep. This can be done
*		either due to receiving of trap - meaning there is some change
*		on the subnet, or we received a handover from a remote sm.
*		In this case we want to sweep and reconfigure the entire
*		subnet. This will cause another heavy sweep to occur when
*		the current sweep is done.
*
*	force_reroute
*		If TRUE - we want to force switches in the fabric to be
*		rerouted.
*
*	in_sweep_hop_0
*		When in_sweep_hop_0 flag is set to TRUE - this means we are
*		in sweep_hop_0 - meaning we do not want to continue beyond
*		the current node.
*		This is relevant for the case of SM on switch, since in the
*		switch info we need to signal somehow not to continue
*		the sweeping.
*
*	in_sweep_sm_remotes_guids
*		When in_sweep_sm_remote_guids flag is set to TRUE - this means we are
*		in sweep_sm_remotes_guids - meaning we do not want to continue beyond
*		the SM remotes nodes.
*		This is relevant for the case of SM on planarized CA, since then the
*		SM might have multiple remote switches which we need to discover, and
*		we need to signal somehow not to continue the sweeping.
*
*       force_first_time_master_sweep
*		This flag is used to avoid race condition when Master SM being
*		in the middle of very long configuration stage of the heavy sweep,
*		receives HANDOVER from another MASTER SM. When the current heavy sweep
*		is finished, new heavy sweep will be started immediately.
*		At the beginning of the sweep, opensm will set first_time_master_sweep,
*		force_heavy_sweep and coming_out_of_standby flags in order to allow full
*		reconfiguration of the fabric. This is required as another MASTER SM could
*		change configuration of the fabric before sending HANDOVER to MASTER SM.
*
*	first_time_master_sweep
*		This flag is used for the PortInfo setting. On the first
*		sweep as master (meaning after moving from Standby|Discovering
*		state), the SM must send a PortInfoSet to all ports. After
*		that - we want to minimize the number of PortInfoSet requests
*		sent, and to send only requests that change the value from
*		what is updated in the port (or send a first request if this
*		is a new port). We will set this flag to TRUE when entering
*		the master state, and set it back to FALSE at the end of the
*		drop manager. This is done since at the end of the drop manager
*		we have updated all the ports that are reachable, and from now
*		on these are the only ports we have data of. We don't want
*		to send extra set requests to these ports anymore.
*
*	coming_out_of_standby
*		TRUE on the first sweep after the SM was in standby.
*		Used for nulling any cache of LID and Routing.
*		The flag is set true if the SM state was standby and now
*		changed to MASTER it is reset at the end of the sweep.
*
*	sweeping_enabled
*		FALSE - sweeping is administratively disabled, all
*		sweeping is inhibited, TRUE - sweeping is done
*		normally
*
*	in_heavy_sweep
*		Indicates whether heavy sweep is in process or not
*
*	had_mad_timeout
*		Indicates whether there was a timeout on some MAD in current sweep.
*		Used to decide whether to rediscover nodes.
*
*	need_update
*		This flag should be on during first non-master heavy
*		(including pre-master discovery stage)
*
*	mgrp_mgid_tbl
*		Container of pointers to all Multicast group objects in
*		the subnet. Indexed by MGID.
*
*	mc_roots_tbl
*		Map between MGID to lists of switches that will be used
*		multicast tree roots.
*
*	mboxes
*		Array of pointers to all Multicast MLID box objects in the
*		subnet. Indexed by MLID offset from base MLID.
*
*	routing_engine_list
*		List of routing engines that should be tried for use.
*
*	routing_engine_used
*		Indicates which routing engine was used to route a subnet.
*
*	routing_chain_engine_id
*		holds the routing chain engine id used for this subnet.
*		when the main engine is not chain this field will be 0.
*
*	no_fallback_routing_engine
*		Indicates if default routing engine should not be used.
*
*	is_auto_engine_used
*		Boolean indicates whether 'auto' is specified in routing engines
*		list. If true, topology detection algorithm is triggered.
*
* 	detected_topo
*		Detected topology type when using 'auto' routing engine.
*
*	path_bit
*		limit routing for subnet to specific lmc LID offset
*
*	light_sweep_counter
*		Number of light sweep performed since last heavy sweep.
*
*	border_sw_tbl
*		Container of pointers to switches which are part of other
*		topologies subnets.
*
*	leaf_switches_list
*		a list of leaf switches updated by routing engine
*
*	hub_switch_guid
*		Guid of the hub switch used for calculating missing LID routes.
*
*	force_multicast_rerouting
*		Flag indicating whether to force rerouting all multicast trees.
*
*	reroute_all_mlids
*		Flag indicating whether to reroute all mlids at next multicast
*		processing.
*
*	found_link_for_reset
*		Flag indicating whether there are links that need to be reset.
*
*	hup_signal
*		Indication whether hup signal received
*
* 	bad_mkey_context_list
* 		Collection of objects to associate expected bad mkey traps with
* 		active ports without peers
*
* 	service_keys_tbl
* 		Map service name to service key
* 		(IBA ServiceRecord 15.2.5.14)
*
* 	pfrn_supported
*		Boolean indicates whether subnet supports pFRN: routing engine
*		supports pFRN and at least one subnet switch supports pFRN
*		(according to ARInfo pFRN support bit).
*
*	pfrn_config_change
*		Boolean indicates whether pFRN parameters were changed or
*		pfrn_supported is changed to be supported, which triggers
*		re-sending of pFRN configuration to all switches.
*
*	topo_config_enabled
*		If set to true The SM will adjust it’s operational
*		mode to take into account the topo_config file.
*
*	partitions_subscriber_list
*		List of subscribers that provide partitions related operations.
*		Currently the operation that is supported is to update the partitions
*		from the subscriber (Instead of reading from the file).
*
*	all_healthy
*		If set to true The SM will adjust it’s operational
*		mode, and will not add any new unhealthy ports
*		unless specified in unhealthy policy file.
*
*	enable_sweep
*		SM will wait until the flag enabled to perform LIGHT/HEAVY sweeps.
*
*	enable_access_link_retraining
*		Enable access link retraining feature (CND for access links). NVLINK only.
*
*	key_seed
*		Seed used for MAD key generation.
*
* SEE ALSO
*	Subnet object
*********/

/*
 * Can be called only if SM lock is released,
 * because there is an order between sm_lock and state_lock
 * state_lock always comes before sm_lock.
 */
uint8_t osm_get_sm_last_port_state(struct osm_sm *p_sm);

/*
 * Can be called only if SM lock is released,
 * because there is an order between sm_lock and state_lock.
 * state_lock always comes before sm_lock.
 */
void osm_set_sm_last_port_state(struct osm_sm *p_sm, uint8_t last_sm_port_state);

/*
 * Can be called only if SM lock is released,
 * because there is an order between sm_lock and state_lock
 * state_lock always comes before sm_lock.
 */
boolean_t osm_get_sm_port_manually_disabled(struct osm_sm *p_sm);

/*
 * Can be called only if SM lock is released,
 * because there is an order between sm_lock and state_lock.
 * state_lock always comes before sm_lock.
 */
void osm_set_sm_port_manually_disabled(struct osm_sm *p_sm, boolean_t sm_port_manually_disabled);

/*
 * Can be called only if SM lock is released,
 * because there is an order between sm_lock and state_lock
 * state_lock always comes before sm_lock.
 */
osm_sm_state_info_t osm_get_sm_state_info(struct osm_sm *p_sm);

typedef enum osm_topoconfig_state_enum {
	OSM_TOPOCONFIG_NONE,
	OSM_TOPOCONFIG_DISABLED,
	OSM_TOPOCONFIG_NO_DISCOVER,
	OSM_TOPOCONFIG_ACTIVE
} osm_topoconfig_state_enum_t;

typedef struct osm_neighbor_info_ {
	uint64_t guid;
	uint8_t node_type;
	uint8_t port;
	osm_topoconfig_state_enum_t state;
} osm_neighbor_info_t;

typedef struct osm_neighbors_info_ {
	osm_neighbor_info_t neighbors[IB_NODE_NUM_PORTS_MAX + 1];
	uint8_t max_port_num;
	uint8_t node_type;
} osm_neighbors_info_t;

osm_neighbors_info_t *osm_topoconfig_lookup(st_table *p_topoconfig_hash_tbl, uint64_t guid);
osm_neighbor_info_t *osm_topoconfig_lookup_port(st_table *p_topoconfig_hash_tbl, uint64_t guid, uint8_t port);
boolean_t osm_topoconfig_neighbor_is_disabled(osm_neighbors_info_t *p_neighbors_info, uint8_t port);

/****s* OpenSM: Subnet/osm_assigned_guids_t
* NAME
*	osm_assigned_guids_t
*
* DESCRIPTION
*	SA assigned GUIDs structure.
*
* SYNOPSIS
*/
typedef struct osm_assigned_guids {
	cl_map_item_t map_item;
	ib_net64_t port_guid;
	ib_net64_t assigned_guid[1];
} osm_assigned_guids_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	port_guid
*		Base port GUID.
*
*	assigned_guids
*		Table of persistent SA assigned GUIDs.
*
* SEE ALSO
*	Subnet object
*********/

/****s* OpenSM: Subnet/osm_mc_roots_list_t
* NAME
*	osm_mc_roots_list_t
*
* DESCRIPTION
* 	Container of list of pointers to switches that OpenSM should try to use
* 	as roots when calculating multicast trees.
*
* SYNOPSIS
*/
typedef struct osm_mc_roots_list {
	cl_fmap_item_t map_item;
	ib_gid_t mgid;
	cl_list_t switch_list;
} osm_mc_roots_list_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_fmap.  MUST BE FIRST MEMBER!
*
*	switch_list
*		List of pointers to osm_switch_t objects.
*
* SEE ALSO
*	Subnet object
*********/

/****s* OpenSM: Subnet/osm_service_key_t
* NAME
*	osm_service_key_t
*
* DESCRIPTION
* 	Container of service key that service name is associated with.
*
* SYNOPSIS
*/
typedef struct osm_service_key {
	cl_fmap_item_t map_item;
	char service_name[IB_SERVICE_NAME_SIZE];
	uint8_t service_key[16];
} osm_service_key_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_fmap.  MUST BE FIRST MEMBER!
*
*	service_key
*		The Service Key may be used to allow authentication
*		of the creation, replacement and deletion of ServiceRecords
*		with selected ServiceNames.
*
* SEE ALSO
*	Subnet object
*********/

/****s* OpenSM: Subnet/osm_bad_mkey_context
* NAME
*	osm_bad_mkey_context
*
* DESCRIPTION
*	Description
*
* SYNOPSIS
*/
typedef struct osm_bad_mkey_context {
	ib_net64_t m_key;
	struct osm_physp *p_physp;
} osm_bad_mkey_context_t;
/*
* FIELDS
*	m_key
*		M_Key used in the request
*
*	p_physp
*		Pointer to osm_physp object associated with request
*
*********/

/****f* OpenSM: Subnet/osm_subn_construct
* NAME
*	osm_subn_construct
*
* DESCRIPTION
*	This function constructs a Subnet object.
*
* SYNOPSIS
*/
void osm_subn_construct(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a Subnet object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_subn_init, and osm_subn_destroy.
*
*	Calling osm_subn_construct is a prerequisite to calling any other
*	method except osm_subn_init.
*
* SEE ALSO
*	Subnet object, osm_subn_init, osm_subn_destroy
*********/

/****f* OpenSM: Subnet/osm_subn_destroy
* NAME
*	osm_subn_destroy
*
* DESCRIPTION
*	The osm_subn_destroy function destroys a subnet, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_subn_destroy(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a Subnet object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified Subnet object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_subn_construct
*	or osm_subn_init.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_init
*********/

/****f* OpenSM: Subnet/osm_subn_init
* NAME
*	osm_subn_init
*
* DESCRIPTION
*	The osm_subn_init function initializes a Subnet object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_subn_init(IN osm_subn_t * p_subn,
			      IN struct osm_opensm *p_osm,
			      IN const osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object to initialize.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	IB_SUCCESS if the Subnet object was initialized successfully.
*
* NOTES
*	Allows calling other Subnet methods.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy
*********/

/*
  Forward references.
*/
struct osm_mad_addr;
struct osm_madw;
struct osm_log;
struct osm_switch;
struct osm_physp;
struct osm_port;
struct osm_mgrp;
struct osm_router;

/****f* OpenSM: Helper/osm_get_gid_by_mad_addr
* NAME
*	osm_get_gid_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester gid in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
ib_api_status_t osm_get_gid_by_mad_addr(IN struct osm_log *p_log,
					IN const osm_subn_t * p_subn,
					IN struct osm_mad_addr *p_mad_addr,
					OUT ib_gid_t * p_gid);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
*	p_gid
*		[out] Pointer to the GID structure to fill in.
*
* RETURN VALUES
*     IB_SUCCESS if able to find the GID by address given.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_sm_physp_by_dr
* NAME
*	osm_get_sm_physp_by_dr
*
* DESCRIPTION
*	This function is used to select the correct physical port in SM osm_port object, based on
*	the binding port specified in the direct path object.
*	The direct path can specify a different exit port that the one that is currently used in
*	osm_port->p_physp. This is relevant for SM running on a planarized CA.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp * osm_get_sm_physp_by_dr(IN const osm_subn_t * p_subn,
					  IN const osm_dr_path_t * p_path,
					  IN uint8_t sm_binding_port_index);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_path
*		[in] Pointer to a DR path object
*
*	sm_binding_port_index
*		[in] The index of the SM bind to use
*
* RETURN VALUES
*	Pointer to SM physical port object used as the origin of the path, if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_physp_by_mad_addr
* NAME
*	osm_get_physp_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester physical port in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp *osm_get_physp_by_mad_addr(IN struct osm_log *p_log,
					     IN const osm_subn_t * p_subn,
					     IN struct osm_mad_addr
					     *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester physical port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/


/****f* OpenSM: Helper/osm_get_ext_physp_by_dr
* NAME
*	osm_get_ext_physp_by_dr
*
* DESCRIPTION
*	Looks for the last physical port in the given DR path
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp *osm_get_ext_physp_by_dr(IN struct osm_sm *sm,
					  IN const osm_dr_path_t * p_path);
/*
* PARAMETERS
*	sm
*		[in] Pointer to the SM object.
*
*	p_path
*		[in] Pointer to the DR path object to traverse
*
* RETURN VALUES
*	Pointer to the last physical port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_physp_by_dr
* NAME
*	osm_get_physp_by_dr
*
* DESCRIPTION
*	Looks for the requester physical port by the given DR path
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp *osm_get_physp_by_dr(IN struct osm_log * p_log,
				      IN const osm_subn_t * p_subn,
				      IN const osm_dr_path_t * p_path,
				      IN uint8_t sm_binding_port_index);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_path
*		[in] Pointer to the DR path object to traverse
*
* RETURN VALUES
*	Pointer to requester physical port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_port_by_mad_addr
* NAME
*	osm_get_port_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester port in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_mad_addr(IN struct osm_log *p_log,
					  IN const osm_subn_t * p_subn,
					  IN struct osm_mad_addr *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Subnet/osm_get_switch_by_guid
* NAME
*	osm_get_switch_by_guid
*
* DESCRIPTION
*	Looks for the given switch guid in the subnet table of switches by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_switch *osm_get_switch_by_guid(IN const osm_subn_t * p_subn,
					  IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The node guid in network byte order
*
* RETURN VALUES
*	The switch structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_switch_t
*********/

/****f* OpenSM: Subnet/osm_get_node_by_guid
* NAME
*	osm_get_node_by_guid
*
* DESCRIPTION
*	This looks for the given node guid in the subnet table of nodes by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_node *osm_get_node_by_guid(IN osm_subn_t const *p_subn,
				      IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The node guid in network byte order
*
* RETURN VALUES
*	The node structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_node_t
*********/

/****f* OpenSM: Subnet/osm_get_gpu_by_guid
* NAME
*	osm_get_gpu_by_guid
*
* DESCRIPTION
*	Searches for the given GPU guid in the subnet table of GPUs by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_gpu *osm_get_gpu_by_guid(IN osm_subn_t const *p_subn,
				    IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The GPU guid in network byte order
*
* RETURN VALUES
*	The GPU structure pointer if found. NULL otherwise.
*
* SEE ALSO
*********/

/****f* OpenSM: Subnet/osm_get_port_by_guid
* NAME
*	osm_get_port_by_guid
*
* DESCRIPTION
*	This looks for the given port guid in the subnet table of ports by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_guid(IN osm_subn_t const *p_subn,
				      IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The port guid in network order
*
* RETURN VALUES
*	The port structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_port_t
*********/

/****f* OpenSM: Port/osm_get_port_by_lid_ho
* NAME
*	osm_get_port_by_lid_ho
*
* DESCRIPTION
*	Returns a pointer of the port object for given lid value.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_lid_ho(const osm_subn_t * subn, uint16_t lid);
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet data structure.
*
*	lid
*		[in] LID requested in host byte order.
*
* RETURN VALUES
*	The port structure pointer if found. NULL otherwise.
*
* SEE ALSO
*       Subnet object, osm_port_t
*********/

/****f* OpenSM: Port/osm_get_gpu_by_alid
* NAME
*	osm_get_gpu_by_alid
*
* DESCRIPTION
*	Returns a pointer of the GPU object for given alid value.
*
* SYNOPSIS
*/
struct osm_gpu *osm_get_gpu_by_alid(const osm_subn_t * subn, uint16_t alid);
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet data structure.
*
*	alid
*		[in] ALID requested in host byte order.
*
* RETURN VALUES
*	The GPU structure pointer if found. NULL otherwise.
*
* SEE ALSO
*       Subnet object, osm_gpu_t
*********/


/****f* OpenSM: Subnet/osm_get_type_by_lid
* NAME
*	osm_get_type_by_lid
*
* DESCRIPTION
*	Get node type of the node holding the port which input LID belongs to.
*
* SYNOPSIS
*/
uint8_t osm_get_type_by_lid_ho(osm_subn_t const * p_subn, uint16_t lid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object.
*	lid
*		[in] LID to check node type for.
*
* RETURN VALUE
*	Node type of the node holding the port which input LID belongs to. Otherwise, 0.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Subnet/osm_get_alias_guid_by_guid
* NAME
*	osm_get_alias_guid_by_guid
*
* DESCRIPTION
*	This looks for the given port guid in the subnet table of ports by
*	alias guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_alias_guid *osm_get_alias_guid_by_guid(IN osm_subn_t const *p_subn,
						  IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The alias port guid in network order
*
* RETURN VALUES
*	The alias guid structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_alias_guid_t
*********/

/****f* OpenSM: Helper/osm_get_alias_guid_by_mad_addr
* NAME
*	osm_get_alias_guid_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester alias guid object in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_alias_guid *
osm_get_alias_guid_by_mad_addr(IN struct osm_log * p_log,
			       IN const osm_subn_t * p_subn,
			       IN struct osm_mad_addr *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester alias guid object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

boolean_t osm_validate_mad_addr(IN struct osm_log * p_log,
				IN osm_subn_t * p_subn,
				IN const struct osm_madw * p_madw);

/****f* OpenSM: Subnet/osm_get_guid_by_lid
* NAME
*	osm_get_guid_by_lid
*
* DESCRIPTION
*  Find guid of port or vport which owns a given lid
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
ib_net64_t osm_get_guid_by_lid(IN osm_subn_t const *p_subn, IN ib_net16_t lid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	lid
*		[in] The lid to find in network order
*
* RETURN VALUES
*	The guid if found. Zero otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_alias_guid_t
*********/

/****f* OpenSM: Subnet/osm_get_port_by_alias_guid
* NAME
*	osm_get_port_by_alias_guid
*
* DESCRIPTION
*	This looks for the given port guid in the subnet table of ports by
*	alias guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_alias_guid(IN osm_subn_t const *p_subn,
					    IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The alias port guid in network order
*
* RETURN VALUES
*	The port structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_port_t
*********/

/****f* OpenSM: Subnet/osm_is_alias_guid
* NAME
*	osm_is_alias_guid
*
* DESCRIPTION
*	All guids (alias or real) are represented as osm_alias_guid in the
*	alias guid table.
*	This function gets a osm_alias_guid from the alias guid table and return
*	indication if this is an alias guid (which means it is NOT real guid)
*
* SYNOPSIS
*/
boolean_t osm_is_alias_guid(IN const struct osm_alias_guid *p_alias_guid);
/*
* PARAMETERS
*	p_alias_guid
*		[in] Pointer to an alias guid object
*
* RETURN VALUES
*	TRUE - This alias guid represents an alias guid
*	FALSE - This alias guid represents a real guid
*********/

/****f* OpenSM: Port/osm_assigned_guids_new
* NAME
*	osm_assigned_guids_new
*
* DESCRIPTION
*	This function allocates and initializes an assigned guids object.
*
* SYNOPSIS
*/
osm_assigned_guids_t *osm_assigned_guids_new(IN const ib_net64_t port_guid,
					     IN const uint32_t num_guids);
/*
* PARAMETERS
*       port_guid
*               [in] Base port GUID in network order
*
* RETURN VALUE
*       Pointer to the initialized assigned alias guid object.
*
* SEE ALSO
*	Subnet object, osm_assigned_guids_t, osm_assigned_guids_delete,
*	osm_get_assigned_guids_by_guid
*********/

/****f* OpenSM: Port/osm_assigned_guids_delete
* NAME
*	osm_assigned_guids_delete
*
* DESCRIPTION
*	This function destroys and deallocates an assigned guids object.
*
* SYNOPSIS
*/
void osm_assigned_guids_delete(IN OUT osm_assigned_guids_t ** pp_assigned_guids);
/*
* PARAMETERS
*       pp_assigned_guids
*		[in][out] Pointer to a pointer to an assigned guids object to delete.
*		On return, this pointer is NULL.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified assigned guids object.
*
* SEE ALSO
*	Subnet object, osm_assigned_guids_new, osm_get_assigned_guids_by_guid
*********/

/****f* OpenSM: Subnet/osm_get_assigned_guids_by_guid
* NAME
*	osm_get_assigned_guids_by_guid
*
* DESCRIPTION
*	This looks for the given port guid and returns a pointer
*	to the guid table of SA assigned alias guids for that port.
*
* SYNOPSIS
*/
osm_assigned_guids_t *osm_get_assigned_guids_by_guid(IN osm_subn_t const *p_subn,
						     IN ib_net64_t port_guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	port_guid
*		[in] The base port guid in network order
*
* RETURN VALUES
*	The osm_assigned_guids structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_assigned_guids_new, osm_assigned_guids_delete,
*	osm_assigned_guids_t
*********/

/****f* OpenSM: Port/osm_get_port_by_lid
* NAME
*	osm_get_port_by_lid
*
* DESCRIPTION
*	Returns a pointer of the port object for given lid value.
*
* SYNOPSIS
*/
static inline struct osm_port *osm_get_port_by_lid(IN osm_subn_t const * subn,
						   IN ib_net16_t lid)
{
	return osm_get_port_by_lid_ho(subn, cl_ntoh16(lid));
}
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet data structure.
*
*	lid
*		[in] LID requested in network byte order.
*
* RETURN VALUES
*	The port structure pointer if found. NULL otherwise.
*
* SEE ALSO
*       Subnet object, osm_port_t
*********/

/****f* OpenSM: Subnet/osm_get_mgrp_by_mgid
* NAME
*	osm_get_mgrp_by_mgid
*
* DESCRIPTION
*	This looks for the given multicast group in the subnet table by mgid.
*	NOTE: this code is not thread safe. Need to grab the lock before
*	calling it.
*
* SYNOPSIS
*/
struct osm_mgrp *osm_get_mgrp_by_mgid(IN osm_subn_t * subn, IN ib_gid_t * mgid);
/*
* PARAMETERS
*	subn
*		[in] Pointer to an osm_subn_t object
*
*	mgid
*		[in] The multicast group MGID value
*
* RETURN VALUES
*	The multicast group structure pointer if found. NULL otherwise.
*********/

/****f* OpenSM: Subnet/osm_get_mbox_by_mlid
* NAME
*	osm_get_mbox_by_mlid
*
* DESCRIPTION
*	This looks for the given multicast group in the subnet table by mlid.
*	NOTE: this code is not thread safe. Need to grab the lock before
*	calling it.
*
* SYNOPSIS
*/
static inline struct osm_mgrp_box *osm_get_mbox_by_mlid(osm_subn_t const *p_subn, ib_net16_t mlid)
{
	return (struct osm_mgrp_box *)p_subn->mboxes[cl_ntoh16(mlid) - IB_LID_MCAST_START_HO];
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	mlid
*		[in] The multicast group mlid in network order
*
* RETURN VALUES
*	The multicast group structure pointer if found. NULL otherwise.
*********/

/****f* OpenSM: Subnet/is_mepi_supported
* NAME
*	is_mepi_supported
*
* DESCRIPTION
*	Checks if given node supports mlnx estended port info
*
* SYNOPSIS
*/
boolean_t is_mepi_supported(struct osm_sm *p_sm, struct osm_node *p_node);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to sm object
*
*	p_node
*		[in] Pointer to node object
*
* RETURN VALUES
*	Boolean indication whether given node support mlnx ext port info
*********/

/****f* OpenSM: Subnet/osm_subn_set_default_opt
* NAME
*	osm_subn_set_default_opt
*
* DESCRIPTION
*	The osm_subn_set_default_opt function sets the default options.
*
* SYNOPSIS
*/
void osm_subn_set_default_opt(IN osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy
*********/

/****f* OpenSM: Subnet/osm_subn_parse_conf_file
* NAME
*	osm_subn_parse_conf_file
*
* DESCRIPTION
*	The osm_subn_parse_conf_file function parses the configuration file
*	and sets the defaults accordingly.
*
* SYNOPSIS
*/
int osm_subn_parse_conf_file(char *conf_file, osm_subn_opt_t * p_opt,
			     boolean_t log);
/*
* PARAMETERS
*
*	conf_file
*		name of configuration file
*	p_opt
*		[in] Pointer to the subnet options structure.
*	log
*		indication whether log print required
*
* RETURN VALUES
*	0 on success, positive value if file doesn't exist,
*	negative value otherwise
*********/

/****f* OpenSM: Subnet/osm_subn_rescan_conf_files
* NAME
*	osm_subn_rescan_conf_files
*
* DESCRIPTION
*	The osm_subn_rescan_conf_files function parses the configuration
*	files and update selected subnet options
*
* SYNOPSIS
*/
int osm_subn_rescan_conf_files(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*
*	p_subn
*		[in] Pointer to the subnet structure.
*
* RETURN VALUES
*	0 on success, positive value if file doesn't exist,
*	negative value otherwise
*
*********/

/****f* OpenSM: Subnet/osm_subn_output_conf
* NAME
*	osm_subn_output_conf
*
* DESCRIPTION
*	Output configuration info
*
* SYNOPSIS
*/
void osm_subn_output_conf(FILE *out, IN osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*
*	out
*		[in] File stream to output to.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	This method does not return a value
*********/

/****f* OpenSM: Subnet/osm_subn_write_conf_file
* NAME
*	osm_subn_write_conf_file
*
* DESCRIPTION
*	Write the configuration file into the cache
*
* SYNOPSIS
*/
int osm_subn_write_conf_file(char *file_name, IN osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	0 on success, negative value otherwise
*
* NOTES
*	Assumes the conf file is part of the cache dir which defaults to
*	OSM_DEFAULT_CACHE_DIR or OSM_CACHE_DIR the name is opensm.opts
*********/
int osm_subn_verify_config(osm_subn_opt_t * p_opt);

/****f* OpenSM: Subnet/osm_is_held_back_sw
* NAME
*	osm_is_held_back_sw
*
* DESCRIPTION
*	Returns true if this switch persists in held_back list
*
* SYNOPSIS
*/
boolean_t osm_is_held_back_sw(IN osm_subn_t const *p_subn, IN ib_net64_t guid);
/*
* PARAMETERS
*
*	p_subn
*		[in] Pointer to the subnet structure.
*
*	guid
*		[in] Guid
*
* RETURN VALUES
*	TRUE if the switch is found and FALSE otherwise.
*********/

/****f* OpenSM: Subnet/subn_opt_destroy
* NAME
*	subn_opt_destroy
*
* DESCRIPTION
*	Destroy opt object
*
* SYNOPSIS
*/
void subn_opt_destroy(osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*
*	p_opt
*		Pointer to the opt object to destroy.
*********/

/****f* OpenSM: OpenSM/lookup_held_back_switch
* NAME
*	lookup_held_back_switch
*
* DESCRIPTION
*	Finds whether this node appears in held_back list
*
* SYNOPSIS
*/

boolean_t lookup_held_back_switch(IN osm_subn_t *subn,
				  IN struct osm_node * p_node);
/*
* PARAMETERS
*	subn
*		[in] Pointer to a subnet object.
*
* 	p_node
* 		[in] Pointer to a node object.
*
* RETURN VALUE
*	True if this node appears in held back switch list and false otherwise.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_add_leaf
* NAME
*	osm_subn_add_leaf
*
* DESCRIPTION
*	Adds switch to subnets list of leaf switches.
*
* SYNOPSIS
*/
int osm_subn_add_leaf(IN osm_subn_t * p_subn, IN struct osm_switch * p_sw);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
* 	p_sw
* 		[in] Pointer to a switch object.
*
* RETURN VALUE
*	0 on success, negative value otherwise
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_clear_leafs
* NAME
*	osm_subn_clear_leafs
*
* DESCRIPTION
*	Clears subnet's list of leaf switches.
*
* SYNOPSIS
*/
void osm_subn_clear_leafs(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_clear_hub_switch_by_plane
* NAME
*	osm_subn_clear_hub_switch_by_plane
*
* DESCRIPTION
*	removes subnet hub switch from the given plane.
*
* SYNOPSIS
*/
static inline void osm_subn_clear_hub_switch_by_plane(IN osm_subn_t * p_subn, int plane)
{
	if (plane_is_valid(plane))
		p_subn->hub_switch_guid[plane] = 0;
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*	plane
*		[in] Plane number
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_clear_hub_switches
* NAME
*	osm_subn_clear_hub_switches
*
* DESCRIPTION
*	removes subnet hub switches from all planes.
*
* SYNOPSIS
*/
static inline void osm_subn_clear_hub_switches(IN osm_subn_t * p_subn)
{
	uint8_t plane;

	for (plane = 0; plane <= IB_MAX_NUM_OF_PLANES_XDR; plane++) {
		osm_subn_clear_hub_switch_by_plane(p_subn, plane);
	}
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_set_hub_switch_guid_by_plane
* NAME
*	osm_subn_set_hub_switch_guid_by_plane
*
* DESCRIPTION
*	Sets the guid of subnet hub switch for the given plane
*
* SYNOPSIS
*/
static void osm_subn_set_hub_switch_guid_by_plane(IN osm_subn_t * p_subn, IN ib_net64_t guid, IN int plane)
{
	if (plane_is_valid(plane))
		p_subn->hub_switch_guid[plane] = guid;
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*	plane
*		[in] Plane number
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_set_hub_switch_guid
* NAME
*	osm_subn_set_hub_switch_guid
*
* DESCRIPTION
*	Sets the guid of subnet hub switch for plane 0 (ie, legacy switches)
*
* SYNOPSIS
*/
static inline void osm_subn_set_hub_switch_guid(IN osm_subn_t * p_subn, IN ib_net64_t guid)
{
	osm_subn_set_hub_switch_guid_by_plane(p_subn, guid, 0);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
* RETURN VALUES
*	None.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_get_hub_switch_guid_by_plane
* NAME
*	osm_subn_get_hub_switch_guid_by_plane
*
* DESCRIPTION
*	returns guid of subnet hub switch for the given plane.
*
* SYNOPSIS
*/
static inline ib_net64_t osm_subn_get_hub_switch_guid_by_plane(IN osm_subn_t * p_subn, IN int plane)
{
	if (plane_is_valid(plane)) {
		return p_subn->hub_switch_guid[plane];
	} else {
		return 0;
	}
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*	plane
*		[in] Plane number
*
* RETURN VALUES
*      Returns guid of the hub switch of the given subnet object in the given plane
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_get_hub_switch_guid
* NAME
*	osm_subn_get_hub_switch_guid
*
* DESCRIPTION
* 	returns guid of subnet hub switch for plane 0.
*
* SYNOPSIS
*/
static inline ib_net64_t osm_subn_get_hub_switch_guid(IN osm_subn_t * p_subn)
{
	return osm_subn_get_hub_switch_guid_by_plane(p_subn, 0);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
* RETURN VALUES
*	Returns guid of the hub switch of the given subnet object
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_get_hub_switch
* NAME
*	osm_subn_get_hub_switch_by_plane
*
* DESCRIPTION
* 	returns subnet hub switch for the given plane.
*
* SYNOPSIS
*/
static inline struct osm_switch * osm_subn_get_hub_switch_by_plane(IN osm_subn_t * p_subn, int plane)
{
	cl_map_item_t * p_item;
	if (!plane_is_valid(plane))
		return NULL;
	if (!p_subn->hub_switch_guid[plane])
		return NULL;
	p_item = cl_qmap_get(&p_subn->sw_guid_tbl, p_subn->hub_switch_guid[plane]);
	OSM_ASSERT(p_item != cl_qmap_end(&p_subn->sw_guid_tbl));
	return (struct osm_switch *)p_item;
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*	plane
*		[in] Plane number
* RETURN VALUES
*	Returns pointer to osm_switch_t object of the hub switch.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_get_hub_switch
* NAME
*	osm_subn_get_hub_switch
*
* DESCRIPTION
* 	returns subnet hub switch for plane 0.
*
* SYNOPSIS
*/
static inline struct osm_switch * osm_subn_get_hub_switch(IN osm_subn_t * p_subn)
{
	return osm_subn_get_hub_switch_by_plane(p_subn, 0);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
* RETURN VALUES
*	Returns pointer to osm_switch_t object of the hub switch.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_get_max_mlid
* NAME
*	osm_subn_get_max_mlid
*
* DESCRIPTION
*	Returns the highest MLID value in use.
*
* SYNOPSIS
*/
uint16_t osm_subn_get_max_mlid(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
* RETURN VALUES
*	Returns the highest MLID value in use.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_set_mbox
* NAME
*	osm_subn_set_mbox
*
* DESCRIPTION
*	Sets value in array of multicast box objects and updates the value of
*	max_mlid.
*
* SYNOPSIS
*/
void osm_subn_set_mbox(IN osm_subn_t * p_subn, struct osm_mgrp_box  * mbox);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
*	mbox
*		[in] Pointer to a mbox object.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_subn_remove_mbox
* NAME
*	osm_subn_remove_mbox
*
* DESCRIPTION
*	Remove multicast box object from the array, and updates the value of
*	max_mlid.
*
* SYNOPSIS
*/
void osm_subn_remove_mbox(IN osm_subn_t * p_subn, IN uint16_t mlid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
*	mlid
*		[in] mlid if the mbox object to remove.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: Port/osm_subn_bs_count
* NAME
*	osm_subn_bs_count
*
* DESCRIPTION
*	Returns number of border switches in the subnet.
*
* SYNOPSIS
*/
static inline size_t osm_subn_bs_count(IN osm_subn_t const *p_subn)
{
	return cl_qmap_count(&p_subn->border_sw_tbl);
}
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUES
*	Count of border switches in the subnet.
*********/

/****f* OpenSM: Subnet/osm_subn_mark_mbox_by_sw_port
* NAME
*	osm_subn_mark_mbox_by_sw_port
*
* DESCRIPTION
* 	Mark osm_mgrp_box_t objects with multicast tree on a given switch port
* 	for recalculation.
*
* SYNOPSIS
*/
void osm_subn_mark_mbox_by_sw(IN osm_subn_t * p_subn,
			      IN struct osm_switch * p_sw,
			      IN uint8_t port_num);
/*
* PARAMETERS
*	subn
*		[in] Pointer to the subnet data structure.
*
*	p_sw
*		[in] Pointer to osm_switch_t object.
*
* 	port_num
*		[in] Port number on the switch. If port_num is 0, check all
*		ports of the switch.
*
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_clear_mtrees
* NAME
*	osm_subn_clear_mtrees
*
* DESCRIPTION
*	Clear multicast tree information from all osm_mgrp_box_t objects of
*	the subnet.
*
* SYNOPSIS
*/
void osm_subn_clear_mtrees(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_clear_mc_roots_tbl
* NAME
*	osm_subn_clear_mc_roots_tbl
*
* DESCRIPTION
*
* SYNOPSIS
*/
void osm_subn_clear_mc_roots_tbl(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_parse_mc_roots_file
* NAME
*	osm_subn_parse_mc_roots_file
*
* DESCRIPTION
* 	Parse multicast roots file
*
* SYNOPSIS
*/
int osm_subn_parse_mc_roots_file(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
* 	Returns 0 if OpenSM parsed the file.
* 	1 otherwise.
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_apply_vlids_to_lfts
* NAME
*	osm_subn_apply_vlids_to_lfts
*
* DESCRIPTION
*	Add virtual ports lids into lft tables.
*	Virtual lids are added with exit port copied from the physical
*	port lid exit port.
*
* SYNOPSIS
*/
boolean_t osm_subn_apply_vlids_to_lfts(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	boolean indication whether vlids applied or not
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_print_conf_changes
* NAME
*	osm_subn_print_conf_changes
*
* DESCRIPTION
*	Print summary of configuration in non-default value
*
* SYNOPSIS
*/
void osm_subn_print_conf_changes(osm_subn_t *p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	None
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_clear_service_name_key_tbl
* NAME
*	osm_subn_clear_service_name_key_tbl
*
* DESCRIPTION
* 	Cleans the service names' keys table.
*
* SYNOPSIS
*/
void osm_subn_clear_service_name_key_tbl(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_update_rtr_selection_algo_parameters
* NAME
*	osm_update_rtr_selection_algo_parameters
*
* DESCRIPTION
*	Updates router selection algorithm parameters
*
* SYNOPSIS
*/
void osm_update_rtr_selection_algo_parameters(osm_subn_t * p_subn,
					      char * selection_param);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*	selection_param
*		[in] Router selection algorithm parameters
*
* RETURN VALUE
*	None
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_parse_service_keys_file
* NAME
*	osm_subn_parse_service_keys_file
*
* DESCRIPTION
* 	Parse service key file
*
* SYNOPSIS
*/
int osm_subn_parse_service_keys_file(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	Returns 0 if OpenSM parsed the file.
*	1 otherwise.
*
* SEE ALSO
*
*********/

/* The default value for prism_an_lmc. Corresponds to 4 planes per prism switch. */
#define OSM_DEFAULT_PRISM_AN_LMC 2

/****f* OpenSM: Subnet/osm_subn_get_max_lmc
* NAME
*	osm_subn_get_max_lmc
*
* DESCRIPTION
*	Return max LMC from configuration parameters
*
* SYNOPSIS
*/

static inline uint8_t osm_subn_get_max_lmc(IN osm_subn_t * p_subn)
{
	uint8_t max_lmc = p_subn->opt.lmc;

	if (max_lmc < p_subn->opt.rtr_lmc)
		max_lmc = p_subn->opt.rtr_lmc;
	if (max_lmc < p_subn->opt.prism_an_lmc)
		max_lmc = p_subn->opt.prism_an_lmc;
	return max_lmc;
}

/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
* RETURN VALUE
*	Returns max LMC
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_update_rtr_selection_function
* NAME
*	osm_update_rtr_selection_function
*
* DESCRIPTION
*	Updates	router selection function
*
* SYNOPSIS
*/
void osm_update_rtr_selection_function(osm_subn_t * p_subn,
                                       char * selection_func);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*	selection_func
*		[in] Router selection function
*
* RETURN VALUE
*	None
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_update_rtr_selection_function
* NAME
*	osm_update_rtr_selection_function
*
* DESCRIPTION
*	Updates	router selection function
*
* SYNOPSIS
*/
void osm_update_rtr_selection_function(osm_subn_t * p_subn,
                                       char * selection_func);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*	selection_func
*		[in] Router selection function
*
* RETURN VALUE
*	None
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_get_max_op_vls_by_node
* NAME
*       osm_get_max_op_vls_by_node
*
* DESCRIPTION
*       return maximum operational vls configured according to node type
*
* SYNOPSIS
*/
uint8_t osm_get_max_op_vls_by_node(IN const osm_subn_t * p_subn,
				   IN const struct osm_node * p_node);
/*
* PARAMETERS
*      p_subn
*               [in] Pointer to the subnet.
*      p_node
*               [in] Pointer to the node object.
*
* RETURN VALUE
*       max operations VLs configured for this type of node
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_update_key_mgr_seed
* NAME
*       osm_update_key_mgr_seed
*
* DESCRIPTION
*       Update the key manager flag for reconfigure keys if seed was changed.
*
* SYNOPSIS
*/
void osm_update_key_mgr_seed(osm_subn_t * p_subn, ib_net64_t seed);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*	seed
*		[in] key manager seed for key configuration.
*
* RETURN VALUE
*       None
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_update_allowed_sm_guids
* NAME
*	osm_update_allowed_sm_guids
*
* DESCRIPTION
*	Updates	the allowed sm guid tbl
*
* SYNOPSIS
*/
void osm_update_allowed_sm_guids(IN osm_subn_t *p_subn, IN const char *str_sm_guids);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*
*	str_sm_guids
*		[in] String of allowed port GUIDs to run an SM on them,
*		     separated by commas.
*
* RETURN VALUE
*	None
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_is_sm_guid_allowed
* NAME
* 	osm_is_sm_guid_allowed
*
* DESCRIPTION
* 	Returns a boolean indicator for whether the guid param is inside the allowed_sm_guid_tbl.
*
* SYNOPSIS
*/
boolean_t osm_is_sm_guid_allowed(IN const osm_subn_t *p_subn, ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object data structure.
*
*	guid
*		[in] A GUID in network order to lookup inside the allowed_sm_guid_tbl.
* RETURN VALUE
*       Boolean indicator for whether the guid param is inside the allowed_sm_guid_tbl.
*
* SEE ALSO
*
*********/

/****f* OpenSM: OpenSM/osm_setup_hbf_weights
* NAME
*	osm_setup_hbf_weights
*
* DESCRIPTION
* 	Set Hash Based Forwarding (HBF) Weights
*
* SYNOPSIS
*/
void osm_setup_hbf_weights(osm_subn_t *p_subn, const char *hbf_weights_str);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object.
*
*	hbf_weights_str
*		[in] HBF weights configuration parameter
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Subnet/osm_subn_is_flids_enabled
* NAME
*	osm_subn_is_flids_enabled
*
* DESCRIPTION
*	Returns TRUE when the the FLIDs feature is enabled
*
* SYNOPSIS
*/
static inline boolean_t osm_subn_is_flids_enabled(IN osm_subn_t * p_subn)
{
	return p_subn->opt.global_flid_start ? TRUE : FALSE;
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	Return TRUE if the FLIDs feature is enabled, FALSE otherwise.
*********/

/****f* OpenSM: Subnet/osm_local_flid_enabled
* NAME
*	osm_local_flid_enabled
*
* DESCRIPTION
*	When configuring the local FLID ranges 0 - 0 the user indicates that the
*	local subnet will not receive FLID assignments.
*
* SYNOPSIS
*/
static inline boolean_t osm_local_flid_enabled(IN osm_subn_t * p_subn)
{
	return (p_subn->opt.local_flid_start && p_subn->opt.local_flid_end);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	Return TRUE when the local subnet is enabled for FLIDs
*********/

/****f* OpenSM: Subnet/osm_subn_clear_ports_mads_queue
* NAME
*	osm_subn_clear_ports_mads_queue
*
* DESCRIPTION
* 	Clear MADs queues of all ports of the subnet.
*
* SYNOPSIS
*/
void osm_subn_clear_ports_mads_queue(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	None
*********/

/****f* OpenSM: Subnet/osm_subn_is_nvlink_enabled
* NAME
*	osm_subn_is_nvlink_enabled
*
* DESCRIPTION
*	Returns TRUE when NVLink is enabled
*
* SYNOPSIS
*/
static inline boolean_t osm_subn_is_nvlink_enabled(IN osm_subn_t * p_subn)
{
	return p_subn->opt.nvlink_enable;
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	Return TRUE if NVLink is enabled, FALSE otherwise.
*********/

void osm_subn_send_profiles_config_blocks(struct osm_sm * p_sm,
					  struct osm_switch * p_sw,
					  uint8_t feature,
					  uint8_t method);

void osm_subn_send_get_profiles_config_block(struct osm_sm * p_sm,
					     struct osm_switch * p_sw,
					     uint8_t feature,
					     uint8_t block_num);

void osm_subn_send_profiles_config_block(struct osm_sm * p_sm,
					 struct osm_switch * p_sw,
					 uint8_t feature,
					 uint8_t block_num);

uint8_t osm_profiles_config_get_profile(ib_profiles_config_t *profiles_cfg, uint8_t port);

struct osm_switch * osm_subn_get_sm_switch(osm_subn_t *p_subn);

/****f* OpenSM: Subnet/osm_subn_validate_key_db
* NAME
*	osm_subn_validate_key_db
*
* DESCRIPTION
*	Validate the key database and initialize it if it wasn't initialized
*
* SYNOPSIS
*/
ib_api_status_t osm_subn_validate_key_db(osm_opensm_t * p_osm,
					 uint8_t mclass,
					 boolean_t neighbor_key);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
*	mclass
*		[in] Management class. Currently supported: CC, VS, N2N.
*
*	neighbor_key
*		Boolean indicates of which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUES
*	IB_SUCCESS if the database object was is valid.
*********/

#define OSM_REDISCOVERY_DISABLED 0
#define OSM_REDISCOVERY_DEFAULT_MAX_LINK_PENALTY 10
#define OSM_REDISCOVERY_DEFAULT_LINK_PENALTY_STEP 2
#define OSM_REDISCOVERY_DEFAULT_MAX_NODE_PENALTY 5
#define OSM_REDISCOVERY_DEFAULT_NODE_PENALTY_STEP 5

END_C_DECLS
