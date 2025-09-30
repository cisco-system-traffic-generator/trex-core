/*
 * Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2016 Mellanox Technologies LTD. All rights reserved.
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
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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
 *      Log Verbose Bypass
 */

#pragma once

#include <complib/cl_map.h>
#include <iba/ib_types.h>
#include <opensm/osm_log.h>

struct osm_sm;

/****s* OpenSM: SM/osm_guid_map_t
* NAME
*  osm_guid_map_t
*
* DESCRIPTION
*  This object represents a map of guids, the usage of it is to keep all
*  guids which belong to the same subnet prefix
*
* SYNOPSIS
*/
typedef struct osm_guid_map {
	cl_map_item_t map_item;
	cl_map_t guids;
} osm_guid_map_t;
/*
* FIELDS
*	map_item
*		Map item required to keep this object in a map
*
*	guids
*		Map of guids for which to bypass log verbosity
*/

/****s* OpenSM: SM/osm_log_verbose_bypass_t
* NAME
*  osm_log_verbose_bypass_t
*
* DESCRIPTION
*  Object which keep data about all elements for which log verbosity
*  shall be ignored
*
* SYNOPSIS
*/
typedef struct osm_log_verbose_bypass {
	cl_map_t attr_ids;
	cl_map_t guids;
	cl_map_t lids;
} osm_log_verbose_bypass_t;
/*
* FIELDS
*	attr_ids
*		Map of attr ids for which to bypass log verbosity
*
*	guids
*		Map of guids for which to bypass log verbosity
*
*	lids
*		Map of lids for which to bypass log verbosity
*/

/****s* OpenSM: SM/osm_verbose_bypass_data_t
* NAME
*  osm_verbose_bypass_data_t
*
* DESCRIPTION
*  Object which keep data required to check if log vebose can be bypassed
*
* SYNOPSIS
*/
#define VERBOSE_DATA_ELEMENTS 10
typedef struct osm_verbose_bypass_data {
	ib_net16_t attr_id;
	ib_net64_t guids[VERBOSE_DATA_ELEMENTS];
	ib_net16_t base_lids[VERBOSE_DATA_ELEMENTS];
} osm_verbose_bypass_data_t;

#define MAP_GROWTH_SIZE 8

void osm_verbose_bypass_construct(osm_log_verbose_bypass_t *p_verbose_bypass);

void osm_verbose_bypass_clear(struct osm_sm *p_sm, osm_log_verbose_bypass_t *p_verbose_bypass);

void osm_verbose_bypass_destroy(struct osm_sm *p_sm, osm_log_verbose_bypass_t *p_verbose_bypass);

boolean_t is_verbose_bypass_allowed(struct osm_sm *p_sm,
				    osm_verbose_bypass_data_t *p_data);

void osm_verbose_bypass_add_attr_id(struct osm_sm *p_sm, uint64_t key);

void osm_verbose_bypass_add_guid(struct osm_sm *p_sm, ib_net64_t key);

void osm_verbose_bypass_add_lid(struct osm_sm *p_sm, ib_net64_t key);

static inline
void osm_verbose_bypass_end(boolean_t registered, osm_log_t *p_log)
{
	if (registered)
		osm_log_unregister_admin_pid(p_log);
}

boolean_t osm_verbose_bypass_handle_pi(struct osm_sm *p_sm,
				       ib_net16_t base_lid,
				       ib_net64_t port_guid,
				       ib_net64_t node_guid);

boolean_t osm_verbose_bypass_handle_ni(struct osm_sm *p_sm, ib_node_info_t *p_ni);

boolean_t osm_verbose_bypass_handle_nd(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_ar_info(struct osm_sm *p_sm,
					     osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_ar_lft(struct osm_sm *p_sm,
					   osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_ar_group_table(struct osm_sm *p_sm,
						   osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_ar_group_table_copy(struct osm_sm *p_sm,
							osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_si(struct osm_sm *p_sm, ib_net64_t node_guid);

boolean_t osm_verbose_bypass_handle_extended_si(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_pkt(struct osm_sm *p_sm, osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_mepi(struct osm_sm *p_sm, osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_gi(struct osm_sm *p_sm, osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_geni(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_vi(struct osm_sm *p_sm, osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_vps(struct osm_sm *p_sm, osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_vnd(struct osm_sm *p_sm, osm_port_t *p_port,
					ib_net64_t vnode_guid);

boolean_t osm_verbose_bypass_handle_vni(struct osm_sm *p_sm, osm_port_t *p_port,
					ib_net64_t vnode_guid);

boolean_t osm_verbose_bypass_handle_vpi(struct osm_sm *p_sm, osm_port_t *p_port,
					ib_net64_t vport_guid);

boolean_t osm_verbose_bypass_handle_pr(struct osm_sm *p_sm, ib_path_rec_t *p_pr,
				       ib_net64_t comp_mask,
				       const osm_alias_guid_t *p_req_alias_guid);

boolean_t osm_verbose_bypass_handle_mcmpr(struct osm_sm *p_sm,
					  ib_member_rec_t *p_mcm_pr);

boolean_t osm_verbose_bypass_handle_qcsl(struct osm_sm *p_sm,
					 osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_hbf_config(struct osm_sm *p_sm,
					       osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_whbf_config(struct osm_sm *p_sm,
						osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_pfrn_config(struct osm_sm *p_sm,
						osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_profiles_config(struct osm_sm *p_sm,
						    osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_credit_watchdog_config(struct osm_sm *p_sm,
							   osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_ber_config(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_alid_info(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_rail_filter_config(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_nvl_hbf_config(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_lft_split(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_end_port_plane_filter_config(struct osm_sm *p_sm,
								 osm_port_t *p_port);

boolean_t osm_verbose_bypass_handle_contain_and_drain_info(struct osm_sm *p_sm, osm_node_t *p_node);

boolean_t osm_verbose_bypass_handle_contain_and_drain_port_state(struct osm_sm *p_sm, osm_node_t *p_node);
