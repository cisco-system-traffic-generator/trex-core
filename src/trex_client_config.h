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
#ifndef __TREX_CLIENT_CONFIG_H__
#define __TREX_CLIENT_CONFIG_H__

#include <stdint.h>
#include <string>
#include <map>
#include "utl_ip.h"
#include "common/Network/Packet/MacAddress.h"

class YAMLParserWrapper;
struct CTupleGenYamlInfo;
class ClientCfgDirExt;

// To save memory, we use here the ClientCfgExt and ClientCfgDirExt,
// and in tuple_gen the ClientCfgBase and ClientCfgDirBase
/**
 * client configuration per direction
 *
 * @author imarom (29-Jun-16)
 */
class ClientCfgDirBase {
    friend class ClientCfgCompactEntry;

 protected:
    enum {
        HAS_SRC_MAC       = 0x1,
        HAS_DST_MAC       = 0x2,
        HAS_VLAN          = 0x4,
        HAS_NEXT_HOP      = 0x8,
        HAS_IPV6_NEXT_HOP = 0x10,
        HAS_SRC_IP        = 0x20,
        HAS_SRC_IPV6      = 0x40,
    };

    MacAddress  m_src_mac;
    MacAddress  m_dst_mac;
    uint16_t    m_vlan;
    uint8_t     m_bitfield;

 public:
    ClientCfgDirBase() {
        m_bitfield = 0;
    }

    virtual void dump(FILE *fd) const;
    bool has_src_mac_addr() const {
        return (m_bitfield & HAS_SRC_MAC);
    }

    void set_src_mac_addr(uint64_t mac_addr) {
        m_src_mac.set(mac_addr);
        m_bitfield |= HAS_SRC_MAC;
    }

    void set_dst_mac_addr(uint64_t mac_addr) {
        m_dst_mac.set(mac_addr);
        m_bitfield |= HAS_DST_MAC;
    }

    void set_vlan(uint16_t vlan_id) {
        m_vlan      = vlan_id;
        m_bitfield |= HAS_VLAN;
    }

    bool has_dst_mac_addr() const {
        return (m_bitfield & HAS_DST_MAC);
    }
    bool has_vlan() const {
        return (m_bitfield & HAS_VLAN);
    }

    const uint8_t *get_src_mac_addr() const {
        assert(has_src_mac_addr());
        return m_src_mac.GetConstBuffer();
    }

    const uint8_t *get_dst_mac_addr() const {
        assert(has_dst_mac_addr());
        return m_dst_mac.GetConstBuffer();
    }

    uint16_t get_vlan() const {
        assert(has_vlan());
        return m_vlan;
    }

    /* updates a configuration with a group index member */
    void update(uint32_t index, const ClientCfgDirExt &cfg);
};

class ClientCfgDirExt : public ClientCfgDirBase {
    friend class ClientCfgCompactEntry;

private:
    enum {
        HAS_SRC_MAC       = 0x1,
        HAS_DST_MAC       = 0x2,
        HAS_VLAN          = 0x4,
        HAS_NEXT_HOP      = 0x8,
        HAS_IPV6_NEXT_HOP = 0x10,
        HAS_SRC_IP        = 0x20,
        HAS_SRC_IPV6      = 0x40,
    };

    uint32_t    m_next_hop;
    uint32_t    m_src_ip;
    uint16_t    m_src_ipv6[8];
    uint16_t    m_ipv6_next_hop[8];
    std::vector <MacAddress> m_resolved_macs;

public:
    void dump(FILE *fd) const;
    void set_resolved_macs(CManyIPInfo &pretest_result, uint16_t count);
    bool need_resolve() const;
    void set_no_resolve_needed();

    bool has_ipv6_next_hop() const {
        return (m_bitfield & HAS_IPV6_NEXT_HOP);
    }

    bool has_next_hop() const {
        return (m_bitfield & HAS_NEXT_HOP);
    }

    bool has_src_ip() const {
        return (m_bitfield & HAS_SRC_IP);
    }

    bool has_src_ipv6() const {
        return (m_bitfield & HAS_SRC_IPV6);
    }

    void set_src_ip(uint32_t src_ip) {
        m_src_ip = src_ip;
        m_bitfield |= HAS_SRC_IP;
    }

    void set_src_ipv6(const uint16_t src_ipv6[8]) {
        for (int i = 0; i < 8; i++) {
            m_src_ipv6[i] = src_ipv6[i];
        }
        m_bitfield |= HAS_SRC_IPV6;
    }

    void set_next_hop(uint32_t next_hop) {
        m_next_hop  = next_hop;
        m_bitfield |= HAS_NEXT_HOP;
    }

    void set_ipv6_next_hop(const uint16_t next_hop[8]) {
        for (int i = 0; i < 8; i++) {
            m_ipv6_next_hop[i] = next_hop[i];
        }
        m_bitfield |= HAS_IPV6_NEXT_HOP;
    }

    virtual MacAddress get_resolved_mac(uint16_t index) const {
        return m_resolved_macs[index];
    }

};

class ClientCfgExt;

/**
 * single client config
 *
 */
class ClientCfgBase {

public:
    virtual void dump (FILE *fd) const {
        fprintf(fd, "initiator:\n");
        m_initiator.dump(fd);
        fprintf(fd, "responder:\n");
        m_responder.dump(fd);
    }
    virtual void update(uint32_t index, const ClientCfgExt *cfg);

 public:
    ClientCfgDirBase m_initiator;
    ClientCfgDirBase m_responder;
};

class ClientCfgExt : public ClientCfgBase {
public:
    virtual void dump (FILE *fd) const {
        fprintf(fd, "initiator:\n");
        m_initiator.dump(fd);
        fprintf(fd, "responder:\n");
        m_responder.dump(fd);
    }

    ClientCfgDirExt m_initiator;
    ClientCfgDirExt m_responder;
};

class ClientCfgCompactEntry {
    friend class ClientCfgDB;
 public:
    uint16_t get_count() {return m_count;}
    uint16_t get_vlan() {return m_vlan;}
    uint16_t get_port() {return m_port;}
    bool is_ipv4() {return m_is_ipv4;}
    uint32_t get_dst_ip() {return m_next_hop_base.ip;}
    uint16_t *get_dst_ipv6() {return m_next_hop_base.ipv6;}
    uint32_t get_src_ip() {return m_src_ip.ip;}
    uint16_t *get_src_ipv6() {return m_src_ip.ipv6;}

 public:
    void fill_from_dir(ClientCfgDirExt cfg, uint8_t port_id);

 private:
    uint16_t m_count;
    uint16_t m_vlan;
    uint8_t m_port;
    bool m_is_ipv4;
    union {
        uint32_t ip;
        uint16_t ipv6[8];
    } m_next_hop_base;
    union {
        uint32_t ip;
        uint16_t ipv6[8];
    } m_src_ip;

};

/******************************** internal section ********************************/

/**
 * describes a single client config
 * entry loaded from the config file
 *
 */
class ClientCfgEntry {

public:

    ClientCfgEntry() {
        reset();
    }

    void dump(FILE *fd) const;
    void set_resolved_macs(CManyIPInfo &pretest_result);
    bool contains(uint32_t ip) const {
        return ( (ip >= m_ip_start) && (ip <= m_ip_end) );
    }

    void reset() {
        m_iterator = 0;
    }


    /**
     * assings a client config from the group
     * it will advance MAC addresses andf etc.
     *
     * @author imarom (27-Jun-16)
     *
     * @param info
     */
    void assign(ClientCfgBase &info) {
        info = m_cfg;
        info.update(m_iterator, &m_cfg);

        /* advance for the next assign */
        m_iterator = (m_iterator + 1) % m_count;
    }

public:
    uint32_t    m_ip_start;
    uint32_t    m_ip_end;

    ClientCfgExt m_cfg;

    uint32_t    m_count;

private:
    uint32_t    m_iterator;
};

/**
 * holds all the configured clients groups
 *
 */
class ClientCfgDB {
public:

    ClientCfgDB() {
        m_is_empty    = true;
        m_cache_group = NULL;
        m_under_vlan  = false;
        m_tg = NULL;
    }

    void dump(FILE *fd) ;

    /**
     * if no config file was loaded
     * this should return true
     *
     */
    bool is_empty() {
        return m_is_empty;
    }

    void set_resolved_macs(CManyIPInfo &pretest_result);
    std::vector<ClientCfgCompactEntry *> get_entry_list();

    /**
     * loads a YAML file
     * configuration will be built
     * according to the YAML config
     *
     */
    void load_yaml_file(const std::string &filename);

    /**
     * lookup for a specific IP address for
     * a group that contains this IP
     *
     */
    ClientCfgEntry * lookup(uint32_t ip);
    ClientCfgEntry * lookup(const std::string &ip);
    void set_tuple_gen_info(CTupleGenYamlInfo *tg) {m_tg = tg;}

private:
    void parse_single_group(YAMLParserWrapper &parser, const YAML::Node &node);
    void parse_dir(YAMLParserWrapper &parser, const YAML::Node &node, ClientCfgDirExt &dir);

    /**
     * verify the YAML file loaded in valid
     *
     */
    void verify(const YAMLParserWrapper &parser) const;

    /* maps the IP start value to client groups */
    std::map<uint32_t, ClientCfgEntry>  m_groups;
    bool m_under_vlan;
    CTupleGenYamlInfo * m_tg;
    ClientCfgEntry * m_cache_group;
    bool m_is_empty;
};

#endif /* __TREX_CLIENT_CONFIG_H__ */
