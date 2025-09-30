/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <vendor/osm_vendor_api.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>
#include <opensm/osm_base.h>
#include <opensm/osm_mad_pool.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

struct osm_sm;

typedef struct osm_mads_engine_ {
	osm_opensm_t *p_osm;
	osm_mad_pool_t *mad_pool;
	osm_vendor_t *vendor[OSM_MAX_BINDING_PORTS];
	osm_bind_handle_t bind_handle[OSM_MAX_BINDING_PORTS];
	uint8_t bind_port_index;
	atomic32_t outstanding_mads;
	atomic32_t outstanding_mads_on_wire;
	uint32_t max_outstanding_mads_on_wire;
	cl_qlist_t mad_queue;
	cl_spinlock_t mad_queue_lock;
	cl_event_t outstanding_mads_done_event;
	cl_event_t sig_mads_on_wire_continue;
	cl_event_t poller_wakeup;
	cl_thread_t poller;
	osm_thread_state_t thread_state;
	uint8_t mgmt_class;
	char *class_name;
	void (*send_mad_failure_cb)(osm_madw_context_t *p_mad_context, osm_log_t *p_log);
	void (*dump_mad)(osm_madw_t *p_madw, osm_log_t *p_log);
	void (*statistic_counter_increase)(osm_opensm_t *p_osm, ib_net16_t attr_id);
} osm_mads_engine_t;

int osm_mads_engine_wait_pending_transactions(osm_mads_engine_t *p_mads_engine);

void osm_mads_engine_decrement_outstanding_mads(osm_mads_engine_t *p_mads_engine);

void osm_mads_engine_post_mad(osm_mads_engine_t *p_mads_engine, osm_madw_t *p_madw);

void osm_mads_engine_post_trap_repress(osm_mads_engine_t *p_mads_engine, osm_madw_t *resp_madw);

cl_status_t osm_mads_engine_init(osm_mads_engine_t *p_mads_engine, IN osm_opensm_t *p_osm,
				 IN osm_mad_pool_t *p_mad_pool,
				 IN const char *const class_name);

void osm_mads_engine_destroy(osm_mads_engine_t *mads_engine);

void osm_mads_engine_destroy_binds(osm_mads_engine_t *p_mads_engine);

ib_api_status_t osm_mads_engine_main_bind(osm_mads_engine_t *p_mads_engine,
					  IN osm_bind_info_t * const p_bind_info,
					  IN osm_vend_mad_recv_callback_t mad_recv_callback,
					  IN osm_vend_mad_send_err_callback_t send_err_callback,
					  IN void *context);

static inline void osm_mads_engine_main_unbind(osm_mads_engine_t *p_mads_engine)
{
	osm_vendor_unbind(p_mads_engine->bind_handle[p_mads_engine->bind_port_index]);
}

static inline osm_madw_t *osm_mads_engine_get_mad(osm_mads_engine_t *p_mads_engine,
					          IN uint32_t total_size,
					          IN const osm_mad_addr_t *p_mad_addr)
{
	return osm_mad_pool_get(p_mads_engine->mad_pool,
				p_mads_engine->bind_handle[p_mads_engine->bind_port_index],
				total_size, p_mad_addr);
}

static inline void osm_mads_engine_put_mad(osm_mads_engine_t *p_mads_engine, osm_madw_t *p_madw)
{
	osm_mad_pool_put(p_mads_engine->mad_pool, p_madw);
}

void osm_mads_engine_shutdown(osm_mads_engine_t *p_mads_engine);

END_C_DECLS
