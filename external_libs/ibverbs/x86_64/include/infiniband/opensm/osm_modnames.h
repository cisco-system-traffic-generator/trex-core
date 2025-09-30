/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2012-2020 Mellanox Technologies LTD. All rights reserved.
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
 * 	Declaration of osm_file_ids_enum.
 *	This object is part of the OpenSM family of objects.
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

/****d* OpenSM: osm_file_ids_enum
* NAME
*	module_names
*
* DESCRIPTION
*	List of module names tied to the file_ids (via file_ids.h)
*
* SYNOPSIS
*/
static const char *module_names[] = {
	[0x00] = "main.c",
	[0x01] = "osm_console.c",
	[0x02] = "osm_console_io.c",
	[0x03] = "osm_db_files.c",
	[0x04] = "osm_db_pack.c",
	[0x05] = "osm_drop_mgr.c",
	[0x06] = "osm_dump.c",
	[0x07] = "osm_event_plugin.c",
	[0x08] = "osm_guid_info_rcv.c",
	[0x09] = "osm_guid_mgr.c",
	[0x0A] = "osm_helper.c",
	[0x0B] = "osm_inform.c",
	[0x0C] = "osm_lid_mgr.c",
	[0x0D] = "osm_lin_fwd_rcv.c",
	[0x0E] = "osm_link_mgr.c",
	[0x0F] = "osm_log.c",
	[0x10] = "osm_mad_pool.c",
	[0x11] = "osm_mcast_fwd_rcv.c",
	[0x12] = "osm_mcast_mgr.c",
	[0x13] = "osm_mcast_tbl.c",
	[0x14] = "osm_mcm_port.c",
	[0x15] = "osm_mesh.c",
	[0x16] = "osm_mlnx_ext_port_info_rcv.c",
	[0x17] = "osm_mtree.c",
	[0x18] = "osm_multicast.c",
	[0x19] = "osm_node.c",
	[0x1A] = "osm_node_desc_rcv.c",
	[0x1B] = "osm_node_info_rcv.c",
	[0x1C] = "osm_opensm.c",
	[0x1D] = "osm_perfmgr.c",
	[0x1E] = "osm_perfmgr_db.c",
	[0x1F] = "osm_pkey.c",
	[0x20] = "osm_pkey_mgr.c",
	[0x21] = "osm_pkey_rcv.c",
	[0x22] = "osm_port.c",
	[0x23] = "osm_port_info_rcv.c",
	[0x24] = "osm_prtn.c",
	[0x25] = "osm_prtn_config.c",
	[0x26] = "osm_qos.c",
	[0x27] = "osm_qos_parser_l.c",
	[0x28] = "osm_qos_parser_y.c",
	[0x29] = "osm_qos_policy.c",
	[0x2A] = "osm_remote_sm.c",
	[0x2B] = "osm_req.c",
	[0x2C] = "osm_resp.c",
	[0x2D] = "osm_router.c",
	[0x2E] = "osm_sa.c",
	[0x2F] = "osm_sa_class_port_info.c",
	[0x30] = "osm_sa_guidinfo_record.c",
	[0x31] = "osm_sa_informinfo.c",
	[0x32] = "osm_sa_lft_record.c",
	[0x33] = "osm_sa_link_record.c",
	[0x34] = "osm_sa_mad_ctrl.c",
	[0x35] = "osm_sa_mcmember_record.c",
	[0x36] = "osm_sa_mft_record.c",
	[0x37] = "osm_sa_multipath_record.c",
	[0x38] = "osm_sa_node_record.c",
	[0x39] = "osm_sa_path_record.c",
	[0x3A] = "osm_sa_pkey_record.c",
	[0x3B] = "osm_sa_portinfo_record.c",
	[0x3C] = "osm_sa_service_record.c",
	[0x3D] = "osm_sa_slvl_record.c",
	[0x3E] = "osm_sa_sminfo_record.c",
	[0x3F] = "osm_sa_sw_info_record.c",
	[0x40] = "osm_sa_vlarb_record.c",
	[0x41] = "osm_service.c",
	[0x42] = "osm_slvl_map_rcv.c",
	[0x43] = "osm_sm.c",
	[0x44] = "osm_sminfo_rcv.c",
	[0x45] = "osm_sm_mad_ctrl.c",
	[0x46] = "osm_sm_state_mgr.c",
	[0x47] = "osm_state_mgr.c",
	[0x48] = "osm_subnet.c",
	[0x49] = "osm_sw_info_rcv.c",
	[0x4A] = "osm_switch.c",
	[0x4B] = "osm_torus.c",
	[0x4C] = "osm_trap_rcv.c",
	[0x4D] = "osm_ucast_cache.c",
	[0x4E] = "osm_ucast_dnup.c",
	[0x4F] = "osm_ucast_file.c",
	[0x50] = "osm_ucast_ftree.c",
	[0x51] = "osm_ucast_lash.c",
	[0x52] = "osm_ucast_mgr.c",
	[0x53] = "osm_ucast_updn.c",
	[0x54] = "osm_vendor_ibumad.c",
	[0x55] = "osm_vl15intf.c",
	[0x56] = "osm_vl_arb_rcv.c",
	[0x57] = "st.c",
	[0x58] = "osm_ucast_dfsssp.c",
	[0x59] = "osm_congestion_control.c",
	[0x5A] = "osm_inform_mgr.c",
	[0x5B] = "osm_unhealthy_ports.c",
	[0x5C] = "osm_routing_chain.c",
	[0x5D] = "osm_port_group.c",
	[0x5E] = "osm_rch_parser_l.c",
	[0x5F] = "osm_rch_parser_y.c",
	[0x60] = "osm_port_grp_parser_l.c",
	[0x61] = "osm_port_grp_parser_y.c",
	[0x62] = "osm_topology_mgr.c",
	[0x63] = "osm_topo_parser_l.c",
	[0x64] = "osm_topo_parser_y.c",
	[0x65] = "osm_ucast_pqft.c",
	[0x66] = "osm_vport.c",
	[0x67] = "osm_vport_info_rcv.c",
	[0x68] = "osm_virtualization_info_rcv.c",
	[0x69] = "osm_vport_state_rcv.c",
	[0x6A] = "osm_general_info_rcv.c",
	[0x6B] = "osm_virt_mgr.c",
	[0x6C] = "osm_vnode_info_rcv.c",
	[0x6D] = "osm_vpkey_rcv.c",
	[0x6E] = "osm_vnode_desc_rcv.c",
	[0x6F] = "osm_router_info_rcv.c",
	[0x70] = "osm_router_mgr.c",
	[0x71] = "osm_router_table_rcv.c",
	[0x72] = "osm_router_nh_table_rcv.c",
	[0x73] = "osm_route_parser_l.c",
	[0x74] = "osm_route_parser_y.c",
	[0x75] = "osm_router_policy.c",
	[0x76] = "osm_verbose_bypass_parser_l.c",
	[0x77] = "osm_verbose_bypass_parser_y.c",
	[0x78] = "osm_verbose_bypass.c",
	[0x79] = "osm_ucast_dor.c",
	[0x7A] = "osm_vlid_mgr.c",
	[0x7B] = "osm_capability_mgr.c",
	[0x7C] = "osm_activity_mgr.c",
	[0x7D] = "osm_enhanced_qos_mgr.c",
	[0x7E] = "osm_qos_config_sl_rcv.c",
	[0x7F] = "osm_an2an.c",
	[0x80] = "osm_sw_port_state_rcv.c",
	[0x81] = "osm_advanced_routing.c",
	[0x82] = "osm_ar_mad_rcv.c",
	[0x83] = "osm_rn_mad_rcv.c",
	[0x84] = "osm_ucast_armgr.cpp",
	[0x85] = "osm_armgr.cpp",
	[0x86] = "osm_parallel_port_groups_calculator.cpp",
	[0x87] = "osm_thread_pool.cpp",
	[0x88] = "osm_plft_mad_rcv.c",
	[0x89] = "osm_mlnx_ext_node_info_rcv.c",
	[0x8A] = "osm_dfp.cpp",
	[0x8B] = "osm_sa_security_report.c",
	[0x8C] = "osm_dfmgr.cpp",
	[0x8D] = "osm_cc_parser_l.c",
	[0x8E] = "osm_cc_parser_y.c",
	[0x8F] = "osm_ar_sw_db_entry.cpp",
	[0x90] = "osm_ar_algorithm.cpp",
	[0x91] = "osm_ar_plft_based_algorithm.cpp",
	[0x92] = "osm_ar_kdor_algorithm.cpp",
	[0x93] = "osm_ar_engines.cpp",
	[0x94] = "osm_ar_dfp_algorithm.cpp",
	[0x95] = "osm_calculate_port_groups_task.cpp",
	[0x96] = "osm_max_flow_algorithm.c",
	[0x97] = "osm_key_mgr.c",
	[0x98] = "osm_vendor_specific.c",
	[0x99] = "osm_device_conf_parser_l.c",
	[0x9A] = "osm_device_conf_parser_y.c",
	[0x9B] = "osm_root_detection.c",
	[0x9C] = "osm_n2n.c",
	[0x9D] = "osm_ucast_auto.c",
	[0x9E] = "osm_cc_algo_parser_l.c",
	[0x9F] = "osm_cc_algo_parser_y.c",
	[0xA0] = "osm_hierarchy_info.c",
	[0xA1] = "osm_fast_recovery_parser_l.c",
	[0xA2] = "osm_fast_recovery_parser_y.c",
	[0xA3] = "osm_profiles_config_rcv.c",
	[0xA4] = "osm_fast_recovery_mads_rcv.c",
	[0xA5] = "osm_plugin.c",
	[0xA6] = "osm_subscriber.c",
	[0xA7] = "osm_fast_recovery.c",
	[0xA8] = "osm_gpu.c",
	[0xA9] = "osm_routing.c",
	[0xAA] = "osm_ucast_minhop.c",
	[0xAB] = "osm_nvlink.c",
	[0xAC] = "osm_nvlink_mad_rcv.c",
	[0xAD] = "osm_rail_filter_config_rcv.c",
	[0xAE] = "osm_tenant_mgr.c",
	[0xAF] = "osm_tenant_parser_l.c",
	[0xB0] = "osm_tenant_parser_y.c",
	[0xB1] = "osm_ucast_asymtree.c",
	[0xB2] = "osm_mads_engine.c",
	[0xB3] = "osm_planarized_mad_rcv.c",
	[0xB4] = "osm_planarized.c",
	[0xB5] = "osm_extended_switch_info_rcv.c",
	[0xB6] = "osm_fabric_mode.c",
	[0xB7] = "osm_fabric_mode_parser_l.c",
	[0xB8] = "osm_fabric_mode_parser_y.c",
	[0xB9] = "osm_issu_mgr.c",
	[0xBA] = "osm_chassis_info.c",
	[0xBB] = "osm_nvlink_prtn.c",
	[0xBC] = "osm_graph.c",
	[0xBD] = "osm_routing_v2.c",
	[0xBE] = "osm_port_recovery_policy.c",
	[0xBF] = "osm_routing_v2_utils.c",
	[0xC0] = "osm_routing_v2_minhop.c",
	[0xC1] = "osm_routing_v2_updn.c",
	[0xC2] = "osm_routing_v2_asym1.c",
	[0xC3] = "osm_routing_v2_asym2.c",
	[0xC4] = "osm_routing_v2_asym3.c",
	[0xC5] = "osm_routing_v2_bfs.c",
	[0xC6] = "osm_routing_v2_dijkstra.c",
	[0xC7] = "osm_root_detection_v2.c",
	[0xC8] = "osm_ucast_mgr_v2.c",
	[0xC9] = "osm_routing_v2_maxflow.c",
	[0xCA] = "osm_routing_v2_ff.c",
};

/***********/

END_C_DECLS
