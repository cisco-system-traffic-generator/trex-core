/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009-2011 ZIH, TU Dresden, Federal Republic of Germany. All rights reserved.
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
 * 	Declaration of osm_opensm_t.
 *	This object represents the OpenSM super object.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <stdio.h>
#include <complib/cl_qlist.h>
#include <complib/cl_dispatcher.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_atomic.h>
#include <complib/cl_nodenamemap.h>
#include <opensm/osm_console_io.h>
#include <opensm/osm_stats.h>
#include <opensm/osm_log.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_sa.h>
#include <opensm/osm_perfmgr.h>
#include <opensm/osm_event_plugin.h>
#include <opensm/osm_db.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_vl15intf.h>
#include <opensm/osm_congestion_control.h>
#include <opensm/osm_vendor_specific.h>
#include <opensm/osm_n2n.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/OpenSM
* NAME
*	OpenSM
*
* DESCRIPTION
*	The OpenSM object encapsulates the information needed by the
*	OpenSM to govern itself.  The OpenSM is one OpenSM object.
*
*	The OpenSM object is thread safe.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

typedef struct osm_perflog_mgr {
	int reports_num;
	FILE *perflog_dump_file;
	unsigned long perflog_file_size;
	unsigned long perflog_max_size;
	char *current_str;
} osm_perflog_mgr_t;

typedef enum statistic_counters {
	HEAVY_SWEEP_COUNT = 0,
	LIGHT_SWEEP_COUNT,
	SM_MAD_TIMEOUTS_COUNT,
	SA_MAD_TIMEOUTS_COUNT,
	CC_MAD_TIMEOUTS_COUNT,
	VS_MAD_TIMEOUTS_COUNT,
	N2N_MAD_TIMEOUTS_COUNT,
	NODE_DESC_RCV_COUNT,
	NODE_DESC_SEND_COUNT,
	NODE_INFO_RCV_COUNT,
	NODE_INFO_SEND_COUNT,
	SWITCH_INFO_RCV_COUNT,
	SWITCH_INFO_SEND_COUNT,
	EXTENDED_SWITCH_INFO_RCV_COUNT,
	EXTENDED_SWITCH_INFO_SEND_COUNT,
	GUID_INFO_RCV_COUNT,
	GUID_INFO_SEND_COUNT,
	PORT_INFO_RCV_COUNT,
	PORT_INFO_SEND_COUNT,
	SPST_RCV_COUNT,
	SPST_SEND_COUNT,
	P_KEY_TBL_RCV_COUNT,
	P_KEY_TBL_SEND_COUNT,
	SL_VL_MAP_RCV_COUNT,
	SL_VL_MAP_SEND_COUNT,
	VL_ARBIT_TBL_RCV_COUNT,
	VL_ARBIT_TBL_SEND_COUNT,
	GENERAL_INFO_RCV_COUNT,
	GENERAL_INFO_SEND_COUNT,
	LFT_RCV_COUNT,
	LFT_SEND_COUNT,
	AR_INFO_RCV_COUNT,
	AR_INFO_SEND_COUNT,
	EXTENDED_NODE_INFO_RCV_COUNT,
	EXTENDED_NODE_INFO_SEND_COUNT,
	AR_GROUP_TABLE_RCV_COUNT,
	AR_GROUP_TABLE_SEND_COUNT,
	AR_GROUP_TABLE_COPY_RCV_COUNT,
	AR_GROUP_TABLE_COPY_SEND_COUNT,
	AR_LFT_RCV_COUNT,
	AR_LFT_SEND_COUNT,
	PLFT_INFO_RCV_COUNT,
	PLFT_INFO_SEND_COUNT,
	PLFT_DEF_RCV_COUNT,
	PLFT_DEF_SEND_COUNT,
	PORT_SL_TO_PLFT_MAP_RCV_COUNT,
	PORT_SL_TO_PLFT_MAP_SEND_COUNT,
	PLFT_MAP_RCV_COUNT,
	PLFT_MAP_SEND_COUNT,
	RN_SUB_GROUP_DIRECTION_TABLE_RCV_COUNT,
	RN_SUB_GROUP_DIRECTION_TABLE_SEND_COUNT,
	RN_GEN_STRING_TABLE_RCV_COUNT,
	RN_GEN_STRING_TABLE_SEND_COUNT,
	RN_RCV_STRING_TABLE_RCV_COUNT,
	RN_RCV_STRING_TABLE_SEND_COUNT,
	RN_XMIT_PORT_MASK_RCV_COUNT,
	RN_XMIT_PORT_MASK_SEND_COUNT,
	RN_GEN_BY_SUB_GROUP_PRIORITY_RCV_COUNT,
	RN_GEN_BY_SUB_GROUP_PRIORITY_SEND_COUNT,
	MFT_RCV_COUNT,
	MFT_SEND_COUNT,
	HIERARCHY_INFO_RCV_COUNT,
	HIERARCHY_INFO_SEND_COUNT,
	CHASSIS_INFO_RCV_COUNT,
	CHASSIS_INFO_SEND_COUNT,
	SM_INFO_RCV_COUNT,
	SM_INFO_SEND_COUNT,
	MEPI_RCV_COUNT,
	MEPI_SEND_COUNT,
	ROUTER_INFO_RCV_COUNT,
	ROUTER_INFO_SEND_COUNT,
	RTR_ADJ_SITE_LOCAL_TBL_RCV_COUNT,
	RTR_ADJ_SITE_LOCAL_TBL_SEND_COUNT,
	RTR_NEXT_HOP_TBL_RCV_COUNT,
	RTR_NEXT_HOP_TBL_SEND_COUNT,
	RTR_ADJ_ROUTER_LID_INFO_RCV_COUNT,
	RTR_ADJ_ROUTER_LID_INFO_SEND_COUNT,
	RTR_ROUTER_LID_TBL_RCV_COUNT,
	RTR_ROUTER_LID_TBL_SEND_COUNT,
	RTR_ROUTER_AR_GROUP_TO_LID_TBL_RCV_COUNT,
	RTR_ROUTER_AR_GROUP_TO_LID_TBL_SEND_COUNT,
	QOS_CONFIG_SL_RCV_COUNT,
	QOS_CONFIG_SL_SEND_COUNT,
	QOS_CONFIG_VL_RCV_COUNT,
	QOS_CONFIG_VL_SEND_COUNT,
	VIRTUALIZATION_INFO_RCV_COUNT,
	VIRTUALIZATION_INFO_SEND_COUNT,
	VPORT_PORT_STATE_TBL_RCV_COUNT,
	VPORT_PORT_STATE_TBL_SEND_COUNT,
	VPORT_NODE_INFO_RCV_COUNT,
	VPORT_NODE_INFO_SEND_COUNT,
	VPORT_PORT_INFO_RCV_COUNT,
	VPORT_PORT_INFO_SEND_COUNT,
	VPORT_PKEY_TABLE_RCV_COUNT,
	VPORT_PKEY_TABLE_SEND_COUNT,
	VPORT_NODE_DESCRIPTION_RCV_COUNT,
	VPORT_NODE_DESCRIPTION_SEND_COUNT,
	VPORT_GUID_INFO_RCV_COUNT,
	VPORT_GUID_INFO_SEND_COUNT,
	VS_CLASS_PORT_INFO_RCV_COUNT,
	VS_CLASS_PORT_INFO_SEND_COUNT,
	VS_KEY_INFO_RCV_COUNT,
	VS_KEY_INFO_SEND_COUNT,
	CC_KEY_INFO_RCV_COUNT,
	CC_KEY_INFO_SEND_COUNT,
	CC_CLASS_PORT_INFO_RCV_COUNT,
	CC_CLASS_PORT_INFO_SEND_COUNT,
	CC_ENH_INFO_RCV_COUNT,
	CC_ENH_INFO_SEND_COUNT,
	CC_HCA_GENERAL_SETTINGS_RCV_COUNT,
	CC_HCA_GENERAL_SETTINGS_SEND_COUNT,
	CC_PORT_SW_PROFILE_SETTINGS_RCV_COUNT,
	CC_PORT_SW_PROFILE_SETTINGS_SEND_COUNT,
	CC_SL_MAPPING_SETTINGS_RCV_COUNT,
	CC_SL_MAPPING_SETTINGS_SEND_COUNT,
	CC_SW_GENERAL_SETTINGS_RCV_COUNT,
	CC_SW_GENERAL_SETTINGS_SEND_COUNT,
	CC_HCA_RP_PARAMETERS_RCV_COUNT,
	CC_HCA_RP_PARAMETERS_SEND_COUNT,
	CC_HCA_NP_PARAMETERS_RCV_COUNT,
	CC_HCA_NP_PARAMETERS_SEND_COUNT,
	PPCC_HCA_ALGO_CONFIG_RCV_COUNT,
	PPCC_HCA_ALGO_CONFIG_SEND_COUNT,
	PPCC_HCA_ALGO_PARAMS_CONFIG_RCV_COUNT,
	PPCC_HCA_ALGO_PARAMS_CONFIG_SEND_COUNT,
	N2N_CLASS_PORT_INFO_RCV_COUNT,
	N2N_CLASS_PORT_INFO_SEND_COUNT,
	N2N_KEY_INFO_RCV_COUNT,
	N2N_KEY_INFO_SEND_COUNT,
	N2N_NEIGHBORS_INFO_RCV_COUNT,
	N2N_NEIGHBORS_INFO_SEND_COUNT,
	NOTICE_128_RCV_COUNT,
	NOTICE_128_SEND_COUNT,
	NOTICE_130_RCV_COUNT,
	NOTICE_130_SEND_COUNT,
	NOTICE_131_RCV_COUNT,
	NOTICE_131_SEND_COUNT,
	NOTICE_144_RCV_COUNT,
	NOTICE_144_SEND_COUNT,
	NOTICE_145_RCV_COUNT,
	NOTICE_145_SEND_COUNT,
	NOTICE_256_RCV_COUNT,
	NOTICE_256_SEND_COUNT,
	NOTICE_257_RCV_COUNT,
	NOTICE_257_SEND_COUNT,
	NOTICE_258_RCV_COUNT,
	NOTICE_258_SEND_COUNT,
	NOTICE_259_RCV_COUNT,
	NOTICE_259_SEND_COUNT,
	NOTICE_1144_RCV_COUNT,
	NOTICE_1144_SEND_COUNT,
	NOTICE_1146_RCV_COUNT,
	NOTICE_1146_SEND_COUNT,
	NOTICE_1257_RCV_COUNT,
	NOTICE_1257_SEND_COUNT,
	NOTICE_1258_RCV_COUNT,
	NOTICE_1258_SEND_COUNT,
	NODE_RECORD_RCV,
	NODE_RECORD_SEND,
	PORT_INFO_RECORD_RCV,
	PORT_INFO_RECORD_SEND,
	INFORM_INFO_RECORD_RCV,
	INFORM_INFO_RECORD_SEND,
	SERVICE_RECORD_RCV,
	SERVICE_RECORD_SEND,
	PATH_RECORD_RCV,
	PATH_RECORD_SEND,
	MC_MEMBER_RECORD_GET_RCV,
	MC_MEMBER_RECORD_SET_RCV,
	MC_MEMBER_RECORD_SEND,
	GUID_INFO_RECORD_RCV,
	GUID_INFO_RECORD_SEND,
	UNTRUSTED_MAD_DROP_COUNT,
	SA_ETM_EXCEEDING_MAX_SUBSCRIPTIONS_COUNT,
	SL_VL_MAP_TBL_RECORD_RCV,
	SL_VL_MAP_TBL_RECORD_SEND,
	SWITCH_INFO_RECORD_RCV,
	SWITCH_INFO_RECORD_SEND,
	LINEAR_FORWARD_TBL_RECORD_RCV,
	LINEAR_FORWARD_TBL_RECORD_SEND,
	MULTICAST_FORWARD_TBL_RECORD_RCV,
	MULTICAST_FORWARD_TBL_RECORD_SEND,
	VL_ARBI_TBL_RECORD_RCV,
	VL_ARBI_TBL_RECORD_SEND,
	SM_INFO_RECORD_RCV,
	SM_INFO_RECORD_SEND,
	P_KEY_TBL_RECORD_RCV,
	P_KEY_TBL_RECORD_SEND,
	LINK_RECORD_RCV,
	LINK_RECORD_SEND,
	MULTI_PATH_RECORD_RCV,
	MULTI_PATH_RECORD_SEND,
	HBF_CONFIG_RCV_COUNT,
	HBF_CONFIG_SEND_COUNT,
	WHBF_CONFIG_RCV_COUNT,
	WHBF_CONFIG_SEND_COUNT,
	NVL_HBF_CONFIG_RCV_COUNT,
	NVL_HBF_CONFIG_SEND_COUNT,
	PFRN_CONFIG_RCV_COUNT,
	PFRN_CONFIG_SEND_COUNT,
	PROFILES_CONFIG_RCV_COUNT,
	PROFILES_CONFIG_SEND_COUNT,
	CREDIT_WATCHDOG_CONFIG_RCV_COUNT,
	CREDIT_WATCHDOG_CONFIG_SEND_COUNT,
	BER_CONFIG_RCV_COUNT,
	BER_CONFIG_SEND_COUNT,
	ALID_INFO_RCV_COUNT,
	ALID_INFO_SEND_COUNT,
	RAIL_FILTER_CONFIG_RCV_COUNT,
	RAIL_FILTER_CONFIG_SEND_COUNT,
	PORT_RECOVERY_POLICY_CONFIG_RCV_COUNT,
	PORT_RECOVERY_POLICY_CONFIG_SEND_COUNT,
	NOTICE_1601_SEND_COUNT,
	LFT_SPLIT_RCV_COUNT,
	LFT_SPLIT_SEND_COUNT,
	END_PORT_PLANE_FILTER_CONFIG_RCV_COUNT,
	END_PORT_PLANE_FILTER_CONFIG_SEND_COUNT,
	NOTICE_1605_SEND_COUNT,
	NOTICE_1606_SEND_COUNT,
	NOTICE_1609_SEND_COUNT,
	NOTICE_1610_SEND_COUNT,
	CND_INFO_RCV_COUNT,
	CND_INFO_SEND_COUNT,
	CND_PORT_STATE_RCV_COUNT,
	CND_PORT_STATE_SEND_COUNT,
	CND_PORT_STATE_CHANGE_TRAP_RCV_COUNT, /* notice 1317 */
	NOTICE_1318_RCV_COUNT,
	NOTICE_1319_RCV_COUNT,
	NOTICE_1320_RCV_COUNT,
	LAST_STAT_COUNTER
} statistic_counters;


#define OSM_STATISTIC_COUNTERS_SIZE		LAST_STAT_COUNTER

/****d* OpenSM: OSM_TIMESTAMP_STR_LEN
* NAME
* 	OSM_TIMESTAMP_STR_LEN
*
* DESCRIPTION
* 	Length of strings containing timestamps.
* SYNOPSIS
*/
#define OSM_TIMESTAMP_STR_LEN 32
/********/

/****s* OpenSM: OpenSM/osm_opensm_t
* NAME
*	osm_opensm_t
*
* DESCRIPTION
*	OpenSM structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_opensm {
	const char *osm_version;
	osm_subn_t subn;
	osm_sm_t sm;
	osm_sa_t sa;
#ifdef ENABLE_OSM_PERF_MGR
	osm_perfmgr_t perfmgr;
#endif				/* ENABLE_OSM_PERF_MGR */
	osm_congestion_control_t cc;
	osm_db_t db;
	boolean_t mad_pool_constructed;
	osm_mad_pool_t mad_pool;
	osm_vendor_t *p_vendor[OSM_MAX_BINDING_PORTS];
	osm_vendor_t *p_server_vendor[OSM_MAX_BINDING_PORTS];
	osm_vendor_t *p_gmp_vendor[OSM_MAX_BINDING_PORTS];
	boolean_t vl15_constructed;
	osm_super_vl15_t super_vl15;
	osm_log_t log;
	cl_dispatcher_t disp;
	cl_dispatcher_t sminfo_get_disp;
	cl_dispatcher_t sa_set_disp;
	cl_dispatcher_t sa_security_disp;
	cl_dispatcher_t gmp_disp;
	cl_dispatcher_t gmp_trap_disp;
	boolean_t sa_set_disp_initialized;
	cl_plock_t lock;
	cl_plock_t routing_lock;
	osm_stats_t stats[OSM_MAX_BINDING_PORTS];
	osm_console_t console;
	nn_map_t *node_name_map;
	FILE *statistic_dump_file[OSM_MAX_BINDING_PORTS + 1];
	unsigned long statistic_file_size[OSM_MAX_BINDING_PORTS + 1];
        unsigned long statistic_max_size;
	uint64_t stats_counters[OSM_MAX_BINDING_PORTS][OSM_STATISTIC_COUNTERS_SIZE];
	osm_perflog_mgr_t perflog_mgr;
	osm_vendor_specific_t vs;
	char start_time_str[OSM_TIMESTAMP_STR_LEN];
	osm_n2n_t n2n;
	osm_vendor_mgr_t vendor_mgr;
} osm_opensm_t;
/*
* FIELDS
* 	osm_version
* 		OpenSM version (as generated in osm_version.h)
*
*	subn
*		Subnet object for this subnet.
*
*	sm
*		The Subnet Manager (SM) object for this subnet.
*
*	sa
*		The Subnet Administration (SA) object for this subnet.
*
*	db
*		Persistent storage of some data required between sessions.
*
*	mad_pool
*		Pool of Management Datagram (MAD) objects.
*
*	p_vendor
*		Pointers to the Vendor specific adapters for various
*		transport interfaces, such as UMADT, AL, etc.  The
*		particular interface is set at compile time. This adapter
*		will handle SMP MADs only
*
*	p_gmp_vendor
*		Pointers to the Vendor specific adapters for GMP MADs
*
*	super_vl15
*		Pointer to Super VL15 object - aggregation of all OpenSM VL15 interfaces.
*
*	log
*		Log facility used by all OpenSM components.
*
*	disp
*		Dispatcher for SMP requests except SMInfo Get
*
*	sminfo_get_disp
*		SMInfo Get dispatcher containing the OpenSM SMInfo Get request.
*
*	sa_set_disp
*		Dispatcher for SA Set and Delete requests.
*
*	sa_security_disp
*		Dispatcher for handling SA requests rejected due to security
*		validation.
*
*	gmp_disp
*		Dispatcher for GMP request except SA Set and Delete requests
*
*	gmp_trap_disp
*		Dispatcher for key manager traps - CC and VS key violation
*
*	event_disp
*		Dispatcher for events to be reported by the SM
*
*	sa_set_disp_initialized.
*		Indicator that sa_set_disp dispatcher was initialized.
*
*	lock
*		Shared lock guarding most OpenSM structures.
*
*	routing_lock
*		Shared lock guarding routing calculation structures.
*
*	stats
*		Open SM statistics block per each OpenSM port
*
*	statistic_file_size
*		The current size of the statistic files per each OpenSM port.
*
*	statistic_max_size
*		The max size of the statistic file.
*
*	stats_counters
*		The statistic counters per SM binding port.
*
*	vs
*		vendor specific management class
*
*	start_time_str
*		OpenSM startup timestamp in string format
*
* SEE ALSO
*********/

/****d* OpenSM: OpenSM/osm_dump_flag_enum
* NAME
*	osm_dump_flag_enum
*
* DESCRIPTION
*	Flags of dump file types.
*
* SYNOPSIS
*/
typedef enum _osm_dump_flag_enum {
	OSM_DUMP_FLAG_ROUTING =		0x1,
	OSM_DUMP_FLAG_SUBNET_LST =	0x2,
	OSM_DUMP_FLAG_VIRTUALIZATION =	0x4,
	OSM_DUMP_FLAG_ROUTERS =		0x8,
	OSM_DUMP_FLAG_SMDB =		0x10,
	OSM_DUMP_FLAG_ALL =		0xFFFF
} osm_dump_flag_enum;
/***********/

/****f* OpenSM: OpenSM/osm_opensm_construct
* NAME
*	osm_opensm_construct
*
* DESCRIPTION
*	This function constructs an OpenSM object.
*
* SYNOPSIS
*/
void osm_opensm_construct(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to a OpenSM object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_opensm_init, osm_opensm_destroy
*
*	Calling osm_opensm_construct is a prerequisite to calling any other
*	method except osm_opensm_init.
*
* SEE ALSO
*	SM object, osm_opensm_init, osm_opensm_destroy
*********/

/****f* OpenSM: OpenSM/osm_opensm_construct_finish
* NAME
*	osm_opensm_construct_finish
*
* DESCRIPTION
*	The osm_opensm_construct_finish function completes
*	the second phase of constructing an OpenSM object.
*
* SYNOPSIS
*/
void osm_opensm_construct_finish(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to a OpenSM object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Calling osm_opensm_construct/osm_construct_finish is a prerequisite
*	to calling any other method except osm_opensm_init/osm_opensm_init_finish.
*
* SEE ALSO
*	SM object, osm_opensm_init, osm_opensm_construct_finish,
*	osm_opensm_destroy, osm_opensm_destroy_finish
*********/


/****f* OpenSM: OpenSM/osm_opensm_destroy
* NAME
*	osm_opensm_destroy
*
* DESCRIPTION
*	The osm_opensm_destroy function destroys an SM, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_opensm_destroy(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to a OpenSM object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified OpenSM object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_opensm_construct or
*	osm_opensm_init.
*
* SEE ALSO
*	SM object, osm_opensm_construct, osm_opensm_init
*********/

/****f* OpenSM: OpenSM/osm_opensm_destroy_finish
* NAME
*	osm_opensm_destroy_finish
*
* DESCRIPTION
*	The osm_opensm_destroy_finish function handles the second phase
*	of destroying an SM, releasing all resources.
*
* SYNOPSIS
*/
void osm_opensm_destroy_finish(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to a OpenSM object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs second phase of any necessary cleanup of the specified OpenSM object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to
*	osm_opensm_construct_finish or osm_opensm_init_finish.
*
* SEE ALSO
*	SM object, osm_opensm_construct, osm_opensm_construct_finish,
*	osm_opensm_init, osm_opensm_init_finish
*********/

/****f* OpenSM: OpenSM/osm_opensm_init
* NAME
*	osm_opensm_init
*
* DESCRIPTION
*	The osm_opensm_init function initializes a OpenSM object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_opensm_init(IN osm_opensm_t * p_osm,
				IN const osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object to initialize.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	IB_SUCCESS if the OpenSM object was initialized successfully.
*
* NOTES
*	Allows calling other OpenSM methods.
*
* SEE ALSO
*	SM object, osm_opensm_construct, osm_opensm_destroy
*********/

/****f* OpenSM: OpenSM/osm_opensm_init_finish
* NAME
*	osm_opensm_init_finish
*
* DESCRIPTION
*	The osm_opensm_init_finish function performs the second phase
*	of initialization of an OpenSM object.
*
* SYNOPSIS
*/
ib_api_status_t osm_opensm_init_finish(IN osm_opensm_t * p_osm,
				       IN const osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object to initialize.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	IB_SUCCESS if the OpenSM object was initialized successfully.
*
* NOTES
*	Allows calling other OpenSM methods.
*
* SEE ALSO
*	SM object, osm_opensm_construct, osm_opensm_construct_finish,
*	osm_opensm_destroy, osm_opensm_destroy_finish
*********/

/****f* OpenSM: OpenSM/osm_opensm_set_exit_flag
* NAME
*	osm_opensm_set_exit_flag
*
* DESCRIPTION
*	Set SM exit flag to initiate shutdown flow
*
* SYNOPSIS
*/
void osm_opensm_set_exit_flag(IN osm_opensm_t * p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object to initialize.
*
* RETURN VALUES
*	IB_SUCCESS if the OpenSM object was initialized successfully.
*
* RETURN VALUE
*	This function does not return a value.
*
*
* SEE ALSO
*	SM object, osm_opensm_construct, osm_opensm_construct_finish,
*	osm_opensm_destroy, osm_opensm_destroy_finish
*********/

/****f* OpenSM: OpenSM/osm_opensm_sweep
* NAME
*	osm_opensm_sweep
*
* DESCRIPTION
*	Initiates a subnet sweep.
*
* SYNOPSIS
*/
static inline void osm_opensm_sweep(IN osm_opensm_t * p_osm)
{
	osm_sm_sweep(&p_osm->sm);
}

/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object on which to
*		initiate a sweep.
*
* RETURN VALUES
*	None
*
* NOTES
*	If the OpenSM object is not bound to a port, this function
*	does nothing.
*
* SEE ALSO
*********/

/****f* OpenSM: OpenSM/osm_opensm_set_log_flags
* NAME
*	osm_opensm_set_log_flags
*
* DESCRIPTION
*	Sets the log level.
*
* SYNOPSIS
*/
static inline void osm_opensm_set_log_flags(IN osm_opensm_t * p_osm,
					    IN osm_log_level_t log_flags)
{
	osm_log_set_level(&p_osm->log, log_flags);
}

/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
*	log_flags
*		[in] Log level flags to set.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: OpenSM/osm_opensm_bind
* NAME
*	osm_opensm_bind
*
* DESCRIPTION
*	Binds the opensm object to a port guid.
*
* SYNOPSIS
*/
ib_api_status_t osm_opensm_bind(IN osm_opensm_t * p_osm, IN ib_net64_t * guids);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object to bind.
*
*	guids
*		[in] Array of local port GUIDs with which to bind.
*
* RETURN VALUES
*	None
*
* NOTES
*	A given opensm object can only be bound to one port at a time.
*
* SEE ALSO
*********/

/****f* OpenSM: OpenSM/osm_opensm_wait_for_subnet_up
* NAME
*	osm_opensm_wait_for_subnet_up
*
* DESCRIPTION
*	Blocks the calling thread until the subnet is up.
*
* SYNOPSIS
*/
static inline cl_status_t
osm_opensm_wait_for_subnet_up(IN osm_opensm_t * p_osm, IN uint32_t wait_us,
			      IN boolean_t interruptible)
{
	return osm_sm_wait_for_subnet_up(&p_osm->sm, wait_us, interruptible);
}

/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
*	wait_us
*		[in] Number of microseconds to wait.
*
*	interruptible
*		[in] Indicates whether the wait operation can be interrupted
*		by external signals.
*
* RETURN VALUES
*	CL_SUCCESS if the wait operation succeeded in response to the event
*	being set.
*
*	CL_TIMEOUT if the specified time period elapses.
*
*	CL_NOT_DONE if the wait was interrupted by an external signal.
*
*	CL_ERROR if the wait operation failed.
*
* NOTES
*
* SEE ALSO
*********/

void osm_opensm_report_event(osm_opensm_t *osm, osm_epi_event_id_t event_id,
			     void *event_data);

/* dump helpers */

#define OSM_PERFLOG_EVENT_NAME_LEN	256
#define OSM_PERFLOG_KEY_INDENT		4
#define OSM_PERFLOG_MAIN_EVENT		"HEAVYSWEEP"

typedef enum osm_perflog_event_state {
	OSM_PERFLOG_EVENT_START,
	OSM_PERFLOG_EVENT_END
} osm_perflog_event_state_enum;

void osm_perflog(osm_opensm_t *p_osm, const char * name, const char *suffix, osm_perflog_event_state_enum event_state);

#define LOGPERF(p_osm, name, suffix, type) do { \
		if (p_osm->subn.opt.enable_performance_logging) \
			osm_perflog(p_osm, name, suffix, type); \
	} while (0)

#define LOGPERF_START(p_osm, name, suffix) \
	LOGPERF(p_osm, name, suffix, OSM_PERFLOG_EVENT_START)

#define LOGPERF_END(p_osm, name, suffix) \
	LOGPERF(p_osm, name, suffix, OSM_PERFLOG_EVENT_END)

/****d* OpenSM: OpenSM/osm_perflog_event_t
* NAME
*       osm_perflog_event_t
*
* DESCRIPTION
*	Represents the event data needed for measuring the performance of the event,
*	and logging to the performance report.
*
* SYNOPSIS
*/
typedef struct osm_perflog_event {
	cl_list_item_t list_item;
	char event_name[OSM_PERFLOG_EVENT_NAME_LEN];
	struct timeval start_time;
	int subtasks;
	int subtask_num;
	int indent;
} osm_perflog_event_t;
/*
* FIELDS
*	list_item
*		Linkage structure for cl_qlist.
*
*	event_name
*		Event name.
*
*	start_time
*		Event start time.
*
*	subtasks
*		Number of subtasks that the event has.
*
*	subtask_num
*		In case the event is a sub task of a bigger task,
*		subtask_num represents the index of the sub task.
*
*	indent
*		Indention that is added to the event when printed to the file.
* SEE ALSO
*********/

void osm_dump_virtualization(osm_opensm_t * osm);
void osm_dump_mcast_routes(osm_opensm_t * osm);
void osm_dump_subn_all(osm_opensm_t * osm, osm_subn_t *p_subn, char* prefix,
		       unsigned dump_flags);
void osm_dump_lfts(osm_opensm_t * osm, osm_subn_t *p_subn, char* prefix);
void osm_dump_qmap_to_file(osm_opensm_t * p_osm, const char *file_name,
			   cl_qmap_t * map,
			   void (*func) (cl_map_item_t *, FILE *, void *),
			   void *cxt);
void osm_dump_func_to_file(osm_opensm_t * p_osm, const char *file_path,
			   void (*func) (FILE *, void *),
			   void *cxt);
void osm_dump_statistic(osm_opensm_t * p_osm);
void osm_dump_sm_db(IN osm_opensm_t * p_osm);
void osm_dump_heldback_switches(IN osm_opensm_t * p_osm);
void osm_dump_roots(IN osm_opensm_t * p_osm, IN const char * file_name);
int find_module_name(const char *name, uint8_t *file_id);

/****v* OpenSM/osm_exit_flag
*/
extern volatile unsigned int osm_exit_flag;
/*
* DESCRIPTION
*  Set to one to cause all threads to leave
*********/

END_C_DECLS
