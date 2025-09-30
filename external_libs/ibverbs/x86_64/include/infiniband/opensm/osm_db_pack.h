/*
 * Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/****h* OpenSM/DB-Pack
* NAME
*	Database Types
*
* DESCRIPTION
*	This module provides packing and unpacking of the database
*  storage into specific types.
*
*  The following domains/conversions are supported:
*  guid2lid - key is a guid and data is a lid.
*
* AUTHOR
*	Eitan Zahavi, Mellanox Technologies LTD
*
*********/

#pragma once

#include <opensm/osm_db.h>
#define MAX_DATE_STR_LEN 32

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****f* OpenSM: DB-Pack/osm_db_guid2lid_init
* NAME
*	osm_db_guid2lid_init
*
* DESCRIPTION
*	Initialize a domain for the guid2lid table
*
* SYNOPSIS
*/
static inline osm_db_domain_t *osm_db_guid2lid_init(IN osm_db_t * p_db)
{
	return (osm_db_domain_init(p_db, "guid2lid"));
}

/*
* PARAMETERS
*	p_db
*		[in] Pointer to the database object to construct
*
* RETURN VALUES
*	The pointer to the new allocated domain object or NULL.
*
* NOTE: DB domains are destroyed by the osm_db_destroy
*
* SEE ALSO
*	Database, osm_db_init, osm_db_destroy
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_init
* NAME
*	osm_db_guid2lid_init
*
* DESCRIPTION
*	Initialize a domain for the guid2lid table
*
* SYNOPSIS
*/
typedef struct osm_db_guid_elem {
	cl_list_item_t item;
	uint64_t guid;
} osm_db_guid_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*
*  guid
*
************/

/****f* OpenSM: DB-Pack/osm_db_guids
* NAME
*	osm_db_guids
*
* DESCRIPTION
*	Provides back a list of guid elements.
*
* SYNOPSIS
*/
int osm_db_guids(IN osm_db_domain_t * p_domain,
		 OUT cl_qlist_t * p_guid_list);
/*
* PARAMETERS
*	p_domain
*		[in] Pointer to the domain in which guids are the entries identifiers.
*
*	p_guid_list
*		[out] A quick list of guid elements of type osm_db_guid_elem_t
*
* RETURN VALUES
*	0 if successful
*
* NOTE: The output qlist should be initialized before calling this function.
*       The caller is responsible for freeing the items in the list, and
*       destroying the list.
*
* SEE ALSO
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_get
* NAME
*	osm_db_guid2lid_get
*
* DESCRIPTION
*	Get a lid range by given guid.
*
* SYNOPSIS
*/
int osm_db_guid2lid_get(IN osm_db_domain_t * p_g2l, IN uint64_t guid,
			OUT uint16_t * p_min_lid, OUT uint16_t * p_max_lid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
*  p_min_lid
*     [out] Pointer to the resulting min lid in host order.
*
*  p_max_lid
*     [out] Pointer to the resulting max lid in host order.
*
* RETURN VALUES
*	0 if successful. The lid will be set to 0 if not found.
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guids
* osm_db_guid2lid_set, osm_db_guid2lid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_set
* NAME
*	osm_db_guid2lid_set
*
* DESCRIPTION
*	Set a lid range for the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2lid_set(IN osm_db_domain_t * p_g2l, IN uint64_t guid,
			IN uint16_t min_lid, IN uint16_t max_lid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
*  min_lid
*     [in] The min lid value to set
*
*  max_lid
*     [in] The max lid value to set
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guids
* osm_db_guid2lid_get, osm_db_guid2lid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_delete
* NAME
*	osm_db_guid2lid_delete
*
* DESCRIPTION
*	Delete the entry by the given guid
*
* SYNOPSIS
*/
int osm_db_guid2lid_delete(IN osm_db_domain_t * p_g2l, IN uint64_t guid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guids
* osm_db_guid2lid_get, osm_db_guid2lid_set
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2mkey_init
* NAME
*	osm_db_guid2mkey_init
*
* DESCRIPTION
*	Initialize a domain for the guid2mkey table
*
* SYNOPSIS
*/
static inline osm_db_domain_t *osm_db_guid2mkey_init(IN osm_db_t * p_db)
{
	return osm_db_domain_init(p_db, "guid2mkey");
}

/*
* PARAMETERS
*	p_db
*		[in] Pointer to the database object to construct
*
* RETURN VALUES
*	The pointer to the new allocated domain object or NULL.
*
* NOTE: DB domains are destroyed by the osm_db_destroy
*
* SEE ALSO
*	Database, osm_db_init, osm_db_destroy
*********/

/****f* OpenSM: DB-Pack/osm_db_neighbor_init
* NAME
*	osm_db_neighbor_init
*
* DESCRIPTION
*	Initialize a domain for the neighbors table
*
* SYNOPSIS
*/
static inline osm_db_domain_t *osm_db_neighbor_init(IN osm_db_t * p_db)
{
	return osm_db_domain_init(p_db, "neighbors");
}

/*
* PARAMETERS
*	p_db
*		[in] Pointer to the database object to construct
*
* RETURN VALUES
*	The pointer to the new allocated domain object or NULL.
*
* NOTE: DB domains are destroyed by the osm_db_destroy
*
* SEE ALSO
*	Database, osm_db_init, osm_db_destroy
*********/

/****f* OpenSM: DB-Pack/osm_db_guid_portnum_elem
* NAME
*	osm_db_guid_portnum_elem
*
* DESCRIPTION
*	Initialize a domain for the neighbor / port2rail tables
*
* SYNOPSIS
*/
typedef struct osm_db_guid_portnum_elem {
	cl_list_item_t item;
	uint64_t guid;
	uint8_t portnum;
} osm_db_guid_portnum_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*
*  guid
*  portnum
*
************/

/****f* OpenSM: DB-Pack/osm_db_guid_portnum_keys_list
* NAME
*	osm_db_guid_portnum_keys_list
*
* DESCRIPTION
*	Provides back a list of elements according to guid:port number as a key.
*
* SYNOPSIS
*/
int osm_db_guid_portnum_keys_list(IN osm_db_domain_t * p_domain,
				  OUT cl_qlist_t * p_guid_list);
/*
* PARAMETERS
*	p_domain
*		[in] Pointer to the domain
*
*  p_guid_list
*     [out] A quick list of elements of type osm_db_guid_portnum_elem_t
*
* RETURN VALUES
*	0 if successful
*
* NOTE: The output qlist should be initialized before calling this function.
*       The caller is responsible for freeing the items in the list, and
*       destroying the list.
*
* SEE ALSO
* osm_db_neighbor_init, osm_db_guid_portnum_keys_list, osm_db_guid_portnum_delete,
* osm_db_neighbor_get, osm_db_neighbor_set,
* osm_db_port2rail_get, osm_db_port2rail_set
*********/

/****f* OpenSM: DB-Pack/osm_db_neighbor_get
* NAME
*	osm_db_neighbor_get
*
* DESCRIPTION
*	Get a neighbor's guid by given guid/port.
*
* SYNOPSIS
*/
int osm_db_neighbor_get(IN osm_db_domain_t * p_neighbor, IN uint64_t guid1,
			IN uint8_t port1, OUT uint64_t * p_guid2,
			OUT uint8_t * p_port2);
/*
* PARAMETERS
*	p_neighbor
*		[in] Pointer to the neighbor domain
*
*  guid1
*     [in] The guid to look for
*
*  port1
*     [in] The port to look for
*
*  p_guid2
*     [out] Pointer to the resulting guid of the neighboring port.
*
*  p_port2
*     [out] Pointer to the resulting port of the neighboring port.
*
* RETURN VALUES
*	0 if successful. The lid will be set to 0 if not found.
*
* SEE ALSO
* osm_db_neighbor_init, osm_db_guid_portnum_keys_list
* osm_db_neighbor_set, osm_db_guid_portnum_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_neighbor_set
* NAME
*	osm_db_neighbor_set
*
* DESCRIPTION
*	Set up a relationship between two ports
*
* SYNOPSIS
*/
int osm_db_neighbor_set(IN osm_db_domain_t * p_neighbor, IN uint64_t guid1,
			IN uint8_t port1, IN uint64_t guid2, IN uint8_t port2);
/*
* PARAMETERS
*	p_neighbor
*		[in] Pointer to the neighbor domain
*
*  guid1
*     [in] The first guid in the relationship
*
*  port1
*     [in] The first port in the relationship
*
*  guid2
*     [in] The second guid in the relationship
*
*  port2
*     [in] The second port in the relationship
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* osm_db_neighbor_init, osm_db_guid_portnum_keys_list
* osm_db_neighbor_get, osm_db_guid_portnum_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid_portnum_delete
* NAME
*	osm_db_guid_portnum_delete
*
* DESCRIPTION
*	Delete entry with guid:portnum key
*
* SYNOPSIS
*/
int osm_db_guid_portnum_delete(IN osm_db_domain_t * p_domain,
			       IN uint64_t guid, IN uint8_t port);
/*
* PARAMETERS
*	p_domain
*		[in] Pointer to the domain
*
*  guid
*     [in] The guid to look for
*
*  port
*     [in] The port to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* osm_db_neighbor_init, osm_db_guid_portnum_keys_list,
* osm_db_neighbor_get, osm_db_neighbor_set,
* osm_db_port2rail_get, osm_db_port2rail_set
*********/

/****f* OpenSM: DB-Pack/osm_db_an2an_elem
* NAME
*	osm_db_an2an_elem
*
* DESCRIPTION
*	Object representation of an2an cache entry. An an2an entry is keyed by
*	a pair of adjacent switch guids, and contains the port numbers through
*	which each switch routes to the 2nd switch's AN.
*
* SYNOPSIS
*/
typedef struct osm_db_an2an_elem {
	cl_list_item_t item;
	uint64_t guid1;
	uint64_t guid2;
	uint8_t portnum1;
	uint8_t portnum2;
	char timestamp_str[MAX_DATE_STR_LEN];
} osm_db_an2an_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*	guid1
*		guid of the first switch
*	guid2
*		guid of the second switch
*	portnum1
*		port number through which routing from the first switch to the
*		second aggregation node takes place
*	portnum2
*		port number through which routing from the second switch to the
*		first aggregation node takes place
*
************/

/****f* OpenSM: DB-Pack/osm_db_an2an_get_all
* NAME
*	osm_db_an2an_get_all
*
* DESCRIPTION
*	Provides a list of an2an elements.
*
* SYNOPSIS
*/
int osm_db_an2an_get_all(IN osm_db_domain_t * p_domain,
			 OUT cl_qlist_t * p_elements);
/*
* PARAMETERS
*  p_domain
*     [in] Pointer to the an2an domain
*
*  p_elements
*     [out] A quick list of an2an elements of type osm_db_an2an_elem_t
*
* RETURN VALUES
*	0 if successful
*
* NOTE: The output qlist should be initialized before calling this function.
*       The caller is responsible for freeing the items in the list, and
*       destroying the list.
*
* SEE ALSO
* osm_db_an2an_get, osm_db_an2an_set, osm_db_an2an_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_an2an_get
* NAME
*	osm_db_an2an_get
*
* DESCRIPTION
*	Get the an2an route for the given switches (guids). The route is defined
*	as the port numbers (port1, port2) through which each switch routes to
*	the 2nd switch's AN.
*
* SYNOPSIS
*/
int osm_db_an2an_get(IN osm_db_domain_t * p_domain,
		     IN uint64_t sw_guid1, IN uint64_t sw_guid2,
		     OUT uint8_t * port1, OUT uint8_t * port2, OUT char *time);
/*
* PARAMETERS
*   p_domain
*     [in] Pointer to the an2an domain
*
*  sw_guid1
*     [in] The first switch guid.
*
*  sw_guid2
*     [in] The second switch guid
*
*  port1
*     [out] Pointer to the stored route's first port number
*
*  port2
*     [out] Pointer to the stored route's second port number
*
* RETURN VALUES
*	0 if successful. In case of a failure, the value of port1/port2 is
*	undefined.
*
* SEE ALSO
* osm_db_an2an_get_all, osm_db_an2an_set, osm_db_an2an_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_an2an_set
* NAME
*	osm_db_an2an_set
*
* DESCRIPTION
*	Set the an2an route for the given switches (guids). The route is defined
*	as the port numbers (port1, port2) through which each switch routes to
*	the 2nd switch's AN.
*
* SYNOPSIS
*/
int osm_db_an2an_set(IN osm_db_domain_t * p_domain,
		     IN uint64_t sw_guid1, IN uint64_t sw_guid2,
		     IN uint8_t port1, IN uint8_t port2);
/*
* PARAMETERS
*  p_domain
*     [in] Pointer to the an2an domain
*
*  sw_guid1
*     [in] The first switch guid
*
*  sw_guid2
*     [in] The second switch guid
*
*  port1
*     [in] The stored route's first port number
*
*  port2
*     [in] The stored route's second port number
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* osm_db_an2an_get_all, osm_db_an2an_get, osm_db_an2an_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_an2an_delete
* NAME
*	osm_db_an2an_delete
*
* DESCRIPTION
*	Delete an an2an route for given switch guids
*
* SYNOPSIS
*/
int osm_db_an2an_delete(IN osm_db_domain_t * p_domain,
			IN uint64_t sw_guid1, IN uint64_t sw_guid2);
/*
* PARAMETERS
*  p_domain
*     [in] Pointer to the an2an domain
*
*  sw_guid1
*     [in] The first switch guid
*
*  sw_guid2
*     [in] The second switch guid
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* osm_db_an2an_get_all, osm_db_an2an_get, osm_db_an2an_set
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2key_get
* NAME
*	osm_db_guid2key_get
*
* DESCRIPTION
*	Get the key for the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2key_get(IN osm_db_domain_t * p_g2key, IN uint64_t guid,
			OUT uint64_t * key);
/*
* PARAMETERS
*	p_g2key
*		[in] Pointer to the guid2key domain
*
*	guid
*		[in] The guid to look for
*
*	key
*		[out] Pointer to the resulting key in host order.
*
* RETURN VALUES
*	0 if successful. The key will be set to 0 if not found.
*
* SEE ALSO
* 	osm_db_guid2key_get, osm_db_guid2key_set, osm_db_guid2key_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2key_set
* NAME
*	osm_db_guid2key_set
*
* DESCRIPTION
*	Set the key for the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2key_set(IN osm_db_domain_t * p_g2key, IN uint64_t guid,
			IN uint64_t key);
/*
* PARAMETERS
*	p_g2key
*		[in] Pointer to the guid2key domain
*
*	guid
*		[in] The guid to look for
*
*	key
*		[in] The key value to set, in host order
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* 	osm_db_guid2key_get, osm_db_guid2key_set, osm_db_guid2key_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2key_delete
* NAME
*	osm_db_guid2key_delete
*
* DESCRIPTION
*	Delete the entry by the given guid
*
* SYNOPSIS
*/
int osm_db_guid2key_delete(IN osm_db_domain_t * p_g2key, IN uint64_t guid);
/*
* PARAMETERS
*	p_g2key
*		[in] Pointer to the guid2key domain
*
*	guid
*		[in] The guid to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* 	osm_db_guid2key_get, osm_db_guid2key_set, osm_db_guid2key_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2groupid_get
* NAME
*	osm_db_guid2groupid_get
*
* DESCRIPTION
*	Get the group ID for the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2groupid_get(IN osm_db_domain_t * p_g2groupid, IN uint64_t guid,
			    OUT uint16_t group_ids[3]);
/*
* PARAMETERS
*	p_g2groupid
*		[in] Pointer to the guid2groupid domain
*
*	guid
*		[in] The guid to look for
*
*	group_ids
*		[out] Array of resulting group IDs in host order.
*
* RETURN VALUES
*	0 if successful.
*
* SEE ALSO
* 	osm_db_guid2groupid_set, osm_db_guid2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2groupid_set
* NAME
*	osm_db_guid2groupid_set
*
* DESCRIPTION
*	Set the group ID for the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2groupid_set(IN osm_db_domain_t * p_g2groupid, IN uint64_t guid,
			    IN uint16_t group_ids[3]);
/*
* PARAMETERS
*	p_g2groupid
*		[in] Pointer to the guid2groupid domain
*
*	guid
*		[in] The guid to look for
*
*	group_ids
*		[in] Array of group IDs value to set, in host order
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* 	osm_db_guid2groupid_get, osm_db_guid2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2groupid_delete
* NAME
*	osm_db_guid2groupid_delete
*
* DESCRIPTION
*	Delete the entry by the given guid
*
* SYNOPSIS
*/
int osm_db_guid2groupid_delete(IN osm_db_domain_t * p_g2groupid, IN uint64_t guid);
/*
* PARAMETERS
*	p_g2groupid
*		[in] Pointer to the guid2groupid domain
*
*	guid
*		[in] The guid to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* 	osm_db_guid2groupid_get, osm_db_guid2groupid_set
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2flid_get
* NAME
*	osm_db_guid2flid_get
*
* DESCRIPTION
*	Get a flid by given guid.
*
* SYNOPSIS
*/
int osm_db_guid2flid_get(IN osm_db_domain_t * p_g2fl, IN uint64_t guid,
			 OUT uint16_t * p_flid);
/*
* PARAMETERS
*	p_g2fl
*		[in] Pointer to the guid2flid domain
*
*	guid
*		[in] The guid to look for
*
*	p_flid
*		[out] Pointer to the resulting flid in host order.
*
* RETURN VALUES
*	0 if successful.
*
* SEE ALSO
* osm_db_guid2flid_set, osm_db_guid2flid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2flid_set
* NAME
*	osm_db_guid2flid_set
*
* DESCRIPTION
*	Set a flid by given guid.
*
* SYNOPSIS
*/
int osm_db_guid2flid_set(IN osm_db_domain_t * p_g2fl, IN uint64_t guid,
			 IN uint16_t flid);
/*
* PARAMETERS
*	p_g2fl
*		[in] Pointer to the guid2flid domain
*
*	guid
*		[in] The guid to look for
*
*	flid
*		[in] The flid value to set.
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* osm_db_guid2flid_get, osm_db_guid2flid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2flid_delete
* NAME
*	osm_db_guid2flid_delete
*
* DESCRIPTION
*	Delete the entry by the given guid
*
* SYNOPSIS
*/
int osm_db_guid2flid_delete(IN osm_db_domain_t * p_g2fl, IN uint64_t guid);
/*
* PARAMETERS
*	p_g2fl
*		[in] Pointer to the guid2flid domain
*
*	guid
*		[in] The guid to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* osm_db_guid2flid_get, osm_db_guid2flid_set
*********/

/****s* OpenSM: DB-Pack/osm_db_lid_elem
* NAME
*	osm_db_lid_elem_t
*
* DESCRIPTION
*	An object to have a LID as a key in the domain.
*	Used for both FLIDs and ALIDs.
*
* SYNOPSIS
*/
typedef struct osm_db_lid_elem {
	cl_list_item_t item;
	uint16_t lid;
} osm_db_lid_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*
*	lid
*		the LID value
*
************/

/****f* OpenSM: DB-Pack/osm_db_lid2groupid_lids
* NAME
*	osm_db_lid2groupid_lids
*
* DESCRIPTION
*	Provides back a list of lid elements.
*
* SYNOPSIS
*/
int osm_db_lid2groupid_lids(IN osm_db_domain_t * p_lid2groupid,
			    OUT cl_qlist_t * p_lid_list);
/*
* PARAMETERS
*	p_lid2groupid
*		[in] Pointer to the lid2groupid domain
*
*	p_lid_list
*		[out] A quick list of lid elements of type osm_db_lid_elem_t
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* NOTE: The output qlist should be initialized before calling this function.
*       The caller is responsible for freeing the items in the list, and
*       destroying the list.
*
* SEE ALSO
*	osm_db_lid2groupid_get, osm_db_lid2groupid_set, osm_db_lid2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_lid2groupid_get
* NAME
*	osm_db_lid2groupid_get
*
* DESCRIPTION
*	Get a group id by given lid.
*
* SYNOPSIS
*/
int osm_db_lid2groupid_get(IN osm_db_domain_t * p_lid2groupid, IN uint16_t lid,
			   OUT uint16_t * p_group_id);
/*
* PARAMETERS
*	p_lid2groupid
*		[in] Pointer to the lid2groupid domain
*
*	lid
*		[in] The lid to look for
*
*	p_group_id
*		[out] Pointer to the resulting group ID in host order.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
* 	osm_db_lid2groupid_set, osm_db_lid2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_lid2groupid_set
* NAME
*	osm_db_lid2groupid_set
*
* DESCRIPTION
*	Set a group id for the given lid.
*
* SYNOPSIS
*/
int osm_db_lid2groupid_set(IN osm_db_domain_t * p_lid2groupid, IN uint16_t lid,
			   IN uint16_t group_id);
/*
* PARAMETERS
*	p_lid2groupid
*		[in] Pointer to the lid2groupid domain
*
*	lid
*		[in] The lid to look for
*
*	group_id
*		[in] The group ID value to set, in host order.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
* 	osm_db_lid2groupid_get, osm_db_lid2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_lid2groupid_delete
* NAME
*	osm_db_lid2groupid_delete
*
* DESCRIPTION
*	Delete the entry by the given lid.
*
* SYNOPSIS
*/
int osm_db_lid2groupid_delete(IN osm_db_domain_t * p_lid2groupid, IN uint16_t lid);
/*
* PARAMETERS
*	p_lid2groupid
*		[in] Pointer to the lid2groupid domain
*
*	lid
*		[in] The lid to look for
*
* RETURN VALUES
*	0 if successful,  otherwise 1
*
* SEE ALSO
* 	osm_db_lid2groupid_get, osm_db_lid2groupid_delete
*********/

/****s* OpenSM: DB-Pack/osm_db_port_num_elem
* NAME
*	osm_db_port_num_elem_t
*
* DESCRIPTION
*	An object to hold a port number as key in the domain.
*
* SYNOPSIS
*/
typedef struct osm_db_port_num_elem {
	cl_list_item_t item;
	uint8_t port_num;
} osm_db_port_num_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*
*	port_num
*		the port number
*
************/

/****f* OpenSM: DB-Pack/osm_db_port_numbers
* NAME
*	osm_db_port_numbers
*
* DESCRIPTION
*	Provides back a list of port number elements.
*
* SYNOPSIS
*/
int osm_db_port_numbers(IN osm_db_domain_t * p_domain,
			OUT cl_qlist_t * p_port_numbers_list);
/*
* PARAMETERS
*	p_domain
*		[in] Pointer to the domain in which port numbers are the entries' identifiers.
*
*	p_port_numbers_list
*		[out] A quick list of port number elements of type osm_db_portnum_elem_t
*
* RETURN VALUES
*	0 if successful
*
* NOTE: The output qlist should be initialized before calling this function.
*       The caller is responsible for freeing the items in the list, and
*       destroying the list.
*
* SEE ALSO
*********/

/****f* OpenSM: DB-Pack/osm_db_portnum2groupid_get
* NAME
*	osm_db_portnum2groupid_get
*
* DESCRIPTION
*	Get the group ID for aggregated ports that start on the given prism port number.
*
* SYNOPSIS
*/
int osm_db_portnum2groupid_get(IN osm_db_domain_t * p_portnum2groupid, IN uint8_t port_num,
			       OUT uint16_t * p_group_id);
/*
* PARAMETERS
*	p_portnum2groupid
*		[in] Pointer to the portnum2groupid domain.
*
*	port_num
*		[in] The port number to look for.
*
*	p_group_id
*		[out] Pointer to the resulting group ID in host order.
*
* RETURN VALUES
*	0 if successful.
*
* SEE ALSO
* 	osm_db_portnum2groupid_set, osm_db_portnum2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_portnum2groupid_set
* NAME
*	osm_db_portnum2groupid_set
*
* DESCRIPTION
*	Set the group ID for the given port number.
*
* SYNOPSIS
*/
int osm_db_portnum2groupid_set(IN osm_db_domain_t * p_portnum2groupid, IN uint8_t port_num,
			       IN uint16_t group_id);
/*
* PARAMETERS
*	p_portnum2groupid
*		[in] Pointer to the portnum2groupid domain.
*
*	port_num
*		[in] The port number to look for.
*
*	group_id
*		[in] Group ID value to set, in host order.
*
* RETURN VALUES
*	0 if successful.
*
* SEE ALSO
* 	osm_db_portnum2groupid_get, osm_db_portnum2groupid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_portnum2groupid_delete
* NAME
*	osm_db_portnum2groupid_delete
*
* DESCRIPTION
*	Delete the entry by the given port number
*
* SYNOPSIS
*/
int osm_db_portnum2groupid_delete(IN osm_db_domain_t * p_portnum2groupid, IN uint8_t port_num);
/*
* PARAMETERS
*	p_portnum2groupid
*		[in] Pointer to the portnum2groupid domain.
*
*	port_num
*		[in] The port number to look for.
*
* RETURN VALUES
*	0 if successful, 1 otherwise.
*
* SEE ALSO
* 	osm_db_portnum2groupid_get, osm_db_portnum2groupid_set
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2plane_get
* NAME
*	osm_db_guid2plane_get
*
* DESCRIPTION
*	Get a plane by given switch guid.
*
* SYNOPSIS
*/
int osm_db_guid2plane_get(IN osm_db_domain_t * p_g2plane, IN uint64_t guid,
			  OUT uint8_t * plane);
/*
* PARAMETERS
*	p_guid2plane
*		[in] Pointer to the guid2plane domain
*
*	guid
*		[in] The guid to look for
*
*	plane
*		[out] Pointer to the resulting plane.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
* 	osm_db_guid2plane_set, osm_db_guid2plane_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2plane_set
* NAME
*	osm_db_guid2plane_set
*
* DESCRIPTION
*	Set a plane for the given switch guid.
*
* SYNOPSIS
*/
int osm_db_guid2plane_set(IN osm_db_domain_t * p_g2plane, IN uint64_t guid,
			  IN uint8_t plane);
/*
* PARAMETERS
*	p_guid2plane
*		[in] Pointer to the guid2plane domain
*
*	guid
*		[in] The GUID to look for
*
*	plane
*		[in] The plane value to set
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
* 	osm_db_guid2plane_get, osm_db_guid2plane_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2plane_delete
* NAME
*	osm_db_guid2plane_delete
*
* DESCRIPTION
*	Delete the entry by the given guid.
*
* SYNOPSIS
*/
int osm_db_guid2plane_delete(IN osm_db_domain_t * p_g2plane, IN uint64_t guid);
/*
* PARAMETERS
*	p_guid2plane
*		[in] Pointer to the guid2plane domain
*
*	guid
*		[in] The guid to look for
*
* RETURN VALUES
*	0 if successful,  otherwise 1
*
* SEE ALSO
* 	osm_db_guid2plane_get, osm_db_guid2plane_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_port2rail_get
* NAME
*	osm_db_port2rail_get
*
* DESCRIPTION
*	Get rail according to entry with guid:portnum key
*
* SYNOPSIS
*/
int osm_db_port2rail_get(IN osm_db_domain_t * p_port2rail, IN uint64_t guid,
			 IN uint8_t port_num, OUT uint8_t * rail);
/*
* PARAMETERS
*	p_port2rail
*		[in] Pointer to the port2rail domain
*
*	guid
*		[in] The guid to look for
*
*	port_num
*		[in] The port number to look for
*
*	rail
*		[out] Pointer to the resulting rail.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
*	osm_db_guid_portnum_keys_list,
* 	osm_db_port2rail_set, osm_db_guid_portnum_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_port2rail_set
* NAME
*	osm_db_port2rail_set
*
* DESCRIPTION
*	Set rail for guid:portnum key
*
* SYNOPSIS
*/
int osm_db_port2rail_set(IN osm_db_domain_t * p_port2rail, IN uint64_t guid,
			 IN uint8_t port_num, IN uint8_t rail);
/*
* PARAMETERS
*	p_port2rail
*		[in] Pointer to the port2rail domain
*
*	guid
*		[in] The guid to look for
*
*	port_num
*		[in] The port number to look for
*
*	rail
*		[in]] The rail to set
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
*	osm_db_guid_portnum_keys_list,
* 	osm_db_port2rail_get, osm_db_guid_portnum_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_port2pkey_get
* NAME
*	osm_db_port2pkey_get
*
* DESCRIPTION
*	Get pkey according to entry with guid:portnum key
*
* SYNOPSIS
*/
int osm_db_port2pkey_get(IN osm_db_domain_t * p_port2pkey, IN uint64_t guid,
			 IN uint8_t port_num, OUT uint16_t * pkey);
/*
* PARAMETERS
*	p_port2pkey
*		[in] Pointer to the port2pkey domain
*
*	guid
*		[in] The guid to look for
*
*	port_num
*		[in] The port number to look for
*
*	pkey
*		[out] Pointer to the resulting pkey.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
*	osm_db_guid_portnum_keys_list,
* 	osm_db_port2pkey_set, osm_db_port2pkey_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_port2pkey_set
* NAME
*	osm_db_port2pkey_set
*
* DESCRIPTION
*	Set pkey to entry with guid:portnum key
*
* SYNOPSIS
*/
int osm_db_port2pkey_set(IN osm_db_domain_t * p_port2pkey, IN uint64_t guid,
			 IN uint8_t port_num, IN uint16_t pkey);
/*
* PARAMETERS
*	p_port2pkey
*		[in] Pointer to the port2pkey domain
*
*	guid
*		[in] The guid to look for
*
*	port_num
*		[in] The port number to look for
*
*	pkey
*		[in] Pointer to the resulting pkey.
*
* RETURN VALUES
*	0 if successful, otherwise 1
*
* SEE ALSO
*	osm_db_guid_portnum_keys_list,
* 	osm_db_port2pkey_get, osm_db_port2pkey_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_port2pkey_delete
* NAME
*	osm_db_port2pkey_delete
*
* DESCRIPTION
*	Delete to entry with guid:portnum key
*
* SYNOPSIS
*/
int osm_db_port2pkey_delete(IN osm_db_domain_t * p_domain, IN uint64_t guid,
			    IN uint8_t portnum);
/*
* PARAMETERS
*	p_port2pkey
*		[in] Pointer to the port2pkey domain
*
*	guid
*		[in] The guid to look for
*
*	port_num
*		[in] The port number to look for
*
* RETURN VALUES
*	Delete to entry with guid:portnum key
*
* SEE ALSO
*	osm_db_guid_portnum_keys_list,
* 	osm_db_port2pkey_get, osm_db_port2pkey_set
*********/
END_C_DECLS
