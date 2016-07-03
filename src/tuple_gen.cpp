/*

 Wenxian Li
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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


#include "tuple_gen.h"
#include <string.h>
#include "utl_yaml.h"
#include "bp_sim.h"

void CServerPool::Create(IP_DIST_t  dist_value,
            uint32_t min_ip,
            uint32_t max_ip,
            double l_flow,
            double t_cps) {
    gen = new CIpPool();
    gen->set_dist(dist_value);
    uint32_t total_ip = max_ip - min_ip +1;
    gen->m_ip_info.resize(total_ip);

    if (total_ip > ((l_flow*t_cps/MAX_PORT))) {
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
                         double          l_flow,
                         double          t_cps,
                         ClientCfgDB     &client_info,
                         uint16_t        tcp_aging,
                         uint16_t        udp_aging) {

    assert(max_ip >= min_ip);

    set_dist(dist_value);

    uint32_t total_ip  = max_ip - min_ip +1;
    bool is_long_range = total_ip > (l_flow * t_cps / MAX_PORT);

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

    for (uint32_t i = 0; i < total_ip; i++) {
        uint32_t ip = min_ip + i;

        /* lookup for the right group of clients */
        ClientCfgEntry *group = client_info.lookup(ip);
        if (!group) {
            std::stringstream ss;
            ss << "client configuration error: could not map IP '" << ip_to_str(ip) << "' to a group\n";
            throw std::runtime_error(ss.str());
        }

        ClientCfg info;
        group->assign(info);

        if (is_long_range) {
            m_ip_info[i] = new CConfiguredClientInfo<CIpInfoL>(ip, info);
        } else {
            m_ip_info[i] = new CConfiguredClientInfo<CIpInfo>(ip, info);
        }
    }
}


bool CTupleGeneratorSmart::add_client_pool(IP_DIST_t      client_dist,
                                          uint32_t        min_client,
                                          uint32_t        max_client,
                                          double          l_flow,
                                          double          t_cps,
                                          ClientCfgDB     &client_info,
                                          uint16_t        tcp_aging,
                                          uint16_t        udp_aging) {
    assert(max_client>=min_client);
    CClientPool* pool = new CClientPool();
    pool->Create(client_dist,
                 min_client,
                 max_client,
                 l_flow,
                 t_cps,
                 client_info,
                 tcp_aging,
                 udp_aging);

    m_client_pool.push_back(pool);
    return(true);
}

bool CTupleGeneratorSmart::add_server_pool(IP_DIST_t  server_dist,
                                          uint32_t min_server,
                                          uint32_t max_server,
                                          double l_flow,
                                          double t_cps,
                                          bool is_bundling){
    assert(max_server>=min_server);
    CServerPoolBase* pool;
    if (is_bundling) 
        pool = new CServerPool();
    else
        pool = new CServerPoolSimple();
    // we currently only supports mac mapping file for client
    pool->Create(server_dist, min_server, max_server,
                 l_flow, t_cps);
    m_server_pool.push_back(pool);
    return(true);
}



bool CTupleGeneratorSmart::Create(uint32_t _id,
                                  uint32_t thread_id)
                                  
{
    m_thread_id     = thread_id;
    m_id = _id;
    m_was_init=true;
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

void CTupleGenPoolYaml::Dump(FILE *fd){
    fprintf(fd,"  dist            : %d \n",m_dist);
    fprintf(fd,"  IPs         : %08x -%08x \n",m_ip_start,m_ip_end);
    fprintf(fd,"  clients per gb  : %d  \n",m_number_of_clients_per_gb);
    fprintf(fd,"  min clients     : %d  \n",m_min_clients);
    fprintf(fd,"  tcp aging       : %d sec \n",m_tcp_aging_sec);
    fprintf(fd,"  udp aging       : %d sec \n",m_udp_aging_sec);
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
    UTL_YAML_READ(uint32, clients_per_gb, fi.m_number_of_clients_per_gb);
    UTL_YAML_READ(uint32, min_clients, fi.m_min_clients);
    UTL_YAML_READ(ip_addr, dual_port_mask, fi.m_dual_interface_mask);
    UTL_YAML_READ(uint16, tcp_aging, fi.m_tcp_aging_sec);
    UTL_YAML_READ(uint16, udp_aging, fi.m_udp_aging_sec);
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
