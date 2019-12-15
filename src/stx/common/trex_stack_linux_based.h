/*
  TRex team
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

#ifndef __TREX_STACK_LINUX_BASED_H__
#define __TREX_STACK_LINUX_BASED_H__

#include <dirent.h>
#include <vector>
#include "trex_stack_base.h"
#include "trex_vlan_filter.h"
#include "bpf_api.h"
#include <rte_spinlock.h>
#include <sys/epoll.h>
#include <unordered_set>

using namespace std;


class CNamespacedIfNode : public CNodeBase {
public:
    CNamespacedIfNode();
    virtual ~CNamespacedIfNode();

    void conf_vlan_internal(const vlan_list_t &vlans, const vlan_list_t &tpids);
    virtual void conf_ip4_internal(const string &ip4_buf, const string &gw4_buf);
    virtual void conf_shared_ns_ip4_internal(const string &ip4_buf, uint8_t subnet);
    void clear_ip4_internal();
    void clear_ip6_internal();
    virtual void conf_ip6_internal(bool enabled, const string &ip6_buf);
    virtual void conf_shared_ns_ip6_internal(bool enabled, const string &ip6_buf, uint8_t subnet);
    virtual void set_mtu_internal(const std::string &mtu);
    virtual void set_vlan_filter(uint8_t mtu);

    int  get_pair_id();
    string &get_vlans_insert_to_pkt();
    uint16_t filter_and_send(const string &pkt);
    virtual void to_json_node(Json::Value &res);

    void set_associated_trex(bool enable){
        m_associated_trex_ports = enable;
    }

    bool is_associated_trex(){
        return (m_associated_trex_ports);
    }

    void set_vlan_filter_mask(VlanFilter::FilterFlags mask) {
        m_vlan_filter.set_cfg_flag(mask);
    }

    virtual bool is_bird_node() = 0;

protected:
    void run_in_ns(const string &cmd, const string &err);
    void create_ns();
    virtual void create_veths(const string &mtu);
    void delete_net();
    void delete_veth();
    void delete_ns();
    void bind_pair();
    void set_src_mac(const string &mac_str, const string &mac_buf);
    void set_pair_id(int pair_id);
    virtual VlanFilter::FilterFlags get_default_vlan_filter_flag() = 0;

    bool            m_associated_trex_ports; /* associated with TRex physical port*/
    bool            m_is_shared_ns;
    int             m_pair_id;
    string          m_ns_name;
    string          m_if_name;
    bpf_h           m_bpf;
    string          m_vlans_insert_to_pkt;
    VlanFilter      m_vlan_filter;
public:
    struct epoll_event m_event;
};

class CLinuxIfNode : public CNamespacedIfNode {
public:
    CLinuxIfNode(const string &ns_name, const string &mac_str, const string &mac_buf, const string &mtu);
    virtual ~CLinuxIfNode();
    void to_json_node(Json::Value &res);
    virtual void conf_ip4_internal(const string &ip4_buf, const string &gw4_buf);
    virtual void conf_ip6_internal(bool enabled, const string &ip6_buf);
    virtual bool is_bird_node() { return false; }

private:
    virtual VlanFilter::FilterFlags get_default_vlan_filter_flag();
};

class CSharedNSIfNode : public CNamespacedIfNode {
public:
    CSharedNSIfNode(const string &ns_name, const string &if_name, const string &mac_str, const string &mac_buf,
                 const string &mtu, bool is_bird);
    virtual ~CSharedNSIfNode();
    void to_json_node(Json::Value &res);
    virtual void conf_shared_ns_ip4_internal(const string &ip4_buf, uint8_t subnet);
    virtual void conf_shared_ns_ip6_internal(bool enabled, const string &ip6_buf, uint8_t subnet);
    virtual void set_mtu_internal(const std::string &mtu);
    virtual bool is_bird_node() { return m_is_bird; };

private:
    void create_veths(const string &mtu);
    virtual VlanFilter::FilterFlags get_default_vlan_filter_flag();
    bool            m_is_bird;
    uint8_t         m_subnet4;
    uint8_t         m_subnet6;
};

class CStackLinuxBased : public CStackBase {
public:
    CStackLinuxBased(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
    ~CStackLinuxBased();

    uint16_t get_capa();

    // TRex port -> node
    void handle_pkt(const rte_mbuf_t *m);

    // node -> TRex port
    uint16_t handle_tx(uint16_t limit);
    
    static string m_bird_ns;


public:
    virtual void rpc_help(const std::string & mac,const std::string & p1,const std::string & p2);

    virtual trex_rpc_cmd_rc_e rpc_add_node(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_add_shared_ns(Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_add_shared_ns_node(const std::string & mac, bool is_bird, const string &shared_ns);
    virtual trex_rpc_cmd_rc_e rpc_remove_node(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_remove_shared_ns(const std::string & shared_ns);
    virtual trex_rpc_cmd_rc_e rpc_set_vlans(const std::string & mac, const vlan_list_t &vlan_list, const vlan_list_t &tpid_list);
    virtual trex_rpc_cmd_rc_e rpc_set_vlan_filter(const std::string &mac, uint8_t mask);
    virtual trex_rpc_cmd_rc_e rpc_set_dg(const std::string & shared_ns, const std::string &dg);
    virtual trex_rpc_cmd_rc_e rpc_set_mtu(const std::string &mac, const std::string &mtu);
    virtual trex_rpc_cmd_rc_e rpc_set_ipv4(const std::string &mac, std::string ip4_buf, std::string gw4_buf);
    virtual trex_rpc_cmd_rc_e rpc_set_shared_ns_ipv4(const std::string &mac, const std::string &ip4_buf, uint8_t subnet);
    virtual trex_rpc_cmd_rc_e rpc_clear_ipv4(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_set_ipv6(const std::string & mac, bool enable, std::string src_ipv6_buf);
    virtual trex_rpc_cmd_rc_e rpc_set_shared_ns_ipv6(const std::string &mac, bool enable, std::string src_ipv6_buf, uint8_t subnet);
    virtual trex_rpc_cmd_rc_e rpc_remove_all();
    virtual trex_rpc_cmd_rc_e rpc_get_nodes(Json::Value &result, bool only_bird = false);
    virtual trex_rpc_cmd_rc_e rpc_get_nodes_info(const Json::Value &params,Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_clear_counters();
    virtual trex_rpc_cmd_rc_e rpc_counters_get_meta(const Json::Value &params, Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_counters_get_value(bool zeros, Json::Value &result);

private:
    string mac_str_to_mac_buf(const std::string & mac);
    void init_bird();

private:
    CNodeBase* add_node_internal(const string &mac_buf);
    CNodeBase* add_shared_ns_node_internal(const string &mac_buf, bool is_bird, const string &shared_ns);
    CNodeBase *add_linux_events_and_node(const string &mac_buf, const string &mac_str, CNamespacedIfNode *node);
    void del_node_internal(const std::string &mac_buf);
    void del_shared_ns_internal(const std::string &shared_ns);
    CNamespacedIfNode * get_node_by_mac(const std::string &mac);
    CNamespacedIfNode * get_node_rpc(const std::string &mac);
    const std::string remove_all_internal();
    void create_bird_ns();
    void run_bird_in_ns();
    void kill_bird_ns();
    const string get_bird_path();

    int                     m_epoll_fd;
    static string           m_mtu;
    static string           m_ns_prefix;
    static string           m_shared_ns_prefix;
    static bool             m_is_initialized;
    uint64_t                m_next_namespace_id;
    uint64_t                m_next_bird_if_id;
    uint64_t                m_next_shared_ns_id; 
    uint64_t                m_next_shared_ns_if_id;
    unordered_set <string>  m_shared_ns_names;
    string                  m_bird_path;
    char                    m_rw_buf[MAX_PKT_ALIGN_BUF_9K];
    rte_spinlock_t          m_main_loop; /* protect main loop in case of add/remove ns*/
};


#endif /* __TREX_STACK_LINUX_BASED_H__ */
