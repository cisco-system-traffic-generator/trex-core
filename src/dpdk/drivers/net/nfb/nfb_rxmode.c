/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#include "nfb_rxmode.h"
#include "nfb.h"

int
nfb_eth_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint16_t i;

	for (i = 0; i < internals->max_rxmac; ++i) {
		nc_rxmac_mac_filter_enable(internals->rxmac[i],
			RXMAC_MAC_FILTER_PROMISCUOUS);
	}

	return 0;
}

int
nfb_eth_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;
	uint16_t i;
	enum nc_rxmac_mac_filter filter = RXMAC_MAC_FILTER_TABLE_BCAST;

	if (dev->data->all_multicast)
		filter = RXMAC_MAC_FILTER_TABLE_BCAST_MCAST;

	for (i = 0; i < internals->max_rxmac; ++i) {
		nc_rxmac_mac_filter_enable(internals->rxmac[i], filter);
	}

	return 0;
}

int
nfb_eth_promiscuous_get(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	struct nc_rxmac_status status;
	status.mac_filter = RXMAC_MAC_FILTER_PROMISCUOUS;

	if (internals->max_rxmac > 0)
		nc_rxmac_read_status(internals->rxmac[0], &status);

	return (status.mac_filter == RXMAC_MAC_FILTER_PROMISCUOUS);
}

int
nfb_eth_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	uint16_t i;
	if (dev->data->promiscuous)
		return 0;
	for (i = 0; i < internals->max_rxmac; ++i) {
		nc_rxmac_mac_filter_enable(internals->rxmac[i],
			RXMAC_MAC_FILTER_TABLE_BCAST_MCAST);
	}

	return 0;
}

int
nfb_eth_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	uint16_t i;

	if (dev->data->promiscuous)
		return 0;

	for (i = 0; i < internals->max_rxmac; ++i) {
		nc_rxmac_mac_filter_enable(internals->rxmac[i],
			RXMAC_MAC_FILTER_TABLE_BCAST);
	}

	return 0;
}

int
nfb_eth_allmulticast_get(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)
		dev->data->dev_private;

	struct nc_rxmac_status status;
	status.mac_filter = RXMAC_MAC_FILTER_PROMISCUOUS;

	if (internals->max_rxmac > 0)
		nc_rxmac_read_status(internals->rxmac[0], &status);

	return (status.mac_filter == RXMAC_MAC_FILTER_TABLE_BCAST_MCAST);
}
