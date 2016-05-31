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
    m_cpu_util.clear();
    m_cpu_util.push_back(0.0); // here it's same as insert to front
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
        double window_cpu_u = (double)m_work/m_ticks;
        /* LPF*/
        m_cpu_util_lpf = (m_cpu_util_lpf*0.75)+(window_cpu_u*0.25);
        m_cpu_util.insert(m_cpu_util.begin(), window_cpu_u);
        if (m_cpu_util.size() > history_size)
            m_cpu_util.pop_back();
        m_ticks=0;
        m_work=0;

    }
}

/* return cpu % Smoothed */
double CCpuUtlCp::GetVal(){
    return (m_cpu_util_lpf*100); // percentage
}

/* return cpu % Raw */
double CCpuUtlCp::GetValRaw(){
    return (m_cpu_util.front()*100); // percentage
}

/* return cpu utilization history */
std::vector<double> CCpuUtlCp::GetHistory(){
    return (m_cpu_util);
}
