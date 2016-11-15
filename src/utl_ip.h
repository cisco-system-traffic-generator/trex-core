#ifndef UTL_IP_H
#define UTL_IP_H
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
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common/basic_utils.h"
#include "common/Network/Packet/CPktCmn.h"
#include "common/Network/Packet/MacAddress.h"

/* IP address, last 32-bits of IPv6 remaps IPv4 */
typedef struct {
    uint16_t v6[6];  /* First 96-bits of IPv6 */
    uint32_t v4;  /* Last 32-bits IPv6 overloads v4 */
} ipaddr_t;

// Routine to create IPv4 address string
inline int ip_to_str(uint32_t ip,char * str) {
    uint32_t ipv4 = PKT_HTONL(ip);
    inet_ntop(AF_INET, (const char *)&ipv4, str, INET_ADDRSTRLEN);
    return(strlen(str));
}

inline std::string ip_to_str(uint32_t ip) {
    char tmp[INET_ADDRSTRLEN];
    ip_to_str(ip, tmp);
    return tmp;
}

// Routine to create IPv6 address string
inline int ipv6_to_str(ipaddr_t *ip, char * str) {
    int idx=0;
    uint16_t ipv6[8];
    for (uint8_t i=0; i<6; i++) {
        ipv6[i] = PKT_HTONS(ip->v6[i]);
    }
    uint32_t ipv4 = PKT_HTONL(ip->v4);
    ipv6[6] = ipv4 & 0xffff;
    ipv6[7] = ipv4 >> 16;

    str[idx++] = '[';
    inet_ntop(AF_INET6, (const char *)&ipv6, &str[1], INET6_ADDRSTRLEN);
    idx = strlen(str);
    str[idx++] = ']';
    str[idx] = 0;
    return(idx);
}

inline std::string ip_to_str(uint8_t *ip) {
    char tmp[INET6_ADDRSTRLEN];
    ipv6_to_str((ipaddr_t *)ip, tmp);
    return tmp;
}

class COneIPInfo {
 public:
    enum {
        IP4_VER=4,
        IP6_VER=6,
    } COneIPInfo_ip_types;

 public:
    virtual void get_mac(uint8_t *mac) const {
        m_mac.copyToArray(mac);
    }
    virtual void set_mac(uint8_t *mac) {
        m_mac.set(mac);
    }
    uint16_t get_vlan() const {return m_vlan;}
    uint16_t get_port() const {return m_port;}
    virtual void dump(FILE *fd) const {
        dump(fd, "");
    }    
    virtual void dump(FILE *fd, const char *offset) const;
    virtual uint8_t ip_ver() const {return 0;}
    virtual uint32_t get_arp_req_len() const=0;
    virtual uint32_t get_grat_arp_len() const=0;
    virtual void fill_arp_req_buf(uint8_t *p, uint16_t port_id, COneIPInfo *sip)=0;
    virtual void fill_grat_arp_buf(uint8_t *p)=0;
    virtual bool resolve_needed() const;

 protected:
    COneIPInfo(uint16_t vlan, MacAddress mac, uint8_t port) : m_mac(mac) {
        m_vlan = vlan;
        m_port = port;
    }
    COneIPInfo(uint16_t vlan, MacAddress mac) : COneIPInfo(vlan, mac, UINT8_MAX) {
    }
    virtual const void get_ip_str(char str[100]) const {
        snprintf(str, 4, "Bad");
    }

 protected:
    uint8_t    m_port;
    uint16_t   m_vlan;
    MacAddress m_mac;
};

class COneIPv4Info : public COneIPInfo {
    friend bool operator== (const COneIPv4Info& lhs, const COneIPv4Info& rhs);

 public:
    COneIPv4Info(uint32_t ip, uint16_t vlan, MacAddress mac) : COneIPInfo(vlan, mac) {
        m_ip = ip;
    }
    COneIPv4Info(uint32_t ip, uint16_t vlan) : COneIPv4Info (ip, vlan, MacAddress()) {
    }
    COneIPv4Info(uint32_t ip, uint16_t vlan, MacAddress mac, uint8_t port) : COneIPInfo(vlan, mac, port) {
        m_ip = ip;
    }
    uint32_t get_ip() {return m_ip;}
    virtual uint8_t ip_ver() const {return IP4_VER;}
    virtual uint32_t get_arp_req_len() const {return 60;}
    virtual uint32_t get_grat_arp_len() const {return 60;}
    virtual void fill_arp_req_buf(uint8_t *p, uint16_t port_id, COneIPInfo *sip);
    virtual void fill_grat_arp_buf(uint8_t *p);

 private:
    virtual const void get_ip_str(char str[100]) const {
        ip_to_str(m_ip, str);
    };
    uint32_t m_ip;
};

inline bool operator== (const COneIPv4Info& lhs, const COneIPv4Info& rhs) {
    if (lhs.m_vlan != rhs.m_vlan)
        return false;

    if (lhs.m_ip != rhs.m_ip)
        return false;

    return true;
}

inline bool operator!= (const COneIPv4Info& lhs, const COneIPv4Info& rhs){ return !(lhs == rhs); }

class COneIPv6Info : public COneIPInfo {
    friend bool operator== (const COneIPv6Info& lhs, const COneIPv6Info& rhs);

 public:
    COneIPv6Info(uint16_t ip[8], uint16_t vlan, MacAddress mac) : COneIPInfo(vlan, mac) {
        memcpy(m_ip, ip, sizeof(m_ip));
    }

    COneIPv6Info(uint16_t ip[8], uint16_t vlan) : COneIPv6Info(ip, vlan, MacAddress()){
    }

    COneIPv6Info(uint16_t ip[8], uint16_t vlan, MacAddress mac, uint8_t port) : COneIPInfo(vlan, mac, port) {
        memcpy(m_ip, ip, sizeof(m_ip));
    }
    
    const uint8_t *get_ipv6() {return (uint8_t *)m_ip;}
    virtual uint8_t ip_ver() const {return IP6_VER;}
    virtual uint32_t get_arp_req_len() const {return 100; /* ??? put correct number for ipv6*/}
    virtual uint32_t get_grat_arp_len() const {return 100; /* ??? put correct number for ipv6*/}
    virtual void fill_arp_req_buf(uint8_t *p, uint16_t port_id, COneIPInfo *sip);
    virtual void fill_grat_arp_buf(uint8_t *p);

 private:
    virtual const void get_ip_str(char str[100]) {
        ipv6_to_str((ipaddr_t *)m_ip, str);
    }
    uint16_t m_ip[8];
};

inline bool operator== (const COneIPv6Info& lhs, const COneIPv6Info& rhs) {
    if (lhs.m_vlan != rhs.m_vlan)
        return false;

    if (memcmp(&lhs.m_ip, &rhs.m_ip, sizeof(rhs.m_ip)))
        return false;

    return true;
}

inline bool operator!= (const COneIPv6Info& lhs, const COneIPv6Info& rhs){ return !(lhs == rhs); }

class CManyIPInfo {
 public:
    CManyIPInfo () {
        m_iter_initiated = false;
    }
    void insert(COneIPv4Info &ip_info);
    bool lookup(uint32_t ip, uint16_t vlan, MacAddress &ret_mac);
    void dump(FILE *fd);
    uint32_t size() { return m_ipv4_resolve.size() + m_ipv6_resolve.size();}
    const COneIPInfo *get_first();
    const COneIPInfo *get_next();
 private:
    std::map<std::pair<uint32_t, uint16_t>, COneIPv4Info> m_ipv4_resolve;
    std::map<std::pair<uint16_t[8], uint16_t>, COneIPv6Info> m_ipv6_resolve;
    std::map<std::pair<uint32_t, uint16_t>, COneIPv4Info>::iterator m_ipv4_iter;
    bool m_iter_initiated;
    
};

#endif
