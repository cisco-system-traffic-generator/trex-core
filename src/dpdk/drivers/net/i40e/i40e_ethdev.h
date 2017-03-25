/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _I40E_ETHDEV_H_
#define _I40E_ETHDEV_H_

#include <rte_eth_ctrl.h>
#include <rte_time.h>
#include <rte_kvargs.h>
#include <rte_hash.h>
#include <rte_flow_driver.h>

#define I40E_VLAN_TAG_SIZE        4

#define I40E_AQ_LEN               32
#define I40E_AQ_BUF_SZ            4096
/* Number of queues per TC should be one of 1, 2, 4, 8, 16, 32, 64 */
#define I40E_MAX_Q_PER_TC         64
#define I40E_NUM_DESC_DEFAULT     512
#define I40E_NUM_DESC_ALIGN       32
#define I40E_BUF_SIZE_MIN         1024
#define I40E_FRAME_SIZE_MAX       9728
#define I40E_QUEUE_BASE_ADDR_UNIT 128
/* number of VSIs and queue default setting */
#define I40E_MAX_QP_NUM_PER_VF    16
#define I40E_DEFAULT_QP_NUM_FDIR  1
#define I40E_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define I40E_VFTA_SIZE            (4096 / I40E_UINT32_BIT_SIZE)
/* Maximun number of MAC addresses */
#define I40E_NUM_MACADDR_MAX       64
/* Maximum number of VFs */
#define I40E_MAX_VF               128

/*
 * vlan_id is a 12 bit number.
 * The VFTA array is actually a 4096 bit array, 128 of 32bit elements.
 * 2^5 = 32. The val of lower 5 bits specifies the bit in the 32bit element.
 * The higher 7 bit val specifies VFTA array index.
 */
#define I40E_VFTA_BIT(vlan_id)    (1 << ((vlan_id) & 0x1F))
#define I40E_VFTA_IDX(vlan_id)    ((vlan_id) >> 5)

/* Default TC traffic in case DCB is not enabled */
#define I40E_DEFAULT_TCMAP        0x1
#define I40E_FDIR_QUEUE_ID        0

/* Always assign pool 0 to main VSI, VMDQ will start from 1 */
#define I40E_VMDQ_POOL_BASE       1

#define I40E_DEFAULT_RX_FREE_THRESH  32
#define I40E_DEFAULT_RX_PTHRESH      8
#define I40E_DEFAULT_RX_HTHRESH      8
#define I40E_DEFAULT_RX_WTHRESH      0

#define I40E_DEFAULT_TX_FREE_THRESH  32
#define I40E_DEFAULT_TX_PTHRESH      32
#define I40E_DEFAULT_TX_HTHRESH      0
#define I40E_DEFAULT_TX_WTHRESH      0
#define I40E_DEFAULT_TX_RSBIT_THRESH 32

/* Bit shift and mask */
#define I40E_4_BIT_WIDTH  (CHAR_BIT / 2)
#define I40E_4_BIT_MASK   RTE_LEN2MASK(I40E_4_BIT_WIDTH, uint8_t)
#define I40E_8_BIT_WIDTH  CHAR_BIT
#define I40E_8_BIT_MASK   UINT8_MAX
#define I40E_16_BIT_WIDTH (CHAR_BIT * 2)
#define I40E_16_BIT_MASK  UINT16_MAX
#define I40E_32_BIT_WIDTH (CHAR_BIT * 4)
#define I40E_32_BIT_MASK  UINT32_MAX
#define I40E_48_BIT_WIDTH (CHAR_BIT * 6)
#define I40E_48_BIT_MASK  RTE_LEN2MASK(I40E_48_BIT_WIDTH, uint64_t)

/* Linux PF host with virtchnl version 1.1 */
#define PF_IS_V11(vf) \
	(((vf)->version_major == I40E_VIRTCHNL_VERSION_MAJOR) && \
	((vf)->version_minor == 1))

/* index flex payload per layer */
enum i40e_flxpld_layer_idx {
	I40E_FLXPLD_L2_IDX    = 0,
	I40E_FLXPLD_L3_IDX    = 1,
	I40E_FLXPLD_L4_IDX    = 2,
	I40E_MAX_FLXPLD_LAYER = 3,
};
#define I40E_MAX_FLXPLD_FIED        3  /* max number of flex payload fields */
#define I40E_FDIR_BITMASK_NUM_WORD  2  /* max number of bitmask words */
#define I40E_FDIR_MAX_FLEXWORD_NUM  8  /* max number of flexpayload words */
#define I40E_FDIR_MAX_FLEX_LEN      16 /* len in bytes of flex payload */
#define I40E_INSET_MASK_NUM_REG     2  /* number of input set mask registers */

/* i40e flags */
#define I40E_FLAG_RSS                   (1ULL << 0)
#define I40E_FLAG_DCB                   (1ULL << 1)
#define I40E_FLAG_VMDQ                  (1ULL << 2)
#define I40E_FLAG_SRIOV                 (1ULL << 3)
#define I40E_FLAG_HEADER_SPLIT_DISABLED (1ULL << 4)
#define I40E_FLAG_HEADER_SPLIT_ENABLED  (1ULL << 5)
#define I40E_FLAG_FDIR                  (1ULL << 6)
#define I40E_FLAG_VXLAN                 (1ULL << 7)
#define I40E_FLAG_RSS_AQ_CAPABLE        (1ULL << 8)
#define I40E_FLAG_VF_MAC_BY_PF          (1ULL << 9)
#define I40E_FLAG_ALL (I40E_FLAG_RSS | \
		       I40E_FLAG_DCB | \
		       I40E_FLAG_VMDQ | \
		       I40E_FLAG_SRIOV | \
		       I40E_FLAG_HEADER_SPLIT_DISABLED | \
		       I40E_FLAG_HEADER_SPLIT_ENABLED | \
		       I40E_FLAG_FDIR | \
		       I40E_FLAG_VXLAN | \
		       I40E_FLAG_RSS_AQ_CAPABLE | \
		       I40E_FLAG_VF_MAC_BY_PF)

#define I40E_RSS_OFFLOAD_ALL ( \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_NONFRAG_IPV4_OTHER | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_NONFRAG_IPV6_SCTP | \
	ETH_RSS_NONFRAG_IPV6_OTHER | \
	ETH_RSS_L2_PAYLOAD)

/* All bits of RSS hash enable for X722*/
#define I40E_RSS_HENA_ALL_X722 ( \
	(1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK) | \
	I40E_RSS_HENA_ALL)

/* All bits of RSS hash enable */
#define I40E_RSS_HENA_ALL ( \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) | \
	(1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_RX) | \
	(1ULL << I40E_FILTER_PCTYPE_FCOE_OTHER) | \
	(1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD))

#define I40E_MISC_VEC_ID                RTE_INTR_VEC_ZERO_OFFSET
#define I40E_RX_VEC_START               RTE_INTR_VEC_RXTX_OFFSET

/* Default queue interrupt throttling time in microseconds */
#define I40E_ITR_INDEX_DEFAULT          0
#define I40E_QUEUE_ITR_INTERVAL_DEFAULT 32 /* 32 us */
#define I40E_QUEUE_ITR_INTERVAL_MAX     8160 /* 8160 us */

/* Special FW support this floating VEB feature */
#define FLOATING_VEB_SUPPORTED_FW_MAJ 5
#define FLOATING_VEB_SUPPORTED_FW_MIN 0

#define I40E_GL_SWT_L2TAGCTRL(_i)             (0x001C0A70 + ((_i) * 4))
#define I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT 16
#define I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_MASK  \
	I40E_MASK(0xFFFF, I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT)

#define I40E_INSET_NONE            0x00000000000000000ULL

/* bit0 ~ bit 7 */
#define I40E_INSET_DMAC            0x0000000000000001ULL
#define I40E_INSET_SMAC            0x0000000000000002ULL
#define I40E_INSET_VLAN_OUTER      0x0000000000000004ULL
#define I40E_INSET_VLAN_INNER      0x0000000000000008ULL
#define I40E_INSET_VLAN_TUNNEL     0x0000000000000010ULL

/* bit 8 ~ bit 15 */
#define I40E_INSET_IPV4_SRC        0x0000000000000100ULL
#define I40E_INSET_IPV4_DST        0x0000000000000200ULL
#define I40E_INSET_IPV6_SRC        0x0000000000000400ULL
#define I40E_INSET_IPV6_DST        0x0000000000000800ULL
#define I40E_INSET_SRC_PORT        0x0000000000001000ULL
#define I40E_INSET_DST_PORT        0x0000000000002000ULL
#define I40E_INSET_SCTP_VT         0x0000000000004000ULL

/* bit 16 ~ bit 31 */
#define I40E_INSET_IPV4_TOS        0x0000000000010000ULL
#define I40E_INSET_IPV4_PROTO      0x0000000000020000ULL
#define I40E_INSET_IPV4_TTL        0x0000000000040000ULL
#define I40E_INSET_IPV6_TC         0x0000000000080000ULL
#define I40E_INSET_IPV6_FLOW       0x0000000000100000ULL
#define I40E_INSET_IPV6_NEXT_HDR   0x0000000000200000ULL
#define I40E_INSET_IPV6_HOP_LIMIT  0x0000000000400000ULL
#define I40E_INSET_TCP_FLAGS       0x0000000000800000ULL

/* bit 32 ~ bit 47, tunnel fields */
#define I40E_INSET_TUNNEL_IPV4_DST       0x0000000100000000ULL
#define I40E_INSET_TUNNEL_IPV6_DST       0x0000000200000000ULL
#define I40E_INSET_TUNNEL_DMAC           0x0000000400000000ULL
#define I40E_INSET_TUNNEL_SRC_PORT       0x0000000800000000ULL
#define I40E_INSET_TUNNEL_DST_PORT       0x0000001000000000ULL
#define I40E_INSET_TUNNEL_ID             0x0000002000000000ULL

/* bit 48 ~ bit 55 */
#define I40E_INSET_LAST_ETHER_TYPE 0x0001000000000000ULL

/* bit 56 ~ bit 63, Flex Payload */
#define I40E_INSET_FLEX_PAYLOAD_W1 0x0100000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W2 0x0200000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W3 0x0400000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W4 0x0800000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W5 0x1000000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W6 0x2000000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W7 0x4000000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD_W8 0x8000000000000000ULL
#define I40E_INSET_FLEX_PAYLOAD \
	(I40E_INSET_FLEX_PAYLOAD_W1 | I40E_INSET_FLEX_PAYLOAD_W2 | \
	I40E_INSET_FLEX_PAYLOAD_W3 | I40E_INSET_FLEX_PAYLOAD_W4 | \
	I40E_INSET_FLEX_PAYLOAD_W5 | I40E_INSET_FLEX_PAYLOAD_W6 | \
	I40E_INSET_FLEX_PAYLOAD_W7 | I40E_INSET_FLEX_PAYLOAD_W8)

struct i40e_adapter;

/**
 * MAC filter structure
 */
struct i40e_mac_filter_info {
	enum rte_mac_filter_type filter_type;
	struct ether_addr mac_addr;
};

TAILQ_HEAD(i40e_mac_filter_list, i40e_mac_filter);

/* MAC filter list structure */
struct i40e_mac_filter {
	TAILQ_ENTRY(i40e_mac_filter) next;
	struct i40e_mac_filter_info mac_info;
};

TAILQ_HEAD(i40e_vsi_list_head, i40e_vsi_list);

struct i40e_vsi;

/* VSI list structure */
struct i40e_vsi_list {
	TAILQ_ENTRY(i40e_vsi_list) list;
	struct i40e_vsi *vsi;
};

struct i40e_rx_queue;
struct i40e_tx_queue;

/* Bandwidth limit information */
struct i40e_bw_info {
	uint16_t bw_limit;      /* BW Limit (0 = disabled) */
	uint8_t  bw_max;        /* Max BW limit if enabled */

	/* Relative credits within same TC with respect to other VSIs or Comps */
	uint8_t  bw_ets_share_credits[I40E_MAX_TRAFFIC_CLASS];
	/* Bandwidth limit per TC */
	uint16_t bw_ets_credits[I40E_MAX_TRAFFIC_CLASS];
	/* Max bandwidth limit per TC */
	uint8_t  bw_ets_max[I40E_MAX_TRAFFIC_CLASS];
};

/* Structure that defines a VEB */
struct i40e_veb {
	struct i40e_vsi_list_head head;
	struct i40e_vsi *associate_vsi; /* Associate VSI who owns the VEB */
	struct i40e_pf *associate_pf; /* Associate PF who owns the VEB */
	uint16_t seid; /* The seid of VEB itself */
	uint16_t uplink_seid; /* The uplink seid of this VEB */
	uint16_t stats_idx;
	struct i40e_eth_stats stats;
	uint8_t enabled_tc;   /* The traffic class enabled */
	struct i40e_bw_info bw_info; /* VEB bandwidth information */
};

/* i40e MACVLAN filter structure */
struct i40e_macvlan_filter {
	struct ether_addr macaddr;
	enum rte_mac_filter_type filter_type;
	uint16_t vlan_id;
};

/*
 * Structure that defines a VSI, associated with a adapter.
 */
struct i40e_vsi {
	struct i40e_adapter *adapter; /* Backreference to associated adapter */
	struct i40e_aqc_vsi_properties_data info; /* VSI properties */

	struct i40e_eth_stats eth_stats_offset;
	struct i40e_eth_stats eth_stats;
	/*
	 * When drivers loaded, only a default main VSI exists. In case new VSI
	 * needs to add, HW needs to know the layout that VSIs are organized.
	 * Besides that, VSI isan element and can't switch packets, which needs
	 * to add new component VEB to perform switching. So, a new VSI needs
	 * to specify the the uplink VSI (Parent VSI) before created. The
	 * uplink VSI will check whether it had a VEB to switch packets. If no,
	 * it will try to create one. Then, uplink VSI will move the new VSI
	 * into its' sib_vsi_list to manage all the downlink VSI.
	 *  sib_vsi_list: the VSI list that shared the same uplink VSI.
	 *  parent_vsi  : the uplink VSI. It's NULL for main VSI.
	 *  veb         : the VEB associates with the VSI.
	 */
	struct i40e_vsi_list sib_vsi_list; /* sibling vsi list */
	struct i40e_vsi *parent_vsi;
	struct i40e_veb *veb;    /* Associated veb, could be null */
	struct i40e_veb *floating_veb; /* Associated floating veb */
	bool offset_loaded;
	enum i40e_vsi_type type; /* VSI types */
	uint16_t vlan_num;       /* Total VLAN number */
	uint16_t mac_num;        /* Total mac number */
	uint32_t vfta[I40E_VFTA_SIZE];        /* VLAN bitmap */
	struct i40e_mac_filter_list mac_list; /* macvlan filter list */
	/* specific VSI-defined parameters, SRIOV stored the vf_id */
	uint32_t user_param;
	uint16_t seid;           /* The seid of VSI itself */
	uint16_t uplink_seid;    /* The uplink seid of this VSI */
	uint16_t nb_qps;         /* Number of queue pairs VSI can occupy */
	uint16_t nb_used_qps;    /* Number of queue pairs VSI uses */
	uint16_t max_macaddrs;   /* Maximum number of MAC addresses */
	uint16_t base_queue;     /* The first queue index of this VSI */
	/*
	 * The offset to visit VSI related register, assigned by HW when
	 * creating VSI
	 */
	uint16_t vsi_id;
	uint16_t msix_intr; /* The MSIX interrupt binds to VSI */
	uint16_t nb_msix;   /* The max number of msix vector */
	uint8_t enabled_tc; /* The traffic class enabled */
	uint8_t vlan_anti_spoof_on; /* The VLAN anti-spoofing enabled */
	struct i40e_bw_info bw_info; /* VSI bandwidth information */
};

struct pool_entry {
	LIST_ENTRY(pool_entry) next;
	uint16_t base;
	uint16_t len;
};

LIST_HEAD(res_list, pool_entry);

struct i40e_res_pool_info {
	uint32_t base;              /* Resource start index */
	uint32_t num_alloc;         /* Allocated resource number */
	uint32_t num_free;          /* Total available resource number */
	struct res_list alloc_list; /* Allocated resource list */
	struct res_list free_list;  /* Available resource list */
};

enum I40E_VF_STATE {
	I40E_VF_INACTIVE = 0,
	I40E_VF_INRESET,
	I40E_VF_ININIT,
	I40E_VF_ACTIVE,
};

/*
 * Structure to store private data for PF host.
 */
struct i40e_pf_vf {
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
	enum I40E_VF_STATE state; /* The number of queue pairs availiable */
	uint16_t vf_idx; /* VF index in pf->vfs */
	uint16_t lan_nb_qps; /* Actual queues allocated */
	uint16_t reset_cnt; /* Total vf reset times */
	struct ether_addr mac_addr;  /* Default MAC address */
};

/*
 * Structure to store private data for flow control.
 */
struct i40e_fc_conf {
	uint16_t pause_time; /* Flow control pause timer */
	/* FC high water 0-7 for pfc and 8 for lfc unit:kilobytes */
	uint32_t high_water[I40E_MAX_TRAFFIC_CLASS + 1];
	/* FC low water  0-7 for pfc and 8 for lfc unit:kilobytes */
	uint32_t low_water[I40E_MAX_TRAFFIC_CLASS + 1];
};

/*
 * Structure to store private data for VMDQ instance
 */
struct i40e_vmdq_info {
	struct i40e_pf *pf;
	struct i40e_vsi *vsi;
};

/*
 * Structure to store flex pit for flow diretor.
 */
struct i40e_fdir_flex_pit {
	uint8_t src_offset;    /* offset in words from the beginning of payload */
	uint8_t size;          /* size in words */
	uint8_t dst_offset;    /* offset in words of flexible payload */
};

struct i40e_fdir_flex_mask {
	uint8_t word_mask;  /**< Bit i enables word i of flexible payload */
	struct {
		uint8_t offset;
		uint16_t mask;
	} bitmask[I40E_FDIR_BITMASK_NUM_WORD];
};

#define I40E_FILTER_PCTYPE_MAX 64
#define I40E_MAX_FDIR_FILTER_NUM (1024 * 8)

struct i40e_fdir_filter {
	TAILQ_ENTRY(i40e_fdir_filter) rules;
	struct rte_eth_fdir_filter fdir;
};

TAILQ_HEAD(i40e_fdir_filter_list, i40e_fdir_filter);
/*
 *  A structure used to define fields of a FDIR related info.
 */
struct i40e_fdir_info {
	struct i40e_vsi *fdir_vsi;     /* pointer to fdir VSI structure */
	uint16_t match_counter_index;  /* Statistic counter index used for fdir*/
	struct i40e_tx_queue *txq;
	struct i40e_rx_queue *rxq;
	void *prg_pkt;                 /* memory for fdir program packet */
	uint64_t dma_addr;             /* physic address of packet memory*/
	/* input set bits for each pctype */
	uint64_t input_set[I40E_FILTER_PCTYPE_MAX];
	/*
	 * the rule how bytes stream is extracted as flexible payload
	 * for each payload layer, the setting can up to three elements
	 */
	struct i40e_fdir_flex_pit flex_set[I40E_MAX_FLXPLD_LAYER * I40E_MAX_FLXPLD_FIED];
	struct i40e_fdir_flex_mask flex_mask[I40E_FILTER_PCTYPE_MAX];

	struct i40e_fdir_filter_list fdir_list;
	struct i40e_fdir_filter **hash_map;
	struct rte_hash *hash_table;
};

/* Ethertype filter number HW supports */
#define I40E_MAX_ETHERTYPE_FILTER_NUM 768

/* Ethertype filter struct */
struct i40e_ethertype_filter_input {
	struct ether_addr mac_addr;   /* Mac address to match */
	uint16_t ether_type;          /* Ether type to match */
};

struct i40e_ethertype_filter {
	TAILQ_ENTRY(i40e_ethertype_filter) rules;
	struct i40e_ethertype_filter_input input;
	uint16_t flags;              /* Flags from RTE_ETHTYPE_FLAGS_* */
	uint16_t queue;              /* Queue assigned to when match */
};

TAILQ_HEAD(i40e_ethertype_filter_list, i40e_ethertype_filter);

struct i40e_ethertype_rule {
	struct i40e_ethertype_filter_list ethertype_list;
	struct i40e_ethertype_filter  **hash_map;
	struct rte_hash *hash_table;
};

/* Tunnel filter number HW supports */
#define I40E_MAX_TUNNEL_FILTER_NUM 400

enum i40e_tunnel_iptype {
	I40E_TUNNEL_IPTYPE_IPV4,
	I40E_TUNNEL_IPTYPE_IPV6,
};

/* Tunnel filter struct */
struct i40e_tunnel_filter_input {
	uint8_t outer_mac[6];    /* Outer mac address to match */
	uint8_t inner_mac[6];    /* Inner mac address to match */
	uint16_t inner_vlan;     /* Inner vlan address to match */
	enum i40e_tunnel_iptype ip_type;
	uint16_t flags;          /* Filter type flag */
	uint32_t tenant_id;      /* Tenant id to match */
};

struct i40e_tunnel_filter {
	TAILQ_ENTRY(i40e_tunnel_filter) rules;
	struct i40e_tunnel_filter_input input;
	uint16_t queue; /* Queue assigned to when match */
};

TAILQ_HEAD(i40e_tunnel_filter_list, i40e_tunnel_filter);

struct i40e_tunnel_rule {
	struct i40e_tunnel_filter_list tunnel_list;
	struct i40e_tunnel_filter  **hash_map;
	struct rte_hash *hash_table;
};

#define I40E_MIRROR_MAX_ENTRIES_PER_RULE   64
#define I40E_MAX_MIRROR_RULES           64
/*
 * Mirror rule structure
 */
struct i40e_mirror_rule {
	TAILQ_ENTRY(i40e_mirror_rule) rules;
	uint8_t rule_type;
	uint16_t index;          /* the sw index of mirror rule */
	uint16_t id;             /* the rule id assigned by firmware */
	uint16_t dst_vsi_seid;   /* destination vsi for this mirror rule. */
	uint16_t num_entries;
	/* the info stores depend on the rule type.
	    If type is I40E_MIRROR_TYPE_VLAN, vlan ids are stored here.
	    If type is I40E_MIRROR_TYPE_VPORT_*, vsi's seid are stored.
	 */
	uint16_t entries[I40E_MIRROR_MAX_ENTRIES_PER_RULE];
};

TAILQ_HEAD(i40e_mirror_rule_list, i40e_mirror_rule);

/*
 * Struct to store flow created.
 */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) node;
	enum rte_filter_type filter_type;
	void *rule;
};

TAILQ_HEAD(i40e_flow_list, rte_flow);

/*
 * Structure to store private data specific for PF instance.
 */
struct i40e_pf {
	struct i40e_adapter *adapter; /* The adapter this PF associate to */
	struct i40e_vsi *main_vsi; /* pointer to main VSI structure */
	uint16_t mac_seid; /* The seid of the MAC of this PF */
	uint16_t main_vsi_seid; /* The seid of the main VSI */
	uint16_t max_num_vsi;
	struct i40e_res_pool_info qp_pool;    /*Queue pair pool */
	struct i40e_res_pool_info msix_pool;  /* MSIX interrupt pool */

	struct i40e_hw_port_stats stats_offset;
	struct i40e_hw_port_stats stats;
	bool offset_loaded;

	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
	struct ether_addr dev_addr; /* PF device mac address */
	uint64_t flags; /* PF feature flags */
	/* All kinds of queue pair setting for different VSIs */
	struct i40e_pf_vf *vfs;
	uint16_t vf_num;
	/* Each of below queue pairs should be power of 2 since it's the
	   precondition after TC configuration applied */
	uint16_t lan_nb_qp_max;
	uint16_t lan_nb_qps; /* The number of queue pairs of LAN */
	uint16_t lan_qp_offset;
	uint16_t vmdq_nb_qp_max;
	uint16_t vmdq_nb_qps; /* The number of queue pairs of VMDq */
	uint16_t vmdq_qp_offset;
	uint16_t vf_nb_qp_max;
	uint16_t vf_nb_qps; /* The number of queue pairs of VF */
	uint16_t vf_qp_offset;
	uint16_t fdir_nb_qps; /* The number of queue pairs of Flow Director */
	uint16_t fdir_qp_offset;

	uint16_t hash_lut_size; /* The size of hash lookup table */
	/* input set bits for each pctype */
	uint64_t hash_input_set[I40E_FILTER_PCTYPE_MAX];
	/* store VXLAN UDP ports */
	uint16_t vxlan_ports[I40E_MAX_PF_UDP_OFFLOAD_PORTS];
	uint16_t vxlan_bitmap; /* Vxlan bit mask */

	/* VMDQ related info */
	uint16_t max_nb_vmdq_vsi; /* Max number of VMDQ VSIs supported */
	uint16_t nb_cfg_vmdq_vsi; /* number of VMDQ VSIs configured */
	struct i40e_vmdq_info *vmdq;

	struct i40e_fdir_info fdir; /* flow director info */
	struct i40e_ethertype_rule ethertype; /* Ethertype filter rule */
	struct i40e_tunnel_rule tunnel; /* Tunnel filter rule */
	struct i40e_fc_conf fc_conf; /* Flow control conf */
	struct i40e_mirror_rule_list mirror_list;
	uint16_t nb_mirror_rule;   /* The number of mirror rules */
	bool floating_veb; /* The flag to use the floating VEB */
	/* The floating enable flag for the specific VF */
	bool floating_veb_list[I40E_MAX_VF];
	struct i40e_flow_list flow_list;
};

enum pending_msg {
	PFMSG_LINK_CHANGE = 0x1,
	PFMSG_RESET_IMPENDING = 0x2,
	PFMSG_DRIVER_CLOSE = 0x4,
};

struct i40e_vsi_vlan_pvid_info {
	uint16_t on;            /* Enable or disable pvid */
	union {
		uint16_t pvid;  /* Valid in case 'on' is set to set pvid */
		struct {
		/*  Valid in case 'on' is cleared. 'tagged' will reject tagged packets,
		 *  while 'untagged' will reject untagged packets.
		 */
			uint8_t tagged;
			uint8_t untagged;
		} reject;
	} config;
};

struct i40e_vf_rx_queues {
	uint64_t rx_dma_addr;
	uint32_t rx_ring_len;
	uint32_t buff_size;
};

struct i40e_vf_tx_queues {
	uint64_t tx_dma_addr;
	uint32_t tx_ring_len;
};

/*
 * Structure to store private data specific for VF instance.
 */
struct i40e_vf {
	struct i40e_adapter *adapter; /* The adapter this VF associate to */
	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
	uint16_t num_queue_pairs;
	uint16_t max_pkt_len; /* Maximum packet length */
	bool promisc_unicast_enabled;
	bool promisc_multicast_enabled;

	uint32_t version_major; /* Major version number */
	uint32_t version_minor; /* Minor version number */
	uint16_t promisc_flags; /* Promiscuous setting */
	uint32_t vlan[I40E_VFTA_SIZE]; /* VLAN bit map */

	/* Event from pf */
	bool dev_closed;
	bool link_up;
	enum i40e_aq_link_speed link_speed;
	bool vf_reset;
	volatile uint32_t pend_cmd; /* pending command not finished yet */
	int32_t cmd_retval; /* return value of the cmd response from PF */
	u16 pend_msg; /* flags indicates events from pf not handled yet */
	uint8_t *aq_resp; /* buffer to store the adminq response from PF */

	/* VSI info */
	struct i40e_virtchnl_vf_resource *vf_res; /* All VSIs */
	struct i40e_virtchnl_vsi_resource *vsi_res; /* LAN VSI */
	struct i40e_vsi vsi;
	uint64_t flags;
};

/*
 * Structure to store private data for each PF/VF instance.
 */
struct i40e_adapter {
	/* Common for both PF and VF */
	struct i40e_hw hw;
	struct rte_eth_dev *eth_dev;

	/* Specific for PF or VF */
	union {
		struct i40e_pf pf;
		struct i40e_vf vf;
	};

	/* For vector PMD */
	bool rx_bulk_alloc_allowed;
	bool rx_vec_allowed;
	bool tx_simple_allowed;
	bool tx_vec_allowed;

	/* For PTP */
	struct rte_timecounter systime_tc;
	struct rte_timecounter rx_tstamp_tc;
	struct rte_timecounter tx_tstamp_tc;
};

extern const struct rte_flow_ops i40e_flow_ops;

union i40e_filter_t {
	struct rte_eth_ethertype_filter ethertype_filter;
	struct rte_eth_fdir_filter fdir_filter;
	struct rte_eth_tunnel_filter_conf tunnel_filter;
};

typedef int (*parse_filter_t)(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error,
			      union i40e_filter_t *filter);
struct i40e_valid_pattern {
	enum rte_flow_item_type *items;
	parse_filter_t parse_filter;
};

int i40e_dev_switch_queues(struct i40e_pf *pf, bool on);
int i40e_vsi_release(struct i40e_vsi *vsi);
struct i40e_vsi *i40e_vsi_setup(struct i40e_pf *pf,
				enum i40e_vsi_type type,
				struct i40e_vsi *uplink_vsi,
				uint16_t user_param);
int i40e_switch_rx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on);
int i40e_switch_tx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on);
int i40e_vsi_add_vlan(struct i40e_vsi *vsi, uint16_t vlan);
int i40e_vsi_delete_vlan(struct i40e_vsi *vsi, uint16_t vlan);
int i40e_vsi_add_mac(struct i40e_vsi *vsi, struct i40e_mac_filter_info *filter);
int i40e_vsi_delete_mac(struct i40e_vsi *vsi, struct ether_addr *addr);
void i40e_update_vsi_stats(struct i40e_vsi *vsi);
void i40e_pf_disable_irq0(struct i40e_hw *hw);
void i40e_pf_enable_irq0(struct i40e_hw *hw);
int i40e_dev_link_update(struct rte_eth_dev *dev,
			 __rte_unused int wait_to_complete);
void i40e_vsi_queues_bind_intr(struct i40e_vsi *vsi);
void i40e_vsi_queues_unbind_intr(struct i40e_vsi *vsi);
int i40e_vsi_vlan_pvid_set(struct i40e_vsi *vsi,
			   struct i40e_vsi_vlan_pvid_info *info);
int i40e_vsi_config_vlan_stripping(struct i40e_vsi *vsi, bool on);
int i40e_vsi_config_vlan_filter(struct i40e_vsi *vsi, bool on);
uint64_t i40e_config_hena(uint64_t flags, enum i40e_mac_type type);
uint64_t i40e_parse_hena(uint64_t flags);
enum i40e_status_code i40e_fdir_setup_tx_resources(struct i40e_pf *pf);
enum i40e_status_code i40e_fdir_setup_rx_resources(struct i40e_pf *pf);
int i40e_fdir_setup(struct i40e_pf *pf);
const struct rte_memzone *i40e_memzone_reserve(const char *name,
					uint32_t len,
					int socket_id);
int i40e_fdir_configure(struct rte_eth_dev *dev);
void i40e_fdir_teardown(struct i40e_pf *pf);
enum i40e_filter_pctype i40e_flowtype_to_pctype(uint16_t flow_type);
uint16_t i40e_pctype_to_flowtype(enum i40e_filter_pctype pctype);
int i40e_fdir_ctrl_func(struct rte_eth_dev *dev,
			  enum rte_filter_op filter_op,
			  void *arg);
int i40e_select_filter_input_set(struct i40e_hw *hw,
				 struct rte_eth_input_set_conf *conf,
				 enum rte_filter_type filter);
void i40e_fdir_filter_restore(struct i40e_pf *pf);
int i40e_hash_filter_inset_select(struct i40e_hw *hw,
			     struct rte_eth_input_set_conf *conf);
int i40e_fdir_filter_inset_select(struct i40e_pf *pf,
			     struct rte_eth_input_set_conf *conf);
int i40e_pf_host_send_msg_to_vf(struct i40e_pf_vf *vf, uint32_t opcode,
				uint32_t retval, uint8_t *msg,
				uint16_t msglen);
void i40e_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
void i40e_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);
struct i40e_ethertype_filter *
i40e_sw_ethertype_filter_lookup(struct i40e_ethertype_rule *ethertype_rule,
			const struct i40e_ethertype_filter_input *input);
int i40e_sw_ethertype_filter_del(struct i40e_pf *pf,
				 struct i40e_ethertype_filter_input *input);
int i40e_sw_fdir_filter_del(struct i40e_pf *pf,
			    struct rte_eth_fdir_input *input);
struct i40e_tunnel_filter *
i40e_sw_tunnel_filter_lookup(struct i40e_tunnel_rule *tunnel_rule,
			     const struct i40e_tunnel_filter_input *input);
int i40e_sw_tunnel_filter_del(struct i40e_pf *pf,
			      struct i40e_tunnel_filter_input *input);
uint64_t i40e_get_default_input_set(uint16_t pctype);
int i40e_ethertype_filter_set(struct i40e_pf *pf,
			      struct rte_eth_ethertype_filter *filter,
			      bool add);
int i40e_add_del_fdir_filter(struct rte_eth_dev *dev,
			     const struct rte_eth_fdir_filter *filter,
			     bool add);
int i40e_dev_tunnel_filter_set(struct i40e_pf *pf,
			       struct rte_eth_tunnel_filter_conf *tunnel_filter,
			       uint8_t add);
int i40e_fdir_flush(struct rte_eth_dev *dev);

#define I40E_DEV_TO_PCI(eth_dev) \
	RTE_DEV_TO_PCI((eth_dev)->device)

/* I40E_DEV_PRIVATE_TO */
#define I40E_DEV_PRIVATE_TO_PF(adapter) \
	(&((struct i40e_adapter *)adapter)->pf)
#define I40E_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct i40e_adapter *)adapter)->hw)
#define I40E_DEV_PRIVATE_TO_ADAPTER(adapter) \
	((struct i40e_adapter *)adapter)

/* I40EVF_DEV_PRIVATE_TO */
#define I40EVF_DEV_PRIVATE_TO_VF(adapter) \
	(&((struct i40e_adapter *)adapter)->vf)

static inline struct i40e_vsi *
i40e_get_vsi_from_adapter(struct i40e_adapter *adapter)
{
	struct i40e_hw *hw;

        if (!adapter)
                return NULL;

	hw = I40E_DEV_PRIVATE_TO_HW(adapter);
	if (hw->mac.type == I40E_MAC_VF || hw->mac.type == I40E_MAC_X722_VF) {
		struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(adapter);
		return &vf->vsi;
	} else {
		struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(adapter);
		return pf->main_vsi;
	}
}
#define I40E_DEV_PRIVATE_TO_MAIN_VSI(adapter) \
	i40e_get_vsi_from_adapter((struct i40e_adapter *)adapter)

/* I40E_VSI_TO */
#define I40E_VSI_TO_HW(vsi) \
	(&(((struct i40e_vsi *)vsi)->adapter->hw))
#define I40E_VSI_TO_PF(vsi) \
	(&(((struct i40e_vsi *)vsi)->adapter->pf))
#define I40E_VSI_TO_VF(vsi) \
	(&(((struct i40e_vsi *)vsi)->adapter->vf))
#define I40E_VSI_TO_DEV_DATA(vsi) \
	(((struct i40e_vsi *)vsi)->adapter->pf.dev_data)
#define I40E_VSI_TO_ETH_DEV(vsi) \
	(((struct i40e_vsi *)vsi)->adapter->eth_dev)

/* I40E_PF_TO */
#define I40E_PF_TO_HW(pf) \
	(&(((struct i40e_pf *)pf)->adapter->hw))
#define I40E_PF_TO_ADAPTER(pf) \
	((struct i40e_adapter *)pf->adapter)

/* I40E_VF_TO */
#define I40E_VF_TO_HW(vf) \
	(&(((struct i40e_vf *)vf)->adapter->hw))

static inline void
i40e_init_adminq_parameter(struct i40e_hw *hw)
{
	hw->aq.num_arq_entries = I40E_AQ_LEN;
	hw->aq.num_asq_entries = I40E_AQ_LEN;
	hw->aq.arq_buf_size = I40E_AQ_BUF_SZ;
	hw->aq.asq_buf_size = I40E_AQ_BUF_SZ;
}

static inline int
i40e_align_floor(int n)
{
	if (n == 0)
		return 0;
	return 1 << (sizeof(n) * CHAR_BIT - 1 - __builtin_clz(n));
}

static inline uint16_t
i40e_calc_itr_interval(int16_t interval)
{
	if (interval < 0 || interval > I40E_QUEUE_ITR_INTERVAL_MAX)
		interval = I40E_QUEUE_ITR_INTERVAL_DEFAULT;

	/* Convert to hardware count, as writing each 1 represents 2 us */
	return interval / 2;
}

#define I40E_VALID_FLOW(flow_type) \
	((flow_type) == RTE_ETH_FLOW_FRAG_IPV4 || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_TCP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_UDP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_SCTP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_OTHER || \
	(flow_type) == RTE_ETH_FLOW_FRAG_IPV6 || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_TCP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_UDP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_SCTP || \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_OTHER || \
	(flow_type) == RTE_ETH_FLOW_L2_PAYLOAD)

#define I40E_VALID_PCTYPE_X722(pctype) \
	((pctype) == I40E_FILTER_PCTYPE_FRAG_IPV4 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_FRAG_IPV6 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_L2_PAYLOAD)

#define I40E_VALID_PCTYPE(pctype) \
	((pctype) == I40E_FILTER_PCTYPE_FRAG_IPV4 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_FRAG_IPV6 || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_UDP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_TCP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP || \
	(pctype) == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER || \
	(pctype) == I40E_FILTER_PCTYPE_L2_PAYLOAD)

#define I40E_PHY_TYPE_SUPPORT_40G(phy_type) \
	(((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_KR4) || \
	((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_CR4_CU) || \
	((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_AOC) || \
	((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_CR4) || \
	((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_SR4) || \
	((phy_type) & I40E_CAP_PHY_TYPE_40GBASE_LR4))

#define I40E_PHY_TYPE_SUPPORT_25G(phy_type) \
	(((phy_type) & I40E_CAP_PHY_TYPE_25GBASE_KR) || \
	((phy_type) & I40E_CAP_PHY_TYPE_25GBASE_CR) || \
	((phy_type) & I40E_CAP_PHY_TYPE_25GBASE_SR) || \
	((phy_type) & I40E_CAP_PHY_TYPE_25GBASE_LR))

#endif /* _I40E_ETHDEV_H_ */
