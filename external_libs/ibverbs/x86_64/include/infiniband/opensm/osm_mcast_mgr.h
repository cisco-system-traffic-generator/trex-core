/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2009 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009-2011 ZIH, TU Dresden, Federal Republic of Germany. All rights reserved.
 * Copyright (C) 2012-2013 Tokyo Institute of Technology. All rights reserved.
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
 * 	Declaration of osm_mcast_work_obj_t.
 * 	Provide access to a mcast function which searches the root switch for
 * 	a spanning tree.
 */

#pragma once

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

typedef struct osm_mcast_work_obj {
	cl_list_item_t list_item;
	osm_port_t *p_port;
	cl_map_item_t map_item;
	uint8_t is_receiver;
	uint16_t plane_mask_receivers;
	uint16_t plane_mask_senders;
} osm_mcast_work_obj_t;

int osm_mcast_make_port_list_and_map(osm_subn_t * p_subn, cl_qlist_t * list, cl_qmap_t * map,
				     osm_mgrp_box_t * mbox,
				     boolean_t *planarized_system_nodes_joined);

void osm_mcast_drop_port_list(cl_qlist_t * list);

osm_switch_t * osm_mcast_mgr_find_root_switch(osm_subn_t * p_subn,
					      cl_qlist_t * list,
					      osm_mgrp_box_t * mbox);

ib_api_status_t osm_mcast_mgr_build_clf_stree(osm_subn_t * p_subn,
					      osm_mgrp_box_t * mbox);

ib_api_status_t osm_mcast_mgr_build_spanning_tree(osm_subn_t * p_subn,
						  osm_mgrp_box_t * mbox);

ib_api_status_t osm_mcast_mgr_build_spanning_tree_const(osm_subn_t * p_subn,
							osm_mgrp_box_t * mbox);

void osm_mcast_mgr_clear_mft(osm_subn_t * p_subn, uint16_t mlid);

ib_api_status_t osm_mcast_mgr_alloc_mfts(osm_subn_t * p_subn, uint16_t max_mlid);

static inline void mcast_mgr_decrease_mtree_root_weight(osm_subn_t *p_subn,
							osm_mgrp_box_t * mbox)
{
	osm_switch_t *p_sw;
	if (mbox->root && mbox->root_guid) {
		p_sw = osm_get_switch_by_guid(p_subn,
					      cl_hton64(mbox->root_guid));
		if (p_sw) {
			OSM_ASSERT(p_sw->mlid_root_num);
			p_sw->mlid_root_num--;
		}
	}
}

static inline void mcast_mgr_increase_mtree_root_weight(osm_subn_t *p_subn,
							osm_mgrp_box_t * mbox)
{
	if (mbox->root) {
		OSM_ASSERT(mbox->root->p_sw);
		mbox->root_guid =
		    osm_node_get_node_guid_ho(mbox->root->p_sw->p_node);
		mbox->root->p_sw->mlid_root_num++;
	}
}

int osm_mcast_mgr_process(osm_sm_t * sm, boolean_t config_all);

void osm_mcast_mgr_decrease_weights(IN osm_subn_t * p_subn,
				    IN osm_mgrp_box_t * mbox);

void osm_mcast_mgr_increase_weights(IN osm_subn_t * p_subn,
				    IN osm_mgrp_box_t * mbox);


static inline void osm_mtree_node_set_plane(IN OUT osm_mtree_node_t *p_mtn,
					    IN uint8_t port_num,
					    IN osm_mcast_work_obj_t *p_wobj) {
	p_mtn->plane_mask_receivers[port_num + 1] = p_wobj->plane_mask_receivers;
	p_mtn->plane_mask_senders[port_num + 1] = p_wobj->plane_mask_senders;
}

/*
 * Function receives end port and returnes what planes portwould be represented with
 * in the multicast tree.
 */
uint8_t osm_mcast_mgr_get_member_plane(IN osm_subn_t *p_subn, IN osm_port_t *p_port);

END_C_DECLS
