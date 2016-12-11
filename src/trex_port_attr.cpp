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

DestAttr::DestAttr(uint8_t port_id) {
    m_port_id = port_id;
    
    m_mac = CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest;
    m_type = DEST_TYPE_MAC;
    
    /* save the default */
    memcpy(m_default_mac, m_mac, 6);
}

 
/**
 * set dest as an IPv4 unresolved
 */
void 
DestAttr::set_dest(uint32_t ipv4) {
    assert(ipv4 != 0);

    m_ipv4 = ipv4;
    memset(m_mac, 0, 6); // just to be on the safe side
    m_type = DEST_TYPE_IPV4_UNRESOLVED;
}

/**
 * set dest as a resolved IPv4
 */
void
DestAttr::set_dest(uint32_t ipv4, const uint8_t *mac) {
    assert(ipv4 != 0);

    m_ipv4 = ipv4;
    
    /* source might be the same as dest (this shadows the datapath memory) */
    memmove(m_mac, mac, 6);
    m_type = DEST_TYPE_IPV4;
}

/**
 * dest dest as MAC
 * 
 */
void
DestAttr::set_dest(const uint8_t *mac) {

    m_ipv4 = 0;
    
    /* source might be the same as dest (this shadows the datapath memory) */
    memmove(m_mac, mac, 6);
    m_type = DEST_TYPE_MAC;
}

void
DestAttr::to_json(Json::Value &output) const {
    switch (m_type) {
    
    case DEST_TYPE_IPV4:
        output["type"] = "ipv4";
        output["ipv4"] = utl_uint32_to_ipv4(m_ipv4);
        output["arp"]  = utl_macaddr_to_str(m_mac);
        break;
        
    case DEST_TYPE_IPV4_UNRESOLVED:
        output["type"] = "ipv4_u";
        output["ipv4"] = utl_uint32_to_ipv4(m_ipv4);
        break;

    case DEST_TYPE_MAC:
        output["type"] = "mac";
        output["mac"]  = utl_macaddr_to_str(m_mac);
        break;

    default:
        assert(0);
    }

}

const uint8_t *
TRexPortAttr::get_src_mac() const {
    return CGlobalInfo::m_options.get_src_mac_addr(m_port_id);
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
TRexPortAttr::to_json(Json::Value &output) {

    output["src_mac"]                = utl_macaddr_to_str(get_src_mac());
    output["promiscuous"]["enabled"] = get_promiscuous();
    output["link"]["up"]             = is_link_up();
    output["speed"]                  = get_link_speed() / 1000; // make sure we have no cards of less than 1 Gbps
    output["rx_filter_mode"]         = get_rx_filter_mode();

    if (get_src_ipv4() != 0) {
        output["src_ipv4"] = utl_uint32_to_ipv4(get_src_ipv4());
    } else {
        output["src_ipv4"] = Json::nullValue;
    }


    int mode;
    get_flow_ctrl(mode);
    output["fc"]["mode"] = mode;

    m_dest.to_json(output["dest"]);

}

void
TRexPortAttr::update_src_dst_mac(uint8_t *raw_pkt) const {
    memcpy(raw_pkt, m_dest.get_dest_mac(), 6);
    memcpy(raw_pkt + 6, get_src_mac(), 6);
}

