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

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define TREX_PATCH

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs_exp.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"

struct fdir_flow_desc {
	uint16_t dst_port;
	uint16_t src_port;
	uint32_t src_ip[4];
	uint32_t dst_ip[4];
    uint8_t  tos;
    uint8_t  ip_id;
    uint8_t  proto;

	uint8_t	mac[6];
	uint16_t vlan_tag;
	enum hash_rxq_type type;
};

struct mlx5_fdir_filter {
	LIST_ENTRY(mlx5_fdir_filter) next;
	uint16_t queue; /* Queue assigned to if FDIR match. */
	struct fdir_flow_desc desc;
	struct ibv_exp_flow *flow;
};

LIST_HEAD(fdir_filter_list, mlx5_fdir_filter);

/**
 * Convert struct rte_eth_fdir_filter to mlx5 filter descriptor.
 *
 * @param[in] fdir_filter
 *   DPDK filter structure to convert.
 * @param[out] desc
 *   Resulting mlx5 filter descriptor.
 * @param mode
 *   Flow director mode.
 */
static void
fdir_filter_to_flow_desc(const struct rte_eth_fdir_filter *fdir_filter,
			 struct fdir_flow_desc *desc, enum rte_fdir_mode mode)
{
	/* Initialize descriptor. */
	memset(desc, 0, sizeof(*desc));

	/* Set VLAN ID. */
	desc->vlan_tag = fdir_filter->input.flow_ext.vlan_tci;

	/* Set MAC address. */
#ifndef TREX_PATCH
	if (mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		rte_memcpy(desc->mac,
			   fdir_filter->input.flow.mac_vlan_flow.mac_addr.
				addr_bytes,
			   sizeof(desc->mac));
		desc->type = HASH_RXQ_ETH;
		return;
	}
#else
    if (fdir_filter->input.flow.ip4_flow.ip_id == 2) {
		desc->type = HASH_RXQ_ETH;
        desc->ip_id = fdir_filter->input.flow.ip4_flow.ip_id;
		return;
    }
#endif

	/* Set mode */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		desc->type = HASH_RXQ_UDPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		desc->type = HASH_RXQ_TCPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		desc->type = HASH_RXQ_IPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		desc->type = HASH_RXQ_UDPV6;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		desc->type = HASH_RXQ_TCPV6;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		desc->type = HASH_RXQ_IPV6;
		break;
	default:
		break;
	}

	/* Set flow values */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		desc->src_port = fdir_filter->input.flow.udp4_flow.src_port;
		desc->dst_port = fdir_filter->input.flow.udp4_flow.dst_port;

	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		desc->src_ip[0] = fdir_filter->input.flow.ip4_flow.src_ip;
		desc->dst_ip[0] = fdir_filter->input.flow.ip4_flow.dst_ip;
        desc->tos       = fdir_filter->input.flow.ip4_flow.ttl; /* TTL is mapped to TOS TREX_PATCH */
        desc->ip_id     = fdir_filter->input.flow.ip4_flow.ip_id;
        desc->proto     = fdir_filter->input.flow.ip4_flow.proto;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		desc->src_port = fdir_filter->input.flow.udp6_flow.src_port;
		desc->dst_port = fdir_filter->input.flow.udp6_flow.dst_port;
		/* Fall through. */
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		rte_memcpy(desc->src_ip,
			   fdir_filter->input.flow.ipv6_flow.src_ip,
			   sizeof(desc->src_ip));
		rte_memcpy(desc->dst_ip,
			   fdir_filter->input.flow.ipv6_flow.dst_ip,
			   sizeof(desc->dst_ip));
        desc->tos       = (uint8_t)fdir_filter->input.flow.ipv6_flow.hop_limits;  /* TTL is mapped to TOS - TREX_PATCH */
        desc->ip_id     = (uint8_t)fdir_filter->input.flow.ipv6_flow.flow_label;
        desc->proto     = fdir_filter->input.flow.ipv6_flow.proto;

		break;
	default:
		break;
	}
}


/**
 * Check if two flow descriptors overlap according to configured mask.
 *
 * @param priv
 *   Private structure that provides flow director mask.
 * @param desc1
 *   First flow descriptor to compare.
 * @param desc2
 *   Second flow descriptor to compare.
 *
 * @return
 *   Nonzero if descriptors overlap.
 */
static int
priv_fdir_overlap(const struct priv *priv,
		  const struct fdir_flow_desc *desc1,
		  const struct fdir_flow_desc *desc2)
{
	const struct rte_eth_fdir_masks *mask =
		&priv->dev->data->dev_conf.fdir_conf.mask;
	unsigned int i;

	if (desc1->type != desc2->type)
		return 0;
	/* Ignore non masked bits. */
	for (i = 0; i != RTE_DIM(desc1->mac); ++i)
		if ((desc1->mac[i] & mask->mac_addr_byte_mask) !=
		    (desc2->mac[i] & mask->mac_addr_byte_mask))
			return 0;
	if (((desc1->src_port & mask->src_port_mask) !=
	     (desc2->src_port & mask->src_port_mask)) ||
	    ((desc1->dst_port & mask->dst_port_mask) !=
	     (desc2->dst_port & mask->dst_port_mask)))
		return 0;

    if  ( (desc1->tos    != desc2->tos)  ||
          (desc1->ip_id  != desc2->ip_id) ||
          (desc1->proto  != desc2->proto) ) 
        return 0;

	switch (desc1->type) {
	case HASH_RXQ_IPV4:
	case HASH_RXQ_UDPV4:
	case HASH_RXQ_TCPV4:
		if (((desc1->src_ip[0] & mask->ipv4_mask.src_ip) !=
		     (desc2->src_ip[0] & mask->ipv4_mask.src_ip)) ||
		    ((desc1->dst_ip[0] & mask->ipv4_mask.dst_ip) !=
		     (desc2->dst_ip[0] & mask->ipv4_mask.dst_ip))) 
			return 0;

		break;
	case HASH_RXQ_IPV6:
	case HASH_RXQ_UDPV6:
	case HASH_RXQ_TCPV6:
		for (i = 0; i != RTE_DIM(desc1->src_ip); ++i)
			if (((desc1->src_ip[i] & mask->ipv6_mask.src_ip[i]) !=
			     (desc2->src_ip[i] & mask->ipv6_mask.src_ip[i])) ||
			    ((desc1->dst_ip[i] & mask->ipv6_mask.dst_ip[i]) !=
			     (desc2->dst_ip[i] & mask->ipv6_mask.dst_ip[i])))
				return 0;
		break;
	default:
		break;
	}
	return 1;
}

/**
 * Create flow director steering rule for a specific filter.
 *
 * @param priv
 *   Private structure.
 * @param mlx5_fdir_filter
 *   Filter to create a steering rule for.
 * @param fdir_queue
 *   Flow director queue for matching packets.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_flow_add(struct priv *priv,
		   struct mlx5_fdir_filter *mlx5_fdir_filter,
		   struct fdir_queue *fdir_queue)
{
	struct ibv_exp_flow *flow;
	struct fdir_flow_desc *desc = &mlx5_fdir_filter->desc;
	enum rte_fdir_mode fdir_mode =
		priv->dev->data->dev_conf.fdir_conf.mode;
	struct rte_eth_fdir_masks *mask =
		&priv->dev->data->dev_conf.fdir_conf.mask;
	FLOW_ATTR_SPEC_ETH(data, priv_flow_attr(priv, NULL, 0, desc->type));
	struct ibv_exp_flow_attr *attr = &data->attr;
	uintptr_t spec_offset = (uintptr_t)&data->spec;
	struct ibv_exp_flow_spec_eth *spec_eth;
	struct ibv_exp_flow_spec_ipv4_ext *spec_ipv4;
	struct ibv_exp_flow_spec_ipv6_ext *spec_ipv6;
	struct ibv_exp_flow_spec_tcp_udp *spec_tcp_udp;
	struct mlx5_fdir_filter *iter_fdir_filter;
	unsigned int i;

	/* Abort if an existing flow overlaps this one to avoid packet
	 * duplication, even if it targets another queue. */
	LIST_FOREACH(iter_fdir_filter, priv->fdir_filter_list, next)
		if ((iter_fdir_filter != mlx5_fdir_filter) &&
		    (iter_fdir_filter->flow != NULL) &&
		    (priv_fdir_overlap(priv,
				       &mlx5_fdir_filter->desc,
				       &iter_fdir_filter->desc))){
            ERROR("overlap rules, please check your rules");
            return EEXIST;
        }

	/*
	 * No padding must be inserted by the compiler between attr and spec.
	 * This layout is expected by libibverbs.
	 */
	assert(((uint8_t *)attr + sizeof(*attr)) == (uint8_t *)spec_offset);
	priv_flow_attr(priv, attr, sizeof(data), desc->type);

	/* Set Ethernet spec */
	spec_eth = (struct ibv_exp_flow_spec_eth *)spec_offset;

	/* The first specification must be Ethernet. */
	assert(spec_eth->type == IBV_EXP_FLOW_SPEC_ETH);
	assert(spec_eth->size == sizeof(*spec_eth));

	/* VLAN ID */
	spec_eth->val.vlan_tag = desc->vlan_tag & mask->vlan_tci_mask;
	spec_eth->mask.vlan_tag = mask->vlan_tci_mask;

	/* Update priority */
	attr->priority = 2;

#ifndef TREX_PATCH
	if (fdir_mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		/* MAC Address */
		for (i = 0; i != RTE_DIM(spec_eth->mask.dst_mac); ++i) {
			spec_eth->val.dst_mac[i] =
				desc->mac[i] & mask->mac_addr_byte_mask;
			spec_eth->mask.dst_mac[i] = mask->mac_addr_byte_mask;
		}
		goto create_flow;
	}
#else
    // empty mask means "match everything". This rule will match all packets, no matter what is the ether type
    if (desc->ip_id == 2) {
        spec_eth->val.ether_type = 0x0806;
        spec_eth->mask.ether_type = 0x0000;
        goto create_flow;
    }
#endif

	switch (desc->type) {
	case HASH_RXQ_IPV4:
	case HASH_RXQ_UDPV4:
	case HASH_RXQ_TCPV4:
		spec_offset += spec_eth->size;

		/* Set IP spec */
		spec_ipv4 = (struct ibv_exp_flow_spec_ipv4_ext *)spec_offset;

		/* The second specification must be IP. */
		assert(spec_ipv4->type == IBV_EXP_FLOW_SPEC_IPV4_EXT);
		assert(spec_ipv4->size == sizeof(*spec_ipv4));

		spec_ipv4->val.src_ip =
			desc->src_ip[0] & mask->ipv4_mask.src_ip;
		spec_ipv4->val.dst_ip =
			desc->dst_ip[0] & mask->ipv4_mask.dst_ip;
		spec_ipv4->mask.src_ip = mask->ipv4_mask.src_ip;
		spec_ipv4->mask.dst_ip = mask->ipv4_mask.dst_ip;

        /* PROTO */
        spec_ipv4->val.proto  = desc->proto & mask->ipv4_mask.proto;
        spec_ipv4->mask.proto = mask->ipv4_mask.proto;

#ifdef TREX_PATCH
        /* TOS */
        if (desc->ip_id == 1) {
            spec_ipv4->mask.tos = 0x1;
            spec_ipv4->val.tos = 0x1;
        } else {
            spec_ipv4->mask.tos = 0x0;
            spec_ipv4->val.tos = 0x0;
        }
        //        spec_ipv4->val.tos = desc->tos & spec_ipv4->mask.tos;// & mask->ipv4_mask.tos;
#endif
		/* Update priority */
		attr->priority = 1;

		if (desc->type == HASH_RXQ_IPV4)
			goto create_flow;

		spec_offset += spec_ipv4->size;
		break;
	case HASH_RXQ_IPV6:
	case HASH_RXQ_UDPV6:
	case HASH_RXQ_TCPV6:
		spec_offset += spec_eth->size;

		/* Set IP spec */
		spec_ipv6 = (struct ibv_exp_flow_spec_ipv6_ext *)spec_offset;

		/* The second specification must be IP. */
		assert(spec_ipv6->type == IBV_EXP_FLOW_SPEC_IPV6_EXT);
		assert(spec_ipv6->size == sizeof(*spec_ipv6));

		for (i = 0; i != RTE_DIM(desc->src_ip); ++i) {
			((uint32_t *)spec_ipv6->val.src_ip)[i] =
				desc->src_ip[i] & mask->ipv6_mask.src_ip[i];
			((uint32_t *)spec_ipv6->val.dst_ip)[i] =
				desc->dst_ip[i] & mask->ipv6_mask.dst_ip[i];
		}
		rte_memcpy(spec_ipv6->mask.src_ip,
			   mask->ipv6_mask.src_ip,
			   sizeof(spec_ipv6->mask.src_ip));
		rte_memcpy(spec_ipv6->mask.dst_ip,
			   mask->ipv6_mask.dst_ip,
			   sizeof(spec_ipv6->mask.dst_ip));

        spec_ipv6->val.next_hdr  = desc->proto & mask->ipv6_mask.proto;
        spec_ipv6->mask.next_hdr = mask->ipv6_mask.proto;

#ifdef TREX_PATCH
        /* TOS */
        if (desc->ip_id == 1) {
            spec_ipv6->mask.traffic_class = 0x1;
            spec_ipv6->val.traffic_class = 0x1;
        } else {
            spec_ipv6->mask.traffic_class = 0;
            spec_ipv6->val.traffic_class = 0;
        }
#endif

		/* Update priority */
		attr->priority = 1;

		if (desc->type == HASH_RXQ_IPV6)
			goto create_flow;

		spec_offset += spec_ipv6->size;
		break;
	default:
		ERROR("invalid flow attribute type");
		return EINVAL;
	}

	/* Set TCP/UDP flow specification. */
	spec_tcp_udp = (struct ibv_exp_flow_spec_tcp_udp *)spec_offset;

	/* The third specification must be TCP/UDP. */
	assert(spec_tcp_udp->type == IBV_EXP_FLOW_SPEC_TCP ||
	       spec_tcp_udp->type == IBV_EXP_FLOW_SPEC_UDP);
	assert(spec_tcp_udp->size == sizeof(*spec_tcp_udp));

	spec_tcp_udp->val.src_port = desc->src_port & mask->src_port_mask;
	spec_tcp_udp->val.dst_port = desc->dst_port & mask->dst_port_mask;
	spec_tcp_udp->mask.src_port = mask->src_port_mask;
	spec_tcp_udp->mask.dst_port = mask->dst_port_mask;

	/* Update priority */
	attr->priority = 0;

create_flow:

	errno = 0;
	flow = ibv_exp_create_flow(fdir_queue->qp, attr);
	if (flow == NULL) {
		/* It's not clear whether errno is always set in this case. */
		ERROR("%p: flow director configuration failed, errno=%d: %s",
		      (void *)priv, errno,
		      (errno ? strerror(errno) : "Unknown error"));
		if (errno)
			return errno;
		return EINVAL;
	}

	DEBUG("%p: added flow director rule (%p)", (void *)priv, (void *)flow);
	mlx5_fdir_filter->flow = flow;
	return 0;
}

/**
 * Get flow director queue for a specific RX queue, create it in case
 * it does not exist.
 *
 * @param priv
 *   Private structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   Related flow director queue on success, NULL otherwise.
 */
static struct fdir_queue *
priv_get_fdir_queue(struct priv *priv, uint16_t idx)
{
	struct rxq_ctrl *rxq_ctrl =
		container_of((*priv->rxqs)[idx], struct rxq_ctrl, rxq);
	struct fdir_queue *fdir_queue = &rxq_ctrl->fdir_queue;
	struct ibv_exp_rwq_ind_table *ind_table = NULL;
	struct ibv_qp *qp = NULL;
	struct ibv_exp_rwq_ind_table_init_attr ind_init_attr;
	struct ibv_exp_rx_hash_conf hash_conf;
	struct ibv_exp_qp_init_attr qp_init_attr;
	int err = 0;

	/* Return immediately if it has already been created. */
	if (fdir_queue->qp != NULL)
		return fdir_queue;

	ind_init_attr = (struct ibv_exp_rwq_ind_table_init_attr){
		.pd = priv->pd,
		.log_ind_tbl_size = 0,
		.ind_tbl = &rxq_ctrl->wq,
		.comp_mask = 0,
	};

	errno = 0;
	ind_table = ibv_exp_create_rwq_ind_table(priv->ctx,
						 &ind_init_attr);
	if (ind_table == NULL) {
		/* Not clear whether errno is set. */
		err = (errno ? errno : EINVAL);
		ERROR("RX indirection table creation failed with error %d: %s",
		      err, strerror(err));
		goto error;
	}

	/* Create fdir_queue qp. */
	hash_conf = (struct ibv_exp_rx_hash_conf){
		.rx_hash_function = IBV_EXP_RX_HASH_FUNC_TOEPLITZ,
		.rx_hash_key_len = rss_hash_default_key_len,
		.rx_hash_key = rss_hash_default_key,
		.rx_hash_fields_mask = 0,
		.rwq_ind_tbl = ind_table,
	};
	qp_init_attr = (struct ibv_exp_qp_init_attr){
		.max_inl_recv = 0, /* Currently not supported. */
		.qp_type = IBV_QPT_RAW_PACKET,
		.comp_mask = (IBV_EXP_QP_INIT_ATTR_PD |
			      IBV_EXP_QP_INIT_ATTR_RX_HASH),
		.pd = priv->pd,
		.rx_hash_conf = &hash_conf,
		.port_num = priv->port,
	};

	qp = ibv_exp_create_qp(priv->ctx, &qp_init_attr);
	if (qp == NULL) {
		err = (errno ? errno : EINVAL);
		ERROR("hash RX QP creation failure: %s", strerror(err));
		goto error;
	}

	fdir_queue->ind_table = ind_table;
	fdir_queue->qp = qp;

	return fdir_queue;

error:
	if (qp != NULL)
		claim_zero(ibv_destroy_qp(qp));

	if (ind_table != NULL)
		claim_zero(ibv_exp_destroy_rwq_ind_table(ind_table));

	return NULL;
}

/**
 * Enable flow director filter and create steering rules.
 *
 * @param priv
 *   Private structure.
 * @param mlx5_fdir_filter
 *   Filter to create steering rule for.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_filter_enable(struct priv *priv,
			struct mlx5_fdir_filter *mlx5_fdir_filter)
{
	struct fdir_queue *fdir_queue;

	/* Check if flow already exists. */
	if (mlx5_fdir_filter->flow != NULL)
		return 0;

	/* Get fdir_queue for specific queue. */
	fdir_queue = priv_get_fdir_queue(priv, mlx5_fdir_filter->queue);

	if (fdir_queue == NULL) {
		ERROR("failed to create flow director rxq for queue %d",
		      mlx5_fdir_filter->queue);
		return EINVAL;
	}

	/* Create flow */
	return priv_fdir_flow_add(priv, mlx5_fdir_filter, fdir_queue);
}

/**
 * Initialize flow director filters list.
 *
 * @param priv
 *   Private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
fdir_init_filters_list(struct priv *priv)
{
	/* Filter list initialization should be done only once. */
	if (priv->fdir_filter_list)
		return 0;

	/* Create filters list. */
	priv->fdir_filter_list =
		rte_calloc(__func__, 1, sizeof(*priv->fdir_filter_list), 0);

	if (priv->fdir_filter_list == NULL) {
		int err = ENOMEM;

		ERROR("cannot allocate flow director filter list: %s",
		      strerror(err));
		return err;
	}

	LIST_INIT(priv->fdir_filter_list);

	return 0;
}

/**
 * Flush all filters.
 *
 * @param priv
 *   Private structure.
 */
static void
priv_fdir_filter_flush(struct priv *priv)
{
	struct mlx5_fdir_filter *mlx5_fdir_filter;

	while ((mlx5_fdir_filter = LIST_FIRST(priv->fdir_filter_list))) {
		struct ibv_exp_flow *flow = mlx5_fdir_filter->flow;

		DEBUG("%p: flushing flow director filter %p",
		      (void *)priv, (void *)mlx5_fdir_filter);
		LIST_REMOVE(mlx5_fdir_filter, next);
		if (flow != NULL)
			claim_zero(ibv_exp_destroy_flow(flow));
		rte_free(mlx5_fdir_filter);
	}
}

/**
 * Remove all flow director filters and delete list.
 *
 * @param priv
 *   Private structure.
 */
void
priv_fdir_delete_filters_list(struct priv *priv)
{
	priv_fdir_filter_flush(priv);
	rte_free(priv->fdir_filter_list);
	priv->fdir_filter_list = NULL;
}

/**
 * Disable flow director, remove all steering rules.
 *
 * @param priv
 *   Private structure.
 */
void
priv_fdir_disable(struct priv *priv)
{
	unsigned int i;
	struct mlx5_fdir_filter *mlx5_fdir_filter;
	struct fdir_queue *fdir_queue;

	/* Run on every flow director filter and destroy flow handle. */
	LIST_FOREACH(mlx5_fdir_filter, priv->fdir_filter_list, next) {
		struct ibv_exp_flow *flow;

		/* Only valid elements should be in the list */
		assert(mlx5_fdir_filter != NULL);
		flow = mlx5_fdir_filter->flow;

		/* Destroy flow handle */
		if (flow != NULL) {
			claim_zero(ibv_exp_destroy_flow(flow));
			mlx5_fdir_filter->flow = NULL;
		}
	}

	/* Run on every RX queue to destroy related flow director QP and
	 * indirection table. */
	for (i = 0; (i != priv->rxqs_n); i++) {
		struct rxq_ctrl *rxq_ctrl =
			container_of((*priv->rxqs)[i], struct rxq_ctrl, rxq);

		fdir_queue = &rxq_ctrl->fdir_queue;
		if (fdir_queue->qp != NULL) {
			claim_zero(ibv_destroy_qp(fdir_queue->qp));
			fdir_queue->qp = NULL;
		}

		if (fdir_queue->ind_table != NULL) {
			claim_zero(ibv_exp_destroy_rwq_ind_table
				   (fdir_queue->ind_table));
			fdir_queue->ind_table = NULL;
		}
	}
}

/**
 * Enable flow director, create steering rules.
 *
 * @param priv
 *   Private structure.
 */
void
priv_fdir_enable(struct priv *priv)
{
	struct mlx5_fdir_filter *mlx5_fdir_filter;

	/* Run on every fdir filter and create flow handle */
	LIST_FOREACH(mlx5_fdir_filter, priv->fdir_filter_list, next) {
		/* Only valid elements should be in the list */
		assert(mlx5_fdir_filter != NULL);

		priv_fdir_filter_enable(priv, mlx5_fdir_filter);
	}
}

/**
 * Find specific filter in list.
 *
 * @param priv
 *   Private structure.
 * @param fdir_filter
 *   Flow director filter to find.
 *
 * @return
 *   Filter element if found, otherwise NULL.
 */
static struct mlx5_fdir_filter *
priv_find_filter_in_list(struct priv *priv,
			 const struct rte_eth_fdir_filter *fdir_filter)
{
	struct fdir_flow_desc desc;
	struct mlx5_fdir_filter *mlx5_fdir_filter;
	enum rte_fdir_mode fdir_mode = priv->dev->data->dev_conf.fdir_conf.mode;

	/* Get flow director filter to look for. */
	fdir_filter_to_flow_desc(fdir_filter, &desc, fdir_mode);

	/* Look for the requested element. */
	LIST_FOREACH(mlx5_fdir_filter, priv->fdir_filter_list, next) {
		/* Only valid elements should be in the list. */
		assert(mlx5_fdir_filter != NULL);

		/* Return matching filter. */
		if (!memcmp(&desc, &mlx5_fdir_filter->desc, sizeof(desc)))
			return mlx5_fdir_filter;
	}

	/* Filter not found */
	return NULL;
}

/**
 * Add new flow director filter and store it in list.
 *
 * @param priv
 *   Private structure.
 * @param fdir_filter
 *   Flow director filter to add.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_filter_add(struct priv *priv,
		     const struct rte_eth_fdir_filter *fdir_filter)
{
	struct mlx5_fdir_filter *mlx5_fdir_filter;
	enum rte_fdir_mode fdir_mode = priv->dev->data->dev_conf.fdir_conf.mode;
	int err = 0;

	/* Validate queue number. */
	if (fdir_filter->action.rx_queue >= priv->rxqs_n) {
		ERROR("invalid queue number %d", fdir_filter->action.rx_queue);
		return EINVAL;
	}

	/* Duplicate filters are currently unsupported. */
	mlx5_fdir_filter = priv_find_filter_in_list(priv, fdir_filter);
	if (mlx5_fdir_filter != NULL) {
#ifndef TREX_PATCH
		ERROR("filter already exists");
#endif
		return EEXIST;
	}

	/* Create new flow director filter. */
	mlx5_fdir_filter =
		rte_calloc(__func__, 1, sizeof(*mlx5_fdir_filter), 0);
	if (mlx5_fdir_filter == NULL) {
		err = ENOMEM;
		ERROR("cannot allocate flow director filter: %s",
		      strerror(err));
		return err;
	}

	/* Set queue. */
	mlx5_fdir_filter->queue = fdir_filter->action.rx_queue;

	/* Convert to mlx5 filter descriptor. */
	fdir_filter_to_flow_desc(fdir_filter,
				 &mlx5_fdir_filter->desc, fdir_mode);

	/* Insert new filter into list. */
	LIST_INSERT_HEAD(priv->fdir_filter_list, mlx5_fdir_filter, next);

	DEBUG("%p: flow director filter %p added",
	      (void *)priv, (void *)mlx5_fdir_filter);

	/* Enable filter immediately if device is started. */
	if (priv->started)
		err = priv_fdir_filter_enable(priv, mlx5_fdir_filter);

	return err;
}

/**
 * Update queue for specific filter.
 *
 * @param priv
 *   Private structure.
 * @param fdir_filter
 *   Filter to be updated.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_filter_update(struct priv *priv,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	struct mlx5_fdir_filter *mlx5_fdir_filter;

	/* Validate queue number. */
	if (fdir_filter->action.rx_queue >= priv->rxqs_n) {
		ERROR("invalid queue number %d", fdir_filter->action.rx_queue);
		return EINVAL;
	}

	mlx5_fdir_filter = priv_find_filter_in_list(priv, fdir_filter);
	if (mlx5_fdir_filter != NULL) {
		struct ibv_exp_flow *flow = mlx5_fdir_filter->flow;
		int err = 0;

		/* Update queue number. */
		mlx5_fdir_filter->queue = fdir_filter->action.rx_queue;

		/* Destroy flow handle. */
		if (flow != NULL) {
			claim_zero(ibv_exp_destroy_flow(flow));
			mlx5_fdir_filter->flow = NULL;
		}
		DEBUG("%p: flow director filter %p updated",
		      (void *)priv, (void *)mlx5_fdir_filter);

		/* Enable filter if device is started. */
		if (priv->started)
			err = priv_fdir_filter_enable(priv, mlx5_fdir_filter);

		return err;
	}

	/* Filter not found, create it. */
	DEBUG("%p: filter not found for update, creating new filter",
	      (void *)priv);
	return priv_fdir_filter_add(priv, fdir_filter);
}

/**
 * Delete specific filter.
 *
 * @param priv
 *   Private structure.
 * @param fdir_filter
 *   Filter to be deleted.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_filter_delete(struct priv *priv,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	struct mlx5_fdir_filter *mlx5_fdir_filter;

	mlx5_fdir_filter = priv_find_filter_in_list(priv, fdir_filter);
	if (mlx5_fdir_filter != NULL) {
		struct ibv_exp_flow *flow = mlx5_fdir_filter->flow;

		/* Remove element from list. */
		LIST_REMOVE(mlx5_fdir_filter, next);

		/* Destroy flow handle. */
		if (flow != NULL) {
			claim_zero(ibv_exp_destroy_flow(flow));
			mlx5_fdir_filter->flow = NULL;
		}

		DEBUG("%p: flow director filter %p deleted",
		      (void *)priv, (void *)mlx5_fdir_filter);

		/* Delete filter. */
		rte_free(mlx5_fdir_filter);

		return 0;
	}

#ifndef TREX_PATCH
	ERROR("%p: flow director delete failed, cannot find filter",
	      (void *)priv);
#endif
	return ENOENT;
}

/**
 * Get flow director information.
 *
 * @param priv
 *   Private structure.
 * @param[out] fdir_info
 *   Resulting flow director information.
 */
static void
priv_fdir_info_get(struct priv *priv, struct rte_eth_fdir_info *fdir_info)
{
	struct rte_eth_fdir_masks *mask =
		&priv->dev->data->dev_conf.fdir_conf.mask;

	fdir_info->mode = priv->dev->data->dev_conf.fdir_conf.mode;
	fdir_info->guarant_spc = 0;

	rte_memcpy(&fdir_info->mask, mask, sizeof(fdir_info->mask));

	fdir_info->max_flexpayload = 0;
	fdir_info->flow_types_mask[0] = 0;

	fdir_info->flex_payload_unit = 0;
	fdir_info->max_flex_payload_segment_num = 0;
	fdir_info->flex_payload_limit = 0;
	memset(&fdir_info->flex_conf, 0, sizeof(fdir_info->flex_conf));
}

/**
 * Deal with flow director operations.
 *
 * @param priv
 *   Pointer to private structure.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_fdir_ctrl_func(struct priv *priv, enum rte_filter_op filter_op, void *arg)
{
	enum rte_fdir_mode fdir_mode =
		priv->dev->data->dev_conf.fdir_conf.mode;
	int ret = 0;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (fdir_mode != RTE_FDIR_MODE_PERFECT &&
	    fdir_mode != RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		ERROR("%p: flow director mode %d not supported",
		      (void *)priv, fdir_mode);
		return EINVAL;
	}

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = priv_fdir_filter_add(priv, arg);
		break;
	case RTE_ETH_FILTER_UPDATE:
		ret = priv_fdir_filter_update(priv, arg);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = priv_fdir_filter_delete(priv, arg);
		break;
	case RTE_ETH_FILTER_FLUSH:
		priv_fdir_filter_flush(priv);
		break;
	case RTE_ETH_FILTER_INFO:
		priv_fdir_info_get(priv, arg);
		break;
	default:
		DEBUG("%p: unknown operation %u", (void *)priv, filter_op);
		ret = EINVAL;
		break;
	}
	return ret;
}

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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = -EINVAL;
	struct priv *priv = dev->data->dev_private;

	switch (filter_type) {
	case RTE_ETH_FILTER_FDIR:
		priv_lock(priv);
		ret = priv_fdir_ctrl_func(priv, filter_op, arg);
		priv_unlock(priv);
		break;
	default:
		ERROR("%p: filter type (%d) not supported",
		      (void *)dev, filter_type);
		break;
	}

	return ret;
}
