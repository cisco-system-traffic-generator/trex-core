/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdio.h>
#include <rte_flow.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include "base/ice_fdir.h"
#include "base/ice_flow.h"
#include "base/ice_type.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"
#include "ice_generic_flow.h"

#define ICE_FDIR_IPV6_TC_OFFSET		20
#define ICE_IPV6_TC_MASK		(0xFF << ICE_FDIR_IPV6_TC_OFFSET)

#define ICE_FDIR_MAX_QREGION_SIZE	128

#define ICE_FDIR_INSET_ETH_IPV4 (\
	ICE_INSET_DMAC | \
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_TOS | \
	ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_PROTO)

#define ICE_FDIR_INSET_ETH_IPV4_UDP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV4_TCP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV4_SCTP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6 (\
	ICE_INSET_DMAC | \
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_TC | \
	ICE_INSET_IPV6_HOP_LIMIT | ICE_INSET_IPV6_NEXT_HDR)

#define ICE_FDIR_INSET_ETH_IPV6_UDP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_TCP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_SCTP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

#define ICE_FDIR_INSET_VXLAN_IPV4 (\
	ICE_INSET_TUN_IPV4_SRC | ICE_INSET_TUN_IPV4_DST)

#define ICE_FDIR_INSET_VXLAN_IPV4_TCP (\
	ICE_FDIR_INSET_VXLAN_IPV4 | \
	ICE_INSET_TUN_TCP_SRC_PORT | ICE_INSET_TUN_TCP_DST_PORT)

#define ICE_FDIR_INSET_VXLAN_IPV4_UDP (\
	ICE_FDIR_INSET_VXLAN_IPV4 | \
	ICE_INSET_TUN_UDP_SRC_PORT | ICE_INSET_TUN_UDP_DST_PORT)

#define ICE_FDIR_INSET_VXLAN_IPV4_SCTP (\
	ICE_FDIR_INSET_VXLAN_IPV4 | \
	ICE_INSET_TUN_SCTP_SRC_PORT | ICE_INSET_TUN_SCTP_DST_PORT)

#define ICE_FDIR_INSET_GTPU (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | ICE_INSET_GTPU_TEID)

#define ICE_FDIR_INSET_GTPU_EH (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | \
	ICE_INSET_GTPU_TEID | ICE_INSET_GTPU_QFI)

static struct ice_pattern_match_item ice_fdir_pattern_os[] = {
	{pattern_eth_ipv4,             ICE_FDIR_INSET_ETH_IPV4,              ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,         ICE_FDIR_INSET_ETH_IPV4_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,         ICE_FDIR_INSET_ETH_IPV4_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_sctp,        ICE_FDIR_INSET_ETH_IPV4_SCTP,         ICE_INSET_NONE},
	{pattern_eth_ipv6,             ICE_FDIR_INSET_ETH_IPV6,              ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,         ICE_FDIR_INSET_ETH_IPV6_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,         ICE_FDIR_INSET_ETH_IPV6_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_sctp,        ICE_FDIR_INSET_ETH_IPV6_SCTP,         ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4,
				       ICE_FDIR_INSET_VXLAN_IPV4,            ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_udp,
				       ICE_FDIR_INSET_VXLAN_IPV4_UDP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_tcp,
				       ICE_FDIR_INSET_VXLAN_IPV4_TCP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_sctp,
				       ICE_FDIR_INSET_VXLAN_IPV4_SCTP,       ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,
				       ICE_FDIR_INSET_VXLAN_IPV4,            ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,
				       ICE_FDIR_INSET_VXLAN_IPV4_UDP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,
				       ICE_FDIR_INSET_VXLAN_IPV4_TCP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_sctp,
				       ICE_FDIR_INSET_VXLAN_IPV4_SCTP,       ICE_INSET_NONE},
};

static struct ice_pattern_match_item ice_fdir_pattern_comms[] = {
	{pattern_eth_ipv4,             ICE_FDIR_INSET_ETH_IPV4,              ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,         ICE_FDIR_INSET_ETH_IPV4_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,         ICE_FDIR_INSET_ETH_IPV4_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_sctp,        ICE_FDIR_INSET_ETH_IPV4_SCTP,         ICE_INSET_NONE},
	{pattern_eth_ipv6,             ICE_FDIR_INSET_ETH_IPV6,              ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,         ICE_FDIR_INSET_ETH_IPV6_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,         ICE_FDIR_INSET_ETH_IPV6_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_sctp,        ICE_FDIR_INSET_ETH_IPV6_SCTP,         ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4,
				       ICE_FDIR_INSET_VXLAN_IPV4,            ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_udp,
				       ICE_FDIR_INSET_VXLAN_IPV4_UDP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_tcp,
				       ICE_FDIR_INSET_VXLAN_IPV4_TCP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_sctp,
				       ICE_FDIR_INSET_VXLAN_IPV4_SCTP,       ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,
				       ICE_FDIR_INSET_VXLAN_IPV4,            ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,
				       ICE_FDIR_INSET_VXLAN_IPV4_UDP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,
				       ICE_FDIR_INSET_VXLAN_IPV4_TCP,        ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_sctp,
				       ICE_FDIR_INSET_VXLAN_IPV4_SCTP,       ICE_INSET_NONE},
	{pattern_eth_ipv4_gtpu,	       ICE_FDIR_INSET_GTPU,                  ICE_INSET_NONE},
	{pattern_eth_ipv4_gtpu_eh,     ICE_FDIR_INSET_GTPU_EH,               ICE_INSET_NONE},
};

static struct ice_flow_parser ice_fdir_parser_os;
static struct ice_flow_parser ice_fdir_parser_comms;

static int
ice_fdir_is_tunnel_profile(enum ice_fdir_tunnel_type tunnel_type);

static const struct rte_memzone *
ice_memzone_reserve(const char *name, uint32_t len, int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(name, len, socket_id,
					   RTE_MEMZONE_IOVA_CONTIG,
					   ICE_RING_BASE_ALIGN);
}

#define ICE_FDIR_MZ_NAME	"FDIR_MEMZONE"

static int
ice_fdir_prof_alloc(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype, fltr_ptype;

	if (!hw->fdir_prof) {
		hw->fdir_prof = (struct ice_fd_hw_prof **)
			ice_malloc(hw, ICE_FLTR_PTYPE_MAX *
				   sizeof(*hw->fdir_prof));
		if (!hw->fdir_prof)
			return -ENOMEM;
	}
	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		if (!hw->fdir_prof[ptype]) {
			hw->fdir_prof[ptype] = (struct ice_fd_hw_prof *)
				ice_malloc(hw, sizeof(**hw->fdir_prof));
			if (!hw->fdir_prof[ptype])
				goto fail_mem;
		}
	}
	return 0;

fail_mem:
	for (fltr_ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     fltr_ptype < ptype;
	     fltr_ptype++) {
		rte_free(hw->fdir_prof[fltr_ptype]);
		hw->fdir_prof[fltr_ptype] = NULL;
	}

	rte_free(hw->fdir_prof);
	hw->fdir_prof = NULL;

	return -ENOMEM;
}

static int
ice_fdir_counter_pool_add(__rte_unused struct ice_pf *pf,
			  struct ice_fdir_counter_pool_container *container,
			  uint32_t index_start,
			  uint32_t len)
{
	struct ice_fdir_counter_pool *pool;
	uint32_t i;
	int ret = 0;

	pool = rte_zmalloc("ice_fdir_counter_pool",
			   sizeof(*pool) +
			   sizeof(struct ice_fdir_counter) * len,
			   0);
	if (!pool) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir counter pool");
		return -ENOMEM;
	}

	TAILQ_INIT(&pool->counter_list);
	TAILQ_INSERT_TAIL(&container->pool_list, pool, next);

	for (i = 0; i < len; i++) {
		struct ice_fdir_counter *counter = &pool->counters[i];

		counter->hw_index = index_start + i;
		TAILQ_INSERT_TAIL(&pool->counter_list, counter, next);
	}

	if (container->index_free == ICE_FDIR_COUNTER_MAX_POOL_SIZE) {
		PMD_INIT_LOG(ERR, "FDIR counter pool is full");
		ret = -EINVAL;
		goto free_pool;
	}

	container->pools[container->index_free++] = pool;
	return 0;

free_pool:
	rte_free(pool);
	return ret;
}

static int
ice_fdir_counter_init(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	uint32_t cnt_index, len;
	int ret;

	TAILQ_INIT(&container->pool_list);

	cnt_index = ICE_FDIR_COUNTER_INDEX(hw->fd_ctr_base);
	len = ICE_FDIR_COUNTERS_PER_BLOCK;

	ret = ice_fdir_counter_pool_add(pf, container, cnt_index, len);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to add fdir pool to container");
		return ret;
	}

	return 0;
}

static int
ice_fdir_counter_release(struct ice_pf *pf)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	uint8_t i;

	for (i = 0; i < container->index_free; i++) {
		rte_free(container->pools[i]);
		container->pools[i] = NULL;
	}

	TAILQ_INIT(&container->pool_list);
	container->index_free = 0;

	return 0;
}

static struct ice_fdir_counter *
ice_fdir_counter_shared_search(struct ice_fdir_counter_pool_container
					*container,
			       uint32_t id)
{
	struct ice_fdir_counter_pool *pool;
	struct ice_fdir_counter *counter;
	int i;

	TAILQ_FOREACH(pool, &container->pool_list, next) {
		for (i = 0; i < ICE_FDIR_COUNTERS_PER_BLOCK; i++) {
			counter = &pool->counters[i];

			if (counter->shared &&
			    counter->ref_cnt &&
			    counter->id == id)
				return counter;
		}
	}

	return NULL;
}

static struct ice_fdir_counter *
ice_fdir_counter_alloc(struct ice_pf *pf, uint32_t shared, uint32_t id)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	struct ice_fdir_counter_pool *pool = NULL;
	struct ice_fdir_counter *counter_free = NULL;

	if (shared) {
		counter_free = ice_fdir_counter_shared_search(container, id);
		if (counter_free) {
			if (counter_free->ref_cnt + 1 == 0) {
				rte_errno = E2BIG;
				return NULL;
			}
			counter_free->ref_cnt++;
			return counter_free;
		}
	}

	TAILQ_FOREACH(pool, &container->pool_list, next) {
		counter_free = TAILQ_FIRST(&pool->counter_list);
		if (counter_free)
			break;
		counter_free = NULL;
	}

	if (!counter_free) {
		PMD_DRV_LOG(ERR, "No free counter found\n");
		return NULL;
	}

	counter_free->shared = shared;
	counter_free->id = id;
	counter_free->ref_cnt = 1;
	counter_free->pool = pool;

	/* reset statistic counter value */
	ICE_WRITE_REG(hw, GLSTAT_FD_CNT0H(counter_free->hw_index), 0);
	ICE_WRITE_REG(hw, GLSTAT_FD_CNT0L(counter_free->hw_index), 0);

	TAILQ_REMOVE(&pool->counter_list, counter_free, next);
	if (TAILQ_EMPTY(&pool->counter_list)) {
		TAILQ_REMOVE(&container->pool_list, pool, next);
		TAILQ_INSERT_TAIL(&container->pool_list, pool, next);
	}

	return counter_free;
}

static void
ice_fdir_counter_free(__rte_unused struct ice_pf *pf,
		      struct ice_fdir_counter *counter)
{
	if (!counter)
		return;

	if (--counter->ref_cnt == 0) {
		struct ice_fdir_counter_pool *pool = counter->pool;

		TAILQ_INSERT_TAIL(&pool->counter_list, counter, next);
	}
}

static int
ice_fdir_init_filter_list(struct ice_pf *pf)
{
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	struct ice_fdir_info *fdir_info = &pf->fdir;
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	int ret;

	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = ICE_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct ice_fdir_fltr_pattern),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_EXT_TABLE,
	};

	/* Initialize hash */
	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE,
		 "fdir_%s", dev->device->name);
	fdir_info->hash_table = rte_hash_create(&fdir_hash_params);
	if (!fdir_info->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table!");
		return -EINVAL;
	}
	fdir_info->hash_map = rte_zmalloc("ice_fdir_hash_map",
					  sizeof(*fdir_info->hash_map) *
					  ICE_MAX_FDIR_FILTER_NUM,
					  0);
	if (!fdir_info->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir hash map!");
		ret = -ENOMEM;
		goto err_fdir_hash_map_alloc;
	}
	return 0;

err_fdir_hash_map_alloc:
	rte_hash_free(fdir_info->hash_table);

	return ret;
}

static void
ice_fdir_release_filter_list(struct ice_pf *pf)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;

	if (fdir_info->hash_map)
		rte_free(fdir_info->hash_map);
	if (fdir_info->hash_table)
		rte_hash_free(fdir_info->hash_table);

	fdir_info->hash_map = NULL;
	fdir_info->hash_table = NULL;
}

/*
 * ice_fdir_setup - reserve and initialize the Flow Director resources
 * @pf: board private structure
 */
static int
ice_fdir_setup(struct ice_pf *pf)
{
	struct rte_eth_dev *eth_dev = pf->adapter->eth_dev;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];
	struct ice_vsi *vsi;
	int err = ICE_SUCCESS;

	if ((pf->flags & ICE_FLAG_FDIR) == 0) {
		PMD_INIT_LOG(ERR, "HW doesn't support FDIR");
		return -ENOTSUP;
	}

	PMD_DRV_LOG(INFO, "FDIR HW Capabilities: fd_fltr_guar = %u,"
		    " fd_fltr_best_effort = %u.",
		    hw->func_caps.fd_fltr_guar,
		    hw->func_caps.fd_fltr_best_effort);

	if (pf->fdir.fdir_vsi) {
		PMD_DRV_LOG(INFO, "FDIR initialization has been done.");
		return ICE_SUCCESS;
	}

	/* make new FDIR VSI */
	vsi = ice_setup_vsi(pf, ICE_VSI_CTRL);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Couldn't create FDIR VSI.");
		return -EINVAL;
	}
	pf->fdir.fdir_vsi = vsi;

	err = ice_fdir_init_filter_list(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init FDIR filter list.");
		return -EINVAL;
	}

	err = ice_fdir_counter_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init FDIR counter.");
		return -EINVAL;
	}

	/*Fdir tx queue setup*/
	err = ice_fdir_setup_tx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR TX resources.");
		goto fail_setup_tx;
	}

	/*Fdir rx queue setup*/
	err = ice_fdir_setup_rx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR RX resources.");
		goto fail_setup_rx;
	}

	err = ice_fdir_tx_queue_start(eth_dev, pf->fdir.txq->queue_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to start FDIR TX queue.");
		goto fail_mem;
	}

	err = ice_fdir_rx_queue_start(eth_dev, pf->fdir.rxq->queue_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to start FDIR RX queue.");
		goto fail_mem;
	}

	/* Enable FDIR MSIX interrupt */
	vsi->nb_used_qps = 1;
	ice_vsi_queues_bind_intr(vsi);
	ice_vsi_enable_queues_intr(vsi);

	/* reserve memory for the fdir programming packet */
	snprintf(z_name, sizeof(z_name), "ICE_%s_%d",
		 ICE_FDIR_MZ_NAME,
		 eth_dev->data->port_id);
	mz = ice_memzone_reserve(z_name, ICE_FDIR_PKT_LEN, SOCKET_ID_ANY);
	if (!mz) {
		PMD_DRV_LOG(ERR, "Cannot init memzone for "
			    "flow director program packet.");
		err = -ENOMEM;
		goto fail_mem;
	}
	pf->fdir.prg_pkt = mz->addr;
	pf->fdir.dma_addr = mz->iova;
	pf->fdir.mz = mz;

	err = ice_fdir_prof_alloc(hw);
	if (err) {
		PMD_DRV_LOG(ERR, "Cannot allocate memory for "
			    "flow director profile.");
		err = -ENOMEM;
		goto fail_prof;
	}

	PMD_DRV_LOG(INFO, "FDIR setup successfully, with programming queue %u.",
		    vsi->base_queue);
	return ICE_SUCCESS;

fail_prof:
	rte_memzone_free(pf->fdir.mz);
	pf->fdir.mz = NULL;
fail_mem:
	ice_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
fail_setup_rx:
	ice_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
fail_setup_tx:
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;
	return err;
}

static void
ice_fdir_prof_free(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype;

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		rte_free(hw->fdir_prof[ptype]);
		hw->fdir_prof[ptype] = NULL;
	}

	rte_free(hw->fdir_prof);
	hw->fdir_prof = NULL;
}

/* Remove a profile for some filter type */
static void
ice_fdir_prof_rm(struct ice_pf *pf, enum ice_fltr_ptype ptype, bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fd_hw_prof *hw_prof;
	uint64_t prof_id;
	uint16_t vsi_num;
	int i;

	if (!hw->fdir_prof || !hw->fdir_prof[ptype])
		return;

	hw_prof = hw->fdir_prof[ptype];

	prof_id = ptype + is_tunnel * ICE_FLTR_PTYPE_MAX;
	for (i = 0; i < pf->hw_prof_cnt[ptype][is_tunnel]; i++) {
		if (hw_prof->entry_h[i][is_tunnel]) {
			vsi_num = ice_get_hw_vsi_num(hw,
						     hw_prof->vsi_h[i]);
			ice_rem_prof_id_flow(hw, ICE_BLK_FD,
					     vsi_num, ptype);
			ice_flow_rem_entry(hw,
					   hw_prof->entry_h[i][is_tunnel]);
			hw_prof->entry_h[i][is_tunnel] = 0;
		}
	}
	ice_flow_rem_prof(hw, ICE_BLK_FD, prof_id);
	rte_free(hw_prof->fdir_seg[is_tunnel]);
	hw_prof->fdir_seg[is_tunnel] = NULL;

	for (i = 0; i < hw_prof->cnt; i++)
		hw_prof->vsi_h[i] = 0;
	pf->hw_prof_cnt[ptype][is_tunnel] = 0;
}

/* Remove all created profiles */
static void
ice_fdir_prof_rm_all(struct ice_pf *pf)
{
	enum ice_fltr_ptype ptype;

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		ice_fdir_prof_rm(pf, ptype, false);
		ice_fdir_prof_rm(pf, ptype, true);
	}
}

/*
 * ice_fdir_teardown - release the Flow Director resources
 * @pf: board private structure
 */
static void
ice_fdir_teardown(struct ice_pf *pf)
{
	struct rte_eth_dev *eth_dev = pf->adapter->eth_dev;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi;
	int err;

	vsi = pf->fdir.fdir_vsi;
	if (!vsi)
		return;

	ice_vsi_disable_queues_intr(vsi);

	err = ice_fdir_tx_queue_stop(eth_dev, pf->fdir.txq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop TX queue.");

	err = ice_fdir_rx_queue_stop(eth_dev, pf->fdir.rxq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop RX queue.");

	err = ice_fdir_counter_release(pf);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to release FDIR counter resource.");

	ice_fdir_release_filter_list(pf);

	ice_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
	ice_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
	ice_fdir_prof_rm_all(pf);
	ice_fdir_prof_free(hw);
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;

	if (pf->fdir.mz) {
		err = rte_memzone_free(pf->fdir.mz);
		pf->fdir.mz = NULL;
		if (err)
			PMD_DRV_LOG(ERR, "Failed to free FDIR memezone.");
	}
}

static int
ice_fdir_cur_prof_conflict(struct ice_pf *pf,
			   enum ice_fltr_ptype ptype,
			   struct ice_flow_seg_info *seg,
			   bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_flow_seg_info *ori_seg;
	struct ice_fd_hw_prof *hw_prof;

	hw_prof = hw->fdir_prof[ptype];
	ori_seg = hw_prof->fdir_seg[is_tunnel];

	/* profile does not exist */
	if (!ori_seg)
		return 0;

	/* if no input set conflict, return -EEXIST */
	if ((!is_tunnel && !memcmp(ori_seg, seg, sizeof(*seg))) ||
	    (is_tunnel && !memcmp(&ori_seg[1], &seg[1], sizeof(*seg)))) {
		PMD_DRV_LOG(DEBUG, "Profile already exists for flow type %d.",
			    ptype);
		return -EEXIST;
	}

	/* a rule with input set conflict already exist, so give up */
	if (pf->fdir_fltr_cnt[ptype][is_tunnel]) {
		PMD_DRV_LOG(DEBUG, "Failed to create profile for flow type %d due to conflict with existing rule.",
			    ptype);
		return -EINVAL;
	}

	/* it's safe to delete an empty profile */
	ice_fdir_prof_rm(pf, ptype, is_tunnel);
	return 0;
}

static bool
ice_fdir_prof_resolve_conflict(struct ice_pf *pf,
			       enum ice_fltr_ptype ptype,
			       bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fd_hw_prof *hw_prof;
	struct ice_flow_seg_info *seg;

	hw_prof = hw->fdir_prof[ptype];
	seg = hw_prof->fdir_seg[is_tunnel];

	/* profile does not exist */
	if (!seg)
		return true;

	/* profile exists and rule exists, fail to resolve the conflict */
	if (pf->fdir_fltr_cnt[ptype][is_tunnel] != 0)
		return false;

	/* it's safe to delete an empty profile */
	ice_fdir_prof_rm(pf, ptype, is_tunnel);

	return true;
}

static int
ice_fdir_cross_prof_conflict(struct ice_pf *pf,
			     enum ice_fltr_ptype ptype,
			     bool is_tunnel)
{
	enum ice_fltr_ptype cflct_ptype;

	switch (ptype) {
	/* IPv4 */
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	/* IPv4 GTPU */
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	/* IPv6 */
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_SCTP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	default:
		break;
	}
	return 0;
err:
	PMD_DRV_LOG(DEBUG, "Failed to create profile for flow type %d due to conflict with existing rule of flow type %d.",
		    ptype, cflct_ptype);
	return -EINVAL;
}

static int
ice_fdir_hw_tbl_conf(struct ice_pf *pf, struct ice_vsi *vsi,
		     struct ice_vsi *ctrl_vsi,
		     struct ice_flow_seg_info *seg,
		     enum ice_fltr_ptype ptype,
		     bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	enum ice_flow_dir dir = ICE_FLOW_RX;
	struct ice_fd_hw_prof *hw_prof;
	struct ice_flow_prof *prof;
	uint64_t entry_1 = 0;
	uint64_t entry_2 = 0;
	uint16_t vsi_num;
	int ret;
	uint64_t prof_id;

	/* check if have input set conflict on current profile. */
	ret = ice_fdir_cur_prof_conflict(pf, ptype, seg, is_tunnel);
	if (ret)
		return ret;

	/* check if the profile is conflict with other profile. */
	ret = ice_fdir_cross_prof_conflict(pf, ptype, is_tunnel);
	if (ret)
		return ret;

	prof_id = ptype + is_tunnel * ICE_FLTR_PTYPE_MAX;
	ret = ice_flow_add_prof(hw, ICE_BLK_FD, dir, prof_id, seg,
				(is_tunnel) ? 2 : 1, NULL, 0, &prof);
	if (ret)
		return ret;
	ret = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, vsi->idx,
				 vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, NULL, 0, &entry_1);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to add main VSI flow entry for %d.",
			    ptype);
		goto err_add_prof;
	}
	ret = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, vsi->idx,
				 ctrl_vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, NULL, 0, &entry_2);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to add control VSI flow entry for %d.",
			    ptype);
		goto err_add_entry;
	}

	hw_prof = hw->fdir_prof[ptype];
	pf->hw_prof_cnt[ptype][is_tunnel] = 0;
	hw_prof->cnt = 0;
	hw_prof->fdir_seg[is_tunnel] = seg;
	hw_prof->vsi_h[hw_prof->cnt] = vsi->idx;
	hw_prof->entry_h[hw_prof->cnt++][is_tunnel] = entry_1;
	pf->hw_prof_cnt[ptype][is_tunnel]++;
	hw_prof->vsi_h[hw_prof->cnt] = ctrl_vsi->idx;
	hw_prof->entry_h[hw_prof->cnt++][is_tunnel] = entry_2;
	pf->hw_prof_cnt[ptype][is_tunnel]++;

	return ret;

err_add_entry:
	vsi_num = ice_get_hw_vsi_num(hw, vsi->idx);
	ice_rem_prof_id_flow(hw, ICE_BLK_FD, vsi_num, prof_id);
	ice_flow_rem_entry(hw, entry_1);
err_add_prof:
	ice_flow_rem_prof(hw, ICE_BLK_FD, prof_id);

	return ret;
}

static void
ice_fdir_input_set_parse(uint64_t inset, enum ice_flow_field *field)
{
	uint32_t i, j;

	struct ice_inset_map {
		uint64_t inset;
		enum ice_flow_field fld;
	};
	static const struct ice_inset_map ice_inset_map[] = {
		{ICE_INSET_DMAC, ICE_FLOW_FIELD_IDX_ETH_DA},
		{ICE_INSET_IPV4_SRC, ICE_FLOW_FIELD_IDX_IPV4_SA},
		{ICE_INSET_IPV4_DST, ICE_FLOW_FIELD_IDX_IPV4_DA},
		{ICE_INSET_IPV4_TOS, ICE_FLOW_FIELD_IDX_IPV4_DSCP},
		{ICE_INSET_IPV4_TTL, ICE_FLOW_FIELD_IDX_IPV4_TTL},
		{ICE_INSET_IPV4_PROTO, ICE_FLOW_FIELD_IDX_IPV4_PROT},
		{ICE_INSET_IPV6_SRC, ICE_FLOW_FIELD_IDX_IPV6_SA},
		{ICE_INSET_IPV6_DST, ICE_FLOW_FIELD_IDX_IPV6_DA},
		{ICE_INSET_IPV6_TC, ICE_FLOW_FIELD_IDX_IPV6_DSCP},
		{ICE_INSET_IPV6_NEXT_HDR, ICE_FLOW_FIELD_IDX_IPV6_PROT},
		{ICE_INSET_IPV6_HOP_LIMIT, ICE_FLOW_FIELD_IDX_IPV6_TTL},
		{ICE_INSET_TCP_SRC_PORT, ICE_FLOW_FIELD_IDX_TCP_SRC_PORT},
		{ICE_INSET_TCP_DST_PORT, ICE_FLOW_FIELD_IDX_TCP_DST_PORT},
		{ICE_INSET_UDP_SRC_PORT, ICE_FLOW_FIELD_IDX_UDP_SRC_PORT},
		{ICE_INSET_UDP_DST_PORT, ICE_FLOW_FIELD_IDX_UDP_DST_PORT},
		{ICE_INSET_SCTP_SRC_PORT, ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT},
		{ICE_INSET_SCTP_DST_PORT, ICE_FLOW_FIELD_IDX_SCTP_DST_PORT},
		{ICE_INSET_TUN_IPV4_SRC, ICE_FLOW_FIELD_IDX_IPV4_SA},
		{ICE_INSET_TUN_IPV4_DST, ICE_FLOW_FIELD_IDX_IPV4_DA},
		{ICE_INSET_TUN_TCP_SRC_PORT, ICE_FLOW_FIELD_IDX_TCP_SRC_PORT},
		{ICE_INSET_TUN_TCP_DST_PORT, ICE_FLOW_FIELD_IDX_TCP_DST_PORT},
		{ICE_INSET_TUN_UDP_SRC_PORT, ICE_FLOW_FIELD_IDX_UDP_SRC_PORT},
		{ICE_INSET_TUN_UDP_DST_PORT, ICE_FLOW_FIELD_IDX_UDP_DST_PORT},
		{ICE_INSET_TUN_SCTP_SRC_PORT, ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT},
		{ICE_INSET_TUN_SCTP_DST_PORT, ICE_FLOW_FIELD_IDX_SCTP_DST_PORT},
		{ICE_INSET_GTPU_TEID, ICE_FLOW_FIELD_IDX_GTPU_IP_TEID},
		{ICE_INSET_GTPU_QFI, ICE_FLOW_FIELD_IDX_GTPU_EH_QFI},
	};

	for (i = 0, j = 0; i < RTE_DIM(ice_inset_map); i++) {
		if ((inset & ice_inset_map[i].inset) ==
		    ice_inset_map[i].inset)
			field[j++] = ice_inset_map[i].fld;
	}
}

static int
ice_fdir_input_set_conf(struct ice_pf *pf, enum ice_fltr_ptype flow,
			uint64_t input_set, enum ice_fdir_tunnel_type ttype)
{
	struct ice_flow_seg_info *seg;
	struct ice_flow_seg_info *seg_tun = NULL;
	enum ice_flow_field field[ICE_FLOW_FIELD_IDX_MAX];
	bool is_tunnel;
	int i, ret;

	if (!input_set)
		return -EINVAL;

	seg = (struct ice_flow_seg_info *)
		ice_malloc(hw, sizeof(*seg));
	if (!seg) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	for (i = 0; i < ICE_FLOW_FIELD_IDX_MAX; i++)
		field[i] = ICE_FLOW_FIELD_IDX_MAX;
	ice_fdir_input_set_parse(input_set, field);

	switch (flow) {
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER:
		if (ttype == ICE_FDIR_TUNNEL_TYPE_GTPU)
			ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_IP |
					  ICE_FLOW_SEG_HDR_IPV4);
		else if (ttype == ICE_FDIR_TUNNEL_TYPE_GTPU_EH)
			ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_EH |
					  ICE_FLOW_SEG_HDR_GTPU_IP |
					  ICE_FLOW_SEG_HDR_IPV4);
		else
			PMD_DRV_LOG(ERR, "not supported tunnel type.");
		break;
	default:
		PMD_DRV_LOG(ERR, "not supported filter type.");
		break;
	}

	for (i = 0; field[i] != ICE_FLOW_FIELD_IDX_MAX; i++) {
		ice_flow_set_fld(seg, field[i],
				 ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	}

	is_tunnel = ice_fdir_is_tunnel_profile(ttype);
	if (!is_tunnel) {
		ret = ice_fdir_hw_tbl_conf(pf, pf->main_vsi, pf->fdir.fdir_vsi,
					   seg, flow, false);
	} else {
		seg_tun = (struct ice_flow_seg_info *)
			ice_malloc(hw, sizeof(*seg) * ICE_FD_HW_SEG_MAX);
		if (!seg_tun) {
			PMD_DRV_LOG(ERR, "No memory can be allocated");
			rte_free(seg);
			return -ENOMEM;
		}
		rte_memcpy(&seg_tun[1], seg, sizeof(*seg));
		ret = ice_fdir_hw_tbl_conf(pf, pf->main_vsi, pf->fdir.fdir_vsi,
					   seg_tun, flow, true);
	}

	if (!ret) {
		return ret;
	} else if (ret < 0) {
		rte_free(seg);
		if (is_tunnel)
			rte_free(seg_tun);
		return (ret == -EEXIST) ? 0 : ret;
	} else {
		return ret;
	}
}

static void
ice_fdir_cnt_update(struct ice_pf *pf, enum ice_fltr_ptype ptype,
		    bool is_tunnel, bool add)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int cnt;

	cnt = (add) ? 1 : -1;
	hw->fdir_active_fltr += cnt;
	if (ptype == ICE_FLTR_PTYPE_NONF_NONE || ptype >= ICE_FLTR_PTYPE_MAX)
		PMD_DRV_LOG(ERR, "Unknown filter type %d", ptype);
	else
		pf->fdir_fltr_cnt[ptype][is_tunnel] += cnt;
}

static int
ice_fdir_init(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_flow_parser *parser;
	int ret;

	ret = ice_fdir_setup(pf);
	if (ret)
		return ret;

	if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		parser = &ice_fdir_parser_comms;
	else if (ad->active_pkg_type == ICE_PKG_TYPE_OS_DEFAULT)
		parser = &ice_fdir_parser_os;
	else
		return -EINVAL;

	return ice_register_parser(parser, ad);
}

static void
ice_fdir_uninit(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_flow_parser *parser;

	if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		parser = &ice_fdir_parser_comms;
	else
		parser = &ice_fdir_parser_os;

	ice_unregister_parser(parser, ad);

	ice_fdir_teardown(pf);
}

static int
ice_fdir_is_tunnel_profile(enum ice_fdir_tunnel_type tunnel_type)
{
	if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN)
		return 1;
	else
		return 0;
}

static int
ice_fdir_add_del_filter(struct ice_pf *pf,
			struct ice_fdir_filter_conf *filter,
			bool add)
{
	struct ice_fltr_desc desc;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	bool is_tun;
	int ret;

	filter->input.dest_vsi = pf->main_vsi->idx;

	memset(&desc, 0, sizeof(desc));
	ice_fdir_get_prgm_desc(hw, &filter->input, &desc, add);

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	memset(pkt, 0, ICE_FDIR_PKT_LEN);
	ret = ice_fdir_get_gen_prgm_pkt(hw, &filter->input, pkt, false, is_tun);
	if (ret) {
		PMD_DRV_LOG(ERR, "Generate dummy packet failed");
		return -EINVAL;
	}

	return ice_fdir_programming(pf, &desc);
}

static void
ice_fdir_extract_fltr_key(struct ice_fdir_fltr_pattern *key,
			  struct ice_fdir_filter_conf *filter)
{
	struct ice_fdir_fltr *input = &filter->input;
	memset(key, 0, sizeof(*key));

	key->flow_type = input->flow_type;
	rte_memcpy(&key->ip, &input->ip, sizeof(key->ip));
	rte_memcpy(&key->mask, &input->mask, sizeof(key->mask));
	rte_memcpy(&key->ext_data, &input->ext_data, sizeof(key->ext_data));
	rte_memcpy(&key->ext_mask, &input->ext_mask, sizeof(key->ext_mask));

	rte_memcpy(&key->gtpu_data, &input->gtpu_data, sizeof(key->gtpu_data));
	rte_memcpy(&key->gtpu_mask, &input->gtpu_mask, sizeof(key->gtpu_mask));

	key->tunnel_type = filter->tunnel_type;
}

/* Check if there exists the flow director filter */
static struct ice_fdir_filter_conf *
ice_fdir_entry_lookup(struct ice_fdir_info *fdir_info,
			const struct ice_fdir_fltr_pattern *key)
{
	int ret;

	ret = rte_hash_lookup(fdir_info->hash_table, key);
	if (ret < 0)
		return NULL;

	return fdir_info->hash_map[ret];
}

/* Add a flow director entry into the SW list */
static int
ice_fdir_entry_insert(struct ice_pf *pf,
		      struct ice_fdir_filter_conf *entry,
		      struct ice_fdir_fltr_pattern *key)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = rte_hash_add_key(fdir_info->hash_table, key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir entry to hash table %d!",
			    ret);
		return ret;
	}
	fdir_info->hash_map[ret] = entry;

	return 0;
}

/* Delete a flow director entry from the SW list */
static int
ice_fdir_entry_del(struct ice_pf *pf, struct ice_fdir_fltr_pattern *key)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = rte_hash_del_key(fdir_info->hash_table, key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir filter to hash table %d!",
			    ret);
		return ret;
	}
	fdir_info->hash_map[ret] = NULL;

	return 0;
}

static int
ice_fdir_create_filter(struct ice_adapter *ad,
		       struct rte_flow *flow,
		       void *meta,
		       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = meta;
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_filter_conf *entry, *node;
	struct ice_fdir_fltr_pattern key;
	bool is_tun;
	int ret;

	ice_fdir_extract_fltr_key(&key, filter);
	node = ice_fdir_entry_lookup(fdir_info, &key);
	if (node) {
		rte_flow_error_set(error, EEXIST,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Rule already exists!");
		return -rte_errno;
	}

	entry = rte_zmalloc("fdir_entry", sizeof(*entry), 0);
	if (!entry) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return -rte_errno;
	}

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	ret = ice_fdir_input_set_conf(pf, filter->input.flow_type,
			filter->input_set, filter->tunnel_type);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Profile configure failed.");
		goto free_entry;
	}

	/* alloc counter for FDIR */
	if (filter->input.cnt_ena) {
		struct rte_flow_action_count *act_count = &filter->act_count;

		filter->counter = ice_fdir_counter_alloc(pf,
							 act_count->shared,
							 act_count->id);
		if (!filter->counter) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Failed to alloc FDIR counter.");
			goto free_entry;
		}
		filter->input.cnt_index = filter->counter->hw_index;
	}

	ret = ice_fdir_add_del_filter(pf, filter, true);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Add filter rule failed.");
		goto free_counter;
	}

	rte_memcpy(entry, filter, sizeof(*entry));
	ret = ice_fdir_entry_insert(pf, entry, &key);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Insert entry to table failed.");
		goto free_entry;
	}

	flow->rule = entry;
	ice_fdir_cnt_update(pf, filter->input.flow_type, is_tun, true);

	return 0;

free_counter:
	if (filter->counter) {
		ice_fdir_counter_free(pf, filter->counter);
		filter->counter = NULL;
	}

free_entry:
	rte_free(entry);
	return -rte_errno;
}

static int
ice_fdir_destroy_filter(struct ice_adapter *ad,
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_filter_conf *filter, *entry;
	struct ice_fdir_fltr_pattern key;
	bool is_tun;
	int ret;

	filter = (struct ice_fdir_filter_conf *)flow->rule;

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	if (filter->counter) {
		ice_fdir_counter_free(pf, filter->counter);
		filter->counter = NULL;
	}

	ice_fdir_extract_fltr_key(&key, filter);
	entry = ice_fdir_entry_lookup(fdir_info, &key);
	if (!entry) {
		rte_flow_error_set(error, ENOENT,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Can't find entry.");
		return -rte_errno;
	}

	ret = ice_fdir_add_del_filter(pf, filter, false);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Del filter rule failed.");
		return -rte_errno;
	}

	ret = ice_fdir_entry_del(pf, &key);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Remove entry from table failed.");
		return -rte_errno;
	}

	ice_fdir_cnt_update(pf, filter->input.flow_type, is_tun, false);
	flow->rule = NULL;

	rte_free(filter);

	return 0;
}

static int
ice_fdir_query_count(struct ice_adapter *ad,
		      struct rte_flow *flow,
		      struct rte_flow_query_count *flow_stats,
		      struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_filter_conf *filter = flow->rule;
	struct ice_fdir_counter *counter = filter->counter;
	uint64_t hits_lo, hits_hi;

	if (!counter) {
		rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_ACTION,
				  NULL,
				  "FDIR counters not available");
		return -rte_errno;
	}

	/*
	 * Reading the low 32-bits latches the high 32-bits into a shadow
	 * register. Reading the high 32-bit returns the value in the
	 * shadow register.
	 */
	hits_lo = ICE_READ_REG(hw, GLSTAT_FD_CNT0L(counter->hw_index));
	hits_hi = ICE_READ_REG(hw, GLSTAT_FD_CNT0H(counter->hw_index));

	flow_stats->hits_set = 1;
	flow_stats->hits = hits_lo | (hits_hi << 32);
	flow_stats->bytes_set = 0;
	flow_stats->bytes = 0;

	if (flow_stats->reset) {
		/* reset statistic counter value */
		ICE_WRITE_REG(hw, GLSTAT_FD_CNT0H(counter->hw_index), 0);
		ICE_WRITE_REG(hw, GLSTAT_FD_CNT0L(counter->hw_index), 0);
	}

	return 0;
}

static struct ice_flow_engine ice_fdir_engine = {
	.init = ice_fdir_init,
	.uninit = ice_fdir_uninit,
	.create = ice_fdir_create_filter,
	.destroy = ice_fdir_destroy_filter,
	.query_count = ice_fdir_query_count,
	.type = ICE_FLOW_ENGINE_FDIR,
};

static int
ice_fdir_parse_action_qregion(struct ice_pf *pf,
			      struct rte_flow_error *error,
			      const struct rte_flow_action *act,
			      struct ice_fdir_filter_conf *filter)
{
	const struct rte_flow_action_rss *rss = act->conf;
	uint32_t i;

	if (act->type != RTE_FLOW_ACTION_TYPE_RSS) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid action.");
		return -rte_errno;
	}

	if (rss->queue_num <= 1) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Queue region size can't be 0 or 1.");
		return -rte_errno;
	}

	/* check if queue index for queue region is continuous */
	for (i = 0; i < rss->queue_num - 1; i++) {
		if (rss->queue[i + 1] != rss->queue[i] + 1) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Discontinuous queue region");
			return -rte_errno;
		}
	}

	if (rss->queue[rss->queue_num - 1] >= pf->dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid queue region indexes.");
		return -rte_errno;
	}

	if (!(rte_is_power_of_2(rss->queue_num) &&
	     (rss->queue_num <= ICE_FDIR_MAX_QREGION_SIZE))) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "The region size should be any of the following values:"
				   "1, 2, 4, 8, 16, 32, 64, 128 as long as the total number "
				   "of queues do not exceed the VSI allocation.");
		return -rte_errno;
	}

	filter->input.q_index = rss->queue[0];
	filter->input.q_region = rte_fls_u32(rss->queue_num) - 1;
	filter->input.dest_ctl = ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QGROUP;

	return 0;
}

static int
ice_fdir_parse_action(struct ice_adapter *ad,
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error,
		      struct ice_fdir_filter_conf *filter)
{
	struct ice_pf *pf = &ad->pf;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark_spec = NULL;
	const struct rte_flow_action_count *act_count;
	uint32_t dest_num = 0;
	uint32_t mark_num = 0;
	uint32_t counter_num = 0;
	int ret;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_num++;

			act_q = actions->conf;
			filter->input.q_index = act_q->index;
			if (filter->input.q_index >=
					pf->dev_data->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "Invalid queue for FDIR.");
				return -rte_errno;
			}
			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			dest_num++;

			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DROP_PKT;
			break;
		case RTE_FLOW_ACTION_TYPE_PASSTHRU:
			dest_num++;

			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			dest_num++;

			ret = ice_fdir_parse_action_qregion(pf,
						error, actions, filter);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark_num++;

			mark_spec = actions->conf;
			filter->input.fltr_id = mark_spec->id;
			filter->input.fdid_prio = ICE_FXD_FLTR_QW1_FDID_PRI_ONE;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			counter_num++;

			act_count = actions->conf;
			filter->input.cnt_ena = ICE_FXD_FLTR_QW0_STAT_ENA_PKTS;
			rte_memcpy(&filter->act_count, act_count,
						sizeof(filter->act_count));

			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action.");
			return -rte_errno;
		}
	}

	if (dest_num == 0 || dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Unsupported action combination");
		return -rte_errno;
	}

	if (mark_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many mark actions");
		return -rte_errno;
	}

	if (counter_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many count actions");
		return -rte_errno;
	}

	return 0;
}

static int
ice_fdir_parse_pattern(__rte_unused struct ice_adapter *ad,
		       const struct rte_flow_item pattern[],
		       struct rte_flow_error *error,
		       struct ice_fdir_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	enum ice_fdir_tunnel_type tunnel_type = ICE_FDIR_TUNNEL_TYPE_NONE;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	const struct rte_flow_item_gtp *gtp_spec, *gtp_mask;
	const struct rte_flow_item_gtp_psc *gtp_psc_spec, *gtp_psc_mask;
	uint64_t input_set = ICE_INSET_NONE;
	uint8_t flow_type = ICE_FLTR_PTYPE_NONF_NONE;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint32_t vtc_flow_cpu;


	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support range");
			return -rte_errno;
		}
		item_type = item->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			if (eth_spec && eth_mask) {
				if (!rte_is_zero_ether_addr(&eth_spec->src) ||
				    !rte_is_zero_ether_addr(&eth_mask->src)) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Src mac not support");
					return -rte_errno;
				}

				if (!rte_is_broadcast_ether_addr(&eth_mask->dst)) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid mac addr mask");
					return -rte_errno;
				}

				input_set |= ICE_INSET_DMAC;
				rte_memcpy(&filter->input.ext_data.dst_mac,
					   &eth_spec->dst,
					   RTE_ETHER_ADDR_LEN);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			if (ipv4_spec && ipv4_mask) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
				    ipv4_mask->hdr.total_length ||
				    ipv4_mask->hdr.packet_id ||
				    ipv4_mask->hdr.fragment_offset ||
				    ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
					return -rte_errno;
				}
				if (ipv4_mask->hdr.src_addr == UINT32_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_IPV4_SRC :
						     ICE_INSET_IPV4_SRC;
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_IPV4_DST :
						     ICE_INSET_IPV4_DST;
				if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TOS;
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TTL;
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_PROTO;

				filter->input.ip.v4.dst_ip =
					ipv4_spec->hdr.src_addr;
				filter->input.ip.v4.src_ip =
					ipv4_spec->hdr.dst_addr;
				filter->input.ip.v4.tos =
					ipv4_spec->hdr.type_of_service;
				filter->input.ip.v4.ttl =
					ipv4_spec->hdr.time_to_live;
				filter->input.ip.v4.proto =
					ipv4_spec->hdr.next_proto_id;
			}

			flow_type = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			if (ipv6_spec && ipv6_mask) {
				/* Check IPv6 mask and update input set */
				if (ipv6_mask->hdr.payload_len) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
					return -rte_errno;
				}

				if (!memcmp(ipv6_mask->hdr.src_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
					input_set |= ICE_INSET_IPV6_SRC;
				if (!memcmp(ipv6_mask->hdr.dst_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
					input_set |= ICE_INSET_IPV6_DST;

				if ((ipv6_mask->hdr.vtc_flow &
				     rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
				    == rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
					input_set |= ICE_INSET_IPV6_TC;
				if (ipv6_mask->hdr.proto == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_NEXT_HDR;
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_HOP_LIMIT;

				rte_memcpy(filter->input.ip.v6.dst_ip,
					   ipv6_spec->hdr.src_addr, 16);
				rte_memcpy(filter->input.ip.v6.src_ip,
					   ipv6_spec->hdr.dst_addr, 16);

				vtc_flow_cpu =
				      rte_be_to_cpu_32(ipv6_spec->hdr.vtc_flow);
				filter->input.ip.v6.tc =
					(uint8_t)(vtc_flow_cpu >>
						  ICE_FDIR_IPV6_TC_OFFSET);
				filter->input.ip.v6.proto =
					ipv6_spec->hdr.proto;
				filter->input.ip.v6.hlim =
					ipv6_spec->hdr.hop_limits;
			}

			flow_type = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_TCP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_TCP;

			if (tcp_spec && tcp_mask) {
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
				    tcp_mask->hdr.recv_ack ||
				    tcp_mask->hdr.data_off ||
				    tcp_mask->hdr.tcp_flags ||
				    tcp_mask->hdr.rx_win ||
				    tcp_mask->hdr.cksum ||
				    tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_TCP_SRC_PORT :
						     ICE_INSET_TCP_SRC_PORT;
				if (tcp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_TCP_DST_PORT :
						     ICE_INSET_TCP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						tcp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						tcp_spec->hdr.dst_port;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.dst_port =
						tcp_spec->hdr.src_port;
					filter->input.ip.v6.src_port =
						tcp_spec->hdr.dst_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_UDP;

			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_UDP_SRC_PORT :
						     ICE_INSET_UDP_SRC_PORT;
				if (udp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_UDP_DST_PORT :
						     ICE_INSET_UDP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						udp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						udp_spec->hdr.dst_port;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.src_port =
						udp_spec->hdr.dst_port;
					filter->input.ip.v6.dst_port =
						udp_spec->hdr.src_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_SCTP;

			if (sctp_spec && sctp_mask) {
				/* Check SCTP mask and update input set */
				if (sctp_mask->hdr.cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (sctp_mask->hdr.src_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_SCTP_SRC_PORT :
						     ICE_INSET_SCTP_SRC_PORT;
				if (sctp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= tunnel_type ?
						     ICE_INSET_TUN_SCTP_DST_PORT :
						     ICE_INSET_SCTP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						sctp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						sctp_spec->hdr.dst_port;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.dst_port =
						sctp_spec->hdr.src_port;
					filter->input.ip.v6.src_port =
						sctp_spec->hdr.dst_port;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			l3 = RTE_FLOW_ITEM_TYPE_END;
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;

			if (vxlan_spec || vxlan_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vxlan field");
				return -rte_errno;
			}

			tunnel_type = ICE_FDIR_TUNNEL_TYPE_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_GTPU:
			l3 = RTE_FLOW_ITEM_TYPE_END;
			gtp_spec = item->spec;
			gtp_mask = item->mask;

			if (gtp_spec && gtp_mask) {
				if (gtp_mask->v_pt_rsv_flags ||
				    gtp_mask->msg_type ||
				    gtp_mask->msg_len) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GTP mask");
					return -rte_errno;
				}

				if (gtp_mask->teid == UINT32_MAX)
					input_set |= ICE_INSET_GTPU_TEID;

				filter->input.gtpu_data.teid = gtp_spec->teid;
			}

			tunnel_type = ICE_FDIR_TUNNEL_TYPE_GTPU;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			gtp_psc_spec = item->spec;
			gtp_psc_mask = item->mask;

			if (gtp_psc_spec && gtp_psc_mask) {
				if (gtp_psc_mask->qfi == UINT8_MAX)
					input_set |= ICE_INSET_GTPU_QFI;

				filter->input.gtpu_data.qfi =
					gtp_psc_spec->qfi;
			}
			tunnel_type = ICE_FDIR_TUNNEL_TYPE_GTPU_EH;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Invalid pattern item.");
			return -rte_errno;
		}
	}

	if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU ||
	    tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU_EH)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;

	filter->tunnel_type = tunnel_type;
	filter->input.flow_type = flow_type;
	filter->input_set = input_set;

	return 0;
}

static int
ice_fdir_parse(struct ice_adapter *ad,
	       struct ice_pattern_match_item *array,
	       uint32_t array_len,
	       const struct rte_flow_item pattern[],
	       const struct rte_flow_action actions[],
	       void **meta,
	       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = &pf->fdir.conf;
	struct ice_pattern_match_item *item = NULL;
	uint64_t input_set;
	int ret;

	memset(filter, 0, sizeof(*filter));
	item = ice_search_pattern_match_item(pattern, array, array_len, error);
	if (!item)
		return -rte_errno;

	ret = ice_fdir_parse_pattern(ad, pattern, error, filter);
	if (ret)
		goto error;
	input_set = filter->input_set;
	if (!input_set || input_set & ~item->input_set_mask) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		ret = -rte_errno;
		goto error;
	}

	ret = ice_fdir_parse_action(ad, actions, error, filter);
	if (ret)
		goto error;

	*meta = filter;
error:
	rte_free(item);
	return ret;
}

static struct ice_flow_parser ice_fdir_parser_os = {
	.engine = &ice_fdir_engine,
	.array = ice_fdir_pattern_os,
	.array_len = RTE_DIM(ice_fdir_pattern_os),
	.parse_pattern_action = ice_fdir_parse,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

static struct ice_flow_parser ice_fdir_parser_comms = {
	.engine = &ice_fdir_engine,
	.array = ice_fdir_pattern_comms,
	.array_len = RTE_DIM(ice_fdir_pattern_comms),
	.parse_pattern_action = ice_fdir_parse,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(ice_fdir_engine_register)
{
	ice_register_flow_engine(&ice_fdir_engine);
}
