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
#include "trex_client_config.h"

#include <random>

typedef uint16_t  pool_index_t;
#define CS_MAX_POOLS UINT16_MAX

static inline uint16_t rss_align_lsb(uint16_t val,
                              uint16_t rss_thread_id,
                              uint16_t rss_thread_max,
                              uint8_t reta_mask){
    /* input to reta table */

    uint16_t hash_input = (val& reta_mask);
    /* make it align to core id */
    hash_input = (hash_input - (hash_input%rss_thread_max)) + rss_thread_id;
    if (hash_input>reta_mask) {
        hash_input=rss_thread_id;
    }
    val = (val & ~((uint16_t)reta_mask)) | hash_input ;
    return(val);

}

static inline uint8_t reverse_bits8(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

static inline uint16_t rss_reverse_bits_port(uint16_t port){        
    return ( (port&0xff00) + reverse_bits8(port&0xff));
}


class CTupleBase {
public:

       CTupleBase() {
           m_client_cfg = NULL;
       }

       uint32_t getClient() {
           return m_client_ip;
       }
       void setClient(uint32_t ip) {
           m_client_ip = ip;
       }
       uint32_t getClientId() {
           return m_client_idx;
       }
       void setClientId(uint32_t id) {
           m_client_idx = id;
       }
       
       uint32_t getServer(){
           return m_server_ip;
       }
       void setServer(uint32_t ip) {
           m_server_ip = ip;
       }
       uint32_t getServerId(){
           return m_server_idx;
       }
       void setServerId(uint32_t id) {
           m_server_idx = id;
       }
       uint16_t getServerPort() {
           return m_server_port;
       }
       void setServerPort(uint16_t port) {
           m_server_port = port;
       }
       uint16_t getClientPort() {
           return m_client_port;
       }
       void setClientPort(uint16_t port) {
           m_client_port = port;
       }
       void setClientCfg(ClientCfgBase *cfg) {
           m_client_cfg = cfg;
       }
       ClientCfgBase *getClientCfg() {
           return m_client_cfg;
       }


       void setClientTuple(uint32_t ip, ClientCfgBase *cfg, uint16_t port) {
           setClient(ip);
           setClientPort(port);
           setClientCfg(cfg);
       }

       void setClientAll2(uint32_t id, uint32_t ip,uint16_t port) {
           setClientId(id);
           setClient(ip);
           setClientPort(port);
       }
 
       void setServerAll(uint32_t id, uint32_t ip) {
           setServerId(id);
           setServer(ip);
       }
       void getClientAll(uint32_t & id, uint32_t & ip, uint32_t & port) {
           id = getClientId();
           ip = getClient();
           port = getClientPort();
       }
       void getServerAll(uint32_t & id, uint32_t & ip) {
           id = getServerId();
           ip = getServer();
       }
private:
       uint32_t m_client_ip;
       uint32_t m_client_idx;

       uint32_t m_server_ip;
       uint32_t m_server_idx;

       ClientCfgBase *m_client_cfg;

       uint16_t m_client_port;
       uint16_t m_server_port;
};



/*
 * Class that handle the client info
 */
#define MAX_CLIENTS 1000000
#define MAX_PORT (64000)
#define MIN_PORT (1024)
#define ILLEGAL_PORT (0)

#define PORT_FREE (0)
#define PORT_IN_USE (1)

/*FIXME*/
#define VLAN_SIZE (2)

#define FOREACH(vector) for(int i=0;i<vector.size();i++)


/* Client distribution */
typedef enum  {
    cdSEQ_DIST    = 0,
    cdRANDOM_DIST = 1,
    cdNORMAL_DIST = 2,
    cdMAX_DIST    = 3
} IP_DIST_t ;

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

class CIpInfoBase {
    public:
        virtual uint16_t get_new_free_port() = 0;
        virtual void return_port(uint16_t a) = 0;
        virtual void generate_tuple(CTupleBase & tuple) = 0;
        virtual void set_start_port(uint16_t a)=0;

        virtual void set_min_port(uint16_t a)=0;
        virtual void set_inc_port(uint16_t a)=0;
        virtual void set_sport_reverse_lsb(bool enable,uint8_t reta_mask)=0; /* for RSS */

        virtual void return_all_ports() = 0;
        virtual ClientCfgBase * get_client_cfg(){
            return (NULL);
        }
        uint32_t get_ip() {
            return m_ip;
        }
        void set_ip(uint32_t ip) {
            m_ip = ip;
        }
        virtual ~CIpInfoBase() {}
    protected:
        uint32_t          m_ip;
};

//CClientInfo for large amount of clients support
class CIpInfoL : public CIpInfoBase {
 private:
    uint16_t m_curr_port;
 public:
    CIpInfoL() {
        m_curr_port = MIN_PORT;
    }

    /* not supported in this mode */
    void set_min_port(uint16_t a){
        assert(0);
    }

    /* not supported in this mode */
    void set_inc_port(uint16_t a){
        assert(0);
    }

    void set_sport_reverse_lsb(bool enable,uint8_t reta_mask){
        assert(0);
    }


    void set_start_port(uint16_t a){
        m_curr_port = a;
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
};



class CIpInfo : public CIpInfoBase {
 private:
    std::bitset<MAX_PORT>  m_bitmap_port;
    uint16_t    m_head_port;
    uint16_t    m_port_inc;
    uint16_t    m_min_port;
    uint8_t     m_reverse_port;
    uint8_t     m_reta_mask;

    friend class CClientInfoUT;

 private:
    /* done for RSS */
    uint16_t convert_sport(uint16_t port){
        if (m_reverse_port==0) {
            return(port);
        }
        return (rss_reverse_bits_port(port));
    }

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
        if (port>=MAX_PORT || port < m_min_port) {
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
            m_head_port = m_min_port;
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
            inc_port();
            if (m_head_port>=MAX_PORT) {
                m_head_port = m_min_port;
            }
        }
    }

    inline void inc_port(){
        if (m_reverse_port) { /* normal */
            if (((m_head_port&m_reta_mask)+m_port_inc)>m_reta_mask) {
              /* there are cases that this need to fix  */
              m_head_port+=m_port_inc;
              uint8_t rss_thread_id=(m_min_port&m_reta_mask)%m_port_inc; /* calc the rss_thread_id back */
              m_head_port = rss_align_lsb(m_head_port,rss_thread_id,m_port_inc,m_reta_mask); /* fixup the port */
            }else{
              m_head_port+=m_port_inc;
            }
        }else{
            m_head_port+=m_port_inc;
        }
    }

 public:
    CIpInfo() {
        m_head_port = MIN_PORT;
        m_min_port  = MIN_PORT;
        m_port_inc  = 1;
        m_reverse_port=0;
        m_bitmap_port.reset();
    }

    void set_min_port(uint16_t a){
        m_min_port = a;
    }

    void set_sport_reverse_lsb(bool enable,uint8_t reta_mask){
        m_reverse_port=enable?1:0;
        m_reta_mask=reta_mask;
    }


    void set_inc_port(uint16_t a){
        m_port_inc=a;
    }


    void set_start_port(uint16_t a){
        m_head_port = a;
    }

    uint16_t get_new_free_port() {
        uint16_t r;

        get_next_free_port_by_bit();
        if (!is_port_available(m_head_port)) {
            m_head_port = m_min_port ;
            return ILLEGAL_PORT;
        }

        m_bitmap_port[m_head_port] = PORT_IN_USE;
        r = m_head_port;
        inc_port();
        if (m_head_port>MAX_PORT) {
            m_head_port = m_min_port;
        }


        return convert_sport(r);
    }

    void return_port(uint16_t a) {
        a=convert_sport(a);
        assert(is_port_legal(a));
        assert(m_bitmap_port[a]==PORT_IN_USE);
        m_bitmap_port[a] = PORT_FREE;
    }

    void return_all_ports() {
        m_head_port = m_min_port;
        m_bitmap_port.reset();
    }
};


/**
 * a flat client info (no configuration)
 *  
 * using template to avoid duplicating the code for CIpInfo and 
 * CIpInfoL 
 *  
 * @author imarom (27-Jun-16)
 */
template <typename T>
class CSimpleClientInfo : public T {

public:
     CSimpleClientInfo(uint32_t ip) {
        T::set_ip(ip);
     }


     void generate_tuple(CTupleBase &tuple) {
         tuple.setClientTuple(T::m_ip,
                              NULL,
                              T::get_new_free_port());
    }
};

/**
 * a configured client object
 * 
 * @author imarom (26-Jun-16)
 */
template <typename T>
class CConfiguredClientInfo : public T {

public:
    CConfiguredClientInfo(uint32_t ip, const ClientCfgBase &cfg) : m_cfg(cfg) {
        printf("CConfiguredClientInfo::CConfiguredClientInfo, cfg: %lu\n", &cfg);
        printf("name of T: %s\n", typeid(T).name());
        T::set_ip(ip);
    }

    virtual ClientCfgBase * get_client_cfg(void){
        return (&m_cfg);
    }

    void generate_tuple(CTupleBase &tuple) {
        tuple.setClientTuple(T::m_ip,
                             &m_cfg,
                             T::get_new_free_port());
    }

private:
    ClientCfgBase m_cfg;
};


class CServerInfo : public CIpInfo {
    void generate_tuple(CTupleBase & tuple) {
        tuple.setServer(m_ip);
    }
};

class CServerInfoL : public CIpInfoL {
    void generate_tuple(CTupleBase & tuple) {
        tuple.setServer(m_ip);
    }
};




class CIpPool {
    public:
        ClientCfgBase * GetClientCfg(uint32_t idx) {
            CIpInfoBase* ip_info = m_ip_info[idx];
            return(ip_info->get_client_cfg());
        }


       uint16_t GenerateOnePort(uint32_t idx) {
            CIpInfoBase* ip_info = m_ip_info[idx];
            uint16_t port;
            port = ip_info->get_new_free_port();
            
            //printf(" alloc extra  %x %d \n",c_ip,port);
            if (port==ILLEGAL_PORT) {
                m_port_allocation_error++;
            }
            m_active_alloc++;
            return (port);
        }

       bool is_valid_ip(uint32_t ip){
            CIpInfoBase* ip_front = m_ip_info.front();
            CIpInfoBase* ip_back  = m_ip_info.back();
            if ((ip>=ip_front->get_ip()) && 
                (ip<=ip_back->get_ip())) {
                return(true);
            }
            printf("invalid ip:%x, min_ip:%x, max_ip:%x, this:%p\n", 
                   ip, ip_front->get_ip(), 
                   ip_back->get_ip(),this);
            return(false);
        }

        uint32_t get_curr_ip() {
            return m_ip_info[m_cur_idx]->get_ip();
        }
        uint32_t get_ip(uint32_t idx) {
            return m_ip_info[idx]->get_ip();
        }
        CIpInfoBase* get_ip_info_by_idx(uint32_t idx) {
            return m_ip_info[idx];
        }

        void inc_cur_idx() {
            switch (m_dist) {
            case cdRANDOM_DIST: 
                m_cur_idx = get_random_idx();
                break;
            case cdSEQ_DIST :
            default:
                m_cur_idx++;
                if (m_cur_idx >= m_ip_info.size())
                    m_cur_idx = 0;
            }
        }
        //return a valid client idx in this pool
        uint32_t generate_ip() {
            uint32_t res_idx = m_cur_idx;
            inc_cur_idx();
            return res_idx;
        }

            

        void set_dist(IP_DIST_t dist) {
            if (dist>=cdMAX_DIST) {
                m_dist = cdSEQ_DIST;
            } else {
                m_dist = dist;
            }
        }
        void Delete() {
            FOREACH(m_ip_info) {
                delete m_ip_info[i];
            }
            m_ip_info.clear();
        }
        uint32_t get_total_ips() {
            return m_ip_info.size();
        }
        void return_all_ports() {
            FOREACH(m_ip_info) {
                m_ip_info[i]->return_all_ports();
            }
        }
        void FreePort(uint32_t id, uint16_t port) {
    //        assert(id<m_ip_info.size());
            m_active_alloc--;
            CIpInfoBase* client = m_ip_info[id];
            client->return_port(port);
        }
        
 
    public:
        std::vector<CIpInfoBase*> m_ip_info;
        IP_DIST_t  m_dist;
        uint32_t m_cur_idx;
        uint32_t m_active_alloc;
        uint32_t m_port_allocation_error;
        std::default_random_engine generator;
        std::uniform_int_distribution<int> *rand_dis;
        void CreateBase() {
            switch (m_dist) {
            case cdRANDOM_DIST:
                rand_dis = new std::uniform_int_distribution<int>
                    (0,get_total_ips()-1);
                break;
            default:
                break;
            }
            m_cur_idx = 0;
            m_active_alloc = 0;
            m_port_allocation_error = 0;
        }
        uint32_t get_random_idx() {
            uint32_t res =  (*rand_dis)(generator);
            return (res);
        }
        bool IsFreePortRequired(void){
            return(true);
        }


};

class CClientPool : public CIpPool {
public:

    CClientPool(){
        m_thread_id=0;
        m_rss_thread_id=0;
        m_rss_thread_max=0;
        m_reta_mask=0;
        m_rss_astf_mode=false;

    }

    uint32_t GenerateTuple(CTupleBase & tuple) {
        uint32_t idx = generate_ip();
        CIpInfoBase* ip_info = m_ip_info[idx];
        ip_info->generate_tuple(tuple);

        tuple.setClientId(idx);
        if (tuple.getClientPort()==ILLEGAL_PORT) {
            m_port_allocation_error++;
        }
        m_active_alloc++;
        return idx;
    }

    uint16_t get_tcp_aging() {
        return m_tcp_aging;
    }
    uint16_t get_udp_aging() {
        return m_udp_aging;
    }

    void Create(IP_DIST_t       dist_value,
                uint32_t        min_ip,
                uint32_t        max_ip,
                double          active_flows,
                ClientCfgDB     &client_info,
                uint16_t        tcp_aging,
                uint16_t        udp_aging); 


    void set_thread_id(uint16_t thread_id){
        m_thread_id = thread_id;
    }
    void set_rss_thread_id(uint16_t rss_thread_id,
                           uint16_t rss_thread_max,
                           uint8_t reta_mask){
        m_rss_astf_mode=true;
        m_rss_thread_id  = rss_thread_id;
        m_rss_thread_max = rss_thread_max;
        m_reta_mask = reta_mask;
    }

public: 
    uint16_t m_tcp_aging;
    uint16_t m_udp_aging;
    uint16_t m_thread_id;
    uint16_t m_rss_thread_id;
    uint16_t m_rss_thread_max;
    uint8_t  m_reta_mask;
    bool     m_rss_astf_mode;

private:
    void allocate_simple_clients(uint32_t  min_ip,
                                 uint32_t  total_ip,
                                 bool      is_long_range);

    void allocate_configured_clients(uint32_t        min_ip,
                                     uint32_t        total_ip,
                                     bool            is_long_range,
                                     ClientCfgDB     &client_info);

    void configure_client(uint32_t indx);
};

class CServerPoolBase {
    public:
    CServerPoolBase(){
        m_thread_id=0;
    }

    virtual ~CServerPoolBase() {}

    virtual void GenerateTuple(CTupleBase& tuple) = 0;
    virtual uint16_t GenerateOnePort(uint32_t idx) = 0;
    virtual void Delete() = 0;
    virtual uint32_t get_total_ips()=0;
    virtual void Create(IP_DIST_t  dist_value,
               uint32_t min_ip,
               uint32_t max_ip,
               double active_flows) = 0; 
    void set_thread_id(uint16_t thread_id){
        m_thread_id =thread_id;
    }

private:
    uint16_t m_thread_id;
};

class CServerPoolSimple : public CServerPoolBase {
public:
    void Create(IP_DIST_t  dist_value,
               uint32_t min_ip,
               uint32_t max_ip,
                double active_flows
               ) {
        m_max_server_ip = max_ip;
        m_min_server_ip = min_ip;
        m_cur_server_ip = min_ip;
    }
    void Delete() {
        return ;
    }
    void GenerateTuple(CTupleBase& tuple) {
        tuple.setServer(m_cur_server_ip);
        m_cur_server_ip ++;
        if (m_cur_server_ip > m_max_server_ip) {
            m_cur_server_ip = m_min_server_ip;
        }
    } 
    uint16_t GenerateOnePort(uint32_t idx) {
        // do nothing
        return 0;
    }
    uint32_t get_total_ips() {
        return (m_max_server_ip-m_min_server_ip+1);
    }
private:
    uint32_t m_max_server_ip;
    uint32_t m_min_server_ip;
    uint32_t m_cur_server_ip;
};

class CServerPool : public CServerPoolBase {
public:
    void GenerateTuple(CTupleBase & tuple) {
       uint32_t idx = gen->generate_ip();
       tuple.setServerAll(idx, gen->get_ip(idx));
    }
    uint16_t GenerateOnePort(uint32_t idx) {
        return gen->GenerateOnePort(idx);
    }
    void Create(IP_DIST_t  dist_value,
                uint32_t min_ip,
                uint32_t max_ip,
                double active_flows); 
 
    void Delete() {
        if (gen!=NULL) {
            gen->Delete();
            delete gen;
            gen=NULL;
        }
    }
    uint32_t get_total_ips() {
        return gen->m_ip_info.size();
    }
private: 
    CIpPool *gen;
};

/* generate for each template */
class CTupleGeneratorSmart {
public:
    /* return the active socket */
    uint32_t ActiveSockets(void){
        uint32_t total_active_alloc = 0;
        FOREACH(m_client_pool) {
            total_active_alloc += m_client_pool[i]->m_active_alloc;
        }
        return (total_active_alloc);
    }

    uint32_t getTotalClients(void){
        uint32_t total_clients = 0;
        FOREACH(m_client_pool) {
            total_clients += m_client_pool[i]->get_total_ips();
        }
        return (total_clients);
    }

    uint32_t getTotalServers(void){
        uint32_t total_servers = 0;
        FOREACH(m_server_pool) {
            total_servers += m_server_pool[i]->get_total_ips();
        }
        return total_servers;
    }

    uint32_t SocketsPerClient(void){
        return (MAX_PORT -MIN_PORT+1);
    }

    uint32_t MaxSockets(void){
        return (SocketsPerClient() * getTotalClients());
    }

    
    void FreePort(pool_index_t pool_idx, uint32_t id, uint16_t port) {
        get_client_pool(pool_idx)->FreePort(id, port);
    }
        
    bool IsFreePortRequired(pool_index_t pool_idx){
        return(get_client_pool(pool_idx)->IsFreePortRequired());
    }
    uint16_t get_tcp_aging(pool_index_t pool_idx) {
        return (get_client_pool(pool_idx)->get_tcp_aging());
    }
    uint16_t get_udp_aging(pool_index_t pool_idx) {
        return (get_client_pool(pool_idx)->get_udp_aging());
    }

public:
    CTupleGeneratorSmart(){
        m_was_init=false;
        m_rss_thread_id=0;
        m_rss_thread_max =0;
        m_reta_mask=0;
        m_rss_astf_mode=false;
    }

    bool Create(uint32_t _id, uint32_t thread_id);

    void Delete();

    void set_astf_rss_mode(uint16_t rss_thread_id,
                           uint16_t rss_thread_max,
                           uint8_t  reta_mask){
        m_rss_thread_id = rss_thread_id;
        m_rss_thread_max = rss_thread_max;
        m_reta_mask  = reta_mask;
        m_rss_astf_mode =true;
    }

    bool is_astf_rss_mode(){
        return(m_rss_astf_mode);
    }


    inline uint32_t GetThreadId(){
        return (  m_thread_id );
    }

    uint32_t getErrorAllocationCounter(){
        uint32_t total_alloc_error = 0;
        FOREACH(m_client_pool) {
            total_alloc_error += m_client_pool[i]->m_port_allocation_error;
        }
        return (total_alloc_error);
    }

    bool add_client_pool(IP_DIST_t     client_dist,
                         uint32_t      min_client,
                         uint32_t      max_client,
                         double        active_flows,
                         ClientCfgDB   &client_info,
                         uint16_t      tcp_aging,
                         uint16_t      udp_aging);

    bool add_server_pool(IP_DIST_t  server_dist,
                         uint32_t   min_server,
                         uint32_t   max_server,
                         double     active_flows,
                         bool       is_bundling);

    CClientPool* get_client_pool(pool_index_t idx) {
        return m_client_pool[idx];
    }
    pool_index_t get_client_pool_num() {
        return m_client_pool.size();
    }
    pool_index_t get_server_pool_num() {
        return m_server_pool.size();
    }
    CServerPoolBase* get_server_pool(pool_index_t idx) {
        return m_server_pool[idx];
    }
private:
    uint32_t m_id;
    uint32_t m_thread_id;
    uint16_t m_rss_thread_id; /* per port thread id 0..x, for 2 dual-ports systems with 8 threads total 
                                  dual-0 : 0,1,2,3
                                  dual-1 : 0,1,2,3
                                 */
    uint16_t m_rss_thread_max; /* how many threads per RSS port */
    uint8_t  m_reta_mask;       /* 0xff or 0x7f */

    bool     m_rss_astf_mode;        /* true for ASTF mode */

    std::vector<CClientPool*> m_client_pool;
    std::vector<CServerPoolBase*> m_server_pool;
    bool     m_was_init;
};

class CTupleTemplateGeneratorSmart {
public:
    /* simple tuple genertion for one low*/
    void GenerateTuple(CTupleBase & tuple){
        if (m_w==1) {
            printf("m_w==1\n");
            /* new client each tuple generate */
            m_client_gen->GenerateTuple(tuple);
            m_server_gen->GenerateTuple(tuple);
            m_cache_client_ip = tuple.getClient();
            m_cache_client_idx = tuple.getClientId();
        }else{
            printf("m_w!=1\n");
            if (m_cnt==0) {
                printf("m_cnt==0\n");
                m_client_gen->GenerateTuple(tuple);
                m_server_gen->GenerateTuple(tuple);
                m_cache_client_ip = tuple.getClient();
                m_cache_client_idx = tuple.getClientId();
                tuple.getServerAll(m_cache_server_idx, m_cache_server_ip);
            }else{
                printf("m_cnt!=0\n");
                tuple.setServerAll(m_cache_server_idx, 
                                   m_cache_server_ip);
                tuple.setClientAll2(m_cache_client_idx,
                                    m_cache_client_ip,
                                    m_client_gen->GenerateOnePort(m_cache_client_idx));
                tuple.setClientCfg(m_client_gen->GetClientCfg(m_cache_client_idx));
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
        return ( m_client_gen->GenerateOnePort(m_cache_client_idx) );
    }

    inline uint32_t GetThreadId(){
        return ( m_gen->GetThreadId() );
    }

public:

    bool Create( CTupleGeneratorSmart * gen,
                 pool_index_t c_pool,
                 pool_index_t s_pool){
        m_gen=gen;
        m_is_single_server=false;
        m_server_ip=0;
        SetW(1);
        m_client_gen = gen->get_client_pool(c_pool);
        m_server_gen = gen->get_server_pool(s_pool);
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

    CTupleGeneratorSmart * get_gen() {
        return m_gen;
    }
private:
    CTupleGeneratorSmart * m_gen;
    CClientPool          * m_client_gen;
    CServerPoolBase      * m_server_gen;
    uint16_t               m_w;
    uint16_t               m_cnt;
    uint32_t               m_server_ip;
    uint32_t               m_cache_client_ip;
    uint32_t               m_cache_client_idx;
    uint32_t               m_cache_server_ip;
    uint32_t               m_cache_server_idx;
    bool                   m_is_single_server;
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

struct CTupleGenPoolYaml {
    IP_DIST_t       m_dist;
    uint32_t        m_ip_start;
    uint32_t        m_ip_end;
    uint32_t        m_number_of_clients_per_gb;
    uint32_t        m_min_clients;
    uint32_t        m_dual_interface_mask;
    uint16_t        m_tcp_aging_sec; /* 0 means there is no aging */
    uint16_t        m_udp_aging_sec;
    std::string     m_name;
    bool            m_is_bundling;
    bool            m_per_core_distro;
    public:
    uint32_t getTotalIps(void){
        return ( m_ip_end-m_ip_start+1);
    }
    uint32_t getDualMask() {
        return m_dual_interface_mask;
    }
    uint32_t get_ip_start() {
        return m_ip_start;
    }
    uint32_t get_ip_end() {
        return m_ip_end;
    }
    bool is_valid(uint32_t num_threads,bool is_plugins);
    void Dump(FILE *fd);
};
   

struct CTupleGenYamlInfo {
    std::vector<CTupleGenPoolYaml> m_client_pool;
    std::vector<CTupleGenPoolYaml> m_server_pool;
        
public:
    bool is_valid(uint32_t num_threads,bool is_plugins);
    pool_index_t get_server_pool_id(std::string name){
         if (name=="default") {
             return 0;
         }
        for (pool_index_t i=0;i<m_server_pool.size();i++) {
            if (m_server_pool[i].m_name==name) 
                return i;
        }
        printf("ERROR invalid server pool name %s, please review your YAML file\n",(char *)name.c_str());
        exit(-1);
        return 0;
    }

    pool_index_t get_client_pool_id(std::string name){
         if (name=="default") {
             return 0;
         }
        for (pool_index_t i=0;i<m_client_pool.size();i++) {
            if (m_client_pool[i].m_name==name) 
                return i;
        }
        printf("ERROR invalid client pool name %s, please review your YAML file\n",(char *)name.c_str());
        exit(-1);
        return 0;
    }

    bool find_port(uint32_t ip_start, uint32_t ip_end, uint8_t &port);
    void dump(FILE *fd);
};


void operator >> (const YAML::Node& node, CTupleGenPoolYaml & fi) ;

void operator >> (const YAML::Node& node, CTupleGenYamlInfo & fi) ;


struct CIpPortion {
    uint32_t m_ip_start;
    uint32_t m_ip_end;
};
void split_ips(uint32_t thread_id,
                   uint32_t total_threads,
                   uint32_t dual_port_id,
                   CTupleGenPoolYaml& poolinfo,
                   CIpPortion & portion);


void split_ips_v2( uint32_t total_threads, 
                   uint32_t rss_thread_id,
                   uint32_t rss_max_threads,
                   uint32_t max_dual_ports, 
                   uint32_t dual_port_id,
                   CTupleGenPoolYaml& poolinfo,
                   CIpPortion & portion);


#endif //TUPLE_GEN_H_
