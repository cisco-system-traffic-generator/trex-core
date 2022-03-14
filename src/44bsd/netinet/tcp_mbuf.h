#ifndef _TCP_MBUF_H_
#define _TCP_MBUF_H_


struct mbuf;

#ifdef __cplusplus
extern "C" {
#endif

struct mbuf* m_adj_fix(struct mbuf *, int, int);
void m_trim(struct mbuf *, int);
void m_freem(struct mbuf *);
uint32_t m_pktlen(struct mbuf *);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif  /* !_TCP_MBUF_H_ */
