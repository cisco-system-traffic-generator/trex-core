/*
 * Copyright (c) 2017 Mellanox Technologies LTD. All rights reserved.
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
 *      Event Manager
 */

#pragma once

#include<arpa/inet.h>
#include<sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <complib/cl_dispatcher.h>
#include <iba/ib_types.h>

struct osm_sm;
struct osm_log;

#define SUBJECTS_NUM 5
#define OSM_ACTIVITY_SUBJECTS_NONE "none"
#define OSM_ACTIVITY_SUBJECTS_ALL "all"
#define NAME_SIZE 128
#define NUM_OF_EVENTS 16

typedef enum osm_activity_mgr_event_id {
	OSM_ACT_MGR_MCM_MEMBER,
	OSM_ACT_MGR_MCG_CHANGE,
	OSM_ACT_MGR_PRTN_ADD_GUID,
	OSM_ACT_MGR_PRTN_DEL_GUID,
	OSM_ACT_MGR_PRTN_CREATE,
	OSM_ACT_MGR_PRTN_DELETE,
	OSM_ACT_MGR_VIRT_DISCOVER,
	OSM_ACT_MGR_VPORT_STATE_CHANGE,
	OSM_ACT_MGR_MCG_TREE_CALC,
	OSM_ACT_MGR_ROUTE_SUCCESS,
	OSM_ACT_MGR_ROUTE_FAIL,
	OSM_ACT_MGR_UCAST_CACHE_INVAL,
	OSM_ACT_MGR_UCAST_CACHE_USED,
	OSM_ACT_MGR_MCM_EXCEEDED_MAX,
	OSM_ACT_MGR_SVCR_EXCEEDED_MAX,
	OSM_ACT_MGR_EVENT_SUBS_EXCEEDED_MAX
} osm_activity_mgr_event_id_enum;

typedef enum osm_act_mgr_mc_state {
	OSM_ACT_MGR_MC_LEAVE = -1,
	OSM_ACT_MGR_MC_JOIN = 1
} osm_act_mgr_mc_state_enum;

typedef enum osm_activity_mgr_prtn_del_reason {
	OSM_ACT_MGR_PRTN_EMPTY = 0,
	OSM_ACT_MGR_PRTN_DUPLICATE,
	OSM_ACT_MGR_PRTN_SHUTDOWN
} osm_activity_mgr_prtn_del_reason_enum;

typedef enum osm_activity_mgr_change {
	OSM_ACT_MGR_CREATE = 0,
	OSM_ACT_MGR_DELETE
} osm_activity_mgr_change_enum;

typedef enum osm_activity_mgr_state {
	OSM_ACT_MGR_STATE_DOWN = 1,
	OSM_ACT_MGR_STATE_INIT,
	OSM_ACT_MGR_STATE_ARMED, /* Not used for vports */
	OSM_ACT_MGR_STATE_ACTIVE
} osm_activity_mgr_state_enum;

typedef enum subject_id {
	MC_SUBJ_ID = 0,
	PRTN_SUBJ_ID,
	VIRT_SUBJ_ID,
	ROUTE_SUBJ_ID,
	EXCEEDS_MAX_SUBJ_ID
} subject_id_t;

/****s* EventMgr/osm_activity_mgr_t
* NAME
*  osm_activity_mgr_t
*
* DESCRIPTION
*	This type represents an event manager.
*
*	The manager responsible for handling events reported by the SM.
*
* SYNOPSIS
*/
typedef struct event_data {
	osm_activity_mgr_event_id_enum event_id;
	ib_net16_t mlid;
	char mgid[NAME_SIZE];
	ib_net64_t port_guid;
	ib_net64_t vport_guid;
	ib_net64_t vnode_guid;
	ib_net16_t pkey;
	char name[NAME_SIZE];
	uint16_t block_index;
	uint8_t pkey_index;
	ib_net16_t top_index;
	uint16_t vport_index;
	uint32_t sa_req_exceeding_counter;
	osm_activity_mgr_state_enum state;
	osm_act_mgr_mc_state_enum join_state;
	osm_activity_mgr_change_enum change;
	osm_activity_mgr_prtn_del_reason_enum del_reason;
} event_data_t;

/****s* EventMgr/osm_activity_mgr_t
* NAME
*  osm_activity_mgr_t
*
* DESCRIPTION
*	This type represents an event manager.
*
*	The manager responsible for handling events reported by the SM.
*
* SYNOPSIS
*/
typedef struct osm_activity_mgr {
	cl_dispatcher_t disp;
	cl_disp_reg_handle_t handlers[NUM_OF_EVENTS];
	struct osm_log *p_log;
	uint8_t *p_sm_state;
	pid_t pid;
	boolean_t enabled_subjects[SUBJECTS_NUM];
	FILE *file;
	boolean_t enabled;
} osm_activity_mgr_t;

/****f* EventMgr: EventMgr/osm_activity_mgr_init
* NAME osm_activity_mgr_init
*
* DESCRIPTION
*	Init event manager
*
* SYNOPSIS
*
*/
ib_api_status_t osm_activity_mgr_init(osm_activity_mgr_t *p_mgr, struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_mgr
*		Pointer to event manager object
*
*	p_sm
*		Pointer to sm object
* RETURN VALUE
*	Init Status
*/

/****f* EventMgr: EventMgr/enable_activity_subjects
* NAME enable_activity_subjects
*
* DESCRIPTION
*	Set the subjects to report activities for (according to configuration)
*
* SYNOPSIS
*
*/
void enable_activity_subjects(osm_activity_mgr_t *p_mgr, struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_mgr
*		Pointer to event manager object
*
*	p_sm
*		Pointer to sm object
*
* RETURN VALUE
*	N/A
*/

/****f* EventMgr: EventMgr/osm_activity_mgr_destroy
* NAME osm_activity_mgr_destroy
*
* DESCRIPTION
*	Destroy event manager
*
* SYNOPSIS
*
*/
void osm_activity_mgr_destroy(osm_activity_mgr_t *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to event manager object
* RETURN VALUE
*	N/A
*/

/****f* EventMgr: EventMgr/osm_act_mgr_subj_enabled
* NAME osm_act_mgr_subj_enabled
*
* DESCRIPTION
*	Check if specific subject is enabled in activity manager
*
* SYNOPSIS
*
*/
static inline boolean_t osm_act_mgr_subj_enabled(osm_activity_mgr_t *p_mgr,
						 subject_id_t id)
{
	return (p_mgr->enabled && p_mgr->enabled_subjects[id]);
}

/*
* PARAMETERS
*	p_mgr
*		Pointer to event manager
*
*	id
*		subject id to check
*
* RETURN VALUE
*	Boolean indication whether requested subject is enabled
*/

/****f* EventMgr: EventMgr/osm_act_mgr_post_event
* NAME osm_act_mgr_post_event
*
* DESCRIPTION
*	post activity manager event
*
* SYNOPSIS
*
*/
void osm_act_mgr_post_event(osm_activity_mgr_t *p_mgr, event_data_t *p_data);
/*
* PARAMETERS
*	p_mgr
*		Pointer to event manager
*
*	p_data
*		event_data_t object which holds all event data
*
* RETURN VALUE
*	None
*/


