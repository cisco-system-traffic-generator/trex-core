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

#include "bp_sim.h"
#include "trex_stack_base.h"
#include "trex_stack_legacy.h"
#include "trex_stack_linux_based.h"

#include <iostream>
#include <functional>
#include <pthread.h>


/*************************************************
*             Tunnel JSON-RPC for abstract Stack *
**************************************************/


void CRpcTunnelCStackBase::init(CStackBase * obj){
    m_obj = obj;
}


trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_add_node(const Json::Value &params, Json::Value &result){
    debug({"rpc_add_node",pretty_json_str(params)});
    string mac       = parse_string(params, "mac", result);
    bool is_bird     = parse_bool(params, "is_bird", result, false);
    string shared_ns = parse_string(params, "shared_ns", result, "");

    if ( is_bird && !shared_ns.empty() ) {
        throw TrexException("Cannot add node with bird option and shared namespace!");
    }

    if ( is_bird || !shared_ns.empty() ) {
        return (m_obj->rpc_add_shared_ns_node(mac, is_bird, shared_ns));
    } else {
        return (m_obj->rpc_add_node(mac));
    }
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_add_shared_ns(const Json::Value &params, Json::Value &result){
    debug({"rpc_add_shared_ns",pretty_json_str(params)});
    return (m_obj->rpc_add_shared_ns(result));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_remove_node(const Json::Value &params, Json::Value &result){
    debug({"rpc_remove_node",pretty_json_str(params)});
    string mac   = parse_string(params, "mac", result);
    return (m_obj->rpc_remove_node(mac));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_remove_shared_ns(const Json::Value &params, Json::Value &result){
    debug({"rpc_remove_shared_ns", pretty_json_str(params)});
    string shared_ns = parse_string(params, "shared_ns", result);

    return m_obj->rpc_remove_shared_ns(shared_ns);
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_vlans(const Json::Value &params, Json::Value &result){
    debug({"rpc_set_vlans",pretty_json_str(params)});
    string mac   = parse_string(params, "mac", result);
    const Json::Value &vlans = parse_array(params, "vlans", result);

    if ( vlans.size() > 2 ) {
        generate_parse_err(result, "Maximal number of stacked VLANs is 2, got: " + to_string(vlans.size()));
    }
    vlan_list_t vlan_list;

    for (int i=0; i<vlans.size(); i++) {
        uint16_t vlan = parse_uint16(vlans, i, result);
        if ( (vlan == 0) || (vlan > 4095) ) {
            generate_parse_err(result, "invalid VLAN tag: '" + to_string(vlan) + "'");
        }
        vlan_list.push_back(vlan);
    }

    vlan_list_t tpid_list;

    if ( params["tpids"].isArray() ) {
        const Json::Value &tpids = parse_array(params, "tpids", result);
        if ( tpids.size() != vlans.size() ) {
            generate_parse_err(result, "mismatch between size of vlan tags and pgids");
        }
        for (int i=0; i<tpids.size(); i++) {
            uint16_t tpid = parse_uint16(tpids, i, result);
            tpid_list.push_back(tpid);
        }
    }

    return (m_obj->rpc_set_vlans(mac, vlan_list, tpid_list));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_ipv4(const Json::Value &params, Json::Value &result){
    debug({"rpc_set_ipv4",pretty_json_str(params)});
    string mac        = parse_string(params, "mac", result);
    string ip4_buf    = parse_ipv4(params, "ipv4", result);
    bool shared_ns = parse_bool(params, "shared_ns", result, false);

    if ( shared_ns ) {
        uint8_t subnet = parse_int(params, "subnet", result);
        return m_obj->rpc_set_shared_ns_ipv4(mac, ip4_buf, subnet);
    } else {
        string gw4_buf = parse_ipv4(params, "dg", result);
        return m_obj->rpc_set_ipv4(mac, ip4_buf, gw4_buf);
    }
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_filter(const Json::Value &params, Json::Value &result) {
    debug({"rpc_set_filter", pretty_json_str(params)});
    string mac     = parse_string(params, "mac", result);
    string filter  = parse_string(params, "filter", result);

    return m_obj->rpc_set_filter(mac, filter);
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_dg(const Json::Value &params, Json::Value &result) {
    debug({"rpc_set_dg", pretty_json_str(params)});
    string shared_ns  = parse_string(params, "shared_ns", result);
    string dg         = parse_string(params, "dg", result);

    return m_obj->rpc_set_dg(shared_ns, dg);
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_mtu(const Json::Value &params, Json::Value &result) {
    debug({"rpc_set_mtu", pretty_json_str(params)});
    string mac  = parse_string(params, "mac", result);
    int mtu     = parse_int(params, "mtu", result);

    return m_obj->rpc_set_mtu(mac, std::to_string(mtu));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_clear_ipv4(const Json::Value &params, Json::Value &result){
    debug({"rpc_clear_ipv4",pretty_json_str(params)});
    string mac   = parse_string(params, "mac", result);
    return (m_obj->rpc_clear_ipv4(mac));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_set_ipv6(const Json::Value &params, Json::Value &result){
    debug({"rpc_set_ipv6",pretty_json_str(params)});
    string mac     = parse_string(params, "mac", result);
    bool enable    = parse_bool(params, "enable", result);
    bool shared_ns = parse_bool(params, "shared_ns", result, false);

    string src_ipv6 = "";
    if ( enable ) {
        src_ipv6 = parse_string(params, "src_ipv6", result);
        if ( src_ipv6.size() > 0 ) {
            src_ipv6 = parse_ipv6(params, "src_ipv6", result);
        }
    }
    if ( shared_ns ) {
        uint8_t subnet = parse_int(params, "subnet", result, 0);
        return m_obj->rpc_set_shared_ns_ipv6(mac, enable, src_ipv6, subnet);
    } else {
        string shared_ns = parse_string(params, "shared_ns", result, "");
        return m_obj->rpc_set_ipv6(mac, enable, src_ipv6);
    }
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_remove_all(const Json::Value &params, Json::Value &result){
    debug({"rpc_remove_all",pretty_json_str(params)});
    return (m_obj->rpc_remove_all());
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_get_nodes(const Json::Value &params, Json::Value &result){
    debug({"rpc_get_nodes",pretty_json_str(params)});
    bool only_bird = parse_bool(params, "only_bird", result, false);

    return (m_obj->rpc_get_nodes(result, only_bird));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_get_nodes_info(const Json::Value &params, Json::Value &result){
    debug({"rpc_get_nodes_info",pretty_json_str(params)});
    if (!params["macs"].isArray()) {
        generate_parse_err(result, "input should be array of MAC address");
    }
    return (m_obj->rpc_get_nodes_info(params["macs"],result));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_get_commands_list(const Json::Value &params, Json::Value &result){
    debug({"rpc_get_commands_list",pretty_json_str(params)});
    /* TBD take the list from the registry */
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_clear_counters(const Json::Value &params, Json::Value &result){
    debug({"rpc_clear_counters",pretty_json_str(params)});
    return (m_obj->rpc_clear_counters());
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_counters_get_meta(const Json::Value &params, Json::Value &result){
    debug({"rpc_counters_get_meta",pretty_json_str(params)});
    return (m_obj->rpc_counters_get_meta(params,result));
}

trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_counters_get_value(const Json::Value &params, Json::Value &result){
    debug({"rpc_counters_get_value",pretty_json_str(params)});
    bool zeros   = parse_bool(params, "zeros", result);
    return (m_obj->rpc_counters_get_value(zeros,result));
}



trex_rpc_cmd_rc_e CRpcTunnelCStackBase::rpc_help(const Json::Value &params, Json::Value &result){

    const string &  mac  =  parse_string(params, "mac", result);
    const string & ipv4  = parse_string(params, "ipv4", result);
    const string & ipv4_dg = parse_string(params, "ipv4_dg", result);

    m_obj->rpc_help(mac,ipv4, ipv4_dg);

    result = Json::nullValue;
    return(TREX_RPC_CMD_OK);
}


void CRpcTunnelCStackBase::update_cmd_count(uint32_t total_exec_commands,
                                            uint32_t err_exec_commands){
    m_obj->update_rcp_cmds_count(total_exec_commands,err_exec_commands);
}


void CRpcTunnelCStackBase::register_rpc_functions(){
    using namespace std::placeholders;

    register_func("add_node",std::bind(&CRpcTunnelCStackBase::rpc_add_node, this, _1, _2));
    register_func("add_shared_ns",std::bind(&CRpcTunnelCStackBase::rpc_add_shared_ns, this, _1, _2));
    register_func("remove_node",std::bind(&CRpcTunnelCStackBase::rpc_remove_node, this, _1, _2));
    register_func("remove_shared_ns",std::bind(&CRpcTunnelCStackBase::rpc_remove_shared_ns, this, _1, _2));
    register_func("set_vlans",std::bind(&CRpcTunnelCStackBase::rpc_set_vlans, this, _1, _2));
    register_func("set_ipv4",std::bind(&CRpcTunnelCStackBase::rpc_set_ipv4, this, _1, _2));
    register_func("set_filter",std::bind(&CRpcTunnelCStackBase::rpc_set_filter, this, _1, _2));
    register_func("set_dg",std::bind(&CRpcTunnelCStackBase::rpc_set_dg, this, _1, _2));
    register_func("set_mtu",std::bind(&CRpcTunnelCStackBase::rpc_set_mtu, this, _1, _2));
    register_func("clear_ipv4",std::bind(&CRpcTunnelCStackBase::rpc_clear_ipv4, this, _1, _2));
    register_func("set_ipv6",std::bind(&CRpcTunnelCStackBase::rpc_set_ipv6, this, _1, _2));
    register_func("remove_all",std::bind(&CRpcTunnelCStackBase::rpc_remove_all, this, _1, _2));
    register_func("get_nodes",std::bind(&CRpcTunnelCStackBase::rpc_get_nodes, this, _1, _2));
    register_func("get_nodes_info",std::bind(&CRpcTunnelCStackBase::rpc_get_nodes_info, this, _1, _2));
    register_func("counters_clear",std::bind(&CRpcTunnelCStackBase::rpc_clear_counters, this, _1, _2));
    register_func("counters_get_meta",std::bind(&CRpcTunnelCStackBase::rpc_counters_get_meta, this, _1, _2));
    register_func("counters_get_value",std::bind(&CRpcTunnelCStackBase::rpc_counters_get_value, this, _1, _2));
    register_func("get_commands",std::bind(&CRpcTunnelCStackBase::rpc_get_commands_list, this, _1, _2));

    /* debug, should be removed */
    register_func("rpc_help",std::bind(&CRpcTunnelCStackBase::rpc_help, this, _1, _2));

}

                                        

/***************************************
*             CStackBase               *
***************************************/

CStackBase::CStackBase(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) {
    debug("base stack ctor");
    m_api = api;
    m_ignore_stats = ignore_stats;
    m_is_running_tasks = false;
    m_rpc_tunnel.init(this);
    m_rpc_tunnel.register_rpc_functions();
    m_counters.Create();
}

CStackBase::~CStackBase() {
    debug("base stack dtor");
    assert(m_nodes.size() == 0);
    m_counters.Delete();
}

void CStackBase::add_port_node_async() {
    debug("add port node");
    uint8_t port_id = m_api->get_port_id();
    string port_mac((char*)CGlobalInfo::m_options.m_mac_addr[port_id].u.m_mac.src, 6);
    add_node_async(port_mac);
}

void CStackBase::attr_to_json(Json::Value &res) {
    debug("attr json");
    assert(!m_is_running_tasks); // CP should check this
    Json::Value &cfg = res["layer_cfg"];
    // MAC
    cfg["ether"]["src"] = utl_macaddr_to_str((uint8_t *)m_port_node->get_src_mac().data());
    cfg["ether"]["dst"] = utl_macaddr_to_str((uint8_t *)m_port_node->get_dst_mac().data());
    cfg["ether"]["state"] = m_port_node->is_dst_mac_valid() ? "configured" : "unconfigured";

    // VLAN
    res["vlan"]["tags"] = Json::arrayValue;
    for (auto &tag : m_port_node->get_vlan_tags()) {
        res["vlan"]["tags"].append(tag);
    }

    // IPv4
    if ( m_port_node->get_src_ip4().size() ) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, m_port_node->get_src_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["src"] = buf;
        inet_ntop(AF_INET, m_port_node->get_dst_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["dst"] = buf;
        if ( m_port_node->is_dst_mac_valid() ) {
            cfg["ipv4"]["state"] = "resolved";
        } else {
            cfg["ipv4"]["state"] = "unresolved";
        }
    } else {
        cfg["ipv4"]["state"] = "none";
    }

    if ( m_port_node->is_ip6_enabled() ) {
        cfg["ipv6"]["enabled"] = true;
        string ipv6_src = m_port_node->get_src_ip6();
        if ( ipv6_src.size() ) {
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, ipv6_src.c_str(), buf, INET6_ADDRSTRLEN);
            ipv6_src = buf;
        }
        cfg["ipv6"]["src"] = ipv6_src;
    } else {
        cfg["ipv6"]["enabled"] = false;
    }

}

void CStackBase::grat_to_json(Json::Value &res) {
    assert(!m_is_running_tasks); // CP should check this
    res["is_active"] = false;
}

void CStackBase::add_node_async(const string &mac_buf) {
    debug("add node");
    assert(mac_buf.size()==6);
    m_add_macs_list.insert(mac_buf);
}

void CStackBase::del_node_async(const string &mac_buf) {
    debug("del node");
    assert(mac_buf.size()==6);
    m_del_macs_list.insert(mac_buf);
}

CNodeBase* CStackBase::get_node(const string &mac_buf) {
    return get_node_internal(mac_buf);
}

CNodeBase* CStackBase::get_node_internal(const string &mac_buf) {
    assert(mac_buf.size()==6);
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair != m_nodes.end() ) {
        return iter_pair->second;
    }
    return nullptr;
}

CNodeBase* CStackBase::get_port_node() {
    assert(m_port_node!=nullptr);
    assert(!m_is_running_tasks);
    return m_port_node;
}

bool CStackBase::has_port(const string &mac_buf) {
    assert(m_port_node!=nullptr);
    assert(mac_buf.size()==6);
    return m_port_node->get_src_mac() == mac_buf;
}

bool CStackBase::has_capa(capa_enum capa) {
    return get_capa() & capa;
}

#define MAX_RESULTS_MAP_ITEMS 128

void clean_old_results(stack_result_map_t &results) {
    while ( results.size() > MAX_RESULTS_MAP_ITEMS ) {
        results.erase(results.begin());
    }
}

bool CStackBase::get_tasks_results(uint64_t ticket_id, stack_result_t &results) {
    auto it = m_results.find(ticket_id);
    if ( it == m_results.end() ) {
        return false;
    }
    results = it->second; // copy the results
    if ( it->second.is_ready ) {
        m_results.erase(it);
    }
    return true;
}

void CStackBase::wait_on_tasks(uint64_t ticket_id, stack_result_t &results, double timeout) {
    dsec_t start_sec = now_sec();
    while ( m_is_running_tasks ) {
        if ( timeout > 0 && now_sec() > start_sec + timeout ) {
            cancel_running_tasks();
            throw TrexException("Timeout on waiting for results for of ticket " + to_string(ticket_id));
        }
        rte_pause_or_delay_lowend();
    }
    bool found = get_tasks_results(ticket_id, results);
    if ( !found ) {
        throw TrexException("Results on ticket " + to_string(ticket_id) + " got deleted");
    }
}


void CStackBase::_finish_rpc(uint64_t ticket_id){
    m_rpc_commands.clear();
    m_results[ticket_id].is_ready = true;
    m_is_running_tasks = false;
    rte_rmb();
}


/* this will run the rpc commands list if exits */
void CStackBase::run_pending_tasks_internal_rpc(uint64_t ticket_id){
    assert(m_rpc_commands.length()>0);

    debug({"run_pending_tasks_internal_rpc", to_string(ticket_id),m_rpc_commands});

    Json::Reader reader;
    Json::Value  requests;


    /* basic JSON parsing */
    bool rc = reader.parse(m_rpc_commands, requests, false);
    if (!rc) {
        m_results[ticket_id].m_results["error"]="Bad JSON Format";
        _finish_rpc(ticket_id);
        return;
    }

    /* request can be an array of requests */
    if (!requests.isArray()) {
        m_results[ticket_id].m_results["error"]="Bad JSON Format";
        _finish_rpc(ticket_id);
        return;
    }
    m_total_cmds = requests.size();
    rte_atomic32_init(&m_exec_cmds);
    rte_atomic32_init(&m_exec_cmds_err);
    rte_rmb();

    m_rpc_tunnel.run_batch(requests,m_results[ticket_id].m_results);
    _finish_rpc(ticket_id);
}


void CStackBase::run_pending_tasks_internal(uint64_t ticket_id) {
    if ( m_add_macs_list.size() ) { // add nodes
        CNodeBase *node;
        for(auto &mac : m_add_macs_list) {
            try {
                node = add_node_internal(mac);
                if ( m_nodes.size() == 1 ) {
                    m_port_node = node;
                }
            } catch (const TrexException &ex) {
                m_results[ticket_id].err_per_mac[mac] = ex.what();
            }
        }
        m_add_macs_list.clear();
    } else if ( m_del_macs_list.size() ) { // del nodes
        for(auto &mac : m_del_macs_list) {
            try {
                del_node_internal(mac);
                if ( m_nodes.size() == 0 ) {
                    m_port_node = nullptr;
                }
            } catch (const TrexException &ex) {
                m_results[ticket_id].err_per_mac[mac] = ex.what();
            }
        }
        m_del_macs_list.clear();
    } else { // nodes cfg tasks
        for (auto &node_pair : m_nodes) {
            debug("another cfg");
            try {
                for (auto &task : node_pair.second->m_tasks) {
                    task();
                }
            } catch (const TrexException &ex) {
                m_results[ticket_id].err_per_mac[node_pair.first] = ex.what();
            }
            node_pair.second->m_tasks.clear();
        }
    }
    m_results[ticket_id].is_ready = true;
    m_is_running_tasks = false;
    rte_rmb();
}

bool CStackBase::has_pending_tasks() {
    if ( m_add_macs_list.size() || m_del_macs_list.size() ) {
        return true;
    }
    for (auto &node_pair : m_nodes) {
        if ( node_pair.second->m_tasks.size() ) {
            return true;
        }
    }
    return false;
}



void CStackBase::get_rpc_cmds(TrexStackResultsRC & rc){
    rte_rmb();
    rc.m_total_cmds = m_total_cmds;
    rc.m_total_exec_cmds = rte_atomic32_read(&m_exec_cmds);
    rc.m_total_errs_cmds = rte_atomic32_read(&m_exec_cmds_err);
}


void CStackBase::update_rcp_cmds_count(uint32_t total_exec_commands,
                                       uint32_t err_commands){
    rte_atomic32_set(&m_exec_cmds,total_exec_commands);
    rte_atomic32_set(&m_exec_cmds_err,err_commands);
    rte_rmb();
}

/* run batch of json commands  in async mode, just save the command */
void CStackBase::conf_name_space_batch_async(const std::string &json){
    debug({"conf_name_space_batch_async", json});
    m_rpc_commands = json;
}


void CStackBase::rpc_help(const std::string & mac,const std::string & p1,const std::string & p2){
    printf(" rpc_help base -- not implemented \n"); 
}


void CStackBase::dummy_rpc_command(string ipv4,string ipv4_dg){
    printf(" dummy_rpc_command %s %s \n",ipv4.c_str(),ipv4_dg.c_str());
}

void CStackBase::cancel_pending_tasks() {
    debug("Canceling pending tasks");
    m_add_macs_list.clear();
    m_del_macs_list.clear();
    m_rpc_commands.clear();
    for (auto &node_pair : m_nodes) {
        node_pair.second->m_tasks.clear();
    }
}

void CStackBase::cancel_running_tasks() {
    debug("Canceling running tasks");
    if ( m_is_running_tasks ) {
        pthread_cancel(m_thread_handle);
        m_is_running_tasks = false;
        rte_rmb();
    }
}

void CStackBase::run_pending_tasks_async(uint64_t ticket_id,bool rpc) {
    debug({"Run pending tasks for ticket", to_string(ticket_id)});
    assert(!m_is_running_tasks);
    clean_old_results(m_results);
    if ( !has_pending_tasks() && (rpc==false) ) {
        debug("No pending tasks");
        m_results[ticket_id].is_ready = true;
        return;
    }
    m_is_running_tasks = true;
    rte_rmb();
    m_results[ticket_id].is_ready = false;
    if ( has_capa(FAST_OPS) ) {
        debug("Running in FG");
        run_pending_tasks_internal(ticket_id);
    } else {
        debug("Running in BG");
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(CGlobalInfo::m_socket.get_master_phy_id(), &cpuset);
        thread thrd;
        if (rpc){
            thrd = thread(&CStackBase::run_pending_tasks_internal_rpc, this, ticket_id);
        }else{
            thrd = thread(&CStackBase::run_pending_tasks_internal, this, ticket_id);
        }
        m_thread_handle = thrd.native_handle();
        // run with same affinity as master
        pthread_setaffinity_np(m_thread_handle, sizeof(cpu_set_t), &cpuset);
        pthread_setname_np(m_thread_handle, "Trex RX async task");
        thrd.detach();
    }
}

void CStackBase::reset_async() {
    cancel_running_tasks();
    cancel_pending_tasks();
    if ( m_nodes.size() <= 1 ) {
        return;
    }
    for (auto &iter_pair : m_nodes) {
        if ( iter_pair.second != m_port_node ) {
            del_node_async(iter_pair.first);
        }
    }
}

void CStackBase::cleanup_async() {
    debug("cleanup");
    cancel_running_tasks();
    cancel_pending_tasks();
    m_results.clear();
    for (auto &iter_pair : m_nodes) {
        del_node_async(iter_pair.first);
    }
}

bool CStackBase::is_running_tasks() {
    return m_is_running_tasks;
}


/***************************************
*             CNodeBase                *
***************************************/

CNodeBase::CNodeBase(){
    m_dst_mac_valid = false;
    m_is_loopback = false;
    m_ip6_enabled = false;
    m_l2_mode =false;
}

CNodeBase::~CNodeBase() {}

// public conf
void CNodeBase::conf_dst_mac_async(const string &dst_mac) {
    assert(dst_mac.size()==6);
    debug("conf dst");
    if ( dst_mac != m_dst_mac ) {
        m_tasks.push_back(bind(&CNodeBase::conf_dst_mac_internal, this, dst_mac));
    }
}

void CNodeBase::set_dst_mac_valid_async() {
    debug("set dst valid");
    if ( !m_dst_mac_valid ) {
        m_tasks.push_back(bind(&CNodeBase::set_dst_mac_valid_internal, this, true));
    }
}

void CNodeBase::set_dst_mac_invalid() {
    if ( get_l2_mode()==false ){
        set_dst_mac_valid_internal(false);
    }
}

void CNodeBase::set_is_loopback_async() {
    debug("set is loop");
    if ( !m_is_loopback ) {
        m_tasks.push_back(bind(&CNodeBase::set_is_loopback_internal, this, true));
    }
}

void CNodeBase::set_not_loopback() {
    debug("set no loop");
    set_is_loopback_internal(false);
}

void CNodeBase::conf_vlan_async(const vlan_list_t &vlans) {
    debug("conf vlan");
    if ( vlans != m_vlan_tags ) {
        vlan_list_t tpids_dummy;
        m_tasks.push_back(bind(&CNodeBase::conf_vlan_internal, this, vlans, tpids_dummy));
    }
}

void CNodeBase::conf_ip4_async(const string &ip4_buf, const string &gw4_buf) {
    assert(ip4_buf.size()==4);
    assert(gw4_buf.size()==4);
    debug("conf ip4");
    if ( ip4_buf != m_ip4 || gw4_buf != m_gw4 ) {
        m_tasks.push_back(bind(&CNodeBase::conf_ip4_internal, this, ip4_buf, gw4_buf));
    }
}

void CNodeBase::clear_ip4_async() {
    debug("clear ip4");
    if ( m_ip4.size() || m_gw4.size() ) {
        m_tasks.push_back(bind(&CNodeBase::clear_ip4_internal, this));
    }
}


void CNodeBase::conf_ip6_async(bool enabled, const string &ip6_buf) {
    assert(ip6_buf.size()==16 || ip6_buf.size()==0);
    debug("conf ip6");
    if ( enabled != m_ip6_enabled || ip6_buf != m_ip6 ) {
        m_tasks.push_back(bind(&CNodeBase::conf_ip6_internal, this, enabled, ip6_buf));
    }
}

void CNodeBase::clear_ip6_async() {
    debug("clear ip6");
    if ( m_ip6_enabled || m_ip6.size() ) {
        m_tasks.push_back(bind(&CNodeBase::clear_ip6_internal, this));
    }
}


void CNodeBase::to_json_node(Json::Value &cfg){

    // MAC
    cfg["ether"]["src"] = utl_macaddr_to_str((uint8_t *)get_src_mac().data());

    // VLAN
    cfg["vlan"]["tags"] = Json::arrayValue;
    for (auto &tag : get_vlan_tags()) {
        cfg["vlan"]["tags"].append(tag);
    }
    cfg["vlan"]["tpids"] = Json::arrayValue;
    for (auto &tpid : get_vlan_tpids()) {
        cfg["vlan"]["tpids"].append(tpid);
    }

    // IPv4
    if ( get_src_ip4().size() ) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, get_src_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["src"] = buf;
        inet_ntop(AF_INET, get_dst_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["dst"] = buf;
    } else {
        cfg["ipv4"]["state"] = "none";
    }

    if ( is_ip6_enabled() ) {
        cfg["ipv6"]["enabled"] = true;
        string ipv6_src = get_src_ip6();
        if ( ipv6_src.size() ) {
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, ipv6_src.c_str(), buf, INET6_ADDRSTRLEN);
            ipv6_src = buf;
        }
        cfg["ipv6"]["src"] = ipv6_src;
    } else {
        cfg["ipv6"]["enabled"] = false;
    }
}


void CNodeBase::to_json(Json::Value &cfg){

        // MAC
    cfg["ether"]["src"] = utl_macaddr_to_str((uint8_t *)get_src_mac().data());
    cfg["ether"]["dst"] = utl_macaddr_to_str((uint8_t *)get_dst_mac().data());
    cfg["ether"]["state"] = is_dst_mac_valid() ? "configured" : "unconfigured";

    // VLAN
    cfg["vlan"]["tags"] = Json::arrayValue;
    for (auto &tag : get_vlan_tags()) {
        cfg["vlan"]["tags"].append(tag);
    }

    // IPv4
    if ( get_src_ip4().size() ) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, get_src_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["src"] = buf;
        inet_ntop(AF_INET, get_dst_ip4().c_str(), buf, INET_ADDRSTRLEN);
        cfg["ipv4"]["dst"] = buf;
        if ( is_dst_mac_valid() ) {
            cfg["ipv4"]["state"] = "resolved";
        } else {
            cfg["ipv4"]["state"] = "unresolved";
        }
    } else {
        cfg["ipv4"]["state"] = "none";
    }

    if ( is_ip6_enabled() ) {
        cfg["ipv6"]["enabled"] = true;
        string ipv6_src = get_src_ip6();
        if ( ipv6_src.size() ) {
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, ipv6_src.c_str(), buf, INET6_ADDRSTRLEN);
            ipv6_src = buf;
        }
        cfg["ipv6"]["src"] = ipv6_src;
    } else {
        cfg["ipv6"]["enabled"] = false;
    }
}

// internal conf
void CNodeBase::set_dst_mac_valid_internal(bool valid) {
    debug("conf dst valid internal");
    m_dst_mac_valid = valid;
}

void CNodeBase::set_is_loopback_internal(bool is_loopback) {
    debug("conf loop internal");
    m_is_loopback = is_loopback;
}

void CNodeBase::conf_dst_mac_internal(const string &dst_mac) {
    debug("conf dst internal");
    m_dst_mac = dst_mac;
}

void CNodeBase::conf_vlan_internal(const vlan_list_t &vlans, const vlan_list_t &tpids) {
    throw TrexException("VLAN is not supported with current stack");
}

void CNodeBase::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    throw TrexException("IPv4 is not supported with current stack");
}

void CNodeBase::clear_ip4_internal() {
    m_ip4.clear();
    m_gw4.clear();
}

void CNodeBase::conf_ip6_internal(bool enabled, const string &ip6_buf) {
    throw TrexException("IPv6 is not supported with current stack");
}


void CNodeBase::clear_ip6_internal() {
    m_ip6_enabled = false;
    m_ip6.clear();
}

// getters
bool CNodeBase::is_dst_mac_valid() {
    return m_dst_mac_valid;
}

bool CNodeBase::is_loopback() {
    return m_is_loopback;
}

bool CNodeBase::is_ip6_enabled() {
    return m_ip6_enabled;
}


string CNodeBase::mac_str_to_mac_buf(const std::string & mac){
    char mac_buf[6];
    if ( utl_str_to_macaddr(mac, (uint8_t*)mac_buf) ){
        string s;
        s.assign(mac_buf,6);
        return(s);
    }
    assert(0);
    return("");
}

std::string CNodeBase::get_src_mac_as_str(){
    return ( utl_macaddr_to_str((uint8_t *)get_src_mac().data()) );
}

const string &CNodeBase::get_src_mac() {
    return m_src_mac;
}

const string &CNodeBase::get_dst_mac() {
    return m_dst_mac;
}

const vlan_list_t &CNodeBase::get_vlan_tags() {
    return m_vlan_tags;
}

const vlan_list_t &CNodeBase::get_vlan_tpids() {
    return m_vlan_tpids;
}

const string &CNodeBase::get_src_ip4() {
    return m_ip4;
}

const string &CNodeBase::get_dst_ip4() {
    return m_gw4;
}

const string &CNodeBase::get_src_ip6() {
    return m_ip6;
}


/***************************************
*            Helper funcs              *
***************************************/

// print with -v 7
void debug(const string &msg) {
    if ( CGlobalInfo::m_options.preview.getVMode() > 3 ) {
        cout << msg << endl;
    }
}

void debug(const initializer_list<const string> &msg_list) {
    if ( CGlobalInfo::m_options.preview.getVMode() > 3 ) {
        for (auto &msg : msg_list) {
            printf("%s ", msg.c_str());
        }
        printf("\n");
    }
}

/***************************************
*            CStackFactory             *
***************************************/

CStackBase* CStackFactory::create(std::string &type_name, RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) {
    if ( type_name == "legacy" ) {
        return new CStackLegacy(api, ignore_stats);
    }
    if ( type_name == "linux_based" ) {
        return new CStackLinuxBased(api, ignore_stats);
    }
    string err;
    err = "ERROR: Unknown stack type \"" + type_name + "\"\n";
    err += "Valid types are: legacy, linux_based";
    throw TrexException(err);
}
