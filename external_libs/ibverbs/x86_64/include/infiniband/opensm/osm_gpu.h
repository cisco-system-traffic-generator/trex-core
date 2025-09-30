/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *    Implementation of osm_gpu_t.
 * This object represents an Infiniband GPU.
 * This object is part of the opensm family of objects.
 *
 * Author:
 *    Or Nechemia, Nvidia
 */

#pragma once

#include <stdlib.h>
#include <iba/ib_types.h>
#include <opensm/osm_file_ids.h>
#include <opensm/osm_node.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

typedef enum _osm_nvlink_prtn_gpu_status {
	NVL_PRTN_GPU_STATUS_NON_MEMBER,
	NVL_PRTN_GPU_STATUS_MEMBER,
	NVL_PRTN_GPU_STATUS_NEW,
	NVL_PRTN_GPU_STATUS_UNKNOWN,
} osm_nvlink_prtn_gpu_status_t;

/****s* OpenSM: GPU/osm_gpu_t
* NAME
*	osm_gpu_t
*
* DESCRIPTION
*	GPU structure.
*
* SYNOPSIS
*/
typedef struct osm_gpu {
	cl_map_item_t map_item;
	osm_node_t *p_node;
	unsigned is_new;
	ib_alid_info_t subn_alid_info;
	ib_alid_info_t calc_alid_info;
	ib_qos_config_vl_t calc_qos_config_vl;
	boolean_t alid_info_updated;
	ib_net16_t last_sweep_pkey;
	uint8_t prtn_member_status[OSM_NVLINK_MAX_PLANES + 1];
} osm_gpu_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	p_node
*		Pointer to the Node object for this GPU.
*
*	is_new
*		Indicates the GPU that was discovered for the first time.
*		Used by ALID configuration flow, as an indication that ALID was changed
*		for existing GPU.
*
*	subn_alid_info
*		Copy of latest ALIDInfo received from this GPU.
*		Currently (for Single Node), supports only 1 ALID per GPU,
*		hence uses a single ALIDInfo block.
*
*	calc_alid_info
*		Copy of latest calculated ALIDInfo for this GPU.
*		Currently (for Single Node), supports only 1 ALID per GPU,
*		hence uses a single ALIDInfo block.
*
*	calc_qos_config_vl
*		Latest calculated QoSConfigVL for this GPU.
*		Currently same for all GPU ports but sent per port.
*
*	alid_info_updated
*		Indicates whether ALIDInfo GET response was received from the subnet.
*
*	last_sweep_pkey
*		Pkey of the partition that this GPU was a member of on previous sweep.
*		Under the assumption that on a single sweep, GPU cannot be in both remove and
*		add requests, this variable is used by the LAAS flow as indication if this GPU
*		was deleted, and from which partition it should be removed.
*
*	prtn_member_status
*		Membership status according to this sweep, per plane (GPU ports might not all
*		be back for all planes on the same sweep).
*		Meaning, GPU can be new for a plane only for the sweep in which it was added.
* SEE ALSO
*********/

/****f* OpenSM: GPU/osm_gpu_get_num_alids
* NAME
*	osm_gpu_get_num_alids
*
* DESCRIPTION
*	Returns number of ALIDs configured for this GPU (ENI alid_top)
*
* SYNOPSIS
*/
uint8_t osm_gpu_get_num_alids(IN osm_gpu_t * p_gpu);
/*
* PARAMETERS
*	p_gpu
*		[in] Pointer to the GPU.
*
* RETURN VALUE
*	Number of ALIDs configured for this GPU (ENI alid_top)
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: GPU/osm_gpu_get_used_alid_ho
* NAME
*	osm_gpu_get_used_alid_ho
*
* DESCRIPTION
*	Returns the ALID that is configured as IsUsed for this GPU
*
* SYNOPSIS
*/
uint16_t osm_gpu_get_used_alid_ho(osm_gpu_t * p_gpu);
/*
* PARAMETERS
*	p_gpu
*		[in] Pointer to the GPU.
*
* RETURN VALUE
*	The ALID that is configured as IsUsed for this GPU
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: GPU/osm_gpu_get_alid_by_index
* NAME
*	osm_gpu_get_alid_by_index
*
* DESCRIPTION
* 	Returns the ALID of GPU's ALIDInfo table entry that corresponds to input index.
*
* SYNOPSIS
*/
uint16_t osm_gpu_get_alid_by_index(osm_gpu_t * p_gpu, uint8_t index);
/*
* PARAMETERS
*	p_gpu
*		Pointer to the GPU.
*
* 	index
*		Index refers to ALIDInfo table entry
*
* RETURN VALUE
* 	ALID of GPU's ALIDInfo table entry that corresponds to input index.
*
* NOTES
*
* SEE ALSO
* 	ib_alid_info_t
*********/

/****f* OpenSM: GPU/osm_gpu_new
* NAME
*	osm_gpu_new
*
* DESCRIPTION
*	The osm_gpu_new function initializes a GPU object for use.
*
* SYNOPSIS
*/
osm_gpu_t * osm_gpu_new(IN osm_node_t * p_node);
/*
* PARAMETERS
*	p_node
*		[in] Pointer to the node object of this gpu
*
* RETURN VALUES
*	Pointer to the new initialized gpu object.
*
* NOTES
*
* SEE ALSO
*	GPU object, osm_gpu_delete
*********/

/****f* OpenSM: GPU/osm_gpu_delete
* NAME
*	osm_gpu_delete
*
* DESCRIPTION
*	Destroys and deallocates the object.
*
* SYNOPSIS
*/
void osm_gpu_delete(IN OUT osm_gpu_t ** pp_gpu);
/*
* PARAMETERS
*	pp_gpu
*		[in] Pointer to a pointer to the object to destroy.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*	GPU object, osm_gpu_new
*********/

/****f* OpenSM: GPU/osm_gpu_get_guid
* NAME
*	osm_gpu_get_guid
*
* DESCRIPTION
*	Returns GPU node guid
*
* SYNOPSIS
*/
ib_net64_t osm_gpu_get_guid(osm_gpu_t * p_gpu);
/*
* PARAMETERS
*	p_gpu
*		Pointer to a GPU object.
*
* RETURN VALUES
*	The node GUID of this GPU node.
*
* NOTES
*
* SEE ALSO
*	osm_gpu_t, osm_node_t
*********/

/****f* OpenSM: GPU/osm_gpu_get_port_plane
* NAME
*	osm_gpu_get_port_plane
*
* DESCRIPTION
*	Returns plane number assigned for GPU port, according to port number.
*
* SYNOPSIS
*/
uint8_t osm_gpu_get_port_plane(osm_gpu_t *p_gpu, uint8_t port_num);
/*
* PARAMETERS
*	p_gpu
*		Pointer to a GPU object.
*
* 	port_num
* 		Port number
*
* RETURN VALUES
*	Plane number assigned for GPU port, according to port number.
*
* NOTES
*
* SEE ALSO
*	osm_gpu_t
*********/

END_C_DECLS
