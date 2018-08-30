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

#include "utl_sync_barrier.h"

using namespace std;

CSyncBarrier::CSyncBarrier(uint16_t max_thread_ids, double timeout_sec) {
    m_max_ids = max_thread_ids;
    m_arr = new uint8_t[m_max_ids];
    reset(timeout_sec);
}

void CSyncBarrier::reset(double timeout_sec) {
    rte_atomic32_init(&m_atomic);
    m_timeout_sec = timeout_sec;

    for (int i=0; i<m_max_ids; i++) {
        m_arr[i]=0;
    }
}


void CSyncBarrier::throw_err(){
    char timeout_str[10];
    snprintf(timeout_str, sizeof(timeout_str), "%.3f", m_timeout_sec);
    string err = "Sync barrier timeout " + string(timeout_str) + " sec, active DP threads:";
    for (int i=0; i<m_max_ids; i++) {
        if ( m_arr[i] == 0 ) {
            err += " " + to_string(i);
        }
    }
    throw BarrierTimeout(err);
}

int CSyncBarrier::sync_barrier(uint16_t thread_id) {

    assert(m_atomic.cnt!=m_max_ids); // This one is already used
    m_arr[thread_id]=1;
    rte_atomic32_inc(&m_atomic);
    return listen(false);
}

int CSyncBarrier::listen(bool throw_error) {
    dsec_t s_time = now_sec();
    int left;
    while (true) {
        left = m_atomic.cnt - m_max_ids;
        if (left == 0){
            break;
        }
        if ((now_sec()-s_time)>m_timeout_sec){
            if ( throw_error ) {
                throw_err();
            }
            return(left);
        }
        rte_pause();
    }
    rte_rmb();
    return (0);
}

CSyncBarrier::~CSyncBarrier(){
    delete []m_arr;
}




