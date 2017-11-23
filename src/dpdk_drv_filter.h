#ifndef DPDK_DRV_FILTER_H
#define DPDK_DRV_FILTER_H

/*
  Hanoh Haim
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rte_ip.h>
#include <rte_flow.h>
#include "trex_defs.h"
#include "dpdk_port_map.h"

typedef enum { mfDISABLE     = 0x12, 
               mfPASS_ALL_RX = 0x13, 
               mfDROP_ALL_PASS_TOS =0x14,
               mfPASS_TOS =0x15
} dpdk_filter_mode_t;


class CDpdkFilterPort {
public:
    CDpdkFilterPort();
    ~CDpdkFilterPort();

    void set_rx_q(uint8_t rx_q){
        m_rx_q=rx_q;
    }

    void set_port_id(repid_t repid){
        m_repid=repid;
    }

    /* in case of TCP we don't want to drop*/
    void set_drop_all_mode(bool enable){
        m_drop_all=enable;
    }
    
    void set_mode(dpdk_filter_mode_t mode);

public:
    /* driver old interface, need refactoring  */
    int configure_rx_filter_rules() {
        if (m_drop_all){
            set_mode(mfDROP_ALL_PASS_TOS);
        }else{
            set_mode(mfPASS_TOS);
        }
        return(0);
    }

    int set_rcv_all(bool set_on){
        if (set_on) {
            set_mode(mfPASS_ALL_RX);
        }else{
            configure_rx_filter_rules();
        }
        return(0);
    }


private:
    void set_tos_filter(bool enable);
    void set_drop_all_filter(bool enable);
    void set_pass_all_filter(bool enable);
    void _set_mode(dpdk_filter_mode_t mode,
                   bool enable);
    void clear_filter(struct rte_flow * &  filter);
private:
    repid_t             m_repid;
    uint8_t             m_rx_q;
    bool                m_drop_all;
    dpdk_filter_mode_t  m_mode;
    struct rte_flow *   m_rx_ipv4_tos;
    struct rte_flow *   m_rx_ipv6_tos;
    struct rte_flow *   m_rx_drop_all;
    struct rte_flow *   m_rx_pass_rx_all;
};



class CDpdkFilterManager {
public:
    CDpdkFilterManager(){
        int i;
        for (i=0;i<TREX_MAX_PORTS; i++) {
            m_port[i]=0;
        }
    }

    int configure_rx_filter_rules(repid_t repid) {
        CDpdkFilterPort * lp=get_port(repid);
        return(lp->configure_rx_filter_rules());
    }

    int set_rcv_all(repid_t repid,bool set_on){
        CDpdkFilterPort * lp=get_port(repid);
        return(lp->set_rcv_all(set_on));
    }

private:
    CDpdkFilterPort * get_port(repid_t repid);
private:
    CDpdkFilterPort * m_port[TREX_MAX_PORTS];
};


#endif
