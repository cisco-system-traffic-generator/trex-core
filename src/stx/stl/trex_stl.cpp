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

#include <iostream>
#include <unistd.h>


#include "rpc-server/trex_rpc_cmds_table.h"

#include "common/trex_rpc_cmds_common.h"

#include "trex_stl_rpc_cmds.h"
#include "trex_stl.h"
#include "trex_stl_port.h"
#include "trex_stl_messaging.h"
#include "trex_stl_dp_core.h"



using namespace std;

/***********************************************************
 * TrexStatelessRxFSLatencyStats
***********************************************************/
TrexStatelessRxFSLatencyStats::TrexStatelessRxFSLatencyStats(CRxCore* rx) {
    m_rx = rx;
}

int
TrexStatelessRxFSLatencyStats::get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch) {
    return m_rx->get_rfc2544_info(rfc2544_info, min, max, reset, period_switch);
}

int
TrexStatelessRxFSLatencyStats::get_rx_err_cntrs(CRxCoreErrCntrs* rx_err_cnts) {
    return m_rx->get_rx_err_cntrs(rx_err_cnts);
}

int
TrexStatelessRxFSLatencyStats::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset,
                                TrexPlatformApi::driver_stat_cap_e type, const vector<pair<uint8_t, uint8_t>> & core_ids) {
    return m_rx->get_rx_stats(port_id, rx_stats, min, max, reset, type);
}

void
TrexStatelessRxFSLatencyStats::reset_rx_stats(uint8_t port_id, const vector<pair<uint8_t, uint8_t>> & core_ids) {
    m_rx->reset_rx_stats(port_id);
}

/***********************************************************
 * TrexStatelessMulticoreSoftwareFSLatencyStats
***********************************************************/
TrexStatelessMulticoreSoftwareFSLatencyStats::TrexStatelessMulticoreSoftwareFSLatencyStats(TrexStateless* stl, const vector<TrexStatelessDpCore*>& dp_core_ptrs) {
    for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
        m_rfc2544_sum[i].create();
    }
    m_fs_latency_sum.create(m_rfc2544_sum, &m_err_cntrs_sum);
    m_stl = stl;
    for (TrexStatelessDpCore* dp_core_ptr : dp_core_ptrs) {
        m_dp_cores.push_back(dp_core_ptr);
        for (uint8_t dir = 0; dir < NUM_PORTS_PER_CORE; dir++) {
            m_fs_latency_dp_core_ptrs[dir].push_back(dp_core_ptr->get_fs_latency_object_ptr(dir));
            m_rfc2544_dp_core_ptrs[dir].push_back(dp_core_ptr->get_rfc2544_object_ptr(dir));
            m_err_cntrs_dp_core_ptrs[dir].push_back(dp_core_ptr->get_err_cntrs_object_ptr(dir));
        }
    }
}

void
TrexStatelessMulticoreSoftwareFSLatencyStats::export_data(rfc2544_info_t *rfc2544_info, int min, int max) {
    for (int hw_id = min; hw_id <= max; hw_id++) {
        CRFC2544Info &curr_rfc2544 = m_rfc2544_sum[hw_id];
        if (rfc2544_info != NULL) {
            curr_rfc2544.export_data(rfc2544_info[hw_id - min]);
        }
    }
}

int
TrexStatelessMulticoreSoftwareFSLatencyStats::get_rfc2544_info(rfc2544_info_t *rfc2544_info, int min, int max, bool reset, bool period_switch) {
    for (auto& dp_core : m_dp_cores) {
        dp_core->rfc2544_stop_and_sample(min, max, reset, period_switch);
    }
    // Aggregate the data from all the DP and export.
    for (int hw_id = min; hw_id <= max; hw_id++) {
        m_rfc2544_sum[hw_id].reset();
        for (uint8_t dir = 0; dir < NUM_PORTS_PER_CORE; dir++) {
            for (auto& m_rfc2544_ptr : m_rfc2544_dp_core_ptrs[dir]) {
                m_rfc2544_sum[hw_id] += m_rfc2544_ptr[hw_id];
            }
        }
    }
    export_data(rfc2544_info, min, max);
    if (reset) {
        for (auto& dp_core : m_dp_cores) {
            dp_core->rfc2544_reset(min, max);
        }
    }
    return 0;
}

int
TrexStatelessMulticoreSoftwareFSLatencyStats::get_rx_err_cntrs(CRxCoreErrCntrs* rx_err_cnts) {
    m_err_cntrs_sum.reset();
    for (uint8_t dir = 0; dir < NUM_PORTS_PER_CORE; dir++) {
        for (auto& m_err_cntrs_dp_core : m_err_cntrs_dp_core_ptrs[dir]) {
            m_err_cntrs_sum += *m_err_cntrs_dp_core;
        }
    }
    *rx_err_cnts = m_err_cntrs_sum;
    return 0;
}

int
TrexStatelessMulticoreSoftwareFSLatencyStats::get_rx_stats(uint8_t port_id, rx_per_flow_t *rx_stats, int min, int max, bool reset,
                                TrexPlatformApi::driver_stat_cap_e type, const vector<pair<uint8_t, uint8_t>> & core_ids) {
    m_fs_latency_sum.reset_stats_partial(0, MAX_FLOW_STATS - 1, TrexPlatformApi::IF_STAT_IPV4_ID);
    m_fs_latency_sum.reset_stats_partial(0, MAX_FLOW_STATS_PAYLOAD - 1, TrexPlatformApi::IF_STAT_PAYLOAD);
    for (auto& core_id_dir_tuple : core_ids) {
        m_fs_latency_sum += *(m_fs_latency_dp_core_ptrs[core_id_dir_tuple.second][core_id_dir_tuple.first]);
        if (reset) {
            m_dp_cores[core_id_dir_tuple.first]->clear_fs_latency_stats_partial(core_id_dir_tuple.second, min, max, type);
        }
    }
    m_fs_latency_sum.get_stats(rx_stats, min, max, reset, type);
    return 0;
}

void
TrexStatelessMulticoreSoftwareFSLatencyStats::reset_rx_stats(uint8_t port_id, const vector<pair<uint8_t, uint8_t>> & core_ids) {
    for (auto& core_id_dir_tuple : core_ids) {
        m_dp_cores[core_id_dir_tuple.first]->clear_fs_latency_stats(core_id_dir_tuple.second);
    }
    m_fs_latency_sum.reset_stats();
}

/***********************************************************
 * Trex stateless object
 * 
 **********************************************************/

/**
 * 
 */
TrexStateless::TrexStateless(const TrexSTXCfg &cfg) : TrexSTX(cfg) {
    /* API core version */
    const int API_VER_MAJOR = 5;
    const int API_VER_MINOR = 1;
    
    /* init the RPC table */
    TrexRpcCommandsTable::get_instance().init("STL", API_VER_MAJOR, API_VER_MINOR);
    
    /* load the RPC components for stateless */
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsCommon());
    TrexRpcCommandsTable::get_instance().load_component(new TrexRpcCmdsSTL());
    
    /* create stateless ports */
    for (int i = 0; i < get_platform_api().get_port_count(); i++) {
        if ( !CGlobalInfo::m_options.m_dummy_port_map[i] ) {
            m_ports[i] = (TrexPort *)new TrexStatelessPort(i);
        }
    }

    /* create RX core */
    CRxCore *rx = new CRxCore();
    rx->create(cfg.m_rx_cfg);

    m_rx = rx;

    m_stats = nullptr;
}


/** 
 * release all memory
 * 
 * @author imarom (08-Oct-15)
 */
TrexStateless::~TrexStateless() {

    /* release memory for ports */
    for (auto &port : m_ports) {
        delete port.second;
    }

    delete m_stats;

    /* Remove all TPG Contexts */
    for (auto itr = m_tpg_ctx_per_user.begin(); itr != m_tpg_ctx_per_user.end(); itr++) {
        delete itr->second;
    }

    /* RX core */
    delete m_rx;
}


void TrexStateless::launch_control_plane() {
    /* start RPC server */
    m_rpc_server.start();
}


/**
* shutdown the server
*/
void TrexStateless::shutdown(bool post_shutdown) {
    if ( !post_shutdown ) {
        /* stop ports */
        for (auto &port : get_port_map()) {
            /* safe to call stop even if not active */
            port.second->stop_traffic("*");
        }
        
        /* shutdown the RPC server */
        m_rpc_server.stop();
        
        /* shutdown all DP cores */
        send_msg_to_all_dp(new TrexDpQuit());
        
        /* shutdown RX */
        send_msg_to_rx(new TrexRxQuit());
    }
}


/**
 * fetch a port by ID
 * 
 */
TrexStatelessPort * TrexStateless::get_port_by_id(uint8_t port_id) {
    return (TrexStatelessPort *)TrexSTX::get_port_by_id(port_id);
}



TrexDpCore *
TrexStateless::create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) {
    TrexStatelessDpCore * lp=new TrexStatelessDpCore(thread_id, core);
    lp->set_need_to_rx(get_dpdk_mode()->dp_rx_queues()>0?true:false);
    return lp;
}

void
TrexStateless::publish_async_data() {
    // json from this class is sent only when requested. Still, we need to maintain the counters periodically.
    CFlowStatRuleMgr::instance()->periodic_update();
}

void
TrexStateless::init_stats_multiqueue(const vector<TrexStatelessDpCore*> & dp_core_ptrs) {
    assert(get_dpdk_mode()->dp_rx_queues());
    m_stats = new TrexStatelessMulticoreSoftwareFSLatencyStats(this, dp_core_ptrs);
}

void
TrexStateless::init_stats_rx() {
    assert (!get_dpdk_mode()->dp_rx_queues());
    m_stats = new TrexStatelessRxFSLatencyStats(get_stl_rx());
}

void
TrexStateless::set_latency_feature(){
    for (uint8_t core_id = 0; core_id < m_dp_core_count; core_id++) {
        static MsgReply<bool> reply;
        reply.reset();
        TrexCpToDpMsgBase* msg = new TrexStatelessDpSetLatencyFeature(reply);
        send_msg_to_dp(core_id, msg);
        reply.wait_for_reply();
    }
}

void
TrexStateless::unset_latency_feature(){
    for (uint8_t core_id = 0; core_id < m_dp_core_count; core_id++) {
        static MsgReply<bool> reply;
        reply.reset();
        TrexCpToDpMsgBase* msg = new TrexStatelessDpUnsetLatencyFeature(reply);
        send_msg_to_dp(core_id, msg);
        reply.wait_for_reply();
    }
}

void
TrexStateless::set_capture_feature(const std::set<uint8_t>& rx_ports) {
    for (auto& port_id : rx_ports) {
        TrexStatelessPort* port = get_port_by_id(port_id);
        for (auto& core_id : port->get_core_id_list()) {
            static MsgReply<bool> reply;
            reply.reset();
            TrexCpToDpMsgBase* msg = new TrexStatelessDpSetCaptureFeature(reply);
            send_msg_to_dp(core_id, msg);
            reply.wait_for_reply();
        }
    }
}

void
TrexStateless::unset_capture_feature() {
    for (uint8_t i = 0; i < get_platform_api().get_port_count(); i+=2) {
        bool port1_rx_active = false;
        bool port2_rx_active = false;
        if ( !CGlobalInfo::m_options.m_dummy_port_map[i] ) {
            port1_rx_active = TrexCaptureMngr::getInstance().is_rx_active(i);
        }
        if ( !CGlobalInfo::m_options.m_dummy_port_map[i+1] ) {
            port2_rx_active = TrexCaptureMngr::getInstance().is_rx_active(i+1);
        }
        if (port1_rx_active || port2_rx_active) {
            continue;
        } else {
            TrexStatelessPort* port = get_port_by_id(i);
            for (auto& core_id : port->get_core_id_list()) {
                static MsgReply<bool> reply;
                reply.reset();
                TrexCpToDpMsgBase* msg = new TrexStatelessDpUnsetCaptureFeature(reply);
                send_msg_to_dp(core_id, msg);
                reply.wait_for_reply();
            }
        }
    }
}

TrexStatelessFSLatencyStats* TrexStateless::get_stats() {
    assert(m_stats);
    return m_stats;
}

/**************************************
 * Tagged Packet Group
*************************************/
bool TrexStateless::create_tpg_ctx(const std::string& username,
                                   const std::vector<uint8_t>& acquired_ports,
                                   const std::vector<uint8_t>& rx_ports,
                                   const uint32_t num_tpgids) {

    if (tpg_ctx_exists(username)) {
        return false;
    }

    /**
     * Collect the relevant cores from the ports on which TPG is enabled.
     * In order to avoid duplicates (dual ports share cores), keep this a set.
     * NOTE: all_cores: represents all the cores relevant to the acquired ports.
     *       designated_cores: represents all the cores that sequenced streams are
     *                       compiled to.
     * NOTE: TPG streams are transmitted only from designated cores. Hence Tx counters
     *       are allocated only in designated cores. However, TPG needs to be enabled
     *       in all cores so we can redirect to Rx.
     **/
    std::set<uint8_t> all_cores;
    std::set<uint8_t> designated_cores;
    for (uint8_t port_id : acquired_ports) {
        TrexStatelessPort* port = get_port_by_id(port_id);
        const std::vector<uint8_t> core_ids_per_port = port->get_core_id_list();
        all_cores.insert(core_ids_per_port.begin(), core_ids_per_port.end());

        /**
         * NOTE: Sequenced streams are compiled to one core, the designated core.
         */
        uint8_t designated_core_id = get_port_by_id(port_id)->get_sequenced_stream_core();
        designated_cores.insert(designated_core_id);
    }

    // Create (core_id->is_designated) Map
    std::unordered_map<uint8_t, bool> core_id_map;
    for (uint8_t core_id : all_cores) {
        core_id_map[core_id] = designated_cores.find(core_id) != designated_cores.end();
    }

    m_tpg_ctx_per_user[username] = new TPGCpCtx(acquired_ports, rx_ports, core_id_map, num_tpgids, username);

    return true;
}

bool TrexStateless::destroy_tpg_ctx(const std::string& username) {
    if (!tpg_ctx_exists(username)) {
        return false;
    }
    delete m_tpg_ctx_per_user[username];
    m_tpg_ctx_per_user.erase(username);
    return true;
}

TPGCpCtx* TrexStateless::get_tpg_ctx(const std::string& username) {
    if (!tpg_ctx_exists(username)) {
        return nullptr;
    }
    return m_tpg_ctx_per_user[username];
}

TPGState TrexStateless::update_tpg_state(const std::string& username) {

    if (!tpg_ctx_exists(username)) {
        // Nothing to do, TPG disabled.
        return TPGState::DISABLED;
    }

    TPGCpCtx* tpg_ctx = m_tpg_ctx_per_user[username];
    assert(tpg_ctx);
    TPGState state = tpg_ctx->get_tpg_state();

    if (!tpg_ctx->is_awaiting_rx()) {
        // Not awaiting Rx, state is synced.
        return state;
    }

    // Get Rx State
    static MsgReply<int> reply;
    reply.reset();
    TrexCpToRxMsgBase* msg = new TrexStatelessRxGetTPGState(username, reply);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);
    int rc = reply.wait_for_reply();
    TPGStateUpdate rx_state = static_cast<TPGStateUpdate>(rc);

    return tpg_ctx->handle_rx_state_update(rx_state);

}

bool TrexStateless::enable_tpg_cp_rx(const std::string& username) {

    if (!tpg_ctx_exists(username)) {
        // Context should exist at this stage.
        return false;
    }

    TPGCpCtx* tpg_ctx = m_tpg_ctx_per_user[username];
    assert(tpg_ctx);

    // Add pointers to TPG Context in all the acquired ports (all the ports that can transmit)
    const std::vector<uint8_t>& ports = tpg_ctx->get_acquired_ports();
    for (uint8_t port_id : ports) {
        TrexStatelessPort* stl_port = get_port_by_id(port_id);
        stl_port->set_tpg_ctx(tpg_ctx);
    }

    // Enable the feature in Rx, this can take a long time so don't wait.
    TrexCpToRxMsgBase *msg = new TrexStatelessRxEnableTaggedPktGroup(tpg_ctx);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);

    tpg_ctx->set_tpg_state(TPGState::ENABLED_CP);
    return true;
}

bool TrexStateless::enable_tpg_dp(const std::string& username) {

    if (!tpg_ctx_exists(username)) {
        return false;
    }

    TPGCpCtx* tpg_ctx = m_tpg_ctx_per_user[username];
    assert(tpg_ctx);
    uint32_t num_tpgids = tpg_ctx->get_num_tpgids();
    const std::unordered_map<uint8_t, bool>& core_id_map = tpg_ctx->get_cores_map();

    // Set the feature in DP so we can start redirecting packets
    for (auto const& it : core_id_map) {
        uint8_t core_id = it.first;
        bool is_designated = it.second;
        static MsgReply<int> reply;
        reply.reset();
        TrexCpToDpMsgBase* msg = new TrexStatelessDpSetTPGFeature(reply, num_tpgids, is_designated);
        send_msg_to_dp(core_id, msg);
        int rc = reply.wait_for_reply();
        TPGStateUpdate dp_state = static_cast<TPGStateUpdate>(rc);
        if (dp_state == TPGStateUpdate::DP_ALLOC_FAIL) {
            tpg_ctx->set_tpg_state(TPGState::DP_ALLOC_FAILED);
            return false;
        }
    }

    // Collect pointers to DP Tx counters
    const std::vector<uint8_t>& acquired_ports = tpg_ctx->get_acquired_ports();
    for (uint8_t port : acquired_ports) {
        static MsgReply<TPGDpMgrPerSide*> reply;
        reply.reset();
        TrexCpToDpMsgBase* msg = new TrexStatelessDpGetTPGMgr(reply, port);
        send_msg_to_dp(get_port_by_id(port)->get_sequenced_stream_core(), msg);
        TPGDpMgrPerSide* mgr_ptr = reply.wait_for_reply();
        tpg_ctx->add_dp_mgr_ptr(port, mgr_ptr);
    }

    tpg_ctx->set_tpg_state(TPGState::ENABLED);
    return true;
}

bool TrexStateless::disable_tpg(const std::string& username) {

    if (!tpg_ctx_exists(username)) {
        return false;
    }

    TPGCpCtx* tpg_ctx = m_tpg_ctx_per_user[username];
    assert(tpg_ctx);
    const std::unordered_map<uint8_t, bool>& core_id_map = tpg_ctx->get_cores_map();

    // Unset the feature in DP
    // This needs to be done first so we don't keep redirecting packets when we don't have counters.
    for (auto const& it : core_id_map) {
        uint8_t core_id = it.first;
        static MsgReply<bool> reply;
        reply.reset();
        TrexCpToDpMsgBase* msg = new TrexStatelessDpUnsetTPGFeature(reply);
        send_msg_to_dp(core_id, msg);
        reply.wait_for_reply();
    }

    // Invalidate context in ports.
    const std::vector<uint8_t>& ports = tpg_ctx->get_acquired_ports();
    for (uint8_t port_id : ports) {
        TrexStatelessPort* stl_port = get_port_by_id(port_id);
        stl_port->set_tpg_ctx(nullptr);
    }

    tpg_ctx->set_tpg_state(TPGState::DISABLED_DP);

    // Unset the feature in Rx. This can take a long time deallocating the packets, hence async.
    TrexCpToRxMsgBase *msg = new TrexStatelessRxDisableTaggedPktGroup(username);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);

    /**
     NOTE: We intentionally don't cleanup the TPGStreamMgr. The states of the streams are relevant even
     if the user disables and re-enables TPG. However, since TPGStreamMgr is a singleton implemented
     as a static variable, we will not have any memory leak.
     **/
     return true;
}

void TrexStateless::clear_tpg_stats(uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag, bool untagged) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexCpToRxMsgBase* msg = new TrexStatelessRxClearTPGStats(reply, port_id, tpgid, min_tag, max_tag, unknown_tag, untagged);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);
    reply.wait_for_reply();
}

void TrexStateless::clear_tpg_tx_stats(uint8_t port_id, uint32_t tpgid) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexCpToDpMsgBase* msg = new TrexStatelessDpClearTPGTxStats(reply, port_id, tpgid);
    send_msg_to_dp(get_port_by_id(port_id)->get_sequenced_stream_core(), msg);
    reply.wait_for_reply();
}

void TrexStateless::get_tpg_unknown_tags(Json::Value& result, uint8_t port_id) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexCpToRxMsgBase *msg = new TrexStatelessRxGetTPGUnknownTags(reply, result, port_id);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);
    reply.wait_for_reply();
}

void TrexStateless::clear_tpg_unknown_tags(uint8_t port_id) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexCpToRxMsgBase *msg = new TrexStatelessRxClearTPGUnknownTags(reply, port_id);
    CNodeRing* ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode*)msg, true);
    reply.wait_for_reply();
}
