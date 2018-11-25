/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include <istream>
#include <fstream>
#include "common/basic_utils.h"
#include <common/Network/Packet/CPktCmn.h>
#include "utl_yaml.h"
#include "inet_pton.h"


bool utl_yaml_read_ip_addr(const YAML::Node& node,
                           const std::string &name,
                           uint32_t & val){
    std::string tmp;
    uint32_t ip;
    bool res=false;
    if ( node.FindValue(name) ) {
        node[name] >> tmp ;
        if ( my_inet_pton4((char *)tmp.c_str(), (unsigned char *)&ip) ){
            val=PKT_NTOHL(ip);
            res=true;
        }else{
            printf(" Error: non valid ip %s \n",(char *)tmp.c_str());
            exit(1);
        }
    }
    return (res);
}

bool utl_yaml_read_uint32(const YAML::Node& node,
                          const std::string &name,
                          uint32_t & val){
    bool res=false;
    if ( node.FindValue(name) ) {
        node[name] >> val ;
        res=true;
    }
    return (res);
}

bool utl_yaml_read_uint16(const YAML::Node& node,
                          const std::string &name,
                          uint16_t & val, uint16_t min, uint16_t max) {
    bool res = utl_yaml_read_uint16(node, name, val);

    if (res) {
        if ((val < min) || (val > max)) {
            fprintf(stderr
                    , "Parsing error: value of field '%s' must be between %d and %d\n"
                    , name.c_str(), min, max);
            exit(1);
        }
    }

    return res;
}

bool utl_yaml_read_uint16(const YAML::Node& node,
                          const std::string &name,
                          uint16_t & val){
    uint32_t val_tmp;
    bool res=false;
    if ( node.FindValue(name) ) {
        node[name] >> val_tmp ;
        val = (uint16_t)val_tmp;
        res=true;
    }

    return (res);
}


/************************
 * YAML Parser Wrapper
 *
 ***********************/
void
YAMLParserWrapper::load(YAML::Node &root) {
    std::stringstream ss;

    /* first check file exists */
    if (!utl_is_file_exists(m_filename)){
        ss << "file '" << m_filename << "' does not exists";
        throw std::runtime_error(ss.str());
    }

    std::ifstream fin(m_filename);

    try {
        YAML::Parser base_parser(fin);
        base_parser.GetNextDocument(root);

    } catch (const YAML::Exception &e) {
        parse_err(e.what());
    }
}

bool
YAMLParserWrapper::parse_bool(const YAML::Node &node, const std::string &name, bool def) {
    if (!node.FindValue(name)) {
        return def;
    }

    return parse_bool(node, name);
}

bool
YAMLParserWrapper::parse_bool(const YAML::Node &node, const std::string &name) {
    try {
        bool val;
        node[name] >> val;
        return (val);

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

const YAML::Node &
YAMLParserWrapper::parse_list(const YAML::Node &node, const std::string &name) {

    try {
        const YAML::Node &val = node[name];
        if (val.Type() != YAML::NodeType::Sequence) {
            throw YAML::InvalidScalar(node[name].GetMark());
        }
        return val;

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting sequence or list for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

const YAML::Node &
YAMLParserWrapper::parse_map(const YAML::Node &node, const std::string &name) {

    try {
        const YAML::Node &val = node[name];
        if (val.Type() != YAML::NodeType::Map) {
            throw YAML::InvalidScalar(node[name].GetMark());
        }
        return val;

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting map for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

uint32_t
YAMLParserWrapper::parse_ip(const YAML::Node &node, const std::string &name) {
    try {
        std::string ip_str;
        uint32_t    ip_num;

        node[name] >> ip_str;
        int rc = my_inet_pton4((char *)ip_str.c_str(), (unsigned char *)&ip_num);
        if (!rc) {
            parse_err("Invalid IP address: " + ip_str, node[name]);
        }

        return PKT_NTOHL(ip_num);

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting valid IP address for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

void
YAMLParserWrapper::parse_ipv6(const YAML::Node &node, const std::string &name, unsigned char *ip_num) {
    try {
        std::string ip_str;

        node[name] >> ip_str;
        int rc = my_inet_pton6((char *)ip_str.c_str(), ip_num);
        if (!rc) {
            parse_err("Invalid IPv6 address: " + ip_str, node[name]);
        }

        // we want host order
        for (int i = 0; i < 8; i++) {
            ((uint16_t *) ip_num)[i] = PKT_NTOHS(((uint16_t *) ip_num)[i]);
        }
        return;

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting valid IPv6 address for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

uint64_t
YAMLParserWrapper::parse_mac_addr(const YAML::Node &node, const std::string &name, uint64_t def) {
    if (!node.FindValue(name)) {
        return def;
    }

    return parse_mac_addr(node, name);
}

uint64_t
YAMLParserWrapper::parse_mac_addr(const YAML::Node &node, const std::string &name) {

    std::string mac_str;
    uint64_t    mac_num;

    try {

        node[name] >> mac_str;
        bool rc = mac2uint64(mac_str, mac_num);
        if (!rc) {
            parse_err("Invalid MAC address: " + mac_str, node[name]);
        }
        return mac_num;

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
    return(0);
}

uint64_t
YAMLParserWrapper::parse_uint(const YAML::Node &node, const std::string &name, uint64_t low, uint64_t high, uint64_t def) {
    if (!node.FindValue(name)) {
        return def;
    }

    return parse_uint(node, name, low, high);
}

uint64_t
YAMLParserWrapper::parse_uint(const YAML::Node &node, const std::string &name, uint64_t low, uint64_t high) {

    try {

        uint64_t val;
        node[name] >> val;

        if ( (val < low) || (val > high) ) {
            std::stringstream ss;
            ss << "valid range for field '" << name << "' is: [" << low << " - " << high << "]";
            parse_err(ss.str(), node[name]);
        }

        return (val);

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("Expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("Can not locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

void
YAMLParserWrapper::parse_err(const std::string &err, const YAML::Node &node) const {
    std::stringstream ss;

    ss << "'" << m_filename << "' - YAML parsing error at line " << node.GetMark().line << ": ";
    ss << err;

    throw std::runtime_error(ss.str());
}

void
YAMLParserWrapper::parse_err(const std::string &err) const {
    std::stringstream ss;

    ss << "'" << m_filename << "' - YAML parsing error: " << err;

    throw std::runtime_error(ss.str());
}
