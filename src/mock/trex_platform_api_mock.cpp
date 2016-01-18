/*
 Itay Marom
 Cisco Systems, Inc.
*/

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

#include <internal_api/trex_platform_api.h>

void
TrexMockPlatformApi::get_global_stats(TrexPlatformGlobalStats &stats) const {

    stats.m_stats.m_cpu_util = 0;

    stats.m_stats.m_tx_bps             = 0;
    stats.m_stats.m_tx_pps             = 0;
    stats.m_stats.m_total_tx_pkts      = 0;
    stats.m_stats.m_total_tx_bytes     = 0;

    stats.m_stats.m_rx_bps             = 0;
    stats.m_stats.m_rx_pps             = 0;
    stats.m_stats.m_total_rx_pkts      = 0;
    stats.m_stats.m_total_rx_bytes     = 0;
}

void 
TrexMockPlatformApi::get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const {

}

uint8_t 
TrexMockPlatformApi::get_dp_core_count() const {
    return (1);
}

void 
TrexMockPlatformApi::port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {
    cores_id_list.push_back(std::make_pair(0, 0));
}

