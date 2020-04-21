/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_FLOW_H_
#define RTE_PMD_MLX5_FLOW_H_

#include <netinet/in.h>
#include <sys/queue.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_atomic.h>
#include <rte_alarm.h>
#include <rte_mtr.h>

#include <mlx5_prm.h>

#include "mlx5.h"

/* Private rte flow items. */
enum mlx5_rte_flow_item_type {
	MLX5_RTE_FLOW_ITEM_TYPE_END = INT_MIN,
	MLX5_RTE_FLOW_ITEM_TYPE_TAG,
	MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE,
	MLX5_RTE_FLOW_ITEM_TYPE_VLAN,
};

/* Private (internal) rte flow actions. */
enum mlx5_rte_flow_action_type {
	MLX5_RTE_FLOW_ACTION_TYPE_END = INT_MIN,
	MLX5_RTE_FLOW_ACTION_TYPE_TAG,
	MLX5_RTE_FLOW_ACTION_TYPE_MARK,
	MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
};

/* Matches on selected register. */
struct mlx5_rte_flow_item_tag {
	enum modify_reg id;
	uint32_t data;
};

/* Modify selected register. */
struct mlx5_rte_flow_action_set_tag {
	enum modify_reg id;
	uint32_t data;
};

struct mlx5_flow_action_copy_mreg {
	enum modify_reg dst;
	enum modify_reg src;
};

/* Matches on source queue. */
struct mlx5_rte_flow_item_tx_queue {
	uint32_t queue;
};

/* Feature name to allocate metadata register. */
enum mlx5_feature_name {
	MLX5_HAIRPIN_RX,
	MLX5_HAIRPIN_TX,
	MLX5_METADATA_RX,
	MLX5_METADATA_TX,
	MLX5_METADATA_FDB,
	MLX5_FLOW_MARK,
	MLX5_APP_TAG,
	MLX5_COPY_MARK,
	MLX5_MTR_COLOR,
	MLX5_MTR_SFX,
};

/* Pattern outer Layer bits. */
#define MLX5_FLOW_LAYER_OUTER_L2 (1u << 0)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV4 (1u << 1)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV6 (1u << 2)
#define MLX5_FLOW_LAYER_OUTER_L4_UDP (1u << 3)
#define MLX5_FLOW_LAYER_OUTER_L4_TCP (1u << 4)
#define MLX5_FLOW_LAYER_OUTER_VLAN (1u << 5)

/* Pattern inner Layer bits. */
#define MLX5_FLOW_LAYER_INNER_L2 (1u << 6)
#define MLX5_FLOW_LAYER_INNER_L3_IPV4 (1u << 7)
#define MLX5_FLOW_LAYER_INNER_L3_IPV6 (1u << 8)
#define MLX5_FLOW_LAYER_INNER_L4_UDP (1u << 9)
#define MLX5_FLOW_LAYER_INNER_L4_TCP (1u << 10)
#define MLX5_FLOW_LAYER_INNER_VLAN (1u << 11)

/* Pattern tunnel Layer bits. */
#define MLX5_FLOW_LAYER_VXLAN (1u << 12)
#define MLX5_FLOW_LAYER_VXLAN_GPE (1u << 13)
#define MLX5_FLOW_LAYER_GRE (1u << 14)
#define MLX5_FLOW_LAYER_MPLS (1u << 15)
/* List of tunnel Layer bits continued below. */

/* General pattern items bits. */
#define MLX5_FLOW_ITEM_METADATA (1u << 16)
#define MLX5_FLOW_ITEM_PORT_ID (1u << 17)
#define MLX5_FLOW_ITEM_TAG (1u << 18)
#define MLX5_FLOW_ITEM_MARK (1u << 19)

/* Pattern MISC bits. */
#define MLX5_FLOW_LAYER_ICMP (1u << 20)
#define MLX5_FLOW_LAYER_ICMP6 (1u << 21)
#define MLX5_FLOW_LAYER_GRE_KEY (1u << 22)

/* Pattern tunnel Layer bits (continued). */
#define MLX5_FLOW_LAYER_IPIP (1u << 23)
#define MLX5_FLOW_LAYER_IPV6_ENCAP (1u << 24)
#define MLX5_FLOW_LAYER_NVGRE (1u << 25)
#define MLX5_FLOW_LAYER_GENEVE (1u << 26)

/* Queue items. */
#define MLX5_FLOW_ITEM_TX_QUEUE (1u << 27)

/* Pattern tunnel Layer bits (continued). */
#define MLX5_FLOW_LAYER_GTP (1u << 28)

/* Outer Masks. */
#define MLX5_FLOW_LAYER_OUTER_L3 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV4 | MLX5_FLOW_LAYER_OUTER_L3_IPV6)
#define MLX5_FLOW_LAYER_OUTER_L4 \
	(MLX5_FLOW_LAYER_OUTER_L4_UDP | MLX5_FLOW_LAYER_OUTER_L4_TCP)
#define MLX5_FLOW_LAYER_OUTER \
	(MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_OUTER_L3 | \
	 MLX5_FLOW_LAYER_OUTER_L4)

/* Tunnel Masks. */
#define MLX5_FLOW_LAYER_TUNNEL \
	(MLX5_FLOW_LAYER_VXLAN | MLX5_FLOW_LAYER_VXLAN_GPE | \
	 MLX5_FLOW_LAYER_GRE | MLX5_FLOW_LAYER_NVGRE | MLX5_FLOW_LAYER_MPLS | \
	 MLX5_FLOW_LAYER_IPIP | MLX5_FLOW_LAYER_IPV6_ENCAP | \
	 MLX5_FLOW_LAYER_GENEVE | MLX5_FLOW_LAYER_GTP)

/* Inner Masks. */
#define MLX5_FLOW_LAYER_INNER_L3 \
	(MLX5_FLOW_LAYER_INNER_L3_IPV4 | MLX5_FLOW_LAYER_INNER_L3_IPV6)
#define MLX5_FLOW_LAYER_INNER_L4 \
	(MLX5_FLOW_LAYER_INNER_L4_UDP | MLX5_FLOW_LAYER_INNER_L4_TCP)
#define MLX5_FLOW_LAYER_INNER \
	(MLX5_FLOW_LAYER_INNER_L2 | MLX5_FLOW_LAYER_INNER_L3 | \
	 MLX5_FLOW_LAYER_INNER_L4)

/* Layer Masks. */
#define MLX5_FLOW_LAYER_L2 \
	(MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_INNER_L2)
#define MLX5_FLOW_LAYER_L3_IPV4 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV4 | MLX5_FLOW_LAYER_INNER_L3_IPV4)
#define MLX5_FLOW_LAYER_L3_IPV6 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV6 | MLX5_FLOW_LAYER_INNER_L3_IPV6)
#define MLX5_FLOW_LAYER_L3 \
	(MLX5_FLOW_LAYER_L3_IPV4 | MLX5_FLOW_LAYER_L3_IPV6)
#define MLX5_FLOW_LAYER_L4 \
	(MLX5_FLOW_LAYER_OUTER_L4 | MLX5_FLOW_LAYER_INNER_L4)

/* Actions */
#define MLX5_FLOW_ACTION_DROP (1u << 0)
#define MLX5_FLOW_ACTION_QUEUE (1u << 1)
#define MLX5_FLOW_ACTION_RSS (1u << 2)
#define MLX5_FLOW_ACTION_FLAG (1u << 3)
#define MLX5_FLOW_ACTION_MARK (1u << 4)
#define MLX5_FLOW_ACTION_COUNT (1u << 5)
#define MLX5_FLOW_ACTION_PORT_ID (1u << 6)
#define MLX5_FLOW_ACTION_OF_POP_VLAN (1u << 7)
#define MLX5_FLOW_ACTION_OF_PUSH_VLAN (1u << 8)
#define MLX5_FLOW_ACTION_OF_SET_VLAN_VID (1u << 9)
#define MLX5_FLOW_ACTION_OF_SET_VLAN_PCP (1u << 10)
#define MLX5_FLOW_ACTION_SET_IPV4_SRC (1u << 11)
#define MLX5_FLOW_ACTION_SET_IPV4_DST (1u << 12)
#define MLX5_FLOW_ACTION_SET_IPV6_SRC (1u << 13)
#define MLX5_FLOW_ACTION_SET_IPV6_DST (1u << 14)
#define MLX5_FLOW_ACTION_SET_TP_SRC (1u << 15)
#define MLX5_FLOW_ACTION_SET_TP_DST (1u << 16)
#define MLX5_FLOW_ACTION_JUMP (1u << 17)
#define MLX5_FLOW_ACTION_SET_TTL (1u << 18)
#define MLX5_FLOW_ACTION_DEC_TTL (1u << 19)
#define MLX5_FLOW_ACTION_SET_MAC_SRC (1u << 20)
#define MLX5_FLOW_ACTION_SET_MAC_DST (1u << 21)
#define MLX5_FLOW_ACTION_ENCAP (1u << 22)
#define MLX5_FLOW_ACTION_DECAP (1u << 23)
#define MLX5_FLOW_ACTION_INC_TCP_SEQ (1u << 24)
#define MLX5_FLOW_ACTION_DEC_TCP_SEQ (1u << 25)
#define MLX5_FLOW_ACTION_INC_TCP_ACK (1u << 26)
#define MLX5_FLOW_ACTION_DEC_TCP_ACK (1u << 27)
#define MLX5_FLOW_ACTION_SET_TAG (1ull << 28)
#define MLX5_FLOW_ACTION_MARK_EXT (1ull << 29)
#define MLX5_FLOW_ACTION_SET_META (1ull << 30)
#define MLX5_FLOW_ACTION_METER (1ull << 31)
#define MLX5_FLOW_ACTION_SET_IPV4_DSCP (1ull << 32)
#define MLX5_FLOW_ACTION_SET_IPV6_DSCP (1ull << 33)

#define MLX5_FLOW_FATE_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_QUEUE | \
	 MLX5_FLOW_ACTION_RSS | MLX5_FLOW_ACTION_JUMP)

#define MLX5_FLOW_FATE_ESWITCH_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_PORT_ID | \
	 MLX5_FLOW_ACTION_JUMP)


#define MLX5_FLOW_MODIFY_HDR_ACTIONS (MLX5_FLOW_ACTION_SET_IPV4_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV4_DST | \
				      MLX5_FLOW_ACTION_SET_IPV6_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV6_DST | \
				      MLX5_FLOW_ACTION_SET_TP_SRC | \
				      MLX5_FLOW_ACTION_SET_TP_DST | \
				      MLX5_FLOW_ACTION_SET_TTL | \
				      MLX5_FLOW_ACTION_DEC_TTL | \
				      MLX5_FLOW_ACTION_SET_MAC_SRC | \
				      MLX5_FLOW_ACTION_SET_MAC_DST | \
				      MLX5_FLOW_ACTION_INC_TCP_SEQ | \
				      MLX5_FLOW_ACTION_DEC_TCP_SEQ | \
				      MLX5_FLOW_ACTION_INC_TCP_ACK | \
				      MLX5_FLOW_ACTION_DEC_TCP_ACK | \
				      MLX5_FLOW_ACTION_OF_SET_VLAN_VID | \
				      MLX5_FLOW_ACTION_SET_TAG | \
				      MLX5_FLOW_ACTION_MARK_EXT | \
				      MLX5_FLOW_ACTION_SET_META | \
				      MLX5_FLOW_ACTION_SET_IPV4_DSCP | \
				      MLX5_FLOW_ACTION_SET_IPV6_DSCP)

#define MLX5_FLOW_VLAN_ACTIONS (MLX5_FLOW_ACTION_OF_POP_VLAN | \
				MLX5_FLOW_ACTION_OF_PUSH_VLAN)

#define MLX5_FLOW_XCAP_ACTIONS (MLX5_FLOW_ACTION_ENCAP | MLX5_FLOW_ACTION_DECAP)

#ifndef IPPROTO_MPLS
#define IPPROTO_MPLS 137
#endif

/* UDP port number for MPLS */
#define MLX5_UDP_PORT_MPLS 6635

/* UDP port numbers for VxLAN. */
#define MLX5_UDP_PORT_VXLAN 4789
#define MLX5_UDP_PORT_VXLAN_GPE 4790

/* UDP port numbers for GENEVE. */
#define MLX5_UDP_PORT_GENEVE 6081

/* Priority reserved for default flows. */
#define MLX5_FLOW_PRIO_RSVD ((uint32_t)-1)

/*
 * Number of sub priorities.
 * For each kind of pattern matching i.e. L2, L3, L4 to have a correct
 * matching on the NIC (firmware dependent) L4 most have the higher priority
 * followed by L3 and ending with L2.
 */
#define MLX5_PRIORITY_MAP_L2 2
#define MLX5_PRIORITY_MAP_L3 1
#define MLX5_PRIORITY_MAP_L4 0
#define MLX5_PRIORITY_MAP_MAX 3

/* Valid layer type for IPV4 RSS. */
#define MLX5_IPV4_LAYER_TYPES \
	(ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 | \
	 ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP | \
	 ETH_RSS_NONFRAG_IPV4_OTHER)

/* IBV hash source bits  for IPV4. */
#define MLX5_IPV4_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4)

/* Valid layer type for IPV6 RSS. */
#define MLX5_IPV6_LAYER_TYPES \
	(ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 | ETH_RSS_NONFRAG_IPV6_TCP | \
	 ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_IPV6_EX  | ETH_RSS_IPV6_TCP_EX | \
	 ETH_RSS_IPV6_UDP_EX | ETH_RSS_NONFRAG_IPV6_OTHER)

/* IBV hash source bits  for IPV6. */
#define MLX5_IPV6_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6)

/* IBV hash bits for L3 SRC. */
#define MLX5_L3_SRC_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_SRC_IPV6)

/* IBV hash bits for L3 DST. */
#define MLX5_L3_DST_IBV_RX_HASH (IBV_RX_HASH_DST_IPV4 | IBV_RX_HASH_DST_IPV6)

/* IBV hash bits for TCP. */
#define MLX5_TCP_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_TCP | \
			      IBV_RX_HASH_DST_PORT_TCP)

/* IBV hash bits for UDP. */
#define MLX5_UDP_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_UDP | \
			      IBV_RX_HASH_DST_PORT_UDP)

/* IBV hash bits for L4 SRC. */
#define MLX5_L4_SRC_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_TCP | \
				 IBV_RX_HASH_SRC_PORT_UDP)

/* IBV hash bits for L4 DST. */
#define MLX5_L4_DST_IBV_RX_HASH (IBV_RX_HASH_DST_PORT_TCP | \
				 IBV_RX_HASH_DST_PORT_UDP)

/* Geneve header first 16Bit */
#define MLX5_GENEVE_VER_MASK 0x3
#define MLX5_GENEVE_VER_SHIFT 14
#define MLX5_GENEVE_VER_VAL(a) \
		(((a) >> (MLX5_GENEVE_VER_SHIFT)) & (MLX5_GENEVE_VER_MASK))
#define MLX5_GENEVE_OPTLEN_MASK 0x3F
#define MLX5_GENEVE_OPTLEN_SHIFT 7
#define MLX5_GENEVE_OPTLEN_VAL(a) \
	    (((a) >> (MLX5_GENEVE_OPTLEN_SHIFT)) & (MLX5_GENEVE_OPTLEN_MASK))
#define MLX5_GENEVE_OAMF_MASK 0x1
#define MLX5_GENEVE_OAMF_SHIFT 7
#define MLX5_GENEVE_OAMF_VAL(a) \
		(((a) >> (MLX5_GENEVE_OAMF_SHIFT)) & (MLX5_GENEVE_OAMF_MASK))
#define MLX5_GENEVE_CRITO_MASK 0x1
#define MLX5_GENEVE_CRITO_SHIFT 6
#define MLX5_GENEVE_CRITO_VAL(a) \
		(((a) >> (MLX5_GENEVE_CRITO_SHIFT)) & (MLX5_GENEVE_CRITO_MASK))
#define MLX5_GENEVE_RSVD_MASK 0x3F
#define MLX5_GENEVE_RSVD_VAL(a) ((a) & (MLX5_GENEVE_RSVD_MASK))
/*
 * The length of the Geneve options fields, expressed in four byte multiples,
 * not including the eight byte fixed tunnel.
 */
#define MLX5_GENEVE_OPT_LEN_0 14
#define MLX5_GENEVE_OPT_LEN_1 63

#define MLX5_ENCAPSULATION_DECISION_SIZE (sizeof(struct rte_flow_item_eth) + \
					  sizeof(struct rte_flow_item_ipv4))

enum mlx5_flow_drv_type {
	MLX5_FLOW_TYPE_MIN,
	MLX5_FLOW_TYPE_DV,
	MLX5_FLOW_TYPE_VERBS,
	MLX5_FLOW_TYPE_MAX,
};

/* Matcher PRM representation */
struct mlx5_flow_dv_match_params {
	size_t size;
	/**< Size of match value. Do NOT split size and key! */
	uint32_t buf[MLX5_ST_SZ_DW(fte_match_param)];
	/**< Matcher value. This value is used as the mask or as a key. */
};

/* Matcher structure. */
struct mlx5_flow_dv_matcher {
	LIST_ENTRY(mlx5_flow_dv_matcher) next;
	/**< Pointer to the next element. */
	struct mlx5_flow_tbl_resource *tbl;
	/**< Pointer to the table(group) the matcher associated with. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *matcher_object; /**< Pointer to DV matcher */
	uint16_t crc; /**< CRC of key. */
	uint16_t priority; /**< Priority of matcher. */
	struct mlx5_flow_dv_match_params mask; /**< Matcher mask. */
};

#define MLX5_ENCAP_MAX_LEN 132

/* Encap/decap resource structure. */
struct mlx5_flow_dv_encap_decap_resource {
	LIST_ENTRY(mlx5_flow_dv_encap_decap_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *verbs_action;
	/**< Verbs encap/decap action object. */
	uint8_t buf[MLX5_ENCAP_MAX_LEN];
	size_t size;
	uint8_t reformat_type;
	uint8_t ft_type;
	uint64_t flags; /**< Flags for RDMA API. */
};

/* Tag resource structure. */
struct mlx5_flow_dv_tag_resource {
	struct mlx5_hlist_entry entry;
	/**< hash list entry for tag resource, tag value as the key. */
	void *action;
	/**< Verbs tag action object. */
	rte_atomic32_t refcnt; /**< Reference counter. */
};

/*
 * Number of modification commands.
 * If extensive metadata registers are supported, the maximal actions amount is
 * 16 and 8 otherwise on root table. The validation could also be done in the
 * lower driver layer.
 * On non-root table, there is no limitation, but 32 is enough right now.
 */
#define MLX5_MAX_MODIFY_NUM			32
#define MLX5_ROOT_TBL_MODIFY_NUM		16
#define MLX5_ROOT_TBL_MODIFY_NUM_NO_MREG	8

/* Modify resource structure */
struct mlx5_flow_dv_modify_hdr_resource {
	LIST_ENTRY(mlx5_flow_dv_modify_hdr_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	struct ibv_flow_action *verbs_action;
	/**< Verbs modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint32_t actions_num; /**< Number of modification actions. */
	uint64_t flags; /**< Flags for RDMA API. */
	struct mlx5_modification_cmd actions[];
	/**< Modification actions. */
};

/* Jump action resource structure. */
struct mlx5_flow_dv_jump_tbl_resource {
	rte_atomic32_t refcnt; /**< Reference counter. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	void *action; /**< Pointer to the rdma core action. */
};

/* Port ID resource structure. */
struct mlx5_flow_dv_port_id_action_resource {
	LIST_ENTRY(mlx5_flow_dv_port_id_action_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Verbs tag action object. */
	uint32_t port_id; /**< Port ID value. */
};

/* Push VLAN action resource structure */
struct mlx5_flow_dv_push_vlan_action_resource {
	LIST_ENTRY(mlx5_flow_dv_push_vlan_action_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action; /**< Direct verbs action object. */
	uint8_t ft_type; /**< Flow table type, Rx, Tx or FDB. */
	rte_be32_t vlan_tag; /**< VLAN tag value. */
};

/* Metadata register copy table entry. */
struct mlx5_flow_mreg_copy_resource {
	/*
	 * Hash list entry for copy table.
	 *  - Key is 32/64-bit MARK action ID.
	 *  - MUST be the first entry.
	 */
	struct mlx5_hlist_entry hlist_ent;
	LIST_ENTRY(mlx5_flow_mreg_copy_resource) next;
	/* List entry for device flows. */
	uint32_t refcnt; /* Reference counter. */
	uint32_t appcnt; /* Apply/Remove counter. */
	struct rte_flow *flow; /* Built flow for copy. */
};

/* Table data structure of the hash organization. */
struct mlx5_flow_tbl_data_entry {
	struct mlx5_hlist_entry entry;
	/**< hash list entry, 64-bits key inside. */
	struct mlx5_flow_tbl_resource tbl;
	/**< flow table resource. */
	LIST_HEAD(matchers, mlx5_flow_dv_matcher) matchers;
	/**< matchers' header associated with the flow table. */
	struct mlx5_flow_dv_jump_tbl_resource jump;
	/**< jump resource, at most one for each table created. */
};

/*
 * Max number of actions per DV flow.
 * See CREATE_FLOW_MAX_FLOW_ACTIONS_SUPPORTED
 * In rdma-core file providers/mlx5/verbs.c
 */
#define MLX5_DV_MAX_NUMBER_OF_ACTIONS 8

/* DV flows structure. */
struct mlx5_flow_dv {
	struct mlx5_hrxq *hrxq; /**< Hash Rx queues. */
	/* Flow DV api: */
	struct mlx5_flow_dv_matcher *matcher; /**< Cache to matcher. */
	struct mlx5_flow_dv_match_params value;
	/**< Holds the value that the packet is compared to. */
	struct mlx5_flow_dv_encap_decap_resource *encap_decap;
	/**< Pointer to encap/decap resource in cache. */
	struct mlx5_flow_dv_modify_hdr_resource *modify_hdr;
	/**< Pointer to modify header resource in cache. */
	struct ibv_flow *flow; /**< Installed flow. */
	struct mlx5_flow_dv_jump_tbl_resource *jump;
	/**< Pointer to the jump action resource. */
	struct mlx5_flow_dv_port_id_action_resource *port_id_action;
	/**< Pointer to port ID action resource. */
	struct mlx5_vf_vlan vf_vlan;
	/**< Structure for VF VLAN workaround. */
	struct mlx5_flow_dv_push_vlan_action_resource *push_vlan_res;
	/**< Pointer to push VLAN action resource in cache. */
	struct mlx5_flow_dv_tag_resource *tag_resource;
	/**< pointer to the tag action. */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	void *actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS];
	/**< Action list. */
#endif
	int actions_n; /**< number of actions. */
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
};

/** Handles information leading to a drop fate. */
struct mlx5_flow_verbs {
	LIST_ENTRY(mlx5_flow_verbs) next;
	unsigned int size; /**< Size of the attribute. */
	struct {
		struct ibv_flow_attr *attr;
		/**< Pointer to the Specification buffer. */
		uint8_t *specs; /**< Pointer to the specifications. */
	};
	struct ibv_flow *flow; /**< Verbs flow pointer. */
	struct mlx5_hrxq *hrxq; /**< Hash Rx queue object. */
	struct mlx5_vf_vlan vf_vlan;
	/**< Structure for VF VLAN workaround. */
};

struct mlx5_flow_rss {
	uint32_t level;
	uint32_t queue_num; /**< Number of entries in @p queue. */
	uint64_t types; /**< Specific RSS hash types (see ETH_RSS_*). */
	uint16_t (*queue)[]; /**< Destination queues to redirect traffic to. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
};

/** Device flow structure. */
struct mlx5_flow {
	LIST_ENTRY(mlx5_flow) next;
	struct rte_flow *flow; /**< Pointer to the main flow. */
	uint64_t layers;
	/**< Bit-fields of present layers, see MLX5_FLOW_LAYER_*. */
	uint64_t actions;
	/**< Bit-fields of detected actions, see MLX5_FLOW_ACTION_*. */
	uint64_t hash_fields; /**< Verbs hash Rx queue hash fields. */
	uint8_t ingress; /**< 1 if the flow is ingress. */
	uint32_t group; /**< The group index. */
	uint8_t transfer; /**< 1 if the flow is E-Switch flow. */
	union {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		struct mlx5_flow_dv dv;
#endif
		struct mlx5_flow_verbs verbs;
	};
	union {
		uint32_t qrss_id; /**< Uniqie Q/RSS suffix subflow tag. */
		uint32_t mtr_flow_id; /**< Unique meter match flow id. */
	};
	bool external; /**< true if the flow is created external to PMD. */
};

/* Flow meter state. */
#define MLX5_FLOW_METER_DISABLE 0
#define MLX5_FLOW_METER_ENABLE 1

#define MLX5_MAN_WIDTH 8
/* Modify this value if enum rte_mtr_color changes. */
#define RTE_MTR_DROPPED RTE_COLORS

/* Meter policer statistics */
struct mlx5_flow_policer_stats {
	struct mlx5_flow_counter *cnt[RTE_COLORS + 1];
	/**< Color counter, extra for drop. */
	uint64_t stats_mask;
	/**< Statistics mask for the colors. */
};

/* Meter table structure. */
struct mlx5_meter_domain_info {
	struct mlx5_flow_tbl_resource *tbl;
	/**< Meter table. */
	void *any_matcher;
	/**< Meter color not match default criteria. */
	void *color_matcher;
	/**< Meter color match criteria. */
	void *jump_actn;
	/**< Meter match action. */
	void *policer_rules[RTE_MTR_DROPPED + 1];
	/**< Meter policer for the match. */
};

/* Meter table set for TX RX FDB. */
struct mlx5_meter_domains_infos {
	uint32_t ref_cnt;
	/**< Table user count. */
	struct mlx5_meter_domain_info egress;
	/**< TX meter table. */
	struct mlx5_meter_domain_info ingress;
	/**< RX meter table. */
	struct mlx5_meter_domain_info transfer;
	/**< FDB meter table. */
	void *drop_actn;
	/**< Drop action as not matched. */
	void *count_actns[RTE_MTR_DROPPED + 1];
	/**< Counters for match and unmatched statistics. */
	uint32_t fmp[MLX5_ST_SZ_DW(flow_meter_parameters)];
	/**< Flow meter parameter. */
	size_t fmp_size;
	/**< Flow meter parameter size. */
	void *meter_action;
	/**< Flow meter action. */
};

/* Meter parameter structure. */
struct mlx5_flow_meter {
	TAILQ_ENTRY(mlx5_flow_meter) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t meter_id;
	/**< Meter id. */
	struct rte_mtr_params params;
	/**< Meter rule parameters. */
	struct mlx5_flow_meter_profile *profile;
	/**< Meter profile parameters. */
	struct rte_flow_attr attr;
	/**< Flow attributes. */
	struct mlx5_meter_domains_infos *mfts;
	/**< Flow table created for this meter. */
	struct mlx5_flow_policer_stats policer_stats;
	/**< Meter policer statistics. */
	uint32_t ref_cnt;
	/**< Use count. */
	uint32_t active_state:1;
	/**< Meter state. */
	uint32_t shared:1;
	/**< Meter shared or not. */
};

/* RFC2697 parameter structure. */
struct mlx5_flow_meter_srtcm_rfc2697_prm {
	/* green_saturation_value = cbs_mantissa * 2^cbs_exponent */
	uint32_t cbs_exponent:5;
	uint32_t cbs_mantissa:8;
	/* cir = 8G * cir_mantissa * 1/(2^cir_exponent) Bytes/Sec */
	uint32_t cir_exponent:5;
	uint32_t cir_mantissa:8;
	/* yellow _saturation_value = ebs_mantissa * 2^ebs_exponent */
	uint32_t ebs_exponent:5;
	uint32_t ebs_mantissa:8;
};

/* Flow meter profile structure. */
struct mlx5_flow_meter_profile {
	TAILQ_ENTRY(mlx5_flow_meter_profile) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t meter_profile_id; /**< Profile id. */
	struct rte_mtr_meter_profile profile; /**< Profile detail. */
	union {
		struct mlx5_flow_meter_srtcm_rfc2697_prm srtcm_prm;
		/**< srtcm_rfc2697 struct. */
	};
	uint32_t ref_cnt; /**< Use count. */
};

/* Flow structure. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	enum mlx5_flow_drv_type drv_type; /**< Driver type. */
	struct mlx5_flow_rss rss; /**< RSS context. */
	struct mlx5_flow_counter *counter; /**< Holds flow counter. */
	struct mlx5_flow_mreg_copy_resource *mreg_copy;
	/**< pointer to metadata register copy table resource. */
	struct mlx5_flow_meter *meter; /**< Holds flow meter. */
	LIST_HEAD(dev_flows, mlx5_flow) dev_flows;
	/**< Device flows that are part of the flow. */
	struct mlx5_fdir *fdir; /**< Pointer to associated FDIR if any. */
	uint32_t hairpin_flow_id; /**< The flow id used for hairpin. */
	uint32_t copy_applied:1; /**< The MARK copy Flow os applied. */
};

typedef int (*mlx5_flow_validate_t)(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item items[],
				    const struct rte_flow_action actions[],
				    bool external,
				    struct rte_flow_error *error);
typedef struct mlx5_flow *(*mlx5_flow_prepare_t)
	(const struct rte_flow_attr *attr, const struct rte_flow_item items[],
	 const struct rte_flow_action actions[], struct rte_flow_error *error);
typedef int (*mlx5_flow_translate_t)(struct rte_eth_dev *dev,
				     struct mlx5_flow *dev_flow,
				     const struct rte_flow_attr *attr,
				     const struct rte_flow_item items[],
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error);
typedef int (*mlx5_flow_apply_t)(struct rte_eth_dev *dev, struct rte_flow *flow,
				 struct rte_flow_error *error);
typedef void (*mlx5_flow_remove_t)(struct rte_eth_dev *dev,
				   struct rte_flow *flow);
typedef void (*mlx5_flow_destroy_t)(struct rte_eth_dev *dev,
				    struct rte_flow *flow);
typedef int (*mlx5_flow_query_t)(struct rte_eth_dev *dev,
				 struct rte_flow *flow,
				 const struct rte_flow_action *actions,
				 void *data,
				 struct rte_flow_error *error);
typedef struct mlx5_meter_domains_infos *(*mlx5_flow_create_mtr_tbls_t)
					    (struct rte_eth_dev *dev,
					     const struct mlx5_flow_meter *fm);
typedef int (*mlx5_flow_destroy_mtr_tbls_t)(struct rte_eth_dev *dev,
					struct mlx5_meter_domains_infos *tbls);
typedef int (*mlx5_flow_create_policer_rules_t)
					(struct rte_eth_dev *dev,
					 struct mlx5_flow_meter *fm,
					 const struct rte_flow_attr *attr);
typedef int (*mlx5_flow_destroy_policer_rules_t)
					(struct rte_eth_dev *dev,
					 const struct mlx5_flow_meter *fm,
					 const struct rte_flow_attr *attr);
typedef struct mlx5_flow_counter * (*mlx5_flow_counter_alloc_t)
				   (struct rte_eth_dev *dev);
typedef void (*mlx5_flow_counter_free_t)(struct rte_eth_dev *dev,
					 struct mlx5_flow_counter *cnt);
typedef int (*mlx5_flow_counter_query_t)(struct rte_eth_dev *dev,
					 struct mlx5_flow_counter *cnt,
					 bool clear, uint64_t *pkts,
					 uint64_t *bytes);
struct mlx5_flow_driver_ops {
	mlx5_flow_validate_t validate;
	mlx5_flow_prepare_t prepare;
	mlx5_flow_translate_t translate;
	mlx5_flow_apply_t apply;
	mlx5_flow_remove_t remove;
	mlx5_flow_destroy_t destroy;
	mlx5_flow_query_t query;
	mlx5_flow_create_mtr_tbls_t create_mtr_tbls;
	mlx5_flow_destroy_mtr_tbls_t destroy_mtr_tbls;
	mlx5_flow_create_policer_rules_t create_policer_rules;
	mlx5_flow_destroy_policer_rules_t destroy_policer_rules;
	mlx5_flow_counter_alloc_t counter_alloc;
	mlx5_flow_counter_free_t counter_free;
	mlx5_flow_counter_query_t counter_query;
};


#define MLX5_CNT_CONTAINER(sh, batch, thread) (&(sh)->cmng.ccont \
	[(((sh)->cmng.mhi[batch] >> (thread)) & 0x1) * 2 + (batch)])
#define MLX5_CNT_CONTAINER_UNUSED(sh, batch, thread) (&(sh)->cmng.ccont \
	[(~((sh)->cmng.mhi[batch] >> (thread)) & 0x1) * 2 + (batch)])

/* mlx5_flow.c */

struct mlx5_flow_id_pool *mlx5_flow_id_pool_alloc(uint32_t max_id);
void mlx5_flow_id_pool_release(struct mlx5_flow_id_pool *pool);
uint32_t mlx5_flow_id_get(struct mlx5_flow_id_pool *pool, uint32_t *id);
uint32_t mlx5_flow_id_release(struct mlx5_flow_id_pool *pool,
			      uint32_t id);
int mlx5_flow_group_to_table(const struct rte_flow_attr *attributes,
			     bool external, uint32_t group, bool fdb_def_rule,
			     uint32_t *table, struct rte_flow_error *error);
uint64_t mlx5_flow_hashfields_adjust(struct mlx5_flow *dev_flow, int tunnel,
				     uint64_t layer_types,
				     uint64_t hash_fields);
uint32_t mlx5_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
				   uint32_t subpriority);
int mlx5_flow_get_reg_id(struct rte_eth_dev *dev,
				     enum mlx5_feature_name feature,
				     uint32_t id,
				     struct rte_flow_error *error);
const struct rte_flow_action *mlx5_flow_find_action
					(const struct rte_flow_action *actions,
					 enum rte_flow_action_type action);
int mlx5_flow_validate_action_count(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    struct rte_flow_error *error);
int mlx5_flow_validate_action_drop(uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_flag(uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_mark(const struct rte_flow_action *action,
				   uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_queue(const struct rte_flow_action *action,
				    uint64_t action_flags,
				    struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    struct rte_flow_error *error);
int mlx5_flow_validate_action_rss(const struct rte_flow_action *action,
				  uint64_t action_flags,
				  struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attr,
				  uint64_t item_flags,
				  struct rte_flow_error *error);
int mlx5_flow_validate_attributes(struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attributes,
				  struct rte_flow_error *error);
int mlx5_flow_item_acceptable(const struct rte_flow_item *item,
			      const uint8_t *mask,
			      const uint8_t *nic_mask,
			      unsigned int size,
			      struct rte_flow_error *error);
int mlx5_flow_validate_item_eth(const struct rte_flow_item *item,
				uint64_t item_flags,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_gre(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_gre_key(const struct rte_flow_item *item,
				    uint64_t item_flags,
				    const struct rte_flow_item *gre_item,
				    struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv4(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t last_item,
				 uint16_t ether_type,
				 const struct rte_flow_item_ipv4 *acc_mask,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv6(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t last_item,
				 uint16_t ether_type,
				 const struct rte_flow_item_ipv6 *acc_mask,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_mpls(struct rte_eth_dev *dev,
				 const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t prev_layer,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_tcp(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				const struct rte_flow_item_tcp *flow_mask,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_udp(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_vlan(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 struct rte_eth_dev *dev,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan_gpe(const struct rte_flow_item *item,
				      uint64_t item_flags,
				      struct rte_eth_dev *dev,
				      struct rte_flow_error *error);
int mlx5_flow_validate_item_icmp(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint8_t target_protocol,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_icmp6(const struct rte_flow_item *item,
				   uint64_t item_flags,
				   uint8_t target_protocol,
				   struct rte_flow_error *error);
int mlx5_flow_validate_item_nvgre(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  uint8_t target_protocol,
				  struct rte_flow_error *error);
int mlx5_flow_validate_item_geneve(const struct rte_flow_item *item,
				   uint64_t item_flags,
				   struct rte_eth_dev *dev,
				   struct rte_flow_error *error);
struct mlx5_meter_domains_infos *mlx5_flow_create_mtr_tbls
					(struct rte_eth_dev *dev,
					 const struct mlx5_flow_meter *fm);
int mlx5_flow_destroy_mtr_tbls(struct rte_eth_dev *dev,
			       struct mlx5_meter_domains_infos *tbl);
int mlx5_flow_create_policer_rules(struct rte_eth_dev *dev,
				   struct mlx5_flow_meter *fm,
				   const struct rte_flow_attr *attr);
int mlx5_flow_destroy_policer_rules(struct rte_eth_dev *dev,
				    struct mlx5_flow_meter *fm,
				    const struct rte_flow_attr *attr);
int mlx5_flow_meter_flush(struct rte_eth_dev *dev,
			  struct rte_mtr_error *error);
#endif /* RTE_PMD_MLX5_FLOW_H_ */
