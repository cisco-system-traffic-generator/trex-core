#ifndef UTL_POLICER_H
#define UTL_POLICER_H
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

#include <stdint.h>
#include "os_time.h"
#include <common/c_common.h>


class CPolicer {

public:

    CPolicer(){
        ClearMeter();
    }

    void ClearMeter(){
        m_cir=0.0;
        m_bucket_size=1.0;
        m_level=0.0;
        m_last_time=0.0;
    }

    bool update(double dsize,dsec_t now_sec);

    void set_cir(double cir){
        BP_ASSERT(cir>=0.0);
        m_cir=cir;
    }
    void set_level(double level){
        m_level =level;
    }

    void set_bucket_size(double bucket){
        m_bucket_size =bucket;
    }

private:

    double                      m_cir;

    double                      m_bucket_size;

    double                      m_level;

    double                      m_last_time;
};



#endif
