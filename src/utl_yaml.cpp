#include "utl_yaml.h"
#include <common/Network/Packet/CPktCmn.h>
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

#include <istream>

#define INADDRSZ 4

static int my_inet_pton4(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL) {
			unsigned int _new = *tp * 10 + (pch - digits);

			if (_new > 255)
				return (0);
			if (! saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
			*tp = (unsigned char)_new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);

	memcpy(dst, tmp, INADDRSZ);
	return (1);
}


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
            printf(" ERROR  not a valid ip %s \n",(char *)tmp.c_str());
            exit(-1);
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

static void
split_str_by_delimiter(std::string str, char delim, std::vector<std::string> &tokens) {
    size_t pos = 0;
    std::string token;

    while ((pos = str.find(delim)) != std::string::npos) {
        token = str.substr(0, pos);
        tokens.push_back(token);
        str.erase(0, pos + 1);
    }

    if (str.size() > 0) {
        tokens.push_back(str);
    }
}

static bool mac2uint64(const std::string &mac_str, uint64_t &mac_num) {
    std::vector<std::string> tokens;
    uint64_t val;

    split_str_by_delimiter(mac_str, ':', tokens);
    if (tokens.size() != 6) {
        return false;
    }

    val = 0;

    for (int i = 0; i < 6 ; i++) {
        char *endptr = NULL;
        unsigned long octet = strtoul(tokens[i].c_str(), &endptr, 16);

        if ( (*endptr != 0) || (octet > 0xff) ) {
            return false;
        }

        val = (val << 8) + octet;
    }

    mac_num = val;

    return true;
}

/************************
 * YAML Parser Wrapper
 *
 ***********************/
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
        parse_err("expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
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
        parse_err("expecting sequence/list for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
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
        parse_err("expecting map for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
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
            parse_err("invalid IP address: " + ip_str, node[name]);
        }

        return PKT_NTOHL(ip_num);

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("expecting valid IP address for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
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
            parse_err("invalid MAC address: " + mac_str, node[name]);
        }
        return mac_num;

    } catch (const YAML::InvalidScalar &ex) {
        parse_err("expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
    }

    assert(0);
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
        parse_err("expecting true/false for field '" + name + "'", node[name]);

    } catch (const YAML::KeyNotFound &ex) {
        parse_err("cannot locate mandatory field '" + name + "'", node);
    }

    assert(0);
}

void
YAMLParserWrapper::parse_err(const std::string &err, const YAML::Node &node) const {
    std::stringstream ss;

    ss << "'" << m_header << "' - YAML parsing error at line " << node.GetMark().line << ": ";
    ss << err;

    throw std::runtime_error(ss.str());
}

void
YAMLParserWrapper::parse_err(const std::string &err) const {
    std::stringstream ss;

    ss << "'" << m_header << "' - YAML parsing error: " << err;

    throw std::runtime_error(ss.str());
}
