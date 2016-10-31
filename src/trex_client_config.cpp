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

    //m_cfg.dump();

    #if 0
    std::cout << "Init. MAC addr: ";
    for (int i = 0; i < 6; i++) {
        printf("%lx:", ( (m_initiator.m_dst_mac >> ( (6-i) * 8)) & 0xFF ) );
    }
    std::cout << "\n";

    std::cout << "Init. VLAN:     " << m_initiator.m_vlan << "\n";

    std::cout << "Res. MAC addr:  ";
    for (int i = 0; i < 6; i++) {
        printf("%lx:", ( (m_responder.m_dst_mac >> ( (6-i) * 8)) & 0xFF ) );
    }
    std::cout << "\n";

    std::cout << "Res. VLAN:      " << m_responder.m_vlan << "\n";
    #endif
}


/**
 * loads a YAML file containing 
 * the client groups configuration 
 * 
 */
void
ClientCfgDB::load_yaml_file(const std::string &filename) {
    std::stringstream ss;

    m_groups.clear();
    m_cache_group = NULL;

    /* wrapper parser */
    YAML::Node root;
    YAMLParserWrapper parser(filename);
    parser.load(root);

    /* parse globals */
    m_under_vlan = parser.parse_bool(root, "vlan");

    const YAML::Node &groups = parser.parse_list(root, "groups");

    /* parse each group */
    for (int i = 0; i < groups.size(); i++) {
        parse_single_group(parser, groups[i]);
    }

    verify(parser);

    m_is_empty = false;

}

/**
 * reads a single group of clients from YAML
 * 
 */
void
ClientCfgDB::parse_single_group(YAMLParserWrapper &parser, const YAML::Node &node) {
    ClientCfgEntry group;

    /* ip_start */
    group.m_ip_start = parser.parse_ip(node, "ip_start");

    /* ip_end */
    group.m_ip_end = parser.parse_ip(node, "ip_end");

    /* sanity check */
    if (group.m_ip_end < group.m_ip_start) {
        parser.parse_err("ip_end must be >= ip_start", node);
    }

    const YAML::Node &init = parser.parse_map(node, "initiator");
    const YAML::Node &resp = parser.parse_map(node, "responder");

    parse_dir(parser, init, group.m_cfg.m_initiator);
    parse_dir(parser, resp, group.m_cfg.m_responder);

  
    group.m_count = parser.parse_uint(node, "count", 0, UINT64_MAX, 1);

    /* add to map with copying */
    m_groups[group.m_ip_start] = group;

}

void 
ClientCfgDB::parse_dir(YAMLParserWrapper &parser, const YAML::Node &node, ClientCfgDir &dir) {
    if (node.FindValue("src_mac")) {
        dir.set_src_mac_addr(parser.parse_mac_addr(node, "src_mac"));
    }
    
    if (node.FindValue("dst_mac")) {
        dir.set_dst_mac_addr(parser.parse_mac_addr(node, "dst_mac"));
    }

    if (m_under_vlan) {
        dir.set_vlan(parser.parse_uint(node, "vlan", 0, 0xfff));
    } else {
        if (node.FindValue("vlan")) {
            parser.parse_err("VLAN config was disabled", node["vlan"]);
        }
    }
}

/**
 * sanity checks
 * 
 * @author imarom (28-Jun-16)
 */
void
ClientCfgDB::verify(const YAMLParserWrapper &parser) const {
    std::stringstream ss;
    uint32_t monotonic = 0;

    /* check that no interval overlaps */
    
    /* all intervals do not overloap iff when sorted each start/end dots are strong monotonic */
    for (const auto &p : m_groups) {
        const ClientCfgEntry &group = p.second;

        if ( (monotonic > 0 ) && (group.m_ip_start <= monotonic) ) {
            ss << "IP '" << ip_to_str(group.m_ip_start) << "' - '" << ip_to_str(group.m_ip_end) << "' overlaps with other groups";
            parser.parse_err(ss.str());
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
    
    /* a cache to avoid constant search (usually its a range of IPs) */
    if ( (m_cache_group) && (m_cache_group->contains(ip)) ) {
        return m_cache_group;
    }

    /* clear the cache pointer */
    m_cache_group = NULL;

    std::map<uint32_t ,ClientCfgEntry>::iterator it;
    
    /* upper bound fetchs the first greater element */
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

/**
 * for convenience - search by IP as string
 * 
 * @author imarom (28-Jun-16)
 * 
 * @param ip 
 * 
 * @return ClientCfgEntry* 
 */
ClientCfgEntry *
ClientCfgDB::lookup(const std::string &ip) {
    uint32_t addr = (uint32_t)inet_addr(ip.c_str());
    addr = PKT_NTOHL(addr);

    return lookup(addr);
}


