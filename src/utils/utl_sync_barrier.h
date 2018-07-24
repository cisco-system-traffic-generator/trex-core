#ifndef UTL_SYNC_BR_H
#define UTL_SYNC_BR_H
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

#include <stdio.h>
#include <stdint.h>
#include "mbuf.h"
#include "os_time.h"
#include <rte_atomic.h>


class CSyncBarrier {
public:

    /**
     * Object initialized by one thread (CP)
     * 
     * @param max_ids
     * @param timeout_sec
     */
    CSyncBarrier(uint16_t max_thread_ids,
                 double   timeout_sec){
        rte_atomic32_init(&m_atomic);
        m_max_ids = max_thread_ids;
        m_timeout_sec = timeout_sec;
        m_arr = new uint8_t[max_thread_ids];
        int i;
        for (i=0; i<max_thread_ids; i++) {
            m_arr[i]=0;
        }
    }
    ~CSyncBarrier(){
        delete []m_arr;
    }

    int sync_barrier(uint16_t thread_id);

private:
    void dump_err();

private:
    rte_atomic32_t m_atomic;
    uint16_t       m_max_ids;
    double         m_timeout_sec;
    uint8_t *      m_arr;

};


#endif

