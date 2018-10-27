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

#include "tw_cfg.h"


bool CTimerWheelYamlInfo::Verify(std::string & err){
       bool res=true;
        if ( (m_levels>4) || (m_levels <1) ){
            err="tw number of levels should be betwean 1..4";
            res=false;
        }
        if ( (m_buckets >=(0xffff)) || ((m_buckets <256)) ){
            err="tw number of bucket should be betwean 256-2^16 and log2";
            res=false;
        }

        if ( (m_bucket_time_usec <10.0) || (((m_bucket_time_usec> 200.0))) ){
            res=false;
            err="bucket time should be betwean 10-200 usec ";
        }
        return res;
}

void CTimerWheelYamlInfo::Dump(FILE *fd){

    if ( m_info_exist ==false ){
        fprintf(fd,"CTimerWheelYamlInfo does not exist  \n");
        return;
    }

    fprintf(fd," tw buckets     :  %lu \n",(ulong)m_buckets);
    fprintf(fd," tw levels      :  %lu \n",(ulong)m_levels);
    fprintf(fd," tw bucket time :  %.02f usec\n",(double)m_bucket_time_usec);

}

void operator >> (const YAML::Node& node, CTimerWheelYamlInfo & info) {

    uint32_t val;

    if ( node.FindValue("buckets") ){
        node["buckets"] >> val;
        info.m_buckets=(uint16_t)val;
        info.m_info_exist = true;
    }

    if ( node.FindValue("levels") ){
        node["levels"] >> val;
        info.m_levels = (uint16_t)val;
        info.m_info_exist = true;
    }

    if ( node.FindValue("bucket_time_usec") ){
        node["bucket_time_usec"] >>info.m_bucket_time_usec;
        info.m_info_exist = true;
    }
}
