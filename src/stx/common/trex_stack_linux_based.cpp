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

#include <regex.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <fcntl.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <future>


void clean_old_nets(void);
void str_from_mbuf(const rte_mbuf_t *m, string &result);
void system_with_err(const string &cmd, const string &err);

/***************************************
*          CStackLinuxBased            *
***************************************/

bool CStackLinuxBased::m_is_initialized = false;
string CStackLinuxBased::m_mtu = "";

CStackLinuxBased::CStackLinuxBased(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) : CStackBase(api, ignore_stats) {
    if ( !m_is_initialized ) {
        m_mtu = to_string(MAX_PKT_ALIGN_BUF_9K);
        clean_old_nets();
        m_is_initialized = true;
    }
}

CStackLinuxBased::~CStackLinuxBased(void) {
    debug("Linux stack dtor");
}

void CStackLinuxBased::handle_pkt(const rte_mbuf_t *m) {
    if ( (m->data_len >= 6) && (m->pkt_len <= MAX_PKT_ALIGN_BUF_9K) ) {
        char *seg_ptr = (char*)m->buf_addr + m->data_off;
        if ( seg_ptr[0] & 1 ) {
            debug("Linux handle_pkt: got broadcast or multicast");
            for (auto &iter_pair : m_nodes ) {
                uint16_t sent_len = ((CLinuxIfNode*)iter_pair.second)->filter_and_send(m);
                debug({"sent:", to_string(sent_len)});
            }
        } else {
            string mac_dst(seg_ptr, 6);
            debug("Linux handle_pkt: got unicast");
            CLinuxIfNode *node = (CLinuxIfNode *)get_node(mac_dst);
            if ( node ) {
                uint16_t sent_len = node->filter_and_send(m);
                debug({"sent:", to_string(sent_len)});
            } else {
                debug({"Linux handle_pkt: did not find node with mac:", utl_macaddr_to_str((uint8_t *)mac_dst.data())});
            }
        }
    }
}

uint16_t CStackLinuxBased::handle_tx(uint16_t limit) {
    string read_buf_str;
    uint16_t cnt_pkts = 0;
    if ( poll(m_pollfds.data(), m_pollfds.size(), 0) > 0 ) {
        for (auto &pollfd: m_pollfds) {
            if ( pollfd.revents == POLLIN ) {
                uint16_t pkt_len = recv(pollfd.fd, m_rw_buf, MAX_PKT_ALIGN_BUF_9K, MSG_DONTWAIT);
                if ( pkt_len <= MAX_PKT_ALIGN_BUF_9K ) {
                    debug({"Linux handle_tx: pkt len:", to_string(pkt_len)});
                    read_buf_str.assign(m_rw_buf, pkt_len);
                    m_api->tx_pkt(read_buf_str);
                    cnt_pkts++;
                }
            }
            if ( limit == cnt_pkts ) {
                break;
            }
        }
    }
    return cnt_pkts;
}

CNodeBase* CStackLinuxBased::add_node_internal(const std::string &mac_buf) {
    string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
    if ( get_node_internal(mac_buf) != nullptr ) {
        throw TrexException("Node with MAC " + mac_str + " already exists!");
    }
    CNodeBase *node = new CLinuxIfNode(m_api->get_port_id(), m_next_namespace_id, mac_str, mac_buf, m_mtu);
    if (node == nullptr) {
        throw TrexException("Could not create node " + mac_str + "!");
    }
    m_next_namespace_id++;
    m_nodes[mac_buf] = node;
    struct pollfd m_pfd;
    m_pfd.fd = ((CLinuxIfNode*)node)->get_pair_id();
    m_pfd.events = POLLIN;
    m_pfd.revents = 0;
    m_pollfds.emplace_back(m_pfd);
    return node;
}

void CStackLinuxBased::del_node_internal(const std::string &mac_buf) {
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair == m_nodes.end() ) {
        string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
        throw TrexException("Node with MAC " + mac_str + " does not exist!");
    }
    delete iter_pair->second;
    m_nodes.erase(iter_pair->first);
}

uint16_t CStackLinuxBased::get_capa(void) {
    return (CLIENTS);
}

/***************************************
*            CLinuxIfNode              *
***************************************/

CLinuxIfNode::CLinuxIfNode(uint8_t port_id, uint64_t ns_id, const string &mac_str, const string &mac_buf, const string &mtu) {
    debug("Linux node ctor");
    string &prefix = CGlobalInfo::m_options.prefix;
    if ( prefix.size() ) {
        m_ns_name = "trex-" + prefix + "-" + to_string(port_id) + "-" + to_string(ns_id);
    } else {
        m_ns_name = "trex-" + to_string(port_id) + "-" + to_string(ns_id);
    }
    create_net(mtu);
    set_src_mac(mac_str, mac_buf);
    bind_pair();
    // TODO: add and remove vlans when they will be supported in this stack
    m_bpf = bpfjit_compile("not udp and not tcp");
}

CLinuxIfNode::~CLinuxIfNode() {
    debug("Linux node dtor");
    delete_net();
}

void CLinuxIfNode::run_in_ns(const string &cmd, const string &err) {
    system_with_err("ip netns exec " + m_ns_name + " " + cmd, err);
}

uint16_t CLinuxIfNode::filter_and_send(const rte_mbuf_t *m) {
    string pkt;
    str_from_mbuf(m, pkt);
    bool rc = bpfjit_run(m_bpf, pkt.c_str(), pkt.size());
    if ( !rc ) {
        debug("Packet filtered out by BPF");
        return 0;
    }
    debug("Packet was NOT filtered by BPF");
    return send(m_pair_id, pkt.c_str(), pkt.size(), MSG_DONTWAIT);
}

void CLinuxIfNode::create_net(const string &mtu) {
    // netns
    system_with_err("ip netns add " + m_ns_name, "Could not create network namespace");
    // veths
    system_with_err("ip link add " + m_ns_name + "-T type veth peer name " + m_ns_name + "-L", "Could not create veth pair");
    system_with_err("ip link set " + m_ns_name + "-T mtu " + mtu + " up", "Could not configure veth");
    system_with_err("ip link set " + m_ns_name + "-L netns " + m_ns_name, "Could not add veth to namespace");
    run_in_ns("ip link set " + m_ns_name + "-L mtu " + mtu + " up", "Could not configure veth");
}

void CLinuxIfNode::delete_net(void) {
    system_with_err("ip link delete " + m_ns_name + "-T", "Could not delete veth");
    system_with_err("ip netns delete " + m_ns_name, "Could not delete network namespace");
}

void CLinuxIfNode::bind_pair(void) {
    int sockfd;
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if ( sockfd < 0 ) {
        throw TrexException("Could not open new node");
    }

    string if_name = m_ns_name + "-T";
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

void CLinuxIfNode::set_src_mac(const string &mac_str, const string &mac_buf) {
    run_in_ns("ip link set dev " + m_ns_name + "-L address " + mac_str, "Could not set MAC address for veth");
    m_src_mac = mac_buf;
}

void CLinuxIfNode::clear_ip4_internal(void) {
    run_in_ns("ip -4 addr flush dev " + m_ns_name + "-L", "Could not flush IPv4 for veth");
    m_ip4.clear();
    m_gw4.clear();
}

void CLinuxIfNode::conf_ip4_internal(const string &ip4_buf, const string &gw4_buf) {
    clear_ip4_internal();
    char buf[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, ip4_buf.c_str(), buf, INET_ADDRSTRLEN);
    string ip4_str(buf);
    run_in_ns("ip -4 addr add " + ip4_str + "/32 dev " + m_ns_name + "-L", "Could not set IPv4 for veth");

    inet_ntop(AF_INET, gw4_buf.c_str(), buf, INET_ADDRSTRLEN);
    string gw4_str(buf);

    run_in_ns("ip -4 route add " + gw4_str + " dev " + m_ns_name + "-L", "Could not add route to default IPv4 gateway from veth");
    run_in_ns("ip -4 route add default via " + gw4_str, "Could not set default IPv4 gateway for veth");
    m_ip4 = ip4_buf;
    m_gw4 = gw4_buf;
}

void CLinuxIfNode::clear_ip6_internal(void) {
    run_in_ns("ip -6 addr flush dev " + m_ns_name + "-L", "Could not flush IPv6 for veth");
    m_ip6.clear();
    m_gw6.clear();
}

void CLinuxIfNode::conf_ip6_internal(const string &ip6_buf, const string &gw6_buf) {
    clear_ip6_internal();
    char buf[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET6, ip6_buf.c_str(), buf, INET6_ADDRSTRLEN);
    string ip6_str(buf);
    run_in_ns("ip -6 addr add " + ip6_str + "/128 dev " + m_ns_name + "-L", "Could not set IPv6 for veth");

    inet_ntop(AF_INET6, gw6_buf.c_str(), buf, INET6_ADDRSTRLEN);
    string gw6_str(buf);
    run_in_ns("ip -6 route add " + gw6_str + " dev " + m_ns_name + "-L", "Could not add route to default IPv6 gateway from veth");
    run_in_ns("ip -6 route add default via " + gw6_str, "Could not set default IPv6 gateway for veth");
    m_ip6 = ip6_buf;
    m_gw6 = gw6_buf;
}

int CLinuxIfNode::get_pair_id(void) {
     return m_pair_id;
}


/***************************************
*            helper func               *
***************************************/

void clean_old_nets_helper(void) {
    DIR *dirp;
    struct dirent *direntp;
    regex_t search_regex;
    string &prefix = CGlobalInfo::m_options.prefix;
    string ns_pattern;
    int rc;
    if ( prefix.size() ) {
        ns_pattern = "trex-" + prefix + "-[0-9]+-[0-9]+";
    } else {
        ns_pattern = "trex-[0-9]+-[0-9]+";
    }
    delay(10000);

    // remove old namespaces
    string read_dir = "/var/run/netns";
    dirp = opendir(read_dir.c_str());
    if ( dirp != nullptr ) {
    
        rc = regcomp(&search_regex, string("^" + ns_pattern + "$").c_str(), REG_EXTENDED);
        if ( rc ) {
            throw TrexException("Could not compile regex");
        }

        while (true) {
            direntp = readdir(dirp);
            if ( direntp == nullptr ) {
                break;
            }
            rc = regexec(&search_regex, direntp->d_name, 0, NULL, 0);
            if ( rc ) {
                continue;
            }
            system_with_err("ip netns delete " + string(direntp->d_name), "Could not remove old namespace");
        }
    }

    // remove old veths
    read_dir = "/sys/class/net";
    vector<string> if_postfixes = {"T", "L"};
    for (auto &if_postfix : if_postfixes) {
        debug("Cleaning IFs with postfixes " + if_postfix);
        dirp = opendir(read_dir.c_str());
        if ( dirp == nullptr ) {
            throw TrexException("Could not read interfaces directory " + read_dir);
        }
        rc = regcomp(&search_regex, string(ns_pattern + "-" + if_postfix + "$").c_str(), REG_EXTENDED);
        if ( rc ) {
            throw TrexException("Could not compile regex");
        }

        while (true) {
            direntp = readdir(dirp);
            if ( direntp == nullptr ) {
                break;
            }
            rc = regexec(&search_regex, direntp->d_name, 0, NULL, 0);
            if ( rc ) {
                continue;
            }
            system_with_err("ip link delete " + string(direntp->d_name), "Could not remove old veth");
        }
    }
}

void clean_old_nets(void) {
    uint8_t timeout_sec = 5;
    printf("Cleanup of old namespaces related to Linux-based stack\n");
    future<void> thread_handle = async(launch::async, clean_old_nets_helper);
    future_status thread_status = thread_handle.wait_for(chrono::seconds(timeout_sec));
    if ( thread_status == future_status::timeout ) {
        printf("Timeout of %u seconds on waiting for cleanup of old namespaces/veths\n", timeout_sec);
        printf("Try removing manually:\n");
        printf("  * namespaces starting with \"trex-\" (ip netns list)\n");
        printf("  * veth interfaces starting with \"trex-\" (ip link)\n");
        throw TrexException("Could not cleanup old namespaces/veths");
    }
    try {
        thread_handle.get();
    } catch (const TrexException &ex) {
        throw TrexException("Could not cleanup old namespaces/veths, error: " + string(ex.what()));
    }
}

void str_from_mbuf(const rte_mbuf_t *m, string &result) {
    while (m) {
        result.append((char*)m->buf_addr + m->data_off, m->data_len);
        m = m->next;
    }
}

void system_with_err(const string &cmd, const string &err) {
    string cmd_without_output = cmd + " &> /dev/null";
    debug("stack going to run: " + cmd);
    int ret = system(cmd_without_output.c_str());
    if ( ret ) {
        debug("stack command: " + cmd + "(exit status: " + to_string(ret) + ")");
        throw TrexException(err + " (" + cmd + ")");
    }
}



/*
typedef struct {
    void (CLinuxNamespace::*func)();
    CLinuxNamespace *obj;
} cloned_args_t;

int CLinuxIfNodes::start_resolve(nodes_set_t &nodes) {
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
