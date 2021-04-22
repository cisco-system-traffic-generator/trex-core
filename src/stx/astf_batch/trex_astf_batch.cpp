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

#include "astf/astf_db.h"
#include "trex_astf_batch.h"
#include "trex_global.h"
#include "utl_sync_barrier.h"


TrexDpCoreAstfBatch::TrexDpCoreAstfBatch(uint32_t thread_id, CFlowGenListPerThread *core) :
        TrexDpCore(thread_id, core, STATE_TRANSMITTING) {
    m_core->Create_tcp_ctx();
}

TrexDpCoreAstfBatch::~TrexDpCoreAstfBatch(void) {
    m_core->Delete_tcp_ctx();
}


void 
TrexDpCoreAstfBatch::start_astf() {
    dsec_t d_time_flow;

    CParserOption *go=&CGlobalInfo::m_options;

    /* do we need to disable this tread client port */
    bool disable_client=false;
    if (go->m_astf_mode==CParserOption::OP_ASTF_MODE_SERVR_ONLY) {
        disable_client=true;
    }else{
        uint8_t p1;
        uint8_t p2;
        m_core->get_port_ids(p1,p2);
        if (go->m_astf_mode==CParserOption::OP_ASTF_MODE_CLIENT_MASK && 
           ((go->m_astf_client_mask & (0x1<<p1))==0) ){
            disable_client=true;
        } else if ( go->m_dummy_port_map[p1] ) { // dummy port
            disable_client=true;
        }
    }

    try {
        m_core->load_tcp_profile();
    } catch (const TrexException &ex) {
        std::cerr << "ERROR in ASTF object creation: " << ex.what();
        return;
    }

    d_time_flow = m_core->m_c_tcp->get_fif_d_time(0); /* set by Create_tcp function */

    double d_phase= 0.01 + (double)m_core->m_thread_id * d_time_flow / (double)m_core->m_max_threads;


    if ( CGlobalInfo::is_realtime()  ) {
        if (d_phase > 0.2 ) {
            d_phase =  0.01 + m_core->m_thread_id * 0.01;
        }
    }
    m_core->m_cur_flow_id = 1;
    m_core->m_stats.clear();

    double old_offset=0.0;

    /* we are delaying only the generation of the traffic 
         timers/rx should Work immediately 
      */

    /* sync all core to the same time */
    CSyncBarrier * b=m_core->get_sync_b();
    if (b) {
       assert(b->sync_barrier(m_core->m_thread_id)==0);
    }
    dsec_t now= now_sec() ;

    dsec_t c_stop_sec = now + d_phase + m_core->m_yaml_info.m_duration_sec;

    m_core->m_cur_time_sec = now;

    CGenNode *node=0;

    if (!disable_client){
        CGenNodeTXFIF *tx_node = (CGenNodeTXFIF*)m_core->create_node();
        tx_node->m_type = CGenNode::TCP_TX_FIF;
        tx_node->m_time = now + d_phase + 0.1; /* phase the transmit a bit */
        tx_node->m_time_stop = 0.0;
        tx_node->m_time_established = 0.0;
        tx_node->m_terminate_duration = 0.0;
        tx_node->m_set_nc = false;
        tx_node->m_pctx = DEFAULT_PROFILE_CTX(m_core->m_c_tcp);
        m_core->m_node_gen.add_node((CGenNode*)tx_node);
    }

    node = m_core->create_node() ;
    node->m_type = CGenNode::TCP_RX_FLUSH;
    node->m_time = now;
    m_core->m_node_gen.add_node(node);

    node = m_core->create_node();
    node->m_type = CGenNode::TCP_TW;
    node->m_time = now;
    m_core->m_node_gen.add_node(node);

    node = m_core->create_node();
    node->m_type = CGenNode::FLOW_SYNC;
    node->m_time = now;
    m_core->m_node_gen.add_node(node);

#ifdef _DEBUG
    if ( m_core->m_preview_mode.getVMode() >2 ) {

        CGenNode::DumpHeader(stdout);
    }
#endif
    m_core->pre_flush_file();
    m_core->m_node_gen.flush_file(c_stop_sec, d_time_flow, false, m_core, old_offset);
    m_core->post_flush_file();

#ifdef VALG
    CALLGRIND_STOP_INSTRUMENTATION;
    printf (" %llu \n",os_get_hr_tick_64()-_start_time);
#endif
    if ( !CGlobalInfo::m_options.preview.getNoCleanFlowClose() &&  (m_core->is_terminated_by_master()==false) ) {
        /* clean close */
        m_core->m_node_gen.flush_file(0, d_time_flow, true, m_core, old_offset);
    }

    if (m_core->m_preview_mode.getVMode() > 1 ) {
        fprintf(stdout,"\n\n");
        fprintf(stdout,"\n\n");
        fprintf(stdout,"file stats \n");
        fprintf(stdout,"=================\n");
        m_core->m_stats.dump(stdout);
    }
    m_core->m_node_gen.close_file(m_core);

    m_core->m_c_tcp->cleanup_flows();
    m_core->m_s_tcp->cleanup_flows();

#ifdef TREX_SIM
   /* this is only for simulator, for calling this in ASTF there is a need to make sure all cores stopped 
      in ASTF interactive we have state machine for testing this 
      for now keeping this for simulation valgrind testing here
    */
    m_core->unload_tcp_profile();
#endif
}



void
TrexAstfBatch::shutdown(bool post_shutdown) {
    if ( !post_shutdown ) {
        /* shutdown all DP cores */
        send_msg_to_all_dp(new TrexDpQuit());
            
        /* stop the latency core */
        get_mg()->stop();
        delay(1000);
    } else {
        CAstfDB::free_instance();
    }
}


void
TrexAstfBatch::publish_async_data() {

}

