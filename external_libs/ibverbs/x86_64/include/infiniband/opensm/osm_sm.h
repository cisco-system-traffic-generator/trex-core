/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2013 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_sm_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_event.h>
#include <complib/cl_thread.h>
#include <complib/cl_dispatcher.h>
#include <complib/cl_event_wheel.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_stats.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_vl15intf.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_log.h>
#include <opensm/osm_sm_mad_ctrl.h>
#include <opensm/osm_lid_mgr.h>
#include <opensm/osm_ucast_mgr.h>
#include <opensm/osm_max_flow_algorithm.h>
#include <opensm/osm_port.h>
#include <opensm/osm_db.h>
#include <opensm/osm_remote_sm.h>
#include <opensm/osm_routing_chain.h>
#include <opensm/osm_port_group.h>
#include <opensm/osm_topology_mgr.h>
#include <opensm/osm_virt_mgr.h>
#include <opensm/osm_router_policy.h>
#include <opensm/osm_verbose_bypass.h>
#include <opensm/osm_capability_mgr.h>
#include <opensm/osm_activity_mgr.h>
#include <opensm/osm_enhanced_qos_mgr.h>
#include <opensm/osm_congestion_control.h>
#include <opensm/osm_key_mgr.h>
#include <opensm/osm_router_mgr.h>
#include <opensm/osm_tenant_mgr.h>
#include <opensm/osm_issu_mgr.h>
#include <opensm/osm_nvlink.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

struct osm_sm;

typedef enum osm_crc_id {
	OSM_CRC_ROOT_GUID_FILE = 0,
	OSM_CRC_CN_GUID_FILE,
	OSM_CRC_IO_GUID_FILE,
	OSM_CRC_IDS_GUID_FILE,
	OSM_CRC_GUID_ROUTING_ORDER_FILE,
	OSM_CRC_PORT_PROF_IGNORE_FILE,
	OSM_CRC_HOP_WEIGHTS_FILE,
	OSM_CRC_PORT_SEARCH_ORDERING_FILE,
	OSM_CRC_PGRP_POLICY_FILE,
	OSM_CRC_RCH_POLICY_FILE,
	OSM_CRC_TOPO_POLICY_FILE,
	OSM_CRC_PARTITION_FILE,
	OSM_CRC_CONFIG_FILE,
	OSM_CRC_PREFIX_ROUTES_FILE,
	OSM_CRC_PER_MODULE_LOG_FILE,
	OSM_CRC_HEALTH_POLICY_FILE,
	OSM_CRC_HELDBACK_FILE,
	OSM_CRC_QOS_POLICY_FILE,
	OSM_CRC_ENHANCED_QOS_POLICY_FILE,
	OSM_CRC_MC_ROOTS_FILE,
	OSM_CRC_ROUTE_POLICY_FILE,
	OSM_CRC_VERBOSE_BYPASS_POLICY_FILE,
	OSM_CRC_NODE_NAME_MAP_FILE,
	OSM_CRC_SERVICE_NAME2KEY_MAP_FILE,
	OSM_CRC_DEVICE_CONFIGURATION_FILE,
	OSM_CRC_TOPO_CONFIGURATION_FILE,
	OSM_CRC_FAST_RECOVERY_CONF_FILE,
	OSM_CRC_TENANTS_POLICY_CONF_FILE,
	OSM_CRC_FABRIC_MODE_POLICY_CONF_FILE,
	OSM_CRC_MAX
} osm_crc_id_enum;

typedef struct osm_crc_element {
	osm_crc_id_enum id;
	unsigned long opt_offset;
	uint32_t crc_value;
	boolean_t crc_changed;
	boolean_t invalidates_cache;
} osm_crc_element_t;
/*
* FIELDS
*	id
*		crc file id
*
*	opt_offset
*		offset of parameter which describes file name within the opt
*		structure
*
*	crc_value
*		Last crc calculation value
*
*	crc_changed
*		Indication whether last calculated crc was different from
*		previous crc
*
*	invalidates_cache
*		Indication whether change in this file shall cause ucast
*		cache invalidation
*/

/****h* OpenSM/SM
* NAME
*	SM
*
* DESCRIPTION
*	The SM object encapsulates the information needed by the
*	OpenSM to instantiate a subnet manager.  The OpenSM allocates
*	one SM object per subnet manager.
*
*	The SM object is thread safe.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/
/****s* OpenSM: SM/osm_sm_t
* NAME
*  osm_sm_t
*
* DESCRIPTION
*  Subnet Manager structure.
*
*  This object should be treated as opaque and should
*  be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_sm {
	osm_thread_state_t thread_state;
	unsigned signal_mask;
	cl_spinlock_t signal_lock;
	cl_spinlock_t state_lock;
	cl_event_t signal_event;
	cl_event_t subnet_up_event;
	cl_timer_t sweep_timer;
	cl_timer_t polling_timer;
	cl_timer_t dump_statistics_timer;
	cl_event_wheel_t trap_aging_tracker;
	cl_event_wheel_t reboot_tracker;
	cl_event_wheel_t sa_etm_drop_tracker;
	cl_thread_t sweeper;
	unsigned master_sm_found;
	uint32_t retry_number;
	ib_net64_t master_sm_guid;
	ib_net64_t polling_sm_guid;
	osm_subn_t *p_subn;
	osm_db_t *p_db;
	osm_vendor_t *p_vendor[OSM_MAX_BINDING_PORTS];
	osm_vendor_t *p_server_vendor[OSM_MAX_BINDING_PORTS];
	osm_log_t *p_log;
	osm_mad_pool_t *p_mad_pool;
	osm_super_vl15_t *p_super_vl15;
	cl_dispatcher_t *p_disp;
	cl_dispatcher_t *p_sminfo_get_disp;
	cl_plock_t *p_lock;
	atomic32_t sm_trans_id;
	uint16_t mlids_init_max;
	unsigned mlids_req_max;
	uint8_t *mlids_req;
	osm_sm_mad_ctrl_t mad_ctrl;
	osm_lid_mgr_t lid_mgr;
	osm_ucast_mgr_t ucast_mgr;
	osm_routing_chain_mgr_t routing_chain_mgr;
	osm_topo_mgr_t topology_mgr;
	osm_port_group_mgr_t port_group_mgr;
	osm_activity_mgr_t activity_mgr;
	osm_enhanced_qos_mgr_t enhanced_qos_mgr;
	osm_cc_mgr_t cc_mgr;
	cl_disp_reg_handle_t sweep_fail_disp_h;
	cl_disp_reg_handle_t ni_disp_h;
	cl_disp_reg_handle_t pi_disp_h;
	cl_disp_reg_handle_t gi_disp_h;
	cl_disp_reg_handle_t nd_disp_h;
	cl_disp_reg_handle_t si_disp_h;
	cl_disp_reg_handle_t lft_disp_h;
	cl_disp_reg_handle_t mft_disp_h;
	cl_disp_reg_handle_t hierarchy_disp_h;
	cl_disp_reg_handle_t chassis_info_disp_h;
	cl_disp_reg_handle_t sm_info_disp_h;
	cl_disp_reg_handle_t sm_info_sminfo_get_disp_h;
	cl_disp_reg_handle_t trap_disp_h;
	cl_disp_reg_handle_t slvl_disp_h;
	cl_disp_reg_handle_t vla_disp_h;
	cl_disp_reg_handle_t pkey_disp_h;
	cl_disp_reg_handle_t mlnx_epi_disp_h;
	cl_disp_reg_handle_t vpi_disp_h;
	cl_disp_reg_handle_t vi_disp_h;
	cl_disp_reg_handle_t vps_disp_h;
	cl_disp_reg_handle_t general_info_disp_h;
	cl_disp_reg_handle_t vni_disp_h;
	cl_disp_reg_handle_t vpkey_disp_h;
	cl_disp_reg_handle_t vnd_disp_h;
	cl_disp_reg_handle_t ri_disp_h;
	cl_disp_reg_handle_t next_hop_rtr_disp_h;
	cl_disp_reg_handle_t rtr_tbl_mads_disp_h;
	cl_disp_reg_handle_t qos_config_sl_h;
	cl_disp_reg_handle_t qos_config_vl_h;
	cl_disp_reg_handle_t sps_disp_h;
	cl_disp_reg_handle_t ar_mad_disp_h;
	cl_disp_reg_handle_t rn_mad_disp_h;
	cl_disp_reg_handle_t plft_mad_disp_h;
	cl_disp_reg_handle_t eni_disp_h;
	cl_disp_reg_handle_t profiles_disp_h;
	cl_disp_reg_handle_t fast_recovery_disp_h;
	cl_disp_reg_handle_t rail_filter_config_disp_h;
	cl_disp_reg_handle_t end_port_plane_filter_config_disp_h;
	cl_disp_reg_handle_t extended_switch_info_disp_h;
	cl_disp_reg_handle_t port_recovery_policy_config_disp_h;
	boolean_t hm_dirty;
	osm_virt_mgr_t virt_mgr;
	osm_tenant_mgr_t tenant_mgr;
	osm_crc_element_t *files_crc;
	uint16_t crc_changed;
	boolean_t sharp_nodes_disabled;
	osm_router_policy_t router_policy_mgr;
	osm_log_verbose_bypass_t verbose_bypass;
	osm_capability_mgr_t cap_mgr;
	osm_router_mgr_t router_mgr;
	osm_nvlink_mgr_t nvl_mgr;
	osm_issu_mgr_t issu_mgr;
	osm_key_mgr_t key_mgr;
} osm_sm_t;
/*
* FIELDS
*	p_subn
*		Pointer to the Subnet object for this subnet.
*
*	p_db
*		Pointer to the database (persistency) object
*
*	p_vendor
*		Pointers to the vendor specific interfaces object
*		per auxiliary OpenSM ports.
*
*	p_log
*		Pointer to the log object.
*
*	p_mad_pool
*		Pointer to the MAD pool.
*
*	p_super_vl15
*		Pointer to the Super VL15 interface.
*
*	mad_ctrl
*		MAD Controller.
*
*	p_disp
*		Pointer to the Dispatcher.
*
*	p_sminfo_get_disp
*		Pointer to SMInfo Get Dispatcher.
*
*	p_lock
*		Pointer to the serializing lock.
*
* 	files_crc
* 		An array of crc elements to track files crc changes
*
*	crc_changed
*		Count of crc elements that changed during last check
*
*	sharp_nodes_disabled
*		Indication whether sharp was disabled on any node during sweep
*
*	router_policy_mgr
*		Manager of router policy
*
*	cap_mgr
*		Manager of capabilities
*
*	router_mgr
*		Manager of routers
*
*	nvlink_mgr
*		Manager for NVLink configuration
*
*	key_mgr
*		Key manager instance.
*
* SEE ALSO
*	SM object
*********/

/****f* OpenSM: SM/osm_sm_construct
* NAME
*	osm_sm_construct
*
* DESCRIPTION
*	This function constructs an SM object.
*
* SYNOPSIS
*/
void osm_sm_construct(IN osm_sm_t * p_sm, IN osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to a SM object to construct.
*
*	p_opt
*		[in] Pointer to OpenSM Subnet options parameters.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_sm_init, osm_sm_destroy
*
*	Calling osm_sm_construct is a prerequisite to calling any other
*	method except osm_sm_init.
*
* SEE ALSO
*	SM object, osm_sm_init, osm_sm_destroy
*********/

/****f* OpenSM: SM/osm_sm_shutdown
* NAME
*	osm_sm_shutdown
*
* DESCRIPTION
*	The osm_sm_shutdown function shutdowns an SM, stopping the sweeper
*	and unregistering all messages from the dispatcher
*
* SYNOPSIS
*/
void osm_sm_shutdown(IN osm_sm_t * p_sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to a SM object to shutdown.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*	SM object, osm_sm_construct, osm_sm_init
*********/

/****f* OpenSM: SM/osm_sm_destroy
* NAME
*	osm_sm_destroy
*
* DESCRIPTION
*	The osm_sm_destroy function destroys an SM, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_sm_destroy(IN osm_sm_t * p_sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to a SM object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified SM object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_sm_construct or
*	osm_sm_init.
*
* SEE ALSO
*	SM object, osm_sm_construct, osm_sm_init
*********/

/****f* OpenSM: SM/osm_sm_init
* NAME
*	osm_sm_init
*
* DESCRIPTION
*	The osm_sm_init function initializes a SM object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_sm_init(IN osm_sm_t * p_sm, IN osm_subn_t * p_subn,
			    IN osm_db_t * p_db, IN osm_vendor_t * p_vendor[],
			    IN osm_vendor_t * p_server_vendor[],
			    IN osm_mad_pool_t * p_mad_pool,
			    IN osm_super_vl15_t * p_super_vl15, IN osm_log_t * p_log,
			    IN osm_stats_t * p_stats,
			    IN cl_dispatcher_t * p_disp,
			    IN cl_dispatcher_t * p_sminfo_get_disp, IN cl_plock_t * p_lock);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object to initialize.
*
*	p_subn
*		[in] Pointer to the Subnet object for this subnet.
*
*	p_vendor
*		[in] Pointers per auxiliary OpenSM ports to
*		the vendor specific interfaces object.
*
*	p_mad_pool
*		[in] Pointer to the MAD pool.
*
*	p_super_vl15
*		[in] Pointer to the Super VL15 interface.
*
*	p_log
*		[in] Pointer to the log object.
*
*	p_stats
*		[in] Pointer to the statistics object.
*
*	p_disp
*		[in] Pointer to the OpenSM central Dispatcher.
*
*	p_sminfo_get_disp
*		[in] Pointer to the OpenSM SMInfo Get Dispatcher.
*
*	p_lock
*		[in] Pointer to the OpenSM serializing lock.
*
* RETURN VALUES
*	IB_SUCCESS if the SM object was initialized successfully.
*
* NOTES
*	Allows calling other SM methods.
*
* SEE ALSO
*	SM object, osm_sm_construct, osm_sm_destroy
*********/

/****f* OpenSM: SM/osm_sm_init_finish
* NAME
*	osm_sm_init_finish
*
* DESCRIPTION
*	The osm_sm_init_finish function finishes the initialization of an SM object.
*	Should be called after osm_sm_init and after plugins are loaded.
*
* SYNOPSIS
*/
ib_api_status_t osm_sm_init_finish(IN osm_sm_t * p_sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object to initialize.
*
* RETURN VALUES
*	IB_SUCCESS if the SM object was initialized successfully.
*
* SEE ALSO
*	SM object, osm_sm_init
*********/

/****f* OpenSM: SM/osm_sm_signal
* NAME
*	osm_sm_signal
*
* DESCRIPTION
*	Signal event to SM
*
* SYNOPSIS
*/
void osm_sm_signal(IN osm_sm_t * p_sm, osm_signal_t signal);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
*	signal
*		[in] sm signal number.
*
* NOTES
*
* SEE ALSO
*	SM object
*********/

/****f* OpenSM: SM/osm_sm_sweep
* NAME
*	osm_sm_sweep
*
* DESCRIPTION
*	Initiates a subnet sweep.
*
* SYNOPSIS
*/
void osm_sm_sweep(IN osm_sm_t * p_sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
* RETURN VALUES
*	IB_SUCCESS if the sweep completed successfully.
*
* NOTES
*
* SEE ALSO
*	SM object
*********/

/****f* OpenSM: SM/osm_sm_bind
* NAME
*	osm_sm_bind
*
* DESCRIPTION
*	Binds the sm object to a port guid.
*
* SYNOPSIS
*/
ib_api_status_t osm_sm_bind(IN osm_sm_t * p_sm, IN ib_net64_t * port_guids);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object to bind.
*
*	port_guids
*		[in] Local port GUIDs with which to bind.
*
*
* RETURN VALUES
*	None
*
* NOTES
*	A given SM object can only be bound to one port at a time.
*
* SEE ALSO
*********/

/****f* OpenSM: SM/osm_send_req
* NAME
*	osm_send_req
*
* DESCRIPTION
*	Starts the process to transmit a directed route request for
*	the attribute. The request may be sent via less loaded OpenSM port.
*
* SYNOPSIS
*/
ib_api_status_t osm_send_req(IN osm_sm_t * sm, IN const osm_dr_path_t * p_path,
			     IN const osm_physp_t *p_physp,
			     IN const uint8_t * p_payload,
			     IN size_t payload_size,
			     IN ib_net16_t attr_id, IN ib_net32_t attr_mod,
			     IN uint8_t method_id,
			     IN boolean_t find_mkey, IN ib_net64_t m_key,
			     IN uint32_t timeout, IN cl_disp_msgid_t err_msg,
			     IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_path
*		[in] Pointer to the directed route path to the node
*		from which to retrieve the attribute.
*
*	p_physp
*		[in] Pointer to osm_physp_t object for taking direct route
*		of the least loaded OpenSM port.
*
*	p_payload
*		[in] Pointer to the SMP payload to send.
*
*	payload_size
*		[in] The size of the payload to be copied to the SMP data field.
*
*	attr_id
*		[in] Attribute ID to request.
*
*	attr_mod
*		[in] Attribute modifier for this request.
*
*	method_id
*		[in] Method ID of the request,
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	timeout
*		[in] Transaction timeout in msec.
*
*	err_msg
*		[in] Message id with which to post this MAD if an error occurs.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*	This function asynchronously requests the specified attribute.
*	The response from the node will be routed through the Dispatcher
*	to the appropriate receive controller object.
*	
*	Also, currently, GET MADs are always sent on the main port.
*	If we introduce sending on the least loaded port to GET MADs as well,
*	it will be in a separate commit.
*********/

/****f* OpenSM: SM/osm_req_get
* NAME
*	osm_req_get
*
* DESCRIPTION
*	Starts the process to transmit a directed route request for
*	the attribute.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_get(IN osm_sm_t * sm, IN const osm_dr_path_t * p_path,
                            IN const osm_physp_t * p_physp,
			    IN ib_net16_t attr_id, IN ib_net32_t attr_mod,
			    IN boolean_t find_mkey, IN ib_net64_t m_key,
			    IN uint32_t timeout, IN cl_disp_msgid_t err_msg,
			    IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_path
*		[in] Pointer to the directed route path to the node
*		from which to retrieve the attribute.
*
*	p_physp
*		[in] Pointer to osm_physp_t object for taking direct route
*		of the least loaded OpenSM port.
*
*	attr_id
*		[in] Attribute ID to request.
*
*	attr_mod
*		[in] Attribute modifier for this request.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	timeout
*		[in] Transaction timeout in msec.
*
*	err_msg
*		[in] Message id with which to post this MAD if an error occurs.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*	This function asynchronously requests the specified attribute.
*	The response from the node will be routed through the Dispatcher
*	to the appropriate receive controller object.
*********/

/***f* OpenSM: SM/osm_prepare_req_get
* NAME
*	osm_prepare_req_get
*
* DESCRIPTION
*	Preallocate a directed route Get() MAD w/o sending it.
*
* SYNOPSIS
*/
osm_madw_t *osm_prepare_req_get(IN osm_sm_t * sm, IN const osm_dr_path_t * p_path,
				IN ib_net16_t attr_id, IN ib_net32_t attr_mod,
				IN boolean_t find_mkey, IN ib_net64_t m_key,
				IN uint32_t timeout, IN cl_disp_msgid_t err_msg,
				IN ib_net64_t dest_guid,
				IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_path
*		[in] Pointer to the directed route path of the recipient.
*
*	attr_id
*		[in] Attribute ID to request.
*
*	attr_mod
*		[in] Attribute modifier for this request.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	timeout
*		[in] Transaction timeout in msec.
*
*	err_msg
*		[in] Message id with which to post this MAD if an error occurs.
*
*	dest_guid
*		[in] Destination node GUID for this MAD.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		     context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	Pointer to the MAD buffer in case of success and NULL in case of failure.
*
*********/

/****f* OpenSM: SM/osm_send_req_mad
* NAME
*       osm_send_req_mad
*
* DESCRIPTION
*	Starts the process to transmit a preallocated/predefined directed route
*	Set() request. The request is sent via less loaded OpenSM port.
*
* SYNOPSIS
*/
void osm_send_req_mad(IN osm_sm_t * sm, IN const osm_physp_t *p_physp,
		      IN osm_madw_t *p_madw);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*	p_physp
*		[in] Pointer to osm_physp_t object for taking direct route
*		of the least loaded OpenSM port.
*	p_madw
*		[in] Pointer to a preallocated MAD buffer
*
*********/

/****f* OpenSM: SM/osm_send_req_mad_batch
* NAME
*	osm_send_req_mad_batch
*
* DESCRIPTION
*	Starts the process to transmit multiple preallocated/predefined Set/Get MADs
*
* SYNOPSIS
*/
void osm_send_req_mad_batch(IN osm_sm_t * sm, IN cl_qlist_t *mad_list);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*	mad_list
*		[in] Pointer to a list of preallocated MAD buffers
*
*********/

/***f* OpenSM: SM/osm_prepare_req_set
* NAME
*	osm_prepare_req_set
*
* DESCRIPTION
*	Preallocate and fill a directed route Set() MAD w/o sending it.
*
* SYNOPSIS
*/
osm_madw_t *osm_prepare_req_set(IN osm_sm_t * sm, IN const osm_dr_path_t * p_path,
				IN const uint8_t * p_payload,
				IN size_t payload_size, IN ib_net16_t attr_id,
				IN ib_net32_t attr_mod, IN boolean_t find_mkey,
				IN ib_net64_t m_key, IN uint32_t timeout,
				IN cl_disp_msgid_t err_msg,
				IN ib_net64_t dest_guid,
				IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_path
*		[in] Pointer to the directed route path of the recipient.
*
*	p_payload
*		[in] Pointer to the SMP payload to send.
*
*	payload_size
*		[in] The size of the payload to be copied to the SMP data field.
*
*	attr_id
*		[in] Attribute ID to request.
*
*	attr_mod
*		[in] Attribute modifier for this request.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	timeout
*		[in] Transaction timeout in msec.
*
*	err_msg
*		[in] Message id with which to post this MAD if an error occurs.
*
*	dest_guid
*		[in] Destination node GUID for this MAD.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		     context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	Pointer the MAD buffer in case of success and NULL in case of failure.
*
*********/

/****f* OpenSM: SM/osm_req_set
* NAME
*	osm_req_set
*
* DESCRIPTION
*	Starts the process to transmit a directed route Set() request.
*	The request is sent via less loaded OpenSM port.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_set(IN osm_sm_t * sm, IN const osm_dr_path_t * p_path,
			    IN const osm_physp_t * p_physp,
			    IN const uint8_t * p_payload,
			    IN size_t payload_size, IN ib_net16_t attr_id,
			    IN ib_net32_t attr_mod, IN boolean_t find_mkey,
			    IN ib_net64_t m_key, IN uint32_t timeout,
			    IN cl_disp_msgid_t err_msg,
			    IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_path
*		[in] Pointer to the directed route path of the recipient.
*
*	p_physp
*		[in] Pointer to osm_physp_t object for taking direct route
*		of the least loaded OpenSM port.
*
*	p_payload
*		[in] Pointer to the SMP payload to send.
*
*	payload_size
*		[in] The size of the payload to be copied to the SMP data field.
*
*	attr_id
*		[in] Attribute ID to request.
*
*	attr_mod
*		[in] Attribute modifier for this request.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
*
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	timeout
*		[in] Transaction timeout in msec.
*
*	err_msg
*		[in] Message id with which to post this MAD if an error occurs.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*	This function asynchronously requests the specified attribute.
*	The response from the node will be routed through the Dispatcher
*	to the appropriate receive controller object.
*********/

/****f* OpenSM: SM/osm_req_set_pi
* NAME
*	osm_req_set_pi
*
* DESCRIPTION
*	Starts the process to transmit a directed route PortInfo Set() request.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_set_pi(IN osm_sm_t * sm, IN const osm_physp_t *p_physp,
			       IN uint8_t port_num, IN const uint8_t * p_payload,
			       IN boolean_t find_mkey, IN ib_net64_t m_key,
			       IN const osm_madw_context_t * p_context,
			       IN boolean_t support_ext_speeds);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_physp
*		[in] Pointer to osm_physp_t object of the recipient.
*
*	port_num
*		[in] Port number to query.
*
*	p_payload
*		[in] Pointer to the SMP payload to send.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
*
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
*	support_ext_speeds
*		[in] Support extended speeds mode.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_req_get_pi
* NAME
*	osm_req_get_pi
*
* DESCRIPTION
*	Starts the process to transmit a directed route PortInfo Get() request.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_get_pi(IN osm_sm_t * sm, IN const osm_physp_t *p_physp,
			       IN uint8_t port_num, IN boolean_t find_mkey,
			       IN ib_net64_t m_key,
			       IN const osm_madw_context_t * p_context,
			       IN boolean_t support_ext_speeds);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_physp
*		[in] Pointer to osm_physp_t object of the recipient.
*
*	port_num
*		[in] Port number to query.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
*
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
*	support_ext_speeds
*		[in] Support extended speeds mode.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_req_get_esi
* NAME
*	osm_req_get_esi
*
* DESCRIPTION
* 	Get the switch ExtendedSwitchInfo attribute.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_get_esi(IN osm_sm_t *p_sm,
				IN osm_switch_t *p_sw);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
* RETURN VALUE
*	ib_api_status_t of the request
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_req_set_esi
* NAME
*	osm_req_set_esi
*
* DESCRIPTION
* 	Set the switch ExtendedSwitchInfo attribute.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_set_esi(IN osm_sm_t *p_sm,
				IN osm_switch_t *p_sw,
				IN ib_extended_switch_info_t *p_esi);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_sw
*		[in] Pointer to an osm_switch_t object.
*
*	p_esi
*		[in] Pointer to the ExtendedSwitchInfo attribute.
*
* RETURN VALUE
*	ib_api_status_t of the request
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_prepare_req_get_pi
* NAME
*	osm_prepare_req_get_pi
*
* DESCRIPTION
* 	Preallocate a directed route PortInfo Get() MAD w/o sending it.
*
* SYNOPSIS
*/
osm_madw_t *osm_prepare_req_get_pi(IN osm_sm_t * sm, IN const osm_physp_t *p_physp,
				   IN uint8_t port_num, IN boolean_t find_mkey,
				   IN ib_net64_t m_key,
				   IN const osm_madw_context_t * p_context,
				   IN boolean_t support_ext_speeds);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_physp
*		[in] Pointer to osm_physp_t object of the recipient.
*
*	port_num
*		[in] Port number to query.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
*
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
*	support_ext_speeds
*		[in] Support extended speeds mode.
*
* RETURN VALUES
*	Pointer to the MAD buffer in case of success and NULL in case of failure.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_prepare_req_set_pi
* NAME
*	osm_prepare_req_set_pi
*
* DESCRIPTION
* 	Preallocate and fill a directed route PortInfo Set() MAD w/o sending it.
*
* SYNOPSIS
*/
osm_madw_t *osm_prepare_req_set_pi(IN osm_sm_t * sm, IN const osm_physp_t *p_physp,
				   IN uint8_t port_num, IN const uint8_t * p_payload,
				   IN boolean_t find_mkey, IN ib_net64_t m_key,
				   IN const osm_madw_context_t * p_context,
				   IN boolean_t support_ext_speeds);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_physp
*		[in] Pointer to osm_physp_t object of the recipient.
*
*	port_num
*		[in] Port number to query.
*
*	p_payload
*		[in] Pointer to the SMP payload to send.
*
*	find_mkey
*		[in] Flag to indicate whether the M_Key should be looked up for
*		     this MAD.
*
* 	m_key
* 		[in] M_Key value to be send with this MAD. Applied, only when
* 		     find_mkey is FALSE.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
*	support_ext_speeds
*		[in] Support extended speeds mode.
*
* RETURN VALUES
*	Pointer to the MAD buffer in case of success and NULL in case of failure.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_req_get_spst
* NAME
*	osm_req_get_spst
*
* DESCRIPTION
*	Starts the process to transmit a directed route SwitchPortStateTable Get() request.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_get_spst(IN osm_sm_t * sm, IN const osm_switch_t * p_sw,
				 IN uint8_t block,
				 IN uint32_t am_mask,
				 IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_sw
*		[in] Pointer to osm_switch_t object of the recipient.
*
*	block
*		[in] Block number to query
*
*	am_mask
*		[in] Additional attribute modifier enablement bits.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_req_get_spsts
* NAME
*	osm_req_get_spsts
*
* DESCRIPTION
*	Send directed route SwitchPortStateTable Get() requests to a switch
*	according to number of ports.
*
* SYNOPSIS
*/
ib_api_status_t osm_req_get_spsts(IN osm_sm_t * sm, IN const osm_switch_t * p_sw,
				  IN uint32_t am_mask,
				  IN const osm_madw_context_t * p_context);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_sw
*		[in] Pointer to osm_switch_t object of the recipient.
*
*	am_mask
*		[in] Additional attribute modifier enablement bits.
*
*	p_context
*		[in] Mad wrapper context structure to be copied into the wrapper
*		context, and thus visible to the recipient of the response.
*
* RETURN VALUES
*	IB_SUCCESS if the request was successful.
*
* NOTES
*
*********/

/****f* OpenSM: SM/osm_resp_send
* NAME
*	osm_resp_send
*
* DESCRIPTION
*	Starts the process to transmit a directed route response.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
ib_api_status_t osm_resp_send(IN osm_sm_t * sm,
			      IN const osm_madw_t * p_req_madw,
			      IN ib_net16_t status,
			      IN const uint8_t * p_payload,
			      IN boolean_t force_flag);
/*
* PARAMETERS
*	p_resp
*		[in] Pointer to an osm_resp_t object.
*
*	p_madw
*		[in] Pointer to the MAD Wrapper object for the requesting MAD
*		to which this response is generated.
*
*	status
*		[in] Status for this response.
*
*	p_payload
*		[in] Pointer to the payload of the response MAD.
*	force_flag
*		[in] If TRUE, indicates that the response should be processed immediately
*		by VL15 poller thread
* RETURN VALUES
*	IB_SUCCESS if the response was successful.
*
*********/

/****f* OpenSM: SM/osm_sm_reroute_mlid
* NAME
*	osm_sm_reroute_mlid
*
* DESCRIPTION
*	Requests (schedules) MLID rerouting
*
* SYNOPSIS
*/
void osm_sm_reroute_mlid(osm_sm_t * sm, ib_net16_t mlid);

/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	mlid
*		[in] MLID value
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: SM/osm_sm_reroute_all_mlids
* NAME
*	osm_sm_reroute_all_mlids
*
* DESCRIPTION
*	Requests (schedules) all MLIDs rerouting
*
* SYNOPSIS
*/
void osm_sm_reroute_all_mlids(osm_sm_t * sm);

/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: OpenSM/osm_sm_wait_for_subnet_up
* NAME
*	osm_sm_wait_for_subnet_up
*
* DESCRIPTION
*	Blocks the calling thread until the subnet is up.
*
* SYNOPSIS
*/
static inline cl_status_t osm_sm_wait_for_subnet_up(IN osm_sm_t * p_sm,
						    IN uint32_t wait_us,
						    IN boolean_t interruptible)
{
	return cl_event_wait_on(&p_sm->subnet_up_event, wait_us, interruptible);
}

/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
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

/****f* OpenSM: State Manager/osm_sm_is_greater_than
* NAME
*	osm_sm_is_greater_than
*
* DESCRIPTION
*	Compares two SM's (14.4.1.2)
*
* SYNOPSIS
*/
static inline boolean_t osm_sm_is_greater_than(IN uint8_t l_priority,
					       IN ib_net64_t l_guid,
					       IN uint8_t r_priority,
					       IN ib_net64_t r_guid)
{
	return (l_priority > r_priority
		|| (l_priority == r_priority
		    && cl_ntoh64(l_guid) < cl_ntoh64(r_guid)));
}

/*
* PARAMETERS
*	l_priority
*		[in] Priority of the SM on the "left"
*
*	l_guid
*		[in] GUID of the SM on the "left"
*
*	r_priority
*		[in] Priority of the SM on the "right"
*
*	r_guid
*		[in] GUID of the SM on the "right"
*
* RETURN VALUES
*	Return TRUE if an sm with l_priority and l_guid is higher than an sm
*	with r_priority and r_guid, return FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	State Manager
*********/

/****f* OpenSM: SM State Manager/osm_sm_state_mgr_process
* NAME
*	osm_sm_state_mgr_process
*
* DESCRIPTION
*	Processes and maintains the states of the SM.
*
* SYNOPSIS
*/
ib_api_status_t osm_sm_state_mgr_process(IN osm_sm_t *sm,
					 IN osm_sm_signal_t signal);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	signal
*		[in] Signal to the state SM engine.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	State Manager
*********/

/****f* OpenSM: SM State Manager/osm_sm_state_mgr_signal_master_is_alive
* NAME
*	osm_sm_state_mgr_signal_master_is_alive
*
* DESCRIPTION
*	Signals that the remote Master SM is alive.
*	Need to clear the retry_number variable.
*
* SYNOPSIS
*/
void osm_sm_state_mgr_signal_master_is_alive(IN osm_sm_t *sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	State Manager
*********/

/****f* OpenSM: SM State Manager/osm_sm_state_mgr_check_legality
* NAME
*	osm_sm_state_mgr_check_legality
*
* DESCRIPTION
*	Checks the legality of the signal received, according to the
*  current state of the SM state machine.
*
* SYNOPSIS
*/
ib_api_status_t osm_sm_state_mgr_check_legality(IN osm_sm_t *sm,
						IN osm_sm_signal_t signal);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	signal
*		[in] Signal to the state SM engine.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	State Manager
*********/

void osm_log_sm_state(osm_sm_t *sm);

/****f* OpenSM: SM State Manager/osm_send_trap144
* NAME
*	osm_send_trap144
*
* DESCRIPTION
*	Send trap 144 to the master SM.
*
* SYNOPSIS
*/
int osm_send_trap144(osm_sm_t *sm, ib_net16_t local);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	local
*		[in] OtherLocalChanges mask in network byte order.
*
* RETURN VALUES
*	0 on success, non-zero value otherwise.
*
*********/

/****f* OpenSM: SM State Manager/osm_set_sm_priority
* NAME
*	osm_set_sm_priority
*
* DESCRIPTION
*	Sets sm_priority
*
* SYNOPSIS
*/
void osm_set_sm_priority(osm_sm_t *sm, uint8_t priority);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	priority
*		[in] SM priority.
*
*********/

/****f* OpenSM: SM State Manager/osm_set_active_sm_priority
* NAME
*	osm_set_active_sm_priority
*
* DESCRIPTION
*	Sets sm_active_priority
*
* SYNOPSIS
*/
void osm_set_active_sm_priority(osm_sm_t *sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*********/

/****f* OpenSM: SM State Manager/osm_calc_files_crc
* NAME
*	osm_calc_files_crc
*
* DESCRIPTION
*	Calc crc for the files we track and mark if it has changed since last
*	calculation
*
* SYNOPSIS
*/
uint16_t osm_calc_files_crc(IN osm_sm_t *p_sm, IN osm_crc_id_enum file_id,
			    IN boolean_t update);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	file_id
*		[in] id of the file to calc.
*		     if OSM_CRC_MAX calc all files
*
*	update
*		[in] Indication whether the function should update the crc
*		     file element with the result or just check if changed
*
* RETURN VALUES
*	Number of files which have changed during last calculation
*********/

/****f* OpenSM: SM State Manager/osm_is_crc_changed
* NAME
*	osm_is_crc_changed
*
* DESCRIPTION
*	Check if crc of specific file id was changed
*
* SYNOPSIS
*/
boolean_t osm_is_crc_changed(IN osm_sm_t *p_sm, IN osm_crc_id_enum file_id);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	file_id
*		[in] id of the file to check if changed.
*
* RETURN VALUES
*	Boolean indication if specified file was changed.
*********/

/****f* OpenSM: SM State Manager/osm_is_crc_exclusive_changed
* NAME
*	osm_is_crc_exclusive_changed
*
* DESCRIPTION
*	Check if crc of specific file id is the only one that was changed
*	while all other files are unchanged
*
* SYNOPSIS
*/
static inline boolean_t osm_is_crc_exclusive_changed(IN osm_sm_t *p_sm,
				       IN osm_crc_id_enum file_id)
{
	return (osm_is_crc_changed(p_sm, file_id) && (p_sm->crc_changed == 1));
}
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	file_id
*		[in] id of the file to check.
*
* RETURN VALUES
*	TRUE - Specified file id was changed and all others are unchanged
*	FALSE - Anything else
*********/

/****f* OpenSM: SM State Manager/osm_two_crc_exclusive_changed
* NAME
*	osm_two_crc_exclusive_changed
*
* DESCRIPTION
*	Check if crc of two specific files are the only one that were changed
*	while all other files are unchanged
*
* SYNOPSIS
*/
static inline boolean_t
osm_two_crc_exclusive_changed(IN osm_sm_t *p_sm,
			      IN osm_crc_id_enum file_id1, IN osm_crc_id_enum file_id2,
			      OUT boolean_t *file1_change, OUT boolean_t *file2_change)
{
	*file1_change = osm_is_crc_changed(p_sm, file_id1);
	*file2_change = osm_is_crc_changed(p_sm, file_id2);
	return ((*file1_change || *file2_change) && (p_sm->crc_changed == 1)) ||
		(*file1_change && *file2_change && (p_sm->crc_changed == 2));
}
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	file_id1
*		[in] id of the first file to check.
*
*	file_id2
*		[in] id of the second file to check.
*
*
* RETURN VALUES
*	TRUE - Specified file id was changed and all others are unchanged
*	FALSE - Anything else
*********/

/****f* OpenSM: SM State Manager/osm_get_crc_value
* NAME
*	osm_get_crc_value
*
* DESCRIPTION
*	Get crc value for specific file id
*
* SYNOPSIS
*/
static inline uint32_t osm_get_crc_value(IN osm_sm_t *p_sm,
					 IN osm_crc_id_enum file_id)
{
	if (file_id < OSM_CRC_MAX)
		return p_sm->files_crc[file_id].crc_value;
	else
		return 0;
}
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
*	file_id
*		[in] id of the file to return crc value for.
*
* RETURN VALUES
*	file crc value if given file_id is valid (zero if file_id not valid)
*********/

/****f* OpenSM: SM State Manager/osm_is_crc_invalidates_cache
* NAME
*	Manager/osm_is_crc_invalidates_cache
*
* DESCRIPTION
*	Check if crc changes should cause invalidatioin of ucast cache
*
* SYNOPSIS
*/
boolean_t osm_is_crc_invalidates_cache(IN osm_sm_t *p_sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to an osm_sm_t object.
*
* RETURN VALUES
*	Boolean indication whether ucast cache should be invalidated due to
*	crc files changes
*********/

/****f* OpenSM: SM State Manager/osm_calc_file_crc32
* NAME
*	Manager/osm_calc_file_crc32
*
* DESCRIPTION
*	calculate the crc for a given file
*
* SYNOPSIS
*/
uint32_t osm_calc_file_crc32(IN const char * file_name);
/*
* PARAMETERS
*	file_name
*		[in] File name to check crc for.
*
* RETURN VALUES
*	crc value for the file.
*********/

END_C_DECLS
