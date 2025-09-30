/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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

/*
 * Abstract:
 * 	Declaration of osm_service_rec_t.
 *	This object represents an IBA Service Record.
 *	This object is part of the OpenSM family of objects.
 */

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_spinlock.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_log.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/Service Record
* NAME
*	Service Record
*
* DESCRIPTION
*	The service record encapsulates the information needed by the
*	SA to manage service registrations.
*
*	The service records is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Anil S Keshavamurthy, Intel
*
*********/
/****s* OpenSM: Service Record/osm_svcr_t
* NAME
*	osm_svcr_t
*
* DESCRIPTION
*	Service Record structure.
*
*	The osm_svcr_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_svcr {
	cl_list_item_t list_item;
	ib_service_record_t service_record;
	uint32_t modified_time;
	uint32_t lease_period;
} osm_svcr_t;
/*
* FIELDS
*	list_item
*		List Item for Quick List linkage.  Must be first element!!
*
*	service_record
*		IB Service record structure
*
*	modified_time
*		Last modified time of this record in milliseconds
*
*	lease_period
*		Remaining lease period for this record
*
*
* SEE ALSO
*********/

/****f* OpenSM: Service Record/osm_svcr_new
* NAME
*	osm_svcr_new
*
* DESCRIPTION
*	Allocates and initializes a Service Record for use.
*
* SYNOPSIS
*/
osm_svcr_t *osm_svcr_new(IN const ib_service_record_t * p_svc_rec);
/*
* PARAMETERS
*	p_svc_rec
*		[in] Pointer to IB Service Record
*
* RETURN VALUES
*	Pointer to osm_svcr_t structure.
*
* NOTES
*	Allows calling other service record methods.
*
* SEE ALSO
*	Service Record, osm_svcr_delete
*********/

/****f* OpenSM: Service Record/osm_svcr_init
* NAME
*	osm_svcr_init
*
* DESCRIPTION
*	Initializes the osm_svcr_t structure.
*
* SYNOPSIS
*/
void osm_svcr_init(IN osm_svcr_t * p_svcr,
		   IN const ib_service_record_t * p_svc_rec);
/*
* PARAMETERS
*	p_svcr
*		[in] Pointer to osm_svcr_t structure
*
*	p_svc_rec
*		[in] Pointer to IB Service Record
*
* SEE ALSO
*	Service Record
*********/

/****f* OpenSM: Service Record/osm_svcr_delete
* NAME
*	osm_svcr_delete
*
* DESCRIPTION
*	Deallocates the osm_svcr_t structure.
*
* SYNOPSIS
*/
void osm_svcr_delete(IN osm_svcr_t * p_svcr);
/*
* PARAMETERS
*	p_svcr
*		[in] Pointer to osm_svcr_t structure
*
* SEE ALSO
*	Service Record, osm_svcr_new
*********/

/****f* OpenSM: Service Record/osm_svcr_get_by_rid
* NAME
*	osm_svcr_get_by_rid
*
* DESCRIPTION
*	Search the Service Record Database by record service_id,
* service_gid and service_pkey (RID).
*
* SYNOPSIS
*/
osm_svcr_t *osm_svcr_get_by_rid(IN osm_subn_t const *p_subn,
				IN osm_log_t * p_log,
				IN ib_service_record_t * p_svc_rec);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to Subnet structure
*
*	p_log
*		[in] Pointer to osm_log_t
*
*	p_svc_rec
*		[in] Pointer to IB Service Record
*
* RETURN VALUES
*	If a matching record is found, pointer to osm_svcr_t structure.
*	Otherwise, pointer to NULL.
*
* SEE ALSO
*	Service Record
*********/

/****f* OpenSM: Service Record/osm_svcr_insert_to_db
* NAME
*	osm_svcr_insert_to_db
*
* DESCRIPTION
*	Insert new Service Record into Database
*
* SYNOPSIS
*/
void osm_svcr_insert_to_db(IN osm_subn_t * p_subn, IN osm_log_t * p_log,
			   IN osm_svcr_t * p_svcr);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to Subnet structure
*
*	p_log
*		[in] Pointer to osm_log_t
*
*	p_svcr
*		[in] Pointer to IB Service Record to be inserted
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Service Record, osm_svcr_remove_from_db
*********/

/****f* OpenSM: Service Record/osm_svcr_remove_from_db
* NAME
*	osm_svcr_remove_from_db
*
* DESCRIPTION
*	Remove a Service Record from Database
*
* SYNOPSIS
*/
void osm_svcr_remove_from_db(IN osm_subn_t * p_subn, IN osm_log_t * p_log,
			     IN osm_svcr_t * p_svcr);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to Subnet structure
*
*	p_log
*		[in] Pointer to osm_log_t
*
*	p_svcr
*		[in] Pointer to IB Service Record to be removed
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Service Record, osm_svcr_insert_to_db
*********/

/****f* OpenSM: Service Record/osm_svcr_remove_by_guids
* NAME
*	osm_svcr_remove_by_guids
*
* DESCRIPTION
*	Remove and delete from Database all Service Records of specified
*	port GUIDs.
*
* SYNOPSIS
*/
void osm_svcr_remove_by_guids(IN osm_subn_t * p_subn, IN osm_log_t * p_log,
			      IN ib_net64_t * guids, IN int num_guids);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to Subnet structure
*
*	p_log
*		[in] Pointer to osm_log_t
*
*	guids
*		[in] Array of GUIDs
*
*	num_guids
*		[in] GUIDs array length
*
*
* RETURN VALUES
*	This function does not return a value.
*
* SEE ALSO
*	Service Record, osm_svcr_insert_to_db, osm_svcr_delete
*********/

/****f* OpenSM: Service Record/osm_svcr_is_differ
* NAME
*	osm_svcr_is_differ
*
* DESCRIPTION
*	Check if service records differ in data blocks
*
* SYNOPSIS
*/
boolean_t osm_svcr_is_differ(IN const osm_svcr_t * p_svcr,
			     IN const ib_service_record_t * p_svc_rec);

/*
* PARAMETERS
*	p_svcr
*		[in] Pointer to IB Service Record to be compared
*	p_svc_rec
*		[in] Pointer to second Service Record for comparison
*
* RETURN VALUES
*	Returns TRUE if the records differ in the data, FALSE otherwise.
*
* SEE ALSO
*
*********/
END_C_DECLS
