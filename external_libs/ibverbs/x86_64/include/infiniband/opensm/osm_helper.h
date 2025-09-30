/*
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2011 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009 HNR Consulting. All rights reserved.
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

#include <stdarg.h>
#include <iba/ib_types.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_base.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_path.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/*
 * Abstract:
 * 	Declaration of helpful functions.
 */

#ifdef NOTYET
/*
 * Convenience macro for naming objects.
 */
#define osm_name(X)	_Generic((X),					\
			osm_node_t *   : osm_node_name,  		\
			osm_switch_t * : osm_switch_name,		\
			osm_vertex_t * : osm_vertex_name		\
			) (X)
#endif // NOTYET

/*
 * Forward references.
 */
struct _cl_qmap;
struct _cl_map;
struct _cl_fmap;

/****f* OpenSM: Helper/ib_get_sa_method_str
 * NAME
 *	ib_get_sa_method_str
 *
 * DESCRIPTION
 *	Returns a string for the specified SA Method value.
 *
 * SYNOPSIS
 */
const char *ib_get_sa_method_str(IN uint8_t method);
/*
 * PARAMETERS
 *	method
 *		[in] Network order METHOD ID value.
 *
 * RETURN VALUES
 *	Pointer to the method string.
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OpenSM: Helper/ib_get_sm_method_str
* NAME
*	ib_get_sm_method_str
*
* DESCRIPTION
*	Returns a string for the specified SM Method value.
*
* SYNOPSIS
*/
const char *ib_get_sm_method_str(IN uint8_t method);
/*
* PARAMETERS
*	method
*		[in] Network order METHOD ID value.
*
* RETURN VALUES
*	Pointer to the method string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_sm_attr_str
* NAME
*	ib_get_sm_attr_str
*
* DESCRIPTION
*	Returns a string for the specified SM attribute value.
*
* SYNOPSIS
*/
const char *ib_get_sm_attr_str(IN ib_net16_t attr);
/*
* PARAMETERS
*	attr
*		[in] Network order attribute ID value.
*
* RETURN VALUES
*	Pointer to the attribute string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_sa_attr_str
* NAME
*	ib_get_sa_attr_str
*
* DESCRIPTION
*	Returns a string for the specified SA attribute value.
*
* SYNOPSIS
*/
const char *ib_get_sa_attr_str(IN ib_net16_t attr);
/*
* PARAMETERS
*	attr
*		[in] Network order attribute ID value.
*
* RETURN VALUES
*	Pointer to the attribute string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_trap_str
* NAME
*	ib_get_trap_str
*
* DESCRIPTION
*	Returns a name for the specified trap.
*
* SYNOPSIS
*/
const char *ib_get_trap_str(uint16_t trap_num);
/*
* PARAMETERS
*	trap_num
*		[in] Network order trap number.
*
* RETURN VALUES
*	Name of the trap.
*
*********/

extern const ib_gid_t ib_zero_gid;

/****f* IBA Base: Types/ib_gid_is_notzero
* NAME
*	ib_gid_is_notzero
*
* DESCRIPTION
*	Returns a boolean indicating whether or not the GID is zero.
*
* SYNOPSIS
*/
static inline boolean_t ib_gid_is_notzero(IN const ib_gid_t * p_gid)
{
	return memcmp(p_gid, &ib_zero_gid, sizeof(*p_gid));
}

/*
* PARAMETERS
*	p_gid
*		[in] Pointer to the GID object.
*
* RETURN VALUES
*	Returns TRUE if GID is not zero.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	ib_gid_t
*********/

/****f* OpenSM: Helper/osm_dump_port_info
* NAME
*	osm_dump_port_info
*
* DESCRIPTION
*	Dumps the PortInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_port_info(IN osm_log_t * p_log, IN ib_net64_t node_guid,
			IN ib_net64_t port_guid, IN uint8_t port_num,
			IN const ib_port_info_t * p_pi,
			IN osm_log_level_t log_level);

void osm_dump_port_info_v2(IN osm_log_t * p_log, IN ib_net64_t node_guid,
			   IN ib_net64_t port_guid, IN uint8_t port_num,
			   IN const ib_port_info_t * p_pi,
			   IN const int file_id,
			   IN osm_log_level_t log_level);

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	node_guid
*		[in] Node GUID that owns this port.
*
*	port_guid
*		[in] Port GUID for this port.
*
*	port_num
*		[in] Port number for this port.
*
*	p_pi
*		[in] Pointer to the PortInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_guid_info
* NAME
*	osm_dump_guid_info
*
* DESCRIPTION
*	Dumps the GUIDInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_guid_info(IN osm_log_t * p_log, IN ib_net64_t node_guid,
			IN ib_net64_t port_guid, IN uint8_t block_num,
			IN const ib_guid_info_t * p_gi,
			IN osm_log_level_t log_level);

void osm_dump_guid_info_v2(IN osm_log_t * p_log, IN ib_net64_t node_guid,
			   IN ib_net64_t port_guid, IN uint8_t block_num,
			   IN const ib_guid_info_t * p_gi,
			   IN const int file_id,
			   IN osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object.
*
*	node_guid
*		[in] Node GUID that owns this port.
*
*	port_guid
*		[in] Port GUID for this port.
*
*	block_num
*		[in] Block number.
*
*	p_gi
*		[in] Pointer to the GUIDInfo attribute.
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

void osm_dump_mlnx_ext_port_info(IN osm_log_t * p_log, IN ib_net64_t node_guid,
				 IN ib_net64_t port_guid, IN uint8_t port_num,
				 IN const ib_mlnx_ext_port_info_t * p_pi,
				 IN osm_log_level_t log_level);

void osm_dump_mlnx_ext_port_info_v2(IN osm_log_t * p_log, IN ib_net64_t node_guid,
				    IN ib_net64_t port_guid, IN uint8_t port_num,
				    IN const ib_mlnx_ext_port_info_t * p_pi,
				    IN const int file_id,
				    IN osm_log_level_t log_level);

void osm_dump_path_record(IN osm_log_t * p_log, IN const ib_path_rec_t * p_pr,
			  IN osm_log_level_t log_level);

void osm_dump_path_record_v2(IN osm_log_t * p_log, IN const ib_path_rec_t * p_pr,
			     IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_multipath_record(IN osm_log_t * p_log,
			       IN const ib_multipath_rec_t * p_mpr,
			       IN osm_log_level_t log_level);

void osm_dump_multipath_record_v2(IN osm_log_t * p_log,
				  IN const ib_multipath_rec_t * p_mpr,
				  IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_node_record(IN osm_log_t * p_log,
			  IN const ib_node_record_t * p_nr,
			  IN osm_log_level_t log_level);

void osm_dump_node_record_v2(IN osm_log_t * p_log,
			     IN const ib_node_record_t * p_nr,
			     IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_mc_record(IN osm_log_t * p_log, IN const ib_member_rec_t * p_mcmr,
			IN osm_log_level_t log_level);

void osm_dump_mc_record_v2(IN osm_log_t * p_log, IN const ib_member_rec_t * p_mcmr,
			   IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_link_record(IN osm_log_t * p_log,
			  IN const ib_link_record_t * p_lr,
			  IN osm_log_level_t log_level);

void osm_dump_link_record_v2(IN osm_log_t * p_log,
			     IN const ib_link_record_t * p_lr,
			     IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_service_record(IN osm_log_t * p_log,
			     IN const ib_service_record_t * p_sr,
			     IN osm_log_level_t log_level);

void osm_dump_service_record_v2(IN osm_log_t * p_log,
				IN const ib_service_record_t * p_sr,
				IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_portinfo_record(IN osm_log_t * p_log,
			      IN const ib_portinfo_record_t * p_pir,
			      IN osm_log_level_t log_level);

void osm_dump_portinfo_record_v2(IN osm_log_t * p_log,
				 IN const ib_portinfo_record_t * p_pir,
				 IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_guidinfo_record(IN osm_log_t * p_log,
			      IN const ib_guidinfo_record_t * p_gir,
			      IN osm_log_level_t log_level);

void osm_dump_guidinfo_record_v2(IN osm_log_t * p_log,
				 IN const ib_guidinfo_record_t * p_gir,
				 IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_inform_info(IN osm_log_t * p_log,
			  IN const ib_inform_info_t * p_ii,
			  IN osm_log_level_t log_level);

void osm_dump_inform_info_v2(IN osm_log_t * p_log,
			     IN const ib_inform_info_t * p_ii,
			     IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_inform_info_record(IN osm_log_t * p_log,
				 IN const ib_inform_info_record_t * p_iir,
				 IN osm_log_level_t log_level);

void osm_dump_inform_info_record_v2(IN osm_log_t * p_log,
				    IN const ib_inform_info_record_t * p_iir,
				    IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_switch_info_record(IN osm_log_t * p_log,
				 IN const ib_switch_info_record_t * p_sir,
				 IN osm_log_level_t log_level);

void osm_dump_switch_info_record_v2(IN osm_log_t * p_log,
				    IN const ib_switch_info_record_t * p_sir,
				    IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_sm_info_record(IN osm_log_t * p_log,
			     IN const ib_sminfo_record_t * p_smir,
			     IN osm_log_level_t log_level);

void osm_dump_sm_info_record_v2(IN osm_log_t * p_log,
				IN const ib_sminfo_record_t * p_smir,
				IN const int file_id, IN osm_log_level_t log_level);

void osm_dump_pkey_block(IN osm_log_t * p_log, IN uint64_t port_guid,
			 IN uint16_t block_num, IN uint8_t port_num,
			 IN const ib_pkey_table_t * p_pkey_tbl,
			 IN osm_log_level_t log_level);

void osm_dump_pkey_block_v2(IN osm_log_t * p_log, IN uint64_t port_guid,
			    IN uint16_t block_num, IN uint8_t port_num,
			    IN const ib_pkey_table_t * p_pkey_tbl,
			    IN const int file_id,
			    IN osm_log_level_t log_level);

void osm_dump_vpkey_block_v2(IN osm_log_t * p_log, IN uint64_t vport_guid,
			     IN uint16_t block_num, IN uint16_t vport_index,
			     IN const ib_pkey_table_t * p_pkey_tbl,
			     IN const int file_id,
			     IN osm_log_level_t log_level);

void osm_dump_slvl_map_table(IN osm_log_t * p_log, IN uint64_t port_guid,
			     IN uint8_t in_port_num, IN uint8_t out_port_num,
			     IN const ib_slvl_table_t * p_slvl_tbl,
			     IN osm_log_level_t log_level);

void osm_dump_slvl_map_table_v2(IN osm_log_t * p_log, IN uint64_t port_guid,
				IN uint8_t in_port_num, IN uint8_t out_port_num,
				IN const ib_slvl_table_t * p_slvl_tbl,
				IN const int file_id,
				IN osm_log_level_t log_level);


void osm_dump_vl_arb_table(IN osm_log_t * p_log, IN uint64_t port_guid,
			   IN uint8_t block_num, IN uint8_t port_num,
			   IN const ib_vl_arb_table_t * p_vla_tbl,
			   IN osm_log_level_t log_level);

void osm_dump_vl_arb_table_v2(IN osm_log_t * p_log, IN uint64_t port_guid,
			      IN uint8_t block_num, IN uint8_t port_num,
			      IN const ib_vl_arb_table_t * p_vla_tbl,
			      IN const int file_id,
			      IN osm_log_level_t log_level);

/****f* OpenSM: Helper/osm_dump_port_info
* NAME
*	osm_dump_port_info
*
* DESCRIPTION
*	Dumps the PortInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_node_info(IN osm_log_t * p_log,
			IN const ib_node_info_t * p_ni,
			IN osm_log_level_t log_level);

void osm_dump_node_info_v2(IN osm_log_t * p_log,
			   IN const ib_node_info_t * p_ni,
			   IN const int file_id,
			   IN osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_ni
*		[in] Pointer to the NodeInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_sm_info
* NAME
*	osm_dump_sm_info
*
* DESCRIPTION
*	Dumps the SMInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_sm_info(IN osm_log_t * p_log, IN const ib_sm_info_t * p_smi,
		      IN osm_log_level_t log_level);

void osm_dump_sm_info_v2(IN osm_log_t * p_log, IN const ib_sm_info_t * p_smi,
			 IN const int file_id, IN osm_log_level_t log_level);

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_smi
*		[in] Pointer to the SMInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_switch_info
* NAME
*	osm_dump_switch_info
*
* DESCRIPTION
*	Dumps the SwitchInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_switch_info(IN osm_log_t * p_log,
			  IN const ib_switch_info_t * p_si,
			  IN osm_log_level_t log_level);

void osm_dump_switch_info_v2(IN osm_log_t * p_log,
			     IN const ib_switch_info_t * p_si,
			     IN const int file_id,
			     IN osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_si
*		[in] Pointer to the SwitchInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_notice
* NAME
*	osm_dump_notice
*
* DESCRIPTION
*	Dumps the Notice attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_notice(IN osm_log_t * p_log,
		     IN const ib_mad_notice_attr_t * p_ntci,
		     IN osm_log_level_t log_level);

void osm_dump_notice_v2(IN osm_log_t * p_log,
			IN const ib_mad_notice_attr_t * p_ntci,
			IN const int file_id,
			IN osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_ntci
*		[in] Pointer to the Notice attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_disp_msg_str
* NAME
*	osm_get_disp_msg_str
*
* DESCRIPTION
*	Returns a string for the specified Dispatcher message.
*
* SYNOPSIS
*/
const char *osm_get_disp_msg_str(IN cl_disp_msgid_t msg);
/*
* PARAMETERS
*	msg
*		[in] Dispatcher message ID value.
*
* RETURN VALUES
*	Pointer to the message description string.
*
* NOTES
*
* SEE ALSO
*********/

void osm_dump_dr_path(IN osm_log_t * p_log, IN const osm_dr_path_t * p_path,
		      IN osm_log_level_t level);

void osm_dump_dr_path_v2(IN osm_log_t * p_log, IN const osm_dr_path_t * p_path,
			 IN const int file_id, IN osm_log_level_t level);


char *osm_get_smp_path_str(IN const ib_smp_t * p_smp,
			   OUT char * buf, IN size_t buf_size,
			   IN ib_net16_t lid);

void osm_dump_smp_dr_path(IN osm_log_t * p_log, IN const ib_smp_t * p_smp,
			  IN ib_net64_t port_guid,
			  IN osm_log_level_t level);

void osm_dump_smp_dr_path_v2(IN osm_log_t * p_log, IN const ib_smp_t * p_smp,
			     IN ib_net64_t port_guid,
			     IN const int file_id, IN osm_log_level_t level);

void osm_dump_dr_smp(IN osm_log_t * p_log, IN const ib_smp_t * p_smp,
		     IN ib_net64_t port_guid, IN ib_net64_t dest_guid,
		     IN osm_log_level_t log_level);

void osm_dump_dr_smp_v2(IN osm_log_t * p_log, IN const ib_smp_t * p_smp,
			IN ib_net64_t port_guid, IN ib_net64_t dest_guid,
			IN const int file_id, IN osm_log_level_t level);

void osm_dump_sa_mad(IN osm_log_t * p_log, IN const ib_sa_mad_t * p_smp,
		     IN osm_log_level_t level);

void osm_dump_sa_mad_v2(IN osm_log_t * p_log, IN const ib_sa_mad_t * p_smp,
			IN const int file_id, IN osm_log_level_t level,
			IN ib_net64_t dest_guid);

void osm_dump_dr_path_as_buf(IN size_t max_len, IN const osm_dr_path_t * p_path,
			     OUT char* buf);

/****f* OpenSM: Helper/osm_dump_router_info
* NAME
*	osm_dump_router_info
*
* DESCRIPTION
*	Dumps the RouterInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_router_info(IN osm_log_t * p_log, IN ib_net64_t node_guid,
			  IN ib_net64_t port_guid, IN uint8_t port_num,
			  IN const ib_router_info_t * p_ri,
			  IN const int file_id, IN osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	node_guid
*		[in] node guid
*
*	port_guid
*		[in] port guid
*
*	port_num
*		[in] port number
*
*	p_ri
*		[in] Pointer to the RouterInfo attribute
*
*	file_id
*		[in] id of the file for profile logging
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/
/****f* OpenSM: Helper/osm_dump_adjacent_router_block
* NAME
*	osm_dump_adjacent_router_block
*
* DESCRIPTION
*	Dumps adjacent RouterTable attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_adjacent_router_block(
	IN osm_log_t * p_log, IN uint64_t port_guid,
	IN uint16_t block_num,
	IN const ib_rtr_adj_table_block_t * p_router_tbl,
	IN const int file_id,
	IN osm_log_level_t log_level);

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_router_tbl
*		[in] Pointer to the adjacent RouteTable attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_next_hop_router_block
* NAME
*	osm_dump_next_hop_router_block
*
* DESCRIPTION
*	Dumps next hop RouterTable attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_next_hop_router_block(
	IN osm_log_t * p_log, IN uint64_t port_guid,
	IN uint16_t block_num,
	IN const ib_rtr_next_hop_table_block_t * p_router_tbl,
	IN const int file_id,
	IN osm_log_level_t log_level);

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_router_tbl
*		[in] Pointer to the next hop RouteTable attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_routers_db
* NAME
*	osm_dump_routers_db
*
* DESCRIPTION
*	Dumps the Router data base to the log.
*
* SYNOPSIS
*/
void osm_dump_routers_db(IN osm_log_t * p_log,
			 IN struct _cl_qmap * p_rtr_guid_tbl,
			 IN struct _cl_fmap * p_prefix_rtr_table,
			 IN const int file_id,
			 IN osm_log_level_t log_level);

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_rtr_guid_tbl
*		[in] Pointer to the table of routers
*
*	p_prefix_rtr_table
*		[in] Pointer to the table of subnet prefixes to routers
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_sm_signal_str
* NAME
*	osm_get_sm_signal_str
*
* DESCRIPTION
*	Returns a string for the specified SM state.
*
* SYNOPSIS
*/
const char *osm_get_sm_signal_str(IN osm_signal_t signal);
/*
* PARAMETERS
*	state
*		[in] Signal value
*
* RETURN VALUES
*	Pointer to the signal description string.
*
* NOTES
*
* SEE ALSO
*********/

const char *osm_get_port_state_str_fixed_width(IN uint8_t port_state);

const char *osm_get_node_type_str_fixed_width(IN uint8_t node_type);

const char *osm_get_manufacturer_str(IN osm_node_t *p_node);

const char *osm_get_mtu_str(IN uint8_t mtu);

const char *osm_get_lwa_str(IN uint8_t lwa);

const char *osm_get_lsa_str(IN uint8_t lsa, IN uint8_t lsea, IN uint8_t lsea2,
			    IN uint8_t state, IN uint8_t mepi_active_speed);

/****f* IBA Base: Types/ib_get_path_record_rate_str
* NAME
*	ib_get_path_record_rate_str
*
* DESCRIPTION
*	Returns a string for the rate speed.
*
* SYNOPSIS
*/
const char *ib_get_path_record_rate_str(IN uint8_t rate);
/*
* PARAMETERS
*	rate
*		[in] A number representing the rate
*
* RETURN VALUES
*	Pointer to the string representing rate in Gb/s.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_wire_speed_val
* NAME
*	osm_get_wire_speed_val
*
* DESCRIPTION
*	Returns the wire speed.
*
* SYNOPSIS
*/
uint32_t osm_get_wire_speed_val(uint8_t wire_speed_rate);
/*
* PARAMETERS
*	wire_speed_rate
*		A number representing the wire speed rate
*
* RETURN VALUES
*	Returns an int for the wire speed in Mb/s.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_sm_mgr_signal_str
* NAME
*	osm_get_sm_mgr_signal_str
*
* DESCRIPTION
*	Returns a string for the specified SM manager signal.
*
* SYNOPSIS
*/
const char *osm_get_sm_mgr_signal_str(IN osm_sm_signal_t signal);
/*
* PARAMETERS
*	signal
*		[in] SM manager signal
*
* RETURN VALUES
*	Pointer to the signal description string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_sm_mgr_state_str
* NAME
*	osm_get_sm_mgr_state_str
*
* DESCRIPTION
*	Returns a string for the specified SM manager state.
*
* SYNOPSIS
*/
const char *osm_get_sm_mgr_state_str(IN uint16_t state);
/*
* PARAMETERS
*	state
*		[in] SM manager state
*
* RETURN VALUES
*	Pointer to the state description string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_mtu_is_valid
* NAME
*	ib_mtu_is_valid
*
* DESCRIPTION
*	Validates encoded MTU
*
* SYNOPSIS
*/
int ib_mtu_is_valid(IN const int mtu);
/*
* PARAMETERS
*	mtu
*		[in] Encoded path mtu.
*
* RETURN VALUES
*	Returns an int indicating mtu is valid (1)
*	or invalid (0).
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_rate_is_valid
* NAME
*	ib_rate_is_valid
*
* DESCRIPTION
*	Validates encoded rate
*
* SYNOPSIS
*/
int ib_rate_is_valid(IN const int rate);
/*
* PARAMETERS
*	rate
*		[in] Encoded path rate.
*
* RETURN VALUES
*	Returns an int indicating rate is valid (1)
*	or invalid (0).
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_compare_rates
* NAME
*	ib_path_compare_rates
*
* DESCRIPTION
*	Compares the encoded values for two path rates and
*	return value is based on the ordered comparison of
*	the path rates (or path rate equivalents).
*
* SYNOPSIS
*/
int ib_path_compare_rates(IN const int rate1, IN const int rate2);

/*
* PARAMETERS
*	rate1
*		[in] Encoded path rate 1.
*
*	rate2
*		[in] Encoded path rate 2.
*
* RETURN VALUES
*	Returns an int indicating less than (-1), equal to (0), or
*	greater than (1) rate1 as compared with rate2.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_rate_get_prev
* NAME
*	ib_path_rate_get_prev
*
* DESCRIPTION
*	Obtains encoded rate for the rate previous to the one requested.
*
* SYNOPSIS
*/
int ib_path_rate_get_prev(IN const int rate);

/*
* PARAMETERS
*	rate
*		[in] Encoded path rate.
*
* RETURN VALUES
*	Returns an int indicating encoded rate or
*	0 if none can be found.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_rate_get_next
* NAME
*	ib_path_rate_get_next
*
* DESCRIPTION
*	Obtains encoded rate for the rate subsequent to the one requested.
*
* SYNOPSIS
*/
int ib_path_rate_get_next(IN const int rate);

/*
* PARAMETERS
*	rate
*		[in] Encoded path rate.
*
* RETURN VALUES
*	Returns an int indicating encoded rate or
*	0 if none can be found.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_get_reduced_rate
* NAME
*	ib_path_get_reduced_rate
*
* DESCRIPTION
*	Obtains encoded rate for a reduced rate, subsequent
*	to input maximal rate.
*
* SYNOPSIS
*/
int ib_path_get_reduced_rate(IN const uint8_t rate, IN const uint8_t limit);
/*
* PARAMETERS
*	rate
*		[in] Encoded path rate.
*
*	limit
*		[in] Encoded maximal rate supported.
*
* RETURN VALUES
*	Returns an int indicating reduced encoded rate supported,
*	or minimal rate if none can be found.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_truncate_file
* NAME
*       osm_truncate_file
*
* DESCRIPTION
*       Truncate a file and resetting the file size to 0.
*
* SYNOPSIS
*/

void osm_truncate_file(FILE *file, unsigned long *file_size);

/*
* PARAMETERS
*       file
*               [in] pointer to file to truncate.
*
*       file_size
*               [in] pointer to file size
*
* RETURN VALUES
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_compare_gids
* NAME
*	ib_compare_gids
*
* DESCRIPTION
*	Compares GID objects
*
* SYNOPSIS
*/
int ib_compare_gids(IN const void * p_gid_1, IN const void * p_gid_2);
/*
* PARAMETERS
*	p_gid_1
*		[in] Pointer to first ib_gid_t object
*
*	p_gid_2
*		[in] Pointer to secondfirst ib_gid_t object
*
* RETURN VALUES
*	Returns 0 if both GIDs are equal.
*	Negative value if the first GID is lower then second GID.
*	Positive value if the first GID is greater then second GID.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_compare_prefix_pkey
* NAME
*	ib_compare_prefix_pkey
*
* DESCRIPTION
*	Compares <subnet_prefix, p_key> objects
*
* SYNOPSIS
*/
int ib_compare_prefix_pkey(IN const void * p_obj1, IN const void * p_obj2);
/*
* PARAMETERS
*	p_obj1
*		[in] Pointer to the first <subnet_prefix, p_key> object
*
*	p_obj2
*		[in] Pointer to the second <subnet_prefix, p_key> object
*
* RETURN VALUES
*	Returns 0 if both pairs are equal.
*	Negative value if the first object is lower then the second.
*	Positive value if the first object is greater then the second.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_rate_max_12xedr
* NAME
*	ib_path_rate_max_12xedr
*
* DESCRIPTION
*	Obtains encoded rate from the set of "original" extended
*	SA rates (up through and including 300 Gbps - 12x EDR).
*
* SYNOPSIS
*/
int ib_path_rate_max_12xedr(IN const int rate);

/*
* PARAMETERS
*	rate
*		[in] Encoded path rate.
*
* RETURN VALUES
*	Returns an int indicating the encoded rate
*	with a maximum of 300 Gbps (12x EDR).
*	For new rates (relating to 2x and HDR), the
*	nearest "original" extended rate lower than
*	the 2x or HDR related rate is returned.
*	0 if none can be found.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/ib_path_rate_2x_hdr_fixups
* NAME
*	ib_path_rate_2x_hdr_fixups
*
* DESCRIPTION
*	Lowers encoded rate to one that's possible according to the PortInfo's
*	capability's mask. For example, whether 2x link width and/or HDR are
*	supported.
*
* SYNOPSIS
*/
int ib_path_rate_2x_hdr_fixups(IN const ib_port_info_t * p_pi,
			       IN const int rate);

/*
* PARAMETERS
*	p_pi
*		[in] Pointer to the PortInfo attribute
*	rate
*		[in] Encoded path rate.
*
* RETURN VALUES
*	Returns an int indicating the fixed up encoded rate
*	based on whether 2x link width and/or HDR are supported.
*
* NOTES
*	Named ib_path_rate_2x_hdr_fixups since HDR using 2x is the first speed
*	which had specific capabilitiy bits.
*	Since EDR doesn't have corresponding capability mask bit, it is assumed
*	that EDR is supported and can be used as fallback.
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/sprint_uint8_arr
* NAME
*	sprint_uint8_arr
*
* DESCRIPTION
*	Create the comma-separated string of numbers
*	from input array of uint8 numbers
*	(e.g. "1,2,3,4")
*
* SYNOPSIS
*/
int sprint_uint8_arr(IN char *buf, IN size_t size,
		     IN const uint8_t * arr, IN size_t len);

/*
* PARAMETERS
*	buf
*		[in] Pointer to the output buffer
*
*	size
*		[in] Size of the output buffer
*
*	arr
*		[in] Pointer to the input array of uint8
*
*	len
*		[in] Size of the input array
*
* RETURN VALUES
*	Return the number of characters printed to the buffer
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/sprint_time_str
* NAME
*	sprint_time_str
*
* DESCRIPTION
* 	Create string with date and time
*
* SYNOPSIS
*/
void sprint_time_str(IN struct timeval *tv, IN char *str, IN int len,
		     IN boolean_t full);
/*
* PARAMETERS
*	tv
*		[in] Pointer to timeval
*
*	str
*		[in] Pointer to string to write time into
*
*	len
*		[in] Size of the string
*
* 	full
* 		[in] Write full time format
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_timestamp_usec
* NAME
*	osm_get_timestamp_usec
*
* DESCRIPTION
*	Returns the current timestamp in microseconds since epoch.
*
* SYNOPSIS
*/
uint64_t osm_get_timestamp_usec(IN char *str, IN int len, IN boolean_t full);
/*
* PARAMETERS
*	str
*		[in] Pointer to string to write time into
*
*	len
*		[in] Size of the string
*
*	full
*		[in] Write full time format
*
* RETURN VALUES
*	Current timestamp in microseconds since epoch.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/phys_mask_template_by_virt
* NAME
*	phys_mask_template_by_virt
*
* DESCRIPTION
* 	Create mask for physical port capability mask by virtual port's
*	capability mask as follows:
* 	Output capability mask's 1, 16, 17, 19, 20 and 23 bits are identical
*	(respectively) to virtual port bits -
*		0: IsCommunicationManagementSupported
*		1: IsSNMPTunnelingSupported
*	 	2: IsDeviceManagementSupported
*		3: IsVendorClassSupported
*	 	4: IsBootManagementSupported
*		5: isSM
* 	Other bits are 0.
*
* SYNOPSIS
*/
static inline ib_net32_t phys_mask_template_by_virt(ib_net16_t vport_mask)
{
        ib_net32_t on_bits = 0;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_SM))
                on_bits |= IB_PORT_CAP_IS_SM;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_COMMUNICATION_MANAGEMENT_SUPPORTED))
                on_bits |= IB_PORT_CAP_HAS_COM_MGT;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_SNMP_TUNNELING_SUPPORTED))
                on_bits |= IB_PORT_CAP_HAS_SNMP;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_DEVICE_MANAGEMENT_SUPPORTED))
                on_bits |= IB_PORT_CAP_HAS_DEV_MGT;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_VENDOR_CLASS_SUPPORTED))
                on_bits |= IB_PORT_CAP_HAS_VEND_CLS;

        if ((vport_mask & IB_VPORT_INFO_CAP_IS_BOOT_MANAGEMENT_SUPPORTED))
                on_bits |= IB_PORT_CAP_HAS_BM;

        return on_bits;
}
/*
* PARAMETERS
*	vport_mask
*		Capability mask of virtual port
*
* RETURN VALUES
*	Capability mask in physical port capability mask format.
*	Bits relevant to vport are 0 if are 0 in input vport_mask.
*
*********/

/****f* OpenSM: Helper/osm_dump_key_violation
* NAME
*	osm_dump_key_violation
*
* DESCRIPTION
*	Dumps the key violation Notice attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_key_violation(IN osm_log_t * p_log,
			    IN const ib_mad_notice_attr_t * p_ntci,
			    IN const int file_id,
			    IN osm_log_level_t log_level,
			    IN uint8_t mgt_class);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_ntci
*		[in] Pointer to the Notice attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
*	mgt_class
*		[in] Management class (VS and CC are supported)
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_compare_uint64
* NAME
*	osm_compare_uint64
*
* DESCRIPTION
*	Compares given values as uint64.
*
* SYNOPSIS
*/
static inline int osm_compare_uint64(IN const void * p_val_1, IN const void * p_val_2)
{
	return memcmp(p_val_1, p_val_2, sizeof(uint64_t));
}
/*
* PARAMETERS
*	p_val_1
*		[in] Pointer to the first value for comparison
*
*	p_val_2
*		[in] Pointer to the second value for comparison
*
* RETURN VALUES
*	Returns 0 if the keys match.
*	Returns less than 0 if *p_key1 is less than *p_key2.
*	Returns greater than 0 if *p_key1 is greater than *p_key2.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_compare_uint16
* NAME
*	osm_compare_uint16
*
* DESCRIPTION
*	Compares given values as uint16.
*
* SYNOPSIS
*/
static inline int osm_compare_uint16(IN const void * p_val_1, IN const void * p_val_2)
{
	return memcmp(p_val_1, p_val_2, sizeof(uint16_t));
}
/*
* PARAMETERS
*	p_val_1
*		[in] Pointer to the first value for comparison
*
*	p_val_2
*		[in] Pointer to the second value for comparison
*
* RETURN VALUES
*	Returns 0 if the keys match.
*	Returns less than 0 if *p_key1 is less than *p_key2.
*	Returns greater than 0 if *p_key1 is greater than *p_key2.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_number_of_set_bits_in_bitmask
* NAME
*	osm_get_number_of_set_bits_in_bitmask
*
* DESCRIPTION
*	Counts and returns number of set bits in the number
*
* SYNOPSIS
*/
uint8_t osm_get_number_of_set_bits_in_bitmask(IN int bitmask);
/*
* PARAMETERS
*	bitmask
*		[in] A bit mask/numeric value to count number of set bits in
*
*
* RETURN VALUES
* 	Returns number of set bits in the given bit mask.
*
* NOTES
*
* SEE ALSO
*********/

/* Sets all the bits in range from 'lsb' bit index to 'msb' bit index in the bitmask. */
#define CL_BITMASK_SET(bitmask, msb, lsb) \
({ (__typeof__(bitmask)) (bitmask | ((1ULL<<((msb) - (lsb) + 1)) - 1) << lsb); })

/* Unsets all the bits in range from 'lsb' bit index to 'msb' bit index in the bitmask. */
#define CL_BITMASK_UNSET(bitmask, msb, lsb) \
({ (__typeof__(bitmask)) (bitmask & ~(((1ULL<<((msb) - (lsb) + 1)) - 1) << lsb)); })

/****f* OpenSM: Helper/osm_any_null
* NAME
*	osm_any_null
*
* DESCRIPTION
*	Detrermines if any of the pointers are NULL.
*
* SYNOPSIS
*/
boolean_t osm_any_null(int ptr_count, ...);
/*
* PARAMETERS
*	ptr_count
*		[in] Number of pointers in the variable list.
*	...
*		[in] list of pointers.
*
* RETURN VALUES
* 	TRUE if any of the pointers are NULL
* 	FALSE otherwise
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_all_non_null
* NAME
*	osm_all_non_null
*
* DESCRIPTION
*	Determines if all of the pointers are not NULL.
*
* SYNOPSIS
*/
boolean_t osm_all_non_null(IN int count, ...);
/*
* PARAMETERS
*	ptr_count
*		[in] Number of pointers in the variable list.
*	...
*		[in] list of pointers.
*
* RETURN VALUES
* 	TRUE if all of the pointers are non NULL
* 	FALSE otherwise
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
