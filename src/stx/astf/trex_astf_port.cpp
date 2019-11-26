/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include "publisher/trex_publisher.h"

#include "trex_astf.h"
#include "trex_astf_port.h"
#include "stl/trex_stl_dp_core.h"

using namespace std;

TrexAstfPort::TrexAstfPort(uint8_t port_id) : TrexPort(port_id) {
    m_is_service_mode_on          = false;
    m_is_service_filtered_mode_on = false;
    m_service_filtered_mask       = 0;
}

TrexAstfPort::~TrexAstfPort() {
}

void TrexAstfPort::change_state(port_state_e state) {
    m_port_state = state;
}

void TrexAstfPort::set_service_mode(bool enabled, bool filtered, uint8_t mask) {
    m_is_service_mode_on = enabled;
    m_is_service_filtered_mode_on = filtered;
    m_service_filtered_mask = mask;
}


