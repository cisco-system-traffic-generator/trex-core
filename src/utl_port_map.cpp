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

#include "utl_port_map.h"
#include <string.h>


void CPciPortCfgDesc::dump(FILE *fd){
    fprintf(fd," %s:%d \n",(char *)m_pci.c_str(),(int)m_id);

}

 int norm_pci_str(std::string pci_n,
                  std::string & out,
                  std::string & err){
    #define BUF_MAX 200
    char buf[BUF_MAX];
    const char *p= pci_n.c_str();

    rte_pci_addr addr;
    if (eal_parse_pci_BDF(p, &addr) != 0 && 
        eal_parse_pci_DomBDF(p, &addr) != 0) {
        snprintf(buf,BUF_MAX,"ERROR not valid PCI addr %s", p);
        err=std::string(buf);
        return(0);
    }
    rte_pci_device_name(&addr,
                            buf, BUF_MAX);
    out =std::string(buf);
    return(0);
}

void CPciPortCfgDesc::update_name(){
    char buf[100];
    snprintf(buf,100,"%s/%d",m_pci.c_str(),m_id);
    /* norm the name */
    m_name = std::string(buf);
}


int CPciPortCfgDesc::parse(std::string & err){


    const char *p=m_name.c_str();
    int len = strlen(m_name.c_str());
    int i; 
    int index=-1;
    for (i=0; i<len; i++ ) {
        if (*p=='/') {
            index=i;
            break;
        }
        p++;
    }

    /* no delimiter*/
    if (index<0) {
        if (norm_pci_str(m_name,m_pci,err)!=0){
            return(-1);
        }
        m_name = m_pci;
        m_id=0;
        return(0);
    }

    if ( index!=(len-2) ) {
        err="ERROR input should have the format dd:dd.d/[d] for example 03:00.0/1  you have: "+ m_name;
        return(-1);
    }

    if ( !isdigit(p[1])){
        err="ERROR last char should be int, should have the format dd:dd.d/[d] for example 03:00.0/1 you have: "+ m_name;
        return(-1);
    }

    if (norm_pci_str(m_name.substr (0,index),m_pci,err)!=0){
        return(-1);
    }

    std::string id  = m_name.substr (index+1,len); 
    m_id=atoi(id.c_str());

    update_name();
    return (0);
}


CPciPorts::CPciPorts(){

}


CPciPorts::~CPciPorts(){
    m_eal_init_vec.clear();
    m_dpdk_pci_scan.clear();
    m_last_result.clear();
    free_cfg_args(m_vec);
    free_cfg_args(m_scan_vec);
}


void CPciPorts::add_dpdk(std::string name){
    int i;
    for (i=0; i<m_eal_init_vec.size(); i++) {
          std::string dname=m_eal_init_vec[i];
          if ( dname == name){
              return;
          }
    }
    m_eal_init_vec.push_back(name);
}


int CPciPorts::set_cfg_input(dpdk_input_args_t & vec,
                             std::string & err){
    dpdk_map_name_to_int_t  map_name;

    m_eal_init_vec.clear();
    m_vec.clear();
    m_scan_vec.clear();
    m_dpdk_pci_scan.clear();
    m_last_result.clear();

    int i;
    for (i=0; i<vec.size(); i++) {
        std::string iname=vec[i];
        m_input.push_back(iname);
        if ( iname == "dummy" ) {
            continue;
        }

        CPciPortCfgDesc * lp= new CPciPortCfgDesc();
        lp->set_name(iname);
        
        if ( lp->parse(err)!=0){
            return(-1);
        }
        m_vec.push_back(lp);
        iname = lp->get_name();

        dpdk_map_name_to_int_iter_t it = map_name.find(iname);

        /* already there */
        if ( it != map_name.end() ){
            /* exist */
            err="ERROR two key with the same value  "+ iname;
            return(-1);
        }
        map_name.insert(std::make_pair(iname,1));

        add_dpdk(lp->get_pci());
    }
    return (0);
}

/*
cfg :
06:00/1 06:00/0 02:00/0  02:00/1

init (both DPDK and script) : 02:00  06:00 

scan: 02:00 02:00 06:00 06:00

map: 3  2  1  0
*/

int CPciPorts::get_map_args(dpdk_input_args_t & dpdk_scan, 
                             dpdk_map_args_t & port_map,
                             std::string & err){
    /* remember the scan */

    int i;
    /* norm the pci name */
    for (i=0; i<dpdk_scan.size(); i++) {
        std::string npci;
        if (norm_pci_str(dpdk_scan[i],npci,err)!=0){
            return(-1);
        }
        m_dpdk_pci_scan.push_back(npci);
    }

    /* check counter */
    if ( dpdk_scan.size() < m_vec.size() ) {
        err ="ERROR scan size should be equal of bigger than input";
        return (-1);
    }

    dpdk_map_name_to_int_t  m_map_scan; /* scan index */

    for (i=0; i<m_dpdk_pci_scan.size(); i++) {
        std::string pci = m_dpdk_pci_scan[i];
        uint8_t val=0;
        dpdk_map_name_to_int_iter_t it = m_map_scan.find(pci);

        /* already there */
        if ( it != m_map_scan.end() ){
            val=(*it).second+1;
            it->second=val;
        }else{
            m_map_scan.insert(std::make_pair(pci,0));
            val=0;
        }
        CPciPortCfgDesc * lp= new CPciPortCfgDesc();
        lp->set_pci(pci);
        lp->set_id(val);
        m_scan_vec.push_back(lp);
    }

    uint8_t index;
    uint8_t m_vec_index = 0;
    for (i=0; i<m_input.size(); i++) {
        if ( m_input[i] == "dummy" ) {
            port_map.push_back(DPDK_MAP_IVALID_REPID);
        } else {
            CPciPortCfgDesc * lp = m_vec[m_vec_index];
            m_vec_index++;
            if ( find(lp, index, err)!=0){
                return(-1);
            }
            port_map.push_back(index);
        }
    }
    m_last_result = port_map;
    return(0);
}


/* could be done faster with hash but who care for max of 16 ports ..*/
int CPciPorts::find(CPciPortCfgDesc * lp,
                    uint8_t & index, 
                    std::string & err){
    int i;
    for (i=0; i<m_scan_vec.size(); i++) {
        if (lp->compare(m_scan_vec[i]) ) {
            index=(uint8_t)i;
            return(0);
        }
    }
    err = "ERROR can't find object";
    lp->dump(stdout);
    return(-1);
}



int CPciPorts::dump_vec(FILE *fd,
                        std::string vec_name,
                        dpdk_input_args_t & vec){
    fprintf(fd," %20s : [", vec_name.c_str());
    int i;
    int len=vec.size();
    for (i=0; i<len; i++) {
        fprintf(fd,"%s", vec[i].c_str());
        if (i<len-1) {
            fprintf(fd,", ");
        }
    }
    fprintf(fd,"]\n");
    return(0);
}


void CPciPorts::free_cfg_args(dpdk_cfg_args_t & v){
    int i;
    for (i=0; i<v.size(); i++) {
        CPciPortCfgDesc * lp=v[i];
        assert(lp);
        delete lp;
    }
    v.clear();
}

int CPciPorts::dump_vec_int(FILE *fd,
                        std::string vec_name,
                        dpdk_map_args_t & vec){
    fprintf(fd," %20s : [ ", vec_name.c_str());
    int i;
    int len=vec.size();
    for (i=0; i<len; i++) {
        fprintf(fd,"%d", vec[i]);
        if (i<len-1) {
            fprintf(fd,", ");
        }
    }
    fprintf(fd,"]\n");
    return(0);
}



void CPciPorts::dump(FILE *fd){
    dump_vec(fd,"input",m_input);
    dump_vec(fd,"dpdk",m_eal_init_vec);
    dump_vec(fd,"pci_scan",m_dpdk_pci_scan);
    dump_vec_int(fd,"map",m_last_result);
}





