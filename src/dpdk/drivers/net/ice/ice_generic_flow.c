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

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "ice_ethdev.h"
#include "ice_generic_flow.h"

/**
 * Non-pipeline mode, fdir and switch both used as distributor,
 * fdir used first, switch used as fdir's backup.
 */
#define ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR_ONLY 0
/*Pipeline mode, switch used at permission stage*/
#define ICE_FLOW_CLASSIFY_STAGE_PERMISSION 1
/*Pipeline mode, fdir used at distributor stage*/
#define ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR 2

static struct ice_engine_list engine_list =
		TAILQ_HEAD_INITIALIZER(engine_list);

static int ice_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static struct rte_flow *ice_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static int ice_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error);
static int ice_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error);
static int ice_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error);

const struct rte_flow_ops ice_flow_ops = {
	.validate = ice_flow_validate,
	.create = ice_flow_create,
	.destroy = ice_flow_destroy,
	.flush = ice_flow_flush,
	.query = ice_flow_query,
};

/* empty */
enum rte_flow_item_type pattern_empty[] = {
	RTE_FLOW_ITEM_TYPE_END,
};

/* L2 */
enum rte_flow_item_type pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_ethertype_vlan[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_ethertype_qinq[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

/* ARP */
enum rte_flow_item_type pattern_eth_arp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

/* non-tunnel IPv4 */
enum rte_flow_item_type pattern_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* non-tunnel IPv6 */
enum rte_flow_item_type pattern_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 VXLAN IPv4 */
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 VXLAN MAC IPv4 */
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 VXLAN IPv4 */
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 VXLAN MAC IPv4 */
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 VXLAN IPv6 */
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 VXLAN MAC IPv6 */
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 VXLAN IPv6 */
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 VXLAN MAC IPv6 */
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 NVGRE IPv4 */
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 NVGRE MAC IPv4 */
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 NVGRE IPv4 */
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 NVGRE MAC IPv4 */
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 NVGRE IPv6 */
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv4 NVGRE MAC IPv6 */
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 NVGRE IPv6 */
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* IPv6 NVGRE MAC IPv6 */
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

/* GTPU */
enum rte_flow_item_type pattern_eth_ipv4_gtpu[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,

};
enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_GTPU,
	RTE_FLOW_ITEM_TYPE_GTP_PSC,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* PPPoE */
enum rte_flow_item_type pattern_eth_pppoed[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOED,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoed[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOED,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoed[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOED,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_icmp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_pppoes_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};
enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_icmp6[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_PPPOES,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP6,
	RTE_FLOW_ITEM_TYPE_END,
};

typedef struct ice_flow_engine * (*parse_engine_t)(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct ice_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error);

void
ice_register_flow_engine(struct ice_flow_engine *engine)
{
	TAILQ_INSERT_TAIL(&engine_list, engine, node);
}

int
ice_flow_init(struct ice_adapter *ad)
{
	int ret;
	struct ice_pf *pf = &ad->pf;
	void *temp;
	struct ice_flow_engine *engine;

	TAILQ_INIT(&pf->flow_list);
	TAILQ_INIT(&pf->rss_parser_list);
	TAILQ_INIT(&pf->perm_parser_list);
	TAILQ_INIT(&pf->dist_parser_list);

	TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (engine->init == NULL) {
			PMD_INIT_LOG(ERR, "Invalid engine type (%d)",
					engine->type);
			return -ENOTSUP;
		}

		ret = engine->init(ad);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to initialize engine %d",
					engine->type);
			return ret;
		}
	}
	return 0;
}

void
ice_flow_uninit(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_flow_engine *engine;
	struct rte_flow *p_flow;
	struct ice_flow_parser_node *p_parser;
	void *temp;

	TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (engine->uninit)
			engine->uninit(ad);
	}

	/* Remove all flows */
	while ((p_flow = TAILQ_FIRST(&pf->flow_list))) {
		TAILQ_REMOVE(&pf->flow_list, p_flow, node);
		if (p_flow->engine->free)
			p_flow->engine->free(p_flow);
		rte_free(p_flow);
	}

	/* Cleanup parser list */
	while ((p_parser = TAILQ_FIRST(&pf->rss_parser_list))) {
		TAILQ_REMOVE(&pf->rss_parser_list, p_parser, node);
		rte_free(p_parser);
	}

	while ((p_parser = TAILQ_FIRST(&pf->perm_parser_list))) {
		TAILQ_REMOVE(&pf->perm_parser_list, p_parser, node);
		rte_free(p_parser);
	}

	while ((p_parser = TAILQ_FIRST(&pf->dist_parser_list))) {
		TAILQ_REMOVE(&pf->dist_parser_list, p_parser, node);
		rte_free(p_parser);
	}
}

static struct ice_parser_list *
ice_get_parser_list(struct ice_flow_parser *parser,
		struct ice_adapter *ad)
{
	struct ice_parser_list *list;
	struct ice_pf *pf = &ad->pf;

	switch (parser->stage) {
	case ICE_FLOW_STAGE_RSS:
		list = &pf->rss_parser_list;
		break;
	case ICE_FLOW_STAGE_PERMISSION:
		list = &pf->perm_parser_list;
		break;
	case ICE_FLOW_STAGE_DISTRIBUTOR:
		list = &pf->dist_parser_list;
		break;
	default:
		return NULL;
	}

	return list;
}

int
ice_register_parser(struct ice_flow_parser *parser,
		struct ice_adapter *ad)
{
	struct ice_parser_list *list;
	struct ice_flow_parser_node *parser_node;

	parser_node = rte_zmalloc("ice_parser", sizeof(*parser_node), 0);
	if (parser_node == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory.");
		return -ENOMEM;
	}
	parser_node->parser = parser;

	list = ice_get_parser_list(parser, ad);
	if (list == NULL)
		return -EINVAL;

	if (ad->devargs.pipe_mode_support) {
		TAILQ_INSERT_TAIL(list, parser_node, node);
	} else {
		if (parser->engine->type == ICE_FLOW_ENGINE_SWITCH ||
				parser->engine->type == ICE_FLOW_ENGINE_HASH)
			TAILQ_INSERT_TAIL(list, parser_node, node);
		else if (parser->engine->type == ICE_FLOW_ENGINE_FDIR)
			TAILQ_INSERT_HEAD(list, parser_node, node);
		else
			return -EINVAL;
	}
	return 0;
}

void
ice_unregister_parser(struct ice_flow_parser *parser,
		struct ice_adapter *ad)
{
	struct ice_parser_list *list;
	struct ice_flow_parser_node *p_parser;
	void *temp;

	list = ice_get_parser_list(parser, ad);
	if (list == NULL)
		return;

	TAILQ_FOREACH_SAFE(p_parser, list, node, temp) {
		if (p_parser->parser->engine->type == parser->engine->type) {
			TAILQ_REMOVE(list, p_parser, node);
			rte_free(p_parser);
		}
	}
}

static int
ice_flow_valid_attr(struct ice_adapter *ad,
		const struct rte_flow_attr *attr,
		int *ice_pipeline_stage,
		struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				attr, "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				attr, "Not support egress.");
		return -rte_errno;
	}

	/* Check pipeline mode support to set classification stage */
	if (ad->devargs.pipe_mode_support) {
		if (attr->priority == 0)
			*ice_pipeline_stage =
				ICE_FLOW_CLASSIFY_STAGE_PERMISSION;
		else
			*ice_pipeline_stage =
				ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR;
	} else {
		*ice_pipeline_stage =
			ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR_ONLY;
		/* Not supported */
		if (attr->priority) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					attr, "Not support priority.");
			return -rte_errno;
		}
	}

	/* Not supported */
	if (attr->group) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				attr, "Not support group.");
		return -rte_errno;
	}

	return 0;
}

/* Find the first VOID or non-VOID item pointer */
static const struct rte_flow_item *
ice_find_first_item(const struct rte_flow_item *item, bool is_void)
{
	bool is_find;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (is_void)
			is_find = item->type == RTE_FLOW_ITEM_TYPE_VOID;
		else
			is_find = item->type != RTE_FLOW_ITEM_TYPE_VOID;
		if (is_find)
			break;
		item++;
	}
	return item;
}

/* Skip all VOID items of the pattern */
static void
ice_pattern_skip_void_item(struct rte_flow_item *items,
			const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = ice_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = ice_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END) {
			break;
		}

		pb = pe + 1;
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
ice_match_pattern(enum rte_flow_item_type *item_array,
		const struct rte_flow_item *pattern)
{
	const struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

struct ice_pattern_match_item *
ice_search_pattern_match_item(const struct rte_flow_item pattern[],
		struct ice_pattern_match_item *array,
		uint32_t array_len,
		struct rte_flow_error *error)
{
	uint16_t i = 0;
	struct ice_pattern_match_item *pattern_match_item;
	/* need free by each filter */
	struct rte_flow_item *items; /* used for pattern without VOID items */
	uint32_t item_num = 0; /* non-void item number */

	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	items = rte_zmalloc("ice_pattern",
			    item_num * sizeof(struct rte_flow_item), 0);
	if (!items) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "No memory for PMD internal items.");
		return NULL;
	}
	pattern_match_item = rte_zmalloc("ice_pattern_match_item",
			sizeof(struct ice_pattern_match_item), 0);
	if (!pattern_match_item) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failed to allocate memory.");
		return NULL;
	}

	ice_pattern_skip_void_item(items, pattern);

	for (i = 0; i < array_len; i++)
		if (ice_match_pattern(array[i].pattern_list,
					items)) {
			pattern_match_item->input_set_mask =
				array[i].input_set_mask;
			pattern_match_item->pattern_list =
				array[i].pattern_list;
			pattern_match_item->meta = array[i].meta;
			rte_free(items);
			return pattern_match_item;
		}
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			   pattern, "Unsupported pattern");

	rte_free(items);
	rte_free(pattern_match_item);
	return NULL;
}

static struct ice_flow_engine *
ice_parse_engine_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct ice_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error)
{
	struct ice_flow_engine *engine = NULL;
	struct ice_flow_parser_node *parser_node;
	void *temp;

	TAILQ_FOREACH_SAFE(parser_node, parser_list, node, temp) {
		int ret;

		if (parser_node->parser->parse_pattern_action(ad,
				parser_node->parser->array,
				parser_node->parser->array_len,
				pattern, actions, meta, error) < 0)
			continue;

		engine = parser_node->parser->engine;
		if (engine->create == NULL) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE,
					NULL, "Invalid engine");
			continue;
		}

		ret = engine->create(ad, flow, *meta, error);
		if (ret == 0)
			return engine;
		else if (ret == -EEXIST)
			return NULL;
	}
	return NULL;
}

static struct ice_flow_engine *
ice_parse_engine_validate(struct ice_adapter *ad,
		struct rte_flow *flow __rte_unused,
		struct ice_parser_list *parser_list,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error)
{
	struct ice_flow_engine *engine = NULL;
	struct ice_flow_parser_node *parser_node;
	void *temp;

	TAILQ_FOREACH_SAFE(parser_node, parser_list, node, temp) {
		if (parser_node->parser->parse_pattern_action(ad,
				parser_node->parser->array,
				parser_node->parser->array_len,
				pattern, actions, meta, error) < 0)
			continue;

		engine = parser_node->parser->engine;
		break;
	}
	return engine;
}

static int
ice_flow_process_filter(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct ice_flow_engine **engine,
		void **meta,
		parse_engine_t ice_parse_engine,
		struct rte_flow_error *error)
{
	int ret = ICE_ERR_NOT_SUPPORTED;
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ice_pipeline_stage = 0;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	ret = ice_flow_valid_attr(ad, attr, &ice_pipeline_stage, error);
	if (ret)
		return ret;

	*engine = ice_parse_engine(ad, flow, &pf->rss_parser_list,
			pattern, actions, meta, error);
	if (*engine != NULL)
		return 0;

	switch (ice_pipeline_stage) {
	case ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR_ONLY:
	case ICE_FLOW_CLASSIFY_STAGE_DISTRIBUTOR:
		*engine = ice_parse_engine(ad, flow, &pf->dist_parser_list,
				pattern, actions, meta, error);
		break;
	case ICE_FLOW_CLASSIFY_STAGE_PERMISSION:
		*engine = ice_parse_engine(ad, flow, &pf->perm_parser_list,
				pattern, actions, meta, error);
		break;
	default:
		return -EINVAL;
	}

	if (*engine == NULL)
		return -EINVAL;

	return 0;
}

static int
ice_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	void *meta;
	struct ice_flow_engine *engine;

	return ice_flow_process_filter(dev, NULL, attr, pattern, actions,
			&engine, &meta, ice_parse_engine_validate, error);
}

static struct rte_flow *
ice_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *flow = NULL;
	int ret;
	struct ice_flow_engine *engine = NULL;
	void *meta;

	flow = rte_zmalloc("ice_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	ret = ice_flow_process_filter(dev, flow, attr, pattern, actions,
			&engine, &meta, ice_parse_engine_create, error);
	if (ret < 0)
		goto free_flow;
	flow->engine = engine;
	TAILQ_INSERT_TAIL(&pf->flow_list, flow, node);
	PMD_DRV_LOG(INFO, "Succeeded to create (%d) flow", engine->type);
	return flow;

free_flow:
	PMD_DRV_LOG(ERR, "Failed to create flow");
	rte_free(flow);
	return NULL;
}

static int
ice_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int ret = 0;

	if (!flow || !flow->engine || !flow->engine->destroy) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Invalid flow");
		return -rte_errno;
	}

	ret = flow->engine->destroy(ad, flow, error);

	if (!ret) {
		TAILQ_REMOVE(&pf->flow_list, flow, node);
		rte_free(flow);
	} else {
		PMD_DRV_LOG(ERR, "Failed to destroy flow");
	}

	return ret;
}

static int
ice_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *p_flow;
	void *temp;
	int ret = 0;

	TAILQ_FOREACH_SAFE(p_flow, &pf->flow_list, node, temp) {
		ret = ice_flow_destroy(dev, p_flow, error);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to flush flows");
			return -EINVAL;
		}
	}

	return ret;
}

static int
ice_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	int ret = -EINVAL;
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_flow_query_count *count = data;

	if (!flow || !flow->engine || !flow->engine->query_count) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Invalid flow");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow->engine->query_count(ad, flow, count, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"action not supported");
		}
	}
	return ret;
}
