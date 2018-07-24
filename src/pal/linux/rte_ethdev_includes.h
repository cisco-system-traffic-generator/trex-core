#ifndef __RTE_ETHDEV_INCLUDES_H__
#define __RTE_ETHDEV_INCLUDES_H__

struct rte_eth_link {
    int link_autoneg;
    int link_speed;
    int link_duplex;
    int link_status;
};


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

#endif /* __RTE_ETHDEV_INCLUDES_H__ */
