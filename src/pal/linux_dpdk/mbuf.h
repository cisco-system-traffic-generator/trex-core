#ifndef MBUF_H
#define MBUF_H
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


#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_random.h>

typedef struct rte_mbuf  rte_mbuf_t;

typedef struct rte_mempool rte_mempool_t;

inline void utl_rte_mempool_delete(rte_mempool_t * & pool){
}


rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                      unsigned n, 
                                      unsigned elt_size,
                                      unsigned cache_size,
                                      uint32_t _id,
                                      uint32_t socket_id  );

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n, 
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               uint32_t _id ,
                                               int socket_id);


static inline rte_mbuf_t * utl_rte_pktmbuf_add_after(rte_mbuf_t *m1,rte_mbuf_t *m2){

    rte_mbuf_refcnt_update(m2,1);
    m1->next=m2;

    m1->pkt_len += m2->data_len;
    m1->nb_segs = m2->nb_segs + 1;
    return (m1);
}

static inline rte_mbuf_t * utl_rte_pktmbuf_add_after2(rte_mbuf_t *m1,rte_mbuf_t *m2){

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



#endif
