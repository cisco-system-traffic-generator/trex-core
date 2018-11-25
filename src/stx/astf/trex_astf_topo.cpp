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

#include <assert.h>

#include <json/json.h>

#include "common/Network/Packet/MacAddress.h"
#include "common/basic_utils.h"

#include "trex_astf_topo.h"
#include "trex_client_config.h"
#include "trex_exception.h"
#include "trex_global.h"
#include "trex_defs.h"

using namespace std;

TopoMngr* TopoMngr::m_inst = nullptr;


TopoError::TopoError(const string &what) : runtime_error(what) {
}

void parse_err(const string &what) {
    throw TopoError("Topology parsing error: " + what);
}

void build_err(const string &what) {
    throw TopoError("Topology building error: " + what);
}

Json::Value json_map_get(Json::Value &json_obj, const string &key, const string &err_prefix) {
    Json::Value json_value = json_obj[key];
    if ( json_value == Json::nullValue ) {
        parse_err(err_prefix + " is missing '" + key + "' key");
    }
    return json_value;
}


TopoGW::TopoGW(string src_start, string src_end, string dst_mac, string dst) {
    m_src_start = src_start;
    m_src_end   = src_end;
    m_dst_mac   = dst_mac;
    m_dst       = dst;
}


void TopoGW::to_json(Json::Value &obj) {
    obj["src_start"] = m_src_start;
    obj["src_end"]   = m_src_end;
    obj["dst_mac"]   = m_dst_mac;
    obj["dst"]       = m_dst;
}


TopoVIF::TopoVIF(string src_mac, uint16_t vlan, string src_ipv4, string src_ipv6) {
    m_src_mac  = src_mac;
    m_vlan     = vlan;
    m_src_ipv4 = src_ipv4;
    m_src_ipv6 = src_ipv6;
}


void TopoVIF::to_json(Json::Value &obj) {
    obj["src_mac"]  = m_src_mac;
    obj["vlan"]     = m_vlan;
    obj["src_ipv4"] = m_src_ipv4;
    obj["src_ipv6"] = m_src_ipv6;
}


TopoMngr::TopoMngr() {
    m_topo_per_port.resize(TREX_MAX_PORTS);
}


void TopoMngr::clear() {
    std::unique_lock<std::recursive_mutex> lock(m_lock);
    for (auto &port_topo: m_topo_per_port) {
        port_topo.clear();
    }
}


void TopoMngr::from_json_str(const string &topo_buffer) {
    vector<port_topo_t> tmp_topo_per_port(TREX_MAX_PORTS);

    Json::Reader reader;
    Json::Value json_obj;
    bool rc = reader.parse(topo_buffer, json_obj, false);
    if (!rc) {
        parse_err(reader.getFormattedErrorMessages());
    }

    // VIFs
    Json::Value vifs = json_map_get(json_obj, "vifs", "JSON map");

    for (auto &vif : vifs) {

        uint8_t trex_port = json_map_get(vif, "trex_port", "VIF").asUInt();
        if ( trex_port >= TREX_MAX_PORTS ) {
            parse_err("VIF TRex port is too large: " + to_string(trex_port));
        }

        uint32_t sub_if = json_map_get(vif, "sub_if", "VIF").asUInt();
        if ( !sub_if ) {
            parse_err("VIF 'sub_if' must be positive");
        }

        string src_mac = json_map_get(vif, "src_mac", "VIF").asString();
        string src_ipv4 = json_map_get(vif, "src_ipv4", "VIF").asString();
        string src_ipv6 = json_map_get(vif, "src_ipv6", "VIF").asString();

        uint16_t vlan = json_map_get(vif, "vlan", "VIF").asUInt();
        if ( trex_port > 4096 ) {
            parse_err("VIF 'vlan' is too large: " + to_string(vlan));
        }

        TopoVIF vif_obj(src_mac, vlan, src_ipv4, src_ipv6);
        if ( !tmp_topo_per_port[trex_port].emplace(sub_if, vif_obj).second ) {
            parse_err("Double sub_if: " + to_string(sub_if));
        }

    }

    // GWs
    Json::Value gws = json_map_get(json_obj, "gws", "JSON map");

    for (auto &gw : gws) {

        uint8_t trex_port = json_map_get(gw, "trex_port", "GW").asUInt();
        if ( trex_port >= TREX_MAX_PORTS ) {
            parse_err("GW TRex port is too large: " + to_string(trex_port));
        }
        if ( trex_port % 2 ) {
            parse_err("GW must be specified only for 'client' TRex ports, got: " + to_string(trex_port));
        }

        uint32_t sub_if = json_map_get(gw, "sub_if", "GW").asUInt();

        auto subif_it = tmp_topo_per_port[trex_port].find(sub_if);
        if ( subif_it == tmp_topo_per_port[trex_port].end() ) {
            if ( !sub_if ) {
                TopoVIF vif_obj("", 0, "", ""); // mock
                subif_it = tmp_topo_per_port[trex_port].emplace(0, vif_obj).first;
            } else {
                parse_err("GW has sub_if '" + to_string(sub_if) + "' which is missing");
            }
        }

        string src_start = json_map_get(gw, "src_start", "GW").asString();
        string src_end = json_map_get(gw, "src_end", "GW").asString();
        string dst_mac = json_map_get(gw, "dst_mac", "GW").asString();
        string dst = json_map_get(gw, "dst", "GW").asString();

        TopoGW gw_obj(src_start, src_end, dst_mac, dst);
        subif_it->second.m_gws.emplace_back(gw_obj);

    }

    std::unique_lock<std::recursive_mutex> lock(m_lock);
    swap(tmp_topo_per_port, m_topo_per_port);

}

/*
void TopoMngr::build_cc_db(ClientCfgDB &cc_db) {

}
*/

void TopoMngr::dump() {
    std::unique_lock<std::recursive_mutex> lock(m_lock);
    for (auto &port_topo: m_topo_per_port) {
        for (auto &iter_pair : port_topo) {
            printf("VIF %u, src: %s\n", iter_pair.first, iter_pair.second.get_src_mac().c_str());
            for (auto &gw : iter_pair.second.m_gws) {
                printf("    GW: dst_mac: %s\n", gw.get_dst_mac().c_str());
            }
        }
    }
}


void TopoMngr::to_json(Json::Value &result) {
    std::unique_lock<std::recursive_mutex> lock(m_lock);
    Json::Value vifs;
    Json::Value gws;
    for (uint8_t port_id = 0; port_id < TREX_MAX_PORTS; port_id++) {
        for (auto &iter_pair : m_topo_per_port[port_id]) {
            if ( iter_pair.first ) {
                Json::Value vif;
                vif["trex_port"] = port_id;
                vif["sub_if"]    = iter_pair.first;
                iter_pair.second.to_json(vif);
                vifs.append(vif);
            }
            for (auto gw_obj : iter_pair.second.m_gws) {
                Json::Value gw;
                gw["trex_port"] = port_id;
                gw["sub_if"]    = iter_pair.first;
                gw_obj.to_json(gw);
                gws.append(gw);
            }
        }
    }

    result["vifs"] = vifs;
    result["gws"] = gws;
}


void TopoMngr::create_instance() {
    assert(!m_inst);
    m_inst = new TopoMngr();
}


TopoMngr* TopoMngr::get_instance() {
    assert(m_inst);
    return m_inst;
}


void TopoMngr::delete_instance() {
    delete m_inst;
}




