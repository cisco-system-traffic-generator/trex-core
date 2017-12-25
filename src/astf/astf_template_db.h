#ifndef ASTF_TEMPLATE_DB_H 
#define ASTF_TEMPLATE_DB_H 

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


#include "tuple_gen.h"
#include "utl_policer.h"
#include <common/n_uniform_prob.h>

class CTcpTuneables;
/* template ID type */
typedef uint16_t astf_t_id_t;
typedef uint16_t astf_thread_id_t;


/* per template RO information */
struct CAstfPerTemplateRO {
    pool_index_t        m_client_pool_idx;  /* client pool index - default is zero*/
    pool_index_t        m_server_pool_idx;  /* server pool index - default is zero*/
    uint16_t            m_destination_port; /* template dest port */
    bool                m_one_app_server;   /* single server */
    uint32_t            m_server_addr;      /* IPv4 single server addr */  
    uint32_t            m_dual_mask;        /* dual mask */
    uint32_t            m_w;                /* tuple generator w */
    double              m_k_cps;            /* kCPS for this template */

};

/* information per template that is R/W */
class CAstfPerTemplateRW {

public:
    bool Create(CTupleGeneratorSmart  * global_gen,
                astf_t_id_t             tid,
                astf_thread_id_t        thread_id,
                CAstfPerTemplateRO *    info,
                uint16_t                dual_port_id);

    void Delete();
public:
    void Dump(FILE *fd);
    uint16_t get_dest_port(){
        return(m_dest_port);
    }
    void set_limit(uint32_t limit){
        m_limit=limit+1; /* need to add 1*/
    }

    bool check_limit(){
        if (m_limit==0){
            return(false);
        }
        if (m_limit>1) {
            --m_limit;
            return(false);
        }else{
            /* stop at 1 */
            return(true);
        }
    }
    void set_tuneables(CTcpTuneables * c_tune, CTcpTuneables *s_tune) {
        m_c_tune = c_tune;
        m_s_tune = s_tune;
    }
    CTcpTuneables *get_c_tune() const { return m_c_tune;}
    CTcpTuneables *get_s_tune() const { return m_s_tune;}

public:
    CTupleTemplateGeneratorSmart  m_tuple_gen;
    pool_index_t                  m_client_pool_idx;  /* client pool index - default is zero*/
    pool_index_t                  m_server_pool_idx;  /* server pool index - default is zero*/
    CPolicer                      m_policer;
    uint16_t                      m_dest_port;
    astf_t_id_t                   m_tid ; 
    astf_thread_id_t              m_thread_id;
    CTcpTuneables               * m_c_tune;
    CTcpTuneables               * m_s_tune;
    uint32_t                      m_limit;
} __rte_cache_aligned;




/* R/W information for all templates */
class CAstfTemplatesRW {

public:
    bool Create(astf_thread_id_t           thread_id,
                astf_thread_id_t           max_threads);
    void Delete();

    CAstfPerTemplateRW * get_template_by_id(astf_t_id_t tid){
        BP_ASSERT(tid<m_cap_gen.size());
        return(m_cap_gen[tid]);
    }
    void Dump(FILE *fd);
    void add_template(CAstfPerTemplateRW *temp_rw) {m_cap_gen.push_back(temp_rw);}
    uint16_t get_num_templates() {return m_cap_gen.size();}
    void init_scheduler(std::vector<double> & dist);
    uint16_t do_schedule_template();
    CTcpTuneables *get_c_tuneables() {return m_c_tuneables;}
    CTcpTuneables *get_s_tuneables() {return m_s_tuneables;}
    void set_tuneables(CTcpTuneables *c_tune, CTcpTuneables *s_tune) {
        m_c_tuneables = c_tune;
        m_s_tuneables = s_tune;
    }

private:
    KxuNuRand *                       m_nru;
    KxuLCRand                         m_rnd;
    std::vector<CAstfPerTemplateRW *> m_cap_gen;
    astf_thread_id_t                  m_thread_id;
    astf_thread_id_t                  m_max_threads;
    CTcpTuneables *                   m_s_tuneables;
    CTcpTuneables *                   m_c_tuneables;
} __rte_cache_aligned;



#endif


