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

#include <map>
#include "msg_manager.h"
#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>
#include "os_time.h"
#include "nat_check_flow_table.h"

// 2msec timeout                                            
#define MAX_TIME_MSG_IN_QUEUE_SEC  ( 0.002 )
#define NAT_FLOW_ID_MASK_TCP_ACK 0x00ffffff
#define NAT_FLOW_ID_MASK_IP_ID   0x000001ff

class  CNatOption {
public:
    enum {
        noIPV4_OPTION   = 0x10, /* dummy IPV4 option */
        noOPTION_LEN    = 0x8,
        noIPV4_MAGIC    = 0xEE,
        noIPV4_MAGIC_RX    = 0xED,

        noIPV6_OPTION_LEN    = (noOPTION_LEN/8)-1,
        noIPV6_OPTION = 0x3C, /*IPv6-Opts	Destination Options for IPv6	RFC 2460*/
    };

    void set_option_type(uint8_t id) {
        u.m_data[0 ] =id;
    }

    uint8_t get_option_type() {
        return (u.m_data[0]);
    }

    void set_option_len(uint8_t len) {
        u.m_data[1] = len;
    }
    uint8_t get_option_len(){
        return ( u.m_data[1]);
    }

    void set_thread_id(uint8_t thread_id) {
        u.m_data[3] = thread_id;
    }

    uint8_t get_thread_id() {
        return (u.m_data[3]);
    }

    void set_magic(uint8_t magic){
        u.m_data[2] = magic;
    }

    uint8_t get_magic(){
        return (u.m_data[2]);
    }

    // Used when doing NAT using IP option
    void set_fid(uint32_t fid) {
        u.m_data_uint32[1] = fid & NAT_FLOW_ID_MASK_TCP_ACK;
    }

    uint32_t get_fid() {
        return (u.m_data_uint32[1]);
    }

    bool is_valid_ipv4_magic_op0(void){
        return ( ( PKT_NTOHL( u.m_data_uint32[0] )& 0xFFFFFF00 ) ==
                 (CNatOption::noIPV4_OPTION <<24) +  (CNatOption::noOPTION_LEN<<16) + (CNatOption::noIPV4_MAGIC<<8) ?true:false);
    }

    bool is_valid_ipv4_magic(void) {
        return (is_valid_ipv4_magic_op0());
    }

    bool is_valid_ipv6_magic(void) {
        return ( ( PKT_NTOHL( u.m_data_uint32[0] )& 0x00FFFF00 ) ==
                 (CNatOption::noIPV6_OPTION_LEN<<16) + (CNatOption::noIPV4_MAGIC<<8) ?true:false);

    }

    void set_init_ipv4_header() {
        set_option_type(CNatOption::noIPV4_OPTION);
        set_option_len(CNatOption::noOPTION_LEN);
        set_magic(CNatOption::noIPV4_MAGIC);
    }

    void set_init_ipv6_header(void){
        set_option_len(noIPV6_OPTION_LEN);
        set_magic(CNatOption::noIPV4_MAGIC);
    }

    void dump(FILE *fd);

private:
    union u_ {
        uint8_t  m_data[8];
        uint32_t m_data_uint32[2];
    } u;
};

struct CNatFlowInfo {
    uint32_t m_external_ip;
    uint32_t m_tcp_seq;
    uint32_t m_fid;
    uint16_t m_external_port;
    uint16_t m_pad;
};

/* size of 64 bytes */
    #define MAX_NAT_FLOW_INFO (7)
    #define MAX_PKT_MSG_INFO  (26)

/* 
     !!!   WARNING  - CGenNodeNatInfo !!

 this struct should be in the same size of CGenNode beacuse allocator is global .

*/
struct CGenNodeNatInfo : public CGenNodeMsgBase {
    uint8_t       m_pad;
    uint16_t      m_cnt;
    //uint32_t      m_pad2;
    uint32_t      m_pad3;
    CNatFlowInfo  m_data[MAX_NAT_FLOW_INFO];
#ifdef __PPC64__
#ifdef TREX_SIM
    uint64_t      m_pad4[8];
#else
    uint64_t      m_pad4[16];
#endif
#else
    uint64_t      m_pad4[8];
#endif

public:
      CNatFlowInfo * get_next_msg() {
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

struct CGenNodeLatencyPktInfo : public CGenNodeMsgBase {
    uint8_t       m_dir;
    uint16_t      m_latency_offset;
    
    uint8_t       m_update_ts;
    uint8_t       m_pad3[3];
    
    struct rte_mbuf *m_pkt;

    uint32_t      m_pad4[MAX_PKT_MSG_INFO];
#ifdef __PPC64__
#ifdef TREX_SIM
    uint64_t      m_pad5[8];
#else
    uint64_t      m_pad5[16];
#endif
#else
    uint64_t      m_pad5[8];
#endif
};


/* per thread ring info for NAT messages 
   try to put as many messages  */
class CNatPerThreadInfo {
public:
    CNatPerThreadInfo() {
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
    uint64_t  m_total_rx;
    uint64_t  m_total_msg;
    /* errors */
    uint64_t  m_err_no_valid_thread_id;
    uint64_t  m_err_no_valid_proto;
    uint64_t  m_err_queue_full;
public:
    void reset();
    uint64_t get_errs(){
        return  (m_err_no_valid_thread_id+m_err_no_valid_proto+m_err_queue_full);
    }
    void Dump(FILE *fd);
};

class CNatRxManager {

public:
    bool Create();
    void Delete();
    void handle_packet_ipv4(CNatOption * option, IPHeader * ipv4, bool is_first);
    void handle_aging();
    void Dump(FILE *fd);
    void DumpShort(FILE *fd);
    static inline uint32_t calc_tcp_ack_val(uint32_t fid, uint8_t thread_id) {
        return ((fid &  NAT_FLOW_ID_MASK_TCP_ACK) << 8) | thread_id;
    }
    static inline uint16_t calc_ip_id_val(uint32_t fid, uint8_t thread_id) {
        // MSB bit == 1 signals this is latency packet
        return ((fid &  NAT_FLOW_ID_MASK_IP_ID) << 6) | (thread_id & 0x3f) | 0x8000;
    }
    void get_info_from_tcp_ack(uint32_t tcp_ack, uint32_t &fid, uint8_t &thread_id);
    void get_info_from_ip_id(uint16_t ip_id, uint32_t &fid, uint8_t &thread_id);
private:
    CNatPerThreadInfo * get_thread_info(uint8_t thread_id);
    void flush_node(CNatPerThreadInfo * thread_info);

private:
    uint8_t               m_max_threads;
    CNatPerThreadInfo   * m_per_thread;
    CNatStats             m_stats;
    CNatCheckFlowTable    m_ft;
};


#endif
