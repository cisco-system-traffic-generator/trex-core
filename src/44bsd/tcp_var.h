#ifndef BSD44_BR_TCP_VAR
#define BSD44_BR_TCP_VAR

#include <stdint.h>
#include <stddef.h>

#ifdef _DEBUG
#define TCPDEBUG
#endif

#include "tcp.h"
#include "tcp_timer.h"
#include "mbuf.h"
#include "tcp_socket.h"
#include "tcp_fsm.h"
#include "tcp_debug.h"
#include <vector>
#include "tcp_dpdk.h"
#include "tcp_bsd_utl.h"
#include "flow_table.h"
#include <common/utl_gcc_diag.h>

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


struct tcpcb {

    tcp_socket m_socket; 

    /* ====== size 8 bytes  */

    uint8_t t_timer[TCPT_NTIMERS];  /* tcp timers */
    char    t_force;        /* 1 if forcing out a byte */
    uint8_t mbuf_socket;    /* mbuf socket */
    uint8_t t_dupacks;      /* consecutive dup acks recd */
    uint8_t m_pad1;
    /*====== end =============*/

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

    uint16_t  m_max_tso;        /* maximum packet size input to TSO */

    /*====== end =============*/

    /*====== size 15*4 = 60 bytes  */

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

    uint32_t  src_ipv4;
    uint32_t  dst_ipv4;
    uint16_t  src_port;
    uint16_t  dst_port;
    uint16_t  vlan;
    uint16_t  l4_pseudo_checksum;

    uint8_t offset_tcp; /* offset of tcp_header, in template */
    uint8_t offset_ip;  /* offset of ip_header in template */
    uint8_t is_ipv6;
    uint8_t m_offload_flags;
    #define TCP_OFFLOAD_TX_CHKSUM   0x0001      /* DPDK_CHECK_SUM */
    #define TCP_OFFLOAD_TSO         0x0002      /* DPDK_TSO_CHECK_SUM */
    #define TCP_OFFLOAD_RX_CHKSUM   0x0004      /* check RX checksum L4*/


    /*====== end =============*/

    /*====== size 128 + 8 = 132 bytes  */

    CTcpReass * m_tpc_reass; /* tcp reassembley object, allocated only when needed */

    uint8_t template_pkt[PACKET_TEMPLATE_SIZE];   /* template packet */

    /*====== end =============*/

public:

    

    inline bool is_tso(){
        return( ((m_offload_flags & TCP_OFFLOAD_TSO)==TCP_OFFLOAD_TSO)?true:false);
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

/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */
#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))


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
    uint64_t    tcps_persisttimeo;  /* persist timeouts */
    uint64_t    tcps_keeptimeo;     /* keepalive timeouts */
    uint64_t    tcps_keepprobe;     /* keepalive probes sent */
    uint64_t    tcps_keepdrops;     /* connections dropped in keepalive */

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
    uint64_t    tcps_rcvoopackdrop;     /* OOO packet drop due to queue len */
    uint64_t    tcps_rcvoobytesdrop;     /* OOO bytes drop due to queue len */

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

    uint64_t    tcps_reasalloc;     /* allocate tcp reasembly object */
    uint64_t    tcps_reasfree;      /* free tcp reasembly object  */
    uint64_t    tcps_nombuf;        /* no mbuf for tcp - drop the packets */
    uint64_t    tcps_rcvackbyte_of;    /* bytes acked by rcvd acks */
};

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct  tcpstat {

    tcpstat_int_t m_sts;
public:
    void Clear();
    void Dump(FILE *fd);
};

#define PRU_SLOWTIMO        19  /* 500ms timeout */


#ifdef TREX_SIM

#define TCP_TIMER_TICK_BASE_MS 200 
#define TCP_TIMER_W_TICK       50
#define TCP_TIMER_W_DIV        1

#define TCP_FAST_TICK (TCP_TIMER_TICK_BASE_MS/TCP_TIMER_W_TICK)
#define TCP_SLOW_RATIO_TICK 1
#define TCP_SLOW_RATIO_MASTER ((500)/TCP_TIMER_W_TICK)
#define TCP_SLOW_FAST_RATIO 1



#else 

// base tick in msec

/* base tick of timer in msec */
#define TCP_TIMER_W_TICK       1  

/* fast tick msec */
#define TCP_TIMER_TICK_FAST_MS 100

/* flow tick msec */
#define TCP_TIMER_TICK_SLOW_MS 500


/* to how many micro tick to div the TCP_TIMER_W_TICK */
#define TCP_TIMER_W_DIV        50


/* ticks of FAST */
#define TCP_FAST_TICK       (TCP_TIMER_TICK_FAST_MS/TCP_TIMER_W_TICK)

#define TCP_SLOW_RATIO_MASTER ((TCP_TIMER_TICK_SLOW_MS*TCP_TIMER_W_DIV)/TCP_TIMER_W_TICK)

#define TCP_TIME_TICK_SEC ((double)TCP_TIMER_W_TICK/((double)TCP_TIMER_W_DIV* 1000.0))

/* how many fast ticks need to get */
#define TCP_SLOW_FAST_RATIO   (TCP_TIMER_TICK_SLOW_MS/TCP_TIMER_TICK_FAST_MS)


#endif

/* main tick in sec */
#define TCP_TIME_TICK_SEC ((double)TCP_TIMER_W_TICK/((double)TCP_TIMER_W_DIV* 1000.0))


#include "h_timer.h"

class CTcpPerThreadCtx ;




class CTcpFlow {

public:
    void Create(CTcpPerThreadCtx *ctx);
    void Delete();

public:
    void set_tuple(uint32_t src,
                   uint32_t dst,
                   uint16_t src_port,
                   uint16_t dst_port,
                   uint16_t vlan,
                   bool is_ipv6){
        m_tcp.vlan = vlan;
        m_tcp.src_ipv4 = src;
        m_tcp.dst_ipv4 = dst;
        m_tcp.src_port = src_port;
        m_tcp.dst_port = dst_port;
        m_tcp.is_ipv6  = is_ipv6;
    }
    void init();
public:

    static CTcpFlow * cast_from_hash_obj(flow_hash_ent_t *p){
        UNSAFE_CONTAINER_OF_PUSH
        CTcpFlow * lp2 =(CTcpFlow *)((uint8_t*)p-offsetof (CTcpFlow,m_hash));
        UNSAFE_CONTAINER_OF_POP
        return (lp2);
    }

    void on_slow_tick();

    void on_fast_tick();

    void on_tick(){
        on_fast_tick();
        if (m_tick==TCP_SLOW_FAST_RATIO) {
            m_tick=0;
            on_slow_tick();
        }else{
            m_tick++;
        }
    }

    bool is_can_close(){
        return (m_tcp.t_state == TCPS_CLOSED ?true:false);
    }

    bool is_close_was_called(){
        return ((m_tcp.t_state > TCPS_CLOSE_WAIT) ?true:false);
    }

    void set_app(CTcpApp  * app){
        m_tcp.m_socket.m_app = app;
    }

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

    /* update MAC addr from the packet in reverse */
    void server_update_mac_from_packet(uint8_t *pkt);

public:
    flow_hash_ent_t   m_hash; /* object   */
    tcpcb             m_tcp;
    CHTimerObj        m_timer;
    CTcpApp           m_app;
    CTcpPerThreadCtx *m_ctx;
    uint8_t           m_tick;
    uint8_t           m_c_idx_enable;
    uint8_t           m_pad[2];
    uint32_t          m_c_idx;
    uint16_t          m_c_pool_idx;
    uint16_t          m_c_template_idx;
};


class CTcpCtxCb {
public:
    virtual ~CTcpCtxCb(){
    }
    virtual int on_tx(CTcpPerThreadCtx *ctx,
                      struct tcpcb * tp,
                      rte_mbuf_t *m)=0;

    virtual int on_flow_close(CTcpPerThreadCtx *ctx,
                              CTcpFlow * flow)=0;

    virtual int on_redirect_rx(CTcpPerThreadCtx *ctx,
                               rte_mbuf_t *m)=0;


};

class CTcpData;
class CAstfTemplatesRW;

class CTcpPerThreadCtx {
public:
    bool Create(uint32_t size,
                bool is_client);
    void Delete();
    RC_HTW_t timer_w_start(CTcpFlow * flow){
        return (m_timer_w.timer_start(&flow->m_timer,TCP_FAST_TICK));
    }

    RC_HTW_t timer_w_restart(CTcpFlow * flow){
        if (flow->is_can_close()) {
            /* free the flow in case it was finished */
            m_ft.handle_close(this,flow,true);
            return(RC_HTW_OK);
        }else{
            return (m_timer_w.timer_start(&flow->m_timer,TCP_FAST_TICK));
        }
    }


    RC_HTW_t timer_w_stop(CTcpFlow * flow){
        return (m_timer_w.timer_stop(&flow->m_timer));
    }

    void timer_w_on_tick();
    bool timer_w_any_events(){
        return(m_timer_w.is_any_events_left());
    }

    CTcpData *get_template_ro() {return m_template_ro;}
    void set_template_ro(CTcpData *t) {m_template_ro = t;}
    void set_template_rw(CAstfTemplatesRW *t) {m_template_rw = t;}
    void set_cb(CTcpCtxCb    * cb){
        m_cb=cb;
    }

    void set_memory_socket(uint8_t socket){
        m_mbuf_socket= socket;
    }

    void set_offload_dev_flags(uint8_t flags){
        m_offload_flags=flags;
    }

    bool get_rx_checksum_check(){
        return( ((m_offload_flags & TCP_OFFLOAD_RX_CHKSUM) == TCP_OFFLOAD_RX_CHKSUM)?true:false);
    }
public:

    /* TUNABLEs */
    uint32_t  tcp_tx_socket_bsize;
    uint32_t  tcp_rx_socket_bsize;

    uint32_t  sb_max ; /* socket max char */
    int tcprexmtthresh;
    int tcp_mssdflt;
    int tcp_max_tso;   /* max tso default */
    int tcp_rttdflt;
    int tcp_do_rfc1323;
    int tcp_keepidle;       /* time before keepalive probes begin */
    int tcp_keepintvl;      /* time between keepalive probes */
    int tcp_keepcnt;
    int tcp_maxidle;            /* time to drop after starting probes */
    int tcp_maxpersistidle;
    int tcp_ttl;            /* time to live for TCP segs */

    //struct    inpcb tcb;      /* head of queue of active tcpcb's */
    struct      tcpstat m_tcpstat;  /* tcp statistics */
    uint32_t    tcp_now;        /* for RFC 1323 timestamps */
    tcp_seq     tcp_iss;            /* tcp initial send seq # */
    uint32_t    m_tick;
    uint8_t     m_mbuf_socket;      /* memory socket */
    uint8_t     m_offload_flags;    /* dev offload flags, see flow def */
    CAstfTemplatesRW * m_template_rw;
    CTcpData    * m_template_ro;
    uint8_t     m_pad[2];


    CNATimerWheel m_timer_w; /* TBD-FIXME one timer , should be pointer */
    CTcpCtxCb    * m_cb;

    CFlowTable   m_ft;
    struct  tcpiphdr tcp_saveti;
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

 int pre_tcp_reass(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp, 
              struct tcpiphdr *ti, 
              struct rte_mbuf *m);

 int tcp_reass_no_data(CTcpPerThreadCtx * ctx,
                         struct tcpcb *tp);

 int tcp_reass(CTcpPerThreadCtx * ctx,
                          struct tcpcb *tp, 
                          struct tcpiphdr *ti, 
                          struct rte_mbuf *m);


 inline uint8_t   get_active_blocks(void){
     return (m_active_blocks);
 }

 void Dump(FILE *fd);

 bool expect(vec_tcp_reas_t & lpkts,FILE * fd);

private:
  uint8_t         m_active_blocks;
  CTcpReassBlock  m_blocks[MAX_TCP_REASS_BLOCKS];
};



#define INC_STAT(ctx,p) {ctx->m_tcpstat.m_sts.p++; }
#define INC_STAT_CNT(ctx,p,cnt) {ctx->m_tcpstat.m_sts.p += cnt; }


void tcp_fasttimo(CTcpPerThreadCtx * ctx, struct tcpcb *tp);
void tcp_slowtimo(CTcpPerThreadCtx * ctx, struct tcpcb *tp);

int tcp_listen(CTcpPerThreadCtx * ctx,struct tcpcb *tp);

int tcp_connect(CTcpPerThreadCtx * ctx,struct tcpcb *tp);
struct tcpcb * tcp_usrclosed(CTcpPerThreadCtx * ctx,struct tcpcb *tp);
struct tcpcb * tcp_disconnect(CTcpPerThreadCtx * ctx,struct tcpcb *tp);



int  tcp_output(CTcpPerThreadCtx * ctx,struct tcpcb * tp);
struct tcpcb *  tcp_close(CTcpPerThreadCtx * ctx,struct tcpcb *tp);
void  tcp_setpersist(CTcpPerThreadCtx * ctx,struct tcpcb *tp);
void  tcp_respond(CTcpPerThreadCtx * ctx,struct tcpcb *tp, tcp_seq ack, tcp_seq seq, int flags);
int  tcp_mss(CTcpPerThreadCtx * ctx,struct tcpcb *tp, u_int offer);

int tcp_reass(CTcpPerThreadCtx * ctx,
              struct tcpcb *tp, 
              struct tcpiphdr *ti, 
              struct rte_mbuf *m);


int tcp_flow_input(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp, 
                   struct rte_mbuf *m,
                   TCPHeader *tcp,
                   int offset_l7,
                   int total_l7_len);


void tcp_trace(CTcpPerThreadCtx * ctx,short act, short ostate, struct tcpcb * tp, struct tcpiphdr * ti, TCPHeader * tio, int req);

const char ** tcp_get_tcpstate();


void tcp_quench(struct tcpcb *tp);
void tcp_template(struct tcpcb *tp);
void     tcp_xmit_timer(CTcpPerThreadCtx * ctx,struct tcpcb *, int16_t rtt);

void tcp_canceltimers(struct tcpcb *tp);

int tcp_build_cpkt(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint16_t tcphlen,
                   CTcpPkt &pkt);

int tcp_build_dpkt(CTcpPerThreadCtx * ctx,
                   struct tcpcb *tp,
                   uint32_t offset, 
                   uint32_t dlen,
                   uint16_t  tcphlen,
                   CTcpPkt &pkt);




struct tcpcb * tcp_drop_now(CTcpPerThreadCtx * ctx,
                            struct tcpcb *tp, 
                            int res);



inline bool tcp_reass_is_exists(struct tcpcb *tp){
    return (tp->m_tpc_reass != ((CTcpReass *)0));
}

inline void tcp_reass_alloc(CTcpPerThreadCtx * ctx,
                            struct tcpcb *tp){
    INC_STAT(ctx,tcps_reasalloc);
    tp->m_tpc_reass = new CTcpReass();
}

inline void tcp_reass_free(CTcpPerThreadCtx * ctx,
                            struct tcpcb *tp){
    INC_STAT(ctx,tcps_reasfree);
    delete tp->m_tpc_reass;
    tp->m_tpc_reass=(CTcpReass *)0;
}

inline void tcp_reass_clean(CTcpPerThreadCtx * ctx,
                            struct tcpcb *tp){
    if (tcp_reass_is_exists(tp) ){
        tcp_reass_free(ctx,tp);
    }
}

extern struct tcpcb *debug_flow; 


inline void tcp_set_debug_flow(struct tcpcb * debug){
#ifdef    TCPDEBUG
    debug_flow  =debug;
#endif
}





#if 0
int  tcp_attach __P((struct socket *));
void     tcp_canceltimers __P((struct tcpcb *));
struct tcpcb *
     tcp_close __P((struct tcpcb *));
void     tcp_ctlinput __P((int, struct sockaddr *, struct ip *));
int  tcp_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
struct tcpcb *
     tcp_disconnect __P((struct tcpcb *));
struct tcpcb *
     tcp_drop __P((struct tcpcb *, int));
void     tcp_dooptions __P((struct tcpcb *,
        u_char *, int, struct tcpiphdr *, int *, uint64_t *, uint64_t *));
void     tcp_drain __P((void));
void     tcp_fasttimo __P((void));
void     tcp_init __P((void));
void     tcp_input __P((struct mbuf *, int));
int  tcp_mss __P((struct tcpcb *, u_int));
struct tcpcb *
     tcp_newtcpcb __P((struct inpcb *));
void     tcp_notify __P((struct inpcb *, int));
int  tcp_output __P((struct tcpcb *));
void     tcp_pulloutofband __P((struct socket *,
        struct tcpiphdr *, struct mbuf *));
int  tcp_reass __P((struct tcpcb *, struct tcpiphdr *, struct mbuf *));
void     tcp_respond __P((struct tcpcb *,
        struct tcpiphdr *, struct mbuf *, uint64_t, uint64_t, int));
void     tcp_setpersist __P((struct tcpcb *));
void     tcp_slowtimo __P((void));
struct tcpiphdr *
     tcp_template __P((struct tcpcb *));
struct tcpcb *
     tcp_timers __P((struct tcpcb *, int));
void     tcp_trace __P((int, int, struct tcpcb *, struct tcpiphdr *, int));
struct tcpcb *
     tcp_usrclosed __P((struct tcpcb *));
int  tcp_usrreq __P((struct socket *,
        int, struct mbuf *, struct mbuf *, struct mbuf *));
void     tcp_xmit_timer __P((struct tcpcb *, int));
#endif


class CTcpAppApiImpl : public CTcpAppApi {
public:

    /* get maximum tx queue space */
    virtual uint32_t get_tx_max_space(CTcpFlow * flow){
        return (flow->m_tcp.m_socket.so_snd.sb_hiwat);
    }

    /* get space in bytes in the tx queue */
    virtual uint32_t get_tx_sbspace(CTcpFlow * flow){
        return (flow->m_tcp.m_socket.so_snd.get_sbspace());
    }

    /* add bytes to tx queue */
    virtual void tx_sbappend(CTcpFlow * flow,uint32_t bytes){
        INC_STAT_CNT(flow->m_ctx,tcps_sndbyte,bytes);
        flow->m_tcp.m_socket.so_snd.sbappend(bytes);
    }

    virtual uint32_t  rx_drain(CTcpFlow * flow){
        /* drain bytes from the rx queue */
        uint32_t res=flow->m_tcp.m_socket.so_rcv.sb_cc;
        flow->m_tcp.m_socket.so_rcv.sb_cc=0;
        return(res);
    }


    virtual void tx_tcp_output(CTcpPerThreadCtx * ctx,CTcpFlow *         flow){
        tcp_output(ctx,&flow->m_tcp);
    }

    virtual void tcp_delay(uint64_t usec){
        assert(0);
        printf("TBD \n");
    }
};



#endif
