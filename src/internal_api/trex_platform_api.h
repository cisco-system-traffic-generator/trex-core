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

#ifndef __TREX_PLATFORM_API_H__
#define __TREX_PLATFORM_API_H__

#include <stdint.h>
#include <vector>
#include <string>

/**
 * Global stats
 * 
 * @author imarom (06-Oct-15)
 */
class TrexPlatformGlobalStats {
public:
    TrexPlatformGlobalStats() {
        m_stats = {0};
    }

    struct {
        double m_cpu_util;

        double m_tx_bps;
        double m_rx_bps;
       
        double m_tx_pps;
        double m_rx_pps;
       
        uint64_t m_total_tx_pkts;
        uint64_t m_total_rx_pkts;
       
        uint64_t m_total_tx_bytes;
        uint64_t m_total_rx_bytes;
       
        uint64_t m_tx_rx_errors;
    } m_stats;
};

/**
 * Per Interface stats
 * 
 * @author imarom (26-Oct-15)
 */
class TrexPlatformInterfaceStats {

public:
    TrexPlatformInterfaceStats() {
        m_stats = {0};
    }

public:

    struct {

        double m_tx_bps;
        double m_rx_bps;
       
        double m_tx_pps;
        double m_rx_pps;
       
        uint64_t m_total_tx_pkts;
        uint64_t m_total_rx_pkts;
       
        uint64_t m_total_tx_bytes;
        uint64_t m_total_rx_bytes;
       
        uint64_t m_tx_rx_errors;
    } m_stats;
};


/**
 * low level API interface
 * can be implemented by DPDK or mock 
 *  
 * @author imarom (25-Oct-15)
 */

class TrexPlatformApi {
public:

    enum driver_speed_e {
        SPEED_INVALID,
        SPEED_1G,
        SPEED_10G,
        SPEED_40G,
    };

    virtual void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const = 0;
    virtual void get_global_stats(TrexPlatformGlobalStats &stats) const = 0;
    virtual void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const = 0;

    virtual void get_interface_info(uint8_t interface_id, std::string &driver_name,
                                    driver_speed_e &speed,
                                    bool &has_crc) const = 0;

    virtual void publish_async_data_now(uint32_t key) const = 0;
    virtual uint8_t get_dp_core_count() const = 0;
    
    virtual ~TrexPlatformApi() {}
};


/**
 * DPDK implementation of the platform API
 * 
 * @author imarom (26-Oct-15)
 */
class TrexDpdkPlatformApi : public TrexPlatformApi {
public:
    void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const;
    void get_global_stats(TrexPlatformGlobalStats &stats) const;
    void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const;

    void get_interface_info(uint8_t interface_id,
                            std::string &driver_name,
                            driver_speed_e &speed,
                            bool &has_crc) const;

    void publish_async_data_now(uint32_t key) const;
    uint8_t get_dp_core_count() const;
    
};

/**
 * MOCK implementation of the platform API
 * 
 * @author imarom (26-Oct-15)
 */
class TrexMockPlatformApi : public TrexPlatformApi {
public:
    void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const;
    void get_global_stats(TrexPlatformGlobalStats &stats) const;
    void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const;

    void get_interface_info(uint8_t interface_id,
                            std::string &driver_name,
                            driver_speed_e &speed,
                            bool &has_crc) const {
        driver_name = "MOCK";
        speed = SPEED_INVALID;
        has_crc = false;
    }

    void publish_async_data_now(uint32_t key) const {}
    uint8_t get_dp_core_count() const;
};

#endif /* __TREX_PLATFORM_API_H__ */
