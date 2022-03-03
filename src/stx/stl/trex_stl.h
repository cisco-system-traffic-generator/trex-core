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
#include "trex_stl_tpg.h"

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
    void shutdown(bool clear_db);
    
    
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

    /**
     * Create a new Tagged Packet Group Control Plane Context.
     *
     * @param username
     *   Username that is creating the TPG context.
     *
     * @param acquired_ports
     *   Vector of ports on which TPG will be active.
     *
     * @param rx_ports
     *   Vector of ports on which TPG will collect.
     *
     * @param num_tpgids
     *   Number of TPGIDs
     *
     * @return bool
     *   True iff the context was created successfully.
    **/
    bool create_tpg_ctx(const std::string& username,
                        const std::vector<uint8_t>& acquired_ports,
                        const std::vector<uint8_t>& rx_ports,
                        const uint32_t num_tpgids);

    /**
     * Destroy the Tagged Packet Group Control Plane Context.
     *
     * @param username
     *   Username whose TPG context we destroy.
     *
     * @return bool
     *   True iff the context was destroyed successfully.
    **/
    bool destroy_tpg_ctx(const std::string& username);

    /**
     * Get Tagged Packet Group Control Plane Context.
     * If the context doesn't exist, it return nullptr.
     *
     * @param username
     *   Username whose TPG context we are trying to get.
     *
     * @return TPGCpCtx
     *   Pointer to context if context exists, else nullptr.
     **/
    TPGCpCtx* get_tpg_ctx(const std::string& username);

    /**
     * Update/Sync the state of Tagged Packet Grouping by checking if Rx has finished
     * using a blocking message.
     *
     * @param username
     *   Username for whom we update the TPG State.
     *
     * @return TPGState
     *   The updated state.
     **/
    TPGState update_tpg_state(const std::string& username);

    /**
     * Enable Tagged Packet Grouping in Control Plane and send an Enable Message to the Rx core.
     * The message is non blocking/async.
     *
     * @param username
     *   Username for whom we enable TPG.
     *
     * @return bool
     *   True iff TPG was enabled successfully in Control Plane and message was sent to Rx.
    **/
    bool enable_tpg_cp_rx(const std::string& username);

    /**
     * Enable Tagged Packet Grouping in Data Plane by sending a message to all data planes.
     * The message is blocking.
     *
     * @param username
     *   Username for whom we enable TPG.
     *
     * @return bool
     **  True iff TPG was enabled successfully in Data Plane.
    **/
    bool enable_tpg_dp(const std::string& username);

    /**
     * Disable Tagged Packet Grouping by sending disable messages to Dps and Rx.
     * The message to DP is blocking.
     * The message to Rx is non blocking.
     *
     * @param username
     *   Username for whom we disable TPG.
     *
     * @return bool
     **  True iff TPG was disabled successfully in Data Plane.
    **/
    bool disable_tpg(const std::string& username);

    /**
     * Update Tagged Packet Group Tags in Rx.
     *
     * @param username
     *   Username whose Tags we want to update.
     *
     * @return bool
     *   Success indicator
     **/
    bool update_tpg_tags(const std::string& username);

    /**
     * Clear Tagged Packet Group stats.
     *
     * @param port_id
     *   Receiveing port on which we want to clear stats.
     *
     * @param min_tpgid
     *   Min Tpgid on which we want to clear the stats.
     *
     * @param max_tpgid
     *   Max Tpgid on which we want to clear the stats.
     *
     * @param tag_list
     *   List of tags to clear.
     **/
    void clear_tpg_stats(uint8_t port_id, uint32_t min_tpgid, uint32_t max_tpgid, const std::vector<uint16_t>& tag_list);

    /**
     * Clear Tagged Packet Group stats.
     *
     * @param port_id
     *   Receiveing port on which we want to clear stats.
     *
     * @param tpgid
     *   Tpgid on which we want to clear the stats.
     *
     * @param tag_list
     *   List of tags to clear.
     *
     * @param unknown_tag
     *   Boolean indicating if we should clear unknown tag stats.
     *
     * @param untagged
     *   Boolean indicating if we should clear untagged stats.
     **/
    void clear_tpg_stats(uint8_t port_id, uint32_t tpgid, const std::vector<uint16_t>& tag_list, bool unknown_tag, bool untagged);

    /**
     * Clear Tagged Packet Group stats.
     *
     * @param port_id
     *   Receiveing port on which we want to clear stats.
     *
     * @param tpgid
     *   Tpgid on which we want to clear the stats.
     *
     * @param min_tag
     *   Tag from which we start clearing. We clear range [min, max).
     *
     * @param max_tag
     *   Tag where we stop clearing. Non inclusive. We clear range [min, max).
     *
     * @param unknown_tag
     *   Boolean indicating if we should clear unknown tag stats.
     *
     * @param untagged
     *   Boolean indicating if we should clear untagged stats.
     **/
    void clear_tpg_stats(uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag, bool untagged);

    /**
     * Clear Tagged Packet Group Tx stats.
     *
     * @param port_id
     *   Transmitting port on which we want to clear stats.
     *
     * @param tpgid
     *   Tpgid on which we want to clear the stats.
     **/
    void clear_tpg_tx_stats(uint8_t port_id, uint32_t tpgid);

    /**
     * Get Tagged Packet Group unknown tag on this port.
     *
     * @param result
     *   Json on which we dump the tags.
     *
     * @param port_id
     *   Collecting port on which we collected the tags.
     **/
    void get_tpg_unknown_tags(Json::Value& result, uint8_t port_id);

    /**
     * Clear Tagged Packet Group unknown tags on this port.
     *
     * @param port_id
     *   Collecting port on which we want to clear the unknown tags.
     **/
    void clear_tpg_unknown_tags(uint8_t port_id);

    virtual void set_capture_feature(const std::set<uint8_t>& rx_ports);

    virtual void unset_capture_feature();

    TrexStatelessFSLatencyStats* get_stats();

    friend TrexStatelessMulticoreSoftwareFSLatencyStats;


protected:

    void _shutdown();

    /**
     * Check if Tagged Packet Group Control Plane Context exists per username.
     *
     * @param username
     *   Username that we are trying to check if he has a TPG Context.
     *
     * @return bool
     *   True if the context exists.
    **/
    bool tpg_ctx_exists(const std::string& username) {
        return m_tpg_ctx_per_user.find(username) != m_tpg_ctx_per_user.end();
    }

public:
    TrexStatelessFSLatencyStats*                    m_stats;                // TRex Stateless Flow Stats and Latency Statistics

    std::unordered_map<std::string, TPGCpCtx*>      m_tpg_ctx_per_user;     // Tagged Packet Group Control Plane Manager per User
};


/**
 * returns the stateless object 
 * if mode is not stateless, will return NULL 
 */
static inline TrexStateless * get_stateless_obj() {
    return dynamic_cast<TrexStateless *>(get_stx());
}

#endif /* __TREX_STL_H__ */

