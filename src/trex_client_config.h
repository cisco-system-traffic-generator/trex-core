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
 * client configuration per direction
 * 
 * @author imarom (29-Jun-16)
 */
class ClientCfgDir {

private:
    enum {
        HAS_SRC_MAC = 0x1,
        HAS_DST_MAC = 0x2,
        HAS_VLAN    = 0x4,
    };

    uint8_t     m_src_mac[6];
    uint8_t     m_dst_mac[6];
    uint16_t    m_vlan;
    uint8_t     m_bitfield;


public:
    ClientCfgDir() {
        m_bitfield = 0;
    }

    bool has_src_mac_addr() const {
        return (m_bitfield & HAS_SRC_MAC);
    }

    bool has_dst_mac_addr() const {
        return (m_bitfield & HAS_DST_MAC);
    }
    bool has_vlan() const {
        return (m_bitfield & HAS_VLAN);
    }

    void set_src_mac_addr(uint64_t mac_addr) {
        for (int i = 0; i < 6; i++) {
            m_src_mac[i] = ( mac_addr >> ((5 - i) * 8) ) & 0xFF;
        }
        m_bitfield |= HAS_SRC_MAC;
    }

    void set_dst_mac_addr(uint64_t mac_addr) {
        for (int i = 0; i < 6; i++) {
            m_dst_mac[i] = ( mac_addr >> ((5 - i) * 8) ) & 0xFF;
        }
        m_bitfield |= HAS_DST_MAC;
    }

    void set_vlan(uint16_t vlan_id) {
        m_vlan      = vlan_id;
        m_bitfield |= HAS_VLAN;
    }

    /* updates a configuration with a group index member */

    void update(uint32_t index) {
        if (has_src_mac_addr()) {
            mac_add(m_src_mac, index);
        }

        if (has_dst_mac_addr()) {
            mac_add(m_dst_mac, index);
        }
    }

    const uint8_t *get_src_mac_addr() {
        assert(has_src_mac_addr());
        return m_src_mac;
    }

    const uint8_t *get_dst_mac_addr() {
        assert(has_dst_mac_addr());
        return m_dst_mac;
    }

    uint16_t get_vlan() {
        assert(has_vlan());
        return m_vlan;
    }

private:
    /**
     * transform MAC address to uint64_t 
     * performs add and return to MAC format 
     * 
     */
    void mac_add(uint8_t *mac, uint32_t i) {
        uint64_t tmp = 0;

        for (int i = 0; i < 6; i++) {
            tmp <<= 8;
            tmp |= mac[i];
        }

        tmp += i;

        for (int i = 0; i < 6; i++) {
            mac[i] = ( tmp >> ((5 - i) * 8) ) & 0xFF;
        }

    }
};

/**
 * single client config
 * 
 */
class ClientCfg {

public:

    void update(uint32_t index) {
        m_initiator.update(index);
        m_responder.update(index);
    }

    ClientCfgDir m_initiator;
    ClientCfgDir m_responder;
};

/******************************** internal section ********************************/

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
        info = m_cfg;
        info.update(m_iterator);
        
        /* advance for the next assign */
        m_iterator = (m_iterator + 1) % m_count;
    }

public:
    uint32_t    m_ip_start;
    uint32_t    m_ip_end;

    ClientCfg   m_cfg;

    uint32_t    m_count;

private:
    uint32_t    m_iterator;
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
    void parse_dir(YAMLParserWrapper &parser, const YAML::Node &node, ClientCfgDir &dir);

    /**
     * verify the YAML file loaded in valid
     * 
     */
    void verify(const YAMLParserWrapper &parser) const;

    /* maps the IP start value to client groups */
    std::map<uint32_t, ClientCfgEntry>  m_groups;
    bool                                m_under_vlan;

    ClientCfgEntry                     *m_cache_group;
    std::string                         m_filename;
    bool                                m_is_empty;
};

#endif /* __TREX_CLIENT_CONFIG_H__ */

