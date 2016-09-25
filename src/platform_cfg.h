#ifndef CPLATFORM_CFG_H
#define CPLATFORM_CFG_H

/*
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

#include <yaml-cpp/yaml.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>


#define CONST_NB_MBUF_2_10G  (16380/2)

typedef enum {         MBUF_64        , // per dual port, per NUMA

                       MBUF_128       ,
                       MBUF_256       ,
                       MBUF_512       ,
                       MBUF_1024      ,
                       MBUF_2048      ,
                       MBUF_4096      ,
                       MBUF_9k        ,


                        // per NUMA
                       TRAFFIC_MBUF_64       ,
                       TRAFFIC_MBUF_128      ,
                       TRAFFIC_MBUF_256      ,
                       TRAFFIC_MBUF_512      ,
                       TRAFFIC_MBUF_1024     ,
                       TRAFFIC_MBUF_2048     ,
                       TRAFFIC_MBUF_4096     ,
                       TRAFFIC_MBUF_9k       ,


                       MBUF_DP_FLOWS        ,
                       MBUF_GLOBAL_FLOWS    , 
                       MBUF_ELM_SIZE         
              } mbuf_sizes_t;

const std::string * get_mbuf_names(void);

/*
#- port_limit      : 2         # this option can limit the number of port of the platform
  cpu_mask_offset : 4    # the offset of the cpu affinity 
  interface_mask  : [ "0000:11:00.00", "0000:11:00.01" ]  # interface that should be mask and not be considered
  scan_only_1g    : true
  enable_zmq_pub  : true  # enable publisher for stats data 
  zmq_pub_port    : 4500
  telnet_port     : 4501 # the telnet port in case it is enable ( with intercative mode )
  port_info       :  # set eh mac addr 
          - dest_mac        :   [0x0,0x0,0x0,0x1,0x0,0x00]  # port 0
            src_mac         :   [0x0,0x0,0x0,0x1,0x0,0x00]

  #for system of 1Gb/sec NIC or VM enable this  
  port_bandwidth_gb : 10  # port bandwidth 10Gb/sec , for VM put here 1 for XL710 put 40 
# memory configuration for 2x10Gb/sec system 
  memory    : 
     mbuf_64     : 16380 
     mbuf_128    : 8190
     mbuf_256    : 8190
     mbuf_512    : 8190
     mbuf_1024   : 8190
     mbuf_2048   : 2049

     traffic_mbuf_128    : 8190
     traffic_mbuf_256    : 8190
     traffic_mbuf_512    : 8190
     traffic_mbuf_1024   : 8190
     traffic_mbuf_2048   : 2049

     dp_flows    : 1048576 
     global_flows : 10240 
            
*/



struct CMacYamlInfo {
    std::vector     <uint8_t> m_dest_base;
    std::vector     <uint8_t> m_src_base;
    uint32_t m_def_gw;
    uint32_t m_ip;
    uint32_t m_mask;
    uint16_t m_vlan;
    void Dump(FILE *fd);

    void copy_dest(char *p);
    void copy_src(char *p);
    uint32_t get_def_gw();
    uint32_t get_ip();
    uint32_t get_vlan();
    uint32_t get_mask();

    void dump_mac_vector( std::vector<uint8_t> & v,FILE *fd){
        int i;
        for (i=0; i<5; i++) {
            fprintf(fd,"%02x:",v[i]);
        }
        fprintf(fd,"%02x\n",v[5]);
    }
};

/*
  platform :
        master_core  : 0
        latency_core : 5
        dual_if   :
             - socket   : 0
               threads  : [1,2,3,4]
             - socket   : 1
               threads  : [16,17,18,16]

*/

struct CPlatformDualIfYamlInfo {
public:
    uint32_t              m_socket; 
    std::vector <uint8_t> m_threads;
public:
    void Dump(FILE *fd);
};

struct CPlatformCoresYamlInfo {
public:

    CPlatformCoresYamlInfo(){
        m_is_exists=false;
    }
    bool             m_is_exists;
    uint32_t         m_master_thread; 
    uint32_t         m_rx_thread;  
    std::vector <CPlatformDualIfYamlInfo> m_dual_if;
public:
    void Dump(FILE *fd);
};



struct CPlatformMemoryYamlInfo {


public:

    CPlatformMemoryYamlInfo(){
        reset();
    }
    uint32_t         m_mbuf[MBUF_ELM_SIZE]; // relative to traffic norm to 2x10G ports 

public:
    void Dump(FILE *fd);
    void reset();
};


struct CPlatformYamlInfo {
public:
    CPlatformYamlInfo(){
        reset();
    }

    void reset(){

        m_if_mask.clear();
        m_mac_info.clear();
        m_if_list.clear();

        m_info_exist=false;
        m_port_limit_exist=false;
        m_port_limit=0xffffffff;

        m_if_mask_exist=false;

        m_enable_zmq_pub_exist=false;
        m_enable_zmq_pub=true;
        m_zmq_pub_port=4500;
        m_zmq_rpc_port = 4501;


        m_telnet_exist=false;
        m_telnet_port=4502  ;

        m_mac_info_exist=false;
        m_port_bandwidth_gb = 10;
        m_memory.reset();
        m_prefix="";
        m_limit_memory=""  ;
        m_thread_per_dual_if=1;

    }

    bool            m_info_exist; /* file exist ?*/

    bool            m_port_limit_exist;
    uint32_t        m_port_limit; 


    bool                          m_if_mask_exist;
    std::vector <std::string>     m_if_mask;

    std::vector <std::string>     m_if_list;

    std::string                   m_prefix;
    std::string                   m_limit_memory;
    uint32_t                      m_thread_per_dual_if;

    uint32_t                      m_port_bandwidth_gb;

    bool                          m_enable_zmq_pub_exist;
    bool                          m_enable_zmq_pub;
    uint16_t                      m_zmq_pub_port;


    bool                          m_telnet_exist;
    uint16_t                      m_telnet_port;

    uint16_t                      m_zmq_rpc_port;

    bool                       m_mac_info_exist;
    std::vector <CMacYamlInfo> m_mac_info;
    CPlatformMemoryYamlInfo     m_memory;
    CPlatformCoresYamlInfo      m_platform;

public:
    std::string get_use_if_comma_seperated();
    void Dump(FILE *fd);
    int load_from_yaml_file(std::string file_name);
};



#endif
