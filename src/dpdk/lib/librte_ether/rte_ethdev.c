/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "rte_ether.h"
#include "rte_ethdev.h"
#include "ethdev_profile.h"

static const char *MZ_RTE_ETH_DEV_DATA = "rte_eth_dev_data";
struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];
static struct rte_eth_dev_data *rte_eth_dev_data;
static uint8_t eth_dev_last_created_port;

/* spinlock for eth device callbacks */
static rte_spinlock_t rte_eth_dev_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove rx callbacks */
static rte_spinlock_t rte_eth_rx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove tx callbacks */
static rte_spinlock_t rte_eth_tx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* store statistics names and its offset in stats structure  */
struct rte_eth_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

static const struct rte_eth_xstats_name_off rte_stats_strings[] = {
	{"rx_good_packets", offsetof(struct rte_eth_stats, ipackets)},
	{"tx_good_packets", offsetof(struct rte_eth_stats, opackets)},
	{"rx_good_bytes", offsetof(struct rte_eth_stats, ibytes)},
	{"tx_good_bytes", offsetof(struct rte_eth_stats, obytes)},
	{"rx_errors", offsetof(struct rte_eth_stats, ierrors)},
	{"tx_errors", offsetof(struct rte_eth_stats, oerrors)},
	{"rx_mbuf_allocation_errors", offsetof(struct rte_eth_stats,
		rx_nombuf)},
};

#define RTE_NB_STATS (sizeof(rte_stats_strings) / sizeof(rte_stats_strings[0]))

static const struct rte_eth_xstats_name_off rte_rxq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_ipackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_ibytes)},
	{"errors", offsetof(struct rte_eth_stats, q_errors)},
};

#define RTE_NB_RXQ_STATS (sizeof(rte_rxq_stats_strings) /	\
		sizeof(rte_rxq_stats_strings[0]))

static const struct rte_eth_xstats_name_off rte_txq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_opackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_obytes)},
};
#define RTE_NB_TXQ_STATS (sizeof(rte_txq_stats_strings) /	\
		sizeof(rte_txq_stats_strings[0]))


/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_eth_dev_callback {
	TAILQ_ENTRY(rte_eth_dev_callback) next; /**< Callbacks list */
	rte_eth_dev_cb_fn cb_fn;                /**< Callback address */
	void *cb_arg;                           /**< Parameter for callback */
	void *ret_param;                        /**< Return parameter */
	enum rte_eth_event_type event;          /**< Interrupt event type */
	uint32_t active;                        /**< Callback is executing */
};

enum {
	STAT_QMAP_TX = 0,
	STAT_QMAP_RX
};

uint16_t
rte_eth_find_next(uint16_t port_id)
{
	while (port_id < RTE_MAX_ETHPORTS &&
	       rte_eth_devices[port_id].state != RTE_ETH_DEV_ATTACHED)
		port_id++;

	if (port_id >= RTE_MAX_ETHPORTS)
		return RTE_MAX_ETHPORTS;

	return port_id;
}

static void
rte_eth_dev_data_alloc(void)
{
	const unsigned flags = 0;
	const struct rte_memzone *mz;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(MZ_RTE_ETH_DEV_DATA,
				RTE_MAX_ETHPORTS * sizeof(*rte_eth_dev_data),
				rte_socket_id(), flags);
	} else
		mz = rte_memzone_lookup(MZ_RTE_ETH_DEV_DATA);
	if (mz == NULL)
		rte_panic("Cannot allocate memzone for ethernet port data\n");

	rte_eth_dev_data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(rte_eth_dev_data, 0,
				RTE_MAX_ETHPORTS * sizeof(*rte_eth_dev_data));
}

struct rte_eth_dev *
rte_eth_dev_allocated(const char *name)
{
	unsigned i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if ((rte_eth_devices[i].state == RTE_ETH_DEV_ATTACHED) &&
		    strcmp(rte_eth_devices[i].data->name, name) == 0)
			return &rte_eth_devices[i];
	}
	return NULL;
}

static uint16_t
rte_eth_dev_find_free_port(void)
{
	unsigned i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (rte_eth_devices[i].state == RTE_ETH_DEV_UNUSED)
			return i;
	}
	return RTE_MAX_ETHPORTS;
}

static struct rte_eth_dev *
eth_dev_get(uint16_t port_id)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[port_id];

	eth_dev->data = &rte_eth_dev_data[port_id];
	eth_dev->state = RTE_ETH_DEV_ATTACHED;
	TAILQ_INIT(&(eth_dev->link_intr_cbs));

	eth_dev_last_created_port = port_id;

	return eth_dev;
}

struct rte_eth_dev *
rte_eth_dev_allocate(const char *name)
{
	uint16_t port_id;
	struct rte_eth_dev *eth_dev;

	port_id = rte_eth_dev_find_free_port();
	if (port_id == RTE_MAX_ETHPORTS) {
		RTE_PMD_DEBUG_TRACE("Reached maximum number of Ethernet ports\n");
		return NULL;
	}

	if (rte_eth_dev_data == NULL)
		rte_eth_dev_data_alloc();

	if (rte_eth_dev_allocated(name) != NULL) {
		RTE_PMD_DEBUG_TRACE("Ethernet Device with name %s already allocated!\n",
				name);
		return NULL;
	}

	memset(&rte_eth_dev_data[port_id], 0, sizeof(struct rte_eth_dev_data));
	eth_dev = eth_dev_get(port_id);
	snprintf(eth_dev->data->name, sizeof(eth_dev->data->name), "%s", name);
	eth_dev->data->port_id = port_id;
	eth_dev->data->mtu = ETHER_MTU;

	return eth_dev;
}

/*
 * Attach to a port already registered by the primary process, which
 * makes sure that the same device would have the same port id both
 * in the primary and secondary process.
 */
struct rte_eth_dev *
rte_eth_dev_attach_secondary(const char *name)
{
	uint16_t i;
	struct rte_eth_dev *eth_dev;

	if (rte_eth_dev_data == NULL)
		rte_eth_dev_data_alloc();

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (strcmp(rte_eth_dev_data[i].name, name) == 0)
			break;
	}
	if (i == RTE_MAX_ETHPORTS) {
		RTE_PMD_DEBUG_TRACE(
			"device %s is not driven by the primary process\n",
			name);
		return NULL;
	}

	eth_dev = eth_dev_get(i);
	RTE_ASSERT(eth_dev->data->port_id == i);

	return eth_dev;
}

int
rte_eth_dev_release_port(struct rte_eth_dev *eth_dev)
{
	if (eth_dev == NULL)
		return -EINVAL;

	eth_dev->state = RTE_ETH_DEV_UNUSED;
	return 0;
}

int
rte_eth_dev_is_valid_port(uint16_t port_id)
{
	if (port_id >= RTE_MAX_ETHPORTS ||
	    (rte_eth_devices[port_id].state != RTE_ETH_DEV_ATTACHED &&
	     rte_eth_devices[port_id].state != RTE_ETH_DEV_DEFERRED))
		return 0;
	else
		return 1;
}

int
rte_eth_dev_socket_id(uint16_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);
	return rte_eth_devices[port_id].data->numa_node;
}

void *
rte_eth_dev_get_sec_ctx(uint8_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
	return rte_eth_devices[port_id].security_ctx;
}

uint16_t
rte_eth_dev_count(void)
{
	uint16_t p;
	uint16_t count;

	count = 0;

	RTE_ETH_FOREACH_DEV(p)
		count++;

	return count;
}

int
rte_eth_dev_get_name_by_port(uint16_t port_id, char *name)
{
	char *tmp;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (name == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	/* shouldn't check 'rte_eth_devices[i].data',
	 * because it might be overwritten by VDEV PMD */
	tmp = rte_eth_dev_data[port_id].name;
	strcpy(name, tmp);
	return 0;
}

int
rte_eth_dev_get_port_by_name(const char *name, uint16_t *port_id)
{
	int i;

	if (name == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	RTE_ETH_FOREACH_DEV(i) {
		if (!strncmp(name,
			rte_eth_dev_data[i].name, strlen(name))) {

			*port_id = i;

			return 0;
		}
	}
	return -ENODEV;
}

/* attach the new device, then store port_id of the device */
int
rte_eth_dev_attach(const char *devargs, uint16_t *port_id)
{
	int ret = -1;
	int current = rte_eth_dev_count();
	char *name = NULL;
	char *args = NULL;

	if ((devargs == NULL) || (port_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}

	/* parse devargs, then retrieve device name and args */
	if (rte_eal_parse_devargs_str(devargs, &name, &args))
		goto err;

	ret = rte_eal_dev_attach(name, args);
	if (ret < 0)
		goto err;

	/* no point looking at the port count if no port exists */
	if (!rte_eth_dev_count()) {
		RTE_LOG(ERR, EAL, "No port found for device (%s)\n", name);
		ret = -1;
		goto err;
	}

	/* if nothing happened, there is a bug here, since some driver told us
	 * it did attach a device, but did not create a port.
	 */
	if (current == rte_eth_dev_count()) {
		ret = -1;
		goto err;
	}

	*port_id = eth_dev_last_created_port;
	ret = 0;

err:
	free(name);
	free(args);
	return ret;
}

/* detach the device, then store the name of the device */
int
rte_eth_dev_detach(uint16_t port_id, char *name)
{
	uint32_t dev_flags;
	int ret = -1;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (name == NULL) {
		ret = -EINVAL;
		goto err;
	}

	dev_flags = rte_eth_devices[port_id].data->dev_flags;
	if (dev_flags & RTE_ETH_DEV_BONDED_SLAVE) {
		RTE_LOG(ERR, EAL, "Port %" PRIu16 " is bonded, cannot detach\n",
			port_id);
		ret = -ENOTSUP;
		goto err;
	}

	snprintf(name, sizeof(rte_eth_devices[port_id].data->name),
		 "%s", rte_eth_devices[port_id].data->name);

	ret = rte_eal_dev_detach(rte_eth_devices[port_id].device);
	if (ret < 0)
		goto err;

	rte_eth_devices[port_id].state = RTE_ETH_DEV_UNUSED;
	return 0;

err:
	return ret;
}

static int
rte_eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_rx_queues;
	void **rxq;
	unsigned i;

	if (dev->data->rx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
				sizeof(dev->data->rx_queues[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->rx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);
		rxq = rte_realloc(rxq, sizeof(rxq[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (rxq == NULL)
			return -(ENOMEM);
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(rxq + old_nb_queues, 0,
				sizeof(rxq[0]) * new_qs);
		}

		dev->data->rx_queues = rxq;

	} else if (dev->data->rx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);

		rte_free(dev->data->rx_queues);
		dev->data->rx_queues = NULL;
	}
	dev->data->nb_rx_queues = nb_queues;
	return 0;
}

int
rte_eth_dev_rx_queue_start(uint16_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_start, -ENOTSUP);

	if (dev->data->rx_queue_state[rx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already started\n",
			rx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->rx_queue_start(dev, rx_queue_id);

}

int
rte_eth_dev_rx_queue_stop(uint16_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_stop, -ENOTSUP);

	if (dev->data->rx_queue_state[rx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already stopped\n",
			rx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->rx_queue_stop(dev, rx_queue_id);

}

int
rte_eth_dev_tx_queue_start(uint16_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_start, -ENOTSUP);

	if (dev->data->tx_queue_state[tx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already started\n",
			tx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->tx_queue_start(dev, tx_queue_id);

}

int
rte_eth_dev_tx_queue_stop(uint16_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_stop, -ENOTSUP);

	if (dev->data->tx_queue_state[tx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already stopped\n",
			tx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->tx_queue_stop(dev, tx_queue_id);

}

static int
rte_eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_tx_queues;
	void **txq;
	unsigned i;

	if (dev->data->tx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
						   sizeof(dev->data->tx_queues[0]) * nb_queues,
						   RTE_CACHE_LINE_SIZE);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->tx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);
		txq = rte_realloc(txq, sizeof(txq[0]) * nb_queues,
				  RTE_CACHE_LINE_SIZE);
		if (txq == NULL)
			return -ENOMEM;
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(txq + old_nb_queues, 0,
			       sizeof(txq[0]) * new_qs);
		}

		dev->data->tx_queues = txq;

	} else if (dev->data->tx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);

		rte_free(dev->data->tx_queues);
		dev->data->tx_queues = NULL;
	}
	dev->data->nb_tx_queues = nb_queues;
	return 0;
}

uint32_t
rte_eth_speed_bitflag(uint32_t speed, int duplex)
{
	switch (speed) {
	case ETH_SPEED_NUM_10M:
		return duplex ? ETH_LINK_SPEED_10M : ETH_LINK_SPEED_10M_HD;
	case ETH_SPEED_NUM_100M:
		return duplex ? ETH_LINK_SPEED_100M : ETH_LINK_SPEED_100M_HD;
	case ETH_SPEED_NUM_1G:
		return ETH_LINK_SPEED_1G;
	case ETH_SPEED_NUM_2_5G:
		return ETH_LINK_SPEED_2_5G;
	case ETH_SPEED_NUM_5G:
		return ETH_LINK_SPEED_5G;
	case ETH_SPEED_NUM_10G:
		return ETH_LINK_SPEED_10G;
	case ETH_SPEED_NUM_20G:
		return ETH_LINK_SPEED_20G;
	case ETH_SPEED_NUM_25G:
		return ETH_LINK_SPEED_25G;
	case ETH_SPEED_NUM_40G:
		return ETH_LINK_SPEED_40G;
	case ETH_SPEED_NUM_50G:
		return ETH_LINK_SPEED_50G;
	case ETH_SPEED_NUM_56G:
		return ETH_LINK_SPEED_56G;
	case ETH_SPEED_NUM_100G:
		return ETH_LINK_SPEED_100G;
	default:
		return 0;
	}
}

/**
 * A conversion function from rxmode bitfield API.
 */
static void
rte_eth_convert_rx_offload_bitfield(const struct rte_eth_rxmode *rxmode,
				    uint64_t *rx_offloads)
{
	uint64_t offloads = 0;

	if (rxmode->header_split == 1)
		offloads |= DEV_RX_OFFLOAD_HEADER_SPLIT;
	if (rxmode->hw_ip_checksum == 1)
		offloads |= DEV_RX_OFFLOAD_CHECKSUM;
	if (rxmode->hw_vlan_filter == 1)
		offloads |= DEV_RX_OFFLOAD_VLAN_FILTER;
	if (rxmode->hw_vlan_strip == 1)
		offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
	if (rxmode->hw_vlan_extend == 1)
		offloads |= DEV_RX_OFFLOAD_VLAN_EXTEND;
	if (rxmode->jumbo_frame == 1)
		offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	if (rxmode->hw_strip_crc == 1)
		offloads |= DEV_RX_OFFLOAD_CRC_STRIP;
	if (rxmode->enable_scatter == 1)
		offloads |= DEV_RX_OFFLOAD_SCATTER;
	if (rxmode->enable_lro == 1)
		offloads |= DEV_RX_OFFLOAD_TCP_LRO;
	if (rxmode->hw_timestamp == 1)
		offloads |= DEV_RX_OFFLOAD_TIMESTAMP;
	if (rxmode->security == 1)
		offloads |= DEV_RX_OFFLOAD_SECURITY;

	*rx_offloads = offloads;
}

/**
 * A conversion function from rxmode offloads API.
 */
static void
rte_eth_convert_rx_offloads(const uint64_t rx_offloads,
			    struct rte_eth_rxmode *rxmode)
{

	if (rx_offloads & DEV_RX_OFFLOAD_HEADER_SPLIT)
		rxmode->header_split = 1;
	else
		rxmode->header_split = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_CHECKSUM)
		rxmode->hw_ip_checksum = 1;
	else
		rxmode->hw_ip_checksum = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
		rxmode->hw_vlan_filter = 1;
	else
		rxmode->hw_vlan_filter = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
		rxmode->hw_vlan_strip = 1;
	else
		rxmode->hw_vlan_strip = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
		rxmode->hw_vlan_extend = 1;
	else
		rxmode->hw_vlan_extend = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_JUMBO_FRAME)
		rxmode->jumbo_frame = 1;
	else
		rxmode->jumbo_frame = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_CRC_STRIP)
		rxmode->hw_strip_crc = 1;
	else
		rxmode->hw_strip_crc = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_SCATTER)
		rxmode->enable_scatter = 1;
	else
		rxmode->enable_scatter = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
		rxmode->enable_lro = 1;
	else
		rxmode->enable_lro = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_TIMESTAMP)
		rxmode->hw_timestamp = 1;
	else
		rxmode->hw_timestamp = 0;
	if (rx_offloads & DEV_RX_OFFLOAD_SECURITY)
		rxmode->security = 1;
	else
		rxmode->security = 0;
}

int
rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf local_conf = *dev_conf;
	int diag;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (nb_rx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_PMD_DEBUG_TRACE(
			"Number of RX queues requested (%u) is greater than max supported(%d)\n",
			nb_rx_q, RTE_MAX_QUEUES_PER_PORT);
		return -EINVAL;
	}

	if (nb_tx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_PMD_DEBUG_TRACE(
			"Number of TX queues requested (%u) is greater than max supported(%d)\n",
			nb_tx_q, RTE_MAX_QUEUES_PER_PORT);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	/*
	 * Convert between the offloads API to enable PMDs to support
	 * only one of them.
	 */
	if ((dev_conf->rxmode.ignore_offload_bitfield == 0)) {
		rte_eth_convert_rx_offload_bitfield(
				&dev_conf->rxmode, &local_conf.rxmode.offloads);
	} else {
		rte_eth_convert_rx_offloads(dev_conf->rxmode.offloads,
					    &local_conf.rxmode);
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, &local_conf, sizeof(dev->data->dev_conf));

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);

	if (nb_rx_q == 0 && nb_tx_q == 0) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d both rx and tx queue cannot be 0\n", port_id);
		return -EINVAL;
	}

	if (nb_rx_q > dev_info.max_rx_queues) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_queues=%d > %d\n",
				port_id, nb_rx_q, dev_info.max_rx_queues);
		return -EINVAL;
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_queues=%d > %d\n",
				port_id, nb_tx_q, dev_info.max_tx_queues);
		return -EINVAL;
	}

	/* Check that the device supports requested interrupts */
	if ((dev_conf->intr_conf.lsc == 1) &&
		(!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC))) {
			RTE_PMD_DEBUG_TRACE("driver %s does not support lsc\n",
					dev->device->driver->name);
			return -EINVAL;
	}
	if ((dev_conf->intr_conf.rmv == 1) &&
	    (!(dev->data->dev_flags & RTE_ETH_DEV_INTR_RMV))) {
		RTE_PMD_DEBUG_TRACE("driver %s does not support rmv\n",
				    dev->device->driver->name);
		return -EINVAL;
	}

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (local_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (dev_conf->rxmode.max_rx_pkt_len >
		    dev_info.max_rx_pktlen) {
			RTE_PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" > max valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)dev_info.max_rx_pktlen);
			return -EINVAL;
		} else if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN) {
			RTE_PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" < min valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)ETHER_MIN_LEN);
			return -EINVAL;
		}
	} else {
		if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN ||
			dev_conf->rxmode.max_rx_pkt_len > ETHER_MAX_LEN)
			/* Use default value */
			dev->data->dev_conf.rxmode.max_rx_pkt_len =
							ETHER_MAX_LEN;
	}

	/*
	 * Setup new number of RX/TX queues and reconfigure device.
	 */
	diag = rte_eth_dev_rx_queue_config(dev, nb_rx_q);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d rte_eth_dev_rx_queue_config = %d\n",
				port_id, diag);
		return diag;
	}

	diag = rte_eth_dev_tx_queue_config(dev, nb_tx_q);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d rte_eth_dev_tx_queue_config = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		return diag;
	}

	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d dev_configure = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		rte_eth_dev_tx_queue_config(dev, 0);
		return diag;
	}

	/* Initialize Rx profiling if enabled at compilation time. */
	diag = __rte_eth_profile_rx_init(port_id, dev);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d __rte_eth_profile_rx_init = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		rte_eth_dev_tx_queue_config(dev, 0);
		return diag;
	}

	return 0;
}

void
_rte_eth_dev_reset(struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
			"port %d must be stopped to allow reset\n",
			dev->data->port_id);
		return;
	}

	rte_eth_dev_rx_queue_config(dev, 0);
	rte_eth_dev_tx_queue_config(dev, 0);

	memset(&dev->data->dev_conf, 0, sizeof(dev->data->dev_conf));
}

static void
rte_eth_dev_config_restore(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct ether_addr *addr;
	uint16_t i;
	uint32_t pool = 0;
	uint64_t pool_mask;

	dev = &rte_eth_devices[port_id];

	rte_eth_dev_info_get(port_id, &dev_info);

	/* replay MAC address configuration including default MAC */
	addr = &dev->data->mac_addrs[0];
	if (*dev->dev_ops->mac_addr_set != NULL)
		(*dev->dev_ops->mac_addr_set)(dev, addr);
	else if (*dev->dev_ops->mac_addr_add != NULL)
		(*dev->dev_ops->mac_addr_add)(dev, addr, 0, pool);

	if (*dev->dev_ops->mac_addr_add != NULL) {
		for (i = 1; i < dev_info.max_mac_addrs; i++) {
			addr = &dev->data->mac_addrs[i];

			/* skip zero address */
			if (is_zero_ether_addr(addr))
				continue;

			pool = 0;
			pool_mask = dev->data->mac_pool_sel[i];

			do {
				if (pool_mask & 1ULL)
					(*dev->dev_ops->mac_addr_add)(dev,
						addr, i, pool);
				pool_mask >>= 1;
				pool++;
			} while (pool_mask);
		}
	}

	/* replay promiscuous configuration */
	if (rte_eth_promiscuous_get(port_id) == 1)
		rte_eth_promiscuous_enable(port_id);
	else if (rte_eth_promiscuous_get(port_id) == 0)
		rte_eth_promiscuous_disable(port_id);

	/* replay all multicast configuration */
	if (rte_eth_allmulticast_get(port_id) == 1)
		rte_eth_allmulticast_enable(port_id);
	else if (rte_eth_allmulticast_get(port_id) == 0)
		rte_eth_allmulticast_disable(port_id);
}

int
rte_eth_dev_start(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int diag;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		RTE_PMD_DEBUG_TRACE("Device with port_id=%" PRIu16
			" already started\n",
			port_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	rte_eth_dev_config_restore(port_id);

	if (dev->data->dev_conf.intr_conf.lsc == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 0);
	}
	return 0;
}

void
rte_eth_dev_stop(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		RTE_PMD_DEBUG_TRACE("Device with port_id=%" PRIu16
			" already stopped\n",
			port_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

int
rte_eth_dev_set_link_up(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_up, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_up)(dev);
}

int
rte_eth_dev_set_link_down(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_down, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_down)(dev);
}

void
rte_eth_dev_close(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_close);
	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_close)(dev);

	dev->data->nb_rx_queues = 0;
	rte_free(dev->data->rx_queues);
	dev->data->rx_queues = NULL;
	dev->data->nb_tx_queues = 0;
	rte_free(dev->data->tx_queues);
	dev->data->tx_queues = NULL;
}

int
rte_eth_dev_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_reset, -ENOTSUP);

	rte_eth_dev_stop(port_id);
	ret = dev->dev_ops->dev_reset(dev);

	return ret;
}

int
rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
		       uint16_t nb_rx_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	int ret;
	uint32_t mbp_buf_size;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf local_conf;
	void **rxq;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_setup, -ENOTSUP);

	/*
	 * Check the size of the mbuf data buffer.
	 * This value must be provided in the private data of the memory pool.
	 * First check that the memory pool has a valid private data.
	 */
	rte_eth_dev_info_get(port_id, &dev_info);
	if (mp->private_data_size < sizeof(struct rte_pktmbuf_pool_private)) {
		RTE_PMD_DEBUG_TRACE("%s private_data_size %d < %d\n",
				mp->name, (int) mp->private_data_size,
				(int) sizeof(struct rte_pktmbuf_pool_private));
		return -ENOSPC;
	}
	mbp_buf_size = rte_pktmbuf_data_room_size(mp);

	if ((mbp_buf_size - RTE_PKTMBUF_HEADROOM) < dev_info.min_rx_bufsize) {
		RTE_PMD_DEBUG_TRACE("%s mbuf_data_room_size %d < %d "
				"(RTE_PKTMBUF_HEADROOM=%d + min_rx_bufsize(dev)"
				"=%d)\n",
				mp->name,
				(int)mbp_buf_size,
				(int)(RTE_PKTMBUF_HEADROOM +
				      dev_info.min_rx_bufsize),
				(int)RTE_PKTMBUF_HEADROOM,
				(int)dev_info.min_rx_bufsize);
		return -EINVAL;
	}

	if (nb_rx_desc > dev_info.rx_desc_lim.nb_max ||
			nb_rx_desc < dev_info.rx_desc_lim.nb_min ||
			nb_rx_desc % dev_info.rx_desc_lim.nb_align != 0) {

		RTE_PMD_DEBUG_TRACE("Invalid value for nb_rx_desc(=%hu), "
			"should be: <= %hu, = %hu, and a product of %hu\n",
			nb_rx_desc,
			dev_info.rx_desc_lim.nb_max,
			dev_info.rx_desc_lim.nb_min,
			dev_info.rx_desc_lim.nb_align);
		return -EINVAL;
	}

	rxq = dev->data->rx_queues;
	if (rxq[rx_queue_id]) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->rx_queue_release)(rxq[rx_queue_id]);
		rxq[rx_queue_id] = NULL;
	}

	if (rx_conf == NULL)
		rx_conf = &dev_info.default_rxconf;

	local_conf = *rx_conf;
	if (dev->data->dev_conf.rxmode.ignore_offload_bitfield == 0) {
		/**
		 * Reflect port offloads to queue offloads in order for
		 * offloads to not be discarded.
		 */
		rte_eth_convert_rx_offload_bitfield(&dev->data->dev_conf.rxmode,
						    &local_conf.offloads);
	}

	ret = (*dev->dev_ops->rx_queue_setup)(dev, rx_queue_id, nb_rx_desc,
					      socket_id, &local_conf, mp);
	if (!ret) {
		if (!dev->data->min_rx_buf_size ||
		    dev->data->min_rx_buf_size > mbp_buf_size)
			dev->data->min_rx_buf_size = mbp_buf_size;
	}

	return ret;
}

/**
 * A conversion function from txq_flags API.
 */
static void
rte_eth_convert_txq_flags(const uint32_t txq_flags, uint64_t *tx_offloads)
{
	uint64_t offloads = 0;

	if (!(txq_flags & ETH_TXQ_FLAGS_NOMULTSEGS))
		offloads |= DEV_TX_OFFLOAD_MULTI_SEGS;
	if (!(txq_flags & ETH_TXQ_FLAGS_NOVLANOFFL))
		offloads |= DEV_TX_OFFLOAD_VLAN_INSERT;
	if (!(txq_flags & ETH_TXQ_FLAGS_NOXSUMSCTP))
		offloads |= DEV_TX_OFFLOAD_SCTP_CKSUM;
	if (!(txq_flags & ETH_TXQ_FLAGS_NOXSUMUDP))
		offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
	if (!(txq_flags & ETH_TXQ_FLAGS_NOXSUMTCP))
		offloads |= DEV_TX_OFFLOAD_TCP_CKSUM;
	if ((txq_flags & ETH_TXQ_FLAGS_NOREFCOUNT) &&
	    (txq_flags & ETH_TXQ_FLAGS_NOMULTMEMP))
		offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	*tx_offloads = offloads;
}

/**
 * A conversion function from offloads API.
 */
static void
rte_eth_convert_txq_offloads(const uint64_t tx_offloads, uint32_t *txq_flags)
{
	uint32_t flags = 0;

	if (!(tx_offloads & DEV_TX_OFFLOAD_MULTI_SEGS))
		flags |= ETH_TXQ_FLAGS_NOMULTSEGS;
	if (!(tx_offloads & DEV_TX_OFFLOAD_VLAN_INSERT))
		flags |= ETH_TXQ_FLAGS_NOVLANOFFL;
	if (!(tx_offloads & DEV_TX_OFFLOAD_SCTP_CKSUM))
		flags |= ETH_TXQ_FLAGS_NOXSUMSCTP;
	if (!(tx_offloads & DEV_TX_OFFLOAD_UDP_CKSUM))
		flags |= ETH_TXQ_FLAGS_NOXSUMUDP;
	if (!(tx_offloads & DEV_TX_OFFLOAD_TCP_CKSUM))
		flags |= ETH_TXQ_FLAGS_NOXSUMTCP;
	if (tx_offloads & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		flags |= (ETH_TXQ_FLAGS_NOREFCOUNT | ETH_TXQ_FLAGS_NOMULTMEMP);

	*txq_flags = flags;
}

int
rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf local_conf;
	void **txq;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_setup, -ENOTSUP);

	rte_eth_dev_info_get(port_id, &dev_info);

	if (nb_tx_desc > dev_info.tx_desc_lim.nb_max ||
	    nb_tx_desc < dev_info.tx_desc_lim.nb_min ||
	    nb_tx_desc % dev_info.tx_desc_lim.nb_align != 0) {
		RTE_PMD_DEBUG_TRACE("Invalid value for nb_tx_desc(=%hu), "
				"should be: <= %hu, = %hu, and a product of %hu\n",
				nb_tx_desc,
				dev_info.tx_desc_lim.nb_max,
				dev_info.tx_desc_lim.nb_min,
				dev_info.tx_desc_lim.nb_align);
		return -EINVAL;
	}

	txq = dev->data->tx_queues;
	if (txq[tx_queue_id]) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->tx_queue_release)(txq[tx_queue_id]);
		txq[tx_queue_id] = NULL;
	}

	if (tx_conf == NULL)
		tx_conf = &dev_info.default_txconf;

	/*
	 * Convert between the offloads API to enable PMDs to support
	 * only one of them.
	 */
	local_conf = *tx_conf;
	if (tx_conf->txq_flags & ETH_TXQ_FLAGS_IGNORE) {
		rte_eth_convert_txq_offloads(tx_conf->offloads,
					     &local_conf.txq_flags);
		/* Keep the ignore flag. */
		local_conf.txq_flags |= ETH_TXQ_FLAGS_IGNORE;
	} else {
		rte_eth_convert_txq_flags(tx_conf->txq_flags,
					  &local_conf.offloads);
	}

	return (*dev->dev_ops->tx_queue_setup)(dev, tx_queue_id, nb_tx_desc,
					       socket_id, &local_conf);
}

void
rte_eth_tx_buffer_drop_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata __rte_unused)
{
	unsigned i;

	for (i = 0; i < unsent; i++)
		rte_pktmbuf_free(pkts[i]);
}

void
rte_eth_tx_buffer_count_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata)
{
	uint64_t *count = userdata;
	unsigned i;

	for (i = 0; i < unsent; i++)
		rte_pktmbuf_free(pkts[i]);

	*count += unsent;
}

int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn cbfn, void *userdata)
{
	buffer->error_callback = cbfn;
	buffer->error_userdata = userdata;
	return 0;
}

int
rte_eth_tx_buffer_init(struct rte_eth_dev_tx_buffer *buffer, uint16_t size)
{
	int ret = 0;

	if (buffer == NULL)
		return -EINVAL;

	buffer->size = size;
	if (buffer->error_callback == NULL) {
		ret = rte_eth_tx_buffer_set_err_callback(
			buffer, rte_eth_tx_buffer_drop_callback, NULL);
	}

	return ret;
}

int
rte_eth_tx_done_cleanup(uint16_t port_id, uint16_t queue_id, uint32_t free_cnt)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	/* Validate Input Data. Bail if not valid or not supported. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_done_cleanup, -ENOTSUP);

	/* Call driver to free pending mbufs. */
	return (*dev->dev_ops->tx_done_cleanup)(dev->data->tx_queues[queue_id],
			free_cnt);
}

void
rte_eth_promiscuous_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_enable);
	(*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = 1;
}

void
rte_eth_promiscuous_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_disable);
	dev->data->promiscuous = 0;
	(*dev->dev_ops->promiscuous_disable)(dev);
}

int
rte_eth_promiscuous_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	return dev->data->promiscuous;
}

void
rte_eth_allmulticast_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_enable);
	(*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = 1;
}

void
rte_eth_allmulticast_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_disable);
	dev->data->all_multicast = 0;
	(*dev->dev_ops->allmulticast_disable)(dev);
}

int
rte_eth_allmulticast_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	return dev->data->all_multicast;
}

static inline int
rte_eth_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

void
rte_eth_link_get(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}
}

void
rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}
}

int
rte_eth_stats_get(uint16_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	memset(stats, 0, sizeof(*stats));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	return (*dev->dev_ops->stats_get)(dev, stats);
}

int
rte_eth_stats_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_reset, -ENOTSUP);
	(*dev->dev_ops->stats_reset)(dev);
	dev->data->rx_mbuf_alloc_failed = 0;

	return 0;
}

static inline int
get_xstats_basic_count(struct rte_eth_dev *dev)
{
	uint16_t nb_rxqs, nb_txqs;
	int count;

	nb_rxqs = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	nb_txqs = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	count = RTE_NB_STATS;
	count += nb_rxqs * RTE_NB_RXQ_STATS;
	count += nb_txqs * RTE_NB_TXQ_STATS;

	return count;
}

static int
get_xstats_count(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int count;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	dev = &rte_eth_devices[port_id];
	if (dev->dev_ops->xstats_get_names_by_id != NULL) {
		count = (*dev->dev_ops->xstats_get_names_by_id)(dev, NULL,
				NULL, 0);
		if (count < 0)
			return count;
	}
	if (dev->dev_ops->xstats_get_names != NULL) {
		count = (*dev->dev_ops->xstats_get_names)(dev, NULL, 0);
		if (count < 0)
			return count;
	} else
		count = 0;


	count += get_xstats_basic_count(dev);

	return count;
}

int
rte_eth_xstats_get_id_by_name(uint16_t port_id, const char *xstat_name,
		uint64_t *id)
{
	int cnt_xstats, idx_xstat;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!id) {
		RTE_PMD_DEBUG_TRACE("Error: id pointer is NULL\n");
		return -ENOMEM;
	}

	if (!xstat_name) {
		RTE_PMD_DEBUG_TRACE("Error: xstat_name pointer is NULL\n");
		return -ENOMEM;
	}

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names_by_id(port_id, NULL, 0, NULL);
	if (cnt_xstats  < 0) {
		RTE_PMD_DEBUG_TRACE("Error: Cannot get count of xstats\n");
		return -ENODEV;
	}

	/* Get id-name lookup table */
	struct rte_eth_xstat_name xstats_names[cnt_xstats];

	if (cnt_xstats != rte_eth_xstats_get_names_by_id(
			port_id, xstats_names, cnt_xstats, NULL)) {
		RTE_PMD_DEBUG_TRACE("Error: Cannot get xstats lookup\n");
		return -1;
	}

	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++) {
		if (!strcmp(xstats_names[idx_xstat].name, xstat_name)) {
			*id = idx_xstat;
			return 0;
		};
	}

	return -EINVAL;
}

/* retrieve ethdev extended statistics names */
int
rte_eth_xstats_get_names_by_id(uint16_t port_id,
	struct rte_eth_xstat_name *xstats_names, unsigned int size,
	uint64_t *ids)
{
	struct rte_eth_xstat_name *xstats_names_copy;
	unsigned int no_basic_stat_requested = 1;
	unsigned int expected_entries;
	struct rte_eth_dev *dev;
	unsigned int i;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	ret = get_xstats_count(port_id);
	if (ret < 0)
		return ret;
	expected_entries = (unsigned int)ret;

	/* Return max number of stats if no ids given */
	if (!ids) {
		if (!xstats_names)
			return expected_entries;
		else if (xstats_names && size < expected_entries)
			return expected_entries;
	}

	if (ids && !xstats_names)
		return -EINVAL;

	if (ids && dev->dev_ops->xstats_get_names_by_id != NULL && size > 0) {
		unsigned int basic_count = get_xstats_basic_count(dev);
		uint64_t ids_copy[size];

		for (i = 0; i < size; i++) {
			if (ids[i] < basic_count) {
				no_basic_stat_requested = 0;
				break;
			}

			/*
			 * Convert ids to xstats ids that PMD knows.
			 * ids known by user are basic + extended stats.
			 */
			ids_copy[i] = ids[i] - basic_count;
		}

		if (no_basic_stat_requested)
			return (*dev->dev_ops->xstats_get_names_by_id)(dev,
					xstats_names, ids_copy, size);
	}

	/* Retrieve all stats */
	if (!ids) {
		int num_stats = rte_eth_xstats_get_names(port_id, xstats_names,
				expected_entries);
		if (num_stats < 0 || num_stats > (int)expected_entries)
			return num_stats;
		else
			return expected_entries;
	}

	xstats_names_copy = calloc(expected_entries,
		sizeof(struct rte_eth_xstat_name));

	if (!xstats_names_copy) {
		RTE_PMD_DEBUG_TRACE("ERROR: can't allocate memory");
		return -ENOMEM;
	}

	/* Fill xstats_names_copy structure */
	rte_eth_xstats_get_names(port_id, xstats_names_copy, expected_entries);

	/* Filter stats */
	for (i = 0; i < size; i++) {
		if (ids[i] >= expected_entries) {
			RTE_PMD_DEBUG_TRACE("ERROR: id value isn't valid\n");
			free(xstats_names_copy);
			return -1;
		}
		xstats_names[i] = xstats_names_copy[ids[i]];
	}

	free(xstats_names_copy);
	return size;
}

int
rte_eth_xstats_get_names(uint16_t port_id,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int size)
{
	struct rte_eth_dev *dev;
	int cnt_used_entries;
	int cnt_expected_entries;
	int cnt_driver_entries;
	uint32_t idx, id_queue;
	uint16_t num_q;

	cnt_expected_entries = get_xstats_count(port_id);
	if (xstats_names == NULL || cnt_expected_entries < 0 ||
			(int)size < cnt_expected_entries)
		return cnt_expected_entries;

	/* port_id checked in get_xstats_count() */
	dev = &rte_eth_devices[port_id];
	cnt_used_entries = 0;

	for (idx = 0; idx < RTE_NB_STATS; idx++) {
		snprintf(xstats_names[cnt_used_entries].name,
			sizeof(xstats_names[0].name),
			"%s", rte_stats_strings[idx].name);
		cnt_used_entries++;
	}
	num_q = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (id_queue = 0; id_queue < num_q; id_queue++) {
		for (idx = 0; idx < RTE_NB_RXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"rx_q%u%s",
				id_queue, rte_rxq_stats_strings[idx].name);
			cnt_used_entries++;
		}

	}
	num_q = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (id_queue = 0; id_queue < num_q; id_queue++) {
		for (idx = 0; idx < RTE_NB_TXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"tx_q%u%s",
				id_queue, rte_txq_stats_strings[idx].name);
			cnt_used_entries++;
		}
	}

	if (dev->dev_ops->xstats_get_names != NULL) {
		/* If there are any driver-specific xstats, append them
		 * to end of list.
		 */
		cnt_driver_entries = (*dev->dev_ops->xstats_get_names)(
			dev,
			xstats_names + cnt_used_entries,
			size - cnt_used_entries);
		if (cnt_driver_entries < 0)
			return cnt_driver_entries;
		cnt_used_entries += cnt_driver_entries;
	}

	return cnt_used_entries;
}

/* retrieve ethdev extended statistics */
int
rte_eth_xstats_get_by_id(uint16_t port_id, const uint64_t *ids,
			 uint64_t *values, unsigned int size)
{
	unsigned int no_basic_stat_requested = 1;
	unsigned int num_xstats_filled;
	uint16_t expected_entries;
	struct rte_eth_dev *dev;
	unsigned int i;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	expected_entries = get_xstats_count(port_id);
	struct rte_eth_xstat xstats[expected_entries];
	dev = &rte_eth_devices[port_id];

	/* Return max number of stats if no ids given */
	if (!ids) {
		if (!values)
			return expected_entries;
		else if (values && size < expected_entries)
			return expected_entries;
	}

	if (ids && !values)
		return -EINVAL;

	if (ids && dev->dev_ops->xstats_get_by_id != NULL && size) {
		unsigned int basic_count = get_xstats_basic_count(dev);
		uint64_t ids_copy[size];

		for (i = 0; i < size; i++) {
			if (ids[i] < basic_count) {
				no_basic_stat_requested = 0;
				break;
			}

			/*
			 * Convert ids to xstats ids that PMD knows.
			 * ids known by user are basic + extended stats.
			 */
			ids_copy[i] = ids[i] - basic_count;
		}

		if (no_basic_stat_requested)
			return (*dev->dev_ops->xstats_get_by_id)(dev, ids_copy,
					values, size);
	}

	/* Fill the xstats structure */
	ret = rte_eth_xstats_get(port_id, xstats, expected_entries);
	if (ret < 0)
		return ret;
	num_xstats_filled = (unsigned int)ret;

	/* Return all stats */
	if (!ids) {
		for (i = 0; i < num_xstats_filled; i++)
			values[i] = xstats[i].value;
		return expected_entries;
	}

	/* Filter stats */
	for (i = 0; i < size; i++) {
		if (ids[i] >= expected_entries) {
			RTE_PMD_DEBUG_TRACE("ERROR: id value isn't valid\n");
			return -1;
		}
		values[i] = xstats[ids[i]].value;
	}
	return size;
}

int
rte_eth_xstats_get(uint16_t port_id, struct rte_eth_xstat *xstats,
	unsigned int n)
{
	struct rte_eth_stats eth_stats;
	struct rte_eth_dev *dev;
	unsigned int count = 0, i, q;
	signed int xcount = 0;
	uint64_t val, *stats_ptr;
	uint16_t nb_rxqs, nb_txqs;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	nb_rxqs = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	nb_txqs = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	/* Return generic statistics */
	count = RTE_NB_STATS + (nb_rxqs * RTE_NB_RXQ_STATS) +
		(nb_txqs * RTE_NB_TXQ_STATS);

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL) {
		/* Retrieve the xstats from the driver at the end of the
		 * xstats struct.
		 */
		xcount = (*dev->dev_ops->xstats_get)(dev,
				     xstats ? xstats + count : NULL,
				     (n > count) ? n - count : 0);

		if (xcount < 0)
			return xcount;
	}

	if (n < count + xcount || xstats == NULL)
		return count + xcount;

	/* now fill the xstats structure */
	count = 0;
	rte_eth_stats_get(port_id, &eth_stats);

	/* global stats */
	for (i = 0; i < RTE_NB_STATS; i++) {
		stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_stats_strings[i].offset);
		val = *stats_ptr;
		xstats[count++].value = val;
	}

	/* per-rxq stats */
	for (q = 0; q < nb_rxqs; q++) {
		for (i = 0; i < RTE_NB_RXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_rxq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}

	/* per-txq stats */
	for (q = 0; q < nb_txqs; q++) {
		for (i = 0; i < RTE_NB_TXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_txq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}

	for (i = 0; i < count; i++)
		xstats[i].id = i;
	/* add an offset to driver-specific stats */
	for ( ; i < count + xcount; i++)
		xstats[i].id += count;

	return count + xcount;
}

/* reset ethdev extended statistics */
void
rte_eth_xstats_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_reset != NULL) {
		(*dev->dev_ops->xstats_reset)(dev);
		return;
	}

	/* fallback to default */
	rte_eth_stats_reset(port_id);
}

static int
set_queue_stats_mapping(uint16_t port_id, uint16_t queue_id, uint8_t stat_idx,
		uint8_t is_rx)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_stats_mapping_set, -ENOTSUP);
	return (*dev->dev_ops->queue_stats_mapping_set)
			(dev, queue_id, stat_idx, is_rx);
}


int
rte_eth_dev_set_tx_queue_stats_mapping(uint16_t port_id, uint16_t tx_queue_id,
		uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, tx_queue_id, stat_idx,
			STAT_QMAP_TX);
}


int
rte_eth_dev_set_rx_queue_stats_mapping(uint16_t port_id, uint16_t rx_queue_id,
		uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, rx_queue_id, stat_idx,
			STAT_QMAP_RX);
}

int
rte_eth_dev_fw_version_get(uint16_t port_id, char *fw_version, size_t fw_size)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fw_version_get, -ENOTSUP);
	return (*dev->dev_ops->fw_version_get)(dev, fw_version, fw_size);
}

void
rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info *dev_info)
{
	struct rte_eth_dev *dev;
	const struct rte_eth_desc_lim lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
	};

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	memset(dev_info, 0, sizeof(struct rte_eth_dev_info));
	dev_info->rx_desc_lim = lim;
	dev_info->tx_desc_lim = lim;

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);
	dev_info->driver_name = dev->device->driver->name;
	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;
}

int
rte_eth_dev_get_supported_ptypes(uint16_t port_id, uint32_t ptype_mask,
				 uint32_t *ptypes, int num)
{
	int i, j;
	struct rte_eth_dev *dev;
	const uint32_t *all_ptypes;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_supported_ptypes_get, 0);
	all_ptypes = (*dev->dev_ops->dev_supported_ptypes_get)(dev);

	if (!all_ptypes)
		return 0;

	for (i = 0, j = 0; all_ptypes[i] != RTE_PTYPE_UNKNOWN; ++i)
		if (all_ptypes[i] & ptype_mask) {
			if (j < num)
				ptypes[j] = all_ptypes[i];
			j++;
		}

	return j;
}

void
rte_eth_macaddr_get(uint16_t port_id, struct ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];
	ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);
}


int
rte_eth_dev_get_mtu(uint16_t port_id, uint16_t *mtu)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	*mtu = dev->data->mtu;
	return 0;
}

int
rte_eth_dev_set_mtu(uint16_t port_id, uint16_t mtu)
{
	int ret;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mtu_set, -ENOTSUP);

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (!ret)
		dev->data->mtu = mtu;

	return ret;
}

int
rte_eth_dev_vlan_filter(uint16_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (!(dev->data->dev_conf.rxmode.offloads &
	      DEV_RX_OFFLOAD_VLAN_FILTER)) {
		RTE_PMD_DEBUG_TRACE("port %d: vlan-filtering disabled\n", port_id);
		return -ENOSYS;
	}

	if (vlan_id > 4095) {
		RTE_PMD_DEBUG_TRACE("(port_id=%d) invalid vlan_id=%u > 4095\n",
				port_id, (unsigned) vlan_id);
		return -EINVAL;
	}
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_filter_set, -ENOTSUP);

	ret = (*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
	if (ret == 0) {
		struct rte_vlan_filter_conf *vfc;
		int vidx;
		int vbit;

		vfc = &dev->data->vlan_filter_conf;
		vidx = vlan_id / 64;
		vbit = vlan_id % 64;

		if (on)
			vfc->ids[vidx] |= UINT64_C(1) << vbit;
		else
			vfc->ids[vidx] &= ~(UINT64_C(1) << vbit);
	}

	return ret;
}

int
rte_eth_dev_set_vlan_strip_on_queue(uint16_t port_id, uint16_t rx_queue_id,
				    int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid rx_queue_id=%d\n", port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_strip_queue_set, -ENOTSUP);
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id, on);

	return 0;
}

int
rte_eth_dev_set_vlan_ether_type(uint16_t port_id,
				enum rte_vlan_type vlan_type,
				uint16_t tpid)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_tpid_set, -ENOTSUP);

	return (*dev->dev_ops->vlan_tpid_set)(dev, vlan_type, tpid);
}

int
rte_eth_dev_set_vlan_offload(uint16_t port_id, int offload_mask)
{
	struct rte_eth_dev *dev;
	int ret = 0;
	int mask = 0;
	int cur, org = 0;
	uint64_t orig_offloads;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	/* save original values in case of failure */
	orig_offloads = dev->data->dev_conf.rxmode.offloads;

	/*check which option changed by application*/
	cur = !!(offload_mask & ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.offloads &
		 DEV_RX_OFFLOAD_VLAN_STRIP);
	if (cur != org) {
		if (cur)
			dev->data->dev_conf.rxmode.offloads |=
				DEV_RX_OFFLOAD_VLAN_STRIP;
		else
			dev->data->dev_conf.rxmode.offloads &=
				~DEV_RX_OFFLOAD_VLAN_STRIP;
		mask |= ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.offloads &
		 DEV_RX_OFFLOAD_VLAN_FILTER);
	if (cur != org) {
		if (cur)
			dev->data->dev_conf.rxmode.offloads |=
				DEV_RX_OFFLOAD_VLAN_FILTER;
		else
			dev->data->dev_conf.rxmode.offloads &=
				~DEV_RX_OFFLOAD_VLAN_FILTER;
		mask |= ETH_VLAN_FILTER_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.offloads &
		 DEV_RX_OFFLOAD_VLAN_EXTEND);
	if (cur != org) {
		if (cur)
			dev->data->dev_conf.rxmode.offloads |=
				DEV_RX_OFFLOAD_VLAN_EXTEND;
		else
			dev->data->dev_conf.rxmode.offloads &=
				~DEV_RX_OFFLOAD_VLAN_EXTEND;
		mask |= ETH_VLAN_EXTEND_MASK;
	}

	/*no change*/
	if (mask == 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_offload_set, -ENOTSUP);

	/*
	 * Convert to the offload bitfield API just in case the underlying PMD
	 * still supporting it.
	 */
	rte_eth_convert_rx_offloads(dev->data->dev_conf.rxmode.offloads,
				    &dev->data->dev_conf.rxmode);
	ret = (*dev->dev_ops->vlan_offload_set)(dev, mask);
	if (ret) {
		/* hit an error restore  original values */
		dev->data->dev_conf.rxmode.offloads = orig_offloads;
		rte_eth_convert_rx_offloads(dev->data->dev_conf.rxmode.offloads,
					    &dev->data->dev_conf.rxmode);
	}

	return ret;
}

int
rte_eth_dev_get_vlan_offload(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.rxmode.offloads &
	    DEV_RX_OFFLOAD_VLAN_STRIP)
		ret |= ETH_VLAN_STRIP_OFFLOAD;

	if (dev->data->dev_conf.rxmode.offloads &
	    DEV_RX_OFFLOAD_VLAN_FILTER)
		ret |= ETH_VLAN_FILTER_OFFLOAD;

	if (dev->data->dev_conf.rxmode.offloads &
	    DEV_RX_OFFLOAD_VLAN_EXTEND)
		ret |= ETH_VLAN_EXTEND_OFFLOAD;

	return ret;
}

int
rte_eth_dev_set_vlan_pvid(uint16_t port_id, uint16_t pvid, int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_pvid_set, -ENOTSUP);
	(*dev->dev_ops->vlan_pvid_set)(dev, pvid, on);

	return 0;
}

int
rte_eth_dev_flow_ctrl_get(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_get, -ENOTSUP);
	memset(fc_conf, 0, sizeof(*fc_conf));
	return (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf);
}

int
rte_eth_dev_flow_ctrl_set(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		RTE_PMD_DEBUG_TRACE("Invalid send_xon, only 0/1 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_set, -ENOTSUP);
	return (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf);
}

int
rte_eth_dev_priority_flow_ctrl_set(uint16_t port_id,
				   struct rte_eth_pfc_conf *pfc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (pfc_conf->priority > (ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		RTE_PMD_DEBUG_TRACE("Invalid priority, only 0-7 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	/* High water, low water validation are device specific */
	if  (*dev->dev_ops->priority_flow_ctrl_set)
		return (*dev->dev_ops->priority_flow_ctrl_set)(dev, pfc_conf);
	return -ENOTSUP;
}

static int
rte_eth_check_reta_mask(struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	uint16_t i, num;

	if (!reta_conf)
		return -EINVAL;

	num = (reta_size + RTE_RETA_GROUP_SIZE - 1) / RTE_RETA_GROUP_SIZE;
	for (i = 0; i < num; i++) {
		if (reta_conf[i].mask)
			return 0;
	}

	return -EINVAL;
}

static int
rte_eth_check_reta_entry(struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size,
			 uint16_t max_rxq)
{
	uint16_t i, idx, shift;

	if (!reta_conf)
		return -EINVAL;

	if (max_rxq == 0) {
		RTE_PMD_DEBUG_TRACE("No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) &&
			(reta_conf[idx].reta[shift] >= max_rxq)) {
			RTE_PMD_DEBUG_TRACE("reta_conf[%u]->reta[%u]: %u exceeds "
				"the maximum rxq index: %u\n", idx, shift,
				reta_conf[idx].reta[shift], max_rxq);
			return -EINVAL;
		}
	}

	return 0;
}

int
rte_eth_dev_rss_reta_update(uint16_t port_id,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	/* Check mask bits */
	ret = rte_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];

	/* Check entry value */
	ret = rte_eth_check_reta_entry(reta_conf, reta_size,
				dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_update, -ENOTSUP);
	return (*dev->dev_ops->reta_update)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_reta_query(uint16_t port_id,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	/* Check mask bits */
	ret = rte_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_query, -ENOTSUP);
	return (*dev->dev_ops->reta_query)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_hash_update(uint16_t port_id,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_update, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_update)(dev, rss_conf);
}

int
rte_eth_dev_rss_hash_conf_get(uint16_t port_id,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_conf_get, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_conf_get)(dev, rss_conf);
}

int
rte_eth_dev_udp_tunnel_port_add(uint16_t port_id,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (udp_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_add, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_port_add)(dev, udp_tunnel);
}

int
rte_eth_dev_udp_tunnel_port_delete(uint16_t port_id,
				   struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_del, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_port_del)(dev, udp_tunnel);
}

int
rte_eth_led_on(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_on, -ENOTSUP);
	return (*dev->dev_ops->dev_led_on)(dev);
}

int
rte_eth_led_off(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_off, -ENOTSUP);
	return (*dev->dev_ops->dev_led_off)(dev);
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
get_mac_addr_index(uint16_t port_id, const struct ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	rte_eth_dev_info_get(port_id, &dev_info);

	for (i = 0; i < dev_info.max_mac_addrs; i++)
		if (memcmp(addr, &dev->data->mac_addrs[i], ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

static const struct ether_addr null_mac_addr;

int
rte_eth_dev_mac_addr_add(uint16_t port_id, struct ether_addr *addr,
			uint32_t pool)
{
	struct rte_eth_dev *dev;
	int index;
	uint64_t pool_mask;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_add, -ENOTSUP);

	if (is_zero_ether_addr(addr)) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}
	if (pool >= ETH_64_POOLS) {
		RTE_PMD_DEBUG_TRACE("pool id must be 0-%d\n", ETH_64_POOLS - 1);
		return -EINVAL;
	}

	index = get_mac_addr_index(port_id, addr);
	if (index < 0) {
		index = get_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_PMD_DEBUG_TRACE("port %d: MAC address array full\n",
				port_id);
			return -ENOSPC;
		}
	} else {
		pool_mask = dev->data->mac_pool_sel[index];

		/* Check if both MAC address and pool is already there, and do nothing */
		if (pool_mask & (1ULL << pool))
			return 0;
	}

	/* Update NIC */
	ret = (*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	if (ret == 0) {
		/* Update address in NIC data structure */
		ether_addr_copy(addr, &dev->data->mac_addrs[index]);

		/* Update pool bitmap in NIC data structure */
		dev->data->mac_pool_sel[index] |= (1ULL << pool);
	}

	return ret;
}

int
rte_eth_dev_mac_addr_remove(uint16_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_remove, -ENOTSUP);

	index = get_mac_addr_index(port_id, addr);
	if (index == 0) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot remove default MAC address\n", port_id);
		return -EADDRINUSE;
	} else if (index < 0)
		return 0;  /* Do nothing if address wasn't found */

	/* Update NIC */
	(*dev->dev_ops->mac_addr_remove)(dev, index);

	/* Update address in NIC data structure */
	ether_addr_copy(&null_mac_addr, &dev->data->mac_addrs[index]);

	/* reset pool bitmap */
	dev->data->mac_pool_sel[index] = 0;

	return 0;
}

int
rte_eth_dev_default_mac_addr_set(uint16_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_set, -ENOTSUP);

	/* Update default address in NIC data structure */
	ether_addr_copy(addr, &dev->data->mac_addrs[0]);

	(*dev->dev_ops->mac_addr_set)(dev, addr);

	return 0;
}


/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
get_hash_mac_addr_index(uint16_t port_id, const struct ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;

	rte_eth_dev_info_get(port_id, &dev_info);
	if (!dev->data->hash_mac_addrs)
		return -1;

	for (i = 0; i < dev_info.max_hash_mac_addrs; i++)
		if (memcmp(addr, &dev->data->hash_mac_addrs[i],
			ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

int
rte_eth_dev_uc_hash_table_set(uint16_t port_id, struct ether_addr *addr,
				uint8_t on)
{
	int index;
	int ret;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (is_zero_ether_addr(addr)) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}

	index = get_hash_mac_addr_index(port_id, addr);
	/* Check if it's already there, and do nothing */
	if ((index >= 0) && (on))
		return 0;

	if (index < 0) {
		if (!on) {
			RTE_PMD_DEBUG_TRACE("port %d: the MAC address was not "
				"set in UTA\n", port_id);
			return -EINVAL;
		}

		index = get_hash_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_PMD_DEBUG_TRACE("port %d: MAC address array full\n",
					port_id);
			return -ENOSPC;
		}
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_hash_table_set, -ENOTSUP);
	ret = (*dev->dev_ops->uc_hash_table_set)(dev, addr, on);
	if (ret == 0) {
		/* Update address in NIC data structure */
		if (on)
			ether_addr_copy(addr,
					&dev->data->hash_mac_addrs[index]);
		else
			ether_addr_copy(&null_mac_addr,
					&dev->data->hash_mac_addrs[index]);
	}

	return ret;
}

int
rte_eth_dev_uc_all_hash_table_set(uint16_t port_id, uint8_t on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_all_hash_table_set, -ENOTSUP);
	return (*dev->dev_ops->uc_all_hash_table_set)(dev, on);
}

int rte_eth_set_queue_rate_limit(uint16_t port_id, uint16_t queue_idx,
					uint16_t tx_rate)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (queue_idx > dev_info.max_tx_queues) {
		RTE_PMD_DEBUG_TRACE("set queue rate limit:port %d: "
				"invalid queue id=%d\n", port_id, queue_idx);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		RTE_PMD_DEBUG_TRACE("set queue rate limit:invalid tx_rate=%d, "
				"bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_queue_rate_limit, -ENOTSUP);
	return (*dev->dev_ops->set_queue_rate_limit)(dev, queue_idx, tx_rate);
}

int
rte_eth_mirror_rule_set(uint16_t port_id,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t rule_id, uint8_t on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (mirror_conf->rule_type == 0) {
		RTE_PMD_DEBUG_TRACE("mirror rule type can not be 0.\n");
		return -EINVAL;
	}

	if (mirror_conf->dst_pool >= ETH_64_POOLS) {
		RTE_PMD_DEBUG_TRACE("Invalid dst pool, pool id must be 0-%d\n",
				ETH_64_POOLS - 1);
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & (ETH_MIRROR_VIRTUAL_POOL_UP |
	     ETH_MIRROR_VIRTUAL_POOL_DOWN)) &&
	    (mirror_conf->pool_mask == 0)) {
		RTE_PMD_DEBUG_TRACE("Invalid mirror pool, pool mask can not be 0.\n");
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & ETH_MIRROR_VLAN) &&
	    mirror_conf->vlan.vlan_mask == 0) {
		RTE_PMD_DEBUG_TRACE("Invalid vlan mask, vlan mask can not be 0.\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_set, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_set)(dev, mirror_conf, rule_id, on);
}

int
rte_eth_mirror_rule_reset(uint16_t port_id, uint8_t rule_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_reset, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_reset)(dev, rule_id);
}

int
rte_eth_dev_callback_register(uint16_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *user_cb;

	if (!cb_fn)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);

	TAILQ_FOREACH(user_cb, &(dev->link_intr_cbs), next) {
		if (user_cb->cb_fn == cb_fn &&
			user_cb->cb_arg == cb_arg &&
			user_cb->event == event) {
			break;
		}
	}

	/* create a new callback. */
	if (user_cb == NULL) {
		user_cb = rte_zmalloc("INTR_USER_CALLBACK",
					sizeof(struct rte_eth_dev_callback), 0);
		if (user_cb != NULL) {
			user_cb->cb_fn = cb_fn;
			user_cb->cb_arg = cb_arg;
			user_cb->event = event;
			TAILQ_INSERT_TAIL(&(dev->link_intr_cbs), user_cb, next);
		}
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return (user_cb == NULL) ? -ENOMEM : 0;
}

int
rte_eth_dev_callback_unregister(uint16_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *cb, *next;

	if (!cb_fn)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);

	ret = 0;
	for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);

		if (cb->cb_fn != cb_fn || cb->event != event ||
				(cb->cb_arg != (void *)-1 &&
				cb->cb_arg != cb_arg))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (cb->active == 0) {
			TAILQ_REMOVE(&(dev->link_intr_cbs), cb, next);
			rte_free(cb);
		} else {
			ret = -EAGAIN;
		}
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return ret;
}

int
_rte_eth_dev_callback_process(struct rte_eth_dev *dev,
	enum rte_eth_event_type event, void *cb_arg, void *ret_param)
{
	struct rte_eth_dev_callback *cb_lst;
	struct rte_eth_dev_callback dev_cb;
	int rc = 0;

	rte_spinlock_lock(&rte_eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->link_intr_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		if (cb_arg != NULL)
			dev_cb.cb_arg = cb_arg;
		if (ret_param != NULL)
			dev_cb.ret_param = ret_param;

		rte_spinlock_unlock(&rte_eth_dev_cb_lock);
		rc = dev_cb.cb_fn(dev->data->port_id, dev_cb.event,
				dev_cb.cb_arg, dev_cb.ret_param);
		rte_spinlock_lock(&rte_eth_dev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return rc;
}

int
rte_eth_dev_rx_intr_ctl(uint16_t port_id, int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	uint16_t qid;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	if (!dev->intr_handle) {
		RTE_PMD_DEBUG_TRACE("RX Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_PMD_DEBUG_TRACE("RX Intr vector unset\n");
		return -EPERM;
	}

	for (qid = 0; qid < dev->data->nb_rx_queues; qid++) {
		vec = intr_handle->intr_vec[qid];
		rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
		if (rc && rc != -EEXIST) {
			RTE_PMD_DEBUG_TRACE("p %u q %u rx ctl error"
					" op %d epfd %d vec %u\n",
					port_id, qid, op, epfd, vec);
		}
	}

	return 0;
}

const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *dev, const char *ring_name,
			 uint16_t queue_id, size_t size, unsigned align,
			 int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
		 dev->device->driver->name, ring_name,
		 dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, size, socket_id, 0, align);
}

int
rte_eth_dev_rx_intr_ctl_q(uint16_t port_id, uint16_t queue_id,
			  int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (!dev->intr_handle) {
		RTE_PMD_DEBUG_TRACE("RX Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_PMD_DEBUG_TRACE("RX Intr vector unset\n");
		return -EPERM;
	}

	vec = intr_handle->intr_vec[queue_id];
	rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
	if (rc && rc != -EEXIST) {
		RTE_PMD_DEBUG_TRACE("p %u q %u rx ctl error"
				" op %d epfd %d vec %u\n",
				port_id, queue_id, op, epfd, vec);
		return rc;
	}

	return 0;
}

int
rte_eth_dev_rx_intr_enable(uint16_t port_id,
			   uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_enable, -ENOTSUP);
	return (*dev->dev_ops->rx_queue_intr_enable)(dev, queue_id);
}

int
rte_eth_dev_rx_intr_disable(uint16_t port_id,
			    uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_disable, -ENOTSUP);
	return (*dev->dev_ops->rx_queue_intr_disable)(dev, queue_id);
}


int
rte_eth_dev_filter_supported(uint16_t port_id,
			     enum rte_filter_type filter_type)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type,
				RTE_ETH_FILTER_NOP, NULL);
}

int
rte_eth_dev_filter_ctrl(uint16_t port_id, enum rte_filter_type filter_type,
		       enum rte_filter_op filter_op, void *arg)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type, filter_op, arg);
}

void *
rte_eth_add_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}
	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];

	if (!tail) {
		rte_eth_devices[port_id].post_rx_burst_cbs[queue_id] = cb;

	} else {
		while (tail->next)
			tail = tail->next;
		tail->next = cb;
	}
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return cb;
}

void *
rte_eth_add_first_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	/* Add the callbacks at fisrt position*/
	cb->next = rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];
	rte_smp_wmb();
	rte_eth_devices[port_id].post_rx_burst_cbs[queue_id] = cb;
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return cb;
}

void *
rte_eth_add_tx_callback(uint16_t port_id, uint16_t queue_id,
		rte_tx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_tx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.tx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_tx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id];

	if (!tail) {
		rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id] = cb;

	} else {
		while (tail->next)
			tail = tail->next;
		tail->next = cb;
	}
	rte_spinlock_unlock(&rte_eth_tx_cb_lock);

	return cb;
}

int
rte_eth_remove_rx_callback(uint16_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_rx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;
	int ret = -EINVAL;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	prev_cb = &dev->post_rx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			*prev_cb = cb->next;
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return ret;
}

int
rte_eth_remove_tx_callback(uint16_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_tx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret = -EINVAL;
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;

	rte_spinlock_lock(&rte_eth_tx_cb_lock);
	prev_cb = &dev->pre_tx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			*prev_cb = cb->next;
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&rte_eth_tx_cb_lock);

	return ret;
}

int
rte_eth_rx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rxq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->rxq_info_get(dev, queue_id, qinfo);
	return 0;
}

int
rte_eth_tx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->txq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->txq_info_get(dev, queue_id, qinfo);
	return 0;
}

int
rte_eth_dev_set_mc_addr_list(uint16_t port_id,
			     struct ether_addr *mc_addr_set,
			     uint32_t nb_mc_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_mc_addr_list, -ENOTSUP);
	return dev->dev_ops->set_mc_addr_list(dev, mc_addr_set, nb_mc_addr);
}

int
rte_eth_timesync_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_enable, -ENOTSUP);
	return (*dev->dev_ops->timesync_enable)(dev);
}

int
rte_eth_timesync_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_disable, -ENOTSUP);
	return (*dev->dev_ops->timesync_disable)(dev);
}

int
rte_eth_timesync_read_rx_timestamp(uint16_t port_id, struct timespec *timestamp,
				   uint32_t flags)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_rx_timestamp, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_rx_timestamp)(dev, timestamp, flags);
}

int
rte_eth_timesync_read_tx_timestamp(uint16_t port_id,
				   struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_tx_timestamp, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_tx_timestamp)(dev, timestamp);
}

int
rte_eth_timesync_adjust_time(uint16_t port_id, int64_t delta)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_adjust_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_adjust_time)(dev, delta);
}

int
rte_eth_timesync_read_time(uint16_t port_id, struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_time)(dev, timestamp);
}

int
rte_eth_timesync_write_time(uint16_t port_id, const struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_write_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_write_time)(dev, timestamp);
}

int
rte_eth_dev_get_reg_info(uint16_t port_id, struct rte_dev_reg_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_reg, -ENOTSUP);
	return (*dev->dev_ops->get_reg)(dev, info);
}

int
rte_eth_dev_get_eeprom_length(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom_length, -ENOTSUP);
	return (*dev->dev_ops->get_eeprom_length)(dev);
}

int
rte_eth_dev_get_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom, -ENOTSUP);
	return (*dev->dev_ops->get_eeprom)(dev, info);
}

int
rte_eth_dev_set_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_eeprom, -ENOTSUP);
	return (*dev->dev_ops->set_eeprom)(dev, info);
}

int
rte_eth_dev_get_dcb_info(uint16_t port_id,
			     struct rte_eth_dcb_info *dcb_info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	memset(dcb_info, 0, sizeof(struct rte_eth_dcb_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_dcb_info, -ENOTSUP);
	return (*dev->dev_ops->get_dcb_info)(dev, dcb_info);
}

int
rte_eth_dev_l2_tunnel_eth_type_conf(uint16_t port_id,
				    struct rte_eth_l2_tunnel_conf *l2_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (l2_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid l2_tunnel parameter\n");
		return -EINVAL;
	}

	if (l2_tunnel->l2_tunnel_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->l2_tunnel_eth_type_conf,
				-ENOTSUP);
	return (*dev->dev_ops->l2_tunnel_eth_type_conf)(dev, l2_tunnel);
}

int
rte_eth_dev_l2_tunnel_offload_set(uint16_t port_id,
				  struct rte_eth_l2_tunnel_conf *l2_tunnel,
				  uint32_t mask,
				  uint8_t en)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (l2_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid l2_tunnel parameter\n");
		return -EINVAL;
	}

	if (l2_tunnel->l2_tunnel_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type.\n");
		return -EINVAL;
	}

	if (mask == 0) {
		RTE_PMD_DEBUG_TRACE("Mask should have a value.\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->l2_tunnel_offload_set,
				-ENOTSUP);
	return (*dev->dev_ops->l2_tunnel_offload_set)(dev, l2_tunnel, mask, en);
}

static void
rte_eth_dev_adjust_nb_desc(uint16_t *nb_desc,
			   const struct rte_eth_desc_lim *desc_lim)
{
	if (desc_lim->nb_align != 0)
		*nb_desc = RTE_ALIGN_CEIL(*nb_desc, desc_lim->nb_align);

	if (desc_lim->nb_max != 0)
		*nb_desc = RTE_MIN(*nb_desc, desc_lim->nb_max);

	*nb_desc = RTE_MAX(*nb_desc, desc_lim->nb_min);
}

int
rte_eth_dev_adjust_nb_rx_tx_desc(uint16_t port_id,
				 uint16_t *nb_rx_desc,
				 uint16_t *nb_tx_desc)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);

	rte_eth_dev_info_get(port_id, &dev_info);

	if (nb_rx_desc != NULL)
		rte_eth_dev_adjust_nb_desc(nb_rx_desc, &dev_info.rx_desc_lim);

	if (nb_tx_desc != NULL)
		rte_eth_dev_adjust_nb_desc(nb_tx_desc, &dev_info.tx_desc_lim);

	return 0;
}

int
rte_eth_dev_pool_ops_supported(uint16_t port_id, const char *pool)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (pool == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->pool_ops_supported == NULL)
		return 1; /* all pools are supported */

	return (*dev->dev_ops->pool_ops_supported)(dev, pool);
}
