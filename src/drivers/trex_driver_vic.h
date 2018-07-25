#ifndef TREX_DRIVERS_VIC_H
#define TREX_DRIVERS_VIC_H

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


class CTRexExtendedDriverBaseVIC : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBaseVIC(){
        if (get_is_tcp_mode()) {
            m_cap = TREX_DRV_CAP_DROP_Q  | TREX_DRV_CAP_MAC_ADDR_CHG | TREX_DRV_DEFAULT_ASTF_MULTI_CORE;
        }else{
            m_cap = TREX_DRV_CAP_DROP_Q  | TREX_DRV_CAP_MAC_ADDR_CHG ;
        }
    }

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, false, false, true, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBaseVIC() );
    }
    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }
    void clear_extended_stats(CPhyEthIF * _if);
    bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);

    virtual int get_min_sample_rate(void);

    virtual int verify_fw_ver(tvpid_t   tvpid);

    virtual void update_configuration(port_cfg_t * cfg);

    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
        flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
            | TrexPlatformApi::IF_STAT_PAYLOAD;
        num_counters = MAX_FLOW_STATS;
        base_ip_id = IP_ID_RESERVE_BASE;
    }

    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);

private:

    virtual void add_del_rules(enum rte_filter_op op, repid_t  repid, uint16_t type, uint16_t id
                               , uint8_t l4_proto, uint8_t tos, int queue);
    virtual int add_del_eth_type_rule(repid_t  repid, enum rte_filter_op op, uint16_t eth_type);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);

};


#endif /* TREX_DRIVERS_VIC_H */
