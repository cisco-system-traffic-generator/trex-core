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
#include "trex_rx_rpc_tunnel.h"
#include "rte_atomic.h"
#include "trex_stack_rc.h"
#include "trex_stack_counters.h"

class CRXCoreIgnoreStat;

void debug(const std::string &msg);
void debug(const std::initializer_list<const std::string> &msg_list);

class CNodeBase  {
public:
    CNodeBase();
    virtual ~CNodeBase();
    // setters
    void conf_dst_mac_async(const std::string &dst_mac);
    void conf_vlan_async(const vlan_list_t &vlans);
    void conf_ip4_async(const std::string &ip4_buf, const std::string &gw4_buf);
    void clear_ip4_async();
    void conf_ip6_async(bool enabled, const std::string &ip6_buf);
    void clear_ip6_async();

    void set_l2_mode(bool enable){
        m_l2_mode =  enable;
    }
    bool get_l2_mode(){
        return (m_l2_mode);
    }

    // mark dst mac as invalid (after link down, or if IPv4 is not resolved)
    void set_dst_mac_invalid();
    // mark dst mac as valid
    void set_dst_mac_valid_async();
    // dst mac is NOT one of our ports
    void set_not_loopback();
    // dst mac is one of our ports
    void set_is_loopback_async();

    // getters
    bool is_dst_mac_valid();
    bool is_loopback();
    bool is_ip6_enabled();
    /* return mac_buf and NOT mac string */
    const std::string &get_src_mac();

    /* return src_mac as string xx:xx:xx:xx:xx  for RPC */
    std::string get_src_mac_as_str();

    const std::string &get_dst_mac();
    const vlan_list_t &get_vlan_tags();
    const vlan_list_t &get_vlan_tpids();
    const std::string &get_src_ip4();
    const std::string &get_dst_ip4();
    const std::string &get_src_ip6();

    task_list_t         m_tasks;

    string mac_str_to_mac_buf(const std::string & mac);

    virtual void to_json(Json::Value &res);

    virtual void to_json_node(Json::Value &res);

protected:
    virtual void set_dst_mac_valid_internal(bool valid);
    virtual void set_is_loopback_internal(bool is_loopback);
    virtual void conf_dst_mac_internal(const std::string &dst_mac);
    virtual void conf_ip4_internal(const std::string &ip6_buf, const std::string &gw4_buf);
    virtual void clear_ip4_internal();
    virtual void conf_vlan_internal(const vlan_list_t &vlans, const vlan_list_t &tpids);
    virtual void conf_ip6_internal(bool enabled, const std::string &ip6_buf);
    virtual void clear_ip6_internal();

    // binary values as it would be in packet
    bool                m_dst_mac_valid;
    bool                m_is_loopback;
    std::string         m_dst_mac;
    std::string         m_src_mac;
    vlan_list_t         m_vlan_tags;
    vlan_list_t         m_vlan_tpids;
    std::string         m_ip4;
    std::string         m_gw4;
    std::string         m_ip6;
    bool                m_l2_mode;
    bool                m_ip6_enabled;
};


class CStackBase;

class CRpcTunnelCStackBase : public CRpcTunnelBatch  {

public:
    void init(CStackBase * obj);

    trex_rpc_cmd_rc_e rpc_add_node(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_add_shared_ns(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_remove_node(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_remove_shared_ns(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_vlans(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_ipv4(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_filter(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_dg(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_mtu(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_clear_ipv4(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_set_ipv6(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_remove_all(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_get_nodes(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_get_nodes_info(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_get_commands_list(const Json::Value &params, Json::Value &result);

    trex_rpc_cmd_rc_e rpc_clear_counters(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_counters_get_meta(const Json::Value &params, Json::Value &result);
    trex_rpc_cmd_rc_e rpc_counters_get_value(const Json::Value &params, Json::Value &result);


    /* debug commands */
    trex_rpc_cmd_rc_e rpc_help(const Json::Value &params, Json::Value &result);

    void register_rpc_functions();

protected:
    virtual void update_cmd_count(uint32_t total_exec_commands,
                                  uint32_t err_exec_commands); 

private:
  CStackBase * m_obj;
};


class CStackBase {
public:
    enum capa_enum {
        CLIENTS     = 1,
        FAST_OPS    = 1 << 1,
        BIRD        = 1 << 2,
    };

    CStackBase(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
    virtual ~CStackBase()=0;

    // Get capabilities of stack
    virtual uint16_t get_capa()=0;

    // Handle RX packet (TRex RX port -> node)
    virtual void handle_pkt(const rte_mbuf_t *m)=0;

    // Handle TX packet (node -> TRex port)
    virtual uint16_t handle_tx(uint16_t limit)=0;

    // MAC/IP/VLAN info to JSON
    virtual void attr_to_json(Json::Value &res);

    // Grat. ARP info to JSON
    virtual void grat_to_json(Json::Value &res);

    // Not used currently, to be used for cleaning extra "client" nodes
    void reset_async();

    // Upon exit, cleanup of all nodes / networks / resources
    void cleanup_async();

    // Add node by MAC
    void add_node_async(const std::string &mac_buf);

    // Add node of port
    void add_port_node_async();

    // Delete node by MAC
    void del_node_async(const std::string &mac_buf);

    // Get node by MAC
    CNodeBase* get_node(const std::string &mac_buf);

    // Get port node
    CNodeBase* get_port_node();

    // Return true if port MAC is mac_buf
    bool has_port(const std::string &mac_buf);

    // Query certain capability
    bool has_capa(capa_enum capa);

    // Is stack in the middle of running tasks
    bool is_running_tasks();

    // Run the tasks with ticket to query status
    void run_pending_tasks_async(uint64_t ticket_id,bool rpc);

    // Get results of running tasks by ticket
    // return false if results not found (deleted by timeout)
    bool get_tasks_results(uint64_t ticket_id, stack_result_t &results);

    // busy wait for tasks to finish
    void wait_on_tasks(uint64_t ticket_id, stack_result_t &results, double timeout);

    void cancel_pending_tasks();
    void cancel_running_tasks();
    CRxCounters &get_counters() { return m_counters; }
public:
    virtual void dummy_rpc_command(string ipv4,string ipv4_dg);
    virtual void rpc_help(const std::string & mac,const std::string & p1,const std::string & p2);

/***************/
/* RPC commands */
/***************/
    void throw_not_supported(){
        throw TrexRpcException(" not supported with this stack ");
    }

    virtual trex_rpc_cmd_rc_e rpc_add_node(const std::string & mac){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_add_shared_ns(Json::Value &result){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_add_shared_ns_node(const std::string & mac, bool is_bird, const string &shared_ns){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_remove_node(const std::string & mac){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_remove_shared_ns(const std::string & shared_ns){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_filter(const std::string & mac, const std::string &filter) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);

    }
    
    virtual trex_rpc_cmd_rc_e rpc_set_dg(const std::string & shared_ns, const std::string &dg) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_mtu(const std::string & mac, const std::string &mtu) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_vlans(const std::string & mac, const vlan_list_t &vlan_list, const vlan_list_t &tpid_list) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_ipv4(const std::string &mac, std::string ip4_buf, std::string gw4_buf) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_shared_ns_ipv4(const std::string & mac, const std::string &ip4_buf, uint8_t subnet) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_clear_ipv4(const std::string & mac){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_ipv6(const std::string & mac,bool enable, std::string src_ipv6_buf){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_set_shared_ns_ipv6(const std::string & mac,bool enable, std::string src_ipv6_buf, uint8_t subnet) {
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_remove_all(){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_get_nodes(Json::Value &result){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_get_nodes_info(const Json::Value &params,Json::Value &result){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_clear_counters(){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_counters_get_meta(const Json::Value &params, Json::Value &result){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }

    virtual trex_rpc_cmd_rc_e rpc_counters_get_value(bool zeros, Json::Value &result){
        throw_not_supported();
        return(TREX_RPC_CMD_INTERNAL_ERR);
    }


/***************/
/* TBD need to add get_nodes etc */


    /* run batch of json commands  in async mode */
    void conf_name_space_batch_async(const std::string &json);

    void get_rpc_cmds(TrexStackResultsRC & rc);

    void update_rcp_cmds_count(uint32_t total_commands,
                               uint32_t err_commands);

protected:
    typedef std::unordered_map<std::string,CNodeBase*> nodes_map_t;
    virtual CNodeBase* add_node_internal(const std::string &mac_buf)=0;
    virtual void del_node_internal(const std::string &mac_buf)=0;
    void run_pending_tasks_internal(uint64_t ticket_id);
    void run_pending_tasks_internal_rpc(uint64_t ticket_id);

    CNodeBase* get_node_internal(const std::string &mac_buf);
    bool has_pending_tasks();

private:
    void _finish_rpc(uint64_t ticket_id);

protected:
    

    str_set_t                   m_add_macs_list;
    str_set_t                   m_del_macs_list;
    volatile bool               m_is_running_tasks;
    RXFeatureAPI               *m_api;
    CRXCoreIgnoreStat          *m_ignore_stats;
    CNodeBase                  *m_port_node;
    nodes_map_t                 m_nodes;
    pthread_t                   m_thread_handle;
    stack_result_map_t          m_results;
    CRpcTunnelCStackBase        m_rpc_tunnel;
    std::string                 m_rpc_commands;
    uint32_t                    m_total_cmds;
    rte_atomic32_t              m_exec_cmds;
    rte_atomic32_t              m_exec_cmds_err;
    CRxCounters                 m_counters;

};


class CStackFactory {
public:
    static CStackBase* create(std::string &type_name, RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats);
};


#endif /* __TREX_STACK_BASE_H__ */
