/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_sa_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_event.h>
#include <complib/cl_thread.h>
#include <complib/cl_timer.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_stats.h>
#include <opensm/osm_subnet.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_log.h>
#include <opensm/osm_sa_mad_ctrl.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_multicast.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

#define SA_INVALID_LID_OFFSET 0xffff
#define SA_RATE_MAX_ENUM 63

BEGIN_C_DECLS
/****h* OpenSM/SA
* NAME
*	SA
*
* DESCRIPTION
*	The SA object encapsulates the information needed by the
*	OpenSM to instantiate subnet administration.  The OpenSM
*	allocates one SA object per subnet manager.
*
*	The SA object is thread safe.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* AUTHOR
*	Ranjit Pandit, Intel
*	Anil Keshavamurthy, Intel
*
*********/

/****d* OpenSM: SA/osm_sa_state_t
* NAME
*	osm_sa_state_t
*
* DESCRIPTION
*	Enumerates the possible states of SA object.
*
* SYNOPSIS
*/
typedef enum _osm_sa_state {
	OSM_SA_STATE_INIT = 0,
	OSM_SA_STATE_READY
} osm_sa_state_t;
/***********/

/****d* OpenSM: SA/osm_mpr_rec_t
* NAME
*	osm_mpr_rec_t
*
* DESCRIPTION
*	SA MultiPathRecord response.
*
* SYNOPSIS
*/
typedef struct osm_mpr_rec {
	ib_path_rec_t path_rec;
	const osm_port_t *p_src_port;
	const osm_port_t *p_dest_port;
	int hops;
} osm_mpr_rec_t;
/***********/

/****d* OpenSM: SA/osm_sa_item_t
* NAME
*	osm_sa_item_t
*
* DESCRIPTION
*	SA response item.
*
* SYNOPSIS
*/
typedef struct osm_sa_item {
	cl_list_item_t list_item;
	union {
		char data[0];
		ib_guidinfo_record_t guid_rec;
		ib_inform_info_t inform;
		ib_inform_info_record_t inform_rec;
		ib_lft_record_t lft_rec;
		ib_link_record_t link_rec;
		ib_member_rec_t mc_rec;
		ib_mft_record_t mft_rec;
		osm_mpr_rec_t mpr_rec;
		ib_node_record_t node_rec;
		ib_path_rec_t path_rec;
		ib_pkey_table_record_t pkey_rec;
		ib_portinfo_record_t port_rec;
		ib_service_record_t service_rec;
		ib_slvl_table_record_t slvl_rec;
		ib_sminfo_record_t sminfo_rec;
		ib_switch_info_record_t swinfo_rec;
		ib_vl_arb_table_record_t vlarb_rec;
	} resp;
} osm_sa_item_t;

/****s* OpenSM: SM/osm_sa_t
* NAME
*	osm_sa_t
*
* DESCRIPTION
*	Subnet Administration structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_sa {
	osm_sa_state_t state;
	osm_sm_t *sm;
	osm_subn_t *p_subn;
	osm_vendor_t *p_vendor[OSM_MAX_BINDING_PORTS];
	osm_log_t *p_log;
	osm_mad_pool_t *p_mad_pool;
	cl_dispatcher_t *p_disp;
	cl_dispatcher_t *p_set_disp;
	cl_dispatcher_t *p_security_disp;
	cl_plock_t *p_lock;
	cl_plock_t *p_routing_lock;
	cl_spinlock_t rate_lock;
	atomic32_t sa_trans_id;
	osm_sa_mad_ctrl_t mad_ctrl;
	cl_timer_t sr_timer;
	boolean_t dirty;
	cl_disp_reg_handle_t cpi_disp_h;
	cl_disp_reg_handle_t nr_disp_h;
	cl_disp_reg_handle_t pir_disp_h;
	cl_disp_reg_handle_t gir_disp_h;
	cl_disp_reg_handle_t lr_disp_h;
	cl_disp_reg_handle_t pr_disp_h;
	cl_disp_reg_handle_t smir_disp_h;
	cl_disp_reg_handle_t mcmr_disp_h;
	cl_disp_reg_handle_t sr_disp_h;
#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
	cl_disp_reg_handle_t mpr_disp_h;
#endif
	cl_disp_reg_handle_t infr_disp_h;
	cl_disp_reg_handle_t infir_disp_h;
	cl_disp_reg_handle_t vlarb_disp_h;
	cl_disp_reg_handle_t slvl_disp_h;
	cl_disp_reg_handle_t pkey_disp_h;
	cl_disp_reg_handle_t lft_disp_h;
	cl_disp_reg_handle_t sir_disp_h;
	cl_disp_reg_handle_t mft_disp_h;
	cl_disp_reg_handle_t infr_set_disp_h;
	cl_disp_reg_handle_t gir_set_disp_h;
	cl_disp_reg_handle_t mcmr_set_disp_h;
	cl_disp_reg_handle_t sr_set_disp_h;
	cl_disp_reg_handle_t sa_sec_sa_key_disp_h;
	cl_disp_reg_handle_t sa_sec_untrusted_disp_h;
	struct osm_inform_mgr *p_inform_mgr;
} osm_sa_t;
/*
* FIELDS
*	state
*		State of this SA object
*
*	sm
*		Pointer to the Subnet Manager object.
*
*	p_subn
*		Pointer to the Subnet object for this subnet.
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
*	p_disp
*		Pointer to dispatcher
*
*	p_set_disp
*		Pointer to dispatcher for Set requests.
*
*	p_security_disp
*		Pointer to dispatcher for requests rejected due to security checks.
*
*	p_lock
*		Pointer to Lock for serialization
*
*	p_routing_lock
*		Pointer to routing lock for serialization
*
*	sa_trans_id
*		Transaction ID
*
*	mad_ctrl
*		Mad Controller
*
*	dirty
*		A flag that denotes that SA DB is dirty and needs
*		to be written to the dump file (if dumping is enabled)
*
* SEE ALSO
*	SM object
*********/

/****f* OpenSM: SA/osm_sa_construct
* NAME
*	osm_sa_construct
*
* DESCRIPTION
*	This function constructs an SA object.
*
* SYNOPSIS
*/
void osm_sa_construct(IN osm_sa_t * p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_sa_destroy.
*
*	Calling osm_sa_construct is a prerequisite to calling any other
*	method except osm_sa_init.
*
* SEE ALSO
*	SA object, osm_sa_init, osm_sa_destroy
*********/

/****f* OpenSM: SA/osm_sa_shutdown
* NAME
*	osm_sa_shutdown
*
* DESCRIPTION
*	The osm_sa_shutdown function shutdowns an SA, unregistering from all
*  dispatcher messages and unbinding the QP1 mad service
*
* SYNOPSIS
*/
void osm_sa_shutdown(IN osm_sa_t * p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to shutdown.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_init
*********/

/****f* OpenSM: SA/osm_sa_destroy
* NAME
*	osm_sa_destroy
*
* DESCRIPTION
*	The osm_sa_destroy function destroys an SA, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_sa_destroy(IN osm_sa_t * p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified SA object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_sa_construct or
*	osm_sa_init.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_init
*********/

/****f* OpenSM: SA/osm_sa_init
* NAME
*	osm_sa_init
*
* DESCRIPTION
*	The osm_sa_init function initializes a SA object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_sa_init(IN osm_sm_t * p_sm, IN osm_sa_t * p_sa,
			    IN osm_subn_t * p_subn, IN osm_vendor_t * p_vendor[],
			    IN osm_mad_pool_t * p_mad_pool,
			    IN osm_log_t * p_log, IN osm_stats_t * p_stats,
			    IN cl_dispatcher_t * p_disp,
			    IN cl_dispatcher_t * p_set_disp,
			    IN cl_dispatcher_t * p_security_disp,
			    IN cl_plock_t * p_lock,
			    IN cl_plock_t * p_routing_lock);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object to initialize.
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
*	p_log
*		[in] Pointer to the log object.
*
*	p_stats
*		[in] Pointer to the statistics object.
*
*	p_disp
*		[in] Pointer to the OpenSM central Dispatcher.
*
*	p_set_disp
*		[in] Pointer to the OpenSM Dispatcher for Set requests.
*
*	p_security_disp
*		[in] Pointer to the OpenSM Dispatcher for SA requests rejecected
*		due security checks.
*
*	p_lock
*		[in] Pointer to the OpenSM serializing lock.
*
*	p_routing_lock
*		[in] Pointer to the routing serializing lock.
*
* RETURN VALUES
*	CL_SUCCESS if the SA object was initialized successfully.
*
* NOTES
*	Allows calling other SA methods.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_destroy
*********/

/****f* OpenSM: SA/osm_sa_bind
* NAME
*	osm_sa_bind
*
* DESCRIPTION
*	Binds the SA object to a port guid.
*
* SYNOPSIS
*/
ib_api_status_t osm_sa_bind(IN osm_sa_t * p_sa, IN ib_net64_t port_guid);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object to bind.
*
*	port_guid
*		[in] Local port GUID with which to bind.
*
*
* RETURN VALUES
*	None
*
* NOTES
*	A given SA object can only be bound to one port at a time.
*
* SEE ALSO
*********/

/****f* OpenSM: SA/osm_sa_send
* NAME
*	osm_sa_send
*
* DESCRIPTION
*	Sends SA MAD via osm_vendor_send and maintains the QP1 sent statistic
*
* SYNOPSIS
*/
ib_api_status_t osm_sa_send(osm_sa_t *sa, IN osm_madw_t * p_madw,
			    IN boolean_t resp_expected);

/****f* IBA Base: Types/osm_sa_send_error
* NAME
*	osm_sa_send_error
*
* DESCRIPTION
*	Sends a generic SA response with the specified error status.
*	The payload is simply replicated from the request MAD.
*
* SYNOPSIS
*/
void osm_sa_send_error(IN osm_sa_t * sa, IN const osm_madw_t * p_madw,
		       IN ib_net16_t sa_status);
/*
* PARAMETERS
*	sa
*		[in] Pointer to an osm_sa_t object.
*
*	p_madw
*		[in] Original MAD to which the response must be sent.
*
*	sa_status
*		[in] Status to send in the response.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	SA object
*********/

/****f* OpenSM: SA/osm_sa_respond
* NAME
*	osm_sa_respond
*
* DESCRIPTION
*	Sends SA MAD response
*/
void osm_sa_respond(osm_sa_t *sa, osm_madw_t *madw, size_t attr_size,
		    cl_qlist_t *list);
/*
* PARAMETERS
*	sa
*		[in] Pointer to an osm_sa_t object.
*
*	p_madw
*		[in] Original MAD to which the response must be sent.
*
*	attr_size
*		[in] Size of this SA attribute.
*
*	list
*		[in] List of attribute to respond - it will be freed after
*		sending.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	SA object
*********/

struct osm_opensm;
/****f* OpenSM: SA/osm_sa_db_file_dump
* NAME
*	osm_sa_db_file_dump
*
* DESCRIPTION
*	Dumps the SA DB to the dump file.
*
* SYNOPSIS
*/
int osm_sa_db_file_dump(struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
* RETURN VALUES
*	 0 if the SA DB was actually dumped
*	>0 if there was no need to dump the SA DB
*	<0 if some error occurred.
*
*********/

/****f* OpenSM: SA/osm_sa_db_file_load
* NAME
*	osm_sa_db_file_load
*
* DESCRIPTION
*	Loads SA DB from the file.
*
* SYNOPSIS
*/
int osm_sa_db_file_load(struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
* RETURN VALUES
*	0 on success, other value on failure.
*
*********/

/****f* OpenSM: MC Member Record Receiver/osm_mcmr_rcv_find_or_create_new_mgrp
* NAME
*	osm_mcmr_rcv_find_or_create_new_mgrp
*
* DESCRIPTION
*	Create new Multicast group
*
* SYNOPSIS
*/

osm_mgrp_t *osm_mcmr_rcv_find_or_create_new_mgrp(IN osm_sa_t * sa,
						 IN ib_net64_t comp_mask,
						 IN ib_member_rec_t *
						 p_recvd_mcmember_rec,
						 IN boolean_t
						 force_validation);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object.
*	comp_mask
*		[in] SA query component mask
*	p_recvd_mcmember_rec
*		[in] Received Multicast member record
*	force_validation
*		[in] Should the function force Multicast member record
*		validation procedures
*
* RETURN VALUES
*	The pointer to MC group object found or created, NULL in case of errors
*
*********/

/**
 * The following expose functionality of osm_sa_path_record.c for internal use
 * by sub managers
 */
typedef struct osm_path_parms {
	ib_net16_t pkey;
	uint8_t mtu;
	uint8_t rate;
	uint8_t sl;
	uint8_t pkt_life;
	boolean_t reversible;
	int hops;
	uint32_t flow_label;
	uint8_t tclass;
} osm_path_parms_t;

ib_api_status_t osm_get_path_params(IN osm_sa_t * sa,
				    IN const osm_port_t * p_src_port,
				    IN const uint16_t slid_ho,
				    IN const osm_port_t * p_dest_port,
				    IN const uint16_t dlid_ho,
				    OUT osm_path_parms_t * p_parms);

ib_net16_t osm_pr_get_end_points(IN osm_sa_t * sa,
					IN const ib_sa_mad_t *sa_mad,
					OUT const osm_alias_guid_t ** pp_src_alias_guid,
					OUT const osm_alias_guid_t ** pp_dest_alias_guid,
					OUT const osm_port_t ** pp_src_port,
					OUT const osm_port_t ** pp_dest_port,
					OUT const ib_gid_t ** pp_sgid,
					OUT const ib_gid_t ** pp_remote_dgid);

void osm_pr_process_pair(IN osm_sa_t * sa, IN const ib_sa_mad_t * sa_mad,
				IN const osm_alias_guid_t *p_req_alias_guid,
				IN const osm_alias_guid_t * p_src_alias_guid,
				IN const osm_alias_guid_t * p_dest_alias_guid,
				IN const ib_gid_t * p_sgid,
				IN const ib_gid_t * p_dgid,
				IN cl_qlist_t * p_list);

void osm_pr_process_half(IN osm_sa_t * sa, IN const ib_sa_mad_t * sa_mad,
				IN const osm_alias_guid_t *p_req_alias_guid,
				IN const osm_alias_guid_t * p_src_alias_guid,
				IN const osm_alias_guid_t * p_dest_alias_guid,
				IN const ib_gid_t * p_sgid,
				IN const ib_gid_t * p_dgid,
				IN cl_qlist_t * p_list);

void osm_pr_rcv_process_routers(IN osm_sa_t * sa,
				IN const ib_sa_mad_t * p_sa_mad,
				IN const osm_port_t *p_src_port,
				IN const osm_alias_guid_t *p_req_alias_guid,
				IN const osm_alias_guid_t * p_src_alias_guid,
				IN const ib_gid_t * p_sgid,
				IN const ib_gid_t * p_remote_dgid,
				IN const ib_gid_t * p_mpr_dgid,
				IN cl_qlist_t * p_list);

osm_sa_item_t * osm_pr_rcv_get_lid_pair_path(IN osm_sa_t * sa,
					     IN const void * p_record,
					     IN const osm_alias_guid_t * p_src_alias_guid,
					     IN const osm_alias_guid_t * p_dest_alias_guid,
					     IN const ib_gid_t * p_sgid,
					     IN const ib_gid_t * p_remote_dgid,
					     IN const ib_gid_t * p_mpr_dgid,
					     IN const uint16_t src_lid_ho,
					     IN const uint16_t dest_lid_ho,
					     IN const ib_net64_t comp_mask,
					     IN const uint8_t preference);

uint32_t osm_pr_rcv_get_port_pair_paths(IN osm_sa_t * sa,
					IN const ib_sa_mad_t *sa_mad,
					IN const osm_alias_guid_t *p_req_alias_guid,
					IN const osm_alias_guid_t * p_src_alias_guid,
					IN const osm_alias_guid_t * p_dest_alias_guid,
					IN const ib_gid_t * p_sgid,
					IN const ib_gid_t * p_remote_dgid,
					IN const ib_gid_t * p_mpr_dgid,
					IN cl_qlist_t * p_list,
					IN uint16_t dlid_offset,
					IN uint32_t rem_paths);

boolean_t osm_sa_validate_request_rate(IN osm_sa_t * sa,
				       IN const struct osm_madw * p_madw,
				       IN osm_alias_guid_t *p_req_alias_guid);

boolean_t sa_etm_is_log_print(IN osm_sm_t *p_sm, IN uint64_t src_key);

/****f* OpenSM: SA/osm_sa_limit_rate
* NAME
*	osm_sa_limit_rate
*
* DESCRIPTION
*	Find decreased rate of input rate that does not exceed
*	maximal rate value of subnet.
*
* SYNOPSIS
*/
uint8_t osm_sa_limit_rate(IN osm_sa_t * sa, IN const uint8_t rate);
/*
* PARAMETERS
*	sa
*		[in] Pointer to a SA object.
*
*	rate
*		[in] Rate to adjust into subnet's maximal rate value.
*
* RETURN VALUE
*	The rate after adjusting to maximal rate, may be the same or lower.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_init
*********/

/****f* OpenSM: SA/osm_sa_acquire_locks
* NAME
*	osm_sa_acquire_locks
*
* DESCRIPTION
*	Acquire both routing lock and SM lock for reading, in order to avoid
*	SA queries access to structures that are changed while routing.
*
* SYNOPSIS
*/
static inline void osm_sa_acquire_locks(IN osm_sa_t * sa)
{
	CL_PLOCK_ACQUIRE(sa->p_routing_lock);
	CL_PLOCK_ACQUIRE(sa->p_lock);
}
/*
* PARAMETERS
*	sa
*		[in] Pointer to a SA object.
*
* RETURN VALUE
*	None.
*
* SEE ALSO
*	osm_sa_release_locks
*********/

/****f* OpenSM: SA/osm_sa_release_locks
* NAME
*	osm_sa_release_locks
*
* DESCRIPTION
*	Release both routing lock and SM lock, which are used in order to avoid
*	SA queries access to structures that are changed while routing.
*
* SYNOPSIS
*/
static inline void osm_sa_release_locks(IN osm_sa_t * sa)
{
	CL_PLOCK_RELEASE(sa->p_lock);
	CL_PLOCK_RELEASE(sa->p_routing_lock);
}
/*
* PARAMETERS
*	sa
*		[in] Pointer to a SA object.
*
* RETURN VALUE
*	None.
*
* SEE ALSO
*	osm_sa_acquire_locks
*********/

/****f* OpenSM: SA/osm_sa_get_physp_rate
* NAME
*	osm_sa_get_physp_rate
*
* DESCRIPTION
*	Returns the rate of the physical port: speed multiply width as reported by the port in the port info.
*	For planarized port return the addapted rate by multiplying by the number of planes
*
* SYNOPSIS
*/
uint8_t osm_sa_get_physp_rate(IN const osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to a physical port object.
*
* RETURN VALUE
*	Returns the rate of the physical port.
*
*********/

/****d* SA/osm_sa_apply_func_t
* NAME
*	osm_sa_apply_func_t
*
* DESCRIPTION
*	The osm_sa_apply_func_t function type defines the prototype for
*	functions to apply over ports.
*
* SYNOPSIS
*/
typedef void (*osm_sa_apply_func_t)(IN osm_sa_t * sa, IN const osm_physp_t * p_physp,
				    IN void * context);
/*
* PARAMETERS
*	sa
*		[in] SA object.
*
*	p_physp
*		[in] Physical port.
*
*	context
*		[in] Context provided in to the function.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	This function type is provided as function prototype reference for the
*	function provided by users as a parameter to osm_sa_apply_on_ports.
*
* SEE ALSO
*	osm_sa_apply_on_ports
*********/

/****f* OpenSM: SA/osm_sa_apply_on_ports
* NAME
*	osm_sa_apply_on_ports
*
* DESCRIPTION
*	Apply a function on all end-ports/physical ports of a node.
*	On planarized CAs, apply the function on each APort once.
*
* SYNOPSIS
*/
void osm_sa_apply_on_ports(IN osm_sa_t * sa, IN osm_node_t * p_node, IN boolean_t only_end_ports,
			   IN osm_sa_apply_func_t func, IN void * context);
/*
* PARAMETERS
*	sa
*		[in] Pointer to a SA object.
*
*	p_node
*		[in] Pointer to a node object.
*
*	only_end_ports
*		[in] Whether to apply the function on end-ports only, or on all physical ports.
*
*	func
*		[in] Function to apply.
*
*	context
*		[in] Context for the function.
*
* RETURN VALUE
*	None.
*
* SEE ALSO
*********/

/****f* OpenSM: SA/osm_sa_get_physp
* NAME
*	osm_sa_get_physp
*
* DESCRIPTION
*	Get the physp object for a given port number.
*	On planarized CAs, return the APorts object with APort number equal to the port number.
*	That's because the client sees the APort as a single port, and doesn't care about the
*	multiple physical ports.
*
* SYNOPSIS
*/
const osm_physp_t *osm_sa_get_physp(IN osm_sa_t * sa, IN osm_node_t * p_node, IN uint8_t port_num);
/*
* PARAMETERS
*	sa
*		[in] Pointer to a SA object.
*
*	p_node
*		[in] Pointer to a node object.
*
*	port_num
*		[in] The port number to get the physp for.
*
* RETURN VALUE
*	Pointer to the physp object for the given port number, or NULL if the port number is invalid.
*	On planarized CAs, return the APorts object with APort number equal to the port number.
*
* SEE ALSO
*********/

/****f* OpenSM: SA/osm_sa_get_rid_port_num
* NAME
*	osm_sa_get_rid_port_num
*
* DESCRIPTION
*	Get the port number used to represent the port in the RID.
*	The client sees the APort as a single port,
*	and doesn't care about the multiple physical ports.
*	Thus for planarized CAs, the RID port number is the APort number.
*	In all other cases, the RID port number is the IB port number.
*
* SYNOPSIS
*/
uint8_t osm_sa_get_rid_port_num(IN const osm_physp_t *p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to a physp object.
*
* RETURN VALUE
*	The port number used to represent the port in the RID of NodeRecord, PortInfoRecord,
*	P_KeyTableRecord, and SLtoVLMappingTableRecord.
*
* SEE ALSO
*********/

END_C_DECLS
