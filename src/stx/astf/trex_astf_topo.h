#ifndef __TREX_ASTF_TOPO_H__
#define __TREX_ASTF_TOPO_H__

/*
 TRex team
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

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Json {
    class Value;
}


class TopoError : public std::runtime_error {
public:
    TopoError(const std::string &what);
};


void build_err(const std::string &what);


class TopoGW {
    std::string m_src_start;
    std::string m_src_end;
    std::string m_dst_mac;
    std::string m_dst;

public:
    TopoGW(std::string src_start, std::string src_end, std::string dst_mac, std::string dst);
    void to_json(Json::Value &obj);
    const std::string& get_start()   const { return m_src_start; }
    const std::string& get_end()     const { return m_src_end; }
    const std::string& get_dst_mac() const { return m_dst_mac; }
};


class TopoVIF {
    std::string m_src_mac;
    std::string m_src_ipv4;
    std::string m_src_ipv6;
    uint16_t m_vlan;

public:
    std::vector<TopoGW> m_gws;
    TopoVIF(std::string src_mac, uint16_t vlan, std::string src_ipv4, std::string src_ipv6);
    void to_json(Json::Value &obj);
    const uint16_t     get_vlan()    const { return m_vlan; }
    const std::string& get_src_mac() const { return m_src_mac; }
};


typedef std::map<uint32_t,TopoVIF> port_topo_t;
typedef std::vector<port_topo_t> topo_per_port_t;


class TopoMngr {
    topo_per_port_t m_topo_per_port;
    std::recursive_mutex m_lock;

public:
    TopoMngr();
    void from_json_str(const std::string &topo_buffer);
    const topo_per_port_t& get_topo() const {
        return m_topo_per_port;
    }
    void to_json(Json::Value &result);
    void clear();
    void dump();
};



#endif /* __TREX_ASTF_TOPO_H__ */
