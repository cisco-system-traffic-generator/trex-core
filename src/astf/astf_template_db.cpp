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

    // set policer give bucket size for bursts
    m_policer.set_cir(info->m_k_cps*1000.0);
    m_policer.set_level(0.0);
    m_policer.set_bucket_size(100.0);
    return (true);
}


void CAstfPerTemplateRW::Delete(){
    m_tuple_gen.Delete();
}

void CAstfPerTemplateRW::Dump(FILE *fd){

}



bool CAstfTemplatesRW::Create(astf_thread_id_t           thread_id,
                              astf_thread_id_t           max_threads){
    m_thread_id = thread_id;
    m_max_threads =max_threads;
    return(true);
}

void CAstfTemplatesRW::Delete(){ 

}




