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
#include <string.h>
#include "flow_stat_parser.h"
#include "trex_defs.h"
#include "trex_port_attr.h"
#include <json/json.h>

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
        double m_rx_cpu_util;
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
    enum driver_stat_cap_e {
        IF_STAT_IPV4_ID = 1,
        IF_STAT_PAYLOAD = 2,
        IF_STAT_IPV6_FLOW_LABEL = 4,
        IF_STAT_RX_BYTES_COUNT = 8, // Card support counting rx bytes
    };

    struct mac_cfg_st {
        uint8_t hw_macaddr[6];
        uint8_t src_macaddr[6];
        uint8_t dst_macaddr[6];
    };

    /**
     * interface static info
     *
     */
    struct intf_info_st {
        std::string     driver_name;
        mac_cfg_st      mac_info;
        std::string     pci_addr;
        int             numa_node;
        bool            has_crc;
    };

    virtual void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const = 0;
    virtual void get_global_stats(TrexPlatformGlobalStats &stats) const = 0;
    virtual void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const = 0;

    virtual void get_interface_info(uint8_t interface_id, intf_info_st &info) const = 0;

    virtual void publish_async_data_now(uint32_t key, bool baseline) const = 0;
    virtual void publish_async_port_attr_changed(uint8_t port_id) const = 0;
    virtual uint8_t get_dp_core_count() const = 0;
    virtual void get_interface_stat_info(uint8_t interface_id, uint16_t &num_counters, uint16_t &capabilities) const =0;
    virtual int get_flow_stats(uint8_t port_id, void *stats, void *tx_stats, int min, int max, bool reset
                               , TrexPlatformApi::driver_stat_cap_e type) const = 0;
    virtual int get_rfc2544_info(void *rfc2544_info, int min, int max, bool reset) const = 0;
    virtual int get_rx_err_cntrs(void *rx_err_cntrs) const = 0;
    virtual int reset_hw_flow_stats(uint8_t port_id) const = 0;
    virtual void get_port_num(uint8_t &port_num) const = 0;
    virtual int add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const = 0;
    virtual int del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const = 0;
    virtual void flush_dp_messages() const = 0;
    virtual int get_active_pgids(flow_stat_active_t &result) const = 0;
    virtual int get_cpu_util_full(cpu_util_full_t &result) const = 0;
    virtual int get_mbuf_util(Json::Value &result) const = 0;
    virtual CFlowStatParser *get_flow_stat_parser() const = 0;
    virtual TRexPortAttr *getPortAttrObj() const = 0;
    virtual void mark_for_shutdown() const = 0;
    virtual int get_xstats_values(uint8_t port_id, xstats_values_t &xstats_values) const = 0;
    virtual int get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names) const = 0;

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

    void get_interface_info(uint8_t interface_id, intf_info_st &info) const;

    void publish_async_data_now(uint32_t key, bool baseline) const;
    void publish_async_port_attr_changed(uint8_t port_id) const;
    uint8_t get_dp_core_count() const;
    void get_interface_stat_info(uint8_t interface_id, uint16_t &num_counters, uint16_t &capabilities) const;
    int get_flow_stats(uint8_t port_id, void *stats, void *tx_stats, int min, int max, bool reset
                       , TrexPlatformApi::driver_stat_cap_e type) const;
    int get_rfc2544_info(void *rfc2544_info, int min, int max, bool reset) const;
    int get_rx_err_cntrs(void *rx_err_cntrs) const;
    int reset_hw_flow_stats(uint8_t port_id) const;
    void get_port_num(uint8_t &port_num) const;
    virtual int add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const;
    virtual int del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const;
    void flush_dp_messages() const;
    int get_active_pgids(flow_stat_active_t &result) const;
    int get_cpu_util_full(cpu_util_full_t &result) const;
    int get_mbuf_util(Json::Value &result) const;
    void mark_for_shutdown() const;
    CFlowStatParser *get_flow_stat_parser() const;
    TRexPortAttr *getPortAttrObj() const;

    int get_xstats_values(uint8_t port_id, xstats_values_t &xstats_values) const;
    int get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names) const;
};


/**
 * for simulation
 *
 * @author imarom (25-Feb-16)
 */
class SimPlatformApi : public TrexPlatformApi {
public:

    SimPlatformApi(int dp_core_count) {
        m_dp_core_count = dp_core_count;
    }

    virtual uint8_t get_dp_core_count() const {
        return m_dp_core_count;
    }

    virtual void get_global_stats(TrexPlatformGlobalStats &stats) const {
    }

    virtual void get_interface_info(uint8_t interface_id, intf_info_st &info) const {

        info.driver_name = "TEST";
        info.has_crc = true;
        info.numa_node = 0;

        memset(&info.mac_info, 0, sizeof(info.mac_info));
    }

    virtual void get_interface_stats(uint8_t interface_id, TrexPlatformInterfaceStats &stats) const {
    }
    virtual void get_interface_stat_info(uint8_t interface_id, uint16_t &num_counters, uint16_t &capabilities) const {num_counters=128; capabilities=TrexPlatformApi::IF_STAT_IPV4_ID | TrexPlatformApi::IF_STAT_PAYLOAD; }

    virtual void port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list) const {
        for (int i = 0; i < m_dp_core_count; i++) {
             cores_id_list.push_back(std::make_pair(i, 0));
        }
    }

    virtual void publish_async_data_now(uint32_t key, bool baseline) const {

    }

    virtual void publish_async_port_attr_changed(uint8_t port_id) const {
    }

    int get_flow_stats(uint8_t port_id, void *stats, void *tx_stats, int min, int max, bool reset
                       , TrexPlatformApi::driver_stat_cap_e type) const {return 0;};
    virtual int get_rfc2544_info(void *rfc2544_info, int min, int max, bool reset) const {return 0;};
    virtual int get_rx_err_cntrs(void *rx_err_cntrs) const {return 0;};
    virtual int reset_hw_flow_stats(uint8_t port_id) const {return 0;};
    virtual void get_port_num(uint8_t &port_num) const {port_num = 2;};
    virtual int add_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const {return 0;};
    virtual int del_rx_flow_stat_rule(uint8_t port_id, uint16_t l3_type, uint8_t l4_proto
                                      , uint8_t ipv6_next_h, uint16_t id) const {return 0;};

    void flush_dp_messages() const {
    }
    int get_active_pgids(flow_stat_active_t &result) const {return 0;}
    int get_cpu_util_full(cpu_util_full_t &result) const {return 0;}
    int get_mbuf_util(Json::Value &result) const {return 0;}
    CFlowStatParser *get_flow_stat_parser() const {return new CFlowStatParser();}
    TRexPortAttr *getPortAttrObj() const {printf("Port attributes should not be used in simulation\n"); return NULL;} // dummy

    void mark_for_shutdown() const {}
    int get_xstats_values(uint8_t port_id, xstats_values_t &xstats_values) const {return 0;};
    int get_xstats_names(uint8_t port_id, xstats_names_t &xstats_names) const {return 0;};

private:
    int m_dp_core_count;
};

#endif /* __TREX_PLATFORM_API_H__ */
