/**
 * Copyright (C) Mellanox Technologies Ltd. 2001-2014.  ALL RIGHTS RESERVED.
 * This software product is a proprietary product of Mellanox Technologies Ltd.
 * (the "Company") and all right, title, and interest and to the software product,
 * including all associated intellectual property rights, are and shall
 * remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 */

#ifndef MLX_HW_H_
#define MLX_HW_H_

#include <linux/types.h>
#include <stdint.h>
#include <pthread.h>
#include <infiniband/driver.h>
#include <infiniband/verbs.h>

#define MLX5_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#if MLX5_GCC_VERSION >= 403
#	define __MLX5_ALGN_F__ __attribute__((noinline, aligned(64)))
#	define __MLX5_ALGN_D__ __attribute__((aligned(64)))
#else
#	define __MLX5_ALGN_F__
#	define __MLX5_ALGN_D__
#endif

#define MLX5_CQ_DB_REQ_NOT_SOL			(1 << 24)
#define MLX5_CQ_DB_REQ_NOT			(0 << 24)
#define MLX5E_CQE_FORMAT_MASK 0xc


enum mlx5_alloc_type { MXM_MLX5_ALLOC_TYPE_DUMMY };
enum mlx5_rsc_type   { MXM_MLX5_RSC_TYPE_DUMMY };
enum mlx5_db_method { MXM_MLX5_DB_TYPE_DUMMY };
enum mlx5_lock_type { MXM_MLX5_LOCK_TYPE_DUMMY };
enum mlx5_lock_state { MXM_MLX5_LOCK_STATE_TYPE_DUMMY };
enum {
	MLX5_RCV_DBR = 0,
	MLX5_SND_DBR = 1,
	MLX5_SEND_WQE_BB = 64,
	MLX5_SEND_WQE_SHIFT = 6,
	MLX5_INLINE_SCATTER_32 = 0x4,
	MLX5_INLINE_SCATTER_64 = 0x8,
	MLX5_OPCODE_NOP = 0x00,
	MLX5_OPCODE_SEND_INVAL = 0x01,
	MLX5_OPCODE_RDMA_WRITE = 0x08,
	MLX5_OPCODE_RDMA_WRITE_IMM = 0x09,
	MLX5_OPCODE_SEND = 0x0a,
	MLX5_OPCODE_SEND_IMM = 0x0b,
	MLX5_OPCODE_TSO = 0x0e,
	MLX5_OPC_MOD_MPW = 0x01,
    MLX5_OPCODE_LSO_MPW =  0x0e,
	MLX5_OPCODE_RDMA_READ = 0x10,
	MLX5_OPCODE_ATOMIC_CS = 0x11,
	MLX5_OPCODE_ATOMIC_FA = 0x12,
	MLX5_OPCODE_ATOMIC_MASKED_CS = 0x14,
	MLX5_OPCODE_ATOMIC_MASKED_FA = 0x15,
	MLX5_OPCODE_BIND_MW = 0x18,
	MLX5_OPCODE_FMR = 0x19,
	MLX5_OPCODE_LOCAL_INVAL = 0x1b,
	MLX5_OPCODE_CONFIG_CMD = 0x1f,
	MLX5_OPCODE_SEND_ENABLE = 0x17,
	MLX5_OPCODE_RECV_ENABLE = 0x16,
	MLX5_OPCODE_CQE_WAIT = 0x0f,
	MLX5_RECV_OPCODE_RDMA_WRITE_IMM = 0x00,
	MLX5_RECV_OPCODE_SEND = 0x01,
	MLX5_RECV_OPCODE_SEND_IMM = 0x02,
	MLX5_RECV_OPCODE_SEND_INVAL = 0x03,
	MLX5_CQE_OPCODE_ERROR = 0x1e,
	MLX5_CQE_OPCODE_RESIZE = 0x16,
	MLX5_SRQ_FLAG_SIGNATURE = 1 << 0,
	MLX5_INLINE_SEG = 0x80000000,
	MLX5_CALC_UINT64_ADD = 0x01,
	MLX5_CALC_FLOAT64_ADD = 0x02,
	MLX5_CALC_UINT64_MAXLOC = 0x03,
	MLX5_CALC_UINT64_AND = 0x04,
	MLX5_CALC_UINT64_OR = 0x05,
	MLX5_CALC_UINT64_XOR = 0x06,
	MLX5_CQ_DOORBELL = 0x20,
	MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR = 0x01,
	MLX5_CQE_SYNDROME_LOCAL_QP_OP_ERR = 0x02,
	MLX5_CQE_SYNDROME_LOCAL_PROT_ERR = 0x04,
	MLX5_CQE_SYNDROME_WR_FLUSH_ERR = 0x05,
	MLX5_CQE_SYNDROME_MW_BIND_ERR = 0x06,
	MLX5_CQE_SYNDROME_BAD_RESP_ERR = 0x10,
	MLX5_CQE_SYNDROME_LOCAL_ACCESS_ERR = 0x11,
	MLX5_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR = 0x12,
	MLX5_CQE_SYNDROME_REMOTE_ACCESS_ERR = 0x13,
	MLX5_CQE_SYNDROME_REMOTE_OP_ERR = 0x14,
	MLX5_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR = 0x15,
	MLX5_CQE_SYNDROME_RNR_RETRY_EXC_ERR = 0x16,
	MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR = 0x22,
	MLX5_CQE_OWNER_MASK = 1,
	MLX5_CQE_REQ = 0,
	MLX5_CQE_RESP_WR_IMM = 1,
	MLX5_CQE_RESP_SEND = 2,
	MLX5_CQE_RESP_SEND_IMM = 3,
	MLX5_CQE_RESP_SEND_INV = 4,
	MLX5_CQE_RESIZE_CQ = 5,
	MLX5_CQE_SIG_ERR = 12,
	MLX5_CQE_REQ_ERR = 13,
	MLX5_CQE_RESP_ERR = 14,
	MLX5_CQE_INVALID = 15,
	MLX5_WQE_CTRL_CQ_UPDATE = 2 << 2,
	MLX5_WQE_CTRL_SOLICITED = 1 << 1,
	MLX5_WQE_CTRL_FENCE = 4 << 5,
	MLX5_INVALID_LKEY = 0x100,
	MLX5_EXTENDED_UD_AV = 0x80000000,
	MLX5_NO_INLINE_DATA = 0x0,
	MLX5_INLINE_DATA32_SEG = 0x1,
	MLX5_INLINE_DATA64_SEG = 0x2,
	MLX5_COMPRESSED = 0x3,
	MLX5_MINI_ARR_SIZE = 8,
	MLX5_ETH_WQE_L3_CSUM = (1 << 6),
	MLX5_ETH_WQE_L4_CSUM = (1 << 7),
	MLX5_ETH_INLINE_HEADER_SIZE = 18,
	MLX5_ETH_VLAN_INLINE_HEADER_SIZE = 18,
	MLX5_CQE_L2_OK = 1 << 0,
	MLX5_CQE_L3_OK = 1 << 1,
	MLX5_CQE_L4_OK = 1 << 2,
	MLX5_CQE_L3_HDR_TYPE_NONE = 0x0,
	MLX5_CQE_L3_HDR_TYPE_IPV6 = 0x4,
	MLX5_CQE_L3_HDR_TYPE_IPV4 = 0x8,
	MLX5_CQE_L4_HDR_TYPE_TCP = 0x10,
	MLX5_CQE_L4_HDR_TYPE_UDP = 0x20,
	MLX5_CQE_L4_HDR_TYPE_TCP_EMP_ACK = 0x30,
	MLX5_CQE_L4_HDR_TYPE_TCP_ACK = 0x40,
	MLX5_CQE_L3_HDR_TYPE_MASK = 0xC,
	MLX5_CQE_L4_HDR_TYPE_MASK = 0x70,
	MLX5_QP_PEER_VA_ID_MAX = 2,
};

struct mlx5_qp;

struct mlx5_resource {
	enum mlx5_rsc_type	type;
	uint32_t		rsn;
};


struct mlx5_wqe_srq_next_seg {
	uint8_t			rsvd0[2];
	uint16_t		next_wqe_index;
	uint8_t			signature;
	uint8_t			rsvd1[11];
};


struct mlx5_wqe_data_seg {
	uint32_t		byte_count;
	uint32_t		lkey;
	uint64_t		addr;
};


struct mlx5_eqe_comp {
	uint32_t	reserved[6];
	uint32_t	cqn;
};


struct mlx5_eqe_qp_srq {
	uint32_t	reserved[6];
	uint32_t	qp_srq_n;
};


struct mlx5_wqe_ctrl_seg {
	uint32_t	opmod_idx_opcode;
	uint32_t	qpn_ds;
	uint8_t		signature;
	uint8_t		rsvd[2];
	uint8_t		fm_ce_se;
	uint32_t	imm;
};


struct mlx5_wqe_xrc_seg {
	uint32_t	xrc_srqn;
	uint8_t		rsvd[12];
};


struct mlx5_wqe_masked_atomic_seg {
	uint64_t	swap_add;
	uint64_t	compare;
	uint64_t	swap_add_mask;
	uint64_t	compare_mask;
};


struct mlx5_base_av {
	union {
		struct {
			uint32_t	qkey;
			uint32_t	reserved;
		} qkey;
		uint64_t	dc_key;
	} key;
	uint32_t	dqp_dct;
	uint8_t		stat_rate_sl;
	uint8_t		fl_mlid;
	uint16_t	rlid;
};


struct mlx5_grh_av {
	uint8_t		reserved0[4];
	uint8_t		rmac[6];
	uint8_t		tclass;
	uint8_t		hop_limit;
	uint32_t	grh_gid_fl;
	uint8_t		rgid[16];
};


struct mlx5_wqe_av {
	struct mlx5_base_av	base;
	struct mlx5_grh_av	grh_sec;
};


struct mlx5_wqe_datagram_seg {
	struct mlx5_wqe_av	av;
};


struct mlx5_wqe_raddr_seg {
	uint64_t	raddr;
	uint32_t	rkey;
	uint32_t	reserved;
};


struct mlx5_wqe_atomic_seg {
	uint64_t	swap_add;
	uint64_t	compare;
};


struct mlx5_wqe_inl_data_seg {
	uint32_t	byte_count;
};


struct mlx5_wqe_umr_ctrl_seg {
	uint8_t		flags;
	uint8_t		rsvd0[3];
	uint16_t	klm_octowords;
	uint16_t	bsf_octowords;
	uint64_t	mkey_mask;
	uint8_t		rsvd1[32];
};


struct mlx5_seg_set_psv {
	uint8_t		rsvd[4];
	uint16_t	syndrome;
	uint16_t	status;
	uint16_t	block_guard;
	uint16_t	app_tag;
	uint32_t	ref_tag;
	uint32_t	mkey;
	uint64_t	va;
};


struct mlx5_seg_get_psv {
	uint8_t		rsvd[19];
	uint8_t		num_psv;
	uint32_t	l_key;
	uint64_t	va;
	uint32_t	psv_index[4];
};


struct mlx5_seg_check_psv {
	uint8_t		rsvd0[2];
	uint16_t	err_coalescing_op;
	uint8_t		rsvd1[2];
	uint16_t	xport_err_op;
	uint8_t		rsvd2[2];
	uint16_t	xport_err_mask;
	uint8_t		rsvd3[7];
	uint8_t		num_psv;
	uint32_t	l_key;
	uint64_t	va;
	uint32_t	psv_index[4];
};


struct mlx5_rwqe_sig {
	uint8_t		rsvd0[4];
	uint8_t		signature;
	uint8_t		rsvd1[11];
};


struct mlx5_wqe_signature_seg {
	uint8_t		rsvd0[4];
	uint8_t		signature;
	uint8_t		rsvd1[11];
};


struct mlx5_wqe_inline_seg {
	uint32_t	byte_count;
};


struct mlx5_wqe_wait_en_seg {
	uint8_t		rsvd0[8];
	uint32_t	pi;
	uint32_t	obj_num;
};


struct mlx5_err_cqe {
	uint8_t		rsvd0[32];
	uint32_t	srqn;
	uint8_t		rsvd1[16];
	uint8_t		hw_err_synd;
	uint8_t		hw_synd_type;
	uint8_t		vendor_err_synd;
	uint8_t		syndrome;
	uint32_t	s_wqe_opcode_qpn;
	uint16_t	wqe_counter;
	uint8_t		signature;
	uint8_t		op_own;
};


struct mlx5_cqe64 {
	uint8_t		rsvd0[2];
	/*
	 * wqe_id is valid only for Striding RQ (Multi-Packet RQ).
	 * It provides the WQE index inside the RQ.
	 */
	uint16_t	wqe_id;
	uint8_t		rsvd4[8];
	uint32_t	rx_hash_res;
	uint8_t		rx_hash_type;
	uint8_t		ml_path;
	uint8_t		rsvd20[2];
	uint16_t	checksum;
	uint16_t	slid;
	uint32_t	flags_rqpn;
	uint8_t		hds_ip_ext;
	uint8_t		l4_hdr_type_etc;
	__be16		vlan_info;
	uint32_t	srqn_uidx;
	uint32_t	imm_inval_pkey;
	uint8_t		rsvd40[4];
	uint32_t	byte_cnt;
	__be64		timestamp;
	union {
		uint32_t	sop_drop_qpn;
		struct {
			uint8_t	sop;
			uint8_t qpn[3];
		} sop_qpn;
	};
	/*
	 * In Striding RQ (Multi-Packet RQ) wqe_counter provides
	 * the WQE stride index (to calc pointer to start of the message)
	 */
	uint16_t	wqe_counter;
	uint8_t		signature;
	uint8_t		op_own;
};


struct mlx5_spinlock {
	pthread_spinlock_t		lock;
	enum mlx5_lock_state		state;
};


struct mlx5_lock {
	pthread_mutex_t                 mutex;
	pthread_spinlock_t              slock;
	enum mlx5_lock_state            state;
	enum mlx5_lock_type             type;
};


struct mlx5_numa_req {
	int valid;
	int numa_id;
};


struct mlx5_peer_direct_mem {
	uint32_t dir;
	uint64_t va_id;
	struct ibv_exp_peer_buf *pb;
	struct ibv_exp_peer_direct_attr *ctx;
};


struct mlx5_buf {
	void			       *buf;
	size_t				length;
	int                             base;
	struct mlx5_hugetlb_mem	       *hmem;
	struct mlx5_peer_direct_mem     peer;
	enum mlx5_alloc_type		type;
	struct mlx5_numa_req		numa_req;
	int				numa_alloc;
};


struct general_data_hot {
	/* post_send hot data */
	unsigned		*wqe_head;
	int			(*post_send_one)(struct ibv_exp_send_wr *wr,
						 struct mlx5_qp *qp,
						 uint64_t exp_send_flags,
						 void *seg, int *total_size);
	void			*sqstart;
	void			*sqend;
	volatile uint32_t	*db;
	struct mlx5_bf		*bf;
	uint32_t		 scur_post;
	/* Used for burst_family interface, keeps the last posted wqe */
	uint32_t		 last_post;
	uint16_t		 create_flags;
	uint8_t			 fm_cache;
	uint8_t			 model_flags; /* use mlx5_qp_model_flags */
};


struct data_seg_data {
	uint32_t	max_inline_data;
};


struct ctrl_seg_data {
	uint32_t	qp_num;
	uint8_t		fm_ce_se_tbl[8];
	uint8_t		fm_ce_se_acc[32];
	uint8_t		wq_sig;
};


struct mpw_data {
	uint8_t		state; /* use mpw_states */
	uint8_t		size;
	uint8_t		num_sge;
	uint32_t	len;
	uint32_t	total_len;
	uint32_t	flags;
	uint32_t	scur_post;
	union {
		struct mlx5_wqe_data_seg	*last_dseg;
		uint8_t				*inl_data;
	};
	uint32_t			*ctrl_update;
};


struct general_data_warm {
	uint32_t		 pattern;
	uint8_t			 qp_type;
};


struct odp_data {
	struct mlx5_pd		*pd;
};


struct mlx5_wq_recv_send_enable {
	unsigned			head_en_index;
	unsigned			head_en_count;
};


struct mlx5_mini_cqe8 {
	union {
		uint32_t rx_hash_result;
		uint32_t checksum;
		struct {
			uint16_t wqe_counter;
			uint8_t  s_wqe_opcode;
			uint8_t  reserved;
		} s_wqe_info;
	};
	uint32_t byte_cnt;
};


struct mlx5_cq {
	struct ibv_cq			ibv_cq;
	uint32_t			creation_flags;
	uint32_t			pattern;
	struct mlx5_buf			buf_a;
	struct mlx5_buf			buf_b;
	struct mlx5_buf		       *active_buf;
	struct mlx5_buf		       *resize_buf;
	int				resize_cqes;
	int				active_cqes;
	struct mlx5_lock		lock;
	uint32_t			cqn;
	uint32_t			cons_index;
	uint32_t                        wait_index;
	uint32_t                        wait_count;
	volatile uint32_t	       *dbrec;
	int				arm_sn;
	int				cqe_sz;
	int				resize_cqe_sz;
	int				stall_next_poll;
	int				stall_enable;
	uint64_t			stall_last_count;
	int				stall_adaptive_enable;
	int				stall_cycles;
	uint8_t				model_flags; /* use mlx5_cq_model_flags */
	uint16_t			cqe_comp_max_num;
	uint8_t				cq_log_size;
	/* Compressed CQE data */
	struct mlx5_cqe64		next_decomp_cqe64;
	struct mlx5_resource	       *compressed_rsc;
	uint16_t			compressed_left;
	uint16_t			compressed_wqe_cnt;
	uint8_t				compressed_req;
	uint8_t				compressed_mp_rq;
	uint8_t				mini_arr_idx;
	struct mlx5_mini_cqe8		mini_array[MLX5_MINI_ARR_SIZE];
	/* peer-direct data */
	int					peer_enabled;
	struct ibv_exp_peer_direct_attr	       *peer_ctx;
	struct mlx5_buf				peer_buf;
	struct mlx5_peek_entry		      **peer_peek_table;
	struct mlx5_peek_entry		       *peer_peek_free;
};


struct mlx5_srq {
	struct mlx5_resource            rsc;  /* This struct must be first */
	struct verbs_srq		vsrq;
	struct mlx5_buf			buf;
	struct mlx5_spinlock		lock;
	uint64_t		       *wrid;
	uint32_t			srqn;
	int				max;
	int				max_gs;
	int				wqe_shift;
	int				head;
	int				tail;
	volatile uint32_t	       *db;
	uint16_t			counter;
	int				wq_sig;
	struct ibv_srq_legacy *ibv_srq_legacy;
	int				is_xsrq;
};


struct mlx5_wq {
	/* common hot data */
	uint64_t		       *wrid;
	unsigned			wqe_cnt;
	unsigned			head;
	unsigned			tail;
	unsigned			max_post;
	int				max_gs;
	struct mlx5_lock		lock;
	/* post_recv hot data */
	void			       *buff;
	volatile uint32_t	       *db;
	int				wqe_shift;
	int				offset;
};


struct mlx5_bf {
	void			       *reg;
	int				need_lock;
	/*
	 * Protect usage of BF address field including data written to the BF
	 * and the BF buffer toggling.
	 */
	struct mlx5_lock		lock;
	unsigned			offset;
	unsigned			buf_size;
	unsigned			uuarn;
	enum mlx5_db_method		db_method;
};


struct mlx5_qp {
	struct mlx5_resource		rsc;
	struct verbs_qp			verbs_qp;
	struct mlx5_buf                 buf;
	int                             buf_size;
	/* For Raw Ethernet QP, use different Buffer for the SQ and RQ */
	struct mlx5_buf                 sq_buf;
	int				sq_buf_size;
	uint8_t	                        sq_signal_bits;
	int				umr_en;

	/* hot data used on data path */
	struct mlx5_wq			rq __MLX5_ALGN_D__;
	struct mlx5_wq			sq __MLX5_ALGN_D__;

	struct general_data_hot		gen_data;
	struct mpw_data			mpw;
	struct data_seg_data		data_seg;
	struct ctrl_seg_data		ctrl_seg;

	/* RAW_PACKET hot data */
	uint8_t				link_layer;

	/* used on data-path but not so hot */
	struct general_data_warm	gen_data_warm;
	/* atomic hot data */
	int				enable_atomics;
	/* odp hot data */
	struct odp_data			odp_data;
	/* ext atomic hot data */
	uint32_t			max_atomic_arg;
	/* umr hot data */
	uint32_t			max_inl_send_klms;
	/* recv-send enable hot data */
	struct mlx5_wq_recv_send_enable rq_enable;
	struct mlx5_wq_recv_send_enable sq_enable;
	int rx_qp;
	/* peer-direct data */
	int					peer_enabled;
	struct ibv_exp_peer_direct_attr	       *peer_ctx;
	void				       *peer_ctrl_seg;
	uint32_t				peer_scur_post;
	uint64_t				peer_va_ids[MLX5_QP_PEER_VA_ID_MAX];
	struct ibv_exp_peer_buf		       *peer_db_buf;
	uint32_t				max_tso_header;
};


struct mlx5_ah {
	struct ibv_ah			ibv_ah;
	struct mlx5_wqe_av		av;
};


struct mlx5_rwq {
	struct mlx5_resource rsc;
	uint32_t pattern;
	struct ibv_exp_wq wq;
	struct mlx5_buf buf;
	int buf_size;
	/* hot data used on data path */
	struct mlx5_wq rq __MLX5_ALGN_D__;
	volatile uint32_t *db;
	/* Multi-Packet RQ hot data */
	/* Table to hold the consumed strides on each WQE */
	uint32_t *consumed_strides_counter;
	uint16_t mp_rq_stride_size;
	uint32_t mp_rq_strides_in_wqe;
	uint8_t mp_rq_packet_padding;
	/* recv-send enable hot data */
	struct mlx5_wq_recv_send_enable rq_enable;
	int wq_sig;
	uint8_t model_flags; /* use mlx5_wq_model_flags */
};


struct mlx5_wqe_eth_seg {
	uint32_t	rsvd0;
	uint8_t		cs_flags;
	uint8_t		rsvd1;
	uint16_t	mss;
	uint32_t	rsvd2;
	uint16_t	inline_hdr_sz;
	uint8_t		inline_hdr_start[2];
	uint8_t		inline_hdr[16];
};


#define to_mxxx(xxx, type)\
	((struct mlx5_##type *)\
	((void *) ((uintptr_t)ib##xxx - offsetof(struct mlx5_##type, ibv_##xxx))))

static inline struct mlx5_qp *to_mqp(struct ibv_qp *ibqp)
{
	struct verbs_qp *vqp = (struct verbs_qp *)ibqp;
	return container_of(vqp, struct mlx5_qp, verbs_qp);
}

static inline struct mlx5_cq *to_mcq(struct ibv_cq *ibcq)
{
	return to_mxxx(cq, cq);
}

struct ibv_mlx5_qp_info {
	uint32_t	qpn;
	volatile uint32_t	*dbrec;
	struct {
		void		*buf;
		unsigned	wqe_cnt;
		unsigned	stride;
	} sq, rq;
	struct {
		void		*reg;
		unsigned	size;
		int             need_lock;
	} bf;
};

static inline int ibv_mlx5_exp_get_qp_info(struct ibv_qp *qp, struct	ibv_mlx5_qp_info *qp_info)
{
	struct mlx5_qp *mqp = to_mqp(qp);

	if ((mqp->gen_data.scur_post != 0) || (mqp->rq.head != 0))
		return -1;

	qp_info->qpn = mqp->ctrl_seg.qp_num;
	qp_info->dbrec = mqp->gen_data.db;
	qp_info->sq.buf = (void *)((uintptr_t)mqp->buf.buf + mqp->sq.offset);
	qp_info->sq.wqe_cnt = mqp->sq.wqe_cnt;
	qp_info->sq.stride = 1 << mqp->sq.wqe_shift;
	qp_info->rq.buf = (void *)((uintptr_t)mqp->buf.buf + mqp->rq.offset);
	qp_info->rq.wqe_cnt = mqp->rq.wqe_cnt;
	qp_info->rq.stride = 1 << mqp->rq.wqe_shift;
	qp_info->bf.reg = mqp->gen_data.bf->reg;
	qp_info->bf.need_lock = mqp->gen_data.bf->need_lock;

	if (mqp->gen_data.bf->uuarn > 0)
		qp_info->bf.size = mqp->gen_data.bf->buf_size;
	else
		qp_info->bf.size = 0;

	return 0;
}

struct ibv_mlx5_cq_info {
	uint32_t	cqn;
	unsigned	cqe_cnt;
	void		*buf;
	volatile uint32_t	*dbrec;
	unsigned	cqe_size;
};

static inline int ibv_mlx5_exp_get_cq_info(struct ibv_cq *cq, struct	ibv_mlx5_cq_info *cq_info)
{
	struct mlx5_cq *mcq = to_mcq(cq);

	if (mcq->cons_index != 0)
		return -1;

	cq_info->cqn = mcq->cqn;
	cq_info->cqe_cnt = mcq->ibv_cq.cqe + 1;
	cq_info->cqe_size = mcq->cqe_sz;
	cq_info->buf = mcq->active_buf->buf;
	cq_info->dbrec = mcq->dbrec;

	return 0;
}

struct ibv_mlx5_srq_info {
	void		*buf;
	volatile uint32_t	*dbrec;
	unsigned	stride;
	unsigned	head;
	unsigned	tail;
};

static inline int ibv_mlx5_exp_get_srq_info(struct ibv_srq *srq, struct ibv_mlx5_srq_info *srq_info)
{
	struct mlx5_srq *msrq;

	if (srq->handle == LEGACY_XRC_SRQ_HANDLE)
	srq = (struct ibv_srq *)(((struct ibv_srq_legacy *)srq)->ibv_srq);

	msrq = container_of(srq, struct mlx5_srq, vsrq.srq);

	if (msrq->counter != 0)
		return -1;

	srq_info->buf = msrq->buf.buf;
	srq_info->dbrec = msrq->db;
	srq_info->stride = 1 << msrq->wqe_shift;
	srq_info->head = msrq->head;
	srq_info->tail = msrq->tail;

	return 0;
}

static inline void ibv_mlx5_exp_update_cq_ci(struct ibv_cq *cq, unsigned cq_ci)
{
	struct mlx5_cq *mcq = to_mcq(cq);

	mcq->cons_index = cq_ci;
}

#endif
