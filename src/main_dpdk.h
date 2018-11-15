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
#include "trex_modes.h"

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
        m_is_dummy       = false;
    }
    virtual ~CPhyEthIF() {}
    bool Create(tvpid_t  tvpid,
                repid_t  repid);

    void set_rx_queue(uint8_t rx_queue){
        m_rx_queue=rx_queue;
    }
    virtual void conf_queues();

    virtual void configure(uint16_t nb_rx_queue,
                   uint16_t nb_tx_queue,
                   const struct rte_eth_conf *eth_conf);
    virtual int dump_fdir_global_stats(FILE *fd);
    virtual int reset_hw_flow_stats();
    virtual int get_flow_stats(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset);
    virtual int get_flow_stats_payload(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset);
    virtual void rx_queue_setup(uint16_t rx_queue_id,
                                uint16_t nb_rx_desc,
                                unsigned int socket_id,
                                const struct rte_eth_rxconf *rx_conf,
                                struct rte_mempool *mb_pool);
    virtual void tx_queue_setup(uint16_t tx_queue_id,
                                uint16_t nb_tx_desc,
                                unsigned int socket_id,
                                const struct rte_eth_txconf *tx_conf);
    virtual void stop_rx_drop_queue();
    virtual void configure_rx_duplicate_rules();
    virtual int set_port_rcv_all(bool is_rcv);
    virtual inline bool is_dummy() { return m_is_dummy; }
    virtual void start();
    virtual void stop();
    virtual void disable_flow_control();
    virtual void dump_stats(FILE *fd);
    virtual void set_ignore_stats_base(CPreTestStats &pre_stats);
    virtual bool get_extended_stats();
    virtual void update_counters();

    virtual void stats_clear();

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
    virtual void flush_rx_queue(void);

    inline uint16_t tx_burst(uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
        if (likely( !m_is_dummy )) {
            return rte_eth_tx_burst(m_repid, queue_id, tx_pkts, nb_pkts);
        } else {
            for (int i=0; i<nb_pkts;i++) {
                rte_pktmbuf_free(tx_pkts[i]);
            }
            return nb_pkts;
        }
    }
    inline uint16_t  rx_burst(uint16_t queue_id, struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
        if (likely( !m_is_dummy )) {
            return rte_eth_rx_burst(m_repid, queue_id, rx_pkts, nb_pkts);
        } else {
            return 0;
        }
    }


    inline uint32_t pci_reg_read(uint32_t reg_off) {
        assert(!m_is_dummy);
        void *reg_addr;
        uint32_t reg_v;
        reg_addr = (void *)((char *)m_port_attr->get_pci_dev()->mem_resource[0].addr +
                            reg_off);
        reg_v = *((volatile uint32_t *)reg_addr);
        return rte_le_to_cpu_32(reg_v);
    }
    inline void pci_reg_write(uint32_t reg_off,
                              uint32_t reg_v) {
        assert(!m_is_dummy);
        void *reg_addr;

        reg_addr = (void *)((char *)m_port_attr->get_pci_dev()->mem_resource[0].addr +
                            reg_off);
        *((volatile uint32_t *)reg_addr) = rte_cpu_to_le_32(reg_v);
    }
    virtual void dump_stats_extended(FILE *fd);

    const std::vector<std::pair<uint8_t, uint8_t>> & get_core_list();
    TRexPortAttr * get_port_attr() { return m_port_attr; }

    virtual void configure_rss();

private:
    void conf_hardware_astf_rss();

    void _conf_queues(uint16_t tx_qs,
                      uint32_t tx_descs,
                      uint16_t rx_qs,
                      rx_que_desc_t & rx_qs_descs,
                      uint16_t rx_qs_drop_qid,
                      trex_dpdk_rx_distro_mode_t rss_mode,
                      bool in_astf_mode);


private:
    void conf_rx_queues_astf_multi_core();

    void configure_rss_astf(bool is_client,
                           uint16_t numer_of_queues,
                           uint16_t skip_queue);



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

 protected:
    bool                     m_is_dummy;

};

// stubs for dummy port
class CPhyEthIFDummy : public CPhyEthIF {
 public:
    CPhyEthIFDummy() {
        m_is_dummy = true;
    }
    void conf_queues() {}
    void configure(uint16_t, uint16_t, const struct rte_eth_conf *) {}
    int dump_fdir_global_stats(FILE *fd) { return 0; }
    int reset_hw_flow_stats() { return 0; }
    int get_flow_stats(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset) { return 0; }
    int get_flow_stats_payload(rx_per_flow_t *rx_stats, tx_per_flow_t *tx_stats, int min, int max, bool reset) { return 0; }
    void rx_queue_setup(uint16_t, uint16_t, unsigned int, const struct rte_eth_rxconf *, struct rte_mempool *) {}
    void tx_queue_setup(uint16_t, uint16_t, unsigned int, const struct rte_eth_txconf *) {}
    void stop_rx_drop_queue() {}
    void configure_rx_duplicate_rules() {}
    int set_port_rcv_all(bool) { return 0; }
    void start() {}
    void stop() {}
    void disable_flow_control() {}
    void dump_stats(FILE *) {}
    void set_ignore_stats_base(CPreTestStats &) {}
    bool get_extended_stats() { return 0; }
    void update_counters() {}
    void stats_clear() {}
    void flush_rx_queue(void) {}
    void dump_stats_extended(FILE *) {}
    void configure_rss(){}
};

// Because it is difficult to move CGlobalTRex into this h file, defining interface class to it
class CGlobalTRexInterface  {
 public:
    CPhyEthIF *get_ports(uint8_t &port_num);
};

bool fill_pci_dev(struct rte_eth_dev_info *dev_info, struct rte_pci_device* pci_dev);
void wait_x_sec(int sec);

typedef uint8_t tvpid_t; /* port ID of trex 0,1,2,3 up to MAX_PORTS*/
typedef uint8_t repid_t; /* DPDK port id  */

#endif
