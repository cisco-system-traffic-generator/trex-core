/*
 * Copyright (c) 2018 Mellanox Technologies LTD. All rights reserved.
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
 *	This file defines the osm_enhanced_qos_mgr_t struct.
 *	This object represents the enhanced qos Manager object.
 */

#pragma once

#include <iba/ib_types.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****h* OpenSM/Ehnanced QoS Manager
* NAME
*	Enhanced QoS Manager
*
* DESCRIPTION
*	The enhanced QoS Manager object is responsible to parse configuration
*	and set accordingly all ports using the QosConfig VS MADs.
*	The QoS config enables enhanced QoS capabilities in comparison with
*	the standard VL Arbitration QoS configuration.
*
*
* AUTHOR
*	Julia Levin, Shlomi Nimrodi, Mellanox
*
*********/

/****s* OpenSM: Enhanced QoS Manager/osm_qos_config_t
* NAME
*	osm_qos_config_t
*
* DESCRIPTION
*	Object used to describe qos config
*
*	Currently qos config consist only out of rate limit.
*
* SYNOPSIS
*/
typedef struct osm_qos_config {
	cl_map_item_t map_item;
	uint32_t rate_limit;
	boolean_t is_manual_rule;
} osm_qos_config_t;
/*
* FIELDS
*	map_item
*		cl map item
*
*	rate_limit
*		Rate Limit
*
*	is_manual_rule
*		Flag indicating that RL was obtained from
*		specific rule (excluding default rule)
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****s* OpenSM: Enhanced QoS Manager/osm_all_sls_qos_config_set_t
* NAME
*	osm_all_sls_qos_config_set_t
*
* DESCRIPTION
*	Object used to describe a set of qos config for all SLs.
*
* SYNOPSIS
*/
typedef struct osm_all_sls_qos_config_set {
	cl_map_item_t map_item;
	osm_qos_config_t qos_configs[IB_NUMBER_OF_SLS];
	boolean_t is_guid_rule;
} osm_all_sls_qos_config_set_t;
/*
* FIELDS
*	map_item
*		cl map item
*
*	rate_limits
*		Array of rate limits, one per each SL.
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****s* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_t
* NAME
*	osm_enhanced_qos_mgr_t
*
* DESCRIPTION
*	Object used to describe a set of rate limits for all SLs.
*
* SYNOPSIS
*/
typedef struct osm_enhanced_qos_mgr {
	cl_qmap_t sl_qos_config;
	cl_qmap_t sl_qos_vport_config;
	osm_all_sls_qos_config_set_t default_sl_set;
	osm_all_sls_qos_config_set_t default_vport_sl_set;
	osm_all_sls_qos_config_set_t unlimited_sl_set;
	osm_subn_t *p_subn;
	boolean_t is_configured;
} osm_enhanced_qos_mgr_t;
/*
* FIELDS
*	sl_qos_config
*		Map of SLs qos config sets indexed by guid
*
*	sl_qos_vport_config
*		Map of SLs qos config sets indexed by vport guid
*
*	default_sl_set
*		Populated from the "default" rule of BW_RULE section
*		- used for guids without per-guid/port_group rule
*		- used for SLs not specified in the per-guid/port_group rule
*		Initialized to unlimited sl set
*
*	default_vport_sl_set
*		Populated from the "default" rule of VPORT_BW_RULE section
*		- used for guids without per-guid/port_group rule
*		- used for SLs not specified in the per-guid rule
*		Initialized to unlimited sl set for vports with non-zero index
*
*	unlimited_sl_set
*		Hard coded unlimited rate limit for all SLs
*
*       p_subn
*		Pointer to an osm_subn_t object
*
*	is_configured
*		A boolean that tells if the configuration exists and is valid
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/* Attribute modifier bit 31 is 1 if the MAD is sent to Vport */
#define VPORT_QOS_CONFIG_SL_ATTR_MOD_HO (1 << 31)

/****f* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_init
* NAME
*	osm_enhanced_qos_mgr_init
*
* DESCRIPTION
*	Init the enhanced QoS manager
*
* SYNOPSIS
*/
ib_api_status_t
osm_enhanced_qos_mgr_init(OUT osm_enhanced_qos_mgr_t *p_mgr,
			  IN struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to Enhanced QoS Manager object.
*
*	p_sm
*		[in] Pointer ro osm_sm_t object
*
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****f* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_destroy
* NAME
*	osm_enhanced_qos_mgr_destroy
*
* DESCRIPTION
*	Destroy the enhanced QoS manager
*
* SYNOPSIS
*/
void osm_enhanced_qos_mgr_destroy(IN osm_enhanced_qos_mgr_t * p_mgr);
/*
* PARAMETERS
*	p_mgr
*		[in] Pointer to Enhanced QoS Manager object.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****f* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_parse_config
* NAME
*	osm_enhanced_qos_mgr_parse_config
*
* DESCRIPTION
*	Parse enhanced qos manager policy file
*
* SYNOPSIS
*/
void osm_enhanced_qos_mgr_parse_config(OUT osm_enhanced_qos_mgr_t *p_mgr,
				       IN struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_mgr
*		[out] Pointer to Enhanced QoS Manager object.
*
*	p_sm
*		[in] Pointer to osm_sm_t object
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****f* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_process
* NAME
*	osm_enhanced_qos_mgr_process
*
* DESCRIPTION
*	Enhanced qos manager goes over eligible ports and adds their
*	qos config sl configuration MADs to the given list.
*
* SYNOPSIS
*/
void osm_enhanced_qos_mgr_process(IN struct osm_sm * p_osm,
				  OUT cl_qlist_t * batch_mad_list);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to osm_sm_t object.
*
*	batch_mad_list
*		[out] Pointer to qos config mads list.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/

/****f* OpenSM: Enhanced QoS Manager/osm_enhanced_qos_mgr_process_vports
* NAME
*	osm_enhanced_qos_mgr_process_vports
*
* DESCRIPTION
*	Enhanced qos manager goes over eligible vports and sends to them
*	qos config sl configuration MADs.
*
* SYNOPSIS
*/
void
osm_enhanced_qos_mgr_process_vports(IN struct osm_sm * p_osm,
				    IN struct osm_port *p_port,
				    OUT cl_qlist_t *port_mad_list);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to osm_sm_t object.
*
*       p_port
*		[in] Pointer to the port for processing its vports.
*
*	port_mad_list
*		[out] Pointer to list of mads to be sent for this port.
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Enhanced QoS Manager object
*********/
typedef struct qos_mad_list {
	cl_list_item_t list_item;
	cl_qlist_t port_mad_list;
} qos_mad_list_t;


END_C_DECLS
