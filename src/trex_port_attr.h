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
#include <string.h>
#include <json/json.h>

#include "rte_ethdev_includes.h"
#include "trex_defs.h"
#include "common/basic_utils.h"

#include "trex_rx_defs.h"
#include "trex_vlan.h"





class TRexPortAttr {
public:

    virtual ~TRexPortAttr(){}

/*    UPDATES    */
    virtual void update_link_status() = 0;
    virtual bool update_link_status_nowait() = 0; // returns true if the status was changed
    virtual void reset_xstats() = 0;

/*    GETTERS    */
    virtual bool get_promiscuous() = 0;
    virtual bool get_multicast() = 0;
    virtual void get_hw_src_mac(struct ether_addr *mac_addr) = 0;
    virtual uint32_t get_link_speed() { return m_link.link_speed; } // L1 Mbps
    virtual bool is_link_duplex() { return (m_link.link_duplex ? true : false); }
    virtual bool is_link_autoneg() { return (m_link.link_autoneg ? true : false); }
    virtual bool is_link_up() { return (m_link.link_status ? true : false); }
    virtual int get_xstats_values(xstats_values_t &xstats_values) = 0;
    virtual int get_xstats_names(xstats_names_t &xstats_names) = 0;
    virtual int get_flow_ctrl(int &mode) = 0;
    virtual bool has_pci() { return flag_has_pci; }
    virtual bool is_virtual() { return flag_is_virtual; }
    virtual bool is_fc_change_supported() { return flag_is_fc_change_supported; }
    virtual bool is_led_change_supported() { return flag_is_led_change_supported; }
    virtual bool is_link_change_supported() { return flag_is_link_change_supported; }
    virtual bool is_prom_change_supported() { return flag_is_prom_change_supported; }
    virtual const std::string &get_description() { return intf_info_st.description; }
    virtual int get_numa(void) { return intf_info_st.numa_node; }
    virtual const std::string& get_pci_addr(void) { return intf_info_st.pci_addr; }
    virtual void get_supported_speeds(supp_speeds_t &supp_speeds) = 0;
    virtual bool is_loopback() const = 0;
    virtual const struct rte_pci_device* get_pci_dev() const {
        assert(flag_has_pci);
        return &m_pci_dev;
    }
    virtual const struct rte_eth_dev_info* get_dev_info() const { return &m_dev_info; }

    virtual std::string get_rx_filter_mode() const;

/*    SETTERS    */
    virtual int set_promiscuous(bool enabled) = 0;
    virtual int set_multicast(bool enabled) = 0;
    virtual int add_mac(char * mac) = 0;
    virtual int set_link_up(bool up) = 0;
    virtual int set_flow_ctrl(int mode) = 0;
    virtual int set_led(bool on) = 0;
    virtual int set_rx_filter_mode(rx_filter_mode_e mode) = 0;

    
    
    /* DUMPS */
    virtual void dump_link(FILE *fd) = 0;

    /* dump object status to JSON */
    void to_json(Json::Value &output);
    
    uint8_t get_port_id() const {
        return m_port_id;
    }

protected:

    virtual void update_device_info() = 0;
    virtual void update_description() = 0;

    uint8_t                   m_port_id;
    rte_eth_link              m_link;

    struct rte_eth_dev_info   m_dev_info;
    struct rte_pci_device     m_pci_dev;

    rx_filter_mode_e m_rx_filter_mode;

    bool       flag_has_pci;
    bool       flag_is_virtual;
    bool       flag_is_fc_change_supported;
    bool       flag_is_led_change_supported;
    bool       flag_is_link_change_supported;
    bool       flag_is_prom_change_supported;

    struct intf_info_st {
        std::string     pci_addr;
        std::string     description;
        int             numa_node;
    }intf_info_st;

};

class DpdkTRexPortAttr : public TRexPortAttr {
public:

    DpdkTRexPortAttr(uint8_t tvpid, 
                     uint8_t repid,
                     bool is_virtual, 
                     bool fc_change_allowed,
                     bool is_prom_allowed,
                     bool has_pci) {

        m_tvpid = tvpid;
        m_repid = repid;
        m_port_id = tvpid; /* child */

        m_rx_filter_mode = RX_FILTER_MODE_HW;

        flag_has_pci    = has_pci;
        flag_is_virtual = is_virtual;
        int tmp;
        flag_is_fc_change_supported = fc_change_allowed && (get_flow_ctrl(tmp) != -ENOTSUP);
        flag_is_led_change_supported = (set_led(true) != -ENOTSUP);
        flag_is_link_change_supported = (set_link_up(true) != -ENOTSUP);
        flag_is_prom_change_supported = is_prom_allowed;
        update_device_info();
        update_description();
    }

/*    UPDATES    */
    virtual void update_link_status();
    virtual bool update_link_status_nowait(); // returns true if the status was changed
    virtual void reset_xstats();

/*    GETTERS    */
    virtual bool get_promiscuous();
    virtual bool get_multicast();
    virtual void get_hw_src_mac(struct ether_addr *mac_addr);
    virtual int get_xstats_values(xstats_values_t &xstats_values);
    virtual int get_xstats_names(xstats_names_t &xstats_names);
    virtual int get_flow_ctrl(int &mode);
    virtual void get_supported_speeds(supp_speeds_t &supp_speeds);
    virtual bool is_loopback() const;
    
/*    SETTERS    */
    virtual int set_promiscuous(bool enabled);
    virtual int set_multicast(bool enabled);
    virtual int add_mac(char * mac);
    virtual int set_link_up(bool up);
    virtual int set_flow_ctrl(int mode);
    virtual int set_led(bool on);

    virtual int set_rx_filter_mode(rx_filter_mode_e mode);

/*    DUMPS    */
    virtual void dump_link(FILE *fd);

protected:
    virtual void update_device_info();
    virtual void update_description();

private:
    uint8_t         m_tvpid ;
    uint8_t         m_repid ;

    rte_eth_fc_conf fc_conf_tmp;
    std::vector <struct rte_eth_xstat> xstats_values_tmp;
    std::vector <struct rte_eth_xstat_name> xstats_names_tmp;

};

/*
Example:
In order to use custom methods of port attributes per driver, need to instantiate this within driver.
*/
class DpdkTRexPortAttrMlnx5G : public DpdkTRexPortAttr {
public:
    DpdkTRexPortAttrMlnx5G(uint8_t tvpid, uint8_t repid, bool is_virtual, bool fc_change_allowed, bool prom_change_allowed, bool has_pci) :
                DpdkTRexPortAttr(tvpid, repid, is_virtual, fc_change_allowed, prom_change_allowed, has_pci) {}
    virtual int set_link_up(bool up);
};


class SimTRexPortAttr : public TRexPortAttr {
public:
    SimTRexPortAttr() {
        m_link.link_speed   = 10000;
        m_link.link_duplex  = 1;
        m_link.link_autoneg = 0;
        m_link.link_status  = 1;
        flag_has_pci = false;
        flag_is_virtual = true;
        flag_is_fc_change_supported = false;
        flag_is_led_change_supported = false;
        flag_is_link_change_supported = false;
        flag_is_prom_change_supported = false;
        update_description();
    }

    /* DUMMY */
    void update_link_status() {}
    bool update_link_status_nowait() { return false; }
    void reset_xstats() {}
    bool get_promiscuous() { return false; }
    bool get_multicast() { return false; }
    void get_hw_src_mac(struct ether_addr *mac_addr) {}
    int get_xstats_values(xstats_values_t &xstats_values) { return -ENOTSUP; }
    int get_xstats_names(xstats_names_t &xstats_names) { return -ENOTSUP; }
    int get_flow_ctrl(int &mode) { return -ENOTSUP; }
    void get_supported_speeds(supp_speeds_t &supp_speeds) {}
    int set_promiscuous(bool enabled) { return -ENOTSUP; }
    int set_multicast(bool enabled) { return -ENOTSUP; }
    int add_mac(char * mac) { return -ENOTSUP; }
    int set_link_up(bool up) { return -ENOTSUP; }
    int set_flow_ctrl(int mode) { return -ENOTSUP; }
    int set_led(bool on) { return -ENOTSUP; }
    void dump_link(FILE *fd) {}
    int set_rx_filter_mode(rx_filter_mode_e mode) { return -ENOTSUP; }
    bool is_loopback() const { return false; }
    std::string get_rx_filter_mode() {return "";}
protected:
    void update_device_info() {}
    void update_description() {
        intf_info_st.pci_addr = "N/A";
        intf_info_st.description = "Dummy port";
        intf_info_st.numa_node = -1;
    }
};


#endif /* __TREX_PORT_ATTR_H__ */
