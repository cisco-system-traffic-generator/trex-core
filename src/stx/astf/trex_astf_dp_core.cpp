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

#include "trex_astf_dp_core.h"
#include "bp_sim.h"
#include "stt_cp.h"
#include "utl_sync_barrier.h"
#include "trex_astf.h"
#include "trex_messaging.h"

TrexAstfDpCore::TrexAstfDpCore(uint8_t thread_id, CFlowGenListPerThread *core) :
                TrexDpCore(thread_id, core, STATE_IDLE) {
    CSyncBarrier *sync_barrier = get_astf_object()->get_barrier();
    m_flow_gen = m_core;
    m_flow_gen->set_sync_barrier(sync_barrier);
    m_flow_gen->Create_tcp_ctx();
    no_close = true;
    sync_stop = false;
}

TrexAstfDpCore::~TrexAstfDpCore(void) {
    m_flow_gen->Delete_tcp_ctx();
}

bool TrexAstfDpCore::are_all_ports_idle() {
    return m_state == STATE_IDLE;
}

bool TrexAstfDpCore::is_port_active(uint8_t port_id) {
    return m_state != STATE_IDLE;
}


void TrexAstfDpCore::start_scheduler() {

    dsec_t d_time_flow;

    CParserOption *go=&CGlobalInfo::m_options;

    /* do we need to disable this tread client port */
    bool disable_client=false;
    if (go->m_astf_mode==CParserOption::OP_ASTF_MODE_SERVR_ONLY) {
        disable_client=true;
    } else {
        uint8_t p1;
        uint8_t p2;
        m_flow_gen->get_port_ids(p1,p2);
        if (go->m_astf_mode==CParserOption::OP_ASTF_MODE_CLIENT_MASK && 
           ((go->m_astf_client_mask & (0x1<<p1))==0) ){
            disable_client=true;
        } else if ( go->m_dummy_port_map[p1] ) { // dummy port
            disable_client=true;
        }
    }

    d_time_flow = m_flow_gen->m_c_tcp->m_fif_d_time; /* set by Create_tcp function */

    double d_phase= 0.01 + (double)m_flow_gen->m_thread_id * d_time_flow / (double)m_flow_gen->m_max_threads;

    if ( CGlobalInfo::is_realtime()  ) {
        if (d_phase > 0.2 ) {
            d_phase =  0.01 + m_flow_gen->m_thread_id * 0.01;
        }
    }
    m_flow_gen->m_cur_flow_id = 1;
    m_flow_gen->m_stats.clear();
    m_flow_gen->Create_tcp_batch();

    double old_offset=0.0;
    CGenNode *node;

    /* sync all core to the same time */
    if ( sync_barrier() ) {
        dsec_t now = now_sec();
        dsec_t c_stop_sec = -1;
        if ( m_flow_gen->m_yaml_info.m_duration_sec > 0 ) {
            c_stop_sec = now + d_phase + m_flow_gen->m_yaml_info.m_duration_sec;
        }

        m_flow_gen->m_cur_time_sec = now;

        if ( !disable_client ) {
            node = m_flow_gen->create_node();
            node->m_type = CGenNode::TCP_TX_FIF;
            node->m_time = now + d_phase + 0.1; /* phase the transmit a bit */
            m_flow_gen->m_node_gen.add_node(node);
        }

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

        m_flow_gen->m_node_gen.flush_file(c_stop_sec, d_time_flow, false, m_flow_gen, old_offset);

        if ( !m_flow_gen->is_terminated_by_master() && !no_close ) { // close gracefully
            m_flow_gen->m_node_gen.flush_file(-1, d_time_flow, true, m_flow_gen, old_offset);
        }
        m_flow_gen->flush_tx_queue();
        m_flow_gen->m_node_gen.close_file(m_flow_gen);
        m_flow_gen->m_c_tcp->cleanup_flows();
        m_flow_gen->m_s_tcp->cleanup_flows();

        if ( sync_stop ) { // explicit stop by user
            sync_barrier();
        }
    }

    if ( m_state != STATE_TERMINATE ) {
        report_finished();
        m_state = STATE_IDLE;
    }
}

void TrexAstfDpCore::delete_tcp_batch(void) {
    m_flow_gen->Delete_tcp_batch();
    sync_barrier();
}

void TrexAstfDpCore::start_transmit(double duration) {
    assert(m_state==STATE_IDLE);

    m_flow_gen->m_yaml_info.m_duration_sec = duration;
    sync_stop = false;
    m_state = STATE_TRANSMITTING;
}

void TrexAstfDpCore::stop_transmit(void) {
    if ( m_state == STATE_IDLE ) { // is stopped, just ack
        sync_barrier();
        return;
    }

    add_global_duration(0.0001);
    sync_stop = true;
}

bool TrexAstfDpCore::sync_barrier(void) {
    return (m_flow_gen->get_sync_b()->sync_barrier(m_flow_gen->m_thread_id) == 0);
}

void TrexAstfDpCore::report_finished(void) {
    TrexDpToCpMsgBase *msg = new TrexDpCoreStopped(m_flow_gen->m_thread_id);
    m_ring_to_cp->Enqueue((CGenNode *)msg);
}

bool TrexAstfDpCore::rx_for_astf(void) {
    return m_flow_gen->handle_rx_pkts(true) > 0;
}


