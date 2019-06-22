#ifndef UTL_YAML_
#define UTL_YAML_
/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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
#include <vector>
#include <yaml-cpp/yaml.h>


/* static methods - please prefer the wrapper over those */
bool utl_yaml_read_ip_addr(const YAML::Node& node,
                           const std::string &name,
                           uint32_t & val);

bool utl_yaml_read_uint32(const YAML::Node& node,
                          const std::string &name,
                          uint32_t & val);

bool utl_yaml_read_uint16(const YAML::Node& node,
                          const std::string &name,
                          uint16_t & val);

bool utl_yaml_read_uint16(const YAML::Node& node,
                          const std::string &name,
                          uint16_t & val, uint16_t min, uint16_t max);

bool mac2vect(const std::string &mac_str, std::vector<uint8_t> &mac);

/* a thin wrapper to customize errors */
class YAMLParserWrapper {
public:
    /* a header that will start every error message */
    YAMLParserWrapper(const std::string &filename) : m_filename(filename) {
    }

    /**
     * loads the file (while parsing it)
     *
     */
    void load(YAML::Node &root);

    /* bool */
    bool parse_bool(const YAML::Node &node, const std::string &name);
    bool parse_bool(const YAML::Node &node, const std::string &name, bool def);

    const YAML::Node & parse_list(const YAML::Node &node, const std::string &name);
    const YAML::Node & parse_map(const YAML::Node &node, const std::string &name);

    uint32_t parse_ip(const YAML::Node &node, const std::string &name);
    void parse_ipv6(const YAML::Node &node, const std::string &name, unsigned char *ip);

    uint64_t parse_mac_addr(const YAML::Node &node, const std::string &name);
    uint64_t parse_mac_addr(const YAML::Node &node, const std::string &name, uint64_t def);

    uint64_t parse_uint(const YAML::Node &node, const std::string &name, uint64_t low = 0, uint64_t high = UINT64_MAX);
    uint64_t parse_uint(const YAML::Node &node, const std::string &name, uint64_t low, uint64_t high, uint64_t def);


public:
    void parse_err(const std::string &err, const YAML::Node &node) const;
    void parse_err(const std::string &err) const;


private:
    std::string m_filename;
};
#endif
