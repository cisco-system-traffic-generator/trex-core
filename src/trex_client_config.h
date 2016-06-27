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
 * single client config
 * 
 * @author imarom (27-Jun-16)
 */
class ClientCfg {
public:
    uint8_t   m_init_mac[6];
    uint16_t  m_init_vlan;

    uint8_t   m_res_mac[6];
    uint16_t  m_res_vlan;

};

/**
 * describes a single client config 
 * entry loaded from the config file
 * 
 */
class ClientCfgEntry {
public:

    ClientCfgEntry() {
        reset();
    }


    void dump() const;

    bool contains(uint32_t ip) const {
        return ( (ip >= m_ip_start) && (ip <= m_ip_end) );
    }

    void reset() {
        m_iterator = 0;
    }

    /**
     * assings a client config from the group 
     * it will advance MAC addresses andf etc. 
     *  
     * @author imarom (27-Jun-16)
     * 
     * @param info 
     */
    void assign(ClientCfg &info) {

        for (int i = 0; i < 6; i++) {
            info.m_init_mac[i]  = ( (m_init_mac + m_iterator) >> ((5 - i) * 8) ) & 0xFF;
            info.m_res_mac[i]   = ( (m_res_mac  + m_iterator) >> ((5 - i) * 8) ) & 0xFF;
        }

        info.m_init_vlan = m_init_vlan;
        info.m_res_vlan  = m_res_vlan;

        /* advance */
        m_iterator = (m_iterator + 1) % m_count;
    }

public:
    uint32_t  m_ip_start;
    uint32_t  m_ip_end;

    uint64_t  m_init_mac;
    uint16_t  m_init_vlan;

    uint64_t  m_res_mac;
    uint16_t  m_res_vlan;

    uint32_t  m_count;

private:
    uint32_t m_iterator;
};

/**
 * describes the DB of every client group
 * 
 */
class ClientCfgDB {
public:

    ClientCfgDB() {
        m_is_empty = true;
        m_cache_group = NULL;
    }

    /**
     * if no config file was loaded 
     * this should return true 
     * 
     */
    bool is_empty() {
        return m_is_empty;
    }

    /**
     * loads a YAML file 
     * configuration will be built 
     * according to the YAML config 
     * 
     */
    void load_yaml_file(const std::string &filename);

    /**
     * lookup for a specific IP address for 
     * a group that contains this IP 
     * 
     */
    ClientCfgEntry * lookup(uint32_t ip);
    ClientCfgEntry * lookup(const std::string &ip);

private:

    void yaml_parse_err(const std::string &err, const YAML::Mark *mark = NULL) const;

    /**
     * verify the YAML file loaded in valid
     * 
     */
    void verify() const;

    /* maps the IP start value to client groups */
    std::map<uint32_t, ClientCfgEntry>  m_groups;
    ClientCfgEntry                     *m_cache_group;
    std::string                         m_filename;
    bool                                m_is_empty;
};

#endif /* __TREX_CLIENT_CONFIG_H__ */
