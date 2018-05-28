#ifndef BP_SIM_H
#define BP_SIM_H
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

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include "mbuf.h"
#include <common/c_common.h>
#include <common/captureFile.h>
#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/Network/Packet/IcmpHeader.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>
#include <math.h>
#include <yaml-cpp/yaml.h>
#include "trex_defs.h"
#include "utl_ip.h"
#include "os_time.h"
#include "pal_utl.h"
#include "rx_check_header.h"
#include "rx_check.h"
#include "time_histogram.h"
#include "utl_cpuu.h"
#include "tuple_gen.h"
#include "utl_jitter.h"
#include "msg_manager.h"
#include "nat_check.h"
#include <common/cgen_map.h>
#include <arpa/inet.h>
#include "platform_cfg.h"

#include "trex_watchdog.h"
#include "trex_client_config.h"
#include "h_timer.h"
#include "tw_cfg.h"
#include "utl_dbl_human.h"
#include "utl_policer.h"
#include "trex_platform.h"
#include "trex_global.h"
#include "plat_support.h"


/* stateless includes */
#include "stl/trex_stl_dp_core.h"
#include "stl/trex_stl_fs.h"

class CGenNodePCAP;

#define FORCE_NO_INLINE __attribute__ ((noinline))
#define FORCE_INLINE __attribute__((always_inline))

/* reserve both 0xFF and 0xFE , router will -1 FF */
#define TTL_RESERVE_DUPLICATE 0xff
#define TOS_GO_TO_CPU 0x1
/*
 * Length of string needed to hold the largest port (16-bit) address
 */
#define INET_PORTSTRLEN 5

/* VM commands */

class CMiniVMCmdBase {
public:
    enum MV_FLAGS {
        MIN_VM_V6=1    // IPv6 addressing
    };
    uint8_t   m_cmd;
    uint8_t   m_flags;
    uint16_t  m_start_0;
    uint16_t  m_stop_1;
    uint16_t  m_add_pkt_len; /* request more length for mbuf packet the size */
};

class CMiniVMReplaceIP : public CMiniVMCmdBase {
public:
    ipaddr_t m_server_ip;
};

class CMiniVMReplaceIPWithPort : public CMiniVMReplaceIP {
public:
    uint16_t  m_start_port;
    uint16_t  m_stop_port;
    uint16_t  m_client_port;
    uint16_t  m_server_port;
};

/* this command replace IP in 2 diffrent location and port

c =  10.1.1.2
o =  10.1.1.2
m = audio 102000

==>

c =  xx.xx.xx.xx
o =  xx.xx.xx.xx
m = audio yyyy

*/

class CMiniVMReplaceIP_IP_Port : public CMiniVMCmdBase {
public:
    ipaddr_t m_ip;
    uint16_t  m_ip0_start;
    uint16_t  m_ip0_stop;

    uint16_t  m_ip1_start;
    uint16_t  m_ip1_stop;


    uint16_t  m_port;
    uint16_t  m_port_start;
    uint16_t  m_port_stop;
};

class CMiniVMReplaceIP_PORT_IP_IP_Port : public CMiniVMReplaceIP_IP_Port {
public:
    ipaddr_t m_ip_via;
    uint16_t m_port_via;

    uint16_t  m_ip_via_start;
    uint16_t  m_ip_via_stop;
};

class CMiniVMDynPyload : public CMiniVMCmdBase {
public:
    void * m_ptr;
    ipaddr_t m_ip;
} ;

/* VM with SIMD commands for RTSP we can add SIP/FTP  commands too */

typedef enum { VM_REPLACE_IP_OFFSET =0x12, /* fix ip at offset  */
               VM_REPLACE_IP_PORT_OFFSET, /*  fix ip at offset and client port*/
               VM_REPLACE_IP_PORT_RESPONSE_OFFSET, /* fix client port and server port  */
               VM_REPLACE_IP_IP_PORT,/* SMID command to replace IPV4 , IPV4, PORT in 3 diffrent location , see CMiniVMReplaceIP_IP_Port*/
               VM_REPLACE_IPVIA_IP_IP_PORT,/* SMID command to replace ip,port IPV4 , IPV4, PORT in 3 diffrent location , see CMiniVMReplaceIP_PORT_IP_IP_Port*/
               VM_DYN_PYLOAD,


               VM_EOP /* end of program */
               } mini_vm_op_code_t;


/* work only on x86 littel */
#define	MY_B(b)	(((int)b)&0xff)

class CFlowPktInfo ;

class CMiniVM {

public:
    CMiniVM(){
        m_new_pkt_size=0;
    }

    int mini_vm_run(CMiniVMCmdBase * cmds[]);
    int mini_vm_replace_ip(CMiniVMReplaceIP * cmd);
    int mini_vm_replace_port_ip(CMiniVMReplaceIPWithPort * cmd);
    int mini_vm_replace_ports(CMiniVMReplaceIPWithPort * cmd);
    int mini_vm_replace_ip_ip_ports(CMiniVMReplaceIP_IP_Port * cmd);
    int mini_vm_replace_ip_via_ip_ip_ports(CMiniVMReplaceIP_PORT_IP_IP_Port * cmd);
    int mini_vm_dyn_payload( CMiniVMDynPyload * cmd);


private:
    int append_with_end_of_line(uint16_t len){
        //assert(m_new_pkt_size<=0);
        if (m_new_pkt_size <0 ) {
            memset(m_pyload_mbuf_ptr+len+m_new_pkt_size,0xa,(-m_new_pkt_size));
        }

        return (0);
    }

public:
    int16_t        m_new_pkt_size; /* New packet size after transform by plugin */
    CFlowPktInfo * m_pkt_info;
    char *         m_pyload_mbuf_ptr; /* pointer to the pyload pointer of new allocated packet from mbuf */
};





class CGenNode;
class CFlowYamlInfo;
class CFlowGenListPerThread ;


/* callback */
void on_node_first(uint8_t plugin_id,CGenNode *     node,
                   CFlowYamlInfo *  template_info,
                   CTupleTemplateGeneratorSmart * tuple_gen,
                   CFlowGenListPerThread  * flow_gen
                   );

void on_node_last(uint8_t plugin_id,CGenNode *     node);

rte_mbuf_t * on_node_generate_mbuf(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);

class CPreviewMode ;

class CLatencyPktData {
 public:
    CLatencyPktData() {m_flow_seq = FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ;}
    inline uint32_t get_seq_num() {return m_seq_num;}
    inline void inc_seq_num() {m_seq_num++;}
    inline uint32_t get_flow_seq() {return m_flow_seq;}
    void reset() {
        m_seq_num = UINT32_MAX - 1; // catch wrap around issues early
        m_flow_seq++;
        if (m_flow_seq == FLOW_STAT_PAYLOAD_INITIAL_FLOW_SEQ)
            m_flow_seq++;
    }

 private:
    uint32_t m_seq_num;  // seq num to put in packet for payload rules. Increased every packet.
    uint16_t m_flow_seq;  // Seq num of flow. Changed when we start new flow on this id.
};

/* represent the virtual interface
*/

/* counters per side */
class CVirtualIFPerSideStats {
public:
    CVirtualIFPerSideStats(){
        Clear();
        m_template.Clear();
    }

    uint64_t   m_tx_pkt;
    uint64_t   m_tx_rx_check_pkt;
    uint64_t   m_tx_bytes;
    uint64_t   m_tx_drop;
    uint64_t   m_tx_queue_full;
    uint64_t   m_tx_alloc_error;
    uint64_t   m_tx_redirect_error;
    tx_per_flow_t m_tx_per_flow[MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD];
    CLatencyPktData m_lat_data[MAX_FLOW_STATS_PAYLOAD];
    CPerTxthreadTemplateInfo m_template;

public:

    void Add(CVirtualIFPerSideStats * obj){
        m_tx_pkt     += obj->m_tx_pkt;
        m_tx_rx_check_pkt +=obj->m_tx_rx_check_pkt;
        m_tx_bytes   += obj->m_tx_bytes;
        m_tx_drop    += obj->m_tx_drop;
        m_tx_alloc_error += obj->m_tx_alloc_error;
        m_tx_queue_full +=obj->m_tx_queue_full;
        m_tx_redirect_error +=obj->m_tx_redirect_error;
        m_template.Add(&obj->m_template);
    }

    void Clear(){
       m_tx_pkt=0;
       m_tx_rx_check_pkt=0;
       m_tx_bytes=0;
       m_tx_drop=0;
       m_tx_alloc_error=0;
       m_tx_queue_full=0;
       m_tx_redirect_error=0;
       m_template.Clear();
       for (int i = 0; i < MAX_FLOW_STATS_PAYLOAD; i++) {
           m_lat_data[i].reset();
       }
       for (int i = 0; i < sizeof(m_tx_per_flow) / sizeof(m_tx_per_flow[0]); i++) {
           m_tx_per_flow[i].clear();
       }
    }

    inline void Dump(FILE *fd);
};


void CVirtualIFPerSideStats::Dump(FILE *fd){

    #define DP_B(f) if (f) printf(" %-40s : %lu \n",#f,f)
    DP_B(m_tx_pkt);
    DP_B(m_tx_rx_check_pkt);
    DP_B(m_tx_bytes);
    DP_B(m_tx_drop);
    DP_B(m_tx_alloc_error);
    DP_B(m_tx_queue_full);
    DP_B(m_tx_redirect_error);
    m_template.Dump(fd);
}

class CVirtualIF {
public:
    CVirtualIF () {
        m_preview_mode = NULL;
    }
    virtual ~CVirtualIF() {}
    virtual int open_file(std::string file_name)=0;
    virtual int close_file(void)=0;

      /* read packet from the right queue , per direction, client/server */
    virtual uint16_t rx_burst(pkt_dir_t dir,
                              struct rte_mbuf **rx_pkts,
                              uint16_t nb_pkts){
        assert(0);
    }

    /* send one packet */
    virtual int send_node(CGenNode * node)=0;

    /* by default does the same */
    virtual int send_node_service_mode(CGenNode *node) {
        return send_node(node);
    }

    /* send one packet to a specific dir. flush all packets */
    virtual void send_one_pkt(pkt_dir_t dir, rte_mbuf_t *m) {}
    /* flush all pending packets into the stream */
    virtual int flush_tx_queue(void)=0;
    /* update the source and destination mac-addr of a given mbuf by global database */
    virtual int update_mac_addr_from_global_cfg(pkt_dir_t dir, uint8_t * p)=0;
    /* translate port_id to the correct dir on the core */
    virtual pkt_dir_t port_id_to_dir(uint8_t port_id) {
        return (CS_INVALID);
    }

    virtual void set_review_mode(CPreviewMode *preview_mode) {
        m_preview_mode = preview_mode;
    }

    virtual CVirtualIFPerSideStats * get_stats() {
        return m_stats;
    }
    virtual  bool redirect_to_rx_core(pkt_dir_t   dir,
                                      rte_mbuf_t * m){
        return(false);
    }

    /* set the time for simulation */
    virtual void set_rx_burst_time(double time){
    }


protected:
    CPreviewMode             *m_preview_mode;
    CVirtualIFPerSideStats    m_stats[CS_NUM];

    /* never add public fields to this class - it is being wrapped and delegated */
};


#define MAX_PYLOAD_PKT_CHANGE 4
/* info for the dynamic plugin */


struct CFlowYamlDpPkt {
    CFlowYamlDpPkt(){
        m_pkt_id=0xff;
        m_pyld_offset=0;
        m_type=0;
        m_len=0;
        m_pkt_mask=0xffffffff;
    }

    uint8_t   m_pkt_id; /* number of packet */
    uint8_t   m_pyld_offset; /* 0-10 */
    uint8_t   m_type;  /* 0 -random , 1 - inc */
    uint8_t   m_len;   /* number of 32bit data 1,2,3,*/

    uint32_t  m_pkt_mask; /* 0xffffffff take all the packet */
public:
    void Dump(FILE *fd);
};

struct CFlowYamlDynamicPyloadPlugin {

    CFlowYamlDynamicPyloadPlugin(){
        m_num=0;
        int i;
        for (i=0;i<MAX_PYLOAD_PKT_CHANGE;i++ ) {
            m_pkt_ids[i]=0xff;
        }
    }

    uint8_t         m_num;/* number of pkts_id*/
    uint8_t         m_pkt_ids[MAX_PYLOAD_PKT_CHANGE]; /* -1 for not valid - fast mask */
    CFlowYamlDpPkt  m_program[MAX_PYLOAD_PKT_CHANGE];
public:
    void Add(CFlowYamlDpPkt & fd);
    void Dump(FILE *fd);
};

struct CVlanYamlInfo {
    CVlanYamlInfo(){
        m_enable=0;
        m_vlan_per_port[0]=100;
        m_vlan_per_port[1]=200;
    }
    bool            m_enable;
    uint16_t        m_vlan_per_port[2];

public:
    void Dump(FILE *fd);

};



struct CFlowYamlInfo {
    CFlowYamlInfo(){
        m_dpPkt=0;
        m_server_addr=0;
        m_client_pool_idx = 0;
        m_server_pool_idx = 0;
        m_cap_mode=false;
        m_ipg_sec=0.01;
        m_rtt_sec=0.01;
    }

    std::string     m_name;
    std::string     m_client_pool_name;
    std::string     m_server_pool_name;
    double          m_k_cps;    //k CPS
    double          m_restart_time; /* restart time of this template */
    dsec_t          m_ipg_sec;   // ipg in sec
    dsec_t          m_rtt_sec;   // rtt in sec
    uint32_t        m_w;
    uint32_t        m_limit;
    uint32_t        m_flowcnt;
    pool_index_t    m_client_pool_idx;
    pool_index_t    m_server_pool_idx;
    uint32_t        m_server_addr;
    uint8_t         m_plugin_id; /* 0 - default , 1 - RTSP160 , 2- RTSP250 */
    bool            m_one_app_server;
    bool            m_one_app_server_was_set;
    bool            m_cap_mode;
    bool            m_cap_mode_was_set;
    bool            m_limit_was_set;
    CFlowYamlDynamicPyloadPlugin * m_dpPkt; /* plugin */

public:
    void Dump(FILE *fd);
};



#define DP(f) if (f) printf(" %-40s: %llu \n",#f,(unsigned long long)f)
#define DP_name(n,f) if (f) printf(" %-40s: %llu \n",n,(unsigned long long)f)

#define DP_S(f,f_s) if (f) printf(" %-40s: %s \n",#f,f_s.c_str())

class CFlowPktInfo;


class CCapFileFlowInfo ;

#define SYNC_TIME_OUT ( 1.0/1000)

//#define SYNC_TIME_OUT ( 2000.0/1000)

/* this is a simple struct, do not add constructor and destractor here!
   we are optimizing the allocation dealocation !!!
 */

struct CNodeTcp {
     double       sim_time;
     rte_mbuf_t * mbuf;
     uint8_t      dir;
};


struct CGenNodeBase  {
public:

    enum {
        FLOW_PKT                =0,
        FLOW_FIF                =1,
        FLOW_DEFER_PORT_RELEASE =2,
        FLOW_PKT_NAT            =3,
        FLOW_SYNC               =4,     /* called evey 1 msec */
        STATELESS_PKT           =5,
        EXIT_SCHED              =6,
        COMMAND                 =7,
        EXIT_PORT_SCHED         =8,
        PCAP_PKT                =9,
        GRAT_ARP                =10,
        TW_SYNC                 =11,
        TW_SYNC1                =12,

        TCP_RX_FLUSH            =13, /* TCP rx flush */
        TCP_TX_FIF              =14, /* TCP FIF */
        TCP_TW                  =15,  /* TCP TW -- need to consolidate */

        RX_MSG                  =16  /* message to Rx core */
    };

    /* flags MASKS*/
    enum {
        NODE_FLAGS_DIR                  =1,
        NODE_FLAGS_MBUF_CACHE           =2,
        NODE_FLAGS_SAMPLE_RX_CHECK      =4,

        NODE_FLAGS_LEARN_MODE           =8,   /* bits 3,4 MASK 0x18 wait for second direction packet */
        NODE_FLAGS_LEARN_MSG_PROCESSED  =0x10,   /* got NAT msg */

        NODE_FLAGS_LATENCY              =0x20,   /* got NAT msg */
        NODE_FLAGS_INIT_START_FROM_SERVER_SIDE = 0x40,
        NODE_FLAGS_ALL_FLOW_SAME_PORT_SIDE     = 0x80,
        NODE_FLAGS_INIT_START_FROM_SERVER_SIDE_SERVER_ADDR = 0x100, /* init packet start from server side with server addr */
        NODE_FLAGS_SLOW_PATH = 0x200 /* used by the nodes to differ between fast path nodes and slow path nodes */
    };


public:
    /*********************************************/
    /* C1  must */
    uint8_t             m_type;
    uint8_t             m_thread_id; /* zero base */
    uint8_t             m_socket_id;
    uint8_t             m_pad2;

    uint16_t            m_src_port;
    uint16_t            m_flags; /* BIT 0 - DIR ,
                                    BIT 1 - mbug_cache
                                    BIT 2 - SAMPLE DUPLICATE */

    double              m_time;    /* can't change this header - size 16 bytes*/

public:
    bool operator <(const CGenNodeBase * rsh ) const {
        return (m_time<rsh->m_time);
    }
    bool operator ==(const CGenNodeBase * rsh ) const {
        return (m_time==rsh->m_time);
    }
    bool operator >(const CGenNodeBase * rsh ) const {
        return (m_time>rsh->m_time);
    }

public:
    void set_socket_id(socket_id_t socket){
        m_socket_id=socket;
    }

    socket_id_t get_socket_id(){
        return ( m_socket_id );
    }

    inline void set_slow_path(bool enable) {
        if (enable) {
            m_flags |= NODE_FLAGS_SLOW_PATH;
        } else {
            m_flags &= ~NODE_FLAGS_SLOW_PATH;
        }
    }

    inline bool get_is_slow_path() const {
        return ( (m_flags & NODE_FLAGS_SLOW_PATH) ? true : false);
    }

    void free_base();

    bool is_flow_node(){
        if ((m_type == FLOW_PKT) || (m_type == FLOW_PKT_NAT)) {
            return (true);
        }else{
            return (false);
        }
    }
};


struct CGenNode : public CGenNodeBase  {

public:

    uint32_t        m_src_ip;  /* client ip */
    uint32_t        m_dest_ip; /* server ip */

    uint64_t            m_flow_id; /* id that goes up for each flow */

    /*c2*/
    CFlowPktInfo *      m_pkt_info;

    CCapFileFlowInfo *  m_flow_info;
    CFlowYamlInfo    *  m_template_info;

    void *              m_plugin_info;

/* cache line -2 */
    CHTimerObj           m_tmr;
    uint64_t             m_tmr_pad[4];

/* cache line -3 */


    CTupleGeneratorSmart *m_tuple_gen;
    // cache line 1 - 64bytes waste of space !
    uint32_t            m_nat_external_ipv4; // NAT client IP
    uint32_t            m_nat_tcp_seq_diff_client; // support for firewalls that do TCP seq num randomization
    uint32_t            m_nat_tcp_seq_diff_server; // And some do seq num randomization for server->client also
    uint16_t            m_nat_external_port; // NAT client port
    uint16_t            m_nat_pad[1];
    const ClientCfgBase *m_client_cfg;
    uint32_t            m_src_idx;
    uint32_t            m_dest_idx;
    uint32_t            m_end_of_cache_line[6];


public:
    void free_gen_node();
public:
    void Dump(FILE *fd);



    static void DumpHeader(FILE *fd);
    inline bool is_last_in_flow();
    inline uint16_t get_template_id();
    inline bool is_repeat_flow();
    inline bool can_cache_mbuf(void);

    /* is it possible to cache MBUF */
    inline uint32_t update_next_pkt_in_flow_tw(void);

    /* update the node time for accurate scheduler */
    inline void update_next_pkt_in_flow_as(void);

    inline uint32_t update_next_pkt_in_flow_both(void);


    inline void reset_pkt_in_flow(void);
    inline uint8_t get_plugin_id(void){
        return ( m_template_info->m_plugin_id);
    }

    inline bool is_responder_pkt();
    inline bool is_initiator_pkt();


    inline bool is_eligible_from_server_side(){
        return ( ( (m_src_ip&1) == 1)?true:false);
    }


    inline void set_initiator_start_from_server_side_with_server_addr(bool enable){
           if (enable) {
            m_flags |= NODE_FLAGS_INIT_START_FROM_SERVER_SIDE_SERVER_ADDR;
           }else{
            m_flags &=~ NODE_FLAGS_INIT_START_FROM_SERVER_SIDE_SERVER_ADDR;
           }
    }

    inline bool get_is_initiator_start_from_server_with_server_addr(){
            return (  (m_flags &NODE_FLAGS_INIT_START_FROM_SERVER_SIDE_SERVER_ADDR)?true:false );
    }

    inline void set_initiator_start_from_server(bool enable){
       if (enable) {
        m_flags |= NODE_FLAGS_INIT_START_FROM_SERVER_SIDE;
       }else{
        m_flags &=~ NODE_FLAGS_INIT_START_FROM_SERVER_SIDE;
       }
    }
    inline bool get_is_initiator_start_from_server(){
        return (  (m_flags &NODE_FLAGS_INIT_START_FROM_SERVER_SIDE)?true:false );
    }

    inline void set_all_flow_from_same_dir(bool enable){
        if (enable) {
         m_flags |= NODE_FLAGS_ALL_FLOW_SAME_PORT_SIDE;
        }else{
         m_flags &=~ NODE_FLAGS_ALL_FLOW_SAME_PORT_SIDE;
        }
    }

    inline bool get_is_all_flow_from_same_dir(void){
        return (  (m_flags &NODE_FLAGS_ALL_FLOW_SAME_PORT_SIDE)?true:false );
    }



    /* direction for ip addr */
    inline  pkt_dir_t cur_pkt_ip_addr_dir();
    /* direction for TCP/UDP port */
    inline  pkt_dir_t cur_pkt_port_addr_dir();
    /* from which interface dir to get out */
    inline  pkt_dir_t cur_interface_dir();


    inline void set_mbuf_cache_dir(pkt_dir_t  dir){
        if (dir) {
            m_flags |=NODE_FLAGS_DIR;
        }else{
            m_flags &=~NODE_FLAGS_DIR;
        }
    }

    inline pkt_dir_t get_mbuf_cache_dir(){
        return ((pkt_dir_t)( m_flags &1));
    }

    inline void set_cache_mbuf(rte_mbuf_t * m){
        m_plugin_info=(void *)m;
        m_flags |= NODE_FLAGS_MBUF_CACHE;
    }

    inline rte_mbuf_t * get_cache_mbuf(){
        if ( m_flags &NODE_FLAGS_MBUF_CACHE ) {
            return ((rte_mbuf_t *)m_plugin_info);
        }else{
            return ((rte_mbuf_t *)0);
        }
    }

public:

    inline void set_rx_check(){
        m_flags |= NODE_FLAGS_SAMPLE_RX_CHECK;
    }

    inline bool is_rx_check_enabled(){
        return ((m_flags & NODE_FLAGS_SAMPLE_RX_CHECK)?true:false);
    }

public:

    inline void set_nat_first_state(){
        btSetMaskBit16(m_flags,4,3,1);
        m_type=FLOW_PKT_NAT;
    }

    inline bool is_nat_first_state(){
        return (btGetMaskBit16(m_flags,4,3)==1?true:false) ;
    }

    inline void set_nat_wait_state(){
        btSetMaskBit16(m_flags,4,3,2);
    }


    inline bool is_nat_wait_state(){
        return (btGetMaskBit16(m_flags,4,3)==2?true:false) ;
    }

    // We saw first TCP SYN. Waiting for SYN+ACK
    inline void set_nat_wait_ack_state() {
        btSetMaskBit16(m_flags, 4, 3, 3);
    }

    inline bool is_nat_wait_ack_state(){
        return (btGetMaskBit16(m_flags,4,3) == 3) ? true : false;
    }

    inline void set_nat_learn_state(){
        m_type=FLOW_PKT; /* normal operation .. repeat might work too */
    }

public:
    inline uint32_t get_short_fid(void){
        return (((uint32_t)m_flow_id) & NAT_FLOW_ID_MASK_TCP_ACK);
    }

    inline uint8_t get_thread_id(void){
        return (m_thread_id);
    }

    inline void set_nat_tcp_seq_diff_client(uint32_t diff) {
        m_nat_tcp_seq_diff_client = diff;
    }

    inline uint32_t get_nat_tcp_seq_diff_client() {
        return m_nat_tcp_seq_diff_client;
    }

    inline void set_nat_tcp_seq_diff_server(uint32_t diff) {
        m_nat_tcp_seq_diff_server = diff;
    }

    inline uint32_t get_nat_tcp_seq_diff_server() {
        return m_nat_tcp_seq_diff_server;
    }

    inline void set_nat_ipv4_addr(uint32_t ip){
        m_nat_external_ipv4 =ip;
    }

    inline void set_nat_ipv4_port(uint16_t port){
        m_nat_external_port = port;
    }

    inline uint32_t  get_nat_ipv4_addr(){
        return ( m_nat_external_ipv4 );
    }

    inline uint16_t get_nat_ipv4_port(){
        return ( m_nat_external_port );
    }

    bool is_external_is_eq_to_internal_ip(){
        /* this API is used to check TRex itself */
        if ( (get_nat_ipv4_addr() == m_src_ip ) &&
             (get_nat_ipv4_port()==m_src_port)) {
            return (true);
        }else{
            return (false);
        }
    }

} __rte_cache_aligned;




#define DEFER_CLIENTS_NUM (16)

/* this class must be in the same size of CGenNode */
struct CGenNodeDeferPort  {
    /* this header must be the same as CGenNode */
    uint8_t             m_type;
    uint8_t             m_pad3;
    uint16_t            m_pad2;
    uint32_t            m_cnt;
    double              m_time;

    uint32_t            m_clients[DEFER_CLIENTS_NUM];
    uint16_t            m_ports[DEFER_CLIENTS_NUM];
    pool_index_t        m_pool_idx[DEFER_CLIENTS_NUM];
    uint64_t            m_pad4[6];

public:
    void init(void){
        m_type=CGenNode::FLOW_DEFER_PORT_RELEASE;
        m_cnt=0;
    }

    /* return true if object is full */
    bool add_client(pool_index_t pool_idx, uint32_t client,
                   uint16_t port){
        m_clients[m_cnt]=client;
        m_ports[m_cnt]=port;
        m_pool_idx[m_cnt] = pool_idx;
        m_cnt++;
        if ( m_cnt == DEFER_CLIENTS_NUM ) {
            return (true);
        }
        return (false);
    }

} __rte_cache_aligned ;

/* run time verification of objects size and offsets
   need to clean this up and derive this objects from base object but require too much refactoring right now
   hhaim
*/

#define COMPARE_NODE_OBJECT(NODE_NAME)     if ( sizeof(NODE_NAME) != sizeof(CGenNode)  ) { \
                                            printf("ERROR sizeof(%s) %lu != sizeof(CGenNode) %lu must be the same size \n",#NODE_NAME,sizeof(NODE_NAME),sizeof(CGenNode)); \
                                            assert(0); \
                                            }\
                                            if ( (int)offsetof(struct NODE_NAME,m_type)!=offsetof(struct CGenNodeBase,m_type) ){\
                                            printf("ERROR offsetof(struct %s,m_type)!=offsetof(struct CGenNodeBase,m_type) \n",#NODE_NAME);\
                                            assert(0);\
                                            }\
                                            if ( (int)offsetof(struct CGenNodeDeferPort,m_time)!=offsetof(struct CGenNodeBase,m_time) ){\
                                            printf("ERROR offsetof(struct %s,m_time)!=offsetof(struct CGenNodeBase,m_time) \n",#NODE_NAME);\
                                            assert(0);\
                                            }

#define COMPARE_NODE_OBJECT_SIZE(NODE_NAME)     if ( sizeof(NODE_NAME) != sizeof(CGenNode)  ) { \
                                            printf("ERROR sizeof(%s) %lu != sizeof(CGenNode) %lu must be the same size \n",#NODE_NAME,sizeof(NODE_NAME),sizeof(CGenNode)); \
                                            assert(0); \
                                            }



inline int check_objects_sizes(void){
    COMPARE_NODE_OBJECT(CGenNodeDeferPort);
    return (0);
}


struct CGenNodeCompare
{
   bool operator() (const CGenNode * lhs, const CGenNode * rhs)
   {
       return lhs->m_time > rhs->m_time;
   }
};


class CCapPktRaw;
class CFileWriterBase;



class CFlowGenStats {
public:
    CFlowGenStats(){
        clear();
    }
    // stats
    uint64_t                         m_total_bytes;
    uint64_t                         m_total_pkt;
    uint64_t                         m_total_open_flows;
    uint64_t                         m_total_close_flows;
    uint64_t                         m_nat_lookup_no_flow_id;
    uint64_t                         m_nat_lookup_remove_flow_id;
    uint64_t                         m_nat_lookup_wait_ack_state;
    uint64_t                         m_nat_lookup_add_flow_id;
    uint64_t                         m_nat_flow_timeout;
    uint64_t                         m_nat_flow_timeout_wait_ack;
    uint64_t                         m_nat_flow_learn_error;

public:
    void clear();
    void dump(FILE *fd);
};



typedef std::priority_queue<CGenNode *, std::vector<CGenNode *>,CGenNodeCompare> pqueue_t;



class CErfIF : public CVirtualIF {
    friend class basic_client_cfg_test1_Test;
public:
    CErfIF(){
        m_writer=NULL;
        m_raw=NULL;
    }
public:

    virtual int open_file(std::string file_name);
    virtual int write_pkt(CCapPktRaw *pkt_raw);
    virtual int close_file(void);

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p){
        return (0);
    }



    /**
     * send one packet
     *
     * @param node
     *
     * @return
     */
    virtual int send_node(CGenNode * node);



    /**
     * flush all pending packets into the stream
     *
     * @return
     */
    virtual int flush_tx_queue(void);


protected:
    void add_vlan(uint16_t vlan_id);
    void apply_client_config(const ClientCfgBase *cfg, pkt_dir_t dir);
    virtual void fill_raw_packet(rte_mbuf_t * m,CGenNode * node,pkt_dir_t dir);

    CFileWriterBase         * m_writer;
    CCapPktRaw              * m_raw;
};

/* for stateless we have a small changes in case we send the packets for optimization */
class CErfIFStl : public CErfIF {

public:

    virtual int send_node(CGenNode * node);

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p);

    virtual pkt_dir_t port_id_to_dir(uint8_t port_id);

private:
    int send_sl_node(CGenNodeStateless * node_sl);
    int send_pcap_node(CGenNodePCAP * pcap_node);

};





/**
 * same as regular STL but no I/O (dry run)
 *
 * @author imarom (07-Jan-16)
 */
class CErfIFStlNull : public CErfIFStl {
public:

    virtual int open_file(std::string file_name) {
        return (0);
    }

    virtual int write_pkt(CCapPktRaw *pkt_raw) {
        return (0);
    }

    virtual int close_file(void) {
        return (0);
    }

    virtual void fill_raw_packet(rte_mbuf_t * m,CGenNode * node,pkt_dir_t dir) {

    }



    virtual int flush_tx_queue(void){
        return (0);

    }

};


static inline int fill_pkt(CCapPktRaw  * raw,rte_mbuf_t * m){
    raw->pkt_len = m->pkt_len;
    char *p=raw->raw;

    rte_mbuf_t *m_next;

    while (m != NULL) {
        m_next = m->next;
        rte_memcpy(p,m->buf_addr,m->data_len);
        p+=m->data_len;
        m = m_next;
    }
    return (0);
}


class CNullIF : public CVirtualIF {

public:
    CNullIF(){
    }

public:

    virtual int open_file(std::string file_name){
        return (0);
    }

    virtual int write_pkt(CCapPktRaw *pkt_raw){
        return (0);
    }

    virtual int close_file(void){
        return (0);
    }

    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p){
        return (0);
    }


    virtual int send_node(CGenNode * node);

    virtual int flush_tx_queue(void){
        return (0);

    }
};





class CNodeGenerator {
public:

    friend CFlowGenListPerThread;

     typedef enum { scINIT = 0x17,
                    scWORK ,
                    scWAIT ,
                    scSTRECH,
                    scTERMINATE
                   } sch_state_t;

   typedef enum { smSTATELESS = 0x17,
                  smSTATEFUL  ,
                 } sch_mode_t;

   #define BURST_OFFSET_DTIME    (100.0/1000000)
   #define EAT_WINDOW_DTIME      (15.0/1000000)
   #define WAIT_WINDOW_SIZE      (-1.0/1000000)

    bool  Create(CFlowGenListPerThread  *  parent);
    void  Delete();

    void  set_vif(CVirtualIF * v_if);

    CFlowGenListPerThread  *  Parent(){
        return (m_parent);
    }

public:
    void  add_node(CGenNode * mynode);
    void  remove_all(CFlowGenListPerThread * thread);

    int   open_file(std::string file_name,
                    CPreviewMode * preview);
    int   close_file(CFlowGenListPerThread * thread);
    int   flush_file(dsec_t max_time,
                     dsec_t d_time,
                     bool on_terminate,
                     CFlowGenListPerThread * thread,
                     double & old_offset);
    int   defer_handler(CFlowGenListPerThread * thread);

    void schedule_node(CGenNode * node,double delay){
        node->m_time = (now_sec()+ delay);
        add_node(node);
    }

    /**
     * set packet limit for the generator
     */
    void set_packet_limit(uint64_t limit) {
        m_limit = limit;
    }

    void DumpHist(FILE *fd){
        fprintf(fd,"\n");
        fprintf(fd,"\n");
        fprintf(fd,"normal\n");
        fprintf(fd,"-------------\n");
        m_realtime_his.Dump(fd);
    }

    void dump_json(std::string & json);



private:

    #ifdef _DEBUG
      #define UPDATE_STATS(a) update_stats(a)
    #else
      #define UPDATE_STATS(a)
    #endif

    int   update_stats(CGenNode * node);

    inline int   flush_one_node_to_file(CGenNode * node){
        return (m_v_if->send_node(node));
    }
    int   update_stl_stats(CGenNodeStateless *node_sl);
    bool  has_limit_reached();

    FORCE_NO_INLINE bool handle_slow_messages(uint8_t type,
                                              CGenNode * node,
                                              CFlowGenListPerThread * thread,
                                              bool on_terminate);

private:
        void add_exit_node(CFlowGenListPerThread * thread,
                                          dsec_t max_time);

        inline bool handle_stl_node(CGenNode * node,
                                    CFlowGenListPerThread * thread);


        FORCE_INLINE bool do_work_stl(CGenNode * node,
                                      CFlowGenListPerThread * thread,
                                      bool on_terminate);

        template<bool ON_TERMINATE>
        FORCE_INLINE bool do_work_both(CGenNode * node,
                                      CFlowGenListPerThread * thread,
                                      dsec_t d_time);

        template<int SCH_MODE,bool ON_TERMINATE>
        FORCE_INLINE bool do_work(CGenNode * node,
                                  CFlowGenListPerThread * thread,
                                  dsec_t d_time);

        FORCE_INLINE void do_sleep(dsec_t & cur_time,
                                   CFlowGenListPerThread * thread,
                                   dsec_t ntime);


        FORCE_INLINE int teardown(CFlowGenListPerThread * thread,
                                   bool on_terminate,
                                   double &old_offset,
                                   double offset);

        template<int SCH_MODE,bool ON_TERIMATE>
        int flush_file_realtime(dsec_t max_time,
                                dsec_t d_time,
                                CFlowGenListPerThread * thread,
                                double &old_offset);

        int flush_file_sim(dsec_t max_time,
                            dsec_t d_time,
                            bool always,
                            CFlowGenListPerThread * thread,
                            double &old_offset);

        FORCE_NO_INLINE void handle_slow_operations(sch_state_t &state,
                                                    CGenNode * &node,
                                                    dsec_t &cur_time,
                                                    dsec_t &n_time,
                                                    dsec_t &offset,
                                                    CFlowGenListPerThread *thread);

        void handle_time_strech(CGenNode * &node,
                                dsec_t &cur_time,
                                dsec_t &n_time,
                                dsec_t &offset,
                                CFlowGenListPerThread *thread);


private:
    void handle_command(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler);
    void handle_flow_pkt(CGenNode *node, CFlowGenListPerThread *thread);
    void handle_flow_sync(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler);
    void handle_pcap_pkt(CGenNode *node, CFlowGenListPerThread *thread);
    void handle_maintenance(CFlowGenListPerThread *thread);
    void handle_batch_tw_level1(CGenNode *node, CFlowGenListPerThread *thread,bool &exit_scheduler,bool on_terminate);


public:
    pqueue_t                  m_p_queue;
    socket_id_t               m_socket_id;
    CVirtualIF *              m_v_if;
    CFlowGenListPerThread  *  m_parent;
    CPreviewMode              m_preview_mode;
    uint64_t                  m_cnt;
    uint64_t                  m_non_active;
    uint64_t                  m_limit;
    CTimeHistogram            m_realtime_his;
    dsec_t                    m_scheduler_offset;
    dsec_t                    nanosleep_overhead; // approx

    dsec_t                    m_last_sync_time_sec;
    dsec_t                    m_tw_level1_next_sec;
};



class CFlowKey {
public:
    uint32_t                m_ipaddr1;
    uint32_t                m_ipaddr2;

    uint16_t                m_port1;
    uint16_t                m_port2;

    uint8_t                 m_ip_proto; /* TCP/UDP 6/17*/
    uint8_t                 m_l2_proto; /*IPV4/IPV6*/
    uint16_t                m_vrfid;

public:
    inline bool operator <(const CFlowKey& rhs) const;
    inline bool operator >(const CFlowKey& rhs) const;
    inline bool operator ==(const CFlowKey& rhs) const;
public:
    void Dump(FILE *fd);
    void Clean();
};


inline  bool CFlowKey::operator <(const CFlowKey& rhs) const{
    int cmp=memcmp(&m_ipaddr1,&rhs.m_ipaddr1 ,sizeof(CFlowKey));
    if (cmp>0) {
        return (true);
    }else{
        return (false);
    }
}

inline bool CFlowKey::operator >(const CFlowKey& rhs) const{
    int cmp=memcmp(&m_ipaddr1,&rhs.m_ipaddr1 ,sizeof(CFlowKey));
    if (cmp<0) {
        return (true);
    }else{
        return (false);
    }
}

inline bool CFlowKey::operator ==(const CFlowKey& rhs) const{
    int cmp=memcmp(&m_ipaddr1,&rhs.m_ipaddr1 ,sizeof(CFlowKey));
    if (cmp==0) {
        return (true);
    }else{
        return (false);
    }
}



/***********************************************************/
/* descriptor flags                                        */

#define IS_SWAP_S 0
#define IS_SWAP_E 0

#define IS_VALID_S 1
#define IS_VALID_E 1

#define PROTO_S 3
#define PROTO_E 2

#define IS_INIT_SIDE 4

#define IS_LAST_PKT_S 5
#define IS_LAST_PKT_E 5

#define IS_RTT 6

#define IS_PCAP_TIMING 7

// 8-12 is used
#define FLOW_ID 8


#define PLUGIN_ENABLE_S 13
#define PLUGIN_ENABLE_E 13
#define BOTH_DIR_FLOW_SE 14
#define LEARN_MODE_ENABLE 15

/***********************************************************/

class CPacketDescriptorPerDir {
public:
    CPacketDescriptorPerDir(){
       m_dir_pkt_num=0;
       m_max_dir_flow_pkts=0;
    }
public:
    void SetMaxPkts( uint32_t val){
        assert(val<65000);
        m_max_dir_flow_pkts = (uint16_t)val;
    }
    uint16_t GetMaxPkts(void){
        return (m_max_dir_flow_pkts);
    }

    void SetPktNum(uint32_t pkt_id){
        assert(pkt_id<65000);
        m_dir_pkt_num=(uint16_t)pkt_id;
    }

    uint16_t GetPktNum(void){
        return (m_dir_pkt_num);
    }

private:
    // per direction info
    uint16_t    m_dir_pkt_num;   // pkt id
    uint16_t    m_max_dir_flow_pkts;
};


class CPacketDescriptor {

public:

    inline void Clear(){
        m_flags = 0;
        m_flow_pkt_num=0;
        m_plugin_id=0;
        m_max_flow_pkts=0;
        m_max_flow_aging=0;
    }

    inline uint8_t getPluginId(){
        return (m_plugin_id);
    }
    inline void SetPluginId(uint8_t plugin_id){
        m_plugin_id=plugin_id;
    }

    inline bool IsLearn(){
        return (btGetMaskBit32(m_flags,LEARN_MODE_ENABLE,LEARN_MODE_ENABLE) ? true:false);
    }
    inline void SetLearn(bool enable){
        btSetMaskBit32(m_flags,LEARN_MODE_ENABLE ,LEARN_MODE_ENABLE ,enable?1:0);
    }


    inline bool IsPluginEnable(){
        return (btGetMaskBit32(m_flags,PLUGIN_ENABLE_S,PLUGIN_ENABLE_S) ? true:false);
    }
    inline void SetPluginEnable(bool enable){
        btSetMaskBit32(m_flags,PLUGIN_ENABLE_S ,PLUGIN_ENABLE_S ,enable?1:0);
    }

    inline bool IsBiDirectionalFlow(){
        return (btGetMaskBit32(m_flags,BOTH_DIR_FLOW_SE,BOTH_DIR_FLOW_SE) ? true:false);
    }
    inline void SetBiPluginEnable(bool enable){
        btSetMaskBit32(m_flags,BOTH_DIR_FLOW_SE ,BOTH_DIR_FLOW_SE ,enable?1:0);
    }


    /* packet number inside the global flow */
    inline void SetFlowPktNum(uint32_t pkt_id){
        m_flow_pkt_num = pkt_id;

    }
    /**
     * start from zero 0,1,2,.. , it is on global flow if you have couple of flows it will count all of the flows
     *
     * flow  FlowPktNum
     * 0         0
     * 0         1
     * 0         2
     * 1         0
     * 1         1
     * 2         0
     *
     * @return
     */
    inline uint32_t getFlowPktNum(){
        return ( m_flow_pkt_num);
    }


    inline void SetFlowId(uint16_t flow_id){
        btSetMaskBit32(m_flags,12,8,flow_id);

    }

    inline uint16_t getFlowId(){
        return ( ( uint16_t)btGetMaskBit32(m_flags,12,8));
    }

    inline void SetPcapTiming(bool is_pcap){
        btSetMaskBit32(m_flags,IS_PCAP_TIMING,IS_PCAP_TIMING,is_pcap?1:0);
    }
    inline bool IsPcapTiming(){
        return (btGetMaskBit32(m_flags,IS_PCAP_TIMING,IS_PCAP_TIMING) ? true:false);
    }


    /* return true if this packet in diff direction from prev flow packet ,
    if true need to choose RTT else IPG for inter packet gap */
    inline bool IsRtt(){
        return (btGetMaskBit32(m_flags,IS_RTT,IS_RTT) ? true:false);
    }
    inline void SetRtt(bool is_rtt){
        btSetMaskBit32(m_flags,IS_RTT,IS_RTT,is_rtt?1:0);
    }

    /* this is in respect to the first flow  */
    inline bool IsInitSide(){
        return (btGetMaskBit32(m_flags,IS_INIT_SIDE,IS_INIT_SIDE) ? true:false);
    }

    /* this is in respect to the first flow , this is what is needed when we replace IP source / destiniation */
    inline void SetInitSide(bool is_init_side){
        btSetMaskBit32(m_flags,IS_INIT_SIDE,IS_INIT_SIDE,is_init_side?1:0);
    }

    /* per flow */
    inline bool IsSwapTuple(){
        return (btGetMaskBit32(m_flags,IS_SWAP_S,IS_SWAP_E) ? true:false);
    }
    inline void SetSwapTuple(bool is_swap){
        btSetMaskBit32(m_flags,IS_SWAP_S,IS_SWAP_E,is_swap?1:0);
    }

    inline bool IsValidPkt(){
        return (btGetMaskBit32(m_flags,IS_VALID_S,IS_VALID_E) ? true:false);
    }

    inline void SetIsValidPkt(bool is_valid){
        btSetMaskBit32(m_flags,IS_VALID_S,IS_VALID_E,is_valid?1:0);
    }

    inline void SetIsTcp(bool is_valid){
        btSetMaskBit32(m_flags,PROTO_S,PROTO_E,is_valid?1:0);
    }

    inline bool IsTcp(){
        return ((btGetMaskBit32(m_flags,PROTO_S,PROTO_E) == 1) ? true:false);
    }

    inline void SetIsUdp(bool is_valid){
        btSetMaskBit32(m_flags,PROTO_S,PROTO_E,is_valid?2:0);
    }

    inline bool IsUdp(){
        return ((btGetMaskBit32(m_flags,PROTO_S,PROTO_E) == 2) ? true:false);
    }

    inline void SetIsIcmp(bool is_valid){
        btSetMaskBit32(m_flags,PROTO_S,PROTO_E,is_valid?3:0);
    }

    inline bool IsIcmp(){
        return ((btGetMaskBit32(m_flags,PROTO_S,PROTO_E) == 3) ? true:false);
    }

    inline void SetId(uint16_t _id){
        btSetMaskBit32(m_flags,31,16,_id);

    }
    inline uint16_t getId(){
        return ( ( uint16_t)btGetMaskBit32(m_flags,31,16));
    }

    inline void SetIsLastPkt(bool is_last){
        btSetMaskBit32(m_flags,IS_LAST_PKT_S,IS_LAST_PKT_E,is_last?1:0);
    }

    /* last packet of couple of flows */
    inline bool IsLastPkt(){
        return (btGetMaskBit32(m_flags,IS_LAST_PKT_S,IS_LAST_PKT_E) ? true:false);
    }

    // there could be couple of flows per template in case of plugin
    inline void SetMaxPktsPerFlow(uint32_t pkts){
        assert(pkts<65000);
        m_max_flow_pkts=pkts;
    }
    inline uint16_t GetMaxPktsPerFlow(){
        return ( m_max_flow_pkts );
    }
        // there could be couple of flows per template in case of plugin
    inline void SetMaxFlowTimeout(double sec){
        //assert (sec<65000);
        sec = sec*2.0+5.0;
        if ( sec > 65000) {
            printf("Warning pcap file aging is %f truncating it \n",sec);
            sec = 65000;
        }
        m_max_flow_aging = (uint16_t)sec;
    }

    inline uint16_t GetMaxFlowTimeout(void){
        return ( m_max_flow_aging );
    }

    /* return per dir info , the dir is with respect to the first flow client/server side , this is tricky */
    CPacketDescriptorPerDir * GetDirInfo(void){
        return (&m_per_dir[IsInitSide()?CLIENT_SIDE:SERVER_SIDE]);
    }

    bool IsOneDirectionalFlow(void){
        if ( ( m_per_dir[CLIENT_SIDE].GetMaxPkts() == GetMaxPktsPerFlow()) || ( m_per_dir[SERVER_SIDE].GetMaxPkts() == GetMaxPktsPerFlow()) ) {
            return (true);
        }else{
            return (false);
        }
    }

public:
    void Dump(FILE *fd);

private:
    uint32_t    m_flags;
    uint16_t    m_flow_pkt_num; // packet number inside the flow
    uint8_t     m_plugin_id; // packet number inside the flow
    uint8_t     m_pad;
    uint16_t    m_max_flow_pkts;  // how many packet per this flow getFlowId()
    uint16_t    m_max_flow_aging; // maximum aging in sec
    CPacketDescriptorPerDir  m_per_dir[CS_NUM]; // per direction info
};


class CPacketParser;
class CFlow ;


class CCPacketParserCounters {
public:
    uint64_t  m_pkt;
    uint64_t  m_ipv4;
    uint64_t  m_ipv6;
    uint64_t  m_non_ip;
    uint64_t  m_vlan;
    uint64_t  m_arp;
    uint64_t  m_mpls;


    /* IP stats */
    uint64_t  m_non_valid_ipv4_ver;
    uint64_t  m_non_valid_ipv6_ver;
    uint64_t  m_ip_checksum_error;
    uint64_t  m_ip_length_error;
    uint64_t  m_ipv6_length_error;
    uint64_t  m_ip_not_first_fragment_error;
    uint64_t  m_ip_ttl_is_zero_error;
    uint64_t  m_ip_multicast_error;
    uint64_t  m_ip_header_options;

    /* TCP/UDP */
    uint64_t  m_non_tcp_udp;
    uint64_t  m_non_tcp_udp_ah;
    uint64_t  m_non_tcp_udp_esp;
    uint64_t  m_non_tcp_udp_icmp;
    uint64_t  m_non_tcp_udp_gre;
    uint64_t  m_non_tcp_udp_ip;
    uint64_t  m_tcp_header_options;
    uint64_t  m_tcp_udp_pkt_length_error;
    uint64_t  m_tcp;
    uint64_t  m_udp;
    uint64_t  m_valid_udp_tcp;

public:
    void Clear();
    uint64_t getTotalErrors();
    void Dump(FILE *fd);
};


class CPacketIndication {

public:
    uint32_t            m_ticks;
    CPacketDescriptor   m_desc;
    CCapPktRaw *        m_packet;

    CFlow *          m_flow;
    EthernetHeader * m_ether;
    union {
        IPHeader       * m_ipv4;
        IPv6Header     * m_ipv6;
    } l3;
    bool        m_is_ipv6;
    bool        m_is_ipv6_converted;
    union {
        TCPHeader * m_tcp;
        UDPHeader * m_udp;
        ICMPHeader * m_icmp;
    } l4;
    uint8_t *       m_payload;
    uint16_t        m_payload_len;
    uint16_t        m_packet_padding; /* total packet size - IP total length */

    dsec_t          m_cap_ipg; /* ipg from cap file */

    CFlowKey            m_flow_key;

    uint8_t             m_ether_offset;
    uint8_t             m_ip_offset;
    uint8_t             m_udp_tcp_offset;
    uint8_t             m_payload_offset;
    uint8_t             m_rw_mbuf_size;    /* first R/W mbuf size 64/128/256 */
    uint8_t             m_pad1;
    uint16_t            m_ro_mbuf_size;    /* the size of the const mbuf, zero if does not exits */

public:

    void Dump(FILE *fd,int verbose);
    void Clean();
    bool ConvertPacketToIpv6InPlace(CCapPktRaw * pkt,
                                    int offset);
    void ProcessPacket(CPacketParser *parser,CCapPktRaw * pkt);
    void Clone(CPacketIndication * obj,CCapPktRaw * pkt);
    void RefreshPointers(void);
    void UpdatePacketPadding();
    void UpdateMbufSize();

    void PostProcessIpv6Packet();


public:
    bool is_ipv6(){
        return (m_is_ipv6);
    }
    char * getBasePtr(){
        return ((char *)m_packet->raw);
    }

    uint32_t getEtherOffset(){
        BP_ASSERT(m_ether);
        return (uint32_t)((uintptr_t) (((char *)m_ether)- getBasePtr()) );
    }
    uint32_t getIpOffset(){
        if (l3.m_ipv4 != NULL) {
            return (uint32_t)((uintptr_t)( ((char *)l3.m_ipv4)-getBasePtr()) );
        }else{
            BP_ASSERT(0);
            return (0);
        }
    }


    /**
     * return the application ipv4/ipv6 option offset
     * if learn bit is ON , it is always the first options ( IPV6/IPV4)
     *
     * @return
     */
    uint32_t getIpAppOptionOffset(){
        if ( is_ipv6() ) {
            return  ( getIpOffset()+IPv6Header::DefaultSize);
        }else{
            return  ( getIpOffset()+IPHeader::DefaultSize);
        }
    }

    uint32_t getTcpOffset(){
        BP_ASSERT(l4.m_tcp);
        return (uint32_t)((uintptr_t) ((char *)l4.m_tcp-getBasePtr()) );
    }
    uint32_t getPayloadOffset(){
        if (m_payload) {
            return (uint32_t)((uintptr_t) ((char *)m_payload-getBasePtr()) );
        }else{
            return (0);
        }
    }

    // if is_nat is true, turn on MSB of IP_ID, else turn it off
    void setIpIdNat(bool is_nat) {
        BP_ASSERT(l3.m_ipv4);
        if (! is_ipv6()) {
            if (is_nat) {
                l3.m_ipv4->setId(l3.m_ipv4->getId() | 0x8000);
            } else {
                l3.m_ipv4->setId(l3.m_ipv4->getId() & 0x7fff);
            }
        }
    }

    void  setTOSReserve(){
        BP_ASSERT(l3.m_ipv4);
        if (is_ipv6()) {
            l3.m_ipv6->setTrafficClass(l3.m_ipv6->getTrafficClass() | TOS_GO_TO_CPU );
        }else{
            l3.m_ipv4->setTOS(l3.m_ipv4->getTOS()| TOS_GO_TO_CPU );
        }
    }

    void  clearTOSReserve(){
        BP_ASSERT(l3.m_ipv4);
        if (is_ipv6()) {
            l3.m_ipv6->setTrafficClass(l3.m_ipv6->getTrafficClass()& (~TOS_GO_TO_CPU) );
        }else{
            l3.m_ipv4->setTOS(l3.m_ipv4->getTOS() & (~TOS_GO_TO_CPU) );
        }
    }

    uint8_t getTTL(){
        BP_ASSERT(l3.m_ipv4);
        if (is_ipv6()) {
            return(l3.m_ipv6->getHopLimit());
        }else{
            return(l3.m_ipv4->getTimeToLive());
        }
    }
    void setTTL(uint8_t ttl){
        BP_ASSERT(l3.m_ipv4);
        if (is_ipv6()) {
            l3.m_ipv6->setHopLimit(ttl);
        }else{
            l3.m_ipv4->setTimeToLive(ttl);
            l3.m_ipv4->updateCheckSum();
        }
    }

    uint8_t getIpProto(){
        BP_ASSERT(l3.m_ipv4);
        if (is_ipv6()) {
            return(l3.m_ipv6->getNextHdr());
        }else{
            return(l3.m_ipv4->getProtocol());
        }
    }

    uint8_t getFastEtherOffset(void){
        return (m_ether_offset);
    }
    uint8_t getFastIpOffsetFast(void){
        return (m_ip_offset);
    }
    uint8_t getFastTcpOffset(void){
        return (m_udp_tcp_offset );
    }
    uint8_t getFastPayloadOffset(void){
        return (m_payload_offset );
    }

    /* first r/w mbuf size could be 64 /128 */
    uint8_t get_rw_mbuf_size(){
        return (m_rw_mbuf_size);
    }

    /* zero mean there is no mbuf */
    uint16_t get_cons_mbuf_size(){
        return (m_ro_mbuf_size);
    }

    /* this is the size of the packet to append, could be smaller than rw_mbuf */
    uint8_t get_rw_pkt_size(){
        return (m_packet->pkt_len > m_rw_mbuf_size) ? m_rw_mbuf_size : m_packet->pkt_len;
    }

private:
    void SetKey(void);
    uint8_t ProcessIpPacketProtocol(CCPacketParserCounters *m_cnt,
                                    uint8_t protocol, int *offset);
    void ProcessIpPacket(CPacketParser *parser,int offset);
    void ProcessIpv6Packet(CPacketParser *parser,int offset);


    void _ProcessPacket(CPacketParser *parser,CCapPktRaw * pkt);

    void UpdateOffsets();


};



#define SRC_IP_BASE 0x10000001
#define DST_IP_BASE 0x20000001

class CFlowTemplateGenerator {
public:
    CFlowTemplateGenerator(uint64_t fid){
        src_ip_base=((SRC_IP_BASE + (uint32_t)fid )& 0x7fffffff);
        dst_ip_base=((DST_IP_BASE + (uint32_t) ((fid & 0xffffffff00000000ULL)>>32)) & 0x7fffffff);
    }
public:
    uint32_t src_ip_base;
    uint32_t dst_ip_base;
};


class CPacketParser {

public:
    bool Create();
    void Delete();
    bool ProcessPacket(CPacketIndication * pkt_indication,
                       CCapPktRaw * raw_packet);
public:
    CCPacketParserCounters m_counter;
public:
    void Dump(FILE *fd);
};


class CFlowTableStats {
public:
    uint64_t  m_lookup;
    uint64_t  m_found;
    uint64_t  m_fif;
    uint64_t  m_add;
    uint64_t  m_remove;
    uint64_t  m_fif_err;
    uint64_t  m_active;
public:
    void Clear();
    void Dump(FILE *fd);
};



class CFlow {
public:
    CFlow(){
        is_fif_swap=0;
        pkt_id=0;
    }
    ~CFlow(){
       }
public:
    void Dump(FILE *fd);
public:
    uint8_t   is_fif_swap;
    uint32_t  pkt_id;
    uint32_t  flow_id;
};

class CFlowTableInterator {
public:
    virtual void do_flow(CFlow *flow)=0;
};

class CFlowTableManagerBase {
public:
    virtual bool Create(int max_size)=0;
    virtual void Delete()=0;
public:
    CFlow * process(const CFlowKey & key,bool &is_fif  );
    virtual void remove(const CFlowKey & key )=0;
    virtual void remove_all()=0;
    virtual uint64_t count()=0;
public:
    void Dump(FILE *fd);
protected:
    virtual CFlow * lookup(const CFlowKey & key )=0;
    virtual CFlow * add(const CFlowKey & key )=0;

    //virtual IterateFlows(CFlowTableInterator * iter)=0;
protected:
    CFlowTableStats  m_stats;
};



typedef CFlow * flow_ptr;
typedef std::map<CFlowKey, flow_ptr, std::less<CFlowKey> > flow_map_t;
typedef flow_map_t::iterator flow_map_iter_t;


class CFlowTableMap  : public CFlowTableManagerBase {
public:
    virtual bool Create(int max_size);
    virtual void Delete();
    virtual void remove(const CFlowKey & key );

protected:
    virtual CFlow * lookup(const CFlowKey & key );
    virtual CFlow * add(const CFlowKey & key );
    virtual void remove_all(void);
    uint64_t count(void);
private:
    flow_map_t m_map;
};

class CFlowInfo {
public:
    uint32_t client_ip;
    uint32_t server_ip;
    uint32_t client_port;
    uint32_t server_port;
    bool     is_init_ip_dir;
    bool     is_init_port_dir;

    bool     replace_server_port;
    CMiniVMCmdBase ** vm_program;/* pointer to vm program */
};

class CFlowPktInfo {
public:
    bool Create(CPacketIndication  * pkt_ind);
    void Delete();
    void Dump(FILE *fd);

    /* generate a new packet */
    inline rte_mbuf_t * generate_new_mbuf(CGenNode * node);
    inline rte_mbuf_t * do_generate_new_mbuf(CGenNode * node);
    inline rte_mbuf_t * do_generate_new_mbuf_big(CGenNode * node);

    /* new packet with rx check info in IP option */
    void do_generate_new_mbuf_rxcheck(rte_mbuf_t * m,
                                 CGenNode * node,
                                 bool single_port);

    inline rte_mbuf_t * do_generate_new_mbuf_ex(CGenNode * node,CFlowInfo * flow_info);
    inline rte_mbuf_t * do_generate_new_mbuf_ex_big(CGenNode * node,CFlowInfo * flow_info);
    inline rte_mbuf_t * do_generate_new_mbuf_ex_vm(CGenNode * node,
                                    CFlowInfo * flow_info, int16_t * s_size);

public:
    /* push the number of bytes into the packets and make more room
      should be used by NAT feature that should have ipv4 option in the first packet
      this function should not be called in runtime, only when template is loaded due to it heavey cost of operation ( malloc/free memory )
    */
    char * push_ipv4_option_offline(uint8_t bytes);
    char * push_ipv6_option_offline(uint8_t bytes);



    /**
     * mark this packet as learn packet
     * should
     * 1. push ipv4 option ( 8 bytes)
     * 2. mark the packet as learn
     * 3. update the option pointer
     */
    void   mark_as_learn();

private:
    inline void append_big_mbuf(rte_mbuf_t * m,
                                              CGenNode * node);

    inline void update_pkt_info(char *p,
                                CGenNode * node);

    inline void update_pkt_info2(char *p,
                                 CFlowInfo * flow_info,
                                 int update_len,
                                 CGenNode * node
                                 );

    inline void update_tcp_cs(TCPHeader * tcp,
                              IPHeader  * ipv4);

    inline void update_udp_cs(UDPHeader * udp,
                              IPHeader  * ipv4);

    inline void update_mbuf(rte_mbuf_t * m);

    void alloc_const_mbuf();

    void free_const_mbuf();

    rte_mbuf_t    *  get_big_mbuf(socket_id_t socket_id){
        return (m_big_mbuf[socket_id]);
    }


public:
    CPacketIndication   m_pkt_indication;
    CCapPktRaw        * m_packet;
    rte_mbuf_t        * m_big_mbuf[MAX_SOCKETS_SUPPORTED]; /* allocate big mbug per socket */
};


inline void CFlowPktInfo::update_pkt_info2(char *p,
                                           CFlowInfo * flow_info,
                                           int update_len ,
                                           CGenNode * node
                                           ){
    IPHeader       * ipv4=
        (IPHeader       *)(p + m_pkt_indication.getFastIpOffsetFast());

    EthernetHeader * et  =
        (EthernetHeader * )(p + m_pkt_indication.getFastEtherOffset());

    (void)et;

    if ( unlikely (m_pkt_indication.is_ipv6())) {
        IPv6Header *ipv6= (IPv6Header *)ipv4;

        if ( update_len ){
            ipv6->setPayloadLen(ipv6->getPayloadLen() + update_len);
        }

        if ( flow_info->is_init_ip_dir  ) {
            ipv6->updateLSBIpv6Src(flow_info->client_ip);
            ipv6->updateLSBIpv6Dst(flow_info->server_ip);
        }else{
            ipv6->updateLSBIpv6Src(flow_info->server_ip);
            ipv6->updateLSBIpv6Dst(flow_info->client_ip);
        }

    }else{
        if ( update_len ){
            ipv4->setTotalLength((ipv4->getTotalLength() + update_len));
        }

        if ( flow_info->is_init_ip_dir  ) {
            ipv4->setSourceIp(flow_info->client_ip);
            ipv4->setDestIp(flow_info->server_ip);
        }else{
            ipv4->setSourceIp(flow_info->server_ip);
            ipv4->setDestIp(flow_info->client_ip);
        }

        if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
            ipv4->myChecksum = 0;
        } else {
            ipv4->updateCheckSumFast();
        }
    }


    /* replace port base on TCP/UDP */
    if ( m_pkt_indication.m_desc.IsTcp() ) {
        TCPHeader * tcp = (TCPHeader *)(p +m_pkt_indication.getFastTcpOffset());
        BP_ASSERT(tcp);
        /* replace port */
        if ( flow_info->is_init_port_dir  ) {
            tcp->setSourcePort(flow_info->client_port);
            if ( flow_info->replace_server_port ){
                tcp->setDestPort(flow_info->server_port);
            }
        }else{
            tcp->setDestPort(flow_info->client_port);
            if ( flow_info->replace_server_port ){
                tcp->setSourcePort(flow_info->server_port);
            }
        }
        update_tcp_cs(tcp,ipv4);

    }else {
        if ( m_pkt_indication.m_desc.IsUdp() ){
            UDPHeader * udp =(UDPHeader *)(p +m_pkt_indication.getFastTcpOffset() );
            BP_ASSERT(udp);
            udp->setLength(udp->getLength() + update_len);
            if ( flow_info->is_init_port_dir  ) {
                udp->setSourcePort(flow_info->client_port);
                if ( flow_info->replace_server_port ){
                    udp->setDestPort(flow_info->server_port);
                }
            }else{
                udp->setDestPort(flow_info->client_port);
                if ( flow_info->replace_server_port ){
                    udp->setSourcePort(flow_info->server_port);
                }
            }
            update_udp_cs(udp,ipv4);

        }else{
            BP_ASSERT(0);
        }
    }
}



inline void CFlowPktInfo::update_mbuf(rte_mbuf_t * m){

    m->l2_len = m_pkt_indication.getFastIpOffsetFast();
    uint8_t l4_offset = m_pkt_indication.getFastTcpOffset();
    BP_ASSERT(l4_offset > m->l2_len);
    m->l3_len = l4_offset - m->l2_len ;

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {

        if ( unlikely (m_pkt_indication.is_ipv6() ) ) {
            m->ol_flags |= PKT_TX_IPV6 ;
        }else{
            /* Ipv4*/
            m->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
        }

        if ( m_pkt_indication.m_desc.IsTcp() ) {
            m->ol_flags |=   PKT_TX_TCP_CKSUM;
        } else {
            if (m_pkt_indication.m_desc.IsUdp()) {
                m->ol_flags |= PKT_TX_UDP_CKSUM;
            }
        }
    }
}


inline void CFlowPktInfo::update_tcp_cs(TCPHeader * tcp,
                                        IPHeader  * ipv4){

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
        /* set pseudo-header checksum */
        if ( m_pkt_indication.is_ipv6() ){
            tcp->setChecksumRaw(rte_ipv6_phdr_cksum((struct ipv6_hdr *)ipv4->getPointer(), PKT_TX_IPV6 |PKT_TX_TCP_CKSUM));
        }else{
            tcp->setChecksumRaw(rte_ipv4_phdr_cksum((struct ipv4_hdr *)ipv4->getPointer(),
                                                             PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM));
        }
    }
}

inline void CFlowPktInfo::update_udp_cs(UDPHeader * udp,
                                        IPHeader  * ipv4){

    if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
        /* set pseudo-header checksum */
        if ( m_pkt_indication.is_ipv6() ){
            udp->setChecksumRaw(rte_ipv6_phdr_cksum((struct ipv6_hdr *) ipv4->getPointer(),
                                                         PKT_TX_IPV6 | PKT_TX_UDP_CKSUM));
        }else{
            udp->setChecksumRaw(rte_ipv4_phdr_cksum((struct ipv4_hdr *) ipv4->getPointer(),
                                                         PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM));
        }
    } else {
        udp->setChecksum(0);
    }
}


inline void CFlowPktInfo::update_pkt_info(char *p,
                                          CGenNode * node){

    uint8_t ip_offset = m_pkt_indication.getFastIpOffsetFast();
    IPHeader       * ipv4=
        (IPHeader       *)(p + ip_offset );

    uint16_t src_port =   node->m_src_port;
    uint32_t tcp_seq_diff_client = 0;
    uint32_t tcp_seq_diff_server = 0;

    pkt_dir_t ip_dir = node->cur_pkt_ip_addr_dir();
    pkt_dir_t port_dir = node->cur_pkt_port_addr_dir();


    if ( unlikely (m_pkt_indication.is_ipv6())) {

        // Update the IPv6 address
        IPv6Header *ipv6= (IPv6Header *)ipv4;

        if ( ip_dir ==  CLIENT_SIDE  ) {
            ipv6->updateLSBIpv6Src(node->m_src_ip);
            ipv6->updateLSBIpv6Dst(node->m_dest_ip);
        }else{
            ipv6->updateLSBIpv6Src(node->m_dest_ip);
            ipv6->updateLSBIpv6Dst(node->m_src_ip);
        }
    }else{

        if ( unlikely ( CGlobalInfo::is_learn_mode()  ) ){
            if (m_pkt_indication.m_desc.IsLearn()) {
                /* might be done twice */
#ifdef NAT_TRACE_
                printf(" %.3f : DP :  learn packet !\n",now_sec());
#endif
                /* first ipv4 option add the info in case of learn packet, usualy only the first packet */
                if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION)) {
                    CNatOption *lpNat =(CNatOption *)ipv4->getOption();
                    lpNat->set_fid(node->get_short_fid());
                    lpNat->set_thread_id(node->get_thread_id());
                } else {
                    if (ipv4->getProtocol() == IPPROTO_TCP) {
                        TCPHeader *tcp = (TCPHeader *)(((uint8_t *)ipv4) + ipv4->getHeaderLength());
                        // Put NAT info in first TCP SYN
                        if (tcp->getSynFlag()) {
                            tcp->setAckNumber(CNatRxManager::calc_tcp_ack_val(node->get_short_fid(), node->get_thread_id()));
                        }
#ifdef NAT_TRACE_
                        printf(" %.3f : flow_id: %x thread_id %x TCP ack %x seq %x\n"
                               ,now_sec(), node->get_short_fid(), node->get_thread_id(), tcp->getAckNumber()
                               , tcp->getSeqNumber());
#endif
                    } else {
                        // If protocol is not TCP, put NAT info in IP_ID
                        ipv4->setId(CNatRxManager::calc_ip_id_val(node->get_short_fid(), node->get_thread_id()));
                    }
                }
            }
            /* in all cases update the ip using the outside ip */

            if ( m_pkt_indication.m_desc.IsInitSide()  ) {
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : DP : i %x:%x -> %x  flow_id: %lx\n",now_sec(), node->m_src_ip
                           , node->m_src_port, node->m_dest_ip, node->m_flow_id);
                }
#endif

                tcp_seq_diff_server = node->get_nat_tcp_seq_diff_server();
                ipv4->updateIpSrc(node->m_src_ip);
                ipv4->updateIpDst(node->m_dest_ip);
            } else {
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : r %x   -> %x:%x  flow_id: %lx \n", now_sec(), node->m_dest_ip
                           , node->m_src_ip, node->m_src_port, node->m_flow_id);
                }
#endif
                src_port = node->get_nat_ipv4_port();
                tcp_seq_diff_client = node->get_nat_tcp_seq_diff_client();
                ipv4->updateIpSrc(node->m_dest_ip);
                ipv4->updateIpDst(node->get_nat_ipv4_addr());
            }

#ifdef NAT_TRACE_
            if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                if ( m_pkt_indication.m_desc.IsInitSide() ==false ){
                    printf(" %.3f : pkt ==> %x %x:%x \n",now_sec(),node->get_nat_ipv4_addr(),
                           node->get_nat_ipv4_port(),node->m_src_port);
                }else{
                    printf(" %.3f : pkt ==> init pkt sent \n",now_sec());
                }
            }
#endif


        }else{
            if ( ip_dir ==  CLIENT_SIDE  ) {
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : i %x:%x -> %x \n",now_sec(),node->m_src_ip,node->m_src_port,node->m_dest_ip);
                }
#endif
                ipv4->updateIpSrc(node->m_src_ip);
                ipv4->updateIpDst(node->m_dest_ip);
            }else{
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : r %x   -> %x:%x  \n",now_sec(),node->m_dest_ip,node->m_src_ip,node->m_src_port);
                }
#endif
                ipv4->updateIpSrc(node->m_dest_ip);
                ipv4->updateIpDst(node->m_src_ip);
            }
        }

        if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
            ipv4->myChecksum = 0;
        } else {
            ipv4->updateCheckSumFast();
        }
    }


    /* replace port base on TCP/UDP */
    if ( m_pkt_indication.m_desc.IsTcp() ) {
        TCPHeader * tcp = (TCPHeader *)(p +m_pkt_indication.getFastTcpOffset());
        BP_ASSERT(tcp);
        /* replace port */
        if ( port_dir ==  CLIENT_SIDE ) {
            tcp->setSourcePort(src_port);
            tcp->setAckNumber(tcp->getAckNumber() + tcp_seq_diff_server);
        }else{
            tcp->setDestPort(src_port);
            tcp->setAckNumber(tcp->getAckNumber() + tcp_seq_diff_client);
        }
        update_tcp_cs(tcp,ipv4);
    }else {
        if ( m_pkt_indication.m_desc.IsUdp() ){
            UDPHeader * udp =(UDPHeader *)(p +m_pkt_indication.getFastTcpOffset() );
            BP_ASSERT(udp);

            if ( port_dir ==  CLIENT_SIDE ) {
                udp->setSourcePort(src_port);
            }else{
                udp->setDestPort(src_port);
            }
            update_udp_cs(udp,ipv4);
        }else{
#ifdef _DEBUG
            if (!m_pkt_indication.m_desc.IsIcmp()) {
               BP_ASSERT(0);
            }
#endif
        }
    }
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_ex(CGenNode * node,
                                                          CFlowInfo * flow_info){
    rte_mbuf_t        * m;
    /* alloc small packet buffer*/
    uint16_t len= m_pkt_indication.get_rw_pkt_size();
    m =  CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(),len);
    assert(m);
    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_mbuf(m);

    update_pkt_info2(p,flow_info,0,node);

    append_big_mbuf(m,node);

    return(m);
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_ex_big(CGenNode * node,
                                                          CFlowInfo * flow_info){
    rte_mbuf_t        * m;
    uint16_t len =  m_packet->pkt_len;

    /* alloc big buffer to update it*/
    m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(), len);
    assert(m);

    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_mbuf(m);

    update_pkt_info2(p,flow_info,0,node);

    return(m);
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_ex_vm(CGenNode * node,
                                              CFlowInfo * flow_info, int16_t * s_size){
    rte_mbuf_t        * m;

    /* sanity check we need to have payload */
    if ( unlikely( m_pkt_indication.m_payload_len == 0) ){
        printf(" ERROR nothing to do \n");
        return (do_generate_new_mbuf_ex(node,flow_info));
    }

    CMiniVMCmdBase ** cmds=flow_info->vm_program;
    BP_ASSERT(cmds);

        /* packet is going to be changed update len with what we expect ( written in first command ) */
    uint16_t len =  m_packet->pkt_len + cmds[0]->m_add_pkt_len;

    /* alloc big buffer to update it*/
    m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(), len);
    assert(m);

    /* append the additional bytes requested and update later */
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    /* copy the headers until the payload  */
    memcpy(p, m_packet->raw, m_pkt_indication.getPayloadOffset() );
    CMiniVM vm;
    vm.m_pkt_info = this;
    vm.m_pyload_mbuf_ptr = p+m_pkt_indication.getPayloadOffset();
    vm.mini_vm_run(cmds);

    /* need to update the mbuf size here .., this is not must but needed for accuracy  */
    uint16_t buf_adjust = len - vm.m_new_pkt_size;
    int rc = rte_pktmbuf_trim(m, buf_adjust);
    (void)rc;

    /* update IP length , and TCP checksum , we can accelerate this using hardware ! */
    int16_t pkt_adjust = vm.m_new_pkt_size - m_packet->pkt_len;

    update_mbuf(m);

    update_pkt_info2(p,flow_info,pkt_adjust,node);

    /* return change in packet size due to packet tranforms */
    *s_size = vm.m_new_pkt_size - m_packet->pkt_len;

    //printf(" new length : actual %d , update:%d \n",m_packet->pkt_len,m_packet->pkt_len + vm.m_new_pkt_size);
    return(m);
}


inline void CFlowPktInfo::append_big_mbuf(rte_mbuf_t * m,
                                          CGenNode * node){

    rte_mbuf_t * mbig= get_big_mbuf(node->get_socket_id());

    if (  mbig == NULL) {
        return ;
    }

    utl_rte_pktmbuf_add_after(m,mbig);
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf(CGenNode * node){
    rte_mbuf_t        * m;
    /* alloc small packet buffer*/
    uint16_t len= m_pkt_indication.get_rw_pkt_size();
    m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(),  len);
    assert(m);
    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_mbuf(m);

    update_pkt_info(p,node);

    append_big_mbuf(m,node);

    return m;
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_big(CGenNode * node){
    rte_mbuf_t        * m;
    uint16_t len =  m_packet->pkt_len;

    /* alloc big buffer to update it*/
    m = CGlobalInfo::pktmbuf_alloc_local(node->get_socket_id(),  len);
    assert(m);

    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_mbuf(m);

    update_pkt_info(p,node);

    return(m);
}


inline rte_mbuf_t * CFlowPktInfo::generate_new_mbuf(CGenNode * node){

    if ( m_pkt_indication.m_desc.IsPluginEnable() ) {
        return ( on_node_generate_mbuf( node->get_plugin_id(),node,this) );
    }
    return  (do_generate_new_mbuf(node));
}



typedef CFlowPktInfo * flow_pkt_info_t;

class CCCapFileMemoryUsage {

public:

  enum { SIZE_MIN = 64,
         SIZE_64  = 64,
        SIZE_128  = 128,
        SIZE_256   = 256,
        SIZE_512   = 512,
        SIZE_1024  = 1024,
        SIZE_2048  = 2048,
        SIZE_4096  = 4096,
        SIZE_8192  = 8192,
        SIZE_16384  = 16384,

        MASK_SIZE =9
      };

  void clear(){
      int i;
      for (i=0; i<CCCapFileMemoryUsage::MASK_SIZE; i++) {
          m_buf[i] = 0;
      }
      m_total_bytes=0;
  }

  void add_size(uint32_t size){
      m_total_bytes+=size;
      int c_size=CCCapFileMemoryUsage::SIZE_MIN;
      int i;
      for (i=0; i<CCCapFileMemoryUsage::MASK_SIZE; i++) {
          if (size<c_size) {
              m_buf[i]+=1;
              return;
          }
          c_size = c_size*2;
      }
      printf("ERROR pkt size bigger than %d is not supported !\n",CCCapFileMemoryUsage::SIZE_2048);
      exit(1);
  }
  void dump(FILE *fd);

  void Add(const CCCapFileMemoryUsage & obj);

public:
    uint32_t m_buf[MASK_SIZE];
    uint64_t m_total_bytes;
};



class CCapFileFlowInfo {
public:
    const int LEARN_MODE_MIN_IPG = 10; // msec

    enum load_cap_file_err {
    kOK = 0,
    kFileNotExist,
    kNegTimestamp,
    kNoSyn,
    kTCPOffsetTooBig,
    kNoTCPFromServer,
    kNoTCPSynAck,
    kTCPLearnModeBadFlow,
    kPktNotSupp,
    kPktProcessFail,
    kEmptyFile,
    kCapFileErr,
    kPlugInWithLearn,
    kIPOptionNotAllowed,
    kTCPIpgTooLow
    };

    bool Create();
    void Delete();
    void clear(void);
    uint64_t Size(void){
        return (m_flow_pkts.size());
    }
    inline CFlowPktInfo * GetPacket(uint32_t index);
    void Append(CPacketIndication * pkt_indication);
    void RemoveAll();
    void dump_pkt_sizes(void);
    enum load_cap_file_err load_cap_file(std::string cap_file, uint16_t _id, uint8_t plugin_id);

    /* update flow info */
    void update_info(CFlowYamlInfo *  info);

    enum CCapFileFlowInfo::load_cap_file_err is_valid_template_load_time();

    void save_to_erf(std::string cap_file_name,int pcap);

    void generate_flow(CTupleTemplateGeneratorSmart   * tuple_gen,
                              CNodeGenerator * gen,
                              dsec_t time,
                              uint64_t flow_id,
                              CFlowYamlInfo *  template_info,
                              CGenNode *     node);

    inline uint64_t get_total_bytes(){
        return (m_total_bytes);
    }
    inline uint64_t get_total_flows(){
        return (m_total_flows);
    }

    inline uint64_t get_total_errors(){
        return (m_total_errors);
    }

    // return the cap file length in sec
    double get_cap_file_length_sec();

    void get_total_memory(CCCapFileMemoryUsage & memory);

public:
    void update_min_ipg(dsec_t min_ipg, dsec_t override_ipg);
    void update_ipg_by_factor(double factor,CFlowYamlInfo *  flow_info);
    void update_pcap_mode();
    void Dump(FILE *fd);

private:
    std::vector<flow_pkt_info_t> m_flow_pkts;
    uint64_t                     m_total_bytes;
    uint64_t                     m_total_flows;
    uint64_t                     m_total_errors;
};



inline CFlowPktInfo * CCapFileFlowInfo::GetPacket(uint32_t index){
    BP_ASSERT(index<m_flow_pkts.size());
    return (m_flow_pkts[index]);
}




struct CFlowsYamlInfo {
public:
    double          m_duration_sec;    //duration in sec for the cap file
// IPv6 addressing
    std::vector     <uint16_t> m_src_ipv6;
    std::vector     <uint16_t> m_dst_ipv6;
    bool             m_ipv6_set;

// new section
    bool            m_cap_mode;
    bool            m_cap_mode_set;

    double          m_cap_ipg_min;
    bool            m_cap_ipg_min_set;

    double          m_cap_overide_ipg;
    bool            m_cap_overide_ipg_set;

    bool            m_one_app_server;
    bool            m_one_app_server_was_set;
    bool            m_mac_replace_by_ip;

    CVlanYamlInfo   m_vlan_info;
    CTupleGenYamlInfo m_tuple_gen;
    bool              m_tuple_gen_was_set;
    std::vector     <CFlowYamlInfo> m_vec;
    bool            m_is_plugin_configured; /* any plugin  is configured */

    CTimerWheelYamlInfo         m_tw;

public:
    bool is_any_template(){
        return(m_vec.size()>1?true:false);
    }
    void set_astf_mode();
    void Dump(FILE *fd);
    int load_from_yaml_file(std::string file_name);
    bool verify_correctness(uint32_t num_threads) ;
    bool is_any_plugin_configured(){
        return ( m_is_plugin_configured);
    }
};




class CFlowStats {
public:
    CFlowStats(){
        Clear();
    }
    uint16_t    m_id;
    std::string m_name;
    double m_pkt;
    double m_bytes;
    double duration_sec;
    double m_cps;
    double m_mb_sec;
    double m_mB_sec;
    double m_c_flows;
    double m_pps ;
    uint64_t m_errors ;
    uint64_t m_flows  ;
    CCCapFileMemoryUsage m_memory;

    /* normalized CPS by the number of flows */
    double get_normal_cps(){
        return ( m_cps*(double)m_flows  );
    }
public:
    void Clear();
    void Add(const CFlowStats & obj);

public:
    static void DumpHeader(FILE *fd);
    void Dump(FILE *fd);
};


class CFlowGeneratorRecPerThread {

public:
    bool Create(CTupleGeneratorSmart  * global_gen,
                CFlowYamlInfo *         info,
                CFlowsYamlInfo *        yaml_flow_info,
                CCapFileFlowInfo *      flow_info,
                uint16_t _id,
                uint32_t thread_id );
    void Delete();
public:
    void Dump(FILE *fd);
    inline void generate_flow(CNodeGenerator * gen,
                              dsec_t time,
                              uint64_t flow_id,
                              CGenNode * node);
    void getFlowStats(CFlowStats * stats);

public:
    CTupleTemplateGeneratorSmart  tuple_gen;

    CCapFileFlowInfo *      m_flow_info;
    CFlowYamlInfo *         m_info;
    CFlowsYamlInfo *        m_flows_info;
    CPolicer                m_policer;
    uint16_t                m_id ;
    uint32_t                m_thread_id;
    bool                    m_tuple_gen_was_set;
} __rte_cache_aligned;




class CFlowGeneratorRec {

public:
    bool Create(CFlowYamlInfo * info,
                CFlowsYamlInfo * flow_info,
                uint16_t _id);
    void Delete();
public:

    void Dump(FILE *fd);
    void getFlowStats(CFlowStats * stats);
    void updateIpg(double factor);

public:
    CCapFileFlowInfo m_flow_info;
    CFlowYamlInfo *  m_info;
    CFlowsYamlInfo * m_flows_info;
    CPolicer         m_policer;
    uint16_t         m_id;
private:
    void fixup_ipg_if_needed();
};



class CFlowGenList;

typedef uint32_t flow_id_t;


class CTcpSeq {
public:
    CTcpSeq (){
        client_seq_delta = 0;
        server_seq_delta = 0;
        server_seq_init=false;
    };
    void update(uint8_t *p, CFlowPktInfo *pkt_info, int16_t s_size);
private:
    uint32_t       client_seq_delta;  /* Delta to TCP seq number for client */
    uint32_t       server_seq_delta;  /* Delta to TCP seq number for server */
    bool           server_seq_init;  /* TCP seq been init for server? */
};




/////////////////////////////////////////////////////////////////////////////////
/* per thread info  */

class CTcpPerThreadCtx;
class CEmulAppProgram;
class CMbufBuffer;
class CTcpCtxCb;
class CSyncBarrier;

class CFlowGenListPerThread {

public:


    friend class CNodeGenerator;
    friend class CPluginCallbackSimple;
    friend class CCapFileFlowInfo;

    typedef  CGenericMap<flow_id_t,CGenNode> flow_id_node_t;

    bool Create(uint32_t           thread_id,
                CFlowGenList  *    flow_list,
                uint32_t           max_threads);
    void Delete();


    uint32_t get_max_active_flows_per_core_tcp();

    void set_terminate_mode(bool is_terminate){
        m_terminated_by_master =is_terminate;
    }
    bool is_terminated_by_master(){
        return (m_terminated_by_master);
    }

    void set_vif(CVirtualIF * v_if){
        m_node_gen.set_vif(v_if);
    }

    void set_sync_barrier(CSyncBarrier * sync_b){
        m_sync_b=sync_b;
    }
    CSyncBarrier * get_sync_b(){
        return (m_sync_b);
    }

    void flush_tx_queue() {
        m_node_gen.m_v_if->flush_tx_queue();
    }

    void tickle() {
        m_monitor.tickle();
    }

    template<bool TEARDOWN>
    inline void on_flow_tick(CGenNode *node);


    /* return the dual port ID this thread is attached to in 4 ports configuration
       there are 2 dual-ports

      thread 0 - dual 0
      thread 1 - dual 1

      thread 2 - dual 0
      thread 3 - dual 1

     */
    uint32_t getDualPortId();


    uint8_t  get_memory_socket_id();

public :
    double get_total_kcps();
    double get_total_kcps(pool_index_t pool_idx, bool is_client);
    double get_delta_flow_is_sec();
    double get_longest_flow();
    double get_longest_flow(pool_index_t pool_idx, bool is_client);
    void inc_current_template(void);
    int generate_flows_roundrobin(bool *done);
    int reschedule_flow(CGenNode *node);


    inline CGenNode * create_node(void);

    inline void free_node(CGenNode *p);
    inline void free_last_flow_node(CGenNode *p);



public:
    void Clean();

    void start(std::string &erf_file_name, CPreviewMode &preview);
    void start_sim(const std::string &erf_file_name, CPreviewMode &preview, uint64_t limit = 0);
  
    /**
     * a core provides services for two interfaces
     * it can either be idle, active for one port
     * or active for both
     */
    bool is_port_active(uint8_t port_id) {
        /* for stateful (batch) core is always active,
           for stateless relay the query to the next level
         */
        return m_dp_core->is_port_active(port_id);
    }


    /**
     * returns the two ports associated with this core
     *
     */
    void get_port_ids(uint8_t &p1, uint8_t &p2) {
        p1 = 2 * getDualPortId();
        p2 = p1 + 1;
    }

    void Dump(FILE *fd);
    void DumpCsv(FILE *fd);
    void DumpStats(FILE *fd);
    void Update(void){
        m_cpu_cp_u.Update();
    }
    double getCpuUtil(void){
        return ( m_cpu_cp_u.GetVal());
    }


    bool check_msgs();

    TrexDpCore * get_dp_core() {
        return m_dp_core;
    }

    
private:

    FORCE_NO_INLINE void   no_memory_error();


    
    bool check_msgs_from_rx();

    void handle_nat_msg(CGenNodeNatInfo * msg);
    void handle_latency_pkt_msg(CGenNodeLatencyPktInfo * msg);

    void terminate_nat_flows(CGenNode *node);


    void init_from_global();
    void defer_client_port_free(CGenNode *p);
    void defer_client_port_free(bool is_tcp,uint32_t c_ip,uint16_t port,
                                pool_index_t pool_idx, CTupleGeneratorSmart*gen);


    FORCE_NO_INLINE void   handler_defer_job(CGenNode *p);
    FORCE_NO_INLINE void   handler_defer_job_flush(void);


    inline CGenNodeDeferPort     * get_tcp_defer(void){
        if (m_tcp_dpc==0) {
            m_tcp_dpc =(CGenNodeDeferPort     *)create_node();
            m_tcp_dpc->init();
        }
        return (m_tcp_dpc);
    }

    inline CGenNodeDeferPort     * get_udp_defer(void){
        if (m_udp_dpc==0) {
            m_udp_dpc =(CGenNodeDeferPort     *)create_node();
            m_udp_dpc->init();
        }
        return (m_udp_dpc);
    }


    
private:
    FORCE_NO_INLINE bool associate(uint32_t fid,CGenNode *     node ){
         if (m_flow_id_to_node_lookup.lookup(fid) != 0)
             return false;
        m_stats.m_nat_lookup_add_flow_id++;
        m_flow_id_to_node_lookup.add(fid,node);
        return true;
    }

public:
    uint32_t                         m_thread_id; /* virtual */

    uint32_t                         m_max_threads;
    CFlowGenList                *    m_flow_list;
    rte_mempool_t *                  m_node_pool;

    std::vector<CFlowGeneratorRecPerThread *> m_cap_gen;

    CFlowsYamlInfo                   m_yaml_info;

    CTupleGeneratorSmart             m_smart_gen;

    TrexMonitor                      m_monitor;

public:
    CNodeGenerator                   m_node_gen;
    CNATimerWheel                    m_tw;

public:
    uint32_t                         m_cur_template;
    uint32_t                         m_non_active_nodes; /* the number of non active nodes -> nodes that try to stop somthing */
    uint64_t                         m_cur_flow_id;
    double                           m_cur_time_sec;
    double                           m_stop_time_sec;

    CPreviewMode                     m_preview_mode;
public:
    CFlowGenStats                    m_stats;
    CBwMeasure                       m_mb_sec;
    CCpuUtlDp                        m_cpu_dp_u;
    CCpuUtlCp                        m_cpu_cp_u;

private:
    CGenNodeDeferPort     *          m_tcp_dpc;
    CGenNodeDeferPort     *          m_udp_dpc;

    CNodeRing *                      m_ring_from_rx; /* ring rx thread -> dp */
    CNodeRing *                      m_ring_to_rx;   /* ring dp -> rx thread */

    flow_id_node_t                   m_flow_id_to_node_lookup;

    TrexDpCore                      *m_dp_core; /* polymorphic DP core (stl/stf/astf) */
    bool                             m_terminated_by_master;

public:
    /* TCP stack memory */
    CTcpPerThreadCtx      *         m_c_tcp;
    CTcpCtxCb             *         m_c_tcp_io;
    CTcpPerThreadCtx      *         m_s_tcp;
    CTcpCtxCb             *         m_s_tcp_io;
    bool                            m_tcp_terminate;
    bool                            m_sched_accurate;
    uint32_t                        m_tcp_terminate_cnt;

private:
    CSyncBarrier *                  m_sync_b;
public:
    double tcp_get_tw_tick_in_sec();

    bool Create_tcp();
    void Delete_tcp();

    void generate_flow(bool &done);

    void handle_rx_flush(CGenNode * node,bool on_terminate);
    void handle_tx_fif(CGenNode * node,bool on_terminate);
    void handle_tw(CGenNode * node,bool on_terminate);


private:
    uint8_t                 m_cacheline_pad[RTE_CACHE_LINE_SIZE][19]; // improve prefech
} __rte_cache_aligned ;

inline CGenNode * CFlowGenListPerThread::create_node(void){
    CGenNode * res=(CGenNode *)0;
    if ( unlikely (rte_mempool_get(m_node_pool, (void **)&res) <0) ){
        no_memory_error();
        return (0);
    }
    return (res);
}



inline void CFlowGenListPerThread::free_node(CGenNode *p){
    p->free_base();
    rte_mempool_put(m_node_pool, p);
}

inline void CFlowGenListPerThread::free_last_flow_node(CGenNode *p){
    m_stats.m_total_close_flows +=p->m_flow_info->get_total_flows();

    uint8_t plugin_id =p->get_plugin_id();
    if ( plugin_id ) {
        /* free memory of the plugin */
        on_node_last(plugin_id,p);
    }
    defer_client_port_free(p);
    free_node( p);
}

class CSTTCp;


class CFlowGenList {

public:
    bool Create();
    void Delete();
    void Clean();
public:
    void generate_p_thread_info(uint32_t num_threads);
    void clean_p_thread_info(void);

public:
    int load_astf();
    int load_from_yaml(std::string csv_file,uint32_t num_threads);
    int load_client_config_file(std::string file_name);
    void set_client_config_tuple_gen_info(CTupleGenYamlInfo * tg);
    void get_client_cfg_ip_list(std::vector<ClientCfgCompactEntry *> &ret);
    void set_client_config_resolved_macs(CManyIPInfo &pretest_result);
    void dump_client_config(FILE *fd);

public:
    void Dump(FILE *fd);
    void DumpCsv(FILE *fd);
    void DumpPktSize();
    void UpdateFast();
    double GetCpuUtil();
    double GetCpuUtilRaw();

public:
    /* update ipg in a way for */
    int update_active_flows(uint32_t active_flows);
    double get_worse_case_active_flows();

    double get_total_kcps();
    double get_total_pps();
    double get_total_tx_bps();
    uint32_t get_total_repeat_flows();
    double get_delta_flow_is_sec();

public:
    std::vector<CFlowGeneratorRec *>        m_cap_gen;   /* global info */
    CFlowsYamlInfo                          m_yaml_info; /* global yaml*/
    std::vector<CFlowGenListPerThread   *>  m_threads_info;
    ClientCfgDB                             m_client_config_info;
    CSTTCp                       *          m_stt_cp;
};

inline void CFlowGeneratorRecPerThread::generate_flow(CNodeGenerator * gen,
                                                      dsec_t time,
                                                      uint64_t flow_id,
                                                      CGenNode * node){

    m_flow_info->generate_flow(&tuple_gen,
                               gen,
                               time,
                               flow_id,
                               m_info,
                               node);
}

inline bool CGenNode::is_responder_pkt(){
    return ( m_pkt_info->m_pkt_indication.m_desc.IsInitSide() ?false:true );
}

inline bool CGenNode::is_initiator_pkt(){
    return ( m_pkt_info->m_pkt_indication.m_desc.IsInitSide() ?true:false );
}



inline uint16_t CGenNode::get_template_id(){
    return ( m_pkt_info->m_pkt_indication.m_desc.getId()  );
}


inline bool CGenNode::is_last_in_flow(){
    return ( m_pkt_info->m_pkt_indication.m_desc.IsLastPkt());
}

inline bool CGenNode::is_repeat_flow(){
    return ( m_template_info->m_limit_was_set);
}


inline void CGenNode::update_next_pkt_in_flow_as(void){

    m_time     += m_pkt_info->m_pkt_indication.m_cap_ipg;
    uint32_t pkt_index   = m_pkt_info->m_pkt_indication.m_packet->pkt_cnt;
    pkt_index++;
    m_pkt_info = m_flow_info->GetPacket((pkt_index-1));
}


inline uint32_t CGenNode::update_next_pkt_in_flow_both(void){
    m_time     += m_pkt_info->m_pkt_indication.m_cap_ipg;
    uint32_t dticks = m_pkt_info->m_pkt_indication.m_ticks;
    uint32_t pkt_index   = m_pkt_info->m_pkt_indication.m_packet->pkt_cnt;
    pkt_index++;
    m_pkt_info = m_flow_info->GetPacket((pkt_index-1));
    return (dticks);
}


inline uint32_t CGenNode::update_next_pkt_in_flow_tw(void){

        uint32_t dticks = m_pkt_info->m_pkt_indication.m_ticks;
        uint32_t pkt_index   = m_pkt_info->m_pkt_indication.m_packet->pkt_cnt;
        pkt_index++;
        m_pkt_info = m_flow_info->GetPacket((pkt_index-1));
        return (dticks);
}

inline void CGenNode::reset_pkt_in_flow(void){
        m_pkt_info = m_flow_info->GetPacket(0);
}

enum MINVM_PLUGIN_ID{
    mpRTSP=1,
    mpSIP_VOICE=2,
    mpDYN_PYLOAD=3,
    mpAVL_HTTP_BROWSIN=4  /* this is a way to change the host ip by client ip */
};

class CPluginCallback {
public:
    virtual ~CPluginCallback(){
    }
    virtual void on_node_first(uint8_t plugin_id,CGenNode *     node,CFlowYamlInfo *  template_info, CTupleTemplateGeneratorSmart * tuple_gen,CFlowGenListPerThread  * flow_gen) =0;
    virtual void on_node_last(uint8_t plugin_id,CGenNode *     node)=0;
    virtual rte_mbuf_t * on_node_generate_mbuf(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info)=0;
public:
    static CPluginCallback * callback;
};

class CPluginCallbackSimple : public CPluginCallback {
public:
    virtual void on_node_first(uint8_t plugin_id,CGenNode *     node,
                               CFlowYamlInfo *  template_info,
                               CTupleTemplateGeneratorSmart * tuple_gen,
                               CFlowGenListPerThread  * flow_gen);
    virtual void on_node_last(uint8_t plugin_id,CGenNode *     node);
    virtual rte_mbuf_t * on_node_generate_mbuf(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);

private:
    rte_mbuf_t * rtsp_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);
    rte_mbuf_t * sip_voice_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);
    rte_mbuf_t * dyn_pyload_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);
    rte_mbuf_t * http_plugin(uint8_t plugin_id,CGenNode *     node,CFlowPktInfo * pkt_info);

};


inline bool CGenNode::can_cache_mbuf(void){
    if ( is_repeat_flow() && ( m_flow_info->Size()==1 ) ){
        return (true);
    }else{
        return (false);
    }
}


/* direction for ip addr SERVER put tuple from server side client put addr of client side */
inline  pkt_dir_t CGenNode::cur_pkt_ip_addr_dir(){

    CFlowPktInfo *  lp=m_pkt_info;
    bool init_from_server=get_is_initiator_start_from_server_with_server_addr();
    bool is_init=lp->m_pkt_indication.m_desc.IsInitSide() ^ init_from_server;
    return ( is_init ?CLIENT_SIDE:SERVER_SIDE);
}

/* direction for TCP/UDP port */
inline  pkt_dir_t CGenNode::cur_pkt_port_addr_dir(){
    CFlowPktInfo *  lp=m_pkt_info;
    bool is_init=lp->m_pkt_indication.m_desc.IsInitSide() ;
    return ( is_init ?CLIENT_SIDE:SERVER_SIDE);
}
/* from which interface dir to get out */
inline  pkt_dir_t CGenNode::cur_interface_dir(){

    CFlowPktInfo *  lp=m_pkt_info;

    bool init_from_server=(get_is_initiator_start_from_server()||
                            get_is_initiator_start_from_server_with_server_addr());
    bool is_init=lp->m_pkt_indication.m_desc.IsInitSide() ^ init_from_server;

    if (get_is_all_flow_from_same_dir()) {
        return (is_eligible_from_server_side()?SERVER_SIDE:CLIENT_SIDE);
    }else{
        return ( is_init ?CLIENT_SIDE:SERVER_SIDE);
    }
}

/* Itay: move this to a better place (common for RX STL and RX STF) */
class CRXCoreIgnoreStat {
    friend class CCPortLatency;
    friend class CLatencyManager;
    friend class CStackLegacy;
    
 public:
     
    CRXCoreIgnoreStat() {
        clear();
    }

    inline CRXCoreIgnoreStat operator- (const CRXCoreIgnoreStat &t_in) const {
        CRXCoreIgnoreStat t_out;
        t_out.m_tx_arp = this->m_tx_arp - t_in.m_tx_arp;
        t_out.m_tx_ipv6_n_solic = this->m_tx_ipv6_n_solic - t_in.m_tx_ipv6_n_solic;
        t_out.m_tot_bytes = this->m_tot_bytes - t_in.m_tot_bytes;
        return t_out;
    }
    uint64_t get_tx_bytes() {return m_tot_bytes;}
    uint64_t get_tx_pkts() {return m_tx_arp + m_tx_ipv6_n_solic;}
    uint64_t get_tx_arp() {return m_tx_arp;}
    uint64_t get_tx_n_solic() {return m_tx_ipv6_n_solic;}
    void clear() {
        m_tx_arp = 0;
        m_tx_ipv6_n_solic = 0;
        m_tot_bytes = 0;
    }

 private:
    uint64_t m_tx_arp;
    uint64_t m_tx_ipv6_n_solic;
    uint64_t m_tot_bytes;
};

static_assert(sizeof(CGenNodeNatInfo) == sizeof(CGenNode), "sizeof(CGenNodeNatInfo) != sizeof(CGenNode)" );
static_assert(sizeof(CGenNodeLatencyPktInfo) == sizeof(CGenNode), "sizeof(CGenNodeLatencyPktInfo) != sizeof(CGenNode)" );

static inline void rte_pause_or_delay_lowend() {
    if (unlikely( CGlobalInfo::m_options.m_is_sleepy_scheduler )) {
        delay_sec(LOWEND_LONG_SLEEP_SEC); // sleep for "long" time (highend would rte_pause)
    } else {
        rte_pause();
    }
}

static inline void delay_lowend() {
    if (unlikely( CGlobalInfo::m_options.m_is_sleepy_scheduler )) {
        delay_sec(LOWEND_SHORT_SLEEP_SEC); // sleep for "short" time (highend would do nothing)
    }
}

#endif
