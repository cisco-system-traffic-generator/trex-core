/*
 * Copyright (c) 2012 Mellanox Technologies LTD. All rights reserved.
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

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_list.h>
#include <opensm/osm_inform.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

typedef enum _osm_inform_mgr_state {
	OSM_INFORM_MGR_STATE_INIT = 0,
	OSM_INFORM_MGR_STATE_READY
} osm_inform_mgr_state_t;

typedef struct osm_inform_mgr {
	cl_spinlock_t traps_lock;
	cl_spinlock_t reports_lock;
	osm_thread_state_t thread_state;
	osm_inform_mgr_state_t state;
	cl_event_t signal;
	cl_thread_t poller;
	osm_sa_t *p_sa;
	osm_log_t *p_log;
} osm_inform_mgr_t;

typedef struct osm_inform_work_obj {
	cl_fmap_item_t map_item;
	cl_list_item_t list_item;
	struct osm_inform_work_obj *p_obj;
	ib_mad_notice_attr_t ntc;
	uint64_t tid;
	uint64_t trap_tid;
} osm_inform_work_obj_t;

osm_inform_work_obj_t *osm_inform_work_obj_new(IN ib_mad_notice_attr_t *p_ntc,
					       IN uint64_t tid,
					       IN uint64_t trap_tid);

void osm_inform_work_obj_delete(IN osm_inform_work_obj_t * p_wobj);

int osm_queue_trap(IN osm_sa_t *sa, IN ib_mad_notice_attr_t *p_ntc,
		   IN uint64_t tid, IN uint64_t trap_tid);
void osm_flush_traps(IN osm_subn_t *p_subn);

void osm_queue_report(IN osm_sa_t *sa, IN osm_madw_t *p_report_madw);
void osm_flush_reports(IN osm_subn_t *p_subn);

void osm_inform_mgr_send_signal(IN osm_sa_t *sa);
void osm_inform_mgr_queue_signal(IN osm_sm_t *sm);

struct osm_inform_mgr *osm_inform_mgr_construct();
ib_api_status_t osm_inform_mgr_init(IN struct osm_inform_mgr *p_inform_mgr,
				    IN osm_sa_t *p_sa, IN osm_log_t *p_log);
void osm_inform_mgr_destroy(IN struct osm_inform_mgr *p_inform_mgr);
void osm_inform_mgr_shutdown(IN struct osm_inform_mgr *p_inform_mgr,
			     IN osm_subn_t *p_subn);

END_C_DECLS
