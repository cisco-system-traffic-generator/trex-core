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
 *      capability manager
 */

#ifndef OSM_CAPABILITY_MGR_H
#define OSM_CAPABILITY_MGR_H

#include <complib/cl_map.h>
#include <iba/ib_types.h>
#include <opensm/osm_log.h>

#define OSM_CAP_MGR_WILDCARD_SUPPORT_STR "all"

#define MIN_CAPABILITIES_TO_KEEP 128
static const uint32_t ANY_VENDOR = 0;

struct osm_sm;
struct osm_node;

typedef enum _osm_capability_id {
	GI = 0,
	MEPI,
	SHARP,
	AME,
	CAP_ID_END
} osm_capability_id_t;

/****s* OpenSM: OpenSM: Capability mgr/osm_capability_mgr_t
* NAME
*	osm_capability_mgr_t
*
* DESCRIPTION
*	Capability Manager object
*
* SYNOPSIS
*/
typedef struct osm_capability_mgr {
	cl_map_t capabilities;
	uint8_t wildcard_capabilities[CAP_ID_END];
} osm_capability_mgr_t;
/*
* FIELDS
*
*	capabilities
*		Map from vendor / device IDs to supported capabilities.
*
*	wildcard_capabilities
*		Capabilities that are considered as supported on all devices.
*
*********/

/****f* OpenSM: Capability mgr/osm_cap_mgr_init
* NAME
*	osm_cap_mgr_init
*
* DESCRIPTION
*	Init the capability manager
*
* SYNOPSIS
*/
ib_api_status_t osm_cap_mgr_init(IN osm_capability_mgr_t *p_cap_mgr,
				 IN struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_cap_mgr
*		Pointer to a cap mgr item.
*
* RETURN VALUE
*	IB_SUCCESS on success
*	IB_ERROR on failure
*/

/****f* OpenSM: Capability mgr/osm_cap_mgr_destroy
* NAME
*	osm_cap_mgr_destroy
*
* DESCRIPTION
*	Destroy the capability manager
*
* SYNOPSIS
*/
void osm_cap_mgr_destroy(IN osm_capability_mgr_t *p_cap_mgr);
/*
* PARAMETERS
*	p_cap_mgr
*		Pointer to a cap mgr object.
*
* RETURN VALUE
*	N/A
*/

/****f* OpenSM: Capability mgr/osm_cap_mgr_process
* NAME
*	osm_cap_mgr_process
*
* DESCRIPTION
*	Process capability manager
*
* SYNOPSIS
*/
void osm_cap_mgr_process(IN osm_capability_mgr_t *p_cap_mgr, IN struct osm_sm *p_sm);
/*
* PARAMETERS
*	p_cap_mgr
*		Pointer to a cap mgr object.
*
*	p_sm
*		Pointer to sm object
*
* RETURN VALUE
*	N/A
*/

/****f* OpenSM: Capability mgr/osm_cap_mgr_is_enabled
* NAME
*	osm_cap_mgr_is_enabled
*
* DESCRIPTION
*	Check if capability is enabled for a specific vendor and device
*
* SYNOPSIS
*/
boolean_t osm_cap_mgr_is_enabled(osm_capability_mgr_t *p_cap_mgr,
				 osm_capability_id_t cap_id,
				 struct osm_node *p_node);
				 //osm_capability_id_t cap_id,
				 //ib_net32_t vendor_id, ib_net16_t device_id);
/*
* PARAMETERS
*	p_cap_mgr
*		Pointer to capability manager object
*
*	cap_id
*		Id of the capability
*
*	vendor_id
*		Vendor ID value
*
*	device_id
*		Device Id value
*
* RETURN VALUE
*	Boolean indication whether the capability is enabled for this vendor
*	and device.
*/

#endif
