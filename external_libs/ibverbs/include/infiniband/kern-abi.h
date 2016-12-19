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

#ifndef KERN_ABI_H
#define KERN_ABI_H

#include <linux/types.h>

/*
 * This file must be kept in sync with the kernel's version of
 * drivers/infiniband/include/ib_user_verbs.h
 */

/*
 * The minimum and maximum kernel ABI that we can handle.
 */
#define IB_USER_VERBS_MIN_ABI_VERSION	3
#define IB_USER_VERBS_MAX_ABI_VERSION	6

#define IB_USER_VERBS_CMD_THRESHOLD    50

enum {
	IB_USER_VERBS_CMD_GET_CONTEXT,
	IB_USER_VERBS_CMD_QUERY_DEVICE,
	IB_USER_VERBS_CMD_QUERY_PORT,
	IB_USER_VERBS_CMD_ALLOC_PD,
	IB_USER_VERBS_CMD_DEALLOC_PD,
	IB_USER_VERBS_CMD_CREATE_AH,
	IB_USER_VERBS_CMD_MODIFY_AH,
	IB_USER_VERBS_CMD_QUERY_AH,
	IB_USER_VERBS_CMD_DESTROY_AH,
	IB_USER_VERBS_CMD_REG_MR,
	IB_USER_VERBS_CMD_REG_SMR,
	IB_USER_VERBS_CMD_REREG_MR,
	IB_USER_VERBS_CMD_QUERY_MR,
	IB_USER_VERBS_CMD_DEREG_MR,
	IB_USER_VERBS_CMD_ALLOC_MW,
	IB_USER_VERBS_CMD_BIND_MW,
	IB_USER_VERBS_CMD_DEALLOC_MW,
	IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL,
	IB_USER_VERBS_CMD_CREATE_CQ,
	IB_USER_VERBS_CMD_RESIZE_CQ,
	IB_USER_VERBS_CMD_DESTROY_CQ,
	IB_USER_VERBS_CMD_POLL_CQ,
	IB_USER_VERBS_CMD_PEEK_CQ,
	IB_USER_VERBS_CMD_REQ_NOTIFY_CQ,
	IB_USER_VERBS_CMD_CREATE_QP,
	IB_USER_VERBS_CMD_QUERY_QP,
	IB_USER_VERBS_CMD_MODIFY_QP,
	IB_USER_VERBS_CMD_DESTROY_QP,
	IB_USER_VERBS_CMD_POST_SEND,
	IB_USER_VERBS_CMD_POST_RECV,
	IB_USER_VERBS_CMD_ATTACH_MCAST,
	IB_USER_VERBS_CMD_DETACH_MCAST,
	IB_USER_VERBS_CMD_CREATE_SRQ,
	IB_USER_VERBS_CMD_MODIFY_SRQ,
	IB_USER_VERBS_CMD_QUERY_SRQ,
	IB_USER_VERBS_CMD_DESTROY_SRQ,
	IB_USER_VERBS_CMD_POST_SRQ_RECV,
	IB_USER_VERBS_CMD_OPEN_XRCD,
	IB_USER_VERBS_CMD_CLOSE_XRCD,
	IB_USER_VERBS_CMD_CREATE_XSRQ,
	IB_USER_VERBS_CMD_OPEN_QP,
};


#define IB_USER_VERBS_CMD_COMMAND_MASK		0xff
#define IB_USER_VERBS_CMD_FLAGS_MASK		0xff000000u
#define IB_USER_VERBS_CMD_FLAGS_SHIFT		24


#define IB_USER_VERBS_CMD_FLAG_EXTENDED		0x80ul


enum {
	IB_USER_VERBS_CMD_CREATE_FLOW = (IB_USER_VERBS_CMD_FLAG_EXTENDED <<
					 IB_USER_VERBS_CMD_FLAGS_SHIFT) +
					IB_USER_VERBS_CMD_THRESHOLD,
	IB_USER_VERBS_CMD_DESTROY_FLOW
};

/*
 * Make sure that all structs defined in this file remain laid out so
 * that they pack the same way on 32-bit and 64-bit architectures (to
 * avoid incompatibility between 32-bit userspace and 64-bit kernels).
 * Specifically:
 *  - Do not use pointer types -- pass pointers in __u64 instead.
 *  - Make sure that any structure larger than 4 bytes is padded to a
 *    multiple of 8 bytes.  Otherwise the structure size will be
 *    different between 32-bit and 64-bit architectures.
 */

struct hdr {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
};

struct response_hdr {
	__u64 response;
};

struct ex_hdr {
	struct {
		__u32 command;
		__u16 in_words;
		__u16 out_words;
	};
	struct {
		__u64 response;
	};
	struct {
		__u16 provider_in_words;
		__u16 provider_out_words;
		__u32 reserved;
	};
};

enum ibv_event_rsc_type {
	IBV_EVENT_RSC_CQ,
	IBV_EVENT_RSC_QP,
	IBV_EVENT_RSC_DCT,
	IBV_EVENT_RSC_SRQ,
	IBV_EVENT_RSC_DEVICE,
};

struct ibv_kern_async_event {
	__u64 element;
	__u32 event_type;
	__u32 rsc_type;
};

struct ibv_comp_event {
	__u64 cq_handle;
};

/*
 * All commands from userspace should start with a __u32 command field
 * followed by __u16 in_words and out_words fields (which give the
 * length of the command block and response buffer if any in 32-bit
 * words).  The kernel driver will read these fields first and read
 * the rest of the command struct based on these value.
 */

#define IBV_RESP_TO_VERBS_RESP_EX_RAW(ex_ptr, ex_type, ibv_type, field) \
	((ibv_type *)((void *)(ex_ptr) + offsetof(ex_type,		\
	field) + sizeof((ex_ptr)->field)))

#define IBV_RESP_TO_VERBS_RESP_EX(ex_ptr, ex_type, ibv_type) \
	IBV_RESP_TO_VERBS_RESP_EX_RAW(ex_ptr, ex_type, ibv_type, comp_mask)

struct ibv_query_params {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
};

struct ibv_query_params_resp {
	__u32 num_cq_events;
};

struct ibv_get_context {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 driver_data[0];
};

struct ibv_get_context_resp {
	__u32 async_fd;
	__u32 num_comp_vectors;
};

struct ibv_query_device {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 driver_data[0];
};

struct ibv_query_device_resp {
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
	__u32 atomic_cap;
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
};

struct ibv_query_port {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u8  port_num;
	__u8  reserved[7];
	__u64 driver_data[0];
};

struct ibv_query_port_resp {
	__u32 port_cap_flags;
	__u32 max_msg_sz;
	__u32 bad_pkey_cntr;
	__u32 qkey_viol_cntr;
	__u32 gid_tbl_len;
	__u16 pkey_tbl_len;
	__u16 lid;
	__u16 sm_lid;
	__u8  state;
	__u8  max_mtu;
	__u8  active_mtu;
	__u8  lmc;
	__u8  max_vl_num;
	__u8  sm_sl;
	__u8  subnet_timeout;
	__u8  init_type_reply;
	__u8  active_width;
	__u8  active_speed;
	__u8  phys_state;
	__u8  link_layer;
	__u8  reserved[2];
};

struct ibv_alloc_pd {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 driver_data[0];
};

struct ibv_alloc_pd_resp {
	__u32 pd_handle;
};

struct ibv_dealloc_pd {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 pd_handle;
};

struct ibv_open_xrcd {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 fd;
	__u32 oflags;
	__u64 driver_data[0];
};

struct ibv_open_xrcd_resp {
	__u32 xrcd_handle;
};

struct ibv_close_xrcd {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 xrcd_handle;
};

struct ibv_reg_mr {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 access_flags;
	__u64 driver_data[0];
};

struct ibv_reg_mr_resp {
	__u32 mr_handle;
	__u32 lkey;
	__u32 rkey;
};

struct ibv_dereg_mr {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 mr_handle;
};

struct ibv_alloc_mw {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 pd_handle;
	__u8  mw_type;
	__u8  reserved[3];
};

struct ibv_alloc_mw_resp {
	__u32 mw_handle;
	__u32 rkey;
};

struct ibv_dealloc_mw {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 mw_handle;
};

struct ibv_create_comp_channel {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
};

struct ibv_create_comp_channel_resp {
	__u32 fd;
};

struct ibv_create_cq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 user_handle;
	__u32 cqe;
	__u32 comp_vector;
	__s32 comp_channel;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_create_cq_resp {
	__u32 cq_handle;
	__u32 cqe;
};

struct ibv_kern_wc {
	__u64  wr_id;
	__u32  status;
	__u32  opcode;
	__u32  vendor_err;
	__u32  byte_len;
	__u32  imm_data;
	__u32  qp_num;
	__u32  src_qp;
	__u32  wc_flags;
	__u16  pkey_index;
	__u16  slid;
	__u8   sl;
	__u8   dlid_path_bits;
	__u8   port_num;
	__u8   reserved;
};

struct ibv_poll_cq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 cq_handle;
	__u32 ne;
};

struct ibv_poll_cq_resp {
	__u32 count;
	__u32 reserved;
	struct ibv_kern_wc wc[0];
};

struct ibv_req_notify_cq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 cq_handle;
	__u32 solicited;
};

struct ibv_resize_cq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 cq_handle;
	__u32 cqe;
	__u64 driver_data[0];
};

struct ibv_resize_cq_resp {
	__u32 cqe;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_destroy_cq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 cq_handle;
	__u32 reserved;
};

struct ibv_destroy_cq_resp {
	__u32 comp_events_reported;
	__u32 async_events_reported;
};

struct ibv_kern_global_route {
	__u8  dgid[16];
	__u32 flow_label;
	__u8  sgid_index;
	__u8  hop_limit;
	__u8  traffic_class;
	__u8  reserved;
};

struct ibv_kern_ah_attr {
	struct ibv_kern_global_route grh;
	__u16 dlid;
	__u8  sl;
	__u8  src_path_bits;
	__u8  static_rate;
	__u8  is_global;
	__u8  port_num;
	__u8  reserved;
};

struct ibv_kern_qp_attr {
	__u32	qp_attr_mask;
	__u32	qp_state;
	__u32	cur_qp_state;
	__u32	path_mtu;
	__u32	path_mig_state;
	__u32	qkey;
	__u32	rq_psn;
	__u32	sq_psn;
	__u32	dest_qp_num;
	__u32	qp_access_flags;

	struct ibv_kern_ah_attr ah_attr;
	struct ibv_kern_ah_attr alt_ah_attr;

	/* ib_qp_cap */
	__u32	max_send_wr;
	__u32	max_recv_wr;
	__u32	max_send_sge;
	__u32	max_recv_sge;
	__u32	max_inline_data;

	__u16	pkey_index;
	__u16	alt_pkey_index;
	__u8	en_sqd_async_notify;
	__u8	sq_draining;
	__u8	max_rd_atomic;
	__u8	max_dest_rd_atomic;
	__u8	min_rnr_timer;
	__u8	port_num;
	__u8	timeout;
	__u8	retry_cnt;
	__u8	rnr_retry;
	__u8	alt_port_num;
	__u8	alt_timeout;
	__u8	reserved[5];
};

struct ibv_create_qp {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
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
	__u64 driver_data[0];
};

struct ibv_open_qp {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 qpn;
	__u8  qp_type;
	__u8  reserved[7];
	__u64 driver_data[0];
};

/* also used for open response */
struct ibv_create_qp_resp {
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 reserved;
};

enum ibv_create_qp_ex_comp_mask {
	IBV_CREATE_QP_EX_CAP_FLAGS          = (1ULL << 0)
};

struct ibv_create_qp_ex {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u16 provider_in_words;
	__u16 provider_out_words;
	__u32 cmd_hdr_reserved;
	__u64 comp_mask;
	__u64 response;
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
	__u64 driver_data[0];
};

struct ibv_create_qp_resp_ex {
	__u64 comp_mask;
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 reserved;
};

struct ibv_qp_dest {
	__u8  dgid[16];
	__u32 flow_label;
	__u16 dlid;
	__u16 reserved;
	__u8  sgid_index;
	__u8  hop_limit;
	__u8  traffic_class;
	__u8  sl;
	__u8  src_path_bits;
	__u8  static_rate;
	__u8  is_global;
	__u8  port_num;
};

struct ibv_query_qp {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 qp_handle;
	__u32 attr_mask;
	__u64 driver_data[0];
};

struct ibv_query_qp_resp {
	struct ibv_qp_dest dest;
	struct ibv_qp_dest alt_dest;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
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
	__u8  sq_draining;
	__u8  max_rd_atomic;
	__u8  max_dest_rd_atomic;
	__u8  min_rnr_timer;
	__u8  port_num;
	__u8  timeout;
	__u8  retry_cnt;
	__u8  rnr_retry;
	__u8  alt_port_num;
	__u8  alt_timeout;
	__u8  sq_sig_all;
	__u8  reserved[5];
	__u64 driver_data[0];
};

struct ibv_modify_qp {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
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
	__u8  reserved[2];
	__u64 driver_data[0];
};

struct ibv_destroy_qp {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 qp_handle;
	__u32 reserved;
};

struct ibv_destroy_qp_resp {
	__u32 events_reported;
};

struct ibv_kern_send_wr {
	__u64 wr_id;
	__u32 num_sge;
	__u32 opcode;
	__u32 send_flags;
	__u32 imm_data;
	union {
		struct {
			__u64 remote_addr;
			__u32 rkey;
			__u32 reserved;
		} rdma;
		struct {
			__u64 remote_addr;
			__u64 compare_add;
			__u64 swap;
			__u32 rkey;
			__u32 reserved;
		} atomic;
		struct {
			__u32 ah;
			__u32 remote_qpn;
			__u32 remote_qkey;
			__u32 reserved;
		} ud;
	} wr;
	union {
		struct {
			__u32 remote_srqn;
		} xrc;
	} qp_type;
};

struct ibv_kern_eth_filter {
	__u8  dst_mac[6];
	__u8  src_mac[6];
	__u16  ether_type;
	__u16  vlan_tag;
};

struct ibv_kern_spec_eth {
	__u32 type;
	__u16  size;
	__u16 reserved;
	struct ibv_kern_eth_filter val;
	struct ibv_kern_eth_filter mask;
};

struct ibv_kern_ib_filter {
	__u32 qpn;
	__u8  dst_gid[16];
};

struct ibv_kern_spec_ib {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_kern_ib_filter val;
	struct ibv_kern_ib_filter mask;
};

struct ibv_kern_ipv4_filter {
	__u32 src_ip;
	__u32 dst_ip;
};

struct ibv_kern_spec_ipv4 {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_kern_ipv4_filter val;
	struct ibv_kern_ipv4_filter mask;
};

struct ibv_kern_tcp_udp_filter {
	__u16 dst_port;
	__u16 src_port;
};

struct ibv_kern_spec_tcp_udp {
	__u32  type;
	__u16  size;
	__u16 reserved;
	struct ibv_kern_tcp_udp_filter val;
	struct ibv_kern_tcp_udp_filter mask;
};


struct ibv_kern_spec {
	union {
		struct {
			__u32 type;
			__u16 size;
			__u16 reserved;
		} hdr;
		struct ibv_kern_spec_ib ib;
		struct ibv_kern_spec_eth eth;
		struct ibv_kern_spec_ipv4 ipv4;
		struct ibv_kern_spec_tcp_udp tcp_udp;
	};

};

struct ibv_kern_flow_attr {
	__u32 type;
	__u16 size;
	__u16 priority;
	__u8 num_of_specs;
	__u8 reserved[2];
	__u8 port;
	__u32 flags;
	/* Following are the optional layers according to user request
	 * struct ibv_kern_flow_spec_xxx
	 * struct ibv_kern_flow_spec_yyy
	 */
};

struct ibv_post_send {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 qp_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ibv_kern_send_wr send_wr[0];
};

struct ibv_post_send_resp {
	__u32 bad_wr;
};

struct ibv_kern_recv_wr {
	__u64 wr_id;
	__u32 num_sge;
	__u32 reserved;
};

struct ibv_post_recv {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 qp_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ibv_kern_recv_wr recv_wr[0];
};

struct ibv_post_recv_resp {
	__u32 bad_wr;
};

struct ibv_post_srq_recv {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 srq_handle;
	__u32 wr_count;
	__u32 sge_count;
	__u32 wqe_size;
	struct ibv_kern_recv_wr recv_wr[0];
};

struct ibv_post_srq_recv_resp {
	__u32 bad_wr;
};

struct ibv_create_ah {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 reserved;
	struct ibv_kern_ah_attr attr;
};

struct ibv_create_ah_resp {
	__u32 handle;
};

struct ibv_destroy_ah {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 ah_handle;
};

struct ibv_attach_mcast {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u8  gid[16];
	__u32 qp_handle;
	__u16 mlid;
	__u16 reserved;
	__u64 driver_data[0];
};

struct ibv_detach_mcast {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u8  gid[16];
	__u32 qp_handle;
	__u16 mlid;
	__u16 reserved;
	__u64 driver_data[0];
};

struct ibv_create_flow  {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 qp_handle;
	struct ibv_kern_flow_attr flow_attr;
};

struct ibv_create_flow_resp {
	__u32 comp_mask;
	__u32 flow_handle;
};

struct ibv_destroy_flow  {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 flow_handle;
};

struct ibv_create_srq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u64 driver_data[0];
};

struct ibv_create_xsrq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u64 user_handle;
	__u32 srq_type;
	__u32 pd_handle;
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u32 reserved;
	__u32 xrcd_handle;
	__u32 cq_handle;
	__u64 driver_data[0];
};

struct ibv_create_srq_resp {
	__u32 srq_handle;
	__u32 max_wr;
	__u32 max_sge;
	__u32 srqn;
};

struct ibv_modify_srq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 srq_handle;
	__u32 attr_mask;
	__u32 max_wr;
	__u32 srq_limit;
	__u64 driver_data[0];
};

struct ibv_query_srq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 srq_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_query_srq_resp {
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u32 reserved;
};

struct ibv_destroy_srq {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u64 response;
	__u32 srq_handle;
	__u32 reserved;
};

struct ibv_destroy_srq_resp {
	__u32 events_reported;
};

/*
 * Compatibility with older ABI versions
 */

enum {
	IB_USER_VERBS_CMD_QUERY_PARAMS_V2,
	IB_USER_VERBS_CMD_GET_CONTEXT_V2,
	IB_USER_VERBS_CMD_QUERY_DEVICE_V2,
	IB_USER_VERBS_CMD_QUERY_PORT_V2,
	IB_USER_VERBS_CMD_QUERY_GID_V2,
	IB_USER_VERBS_CMD_QUERY_PKEY_V2,
	IB_USER_VERBS_CMD_ALLOC_PD_V2,
	IB_USER_VERBS_CMD_DEALLOC_PD_V2,
	IB_USER_VERBS_CMD_CREATE_AH_V2,
	IB_USER_VERBS_CMD_MODIFY_AH_V2,
	IB_USER_VERBS_CMD_QUERY_AH_V2,
	IB_USER_VERBS_CMD_DESTROY_AH_V2,
	IB_USER_VERBS_CMD_REG_MR_V2,
	IB_USER_VERBS_CMD_REG_SMR_V2,
	IB_USER_VERBS_CMD_REREG_MR_V2,
	IB_USER_VERBS_CMD_QUERY_MR_V2,
	IB_USER_VERBS_CMD_DEREG_MR_V2,
	IB_USER_VERBS_CMD_ALLOC_MW_V2,
	IB_USER_VERBS_CMD_BIND_MW_V2,
	IB_USER_VERBS_CMD_DEALLOC_MW_V2,
	IB_USER_VERBS_CMD_CREATE_CQ_V2,
	IB_USER_VERBS_CMD_RESIZE_CQ_V2,
	IB_USER_VERBS_CMD_DESTROY_CQ_V2,
	IB_USER_VERBS_CMD_POLL_CQ_V2,
	IB_USER_VERBS_CMD_PEEK_CQ_V2,
	IB_USER_VERBS_CMD_REQ_NOTIFY_CQ_V2,
	IB_USER_VERBS_CMD_CREATE_QP_V2,
	IB_USER_VERBS_CMD_QUERY_QP_V2,
	IB_USER_VERBS_CMD_MODIFY_QP_V2,
	IB_USER_VERBS_CMD_DESTROY_QP_V2,
	IB_USER_VERBS_CMD_POST_SEND_V2,
	IB_USER_VERBS_CMD_POST_RECV_V2,
	IB_USER_VERBS_CMD_ATTACH_MCAST_V2,
	IB_USER_VERBS_CMD_DETACH_MCAST_V2,
	IB_USER_VERBS_CMD_CREATE_SRQ_V2,
	IB_USER_VERBS_CMD_MODIFY_SRQ_V2,
	IB_USER_VERBS_CMD_QUERY_SRQ_V2,
	IB_USER_VERBS_CMD_DESTROY_SRQ_V2,
	IB_USER_VERBS_CMD_POST_SRQ_RECV_V2,
	/*
	 * Set commands that didn't exist to -1 so our compile-time
	 * trick opcodes in IBV_INIT_CMD() doesn't break.
	 */
	IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL_V2 = -1,
	IB_USER_VERBS_CMD_CREATE_QP_EX_V2 = -1,
	IB_USER_VERBS_CMD_MODIFY_CQ_EX_V2 = -1,
	IB_USER_VERBS_CMD_CREATE_FLOW_V2 = -1,
	IB_USER_VERBS_CMD_DESTROY_FLOW_V2 = -1,
	IB_USER_VERBS_CMD_OPEN_XRCD_V2 = -1,
	IB_USER_VERBS_CMD_CLOSE_XRCD_V2 = -1,
	IB_USER_VERBS_CMD_CREATE_XSRQ_V2 = -1,
	IB_USER_VERBS_CMD_OPEN_QP_V2 = -1,
	IB_USER_VERBS_CMD_MODIFY_QP_EX_V2 = -1,
	IB_USER_VERBS_CMD_CREATE_CQ_EX_V2 = -1,
	IB_USER_VERBS_CMD_QUERY_DEVICE_EX_V2 = -1,
	IB_USER_VERBS_CMD_CREATE_DCT_V2 = -1,
	IB_USER_VERBS_CMD_DESTROY_DCT_V2 = -1,
	IB_USER_VERBS_CMD_QUERY_DCT_V2 = -1,
	IB_USER_VERBS_CMD_EXP_REG_MR_V2 = -1,
	IB_USER_VERBS_CMD_EXP_PREFETCH_MR_V2 = -1,
};

struct ibv_modify_srq_v3 {
	__u32 command;
	__u16 in_words;
	__u16 out_words;
	__u32 srq_handle;
	__u32 attr_mask;
	__u32 max_wr;
	__u32 max_sge;
	__u32 srq_limit;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_create_qp_resp_v3 {
	__u32 qp_handle;
	__u32 qpn;
};

struct ibv_create_qp_resp_v4 {
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
};

struct ibv_create_srq_resp_v5 {
	__u32 srq_handle;
};

#endif /* KERN_ABI_H */
