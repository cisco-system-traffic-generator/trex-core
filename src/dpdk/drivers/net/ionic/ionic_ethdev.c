/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <ethdev_pci.h>

#include "ionic_logs.h"
#include "ionic.h"
#include "ionic_dev.h"
#include "ionic_mac_api.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"
#include "ionic_rxtx.h"

static int  eth_ionic_dev_init(struct rte_eth_dev *eth_dev, void *init_params);
static int  eth_ionic_dev_uninit(struct rte_eth_dev *eth_dev);
static int  ionic_dev_info_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_dev_info *dev_info);
static int  ionic_dev_configure(struct rte_eth_dev *dev);
static int  ionic_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int  ionic_dev_start(struct rte_eth_dev *dev);
static int  ionic_dev_stop(struct rte_eth_dev *dev);
static int  ionic_dev_close(struct rte_eth_dev *dev);
static int  ionic_dev_set_link_up(struct rte_eth_dev *dev);
static int  ionic_dev_set_link_down(struct rte_eth_dev *dev);
static int  ionic_flow_ctrl_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_fc_conf *fc_conf);
static int  ionic_flow_ctrl_set(struct rte_eth_dev *eth_dev,
	struct rte_eth_fc_conf *fc_conf);
static int  ionic_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask);
static int  ionic_dev_rss_reta_update(struct rte_eth_dev *eth_dev,
	struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size);
static int  ionic_dev_rss_reta_query(struct rte_eth_dev *eth_dev,
	struct rte_eth_rss_reta_entry64 *reta_conf, uint16_t reta_size);
static int  ionic_dev_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_rss_conf *rss_conf);
static int  ionic_dev_rss_hash_update(struct rte_eth_dev *eth_dev,
	struct rte_eth_rss_conf *rss_conf);
static int  ionic_dev_stats_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_stats *stats);
static int  ionic_dev_stats_reset(struct rte_eth_dev *eth_dev);
static int  ionic_dev_xstats_get(struct rte_eth_dev *dev,
	struct rte_eth_xstat *xstats, unsigned int n);
static int  ionic_dev_xstats_get_by_id(struct rte_eth_dev *dev,
	const uint64_t *ids, uint64_t *values, unsigned int n);
static int  ionic_dev_xstats_reset(struct rte_eth_dev *dev);
static int  ionic_dev_xstats_get_names(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int size);
static int  ionic_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
	const uint64_t *ids, struct rte_eth_xstat_name *xstats_names,
	unsigned int limit);
static int  ionic_dev_fw_version_get(struct rte_eth_dev *eth_dev,
	char *fw_version, size_t fw_size);

static const struct rte_pci_id pci_id_ionic_map[] = {
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_PF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_VF) },
	{ RTE_PCI_DEVICE(IONIC_PENSANDO_VENDOR_ID, IONIC_DEV_ID_ETH_MGMT) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = IONIC_MAX_RING_DESC,
	.nb_min = IONIC_MIN_RING_DESC,
	.nb_align = 1,
};

static const struct rte_eth_desc_lim tx_desc_lim_v1 = {
	.nb_max = IONIC_MAX_RING_DESC,
	.nb_min = IONIC_MIN_RING_DESC,
	.nb_align = 1,
	.nb_seg_max = IONIC_TX_MAX_SG_ELEMS_V1 + 1,
	.nb_mtu_seg_max = IONIC_TX_MAX_SG_ELEMS_V1 + 1,
};

static const struct eth_dev_ops ionic_eth_dev_ops = {
	.dev_infos_get          = ionic_dev_info_get,
	.dev_configure          = ionic_dev_configure,
	.mtu_set                = ionic_dev_mtu_set,
	.dev_start              = ionic_dev_start,
	.dev_stop               = ionic_dev_stop,
	.dev_close              = ionic_dev_close,
	.link_update            = ionic_dev_link_update,
	.dev_set_link_up        = ionic_dev_set_link_up,
	.dev_set_link_down      = ionic_dev_set_link_down,
	.mac_addr_add           = ionic_dev_add_mac,
	.mac_addr_remove        = ionic_dev_remove_mac,
	.mac_addr_set           = ionic_dev_set_mac,
	.vlan_filter_set        = ionic_dev_vlan_filter_set,
	.promiscuous_enable     = ionic_dev_promiscuous_enable,
	.promiscuous_disable    = ionic_dev_promiscuous_disable,
	.allmulticast_enable    = ionic_dev_allmulticast_enable,
	.allmulticast_disable   = ionic_dev_allmulticast_disable,
	.flow_ctrl_get          = ionic_flow_ctrl_get,
	.flow_ctrl_set          = ionic_flow_ctrl_set,
	.rxq_info_get           = ionic_rxq_info_get,
	.txq_info_get           = ionic_txq_info_get,
	.rx_queue_setup         = ionic_dev_rx_queue_setup,
	.rx_queue_release       = ionic_dev_rx_queue_release,
	.rx_queue_start	        = ionic_dev_rx_queue_start,
	.rx_queue_stop          = ionic_dev_rx_queue_stop,
	.tx_queue_setup         = ionic_dev_tx_queue_setup,
	.tx_queue_release       = ionic_dev_tx_queue_release,
	.tx_queue_start	        = ionic_dev_tx_queue_start,
	.tx_queue_stop          = ionic_dev_tx_queue_stop,
	.vlan_offload_set       = ionic_vlan_offload_set,
	.reta_update            = ionic_dev_rss_reta_update,
	.reta_query             = ionic_dev_rss_reta_query,
	.rss_hash_conf_get      = ionic_dev_rss_hash_conf_get,
	.rss_hash_update        = ionic_dev_rss_hash_update,
	.stats_get              = ionic_dev_stats_get,
	.stats_reset            = ionic_dev_stats_reset,
	.xstats_get             = ionic_dev_xstats_get,
	.xstats_get_by_id       = ionic_dev_xstats_get_by_id,
	.xstats_reset           = ionic_dev_xstats_reset,
	.xstats_get_names       = ionic_dev_xstats_get_names,
	.xstats_get_names_by_id = ionic_dev_xstats_get_names_by_id,
	.fw_version_get         = ionic_dev_fw_version_get,
};

struct rte_ionic_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct rte_ionic_xstats_name_off rte_ionic_xstats_strings[] = {
	/* RX */
	{"rx_ucast_bytes", offsetof(struct ionic_lif_stats,
			rx_ucast_bytes)},
	{"rx_ucast_packets", offsetof(struct ionic_lif_stats,
			rx_ucast_packets)},
	{"rx_mcast_bytes", offsetof(struct ionic_lif_stats,
			rx_mcast_bytes)},
	{"rx_mcast_packets", offsetof(struct ionic_lif_stats,
			rx_mcast_packets)},
	{"rx_bcast_bytes", offsetof(struct ionic_lif_stats,
			rx_bcast_bytes)},
	{"rx_bcast_packets", offsetof(struct ionic_lif_stats,
			rx_bcast_packets)},
	/* RX drops */
	{"rx_ucast_drop_bytes", offsetof(struct ionic_lif_stats,
			rx_ucast_drop_bytes)},
	{"rx_ucast_drop_packets", offsetof(struct ionic_lif_stats,
			rx_ucast_drop_packets)},
	{"rx_mcast_drop_bytes", offsetof(struct ionic_lif_stats,
			rx_mcast_drop_bytes)},
	{"rx_mcast_drop_packets", offsetof(struct ionic_lif_stats,
			rx_mcast_drop_packets)},
	{"rx_bcast_drop_bytes", offsetof(struct ionic_lif_stats,
			rx_bcast_drop_bytes)},
	{"rx_bcast_drop_packets", offsetof(struct ionic_lif_stats,
			rx_bcast_drop_packets)},
	{"rx_dma_error", offsetof(struct ionic_lif_stats,
			rx_dma_error)},
	/* TX */
	{"tx_ucast_bytes", offsetof(struct ionic_lif_stats,
			tx_ucast_bytes)},
	{"tx_ucast_packets", offsetof(struct ionic_lif_stats,
			tx_ucast_packets)},
	{"tx_mcast_bytes", offsetof(struct ionic_lif_stats,
			tx_mcast_bytes)},
	{"tx_mcast_packets", offsetof(struct ionic_lif_stats,
			tx_mcast_packets)},
	{"tx_bcast_bytes", offsetof(struct ionic_lif_stats,
			tx_bcast_bytes)},
	{"tx_bcast_packets", offsetof(struct ionic_lif_stats,
			tx_bcast_packets)},
	/* TX drops */
	{"tx_ucast_drop_bytes", offsetof(struct ionic_lif_stats,
			tx_ucast_drop_bytes)},
	{"tx_ucast_drop_packets", offsetof(struct ionic_lif_stats,
			tx_ucast_drop_packets)},
	{"tx_mcast_drop_bytes", offsetof(struct ionic_lif_stats,
			tx_mcast_drop_bytes)},
	{"tx_mcast_drop_packets", offsetof(struct ionic_lif_stats,
			tx_mcast_drop_packets)},
	{"tx_bcast_drop_bytes", offsetof(struct ionic_lif_stats,
			tx_bcast_drop_bytes)},
	{"tx_bcast_drop_packets", offsetof(struct ionic_lif_stats,
			tx_bcast_drop_packets)},
	{"tx_dma_error", offsetof(struct ionic_lif_stats,
			tx_dma_error)},
	/* Rx Queue/Ring drops */
	{"rx_queue_disabled", offsetof(struct ionic_lif_stats,
			rx_queue_disabled)},
	{"rx_queue_empty", offsetof(struct ionic_lif_stats,
			rx_queue_empty)},
	{"rx_queue_error", offsetof(struct ionic_lif_stats,
			rx_queue_error)},
	{"rx_desc_fetch_error", offsetof(struct ionic_lif_stats,
			rx_desc_fetch_error)},
	{"rx_desc_data_error", offsetof(struct ionic_lif_stats,
			rx_desc_data_error)},
	/* Tx Queue/Ring drops */
	{"tx_queue_disabled", offsetof(struct ionic_lif_stats,
			tx_queue_disabled)},
	{"tx_queue_error", offsetof(struct ionic_lif_stats,
			tx_queue_error)},
	{"tx_desc_fetch_error", offsetof(struct ionic_lif_stats,
			tx_desc_fetch_error)},
	{"tx_desc_data_error", offsetof(struct ionic_lif_stats,
			tx_desc_data_error)},
};

#define IONIC_NB_HW_STATS RTE_DIM(rte_ionic_xstats_strings)

static int
ionic_dev_fw_version_get(struct rte_eth_dev *eth_dev,
		char *fw_version, size_t fw_size)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	int ret;

	ret = snprintf(fw_version, fw_size, "%s",
		 adapter->fw_version);
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

/*
 * Set device link up, enable tx.
 */
static int
ionic_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	int err;

	IONIC_PRINT_CALL();

	err = ionic_lif_start(lif);
	if (err)
		IONIC_PRINT(ERR, "Could not start lif to set link up");

	ionic_dev_link_update(lif->eth_dev, 0);

	return err;
}

/*
 * Set device link down, disable tx.
 */
static int
ionic_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	ionic_lif_stop(lif);

	ionic_dev_link_update(lif->eth_dev, 0);

	return 0;
}

int
ionic_dev_link_update(struct rte_eth_dev *eth_dev,
		int wait_to_complete __rte_unused)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct rte_eth_link link;

	IONIC_PRINT_CALL();

	/* Initialize */
	memset(&link, 0, sizeof(link));

	if (adapter->idev.port_info->config.an_enable) {
		link.link_autoneg = RTE_ETH_LINK_AUTONEG;
	}

	if (!adapter->link_up ||
	    !(lif->state & IONIC_LIF_F_UP)) {
		/* Interface is down */
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	} else {
		/* Interface is up */
		link.link_status = RTE_ETH_LINK_UP;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		switch (adapter->link_speed) {
		case  10000:
			link.link_speed = RTE_ETH_SPEED_NUM_10G;
			break;
		case  25000:
			link.link_speed = RTE_ETH_SPEED_NUM_25G;
			break;
		case  40000:
			link.link_speed = RTE_ETH_SPEED_NUM_40G;
			break;
		case  50000:
			link.link_speed = RTE_ETH_SPEED_NUM_50G;
			break;
		case 100000:
			link.link_speed = RTE_ETH_SPEED_NUM_100G;
			break;
		default:
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			break;
		}
	}

	return rte_eth_linkstatus_set(eth_dev, &link);
}

/**
 * Interrupt handler triggered by NIC for handling
 * specific interrupt.
 *
 * @param param
 *  The address of parameter registered before.
 *
 * @return
 *  void
 */
static void
ionic_dev_interrupt_handler(void *param)
{
	struct ionic_adapter *adapter = (struct ionic_adapter *)param;

	IONIC_PRINT(DEBUG, "->");

	if (adapter->lif)
		ionic_notifyq_handler(adapter->lif, -1);
}

static int
ionic_dev_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	int err;

	IONIC_PRINT_CALL();

	/*
	 * Note: mtu check against IONIC_MIN_MTU, IONIC_MAX_MTU
	 * is done by the API.
	 */

	err = ionic_lif_change_mtu(lif, mtu);
	if (err)
		return err;

	return 0;
}

static int
ionic_dev_info_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_dev_info *dev_info)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_identity *ident = &adapter->ident;
	union ionic_lif_config *cfg = &ident->lif.eth.config;

	IONIC_PRINT_CALL();

	dev_info->max_rx_queues = (uint16_t)
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_RXQ]);
	dev_info->max_tx_queues = (uint16_t)
		rte_le_to_cpu_32(cfg->queue_count[IONIC_QTYPE_TXQ]);

	/* Also add ETHER_CRC_LEN if the adapter is able to keep CRC */
	dev_info->min_rx_bufsize = IONIC_MIN_MTU + RTE_ETHER_HDR_LEN;
	dev_info->max_rx_pktlen = IONIC_MAX_MTU + RTE_ETHER_HDR_LEN;
	dev_info->max_mac_addrs = adapter->max_mac_addrs;
	dev_info->min_mtu = IONIC_MIN_MTU;
	dev_info->max_mtu = IONIC_MAX_MTU;

	dev_info->hash_key_size = IONIC_RSS_HASH_KEY_SIZE;
	dev_info->reta_size = rte_le_to_cpu_16(ident->lif.eth.rss_ind_tbl_sz);
	dev_info->flow_type_rss_offloads = IONIC_ETH_RSS_OFFLOAD_ALL;

	dev_info->speed_capa =
		RTE_ETH_LINK_SPEED_10G |
		RTE_ETH_LINK_SPEED_25G |
		RTE_ETH_LINK_SPEED_40G |
		RTE_ETH_LINK_SPEED_50G |
		RTE_ETH_LINK_SPEED_100G;

	/*
	 * Per-queue capabilities
	 * RTE does not support disabling a feature on a queue if it is
	 * enabled globally on the device. Thus the driver does not advertise
	 * capabilities like RTE_ETH_TX_OFFLOAD_IPV4_CKSUM as per-queue even
	 * though the driver would be otherwise capable of disabling it on
	 * a per-queue basis.
	 */

	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_queue_offload_capa = 0;

	/*
	 * Per-port capabilities
	 * See ionic_set_features to request and check supported features
	 */

	dev_info->rx_offload_capa = dev_info->rx_queue_offload_capa |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_SCATTER |
		RTE_ETH_RX_OFFLOAD_RSS_HASH |
		0;

	dev_info->tx_offload_capa = dev_info->tx_queue_offload_capa |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		0;

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim_v1;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = IONIC_DEF_TXRX_DESC;
	dev_info->default_txportconf.ring_size = IONIC_DEF_TXRX_DESC;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		/* Packets are always dropped if no desc are available */
		.rx_drop_en = 1,
	};

	return 0;
}

static int
ionic_flow_ctrl_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;

	if (idev->port_info) {
		/* Flow control autoneg not supported */
		fc_conf->autoneg = 0;

		if (idev->port_info->config.pause_type)
			fc_conf->mode = RTE_ETH_FC_FULL;
		else
			fc_conf->mode = RTE_ETH_FC_NONE;
	}

	return 0;
}

static int
ionic_flow_ctrl_set(struct rte_eth_dev *eth_dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;
	uint8_t pause_type = IONIC_PORT_PAUSE_TYPE_NONE;
	int err;

	if (fc_conf->autoneg) {
		IONIC_PRINT(WARNING, "Flow control autoneg not supported");
		return -ENOTSUP;
	}

	switch (fc_conf->mode) {
	case RTE_ETH_FC_NONE:
		pause_type = IONIC_PORT_PAUSE_TYPE_NONE;
		break;
	case RTE_ETH_FC_FULL:
		pause_type = IONIC_PORT_PAUSE_TYPE_LINK;
		break;
	case RTE_ETH_FC_RX_PAUSE:
	case RTE_ETH_FC_TX_PAUSE:
		return -ENOTSUP;
	}

	ionic_dev_cmd_port_pause(idev, pause_type);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		IONIC_PRINT(WARNING, "Failed to configure flow control");

	return err;
}

static int
ionic_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	ionic_lif_configure_vlan_offload(lif, mask);

	ionic_lif_set_features(lif);

	return 0;
}

static int
ionic_dev_rss_reta_update(struct rte_eth_dev *eth_dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_identity *ident = &adapter->ident;
	uint32_t i, j, index, num;
	uint16_t tbl_sz = rte_le_to_cpu_16(ident->lif.eth.rss_ind_tbl_sz);

	IONIC_PRINT_CALL();

	if (!lif->rss_ind_tbl) {
		IONIC_PRINT(ERR, "RSS RETA not initialized, "
			"can't update the table");
		return -EINVAL;
	}

	if (reta_size != tbl_sz) {
		IONIC_PRINT(ERR, "The size of hash lookup table configured "
			"(%d) does not match the number hardware can support "
			"(%d)",
			reta_size, tbl_sz);
		return -EINVAL;
	}

	num = tbl_sz / RTE_ETH_RETA_GROUP_SIZE;

	for (i = 0; i < num; i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++) {
			if (reta_conf[i].mask & ((uint64_t)1 << j)) {
				index = (i * RTE_ETH_RETA_GROUP_SIZE) + j;
				lif->rss_ind_tbl[index] = reta_conf[i].reta[j];
			}
		}
	}

	return ionic_lif_rss_config(lif, lif->rss_types, NULL, NULL);
}

static int
ionic_dev_rss_reta_query(struct rte_eth_dev *eth_dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_identity *ident = &adapter->ident;
	int i, num;
	uint16_t tbl_sz = rte_le_to_cpu_16(ident->lif.eth.rss_ind_tbl_sz);

	IONIC_PRINT_CALL();

	if (reta_size != tbl_sz) {
		IONIC_PRINT(ERR, "The size of hash lookup table configured "
			"(%d) does not match the number hardware can support "
			"(%d)",
			reta_size, tbl_sz);
		return -EINVAL;
	}

	if (!lif->rss_ind_tbl) {
		IONIC_PRINT(ERR, "RSS RETA has not been built yet");
		return -EINVAL;
	}

	num = reta_size / RTE_ETH_RETA_GROUP_SIZE;

	for (i = 0; i < num; i++) {
		memcpy(reta_conf->reta,
			&lif->rss_ind_tbl[i * RTE_ETH_RETA_GROUP_SIZE],
			RTE_ETH_RETA_GROUP_SIZE);
		reta_conf++;
	}

	return 0;
}

static int
ionic_dev_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint64_t rss_hf = 0;

	IONIC_PRINT_CALL();

	if (!lif->rss_ind_tbl) {
		IONIC_PRINT(NOTICE, "RSS not enabled");
		return 0;
	}

	/* Get key value (if not null, rss_key is 40-byte) */
	if (rss_conf->rss_key != NULL &&
			rss_conf->rss_key_len >= IONIC_RSS_HASH_KEY_SIZE)
		memcpy(rss_conf->rss_key, lif->rss_hash_key,
			IONIC_RSS_HASH_KEY_SIZE);

	if (lif->rss_types & IONIC_RSS_TYPE_IPV4)
		rss_hf |= RTE_ETH_RSS_IPV4;
	if (lif->rss_types & IONIC_RSS_TYPE_IPV4_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
	if (lif->rss_types & IONIC_RSS_TYPE_IPV4_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
	if (lif->rss_types & IONIC_RSS_TYPE_IPV6)
		rss_hf |= RTE_ETH_RSS_IPV6;
	if (lif->rss_types & IONIC_RSS_TYPE_IPV6_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;
	if (lif->rss_types & IONIC_RSS_TYPE_IPV6_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;

	rss_conf->rss_hf = rss_hf;

	return 0;
}

static int
ionic_dev_rss_hash_update(struct rte_eth_dev *eth_dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	uint32_t rss_types = 0;
	uint8_t *key = NULL;

	IONIC_PRINT_CALL();

	if (rss_conf->rss_key)
		key = rss_conf->rss_key;

	if ((rss_conf->rss_hf & IONIC_ETH_RSS_OFFLOAD_ALL) == 0) {
		/*
		 * Can't disable rss through hash flags,
		 * if it is enabled by default during init
		 */
		if (lif->rss_ind_tbl)
			return -EINVAL;
	} else {
		/* Can't enable rss if disabled by default during init */
		if (!lif->rss_ind_tbl)
			return -EINVAL;

		if (rss_conf->rss_hf & RTE_ETH_RSS_IPV4)
			rss_types |= IONIC_RSS_TYPE_IPV4;
		if (rss_conf->rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
			rss_types |= IONIC_RSS_TYPE_IPV4_TCP;
		if (rss_conf->rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
			rss_types |= IONIC_RSS_TYPE_IPV4_UDP;
		if (rss_conf->rss_hf & RTE_ETH_RSS_IPV6)
			rss_types |= IONIC_RSS_TYPE_IPV6;
		if (rss_conf->rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
			rss_types |= IONIC_RSS_TYPE_IPV6_TCP;
		if (rss_conf->rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
			rss_types |= IONIC_RSS_TYPE_IPV6_UDP;

		ionic_lif_rss_config(lif, rss_types, key, NULL);
	}

	return 0;
}

static int
ionic_dev_stats_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_stats *stats)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	ionic_lif_get_stats(lif, stats);

	return 0;
}

static int
ionic_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	ionic_lif_reset_stats(lif);

	return 0;
}

static int
ionic_dev_xstats_get_names(__rte_unused struct rte_eth_dev *eth_dev,
		struct rte_eth_xstat_name *xstats_names,
		__rte_unused unsigned int size)
{
	unsigned int i;

	if (xstats_names != NULL) {
		for (i = 0; i < IONIC_NB_HW_STATS; i++) {
			snprintf(xstats_names[i].name,
					sizeof(xstats_names[i].name),
					"%s", rte_ionic_xstats_strings[i].name);
		}
	}

	return IONIC_NB_HW_STATS;
}

static int
ionic_dev_xstats_get_names_by_id(struct rte_eth_dev *eth_dev,
		const uint64_t *ids, struct rte_eth_xstat_name *xstats_names,
		unsigned int limit)
{
	struct rte_eth_xstat_name xstats_names_copy[IONIC_NB_HW_STATS];
	uint16_t i;

	if (!ids) {
		if (xstats_names != NULL) {
			for (i = 0; i < IONIC_NB_HW_STATS; i++) {
				snprintf(xstats_names[i].name,
					sizeof(xstats_names[i].name),
					"%s", rte_ionic_xstats_strings[i].name);
			}
		}

		return IONIC_NB_HW_STATS;
	}

	ionic_dev_xstats_get_names_by_id(eth_dev, NULL, xstats_names_copy,
		IONIC_NB_HW_STATS);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= IONIC_NB_HW_STATS) {
			IONIC_PRINT(ERR, "id value isn't valid");
			return -1;
		}

		strcpy(xstats_names[i].name, xstats_names_copy[ids[i]].name);
	}

	return limit;
}

static int
ionic_dev_xstats_get(struct rte_eth_dev *eth_dev, struct rte_eth_xstat *xstats,
		unsigned int n)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_lif_stats hw_stats;
	uint16_t i;

	if (n < IONIC_NB_HW_STATS)
		return IONIC_NB_HW_STATS;

	ionic_lif_get_hw_stats(lif, &hw_stats);

	for (i = 0; i < IONIC_NB_HW_STATS; i++) {
		xstats[i].value = *(uint64_t *)(((char *)&hw_stats) +
				rte_ionic_xstats_strings[i].offset);
		xstats[i].id = i;
	}

	return IONIC_NB_HW_STATS;
}

static int
ionic_dev_xstats_get_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids,
		uint64_t *values, unsigned int n)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_lif_stats hw_stats;
	uint64_t values_copy[IONIC_NB_HW_STATS];
	uint16_t i;

	if (!ids) {
		if (!ids && n < IONIC_NB_HW_STATS)
			return IONIC_NB_HW_STATS;

		ionic_lif_get_hw_stats(lif, &hw_stats);

		for (i = 0; i < IONIC_NB_HW_STATS; i++) {
			values[i] = *(uint64_t *)(((char *)&hw_stats) +
					rte_ionic_xstats_strings[i].offset);
		}

		return IONIC_NB_HW_STATS;
	}

	ionic_dev_xstats_get_by_id(eth_dev, NULL, values_copy,
			IONIC_NB_HW_STATS);

	for (i = 0; i < n; i++) {
		if (ids[i] >= IONIC_NB_HW_STATS) {
			IONIC_PRINT(ERR, "id value isn't valid");
			return -1;
		}

		values[i] = values_copy[ids[i]];
	}

	return n;
}

static int
ionic_dev_xstats_reset(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	ionic_lif_reset_hw_stats(lif);

	return 0;
}

static int
ionic_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	ionic_lif_configure(lif);

	ionic_lif_set_features(lif);

	return 0;
}

static inline uint32_t
ionic_parse_link_speeds(uint16_t link_speeds)
{
	if (link_speeds & RTE_ETH_LINK_SPEED_100G)
		return 100000;
	else if (link_speeds & RTE_ETH_LINK_SPEED_50G)
		return 50000;
	else if (link_speeds & RTE_ETH_LINK_SPEED_40G)
		return 40000;
	else if (link_speeds & RTE_ETH_LINK_SPEED_25G)
		return 25000;
	else if (link_speeds & RTE_ETH_LINK_SPEED_10G)
		return 10000;
	else
		return 0;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
ionic_dev_start(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;
	uint32_t speed = 0, allowed_speeds;
	uint8_t an_enable;
	int err;

	IONIC_PRINT_CALL();

	allowed_speeds =
		RTE_ETH_LINK_SPEED_FIXED |
		RTE_ETH_LINK_SPEED_10G |
		RTE_ETH_LINK_SPEED_25G |
		RTE_ETH_LINK_SPEED_40G |
		RTE_ETH_LINK_SPEED_50G |
		RTE_ETH_LINK_SPEED_100G;

	if (dev_conf->link_speeds & ~allowed_speeds) {
		IONIC_PRINT(ERR, "Invalid link setting");
		return -EINVAL;
	}

	if (dev_conf->lpbk_mode)
		IONIC_PRINT(WARNING, "Loopback mode not supported");

	err = ionic_lif_start(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot start LIF: %d", err);
		return err;
	}

	/* Configure link */
	an_enable = (dev_conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) == 0;

	ionic_dev_cmd_port_autoneg(idev, an_enable);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		IONIC_PRINT(WARNING, "Failed to %s autonegotiation",
			an_enable ? "enable" : "disable");

	if (!an_enable)
		speed = ionic_parse_link_speeds(dev_conf->link_speeds);
	if (speed) {
		ionic_dev_cmd_port_speed(idev, speed);
		err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
		if (err)
			IONIC_PRINT(WARNING, "Failed to set link speed %u",
				speed);
	}

	ionic_dev_link_update(eth_dev, 0);

	return 0;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
ionic_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);

	IONIC_PRINT_CALL();

	ionic_lif_stop(lif);

	return 0;
}

static void ionic_unconfigure_intr(struct ionic_adapter *adapter);

/*
 * Reset and stop device.
 */
static int
ionic_dev_close(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;

	IONIC_PRINT_CALL();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ionic_lif_stop(lif);

	ionic_lif_free_queues(lif);

	IONIC_PRINT(NOTICE, "Removing device %s", eth_dev->device->name);
	ionic_unconfigure_intr(adapter);

	rte_eth_dev_destroy(eth_dev, eth_ionic_dev_uninit);

	ionic_port_reset(adapter);
	ionic_reset(adapter);

	rte_free(adapter);

	return 0;
}

static int
eth_ionic_dev_init(struct rte_eth_dev *eth_dev, void *init_params)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = (struct ionic_adapter *)init_params;
	int err;

	IONIC_PRINT_CALL();

	eth_dev->dev_ops = &ionic_eth_dev_ops;
	eth_dev->rx_pkt_burst = &ionic_recv_pkts;
	eth_dev->tx_pkt_burst = &ionic_xmit_pkts;
	eth_dev->tx_pkt_prepare = &ionic_prep_pkts;

	/* Multi-process not supported, primary does initialization anyway */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	lif->eth_dev = eth_dev;
	lif->adapter = adapter;
	adapter->lif = lif;

	IONIC_PRINT(DEBUG, "Up to %u MAC addresses supported",
		adapter->max_mac_addrs);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ionic",
		RTE_ETHER_ADDR_LEN * adapter->max_mac_addrs, 0);

	if (eth_dev->data->mac_addrs == NULL) {
		IONIC_PRINT(ERR, "Failed to allocate %u bytes needed to "
			"store MAC addresses",
			RTE_ETHER_ADDR_LEN * adapter->max_mac_addrs);
		err = -ENOMEM;
		goto err;
	}

	err = ionic_lif_alloc(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate LIFs: %d, aborting",
			err);
		goto err;
	}

	err = ionic_lif_init(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init LIFs: %d, aborting", err);
		goto err_free_lif;
	}

	/* Copy the MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)lif->mac_addr,
		&eth_dev->data->mac_addrs[0]);

	IONIC_PRINT(DEBUG, "Port %u initialized", eth_dev->data->port_id);

	return 0;

err_free_lif:
	ionic_lif_free(lif);
err:
	return err;
}

static int
eth_ionic_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct ionic_lif *lif = IONIC_ETH_DEV_TO_LIF(eth_dev);
	struct ionic_adapter *adapter = lif->adapter;

	IONIC_PRINT_CALL();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	adapter->lif = NULL;

	ionic_lif_deinit(lif);
	ionic_lif_free(lif);

	if (!(lif->state & IONIC_LIF_F_FW_RESET))
		ionic_lif_reset(lif);

	return 0;
}

static int
ionic_configure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int err;

	IONIC_PRINT(DEBUG, "Configuring %u intrs", adapter->nintrs);

	if (rte_intr_efd_enable(intr_handle, adapter->nintrs)) {
		IONIC_PRINT(ERR, "Fail to create eventfd");
		return -1;
	}

	if (rte_intr_dp_is_en(intr_handle))
		IONIC_PRINT(DEBUG,
			"Packet I/O interrupt on datapath is enabled");

	if (rte_intr_vec_list_alloc(intr_handle, "intr_vec", adapter->nintrs)) {
		IONIC_PRINT(ERR, "Failed to allocate %u vectors",
			    adapter->nintrs);
		return -ENOMEM;
	}

	err = rte_intr_callback_register(intr_handle,
		ionic_dev_interrupt_handler,
		adapter);

	if (err) {
		IONIC_PRINT(ERR,
			"Failure registering interrupts handler (%d)",
			err);
		return err;
	}

	/* enable intr mapping */
	err = rte_intr_enable(intr_handle);

	if (err) {
		IONIC_PRINT(ERR, "Failure enabling interrupts (%d)", err);
		return err;
	}

	return 0;
}

static void
ionic_unconfigure_intr(struct ionic_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	rte_intr_disable(intr_handle);

	rte_intr_callback_unregister(intr_handle,
		ionic_dev_interrupt_handler,
		adapter);
}

static int
eth_ionic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_mem_resource *resource;
	struct ionic_adapter *adapter;
	struct ionic_hw *hw;
	unsigned long i;
	int err;

	/* Check structs (trigger error at compilation time) */
	ionic_struct_size_checks();

	/* Multi-process not supported */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		err = -EPERM;
		goto err;
	}

	IONIC_PRINT(DEBUG, "Initializing device %s",
		pci_dev->device.name);

	adapter = rte_zmalloc("ionic", sizeof(*adapter), 0);
	if (!adapter) {
		IONIC_PRINT(ERR, "OOM");
		err = -ENOMEM;
		goto err;
	}

	adapter->pci_dev = pci_dev;
	hw = &adapter->hw;

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;

	err = ionic_init_mac(hw);
	if (err != 0) {
		IONIC_PRINT(ERR, "Mac init failed: %d", err);
		err = -EIO;
		goto err_free_adapter;
	}

	adapter->num_bars = 0;
	for (i = 0; i < PCI_MAX_RESOURCE && i < IONIC_BARS_MAX; i++) {
		resource = &pci_dev->mem_resource[i];
		if (resource->phys_addr == 0 || resource->len == 0)
			continue;
		adapter->bars[adapter->num_bars].vaddr = resource->addr;
		adapter->bars[adapter->num_bars].bus_addr = resource->phys_addr;
		adapter->bars[adapter->num_bars].len = resource->len;
		adapter->num_bars++;
	}

	/* Discover ionic dev resources */

	err = ionic_setup(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot setup device: %d, aborting", err);
		goto err_free_adapter;
	}

	err = ionic_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify device: %d, aborting",
			err);
		goto err_free_adapter;
	}

	err = ionic_init(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init device: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Configure the ports */
	err = ionic_port_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify port: %d, aborting",
			err);
		goto err_free_adapter;
	}

	err = ionic_port_init(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot init port: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Configure LIFs */
	err = ionic_lif_identify(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot identify lif: %d, aborting", err);
		goto err_free_adapter;
	}

	/* Allocate and init LIFs */
	err = ionic_lifs_size(adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot size LIFs: %d, aborting", err);
		goto err_free_adapter;
	}

	adapter->max_mac_addrs =
		rte_le_to_cpu_32(adapter->ident.lif.eth.max_ucast_filters);

	if (rte_le_to_cpu_32(adapter->ident.dev.nlifs) != 1) {
		IONIC_PRINT(ERR, "Unexpected request for %d LIFs",
			rte_le_to_cpu_32(adapter->ident.dev.nlifs));
		goto err_free_adapter;
	}

	snprintf(name, sizeof(name), "%s_lif", pci_dev->device.name);
	err = rte_eth_dev_create(&pci_dev->device,
			name, sizeof(struct ionic_lif),
			NULL, NULL, eth_ionic_dev_init, adapter);
	if (err) {
		IONIC_PRINT(ERR, "Cannot create eth device for %s", name);
		goto err_free_adapter;
	}

	err = ionic_configure_intr(adapter);

	if (err) {
		IONIC_PRINT(ERR, "Failed to configure interrupts");
		goto err_free_adapter;
	}

	return 0;

err_free_adapter:
	rte_free(adapter);
err:
	return err;
}

static int
eth_ionic_pci_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_dev *eth_dev;

	/* Adapter lookup is using the eth_dev name */
	snprintf(name, sizeof(name), "%s_lif", pci_dev->device.name);

	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev)
		ionic_dev_close(eth_dev);
	else
		IONIC_PRINT(DEBUG, "Cannot find device %s",
			pci_dev->device.name);

	return 0;
}

static struct rte_pci_driver rte_ionic_pmd = {
	.id_table = pci_id_ionic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ionic_pci_probe,
	.remove = eth_ionic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ionic, rte_ionic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ionic, pci_id_ionic_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ionic, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_LOG_REGISTER_DEFAULT(ionic_logtype, NOTICE);
