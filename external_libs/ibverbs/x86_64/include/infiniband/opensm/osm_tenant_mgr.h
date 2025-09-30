/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <complib/cl_ptr_vector.h>
#include <complib/cl_hashmap.h>
#include <opensm/osm_subnet.h>


#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

typedef	struct osm_sm		osm_sm_t;
typedef	struct osm_log		osm_log_t;
typedef	struct osm_opensm	osm_opensm_t;

/* -------------------------------------------------------------------------------------------------
 *
 * Internal typedefs
 */

/****d* OpenSM: Types/osm_tenant_mgr_notice_type
* NAME
*	osm_tenant_mgr_notice_type
*
* DESCRIPTION
*       Enumerates tenant violation notice types.
*
* SYNOPSIS
*/
typedef enum _osm_tenant_mgr_notice_type {
	OSM_TENANT_MGR_NOTICE_GUID_DEVIATION = 0,
	OSM_TENANT_MGR_NOTICE_MC_GROUP_VIOLATION
} osm_tenant_mgr_notice_type;
/***********/

#define OSM_DEFAULT_MAX_TENANT_MC_GROUPS  0xFFFFFFFF

typedef cl_hashmap_t		osm_tenant_guid_list_t;
typedef cl_hashmap_t		osm_tenant_list_t;

typedef struct {
	osm_tenant_guid_list_t	virtual_guids;
	osm_tenant_guid_list_t	physical_guids;
	osm_tenant_list_t	tenant_list;
} osm_tenant_database_t;

typedef struct {
	uint64_t	tenant_id;
	uint32_t	max_tenant_mc_groups;
	uint32_t	mc_groups_counter;
	boolean_t	tenant_vport_lids;
} osm_tenant_t;
/*
* FIELDS
*	tenant_id
*		ID of the tenant
*
*	max_tenant_mc_groups
*		Maximum number of multicast groups this tenant is permited to open
*     		Default value is 0xFFFFFFFF
*
*	mc_groups_counter
*		Counter of the multicast groups currently opened by this tenant
*
*	tenant_vport_lids
*		Allow Vport LIDs assignment
*		Default is False
*********/

typedef struct {
	osm_log_t		*p_log;
	osm_tenant_database_t	*p_tenant_db;	/* struct being filled in by parser */
	osm_tenant_t 		*p_tenant;	/* tenant object currently being updated */
	boolean_t		is_virtual_guid; /* are the guids currently being parsed virtual */
} osm_tenant_parser_t;

/* -------------------------------------------------------------------------------------------------
 *
 * API functions and definitions
 */

typedef struct {
	osm_subn_t		*p_subn;
	osm_log_t		*p_log;
	osm_tenant_database_t	*p_tenant_db;
	char			*filename;
	uint32_t		crc;
} osm_tenant_mgr_t;

int osm_tenant_db_insert_guid(IN osm_tenant_database_t *p_tenant_db, IN uint64_t guid, IN osm_tenant_t *p_tenant, IN boolean_t is_virtual_guid);
int osm_tenant_db_insert_tenant(IN osm_tenant_database_t *p_tenant_db, IN uint64_t tenant_id, IN osm_tenant_t *p_tenant);
osm_tenant_t *osm_tenant_list_find_by_id(IN osm_tenant_database_t *p_tenant_db, IN uint64_t tenant_id);
osm_tenant_t *osm_tenant_create(IN osm_log_t *p_log, IN uint64_t tenant_id);
int osm_tenant_delete_by_id(IN osm_tenant_mgr_t *p_mgr, IN uint64_t tenant_id);
int osm_tenant_mgr_validate_mcgroup_create(IN osm_tenant_mgr_t *p_mgr, IN ib_member_rec_t *mcm_rec);
void osm_tenant_mgr_decrease_counter(IN osm_tenant_mgr_t *p_mgr, IN ib_member_rec_t *mcm_rec);
osm_tenant_t *osm_tenant_db_get_tenant_by_physical_guid(IN osm_tenant_database_t *p_db, IN uint64_t guid);
osm_tenant_t *osm_tenant_db_get_tenant_by_virtual_guid(IN osm_tenant_database_t *p_db, IN uint64_t guid);

ib_api_status_t osm_tenant_mgr_init(osm_tenant_mgr_t *p_tenant_mgr, osm_sm_t *p_sm);
void osm_tenant_mgr_destroy(osm_tenant_mgr_t *p_tenant_mgr, osm_sm_t *p_sm);
int osm_tenant_mgr_rescan(osm_subn_t *p_subn);
void osm_tenant_mgr_config_change(osm_subn_t *p_subn, void *p_value);
int osm_tenant_mgr_validate(osm_sm_t *p_sm, uint64_t virtual_guid, uint64_t physical_guid, uint16_t vport_index);
void osm_tenant_mgr_count_mc_groups_in_fabric(IN osm_subn_t *p_subn);
void osm_tenant_mgr_log_mc_group_counts(IN cl_qhashmap_item_t *p_item, IN void *context);
void osm_tenant_mgr_zero_tenants_mc_counter(IN cl_qhashmap_item_t *p_item, IN void *context);

END_C_DECLS

