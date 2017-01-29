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

#include "trex_port_attr.h"
#include "bp_sim.h"

LayerConfigMAC::LayerConfigMAC(uint8_t port_id) {
    /* use this for container (DP copies from here) */
    m_src_mac = CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.src;
    m_dst_mac = CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest;
    
    m_state = STATE_UNCONFIGRED;
}

Json::Value
LayerConfigMAC::to_json() const {
    Json::Value output;
    
    output["src"] = utl_macaddr_to_str(m_src_mac);
    output["dst"] = utl_macaddr_to_str(m_dst_mac);
    
    switch (m_state) {
    case STATE_CONFIGURED:
        output["state"] = "configured";
        break;
    case STATE_UNCONFIGRED:
        output["state"] = "unconfigured";
        break;
        
    default:
        assert(0);
    }
    
    
    return output;
}

Json::Value
LayerConfigIPv4::to_json() const {
    Json::Value output;
    
    switch (m_state) {
    case STATE_NONE:
        output["state"] = "none";
        break;
        
    case STATE_UNRESOLVED:
        output["state"] = "unresolved";
        break;
        
    case STATE_RESOLVED:
        output["state"] = "resolved";
        break;
        
    default:
        assert(0);
    }
    
    if (m_state != STATE_NONE) {
        output["src"] = utl_uint32_to_ipv4(m_src_ipv4);
        output["dst"] = utl_uint32_to_ipv4(m_dst_ipv4);
    }
    
    return output;
}

void LayerConfig::set_l2_mode(const uint8_t *dst_mac) {
    /* set dst MAC */
    m_l2_config.set_dst(dst_mac);
    m_l2_config.set_state(LayerConfigMAC::STATE_CONFIGURED);
        
    /* remove any IPv4 configuration*/
    m_l3_ipv4_config.set_state(LayerConfigIPv4::STATE_NONE);
}

void LayerConfig::set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4) {
    
    /* L2 config */
    m_l2_config.set_state(LayerConfigMAC::STATE_UNCONFIGRED);
    
    /* L3 config */
    m_l3_ipv4_config.set_src(src_ipv4);
    m_l3_ipv4_config.set_dst(dst_ipv4);
    m_l3_ipv4_config.set_state(LayerConfigIPv4::STATE_UNRESOLVED);
}

void LayerConfig::set_l3_mode(uint32_t src_ipv4, uint32_t dst_ipv4, const uint8_t *resolved_mac) {

    /* L2 config */
    m_l2_config.set_dst(resolved_mac);
    m_l2_config.set_state(LayerConfigMAC::STATE_CONFIGURED);
    
    /* L3 config */
    m_l3_ipv4_config.set_src(src_ipv4);
    m_l3_ipv4_config.set_dst(dst_ipv4);
    m_l3_ipv4_config.set_state(LayerConfigIPv4::STATE_RESOLVED);
}

void
LayerConfig::on_link_down() {

    m_l2_config.set_state(LayerConfigMAC::STATE_UNCONFIGRED);
    
    if (m_l3_ipv4_config.get_state() == LayerConfigIPv4::STATE_RESOLVED) {
        m_l3_ipv4_config.set_state(LayerConfigIPv4::STATE_UNRESOLVED);
    }
}

Json::Value
LayerConfig::to_json() const {
    Json::Value output;
    
    output["ether"] = m_l2_config.to_json();
    output["ipv4"]  = m_l3_ipv4_config.to_json();
    
    return output;
}

std::string
TRexPortAttr::get_rx_filter_mode() const {
    switch (m_rx_filter_mode) {
    case RX_FILTER_MODE_ALL:
        return "all";
    case RX_FILTER_MODE_HW:
        return "hw";
    default:
        assert(0);
    }
}

void
TRexPortAttr::on_link_down() {
    m_layer_cfg.on_link_down();
}

void
TRexPortAttr::to_json(Json::Value &output) {

    output["promiscuous"]["enabled"] = get_promiscuous();
    output["multicast"]["enabled"]   = get_multicast();
    output["link"]["up"]             = is_link_up();
    output["speed"]                  = get_link_speed() / 1000; // make sure we have no cards of less than 1 Gbps
    output["rx_filter_mode"]         = get_rx_filter_mode();

    int mode;
    get_flow_ctrl(mode);
    output["fc"]["mode"] = mode;

    output["layer_cfg"] = m_layer_cfg.to_json();
}


