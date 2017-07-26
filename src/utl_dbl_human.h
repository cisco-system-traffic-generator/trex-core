#ifndef UTL_DBL_HUMAN_H
#define UTL_DBL_HUMAN_H
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


#include <string>

#define _1MB_DOUBLE ((double)(1024.0*1024.0))
#define _1GB_DOUBLE ((double)(1024.0*1024.0*1024.0))

#define _1Mb_DOUBLE ((double)(1000.0*1000.0))

#define _1MB ((1024*1024)ULL)
#define _1GB 1000000000ULL
#define _500GB (_1GB*500)

            
typedef enum {
    KBYE_1024,
    KBYE_1000
} human_kbyte_t;

std::string double_to_human_str(double num,
                                std::string units,
                                human_kbyte_t etype);


class CPPSMeasure {
public:
    CPPSMeasure(){
        reset();
    }
    //reset
    void reset(void){
        m_start=false;
        m_last_time_msec=0;
        m_last_pkts=0;
        m_last_result=0.0;
    }
    //add packet size
    float add(uint64_t pkts);

private:
    float calc_pps(uint32_t dtime_msec,
                   uint32_t pkts);

public:
   bool      m_start;
   uint32_t  m_last_time_msec;
   uint64_t  m_last_pkts;
   float     m_last_result;
};



class CBwMeasure {
public:
    CBwMeasure();
    //reset
    void reset(void);
    //add packet size
    double add(uint64_t size);

private:
    double calc_MBsec(uint32_t dtime_msec,
                     uint64_t dbytes);

public:
   bool      m_start;
   uint32_t  m_last_time_msec;
   uint64_t  m_last_bytes;
   double     m_last_result;
};



#endif
