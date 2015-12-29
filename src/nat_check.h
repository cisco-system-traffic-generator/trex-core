#ifndef  NAT_CHECK_H
#define  NAT_CHECK_H
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

#include "rx_check_header.h"
#include "msg_manager.h"
#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>



// 2msec timeout                                            
#define MAX_TIME_MSG_IN_QUEUE_SEC  ( 0.002 )

struct CNatFlowInfo {
    uint32_t m_external_ip;
    uint32_t m_external_ip_server;
    uint32_t m_fid;
    uint16_t m_external_port;
    uint16_t m_pad;
};

#if __x86_64__
/* size of 64 bytes */
    #define MAX_NAT_FLOW_INFO (7)
    #define MAX_PKT_MSG_INFO  (26)

#else
    #define MAX_NAT_FLOW_INFO (8)
    #define MAX_PKT_MSG_INFO  (30)
#endif


/* 
     !!!   WARNING  - CGenNodeNatInfo !!

 this struct should be in the same size of CGenNode beacuse allocator is global .

*/

struct CGenNodeNatInfo : public CGenNodeMsgBase  {
    uint8_t       m_pad;
    uint16_t      m_cnt;
    uint32_t      m_pad2;
    CNatFlowInfo  m_data[MAX_NAT_FLOW_INFO];

public:
      CNatFlowInfo * get_next_msg(){
          CNatFlowInfo * lp=&m_data[m_cnt];
          m_cnt++;
          return (lp);
      }

      void init();

      bool is_full(){
          return (m_cnt==MAX_NAT_FLOW_INFO?true:false);
      }
      void dump(FILE *fd);
};

struct CGenNodeLatencyPktInfo : public CGenNodeMsgBase  {
    uint8_t       m_dir;
    uint16_t      m_latency_offset;
    #if __x86_64__
    uint32_t      m_pad3;
    #endif
    struct rte_mbuf *   m_pkt;

    uint32_t      m_pad4[MAX_PKT_MSG_INFO];
};


/* per thread ring info for NAT messages 
   try to put as many messages  */
class CNatPerThreadInfo {
public:
    CNatPerThreadInfo(){
        m_last_time=0;
        m_cur_nat_msg=0;
        m_ring=0;
    }
public:
    dsec_t            m_last_time;
    CGenNodeNatInfo * m_cur_nat_msg;
    CNodeRing       * m_ring;  
};


class CNatStats {
public:
    void reset();
public:
    uint64_t  m_total_rx;
    uint64_t  m_total_msg;
    /* errors */
    uint64_t  m_err_no_valid_thread_id;
    uint64_t  m_err_no_valid_proto;
    uint64_t  m_err_queue_full;
public:
    uint64_t get_errs(){
        return  (m_err_no_valid_thread_id+m_err_no_valid_proto+m_err_queue_full);
    }
    void Dump(FILE *fd);
};


class CNatRxManager {

public:
    bool Create();
    void Delete();
    void handle_packet_ipv4(CNatOption * option,
                       IPHeader * ipv4);
    void handle_aging();
	void Dump(FILE *fd);
    void DumpShort(FILE *fd);
private:
    CNatPerThreadInfo * get_thread_info(uint8_t thread_id);
    void flush_node(CNatPerThreadInfo * thread_info);

private:
    uint8_t               m_max_threads;
    CNatPerThreadInfo   * m_per_thread;
    CNatStats             m_stats;
};


#endif
