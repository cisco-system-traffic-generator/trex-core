#ifndef TREX_GLOBAL_H
#define TREX_GLOBAL_H
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
#include "mbuf.h"
#include <common/bitMan.h>
#include "platform_cfg.h"
#include "utl_json.h"
#include <json/json.h>
#include "pal_utl.h"
#include "trex_platform.h"



#define CONST_NB_MBUF  16380

/* this is the first small part of the packet that we manipulate */
#define _FIRST_PKT_SIZE 64
#define CONST_SMALL_MBUF_SIZE (_FIRST_PKT_SIZE + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

#define _64_MBUF_SIZE 64
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


#define TW_BUCKETS       (CGlobalInfo::m_options.get_tw_buckets())
#define TW_BUCKETS_LEVEL1_DIV (16)
#define TW_LEVELS        (CGlobalInfo::m_options.get_tw_levels())
#define BUCKET_TIME_SEC (CGlobalInfo::m_options.get_tw_bucket_time_in_sec())
#define BUCKET_TIME_SEC_LEVEL1 (CGlobalInfo::m_options.get_tw_bucket_level1_time_in_sec())
#define TCP_RX_FLUSH_SEC  (20.0/1000000.0)

class CGenNode;

class CPreviewMode {
public:
    enum {
        VLAN_MODE_NONE = 0,
        VLAN_MODE_NORMAL = 1,
        VLAN_MODE_LOAD_BALANCE = 2,
    };

    CPreviewMode(){
        clean();
    }
    void clean(){
        m_flags = 0;
        m_flags1=0;
        setCores(1);
        set_vlan_mode(VLAN_MODE_NONE);
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

    void set_disable_hw_flow_stat(bool enable) {
        btSetMaskBit32(m_flags, 3, 3, enable ? 1 : 0);
    }

    bool get_disable_hw_flow_stat() {
        return (btGetMaskBit32(m_flags, 3, 3) ? true:false);
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

    void set_pcap_mode_enable(bool enable){
        btSetMaskBit32(m_flags,26,26,enable?1:0);
    }

    bool get_pcap_mode_enable(){
        return (btGetMaskBit32(m_flags,26,26) ? true:false);
    }

    void set_zmq_publish_enable(bool enable){
        btSetMaskBit32(m_flags,27,27,enable?1:0);
    }

    bool get_zmq_publish_enable(){
        return (btGetMaskBit32(m_flags,27,27) ? true:false);
    }

    uint8_t get_vlan_mode() {
        return (btGetMaskBit32(m_flags, 29, 28));
    }

    void set_vlan_mode(uint8_t mode) {
        btSetMaskBit32(m_flags, 29, 28, mode);
    }

    void set_vlan_mode_verify(uint8_t mode);
    bool get_mac_ip_overide_enable(){
        return (btGetMaskBit32(m_flags,30,30) ? true:false);
    }

    void set_mac_ip_overide_enable(bool enable){
        btSetMaskBit32(m_flags,30,30,enable?1:0);
        if (enable) {
            set_slowpath_features_on(enable);
        }
    }

    bool get_is_rx_check_enable(){
        return (btGetMaskBit32(m_flags,31,31) ? true:false);
    }

    void set_rx_check_enable(bool enable){
        btSetMaskBit32(m_flags,31,31,enable?1:0);
    }

    bool get_is_slowpath_features_on() {
        return (btGetMaskBit32(m_flags1, 0, 0) ? true : false);
    }

    void set_slowpath_features_on(bool enable) {
        btSetMaskBit32(m_flags1, 0, 0, enable ? 1 : 0);
    }

    bool get_is_client_cfg_enable() {
        return (btGetMaskBit32(m_flags1, 1, 1) ? true : false);
    }

    void set_client_cfg_enable(bool enable){
        btSetMaskBit32(m_flags1, 1, 1, enable ? 1 : 0);
        if (enable) {
            set_slowpath_features_on(enable);
        }
    }

    // m_flags1 - bit 2 is free
    void set_no_keyboard(bool enable){
        btSetMaskBit32(m_flags1,5,5,enable?1:0);
    }

    bool get_no_keyboard(){
        return (btGetMaskBit32(m_flags1,5,5) ? true:false);
    }

    /* -e */
    void setClientServerFlowFlipAddr(bool enable){
        btSetMaskBit32(m_flags1,3,3,enable?1:0);
    }

    bool getClientServerFlowFlipAddr(){
        return (btGetMaskBit32(m_flags1,3,3) ? true:false);
    }

    /* split mac is enabled */
    void setWDDisable(bool wd_disable){
        btSetMaskBit32(m_flags1,6,6,wd_disable?1:0);
    }

    bool getWDDisable(){
        return (btGetMaskBit32(m_flags1,6,6) ? true:false);
    }

    void setCoreDumpEnable(bool enable) {
        btSetMaskBit32(m_flags1, 7, 7, (enable ? 1 : 0) );
    }

    bool getCoreDumpEnable(){
        return (btGetMaskBit32(m_flags1, 7, 7) ? true : false);
    }

    void setChecksumOffloadEnable(bool enable) {
        btSetMaskBit32(m_flags1, 8, 8, (enable ? 1 : 0) );
    }

    bool getChecksumOffloadEnable(){
        return (btGetMaskBit32(m_flags1, 8, 8) ? true : false);
    }

    void setCloseEnable(bool enable) {
        btSetMaskBit32(m_flags1, 9, 9, (enable ? 1 : 0) );
    }

    bool getCloseEnable(){
        return (btGetMaskBit32(m_flags1, 9, 9) ? true : false);
    }

    void set_rt_prio_mode(bool enable) {
        btSetMaskBit32(m_flags1, 10, 10, (enable ? 1 : 0) );
    }

    bool get_rt_prio_mode() {
        return (btGetMaskBit32(m_flags1, 10, 10) ? true : false);
    }

    void set_mlx5_so_mode(bool enable) {
        btSetMaskBit32(m_flags1, 11, 11, (enable ? 1 : 0) );
    }

    bool get_mlx5_so_mode() {
        return (btGetMaskBit32(m_flags1, 11, 11) ? true : false);
    }

    void set_mlx4_so_mode(bool enable) {
        btSetMaskBit32(m_flags1, 12, 12, (enable ? 1 : 0) );
    }

    bool get_mlx4_so_mode() {
        return (btGetMaskBit32(m_flags1, 12, 12) ? true : false);
    }

    void setChecksumOffloadDisable(bool enable) {
        btSetMaskBit32(m_flags1, 13, 13, (enable ? 1 : 0) );
    }

    bool getChecksumOffloadDisable(){
        return (btGetMaskBit32(m_flags1, 13, 13) ? true : false);
    }

    void set_dev_tso_support(bool enable) {
        btSetMaskBit32(m_flags1, 15, 15, (enable ? 1 : 0) );
    }

    bool get_dev_tso_support() {
        return (btGetMaskBit32(m_flags1, 15, 15) ? true : false);
    }

    void setTsoOffloadDisable(bool enable){
        btSetMaskBit32(m_flags1, 16, 16, (enable ? 1 : 0) );
    }

    bool getTsoOffloadDisable() {
        return (btGetMaskBit32(m_flags1, 16, 16) ? true : false);
    }

    void set_ntacc_so_mode(bool enable) {
        btSetMaskBit32(m_flags1, 17, 17, (enable ? 1 : 0) );
    }

    bool get_ntacc_so_mode() {
        return (btGetMaskBit32(m_flags1, 17, 17) ? true : false);
    }


    void set_termio_disabled(bool enable) {
        btSetMaskBit32(m_flags1, 18, 18, (enable ? 1 : 0) );
    }

    bool get_is_termio_disabled() {
        return (btGetMaskBit32(m_flags1, 18, 18) ? true : false);
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
        uint8_t is_set;
        uint8_t pad[3];
} mac_align_t  ;

struct CMacAddrCfg {
public:
    CMacAddrCfg () {
        reset();
    }
    void reset () {
        memset(u.m_data, 0, sizeof(u.m_data));
        u.m_mac.dest[3] = 1;
        u.m_mac.is_set = 0;
    }
    union {
        mac_align_t m_mac;
        uint8_t     m_data[16];
    } u;
} __rte_cache_aligned; ;

class CPerPortIPCfg {
 public:
    uint32_t get_ip() {return m_ip;}
    uint32_t get_mask() {return m_mask;}
    uint32_t get_def_gw() {return m_def_gw;}
    uint32_t get_vlan() {return m_vlan;}
    void set_ip(uint32_t val) {m_ip = val;}
    void set_mask(uint32_t val) {m_mask = val;}
    void set_def_gw(uint32_t val) {m_def_gw = val;}
    void set_vlan(uint16_t val) {m_vlan = val;}

 private:
    uint32_t m_def_gw;
    uint32_t m_ip;
    uint32_t m_mask;
    uint16_t m_vlan;
};


class CParserOption {

public:
    /* Runtime flags */
    enum {
        RUN_FLAGS_RXCHECK_CONST_TS =1,
    };

    /** 
     * operation mode 
     * stateful, stateless or advacned stateful
     */
    enum trex_op_mode_e {
        OP_MODE_INVALID,
        OP_MODE_STF,
        OP_MODE_STL,
        OP_MODE_ASTF,
        OP_MODE_ASTF_BATCH,
        
        OP_MODE_DUMP_INTERFACES,
    };

    enum trex_astf_mode_e {
        OP_ASTF_MODE_NONE       =0,
        OP_ASTF_MODE_SERVR_ONLY =1,
        OP_ASTF_MODE_CLIENT_MASK=2
    };

    enum trex_learn_mode_e {
    LEARN_MODE_DISABLED=0,
    LEARN_MODE_TCP_ACK=1,
    LEARN_MODE_IP_OPTION=2,
    LEARN_MODE_TCP_ACK_NO_SERVER_SEQ_RAND=3,
    LEARN_MODE_MAX=LEARN_MODE_TCP_ACK_NO_SERVER_SEQ_RAND,
    // This is used to check if 1 or 3 exist
    LEARN_MODE_TCP=100
    };

public:

    void reset() {
        preview.clean();
        m_tw_buckets = 1024;
        m_tw_levels = 3;
        m_active_flows = 0;
        m_factor = 1.0;
        m_mbuf_factor = 1.0;
        m_duration = 0.0;
        m_platform_factor = 1.0;
        m_vlan_port[0] = 100;
        m_vlan_port[1] = 100;
        memset(m_src_ipv6, 0, sizeof(m_src_ipv6));
        memset(m_dst_ipv6, 0, sizeof(m_dst_ipv6));
        memset(m_ip_cfg, 0, sizeof(m_ip_cfg));
        m_latency_rate = 0;
        m_latency_mask = 0xffffffff;
        m_latency_prev = 0;
        m_rx_check_sample = 0;
        m_rx_check_hops = 0;
        m_wait_before_traffic = 1;
        m_zmq_port = 4500;
        m_telnet_port = 4501;
        m_expected_portd = 4; /* should be at least the number of ports found in the system but could be less */
        m_io_mode = 1;
        m_run_flags = 0;
        m_l_pkt_mode = 0;
        m_learn_mode = 0;
        m_debug_pkt_proto = 0;
        m_arp_ref_per = 120; // in seconds
        m_rx_thread_enabled = false;
        m_op_mode = OP_MODE_INVALID;
        cfg_file = "";
        client_cfg_file = "";
        platform_cfg_file = "";
        out_file = "";
        prefix = "";
        set_tw_bucket_time_in_usec(20.0);
        // we read every 0.5 second. We want to catch the counter when it approach the maximum (where it will stuck,
        // and we will start losing packets).
        x710_fdir_reset_threshold = 0xffffffff - 1000000000/8/64*40;
        m_astf_mode =OP_ASTF_MODE_NONE;
        m_astf_client_mask=0;
    }

    CParserOption(){
        reset();
    }

    CPreviewMode    preview;
    uint16_t        m_tw_buckets;
    uint16_t        m_tw_levels;
    uint32_t        m_active_flows;
    float           m_factor;
    float           m_mbuf_factor;
    float           m_duration;
    float           m_platform_factor;
    uint16_t		m_vlan_port[2]; /* vlan value */
    uint16_t		m_src_ipv6[6];  /* Most signficant 96-bits */
    uint16_t		m_dst_ipv6[6];  /* Most signficant 96-bits */
    CPerPortIPCfg   m_ip_cfg[TREX_MAX_PORTS];
    uint32_t        m_latency_rate; /* pkt/sec for each thread/port zero disable */
    uint32_t        m_latency_mask;
    uint32_t        m_latency_prev;
    uint32_t        m_wait_before_traffic;
    uint16_t        m_rx_check_sample; /* the sample rate of flows */
    uint16_t        m_rx_check_hops;
    uint16_t        m_zmq_port;
    uint16_t        m_telnet_port;
    uint16_t        m_expected_portd;
    uint16_t        m_io_mode; //0,1,2 0 disable, 1- normal , 2 - short
    uint16_t        m_run_flags;
    uint8_t         m_l_pkt_mode;
    uint8_t         m_learn_mode;
    uint16_t        m_debug_pkt_proto;
    uint16_t        m_arp_ref_per;
    bool            m_rx_thread_enabled;
    trex_op_mode_e  m_op_mode;
    trex_astf_mode_e m_astf_mode;
    uint32_t         m_astf_client_mask;

    
    std::string        cfg_file;
    std::string        astf_cfg_file;
    std::string        client_cfg_file;
    std::string        platform_cfg_file;
    std::string        out_file;
    std::string        prefix;
    std::vector<std::string> dump_interfaces;
    CMacAddrCfg     m_mac_addr[TREX_MAX_PORTS];
    double          m_tw_bucket_time_sec;
    double          m_tw_bucket_time_sec_level1;
    uint32_t        x710_fdir_reset_threshold;


public:
    uint8_t *       get_src_mac_addr(int if_index){
        return (m_mac_addr[if_index].u.m_mac.src);
    }
    uint8_t *       get_dst_src_mac_addr(int if_index){
        return (m_mac_addr[if_index].u.m_mac.dest);
    }

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
    bool is_latency_enabled() {
        return ( (m_latency_rate == 0) ? false : true);
    }
    bool is_rx_enabled() {
        return m_rx_thread_enabled;
    }
    void set_rx_enabled() {
        m_rx_thread_enabled = true;
    }
    uint32_t get_x710_fdir_reset_threshold() {
        return (x710_fdir_reset_threshold);
    }
    void set_x710_fdir_reset_threshold(uint32_t val) {
        x710_fdir_reset_threshold = val;
    }

    inline double get_tw_bucket_time_in_sec(void){
        return (m_tw_bucket_time_sec);
    }

    inline double get_tw_bucket_level1_time_in_sec(void){
        return (m_tw_bucket_time_sec_level1);
    }

    void set_tw_bucket_time_in_usec(double usec){
        m_tw_bucket_time_sec= (usec/1000000.0);
        m_tw_bucket_time_sec_level1 = (m_tw_bucket_time_sec*(double)m_tw_buckets)/((double)TW_BUCKETS_LEVEL1_DIV);
    }

    void     set_tw_buckets(uint16_t buckets){
        m_tw_buckets=buckets;
    }

    inline uint16_t get_tw_buckets(void){
        return (m_tw_buckets);
    }

    void     set_tw_levels(uint16_t levels){
        m_tw_levels=levels;
    }

    inline uint16_t get_tw_levels(void){
        return (m_tw_levels);
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

    void verify();
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


class CRteMemPool {

public:
    inline rte_mbuf_t   * _rte_pktmbuf_alloc(rte_mempool_t * mp ){
        rte_mbuf_t   * m=rte_pktmbuf_alloc(mp);
        if ( likely(m>0) ) {
            return (m);
        }
        // hack for failure when using lots of 4k mbufs. See TRex-393
        // real solution should be to always use 2K mbufs, and concatenate if needed
        if (mp == m_mbuf_pool_4096) {
            return _rte_pktmbuf_alloc(m_mbuf_pool_9k);
        }
        dump_in_case_of_error(stderr, mp);
        assert(0);
    }

    inline rte_mempool_t * pktmbuf_get_pool(uint16_t size){

        rte_mempool_t * p;

        if ( size <= _128_MBUF_SIZE) {
            p = m_mbuf_pool_128;
        }else if ( size <= _256_MBUF_SIZE) {
            p = m_mbuf_pool_256;
        }else if (size <= _512_MBUF_SIZE) {
            p = m_mbuf_pool_512;
        }else if (size <= _1024_MBUF_SIZE) {
            p = m_mbuf_pool_1024;
        }else if (size <= _2048_MBUF_SIZE) {
            p = m_mbuf_pool_2048;
        }else if (size <= _4096_MBUF_SIZE) {
            p = m_mbuf_pool_4096;
        }else{
            assert(size<=MAX_PKT_ALIGN_BUF_9K);
            p = m_mbuf_pool_9k;
        }
        return (p);
    }


    inline rte_mbuf_t   * pktmbuf_alloc(uint16_t size){

        rte_mbuf_t        * m;
        if ( size <= _128_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_128);
        }else if ( size <= _256_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_256);
        }else if (size <= _512_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_512);
        }else if (size <= _1024_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_1024);
        }else if (size <= _2048_MBUF_SIZE) {
            m = _rte_pktmbuf_alloc(m_mbuf_pool_2048);
        }else if (size <= _4096_MBUF_SIZE) {
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

    bool dump_one(FILE *fd, const char *name, rte_mempool_t *pool);
    void dump(FILE *fd);

    void dump_in_case_of_error(FILE *fd, rte_mempool_t * mp);

    void dump_as_json(Json::Value &json);

private:
    void add_to_json(Json::Value &json, std::string name, rte_mempool_t * pool);

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
    typedef enum {
        Q_MODE_NORMAL,
        Q_MODE_ONE_QUEUE, // One RX queue and one TX queue
        Q_MODE_RSS,
        Q_MODE_MANY_DROP_Q // For Mellanox
    } queues_mode;


    static void init_pools(uint32_t rx_buffers, uint32_t rx_pool);
    /* for simulation */
    static void free_pools();

    static inline rte_mbuf_t   * pktmbuf_alloc_small(socket_id_t socket){
        return ( m_mem_pool[socket].pktmbuf_alloc_small() );
    }

    static inline rte_mbuf_t * pktmbuf_alloc_small_by_port(uint8_t port_id) {
        return ( m_mem_pool[m_socket.port_to_socket(port_id)].pktmbuf_alloc_small() );
    }

    static inline rte_mempool_t * pktmbuf_get_pool(socket_id_t socket,uint16_t size){
        return (m_mem_pool[socket].pktmbuf_get_pool(size));
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
        if (size<=_64_MBUF_SIZE) {
            return ( pktmbuf_alloc_small(socket));
        }
        return (m_mem_pool[socket].pktmbuf_alloc(size));
    }

    
    static inline rte_mbuf_t   * pktmbuf_alloc_small_local(socket_id_t socket){
        rte_mbuf_t *m = pktmbuf_alloc_small(socket);
        if (m) {
            rte_mbuf_set_as_core_local(m);
        }
        return m;
    }
    
      
    static inline rte_mbuf_t   * pktmbuf_alloc_local(socket_id_t socket,uint16_t size) {
        rte_mbuf_t *m = pktmbuf_alloc(socket, size);
        if (m) {
            rte_mbuf_set_as_core_local(m);
        }
        return m;
    }
    
    
    static inline rte_mbuf_t * pktmbuf_alloc_by_port(uint8_t port_id, uint16_t size){
        socket_id_t socket = m_socket.port_to_socket(port_id);
        if (size<=_64_MBUF_SIZE) {
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
        if (mode == CParserOption::LEARN_MODE_TCP) {
            return ((m_options.m_learn_mode == CParserOption::LEARN_MODE_TCP_ACK_NO_SERVER_SEQ_RAND)
                    || (m_options.m_learn_mode == CParserOption::LEARN_MODE_TCP_ACK));
        } else
            return (m_options.m_learn_mode == mode);
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


    static void dump_pool_as_json(Json::Value &json);
    static std::string dump_pool_as_json_str(void);
    static inline int get_queues_mode() {
        return m_q_mode;
    }
    static inline void set_queues_mode(queues_mode mode) {
        m_q_mode = mode;
    }

public:
    static CRteMemPool       m_mem_pool[MAX_SOCKETS_SUPPORTED];
    static uint32_t              m_nodes_pool_size;
    static CParserOption         m_options;
    static CGlobalMemory         m_memory_cfg;
    static CPlatformSocketInfo   m_socket;
    static queues_mode           m_q_mode;
};


static inline CParserOption::trex_op_mode_e get_op_mode() {
    return CGlobalInfo::m_options.m_op_mode;
}

static inline int get_is_stateless(){
    return (get_op_mode() == CParserOption::OP_MODE_STL);
}

static inline int get_is_tcp_mode(){
    return ( (get_op_mode() == CParserOption::OP_MODE_ASTF) || (get_op_mode() == CParserOption::OP_MODE_ASTF_BATCH) );
}

static inline int get_is_interactive(){
    return ( (get_op_mode() == CParserOption::OP_MODE_STL) || (get_op_mode() == CParserOption::OP_MODE_ASTF) );
}


static inline int get_is_rx_check_mode(){
    return (CGlobalInfo::m_options.preview.get_is_rx_check_enable() ?1:0);
}

static inline bool get_is_rx_filter_enable(){
    uint32_t latency_rate=CGlobalInfo::m_options.m_latency_rate;
    return ( ( get_is_rx_check_mode() || CGlobalInfo::is_learn_mode() || latency_rate != 0
               || get_is_stateless()) &&  ((CGlobalInfo::get_queues_mode() != CGlobalInfo::Q_MODE_RSS)
                                           && (CGlobalInfo::get_queues_mode() != CGlobalInfo::Q_MODE_ONE_QUEUE))
             ?true:false );
}
static inline uint16_t get_rx_check_hops() {
    return (CGlobalInfo::m_options.m_rx_check_hops);
}


#endif
