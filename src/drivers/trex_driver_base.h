#ifndef TREX_DRIVER_BASE_H
#define TREX_DRIVER_BASE_H

/*
  TRex team
  Cisco Systems, Inc.
*/

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

#include "main_dpdk.h"
#include "dpdk_drv_filter.h"

class CTrexDpdkParamsOverride {
 public:
   uint16_t rx_desc_num_data_q;
   uint16_t rx_desc_num_drop_q;
   uint16_t rx_desc_num_dp_q; 

   uint16_t tx_desc_num;
};


struct port_cfg_t {
public:
    port_cfg_t();

    void update_var(void);
    void update_global_config_fdir(void);

    struct rte_eth_conf     m_port_conf;
    struct rte_eth_rxconf   m_rx_conf;
    struct rte_eth_rxconf   m_rx_drop_conf;
    struct rte_eth_txconf   m_tx_conf;
    struct {
        uint64_t common_best_effort;
        uint64_t common_required;
        uint64_t astf_best_effort;
    } tx_offloads;
};


class CTRexExtendedDriverBase {
protected:
    enum {
        //see trex_driver_cap_t

        /* Does this NIC type support automatic packet dropping in case of a link down?
           in case it is supported the packets will be dropped, else there would be a back pressure to tx queues
           this interface is used as a workaround to let TRex work without link in stateless mode, driver that
           does not support that will be failed at init time because it will cause watchdog due to watchdog hang */
        TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN = 0x100,

        // Does the driver support changing MAC address?
        TREX_DRV_CAP_MAC_ADDR_CHG = 0x200,

    } trex_drv_cap;

public:
    virtual int get_min_sample_rate(void)=0;
    virtual void update_configuration(port_cfg_t * cfg)=0;
    virtual void update_global_config_fdir(port_cfg_t * cfg)=0;
    virtual int configure_rx_filter_rules(CPhyEthIF * _if)=0;
    virtual int add_del_rx_flow_stat_rule(CPhyEthIF * _if, enum rte_filter_op op, uint16_t l3, uint8_t l4
                                          , uint8_t ipv6_next_h, uint16_t id) {return 0;}

    bool hardware_support_mac_change() {
        return ((m_cap & TREX_DRV_CAP_MAC_ADDR_CHG) != 0);
    }

    bool drop_packets_incase_of_linkdown() {
        return ((m_cap & TREX_DRV_CAP_DROP_PKTS_IF_LNK_DOWN) != 0);
    }

    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual bool get_extended_stats_fixed(CPhyEthIF * _if, CPhyEthIFStats *stats, int fix_i, int fix_o);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats)=0;
    virtual void clear_extended_stats(CPhyEthIF * _if)=0;
    virtual int  wait_for_stable_link();
    virtual bool sleep_after_arp_needed(){
        return(false);
    }
    virtual void wait_after_link_up();
    virtual bool hw_rx_stat_supported(){return false;}
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes
                             , int min, int max) {return -1;}
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len) {}
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd) { return -1;}
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) = 0;

    /* can't get CPhyEthIF as it won't be valid at that time */
    virtual int verify_fw_ver(tvpid_t   tvpid) {return 0;}
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on)=0;
    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) = 0;

    uint32_t get_capabilities(void) {
        return m_cap;
    }

    /* return true in case the driver can get 2K buffers and can make then one big 9K buffer 
       e.g. mlx5 and napatech can't do that 
    */
    virtual bool is_support_for_rx_scatter_gather(){
        return (true);
    }

    virtual bool is_override_dpdk_params(CTrexDpdkParamsOverride & dpdk_p){
        return(false);
    }


protected:
    // flags describing interface capabilities
    uint32_t m_cap;
};


// stubs in case of dummy port, call to normal function otherwise
class CTRexExtendedDriverDummySelector : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverDummySelector(CTRexExtendedDriverBase *original_driver);

    int get_min_sample_rate(void) {
        return m_real_drv->get_min_sample_rate();
    }
    void update_configuration(port_cfg_t *cfg) {
        m_real_drv->update_configuration(cfg);
    }
    void update_global_config_fdir(port_cfg_t *cfg) {
        m_real_drv->update_global_config_fdir(cfg);
    }
    int configure_rx_filter_rules(CPhyEthIF *_if);
    int add_del_rx_flow_stat_rule(CPhyEthIF *_if, enum rte_filter_op op, uint16_t l3, uint8_t l4, uint8_t ipv6_next_h, uint16_t id);
    int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    bool get_extended_stats_fixed(CPhyEthIF * _if, CPhyEthIFStats *stats, int fix_i, int fix_o);
    bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    void clear_extended_stats(CPhyEthIF * _if);
    int  wait_for_stable_link() {
        return m_real_drv->wait_for_stable_link();
    }
    bool sleep_after_arp_needed() {
        return m_real_drv->sleep_after_arp_needed();
    }
    void wait_after_link_up() {
        m_real_drv->wait_after_link_up();
    }
    bool hw_rx_stat_supported() {
        return m_real_drv->hw_rx_stat_supported();
    }
    int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
         m_real_drv->get_rx_stat_capabilities(flags, num_counters, base_ip_id);
    }
    int verify_fw_ver(tvpid_t tvpid);
    CFlowStatParser *get_flow_stat_parser() {
        return m_real_drv->get_flow_stat_parser();
    }

    bool is_support_for_rx_scatter_gather(){
        return m_real_drv->is_support_for_rx_scatter_gather();
    }

    bool is_override_dpdk_params(CTrexDpdkParamsOverride & dpdk_p){
        return (m_real_drv->is_override_dpdk_params(dpdk_p));
    }


    int set_rcv_all(CPhyEthIF * _if, bool set_on);
    TRexPortAttr * create_port_attr(tvpid_t tvpid, repid_t repid);
private:
    CTRexExtendedDriverBase *m_real_drv;
};


typedef CTRexExtendedDriverBase * (*create_object_t) (void);

class CTRexExtendedDriverRec {
public:
    std::string         m_driver_name;
    create_object_t     m_constructor;
};

class CTRexExtendedDriverDb {
public:

    const std::string & get_driver_name() {
        return m_driver_name;
    }

    bool is_driver_exists(std::string name);


    void set_driver_name(std::string name);

    void create_dummy();

    CTRexExtendedDriverBase* get_drv();

public:

    static CTRexExtendedDriverDb * Ins();

private:
    CTRexExtendedDriverBase * create_driver(std::string name);

    CTRexExtendedDriverDb();
    void register_driver(std::string name, create_object_t func);
    static CTRexExtendedDriverDb * m_ins;
    bool        m_driver_was_set;
    bool        m_dummy_selector_created;
    std::string m_driver_name;
    CTRexExtendedDriverBase * m_drv;
    std::vector <CTRexExtendedDriverRec*>     m_list;

};


CTRexExtendedDriverBase * get_ex_drv();

std::string& get_ntacc_so_string(void);
std::string& get_mlx5_so_string(void);
std::string& get_mlx4_so_string(void);


#endif /* TREX_DRIVER_BASE_H */
