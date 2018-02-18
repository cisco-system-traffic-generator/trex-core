#ifndef COMMON_MBUF_H
#define COMMON_MBUF_H

/*
Copyright (c) 2016-2017 Cisco Systems, Inc.

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
#include <assert.h>

static inline void add_vlan(rte_mbuf_t *m, uint16_t vlan_id) {
    m->ol_flags |= PKT_TX_VLAN_PKT;
    assert(m->l2_len!=0);
    m->vlan_tci = vlan_id;
}

static inline rte_mbuf_t * utl_rte_pktmbuf_add_after2(rte_mbuf_t *m1,rte_mbuf_t *m2){
    utl_rte_pktmbuf_check(m1);
    utl_rte_pktmbuf_check(m2);

    m1->next=m2;
    m1->pkt_len += m2->data_len;
    m1->nb_segs = m2->nb_segs + 1;
    return (m1);
}

static inline rte_mbuf_t * utl_rte_pktmbuf_add_after(rte_mbuf_t *m1,rte_mbuf_t *m2){

    utl_rte_pktmbuf_check(m1);
    utl_rte_pktmbuf_check(m2);

    rte_mbuf_refcnt_update(m2,1);
    m1->next=m2;
    m1->pkt_len += m2->data_len;
    m1->nb_segs = m2->nb_segs + 1;
    return (m1);
}


static inline void utl_rte_pktmbuf_add_last(rte_mbuf_t *m,rte_mbuf_t *m_last){

    //there could be 2 cases supported
    //1. one mbuf
    //2. two mbug where last is indirect

    if ( m->next == NULL ) {
        utl_rte_pktmbuf_add_after2(m,m_last);
    }else{
        m->next->next=m_last;
        m->pkt_len += m_last->data_len;
        m->nb_segs = 3;
    }
}

// Create following m_buf structure:
// Assuming following in input:
// base->indirect
// Creating:
// base -> indirect -> last
// Trimming from indirect the number of bytes in last
static inline rte_mbuf_t * utl_rte_pktmbuf_chain_to_indirect (rte_mbuf_t *base, rte_mbuf_t *indirect
                                                                , rte_mbuf_t *last) {

    indirect->next = last;
    indirect->data_len -= last->data_len;
    assert(indirect->data_len >= 0);
    base->nb_segs = 3;
    indirect->nb_segs = 2;
    last->nb_segs = 1;
    return base;
}

// Create following m_buf structure:
// base -> indirect -> last
// Read only is the direct of indirect.
static inline rte_mbuf_t * utl_rte_pktmbuf_chain_with_indirect (rte_mbuf_t *base, rte_mbuf_t *indirect
                                                  , rte_mbuf_t *read_only, rte_mbuf_t *last) {
    rte_pktmbuf_attach(indirect, read_only);
    base->next = indirect;
    indirect->next = last;
    rte_pktmbuf_refcnt_update(read_only, -1);
    base->nb_segs = 3;
    indirect->nb_segs = 2;
    last->nb_segs = 1;
    return base;
}

// return pointer to consequence buffer containing the last num_bytes from mbuf chain m
// If bytes are already consequence in memory, will return pointer to them.
//   otherwise, will copy last num_bytes into buf, and return pointer to buf
inline uint8_t *utl_rte_pktmbuf_get_last_bytes(const rte_mbuf_t *m, uint16_t num_bytes, uint8_t *buf) {
    const rte_mbuf_t *last = m;
    const rte_mbuf_t *before_last = m;

    assert(m->pkt_len >= num_bytes);

    while (last->next != NULL) {
        before_last = last;
        last = last->next;
    }

    if (last->data_len >= num_bytes) {
        return rte_pktmbuf_mtod(last, uint8_t*) + last->data_len - num_bytes;
    } else {
        bcopy(rte_pktmbuf_mtod(before_last, uint8_t*) + before_last->data_len - (num_bytes - last->data_len)
              , buf, num_bytes - last->data_len);
        bcopy(rte_pktmbuf_mtod(last, uint8_t*), buf + num_bytes - last->data_len, last->data_len);
        return buf;
    }
}

rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                      unsigned n,
                                      unsigned elt_size,
                                      unsigned cache_size,
                                      int socket_id,
                                      bool is_hugepages);

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n,
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               int socket_id,
                                               bool share,
                                               bool is_hugepages);

#endif
