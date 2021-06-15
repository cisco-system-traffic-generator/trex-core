#ifndef __RTE_ETHDEV_INCLUDES_H__
#define __RTE_ETHDEV_INCLUDES_H__

struct rte_eth_link {
    int link_autoneg;
    int link_speed;
    int link_duplex;
    int link_status;
};

#define ETH_SPEED_NUM_100G    100000 /**< 100 Gbps */

/**
 * Force a structure to be packed
 */
#define __rte_packed __attribute__((__packed__))

enum rte_eth_fc_mode {
};

struct rte_eth_xstat {
};

struct rte_eth_xstat_name {
};

struct rte_eth_fc_conf {
};

struct rte_eth_dev_info {
};

struct rte_pci_device {
};

static inline int
rte_eth_timesync_read_rx_timestamp(uint16_t port_id, struct timespec *timestamp,
				   uint32_t flags){
    return 0;
}

#endif /* __RTE_ETHDEV_INCLUDES_H__ */
