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
#include <unistd.h>


char clean_old_nets_and_get_prefix(void);
void verify_programs(void);
void str_from_mbuf(const rte_mbuf_t *m, string &result);
void popen_with_err(const string &cmd, const string &err);

/***************************************
*          CStackLinuxBased            *
***************************************/

bool CStackLinuxBased::m_is_initialized = false;
string CStackLinuxBased::m_mtu = "";
string CStackLinuxBased::m_ns_prefix = "";

CStackLinuxBased::CStackLinuxBased(RXFeatureAPI *api, CRXCoreIgnoreStat *ignore_stats) : CStackBase(api, ignore_stats) {
    if ( !m_is_initialized ) {
        verify_programs();
        char prefix_char = clean_old_nets_and_get_prefix();
        m_ns_prefix = string("trex-") + prefix_char + "-";
        debug("Using netns prefix " + m_ns_prefix);
        m_mtu = to_string(MAX_PKT_ALIGN_BUF_9K);
        m_is_initialized = true;
    }
    get_platform_api().getPortAttrObj(api->get_port_id())->set_multicast(true); // We need multicast for IPv6
    m_next_namespace_id = 0;
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
                if ( pkt_len < 14 ) {
                    continue;
                }
                string src_mac(m_rw_buf + 6, 6);
                auto iter_pair = m_nodes.find(src_mac);
                if ( iter_pair == m_nodes.end() ) {
                    continue;
                }
                CLinuxIfNode *node = (CLinuxIfNode*)iter_pair->second;
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

CNodeBase* CStackLinuxBased::add_node_internal(const string &mac_buf) {
    string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
    if ( get_node_internal(mac_buf) != nullptr ) {
        throw TrexException("Node with MAC " + mac_str + " already exists");
    }
    stringstream ss;
    ss << m_ns_prefix << hex << (int)m_api->get_port_id() << "-" << m_next_namespace_id;
    CLinuxIfNode *node = new CLinuxIfNode(ss.str(), mac_str, mac_buf, m_mtu);
    if (node == nullptr) {
        throw TrexException("Could not create node " + mac_str);
    }
    m_next_namespace_id++;
    m_nodes[mac_buf] = node;
    struct pollfd m_pfd;
    m_pfd.fd = node->get_pair_id();
    m_pfd.events = POLLIN;
    m_pfd.revents = 0;
    m_pollfds.emplace_back(m_pfd);
    return node;
}

void CStackLinuxBased::del_node_internal(const string &mac_buf) {
    auto iter_pair = m_nodes.find(mac_buf);
    if ( iter_pair == m_nodes.end() ) {
        string mac_str = utl_macaddr_to_str((uint8_t *)mac_buf.data());
        throw TrexException("Node with MAC " + mac_str + " does not exist");
    }
    CLinuxIfNode *node = (CLinuxIfNode*)iter_pair->second;
    int pair_id = node->get_pair_id();
    delete node;
    m_nodes.erase(iter_pair->first);
    for (auto it = m_pollfds.begin(); it != m_pollfds.end(); it++ ) {
        if ( it->fd == pair_id ) {
            m_pollfds.erase(it);
            break;
        }
    }
}

uint16_t CStackLinuxBased::get_capa(void) {
    return (CLIENTS);
}

/***************************************
*            CLinuxIfNode              *
***************************************/

CLinuxIfNode::CLinuxIfNode(const string &ns_name, const string &mac_str, const string &mac_buf, const string &mtu) {
    debug("Linux node ctor");
    m_ns_name = ns_name;
    create_net(mtu);
    set_src_mac(mac_str, mac_buf);
    bind_pair();
    m_bpf = bpfjit_compile("not udp and not tcp");
}

CLinuxIfNode::~CLinuxIfNode() {
    debug("Linux node dtor");
    delete_net();
}

void CLinuxIfNode::run_in_ns(const string &cmd, const string &err) {
    popen_with_err("ip netns exec " + m_ns_name + " " + cmd, err);
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
    if ( m_vlans_insert_to_pkt.size() ) {
        pkt = pkt.substr(0, 12) + pkt.substr(12 + m_vlans_insert_to_pkt.size());
    }
    return send(m_pair_id, pkt.c_str(), pkt.size(), MSG_DONTWAIT);
}

void CLinuxIfNode::create_net(const string &mtu) {
    // netns
    popen_with_err("ip netns add " + m_ns_name, "Could not create network namespace");
    // veths
    popen_with_err("ip link add " + m_ns_name + "-T type veth peer name " + m_ns_name + "-L", "Could not create veth pair");
    popen_with_err("sysctl net.ipv6.conf." + m_ns_name + "-T.disable_ipv6=1", "Could not disable ipv6 for veth");
    popen_with_err("ip link set " + m_ns_name + "-T mtu " + mtu + " up", "Could not configure veth");
    popen_with_err("ip link set " + m_ns_name + "-L netns " + m_ns_name, "Could not add veth to namespace");
    run_in_ns("sysctl net.ipv6.conf." + m_ns_name + "-L.disable_ipv6=1", "Could not disable ipv6 for veth");
    run_in_ns("ip link set " + m_ns_name + "-L mtu " + mtu + " up", "Could not configure veth");
}

void CLinuxIfNode::delete_net(void) {
    popen_with_err("ip link delete " + m_ns_name + "-T", "Could not delete veth");
    popen_with_err("ip netns delete " + m_ns_name, "Could not delete network namespace");
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

void append_to_str(uint16_t num, string &str) {
    str += num >> 8;
    str += num & 0xff;
}

void CLinuxIfNode::conf_vlan_internal(const vlan_list_t &vlans) {
    string bpf_str = "";
    m_vlans_insert_to_pkt = "";
    for (auto &vlan : vlans) {
        if ( vlans.size() == 2 && !bpf_str.size() ) {
            append_to_str(EthernetHeader::Protocol::QINQ, m_vlans_insert_to_pkt);
        } else {
            append_to_str(EthernetHeader::Protocol::VLAN, m_vlans_insert_to_pkt);
        }
        append_to_str(vlan, m_vlans_insert_to_pkt);
        bpf_str += "vlan " + to_string(vlan) + " and ";
    }
    bpf_str += "not udp and not tcp";
    m_bpf = bpfjit_compile(bpf_str.c_str());
    m_vlan_tags = vlans;
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
    run_in_ns("sysctl net.ipv6.conf." + m_ns_name + "-L.disable_ipv6=1", "Could not disable ipv6 for veth");
    m_ip6_enabled = false;
    m_ip6.clear();
}

void CLinuxIfNode::conf_ip6_internal(bool enabled, const string &ip6_buf) {
    clear_ip6_internal();
    char buf[INET6_ADDRSTRLEN];
    if ( enabled ) {
        run_in_ns("sysctl net.ipv6.conf." + m_ns_name + "-L.disable_ipv6=0", "Could not enable ipv6 for veth");
        if ( ip6_buf.size() ) {
            inet_ntop(AF_INET6, ip6_buf.c_str(), buf, INET6_ADDRSTRLEN);
            string ip6_str(buf);
            run_in_ns("ip -6 addr add " + ip6_str + "/128 dev " + m_ns_name + "-L", "Could not set IPv6 for veth");
        }
    }
    m_ip6_enabled = enabled;
    m_ip6 = ip6_buf;
}

// veth pair (from TRex side)
int CLinuxIfNode::get_pair_id(void) {
    return m_pair_id;
}

// string of VLAN header(s) to insert into packet
string &CLinuxIfNode::get_vlans_insert_to_pkt(void) {
    return m_vlans_insert_to_pkt;
}

/***************************************
*            helper func               *
***************************************/

bool is_file_exists(const string &filename) {
    struct stat buf;
    return stat(filename.data(), &buf) == 0;
}

char clean_old_nets_helper(void) {
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
            for (if_iter = if_list; if_iter != nullptr; if_iter = if_iter->ifa_next) {
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

int lock_cleanup(void) {
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

char clean_old_nets_and_get_prefix(void) {
    uint8_t timeout_sec = 5;

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
    return prefix_char;
}

void verify_programs(void) {
    // ensure sbin(s) are in path
    string path = getenv("PATH");
    setenv("PATH", ("/sbin:/usr/sbin:/usr/local/sbin/:" + path).c_str(), 1);

    vector<vector<string>> cmds = {{"ip", "ip -V"}, {"sysctl", "sysctl -V"}};
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

void popen_with_err(const string &cmd, const string &err) {
    string cmd_with_redirect = cmd + " 2>&1";
    debug("stack going to run: " + cmd);
    FILE *fstream = popen(cmd_with_redirect.c_str(), "r");
    if ( fstream == nullptr ) {
        throw TrexException(err + " (popen could not allocate memory to execute cmd: " + cmd + ").");
    }
    string output = "";
    char buffer[1024];
    while ( fgets(buffer, sizeof(buffer), fstream) != nullptr ) {
        output += buffer;
    }
    int ret = pclose(fstream);
    if ( ret ) {
        if ( WIFEXITED(ret) ) {
            throw TrexException(err + "\nCmd: " + cmd + "\nReturn code: " + to_string(WEXITSTATUS(ret)) + "\nOutput: " + output + "\n");
        } else {
            throw TrexException(err + "\nCmd: " + cmd + "\nOutput: " + output + "\n");
        }
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
