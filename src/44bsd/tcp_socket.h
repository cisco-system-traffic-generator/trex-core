#ifndef BSD44_BR_TCP_SOCKET
#define BSD44_BR_TCP_SOCKET

/*
 * Copyright (c) 1982, 1986, 1993
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
 *  @(#)tcp_socket.h    8.1 (Berkeley) 6/10/93
 */

#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

                 

struct  sockbuf {
    uint32_t    sb_cc;      /* actual chars in buffer */
    uint32_t    sb_hiwat;   /* max actual char count */
    short       sb_flags;   /* flags, see below */
    //short sb_timeo;   /* timeout for read/write */
};

// so_state
#define US_SS_CANTRCVMORE  1


#define US_SO_DEBUG 0x0001      /* turn on debugging info recording */
#define US_SO_ACCEPTCONN    0x0002      /* socket has had listen() */
#define US_SO_REUSEADDR 0x0004      /* allow local address reuse */
#define US_SO_KEEPALIVE 0x0008      /* keep connections alive */
#define US_SO_DONTROUTE 0x0010      /* just use interface addresses */
#define US_SO_BROADCAST 0x0020      /* permit sending of broadcast msgs */

#define US_SO_LINGER    0x0080      /* linger on close if data present */
#define US_SO_OOBINLINE 0x0100      /* leave received OOB data in line */
//#if __BSD_VISIBLE

#define US_SO_REUSEPORT 0x0200      /* allow local address & port reuse */

#define US_SO_TIMESTAMP 0x0400      /* timestamp received dgram traffic */
#define US_SO_NOSIGPIPE 0x0800      /* no SIGPIPE from EPIPE */
#define US_SO_ACCEPTFILTER  0x1000      /* there is an accept filter */
#define US_SO_BINTIME   0x2000      /* timestamp received dgram traffic */
//#endif
#define US_SO_NO_OFFLOAD    0x4000      /* socket cannot be offloaded */
#define US_SO_NO_DDP    0x8000      /* disable direct data placement */


#define SB_MAX      (256*1024)  /* default for max chars in sockbuf */
#define SB_LOCK     0x01        /* lock on data queue */
#define SB_WANT     0x02        /* someone is waiting to lock */
#define SB_WAIT     0x04        /* someone is waiting for data/space */
#define SB_SEL      0x08        /* someone is selecting */
#define SB_ASYNC    0x10        /* ASYNC I/O, need signals */
#define SB_NOTIFY   (SB_WAIT|SB_SEL|SB_ASYNC)
#define SB_NOINTR   0x40        /* operations not interruptible */
#define SB_DROPCALL 0x80



struct tcp_socket * sonewconn(struct tcp_socket *head, int connstatus);

uint32_t    sbspace(struct sockbuf *sb); 
//void    sbdrop(struct sockbuf *sb, int len);

int soabort(struct tcp_socket *so);
void sowwakeup(struct tcp_socket *so);
void sorwakeup(struct tcp_socket *so);


void    soisdisconnected(struct tcp_socket *so);
void    soisdisconnecting(struct tcp_socket *so);


void sbflush (struct sockbuf *sb);
//void    sbappend(struct sockbuf *sb, struct rte_mbuf *m);

void    sbappend(struct tcp_socket *so,
                 struct sockbuf *sb, 
                 struct rte_mbuf *m,
                 uint32_t len);

void    sbappend_bytes(struct tcp_socket *so,
                       struct sockbuf *sb, 
                       uint32_t len);

#ifdef FIXME
#define sorwakeup(so)   { sowakeup((so), &(so)->so_rcv); \
              if ((so)->so_upcall) \
                (*((so)->so_upcall))((so), (so)->so_upcallarg, M_DONTWAIT); \
                
            }
#else


#endif            

void    soisconnected(struct tcp_socket *so);
void    soisconnecting(struct tcp_socket *so);

void    socantrcvmore(struct tcp_socket *so);


//#define   IN_CLASSD(i)        (((u_int32_t)(i) & 0xf0000000) == 0xe0000000)
/* send/received as ip/link-level broadcast */
#define TCP_PKT_M_BCAST     (1ULL << 1) 

/* send/received as iplink-level multicast */
#define TCP_PKT_M_MCAST     (1ULL << 2) 
#define TCP_PKT_M_B_OR_MCAST    (TCP_PKT_M_BCAST  | TCP_PKT_M_MCAST)

typedef uint8_t tcp_l2_pkt_flags_t ;



#define TCP_US_ECONNREFUSED  (7)
#define TCP_US_ECONNRESET    (8)

#define TCP_US_ETIMEDOUT     (9)
#define TCP_US_ECONNABORTED  (10)
#define TCP_US_ENOBUFS       (11)


typedef enum { MO_CONST=17,
               MO_RW 
             } mbuf_types_enum_t;

typedef uint32_t mbuf_types_t;

class CMbufObject{
public:
    mbuf_types_t      m_type;
    struct rte_mbuf * m_mbuf;
    void Dump(FILE *fd);
    void Free(){
        if (m_mbuf) {
            rte_pktmbuf_free(m_mbuf);
            m_mbuf=0;
        }
    }
};

class CBufMbufRef : public CMbufObject {

public:
    inline uint16_t get_mbuf_size(){
        return (m_mbuf->pkt_len-m_offset);
    }

    inline bool need_indirect_mbuf(uint16_t asked_pkt_size){
        /* need indirect if we need partial packet*/
        if ( (m_offset!=0) || (m_mbuf->pkt_len!=asked_pkt_size) ) {
            return(true);
        }
        return(false);
    }

    /*uint16_t get_mbuf_ptr(){
        m_mbuf->pkt_len
    } */

    void Dump(FILE *fd);

public:
    uint16_t m_offset;  /* offset to the packet */
};



typedef std::vector<CMbufObject> vec_mbuf_t;

class CMbufBuffer {
public:
    void Create(uint32_t blk_size);
    void Delete();

    /**
     * the blocks must be in the same size 
     * 
     * @param obj
     */
    void Add_mbuf(CMbufObject & obj);
    bool Verify();
    void Dump(FILE *fd);


    /**
     * return pointer of mbuf in offset and res size
     * 
     * @param offset
     * @param res
     * 
     */
    void get_by_offset(uint32_t offset,CBufMbufRef & res);

private:
    void Reset();
public:
    vec_mbuf_t   m_vec;
    uint32_t     m_num_pkt;      // number of packet in the vector, for speed can be taken from vec.size()
    uint32_t     m_blk_size;     // the size of each mbuf ( maximum , except the last block */
    uint32_t     m_t_bytes;      // the total bytes in all mbufs 
};


typedef enum { taTX_ACK            =17,   /* tx acked  */
               taRX_BYTES          =19,   /* Rx byte */

} tcp_app_event_bh_enum_t;

typedef uint8_t tcp_app_event_bh_t;


typedef enum { tcTX_BUFFER             =1,   /* send buffer of   CMbufBuffer */
               tcDELAY                 =2,   /* delay some time */
               tcRX_BUFFER             =3,   /* enable/disable Rx counters */
               tcRESET                 =4    /* explicit reset */
} tcp_app_cmd_enum_t;

typedef uint8_t tcp_app_cmd_t;



/* CMD == tcTX_BUFFER*/
struct CTcpAppCmdTxBuffer {
    CMbufBuffer *   m_buf;
};

/* CMD == tcRX_BUFFER */
struct CTcpAppCmdRxBuffer {

    enum {
            rxcmd_NONE      = 0,
            rxcmd_CLEAR     = 1,
            rxcmd_WAIT      = 2,
            rxcmd_DISABLE_RX   =4

    };

    uint32_t        m_flags;
    uint32_t        m_rx_bytes_wm;
};

struct CTcpAppCmdDelay {
    uint64_t     m_usec_delay;
};


/* Commands read-only  */
struct CTcpAppCmd {

    tcp_app_cmd_t     m_cmd;

    union {
        CTcpAppCmdTxBuffer  m_tx_cmd;
        CTcpAppCmdRxBuffer  m_rx_cmd;
        CTcpAppCmdDelay     m_delay_cmd;
    } u;
public:
    void Dump(FILE *fd);
};

typedef std::vector<CTcpAppCmd> tcp_app_cmd_list_t;


class CTcpFlow;
class CTcpPerThreadCtx;

/* Api from application to TCP */
class CTcpAppApi {
public:

    /* get maximum tx queue space */
    virtual uint32_t get_tx_max_space(CTcpFlow * flow)=0;

    /* get space in bytes in the tx queue */
    virtual uint32_t get_tx_sbspace(CTcpFlow * flow)=0;

    /* add bytes to tx queue */
    virtual void tx_sbappend(CTcpFlow * flow,uint32_t bytes)=0;

    virtual uint32_t rx_drain(CTcpFlow * flow)=0;

    virtual void tx_tcp_output(CTcpPerThreadCtx * ctx,
                               CTcpFlow *         flow)=0;

    virtual void delay(uint64_t usec)=0;

};

/* read-only program, many flows point to this program */
class  CTcpAppProgram {

public:
    void add_cmd(CTcpAppCmd & cmd);

    void Dump(FILE *fd);

    int get_size(){
        return (m_cmds.size());
    }

    CTcpAppCmd * get_index(uint32_t index){
        return (&m_cmds[index]);
    }

private:
    tcp_app_cmd_list_t     m_cmds; 
};



typedef enum { te_SOISCONNECTING  =17,   
               te_SOISCONNECTED,      
               te_SOCANTRCVMORE,      
               te_SOABORT     ,       
               te_SOWWAKEUP   ,
               te_SORWAKEUP   ,
               te_SOISDISCONNECTING,
               te_SOISDISCONNECTED 
} tcp_app_events_enum_t;

typedef uint8_t tcp_app_events_t;

std::string get_tcp_app_events_name(tcp_app_events_t event);


typedef enum { te_NONE     =0,
               te_SEND     =17,   
               te_WAIT_RX  =18,      
               te_DELAY    =19,      
} tcp_app_state_enum_t;

typedef uint8_t tcp_app_state_t;


class CTcpApp  {
public:

    /* flags */
    enum {
            taINTERRUPT   = 1,
            taRX_DISABLED = 2
    };



    CTcpApp() {
        m_flow = (CTcpFlow *)0;
        m_ctx =(CTcpPerThreadCtx *)0;
        m_api=(CTcpAppApi *)0;
        m_tx_active =0;
        m_program =(CTcpAppProgram *)0;
        m_flags=0;
        m_state =te_NONE;
        m_debug_id=0;
        m_pad=0;
        m_cmd_index=0;
        m_tx_offset=0;
        m_tx_residue =0;
        m_cmd_rx_bytes=0;
        m_cmd_rx_bytes_wm=0;
    }

    virtual ~CTcpApp(){
    }

public:
    /* inside the Rx */
    void set_interrupt(bool enable){
        if (enable){
            m_flags|=taINTERRUPT;
        }else{
            m_flags&=(~taINTERRUPT);
        }
    }

    bool get_interrupt(){
        return ((m_flags &taINTERRUPT)?true:false);
    }

    void set_rx_disabled(bool disabled){
        if (disabled){
            m_flags|=taRX_DISABLED;
        }else{
            m_flags&=(~taRX_DISABLED);
        }
    }

    bool get_rx_enabled(){
        return ((m_flags &taRX_DISABLED)?false:true);
    }

    bool get_rx_disabled(){
        return ((m_flags &taRX_DISABLED)?true:false);
    }

    

public:

    void set_debug_id(uint8_t id){
        m_debug_id = id;
    }

    uint8_t get_debug_id(){
        return(m_debug_id);
    }

    void set_bh_api(CTcpAppApi * api){
        m_api =api;
    }

    void set_program(CTcpAppProgram * prog){
        m_program = prog;
    }

    void set_flow_ctx(CTcpPerThreadCtx *  ctx,
                      CTcpFlow *          flow){
        m_ctx = ctx;
        m_flow = flow;
    }

public:


    void start(bool interrupt);

    void next();

public:

    void get_by_offset(uint32_t offset,CBufMbufRef & res){
        assert(m_tx_active);
        m_tx_active->get_by_offset(m_tx_offset+offset,res);
    }

    /* application events */
public:


    /* events from TCP stack */

    /* tx queues number of bytes acked */
    virtual int on_bh_tx_acked(uint32_t tx_bytes);

    /* rx bytes */
    virtual int on_bh_rx_bytes(uint32_t rx_bytes,
                               struct rte_mbuf * m_mbuf);

    virtual void on_bh_event(tcp_app_events_t event);

private:

    void process_cmd(CTcpAppCmd * cmd);

    inline void check_rx_condition(){
        if (m_cmd_rx_bytes>= m_cmd_rx_bytes_wm) {
            next();
        }
    }

    void tcp_close();


private:
    CTcpFlow *             m_flow;
    CTcpPerThreadCtx *     m_ctx;
    CTcpAppApi *           m_api; 
    CMbufBuffer *          m_tx_active;
    CTcpAppProgram       * m_program;
    uint8_t                m_flags;
    tcp_app_state_t        m_state;
    uint8_t                m_debug_id;
    uint8_t                m_pad;

    uint32_t               m_cmd_index; /* the index of current command */
    uint32_t               m_tx_offset; /* in case of TX tcTX_BUFFER command, offset into the buffer */
    uint32_t               m_tx_residue; /* how many bytes we can add to the socket queue */

    uint32_t               m_cmd_rx_bytes;
    uint32_t               m_cmd_rx_bytes_wm; /* water mark to check */

};



class CTcpPerThreadCtx;


class   CTcpSockBuf {

public:
    void Create(uint32_t max_size){
        sb_hiwat=max_size;
        sb_cc=0;
        sb_flags=0;
    }
    void Delete(){
    }

    inline bool is_empty(void){
        return (sb_cc==0);
    }

    uint32_t get_sbspace(void){
        return (sb_hiwat-sb_cc);
    }

    void sbappend(uint32_t bytes){
        sb_cc+=bytes;
    }

    void sbdrop_all(struct tcp_socket *so){
        sbdrop(so,sb_cc);
    }

    /* drop x bytes from tail */
    inline void sbdrop(struct tcp_socket *so,
                       int len);

    inline void get_by_offset(struct tcp_socket *so,uint32_t offset,
                              CBufMbufRef & res);

public:

    uint32_t    sb_cc;      /* actual chars in buffer */
    uint32_t    sb_hiwat;   /* max actual char count */
    uint32_t    sb_flags;   /* flags, see below */
};


/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct tcp_socket {
    short   so_options; 
    int     so_error;
    int     so_state;
/*
 * Variables for socket buffering.
 */
    struct  sockbuf so_rcv;
    CTcpSockBuf     so_snd;

    CTcpApp  *      m_app; /* call back pointer */
};


inline void CTcpSockBuf::sbdrop(struct tcp_socket *so,
            int len){
    assert(sb_cc>=len);
    sb_cc-=len;
    sb_flags |=SB_DROPCALL;
    if (len) {
        so->m_app->on_bh_tx_acked((uint32_t)len);
    }
}

inline void CTcpSockBuf::get_by_offset(struct tcp_socket *so,uint32_t offset,
                          CBufMbufRef & res){
    so->m_app->get_by_offset(offset,res);
}



int utl_mbuf_buffer_create_and_fill(CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint32_t size);

int utl_mbuf_buffer_create_and_copy(CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint8_t *p,
                                    uint32_t size);


#endif


