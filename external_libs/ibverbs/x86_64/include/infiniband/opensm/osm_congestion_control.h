/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2012 Lawrence Livermore National Lab.  All rights reserved.
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
 *    OSM Congestion Control types and prototypes
 *
 * Author:
 *    Albert Chu, LLNL
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>
//#include <opensm/osm_sm.h>
#include <opensm/osm_base.h>
#include <opensm/osm_mads_engine.h>
#include <opensm/osm_port_group.h>

/****s* OpenSM: Base/OSM_DEFAULT_CC_KEY
 * NAME
 *       OSM_DEFAULT_CC_KEY
 *
 * DESCRIPTION
 *       Congestion Control Key used by OpenSM.
 *
 * SYNOPSIS
 */
#define OSM_DEFAULT_CC_KEY 0

#define OSM_CC_DEFAULT_MAX_OUTSTANDING_QUERIES 500

#define OSM_CC_TIMEOUT_COUNT_THRESHOLD 3

#define CC_MAX_PROFILES_NAME_LEN 	32
#define CC_MAX_CA_ALGO_NAME_LEN 	32
#define CC_MAX_CA_ALGO_PARAM_NAME_LEN 	256
#define PPCC_ALGO_MAX_PARAMS		44
#define PPCC_MAX_PARAM_NAME_LEN 	256
#define PPCC_ALGO_MAX_FILE_NAME 	256
#define PPCC_MAX_CA_ALGOS		16

/****s* OpenSM: CongestionControl/osm_congestion_control_t
*  This object should be treated as opaque and should
*  be manipulated only through the provided functions.
*/
struct osm_sm;

typedef struct osm_congestion_control {
	struct osm_opensm *osm;
	osm_subn_t *subn;
	struct osm_sm *sm;
	osm_log_t *log;
	atomic32_t trans_id;
	cl_disp_reg_handle_t cc_disp_h;
	cl_disp_reg_handle_t cc_trap_disp_h;
	ib_net64_t port_guid;
        osm_mads_engine_t mads_engine;
	ib_sw_cong_setting_t sw_cong_setting;
	ib_ca_cong_setting_t ca_cong_setting;
	ib_cc_tbl_t cc_tbl[OSM_CCT_ENTRY_MAD_BLOCKS];
	unsigned int cc_tbl_mads;
	boolean_t cc_key_param_update;
	unsigned int num_configured;
} osm_congestion_control_t;
/*
* FIELDS
*       subn
*             Subnet object for this subnet.
*
*       log
*             Pointer to the log object.
*
*       mad_pool
*             Pointer to the MAD pool.
*
*       mad_ctrl
*             Mad Controller
*
*	cc_key_param_update
*		Boolean indicate whether should update CC key parameters by
*		KeyInfo MAD.
*
*	num_configured
*		Number of devices which CC keys were configured successfully.
*
*********/

#define OSM_CC_MAX_PROFILE_NUM IB_CC_PORT_PROFILES_BLOCK_SIZE

typedef enum _osm_cc_type_enum {
	OSM_CC_V0_CC = 0,
	OSM_CC_V1_PPCC
} osm_cc_type_enum;
/*
* FIELDS
*       OSM_CC_V0_CC
*		Version 0 of Congestion Control attributes.
*
*       OSM_CC_V1_PPCC
*		Version 1 of Congestion Control attributes. (PPCC)
*
*********/

typedef enum _osm_cc_group_type_enum {
	OSM_CC_GROUP_NOT_DEFINED = 0,
	OSM_CC_GROUP_ALL_SWITCHES,
	OSM_CC_GROUP_ALL_CAS,
	OSM_CC_GROUP_PORT_GROUP,
	OSM_CC_GROUP_SWITCH_PORTS
} osm_cc_group_type_enum;

osm_cc_group_type_enum osm_cc_get_group_type(IN const char *str);

typedef struct osm_cc_mgr {
	cl_fmap_t cc_profiles_map;
	cl_qlist_t sl_profiles_conf;
	cl_qlist_t vl_profiles_conf;
	cl_qlist_t sw_conf;
	cl_qlist_t ca_conf;
	cl_fmap_t ca_algo_conf;
	struct osm_ppcc_ca_algos_import_conf *ca_algo_import_conf;
	cl_fmap_t algo_version_to_id_map;
	ib_ppcc_hca_config_param_t algo_id_params_mad_map[PPCC_MAX_CA_ALGOS + 1];
} osm_cc_mgr_t;

typedef struct osm_cc_profile {
	uint8_t percent;
	uint64_t min;
	uint64_t max;
} osm_cc_profile_t;

typedef struct osm_cc_sw_conf {
	cl_list_item_t list_item;
	osm_cc_group_type_enum group_type;
	boolean_t enable;
	uint8_t aqs_weight;
	uint8_t aqs_time;
} osm_cc_sw_conf_t;

typedef struct osm_cc_hca_rp_params_conf {
	boolean_t enable_clamp_tgt_rate;
	boolean_t enable_clamp_tgt_rate_after_time_inc;
	uint32_t rpg_time_reset;
	uint32_t rpg_byte_reset;
	uint8_t  rpg_threshold;
	uint32_t rpg_max_rate;
	uint16_t rpg_ai_rate;
	uint16_t rpg_hai_rate;
	uint8_t rpg_gd;
	uint8_t rpg_min_dec_fac;
	uint32_t rpg_min_rate;
	uint32_t rate_to_set_on_first_cnp;
	uint16_t dce_tcp_g;
	uint32_t dce_tcp_rtt;
	uint32_t rate_reduce_monitor_period;
	uint16_t initial_alpha_value;
} osm_cc_hca_rp_params_conf_t;

typedef struct osm_cc_hca_np_params_conf {
	uint16_t min_time_between_cnps;
	boolean_t cnp_sl_mode;
	uint8_t cnp_sl;
	boolean_t rtt_sl_mode;
	uint8_t rtt_resp_sl;
} osm_cc_hca_np_params_conf_t;

typedef struct osm_cc_algo_param_ {
	char param_name[CC_MAX_CA_ALGO_PARAM_NAME_LEN];
	uint32_t value;
	uint32_t min_value;
	uint32_t max_value;
} osm_cc_algo_param_t;

typedef struct osm_ppcc_dictionary_entry_ {
	char str[PPCC_MAX_PARAM_NAME_LEN];
	uint32_t value;
} osm_ppcc_dictionary_entry_t;

typedef struct osm_ppcc_algo_id_ {
	cl_fmap_item_t fmap_item;
	ib_ppcc_algo_info_t algo_version;
	int algo_id;
} osm_ppcc_algo_id_map_item_t;

typedef struct osm_cc_algo_param_index_ {
	cl_fmap_item_t fmap_item;
	char param_name[CC_MAX_CA_ALGO_PARAM_NAME_LEN];
	int param_index;
} osm_cc_algo_param_index_t;

typedef struct osm_cc_ca_algo_conf {
	cl_fmap_item_t fmap_item;
	char file_name[PPCC_ALGO_MAX_FILE_NAME];
	char name[CC_MAX_CA_ALGO_NAME_LEN];
	ib_ppcc_algo_info_t algo_version;
	osm_cc_algo_param_t algo_param_list[PPCC_ALGO_MAX_PARAMS];
	cl_fmap_t param_name_to_index;
	int param_num;
} osm_cc_ca_algo_conf_t;

typedef struct osm_ppcc_ca_algo_ {
	cl_map_item_t map_item;
	int algo_id;
	char file_name[PPCC_ALGO_MAX_FILE_NAME];
	osm_ppcc_dictionary_entry_t *param_list[PPCC_ALGO_MAX_PARAMS];
} osm_ppcc_ca_algo_t;


typedef struct osm_ppcc_ca_algos_import_conf {
	cl_qmap_t ca_algos;
	int algo_num;
	osm_ppcc_ca_algo_t *p_current_ca_algo;
} osm_ppcc_ca_algos_import_conf_t;

typedef struct osm_ppcc_ca_conf {
	uint8_t algo_sl_map[IB_NUMBER_OF_SLS];
} osm_ppcc_ca_conf_t;

typedef struct osm_cc_ca_conf {
	cl_list_item_t list_item;
	osm_cc_group_type_enum group_type;
	osm_port_group_t *port_group;
	osm_cc_type_enum cc_type;
	boolean_t enable_notify;
	boolean_t enable_react;
	boolean_t enable_per_plane;
	osm_cc_hca_rp_params_conf_t hca_rp_params;
	osm_cc_hca_np_params_conf_t hca_np_params;
	osm_ppcc_ca_conf_t ppcc_ca_conf;
} osm_cc_ca_conf_t;

typedef struct osm_cc_sl_conf {
	cl_list_item_t list_item;
	osm_cc_group_type_enum group_type;
	uint8_t sl2profile_num[IB_NUMBER_OF_SLS];
} osm_cc_sl_conf_t;

typedef struct osm_cc_profiles_conf {
	cl_fmap_item_t fmap_item;
	char name[CC_MAX_PROFILES_NAME_LEN];
	uint8_t mode;
	osm_cc_profile_t profiles[OSM_CC_MAX_PROFILE_NUM];
} osm_cc_profiles_conf_t;

typedef struct osm_cc_vl_conf {
	cl_list_item_t list_item;
	osm_cc_group_type_enum group_type;
	osm_cc_profiles_conf_t* vl_to_profiles[IB_MAX_NUM_VLS];
} osm_cc_vl_conf_t;

static inline int ppcc_algo_compare_param_names(IN const void * p_param_name_1, IN const void * p_param_name_2)
{
	return strcmp((const char *)p_param_name_1, (const char *)p_param_name_2);
}
static inline int ppcc_algo_compare_strings(IN const void * p_str_1, IN const void * p_str_2)
{
	return strcmp((const char *)p_str_1, (const char *)p_str_2);
}

void osm_cc_mgr_construct(IN osm_cc_mgr_t * p_mgr);

void osm_cc_destroy_ppcc_ca_algo(osm_ppcc_ca_algo_t **pp_ppcc_ca_algo);
void osm_cc_mgr_destroy(IN osm_cc_mgr_t * p_mgr);

struct osm_opensm;

int osm_congestion_control_setup(struct osm_opensm *osm, boolean_t reload_configuration);

int osm_congestion_control_wait_pending_transactions(struct osm_opensm *osm);

ib_api_status_t osm_congestion_control_init(osm_congestion_control_t * p_cc,
					    struct osm_opensm *osm,
					    const osm_subn_opt_t * p_opt);

ib_api_status_t osm_congestion_control_bind(osm_congestion_control_t * p_cc,
					    ib_net64_t port_guid);

void osm_congestion_control_invalidate(osm_congestion_control_t * p_cc);

void osm_congestion_control_shutdown(osm_congestion_control_t * p_cc);

void osm_congestion_control_destroy(osm_congestion_control_t * p_cc);
