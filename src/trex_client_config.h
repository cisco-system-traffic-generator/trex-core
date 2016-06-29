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

class YAMLParserWrapper;

/**
 * single client config
 * 
 */
class ClientCfg {
public:
    struct dir_st {
        uint8_t   m_dst_mac[6];
        uint16_t  m_vlan;
    };
    
    dir_st m_initiator;
    dir_st m_responder;
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

        /* assigns MAC addrs as big endian */
        for (int i = 0; i < 6; i++) {
            info.m_initiator.m_dst_mac[i] = ( (m_initiator.m_dst_mac + m_iterator) >> ((5 - i) * 8) ) & 0xFF;
            info.m_responder.m_dst_mac[i] = ( (m_responder.m_dst_mac + m_iterator) >> ((5 - i) * 8) ) & 0xFF;
        }

        info.m_initiator.m_vlan = m_initiator.m_vlan;
        info.m_responder.m_vlan = m_responder.m_vlan;

        /* advance for the next assign */
        m_iterator = (m_iterator + 1) % m_count;
    }

public:
    uint32_t  m_ip_start;
    uint32_t  m_ip_end;

    struct cfg_dir_st {
        //uint64_t m_src_mac;
        uint64_t m_dst_mac;
        uint16_t m_vlan;
    };

    cfg_dir_st  m_initiator;
    cfg_dir_st  m_responder;

    uint32_t    m_count;

private:
    uint32_t m_iterator;
};

/**
 * holds all the configured clients groups
 * 
 */
class ClientCfgDB {
public:

    ClientCfgDB() {
        m_is_empty    = true;
        m_cache_group = NULL;
        m_under_vlan  = false;
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
    void parse_single_group(YAMLParserWrapper &parser, const YAML::Node &node);
    void yaml_parse_err(const std::string &err, const YAML::Mark *mark = NULL) const;

    /**
     * verify the YAML file loaded in valid
     * 
     */
    void verify() const;

    /* maps the IP start value to client groups */
    std::map<uint32_t, ClientCfgEntry>  m_groups;
    bool                                m_under_vlan;

    ClientCfgEntry                     *m_cache_group;
    std::string                         m_filename;
    bool                                m_is_empty;
};

#endif /* __TREX_CLIENT_CONFIG_H__ */
