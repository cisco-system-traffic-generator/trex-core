/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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
#include "trex_platform.h"
#include "platform_cfg.h"


bool CPlatformSocketInfoNoConfig::is_sockets_enable(socket_id_t socket){
    if ( socket==0 ) {
        return(true);
    }
    return (false);
}

socket_id_t CPlatformSocketInfoNoConfig::max_num_active_sockets(){
    return (1);
}


socket_id_t CPlatformSocketInfoNoConfig::port_to_socket(port_id_t port){
    return (0);
}


void CPlatformSocketInfoNoConfig::set_rx_thread_is_enabled(bool enable) {
    m_rx_is_enabled = enable;
}

void CPlatformSocketInfoNoConfig::set_number_of_dual_ports(uint8_t num_dual_ports){
    m_dual_if   = num_dual_ports;
}


void CPlatformSocketInfoNoConfig::set_number_of_threads_per_ports(uint8_t num_threads){
    m_threads_per_dual_if = num_threads;
}

bool CPlatformSocketInfoNoConfig::sanity_check(){
    return (true);
}

/* return the core mask */
uint64_t CPlatformSocketInfoNoConfig::get_cores_mask(){

    uint32_t cores_number = m_threads_per_dual_if*m_dual_if;
    if ( m_rx_is_enabled ) {
        cores_number +=   2;
    }else{
        cores_number += 1; /* only MASTER*/
    }
    int i;
    int offset=0;
    /* master */
    uint64_t res=1;
    uint64_t mask=(1LL<<(offset+1));
    for (i=0; i<(cores_number-1); i++) {
        res |= mask ;
        mask = mask <<1;
   }
   return (res);
}

virtual_thread_id_t CPlatformSocketInfoNoConfig::thread_phy_to_virt(physical_thread_id_t  phy_id){
    return (phy_id);
}

physical_thread_id_t CPlatformSocketInfoNoConfig::thread_virt_to_phy(virtual_thread_id_t virt_id){
    return (virt_id);
}

physical_thread_id_t CPlatformSocketInfoNoConfig::get_master_phy_id() {
    return (0);
}

bool CPlatformSocketInfoNoConfig::thread_phy_is_rx(physical_thread_id_t  phy_id){
    return (phy_id==(m_threads_per_dual_if*m_dual_if+1));
}


void CPlatformSocketInfoNoConfig::dump(FILE *fd){
    fprintf(fd," there is no configuration file given \n");
}

////////////////////////////////////////

bool CPlatformSocketInfoConfig::Create(CPlatformCoresYamlInfo * platform){
    m_platform=platform;
    assert(m_platform);
    assert(m_platform->m_is_exists);
    reset();
    return (true);
}

bool CPlatformSocketInfoConfig::init(){

    /* iterate the sockets */
    uint32_t num_threads=0;
    uint32_t num_dual_if = m_platform->m_dual_if.size();

    if ( m_num_dual_if > num_dual_if ){
        printf("ERROR number of dual if %d is higher than defined in configuration file %d\n",
               (int)m_num_dual_if,
               (int)num_dual_if);
    }

    int i;
    for (i=0; i<m_num_dual_if; i++) {
        CPlatformDualIfYamlInfo * lp=&m_platform->m_dual_if[i];
        if ( lp->m_socket>=MAX_SOCKETS_SUPPORTED ){
            printf("ERROR socket %d is bigger than max %d \n",lp->m_socket,MAX_SOCKETS_SUPPORTED);
            exit(1);
        }

        if (!m_sockets_enable[lp->m_socket] ) {
            m_sockets_enable[lp->m_socket]=true;
            m_sockets_enabled++;
        }

        m_socket_per_dual_if[i]=lp->m_socket;

        /* learn how many threads per dual-if */
        if (i==0) {
            num_threads = lp->m_threads.size();
            m_max_threads_per_dual_if = num_threads;
        }else{
            if (lp->m_threads.size() != num_threads) {
                printf("ERROR, the number of threads per dual ports should be the same for all dual ports\n");
                exit(1);
            }
        }

        if (m_threads_per_dual_if > m_max_threads_per_dual_if) {
            printf("ERROR: Maximum threads in platform section of config file is %d, unable to run with -c %d.\n",
                    m_max_threads_per_dual_if, m_threads_per_dual_if);
            printf("Please increase the pool in config or use lower -c.\n");
            exit(1);
        }

            int j;

            for (j=0; j<m_threads_per_dual_if; j++) {
                uint8_t virt_thread = 1+ i + j*m_num_dual_if; /* virtual thread */
                uint8_t phy_thread  = lp->m_threads[j];

                if (phy_thread>MAX_THREADS_SUPPORTED) {
                    printf("ERROR, physical thread id is %d higher than max %d \n",phy_thread,MAX_THREADS_SUPPORTED);
                    exit(1);
                }

                if (virt_thread>MAX_THREADS_SUPPORTED) {
                    printf("ERROR virtual thread id is %d higher than max %d \n",virt_thread,MAX_THREADS_SUPPORTED);
                    exit(1);
                }

                if ( m_thread_phy_to_virtual[phy_thread] ){
                    printf("ERROR physical thread %d defined twice\n",phy_thread);
                    exit(1);
                }
                m_thread_phy_to_virtual[phy_thread]=virt_thread;
                m_thread_virt_to_phy[virt_thread] =phy_thread;
            }
    }

    if ( m_thread_phy_to_virtual[m_platform->m_master_thread] ){
        printf("ERROR physical master thread %d already defined  \n",m_platform->m_master_thread);
        exit(1);
    }

    if ( m_thread_phy_to_virtual[m_platform->m_rx_thread] ){
        printf("ERROR physical latency thread %d already defined \n",m_platform->m_rx_thread);
        exit(1);
    }

    if (m_max_threads_per_dual_if < m_threads_per_dual_if ) {
        printf("ERROR number of threads asked per dual if is %d lower than max %d \n",
               (int)m_threads_per_dual_if,
               (int)m_max_threads_per_dual_if);
        exit(1);
    }
    return (true);
}


void CPlatformSocketInfoConfig::dump(FILE *fd){
    fprintf(fd," core_mask  %llx  \n",(unsigned long long)get_cores_mask());
    fprintf(fd," sockets :");
    int i;
    for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
        if ( is_sockets_enable(i) ){
            fprintf(fd," %d ",i);
        }
    }
    fprintf(fd," \n");
    fprintf(fd," active sockets : %d \n",max_num_active_sockets());

    fprintf(fd," ports_sockets : %d \n",max_num_active_sockets());

    for (i = 0; i <  TREX_MAX_PORTS; i++) {
        fprintf(fd,"%d,",port_to_socket(i));
    }
    fprintf(fd,"\n");

    fprintf(fd," phy   |   virt   \n");
    for (i=0; i<MAX_THREADS_SUPPORTED; i++) {
        virtual_thread_id_t virt=thread_phy_to_virt(i);
        if ( virt ){
            fprintf(fd," %d      %d   \n",i,virt);
        }
    }
}


void CPlatformSocketInfoConfig::reset(){
    m_sockets_enabled=0;
    int i;
    for (i=0; i<MAX_SOCKETS_SUPPORTED; i++) {
        m_sockets_enable[i]=false;
    }

    for (i=0; i<MAX_THREADS_SUPPORTED; i++) {
        m_thread_virt_to_phy[i]=0;
    }
    for (i=0; i<MAX_THREADS_SUPPORTED; i++) {
        m_thread_phy_to_virtual[i]=0;
    }
    for (i = 0; i < TREX_MAX_PORTS >> 1; i++) {
        m_socket_per_dual_if[i]=0;
    }

    m_num_dual_if=0;

    m_threads_per_dual_if=0;
    m_rx_is_enabled=false;
    m_max_threads_per_dual_if=0;
}


void CPlatformSocketInfoConfig::Delete(){

}

bool CPlatformSocketInfoConfig::is_sockets_enable(socket_id_t socket){
    assert(socket<MAX_SOCKETS_SUPPORTED);
    return ( m_sockets_enable[socket] );
}

socket_id_t CPlatformSocketInfoConfig::max_num_active_sockets(){
    return  ((socket_id_t)m_sockets_enabled);
}

socket_id_t CPlatformSocketInfoConfig::port_to_socket(port_id_t port){
    return ( m_socket_per_dual_if[(port>>1)]);
}

void CPlatformSocketInfoConfig::set_rx_thread_is_enabled(bool enable){
    m_rx_is_enabled =enable;
}

void CPlatformSocketInfoConfig::set_number_of_dual_ports(uint8_t num_dual_ports){
    m_num_dual_if = num_dual_ports;
}

void CPlatformSocketInfoConfig::set_number_of_threads_per_ports(uint8_t num_threads){
     m_threads_per_dual_if =num_threads;
}

bool CPlatformSocketInfoConfig::sanity_check(){
    return (init());
}

/* return the core mask */
uint64_t CPlatformSocketInfoConfig::get_cores_mask(){
    int i;
    uint64_t mask=0;
    for (i=0; i<MAX_THREADS_SUPPORTED; i++) {
        if ( m_thread_phy_to_virtual[i] ) {

            if (i>=64) {
                printf(" ERROR phy threads can't be higher than 64 \n");
                exit(1);
            }
            mask |=(1LL<<i);
        }
    }

    mask |=(1LL<<m_platform->m_master_thread);
    assert(m_platform->m_master_thread<64);
    if (m_rx_is_enabled) {
        mask |=(1LL<<m_platform->m_rx_thread);
        assert(m_platform->m_rx_thread<64);
    }
    return (mask);
}

virtual_thread_id_t CPlatformSocketInfoConfig::thread_phy_to_virt(physical_thread_id_t  phy_id){
    return (m_thread_phy_to_virtual[phy_id]);
}

physical_thread_id_t CPlatformSocketInfoConfig::thread_virt_to_phy(virtual_thread_id_t virt_id){
    return ( m_thread_virt_to_phy[virt_id]);
}

physical_thread_id_t CPlatformSocketInfoConfig::get_master_phy_id() {
    return m_platform->m_master_thread;
}

bool CPlatformSocketInfoConfig::thread_phy_is_rx(physical_thread_id_t  phy_id){
    return (m_platform->m_rx_thread == phy_id?true:false);
}



bool CPlatformSocketInfo::Create(CPlatformCoresYamlInfo * platform){
    if ( (platform) && (platform->m_is_exists) ) {
        CPlatformSocketInfoConfig * lp=new CPlatformSocketInfoConfig();
        assert(lp);
        lp->Create(platform);
        m_obj= lp;
    }else{
        m_obj= new CPlatformSocketInfoNoConfig();
    }
    return(true);
}

void CPlatformSocketInfo::Delete(){
    if ( m_obj ){
        delete m_obj;
        m_obj=NULL;
    }
}

bool CPlatformSocketInfo::is_sockets_enable(socket_id_t socket){
     return ( m_obj->is_sockets_enable(socket) );
}

socket_id_t CPlatformSocketInfo::max_num_active_sockets(){
    return ( m_obj->max_num_active_sockets() );
}


socket_id_t CPlatformSocketInfo::port_to_socket(port_id_t port){
    return ( m_obj->port_to_socket(port) );
}


void CPlatformSocketInfo::set_rx_thread_is_enabled(bool enable){
    m_obj->set_rx_thread_is_enabled(enable);
}

void CPlatformSocketInfo::set_number_of_dual_ports(uint8_t num_dual_ports){
    m_obj->set_number_of_dual_ports(num_dual_ports);
}

void CPlatformSocketInfo::set_number_of_threads_per_ports(uint8_t num_threads){
    m_obj->set_number_of_threads_per_ports(num_threads);
}

bool CPlatformSocketInfo::sanity_check(){
    return ( m_obj->sanity_check());
}

/* return the core mask */
uint64_t CPlatformSocketInfo::get_cores_mask(){
    return ( m_obj->get_cores_mask());
}

virtual_thread_id_t CPlatformSocketInfo::thread_phy_to_virt(physical_thread_id_t  phy_id){
    return ( m_obj->thread_phy_to_virt(phy_id));
}

physical_thread_id_t CPlatformSocketInfo::thread_virt_to_phy(virtual_thread_id_t virt_id){
    return ( m_obj->thread_virt_to_phy(virt_id));
}

bool CPlatformSocketInfo::thread_phy_is_master(physical_thread_id_t  phy_id){
    return ( m_obj->thread_phy_is_master(phy_id));
}

physical_thread_id_t CPlatformSocketInfo::get_master_phy_id() {
    return ( m_obj->get_master_phy_id());
}

bool CPlatformSocketInfo::thread_phy_is_rx(physical_thread_id_t  phy_id) {
    return ( m_obj->thread_phy_is_rx(phy_id));
}

void CPlatformSocketInfo::dump(FILE *fd){
    m_obj->dump(fd);
}


