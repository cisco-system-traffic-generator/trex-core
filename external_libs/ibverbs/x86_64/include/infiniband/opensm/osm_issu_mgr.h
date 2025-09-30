
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <complib/cl_event_osd.h>
#include <complib/cl_ptr_vector.h>
#include <opensm/osm_subnet.h>


#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

typedef	struct osm_log		osm_log_t;

/* -------------------------------------------------------------------------------------------------
 *
 * API functions and definitions
 */
#define issu_is_disabled(P_MGR)		(((P_MGR)->mode == ISSU_MODE_DISABLED) || !(P_MGR)->inited)
#define issu_is_passive(P_MGR)		(((P_MGR)->mode == ISSU_MODE_PASSIVE) && (P_MGR)->inited)
#define issu_is_enabled(P_MGR)		(((P_MGR)->mode == ISSU_MODE_ENABLED) && (P_MGR)->inited)

typedef struct {
	boolean_t		init;
	cl_ptr_vector_t		vector;
} issu_set_t;

typedef enum {
	ISSU_MODE_DISABLED = 0,
	ISSU_MODE_ENABLED = 1,
	ISSU_MODE_PASSIVE = 2,
	ISSU_MODE_MAX = 3,
} issu_mode_t;

typedef enum {
	ISSU_STEP_IDLE = 0,
	ISSU_STEP_PRE_UPGRADE = 1,
	ISSU_STEP_UPGRADING = 2,
} issu_step_t;

typedef struct {
	osm_sm_t		*p_sm;
	osm_log_t		*p_log;
	osm_subn_t		*p_subn;

	issu_set_t		capable;
	issu_set_t		waiting;
	issu_set_t		upgrades;
	issu_set_t		isolated;

	issu_mode_t		mode;
	issu_step_t		step;
	boolean_t		inited;

	cl_event_t		event;
	cl_plock_t		lock;
	cl_qlist_t		traps;

	uint32_t		timeout;
	uint32_t		pre_upgrade_time;
	time_t			start;
	time_t			end;
	boolean_t		invalidate;

	char			startup_file[PATH_MAX];
} osm_issu_mgr_t;

#define	ISSU_PRE_UPGRADE_TIMEOUT	15
#define	ISSU_UPGRADE_TIMEOUT		20
#define	ISSU_STARTUP_FILE		"issu_startup"

typedef enum {
	ISSU_EVENT_NONE = 0,
	ISSU_EVENT_PREWAIT,
	ISSU_EVENT_POSTWAIT,
	ISSU_EVENT_ISOLATE,
	ISSU_EVENT_RECOVER,
	ISSU_EVENT_LAST,
} osm_issu_event_t;

const char *osm_issu_status(osm_subn_t *, osm_switch_t *p_sw);
ib_api_status_t osm_issu_mgr_init(osm_issu_mgr_t *p_issu_mgr, osm_sm_t *p_sm);
void osm_issu_mgr_destroy(osm_issu_mgr_t *p_issu_mgr, osm_sm_t *p_sm);
void osm_issu_mgr_mode_change(osm_subn_t *p_subn, void *p_value);
void osm_issu_mgr_timeout_change(osm_subn_t *p_subn, void *p_value);
void osm_issu_mgr_pre_upgrade_time_change(osm_subn_t *p_subn, void *p_value);
void osm_issu_mgr_startup_file_change(osm_subn_t *p_subn, void *p_value);
void osm_issu_mgr_enqueue_trap(osm_sm_t *p_sm, uint16_t lid, uint32_t state);

boolean_t osm_issu_mgr_check(osm_sm_t *p_sm);
int osm_issu_mgr_process(osm_sm_t *p_sm);

END_C_DECLS

