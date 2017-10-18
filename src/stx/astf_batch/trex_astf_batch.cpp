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

#include "trex_astf_batch.h"

void 
TrexDpCoreAstfBatch::start_astf() {
    dsec_t d_time_flow;

    if ( !m_core->Create_tcp() ) {
        fprintf(stderr," ERROR in tcp object creation \n");
        return;
    }

    d_time_flow = m_core->m_tcp_fif_d_time; /* set by Create_tcp function */
    m_core->m_cur_time_sec =  0.01 + (double)m_core->m_thread_id * m_core->m_tcp_fif_d_time / (double)m_core->m_max_threads;


    if ( CGlobalInfo::is_realtime()  ) {
        if (m_core->m_cur_time_sec > 0.2 ) {
            m_core->m_cur_time_sec =  0.01 + m_core->m_thread_id * 0.01;
        }
        m_core->m_cur_time_sec += now_sec() + 0.1 ;
    }
    dsec_t c_stop_sec = m_core->m_cur_time_sec + m_core->m_yaml_info.m_duration_sec;
    m_core->m_stop_time_sec = c_stop_sec;
    m_core->m_cur_flow_id = 1;
    m_core->m_stats.clear();

    double old_offset=0.0;

    /* we are delaying only the generation of the traffic 
         timers/rx should Work immediately 
      */

    m_core->m_tcp_fif_d_time = d_time_flow;
    CGenNode *node = m_core->create_node();

    node->m_type = CGenNode::TCP_TX_FIF;
    node->m_time = m_core->m_cur_time_sec;
    m_core->m_node_gen.add_node(node);

    dsec_t now = now_sec() ;

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

    m_core->m_node_gen.flush_file(c_stop_sec, d_time_flow, false, m_core, old_offset);


#ifdef VALG
    CALLGRIND_STOP_INSTRUMENTATION;
    printf (" %llu \n",os_get_hr_tick_64()-_start_time);
#endif
    if ( !CGlobalInfo::m_options.preview.getNoCleanFlowClose() &&  (m_core->is_terminated_by_master()==false) ) {
        /* clean close */
        m_core->m_node_gen.flush_file(m_core->m_cur_time_sec, d_time_flow, true, m_core, old_offset);
    }

    if (m_core->m_preview_mode.getVMode() > 1 ) {
        fprintf(stdout,"\n\n");
        fprintf(stdout,"\n\n");
        fprintf(stdout,"file stats \n");
        fprintf(stdout,"=================\n");
        m_core->m_stats.dump(stdout);
    }
    m_core->m_node_gen.close_file(m_core);
}



void
TrexAstfBatch::shutdown() {
    /* shutdown all DP cores */
    send_msg_to_all_dp(new TrexDpQuit());
        
    /* stop the latency core */
    get_mg()->stop();
    delay(1000);
}


void
TrexAstfBatch::publish_async_data() {
    std::string json;

    /* backward compatible */
    get_mg()->dump_json(json);
    get_publisher()->publish_json(json);

    /* more info */
    get_mg()->dump_json_v2(json);
    get_publisher()->publish_json(json);

    CSTTCp *lpstt = get_platform_api().get_fl()->m_stt_cp;
    if (lpstt) {
        //if ( m_stats_cnt%4==0) { /* could be significat, reduce the freq */
        if (lpstt->dump_json(json)) {
            get_publisher()->publish_json(json);
        }
        //}
    }
}

