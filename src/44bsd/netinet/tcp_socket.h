#ifndef _TCP_SOCKET_H_
#define _TCP_SOCKET_H_

struct sockbuf {
    u_int   sb_cc;          /* (a) chars in buffer */
    u_int   sb_hiwat;       /* (a) max actual char count */
    short   sb_flags;       /* (a) flags, see above */
    short   sb_state;       /* (a) socket state on sockbuf */
#define SBS_CANTRCVMORE         0x0020  /* can't receive more data from peer */
};

#define sbused(sb)  ((sb)->sb_cc)
#define sbavail(sb) ((sb)->sb_cc)
static inline long sbspace(struct sockbuf *sb) {
    return sb->sb_hiwat - sb->sb_cc;
}


struct socket {
    short so_options;           /* (b) from socket call, see socket.h */
/* Linux asm-generic/socket.h header defines the different value for setsockopt(2) */
#if defined(SO_KEEPALIVE)
#undef SO_DEBUG
#undef SO_KEEPALIVE
#else
/* experimental! to prevent re-definition of SO_KEEPALIVE from Linux header */
#define __ASM_GENERIC_SOCKET_H
#endif
#define SO_DEBUG        0x00000001      /* turn on debugging info recording */
#define SO_KEEPALIVE    0x00000008      /* keep connections alive */
    int so_error;               /* (f) error affecting connection */
    int so_state;               /* (b) internal state flags SS_* */
#define SS_NOFDREF              0x0001  /* no file table ref any more */
#define SS_ISCONNECTED          0x0002  /* socket connected to a peer */
#define SS_ISCONNECTING         0x0004  /* in process of connecting to peer */
#define SS_ISDISCONNECTING      0x0008  /* in process of disconnecting */
#define SS_ISDISCONNECTED       0x2000  /* socket disconnected from peer */
    struct sockbuf so_rcv, so_snd;
};

#ifdef __cplusplus
extern "C" {
#endif

#define sbreserve_locked(sb,cc,so,td)   sbreserve(sb,cc)
void sbreserve(struct sockbuf *sb, u_int cc);
/* for so_rcv */
#define sbappendstream_locked(sb,m,flags,so)   sbappend(sb,m,flags,so)
void sbappend(struct sockbuf *sb, struct mbuf *m, int flags, struct socket *so);
/* for so_snd */
void sbdrop(struct sockbuf *sb, int len, struct socket *so);

void sorwakeup(struct socket *so);
void sowwakeup(struct socket *so);
void soisconnected(struct socket *so);
void soisdisconnected(struct socket *so);
void socantrcvmore(struct socket *so);  /* socantrcvmore need to set SBS_CANTRCVMORE to so_rcv.sb_state */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !_TCP_SOCKET_H_ */
