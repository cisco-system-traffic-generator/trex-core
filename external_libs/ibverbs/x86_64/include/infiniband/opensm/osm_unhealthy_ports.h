/*
 * Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
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
 *	Declaration of osm_unhealthy_port_t.
 */

#pragma once

#include <sys/time.h>
#include <complib/cl_fleximap.h>
#include <iba/ib_types.h>
#include <opensm/osm_base.h>
#include <opensm/osm_path.h>
#include <opensm/osm_subnet.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

struct osm_physp;
struct osm_node;
struct osm_sm;

/****s* OpenSM: Port/osm_healthy_port_t
* NAME
*	osm_healthy_port_t
*
* DESCRIPTION
*	This object represents a healthy port.
*	Meaning, a port that was marked as 'healthy' by unhealthy configuration file.
*
* SYNOPSIS
*/
typedef struct osm_healthy_port {
	cl_fmap_item_t fmap_item;
	struct osm_healthy_port *p_obj;
	ib_net64_t peer_guid;
	uint8_t peer_port_num;
} osm_healthy_port_t;
/*
*  NOTES
*	Node GUID and port number of unhealthy configuration file entries are both of
*	unhealthy's peer. Hence, when parsing a 'healthy' port which is reffered by
*	this structure, parameters indicate of port peer.
*
* SEE ALSO
*	osm_unhealthy_port_t
*
*********/

/****s* OpenSM: Port/osm_unhealthy_port_t
* NAME
*	osm_unhealthy_port_t
*
* DESCRIPTION
*	This object represents an unhealthy port.
*
* SYNOPSIS
*/
typedef struct osm_unhealthy_port {
	cl_fmap_item_t fmap_item;
	struct osm_unhealthy_port *p_obj;
	ib_net64_t guid;
	ib_net64_t peer_guid;
	uint64_t cond_mask;
	struct timeval timestamp;
	boolean_t recover;
	uint32_t set_err_hist;
	uint32_t flapping_hist;
	uint32_t no_resp_hist;
	uint32_t illegal_count;
	uint8_t port_num;
	char *node_desc;
	uint8_t node_type;
	uint8_t peer_port_num;
	char *peer_node_desc;
	boolean_t report;
	boolean_t ignore;
	boolean_t disable;
	boolean_t isolate;
	boolean_t no_discover;
	boolean_t answered_last_ls;
	osm_hm_action_type_enum	manual_action;
} osm_unhealthy_port_t;
/*
* FIELDS
*	recover
*		Boolean used while parsing unhealthy file, indicates whether to
*		recover the port when using "all healthy".
*		If port still exists in unhealthy file, should not be recovered.
*
* SEE ALSO
*/

/****f* OpenSM: Unhealthy ports/osm_hm_set_by_node
* NAME
*	osm_hm_set_by_node
*
* DESCRIPTION
* 	Returns the action that is taken due to the cond, if some new unhealthy port was created.
* 	If no unhealthy port was created or the action is OSM_HM_ACTION_IGNORE,
* 	return OSM_HM_ACTION_IGNORE.
*
* SYNOPSIS
*/
osm_hm_action_type_enum osm_hm_set_by_node(IN struct osm_sm *sm, IN struct osm_node *p_node,
					   IN osm_hm_cond_type_enum cond);

static inline boolean_t osm_hm_is_cond_set(IN osm_unhealthy_port_t *p_port,
				    IN osm_hm_cond_type_enum cond)
{
	if (p_port)
		return (p_port->cond_mask & 1 << cond);
	else
		return FALSE;
}

/****f* OpenSM: Unhealthy ports/osm_hm_set_by_physp
* NAME
*	osm_hm_set_by_physp
*
* DESCRIPTION
* 	Returns the action that is taken due to the cond, if a new unhealthy port was created.
* 	If no unhealthy port was created or the action is OSM_HM_ACTION_IGNORE,
* 	return OSM_HM_ACTION_IGNORE.
*
* SYNOPSIS
*/
osm_hm_action_type_enum osm_hm_set_by_physp(IN struct osm_sm *sm, IN struct osm_physp *p_physp,
					    IN osm_hm_cond_type_enum cond);


/****f* OpenSM: Unhealthy ports/osm_hm_set_by_peer_physp
* NAME
*	osm_hm_set_by_peer_physp
*
* DESCRIPTION
* 	Returns the action that is taken due to the cond, if a new unhealthy port was created.
* 	If no unhealthy port was created or the action is OSM_HM_ACTION_IGNORE,
* 	return OSM_HM_ACTION_IGNORE.
*
* SYNOPSIS
*/
osm_hm_action_type_enum osm_hm_set_by_peer_physp(IN struct osm_sm *sm,
						 IN struct osm_physp *p_peer_physp,
						 IN osm_hm_cond_type_enum cond);

void osm_hm_set_by_smp_dr_path(IN struct osm_sm *sm, IN osm_madw_t * p_madw,
			       IN osm_hm_cond_type_enum cond);

void osm_unhealthy_port_delete(IN OUT osm_unhealthy_port_t
			       **pp_unhealthy_port);

void osm_unhealthy_port_remove(IN struct osm_sm *sm,
			       IN OUT osm_unhealthy_port_t **pp_unhealthy_port);

void osm_hm_mgr_process(IN struct osm_sm * sm);

boolean_t lookup_isolated_port(IN struct osm_sm *sm, IN struct osm_node * p_node,
			       IN const uint8_t port_num);

boolean_t lookup_no_discover_port(IN struct osm_sm *sm,
				  IN struct osm_node *p_node,
				  IN const uint8_t port_num);

boolean_t osm_hm_set_port_answer_ls(IN struct osm_sm *sm,
				    IN struct osm_node *p_node,
				    IN const uint8_t port_num,
				    IN boolean_t answered);

osm_unhealthy_port_t *osm_hm_get_unhealthy_port(IN struct osm_sm *sm,
						IN struct osm_node *p_node,
						IN const uint8_t port_num);

osm_unhealthy_port_t *osm_hm_get_unhealthy_port_by_guid(IN struct osm_sm *sm,
							IN ib_net64_t guid,
							IN const uint8_t port_num);

void osm_hm_set_cond(IN struct osm_sm *sm,
		     IN osm_unhealthy_port_t * p_unhealthy_port,
		     IN osm_hm_cond_type_enum cond);

void osm_hm_update_actions(IN struct osm_sm *sm,
			   IN osm_unhealthy_port_t * p_unhealthy_port);

osm_unhealthy_port_t *osm_unhealthy_port_new(IN ib_net64_t guid,
					     IN uint8_t port_num,
					     IN char *node_desc,
					     IN uint8_t node_type,
					     IN ib_net64_t peer_guid,
					     IN uint8_t peer_port_num,
					     IN char *peer_node_desc);

void osm_hm_mark_recover_all(IN osm_subn_t * p_subn);
void osm_hm_recover_all_healthy(IN osm_subn_t * p_subn);

/****f* OpenSM: Unhealthy ports/osm_unhealthy_port_is_healthy
* NAME
*	osm_unhealthy_port_is_healthy
*
* DESCRIPTION
* 	Returns indication of whether an osm_unhealthy_port object indicates
* 	that the port is healthy.
* 	The port is considered healthy, only if osm_unhealthy_port object does
* 	not hold any 'unhealthiness' information on a port.
*
* SYNOPSIS
*/
boolean_t osm_unhealthy_port_is_healthy(IN struct osm_sm *sm,
                                        IN osm_unhealthy_port_t *p_unhealthy_port);
/*
* PARAMETERS
*	sm
*		[in] Pointer to a subnet manager object
*
* 	p_unhealthy_port
* 		[in] Pointer to a osm_unhealthy_port_t
*
* RETURN VALUE
*	True if this osm_unhealthy_port_t object indicates that the port is
*	healthy, False otherwise.
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* OpenSM: Unhealthy ports/osm_hm_insert_marked_healthy
* NAME
*	osm_hm_insert_marked_healthy
*
* DESCRIPTION
* 	Inserts to healthy_port_tbl a new object reffering to the input unhealthy
* 	port (by guid and port number).
*
* SYNOPSIS
*/
void osm_hm_insert_marked_healthy(IN struct osm_sm * sm,
				  IN ib_net64_t peer_guid,
				  IN const uint8_t peer_port_num);
/*
* PARAMETERS
*	sm
*		Pointer to a subnet manager object
*
*	peer_guid
*		Peer guid
*
*	peer_port_num
*		Peer port number
*
* RETURN VALUE
*	None
*
* NOTES
* 	healthy_port_tbl is used to indicate ports that are marked as 'healthy' on
* 	unhealthy configuration file.
*
* SEE ALSO
* 	healthy_port_tbl
*
*********/

/****f* OpenSM: Unhealthy ports/osm_hm_is_marked_healthy_port
* NAME
*	osm_hm_is_marked_healthy_port
*
* DESCRIPTION
*	Searches in healthy_port_tbl for a healthy port with input parameters.
*	If found, return TRUE. Otherwise, FALSE.
*
* SYNOPSIS
*/
boolean_t osm_hm_is_marked_healthy_port(IN struct osm_sm * sm,
					IN ib_net64_t peer_guid,
					IN const uint8_t peer_port_num);
/*
* PARAMETERS
*	sm
*		[in] Pointer to a subnet manager object
*
*	peer_guid
*		Peer guid
*
*	peer_port_num
*		Peer port number
*
* RETURN VALUE
*	TRUE if healthy port with input parameters appears in healthy_port_tbl.
*	Otherwise, FALSE.
*
* NOTES
* 	healthy_port_tbl is used to indicate ports that are marked as 'healthy' on
* 	unhealthy configuration file.
*
* SEE ALSO
* 	healthy_port_tbl
*
*********/

/****f* OpenSM: Unhealthy ports/osm_hm_clear_marked_healthy_ports
* NAME
*	osm_hm_clear_marked_healthy_ports
*
* DESCRIPTION
*	Clear healthy ports table
*
* SYNOPSIS
*/
void osm_hm_clear_marked_healthy_ports(IN struct osm_sm * sm);
/*
* PARAMETERS
*	sm
*		[in] Pointer to a subnet manager object
*
* RETURN VALUE
* 	None
*
* NOTES
* 	healthy_port_tbl is used to indicate ports that are marked as 'healthy' on
* 	unhealthy configuration file.
*
* SEE ALSO
* 	healthy_port_tbl
*
*********/

/****f* OpenSM: Unhealthy ports/osm_hm_is_unhealthy_by_sw_decision
* NAME
*	osm_hm_is_unhealthy_by_sw_decision
*
* DESCRIPTION
*	Indicated whether the switch is marked as unhealthy due to switch decision.
*
* SYNOPSIS
*/
boolean_t osm_hm_is_unhealthy_by_sw_decision(IN struct osm_sm * sm,
					     IN ib_net64_t peer_guid,
					     IN const uint8_t peer_port_num);
/*
* PARAMETERS
*	sm
*		Pointer to a subnet manager object
*
*	peer_guid
*		Peer guid
*
*	peer_port_num
*		Peer port number
*
* RETURN VALUE
*	Return TRUE if switch is marked as unhealthy due to switch decision,
*	as indicated by MEPI.unhealthy_reason received from the switch.
*	Otherwise, return FALSE.
*
* NOTES
*
* SEE ALSO
*
*********/

END_C_DECLS
