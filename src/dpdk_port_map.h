#ifndef DPDK_PORT_MAPPER_H
#define DPDK_PORT_MAPPER_H
#include <assert.h>
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

it maps betwean virtual trex port to dpdk ports (for cases that there is no 1:1 map 

*/ 

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "utl_port_map.h"



#define DPDK_MAP_IVALID_REPID (255)

typedef uint8_t tvpid_t; /* port ID of trex 0,1,2,3 up to MAX_PORTS*/
typedef uint8_t repid_t; /* DPDK port id  */


class CTRexPortMapper {
public:
    CTRexPortMapper();

    void set(uint8_t rte_ports,dpdk_map_args_t   &   pmap);


    uint8_t get_tv_nports(){
        return(m_max_trex_vports);
    }
    void set_tv_nports(uint8_t vports){
        m_max_trex_vports=vports;
    }

    uint8_t get_re_nports(){
        return(m_max_rte_eth_ports);
    }

    void set_re_nport(uint8_t reports){
        m_max_rte_eth_ports=reports;

    }

    /* map  tvpid ->repid */
    void set_map(tvpid_t tvpid,repid_t repid){
        assert(repid<TREX_MAX_PORTS);
        m_map[tvpid]=repid;
    }

    /* map  repid -> tvpid  */
    void set_rmap(repid_t repid,tvpid_t tvpid){
        assert(repid<TREX_MAX_PORTS);
        m_rmap[repid]=tvpid;
    }

public:

    tvpid_t  get_tvpid(repid_t repid){
        assert(repid<m_max_rte_eth_ports);
        tvpid_t tvpid=m_rmap[repid];
        assert(tvpid<m_max_trex_vports);
        return(tvpid);
    }

    repid_t   get_repid(tvpid_t tvpid){
        assert(tvpid<m_max_trex_vports);
        repid_t repid=m_map[tvpid];
        assert(repid<m_max_rte_eth_ports);
        return(repid);
    }

    void Dump(FILE *fd);


    static CTRexPortMapper * Ins();

private:
    tvpid_t            m_max_trex_vports;    /* how many trex vports */
    repid_t            m_max_rte_eth_ports;  /* how many rte_eth ports */
    repid_t            m_map[TREX_MAX_PORTS];   /* map from  input :tvpid_t output: repid_t (phy) */
    tvpid_t            m_rmap[TREX_MAX_PORTS]; /* reverse map map from  input :repid_t  output: tvpid_t (virt)   */

    static CTRexPortMapper * m_ins;
};

/* trex virtual port */
class CTVPort {
public:
    CTVPort(tvpid_t tvport){
        m_tvport=tvport;
    }

    /* This API should *not* be used in performance sensetive location */
    repid_t get_repid(){
        CTRexPortMapper * lp=CTRexPortMapper::Ins();
        return (lp->get_repid(m_tvport));
    }

    tvpid_t tvpid(void){
        return(m_tvport);
    }

private:
    tvpid_t  m_tvport;
};

/* rte eth port id */
class CREPort {
public:
    CREPort(repid_t repid){
        m_repid=repid;
    }

    /* This API should *not* be used in performance sensetive location */
    tvpid_t get_tvpid(){
        CTRexPortMapper * lp=CTRexPortMapper::Ins();
        return (lp->get_tvpid(m_repid));
    }

    repid_t  repid(void){
        return(m_repid);
    }

private:
    repid_t  m_repid;
};



#endif
