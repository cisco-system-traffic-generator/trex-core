#ifndef C_TIME_HISTOGRAM_H
#define C_TIME_HISTOGRAM_H
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
#include <stdio.h>
#include <math.h>
#include <string>
#include <float.h>
#include <json/json.h>
#include "mbuf.h"
#include "os_time.h"
#include "hdr_histogram.h"
/* unfortunately hdr_histograh.h pulls in stdbool.h and this causes redefinition
   for true and false in other code. So we have to undefine them here */
#undef true
#undef false

class CTimeHistogramPerPeriodData {
 public:
    void reset() {
        m_sum = 0;
        m_cnt = 0;
        m_cnt_high = 0;
        m_max = 0;
        m_min = DBL_MAX;
    }
    void inc_cnt() {m_cnt++;}
    void inc_high_cnt() {m_cnt_high++;}
    void update_max(dsec_t dt) {
        if (dt > m_max) {
            m_max = dt;
        }
    }
    void update_min(dsec_t dt) {
        if (dt < m_min) {
            m_min = dt;
        }
    }
    void update_sum(dsec_t dt) {
        m_sum += dt;
    }
    inline dsec_t get_sum() {return m_sum;}
    inline uint64_t get_cnt() {return m_cnt;}
    inline uint64_t get_high_cnt() {return m_cnt_high;}
    inline dsec_t get_max() {return m_max;}
    inline dsec_t get_max_usec() {return m_max * 1000000;}
    inline dsec_t get_min() {return m_min;}
    inline CTimeHistogramPerPeriodData operator+= (const CTimeHistogramPerPeriodData& in) {
        this->m_sum += in.m_sum;
        this->m_cnt += in.m_cnt;
        this->m_cnt_high += in.m_cnt_high; // assuming they have the same threshold.
        this->m_max = std::max(this->m_max, in.m_max);
        this->m_min = std::min(this->m_min, in.m_min);
        return *this;
    }


 private:
    dsec_t m_sum; // Sum of samples in seconds
    uint64_t m_cnt;  // Number of samples
    uint64_t m_cnt_high;  // Number of samples above configured threshold
    dsec_t   m_max;  // Max sample in seconds
    dsec_t   m_min;  // Min sample in seconds
};

class CTimeHistogram {
public:
    enum {
        HISTOGRAM_SIZE=9,
        HISTOGRAM_SIZE_LOG=5,
        HISTOGRAM_QUEUE_SIZE=14,
        HOT_MAX_PERIOD_UPDATE_CALLS = 4, // Represents the number of times update() needs to be called before considering the hot 
        // Max Latency period done. Update() is called every 0.5 secs, hence for now the HOT_MAX_PERIOD is 2 secs.
    };
    bool Create(void);
    void Delete();
    void Reset();
    bool Add(dsec_t dt);
    void set_hot_max_cnt(uint32_t hot){
        m_hot_max = hot;
    }
    void Dump(FILE *fd);
    void DumpWinMax(FILE *fd);
    /* should be called once each sampling period */
    void update();
    inline dsec_t get_average_latency() {
        // Get average of total data in usec.
        return get_usec(m_average);
    }
    inline dsec_t get_max_latency() {
        // Returns the Max latency in usec.
        return (get_usec(m_max_dt));
    }
    inline dsec_t get_max_latency_last_update() {
        // Returns the max latency in the last update in usec.
        CTimeHistogramPerPeriodData &period_elem = m_period_data[get_read_period_index()];
        return period_elem.get_max_usec();
    }
    void dump_json(std::string name,std::string & json );
    void dump_json(Json::Value & json, bool add_histogram = true);
    inline uint64_t get_count() {return m_total_cnt;}
    inline uint64_t get_high_count() {return m_total_cnt_high;}
    CTimeHistogram operator+= (const CTimeHistogram& in);
    friend std::ostream& operator<<(std::ostream& os, const CTimeHistogram& in);

private:
    inline uint32_t get_usec(dsec_t d) {
        return (uint32_t)(d*1000000.0);
    }
    void update_average(CTimeHistogramPerPeriodData &period_elem);
    inline uint8_t get_read_period_index() {
        return 1 - m_period;
    }

private:
    dsec_t   m_min_delta;/* set to 10usec*/
    // One element collects data for current period, other is saved for sending report.
    // Each period we switch between the two
    CTimeHistogramPerPeriodData m_period_data[2];
    uint64_t m_short_latency;
    uint8_t  m_period; // 0 or 1 according to m_period_data element we currently use
    uint64_t m_total_cnt;
    uint64_t m_total_cnt_high;
    dsec_t   m_max_dt;  // Total maximum latency in seconds
    dsec_t   m_min_dt;  // Total min latency in seconds
    dsec_t   m_average; // Sliding average with low pass filter in seconds
    uint32_t m_win_cnt;
    uint32_t m_hot_max;
    bool m_hot_max_done; // Flag that represents that the hot period in which Max Latency values should be ignored is done.
    dsec_t   m_max_ar[HISTOGRAM_QUEUE_SIZE]; // Array of maximum latencies for previous periods
    uint64_t m_hcnt[HISTOGRAM_SIZE_LOG][HISTOGRAM_SIZE];
    hdr_histogram *m_hdrh; // Hdr histogram instance

};


std::ostream& operator<<(std::ostream& os, const CTimeHistogram& in);

#endif
