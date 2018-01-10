#include "astf_template_db.h"

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#include <astf/astf_db.h>



bool CAstfPerTemplateRW::Create(CTupleGeneratorSmart  * global_gen,
                                astf_t_id_t             tid,
                                astf_thread_id_t        thread_id,
                                CAstfPerTemplateRO *    info,
                                uint16_t                dual_port_id){

    m_thread_id =thread_id ;
    m_tid   = tid;

    m_tuple_gen.Create(global_gen, 
                       info->m_client_pool_idx,
                       info->m_server_pool_idx);


    m_tuple_gen.SetSingleServer(info->m_one_app_server,
                                info->m_server_addr,
                                dual_port_id,
                                info->m_dual_mask);

    m_tuple_gen.SetW(info->m_w);
    m_dest_port = info->m_destination_port;

    m_client_pool_idx = info->m_client_pool_idx;
    m_server_pool_idx = info->m_server_pool_idx;

    // set policer give bucket size for bursts
    m_policer.set_cir(info->m_k_cps*1000.0);
    m_policer.set_level(0.0);
    m_policer.set_bucket_size(100.0);
    m_limit=0;
    return (true);
}


void CAstfPerTemplateRW::Delete(){
    m_tuple_gen.Delete();
    delete m_c_tune;
    // m_s_tune is freed as part of m_s_tuneables in CAstfDbRO
}

void CAstfPerTemplateRW::Dump(FILE *fd){
    fprintf(fd, "  port:%d\n", m_dest_port);
    fprintf(fd, "  thread_id:%d template id:%d\n", m_thread_id, m_tid);
    fprintf(fd, "  First IPs from client pool 0:\n");
    fprintf(fd, "  Client tuneable:\n");
    m_c_tune->dump(fd);
    fprintf(fd, "  Server tuneable:\n");
    m_s_tune->dump(fd);

    CTupleBase tuple;
    for (uint16_t idx = 0; idx < 20; idx++) {
        m_tuple_gen.GenerateTuple(tuple);
        printf("  c:%x(%d) s:%x(%d)\n", tuple.getClient(), tuple.getClientPort(), tuple.getServer(), tuple.getServerPort());
    }
}

void CAstfTemplatesRW::Dump(FILE *fd) {
    for (int i = 0; i < m_cap_gen.size(); i++) {
        fprintf(fd, "template %d:\n", i);
        m_cap_gen[i]->Dump(fd);
    }

}


void CAstfTemplatesRW::init_scheduler(std::vector<double> & dist){
    m_nru =new KxuNuRand(dist,&m_rnd);
}

uint16_t CAstfTemplatesRW::do_schedule_template(){
    assert(m_nru);
    return ((uint16_t)m_nru->getRandom());
}


bool CAstfTemplatesRW::Create(astf_thread_id_t           thread_id,
                              astf_thread_id_t           max_threads){
    m_thread_id = thread_id;
    m_max_threads =max_threads;
    if (thread_id!=0) {
        m_rnd.setSeed(thread_id);
    }
    m_nru = 0;
    m_c_tuneables = NULL;
    m_s_tuneables = NULL;
    return(true);
}


void CAstfTemplatesRW::Delete(){ 
    int i;
    for (i=0; i<m_cap_gen.size(); i++) {
        CAstfPerTemplateRW * lp=m_cap_gen[i];
        lp->Delete();
        delete lp;
    }

    if (m_nru) {
        delete m_nru;
    }
    delete m_c_tuneables;
    delete m_s_tuneables;
}




