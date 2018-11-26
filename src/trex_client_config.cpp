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
#include "stx/astf/trex_astf_topo.h"


void ClientCfgDirBase::dump(FILE *fd) const {
    if (has_src_mac_addr()) {
        fprintf(fd, "        src_mac: %s\n", utl_macaddr_to_str(m_src_mac.GetConstBuffer()).c_str());
    } else {
        fprintf(fd, "#       No src MAC\n");
    }
    if (has_dst_mac_addr()) {
        fprintf(fd, "        dst_mac: %s\n", utl_macaddr_to_str(m_dst_mac.GetConstBuffer()).c_str());
    } else {
        fprintf(fd, "#       No dst MAC\n");
    }
    if (has_vlan()) {
        fprintf(fd, "        vlan: %d\n", m_vlan);
    } else {
        fprintf(fd, "#       No vlan\n");
    }
}

void ClientCfgDirBase::update(uint32_t index, const ClientCfgDirExt &cfg) {
    if (has_src_mac_addr()) {
        m_src_mac += index;
    }

    if (!has_dst_mac_addr() && (cfg.has_next_hop() || cfg.has_ipv6_next_hop())) {
        m_dst_mac = cfg.get_resolved_mac(index);
        m_bitfield |= HAS_DST_MAC;
    }
}

bool ClientCfgDirExt::need_resolve() const {
    if (has_next_hop() || has_ipv6_next_hop())
        return true;
    else
        return false;
}

void ClientCfgDirExt::set_no_resolve_needed() {
    m_bitfield &= ~(HAS_DST_MAC | HAS_IPV6_NEXT_HOP | HAS_NEXT_HOP);
    m_bitfield |= CAN_NOT_USE;
}

void ClientCfgDirExt::dump(FILE *fd) const {
    ClientCfgDirBase::dump(fd);

    if (has_next_hop()) {
        fprintf(fd, "        next_hop: %s\n", ip_to_str(m_next_hop).c_str());
    } else {
        fprintf(fd, "#       No next hop\n");
    }
    if (has_ipv6_next_hop()) {
        fprintf(fd, "        ipv6_next_hop: %s\n", ip_to_str((unsigned char *)m_ipv6_next_hop).c_str());
    } else {
        fprintf(fd, "#       No IPv6 next hop\n");
    }

    if (m_resolved_macs.size() > 0) {
        fprintf(fd, "#  Resolved MAC list:\n");
        for (int i = 0; i < m_resolved_macs.size(); i++) {
            fprintf(fd, "#     %s\n", utl_macaddr_to_str(m_resolved_macs[i].GetConstBuffer()).c_str());
        }
    }
}

void ClientCfgDirExt::set_resolved_macs(CManyIPInfo &pretest_result, uint16_t count) {
    uint16_t vlan = has_vlan() ? m_vlan : 0;
    MacAddress base_mac = m_dst_mac;
    m_resolved_macs.resize(count);

    for (int i = 0; i < count; i++) {
        if (need_resolve()) {
            if (has_next_hop()) {
                if (!pretest_result.lookup(m_next_hop + i, vlan, m_resolved_macs[i])) {
                    fprintf(stderr, "Failed resolving ip:%x, vlan:%d - exiting\n", m_next_hop+i, vlan);
                    exit(1);
                }
            } else {
                //??? handle ipv6
            }
        } else {
            m_resolved_macs[i] = base_mac;
            base_mac += 1;
        }
    }
}

void ClientCfgBase::update(uint32_t index, const ClientCfgExt *cfg) {
    m_initiator.update(index, cfg->m_initiator);
    m_responder.update(index, cfg->m_responder);
    
    m_is_set = true;
}


void
ClientCfgBase::apply(rte_mbuf_t *m, pkt_dir_t dir) const {

    assert(m_is_set);
    
    uint8_t *p = rte_pktmbuf_mtod(m, uint8_t *);
    
    /* take the right direction config */
    const ClientCfgDirBase &cfg_dir = ( (dir == CLIENT_SIDE) ? m_initiator : m_responder);

    /* dst mac */
    if (cfg_dir.has_dst_mac_addr()) {
        memcpy(p, cfg_dir.get_dst_mac_addr(), 6);
    }

    /* src mac */
    if (cfg_dir.has_src_mac_addr()) {
        memcpy(p + 6, cfg_dir.get_src_mac_addr(), 6);
    }

    /* VLAN */
    if (cfg_dir.has_vlan()) {
        add_vlan(m, cfg_dir.get_vlan());
    }
}


void
ClientCfgEntry::dump(FILE *fd) const {

    fprintf(fd, "-   ip_start : %s\n", ip_to_str(m_ip_start).c_str());
    fprintf(fd, "    ip_end   : %s\n", ip_to_str(m_ip_end).c_str());
    m_cfg.dump(fd);
    fprintf(fd, "    count    : %d\n", m_count);
}

void ClientCfgEntry::set_resolved_macs(CManyIPInfo &pretest_result) {
    m_cfg.m_initiator.set_resolved_macs(pretest_result, m_count);
    m_cfg.m_responder.set_resolved_macs(pretest_result, m_count);
}

void ClientCfgCompactEntry::fill_from_dir(ClientCfgDirExt cfg, uint8_t port_id) {
    m_port = port_id;
    if (cfg.has_next_hop()) {
        m_next_hop_base.ip = cfg.m_next_hop;
        if (cfg.has_src_ip()) {
            m_src_ip.ip = cfg.m_src_ip;
        } else {
            m_src_ip.ip = 0;
        }
        m_is_ipv4 = true;
    } else if (cfg.has_ipv6_next_hop()) {
        memcpy(m_next_hop_base.ipv6, cfg.m_ipv6_next_hop, sizeof(m_next_hop_base.ipv6));
        if (cfg.has_src_ipv6()) {
            memcpy(m_src_ip.ipv6, cfg.m_src_ipv6, sizeof(m_src_ip.ipv6));
        } else {
            memset(m_src_ip.ipv6, 0, sizeof(m_src_ip.ipv6));
        }
        m_is_ipv4 = false;
    }

    if (cfg.has_vlan()) {
        m_vlan = cfg.m_vlan;
    } else {
        m_vlan = 0;
    }
}

ClientCfgDB::ClientCfgDB() {
    m_cache_group = NULL;
    m_under_vlan  = false;
    m_tg = NULL;
}

ClientCfgDB::~ClientCfgDB() {
    clear();
}

void
ClientCfgDB::dump(FILE *fd) {
    //fprintf(fd, "#**********Client config file start*********\n");
    fprintf(fd, "vlan: %s\n", m_under_vlan ? "true" : "false");
    fprintf(fd, "groups:\n");

    for (std::map<uint32_t, ClientCfgEntry>::iterator it = m_groups.begin(); it != m_groups.end(); ++it) {
        fprintf(fd, "# ****%s:****\n", ip_to_str(it->first).c_str());
        ((ClientCfgEntry)it->second).dump(fd);
    }
    //fprintf(fd, "#**********Client config end*********\n");
}

void ClientCfgDB::clear() {
    m_groups.clear();
}

void ClientCfgDB::set_resolved_macs(CManyIPInfo &pretest_result) {
    std::map<uint32_t, ClientCfgEntry>::iterator it;
    for (it = m_groups.begin(); it != m_groups.end(); it++) {
        ClientCfgEntry &cfg = it->second;
        cfg.set_resolved_macs(pretest_result);
    }
}

void ClientCfgDB::get_entry_list(std::vector<ClientCfgCompactEntry *> &ret) {
    uint8_t port;
    bool result;

    for (std::map<uint32_t, ClientCfgEntry>::iterator it = m_groups.begin(); it != m_groups.end(); ++it) {
        ClientCfgEntry &cfg = it->second;
        if (cfg.m_cfg.m_initiator.need_resolve() || cfg.m_cfg.m_responder.need_resolve()) {
            assert(m_tg != NULL);
            result = m_tg->find_port(cfg.m_ip_start, cfg.m_ip_end, port);
            if (! result) {
                fprintf(stderr, "Error in client config range %s - %s.\n"
                        , ip_to_str(cfg.m_ip_start).c_str(), ip_to_str(cfg.m_ip_end).c_str());
                    exit(-1);
            }
            if (port == UINT8_MAX) {
                // if port not found, it means this adderss is not needed. Don't try to resolve.
                cfg.m_cfg.m_initiator.set_no_resolve_needed();
                cfg.m_cfg.m_responder.set_no_resolve_needed();
            } else {
                if (cfg.m_cfg.m_initiator.need_resolve()) {
                    ClientCfgCompactEntry *init_entry = new ClientCfgCompactEntry();
                    assert(init_entry);
                    init_entry->m_count = cfg.m_count;
                    init_entry->fill_from_dir(cfg.m_cfg.m_initiator, port);
                    ret.push_back(init_entry);
                }

                if (cfg.m_cfg.m_responder.need_resolve()) {
                    ClientCfgCompactEntry *resp_entry = new ClientCfgCompactEntry();
                    assert(resp_entry);
                    resp_entry->m_count = cfg.m_count;
                    resp_entry->fill_from_dir(cfg.m_cfg.m_responder, port + 1);
                    ret.push_back(resp_entry);
                }
            }
        }
    }
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
    m_cache_group = nullptr;

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

    std::string err = "";
    verify(err);
    if ( err.size() ) {
        parser.parse_err(err);
    }

}

void ClientCfgDB::load_from_topo(const TopoMngr *topomngr) {
    const topo_per_port_t &topo_per_port = topomngr->get_topo();
    std::string err = "";

    m_groups.clear();
    m_cache_group = nullptr;

    bool rc;
    uint16_t vlan;

    for (uint8_t trex_port = 0; trex_port < TREX_MAX_PORTS; trex_port+=2) {
        for (auto &iter_pair : topo_per_port[trex_port]) {
            const TopoVIF &vif = iter_pair.second;
            for (auto gw : vif.m_gws) {
                ClientCfgEntry group;

                rc = utl_ipv4_to_uint32(gw.get_start().c_str(), group.m_ip_start);
                if ( !rc ) {
                    build_err("GW has invalid start of IP range: " + gw.get_start());
                }

                rc = utl_ipv4_to_uint32(gw.get_end().c_str(), group.m_ip_end);
                if ( !rc ) {
                    build_err("GW has invalid end of IP range: " + gw.get_end());
                }

                if ( group.m_ip_start > group.m_ip_end ) {
                    build_err("GW start of IP range: " + gw.get_start() + " is greater than end: " + gw.get_end());
                }

                group.m_count = 1;

                ClientCfgDirExt &cdir = group.m_cfg.m_initiator;
                if ( iter_pair.first ) { // sub_if
                    uint64_t src_mac;
                    rc = mac2uint64(vif.get_src_mac(), src_mac);
                    if ( !rc ) {
                        build_err("VIF has invalid MAC: " + vif.get_src_mac());
                    }

                    cdir.set_src_mac_addr(src_mac);
                    vlan = vif.get_vlan();
                } else {
                    vlan = CGlobalInfo::m_options.m_ip_cfg[trex_port].get_vlan();
                }

                if ( vlan ) {
                    cdir.set_vlan(vlan);
                }

                uint64_t dst_mac;
                rc = mac2uint64(gw.get_dst_mac(), dst_mac);
                if ( !rc ) {
                    build_err("GW has invalid MAC: " + gw.get_dst_mac());
                }
                cdir.set_dst_mac_addr(dst_mac);

                //group.dump(stdout);
                m_groups[group.m_ip_start] = group;
            }
        }
    }

    verify(err);
    if ( err.size() ) {
        build_err(err);
    }
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
ClientCfgDB::parse_dir(YAMLParserWrapper &parser, const YAML::Node &node, ClientCfgDirExt &dir) {
    if (node.FindValue("src_ip")) {
        dir.set_src_ip(parser.parse_ip(node, "src_ip"));
    }

    if (node.FindValue("src_ipv6")) {
        uint16_t ip_num[8];
        parser.parse_ipv6(node, "src_ipv6", (unsigned char *)&ip_num);
        dir.set_src_ipv6(ip_num);
    }

    if (node.FindValue("next_hop")) {
        dir.set_next_hop(parser.parse_ip(node, "next_hop"));
    }

    if (node.FindValue("ipv6_next_hop")) {
        uint16_t ip_num[8];
        parser.parse_ipv6(node, "ipv6_next_hop", (unsigned char *)&ip_num);
        dir.set_ipv6_next_hop(ip_num);
    }

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

    if ((dir.has_next_hop() || dir.has_ipv6_next_hop()) && (dir.has_dst_mac_addr() || dir.has_src_mac_addr())) {
        parser.parse_err("Should not configure both next_hop/ipv6_next_hop and dst_mac or src_mac", node);
    }

    if (dir.has_next_hop() && dir.has_ipv6_next_hop()) {
        parser.parse_err("Should not configure both next_hop and ipv6_next_hop", node);
    }

}

/**
 * sanity checks
 *
 * @author imarom (28-Jun-16)
 */
void
ClientCfgDB::verify(std::string &err) const {
    uint32_t monotonic = 0;

    /* check that no interval overlaps */

    /* all intervals do not overloap iff when sorted each start/end dots are strong monotonic */
    for (const auto &p : m_groups) {
        const ClientCfgEntry &group = p.second;

        if ( (monotonic > 0 ) && (group.m_ip_start <= monotonic) ) {
            err = "IP '" + ip_to_str(group.m_ip_start) + "' - '" + ip_to_str(group.m_ip_end) + "' overlaps with other groups";
            return;
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
    if (! group.m_cfg.m_initiator.can_be_used()) {
        fprintf(stderr, "There is some client configuration related error\n");
        fprintf(stderr, "Please check that ip_generator ip range and client_cfg ip_range match\n");
        fprintf(stderr, "Problem is with following group:\n");
        group.dump(stdout);
        exit(1);
    }

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
