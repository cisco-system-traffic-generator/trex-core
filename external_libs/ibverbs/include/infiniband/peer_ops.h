/*
 * Copyright (c) 2016 Mellanox Technologies Inc.  All rights reserved.
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
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
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

#ifndef PEER_OPS_H
#define PEER_OPS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <infiniband/verbs.h>

BEGIN_C_DECLS

enum ibv_exp_peer_op {
	IBV_EXP_PEER_OP_RESERVED1	= 1,

	IBV_EXP_PEER_OP_FENCE		= 0,

	IBV_EXP_PEER_OP_STORE_DWORD	= 4,
	IBV_EXP_PEER_OP_STORE_QWORD	= 2,
	IBV_EXP_PEER_OP_COPY_BLOCK	= 3,

	IBV_EXP_PEER_OP_POLL_AND_DWORD	= 12,
	IBV_EXP_PEER_OP_POLL_NOR_DWORD	= 13,
	IBV_EXP_PEER_OP_POLL_GEQ_DWORD	= 14,
};

enum ibv_exp_peer_op_caps {
	IBV_EXP_PEER_OP_FENCE_CAP	= (1 << IBV_EXP_PEER_OP_FENCE),
	IBV_EXP_PEER_OP_STORE_DWORD_CAP	= (1 << IBV_EXP_PEER_OP_STORE_DWORD),
	IBV_EXP_PEER_OP_STORE_QWORD_CAP	= (1 << IBV_EXP_PEER_OP_STORE_QWORD),
	IBV_EXP_PEER_OP_COPY_BLOCK_CAP	= (1 << IBV_EXP_PEER_OP_COPY_BLOCK),
	IBV_EXP_PEER_OP_POLL_AND_DWORD_CAP
		= (1 << IBV_EXP_PEER_OP_POLL_AND_DWORD),
	IBV_EXP_PEER_OP_POLL_NOR_DWORD_CAP
		= (1 << IBV_EXP_PEER_OP_POLL_NOR_DWORD),
	IBV_EXP_PEER_OP_POLL_GEQ_DWORD_CAP
		= (1 << IBV_EXP_PEER_OP_POLL_GEQ_DWORD),
};

enum ibv_exp_peer_fence {
	IBV_EXP_PEER_FENCE_OP_READ		= (1 << 0),
	IBV_EXP_PEER_FENCE_OP_WRITE		= (1 << 1),
	IBV_EXP_PEER_FENCE_FROM_CPU		= (1 << 2),
	IBV_EXP_PEER_FENCE_FROM_HCA		= (1 << 3),
	IBV_EXP_PEER_FENCE_MEM_SYS		= (1 << 4),
	IBV_EXP_PEER_FENCE_MEM_PEER		= (1 << 5),
};

/* Indicate HW entities supposed to access memory buffer:
 * IBV_EXP_PEER_DIRECTION_FROM_X means X writes to the buffer
 * IBV_EXP_PEER_DIRECTION_TO_Y means Y read from the buffer
 */
enum ibv_exp_peer_direction {
	IBV_EXP_PEER_DIRECTION_FROM_CPU	 = (1 << 0),
	IBV_EXP_PEER_DIRECTION_FROM_HCA	 = (1 << 1),
	IBV_EXP_PEER_DIRECTION_FROM_PEER = (1 << 2),
	IBV_EXP_PEER_DIRECTION_TO_CPU	 = (1 << 3),
	IBV_EXP_PEER_DIRECTION_TO_HCA	 = (1 << 4),
	IBV_EXP_PEER_DIRECTION_TO_PEER	 = (1 << 5),
};

struct ibv_exp_peer_buf_alloc_attr {
	size_t length;
	/* Bitmask from enum ibv_exp_peer_direction */
	uint32_t dir;
	/* The ID of the peer device which will be
	 * accessing the allocated buffer
	 */
	uint64_t peer_id;
	/* Data alignment */
	uint32_t alignment;
	/* Reserved for future extensions, must be 0 */
	uint32_t comp_mask;
};

struct ibv_exp_peer_buf {
	void *addr;
	size_t length;
	/* Reserved for future extensions, must be 0 */
	uint32_t comp_mask;
};

enum ibv_exp_peer_direct_attr_mask {
	IBV_EXP_PEER_DIRECT_VERSION	= (1 << 0) /* Must be set */
};

#define IBV_EXP_PEER_IOMEMORY ((struct ibv_exp_peer_buf *)-1UL)

struct ibv_exp_peer_direct_attr {
	/* Unique ID per peer device.
	 * Used to identify specific HW devices where relevant.
	 */
	uint64_t peer_id;
	/* buf_alloc callback should return struct ibv_exp_peer_buf with buffer
	 * of at least attr->length.
	 * @attr: description of desired buffer
	 *
	 * Buffer should be mapped in the application address space
	 * for read/write (depends on attr->dir value).
	 * attr->dir value is supposed to indicate the expected directions
	 * of access to the buffer, to allow optimization by the peer driver.
	 * If NULL returned then buffer will be allocated in system memory
	 * by ibverbs driver.
	 */
	struct ibv_exp_peer_buf *(*buf_alloc)(struct ibv_exp_peer_buf_alloc_attr *attr);
	/* If buffer was allocated by buf_alloc then buf_release will be
	 * called to release it.
	 * @pb: struct returned by buf_alloc
	 *
	 * buf_release is responsible to release everything allocated by
	 * buf_alloc.
	 * Return 0 on succes.
	 */
	int (*buf_release)(struct ibv_exp_peer_buf *pb);
	/* register_va callback should register virtual address from the
	 * application as an area the peer is allowed to access.
	 * @start: pointer to beginning of region in virtual space
	 * @length: length of region
	 * @peer_id: the ID of the peer device which will be accessing
	 * the region.
	 * @pb: if registering a buffer that was returned from buf_alloc(),
	 * pb is the struct that was returned. If registering io memory area,
	 * pb is IBV_EXP_PEER_IOMEMORY. Otherwise - NULL
	 *
	 * Return id of registered address on success, 0 on failure.
	 */
	uint64_t (*register_va)(void *start, size_t length, uint64_t peer_id,
				struct ibv_exp_peer_buf *pb);
	/* If virtual address was registered with register_va then
	 * unregister_va will be called to unregister it.
	 * @target_id: id returned by register_va
	 * @peer_id: the ID of the peer device passed to register_va
	 *
	 * Return 0 on success.
	 */
	int (*unregister_va)(uint64_t target_id, uint64_t peer_id);
	/* Bitmask from ibv_exp_peer_op_caps */
	uint64_t caps;
	/* Maximal length of DMA operation the peer can do in copy-block */
	size_t peer_dma_op_map_len;
	/* From ibv_exp_peer_direct_attr_mask */
	uint32_t comp_mask;
	/* Feature version, must be 1 */
	uint32_t version;
};

/* QP API - CPU posts send work-requests without exposing them to the HW.
 * Later, the peer device exposes the relevant work requests to the HCA
 * for execution.
 */

struct peer_op_wr {
	struct peer_op_wr *next;
	enum ibv_exp_peer_op type;
	union {
		struct {
			uint64_t fence_flags; /* from ibv_exp_peer_fence */
		} fence;

		struct {
			uint32_t  data;
			uint64_t  target_id;
			size_t	  offset;
		} dword_va; /* Use for all operations targeting dword */

		struct {
			uint64_t  data;
			uint64_t  target_id;
			size_t	  offset;
		} qword_va; /* Use for all operations targeting qword */

		struct {
			void	 *src;
			uint64_t  target_id;
			size_t	  offset;
			size_t	  len;
		} copy_op;
	} wr;
	uint32_t comp_mask; /* Reserved for future expensions, must be 0 */
};

struct ibv_exp_peer_commit {
	/* IN/OUT - linked list of empty/filled descriptors */
	struct peer_op_wr *storage;
	/* IN/OUT - number of allocated/filled descriptors */
	uint32_t entries;
	/* OUT - identifier used in ibv_exp_rollback_qp to rollback WQEs set */
	uint64_t rollback_id;
	uint32_t comp_mask; /* Reserved for future expensions, must be 0 */
};

/**
 * ibv_exp_peer_commit_qp - request descriptors for committing all WQEs
 * currently posted to the send work queue
 * @qp: the QP being requested
 * @peer: context with list of &struct peer_op_wr describing actions
 *   necessary to commit WQEs
 *
 * Function
 * - fill peer->storage with descriptors
 * - put number of filled descriptors to peer->entries;
 * - put data necessary for rollback to peer->rollback_id
 * If number of entries is not sufficient - return -ENOSPC
 *
 * Note: caller is responsible to ensure that the peer fences any data store
 * before executing the commit
 */
static inline int ibv_exp_peer_commit_qp(struct ibv_qp *qp,
					 struct ibv_exp_peer_commit *peer)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(qp->context, exp_peer_commit_qp);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_peer_commit_qp(qp, peer);
}

enum ibv_exp_rollback_flags {
	/* Abort all WQEs which were not committed to HW yet.
	 * rollback_id is ignored. **/
	IBV_EXP_ROLLBACK_ABORT_UNCOMMITED = (1 << 0),
	/* Abort the request even if there are following requests
	 * being aborted as well. **/
	IBV_EXP_ROLLBACK_ABORT_LATE = (1 << 1),
};

struct ibv_exp_rollback_ctx {
	uint64_t rollback_id; /* from ibv_exp_peer_commit call */
	uint32_t flags; /* from ibv_exp_rollback_flags */
	uint32_t comp_mask; /* Reserved for future expensions, must be 0 */
};

/**
 * ibv_exp_rollback_qp - indicate that the commit attempt failed
 * @qp: the QP being rolled back
 * @rollback: context with rollback_id returned by
 *   earlier ibv_exp_peer_commit_qp and flags
 */
static inline int ibv_exp_rollback_qp(struct ibv_qp *qp,
				      struct ibv_exp_rollback_ctx *rollback)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(qp->context, exp_rollback_send);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_rollback_send(qp, rollback);
}

/* CQ interface - peek into a CQ and describe how to check if
 * there is a CQ entry available.
 */

enum {
	IBV_EXP_PEER_PEEK_ABSOLUTE,
	IBV_EXP_PEER_PEEK_RELATIVE
};

struct ibv_exp_peer_peek {
	/* IN/OUT - linked list of empty/filled descriptors */
	struct peer_op_wr *storage;
	/* IN/OUT - number of allocated/filled descriptors */
	uint32_t entries;
	/* IN - Which CQ entry does the peer want to peek for
	 * completion. According to "whence" directive entry
	 * chosen as follows:
	 * IBV_EXP_PEER_PEEK_ABSOLUTE -
	 *	"offset" is absolute index of entry wrapped to 32-bit
	 * IBV_EXP_PEER_PEEK_RELATIVE -
	 *      "offset" is relative to current poll_cq location.
	 */
	uint32_t whence;
	uint32_t offset;
	/* OUT - identifier used in ibv_exp_peer_ack_peek_cq to advance CQ */
	uint64_t peek_id;
	uint32_t comp_mask; /* Reserved for future expensions, must be 0 */
};

/**
 * ibv_exp_peer_peek_cq - request descriptors for peeking CQ in specific
 *   offset from currently expected CQ entry.
 * @cq: the CQ being requested
 * @peer_ctx: context with list of &struct peer_op_wr describing actions
 *   necessary to wait for desired CQ entry is delivered and report
 *   this to ibverbs.
 *
 * A peek CQ request places a "block" on the relevant CQ entry.
 *   Poll CQ requests to poll the CQ entry will fail with an error.
 *   The block will be removed by executing the descriptors.
 *   If the peer will not be able to execute the descriptors,
 *   it should call ibv_exp_peer_abort_peek_cq to remove the block.
 *
 * Function
 * - fill peek_ctx->storage with descriptors.
 * - put number of filled descriptors to peek_ctx->entries.
 * - put data necessary to abort peek.
 * If number of entries is not sufficient - return -ENOSPC.
 */
static inline int ibv_exp_peer_peek_cq(struct ibv_cq *cq,
				       struct ibv_exp_peer_peek *peek_ctx)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(cq->context, exp_peer_peek_cq);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_peer_peek_cq(cq, peek_ctx);
}

struct ibv_exp_peer_abort_peek {
	uint64_t peek_id; /* From the peer_peek_cq call */
	uint32_t comp_mask; /* Reserved for future expensions, must be 0 */
};

/**
 * ibv_exp_peer_abort_peek_cq - indicate that peek is aborted
 * @cq: the CQ being rolled back
 * @abort_ctx: context with peek_id returned by earlier ibv_exp_peer_peek_cq
 *
 * Note: This should be done only if the peek descriptors were not executed
 */
static inline int ibv_exp_peer_abort_peek_cq(struct ibv_cq *cq,
				     struct ibv_exp_peer_abort_peek *abort_ctx)
{
	struct verbs_context_exp *vctx;

	vctx = verbs_get_exp_ctx_op(cq->context, exp_peer_abort_peek_cq);
	if (!vctx)
		return ENOSYS;

	return vctx->exp_peer_abort_peek_cq(cq, abort_ctx);
}

END_C_DECLS

#endif /* PEER_OPS_H */
