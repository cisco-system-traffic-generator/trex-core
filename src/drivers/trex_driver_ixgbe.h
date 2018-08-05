#ifndef TREX_DRIVERS_IXGBE_H
#define TREX_DRIVERS_IXGBE_H

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


class CTRexExtendedDriverBase10G : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBase10G();

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, false, true, true, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase10G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg);

    virtual int get_min_sample_rate(void);
    virtual void update_configuration(port_cfg_t * cfg);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_stateless(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id);
    virtual CFlowStatParser *get_flow_stat_parser();
    int add_del_eth_filter(CPhyEthIF * _if, bool is_add, uint16_t ethertype);
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);
};


#endif /* TREX_DRIVERS_IXGBE_H */
