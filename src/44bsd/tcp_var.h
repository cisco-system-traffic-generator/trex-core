#ifndef BSD44_BR_TCP_VAR
#define BSD44_BR_TCP_VAR


#include <stdint.h>
#include <stddef.h>
#include "hot_section.h"

#ifdef _DEBUG
#define TCPDEBUG
#endif

#include "tcp.h"
#include "tcp_timer.h"
#include "mbuf.h"
#include "tcp_socket.h"
#include "tcp_fsm.h"
#include "tcp_debug.h"
#include "netinet/tcp_var.h"
#include <vector>
#include "tcp_dpdk.h"
#include "tcp_bsd_utl.h"
#include "flow_table.h"
#include <common/utl_gcc_diag.h>
#include "timer_types.h"
#include <common/n_uniform_prob.h>
#include <stdlib.h>
#include <functional>
#include "sch_rampup.h"
#include "tick_cmd_clock.h"
#include "os_time.h"
#include <algorithm>
#include "tunnels/tunnel_factory.h"
#include <unordered_set>
/*
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)tcp_var.h   8.4 (Berkeley) 5/24/95
 */

/*
 * Kernel variables for tcp.
 */

/*
 * Tcp control block, one per tcp; fields:
 */




class CTcpReass;
class CAstfPerTemplateRW;
class CTcpServerInfo;

struct CTcpPkt : public tcp_pkt {

    inline char * get_header_ptr(){
        return (rte_pktmbuf_mtod(m_buf,char *));
    }

    inline uint32_t get_pkt_size(){
        return (m_buf->pkt_len);
    }

};

#define PACKET_TEMPLATE_SIZE (128)


class CTcpPerThreadCtx;

struct CTcpCb: public tcpcb {

    tcp_socket m_socket;

    /* ====== size 8 bytes  */

    uint8_t mbuf_socket;    /* mbuf socket */
    uint16_t t_pkts_cnt;    /* packets arrived until ack */
    uint16_t m_delay_limit; /* packets limit without ack */
#define TUNE_HAS_PARENT_FLOW         0x01 /* means that this object is part of a bigger object */
#define TUNE_MSS                     0x02
#define TUNE_INIT_WIN                0x04
#define TUNE_NO_DELAY                0x08
#define TUNE_TSO_CACHE               0x10

    uint8_t m_tuneable_flags;
    /*====== end =============*/

#define m_max_tso   t_tsomax

    /*====== end =============*/

    /*====== size 15*4 = 60 bytes  */

    /*====== size 128 + 8 = 132 bytes  */

    CTcpReass * m_tpc_reass; /* tcp reassembley object, allocated only when needed */
    CTcpFlow  * m_flow;      /* back pointer to flow*/
    CTcpPerThreadCtx* m_ctx; /* direct link to the current TCP thread context */
    bool m_reass_disabled;   /* don't reassemble ooo packet, to make payload content in order */

public:

    
    CTcpCb() {
        m_tuneable_flags = 0;
        m_delay_limit    = 0;
        t_pkts_cnt       = 0;
    }

    inline bool is_tso(){
        return( ((m_tuneable_flags & TUNE_TSO_CACHE)==TUNE_TSO_CACHE)?true:false);
    }

    /* in case TSO is enable return the hardware TSO maximum packet size */
    inline uint16_t get_maxseg_tso(bool & tso){
        if (is_tso()) {
            tso=true;
            return(m_max_tso);
        }else{
            return(t_maxseg);
        }
    }


    inline rte_mbuf_t   * pktmbuf_alloc_small(void){
        return (tcp_pktmbuf_alloc_small(mbuf_socket));
    }

    inline rte_mbuf_t   * pktmbuf_alloc(uint16_t size){
        return (tcp_pktmbuf_alloc(mbuf_socket,size));
    }
    
};


/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */
#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))


struct  tcpstat_int_t: public tcpstat {
    uint64_t    tcps_testdrops;     /* connections dropped at the end of the test due to --nc  */

    uint64_t    tcps_rcvoffloads;       /* receive offload packets by software */

    uint64_t    tcps_rcvoopackdrop;     /* OOO packet drop due to queue len */
    uint64_t    tcps_rcvoobytesdrop;     /* OOO bytes drop due to queue len */

    uint64_t    tcps_reasalloc;     /* allocate tcp reasembly object */
    uint64_t    tcps_reasfree;      /* free tcp reasembly object  */
    uint64_t    tcps_reas_hist_4;       /* count of max queue <= 4 */
    uint64_t    tcps_reas_hist_16;      /* count of max queue <= 16 */
    uint64_t    tcps_reas_hist_100;     /* count of max queue <= 100 */
    uint64_t    tcps_reas_hist_other;   /* count of max queue > 100 */

    uint64_t    tcps_nombuf;        /* no mbuf for tcp - drop the packets */
    uint64_t    tcps_notunnel;       /* no GTP Tunnel for tcp - drop the packets */
};

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct  CTcpStats {

    tcpstat_int_t*  m_sts_tg_id;
    tcpstat_int_t   m_sts;
    uint16_t        m_num_of_tg_ids;
public:
    CTcpStats(uint16_t num_of_tg_ids=1);
    ~CTcpStats();
    void Init(uint16_t num_of_tg_ids);
    void Clear();
    void ClearPerTGID(uint16_t tg_id);
    void Dump(FILE *fd);
    void Resize(uint16_t new_num_of_tg_ids);
};


#ifdef TREX_SIM

/*The tick is 50msec , resolotion */

/* only one level (0) is used */
#define TCP_TIMER_LEVEL1_DIV   1


#define TCP_TIMER_TICK_FAST_MS 200 

/* tick is 50 msec */
#define TCP_TIMER_W_TICK       50

#define TCP_TIMER_W_DIV        1

#define TCP_FAST_TICK_ (TCP_TIMER_TICK_FAST_MS/TCP_TIMER_W_TICK)
#define TCP_SLOW_RATIO_TICK 1
#define TCP_SLOW_RATIO_MASTER ((500)/TCP_TIMER_W_TICK)
#define TCP_SLOW_FAST_RATIO_ 1
#define TCP_TIMER_TICK_SLOW_MS TCP_TIMER_TICK_FAST_MS * TCP_SLOW_FAST_RATIO_


/* main tick in sec */
#define TCP_TIME_TICK_SEC ((double)TCP_TIMER_W_TICK/((double)TCP_TIMER_W_DIV* 1000.0))

#define TCP_TIMER_W_1_TICK_US    (TCP_TIMER_W_TICK*1000) 

#else 

/* 2 levels (0,1) of the tw are used */
#define TCP_TIMER_LEVEL1_DIV   16

// base tick in msec

/* base tick of timer in 20usec */
#define TCP_TIMER_W_1_TICK_US     20  

#define TCP_TIMER_W_1_MS (1000/TCP_TIMER_W_1_TICK_US)


/* fast tick msec */
#define TCP_TIMER_TICK_FAST_MS 100

/* flow tick msec */
#define TCP_TIMER_TICK_SLOW_MS 500


/* ticks of FAST */
#define TCP_FAST_TICK_       (TCP_TIMER_TICK_FAST_MS * TCP_TIMER_W_1_MS)

/* how many ticks of TCP_TIMER_W_1_TICK_US */
#define TCP_SLOW_RATIO_MASTER (TCP_TIMER_TICK_SLOW_MS * TCP_TIMER_W_1_MS)

/* how many fast ticks need to get */
#define TCP_SLOW_FAST_RATIO_   (TCP_TIMER_TICK_SLOW_MS/TCP_TIMER_TICK_FAST_MS)

/* one tick that derive the TW in sec  */
#define TCP_TIME_TICK_SEC ((double)TCP_TIMER_W_1_TICK_US/((double)1000000.0))


#endif

static inline uint32_t tw_time_usec_to_ticks(uint32_t usec){
    return (usec/TCP_TIMER_W_1_TICK_US);
}

static inline uint32_t tw_time_msec_to_ticks(uint32_t msec){
    return (tw_time_usec_to_ticks(1000*msec));
}

static inline uint32_t convert_slow_sec_to_ticks(uint16_t sec) {
    float sec_to_ticks = 1000.0f / (TCP_SLOW_FAST_RATIO_ * TCP_TIMER_TICK_FAST_MS);
    return uint32_t(round(float(sec) * sec_to_ticks));
}

#include "h_timer.h"

class CPerProfileCtx;
class CTcpPerThreadCtx ;
class CAstfDbRO;
class CTcpTuneables;

/* build packet template */

class CFlowTemplate {

public:
    void set_tuple(uint32_t src,
                   uint32_t dst,
                   uint16_t src_port,
                   uint16_t dst_port,
                   tunnel_cfg_data_t* tunnel_data,
                   uint8_t  proto,
                   void    *tunnel_ctx,
                   bool     is_ipv6){
        m_tunnel_data = tunnel_data;
        m_src_ipv4 = src;
        m_dst_ipv4 = dst;
        m_src_port = src_port;
        m_dst_port = dst_port;
        m_is_ipv6  = is_ipv6;
        m_proto    = proto;
        m_tunnel_ctx = tunnel_ctx;
    }

    void server_update_mac(uint8_t *pkt, CPerProfileCtx * pctx, tvpid_t port_id);

    void learn_ipv6_headers_from_network(IPv6Header * net_ipv6);

    void build_template(CPerProfileCtx * pctx,uint16_t  template_idx);

    void set_offload_mask(uint8_t flags){
        m_offload_flags=flags;
    }
    bool is_ipv6(){
        return((m_is_ipv6?true:false));
    }

    inline bool is_tcp_tso();

    uint32_t get_src_ipv4() const {
        return(m_src_ipv4);
    }
    uint32_t get_dst_ipv4() const {
        return(m_dst_ipv4);
    }

    uint16_t get_src_port() const {
        return(m_src_port);
    }
    uint16_t get_dst_port() const {
        return(m_dst_port);
    }

    inline bool is_tunnel_aware(){
        return (m_tunnel_ctx != NULL);
    }

    inline bool is_tunnel(){
        return CGlobalInfo::m_options.m_tunnel_enabled;
    }
private:
    /* support either UDP or TCP for now */
    bool is_tcp(){
        if (m_proto==IPHeader::Protocol::TCP) {
            return(true);
        }else{
            assert(m_proto==IPHeader::Protocol::UDP);
            return(false);
        }
    }

    void build_template_ip(CPerProfileCtx * pctx,
                           uint16_t  template_idx);
    void build_template_tcp(CPerProfileCtx * pctx);
    void build_template_udp(CPerProfileCtx * pctx);

public:
    /* cache line 0 */

    uint32_t  m_src_ipv4;
    uint32_t  m_dst_ipv4;
    uint16_t  m_src_port;
    uint16_t  m_dst_port;
    tunnel_cfg_data_t* m_tunnel_data;
    uint16_t  m_l4_pseudo_checksum;
    void     *m_tunnel_ctx;
    uint8_t   m_offset_l4; /* offset of tcp_header, in template */
    uint8_t   m_offset_ip;  /* offset of ip_header in template */
    uint8_t   m_is_ipv6;
    uint8_t   m_proto;      /* 6 - TCP ,0x11- UDP */

    uint8_t   m_offload_flags;
    #define OFFLOAD_TX_CHKSUM   0x0001      /* DPDK_CHECK_SUM */
    #define TCP_OFFLOAD_TSO     0x0002      /* DPDK_TSO_CHECK_SUM */
    #define OFFLOAD_RX_CHKSUM   0x0004      /* check RX checksum L4*/

    uint8_t  m_template_pkt[PACKET_TEMPLATE_SIZE];   /* template packet */
};

inline bool CFlowTemplate::is_tcp_tso(){
   return( ((m_offload_flags & TCP_OFFLOAD_TSO)==TCP_OFFLOAD_TSO)?true:false);
}


class CFlowBase {

public:
    void Create(CPerProfileCtx *pctx, uint16_t tg_id=0);
    void Delete();

    void set_tuple(uint32_t src,
                   uint32_t dst,
                   uint16_t src_port,
                   uint16_t dst_port,
                   tunnel_cfg_data_t* tunnel_data,
                   uint8_t  proto,
                   bool is_ipv6){
        m_template.set_tuple(src,dst,src_port,dst_port,tunnel_data,proto,NULL,is_ipv6);
    }

    static CFlowBase * cast_from_hash_obj(flow_hash_ent_t *p){
        UNSAFE_CONTAINER_OF_PUSH
        CFlowBase * lp2 =(CFlowBase *)((uint8_t*)p-offsetof (CFlowBase,m_hash));
        UNSAFE_CONTAINER_OF_POP
        return (lp2);
    }

    bool is_udp(){
        return(m_template.m_proto==IPHeader::Protocol::UDP);
    }

    void init();

    HOT_FUNC void check_defer_function(){
        check_defer_functions(&m_app);
    }


    void learn_ipv6_headers_from_network(IPv6Header * net_ipv6);

    void set_tuple_generator(uint32_t          c_idx,
                             uint16_t          c_pool_idx,
                             uint16_t          c_template_idx,
                             bool enable){
        m_c_idx_enable = enable;
        m_c_idx = c_idx;
        m_c_pool_idx = c_pool_idx;
        m_c_template_idx = c_template_idx;
    }

    void get_tuple_generator(uint32_t   &       c_idx,
                             uint16_t   &       c_pool_idx,
                             uint16_t   &       c_template_id,
                             bool & enable){
        enable =  m_c_idx_enable;
        c_idx = m_c_idx;
        c_pool_idx = m_c_pool_idx;
        c_template_id = m_c_template_idx;
    }

    void set_tunnel_ctx();

    void setup_base_flow(CFlowBase* flow) {
        if (flow) {
            assert(flow->m_exec_flow == nullptr);
            flow->m_exec_flow = this;
            m_base_flow = flow;
        }
    }

    void resume_base_flow() {
        if (m_base_flow) {
            assert(m_base_flow->m_exec_flow == this);
            m_base_flow->m_app.resume_by(&m_app);
            m_base_flow->m_exec_flow = nullptr;
            m_base_flow = nullptr;
        }
    }

    void clear_exec_flow() {
        if (m_exec_flow) {
            assert(m_exec_flow->m_base_flow == this);
            m_exec_flow->m_base_flow = nullptr;
            m_exec_flow = nullptr;
        }
    }

public:
    uint8_t              m_c_idx_enable;
    uint8_t              m_pad[3];
    uint32_t             m_c_idx;
    uint16_t             m_c_pool_idx;
    uint16_t             m_c_template_idx;
    uint16_t             m_tg_id;
   
    CPerProfileCtx      *m_pctx;
    flow_hash_ent_t      m_hash;  /* hash object - 64bit  */
    CEmulApp             m_app;   
    CFlowTemplate        m_template;  /* 128+32 bytes */

    CFlowBase           *m_base_flow;  /* execution base flow */
    CFlowBase           *m_exec_flow;  /* execution flow link */
};


/*****************************************************/
/* UDP */
struct  udp_stat_int_t {
    uint64_t    udps_sndbyte;  
    uint64_t    udps_sndpkt; 
    uint64_t    udps_rcvbyte;  
    uint64_t    udps_rcvpkt;

    uint64_t    udps_accepts;       /* connections accepted */
    uint64_t    udps_connects;      /* connections established */
    uint64_t    udps_closed;        /* conn. closed (includes drops) */

    uint64_t    udps_keepdrops;     
    uint64_t    udps_nombuf;        /* no mbuf for udp - drop the packets */
    uint64_t    udps_pkt_toobig;    /* pkt too big */
    
    uint64_t    udps_notunnel;      /* No GTPU tunnel */

};

struct  CUdpStats {

    udp_stat_int_t* m_sts_tg_id;
    udp_stat_int_t  m_sts;
    uint16_t        m_num_of_tg_ids;
public:
    CUdpStats(uint16_t num_of_tg_ids=1);
    ~CUdpStats();
    void Init(uint16_t num_of_tg_ids);
    void Clear();
    void ClearPerTGID(uint16_t tg_id);
    void Dump(FILE *fd);
    void Resize(uint16_t new_num_of_tg_ids);
};


#define INC_UDP_STAT(ctx, tg_id, p) {ctx->m_udpstat.m_sts.p++; ctx->m_udpstat.m_sts_tg_id[tg_id].p++; }
#define INC_UDP_STAT_CNT(ctx, tg_id, p, cnt) {ctx->m_udpstat.m_sts.p += cnt; ctx->m_udpstat.m_sts_tg_id[tg_id].p += cnt; }


class CUdpFlow : public CFlowBase {

public:
    void Create(CPerProfileCtx *pctx, bool client, uint16_t tg_id=0);
    void Delete();

    static CUdpFlow * cast_from_hash_obj(flow_hash_ent_t *p){
        return((CUdpFlow *)CFlowBase::cast_from_hash_obj(p));
    }

    void init();

    void send_pkt(CMbufBuffer * buf);

    void disconnect();

    void set_keepalive(uint64_t msec, bool rx_mode);

    void on_tick();

    bool is_can_closed(){
        return(m_closed);
    }

    void on_rx_packet(struct rte_mbuf *m,
                      UDPHeader *udp,
                      int offset_l7,
                      int total_l7_len);


public:
    void set_c_udp_info(const CAstfPerTemplateRW *rw_db, uint16_t temp_id){
    }

    void set_s_udp_info(const CAstfDbRO * ro_db, CTcpTuneables *tune){
    }

private:

    rte_mbuf_t   *    alloc_and_build(CMbufBuffer *      buf);

    void update_checksum_and_lenght(CFlowTemplate *ftp,
                                    rte_mbuf_t * m,
                                    uint16_t     udp_pyld_bytes,
                                    char *pkt);

    inline rte_mbuf_t   * pktmbuf_alloc_small(void){
        return (tcp_pktmbuf_alloc_small(m_mbuf_socket));
    }

    inline rte_mbuf_t   * pktmbuf_alloc(uint16_t size){
        return (tcp_pktmbuf_alloc(m_mbuf_socket,size));
    }

    void keepalive_timer_start(bool init);

public:
    CHTimerObj        m_keep_alive_timer; /* 32 bytes */ 
    bool              m_keepalive_rx_mode; /* keepalive by rx only */
    uint32_t          m_keepalive_ticks;
    uint8_t           m_mbuf_socket;    /* mbuf socket */
    uint8_t           m_keepalive;      /* 1- no-alive 0- alive */
    uint32_t          m_remain_ticks;
    bool              m_closed;

};

/*****************************************************/

class CServerTemplateInfo;
class CServerIpPayloadInfo;

class CTcpFlow : public CFlowBase {

public:
    void Create(CPerProfileCtx *pctx, uint16_t tg_id = 0);
#ifdef  TREX_SIM
    void Create(CTcpPerThreadCtx *ctx, uint16_t tg_id = 0);
#endif
    void Delete();

    void init();

public:

    static CTcpFlow * cast_from_hash_obj(flow_hash_ent_t *p){
        return((CTcpFlow *)CFlowBase::cast_from_hash_obj(p));
    }

    void on_slow_tick();

    void on_fast_tick();

    inline void on_tick();


    HOT_FUNC bool is_can_close() const {
        return (m_tcp.t_state == TCPS_CLOSED ?true:false);
    }

    bool is_close_was_called() const {
        return ((m_tcp.t_state > TCPS_CLOSE_WAIT) ?true:false);
    }

    void set_app(CEmulApp  * app){
        m_tcp.m_socket.m_app = app;
    }

    void set_c_tcp_info(const CAstfPerTemplateRW *rw_db, uint16_t temp_id);

    void set_s_tcp_info(const CAstfDbRO * ro_db, CTcpTuneables *tune);

    CPerProfileCtx* create_on_flow_profile();
    void start_identifying_template(CPerProfileCtx *pctx, CServerIpPayloadInfo* payload_info);
    bool new_template_identified();
    bool is_activated();

    bool check_template_assoc_by_l7_data(uint8_t* l7_data, uint16_t l7_len);
    void update_new_template_assoc_info();

    const std::string get_flow_id() const;
    void to_json(Json::Value& obj) const;

public:
    CHTimerObj        m_timer; /* 32 bytes */ 
    uint32_t          m_timer_ticks; /* timer ticks */
    CTcpCb            m_tcp;
    uint8_t           m_tick;

    CServerTemplateInfo*    m_template_info; /* to save the identified template */
    CServerIpPayloadInfo*   m_payload_info; /* to manage template during identifying */

    CTcpRxOffloadBuf* m_lro_buf;
};

/* general timer object used by ASTF, 
  speed does not matter here, so we allocate it dynamically. 
  Callbacks and opaque data inside it */

class CAstfTimerObj;
typedef void (*astf_on_gen_tick_cb_t)(void *userdata0,
                                      void *userdata1,
                                      CAstfTimerObj *tmr);

class CAstfTimerObj : public CHTimerObj  {

public:
    CAstfTimerObj(){
        CHTimerObj::reset();
        m_type=ttGen;  
        m_cb=0;
        m_userdata1=0;
    }

public:
    astf_on_gen_tick_cb_t m_cb;
    void *                m_userdata1;
};

class CAstfTimerFunctorObj;

typedef std::function<void(CAstfTimerFunctorObj *tmr)> method_timer_cb_t;

/* callback for object method  
   WATCH OUT : this object is super not efficient due to the c++ standard std::function. 
   It is very big ~80 bytes and slow to allocate however, it gives a good generic interface for object methods. Use it only in none performance oriented features.

*/
class CAstfTimerFunctorObj : public CHTimerObj  {

public:
    CAstfTimerFunctorObj(){
        CHTimerObj::reset();
        m_type=ttGenFunctor;  
        m_cb=0;
        m_userdata1=0;
    }

public:
    method_timer_cb_t   m_cb;  
    void *              m_userdata1;
};




class CTcpCtxCb {
public:
    virtual ~CTcpCtxCb(){
    }
    virtual int on_tx(CTcpPerThreadCtx *ctx,
                      struct CTcpCb * tp,
                      rte_mbuf_t *m)=0;

    virtual int on_flow_close(CTcpPerThreadCtx *ctx,
                              CFlowBase * flow)=0;

    virtual int on_redirect_rx(CTcpPerThreadCtx *ctx,
                               rte_mbuf_t *m)=0;


};

class CAstfDbRO;
class CAstfTemplatesRW;
class CTcpTuneables;
class CGenNode;

typedef void (*on_stopped_cb_t)(void *data, profile_id_t profile_id);

class CTcpTunableCtx: public tcp_tune {
public:
    /* TUNABLEs */
    uint32_t  tcp_tx_socket_bsize;
    uint32_t  tcp_rx_socket_bsize;

#define tcp_initwnd_factor  tcp_initcwnd_segments
    uint32_t  sb_max ; /* socket max char */
    int tcp_max_tso;   /* max tso default */
    int tcp_rttdflt;
    int tcp_no_delay;
    int tcp_no_delay_counter; /* number of recv bytes to wait until ack them */
    int tcp_blackhole;
    int tcp_maxidle;            /* time to drop after starting probes */
    int tcp_maxpersistidle;
    int tcp_ttl;            /* time to live for TCP segs */
    int tcp_reass_maxqlen;  /* maximum number of TCP segments per reassembly queue */

    uint8_t use_inbound_mac;    /* whether to use MACs from incoming pkts */

    int tcp_cc_algo;    /* select CC algorithm (0: newreno, 1: cubic) */

public:
    CTcpTunableCtx();

    inline int convert_slow_sec_to_ticks(uint16_t sec);
    void update_tuneables(CTcpTuneables *tune);
};

class CPerProfileCtx {
public:
    profile_id_t        m_profile_id;

    CTcpPerThreadCtx  * m_ctx;

    CAstfFifRampup    * m_sch_rampup; /* rampup for CPS */
    double              m_fif_d_time;

    CAstfTemplatesRW  * m_template_rw;
    CAstfDbRO         * m_template_ro;

    struct CTcpStats    m_tcpstat; /* tcp statistics */
    struct CUdpStats    m_udpstat; /* udp statistics */
    struct CAppStats    m_appstat; /* app statistics */

    uint32_t            m_stop_id;

    int                 m_flow_cnt; /* active flow count */

    on_stopped_cb_t     m_on_stopped_cb;
    void              * m_cb_data;

    union {
        void          * m_tx_node;  /* to make CGenNodeTXFIF stop safe */
        void          * m_flow_dump;
    };

    CTcpTunableCtx      m_tunable_ctx;

private:
    bool                m_stopping;
    bool                m_stopped;  /* reported to CP */
    bool                m_nc_flow_close;

    bool                m_on_flow; /* dedicated to a flow */

    hr_time_t           m_base_time; /* first flow connected time */
    hr_time_t           m_last_time; /* last flow closed time */

public:
    ~CPerProfileCtx() {
        if (m_sch_rampup != nullptr) {
            delete m_sch_rampup;
            m_sch_rampup = nullptr;
        }
    }
    void activate() { m_stopping = false; m_stopped = false; }
    void deactivate() { m_stopping = true; }
    bool is_active() { return m_stopping == false; }
    void set_stopped() { m_stopped = true; }
    bool is_stopped() { return m_stopped == true; }

    void set_nc(bool nc) { m_nc_flow_close = nc; }
    bool get_nc() { return m_nc_flow_close; }

    bool is_open_flow_allowed() { return is_active()/* || (!get_nc() && !is_stopped())*/; }

    void on_flow_close() {
        if (m_flow_cnt == 0 && !is_active()) {
            if (m_on_stopped_cb) {
                m_on_stopped_cb(m_cb_data, m_profile_id);
            }
        }
    }
    void set_on_flow() { m_on_flow = true; }
    bool is_on_flow() { return m_on_flow; }
    void update_profile_stats(CPerProfileCtx* pctx);
    void update_tg_id_stats(uint16_t tg_id, CPerProfileCtx* pctx, uint16_t in_tg_id);

    void update_tuneables(CTcpTuneables *tune) { m_tunable_ctx.update_tuneables(tune); }
    void copy_tunable_values(CPerProfileCtx *pctx) { m_tunable_ctx = pctx->m_tunable_ctx; }

    void set_time_connects() {
        if (m_base_time == 0) {
            m_base_time = os_get_hr_tick_64();
        }
    }
    hr_time_t get_time_connects() { return m_base_time; }
    void update_time_connects(hr_time_t new_time) {
        if (m_base_time == 0) {
            m_base_time = new_time;
        }
    }
    void set_time_closed() { m_last_time = os_get_hr_tick_64(); }
    double get_time_lap() {
        auto last_time = m_last_time;
        if (m_flow_cnt) {   // ongoing traffic exists
            last_time = os_get_hr_tick_64();
        }
        return m_base_time ? ptime_convert_hr_dsec(last_time - m_base_time): 0.0;
    }

private:
    CTcpFlow* m_tcp_flow;

public:
    void set_flow_info(CTcpFlow* flow) {
        if (m_tcp_flow == nullptr) {
            m_tcp_flow = flow;
        }
    }
    void clear_flow_info(CTcpFlow* flow) {
        if (m_tcp_flow == flow) {
            m_tcp_flow = nullptr;
        }
    }
    void dump_flow_info(Json::Value& result) {
        if (m_tcp_flow) {
            std::string flow_id = m_tcp_flow->get_flow_id();
            m_tcp_flow->to_json(result[flow_id]);
        }
    }
};


class CServerTemplateInfo {
    CTcpServerInfo* m_server_info;
    CPerProfileCtx* m_pctx;

public:
    CServerTemplateInfo() {}
    CServerTemplateInfo(CTcpServerInfo* info, CPerProfileCtx* pctx) : m_server_info(info), m_pctx(pctx) {}

    CTcpServerInfo* get_server_info() const { return m_server_info; }
    CPerProfileCtx* get_profile_ctx() const { return m_pctx; }

    bool has_payload_params();
};

class CServerIpInfo {
protected:
    uint32_t m_ip_start;
    uint32_t m_ip_end;

public:
    CServerIpInfo() {}
    CServerIpInfo(uint32_t start, uint32_t end) : m_ip_start(start), m_ip_end(end) {}

    uint32_t ip_start() const { return m_ip_start; }
    uint32_t ip_end() const { return m_ip_end; }
    uint32_t ip_range() const { return m_ip_end - m_ip_start; }

    bool is_overlap(uint32_t start, uint32_t end) const {
        return (end >= m_ip_start && start <= m_ip_end);
    }
    bool is_in(uint32_t ip) const { return is_overlap(ip, ip); }
    bool is_equal(uint32_t start, uint32_t end) const {
        return (start == m_ip_start && end == m_ip_end);
    }
};

class CServerIpTemplateInfo : CServerIpInfo {
    CServerTemplateInfo m_template;

public:
    CServerIpTemplateInfo() {}
    CServerIpTemplateInfo(uint32_t start, uint32_t end, CServerTemplateInfo& temp) : CServerIpInfo(start, end), m_template(temp) {}

    CServerTemplateInfo* get_template_info() { return &m_template; }

    uint32_t ip_start() const { return m_ip_start; }
    uint32_t ip_end() const { return m_ip_end; }

    bool is_ip_overlap(uint32_t start, uint32_t end) { return is_overlap(start, end); }
    bool is_ip_overlap(CServerIpTemplateInfo& in) { return is_overlap(in.ip_start(), in.ip_end()); }
    bool is_ip_in(uint32_t ip) { return is_overlap(ip, ip); }

    std::string to_string() {
        std::stringstream ss;
        ss << std::hex << "  ip[" << m_ip_start << ", " << m_ip_end << "]";
        ss << ", server=" << m_template.get_server_info() << ", pctx=" << m_template.get_profile_ctx();
        return ss.str();
    }
};

struct PayloadRule {
    uint8_t m_mask;
    uint8_t m_offset;

    PayloadRule(uint8_t mask, uint8_t offset) : m_mask(mask), m_offset(offset) {}
    PayloadRule(uint16_t rule) { m_mask = rule >> 8; m_offset = rule & UINT8_MAX; }
    operator uint16_t() { return ((uint16_t)m_mask << 8) | m_offset; }

    bool operator==(const PayloadRule& rhs) { return m_mask == rhs.m_mask && m_offset == rhs.m_offset; }
    bool operator<(const PayloadRule& rhs) { return m_mask != rhs.m_mask ? m_mask < rhs.m_mask : m_offset < rhs.m_offset; }
};

typedef std::vector<uint16_t> payload_rule_t;
typedef uint64_t payload_value_t;

class CServerIpPayloadInfo : CServerIpInfo {
    payload_rule_t m_payload_rule;
    std::unordered_map<payload_value_t,CServerTemplateInfo> m_payload_map;

    CServerTemplateInfo* m_template_ref;    // IP only template >> latest payload template
    std::set<CTcpFlow*> m_template_flows;   // flows to update reference template info

public:
    CServerIpPayloadInfo() {}
    CServerIpPayloadInfo(uint32_t start, uint32_t end, CServerTemplateInfo& temp);

    uint32_t ip_start() const { return m_ip_start; }
    uint32_t ip_end() const { return m_ip_end; }

    bool is_ip_overlap(uint32_t start, uint32_t end) { return is_overlap(start, end); }
    bool is_ip_overlap(CServerIpPayloadInfo& in) { return is_overlap(in.ip_start(), in.ip_end()); }
    bool is_ip_equal(CServerIpPayloadInfo& in) { return is_equal(in.ip_start(), in.ip_end()); }
    bool is_ip_in(uint32_t ip) { return is_overlap(ip, ip); }

    bool is_rule_equal(const payload_rule_t& rule) { return m_payload_rule == rule; }
    bool is_rule_equal(CServerIpPayloadInfo& in) { return is_rule_equal(in.m_payload_rule); }
    bool is_payload_rule() { return !m_payload_rule.empty(); }
    bool is_payload_enough(uint16_t len) {
        PayloadRule rule = m_payload_rule.back(); // offset value is ascending order
        return len > rule.m_offset;
    }
    bool is_server_compatible(CTcpServerInfo* server);

    payload_value_t get_payload_value(uint8_t* data) {
        payload_value_t pval = 0;
        for (auto& it: m_payload_rule) {
            PayloadRule rule{ it };
            pval <<= 8;
            pval |= data[rule.m_offset] & rule.m_mask;
        }
        return pval;
    }

    CServerTemplateInfo* get_template_info(payload_value_t pval) {
        auto& map = m_payload_map;
        return (map.find(pval) != map.end()) ? &map[pval]: nullptr;
    }
    CServerTemplateInfo* get_template_info(uint8_t* data) {
        return data ? get_template_info(get_payload_value(data)): nullptr;
    }
    void set_reference_template_info(CServerTemplateInfo* temp) { m_template_ref = temp; }
    CServerTemplateInfo* get_reference_template_info();

    bool insert_template_info(payload_value_t pval, CTcpServerInfo* server, CPerProfileCtx* pctx) {
        if (!get_template_info(pval)) {
            m_payload_map.insert(std::make_pair(pval, CServerTemplateInfo(server, pctx)));
            m_template_ref = &m_payload_map[pval];  // updated by recently added one
            return true;
        }
        return false;
    }

    bool insert_template_info(const CServerIpPayloadInfo& in) {
        assert(is_rule_equal(in.m_payload_rule));
        assert(in.m_payload_map.size() == 1);
        auto it = in.m_payload_map.begin();
        CTcpServerInfo* server = it->second.get_server_info();
        CPerProfileCtx* pctx = it->second.get_profile_ctx();
        return insert_template_info(it->first, server, pctx);
    }

    bool remove_template_info(CPerProfileCtx* pctx);

    void insert_template_flow(CTcpFlow* flow) {
        m_template_flows.insert(flow);
    }
    void remove_template_flow(CTcpFlow* flow) {
        auto it = m_template_flows.find(flow);
        if (it != m_template_flows.end()) {
            m_template_flows.erase(it);
        }
    }
    void update_template_flows(CPerProfileCtx* pctx);

    std::string to_string() {
        std::stringstream ss;
        ss << std::hex << "  ip[" << m_ip_start << ", " << m_ip_end << "]";
        for (auto it: m_payload_rule) {
            PayloadRule rule{ it };
            ss << " " << unsigned(rule.m_offset) << "/" << unsigned(rule.m_mask);
        }
        for (auto it: m_payload_map) {
            ss << std::endl << "    ";
            ss << "- " << it.first << "(server=" << it.second.get_server_info() << ",pctx=" << it.second.get_profile_ctx() << ")";
        }
        return ss.str();
    }
};

struct PortParams {
    bool m_stream;
    uint16_t m_port;

    PortParams(uint16_t port, bool stream) : m_stream(stream), m_port(port) {}
    PortParams(uint32_t params) { m_stream = params >> 16; m_port = params & UINT16_MAX; }
    operator uint32_t() { return ((uint32_t)m_stream << 16) | m_port; }

    bool operator==(const PortParams& rhs) { return m_stream == rhs.m_stream && m_port == rhs.m_port; }
    bool operator<(const PortParams& rhs) { return m_stream != rhs.m_stream ? m_stream < rhs.m_stream : m_port < rhs.m_port; }
};

class CServerPortInfo {
    std::map<uint32_t,CServerIpTemplateInfo> m_ip_map;    // key is m_ip_end
    std::map<uint32_t,CServerIpPayloadInfo> m_ip_map_payload;
    CServerTemplateInfo *m_template_cache;    // all ip range's template cache for the fast lookup.

    void update_payload_template_reference(CServerTemplateInfo* temp);
    bool insert_template_payload(CTcpServerInfo* info, CPerProfileCtx* pctx, std::string& msg);
    CServerTemplateInfo* get_template_info_by_ip(uint32_t ip);
    CServerTemplateInfo* get_template_info_by_payload(uint32_t ip, uint8_t* data, uint16_t len);
public:
    bool is_empty() { return m_ip_map.empty() && m_ip_map_payload.empty(); }

    bool insert_template_info(CTcpServerInfo* info, CPerProfileCtx* pctx, std::string& msg);
    void remove_template_info_by_profile(CPerProfileCtx* pctx);

    CServerIpPayloadInfo* get_ip_payload_info(uint32_t ip);

    CServerTemplateInfo* get_template_info(uint32_t ip, uint8_t* data, uint16_t len);
    CServerTemplateInfo* get_template_info(uint32_t ip) {
        return m_template_cache ? m_template_cache : get_template_info_by_ip(ip);
    }
    CServerTemplateInfo* get_template_info() { return m_template_cache; }

    void print_server_port();
};


class CFlowGenListPerThread;

class CTcpPerThreadCtx {
public:
    bool Create(uint32_t size,
                bool is_client);
    void Delete();

    /* called after init */
    void call_startup(profile_id_t profile_id = 0);

    /* cleanup m_timer_w from left flows */
    void cleanup_flows();

public:
    RC_HTW_t timer_w_start(CTcpFlow * flow){
        return (m_timer_w.timer_start(&flow->m_timer,flow->m_timer_ticks));
    }

    void handle_udp_timer(CUdpFlow * flow){
        if (flow->is_can_closed()) {
          m_ft.handle_close(this,flow,true);
        } else if (!flow->m_pctx->is_active() && flow->m_pctx->get_nc()) {
          m_ft.terminate_flow(this,flow,true);
        }
    }

    RC_HTW_t timer_w_restart(CTcpFlow * flow){
        if (flow->is_can_close()) {
            /* free the flow in case it was finished */
            m_ft.handle_close(this,flow,true);
        } else if (!flow->m_pctx->is_active() && flow->m_pctx->get_nc()) {
            /* terminate the flow in case --nc specified */
            m_ft.terminate_flow(this,flow,true);
        }else{
            return (m_timer_w.timer_start(&flow->m_timer,flow->m_timer_ticks));
        }
        return(RC_HTW_OK);
    }


    RC_HTW_t timer_w_stop(CTcpFlow * flow){
        return (m_timer_w.timer_stop(&flow->m_timer));
    }

    void timer_w_on_tick();
    bool timer_w_any_events(){
        return(m_timer_w.is_any_events_left());
    }

    void set_cb(CTcpCtxCb    * cb){
        m_cb=cb;
    }
    CTcpCtxCb *get_cb() {return m_cb;}

    void set_thread(CFlowGenListPerThread* thread){ m_thread=thread; }
    CFlowGenListPerThread *get_thread() const { return m_thread; }

    void set_memory_socket(uint8_t socket){
        m_mbuf_socket= socket;
    }

    void set_offload_dev_flags(uint8_t flags){
        m_offload_flags=flags;
    }

    bool get_rx_checksum_check(){
        return( ((m_offload_flags & OFFLOAD_RX_CHKSUM) == OFFLOAD_RX_CHKSUM)?true:false);
    }

    /*  this function is called every 20usec to see if we have an issue with resource */
    void maintain_resouce();

    bool is_open_flow_enabled();

    bool is_client_side(void) {
        return (m_ft.is_client_side());
    }

    void resize_stats(profile_id_t profile_id = 0);

private:
    void delete_startup(profile_id_t profile_id);

    void init_sch_rampup(profile_id_t profile_id);

public:
    //struct    inpcb tcb;      /* head of queue of active tcpcb's */
    uint32_t    tcp_now;        /* for RFC 1323 timestamps */
    tcp_seq     tcp_iss;            /* tcp initial send seq # */
    CAstfTickCmdClock *m_tick_var;   /* clock for tick var commands */
    uint32_t    m_tick;
    uint8_t     m_mbuf_socket;      /* memory socket */
    uint8_t     m_offload_flags;    /* dev offload flags, see flow def */
    uint8_t     m_disable_new_flow;
    uint8_t     m_pad;

    CFlowGenListPerThread * m_thread; /* back pointer */
    KxuLCRand         *  m_rand; /* per context */
    CTcpCtxCb         *  m_cb;
    CNATimerWheel        m_timer_w; /* TBD-FIXME one timer , should be pointer */

    CFlowTable           m_ft;
    struct  tcpiphdr tcp_saveti;
    CTunnelHandler   *m_tunnel_handler;

    /* server port management */
private:
    std::unordered_map<uint32_t,CServerPortInfo> m_server_ports;
    std::unordered_set<uint64_t>                 m_ignored_macs;
    std::unordered_set<uint32_t>                 m_ignored_ips;
public:
    void append_server_ports(profile_id_t profile_id);
    void remove_server_ports(profile_id_t profile_id);
    CServerTemplateInfo * get_template_info_by_port(uint16_t port, bool stream);
    CServerTemplateInfo * get_template_info(uint16_t port, bool stream, uint32_t ip, uint8_t* data=nullptr, uint16_t len=0);
    CServerTemplateInfo * get_template_info(uint16_t port, bool stream, uint32_t ip, CServerIpPayloadInfo** payload_info_p);
    CTcpServerInfo * get_server_info(uint16_t port, bool stream, uint32_t ip, uint8_t* data=nullptr, uint16_t len=0);
    void print_server_ports();

    /* profile management */
private:
    std::unordered_map<profile_id_t, CPerProfileCtx*> m_profiles;
    std::unordered_map<profile_id_t, CPerProfileCtx*> m_active_profiles;

public:
    bool is_profile_ctx(profile_id_t profile_id) { return m_profiles.find(profile_id) != m_profiles.end(); }
    bool is_any_profile(void) { return (m_active_profiles.size() != 0); }
#define FALLBACK_PROFILE_CTX(ctx)   ((ctx)->get_first_profile_ctx())
#define DEFAULT_PROFILE_CTX(ctx)    ((ctx)->get_profile_ctx(0))
    CPerProfileCtx* get_profile_ctx(profile_id_t profile_id) {
#ifdef TREX_SIM
        if (profile_id == 0 && !is_profile_ctx(0)) {
            create_profile_ctx(0);  // default profile need to be static in simulator.
            append_active_profile(0);
        }
#endif
        return m_profiles.at(profile_id);
    }
    CPerProfileCtx* get_first_profile_ctx() {
        assert(m_active_profiles.size() != 0);
        return m_active_profiles.begin()->second;
    }
    void create_profile_ctx(profile_id_t profile_id) {
        CPerProfileCtx* pctx;

        if (is_profile_ctx(profile_id)) {
            pctx = m_profiles[profile_id];
            pctx->~CPerProfileCtx();
            new(pctx) CPerProfileCtx();
        }
        else {
            pctx = new CPerProfileCtx();
            m_profiles[profile_id] = pctx;
        }
        pctx->m_ctx = this;
        pctx->m_profile_id = profile_id;
    }
    void remove_profile_ctx(profile_id_t profile_id) {
        if (is_profile_ctx(profile_id)) {
            delete m_profiles[profile_id];
            m_profiles.erase(profile_id);
        }
    }

    void append_active_profile(profile_id_t profile_id) {
        if (is_profile_ctx(profile_id)) {
            m_active_profiles[profile_id] = m_profiles.at(profile_id);
        }
    }

    void remove_active_profile(profile_id_t profile_id) {
        if (is_profile_ctx(profile_id)) {
            m_active_profiles.erase(profile_id);
        }
    }

    void insert_ignored_macs(std::vector<uint64_t>& ignored_macs) {
        m_ignored_macs.clear();
        m_ignored_macs.insert(ignored_macs.begin(), ignored_macs.end());
        uint8_t mask = 1;
        if (m_ignored_macs.size()) {
            m_ft.m_black_list |= mask;
        } else{
            m_ft.m_black_list &= ~mask;
        }
    }

    bool mac_lookup(uint64_t mac) {
        return (m_ignored_macs.find(mac) != m_ignored_macs.end());
    }

    bool is_ignored_macs_empty() {
        return (m_ignored_macs.size() == 0);
    }

    void insert_ignored_ips(std::vector<uint32_t>& ignored_ips) {
        m_ignored_ips.clear();
        m_ignored_ips.insert(ignored_ips.begin(), ignored_ips.end());
        uint8_t mask = 2;
        if (m_ignored_ips.size()) {
            m_ft.m_black_list |= mask;
        } else {
            m_ft.m_black_list &= ~mask;
        }
    }

    bool ip_lookup(uint32_t ip) {
        return (m_ignored_ips.find(ip) != m_ignored_ips.end());
    }

    bool is_ignored_ips_empty() {
        return (m_ignored_ips.size() == 0);
    }

    /* per profile context access by profile_id */
public:
    int profile_flow_cnt(profile_id_t profile_id) { return get_profile_ctx(profile_id)->m_flow_cnt; }

    void set_stop_id(profile_id_t profile_id, uint32_t stop_id) {
        get_profile_ctx(profile_id)->m_stop_id = stop_id;
    }
    int get_stop_id(profile_id_t profile_id) {
        return get_profile_ctx(profile_id)->m_stop_id;
    }

    void activate_profile_ctx(profile_id_t profile_id) { get_profile_ctx(profile_id)->activate(); }
    void deactivate_profile_ctx(profile_id_t profile_id) { get_profile_ctx(profile_id)->deactivate(); }

    void set_profile_nc(profile_id_t profile_id, bool nc) { get_profile_ctx(profile_id)->set_nc(nc); }
    bool get_profile_nc(profile_id_t profile_id) { return get_profile_ctx(profile_id)->get_nc(); }

    void set_profile_cb(profile_id_t profile_id, void *cb_data, on_stopped_cb_t cb) {
        CPerProfileCtx *pctx = get_profile_ctx(profile_id);
        pctx->m_on_stopped_cb = cb;
        pctx->m_cb_data = cb_data;
    }

public:
    CAstfFifRampup* get_sch_rampup(profile_id_t profile_id) { return get_profile_ctx(profile_id)->m_sch_rampup; }
    void set_sch_rampup(CAstfFifRampup* t, profile_id_t profile_id) { get_profile_ctx(profile_id)->m_sch_rampup = t; }
    double get_fif_d_time(profile_id_t profile_id) { return get_profile_ctx(profile_id)->m_fif_d_time; }
    void set_fif_d_time(double t, profile_id_t profile_id) { get_profile_ctx(profile_id)->m_fif_d_time = t; }

    CAstfDbRO* get_template_ro(profile_id_t profile_id) { return get_profile_ctx(profile_id)->m_template_ro; }
    CAstfTemplatesRW* get_template_rw(profile_id_t profile_id) { return get_profile_ctx(profile_id)->m_template_rw; }
    void set_template_ro(CAstfDbRO* t, profile_id_t profile_id=0) { get_profile_ctx(profile_id)->m_template_ro = t; }
    void set_template_rw(CAstfTemplatesRW* t, profile_id_t profile_id) { get_profile_ctx(profile_id)->m_template_rw = t; }

    struct CTcpStats* get_tcpstat(profile_id_t profile_id=0) { return &get_profile_ctx(profile_id)->m_tcpstat; }
    struct CUdpStats* get_udpstat(profile_id_t profile_id=0) { return &get_profile_ctx(profile_id)->m_udpstat; }
    struct CAppStats* get_appstat(profile_id_t profile_id=0) { return &get_profile_ctx(profile_id)->m_appstat; }
};


class TrexAstfDpCore;

class CPerProfileFlowDump {
    double m_interval;
    CPerProfileCtx* m_pctx;
    TrexAstfDpCore* m_dp_core;

    CTcpPerThreadCtx* m_ctx;
    CAstfTimerFunctorObj* m_tmr;
public:
    CPerProfileFlowDump(double interval, CPerProfileCtx* pctx, TrexAstfDpCore* dp_core);
    ~CPerProfileFlowDump();

    void on_timer_update(CAstfTimerFunctorObj* tmr);
};


class CTcpSeqKey {
    uint32_t m_seq;

public:
    CTcpSeqKey(uint32_t seq): m_seq(seq) {}

    bool operator<(const CTcpSeqKey& rhs) const {
        return int(m_seq - rhs.m_seq) < 0;
    }
};


class CTcpReassBlock {

public:
  void Dump(FILE *fd);
  bool operator==(CTcpReassBlock& rhs)const;

  uint32_t m_seq;  
  uint32_t m_len;  /* scale option require 32bit maximum adv window */
  uint32_t m_flags; /* only 1 bit is needed, for align*/
};

typedef std::vector<CTcpReassBlock> vec_tcp_reas_t;


#define MAX_TCP_REASS_BLOCKS (4)

class CTcpReass {

public:
    CTcpReass(uint16_t size): m_max_size(size), m_max_used(0) {}

    int pre_tcp_reass(CPerProfileCtx * pctx,
                      struct CTcpCb *tp,
                      struct tcpiphdr *ti,
                      struct rte_mbuf *m);

    int tcp_reass_no_data(CPerProfileCtx * pctx,
                          struct CTcpCb *tp);

    int tcp_reass(CPerProfileCtx * pctx,
                  struct CTcpCb *tp,
                  struct tcpiphdr *ti,
                  struct rte_mbuf *m);

#ifdef  TREX_SIM
    CTcpReass(): CTcpReass(MAX_TCP_REASS_BLOCKS) {}

    int pre_tcp_reass(CTcpPerThreadCtx * ctx,
                      struct CTcpCb *tp,
                      struct tcpiphdr *ti,
                      struct rte_mbuf *m) { return pre_tcp_reass(DEFAULT_PROFILE_CTX(ctx), tp, ti, m); }

    int tcp_reass_no_data(CTcpPerThreadCtx * ctx,
                          struct CTcpCb *tp) { return tcp_reass_no_data(DEFAULT_PROFILE_CTX(ctx), tp); }

    int tcp_reass(CTcpPerThreadCtx * ctx,
                  struct CTcpCb *tp,
                  struct tcpiphdr *ti,
                  struct rte_mbuf *m) { return tcp_reass(DEFAULT_PROFILE_CTX(ctx), tp, ti, m); }
#endif

    inline uint16_t get_active_blocks(void){
        return (uint16_t)m_blocks.size();
    }

    inline uint16_t get_max_blocks(void) {
        return m_max_used;
    }

    void Dump(FILE *fd);

    bool expect(vec_tcp_reas_t & lpkts,FILE * fd);

private:
    std::map<CTcpSeqKey,CTcpReassBlock> m_blocks;
    uint16_t m_max_size;
    uint16_t m_max_used;
};


#define INC_STAT(ctx, tg_id, p) {ctx->m_tcpstat.m_sts.p++; ctx->m_tcpstat.m_sts_tg_id[tg_id].p++; }
#define INC_STAT_CNT(ctx, tg_id, p, cnt) {ctx->m_tcpstat.m_sts.p += cnt; ctx->m_tcpstat.m_sts_tg_id[tg_id].p += cnt; }


CTcpTuneables * tcp_get_parent_tunable(CPerProfileCtx * pctx,struct CTcpCb *tp);

int tcp_reass(CPerProfileCtx * pctx,
              struct CTcpCb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m);
#ifdef  TREX_SIM
int tcp_reass(CTcpPerThreadCtx * ctx,
              struct CTcpCb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m);
#endif


#if 0
const char ** tcp_get_tcpstate();


int tcp_build_cpkt(CPerProfileCtx * pctx,
                   struct CTcpCb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt);

int tcp_build_dpkt(CPerProfileCtx * pctx,
                   struct CTcpCb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t  tcphlen,
                   CTcpPkt &pkt);
#endif


inline bool tcp_reass_is_exists(struct CTcpCb *tp){
    return (tp->m_tpc_reass != ((CTcpReass *)0));
}

inline void tcp_reass_alloc(CPerProfileCtx * pctx,
                            struct CTcpCb *tp){
    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reasalloc);

    CTcpTunableCtx * tctx = &pctx->m_tunable_ctx;
    int maxqlen = tctx->tcp_reass_maxqlen;
    if (tp->t_maxseg) {
        maxqlen = std::min(int(tp->m_socket.so_rcv.sb_hiwat/tp->t_maxseg) + 1, maxqlen);
    }
    tp->m_tpc_reass = new CTcpReass(maxqlen);
}

inline void tcp_reass_free(CPerProfileCtx * pctx,
                            struct CTcpCb *tp){
    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reasfree);
    uint16_t max_blocks = tp->m_tpc_reass->get_max_blocks();
    if (max_blocks <= 4) {
        INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reas_hist_4);
    } else if (max_blocks <= 16) {
        INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reas_hist_16);
    } else if (max_blocks <= 100) {
        INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reas_hist_100);
    } else {
        INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reas_hist_other);
    }
    delete tp->m_tpc_reass;
    tp->m_tpc_reass=(CTcpReass *)0;
}

inline void tcp_reass_clean(CPerProfileCtx * pctx,
                            struct CTcpCb *tp){
    if (tcp_reass_is_exists(tp) ){
        tcp_reass_free(pctx,tp);
    }
}

extern struct CTcpCb *debug_flow; 


inline void tcp_set_debug_flow(struct CTcpCb * debug){
#ifdef    TCPDEBUG
    debug_flow  =debug;
#endif
}



class CEmulAppApiImpl : public CEmulAppApi {
public:

    /* get maximum tx queue space */
    virtual uint32_t get_tx_max_space(CTcpFlow * flow){
        return (flow->m_tcp.m_socket.so_snd.sb_hiwat);
    }

    /* get space in bytes in the tx queue */
    virtual uint32_t get_tx_sbspace(CTcpFlow * flow){
        return (((CTcpSockBuf*)&flow->m_tcp.m_socket.so_snd)->get_sbspace());
    }

    /* add bytes to tx queue */
    virtual void tx_sbappend(CTcpFlow * flow,uint32_t bytes){
        INC_STAT_CNT(flow->m_pctx, flow->m_tg_id, tcps_sndbyte,bytes);
        ((CTcpSockBuf*)&flow->m_tcp.m_socket.so_snd)->sbappend(bytes);
    }

    virtual uint32_t  rx_drain(CTcpFlow * flow){
        /* drain bytes from the rx queue */
        uint32_t res=flow->m_tcp.m_socket.so_rcv.sb_cc;
        flow->m_tcp.m_socket.so_rcv.sb_cc=0;
        return(res);
    }


    virtual void tx_tcp_output(CPerProfileCtx * pctx,CTcpFlow *         flow){
        tcp_int_output(&flow->m_tcp);
    }

    virtual void disconnect(CPerProfileCtx * pctx,
                            CTcpFlow *         flow){
        tcp_disconnect(&flow->m_tcp);
    }

public:
    /* UDP API not implemented  */
    virtual void send_pkt(CUdpFlow *         flow,
                          CMbufBuffer *      buf){
        assert(0);
    }

    virtual void set_keepalive(CUdpFlow *         flow,
                               uint64_t           msec,
                               bool               rx_mode){
        assert(0);
    }
};



class CEmulAppApiUdpImpl : public CEmulAppApi {
public:

    /* get maximum tx queue space */
    virtual uint32_t get_tx_max_space(CTcpFlow * flow){
        assert(0);
    }

    /* get space in bytes in the tx queue */
    virtual uint32_t get_tx_sbspace(CTcpFlow * flow){
        assert(0);
        return(0);
    }

    /* add bytes to tx queue */
    virtual void tx_sbappend(CTcpFlow * flow,uint32_t bytes){
        assert(0);
    }

    virtual uint32_t  rx_drain(CTcpFlow * flow){
        assert(0);
        return(0);
    }


    virtual void tx_tcp_output(CPerProfileCtx * pctx,CTcpFlow *         flow){
        assert(0);
    }

public:
    virtual void disconnect(CPerProfileCtx * pctx,
                            CTcpFlow *         flow){
        CUdpFlow *         uflow=(CUdpFlow *)flow;
        uflow->disconnect();

        /* free here */
    }

    /* UDP API not implemented  */
    virtual void send_pkt(CUdpFlow *         flow,
                          CMbufBuffer *      buf){
        flow->send_pkt(buf);
    }

    virtual void set_keepalive(CUdpFlow *         flow,
                               uint64_t           msec,
                               bool               rx_mode){
        flow->set_keepalive(msec, rx_mode);
    }
};



inline void CTcpFlow::on_tick(){
    m_tick++;
    tcp_handle_timers(&m_tcp);
}




#endif
