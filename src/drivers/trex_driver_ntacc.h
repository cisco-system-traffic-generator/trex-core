#ifndef TREX_DRIVERS_NTACC_H
#define TREX_DRIVERS_NTACC_H

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
#include <dlfcn.h>


class CTRexExtendedDriverBaseNtAcc : public CTRexExtendedDriverBase {
public:
    CTRexExtendedDriverBaseNtAcc();
    ~CTRexExtendedDriverBaseNtAcc();

    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, false, false, true, true);
    }

    virtual bool sleep_after_arp_needed(){
        return(true);
    }

    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverBaseNtAcc() );
    }

    virtual void update_global_config_fdir(port_cfg_t * cfg){
    }

    virtual bool is_support_for_rx_scatter_gather(){
        return (false);
    }

    virtual int get_min_sample_rate(void);

    virtual void update_configuration(port_cfg_t * cfg);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    void clear_extended_stats(CPhyEthIF * _if);
    virtual void reset_rx_stats(CPhyEthIF * _if, uint32_t *stats, int min, int len);
    virtual int get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts, uint32_t *bytes, uint32_t *prev_bytes, int min, int max);
    virtual int get_stat_counters_num() {return MAX_FLOW_STATS;}
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id) {
        // Even though the NIC support per flow statistics it is not yet available in the DPDK so SW must be used
        flags = TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_RX_BYTES_COUNT
        | TrexPlatformApi::IF_STAT_PAYLOAD;
        num_counters = MAX_FLOW_STATS;
        base_ip_id = IP_ID_RESERVE_BASE;
    }
    virtual CFlowStatParser *get_flow_stat_parser();
    virtual int dump_fdir_global_stats(CPhyEthIF * _if, FILE *fd);
    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on);
    virtual int verify_fw_ver(int i);
    static std::string ntacc_so_str;

private:
    void* (*ntacc_add_rules)(uint8_t port_id, uint16_t type,
            uint8_t l4_proto, int queue, char *ntpl_str);
    void* (*ntacc_del_rules)(uint8_t port_id, void *rte_flow);

    virtual void add_del_rules(enum rte_filter_op op, uint8_t port_id, uint16_t type,
                               uint8_t l4_proto, int queue, uint32_t f_id, char *ntpl_str);
    virtual int add_del_eth_type_rule(uint8_t port_id, enum rte_filter_op op, uint16_t eth_type);
    virtual int configure_rx_filter_rules_stateless(CPhyEthIF * _if);
    virtual int configure_rx_filter_rules_statefull(CPhyEthIF * _if);
    struct fid_s {
        uint8_t port_id;
        uint32_t id;
        void *rte_flow;
        TAILQ_ENTRY(fid_s) leTQ; //!< TailQ element.
    };
    TAILQ_HEAD(, fid_s) lh_fid;
};


#endif /* TREX_DRIVERS_NTACC_Hf */
