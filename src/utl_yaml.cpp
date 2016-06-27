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

bool utl_yaml_read_mac_addr(const YAML::Node &node,
                            const std::string &name,
                            uint64_t &val) {

    std::string mac_str;

    if (!node.FindValue(name)) {
        return false;
    }

    node[name] >> mac_str;

    std::vector<std::string> tokens;
    split_str_by_delimiter(mac_str, ':', tokens);

    if (tokens.size() != 6) {
        throw YAML::InvalidScalar(node[name].GetMark());
    }

    val = 0;

    for (int i = 0; i < 6 ; i++) {
        char *endptr = NULL;
        unsigned long octet = strtoul(tokens[i].c_str(), &endptr, 16);

        if ( (*endptr != 0) || (octet > 0xff) ) {
            throw YAML::InvalidScalar(node[name].GetMark());
        }

        //mac_addr[i] = (uint8_t)octet;
        val = (val << 8) + octet;
    }   
                  
    return true;
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

bool utl_yaml_read_bool(const YAML::Node& node, 
                        const std::string &name,
                        bool & val){
    bool res=false;
    if ( node.FindValue(name) ) {
        node[name] >> val ;
        res=true;
    }
    return( res);
}


