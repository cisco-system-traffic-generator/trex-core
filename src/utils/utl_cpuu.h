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
#include <cstring>
#include "trex_defs.h"
#include "os_time.h"
#include "mbuf.h"

class CCpuUtlDp {

public:
    CCpuUtlDp(){
        m_data=0;
    }
    inline void  start_work1(){
        m_data=1;

    }
    inline void commit1(){
        m_data=0;
    }

    inline uint8_t sample_data(){
        return (m_data);
    }


private:
    volatile uint8_t m_data;
} __rte_cache_aligned; 


/**
 * a thin wrapper over CPU util class 
 * it uses a predicator to determine 
 * whether to start the CPU util measurement 
 * 
 * @author imarom (7/6/2017)
 */
class CCpuUtlDpPredict {
public:
    
    CCpuUtlDpPredict() {
        m_cpu_util  = NULL;
        m_predictor = false;
    }
    
    void create(CCpuUtlDp *cpu_util) {
        m_cpu_util = cpu_util;
    }
    
    /**
     * start with heuristics 
     * if the predicator guesses it won't be needed 
     * then it won't start the measurement 
     *  
     * use a simple 1-bit predictor based on history 
     */
    inline void start_heur() {
        if (m_predictor) {
            m_cpu_util->start_work1();
        }
    }
    
    /**
     * update the predicator with the correct state 
     */
    inline void update(bool result) {
        /* if the predicator expected the correct state - do nothing */
        if (result == m_predictor) {
            return;
        }
        
        if (result && !m_predictor) {
            /* if the predicator was wrong and there was work to be done - start as soon as possible */
            m_cpu_util->start_work1();
        } else {
            /* if the predicator was wrong and no work was to be done - stop as soon as possible */
            m_cpu_util->commit1();
        }
        
        /* anyway - update the predicator for next time */
        m_predictor = result;
    }
    
    inline void commit() {
        m_cpu_util->commit1();
    }
    
private:
    CCpuUtlDp *m_cpu_util;
    bool       m_predictor;
};


class CCpuUtlCp {
public:
    void Create(CCpuUtlDp * cdp);
    void Delete();
    /* should be called each 1 sec */
    void Update();
    /* return cpu % */
    double GetVal();
    uint8_t GetValRaw();
    void GetHistory(cpu_vct_st &cpu_vct);
private:
    void AppendHistory(uint8_t);
    CCpuUtlDp *          m_dpcpu;
    uint32_t             m_ticks;
    uint32_t             m_work;

    static const int    m_history_size=20;
    uint8_t             m_cpu_util[m_history_size]; // history as cyclic array
    uint8_t             m_history_latest_index;
    double              m_cpu_util_lpf;
};

#endif
