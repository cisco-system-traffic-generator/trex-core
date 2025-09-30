/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/*
 * Abstract:
 * 	Declaration of osm_node_t.
 *	This object represents an IBA node.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <complib/cl_qmap.h>
#include <iba/ib_types.h>
#include <opensm/osm_base.h>
#include <opensm/osm_port.h>
#include <opensm/osm_path.h>
#include <opensm/osm_madw.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

struct osm_switch;

/****h* OpenSM/Node
* NAME
*	Node
*
* DESCRIPTION
*	The Node object encapsulates the information needed by the
*	OpenSM to manage nodes.  The OpenSM allocates one Node object
*	per node in the IBA subnet.
*
*	The Node object is not thread safe, thus callers must provide
*	serialization.
*
*	This object should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****d* OpenSM: Node/physp_ignored_t
* NAME
*       physp_ignored_t
*
* DESCRIPTION
*       Enumerates the possible states of physp_ignored.
*
* SYNOPSIS
*/
typedef enum _physp_ignored_t {
	OSM_PHYSP_NORMAL = 0,
	OSM_PHYSP_IGNORE_NEW,
	OSM_PHYSP_IGNORE_EXISTING
} physp_ignored_t;
/***********/

/****d* OpenSM: Node/ignore_port_t
* NAME
*       ignore_port_t
*
* DESCRIPTION
*       Enumerates the possible types of ignored port.
*
* SYNOPSIS
*/
typedef enum osm_ignore_port_t {
	OSM_IGNORE_PORT_NONE = 0,
	OSM_IGNORE_PORT_ISOLATE,
	OSM_IGNORE_PORT_HELDBACK,
	OSM_IGNORE_PORT_NO_DISCOVER,
	OSM_IGNORE_PORT_TOPOCONFIG_MISMATCH
} osm_ignore_port_t;
/***********/

typedef struct osm_conf_params_node {
	uint8_t sharp_enabled;
	ib_qos_config_vl_t qos_config_vl;
	boolean_t is_qos_config_vl_parsed;
	boolean_t is_port_recovery_policy_parsed;
} osm_conf_params_node_t;

typedef struct osm_n2n_node {
	uint8_t num_blocks;
	ib_neighbors_info_t *p_neighbors_info_data;
} osm_n2n_node_data_t;

/****d* OpenSM: Node/osm_rediscovery_flags_t
* NAME
*	osm_rediscovery_flags_t
*
* DESCRIPTION
*	Flags describing the stages of node rediscovery.
*	During discovery, if a discovery MAD fails, the SM should re-attempt node discovery
*	using a different path, in case the issue is with the path.
*	This is applicable only to switches.
*
* SYNOPSIS
*/
typedef enum osm_node_rediscovery_flags {
	OSM_REDISCOVERY_NEEDED = (1 << 0),
	OSM_REDISCOVERY_FOUND_PATH = (1 << 1),
} osm_node_rediscovery_flags_t;
/***********/

/****s* OpenSM: Node/osm_node_t
* NAME
*	osm_node_t
*
* DESCRIPTION
*	Node structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_node {
	cl_map_item_t map_item;
	struct osm_switch *sw;
	struct osm_gpu *p_gpu;
	ib_node_info_t node_info;
	ib_node_desc_t node_desc;
	uint32_t discovery_count;
	uint32_t physp_tbl_size;
	char *print_desc;
	uint8_t *physp_discovered;
	physp_ignored_t *physp_ignored;
	osm_physp_plane_symmetric_state_t *physp_plane_symmetric_state;
	osm_ignore_port_t *remote_ignore_type;
	ib_general_info_smp_t general_info;
	ib_mlnx_ext_node_info_t ext_node_info;
	boolean_t ext_node_info_updated;
	ib_mlnx_ext_node_info_t new_ext_node_info;
	boolean_t gi_updated;
	ib_cc_enh_info_t cc_enh_info;
	osm_conf_params_node_t conf_params;
	boolean_t force_drop_node;
	uint8_t rediscovery_bitmask;
	uint8_t node_penalty;
	osm_n2n_node_data_t n2n_data;
	uint64_t state_timestamp_microsec;
	osm_node_hierarchy_info_t hierarchy_info;
	ib_chassis_info_t chassis_info;
	boolean_t chassis_info_updated;
	osm_physp_t physp_table[1];
} osm_node_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	sw
*		For switch node contains pointer to appropriate osm_switch
*		structure. NULL for non-switch nodes. Can be used for fast
*		access to switch object and for simple node type detection
*
*	node_info
*		The IBA defined NodeInfo data for this node.
*
*	node_desc
*		The IBA defined NodeDescription data for this node.
*
*	discovery_count
*		The number of times this node has been discovered
*		during the current fabric sweep.  This number is reset
*		to zero at the start of a sweep.
*
*	phsyp_tbl_size
*		The size of the physp_table array.  This value is one greater
*		than the number of ports in the node, since port numbers
*		start with 1 for some bizarre reason.
*
*	print_desc
*		A printable version of the node description.
*
*	physp_discovered
*		Array of physp_discovered objects for all ports of this node.
*		Each object indiactes whether the port has been discovered
*		during the sweep or not. 1 means that the port had been discovered.
*
*
*	physp_ignored
*		Array of physp_ignored objects for all ports of this node.
*		Each object indicates whether the port should be ignored from the
*		unicast routing or not.
*
*	remote_ignore_type
*		Array of port_ignore_type objects to indicate if the remote port
*		is ignored (and which type of ignored port if so)
*
*	general_info
*		general info data, used for capability mask discovery
*
*	gi_updated
*		Indication whether general info updated for the node
*
*	ext_node_info
*		Mellanox vendor specific ExtendedNodeInfo data for this node.
*
*	ext_node_info_updated
*		Indicator, indicates that the node has updated copy of
*		ext_node_info.
*
*	new_ext_node_info
*		newly calculated Mellanox vendor specific ExtendedNodeInfo data for this node.
*
*	conf_params
*		Values for parameters that are prioritized for the ports belong
*		to this node.
*
*	force_drop_node
*		Indication whether to force drop_mgr to drop the node.
*		Currently activated when port info validation fails while OpenSM is in MASTER state.
*
*	rediscovery_bitmask
*		A bitmask representing the state of node rediscovery.
*		During discovery, if there's a timeout on discovery MAD, the SM should attempt
*		rediscovery of that node using a different path, in case the issue is with the path.
*
*	node_penalty
*		Penalty of node due to timeouts of discovery MADs that passed through the port.
*		Used to avoid suspect nodes when doing rediscovery.
*
*	n2n_data
*		Structure used for saving latest NeighborInfo data that was configured on
*		this node, in order to check if should resent on next calculation.
*
*	state_timestamp_microsec
*	        Timestamp of the last time of the ports changed its state
*
*	hierarchy_info
*		The node data from hierarchy information defined in IBA.
*
*	chassis_info
*		ChassisInfo data for this node.
*
*	chassis_info_updated
*		Indicator, indicates that the node has updated copy of
*		chassis_info.
*
*	phsyp_table
*		Array of physical port objects belonging to this node.
*		Index is contiguous by local port number.
*		For switches, port 0 is the always the management port (14.2.5.6).
*		MUST BE LAST MEMBER! - Since it grows !!!!
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Switch/osm_node_name
* NAME
*	osm_node_name
*
* DESCRIPTION
*	Get the 'name' of the node.
*
* SYNOPSIS
*/
static inline char * osm_node_name(IN osm_node_t * p_node)
{
	return p_node->print_desc;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the object.
*
* RETURN VALUE
*	The nodeInfo description.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_delete
* NAME
*	osm_node_delete
*
* DESCRIPTION
*	The osm_node_delete function destroys a node, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_node_delete(IN OUT osm_node_t ** p_node);
/*
* PARAMETERS
*	p_node
*		[in][out] Pointer to a Pointer a Node object to destroy.
*		On return, the pointer to set to NULL.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified Node object.
*	This function should only be called after a call to osm_node_new.
*
* SEE ALSO
*	Node object, osm_node_new
*********/

/****f* OpenSM: Node/osm_node_new
* NAME
*	osm_node_new
*
* DESCRIPTION
*	The osm_node_new function initializes a Node object for use.
*
* SYNOPSIS
*/
osm_node_t *osm_node_new(IN const osm_madw_t * p_madw, uint8_t sm_port_idx);
/*
* PARAMETERS
*	p_madw
*		[in] Pointer to a osm_madw_t object containing a mad with
*		the node's NodeInfo attribute.  The caller may discard the
*		osm_madw_t structure after calling osm_node_new.
*
*	sm_port_idx
*		[in] SM port index.
*
* RETURN VALUES
*	On success, a pointer to the new initialized osm_node_t structure.
*	NULL otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_remote_ignore_type
* NAME
*	osm_node_get_remote_ignore_type
*
* DESCRIPTION
*	Returns the ignore type of the node at the relevant port.
*
* SYNOPSIS
*/
static inline osm_ignore_port_t osm_node_get_remote_ignore_type(IN osm_node_t *p_node,
								IN const uint8_t port_num)
{
	return p_node->remote_ignore_type[port_num];
}
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*	port_num
*		[in] Port number of remote ignore type.
*
* RETURN VALUES
*	The ignore type of the port on the remote side of the node at the port number.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_physp_ptr
* NAME
*	osm_node_get_physp_ptr
*
* DESCRIPTION
*	Returns a pointer to the physical port object at the
*	specified local port number.
*
* SYNOPSIS
*/
static inline osm_physp_t *osm_node_get_physp_ptr(IN osm_node_t * p_node,
						  IN uint32_t port_num)
{

	OSM_ASSERT(p_node != NULL);
	OSM_ASSERT(port_num < p_node->physp_tbl_size);
	return osm_physp_is_valid(&p_node->physp_table[port_num]) ?
		&p_node->physp_table[port_num] : NULL;
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
* RETURN VALUES
*	Returns a pointer to the physical port object at the
*	specified local port number.
*	A return value of NULL means the port number was out of range.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_is_switch
* NAME
*	osm_node_is_switch
*
* DESCRIPTION
*	Returns whether the node's IBA type is IB_NODE_TYPE_SWITCH.
*
* SYNOPSIS
*/
static inline boolean_t osm_node_is_switch(IN const osm_node_t * p_node)
{
	return (IB_NODE_TYPE_SWITCH == p_node->node_info.node_type);
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns TRUE if the node is a switch and FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_is_router
* NAME
*	osm_node_is_router
*
* DESCRIPTION
*	Returns whether the node's IBA type is IB_NODE_TYPE_ROUTER.
*
* SYNOPSIS
*/
static inline boolean_t osm_node_is_router(IN const osm_node_t * p_node)
{
	return (IB_NODE_TYPE_ROUTER == p_node->node_info.node_type);
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns TRUE if the node is a router and FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_type
* NAME
*	osm_node_get_type
*
* DESCRIPTION
*	Returns the type of this node.
*
* SYNOPSIS
*/
static inline uint8_t osm_node_get_type(IN const osm_node_t * p_node)
{
	return p_node->node_info.node_type;
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns the IBA defined type of this node.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_num_physp
* NAME
*	osm_node_get_num_physp
*
* DESCRIPTION
*	Returns the number of osm_physp ports allocated for this node.
*	For switches, it is the number of external physical ports plus
*	port 0. For CAs and routers, it is the number of external physical
*	ports plus 1.
*
* SYNOPSIS
*/
static inline uint8_t osm_node_get_num_physp(IN const osm_node_t * p_node)
{
	return (uint8_t) p_node->physp_tbl_size;
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns the IBA defined type of this node.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_remote_node
* NAME
*	osm_node_get_remote_node
*
* DESCRIPTION
*	Returns a pointer to the node on the other end of the
*	specified port.
*	Returns NULL if no remote node exists.
*
* SYNOPSIS
*/
osm_node_t *osm_node_get_remote_node(IN osm_node_t * p_node,
				     IN uint8_t port_num,
				     OUT uint8_t * p_remote_port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to get the remote node.
*
*	p_remote_port_num
*		[out] Port number in the remote's node through which this
*		link exists.  The caller may specify NULL for this pointer
*		if the port number isn't needed.
*
* RETURN VALUES
*	Returns a pointer to the node on the other end of the
*	specified port.
*	Returns NULL if no remote node exists.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_is_remote_switch
* NAME
*	osm_node_is_remote_switch
*
* DESCRIPTION
*	Returns TRUE if the remote node connected to the 
*	other end of the specified port is a switch node.
*	Returns FALSE if the remote doesn't exist or if it's not a switch.
*
* SYNOPSIS
*/
boolean_t osm_node_is_remote_switch(IN osm_node_t * p_node, IN uint8_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to get the remote node.
*
* RETURN VALUES
*	Returns TRUE if the remote node connected to the 
*	other end of the specified port is a switch node.
*	Returns FALSE if the remote doesn't exist or if it's not a switch.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_base_lid
* NAME
*	osm_node_get_base_lid
*
* DESCRIPTION
*	Returns the LID value of the specified port on this node.
*
* SYNOPSIS
*/
static inline ib_net16_t osm_node_get_base_lid(IN const osm_node_t * p_node,
					       IN uint32_t port_num)
{
	OSM_ASSERT(port_num < p_node->physp_tbl_size);
	return osm_physp_get_base_lid(&p_node->physp_table[port_num]);
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
* RETURN VALUES
*	Returns a pointer to the physical port object at the
*	specified local port number.
*	A return value of zero means the port number was out of range.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_base_lid_ho
* NAME
*	osm_node_get_base_lid_ho
*
* DESCRIPTION
*	Returns the host order LID value of the specified port on this node.
*
* SYNOPSIS
*/
static inline uint16_t osm_node_get_base_lid_ho(IN const osm_node_t * p_node,
						IN uint32_t port_num)
{
	return cl_ntoh16(osm_node_get_base_lid(p_node, port_num));
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
* RETURN VALUES
*	Returns the port lid in host order.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_remote_base_lid
* NAME
*	osm_node_get_remote_base_lid
*
* DESCRIPTION
*	Returns the base LID value of the port on the other side
*	of the wire from the specified port on this node.
*
* SYNOPSIS
*/
ib_net16_t osm_node_get_remote_base_lid(IN osm_node_t * p_node,
					IN uint32_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
* RETURN VALUES
*	Returns a pointer to the physical port object at the
*	specified local port number.
*	A return value of zero means the port number was out of range.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_lmc
* NAME
*	osm_node_get_lmc
*
* DESCRIPTION
*	Returns the LMC value of the specified port on this node.
*
* SYNOPSIS
*/
static inline uint8_t osm_node_get_lmc(IN const osm_node_t * p_node,
				       IN uint32_t port_num)
{
	OSM_ASSERT(port_num < p_node->physp_tbl_size);
	return osm_physp_get_lmc(&p_node->physp_table[port_num]);
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
* RETURN VALUES
*	Returns the LMC value of the specified port on this node.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_init_physp
* NAME
*	osm_node_init_physp
*
* DESCRIPTION
*	Initializes a physical port for the given node.
*
* SYNOPSIS
*/
void osm_node_init_physp(IN osm_node_t * p_node,
			 uint8_t port_num,
			 IN const osm_madw_t * p_madw,
			 IN uint8_t sm_port_idx);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Local port number.
*
*	p_madw
*		[in] Pointer to a osm_madw_t object containing a mad with
*		the node's NodeInfo attribute as discovered through the
*		Physical Port to add to the node.  The caller may discard the
*		osm_madw_t structure after calling osm_node_new.
*
*	sm_port_idx
*		[in] SM port index.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Node object, Physical Port object.
*********/

/****f* OpenSM: Node/osm_node_get_node_guid
* NAME
*	osm_node_get_node_guid
*
* DESCRIPTION
*	Returns the node GUID of this node.
*
* SYNOPSIS
*/
static inline ib_net64_t osm_node_get_node_guid(IN const osm_node_t * p_node)
{
	return p_node->node_info.node_guid;
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns the node GUID of this node.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_node_guid_ho
* NAME
*	osm_node_get_node_guid_ho
*
* DESCRIPTION
*	Returns the node GUID of this node in host order.
*
* SYNOPSIS
*/
static inline uint64_t osm_node_get_node_guid_ho(IN const osm_node_t * p_node)
{
	return cl_ntoh64(p_node->node_info.node_guid);
}

/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns the node GUID of this node in host order.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_link
* NAME
*	osm_node_link
*
* DESCRIPTION
*	Logically connects a node to another node through the specified port.
*
* SYNOPSIS
*/
void osm_node_link(IN osm_node_t * p_node, IN uint8_t port_num,
		   IN osm_node_t * p_remote_node, IN uint8_t remote_port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to create the link.
*
*	p_remote_node
*		[in] Pointer to the remote node object.
*
*	remote_port_num
*		[in] Port number in the remote's node through which to
*		create this link.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_unlink
* NAME
*	osm_node_unlink
*
* DESCRIPTION
*	Logically disconnects a node from another node through
*	the specified port.
*
* SYNOPSIS
*/
void osm_node_unlink(IN osm_node_t * p_node, IN uint8_t port_num,
		     IN osm_node_t * p_remote_node, IN uint8_t remote_port_num,
		     IN boolean_t backup_link);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to unlink.
*
*	p_remote_node
*		[in] Pointer to the remote node object.
*
*	remote_port_num
*		[in] Port number in the remote's node through which to unlink.
*
*	backup_link
*		[in] Flag to backup the links for restoring later.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_relink
* NAME
*	osm_node_relink
*
* DESCRIPTION
*	Reconnects a node to a remote node using the backup links.
*
* SYNOPSIS
*/
boolean_t osm_node_relink(IN osm_node_t * p_node, IN uint8_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node to relink.
*
* RETURN VALUES
*	True if succeeded to relink and false otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_link_exists
* NAME
*	osm_node_link_exists
*
* DESCRIPTION
*	Return TRUE if a link exists between the specified nodes on
*	the specified ports.
*	Returns FALSE otherwise.
*
* SYNOPSIS
*/
boolean_t osm_node_link_exists(IN osm_node_t * p_node, IN uint8_t port_num,
			       IN osm_node_t * p_remote_node,
			       IN uint8_t remote_port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to check the link.
*
*	p_remote_node
*		[in] Pointer to the remote node object.
*
*	remote_port_num
*		[in] Port number in the remote's node through which to
*		check this link.
*
* RETURN VALUES
*	Return TRUE if a link exists between the specified nodes on
*	the specified ports.
*	Returns FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_has_any_link
* NAME
*	osm_node_has_any_link
*
* DESCRIPTION
*	Return TRUE if a any link exists from the specified nodes on
*	the specified port.
*	Returns FALSE otherwise.
*
* SYNOPSIS
*/
boolean_t osm_node_has_any_link(IN osm_node_t * p_node, IN uint8_t port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to check the link.
*
* RETURN VALUES
*	Return TRUE if a any link exists from the specified nodes on
*	the specified port.
*	Returns FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_link_has_valid_ports
* NAME
*	osm_node_link_has_valid_ports
*
* DESCRIPTION
*	Return TRUE if both ports in the link are valid (initialized).
*	Returns FALSE otherwise.
*
* SYNOPSIS
*/
boolean_t osm_node_link_has_valid_ports(IN osm_node_t * p_node,
					IN uint8_t port_num,
					IN osm_node_t * p_remote_node,
					IN uint8_t remote_port_num);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node through which to check the link.
*
*	p_remote_node
*		[in] Pointer to the remote node object.
*
*	remote_port_num
*		[in] Port number in the remote's node through which to
*		check this link.
*
* RETURN VALUES
*	Return TRUE if both ports in the link are valid (initialized).
*	Returns FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_increase_mcast_weight
* NAME
*	osm_node_increase_mcast_weight
*
* DESCRIPTION
*	Increase number of multicast routes on a link.
*
* SYNOPSIS
*/
static inline void osm_node_increase_mcast_weight(IN osm_node_t * p_node,
						  IN uint8_t port_num)
{
	osm_physp_t *p_physp = osm_node_get_physp_ptr(p_node, port_num);
	osm_physp_t *p_remote_physp;

	if (p_physp && (p_remote_physp = osm_physp_get_remote(p_physp))) {
		p_physp->mcast_route_weight++;
		p_remote_physp->mcast_route_weight++;
	}
}
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node to increase number of multicast
*		routes that goes through it.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_decrease_mcast_weight
* NAME
*	osm_node_decrease_mcast_weight
*
* DESCRIPTION
*	Decrease number of multicast routes on a link.
*
* SYNOPSIS
*/
static inline void osm_node_decrease_mcast_weight(IN osm_node_t * p_node,
						  IN uint8_t port_num)
{
	osm_physp_t *p_physp = osm_node_get_physp_ptr(p_node, port_num);
	osm_physp_t *p_remote_physp;

	if (p_physp) {
		if (p_physp->mcast_route_weight)
			p_physp->mcast_route_weight--;
		p_remote_physp = osm_physp_get_remote(p_physp);
		if (p_remote_physp && p_remote_physp->mcast_route_weight)
			p_remote_physp->mcast_route_weight--;
	}
}
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
*	port_num
*		[in] Port number in p_node to decrease number of multicast
*		routes that goes through it.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_get_any_topo_id
* NAME
*	Node/osm_node_get_any_topo_id
*
* DESCRIPTION
*	Return topology id for a topology that this node is part of
*
* SYNOPSIS
*/
uint64_t osm_node_get_any_topo_id(IN osm_node_t * p_node);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	topo_id for first topology found.
*	NO_TOPO_ID if the node is not member of any topology.
*********/

/****f* OpenSM: Node/osm_node_get_extended_node_type
* NAME
*	osm_node_get_extended_node_type
*
* DESCRIPTION
*	Returns the type of this node.
*
* SYNOPSIS
*/
uint8_t osm_node_get_extended_node_type(osm_node_t * p_node);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns the IBA defined extended node type of this node.
*
* NOTES
*
* SEE ALSO
*	Node object
*********/

/****f* OpenSM: Node/osm_node_is_gpu
* NAME
*	osm_node_is_gpu
*
* DESCRIPTION
*	Return whether inpute node is a GPU
*
* SYNOPSIS
*/
boolean_t osm_node_is_gpu(IN osm_node_t * p_node);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Return whether inpute node is a GPU
*
*********/

/****f* OpenSM: Node/osm_node_get_valid_physp
* NAME
*	osm_node_get_valid_physp
*
* DESCRIPTION
*	Returns a pointer to one of the valid physical ports of the node.
*	If there is no such a port, returns NULL.
*
* SYNOPSIS
*/
osm_physp_t * osm_node_get_valid_physp(IN osm_node_t * p_node);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to an osm_node_t object.
*
* RETURN VALUES
*	Returns a pointer to one of the valid physical ports of the node.
*	If there is no such a port, returns NULL.
*
* NOTES
*
* SEE ALSO
*	Physical Port object
*********/

END_C_DECLS
