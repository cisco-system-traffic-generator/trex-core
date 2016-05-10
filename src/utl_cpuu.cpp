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
    m_cpu_util=0.0;
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
        double window_cpu_u = ((double)m_work/(double)m_ticks);
        /* LPF*/
        m_cpu_util = (m_cpu_util*0.75)+(window_cpu_u*0.25);
        m_ticks=0;
        m_work=0;

    }
}

/* return cpu % */
double CCpuUtlCp::GetVal(){
    return (m_cpu_util*100);
}

