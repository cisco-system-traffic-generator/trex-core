/*
 * Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * This module handles Fabric Mode features:
 * 	Rate Limiter
 * 	Turbo Path
 *
 * Author:
 * 	Or Nechemia, NVIDIA
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_log.h>

#define OSM_TP_DISABLE				0
#define OSM_TP_ENABLE_DEVICE_DEFAULTS		1
#define OSM_TP_ENABLE_CUSTOM			7
/* tp_enable field is up to 3 bits. Hence using value > 7 for this mode */
#define OSM_TP_ENABLE_NO_CHANGE			8

#define OSM_FABRIC_PROFILE_NAME_MAX_LEN		32

/* For no change of bandwidth utilization parameter of ExtendedPortInfo */
#define INVALID_BW_UTILIZATION			0x400
/* For no change of Turbo Path parameters of ExtendedSwitchInfo (represent values for FW registers) */
#define INVALID_TP_REG_VALUE			0x400

struct osm_sm;
struct osm_subn;

typedef struct osm_fabric_mode_profile {
	cl_fmap_item_t fmap_item;
	char name[OSM_FABRIC_PROFILE_NAME_MAX_LEN];
	boolean_t bw_util_enable;
	uint16_t bw_utilization;
	uint8_t tp_enable;
        uint16_t tp_req_delay;
        uint16_t tp_set_trig_thr;
        uint16_t tp_rst_trig_thr;
        uint16_t tp_req_trig_window;
} osm_fabric_mode_profile_t;

typedef struct osm_fabric_mode_data {
	cl_fmap_t fabric_mode_profiles_map;
} osm_fabric_mode_data_t;

/****f* OpenSM: FabricMode/osm_is_fabric_mode_enabled
* NAME
*	osm_is_fabric_mode_enabled
*
* DESCRIPTION
*	Returns TRUE when NVLink is enabled
*
* SYNOPSIS
*/
boolean_t osm_is_fabric_mode_enabled(IN struct osm_subn * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
* RETURN VALUES
*	Return TRUE if Fabric Mode is enabled, FALSE otherwise.
*********/

void osm_fabric_mode_clear_profiles(struct osm_subn * p_subn);

void osm_fabric_mode_process(struct osm_sm * sm);

