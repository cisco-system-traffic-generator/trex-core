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
//#ifndef TREX_FBSD
#include "tcp_fsm.h"
#include "tcp_debug.h"
//#endif
#ifdef TREX_FBSD
#define EXTEND_TCPCB
#include "netinet/tcp_int.h"
#endif /* TREX_FBSD */
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

struct CTcpPkt {
    rte_mbuf_t   * m_buf;
    TCPHeader    * lpTcp;
    uint16_t       m_optlen;

    inline char * get_header_ptr(){
        return (rte_pktmbuf_mtod(m_buf,char *));
    }

    inline uint32_t get_pkt_size(){
        return (m_buf->pkt_len);
    }

};

#define PACKET_TEMPLATE_SIZE (128)


#ifndef TREX_FBSD
struct tcpcb {
#else
struct tcpcb: public tcpcb_base {
#endif

    tcp_socket m_socket;

    /* ====== size 8 bytes  */

#ifndef TREX_FBSD
    uint16_t t_timer[TCPT_NTIMERS];  /* tcp timers */
    char    t_force;        /* 1 if forcing out a byte */
#endif
    uint8_t mbuf_socket;    /* mbuf socket */
#ifndef TREX_FBSD
    uint8_t t_dupacks;      /* consecutive dup acks recd */
#endif
    uint16_t t_pkts_cnt;    /* packets arrived until ack */
    uint16_t m_delay_limit; /* packets limit without ack */
#define TUNE_HAS_PARENT_FLOW         0x01 /* means that this object is part of a bigger object */
#define TUNE_MSS                     0x02
#define TUNE_INIT_WIN                0x04
#define TUNE_NO_DELAY                0x08
#define TUNE_TSO_CACHE               0x10

    uint8_t m_tuneable_flags;
    /*====== end =============*/

#ifndef TREX_FBSD
    /* ======= size 12 bytes  */

    int16_t t_state;        /* state of this connection */
    int16_t t_rxtshift;     /* log(2) of rexmt exp. backoff */
    int16_t t_rxtcur;       /* current retransmit value */
    u_short t_maxseg;       /* maximum segment size */
    u_short t_flags;
#define TF_ACKNOW   0x0001      /* ack peer immediately */
#define TF_DELACK   0x0002      /* ack, but try to delay it */
#define TF_NODELAY  0x0004      /* don't delay packets to coalesce */
#define TF_NOOPT    0x0008      /* don't use tcp options */
#define TF_SENTFIN  0x0010      /* have sent FIN */
#define TF_REQ_SCALE    0x0020      /* have/will request window scaling */
#define TF_RCVD_SCALE   0x0040      /* other side has requested scaling */
#define TF_REQ_TSTMP    0x0080      /* have/will request timestamps */
#define TF_RCVD_TSTMP   0x0100      /* a timestamp was received in SYN */
#define TF_SACK_PERMIT  0x0200      /* other side said I could SACK */
#define TF_NODELAY_PUSH 0x0400      /* other side said I could SACK */

    uint16_t  m_max_tso;        /* maximum packet size input to TSO */
#else /* TREX_FBSD */
#define m_max_tso   t_tsomax
#endif /* TREX_FBSD */

    /*====== end =============*/

    /*====== size 15*4 = 60 bytes  */

#ifndef TREX_FBSD
/*
 * The following fields are used as in the protocol specification.
 * See RFC783, Dec. 1981, page 21.
 */
/* send sequence variables */
    tcp_seq snd_una;        /* send unacknowledged */
    tcp_seq snd_nxt;        /* send next */
    tcp_seq snd_up;         /* send urgent pointer */
    tcp_seq snd_wl1;        /* window update seg seq number */
    tcp_seq snd_wl2;        /* window update seg ack number */
    tcp_seq iss;            /* initial send sequence number */
    uint32_t snd_wnd;        /* send window */
/* receive sequence variables */
    uint32_t  rcv_wnd;        /* receive window */
    tcp_seq rcv_nxt;        /* receive next */
    tcp_seq rcv_up;         /* receive urgent pointer */
    tcp_seq irs;            /* initial receive sequence number */
/*
 * Additional variables for this implementation.
 */
/* receive variables */
    tcp_seq rcv_adv;        /* advertised window */
/* retransmit variables */
    tcp_seq snd_max;        /* highest sequence number sent;
                     * used to recognize retransmits
                     */
/* congestion control (for slow start, source quench, retransmit after loss) */
    uint32_t  snd_cwnd;       /* congestion-controlled window */
    uint32_t  snd_ssthresh;       /* snd_cwnd size threshhold for
                     * for slow start exponential to
                     * linear switch
                     */
/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
    /*====== end =============*/

    /*====== size 13 *4 = 48 bytes  */

    u_short t_idle;         /* inactivity time */
    int16_t t_rtt;          /* round trip time */
    int16_t t_srtt;         /* smoothed round-trip time */
    int16_t t_rttvar;       /* variance in round-trip time */

    tcp_seq t_rtseq;        /* sequence number being timed */
    uint32_t max_sndwnd;     /* largest window peer has offered */
    u_short t_rttmin;       /* minimum rtt allowed */

/* out-of-band data */
#if 0
    char    t_oobflags;     /* have some */
    char    t_iobc;         /* input character */
#endif
#define TCPOOB_HAVEDATA 0x01
#define TCPOOB_HADDATA  0x02
    int16_t t_softerror;        /* possible error not yet reported */

/* RFC 1323 variables */
    u_char  snd_scale;      /* window scaling for send window */
    u_char  rcv_scale;      /* window scaling for recv window */
    u_char  request_r_scale;    /* pending window scaling */
    u_char  requested_s_scale;
    uint32_t  ts_recent;      /* timestamp echo data */
    uint32_t  ts_recent_age;      /* when last updated */
    tcp_seq last_ack_sent;
#endif /* !TREX_FBSD */

    /*====== size 128 + 8 = 132 bytes  */

    CTcpReass * m_tpc_reass; /* tcp reassembley object, allocated only when needed */
    CTcpFlow  * m_flow;      /* back pointer to flow*/
    bool m_reass_disabled;   /* don't reassemble ooo packet, to make payload content in order */

public:

    
    tcpcb() {
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

#ifndef TREX_FBSD
#define intotcpcb(ip)   ((struct tcpcb *)(ip)->inp_ppcb)
#define sototcpcb(so)   (intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define TCP_RTT_SCALE       8   /* multiplier for srtt; 3 bits frac. */
#define TCP_RTT_SHIFT       3   /* shift for srtt; 3 bits frac. */
#define TCP_RTTVAR_SCALE    4   /* multiplier for rttvar; 2 bits */
#define TCP_RTTVAR_SHIFT    2   /* multiplier for rttvar; 2 bits */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */
#define TCP_REXMTVAL(tp) \
    (((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)
#endif /* !TREX_FBSD */

/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */
#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))


#ifdef TREX_FBSD
struct  tcpstat_int_t: public tcpstat {
#else
struct  tcpstat_int_t {
    uint64_t    tcps_connattempt;   /* connections initiated */
    uint64_t    tcps_accepts;       /* connections accepted */
    uint64_t    tcps_connects;      /* connections established */
    uint64_t    tcps_closed;        /* conn. closed (includes drops) */
    uint64_t    tcps_segstimed;     /* segs where we tried to get rtt */
    uint64_t    tcps_rttupdated;    /* times we succeeded */
    uint64_t    tcps_delack;        /* delayed acks sent */
    uint64_t    tcps_sndtotal;      /* total packets sent */
    uint64_t    tcps_sndpack;       /* data packets sent */

    uint64_t    tcps_sndbyte;       /* data bytes sent by application layer  */
    uint64_t    tcps_sndbyte_ok;    /* data bytes sent by tcp  */

    uint64_t    tcps_sndctrl;       /* control (SYN|FIN|RST) packets sent */
    uint64_t    tcps_sndacks;       /* ack-only packets sent */
    uint64_t    tcps_rcvtotal;      /* total packets received */
    uint64_t    tcps_rcvpack;       /* packets received in sequence */
    uint64_t    tcps_rcvbyte;       /* bytes received in sequence */
    uint64_t    tcps_rcvackpack;    /* rcvd ack packets */
    uint64_t    tcps_rcvackbyte;    /* bytes acked by rcvd acks */
    uint64_t    tcps_preddat;       /* times hdr predict ok for data pkts */

/* ERRORs */
    uint64_t    tcps_drops;     /* connections dropped */
    uint64_t    tcps_conndrops;     /* embryonic connections dropped */
    uint64_t    tcps_timeoutdrop;   /* conn. dropped in rxmt timeout */
    uint64_t    tcps_rexmttimeo;    /* retransmit timeouts */
    uint64_t    tcps_rexmttimeo_syn;  /* retransmit SYN timeouts */
    uint64_t    tcps_persisttimeo;  /* persist timeouts */
    uint64_t    tcps_keeptimeo;     /* keepalive timeouts */
    uint64_t    tcps_keepprobe;     /* keepalive probes sent */
    uint64_t    tcps_keepdrops;     /* connections dropped in keepalive */
#endif
    uint64_t    tcps_testdrops;     /* connections dropped at the end of the test due to --nc  */

#ifndef TREX_FBSD
    uint64_t    tcps_sndrexmitpack; /* data packets retransmitted */
    uint64_t    tcps_sndrexmitbyte; /* data bytes retransmitted */
    uint64_t    tcps_sndprobe;      /* window probes sent */
    uint64_t    tcps_sndurg;        /* packets sent with URG only */
    uint64_t    tcps_sndwinup;      /* window update-only packets sent */

    uint64_t    tcps_rcvbadsum;     /* packets received with ccksum errs */
    uint64_t    tcps_rcvbadoff;     /* packets received with bad offset */
    uint64_t    tcps_rcvshort;      /* packets received too short */
    uint64_t    tcps_rcvduppack;    /* duplicate-only packets received */
    uint64_t    tcps_rcvdupbyte;    /* duplicate-only bytes received */
    uint64_t    tcps_rcvpartduppack;    /* packets with some duplicate data */
    uint64_t    tcps_rcvpartdupbyte;    /* dup. bytes in part-dup. packets */
#endif
    uint64_t    tcps_rcvoopackdrop;     /* OOO packet drop due to queue len */
    uint64_t    tcps_rcvoobytesdrop;     /* OOO bytes drop due to queue len */

#ifndef TREX_FBSD
    uint64_t    tcps_rcvoopack;     /* out-of-order packets received */
    uint64_t    tcps_rcvoobyte;     /* out-of-order bytes received */
    uint64_t    tcps_rcvpackafterwin;   /* packets with data after window */
    uint64_t    tcps_rcvbyteafterwin;   /* bytes rcvd after window */
    uint64_t    tcps_rcvafterclose; /* packets rcvd after "close" */
    uint64_t    tcps_rcvwinprobe;   /* rcvd window probe packets */
    uint64_t    tcps_rcvdupack;     /* rcvd duplicate acks */
    uint64_t    tcps_rcvacktoomuch; /* rcvd acks for unsent data */
    uint64_t    tcps_rcvwinupd;     /* rcvd window update packets */
    uint64_t    tcps_pawsdrop;      /* segments dropped due to PAWS */
    uint64_t    tcps_predack;       /* times hdr predict ok for acks */
    uint64_t    tcps_persistdrop;   /* timeout in persist state */
    uint64_t    tcps_badsyn;        /* bogus SYN, e.g. premature ACK */
#endif

    uint64_t    tcps_reasalloc;     /* allocate tcp reasembly object */
    uint64_t    tcps_reasfree;      /* free tcp reasembly object  */
    uint64_t    tcps_nombuf;        /* no mbuf for tcp - drop the packets */
    uint64_t    tcps_notunnel;       /* no GTP Tunnel for tcp - drop the packets */
#ifndef TREX_FBSD
    uint64_t    tcps_rcvackbyte_of;    /* bytes acked by rcvd acks */
#endif
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

#ifndef TREX_FBSD
#define PRU_SLOWTIMO        19  /* 500ms timeout */
#endif


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
                   uint16_t vlan,
                   uint8_t  proto,
                   void    *tunnel_ctx,
                   bool     is_ipv6){
        m_vlan = vlan;
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

    uint32_t get_src_ipv4(){
        return(m_src_ipv4);
    }
    uint32_t get_dst_ipv4(){
        return(m_dst_ipv4);
    }

    uint16_t get_src_port(){
        return(m_src_port);
    }
    uint16_t get_dst_port(){
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
    uint16_t  m_vlan;
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
                   uint16_t vlan,
                   uint8_t  proto,
                   bool is_ipv6){
        m_template.set_tuple(src,dst,src_port,dst_port,vlan,proto,NULL,is_ipv6);
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


    HOT_FUNC bool is_can_close(){
        return (m_tcp.t_state == TCPS_CLOSED ?true:false);
    }

    bool is_close_was_called(){
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

public:
    CHTimerObj        m_timer; /* 32 bytes */ 
    tcpcb             m_tcp;
    uint8_t           m_tick;

    CServerTemplateInfo*    m_template_info; /* to save the identified template */
    CServerIpPayloadInfo*   m_payload_info; /* to manage template during identifying */
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
                      struct tcpcb * tp,
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

#ifndef TREX_FBSD
static inline uint16_t _update_initwnd(uint16_t mss,uint16_t initwnd){
    uint32_t calc =mss*initwnd;

    if (calc>48*1024) {
        calc=48*1024;
    }
    return((uint16_t)calc);
}
#endif

typedef void (*on_stopped_cb_t)(void *data, profile_id_t profile_id);

#ifdef TREX_FBSD
class CTcpTunableCtx: public tcp_tune {
#else
class CTcpTunableCtx {
#endif
public:
    /* TUNABLEs */
    uint32_t  tcp_tx_socket_bsize;
    uint32_t  tcp_rx_socket_bsize;

#ifndef TREX_FBSD
    int tcprexmtthresh;
    int tcp_mssdflt;
    int tcp_initwnd_factor; /* slow start initwnd, should be 1 in default but for optimization we start at 5 */
    int tcp_initwnd;        /*  tcp_initwnd_factor *tcp_mssdflt*/
#else
#define tcp_initwnd_factor  tcp_initcwnd_segments
#endif
    uint32_t  sb_max ; /* socket max char */
    int tcp_max_tso;   /* max tso default */
    int tcp_rttdflt;
#ifndef TREX_FBSD
    int tcp_do_rfc1323;
#endif
    int tcp_no_delay;
    int tcp_no_delay_counter; /* number of recv bytes to wait until ack them */
#ifndef TREX_FBSD
    int tcp_keepinit;
    int tcp_keepidle;       /* time before keepalive probes begin */
    int tcp_keepintvl;      /* time between keepalive probes */
#endif
    int tcp_blackhole;
#ifndef TREX_FBSD
    int tcp_keepcnt;
#endif
    int tcp_maxidle;            /* time to drop after starting probes */
    int tcp_maxpersistidle;
    uint32_t tcp_fast_ticks;
#ifndef TREX_FBSD
    uint32_t tcp_slow_fast_ratio;
#endif
    int tcp_ttl;            /* time to live for TCP segs */

    uint8_t use_inbound_mac;    /* whether to use MACs from incoming pkts */

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

    uint32_t            m_stop_id;

    int                 m_flow_cnt; /* active flow count */

    on_stopped_cb_t     m_on_stopped_cb;
    void              * m_cb_data;

    void              * m_tx_node;  /* to make CGenNodeTXFIF stop safe */

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
        return (m_timer_w.timer_start(&flow->m_timer,flow->m_pctx->m_tunable_ctx.tcp_fast_ticks));
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
            return (m_timer_w.timer_start(&flow->m_timer,flow->m_pctx->m_tunable_ctx.tcp_fast_ticks));
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

    KxuLCRand         *  m_rand; /* per context */
    CTcpCtxCb         *  m_cb;
    CNATimerWheel        m_timer_w; /* TBD-FIXME one timer , should be pointer */

    CFlowTable           m_ft;
    struct  tcpiphdr tcp_saveti;
    CTunnelHandler   *m_tunnel_handler;

    /* server port management */
private:
    std::unordered_map<uint32_t,CServerPortInfo> m_server_ports;
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
 CTcpReass(){
     m_active_blocks=0;
 }

 int pre_tcp_reass(CPerProfileCtx * pctx,
              struct tcpcb *tp, 
              struct tcpiphdr *ti, 
              struct rte_mbuf *m);

 int tcp_reass_no_data(CPerProfileCtx * pctx,
                         struct tcpcb *tp);

 int tcp_reass(CPerProfileCtx * pctx,
                          struct tcpcb *tp, 
                          struct tcpiphdr *ti, 
                          struct rte_mbuf *m);

#ifdef  TREX_SIM
 int pre_tcp_reass(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m) { return pre_tcp_reass(DEFAULT_PROFILE_CTX(ctx), tp, ti, m); }

 int tcp_reass_no_data(CTcpPerThreadCtx * ctx,
                         struct tcpcb *tp) { return tcp_reass_no_data(DEFAULT_PROFILE_CTX(ctx), tp); }

 int tcp_reass(CTcpPerThreadCtx * ctx,
                          struct tcpcb *tp,
                          struct tcpiphdr *ti,
                          struct rte_mbuf *m) { return tcp_reass(DEFAULT_PROFILE_CTX(ctx), tp, ti, m); }
#endif

 inline uint8_t   get_active_blocks(void){
     return (m_active_blocks);
 }

 void Dump(FILE *fd);

 bool expect(vec_tcp_reas_t & lpkts,FILE * fd);

private:
  uint8_t         m_active_blocks;
  CTcpReassBlock  m_blocks[MAX_TCP_REASS_BLOCKS];
};


#define INC_STAT(ctx, tg_id, p) {ctx->m_tcpstat.m_sts.p++; ctx->m_tcpstat.m_sts_tg_id[tg_id].p++; }
#define INC_STAT_CNT(ctx, tg_id, p, cnt) {ctx->m_tcpstat.m_sts.p += cnt; ctx->m_tcpstat.m_sts_tg_id[tg_id].p += cnt; }


void tcp_fasttimo(CPerProfileCtx * pctx, struct tcpcb *tp);
void tcp_slowtimo(CPerProfileCtx * pctx, struct tcpcb *tp);

int tcp_listen(CPerProfileCtx * pctx,struct tcpcb *tp);

int tcp_connect(CPerProfileCtx * pctx,struct tcpcb *tp);
struct tcpcb * tcp_usrclosed(CPerProfileCtx * pctx,struct tcpcb *tp);
struct tcpcb * tcp_disconnect(CPerProfileCtx * pctx,struct tcpcb *tp);



int  tcp_output(CPerProfileCtx * pctx,struct tcpcb * tp);
struct tcpcb *  tcp_close(CPerProfileCtx * pctx,struct tcpcb *tp);
void  tcp_setpersist(CPerProfileCtx * pctx,struct tcpcb *tp);
void  tcp_respond(CPerProfileCtx * pctx,struct tcpcb *tp, tcp_seq ack, tcp_seq seq, int flags);
int  tcp_mss(CPerProfileCtx * pctx,struct tcpcb *tp, u_int offer);

CTcpTuneables * tcp_get_parent_tunable(CPerProfileCtx * pctx,struct tcpcb *tp);

int tcp_reass(CPerProfileCtx * pctx,
              struct tcpcb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m);
#ifdef  TREX_SIM
int tcp_connect(CTcpPerThreadCtx * ctx,struct tcpcb *tp);
int tcp_reass(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp,
              struct tcpiphdr *ti,
              struct rte_mbuf *m);
#endif


int tcp_flow_input(CPerProfileCtx * pctx,
                   struct tcpcb *tp, 
                   struct rte_mbuf *m,
                   TCPHeader *tcp,
                   int offset_l7,
                   int total_l7_len);


void tcp_trace(CPerProfileCtx * pctx,short act, short ostate, struct tcpcb * tp, struct tcpiphdr * ti, TCPHeader * tio, int req);

const char ** tcp_get_tcpstate();


void tcp_quench(struct tcpcb *tp);
void     tcp_xmit_timer(CPerProfileCtx * pctx,struct tcpcb *, int16_t rtt);

void tcp_canceltimers(struct tcpcb *tp);

int tcp_build_cpkt(CPerProfileCtx * pctx,
                   struct tcpcb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt);

int tcp_build_dpkt(CPerProfileCtx * pctx,
                   struct tcpcb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t  tcphlen,
                   CTcpPkt &pkt);




struct tcpcb * tcp_drop_now(CPerProfileCtx * pctx,
                            struct tcpcb *tp, 
                            int res);



inline bool tcp_reass_is_exists(struct tcpcb *tp){
    return (tp->m_tpc_reass != ((CTcpReass *)0));
}

inline void tcp_reass_alloc(CPerProfileCtx * pctx,
                            struct tcpcb *tp){
    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reasalloc);
    tp->m_tpc_reass = new CTcpReass();
}

inline void tcp_reass_free(CPerProfileCtx * pctx,
                            struct tcpcb *tp){
    INC_STAT(pctx, tp->m_flow->m_tg_id, tcps_reasfree);
    delete tp->m_tpc_reass;
    tp->m_tpc_reass=(CTcpReass *)0;
}

inline void tcp_reass_clean(CPerProfileCtx * pctx,
                            struct tcpcb *tp){
    if (tcp_reass_is_exists(tp) ){
        tcp_reass_free(pctx,tp);
    }
}

extern struct tcpcb *debug_flow; 


inline void tcp_set_debug_flow(struct tcpcb * debug){
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
        tcp_output(pctx,&flow->m_tcp);
    }

    virtual void disconnect(CPerProfileCtx * pctx,
                            CTcpFlow *         flow){
        tcp_disconnect(pctx,&flow->m_tcp);
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
#ifndef TREX_FBSD
        on_fast_tick();
        m_tick++;
        if (m_tick == m_pctx->m_tunable_ctx.tcp_slow_fast_ratio) {
            on_slow_tick();
            m_tick=0;
        }
#else
        m_tick++;
        tcp_handle_timers(&m_tcp);
#endif
}




#endif
