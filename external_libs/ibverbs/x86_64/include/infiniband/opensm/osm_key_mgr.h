/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2020 Mellanox Technologies LTD. All rights reserved.
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
 *    OSM Key Manager for key configuration of multiple classes
 *
 * Author:
 *    Or Nechemia, Mellanox
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <complib/cl_timer.h>
#include <complib/cl_passivelock.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>
#include <opensm/osm_vendor_specific.h>
#include <opensm/osm_congestion_control.h>
#include <opensm/osm_n2n.h>
#include <opensm/osm_port.h>

#define OSM_KEY_TYPE_MANAGER		0
#define OSM_KEY_TYPE_NEIGHBOR_SW	1

/*
 * Flags for invalidating port keys:
 * OSM_KEY_MGR_FLAG_INVL_NONE - no invalidation
 * OSM_KEY_MGR_FLAG_INVL_DUAL_KEY_SUPPORTED - invalidate only ports that support dual key
 * OSM_KEY_MGR_FLAG_INVL_ALL - invalidate all ports
 */
#define OSM_KEY_MGR_FLAG_INVL_NONE			0x00
#define OSM_KEY_MGR_FLAG_INVL_DUAL_KEY_SUPPORTED	0x01
#define OSM_KEY_MGR_FLAG_INVL_ALL 			0xFF

struct osm_opensm;
struct osm_sm;

typedef enum _osm_key_mgr_recalculate_keys_reason {
	/*
	 * This is used to indicate that the recalculation keys reason is not a periodic update.
	 * The actual reason could be a parameter change (opts.key_mgr_seed or m_key).
	 */
	OSM_KEY_MGR_RECALC_KEYS_REASON_OTHER,
	OSM_KEY_MGR_RECALC_KEYS_REASON_PERIODIC_UPDATE,
} recalculate_keys_reason_t;

/****s* OpenSM: KeyManager/osm_key_mgr_t
* NAME
*	osm_key_mgr_t
*
* DESCRIPTION
*	OpenSM Key manager object structure.
*	This structure should be treated as opaque and should be manipulated
*	only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_key_mgr {
	struct osm_sm * p_sm;
	const osm_subn_opt_t * p_opt;
	osm_log_t * p_log;
	cl_plock_t seed_lock;
	uint8_t key_seed[64];
	uint32_t seed_version_smp;
	uint32_t seed_version_vs;
	uint32_t seed_version_cc;
	uint32_t seed_version_n2n;
	boolean_t periodic_update_active;
	boolean_t restart_timer;
	cl_timer_t periodic_key_update_timer;
	recalculate_keys_reason_t recalc_keys_reason;
	unsigned int num_mkeys_configured;
} osm_key_mgr_t;
/*
* FIELDS
*	p_sm
*		Pointer to Subnet Manager instance
*
*	p_opt
*		Pointer to SM options structure
*
*	p_log
*		Pointer to SM logging structure
*
*	key_seed
*		Seed used for calculating key per port.
*
*	seed_version_smp
*		Seed version for SMP datagrams. Determines M_Key validity.
*
*	seed_version_vs
*		Seed version for VS. Determines VS key validity.
*
*	seed_version_cc
*		Seed version for CC. Determines CC key validity.
*
*	seed_version_n2n
*		Seed version for N2N. Determines N2N key validity (for both manager and neighbor keys).
*
*	periodic_update_active
*		An indicator of the periodic update activity status.
*		In case periodic update is active, we have to wait one interval before
*		actually changing keys
*
*	restart_timer
*		A flag that indicates that the periodic update timer should be restarted
*		(in the next call to `osm_key_mgr_restart_periodic_update`).
*
*	periodic_key_update_timer
*		A timer used to periodically change the key seed.
*
*	num_mkeys_configured
*		Number of end ports that successfully configured M_Key (in the last cycle).
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_init
* NAME
* 	osm_key_mgr_init
*
* DESCRIPTION
*	Initializes osm_key_mgr_t structure for use
*
* SYNOPSIS
*/
ib_api_status_t osm_key_mgr_init(osm_key_mgr_t * p_key_mgr, struct osm_sm * p_sm);
/*
* PARAMETERS
*	p_key_mgr
*
*		The key manager instance to initalize.
*	p_sm
*		Pointer to Subnet Manager instance.
*
* RETURN VALUE
*	ib_api_status_t
*
* NOTES
*	osm_key_mgr_destroy must be called to free resources used by the key manager.
*
* SEE ALSO
*	osm_key_mgr_destroy
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_destroy
* NAME
* 	osm_key_mgr_destroy
*
* DESCRIPTION
*	Frees resources used by the key manager
*
* SYNOPSIS
*/
void osm_key_mgr_destroy(osm_key_mgr_t * p_key_mgr);
/*
* PARAMETERS
*	p_key_mgr
*		The key manager instance to destroy.
*
* RETURN VALUE
*	None
*
* NOTES
*	osm_key_mgr_destroy must be called to free resources used by the key manager.
*
* SEE ALSO
*	osm_key_mgr_init
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_db_is_initialized
* NAME
* 	osm_key_mgr_db_is_initialized
*
* DESCRIPTION
*	Checks whether the database was initialized.
*
* SYNOPSIS
*/
boolean_t osm_key_mgr_db_is_initialized(osm_opensm_t * p_osm, uint8_t mgmt_class, uint8_t key_type);
/*
* PARAMETERS
*	p_osm
*		Pointer to an osm_opensm_t object.
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N, SMP
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUE
*	Boolean indicating whether the db was initialized.
*
* SEE ALSO
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_flush_db_to_disk
* NAME
* 	osm_key_mgr_flush_db_to_disk
*
* DESCRIPTION
* 	Writes the guid2xkey database to disk.
*
* SYNOPSIS
*/
int osm_key_mgr_flush_db_to_disk(osm_opensm_t * p_osm, uint8_t mgmt_class, uint8_t key_type);
/*
* PARAMETERS
*	p_osm
*		Pointer to an osm_opensm_t object.
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N, SMP
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUE
*	0 if successful, 1 otherwise.
*
* SEE ALSO
*********/

/****f* OpenSM: KeyManager/key_mgr_is_dual_key_supported
* NAME
* 	key_mgr_is_dual_key_supported
*
* DESCRIPTION
* 	Query whether a port supports dual key for a management class.
*
* SYNOPSIS
*/
boolean_t key_mgr_is_dual_key_supported(const osm_port_t * p_port, uint8_t mgmt_class);
/*
* PARAMETERS
*	p_port
*		Pointer to port
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N, SMP
*
* RETURN VALUE
*	Boolean indicating whether the port supports dual key for the required class.
*
* SEE ALSO
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_calculate_key
* NAME
* 	osm_key_mgr_calculate_key
*
* DESCRIPTION
* 	Calculate key for port by its guid
*
* SYNOPSIS
*/

ib_net64_t osm_key_mgr_calculate_key(osm_key_mgr_t * p_key_mgr,
				     ib_net64_t guid,
				     uint8_t mclass,
				     uint8_t key_type);
/*
* PARAMETERS
*	p_key_mgr
*		Pointer to key manager
*
*	guid
*		Guid of the port
*
*	mclass
*		Management class. Currently supported: CC, VS, N2N, SMP
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUE
*	Class key for input port.
*
* SEE ALSO
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_generate_key
* NAME
* 	osm_key_mgr_generate_key
*
* DESCRIPTION
* 	Get key for port by its guid
*
* SYNOPSIS
*/
ib_net64_t osm_key_mgr_generate_key(struct osm_opensm * p_osm, osm_port_t * p_port,
				    uint8_t mgmt_class, uint8_t key_type);
/*
* PARAMETERS
*	p_osm
*		Pointer to an osm_opensm_t object.
*
*	p_port
*		Pointer to port
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N, SMP
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUE
*	Class key for input port.
*
* SEE ALSO
*
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_restart_periodic_update
* NAME
* 	osm_key_mgr_restart_periodic_update
*
* DESCRIPTION
*	Starts/Restarts periodic update of the key seed.
*
* SYNOPSIS
*/
cl_status_t osm_key_mgr_restart_periodic_update(osm_key_mgr_t * p_key_mgr, boolean_t force_restart);
/*
* PARAMETERS
*	p_key_mgr
*		The key manager instance.
*
*	force_restart
*		Specifying TRUE will restart periodic update immediately.
*		Specifying FALSE will restart periodic update only when restart_timer flag is set.
*
* RETURN VALUE
*	cl_status_t
*
* NOTES
*	osm_key_mgr_init must be called prior to this function.
*
* SEE ALSO
*	osm_key_mgr_stop_periodic_update
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_stop_periodic_update
* NAME
* 	osm_key_mgr_stop_periodic_update
*
* DESCRIPTION
*	Stops periodic update of the key seed.
*
* SYNOPSIS
*/
void osm_key_mgr_stop_periodic_update(osm_key_mgr_t * p_key_mgr);
/*
* PARAMETERS
*	p_key_mgr
*		The key manager instance.
*
* RETURN VALUE
*	None
*
* NOTES
*	osm_key_mgr_init must be called prior to this function.
*
* SEE ALSO
*	osm_key_mgr_restart_periodic_update
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_intiate_key_update
* NAME
* 	osm_key_mgr_intiate_key_update
*
* DESCRIPTION
*	Called by state manager, this function handles the OSM_SIGNAL_UPDATE_KEYS signal.
*	It prepares key manager state for key change and invokes a heavy sweep
*	to send the key configuration MADs.
*
* SYNOPSIS
*/
void osm_key_mgr_intiate_key_update(struct osm_sm * p_sm);
/*
* PARAMETERS
*	p_sm
*		SM instance.
*
* RETURN VALUE
*	None
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_key_update_preprocess
* NAME
* 	osm_key_mgr_key_update_preprocess
*
* DESCRIPTION
*	Called at the start of a sweep, this function marks supported keys for recalculation
*	in order for the KeyInfo mads to be sent during this sweep.
*
* SYNOPSIS
*/
void osm_key_mgr_key_update_preprocess(struct osm_sm * p_sm);
/*
* PARAMETERS
*	p_sm
*		SM instance.
*
* RETURN VALUE
*	None
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_key_update_postprocess
* NAME
* 	osm_key_mgr_key_update_postprocess
*
* DESCRIPTION
*	Called at the end of a sweep, this function resets key manager state,
*	logs finish of key update cycle, reports it to UFM and restarts timer
*	to start another cycle when due.
*
* SYNOPSIS
*/
void osm_key_mgr_key_update_postprocess(struct osm_sm * p_sm);
/*
* PARAMETERS
*	p_sm
*		SM instance.
*
* RETURN VALUE
*	None
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_is_key_valid
* NAME
* 	osm_key_mgr_is_key_valid
*
* DESCRIPTION
*	Checks if the key is valid for the given port and management class.
*
* SYNOPSIS
*/
boolean_t osm_key_mgr_is_key_valid(osm_opensm_t * p_osm, osm_port_t * p_port,
				   uint8_t mgmt_class, uint8_t key_type);
/*
* PARAMETERS
*	p_osm
*		Pointer to an osm_opensm_t object.
*
*	p_port
*		Pointer to port
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
* RETURN VALUE
*	Boolean indicating whether the key is valid.
*********/

/****f* OpenSM: KeyManager/osm_key_mgr_invalidate_port_keys
* NAME
* 	osm_key_mgr_invalidate_port_keys
*
* DESCRIPTION
*	Invalidates port keys for the given management class.
*
* SYNOPSIS
*/
void osm_key_mgr_invalidate_port_keys(struct osm_sm * p_sm, uint8_t mgmt_class, uint8_t flag);
/*
* PARAMETERS
*	p_sm
*		SM instance.
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N
*
*	flag
*		Flag indicating keys of which ports to invalidate. (OSM_KEY_MGR_FLAG_INVL_*)
*
* RETURN VALUE
*	None
*********/

void osm_key_mgr_get_cached_key(struct osm_opensm * p_osm, osm_port_t * p_port,
				uint8_t mgmt_class, uint8_t key_type);

void osm_key_mgr_set_cached_key(struct osm_opensm * p_osm, osm_port_t * p_port,
				uint8_t mgmt_class, uint8_t key_type);

/****f* OpenSM: KeyManager/osm_key_mgr_set_key_if_not_present_in_db
* NAME
* 	osm_key_mgr_set_key_if_not_present_in_db
*
* DESCRIPTION
* 	Sets a key to the guid2xkey database, if the database doesn't contain a key already.
*
* SYNOPSIS
*/
void osm_key_mgr_set_key_if_not_present_in_db(
	struct osm_opensm * p_osm, osm_port_t * p_port,
	uint8_t mgmt_class, uint8_t key_type,
	uint64_t key);
/*
* PARAMETERS
*	p_osm
*		Pointer to an osm_opensm_t object.
*
*	p_port
*		Pointer to port
*
*	mgmt_class
*		Management class. Currently supported: CC, VS, N2N, SMP
*
*	key_type
*		Indicates which N2N Class key is referred -
*		manager key or node to node key.
*
*	key
*		The key to set to the database.
*
* RETURN VALUE
*	None
*********/
