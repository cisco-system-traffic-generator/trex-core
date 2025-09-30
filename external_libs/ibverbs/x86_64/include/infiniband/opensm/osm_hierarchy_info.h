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

/*
 * Abstract:
 * 	Declaration of hierarchy information and planarization related functionality.
 *
 * Author: Julia Levin, Nvidia
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
/*
 * Forward references.
 */
struct osm_port;
struct osm_physp;
struct osm_node;
struct osm_sm;
struct osm_switch;

/*
 * Template GUID that describes Hierarchy Info for NDR generation switches and HCA.
 * This template GUID specified in the architecture specification "Hierarchy Information Use-Cases"
 */
#define OSM_HI_TEMPLATE_GUID_NDR	3

typedef enum osm_hierarchical_info_template_guid_ndr {
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_SPLIT = 0,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_PORT,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_CAGE,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_ASIC,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_SLOT,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_TYPE,
	HI_TEMPLATE_GUID_NDR_RECORD_SELECTOR_BDF
} osm_hierarchical_info_template_guid_ndr_t;

/*
 * Template GUID that describes Hierarchy Info for XDR generation switches and HCA.
 * This template GUID specified in the architecture specification
 * "Hierarchy Information for planarize topology".
 */
#define OSM_HI_TEMPLATE_GUID_4		4

#define OSM_HI_TEMPLATE_GUID_5		5

typedef enum osm_hierarchical_info_template_guid_4 {
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_PORT_TYPE = 0,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_ASIC_NAME,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_ACCESS_PORT_IB_PORT,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_SWITCH_CAGE,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_IPIL,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_SPLIT,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_ASIC,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_TYPE = 8,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_IS_CAGE_MANAGER,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_PLANE,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_NUM_OF_PLANES,
	HI_TEMPLATE_GUID_4_RECORD_SELECTOR_APORT
} osm_hierarchical_info_template_guid_4_t;

typedef enum osm_hierarchical_info_template_guid_5 {
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_PORT_TYPE = 0,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_NUM_ON_BASE_BOARD,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_HARDWARE_PORT_NUM,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_CAGE,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_IPIL,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_SPLIT,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_BDF = 9,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_PLANE,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_NUM_OF_PLANES,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_APORT,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_DEVICE_RECORD,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_BOARD_RECORD,
	HI_TEMPLATE_GUID_5_RECORD_SELECTOR_SYSTEM_RECORD
} osm_hierarchical_info_template_guid_5_t;

#define OSM_HI_TEMPLATE_GUID_6		6

/*
 * Template GUID #6 is reported by switch port 0 only.
 * GPUs and HCA will not report template GUID #6.
 * Instead, the same records are added to template GUID #5.
 * */
typedef enum osm_hierarchical_info_template_guid_6 {
	HI_TEMPLATE_GUID_6_RECORD_SELECTOR_DEVICE_RECORD = 0,
	HI_TEMPLATE_GUID_6_RECORD_SELECTOR_BOARD_RECORD,
	HI_TEMPLATE_GUID_6_RECORD_SELECTOR_SYSTEM_RECORD
} osm_hierarchical_info_template_guid_6_t;

#define OSM_PORT_LABEL_LEN				100

typedef struct osm_hierarchy_info {
	ib_hierarchy_info_t block;
	boolean_t is_planarized;
	char port_label[OSM_PORT_LABEL_LEN];
	int aport;
	int plane_number;
	int num_of_planes;
	int asic;
	int port_location_type;
	int port_type;
	int ib_port;
	int hardware_port_num;
	int cage;
	int ipil;
	int split;
	osm_aport_t *p_aport;
} osm_hierarchy_info_t;

typedef struct osm_node_hierarchy_info {
	boolean_t is_node_hierarchy_reset;
	int device_num_on_cpu_node;
	int cpu_node_number;
	int board_type;
	int chassis_slot_index;
	int tray_index;
	int topology_id;
} osm_node_hierarchy_info_t;

/****f* OpenSM: Hierarchy Information/osm_get_hierarchy_info
* NAME
*	osm_get_hierarchy_info
*
* DESCRIPTION
*	Get the hierarchy info MAD for specific port.
*
* SYNOPSIS
*/
void osm_get_hierarchy_info(IN struct osm_sm * sm, IN struct osm_node * p_node,
			    IN struct osm_port * p_port,
			    IN struct osm_physp * p_physp,
			    IN uint8_t index);
/*
* PARAMETERS
*	p_sm
*		[in] Pointer to an osm_sm_t object.
*
*	p_node
*		[in] Pointer to the parent Node object of this Physical Port.
*
*	p_port
*		[in] Pointer to a pointer to a Port object of this Physical port.
*
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* 	index
* 		[in] hierarchy index of the block to get.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_info_init
* NAME
*	osm_hierarchy_info_init
*
* DESCRIPTION
*	Initialize the hierarchy info structure.
*
* SYNOPSIS
*/
static inline void osm_hierarchy_info_init(IN OUT osm_hierarchy_info_t* p_hi)
{
	p_hi->p_aport = NULL;
	p_hi->is_planarized = FALSE;
	p_hi->port_type = -1;
	p_hi->ib_port = -1;
	p_hi->hardware_port_num = -1;
	p_hi->cage = -1;
	p_hi->ipil = -1;
	p_hi->split = -1;
	p_hi->aport = -1;
	p_hi->plane_number = -1;
	p_hi->num_of_planes = -1;
	p_hi->asic = -1;
	p_hi->port_location_type = -1;
}
/*
* PARAMETERS
*	p_hi
*		[in] Pointer to an osm_hierarchy_info_t object.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_node_hierarchy_info_clear_reset_bit
* NAME
*	osm_node_hierarchy_info_clear_reset_bit
*
* DESCRIPTION
*	Clear the hierarchy info reset bit from all the nodes.
*	The hierarchy_info_reset bit indicates that the hierarchy_info for a specific node was cleared.
*	This field is needed because hierarchy info is sent per port,
*	but the hierarchy info for the node is the same for all the ports.
*	Therefore, it is necessary to reset the hierarchy node data only once before sending
*	the MAD to the first port of the node.
*	This function sets the value of the field on all nodes to FALSE.
*
* SYNOPSIS
*/
void osm_node_hierarchy_info_clear_reset_bit(IN cl_map_item_t * p_map_item, IN void *context);
/*
* PARAMETERS
*	p_map_item
*		[in] Node map item.
*
*	context
*		[in] A pointer to a context of osm_sm_t struct.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_node_hierarchy_info_init
* NAME
*	osm_node_hierarchy_info_init
*
* DESCRIPTION
*	Initialize the node hierarchy info structure.
*
* SYNOPSIS
*/
static inline void osm_node_hierarchy_info_init(IN OUT osm_node_hierarchy_info_t* p_hi)
{
	p_hi->device_num_on_cpu_node = -1;
	p_hi->cpu_node_number = -1;
	p_hi->board_type = -1;
	p_hi->chassis_slot_index = -1;
	p_hi->tray_index = -1;
	p_hi->topology_id = -1;
	p_hi->is_node_hierarchy_reset = TRUE;
}
/*
* PARAMETERS
*	p_hi
*		[in] Pointer to an osm_node_hierarchy_info_t object.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_build_aports
* NAME
*	osm_build_aports
*
* DESCRIPTION
*	This function builds aports systems and their aports based on hierarchy info in the ports.
*
* SYNOPSIS
*/
void osm_build_aports(IN OUT struct osm_sm * sm);
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

/****f* OpenSM: Hierarchy Information/osm_hierarchy_is_planarized_physp
* NAME
*	osm_hierarchy_is_planarized_physp
*
* DESCRIPTION
*	This function returns TRUE if the port has a valid planarization information.
*
* SYNOPSIS
*/
boolean_t osm_hierarchy_is_planarized_physp(IN const struct osm_physp * p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp object.
*
* RETURN VALUES
*	TRUE if the physical port has a valid planarized information, FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_physp_get_aport
* NAME
*	osm_physp_get_aport
*
* DESCRIPTION
*	This function returns the aport that contains the physp,
*	or NULL if the physp is not part of an aport.
*
* SYNOPSIS
*/
osm_aport_t * osm_physp_get_aport(IN struct osm_physp * p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp object.
*
* RETURN VALUES
*	Pointer to an aport object containing the physp, if one exists. NULL otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_aport_get_remote
* NAME
*	osm_aport_get_remote
*
* DESCRIPTION
*	Returns a pointer to the aggregated port on the other side of the wire(s).
*
* SYNOPSIS
*/
osm_aport_t * osm_aport_get_remote(IN osm_aport_t * p_aport);
/*
* PARAMETERS
*	p_aport
*		[in] Pointer to an aport object.
*
* RETURN VALUES
*	Pointer to the aggregated port on the other side of the wire(s),
*	or NULL if there is no remote.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_aport_get_aports_system
* NAME
*	osm_aport_get_aports_system
*
* DESCRIPTION
*	Returns a pointer to the aports system the aport belongs to.
*
* SYNOPSIS
*/
osm_aports_system_t * osm_aport_get_aports_system(IN osm_aport_t * p_aport);
/*
* PARAMETERS
*	p_aport
*		[in] Pointer to an aport object.
*
* RETURN VALUES
*	Pointer to the aports system the aport belongs to
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_aport_get_prism_switch
* NAME
*	osm_aport_get_prism_switch
*
* DESCRIPTION
*	Returns a pointer to the prism switch the aport belongs to.
*
* SYNOPSIS
*/
struct osm_switch * osm_aport_get_prism_switch(IN osm_aport_t * p_aport);
/*
* PARAMETERS
*	p_aport
*		[in] Pointer to an aport object.
*
* RETURN VALUES
*	Pointer to the prism switch the aport belongs to.
*	Returns NULL if the aport does not belong to a prism switch (for example, if the system
*	contains no prism switches).
*
* NOTES
*	A prism system consists of multiple switches, but each aport on a prism system is located
*	wholly on a single switch.
*
* SEE ALSO
*********/

/****d* OpenSM: Hierarchy Information/osm_aport_iter_t
* NAME
*	osm_aport_iter_t
*
* DESCRIPTION
*	An iterator over an osm_aports_system_t's valid aports.
*
* SYNOPSIS
*/
typedef osm_aport_t* osm_aport_iter_t;
/*********/

/****f* OpenSM: Hierarchy Information/osm_aports_system_begin
* NAME
*	osm_aports_system_begin
*
* DESCRIPTION
*	Returns an iterator to iterate over the aports system's valid aports.
*
* SYNOPSIS
*/
osm_aport_iter_t osm_aports_system_begin(IN osm_aports_system_t * p_aports_system);
/*
* PARAMETERS
*	p_aports_system
*		[in] Pointer to the aports system object whose aports need to be iterated over.
*
* RETURN VALUE
*	An aport iterator for iterating over the aports system's valid aports.
*
* NOTES
*
* SEE ALSO
*	osm_aport_iter_t.
*********/

/****f* OpenSM: Hierarchy Information/osm_aport_iter_next
* NAME
*	osm_aport_iter_next
*
* DESCRIPTION
*	Advances the iterator.
*
* SYNOPSIS
*/
osm_aport_iter_t osm_aport_iter_next(IN osm_aport_iter_t aport_iter);
/*
* PARAMETERS
*	aport_iter
*		[in] The aport iterator.
*
* RETURN VALUE
*	An iterator pointing at the next valid aport in the aports system, or an iterarator
*	representing the end of the aports system's aports if there's no next valid aport.
*
* NOTES
*
* SEE ALSO
*	osm_aport_iter_t.
*********/

/****f* OpenSM: Hierarchy Information/osm_aports_system_end
* NAME
*	osm_aports_system_end
*
* DESCRIPTION
*	Returns an osm_aport_iter_t representing the end of the aports system's aports.
*
* SYNOPSIS
*/
osm_aport_iter_t osm_aports_system_end(IN osm_aports_system_t * p_aports_system);
/*
* PARAMETERS
*	p_aports_system
*		[in] Pointer to the aports system.
*
* RETURN VALUE
*	osm_aport_iter_t representing the end of the aports system's aports.
*
* NOTES
*
* SEE ALSO
*	osm_aport_iter_t.
*********/

/****f* OpenSM: Hierarchy Information/osm_aports_system_delete
* NAME
*	osm_aports_system_delete
*
* DESCRIPTION
*	Destroys and deallocates the object.
*
* SYNOPSIS
*/
void osm_aports_system_delete(IN OUT osm_aports_system_t ** p_aports_system);
/*
* PARAMETERS
*	p_aports_system
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*	Aports system object, osm_aports_system_new
*********/

static inline boolean_t osm_hierarchy_is_asymmetric_aport(IN osm_aport_t *p_aport)
{
	return p_aport->asymmetric;
}

boolean_t osm_hierarchy_is_asymmetric_aport_physp(IN struct osm_physp *p_physp);

/****f* OpenSM: Hierarchy Information/osm_build_system_ar_group
* NAME
*	osm_build_system_ar_group
*
* DESCRIPTION
*	Builds routing from prism switch towards non-prism (ex. Black Mamba) Leaf's HCAs.
*	This functionality is applied on routing tables after routing engine calculations.
*
* SYNOPSIS
*/
void osm_build_system_ar_group(IN osm_subn_t *p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_build_planarized_hcas_ar_groups
* NAME
*	osm_build_planarized_hcas_ar_groups
*
* DESCRIPTION
*	Builds routing from prism switches towards their own planarized HCAs.
*	This functionality is applied on routing tables after routing engine calculations.
*
* SYNOPSIS
*/
void osm_build_planarized_hcas_ar_groups(IN osm_subn_t * p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object
*
* RETURN VALUE
*	None.
*
* NOTES
*	In order to support multi-plane transmittions and avoid packet loss, a prism switch
*	transmitting to a planarized HCA connected directly to it needs an AR group that includes
*	all the ports in the APort toward that HCA.
*	This function sets up AR groups for that purpose.
*	To avoid using up too much AR group IDs, they are reused on different switches,
*	using the following logic:
*	Each APort is an aggregation of multiple physical ports. On the prism switch, the APort can
*	be uniquely represented by the port number of the physp with the lowest port number in that
*	APort.
*	The AR group IDs are allocated for each port number that is used to represent any such
*	APort, and are used for all physical ports of these APorts.
*	e.g. if SW-1 has an APort on ports 5-8 for H-1, and SW-2 has an APort on ports 5-8 for H-2,
*	the representative port number for both these APorts is 5, so a single AR group ID will be
*	allocated for port number 5, and this group ID will be used both for routing from SW-1 to
*	H-1 and from SW-2 to H-2.
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_is_planarized
* NAME
*	osm_hierarchy_is_planarized
*
* DESCRIPTION
*	Function returns whether a given switch is planarized
*
* SYNOPSIS
*/
boolean_t osm_hierarchy_is_planarized(IN struct osm_switch *p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object
*
* RETURN VALUES
*	Returns TRUE if the switch is planarized
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_get_sw_by_plane
* NAME
*	osm_hierarchy_get_sw_by_plane
*
* DESCRIPTION
*	Function returns a pointer to a switch that has a given plane
*	from the same system as the given 'p_sw' switch.
*
* SYNOPSIS
*/
struct osm_switch *
osm_hierarchy_get_sw_by_plane(osm_subn_t *p_subn, struct osm_switch *p_sw, uint8_t plane);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to osm_subn_t object
*	p_sw
*		[in] Pointer to the switch object
*
*	plane
*		[in] a required plane to search for
*
* RETURN VALUES
*	Returns a pointer to a switch that has a given plane
*	from the same system as the given 'p_sw' switch.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_get_sw_port0_by_plane
* NAME
*	osm_hierarchy_get_sw_port0_by_plane
*
* DESCRIPTION
*	Function returns a switch port 0 with plane equal to input plane,
*	from the same system as the input switch port.
*	In case such asic doesn't exist, returns NULL.
*
* SYNOPSIS
*/
struct osm_port *
osm_hierarchy_get_sw_port0_by_plane(osm_subn_t *p_subn, struct osm_port *p_sw_port, uint8_t plane);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
*	p_sw_port
*		[in] Pointer to the switch port of planarized system.
*
*	plane
*		[in] A required plane to search for on the planarized system.
*
* RETURN VALUES
*	Returns switch port on the same system with the given plane.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_get_an_by_plane
* NAME
*	osm_hierarchy_get_an_by_plane
*
* DESCRIPTION
*	The function receives a supposedly planarized switch and a plane,
*	and returns aggregation node port of switch in the same system with desired plane.
* SYNOPSIS
*/
struct osm_port *
osm_hierarchy_get_an_by_plane(osm_subn_t * p_subn,
			      struct osm_switch *p_sw,
			      struct osm_port *p_an_port,
			      uint8_t plane);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
*	p_sw
*		[in] Pointer to the switch port of planarized system.
*
*	p_an_port
*		[in] Pointer to Aggregation Node (AN) connected to the given switch
*
*	plane
*		[in] A required plane to search for on the planarized system.
*
* RETURN VALUES
*	Returns Aggregation Node (AN) port on the same system
*	connected to the switch with the given plane.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Hierarchy Information/osm_hierarchy_get_cage
* NAME
*	osm_hierarchy_get_cage
*
* DESCRIPTION
*	The function returns the number of reported planes by p_physp
* SYNOPSIS
*/
int osm_hierarchy_get_cage(IN struct osm_physp *p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp object.
*
* RETURN VALUES
*	Returns cage of p_physp.
*
* NOTES
*
* SEE ALSO
*********/

boolean_t osm_hierarchy_is_port_allowed_to_route(IN osm_subn_t *p_subn,
						 IN struct osm_switch *p_sw,
						 IN struct osm_physp *p_physp,
						 IN struct osm_switch *p_other_sw);

/****f* OpenSM: Hierarchy Information/osm_hierarchy_get_port_label
* NAME
*	osm_hierarchy_get_port_label
*
* DESCRIPTION
*	Function fills in the port label string for printing of a given physp
*	if the physp exists and has a valid port label.
*	If the physp does not exist or has no valid port label, the port label string
*	is set to an empty string.
*
* SYNOPSIS
*/
char* osm_hierarchy_get_port_label(IN struct osm_physp *p_physp,
				   OUT char *p_port_label,
				   IN size_t len,
				   boolean_t get_brackets);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp object.
*	p_port_label
*		[out] Pointer to a buffer to store the port label.
*	len
*		[in] Length of the buffer.
*	get_brackets
*		[in] Whether to get the port label with brackets.
*
* RETURN VALUES
*	Return the pointer to the received buffer for port label.
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
