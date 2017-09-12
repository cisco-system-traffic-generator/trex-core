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

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include "common/basic_utils.h"
#include "utl_yaml.h"
#include "platform_cfg.h"
#include "utl_yaml.h"
#include "tunnel.h"

void CPlatformMemoryYamlInfo::reset(){
       int i;
       i=0;
       for (i=0; i<MBUF_ELM_SIZE; i++) {
           m_mbuf[i] = CONST_NB_MBUF_2_10G;
       }
       m_mbuf[MBUF_64]           = m_mbuf[MBUF_64]*2;

       m_mbuf[MBUF_2048]         = CONST_NB_MBUF_2_10G/2;

       m_mbuf[MBUF_4096]         = 128;
       m_mbuf[MBUF_9k]           = 512;


       m_mbuf[TRAFFIC_MBUF_64]           = m_mbuf[MBUF_64] * 4;
       m_mbuf[TRAFFIC_MBUF_128]           = m_mbuf[MBUF_128] * 4;

       m_mbuf[TRAFFIC_MBUF_2048]         = CONST_NB_MBUF_2_10G * 8;

       m_mbuf[TRAFFIC_MBUF_4096]         = 128;
       m_mbuf[TRAFFIC_MBUF_9k]           = 512;


       m_mbuf[MBUF_DP_FLOWS]     = (1024*1024/2);
       m_mbuf[MBUF_GLOBAL_FLOWS] =(10*1024/2);
}

const std::string names []={
                   "MBUF_64",
                   "MBUF_128",
                   "MBUF_256",
                   "MBUF_512",
                   "MBUF_1024",
                   "MBUF_2048",
                   "MBUF_4096",
                   "MBUF_9K",

                   "TRAFFIC_MBUF_64",
                   "TRAFFIC_MBUF_128",
                   "TRAFFIC_MBUF_256",
                   "TRAFFIC_MBUF_512",
                   "TRAFFIC_MBUF_1024",
                   "TRAFFIC_MBUF_2048",
                   "TRAFFIC_MBUF_4096",
                   "TRAFFIC_MBUF_9K",

                   "MBUF_DP_FLOWS",
                   "MBUF_GLOBAL_FLOWS"

};

const std::string * get_mbuf_names(void){
  return names;
}

void CPlatformDualIfYamlInfo::Dump(FILE *fd){
    fprintf(fd,"    socket  : %d  \n",m_socket);
    int i;
    fprintf(fd,"   [  ");
    for (i=0; i<m_threads.size(); i++) {
        fprintf(fd," %d  ",(int)m_threads[i]);
    }
    fprintf(fd,"   ]  \n");
}

void CPlatformCoresYamlInfo::Dump(FILE *fd){
    if ( m_is_exists == false){
       fprintf(fd," no platform info \n");
       return;
    }
    fprintf(fd," master   thread  : %d  \n", m_master_thread);
    fprintf(fd," rx  thread  : %d  \n", m_rx_thread);
    int i;
    for (i=0; i<m_dual_if.size(); i++) {
        printf(" dual_if : %d \n",i);
        CPlatformDualIfYamlInfo * lp=&m_dual_if[i];
        lp->Dump(fd);
    }
}

void operator >> (const YAML::Node& node, CPlatformDualIfYamlInfo & plat_info) {
    node["socket"] >> plat_info.m_socket;
    const YAML::Node& threads = node["threads"];
    /* fill the vector*/
    for(unsigned i=0;i<threads.size();i++) {
        uint32_t fi;
        const YAML::Node & node = threads;
        node[i]  >> fi;
        plat_info.m_threads.push_back(fi);
    }
}

void operator >> (const YAML::Node& node, CPlatformCoresYamlInfo & plat_info) {
     node["master_thread_id"] >> plat_info.m_master_thread;
     if (node.FindValue("rx_thread_id")) {
         node["rx_thread_id"] >> plat_info.m_rx_thread;
     } else {
         // Obolete option.
         if (node.FindValue("latency_thread_id")) {
             node["latency_thread_id"] >> plat_info.m_rx_thread;
         } else {
             node["rx_thread_id"] >> plat_info.m_rx_thread; // do this to get the error message
         }
     }

     const YAML::Node& dual_info = node["dual_if"];
     for(unsigned i=0;i<dual_info.size();i++) {
        CPlatformDualIfYamlInfo  fi;
         dual_info[i]  >> fi;
         plat_info.m_dual_if.push_back(fi);
     }
}

void CPlatformMemoryYamlInfo::Dump(FILE *fd){
    fprintf(fd," memory per 2x10G ports  \n");
    const std::string * names =get_mbuf_names();

    int i=0;
    for (i=0; i<MBUF_ELM_SIZE; i++) {
        fprintf(fd," %-40s  : %lu \n",names[i].c_str(), (ulong)m_mbuf[i]);
    }
}

void CMacYamlInfo::copy_dest(char *p){
    assert(m_dest_base.size() == 6);
    int i;
    for (i=0; i<m_dest_base.size(); i++) {
        p[i]=m_dest_base[i];
    }
}

void CMacYamlInfo::copy_src(char *p){
        assert(m_src_base.size() == 6);
        int i;
        for (i=0; i<m_src_base.size(); i++) {
            p[i]=m_src_base[i];
        }
}

uint32_t CMacYamlInfo::get_def_gw() {
    return m_def_gw;
}

uint32_t CMacYamlInfo::get_ip() {
    return m_ip;
}

uint32_t CMacYamlInfo::get_mask() {
    return m_mask;
}

uint32_t CMacYamlInfo::get_vlan() {
    return m_vlan;
}

void CMacYamlInfo::Dump(FILE *fd){
    if (m_dest_base.size() != 6) {
        fprintf(fd,"ERROR in dest mac addr \n");
        return;
    }
    if (m_src_base.size() != 6) {
        fprintf(fd,"ERROR in src mac addr \n");
        return;
    }
    fprintf (fd," src     : ");
    dump_mac_vector( m_src_base,fd);
    fprintf (fd," dest    : ");
    dump_mac_vector( m_dest_base,fd);

}

void operator >> (const YAML::Node& node, CMacYamlInfo & mac_info) {
    uint32_t fi;
    bool res;
    std::string mac_str;

    if (node.FindValue("dest_mac")) {
        const YAML::Node& dmac = node["dest_mac"];
        if (dmac.Type() == YAML::NodeType::Sequence) { // [1,2,3,4,5,6]
            ASSERT_MSG(dmac.size() == 6, "Array of dest MAC should have 6 elements.");
            for(unsigned i=0;i<dmac.size();i++) {
                dmac[i]  >> fi;
                mac_info.m_dest_base.push_back(fi);
            }
        }
        else if (dmac.Type() == YAML::NodeType::Scalar) { // "12:34:56:78:9a:bc"
            dmac >> mac_str;
            res = mac2vect(mac_str, mac_info.m_dest_base);
            ASSERT_MSG(res && mac_info.m_dest_base.size() == 6
                       , "String of dest MAC should be in format '12:34:56:78:9a:bc'.");
        }
    } else {
        for(unsigned i = 0; i < 6; i++) {
            mac_info.m_dest_base.push_back(0);
        }
    }

    if (node.FindValue("src_mac")) {
        const YAML::Node& smac = node["src_mac"];
        if (smac.Type() == YAML::NodeType::Sequence) {
            ASSERT_MSG(smac.size() == 6, "Array of src MAC should have 6 elements.");
            for(unsigned i=0;i<smac.size();i++) {
                smac[i]  >> fi;
                mac_info.m_src_base.push_back(fi);
            }
        }
        else if (smac.Type() == YAML::NodeType::Scalar) {
            smac >> mac_str;
            res = mac2vect(mac_str, mac_info.m_src_base);
            ASSERT_MSG(res && mac_info.m_src_base.size() == 6
                       , "String of src MAC should be in format '12:34:56:78:9a:bc'.");
        }
    } else {
        for(unsigned i = 0; i < 6; i++) {
            mac_info.m_src_base.push_back(0);
        }
    }

    if (! utl_yaml_read_ip_addr(node, "default_gw", mac_info.m_def_gw)) {
        mac_info.m_def_gw = 0;
    }

    if (! utl_yaml_read_ip_addr(node, "ip", mac_info.m_ip)) {
        mac_info.m_ip = 0;
    }
    if (! utl_yaml_read_ip_addr(node, "mask", mac_info.m_mask)) {
        mac_info.m_mask = 0;
    }
    if (! utl_yaml_read_uint16(node, "vlan", mac_info.m_vlan, 0, 0xfff)) {
        mac_info.m_vlan = 0;
    }

    if (node.FindValue("tunnels")) {
        Tunnel::Parse(node["tunnels"], mac_info.m_tunnels);
    }
}

void operator >> (const YAML::Node& node, CPlatformMemoryYamlInfo & plat_info) {
    if ( node.FindValue("mbuf_64") ){
        node["mbuf_64"] >> plat_info.m_mbuf[MBUF_64];
    }

    if ( node.FindValue("mbuf_128") ){
        node["mbuf_128"] >> plat_info.m_mbuf[MBUF_128];
    }

    if ( node.FindValue("mbuf_256") ){
        node["mbuf_256"] >> plat_info.m_mbuf[MBUF_256];
    }

    if ( node.FindValue("mbuf_512") ){
        node["mbuf_512"] >> plat_info.m_mbuf[MBUF_512];
    }

    if ( node.FindValue("mbuf_1024") ){
        node["mbuf_1024"] >> plat_info.m_mbuf[MBUF_1024];
    }

    if ( node.FindValue("mbuf_2048") ){
        node["mbuf_2048"] >> plat_info.m_mbuf[MBUF_2048];
    }

    if ( node.FindValue("mbuf_4096") ){
        node["mbuf_4096"] >> plat_info.m_mbuf[MBUF_4096];
    }

    if ( node.FindValue("mbuf_9k") ){
        node["mbuf_9k"] >> plat_info.m_mbuf[MBUF_9k];
    }


    if ( node.FindValue("traffic_mbuf_64") ){
        node["traffic_mbuf_64"] >> plat_info.m_mbuf[TRAFFIC_MBUF_64];
    }

    if ( node.FindValue("traffic_mbuf_128") ){
        node["traffic_mbuf_128"] >> plat_info.m_mbuf[TRAFFIC_MBUF_128];
    }

    if ( node.FindValue("traffic_mbuf_256") ){
        node["traffic_mbuf_256"] >> plat_info.m_mbuf[TRAFFIC_MBUF_256];
    }

    if ( node.FindValue("traffic_mbuf_512") ){
        node["traffic_mbuf_512"] >> plat_info.m_mbuf[TRAFFIC_MBUF_512];
    }

    if ( node.FindValue("traffic_mbuf_1024") ){
        node["traffic_mbuf_1024"] >> plat_info.m_mbuf[TRAFFIC_MBUF_1024];
    }

    if ( node.FindValue("traffic_mbuf_2048") ){
        node["traffic_mbuf_2048"] >> plat_info.m_mbuf[TRAFFIC_MBUF_2048];
    }

    if ( node.FindValue("traffic_mbuf_4096") ){
        node["traffic_mbuf_4096"] >> plat_info.m_mbuf[TRAFFIC_MBUF_4096];
    }

    if ( node.FindValue("traffic_mbuf_9k") ){
        node["traffic_mbuf_9k"] >> plat_info.m_mbuf[TRAFFIC_MBUF_9k];
    }


    if ( node.FindValue("dp_flows") ){
        node["dp_flows"] >> plat_info.m_mbuf[MBUF_DP_FLOWS];
    }

    if ( node.FindValue("global_flows") ){
        node["global_flows"] >> plat_info.m_mbuf[MBUF_GLOBAL_FLOWS];
    }

}

void operator >> (const YAML::Node& node, CPlatformYamlInfo & plat_info) {
    if (node.FindValue("interface_mask")) {
        printf("WARNING interface_mask in not used any more !\n");
    }

    /* must have interfaces */
    const YAML::Node& interfaces = node["interfaces"];
    if ( interfaces.size() > TREX_MAX_PORTS ) {
        printf("ERROR: Maximal number of interfaces is: %d, you have specified: %d.\n",
                    TREX_MAX_PORTS, (int) interfaces.size());
        exit(-1);
    }

    for(unsigned i=0;i<interfaces.size();i++) {
        std::string  fi;
        const YAML::Node & node = interfaces;
        node[i]  >> fi;
        plat_info.m_if_list.push_back(fi);
    }


    if ( node.FindValue("port_limit") ){
        node["port_limit"] >> plat_info.m_port_limit;
        plat_info.m_port_limit_exist=true;
    }


    plat_info.m_enable_zmq_pub_exist = true;

    if ( node.FindValue("enable_zmq_pub") ){
        node["enable_zmq_pub"] >> plat_info.m_enable_zmq_pub;
        plat_info.m_enable_zmq_pub_exist = true;
    }

    if ( node.FindValue("zmq_pub_port") ){
        node["zmq_pub_port"] >> plat_info.m_zmq_pub_port;
        plat_info.m_enable_zmq_pub_exist = true;
    }

    if ( node.FindValue("prefix") ){
        node["prefix"] >> plat_info.m_prefix;
    }

    if ( node.FindValue("limit_memory") ){
        node["limit_memory"] >> plat_info.m_limit_memory;
    }

    if ( node.FindValue("c") ){
        node["c"] >> plat_info.m_thread_per_dual_if;
    }

    if ( node.FindValue("telnet_port") ){
        node["telnet_port"] >> plat_info.m_telnet_port;
        plat_info.m_telnet_exist=true;
    }

    if ( node.FindValue("zmq_rpc_port") ){
        node["zmq_rpc_port"] >> plat_info.m_zmq_rpc_port;
    }

    if ( node.FindValue("port_bandwidth_gb") ){
        node["port_bandwidth_gb"] >> plat_info.m_port_bandwidth_gb;
    }

    if ( node.FindValue("memory") ){
        node["memory"] >> plat_info.m_memory;
    }

    if ( node.FindValue("platform") ){
        node["platform"] >> plat_info.m_platform;
        plat_info.m_platform.m_is_exists=true;
    }

    if ( node.FindValue("tw") ){
        node["tw"] >> plat_info.m_tw;
    }


    if ( node.FindValue("port_info")  ) {
        const YAML::Node& mac_info = node["port_info"];
        for(unsigned i=0;i<mac_info.size();i++) {
           CMacYamlInfo  fi;
            const YAML::Node & node =mac_info;
            node[i]  >> fi;
            plat_info.m_mac_info.push_back(fi);
        }
        plat_info.m_mac_info_exist = true;
    }
}

int CPlatformYamlInfo::load_from_yaml_file(std::string file_name){
    reset();
    m_info_exist =true;

    if ( !utl_is_file_exists(file_name) ){
        printf(" ERROR file %s does not exists \n",file_name.c_str());
        exit(-1);
    }

    try {
       std::ifstream fin((char *)file_name.c_str());
       YAML::Parser parser(fin);
       YAML::Node doc;

       parser.GetNextDocument(doc);
       for(unsigned i=0;i<doc.size();i++) {
          doc[i] >> *this;
          break;
       }
    } catch ( const std::exception& e ) {
        std::cout << e.what() << "\n";
        exit(-1);
    }
    return (0);
}

std::string CPlatformYamlInfo::get_use_if_comma_seperated(){
    std::string s="";
    int i;
    for (i=0; i<(int)m_if_list.size()-1; i++) {
        s+=m_if_list[i]+",";
    }
    s+=m_if_list[i];
    return (s);
}

void CPlatformYamlInfo::Dump(FILE *fd){
    if ( m_info_exist ==false ){
        fprintf(fd," file info does not exist  \n");
        return;
    }

    if (m_port_limit_exist && (m_port_limit != 0xffffffff)) {
        fprintf(fd," port limit     :  %d \n",m_port_limit);
    }else{
        fprintf(fd," port limit     :  not configured \n");
    }
    fprintf(fd," port_bandwidth_gb    :  %lu \n", (ulong)m_port_bandwidth_gb);

    if ( m_if_mask_exist && m_if_mask.size() ) {
        fprintf(fd," if_mask        : ");
        int i;
        for (i=0; i<(int)m_if_mask.size(); i++) {
            fprintf(fd," %s,",m_if_mask[i].c_str());
        }
        fprintf(fd,"\n");

    }else{
        fprintf(fd," if_mask        : None \n");
    }

    if ( m_prefix.length() ){
        fprintf(fd," prefix              : %s \n",m_prefix.c_str());
    }
    if ( m_limit_memory.length() ){
        fprintf(fd," limit_memory        : %s \n",m_limit_memory.c_str());
    }
    fprintf(fd," thread_per_dual_if      : %d \n",(int)m_thread_per_dual_if);

    fprintf(fd," if        : ");
    int i;
    for (i=0; i<(int)m_if_list.size(); i++) {
        fprintf(fd," %s,",m_if_list[i].c_str());
    }
    fprintf(fd,"\n");

    if ( m_enable_zmq_pub_exist ){
        fprintf(fd," enable_zmq_pub :  %d \n",m_enable_zmq_pub?1:0);
        fprintf(fd," zmq_pub_port   :  %d \n",m_zmq_pub_port);
    }
    if ( m_telnet_exist ){
        fprintf(fd," telnet_port    :  %d \n",m_telnet_port);

    }
    fprintf(fd," m_zmq_rpc_port    :  %d \n",m_zmq_rpc_port);

    if ( m_mac_info_exist ){
        int i;
        for (i=0; i<(int)m_mac_info.size(); i++) {
            m_mac_info[i].Dump(fd);
        }
    }
    m_memory.Dump(fd);
    m_platform.Dump(fd);
    m_tw.Dump(fd);
}
