#ifndef TREX_DRIVERS_MLX5_H
#define TREX_DRIVERS_MLX5_H

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


class CTRexExtendedDriverBaseMlnx5G : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBaseMlnx5G();

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        // disabling flow control on 40G using DPDK API causes the interface to malfunction
        return new DpdkTRexPortAttr(tvpid, repid, false, false, true, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBaseMlnx5G() );
    }


    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }

    virtual bool is_support_for_rx_scatter_gather();

    virtual int get_min_sample_rate(void);

    virtual void update_configuration(port_cfg_t * cfg);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id);

    virtual int wait_for_stable_link();
    // disabling flow control on 40G using DPDK API causes the interface to malfunction
    virtual bool flow_control_disable_supported(){return false;}
    virtual CFlowStatParser *get_flow_stat_parser();

    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on){
        return(m_filter_manager.set_rcv_all(_if->get_repid(),set_on));
    }

    virtual int configure_rx_filter_rules(CPhyEthIF * _if){
        return(m_filter_manager.configure_rx_filter_rules(_if->get_repid()));
    }
    static std::string mlx5_so_str;

private:
    CDpdkFilterManager  m_filter_manager;
    struct xstats_struct {
        uint16_t total_count;
        bool init;
    };
    xstats_struct m_port_xstats[TREX_MAX_PORTS];
};


#endif /* TREX_DRIVERS_MLX5_H */
