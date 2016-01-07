/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include "bp_sim.h"

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

public:
    void Clear();
    void Dump(FILE *fd);
    void DumpAll(FILE *fd);
};

class CPhyEthIF  {
public:
    CPhyEthIF (){
        m_port_id=0;
        m_rx_queue=0;
    }
    bool Create(uint8_t portid){
        m_port_id      = portid;
        m_last_rx_rate = 0.0;
        m_last_tx_rate = 0.0;
        m_last_tx_pps  = 0.0;
        return (true);
    }
    void Delete();

    void set_rx_queue(uint8_t rx_queue){
        m_rx_queue=rx_queue;
    }

    void configure(uint16_t nb_rx_queue,
				  uint16_t nb_tx_queue,
				  const struct rte_eth_conf *eth_conf);
    void macaddr_get(struct ether_addr *mac_addr);
    void get_stats(CPhyEthIFStats *stats);
    void get_stats_1g(CPhyEthIFStats *stats);
    void rx_queue_setup(uint16_t rx_queue_id,
                        uint16_t nb_rx_desc, 
                        unsigned int socket_id,
                        const struct rte_eth_rxconf *rx_conf,
                        struct rte_mempool *mb_pool);
    void tx_queue_setup(uint16_t tx_queue_id,
                        uint16_t nb_tx_desc, 
                        unsigned int socket_id,
                        const struct rte_eth_txconf *tx_conf);
    void configure_rx_drop_queue();
    void configure_rx_duplicate_rules();
    void start();
    void stop();
    void update_link_status();
    bool is_link_up(){
        return (m_link.link_status?true:false);
    }
    void dump_link(FILE *fd);
    void disable_flow_control();
    void set_promiscuous(bool enable);
    void add_mac(char * mac);
    bool get_promiscuous();
    void dump_stats(FILE *fd);
    void update_counters();
    void stats_clear();
    uint8_t             get_port_id(){
	return (m_port_id);
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
    void flush_rx_queue(void);

public:
    inline uint16_t  tx_burst(uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
        return rte_eth_tx_burst(m_port_id, queue_id, tx_pkts, nb_pkts);
    }
    inline uint16_t  rx_burst(uint16_t queue_id, struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        return rte_eth_rx_burst(m_port_id, queue_id, rx_pkts, nb_pkts);
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
    uint8_t                  get_rte_port_id(void) {
	return m_port_id;
    }
private:
    uint8_t                  m_port_id;
    uint8_t                  m_rx_queue;
    struct rte_eth_link      m_link;
    uint64_t                 m_sw_try_tx_pkt;
    uint64_t                 m_sw_tx_drop_pkt;
    CBwMeasure               m_bw_tx;
    CBwMeasure               m_bw_rx;
    CPPSMeasure              m_pps_tx;
    CPPSMeasure              m_pps_rx;
    CPhyEthIFStats           m_stats;
    float                    m_last_tx_rate;
    float                    m_last_rx_rate;
    float                    m_last_tx_pps;
    float                    m_last_rx_pps;
public:
    struct rte_eth_dev_info  m_dev_info;   
};

#endif
