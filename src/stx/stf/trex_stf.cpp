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

#include "trex_stf.h"


void
TrexDpCoreStf::start_stf() {
    dsec_t d_time_flow;

    if ( m_core->m_cap_gen.size()==0 ) {
        fprintf(stderr," nothing to generate no template loaded \n");
        return;
    }

    m_core->m_cur_template =(m_core->m_thread_id % m_core->m_cap_gen.size());

    d_time_flow = m_core->get_delta_flow_is_sec();
    m_core->m_cur_time_sec =  0.01 + m_core->m_thread_id * m_core->m_flow_list->get_delta_flow_is_sec();

    if ( CGlobalInfo::is_realtime()  ) {
        if (m_core->m_cur_time_sec > 0.2 ) {
            m_core->m_cur_time_sec =  0.01 + m_core->m_thread_id*0.01;
        }
        m_core->m_cur_time_sec += now_sec() + 0.1 ;
    }

    dsec_t c_stop_sec = m_core->m_cur_time_sec + m_core->m_yaml_info.m_duration_sec;
    m_core->m_stop_time_sec = c_stop_sec;
    m_core->m_cur_flow_id = 1;
    m_core->m_stats.clear();

    double old_offset=0.0;


    CGenNode * node = m_core->create_node() ;
    /* add periodic */
    node->m_type = CGenNode::FLOW_FIF;
    node->m_time = m_core->m_cur_time_sec;
    m_core->m_node_gen.add_node(node);


    node = m_core->create_node() ;
    node->m_type = CGenNode::FLOW_SYNC;
    node->m_time = m_core->m_cur_time_sec + SYNC_TIME_OUT;
    m_core->m_node_gen.add_node(node);


    /* add TW only for Stateful right now */
    node = m_core->create_node() ;
    node->m_type = CGenNode::TW_SYNC;
    node->m_time = m_core->m_cur_time_sec + BUCKET_TIME_SEC ;
    m_core->m_node_gen.add_node(node);

    node = m_core->create_node() ;
    node->m_type = CGenNode::TW_SYNC1;
    node->m_time = m_core->m_cur_time_sec + BUCKET_TIME_SEC_LEVEL1 ;
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
TrexStateful::shutdown() {
    /* shutdown all DP cores */
    send_msg_to_all_dp(new TrexDpQuit());
        
    /* stop the latency core */
    get_mg()->stop();
    delay(1000);
}


void
TrexStateful::publish_async_data() {
    std::string json;
        
    dump_template_info(json);
    if (json != ""){
        get_publisher()->publish_json(json);
    }

    if (get_is_rx_check_mode()) {
        get_mg()->rx_check_dump_json(json);
        get_publisher()->publish_json(json);
    }
        
    /* backward compatible */
    get_mg()->dump_json(json);
    get_publisher()->publish_json(json);

    /* more info */
    get_mg()->dump_json_v2(json);
    get_publisher()->publish_json(json);
}


void
TrexStateful::dump_template_info(std::string & json) {

    CFlowGenListPerThread   * lpt = get_platform_api().get_fl()->m_threads_info[0];
    CFlowsYamlInfo * yaml_info = &lpt->m_yaml_info;
    if ( yaml_info->is_any_template()==false) {
        json="";
        return;
    }

    json="{\"name\":\"template_info\",\"type\":0,\"data\":[";
    int i;
    for (i=0; i<yaml_info->m_vec.size()-1; i++) {
        CFlowYamlInfo * r=&yaml_info->m_vec[i] ;
        json+="\""+ r->m_name+"\"";
        json+=",";
    }
    json+="\""+yaml_info->m_vec[i].m_name+"\"";
    json+="]}" ;
}

