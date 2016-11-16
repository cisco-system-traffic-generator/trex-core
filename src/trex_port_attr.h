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
#include "rte_ethdev_includes.h"
#include "trex_defs.h"
#include "common/basic_utils.h"
#include <json/json.h>
#include "trex_stateless_rx_defs.h"
#include <string.h>

/**
 * destination port attribute
 * 
 */
class DestAttr {
private:
    static const uint8_t g_dummy_mac[6];
public:

    DestAttr(uint8_t port_id);
    
    enum dest_type_e {
        DEST_TYPE_IPV4   = 1,
        DEST_TYPE_MAC    = 2
    };
    
    /**
     * set dest as an IPv4 unresolved
     */
    void set_dest_ipv4(uint32_t ipv4) {
        assert(ipv4 != 0);
        
        m_src_ipv4 = ipv4;
        memset(m_mac, 0, 6);
        m_type = DEST_TYPE_IPV4;
    }
    
    /**
     * set dest as a resolved IPv4
     */
    void set_dest_ipv4(uint32_t ipv4, const uint8_t *mac) {
        assert(ipv4 != 0);
        
        m_src_ipv4 = ipv4;
        memcpy(m_mac, mac, 6);
        m_type = DEST_TYPE_IPV4;
        
    }

    /**
     * dest dest as MAC
     * 
     */
    void set_dest_mac(const uint8_t *mac) {

        m_src_ipv4 = 0;
        memcpy(m_mac, mac, 6);
        m_type = DEST_TYPE_MAC;
    }
    
    
    bool is_resolved() const {
        if (m_type == DEST_TYPE_MAC) {
            return true;
        }
        
        for (int i = 0; i < 6; i++) {
            if (m_mac[i] != 0) {
                return true;
            }
        }
        
        /* all zeroes - non resolved */
        return false;
    }
    
    /**
     * get the dest mac
     * if no MAC is configured and dest was not resolved 
     * will return a dummy 
     */
    const uint8_t *get_dest_mac() {
        
        if (is_resolved()) {
            return m_mac;
        } else {
            return g_dummy_mac;
        }
    }
    
    /**
     * when link gets down - this should be called
     * 
     */
    void on_link_down() {
        if (m_type == DEST_TYPE_IPV4) {
            /* reset the IPv4 dest with no resolution */
            set_dest_ipv4(m_src_ipv4);
        }
    }
    
    void to_json(Json::Value &output) {
        switch (m_type) {

        case DEST_TYPE_IPV4:
            output["type"] = "ipv4";
            output["addr"] = utl_uint32_to_ipv4(m_src_ipv4);
            if (is_resolved()) {
                output["arp"] = utl_macaddr_to_str(m_mac);
            } else {
                output["arp"] = "none";
            }
            break;
            
        case DEST_TYPE_MAC:
            output["type"]      = "mac";
            output["addr"]      = utl_macaddr_to_str(m_mac);
            break;
            
        default:
            assert(0);
        }
        
    }
    
private:
    uint32_t          m_src_ipv4;
    uint8_t          *m_mac;
    dest_type_e       m_type;
    uint8_t           m_port_id;
};


class TRexPortAttr {
public:

    TRexPortAttr(uint8_t port_id) : m_dest(port_id) {
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

    uint32_t get_src_ipv4() {return m_src_ipv4;}
    DestAttr & get_dest() {return m_dest;}
    
    const uint8_t *get_src_mac() const;
    std::string get_rx_filter_mode() const;

    /* for a raw packet, write the src/dst MACs */
    void update_src_dst_mac(uint8_t *raw_pkt);
    
/*    SETTERS    */
    virtual int set_promiscuous(bool enabled) = 0;
    virtual int add_mac(char * mac) = 0;
    virtual int set_link_up(bool up) = 0;
    virtual int set_flow_ctrl(int mode) = 0;
    virtual int set_led(bool on) = 0;
    virtual int set_rx_filter_mode(rx_filter_mode_e mode) = 0;
    
    void set_src_ipv4(uint32_t addr) {
        m_src_ipv4 = addr;
    }
    
    /* DUMPS */
    virtual void dump_link(FILE *fd) = 0;

    /* dump object status to JSON */
    void to_json(Json::Value &output);
    
    
protected:
    
    uint8_t                   m_port_id;
    rte_eth_link              m_link;
    uint32_t                  m_src_ipv4;
    DestAttr                  m_dest;
    
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

    DpdkTRexPortAttr(uint8_t port_id, bool is_virtual, bool fc_change_allowed) : TRexPortAttr(port_id) {

        m_port_id = port_id;
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
    virtual void get_hw_src_mac(struct ether_addr *mac_addr);
    virtual int get_xstats_values(xstats_values_t &xstats_values);
    virtual int get_xstats_names(xstats_names_t &xstats_names);
    virtual int get_flow_ctrl(int &mode);
    virtual void get_supported_speeds(supp_speeds_t &supp_speeds);

/*    SETTERS    */
    virtual int set_promiscuous(bool enabled);
    virtual int add_mac(char * mac);
    virtual int set_link_up(bool up);
    virtual int set_flow_ctrl(int mode);
    virtual int set_led(bool on);

    virtual int set_rx_filter_mode(rx_filter_mode_e mode);

/*    DUMPS    */
    virtual void dump_link(FILE *fd);

private:
    rte_eth_fc_conf fc_conf_tmp;
    std::vector <struct rte_eth_xstat> xstats_values_tmp;
    std::vector <struct rte_eth_xstat_name> xstats_names_tmp;

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
    void get_hw_src_mac(struct ether_addr *mac_addr) {}
    int get_xstats_values(xstats_values_t &xstats_values) { return -ENOTSUP; }
    int get_xstats_names(xstats_names_t &xstats_names) { return -ENOTSUP; }
    int get_flow_ctrl(int &mode) { return -ENOTSUP; }
    void get_description(std::string &description) {}
    void get_supported_speeds(supp_speeds_t &supp_speeds) {}
    int set_promiscuous(bool enabled) { return -ENOTSUP; }
    int add_mac(char * mac) { return -ENOTSUP; }
    int set_link_up(bool up) { return -ENOTSUP; }
    int set_flow_ctrl(int mode) { return -ENOTSUP; }
    int set_led(bool on) { return -ENOTSUP; }
    void dump_link(FILE *fd) {}
    int set_rx_filter_mode(rx_filter_mode_e mode) { return -ENOTSUP; }
};


#endif /* __TREX_PORT_ATTR_H__ */
