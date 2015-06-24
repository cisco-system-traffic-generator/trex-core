#ifndef C_TIME_HISTOGRAM_H
#define C_TIME_HISTOGRAM_H
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
#include <stdio.h>
#include <math.h>
#include "mbuf.h"
#include <string>


class CTimeHistogram {
public:
    enum {
        HISTOGRAM_SIZE=9,
        HISTOGRAM_SIZE_LOG=5,
        HISTOGRAM_QUEUE_SIZE=14,
    };
    bool Create(void);
    void Delete();
    void Reset();
    bool Add(dsec_t dt);
    void Dump(FILE *fd);
    void DumpWinMax(FILE *fd);
    /* should be called each 1 sec */
    void update();
    dsec_t  get_average_latency();
    /* get average of total data */
    dsec_t get_total_average();


    dsec_t  get_max_latency(){
        return (get_usec(m_max_dt));
    }

    dsec_t  get_max_latency_last_update(){
        return ( get_usec(m_max_win_last_dt) );
    }

    void  dump_json(std::string name,std::string & json );


private:
    uint32_t get_usec(dsec_t d);
    double  get_cur_average();
    void  update_average();

    double get_base_usec(int j,int i){
        double base=pow(10.0,(double)j+1.0);
        return ( base * ((double)i+1.0) );
    }

public:
    dsec_t   m_min_delta;/* set to 10usec*/
    uint64_t m_cnt;
    uint64_t m_high_cnt;
    dsec_t   m_max_dt;
    dsec_t   m_max_win_dt;
    dsec_t   m_max_win_last_dt;
    dsec_t   m_average; /* moving average */

    uint32_t m_win_cnt;
    dsec_t   m_max_ar[HISTOGRAM_QUEUE_SIZE];

    uint64_t m_hcnt[HISTOGRAM_SIZE_LOG][HISTOGRAM_SIZE] __rte_cache_aligned ;
    uint64_t m_hcnt_shadow[HISTOGRAM_SIZE_LOG][HISTOGRAM_SIZE] __rte_cache_aligned ; // this this contorl side 
    uint64_t m_cnt_shadow;
    uint64_t m_high_cnt_shadow;
};

#endif
