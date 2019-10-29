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
#ifndef __TREX_STL_H__
#define __TREX_STL_H__

#include <stdint.h>
#include <vector>

#include "common/trex_stx.h"

class TrexStatelessPort;
class TrexStateless;

typedef std::unordered_map<uint8_t, TrexStatelessPort*> stl_port_map_t;

class TrexStatelessFSLatencyStats {
public:
    virtual ~TrexStatelessFSLatencyStats(){};
    virtual int  get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch) = 0;
    virtual int  get_rx_err_cntrs(CRxCoreErrCntrs* rx_err_cnts) = 0;
    virtual int  get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset,
                         TrexPlatformApi::driver_stat_cap_e type, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) = 0;
    virtual void reset_rx_stats(uint8_t port_id, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) = 0;
};

class TrexStatelessRxFSLatencyStats : public TrexStatelessFSLatencyStats {
private:
    CRxCore*     m_rx;
public:
    TrexStatelessRxFSLatencyStats(CRxCore* rx);
    int  get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch) override;
    int  get_rx_err_cntrs(CRxCoreErrCntrs* rx_err_cnts) override;
    int  get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset,
                        TrexPlatformApi::driver_stat_cap_e type, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) override;
    void reset_rx_stats(uint8_t port_id, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) override;
};

class TrexStatelessMulticoreSoftwareFSLatencyStats : public TrexStatelessFSLatencyStats {
private:
    std::vector<TrexStatelessDpCore*>   m_dp_cores;
    std::vector<CRxCoreErrCntrs*>       m_err_cntrs_dp_core_ptrs[NUM_PORTS_PER_CORE];
    std::vector<CRFC2544Info*>          m_rfc2544_dp_core_ptrs[NUM_PORTS_PER_CORE];
    std::vector<RXLatency*>             m_fs_latency_dp_core_ptrs[NUM_PORTS_PER_CORE];
    CRxCoreErrCntrs                     m_err_cntrs_sum;
    CRFC2544Info                        m_rfc2544_sum[MAX_FLOW_STATS_PAYLOAD];
    RXLatency                           m_fs_latency_sum;
    TrexStateless*                      m_stl;

    void export_data(rfc2544_info_t *rfc2544_info, int min, int max);
public:
    TrexStatelessMulticoreSoftwareFSLatencyStats(TrexStateless* stl, const std::vector<TrexStatelessDpCore*>& dp_core_ptrs);
    int  get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch) override;
    int  get_rx_err_cntrs(CRxCoreErrCntrs* rx_err_cnts) override;
    int  get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset,
                        TrexPlatformApi::driver_stat_cap_e type, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) override;
    void reset_rx_stats(uint8_t port_id, const std::vector<std::pair<uint8_t, uint8_t>> & core_ids) override;
};

/**
 * TRex stateless interactive object
 *
 */
class TrexStateless : public TrexSTX {
public:

    /**
     * create stateless object
     */
    TrexStateless(const TrexSTXCfg &cfg);
    virtual ~TrexStateless();


    /**
     * launch control plane
     *
     */
    void launch_control_plane();


    /**
     * shutdown
     *
     */
    void shutdown();


    /**
     * create a STL DP core
     *
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);


    /**
     * fetch stateless port object by port ID
     *
     */
    TrexStatelessPort * get_port_by_id(uint8_t port_id);


    /**
     * publish stateless async data
     *
     */
    void publish_async_data();


    /**
     * returns the port list as stateless port objects
     */
    const stl_port_map_t &get_port_map() const {
        return *(stl_port_map_t *)&m_ports;
    }


    CRxCore *get_stl_rx(){
        return static_cast<CRxCore *>(m_rx);
    }


    void init_stats_multiqueue(const std::vector<TrexStatelessDpCore*>& dp_core_ptrs);

    void init_stats_rx();

    void set_latency_feature();

    void unset_latency_feature();

    virtual void set_capture_feature(const std::set<uint8_t>& rx_ports);

    virtual void unset_capture_feature();

    TrexStatelessFSLatencyStats* get_stats();

    friend TrexStatelessMulticoreSoftwareFSLatencyStats;


protected:

    void _shutdown();

public:
    TrexStatelessFSLatencyStats*        m_stats;
};


/**
 * returns the stateless object
 * if mode is not stateless, will return NULL
 */
static inline TrexStateless * get_stateless_obj() {
    return dynamic_cast<TrexStateless *>(get_stx());
}

#endif /* __TREX_STL_H__ */

