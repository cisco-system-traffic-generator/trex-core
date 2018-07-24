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


void CSyncBarrier::dump_err(){
    fprintf(stderr,"ERROR sync barrier, active threads : ");
    int i;
    for (i=0; i<m_max_ids; i++) {
        fprintf(stderr," %d ",m_arr[i]);
    }
    fprintf(stderr," \n");
}

int CSyncBarrier::sync_barrier(uint16_t thread_id){

    assert(m_atomic.cnt!=m_max_ids);
    m_arr[thread_id]=1;
    dsec_t s_time = now_sec();
    rte_atomic32_inc(&m_atomic);
    while (true) {
        if (m_atomic.cnt==m_max_ids){
            break;
        }
        if ((now_sec()-s_time)>m_timeout_sec){
            dump_err();
            return(-1);
        }
        rte_pause();
    }
    rte_rmb();
    return (0);
}





