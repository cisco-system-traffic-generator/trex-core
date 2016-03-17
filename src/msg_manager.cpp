/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include "msg_manager.h"
#include "bp_sim.h"
#include <stdio.h>
#include <string>

/*TBD: need to fix socket_id for NUMA */

bool CMessagingManager::Create(uint8_t num_dp_threads,std::string a_name){
    m_num_dp_threads=num_dp_threads;
    assert(m_dp_to_cp==0);
    assert(m_cp_to_dp==0);
    m_cp_to_dp = new CNodeRing[num_dp_threads] ;
    m_dp_to_cp = new CNodeRing[num_dp_threads];
    int i;
    for (i=0; i<num_dp_threads; i++) {
        CNodeRing * lp;
        char name[100];

        lp=getRingCpToDp(i);
        sprintf(name,"%s_to_%d",(char *)a_name.c_str(),i);
        assert(lp->Create(std::string(name),1024,0)==true);

        lp=getRingDpToCp(i);
        sprintf(name,"%s_from_%d",(char *)a_name.c_str(),i);
        assert(lp->Create(std::string(name),1024,0)==true);

    }
    assert(m_dp_to_cp);
    assert(m_cp_to_dp);
    return (true);
}
void CMessagingManager::Delete(){

    int i;
    for (i=0; i<m_num_dp_threads; i++) {
        CNodeRing * lp;
        lp=getRingCpToDp(i);
        lp->Delete();
        lp=getRingDpToCp(i);
        lp->Delete();
    }

    if (m_dp_to_cp) {
        delete [] m_dp_to_cp;
        m_dp_to_cp = NULL;
    }

    if (m_cp_to_dp) {
        delete [] m_cp_to_dp;
        m_cp_to_dp = NULL;
    }

}

CNodeRing * CMessagingManager::getRingCpToDp(uint8_t thread_id){
    assert(thread_id<m_num_dp_threads);
    return (&m_cp_to_dp[thread_id]);
}

CNodeRing * CMessagingManager::getRingDpToCp(uint8_t thread_id){
    assert(thread_id<m_num_dp_threads);
    return (&m_dp_to_cp[thread_id]);

}

void CMsgIns::Free(){
    if (m_ins) {
        m_ins->Delete();
        delete m_ins;
        m_ins = NULL;
    }
}

CMsgIns  * CMsgIns::Ins(void){
    if (!m_ins) {
        m_ins= new CMsgIns();
    }
    assert(m_ins);
    return (m_ins);
}

bool CMsgIns::Create(uint8_t num_threads){

    bool res = m_cp_dp.Create(num_threads,"cp_dp");
    if (!res) {
        return (res);
    }
    res = m_cp_rx.Create(1, "cp_rx");
    if (!res) {
        return (res);
    }

    return (m_rx_dp.Create(num_threads,"rx_dp"));
}


void CMsgIns::Delete(){
    m_cp_dp.Delete();
    m_rx_dp.Delete();
    m_cp_rx.Delete();
}


CMsgIns  * CMsgIns::m_ins=0;
