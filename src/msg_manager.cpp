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

/*===============================================================================
 CMessagingManager
===============================================================================*/

// Creates the CMessagingManager.
bool CMessagingManager::Create(uint8_t num_dp_threads,std::string a_name){
    m_num_dp_threads=num_dp_threads;
    assert(m_dp_to_cp==0);
    assert(m_cp_to_dp==0);
    m_cp_to_dp = new CNodeRing[num_dp_threads] ;
    m_dp_to_cp = new CNodeRing[num_dp_threads];
    int master_socket_id = rte_lcore_to_socket_id(CGlobalInfo::m_socket.get_master_phy_id());
    int i;
    for (i=0; i<num_dp_threads; i++) {
        CNodeRing * lp;
        char name[100];

        lp=getRingCpToDp(i);
        sprintf(name,"%s_to_%d",(char *)a_name.c_str(),i);
        assert(lp->Create(std::string(name),1024,master_socket_id)==true);

        lp=getRingDpToCp(i);
        sprintf(name,"%s_from_%d",(char *)a_name.c_str(),i);
        assert(lp->Create(std::string(name),1024,master_socket_id)==true);

    }
    assert(m_dp_to_cp);
    assert(m_cp_to_dp);
    return (true);
}

// Deletes the CMessagingManager.
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

// Gets the CNodeRing* from Control plane to Data plane number @param: thread_id.
CNodeRing * CMessagingManager::getRingCpToDp(uint8_t thread_id){
    assert(thread_id<m_num_dp_threads);
    return (&m_cp_to_dp[thread_id]);
}

// Gets the CNodeRing* from Data plane number @param: thread_id to Control plane.
CNodeRing * CMessagingManager::getRingDpToCp(uint8_t thread_id){
    assert(thread_id<m_num_dp_threads);
    return (&m_dp_to_cp[thread_id]);

}


/*===============================================================================
 CDpToDpMessagingManager
===============================================================================*/

// Creates the CDpToDpMessagingManager.
bool CDpToDpMessagingManager::Create(uint8_t num_dp_threads, std::string a_name) {
    m_num_dp_threads = num_dp_threads;
    assert(m_dp_to_dp == nullptr); // Make sure no one calls Create on an already created manager.
    m_dp_to_dp = new CMbufRing[m_num_dp_threads];
    int master_socket_id = rte_lcore_to_socket_id(CGlobalInfo::m_socket.get_master_phy_id());
    uint16_t tx_ring_size = CGlobalInfo::m_options.m_tx_ring_size;

    for (int i = 0; i < m_num_dp_threads; i++) {
        // Create all CMBufRings.
        CMbufRing* p;
        char name[100];
        p = getRingToDp(i);
        sprintf(name, "%s_to_%d", (char *)a_name.c_str(), i);
        assert(p->Create(std::string(name), tx_ring_size, master_socket_id, false, true) == true);
    }
    return true;
}

// Deletes the CDpToDpMessagingManager
void CDpToDpMessagingManager::Delete() {

    for (int i = 0; i < m_num_dp_threads; i++) {
        CMbufRing* p;
        p = getRingToDp(i);
        p->Delete();
    }

    if (m_dp_to_dp) {
        delete [] m_dp_to_dp;
        m_dp_to_dp = nullptr;
    }
}


// Gets the CMbufRing* to data plane number @param: thread_id.
CMbufRing* CDpToDpMessagingManager::getRingToDp(uint8_t thread_id) {
    assert (thread_id < m_num_dp_threads);
    return &m_dp_to_dp[thread_id];
}


/*===============================================================================
 CMsgIns
===============================================================================*/

// Frees the messaging instance.
void CMsgIns::Free(){
    if (m_ins) {
        m_ins->Delete();
        delete m_ins;
        m_ins = NULL;
    }
}

// Get the existing instance of messaging or create a new one in case there isn't any.
CMsgIns  * CMsgIns::Ins(void){
    if (!m_ins) {
        m_ins= new CMsgIns();
    }
    assert(m_ins);
    return (m_ins);
}

// Creates a new message instance CMsgIns.
bool CMsgIns::Create(uint8_t num_threads, bool is_software_rss){

    m_is_software_rss = is_software_rss;

    bool res = m_cp_dp.Create(num_threads,"cp_dp");
    if (!res) {
        return (res);
    }
    res = m_cp_rx.Create(1, "cp_rx");
    if (!res) {
        return (res);
    }
    res = m_rx_dp.Create(num_threads, "rx_dp");
    if (!res) {
        return res;
    }

    if (m_is_software_rss) {
        return (m_dp_dp.Create(num_threads, "dp_dp"));
    }

    return true;
}

// Deletes the messaging instance by deleting all of the messaging managers.
void CMsgIns::Delete(){
    m_cp_dp.Delete();
    m_rx_dp.Delete();
    m_cp_rx.Delete();
    if (m_is_software_rss) {
        m_dp_dp.Delete();
    }
}

CMsgIns  * CMsgIns::m_ins=0;
