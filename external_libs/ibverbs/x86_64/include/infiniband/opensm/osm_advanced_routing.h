/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2017-2020 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_advanced_routing_data_t.
 *
 *	This object is part of the OpenSM family of objects.
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

/****s* OpenSM: Switch/osm_advanced_routing_data_t
* NAME
*	osm_advanced_routing_data_t
*
* DESCRIPTION
*	Adaptive Routing Data structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/


#define ADV_ROUTING_OPT_MODE_DISABLED		0
#define ADV_ROUTING_OPT_MODE_ENABLED		1
#define ADV_ROUTING_OPT_MODE_RN			2
#define AR_STATUS_SUCCESS			1
#define AR_STATUS_FAILURE			0

#define AR_MAX_NUM_PLFT 4

#define ADV_ROUTING_ALL_PLFTS			0xff
#define AR_INVALID_GROUP			0xff
#define AR_INVALID_GROUP_ID			0xffff

typedef enum OSM_AR_TABLE_TYPE {
	OSM_AR_CALC_TABLE = 0,
	OSM_AR_SUBNET_TABLE
} osm_ar_table_type_t;

typedef enum osm_ar_info_type_ {
	OSM_AR_INFO_REQ = 0,
	OSM_AR_INFO_SUBNET
} osm_ar_info_type_t;

/*
 * Forward declarations
 */
struct osm_sm;
struct osm_switch;

typedef union osm_ar_subgroup {
	uint8_t mask[32];
	uint64_t mask64[4];
} osm_ar_subgroup_t;
/*
* FIELDS
*/

typedef enum osm_ar_group_set_type {
	OSM_AR_GROUP_SET_TYPE_OVERRIDE = 0,
	OSM_AR_GROUP_SET_TYPE_ADD
} osm_ar_group_set_type_t;

typedef struct osm_ar_capability {
	cl_list_item_t list_item;
	ib_ar_info_t ar_info;
} osm_ar_capability_t;
/*
* FIELDS
*/


#define IB_RN_XMIT_PORT_MASK_BLOCK_ELEMENTS_NUMBER ((IB_RN_XMIT_PORT_MASK_BLOCK_SIZE) * 2)

typedef struct osm_ar_tables {
	osm_ar_subgroup_t *ar_subgroups;
	ib_ar_lft_entry_t* ar_lft[AR_MAX_NUM_PLFT];
	uint8_t *rn_subgroup_direction;
	ib_net16_t *rn_direction_string;
	ib_rn_rcv_string_entry_t *rn_rcv_strings;
	ib_net32_t rn_subgroup_priority_flags[IB_RN_GEN_BY_SUB_GROUP_PRIORITY_BLOCK_SIZE];
	uint8_t rn_xmit_port_mask[IB_RN_XMIT_PORT_MASK_BLOCK_ELEMENTS_NUMBER];
	ib_plft_def_element_t *plft_defs;
	ib_port_sl_to_plft_map_entry_t *port_sl_plfts;
	uint16_t plft_top[AR_MAX_NUM_PLFT];
	ib_ar_group_hbf_weight_t *hbf_subgroup_weights;
} osm_ar_tables_t;

typedef struct osm_ar_group_id {
	boolean_t is_group_empty;
	int id;
} osm_ar_group_id_t;

typedef struct osm_advanced_routing_data {
	uint8_t status;
	uint64_t pending_mads;
	ib_ar_info_t ar_info;
	ib_ar_info_t req_ar_info;
	ib_plft_info_t plft_info;
	ib_plft_info_t req_plft_info;
	uint16_t num_groups;
	uint8_t num_tables;
	uint8_t num_group_subgroups;
	uint16_t num_lids;
	uint16_t num_strings;
	uint16_t num_directions;
	uint8_t num_plfts;
	uint8_t req_num_plfts;
	uint16_t group_top;
	osm_ar_tables_t subn_tables;
	osm_ar_tables_t calc_tables;
	cl_qlist_t cached_capabilities_list;
	uint16_t *ar_group_copy_from_table;
	boolean_t ar_info_updated;
	boolean_t plft_info_updated;
	ib_hbf_config_t hbf_config;
	ib_hbf_config_t req_hbf_config;
	ib_pfrn_config_t pfrn_config;
	ib_pfrn_config_t req_pfrn_config;
	boolean_t pfrn_config_updated;
	osm_ar_group_id_t *group_ids;
} osm_advanced_routing_data_t;
/*
* FIELDS
*/

enum OSM_AR_LID_STATE {
    OSM_AR_IB_LID_STATE_BOUNDED = 0x0,
    OSM_AR_IB_LID_STATE_FREE = 0x1,
    OSM_AR_IB_LID_STATE_STATIC = 0x2,
    OSM_AR_IB_LID_STATE_HBF = 0x3,
    OSM_AR_IB_LID_STATE_LAST
};

static inline const char *osm_ar_get_port_state_str(uint8_t state) {
	if (state == OSM_AR_IB_LID_STATE_BOUNDED)
		return "Bounded";
	if (state == OSM_AR_IB_LID_STATE_FREE)
		return "Free";
	if (state == OSM_AR_IB_LID_STATE_STATIC)
		return "Static";
	if (state == OSM_AR_IB_LID_STATE_HBF)
		return "HBF";
	return "??";
}
const char *get_adv_routing_name_from_type(osm_adv_routing_type_enum adv_routing_type);

osm_adv_routing_type_enum get_adv_routing_type_from_name(const char *ar_name);

static inline osm_adv_routing_type_enum
get_adv_routing_auto_type(struct osm_routing_engine *routing_engine)
{
	return routing_engine ? routing_engine->adv_routing_engine_auto :
		OSM_ADV_ROUTING_TYPE_NONE;
}


/* advanced routing data initialization */

void osm_ar_data_init(osm_advanced_routing_data_t *p_ar_data);

int osm_ar_prepare_tables(osm_subn_t *p_subn, osm_ar_info_type_t ar_info_type);

int osm_ar_prepare_sw_tables(osm_subn_t *p_subn, struct osm_switch *p_sw,
			     uint16_t num_lids, osm_ar_info_type_t ar_info_type);

void osm_ar_data_destroy(osm_advanced_routing_data_t *p_ar_data);


/* advanced routing data configuration and retrieval */

void osm_ar_set_required_ar_info(osm_advanced_routing_data_t *p_ar_data,
				 const ib_ar_info_t *p_ar_info);

int osm_ar_set_required_group_top(osm_advanced_routing_data_t *p_ar_data,
				  uint16_t group_top);

int osm_ar_set_required_ar_mode(osm_advanced_routing_data_t *p_ar_data,
				boolean_t enable_ar, boolean_t enable_fr);

int osm_ar_set_required_num_subgroups(osm_advanced_routing_data_t *p_ar_data,
				      uint8_t num_subgroups);

int osm_ar_set_required_enabled_sl_mask(osm_advanced_routing_data_t *p_ar_data,
					uint16_t sl_mask);

int osm_ar_set_required_enabled_transport_mask(osm_advanced_routing_data_t *p_ar_data,
					       uint8_t enabled_transport_mask);

void osm_ar_set_required_plft_number(osm_advanced_routing_data_t *p_ar_data,
				     uint8_t num_plfts);

void osm_ar_set_required_plft_active_mode(osm_advanced_routing_data_t *p_ar_data,
					  uint8_t active_mode);

void osm_ar_set_required_plft_top(osm_advanced_routing_data_t *p_ar_data, uint8_t plft_id, uint16_t top);

osm_ar_subgroup_t * osm_ar_subgroup_get(osm_advanced_routing_data_t *p_ar_data,
					uint16_t group_id, uint8_t subgroup_id);

void osm_ar_subgroup_add_port(osm_ar_subgroup_t *p_subgroup, uint8_t port);

void osm_ar_subgroup_remove_port(osm_ar_subgroup_t *p_subgroup, uint8_t port);

boolean_t osm_ar_subgroup_check_port(osm_ar_subgroup_t *p_subgroup, uint8_t port);

uint8_t osm_ar_get_first_port_set_in_subgroup(osm_ar_subgroup_t *p_subgroup);

void osm_ar_subgroup_clear(osm_ar_subgroup_t *p_subgroup);


void osm_ar_groups_clear_group(osm_advanced_routing_data_t *p_ar_data,
			       uint16_t group_id);

void osm_ar_groups_clear_tbl(osm_advanced_routing_data_t *p_ar_data);

void osm_ar_groups_clear_all(osm_advanced_routing_data_t *p_ar_data);

void osm_ar_groups_copy_group(osm_advanced_routing_data_t *p_ar_data,
			      uint16_t src_group_id, uint16_t dst_group_id,
			      boolean_t copy_direction);

void osm_ar_lft_set(osm_advanced_routing_data_t *p_ar_data, uint16_t lid,
		    uint8_t plft_idx,
		    uint8_t port, uint8_t state, uint16_t group_id);

void osm_ar_lft_update_port(osm_advanced_routing_data_t *p_ar_data, uint16_t lid,
			    uint8_t plft_id, uint8_t port);

void osm_ar_lft_copy(osm_advanced_routing_data_t *p_ar_data, uint8_t plft_id,
		     uint16_t copy_to_lid, uint16_t copy_from_lid);

void osm_ar_data_copy(osm_subn_t *p_to_subn, osm_subn_t *p_from_subn,
		      struct osm_switch *p_to_sw, struct osm_switch *p_from_sw);

void osm_ar_set_subgroup_direction(osm_advanced_routing_data_t *p_ar_data,
				   uint16_t group_id, uint8_t subgroup_id,
				   uint8_t direction);

uint8_t osm_ar_subgroup_direction_get(osm_advanced_routing_data_t *p_ar_data,
				      uint16_t group_id, uint8_t subgroup_id);

void osm_ar_subgroup_pri_rn_flags_set(osm_advanced_routing_data_t *p_ar_data,
				      uint8_t subgroup_priority,
				      uint8_t rn_flags);
void osm_ar_rn_direction_string_set(osm_advanced_routing_data_t *p_ar_data,
				    uint8_t direction, uint16_t string);
void osm_ar_plft_def_set(osm_advanced_routing_data_t *p_ar_data,
			 uint8_t plft_id, uint8_t offset,
			 uint8_t space, uint8_t bank,
			 uint8_t force_plft_sub_group0);
void osm_ar_rn_string_handling_set(osm_advanced_routing_data_t *p_ar_data,
				   uint16_t string, uint8_t decision,
				   uint16_t new_string,
				   uint8_t plft_id);

void osm_ar_rn_port_xmit_mask_add_port(osm_advanced_routing_data_t *p_ar_data,
				       uint8_t port, uint8_t mask);

void osm_ar_rn_port_xmit_mask_clear(osm_advanced_routing_data_t *p_ar_data);

ib_api_status_t osm_ar_get_ar_info(struct osm_sm *sm, struct osm_switch *p_sw);
ib_api_status_t osm_ar_query_ar_info(struct osm_sm *sm, struct osm_switch *p_sw,
				     ib_ar_info_t *p_ar_info);
ib_api_status_t osm_ar_set_ar_info(struct osm_sm *sm, struct osm_switch *p_sw,
				   ib_ar_info_t *p_ar_info);

void osm_ar_capabilities_clear_all(osm_advanced_routing_data_t *p_ar_data);

const ib_ar_info_t *
osm_ar_capabilities_search(osm_advanced_routing_data_t *p_ar_data,
			  const ib_ar_info_t *p_ar_info);
int osm_ar_capabilities_update(osm_advanced_routing_data_t *p_ar_data,
			      const ib_ar_info_t *p_ar_info);

ib_api_status_t osm_ar_get_pfrn_config(struct osm_sm * sm, struct osm_switch * p_sw);

/* subnet configuration */

/*
 * Updates ar_data.status only to reflect that all the required
 * AR MADs that were sent, were received.
 */
void osm_ar_update_status(struct osm_switch *p_sw);

int osm_ar_set_subn_ar_tables(struct osm_sm *sm, osm_node_filter_t filter);

/* handle received data */

int osm_ar_groups_set_block(osm_advanced_routing_data_t *p_ar_data,
			    uint16_t block_id, ib_ar_group_table_t *p_block,
			    boolean_t get_subnet_top);

int osm_ar_plft_defs_set_block(osm_advanced_routing_data_t *p_ar_data,
			       uint8_t block_id,
			       ib_plft_def_t *p_block);

int osm_ar_port_sl_to_plft_set_block(struct osm_switch *p_sw,
				     uint8_t block_id,
				     boolean_t subnet,
				     ib_port_sl_to_plft_map_t *p_block);

int osm_ar_plft_map_set_block(osm_advanced_routing_data_t *p_ar_data,
			      uint8_t plft_id, ib_plft_map_t *p_block);

int osm_ar_lft_set_block(osm_advanced_routing_data_t *p_ar_data,
			 uint8_t plft_id,
			 uint16_t block_id, ib_ar_lft_t *p_block);

int osm_ar_rn_gen_string_set_block(osm_advanced_routing_data_t *p_ar_data,
				   int16_t block_id,
				   ib_rn_gen_string_table_t *p_block);

void osm_ar_rn_string_handling_clear(osm_advanced_routing_data_t *p_ar_data);

int osm_ar_rn_rcv_string_set_block(osm_advanced_routing_data_t *p_ar_data,
				   int16_t block_id,
				   ib_rn_rcv_string_table_t *p_block);

int osm_ar_rn_sub_group_direction_set_block(osm_advanced_routing_data_t *p_ar_data,
					    int16_t block_id,
					    ib_rn_sub_group_direction_table_t *p_block);

int osm_ar_rn_xmit_port_mask_set_block(osm_advanced_routing_data_t *p_ar_data,
				       int16_t block_id,
				       ib_rn_xmit_port_mask_t *p_block);

int osm_ar_rn_subgroup_priority_flags_set_block(osm_advanced_routing_data_t *p_ar_data,
						ib_rn_gen_by_sub_group_priority_t *p_block);


/* compare calculated AR LFT data to assigned AR LFT data */
int osm_ar_cmp_ar_lft(osm_advanced_routing_data_t *p_ar_data);

/* reallocate LFT tables per all plfts according to the given number of lids */
int osm_ar_reallocate_lft_tables(osm_advanced_routing_data_t *p_ar_data,
				 uint16_t num_lids,
				 boolean_t clear_all_calculated);

ib_api_status_t osm_ar_get_plft_info(struct osm_sm *sm, struct osm_switch *p_sw);

/* Modify state of all ARLFT entries for specified destination lids to static */
void osm_ar_lft_set_static_state_for_dlids(osm_subn_t *p_subn,
					   osm_advanced_routing_data_t *p_ar_data,
					   uint16_t *lids, uint16_t num_lids);

/****f* OpenSM: advanced routing/osm_ar_invalidate_ar_cache_params
* NAME
*	osm_ar_invalidate_ar_cache_params
*
* DESCRIPTION
*	Invalidate AR cache params
*
* SYNOPSIS
*/
void osm_ar_invalidate_ar_cache_params(struct osm_switch *p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch.
*
* RETURN VALUE
*	None
*
* NOTES
*
* SEE ALSO
*********/

static inline
uint16_t osm_ar_info_is_ar_enabled(const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->enable_features & IB_AR_INFO_FEATURE_MASK_AR_ENABLED);
}

static inline
uint16_t osm_ar_info_is_fr_enabled(const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->enable_features & IB_AR_INFO_FEATURE_MASK_FR_ENABLED);
}

static inline
uint16_t osm_ar_info_is_rn_xmit_enabled(const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->enable_features & IB_AR_INFO_FEATURE_MASK_RN_XMIT_ENABLED);
}

static inline
uint8_t osm_ar_info_is_hbf_support(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->cap_fields1 & IB_AR_INFO_HBF_CAP_HBF_SUP;
}

static inline
uint8_t osm_ar_info_is_nvl_hbf_supported(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->cap_fields1 & IB_AR_INFO_CAP_FEILDS_NVL_HBF_SUPPORTED;
}

static inline
uint8_t osm_ar_info_is_hbf_bthdqp_hash_support(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->cap_fields1 & IB_AR_INFO_HBF_CAP_BTHDQP_HASH_SUP;
}

static inline
uint8_t osm_ar_info_is_hbf_dceth_hash_support(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->cap_fields1 & IB_AR_INFO_HBF_CAP_DCETH_HASH_SUP;
}

static inline
uint8_t osm_ar_info_is_whbf_support(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->cap_fields1 & IB_AR_INFO_HBF_CAP_WHBF_SUP;
}

static inline
uint8_t osm_ar_info_is_hbf_enabled(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->enable_features1 & IB_AR_INFO_HBF_EN_BY_SL_HBF_EN;
}

static inline
uint8_t osm_ar_info_is_whbf_enabled(IN ib_ar_info_t * const p_ar_info)
{
	return p_ar_info->enable_features1 & IB_AR_INFO_HBF_EN_WHBF_EN;
}

static inline
uint8_t osm_ar_info_is_pfrn_enabled(const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->enable_features1 & IB_AR_INFO_FEATURE_MASK_PFRN_ENABLED);
}

static inline
uint8_t osm_ar_info_is_pfrn_supported(const ib_ar_info_t * const p_ar_info)
{
	return (p_ar_info->cap_fields1 & IB_AR_INFO_CAP_FEILDS_PFRN_SUPPORTED);
}

static inline
void osm_ar_info_set_pfrn_enabled(ib_ar_info_t * const p_ar_info)
{
	p_ar_info->enable_features1 |= IB_AR_INFO_FEATURE_MASK_PFRN_ENABLED;
}

static inline
void osm_ar_info_set_pfrn_disabled(ib_ar_info_t * const p_ar_info)
{
	p_ar_info->enable_features1 &= ~IB_AR_INFO_FEATURE_MASK_PFRN_ENABLED;
}

void osm_ar_print_subgroups(FILE * file, struct osm_switch *p_sw);

uint16_t osm_ar_get_plft_top(osm_advanced_routing_data_t *p_ar_data,
			     uint8_t plft_id);

void osm_ar_lft_entry_get(struct osm_switch *p_sw, uint8_t plft_id, uint16_t lid,
			  uint8_t *port, uint8_t *state, uint16_t *group_id_ho);

osm_madw_t *get_ar_lft_mad(struct osm_sm *sm, struct osm_switch *p_sw);
osm_madw_t *get_group_table_mad(struct osm_sm *sm, struct osm_switch *p_sw);
osm_madw_t *osm_get_group_table_get_mad(struct osm_sm *sm, struct osm_switch *p_sw);
osm_madw_t *get_group_table_copy_mad(struct osm_sm *sm, struct osm_switch *p_sw);
osm_madw_t *get_rn_sub_group_direction_mad(struct osm_sm *sm, struct osm_switch *p_sw);
int osm_get_ar_group_tables(struct osm_sm *sm);

void osm_ar_calculate_hbf(cl_map_item_t * p_map_item, void *cxt);
void osm_ar_calculate_group_to_copy(struct osm_switch *p_sw, void *cxt, unsigned thread_id);
int  osm_set_whbf_block(osm_advanced_routing_data_t *p_ar_data,
			uint16_t block_id, ib_whbf_config_t *p_block);
osm_madw_t *get_whbf_mad(struct osm_sm *sm, struct osm_switch *p_sw);

void osm_ar_calculate_pfrn(osm_subn_t * p_subn);

static inline boolean_t ar_is_port_in_subgroup(osm_ar_subgroup_t *p_subgroup, uint8_t port)
{
	/* the ports bitmask is presented in network order */
	return (p_subgroup->mask[31 - (port / 8)] & (1 << (port % 8)));
}

void osm_ar_subgroup_add_subgroup(osm_ar_subgroup_t *p_to_subgroup,
				  osm_ar_subgroup_t *p_from_subgroup);

osm_ar_subgroup_t * osm_ar_subgroup_get_subn(osm_advanced_routing_data_t *p_ar_data,
					     uint16_t group_id, uint8_t subgroup_id);

boolean_t osm_ar_is_empty_group(osm_ar_subgroup_t *p_subgroup);

END_C_DECLS
