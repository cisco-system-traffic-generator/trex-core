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
#include "bpf_api.h"
#include <rte_spinlock.h>
#include <sys/epoll.h>

using namespace std;

class CLinuxIfNode : public CNodeBase {
public:
    CLinuxIfNode(const string &ns_name, const string &mac_str, const string &mac_buf, const string &mtu);
    ~CLinuxIfNode();

    void conf_vlan_internal(const vlan_list_t &vlans);
    void conf_ip4_internal(const string &ip4_buf, const string &gw4_buf);
    void clear_ip4_internal(void);
    void conf_ip6_internal(bool enabled, const string &ip6_buf);
    void clear_ip6_internal(void);
    int  get_pair_id(void);
    string &get_vlans_insert_to_pkt(void);
    uint16_t filter_and_send(const rte_mbuf_t *m);

    void set_associated_trex(bool enable){
        m_associated_trex_ports = enable;
    }

    bool is_associated_trex(void){
        return (m_associated_trex_ports);
    }

private:
    void run_in_ns(const string &cmd, const string &err);
    void create_net(const string &mtu);
    void delete_net(void);
    void bind_pair(void);
    void set_src_mac(const string &mac_str, const string &mac_buf);
    void set_pair_id(int pair_id);

    bool            m_associated_trex_ports; /* associated with TRex physical port*/
    int             m_pair_id;
    string          m_ns_name;
    bpf_h           m_bpf;
    string          m_vlans_insert_to_pkt;
public:
    struct epoll_event m_event;
};


class CStackLinuxBased : public CStackBase {
public:
    CStackLinuxBased(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
    ~CStackLinuxBased(void);

    uint16_t get_capa(void);

    // TRex port -> node
    void handle_pkt(const rte_mbuf_t *m);

    // node -> TRex port
    uint16_t handle_tx(uint16_t limit);


public:
    virtual void rpc_help(const std::string & mac,const std::string & p1,const std::string & p2);

    virtual trex_rpc_cmd_rc_e rpc_add_node(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_remove_node(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_set_vlans(const std::string & mac,vlan_list_t vlan_list);
    virtual trex_rpc_cmd_rc_e rpc_set_ipv4(const std::string & mac,std::string ip4_buf,std::string gw4_buf);
    virtual trex_rpc_cmd_rc_e rpc_clear_ipv4(const std::string & mac);
    virtual trex_rpc_cmd_rc_e rpc_set_ipv6(const std::string & mac,bool enable, std::string src_ipv6_buf);
    virtual trex_rpc_cmd_rc_e rpc_remove_all(void);
    virtual trex_rpc_cmd_rc_e rpc_get_nodes(Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_get_nodes_info(const Json::Value &params,Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_clear_counters(void);
    virtual trex_rpc_cmd_rc_e rpc_counters_get_meta(const Json::Value &params, Json::Value &result);
    virtual trex_rpc_cmd_rc_e rpc_counters_get_value(bool zeros, Json::Value &result);

private:
    string mac_str_to_mac_buf(const std::string & mac);

private:
    CNodeBase* add_node_internal(const std::string &mac_buf);
    void del_node_internal(const std::string &mac_buf);
    CLinuxIfNode * get_node_by_mac(const std::string &mac);
    CLinuxIfNode * get_node_rpc(const std::string &mac);

        
    int                 m_epoll_fd;
    static string       m_mtu;
    static string       m_ns_prefix;
    static bool         m_is_initialized;
    uint64_t            m_next_namespace_id;
    char                m_rw_buf[MAX_PKT_ALIGN_BUF_9K];
    rte_spinlock_t      m_main_loop; /* protect main loop in case of add/remove ns*/
};


#endif /* __TREX_STACK_LINUX_BASED_H__ */
