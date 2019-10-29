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
#include "trex_stack_linux_based.h"
#include "trex_global.h"
#include "common/basic_utils.h"
#include "utl_sync_barrier.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <future>
#include <ifaddrs.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <regex.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include "os_time.h"


char clean_old_nets_and_get_prefix();
void verify_programs();
void str_from_mbuf(const rte_mbuf_t *m, string &result);
void popen_with_err(const string &cmd, const string &err);
void popen_general(const string &cmd, const string &err, bool throw_exception, string &output);
void popen_with_output(const string &cmd, const string &err, bool throw_exception, string &output);

CMcastFilter::CMcastFilter() {
    m_vlan_nodes[0] = 0;
    m_vlan_nodes[1] = 0;
    m_vlan_nodes[2] = 0;
    m_multicast_bpf = bpfjit_compile("");
}

void CMcastFilter::renew_multicast_bpf() {
    const string emul_pkts = "not udp and not tcp";
    string bpf_str;
    bool v0 = m_vlan_nodes[0];
    bool v1 = m_vlan_nodes[1];
    bool v2 = m_vlan_nodes[2];

    if ( v0 ) {
        if ( v1 ) {
            if ( v2 ) { // all the mix
                bpf_str = emul_pkts + " or vlan and " + emul_pkts + " or vlan and " + emul_pkts;
            } else { // untagged and 1 VLAN
                bpf_str = emul_pkts + " or vlan and " + emul_pkts;
            }
        } else {
            if ( v2 ) { // untagged and 2 VLANs
                bpf_str = emul_pkts + " or vlan and vlan and " + emul_pkts;
            } else { // untagged only
                bpf_str = emul_pkts;
            }
        }
    } else {
        if ( v1 ) {
            if ( v2 ) { // 1 and 2 VLANs only
                bpf_str = "vlan and " + emul_pkts + " or vlan and " + emul_pkts;
            } else { // only 1 VLAN
                bpf_str = "vlan and " + emul_pkts;
            }
        } else {
            if ( v2 ) { // only 2 VLANs
                bpf_str = "vlan and vlan and " + emul_pkts;
            } else { // no nodes
                bpf_str = "";
            }
        }
    }

    m_multicast_bpf = bpfjit_compile(bpf_str.c_str());
}

const bpf_h& CMcastFilter::get_bpf() {
    return m_multicast_bpf;
}

void CMcastFilter::add_del_vlans(uint8_t add_vlan_size, uint8_t del_vlan_size) {
    if ( add_vlan_size == del_vlan_size ) {
        return;
    }
    m_vlan_nodes[add_vlan_size]++;
    m_vlan_nodes[del_vlan_size]--;
    renew_multicast_bpf();
}

void CMcastFilter::add_empty() {
    m_vlan_nodes[0]++;
    if ( m_vlan_nodes[0] == 1 ) {
        renew_multicast_bpf();
    }
}


/***************************************
*          CStackLinuxBased            *
***************************************/

bool CStackLinuxBased::m_is_initialized = false;
string CStackLinuxBased::m_mtu = "";
string CStackLinuxBased::m_ns_prefix = "";
string CStackLinuxBased::m_bird_ns = "";

CStackLinuxBased::CStackLinuxBased(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) : CStackBase(api, ignore_stats) {
    if ( !m_is_initialized ) {
        verify_programs();
        char prefix_char = clean_old_nets_and_get_prefix();
        m_ns_prefix = string("trex-") + prefix_char + "-";
        debug("Using netns prefix " + m_ns_prefix);
        m_mtu = to_string(MAX_PKT_ALIGN_BUF_9K);
        if ( CGlobalInfo::m_options.m_is_bird_enabled ) {
            init_bird();
        }
        m_is_initialized = true;
    }
    rte_spinlock_init(&m_main_loop);
    m_epoll_fd = epoll_create1(0);
    if(m_epoll_fd == -1){
        throw TrexException("Failed to create epoll file descriptor  ");
    }
    get_platform_api().getPortAttrObj(api->get_port_id())->set_multicast(true); // We need multicast for IPv6
    m_next_namespace_id = 0;
    m_next_bird_if_id = 0;
}

CStackLinuxBased::~CStackLinuxBased() {
    debug("Linux stack dtor");
    assert(m_epoll_fd);
    close(m_epoll_fd);
    if ( m_is_initialized && CGlobalInfo::m_options.m_is_bird_enabled ) {
        kill_bird_and_ns();
        m_is_initialized = false;
    }
}

void CStackLinuxBased::handle_pkt(const rte_mbuf_t *m) {
    if ( (m->data_len >= 6) && (m->pkt_len <= MAX_PKT_ALIGN_BUF_9K) ) {
        string pkt;
        str_from_mbuf(m, pkt);

        CSpinLock lock(&m_main_loop);
        /* TBD need to fix the multicast issue could create bursts */

        if ( pkt[0] & 1 ) {
            bool rc = bpfjit_run(m_mcast_filter.get_bpf(), pkt.c_str(), pkt.size());
            if ( (uint8_t)pkt[0] == 0xff ) {
                if ( !rc ) {
                    m_counters.m_rx_bcast_filtered++;
                    return;
                }
                m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_PKT][CRxCounters::CNT_BROADCAST]++;
                m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_BYTE][CRxCounters::CNT_BROADCAST] += pkt.size();
            } else {
                if ( !rc ) {
                    m_counters.m_rx_mcast_filtered++;
                    return;
                }
                m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_PKT][CRxCounters::CNT_MULTICAST]++;
                m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_BYTE][CRxCounters::CNT_MULTICAST] += pkt.size();
            }
            debug("Linux handle_pkt: got broadcast or multicast");
            for (auto &iter_pair : m_nodes ) {
                uint16_t sent_len = ((CNamespacedIfNode*)iter_pair.second)->filter_and_send(pkt);
                debug({"sent:", to_string(sent_len)});
            }
        } else {
            m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_PKT][CRxCounters::CNT_UNICAST]++;
            m_counters.m_gen_cnt[CRxCounters::CNT_RX][CRxCounters::CNT_BYTE][CRxCounters::CNT_UNICAST] += pkt.size();
            string mac_dst_buf(pkt, 0, 6);
            debug("Linux handle_pkt: got unicast");
            CNamespacedIfNode *node = (CNamespacedIfNode *)get_node(mac_dst_buf);
            if ( node ) {
                uint16_t sent_len = node->filter_and_send(pkt);
                debug({"sent:", to_string(sent_len)});
            } else {
                debug({"Linux handle_pkt: did not find node with mac:", utl_macaddr_to_str((uint8_t *)mac_dst_buf.data())});
            }
        }
    }else{
        m_counters.m_rx_err_invalid_pkt++;
    }
}


void CStackLinuxBased::rpc_help(const std::string & mac,const std::string & p1,const std::string & p2){
    printf(" rpc_help in the thread %s %s %s \n",mac.c_str(),p1.c_str(),p2.c_str());
    delay(2000); /* delay 2 sec */
}


string CStackLinuxBased::mac_str_to_mac_buf(const std::string & mac){
    char mac_buf[6];
    if ( utl_str_to_macaddr(mac, (uint8_t*)mac_buf) ){
        string s;
        s.assign(mac_buf,6);
        return(s);
    }
    throw (TrexRpcCommandException(TREX_RPC_CMD_PARSE_ERR,"wrong mac format"));
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_add_node(const std::string & mac){
    /* this is RPC command */
    try {
        CNamespacedIfNode *node = (CNamespacedIfNode *)add_node_internal(mac_str_to_mac_buf(mac));
        node->set_associated_trex(false); /* from RPC we need to mark them as namespace ! */
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_add_bird_node(const std::string & mac){
    /* this is RPC command */
    try {
        CNamespacedIfNode *node = (CNamespacedIfNode *)add_bird_node_internal(mac_str_to_mac_buf(mac));
        node->set_associated_trex(false); /* from RPC we need to mark them as namespace ! */
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_remove_node(const std::string & mac){
    try {
       del_node_internal(mac_str_to_mac_buf(mac));
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_set_vlans(const std::string & mac,
                                                  const vlan_list_t &vlan_list,
                                                  const vlan_list_t &tpid_list) {
    CNamespacedIfNode * lp=get_node_rpc(mac);

    CSpinLock lock(&m_main_loop);
    lp->conf_vlan_internal(vlan_list, tpid_list);

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_set_ipv4(const std::string & mac,
                                                 std::string ip4_buf,
                                                 std::string gw4_buf) {
    CNamespacedIfNode * lp = get_node_rpc(mac);
    try {
        lp->conf_ip4_internal(ip4_buf, gw4_buf);
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_set_ipv4_bird(const std::string &mac,
                                                      std::string ip4_buf,
                                                      uint8_t subnet) {
    CNamespacedIfNode * lp = get_node_rpc(mac);
    try {
        lp->conf_ip4_internal_bird(ip4_buf, subnet);
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}


trex_rpc_cmd_rc_e CStackLinuxBased::rpc_clear_ipv4(const std::string & mac){
    CNamespacedIfNode * lp=get_node_rpc(mac);
    try {
       lp->clear_ip4_internal();
    } catch (const TrexException &ex) {
       throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_set_ipv6(const std::string & mac,bool enable, std::string src_ipv6_buf){
    CNamespacedIfNode * lp=get_node_rpc(mac);
    try {
        if (enable) {
            lp->conf_ip6_internal(true, src_ipv6_buf);
        }else{
            lp->clear_ip6_internal();
        }
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_set_ipv6_bird(const std::string &mac,bool enable, std::string src_ipv6_buf, uint8_t subnet) {
    CNamespacedIfNode * lp=get_node_rpc(mac);
    try {
        if (enable) {
            lp->conf_ip6_internal_bird(true, src_ipv6_buf, subnet);
        } else {
            lp->clear_ip6_internal();
        }
    } catch (const TrexException &ex) {
        throw TrexRpcException(ex.what());
    }
    return (TREX_RPC_CMD_OK);
}

void CStackLinuxBased::create_bird_ns() {
    popen_with_err("ip netns add " + m_bird_ns, "Could not create network namespace");
}

void CStackLinuxBased::run_bird_in_ns() {
    run_in_ns(" cd " + m_bird_path + "; ./trex_bird -c " + "bird.conf -s " + "bird.ctl", "Error running bird process");
    popen_with_err("chmod 666 bird/bird.ctl", "cannot change permissions of bird.ctl for PyBird client communication");
}

void CStackLinuxBased::run_in_ns(const string &cmd, const string &err) {
    // using "cmd" for multiple commands
    popen_with_err(("ip netns exec "  + m_bird_ns + " bash -c " + "\"" + cmd + "\"").c_str(), "cannot run " + cmd + " in ns " + m_bird_ns);
}

void CStackLinuxBased::kill_bird_and_ns() {
    string out = "";
    popen_with_output("pgrep trex_bird", "cannot get bird pid!", false, out);
    if ( out != "" ) {
        popen_with_err("kill $(pgrep trex_bird)", "Error killing bird");
    }
    out = "";
    popen_with_output("ip netns list", "cannot get namespaces", false, out);
    if ( out.find(m_bird_ns) != string::npos) {
        popen_with_err("ip netns delete " + m_bird_ns, "Error deleting bird-ns namespace");
    }
}

const string CStackLinuxBased::get_bird_path() {
    std::string bird_path;
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        TrexException("cannot get bird path");
    } else {
        bird_path += (cwd + string("/bird"));
    }
    return bird_path;
}


#define MAX_EVENTS 128

uint16_t CStackLinuxBased::handle_tx(uint16_t limit) {
    /* limit is not used */
    string read_buf_str;
    struct epoll_event events[MAX_EVENTS];
    int event_count = epoll_wait(m_epoll_fd, events, MAX_EVENTS, 0);
    if ( event_count ) {
        int i;
        for (i=0; i<event_count; i++) {
                // read one packet
            uint16_t pkt_len = recv(events[i].data.fd, m_rw_buf, MAX_PKT_ALIGN_BUF_9K, MSG_DONTWAIT);
            if ( pkt_len < 14 ) {
                m_counters.m_tx_err_small_pkt++;
                continue;
            }
            string src_mac(m_rw_buf + 6, 6);

            /* update counters */
            if ( (uint8_t)m_rw_buf[0] == 0xff ) {
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_PKT][CRxCounters::CNT_BROADCAST]++;
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_BYTE][CRxCounters::CNT_BROADCAST] += pkt_len;
            } else if ( m_rw_buf[0] & 1 ) {
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_PKT][CRxCounters::CNT_MULTICAST]++;
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_BYTE][CRxCounters::CNT_MULTICAST] += pkt_len;
            } else {
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_PKT][CRxCounters::CNT_UNICAST]++;
                m_counters.m_gen_cnt[CRxCounters::CNT_TX][CRxCounters::CNT_BYTE][CRxCounters::CNT_UNICAST] += pkt_len;
            }

            CSpinLock lock(&m_main_loop);
            auto iter_pair = m_nodes.find(src_mac);
            if ( iter_pair == m_nodes.end() ) {
                lock.unlock();
                continue;
            }
            CNamespacedIfNode *node = (CNamespacedIfNode*)iter_pair->second;
            string &vlans_insert_to_pkt = node->get_vlans_insert_to_pkt();
            if ( pkt_len + vlans_insert_to_pkt.size() <= MAX_PKT_ALIGN_BUF_9K ) {
                debug({"Linux handle_tx: pkt len:", to_string(pkt_len)});
                if ( vlans_insert_to_pkt.size() ) {
                    read_buf_str.assign(m_rw_buf, 12);
                    read_buf_str += vlans_insert_to_pkt;
                    read_buf_str.append(m_rw_buf + 12, pkt_len - 12);
                } else {
                    read_buf_str.assign(m_rw_buf, pkt_len);
                }
                m_api->tx_pkt(read_buf_str);
            }else{
                m_counters.m_tx_err_big_9k++;
            }
            lock.unlock();
        }
    }
    return event_count;
}

CNodeBase* CStackLinuxBased::add_bird_node_internal(const string &mac_buf) {
    debug({" add_bird_node_internal  ", utl_macaddr_to_str((uint8_t *)mac_buf.data())} );

    string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
    stringstream ss;
    ss << m_ns_prefix << hex << (int)m_api->get_port_id() << "-B-" << m_next_bird_if_id;

    if ( get_node_internal(mac_buf) != nullptr ) {
        throw TrexException("Node with MAC " + mac_str + " already exists");
    }

    CNamespacedIfNode *node = new CBirdIfNode(m_bird_ns, ss.str(), mac_str, mac_buf, m_mtu, m_mcast_filter);
    m_next_bird_if_id++;
    return add_linux_events_and_node(mac_buf, mac_str, node);
}

CNodeBase* CStackLinuxBased::add_node_internal(const string &mac_buf) {
    debug({" add_node_internal  ", utl_macaddr_to_str((uint8_t *)mac_buf.data())} );

    string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
    if ( get_node_internal(mac_buf) != nullptr ) {
        throw TrexException("Node with MAC " + mac_str + " already exists");
    }

    stringstream ss;
    ss << m_ns_prefix << hex << (int)m_api->get_port_id() << "-" << m_next_namespace_id;
    CNamespacedIfNode *node = new CLinuxIfNode(ss.str(), mac_str, mac_buf, m_mtu, m_mcast_filter);

    if (node == nullptr) {
        throw TrexException("Could not create node " + mac_str);
    }

    m_next_namespace_id++;

    return add_linux_events_and_node(mac_buf, mac_str, node);
}

CNodeBase *CStackLinuxBased::add_linux_events_and_node(const string &mac_buf, const string &mac_str, CNamespacedIfNode *node) {
    node->m_event.events = EPOLLIN;
    node->m_event.data.fd = node->get_pair_id();

    // thread safe
    if(epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, node->get_pair_id(), &node->m_event)){
       throw TrexException("Failed to add file descriptor to epoll " + mac_str +" " +strerror(errno));
    }

    CSpinLock lock(&m_main_loop);
    /* add the nodes */
    m_nodes[mac_buf] = node;

    return node;
}


CNamespacedIfNode * CStackLinuxBased::get_node_rpc(const std::string &mac){
    CNamespacedIfNode * lp=get_node_by_mac(mac);
    if (!lp) {
        stringstream ss;
        ss << "node " << mac << " does not exits " ;
        throw (TrexRpcCommandException(TREX_RPC_CMD_PARSE_ERR,ss.str()));
    }
    return (lp);
}

CNamespacedIfNode * CStackLinuxBased::get_node_by_mac(const std::string &mac){
    string mac_buf = mac_str_to_mac_buf(mac);
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair == m_nodes.end() ) {
        return(0);
    }
    CNamespacedIfNode *node = (CNamespacedIfNode*)iter_pair->second;
    return (node);
}

void CStackLinuxBased::del_node_internal(const string &mac_buf) {
    string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());

    debug({" del_node_internal  ", mac_str} );

    CSpinLock lock(&m_main_loop);
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair == m_nodes.end() ) {
        throw TrexException("Node with MAC " + mac_str + " does not exist");
    }
    CNamespacedIfNode *node = (CNamespacedIfNode*)iter_pair->second;
    int pair_id = node->get_pair_id();
    m_nodes.erase(iter_pair->first);
    lock.unlock();

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = pair_id;

    if(epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pair_id, &event)){
       throw TrexException("Failed to remove file descriptor to epoll " + mac_str);
    }

    /* could raise an exception */
    delete node;
}


#define MAX_REMOVES_UNDER_LOCK 20

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_remove_all(){

    std::vector<std::string> m_vec;

    std::string last_ex;
    bool error = false;
    while (true) {

        /* under the lock */
        CSpinLock lock(&m_main_loop);
        for (auto &iter_pair : m_nodes ) {
            CNamespacedIfNode * lp=(CNamespacedIfNode*)iter_pair.second;
            /* add the nodes that are not related to trex physical ports */
            if (!lp->is_associated_trex()){
                m_vec.push_back(lp->get_src_mac());
            }
            if (m_vec.size()>MAX_REMOVES_UNDER_LOCK){
                break;
            }
        }
        lock.unlock();

        if (m_vec.size()==0) {
            break;
        }
        /* try to remove them, might be removed by other command*/
        for (auto &mac_buf:m_vec) {
            try {
              del_node_internal(mac_buf);
            } catch (const TrexException &ex) {
                last_ex  = ex.what();
                error =true;
            }
        }
        m_vec.clear();
    }

    if (error) {
        throw TrexRpcException(last_ex);
    }

    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_clear_counters(){
    m_counters.clear_counters();
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_counters_get_meta(const Json::Value &params, Json::Value &result){
    m_counters.dump_meta("stack_counters",result);
    return (TREX_RPC_CMD_OK);
}

trex_rpc_cmd_rc_e CStackLinuxBased::rpc_counters_get_value(bool zeros, Json::Value &result){
    m_counters.dump_values("stack_counters",zeros,result);
    return (TREX_RPC_CMD_OK);
}


trex_rpc_cmd_rc_e CStackLinuxBased::rpc_get_nodes_info(const Json::Value &params,
                                                       Json::Value &result){
    result["nodes"]= Json::arrayValue;

    int i;
    for (i=0; i<params.size(); i++) {
        string mac_str = params[i].asString();
        CNamespacedIfNode * lp=get_node_rpc(mac_str);
        if (lp) {
            Json::Value json_val;
            lp->to_json_node(json_val);
            result["nodes"].append(json_val);
        }
    }
    return (TREX_RPC_CMD_OK);
}


trex_rpc_cmd_rc_e CStackLinuxBased::rpc_get_nodes(Json::Value &result){

    result["nodes"]= Json::arrayValue;
    /* no need for locks */
    for (auto &iter_pair : m_nodes ) {
        CNamespacedIfNode * lp=(CNamespacedIfNode*)iter_pair.second;
        /* add the nodes that are not related to trex physical ports */
        if (!lp->is_associated_trex()){
            Json::Value json_val=lp->get_src_mac_as_str();
            result["nodes"].append(json_val);
        }
    }

   return (TREX_RPC_CMD_OK);
}


uint16_t CStackLinuxBased::get_capa() {
    return (CLIENTS | BIRD);
}

void CStackLinuxBased::init_bird() {
    m_bird_path = get_bird_path();
    m_bird_ns = m_ns_prefix + "bird-ns";
    kill_bird_and_ns(); // ensure bird isn't running & bird namespace isn't up
    create_bird_ns();
    run_bird_in_ns();
}


/***************************************
*         CNamespacedIfNode            *
***************************************/

CNamespacedIfNode::CNamespacedIfNode() {}

CNamespacedIfNode::~CNamespacedIfNode() {}

void CNamespacedIfNode::to_json_node(Json::Value &res) {
    CNodeBase::to_json_node(res);
    res["linux-ns"] = m_ns_name;
    res["linux-veth-internal"] = m_if_name+"-L";
    res["linux-veth-external"] = m_if_name+"-T";
}

uint16_t CNamespacedIfNode::filter_and_send(const string &pkt) {
    bool rc = bpfjit_run(m_bpf, pkt.c_str(), pkt.size());
    if ( !rc ) {
        debug("Packet filtered out by BPF");
        return 0;
    }
    debug("Packet was NOT filtered by BPF");
    if ( m_vlans_insert_to_pkt.size() ) {
        string vlan_pkt(pkt, 0, 12);
        vlan_pkt += pkt.substr(12 + m_vlans_insert_to_pkt.size());
        return send(m_pair_id, vlan_pkt.c_str(), vlan_pkt.size(), MSG_DONTWAIT);
    } else {
        return send(m_pair_id, pkt.c_str(), pkt.size(), MSG_DONTWAIT);
    }
}

void CNamespacedIfNode::create_ns() {
    popen_with_err("ip netns add " + m_ns_name, "Could not create network namespace");
}

void CNamespacedIfNode::create_veths(const string &mtu) {
    popen_with_err("ip link add " + m_if_name + "-T type veth peer name " + m_if_name + "-L", "Could not create veth pair");
    popen_with_err("sysctl net.ipv6.conf." + m_if_name + "-T.disable_ipv6=1", "Could not disable ipv6 for veth");
    popen_with_err("ip link set " + m_if_name + "-T mtu " + mtu + " up", "Could not configure veth");
    popen_with_err("ip link set " + m_if_name + "-L netns " + m_ns_name, "Could not add veth to namespace");
    run_in_ns("sysctl net.ipv6.conf." + m_if_name + "-L.disable_ipv6=1", "Could not disable ipv6 for veth");
    run_in_ns("ip link set " + m_if_name + "-L mtu " + mtu + " up", "Could not configure veth");
}

void CNamespacedIfNode::run_in_ns(const string &cmd, const string &err) {
    popen_with_err("ip netns exec " + m_ns_name + " " + cmd, err);
}

void CNamespacedIfNode::delete_net() {
    delete_veth();
    delete_ns();
}

void CNamespacedIfNode::delete_veth() {
    popen_with_err("ip link delete " + m_if_name + "-T", "Could not delete veth");
}

void CNamespacedIfNode::delete_ns() {
    popen_with_err("ip netns delete " + m_ns_name, "Could not delete network namespace");
}

void CNamespacedIfNode::bind_pair() {
    int sockfd;
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if ( sockfd < 0 ) {
        throw TrexException("Could not open new node");
    }

    string if_name = m_if_name + "-T";
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ-1);
    if ( ioctl(sockfd , SIOCGIFINDEX , &ifr) < 0) {
        throw TrexException("Unable to find interface index for node");
    }
    struct sockaddr_ll sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sll_family   = AF_PACKET;
    sockaddr.sll_protocol = htons(ETH_P_ALL);
    sockaddr.sll_ifindex  = ifr.ifr_ifindex;

    if ( bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0 ) {
        throw TrexException("Unable to bind to node");
    }
    m_pair_id = sockfd;
}

void CNamespacedIfNode::set_src_mac(const string &mac_str, const string &mac_buf) {
    run_in_ns("ip link set dev " + m_if_name + "-L address " + mac_str, "Could not set MAC address for veth");
    m_src_mac = mac_buf;
}

void append_to_str(uint16_t num, string &str) {
    str += num >> 8;
    str += num & 0xff;
}

uint16_t get_tpid(const vlan_list_t &tpids, uint8_t index, uint16_t def) {
    return tpids.size() > index ? tpids[index] : def;
}

void CNamespacedIfNode::conf_vlan_internal(const vlan_list_t &vlans, const vlan_list_t &tpids) {
    string bpf_str = "";
    m_vlans_insert_to_pkt = "";

    uint16_t tpid;
    for (auto &vlan : vlans) {
        if ( vlans.size() == 2 && !bpf_str.size() ) {
            tpid = get_tpid(tpids, 0, EthernetHeader::Protocol::QINQ);
            append_to_str(tpid, m_vlans_insert_to_pkt);
        } else {
            tpid = get_tpid(tpids, 1, EthernetHeader::Protocol::VLAN);
            append_to_str(tpid, m_vlans_insert_to_pkt);
        }
        append_to_str(vlan, m_vlans_insert_to_pkt);
        bpf_str += "vlan " + to_string(vlan) + " and ";
    }

    bpf_str += get_default_bpf();
    m_bpf = bpfjit_compile(bpf_str.c_str());
    m_mcast_filter->add_del_vlans(vlans.size(), m_vlan_tags.size());
    m_vlan_tags = vlans;
    m_vlan_tpids = tpids;
}

void CNamespacedIfNode::clear_ip4_internal() {
    run_in_ns("ip -4 addr flush dev " + m_if_name + "-L", "Could not flush IPv4 for veth");
    m_ip4.clear();
    m_gw4.clear();
}

void CNamespacedIfNode::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    throw TrexException("Wrong node type, must not be bird!");
}

void CNamespacedIfNode::conf_ip4_internal_bird(const string &ip4_buf, uint8_t subnet) {
    throw TrexException("Wrong node type, must be bird!");
}

void CNamespacedIfNode::conf_ip6_internal(bool enabled, const string &ip6_buf) {
    throw TrexException("Wrong node type, must not be bird!");
}

void CNamespacedIfNode::conf_ip6_internal_bird(bool enabled, const string &ip6_buf, uint8_t subnet) {
    throw TrexException("Wrong node type, must be bird!");
}

void CLinuxIfNode::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    clear_ip4_internal();
    char buf[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, ip4_buf.c_str(), buf, INET_ADDRSTRLEN);
    string ip4_str(buf);
    run_in_ns("ip -4 addr add " + ip4_str + "/32 dev " + m_if_name + "-L", "Could not set IPv4 for veth");

    inet_ntop(AF_INET, gw4_buf.c_str(), buf, INET_ADDRSTRLEN);
    string gw4_str(buf);

    run_in_ns("ip -4 route add " + gw4_str + " dev " + m_if_name + "-L", "Could not add route to default IPv4 gateway from veth");
    run_in_ns("ip -4 route add default via " + gw4_str, "Could not set default IPv4 gateway for veth");

    m_ip4 = ip4_buf;
    m_gw4 = gw4_buf;
}

void CBirdIfNode::conf_ip4_internal_bird(const string &ip4_buf, uint8_t subnet) {
    if ( subnet < 1 || subnet > 32 ) {
        throw TrexException("subnet: " + to_string(subnet) + " is not valid!");
    }
    clear_ip4_internal();
    char buf[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, ip4_buf.c_str(), buf, INET_ADDRSTRLEN);
    string ip4_str(buf);
    run_in_ns("ip -4 addr add " + ip4_str + "/" + to_string(subnet) + " dev " + m_if_name + "-L", "Could not set IPv4 for veth");

    m_ip4 = ip4_buf;
    m_subnet4 = subnet;
}

void CNamespacedIfNode::clear_ip6_internal() {
    run_in_ns("sysctl net.ipv6.conf." + m_if_name + "-L.disable_ipv6=1", "Could not disable ipv6 for veth");
    m_ip6_enabled = false;
    m_ip6.clear();
}

void CLinuxIfNode::conf_ip6_internal(bool enabled, const string &ip6_buf) {
    clear_ip6_internal();
    char buf[INET6_ADDRSTRLEN];
    if ( enabled ) {
        run_in_ns("sysctl net.ipv6.conf." + m_if_name + "-L.disable_ipv6=0", "Could not enable ipv6 for veth");
        run_in_ns("ip -6 route add default dev " + m_if_name + "-L", "Could not set interface as default route");
        if ( ip6_buf.size() ) {
            inet_ntop(AF_INET6, ip6_buf.c_str(), buf, INET6_ADDRSTRLEN);
            string ip6_str(buf);
            run_in_ns("ip -6 addr add " + ip6_str + "/128 dev " + m_if_name + "-L", "Could not set IPv6 for veth");
        }
    }
    m_ip6_enabled = enabled;
    m_ip6 = ip6_buf;
}

void CBirdIfNode::conf_ip6_internal_bird(bool enabled, const string &ip6_buf, uint8_t subnet) {
    if ( subnet < 1 || subnet > 128 ) {
        throw TrexException("subnet: " + to_string(subnet) + " is not valid!");
    }
    clear_ip6_internal();
    char buf[INET6_ADDRSTRLEN];
    if ( enabled ) {
        run_in_ns("sysctl net.ipv6.conf." + m_if_name + "-L.disable_ipv6=0", "Could not enable ipv6 for veth");
        if ( ip6_buf.size() ) {
            inet_ntop(AF_INET6, ip6_buf.c_str(), buf, INET6_ADDRSTRLEN);
            string ip6_str(buf);
            run_in_ns("ip -6 addr add " + ip6_str + "/" + to_string(subnet) + " dev " + m_if_name + "-L", "Could not set IPv6 for veth");
        }
    }
    m_ip6_enabled = enabled;
    m_ip6 = ip6_buf;
    m_subnet6 = subnet;
}

// veth pair (from TRex side)
int CNamespacedIfNode::get_pair_id() {
    return m_pair_id;
}

// string of VLAN header(s) to insert into packet
string &CNamespacedIfNode::get_vlans_insert_to_pkt() {
    return m_vlans_insert_to_pkt;
}


/***************************************
*            CLinuxIfNode              *
***************************************/

CLinuxIfNode::CLinuxIfNode(const string &ns_name, const string &mac_str, const string &mac_buf,
            const string &mtu, CMcastFilter &mcast_filter) {
    debug("Linux node ctor");
    m_ns_name = ns_name;
    m_if_name = ns_name;
    m_associated_trex_ports = true;
    create_ns();
    create_veths(mtu);
    set_src_mac(mac_str, mac_buf);
    bind_pair();
    m_bpf = bpfjit_compile(get_default_bpf());
    m_mcast_filter = &mcast_filter;
    m_mcast_filter->add_empty();
}

CLinuxIfNode::~CLinuxIfNode() {
    debug("Linux node dtor");
    delete_net();
}

const char *CLinuxIfNode::get_default_bpf() {
    return "not udp and not tcp";
}

void CLinuxIfNode::to_json_node(Json::Value &res) {
    CNamespacedIfNode::to_json_node(res);
    res["is_bird"] = false;
}


/***************************************
*             CBirdIfNode              *
***************************************/

CBirdIfNode::CBirdIfNode(const string &ns_name, const string &if_name, const string &mac_str, const string &mac_buf,
                 const string &mtu, CMcastFilter &mcast_filter) {
    debug("Bird node ctor");
    m_ns_name = ns_name;
    m_if_name = if_name;
    debug("Initializing Bird veth");
    m_associated_trex_ports = true;
    create_veths(mtu);
    set_src_mac(mac_str, mac_buf);
    bind_pair();
    m_bpf = bpfjit_compile(get_default_bpf());
    m_mcast_filter = &mcast_filter;
    m_mcast_filter->add_empty();
    m_subnet4 = 0;
    m_subnet6 = 0;
}

CBirdIfNode::~CBirdIfNode() {
    debug("Bird node dtor");
    delete_veth();
}

void CBirdIfNode::create_veths(const string &mtu) {
    CNamespacedIfNode::create_veths(mtu);
    run_in_ns("ethtool -K " + m_if_name + "-L tx off rx off sg off",
    "Could not disable rx checksum, tx checksum, and tcp segmentation for bird internal veth");

}

const char *CBirdIfNode::get_default_bpf() {
    return "";
}

void CBirdIfNode::to_json_node(Json::Value &res) {
    CNamespacedIfNode::to_json_node(res);
    res["ipv4"]["subnet"] = m_subnet4;
    res["ipv6"]["subnet"] = m_subnet6;
    res["is_bird"] = true;
}


/***************************************
*            helper func               *
***************************************/

bool is_file_exists(const string &filename) {
    struct stat buf;
    return stat(filename.data(), &buf) == 0;
}

char clean_old_nets_helper() {
    DIR *dirp;
    struct dirent *direntp;
    regex_t search_regex;
    char free_prefix = 0;
    for ( char prefix_char = 'a'; prefix_char <= 'z'; prefix_char++ ) {
        string netns_lock_name = string("/var/lock/trex_netns_") + prefix_char;

        // is lock file exists?
        bool exists = is_file_exists(netns_lock_name);
        if ( !exists ) {
            if ( free_prefix == 0 ) {
                free_prefix = prefix_char;
            }
            continue;
        }

        // is lock file locked?
        int fd = open(netns_lock_name.data(), O_RDONLY);
        if ( fd == -1 ) {
            throw TrexException("Could not open  " + netns_lock_name);
        }
        int res = flock(fd, LOCK_EX | LOCK_NB);
        if ( res ) {
            continue; // locked, try next
        }
        res = flock(fd, LOCK_UN);
        if ( res ) {
            throw TrexException("Could not unlock " + netns_lock_name);
        }

        string ns_pattern = string("trex-") + prefix_char + "-[0-9a-f]+-[0-9a-f]+";

        // remove unused veths
        struct ifaddrs *if_list, *if_iter;
        res = getifaddrs(&if_list);
        if ( res ) {
            throw TrexException("Could not get list of interfaces for cleanup");
        }
        vector<string> if_postfixes = {"T", "L"};
        for (auto &if_postfix : if_postfixes) {
            debug("Cleaning IFs with postfixes " + if_postfix);
            res = regcomp(&search_regex, ("^" + ns_pattern + "-" + if_postfix + "$").c_str(), REG_EXTENDED);
            if ( res ) {
                throw TrexException("Could not compile regex");
            }
            for (if_iter = if_list; if_iter; if_iter = if_iter->ifa_next) {
                if (if_iter->ifa_addr == nullptr) {
                   continue;
                }
                res = regexec(&search_regex, if_iter->ifa_name, 0, NULL, 0);
                if ( res ) {
                    continue;
                }
                popen_with_err("ip link delete " + string(if_iter->ifa_name), "Could not remove old veth");
            }
            regfree(&search_regex);
        }
        freeifaddrs(if_list);

        // remove unused namespace
        string read_dir = "/var/run/netns";
        dirp = opendir(read_dir.c_str());
        if ( dirp != nullptr ) {

            res = regcomp(&search_regex, ("^" + ns_pattern + "$").c_str(), REG_EXTENDED);
            if ( res ) {
                throw TrexException("Could not compile regex");
            }

            while (true) {
                direntp = readdir(dirp);
                if ( direntp == nullptr ) {
                    break;
                }
                res = regexec(&search_regex, direntp->d_name, 0, NULL, 0);
                if ( res ) {
                    continue;
                }
                popen_with_err("ip netns delete " + string(direntp->d_name), "Could not remove old namespace");
            }
            regfree(&search_regex);
            closedir(dirp);
        }

        close(fd);
        unlink(netns_lock_name.data());
        if ( free_prefix == 0 ) {
            free_prefix = prefix_char;
        }
    }
    if ( free_prefix == 0 ) {
        throw TrexException("Could not determine prefix for Linux-based stack, everything from 'a' to 'z' is in use.");
    }
    return free_prefix;
}

int lock_cleanup() {
    string lock_cleanup_file = "/var/lock/trex_cleanup";
    debug("Locking cleanup file " + lock_cleanup_file);
    int cleanup_fd = open(lock_cleanup_file.data(), O_RDONLY | O_CREAT, 0600);
    if ( cleanup_fd == -1 ) {
        throw TrexException("Could not open/create " + lock_cleanup_file);
    }
    int res = flock(cleanup_fd, LOCK_EX); // blocks waiting for our lock
    if ( res ) {
        throw TrexException("Could not lock cleanup file " + lock_cleanup_file);
    }
    return cleanup_fd;
}

char clean_old_nets_and_get_prefix() {
    uint16_t timeout_sec = 1000;

    future<int> lock_thread_handle = async(launch::async, lock_cleanup);
    future_status thread_status = lock_thread_handle.wait_for(chrono::seconds(timeout_sec));
    if ( thread_status == future_status::timeout ) {
        printf("Timeout of %u seconds on waiting for cleanup of old namespaces/veths\n", timeout_sec);
        printf("Try removing manually:\n");
        printf("  * namespaces starting with \"trex-\" (ip netns list)\n");
        printf("  * veth interfaces starting with \"trex-\" (ip link)\n");
        throw TrexException("Could not cleanup old namespaces/veths");
    }
    int cleanup_fd;
    try {
        cleanup_fd = lock_thread_handle.get();
    } catch (const TrexException &ex) {
        throw TrexException("Could not lock for cleanup: " + string(ex.what()));
    }

    printf("Cleanup of old namespaces related to Linux-based stack\n");
    future<char> cleanup_thread_handle = async(launch::async, clean_old_nets_helper);
    thread_status = cleanup_thread_handle.wait_for(chrono::seconds(timeout_sec));
    if ( thread_status == future_status::timeout ) {
        printf("Timeout of %u seconds on waiting for cleanup of old namespaces/veths\n", timeout_sec);
        printf("Try removing manually:\n");
        printf("  * namespaces starting with \"trex-\" (ip netns list)\n");
        printf("  * veth interfaces starting with \"trex-\" (ip link)\n");
        throw TrexException("Could not cleanup old namespaces/veths");
    }
    char prefix_char;
    try {
        prefix_char = cleanup_thread_handle.get();
    } catch (const TrexException &ex) {
        flock(cleanup_fd, LOCK_UN);
        throw TrexException("Could not cleanup old namespaces/veths, error: " + string(ex.what()));
    }

    // lock the netns file
    string netns_lock_name = string("/var/lock/trex_netns_") + prefix_char;
    int netns_fd = open(netns_lock_name.c_str(), O_RDONLY | O_CREAT, 0600);
    if ( netns_fd == -1 ) {
        throw TrexException("Could not open  " + netns_lock_name);
    }
    int res = flock(netns_fd, LOCK_EX | LOCK_NB);
    if ( res ) {
        throw TrexException("Could not lock file " + netns_lock_name);
    }

    // unlock cleanup file
    res = flock(cleanup_fd, LOCK_UN);
    if ( res ) {
        throw TrexException("Could not unlock cleanup file");
    }
    printf("Cleanup Done\n");
    return prefix_char;
}

void verify_programs() {
    // ensure sbin(s) are in path
    string path = getenv("PATH");
    setenv("PATH", ("/sbin:/usr/sbin:/usr/local/sbin/:" + path).c_str(), 1);

    vector<vector<string>> cmds = {{"ip", "ip -V"}, {"sysctl", "sysctl -V"}, {"ethtool", "ethtool --version"}};
    for (auto &cmd : cmds) {
        popen_with_err(cmd[1], "Could not find program \"" + cmd[0] + "\", which is required for Linux-based stack");
    }
}

void str_from_mbuf(const rte_mbuf_t *m, string &result) {
    while (m) {
        result.append((char*)m->buf_addr + m->data_off, m->data_len);
        m = m->next;
    }
}

void popen_general(const string &cmd, const string &err, bool throw_exception, string &output) {
    string cmd_with_redirect = cmd + " 2>&1";
    debug("stack going to run: " + cmd);
    FILE *fstream = popen(cmd_with_redirect.c_str(), "r");
    if ( fstream == nullptr && throw_exception ) {
        throw TrexException(err + " (popen could not allocate memory to execute cmd: " + cmd + ").");
    }
    char buffer[1024];
    while ( fgets(buffer, sizeof(buffer), fstream) != nullptr ) {
        output += buffer;
    }
    int ret = pclose(fstream);
    if ( ret && throw_exception ) {
        if ( WIFEXITED(ret) ) {
            throw TrexException(err + "\nCmd: " + cmd + "\nReturn code: " + to_string(WEXITSTATUS(ret)) + "\nOutput: " + output + "\n");
        } else {
            throw TrexException(err + "\nCmd: " + cmd + "\nOutput: " + output + "\n");
        }
    }
}


void popen_with_output(const string &cmd, const string &err, bool throw_exception, string &output) {
    popen_general(cmd, err, throw_exception, output);
}

void popen_with_err(const string &cmd, const string &err) {
    string out = "";
    popen_general(cmd, err, true, out);
}



/*
typedef struct {
    void (CLinuxNamespace::*func)();
    CLinuxNamespace *obj;
} cloned_args_t;

int CNamespacedIfNodes::start_resolve(nodes_set_t &nodes) {
    unordered_map<CLinuxNamespace*,set<uint8_t*>> ips_per_namespace;
    // resolve only unique IP/namespace pairs
    for (auto &node : nodes) {
        ips_per_namespace[node->m_namespace].insert(node->m_ip4_src);
    }
    for (auto &ns : ips_per_namespace) {
        for (auto &ip : ns.second) {
            // resolve here
            (void)ip;
        }
    }
    return 0;
}

void CLinuxNamespace::run_in_ns(void (CLinuxNamespace::*func)()) {
    m_child_done = false;
    cloned_args_t cloned_args;
    cloned_args.func = func;
    cloned_args.obj = this;

    int pid = clone(run_in_cloned, m_stack + STACK_SIZE, CLONE_NEWNET, (void*) &cloned_args);
    if (pid == -1) {
        printf("clone failed\n");
        exit(1);
    }
    while ( !m_child_done ) {
        printf("waiting for child\n");
    }
    printf("done\n");
}

int CLinuxNamespace::run_in_cloned(void *cloned_args_void) {
    cloned_args_t *cloned_args = (cloned_args_t*) cloned_args_void;

    string ns_path = "/var/run/netns/" + cloned_args->obj->m_name;
    int fd = open(ns_path.c_str(), O_RDONLY);
    if (fd == -1) {
        printf("open ns error\n");
        exit(1);
    }
    if (setns(fd, CLONE_NEWNET) == -1) {
        printf("netns error\n");
        exit(1);
    }
    (cloned_args->func)();
    cloned_args->obj->m_child_done = true;
    return 0;
}
*/
