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
#include <string.h>

#define MAGIC0 0xAABBCCDD
#define MAGIC2 0x11223344

struct rte_mempool {
    uint32_t  magic;
    uint32_t  elt_size;
    uint32_t  magic2;
    uint32_t  _id;
    int size;
};




struct rte_mbuf {
    uint32_t magic;
    struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
    void *buf_addr;           /**< Virtual address of segment buffer. */
    uint16_t buf_len;         /**< Length of segment buffer. */
    uint16_t data_off;

    struct rte_mbuf *next;  /**< Next segment of scattered packet. */
    uint16_t data_len;      /**< Amount of data in segment buffer. */

    /* these fields are valid for first segment only */
    uint8_t nb_segs;        /**< Number of segments. */
    uint8_t in_port;        /**< Input port. */
    uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */

    uint32_t magic2;
    uint32_t refcnt_reserved;     /**< Do not use this field */
} ;


typedef struct rte_mbuf  rte_mbuf_t;

typedef struct rte_mempool rte_mempool_t;

#define RTE_PKTMBUF_HEADROOM  0

rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                      unsigned n, 
                                      unsigned elt_size,
                                      unsigned cache_size,
                                      uint32_t _id ,
                                      int socket_id
                                       );

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n, 
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               uint32_t _id ,
                                               int socket_id);

inline unsigned rte_mempool_count(rte_mempool_t  *mp){
    return (10);
}



void rte_pktmbuf_free(rte_mbuf_t *m);

rte_mbuf_t *rte_pktmbuf_alloc(rte_mempool_t *mp);

char *rte_pktmbuf_append(rte_mbuf_t *m, uint16_t len);

char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len);

int rte_pktmbuf_trim(rte_mbuf_t *m, uint16_t len);

void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md);




void rte_pktmbuf_free_seg(rte_mbuf_t *m);

uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value);

rte_mbuf_t * utl_rte_pktmbuf_add_after(rte_mbuf_t *m1,rte_mbuf_t *m2);
rte_mbuf_t * utl_rte_pktmbuf_add_after2(rte_mbuf_t *m1,rte_mbuf_t *m2);

void rte_pktmbuf_dump(const struct rte_mbuf *m, unsigned dump_len);


int rte_mempool_sc_get(struct rte_mempool *mp, void **obj_p);

void rte_mempool_sp_put(struct rte_mempool *mp, void *obj);

inline int rte_mempool_get(struct rte_mempool *mp, void **obj_p){
    return (rte_mempool_sc_get(mp, obj_p));
}

inline void rte_mempool_put(struct rte_mempool *mp, void *obj){
    rte_mempool_sp_put(mp, obj);
}


static inline void *
rte_memcpy(void *dst, const void *src, size_t n)
{
    return (memcpy(dst, src, n));
}



void rte_exit(int exit_code, const char *format, ...);

static inline unsigned
rte_lcore_to_socket_id(unsigned lcore_id){
    return (0);
}

#define rte_pktmbuf_mtod(m, t) ((t)((char *)(m)->buf_addr + (m)->data_off))

/**
 * A macro that returns the length of the packet.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_pkt_len(m) ((m)->pkt_len)

/**
 * A macro that returns the length of the segment.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_data_len(m) ((m)->data_len)


uint64_t rte_rand(void);


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




#define __rte_cache_aligned 


#define CACHE_LINE_SIZE 64
#define RTE_CACHE_LINE_SIZE 64

#define RTE_CACHE_LINE_SIZE (CACHE_LINE_SIZE)

#define SOCKET_ID_ANY 0

#endif
