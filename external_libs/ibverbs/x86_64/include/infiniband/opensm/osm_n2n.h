/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *    osm Node to Node class (class 0x0C)
 *
 * Author:
 *    Or Nechemia, NVIDIA
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mads_engine.h>

#define OSM_N2N_TIMEOUT_COUNT_THRESHOLD			3
#define OSM_N2N_DEFAULT_MAX_OUTSTANDING_QUERIES		500
#define OSM_N2N_KEY_DEFAULT_PROTECT_BIT			0x1

struct osm_sm;

/****s* OpenSM: N2N/osm_n2n_t
* NAME
*	osm_n2n_t
*
* DESCRIPTION
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_n2n {
	struct osm_opensm *p_osm;
	atomic32_t trans_id;
	cl_disp_reg_handle_t n2n_disp_h;
	cl_disp_reg_handle_t n2n_trap_disp_h;
        osm_mads_engine_t mads_engine;
	ib_net64_t port_guid;
	boolean_t n2n_key_param_update;
	unsigned int num_configured;
} osm_n2n_t;
/*
* FIELDS
*	p_osm
*       	Pointer to the OpenSM object.
*
*	n2n_key_param_update
*		Boolean indicate whether should update N2N key parameters by
*		KeyInfo MAD.
*
*	num_configured
*		Number of devices which N2N keys were configured successfully.
*
*********/

/****f* OpenSM: N2N/osm_n2n_init
* NAME
* 	osm_n2n_init
*
* DESCRIPTION
* 	Initialize n2n class object.
*
* SYNOPSIS
*/
ib_api_status_t osm_n2n_init(osm_n2n_t * p_n2n,
			     struct osm_opensm * p_osm,
			     const osm_subn_opt_t * p_opt);
/*
* PARAMETERS
*	p_n2n
*		Pointer to n2n class object.
*
*	p_osm
*		Pointer to SM object.
*
*	p_opt
*		Pointer to the subnet options structure.
*
* RETURN VALUE
*	IB_SUCCESS if the n2n management object was initialized successfully.
*
* SEE ALSO
*
*********/

ib_api_status_t osm_n2n_bind(osm_n2n_t * p_n2n, ib_net64_t port_guid);

void osm_n2n_shutdown(osm_n2n_t * p_n2n);
void osm_n2n_destroy(osm_n2n_t * p_n2n);
int osm_n2n_wait_pending_transactions(struct osm_opensm * p_osm);

void osm_n2n_configure(struct osm_opensm * p_osm);

