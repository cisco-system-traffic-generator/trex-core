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
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

static __rte_always_inline uint32_t
rxq_cq_to_pkt_type(volatile struct mlx5_cqe *cqe);

static __rte_always_inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, uint32_t *rss_hash);

static __rte_always_inline uint32_t
rxq_cq_to_ol_flags(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe);

uint32_t mlx5_ptype_table[] __rte_cache_aligned = {
	[0xff] = RTE_PTYPE_ALL_MASK, /* Last entry for errored packet. */
};

/**
 * Build a table to translate Rx completion flags to packet type.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 */
void
mlx5_set_ptype_table(void)
{
	unsigned int i;
	uint32_t (*p)[RTE_DIM(mlx5_ptype_table)] = &mlx5_ptype_table;

	/* Last entry must not be overwritten, reserved for errored packet. */
	for (i = 0; i < RTE_DIM(mlx5_ptype_table) - 1; ++i)
		(*p)[i] = RTE_PTYPE_UNKNOWN;
	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	/* L2 */
	(*p)[0x00] = RTE_PTYPE_L2_ETHER;
	/* L3 */
	(*p)[0x01] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0x02] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	/* Fragmented */
	(*p)[0x21] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0x22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	/* TCP */
	(*p)[0x05] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x06] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	/* UDP */
	(*p)[0x09] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	(*p)[0x0a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	/* Repeat with outer_l3_type being set. Just in case. */
	(*p)[0x81] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0x82] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0xa1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0xa2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0x85] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x86] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x89] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	(*p)[0x8a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	/* Tunneled - L3 */
	(*p)[0x41] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0x42] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0xc1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0xc2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	/* Tunneled - Fragmented */
	(*p)[0x61] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0x62] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0xe1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0xe2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	/* Tunneled - TCP */
	(*p)[0x45] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x46] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xc5] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xc6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	/* Tunneled - UDP */
	(*p)[0x49] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0x4a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0xc9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0xca] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
}

/**
 * Return the size of tailroom of WQ.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param addr
 *   Pointer to tail of WQ.
 *
 * @return
 *   Size of tailroom.
 */
static inline size_t
tx_mlx5_wq_tailroom(struct mlx5_txq_data *txq, void *addr)
{
	size_t tailroom;
	tailroom = (uintptr_t)(txq->wqes) +
		   (1 << txq->wqe_n) * MLX5_WQE_SIZE -
		   (uintptr_t)addr;
	return tailroom;
}

/**
 * Copy data to tailroom of circular queue.
 *
 * @param dst
 *   Pointer to destination.
 * @param src
 *   Pointer to source.
 * @param n
 *   Number of bytes to copy.
 * @param base
 *   Pointer to head of queue.
 * @param tailroom
 *   Size of tailroom from dst.
 *
 * @return
 *   Pointer after copied data.
 */
static inline void *
mlx5_copy_to_wq(void *dst, const void *src, size_t n,
		void *base, size_t tailroom)
{
	void *ret;

	if (n > tailroom) {
		rte_memcpy(dst, src, tailroom);
		rte_memcpy(base, (void *)((uintptr_t)src + tailroom),
			   n - tailroom);
		ret = (uint8_t *)base + n - tailroom;
	} else {
		rte_memcpy(dst, src, n);
		ret = (n == tailroom) ? base : (uint8_t *)dst + n;
	}
	return ret;
}

/**
 * DPDK callback to check the status of a tx descriptor.
 *
 * @param tx_queue
 *   The tx queue.
 * @param[in] offset
 *   The index of the descriptor in the ring.
 *
 * @return
 *   The status of the tx descriptor.
 */
int
mlx5_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct mlx5_txq_data *txq = tx_queue;
	uint16_t used;

	mlx5_tx_complete(txq);
	used = txq->elts_head - txq->elts_tail;
	if (offset < used)
		return RTE_ETH_TX_DESC_FULL;
	return RTE_ETH_TX_DESC_DONE;
}

/**
 * DPDK callback to check the status of a rx descriptor.
 *
 * @param rx_queue
 *   The rx queue.
 * @param[in] offset
 *   The index of the descriptor in the ring.
 *
 * @return
 *   The status of the tx descriptor.
 */
int
mlx5_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct mlx5_rxq_data *rxq = rx_queue;
	struct rxq_zip *zip = &rxq->zip;
	volatile struct mlx5_cqe *cqe;
	const unsigned int cqe_n = (1 << rxq->cqe_n);
	const unsigned int cqe_cnt = cqe_n - 1;
	unsigned int cq_ci;
	unsigned int used;

	/* if we are processing a compressed cqe */
	if (zip->ai) {
		used = zip->cqe_cnt - zip->ca;
		cq_ci = zip->cq_ci;
	} else {
		used = 0;
		cq_ci = rxq->cq_ci;
	}
	cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	while (check_cqe(cqe, cqe_n, cq_ci) == 0) {
		int8_t op_own;
		unsigned int n;

		op_own = cqe->op_own;
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED)
			n = rte_be_to_cpu_32(cqe->byte_cnt);
		else
			n = 1;
		cq_ci += n;
		used += n;
		cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	}
	used = RTE_MIN(used, (1U << rxq->elts_n) - 1);
	if (offset < used)
		return RTE_ETH_RX_DESC_DONE;
	return RTE_ETH_RX_DESC_AVAIL;
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
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	uint16_t max_elts;
	unsigned int max_inline = txq->max_inline;
	const unsigned int inline_en = !!max_inline && txq->inline_en;
	uint16_t max_wqe;
	unsigned int comp;
	volatile struct mlx5_wqe_v *wqe = NULL;
	volatile struct mlx5_wqe_ctrl *last_wqe = NULL;
	unsigned int segs_n = 0;
	struct rte_mbuf *buf = NULL;
	uint8_t *raw;

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(*pkts);
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	/* A CQE slot must always be available. */
	assert((1u << txq->cqe_n) - (txq->cq_pi - txq->cq_ci));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		volatile rte_v128u32_t *dseg = NULL;
		uint32_t length;
		unsigned int ds = 0;
		unsigned int sg = 0; /* counter of additional segs attached. */
		uintptr_t addr;
		uint64_t naddr;
		uint16_t pkt_inline_sz = MLX5_WQE_DWORD_SIZE + 2;
		uint16_t tso_header_sz = 0;
		uint16_t ehdr;
		uint8_t cs_flags = 0;
		uint64_t tso = 0;
		uint16_t tso_segsz = 0;
#ifdef MLX5_PMD_SOFT_COUNTERS
		uint32_t total_length = 0;
#endif

		/* first_seg */
		buf = *pkts;
		segs_n = buf->nb_segs;
		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		max_elts -= segs_n;
		sg = --segs_n;
		if (unlikely(--max_wqe == 0))
			break;
		wqe = (volatile struct mlx5_wqe_v *)
			tx_mlx5_wqe(txq, txq->wqe_ci);
		rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
		if (pkts_n - i > 1)
			rte_prefetch0(*(pkts + 1));
		addr = rte_pktmbuf_mtod(buf, uintptr_t);
		length = DATA_LEN(buf);
		ehdr = (((uint8_t *)addr)[1] << 8) |
		       ((uint8_t *)addr)[0];
#ifdef MLX5_PMD_SOFT_COUNTERS
		total_length = length;
#endif
		if (length < (MLX5_WQE_DWORD_SIZE + 2)) {
			txq->stats.oerrors++;
			break;
		}
		/* Update element. */
		(*txq->elts)[elts_head & elts_m] = buf;
		/* Prefetch next buffer data. */
		if (pkts_n - i > 1)
			rte_prefetch0(
			    rte_pktmbuf_mtod(*(pkts + 1), volatile void *));
		/* Should we enable HW CKSUM offload */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
			const uint64_t is_tunneled = buf->ol_flags &
						     (PKT_TX_TUNNEL_GRE |
						      PKT_TX_TUNNEL_VXLAN);

			if (is_tunneled && txq->tunnel_en) {
				cs_flags = MLX5_ETH_WQE_L3_INNER_CSUM |
					   MLX5_ETH_WQE_L4_INNER_CSUM;
				if (buf->ol_flags & PKT_TX_OUTER_IP_CKSUM)
					cs_flags |= MLX5_ETH_WQE_L3_CSUM;
			} else {
				cs_flags = MLX5_ETH_WQE_L3_CSUM |
					   MLX5_ETH_WQE_L4_CSUM;
			}
		}
		raw = ((uint8_t *)(uintptr_t)wqe) + 2 * MLX5_WQE_DWORD_SIZE;
		/* Replace the Ethernet type by the VLAN if necessary. */
		if (buf->ol_flags & PKT_TX_VLAN_PKT) {
			uint32_t vlan = rte_cpu_to_be_32(0x81000000 |
							 buf->vlan_tci);
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
		raw += MLX5_WQE_DWORD_SIZE;
		tso = txq->tso_en && (buf->ol_flags & PKT_TX_TCP_SEG);
		if (tso) {
			uintptr_t end =
				(uintptr_t)(((uintptr_t)txq->wqes) +
					    (1 << txq->wqe_n) * MLX5_WQE_SIZE);
			unsigned int copy_b;
			uint8_t vlan_sz =
				(buf->ol_flags & PKT_TX_VLAN_PKT) ? 4 : 0;
			const uint64_t is_tunneled =
				buf->ol_flags & (PKT_TX_TUNNEL_GRE |
						 PKT_TX_TUNNEL_VXLAN);

			tso_header_sz = buf->l2_len + vlan_sz +
					buf->l3_len + buf->l4_len;
			tso_segsz = buf->tso_segsz;
			if (unlikely(tso_segsz == 0)) {
				txq->stats.oerrors++;
				break;
			}
			if (is_tunneled	&& txq->tunnel_en) {
				tso_header_sz += buf->outer_l2_len +
						 buf->outer_l3_len;
				cs_flags |= MLX5_ETH_WQE_L4_INNER_CSUM;
			} else {
				cs_flags |= MLX5_ETH_WQE_L4_CSUM;
			}
			if (unlikely(tso_header_sz > MLX5_MAX_TSO_HEADER)) {
				txq->stats.oerrors++;
				break;
			}
			copy_b = tso_header_sz - pkt_inline_sz;
			/* First seg must contain all headers. */
			assert(copy_b <= length);
			if (copy_b && ((end - (uintptr_t)raw) > copy_b)) {
				uint16_t n = (MLX5_WQE_DS(copy_b) - 1 + 3) / 4;

				if (unlikely(max_wqe < n))
				break;
				max_wqe -= n;
				rte_memcpy((void *)raw, (void *)addr, copy_b);
				addr += copy_b;
				length -= copy_b;
				/* Include padding for TSO header. */
				copy_b = MLX5_WQE_DS(copy_b) *
					 MLX5_WQE_DWORD_SIZE;
				pkt_inline_sz += copy_b;
				raw += copy_b;
			} else {
				/* NOP WQE. */
				wqe->ctrl = (rte_v128u32_t){
					rte_cpu_to_be_32(txq->wqe_ci << 8),
					rte_cpu_to_be_32(txq->qp_num_8s | 1),
					0,
					0,
				};
				ds = 1;
#ifdef MLX5_PMD_SOFT_COUNTERS
				total_length = 0;
#endif
				k++;
				goto next_wqe;
		}
		}
		/* Inline if enough room. */
		if (inline_en || tso) {
			uint32_t inl = 0;
			uintptr_t end = (uintptr_t)
				(((uintptr_t)txq->wqes) +
				 (1 << txq->wqe_n) * MLX5_WQE_SIZE);
			unsigned int inline_room = max_inline *
						   RTE_CACHE_LINE_SIZE -
						   (pkt_inline_sz - 2) -
						   !!tso * sizeof(inl);
			uintptr_t addr_end;
			unsigned int copy_b;

pkt_inline:
			addr_end = RTE_ALIGN_FLOOR(addr + inline_room,
						   RTE_CACHE_LINE_SIZE);
			copy_b = (addr_end > addr) ?
				 RTE_MIN((addr_end - addr), length) : 0;
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
				if (tso && !inl) {
					inl = rte_cpu_to_be_32(copy_b |
							       MLX5_INLINE_SEG);
					rte_memcpy((void *)raw,
						   (void *)&inl, sizeof(inl));
					raw += sizeof(inl);
					pkt_inline_sz += sizeof(inl);
				}
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
				raw += copy_b;
				inline_room -= copy_b;
				--segs_n;
				buf = buf->next;
				assert(buf);
				addr = rte_pktmbuf_mtod(buf, uintptr_t);
				length = DATA_LEN(buf);
#ifdef MLX5_PMD_SOFT_COUNTERS
				total_length += length;
#endif
				(*txq->elts)[++elts_head & elts_m] = buf;
				goto pkt_inline;
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
			naddr = rte_cpu_to_be_64(addr);
			*dseg = (rte_v128u32_t){
				rte_cpu_to_be_32(length),
				mlx5_tx_mb2mr(txq, buf),
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
		naddr = rte_cpu_to_be_64(rte_pktmbuf_mtod(buf, uintptr_t));
		*dseg = (rte_v128u32_t){
			rte_cpu_to_be_32(length),
			mlx5_tx_mb2mr(txq, buf),
			naddr,
			naddr >> 32,
		};
		(*txq->elts)[++elts_head & elts_m] = buf;
		if (--segs_n)
            goto next_seg;
next_pkt:
		if (ds > MLX5_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		++elts_head;
		++pkts;
		++i;
		j += sg;
		/* Initialize known and common part of the WQE structure. */
		if (tso) {
			wqe->ctrl = (rte_v128u32_t){
				rte_cpu_to_be_32((txq->wqe_ci << 8) |
						 MLX5_OPCODE_TSO),
				rte_cpu_to_be_32(txq->qp_num_8s | ds),
				0,
				0,
			};
			wqe->eseg = (rte_v128u32_t){
				0,
				cs_flags | (rte_cpu_to_be_16(tso_segsz) << 16),
				0,
				(ehdr << 16) | rte_cpu_to_be_16(tso_header_sz),
			};
		} else {
			wqe->ctrl = (rte_v128u32_t){
				rte_cpu_to_be_32((txq->wqe_ci << 8) |
						 MLX5_OPCODE_SEND),
				rte_cpu_to_be_32(txq->qp_num_8s | ds),
				0,
				0,
			};
			wqe->eseg = (rte_v128u32_t){
				0,
				cs_flags,
				0,
				(ehdr << 16) | rte_cpu_to_be_16(pkt_inline_sz),
			};
		}
next_wqe:
		txq->wqe_ci += (ds + 3) / 4;
		/* Save the last successful WQE for completion request */
		last_wqe = (volatile struct mlx5_wqe_ctrl *)wqe;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += total_length;
#endif
	} while (i < pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely((i + k) == 0))
		return 0;
	txq->elts_head += (i + j);
	/* Check whether completion threshold has been reached. */
	comp = txq->elts_comp + i + j + k;
	if (comp >= MLX5_TX_COMP_THRESH) {
		/* Request completion on last WQE. */
		last_wqe->ctrl2 = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		last_wqe->ctrl3 = txq->elts_head;
		txq->elts_comp = 0;
#ifndef NDEBUG
		++txq->cq_pi;
#endif
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, (volatile struct mlx5_wqe *)last_wqe);
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
mlx5_mpw_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw, uint32_t length)
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
	mpw->wqe->eseg.mss = rte_cpu_to_be_16(length);
	mpw->wqe->eseg.inline_hdr_sz = 0;
	mpw->wqe->eseg.rsvd0 = 0;
	mpw->wqe->eseg.rsvd1 = 0;
	mpw->wqe->eseg.rsvd2 = 0;
	mpw->wqe->ctrl[0] = rte_cpu_to_be_32((MLX5_OPC_MOD_MPW << 24) |
					     (txq->wqe_ci << 8) |
					     MLX5_OPCODE_TSO);
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
mlx5_mpw_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	unsigned int num = mpw->pkts_n;

	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s | (2 + num));
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
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
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
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	/* A CQE slot must always be available. */
	assert((1u << txq->cqe_n) - (txq->cq_pi - txq->cq_ci));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *(pkts++);
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint32_t cs_flags = 0;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		max_elts -= segs_n;
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

			assert(buf);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			dseg = mpw.data.dseg[mpw.pkts_n];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			*dseg = (struct mlx5_wqe_data_seg){
				.byte_count = rte_cpu_to_be_32(DATA_LEN(buf)),
				.lkey = mlx5_tx_mb2mr(txq, buf),
				.addr = rte_cpu_to_be_64(addr),
			};
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
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
#ifndef NDEBUG
		++txq->cq_pi;
#endif
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
mlx5_mpw_inline_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw,
		    uint32_t length)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);
	struct mlx5_wqe_inl_small *inl;

	mpw->state = MLX5_MPW_INL_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->len = length;
	mpw->total_len = 0;
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->ctrl[0] = rte_cpu_to_be_32((MLX5_OPC_MOD_MPW << 24) |
					     (txq->wqe_ci << 8) |
					     MLX5_OPCODE_TSO);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	mpw->wqe->eseg.mss = rte_cpu_to_be_16(length);
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
mlx5_mpw_inline_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	unsigned int size;
	struct mlx5_wqe_inl_small *inl = (struct mlx5_wqe_inl_small *)
		(((uintptr_t)mpw->wqe) + (2 * MLX5_WQE_DWORD_SIZE));

	size = MLX5_WQE_SIZE - MLX5_MWQE64_INL_DATA + mpw->total_len;
	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s |
					     MLX5_WQE_DS(size));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	inl->byte_cnt = rte_cpu_to_be_32(mpw->total_len | MLX5_INLINE_SEG);
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
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
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
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	/* A CQE slot must always be available. */
	assert((1u << txq->cqe_n) - (txq->cq_pi - txq->cq_ci));
	do {
		struct rte_mbuf *buf = *(pkts++);
		uintptr_t addr;
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint32_t cs_flags = 0;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		max_elts -= segs_n;
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

				assert(buf);
				(*txq->elts)[elts_head++ & elts_m] = buf;
				dseg = mpw.data.dseg[mpw.pkts_n];
				addr = rte_pktmbuf_mtod(buf, uintptr_t);
				*dseg = (struct mlx5_wqe_data_seg){
					.byte_count =
					       rte_cpu_to_be_32(DATA_LEN(buf)),
					.lkey = mlx5_tx_mb2mr(txq, buf),
					.addr = rte_cpu_to_be_64(addr),
				};
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
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			(*txq->elts)[elts_head++ & elts_m] = buf;
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
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
#ifndef NDEBUG
		++txq->cq_pi;
#endif
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
 * Open an Enhanced MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_empw_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw, int padding)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);

	mpw->state = MLX5_MPW_ENHANCED_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->total_len = sizeof(struct mlx5_wqe);
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->ctrl[0] =
		rte_cpu_to_be_32((MLX5_OPC_MOD_ENHANCED_MPSW << 24) |
				 (txq->wqe_ci << 8) |
				 MLX5_OPCODE_ENHANCED_MPSW);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	memset((void *)(uintptr_t)&mpw->wqe->eseg, 0, MLX5_WQE_DWORD_SIZE);
	if (unlikely(padding)) {
		uintptr_t addr = (uintptr_t)(mpw->wqe + 1);

		/* Pad the first 2 DWORDs with zero-length inline header. */
		*(volatile uint32_t *)addr = rte_cpu_to_be_32(MLX5_INLINE_SEG);
		*(volatile uint32_t *)(addr + MLX5_WQE_DWORD_SIZE) =
			rte_cpu_to_be_32(MLX5_INLINE_SEG);
		mpw->total_len += 2 * MLX5_WQE_DWORD_SIZE;
		/* Start from the next WQEBB. */
		mpw->data.raw = (volatile void *)(tx_mlx5_wqe(txq, idx + 1));
	} else {
		mpw->data.raw = (volatile void *)(mpw->wqe + 1);
	}
}

/**
 * Close an Enhanced MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 *
 * @return
 *   Number of consumed WQEs.
 */
static inline uint16_t
mlx5_empw_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	uint16_t ret;

	/* Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s |
					     MLX5_WQE_DS(mpw->total_len));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	ret = (mpw->total_len + (MLX5_WQE_SIZE - 1)) / MLX5_WQE_SIZE;
	txq->wqe_ci += ret;
	return ret;
}

/**
 * TX with Enhanced MPW support.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static inline uint16_t
txq_burst_empw(struct mlx5_txq_data *txq, struct rte_mbuf **pkts,
	       uint16_t pkts_n)
{
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
	uint16_t max_wqe;
	unsigned int max_inline = txq->max_inline * RTE_CACHE_LINE_SIZE;
	unsigned int mpw_room = 0;
	unsigned int inl_pad = 0;
	uint32_t inl_hdr;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};

	if (unlikely(!pkts_n))
		return 0;
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	/* A CQE slot must always be available. */
	assert((1u << txq->cqe_n) - (txq->cq_pi - txq->cq_ci));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *(pkts++);
		uintptr_t addr;
		uint64_t naddr;
		unsigned int n;
		unsigned int do_inline = 0; /* Whether inline is possible. */
		uint32_t length;
		uint32_t cs_flags = 0;

		/* Multi-segmented packet is handled in slow-path outside. */
		assert(NB_SEGS(buf) == 1);
		/* Make sure there is enough room to store this packet. */
		if (max_elts - j == 0)
			break;
		/* Should we enable HW CKSUM offload. */
		if (buf->ol_flags &
		    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))
			cs_flags = MLX5_ETH_WQE_L3_CSUM | MLX5_ETH_WQE_L4_CSUM;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		/* Start new session if:
		 * - multi-segment packet
		 * - no space left even for a dseg
		 * - next packet can be inlined with a new WQE
		 * - cs_flag differs
		 */
		if (mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED) {
			if ((inl_pad + sizeof(struct mlx5_wqe_data_seg) >
			     mpw_room) ||
                (length <= txq->inline_max_packet_sz &&
			     inl_pad + sizeof(inl_hdr) + length >
			     mpw_room) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags))
				max_wqe -= mlx5_empw_close(txq, &mpw);
		}
		if (unlikely(mpw.state == MLX5_MPW_STATE_CLOSED)) {
			/* In Enhanced MPW, inline as much as the budget is
			 * allowed. The remaining space is to be filled with
			 * dsegs. If the title WQEBB isn't padded, it will have
			 * 2 dsegs there.
			 */
			mpw_room = RTE_MIN(MLX5_WQE_SIZE_MAX,
					   (max_inline ? max_inline :
					    pkts_n * MLX5_WQE_DWORD_SIZE) +
					   MLX5_WQE_SIZE);
			if (unlikely(max_wqe * MLX5_WQE_SIZE < mpw_room))
				break;
			/* Don't pad the title WQEBB to not waste WQ. */
			mlx5_empw_new(txq, &mpw, 0);
			mpw_room -= mpw.total_len;
			inl_pad = 0;
			do_inline = length <= txq->inline_max_packet_sz &&
				    sizeof(inl_hdr) + length <= mpw_room &&
				    !txq->mpw_hdr_dseg;
			mpw.wqe->eseg.cs_flags = cs_flags;
		} else {
			/* Evaluate whether the next packet can be inlined.
			 * Inlininig is possible when:
			 * - length is less than configured value
			 * - length fits for remaining space
			 * - not required to fill the title WQEBB with dsegs
			 */
			do_inline =
				length <= txq->inline_max_packet_sz &&
				inl_pad + sizeof(inl_hdr) + length <=
				 mpw_room &&
				(!txq->mpw_hdr_dseg ||
				 mpw.total_len >= MLX5_WQE_SIZE);
		}
		if (do_inline) {
			/* Inline packet into WQE. */
			unsigned int max;

			assert(mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED);
			assert(length == DATA_LEN(buf));
			inl_hdr = rte_cpu_to_be_32(length | MLX5_INLINE_SEG);
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			mpw.data.raw = (volatile void *)
				((uintptr_t)mpw.data.raw + inl_pad);
			max = tx_mlx5_wq_tailroom(txq,
					(void *)(uintptr_t)mpw.data.raw);
			/* Copy inline header. */
			mpw.data.raw = (volatile void *)
				mlx5_copy_to_wq(
					  (void *)(uintptr_t)mpw.data.raw,
					  &inl_hdr,
					  sizeof(inl_hdr),
					  (void *)(uintptr_t)txq->wqes,
					  max);
			max = tx_mlx5_wq_tailroom(txq,
					(void *)(uintptr_t)mpw.data.raw);
			/* Copy packet data. */
			mpw.data.raw = (volatile void *)
				mlx5_copy_to_wq(
					  (void *)(uintptr_t)mpw.data.raw,
					  (void *)addr,
					  length,
					  (void *)(uintptr_t)txq->wqes,
					  max);
			++mpw.pkts_n;
			mpw.total_len += (inl_pad + sizeof(inl_hdr) + length);
			/* No need to get completion as the entire packet is
			 * copied to WQ. Free the buf right away.
			 */
			rte_pktmbuf_free_seg(buf);
			mpw_room -= (inl_pad + sizeof(inl_hdr) + length);
			/* Add pad in the next packet if any. */
			inl_pad = (((uintptr_t)mpw.data.raw +
					(MLX5_WQE_DWORD_SIZE - 1)) &
					~(MLX5_WQE_DWORD_SIZE - 1)) -
				  (uintptr_t)mpw.data.raw;
		} else {
			/* No inline. Load a dseg of packet pointer. */
			volatile rte_v128u32_t *dseg;

			assert(mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED);
			assert((inl_pad + sizeof(*dseg)) <= mpw_room);
			assert(length == DATA_LEN(buf));
			if (!tx_mlx5_wq_tailroom(txq,
					(void *)((uintptr_t)mpw.data.raw
						+ inl_pad)))
				dseg = (volatile void *)txq->wqes;
			else
				dseg = (volatile void *)
					((uintptr_t)mpw.data.raw +
					 inl_pad);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			for (n = 0; n * RTE_CACHE_LINE_SIZE < length; n++)
				rte_prefetch2((void *)(addr +
						n * RTE_CACHE_LINE_SIZE));
			naddr = rte_cpu_to_be_64(addr);
			*dseg = (rte_v128u32_t) {
				rte_cpu_to_be_32(length),
				mlx5_tx_mb2mr(txq, buf),
				naddr,
				naddr >> 32,
			};
			mpw.data.raw = (volatile void *)(dseg + 1);
			mpw.total_len += (inl_pad + sizeof(*dseg));
			++j;
			++mpw.pkts_n;
			mpw_room -= (inl_pad + sizeof(*dseg));
			inl_pad = 0;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (i < pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	if (txq->elts_comp + j >= MLX5_TX_COMP_THRESH ||
			(uint16_t)(txq->wqe_ci - txq->mpw_comp) >=
			 (1 << txq->wqe_n) / MLX5_TX_COMP_THRESH_INLINE_DIV) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* Request completion on last WQE. */
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
		txq->mpw_comp = txq->wqe_ci;
#ifndef NDEBUG
		++txq->cq_pi;
#endif
	} else {
		txq->elts_comp += j;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	if (mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED)
		mlx5_empw_close(txq, &mpw);
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * DPDK callback for TX with Enhanced MPW support.
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
mlx5_tx_burst_empw(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint16_t n;
		uint16_t ret;

		n = txq_count_contig_multi_seg(&pkts[nb_tx], pkts_n - nb_tx);
		if (n) {
			ret = mlx5_tx_burst(dpdk_txq, &pkts[nb_tx], n);
			if (!ret)
				break;
			nb_tx += ret;
		}
		n = txq_count_contig_single_seg(&pkts[nb_tx], pkts_n - nb_tx);
		if (n) {
			ret = txq_burst_empw(txq, &pkts[nb_tx], n);
			if (!ret)
				break;
			nb_tx += ret;
		}
	}
	return nb_tx;
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
	uint8_t idx;
	uint8_t pinfo = cqe->pkt_info;
	uint16_t ptype = cqe->hdr_type_etc;

	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	idx = ((pinfo & 0x3) << 6) | ((ptype & 0xfc00) >> 10);
	return mlx5_ptype_table[idx];
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
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
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
			(uintptr_t)(&(*rxq->cqes)[zip->ca & cqe_cnt].pkt_info);

		len = rte_be_to_cpu_32((*mc)[zip->ai & 7].byte_cnt);
		*rss_hash = rte_be_to_cpu_32((*mc)[zip->ai & 7].rx_hash_result);
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
							  cqe_cnt].pkt_info);

			/* Fix endianness. */
			zip->cqe_cnt = rte_be_to_cpu_32(cqe->byte_cnt);
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
			len = rte_be_to_cpu_32((*mc)[0].byte_cnt);
			*rss_hash = rte_be_to_cpu_32((*mc)[0].rx_hash_result);
			zip->ai = 1;
			/* Prefetch all the entries to be invalidated */
			idx = zip->ca;
			end = zip->cq_ci;
			while (idx != end) {
				rte_prefetch0(&(*rxq->cqes)[(idx) & cqe_cnt]);
				++idx;
			}
		} else {
			len = rte_be_to_cpu_32(cqe->byte_cnt);
			*rss_hash = rte_be_to_cpu_32(cqe->rx_hash_res);
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
rxq_cq_to_ol_flags(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe)
{
	uint32_t ol_flags = 0;
	uint16_t flags = rte_be_to_cpu_16(cqe->hdr_type_etc);

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
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	const unsigned int wqe_cnt = (1 << rxq->elts_n) - 1;
	const unsigned int cqe_cnt = (1 << rxq->cqe_n) - 1;
	const unsigned int sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	volatile struct mlx5_cqe *cqe =
		&(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
	unsigned int i = 0;
	unsigned int rq_ci = rxq->rq_ci << sges_n;
	int len = 0; /* keep its value across iterations. */

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
				NEXT(pkt) = NULL;
				NB_SEGS(pkt) = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			break;
		}
		if (!pkt) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt,
					       &rss_hash_res);
			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}
			if (unlikely(len == -1)) {
				/* RX error, packet is likely too large. */
				rte_mbuf_raw_free(rep);
				++rxq->stats.idropped;
				goto skip;
			}
			pkt = seg;
			assert(len >= (rxq->crc_present << 2));
			/* Update packet information. */
			pkt->packet_type = rxq_cq_to_pkt_type(cqe);
			pkt->ol_flags = 0;
			if (rss_hash_res && rxq->rss_hash) {
				pkt->hash.rss = rss_hash_res;
				pkt->ol_flags = PKT_RX_RSS_HASH;
			}
			if (rxq->mark &&
			    MLX5_FLOW_MARK_IS_VALID(cqe->sop_drop_qpn)) {
				pkt->ol_flags |= PKT_RX_FDIR;
				if (cqe->sop_drop_qpn !=
				    rte_cpu_to_be_32(MLX5_FLOW_MARK_DEFAULT)) {
					uint32_t mark = cqe->sop_drop_qpn;

					pkt->ol_flags |= PKT_RX_FDIR_ID;
					pkt->hash.fdir.hi =
						mlx5_flow_mark_get(mark);
				}
			}
			if (rxq->csum | rxq->csum_l2tun)
				pkt->ol_flags |= rxq_cq_to_ol_flags(rxq, cqe);
			if (rxq->vlan_strip &&
			    (cqe->hdr_type_etc &
			     rte_cpu_to_be_16(MLX5_CQE_VLAN_STRIPPED))) {
				pkt->ol_flags |= PKT_RX_VLAN |
					PKT_RX_VLAN_STRIPPED;
				pkt->vlan_tci =
					rte_be_to_cpu_16(cqe->vlan_info);
			}
			if (rxq->hw_timestamp) {
				pkt->timestamp =
					rte_be_to_cpu_64(cqe->timestamp);
				pkt->ol_flags |= PKT_RX_TIMESTAMP;
			}
			if (rxq->crc_present)
				len -= ETHER_CRC_LEN;
			PKT_LEN(pkt) = len;
		}
		DATA_LEN(rep) = DATA_LEN(seg);
		PKT_LEN(rep) = PKT_LEN(seg);
		SET_DATA_OFF(rep, DATA_OFF(seg));
		PORT(rep) = PORT(seg);
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer.  The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		wqe->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(rep, uintptr_t));
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
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
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

/*
 * Vectorized Rx/Tx routines are not compiled in when required vector
 * instructions are not supported on a target architecture. The following null
 * stubs are needed for linkage when those are not included outside of this file
 * (e.g.  mlx5_rxtx_vec_sse.c for x86).
 */

uint16_t __attribute__((weak))
mlx5_tx_burst_raw_vec(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

uint16_t __attribute__((weak))
mlx5_tx_burst_vec(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_txq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

uint16_t __attribute__((weak))
mlx5_rx_burst_vec(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	(void)dpdk_rxq;
	(void)pkts;
	(void)pkts_n;
	return 0;
}

int __attribute__((weak))
priv_check_raw_vec_tx_support(struct priv *priv)
{
	(void)priv;
	return -ENOTSUP;
}

int __attribute__((weak))
priv_check_vec_tx_support(struct priv *priv)
{
	(void)priv;
	return -ENOTSUP;
}

int __attribute__((weak))
rxq_check_vec_support(struct mlx5_rxq_data *rxq)
{
	(void)rxq;
	return -ENOTSUP;
}

int __attribute__((weak))
priv_check_vec_rx_support(struct priv *priv)
{
	(void)priv;
	return -ENOTSUP;
}
