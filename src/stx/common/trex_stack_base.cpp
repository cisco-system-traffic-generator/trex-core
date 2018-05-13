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


/***************************************
*             CStackBase               *
***************************************/

CStackBase::CStackBase(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) {
    debug("base stack ctor");
    m_api = api;
    m_ignore_stats = ignore_stats;
    m_is_running_tasks = false;
}

CStackBase::~CStackBase(void) {
    debug("base stack dtor");
    assert(m_nodes.size() == 0);
}

void CStackBase::add_port_node_async(void) {
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
    for (auto tag : m_port_node->get_vlan()) {
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
    assert(!m_is_running_tasks);
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

CNodeBase* CStackBase::get_port_node(void) {
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
}

bool CStackBase::has_pending_tasks(void) {
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

void CStackBase::cancel_pending_tasks(void) {
    debug("Canceling pending tasks");
    m_add_macs_list.clear();
    m_del_macs_list.clear();
    for (auto &node_pair : m_nodes) {
        node_pair.second->m_tasks.clear();
    }
}

void CStackBase::cancel_running_tasks(void) {
    debug("Canceling running tasks");
    if ( m_is_running_tasks ) {
        pthread_cancel(m_thread_handle);
        m_is_running_tasks = false;
    }
}

void CStackBase::run_pending_tasks_async(uint64_t ticket_id) {
    debug({"Run pending tasks for ticket", to_string(ticket_id)});
    assert(!m_is_running_tasks);
    clean_old_results(m_results);
    if ( !has_pending_tasks() ) {
        debug("No pending tasks");
        m_results[ticket_id].is_ready = true;
        return;
    }
    m_is_running_tasks = true;
    m_results[ticket_id].is_ready = false;
    if ( has_capa(FAST_OPS) ) {
        debug("Running in FG");
        run_pending_tasks_internal(ticket_id);
    } else {
        debug("Running in BG");
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(CGlobalInfo::m_socket.get_master_phy_id(), &cpuset);

        thread thrd = thread(&CStackBase::run_pending_tasks_internal, this, ticket_id);
        m_thread_handle = thrd.native_handle();
        // run with same affinity as master
        pthread_setaffinity_np(m_thread_handle, sizeof(cpu_set_t), &cpuset);
        pthread_setname_np(m_thread_handle, "Trex RX async task");
        thrd.detach();
    }
}

void CStackBase::reset_async(void) {
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

void CStackBase::cleanup_async(void) {
    debug("cleanup");
    cancel_running_tasks();
    cancel_pending_tasks();
    m_results.clear();
    for (auto &iter_pair : m_nodes) {
        del_node_async(iter_pair.first);
    }
}

bool CStackBase::is_running_tasks(void) {
    return m_is_running_tasks;
}


/***************************************
*             CNodeBase                *
***************************************/

CNodeBase::CNodeBase() {
    m_dst_mac_valid = false;
    m_is_loopback = false;
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

void CNodeBase::set_dst_mac_valid_async(void) {
    debug("set dst valid");
    if ( !m_dst_mac_valid ) {
        m_tasks.push_back(bind(&CNodeBase::set_dst_mac_valid_internal, this, true));
    }
}

void CNodeBase::set_dst_mac_invalid(void) {
    set_dst_mac_valid_internal(false);
}

void CNodeBase::set_is_loopback_async(void) {
    debug("set is loop");
    if ( !m_is_loopback ) {
        m_tasks.push_back(bind(&CNodeBase::set_is_loopback_internal, this, true));
    }
}

void CNodeBase::set_not_loopback(void) {
    debug("set no loop");
    set_is_loopback_internal(false);
}

void CNodeBase::conf_vlan_async(const vlan_list_t &vlans) {
    debug("conf vlan");
    if ( vlans != m_vlan_tags ) {
        m_tasks.push_back(bind(&CNodeBase::conf_vlan_internal, this, vlans));
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

void CNodeBase::clear_ip4_async(void) {
    debug("clear ip4");
    if ( m_ip4.size() || m_gw4.size() ) {
        m_tasks.push_back(bind(&CNodeBase::clear_ip4_internal, this));
    }
}

void CNodeBase::conf_ip6_async(const string &ip6_buf, const string &gw6_buf) {
    assert(ip6_buf.size()==16);
    assert(gw6_buf.size()==16);
    debug("conf ip6");
    if ( ip6_buf != m_ip6 || gw6_buf != m_gw6 ) {
        m_tasks.push_back(bind(&CNodeBase::conf_ip6_internal, this, ip6_buf, gw6_buf));
    }
}

void CNodeBase::clear_ip6_async(void) {
    debug("clear ip6");
    if ( m_ip6.size() || m_gw6.size() ) {
        m_tasks.push_back(bind(&CNodeBase::clear_ip6_internal, this));
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

void CNodeBase::conf_vlan_internal(const vlan_list_t &vlans) {
    throw TrexException("VLAN is not supported with current stack");
}

void CNodeBase::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    throw TrexException("IPv4 is not supported with current stack");
}

void CNodeBase::clear_ip4_internal(void) {
    m_ip4.clear();
    m_gw4.clear();
}

void CNodeBase::conf_ip6_internal(const string &ip6_buf, const string &gw6_buf) {
    throw TrexException("IPv6 is not supported with current stack");
}

void CNodeBase::clear_ip6_internal(void) {
    m_ip6.clear();
    m_gw6.clear();
}

// getters
bool CNodeBase::is_dst_mac_valid(void) {
    return m_dst_mac_valid;
}

bool CNodeBase::is_loopback(void) {
    return m_is_loopback;
}

const string &CNodeBase::get_src_mac(void) {
    return m_src_mac;
}

const string &CNodeBase::get_dst_mac(void) {
    return m_dst_mac;
}

const string &CNodeBase::get_src_ip4(void) {
    return m_ip4;
}

const string &CNodeBase::get_dst_ip4(void) {
    return m_gw4;
}

const vlan_list_t &CNodeBase::get_vlan(void) {
    return m_vlan_tags;
}


/***************************************
*            Helper funcs              *
***************************************/

// print with -v 7
void debug(const string &msg) {
    if ( CGlobalInfo::m_options.preview.getVMode() > 6 ) {
        cout << msg << endl;
    }
}

void debug(const initializer_list<const string> &msg_list) {
    if ( CGlobalInfo::m_options.preview.getVMode() > 6 ) {
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
