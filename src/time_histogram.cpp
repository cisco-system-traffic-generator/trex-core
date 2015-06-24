#include "time_histogram.h"
#include <string.h>
#include "utl_json.h"
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




void CTimeHistogram::Reset(){
    m_max_dt=0.0;
    m_cnt =0;
    m_high_cnt =0;
	m_max_win_dt=0;
    m_max_win_last_dt=0;
    m_average=0.0;
	memset(&m_max_ar[0],0,sizeof(m_max_ar));
	m_win_cnt=0;

    int i;
    int j;
    for (i=0;i<HISTOGRAM_SIZE; i++) {
        for (j=0; j<HISTOGRAM_SIZE_LOG;j++) {
            m_hcnt[j][i]=0;
        }
    }
    for (i=0;i<HISTOGRAM_SIZE; i++) {
        for (j=0; j<HISTOGRAM_SIZE_LOG;j++) {
            m_hcnt_shadow[j][i]=0;
        }
    }
    m_cnt_shadow=0;
    m_high_cnt_shadow=0;
}

bool CTimeHistogram::Create(){
    m_min_delta =10.0/1000000.0;
    Reset();
    return (true);
}
void CTimeHistogram::Delete(){
}

bool CTimeHistogram::Add(dsec_t dt){

    m_cnt++;
    if (dt < m_min_delta) {
        return false;
    }
    m_high_cnt++;

    if ( m_max_dt < dt){
        m_max_dt = dt;
    }
	if ( m_max_win_dt < dt){
		m_max_win_dt = dt;
	}

    uint32_t d_10usec=(uint32_t)(dt*100000.0);
    // 1 10-19 usec
    //,2 -20-29 usec
    //,3,

    int j;
    for (j=0; j<HISTOGRAM_SIZE_LOG; j++) {
        uint32_t low  = d_10usec % 10;
        uint32_t high = d_10usec / 10;
        if (high == 0 ) {
            if (low>0) {
                low=low-1;
            }
            m_hcnt[j][low]++;
            break;
        }else{
            d_10usec =high;
        }
    }

    return true;
}


void CTimeHistogram::update(){

	m_max_ar[m_win_cnt]=m_max_win_dt;
    m_max_win_last_dt=m_max_win_dt;
	m_max_win_dt=0.0;
	m_win_cnt++;
	if (m_win_cnt==HISTOGRAM_QUEUE_SIZE) {
		m_win_cnt=0;
	}
    update_average();
}

double  CTimeHistogram::get_cur_average(){
    int i,j;
    uint64_t d_cnt;
    uint64_t d_cnt_high;

    d_cnt = m_cnt - m_cnt_shadow;
    m_cnt_shadow =m_cnt;
    d_cnt_high   = m_high_cnt - m_high_cnt_shadow;
    m_high_cnt_shadow =m_high_cnt;

    uint64_t low_events = d_cnt - d_cnt_high;
    uint64_t sum= low_events;
    double s = ((double)low_events * 5.0); 


    for (j=0; j<HISTOGRAM_SIZE_LOG; j++) {
        for (i=0; i<HISTOGRAM_SIZE; i++) {
            uint64_t cnt=m_hcnt[j][i];
            if (cnt  > 0 ) {
                uint64_t d= cnt - m_hcnt_shadow[j][i];
                sum += d;
                s+= ((double)d)*1.5*get_base_usec(j,i);
                m_hcnt_shadow[j][i] = cnt;
            }
        }
    }

    double c_average;
    if ( sum > 0 ) {
       c_average=s/(double)sum;
    }else{
        c_average=0.0;
    }
    return c_average;
}

void  CTimeHistogram::update_average(){
    double c_average=get_cur_average();

    // low pass filter 
    m_average = 0.5*m_average + 0.5*c_average;
}

dsec_t  CTimeHistogram::get_total_average(){
    return (get_cur_average());
}

dsec_t  CTimeHistogram::get_average_latency(){
    return (m_average);
}


uint32_t CTimeHistogram::get_usec(dsec_t d){
    return (uint32_t)(d*1000000.0);
}

void CTimeHistogram::DumpWinMax(FILE *fd){

	int i;
	uint32_t ci=m_win_cnt;

	for (i=0; i<HISTOGRAM_QUEUE_SIZE-1; i++) {
		dsec_t d=get_usec(m_max_ar[ci]);
		ci++;
		if (ci>HISTOGRAM_QUEUE_SIZE-1) {
			ci=0;
		}
	    fprintf(fd," %.0f ",d);
	}
}

void CTimeHistogram::Dump(FILE *fd){
    fprintf (fd," min_delta  : %lu usec \n",get_usec(m_min_delta));
    fprintf (fd," cnt        : %lu \n",m_cnt);
    fprintf (fd," high_cnt   : %lu \n",m_high_cnt);
    fprintf (fd," max_d_time : %lu usec\n",get_usec(m_max_dt));
    //fprintf (fd," average    : %.0f usec\n", get_total_average());
    fprintf (fd," sliding_average    : %.0f usec\n", get_average_latency());
    fprintf (fd," precent    : %.1f %%\n",(100.0*(double)m_high_cnt/(double)m_cnt));

    fprintf (fd," histogram \n");
    fprintf (fd," -----------\n");
    int i;
    int j;
    int base=10;
    for (j=0; j<HISTOGRAM_SIZE_LOG; j++) {
        for (i=0; i<HISTOGRAM_SIZE; i++) {
            if (m_hcnt[j][i] >0 ) {
                fprintf (fd," h[%lu]  :  %lu \n",(base*(i+1)),m_hcnt[j][i]);
            }
        }
        base=base*10;
    }
}

/*
 { "histogram" : [ {} ,{} ]  }

*/

void CTimeHistogram::dump_json(std::string name,std::string & json ){
    char buff[200];
    sprintf(buff,"\"%s\":{",name.c_str());
    json+=std::string(buff);

    json+=add_json("min_usec",get_usec(m_min_delta));
    json+=add_json("max_usec",get_usec(m_max_dt));
    json+=add_json("high_cnt",m_high_cnt);
    json+=add_json("cnt",m_cnt);
    //json+=add_json("t_avg",get_total_average());
    json+=add_json("s_avg",get_average_latency());
    int i;
    int j;
    uint32_t base=10;

    json+=" \"histogram\": [";
    bool first=true; 
    for (j=0; j<HISTOGRAM_SIZE_LOG; j++) {
        for (i=0; i<HISTOGRAM_SIZE; i++) {
            if (m_hcnt[j][i] >0 ) {
                if ( first ){
                    first=false;
                }else{
                    json+=",";
                }
                json+="{";
                json+=add_json("key",(base*(i+1)));
                json+=add_json("val",m_hcnt[j][i],true);
                json+="}";
            }
        }
        base=base*10;
    }
    json+="  ] } ,";

}



