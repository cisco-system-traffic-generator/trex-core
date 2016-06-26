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
ClientGroup::dump() {
    char ip_str[100];

    ip_to_str(m_ip_start, ip_str);
    std::cout << "IP start:       " << ip_str << "\n";

    ip_to_str(m_ip_end, ip_str);
    std::cout << "IP end:         " << ip_str << "\n";

    std::cout << "Init. MAC addr: ";
    for (int i = 0; i < 6; i++) {
        printf("%x:", m_init_mac[i]);
    }
    std::cout << "\n";

    std::cout << "Init. VLAN:     " << m_init_vlan << "\n";

    std::cout << "Res. MAC addr:  ";
    for (int i = 0; i < 6; i++) {
        printf("%x:", m_res_mac[i]);
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
ClientGroupsDB::load_yaml_file(const std::string &filename) {
    std::stringstream ss;

    ss << "'" << filename << "' YAML parsing error: ";

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
        ClientGroup group;
        bool rc;

        /* ip_start */
        rc = utl_yaml_read_ip_addr(root[i], "ip_start", group.m_ip_start);
        if (!rc) {
            ss << "line " << mark.line << " - 'ip_start' field does not exists";
            throw std::runtime_error(ss.str());
        }

        /* ip_end */
        rc = utl_yaml_read_ip_addr(root[i], "ip_end",   group.m_ip_end);
        if (!rc) {
            ss << "line " << mark.line << " - 'ip_end' field does not exists";
            throw std::runtime_error(ss.str());
        }

        /* init_mac */
        rc = utl_yaml_read_mac_addr(root[i], "init_mac", group.m_init_mac);
        if (!rc) {
            ss << "line " << mark.line << " - 'init_mac' field does not exists";
            throw std::runtime_error(ss.str());
        }

        /* res_mac */
        rc = utl_yaml_read_mac_addr(root[i], "res_mac", group.m_res_mac);
        if (!rc) {
            ss << "line " << mark.line << " - 'init_mac' field does not exists";
            throw std::runtime_error(ss.str());
        }


        root[i]["init_vlan"] >> group.m_init_vlan;
        root[i]["res_vlan"]  >> group.m_res_vlan;
        root[i]["count"]     >> group.m_count;

        printf("\n");
        group.dump();
        printf("\n");
        //std::cout << root[i]["ip_start"] << "\n";
    }
}
