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

const uint8_t DestAttr::g_dummy_mac[6] = {0x0,0x0,0x0,0x1,0x0,0x0};


DestAttr::DestAttr(uint8_t port_id) {
    m_port_id = port_id;

    m_mac = CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.dest;
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
    output["speed"]                  = get_link_speed();
    output["rx_filter_mode"]         = get_rx_filter_mode();

    if (get_src_ipv4() != 0) {
        output["src_ipv4"] = utl_uint32_to_ipv4(get_src_ipv4());
    } else {
        output["src_ipv4"] = "none";
    }


    int mode;
    get_flow_ctrl(mode);
    output["fc"]["mode"] = mode;

    m_dest.to_json(output["dest"]);

}

void
TRexPortAttr::update_src_dst_mac(uint8_t *raw_pkt) {
    memcpy(raw_pkt, get_dest().get_dest_mac(), 6);
    memcpy(raw_pkt + 6, get_src_mac(), 6);
}

