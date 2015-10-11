/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef __MOCK_FILE_RTE_ETHDEV_H__
#define __MOCK_FILE_RTE_ETHDEV_H__

#include <string.h>

struct rte_eth_stats {
    uint64_t obytes;
    uint64_t ibytes;
    uint64_t opackets;
    uint64_t ipackets;
};

static inline void
rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats) {
    memset(stats, 0, sizeof(rte_eth_stats));
}

static inline uint16_t
rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
                 struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
    return (0);
}

#endif /* __MOCK_FILE_RTE_ETHDEV_H__ */
