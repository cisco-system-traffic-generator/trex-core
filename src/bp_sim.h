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
#include <common/bitMan.h>
#include <yaml-cpp/yaml.h>
#include "trex_defs.h"
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
#include "flow_stat.h"

#include <trex_stateless_dp_core.h>

class CGenNodePCAP;

#undef NAT_TRACE_

#define FORCE_NO_INLINE __attribute__ ((noinline))
#define FORCE_INLINE __attribute__((always_inline))

/* IP address, last 32-bits of IPv6 remaps IPv4 */
typedef struct {
    uint16_t v6[6];  /* First 96-bits of IPv6 */
    uint32_t v4;  /* Last 32-bits IPv6 overloads v4 */
} ipaddr_t;

/* reserve both 0xFF and 0xFE , router will -1 FF */
#define TTL_RESERVE_DUPLICATE 0xff

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

// Routine to create IPv4 address string
inline int ip_to_str(uint32_t ip,char * str){
    uint32_t ipv4 = PKT_HTONL(ip);
    inet_ntop(AF_INET, (const char *)&ipv4, str, INET_ADDRSTRLEN);
    return(strlen(str));
}

// Routine to create IPv6 address string
inline int ipv6_to_str(ipaddr_t *ip,char * str){
    int idx=0;
    uint16_t ipv6[8];
    for (uint8_t i=0; i<6; i++) {
        ipv6[i] = PKT_HTONS(ip->v6[i]);
    }
    uint32_t ipv4 = PKT_HTONL(ip->v4);
    ipv6[6] = ipv4 & 0xffff;
    ipv6[7] = ipv4 >> 16;

    str[idx++] = '[';
    inet_ntop(AF_INET6, (const char *)&ipv6, &str[1], INET6_ADDRSTRLEN);
    idx = strlen(str);
    str[idx++] = ']';
    str[idx] = 0;
    return(idx);
}

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
struct CGenNode;
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
    tx_per_flow_t m_tx_per_flow[MAX_FLOW_STATS + MAX_FLOW_STATS_PAYLOAD];
    uint64_t   m_seq_num[MAX_FLOW_STATS_PAYLOAD]; // seq num to put in packet for payload rules
    CPerTxthreadTemplateInfo m_template;

public:

    void Add(CVirtualIFPerSideStats * obj){
        m_tx_pkt     += obj->m_tx_pkt;
        m_tx_rx_check_pkt +=obj->m_tx_rx_check_pkt;
        m_tx_bytes   += obj->m_tx_bytes;
        m_tx_drop    += obj->m_tx_drop;
        m_tx_alloc_error += obj->m_tx_alloc_error;
        m_tx_queue_full +=obj->m_tx_queue_full;
        m_template.Add(&obj->m_template);
    }

    void Clear(){
       m_tx_pkt=0;
       m_tx_rx_check_pkt=0;
       m_tx_bytes=0;
       m_tx_drop=0;
       m_tx_alloc_error=0;
       m_tx_queue_full=0;
       m_template.Clear();
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
    m_template.Dump(fd);
}






class CVirtualIF {
public:


    CVirtualIF (){
        m_preview_mode =NULL;
    }

    virtual ~CVirtualIF(){
    }
public:

    virtual int open_file(std::string file_name)=0;

    virtual int close_file(void)=0;


    /**
     * send one packet
     *
     * @param node
     *
     * @return
     */
    virtual int send_node(CGenNode * node) =0;


    /**
     * send one packet to a specific dir. flush all packets
     *
     * @param dir
     * @param m
     */
    virtual void send_one_pkt(pkt_dir_t       dir, rte_mbuf_t      *m){
    }


    /**
     * flush all pending packets into the stream
     *
     * @return
     */
    virtual int flush_tx_queue(void)=0;
    // read all packets from rx_queue on dp core
    virtual void flush_dp_rx_queue(void) {};
    // read all packets from rx queue
    virtual void flush_rx_queue(void) {};

    /**
     * update the source and destination mac-addr of a given mbuf by global database
     *
     * @param dir
     * @param m
     *
     * @return
     */
    virtual int update_mac_addr_from_global_cfg(pkt_dir_t       dir, uint8_t * p)=0;

    /**
     * translate a port_id to the correct dir on the core
     *
     */
    virtual pkt_dir_t port_id_to_dir(uint8_t port_id) {
        return (CS_INVALID);
    }

public:


    void set_review_mode(CPreviewMode  * preview_mode){
        m_preview_mode =preview_mode;
    }

protected :
    CPreviewMode            * m_preview_mode;

public:
    CVirtualIFPerSideStats    m_stats[CS_NUM];
};



/* global info */

#define CONST_NB_MBUF  16380

/* this is the first small part of the packet that we manipulate */
#define FIRST_PKT_SIZE 64
#define CONST_SMALL_MBUF_SIZE (FIRST_PKT_SIZE + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)


#define _128_MBUF_SIZE 128
#define _256_MBUF_SIZE 256
#define _512_MBUF_SIZE 512
#define _1024_MBUF_SIZE 1024
#define _2048_MBUF_SIZE 2048
#define _4096_MBUF_SIZE 4096
#define MAX_PKT_ALIGN_BUF_9K       (9*1024+64)

#define MBUF_PKT_PREFIX ( sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM )

#define CONST_128_MBUF_SIZE (128 + MBUF_PKT_PREFIX )
#define CONST_256_MBUF_SIZE (256 + MBUF_PKT_PREFIX )
#define CONST_512_MBUF_SIZE (512 + MBUF_PKT_PREFIX)
#define CONST_1024_MBUF_SIZE (1024 + MBUF_PKT_PREFIX)
#define CONST_2048_MBUF_SIZE (2048 + MBUF_PKT_PREFIX)
#define CONST_4096_MBUF_SIZE (4096 + MBUF_PKT_PREFIX)
#define CONST_9k_MBUF_SIZE   (MAX_PKT_ALIGN_BUF_9K + MBUF_PKT_PREFIX)


class CPreviewMode {
public:
    CPreviewMode(){
        clean();
    }
    void clean(){
        m_flags = 0;
        m_flags1=0;
        setCores(1);
        set_zmq_publish_enable(true);
    }

    void setFileWrite(bool enable){
        btSetMaskBit32(m_flags,0,0,enable?1:0);
    }

    bool getFileWrite(){
        return (btGetMaskBit32(m_flags,0,0) ? true:false);
    }

    void setDisableMbufCache(bool enable){
        btSetMaskBit32(m_flags,2,2,enable?1:0);
    }

    bool isMbufCacheDisabled(){
        return (btGetMaskBit32(m_flags,2,2) ? true:false);
    }

    void set_disable_flow_control_setting(bool enable){
        btSetMaskBit32(m_flags,4,4,enable?1:0);
    }

    bool get_is_disable_flow_control_setting(){
        return (btGetMaskBit32(m_flags,4,4) ? true:false);
    }


          /* learn & verify mode  */
    void set_learn_and_verify_mode_enable(bool enable){
        btSetMaskBit32(m_flags,5,5,enable?1:0);
    }

    bool get_learn_and_verify_mode_enable(){
        return (btGetMaskBit32(m_flags,5,5) ? true:false);
    }

  /* IPv6 enable/disable */
    void set_ipv6_mode_enable(bool enable){
        btSetMaskBit32(m_flags,7,7,enable?1:0);
    }

    bool get_ipv6_mode_enable(){
        return (btGetMaskBit32(m_flags,7,7) ? true:false);
    }

    void setVMode(uint8_t vmode){
        btSetMaskBit32(m_flags,10,8,vmode);
    }
    uint8_t  getVMode(){
        return (btGetMaskBit32(m_flags,10,8) );
    }


    void setRealTime(bool enable){
        btSetMaskBit32(m_flags,11,11,enable?1:0);
    }

    bool getRealTime(){
        return (btGetMaskBit32(m_flags,11,11) ? true:false);
    }

    void setClientServerFlip(bool enable){
        btSetMaskBit32(m_flags,12,12,enable?1:0);
    }

    bool getClientServerFlip(){
        return (btGetMaskBit32(m_flags,12,12) ? true:false);
    }

    void setSingleCore(bool enable){
        btSetMaskBit32(m_flags,13,13,enable?1:0);
    }

    bool getSingleCore(){
        return (btGetMaskBit32(m_flags,13,13) ? true:false);
    }

    /* -p */
    void setClientServerFlowFlip(bool enable){
        btSetMaskBit32(m_flags,14,14,enable?1:0);
    }

    bool getClientServerFlowFlip(){
        return (btGetMaskBit32(m_flags,14,14) ? true:false);
    }



    void setNoCleanFlowClose(bool enable){
        btSetMaskBit32(m_flags,15,15,enable?1:0);
    }

    bool getNoCleanFlowClose(){
        return (btGetMaskBit32(m_flags,15,15) ? true:false);
    }

    void setCores(uint8_t cores){
        btSetMaskBit32(m_flags,24,16,cores);
    }

    uint8_t getCores(){
        return (btGetMaskBit32(m_flags,24,16) );
    }

    bool  getIsOneCore(){
        return (getCores()==1?true:false);
    }

    void setOnlyLatency(bool enable){
        btSetMaskBit32(m_flags,25,25,enable?1:0);
    }

    bool getOnlyLatency(){
        return (btGetMaskBit32(m_flags,25,25) ? true:false);
    }


    void set_1g_mode(bool enable){
        btSetMaskBit32(m_flags,26,26,enable?1:0);
    }

    bool get_1g_mode(){
        return (btGetMaskBit32(m_flags,26,26) ? true:false);
    }


    void set_zmq_publish_enable(bool enable){
        btSetMaskBit32(m_flags,27,27,enable?1:0);
    }

    bool get_zmq_publish_enable(){
        return (btGetMaskBit32(m_flags,27,27) ? true:false);
    }

    void set_pcap_mode_enable(bool enable){
        btSetMaskBit32(m_flags,28,28,enable?1:0);
    }

    bool get_pcap_mode_enable(){
        return (btGetMaskBit32(m_flags,28,28) ? true:false);
    }

    /* VLAN enable/disable */
    bool get_vlan_mode_enable(){
        return (btGetMaskBit32(m_flags,29,29) ? true:false);
    }

    void set_vlan_mode_enable(bool enable){
        btSetMaskBit32(m_flags,29,29,enable?1:0);
    }

    bool get_mac_ip_overide_enable(){
        return (btGetMaskBit32(m_flags,30,30) ? true:false);
    }

    void set_mac_ip_overide_enable(bool enable){
        btSetMaskBit32(m_flags,30,30,enable?1:0);
        if (enable) {
            set_mac_ip_features_enable(enable);
        }
    }

    bool get_is_rx_check_enable(){
        return (btGetMaskBit32(m_flags,31,31) ? true:false);
    }

    void set_rx_check_enable(bool enable){
        btSetMaskBit32(m_flags,31,31,enable?1:0);
    }


    bool get_mac_ip_features_enable(){
        return (btGetMaskBit32(m_flags1,0,0) ? true:false);
    }

    void set_mac_ip_features_enable(bool enable){
        btSetMaskBit32(m_flags1,0,0,enable?1:0);
    }

    bool get_mac_ip_mapping_enable(){
        return (btGetMaskBit32(m_flags1,1,1) ? true:false);
    }

    void set_mac_ip_mapping_enable(bool enable){
        btSetMaskBit32(m_flags1,1,1,enable?1:0);
        if (enable) {
            set_mac_ip_features_enable(enable);
        }
    }

    bool get_vm_one_queue_enable(){
        return (btGetMaskBit32(m_flags1,2,2) ? true:false);
    }

    void set_no_keyboard(bool enable){
        btSetMaskBit32(m_flags1,5,5,enable?1:0);
    }

    bool get_no_keyboard(){
        return (btGetMaskBit32(m_flags1,5,5) ? true:false);
    }

    void set_vm_one_queue_enable(bool enable){
        btSetMaskBit32(m_flags1,2,2,enable?1:0);
    }

    /* -e */
    void setClientServerFlowFlipAddr(bool enable){
        btSetMaskBit32(m_flags1,3,3,enable?1:0);
    }

    bool getClientServerFlowFlipAddr(){
        return (btGetMaskBit32(m_flags1,3,3) ? true:false);
    }


    /* split mac is enabled */
    void setDestMacSplit(bool enable){
        btSetMaskBit32(m_flags1,4,4,enable?1:0);
    }

    bool getDestMacSplit(){
        return (btGetMaskBit32(m_flags1,4,4) ? true:false);
    }




public:
    void Dump(FILE *fd);

private:
    uint32_t      m_flags;
    uint32_t      m_flags1;


};



typedef  struct mac_align_t_ {
        uint8_t dest[6];
        uint8_t src[6];
        uint8_t pad[4];
} mac_align_t  ;

struct CMacAddrCfg {
public:
    CMacAddrCfg (){
        memset(u.m_data,0,sizeof(u.m_data));
        u.m_mac.dest[3]=1;
        u.m_mac.src[3]=1;
    }
    union {
        mac_align_t m_mac;
        uint8_t     m_data[16];
    } u;
} __rte_cache_aligned; ;

struct CParserOption {

public:
    /* Runtime flags */
    enum {
        RUN_FLAGS_RXCHECK_CONST_TS =1,
    };

    /**
     * different running modes for Trex
     */
    enum trex_run_mode_e {
        RUN_MODE_INVALID,
        RUN_MODE_BATCH,
        RUN_MODE_INTERACTIVE
    };

    enum trex_learn_mode_e {
    LEARN_MODE_DISABLED=0,
    LEARN_MODE_TCP_ACK=1,
    LEARN_MODE_IP_OPTION=2,
    LEARN_MODE_MAX=LEARN_MODE_IP_OPTION
    };

public:
    CParserOption(){
        m_factor=1.0;
        m_mbuf_factor=1.0;
        m_duration=0.0;
        m_latency_rate =0;
        m_latency_mask =0xffffffff;
        m_latency_prev=0;
        m_zmq_port=4500;
        m_telnet_port =4501;
        m_platform_factor=1.0;
        m_expected_portd = 4; /* should be at least the number of ports found in the system but could be less */
        m_vlan_port[0]=100;
        m_vlan_port[1]=100;
        m_rx_check_sample=0;
        m_rx_check_hops = 0;
        m_io_mode=1;
        m_run_flags=0;
        prefix="";
        m_mac_splitter=0;
        m_run_mode = RUN_MODE_INVALID;
        m_l_pkt_mode = 0;
        m_rx_thread_enabled = false;
    }


    CPreviewMode    preview;
    float           m_factor;
    float           m_mbuf_factor;
    float           m_duration;
    float           m_platform_factor;
    uint16_t		m_vlan_port[2]; /* vlan value */
    uint16_t		m_src_ipv6[6];  /* Most signficant 96-bits */
    uint16_t		m_dst_ipv6[6];  /* Most signficant 96-bits */

    uint32_t        m_latency_rate; /* pkt/sec for each thread/port zero disable */
    uint32_t        m_latency_mask;
    uint32_t        m_latency_prev;
    uint16_t        m_rx_check_sample; /* the sample rate of flows */
    uint16_t        m_rx_check_hops;
    uint16_t        m_zmq_port;
    uint16_t        m_telnet_port;
    uint16_t        m_expected_portd;
    uint16_t        m_io_mode; //0,1,2 0 disable, 1- normal , 2 - short
    uint16_t        m_run_flags;
    uint8_t         m_mac_splitter;
    uint8_t         m_l_pkt_mode;
    uint8_t         m_learn_mode;
    uint16_t        m_debug_pkt_proto;
    bool            m_rx_thread_enabled;
    trex_run_mode_e    m_run_mode;



    std::string        cfg_file;
    std::string        mac_file;
    std::string        platform_cfg_file;

    std::string        out_file;
    std::string        prefix;


    CMacAddrCfg     m_mac_addr[TREX_MAX_PORTS];

    uint8_t *       get_src_mac_addr(int if_index){
        return (m_mac_addr[if_index].u.m_mac.src);
    }
    uint8_t *       get_dst_src_mac_addr(int if_index){
        return (m_mac_addr[if_index].u.m_mac.dest);
    }

public:
    uint32_t get_expected_ports(){
        return (m_expected_portd);
    }

    /* how many dual ports supported */
    uint32_t get_expected_dual_ports(void){
        return (m_expected_portd>>1);
    }

    uint32_t get_number_of_dp_cores_needed() {
        return ( (m_expected_portd>>1)   * preview.getCores());
    }
    bool is_stateless(){
        return (m_run_mode == RUN_MODE_INTERACTIVE ?true:false);
    }
    bool is_latency_enabled() {
        return ( (m_latency_rate == 0) ? false : true);
    }
    bool is_rx_enabled() {
        return m_rx_thread_enabled;
    }
    void set_rx_enabled() {
        m_rx_thread_enabled = true;
    }

    inline void set_rxcheck_const_ts(){
        m_run_flags |= RUN_FLAGS_RXCHECK_CONST_TS;
    }
    inline void clear_rxcheck_const_ts(){
        m_run_flags &=~ RUN_FLAGS_RXCHECK_CONST_TS;
    }

    inline bool is_rxcheck_const_ts(){
        return (  (m_run_flags &RUN_FLAGS_RXCHECK_CONST_TS)?true:false );
    }

    inline uint8_t get_l_pkt_mode(){
        return (m_l_pkt_mode);
    }
    void dump(FILE *fd);
    bool is_valid_opt_val(int val, int min, int max, const std::string &opt_name);
};


class  CGlobalMemory {

public:
    CGlobalMemory(){
        CPlatformMemoryYamlInfo info;
        m_num_cores=1;
        m_pool_cache_size=32;
    }
    void set(const CPlatformMemoryYamlInfo &info,float mul);

    uint32_t get_2k_num_blocks(){
        return ( m_mbuf[MBUF_2048]);
    }

    uint32_t get_each_core_dp_flows(){
        return ( m_mbuf[MBUF_DP_FLOWS]/m_num_cores );
    }
    void set_number_of_dp_cors(uint32_t cores){
        m_num_cores = cores;
    }

    void set_pool_cache_size(uint32_t pool_cache){
        m_pool_cache_size=pool_cache;
    }

    void Dump(FILE *fd);

public:
    uint32_t         m_mbuf[MBUF_ELM_SIZE]; // relative to traffic norm to 2x10G ports
    uint32_t         m_num_cores;
    uint32_t         m_pool_cache_size;

};

typedef uint8_t socket_id_t;
typedef uint8_t port_id_t;
/* the real phsical thread id */
typedef uint8_t physical_thread_id_t;


typedef uint8_t virtual_thread_id_t;
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

#define MAX_SOCKETS_SUPPORTED   (4)
#define MAX_THREADS_SUPPORTED   (120)


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

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id)=0;

    /* return  the map betwean virtual to phy id */
    virtual physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id)=0;

    virtual bool thread_phy_is_master(physical_thread_id_t  phy_id)=0;
    virtual bool thread_phy_is_rx(physical_thread_id_t  phy_id)=0;

    virtual void dump(FILE *fd)=0;
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

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    bool thread_phy_is_master(physical_thread_id_t  phy_id);
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

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    bool thread_phy_is_master(physical_thread_id_t  phy_id);
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

    /* virtual thread_id is always from   1..number of threads  virtual  */
    virtual_thread_id_t thread_phy_to_virt(physical_thread_id_t  phy_id);

    /* return  the map betwean virtual to phy id */
    physical_thread_id_t thread_virt_to_phy(virtual_thread_id_t virt_id);

    bool thread_phy_is_master(physical_thread_id_t  phy_id);
    bool thread_phy_is_rx(physical_thread_id_t  phy_id);

    void dump(FILE *fd);


private:
    CPlatformSocketInfoBase * m_obj;
    CPlatformCoresYamlInfo * m_platform;
};

class CRteMemPool {

public:
    inline rte_mbuf_t   * _rte_pktmbuf_alloc(rte_mempool_t * mp ){
        rte_mbuf_t   * m=rte_pktmbuf_alloc(mp);
        if ( likely(m>0) ) {
            return (m);
        }
        dump_in_case_of_error(stderr);
        assert(0);
    }

    inline rte_mbuf_t   * pktmbuf_alloc(uint16_t size){

        rte_mbuf_t        * m;
        if ( size < _128_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_128);
        }else if ( size < _256_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_256);
        }else if (size < _512_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_512);
        }else if (size < _1024_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_1024);
        }else if (size < _2048_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_2048);
        }else if (size < _4096_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_4096);
        }else{
            assert(size<MAX_PKT_ALIGN_BUF_9K);
            m = _rte_pktmbuf_alloc(m_mbuf_pool_9k);
        }
        return (m);
    }

    inline rte_mbuf_t   * pktmbuf_alloc_small(){
        return ( _rte_pktmbuf_alloc(m_small_mbuf_pool) );
    }


    void dump(FILE *fd);

    void dump_in_case_of_error(FILE *fd);

    std::string dump_as_json(uint8_t id,bool last);

private:
        std::string add_to_json(std::string name,
                                rte_mempool_t * pool,
                                bool last=false);


public:
    rte_mempool_t *   m_small_mbuf_pool; /* pool for start packets */

    rte_mempool_t *   m_mbuf_pool_128;
    rte_mempool_t *   m_mbuf_pool_256;
    rte_mempool_t *   m_mbuf_pool_512;
    rte_mempool_t *   m_mbuf_pool_1024;
    rte_mempool_t *   m_mbuf_pool_2048;
    rte_mempool_t *   m_mbuf_pool_4096;
    rte_mempool_t *   m_mbuf_pool_9k;

    rte_mempool_t *   m_mbuf_global_nodes;
    uint32_t          m_pool_id;
};




class CGlobalInfo {
public:
    static void init_pools(uint32_t rx_buffers);
    /* for simulation */
    static void free_pools();


    static inline rte_mbuf_t   * pktmbuf_alloc_small(socket_id_t socket){
        return ( m_mem_pool[socket].pktmbuf_alloc_small() );
    }



    /**
     * try to allocate small buffers too
     * _alloc allocate big buffers only
     *
     * @param socket
     * @param size
     *
     * @return
     */
    static inline rte_mbuf_t   * pktmbuf_alloc(socket_id_t socket,uint16_t size){
        if (size<FIRST_PKT_SIZE) {
            return ( pktmbuf_alloc_small(socket));
        }
        return (m_mem_pool[socket].pktmbuf_alloc(size));
    }


    static inline bool is_learn_verify_mode(){
        return ( (m_options.m_learn_mode != CParserOption::LEARN_MODE_DISABLED) && m_options.preview.get_learn_and_verify_mode_enable());
    }

    static inline bool is_learn_mode(){
        return ( (m_options.m_learn_mode != CParserOption::LEARN_MODE_DISABLED));
    }

    static inline bool is_learn_mode(CParserOption::trex_learn_mode_e mode){
        return ( (m_options.m_learn_mode == mode));
    }

    static inline bool is_ipv6_enable(void){
        return ( m_options.preview.get_ipv6_mode_enable() );
    }

    static inline bool is_realtime(void){
        //return (false);
        return ( m_options.preview.getRealTime() );
    }

    static inline void set_realtime(bool enable){
        m_options.preview.setRealTime(enable);
    }

    static uint32_t get_node_pool_size(){
        return (m_nodes_pool_size);
    }

    static inline CGenNode * create_node(void){
        CGenNode * res;
        if ( unlikely (rte_mempool_get(m_mem_pool[0].m_mbuf_global_nodes, (void **)&res) <0) ){
            rte_exit(EXIT_FAILURE, "can't allocate m_mbuf_global_nodes  objects try to tune the configuration file \n");
            return (0);
        }
        return (res);
    }


    static inline void free_node(CGenNode *p){
        rte_mempool_put(m_mem_pool[0].m_mbuf_global_nodes, p);
    }


    static std::string dump_pool_as_json(void);


public:
    static CRteMemPool       m_mem_pool[MAX_SOCKETS_SUPPORTED];

    static uint32_t              m_nodes_pool_size;
    static CParserOption         m_options;
    static CGlobalMemory         m_memory_cfg;
    static CPlatformSocketInfo   m_socket;
};

static inline int get_is_stateless(){
    return (CGlobalInfo::m_options.is_stateless() );
}

static inline int get_is_rx_check_mode(){
    return (CGlobalInfo::m_options.preview.get_is_rx_check_enable() ?1:0);
}

static inline bool get_is_rx_filter_enable(){
    uint32_t latency_rate=CGlobalInfo::m_options.m_latency_rate;
    return ( ( get_is_rx_check_mode() || CGlobalInfo::is_learn_mode() || latency_rate != 0
               || get_is_stateless()) ?true:false );
}
static inline uint16_t get_rx_check_hops() {
    return (CGlobalInfo::m_options.m_rx_check_hops);
}

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
    }

    std::string     m_name;
    std::string     m_client_pool_name;
    std::string     m_server_pool_name;
    double          m_k_cps;    //k CPS
    double          m_restart_time; /* restart time of this template */
    dsec_t          m_ipg_sec;   // ipg in sec
    dsec_t          m_rtt_sec;   // rtt in sec
    uint32_t        m_w;
    uint32_t        m_wlength;
    uint32_t        m_limit;
    uint32_t        m_flowcnt;
    uint8_t         m_plugin_id; /* 0 - default , 1 - RTSP160 , 2- RTSP250 */
    uint8_t         m_client_pool_idx;
    uint8_t         m_server_pool_idx;
    bool            m_one_app_server;
    uint32_t        m_server_addr;
    bool            m_one_app_server_was_set;
    bool            m_cap_mode;
    bool            m_cap_mode_was_set;
    bool            m_wlength_set;
    bool            m_limit_was_set;
    CFlowYamlDynamicPyloadPlugin * m_dpPkt; /* plugin */

public:
    void Dump(FILE *fd);
};




#define _1MB_DOUBLE ((double)(1024.0*1024.0))
#define _1GB_DOUBLE ((double)(1024.0*1024.0*1024.0))

#define _1Mb_DOUBLE ((double)(1000.0*1000.0))


#define _1MB ((1024*1024)ULL)
#define _1GB 1000000000ULL
#define _500GB (_1GB*500)



#define DP(f) if (f) printf(" %-40s: %llu \n",#f,(unsigned long long)f)
#define DP_name(n,f) if (f) printf(" %-40s: %llu \n",n,(unsigned long long)f)

#define DP_S(f,f_s) if (f) printf(" %-40s: %s \n",#f,f_s.c_str())

class CFlowPktInfo;



typedef enum {
    KBYE_1024,
    KBYE_1000
} human_kbyte_t;

std::string double_to_human_str(double num,
                                std::string units,
                                human_kbyte_t etype);



class CCapFileFlowInfo ;

#define SYNC_TIME_OUT ( 1.0/1000)

//#define SYNC_TIME_OUT ( 2000.0/1000)

/* this is a simple struct, do not add constructor and destractor here!
   we are optimizing the allocation dealocation !!!
 */

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

//private:

    CTupleGeneratorSmart *m_tuple_gen;
    // cache line 1 - 64bytes waste of space !
    uint32_t            m_nat_external_ipv4; /* client */
    uint32_t            m_nat_external_ipv4_server;
    uint16_t            m_nat_external_port;

    uint16_t            m_nat_pad[3];
    mac_addr_align_t    m_src_mac;
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

    inline void update_next_pkt_in_flow(void);
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

    inline void set_nat_learn_state(){
        m_type=FLOW_PKT; /* normal operation .. repeat might work too */
    }

public:
    inline uint32_t get_short_fid(void){
        return (((uint32_t)m_flow_id) & NAT_FLOW_ID_MASK);
    }

    inline uint8_t get_thread_id(void){
        return (m_thread_id);
    }

    inline void set_nat_ipv4_addr_server(uint32_t ip){
        m_nat_external_ipv4_server =ip;
    }

    inline uint32_t  get_nat_ipv4_addr_server(){
        return ( m_nat_external_ipv4_server );
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
             (get_nat_ipv4_port()==m_src_port) &&
             ( get_nat_ipv4_addr_server() == m_dest_ip) ) {
            return (true);
        }else{
            return (false);
        }
    }


public:
    inline void replace_tuple(void);

} __rte_cache_aligned;





#if __x86_64__
/* size of 64 bytes */
    #define DEFER_CLIENTS_NUM (16)
#else
    #define DEFER_CLIENTS_NUM (16)
#endif

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
    uint8_t             m_pool_idx[DEFER_CLIENTS_NUM];
public:
    void init(void){
        m_type=CGenNode::FLOW_DEFER_PORT_RELEASE;
        m_cnt=0;
    }

    /* return true if object is full */
    bool add_client(uint8_t pool_idx, uint32_t client,
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
    uint64_t                         m_nat_lookup_add_flow_id;
    uint64_t                         m_nat_flow_timeout;
    uint64_t                         m_nat_flow_learn_error;

public:
    void clear();
    void dump(FILE *fd);
};



typedef std::priority_queue<CGenNode *, std::vector<CGenNode *>,CGenNodeCompare> pqueue_t;



class CErfIF : public CVirtualIF {

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

     typedef enum { scINIT = 0x17,
                    scWORK ,
                    scWAIT , 
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
    void  remove_all_stateless(CFlowGenListPerThread * thread);

    int   open_file(std::string file_name,
                    CPreviewMode * preview);
    int   close_file(CFlowGenListPerThread * thread);
    int   flush_file(dsec_t max_time,
                     dsec_t d_time,
                     bool always,
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
    inline int   flush_one_node_to_file(CGenNode * node){
        return (m_v_if->send_node(node));
    }
    int   update_stats(CGenNode * node);
    int   update_stl_stats(CGenNodeStateless *node_sl);
    bool  has_limit_reached();

    FORCE_NO_INLINE bool handle_slow_messages(uint8_t type,
                                              CGenNode * node,
                                              CFlowGenListPerThread * thread,
                                              bool always);

private:
        void add_exit_node(CFlowGenListPerThread * thread,
                                          dsec_t max_time);

        inline bool handle_stl_node(CGenNode * node,
                                    CFlowGenListPerThread * thread);


        FORCE_INLINE bool do_work_stl(CGenNode * node,
                                      CFlowGenListPerThread * thread,
                                      bool always);
        
        FORCE_INLINE bool do_work_both(CGenNode * node,
                                      CFlowGenListPerThread * thread,
                                      dsec_t d_time,
                                      bool always);
        
        template<int SCH_MODE>
        FORCE_INLINE bool do_work(CGenNode * node,
                                  CFlowGenListPerThread * thread,
                                  dsec_t d_time,
                                  bool always);
        
        FORCE_INLINE void do_sleep(dsec_t & cur_time,
                                   CFlowGenListPerThread * thread,
                                   dsec_t ntime);
        
        
        FORCE_INLINE int teardown(CFlowGenListPerThread * thread,
                                   bool always,
                                   double &old_offset,
                                   double offset);
        
        template<int SCH_MODE>
        int flush_file_realtime(dsec_t max_time, 
                                dsec_t d_time,
                                bool always,
                                CFlowGenListPerThread * thread,
                                double &old_offset);
        
        int flush_file_sim(dsec_t max_time, 
                            dsec_t d_time,
                            bool always,
                            CFlowGenListPerThread * thread,
                            double &old_offset);
private:        
    void handle_command(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler);
    void handle_flow_pkt(CGenNode *node, CFlowGenListPerThread *thread);
    void handle_flow_sync(CGenNode *node, CFlowGenListPerThread *thread, bool &exit_scheduler);
    void handle_pcap_pkt(CGenNode *node, CFlowGenListPerThread *thread);

public:
    pqueue_t                  m_p_queue;
    socket_id_t               m_socket_id;
    bool                      m_is_realtime;
    CVirtualIF *              m_v_if;
    CFlowGenListPerThread  *  m_parent;
    CPreviewMode              m_preview_mode;
    uint64_t                  m_cnt;
    uint64_t                  m_non_active;
    uint64_t                  m_limit;
    CTimeHistogram            m_realtime_his;
};


class CPolicer {

public:

    CPolicer(){
        ClearMeter();
    }

    void ClearMeter(){
        m_cir=0.0;
        m_bucket_size=1.0;
        m_level=0.0;
        m_last_time=0.0;
    }

    bool update(double dsize,dsec_t now_sec);

    void set_cir(double cir){
        BP_ASSERT(cir>=0.0);
        m_cir=cir;
    }
    void set_level(double level){
        m_level =level;
    }

    void set_bucket_size(double bucket){
        m_bucket_size =bucket;
    }

private:

    double                      m_cir;

    double                      m_bucket_size;

    double                      m_level;

    double                      m_last_time;
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
    dsec_t       m_cap_ipg; /* ipg from cap file */
    CCapPktRaw * m_packet;

    CFlow *          m_flow;
    EthernetHeader * m_ether;
    union {
        IPHeader       * m_ipv4;
        IPv6Header     * m_ipv6;
    } l3;
    bool        m_is_ipv6;
    union {
        TCPHeader * m_tcp;
        UDPHeader * m_udp;
        ICMPHeader * m_icmp;
    } l4;
    uint8_t *       m_payload;
    uint16_t        m_payload_len;
    uint16_t        m_packet_padding; /* total packet size - IP total length */


    CFlowKey            m_flow_key;
    CPacketDescriptor   m_desc;

    uint8_t             m_ether_offset;
    uint8_t             m_ip_offset;
    uint8_t             m_udp_tcp_offset;
    uint8_t             m_payload_offset;

public:

    void Dump(FILE *fd,int verbose);
    void Clean();
    bool ConvertPacketToIpv6InPlace(CCapPktRaw * pkt,
                                    int offset);
    void ProcessPacket(CPacketParser *parser,CCapPktRaw * pkt);
    void Clone(CPacketIndication * obj,CCapPktRaw * pkt);
    void RefreshPointers(void);
    void UpdatePacketPadding();

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
    inline void replace_tuple(CGenNode * node);

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
     * 2. mask the packet as learn
     * 3. update the option pointer
     */
    void   mask_as_learn();

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


inline void CFlowPktInfo::replace_tuple(CGenNode * node){
    update_pkt_info(m_packet->raw,node);
}

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
        ipv4->updateCheckSum();
    }



    /* replace port base on TCP/UDP */
    if ( m_pkt_indication.m_desc.IsTcp() ) {
        TCPHeader * m_tcp = (TCPHeader *)(p +m_pkt_indication.getFastTcpOffset());
        BP_ASSERT(m_tcp);
        /* replace port */
        if ( flow_info->is_init_port_dir  ) {
            m_tcp->setSourcePort(flow_info->client_port);
            if ( flow_info->replace_server_port ){
                m_tcp->setDestPort(flow_info->server_port);
            }
        }else{
            m_tcp->setDestPort(flow_info->client_port);
            if ( flow_info->replace_server_port ){
                m_tcp->setSourcePort(flow_info->server_port);
            }
        }

    }else {
        if ( m_pkt_indication.m_desc.IsUdp() ){
            UDPHeader * m_udp =(UDPHeader *)(p +m_pkt_indication.getFastTcpOffset() );
            BP_ASSERT(m_udp);
            m_udp->setLength(m_udp->getLength() + update_len);
            m_udp->setChecksum(0);
            if ( flow_info->is_init_port_dir  ) {
                m_udp->setSourcePort(flow_info->client_port);
                if ( flow_info->replace_server_port ){
                    m_udp->setDestPort(flow_info->server_port);
                }
            }else{
                m_udp->setDestPort(flow_info->client_port);
                if ( flow_info->replace_server_port ){
                    m_udp->setSourcePort(flow_info->server_port);
                }
            }
        }else{
            BP_ASSERT(0);
        }
    }
}


inline void CFlowPktInfo::update_pkt_info(char *p,
                                   CGenNode * node){

    IPHeader       * ipv4=
        (IPHeader       *)(p + m_pkt_indication.getFastIpOffsetFast());

    EthernetHeader * et  =
        (EthernetHeader * )(p + m_pkt_indication.getFastEtherOffset());

    (void)et;

    uint16_t src_port =   node->m_src_port;

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
                ipv4->setTimeToLive(TTL_RESERVE_DUPLICATE);

                /* first ipv4 option add the info in case of learn packet, usualy only the first packet */
        if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_IP_OPTION)) {
            CNatOption *lpNat =(CNatOption *)ipv4->getOption();
            lpNat->set_fid(node->get_short_fid());
            lpNat->set_thread_id(node->get_thread_id());
        } else {
            // This method only work on first TCP SYN
            if (ipv4->getProtocol() == IPPROTO_TCP) {
            TCPHeader *tcp = (TCPHeader *)(((uint8_t *)ipv4) + ipv4->getHeaderLength());
            if (tcp->getSynFlag()) {
                tcp->setAckNumber(CNatRxManager::calc_tcp_ack_val(node->get_short_fid(), node->get_thread_id()));
            }
#ifdef NAT_TRACE_
            printf(" %.3f : flow_id: %x thread_id %x TCP ack %x\n",now_sec(), node->get_short_fid(), node->get_thread_id(), tcp->getAckNumber());
#endif
            }
        }
            }
            /* in all cases update the ip using the outside ip */

            if ( m_pkt_indication.m_desc.IsInitSide()  ) {
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : DP : i %x:%x -> %x  flow_id: %lx\n",now_sec(),node->m_src_ip,node->m_src_port,node->m_dest_ip,node->m_flow_id);
                }
#endif

                ipv4->updateIpSrc(node->m_src_ip);
                ipv4->updateIpDst(node->m_dest_ip);
            }else{
#ifdef NAT_TRACE_
                if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                    printf(" %.3f : r %x   -> %x:%x  flow_id: %lx \n",now_sec(),node->m_dest_ip,node->m_src_ip,node->m_src_port,node->m_flow_id);
                }
#endif
                src_port = node->get_nat_ipv4_port();
                ipv4->updateIpSrc(node->get_nat_ipv4_addr_server());
                ipv4->updateIpDst(node->get_nat_ipv4_addr());
            }

            /* TBD remove this */
#ifdef NAT_TRACE_
            if (node->m_flags != CGenNode::NODE_FLAGS_LATENCY ) {
                if ( m_pkt_indication.m_desc.IsInitSide() ==false ){
                    printf(" %.3f : pkt ==> %x:%x %x:%x \n",now_sec(),node->get_nat_ipv4_addr(),node->get_nat_ipv4_addr_server(),
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

        ipv4->updateCheckSum();
    }


    /* replace port base on TCP/UDP */
    if ( m_pkt_indication.m_desc.IsTcp() ) {
        TCPHeader * m_tcp = (TCPHeader *)(p +m_pkt_indication.getFastTcpOffset());
        BP_ASSERT(m_tcp);
        /* replace port */
        if ( port_dir ==  CLIENT_SIDE ) {
            m_tcp->setSourcePort(src_port);
        }else{
            m_tcp->setDestPort(src_port);
        }
    }else {
        if ( m_pkt_indication.m_desc.IsUdp() ){
            UDPHeader * m_udp =(UDPHeader *)(p +m_pkt_indication.getFastTcpOffset() );
            BP_ASSERT(m_udp);
            m_udp->setChecksum(0);
            if ( port_dir ==  CLIENT_SIDE ) {
                m_udp->setSourcePort(src_port);
            }else{
                m_udp->setDestPort(src_port);
            }
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
    m =  CGlobalInfo::pktmbuf_alloc_small(node->get_socket_id());
    assert(m);
    uint16_t len= ( m_packet->pkt_len > FIRST_PKT_SIZE) ?FIRST_PKT_SIZE:m_packet->pkt_len;
    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_pkt_info2(p,flow_info,0,node);

    append_big_mbuf(m,node);

    return(m);
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_ex_big(CGenNode * node,
                                                          CFlowInfo * flow_info){
    rte_mbuf_t        * m;
    uint16_t len =  m_packet->pkt_len;

    /* alloc big buffer to update it*/
    m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), len);
    assert(m);

    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

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
    m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), len);
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
    uint16_t pkt_adjust = vm.m_new_pkt_size - m_packet->pkt_len;
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
    m = CGlobalInfo::pktmbuf_alloc_small(node->get_socket_id());
    assert(m);
    uint16_t len= ( m_packet->pkt_len > FIRST_PKT_SIZE) ?FIRST_PKT_SIZE:m_packet->pkt_len;
    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

    update_pkt_info(p,node);

    append_big_mbuf(m,node);

    return m;
}


inline rte_mbuf_t * CFlowPktInfo::do_generate_new_mbuf_big(CGenNode * node){
    rte_mbuf_t        * m;
    uint16_t len =  m_packet->pkt_len;

    /* alloc big buffer to update it*/
    m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(),  len);
    assert(m);

    /* append*/
    char *p=rte_pktmbuf_append(m, len);

    BP_ASSERT ( (((uintptr_t)m_packet->raw) & 0x7f )== 0) ;

    memcpy(p,m_packet->raw,len);

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
    enum load_cap_file_err {
    kOK = 0,
    kFileNotExist,
    kNegTimestamp,
    kNoSyn,
    kTCPOffsetTooBig,
    kNoTCPFromServer,
    kPktNotSupp,
    kPktProcessFail,
    kCapFileErr
    };

    bool Create();
    void Delete();
    uint64_t Size(void){
        return (m_flow_pkts.size());
    }
    inline CFlowPktInfo * GetPacket(uint32_t index);
    void Append(CPacketIndication * pkt_indication);
    void RemoveAll();
    void dump_pkt_sizes(void);
    enum load_cap_file_err load_cap_file(std::string cap_file, uint16_t _id, uint8_t plugin_id);

    /* update flow info */
    void update_info();

    bool is_valid_template_load_time(std::string & err);

    void save_to_erf(std::string cap_file_name,int pcap);

    inline void generate_flow(CTupleTemplateGeneratorSmart   * tuple_gen,
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

// IPv4 addressing

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

    uint32_t        m_wlength;
    bool            m_wlength_set;

    bool            m_one_app_server;
    bool            m_one_app_server_was_set;
    bool            m_mac_replace_by_ip;

    CVlanYamlInfo   m_vlan_info;
    CTupleGenYamlInfo m_tuple_gen;
    bool              m_tuple_gen_was_set;


    std::vector     <uint8_t> m_mac_base;

    std::vector     <CFlowYamlInfo> m_vec;

    bool            m_is_plugin_configured; /* any plugin  is configured */
public:
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
    double m_total_Mbytes ;
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
public:
    CCapFileFlowInfo m_flow_info;
    CFlowYamlInfo *  m_info;
    CFlowsYamlInfo * m_flows_info;
    CPolicer         m_policer;
    uint16_t         m_id;
private:
    void fixup_ipg_if_needed();
};

class CPPSMeasure {
public:
    CPPSMeasure(){
        reset();
    }
    //reset
    void reset(void){
        m_start=false;
        m_last_time_msec=0;
        m_last_pkts=0;
        m_last_result=0.0;
    }
    //add packet size
    float add(uint64_t pkts);

private:
    float calc_pps(uint32_t dtime_msec,
                                 uint32_t pkts){
        float rate=( (  (float)pkts*(float)os_get_time_freq())/((float)dtime_msec) );
        return (rate);

    }

public:
   bool      m_start;
   uint32_t  m_last_time_msec;
   uint64_t  m_last_pkts;
   float     m_last_result;
};



class CBwMeasure {
public:
    CBwMeasure();
    //reset
    void reset(void);
    //add packet size
    double add(uint64_t size);

private:
    double calc_MBsec(uint32_t dtime_msec,
                     uint64_t dbytes);

public:
   bool      m_start;
   uint32_t  m_last_time_msec;
   uint64_t  m_last_bytes;
   double     m_last_result;
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
class CFlowGenListPerThread {

public:


    friend class CNodeGenerator;
    friend class CPluginCallbackSimple;
    friend class CCapFileFlowInfo;

    typedef  CGenericMap<flow_id_t,CGenNode> flow_id_node_t;

    bool Create(uint32_t           thread_id,
                uint32_t           core_id,
                CFlowGenList  *    flow_list,
                uint32_t           max_threads);
    void Delete();

    void set_terminate_mode(bool is_terminate){
        m_terminated_by_master =is_terminate;
    }
    bool is_terminated_by_master(){
        return (m_terminated_by_master);
    }

    void set_vif(CVirtualIF * v_if){
        m_node_gen.set_vif(v_if);
    }

    void flush_tx_queue() {
        m_node_gen.m_v_if->flush_tx_queue();
    }

    /* return the dual port ID this thread is attached to in 4 ports configuration
       there are 2 dual-ports

      thread 0 - dual 0
      thread 1 - dual 1

      thread 2 - dual 0
      thread 3 - dual 1

     */
    uint32_t getDualPortId();
public :
    double get_total_kcps();
    double get_total_kcps(uint8_t pool_idx, bool is_client);
    double get_delta_flow_is_sec();
    double get_longest_flow();
    double get_longest_flow(uint8_t pool_idx, bool is_client);
    void inc_current_template(void);
    int generate_flows_roundrobin(bool *done);
    int reschedule_flow(CGenNode *node);


    inline CGenNode * create_node(void);

    inline CGenNodeStateless * create_node_sl(void){
        return ((CGenNodeStateless*)create_node() );
    }

    inline CGenNodePCAP * allocate_pcap_node(void) {
        return ((CGenNodePCAP*)create_node());
    }

    inline void free_node(CGenNode *p);
    inline void free_last_flow_node(CGenNode *p);


public:
    void Clean();
    void start_generate_stateful(std::string erf_file_name,CPreviewMode &preview);
    void start_stateless_daemon(CPreviewMode &preview);

    void start_stateless_daemon_simulation();

    /* open a file for simulation */
    void start_stateless_simulation_file(std::string erf_file_name,CPreviewMode &preview, uint64_t limit = 0);
    /* close a file for simulation */
    void stop_stateless_simulation_file();

    /* return true if we need to shedule next_stream,  */
    bool  set_stateless_next_node( CGenNodeStateless * cur_node,
                                   CGenNodeStateless * next_node);

    void stop_stateless_traffic(uint8_t port_id) { 
        m_stateless_dp_info.stop_traffic(port_id, false, 0);
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

private:
    void check_msgs(void);

    void handle_nat_msg(CGenNodeNatInfo * msg);
    void handle_latency_pkt_msg(CGenNodeLatencyPktInfo * msg);

    void terminate_nat_flows(CGenNode *node);


    void init_from_global(CIpPortion &);
    void defer_client_port_free(CGenNode *p);
    void defer_client_port_free(bool is_tcp,uint32_t c_ip,uint16_t port,
                                uint8_t pool_idx, CTupleGeneratorSmart*gen);


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
     FORCE_NO_INLINE void associate(uint32_t fid,CGenNode *     node ){
        assert(m_flow_id_to_node_lookup.lookup(fid)==0);
        m_stats.m_nat_lookup_add_flow_id++;
        m_flow_id_to_node_lookup.add(fid,node);
    }

public:
    uint32_t                         m_thread_id; /* virtual */
    uint32_t                         m_core_id;   /* phsical */

    uint32_t                         m_max_threads;
    CFlowGenList                *    m_flow_list;
    rte_mempool_t *                  m_node_pool;

    std::vector<CFlowGeneratorRecPerThread *> m_cap_gen;

    CFlowsYamlInfo                   m_yaml_info;

    CTupleGeneratorSmart             m_smart_gen;


public:
    CNodeGenerator                   m_node_gen;
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

    TrexStatelessDpCore              m_stateless_dp_info;
    bool                             m_terminated_by_master;

private:
    uint8_t                 m_cacheline_pad[RTE_CACHE_LINE_SIZE][19]; // improve prefech
} __rte_cache_aligned ;

inline CGenNode * CFlowGenListPerThread::create_node(void){
    CGenNode * res;
    if ( unlikely (rte_mempool_sc_get(m_node_pool, (void **)&res) <0) ){
        rte_exit(EXIT_FAILURE, "cant allocate object , need more \n");
        return (0);
    }
    return (res);
}



inline void CFlowGenListPerThread::free_node(CGenNode *p){
    p->free_base();
    rte_mempool_sp_put(m_node_pool, p);
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


class CFlowGenList {

public:
    bool Create();
    void Delete();
    void Clean();
public:
    void generate_p_thread_info(uint32_t num_threads);
    void clean_p_thread_info(void);

public:

    int load_from_yaml(std::string csv_file,uint32_t num_threads);
    int load_from_mac_file(std::string csv_file);
public:
    void Dump(FILE *fd);
    void DumpCsv(FILE *fd);
    void DumpPktSize();
    void UpdateFast();
    double GetCpuUtil();

public:
    double get_total_kcps();
    double get_total_pps();
    double get_total_tx_bps();
    uint32_t get_total_repeat_flows();
    double get_delta_flow_is_sec();
    bool   get_is_mac_conf() { return m_mac_info.is_configured();}
public:
    std::vector<CFlowGeneratorRec *> m_cap_gen;   /* global info */
    CFlowsYamlInfo                   m_yaml_info; /* global yaml*/
    std::vector<CFlowGenListPerThread   *> m_threads_info;
    CFlowGenListMac                  m_mac_info;
};






inline void CCapFileFlowInfo::generate_flow(CTupleTemplateGeneratorSmart   * tuple_gen,
                                            CNodeGenerator * gen,
                                            dsec_t time,
                                            uint64_t flow_id,
                                            CFlowYamlInfo *  template_info,
                                            CGenNode *     node){
    dsec_t c_time = time;

    node->m_type=CGenNode::FLOW_PKT;
    CTupleBase  tuple;
    tuple_gen->GenerateTuple(tuple);

    /* add the first packet of the flow */
    CFlowPktInfo *  lp=GetPacket((uint32_t)0);

    node->set_socket_id(gen->m_socket_id);

    node->m_thread_id = tuple_gen->GetThreadId();
    node->m_flow_id = (flow_id & (0x000fffffffffffffULL)) |
                      ( ((uint64_t)(tuple_gen->GetThreadId()& 0xff)) <<56 ) ;

    node->m_time     = c_time;
    node->m_pkt_info = lp;
    node->m_flow_info = this;
    node->m_flags=0;
    node->m_template_info =template_info;
    node->m_tuple_gen = tuple_gen->get_gen();
    node->m_src_ip= tuple.getClient();
    node->m_dest_ip = tuple.getServer();
    node->m_src_idx = tuple.getClientId();
    node->m_dest_idx = tuple.getServerId();
    node->m_src_port = tuple.getClientPort();
    memcpy(&node->m_src_mac,
           tuple.getClientMac(),
           sizeof(mac_addr_align_t));
    node->m_plugin_info =(void *)0;

    if ( unlikely( CGlobalInfo::is_learn_mode()  ) ){
        // check if flow is two direction
        if ( lp->m_pkt_indication.m_desc.IsBiDirectionalFlow() ) {
            /* we are in learn mode */
            CFlowGenListPerThread  * lpThread=gen->Parent();
            lpThread->associate(((uint32_t)flow_id) & NAT_FLOW_ID_MASK, node);  /* associate flow_id=>node */
            node->set_nat_first_state();
        }
    }

    if ( unlikely(  get_is_rx_check_mode()) ) {
        if  ( (CGlobalInfo::m_options.m_rx_check_sample == 1 ) ||
            ( ( rte_rand() % CGlobalInfo::m_options.m_rx_check_sample ) == 1 )){
           if (unlikely(!node->is_repeat_flow() )) {
               node->set_rx_check();
           }
        }
    }

    if ( unlikely( CGlobalInfo::m_options.preview.getClientServerFlowFlipAddr() ) ){
        node->set_initiator_start_from_server_side_with_server_addr(node->is_eligible_from_server_side());
    }else{
        /* -p */
        if ( likely( CGlobalInfo::m_options.preview.getClientServerFlowFlip() ) ){
            node->set_initiator_start_from_server(node->is_eligible_from_server_side());
            node->set_all_flow_from_same_dir(true);
        }else{
            /* --flip */
            if ( unlikely( CGlobalInfo::m_options.preview.getClientServerFlip() ) ){
                node->set_initiator_start_from_server(node->is_eligible_from_server_side());
            }
        }
    }


    /* in case of plugin we need to call the callback */
    if ( template_info->m_plugin_id ) {
        /* alloc the info , generate the ports */
        on_node_first(template_info->m_plugin_id,node,template_info,tuple_gen,gen->Parent() );
    }

    gen->add_node(node);
}


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

inline void CGenNode::update_next_pkt_in_flow(void){
        if ( likely ( m_pkt_info->m_pkt_indication.m_desc.IsPcapTiming()) ){
            m_time     += m_pkt_info->m_pkt_indication.m_cap_ipg ;
        }else{
            if ( m_pkt_info->m_pkt_indication.m_desc.IsRtt() ){
                m_time     += m_template_info->m_rtt_sec ;
            }else{
                m_time     += m_template_info->m_ipg_sec;
            }
        }

        uint32_t pkt_index   = m_pkt_info->m_pkt_indication.m_packet->pkt_cnt;
        pkt_index++;
        m_pkt_info = m_flow_info->GetPacket((pkt_index-1));
}

inline void CGenNode::reset_pkt_in_flow(void){
        m_pkt_info = m_flow_info->GetPacket(0);
}

inline void CGenNode::replace_tuple(void){
    m_pkt_info->replace_tuple(this);
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



#endif
