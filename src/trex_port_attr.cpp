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

    output["promiscuous"]["enabled"] = get_promiscuous();
    output["multicast"]["enabled"]   = get_multicast();
    output["link"]["up"]             = is_link_up();
    output["speed"]                  = get_link_speed() / 1000; // make sure we have no cards of less than 1 Gbps
    output["rx_filter_mode"]         = get_rx_filter_mode();

    int mode;
    get_flow_ctrl(mode);
    output["fc"]["mode"] = mode;
}


