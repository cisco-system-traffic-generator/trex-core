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



              
/* simple tuple genertion for one low*/
void CTupleGeneratorSmart::GenerateTuple(CTupleBase & tuple) {
    BP_ASSERT(m_was_init);
    Generate_client_server();
    m_was_generated = true;
    m_result_client_port = GenerateOneClientPort(m_client_ip);
    tuple.setClient(m_result_client_ip);
    tuple.setServer(m_result_server_ip);
    tuple.setClientPort(m_result_client_port);
    tuple.setClientMac(&m_result_client_mac);
//    printf(" alloc  %x %d mac:%x,%x\n",m_result_client_ip,m_result_client_port, m_result_client_mac.mac[0], m_result_client_mac.mac[1]);
}




/*
 * allocate base tuple with n exta ports, used by bundels SIP
 * for example need to allocat 3 ports for this C/S
 */
void CTupleGeneratorSmart::GenerateTupleEx(CTupleBase & tuple,
                                              uint8_t extra_ports_no,
                                              uint16_t * extra_ports) {
    GenerateTuple(tuple) ;
    for (int idx=0;idx<extra_ports_no;idx++) {
        extra_ports[idx] = GenerateOneClientPort(m_client_ip);
    }
}

void CTupleGeneratorSmart::Dump(FILE  *fd){
    fprintf(fd," id: %x,  %x:%x -  %x \n client:%x - %x, server:%x-%x\n",m_id,m_result_client_ip,m_result_server_ip,m_result_client_port,m_min_client_ip, m_max_client_ip, m_min_server_ip, m_max_server_ip);
}


void delay(int msec);

bool CTupleGeneratorSmart::Create(uint32_t _id,
                                     uint32_t thread_id,
                                     IP_DIST_t  dist,
                                     uint32_t min_client,
                                     uint32_t max_client,
                                     uint32_t min_server,
                                     uint32_t max_server,
                                     double l_flow,
                                     double t_cps,
                                     CFlowGenList* fl_list){

    m_active_alloc=0;
    if (dist>=cdMAX_DIST) {
        m_client_dist = cdSEQ_DIST;
    } else {
        m_client_dist = dist;
    }
    m_min_client_ip = min_client;
    m_max_client_ip = max_client;
    m_min_server_ip = min_server;
    m_max_server_ip = max_server;
    assert(m_max_client_ip>=m_min_client_ip);
    assert(m_max_server_ip>=m_min_server_ip);
    assert((m_max_client_ip- m_min_client_ip)<50000);

    uint32_t total_clients = getTotalClients();
    /*printf("\ntotal_clients:%d, longest_flow:%f sec, total_cps:%f\n",
            total_clients, l_flow, t_cps);*/
    m_client.resize(m_max_client_ip-m_min_client_ip+1);
    if (fl_list == NULL || !is_mac_info_conf(fl_list)) {
        if (total_clients > ((l_flow*t_cps/MAX_PORT))) {
            for (int idx=0;idx<m_client.size();idx++) 
                m_client[idx] = new CClientInfoL();
        } else {
            for (int idx=0;idx<m_client.size();idx++) 
                m_client[idx] = new CClientInfo();
        }
    } else {
        if (total_clients > ((l_flow*t_cps/MAX_PORT))) {
            for (int idx=0;idx<m_client.size();idx++) {
                m_client[idx] = new CClientInfoL(
                           get_mac_addr_by_ip(fl_list, min_client+idx)); 
            }
        } else {
            for (int idx=0;idx<m_client.size();idx++) 
                m_client[idx] = new CClientInfo(
                           get_mac_addr_by_ip(fl_list, min_client+idx)); 
        }
    }
    m_was_generated = false;
    m_thread_id     = thread_id;

    m_id = _id;
    m_was_init=true;
    m_port_allocation_error=0;
    return(true);
}

void CTupleGeneratorSmart::Delete(){
    m_was_generated = false;
    m_was_init=false;
    m_client_dist = cdSEQ_DIST;

    for (int idx=0;idx<m_client.size();idx++){
        delete m_client[idx];
    }
    m_client.clear();
}

void CTupleGeneratorSmart::Generate_client_server(){
    if (m_was_generated == false) {
        /*first time */
        m_was_generated = true;
        m_cur_client_ip = m_min_client_ip;
        m_cur_server_ip = m_min_server_ip;
    }

    uint32_t client_ip;
    int i=0;
    for (;i<100;i++) {
        if (is_client_available(m_cur_client_ip)) {
            break;
        }
        if (m_cur_client_ip >= m_max_client_ip) {
            m_cur_client_ip = m_min_client_ip;
        } else {
            m_cur_client_ip++;
        }
    }
    if (i>=100) {
        printf(" ERROR ! sparse mac-ip files is not supported yet !\n"); 
        exit(-1);
    }

    m_client_ip = m_cur_client_ip;
    CClientInfoBase* client = get_client_by_ip(m_client_ip);
    memcpy(&m_result_client_mac, 
           client->get_mac_addr(),
           sizeof(mac_addr_align_t));
    m_result_client_ip = m_client_ip;
    m_result_server_ip = m_cur_server_ip ;
/*
printf("ip:%x,mac:%x,%x,%x,%x,%x,%x, inused:%x\n",m_client_ip,
       m_result_client_mac.mac[0],
       m_result_client_mac.mac[1],
       m_result_client_mac.mac[2],
       m_result_client_mac.mac[3],
       m_result_client_mac.mac[4],
       m_result_client_mac.mac[5],
       m_result_client_mac.inused);
*/
    m_cur_client_ip ++;
    m_cur_server_ip ++;
    if (m_cur_client_ip > m_max_client_ip) {
        m_cur_client_ip = m_min_client_ip;
    }
    if (m_cur_server_ip > m_max_server_ip) {
        m_cur_server_ip = m_min_server_ip;
    }
}

void CTupleGeneratorSmart::return_all_client_ports() {
    for(int idx=0;idx<m_client.size();++idx) {
        m_client.at(idx)->return_all_ports();
    }
}


void CTupleGenYamlInfo::Dump(FILE *fd){
    fprintf(fd,"  dist            : %d \n",m_client_dist);
    fprintf(fd,"  clients         : %08x -%08x \n",m_clients_ip_start,m_clients_ip_end);
    fprintf(fd,"  servers         : %08x -%08x \n",m_servers_ip_start,m_servers_ip_end);
    fprintf(fd,"  clients per gb  : %d  \n",m_number_of_clients_per_gb);
    fprintf(fd,"  min clients     : %d  \n",m_min_clients);
    fprintf(fd,"  tcp aging       : %d sec \n",m_tcp_aging_sec);
    fprintf(fd,"  udp aging       : %d sec \n",m_udp_aging_sec);
}



void operator >> (const YAML::Node& node, CTupleGenYamlInfo & fi) {
    std::string tmp;

    try {
     node["distribution"] >> tmp ;
      if (tmp == "seq" ) {
          fi.m_client_dist=cdSEQ_DIST;
      }else{
          if (tmp == "random") {
              fi.m_client_dist=cdRANDOM_DIST;
          }else{
              if (tmp == "normal") {
                  fi.m_client_dist=cdNORMAL_DIST;
              }
          }
      }
    }catch ( const std::exception& e ) {
        fi.m_client_dist=cdSEQ_DIST;
    }
   utl_yaml_read_ip_addr(node,"clients_start",fi.m_clients_ip_start);
   utl_yaml_read_ip_addr(node,"clients_end",fi.m_clients_ip_end);
   utl_yaml_read_ip_addr(node,"servers_start",fi.m_servers_ip_start);
   utl_yaml_read_ip_addr(node,"servers_end",fi.m_servers_ip_end);
   utl_yaml_read_uint32(node,"clients_per_gb",fi.m_number_of_clients_per_gb);
   utl_yaml_read_uint32(node,"min_clients",fi.m_min_clients);
   utl_yaml_read_ip_addr(node,"dual_port_mask",fi.m_dual_interface_mask);
   utl_yaml_read_uint16(node,"tcp_aging",fi.m_tcp_aging_sec);
   utl_yaml_read_uint16(node,"udp_aging",fi.m_udp_aging_sec);

}

bool CTupleGenYamlInfo::is_valid(uint32_t num_threads,bool is_plugins){
    if ( m_servers_ip_start > m_servers_ip_end ){
        printf(" ERROR The servers_ip_start must be bigger than servers_ip_end \n");
        return(false);
    }

    if ( m_clients_ip_start > m_clients_ip_end ){
        printf(" ERROR The clients_ip_start must be bigger than clients_ip_end \n");
        return(false);
    }
    uint32_t servers= (m_servers_ip_end - m_servers_ip_start +1);
    if ( servers < num_threads ) {
        printf(" ERROR The number of servers should be at least number of threads %d \n",num_threads);
        return (false);
    }

    uint32_t clients= (m_clients_ip_end - m_clients_ip_start +1);
    if ( clients < num_threads ) {
        printf(" ERROR The number of clients should be at least number of threads %d \n",num_threads);
        return (false);
    }

    /* defect for plugin  */
    if (is_plugins) {
        if ( getTotalServers() < getTotalClients() ){
            printf(" Plugin is configured. in that case due to a limitation ( defect trex-54 ) \n");
            printf(" the number of servers should be bigger than number of clients  \n");
            return (false);
        }

        /* update number of servers in a way that it would be exact multiplication */
        uint32_t mul=getTotalServers() / getTotalClients();
        uint32_t new_server_num=mul*getTotalClients();
        m_servers_ip_end = m_servers_ip_start + new_server_num-1 ;

        assert(getTotalServers() %getTotalClients() ==0);
    }

/*    if (clients > 00000) {
        printf("  The number of clients requested is %d maximum supported : %d \n",clients,100000);
        return (false);
    }
 */   return (true);
}


/* split the clients and server by dual_port_id and thread_id ,
  clients is splited by threads and dual_port_id
  servers is spliteed by dual_port_id */
void split_clients(uint32_t thread_id, 
                   uint32_t total_threads, 
                   uint32_t dual_port_id,
                   CTupleGenYamlInfo & fi,
                   CClientPortion & portion){

    uint32_t clients_chunk = fi.getTotalClients()/total_threads;
    // FIXME need to fix this when fixing the server 
    uint32_t servers_chunk = fi.getTotalServers()/total_threads;

    assert(clients_chunk>0);
    assert(servers_chunk>0);

    uint32_t dual_if_mask=(dual_port_id*fi.m_dual_interface_mask);

    portion.m_client_start  = fi.m_clients_ip_start  + thread_id*clients_chunk + dual_if_mask;
    portion.m_client_end    = portion.m_client_start + clients_chunk -1 ;

    portion.m_server_start  = fi.m_servers_ip_start  + thread_id*servers_chunk +dual_if_mask;
    portion.m_server_end    = portion.m_server_start   + servers_chunk -1;
}
