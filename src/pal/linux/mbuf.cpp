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

void rte_pktmbuf_free_seg(rte_mbuf_t *m) {

    utl_rte_pktmbuf_check(m);

	if (rte_mbuf_refcnt_update(m, -1) == 0) {
        /* if this is an indirect mbuf, then
         *  - detach mbuf
         *  - free attached mbuf segment
         */

        if (RTE_MBUF_INDIRECT(m)) {
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
