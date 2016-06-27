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

#include <stdexcept>
#include <sstream>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include "utl_yaml.h"
#include "trex_client_config.h"
#include "common/basic_utils.h"
#include "bp_sim.h"

void
ClientCfgEntry::dump() const {

    std::cout << "IP start:       " << ip_to_str(m_ip_start) << "\n";
    std::cout << "IP end:         " << ip_to_str(m_ip_end) << "\n";

    std::cout << "Init. MAC addr: ";
    for (int i = 0; i < 6; i++) {
        printf("%lx:", ( (m_init_mac >> ( (6-i) * 8)) & 0xFF ) );
    }
    std::cout << "\n";

    std::cout << "Init. VLAN:     " << m_init_vlan << "\n";

    std::cout << "Res. MAC addr:  ";
    for (int i = 0; i < 6; i++) {
        printf("%lx:", ( (m_res_mac >> ( (6-i) * 8)) & 0xFF ) );
    }
    std::cout << "\n";

    std::cout << "Res. VLAN:      " << m_res_vlan << "\n";
}


/**
 * loads a YAML file containing 
 * the client groups configuration 
 * 
 */
void
ClientCfgDB::load_yaml_file(const std::string &filename) {
    std::stringstream ss;

    m_filename = filename;

    if (!utl_is_file_exists(filename)){
        ss << "file does not exists";
        throw std::runtime_error(ss.str());
    }

    std::ifstream fin(filename);
    YAML::Parser parser(fin);
    YAML::Node root;

    parser.GetNextDocument(root);

    for (int i = 0; i < root.size(); i++) {
        const YAML::Mark &mark = root[i].GetMark();
        ClientCfgEntry group;
        bool rc;

        /* ip_start */
        rc = utl_yaml_read_ip_addr(root[i], "ip_start", group.m_ip_start);
        if (!rc) {
            yaml_parse_err("'ip_start' field does not exists", &mark);
        }

        /* ip_end */
        rc = utl_yaml_read_ip_addr(root[i], "ip_end",   group.m_ip_end);
        if (!rc) {
            yaml_parse_err("'ip_end' field does not exists", &mark);
        }

        /* sanity check */
        if (group.m_ip_end <= group.m_ip_start) {
            yaml_parse_err("IP group range must be positive and include at least one entry", &mark);
        }

        /* init_mac */
        rc = utl_yaml_read_mac_addr(root[i], "init_mac", group.m_init_mac);
        if (!rc) {
            yaml_parse_err("'init_mac' field does not exists", &mark);
        }

        /* res_mac */
        rc = utl_yaml_read_mac_addr(root[i], "res_mac", group.m_res_mac);
        if (!rc) {
            yaml_parse_err("'res_mac' field does not exists", &mark);
        }


        root[i]["init_vlan"] >> group.m_init_vlan;
        root[i]["res_vlan"]  >> group.m_res_vlan;
        root[i]["count"]     >> group.m_count;


        /* add to map with copying */
        m_groups[group.m_ip_start] = group;
    }

    verify();

    m_is_empty = false;

}

void
ClientCfgDB::verify() const {
    std::stringstream ss;
    uint32_t monotonic = 0;

    /* check that no interval overlaps */
    
    for (const auto &p : m_groups) {
        const ClientCfgEntry &group = p.second;

        if ( (monotonic > 0 ) && (group.m_ip_start <= monotonic) ) {
            ss << "IP '" << ip_to_str(group.m_ip_start) << "' - '" << ip_to_str(group.m_ip_end) << "' overlaps with other groups";
            yaml_parse_err(ss.str());
        }

        monotonic = group.m_ip_end;
    }
}

/**
 * lookup function 
 * should be fast 
 * 
 */
ClientCfgEntry *
ClientCfgDB::lookup(uint32_t ip) {
    
    /* check the cache */
    if ( (m_cache_group) && (m_cache_group->contains(ip)) ) {
        return m_cache_group;
    }

    std::map<uint32_t ,ClientCfgEntry>::iterator it;
    
    it = m_groups.upper_bound(ip);

    /* if the first element in the map is bigger - its not in the map */
    if (it == m_groups.begin()) {
        return NULL;
    }

    /* go one back - we know it's not on begin so we have at least one back */
    it--;

    ClientCfgEntry &group = (*it).second;

    /* because this is a non overlapping intervals
       if IP exists it must be in this group
       because if it exists in some other previous group
       its end_range >= ip so start_range > ip
       and so start_range is the upper_bound
     */
    if (group.contains(ip)) {
        /* save as cache */
        m_cache_group = &group;
        return &group;
    } else {
        return NULL;
    }

}

ClientCfgEntry *
ClientCfgDB::lookup(const std::string &ip) {
    uint32_t addr = (uint32_t)inet_addr(ip.c_str());
    addr = PKT_NTOHL(addr);

    return lookup(addr);
}


void
ClientCfgDB::yaml_parse_err(const std::string &err, const YAML::Mark *mark) const {
    std::stringstream ss;

    if (mark) {
        ss << "\n*** '" << m_filename << "' - YAML parsing error at line " << mark->line << ": ";
    } else {
        ss << "\n*** '" << m_filename << "' - YAML parsing error: ";
    }

    ss << err;
    throw std::runtime_error(ss.str());
}
