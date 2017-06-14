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

#include "trex_stateless_rx_defs.h"
#include "trex_vlan.h"


/**
 * holds L2 MAC configuration
 */
class LayerConfigMAC {
public:
    
    /**
     * IPv4 state of resolution
     */
    enum ether_state_e {
        STATE_UNCONFIGRED,
        STATE_CONFIGURED
    };
    
    LayerConfigMAC(uint8_t port_id);
    
    void set_src(const uint8_t *src_mac) {
        memcpy(m_src_mac, src_mac, 6);
    }
    
    void set_dst(const uint8_t *dst_mac) {
        memcpy(m_dst_mac, dst_mac, 6);
    }
    
    const uint8_t *get_src() const {
        return m_src_mac;
    }
    
    const uint8_t *get_dst() const {
        return m_dst_mac;
    }
    
    void set_state(ether_state_e state) {
        m_state = state;    
    }
    
    ether_state_e get_state() const {
        return m_state;
    }
    
    Json::Value to_json() const;
    
private:
    uint8_t         *m_src_mac;
    uint8_t         *m_dst_mac;
    ether_state_e    m_state;
};

/**
 * holds L3 IPv4 configuration
 */
class LayerConfigIPv4 {
    
public:
    
    /**
     * IPv4 state of resolution
     */
    enum ipv4_state_e {
        STATE_NONE,
        STATE_UNRESOLVED,
        STATE_RESOLVED
    };
    
    LayerConfigIPv4() {
        m_state = STATE_NONE;
    }
    
    void set_src(uint32_t src_ipv4) {
        m_src_ipv4 = src_ipv4;
    }
    
    void set_dst(uint32_t dst_ipv4) {
        m_dst_ipv4 = dst_ipv4;
    }
    
    void set_state(ipv4_state_e state) {
        m_state = state;
    }
    
    uint32_t get_src() const {
        return m_src_ipv4;
    }
    
    uint32_t get_dst() const {
        return m_dst_ipv4;
    }
    
    ipv4_state_e get_state() const {
        return m_state;
    }
    
    Json::Value to_json() const;
    
private:
    ipv4_state_e    m_state;
    uint32_t        m_src_ipv4;
    uint32_t        m_dst_ipv4;
};

/**
 * holds all layer configuration
 * 
 * @author imarom (12/25/2016)
 */
class LayerConfig {
public:
    
    LayerConfig(uint8_t port_id) : m_l2_config(port_id) {
        m_port_id = port_id;
    }
    
    /**
     * configure port for L2 (no L3)
     * 
     */
    void set_l2_mode(const uint8_t *dst_mac);
    
    /**
     * configure port IPv4 (unresolved)
     * 
     */
    void set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4);
    
    /**
     * configure port IPv4 (resolved)
     * 
     */
    void set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4, const uint8_t *resolved_mac);
    
    /**
     * event handler in case of a link down event
     * 
     * @author imarom (12/22/2016)
     */
    void on_link_down();
    
    const LayerConfigMAC& get_ether() const {
        return m_l2_config;
    }
    
    const LayerConfigIPv4& get_ipv4() const {
        return m_l3_ipv4_config;
    }
    
    /**
     * write state to JSON
     * 
     */
    Json::Value to_json() const;
        
private:
    
    uint8_t          m_port_id;
    LayerConfigMAC   m_l2_config;
    LayerConfigIPv4  m_l3_ipv4_config;
};



class TRexPortAttr {
public:

    TRexPortAttr(uint8_t port_id) : m_layer_cfg(port_id) {
        m_src_ipv4 = 0;
    }
    
    virtual ~TRexPortAttr(){}

/*    UPDATES    */
    virtual void update_link_status() = 0;
    virtual bool update_link_status_nowait() = 0; // returns true if the status was changed
    virtual void update_device_info() = 0;
    virtual void reset_xstats() = 0;
    virtual void update_description() = 0;
    
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
    virtual bool is_virtual() { return flag_is_virtual; }
    virtual bool is_fc_change_supported() { return flag_is_fc_change_supported; }
    virtual bool is_led_change_supported() { return flag_is_led_change_supported; }
    virtual bool is_link_change_supported() { return flag_is_link_change_supported; }
    virtual void get_description(std::string &description) { description = intf_info_st.description; }
    virtual void get_supported_speeds(supp_speeds_t &supp_speeds) = 0;
    virtual bool is_loopback() const = 0;
    
    std::string get_rx_filter_mode() const;

/*    SETTERS    */
    virtual int set_promiscuous(bool enabled) = 0;
    virtual int set_multicast(bool enabled) = 0;
    virtual int add_mac(char * mac) = 0;
    virtual int set_link_up(bool up) = 0;
    virtual int set_flow_ctrl(int mode) = 0;
    virtual int set_led(bool on) = 0;
    virtual int set_rx_filter_mode(rx_filter_mode_e mode) = 0;
    
    /**
     * configures port for L2 mode
     * 
     */
    void set_l2_mode(const uint8_t *dest_mac) {
        m_layer_cfg.set_l2_mode(dest_mac);
    }

    /**
     * configures port in L3 mode
     * unresolved
     */
    void set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4) {
        m_layer_cfg.set_l3_mode(src_ipv4, dst_ipv4);
    }
    
    /**
     * configure port for L3 mode 
     * resolved
     */
    void set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4, const uint8_t *resolved_mac) {
        m_layer_cfg.set_l3_mode(src_ipv4, dst_ipv4, resolved_mac);
    }

    const LayerConfig & get_layer_cfg() const {
        return m_layer_cfg;
    }


    void set_vlan_cfg(const VLANConfig &vlan_cfg) {
        m_vlan_cfg = vlan_cfg;
    }
    
    
    /**
     * gets the port VLAN configuration
     * 
     */
    const VLANConfig & get_vlan_cfg() const {
        return m_vlan_cfg;
    }
    
    
    /* DUMPS */
    virtual void dump_link(FILE *fd) = 0;

    /* dump object status to JSON */
    void to_json(Json::Value &output);
    
    uint8_t get_port_id() const {
        return m_port_id;
    } 
    
    /**
     * event handler for link down event
     */
    void on_link_down();
    
protected:

    uint8_t                   m_port_id;
    rte_eth_link              m_link;
    uint32_t                  m_src_ipv4;

    LayerConfig               m_layer_cfg;
    VLANConfig                m_vlan_cfg;
        
    struct rte_eth_dev_info   dev_info;
    
    rx_filter_mode_e m_rx_filter_mode;

    bool       flag_is_virtual;
    bool       flag_is_fc_change_supported;
    bool       flag_is_led_change_supported;
    bool       flag_is_link_change_supported;
    

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
                     bool fc_change_allowed) : TRexPortAttr(tvpid) {

        m_tvpid = tvpid;
        m_repid = repid;
        m_port_id = tvpid; /* child */

        m_rx_filter_mode = RX_FILTER_MODE_HW;

        flag_is_virtual = is_virtual;
        int tmp;
        flag_is_fc_change_supported = fc_change_allowed && (get_flow_ctrl(tmp) != -ENOTSUP);
        flag_is_led_change_supported = (set_led(true) != -ENOTSUP);
        flag_is_link_change_supported = (set_link_up(true) != -ENOTSUP);
        update_description();
        update_device_info();
    }

/*    UPDATES    */
    virtual void update_link_status();
    virtual bool update_link_status_nowait(); // returns true if the status was changed
    virtual void update_device_info();
    virtual void reset_xstats();
    virtual void update_description();

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
    DpdkTRexPortAttrMlnx5G(uint8_t tvpid, 
                     uint8_t repid, bool is_virtual, bool fc_change_allowed) : DpdkTRexPortAttr(tvpid,repid, is_virtual, fc_change_allowed) {}
    virtual int set_link_up(bool up);
};


class SimTRexPortAttr : public TRexPortAttr {
public:
    SimTRexPortAttr() : TRexPortAttr(0) {
        m_link.link_speed   = 10000;
        m_link.link_duplex  = 1;
        m_link.link_autoneg = 0;
        m_link.link_status  = 1;
        flag_is_virtual = true;
        flag_is_fc_change_supported = false;
        flag_is_led_change_supported = false;
        flag_is_link_change_supported = false;
    }

    /* DUMMY */
    void update_link_status() {}
    bool update_link_status_nowait() { return false; }
    void update_device_info() {}
    void reset_xstats() {}
    void update_description() {}
    bool get_promiscuous() { return false; }
    bool get_multicast() { return false; }
    void get_hw_src_mac(struct ether_addr *mac_addr) {}
    int get_xstats_values(xstats_values_t &xstats_values) { return -ENOTSUP; }
    int get_xstats_names(xstats_names_t &xstats_names) { return -ENOTSUP; }
    int get_flow_ctrl(int &mode) { return -ENOTSUP; }
    void get_description(std::string &description) {}
    void get_supported_speeds(supp_speeds_t &supp_speeds) {}
    int set_promiscuous(bool enabled) { return -ENOTSUP; }
    int set_multicast(bool enabled) { return -ENOTSUP; }
    int add_mac(char * mac) { return -ENOTSUP; }
    int set_link_up(bool up) { return -ENOTSUP; }
    int set_flow_ctrl(int mode) { return -ENOTSUP; }
    int set_led(bool on) { return -ENOTSUP; }
    void dump_link(FILE *fd) {}
    int set_rx_filter_mode(rx_filter_mode_e mode) { return -ENOTSUP; }
    virtual bool is_loopback() const { return false; }
};


#endif /* __TREX_PORT_ATTR_H__ */
