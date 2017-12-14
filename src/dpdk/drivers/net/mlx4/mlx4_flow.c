/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
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

/**
 * @file
 * Flow API operations for mlx4 driver.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_eth_ctrl.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>

/* PMD headers. */
#include "mlx4.h"
#include "mlx4_flow.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/** Static initializer for a list of subsequent item types. */
#define NEXT_ITEM(...) \
	(const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	}

/** Processor structure associated with a flow item. */
struct mlx4_flow_proc_item {
	/** Bit-mask for fields supported by this PMD. */
	const void *mask_support;
	/** Bit-mask to use when @p item->mask is not provided. */
	const void *mask_default;
	/** Size in bytes for @p mask_support and @p mask_default. */
	const unsigned int mask_sz;
	/** Merge a pattern item into a flow rule handle. */
	int (*merge)(struct rte_flow *flow,
		     const struct rte_flow_item *item,
		     const struct mlx4_flow_proc_item *proc,
		     struct rte_flow_error *error);
	/** Size in bytes of the destination structure. */
	const unsigned int dst_sz;
	/** List of possible subsequent items. */
	const enum rte_flow_item_type *const next_item;
};

/** Shared resources for drop flow rules. */
struct mlx4_drop {
	struct ibv_qp *qp; /**< QP target. */
	struct ibv_cq *cq; /**< CQ associated with above QP. */
	struct priv *priv; /**< Back pointer to private data. */
	uint32_t refcnt; /**< Reference count. */
};

/**
 * Convert DPDK RSS hash fields to their Verbs equivalent.
 *
 * @param rss_hf
 *   Hash fields in DPDK format (see struct rte_eth_rss_conf).
 *
 * @return
 *   A valid Verbs RSS hash fields mask for mlx4 on success, (uint64_t)-1
 *   otherwise and rte_errno is set.
 */
static uint64_t
mlx4_conv_rss_hf(uint64_t rss_hf)
{
	enum { IPV4, IPV6, TCP, UDP, };
	const uint64_t in[] = {
		[IPV4] = (ETH_RSS_IPV4 |
			  ETH_RSS_FRAG_IPV4 |
			  ETH_RSS_NONFRAG_IPV4_TCP |
			  ETH_RSS_NONFRAG_IPV4_UDP |
			  ETH_RSS_NONFRAG_IPV4_OTHER),
		[IPV6] = (ETH_RSS_IPV6 |
			  ETH_RSS_FRAG_IPV6 |
			  ETH_RSS_NONFRAG_IPV6_TCP |
			  ETH_RSS_NONFRAG_IPV6_UDP |
			  ETH_RSS_NONFRAG_IPV6_OTHER |
			  ETH_RSS_IPV6_EX |
			  ETH_RSS_IPV6_TCP_EX |
			  ETH_RSS_IPV6_UDP_EX),
		[TCP] = (ETH_RSS_NONFRAG_IPV4_TCP |
			 ETH_RSS_NONFRAG_IPV6_TCP |
			 ETH_RSS_IPV6_TCP_EX),
		/*
		 * UDP support is temporarily disabled due to an
		 * implementation issue in the kernel.
		 */
		[UDP] = 0,
	};
	const uint64_t out[RTE_DIM(in)] = {
		[IPV4] = IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4,
		[IPV6] = IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6,
		[TCP] = IBV_RX_HASH_SRC_PORT_TCP | IBV_RX_HASH_DST_PORT_TCP,
		[UDP] = IBV_RX_HASH_SRC_PORT_UDP | IBV_RX_HASH_DST_PORT_UDP,
	};
	uint64_t seen = 0;
	uint64_t conv = 0;
	unsigned int i;

	for (i = 0; i != RTE_DIM(in); ++i)
		if (rss_hf & in[i]) {
			seen |= rss_hf & in[i];
			conv |= out[i];
		}
	if (!(rss_hf & ~seen))
		return conv;
	rte_errno = ENOTSUP;
	return (uint64_t)-1;
}

/**
 * Merge Ethernet pattern item into flow rule handle.
 *
 * Additional mlx4-specific constraints on supported fields:
 *
 * - No support for partial masks, except in the specific case of matching
 *   all multicast traffic (@p spec->dst and @p mask->dst equal to
 *   01:00:00:00:00:00).
 * - Not providing @p item->spec or providing an empty @p mask->dst is
 *   *only* supported if the rule doesn't specify additional matching
 *   criteria (i.e. rule is promiscuous-like).
 *
 * @param[in, out] flow
 *   Flow rule handle to update.
 * @param[in] item
 *   Pattern item to merge.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_merge_eth(struct rte_flow *flow,
		    const struct rte_flow_item *item,
		    const struct mlx4_flow_proc_item *proc,
		    struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask =
		spec ? (item->mask ? item->mask : proc->mask_default) : NULL;
	struct ibv_flow_spec_eth *eth;
	const char *msg;
	unsigned int i;

	if (!mask) {
		flow->promisc = 1;
	} else {
		uint32_t sum_dst = 0;
		uint32_t sum_src = 0;

		for (i = 0; i != sizeof(mask->dst.addr_bytes); ++i) {
			sum_dst += mask->dst.addr_bytes[i];
			sum_src += mask->src.addr_bytes[i];
		}
		if (sum_src) {
			msg = "mlx4 does not support source MAC matching";
			goto error;
		} else if (!sum_dst) {
			flow->promisc = 1;
		} else if (sum_dst == 1 && mask->dst.addr_bytes[0] == 1) {
			if (!(spec->dst.addr_bytes[0] & 1)) {
				msg = "mlx4 does not support the explicit"
					" exclusion of all multicast traffic";
				goto error;
			}
			flow->allmulti = 1;
		} else if (sum_dst != (UINT8_C(0xff) * ETHER_ADDR_LEN)) {
			msg = "mlx4 does not support matching partial"
				" Ethernet fields";
			goto error;
		}
	}
	if (!flow->ibv_attr)
		return 0;
	if (flow->promisc) {
		flow->ibv_attr->type = IBV_FLOW_ATTR_ALL_DEFAULT;
		return 0;
	}
	if (flow->allmulti) {
		flow->ibv_attr->type = IBV_FLOW_ATTR_MC_DEFAULT;
		return 0;
	}
	++flow->ibv_attr->num_of_specs;
	eth = (void *)((uintptr_t)flow->ibv_attr + flow->ibv_attr_size);
	*eth = (struct ibv_flow_spec_eth) {
		.type = IBV_FLOW_SPEC_ETH,
		.size = sizeof(*eth),
	};
	memcpy(eth->val.dst_mac, spec->dst.addr_bytes, ETHER_ADDR_LEN);
	memcpy(eth->mask.dst_mac, mask->dst.addr_bytes, ETHER_ADDR_LEN);
	/* Remove unwanted bits from values. */
	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		eth->val.dst_mac[i] &= eth->mask.dst_mac[i];
	}
	return 0;
error:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Merge VLAN pattern item into flow rule handle.
 *
 * Additional mlx4-specific constraints on supported fields:
 *
 * - Matching *all* VLAN traffic by omitting @p item->spec or providing an
 *   empty @p item->mask would also include non-VLAN traffic. Doing so is
 *   therefore unsupported.
 * - No support for partial masks.
 *
 * @param[in, out] flow
 *   Flow rule handle to update.
 * @param[in] item
 *   Pattern item to merge.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_merge_vlan(struct rte_flow *flow,
		     const struct rte_flow_item *item,
		     const struct mlx4_flow_proc_item *proc,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask =
		spec ? (item->mask ? item->mask : proc->mask_default) : NULL;
	struct ibv_flow_spec_eth *eth;
	const char *msg;

	if (!mask || !mask->tci) {
		msg = "mlx4 cannot match all VLAN traffic while excluding"
			" non-VLAN traffic, TCI VID must be specified";
		goto error;
	}
	if (mask->tci != RTE_BE16(0x0fff)) {
		msg = "mlx4 does not support partial TCI VID matching";
		goto error;
	}
	if (!flow->ibv_attr)
		return 0;
	eth = (void *)((uintptr_t)flow->ibv_attr + flow->ibv_attr_size -
		       sizeof(*eth));
	eth->val.vlan_tag = spec->tci;
	eth->mask.vlan_tag = mask->tci;
	eth->val.vlan_tag &= eth->mask.vlan_tag;
	return 0;
error:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Merge IPv4 pattern item into flow rule handle.
 *
 * Additional mlx4-specific constraints on supported fields:
 *
 * - No support for partial masks.
 *
 * @param[in, out] flow
 *   Flow rule handle to update.
 * @param[in] item
 *   Pattern item to merge.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_merge_ipv4(struct rte_flow *flow,
		     const struct rte_flow_item *item,
		     const struct mlx4_flow_proc_item *proc,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask =
		spec ? (item->mask ? item->mask : proc->mask_default) : NULL;
	struct ibv_flow_spec_ipv4 *ipv4;
	const char *msg;

	if (mask &&
	    ((uint32_t)(mask->hdr.src_addr + 1) > UINT32_C(1) ||
	     (uint32_t)(mask->hdr.dst_addr + 1) > UINT32_C(1))) {
		msg = "mlx4 does not support matching partial IPv4 fields";
		goto error;
	}
	if (!flow->ibv_attr)
		return 0;
	++flow->ibv_attr->num_of_specs;
	ipv4 = (void *)((uintptr_t)flow->ibv_attr + flow->ibv_attr_size);
	*ipv4 = (struct ibv_flow_spec_ipv4) {
		.type = IBV_FLOW_SPEC_IPV4,
		.size = sizeof(*ipv4),
	};
	if (!spec)
		return 0;
	ipv4->val = (struct ibv_flow_ipv4_filter) {
		.src_ip = spec->hdr.src_addr,
		.dst_ip = spec->hdr.dst_addr,
	};
	ipv4->mask = (struct ibv_flow_ipv4_filter) {
		.src_ip = mask->hdr.src_addr,
		.dst_ip = mask->hdr.dst_addr,
	};
	/* Remove unwanted bits from values. */
	ipv4->val.src_ip &= ipv4->mask.src_ip;
	ipv4->val.dst_ip &= ipv4->mask.dst_ip;
	return 0;
error:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Merge UDP pattern item into flow rule handle.
 *
 * Additional mlx4-specific constraints on supported fields:
 *
 * - No support for partial masks.
 *
 * @param[in, out] flow
 *   Flow rule handle to update.
 * @param[in] item
 *   Pattern item to merge.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_merge_udp(struct rte_flow *flow,
		    const struct rte_flow_item *item,
		    const struct mlx4_flow_proc_item *proc,
		    struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask =
		spec ? (item->mask ? item->mask : proc->mask_default) : NULL;
	struct ibv_flow_spec_tcp_udp *udp;
	const char *msg;

	if (mask &&
	    ((uint16_t)(mask->hdr.src_port + 1) > UINT16_C(1) ||
	     (uint16_t)(mask->hdr.dst_port + 1) > UINT16_C(1))) {
		msg = "mlx4 does not support matching partial UDP fields";
		goto error;
	}
	if (!flow->ibv_attr)
		return 0;
	++flow->ibv_attr->num_of_specs;
	udp = (void *)((uintptr_t)flow->ibv_attr + flow->ibv_attr_size);
	*udp = (struct ibv_flow_spec_tcp_udp) {
		.type = IBV_FLOW_SPEC_UDP,
		.size = sizeof(*udp),
	};
	if (!spec)
		return 0;
	udp->val.dst_port = spec->hdr.dst_port;
	udp->val.src_port = spec->hdr.src_port;
	udp->mask.dst_port = mask->hdr.dst_port;
	udp->mask.src_port = mask->hdr.src_port;
	/* Remove unwanted bits from values. */
	udp->val.src_port &= udp->mask.src_port;
	udp->val.dst_port &= udp->mask.dst_port;
	return 0;
error:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Merge TCP pattern item into flow rule handle.
 *
 * Additional mlx4-specific constraints on supported fields:
 *
 * - No support for partial masks.
 *
 * @param[in, out] flow
 *   Flow rule handle to update.
 * @param[in] item
 *   Pattern item to merge.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_merge_tcp(struct rte_flow *flow,
		    const struct rte_flow_item *item,
		    const struct mlx4_flow_proc_item *proc,
		    struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask =
		spec ? (item->mask ? item->mask : proc->mask_default) : NULL;
	struct ibv_flow_spec_tcp_udp *tcp;
	const char *msg;

	if (mask &&
	    ((uint16_t)(mask->hdr.src_port + 1) > UINT16_C(1) ||
	     (uint16_t)(mask->hdr.dst_port + 1) > UINT16_C(1))) {
		msg = "mlx4 does not support matching partial TCP fields";
		goto error;
	}
	if (!flow->ibv_attr)
		return 0;
	++flow->ibv_attr->num_of_specs;
	tcp = (void *)((uintptr_t)flow->ibv_attr + flow->ibv_attr_size);
	*tcp = (struct ibv_flow_spec_tcp_udp) {
		.type = IBV_FLOW_SPEC_TCP,
		.size = sizeof(*tcp),
	};
	if (!spec)
		return 0;
	tcp->val.dst_port = spec->hdr.dst_port;
	tcp->val.src_port = spec->hdr.src_port;
	tcp->mask.dst_port = mask->hdr.dst_port;
	tcp->mask.src_port = mask->hdr.src_port;
	/* Remove unwanted bits from values. */
	tcp->val.src_port &= tcp->mask.src_port;
	tcp->val.dst_port &= tcp->mask.dst_port;
	return 0;
error:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg);
}

/**
 * Perform basic sanity checks on a pattern item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] proc
 *   Associated item-processing object.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_item_check(const struct rte_flow_item *item,
		     const struct mlx4_flow_proc_item *proc,
		     struct rte_flow_error *error)
{
	const uint8_t *mask;
	unsigned int i;

	/* item->last and item->mask cannot exist without item->spec. */
	if (!item->spec && (item->mask || item->last))
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "\"mask\" or \"last\" field provided without a"
			 " corresponding \"spec\"");
	/* No spec, no mask, no problem. */
	if (!item->spec)
		return 0;
	mask = item->mask ?
		(const uint8_t *)item->mask :
		(const uint8_t *)proc->mask_default;
	assert(mask);
	/*
	 * Single-pass check to make sure that:
	 * - Mask is supported, no bits are set outside proc->mask_support.
	 * - Both item->spec and item->last are included in mask.
	 */
	for (i = 0; i != proc->mask_sz; ++i) {
		if (!mask[i])
			continue;
		if ((mask[i] | ((const uint8_t *)proc->mask_support)[i]) !=
		    ((const uint8_t *)proc->mask_support)[i])
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				 item, "unsupported field found in \"mask\"");
		if (item->last &&
		    (((const uint8_t *)item->spec)[i] & mask[i]) !=
		    (((const uint8_t *)item->last)[i] & mask[i]))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				 item,
				 "range between \"spec\" and \"last\""
				 " is larger than \"mask\"");
	}
	return 0;
}

/** Graph of supported items and associated actions. */
static const struct mlx4_flow_proc_item mlx4_flow_proc_item_list[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_VLAN,
				       RTE_FLOW_ITEM_TYPE_IPV4),
		.mask_support = &(const struct rte_flow_item_eth){
			/* Only destination MAC can be matched. */
			.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		},
		.mask_default = &rte_flow_item_eth_mask,
		.mask_sz = sizeof(struct rte_flow_item_eth),
		.merge = mlx4_flow_merge_eth,
		.dst_sz = sizeof(struct ibv_flow_spec_eth),
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_IPV4),
		.mask_support = &(const struct rte_flow_item_vlan){
			/* Only TCI VID matching is supported. */
			.tci = RTE_BE16(0x0fff),
		},
		.mask_default = &rte_flow_item_vlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.merge = mlx4_flow_merge_vlan,
		.dst_sz = 0,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_UDP,
				       RTE_FLOW_ITEM_TYPE_TCP),
		.mask_support = &(const struct rte_flow_item_ipv4){
			.hdr = {
				.src_addr = RTE_BE32(0xffffffff),
				.dst_addr = RTE_BE32(0xffffffff),
			},
		},
		.mask_default = &rte_flow_item_ipv4_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.merge = mlx4_flow_merge_ipv4,
		.dst_sz = sizeof(struct ibv_flow_spec_ipv4),
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.mask_support = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = RTE_BE16(0xffff),
				.dst_port = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_udp_mask,
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.merge = mlx4_flow_merge_udp,
		.dst_sz = sizeof(struct ibv_flow_spec_tcp_udp),
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.mask_support = &(const struct rte_flow_item_tcp){
			.hdr = {
				.src_port = RTE_BE16(0xffff),
				.dst_port = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_tcp_mask,
		.mask_sz = sizeof(struct rte_flow_item_tcp),
		.merge = mlx4_flow_merge_tcp,
		.dst_sz = sizeof(struct ibv_flow_spec_tcp_udp),
	},
};

/**
 * Make sure a flow rule is supported and initialize associated structure.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @param[in, out] addr
 *   Buffer where the resulting flow rule handle pointer must be stored.
 *   If NULL, stop processing after validation stage.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_prepare(struct priv *priv,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error,
		  struct rte_flow **addr)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *action;
	const struct mlx4_flow_proc_item *proc;
	struct rte_flow temp = { .ibv_attr_size = sizeof(*temp.ibv_attr) };
	struct rte_flow *flow = &temp;
	const char *msg = NULL;

	if (attr->group)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
			 NULL, "groups are not supported");
	if (attr->priority > MLX4_FLOW_PRIORITY_LAST)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			 NULL, "maximum priority level is "
			 MLX4_STR_EXPAND(MLX4_FLOW_PRIORITY_LAST));
	if (attr->egress)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
			 NULL, "egress is not supported");
	if (!attr->ingress)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
			 NULL, "only ingress is supported");
fill:
	proc = mlx4_flow_proc_item_list;
	/* Go over pattern. */
	for (item = pattern; item->type; ++item) {
		const struct mlx4_flow_proc_item *next = NULL;
		unsigned int i;
		int err;

		if (item->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		if (item->type == MLX4_FLOW_ITEM_TYPE_INTERNAL) {
			flow->internal = 1;
			continue;
		}
		if (flow->promisc || flow->allmulti) {
			msg = "mlx4 does not support additional matching"
				" criteria combined with indiscriminate"
				" matching on Ethernet headers";
			goto exit_item_not_supported;
		}
		for (i = 0; proc->next_item && proc->next_item[i]; ++i) {
			if (proc->next_item[i] == item->type) {
				next = &mlx4_flow_proc_item_list[item->type];
				break;
			}
		}
		if (!next)
			goto exit_item_not_supported;
		proc = next;
		/*
		 * Perform basic sanity checks only once, while handle is
		 * not allocated.
		 */
		if (flow == &temp) {
			err = mlx4_flow_item_check(item, proc, error);
			if (err)
				return err;
		}
		if (proc->merge) {
			err = proc->merge(flow, item, proc, error);
			if (err)
				return err;
		}
		flow->ibv_attr_size += proc->dst_sz;
	}
	/* Go over actions list. */
	for (action = actions; action->type; ++action) {
		switch (action->type) {
			const struct rte_flow_action_queue *queue;
			const struct rte_flow_action_rss *rss;
			const struct rte_eth_rss_conf *rss_conf;
			unsigned int i;

		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		case RTE_FLOW_ACTION_TYPE_DROP:
			flow->drop = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (flow->rss)
				break;
			queue = action->conf;
			if (queue->index >= priv->dev->data->nb_rx_queues) {
				msg = "queue target index beyond number of"
					" configured Rx queues";
				goto exit_action_not_supported;
			}
			flow->rss = mlx4_rss_get
				(priv, 0, mlx4_rss_hash_key_default, 1,
				 &queue->index);
			if (!flow->rss) {
				msg = "not enough resources for additional"
					" single-queue RSS context";
				goto exit_action_not_supported;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (flow->rss)
				break;
			rss = action->conf;
			/* Default RSS configuration if none is provided. */
			rss_conf =
				rss->rss_conf ?
				rss->rss_conf :
				&(struct rte_eth_rss_conf){
					.rss_key = mlx4_rss_hash_key_default,
					.rss_key_len = MLX4_RSS_HASH_KEY_SIZE,
					.rss_hf = (ETH_RSS_IPV4 |
						   ETH_RSS_NONFRAG_IPV4_TCP |
						   ETH_RSS_IPV6 |
						   ETH_RSS_NONFRAG_IPV6_TCP),
				};
			/* Sanity checks. */
			for (i = 0; i < rss->num; ++i)
				if (rss->queue[i] >=
				    priv->dev->data->nb_rx_queues)
					break;
			if (i != rss->num) {
				msg = "queue index target beyond number of"
					" configured Rx queues";
				goto exit_action_not_supported;
			}
			if (!rte_is_power_of_2(rss->num)) {
				msg = "for RSS, mlx4 requires the number of"
					" queues to be a power of two";
				goto exit_action_not_supported;
			}
			if (rss_conf->rss_key_len !=
			    sizeof(flow->rss->key)) {
				msg = "mlx4 supports exactly one RSS hash key"
					" length: "
					MLX4_STR_EXPAND(MLX4_RSS_HASH_KEY_SIZE);
				goto exit_action_not_supported;
			}
			for (i = 1; i < rss->num; ++i)
				if (rss->queue[i] - rss->queue[i - 1] != 1)
					break;
			if (i != rss->num) {
				msg = "mlx4 requires RSS contexts to use"
					" consecutive queue indices only";
				goto exit_action_not_supported;
			}
			if (rss->queue[0] % rss->num) {
				msg = "mlx4 requires the first queue of a RSS"
					" context to be aligned on a multiple"
					" of the context size";
				goto exit_action_not_supported;
			}
			flow->rss = mlx4_rss_get
				(priv, mlx4_conv_rss_hf(rss_conf->rss_hf),
				 rss_conf->rss_key, rss->num, rss->queue);
			if (!flow->rss) {
				msg = "either invalid parameters or not enough"
					" resources for additional multi-queue"
					" RSS context";
				goto exit_action_not_supported;
			}
			break;
		default:
			goto exit_action_not_supported;
		}
	}
	if (!flow->rss && !flow->drop)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "no valid action");
	/* Validation ends here. */
	if (!addr) {
		if (flow->rss)
			mlx4_rss_put(flow->rss);
		return 0;
	}
	if (flow == &temp) {
		/* Allocate proper handle based on collected data. */
		const struct mlx4_malloc_vec vec[] = {
			{
				.align = alignof(struct rte_flow),
				.size = sizeof(*flow),
				.addr = (void **)&flow,
			},
			{
				.align = alignof(struct ibv_flow_attr),
				.size = temp.ibv_attr_size,
				.addr = (void **)&temp.ibv_attr,
			},
		};

		if (!mlx4_zmallocv(__func__, vec, RTE_DIM(vec)))
			return rte_flow_error_set
				(error, -rte_errno,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				 "flow rule handle allocation failure");
		/* Most fields will be updated by second pass. */
		*flow = (struct rte_flow){
			.ibv_attr = temp.ibv_attr,
			.ibv_attr_size = sizeof(*flow->ibv_attr),
			.rss = temp.rss,
		};
		*flow->ibv_attr = (struct ibv_flow_attr){
			.type = IBV_FLOW_ATTR_NORMAL,
			.size = sizeof(*flow->ibv_attr),
			.priority = attr->priority,
			.port = priv->port,
		};
		goto fill;
	}
	*addr = flow;
	return 0;
exit_item_not_supported:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  item, msg ? msg : "item not supported");
exit_action_not_supported:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				  action, msg ? msg : "action not supported");
}

/**
 * Validate a flow supported by the NIC.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
static int
mlx4_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	return mlx4_flow_prepare(priv, attr, pattern, actions, error, NULL);
}

/**
 * Get a drop flow rule resources instance.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   Pointer to drop flow resources on success, NULL otherwise and rte_errno
 *   is set.
 */
static struct mlx4_drop *
mlx4_drop_get(struct priv *priv)
{
	struct mlx4_drop *drop = priv->drop;

	if (drop) {
		assert(drop->refcnt);
		assert(drop->priv == priv);
		++drop->refcnt;
		return drop;
	}
	drop = rte_malloc(__func__, sizeof(*drop), 0);
	if (!drop)
		goto error;
	*drop = (struct mlx4_drop){
		.priv = priv,
		.refcnt = 1,
	};
	drop->cq = ibv_create_cq(priv->ctx, 1, NULL, NULL, 0);
	if (!drop->cq)
		goto error;
	drop->qp = ibv_create_qp(priv->pd,
				 &(struct ibv_qp_init_attr){
					.send_cq = drop->cq,
					.recv_cq = drop->cq,
					.qp_type = IBV_QPT_RAW_PACKET,
				 });
	if (!drop->qp)
		goto error;
	priv->drop = drop;
	return drop;
error:
	if (drop->qp)
		claim_zero(ibv_destroy_qp(drop->qp));
	if (drop->cq)
		claim_zero(ibv_destroy_cq(drop->cq));
	if (drop)
		rte_free(drop);
	rte_errno = ENOMEM;
	return NULL;
}

/**
 * Give back a drop flow rule resources instance.
 *
 * @param drop
 *   Pointer to drop flow rule resources.
 */
static void
mlx4_drop_put(struct mlx4_drop *drop)
{
	assert(drop->refcnt);
	if (--drop->refcnt)
		return;
	drop->priv->drop = NULL;
	claim_zero(ibv_destroy_qp(drop->qp));
	claim_zero(ibv_destroy_cq(drop->cq));
	rte_free(drop);
}

/**
 * Toggle a configured flow rule.
 *
 * @param priv
 *   Pointer to private structure.
 * @param flow
 *   Flow rule handle to toggle.
 * @param enable
 *   Whether associated Verbs flow must be created or removed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_toggle(struct priv *priv,
		 struct rte_flow *flow,
		 int enable,
		 struct rte_flow_error *error)
{
	struct ibv_qp *qp = NULL;
	const char *msg;
	int err;

	if (!enable) {
		if (!flow->ibv_flow)
			return 0;
		claim_zero(ibv_destroy_flow(flow->ibv_flow));
		flow->ibv_flow = NULL;
		if (flow->drop)
			mlx4_drop_put(priv->drop);
		else if (flow->rss)
			mlx4_rss_detach(flow->rss);
		return 0;
	}
	assert(flow->ibv_attr);
	if (!flow->internal &&
	    !priv->isolated &&
	    flow->ibv_attr->priority == MLX4_FLOW_PRIORITY_LAST) {
		if (flow->ibv_flow) {
			claim_zero(ibv_destroy_flow(flow->ibv_flow));
			flow->ibv_flow = NULL;
			if (flow->drop)
				mlx4_drop_put(priv->drop);
			else if (flow->rss)
				mlx4_rss_detach(flow->rss);
		}
		err = EACCES;
		msg = ("priority level "
		       MLX4_STR_EXPAND(MLX4_FLOW_PRIORITY_LAST)
		       " is reserved when not in isolated mode");
		goto error;
	}
	if (flow->rss) {
		struct mlx4_rss *rss = flow->rss;
		int missing = 0;
		unsigned int i;

		/* Stop at the first nonexistent target queue. */
		for (i = 0; i != rss->queues; ++i)
			if (rss->queue_id[i] >=
			    priv->dev->data->nb_rx_queues ||
			    !priv->dev->data->rx_queues[rss->queue_id[i]]) {
				missing = 1;
				break;
			}
		if (flow->ibv_flow) {
			if (missing ^ !flow->drop)
				return 0;
			/* Verbs flow needs updating. */
			claim_zero(ibv_destroy_flow(flow->ibv_flow));
			flow->ibv_flow = NULL;
			if (flow->drop)
				mlx4_drop_put(priv->drop);
			else
				mlx4_rss_detach(rss);
		}
		if (!missing) {
			err = mlx4_rss_attach(rss);
			if (err) {
				err = -err;
				msg = "cannot create indirection table or hash"
					" QP to associate flow rule with";
				goto error;
			}
			qp = rss->qp;
		}
		/* A missing target queue drops traffic implicitly. */
		flow->drop = missing;
	}
	if (flow->drop) {
		mlx4_drop_get(priv);
		if (!priv->drop) {
			err = rte_errno;
			msg = "resources for drop flow rule cannot be created";
			goto error;
		}
		qp = priv->drop->qp;
	}
	assert(qp);
	if (flow->ibv_flow)
		return 0;
	flow->ibv_flow = ibv_create_flow(qp, flow->ibv_attr);
	if (flow->ibv_flow)
		return 0;
	if (flow->drop)
		mlx4_drop_put(priv->drop);
	else if (flow->rss)
		mlx4_rss_detach(flow->rss);
	err = errno;
	msg = "flow rule rejected by device";
error:
	return rte_flow_error_set
		(error, err, RTE_FLOW_ERROR_TYPE_HANDLE, flow, msg);
}

/**
 * Create a flow.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
static struct rte_flow *
mlx4_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	int err;

	err = mlx4_flow_prepare(priv, attr, pattern, actions, error, &flow);
	if (err)
		return NULL;
	err = mlx4_flow_toggle(priv, flow, priv->started, error);
	if (!err) {
		struct rte_flow *curr = LIST_FIRST(&priv->flows);

		/* New rules are inserted after internal ones. */
		if (!curr || !curr->internal) {
			LIST_INSERT_HEAD(&priv->flows, flow, next);
		} else {
			while (LIST_NEXT(curr, next) &&
			       LIST_NEXT(curr, next)->internal)
				curr = LIST_NEXT(curr, next);
			LIST_INSERT_AFTER(curr, flow, next);
		}
		return flow;
	}
	if (flow->rss)
		mlx4_rss_put(flow->rss);
	rte_flow_error_set(error, -err, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   error->message);
	rte_free(flow);
	return NULL;
}

/**
 * Configure isolated mode.
 *
 * @see rte_flow_isolate()
 * @see rte_flow_ops
 */
static int
mlx4_flow_isolate(struct rte_eth_dev *dev,
		  int enable,
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;

	if (!!enable == !!priv->isolated)
		return 0;
	priv->isolated = !!enable;
	if (mlx4_flow_sync(priv, error)) {
		priv->isolated = !enable;
		return -rte_errno;
	}
	return 0;
}

/**
 * Destroy a flow rule.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
static int
mlx4_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	int err = mlx4_flow_toggle(priv, flow, 0, error);

	if (err)
		return err;
	LIST_REMOVE(flow, next);
	if (flow->rss)
		mlx4_rss_put(flow->rss);
	rte_free(flow);
	return 0;
}

/**
 * Destroy user-configured flow rules.
 *
 * This function skips internal flows rules.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
static int
mlx4_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow = LIST_FIRST(&priv->flows);

	while (flow) {
		struct rte_flow *next = LIST_NEXT(flow, next);

		if (!flow->internal)
			mlx4_flow_destroy(dev, flow, error);
		flow = next;
	}
	return 0;
}

/**
 * Helper function to determine the next configured VLAN filter.
 *
 * @param priv
 *   Pointer to private structure.
 * @param vlan
 *   VLAN ID to use as a starting point.
 *
 * @return
 *   Next configured VLAN ID or a high value (>= 4096) if there is none.
 */
static uint16_t
mlx4_flow_internal_next_vlan(struct priv *priv, uint16_t vlan)
{
	while (vlan < 4096) {
		if (priv->dev->data->vlan_filter_conf.ids[vlan / 64] &
		    (UINT64_C(1) << (vlan % 64)))
			return vlan;
		++vlan;
	}
	return vlan;
}

/**
 * Generate internal flow rules.
 *
 * Various flow rules are created depending on the mode the device is in:
 *
 * 1. Promiscuous: port MAC + catch-all (VLAN filtering is ignored).
 * 2. All multicast: port MAC/VLAN + catch-all multicast.
 * 3. Otherwise: port MAC/VLAN + broadcast MAC/VLAN.
 *
 * About MAC flow rules:
 *
 * - MAC flow rules are generated from @p dev->data->mac_addrs
 *   (@p priv->mac array).
 * - An additional flow rule for Ethernet broadcasts is also generated.
 * - All these are per-VLAN if @p dev->data->dev_conf.rxmode.hw_vlan_filter
 *   is enabled and VLAN filters are configured.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_flow_internal(struct priv *priv, struct rte_flow_error *error)
{
	struct rte_flow_attr attr = {
		.priority = MLX4_FLOW_PRIORITY_LAST,
		.ingress = 1,
	};
	struct rte_flow_item_eth eth_spec;
	const struct rte_flow_item_eth eth_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	const struct rte_flow_item_eth eth_allmulti = {
		.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
	};
	struct rte_flow_item_vlan vlan_spec;
	const struct rte_flow_item_vlan vlan_mask = {
		.tci = RTE_BE16(0x0fff),
	};
	struct rte_flow_item pattern[] = {
		{
			.type = MLX4_FLOW_ITEM_TYPE_INTERNAL,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &eth_spec,
			.mask = &eth_mask,
		},
		{
			/* Replaced with VLAN if filtering is enabled. */
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	/*
	 * Round number of queues down to their previous power of 2 to
	 * comply with RSS context limitations. Extra queues silently do not
	 * get RSS by default.
	 */
	uint32_t queues =
		rte_align32pow2(priv->dev->data->nb_rx_queues + 1) >> 1;
	alignas(struct rte_flow_action_rss) uint8_t rss_conf_data
		[offsetof(struct rte_flow_action_rss, queue) +
		 sizeof(((struct rte_flow_action_rss *)0)->queue[0]) * queues];
	struct rte_flow_action_rss *rss_conf = (void *)rss_conf_data;
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = rss_conf,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct ether_addr *rule_mac = &eth_spec.dst;
	rte_be16_t *rule_vlan =
		priv->dev->data->dev_conf.rxmode.hw_vlan_filter &&
		!priv->dev->data->promiscuous ?
		&vlan_spec.tci :
		NULL;
	int broadcast =
		!priv->dev->data->promiscuous &&
		!priv->dev->data->all_multicast;
	uint16_t vlan = 0;
	struct rte_flow *flow;
	unsigned int i;
	int err = 0;

	/* Nothing to be done if there are no Rx queues. */
	if (!queues)
		goto error;
	/* Prepare default RSS configuration. */
	*rss_conf = (struct rte_flow_action_rss){
		.rss_conf = NULL, /* Rely on default fallback settings. */
		.num = queues,
	};
	for (i = 0; i != queues; ++i)
		rss_conf->queue[i] = i;
	/*
	 * Set up VLAN item if filtering is enabled and at least one VLAN
	 * filter is configured.
	 */
	if (rule_vlan) {
		vlan = mlx4_flow_internal_next_vlan(priv, 0);
		if (vlan < 4096) {
			pattern[2] = (struct rte_flow_item){
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &vlan_spec,
				.mask = &vlan_mask,
			};
next_vlan:
			*rule_vlan = rte_cpu_to_be_16(vlan);
		} else {
			rule_vlan = NULL;
		}
	}
	for (i = 0; i != RTE_DIM(priv->mac) + broadcast; ++i) {
		const struct ether_addr *mac;

		/* Broadcasts are handled by an extra iteration. */
		if (i < RTE_DIM(priv->mac))
			mac = &priv->mac[i];
		else
			mac = &eth_mask.dst;
		if (is_zero_ether_addr(mac))
			continue;
		/* Check if MAC flow rule is already present. */
		for (flow = LIST_FIRST(&priv->flows);
		     flow && flow->internal;
		     flow = LIST_NEXT(flow, next)) {
			const struct ibv_flow_spec_eth *eth =
				(const void *)((uintptr_t)flow->ibv_attr +
					       sizeof(*flow->ibv_attr));
			unsigned int j;

			if (!flow->mac)
				continue;
			assert(flow->ibv_attr->type == IBV_FLOW_ATTR_NORMAL);
			assert(flow->ibv_attr->num_of_specs == 1);
			assert(eth->type == IBV_FLOW_SPEC_ETH);
			assert(flow->rss);
			if (rule_vlan &&
			    (eth->val.vlan_tag != *rule_vlan ||
			     eth->mask.vlan_tag != RTE_BE16(0x0fff)))
				continue;
			if (!rule_vlan && eth->mask.vlan_tag)
				continue;
			for (j = 0; j != sizeof(mac->addr_bytes); ++j)
				if (eth->val.dst_mac[j] != mac->addr_bytes[j] ||
				    eth->mask.dst_mac[j] != UINT8_C(0xff) ||
				    eth->val.src_mac[j] != UINT8_C(0x00) ||
				    eth->mask.src_mac[j] != UINT8_C(0x00))
					break;
			if (j != sizeof(mac->addr_bytes))
				continue;
			if (flow->rss->queues != queues ||
			    memcmp(flow->rss->queue_id, rss_conf->queue,
				   queues * sizeof(flow->rss->queue_id[0])))
				continue;
			break;
		}
		if (!flow || !flow->internal) {
			/* Not found, create a new flow rule. */
			memcpy(rule_mac, mac, sizeof(*mac));
			flow = mlx4_flow_create(priv->dev, &attr, pattern,
						actions, error);
			if (!flow) {
				err = -rte_errno;
				goto error;
			}
		}
		flow->select = 1;
		flow->mac = 1;
	}
	if (rule_vlan) {
		vlan = mlx4_flow_internal_next_vlan(priv, vlan + 1);
		if (vlan < 4096)
			goto next_vlan;
	}
	/* Take care of promiscuous and all multicast flow rules. */
	if (!broadcast) {
		for (flow = LIST_FIRST(&priv->flows);
		     flow && flow->internal;
		     flow = LIST_NEXT(flow, next)) {
			if (priv->dev->data->promiscuous) {
				if (flow->promisc)
					break;
			} else {
				assert(priv->dev->data->all_multicast);
				if (flow->allmulti)
					break;
			}
		}
		if (flow && flow->internal) {
			assert(flow->rss);
			if (flow->rss->queues != queues ||
			    memcmp(flow->rss->queue_id, rss_conf->queue,
				   queues * sizeof(flow->rss->queue_id[0])))
				flow = NULL;
		}
		if (!flow || !flow->internal) {
			/* Not found, create a new flow rule. */
			if (priv->dev->data->promiscuous) {
				pattern[1].spec = NULL;
				pattern[1].mask = NULL;
			} else {
				assert(priv->dev->data->all_multicast);
				pattern[1].spec = &eth_allmulti;
				pattern[1].mask = &eth_allmulti;
			}
			pattern[2] = pattern[3];
			flow = mlx4_flow_create(priv->dev, &attr, pattern,
						actions, error);
			if (!flow) {
				err = -rte_errno;
				goto error;
			}
		}
		assert(flow->promisc || flow->allmulti);
		flow->select = 1;
	}
error:
	/* Clear selection and clean up stale internal flow rules. */
	flow = LIST_FIRST(&priv->flows);
	while (flow && flow->internal) {
		struct rte_flow *next = LIST_NEXT(flow, next);

		if (!flow->select)
			claim_zero(mlx4_flow_destroy(priv->dev, flow, error));
		else
			flow->select = 0;
		flow = next;
	}
	return err;
}

/**
 * Synchronize flow rules.
 *
 * This function synchronizes flow rules with the state of the device by
 * taking into account isolated mode and whether target queues are
 * configured.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx4_flow_sync(struct priv *priv, struct rte_flow_error *error)
{
	struct rte_flow *flow;
	int ret;

	/* Internal flow rules are guaranteed to come first in the list. */
	if (priv->isolated) {
		/*
		 * Get rid of them in isolated mode, stop at the first
		 * non-internal rule found.
		 */
		for (flow = LIST_FIRST(&priv->flows);
		     flow && flow->internal;
		     flow = LIST_FIRST(&priv->flows))
			claim_zero(mlx4_flow_destroy(priv->dev, flow, error));
	} else {
		/* Refresh internal rules. */
		ret = mlx4_flow_internal(priv, error);
		if (ret)
			return ret;
	}
	/* Toggle the remaining flow rules . */
	LIST_FOREACH(flow, &priv->flows, next) {
		ret = mlx4_flow_toggle(priv, flow, priv->started, error);
		if (ret)
			return ret;
	}
	if (!priv->started)
		assert(!priv->drop);
	return 0;
}

/**
 * Clean up all flow rules.
 *
 * Unlike mlx4_flow_flush(), this function takes care of all remaining flow
 * rules regardless of whether they are internal or user-configured.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
mlx4_flow_clean(struct priv *priv)
{
	struct rte_flow *flow;

	while ((flow = LIST_FIRST(&priv->flows)))
		mlx4_flow_destroy(priv->dev, flow, NULL);
	assert(LIST_EMPTY(&priv->rss));
}

static const struct rte_flow_ops mlx4_flow_ops = {
	.validate = mlx4_flow_validate,
	.create = mlx4_flow_create,
	.destroy = mlx4_flow_destroy,
	.flush = mlx4_flow_flush,
	.isolate = mlx4_flow_isolate,
};

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_filter_ctrl(struct rte_eth_dev *dev,
		 enum rte_filter_type filter_type,
		 enum rte_filter_op filter_op,
		 void *arg)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			break;
		*(const void **)arg = &mlx4_flow_ops;
		return 0;
	default:
		ERROR("%p: filter type (%d) not supported",
		      (void *)dev, filter_type);
		break;
	}
	rte_errno = ENOTSUP;
	return -rte_errno;
}
