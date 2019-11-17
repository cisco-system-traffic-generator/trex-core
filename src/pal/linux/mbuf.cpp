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

/*
 * This file is compiled only for simulation.
 * Since in simulation, we do not use DPDK libraries, all mbuf functions needed for simulation are duplictated here.
 *   with some small changes.
 */

#include "mbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pal_utl.h"

void rte_pktmbuf_detach(struct rte_mbuf *m);


rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n,
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               int socket_id,
                                               bool share,
                                               bool is_hugepages){
    rte_mempool_t * p=new rte_mempool_t();
    assert(p);
    p->elt_size =elt_size;
    p->size=n;
    p->priv_size=0;
    p->magic=MAGIC0;
    p->magic2=MAGIC2;
    return p;
}

rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                      unsigned n,
                                      unsigned elt_size,
                                      unsigned cache_size,
                                      int socket_id,
                                      bool is_hugepages){

    rte_mempool_t * p=new rte_mempool_t();
    assert(p);
    p->size=n;
    p->elt_size =elt_size;
    p->priv_size=sizeof(rte_mbuf_t); /* each pool has less memory */
    p->magic=MAGIC0;
    p->magic2=MAGIC2;
    return p;
}

void utl_rte_mempool_delete(rte_mempool_t * & pool){
    if (pool) {
        delete pool;
        pool=0;
    }
}

static inline uint16_t _rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value){
    utl_rte_pktmbuf_check(m);
    m->refcnt_reserved = (uint16_t)(m->refcnt_reserved + value);
    assert(m->refcnt_reserved >= 0);
    return m->refcnt_reserved;
}


#ifdef TREX_MBUF_SIM_LOCAL

uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value)
{
    if (m->m_core_locality==RTE_MBUF_CORE_LOCALITY_CONST) {
        assert(m->refcnt_reserved==1);
        return(m->refcnt_reserved);
    }else{
        return(_rte_mbuf_refcnt_update(m,value));
    }
}

#else


uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value)
{
    return (_rte_mbuf_refcnt_update(m,value));
}

#endif


void rte_pktmbuf_reset(struct rte_mbuf *m)
{
    utl_rte_pktmbuf_check(m);
    m->next = NULL;
    m->pkt_len = 0;
    m->nb_segs = 1;
    m->in_port = 0xff;
    m->ol_flags = 0;
    m->tso_segsz=0;
    m->l2_len=0;
    m->l3_len=0;
    m->l4_len=0;
    m->vlan_tci=0;

    #if RTE_PKTMBUF_HEADROOM > 0
    m->data_off = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
            RTE_PKTMBUF_HEADROOM : m->buf_len;
    #else
    m->data_off = RTE_PKTMBUF_HEADROOM ;
    #endif

    m->data_len = 0;
    m->m_core_locality=0;
}


rte_mbuf_t *rte_pktmbuf_alloc(rte_mempool_t *mp){

    uint16_t buf_len;

    utl_rte_check(mp);

    buf_len = mp->elt_size ;

    rte_mbuf_t *m =(rte_mbuf_t *)malloc(buf_len );
    assert(m);

    m->magic  = MAGIC0;
    m->magic2 = MAGIC2;
    m->pool   = mp;
    m->refcnt_reserved = 1;
    m->buf_len    = buf_len-(sizeof(rte_mbuf_t)+RTE_PKTMBUF_HEADROOM);
    m->buf_addr   =(char *)((char *)m+sizeof(rte_mbuf_t)+RTE_PKTMBUF_HEADROOM) ;

    rte_pktmbuf_reset(m);

    return (m);
}

rte_mbuf_t *rte_pktmbuf_alloc_no_assert(rte_mempool_t *mp){

    uint16_t buf_len;

    utl_rte_check(mp);

    buf_len = mp->elt_size ;

    rte_mbuf_t *m =(rte_mbuf_t *)malloc(buf_len );
    if ( m ) {
        m->magic  = MAGIC0;
        m->magic2 = MAGIC2;
        m->pool   = mp;
        m->refcnt_reserved = 1;
        m->buf_len    = buf_len-(sizeof(rte_mbuf_t)+RTE_PKTMBUF_HEADROOM);
        m->buf_addr   =(char *)((char *)m+sizeof(rte_mbuf_t)+RTE_PKTMBUF_HEADROOM) ;

        rte_pktmbuf_reset(m);
    }

    return (m);
}

void rte_pktmbuf_free_seg(rte_mbuf_t *m) {

    utl_rte_pktmbuf_check(m);

	if (rte_mbuf_refcnt_update(m, -1) == 0) {
        /* if this is an indirect mbuf, then
         *  - detach mbuf
         *  - free attached mbuf segment
         */

        if (RTE_MBUF_CLONED(m)) {
            struct rte_mbuf *md = RTE_MBUF_FROM_BADDR(m->buf_addr);
            rte_pktmbuf_detach(m);
            if (rte_mbuf_refcnt_update(md, -1) == 0)
                free(md);
        }
        free(m);
    }
}

void rte_pktmbuf_free(rte_mbuf_t *m){

    rte_mbuf_t *m_next;

    utl_rte_pktmbuf_check(m);

    while (m != NULL) {
        m_next = m->next;
        rte_pktmbuf_free_seg(m);
        m = m_next;
    }
}

static inline struct rte_mbuf *rte_pktmbuf_lastseg(struct rte_mbuf *m)
{
    struct rte_mbuf *m2 = (struct rte_mbuf *)m;
    utl_rte_pktmbuf_check(m);


    while (m2->next != NULL)
        m2 = m2->next;
    return m2;
}



char *rte_pktmbuf_append(struct rte_mbuf *m, uint16_t len)
{
    void *tail;
    struct rte_mbuf *m_last;
    utl_rte_pktmbuf_check(m);


    m_last = rte_pktmbuf_lastseg(m);
    if (len > rte_pktmbuf_tailroom(m_last))
        return NULL;

    tail = (char*) m_last->buf_addr + m_last->data_len;
    m_last->data_len = (uint16_t)(m_last->data_len + len);
    m->pkt_len  = (m->pkt_len + len);
    return (char*) tail;
}


char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len)
{
    utl_rte_pktmbuf_check(m);

    if (len > m->data_len)
        return NULL;

    m->data_len = (uint16_t)(m->data_len - len);
    m->data_off += len;
    m->pkt_len  = (m->pkt_len - len);
    return (char *)m->buf_addr + m->data_off;
}


int rte_pktmbuf_trim(struct rte_mbuf *m, uint16_t len)
{
    struct rte_mbuf *m_last;
        utl_rte_pktmbuf_check(m);

    m_last = rte_pktmbuf_lastseg(m);
    if (len > m_last->data_len)
        return -1;

    m_last->data_len = (uint16_t)(m_last->data_len - len);
    m->pkt_len  = (m->pkt_len - len);
    return 0;
}

int rte_pktmbuf_is_contiguous(const struct rte_mbuf *m)
{
    return (m->nb_segs == 1);
}

static void
rte_pktmbuf_hexdump(const void *buf, unsigned int len)
{
    unsigned int i, out, ofs;
    const unsigned char *data = (unsigned char *)buf;
#define LINE_LEN 80
    char line[LINE_LEN];

    printf("  dump data at 0x%p, len=%u\n", data, len);
    ofs = 0;
    while (ofs < len) {
        out = snprintf(line, LINE_LEN, "  %08X", ofs);
        for (i = 0; ofs+i < len && i < 16; i++)
            out += snprintf(line+out, LINE_LEN - out, " %02X",
                    data[ofs+i]&0xff);
        for (; i <= 16; i++)
            out += snprintf(line+out, LINE_LEN - out, "   ");
        for (i = 0; ofs < len && i < 16; i++, ofs++) {
            unsigned char c = data[ofs];
            if (!isascii(c) || !isprint(c))
                c = '.';
            out += snprintf(line+out, LINE_LEN - out, "%c", c);
        }
        printf("%s\n", line);
    }
}


int _rte_mempool_sc_get(struct rte_mempool *mp, void **obj_p){
    utl_rte_check(mp);
    uint16_t buf_len;
    buf_len = mp->elt_size ;
    *obj_p=(void *)::malloc(buf_len);
    return (0);
}

void _rte_mempool_sp_put(struct rte_mempool *mp, void *obj){
    free(obj);
}


void rte_exit(int exit_code, const char *format, ...){
    exit(exit_code);
}



void
rte_pktmbuf_dump(const struct rte_mbuf *m, unsigned dump_len)
{
    unsigned int len;
    unsigned nb_segs;


    printf("dump mbuf at 0x%p, phys=0x%p, buf_len=%u\n",
           m, m->buf_addr, (unsigned)m->buf_len);
    printf("  pkt_len=%u,  nb_segs=%u, "
           "in_port=%u\n", m->pkt_len,
           (unsigned)m->nb_segs, (unsigned)m->in_port);
    nb_segs = m->nb_segs;

    while (m && nb_segs != 0) {

        printf("  segment at 0x%p, data=0x%p, data_len=%u\n",
               m, m->buf_addr, (unsigned)m->data_len);
        len = dump_len;
        if (len > m->data_len)
            len = m->data_len;

        if (len != 0)
            rte_pktmbuf_hexdump(rte_pktmbuf_mtod(m,char *), len);
        dump_len -= len;
        m = m->next;
        nb_segs --;
    }
}

void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md)
{

    rte_mbuf_refcnt_update(md, 1);
    mi->buf_addr = md->buf_addr;
    mi->buf_len = md->buf_len;
    mi->data_off = md->data_off;

    mi->next = NULL;
    mi->data_len = md->data_len;
    mi->pkt_len  = mi->data_len;
    mi->ol_flags = mi->ol_flags | IND_ATTACHED_MBUF;
    mi->nb_segs = 1;
}

void rte_pktmbuf_detach(struct rte_mbuf *m)
{
    const struct rte_mempool *mp = m->pool;
    void *buf = RTE_MBUF_TO_BADDR(m);
    uint32_t buf_len = mp->elt_size - sizeof(*m);

    m->buf_addr = buf;
    m->buf_len = (uint16_t)buf_len;

    #if RTE_PKTMBUF_HEADROOM > 0
    m->data_off = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
            RTE_PKTMBUF_HEADROOM : m->buf_len;
    #else
    m->data_off = RTE_PKTMBUF_HEADROOM ;
    #endif


    m->data_len = 0;
	m->ol_flags = 0;
}

uint64_t rte_rand(void){
    return ( rand() );
}

/* taken from DPDK to simulate hardware checksum calculation */

#define DEV_TX_OFFLOAD_VLAN_INSERT 0x00000001
#define DEV_TX_OFFLOAD_IPV4_CKSUM  0x00000002
#define DEV_TX_OFFLOAD_UDP_CKSUM   0x00000004
#define DEV_TX_OFFLOAD_TCP_CKSUM   0x00000008

struct tcp_hdr {
	uint16_t src_port;  /**< TCP source port. */
	uint16_t dst_port;  /**< TCP destination port. */
	uint32_t sent_seq;  /**< TX data sequence number. */
	uint32_t recv_ack;  /**< RX data acknowledgement sequence number. */
	uint8_t  data_off;  /**< Data offset. */
	uint8_t  tcp_flags; /**< TCP flags */
	uint16_t rx_win;    /**< RX flow control window. */
	uint16_t cksum;     /**< TCP checksum. */
	uint16_t tcp_urp;   /**< TCP urgent pointer, if any. */
} __attribute__((__packed__));


struct udp_hdr {
	uint16_t src_port;    /**< UDP source port. */
	uint16_t dst_port;    /**< UDP destination port. */
	uint16_t dgram_len;   /**< UDP datagram length */
	uint16_t dgram_cksum; /**< UDP datagram checksum */
} __attribute__((__packed__));

struct ipv4_hdr {
	uint8_t  version_ihl;		/**< version and header length */
	uint8_t  type_of_service;	/**< type of service */
	uint16_t total_length;		/**< length of packet */
	uint16_t packet_id;		/**< packet ID */
	uint16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	uint16_t hdr_checksum;		/**< header checksum */
	uint32_t src_addr;		/**< source address */
	uint32_t dst_addr;		/**< destination address */
} __attribute__((__packed__));

struct ipv6_hdr {
	uint32_t vtc_flow;     /**< IP version, traffic class & flow label. */
	uint16_t payload_len;  /**< IP packet length - includes sizeof(ip_header). */
	uint8_t  proto;        /**< Protocol, next header. */
	uint8_t  hop_limits;   /**< Hop limits. */
	uint8_t  src_addr[16]; /**< IP address of source host. */
	uint8_t  dst_addr[16]; /**< IP address of destination host(s). */
} __attribute__((__packed__));


static inline uint32_t
__rte_raw_cksum(const void *buf, size_t len, uint32_t sum)
{
	/* workaround gcc strict-aliasing warning */
	uintptr_t ptr = (uintptr_t)buf;
	typedef uint16_t __attribute__((__may_alias__)) u16_p;
	const u16_p *u16_buf = (const u16_p *)ptr;

	while (len >= (sizeof(*u16_buf) * 4)) {
		sum += u16_buf[0];
		sum += u16_buf[1];
		sum += u16_buf[2];
		sum += u16_buf[3];
		len -= sizeof(*u16_buf) * 4;
		u16_buf += 4;
	}
	while (len >= sizeof(*u16_buf)) {
		sum += *u16_buf;
		len -= sizeof(*u16_buf);
		u16_buf += 1;
	}

	/* if length is in odd bytes */
	if (len == 1)
		sum += *((const uint8_t *)u16_buf) & PAL_NTOHS(0xff00);

	return sum;
}

static inline uint16_t
__rte_raw_cksum_reduce(uint32_t sum)
{
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	return (uint16_t)sum;
}


static inline uint16_t
rte_raw_cksum(const void *buf, size_t len)
{
	uint32_t sum;

	sum = __rte_raw_cksum(buf, len, 0);
	return __rte_raw_cksum_reduce(sum);
}


#define rte_pktmbuf_mtod_offset(m, t, o)	\
	((t)((char *)(m)->buf_addr + (m)->data_off + (o)))


static inline int
rte_raw_cksum_mbuf(const struct rte_mbuf *m, uint32_t off, uint32_t len,
	uint16_t *cksum)
{
	const struct rte_mbuf *seg;
	const char *buf;
	uint32_t sum, tmp;
	uint32_t seglen, done;

	/* easy case: all data in the first segment */
	if (off + len <= rte_pktmbuf_data_len(m)) {
		*cksum = rte_raw_cksum(rte_pktmbuf_mtod_offset(m,
				const char *, off), len);
		return 0;
	}

	if (unlikely(off + len > rte_pktmbuf_pkt_len(m)))
		return -1; /* invalid params, return a dummy value */

	/* else browse the segment to find offset */
	seglen = 0;
	for (seg = m; seg != NULL; seg = seg->next) {
		seglen = rte_pktmbuf_data_len(seg);
		if (off < seglen)
			break;
		off -= seglen;
	}
	seglen -= off;
	buf = rte_pktmbuf_mtod_offset(seg, const char *, off);
	if (seglen >= len) {
		/* all in one segment */
		*cksum = rte_raw_cksum(buf, len);
		return 0;
	}

	/* hard case: process checksum of several segments */
	sum = 0;
	done = 0;
	for (;;) {
		tmp = __rte_raw_cksum(buf, seglen, 0);
		if (done & 1) {
			tmp = __rte_raw_cksum_reduce(tmp);
			tmp = PAL_NTOHS((uint16_t)tmp);
                }
		sum += tmp;
		done += seglen;
		if (done == len)
			break;
		seg = seg->next;
		buf = rte_pktmbuf_mtod(seg, const char *);
		seglen = rte_pktmbuf_data_len(seg);
		if (seglen > len - done)
			seglen = len - done;
	}

	*cksum = __rte_raw_cksum_reduce(sum);
	return 0;
}

static inline uint16_t rte_ipv4_header_len(const struct ipv4_hdr *ipv4_hdr){
   return((ipv4_hdr->version_ihl &0xf)<<2);
}




static inline uint16_t
rte_ipv4_cksum(const struct ipv4_hdr *ipv4_hdr)
{
	uint16_t cksum;
	cksum = rte_raw_cksum(ipv4_hdr, rte_ipv4_header_len(ipv4_hdr));
	return (cksum == 0xffff) ? cksum : (uint16_t)~cksum;
}



uint16_t rte_ipv4_phdr_cksum(const struct ipv4_hdr *ipv4_hdr, uint64_t ol_flags)
{
	struct ipv4_psd_header {
		uint32_t src_addr; /* IP address of source host. */
		uint32_t dst_addr; /* IP address of destination host. */
		uint8_t  zero;     /* zero. */
		uint8_t  proto;    /* L4 protocol type. */
		uint16_t len;      /* L4 length. */
	} psd_hdr;

	psd_hdr.src_addr = ipv4_hdr->src_addr;
	psd_hdr.dst_addr = ipv4_hdr->dst_addr;
	psd_hdr.zero = 0;
	psd_hdr.proto = ipv4_hdr->next_proto_id;
	if (ol_flags & PKT_TX_TCP_SEG) {
		psd_hdr.len = 0;
	} else {
		psd_hdr.len =  PAL_NTOHS(
			(uint16_t)( PAL_NTOHS(ipv4_hdr->total_length)
				- rte_ipv4_header_len(ipv4_hdr)));
	}
	return rte_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

uint16_t rte_ipv6_phdr_cksum(const struct ipv6_hdr *ipv6_hdr, uint64_t ol_flags)
{
	uint32_t sum;
	struct {
		uint32_t len;   /* L4 length. */
		uint32_t proto; /* L4 protocol - top 3 bytes must be zero */
	} psd_hdr;

	psd_hdr.proto = (uint32_t)(ipv6_hdr->proto << 24);
	if (ol_flags & PKT_TX_TCP_SEG) {
		psd_hdr.len = 0;
	} else {
		psd_hdr.len = ipv6_hdr->payload_len;
	}

	sum = __rte_raw_cksum(ipv6_hdr->src_addr,
		sizeof(ipv6_hdr->src_addr) + sizeof(ipv6_hdr->dst_addr),
		0);
	sum = __rte_raw_cksum(&psd_hdr, sizeof(psd_hdr), sum);
	return __rte_raw_cksum_reduce(sum);
}



static void tx_offload_csum(struct rte_mbuf *m, uint64_t tx_offload) {

        /* assume that l2_len and l3_len in rte_mbuf updated properly */
        uint16_t csum = 0, csum_start = m->l2_len + m->l3_len;

        /* assume that IP pseudo header checksum was already caclulated */
        if (rte_raw_cksum_mbuf(m, csum_start, rte_pktmbuf_pkt_len(m) - csum_start, &csum) < 0)
            return;
        csum = (csum != 0xffff) ? ~csum: csum;

        if (((m->ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM) &&
            !(tx_offload & DEV_TX_OFFLOAD_TCP_CKSUM)) {
            struct tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(m, struct tcp_hdr *, csum_start);

            tcp_hdr->cksum = csum;
            m->ol_flags &= ~PKT_TX_L4_MASK;     /* PKT_TX_L4_NO_CKSUM is 0 */
        } else if (((m->ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM) &&
                    !(tx_offload & DEV_TX_OFFLOAD_UDP_CKSUM)) {
            struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(m, struct udp_hdr *, csum_start);

            udp_hdr->dgram_cksum = csum;
            m->ol_flags &= ~PKT_TX_L4_MASK;     /* PKT_TX_L4_NO_CKSUM is 0 */
        }

        if ((m->ol_flags & PKT_TX_IPV4) && (m->ol_flags & PKT_TX_IP_CKSUM) &&
            !(tx_offload & DEV_TX_OFFLOAD_IPV4_CKSUM)) {
            struct ipv4_hdr *iph = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *, m->l2_len);

            if (!iph->hdr_checksum) {
                iph->hdr_checksum = rte_ipv4_cksum(iph);
                m->ol_flags &= ~PKT_TX_IP_CKSUM;
            }
        }
}

void hw_checksum_sim(struct rte_mbuf *m){

    if (((m->ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM) ||
        ((m->ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM)) {
        tx_offload_csum(m, 0 );
    }
}






#ifdef ONLY_A_TEST


#define CONST_NB_MBUF  2048
#define CONST_MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)


void test_pkt_mbuf(){

    rte_mempool_t * mp1=utl_rte_mempool_create("big-const",
                                          CONST_NB_MBUF,
                                          CONST_MBUF_SIZE,
                                          32);
    rte_mbuf_t * m1 = rte_pktmbuf_alloc(mp1);
    rte_mbuf_t * m2 = rte_pktmbuf_alloc(mp1);

    char *p=rte_pktmbuf_append(m1, 10);
    int i;

    for (i=0; i<10;i++) {
        p[i]=i;
    }

    p=rte_pktmbuf_append(m2, 10);

    for (i=0; i<10;i++) {
        p[i]=0x55+i;
    }

    rte_pktmbuf_dump(m1, m1->pkt_len);
    rte_pktmbuf_dump(m2, m1->pkt_len);

    rte_pktmbuf_free(m1);
    rte_pktmbuf_free(m2);
}




#endif
