/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
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
 */

#ifndef KERN_ABI_EXP_H
#define KERN_ABI_EXP_H

#include <infiniband/kern-abi.h>

/*
 * This file must be kept in sync with the kernel's version of
 * drivers/infiniband/include/ib_user_verbs_exp.h
 */

enum {
	IB_USER_VERBS_EXP_CMD_FIRST = 64
};

enum {
	IB_USER_VERBS_EXP_CMD_CREATE_QP,
	IB_USER_VERBS_EXP_CMD_MODIFY_CQ,
	IB_USER_VERBS_EXP_CMD_MODIFY_QP,
	IB_USER_VERBS_EXP_CMD_CREATE_CQ,
	IB_USER_VERBS_EXP_CMD_QUERY_DEVICE,
	IB_USER_VERBS_EXP_CMD_CREATE_DCT,
	IB_USER_VERBS_EXP_CMD_DESTROY_DCT,
	IB_USER_VERBS_EXP_CMD_QUERY_DCT,
	IB_USER_VERBS_EXP_CMD_ARM_DCT,
	IB_USER_VERBS_EXP_CMD_CREATE_MR,
	IB_USER_VERBS_EXP_CMD_QUERY_MKEY,
	IB_USER_VERBS_EXP_CMD_REG_MR,
	IB_USER_VERBS_EXP_CMD_PREFETCH_MR,
	IB_USER_VERBS_EXP_CMD_REREG_MR,
	IB_USER_VERBS_EXP_CMD_CREATE_WQ,
	IB_USER_VERBS_EXP_CMD_MODIFY_WQ,
	IB_USER_VERBS_EXP_CMD_DESTROY_WQ,
	IB_USER_VERBS_EXP_CMD_CREATE_RWQ_IND_TBL,
	IB_USER_VERBS_EXP_CMD_DESTROY_RWQ_IND_TBL,
	IB_USER_VERBS_EXP_CMD_CREATE_FLOW,
};

enum {
	IB_USER_VERBS_CMD_EXP_CREATE_WQ =
			IB_USER_VERBS_EXP_CMD_CREATE_WQ +
			IB_USER_VERBS_EXP_CMD_FIRST,
	IB_USER_VERBS_CMD_EXP_MODIFY_WQ =
			IB_USER_VERBS_EXP_CMD_MODIFY_WQ +
			IB_USER_VERBS_EXP_CMD_FIRST,
	IB_USER_VERBS_CMD_EXP_DESTROY_WQ =
			IB_USER_VERBS_EXP_CMD_DESTROY_WQ +
			IB_USER_VERBS_EXP_CMD_FIRST,
	IB_USER_VERBS_CMD_EXP_CREATE_RWQ_IND_TBL =
			IB_USER_VERBS_EXP_CMD_CREATE_RWQ_IND_TBL +
			IB_USER_VERBS_EXP_CMD_FIRST,
	IB_USER_VERBS_CMD_EXP_DESTROY_RWQ_IND_TBL =
			IB_USER_VERBS_EXP_CMD_DESTROY_RWQ_IND_TBL +
			IB_USER_VERBS_EXP_CMD_FIRST,
	/*
	 * Set commands that didn't exist to -1 so our compile-time
	 * trick opcodes in IBV_INIT_CMD() doesn't break.
	 */
	IB_USER_VERBS_CMD_EXP_CREATE_WQ_V2 = -1,
	IB_USER_VERBS_CMD_EXP_MODIFY_WQ_V2 = -1,
	IB_USER_VERBS_CMD_EXP_DESTROY_WQ_V2 = -1,
	IB_USER_VERBS_CMD_EXP_CREATE_RWQ_IND_TBL_V2 = -1,
	IB_USER_VERBS_CMD_EXP_DESTROY_RWQ_IND_TBL_V2 = -1,
};

enum ibv_exp_create_qp_comp_mask {
	IBV_EXP_CREATE_QP_CAP_FLAGS          = (1ULL << 0),
	IBV_EXP_CREATE_QP_INL_RECV           = (1ULL << 1),
	IBV_EXP_CREATE_QP_QPG                = (1ULL << 2),
	IBV_EXP_CREATE_QP_MAX_INL_KLMS	     = (1ULL << 3)
};

struct ibv_create_qpg_init_attrib {
	__u32 tss_child_count;
	__u32 rss_child_count;
};

struct ibv_create_qpg {
	__u32 qpg_type;
	union {
		struct {
			__u32 parent_handle;
			__u32 reserved;
		};
		struct ibv_create_qpg_init_attrib parent_attrib;
	};
	__u32 reserved2;
};

enum ibv_exp_create_qp_kernel_flags {
	IBV_EXP_CREATE_QP_KERNEL_FLAGS = IBV_EXP_QP_CREATE_CROSS_CHANNEL  |
					 IBV_EXP_QP_CREATE_MANAGED_SEND   |
					 IBV_EXP_QP_CREATE_MANAGED_RECV   |
					 IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY |
					 IBV_EXP_QP_CREATE_RX_END_PADDING |
					 IBV_EXP_QP_CREATE_SCATTER_FCS
};

struct ibv_exp_create_qp {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 send_cq_handle;
	__u32 recv_cq_handle;
	__u32 srq_handle;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u8  sq_sig_all;
	__u8  qp_type;
	__u8  is_srq;
	__u8  reserved;
	__u64 qp_cap_flags;
	__u32 max_inl_recv;
	__u32 reserved1;
	struct ibv_create_qpg qpg;
	__u64 max_inl_send_klms;
	struct {
		__u64 rx_hash_fields_mask;
		__u32 rwq_ind_tbl_handle;
		__u8 rx_hash_function;
		__u8 rx_hash_key_len;
		__u8 rx_hash_key[128];
		__u16 reserved;
	} rx_hash_info;
	__u8 port_num;
	__u8 reserved_2[7];
	__u64 driver_data[0];
};

enum ibv_exp_create_qp_resp_comp_mask {
	IBV_EXP_CREATE_QP_RESP_INL_RECV       = (1ULL << 0),
};

struct ibv_exp_create_qp_resp {
	__u64 comp_mask;
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 max_inl_recv;
};

struct ibv_exp_umr_caps_resp {
	__u32 max_klm_list_size;
	__u32 max_send_wqe_inline_klms;
	__u32 max_umr_recursion_depth;
	__u32 max_umr_stride_dimension;
};

struct ibv_exp_odp_caps_resp {
	__u64	general_odp_caps;
	struct {
		__u32	rc_odp_caps;
		__u32	uc_odp_caps;
		__u32	ud_odp_caps;
		__u32	dc_odp_caps;
		__u32	xrc_odp_caps;
		__u32	raw_eth_odp_caps;
	} per_transport_caps;
};

struct ibv_exp_query_device {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 driver_data[0];
};

struct ibv_exp_rx_hash_caps_resp {
	__u32	max_rwq_indirection_tables;
	__u32	max_rwq_indirection_table_size;
	__u64	supported_packet_fields;
	__u32	supported_qps;
	__u8	supported_hash_functions;
	__u8	reserved[3];
};

struct ibv_exp_mp_rq_caps_resp {
	__u32	supported_qps; /* use ibv_exp_supported_qp_types */
	__u32	allowed_shifts; /* use ibv_exp_mp_rq_shifts */
	__u8	min_single_wqe_log_num_of_strides;
	__u8	max_single_wqe_log_num_of_strides;
	__u8	min_single_stride_log_num_of_bytes;
	__u8	max_single_stride_log_num_of_bytes;
	__u32	reserved;
};

struct ibv_exp_ec_caps_resp {
        __u32        max_ec_data_vector_count;
        __u32        max_ec_calc_inflight_calcs;
};

struct ibv_exp_masked_atomic_caps {
	__u32 max_fa_bit_boundary;
	__u32 log_max_atomic_inline;
	__u64 masked_log_atomic_arg_sizes;
	__u64 masked_log_atomic_arg_sizes_network_endianness;
};

struct ibv_exp_lso_caps_resp {
	__u32 max_tso;
	__u32 supported_qpts;
};

struct ibv_exp_packet_pacing_caps_resp {
	__u32 qp_rate_limit_min;
	__u32 qp_rate_limit_max; /* In kbps */
	__u32 supported_qpts;
	__u32 reserved;
};

struct ibv_exp_query_device_resp {
	__u64 comp_mask;
	__u64 fw_ver;
	__u64 node_guid;
	__u64 sys_image_guid;
	__u64 max_mr_size;
	__u64 page_size_cap;
	__u32 vendor_id;
	__u32 vendor_part_id;
	__u32 hw_ver;
	__u32 max_qp;
	__u32 max_qp_wr;
	__u32 device_cap_flags;
	__u32 max_sge;
	__u32 max_sge_rd;
	__u32 max_cq;
	__u32 max_cqe;
	__u32 max_mr;
	__u32 max_pd;
	__u32 max_qp_rd_atom;
	__u32 max_ee_rd_atom;
	__u32 max_res_rd_atom;
	__u32 max_qp_init_rd_atom;
	__u32 max_ee_init_rd_atom;
	__u32 exp_atomic_cap;
	__u32 max_ee;
	__u32 max_rdd;
	__u32 max_mw;
	__u32 max_raw_ipv6_qp;
	__u32 max_raw_ethy_qp;
	__u32 max_mcast_grp;
	__u32 max_mcast_qp_attach;
	__u32 max_total_mcast_qp_attach;
	__u32 max_ah;
	__u32 max_fmr;
	__u32 max_map_per_fmr;
	__u32 max_srq;
	__u32 max_srq_wr;
	__u32 max_srq_sge;
	__u16 max_pkeys;
	__u8  local_ca_ack_delay;
	__u8  phys_port_cnt;
	__u8  reserved[4];
	__u64 timestamp_mask;
	__u64 hca_core_clock;
	__u64 device_cap_flags2;
	__u32 dc_rd_req;
	__u32 dc_rd_res;
	__u32 inline_recv_sz;
	__u32 max_rss_tbl_sz;
	__u64 log_atomic_arg_sizes;
	__u32 max_fa_bit_boundary;
	__u32 log_max_atomic_inline;
	struct ibv_exp_umr_caps_resp umr_caps;
	struct ibv_exp_odp_caps_resp odp_caps;
	__u32 max_dct;
	__u32 max_ctx_res_domain;
	struct ibv_exp_rx_hash_caps_resp rx_hash;
	__u32 max_wq_type_rq;
	__u32 max_device_ctx;
	struct ibv_exp_mp_rq_caps_resp mp_rq_caps;
	__u16 wq_vlan_offloads_cap;
	__u8 reserved1[6];
	struct ibv_exp_ec_caps_resp ec_caps;
	struct ibv_exp_masked_atomic_caps masked_atomic_caps;
	__u16 rx_pad_end_addr_align;
	__u8 reserved2[6];
	struct ibv_exp_lso_caps_resp tso_caps;
	struct ibv_exp_packet_pacing_caps_resp packet_pacing_caps;
};

struct ibv_exp_create_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 cq_handle;
	__u32 srq_handle;
	__u32 access_flags;
	__u64 dc_key;
	__u32 flow_label;
	__u8  min_rnr_timer;
	__u8  tclass;
	__u8  port;
	__u8  pkey_index;
	__u8  gid_index;
	__u8  hop_limit;
	__u8  mtu;
	__u8  rsvd0;
	__u32 create_flags;
	__u32 inline_size;
	__u32 rsvd1;
	__u64 driver_data[0];
};

struct ibv_exp_create_dct_resp {
	__u32 dct_handle;
	__u32 dct_num;
	__u32 inline_size;
	__u32 rsvd;
};

struct ibv_exp_destroy_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 rsvd;
	__u64 driver_data[0];
};

struct ibv_exp_destroy_dct_resp {
	__u32	events_reported;
	__u32	reserved;
};

struct ibv_exp_query_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_dct_resp {
	__u64	dc_key;
	__u32	access_flags;
	__u32	flow_label;
	__u32	key_violations;
	__u8	port;
	__u8	min_rnr_timer;
	__u8	tclass;
	__u8	mtu;
	__u8	pkey_index;
	__u8	gid_index;
	__u8	hop_limit;
	__u8	state;
	__u32	rsvd;
	__u64	driver_data[0];
};

struct ibv_exp_arm_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_arm_dct_resp {
	__u64	reserved;
};

struct ibv_exp_modify_cq {
	struct ex_hdr hdr;
	__u32 cq_handle;
	__u32 attr_mask;
	__u16 cq_count;
	__u16 cq_period;
	__u32 cq_cap_flags;
	__u32 comp_mask;
	__u32 rsvd;
};

struct ibv_exp_modify_qp {
	struct ex_hdr hdr;
	__u32 comp_mask;
	struct ibv_qp_dest dest;
	struct ibv_qp_dest alt_dest;
	__u32 qp_handle;
	__u32 attr_mask;
	__u32 qkey;
	__u32 rq_psn;
	__u32 sq_psn;
	__u32 dest_qp_num;
	__u32 qp_access_flags;
	__u16 pkey_index;
	__u16 alt_pkey_index;
	__u8  qp_state;
	__u8  cur_qp_state;
	__u8  path_mtu;
	__u8  path_mig_state;
	__u8  en_sqd_async_notify;
	__u8  max_rd_atomic;
	__u8  max_dest_rd_atomic;
	__u8  min_rnr_timer;
	__u8  port_num;
	__u8  timeout;
	__u8  retry_cnt;
	__u8  rnr_retry;
	__u8  alt_port_num;
	__u8  alt_timeout;
	__u8  reserved[6];
	__u64 dct_key;
	__u32 exp_attr_mask;
	__u32 flow_entropy;
	__u64 driver_data[0];
	__u32 rate_limit;
	__u32 reserved1;
};

enum ibv_exp_create_cq_comp_mask {
	IBV_EXP_CREATE_CQ_CAP_FLAGS	= (uint64_t)1 << 0,
};

struct ibv_exp_create_cq {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 cqe;
	__u32 comp_vector;
	__s32 comp_channel;
	__u32 reserved;
	__u64 create_flags;
	__u64 driver_data[0];
};

struct ibv_exp_create_mr {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 pd_handle;
	__u32 max_klm_list_size;
	__u64 exp_access_flags;
	__u32 create_flags;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_create_mr_resp {
	__u64 comp_mask;
	__u32 handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_mkey {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_mkey_resp {
	__u64 comp_mask;
	__u32 max_klm_list_size;
	__u32 reserved;
	__u64 driver_data[0];
};

enum ibv_exp_reg_mr_comp_mask {
	IBV_EXP_REG_MR_EXP_ACCESS_FLAGS = 1ULL << 0,
};

struct ibv_exp_reg_mr {
	struct ex_hdr hdr;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 reserved;
	__u64 exp_access_flags;
	__u64 comp_mask;
};

struct ibv_exp_prefetch_mr {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 mr_handle;
	__u32 flags;
	__u64 start;
	__u64 length;
};

struct ibv_exp_reg_mr_resp {
	__u32 mr_handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 comp_mask;
};

struct ibv_exp_rereg_mr {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 mr_handle;
	__u32 flags;
	__u32 reserved;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 access_flags;
};

struct ibv_exp_rereg_mr_resp {
	__u32 comp_mask;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
};

struct ibv_exp_cmd_wq_mp_rq {
	__u32 use_shift; /* use ibv_exp_mp_rq_shifts */
	__u8  single_wqe_log_num_of_strides;
	__u8  single_stride_log_num_of_bytes;
	__u16 reserved;
};

enum ibv_exp_cmd_create_wq_comp_mask {
	IBV_EXP_CMD_CREATE_WQ_MP_RQ		= 1 << 0,
	IBV_EXP_CMD_CREATE_WQ_VLAN_OFFLOADS	= 1 << 1,
	IBV_EXP_CMD_CREATE_WQ_FLAGS		= 1 << 2,
};

struct ibv_exp_create_wq {
	struct ex_hdr hdr;
	__u32 comp_mask; /* enum ibv_exp_cmd_create_wq_comp_mask */
	__u32 wq_type; /* enum ibv_exp_wq_type */
	__u64 user_handle;
	__u32 pd_handle;
	__u32 cq_handle;
	__u32 srq_handle;
	__u32 max_recv_wr;
	__u32 max_recv_sge;
	__u32 reserved;
	struct ibv_exp_cmd_wq_mp_rq mp_rq;
	__u16 wq_vlan_offloads;
	__u8 reserved1[6];
	__u64 flags;
};

struct ibv_exp_create_wq_resp {
	__u32 comp_mask;
	__u32 response_length;
	__u32 wq_handle;
	__u32 max_recv_wr;
	__u32 max_recv_sge;
	__u32 wqn;
};

struct ib_exp_destroy_wq {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 wq_handle;
};

struct ib_exp_modify_wq  {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 wq_handle;
	__u32 wq_state;
	__u32 curr_wq_state;
	__u16 wq_vlan_offloads;
	__u8 reserved[6];
};

struct ibv_exp_create_rwq_ind_table {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 pd_handle;
	__u32 log_ind_tbl_size;
	__u32 reserved;
	/* Following are wq handles based on log_ind_tbl_size, must be 64 bytes aligned.
	 * __u32 wq_handle1
	 * __u32 wq_handle2
	 */
};

struct ibv_exp_create_rwq_ind_table_resp {
	__u32 comp_mask;
	__u32 response_length;
	__u32 ind_tbl_handle;
	__u32 ind_tbl_num;
};

struct ibv_exp_destroy_rwq_ind_table {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 ind_tbl_handle;
};

struct ibv_exp_kern_ipv6_filter {
	__u8 src_ip[16];
	__u8 dst_ip[16];
};

struct ibv_exp_kern_spec_ipv6 {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_exp_kern_ipv6_filter val;
	struct ibv_exp_kern_ipv6_filter mask;
};

struct ibv_exp_kern_ipv6_ext_filter {
	__u8 src_ip[16];
	__u8 dst_ip[16];
	__u32 flow_label;
	__u8  next_hdr;
	__u8  traffic_class;
	__u8  hop_limit;
	__u8  reserved;
};

struct ibv_exp_kern_spec_ipv6_ext {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_exp_kern_ipv6_ext_filter val;
	struct ibv_exp_kern_ipv6_ext_filter mask;
};

struct ibv_exp_kern_ipv4_ext_filter {
	__u32 src_ip;
	__u32 dst_ip;
	__u8  proto;
	__u8  tos;
	__u8  ttl;
	__u8  flags;
};

struct ibv_exp_kern_spec_ipv4_ext {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_exp_kern_ipv4_ext_filter val;
	struct ibv_exp_kern_ipv4_ext_filter mask;
};

struct ibv_exp_kern_tunnel_filter {
	__u32 tunnel_id;
};

struct ibv_exp_kern_spec_tunnel {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_exp_kern_tunnel_filter val;
	struct ibv_exp_kern_tunnel_filter mask;
};

struct ibv_exp_kern_spec_action_tag {
	__u32  type;
	__u16  size;
	__u16 reserved;
	__u32 tag_id;
	__u32 reserved1;
};

struct ibv_exp_kern_spec {
	union {
		struct {
			__u32 type;
			__u16 size;
			__u16 reserved;
		} hdr;
		struct ibv_kern_spec_ib ib;
		struct ibv_kern_spec_eth eth;
		struct ibv_kern_spec_ipv4 ipv4;
		struct ibv_exp_kern_spec_ipv4_ext ipv4_ext;
		struct ibv_kern_spec_tcp_udp tcp_udp;
		struct ibv_exp_kern_spec_ipv6 ipv6;
		struct ibv_exp_kern_spec_ipv6_ext ipv6_ext;
		struct ibv_exp_kern_spec_tunnel tunnel;
		struct ibv_exp_kern_spec_action_tag flow_tag;
	};
};
#endif /* KERN_ABI_EXP_H */
