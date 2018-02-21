#include "mbuf.h"

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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


rte_mempool_t * utl_rte_mempool_create(const char  *name,
                                       unsigned n, 
                                       unsigned elt_size,
                                       unsigned cache_size,
                                       int socket_id,
                                       bool is_hugepages){
    char buffer[100];
    sprintf(buffer,"%s-%d",name,socket_id);
    unsigned flags = is_hugepages ? 0 : MEMPOOL_F_NO_PHYS_CONTIG;
    //printf("Creating pool: %s, count: %u, size: %u, flags: %u\n", name, n, elt_size, flags);

    rte_mempool_t *  res=
        rte_mempool_create(buffer, n,
                   elt_size, cache_size,
                   sizeof(struct rte_pktmbuf_pool_private),
                   rte_pktmbuf_pool_init, NULL,
                   rte_pktmbuf_init, NULL,
                   socket_id, flags);
    if (res == NULL){
        if (is_hugepages) {
            printf(" ERROR there is not enough huge-pages memory in your system \n");
        } else {
            printf(" ERROR there is not enough free memory in your system \n");
        }
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool %s\n",name);
    }
    return res;
}

rte_mempool_t * utl_rte_mempool_create_non_pkt(const char  *name,
                                               unsigned n, 
                                               unsigned elt_size,
                                               unsigned cache_size,
                                               int socket_id,
                                               bool share,
                                               bool is_hugepages){
    char buffer[100];
    sprintf(buffer,"%s-%d",name,socket_id);

    unsigned flags=0; /* shared by default */
    if (!share) {
        flags = (MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET);
    }

    if (!is_hugepages) {
        flags |= MEMPOOL_F_NO_PHYS_CONTIG;
    }

    //printf("Creating non-pkt-pool: %s, count: %u, size: %u, flags: %u\n", name, n, elt_size, flags);
    rte_mempool_t *  res=
        rte_mempool_create(buffer, n,
                           elt_size, 
                           cache_size,
                           0,
                           NULL, NULL,
                           NULL, NULL,
                           socket_id, flags);
    if (res == NULL) {
        if (is_hugepages) {
            printf(" ERROR there is not enough huge-pages memory in your system \n");
        } else {
            printf(" ERROR there is not enough free memory in your system \n");
        }
        rte_exit(EXIT_FAILURE, "Cannot init nodes mbuf pool %s\n",name);
    }
    return res;
}



/* make sure we didn't enlarge the MBUF */
static_assert( (sizeof(struct rte_mbuf) == 128), "size of MBUF must be 128" );


