#ifndef TREX_DRIVERS_IGB_H
#define TREX_DRIVERS_IGB_H

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

class CTRexExtendedDriverBase1G : public CTRexExtendedDriverBase {

public:
    CTRexExtendedDriverBase1G(){
        m_cap = TREX_DRV_CAP_DROP_Q | TREX_DRV_CAP_MAC_ADDR_CHG;
    }

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, false, true, true, true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBase1G() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg);

    virtual int get_min_sample_rate(void);
    virtual void update_configuration(port_cfg_t * cfg);
    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_stateless(CPhyEthIF * _if);
    virtual void clear_rx_filter_rules(CPhyEthIF * _if);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd) {return 0;}
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
        flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
            | TrexPlatformApi::IF_STAT_PAYLOAD;

        if (CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_ONE_QUEUE
            || CGlobalInfo::get_queues_mode() == CGlobalInfo::Q_MODE_RSS) {
            num_counters = MAX_FLOW_STATS;
            base_ip_id = IP_ID_RESERVE_BASE;
        } else {
            num_counters = UINT8_MAX;
            // Must be 0xff00, since we configure HW filter for the 0xff byte
            // The filter must catch all flow stat packets, and latency packets (having 0xffff in IP ID)
            base_ip_id = 0xff00;
        }
    }
    virtual int wait_for_stable_link();
    virtual void wait_after_link_up();
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);
};



#endif /* TREX_DRIVERS_IGB_H */
