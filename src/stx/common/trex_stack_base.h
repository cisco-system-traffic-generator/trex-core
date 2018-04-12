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

#ifndef __TREX_STACK_BASE_H__
#define __TREX_STACK_BASE_H__

#include <string>
#include <thread>
#include <set>
#include <unordered_map>
#include <json/json.h>
#include "trex_defs.h"
#include "mbuf.h"
#include "trex_exception.h"
#include "trex_rx_feature_api.h"

class CRXCoreIgnoreStat;

void debug(const std::string &msg);
void debug(const std::initializer_list<const std::string> &msg_list);

class CNodeBase {
public:
    CNodeBase();
    virtual ~CNodeBase();
    // setters
    void conf_dst_mac_async(const std::string &dst_mac);
    void conf_vlan_async(const vlan_list_t &vlans);
    void conf_ip4_async(const std::string &ip4_buf, const std::string &gw4_buf);
    void clear_ip4_async(void);
    void conf_ip6_async(const std::string &ip6_buf, const std::string &gw6_buf);
    void clear_ip6_async(void);

    // mark dst mac as invalid (after link down, or if IPv4 is not resolved)
    void set_dst_mac_invalid(void);
    // mark dst mac as valid
    void set_dst_mac_valid_async(void);
    // dst mac is NOT one of our ports
    void set_not_loopback(void);
    // dst mac is one of our ports
    void set_is_loopback_async(void);

    // getters
    bool is_dst_mac_valid(void);
    bool is_loopback(void);
    const std::string &get_src_mac(void);
    const std::string &get_dst_mac(void);
    const vlan_list_t &get_vlan(void);
    const std::string &get_src_ip4(void);
    const std::string &get_dst_ip4(void);
    const std::string &get_src_ip6(void);
    const std::string &get_dst_ip6(void);

    task_list_t         m_tasks;

protected:
    virtual void set_dst_mac_valid_internal(bool valid);
    virtual void set_is_loopback_internal(bool is_loopback);
    virtual void conf_dst_mac_internal(const std::string &dst_mac);
    virtual void conf_ip4_internal(const std::string &ip6_buf, const std::string &gw4_buf);
    virtual void clear_ip4_internal(void);
    virtual void conf_vlan_internal(const vlan_list_t &vlans);
    virtual void conf_ip6_internal(const std::string &ip6_buf, const std::string &gw6_buf);
    virtual void clear_ip6_internal(void);

    // binary values as it would be in packet
    bool                m_dst_mac_valid;
    bool                m_is_loopback;
    std::string         m_dst_mac;
    std::string         m_src_mac;
    vlan_list_t         m_vlan_tags;
    std::string         m_ip4;
    std::string         m_gw4;
    std::string         m_ip6;
    std::string         m_gw6;
};


class CStackBase {
public:
    enum capa_enum {
        CLIENTS     = 1,
        FAST_OPS    = 1<<1,
    };

    CStackBase(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
    virtual ~CStackBase(void)=0;

    // Get capabilities of stack
    virtual uint16_t get_capa(void)=0;

    // Handle RX packet (TRex RX port -> node)
    virtual void handle_pkt(const rte_mbuf_t *m)=0;

    // Handle TX packet (node -> TRex port)
    virtual uint16_t handle_tx(uint16_t limit)=0;

    // MAC/IP/VLAN info to JSON
    virtual void attr_to_json(Json::Value &res);

    // Grat. ARP info to JSON
    virtual void grat_to_json(Json::Value &res);

    // Not used currently, to be used for cleaning extra "client" nodes
    void reset_async(void);

    // Upon exit, cleanup of all nodes / networks / resources
    void cleanup_async(void);

    // Add node by MAC
    void add_node_async(const std::string &mac_buf);

    // Add node of port
    void add_port_node_async(void);

    // Delete node by MAC
    void del_node_async(const std::string &mac_buf);

    // Get node by MAC
    CNodeBase* get_node(const std::string &mac_buf);

    // Get port node
    CNodeBase* get_port_node(void);

    // Return true if port MAC is mac_buf
    bool has_port(const std::string &mac_buf);

    // Query certain capability
    bool has_capa(capa_enum capa);

    // Is stack in the middle of running tasks
    bool is_running_tasks(void);

    // Run the tasks with ticket to query status
    void run_pending_tasks_async(uint64_t ticket_id);

    // Get results of running tasks by ticket
    // return false if results not found (deleted by timeout)
    bool get_tasks_results(uint64_t ticket_id, stack_result_t &results);

    // busy wait for tasks to finish
    void wait_on_tasks(uint64_t ticket_id, stack_result_t &results, double timeout);

protected:
    typedef std::unordered_map<std::string,CNodeBase*> nodes_map_t;
    virtual CNodeBase* add_node_internal(const std::string &mac_buf)=0;
    virtual void del_node_internal(const std::string &mac_buf)=0;
    void run_pending_tasks_internal(uint64_t ticket_id);
    void cancel_pending_tasks(void);
    void cancel_running_tasks(void);
    CNodeBase* get_node_internal(const std::string &mac_buf);
    bool has_pending_tasks(void);

    str_set_t                   m_add_macs_list;
    str_set_t                   m_del_macs_list;
    bool                        m_is_running_tasks;
    RXFeatureAPI               *m_api;
    CRXCoreIgnoreStat          *m_ignore_stats;
    CNodeBase                  *m_port_node;
    nodes_map_t                 m_nodes;
    pthread_t                   m_thread_handle;
    stack_result_map_t          m_results;
};


class CStackFactory {
public:
    static CStackBase* create(std::string &type_name, RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
};


#endif /* __TREX_STACK_BASE_H__ */
