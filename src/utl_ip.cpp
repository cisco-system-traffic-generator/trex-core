/*
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2016-2016 Cisco Systems, Inc.

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
#include <string>
#include <iostream>
#include <pkt_gen.h>
#include "utl_ip.h"

void COneIPInfo::dump(FILE *fd, const char *offset) const {
    uint8_t mac[ETHER_ADDR_LEN];
    m_mac.copyToArray(mac);
    char ip_str[100];
    get_ip_str(ip_str);
    std::string mac_str;
    utl_macaddr_to_str(mac, mac_str);
    const char *mac_char = resolve_needed() ?  "Unknown" : mac_str.c_str();
    fprintf(fd, "%sip: %s ", offset, ip_str);
    if (m_vlan != 0)
        fprintf(fd, "vlan: %d ", m_vlan);
    if (m_port != UINT8_MAX)
        fprintf(fd, "port: %d ", m_port);
    fprintf(fd, "mac: %s", mac_char);
    fprintf(fd, "\n");
}

bool COneIPInfo::resolve_needed() const {
    return m_mac.isDefaultAddress();
}

/*
 * Fill buffer p with arp request.
 * port_id - port id we intend to send on
 * sip - source IP/MAC information
 */
void COneIPv4Info::fill_arp_req_buf(uint8_t *p, uint16_t port_id, COneIPInfo *sip) {
    uint8_t src_mac[ETHER_ADDR_LEN];
    sip->get_mac(src_mac);

    CTestPktGen::create_arp_req(p, ((COneIPv4Info *)sip)->get_ip(), m_ip, src_mac, m_vlan, port_id);
}

void COneIPv4Info::fill_grat_arp_buf(uint8_t *p) {
    uint8_t src_mac[ETHER_ADDR_LEN];
    get_mac(src_mac);

    CTestPktGen::create_arp_req(p, m_ip, m_ip, src_mac, m_vlan, 0);
}

void COneIPv6Info::fill_arp_req_buf(uint8_t *p, uint16_t port_id, COneIPInfo *sip) {
    //??? implement ipv6
}

void COneIPv6Info::fill_grat_arp_buf(uint8_t *p) {
    //??? implement ipv6
}

const COneIPInfo *CManyIPInfo::get_next() {
    COneIPInfo *ret;

    if (!m_iter_initiated) {
        m_ipv4_iter = m_ipv4_resolve.begin();
        m_iter_initiated = true;
    }

    if (m_ipv4_iter == m_ipv4_resolve.end()) {
        m_ipv4_iter = m_ipv4_resolve.begin();
        return NULL;
    }

    ret = &(m_ipv4_iter->second);
    m_ipv4_iter++;
    return ret;
}

void CManyIPInfo::dump(FILE *fd) {
    ip_vlan_to_many_ip_iter_t it;
    for (it = m_ipv4_resolve.begin(); it != m_ipv4_resolve.end(); it++) {
        fprintf(fd, "IPv4 resolved list:\n");
        uint8_t mac[ETHER_ADDR_LEN];
        it->second.get_mac(mac);
        fprintf(fd, "ip:%s vlan: %d resolved to mac %s\n", ip_to_str(it->first.get_ip()).c_str(), it->first.get_vlan()
                , utl_macaddr_to_str(mac).c_str());
    }
}

void CManyIPInfo::insert(COneIPv4Info &ip_info) {
    CIpVlan ip_vlan(ip_info.get_ip(), ip_info.get_vlan());

    m_ipv4_resolve.insert(std::make_pair(ip_vlan, ip_info));
}

bool CManyIPInfo::lookup(uint32_t ip, uint16_t vlan, MacAddress &ret_mac) {
    ip_vlan_to_many_ip_iter_t it = m_ipv4_resolve.find(CIpVlan(ip, vlan));
    if (it != m_ipv4_resolve.end()) {
        uint8_t mac[ETHER_ADDR_LEN];
        (*it).second.get_mac(mac);
        ret_mac.set(mac);
        return true;
    } else {
        return false;
    }
}

const COneIPInfo *CManyIPInfo::get_first() {
    if (m_ipv4_resolve.size() == 0) {
        return NULL;
    } else {
        m_ipv4_iter = m_ipv4_resolve.begin();
        return &(m_ipv4_iter->second);
    }
}
