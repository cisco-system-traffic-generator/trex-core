/*
 Itay Marom
 Hanoch Haim
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

#include "astf/astf_db.h"
#include "bp_sim.h"
#include "stt_cp.h"
#include "utl_sync_barrier.h"
#include "trex_messaging.h"
#include "trex_astf_messaging.h"    /* TrexAstfDpStop */

#include "trex_astf.h"
#include "trex_astf_dp_core.h"
#include "trex_astf_topo.h"
#include "trex_client_config.h"
#include "tunnels/tunnel_factory.h"

using namespace std;

TrexAstfDpCore::TrexAstfDpCore(uint8_t thread_id, CFlowGenListPerThread *core) :
                TrexStatelessDpCore(thread_id, core) {
    CSyncBarrier *sync_barrier = get_astf_object()->get_barrier();
    m_flow_gen = m_core;
    m_flow_gen->set_sync_barrier(sync_barrier);
    m_flow_gen->Create_tcp_ctx();
    m_active_profile_cnt = 0;
    m_profile_states.clear();
    m_mbuf_redirect_cache = nullptr;
    m_tunnel_handler = nullptr;
    if (CGlobalInfo::m_options.m_astf_best_effort_mode) {
        // software RSS, need to create cache.
        uint8_t num_dp_cores = CGlobalInfo::m_options.preview.getCores() * CGlobalInfo::m_options.get_expected_dual_ports();
        CTcpPerThreadCtx* client_tcp_ctx = m_core->m_c_tcp; // Client context as we redirect only for clients.
        m_mbuf_redirect_cache = new MbufRedirectCache(client_tcp_ctx, num_dp_cores, m_thread_id);
    }
}

TrexAstfDpCore::~TrexAstfDpCore() {
    delete m_mbuf_redirect_cache;
    m_mbuf_redirect_cache = nullptr;
    m_flow_gen->Delete_tcp_ctx();
    delete(m_tunnel_handler);
}

void TrexAstfDpCore::stop() {
    if (m_mbuf_redirect_cache) {
        m_mbuf_redirect_cache->flush_and_stop_redirect();
    }
    TrexDpCore::stop();
}

bool TrexAstfDpCore::are_all_ports_idle() {
    return m_state == STATE_IDLE;
}

bool TrexAstfDpCore::is_port_active(uint8_t port_id) {
    return m_state != STATE_IDLE;
}

bool TrexAstfDpCore::is_profile_stop_id(profile_id_t profile_id, uint32_t stop_id) {
    return (m_flow_gen->m_c_tcp->get_stop_id(profile_id) == stop_id);
}

void TrexAstfDpCore::set_profile_stop_id(profile_id_t profile_id, uint32_t stop_id) {
    m_flow_gen->m_c_tcp->set_stop_id(profile_id, stop_id);
}

void TrexAstfDpCore::set_profile_active(profile_id_t profile_id) {
    set_profile_state(profile_id, pSTATE_ACTIVE);
    m_flow_gen->m_c_tcp->activate_profile_ctx(profile_id);
    m_flow_gen->m_s_tcp->activate_profile_ctx(profile_id);
}

void TrexAstfDpCore::set_profile_nc(profile_id_t profile_id, bool nc) {
    m_flow_gen->m_c_tcp->set_profile_nc(profile_id, nc);
    m_flow_gen->m_s_tcp->set_profile_nc(profile_id, nc);
}

bool TrexAstfDpCore::get_profile_nc(profile_id_t profile_id) {
    return m_flow_gen->m_c_tcp->get_profile_nc(profile_id);
}

void TrexAstfDpCore::set_profile_stopping(profile_id_t profile_id, bool stop_all) {
    set_profile_state(profile_id, pSTATE_WAIT);
    m_flow_gen->m_c_tcp->deactivate_profile_ctx(profile_id);
    if (stop_all) {
        m_flow_gen->m_s_tcp->deactivate_profile_ctx(profile_id);

        CPerProfileCtx* pctx = m_flow_gen->m_c_tcp->get_profile_ctx(profile_id);
        CGenNodeTXFIF* tx_node = (CGenNodeTXFIF*)pctx->m_tx_node;
        if (tx_node) {
            tx_node->m_pctx = nullptr;  // trigger stopping generate_flow() safely.
            pctx->m_tx_node = nullptr;  // prevent from unexpected reference.
        }

        CPerProfileCtx* s_pctx = m_flow_gen->m_s_tcp->get_profile_ctx(profile_id);
        CPerProfileFlowDump* flow_dump = (CPerProfileFlowDump*)s_pctx->m_flow_dump;
        if (flow_dump) {
            delete flow_dump;
            s_pctx->m_flow_dump = nullptr;
        }
    }
}

void TrexAstfDpCore::on_profile_stop_event(profile_id_t profile_id) {
    if (get_profile_state(profile_id) == pSTATE_WAIT && m_state == STATE_TRANSMITTING) {
        CPerProfileCtx* c_pctx = m_flow_gen->m_c_tcp->get_profile_ctx(profile_id);
        CPerProfileCtx* s_pctx = m_flow_gen->m_s_tcp->get_profile_ctx(profile_id);

        if (!c_pctx->is_stopped()) {
            if (m_flow_gen->m_c_tcp->profile_flow_cnt(profile_id) == 0) {
                report_finished_partial(profile_id);
                c_pctx->set_stopped();
            }
        }
        else if (!s_pctx->is_active() && m_flow_gen->m_s_tcp->profile_flow_cnt(profile_id) == 0) {
            set_profile_state(profile_id, pSTATE_LOADED);
            report_finished(profile_id);
            s_pctx->set_stopped();

            m_flow_gen->m_c_tcp->set_profile_cb(profile_id, this, nullptr);
            m_flow_gen->m_s_tcp->set_profile_cb(profile_id, this, nullptr);
        }
    }
}

static void dp_core_on_profile_stop_event(void *core, profile_id_t profile_id) {
    TrexAstfDpCore * dp_core = (TrexAstfDpCore*)core;
    dp_core->on_profile_stop_event(profile_id);
}

void TrexAstfDpCore::set_profile_stop_event(profile_id_t profile_id) {
    m_flow_gen->m_c_tcp->set_profile_cb(profile_id, this, &dp_core_on_profile_stop_event);
    m_flow_gen->m_s_tcp->set_profile_cb(profile_id, this, &dp_core_on_profile_stop_event);
}

void TrexAstfDpCore::clear_profile_stop_event_all() {
    for (auto it: m_profile_states) {
        profile_id_t profile_id = it.first;
        m_flow_gen->m_c_tcp->set_profile_cb(profile_id, nullptr, nullptr);
        m_flow_gen->m_s_tcp->set_profile_cb(profile_id, nullptr, nullptr);
    }
}

void TrexAstfDpCore::add_profile_duration(profile_id_t profile_id, double duration) {
    if (duration > 0.0) {
        CGenNodeCommand * node = (CGenNodeCommand*)m_core->create_node() ;
        uint32_t stop_id = profile_stop_id();

        set_profile_stop_id(profile_id, stop_id);

        node->m_type = CGenNode::COMMAND;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        TrexAstfDpStop * cmd = new TrexAstfDpStop(profile_id, stop_id);

        /* test this */
        m_core->m_non_active_nodes++;
        node->m_cmd = cmd;
        cmd->set_core_ptr(m_core);

        m_core->m_node_gen.add_node((CGenNode *)node);
    }
}

void TrexAstfDpCore::get_scheduler_options(profile_id_t profile_id, bool& disable_client, dsec_t& d_time_flow, double& d_phase) {
    CParserOption *go = &CGlobalInfo::m_options;

    /* do we need to disable this tread client port */
    if ( go->m_astf_mode == CParserOption::OP_ASTF_MODE_SERVR_ONLY ) {
        disable_client = true;
    } else {
        uint8_t p1;
        uint8_t p2;
        m_flow_gen->get_port_ids(p1, p2);
        if ( go->m_astf_mode == CParserOption::OP_ASTF_MODE_CLIENT_MASK && ((go->m_astf_client_mask & (0x1<<p1))==0) ) {
            disable_client=true;
        } else if ( go->m_dummy_port_map[p1] ) { // dummy port
            disable_client=true;
        }
    }

    d_time_flow = m_flow_gen->m_c_tcp->get_fif_d_time(profile_id); /* set by Create_tcp function */

    d_phase = 0.01 + (double)m_flow_gen->m_thread_id * d_time_flow / (double)m_flow_gen->m_max_threads;

    if ( CGlobalInfo::is_realtime() ) {
        if (d_phase > 0.2 ) {
            d_phase =  0.01 + m_flow_gen->m_thread_id * 0.01;
        }
    }
}

void TrexAstfDpCore::start_scheduler() {

    m_state = STATE_TRANSMITTING;
    report_dp_state();

    double old_offset = 0.0;
    CGenNode *node;

    /* sync all core to the same time */
    if ( sync_barrier() ) {
        dsec_t now = now_sec();

        m_flow_gen->m_cur_time_sec = now;

        for (auto param: m_sched_param) {
            start_profile_ctx(param.m_profile_id, param.m_duration, param.m_nc_flow_close, param.m_establish_timeout, param.m_terminate_duration, param.m_dump_interval);
        }
        m_sched_param.clear();

        node = m_flow_gen->create_node() ;
        node->m_type = CGenNode::TCP_RX_FLUSH;
        node->m_time = now;
        m_flow_gen->m_node_gen.add_node(node);

        node = m_flow_gen->create_node();
        node->m_type = CGenNode::TCP_TW;
        node->m_time = now;
        m_flow_gen->m_node_gen.add_node(node);

        node = m_flow_gen->create_node();
        node->m_type = CGenNode::FLOW_SYNC;
        node->m_time = now;
        m_flow_gen->m_node_gen.add_node(node);
    
        m_core->pre_flush_file();
        m_flow_gen->m_node_gen.flush_file(-1, 0, false, m_flow_gen, old_offset);
        m_core->post_flush_file();

        m_flow_gen->flush_tx_queue();
        if (m_state != STATE_TERMINATE) {
            m_flow_gen->m_node_gen.close_file(m_flow_gen);
        }
        m_flow_gen->m_c_tcp->cleanup_flows();
        m_flow_gen->m_s_tcp->cleanup_flows();
    } else {
        for (auto param: m_sched_param) {
            report_error(param.m_profile_id, "Could not sync DP thread for start, core ID: " + to_string(m_flow_gen->m_thread_id));
        }
        m_sched_param.clear();
    }

    if ( m_state != STATE_TERMINATE ) {
        for (auto it: m_profile_states) {
            if (it.second == pSTATE_ACTIVE || it.second == pSTATE_WAIT) {
                set_profile_state(it.first, pSTATE_LOADED);
                report_finished(it.first);
            }
        }
        m_state = STATE_IDLE;
        report_dp_state();
    }
}

void TrexAstfDpCore::start_profile_ctx(profile_id_t profile_id, double duration, bool nc, double establish_timeout, double terminate_duration, double dump_interval) {

    dsec_t d_time_flow;
    bool disable_client = false;
    double d_phase;

    /* is this needed? */
    if ( duration <= 0.0 && m_flow_gen->m_yaml_info.m_duration_sec > 0 ) {
        duration = m_flow_gen->m_yaml_info.m_duration_sec;
    }

    get_scheduler_options(profile_id, disable_client, d_time_flow, d_phase);

    if ( !disable_client ) {
        CGenNodeTXFIF *tx_node = (CGenNodeTXFIF*)m_flow_gen->create_node();

        tx_node->m_type = CGenNode::TCP_TX_FIF;
        tx_node->m_time = m_core->m_cur_time_sec + d_phase + 0.1; /* phase the transmit a bit */
        tx_node->m_time_stop = (duration > 0) ? tx_node->m_time + duration : 0.0;
        tx_node->m_set_nc = nc;
        tx_node->m_time_established = (establish_timeout > 0) ? tx_node->m_time + establish_timeout : 0.0;
        tx_node->m_terminate_duration = terminate_duration;
        tx_node->m_pctx = m_flow_gen->m_c_tcp->get_profile_ctx(profile_id);
        tx_node->m_pctx->m_tx_node = tx_node;
        m_flow_gen->m_node_gen.add_node((CGenNode*)tx_node);
    }

    if (dump_interval) {
        CPerProfileCtx* pctx = m_flow_gen->m_s_tcp->get_profile_ctx(profile_id);

        auto flow_dump = new CPerProfileFlowDump(dump_interval, pctx, this);
        pctx->m_flow_dump = flow_dump;
    }

    set_profile_active(profile_id);

    if ( disable_client ) {
        set_profile_nc(profile_id, nc);
        if (duration > 0) {
            add_profile_duration(profile_id, duration + d_phase);
        }
    }
}

void TrexAstfDpCore::stop_profile_ctx(profile_id_t profile_id, uint32_t stop_id) {
    /* skip unmatched stop_id which comes from removed profile */
    if (stop_id && !is_profile_stop_id(profile_id, stop_id)) {
        return;
    }

    set_profile_stopping(profile_id, (stop_id == 0));

    if (m_flow_gen->is_terminated_by_master()) {
        add_global_duration(0.0001); // trigger exit from node scheduler
        return;
    }

    if (stop_id) {  /* stop client only */
        CPerProfileCtx* c_pctx = m_flow_gen->m_c_tcp->get_profile_ctx(profile_id);

        if (!c_pctx->is_stopped() && (m_flow_gen->m_c_tcp->profile_flow_cnt(profile_id) == 0)) {
            report_finished_partial(profile_id);
            c_pctx->set_stopped();
        }
    }
    else {  /* stop all: should be after all clients are stopped */
        assert(m_flow_gen->m_c_tcp->profile_flow_cnt(profile_id) == 0);

        if (m_flow_gen->m_s_tcp->profile_flow_cnt(profile_id) == 0) {
            m_flow_gen->flush_tx_queue();

            set_profile_state(profile_id, pSTATE_LOADED);
            report_finished(profile_id);
            m_flow_gen->m_s_tcp->get_profile_ctx(profile_id)->set_stopped();
            return;
        }
    }
    set_profile_stop_event(profile_id);
}

void TrexAstfDpCore::parse_astf_json(profile_id_t profile_id,
                                     string *profile_buffer,
                                     string *topo_buffer,
                                     CAstfDB *astf_db,
                                     const string* tunnel_topo_buffer) {
    TrexWatchDog::IOFunction dummy;
    (void)dummy;

    if (!astf_db) {
        report_error(profile_id, "ASTF DB should be given.");
        return;
    }
    astf_db->Create();  // initialize CAstfDB instance

    string err = "";
    bool rc;

    if ( topo_buffer ) {
        try {
            TopoMngr *topo_mngr = astf_db->get_topo();
            topo_mngr->from_json_str(*topo_buffer);
            ClientCfgDB &m_cc_db = m_flow_gen->m_flow_list->m_client_config_info;
            m_cc_db.load_from_topo(topo_mngr);
            astf_db->set_client_cfg_db(&m_cc_db);
            //topo_mngr->dump();
        } catch (const TopoError &ex) {
            report_error(profile_id, ex.what());
            return;
        }
    }

    if (tunnel_topo_buffer) {
        //verifies that has tunnel handler
        assert(m_tunnel_handler);
        CTunnelsTopo* tunnel_topo = astf_db->get_tunnel_topo();
        //initialize the tunnel topo
        tunnel_topo->from_json_str(*tunnel_topo_buffer);
        // initialize the tunnel db with the tunnel topo
        CTunnelsDB* m_tunnel_db = m_flow_gen->m_flow_list->m_client_config_info.get_tunnel_db();
        m_tunnel_db->load_from_tunnel_topo(tunnel_topo);
    }

    if ( !profile_buffer ) {
        report_finished(profile_id);
        return;
    }

    rc = astf_db->set_profile_one_msg(*profile_buffer, err);
    if ( !rc ) {
        report_error(profile_id, "Profile parsing error: " + err);
        return;
    }

    // once we support specifying number of cores in start,
    // this should not be disabled by cache of profile hash
    int num_dp_cores = CGlobalInfo::m_options.preview.getCores() * CGlobalInfo::m_options.get_expected_dual_ports();
    CJsonData_err err_obj = astf_db->verify_data(num_dp_cores);

    if ( err_obj.is_error() ) {
        report_error(profile_id, "Profile split to DP cores error: " + err_obj.description());
    } else {
        report_finished(profile_id);
    }
}

void TrexAstfDpCore::remove_astf_json(profile_id_t profile_id, CAstfDB* astf_db) {
    TrexWatchDog::IOFunction dummy;
    (void)dummy;

    if (is_profile(profile_id)) {
        report_error(profile_id, "Cannot delete ASTF DB on existing profile: state " + std::to_string(get_profile_state(profile_id)));
        return;
    }
    // try removing the profile_ctx which contains statistics.
    m_flow_gen->remove_tcp_profile(profile_id);

    if (astf_db) {
        astf_db->Delete();
    }
    report_finished(profile_id);
}

void TrexAstfDpCore::create_tcp_batch(profile_id_t profile_id, double factor, CAstfDB* astf_db) {
    TrexWatchDog::IOFunction dummy;
    (void)dummy;

    CParserOption *go = &CGlobalInfo::m_options;

    m_flow_gen->m_yaml_info.m_duration_sec = go->m_duration;

    if (is_profile(profile_id)) {
        report_error(profile_id, "Profile already exists: state " + std::to_string(get_profile_state(profile_id)));
        return;
    }

    if (!astf_db) {
        report_error(profile_id, "ASTF DB should be given");
        return;
    } else {
        astf_db->set_factor(factor);
    }

    string errmsg;

    try {
        create_profile_state(profile_id);
        m_flow_gen->load_tcp_profile(profile_id, profile_cnt() == 1, astf_db);
    } catch (const Json::LogicError &ex) {
        errmsg = ex.what();
    } catch (const TrexException &ex) {
        errmsg = ex.what();
    }

    if (!errmsg.empty()) {
        remove_profile_state(profile_id);
        m_flow_gen->unload_tcp_profile(profile_id, profile_cnt() == 0, astf_db);
        m_flow_gen->remove_tcp_profile(profile_id);
        report_error(profile_id, "Could not create ASTF batch: " + errmsg);
        return;
    }

    // In case of tunnel mode we need to assign the CIpInfoBase objects with the tunnel handler and
    // tunnel context deletion function, so the tunnel context could be deleted
    if (m_tunnel_handler) {
        m_flow_gen->set_tunnel_handler(m_tunnel_handler, (void*)m_tunnel_handler->get_tunnel_ctx_del_cb());
    }
    set_profile_state(profile_id, pSTATE_LOADED);
    report_profile_ctx(profile_id);
}

void TrexAstfDpCore::delete_tcp_batch(profile_id_t profile_id, bool do_remove, CAstfDB* astf_db) {
    TrexWatchDog::IOFunction dummy;
    (void)dummy;

    try {
        if (is_profile(profile_id)) {
            if (get_profile_state(profile_id) > pSTATE_LOADED) {
                throw TrexException("Invalid profile state " + std::to_string(get_profile_state(profile_id)));
            }

            remove_profile_state(profile_id);
            m_flow_gen->unload_tcp_profile(profile_id, profile_cnt() == 0, astf_db);
        }

        if (do_remove) {
            m_flow_gen->remove_tcp_profile(profile_id);
        }
    } catch (const TrexException &ex) {
        report_error(profile_id, "Could not delete ASTF batch: " + string(ex.what()));
        return;
    }

    report_finished(profile_id);
}

void TrexAstfDpCore::start_transmit(profile_id_t profile_id, double duration, bool nc, double establish_timeout, double terminate_duration, double dump_interval) {
    if (!is_profile(profile_id)) {
        report_error(profile_id, "Start of unknown profile");
        return;
    }
    if (get_profile_state(profile_id) != pSTATE_LOADED) {
        report_error(profile_id, "Start of invalid profile state " + std::to_string(get_profile_state(profile_id)));
        return;
    }
#ifndef TREX_SIM
    /* prevent from no_memory_error, reserve 1 for add_global_duration */
    if (rte_mempool_avail_count(m_flow_gen->m_node_pool) <= 1) {
        report_error(profile_id, "Not enough node object to start_transmit");
        return;
    }
#endif

    set_profile_state(profile_id, pSTATE_STARTING);

    switch (m_state) {
    case STATE_IDLE:
        m_state = STATE_STARTING;
        report_dp_state();
        m_sched_param.clear();
    case STATE_STARTING:
        m_sched_param.push_back({profile_id, duration, nc, establish_timeout, terminate_duration, dump_interval});
        break;
    case STATE_TRANSMITTING:
        start_profile_ctx(profile_id, duration, nc, establish_timeout, terminate_duration, dump_interval);
        break;
    case STATE_STOPPING:
    default:
        report_error(profile_id, "Start in unexpected DP core state: " + std::to_string(m_state));
        break;
    }

    if (m_mbuf_redirect_cache) {
        // Start timer, it is safe in case of an already started timer.
        m_mbuf_redirect_cache->start_redirect_timer();
    }
}

void TrexAstfDpCore::stop_transmit(profile_id_t profile_id, uint32_t stop_id, bool set_nc) {
    if (!is_profile(profile_id) || get_profile_state(profile_id) < pSTATE_STARTING) {
        return; // no action for invalid profile
    }

    if (set_nc) {   // means CP requests stop client (i.e. stop_id != 0)
        set_profile_nc(profile_id, true);
        set_profile_stop_id(profile_id, stop_id);
    }

    switch (m_state) {
    case STATE_IDLE: // is stopped, just ack
        break;
    case STATE_STARTING:
        for (auto& param: m_sched_param) {
            if (param.m_profile_id == profile_id) {
                param.m_duration = 0.0001; // to trigger stop just after started.
            }
        }
        break;
    case STATE_TRANSMITTING:
        stop_profile_ctx(profile_id, stop_id);
        break;
    case STATE_STOPPING:
        set_profile_stopping(profile_id);
        break;
    default:
        report_error(profile_id, "Stop in unexpected DP core state: " + std::to_string(m_state));
        break;
    }

    if (m_mbuf_redirect_cache && (m_active_profile_cnt == 0)) {
        // Stop the timer when stopping the last profile.
        // This is safe to call even in case of a stopped timer.
        m_mbuf_redirect_cache->flush_and_stop_redirect();
    }
}

void TrexAstfDpCore::stop_transmit(profile_id_t profile_id, bool set_nc) {
    uint32_t tmp_stop_id = -1;  // request stop client
    set_profile_stop_id(profile_id, tmp_stop_id);
    stop_transmit(profile_id, tmp_stop_id, set_nc);
}

void TrexAstfDpCore::scheduler(bool activate) {
    switch (m_state) {
    case STATE_IDLE:
        // TODO: activating node scheduler by CP
        break;
    case STATE_TRANSMITTING:
        if (activate == false) {
            m_state = STATE_STOPPING;
            clear_profile_stop_event_all();

            add_global_duration(0.0001); // trigger exit from node scheduler
        }
        break;
    default:
        break;
    }
    report_dp_state();
}

void TrexAstfDpCore::set_service_mode(bool enabled, bool filtered, uint8_t mask) {
    service_status status;
    if ( enabled ) {
        status = SERVICE_ON;
    } else if ( filtered ) {
        status = SERVICE_FILTERED;
    } else {
        status = SERVICE_OFF;
    }

    m_flow_gen->m_c_tcp->m_ft.m_service_status        = status;
    m_flow_gen->m_c_tcp->m_ft.m_service_filtered_mask = mask;
    
    m_flow_gen->m_s_tcp->m_ft.m_service_status        = status;
    m_flow_gen->m_s_tcp->m_ft.m_service_filtered_mask = mask;
}

void TrexAstfDpCore::update_rate(profile_id_t profile_id, double old_new_ratio) {
    double fif_d_time = m_flow_gen->m_c_tcp->get_fif_d_time(profile_id);
    m_flow_gen->m_c_tcp->set_fif_d_time(fif_d_time*old_new_ratio, profile_id);
}

bool TrexAstfDpCore::sync_barrier() {
    return (m_flow_gen->get_sync_b()->sync_barrier(m_flow_gen->m_thread_id) == 0);
}

void TrexAstfDpCore::report_finished(profile_id_t profile_id) {
    TrexDpToCpMsgBase *msg = new TrexDpCoreStopped(m_flow_gen->m_thread_id, profile_id);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}

void TrexAstfDpCore::report_finished_partial(profile_id_t profile_id) {
    TrexDpToCpMsgBase *msg = new TrexDpCoreStopped(m_flow_gen->m_thread_id, profile_id, true);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}

void TrexAstfDpCore::report_profile_ctx(profile_id_t profile_id) {
    CPerProfileCtx* client = m_flow_gen->m_c_tcp->get_profile_ctx(profile_id);
    CPerProfileCtx* server = m_flow_gen->m_s_tcp->get_profile_ctx(profile_id);
    TrexDpToCpMsgBase *msg = new TrexDpCoreProfileCtx(m_flow_gen->m_thread_id, profile_id, client, server);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}


void TrexAstfDpCore::report_error(profile_id_t profile_id, const string &error) {
    TrexDpToCpMsgBase *msg = new TrexDpCoreError(m_flow_gen->m_thread_id, profile_id, error);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}

void TrexAstfDpCore::report_dp_state() {
    TrexDpToCpMsgBase *msg = new TrexDpCoreState(m_flow_gen->m_thread_id, m_state);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}

bool TrexAstfDpCore::rx_for_idle() {
    return m_flow_gen->handle_rx_pkts(true) > 0;
}

void inline TrexAstfDpCore::client_lookup_and_activate(uint32_t client, bool activate) {
     CIpInfoBase *ip_info = m_flow_gen->client_lookup(client);
     if (ip_info){
         if (m_tunnel_handler && !activate){
             m_tunnel_handler->delete_tunnel_ctx(ip_info->get_tunnel_ctx());
             ip_info->set_tunnel_ctx(nullptr);
         }
         ip_info->set_client_active(activate);
     }
}

void TrexAstfDpCore::activate_client(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool activate, bool is_range) {

    if (is_range){
        for (uint32_t client = msg_data[0]; client <= msg_data[1]; client++) {
            client_lookup_and_activate(client, activate);
        }
        return;
    }
    for ( auto client : msg_data)
    {
        client_lookup_and_activate(client, activate);
    }
}


Json::Value TrexAstfDpCore::client_data_to_json(void *cip_info) {
    CIpInfoBase *ip_info = (CIpInfoBase *)cip_info;
    Json::Value c_data = Json::objectValue;
   
    c_data["Found"] = 0;
    
    if (!ip_info)
        return c_data;
    
    c_data["Found"] = 1;    
    if (ip_info->is_active()) 
       c_data["state"] = "Active";
    else
       c_data["state"] = "Inactive";
    if (m_tunnel_handler) {
        if (ip_info->get_tunnel_ctx()) {
            c_data["tunnel_type"] =  m_tunnel_handler->get_tunnel_type_str();
        }
    }
    return c_data;
}

void TrexAstfDpCore::get_client_stats(std::vector<uint32_t> &msg_data, bool is_range) {

    Json::Value res = Json::objectValue;
    if (is_range){
        for (uint32_t client = msg_data[0]; client <= msg_data[1]; client++) {
            CIpInfoBase *ip_info = m_flow_gen->client_lookup(client);
            res[to_string(client)] = client_data_to_json(ip_info);
        }
    }
    else {
        for ( auto client : msg_data) {
            CIpInfoBase *ip_info = m_flow_gen->client_lookup(client);
            res[to_string(client)] = client_data_to_json(ip_info);
        }
    }
    TrexDpToCpMsgBase *msg = new TrexAstfDpSentClientStats(res);
    m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
}


void TrexAstfDpCore::dump_profile_flows(profile_id_t profile_id) {
    Json::Value res = Json::objectValue;

    std::array<CTcpPerThreadCtx*,2> ctxs{ m_flow_gen->m_c_tcp, m_flow_gen->m_s_tcp };
    for (auto ctx: ctxs) {
        if (ctx->is_profile_ctx(profile_id)) {
            ctx->get_profile_ctx(profile_id)->dump_flow_info(res);
        }
    }

    if (res.size()) {
        TrexDpToCpMsgBase *msg = new TrexAstfDpFlowInfo(profile_id, res);
        m_ring_to_cp->SecureEnqueue((CGenNode *)msg, true);
    }
}


void TrexAstfDpCore::insert_ignored_mac_addresses(std::vector<uint64_t>& mac_addresses) {
    m_flow_gen->m_s_tcp->insert_ignored_macs(mac_addresses);
    m_flow_gen->m_c_tcp->insert_ignored_macs(mac_addresses);
}


void TrexAstfDpCore::insert_ignored_ip_addresses(std::vector<uint32_t>& ip_addresses) {
    m_flow_gen->m_s_tcp->insert_ignored_ips(ip_addresses);
    m_flow_gen->m_c_tcp->insert_ignored_ips(ip_addresses);
}


void TrexAstfDpCore::update_tunnel_for_client(CAstfDB* astf_db, std::vector<client_tunnel_data_t> msg_data) {
    assert(m_tunnel_handler);
    for (auto elem : msg_data) {
        CIpInfoBase *ip_info = m_flow_gen->client_lookup(elem.client_ip);
        if (ip_info) {
           void *tunnel_ctx = ip_info->get_tunnel_ctx();
           if (tunnel_ctx){
               m_tunnel_handler->update_tunnel_ctx(&elem, tunnel_ctx);
           } else {
               void *tunnel_ctx = m_tunnel_handler->get_tunnel_ctx(&elem);
               ip_info->set_tunnel_ctx(tunnel_ctx);
           }
        }
    }
}


dp_sts_t TrexAstfDpCore::activate_tunnel_handler(bool activate, uint8_t tunnel_type, bool loopback) {
    if (activate) {
        uint8_t tunnel_mode = (uint8_t)(TUNNEL_MODE_TX | TUNNEL_MODE_RX) | (TUNNEL_MODE_DP);
        if (loopback) {
            tunnel_mode = (uint8_t) tunnel_mode | TUNNEL_MODE_LOOPBACK;
        }
        m_tunnel_handler = create_tunnel_handler(tunnel_type, tunnel_mode);
        assert(m_tunnel_handler);
        m_flow_gen->m_node_gen.m_v_if->set_tunnel(m_tunnel_handler, loopback);
        if (loopback) {
            m_flow_gen->m_s_tcp->m_tunnel_handler = m_tunnel_handler;
        }
        return m_tunnel_handler->get_counters();
    } else {
        delete m_tunnel_handler;
        m_tunnel_handler = nullptr;
        m_flow_gen->m_s_tcp->m_tunnel_handler = nullptr;
        m_flow_gen->m_node_gen.m_v_if->delete_tunnel();
        return std::make_pair(nullptr,nullptr);
    }
}
