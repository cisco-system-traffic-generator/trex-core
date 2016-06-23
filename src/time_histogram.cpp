/*
 Hanoh Haim
 Ido Barnea
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

#include <stdint.h>
#include <string.h>
#include "utl_json.h"
#include "pal/linux/sanb_atomic.h"
#include "time_histogram.h"

void CTimeHistogram::Reset() {
    m_period_data[0].reset();
    m_period_data[1].reset();
    m_period = 0;
    m_total_cnt = 0;
    m_total_cnt_high = 0;
    m_max_dt = 0;
    m_average = 0;
    memset(&m_max_ar[0],0,sizeof(m_max_ar));
    m_win_cnt = 0;

    int i;
    int j;
    for (i = 0; i < HISTOGRAM_SIZE; i++) {
        for (j = 0; j < HISTOGRAM_SIZE_LOG; j++) {
            m_hcnt[j][i] = 0;
        }
    }
}

bool CTimeHistogram::Create() {
    Reset();
    m_min_delta =10.0/1000000.0;
    return (true);
}

void CTimeHistogram::Delete() {
}

bool CTimeHistogram::Add(dsec_t dt) {
    CTimeHistogramPerPeriodData &period_elem = m_period_data[m_period];

    period_elem.inc_cnt();
    period_elem.update_sum(dt);
    period_elem.update_max(dt);

    // values smaller then certain threshold do not get into the histogram
    if (dt < m_min_delta) {
        return false;
    }
    period_elem.inc_high_cnt();

    uint32_t d_10usec = (uint32_t)(dt*100000.0);
    // 1 10-19 usec
    //,2 -20-29 usec
    //,3,

    int j;
    for (j = 0; j < HISTOGRAM_SIZE_LOG; j++) {
        uint32_t low  = d_10usec % 10;
        uint32_t high = d_10usec / 10;
        if (high == 0) {
            if (low > 0) {
                low = low - 1;
            }
            m_hcnt[j][low]++;
            break;
        } else {
            d_10usec = high;
        }
    }

    return true;
}

void CTimeHistogram::update() {
    // switch period, and get values for pervious period
    CTimeHistogramPerPeriodData &period_elem = m_period_data[m_period];
    uint8_t new_period;

    if (m_period == 0) {
        new_period = 1;
    } else {
        new_period = 0;
    }
    m_period_data[new_period].reset();
    sanb_smp_memory_barrier();
    m_period = new_period;
    sanb_smp_memory_barrier();

    m_max_ar[m_win_cnt] = period_elem.get_max();
    m_win_cnt++;
    if (m_win_cnt == HISTOGRAM_QUEUE_SIZE) {
        m_win_cnt = 0;
    }
    update_average(period_elem);
    m_total_cnt += period_elem.get_cnt();
    m_total_cnt_high += period_elem.get_high_cnt();
    if ( m_max_dt < period_elem.get_max()) {
        m_max_dt = period_elem.get_max();
    }
}

void  CTimeHistogram::update_average(CTimeHistogramPerPeriodData &period_elem) {
    double c_average;

    if (period_elem.get_cnt() != 0)
        c_average = period_elem.get_sum() / period_elem.get_cnt();
    else
        c_average = 0;

    // low pass filter
    m_average = 0.5 * m_average + 0.5 * c_average;
}

dsec_t  CTimeHistogram::get_average_latency() {
    return (m_average);
}


uint32_t CTimeHistogram::get_usec(dsec_t d) {
    return (uint32_t)(d*1000000.0);
}

void CTimeHistogram::DumpWinMax(FILE *fd) {
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

void CTimeHistogram::Dump(FILE *fd) {
    CTimeHistogramPerPeriodData &period_elem = m_period_data[get_read_period_index()];

    fprintf (fd," min_delta  : %lu usec \n", (ulong)get_usec(m_min_delta));
    fprintf (fd," cnt        : %lu \n", period_elem.get_cnt());
    fprintf (fd," high_cnt   : %lu \n", period_elem.get_high_cnt());
    fprintf (fd," max_d_time : %lu usec\n", (ulong)get_usec(m_max_dt));
    fprintf (fd," sliding_average    : %.0f usec\n", get_average_latency());
    fprintf (fd," precent    : %.1f %%\n",(100.0*(double)period_elem.get_high_cnt()/(double)period_elem.get_cnt()));

    fprintf (fd," histogram \n");
    fprintf (fd," -----------\n");
    int i;
    int j;
    int base=10;
    for (j = 0; j < HISTOGRAM_SIZE_LOG; j++) {
        for (i = 0; i < HISTOGRAM_SIZE; i++) {
            if (m_hcnt[j][i] > 0) {
                fprintf (fd," h[%u]  :  %llu \n",(base*(i+1)),(unsigned long long)m_hcnt[j][i]);
            }
        }
        base=base*10;
    }
}

// Used in statefull
void CTimeHistogram::dump_json(std::string name,std::string & json ) {
    char buff[200];
    if (name != "")
        sprintf(buff,"\"%s\":{",name.c_str());
    else
        sprintf(buff,"{");
    json+=std::string(buff);

    json += add_json("min_usec", get_usec(m_min_delta));
    json += add_json("max_usec", get_usec(m_max_dt));
    json += add_json("high_cnt", m_total_cnt_high);
    json += add_json("cnt", m_total_cnt);
    json+=add_json("s_avg", get_average_latency());
    int i;
    int j;
    uint32_t base=10;

    json+=" \"histogram\": [";
    bool first=true;
    for (j = 0; j < HISTOGRAM_SIZE_LOG; j++) {
        for (i = 0; i < HISTOGRAM_SIZE; i++) {
            if (m_hcnt[j][i] > 0) {
                if ( first ) {
                    first = false;
                }else{
                    json += ",";
                }
                json += "{";
                json += add_json("key",(base*(i+1)));
                json += add_json("val",m_hcnt[j][i],true);
                json += "}";
            }
        }
        base = base * 10;
    }
    json+="  ] } ,";
}

// Used in stateless
void CTimeHistogram::dump_json(Json::Value & json, bool add_histogram) {
    int i, j;
    uint32_t base=10;
    CTimeHistogramPerPeriodData &period_elem = m_period_data[get_read_period_index()];

    json["total_max"] = get_usec(m_max_dt);
    json["last_max"] = get_usec(period_elem.get_max());
    json["average"] = get_average_latency();

    if (add_histogram) {
        for (j = 0; j < HISTOGRAM_SIZE_LOG; j++) {
            for (i = 0; i < HISTOGRAM_SIZE; i++) {
                if (m_hcnt[j][i] > 0) {
                    std::string key = static_cast<std::ostringstream*>( &(std::ostringstream()
                                                                          << int(base * (i + 1)) ) )->str();
                    json["histogram"][key] = Json::Value::UInt64(m_hcnt[j][i]);
                }
            }
            base = base * 10;
        }
        if (m_total_cnt != m_total_cnt_high) {
            json["histogram"]["0"] = Json::Value::UInt64(m_total_cnt - m_total_cnt_high);
        }
    }
}

