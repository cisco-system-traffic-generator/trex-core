/*
  Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#ifndef MAIN_DPDK_H
#define MAIN_DPDK_H

#include <rte_ethdev.h>
#include "pre_test.h"
#include "bp_sim.h"
#include "dpdk_port_map.h"

enum {
    MAIN_DPDK_DROP_Q = 0,
    MAIN_DPDK_RX_Q = 1,
};

class CTrexDpdkParams {
 public:
    uint16_t rx_data_q_num;
    uint16_t rx_drop_q_num;
    uint16_t rx_desc_num_data_q;
    uint16_t rx_desc_num_drop_q;
    uint16_t tx_desc_num;
    uint16_t rx_mbuf_type;
};

// These are statistics for packets we send, and do not expect to get back (Like ARP)
// We reduce them from general statistics we report (and report them separately, so we can keep the assumption
// that tx_pkts == rx_pkts and tx_bytes==rx_bytes
class CPhyEthIgnoreStats {
    friend class CPhyEthIF;

 public:
    uint64_t get_rx_arp() {return m_rx_arp;}
    uint64_t get_tx_arp() {return m_tx_arp;}
 private:
    uint64_t ipackets;  /**< Total number of successfully received packets. */
    uint64_t ibytes;    /**< Total number of successfully received bytes. */
    uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
    uint64_t obytes;    /**< Total number of successfully transmitted bytes. */
    uint64_t m_tx_arp;    /**< Total number of successfully transmitted ARP packets */
    uint64_t m_rx_arp;    /**< Total number of successfully received ARP packets */

 private:
    void dump(FILE *fd);
};

class CPhyEthIFStats {
 public:
    uint64_t ipackets;  /**< Total number of successfully received packets. */
    uint64_t ibytes;    /**< Total number of successfully received bytes. */
    uint64_t f_ipackets;  /**< Total number of successfully received packets - filter SCTP*/
    uint64_t f_ibytes;    /**< Total number of successfully received bytes. - filter SCTP */
    uint64_t opackets;  /**< Total number of successfully transmitted packets.*/
    uint64_t obytes;    /**< Total number of successfully transmitted bytes. */
    uint64_t ierrors;   /**< Total number of erroneous received packets. */
    uint64_t oerrors;   /**< Total number of failed transmitted packets. */
    uint64_t imcasts;   /**< Total number of multicast received packets. */
    uint64_t rx_nombuf; /**< Total number of RX mbuf allocation failures. */
    struct rte_eth_stats m_prev_stats;
    uint64_t m_rx_per_flow_pkts [MAX_FLOW_STATS]; // Per flow RX pkts
    uint64_t m_rx_per_flow_bytes[MAX_FLOW_STATS]; // Per flow RX bytes
    // Previous fdir stats values read from driver. Since on xl710 this is 32 bit, we save old value, to handle wrap around.
    uint32_t  m_fdir_prev_pkts [MAX_FLOW_STATS];
    uint32_t  m_fdir_prev_bytes [MAX_FLOW_STATS];
 public:
    void Clear();
    void Dump(FILE *fd);
    void DumpAll(FILE *fd);
};

class CPhyEthIF  {
 public:
    CPhyEthIF (){
        m_tvpid          = DPDK_MAP_IVALID_REPID;
        m_repid          = DPDK_MAP_IVALID_REPID;
        m_rx_queue       = 0;
        m_stats_err_cnt  = 0;
    }
    bool Create(tvpid_t  tvpid,
                repid_t  repid);

    void Delete();

    void set_rx_queue(uint8_t rx_queue){
        m_rx_queue=rx_queue;
    }
    void conf_queues();
    void configure(uint16_t nb_rx_queue,
                   uint16_t nb_tx_queue,
                   const struct rte_eth_conf *eth_conf);
    void get_stats(CPhyEthIFStats *stats);
    int dump_fdir_global_stats(FILE *fd);
    int reset_hw_flow_stats();
    int get_flow_stats(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset);
    int get_flow_stats_payload(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset);
    void rx_queue_setup(uint16_t rx_queue_id,
                        uint16_t nb_rx_desc,
                        unsigned int socket_id,
                        const struct rte_eth_rxconf *rx_conf,
                        struct rte_mempool *mb_pool);
    void tx_queue_setup(uint16_t tx_queue_id,
                        uint16_t nb_tx_desc,
                        unsigned int socket_id,
                        const struct rte_eth_txconf *tx_conf);
    void stop_rx_drop_queue();
    void configure_rx_duplicate_rules();
    int set_port_rcv_all(bool is_rcv);
    void start();
    void stop();
    void disable_flow_control();
    void dump_stats(FILE *fd);
    void set_ignore_stats_base(CPreTestStats &pre_stats);
    bool get_extended_stats();
    void update_counters();
    int configure_rss_redirect_table(uint16_t numer_of_queues,
                                     uint16_t skip_queue);

    void stats_clear();

    tvpid_t             get_tvpid(){
        return (m_tvpid);
    }

    repid_t             get_repid(){
        return (m_repid);
    }

    float get_last_tx_rate(){
        return (m_last_tx_rate);
    }
    float get_last_rx_rate(){
        return (m_last_rx_rate);
    }
    float get_last_tx_pps_rate(){
        return (m_last_tx_pps);
    }
    float get_last_rx_pps_rate(){
        return (m_last_rx_pps);
    }

    CPhyEthIFStats     & get_stats(){
        return ( m_stats );
    }
    CPhyEthIgnoreStats & get_ignore_stats() {
        return m_ignore_stats;
    }
    void flush_dp_rx_queue(void);
    void flush_rx_queue(void);
    int add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                          , uint8_t ipv6_next_h, uint16_t id) const;
    int del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                          , uint8_t ipv6_next_h, uint16_t id) const;
    inline uint16_t  tx_burst(uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
        return rte_eth_tx_burst(m_repid, queue_id, tx_pkts, nb_pkts);
    }
    inline uint16_t  rx_burst(uint16_t queue_id, struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        return rte_eth_rx_burst(m_repid, queue_id, rx_pkts, nb_pkts);
    }

    inline uint16_t  rx_burst_dq(struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        return rte_eth_rx_burst(m_repid, 0, rx_pkts, nb_pkts);
    }

    inline uint32_t pci_reg_read(uint32_t reg_off) {
        void *reg_addr;
        uint32_t reg_v;
        reg_addr = (void *)((char *)m_dev_info.pci_dev->mem_resource[0].addr +
                            reg_off);
        reg_v = *((volatile uint32_t *)reg_addr);
        return rte_le_to_cpu_32(reg_v);
    }
    inline void pci_reg_write(uint32_t reg_off,
                              uint32_t reg_v) {
        void *reg_addr;

        reg_addr = (void *)((char *)m_dev_info.pci_dev->mem_resource[0].addr +
                            reg_off);
        *((volatile uint32_t *)reg_addr) = rte_cpu_to_le_32(reg_v);
    }
    void dump_stats_extended(FILE *fd);

    int get_rx_stat_capabilities();

    const std::vector<std::pair<uint8_t, uint8_t>> & get_core_list();
    TRexPortAttr * get_port_attr() { return m_port_attr; }

 private:
    tvpid_t                  m_tvpid;
    repid_t                  m_repid;
    uint8_t                  m_rx_queue;
    uint64_t                 m_sw_try_tx_pkt;
    uint64_t                 m_sw_tx_drop_pkt;
    uint32_t                 m_stats_err_cnt;
    CBwMeasure               m_bw_tx;
    CBwMeasure               m_bw_rx;
    CPPSMeasure              m_pps_tx;
    CPPSMeasure              m_pps_rx;
    CPhyEthIFStats           m_stats;
    CPhyEthIgnoreStats       m_ignore_stats;
    TRexPortAttr            *m_port_attr;
    float                    m_last_tx_rate;
    float                    m_last_rx_rate;
    float                    m_last_tx_pps;
    float                    m_last_rx_pps;

    /* holds the core ID list for this port - (core, dir) list*/
    std::vector<std::pair<uint8_t, uint8_t>> m_core_id_list;

 public:
    struct rte_eth_dev_info  m_dev_info;
};

// Because it is difficult to move CGlobalTRex into this h file, defining interface class to it
class CGlobalTRexInterface  {
 public:
    CPhyEthIF *get_ports(uint8_t &port_num);
};

#endif
