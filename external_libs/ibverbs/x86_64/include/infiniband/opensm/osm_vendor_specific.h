/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2020 Mellanox Technologies LTD. All rights reserved.
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
 *    osm vendor specific (class 0x0A)
 *
 * Author:
 *    Or Nechemia, Mellanox
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mads_engine.h>

#define OSM_VS_TIMEOUT_COUNT_THRESHOLD 3
#define OSM_VS_DEFAULT_MAX_OUTSTANDING_QUERIES 500
#define OSM_VS_KEY_DEFAULT_CI_PROTECT 0x1

struct osm_sm;

/****s* OpenSM: VendorSpecific/osm_vendor_specific_t
* NAME
*	osm_vendor_specific_t
*
* DESCRIPTION
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_vendor_specific {
	struct osm_opensm *p_osm;
	atomic32_t trans_id;
	osm_mads_engine_t mads_engine;
	cl_disp_reg_handle_t vs_disp_h;
	cl_disp_reg_handle_t vs_trap_disp_h;
	ib_net64_t port_guid;
	boolean_t vs_key_param_update;
	unsigned int num_configured;
} osm_vendor_specific_t;
/*
* FIELDS
*	p_osm
*       	Pointer to the OpenSM object.
*
*	vs_key_param_update
*		Boolean indicate whether should update VS key parameters by
*		KeyInfo MAD.
*
*	num_configured
*		Number of devices which VS keys were configured successfully.
*
*********/

/****f* OpenSM: VendorSpecific/osm_vendor_specific_init
* NAME
* 	osm_vendor_specific_init
*
* DESCRIPTION
* 	Initialize vendor specific class object.
*
* SYNOPSIS
*/
ib_api_status_t osm_vendor_specific_init(osm_vendor_specific_t * p_vs,
					struct osm_opensm * p_osm,
					const osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_vs
*		Pointer to vendor specific class object.
*
*	p_osm
*		Pointer to SM object.
*
*	p_opt
*		Pointer to the subnet options structure.
*
* RETURN VALUE
*	IB_SUCCESS if the vendor specific management object was
*	initialized successfully.
*
* SEE ALSO
*
*********/

/****f* OpenSM: VendorSpecific/osm_vendor_specific_bind
* NAME
* 	osm_vendor_specific_bind
*
* DESCRIPTION
* 	Bind vendor specific class to port of input GUID.
*
* SYNOPSIS
*/
ib_api_status_t osm_vendor_specific_bind(osm_vendor_specific_t * p_vs,
					 ib_net64_t port_guid);
/*
* PARAMETERS
*	p_vs
*		Pointer to vendor specific class object.
*
*	port_guid
*		Port GUID to bind vendor specific management class to.
*
* RETURN VALUE
*	IB_SUCCESS if vendor specific management class bound successfully.
*
* SEE ALSO
*
*********/

/****f* OpenSM: KeyManager/osm_vendor_specific_wait_pending_transactions
* NAME
* 	osm_vendor_specific_wait_pending_transactions
*
* DESCRIPTION
*	Wait for all VS outstanding MADs.
*
* SYNOPSIS
*/
int osm_vendor_specific_wait_pending_transactions(struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to OpenSM object.
*
* RETURN VALUE
*	None.
*
* SEE ALSO
*
*********/

void osm_vendor_specific_destroy(osm_vendor_specific_t * p_vs);

void osm_vendor_specific_shutdown(osm_vendor_specific_t * p_vs);

/****f* OpenSM: VendorSpecific/osm_vendor_specific_configure_subnet_keys
* NAME
* 	osm_vendor_specific_configure_subnet_keys
*
* DESCRIPTION
* 	Configure VS keys for ports by sending ClassPortInfo and KeyInfo.
*
* SYNOPSIS
*/
int osm_vendor_specific_configure_subnet_keys(struct osm_opensm * p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to SM object.
*
* RETURN VALUE
*	1 if successfully configured keys for all relevant ports. Otherwise 0.
*
* SEE ALSO
*
*********/

int osm_vendor_specific_setup(struct osm_opensm * p_osm);

