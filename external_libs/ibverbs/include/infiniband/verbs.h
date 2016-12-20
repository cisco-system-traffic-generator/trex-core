/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2004, 2011-2012 Intel Corporation.  All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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

#ifndef INFINIBAND_VERBS_H
#define INFINIBAND_VERBS_H

#include <stdint.h>
#include <pthread.h>
#include <stddef.h>
#include <errno.h>
#include <infiniband/ofa_verbs.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else /* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif /* __cplusplus */

#if __GNUC__ >= 3
#  define __attribute_const __attribute__((const))
#else
#  define __attribute_const
#endif

BEGIN_C_DECLS

union ibv_gid {
	uint8_t			raw[16];
	struct {
		uint64_t	subnet_prefix;
		uint64_t	interface_id;
	} global;
};

#ifndef container_of
/**
  * container_of - cast a member of a structure out to the containing structure
  * @ptr:        the pointer to the member.
  * @type:       the type of the container struct this is embedded in.
  * @member:     the name of the member within the struct.
  *
 */
#define container_of(ptr, type, member) \
	((type *) ((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define vext_field_avail(type, fld, sz) (offsetof(type, fld) < (sz))

static void *__VERBS_ABI_IS_EXTENDED = ((uint8_t *)NULL) - 1;

enum ibv_node_type {
	IBV_NODE_UNKNOWN	= -1,
	IBV_NODE_CA 		= 1,
	IBV_NODE_SWITCH,
	IBV_NODE_ROUTER,
	IBV_NODE_RNIC,

	/* Leave a gap for future node types before starting with
	 * experimental node types.
	 */
	IBV_EXP_NODE_TYPE_START	= 32,
	IBV_EXP_NODE_MIC	= IBV_EXP_NODE_TYPE_START
};

enum ibv_transport_type {
	IBV_TRANSPORT_UNKNOWN	= -1,
	IBV_TRANSPORT_IB	= 0,
	IBV_TRANSPORT_IWARP,

	/* Leave a gap for future transport types before starting with
	 * experimental transport types.
	 */
	IBV_EXP_TRANSPORT_TYPE_START	= 32,
	IBV_EXP_TRANSPORT_SCIF		= IBV_EXP_TRANSPORT_TYPE_START
};

enum ibv_device_cap_flags {
	IBV_DEVICE_RESIZE_MAX_WR	= 1,
	IBV_DEVICE_BAD_PKEY_CNTR	= 1 <<  1,
	IBV_DEVICE_BAD_QKEY_CNTR	= 1 <<  2,
	IBV_DEVICE_RAW_MULTI		= 1 <<  3,
	IBV_DEVICE_AUTO_PATH_MIG	= 1 <<  4,
	IBV_DEVICE_CHANGE_PHY_PORT	= 1 <<  5,
	IBV_DEVICE_UD_AV_PORT_ENFORCE	= 1 <<  6,
	IBV_DEVICE_CURR_QP_STATE_MOD	= 1 <<  7,
	IBV_DEVICE_SHUTDOWN_PORT	= 1 <<  8,
	IBV_DEVICE_INIT_TYPE		= 1 <<  9,
	IBV_DEVICE_PORT_ACTIVE_EVENT	= 1 << 10,
	IBV_DEVICE_SYS_IMAGE_GUID	= 1 << 11,
	IBV_DEVICE_RC_RNR_NAK_GEN	= 1 << 12,
	IBV_DEVICE_SRQ_RESIZE		= 1 << 13,
	IBV_DEVICE_N_NOTIFY_CQ		= 1 << 14,
	IBV_DEVICE_XRC			= 1 << 20,
	IBV_DEVICE_MANAGED_FLOW_STEERING = 1 << 29
};

enum ibv_atomic_cap {
	IBV_ATOMIC_NONE,
	IBV_ATOMIC_HCA,
	IBV_ATOMIC_GLOB
};

struct ibv_device_attr {
	char			fw_ver[64];
	uint64_t		node_guid;
	uint64_t		sys_image_guid;
	uint64_t		max_mr_size;
	uint64_t		page_size_cap;
	uint32_t		vendor_id;
	uint32_t		vendor_part_id;
	uint32_t		hw_ver;
	int			max_qp;
	int			max_qp_wr;
	int			device_cap_flags;
	int			max_sge;
	int			max_sge_rd;
	int			max_cq;
	int			max_cqe;
	int			max_mr;
	int			max_pd;
	int			max_qp_rd_atom;
	int			max_ee_rd_atom;
	int			max_res_rd_atom;
	int			max_qp_init_rd_atom;
	int			max_ee_init_rd_atom;
	enum ibv_atomic_cap	atomic_cap;
	int			max_ee;
	int			max_rdd;
	int			max_mw;
	int			max_raw_ipv6_qp;
	int			max_raw_ethy_qp;
	int			max_mcast_grp;
	int			max_mcast_qp_attach;
	int			max_total_mcast_qp_attach;
	int			max_ah;
	int			max_fmr;
	int			max_map_per_fmr;
	int			max_srq;
	int			max_srq_wr;
	int			max_srq_sge;
	uint16_t		max_pkeys;
	uint8_t			local_ca_ack_delay;
	uint8_t			phys_port_cnt;
};

enum ibv_mtu {
	IBV_MTU_256  = 1,
	IBV_MTU_512  = 2,
	IBV_MTU_1024 = 3,
	IBV_MTU_2048 = 4,
	IBV_MTU_4096 = 5
};

enum ibv_port_state {
	IBV_PORT_NOP		= 0,
	IBV_PORT_DOWN		= 1,
	IBV_PORT_INIT		= 2,
	IBV_PORT_ARMED		= 3,
	IBV_PORT_ACTIVE		= 4,
	IBV_PORT_ACTIVE_DEFER	= 5
};

enum {
	IBV_LINK_LAYER_UNSPECIFIED,
	IBV_LINK_LAYER_INFINIBAND,
	IBV_LINK_LAYER_ETHERNET,

	/* Leave a gap for future link layer types before starting with
	 * experimental link layer.
	 */
	IBV_EXP_LINK_LAYER_START	= 32,
	IBV_EXP_LINK_LAYER_SCIF		= IBV_EXP_LINK_LAYER_START
};

enum ibv_port_cap_flags {
	IBV_PORT_SM				= 1 <<  1,
	IBV_PORT_NOTICE_SUP			= 1 <<  2,
	IBV_PORT_TRAP_SUP			= 1 <<  3,
	IBV_PORT_OPT_IPD_SUP			= 1 <<  4,
	IBV_PORT_AUTO_MIGR_SUP			= 1 <<  5,
	IBV_PORT_SL_MAP_SUP			= 1 <<  6,
	IBV_PORT_MKEY_NVRAM			= 1 <<  7,
	IBV_PORT_PKEY_NVRAM			= 1 <<  8,
	IBV_PORT_LED_INFO_SUP			= 1 <<  9,
	IBV_PORT_SYS_IMAGE_GUID_SUP		= 1 << 11,
	IBV_PORT_PKEY_SW_EXT_PORT_TRAP_SUP	= 1 << 12,
	IBV_PORT_EXTENDED_SPEEDS_SUP		= 1 << 14,
	IBV_PORT_CM_SUP				= 1 << 16,
	IBV_PORT_SNMP_TUNNEL_SUP		= 1 << 17,
	IBV_PORT_REINIT_SUP			= 1 << 18,
	IBV_PORT_DEVICE_MGMT_SUP		= 1 << 19,
	IBV_PORT_VENDOR_CLASS			= 1 << 24,
	IBV_PORT_CLIENT_REG_SUP			= 1 << 25,
	IBV_PORT_IP_BASED_GIDS			= 1 << 26,
};

struct ibv_port_attr {
	enum ibv_port_state	state;
	enum ibv_mtu		max_mtu;
	enum ibv_mtu		active_mtu;
	int			gid_tbl_len;
	uint32_t		port_cap_flags;
	uint32_t		max_msg_sz;
	uint32_t		bad_pkey_cntr;
	uint32_t		qkey_viol_cntr;
	uint16_t		pkey_tbl_len;
	uint16_t		lid;
	uint16_t		sm_lid;
	uint8_t			lmc;
	uint8_t			max_vl_num;
	uint8_t			sm_sl;
	uint8_t			subnet_timeout;
	uint8_t			init_type_reply;
	uint8_t			active_width;
	uint8_t			active_speed;
	uint8_t			phys_state;
	uint8_t			link_layer;
	uint8_t			reserved;
};

enum ibv_event_type {
	IBV_EVENT_CQ_ERR,
	IBV_EVENT_QP_FATAL,
	IBV_EVENT_QP_REQ_ERR,
	IBV_EVENT_QP_ACCESS_ERR,
	IBV_EVENT_COMM_EST,
	IBV_EVENT_SQ_DRAINED,
	IBV_EVENT_PATH_MIG,
	IBV_EVENT_PATH_MIG_ERR,
	IBV_EVENT_DEVICE_FATAL,
	IBV_EVENT_PORT_ACTIVE,
	IBV_EVENT_PORT_ERR,
	IBV_EVENT_LID_CHANGE,
	IBV_EVENT_PKEY_CHANGE,
	IBV_EVENT_SM_CHANGE,
	IBV_EVENT_SRQ_ERR,
	IBV_EVENT_SRQ_LIMIT_REACHED,
	IBV_EVENT_QP_LAST_WQE_REACHED,
	IBV_EVENT_CLIENT_REREGISTER,
	IBV_EVENT_GID_CHANGE,

	/* new experimental events start here leaving enough
	 * room for 14 events which should be enough
	 */
	IBV_EXP_EVENT_DCT_KEY_VIOLATION = 32,
	IBV_EXP_EVENT_DCT_ACCESS_ERR,
	IBV_EXP_EVENT_DCT_REQ_ERR,
};

struct ibv_async_event {
	union {
		struct ibv_cq  *cq;
		struct ibv_qp  *qp;
		struct ibv_srq *srq;
		struct ibv_exp_dct *dct;
		int		port_num;
		/* For source compatible with Legacy API */
		uint32_t	xrc_qp_num;
	} element;
	enum ibv_event_type	event_type;
};

enum ibv_wc_status {
	IBV_WC_SUCCESS,
	IBV_WC_LOC_LEN_ERR,
	IBV_WC_LOC_QP_OP_ERR,
	IBV_WC_LOC_EEC_OP_ERR,
	IBV_WC_LOC_PROT_ERR,
	IBV_WC_WR_FLUSH_ERR,
	IBV_WC_MW_BIND_ERR,
	IBV_WC_BAD_RESP_ERR,
	IBV_WC_LOC_ACCESS_ERR,
	IBV_WC_REM_INV_REQ_ERR,
	IBV_WC_REM_ACCESS_ERR,
	IBV_WC_REM_OP_ERR,
	IBV_WC_RETRY_EXC_ERR,
	IBV_WC_RNR_RETRY_EXC_ERR,
	IBV_WC_LOC_RDD_VIOL_ERR,
	IBV_WC_REM_INV_RD_REQ_ERR,
	IBV_WC_REM_ABORT_ERR,
	IBV_WC_INV_EECN_ERR,
	IBV_WC_INV_EEC_STATE_ERR,
	IBV_WC_FATAL_ERR,
	IBV_WC_RESP_TIMEOUT_ERR,
	IBV_WC_GENERAL_ERR
};
const char *ibv_wc_status_str(enum ibv_wc_status status);

enum ibv_wc_opcode {
	IBV_WC_SEND,
	IBV_WC_RDMA_WRITE,
	IBV_WC_RDMA_READ,
	IBV_WC_COMP_SWAP,
	IBV_WC_FETCH_ADD,
	IBV_WC_BIND_MW,
/*
 * Set value of IBV_WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & IBV_WC_RECV).
 */
	IBV_WC_RECV			= 1 << 7,
	IBV_WC_RECV_RDMA_WITH_IMM
};

enum ibv_wc_flags {
	IBV_WC_GRH		= 1 << 0,
	IBV_WC_WITH_IMM		= 1 << 1
};

struct ibv_wc {
	uint64_t		wr_id;
	enum ibv_wc_status	status;
	enum ibv_wc_opcode	opcode;
	uint32_t		vendor_err;
	uint32_t		byte_len;
	uint32_t		imm_data;	/* in network byte order */
	uint32_t		qp_num;
	uint32_t		src_qp;
	int			wc_flags;
	uint16_t		pkey_index;
	uint16_t		slid;
	uint8_t			sl;
	uint8_t			dlid_path_bits;
};

enum ibv_access_flags {
	IBV_ACCESS_LOCAL_WRITE		= 1,
	IBV_ACCESS_REMOTE_WRITE		= (1<<1),
	IBV_ACCESS_REMOTE_READ		= (1<<2),
	IBV_ACCESS_REMOTE_ATOMIC	= (1<<3),
	IBV_ACCESS_MW_BIND		= (1<<4)
};

struct ibv_pd {
	struct ibv_context     *context;
	uint32_t		handle;
};

enum ibv_xrcd_init_attr_mask {
	IBV_XRCD_INIT_ATTR_FD	    = 1 << 0,
	IBV_XRCD_INIT_ATTR_OFLAGS   = 1 << 1,
	IBV_XRCD_INIT_ATTR_RESERVED = 1 << 2
};

struct ibv_xrcd_init_attr {
	uint32_t comp_mask;
	int	 fd;
	int	 oflags;
};

struct ibv_xrcd {
	struct ibv_context     *context;
};

enum ibv_rereg_mr_flags {
	IBV_REREG_MR_CHANGE_TRANSLATION	= (1 << 0),
	IBV_REREG_MR_CHANGE_PD		= (1 << 1),
	IBV_REREG_MR_CHANGE_ACCESS	= (1 << 2),
	IBV_REREG_MR_KEEP_VALID		= (1 << 3)
};

struct ibv_mr {
	struct ibv_context     *context;
	struct ibv_pd	       *pd;
	void		       *addr;
	size_t			length;
	uint32_t		handle;
	uint32_t		lkey;
	uint32_t		rkey;
};

enum ibv_mw_type {
	IBV_MW_TYPE_1			= 1,
	IBV_MW_TYPE_2			= 2
};

struct ibv_mw {
	struct ibv_context     *context;
	struct ibv_pd	       *pd;
	uint32_t		rkey;
};

struct ibv_global_route {
	union ibv_gid		dgid;
	uint32_t		flow_label;
	uint8_t			sgid_index;
	uint8_t			hop_limit;
	uint8_t			traffic_class;
};

struct ibv_grh {
	uint32_t		version_tclass_flow;
	uint16_t		paylen;
	uint8_t			next_hdr;
	uint8_t			hop_limit;
	union ibv_gid		sgid;
	union ibv_gid		dgid;
};

enum ibv_rate {
	IBV_RATE_MAX      = 0,
	IBV_RATE_2_5_GBPS = 2,
	IBV_RATE_5_GBPS   = 5,
	IBV_RATE_10_GBPS  = 3,
	IBV_RATE_20_GBPS  = 6,
	IBV_RATE_30_GBPS  = 4,
	IBV_RATE_40_GBPS  = 7,
	IBV_RATE_60_GBPS  = 8,
	IBV_RATE_80_GBPS  = 9,
	IBV_RATE_120_GBPS = 10,
	IBV_RATE_14_GBPS  = 11,
	IBV_RATE_56_GBPS  = 12,
	IBV_RATE_112_GBPS = 13,
	IBV_RATE_168_GBPS = 14,
	IBV_RATE_25_GBPS  = 15,
	IBV_RATE_100_GBPS = 16,
	IBV_RATE_200_GBPS = 17,
	IBV_RATE_300_GBPS = 18
};

/**
 * ibv_rate_to_mult - Convert the IB rate enum to a multiple of the
 * base rate of 2.5 Gbit/sec.  For example, IBV_RATE_5_GBPS will be
 * converted to 2, since 5 Gbit/sec is 2 * 2.5 Gbit/sec.
 * @rate: rate to convert.
 */
int ibv_rate_to_mult(enum ibv_rate rate) __attribute_const;

/**
 * mult_to_ibv_rate - Convert a multiple of 2.5 Gbit/sec to an IB rate enum.
 * @mult: multiple to convert.
 */
enum ibv_rate mult_to_ibv_rate(int mult) __attribute_const;

/**
 * ibv_rate_to_mbps - Convert the IB rate enum to Mbit/sec.
 * For example, IBV_RATE_5_GBPS will return the value 5000.
 * @rate: rate to convert.
 */
int ibv_rate_to_mbps(enum ibv_rate rate) __attribute_const;

/**
 * mbps_to_ibv_rate - Convert a Mbit/sec value to an IB rate enum.
 * @mbps: value to convert.
 */
enum ibv_rate mbps_to_ibv_rate(int mbps) __attribute_const;

struct ibv_ah_attr {
	struct ibv_global_route	grh;
	uint16_t		dlid;
	uint8_t			sl;
	uint8_t			src_path_bits;
	uint8_t			static_rate;
	uint8_t			is_global;
	uint8_t			port_num;
};

enum ibv_srq_attr_mask {
	IBV_SRQ_MAX_WR	= 1 << 0,
	IBV_SRQ_LIMIT	= 1 << 1
};

struct ibv_srq_attr {
	uint32_t		max_wr;
	uint32_t		max_sge;
	uint32_t		srq_limit;
};

struct ibv_srq_init_attr {
	void		       *srq_context;
	struct ibv_srq_attr	attr;
};

enum ibv_srq_type {
	IBV_SRQT_BASIC,
	IBV_SRQT_XRC
};

enum ibv_srq_init_attr_mask {
	IBV_SRQ_INIT_ATTR_TYPE		= 1 << 0,
	IBV_SRQ_INIT_ATTR_PD		= 1 << 1,
	IBV_SRQ_INIT_ATTR_XRCD		= 1 << 2,
	IBV_SRQ_INIT_ATTR_CQ		= 1 << 3,
	IBV_SRQ_INIT_ATTR_RESERVED	= 1 << 4
};

struct ibv_srq_init_attr_ex {
	void		       *srq_context;
	struct ibv_srq_attr	attr;

	uint32_t		comp_mask;
	enum ibv_srq_type	srq_type;
	struct ibv_pd	       *pd;
	struct ibv_xrcd	       *xrcd;
	struct ibv_cq	       *cq;
};

enum ibv_qp_type {
	IBV_QPT_RC = 2,
	IBV_QPT_UC,
	IBV_QPT_UD,
	/* XRC compatible code */
	IBV_QPT_XRC,
	IBV_QPT_RAW_PACKET = 8,
	IBV_QPT_RAW_ETH = 8,
	IBV_QPT_XRC_SEND = 9,
	IBV_QPT_XRC_RECV,

	/* Leave a gap for future qp types before starting with
	 * experimental qp types.
	 */
	IBV_EXP_QP_TYPE_START	= 32,
	IBV_EXP_QPT_DC_INI	= IBV_EXP_QP_TYPE_START
};

struct ibv_qp_cap {
	uint32_t		max_send_wr;
	uint32_t		max_recv_wr;
	uint32_t		max_send_sge;
	uint32_t		max_recv_sge;
	uint32_t		max_inline_data;
};

struct ibv_qp_init_attr {
	void		       *qp_context;
	struct ibv_cq	       *send_cq;
	struct ibv_cq	       *recv_cq;
	struct ibv_srq	       *srq;
	struct ibv_qp_cap	cap;
	enum ibv_qp_type	qp_type;
	int			sq_sig_all;
	/* Below is needed for backwards compatabile */
	struct ibv_xrc_domain  *xrc_domain;
};

enum ibv_qp_init_attr_mask {
	IBV_QP_INIT_ATTR_PD		= 1 << 0,
	IBV_QP_INIT_ATTR_XRCD		= 1 << 1,
	IBV_QP_INIT_ATTR_RESERVED	= 1 << 2
};

struct ibv_qp_init_attr_ex {
	void		       *qp_context;
	struct ibv_cq	       *send_cq;
	struct ibv_cq	       *recv_cq;
	struct ibv_srq	       *srq;
	struct ibv_qp_cap	cap;
	enum ibv_qp_type	qp_type;
	int			sq_sig_all;

	uint32_t		comp_mask;
	struct ibv_pd	       *pd;
	struct ibv_xrcd	       *xrcd;
};

enum ibv_qp_open_attr_mask {
	IBV_QP_OPEN_ATTR_NUM		= 1 << 0,
	IBV_QP_OPEN_ATTR_XRCD		= 1 << 1,
	IBV_QP_OPEN_ATTR_CONTEXT	= 1 << 2,
	IBV_QP_OPEN_ATTR_TYPE		= 1 << 3,
	IBV_QP_OPEN_ATTR_RESERVED	= 1 << 4
};

struct ibv_qp_open_attr {
	uint32_t		comp_mask;
	uint32_t		qp_num;
	struct ibv_xrcd        *xrcd;
	void		       *qp_context;
	enum ibv_qp_type	qp_type;
};

enum ibv_qp_attr_mask {
	IBV_QP_STATE			= 1 << 	0,
	IBV_QP_CUR_STATE		= 1 << 	1,
	IBV_QP_EN_SQD_ASYNC_NOTIFY	= 1 << 	2,
	IBV_QP_ACCESS_FLAGS		= 1 << 	3,
	IBV_QP_PKEY_INDEX		= 1 << 	4,
	IBV_QP_PORT			= 1 << 	5,
	IBV_QP_QKEY			= 1 << 	6,
	IBV_QP_AV			= 1 << 	7,
	IBV_QP_PATH_MTU			= 1 << 	8,
	IBV_QP_TIMEOUT			= 1 << 	9,
	IBV_QP_RETRY_CNT		= 1 << 10,
	IBV_QP_RNR_RETRY		= 1 << 11,
	IBV_QP_RQ_PSN			= 1 << 12,
	IBV_QP_MAX_QP_RD_ATOMIC		= 1 << 13,
	IBV_QP_ALT_PATH			= 1 << 14,
	IBV_QP_MIN_RNR_TIMER		= 1 << 15,
	IBV_QP_SQ_PSN			= 1 << 16,
	IBV_QP_MAX_DEST_RD_ATOMIC	= 1 << 17,
	IBV_QP_PATH_MIG_STATE		= 1 << 18,
	IBV_QP_CAP			= 1 << 19,
	IBV_QP_DEST_QPN			= 1 << 20
};

enum ibv_qp_state {
	IBV_QPS_RESET,
	IBV_QPS_INIT,
	IBV_QPS_RTR,
	IBV_QPS_RTS,
	IBV_QPS_SQD,
	IBV_QPS_SQE,
	IBV_QPS_ERR,
	IBV_QPS_UNKNOWN
};

enum ibv_mig_state {
	IBV_MIG_MIGRATED,
	IBV_MIG_REARM,
	IBV_MIG_ARMED
};

struct ibv_qp_attr {
	enum ibv_qp_state	qp_state;
	enum ibv_qp_state	cur_qp_state;
	enum ibv_mtu		path_mtu;
	enum ibv_mig_state	path_mig_state;
	uint32_t		qkey;
	uint32_t		rq_psn;
	uint32_t		sq_psn;
	uint32_t		dest_qp_num;
	int			qp_access_flags;
	struct ibv_qp_cap	cap;
	struct ibv_ah_attr	ah_attr;
	struct ibv_ah_attr	alt_ah_attr;
	uint16_t		pkey_index;
	uint16_t		alt_pkey_index;
	uint8_t			en_sqd_async_notify;
	uint8_t			sq_draining;
	uint8_t			max_rd_atomic;
	uint8_t			max_dest_rd_atomic;
	uint8_t			min_rnr_timer;
	uint8_t			port_num;
	uint8_t			timeout;
	uint8_t			retry_cnt;
	uint8_t			rnr_retry;
	uint8_t			alt_port_num;
	uint8_t			alt_timeout;
};

enum ibv_wr_opcode {
	IBV_WR_RDMA_WRITE,
	IBV_WR_RDMA_WRITE_WITH_IMM,
	IBV_WR_SEND,
	IBV_WR_SEND_WITH_IMM,
	IBV_WR_RDMA_READ,
	IBV_WR_ATOMIC_CMP_AND_SWP,
	IBV_WR_ATOMIC_FETCH_AND_ADD
};

enum ibv_send_flags {
	IBV_SEND_FENCE		= 1 << 0,
	IBV_SEND_SIGNALED	= 1 << 1,
	IBV_SEND_SOLICITED	= 1 << 2,
	IBV_SEND_INLINE		= 1 << 3
};

struct ibv_sge {
	uint64_t		addr;
	uint32_t		length;
	uint32_t		lkey;
};

struct ibv_send_wr {
	uint64_t		wr_id;
	struct ibv_send_wr     *next;
	struct ibv_sge	       *sg_list;
	int			num_sge;
	enum ibv_wr_opcode	opcode;
	int			send_flags;
	uint32_t		imm_data;	/* in network byte order */
	union {
		struct {
			uint64_t	remote_addr;
			uint32_t	rkey;
		} rdma;
		struct {
			uint64_t	remote_addr;
			uint64_t	compare_add;
			uint64_t	swap;
			uint32_t	rkey;
		} atomic;
		struct {
			struct ibv_ah  *ah;
			uint32_t	remote_qpn;
			uint32_t	remote_qkey;
		} ud;
	} wr;
	union {
		union {
			struct {
				uint32_t    remote_srqn;
			} xrc;
		} qp_type;

		uint32_t		xrc_remote_srq_num;
	};
};

struct ibv_recv_wr {
	uint64_t		wr_id;
	struct ibv_recv_wr     *next;
	struct ibv_sge	       *sg_list;
	int			num_sge;
};

struct ibv_mw_bind {
	uint64_t		wr_id;
	struct ibv_mr	       *mr;
	void		       *addr;
	size_t			length;
	int			send_flags;
	int			mw_access_flags;
};

struct ibv_srq {
	struct ibv_context     *context;
	void		       *srq_context;
	struct ibv_pd	       *pd;
	uint32_t		handle;

	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	uint32_t		events_completed;

	/* below are for source compatabilty with legacy XRC,
	*   padding based on ibv_srq_legacy.
	*/
	uint32_t		xrc_srq_num_bin_compat_padding;
	struct ibv_xrc_domain	*xrc_domain_bin_compat_padding;
	struct ibv_cq	*xrc_cq_bin_compat_padding;
	void		*ibv_srq_padding;

	/* legacy fields */
	uint32_t		xrc_srq_num;
	struct ibv_xrc_domain	*xrc_domain;
	struct ibv_cq		*xrc_cq;
};

/* Not in use in new API, needed for compilation as part of source compat layer */
enum ibv_event_flags {
	IBV_XRC_QP_EVENT_FLAG = 0x80000000,
};



struct ibv_qp {
	struct ibv_context     *context;
	void		       *qp_context;
	struct ibv_pd	       *pd;
	struct ibv_cq	       *send_cq;
	struct ibv_cq	       *recv_cq;
	struct ibv_srq	       *srq;
	uint32_t		handle;
	uint32_t		qp_num;
	enum ibv_qp_state       state;
	enum ibv_qp_type	qp_type;

	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	uint32_t		events_completed;
};

struct ibv_comp_channel {
	struct ibv_context     *context;
	int			fd;
	int			refcnt;
};

struct ibv_cq {
	struct ibv_context     *context;
	struct ibv_comp_channel *channel;
	void		       *cq_context;
	uint32_t		handle;
	int			cqe;

	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	uint32_t		comp_events_completed;
	uint32_t		async_events_completed;
};

struct ibv_ah {
	struct ibv_context     *context;
	struct ibv_pd	       *pd;
	uint32_t		handle;
};

enum ibv_flow_flags {
	IBV_FLOW_ATTR_FLAGS_ALLOW_LOOP_BACK = 1,
	IBV_FLOW_ATTR_FLAGS_DONT_TRAP = 1 << 1,
};

enum ibv_flow_attr_type {
	/* steering according to rule specifications */
	IBV_FLOW_ATTR_NORMAL		= 0x0,
	/* default unicast and multicast rule -
	 * receive all Eth traffic which isn't steered to any QP
	 */
	IBV_FLOW_ATTR_ALL_DEFAULT	= 0x1,
	/* default multicast rule -
	 * receive all Eth multicast traffic which isn't steered to any QP
	 */
	IBV_FLOW_ATTR_MC_DEFAULT	= 0x2,
};

enum ibv_flow_spec_type {
	IBV_FLOW_SPEC_ETH	= 0x20,
	IBV_FLOW_SPEC_IPV4	= 0x30,
	IBV_FLOW_SPEC_TCP	= 0x40,
	IBV_FLOW_SPEC_UDP	= 0x41,
};

struct ibv_flow_eth_filter {
	uint8_t		dst_mac[6];
	uint8_t		src_mac[6];
	uint16_t	ether_type;
	/*
	 * same layout as 802.1q: prio 3, cfi 1, vlan id 12
	 */
	uint16_t	vlan_tag;
};

struct ibv_flow_spec_eth {
	enum ibv_flow_spec_type  type;
	uint16_t  size;
	struct ibv_flow_eth_filter val;
	struct ibv_flow_eth_filter mask;
};

struct ibv_flow_ipv4_filter {
	uint32_t src_ip;
	uint32_t dst_ip;
};

struct ibv_flow_spec_ipv4 {
	enum ibv_flow_spec_type  type;
	uint16_t  size;
	struct ibv_flow_ipv4_filter val;
	struct ibv_flow_ipv4_filter mask;
};

struct ibv_flow_tcp_udp_filter {
	uint16_t dst_port;
	uint16_t src_port;
};

struct ibv_flow_spec_tcp_udp {
	enum ibv_flow_spec_type  type;
	uint16_t  size;
	struct ibv_flow_tcp_udp_filter val;
	struct ibv_flow_tcp_udp_filter mask;
};

struct ibv_flow_spec {
	union {
		struct {
			enum ibv_flow_spec_type	type;
			uint16_t		size;
		} hdr;
		struct ibv_flow_spec_eth eth;
		struct ibv_flow_spec_ipv4 ipv4;
		struct ibv_flow_spec_tcp_udp tcp_udp;
	};
};

struct ibv_flow_attr {
	uint32_t comp_mask;
	enum ibv_flow_attr_type type;
	uint16_t size;
	uint16_t priority;
	uint8_t num_of_specs;
	uint8_t port;
	uint32_t flags;
	/* Following are the optional layers according to user request
	 * struct ibv_flow_spec_xxx [L2]
	 * struct ibv_flow_spec_yyy [L3/L4]
	 */
};

struct ibv_flow {
	uint32_t	   comp_mask;
	struct ibv_context *context;
	uint32_t	   handle;
};

struct ibv_device;
struct ibv_context;

struct ibv_device_ops {
	struct ibv_context *	(*alloc_context)(struct ibv_device *device, int cmd_fd);
	void			(*free_context)(struct ibv_context *context);
};

enum {
	IBV_SYSFS_NAME_MAX	= 64,
	IBV_SYSFS_PATH_MAX	= 256
};

struct ibv_device {
	struct ibv_device_ops	ops;
	enum ibv_node_type	node_type;
	enum ibv_transport_type	transport_type;
	/* Name of underlying kernel IB device, eg "mthca0" */
	char			name[IBV_SYSFS_NAME_MAX];
	/* Name of uverbs device, eg "uverbs0" */
	char			dev_name[IBV_SYSFS_NAME_MAX];
	/* Path to infiniband_verbs class device in sysfs */
	char			dev_path[IBV_SYSFS_PATH_MAX];
	/* Path to infiniband class device in sysfs */
	char			ibdev_path[IBV_SYSFS_PATH_MAX];
};

struct verbs_device {
	struct ibv_device device; /* Must be first */
	size_t	sz;
	size_t	size_of_context;
	int	(*init_context)(struct verbs_device *device,
				struct ibv_context *ctx, int cmd_fd);
	void	(*uninit_context)(struct verbs_device *device,
				struct ibv_context *ctx);
	/* future fields added here */
};

struct ibv_context_ops {
	int			(*query_device)(struct ibv_context *context,
					      struct ibv_device_attr *device_attr);
	int			(*query_port)(struct ibv_context *context, uint8_t port_num,
					      struct ibv_port_attr *port_attr);
	struct ibv_pd *		(*alloc_pd)(struct ibv_context *context);
	int			(*dealloc_pd)(struct ibv_pd *pd);
	struct ibv_mr *		(*reg_mr)(struct ibv_pd *pd, void *addr, size_t length,
					  int access);
	struct ibv_mr *		(*rereg_mr)(struct ibv_mr *mr,
					    int flags,
					    struct ibv_pd *pd, void *addr,
					    size_t length,
					    int access);
	int			(*dereg_mr)(struct ibv_mr *mr);
	struct ibv_mw *		(*alloc_mw)(struct ibv_pd *pd, enum ibv_mw_type type);
	int			(*bind_mw)(struct ibv_qp *qp, struct ibv_mw *mw,
					   struct ibv_mw_bind *mw_bind);
	int			(*dealloc_mw)(struct ibv_mw *mw);
	struct ibv_cq *		(*create_cq)(struct ibv_context *context, int cqe,
					     struct ibv_comp_channel *channel,
					     int comp_vector);
	int			(*poll_cq)(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc);
	int			(*req_notify_cq)(struct ibv_cq *cq, int solicited_only);
	void			(*cq_event)(struct ibv_cq *cq);
	int			(*resize_cq)(struct ibv_cq *cq, int cqe);
	int			(*destroy_cq)(struct ibv_cq *cq);
	struct ibv_srq *	(*create_srq)(struct ibv_pd *pd,
					      struct ibv_srq_init_attr *srq_init_attr);
	int			(*modify_srq)(struct ibv_srq *srq,
					      struct ibv_srq_attr *srq_attr,
					      int srq_attr_mask);
	int			(*query_srq)(struct ibv_srq *srq,
					     struct ibv_srq_attr *srq_attr);
	int			(*destroy_srq)(struct ibv_srq *srq);
	int			(*post_srq_recv)(struct ibv_srq *srq,
						 struct ibv_recv_wr *recv_wr,
						 struct ibv_recv_wr **bad_recv_wr);
	struct ibv_qp *		(*create_qp)(struct ibv_pd *pd, struct ibv_qp_init_attr *attr);
	int			(*query_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr,
					    int attr_mask,
					    struct ibv_qp_init_attr *init_attr);
	int			(*modify_qp)(struct ibv_qp *qp, struct ibv_qp_attr *attr,
					     int attr_mask);
	int			(*destroy_qp)(struct ibv_qp *qp);
	int			(*post_send)(struct ibv_qp *qp, struct ibv_send_wr *wr,
					     struct ibv_send_wr **bad_wr);
	int			(*post_recv)(struct ibv_qp *qp, struct ibv_recv_wr *wr,
					     struct ibv_recv_wr **bad_wr);
	struct ibv_ah *		(*create_ah)(struct ibv_pd *pd, struct ibv_ah_attr *attr);
	int			(*destroy_ah)(struct ibv_ah *ah);
	int			(*attach_mcast)(struct ibv_qp *qp, const union ibv_gid *gid,
						uint16_t lid);
	int			(*detach_mcast)(struct ibv_qp *qp, const union ibv_gid *gid,
						uint16_t lid);
	void			(*async_event)(struct ibv_async_event *event);
};

struct ibv_context {
	struct ibv_device      *device;
	struct ibv_context_ops	ops;
	int			cmd_fd;
	int			async_fd;
	int			num_comp_vectors;
	pthread_mutex_t		mutex;
	void		       *abi_compat;
};

enum verbs_context_mask {
	VERBS_CONTEXT_XRCD         = (uint64_t)1 << 0,
	VERBS_CONTEXT_SRQ          = (uint64_t)1 << 1,
	VERBS_CONTEXT_QP           = (uint64_t)1 << 2,
	VERBS_CONTEXT_RESERVED     = (uint64_t)1 << 3,
	VERBS_CONTEXT_EXP	   = (uint64_t)1 << 62
};

struct verbs_context {
	/*  "grows up" - new fields go here */
	int (*_reserved_2) (void);
	int (*destroy_flow) (struct ibv_flow *flow);
	int (*_reserved_1) (void);
	struct ibv_flow * (*create_flow) (struct ibv_qp *qp,
					  struct ibv_flow_attr *flow_attr);
	struct ibv_qp * (*open_qp)(struct ibv_context *context,
			struct ibv_qp_open_attr *attr);
	struct ibv_qp * (*create_qp_ex)(struct ibv_context *context,
			struct ibv_qp_init_attr_ex *qp_init_attr_ex);
	int (*get_srq_num)(struct ibv_srq *srq, uint32_t *srq_num);
	struct ibv_srq * (*create_srq_ex)(struct ibv_context *context,
			struct ibv_srq_init_attr_ex *srq_init_attr_ex);
	struct ibv_xrcd * (*open_xrcd)(struct ibv_context *context,
			struct ibv_xrcd_init_attr *xrcd_init_attr);
	int  (*close_xrcd)(struct ibv_xrcd *xrcd);
	uint64_t has_comp_mask;
	size_t   sz;	/* Must be immediately before struct ibv_context */
	struct ibv_context context;/* Must be last field in the struct */
};

static inline struct verbs_context *verbs_get_ctx(struct ibv_context *ctx)
{
	return (!ctx || (ctx->abi_compat != __VERBS_ABI_IS_EXTENDED)) ?
		NULL : container_of(ctx, struct verbs_context, context);
}

#define verbs_get_ctx_op(ctx, op) ({ \
	struct verbs_context *_vctx = verbs_get_ctx(ctx); \
	(!_vctx || (_vctx->sz < sizeof(*_vctx) - offsetof(struct verbs_context, op)) || \
	!_vctx->op) ? NULL : _vctx; })

#define verbs_set_ctx_op(_vctx, op, ptr) ({ \
	struct verbs_context *vctx = _vctx; \
	if (vctx && (vctx->sz >= sizeof(*vctx) - offsetof(struct verbs_context, op))) \
		vctx->op = ptr; })

static inline struct verbs_device *verbs_get_device(struct ibv_device *dev)
{
	return (dev->ops.alloc_context) ?
		NULL : container_of(dev, struct verbs_device, device);
}

/**
 * ibv_get_device_list - Get list of IB devices currently available
 * @num_devices: optional.  if non-NULL, set to the number of devices
 * returned in the array.
 *
 * Return a NULL-terminated array of IB devices.  The array can be
 * released with ibv_free_device_list().
 */
struct ibv_device **ibv_get_device_list(int *num_devices);

/**
 * ibv_free_device_list - Free list from ibv_get_device_list()
 *
 * Free an array of devices returned from ibv_get_device_list().  Once
 * the array is freed, pointers to devices that were not opened with
 * ibv_open_device() are no longer valid.  Client code must open all
 * devices it intends to use before calling ibv_free_device_list().
 */
void ibv_free_device_list(struct ibv_device **list);

/**
 * ibv_get_device_name - Return kernel device name
 */
const char *ibv_get_device_name(struct ibv_device *device);

/**
 * ibv_get_device_guid - Return device's node GUID
 */
uint64_t ibv_get_device_guid(struct ibv_device *device);

/**
 * ibv_open_device - Initialize device for use
 */
struct ibv_context *ibv_open_device(struct ibv_device *device);

/**
 * ibv_close_device - Release device
 */
int ibv_close_device(struct ibv_context *context);

/**
 * ibv_get_async_event - Get next async event
 * @event: Pointer to use to return async event
 *
 * All async events returned by ibv_get_async_event() must eventually
 * be acknowledged with ibv_ack_async_event().
 */
int ibv_get_async_event(struct ibv_context *context,
			struct ibv_async_event *event);

/**
 * ibv_ack_async_event - Acknowledge an async event
 * @event: Event to be acknowledged.
 *
 * All async events which are returned by ibv_get_async_event() must
 * be acknowledged.  To avoid races, destroying an object (CQ, SRQ or
 * QP) will wait for all affiliated events to be acknowledged, so
 * there should be a one-to-one correspondence between acks and
 * successful gets.
 */
void ibv_ack_async_event(struct ibv_async_event *event);

/**
 * ibv_query_device - Get device properties
 */
int ibv_query_device(struct ibv_context *context,
		     struct ibv_device_attr *device_attr);

/**
 * ibv_query_port - Get port properties
 */
int ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct ibv_port_attr *port_attr);

static inline int ___ibv_query_port(struct ibv_context *context,
				    uint8_t port_num,
				    struct ibv_port_attr *port_attr)
{
	/* For compatibility when running with old libibverbs */
	port_attr->link_layer = IBV_LINK_LAYER_UNSPECIFIED;
	port_attr->reserved   = 0;

	return ibv_query_port(context, port_num, port_attr);
}

#define ibv_query_port(context, port_num, port_attr) \
	___ibv_query_port(context, port_num, port_attr)

/**
 * ibv_query_gid - Get a GID table entry
 */
int ibv_query_gid(struct ibv_context *context, uint8_t port_num,
		  int index, union ibv_gid *gid);

/**
 * ibv_query_pkey - Get a P_Key table entry
 */
int ibv_query_pkey(struct ibv_context *context, uint8_t port_num,
		   int index, uint16_t *pkey);

/**
 * ibv_alloc_pd - Allocate a protection domain
 */
struct ibv_pd *ibv_alloc_pd(struct ibv_context *context);

/**
 * ibv_dealloc_pd - Free a protection domain
 */
int ibv_dealloc_pd(struct ibv_pd *pd);

static inline struct ibv_flow *ibv_create_flow(struct ibv_qp *qp,
					       struct ibv_flow_attr *flow)
{
	struct verbs_context *vctx = verbs_get_ctx_op(qp->context,
						      create_flow);
	if (!vctx)
		return NULL;

	return vctx->create_flow(qp, flow);
}

static inline int ibv_destroy_flow(struct ibv_flow *flow_id)
{
	struct verbs_context *vctx = verbs_get_ctx_op(flow_id->context,
						      destroy_flow);
	if (!vctx)
		return -ENOSYS;
	return vctx->destroy_flow(flow_id);
}

/**
 * ibv_open_xrcd - Open an extended connection domain
 */
static inline struct ibv_xrcd *
ibv_open_xrcd(struct ibv_context *context, struct ibv_xrcd_init_attr *xrcd_init_attr)
{
	struct verbs_context *vctx = verbs_get_ctx_op(context, open_xrcd);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	return vctx->open_xrcd(context, xrcd_init_attr);
}

/**
 * ibv_close_xrcd - Close an extended connection domain
 */
static inline int ibv_close_xrcd(struct ibv_xrcd *xrcd)
{
	struct verbs_context *vctx = verbs_get_ctx(xrcd->context);
	return vctx->close_xrcd(xrcd);
}

/**
 * ibv_reg_mr - Register a memory region
 */
struct ibv_mr *ibv_reg_mr(struct ibv_pd *pd, void *addr,
			  size_t length, int access);

/**
 * ibv_dereg_mr - Deregister a memory region
 */
int ibv_dereg_mr(struct ibv_mr *mr);

/**
 * ibv_alloc_mw - Allocate a memory window
 */
static inline struct ibv_mw *ibv_alloc_mw(struct ibv_pd *pd,
		enum ibv_mw_type type)
{
	if (!pd->context->ops.alloc_mw) {
		errno = ENOSYS;
		return NULL;
	}

	struct ibv_mw *mw = pd->context->ops.alloc_mw(pd, type);
	if (mw) {
		mw->context = pd->context;
		mw->pd      = pd;
	}
	return mw;
}

/**
 * ibv_dealloc_mw - Free a memory window
 */
static inline int ibv_dealloc_mw(struct ibv_mw *mw)
{
	return mw->context->ops.dealloc_mw(mw);
}

/**
 * ibv_inc_rkey - increase the 8 lsb in the given rkey
 */
static inline uint32_t ibv_inc_rkey(uint32_t rkey)
{
	const uint32_t mask = 0x000000ff;
	uint8_t newtag = (uint8_t) ((rkey + 1) & mask);
	return (rkey & ~mask) | newtag;
}

/**
 * ibv_create_comp_channel - Create a completion event channel
 */
struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context);

/**
 * ibv_destroy_comp_channel - Destroy a completion event channel
 */
int ibv_destroy_comp_channel(struct ibv_comp_channel *channel);

/**
 * ibv_create_cq - Create a completion queue
 * @context - Context CQ will be attached to
 * @cqe - Minimum number of entries required for CQ
 * @cq_context - Consumer-supplied context returned for completion events
 * @channel - Completion channel where completion events will be queued.
 *     May be NULL if completion events will not be used.
 * @comp_vector - Completion vector used to signal completion events.
 *     Must be >= 0 and < context->num_comp_vectors.
 */
struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,
			     void *cq_context,
			     struct ibv_comp_channel *channel,
			     int comp_vector);

/**
 * ibv_resize_cq - Modifies the capacity of the CQ.
 * @cq: The CQ to resize.
 * @cqe: The minimum size of the CQ.
 *
 * Users can examine the cq structure to determine the actual CQ size.
 */
int ibv_resize_cq(struct ibv_cq *cq, int cqe);

/**
 * ibv_destroy_cq - Destroy a completion queue
 */
int ibv_destroy_cq(struct ibv_cq *cq);

/**
 * ibv_get_cq_event - Read next CQ event
 * @channel: Channel to get next event from.
 * @cq: Used to return pointer to CQ.
 * @cq_context: Used to return consumer-supplied CQ context.
 *
 * All completion events returned by ibv_get_cq_event() must
 * eventually be acknowledged with ibv_ack_cq_events().
 */
int ibv_get_cq_event(struct ibv_comp_channel *channel,
		     struct ibv_cq **cq, void **cq_context);

/**
 * ibv_ack_cq_events - Acknowledge CQ completion events
 * @cq: CQ to acknowledge events for
 * @nevents: Number of events to acknowledge.
 *
 * All completion events which are returned by ibv_get_cq_event() must
 * be acknowledged.  To avoid races, ibv_destroy_cq() will wait for
 * all completion events to be acknowledged, so there should be a
 * one-to-one correspondence between acks and successful gets.  An
 * application may accumulate multiple completion events and
 * acknowledge them in a single call to ibv_ack_cq_events() by passing
 * the number of events to ack in @nevents.
 */
void ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents);

/**
 * ibv_poll_cq - Poll a CQ for work completions
 * @cq:the CQ being polled
 * @num_entries:maximum number of completions to return
 * @wc:array of at least @num_entries of &struct ibv_wc where completions
 *   will be returned
 *
 * Poll a CQ for (possibly multiple) completions.  If the return value
 * is < 0, an error occurred.  If the return value is >= 0, it is the
 * number of completions returned.  If the return value is
 * non-negative and strictly less than num_entries, then the CQ was
 * emptied.
 */
static inline int ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
	return cq->context->ops.poll_cq(cq, num_entries, wc);
}

/**
 * ibv_req_notify_cq - Request completion notification on a CQ.  An
 *   event will be added to the completion channel associated with the
 *   CQ when an entry is added to the CQ.
 * @cq: The completion queue to request notification for.
 * @solicited_only: If non-zero, an event will be generated only for
 *   the next solicited CQ entry.  If zero, any CQ entry, solicited or
 *   not, will generate an event.
 */
static inline int ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only)
{
	return cq->context->ops.req_notify_cq(cq, solicited_only);
}

/**
 * ibv_create_srq - Creates a SRQ associated with the specified protection
 *   domain.
 * @pd: The protection domain associated with the SRQ.
 * @srq_init_attr: A list of initial attributes required to create the SRQ.
 *
 * srq_attr->max_wr and srq_attr->max_sge are read the determine the
 * requested size of the SRQ, and set to the actual values allocated
 * on return.  If ibv_create_srq() succeeds, then max_wr and max_sge
 * will always be at least as large as the requested values.
 */
struct ibv_srq *ibv_create_srq(struct ibv_pd *pd,
			       struct ibv_srq_init_attr *srq_init_attr);

static inline struct ibv_srq *
ibv_create_srq_ex(struct ibv_context *context,
		  struct ibv_srq_init_attr_ex *srq_init_attr_ex)
{
	struct verbs_context *vctx;
	uint32_t mask = srq_init_attr_ex->comp_mask;

	if (!(mask & ~(IBV_SRQ_INIT_ATTR_PD | IBV_SRQ_INIT_ATTR_TYPE)) &&
	    (mask & IBV_SRQ_INIT_ATTR_PD) &&
	    (!(mask & IBV_SRQ_INIT_ATTR_TYPE) ||
	     (srq_init_attr_ex->srq_type == IBV_SRQT_BASIC)))
		return ibv_create_srq(srq_init_attr_ex->pd,
				      (struct ibv_srq_init_attr *) srq_init_attr_ex);

	vctx = verbs_get_ctx_op(context, create_srq_ex);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	return vctx->create_srq_ex(context, srq_init_attr_ex);
}

/**
 * ibv_modify_srq - Modifies the attributes for the specified SRQ.
 * @srq: The SRQ to modify.
 * @srq_attr: On input, specifies the SRQ attributes to modify.  On output,
 *   the current values of selected SRQ attributes are returned.
 * @srq_attr_mask: A bit-mask used to specify which attributes of the SRQ
 *   are being modified.
 *
 * The mask may contain IBV_SRQ_MAX_WR to resize the SRQ and/or
 * IBV_SRQ_LIMIT to set the SRQ's limit and request notification when
 * the number of receives queued drops below the limit.
 */
int ibv_modify_srq(struct ibv_srq *srq,
		   struct ibv_srq_attr *srq_attr,
		   int srq_attr_mask);

/**
 * ibv_query_srq - Returns the attribute list and current values for the
 *   specified SRQ.
 * @srq: The SRQ to query.
 * @srq_attr: The attributes of the specified SRQ.
 */
int ibv_query_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr);

static inline int ibv_get_srq_num(struct ibv_srq *srq, uint32_t *srq_num)
{
	struct verbs_context *vctx = verbs_get_ctx_op(srq->context, get_srq_num);

	if (!vctx)
		return ENOSYS;

	return vctx->get_srq_num(srq, srq_num);
}

/**
 * ibv_destroy_srq - Destroys the specified SRQ.
 * @srq: The SRQ to destroy.
 */
int ibv_destroy_srq(struct ibv_srq *srq);

/**
 * ibv_post_srq_recv - Posts a list of work requests to the specified SRQ.
 * @srq: The SRQ to post the work request on.
 * @recv_wr: A list of work requests to post on the receive queue.
 * @bad_recv_wr: On an immediate failure, this parameter will reference
 *   the work request that failed to be posted on the QP.
 */
static inline int ibv_post_srq_recv(struct ibv_srq *srq,
				    struct ibv_recv_wr *recv_wr,
				    struct ibv_recv_wr **bad_recv_wr)
{
	return srq->context->ops.post_srq_recv(srq, recv_wr, bad_recv_wr);
}

/**
 * ibv_create_qp - Create a queue pair.
 */
struct ibv_qp *ibv_create_qp(struct ibv_pd *pd,
			     struct ibv_qp_init_attr *qp_init_attr);

static inline struct ibv_qp *
ibv_create_qp_ex(struct ibv_context *context, struct ibv_qp_init_attr_ex *qp_init_attr_ex)
{
	struct verbs_context *vctx;
	uint32_t mask = qp_init_attr_ex->comp_mask;

	if (mask == IBV_QP_INIT_ATTR_PD)
		return ibv_create_qp(qp_init_attr_ex->pd,
				     (struct ibv_qp_init_attr *) qp_init_attr_ex);

	vctx = verbs_get_ctx_op(context, create_qp_ex);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	return vctx->create_qp_ex(context, qp_init_attr_ex);
}

/**
 * ibv_open_qp - Open a shareable queue pair.
 */
static inline struct ibv_qp *
ibv_open_qp(struct ibv_context *context, struct ibv_qp_open_attr *qp_open_attr)
{
	struct verbs_context *vctx = verbs_get_ctx_op(context, open_qp);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	return vctx->open_qp(context, qp_open_attr);
}

/**
 * ibv_modify_qp - Modify a queue pair.
 */
int ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		  int attr_mask);

/**
 * ibv_query_qp - Returns the attribute list and current values for the
 *   specified QP.
 * @qp: The QP to query.
 * @attr: The attributes of the specified QP.
 * @attr_mask: A bit-mask used to select specific attributes to query.
 * @init_attr: Additional attributes of the selected QP.
 *
 * The qp_attr_mask may be used to limit the query to gathering only the
 * selected attributes.
 */
int ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		 int attr_mask,
		 struct ibv_qp_init_attr *init_attr);

/**
 * ibv_destroy_qp - Destroy a queue pair.
 */
int ibv_destroy_qp(struct ibv_qp *qp);

/**
 * ibv_post_send - Post a list of work requests to a send queue.
 *
 * If IBV_SEND_INLINE flag is set, the data buffers can be reused
 * immediately after the call returns.
 */
static inline int ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr,
				struct ibv_send_wr **bad_wr)
{
	return qp->context->ops.post_send(qp, wr, bad_wr);
}

/**
 * ibv_post_recv - Post a list of work requests to a receive queue.
 */
static inline int ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr,
				struct ibv_recv_wr **bad_wr)
{
	return qp->context->ops.post_recv(qp, wr, bad_wr);
}

/**
 * ibv_create_ah - Create an address handle.
 */
struct ibv_ah *ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);

/**
 * ibv_init_ah_from_wc - Initializes address handle attributes from a
 *   work completion.
 * @context: Device context on which the received message arrived.
 * @port_num: Port on which the received message arrived.
 * @wc: Work completion associated with the received message.
 * @grh: References the received global route header.  This parameter is
 *   ignored unless the work completion indicates that the GRH is valid.
 * @ah_attr: Returned attributes that can be used when creating an address
 *   handle for replying to the message.
 */
int ibv_init_ah_from_wc(struct ibv_context *context, uint8_t port_num,
			struct ibv_wc *wc, struct ibv_grh *grh,
			struct ibv_ah_attr *ah_attr);

/**
 * ibv_create_ah_from_wc - Creates an address handle associated with the
 *   sender of the specified work completion.
 * @pd: The protection domain associated with the address handle.
 * @wc: Work completion information associated with a received message.
 * @grh: References the received global route header.  This parameter is
 *   ignored unless the work completion indicates that the GRH is valid.
 * @port_num: The outbound port number to associate with the address.
 *
 * The address handle is used to reference a local or global destination
 * in all UD QP post sends.
 */
struct ibv_ah *ibv_create_ah_from_wc(struct ibv_pd *pd, struct ibv_wc *wc,
				     struct ibv_grh *grh, uint8_t port_num);

/**
 * ibv_destroy_ah - Destroy an address handle.
 */
int ibv_destroy_ah(struct ibv_ah *ah);

/**
 * ibv_attach_mcast - Attaches the specified QP to a multicast group.
 * @qp: QP to attach to the multicast group.  The QP must be a UD QP.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 *
 * In order to route multicast packets correctly, subnet
 * administration must have created the multicast group and configured
 * the fabric appropriately.  The port associated with the specified
 * QP must also be a member of the multicast group.
 */
int ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);

/**
 * ibv_detach_mcast - Detaches the specified QP from a multicast group.
 * @qp: QP to detach from the multicast group.
 * @gid: Multicast group GID.
 * @lid: Multicast group LID in host byte order.
 */
int ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);

/**
 * ibv_fork_init - Prepare data structures so that fork() may be used
 * safely.  If this function is not called or returns a non-zero
 * status, then libibverbs data structures are not fork()-safe and the
 * effect of an application calling fork() is undefined.
 */
int ibv_fork_init(void);

/**
 * ibv_node_type_str - Return string describing node_type enum value
 */
const char *ibv_node_type_str(enum ibv_node_type node_type);

/**
 * ibv_port_state_str - Return string describing port_state enum value
 */
const char *ibv_port_state_str(enum ibv_port_state port_state);

/**
 * ibv_event_type_str - Return string describing event_type enum value
 */
const char *ibv_event_type_str(enum ibv_event_type event);

END_C_DECLS

#  undef __attribute_const

#include <infiniband/verbs_exp.h>

#endif /* INFINIBAND_VERBS_H */
