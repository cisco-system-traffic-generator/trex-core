/*

 Wenxian Li
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#include <string.h>
#include "utl_yaml.h"
#include "bp_sim.h"
#include "tuple_gen.h"
#include "trex_exception.h"

static bool _enough_ips(uint32_t total_ip,
                       double active_flows,
                        bool rss_astf_mode){
    if (rss_astf_mode){
        /* don't do the optimization in case of ASTF RSS mode */
        return(false);
    }
    /* Socket utilization is lower than 20% */
    if ( (active_flows/((double)total_ip*(double)MAX_PORT))>0.10 ) {
        return(false);
    }
    return (true);
}

void CServerPool::Create(IP_DIST_t  dist_value,
            uint32_t min_ip,
            uint32_t max_ip,
            double active_flows
            ) {
    CServerPoolBase::set_thread_id(0);
    gen = new CIpPool();
    gen->set_dist(dist_value);
    uint32_t total_ip = max_ip - min_ip +1;
    gen->m_ip_info.resize(total_ip);

    if ( _enough_ips(total_ip,active_flows,false) ) {
        for(int idx=0;idx<total_ip;idx++){
            gen->m_ip_info[idx] = new CServerInfoL();
            gen->m_ip_info[idx]->set_ip(min_ip+idx);
        }
    } else {
        for(int idx=0;idx<total_ip;idx++){
            gen->m_ip_info[idx] = new CServerInfo();
            gen->m_ip_info[idx]->set_ip(min_ip+idx);
        }
    }
    gen->CreateBase();
}



void CClientPool::Create(IP_DIST_t       dist_value,
                         uint32_t        min_ip,
                         uint32_t        max_ip,
                         double          active_flows,
                         ClientCfgDB     &client_info,
                         uint16_t        tcp_aging,
                         uint16_t        udp_aging) {

    assert(max_ip >= min_ip);

    set_dist(dist_value);

    uint32_t total_ip  = max_ip - min_ip +1;
    bool is_long_range = _enough_ips(total_ip,active_flows,m_rss_astf_mode);

    m_ip_info.resize(total_ip);

    /* if client info is empty - flat allocation o.w use configured clients */
    if (client_info.is_empty()) {
        allocate_simple_clients(min_ip, total_ip, is_long_range);
    } else {
        allocate_configured_clients(min_ip, total_ip, is_long_range, client_info);
    }

    m_tcp_aging = tcp_aging;
    m_udp_aging = udp_aging;
    CreateBase(); 
}


/* base on thread_id and client_index 
  client index would be the same for all thread 
*/
static uint16_t generate_rand_sport(uint32_t client_index,
                                    uint16_t threadid,
                                    uint16_t rss_thread_id,
                                    uint16_t rss_thread_max,
                                    uint8_t  reta_mask,
                                    bool     rss_astf_mode){

    uint32_t rand  = ((214013*(client_index + 11213*threadid) + 2531011));
    uint16_t rand16 = ((rand)&0xffff);
    uint16_t port = MIN_PORT+(rand16%(MAX_PORT-MIN_PORT))+1;

    if (rss_astf_mode) {
        port = rss_align_lsb(port,rss_thread_id,rss_thread_max,reta_mask);
    }
    return (port);
}

/**
 * simple allocation of a client - no configuration was provided
 * 
 * @author imarom (27-Jun-16)
 * 
 * @param ip 
 * @param index 
 * @param is_long_range 
 */
void CClientPool::allocate_simple_clients(uint32_t  min_ip,
                                          uint32_t  total_ip,
                                          bool      is_long_range) {

    /* simple creation of clients - no extended info */
    for (uint32_t i = 0; i < total_ip; i++) {
        uint32_t ip = min_ip + i;
        if (is_long_range) {
            m_ip_info[i] = new CSimpleClientInfo<CIpInfoL>(ip);
        } else {
            m_ip_info[i] = new CSimpleClientInfo<CIpInfo>(ip);
        }
        configure_client(i);
    }

}


void CClientPool::configure_client(uint32_t indx){

    uint16_t port = generate_rand_sport(indx,m_thread_id,
                                             m_rss_thread_id,
                                             m_rss_thread_max,
                                             m_reta_mask,
                                             m_rss_astf_mode);
    CIpInfoBase* lp=m_ip_info[indx];
    lp->set_start_port(port);
    if (m_rss_astf_mode){
        lp->set_min_port(rss_align_lsb(MIN_PORT,m_rss_thread_id,m_rss_thread_max,m_reta_mask));
        lp->set_inc_port(m_rss_thread_max);
        lp->set_sport_reverse_lsb(true,m_reta_mask);
    }
}


/**
 * simple allocation of a client - no configuration was provided
 * 
 * @author imarom (27-Jun-16)
 * 
 * @param ip 
 * @param index 
 * @param is_long_range 
 */
void CClientPool::allocate_configured_clients(uint32_t        min_ip,
                                              uint32_t        total_ip,
                                              bool            is_long_range,
                                              ClientCfgDB     &client_info) {

    printf("allocate_configured_clients\n");
    for (uint32_t i = 0; i < total_ip; i++) {
        uint32_t ip = min_ip + i;

        /* lookup for the right group of clients */
        ClientCfgEntry *group = client_info.lookup(ip);
        if (!group) {
            throw TrexException("Client configuration error - no group containing IP: " + ip_to_str(ip));
        }

        ClientCfgBase info;
        group->assign(info, ip);

        if (is_long_range) {
            m_ip_info[i] = new CConfiguredClientInfo<CIpInfoL>(ip, info);
        } else {
            m_ip_info[i] = new CConfiguredClientInfo<CIpInfo>(ip, info);
        }
        configure_client(i);
    }
}


bool CTupleGeneratorSmart::add_client_pool(IP_DIST_t      client_dist,
                                          uint32_t        min_client,
                                          uint32_t        max_client,
                                          double          active_flows,
                                          ClientCfgDB     &client_info,
                                          uint16_t        tcp_aging,
                                          uint16_t        udp_aging) {
    assert(max_client>=min_client);
    CClientPool* pool = new CClientPool();
    pool->set_thread_id((uint16_t)m_thread_id);
    if (m_rss_astf_mode) {
        pool->set_rss_thread_id(m_rss_thread_id,m_rss_thread_max,m_reta_mask);
    }
    pool->Create(client_dist,
                 min_client,
                 max_client,
                 active_flows,
                 client_info,
                 tcp_aging,
                 udp_aging);
    m_client_pool.push_back(pool);
    return(true);
}

bool CTupleGeneratorSmart::add_server_pool(IP_DIST_t  server_dist,
                                          uint32_t min_server,
                                          uint32_t max_server,
                                          double   active_flows,
                                          bool is_bundling){
    assert(max_server>=min_server);
    CServerPoolBase* pool;
    if (is_bundling) 
        pool = new CServerPool();
    else
        pool = new CServerPoolSimple();
    // we currently only supports mac mapping file for client
    pool->set_thread_id((uint16_t)m_thread_id);
    pool->Create(server_dist, min_server, max_server,active_flows);

    m_server_pool.push_back(pool);
    return(true);
}



bool CTupleGeneratorSmart::Create(uint32_t _id,
                                  uint32_t thread_id)
                                  
{
    m_thread_id     = thread_id;
    m_id = _id;
    m_was_init=true;
    m_rss_thread_id=0;
    m_rss_thread_max=0;
    m_rss_astf_mode=false;
    return(true);
}

void CTupleGeneratorSmart::Delete(){
    m_was_init=false;

    for (int idx=0;idx<m_client_pool.size();idx++) {
        m_client_pool[idx]->Delete();
        delete m_client_pool[idx];
    }
    m_client_pool.clear();

    for (int idx=0;idx<m_server_pool.size();idx++) {
        m_server_pool[idx]->Delete();
        delete m_server_pool[idx];
    }
    m_server_pool.clear();
}

void CTupleGenYamlInfo::dump(FILE *fd) {
    fprintf(fd, "Client pools:\n");
    for (int i=0; i < m_client_pool.size(); i++) {
        m_client_pool[i].Dump(fd);
    }
    fprintf(fd, "Server pools:\n");
    for (int i=0; i < m_server_pool.size(); i++) {
        m_server_pool[i].Dump(fd);
    }
}

// Find out matching port for given ip range.
// If found, port is returned in port, otherwise port is set to UINT8_MAX
// Return false in case of error. True otherwise. Port not found is not considered error.
bool CTupleGenYamlInfo::find_port(uint32_t ip_start, uint32_t ip_end, uint8_t &port) {
    uint8_t num_ports = CGlobalInfo::m_options.get_expected_ports();

    for (int i=0; i < m_client_pool.size(); i++) {
            CTupleGenPoolYaml &pool = m_client_pool[i];
            uint32_t pool_start = pool.get_ip_start();
            uint32_t pool_end = pool.get_ip_end();
            uint32_t pool_offset = pool.getDualMask();
            for (uint8_t port_id = 0; port_id < num_ports; port_id += 2) {
                uint32_t pool_port_start = pool_start + pool_offset * port_id / 2;
                uint32_t pool_port_end = pool_end + pool_offset * port_id / 2;
                if ((ip_start >= pool_port_start) &&  (ip_start <= pool_port_end)) {
                    if ((ip_end >= pool_port_start) &&  (ip_end <= pool_port_end)) {
                        port = port_id;
                        return true;
                    } else {
                        // ip_start in range, ip_end not
                        fprintf(stderr, "Error for range %s - %s. Start is inside range %s - %s, but end is outside\n"
                                , ip_to_str(ip_start).c_str(), ip_to_str(ip_end).c_str()
                                , ip_to_str(pool_port_start).c_str(), ip_to_str(pool_port_end).c_str());
                                port = UINT8_MAX;
                                return false;
                    }
                }
            }
        }

        for (int i=0; i < m_server_pool.size(); i++) {
            CTupleGenPoolYaml &pool = m_server_pool[i];
            uint32_t pool_start = pool.get_ip_start();
            uint32_t pool_end = pool.get_ip_end();
            uint32_t pool_offset = pool.getDualMask();
            for (uint8_t port_id = 1; port_id < num_ports; port_id += 2) {
                uint32_t pool_port_start = pool_start + pool_offset * (port_id - 1) / 2;
                uint32_t pool_port_end = pool_end + pool_offset * (port_id - 1)/ 2;
                if ((ip_start >= pool_port_start) &&  (ip_start <= pool_port_end)) {
                    if ((ip_end >= pool_port_start) &&  (ip_end <= pool_port_end)) {
                        port = port_id;
                        return true;
                    } else {
                        fprintf(stderr, "Error for range %s - %s. Start is inside range %s - %s, but end is outside\n"
                                , ip_to_str(ip_start).c_str(), ip_to_str(ip_end).c_str()
                                , ip_to_str(pool_port_start).c_str(), ip_to_str(pool_port_end).c_str());
                        // ip_start in range, ip_end not
                        port = UINT8_MAX;
                        return false;
                    }
                }
            }
        }

        port = UINT8_MAX;
        return true;
}

void CTupleGenPoolYaml::Dump(FILE *fd){
    fprintf(fd," Pool %s:\n", (m_name.size() == 0) ? "default":m_name.c_str());
    fprintf(fd,"  dist           : %d \n",m_dist);
    fprintf(fd,"  IPs            : %s - %s \n",ip_to_str(m_ip_start).c_str(), ip_to_str(m_ip_end).c_str());
    fprintf(fd,"  dual_port_mask : %s \n",ip_to_str(m_dual_interface_mask).c_str());
    fprintf(fd,"  clients per gb : %d  \n",m_number_of_clients_per_gb);
    fprintf(fd,"  min clients    : %d  \n",m_min_clients);
    fprintf(fd,"  tcp aging      : %d sec \n",m_tcp_aging_sec);
    fprintf(fd,"  udp aging      : %d sec \n",m_udp_aging_sec);
}

bool CTupleGenPoolYaml::is_valid(uint32_t num_threads,bool is_plugins){
    if ( m_ip_start > m_ip_end ){
        printf(" ERROR The ip_start must be bigger than ip_end \n");
        return(false);
    }
    
    uint32_t ips= (m_ip_end - m_ip_start +1);
    if ( ips < num_threads ) {
        printf(" ERROR The number of ips should be at least number of threads %d \n",num_threads);
        return (false);
    }

    if (ips > 1000000) {
        printf("  The number of clients requested is %d maximum supported : %d \n",ips,1000000);
        return (false);
    }
    return (true);
}

#define UTL_YAML_READ(type, field, target) if (node.FindValue(#field)) { \
    utl_yaml_read_ ## type (node, #field , target); \
    } else { printf("generator definition mising " #field "\n"); } 

#define UTL_YAML_READ_NO_MSG(type, field, target) if (node.FindValue(#field)) { \
    utl_yaml_read_ ## type (node, #field , target); \
    } 

IP_DIST_t convert_distribution (const YAML::Node& node) {
    std::string tmp;
    node["distribution"] >> tmp ;
    if (tmp == "random") {
        return cdRANDOM_DIST;
    }else if (tmp == "normal") {
        return cdNORMAL_DIST;
    } else {
        return cdSEQ_DIST;
    }
}

void read_tuple_para(const YAML::Node& node, CTupleGenPoolYaml & fi) {
    UTL_YAML_READ_NO_MSG(uint32, clients_per_gb, fi.m_number_of_clients_per_gb);
    UTL_YAML_READ_NO_MSG(uint32, min_clients, fi.m_min_clients);
    UTL_YAML_READ_NO_MSG(ip_addr, dual_port_mask, fi.m_dual_interface_mask);
    UTL_YAML_READ_NO_MSG(uint16, tcp_aging, fi.m_tcp_aging_sec);
    UTL_YAML_READ_NO_MSG(uint16, udp_aging, fi.m_udp_aging_sec);
}

void operator >> (const YAML::Node& node, CTupleGenPoolYaml & fi) {
    if (node.FindValue("name")) {
        node["name"] >> fi.m_name;
    } else {
        printf("error in generator definition, name missing\n");
        assert(0);
    }
    if (node.FindValue("distribution")) {
        fi.m_dist = convert_distribution(node); 
    } 
    UTL_YAML_READ(ip_addr, ip_start, fi.m_ip_start);
    UTL_YAML_READ(ip_addr, ip_end, fi.m_ip_end);

    fi.m_number_of_clients_per_gb = 0;
    fi.m_min_clients = 0;
    fi.m_is_bundling = false;
    fi.m_per_core_distro =false;
    fi.m_tcp_aging_sec = 0;
    fi.m_udp_aging_sec = 0;
    fi.m_dual_interface_mask = 0;
    read_tuple_para(node, fi);
    if (node.FindValue("track_ports")) {
        node["track_ports"] >> fi.m_is_bundling;
    }
}
void copy_global_pool_para(CTupleGenPoolYaml & src, CTupleGenPoolYaml & dst) {
    if (src.m_number_of_clients_per_gb == 0)
        src.m_number_of_clients_per_gb = dst.m_number_of_clients_per_gb;
    if (src.m_min_clients == 0)
        src.m_min_clients = dst.m_min_clients;
    if (src.m_dual_interface_mask == 0)
        src.m_dual_interface_mask = dst.m_dual_interface_mask;
    if (src.m_tcp_aging_sec == 0)
        src.m_tcp_aging_sec = dst.m_tcp_aging_sec;
    if (src.m_udp_aging_sec == 0)
        src.m_udp_aging_sec = dst.m_udp_aging_sec;
}


void operator >> (const YAML::Node& node, CTupleGenYamlInfo & fi) {

    CTupleGenPoolYaml c_pool;
    CTupleGenPoolYaml s_pool;
    
    if (node.FindValue("distribution")) {
        c_pool.m_dist = convert_distribution(node); 
        s_pool.m_dist = c_pool.m_dist;
        UTL_YAML_READ(ip_addr, clients_start, c_pool.m_ip_start);
        UTL_YAML_READ(ip_addr, clients_end, c_pool.m_ip_end);
        UTL_YAML_READ(ip_addr, servers_start, s_pool.m_ip_start);
        UTL_YAML_READ(ip_addr, servers_end, s_pool.m_ip_end);
        read_tuple_para(node, c_pool);
        s_pool.m_dual_interface_mask = c_pool.m_dual_interface_mask;
        s_pool.m_is_bundling = false;
        s_pool.m_per_core_distro =false;
        fi.m_client_pool.push_back(c_pool);
        fi.m_server_pool.push_back(s_pool);
    } else {
        printf("No default generator defined.\n");
    }
    
    if (node.FindValue("generator_clients")) {
        const YAML::Node& c_pool_info = node["generator_clients"];
        for (uint16_t idx=0;idx<c_pool_info.size();idx++) {
            CTupleGenPoolYaml pool;
            c_pool_info[idx] >> pool;
            if (fi.m_client_pool.size()>0) {
                copy_global_pool_para(pool, fi.m_client_pool[0]);
            }
            fi.m_client_pool.push_back(pool);
        }
    } else {
        printf("no client generator pool configured, using default pool\n");
    } 

    if (fi.m_client_pool.size() >= CS_MAX_POOLS) {
        printf(" ERROR pool_size: %lu is bigger than maximum %lu \n",(ulong)fi.m_client_pool.size(),(ulong)CS_MAX_POOLS);
        exit(1);
    }

    if (node.FindValue("generator_servers")) {
        const YAML::Node& s_pool_info = node["generator_servers"];
        for (uint16_t idx=0;idx<s_pool_info.size();idx++) {
            CTupleGenPoolYaml pool;
            s_pool_info[idx] >> pool;
            if (fi.m_server_pool.size()>0) {
                copy_global_pool_para(pool, fi.m_server_pool[0]);
            }
            fi.m_server_pool.push_back(pool);
        }
    } else {
        printf("no server generator pool configured, using default pool\n");
    } 

    if (fi.m_server_pool.size() >= CS_MAX_POOLS) {
        printf(" ERROR pool_size: %lu is bigger than maximum %lu \n",(ulong)fi.m_server_pool.size(),(ulong)CS_MAX_POOLS);
        exit(1);
    }

}

bool CTupleGenYamlInfo::is_valid(uint32_t num_threads,bool is_plugins){
    for (int i=0;i<m_client_pool.size();i++) {
        if (m_client_pool[i].is_valid(num_threads, is_plugins)==false) 
            return false;
    }
    for (int i=0;i<m_server_pool.size();i++) {
        if (m_server_pool[i].is_valid(num_threads, is_plugins)==false) 
            return false;
    }

    return true;
}


/* split the clients and server by dual_port_id and thread_id ,
  clients is splited by threads and dual_port_id
  servers is spliteed by dual_port_id */
void split_ips(uint32_t thread_id, 
               uint32_t total_threads, 
               uint32_t dual_port_id,
               CTupleGenPoolYaml& poolinfo,
               CIpPortion & portion){

    uint32_t chunks = poolinfo.getTotalIps()/total_threads;

    assert(chunks>0);

    uint32_t dual_if_mask=(dual_port_id*poolinfo.getDualMask());
    
    portion.m_ip_start  = poolinfo.get_ip_start()  + thread_id*chunks + dual_if_mask;
    portion.m_ip_end    = portion.m_ip_start + chunks -1 ;
}


/* split in way that each core will get continues range of ip's */

void split_ips_v2( uint32_t total_threads, 
                   uint32_t rss_thread_id,
                   uint32_t rss_max_threads,
                   uint32_t max_dual_ports, 
                   uint32_t dual_port_id,
                   CTupleGenPoolYaml& poolinfo,
                   CIpPortion & portion){

    uint32_t chunks = poolinfo.getTotalIps()/total_threads;

    assert(chunks>0);

    uint32_t dual_if_mask=(dual_port_id*poolinfo.getDualMask());

    portion.m_ip_start  = poolinfo.get_ip_start()  + (rss_thread_id+rss_max_threads*dual_port_id)*chunks + dual_if_mask;
    portion.m_ip_end    = portion.m_ip_start + chunks -1 ;
}

