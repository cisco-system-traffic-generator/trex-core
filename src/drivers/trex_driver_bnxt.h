#ifndef TREX_DRIVERS_BNXT_H
#define TREX_DRIVERS_BNXT_H

/*
  TRex team
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2019 Broadcom Inc.

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

class CTRexExtendedDriverBnxt : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBnxt();

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBnxt() );
    }

    virtual TRexPortAttr* create_port_attr(tvpid_t tvpid,repid_t repid);

    virtual void get_dpdk_drv_params(CTrexDpdkParams &p) { }
    virtual int get_min_sample_rate(void);
    virtual bool is_support_for_rx_scatter_gather();
    virtual void update_configuration(port_cfg_t * cfg);

    virtual void update_global_config_fdir(port_cfg_t * cfg) { }
    virtual int configure_rx_filter_rules(CPhyEthIF * _if) { return 1; }

    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
        return get_extended_stats_fixed(_if, stats, 4, 4);
    };
    virtual void clear_extended_stats(CPhyEthIF * _if){ }
    virtual int wait_for_stable_link() { return 1; }
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) { }
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on) { return 1; }

    static std::string bnxt_so_str;
};

#endif /* TREX_DRIVERS_BNXT_H */
