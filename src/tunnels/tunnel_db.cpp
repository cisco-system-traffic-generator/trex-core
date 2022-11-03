#include "tunnel_db.h"
#include <json/json.h>
#include "common/basic_utils.h"
#include "arpa/inet.h"
#include "trex_client_config.h"
#include "tunnels/tunnel_factory.h"
#include "rx_check.h"

/************************************************* CClientCfgEntryTunnel ****************************************************************************/

void CClientCfgEntryTunnel::assign(ClientCfgBase &info, uint32_t ip) const {
    assert(contains(ip));
    uint32_t count = m_tunnel_group.get_end_ip() - m_tunnel_group.get_start_ip() + 1;
    uint32_t index = (ip -  m_tunnel_group.get_start_ip()) % count;
    client_tunnel_data_t tmp;
    m_tunnel_group.get_tunnel_data(tmp);
    // teid = inital_teid + (index) * teid_jump
    tmp.teid = tmp.teid + (index) * m_tunnel_group.get_teid_jump();
    info.m_tunnel_ctx = get_tunnel_ctx(&tmp);
}

/************************************************* CTunnelsLatencyPerPort ****************************************************************************/

void CTunnelsLatencyPerPort::to_json(Json::Value& latency_json) const {
    latency_json["client_port_id"] = m_client_port_id;
    latency_json["client_ip"] = m_client_ip;
    latency_json["server_ip"] = m_server_ip;
}


/************************************************* CTunnelsCtxGroup ****************************************************************************/


void CTunnelsCtxGroup::to_json(Json::Value& group_json) const {
    group_json["src_start"] = utl_uint32_to_ipv4(m_start_ip);
    group_json["src_end"] = utl_uint32_to_ipv4(m_end_ip);
    group_json["initial_teid"] = m_tunnel_data.teid;
    group_json["teid_jump"] = m_teid_jump;
    group_json["sport"] = m_tunnel_data.src_port;
    group_json["version"] = m_tunnel_data.version;
    group_json["tunnel_type"] = m_tunnel_data.type;
    group_json["activate"] = m_activate;
    group_json["vid"] = m_vid;
    group_json["cvlan_id"] = m_cvlan_id;
    group_json["svlan_id"] = m_svlan_id;
    std::string src_ip;
    std::string dst_ip;
    if (m_tunnel_data.version == 6) {
        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, (const char *)&m_tunnel_data.src.ipv6.addr, (char*)str, INET6_ADDRSTRLEN);
        src_ip = std::string(str);
        inet_ntop(AF_INET6, (const char *)&m_tunnel_data.dst.ipv6.addr, (char*)str, INET6_ADDRSTRLEN);
        dst_ip = std::string(str);
    } else {
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, (const char *)&m_tunnel_data.src.ipv4, (char*)str, INET_ADDRSTRLEN);
        src_ip = std::string(str);
        inet_ntop(AF_INET, (const char *)&m_tunnel_data.dst.ipv4, (char*)str, INET_ADDRSTRLEN);
        dst_ip = std::string(str);
    }
    group_json["src_ip"] = src_ip;
    group_json["dst_ip"] = dst_ip;
}


/************************************************* CTunnelsTopo ***************************************************************************/


TunnelTopoError::TunnelTopoError(const string &what) : runtime_error(what) {
}


void parse_tunnel_topo_err(const string &what) {
    throw TunnelTopoError("Tunnel Topology parsing error: " + what);
}


void json_map_init(Json::Value &json_obj, const string &key, const string &err_prefix, Json::Value& json_value) {
    json_value = json_obj[key];
    if ( json_value == Json::nullValue ) {
        parse_tunnel_topo_err(err_prefix + " is missing '" + key + "' key");
    }
}


Json::Value json_map_get_value(Json::Value &json_obj, const string &key, const string &err_prefix) {
    Json::Value json_value = json_obj[key];
    if ( json_value == Json::nullValue ) {
        parse_tunnel_topo_err(err_prefix + " is missing '" + key + "' key");
    }
    return json_value;
}


bool compare_tunnel_groups(CTunnelsCtxGroup group1, CTunnelsCtxGroup group2)
{
    return (group1.get_start_ip() < group2.get_start_ip());
}


void CTunnelsTopo::validate_tunnel_topo(std::vector<CTunnelsCtxGroup>& groups) {
    sort(groups.begin(), groups.end(), compare_tunnel_groups);
    CTunnelsCtxGroup* prev = nullptr;
    for(int i=0;i<groups.size();i++) {
        if (prev) {
            if (groups[i].get_start_ip() == prev->get_start_ip()) {
                std::stringstream ss;
                ss<<"At least two Tunnel contexts start range with: ";
                ss<<utl_uint32_to_ipv4(prev->get_start_ip());
                parse_tunnel_topo_err(ss.str());
            }
            if (prev->get_end_ip() >= groups[i].get_start_ip()) {
                std::stringstream ss;
                ss<<"Tunnel context ranges intersect: ";
                ss<<utl_uint32_to_ipv4(prev->get_start_ip())<<"-"<<utl_uint32_to_ipv4(prev->get_end_ip())<<" ";
                ss<<utl_uint32_to_ipv4(groups[i].get_start_ip())<<"-"<<utl_uint32_to_ipv4(groups[i].get_end_ip());
                parse_tunnel_topo_err(ss.str());
            }
        }
        prev = &groups[i];
    }
}


void CTunnelsTopo::from_json_obj_tunnel_groups(Json::Value& tunnels_group_json) {
    std::vector<CTunnelsCtxGroup> tmp_groups;
    for (auto &group : tunnels_group_json) {
        client_tunnel_data_t tunnel_data;
        memset(&tunnel_data, 0 , sizeof(client_tunnel_data_t));
        tunnel_data.client_ip = 0;
        std::string src_start_str = json_map_get_value(group, "src_start", "JSON map").asString();
        std::string src_end_str = json_map_get_value(group, "src_end", "JSON map").asString();
        uint32_t src_start;
        uint32_t src_end;
        // uint16_t cvlan_id = 0;
        // uint16_t svlan_id = 0;
        uint16_t vid = 0;
        bool rc = utl_ipv4_to_uint32(src_start_str.c_str(), src_start);
        rc = rc && utl_ipv4_to_uint32(src_end_str.c_str(), src_end);
        if (!rc) {
            parse_tunnel_topo_err("invalid src_start and src_end: " + src_start_str + " " + src_end_str);
        }
        if (src_start > src_end) {
            parse_tunnel_topo_err("src_start must be <= to src_end");
        }
        uint32_t teid_jump = json_map_get_value(group, "teid_jump", "JSON map").asUInt();
        tunnel_data.teid = json_map_get_value(group, "initial_teid", "JSON map").asUInt();
        tunnel_data.src_port = json_map_get_value(group, "sport", "JSON map").asUInt();
        tunnel_data.version = json_map_get_value(group, "version", "JSON map").asUInt();
        tunnel_data.type = json_map_get_value(group, "tunnel_type", "JSON map").asUInt();
        std::string src_ipv46 = json_map_get_value(group, "src_ip", "JSON map").asString();
        std::string dst_ipv46 = json_map_get_value(group, "dst_ip", "JSON map").asString();
        bool activate = json_map_get_value(group, "activate", "JSON map").asBool();
        if (tunnel_data.type == TUNNEL_TYPE_VLAN) {
            tunnel_data.vid = json_map_get_value(group, "vid", "JSON map").asUInt();
            vid = tunnel_data.vid;
        }
        if (tunnel_data.version == 6) {
           rc = (inet_pton(AF_INET6, src_ipv46.c_str(), tunnel_data.src.ipv6.addr) == 1);
           rc = rc && (inet_pton(AF_INET6, dst_ipv46.c_str(), tunnel_data.dst.ipv6.addr) == 1);
        } else {
           rc = (inet_pton(AF_INET, src_ipv46.c_str(), &tunnel_data.src.ipv4) == 1);
           rc = rc && (inet_pton(AF_INET, dst_ipv46.c_str(), &tunnel_data.dst.ipv4) == 1);
        }
        if (!rc) {
            parse_tunnel_topo_err("invalid src_ip and dst_ip: " + src_ipv46 + " " + dst_ipv46);
        }
        tmp_groups.push_back(CTunnelsCtxGroup(src_start, src_end, teid_jump, tunnel_data, activate, vid));
    }

    validate_tunnel_topo(tmp_groups);
    std::unique_lock<std::recursive_mutex> lock(m_lock);
    std::swap(tmp_groups, m_tunnels_groups);
}


void CTunnelsTopo::from_json_obj_tunnel_latency(Json::Value& latency_json) {
    client_per_port_t tmp_latency_clients;
    uint8_t expected_clients = CGlobalInfo::m_options.get_expected_dual_ports();
    for (auto &l : latency_json) {
        std::vector<uint32_t> ips = {0, 0};
        std::vector<std::string> ips_dir = {"client_ip", "server_ip"};
        for (int i=0;i<ips.size();i++) {
            std::string ip_str = json_map_get_value(l, ips_dir[i], "JSON map").asString();
            bool rc = utl_ipv4_to_uint32(ip_str.c_str(), ips[i]);
            if (!rc) {
                parse_tunnel_topo_err("invalid " + ips_dir[i] + " " + ip_str);
            }
        }
        uint8_t client_port_id = json_map_get_value(l, "client_port_id", "JSON map").asUInt();
        pkt_dir_t dir = port_id_to_dir(client_port_id);
        if (dir != CLIENT_SIDE) {
            parse_tunnel_topo_err("client_port_id must be even number " + to_string(client_port_id));
        }
        if (client_port_id >= expected_clients<<1) {
            parse_tunnel_topo_err("client_port_id: " + to_string(client_port_id) + "exceeds num of ports: " + to_string(expected_clients<<1));
        }
        std::pair<uint8_t, CTunnelsLatencyPerPort> p = std::make_pair(client_port_id, CTunnelsLatencyPerPort(client_port_id, ips[0], ips[1]));
        if (!tmp_latency_clients.insert(p).second) {
            parse_tunnel_topo_err("at least two instances of the port: " + to_string(client_port_id));
        }
    }

    if ((tmp_latency_clients.size() != expected_clients) && tmp_latency_clients.size()) {
        std::stringstream ss;
        ss<<"There should be one latency client per dual port."<<endl;
        ss<<"num of latency clients found is: "<<tmp_latency_clients.size()<<endl;
        ss<<"num of dual ports is: "<<to_string(expected_clients)<<endl;
        parse_tunnel_topo_err(ss.str());
    }

    std::unique_lock<std::recursive_mutex> lock(m_lock);
    std::swap(tmp_latency_clients, m_latency_clients);
}


void CTunnelsTopo::from_json_str(const std::string &topo_buffer) {
    std::vector<CTunnelsCtxGroup> tmp_groups;
    Json::Reader reader;
    Json::Value json_obj;
    bool rc = reader.parse(topo_buffer, json_obj, false);
    if (!rc) {
        parse_tunnel_topo_err(reader.getFormattedErrorMessages());
    }
    Json::Value tunnels_group;
    Json::Value latency;
    json_map_init(json_obj, "tunnels", "JSON map", tunnels_group);
    json_map_init(json_obj, "latency", "JSON map", latency);
    from_json_obj_tunnel_groups(tunnels_group);
    from_json_obj_tunnel_latency(latency);
}


void CTunnelsTopo::clear() {
    std::unique_lock<std::recursive_mutex> lock(m_lock);
    m_tunnels_groups.clear();
    m_latency_clients.clear();
}


const std::vector<CTunnelsCtxGroup>& CTunnelsTopo::get_tunnel_topo() const {
    return m_tunnels_groups;
}


const client_per_port_t& CTunnelsTopo::get_latency_clients() const {
    return m_latency_clients;
}


void CTunnelsTopo::to_json(Json::Value& val){
    val["tunnels"] = Json::arrayValue;
    for (int i=0;i<m_tunnels_groups.size();i++) {
        Json::Value value;
        m_tunnels_groups[i].to_json(value);
        val["tunnels"].append(value);
    }
    val["latency"] = Json::arrayValue;
    for (auto& l : m_latency_clients) {
        Json::Value value;
        l.second.to_json(value);
        val["latency"].append(value);
    }
}


/************************************************* CTunnelsDB ****************************************************************************/


void CTunnelsDB::load_from_tunnel_topo(const CTunnelsTopo *topo) {
    const std::vector<CTunnelsCtxGroup> &tunnel_topo = topo->get_tunnel_topo();
    m_groups.clear();
    m_cache_group = nullptr;
    for (const CTunnelsCtxGroup& tunnels_group : tunnel_topo) {
        CClientCfgEntryTunnel group_entry;
        group_entry.m_tunnel_group = tunnels_group;
        m_groups[group_entry.m_tunnel_group.get_start_ip()] = group_entry;
    }
}


CClientCfgEntryTunnel* CTunnelsDB::lookup(uint32_t ip) {

    /* a cache to avoid constant search (usually its a range of IPs) */
    if ( (m_cache_group) && (m_cache_group->contains(ip)) ) {
        return m_cache_group;
    }

    /* clear the cache pointer */
    m_cache_group = NULL;

    std::map<uint32_t ,CClientCfgEntryTunnel>::iterator it;

    /* upper bound fetchs the first greater element */
    it = m_groups.upper_bound(ip);

    /* if the first element in the map is bigger - its not in the map */
    if (it == m_groups.begin()) {
        return NULL;
    }

    /* go one back - we know it's not on begin so we have at least one back */
    it--;

    CClientCfgEntryTunnel &group = (*it).second;

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


CClientCfgEntryTunnel* CTunnelsDB::lookup(const std::string &ip) {
    uint32_t addr = (uint32_t)inet_addr(ip.c_str());
    addr = PKT_NTOHL(addr);

    return lookup(addr);
}