#ifndef TREX_DRIVERS_I40E_H
#define TREX_DRIVERS_I40E_H

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

#include "trex_driver_base.h"


class CTRexExtendedDriverBase40G : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBase40G();

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        // disabling flow control on 40G using DPDK API causes the interface to malfunction
        return new DpdkTRexPortAttr(tvpid, repid, false, false, true, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase40G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }
    virtual int get_min_sample_rate(void);
    virtual void update_configuration(port_cfg_t * cfg);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int add_del_rx_flow_stat_rule(CPhyEthIF * _if, enum rte_filter_op op, uint16_t l3_proto
                                          , uint8_t l4_proto, uint8_t ipv6_next_h, uint16_t id);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id);
    virtual int wait_for_stable_link();
    virtual bool hw_rx_stat_supported();
    virtual int verify_fw_ver(tvpid_t   tvpid);
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);

private:
    virtual void add_del_rules(enum rte_filter_op op, repid_t  repid, uint16_t type, uint8_t ttl
                               , uint16_t ip_id, uint8_t l4_proto, int queue, uint16_t stat_idx);
    virtual int add_del_eth_type_rule(repid_t  repid, enum rte_filter_op op, uint16_t eth_type);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    uint32_t get_flow_stats_offset(repid_t repid);

private:
    uint16_t m_max_flow_stats;
};

#endif /* TREX_DRIVERS_I40E_H */
