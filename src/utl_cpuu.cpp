#include "utl_cpuu.h"
#include <stdio.h>
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



void CCpuUtlCp::Create(CCpuUtlDp * cdp){
    m_dpcpu=cdp;
    memset(m_cpu_util, 0, sizeof(m_cpu_util));
    m_history_latest_index = 0;
    m_cpu_util_lpf=0.0;
    m_ticks=0;
    m_work=0;
}

void CCpuUtlCp::Delete(){

}


void CCpuUtlCp::Update(){
    m_ticks++ ;
    if ( m_dpcpu->sample_data() ) {
        m_work++;
    }
    if (m_ticks==100) {
        /* LPF*/
        m_cpu_util_lpf = (m_cpu_util_lpf*0.75)+((double)m_work*0.25);
        AppendHistory(m_work);
        m_ticks=0;
        m_work=0;
    }
}

/* return cpu % Smoothed */
double CCpuUtlCp::GetVal(){
    return (m_cpu_util_lpf);
}

/* return cpu % Raw */
uint8_t CCpuUtlCp::GetValRaw(){
    return (m_cpu_util[m_history_latest_index]);
}

/* return cpu % utilization history */
std::vector<uint8_t> CCpuUtlCp::GetHistory(){
    std::vector<uint8_t> history_vect;
    for (int i = m_history_latest_index + m_history_size; i > m_history_latest_index; i--) {
        history_vect.insert(history_vect.begin(), m_cpu_util[i % m_history_size]);
    }
    return (history_vect);
}

/* save last CPU % util in history */
void CCpuUtlCp::AppendHistory(uint8_t val){
    m_history_latest_index = (m_history_latest_index + 1) % m_history_size;
    m_cpu_util[m_history_latest_index] = val;
}
