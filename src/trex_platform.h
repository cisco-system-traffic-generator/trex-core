#ifndef TREX_PLATFORM_H
#define TREX_PLATFORM_H
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "trex_defs.h"

typedef uint8_t socket_id_t;
typedef uint8_t port_id_t;
/* the real phsical thread id */
typedef uint8_t physical_thread_id_t;


typedef uint8_t virtual_thread_id_t;

class CPlatformCoresYamlInfo;

/*

 virtual thread 0 (v0)- is always the master

for 2 dual ports ( 2x2 =4 ports) the virtual thread looks like that
-----------------
DEFAULT:
-----------------
  (0,1)       (2,3)
  dual-if0       dual-if-1
    v1        v2
    v3        v4
    v5        v6
    v7        v8

    rx is v9

  */



class CPlatformSocketInfoBase {


public:
    /* sockets API */

    /* is socket enabled */
    virtual bool is_sockets_enable(socket_id_t socket)=0;

    /* number of main active sockets. socket #0 is always used  */
    virtual socket_id_t max_num_active_sockets()=0;

    virtual ~CPlatformSocketInfoBase() {}

public:
    /* which socket to allocate memory to each port */
    virtual socket_id_t port_to_socket(port_id_t port)=0;

public:
    /* this is from CLI, number of thread per dual port */
    virtual void set_number_of_threads_per_ports(uint8_t num_threads)=0;
    virtual void set_rx_thread_is_enabled(bool enable)=0;
    virtual void set_number_of_dual_ports(uint8_t num_dual_ports)=0;


    virtual bool sanity_check()=0;

    /* return the core mask */
    virtual uint64_t get_cores_mask()=0;

    /* return the core list */
    virtual void get_cores_list(char *)=0;

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id)=0;

    /* return  the map betwean virtual to phy id */
    virtual physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id)=0;


    virtual physical_thread_id_t get_master_phy_id() = 0;
    virtual bool thread_phy_is_rx(physical_thread_id_t  phy_id)=0;

    virtual void dump(FILE *fd)=0;

    bool thread_phy_is_master(physical_thread_id_t  phy_id) {
        return (get_master_phy_id() == phy_id);
    }

};

class CPlatformSocketInfoNoConfig : public CPlatformSocketInfoBase {

public:
    CPlatformSocketInfoNoConfig(){
        m_dual_if=0;
        m_threads_per_dual_if=0;
        m_rx_is_enabled=false;
    }

    /* is socket enabled */
    bool is_sockets_enable(socket_id_t socket);

    /* number of main active sockets. socket #0 is always used  */
    socket_id_t max_num_active_sockets();

public:
    /* which socket to allocate memory to each port */
    socket_id_t port_to_socket(port_id_t port);

public:
    /* this is from CLI, number of thread per dual port */
    void set_number_of_threads_per_ports(uint8_t num_threads);
    void set_rx_thread_is_enabled(bool enable);
    void set_number_of_dual_ports(uint8_t num_dual_ports);

    bool sanity_check();

    /* return the core mask */
    uint64_t get_cores_mask();

    void get_cores_list(char *);
    uint32_t get_cores_count(void);

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    physical_thread_id_t get_master_phy_id();
    bool thread_phy_is_rx(physical_thread_id_t  phy_id);

    virtual void dump(FILE *fd);

private:
    uint32_t                 m_dual_if;
    uint32_t                 m_threads_per_dual_if;
    bool                     m_rx_is_enabled;
};



/* there is a configuration file */
class CPlatformSocketInfoConfig : public CPlatformSocketInfoBase {
public:
    bool Create(CPlatformCoresYamlInfo * platform);
    void Delete();

        /* is socket enabled */
    bool is_sockets_enable(socket_id_t socket);

    /* number of main active sockets. socket #0 is always used  */
    socket_id_t max_num_active_sockets();

public:
    /* which socket to allocate memory to each port */
    socket_id_t port_to_socket(port_id_t port);

public:
    /* this is from CLI, number of thread per dual port */
    void set_number_of_threads_per_ports(uint8_t num_threads);
    void set_rx_thread_is_enabled(bool enable);
    void set_number_of_dual_ports(uint8_t num_dual_ports);

    bool sanity_check();

    /* return the core mask */
    uint64_t get_cores_mask();

    void get_cores_list(char *);

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    physical_thread_id_t get_master_phy_id();
    bool thread_phy_is_rx(physical_thread_id_t  phy_id);

public:
    virtual void dump(FILE *fd);
private:
    void reset();
    bool init();

private:
    bool                     m_sockets_enable[MAX_SOCKETS_SUPPORTED];
    uint32_t                 m_sockets_enabled;
    socket_id_t              m_socket_per_dual_if[(TREX_MAX_PORTS >> 1)];

    uint32_t                 m_max_threads_per_dual_if;

    uint32_t                 m_num_dual_if;
    uint32_t                 m_threads_per_dual_if;
    bool                     m_rx_is_enabled;
    uint8_t                  m_thread_virt_to_phy[MAX_THREADS_SUPPORTED];
    uint8_t                  m_thread_phy_to_virtual[MAX_THREADS_SUPPORTED];

    CPlatformCoresYamlInfo * m_platform;
};



class CPlatformSocketInfo {

public:
    bool Create(CPlatformCoresYamlInfo * platform);
    void Delete();

public:
    /* sockets API */

    /* is socket enabled */
    bool is_sockets_enable(socket_id_t socket);

    /* number of main active sockets. socket #0 is always used  */
    socket_id_t max_num_active_sockets();

public:
    /* which socket to allocate memory to each port */
    socket_id_t port_to_socket(port_id_t port);

public:
    /* this is from CLI, number of thread per dual port */
    void set_number_of_threads_per_ports(uint8_t num_threads);
    void set_rx_thread_is_enabled(bool enable);
    void set_number_of_dual_ports(uint8_t num_dual_ports);


    bool sanity_check();

    /* return the core mask */
    uint64_t get_cores_mask();

    void get_cores_list(char *);

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    bool thread_phy_is_master(physical_thread_id_t  phy_id);
    physical_thread_id_t get_master_phy_id();
    bool thread_phy_is_rx(physical_thread_id_t  phy_id);

    void dump(FILE *fd);


private:
    CPlatformSocketInfoBase * m_obj;
    CPlatformCoresYamlInfo * m_platform;
};

#endif
