/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *	Module for planarization feature
 *
 * Author:
 *	Gull Sharon, NVIDIA
 *	Julia Levin, NVIDIA
 */

#pragma once

#include <iba/ib_types.h>
#include <complib/cl_types_osd.h>
#include <opensm/osm_log.h>

/*
 * Forward references.
 */
struct osm_port;
struct osm_physp;
struct osm_node;
struct osm_sm;
struct osm_subn;
struct osm_switch;
struct osm_aports_system;

#define OSM_PLANAR_MAX_NUM_OF_ASICS	4

#define OSM_PLANAR_APORT_ID_STR_LEN	1024

/****f* OpenSM: Planarized/osm_planar_get_num_of_planes
* NAME
*	osm_planar_get_num_of_planes
*
* DESCRIPTION
*	The function returns the number of reported planes by p_physp
* SYNOPSIS
*/
uint8_t osm_planar_get_num_of_planes(IN const struct osm_physp *p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp object.
*
* RETURN VALUES
*	Returns the number of reported planes by p_physp.
*
* NOTES
*
* SEE ALSO
*********/

typedef enum osm_physp_plane_symmetric_state {
	OSM_PHYSP_PLANE_SYMMETRIC = 0,
	OSM_PHYSP_PLANE_ASYMMETRIC
} osm_physp_plane_symmetric_state_t;

typedef struct osm_aport {
	struct osm_physp * plane_ports[IB_MAX_NUM_OF_PLANES_XDR + 1];
	uint8_t aport_num;
	uint8_t min_found_plane_number;
	uint8_t max_found_plane_number;
	boolean_t asymmetric;
	struct osm_aports_system * p_aports_system;
	uint8_t min_port_num;
} osm_aport_t;

static inline boolean_t osm_aport_is_valid(IN const osm_aport_t *p_aport)
{
	return (p_aport->min_found_plane_number != 0);
}

typedef enum osm_aports_system_type {
	OSM_APORTS_SYSTEM_TYPE_SWITCH = 0,
	OSM_APORTS_SYSTEM_TYPE_CA,
	OSM_APORTS_SYSTEM_TYPE_MAX
} osm_aports_system_type_t;

/****s* OpenSM: Planarized/osm_aports_system_t
* NAME
*	osm_aports_system_t
*
* DESCRIPTION
*	A structure holding aports uniquely.
*	Represents either a planarized (XDR) switch system, or a planarized HCA node.
*	For switch systems APorts are unique per system (defined by system GUID).
*	For CA systems APorts are unique per CA node (defined per node GUID).
*
* SYNOPSIS
*/
typedef struct osm_aports_system {
	cl_map_item_t map_item;
	ib_net64_t guid_id;
	osm_aports_system_type_t type;
	uint8_t aports_number;
	uint8_t max_aport_available;
	osm_aport_t * aport_table;
} osm_aports_system_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	guid_id
*		The GUID that identifies the aports system of this type.
*		For switch aports systems, this is the system GUID.
*		For CA aports systems, this is the node GUID.
*
*	type
*		The aports system type. It can be either a CA type or a switch type.
*
*	aports_number
*		The size allocated for aport_table.
*
*	max_aport_available
*		The maximum aport found on this aports system.
*		Also the index of said aport in the aport_table.
*
*	aport_table
*		Pointer to the array of aports belonging to this system.
*		See aports_number for array size.
*
* SEE ALSO
*	osm_aport_t.
*********/

/****f* OpenSM: Planarized/osm_aports_system_get_type_str
* NAME
*	osm_aports_system_get_type_str
*
* DESCRIPTION
*	Get string describing aports system type for printing purposes
*
* SYNOPSIS
*/
const char * osm_aports_system_get_type_str(IN const osm_aports_system_t * p_aports_system);
/*
* PARAMETERS
*	p_aports_system
*		Pointer to an aports system object.
*
* RETURN VALUES
*	String describing the aports system type.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_aports_system_get_aport
* NAME
*	osm_aports_system_get_aport
*
* DESCRIPTION
*	Returns the planarized system's APort corresponding to the given APort number,
*	or NULL if no such APort is found.
*
* SYNOPSIS
*/
osm_aport_t * osm_aports_system_get_aport(IN osm_aports_system_t * p_system, IN uint8_t aport_num);
/*
* PARAMETERS
*	p_system
*		Pointer to the APorts system object.
*
*	aport_num
*		The number of the requested APort.
*
* RETURN VALUES
*	Pointer to the planarized system's APort corresponding to the given APort number,
*	or NULL if no such APort is found.
*
* SEE ALSO
*********/

/****s* OpenSM: Planarized/osm_ca_system_t
* NAME
*	osm_ca_system_t
*
* DESCRIPTION
*	A structure representing a planarized (XDR) CA node.
*	APorts on planarized CA system are unique per node GUID and not per system GUID.
*	This structure is a container of APorts for planarized CA.
*
* SYNOPSIS
*/
typedef struct osm_ca_system {
	osm_aports_system_t aports;
	struct osm_node * node;
} osm_ca_system_t;
/*
* FIELDS
*	aports
*		The container of this planarized CA node's aports.  MUST BE FIRST MEMBER!
*
*	node
*		A pointer to the node object the APorts belong to.
*
* SEE ALSO
*	osm_aports_system_t.
*********/

#define OSM_PLANAR_NUM_INTERNAL_FNM_PORTS	2

/****s* OpenSM: Planarized/osm_switch_system_t
* NAME
*	osm_switch_system_t
*
* DESCRIPTION
*	A structure representing a planarized (XDR) switch system.
*
* SYNOPSIS
*/
typedef struct osm_switch_system {
	osm_aports_system_t aports;
	uint8_t min_asic_available;
	uint8_t max_asic_available;
	uint8_t num_internal_fnm_ports;
	struct osm_node * nodes[OSM_PLANAR_MAX_NUM_OF_ASICS + 1];
	uint8_t node_fnm_ports[OSM_PLANAR_MAX_NUM_OF_ASICS + 1][OSM_PLANAR_NUM_INTERNAL_FNM_PORTS];
	struct osm_node* plane_to_system_node[IB_MAX_NUM_OF_PLANES_XDR + 1];
} osm_switch_system_t;
/*
* FIELDS
*	aports
*		The container of this system's aports.  MUST BE FIRST MEMBER!
*
*	min_asic_available
*		The minimal asic found in this system.
*
*	max_asic_available
*		The maximal asic found in this system.
*
*	num_internal_fnm_ports
*		The number of internal FNM ports is used for checking whether the system has a
*		complete FNM ring.
*
*	nodes
*		An array of pointers to the nodes that hold the aports, ordered by the asic
*		number reported in HierarchyInfo.
*		nodes is length OSM_PLANAR_MAX_NUM_OF_ASICS + 1 since asics start at 1.
*
*	plane_to_system_node
*		An array of pointers to the nodes of the system arranged by plane
*		reported on them in HierarchyInfo.
*		This information is used during routing calculation and is relevant
*		for non-prism switches (aka Black Mamba).
*		plane_to_system_node's length is IB_MAX_NUM_OF_PLANES_XDR + 1 since plane start at 1.
*
* SEE ALSO
*	osm_aports_system_t.
*********/

/****f* OpenSM: Planarized/osm_aport_need_update
* NAME
*	osm_aport_need_update
*
* DESCRIPTION
*	Check whether any physp in the aport needs update
*
* SYNOPSIS
*/
boolean_t osm_aport_need_update(IN osm_aport_t * p_aport);
/*
* PARAMETERS
*	p_aport
*		Pointer to aport to check.
*
* RETURN VALUES
*	Whether any physp in the aport needs update.
*
* SEE ALSO
*********/

/****s* OpenSM: Planarized/osm_planarized_data_t
* NAME
*	osm_planarized_data_t
*
* DESCRIPTION
*	This object represents planarization data which is relevant for a osm_port_t object.
*
* SYNOPSIS
*/
typedef struct osm_planarized_data_t {
	ib_end_port_plane_filter_config_t subn_end_port_plane_filter_config;
} osm_planarized_data_t;
/*
* FIELDS
*	subn_end_port_plane_filter_config
*		The EndPortPlaneFilterConfig MAD content configured on the port in the subnet.
*
* SEE ALSO
********/

/****f* OpenSM: Planarized/osm_planar_set_end_port_plane_filter_config
* NAME
*	osm_planar_set_end_port_plane_filter_config
*
* DESCRIPTION
*	Calculate and send EndPortPlaneFilterConfig to HCAs.
*
* SYNOPSIS
*/
void osm_planar_set_end_port_plane_filter_config(IN cl_map_item_t * p_item, IN void * context);
/*
* PARAMETERS
*	p_item
*		A cl_map_item_t representing an osm_port_t to set EndPortPlaneFilterConfig on.
*
*	context
*		A void pointer representing a pointer to the SM.
*
* RETURN VALUES
*
* SEE ALSO
*********/

/****s* OpenSM: Planarized/osm_aports_systems_tables_t
* NAME
*	osm_aports_systems_tables_t
*
* DESCRIPTION
*	This object holds the aports systems tables.
*
* SYNOPSIS
*/
typedef struct osm_aports_systems_tables {
	cl_qmap_t switch_aports_systems;
	cl_qmap_t ca_aports_systems;
} osm_aports_systems_tables_t;
/*
* FIELDS
*	switch_aports_systems
*		Table of pointers to all switch aports system objects in the subnet.
*		Indexed by system GUID, since switch aports systems represent planarized systems.
*
*	ca_aports_systems
*		Table of pointers to all CA aports system objects in the subnet.
*		Indexed by node GUID, since CA aports systems represent CA node.
*
* SEE ALSO
*	osm_aports_system_t, osm_aports_systems_tables_init,
*	osm_aports_systems_tables_destroy.
********/

/****f* OpenSM: Planarized/osm_aports_systems_tables_init
* NAME
*	osm_aports_systems_tables_init
*
* DESCRIPTION
*	Initialize the aports systems tables struct.
*
* SYNOPSIS
*/
void osm_aports_systems_tables_init(IN osm_aports_systems_tables_t * p_aports_systems_tables);
/*
* PARAMETERS
*	p_aports_systems_tables
*		Pointer to the aports systems tables struct to initialize.
*
* RETURN VALUES
*
* SEE ALSO
*	osm_aports_systems_tables_t
*********/

/****f* OpenSM: Planarized/osm_aports_systems_tables_destroy
* NAME
*	osm_aports_systems_tables_destroy
*
* DESCRIPTION
*	Destroy the aports systems tables struct.
*
* SYNOPSIS
*/
void osm_aports_systems_tables_destroy(IN osm_aports_systems_tables_t * p_aports_systems_tables);
/*
* PARAMETERS
*	p_aports_systems_tables
*		Pointer to the aports systems tables struct to destroy.
*
* RETURN VALUES
*
* SEE ALSO
*	osm_aports_systems_tables_t
*********/

/****f* OpenSM: Planarized/osm_switch_get_planarized_system
* NAME
*	osm_switch_get_planarized_system
*
* DESCRIPTION
*	Returns the planarized switch system that contains the given switch,
*	or NULL if no such system is found.
*
* SYNOPSIS
*/
osm_switch_system_t * osm_switch_get_planarized_system(IN struct osm_subn * p_subn,
						       IN struct osm_switch * p_sw);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
*	p_sw
*		Pointer to a switch object.
*
* RETURN VALUES
*	Pointer to the planarized switch system that contains the switch,
*	or NULL if no such system is found.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_node_get_planarized_ca_system
* NAME
*	osm_node_get_planarized_ca_system
*
* DESCRIPTION
*	Returns the planarized CA system that contains the given CA node,
*	or NULL if no such system is found.
*
* SYNOPSIS
*/
osm_ca_system_t * osm_node_get_planarized_ca_system(IN struct osm_subn * p_subn,
						    IN struct osm_node * p_node);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
*	p_node
*		Pointer to a CA node object.
*
* RETURN VALUES
*	Pointer to the planarized CA system that contains the node,
*	or NULL if no such system is found.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_link_is_internal_system_fnm_link
* NAME
*	osm_planar_link_is_internal_system_fnm_link
*
* DESCRIPTION
*	Checks whether both sides of the link are FNM ports belonging to the same planarized switch
*	system.
*
* SYNOPSIS
*/
boolean_t osm_planar_link_is_internal_system_fnm_link(IN struct osm_subn * p_subn,
						      IN struct osm_physp * p_physp);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
*	p_physp
*		Pointer to a physical port on one side of the link.
*
* RETURN VALUES
*	TRUE if the link is internal FNM link in the planarized switch system. FALSE otherwise.
*
* SEE ALSO
*	osm_aports_sets_tables_t
*********/

/****f* OpenSM: Planarized/osm_planar_get_physp_by_plane
* NAME
*	osm_planar_get_physp_by_plane
*
* DESCRIPTION
*	The function receives a physical port and a plane and returns the physical port
*	of the desired plane. If such port doesn't exist returns NULL.
*
* SYNOPSIS
*/
struct osm_physp * osm_planar_get_physp_by_plane(IN struct osm_physp *p_physp, IN uint8_t plane);
/*
* PARAMETERS
*	p_physp
*		Pointer to the physical port object.
*
*	plane
*		The required plane.
*
* RETURN VALUES
*	If the given physical port is planarized and such plane exists in the osm_port_t object,
*	which p_physp is part of, then return the p_physp of the desired plane,
*	otherwise returns NULL.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_get_node_port_plane
* NAME
*	osm_planar_get_node_port_plane
*
* DESCRIPTION
*	Returns the plane number of the physical port at the specified port number.
*	Returns 0 for non-planarized port.
*
* SYNOPSIS
*/
uint8_t osm_planar_get_node_port_plane(IN struct osm_node * p_node, IN uint8_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to the node object.
*
*	port_num
*		[in] Port number to check.
*
* RETURN VALUES
*	Returns the plane of the physical port at the specified port number.
*	Returns 0 for non-planarized port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_get_remote_node_by_plane
* NAME
*	osm_planar_get_remote_node_by_plane
*
* DESCRIPTION
*	Returns a pointer to the node with specified plane on the other end of the specified port.
*	Returns NULL if no such remote node exists.
*
* SYNOPSIS
*/
struct osm_node * osm_planar_get_remote_node_by_plane(IN struct osm_subn * p_subn,
						      IN struct osm_port * p_port,
						      IN uint8_t plane,
						      OUT uint8_t * p_remote_port_num);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
*	p_port
*		Pointer to the osm port object.
*
*	plane
*		The required plane.
*
*	p_remote_port_num
*		[out] Port number in the remote's node through which this
*		link exists.  The caller may specify NULL for this pointer
*		if the port number isn't needed.
*
* RETURN VALUES
*	Returns a pointer to the node with specified plane on the other end of the specified port.
*	Returns NULL if no such remote node exists.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_get_plane_mask
* NAME
*	osm_planar_get_plane_mask
*
* DESCRIPTION
* 	Returns the mask of the enabled planes on the give physical port.
* 	For non planarized ports it would be zero.
*
* SYNOPSIS
*/
uint8_t osm_planar_get_plane_mask(IN struct osm_physp *p_physp);
/*
* PARAMETERS
*	p_physp
*		Pointer to the physical port object.
*
* RETURN VALUES
* 	Returns the mask of the enabled planes on the give physical port.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_is_link_asymmetric
* NAME
*	osm_planar_is_link_asymmetric
*
* DESCRIPTION
* 	Returns TRUE if the link is asymmetric
* 	For non planarized ports it would be FALSE.
*
* SYNOPSIS
*/
boolean_t osm_planar_is_link_asymmetric(IN struct osm_subn * p_subn,
					IN struct osm_physp * p_physp,
                                        OUT boolean_t * p_is_port_asymmetric);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*	p_physp
*		[in] Pointer to the physical port object.
*	p_is_port_asymmetric
*		[out] if non-NULL, TRUE if this side of link is asymmetric
*
* RETURN VALUES
* 	TRUE if link is asymmetric (from either side)
*	FALSE otherwise
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_are_all_switches_same_system
* NAME
*	osm_planar_are_all_switches_same_system
*
* DESCRIPTION
* 	Returns TRUE if all the switches in the fabric belong to the same planarized system.
*
* SYNOPSIS
*/
boolean_t osm_planar_are_all_switches_same_system(IN struct osm_subn * p_subn);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
* RETURN VALUES
* 	Returns TRUE if all the switches in the fabric belong to the same planarized system,
* 	FALSE otherwise.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_disable_ar_to_switches_on_same_system
* NAME
*	osm_planar_disable_ar_to_switches_on_same_system
*
* DESCRIPTION
* 	Disables advanced routing (AR) to destination LIDs of switches on the same
* 	planarized non-prism switch system (ex. Black Mamba)
*
* SYNOPSIS
*/
void osm_planar_disable_ar_to_switches_on_same_system(IN struct osm_subn * p_subn);
/*
* PARAMETERS
*	p_subn
*		Pointer to the subnet object
*
* RETURN VALUES
* 	None
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_analyze_aports
* NAME
*	osm_planar_analyze_aports
*
* DESCRIPTION
*	This function analyzes the aports for asymmetricity.
*
* SYNOPSIS
*/
void osm_planar_analyze_aports(IN OUT struct osm_sm * sm);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_decrease_system_fnm_links_number
* NAME
*	osm_planar_decrease_system_fnm_links_number
*
* DESCRIPTION
*	This function decreases counter of internal fnm links system has.
*	It's used in the routing stage.
*
* SYNOPSIS
*/
void osm_planar_decrease_system_fnm_links_number(IN OUT struct osm_sm * sm,
						 IN struct osm_node *p_node);
/*
* PARAMETERS
*	sm
*		[in/out] Pointer to an osm_sm_t object.
*	p_node
*		[in] A pointer to a switch node
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_is_same_non_prism_system
* NAME
*	osm_planar_is_same_non_prism_system
*
* DESCRIPTION
*	This function determines if the switch and a switch with a given guid are asics on the same
*	non-prism (a.k.a Black Mamba) system.
*
* SYNOPSIS
*/
boolean_t osm_planar_is_same_non_prism_system(IN struct osm_subn *p_subn,
					      IN struct osm_switch *p_sw,
					      IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn object.
*	p_sw
*		[in] Pointer to a switch object
*	guid
*		[in] A GUID of another switch
*
* RETURN VALUES
*	This function returns TRUE if the switch and a switch with given 'guid' are asics on the same
*	non-prism (a.k.a Black Mamba) system.
*	FALSE otherwise.
* NOTES
*
* SEE ALSO
*********/

void osm_planar_set_fnm_port(IN struct osm_subn *p_subn,
			     IN ib_net64_t guid_id,
			     IN int asic,
			     IN uint8_t fnm_port);

boolean_t osm_planar_is_fnm_ring_broken(IN osm_switch_system_t *p_sw_system);

void osm_planar_add_fnm_routing(IN struct osm_sm * sm);

boolean_t osm_planar_is_prism_sw_system(osm_switch_system_t *p_sw_system);

void osm_planar_add_systems_routing(IN struct osm_sm * sm);

/****f* OpenSM: Planarized/osm_planar_port_routable_in_plane
* NAME
*	osm_planar_port_routable_in_plane
*
* DESCRIPTION
*	Check whether this port can pass unicast traffic received on the specified plane.
*	Prism switch can receive traffic in all 4 planes, but it can also have 2-planes hosts or
*	legacy switches/hosts connected to it.
*	In such case traffic can flow through the prism switch from the 4 planes into 2 planes or
*	into non-planarized ports, according to the following logic:
*	- Ports can pass traffic received on plane X if they also belong to plane X.
*	- Non-planarized ports can pass traffic received on all planes.
*	- Ports belonging to 2-planes APort that belong to plane X can pass traffic received on
*	  plane X and on plane X+2. I.e. 1 <-> 1 or 3 and 2 <-> 2 or 4.
*	This logic is derived from the way FW set up the entry plane filter, which maps the plane
*	the traffic entered through into the planes it can exit through. The entry plane filter is
*	configured by FW, and its logic is mimicked here.
*	Note that this only affects unicast routing. Multicast routing uses different logic.
*
* SYNOPSIS
*/
boolean_t osm_planar_port_routable_in_plane(IN struct osm_node * p_node,
					    IN uint32_t port_num,
					    IN uint8_t plane);
/*
* PARAMETERS
*	p_node
*		[in] A pointer to a node
*	port_num
*		[in] The port number of the port on the node
*	plane
*		[in] The plane being asked about
*
* RETURN VALUES
*	Returns TRUE if the plane's traffic can flow through the port, and FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_port_connectivity_correct
* NAME
*	osm_planar_port_connectivity_correct
*
* DESCRIPTION
*	Check whether this port's APort is connected correctly, meaning:
*	- Both sides of the link are planarized, or both are unplanarized.
*	- If planarized, the APorts of both sides of the link are connected plane to plane (plane 1
*	  to plane 1, etc).
*	Note that this function should be symmetric on the link. Calling it on either side should
*	return the same result.
*
* SYNOPSIS
*/
boolean_t osm_planar_port_connectivity_correct(IN struct osm_node * p_node, IN uint32_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] A pointer to a node
*	port_num
*		[in] The port number of the port on the node
*
* RETURN VALUES
*	Returns TRUE if the Port's APort is connected correctly, FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_is_planarized_switch
* NAME
*	osm_planar_is_planarized_switch
*
* DESCRIPTION
*	Check whether this switch is a planarized switch.
*
* SYNOPSIS
*/
boolean_t osm_planar_is_planarized_switch(IN struct osm_subn *p_subn, IN struct osm_switch *p_sw);
/*
* PARAMETERS
*	p_subn
*		[in] A pointer to the subnet
*	p_sw
*		[in] A pointer to the switch
*
* RETURN VALUES
*	Returns TRUE if this is a planarized switch, FALSE otherwise.
*
* NOTES
*	Note that this function can be called only after APorts were built.
*	This function can return True also for invalid asic of planarized non-prism switch, that doesn't have a hierarchy info ,
*	but it belongs to planarized system, where other asics reported valid hierarchy info.
*
* SEE ALSO
*********/

/****f* OpenSM: Planarized/osm_planar_get_aport_identifier_string
* NAME
*	osm_planar_get_aport_identifier_string
*
* DESCRIPTION
*	Get APort string identifier in format APort <num> on <HCA/Switch> System ID <GUID> (port label).
*
* SYNOPSIS
*/
char* osm_planar_get_aport_identifier_string(IN osm_aport_t *p_aport,
					     IN struct osm_physp *p_physp,
					     OUT char *p_aport_str,
					     IN size_t len);
/*
* PARAMETERS
*	p_aport
*		[in] A pointer to the APort object.
*	p_physp
*		[in] A pointer to the osm_physp object, that is one of plane ports in the given APort.
*	p_aport_str
*		[out] Pointer to a buffer to store the APort string identifier.
*	len
*		[in] Length of the buffer.
*
* RETURN VALUES
*	Return the pointer to the received buffer for APort string identifier.
*
* SEE ALSO
*********/
