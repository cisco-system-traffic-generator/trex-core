/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5_hw.h>
#include <infiniband/arch.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

static inline int
check_cqe(volatile struct mlx5_cqe *cqe,
	  unsigned int cqes_n, const uint16_t ci)
	  __attribute__((always_inline));

static inline void
txq_complete(struct txq *txq) __attribute__((always_inline));

static inline uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
	__attribute__((always_inline));

static inline void
mlx5_tx_dbrec(struct txq *txq, volatile struct mlx5_wqe *wqe)
	__attribute__((always_inline));

static inline uint32_t
rxq_cq_to_pkt_type(volatile struct mlx5_cqe *cqe)
	__attribute__((always_inline));

static inline int
mlx5_rx_poll_len(struct rxq *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, uint32_t *rss_hash)
		 __attribute__((always_inline));

static inline uint32_t
rxq_cq_to_ol_flags(struct rxq *rxq, volatile struct mlx5_cqe *cqe)
		   __attribute__((always_inline));

#ifndef NDEBUG

/**
 * Verify or set magic value in CQE.
 *
 * @param cqe
 *   Pointer to CQE.
 *
 * @return
 *   0 the first time.
 */
static inline int
check_cqe_seen(volatile struct mlx5_cqe *cqe)
{
	static const uint8_t magic[] = "seen";
	volatile uint8_t (*buf)[sizeof(cqe->rsvd0)] = &cqe->rsvd0;
	int ret = 1;
	unsigned int i;

	for (i = 0; i < sizeof(magic) && i < sizeof(*buf); ++i)
		if (!ret || (*buf)[i] != magic[i]) {
			ret = 0;
			(*buf)[i] = magic[i];
		}
	return ret;
}

#endif /* NDEBUG */

/**
 * Check whether CQE is valid.
 *
 * @param cqe
 *   Pointer to CQE.
 * @param cqes_n
 *   Size of completion queue.
 * @param ci
 *   Consumer index.
 *
 * @return
 *   0 on success, 1 on failure.
 */
static inline int
check_cqe(volatile struct mlx5_cqe *cqe,
	  unsigned int cqes_n, const uint16_t ci)
{
	uint16_t idx = ci & cqes_n;
	uint8_t op_own = cqe->op_own;
	uint8_t op_owner = MLX5_CQE_OWNER(op_own);
	uint8_t op_code = MLX5_CQE_OPCODE(op_own);

	if (unlikely((op_owner != (!!(idx))) || (op_code == MLX5_CQE_INVALID)))
		return 1; /* No CQE. */
#ifndef NDEBUG
	if ((op_code == MLX5_CQE_RESP_ERR) ||
	    (op_code == MLX5_CQE_REQ_ERR)) {
		volatile struct mlx5_err_cqe *err_cqe = (volatile void *)cqe;
		uint8_t syndrome = err_cqe->syndrome;

		if ((syndrome == MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR) ||
		    (syndrome == MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR))
			return 0;
		if (!check_cqe_seen(cqe))
			ERROR("unexpected CQE error %u (0x%02x)"
			      " syndrome 0x%02x",
			      op_code, op_code, syndrome);
		return 1;
	} else if ((op_code != MLX5_CQE_RESP_SEND) &&
		   (op_code != MLX5_CQE_REQ)) {
		if (!check_cqe_seen(cqe))
			ERROR("unexpected CQE opcode %u (0x%02x)",
			      op_code, op_code);
		return 1;
	}
#endif /* NDEBUG */
	return 0;
}

/**
 * Return the address of the WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param  wqe_ci
 *   WQE consumer index.
 *
 * @return
 *   WQE address.
 */
static inline uintptr_t *
tx_mlx5_wqe(struct txq *txq, uint16_t ci)
{
	ci &= ((1 << txq->wqe_n) - 1);
	return (uintptr_t *)((uintptr_t)txq->wqes + ci * MLX5_WQE_SIZE);
}

/**
 * Manage TX completions.
 *
 * When sending a burst, mlx5_tx_burst() posts several WRs.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static inline void
txq_complete(struct txq *txq)
{
	const unsigned int elts_n = 1 << txq->elts_n;
	const unsigned int cqe_n = 1 << txq->cqe_n;
	const unsigned int cqe_cnt = cqe_n - 1;
	uint16_t elts_free = txq->elts_tail;
	uint16_t elts_tail;
	uint16_t cq_ci = txq->cq_ci;
	volatile struct mlx5_cqe *cqe = NULL;
	volatile struct mlx5_wqe_ctrl *ctrl;

	do {
		volatile struct mlx5_cqe *tmp;

		tmp = &(*txq->cqes)[cq_ci & cqe_cnt];
		if (check_cqe(tmp, cqe_n, cq_ci))
			break;
		cqe = tmp;
#ifndef NDEBUG
		if (MLX5_CQE_FORMAT(cqe->op_own) == MLX5_COMPRESSED) {
			if (!check_cqe_seen(cqe))
				ERROR("unexpected compressed CQE, TX stopped");
			return;
		}
		if ((MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_RESP_ERR) ||
		    (MLX5_CQE_OPCODE(cqe->op_own) == MLX5_CQE_REQ_ERR)) {
			if (!check_cqe_seen(cqe))
				ERROR("unexpected error CQE, TX stopped");
			return;
		}
#endif /* NDEBUG */
		++cq_ci;
	} while (1);
	if (unlikely(cqe == NULL))
		return;
	txq->wqe_pi = ntohs(cqe->wqe_counter);
	ctrl = (volatile struct mlx5_wqe_ctrl *)
		tx_mlx5_wqe(txq, txq->wqe_pi);
	elts_tail = ctrl->ctrl3;
	assert(elts_tail < (1 << txq->wqe_n));
	/* Free buffers. */
	while (elts_free != elts_tail) {
		struct rte_mbuf *elt = (*txq->elts)[elts_free];
		unsigned int elts_free_next =
			(elts_free + 1) & (elts_n - 1);
		struct rte_mbuf *elt_next = (*txq->elts)[elts_free_next];

#ifndef NDEBUG
		/* Poisoning. */
		memset(&(*txq->elts)[elts_free],
		       0x66,
		       sizeof((*txq->elts)[elts_free]));
#endif
		RTE_MBUF_PREFETCH_TO_FREE(elt_next);
		/* Only one segment needs to be freed. */
		rte_pktmbuf_free_seg(elt);
		elts_free = elts_free_next;
	}
	txq->cq_ci = cq_ci;
	txq->elts_tail = elts_tail;
	/* Update the consumer index. */
	rte_wmb();
	*txq->cq_db = htonl(cq_ci);
}

/**
 * Get Memory Pool (MP) from mbuf. If mbuf is indirect, the pool from which
 * the cloned mbuf is allocated is returned instead.
 *
 * @param buf
 *   Pointer to mbuf.
 *
 * @return
 *   Memory pool where data is located for given mbuf.
 */
static struct rte_mempool *
txq_mb2mp(struct rte_mbuf *buf)
{
	if (unlikely(RTE_MBUF_INDIRECT(buf)))
		return rte_mbuf_from_indirect(buf)->pool;
	return buf->pool;
}

/**
 * Get Memory Region (MR) <-> Memory Pool (MP) association from txq->mp2mr[].
 * Add MP to txq->mp2mr[] if it's not registered yet. If mp2mr[] is full,
 * remove an entry first.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
static inline uint32_t
txq_mp2mr(struct txq *txq, struct rte_mempool *mp)
{
	unsigned int i;
	uint32_t lkey = (uint32_t)-1;

	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (unlikely(txq->mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq->mp2mr[i].mp == mp) {
			assert(txq->mp2mr[i].lkey != (uint32_t)-1);
			assert(htonl(txq->mp2mr[i].mr->lkey) ==
			       txq->mp2mr[i].lkey);
			lkey = txq->mp2mr[i].lkey;
			break;
		}
	}
	if (unlikely(lkey == (uint32_t)-1))
		lkey = txq_mp2mr_reg(txq, mp, i);
	return lkey;
}

/**
 * Ring TX queue doorbell.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param wqe
 *   Pointer to the last WQE posted in the NIC.
 */
static inline void
mlx5_tx_dbrec(struct txq *txq, volatile struct mlx5_wqe *wqe)
{
	uint64_t *dst = (uint64_t *)((uintptr_t)txq->bf_reg);
	volatile uint64_t *src = ((volatile uint64_t *)wqe);

	rte_wmb();
	*txq->qp_db = htonl(txq->wqe_ci);
	/* Ensure ordering between DB record and BF copy. */
	rte_wmb();
	*dst = *src;
}

/**
 * DPDK callback for TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const unsigned int elts_n = 1 << txq->elts_n;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int max;
	uint16_t max_wqe;
	unsigned int comp;
	volatile struct mlx5_wqe_v *wqe = NULL;
	unsigned int segs_n = 0;
	struct rte_mbuf *buf = NULL;
	uint8_t *raw;

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(*pkts);
	/* Start processing. */
	txq_complete(txq);
	max = (elts_n - (elts_head - txq->elts_tail));
	if (max > elts_n)
		max -= elts_n;
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		volatile rte_v128u32_t *dseg = NULL;
		uint32_t length;
		unsigned int ds = 0;
		uintptr_t addr;
		uint64_t naddr;
		uint16_t pkt_inline_sz = MLX5_WQE_DWORD_SIZE + 2;
		uint16_t ehdr;
		uint8_t cs_flags = 0;
#ifdef MLX5_PMD_SOFT_COUNTERS
		uint32_t total_length = 0;
#endif

		/* first_seg */
		buf = *(pkts++);
		segs_n = buf->nb_segs;
		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max < segs_n + 1)
			break;
		max -= segs_n;
		--segs_n;
		if (!segs_n)
			--pkts_n;
		if (unlikely(--max_wqe == 0))
			break;
		wqe = (volatile struct mlx5_wqe_v *)
			tx_mlx5_wqe(txq, txq->wqe_ci);
		rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		addr = rte_pktmbuf_mtod(buf, uintptr_t);
		length = DATA_LEN(buf);
		ehdr = (((uint8_t *)addr)[1] << 8) |
		       ((uint8_t *)addr)[0];
#ifdef MLX5_PMD_SOFT_COUNTERS
		total_length = length;
#endif
		assert(length >= MLX5_WQE_DWORD_SIZE);
		/* Update element. */
		(*txq->elts)[elts_head] = buf;
		elts_head = (elts_head + 1) & (elts_n - 1);
		/* Prefetch next buffer data. */
		if (pkts_n > 1) {
			volatile void *pkt_addr;

			pkt_addr = rte_pktmbuf_mtod(*pkts, volatile void *);
			rte_prefetch0(pkt_addr);
		}
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
			cs_flags = MLX5_ETH_WQE_L3_CSUM | MLX5_ETH_WQE_L4_CSUM;
		}
		raw = ((uint8_t *)(uintptr_t)wqe) + 2 * MLX5_WQE_DWORD_SIZE;
		/* Replace the Ethernet type by the VLAN if necessary. */
		if (buf->ol_flags & PKT_TX_VLAN_PKT) {
			uint32_t vlan = htonl(0x81000000 | buf->vlan_tci);
			unsigned int len = 2 * ETHER_ADDR_LEN - 2;

			addr += 2;
			length -= 2;
			/* Copy Destination and source mac address. */
			memcpy((uint8_t *)raw, ((uint8_t *)addr), len);
			/* Copy VLAN. */
			memcpy((uint8_t *)raw + len, &vlan, sizeof(vlan));
			/* Copy missing two bytes to end the DSeg. */
			memcpy((uint8_t *)raw + len + sizeof(vlan),
			       ((uint8_t *)addr) + len, 2);
			addr += len + 2;
			length -= (len + 2);
		} else {
			memcpy((uint8_t *)raw, ((uint8_t *)addr) + 2,
			       MLX5_WQE_DWORD_SIZE);
			length -= pkt_inline_sz;
			addr += pkt_inline_sz;
		}
		/* Inline if enough room. */
		if (txq->max_inline) {
			uintptr_t end = (uintptr_t)
				(((uintptr_t)txq->wqes) +
				 (1 << txq->wqe_n) * MLX5_WQE_SIZE);
			unsigned int max_inline = txq->max_inline *
						  RTE_CACHE_LINE_SIZE -
						  MLX5_WQE_DWORD_SIZE;
			uintptr_t addr_end = (addr + max_inline) &
					     ~(RTE_CACHE_LINE_SIZE - 1);
			unsigned int copy_b = (addr_end > addr) ?
				RTE_MIN((addr_end - addr), length) :
				0;

			raw += MLX5_WQE_DWORD_SIZE;
			if (copy_b && ((end - (uintptr_t)raw) > copy_b)) {
				/*
				 * One Dseg remains in the current WQE.  To
				 * keep the computation positive, it is
				 * removed after the bytes to Dseg conversion.
				 */
				uint16_t n = (MLX5_WQE_DS(copy_b) - 1 + 3) / 4;

				if (unlikely(max_wqe < n))
					break;
				max_wqe -= n;
				rte_memcpy((void *)raw, (void *)addr, copy_b);
				addr += copy_b;
				length -= copy_b;
				pkt_inline_sz += copy_b;
			}
			/*
			 * 2 DWORDs consumed by the WQE header + ETH segment +
			 * the size of the inline part of the packet.
			 */
			ds = 2 + MLX5_WQE_DS(pkt_inline_sz - 2);
			if (length > 0) {
				if (ds % (MLX5_WQE_SIZE /
					  MLX5_WQE_DWORD_SIZE) == 0) {
					if (unlikely(--max_wqe == 0))
						break;
					dseg = (volatile rte_v128u32_t *)
					       tx_mlx5_wqe(txq, txq->wqe_ci +
							   ds / 4);
				} else {
					dseg = (volatile rte_v128u32_t *)
						((uintptr_t)wqe +
						 (ds * MLX5_WQE_DWORD_SIZE));
				}
				goto use_dseg;
			} else if (!segs_n) {
				goto next_pkt;
			} else {
				/* dseg will be advance as part of next_seg */
				dseg = (volatile rte_v128u32_t *)
					((uintptr_t)wqe +
					 ((ds - 1) * MLX5_WQE_DWORD_SIZE));
				goto next_seg;
			}
		} else {
			/*
			 * No inline has been done in the packet, only the
			 * Ethernet Header as been stored.
			 */
			dseg = (volatile rte_v128u32_t *)
				((uintptr_t)wqe + (3 * MLX5_WQE_DWORD_SIZE));
			ds = 3;
use_dseg:
			/* Add the remaining packet as a simple ds. */
			naddr = htonll(addr);
			*dseg = (rte_v128u32_t){
				htonl(length),
				txq_mp2mr(txq, txq_mb2mp(buf)),
				naddr,
				naddr >> 32,
			};
			++ds;
			if (!segs_n)
				goto next_pkt;
		}
next_seg:
		assert(buf);
		assert(ds);
		assert(wqe);
		/*
		 * Spill on next WQE when the current one does not have
		 * enough room left. Size of WQE must a be a multiple
		 * of data segment size.
		 */
		assert(!(MLX5_WQE_SIZE % MLX5_WQE_DWORD_SIZE));
		if (!(ds % (MLX5_WQE_SIZE / MLX5_WQE_DWORD_SIZE))) {
			if (unlikely(--max_wqe == 0))
				break;
			dseg = (volatile rte_v128u32_t *)
			       tx_mlx5_wqe(txq, txq->wqe_ci + ds / 4);
			rte_prefetch0(tx_mlx5_wqe(txq,
						  txq->wqe_ci + ds / 4 + 1));
		} else {
			++dseg;
		}
		++ds;
		buf = buf->next;
		assert(buf);
		length = DATA_LEN(buf);
#ifdef MLX5_PMD_SOFT_COUNTERS
		total_length += length;
#endif
		/* Store segment information. */
		naddr = htonll(rte_pktmbuf_mtod(buf, uintptr_t));
		*dseg = (rte_v128u32_t){
			htonl(length),
			txq_mp2mr(txq, txq_mb2mp(buf)),
			naddr,
			naddr >> 32,
		};
		(*txq->elts)[elts_head] = buf;
		elts_head = (elts_head + 1) & (elts_n - 1);
		++j;
		--segs_n;
		if (segs_n)
			goto next_seg;
		else
			--pkts_n;
next_pkt:
		++i;
		/* Initialize known and common part of the WQE structure. */
		wqe->ctrl = (rte_v128u32_t){
			htonl((txq->wqe_ci << 8) | MLX5_OPCODE_SEND),
			htonl(txq->qp_num_8s | ds),
			0,
			0,
		};
		wqe->eseg = (rte_v128u32_t){
			0,
			cs_flags,
			0,
			(ehdr << 16) | htons(pkt_inline_sz),
		};
		txq->wqe_ci += (ds + 3) / 4;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += total_length;
#endif
	} while (pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	comp = txq->elts_comp + i + j;
	if (comp >= MLX5_TX_COMP_THRESH) {
		volatile struct mlx5_wqe_ctrl *w =
			(volatile struct mlx5_wqe_ctrl *)wqe;

		/* Request completion on last WQE. */
		w->ctrl2 = htonl(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		w->ctrl3 = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, (volatile struct mlx5_wqe *)wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Open a MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_mpw_new(struct txq *txq, struct mlx5_mpw *mpw, uint32_t length)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);
	volatile struct mlx5_wqe_data_seg (*dseg)[MLX5_MPW_DSEG_MAX] =
		(volatile struct mlx5_wqe_data_seg (*)[])
		tx_mlx5_wqe(txq, idx + 1);

	mpw->state = MLX5_MPW_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->len = length;
	mpw->total_len = 0;
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->eseg.mss = htons(length);
	mpw->wqe->eseg.inline_hdr_sz = 0;
	mpw->wqe->eseg.rsvd0 = 0;
	mpw->wqe->eseg.rsvd1 = 0;
	mpw->wqe->eseg.rsvd2 = 0;
	mpw->wqe->ctrl[0] = htonl((MLX5_OPC_MOD_MPW << 24) |
				  (txq->wqe_ci << 8) | MLX5_OPCODE_TSO);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	mpw->data.dseg[0] = (volatile struct mlx5_wqe_data_seg *)
		(((uintptr_t)mpw->wqe) + (2 * MLX5_WQE_DWORD_SIZE));
	mpw->data.dseg[1] = (volatile struct mlx5_wqe_data_seg *)
		(((uintptr_t)mpw->wqe) + (3 * MLX5_WQE_DWORD_SIZE));
	mpw->data.dseg[2] = &(*dseg)[0];
	mpw->data.dseg[3] = &(*dseg)[1];
	mpw->data.dseg[4] = &(*dseg)[2];
}

/**
 * Close a MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 */
static inline void
mlx5_mpw_close(struct txq *txq, struct mlx5_mpw *mpw)
{
	unsigned int num = mpw->pkts_n;

	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = htonl(txq->qp_num_8s | (2 + num));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	if (num < 3)
		++txq->wqe_ci;
	else
		txq->wqe_ci += 2;
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
}

/**
 * DPDK callback for TX with MPW support.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_mpw(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const unsigned int elts_n = 1 << txq->elts_n;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int max;
	uint16_t max_wqe;
	unsigned int comp;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
	/* Start processing. */
	txq_complete(txq);
	max = (elts_n - (elts_head - txq->elts_tail));
	if (max > elts_n)
		max -= elts_n;
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *(pkts++);
		unsigned int elts_head_next;
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint32_t cs_flags = 0;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max < segs_n + 1)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX)
			break;
		max -= segs_n;
		--pkts_n;
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))
			cs_flags = MLX5_ETH_WQE_L3_CSUM | MLX5_ETH_WQE_L4_CSUM;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		assert(length);
		/* Start new session if packet differs. */
		if ((mpw.state == MLX5_MPW_STATE_OPENED) &&
		    ((mpw.len != length) ||
		     (segs_n != 1) ||
		     (mpw.wqe->eseg.cs_flags != cs_flags)))
			mlx5_mpw_close(txq, &mpw);
		if (mpw.state == MLX5_MPW_STATE_CLOSED) {
			/*
			 * Multi-Packet WQE consumes at most two WQE.
			 * mlx5_mpw_new() expects to be able to use such
			 * resources.
			 */
			if (unlikely(max_wqe < 2))
				break;
			max_wqe -= 2;
			mlx5_mpw_new(txq, &mpw, length);
			mpw.wqe->eseg.cs_flags = cs_flags;
		}
		/* Multi-segment packets must be alone in their MPW. */
		assert((segs_n == 1) || (mpw.pkts_n == 0));
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
		length = 0;
#endif
		do {
			volatile struct mlx5_wqe_data_seg *dseg;
			uintptr_t addr;

			elts_head_next = (elts_head + 1) & (elts_n - 1);
			assert(buf);
			(*txq->elts)[elts_head] = buf;
			dseg = mpw.data.dseg[mpw.pkts_n];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			*dseg = (struct mlx5_wqe_data_seg){
				.byte_count = htonl(DATA_LEN(buf)),
				.lkey = txq_mp2mr(txq, txq_mb2mp(buf)),
				.addr = htonll(addr),
			};
			elts_head = elts_head_next;
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
			length += DATA_LEN(buf);
#endif
			buf = buf->next;
			++mpw.pkts_n;
			++j;
		} while (--segs_n);
		assert(length == mpw.len);
		if (mpw.pkts_n == MLX5_MPW_DSEG_MAX)
			mlx5_mpw_close(txq, &mpw);
		elts_head = elts_head_next;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	/* "j" includes both packets and segments. */
	comp = txq->elts_comp + j;
	if (comp >= MLX5_TX_COMP_THRESH) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* Request completion on last WQE. */
		wqe->ctrl[2] = htonl(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	if (mpw.state == MLX5_MPW_STATE_OPENED)
		mlx5_mpw_close(txq, &mpw);
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Open a MPW inline session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_mpw_inline_new(struct txq *txq, struct mlx5_mpw *mpw, uint32_t length)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);
	struct mlx5_wqe_inl_small *inl;

	mpw->state = MLX5_MPW_INL_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->len = length;
	mpw->total_len = 0;
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->ctrl[0] = htonl((MLX5_OPC_MOD_MPW << 24) |
				  (txq->wqe_ci << 8) |
				  MLX5_OPCODE_TSO);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	mpw->wqe->eseg.mss = htons(length);
	mpw->wqe->eseg.inline_hdr_sz = 0;
	mpw->wqe->eseg.cs_flags = 0;
	mpw->wqe->eseg.rsvd0 = 0;
	mpw->wqe->eseg.rsvd1 = 0;
	mpw->wqe->eseg.rsvd2 = 0;
	inl = (struct mlx5_wqe_inl_small *)
		(((uintptr_t)mpw->wqe) + 2 * MLX5_WQE_DWORD_SIZE);
	mpw->data.raw = (uint8_t *)&inl->raw;
}

/**
 * Close a MPW inline session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 */
static inline void
mlx5_mpw_inline_close(struct txq *txq, struct mlx5_mpw *mpw)
{
	unsigned int size;
	struct mlx5_wqe_inl_small *inl = (struct mlx5_wqe_inl_small *)
		(((uintptr_t)mpw->wqe) + (2 * MLX5_WQE_DWORD_SIZE));

	size = MLX5_WQE_SIZE - MLX5_MWQE64_INL_DATA + mpw->total_len;
	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = htonl(txq->qp_num_8s | MLX5_WQE_DS(size));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	inl->byte_cnt = htonl(mpw->total_len | MLX5_INLINE_SEG);
	txq->wqe_ci += (size + (MLX5_WQE_SIZE - 1)) / MLX5_WQE_SIZE;
}

/**
 * DPDK callback for TX with MPW inline support.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_mpw_inline(void *dpdk_txq, struct rte_mbuf **pkts,
			 uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const unsigned int elts_n = 1 << txq->elts_n;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int max;
	uint16_t max_wqe;
	unsigned int comp;
	unsigned int inline_room = txq->max_inline * RTE_CACHE_LINE_SIZE;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};
	/*
	 * Compute the maximum number of WQE which can be consumed by inline
	 * code.
	 * - 2 DSEG for:
	 *   - 1 control segment,
	 *   - 1 Ethernet segment,
	 * - N Dseg from the inline request.
	 */
	const unsigned int wqe_inl_n =
		((2 * MLX5_WQE_DWORD_SIZE +
		  txq->max_inline * RTE_CACHE_LINE_SIZE) +
		 RTE_CACHE_LINE_SIZE - 1) / RTE_CACHE_LINE_SIZE;

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
	/* Start processing. */
	txq_complete(txq);
	max = (elts_n - (elts_head - txq->elts_tail));
	if (max > elts_n)
		max -= elts_n;
	do {
		struct rte_mbuf *buf = *(pkts++);
		unsigned int elts_head_next;
		uintptr_t addr;
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint32_t cs_flags = 0;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max < segs_n + 1)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX)
			break;
		max -= segs_n;
		--pkts_n;
		/*
		 * Compute max_wqe in case less WQE were consumed in previous
		 * iteration.
		 */
		max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))
			cs_flags = MLX5_ETH_WQE_L3_CSUM | MLX5_ETH_WQE_L4_CSUM;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		/* Start new session if packet differs. */
		if (mpw.state == MLX5_MPW_STATE_OPENED) {
			if ((mpw.len != length) ||
			    (segs_n != 1) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags))
				mlx5_mpw_close(txq, &mpw);
		} else if (mpw.state == MLX5_MPW_INL_STATE_OPENED) {
			if ((mpw.len != length) ||
			    (segs_n != 1) ||
			    (length > inline_room) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags)) {
				mlx5_mpw_inline_close(txq, &mpw);
				inline_room =
					txq->max_inline * RTE_CACHE_LINE_SIZE;
			}
		}
		if (mpw.state == MLX5_MPW_STATE_CLOSED) {
			if ((segs_n != 1) ||
			    (length > inline_room)) {
				/*
				 * Multi-Packet WQE consumes at most two WQE.
				 * mlx5_mpw_new() expects to be able to use
				 * such resources.
				 */
				if (unlikely(max_wqe < 2))
					break;
				max_wqe -= 2;
				mlx5_mpw_new(txq, &mpw, length);
				mpw.wqe->eseg.cs_flags = cs_flags;
			} else {
				if (unlikely(max_wqe < wqe_inl_n))
					break;
				max_wqe -= wqe_inl_n;
				mlx5_mpw_inline_new(txq, &mpw, length);
				mpw.wqe->eseg.cs_flags = cs_flags;
			}
		}
		/* Multi-segment packets must be alone in their MPW. */
		assert((segs_n == 1) || (mpw.pkts_n == 0));
		if (mpw.state == MLX5_MPW_STATE_OPENED) {
			assert(inline_room ==
			       txq->max_inline * RTE_CACHE_LINE_SIZE);
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
			length = 0;
#endif
			do {
				volatile struct mlx5_wqe_data_seg *dseg;

				elts_head_next =
					(elts_head + 1) & (elts_n - 1);
				assert(buf);
				(*txq->elts)[elts_head] = buf;
				dseg = mpw.data.dseg[mpw.pkts_n];
				addr = rte_pktmbuf_mtod(buf, uintptr_t);
				*dseg = (struct mlx5_wqe_data_seg){
					.byte_count = htonl(DATA_LEN(buf)),
					.lkey = txq_mp2mr(txq, txq_mb2mp(buf)),
					.addr = htonll(addr),
				};
				elts_head = elts_head_next;
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
				length += DATA_LEN(buf);
#endif
				buf = buf->next;
				++mpw.pkts_n;
				++j;
			} while (--segs_n);
			assert(length == mpw.len);
			if (mpw.pkts_n == MLX5_MPW_DSEG_MAX)
				mlx5_mpw_close(txq, &mpw);
		} else {
			unsigned int max;

			assert(mpw.state == MLX5_MPW_INL_STATE_OPENED);
			assert(length <= inline_room);
			assert(length == DATA_LEN(buf));
			elts_head_next = (elts_head + 1) & (elts_n - 1);
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			(*txq->elts)[elts_head] = buf;
			/* Maximum number of bytes before wrapping. */
			max = ((((uintptr_t)(txq->wqes)) +
				(1 << txq->wqe_n) *
				MLX5_WQE_SIZE) -
			       (uintptr_t)mpw.data.raw);
			if (length > max) {
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)addr,
					   max);
				mpw.data.raw = (volatile void *)txq->wqes;
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)(addr + max),
					   length - max);
				mpw.data.raw += length - max;
			} else {
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)addr,
					   length);

				if (length == max)
					mpw.data.raw =
						(volatile void *)txq->wqes;
				else
					mpw.data.raw += length;
			}
			++mpw.pkts_n;
			mpw.total_len += length;
			++j;
			if (mpw.pkts_n == MLX5_MPW_DSEG_MAX) {
				mlx5_mpw_inline_close(txq, &mpw);
				inline_room =
					txq->max_inline * RTE_CACHE_LINE_SIZE;
			} else {
				inline_room -= length;
			}
		}
		elts_head = elts_head_next;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	/* "j" includes both packets and segments. */
	comp = txq->elts_comp + j;
	if (comp >= MLX5_TX_COMP_THRESH) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* Request completion on last WQE. */
		wqe->ctrl[2] = htonl(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	if (mpw.state == MLX5_MPW_INL_STATE_OPENED)
		mlx5_mpw_inline_close(txq, &mpw);
	else if (mpw.state == MLX5_MPW_STATE_OPENED)
		mlx5_mpw_close(txq, &mpw);
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Translate RX completion flags to packet type.
 *
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(volatile struct mlx5_cqe *cqe)
{
	uint32_t pkt_type;
	uint16_t flags = ntohs(cqe->hdr_type_etc);

	if (cqe->pkt_info & MLX5_CQE_RX_TUNNEL_PACKET) {
		pkt_type =
			TRANSPOSE(flags,
				  MLX5_CQE_RX_IPV4_PACKET,
				  RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  MLX5_CQE_RX_IPV6_PACKET,
				  RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN);
		pkt_type |= ((cqe->pkt_info & MLX5_CQE_RX_OUTER_PACKET) ?
			     RTE_PTYPE_L3_IPV6_EXT_UNKNOWN :
			     RTE_PTYPE_L3_IPV4_EXT_UNKNOWN);
	} else {
		pkt_type =
			TRANSPOSE(flags,
				  MLX5_CQE_L3_HDR_TYPE_IPV6,
				  RTE_PTYPE_L3_IPV6_EXT_UNKNOWN) |
			TRANSPOSE(flags,
				  MLX5_CQE_L3_HDR_TYPE_IPV4,
				  RTE_PTYPE_L3_IPV4_EXT_UNKNOWN);
	}
	return pkt_type;
}

/**
 * Get size of the next packet for a given CQE. For compressed CQEs, the
 * consumer index is updated only once all packets of the current one have
 * been processed.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param cqe
 *   CQE to process.
 * @param[out] rss_hash
 *   Packet RSS Hash result.
 *
 * @return
 *   Packet size in bytes (0 if there is none), -1 in case of completion
 *   with error.
 */
static inline int
mlx5_rx_poll_len(struct rxq *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, uint32_t *rss_hash)
{
	struct rxq_zip *zip = &rxq->zip;
	uint16_t cqe_n = cqe_cnt + 1;
	int len = 0;
	uint16_t idx, end;

	/* Process compressed data in the CQE and mini arrays. */
	if (zip->ai) {
		volatile struct mlx5_mini_cqe8 (*mc)[8] =
			(volatile struct mlx5_mini_cqe8 (*)[8])
			(uintptr_t)(&(*rxq->cqes)[zip->ca & cqe_cnt]);

		len = ntohl((*mc)[zip->ai & 7].byte_cnt);
		*rss_hash = ntohl((*mc)[zip->ai & 7].rx_hash_result);
		if ((++zip->ai & 7) == 0) {
			/* Invalidate consumed CQEs */
			idx = zip->ca;
			end = zip->na;
			while (idx != end) {
				(*rxq->cqes)[idx & cqe_cnt].op_own =
					MLX5_CQE_INVALIDATE;
				++idx;
			}
			/*
			 * Increment consumer index to skip the number of
			 * CQEs consumed. Hardware leaves holes in the CQ
			 * ring for software use.
			 */
			zip->ca = zip->na;
			zip->na += 8;
		}
		if (unlikely(rxq->zip.ai == rxq->zip.cqe_cnt)) {
			/* Invalidate the rest */
			idx = zip->ca;
			end = zip->cq_ci;

			while (idx != end) {
				(*rxq->cqes)[idx & cqe_cnt].op_own =
					MLX5_CQE_INVALIDATE;
				++idx;
			}
			rxq->cq_ci = zip->cq_ci;
			zip->ai = 0;
		}
	/* No compressed data, get next CQE and verify if it is compressed. */
	} else {
		int ret;
		int8_t op_own;

		ret = check_cqe(cqe, cqe_n, rxq->cq_ci);
		if (unlikely(ret == 1))
			return 0;
		++rxq->cq_ci;
		op_own = cqe->op_own;
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
			volatile struct mlx5_mini_cqe8 (*mc)[8] =
				(volatile struct mlx5_mini_cqe8 (*)[8])
				(uintptr_t)(&(*rxq->cqes)[rxq->cq_ci &
							  cqe_cnt]);

			/* Fix endianness. */
			zip->cqe_cnt = ntohl(cqe->byte_cnt);
			/*
			 * Current mini array position is the one returned by
			 * check_cqe64().
			 *
			 * If completion comprises several mini arrays, as a
			 * special case the second one is located 7 CQEs after
			 * the initial CQE instead of 8 for subsequent ones.
			 */
			zip->ca = rxq->cq_ci;
			zip->na = zip->ca + 7;
			/* Compute the next non compressed CQE. */
			--rxq->cq_ci;
			zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
			/* Get packet size to return. */
			len = ntohl((*mc)[0].byte_cnt);
			*rss_hash = ntohl((*mc)[0].rx_hash_result);
			zip->ai = 1;
			/* Prefetch all the entries to be invalidated */
			idx = zip->ca;
			end = zip->cq_ci;
			while (idx != end) {
				rte_prefetch0(&(*rxq->cqes)[(idx) & cqe_cnt]);
				++idx;
			}
		} else {
			len = ntohl(cqe->byte_cnt);
			*rss_hash = ntohl(cqe->rx_hash_res);
		}
		/* Error while receiving packet. */
		if (unlikely(MLX5_CQE_OPCODE(op_own) == MLX5_CQE_RESP_ERR))
			return -1;
	}
	return len;
}

/**
 * Translate RX completion flags to offload flags.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @return
 *   Offload flags (ol_flags) for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_ol_flags(struct rxq *rxq, volatile struct mlx5_cqe *cqe)
{
	uint32_t ol_flags = 0;
	uint16_t flags = ntohs(cqe->hdr_type_etc);

	ol_flags =
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L3_HDR_VALID,
			  PKT_RX_IP_CKSUM_GOOD) |
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L4_HDR_VALID,
			  PKT_RX_L4_CKSUM_GOOD);
	if ((cqe->pkt_info & MLX5_CQE_RX_TUNNEL_PACKET) && (rxq->csum_l2tun))
		ol_flags |=
			TRANSPOSE(flags,
				  MLX5_CQE_RX_L3_HDR_VALID,
				  PKT_RX_IP_CKSUM_GOOD) |
			TRANSPOSE(flags,
				  MLX5_CQE_RX_L4_HDR_VALID,
				  PKT_RX_L4_CKSUM_GOOD);
	return ol_flags;
}

/**
 * DPDK callback for RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = dpdk_rxq;
	const unsigned int wqe_cnt = (1 << rxq->elts_n) - 1;
	const unsigned int cqe_cnt = (1 << rxq->cqe_n) - 1;
	const unsigned int sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	volatile struct mlx5_cqe *cqe =
		&(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
	unsigned int i = 0;
	unsigned int rq_ci = rxq->rq_ci << sges_n;
	int len; /* keep its value across iterations. */

	while (pkts_n) {
		unsigned int idx = rq_ci & wqe_cnt;
		volatile struct mlx5_wqe_data_seg *wqe = &(*rxq->wqes)[idx];
		struct rte_mbuf *rep = (*rxq->elts)[idx];
		uint32_t rss_hash_res = 0;

		if (pkt)
			NEXT(seg) = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(cqe);
		rte_prefetch0(wqe);
		rep = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			if (!pkt) {
				/*
				 * no buffers before we even started,
				 * bail out silently.
				 */
				break;
			}
			while (pkt != seg) {
				assert(pkt != (*rxq->elts)[idx]);
				rep = NEXT(pkt);
				rte_mbuf_refcnt_set(pkt, 0);
				__rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			break;
		}
		if (!pkt) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt,
					       &rss_hash_res);
			if (!len) {
				rte_mbuf_refcnt_set(rep, 0);
				__rte_mbuf_raw_free(rep);
				break;
			}
			if (unlikely(len == -1)) {
				/* RX error, packet is likely too large. */
				rte_mbuf_refcnt_set(rep, 0);
				__rte_mbuf_raw_free(rep);
				++rxq->stats.idropped;
				goto skip;
			}
			pkt = seg;
			assert(len >= (rxq->crc_present << 2));
			/* Update packet information. */
			pkt->packet_type = 0;
			pkt->ol_flags = 0;
			if (rss_hash_res && rxq->rss_hash) {
				pkt->hash.rss = rss_hash_res;
				pkt->ol_flags = PKT_RX_RSS_HASH;
			}
			if (rxq->mark &&
			    ((cqe->sop_drop_qpn !=
			      htonl(MLX5_FLOW_MARK_INVALID)) ||
			     (cqe->sop_drop_qpn !=
			      htonl(MLX5_FLOW_MARK_DEFAULT)))) {
				pkt->hash.fdir.hi =
					mlx5_flow_mark_get(cqe->sop_drop_qpn);
				pkt->ol_flags &= ~PKT_RX_RSS_HASH;
				pkt->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
			}
			if (rxq->csum | rxq->csum_l2tun | rxq->vlan_strip |
			    rxq->crc_present) {
				if (rxq->csum) {
					pkt->packet_type =
						rxq_cq_to_pkt_type(cqe);
					pkt->ol_flags |=
						rxq_cq_to_ol_flags(rxq, cqe);
				}
				if (cqe->hdr_type_etc &
				    MLX5_CQE_VLAN_STRIPPED) {
					pkt->ol_flags |= PKT_RX_VLAN_PKT |
						PKT_RX_VLAN_STRIPPED;
					pkt->vlan_tci = ntohs(cqe->vlan_info);
				}
				if (rxq->crc_present)
					len -= ETHER_CRC_LEN;
			}
			PKT_LEN(pkt) = len;
		}
		DATA_LEN(rep) = DATA_LEN(seg);
		PKT_LEN(rep) = PKT_LEN(seg);
		SET_DATA_OFF(rep, DATA_OFF(seg));
		NB_SEGS(rep) = NB_SEGS(seg);
		PORT(rep) = PORT(seg);
		NEXT(rep) = NULL;
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer.  The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		wqe->addr = htonll(rte_pktmbuf_mtod(rep, uintptr_t));
		if (len > DATA_LEN(seg)) {
			len -= DATA_LEN(seg);
			++NB_SEGS(pkt);
			++rq_ci;
			continue;
		}
		DATA_LEN(seg) = len;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += PKT_LEN(pkt);
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++i;
skip:
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely((i == 0) && ((rq_ci >> sges_n) == rxq->rq_ci)))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_wmb();
	*rxq->cq_db = htonl(rxq->cq_ci);
	rte_wmb();
	*rxq->rq_db = htonl(rxq->rq_ci);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

/**
 * Dummy DPDK callback for TX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
removed_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

/**
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
removed_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}
