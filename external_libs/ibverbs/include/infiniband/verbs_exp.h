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

#ifndef INFINIBAND_VERBS_EXP_H
#define INFINIBAND_VERBS_EXP_H

#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>

#if __GNUC__ >= 3
#  define __attribute_const __attribute__((const))
#else
#  define __attribute_const
#endif

BEGIN_C_DECLS

#define IBV_EXP_RET_ON_INVALID_COMP_MASK(val, valid_mask, ret)						\
	if ((val) > (valid_mask)) {									\
		fprintf(stderr, "%s: invalid comp_mask !!! (comp_mask = 0x%x valid_mask = 0x%x)\n",	\
			__FUNCTION__, val, valid_mask);							\
		errno = EINVAL;										\
		return ret;										\
	}

#define IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(val, valid_mask)	\
		IBV_EXP_RET_ON_INVALID_COMP_MASK(val, valid_mask, NULL)

#define IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(val, valid_mask)	\
		IBV_EXP_RET_ON_INVALID_COMP_MASK(val, valid_mask, EINVAL)


#define IBV_EXP_IMPLICIT_MR_SIZE (~((size_t)0))

enum ibv_exp_func_name {
	IBV_EXP_POST_SEND_FUNC,
	IBV_EXP_POLL_CQ_FUNC,
	IBV_POST_SEND_FUNC,
	IBV_POLL_CQ_FUNC,
	IBV_POST_RECV_FUNC
};

enum ibv_exp_start_values {
	IBV_EXP_START_ENUM	= 0x40,
	IBV_EXP_START_FLAG_LOC	= 0x20,
	IBV_EXP_START_FLAG	= (1ULL << IBV_EXP_START_FLAG_LOC),
};

/*
 * Capabilities for exp_atomic_cap field in ibv_exp_device_attr struct
 */
enum ibv_exp_atomic_cap {
	IBV_EXP_ATOMIC_NONE		= IBV_ATOMIC_NONE,
	IBV_EXP_ATOMIC_HCA		= IBV_ATOMIC_HCA,
	IBV_EXP_ATOMIC_GLOB		= IBV_ATOMIC_GLOB,

	IBV_EXP_ATOMIC_HCA_REPLY_BE	= IBV_EXP_START_ENUM /* HOST is LE and atomic reply is BE */
};

/*
 * Flags for exp_device_cap_flags field in ibv_exp_device_attr struct
 */
enum ibv_exp_device_cap_flags {
	IBV_EXP_DEVICE_RESIZE_MAX_WR		= IBV_DEVICE_RESIZE_MAX_WR,
	IBV_EXP_DEVICE_BAD_PKEY_CNTR		= IBV_DEVICE_BAD_PKEY_CNTR,
	IBV_EXP_DEVICE_BAD_QKEY_CNTR		= IBV_DEVICE_BAD_QKEY_CNTR,
	IBV_EXP_DEVICE_RAW_MULTI		= IBV_DEVICE_RAW_MULTI,
	IBV_EXP_DEVICE_AUTO_PATH_MIG		= IBV_DEVICE_AUTO_PATH_MIG,
	IBV_EXP_DEVICE_CHANGE_PHY_PORT		= IBV_DEVICE_CHANGE_PHY_PORT,
	IBV_EXP_DEVICE_UD_AV_PORT_ENFORCE	= IBV_DEVICE_UD_AV_PORT_ENFORCE,
	IBV_EXP_DEVICE_CURR_QP_STATE_MOD	= IBV_DEVICE_CURR_QP_STATE_MOD,
	IBV_EXP_DEVICE_SHUTDOWN_PORT		= IBV_DEVICE_SHUTDOWN_PORT,
	IBV_EXP_DEVICE_INIT_TYPE		= IBV_DEVICE_INIT_TYPE,
	IBV_EXP_DEVICE_PORT_ACTIVE_EVENT	= IBV_DEVICE_PORT_ACTIVE_EVENT,
	IBV_EXP_DEVICE_SYS_IMAGE_GUID		= IBV_DEVICE_SYS_IMAGE_GUID,
	IBV_EXP_DEVICE_RC_RNR_NAK_GEN		= IBV_DEVICE_RC_RNR_NAK_GEN,
	IBV_EXP_DEVICE_SRQ_RESIZE		= IBV_DEVICE_SRQ_RESIZE,
	IBV_EXP_DEVICE_N_NOTIFY_CQ		= IBV_DEVICE_N_NOTIFY_CQ,
	IBV_EXP_DEVICE_XRC			= IBV_DEVICE_XRC,

	IBV_EXP_DEVICE_DC_TRANSPORT		= (IBV_EXP_START_FLAG << 0),
	IBV_EXP_DEVICE_QPG			= (IBV_EXP_START_FLAG << 1),
	IBV_EXP_DEVICE_UD_RSS			= (IBV_EXP_START_FLAG << 2),
	IBV_EXP_DEVICE_UD_TSS			= (IBV_EXP_START_FLAG << 3),
	IBV_EXP_DEVICE_EXT_ATOMICS		= (IBV_EXP_START_FLAG << 4),
	IBV_EXP_DEVICE_NOP			= (IBV_EXP_START_FLAG << 5),
	IBV_EXP_DEVICE_UMR			= (IBV_EXP_START_FLAG << 6),
	IBV_EXP_DEVICE_ODP			= (IBV_EXP_START_FLAG << 7),
	IBV_EXP_DEVICE_VXLAN_SUPPORT		= (IBV_EXP_START_FLAG << 10),
	IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT	= (IBV_EXP_START_FLAG << 11),
	IBV_EXP_DEVICE_RX_CSUM_IP_PKT		= (IBV_EXP_START_FLAG << 12),
	IBV_EXP_DEVICE_EC_OFFLOAD		= (IBV_EXP_START_FLAG << 13),
	IBV_EXP_DEVICE_EXT_MASKED_ATOMICS	= (IBV_EXP_START_FLAG << 14),
	IBV_EXP_DEVICE_RX_TCP_UDP_PKT_TYPE	= (IBV_EXP_START_FLAG << 15),
	IBV_EXP_DEVICE_SCATTER_FCS		= (IBV_EXP_START_FLAG << 16),
	IBV_EXP_DEVICE_MEM_WINDOW		= (IBV_EXP_START_FLAG << 17),
	IBV_EXP_DEVICE_MEM_MGT_EXTENSIONS	= (IBV_EXP_START_FLAG << 21),
	IBV_EXP_DEVICE_DC_INFO			= (IBV_EXP_START_FLAG << 22),
	/* Jumping to 23 as of next capability in include/rdma/ib_verbs.h */
	IBV_EXP_DEVICE_MW_TYPE_2A		= (IBV_EXP_START_FLAG << 23),
	IBV_EXP_DEVICE_MW_TYPE_2B		= (IBV_EXP_START_FLAG << 24),
	IBV_EXP_DEVICE_CROSS_CHANNEL		= (IBV_EXP_START_FLAG << 28),
	IBV_EXP_DEVICE_MANAGED_FLOW_STEERING	= (IBV_EXP_START_FLAG << 29),
	IBV_EXP_DEVICE_MR_ALLOCATE		= (IBV_EXP_START_FLAG << 30),
	IBV_EXP_DEVICE_SHARED_MR		= (IBV_EXP_START_FLAG << 31),
};

/*
 * Flags for ibv_exp_device_attr struct comp_mask.
 */
enum ibv_exp_device_attr_comp_mask {
	IBV_EXP_DEVICE_ATTR_CALC_CAP		= (1 << 0),
	IBV_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK	= (1 << 1),
	IBV_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK	= (1 << 2),
	IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS	= (1 << 3),
	IBV_EXP_DEVICE_DC_RD_REQ		= (1 << 4),
	IBV_EXP_DEVICE_DC_RD_RES		= (1 << 5),
	IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ	= (1 << 6),
	IBV_EXP_DEVICE_ATTR_RSS_TBL_SZ		= (1 << 7),
	IBV_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS	= (1 << 8),
	IBV_EXP_DEVICE_ATTR_UMR                 = (1 << 9),
	IBV_EXP_DEVICE_ATTR_ODP			= (1 << 10),
	IBV_EXP_DEVICE_ATTR_MAX_DCT		= (1 << 11),
	IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN	= (1 << 12),
	IBV_EXP_DEVICE_ATTR_RX_HASH		= (1 << 13),
	IBV_EXP_DEVICE_ATTR_MAX_WQ_TYPE_RQ	= (1 << 14),
	IBV_EXP_DEVICE_ATTR_MAX_DEVICE_CTX	= (1 << 15),
	IBV_EXP_DEVICE_ATTR_MP_RQ		= (1 << 16),
	IBV_EXP_DEVICE_ATTR_VLAN_OFFLOADS	= (1 << 17),
	IBV_EXP_DEVICE_ATTR_EC_CAPS		= (1 << 18),
	IBV_EXP_DEVICE_ATTR_MASKED_ATOMICS	= (1 << 19),
	IBV_EXP_DEVICE_ATTR_RX_PAD_END_ALIGN	= (1 << 20),
	IBV_EXP_DEVICE_ATTR_TSO_CAPS		= (1 << 21),
	IBV_EXP_DEVICE_ATTR_PACKET_PACING_CAPS	= (1 << 22),
	/* set supported bits for validity check */
	IBV_EXP_DEVICE_ATTR_RESERVED		= (1 << 23),
};

struct ibv_exp_device_calc_cap {
	uint64_t		data_types;
	uint64_t		data_sizes;
	uint64_t		int_ops;
	uint64_t		uint_ops;
	uint64_t		fp_ops;
};

struct ibv_exp_ext_atomics_params {
	/* defines which masked operation sizes are supported with same
	 * endianness as stated in atomic_cap field
	 */
	uint64_t		log_atomic_arg_sizes; /* bit-mask of supported sizes */
	uint32_t		max_fa_bit_boundary;
	uint32_t		log_max_atomic_inline;
};

struct ibv_exp_masked_atomic_params {
	uint32_t	max_fa_bit_boundary;
	uint32_t	log_max_atomic_inline;
	uint64_t	masked_log_atomic_arg_sizes;
	uint64_t	masked_log_atomic_arg_sizes_network_endianness;
};

enum ibv_odp_general_cap_bits {
	IBV_EXP_ODP_SUPPORT = 1 << 0,
};

enum ibv_odp_transport_cap_bits {
	IBV_EXP_ODP_SUPPORT_SEND     = 1 << 0,
	IBV_EXP_ODP_SUPPORT_RECV     = 1 << 1,
	IBV_EXP_ODP_SUPPORT_WRITE    = 1 << 2,
	IBV_EXP_ODP_SUPPORT_READ     = 1 << 3,
	IBV_EXP_ODP_SUPPORT_ATOMIC   = 1 << 4,
	IBV_EXP_ODP_SUPPORT_SRQ_RECV = 1 << 5,
};

struct ibv_exp_umr_caps {
	uint32_t                max_klm_list_size;
	uint32_t                max_send_wqe_inline_klms;
	uint32_t                max_umr_recursion_depth;
	uint32_t                max_umr_stride_dimension;
};

struct ibv_exp_odp_caps {
	uint64_t	general_odp_caps;
	struct {
		uint32_t	rc_odp_caps;
		uint32_t	uc_odp_caps;
		uint32_t	ud_odp_caps;
		uint32_t	dc_odp_caps;
		uint32_t	xrc_odp_caps;
		uint32_t	raw_eth_odp_caps;
	} per_transport_caps;
};

enum ibv_exp_supported_qp_types {
	IBV_EXP_QPT_RC		= 1ULL << 0,
	IBV_EXP_QPT_UC		= 1ULL << 1,
	IBV_EXP_QPT_UD		= 1ULL << 2,
	IBV_EXP_QPT_XRC_INIT	= 1ULL << 3,
	IBV_EXP_QPT_XRC_TGT	= 1ULL << 4,
	IBV_EXP_QPT_RAW_PACKET	= 1ULL << 5,
	IBV_EXP_QPT_RESERVED	= 1ULL << 6
};

struct ibv_exp_rx_hash_caps {
	uint32_t max_rwq_indirection_tables;
	uint32_t max_rwq_indirection_table_size;
	uint8_t  supported_hash_functions; /* from ibv_exp_rx_hash_function_flags */
	uint64_t supported_packet_fields;  /* from ibv_exp_rx_hash_fields */
	uint32_t supported_qps;		   /* from ibv_exp_supported_qp_types */
};

enum ibv_exp_mp_rq_shifts {
	IBV_EXP_MP_RQ_NO_SHIFT		= 0,
	IBV_EXP_MP_RQ_2BYTES_SHIFT	= 1 << 0
};

struct ibv_exp_mp_rq_caps {
	uint32_t supported_qps; /* use ibv_exp_supported_qp_types */
	uint32_t allowed_shifts; /* use ibv_exp_mp_rq_shifts */
	uint8_t min_single_wqe_log_num_of_strides;
	uint8_t max_single_wqe_log_num_of_strides;
	uint8_t min_single_stride_log_num_of_bytes;
	uint8_t max_single_stride_log_num_of_bytes;
};

struct ibv_exp_ec_caps {
	uint32_t	max_ec_data_vector_count;
	uint32_t	max_ec_calc_inflight_calcs;
};

#define ibv_is_qpt_supported(caps, qpt) ((caps) & (1 << (qpt)))

struct ibv_exp_tso_caps {
	uint32_t max_tso;
	uint32_t supported_qpts;
};

struct ibv_exp_packet_pacing_caps {
	uint32_t qp_rate_limit_min;
	uint32_t qp_rate_limit_max; /* In kbps */
	uint32_t supported_qpts;
	uint32_t reserved;
};

struct ibv_exp_device_attr {
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
	int			reserved; /* place holder to align with ibv_device_attr */
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
	enum ibv_exp_atomic_cap	exp_atomic_cap;
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
	uint32_t                comp_mask;
	struct ibv_exp_device_calc_cap calc_cap;
	uint64_t		timestamp_mask;
	uint64_t		hca_core_clock;
	uint64_t		exp_device_cap_flags; /* use ibv_exp_device_cap_flags */
	int			max_dc_req_rd_atom;
	int			max_dc_res_rd_atom;
	int			inline_recv_sz;
	uint32_t		max_rss_tbl_sz;
	struct ibv_exp_ext_atomics_params ext_atom;
	struct ibv_exp_umr_caps umr_caps;
	struct ibv_exp_odp_caps	odp_caps;
	int			max_dct;
	int			max_ctx_res_domain;
	struct ibv_exp_rx_hash_caps	rx_hash_caps;
	uint32_t			max_wq_type_rq;
	int 				max_device_ctx;
	struct ibv_exp_mp_rq_caps	mp_rq_caps;
	uint16_t		wq_vlan_offloads_cap; /* use ibv_exp_vlan_offloads enum */
	struct ibv_exp_ec_caps         ec_caps;
	struct ibv_exp_masked_atomic_params masked_atomic;
	/*
	 * The alignment of the padding end address.
	 * When RX end of packet padding is enabled the device will pad the end
	 * of RX packet up until the next address which is aligned to the
	 * rx_pad_end_addr_align size.
	 * Expected size for this field is according to system cache line size,
	 * for example 64 or 128. When field is 0 padding is not supported.
	 */
	int			rx_pad_end_addr_align;
	struct ibv_exp_tso_caps	tso_caps;
	struct ibv_exp_packet_pacing_caps packet_pacing_caps;
};

enum ibv_exp_access_flags {
	IBV_EXP_ACCESS_LOCAL_WRITE	= IBV_ACCESS_LOCAL_WRITE,
	IBV_EXP_ACCESS_REMOTE_WRITE	= IBV_ACCESS_REMOTE_WRITE,
	IBV_EXP_ACCESS_REMOTE_READ	= IBV_ACCESS_REMOTE_READ,
	IBV_EXP_ACCESS_REMOTE_ATOMIC	= IBV_ACCESS_REMOTE_ATOMIC,
	IBV_EXP_ACCESS_MW_BIND		= IBV_ACCESS_MW_BIND,

	IBV_EXP_ACCESS_ALLOCATE_MR		= (IBV_EXP_START_FLAG << 5),
	IBV_EXP_ACCESS_SHARED_MR_USER_READ	= (IBV_EXP_START_FLAG << 6),
	IBV_EXP_ACCESS_SHARED_MR_USER_WRITE	= (IBV_EXP_START_FLAG << 7),
	IBV_EXP_ACCESS_SHARED_MR_GROUP_READ	= (IBV_EXP_START_FLAG << 8),
	IBV_EXP_ACCESS_SHARED_MR_GROUP_WRITE	= (IBV_EXP_START_FLAG << 9),
	IBV_EXP_ACCESS_SHARED_MR_OTHER_READ	= (IBV_EXP_START_FLAG << 10),
	IBV_EXP_ACCESS_SHARED_MR_OTHER_WRITE	= (IBV_EXP_START_FLAG << 11),
	IBV_EXP_ACCESS_NO_RDMA			= (IBV_EXP_START_FLAG << 12),
	IBV_EXP_ACCESS_MW_ZERO_BASED		= (IBV_EXP_START_FLAG << 13),
	IBV_EXP_ACCESS_ON_DEMAND		= (IBV_EXP_START_FLAG << 14),
	IBV_EXP_ACCESS_RELAXED			= (IBV_EXP_START_FLAG << 15),
	IBV_EXP_ACCESS_PHYSICAL_ADDR		= (IBV_EXP_START_FLAG << 16),
	/* set supported bits for validity check */
	IBV_EXP_ACCESS_RESERVED			= (IBV_EXP_START_FLAG << 17)
};

/* memory window information struct that is common to types 1 and 2 */
struct ibv_exp_mw_bind_info {
	struct ibv_mr	*mr;
	uint64_t	 addr;
	uint64_t	 length;
	uint64_t	 exp_mw_access_flags; /* use ibv_exp_access_flags */
};

/*
 * Flags for ibv_exp_mw_bind struct comp_mask
 */
enum ibv_exp_bind_mw_comp_mask {
	IBV_EXP_BIND_MW_RESERVED	= (1 << 0)
};

/* type 1 specific info */
struct ibv_exp_mw_bind {
	struct ibv_qp			*qp;
	struct ibv_mw			*mw;
	uint64_t			 wr_id;
	uint64_t			 exp_send_flags; /* use ibv_exp_send_flags */
	struct ibv_exp_mw_bind_info	 bind_info;
	uint32_t			 comp_mask; /* reserved for future growth (must be 0) */
};

enum ibv_exp_calc_op {
	IBV_EXP_CALC_OP_ADD	= 0,
	IBV_EXP_CALC_OP_MAXLOC,
	IBV_EXP_CALC_OP_BAND,
	IBV_EXP_CALC_OP_BXOR,
	IBV_EXP_CALC_OP_BOR,
	IBV_EXP_CALC_OP_NUMBER
};

enum ibv_exp_calc_data_type {
	IBV_EXP_CALC_DATA_TYPE_INT	= 0,
	IBV_EXP_CALC_DATA_TYPE_UINT,
	IBV_EXP_CALC_DATA_TYPE_FLOAT,
	IBV_EXP_CALC_DATA_TYPE_NUMBER
};

enum ibv_exp_calc_data_size {
	IBV_EXP_CALC_DATA_SIZE_64_BIT	= 0,
	IBV_EXP_CALC_DATA_SIZE_NUMBER
};

enum ibv_exp_wr_opcode {
	IBV_EXP_WR_RDMA_WRITE		= IBV_WR_RDMA_WRITE,
	IBV_EXP_WR_RDMA_WRITE_WITH_IMM	= IBV_WR_RDMA_WRITE_WITH_IMM,
	IBV_EXP_WR_SEND			= IBV_WR_SEND,
	IBV_EXP_WR_SEND_WITH_IMM	= IBV_WR_SEND_WITH_IMM,
	IBV_EXP_WR_RDMA_READ		= IBV_WR_RDMA_READ,
	IBV_EXP_WR_ATOMIC_CMP_AND_SWP	= IBV_WR_ATOMIC_CMP_AND_SWP,
	IBV_EXP_WR_ATOMIC_FETCH_AND_ADD	= IBV_WR_ATOMIC_FETCH_AND_ADD,

	IBV_EXP_WR_SEND_WITH_INV	= 8 + IBV_EXP_START_ENUM,
	IBV_EXP_WR_LOCAL_INV		= 10 + IBV_EXP_START_ENUM,
	IBV_EXP_WR_BIND_MW		= 14 + IBV_EXP_START_ENUM,
	IBV_EXP_WR_TSO			= 15 + IBV_EXP_START_ENUM,
	IBV_EXP_WR_SEND_ENABLE		= 0x20 + IBV_EXP_START_ENUM,
	IBV_EXP_WR_RECV_ENABLE,
	IBV_EXP_WR_CQE_WAIT,
	IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP,
	IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD,
	IBV_EXP_WR_NOP,
	IBV_EXP_WR_UMR_FILL,
	IBV_EXP_WR_UMR_INVALIDATE,
};

enum ibv_exp_send_flags {
	IBV_EXP_SEND_FENCE		= IBV_SEND_FENCE,
	IBV_EXP_SEND_SIGNALED		= IBV_SEND_SIGNALED,
	IBV_EXP_SEND_SOLICITED		= IBV_SEND_SOLICITED,
	IBV_EXP_SEND_INLINE		= IBV_SEND_INLINE,

	IBV_EXP_SEND_IP_CSUM		= (IBV_EXP_START_FLAG << 0),
	IBV_EXP_SEND_WITH_CALC		= (IBV_EXP_START_FLAG << 1),
	IBV_EXP_SEND_WAIT_EN_LAST	= (IBV_EXP_START_FLAG << 2),
	IBV_EXP_SEND_EXT_ATOMIC_INLINE	= (IBV_EXP_START_FLAG << 3),
};

struct ibv_exp_cmp_swap {
	uint64_t        compare_mask;
	uint64_t        compare_val;
	uint64_t        swap_val;
	uint64_t        swap_mask;
};

struct ibv_exp_fetch_add {
	uint64_t        add_val;
	uint64_t        field_boundary;
};

/*
 * Flags for ibv_exp_send_wr struct comp_mask
 */
enum ibv_exp_send_wr_comp_mask {
	IBV_EXP_SEND_WR_ATTR_RESERVED	= 1 << 0
};

struct ibv_exp_mem_region {
	uint64_t base_addr;
	struct ibv_mr *mr;
	size_t length;
};

struct ibv_exp_mem_repeat_block {
	uint64_t base_addr; /* array, size corresponds to ndim */
	struct ibv_mr *mr;
	size_t *byte_count; /* array, size corresponds to ndim */
	size_t *stride; /* array, size corresponds to ndim */
};

enum ibv_exp_umr_wr_type {
	IBV_EXP_UMR_MR_LIST,
	IBV_EXP_UMR_REPEAT
};

struct ibv_exp_send_wr {
	uint64_t		wr_id;
	struct ibv_exp_send_wr *next;
	struct ibv_sge	       *sg_list;
	int			num_sge;
	enum ibv_exp_wr_opcode	exp_opcode; /* use ibv_exp_wr_opcode */
	int			reserved; /* place holder to align with ibv_send_wr */
	union {
		uint32_t	imm_data; /* in network byte order */
		uint32_t	invalidate_rkey;
	} ex;
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
	union {
		struct {
			uint64_t		remote_addr;
			uint32_t		rkey;
		} rdma;
		struct {
			uint64_t		remote_addr;
			uint64_t		compare_add;
			uint64_t		swap;
			uint32_t		rkey;
		} atomic;
		struct {
			struct ibv_cq	*cq;
			int32_t  cq_count;
		} cqe_wait;
		struct {
			struct ibv_qp	*qp;
			int32_t  wqe_count;
		} wqe_enable;
	} task;
	union {
		struct {
			enum ibv_exp_calc_op        calc_op;
			enum ibv_exp_calc_data_type data_type;
			enum ibv_exp_calc_data_size data_size;
		} calc;
	} op;
	struct {
		struct ibv_ah   *ah;
		uint64_t        dct_access_key;
		uint32_t        dct_number;
	} dc;
	union {
		struct {
			struct ibv_mw			*mw;
			uint32_t			rkey;
			struct ibv_exp_mw_bind_info	bind_info;
		} bind_mw;
		struct {
			void				*hdr;
			uint16_t			hdr_sz;
			uint16_t			mss;
		} tso;
	};
	uint64_t	exp_send_flags; /* use ibv_exp_send_flags */
	uint32_t	comp_mask; /* reserved for future growth (must be 0) */
	union {
		struct {
			uint32_t umr_type; /* use ibv_exp_umr_wr_type */
			struct ibv_exp_mkey_list_container *memory_objects; /* used when IBV_EXP_SEND_INLINE is not set */
			uint64_t exp_access; /* use ibv_exp_access_flags */
			struct ibv_mr *modified_mr;
			uint64_t base_addr;
			uint32_t num_mrs; /* array size of mem_repeat_block_list or mem_reg_list */
			union {
				struct ibv_exp_mem_region *mem_reg_list; /* array, size corresponds to num_mrs */
				struct {
					struct ibv_exp_mem_repeat_block *mem_repeat_block_list; /* array,  size corresponds to num_mr */
					size_t *repeat_count; /* array size corresponds to stride_dim */
					uint32_t stride_dim;
				} rb;
			} mem_list;
		} umr;
		struct {
			uint32_t        log_arg_sz;
			uint64_t	remote_addr;
			uint32_t	rkey;
			union {
				struct {
					/* For the next four fields:
					 * If operand_size <= 8 then inline data is immediate
					 * from the corresponding field; for small opernands,
					 * ls bits are used.
					 * Else the fields are pointers in the process's address space
					 * where arguments are stored
					 */
					union {
						struct ibv_exp_cmp_swap cmp_swap;
						struct ibv_exp_fetch_add fetch_add;
					} op;
				} inline_data;       /* IBV_EXP_SEND_EXT_ATOMIC_INLINE is set */
				/* in the future add support for non-inline argument provisioning */
			} wr_data;
		} masked_atomics;
	} ext_op;
};

/*
 * Flags for ibv_exp_values struct comp_mask
 */
enum ibv_exp_values_comp_mask {
		IBV_EXP_VALUES_HW_CLOCK_NS	= 1 << 0,
		IBV_EXP_VALUES_HW_CLOCK		= 1 << 1,
		IBV_EXP_VALUES_RESERVED		= 1 << 2
};

struct ibv_exp_values {
	uint32_t comp_mask;
	uint64_t hwclock_ns;
	uint64_t hwclock;
};

/*
 * Flags for flags field in the ibv_exp_cq_init_attr struct
 */
enum ibv_exp_cq_create_flags {
	IBV_EXP_CQ_CREATE_CROSS_CHANNEL		= 1 << 0,
	IBV_EXP_CQ_TIMESTAMP			= 1 << 1,
	IBV_EXP_CQ_TIMESTAMP_TO_SYS_TIME	= 1 << 2,
	IBV_EXP_CQ_COMPRESSED_CQE		= 1 << 3,
	/*
	 * note: update IBV_EXP_CQ_CREATE_FLAGS_MASK when adding new fields
	 */
};

enum {
	IBV_EXP_CQ_CREATE_FLAGS_MASK	= IBV_EXP_CQ_CREATE_CROSS_CHANNEL |
					  IBV_EXP_CQ_TIMESTAMP |
					  IBV_EXP_CQ_TIMESTAMP_TO_SYS_TIME |
					  IBV_EXP_CQ_COMPRESSED_CQE,
};

/*
 * Flags for ibv_exp_cq_init_attr struct comp_mask
 * Set flags only when relevant field is valid
 */
enum ibv_exp_cq_init_attr_mask {
	IBV_EXP_CQ_INIT_ATTR_FLAGS		= 1 << 0,
	IBV_EXP_CQ_INIT_ATTR_RESERVED		= 1 << 1, /* This field is kept for backward compatibility
							   * of application which use the following to set comp_mask:
							   * cq_init_attr.comp_mask = IBV_EXP_CQ_INIT_ATTR_RESERVED - 1
							   * This kind of setting is no longer accepted and application
							   * may set only valid known fields, for example:
							   * cq_init_attr.comp_mask = IBV_EXP_CQ_INIT_ATTR_FLAGS |
							   *				IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN
							   */
	IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN		= 1 << 1,
	IBV_EXP_CQ_INIT_ATTR_PEER_DIRECT	= 1 << 2,
	IBV_EXP_CQ_INIT_ATTR_RESERVED1		= 1 << 3,
};

struct ibv_exp_res_domain {
	struct ibv_context     *context;
};

struct ibv_exp_cq_init_attr {
	uint32_t			 comp_mask;
	uint32_t			 flags;
	struct ibv_exp_res_domain	*res_domain;
	struct ibv_exp_peer_direct_attr *peer_direct_attrs;
};

/*
 * Flags for ibv_exp_ah_attr struct comp_mask
 */
enum ibv_exp_ah_attr_attr_comp_mask {
	IBV_EXP_AH_ATTR_LL		= 1 << 0,
	IBV_EXP_AH_ATTR_VID		= 1 << 1,
	IBV_EXP_AH_ATTR_RESERVED	= 1 << 2
};

enum ll_address_type {
	LL_ADDRESS_UNKNOWN,
	LL_ADDRESS_IB,
	LL_ADDRESS_ETH,
	LL_ADDRESS_SIZE
};

struct ibv_exp_ah_attr {
	struct ibv_global_route	grh;
	uint16_t		dlid;
	uint8_t			sl;
	uint8_t			src_path_bits;
	uint8_t			static_rate;
	uint8_t			is_global;
	uint8_t			port_num;
	uint32_t		comp_mask;
	struct {
		enum ll_address_type type;
		uint32_t	len;
		char		*address;
	} ll_address;
	uint16_t		vid;
};

/*
 * Flags for exp_attr_mask argument of ibv_exp_modify_qp
 */
enum ibv_exp_qp_attr_mask {
	IBV_EXP_QP_STATE		= IBV_QP_STATE,
	IBV_EXP_QP_CUR_STATE		= IBV_QP_CUR_STATE,
	IBV_EXP_QP_EN_SQD_ASYNC_NOTIFY	= IBV_QP_EN_SQD_ASYNC_NOTIFY,
	IBV_EXP_QP_ACCESS_FLAGS		= IBV_QP_ACCESS_FLAGS,
	IBV_EXP_QP_PKEY_INDEX		= IBV_QP_PKEY_INDEX,
	IBV_EXP_QP_PORT			= IBV_QP_PORT,
	IBV_EXP_QP_QKEY			= IBV_QP_QKEY,
	IBV_EXP_QP_AV			= IBV_QP_AV,
	IBV_EXP_QP_PATH_MTU		= IBV_QP_PATH_MTU,
	IBV_EXP_QP_TIMEOUT		= IBV_QP_TIMEOUT,
	IBV_EXP_QP_RETRY_CNT		= IBV_QP_RETRY_CNT,
	IBV_EXP_QP_RNR_RETRY		= IBV_QP_RNR_RETRY,
	IBV_EXP_QP_RQ_PSN		= IBV_QP_RQ_PSN,
	IBV_EXP_QP_MAX_QP_RD_ATOMIC	= IBV_QP_MAX_QP_RD_ATOMIC,
	IBV_EXP_QP_ALT_PATH		= IBV_QP_ALT_PATH,
	IBV_EXP_QP_MIN_RNR_TIMER	= IBV_QP_MIN_RNR_TIMER,
	IBV_EXP_QP_SQ_PSN		= IBV_QP_SQ_PSN,
	IBV_EXP_QP_MAX_DEST_RD_ATOMIC	= IBV_QP_MAX_DEST_RD_ATOMIC,
	IBV_EXP_QP_PATH_MIG_STATE	= IBV_QP_PATH_MIG_STATE,
	IBV_EXP_QP_CAP			= IBV_QP_CAP,
	IBV_EXP_QP_DEST_QPN		= IBV_QP_DEST_QPN,

	IBV_EXP_QP_GROUP_RSS		= IBV_EXP_START_FLAG << 21,
	IBV_EXP_QP_DC_KEY		= IBV_EXP_START_FLAG << 22,
	IBV_EXP_QP_FLOW_ENTROPY		= IBV_EXP_START_FLAG << 23,
	IBV_EXP_QP_RATE_LIMIT		= IBV_EXP_START_FLAG << 25,
};

/*
 * Flags for ibv_exp_qp_attr struct comp_mask
 * Set flags only when relevant field is valid
 */
enum ibv_exp_qp_attr_comp_mask {
	IBV_EXP_QP_ATTR_FLOW_ENTROPY	= 1UL << 0,
	IBV_EXP_QP_ATTR_RESERVED	= 1UL << 1
};

struct ibv_exp_qp_attr {
	enum ibv_qp_state	qp_state;
	enum ibv_qp_state	cur_qp_state;
	enum ibv_mtu		path_mtu;
	enum ibv_mig_state	path_mig_state;
	uint32_t		qkey;
	uint32_t		rq_psn;
	uint32_t		sq_psn;
	uint32_t		dest_qp_num;
	int			qp_access_flags; /* use ibv_access_flags form verbs.h */
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
	uint64_t		dct_key;
	uint32_t		comp_mask; /* reserved for future growth (must be 0) */
	uint32_t		flow_entropy;
	uint32_t		rate_limit;
};

/*
 * Flags for ibv_exp_qp_init_attr struct comp_mask
 * Set flags only when relevant field is valid
 */
enum ibv_exp_qp_init_attr_comp_mask {
	IBV_EXP_QP_INIT_ATTR_PD			= 1 << 0,
	IBV_EXP_QP_INIT_ATTR_XRCD		= 1 << 1,
	IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS	= 1 << 2,
	IBV_EXP_QP_INIT_ATTR_INL_RECV		= 1 << 3,
	IBV_EXP_QP_INIT_ATTR_QPG		= 1 << 4,
	IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG	= 1 << 5,
	IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS       = 1 << 6,
	IBV_EXP_QP_INIT_ATTR_RESERVED		= 1 << 7, /* This field is kept for backward compatibility
							   * of application which use the following to set comp_mask:
							   * qp_init_attr.comp_mask = IBV_EXP_QP_INIT_ATTR_RESERVED - 1
							   * This kind of setting is no longer accepted and application
							   * may set only valid known fields, for example:
							   * qp_init_attr.comp_mask = IBV_EXP_QP_INIT_ATTR_PD |
							   *				IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS
							   */
	IBV_EXP_QP_INIT_ATTR_RES_DOMAIN         = 1 << 7,
	IBV_EXP_QP_INIT_ATTR_RX_HASH		= 1 << 8,
	IBV_EXP_QP_INIT_ATTR_PORT		= 1 << 9,
	IBV_EXP_QP_INIT_ATTR_PEER_DIRECT	= 1 << 10,
	IBV_EXP_QP_INIT_ATTR_MAX_TSO_HEADER	= 1 << 11,
	IBV_EXP_QP_INIT_ATTR_RESERVED1		= 1 << 12,
};

enum ibv_exp_qpg_type {
	IBV_EXP_QPG_NONE	= 0,
	IBV_EXP_QPG_PARENT	= (1<<0),
	IBV_EXP_QPG_CHILD_RX	= (1<<1),
	IBV_EXP_QPG_CHILD_TX	= (1<<2)
};

struct ibv_exp_qpg_init_attrib {
	uint32_t tss_child_count;
	uint32_t rss_child_count;
};

struct ibv_exp_qpg {
	uint32_t qpg_type;
	union {
		struct ibv_qp *qpg_parent; /* see qpg_type */
		struct ibv_exp_qpg_init_attrib parent_attrib;
	};
};

/*
 * RX Hash Function flags.
*/
enum ibv_exp_rx_hash_function_flags {
	IBV_EXP_RX_HASH_FUNC_TOEPLITZ	= 1 << 0,
	IBV_EXP_RX_HASH_FUNC_XOR	= 1 << 1
};

/*
 * RX Hash flags, these flags allows to set which incoming packet field should
 * participates in RX Hash. Each flag represent certain packet's field,
 * when the flag is set the field that is represented by the flag will
 * participate in RX Hash calculation.
 * Notice: *IPV4 and *IPV6 flags can't be enabled together on the same QP
 * and *TCP and *UDP flags can't be enabled together on the same QP.
*/
enum ibv_exp_rx_hash_fields {
	IBV_EXP_RX_HASH_SRC_IPV4		= 1 << 0,
	IBV_EXP_RX_HASH_DST_IPV4		= 1 << 1,
	IBV_EXP_RX_HASH_SRC_IPV6		= 1 << 2,
	IBV_EXP_RX_HASH_DST_IPV6		= 1 << 3,
	IBV_EXP_RX_HASH_SRC_PORT_TCP	= 1 << 4,
	IBV_EXP_RX_HASH_DST_PORT_TCP	= 1 << 5,
	IBV_EXP_RX_HASH_SRC_PORT_UDP	= 1 << 6,
	IBV_EXP_RX_HASH_DST_PORT_UDP	= 1 << 7
};

/*
 * RX Hash QP configuration. Sets hash function, hash types and
 * Indirection table for QPs with enabled IBV_QP_INIT_ATTR_RX_HASH flag.
*/
struct ibv_exp_rx_hash_conf {
	/* enum ibv_exp_rx_hash_function_flags */
	uint8_t				rx_hash_function;
	/* valid only for Toeplitz */
	uint8_t			rx_hash_key_len;
	uint8_t                        *rx_hash_key;
	/* enum ibv_exp_rx_hash_fields */
	uint64_t			rx_hash_fields_mask;
	struct ibv_exp_rwq_ind_table       *rwq_ind_tbl;
};

/*
 * Flags for exp_create_flags field in ibv_exp_qp_init_attr struct
 */
enum ibv_exp_qp_create_flags {
	IBV_EXP_QP_CREATE_CROSS_CHANNEL        = (1 << 2),
	IBV_EXP_QP_CREATE_MANAGED_SEND         = (1 << 3),
	IBV_EXP_QP_CREATE_MANAGED_RECV         = (1 << 4),
	IBV_EXP_QP_CREATE_IGNORE_SQ_OVERFLOW   = (1 << 6),
	IBV_EXP_QP_CREATE_IGNORE_RQ_OVERFLOW   = (1 << 7),
	IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY      = (1 << 8),
	IBV_EXP_QP_CREATE_UMR		       = (1 << 9),
	IBV_EXP_QP_CREATE_EC_PARITY_EN	       = (1 << 10),
	IBV_EXP_QP_CREATE_RX_END_PADDING       = (1 << 11),
	IBV_EXP_QP_CREATE_SCATTER_FCS          = (1 << 12),
	IBV_EXP_QP_CREATE_INTERNAL_USE	       = (1 << 15),
	/* set supported bits for validity check */
	IBV_EXP_QP_CREATE_MASK                 = (0x00001FDC)
};

struct ibv_exp_qp_init_attr {
	void		       *qp_context;
	struct ibv_cq	       *send_cq;
	struct ibv_cq	       *recv_cq;
	struct ibv_srq	       *srq;
	struct ibv_qp_cap	cap;
	enum ibv_qp_type	qp_type;
	int			sq_sig_all;

	uint32_t		comp_mask; /* use ibv_exp_qp_init_attr_comp_mask */
	struct ibv_pd	       *pd;
	struct ibv_xrcd	       *xrcd;
	uint32_t		exp_create_flags; /* use ibv_exp_qp_create_flags */

	uint32_t		max_inl_recv;
	struct ibv_exp_qpg	qpg;
	uint32_t		max_atomic_arg;
	uint32_t                max_inl_send_klms;
	struct ibv_exp_res_domain *res_domain;
	struct ibv_exp_rx_hash_conf *rx_hash_conf;
	uint8_t port_num;
	struct ibv_exp_peer_direct_attr *peer_direct_attrs;
	uint16_t		max_tso_header;
};

/*
 * Flags for ibv_exp_dct_init_attr struct comp_mask
 */
enum ibv_exp_dct_init_attr_comp_mask {
	IBV_EXP_DCT_INIT_ATTR_RESERVED	= 1 << 0
};

enum {
	IBV_EXP_DCT_CREATE_FLAGS_MASK		= (1 << 0) - 1,
};

struct ibv_exp_dct_init_attr {
	struct ibv_pd	       *pd;
	struct ibv_cq	       *cq;
	struct ibv_srq	       *srq;
	uint64_t		dc_key;
	uint8_t			port;
	uint32_t		access_flags; /* use ibv_access_flags form verbs.h */
	uint8_t			min_rnr_timer;
	uint8_t			tclass;
	uint32_t		flow_label;
	enum ibv_mtu		mtu;
	uint8_t			pkey_index;
	uint8_t			gid_index;
	uint8_t			hop_limit;
	uint32_t		inline_size;
	uint32_t		create_flags;
	uint32_t		comp_mask; /* reserved for future growth (must be 0) */
};

enum {
	IBV_EXP_DCT_STATE_ACTIVE	= 0,
	IBV_EXP_DCT_STATE_DRAINING	= 1,
	IBV_EXP_DCT_STATE_DRAINED	= 2
};

/*
 * Flags for ibv_exp_dct_attr struct comp_mask
 */
enum ibv_exp_dct_attr_comp_mask {
	IBV_EXP_DCT_ATTR_RESERVED	= 1 << 0
};

struct ibv_exp_dct_attr {
	uint64_t		dc_key;
	uint8_t			port;
	uint32_t		access_flags; /* use ibv_access_flags form verbs.h */
	uint8_t			min_rnr_timer;
	uint8_t			tclass;
	uint32_t		flow_label;
	enum ibv_mtu		mtu;
	uint8_t			pkey_index;
	uint8_t			gid_index;
	uint8_t			hop_limit;
	uint32_t		key_violations;
	uint8_t			state;
	struct ibv_srq	       *srq;
	struct ibv_cq	       *cq;
	struct ibv_pd	       *pd;
	uint32_t		comp_mask; /* reserved for future growth (must be 0) */
};

enum {
	IBV_EXP_QUERY_PORT_STATE		= 1 << 0,
	IBV_EXP_QUERY_PORT_MAX_MTU		= 1 << 1,
	IBV_EXP_QUERY_PORT_ACTIVE_MTU		= 1 << 2,
	IBV_EXP_QUERY_PORT_GID_TBL_LEN		= 1 << 3,
	IBV_EXP_QUERY_PORT_CAP_FLAGS		= 1 << 4,
	IBV_EXP_QUERY_PORT_MAX_MSG_SZ		= 1 << 5,
	IBV_EXP_QUERY_PORT_BAD_PKEY_CNTR	= 1 << 6,
	IBV_EXP_QUERY_PORT_QKEY_VIOL_CNTR	= 1 << 7,
	IBV_EXP_QUERY_PORT_PKEY_TBL_LEN		= 1 << 8,
	IBV_EXP_QUERY_PORT_LID			= 1 << 9,
	IBV_EXP_QUERY_PORT_SM_LID		= 1 << 10,
	IBV_EXP_QUERY_PORT_LMC			= 1 << 11,
	IBV_EXP_QUERY_PORT_MAX_VL_NUM		= 1 << 12,
	IBV_EXP_QUERY_PORT_SM_SL		= 1 << 13,
	IBV_EXP_QUERY_PORT_SUBNET_TIMEOUT	= 1 << 14,
	IBV_EXP_QUERY_PORT_INIT_TYPE_REPLY	= 1 << 15,
	IBV_EXP_QUERY_PORT_ACTIVE_WIDTH		= 1 << 16,
	IBV_EXP_QUERY_PORT_ACTIVE_SPEED		= 1 << 17,
	IBV_EXP_QUERY_PORT_PHYS_STATE		= 1 << 18,
	IBV_EXP_QUERY_PORT_LINK_LAYER		= 1 << 19,
	/* mask of the fields that exists in the standard query_port_command */
	IBV_EXP_QUERY_PORT_STD_MASK		= (1 << 20) - 1,
	/* mask of all supported fields */
	IBV_EXP_QUERY_PORT_MASK			= IBV_EXP_QUERY_PORT_STD_MASK,
};

/*
 * Flags for ibv_exp_port_attr struct comp_mask
 * Set flags only when relevant field is valid
 */
enum ibv_exp_query_port_attr_comp_mask {
	IBV_EXP_QUERY_PORT_ATTR_MASK1		= 1 << 0,
	IBV_EXP_QUERY_PORT_ATTR_RESERVED	= 1 << 1,

	IBV_EXP_QUERY_PORT_ATTR_MASKS		= IBV_EXP_QUERY_PORT_ATTR_RESERVED - 1
};

struct ibv_exp_port_attr {
	union {
		struct {
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
		struct ibv_port_attr		port_attr;
	};
	uint32_t		comp_mask;
	uint32_t		mask1;
};

enum ibv_exp_cq_attr_mask {
	IBV_EXP_CQ_MODERATION                  = 1 << 0,
	IBV_EXP_CQ_CAP_FLAGS                   = 1 << 1
};

enum ibv_exp_cq_cap_flags {
	IBV_EXP_CQ_IGNORE_OVERRUN              = (1 << 0),
	/* set supported bits for validity check */
	IBV_EXP_CQ_CAP_MASK                    = (0x00000001)
};

/*
 * Flags for ibv_exp_cq_attr struct comp_mask
 * Set flags only when relevant field is valid
 */
enum ibv_exp_cq_attr_comp_mask {
	IBV_EXP_CQ_ATTR_MODERATION	= (1 << 0),
	IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS	= (1 << 1),
	/* set supported bits for validity check */
	IBV_EXP_CQ_ATTR_RESERVED	= (1 << 2)
};

struct ibv_exp_cq_attr {
	uint32_t                comp_mask;
	struct {
		uint16_t        cq_count;
		uint16_t        cq_period;
	} moderation;
	uint32_t		cq_cap_flags;
};

enum ibv_exp_rereg_mr_flags {
	IBV_EXP_REREG_MR_CHANGE_TRANSLATION	= IBV_REREG_MR_CHANGE_TRANSLATION,
	IBV_EXP_REREG_MR_CHANGE_PD		= IBV_REREG_MR_CHANGE_PD,
	IBV_EXP_REREG_MR_CHANGE_ACCESS		= IBV_REREG_MR_CHANGE_ACCESS,
	IBV_EXP_REREG_MR_KEEP_VALID		= IBV_REREG_MR_KEEP_VALID,
	IBV_EXP_REREG_MR_FLAGS_SUPPORTED	= ((IBV_EXP_REREG_MR_KEEP_VALID << 1) - 1)
};

enum ibv_exp_rereg_mr_attr_mask {
	IBV_EXP_REREG_MR_ATTR_RESERVED		= (1 << 0)
};

struct ibv_exp_rereg_mr_attr {
	uint32_t	comp_mask;  /* use ibv_exp_rereg_mr_attr_mask */
};

/*
 * Flags for ibv_exp_reg_shared_mr_in struct comp_mask
 */
enum ibv_exp_reg_shared_mr_comp_mask {
	IBV_EXP_REG_SHARED_MR_RESERVED	= (1 << 0)
};

struct ibv_exp_reg_shared_mr_in {
	uint32_t mr_handle;
	struct ibv_pd *pd;
	void *addr;
	uint64_t exp_access; /* use ibv_exp_access_flags */
	uint32_t comp_mask; /* reserved for future growth (must be 0) */
};

enum ibv_exp_flow_flags {
	IBV_EXP_FLOW_ATTR_FLAGS_ALLOW_LOOP_BACK = 1,
};

enum ibv_exp_flow_attr_type {
	/* steering according to rule specifications */
	IBV_EXP_FLOW_ATTR_NORMAL		= 0x0,
	/* default unicast and multicast rule -
	 * receive all Eth traffic which isn't steered to any QP
	 */
	IBV_EXP_FLOW_ATTR_ALL_DEFAULT	= 0x1,
	/* default multicast rule -
	 * receive all Eth multicast traffic which isn't steered to any QP
	 */
	IBV_EXP_FLOW_ATTR_MC_DEFAULT	= 0x2,
	/* sniffer rule - receive all port traffic */
	IBV_EXP_FLOW_ATTR_SNIFFER		= 0x3,
};

enum ibv_exp_flow_spec_type {
	IBV_EXP_FLOW_SPEC_ETH		= 0x20,
	IBV_EXP_FLOW_SPEC_IB		= 0x21,
	IBV_EXP_FLOW_SPEC_IPV4		= 0x30,
	IBV_EXP_FLOW_SPEC_IPV6		= 0x31,
	IBV_EXP_FLOW_SPEC_IPV4_EXT	= 0x32,
	IBV_EXP_FLOW_SPEC_IPV6_EXT	= 0x33,
	IBV_EXP_FLOW_SPEC_TCP		= 0x40,
	IBV_EXP_FLOW_SPEC_UDP		= 0x41,
	IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL	= 0x50,
	IBV_EXP_FLOW_SPEC_INNER		= 0x100,
	IBV_EXP_FLOW_SPEC_ACTION_TAG	= 0x1000,
};

struct ibv_exp_flow_eth_filter {
	uint8_t		dst_mac[6];
	uint8_t		src_mac[6];
	uint16_t	ether_type;
	/*
	 * same layout as 802.1q: prio 3, cfi 1, vlan id 12
	 */
	uint16_t	vlan_tag;
};

struct ibv_exp_flow_spec_eth {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_eth_filter val;
	struct ibv_exp_flow_eth_filter mask;
};

struct ibv_exp_flow_ib_filter {
	uint32_t qpn;
	uint8_t  dst_gid[16];
};

struct ibv_exp_flow_spec_ib {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_ib_filter val;
	struct ibv_exp_flow_ib_filter mask;
};

struct ibv_exp_flow_ipv4_filter {
	uint32_t src_ip;
	uint32_t dst_ip;
};

struct ibv_exp_flow_spec_ipv4 {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_ipv4_filter val;
	struct ibv_exp_flow_ipv4_filter mask;
};

struct ibv_exp_flow_ipv6_filter {
	uint8_t src_ip[16];
	uint8_t dst_ip[16];
};

struct ibv_exp_flow_spec_ipv6 {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_ipv6_filter val;
	struct ibv_exp_flow_ipv6_filter mask;
};

struct ibv_exp_flow_spec_action_tag {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	uint32_t  tag_id;
};

struct ibv_exp_flow_ipv6_ext_filter {
	uint8_t src_ip[16];
	uint8_t dst_ip[16];
	uint32_t flow_label;
	uint8_t  next_hdr;
	uint8_t  traffic_class;
	uint8_t  hop_limit;
};

struct ibv_exp_flow_spec_ipv6_ext {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_ipv6_ext_filter val;
	struct ibv_exp_flow_ipv6_ext_filter mask;
};

struct ibv_exp_flow_ipv4_ext_filter {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint8_t  proto;
	uint8_t  tos;
	uint8_t  ttl;
	uint8_t  flags;
};

struct ibv_exp_flow_spec_ipv4_ext {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_ipv4_ext_filter val;
	struct ibv_exp_flow_ipv4_ext_filter mask;
};

struct ibv_exp_flow_tcp_udp_filter {
	uint16_t dst_port;
	uint16_t src_port;
};

struct ibv_exp_flow_spec_tcp_udp {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_tcp_udp_filter val;
	struct ibv_exp_flow_tcp_udp_filter mask;
};

struct ibv_exp_flow_tunnel_filter {
	uint32_t tunnel_id;
};

struct ibv_exp_flow_spec_tunnel {
	enum ibv_exp_flow_spec_type  type;
	uint16_t  size;
	struct ibv_exp_flow_tunnel_filter val;
	struct ibv_exp_flow_tunnel_filter mask;
};

struct ibv_exp_flow_spec {
	union {
		struct {
			uint32_t type;
			uint16_t size;
		} hdr;
		struct ibv_exp_flow_spec_ib ib;
		struct ibv_exp_flow_spec_eth eth;
		struct ibv_exp_flow_spec_ipv4 ipv4;
		struct ibv_exp_flow_spec_ipv4_ext ipv4_ext;
		struct ibv_exp_flow_spec_tcp_udp tcp_udp;
		struct ibv_exp_flow_spec_ipv6 ipv6;
		struct ibv_exp_flow_spec_ipv6_ext ipv6_ext;
		struct ibv_exp_flow_spec_tunnel tunnel;
		struct ibv_exp_flow_spec_action_tag flow_tag;
	};
};

struct ibv_exp_flow_attr {
	enum ibv_exp_flow_attr_type type;
	uint16_t size;
	uint16_t priority;
	uint8_t num_of_specs;
	uint8_t port;
	uint32_t flags;
	/* Following are the optional layers according to user request
	 * struct ibv_exp_flow_spec_xxx [L2]
	 * struct ibv_exp_flow_spec_yyy [L3/L4]
	 */
	uint64_t reserved; /* reserved for future growth (must be 0) */
};

struct ibv_exp_flow {
	struct ibv_context *context;
	uint32_t	   handle;
};

struct ibv_exp_dct {
	struct ibv_context     *context;
	uint32_t		handle;
	uint32_t		dct_num;
	struct ibv_pd	       *pd;
	struct ibv_srq	       *srq;
	struct ibv_cq	       *cq;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	uint32_t		events_completed;
};

enum ibv_exp_wc_opcode {
	IBV_EXP_WC_SEND,
	IBV_EXP_WC_RDMA_WRITE,
	IBV_EXP_WC_RDMA_READ,
	IBV_EXP_WC_COMP_SWAP,
	IBV_EXP_WC_FETCH_ADD,
	IBV_EXP_WC_BIND_MW,
	IBV_EXP_WC_LOCAL_INV		=	7,
	IBV_EXP_WC_MASKED_COMP_SWAP	=	9,
	IBV_EXP_WC_MASKED_FETCH_ADD	=	10,
	IBV_EXP_WC_TSO,
	IBV_EXP_WC_UMR			=	0x100,
/*
 * Set value of IBV_EXP_WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & IBV_EXP_WC_RECV).
 */
	IBV_EXP_WC_RECV			= 1 << 7,
	IBV_EXP_WC_RECV_RDMA_WITH_IMM
};

enum ibv_exp_wc_flags {
	IBV_EXP_WC_GRH		= IBV_WC_GRH,
	IBV_EXP_WC_WITH_IMM	= IBV_WC_WITH_IMM,

	IBV_EXP_WC_WITH_INV			= IBV_EXP_START_FLAG << 2,
	IBV_EXP_WC_WITH_SL			= IBV_EXP_START_FLAG << 4,
	IBV_EXP_WC_WITH_SLID			= IBV_EXP_START_FLAG << 5,
	IBV_EXP_WC_WITH_TIMESTAMP		= IBV_EXP_START_FLAG << 6,
	IBV_EXP_WC_QP				= IBV_EXP_START_FLAG << 7,
	IBV_EXP_WC_SRQ				= IBV_EXP_START_FLAG << 8,
	IBV_EXP_WC_DCT				= IBV_EXP_START_FLAG << 9,
	IBV_EXP_WC_RX_IP_CSUM_OK		= IBV_EXP_START_FLAG << 10,
	IBV_EXP_WC_RX_TCP_UDP_CSUM_OK		= IBV_EXP_START_FLAG << 11,
	IBV_EXP_WC_RX_IPV4_PACKET		= IBV_EXP_START_FLAG << 12,
	IBV_EXP_WC_RX_IPV6_PACKET		= IBV_EXP_START_FLAG << 13,
	IBV_EXP_WC_RX_TUNNEL_PACKET		= IBV_EXP_START_FLAG << 14,
	IBV_EXP_WC_RX_OUTER_IP_CSUM_OK		= IBV_EXP_START_FLAG << 15,
	IBV_EXP_WC_RX_OUTER_TCP_UDP_CSUM_OK	= IBV_EXP_START_FLAG << 16,
	IBV_EXP_WC_RX_OUTER_IPV4_PACKET		= IBV_EXP_START_FLAG << 17,
	IBV_EXP_WC_RX_OUTER_IPV6_PACKET		= IBV_EXP_START_FLAG << 18,
};

struct ibv_exp_wc {
	uint64_t		wr_id;
	enum ibv_wc_status	status;
	enum ibv_exp_wc_opcode	exp_opcode;
	uint32_t		vendor_err;
	uint32_t		byte_len;
	uint32_t		imm_data; /* in network byte order */
	uint32_t		qp_num;
	uint32_t		src_qp;
	int			reserved; /* place holder to align with ibv_wc */
	uint16_t		pkey_index;
	uint16_t		slid; /* invalid when TS is used */
	uint8_t			sl; /* invalid when TS is used */
	uint8_t			dlid_path_bits;
	uint64_t		timestamp;
	struct ibv_qp	       *qp;
	struct ibv_srq	       *srq;
	struct ibv_exp_dct     *dct;
	uint64_t		exp_wc_flags; /* use ibv_exp_wc_flags */
};

/*
 * Flags for ibv_exp_prefetch_mr comp_mask
 */
enum ibv_exp_prefetch_attr_comp_mask {
	IBV_EXP_PREFETCH_MR_RESERVED	= (1 << 0),
};

/*
 * Flags for ibv_exp_prefetch_mr flags
 */
enum ibv_exp_prefetch_attr_flags {
	/* request prefetching for write access. Used for both local and remote */
	IBV_EXP_PREFETCH_WRITE_ACCESS = (1 << 0),
};

struct ibv_exp_prefetch_attr {
	/* Use enum ibv_exp_prefetch_attr_flags */
	uint32_t flags;
	/* Address of the area to prefetch */
	void *addr;
	/* Length of the area to prefetch */
	size_t length;
	uint32_t comp_mask;
};

/*
 * Flags for ibv_exp_reg_mr_in struct comp_mask
 */
enum ibv_exp_reg_mr_in_comp_mask {
	/* set supported bits for validity check */
	IBV_EXP_REG_MR_CREATE_FLAGS	= (1 << 0),
	IBV_EXP_REG_MR_RESERVED		= (1 << 1)
};

enum ibv_exp_reg_mr_create_flags {
	IBV_EXP_REG_MR_CREATE_CONTIG		= (1 << 0)  /* register mr with contiguous pages */
};

struct ibv_exp_reg_mr_in {
	struct ibv_pd *pd;
	void *addr;
	size_t length;
	uint64_t exp_access; /* use ibv_exp_access_flags */
	uint32_t comp_mask; /* reserved for future growth (must be 0) */
	uint32_t create_flags; /* use ibv_exp_reg_mr_create_flags */
};


enum ibv_exp_task_type {
	IBV_EXP_TASK_SEND         = 0,
	IBV_EXP_TASK_RECV         = 1
};

/*
 * Flags for ibv_exp_task struct comp_mask
 */
enum ibv_exp_task_comp_mask {
	IBV_EXP_TASK_RESERVED	= (1 << 0)
};

struct ibv_exp_task {
	enum ibv_exp_task_type	task_type;
	struct {
		struct ibv_qp  *qp;
		union {
			struct ibv_exp_send_wr  *send_wr;
			struct ibv_recv_wr  *recv_wr;
		};
	} item;
	struct ibv_exp_task    *next;
	uint32_t                comp_mask; /* reserved for future growth (must be 0) */
};

/*
 * Flags for ibv_exp_arm_attr struct comp_mask
 */
enum ibv_exp_arm_attr_comp_mask {
	IBV_EXP_ARM_ATTR_RESERVED	= (1 << 0)
};
struct ibv_exp_arm_attr {
	uint32_t	comp_mask; /* reserved for future growth (must be 0) */
};

enum ibv_exp_mr_create_flags {
	IBV_EXP_MR_SIGNATURE_EN		= (1 << 0),
	IBV_EXP_MR_INDIRECT_KLMS	= (1 << 1)
};

struct ibv_exp_mr_init_attr {
	uint32_t max_klm_list_size; /* num of entries */
	uint32_t create_flags; /* use ibv_exp_mr_create_flags */
	uint64_t exp_access_flags; /* use ibv_exp_access_flags */
};

/*
 * Comp_mask for ibv_exp_create_mr_in struct comp_mask
 */
enum ibv_exp_create_mr_in_comp_mask {
	IBV_EXP_CREATE_MR_IN_RESERVED	= (1 << 0)
};

struct ibv_exp_create_mr_in {
	struct ibv_pd *pd;
	struct ibv_exp_mr_init_attr attr;
	uint32_t comp_mask; /* use ibv_exp_create_mr_in_comp_mask */
};

/*
 * Flags for ibv_exp_mkey_attr struct comp_mask
 */
enum ibv_exp_mkey_attr_comp_mask {
	IBV_EXP_MKEY_ATTR_RESERVED	= (1 << 0)
};

struct ibv_exp_mkey_attr {
	uint32_t max_klm_list_size;
	uint32_t comp_mask; /* use ibv_exp_mkey_attr_comp_mask */
};

struct ibv_exp_mkey_list_container {
	uint32_t max_klm_list_size;
	struct ibv_context *context;
};

enum ibv_exp_mkey_list_type {
	IBV_EXP_MKEY_LIST_TYPE_INDIRECT_MR
};

/*
 * Flags for ibv_exp_mkey_list_container_attr struct comp_mask
 */
enum ibv_exp_alloc_mkey_list_comp_mask {
	IBV_EXP_MKEY_LIST_CONTAINER_RESERVED	= (1 << 0)
};

struct ibv_exp_mkey_list_container_attr {
	struct ibv_pd *pd;
	uint32_t mkey_list_type;  /* use ibv_exp_mkey_list_type */
	uint32_t max_klm_list_size;
	uint32_t comp_mask; /*use ibv_exp_alloc_mkey_list_comp_mask */
};

/*
 * Flags for ibv_exp_rereg_out struct comp_mask
 */
enum ibv_exp_rereg_mr_comp_mask {
	IBV_EXP_REREG_MR_RESERVED	= (1 << 0)
};

struct ibv_exp_rereg_out {
	int need_dofork;
	uint32_t comp_mask; /* use ibv_exp_rereg_mr_comp_mask */
};

/*
 * Flags for ibv_exp_dereg_out struct comp_mask
 */
enum ibv_exp_dereg_mr_comp_mask {
	IBV_EXP_DEREG_MR_RESERVED	= (1 << 0)
};

struct ibv_exp_dereg_out {
	int need_dofork;
	uint32_t comp_mask; /* use ibv_exp_dereg_mr_comp_mask */
};

struct verbs_env_item {
	char			*name;
	char			*value;
	struct verbs_env_item	*next;
};

struct verbs_environment {
	struct verbs_env_item  *head;
	pthread_mutex_t		mtx;
};

/* RSS stuff */

enum ibv_exp_wq_type {
	IBV_EXP_WQT_RQ,
	IBV_EXP_WQT_SRQ
};

enum ibv_exp_wq_state {
	IBV_EXP_WQS_RESET,
	IBV_EXP_WQS_RDY,
	IBV_EXP_WQS_ERR,
	IBV_EXP_WQS_UNKNOWN
};

/* VLAN Offloads */
enum ibv_exp_vlan_offloads {
	/* Represents C-VLAN stripping feature */
	IBV_EXP_RECEIVE_WQ_CVLAN_STRIP 			= (1 << 0),
	/* Represents C-VLAN insertion feature*/
	IBV_EXP_RECEIVE_WQ_CVLAN_INSERTION 		= (1 << 1),
	IBV_EXP_RECEIVE_WQ_VLAN_OFFLOADS_RESERVED 	= (1 << 2),
};

/*
 * Work Queue. QP can be created without internal WQs "packaged" inside it,
 * this QPs can be configured to use "external" WQ object as its
 * receive/send queue.
 * WQ associated (many to one) with Completion Queue it owns WQ properties
 * (PD, WQ size etc).
 * WQ of type IBV_EXP_WQT_RQ contains receive WQEs, in which case its PD serves
 * scatter as well.
 * WQ of type IBV_EXP_WQT_SRQ is associated (many to one) with regular ibv_srq,
 * in which case it does not hold receive WQEs.
 * QPs can be associated with IBV_EXP_WQT_S/RQ WQs via WQ Indirection Table.
 */
struct ibv_exp_wq {
	struct ibv_context     *context;
	void		       *wq_context; /* Associated Context of the WQ */
	uint32_t		handle;
	/* Protection domain WQ should be associated with */
	struct	ibv_pd	       *pd;
	/* CQ to be associated with the WQ */
	struct	ibv_cq	       *cq;
	/* SRQ handle if WQ is to be associated with an SRQ, otherwise NULL */
	struct	ibv_srq	       *srq;
	uint32_t		wq_num;
	enum ibv_exp_wq_state       state;
	enum ibv_exp_wq_type	wq_type;
	uint32_t		comp_mask;
};

enum ibv_exp_wq_init_attr_mask {
	IBV_EXP_CREATE_WQ_RES_DOMAIN	= (1 << 0),
	IBV_EXP_CREATE_WQ_MP_RQ		= (1 << 1),
	IBV_EXP_CREATE_WQ_VLAN_OFFLOADS = (1 << 2),
	IBV_EXP_CREATE_WQ_FLAGS		= (1 << 3),
	IBV_EXP_CREATE_WQ_RESERVED	= (1 << 4)
};

struct ibv_exp_wq_mp_rq {
	enum ibv_exp_mp_rq_shifts	use_shift;
	uint8_t				single_wqe_log_num_of_strides;
	uint8_t				single_stride_log_num_of_bytes;
};

enum ibv_exp_wq_init_attr_flags {
	IBV_EXP_CREATE_WQ_FLAG_RX_END_PADDING	= (1ULL << 0),
	IBV_EXP_CREATE_WQ_FLAG_SCATTER_FCS	= (1ULL << 1),
	IBV_EXP_CREATE_WQ_FLAG_RESERVED		= (1ULL << 2)
};

struct ibv_exp_wq_init_attr {
	/* Associated Context of the WQ */
	void		       *wq_context;
	enum ibv_exp_wq_type	wq_type;
	/* Valid for non IBV_EXP_WQT_SRQ WQ */
	uint32_t		max_recv_wr;
	/* Valid for non IBV_EXP_WQT_SRQ WQ */
	uint32_t		max_recv_sge;
	/* Protection domain WQ should be associated with */
	struct	ibv_pd	       *pd;
	/* CQ to be associated with the WQ */
	struct	ibv_cq	       *cq;
	/* SRQ handle if WQ is of type IBV_EXP_WQT_SRQ, otherwise NULL */
	struct	ibv_srq	       *srq;
	/* refers to ibv_exp_wq_init_attr_mask */
	uint32_t		comp_mask;
	struct ibv_exp_res_domain *res_domain;
	struct ibv_exp_wq_mp_rq	mp_rq;
	uint16_t		vlan_offloads; /* use ibv_exp_vlan_offloads enum */
	uint64_t		flags; /* general wq create flags */
};

enum ibv_exp_wq_attr_mask {
	IBV_EXP_WQ_ATTR_STATE		= 1 << 0,
	IBV_EXP_WQ_ATTR_CURR_STATE	= 1 << 1,
	IBV_EXP_WQ_ATTR_VLAN_OFFLOADS	= 1 << 2,
	IBV_EXP_WQ_ATTR_RESERVED	= 1 << 3
};

struct ibv_exp_wq_attr {
	/* enum ibv_exp_wq_attr_mask */
	uint32_t		attr_mask;
	/* Move the RQ to this state */
	enum	ibv_exp_wq_state	wq_state;
	/* Assume this is the current RQ state */
	enum	ibv_exp_wq_state	curr_wq_state;
	uint16_t		vlan_offloads; /* use ibv_exp_vlan_offloads enum */
};

/*
 * Receive Work Queue Indirection Table.
 * It's used in order to distribute incoming packets between different
 * Receive Work Queues. Associating Receive WQs with different CPU cores
 * allows to workload the traffic between different CPU cores.
 * The Indirection Table can contain only WQs of type IBV_EXP_WQT_S/RQ.
*/
struct ibv_exp_rwq_ind_table {
	struct ibv_context *context;
	struct ibv_pd *pd;
	int ind_tbl_handle;
	int ind_tbl_num;
	uint32_t comp_mask;
};

enum ibv_exp_ind_table_init_attr_mask {
	IBV_EXP_CREATE_IND_TABLE_RESERVED = (1 << 0)
};

/*
 * Receive Work Queue Indirection Table attributes
*/
struct ibv_exp_rwq_ind_table_init_attr {
	struct ibv_pd *pd;
	/* Log, base 2, of Indirection table size */
	uint32_t log_ind_tbl_size;
	/* Each entry is a pointer to Receive Work Queue */
	struct ibv_exp_wq **ind_tbl;
	uint32_t comp_mask;
};

/* Accelerated verbs */
enum ibv_exp_thread_model {
	IBV_EXP_THREAD_SAFE,	/* The lib responsible to protect the object in multithreaded environment */
	IBV_EXP_THREAD_UNSAFE,	/* The application responsible to protect the object in multithreaded environment */
	IBV_EXP_THREAD_SINGLE	/* The object is called from only one thread */
};

enum ibv_exp_msg_model {
	IBV_EXP_MSG_DEFAULT,		/* Use the provider default message model */
	IBV_EXP_MSG_LOW_LATENCY,	/* Hint the provider to optimize for low latency */
	IBV_EXP_MSG_HIGH_BW,		/* Hint the provider to optimize for high bandwidth */
	IBV_EXP_MSG_FORCE_LOW_LATENCY,	/* Force the provider to optimize for low latency */
};

/*
 * Resource domains
 */
enum ibv_exp_res_domain_init_attr_comp_mask {
	IBV_EXP_RES_DOMAIN_THREAD_MODEL	= (1 << 0),
	IBV_EXP_RES_DOMAIN_MSG_MODEL	= (1 << 1),
	IBV_EXP_RES_DOMAIN_RESERVED	= (1 << 2),
};

struct ibv_exp_res_domain_init_attr {
	uint32_t			comp_mask; /* use ibv_exp_res_domain_init_attr_comp_mask */
	enum ibv_exp_thread_model	thread_model;
	enum ibv_exp_msg_model		msg_model;
};

enum ibv_exp_destroy_res_domain_comp_mask {
	IBV_EXP_DESTROY_RES_DOMAIN_RESERVED	= (1 << 0),
};

struct ibv_exp_destroy_res_domain_attr {
	uint32_t	comp_mask; /* use ibv_exp_destroy_res_domain_comp_mask */
};

/*
 * Query interface (specialized Verbs)
 */

enum ibv_exp_query_intf_flags {
	/* Interface functions includes correctness and validity checks */
	IBV_EXP_QUERY_INTF_FLAG_ENABLE_CHECKS	= (1 << 0),
};

enum ibv_exp_intf_family {
	IBV_EXP_INTF_QP_BURST,
	IBV_EXP_INTF_CQ,
	IBV_EXP_INTF_WQ,
	IBV_EXP_INTF_RESERVED,
};

enum ibv_exp_experimental_intf_family {
	IBV_EXP_EXPERIMENTAL_INTF_RESERVED,
};

enum ibv_exp_intf_scope {
	IBV_EXP_INTF_GLOBAL, /* Permanent interface, identified by
	 	 	      * the ibv_exp_intf_family enum
	 	 	      */
	IBV_EXP_INTF_EXPERIMENTAL, /* Interface under evaluation, identified by
	 	 	 	    * the ibv_exp_experimental_intf_family enum
	 	 	 	    * This interface may change between lib
	 	 	 	    * versions
	 			    */
	IBV_EXP_INTF_VENDOR, /* Vendor specific interface, defined in vendor
			      * separate header file
			      */
	IBV_EXP_INTF_VENDOR_EXPERIMENTAL, /* Vendor interface under evaluation,
	 	 	 	 	   * defined in vendor separate header
	 	 	 	 	   * file
	 	 	 	 	   */
};

/* Return status from ibv_exp_query_intf */
enum ibv_exp_query_intf_status {
	IBV_EXP_INTF_STAT_OK,
	IBV_EXP_INTF_STAT_VENDOR_NOT_SUPPORTED, /* The provided 'vendor_guid' is not supported */
	IBV_EXP_INTF_STAT_INTF_NOT_SUPPORTED, /* The provided 'intf' is not supported */
	IBV_EXP_INTF_STAT_VERSION_NOT_SUPPORTED, /* The provided 'intf_version' is not supported */
	IBV_EXP_INTF_STAT_INVAL_PARARM, /* General invalid parameter */
	IBV_EXP_INTF_STAT_INVAL_OBJ_STATE, /* QP is not in INIT, RTR or RTS state */
	IBV_EXP_INTF_STAT_INVAL_OBJ, /* Mismatch between the provided 'obj'(CQ/QP/WQ) and requested 'intf' */
	IBV_EXP_INTF_STAT_FLAGS_NOT_SUPPORTED, /* The provided set of 'flags' is not supported */
	IBV_EXP_INTF_STAT_FAMILY_FLAGS_NOT_SUPPORTED, /* The provided set of 'family_flags' is not supported */
};

enum ibv_exp_query_intf_comp_mask {
	IBV_EXP_QUERY_INTF_RESERVED	= (1 << 0),
};

struct ibv_exp_query_intf_params {
	uint32_t			flags;		/* use ibv_exp_query_intf_flags */
	enum ibv_exp_intf_scope		intf_scope;
	uint64_t			vendor_guid;	/* set in case VENDOR intf_scope selected */
	uint32_t			intf;		/* for GLOBAL intf_scope use ibv_exp_intf_family enum */
	uint32_t			intf_version;	/* Version */
	void				*obj;		/* interface object (CQ/QP/WQ) */
	void				*family_params;	/* Family-specific params */
	uint32_t			family_flags;	/* Family-specific flags */
	uint32_t			comp_mask;	/* use ibv_exp_query_intf_comp_mask */
};

enum ibv_exp_release_intf_comp_mask {
	IBV_EXP_RELEASE_INTF_RESERVED =	(1 << 0),
};

struct ibv_exp_release_intf_params {
	uint32_t comp_mask; /* use ibv_exp_release_intf_comp_mask */
};

/*
 * Family interfaces
 */

/* QP burst family */

/* Flags to use in family_flags field of ibv_exp_query_intf_params on family creation */
enum ibv_exp_qp_burst_family_create_flags {
	/* To disable loop-back of multi-cast messages in RAW-ETH */
	IBV_EXP_QP_BURST_CREATE_DISABLE_ETH_LOOPBACK		= (1 << 0),
	/* To enable Multi-Packet send WR when possible */
	IBV_EXP_QP_BURST_CREATE_ENABLE_MULTI_PACKET_SEND_WR	= (1 << 1),
};

/* Flags to use on send functions of QP burst family */
enum ibv_exp_qp_burst_family_flags {
	IBV_EXP_QP_BURST_SIGNALED	= 1 << 0,
	IBV_EXP_QP_BURST_SOLICITED	= 1 << 1,
	IBV_EXP_QP_BURST_IP_CSUM	= 1 << 2,
	IBV_EXP_QP_BURST_TUNNEL		= 1 << 3,
	IBV_EXP_QP_BURST_FENCE		= 1 << 4,
};

/* All functions of QP family included in QP family version 1 */
struct ibv_exp_qp_burst_family {
	int (*send_pending)(struct ibv_qp *qp, uint64_t addr, uint32_t length, uint32_t lkey, uint32_t flags);
	int (*send_pending_inline)(struct ibv_qp *qp, void *addr, uint32_t length, uint32_t flags);
	int (*send_pending_sg_list)(struct ibv_qp *qp, struct ibv_sge *sg_list, uint32_t num, uint32_t flags);
	int (*send_flush)(struct ibv_qp *qp);
	int (*send_burst)(struct ibv_qp *qp, struct ibv_sge *msg_list, uint32_t num, uint32_t flags);
	int (*recv_burst)(struct ibv_qp *qp, struct ibv_sge *msg_list, uint32_t num);
};

struct ibv_exp_qp_burst_family_v1 {
	/*
	 * send_pending - Put one message in the provider send queue.
	 *
	 * Common usage: After calling several times to send_pending
	 *    the application need to call send_flush to ensure the send
	 *    of the pending messages.
	 * Note: Use ibv_exp_qp_burst_family_flags for the flags field
	 */
	int (*send_pending)(struct ibv_qp *qp, uint64_t addr, uint32_t length, uint32_t lkey, uint32_t flags);
	/*
	 * send_pending_inline - Put one inline message in the provider send queue.
	 *
	 * Common usage: Same as send_pending
	 * Notes:
	 *  - The message length must fit the max inline size of the QP.
	 *    Providing bigger messages may lead to data corruption and
	 *    segmentation fault.
	 *  - Use ibv_exp_qp_burst_family_flags for the flags field
	 */
	int (*send_pending_inline)(struct ibv_qp *qp, void *addr, uint32_t length, uint32_t flags);
	/*
	 * send_pending_sg_list - Put one scatter-gather(sg) message in the provider send queue.
	 *
	 * Common usage: Same as send_pending
	 * Notes:
	 *  - The number of sg entries must fit the max_send_sge of the QP.
	 *    Providing bigger list of sg entries may lead to data corruption and
	 *    segmentation fault.
	 *  - Use ibv_exp_qp_burst_family_flags for the flags field
	 */
	int (*send_pending_sg_list)(struct ibv_qp *qp, struct ibv_sge *sg_list, uint32_t num, uint32_t flags);
	/*
	 * send_flush - To flush the pending messages.
	 *
	 * Note: Use ibv_exp_qp_burst_family_flags for the flags field
	 */
	int (*send_flush)(struct ibv_qp *qp);
	/*
	 * send_burst - Send a list of 'num' messages (no send_flush required in this case)
	 */
	int (*send_burst)(struct ibv_qp *qp, struct ibv_sge *msg_list, uint32_t num, uint32_t flags);
	/*
	 * recv_burst - Post a set of 'num' receive buffers.
	 *
	 * Note: One sge per message is supported by this function
	 */
	int (*recv_burst)(struct ibv_qp *qp, struct ibv_sge *msg_list, uint32_t num);
	/*
	 * send_pending_vlan - Put one message in the provider send queue
	 * and insert vlan_tci to header.
	 *
	 * Common usage: Same as send_pending
	 * Note:
	 * - Same as send_pending
	 * - Not supported when MP enable.
	 */
	int (*send_pending_vlan)(struct ibv_qp *qp, uint64_t addr, uint32_t length,
				 uint32_t lkey, uint32_t flags, uint16_t *vlan_tci);
	/*
	 * send_pending_inline - Put one inline message in the provider send queue
	 * and insert vlan_tci to header.
	 *
	 * Common usage: Same as send_pending_inline
	 * Notes:
	 * - Same as send_pending_inline
	 * - Not supported when MP enable.
	 */
	int (*send_pending_inline_vlan)(struct ibv_qp *qp, void *addr, uint32_t length,
					uint32_t flags, uint16_t *vlan_tci);
	/*
	 * send_pending_sg_list - Put one scatter-gather(sg) message in the provider send queue
	 * and insert vlan_tci to header.
	 *
	 * Common usage: Same as send_pending_sg_list
	 * Notes:
	 * - Same as send_pending_sg_list
	 * - Not supported when MP enable.
	 */
	int (*send_pending_sg_list_vlan)(struct ibv_qp *qp, struct ibv_sge *sg_list, uint32_t num,
					 uint32_t flags, uint16_t *vlan_tci);
};

/* WQ family */
struct ibv_exp_wq_family {
	/*
	 * recv_sg_list - Post one scatter-gather(sg) receive buffer.
	 *
	 * Note:
	 *  - The number of sg entries must fit the max_recv_sge of the WQ.
	 *    Providing bigger list of sg entries may lead to data corruption and
	 *    segmentation fault.
	 */
	int (*recv_sg_list)(struct ibv_exp_wq *wq, struct ibv_sge *sg_list, uint32_t num_sg);
	/*
	 * recv_burst - Post a set of 'num' receive buffers.
	 *
	 * Note: One sge per message is supported by this function
	 */
	int (*recv_burst)(struct ibv_exp_wq *wq, struct ibv_sge *msg_list, uint32_t num);
};

/* CQ family */
enum ibv_exp_cq_family_flags {
	/* RX offloads flags */
							/* The cq_family_flags are applicable
							 * according to the existence of the
							 * related device capabilities flags */
	IBV_EXP_CQ_RX_IP_CSUM_OK		= 1 << 0, /* IBV_EXP_DEVICE_RX_CSUM_IP_PKT or IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT */
	IBV_EXP_CQ_RX_TCP_UDP_CSUM_OK		= 1 << 1, /* IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT */
	IBV_EXP_CQ_RX_IPV4_PACKET		= 1 << 2, /* IBV_EXP_DEVICE_RX_CSUM_IP_PKT or IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT */
	IBV_EXP_CQ_RX_IPV6_PACKET		= 1 << 3, /* IBV_EXP_DEVICE_RX_CSUM_IP_PKT or IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT */
	IBV_EXP_CQ_RX_TUNNEL_PACKET		= 1 << 4, /* IBV_EXP_DEVICE_VXLAN_SUPPORT */
	IBV_EXP_CQ_RX_OUTER_IP_CSUM_OK		= 1 << 5, /* IBV_EXP_DEVICE_VXLAN_SUPPORT */
	IBV_EXP_CQ_RX_OUTER_TCP_UDP_CSUM_OK	= 1 << 6, /* IBV_EXP_DEVICE_VXLAN_SUPPORT */
	IBV_EXP_CQ_RX_OUTER_IPV4_PACKET		= 1 << 7, /* IBV_EXP_DEVICE_VXLAN_SUPPORT */
	IBV_EXP_CQ_RX_OUTER_IPV6_PACKET		= 1 << 8, /* IBV_EXP_DEVICE_VXLAN_SUPPORT */

	/* Flags supported from CQ family version 1 */
	/* Multi-Packet RQ flag */
	IBV_EXP_CQ_RX_MULTI_PACKET_LAST_V1	= 1 << 9, /* Last packet on WR */
	/* CVLAN stripping RQ flag */
	IBV_EXP_CQ_RX_CVLAN_STRIPPED_V1		= 1 << 10, /*
							    * When set, CVLAN is stripped
							    * from incoming packets.
							    */

							   /* The RX TCP/UDP packet flags
							    * applicable according to
							    * IBV_EXP_DEVICE_RX_TCP_UDP_PKT_TYPE
							    * device capabilities flag
							    */
	IBV_EXP_CQ_RX_TCP_PACKET		= 1 << 11,
	IBV_EXP_CQ_RX_UDP_PACKET		= 1 << 12,
};

/* All functions of CQ family included in CQ family version 1 */
struct ibv_exp_cq_family {
	int32_t (*poll_cnt)(struct ibv_cq *cq, uint32_t max);
	int32_t (*poll_length)(struct ibv_cq *cq, void *buf, uint32_t *inl);
	int32_t (*poll_length_flags)(struct ibv_cq *cq, void *buf, uint32_t *inl, uint32_t *flags);
};

struct ibv_exp_cq_family_v1 {
	/*
	 * poll_cnt - Poll up to 'max' valid completions
	 *
	 * The function returns the number of valid completions it
	 * managed to drain from the CQ.
	 *
	 * Usage example: In case a CQ is connected to one send-queue
	 *                the application may use this function to get
	 *                the number of the QP send-completions.
	 *
	 * Return value (n):
	 *    n >= 0 : number extracted completions.
	 *    n == -1 : operation failed. completion is not extracted.
	 *              To extract this completion, ibv_poll_cq() must be used
	 *
	 * Note: The function designed to support TX completion, it may also be
	 *    used for RX completion but it is not supporting RX inline-scatter.
	 */
	int32_t (*poll_cnt)(struct ibv_cq *cq, uint32_t max);
	/*
	 * poll_length - Poll one receive completion and provide the related
	 *               message length.
	 *
	 * The function returns only the length of the completed message.
	 * In case of inline received message the message will be copied
	 * to the provided buffer ('buf') and the '*inl' status will be set.
	 * The function extracts only completion of regular receive-messages.
	 * In case of send-message completion or SRQ receive-message completion
	 * it returns -1.
	 *
	 * Usage example: In case a CQ is connected to one receive-queue
	 *                the application may use this function to get
	 *                the size of the next received message.
	 *
	 * Return value (n):
	 *    n > 0 : successful completion with positive length.
	 *            *inl will be set to 1 if data was copied to buffer.
	 *
	 *    0     : Empty.
	 *    n == -1 : operation failed. completion is not extracted.
	 *              To extract this completion, ibv_poll_cq() must be used
	 */
	int32_t (*poll_length)(struct ibv_cq *cq, void *buf, uint32_t *inl);
	/*
	 * poll_length_flags - Poll one receive completion and provide the related
	 *                     message length and completion flags.
	 *
	 * The same as poll_length but also retrieves completion flags as
	 * defined by the enum ibv_exp_cq_family_flags
	 */
	int32_t (*poll_length_flags)(struct ibv_cq *cq, void *buf, uint32_t *inl, uint32_t *flags);
	/*
	 * poll_length_flags_mp_rq - Poll one receive completion and provide the related
	 *                           message length, packet-offset and completion flags.
	 *
	 * The same as poll_length_flags but:
	 *  - Without the inline-receive support.
	 *  - Also retrieves offset in the WR posted buffer as defined by the WR SG list.
	 *    The start of the received packet is located in this offset.
	 */
	int32_t (*poll_length_flags_mp_rq)(struct ibv_cq *cq, uint32_t *offset, uint32_t *flags);
	/*
	 * poll_length_flags_cvlan - Poll one receive completion and provide the
	 *			     related message length, completion flags
	 *			     and CVLAN TCI.
	 *			     The CVLAN TCI value is valid only when
	 *			     IBV_EXP_CQ_RX_CVLAN_STRIPPED_V1 flag is
	 *			     set.
	 *
	 * The same as poll_length_flags but:
	 *  - Also retrievs the packet's CVLAN TCI that was stripped by the HW.
	 */
	int32_t (*poll_length_flags_cvlan)(struct ibv_cq *cq, void *buf,
					   uint32_t *inl, uint32_t *flags,
					   uint16_t *vlan_tci);
	/*
	 * poll_length_flags_mp_rq_cvlan - Poll one receive completion and provide
	 *				   the related message length,
	 *				   packet-offset, completion flags and
	 *				   CVLAN TCI
	 *
	 * The same as poll_length_flags_cvlan but:
	 *  - Without the inline-receive support.
	 *  - Also retrives offset in the WR posted buffer as defined by the
	 *    WR SG list. The start of the received packet is located in this
	 *    offset.
	 */
	int32_t (*poll_length_flags_mp_rq_cvlan)(struct ibv_cq *cq,
						 uint32_t *offset,
						 uint32_t *flags,
						 uint16_t *vlan_tci);
};

enum {
	IBV_EXP_NUM_DC_INFO_LIDS	= 30
};

struct ibv_exp_dc_info_ent {
	uint16_t	lid[IBV_EXP_NUM_DC_INFO_LIDS];
	uint32_t	seqnum;
};

enum ibv_exp_roce_gid_type {
	IBV_EXP_IB_ROCE_V1_GID_TYPE,
	IBV_EXP_ROCE_V2_GID_TYPE,
	IBV_EXP_ROCE_V1_5_GID_TYPE,
};

enum ibv_exp_query_gid_attr {
	IBV_EXP_QUERY_GID_ATTR_TYPE = (1 << 0),
	IBV_EXP_QUERY_GID_ATTR_GID	= (1 << 1),
	IBV_EXP_QUERY_GID_ATTR_RESERVED	= (1 << 2),
};

struct ibv_exp_gid_attr {
	uint32_t			comp_mask;
	enum ibv_exp_roce_gid_type	type;
	union ibv_gid			gid;
};

/**
 * enum ibv_exp_ec_calc_attr_comp_mask - erasure coding context
 *    init attributes compatibility enumeration
 */
enum ibv_exp_ec_calc_attr_comp_mask {
	IBV_EXP_EC_CALC_ATTR_MAX_INFLIGHT	= (1 << 0),
	IBV_EXP_EC_CALC_ATTR_K			= (1 << 1),
	IBV_EXP_EC_CALC_ATTR_M			= (1 << 2),
	IBV_EXP_EC_CALC_ATTR_W			= (1 << 3),
	IBV_EXP_EC_CALC_ATTR_MAX_DATA_SGE	= (1 << 4),
	IBV_EXP_EC_CALC_ATTR_MAX_CODE_SGE	= (1 << 5),
	IBV_EXP_EC_CALC_ATTR_ENCODE_MAT		= (1 << 6),
	IBV_EXP_EC_CALC_ATTR_AFFINITY		= (1 << 7),
	IBV_EXP_EC_CALC_ATTR_POLLING		= (1 << 8),
	IBV_EXP_EC_CALC_INIT_ATTR_RESERVED	= (1 << 9),
};

/**
 * struct ibv_exp_ec_calc_init_attr - erasure coding engine
 *     initialization attributes
 *
 * @comp_mask:            compatibility bitmask
 * @max_inflight_calcs:   maximum inflight calculations
 * @k:                    number of data blocks
 * @m:                    number of core blocks
 * @w:                    Galois field bits GF(2^w)
 * @max_data_sge:	  maximum data sg elements to be used for encode/decode
 * @max_code_sge:	  maximum code sg elements to be used for encode/decode
 * @encode_matrix:        buffer that contain the encoding matrix
 * @affinity_hint:        affinity hint for asynchronous calcs completion
 *                        steering.
 * @polling:              polling mode (if set no completions will be generated
 *                        by events).
 */
struct ibv_exp_ec_calc_init_attr {
	uint32_t		comp_mask;
	uint32_t		max_inflight_calcs;
	int			k;
	int			m;
	int			w;
	int			max_data_sge;
	int			max_code_sge;
	uint8_t			*encode_matrix;
	int			affinity_hint;
	int			polling;
};

/**
 * enum ibv_exp_ec_status - EX calculation status
 *
 * @IBV_EXP_EC_CALC_SUCCESS:   EC calc operation succeded
 * @IBV_EXP_EC_CALC_FAIL:      EC calc operation failed
 */
enum ibv_exp_ec_status {
	IBV_EXP_EC_CALC_SUCCESS,
	IBV_EXP_EC_CALC_FAIL,
};

/**
 * struct ibv_exp_ec_comp - completion context of EC calculation
 *
 * @done:      function handle of the EC calculation completion
 * @status:    status of the EC calculation
 *
 * The consumer is expected to embed this structure in his calculation context
 * so that the user context would be acquired back using offsetof()
 */
struct ibv_exp_ec_comp {
	void (*done)(struct ibv_exp_ec_comp *comp);
	enum ibv_exp_ec_status status;
};

/**
 * struct ibv_exp_ec_calc - erasure coding engine context
 *
 * @pd:                 protection domain
 */
struct ibv_exp_ec_calc {
	struct ibv_pd		*pd;
};

/**
 * struct ibv_exp_ec_mem - erasure coding memory layout context
 *
 * @data_blocks:        array of data sg elements
 * @num_data_sge:       number of data sg elements
 * @code_blocks:        array of code sg elements
 * @num_code_sge:       number of code sg elements
 * @block_size:         logical block size
 */
struct ibv_exp_ec_mem {
	struct ibv_sge		*data_blocks;
	int			num_data_sge;
	struct ibv_sge		*code_blocks;
	int			num_code_sge;
	int			block_size;
};

/**
 * struct ibv_exp_ec_stripe - erasure coding stripe descriptor
 *
 * @qp:    queue-pair connected to the relevant peer
 * @wr:    send work request, can either be a RDMA wr or a SEND
 */
struct ibv_exp_ec_stripe {
	struct ibv_qp		*qp;
	struct ibv_send_wr	*wr;
};

struct ibv_exp_peer_commit;
struct ibv_exp_rollback_ctx;


struct ibv_exp_peer_peek;
struct ibv_exp_peer_abort_peek;

struct verbs_context_exp {
	/*  "grows up" - new fields go here */
	int (*exp_peer_peek_cq)(struct ibv_cq *ibcq,
				struct ibv_exp_peer_peek *peek_ctx);
	int (*exp_peer_abort_peek_cq)(struct ibv_cq *ibcq,
				      struct ibv_exp_peer_abort_peek *ack_ctx);
	int (*exp_peer_commit_qp)(struct ibv_qp *qp,
				  struct ibv_exp_peer_commit *peer);
	int (*exp_rollback_send)(struct ibv_qp *qp,
				 struct ibv_exp_rollback_ctx *rollback);
	int (*ec_update_sync)(struct ibv_exp_ec_calc *calc,
			      struct ibv_exp_ec_mem *ec_mem,
			      uint8_t *data_updates,
			      uint8_t *code_updates);
	int (*ec_update_async)(struct ibv_exp_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       uint8_t *data_updates,
			       uint8_t *code_updates,
			       struct ibv_exp_ec_comp *ec_comp);
	struct ibv_exp_ec_calc *(*alloc_ec_calc)(struct ibv_pd *pd,
						 struct ibv_exp_ec_calc_init_attr *attr);
	void (*dealloc_ec_calc)(struct ibv_exp_ec_calc *calc);
	int (*ec_encode_async)(struct ibv_exp_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       struct ibv_exp_ec_comp *ec_comp);
	int (*ec_encode_sync)(struct ibv_exp_ec_calc *calc,
			      struct ibv_exp_ec_mem *ec_mem);
	int (*ec_decode_async)(struct ibv_exp_ec_calc *calc,
			       struct ibv_exp_ec_mem *ec_mem,
			       uint8_t *erasures,
			       uint8_t *decode_matrix,
			       struct ibv_exp_ec_comp *ec_comp);
	int (*ec_decode_sync)(struct ibv_exp_ec_calc *calc,
			      struct ibv_exp_ec_mem *ec_mem,
			      uint8_t *erasures,
			      uint8_t *decode_matrix);
	int (*ec_poll)(struct ibv_exp_ec_calc *calc, int n);
	int (*ec_encode_send)(struct ibv_exp_ec_calc *calc,
			      struct ibv_exp_ec_mem *ec_mem,
			      struct ibv_exp_ec_stripe *data_stripes,
			      struct ibv_exp_ec_stripe *code_stripes);
	int (*exp_query_gid_attr)(struct ibv_context *context, uint8_t port_num,
				  unsigned int index,
				  struct ibv_exp_gid_attr *attr);
	int (*exp_destroy_rwq_ind_table)(struct ibv_exp_rwq_ind_table *rwq_ind_table);
	struct ibv_exp_rwq_ind_table *(*exp_create_rwq_ind_table)(struct ibv_context *context,
								  struct ibv_exp_rwq_ind_table_init_attr *init_attr);
	int (*exp_destroy_wq)(struct ibv_exp_wq *wq);
	int (*exp_modify_wq)(struct ibv_exp_wq *wq,
				 struct ibv_exp_wq_attr *wq_attr);
	struct ibv_exp_wq * (*exp_create_wq)(struct ibv_context *context,
						 struct ibv_exp_wq_init_attr *wq_init_attr);
	int (*drv_exp_poll_dc_info)(struct ibv_context *context,
				    struct ibv_exp_dc_info_ent *ents,
				    int nent, int port);
	void *(*exp_query_intf)(struct ibv_context *context, struct ibv_exp_query_intf_params *params,
				enum ibv_exp_query_intf_status *status);
	int (*exp_release_intf)(struct ibv_context *context, void *intf,
				struct ibv_exp_release_intf_params *params);
	struct ibv_exp_res_domain *(*exp_create_res_domain)(struct ibv_context *context,
							    struct ibv_exp_res_domain_init_attr *attr);
	int (*exp_destroy_res_domain)(struct ibv_context *context,
				      struct ibv_exp_res_domain *res_dom,
				      struct ibv_exp_destroy_res_domain_attr *attr);
	int (*lib_exp_use_priv_env)(struct ibv_context *context);
	int (*lib_exp_setenv)(struct ibv_context *context, const char *name,
			      const char *value, int overwrite);
	struct verbs_environment *venv;
	int (*drv_exp_dereg_mr)(struct ibv_mr *mr, struct ibv_exp_dereg_out *out);
	int (*exp_rereg_mr)(struct ibv_mr *mr, int flags, struct ibv_pd *pd,
			    void *addr, size_t length, uint64_t access,
			    struct ibv_exp_rereg_mr_attr *attr);
	int (*drv_exp_rereg_mr)(struct ibv_mr *mr, int flags, struct ibv_pd *pd,
				void *addr, size_t length, uint64_t access,
				struct ibv_exp_rereg_mr_attr *attr, struct ibv_exp_rereg_out *out);
	int (*drv_exp_prefetch_mr)(struct ibv_mr *mr,
				   struct ibv_exp_prefetch_attr *attr);
	int (*lib_exp_prefetch_mr)(struct ibv_mr *mr,
				   struct ibv_exp_prefetch_attr *attr);
	struct ibv_exp_mkey_list_container * (*drv_exp_alloc_mkey_list_memory)(struct ibv_exp_mkey_list_container_attr *attr);
	struct ibv_exp_mkey_list_container * (*lib_exp_alloc_mkey_list_memory)(struct ibv_exp_mkey_list_container_attr *attr);
	int (*drv_exp_dealloc_mkey_list_memory)(struct ibv_exp_mkey_list_container *mem);
	int (*lib_exp_dealloc_mkey_list_memory)(struct ibv_exp_mkey_list_container *mem);
	int (*drv_exp_query_mkey)(struct ibv_mr *mr, struct ibv_exp_mkey_attr *mkey_attr);
	int (*lib_exp_query_mkey)(struct ibv_mr *mr, struct ibv_exp_mkey_attr *mkey_attr);
	struct ibv_mr * (*drv_exp_create_mr)(struct ibv_exp_create_mr_in *in);
	struct ibv_mr * (*lib_exp_create_mr)(struct ibv_exp_create_mr_in *in);
	int (*drv_exp_arm_dct)(struct ibv_exp_dct *dct, struct ibv_exp_arm_attr *attr);
	int (*lib_exp_arm_dct)(struct ibv_exp_dct *dct, struct ibv_exp_arm_attr *attr);
	int (*drv_exp_bind_mw)(struct ibv_exp_mw_bind *mw_bind);
	int (*lib_exp_bind_mw)(struct ibv_exp_mw_bind *mw_bind);
	int (*drv_exp_post_send)(struct ibv_qp *qp,
				 struct ibv_exp_send_wr *wr,
				 struct ibv_exp_send_wr **bad_wr);
	struct ibv_mr * (*drv_exp_reg_mr)(struct ibv_exp_reg_mr_in *in);
	struct ibv_mr * (*lib_exp_reg_mr)(struct ibv_exp_reg_mr_in *in);
	struct ibv_ah * (*drv_exp_ibv_create_ah)(struct ibv_pd *pd,
						 struct ibv_exp_ah_attr *attr_exp);
	int (*drv_exp_query_values)(struct ibv_context *context, int q_values,
				    struct ibv_exp_values *values);
	struct ibv_cq * (*exp_create_cq)(struct ibv_context *context, int cqe,
					 struct ibv_comp_channel *channel,
					 int comp_vector, struct ibv_exp_cq_init_attr *attr);
	int (*drv_exp_ibv_poll_cq)(struct ibv_cq *ibcq, int num_entries,
				   struct ibv_exp_wc *wc, uint32_t wc_size);
	void * (*drv_exp_get_legacy_xrc) (struct ibv_srq *ibv_srq);
	void (*drv_exp_set_legacy_xrc) (struct ibv_srq *ibv_srq, void *legacy_xrc);
	struct ibv_mr * (*drv_exp_ibv_reg_shared_mr)(struct ibv_exp_reg_shared_mr_in *in);
	struct ibv_mr * (*lib_exp_ibv_reg_shared_mr)(struct ibv_exp_reg_shared_mr_in *in);
	int (*drv_exp_modify_qp)(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
				 uint64_t exp_attr_mask);
	int (*lib_exp_modify_qp)(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
				 uint64_t exp_attr_mask);
	int (*drv_exp_post_task)(struct ibv_context *context,
				 struct ibv_exp_task *task,
				 struct ibv_exp_task **bad_task);
	int (*lib_exp_post_task)(struct ibv_context *context,
				 struct ibv_exp_task *task,
				 struct ibv_exp_task **bad_task);
	int (*drv_exp_modify_cq)(struct ibv_cq *cq,
				 struct ibv_exp_cq_attr *attr, int attr_mask);
	int (*lib_exp_modify_cq)(struct ibv_cq *cq,
				 struct ibv_exp_cq_attr *attr, int attr_mask);
	int (*drv_exp_ibv_destroy_flow) (struct ibv_exp_flow *flow);
	int (*lib_exp_ibv_destroy_flow) (struct ibv_exp_flow *flow);
	struct ibv_exp_flow * (*drv_exp_ibv_create_flow) (struct ibv_qp *qp,
						      struct ibv_exp_flow_attr
						      *flow_attr);
	struct ibv_exp_flow * (*lib_exp_ibv_create_flow) (struct ibv_qp *qp,
							  struct ibv_exp_flow_attr
							  *flow_attr);

	int (*drv_exp_query_port)(struct ibv_context *context, uint8_t port_num,
				  struct ibv_exp_port_attr *port_attr);
	int (*lib_exp_query_port)(struct ibv_context *context, uint8_t port_num,
				  struct ibv_exp_port_attr *port_attr);
	struct ibv_exp_dct *(*create_dct)(struct ibv_context *context,
					  struct ibv_exp_dct_init_attr *attr);
	int (*destroy_dct)(struct ibv_exp_dct *dct);
	int (*query_dct)(struct ibv_exp_dct *dct, struct ibv_exp_dct_attr *attr);
	int (*drv_exp_query_device)(struct ibv_context *context,
				    struct ibv_exp_device_attr *attr);
	int (*lib_exp_query_device)(struct ibv_context *context,
				    struct ibv_exp_device_attr *attr);
	struct ibv_qp *(*drv_exp_create_qp)(struct ibv_context *context,
					    struct ibv_exp_qp_init_attr *init_attr);
	struct ibv_qp *(*lib_exp_create_qp)(struct ibv_context *context,
					    struct ibv_exp_qp_init_attr *init_attr);
	size_t sz;	/* Set by library on struct allocation,	*/
			/* must be located as last field	*/
};


static inline struct verbs_context_exp *verbs_get_exp_ctx(struct ibv_context *ctx)
{
	struct verbs_context *app_ex_ctx = verbs_get_ctx(ctx);
	char *actual_ex_ctx;

	if (!app_ex_ctx || !(app_ex_ctx->has_comp_mask & VERBS_CONTEXT_EXP))
		return NULL;

	actual_ex_ctx = ((char *)ctx) - (app_ex_ctx->sz - sizeof(struct ibv_context));

	return (struct verbs_context_exp *)(actual_ex_ctx - sizeof(struct verbs_context_exp));
}

#define verbs_get_exp_ctx_op(ctx, op) ({ \
	struct verbs_context_exp *_vctx = verbs_get_exp_ctx(ctx); \
	(!_vctx || (_vctx->sz < sizeof(*_vctx) - offsetof(struct verbs_context_exp, op)) || \
	!_vctx->op) ? NULL : _vctx; })

#define verbs_set_exp_ctx_op(_vctx, op, ptr) ({ \
	struct verbs_context_exp *vctx = _vctx; \
	if (vctx && (vctx->sz >= sizeof(*vctx) - offsetof(struct verbs_context_exp, op))) \
		vctx->op = ptr; })


/*
 * ibv_exp_alloc_ec_calc() - allocate an erasure coding
 *     calculation offload context
 * @pd:          user allocated protection domain
 * @attrs:       initialization attributes
 *
 * Returns handle handle to the EC calculation APIs
 */
static inline struct ibv_exp_ec_calc *
ibv_exp_alloc_ec_calc(struct ibv_pd *pd,
		      struct ibv_exp_ec_calc_init_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(pd->context, alloc_ec_calc);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(attr->comp_mask,
					      IBV_EXP_EC_CALC_INIT_ATTR_RESERVED - 1);

	return vctx->alloc_ec_calc(pd, attr);
}

/*
 * ibv_exp_dealloc_ec_calc() - free an erasure coding
 *     calculation offload context
 * @ec_calc:       ec context
 */
static inline void ibv_exp_dealloc_ec_calc(struct ibv_exp_ec_calc *calc)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, dealloc_ec_calc);
	if (!vctx) {
		errno = ENOSYS;
		return;
	}

	vctx->dealloc_ec_calc(calc);
}

/**
 * ibv_exp_ec_encode_async() - asynchronous encode of given data blocks
 *    and place in code_blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 * @ec_comp:          EC calculation completion context
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout, the total length of the sg elements must satisfy
 *   k * ec_mem.block_size.
 * - ec_mem.num_data_sg must not exceed the calc max_data_sge
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   m * ec_mem.block_size.
 * - ec_mem.num_code_sg must not exceed the calc max_code_sge
 *
 * Notes:
 * The ec_calc will perform the erasure coding calc operation,
 * once it completes, it will call ec_comp->done() handle.
 * The caller will take it from there.
 */
static inline int
ibv_exp_ec_encode_async(struct ibv_exp_ec_calc *calc,
			struct ibv_exp_ec_mem *ec_mem,
			struct ibv_exp_ec_comp *ec_comp)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_encode_async);
	if (!vctx)
		return ENOSYS;

	return vctx->ec_encode_async(calc, ec_mem, ec_comp);
}

/**
 * ibv_exp_ec_encode_sync() - synchronous encode of given data blocks
 *    and place in code_blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout, the total length of the sg elements must satisfy
 *   k * ec_mem.block_size.
 * - ec_mem.num_data_sg must not exceed the calc max_data_sge
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   m * ec_mem.block_size.
 * - ec_mem.num_code_sg must not exceed the calc max_code_sge
 *
 * Returns 0 on success, non-zero on failure.
 *
 */
static inline int
ibv_exp_ec_encode_sync(struct ibv_exp_ec_calc *calc,
		       struct ibv_exp_ec_mem *ec_mem)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_encode_sync);
	if (!vctx)
		return ENOSYS;

	return vctx->ec_encode_sync(calc, ec_mem);
}

/**
 * ibv_exp_ec_decode_async() - decode a given set of data blocks
 *    and code_blocks and place into output recovery blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 * @erasures:         pointer to byte-map of which blocks were erased
 * 		      and needs to be recovered
 * @decode_matrix:    buffer that contains the decode matrix
 * @ec_comp:          EC calculation completion context
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout, the total length of the sg elements must satisfy
 *   k * ec_mem.block_size.
 * - ec_mem.num_data_sg must not exceed the calc max_data_sge
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   number of missing blocks * ec_mem.block_size.
 * - ec_mem.num_code_sg must not exceed the calc max_code_sge
 * - erasures byte-mask consists of the survived and erased blocks.
 *   The first k bytes stand for the k data blocks followed by
 *   m bytes that stand for the code blocks.
 *
 * Returns 0 on success, or non-zero on failure with a corresponding
 * errno.
 *
 * Notes:
 * The ec_calc will perform the erasure coding calc operation,
 * once it completes, it will call ec_comp->done() handle.
 * The caller will take it from there
 */
static inline int
ibv_exp_ec_decode_async(struct ibv_exp_ec_calc *calc,
			struct ibv_exp_ec_mem *ec_mem,
			uint8_t *erasures,
			uint8_t *decode_matrix,
			struct ibv_exp_ec_comp *ec_comp)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_decode_async);
	if (!vctx)
		return ENOSYS;

	return vctx->ec_decode_async(calc, ec_mem, erasures,
				     decode_matrix, ec_comp);
}

/**
 * ibv_exp_ec_decode_sync() - decode a given set of data blocks
 *    and code_blocks and place into output recovery blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 * @erasures:         pointer to byte-map of which blocks were erased
 * 		      and needs to be recovered
 * @decode_matrix:    registered buffer of the decode matrix
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout, the total length of the sg elements must satisfy
 *   k * ec_mem.block_size.
 * - ec_mem.num_data_sg must not exceed the calc max_data_sge
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   number of missing blocks * ec_mem.block_size.
 * - ec_mem.num_code_sg must not exceed the calc max_code_sge
 * - erasures byte-map consists of the survived and erased blocks.
 *   The first k bytes stand for the k data blocks followed by
 *   m bytes that stand for the code blocks.
 *
 * Returns 0 on success, or non-zero on failure with a corresponding
 * errno.
 */
static inline int
ibv_exp_ec_decode_sync(struct ibv_exp_ec_calc *calc,
		       struct ibv_exp_ec_mem *ec_mem,
		       uint8_t *erasures,
		       uint8_t *decode_matrix)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_decode_sync);
	if (!vctx)
		return ENOSYS;

	return vctx->ec_decode_sync(calc, ec_mem, erasures, decode_matrix);
}

/**
 * ibv_exp_ec_update_async() - copmutes redundancies based on updated blocks,
 *    their replacements and old redundancies and place into output code blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 * @data_updates:     array which is a map of data blocks that are updated
 * @code_updates:     array which is a map of code blocks to be computed
 * @ec_comp:          EC calculation completion context
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout in the following way:
 *   assume we want to update blocks d_i and d_j with i<j,
 *   then sg enries should be as follows:
 *   c_0 ... c_m d_i d'_i d_j d'_j
 *   were c_0 ... c_m are all previous redundancies,
 *   d_i is an original i-th block, d'_i is new i-th block
 * - ec_mem.num_data_sg should be equal to the number of sg entries,
 *   i.e. to num of code blocks to be updated + 2*num of updates
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   number of overall code blocks to be updated.
 * - ec_mem.num_code_sg must be equal to the number of code blocks
 *   to be updated and not to exceed the calc max_code_sge
 * - data_updates is an array of size k (=number of data blocks)
 *   and is a byte-map for blocks to be updated, i.e
 *   if we want to update i-th block and do not want to update j-th block,
 *   then data_updates[i]=1 and data_updates[j]=0.
 * - code_updates is an array of size m(=number of code blocks)
 *   and is a byte-map of code blocks that should be computed, i.e
 *   if we want to compute i-th block and do not want to compute j-th block,
 *   then code_updates[i]=1 and code_updates[j]=0.
 *
 * Returns 0 on success, or non-zero on failure with a corresponding
 * errno.
 */

static inline int
ibv_exp_ec_update_async(struct ibv_exp_ec_calc *calc,
			struct ibv_exp_ec_mem *ec_mem,
			uint8_t *data_updates,
			uint8_t *code_updates,
			struct ibv_exp_ec_comp *ec_comp)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_update_async);
	if (!vctx)
		return -ENOSYS;

	return vctx->ec_update_async(calc, ec_mem, data_updates,
				     code_updates, ec_comp);
}

/**
 * ibv_exp_ec_update_sync() - copmutes redundancies based on updated blocks,
 *    their replacements and old redundancies and place into output code blocks
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout
 * @data_updates:     array which is a map of data blocks that are updated
 * @code_updates:     array which is a map of code blocks to be computed
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout in the following way:
 *   assume we want to update blocks d_i and d_j with i<j,
 *   then enries of sg should be as follows:
 *   c_0..c_m d_i d'_i d_j d'_j
 *   were c_0 .. c_m are previous redundancies,
 *   d_i is an original i-th block, d'_i is new i-th block
 * - ec_mem.num_data_sg should be equal to the number of sg entries,
 *   i.e. to num of code blocks to be updated + 2*num of updates
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   number of overall code blocks to be updated.
 * - ec_mem.num_code_sg must be equal to the number of code blocks
 *   to be updated and not to exceed the calc max_code_sge
 * - data_updates is an array of size k (=number of data blocks)
 *   and is a byte-map for blocks to be updated, i.e
 *   if we want to update i-th block and do not want to update j-th block,
 *   then data_updates[i]=1 and data_updates[j]=0.
 * - code_updates is an array of size m(=number of code blocks)
 *   and is a bytemap of code blocks that should be computed, i.e
 *   if we want to compute i-th block and do not want to compute j-th block,
 *   then code_updates[i]=1 and code_updates[j]=0.
 *
 * Returns 0 on success, or non-zero on failure with a corresponding
 * errno.
 */

static inline int
ibv_exp_ec_update_sync(struct ibv_exp_ec_calc *calc,
		       struct ibv_exp_ec_mem *ec_mem,
		       uint8_t *data_updates,
		       uint8_t *code_updates)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_update_sync);
	if (!vctx)
		return -ENOSYS;

	return vctx->ec_update_sync(calc, ec_mem, data_updates, code_updates);
}
/**
 * ibv_exp_ec_calc_poll() - poll for EC calculation
 *
 * @calc: EC calc context
 * @n: number of calculations to poll
 *
 * Returns the number of calc completions processed which
 * is lower or equal to n. Relevant only when EC calc context
 * was allocated in polling mode.
 */
static inline int
ibv_exp_ec_poll(struct ibv_exp_ec_calc *calc, int n)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_poll);
	if (!vctx)
		return ENOSYS;

	return vctx->ec_poll(calc, n);
}

/**
 * ibv_exp_ec_encode_send() - encode a given data blocks
 *    initiate the data and code blocks transfers to the wire with the qps array.
 * @ec_calc:          erasure coding calculation engine
 * @ec_mem:           erasure coding memory layout context
 * @data_stripes:     array of stripe handles, each represents a data block channel
 * @code_stripes:     array of qp handles, each represents a code block channel
 *
 * Restrictions:
 * - ec_calc is an initialized erasure coding calc engine structure
 * - ec_mem.data_blocks sg array must describe the data memory
 *   layout, the total length of the sg elements must satisfy
 *   k * ec_mem.block_size.
 * - ec_mem.num_data_sg must not exceed the calc max_data_sge
 * - ec_mem.code_blocks sg array must describe the code memory
 *   layout, the total length of the sg elements must satisfy
 *   m * ec_mem.block_size.
 * - ec_mem.num_code_sg must not exceed the calc max_code_sge
 *
 * Returns 0 on success, or non-zero on failure with a corresponding
 * errno.
 */
static inline int
ibv_exp_ec_encode_send(struct ibv_exp_ec_calc *calc,
		       struct ibv_exp_ec_mem *ec_mem,
		       struct ibv_exp_ec_stripe *data_stripes,
		       struct ibv_exp_ec_stripe *code_stripes)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(calc->pd->context, ec_encode_send);
	if (!vctx)
		return -ENOSYS;

	return vctx->ec_encode_send(calc, ec_mem, data_stripes, code_stripes);
}

static inline struct ibv_qp *
ibv_exp_create_qp(struct ibv_context *context, struct ibv_exp_qp_init_attr *qp_init_attr)
{
	struct verbs_context_exp *vctx;
	uint32_t mask = qp_init_attr->comp_mask;

	if (mask == IBV_EXP_QP_INIT_ATTR_PD)
		return ibv_create_qp(qp_init_attr->pd,
				     (struct ibv_qp_init_attr *) qp_init_attr);

	vctx = verbs_get_exp_ctx_op(context, lib_exp_create_qp);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(qp_init_attr->comp_mask,
					      IBV_EXP_QP_INIT_ATTR_RESERVED1 - 1);

	return vctx->lib_exp_create_qp(context, qp_init_attr);
}

/*
 * ibv_exp_use_priv_env
 *
 * switch to use private environment
 */
static inline int ibv_exp_use_priv_env(struct ibv_context *context)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, lib_exp_use_priv_env);
	if (!vctx) {
		errno = ENOSYS;
		return -1;
	}

	return vctx->lib_exp_use_priv_env(context);
}

/*
 * ibv_exp_poll_dc_info
 *
 * The function is not thread safe. Any locking must be done by the user.
 *
 * Return: >= 0 number of returned entries
 *	   < 0 error
 *
 */
static inline int ibv_exp_poll_dc_info(struct ibv_context *context,
				       struct ibv_exp_dc_info_ent *ents,
				       int nent, int port)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, drv_exp_poll_dc_info);
	if (!vctx) {
		errno = ENOSYS;
		return -1;
	}

	return vctx->drv_exp_poll_dc_info(context, ents, nent, port);
}

/*
 * ibv_exp_setenv
 *
 * see man setenv for parameter description
 */
static inline int ibv_exp_setenv(struct ibv_context *context,
				 const char *name,
				 const char *value,
				 int overwrite)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, lib_exp_setenv);
	if (!vctx)
		return setenv(name, value, overwrite);

	return vctx->lib_exp_setenv(context, name, value, overwrite);
}

static inline int ibv_exp_query_device(struct ibv_context *context,
				       struct ibv_exp_device_attr *attr)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(context,
							      lib_exp_query_device);
	if (!vctx)
		return ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_DEVICE_ATTR_RESERVED - 1);
	return vctx->lib_exp_query_device(context, attr);
}

static inline struct ibv_exp_dct *
ibv_exp_create_dct(struct ibv_context *context,
		   struct ibv_exp_dct_init_attr *attr)
{
	struct verbs_context_exp *vctx;
	struct ibv_exp_dct *dct;

	vctx = verbs_get_exp_ctx_op(context, create_dct);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(attr->comp_mask,
					      IBV_EXP_DCT_INIT_ATTR_RESERVED - 1);
	pthread_mutex_lock(&context->mutex);
	dct = vctx->create_dct(context, attr);
	if (dct)
		dct->context = context;

	pthread_mutex_unlock(&context->mutex);

	return dct;
}

static inline int ibv_exp_destroy_dct(struct ibv_exp_dct *dct)
{
	struct verbs_context_exp *vctx;
	struct ibv_context *context = dct->context;
	int err;

	vctx = verbs_get_exp_ctx_op(context, destroy_dct);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}

	pthread_mutex_lock(&context->mutex);
	err = vctx->destroy_dct(dct);
	pthread_mutex_unlock(&context->mutex);

	return err;
}

static inline int ibv_exp_query_dct(struct ibv_exp_dct *dct,
				    struct ibv_exp_dct_attr *attr)
{
	struct verbs_context_exp *vctx;
	struct ibv_context *context = dct->context;
	int err;

	vctx = verbs_get_exp_ctx_op(context, query_dct);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_DCT_ATTR_RESERVED - 1);
	pthread_mutex_lock(&context->mutex);
	err = vctx->query_dct(dct, attr);
	pthread_mutex_unlock(&context->mutex);

	return err;
}

static inline int ibv_exp_arm_dct(struct ibv_exp_dct *dct,
				  struct ibv_exp_arm_attr *attr)
{
	struct verbs_context_exp *vctx;
	struct ibv_context *context = dct->context;
	int err;

	vctx = verbs_get_exp_ctx_op(context, lib_exp_arm_dct);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_ARM_ATTR_RESERVED - 1);
	pthread_mutex_lock(&context->mutex);
	err = vctx->lib_exp_arm_dct(dct, attr);
	pthread_mutex_unlock(&context->mutex);

	return err;
}

static inline int ibv_exp_query_port(struct ibv_context *context,
				     uint8_t port_num,
				     struct ibv_exp_port_attr *port_attr)
{
	struct verbs_context_exp *vctx;

	if (0 == port_attr->comp_mask)
		return ibv_query_port(context, port_num,
				      &port_attr->port_attr);

	/* Check that only valid flags were given */
	if ((!port_attr->comp_mask & IBV_EXP_QUERY_PORT_ATTR_MASK1) ||
	    (port_attr->comp_mask & ~IBV_EXP_QUERY_PORT_ATTR_MASKS) ||
	    (port_attr->mask1 & ~IBV_EXP_QUERY_PORT_MASK)) {
		errno = EINVAL;
		return -errno;
	}

	vctx = verbs_get_exp_ctx_op(context, lib_exp_query_port);

	if (!vctx) {
		/* Fallback to legacy mode */
		if (port_attr->comp_mask == IBV_EXP_QUERY_PORT_ATTR_MASK1 &&
		    !(port_attr->mask1 & ~IBV_EXP_QUERY_PORT_STD_MASK))
			return ibv_query_port(context, port_num,
					      &port_attr->port_attr);

		/* Unsupported field was requested */
		errno = ENOSYS;
		return -errno;
	}
	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(port_attr->comp_mask,
						IBV_EXP_QUERY_PORT_ATTR_RESERVED - 1);

	return vctx->lib_exp_query_port(context, port_num, port_attr);
}

/**
 * ibv_exp_post_task - Post a list of tasks to different QPs.
 */
static inline int ibv_exp_post_task(struct ibv_context *context,
				    struct ibv_exp_task *task,
				    struct ibv_exp_task **bad_task)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(context,
							      lib_exp_post_task);
	if (!vctx)
		return ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(task->comp_mask,
						IBV_EXP_TASK_RESERVED - 1);

	return vctx->lib_exp_post_task(context, task, bad_task);
}

static inline int ibv_exp_query_values(struct ibv_context *context, int q_values,
				       struct ibv_exp_values *values)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(context,
							      drv_exp_query_values);
	if (!vctx) {
		errno = ENOSYS;
		return -errno;
	}
	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(values->comp_mask,
						IBV_EXP_VALUES_RESERVED - 1);

	return vctx->drv_exp_query_values(context, q_values, values);
}

static inline struct ibv_exp_flow *ibv_exp_create_flow(struct ibv_qp *qp,
						       struct ibv_exp_flow_attr *flow)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(qp->context,
							      lib_exp_ibv_create_flow);
	if (!vctx || !vctx->lib_exp_ibv_create_flow)
		return NULL;

	if (flow->reserved != 0L) {
		fprintf(stderr, "%s:%d: flow->reserved must be 0\n", __FUNCTION__, __LINE__);
		flow->reserved = 0L;
	}

	return vctx->lib_exp_ibv_create_flow(qp, flow);
}

static inline int ibv_exp_destroy_flow(struct ibv_exp_flow *flow_id)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(flow_id->context,
							      lib_exp_ibv_destroy_flow);
	if (!vctx || !vctx->lib_exp_ibv_destroy_flow)
		return -ENOSYS;

	return vctx->lib_exp_ibv_destroy_flow(flow_id);
}

static inline int ibv_exp_poll_cq(struct ibv_cq *ibcq, int num_entries,
				  struct ibv_exp_wc *wc, uint32_t wc_size)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(ibcq->context,
							      drv_exp_ibv_poll_cq);
	if (!vctx)
		return -ENOSYS;

	return vctx->drv_exp_ibv_poll_cq(ibcq, num_entries, wc, wc_size);
}

/**
 * ibv_exp_post_send - Post a list of work requests to a send queue.
 */
static inline int ibv_exp_post_send(struct ibv_qp *qp,
				    struct ibv_exp_send_wr *wr,
				    struct ibv_exp_send_wr **bad_wr)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(qp->context,
							      drv_exp_post_send);
	if (!vctx)
		return -ENOSYS;

	return vctx->drv_exp_post_send(qp, wr, bad_wr);
}

/**
 * ibv_exp_reg_shared_mr - Register to an existing shared memory region
 * @in - Experimental register shared MR input data.
 */
static inline struct ibv_mr *ibv_exp_reg_shared_mr(struct ibv_exp_reg_shared_mr_in *mr_in)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(mr_in->pd->context,
							      lib_exp_ibv_reg_shared_mr);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(mr_in->comp_mask,
					      IBV_EXP_REG_SHARED_MR_RESERVED - 1);

	return vctx->lib_exp_ibv_reg_shared_mr(mr_in);
}

/**
 * ibv_exp_modify_cq - Modifies the attributes for the specified CQ.
 * @cq: The CQ to modify.
 * @cq_attr: Specifies the CQ attributes to modify.
 * @cq_attr_mask: A bit-mask used to specify which attributes of the CQ
 *   are being modified.
 */
static inline int ibv_exp_modify_cq(struct ibv_cq *cq,
				    struct ibv_exp_cq_attr *cq_attr,
				    int cq_attr_mask)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(cq->context,
							      lib_exp_modify_cq);
	if (!vctx)
		return ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(cq_attr->comp_mask,
						IBV_EXP_CQ_ATTR_RESERVED - 1);

	return vctx->lib_exp_modify_cq(cq, cq_attr, cq_attr_mask);
}

static inline struct ibv_cq *ibv_exp_create_cq(struct ibv_context *context,
					       int cqe,
					       void *cq_context,
					       struct ibv_comp_channel *channel,
					       int comp_vector,
					       struct ibv_exp_cq_init_attr *attr)
{
	struct verbs_context_exp *vctx;
	struct ibv_cq *cq;

	vctx = verbs_get_exp_ctx_op(context, exp_create_cq);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(attr->comp_mask,
					      IBV_EXP_CQ_INIT_ATTR_RESERVED1 - 1);
	pthread_mutex_lock(&context->mutex);
	cq = vctx->exp_create_cq(context, cqe, channel, comp_vector, attr);
	if (cq) {
		cq->context		   = context;
		cq->channel		   = channel;
		if (channel)
			++channel->refcnt;
		cq->cq_context		   = cq_context;
		cq->comp_events_completed  = 0;
		cq->async_events_completed = 0;
		pthread_mutex_init(&cq->mutex, NULL);
		pthread_cond_init(&cq->cond, NULL);
	}

	pthread_mutex_unlock(&context->mutex);

	return cq;
}

/**
 * ibv_exp_modify_qp - Modify a queue pair.
 * The argument exp_attr_mask specifies the QP attributes to be modified.
 * Use ibv_exp_qp_attr_mask for this argument.
 */
static inline int
ibv_exp_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr, uint64_t exp_attr_mask)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(qp->context, lib_exp_modify_qp);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}
	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_QP_ATTR_RESERVED - 1);

	return vctx->lib_exp_modify_qp(qp, attr, exp_attr_mask);
}

/**
 * ibv_exp_reg_mr - Register a memory region
 */
static inline struct ibv_mr *ibv_exp_reg_mr(struct ibv_exp_reg_mr_in *in)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(in->pd->context, lib_exp_reg_mr);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}
	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(in->comp_mask,
					      IBV_EXP_REG_MR_RESERVED - 1);

	return vctx->lib_exp_reg_mr(in);
}


/**
 * ibv_exp_bind_mw - Bind a memory window to a region
 */
static inline int ibv_exp_bind_mw(struct ibv_exp_mw_bind *mw_bind)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(mw_bind->mw->context, lib_exp_bind_mw);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}
	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(mw_bind->comp_mask,
						IBV_EXP_BIND_MW_RESERVED - 1);

	return vctx->lib_exp_bind_mw(mw_bind);
}

/**
 * ibv_exp_prefetch_mr - Prefetch part of a memory region.
 *
 * Can be used only with MRs registered with IBV_EXP_ACCESS_ON_DEMAND
 *
 * Returns 0 on success,
 * - ENOSYS libibverbs or provider driver doesn't support the prefetching verb.
 * - EFAULT when the range requested is out of the memory region bounds, or when
 *   parts of it are not part of the process address space.
 * - EINVAL when the MR is invalid.
 */
static inline int ibv_exp_prefetch_mr(
		struct ibv_mr *mr,
		struct ibv_exp_prefetch_attr *attr)
{
	struct verbs_context_exp *vctx = verbs_get_exp_ctx_op(mr->context,
			lib_exp_prefetch_mr);

	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}
	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_PREFETCH_MR_RESERVED - 1);

	return vctx->lib_exp_prefetch_mr(mr, attr);
}

typedef int (*drv_exp_post_send_func)(struct ibv_qp *qp,
				 struct ibv_exp_send_wr *wr,
				 struct ibv_exp_send_wr **bad_wr);
typedef int (*drv_post_send_func)(struct ibv_qp *qp, struct ibv_send_wr *wr,
				struct ibv_send_wr **bad_wr);
typedef int (*drv_exp_poll_cq_func)(struct ibv_cq *ibcq, int num_entries,
				   struct ibv_exp_wc *wc, uint32_t wc_size);
typedef int (*drv_poll_cq_func)(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc);
typedef int (*drv_post_recv_func)(struct ibv_qp *qp, struct ibv_recv_wr *wr,
			 struct ibv_recv_wr **bad_wr);

static inline void *ibv_exp_get_provider_func(struct ibv_context *context,
						enum ibv_exp_func_name name)
{
	struct verbs_context_exp *vctx;

	switch (name) {
	case IBV_EXP_POST_SEND_FUNC:
		vctx = verbs_get_exp_ctx_op(context, drv_exp_post_send);
		if (!vctx)
			goto error;

		return (void *)vctx->drv_exp_post_send;

	case IBV_EXP_POLL_CQ_FUNC:
		vctx = verbs_get_exp_ctx_op(context, drv_exp_ibv_poll_cq);
		if (!vctx)
			goto error;

		return (void *)vctx->drv_exp_ibv_poll_cq;

	case IBV_POST_SEND_FUNC:
		if (!context->ops.post_send)
			goto error;

		return (void *)context->ops.post_send;

	case IBV_POLL_CQ_FUNC:
		if (!context->ops.poll_cq)
			goto error;

		return (void *)context->ops.poll_cq;

	case IBV_POST_RECV_FUNC:
		if (!context->ops.post_recv)
			goto error;

		return (void *)context->ops.post_recv;

	default:
		break;
	}

error:
	errno = ENOSYS;
	return NULL;
}

static inline struct ibv_mr *ibv_exp_create_mr(struct ibv_exp_create_mr_in *in)
{
	struct verbs_context_exp *vctx;
	struct ibv_mr *mr;

	vctx = verbs_get_exp_ctx_op(in->pd->context, lib_exp_create_mr);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(in->comp_mask,
					      IBV_EXP_CREATE_MR_IN_RESERVED - 1);
	mr = vctx->lib_exp_create_mr(in);
	if (mr)
		mr->pd = in->pd;

	return mr;
}

static inline int ibv_exp_query_mkey(struct ibv_mr *mr,
				     struct ibv_exp_mkey_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(mr->context, lib_exp_query_mkey);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_MKEY_ATTR_RESERVED - 1);

	return vctx->lib_exp_query_mkey(mr, attr);
}

static inline int ibv_exp_dealloc_mkey_list_memory(struct ibv_exp_mkey_list_container *mem)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(mem->context,
				    lib_exp_dealloc_mkey_list_memory);
	if (!vctx) {
		errno = ENOSYS;
		return errno;
	}

	return vctx->lib_exp_dealloc_mkey_list_memory(mem);
}

static inline struct ibv_exp_mkey_list_container *
ibv_exp_alloc_mkey_list_memory(struct ibv_exp_mkey_list_container_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(attr->pd->context,
				    lib_exp_alloc_mkey_list_memory);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(attr->comp_mask,
					      IBV_EXP_MKEY_LIST_CONTAINER_RESERVED - 1);

	return vctx->lib_exp_alloc_mkey_list_memory(attr);
}

/**
 * ibv_rereg_mr - Re-Register a memory region
 *
 * For exp_access use ibv_exp_access_flags
 */
static inline int ibv_exp_rereg_mr(struct ibv_mr *mr, int flags,
				   struct ibv_pd *pd, void *addr,
				   size_t length, uint64_t exp_access,
				   struct ibv_exp_rereg_mr_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(mr->context, exp_rereg_mr);
	if (!vctx)
		return errno = ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_REREG_MR_ATTR_RESERVED - 1);

	return vctx->exp_rereg_mr(mr, flags, pd, addr, length, exp_access, attr);
}

/**
 * ibv_exp_create_res_domain - create resource domain
 */
static inline struct ibv_exp_res_domain *ibv_exp_create_res_domain(struct ibv_context *context,
								   struct ibv_exp_res_domain_init_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_create_res_domain);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(attr->comp_mask,
					      IBV_EXP_RES_DOMAIN_RESERVED - 1);

	return vctx->exp_create_res_domain(context, attr);
}

/**
 * ibv_exp_destroy_res_domain - destroy resource domain
 */
static inline int ibv_exp_destroy_res_domain(struct ibv_context *context,
					     struct ibv_exp_res_domain *res_dom,
					     struct ibv_exp_destroy_res_domain_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_destroy_res_domain);
	if (!vctx)
		return errno = ENOSYS;

	if (attr)
		IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
							IBV_EXP_DESTROY_RES_DOMAIN_RESERVED - 1);

	return vctx->exp_destroy_res_domain(context, res_dom, attr);
}

/**
 * ibv_exp_query_intf - query for family of verbs interface for specific QP/CQ
 *
 * Usually family of data-path verbs.
 * Application may call ibv_exp_query_intf for QPs in the following states:
 *    IBV_QPS_INIT, IBV_QPS_RTR and IBV_QPS_RTS
 *
 * Returns the family of verbs.
 * On failure returns NULL. The failure reason provided by the 'status'
 * output variable.
 */
static inline void *ibv_exp_query_intf(struct ibv_context *context,
				       struct ibv_exp_query_intf_params *params,
				       enum ibv_exp_query_intf_status *status)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_query_intf);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(params->comp_mask,
					      IBV_EXP_QUERY_INTF_RESERVED - 1);

	return vctx->exp_query_intf(context, params, status);
}

/**
 * ibv_exp_release_intf - release the queried interface
 */
static inline int ibv_exp_release_intf(struct ibv_context *context, void *intf,
				       struct ibv_exp_release_intf_params *params)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_release_intf);
	if (!vctx)
		return errno = ENOSYS;

	if (params)
		IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(params->comp_mask,
							IBV_EXP_RELEASE_INTF_RESERVED - 1);

	return vctx->exp_release_intf(context, intf, params);
}

static inline struct ibv_exp_wq *ibv_exp_create_wq(struct ibv_context *context,
						   struct ibv_exp_wq_init_attr *wq_init_attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_create_wq);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(wq_init_attr->comp_mask,
							IBV_EXP_CREATE_WQ_RESERVED - 1);

	return vctx->exp_create_wq(context, wq_init_attr);
}

static inline int ibv_exp_modify_wq(struct ibv_exp_wq *wq, struct ibv_exp_wq_attr *wq_attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(wq->context, exp_modify_wq);
	if (!vctx)
		return ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(wq_attr->attr_mask,
							IBV_EXP_WQ_ATTR_RESERVED - 1);
	return vctx->exp_modify_wq(wq, wq_attr);
}

static inline int ibv_exp_destroy_wq(struct ibv_exp_wq *wq)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(wq->context, exp_destroy_wq);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_destroy_wq(wq);
}

/*
 * ibv_exp_create_rwq_ind_table - Creates a RQ Indirection Table associated
 * with the specified protection domain.
 * @pd: The protection domain associated with the Indirection Table.
 * @ibv_exp_rwq_ind_table_init_attr: A list of initial attributes required to
 * create the Indirection Table.
 * Return Value
 * ibv_exp_create_rwq_ind_table returns a pointer to the created
 * Indirection Table, or NULL if the request fails.
 */
static inline struct ibv_exp_rwq_ind_table *ibv_exp_create_rwq_ind_table(struct ibv_context *context,
									 struct ibv_exp_rwq_ind_table_init_attr *init_attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_create_rwq_ind_table);
	if (!vctx) {
		errno = ENOSYS;
		return NULL;
	}

	IBV_EXP_RET_NULL_ON_INVALID_COMP_MASK(init_attr->comp_mask,
					      IBV_EXP_CREATE_IND_TABLE_RESERVED - 1);
	return vctx->exp_create_rwq_ind_table(context, init_attr);
}

/*
 * ibv_exp_destroy_rwq_ind_table - Destroys the specified Indirection Table.
 * @rwq_ind_table: The Indirection Table to destroy.
 * Return Value
 * ibv_destroy_rwq_ind_table() returns 0 on success, or the value of errno
 * on failure (which indicates the failure reason).
*/
static inline int ibv_exp_destroy_rwq_ind_table(struct ibv_exp_rwq_ind_table *rwq_ind_table)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(rwq_ind_table->context, exp_destroy_rwq_ind_table);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_destroy_rwq_ind_table(rwq_ind_table);
}

/*
 * ibv_exp_query_gid_attr - query a GID attributes
 * @context: ib context
 * @port_num: port number
 * @index: gid index in the gids table
 * @attr: the gid attributes of index in the gids table
 * Return value
 * ibv_exp_query_gid_attr return 0 on success, or the value of errno on failure.
 */
static inline int ibv_exp_query_gid_attr(struct ibv_context *context,
					 uint8_t port_num,
					 unsigned int index,
					 struct ibv_exp_gid_attr *attr)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(context, exp_query_gid_attr);
	if (!vctx)
		return ENOSYS;

	IBV_EXP_RET_EINVAL_ON_INVALID_COMP_MASK(attr->comp_mask,
						IBV_EXP_QUERY_GID_ATTR_RESERVED - 1);
	return vctx->exp_query_gid_attr(context, port_num, index, attr);
}
END_C_DECLS

#define VERBS_MAX_ENV_VAL 4096

#  undef __attribute_const


#endif /* INFINIBAND_VERBS_EXP_H */
