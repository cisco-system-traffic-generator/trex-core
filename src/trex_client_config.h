/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2016 Cisco Systems, Inc.

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
#ifndef __TREX_CLIENT_CONFIG_H__
#define __TREX_CLIENT_CONFIG_H__

#include <stdint.h>
#include <string>
#include <map>

/**
 * describes a single client group
 * configuration
 */
class ClientGroup {
public:

    uint32_t  m_ip_start;
    uint32_t  m_ip_end;

    uint8_t   m_init_mac[6];
    uint16_t  m_init_vlan;

    uint8_t   m_res_mac[6];
    uint16_t  m_res_vlan;

    uint32_t  m_count;


    void dump();
};

/**
 * describes the DB of every client group
 * 
 */
class ClientGroupsDB {
public:
    /**
     * loads a YAML file 
     * configuration will be built 
     * according to the YAML config 
     * 
     */
    void load_yaml_file(const std::string &filename);


private:
    /* maps the IP start value to client groups */
    std::map<uint32_t, ClientGroup> m_groups;
};

#endif /* __TREX_CLIENT_CONFIG_H__ */
