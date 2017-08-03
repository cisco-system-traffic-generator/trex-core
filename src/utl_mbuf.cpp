/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#include "utl_mbuf.h"
#include <algorithm>
#include <common/basic_utils.h>



struct rte_mbuf * utl_rte_pktmbuf_cpy(char *p,
                                      struct rte_mbuf *mi,
                                      uint16_t cp_size, 
                                      uint16_t & off){

    while (cp_size) {
        char *md=rte_pktmbuf_mtod(mi, char *);
        assert(mi->data_len > off);
        uint16_t msz=mi->data_len-off;
        uint16_t sz=std::min(msz,cp_size);
        memcpy(p,md+off,sz);
        p+=sz;
        cp_size-=sz;

        if (sz == msz) {
            mi=mi->next;
            if (mi==NULL) {
                /* error */
                return(mi);
            }
            off=0;
        }else{
            off+=sz;
        }
    }
    return(mi);
}


/**
 * Creates a "clone" of the given packet mbuf - allocate new mbuf in size of block_size and chain them 
 * this is to simulate Tx->Rx packet convertion 
 *
 */
struct rte_mbuf * utl_rte_pktmbuf_deepcopy(struct rte_mempool *mp,
                                           struct rte_mbuf *mi){
    struct rte_mbuf *mc, *m;
    uint32_t pktlen;
    uint32_t origin_pktlen;
    uint8_t nseg;

    mc = rte_pktmbuf_alloc(mp);
    if ( mc== NULL){
        return NULL;
    }

    uint16_t nsegsize = rte_pktmbuf_tailroom(mc);

    m = mc; /* root */
    pktlen = mi->pkt_len;
    origin_pktlen = mi->pkt_len;
    
    uint16_t off=0;
        
    nseg = 0;

    while (true) {

        uint16_t cp_size=std::min((uint32_t)nsegsize,pktlen);

        char *p=rte_pktmbuf_append(mc, cp_size);

        if (mi == NULL) {
            goto err;
        }
        mi=utl_rte_pktmbuf_cpy(p,mi,cp_size,off);

        nseg++; /* new */

        pktlen-=cp_size;

        if (pktlen==0) {
            break;  /* finish */
        }else{
            struct rte_mbuf * mt = rte_pktmbuf_alloc(mp);
            if (mt == NULL) {
                goto err;
            }
          mc->next=mt;
          mc=mt;
        }
    }
    m->nb_segs = nseg;
    m->pkt_len = origin_pktlen;
    

    return m;
err:
   rte_pktmbuf_free(m);
   return (NULL); 
}

char * utl_rte_pktmbuf_to_mem(struct rte_mbuf *m){
    char * p=(char *)malloc(m->pkt_len);
    char * op=p;
    uint32_t pkt_len=m->pkt_len;

    while (m) {
        uint16_t seg_len=m->data_len;
        memcpy(p,rte_pktmbuf_mtod(m,char *),seg_len);
        p+=seg_len;
        assert(pkt_len>=seg_len);
        pkt_len-=seg_len;
        m = m->next;
        if (pkt_len==0) {
            assert(m==0);
        }
    }
    return(op);
}


/* return 0   equal
          -1  not equal 

*/          
int utl_rte_pktmbuf_deepcmp(struct rte_mbuf *ma,
                            struct rte_mbuf *mb){

    if (ma->pkt_len != mb->pkt_len) {
        return(-1);
    }
    int res;
    char *pa;
    char *pb;
    pa=utl_rte_pktmbuf_to_mem(ma);
    pb=utl_rte_pktmbuf_to_mem(mb);
    res=memcmp(pa,pb,ma->pkt_len);
    free(pa);
    free(pb);
    return(res);
}


/* add test of buffer with 100 bytes-> 100byte .. deepcopy to 1024 byte buffer */


void  utl_rte_pktmbuf_fill(rte_mbuf_t   * m,
                     uint16_t      b_size,
                     uint8_t  &     start_cnt,
                     int to_rand){

    char *p=rte_pktmbuf_append(m, b_size);
    int i;
    for (i=0; i<(int)b_size; i++) {
        if (to_rand) {
            *p=(uint8_t)(rand()&0xff);
        }else{
            *p=start_cnt++;
        }
        p++;
    }
}



void  utl_rte_pktmbuf_dump_k12(FILE* fp,rte_mbuf_t   * m){
    if (m->nb_segs==1) {
       uint8_t *pkt = rte_pktmbuf_mtod(m, uint8_t*);
       utl_k12_pkt_format(fp,pkt,  m->pkt_len,0);
    }else{
        /* copy the packet */
        char *p;
        p=utl_rte_pktmbuf_to_mem(m);
        utl_k12_pkt_format(fp,p,m->pkt_len,0);
        free(p);
    }
}


char * utl_rte_pktmbuf_mem_fill(uint32_t size, 
                                   int to_rand){
    char *p=(char *)malloc(size);
    char *res=p;

    uint8_t start_cnt=0x1;

    int i;
    for (i=0; i<(int)size; i++) {
        if (to_rand) {
            *p=(uint8_t)(rand()&0xff);
        }else{
            *p=start_cnt++;
        }
        p++;
    }
    return(res);
}

/* convert contiguous buffer to chanin of mbuf in the size of pool  */
struct rte_mbuf *  utl_rte_pktmbuf_mem_to_pkt(char *   buf,
                                              uint32_t size, 
                                              uint16_t mp_blk_size,
                                              struct  rte_mempool *mp){
    uint16_t blk_size= mp_blk_size; 
    rte_mbuf * mr=NULL;
    rte_mbuf * mlast=NULL;
    uint16_t nseg=0;        
    uint32_t pkt_size=size;
    
    while (size>0) {
        uint16_t alloc_size=std::min((uint32_t)blk_size,size);
        rte_mbuf_t   * m= rte_pktmbuf_alloc(mp);
        assert(m);
        nseg++;
        if (mr==NULL) {
            mr=m;
        }
        if (mlast) {
            mlast->next=m;
        }
        mlast=m;
        char *p=(char *)rte_pktmbuf_append(m, alloc_size);
        memcpy(p,buf,alloc_size);
        buf+=alloc_size;
        size-=alloc_size;
    }
    mr->nb_segs = nseg;
    mr->pkt_len = pkt_size;
    return(mr);
}

__attribute__ ((noinline)) int _utl_rte_pktmbuf_trim_ex(struct rte_mbuf *m, 
                                                        uint16_t len){
        /* more than 1 mbufs */
    if (m->pkt_len < len) {
        return(-1);
    }
    uint32_t new_pkt_len = m->pkt_len - len;
    uint32_t sum=0;
    uint16_t nb_segs=1;

    struct rte_mbuf *m2 = (struct rte_mbuf *)m;
    while (m2->next != NULL) {
        if (new_pkt_len <= m2->data_len){
            break;
        }
        new_pkt_len -=m2->data_len;
        sum+=m2->data_len;
        m2 = m2->next;
        nb_segs++;
    }

    m->pkt_len  -= len;
    m->nb_segs  = nb_segs;
    m2->data_len = (uint16_t)(m->pkt_len - sum); /* sum has the prev mbuf sizes*/

    if (m2->next != 0) {
        //we are in the last but there are more mbufs after that 
        rte_pktmbuf_free(m2->next);
        m2->next= NULL;
    }

    return 0;
}

/* check that mbuf is valid */
int utl_rte_pktmbuf_verify(struct rte_mbuf *m){

    uint32_t pkt_len=0;
    uint16_t nb_seg=0;
    struct rte_mbuf *o=m;
    while (m) {
        uint16_t seg_len=m->data_len;
        nb_seg++;
        if (seg_len==0) {
            printf(" SEG has zero data \n");
            return(-1);
        }
        pkt_len+=seg_len;
        if (rte_pktmbuf_mtod(m,char *)==0){
            printf(" SEG has pointer zero \n");
            return(-1);
        }
        m = m->next;
        if (m==NULL) {
            break;
        }
    }
    if (pkt_len != o->pkt_len){
        printf(" total packets length is not valid \n");
        return(-1);
    }
    if (nb_seg != o->nb_segs){
        printf(" #seg is not valid \n");
        return(-1);
    }
    return(0);
}


/* adjust chain of mbufs */
__attribute__ ((noinline)) char *_utl_rte_pktmbuf_adj_ex(struct rte_mbuf  * & m, uint16_t len){

    if (len > m->pkt_len){
        return NULL;
    }
    uint16_t pnb_segs=0;

    /* save original */
    struct rte_mbuf *p = 0;
    struct rte_mbuf *m2 = (struct rte_mbuf *)m;
    while (m2->next != NULL) {
        if (len < m2->data_len){
            break;
        }
        len -=m2->data_len;
        p = m2;
        m2 = m2->next;
        pnb_segs++;
    }

    if (p) {
        /* terminate the mbuf */
        p->next=0;
    }

    m2->data_len -= (uint16_t)(len);
    m2->data_off += (uint16_t)(len);

    m2->pkt_len = (m->pkt_len - len);
    m2->nb_segs =  m->nb_segs -pnb_segs;

    if (m != m2) {
        struct rte_mbuf *o = m;
        rte_pktmbuf_free(o);
        m=m2;
    }
    return (char *)m2->buf_addr + m2->data_off;
}

