/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2016, Bay Microsystems, Inc All Rights Reserved
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
 *    Declaration of OSM route Policy data types and functions.
 *
 * Author:
 *    Manickavel Subramani, Bay Microsystems Inc
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_list.h>
#include <opensm/osm_router.h>

struct osm_subn;
struct osm_sm;
struct osm_sa_item;

/***************************************************/
typedef struct osm_route_rule {
	char *name;			/* single string (this route rule name) */
	cl_list_t prefix_list;		/* List of subnet prefixs */
	uint64_t router_guid;		/* Router guid */
	uint32_t weight;		/* route weight */
} osm_route_rule_t;
/*
* FIELDS
*	name
*		Name of the policy type static/dynamic/default.
*	prefix_list
*		Contains list of prefixes that can be reached through router.
*
*	router_guid
*		guid of the router listed subnet prefixes can be reached.
*
*	weight
*		Weight of thr route higher the weight is most preferred router.
*/

/***************************************************/
typedef struct osm_load_share_rule {
	uint64_t subnet_prefix;
	cl_map_t load_share_map;
} osm_load_share_rule_t;

/*
* FIELDS
*       subnet_prefix
*               Contains remote subnet prefix.
*
*      load_share_map
*               Contains router GUID to percentage map.
*/

/***************************************************/

typedef struct osm_router_policy {
	cl_list_t route_rules;		/* list of osm_route_rule_t */
	cl_list_t load_share_rules;	/* list of osm_load_share_rule_t */
	cl_map_t  load_share_state;
} osm_router_policy_t;

/*
* FIELDS
*      route_rules
*               Contains list of route rules.
*
*      load_share_rules
*               Contains list of load share rules.
*
*      load_share_state
*               Contains map of subnet prefix to list of route factor.
*/

/***************************************************/

/****f* OpenSM: Router Policy/osm_router_policy_init
* NAME
*	osm_router_policy_init
*
* DESCRIPTION
*	The osm_router_policy_init function intilaizes the router policy
*
* SYNOPSIS
*/

void osm_router_policy_init(OUT osm_subn_t *p_subn);
/*
* PARAMETERS
*	p_subn
*		[out] Pointer to a subnet object,
*
* RETURN VALUE
*	This function does not return a value.
*********/

/****f* OpenSM: Router Policy/osm_router_policy_construct
* NAME
*	osm_router_policy_construct
*
* DESCRIPTION
*	The osm_router_policy_construct function construct the router policy
*
* SYNOPSIS
*/

void osm_router_policy_construct(IN osm_router_policy_t * p_rp);
/*
* PARAMETERS
*	p_rp
*		[in] Pointer to a router policy object,
* RETURN VALUE
*	This function does not return a value.
*********/

/****f* OpenSM: Router Policy/osm_router_policy_is_policy_enabled
* NAME
*	osm_router_policy_is_policy_enabled
*
* DESCRIPTION
*	This function returns True when legacy router policy is enabled,
*	i.e router_policy_enabled and FLIDs section is not present in the file
*	(the file can be empty or corrupted, but what is important
*	that FLID section is not there).
*
* SYNOPSIS
*/

boolean_t
osm_router_policy_is_policy_enabled(IN osm_subn_t * p_subn, IN osm_router_policy_t * p_rp);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*	p_rp
*		[in] Pointer to a router policy object,
* RETURN VALUE
*	True if legacy router policy enabled, False otherwise
*********/

/****f* OpenSM: Router Policy/osm_router_policy_destroy
* NAME
*	osm_router_policy_destroy
*
* DESCRIPTION
*	The osm_router_policy_destroy function destroies router policy
*
* SYNOPSIS
*/
void osm_router_policy_destroy(IN osm_router_policy_t * p_rp);
/*
* PARAMETERS
*	p_rp
*		[in] Pointer to a router policy object,
* RETURN VALUE
*	This function does not return a value.
*********/


/****f* OpenSM: Router Policy/osm_router_policy_route_rule_destroy
* NAME
*       osm_router_policy_route_rule_destroy
*
* DESCRIPTION
*       The osm_router_policy_route_rule_destroy function
*	destroys route rule
*
* SYNOPSIS
*/
void osm_router_policy_route_rule_destroy(osm_route_rule_t * p);
/*
* PARAMETERS
*       p_rp
*               [in] Pointer to a route rule object,
* RETURN VALUE
*       This function does not return a value.
*********/


/****f* OpenSM: Router Policy/osm_router_policy_load_share_rule_destroy
* NAME
*       osm_router_policy_load_share_rule_destroy
*
* DESCRIPTION
*       The osm_router_policy_load_share_rule_destroy function
*       destroys load share rule
*
* SYNOPSIS
*/
void osm_router_policy_load_share_rule_destroy(osm_load_share_rule_t * p);
/*
* PARAMETERS
*       p
*               [in] Pointer to a load share rule object,
* RETURN VALUE
*       This function does not return a value.
*********/


/****f* OpenSM: Router Policy/osm_route_parse_policy_file
* NAME
*	osm_route_parse_policy_file
*
* DESCRIPTION
*	The osm_route_parse_policy_file function parses router
*	router policy file if configured.
*
* SYNOPSIS
*/
void osm_route_parse_policy_file(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object,
* RETURN VALUE
*	This function does not return a value.
*/

/****f* OpenSM: Router Policy/pr_rcv_filter_routers
* NAME
*	pr_rcv_filter_routers
*
* DESCRIPTION
*	The pr_rcv_filter_routers function filters the routes
*	based on the avialble dynamic routes and configured
*	static routes through route policy file
*
* SYNOPSIS
*/
void pr_rcv_filter_routers(IN const osm_subn_t *p_subn,
			   IN const cl_qlist_t * p_list,
			   OUT struct osm_sa_item ** p_pr_item);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a subnet object
*
*	p_list
*		[in] contains selected Path Record objects
*
*	p_pr_item
*		[out] contains Path Record objects
* RETURN VALUE
*	This function does not return a value.
*/


