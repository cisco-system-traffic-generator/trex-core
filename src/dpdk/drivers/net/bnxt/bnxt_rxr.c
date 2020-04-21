/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitmap.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_rxr.h"
#include "bnxt_rxq.h"
#include "hsi_struct_def_dpdk.h"
#ifdef RTE_LIBRTE_IEEE1588
#include "bnxt_hwrm.h"
#endif

/*
 * RX Ring handling
 */

static inline struct rte_mbuf *__bnxt_alloc_rx_data(struct rte_mempool *mb)
{
	struct rte_mbuf *data;

	data = rte_mbuf_raw_alloc(mb);

	return data;
}

static inline int bnxt_alloc_rx_data(struct bnxt_rx_queue *rxq,
				     struct bnxt_rx_ring_info *rxr,
				     uint16_t prod)
{
	struct rx_prod_pkt_bd *rxbd = &rxr->rx_desc_ring[prod];
	struct bnxt_sw_rx_bd *rx_buf = &rxr->rx_buf_ring[prod];
	struct rte_mbuf *mbuf;

	mbuf = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!mbuf) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	rx_buf->mbuf = mbuf;
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;

	rxbd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	return 0;
}

static inline int bnxt_alloc_ag_data(struct bnxt_rx_queue *rxq,
				     struct bnxt_rx_ring_info *rxr,
				     uint16_t prod)
{
	struct rx_prod_pkt_bd *rxbd = &rxr->ag_desc_ring[prod];
	struct bnxt_sw_rx_bd *rx_buf = &rxr->ag_buf_ring[prod];
	struct rte_mbuf *mbuf;

	if (rxbd == NULL) {
		PMD_DRV_LOG(ERR, "Jumbo Frame. rxbd is NULL\n");
		return -EINVAL;
	}

	if (rx_buf == NULL) {
		PMD_DRV_LOG(ERR, "Jumbo Frame. rx_buf is NULL\n");
		return -EINVAL;
	}

	mbuf = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!mbuf) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	rx_buf->mbuf = mbuf;
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;

	rxbd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	return 0;
}

static inline void bnxt_reuse_rx_mbuf(struct bnxt_rx_ring_info *rxr,
			       struct rte_mbuf *mbuf)
{
	uint16_t prod = RING_NEXT(rxr->rx_ring_struct, rxr->rx_prod);
	struct bnxt_sw_rx_bd *prod_rx_buf;
	struct rx_prod_pkt_bd *prod_bd;

	prod_rx_buf = &rxr->rx_buf_ring[prod];

	RTE_ASSERT(prod_rx_buf->mbuf == NULL);
	RTE_ASSERT(mbuf != NULL);

	prod_rx_buf->mbuf = mbuf;

	prod_bd = &rxr->rx_desc_ring[prod];

	prod_bd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	rxr->rx_prod = prod;
}

static inline
struct rte_mbuf *bnxt_consume_rx_buf(struct bnxt_rx_ring_info *rxr,
				     uint16_t cons)
{
	struct bnxt_sw_rx_bd *cons_rx_buf;
	struct rte_mbuf *mbuf;

	cons_rx_buf = &rxr->rx_buf_ring[cons];
	RTE_ASSERT(cons_rx_buf->mbuf != NULL);
	mbuf = cons_rx_buf->mbuf;
	cons_rx_buf->mbuf = NULL;
	return mbuf;
}

static void bnxt_tpa_start(struct bnxt_rx_queue *rxq,
			   struct rx_tpa_start_cmpl *tpa_start,
			   struct rx_tpa_start_cmpl_hi *tpa_start1)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t agg_id;
	uint16_t data_cons;
	struct bnxt_tpa_info *tpa_info;
	struct rte_mbuf *mbuf;

	agg_id = bnxt_tpa_start_agg_id(rxq->bp, tpa_start);

	data_cons = tpa_start->opaque;
	tpa_info = &rxr->tpa_info[agg_id];

	mbuf = bnxt_consume_rx_buf(rxr, data_cons);

	bnxt_reuse_rx_mbuf(rxr, tpa_info->mbuf);

	tpa_info->agg_count = 0;
	tpa_info->mbuf = mbuf;
	tpa_info->len = rte_le_to_cpu_32(tpa_start->len);

	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rte_le_to_cpu_32(tpa_start->len);
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;
	mbuf->ol_flags = PKT_RX_LRO;
	if (likely(tpa_start->flags_type &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS_RSS_VALID))) {
		mbuf->hash.rss = rte_le_to_cpu_32(tpa_start->rss_hash);
		mbuf->ol_flags |= PKT_RX_RSS_HASH;
	} else {
		mbuf->hash.fdir.id = rte_le_to_cpu_16(tpa_start1->cfa_code);
		mbuf->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
	}
	if (tpa_start1->flags2 &
	    rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_META_FORMAT_VLAN)) {
		mbuf->vlan_tci = rte_le_to_cpu_32(tpa_start1->metadata);
		mbuf->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
	}
	if (likely(tpa_start1->flags2 &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_L4_CS_CALC)))
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;

	/* recycle next mbuf */
	data_cons = RING_NEXT(rxr->rx_ring_struct, data_cons);
	bnxt_reuse_rx_mbuf(rxr, bnxt_consume_rx_buf(rxr, data_cons));
}

static int bnxt_agg_bufs_valid(struct bnxt_cp_ring_info *cpr,
		uint8_t agg_bufs, uint32_t raw_cp_cons)
{
	uint16_t last_cp_cons;
	struct rx_pkt_cmpl *agg_cmpl;

	raw_cp_cons = ADV_RAW_CMP(raw_cp_cons, agg_bufs);
	last_cp_cons = RING_CMP(cpr->cp_ring_struct, raw_cp_cons);
	agg_cmpl = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[last_cp_cons];
	cpr->valid = FLIP_VALID(raw_cp_cons,
				cpr->cp_ring_struct->ring_mask,
				cpr->valid);
	return CMP_VALID(agg_cmpl, raw_cp_cons, cpr->cp_ring_struct);
}

/* TPA consume agg buffer out of order, allocate connected data only */
static int bnxt_prod_ag_mbuf(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t next = RING_NEXT(rxr->ag_ring_struct, rxr->ag_prod);

	/* TODO batch allocation for better performance */
	while (rte_bitmap_get(rxr->ag_bitmap, next)) {
		if (unlikely(bnxt_alloc_ag_data(rxq, rxr, next))) {
			PMD_DRV_LOG(ERR,
				"agg mbuf alloc failed: prod=0x%x\n", next);
			break;
		}
		rte_bitmap_clear(rxr->ag_bitmap, next);
		rxr->ag_prod = next;
		next = RING_NEXT(rxr->ag_ring_struct, next);
	}

	return 0;
}

static int bnxt_rx_pages(struct bnxt_rx_queue *rxq,
			 struct rte_mbuf *mbuf, uint32_t *tmp_raw_cons,
			 uint8_t agg_buf, struct bnxt_tpa_info *tpa_info)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	int i;
	uint16_t cp_cons, ag_cons;
	struct rx_pkt_cmpl *rxcmp;
	struct rte_mbuf *last = mbuf;
	bool is_thor_tpa = tpa_info && BNXT_CHIP_THOR(rxq->bp);

	for (i = 0; i < agg_buf; i++) {
		struct bnxt_sw_rx_bd *ag_buf;
		struct rte_mbuf *ag_mbuf;

		if (is_thor_tpa) {
			rxcmp = (void *)&tpa_info->agg_arr[i];
		} else {
			*tmp_raw_cons = NEXT_RAW_CMP(*tmp_raw_cons);
			cp_cons = RING_CMP(cpr->cp_ring_struct, *tmp_raw_cons);
			rxcmp = (struct rx_pkt_cmpl *)
					&cpr->cp_desc_ring[cp_cons];
		}

#ifdef BNXT_DEBUG
		bnxt_dump_cmpl(cp_cons, rxcmp);
#endif

		ag_cons = rxcmp->opaque;
		RTE_ASSERT(ag_cons <= rxr->ag_ring_struct->ring_mask);
		ag_buf = &rxr->ag_buf_ring[ag_cons];
		ag_mbuf = ag_buf->mbuf;
		RTE_ASSERT(ag_mbuf != NULL);

		ag_mbuf->data_len = rte_le_to_cpu_16(rxcmp->len);

		mbuf->nb_segs++;
		mbuf->pkt_len += ag_mbuf->data_len;

		last->next = ag_mbuf;
		last = ag_mbuf;

		ag_buf->mbuf = NULL;

		/*
		 * As aggregation buffer consumed out of order in TPA module,
		 * use bitmap to track freed slots to be allocated and notified
		 * to NIC
		 */
		rte_bitmap_set(rxr->ag_bitmap, ag_cons);
	}
	bnxt_prod_ag_mbuf(rxq);
	return 0;
}

static inline struct rte_mbuf *bnxt_tpa_end(
		struct bnxt_rx_queue *rxq,
		uint32_t *raw_cp_cons,
		struct rx_tpa_end_cmpl *tpa_end,
		struct rx_tpa_end_cmpl_hi *tpa_end1)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t agg_id;
	struct rte_mbuf *mbuf;
	uint8_t agg_bufs;
	uint8_t payload_offset;
	struct bnxt_tpa_info *tpa_info;

	if (BNXT_CHIP_THOR(rxq->bp)) {
		struct rx_tpa_v2_end_cmpl *th_tpa_end;
		struct rx_tpa_v2_end_cmpl_hi *th_tpa_end1;

		th_tpa_end = (void *)tpa_end;
		th_tpa_end1 = (void *)tpa_end1;
		agg_id = BNXT_TPA_END_AGG_ID_TH(th_tpa_end);
		agg_bufs = BNXT_TPA_END_AGG_BUFS_TH(th_tpa_end1);
		payload_offset = th_tpa_end1->payload_offset;
	} else {
		agg_id = BNXT_TPA_END_AGG_ID(tpa_end);
		agg_bufs = BNXT_TPA_END_AGG_BUFS(tpa_end);
		if (!bnxt_agg_bufs_valid(cpr, agg_bufs, *raw_cp_cons))
			return NULL;
		payload_offset = tpa_end->payload_offset;
	}

	tpa_info = &rxr->tpa_info[agg_id];
	mbuf = tpa_info->mbuf;
	RTE_ASSERT(mbuf != NULL);

	rte_prefetch0(mbuf);
	if (agg_bufs) {
		bnxt_rx_pages(rxq, mbuf, raw_cp_cons, agg_bufs, tpa_info);
	}
	mbuf->l4_len = payload_offset;

	struct rte_mbuf *new_data = __bnxt_alloc_rx_data(rxq->mb_pool);
	RTE_ASSERT(new_data != NULL);
	if (!new_data) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return NULL;
	}
	tpa_info->mbuf = new_data;

	return mbuf;
}

static uint32_t
bnxt_parse_pkt_type(struct rx_pkt_cmpl *rxcmp, struct rx_pkt_cmpl_hi *rxcmp1)
{
	uint32_t l3, pkt_type = 0;
	uint32_t t_ipcs = 0, ip6 = 0, vlan = 0;
	uint32_t flags_type;

	vlan = !!(rxcmp1->flags2 &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN));
	pkt_type |= vlan ? RTE_PTYPE_L2_ETHER_VLAN : RTE_PTYPE_L2_ETHER;

	t_ipcs = !!(rxcmp1->flags2 &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC));
	ip6 = !!(rxcmp1->flags2 &
		 rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_TYPE));

	flags_type = rxcmp->flags_type &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS_ITYPE_MASK);

	if (!t_ipcs && !ip6)
		l3 = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (!t_ipcs && ip6)
		l3 = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	else if (t_ipcs && !ip6)
		l3 = RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
	else
		l3 = RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;

	switch (flags_type) {
	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_ICMP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_ICMP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_ICMP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_TCP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_TCP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_TCP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_UDP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_UDP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_UDP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_IP):
		pkt_type |= l3;
		break;
	}

	return pkt_type;
}

#ifdef RTE_LIBRTE_IEEE1588
static void
bnxt_get_rx_ts_thor(struct bnxt *bp, uint32_t rx_ts_cmpl)
{
	uint64_t systime_cycles = 0;

	if (!BNXT_CHIP_THOR(bp))
		return;

	/* On Thor, Rx timestamps are provided directly in the
	 * Rx completion records to the driver. Only 32 bits of
	 * the timestamp is present in the completion. Driver needs
	 * to read the current 48 bit free running timer using the
	 * HWRM_PORT_TS_QUERY command and combine the upper 16 bits
	 * from the HWRM response with the lower 32 bits in the
	 * Rx completion to produce the 48 bit timestamp for the Rx packet
	 */
	bnxt_hwrm_port_ts_query(bp, BNXT_PTP_FLAGS_CURRENT_TIME,
				&systime_cycles);
	bp->ptp_cfg->rx_timestamp = (systime_cycles & 0xFFFF00000000);
	bp->ptp_cfg->rx_timestamp |= rx_ts_cmpl;
}
#endif

static int bnxt_rx_pkt(struct rte_mbuf **rx_pkt,
			    struct bnxt_rx_queue *rxq, uint32_t *raw_cons)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct rx_pkt_cmpl *rxcmp;
	struct rx_pkt_cmpl_hi *rxcmp1;
	uint32_t tmp_raw_cons = *raw_cons;
	uint16_t cons, prod, cp_cons =
	    RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	struct rte_mbuf *mbuf;
	int rc = 0;
	uint8_t agg_buf = 0;
	uint16_t cmp_type;
	uint32_t flags2_f = 0;
	uint16_t flags_type;

	rxcmp = (struct rx_pkt_cmpl *)
	    &cpr->cp_desc_ring[cp_cons];

	cmp_type = CMP_TYPE(rxcmp);

	if (cmp_type == RX_TPA_V2_ABUF_CMPL_TYPE_RX_TPA_AGG) {
		struct rx_tpa_v2_abuf_cmpl *rx_agg = (void *)rxcmp;
		uint16_t agg_id = rte_cpu_to_le_16(rx_agg->agg_id);
		struct bnxt_tpa_info *tpa_info;

		tpa_info = &rxr->tpa_info[agg_id];
		RTE_ASSERT(tpa_info->agg_count < 16);
		tpa_info->agg_arr[tpa_info->agg_count++] = *rx_agg;
		rc = -EINVAL; /* Continue w/o new mbuf */
		goto next_rx;
	}

	tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
	cp_cons = RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	rxcmp1 = (struct rx_pkt_cmpl_hi *)&cpr->cp_desc_ring[cp_cons];

	if (!CMP_VALID(rxcmp1, tmp_raw_cons, cpr->cp_ring_struct))
		return -EBUSY;

	cpr->valid = FLIP_VALID(cp_cons,
				cpr->cp_ring_struct->ring_mask,
				cpr->valid);

	if (cmp_type == RX_TPA_START_CMPL_TYPE_RX_TPA_START) {
		bnxt_tpa_start(rxq, (struct rx_tpa_start_cmpl *)rxcmp,
			       (struct rx_tpa_start_cmpl_hi *)rxcmp1);
		rc = -EINVAL; /* Continue w/o new mbuf */
		goto next_rx;
	} else if (cmp_type == RX_TPA_END_CMPL_TYPE_RX_TPA_END) {
		mbuf = bnxt_tpa_end(rxq, &tmp_raw_cons,
				   (struct rx_tpa_end_cmpl *)rxcmp,
				   (struct rx_tpa_end_cmpl_hi *)rxcmp1);
		if (unlikely(!mbuf))
			return -EBUSY;
		*rx_pkt = mbuf;
		goto next_rx;
	} else if (cmp_type != 0x11) {
		rc = -EINVAL;
		goto next_rx;
	}

	agg_buf = (rxcmp->agg_bufs_v1 & RX_PKT_CMPL_AGG_BUFS_MASK)
			>> RX_PKT_CMPL_AGG_BUFS_SFT;
	if (agg_buf && !bnxt_agg_bufs_valid(cpr, agg_buf, tmp_raw_cons))
		return -EBUSY;

	prod = rxr->rx_prod;

	cons = rxcmp->opaque;
	mbuf = bnxt_consume_rx_buf(rxr, cons);
	if (mbuf == NULL)
		return -EBUSY;

	rte_prefetch0(mbuf);

	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rxcmp->len;
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;
	mbuf->ol_flags = 0;

	flags_type = rte_le_to_cpu_16(rxcmp->flags_type);
	if (flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID) {
		mbuf->hash.rss = rxcmp->rss_hash;
		mbuf->ol_flags |= PKT_RX_RSS_HASH;
	}

	bnxt_set_mark_in_mbuf(rxq->bp, rxcmp1, mbuf);

#ifdef RTE_LIBRTE_IEEE1588
	if (unlikely((flags_type & RX_PKT_CMPL_FLAGS_MASK) ==
		     RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP)) {
		mbuf->ol_flags |= PKT_RX_IEEE1588_PTP | PKT_RX_IEEE1588_TMST;
		bnxt_get_rx_ts_thor(rxq->bp, rxcmp1->reorder);
	}
#endif
	if (agg_buf)
		bnxt_rx_pages(rxq, mbuf, &tmp_raw_cons, agg_buf, NULL);

	if (rxcmp1->flags2 & RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN) {
		mbuf->vlan_tci = rxcmp1->metadata &
			(RX_PKT_CMPL_METADATA_VID_MASK |
			RX_PKT_CMPL_METADATA_DE |
			RX_PKT_CMPL_METADATA_PRI_MASK);
		mbuf->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
	}

	flags2_f = flags2_0xf(rxcmp1);
	/* IP Checksum */
	if (likely(IS_IP_NONTUNNEL_PKT(flags2_f))) {
		if (unlikely(RX_CMP_IP_CS_ERROR(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		else if (unlikely(RX_CMP_IP_CS_UNKNOWN(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_UNKNOWN;
		else
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else if (IS_IP_TUNNEL_PKT(flags2_f)) {
		if (unlikely(RX_CMP_IP_OUTER_CS_ERROR(rxcmp1) ||
			     RX_CMP_IP_CS_ERROR(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		else if (unlikely(RX_CMP_IP_CS_UNKNOWN(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_UNKNOWN;
		else
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	}

	/* L4 Checksum */
	if (likely(IS_L4_NONTUNNEL_PKT(flags2_f))) {
		if (unlikely(RX_CMP_L4_INNER_CS_ERR2(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
	} else if (IS_L4_TUNNEL_PKT(flags2_f)) {
		if (unlikely(RX_CMP_L4_INNER_CS_ERR2(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
		if (unlikely(RX_CMP_L4_OUTER_CS_ERR2(rxcmp1))) {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_BAD;
		} else if (unlikely(IS_L4_TUNNEL_PKT_ONLY_INNER_L4_CS
				    (flags2_f))) {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_UNKNOWN;
		} else {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_GOOD;
		}
	} else if (unlikely(RX_CMP_L4_CS_UNKNOWN(rxcmp1))) {
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_UNKNOWN;
	}

	mbuf->packet_type = bnxt_parse_pkt_type(rxcmp, rxcmp1);

#ifdef BNXT_DEBUG
	if (rxcmp1->errors_v2 & RX_CMP_L2_ERRORS) {
		/* Re-install the mbuf back to the rx ring */
		bnxt_reuse_rx_mbuf(rxr, cons, mbuf);

		rc = -EIO;
		goto next_rx;
	}
#endif
	/*
	 * TODO: Redesign this....
	 * If the allocation fails, the packet does not get received.
	 * Simply returning this will result in slowly falling behind
	 * on the producer ring buffers.
	 * Instead, "filling up" the producer just before ringing the
	 * doorbell could be a better solution since it will let the
	 * producer ring starve until memory is available again pushing
	 * the drops into hardware and getting them out of the driver
	 * allowing recovery to a full producer ring.
	 *
	 * This could also help with cache usage by preventing per-packet
	 * calls in favour of a tight loop with the same function being called
	 * in it.
	 */
	prod = RING_NEXT(rxr->rx_ring_struct, prod);
	if (bnxt_alloc_rx_data(rxq, rxr, prod)) {
		PMD_DRV_LOG(ERR, "mbuf alloc failed with prod=0x%x\n", prod);
		rc = -ENOMEM;
		goto rx;
	}
	rxr->rx_prod = prod;
	/*
	 * All MBUFs are allocated with the same size under DPDK,
	 * no optimization for rx_copy_thresh
	 */
rx:
	*rx_pkt = mbuf;

next_rx:

	*raw_cons = tmp_raw_cons;

	return rc;
}

uint16_t bnxt_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	int nb_rx_pkts = 0;
	struct rx_pkt_cmpl *rxcmp;
	uint16_t prod = rxr->rx_prod;
	uint16_t ag_prod = rxr->ag_prod;
	int rc = 0;
	bool evt = false;

	if (unlikely(is_bnxt_in_error(rxq->bp)))
		return 0;

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started ||
		     !rte_spinlock_trylock(&rxq->lock)))
		return 0;

	/* Handle RX burst request */
	while (1) {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		rte_prefetch0(&cpr->cp_desc_ring[cons]);
		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!CMP_VALID(rxcmp, raw_cons, cpr->cp_ring_struct))
			break;
		cpr->valid = FLIP_VALID(cons,
					cpr->cp_ring_struct->ring_mask,
					cpr->valid);

		/* TODO: Avoid magic numbers... */
		if ((CMP_TYPE(rxcmp) & 0x30) == 0x10) {
			rc = bnxt_rx_pkt(&rx_pkts[nb_rx_pkts], rxq, &raw_cons);
			if (likely(!rc) || rc == -ENOMEM)
				nb_rx_pkts++;
			if (rc == -EBUSY)	/* partial completion */
				break;
		} else if (!BNXT_NUM_ASYNC_CPR(rxq->bp)) {
			evt =
			bnxt_event_hwrm_resp_handler(rxq->bp,
						     (struct cmpl_base *)rxcmp);
			/* If the async event is Fatal error, return */
			if (unlikely(is_bnxt_in_error(rxq->bp)))
				goto done;
		}

		raw_cons = NEXT_RAW_CMP(raw_cons);
		if (nb_rx_pkts == nb_pkts || evt)
			break;
		/* Post some Rx buf early in case of larger burst processing */
		if (nb_rx_pkts == BNXT_RX_POST_THRESH)
			bnxt_db_write(&rxr->rx_db, rxr->rx_prod);
	}

	cpr->cp_raw_cons = raw_cons;
	if (!nb_rx_pkts && !evt) {
		/*
		 * For PMD, there is no need to keep on pushing to REARM
		 * the doorbell if there are no new completions
		 */
		goto done;
	}

	if (prod != rxr->rx_prod)
		bnxt_db_write(&rxr->rx_db, rxr->rx_prod);

	/* Ring the AGG ring DB */
	if (ag_prod != rxr->ag_prod)
		bnxt_db_write(&rxr->ag_db, rxr->ag_prod);

	bnxt_db_cq(cpr);

	/* Attempt to alloc Rx buf in case of a previous allocation failure. */
	if (rc == -ENOMEM) {
		int i = RING_NEXT(rxr->rx_ring_struct, prod);
		int cnt = nb_rx_pkts;

		for (; cnt;
			i = RING_NEXT(rxr->rx_ring_struct, i), cnt--) {
			struct bnxt_sw_rx_bd *rx_buf = &rxr->rx_buf_ring[i];

			/* Buffer already allocated for this index. */
			if (rx_buf->mbuf != NULL)
				continue;

			/* This slot is empty. Alloc buffer for Rx */
			if (!bnxt_alloc_rx_data(rxq, rxr, i)) {
				rxr->rx_prod = i;
				bnxt_db_write(&rxr->rx_db, rxr->rx_prod);
			} else {
				PMD_DRV_LOG(ERR, "Alloc  mbuf failed\n");
				break;
			}
		}
	}

done:
	rte_spinlock_unlock(&rxq->lock);

	return nb_rx_pkts;
}

/*
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 */
uint16_t
bnxt_dummy_recv_pkts(void *rx_queue __rte_unused,
		     struct rte_mbuf **rx_pkts __rte_unused,
		     uint16_t nb_pkts __rte_unused)
{
	return 0;
}

void bnxt_free_rx_rings(struct bnxt *bp)
{
	int i;
	struct bnxt_rx_queue *rxq;

	if (!bp->rx_queues)
		return;

	for (i = 0; i < (int)bp->rx_nr_rings; i++) {
		rxq = bp->rx_queues[i];
		if (!rxq)
			continue;

		bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
		rte_free(rxq->rx_ring->rx_ring_struct);

		/* Free the Aggregator ring */
		bnxt_free_ring(rxq->rx_ring->ag_ring_struct);
		rte_free(rxq->rx_ring->ag_ring_struct);
		rxq->rx_ring->ag_ring_struct = NULL;

		rte_free(rxq->rx_ring);

		bnxt_free_ring(rxq->cp_ring->cp_ring_struct);
		rte_free(rxq->cp_ring->cp_ring_struct);
		rte_free(rxq->cp_ring);

		rte_free(rxq);
		bp->rx_queues[i] = NULL;
	}
}

int bnxt_init_rx_ring_struct(struct bnxt_rx_queue *rxq, unsigned int socket_id)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;

	rxq->rx_buf_size = BNXT_MAX_PKT_LEN + sizeof(struct rte_mbuf);

	rxr = rte_zmalloc_socket("bnxt_rx_ring",
				 sizeof(struct bnxt_rx_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxr == NULL)
		return -ENOMEM;
	rxq->rx_ring = rxr;

	ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
				   sizeof(struct bnxt_ring),
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	rxr->rx_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxq->nb_rx_desc);
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)rxr->rx_desc_ring;
	ring->bd_dma = rxr->rx_desc_mapping;
	ring->vmem_size = ring->ring_size * sizeof(struct bnxt_sw_rx_bd);
	ring->vmem = (void **)&rxr->rx_buf_ring;

	cpr = rte_zmalloc_socket("bnxt_rx_ring",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (cpr == NULL)
		return -ENOMEM;
	rxq->cp_ring = cpr;

	ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
				   sizeof(struct bnxt_ring),
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	cpr->cp_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxr->rx_ring_struct->ring_size *
					  (2 + AGG_RING_SIZE_FACTOR));
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->vmem_size = 0;
	ring->vmem = NULL;

	/* Allocate Aggregator rings */
	ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
				   sizeof(struct bnxt_ring),
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	rxr->ag_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxq->nb_rx_desc *
					  AGG_RING_SIZE_FACTOR);
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)rxr->ag_desc_ring;
	ring->bd_dma = rxr->ag_desc_mapping;
	ring->vmem_size = ring->ring_size * sizeof(struct bnxt_sw_rx_bd);
	ring->vmem = (void **)&rxr->ag_buf_ring;

	return 0;
}

static void bnxt_init_rxbds(struct bnxt_ring *ring, uint32_t type,
			    uint16_t len)
{
	uint32_t j;
	struct rx_prod_pkt_bd *rx_bd_ring = (struct rx_prod_pkt_bd *)ring->bd;

	if (!rx_bd_ring)
		return;
	for (j = 0; j < ring->ring_size; j++) {
		rx_bd_ring[j].flags_type = rte_cpu_to_le_16(type);
		rx_bd_ring[j].len = rte_cpu_to_le_16(len);
		rx_bd_ring[j].opaque = j;
	}
}

int bnxt_init_one_rx_ring(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;
	uint32_t prod, type;
	unsigned int i;
	uint16_t size;

	size = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
	size = RTE_MIN(BNXT_MAX_PKT_LEN, size);

	type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT | RX_PROD_PKT_BD_FLAGS_EOP_PAD;

	rxr = rxq->rx_ring;
	ring = rxr->rx_ring_struct;
	bnxt_init_rxbds(ring, type, size);

	prod = rxr->rx_prod;
	for (i = 0; i < ring->ring_size; i++) {
		if (bnxt_alloc_rx_data(rxq, rxr, prod) != 0) {
			PMD_DRV_LOG(WARNING,
				"init'ed rx ring %d with %d/%d mbufs only\n",
				rxq->queue_id, i, ring->ring_size);
			break;
		}
		rxr->rx_prod = prod;
		prod = RING_NEXT(rxr->rx_ring_struct, prod);
	}

	ring = rxr->ag_ring_struct;
	type = RX_PROD_AGG_BD_TYPE_RX_PROD_AGG;
	bnxt_init_rxbds(ring, type, size);
	prod = rxr->ag_prod;

	for (i = 0; i < ring->ring_size; i++) {
		if (bnxt_alloc_ag_data(rxq, rxr, prod) != 0) {
			PMD_DRV_LOG(WARNING,
			"init'ed AG ring %d with %d/%d mbufs only\n",
			rxq->queue_id, i, ring->ring_size);
			break;
		}
		rxr->ag_prod = prod;
		prod = RING_NEXT(rxr->ag_ring_struct, prod);
	}
	PMD_DRV_LOG(DEBUG, "AGG Done!\n");

	if (rxr->tpa_info) {
		unsigned int max_aggs = BNXT_TPA_MAX_AGGS(rxq->bp);

		for (i = 0; i < max_aggs; i++) {
			rxr->tpa_info[i].mbuf =
				__bnxt_alloc_rx_data(rxq->mb_pool);
			if (!rxr->tpa_info[i].mbuf) {
				rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
				return -ENOMEM;
			}
		}
	}
	PMD_DRV_LOG(DEBUG, "TPA alloc Done!\n");

	return 0;
}

void bnxt_set_mark_in_mbuf(struct bnxt *bp,
			   struct rx_pkt_cmpl_hi *rxcmp1,
			   struct rte_mbuf *mbuf)
{
	uint32_t cfa_code = 0;
	uint8_t meta_fmt =  0;
	uint16_t flags2 = 0;
	uint32_t meta =  0;

	cfa_code = rte_le_to_cpu_16(rxcmp1->cfa_code);
	if (!cfa_code)
		return;

	if (cfa_code && !bp->mark_table[cfa_code].valid)
		return;

	flags2 = rte_le_to_cpu_16(rxcmp1->flags2);
	meta = rte_le_to_cpu_32(rxcmp1->metadata);
	if (meta) {
		meta >>= BNXT_RX_META_CFA_CODE_SHIFT;

		/*
		 * The flags field holds extra bits of info from [6:4]
		 * which indicate if the flow is in TCAM or EM or EEM
		 */
		meta_fmt = (flags2 & BNXT_CFA_META_FMT_MASK) >>
			   BNXT_CFA_META_FMT_SHFT;

		/*
		 * meta_fmt == 4 => 'b100 => 'b10x => EM.
		 * meta_fmt == 5 => 'b101 => 'b10x => EM + VLAN
		 * meta_fmt == 6 => 'b110 => 'b11x => EEM
		 * meta_fmt == 7 => 'b111 => 'b11x => EEM + VLAN.
		 */
		meta_fmt >>= BNXT_CFA_META_FMT_EM_EEM_SHFT;
	}

	mbuf->hash.fdir.hi = bp->mark_table[cfa_code].mark_id;
	mbuf->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
}
