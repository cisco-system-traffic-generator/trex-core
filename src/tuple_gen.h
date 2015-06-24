#ifndef TUPLE_GEN_H_
#define TUPLE_GEN_H_
/*
 Wenxian Li 
 
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


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include "common/c_common.h"
#include <bitset>
#include <yaml-cpp/yaml.h>


/*
 * Class that handle the client info
 */
#define MAX_PORT (64000)
#define MIN_PORT (1024)
#define ILLEGAL_PORT (0)

#define PORT_FREE (0)
#define PORT_IN_USE (1)

/*FIXME*/
#define VLAN_SIZE (2)

/* Client distribution */


typedef enum  {
    cdSEQ_DIST    = 0,
    cdRANDOM_DIST = 1,
    cdNORMAL_DIST = 2,
    cdMAX_DIST    = 3
} IP_DIST_t ;

typedef struct mac_addr_align_ {
public:
    uint8_t mac[6];
    uint8_t inused;
    uint8_t pad;
} mac_addr_align_t;
#define INUSED 0
#define UNUSED 1

/* For type 1, we generator port by maintaining a 64K bit array for each port.
 * In this case, we cannot support large number of clients due to memory exhausted. 
 *
 * So we develop a type 2 tuple generator. In this case, we only maintain a 16 bit
 * current port number for each client. To apply to type 2, it should meet:
 * number of clients > (longest_flow*Total_CPS)/64K
 *
 * TRex will decide which type to use automatically. It is transparent to users.
 * */

#define TYPE1 0
#define TYPE2 1
#define MAX_TYPE 3

class CClientInfoBase {
    public:
        virtual uint16_t get_new_free_port() = 0;
        virtual void return_port(uint16_t a) = 0;
        virtual void return_all_ports() = 0;
        virtual bool is_client_available() = 0;
        virtual mac_addr_align_t* get_mac_addr() = 0; 
};

//CClientInfo for large amount of clients support
class CClientInfoL : public CClientInfoBase {
    mac_addr_align_t mac;
 private:
    uint16_t m_curr_port;
 public:
    CClientInfoL(mac_addr_align_t* mac_adr) {
        m_curr_port = MIN_PORT;
        if (mac_adr) {
            mac = *mac_adr;
            mac.inused = INUSED;
        } else {
            memset(&mac, 0, sizeof(mac_addr_align_t));
            mac.inused = UNUSED;
        }
    }

    CClientInfoL() {
        m_curr_port = MIN_PORT;
        memset(&mac, 0, sizeof(mac_addr_align_t));
        mac.inused = INUSED;
    }
    mac_addr_align_t* get_mac_addr() {
        return &mac;
    }
    uint16_t get_new_free_port() {
        if (m_curr_port>MAX_PORT) {
            m_curr_port = MIN_PORT;
        }
        return m_curr_port++;
    }

    void return_port(uint16_t a) {
    }

    void return_all_ports() {
        m_curr_port = MIN_PORT;
    }

    bool is_client_available() {
        if (mac.inused == INUSED) {
            return true;
        } else {
            return false;
        }
    }
};


class CClientInfo : public CClientInfoBase {
 private:
    std::bitset<MAX_PORT>  m_bitmap_port;
    uint16_t m_head_port;
    mac_addr_align_t mac;
    friend class CClientInfoUT;

 private:
    bool is_port_available(uint16_t port) {
        if (!is_port_legal(port)) {
            return PORT_IN_USE;
        }
        return (m_bitmap_port[port] == PORT_FREE);
    }

    /*
     * Return true if the port is legal
     *        false if the port is illegal.
     */
    bool is_port_legal(uint16_t port) {
        if (port>=MAX_PORT || port < MIN_PORT) {
                return false;
        }
        return true;
    }

    void m_head_port_set(uint16_t head) {
        if (!is_port_legal(head)) {
            return;
        }
        m_head_port = head;
    }

    // Try to find next free port
    void get_next_free_port_by_bit() {
        uint16_t cnt = 0;
        if (!is_port_legal(m_head_port)) {
            m_head_port = MIN_PORT;
        }
        while (true) {
            if (is_port_available(m_head_port)) {
                return;
            }
            cnt++;
            if (cnt>20) {
                /*FIXME: need to trigger some alarms?*/
                return;
            }
            m_head_port++;
            if (m_head_port>=MAX_PORT) {
                m_head_port = MIN_PORT;
            }
        }
    }


 public:
    CClientInfo() {
        m_head_port = MIN_PORT;
        m_bitmap_port.reset();
        memset(&mac, 0, sizeof(mac_addr_align_t));
        mac.inused = INUSED;
    }
    CClientInfo(mac_addr_align_t* mac_info) {
        m_head_port = MIN_PORT;
        m_bitmap_port.reset();
        if (mac_info) {
            mac = *mac_info;
            mac.inused = INUSED;
        } else {
            memset(&mac, 0, sizeof(mac_addr_align_t));
            mac.inused = UNUSED;
        }
 
    }

    mac_addr_align_t* get_mac_addr() {
        return &mac;
    }

    uint16_t get_new_free_port() {
        uint16_t r;

        get_next_free_port_by_bit();
        if (!is_port_available(m_head_port)) {
            m_head_port = MIN_PORT;
            return ILLEGAL_PORT;
        }

        m_bitmap_port[m_head_port] = PORT_IN_USE;
        r = m_head_port;
        m_head_port++;
        if (m_head_port>MAX_PORT) {
            m_head_port = MIN_PORT;
        }
        return r;
    }

    void return_port(uint16_t a) {
        assert(is_port_legal(a));
        assert(m_bitmap_port[a]==PORT_IN_USE);
        m_bitmap_port[a] = PORT_FREE;
    }

    void return_all_ports() {
        m_head_port = MIN_PORT;
        m_bitmap_port.reset();
    }
    bool is_client_available() {
        if (mac.inused == INUSED) {
            return true;
        } else {
            return false;
        }
    }

};

class CTupleBase {
public:
       uint32_t getClient() {
           return m_client_ip;
       }
       void setClient(uint32_t ip) {
           m_client_ip = ip;
       }
       uint32_t getServer(){
           return m_server_ip;
       }
       void setServer(uint32_t ip) {
           m_server_ip = ip;
       }
       uint16_t getClientPort() {
           return m_client_port;
       }
       void setClientPort(uint16_t port) {
           m_client_port = port;
       }
       mac_addr_align_t* getClientMac() {
           return &m_client_mac;
       }
       void setClientMac(mac_addr_align_t* mac_info) {
           memcpy(&m_client_mac, mac_info, sizeof(mac_addr_align_t));
       }
private:
       uint32_t m_client_ip;
       uint32_t m_server_ip;
       uint16_t m_client_port;
       uint16_t pad1;
       uint32_t pad2;
       mac_addr_align_t m_client_mac;
       uint32_t pad3[3];
};



class CFlowGenList;
mac_addr_align_t * get_mac_addr_by_ip(CFlowGenList *fl_list,
                                             uint32_t ip);
bool is_mac_info_conf(CFlowGenList *fl_list);

/* generate for each template */
class CTupleGeneratorSmart {
public:
    /* simple tuple genertion for one low*/
    void GenerateTuple(CTupleBase & tuple);
    /*
     * allocate base tuple with n exta ports, used by bundels SIP
     * for example need to allocat 3 ports for this C/S
     */
    void GenerateTupleEx(CTupleBase & tuple,uint8_t extra_ports_no,
                         uint16_t * extra_ports);

    /* free client port */
    void FreePort(uint32_t c_ip,
                  uint16_t port){
        //printf(" free %x %d \n",c_ip,port);
        m_active_alloc--;
        CClientInfoBase* client = get_client_by_ip(c_ip);
        client->return_port(port);
    }

    /* return true if this type of generator require to free resource */
    bool IsFreePortRequired(void){
        return(true);
    }

    /* return the active socket */
    uint32_t ActiveSockets(void){
        return (m_active_alloc);
    }

    uint32_t getTotalClients(void){
        return (m_max_client_ip -m_min_client_ip +1);
    }

    uint32_t getTotalServers(void){
        return (m_max_server_ip -m_min_server_ip +1);
    }

    uint32_t SocketsPerClient(void){
        return (MAX_PORT -MIN_PORT+1);
    }

    uint32_t MaxSockets(void){
        return (SocketsPerClient() * getTotalClients());
    }


public:
    CTupleGeneratorSmart(){
        m_was_init=false;
        m_client_dist = cdSEQ_DIST;
    }
    bool Create(uint32_t _id,
            uint32_t thread_id,
            IP_DIST_t  dist,
            uint32_t min_client,
            uint32_t max_client,
            uint32_t min_server,
            uint32_t max_server,
            double longest_flow,
            double total_cps,
            CFlowGenList * fl_list = NULL);

    void Delete();

    void Dump(FILE  *fd);

    void SetClientDist(IP_DIST_t dist) {
        m_client_dist = dist;
    }

    IP_DIST_t GetClientDist() {
        return (m_client_dist);
    }

    inline uint32_t GetThreadId(){
        return (  m_thread_id );
    }

    bool is_valid_client(uint32_t c_ip){
        if ((c_ip>=m_min_client_ip) && (c_ip<=m_max_client_ip)) {
            return(true);
        }
        printf("invalid client ip:%x, min_ip:%x, max_ip:%x\n", 
               c_ip, m_min_client_ip, m_max_client_ip);
        return(false);
    }

    CClientInfoBase* get_client_by_ip(uint32_t c_ip){
        BP_ASSERT( is_valid_client(c_ip) );
        return m_client.at(c_ip-m_min_client_ip);
    }

    bool is_client_available (uint32_t c_ip) {
        CClientInfoBase* client = get_client_by_ip(c_ip);
        if (client) {
            return client->is_client_available();
        }
        return false;
    }

    uint16_t GenerateOneClientPort(uint32_t c_ip) {
        CClientInfoBase* client = get_client_by_ip(c_ip);
        uint16_t port;
        port = client->get_new_free_port();
        
        //printf(" alloc extra  %x %d \n",c_ip,port);
        if (port==ILLEGAL_PORT) {
            m_port_allocation_error++;
        }
        m_active_alloc++;
        return (port);
    }

    uint32_t getErrorAllocationCounter(){
        return ( m_port_allocation_error  );
    }

private:
    void return_all_client_ports();


    void Generate_client_server();


private:
    std::vector<CClientInfoBase*> m_client;

    uint32_t m_id;
    bool     m_was_generated;
    bool     m_was_init;

    IP_DIST_t  m_client_dist;

    uint32_t m_cur_server_ip;
    uint32_t m_cur_client_ip;
    // min-max client ip +1 and get back
    uint32_t m_min_client_ip;
    uint32_t m_max_client_ip;

    // min max server ip ( random )
    uint32_t m_min_server_ip;
    uint32_t m_max_server_ip;

    uint32_t m_thread_id;

    // result of the generator FIXME need to clean this
    uint32_t    m_client_ip;
    uint32_t m_result_client_ip;
    uint32_t m_result_server_ip;
    uint32_t m_active_alloc;
    mac_addr_align_t m_result_client_mac;
    uint16_t m_result_client_port;
            
    uint32_t m_port_allocation_error;

};


class CTupleTemplateGeneratorSmart {
public:
    /* simple tuple genertion for one low*/
    void GenerateTuple(CTupleBase & tuple){
        if (m_w==1) {
            /* new client each tuple generate */
            m_gen->GenerateTuple(tuple);
            m_cache_client_ip=tuple.getClient();
        }else{
            if (m_cnt==0) {
                m_gen->GenerateTuple(tuple);
                m_cache_client_ip = tuple.getClient();
                m_cache_server_ip = tuple.getServer();
            }else{
                tuple.setServer(m_cache_server_ip);
                tuple.setClient(m_cache_client_ip);
                tuple.setClientPort( m_gen->GenerateOneClientPort(m_cache_client_ip));
            }
            m_cnt++;
            if (m_cnt>=m_w) {
                m_cnt=0;
            }
        }
        if ( m_is_single_server ) {
            tuple.setServer(m_server_ip);
        }
    }

    uint16_t GenerateOneSourcePort(){
        return ( m_gen->GenerateOneClientPort(m_cache_client_ip) );
    }

    inline uint32_t GetThreadId(){
        return ( m_gen->GetThreadId() );
    }

public:

    bool Create( CTupleGeneratorSmart * gen
                 ){
        m_gen=gen;
        m_is_single_server=false;
        m_server_ip=0;
        SetW(1);
        return (true);
    }

    void Delete(){
    }
public:
    void SetW(uint16_t w){
        m_w=w;
        m_cnt=0;
    }

    uint16_t getW(){
        return (m_w);
    }


    void SetSingleServer(bool is_single, 
                         uint32_t server_ip,
                         uint32_t dual_port_index,
                         uint32_t dual_mask){
        m_is_single_server = is_single;
        m_server_ip = server_ip+dual_mask*dual_port_index;
    }
    bool IsSingleServer(){
        return (m_is_single_server);
    }

private:
    CTupleGeneratorSmart * m_gen;
    bool                   m_is_single_server;
    uint16_t               m_w;
    uint16_t               m_cnt;
    uint32_t               m_server_ip;
    uint32_t               m_cache_client_ip;
    uint32_t               m_cache_server_ip;

};


/* YAML of generator */
#if 0
        -  client_distribution      : 'seq' - ( e.g c0,1,2,3,4  
                                      'random' - random from the pool 
                                      'normal' - need to give average and dev -- second phase      
                                        
        -  client_pool_mask         : 10.0.0.0-20.0.0.0
        -  server_pool_mask         : 70.0.0.0-70.0.20.0    
        -  number_of_clients_per_gb : 20
        -  dual_interface_mask      : 1.0.0.0  // each dual ports will add this to the pool of clients 
#endif

struct CTupleGenYamlInfo {
    CTupleGenYamlInfo(){
        m_client_dist=cdSEQ_DIST;
        m_clients_ip_start =0x11000000; 
        m_clients_ip_end   =0x21000000;

        m_servers_ip_start = 0x30000000;
        m_servers_ip_end   = 0x40000000;
        m_number_of_clients_per_gb=10;
        m_min_clients=100;
        m_dual_interface_mask=0x10000000;
        m_tcp_aging_sec=2;
        m_udp_aging_sec=5;
    }

    IP_DIST_t       m_client_dist;
    uint32_t        m_clients_ip_start;
    uint32_t        m_clients_ip_end;

    uint32_t        m_servers_ip_start;
    uint32_t        m_servers_ip_end;
    uint32_t        m_number_of_clients_per_gb;
    uint32_t        m_min_clients;
    uint32_t        m_dual_interface_mask;
    uint16_t        m_tcp_aging_sec; /* 0 means there is no aging */
    uint16_t        m_udp_aging_sec;

public:
    void Dump(FILE *fd);
    uint32_t getTotalClients(void){
        return ( m_clients_ip_end-m_clients_ip_start+1);
    }
    uint32_t getTotalServers(void){
        return ( m_servers_ip_end-m_servers_ip_start+1);
    }


    bool is_valid(uint32_t num_threads,bool is_plugins);
};

void operator >> (const YAML::Node& node, CTupleGenYamlInfo & fi) ;


struct CClientPortion {
    uint32_t m_client_start;
    uint32_t m_client_end;
    uint32_t m_server_start;
    uint32_t m_server_end;
};

void split_clients(uint32_t thread_id, 
                   uint32_t total_threads, 
                   uint32_t dual_port_id,
                   CTupleGenYamlInfo & fi,
                   CClientPortion & portion);



#endif //TUPLE_GEN_H_
