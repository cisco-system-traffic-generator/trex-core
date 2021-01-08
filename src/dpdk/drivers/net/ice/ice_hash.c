/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "ice_logs.h"
#include "base/ice_type.h"
#include "base/ice_flow.h"
#include "ice_ethdev.h"
#include "ice_generic_flow.h"

struct rss_type_match_hdr {
	uint32_t hdr_mask;
	uint64_t eth_rss_hint;
};

struct ice_hash_match_type {
	uint64_t hash_type;
	uint64_t hash_flds;
};

struct rss_meta {
	uint32_t pkt_hdr;
	uint64_t hash_flds;
	uint8_t hash_function;
};

struct ice_hash_flow_cfg {
	bool simple_xor;
	struct ice_rss_cfg rss_cfg;
};

static int
ice_hash_init(struct ice_adapter *ad);

static int
ice_hash_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error);

static int
ice_hash_destroy(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error);

static void
ice_hash_uninit(struct ice_adapter *ad);

static void
ice_hash_free(struct rte_flow *flow);

static int
ice_hash_parse_pattern_action(struct ice_adapter *ad,
			struct ice_pattern_match_item *array,
			uint32_t array_len,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			void **meta,
			struct rte_flow_error *error);

/* The first member is protocol header, the second member is ETH_RSS_*. */
struct rss_type_match_hdr hint_0 = {
	ICE_FLOW_SEG_HDR_NONE,	0};
struct rss_type_match_hdr hint_1 = {
	ICE_FLOW_SEG_HDR_IPV4,	ETH_RSS_IPV4};
struct rss_type_match_hdr hint_2 = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_UDP, ETH_RSS_NONFRAG_IPV4_UDP};
struct rss_type_match_hdr hint_3 = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_TCP, ETH_RSS_NONFRAG_IPV4_TCP};
struct rss_type_match_hdr hint_4 = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP};
struct rss_type_match_hdr hint_5 = {
	ICE_FLOW_SEG_HDR_IPV6,	ETH_RSS_IPV6};
struct rss_type_match_hdr hint_6 = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_UDP, ETH_RSS_NONFRAG_IPV6_UDP};
struct rss_type_match_hdr hint_7 = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_TCP, ETH_RSS_NONFRAG_IPV6_TCP};
struct rss_type_match_hdr hint_8 = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP};
struct rss_type_match_hdr hint_9 = {
	ICE_FLOW_SEG_HDR_GTPU_IP,	ETH_RSS_IPV4};
struct rss_type_match_hdr hint_10 = {
	ICE_FLOW_SEG_HDR_PPPOE,	ETH_RSS_IPV4};
struct rss_type_match_hdr hint_11 = {
	ICE_FLOW_SEG_HDR_PPPOE,	ETH_RSS_NONFRAG_IPV4_UDP};
struct rss_type_match_hdr hint_12 = {
	ICE_FLOW_SEG_HDR_PPPOE,	ETH_RSS_NONFRAG_IPV4_TCP};
struct rss_type_match_hdr hint_13 = {
	ICE_FLOW_SEG_HDR_PPPOE,	ETH_RSS_NONFRAG_IPV4_SCTP};

/* Supported pattern for os default package. */
static struct ice_pattern_match_item ice_hash_pattern_list_os[] = {
	{pattern_eth_ipv4,	ICE_INSET_NONE,	&hint_1},
	{pattern_eth_ipv4_udp,	ICE_INSET_NONE,	&hint_2},
	{pattern_eth_ipv4_tcp,	ICE_INSET_NONE,	&hint_3},
	{pattern_eth_ipv4_sctp,	ICE_INSET_NONE,	&hint_4},
	{pattern_eth_ipv6,	ICE_INSET_NONE,	&hint_5},
	{pattern_eth_ipv6_udp,	ICE_INSET_NONE,	&hint_6},
	{pattern_eth_ipv6_tcp,	ICE_INSET_NONE,	&hint_7},
	{pattern_eth_ipv6_sctp,	ICE_INSET_NONE,	&hint_8},
	{pattern_empty,		ICE_INSET_NONE,	&hint_0},
};

/* Supported pattern for comms package. */
static struct ice_pattern_match_item ice_hash_pattern_list_comms[] = {
	{pattern_eth_ipv4,		    ICE_INSET_NONE,  &hint_1},
	{pattern_eth_ipv4_udp,		    ICE_INSET_NONE,  &hint_2},
	{pattern_eth_ipv4_tcp,		    ICE_INSET_NONE,  &hint_3},
	{pattern_eth_ipv4_sctp,		    ICE_INSET_NONE,  &hint_4},
	{pattern_eth_ipv6,		    ICE_INSET_NONE,  &hint_5},
	{pattern_eth_ipv6_udp,		    ICE_INSET_NONE,  &hint_6},
	{pattern_eth_ipv6_tcp,		    ICE_INSET_NONE,  &hint_7},
	{pattern_eth_ipv6_sctp,		    ICE_INSET_NONE,  &hint_8},
	{pattern_empty,			    ICE_INSET_NONE,  &hint_0},
	{pattern_eth_ipv4_gtpu_eh_ipv4,	    ICE_INSET_NONE,  &hint_9},
	{pattern_eth_ipv4_gtpu_eh_ipv4_udp, ICE_INSET_NONE,  &hint_9},
	{pattern_eth_ipv4_gtpu_eh_ipv4_tcp, ICE_INSET_NONE,  &hint_9},
	{pattern_eth_pppoes_ipv4,	    ICE_INSET_NONE,  &hint_10},
	{pattern_eth_pppoes_ipv4_udp,	    ICE_INSET_NONE,  &hint_11},
	{pattern_eth_pppoes_ipv4_tcp,	    ICE_INSET_NONE,  &hint_12},
	{pattern_eth_pppoes_ipv4_sctp,	    ICE_INSET_NONE,  &hint_13},
};

/**
 * The first member is input set combination,
 * the second member is hash fields.
 */
struct ice_hash_match_type ice_hash_type_list[] = {
	{ETH_RSS_IPV4 | ETH_RSS_L3_SRC_ONLY,					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA)},
	{ETH_RSS_IPV4 | ETH_RSS_L3_DST_ONLY,					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA)},
	{ETH_RSS_IPV4,								ICE_FLOW_HASH_IPV4},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_UDP,						ICE_HASH_UDP_IPV4},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_TCP,						ICE_HASH_TCP_IPV4},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV4_SCTP,						ICE_HASH_SCTP_IPV4},
	{ETH_RSS_IPV6 | ETH_RSS_L3_SRC_ONLY,					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA)},
	{ETH_RSS_IPV6 | ETH_RSS_L3_DST_ONLY,					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA)},
	{ETH_RSS_IPV6,								ICE_FLOW_HASH_IPV6},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_UDP,						ICE_HASH_UDP_IPV6},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_TCP,						ICE_HASH_TCP_IPV6},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,	BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA) | BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L3_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L4_SRC_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP | ETH_RSS_L4_DST_ONLY,			BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT)},
	{ETH_RSS_NONFRAG_IPV6_SCTP,						ICE_HASH_SCTP_IPV6},
};

static struct ice_flow_engine ice_hash_engine = {
	.init = ice_hash_init,
	.create = ice_hash_create,
	.destroy = ice_hash_destroy,
	.uninit = ice_hash_uninit,
	.free = ice_hash_free,
	.type = ICE_FLOW_ENGINE_HASH,
};

/* Register parser for os package. */
static struct ice_flow_parser ice_hash_parser_os = {
	.engine = &ice_hash_engine,
	.array = ice_hash_pattern_list_os,
	.array_len = RTE_DIM(ice_hash_pattern_list_os),
	.parse_pattern_action = ice_hash_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_RSS,
};

/* Register parser for comms package. */
static struct ice_flow_parser ice_hash_parser_comms = {
	.engine = &ice_hash_engine,
	.array = ice_hash_pattern_list_comms,
	.array_len = RTE_DIM(ice_hash_pattern_list_comms),
	.parse_pattern_action = ice_hash_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_RSS,
};

RTE_INIT(ice_hash_engine_init)
{
	struct ice_flow_engine *engine = &ice_hash_engine;
	ice_register_flow_engine(engine);
}

static int
ice_hash_init(struct ice_adapter *ad)
{
	struct ice_flow_parser *parser = NULL;

	if (ad->active_pkg_type == ICE_PKG_TYPE_OS_DEFAULT)
		parser = &ice_hash_parser_os;
	else if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		parser = &ice_hash_parser_comms;
	else
		return -EINVAL;

	return ice_register_parser(parser, ad);
}

static int
ice_hash_check_inset(const struct rte_flow_item pattern[],
		struct rte_flow_error *error)
{
	const struct rte_flow_item *item = pattern;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not support range");
			return -rte_errno;
		}

		/* Ignore spec and mask. */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Invalid spec/mask.");
			return -rte_errno;
		}
	}

	return 0;
}

static int
ice_hash_parse_action(struct ice_pattern_match_item *pattern_match_item,
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error)
{
	const struct rte_flow_action *action;
	enum rte_flow_action_type action_type;
	const struct rte_flow_action_rss *rss;
	struct rss_type_match_hdr *m = (struct rss_type_match_hdr *)
				(pattern_match_item->meta);
	uint32_t type_list_len = RTE_DIM(ice_hash_type_list);
	struct ice_hash_match_type *type_match_item;
	uint64_t rss_hf;
	uint16_t i;

	/* Supported action is RSS. */
	for (action = actions; action->type !=
		RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = action->conf;
			rss_hf = rss->types;

			/**
			 * Check simultaneous use of SRC_ONLY and DST_ONLY
			 * of the same level.
			 */
			rss_hf = rte_eth_rss_hf_refine(rss_hf);

			/* Check if pattern is empty. */
			if (pattern_match_item->pattern_list !=
				pattern_empty && rss->func ==
				RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Not supported flow");

			/* Check if rss types match pattern. */
			if (rss->func != RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
				if (((rss_hf & ETH_RSS_IPV4) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV4_SCTP) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_IPV6) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV6_UDP) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV6_TCP) != m->eth_rss_hint) &&
				((rss_hf & ETH_RSS_NONFRAG_IPV6_SCTP) != m->eth_rss_hint))
					return rte_flow_error_set(error,
					ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
					action, "Not supported RSS types");
			}

			if (rss->level)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS encapsulation level is not supported");

			if (rss->key_len)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS key_len is not supported");

			if (rss->queue)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a non-NULL RSS queue is not supported");

			/* Check hash function and save it to rss_meta. */
			if (rss->func ==
				RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
				((struct rss_meta *)*meta)->hash_function =
				RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;

			if (rss->func ==
				RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
				((struct rss_meta *)*meta)->hash_function =
				RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;

			type_match_item = rte_zmalloc("ice_type_match_item",
					sizeof(struct ice_hash_match_type), 0);
			if (!type_match_item) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"No memory for type_match_item");
				return -ENOMEM;
			}

			/* Find matched hash fields according to hash type. */
			for (i = 0; i < type_list_len; i++) {
				if (rss_hf ==
					ice_hash_type_list[i].hash_type) {
					type_match_item->hash_type =
						ice_hash_type_list[i].hash_type;
					type_match_item->hash_flds =
						ice_hash_type_list[i].hash_flds;
				}
			}

			/* Save hash fileds to rss_meta. */
			((struct rss_meta *)*meta)->hash_flds =
					type_match_item->hash_flds;

			rte_free(type_match_item);
			break;

		case RTE_FLOW_ACTION_TYPE_END:
			break;

		default:
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Invalid action.");
			return -rte_errno;
		}
	}

	return 0;
}

static int
ice_hash_parse_pattern_action(__rte_unused struct ice_adapter *ad,
			struct ice_pattern_match_item *array,
			uint32_t array_len,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			void **meta,
			struct rte_flow_error *error)
{
	int ret = -rte_errno;
	struct ice_pattern_match_item *pattern_match_item;
	struct rss_meta *rss_meta_ptr;

	rss_meta_ptr = rte_zmalloc(NULL, sizeof(*rss_meta_ptr), 0);
	if (!rss_meta_ptr) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"No memory for rss_meta_ptr");
		return -ENOMEM;
	}

	/* Check rss supported pattern and find matched pattern. */
	pattern_match_item = ice_search_pattern_match_item(pattern,
					array, array_len, error);
	if (!pattern_match_item)
		goto error;

	ret = ice_hash_check_inset(pattern, error);
	if (ret)
		goto error;

	/* Save protocol header to rss_meta. */
	*meta = rss_meta_ptr;
	((struct rss_meta *)*meta)->pkt_hdr = ((struct rss_type_match_hdr *)
		(pattern_match_item->meta))->hdr_mask;

	/* Check rss action. */
	ret = ice_hash_parse_action(pattern_match_item, actions, meta, error);
error:
	if (ret)
		rte_free(rss_meta_ptr);
	rte_free(pattern_match_item);

	return ret;
}

static int
ice_hash_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;
	uint32_t reg;
	struct ice_hash_flow_cfg *filter_ptr;

	uint32_t headermask = ((struct rss_meta *)meta)->pkt_hdr;
	uint64_t hash_field = ((struct rss_meta *)meta)->hash_flds;
	uint8_t hash_function = ((struct rss_meta *)meta)->hash_function;

	filter_ptr = rte_zmalloc("ice_rss_filter",
				sizeof(struct ice_hash_flow_cfg), 0);
	if (!filter_ptr) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"No memory for filter_ptr");
		return -ENOMEM;
	}

	if (hash_function == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		/* Enable registers for simple_xor hash function. */
		reg = ICE_READ_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id));
		reg = (reg & (~VSIQF_HASH_CTL_HASH_SCHEME_M)) |
			(2 << VSIQF_HASH_CTL_HASH_SCHEME_S);
		ICE_WRITE_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id), reg);

		filter_ptr->simple_xor = 1;

		goto out;
	} else {
		filter_ptr->rss_cfg.packet_hdr = headermask;
		filter_ptr->rss_cfg.hashed_flds = hash_field;
		filter_ptr->rss_cfg.symm =
			(hash_function ==
				RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ);

		ret = ice_add_rss_cfg(hw, vsi->idx,
				filter_ptr->rss_cfg.hashed_flds,
				filter_ptr->rss_cfg.packet_hdr,
				filter_ptr->rss_cfg.symm);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"rss flow create fail");
			goto error;
		}
	}

out:
	flow->rule = filter_ptr;
	rte_free(meta);
	return 0;

error:
	rte_free(filter_ptr);
	rte_free(meta);
	return -rte_errno;
}

static int
ice_hash_destroy(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(ad);
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;
	uint32_t reg;
	struct ice_hash_flow_cfg *filter_ptr;

	filter_ptr = (struct ice_hash_flow_cfg *)flow->rule;

	if (filter_ptr->simple_xor == 1) {
		/* Return to symmetric_toeplitz state. */
		reg = ICE_READ_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id));
		reg = (reg & (~VSIQF_HASH_CTL_HASH_SCHEME_M)) |
			(1 << VSIQF_HASH_CTL_HASH_SCHEME_S);
		ICE_WRITE_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id), reg);
	} else {
		ret = ice_rem_rss_cfg(hw, vsi->idx,
				filter_ptr->rss_cfg.hashed_flds,
				filter_ptr->rss_cfg.packet_hdr);
		/* Fixme: Ignore the error if a rule does not exist.
		 * Currently a rule for inputset change or symm turn on/off
		 * will overwrite an exist rule, while application still
		 * have 2 rte_flow handles.
		 **/
		if (ret && ret != ICE_ERR_DOES_NOT_EXIST) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"rss flow destroy fail");
			goto error;
		}
	}

	rte_free(filter_ptr);
	return 0;

error:
	rte_free(filter_ptr);
	return -rte_errno;
}

static void
ice_hash_uninit(struct ice_adapter *ad)
{
	if (ad->active_pkg_type == ICE_PKG_TYPE_OS_DEFAULT)
		ice_unregister_parser(&ice_hash_parser_os, ad);
	else if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS)
		ice_unregister_parser(&ice_hash_parser_comms, ad);
}

static void
ice_hash_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
}
