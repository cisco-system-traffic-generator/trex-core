#ifndef CTW_CFG_H
#define CTW_CFG_H

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

#include <yaml-cpp/yaml.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>



struct CTimerWheelYamlInfo {
public:
    CTimerWheelYamlInfo(){
        reset();
    }


    void reset(){
        m_info_exist =false;
        m_buckets=1024;
        m_levels=3;
        m_bucket_time_usec=20.0;
    }

    bool            m_info_exist; /* file exist ?*/
    uint32_t        m_buckets;
    uint32_t        m_levels;
    double          m_bucket_time_usec;


public:
    void Dump(FILE *fd);
    bool Verify(std::string & err);

};

void operator >> (const YAML::Node& node, CTimerWheelYamlInfo & mac_info);




#endif
