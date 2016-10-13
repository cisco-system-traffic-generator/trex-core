/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

#ifndef __TREX_PORT_ATTR_H__
#define __TREX_PORT_ATTR_H__

#include <string>
#include <vector>
#include <rte_ethdev.h>
#include "trex_defs.h"


class TRexPortAttr {
public:
    TRexPortAttr(uint8_t total_ports, bool is_virtual = false) : total_ports(total_ports) {
        flag_is_virtual = is_virtual;
        m_link.resize(total_ports);
        intf_info_st.resize(total_ports);
    }

/*    UPDATES    */
    virtual void update_link_status(uint8_t port_id);
    virtual void update_link_status_nowait(uint8_t port_id);
    virtual void reset_xstats(uint8_t port_id);
    virtual void update_info();

/*    GETTERS    */
    virtual bool get_promiscuous(uint8_t port_id);
    virtual void macaddr_get(uint8_t port_id, struct ether_addr *mac_addr);
    virtual uint32_t get_link_speed(uint8_t port_id) { return m_link[port_id].link_speed; } // L1 Mbps
    virtual bool is_link_duplex(uint8_t port_id) { return (m_link[port_id].link_duplex ? true : false); }
    virtual bool is_link_autoneg(uint8_t port_id) { return (m_link[port_id].link_autoneg ? true : false); }
    virtual bool is_link_up(uint8_t port_id) { return (m_link[port_id].link_status ? true : false); }
    virtual int get_xstats_values(uint8_t port_id, xstats_values_t &xstats_values);
    virtual int get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names);
    virtual int get_flow_ctrl(uint8_t port_id, enum rte_eth_fc_mode *mode);

/*    SETTERS    */
    virtual int set_promiscuous(uint8_t port_id, bool enabled);
    virtual int add_mac(uint8_t port_id, char * mac);
    virtual int set_link_up(uint8_t port_id, bool up);
    virtual int set_flow_ctrl(uint8_t port_id, const enum rte_eth_fc_mode mode);
    virtual int set_led(uint8_t port_id, bool on);


/*    DUMPS    */
    virtual void dump_link(uint8_t port_id, FILE *fd);
    virtual bool is_virtual() {
        return flag_is_virtual;
    }

private:
    bool flag_is_virtual;
    const uint8_t total_ports;
    rte_eth_fc_conf fc_conf_tmp;
    std::vector <rte_eth_link> m_link;
    std::vector<struct rte_eth_xstat> xstats_values_tmp;
    std::vector<struct rte_eth_xstat_name> xstats_names_tmp;

    struct mac_cfg_st {
        uint8_t hw_macaddr[6];
        uint8_t src_macaddr[6];
        uint8_t dst_macaddr[6];
    };

    struct intf_info_st_type {
        mac_cfg_st      mac_info;
        std::string     pci_addr;
        std::string     description;
        int             numa_node;
    };

    std::vector <intf_info_st_type> intf_info_st;

};


#endif /* __TREX_PORT_ATTR_H__ */
