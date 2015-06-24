#ifndef UTL_CPUU_H
#define UTL_CPUU_H
/*
 Hanoh Haim
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

#include <stdint.h>
#include "os_time.h"
#include "mbuf.h"

class CCpuUtlDp {

public:
    CCpuUtlDp(){
        m_total_cycles=0;
        m_data=0;
    }
    inline void start_work(){
        m_data=os_get_hr_tick_64();
    }
    inline void revert(){
    }
    inline void commit(){
        m_total_cycles+=(os_get_hr_tick_64()-m_data);
    }
    inline uint64_t get_total_cycles(void){
        return ( os_get_hr_tick_64());
    }

    inline uint64_t get_work_cycles(void){
        return ( m_total_cycles );
    }

private:
    uint64_t m_total_cycles;
    uint64_t m_data;

} __rte_cache_aligned; 

class CCpuUtlCp {
public:
    void Create(CCpuUtlDp * cdp);
    void Delete();
    /* should be called each 1 sec */
    void Update();
    /* return cpu % */
    double GetVal();

private:
    CCpuUtlDp * m_dpcpu;
    double      m_cpu_util;
    uint64_t    m_last_total_cycles;
    uint64_t    m_last_work_cycles;


    // add filter 
};

#endif
