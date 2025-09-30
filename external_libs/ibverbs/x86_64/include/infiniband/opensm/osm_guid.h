/*
 * Copyright (c) 2011 Mellanox Technologies LTD. All rights reserved.
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
#include <opensm/osm_sa.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

#define OSM_UNIQ_CNT_FUNC      "uniq_count"
#define OSM_PHYS_BAS_FUNC      "base_port"

typedef struct osm_guidinfo_work_obj {
	cl_list_item_t list_item;
	osm_port_t *p_port;
	uint16_t vport_index;
	uint8_t block_num;
	boolean_t force_client_rereg;
	boolean_t req_dirty;
} osm_guidinfo_work_obj_t;

osm_guidinfo_work_obj_t *osm_guid_work_obj_new(IN osm_port_t * p_port,  IN uint16_t vport_index,
					       IN uint8_t block_num, boolean_t force_client_rereg);

void osm_guid_work_obj_delete(IN osm_guidinfo_work_obj_t * p_wobj);

int osm_queue_guidinfo(IN osm_sa_t *sa, IN osm_port_t *p_port,
		       IN uint16_t vport_index,
		       IN uint8_t block_num, IN boolean_t force_client_rereg);

void osm_guid_mgr_process_vports(IN osm_sm_t * sm, IN osm_port_t * p_port);
END_C_DECLS
