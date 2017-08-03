#ifndef UTL_MBUF_H
#define UTL_MBUF_H

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

#include "mbuf.h"
#include <stdlib.h>
#include <stdio.h>

/* 

copy from m + offset -> end into  p 
*/
struct rte_mbuf * utl_rte_pktmbuf_cpy(char *p,
                                      struct rte_mbuf *mi,
                                      uint16_t cp_size, 
                                      uint16_t & off);


/* deep copy mi to a new mbuf allocated from mp pool, free the input mi at the end */
struct rte_mbuf * utl_rte_pktmbuf_deepcopy(struct rte_mempool *mp,
                                           struct rte_mbuf *mi);

/* convert mbuf to contiguous memory */
char * utl_rte_pktmbuf_to_mem(struct rte_mbuf *m);


/* compare two mbufs 
          return 0   equal
          -1  not equal 

*/          
int utl_rte_pktmbuf_deepcmp(struct rte_mbuf *ma,
                            struct rte_mbuf *mb);



void  utl_rte_pktmbuf_fill(rte_mbuf_t   * m,
                         uint16_t      b_size,
                     uint8_t  &     start_cnt,
                     int to_rand);


void  utl_rte_pktmbuf_dump_k12(FILE* fp,rte_mbuf_t   * m);


char * utl_rte_pktmbuf_mem_fill(uint32_t size, 
                                int to_rand);

/* convert contiguous buffer to chanin of mbuf in the size of pool  */
struct rte_mbuf *  utl_rte_pktmbuf_mem_to_pkt(char *   buf,
                                              uint32_t size, 
                                              uint16_t mp_blk_size,
                                              struct  rte_mempool *mp);

__attribute__ ((noinline)) int _utl_rte_pktmbuf_trim_ex(struct rte_mbuf *m, 
                                                        uint16_t len);

inline int utl_rte_pktmbuf_trim_ex(struct rte_mbuf *m, 
                            uint16_t len){
    if (m->nb_segs==1){
        return(rte_pktmbuf_trim(m,len));
    }
    return (_utl_rte_pktmbuf_trim_ex(m, len));
}

__attribute__ ((noinline)) char *_utl_rte_pktmbuf_adj_ex(struct rte_mbuf  * & m, uint16_t len);


inline char *utl_rte_pktmbuf_adj_ex(struct rte_mbuf  * & m, uint16_t len){
    if (len <= m->data_len){
        return(rte_pktmbuf_adj(m,len));
    }
    return(_utl_rte_pktmbuf_adj_ex(m, len));
}


/* check that mbuf is valid */
int utl_rte_pktmbuf_verify(struct rte_mbuf *m);



#endif
