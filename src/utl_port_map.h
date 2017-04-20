#ifndef UTL_PORT_MAPPER_H
#define UTL_PORT_MAPPER_H

/*
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

/* 

This file should be included only in DPDK specific files like main_dpdk.cpp, 
general trex core always works in virtual ports id 

*/ 

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>
#include <map>
#include "rte_pci.h"
                                  

int  norm_pci_str(std::string pci_i,
                  std::string & out,
                  std::string & err);

class CPciPortCfgDesc {

public:
    void set_name(std::string name){
        m_name=name;
    }

    std::string get_name(){
        return(m_name);
    }

    int parse(std::string & err);

    void set_pci(std::string name){
        m_pci=name;
    }

    void update_name();

    std::string  get_pci(){
        return (m_pci);
    }
    uint8_t      get_id(){
        return(m_id);
    }
    void set_id(uint8_t id){
        m_id=id;
    }


    void dump(FILE *fd);

    bool compare(CPciPortCfgDesc * rhs){ 
        return (( get_id() == rhs->get_id() ) && (get_pci() == rhs->get_pci() ) );
    }

private:
  std::string   m_name; /* format 0000:03:00.0/1 or 0000:03:00.0 or 0000:03:00.0 */
  std::string   m_pci;  /* 03:00.0 */
  uint8_t       m_id;   /* id */
};


typedef  std::vector<std::string>        dpdk_input_args_t;      /* input to DPDK */ 
typedef  std::vector<CPciPortCfgDesc *>  dpdk_cfg_args_t; /* config args */
typedef  std::vector<uint8_t>            dpdk_map_args_t; /* config args */
typedef std::map<std::string, int>       dpdk_map_name_to_int_t;
typedef std::map<std::string, int>::iterator dpdk_map_name_to_int_iter_t;


class CPciPorts  {
public:
    CPciPorts();
    ~CPciPorts();

    /* fill dpdk input '03:00.0/1' or '03:00.0' */
    int set_cfg_input(dpdk_input_args_t & vec,
                      std::string & err);

    /* fill dpdk in format  '03:00.0' */
    dpdk_input_args_t * get_dpdk_input_args(){
        return (&m_eal_init_vec);
    }

    /* input scan and get port map */
    int get_map_args(dpdk_input_args_t & dpdk_scan, 
                      dpdk_map_args_t & port_map,
                      std::string & err);

    dpdk_cfg_args_t * get_input(){
        return (&m_vec);
    }

    uint8_t get_max_num_ports(){
        return (m_input.size());

    }

    void dump(FILE *fd);

private:

    void add_dpdk(std::string name);

    int find(CPciPortCfgDesc * lp,
             uint8_t & index, 
             std::string & err);

    int dump_vec(FILE *fd,
                 std::string vec_name,
                 dpdk_input_args_t & vec);

    int dump_vec_int(FILE *fd,
                     std::string vec_name,
                     dpdk_map_args_t & vec);

    void free_cfg_args(dpdk_cfg_args_t & v);
private:

   dpdk_input_args_t    m_input;         /* input string */
   dpdk_cfg_args_t      m_vec;           /* parse input */ 
   dpdk_cfg_args_t      m_scan_vec;      /* parse scan include the scan better format*/
   dpdk_input_args_t    m_eal_init_vec;  /* dpdk string */
   dpdk_input_args_t    m_dpdk_pci_scan; /* output from dpdk */
   dpdk_map_args_t      m_last_result;   /* save result */ 
};



#endif
