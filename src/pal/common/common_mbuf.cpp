/*
Copyright (c) 2016-2016 Cisco Systems, Inc.

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
#include <stdio.h>
#include <mbuf.h>
#include "common_mbuf.h"

/* Dump structure of mbuf chain, without the data */
void
utl_rte_pktmbuf_dump(const struct rte_mbuf *m) {
            while (m) {
                printf("(%d %d %d)", m->pkt_len, m->data_len,
#ifdef TREX_SIM
                       (int)m->refcnt_reserved);
#else
                       (int)m->refcnt_atomic.cnt);
#endif
                if (RTE_MBUF_INDIRECT(m)) {
#ifdef TREX_SIM
                    struct rte_mbuf *md = RTE_MBUF_FROM_BADDR(m->buf_addr);
#else
                    struct rte_mbuf *md = rte_mbuf_from_indirect((struct rte_mbuf *)m);
#endif
                    printf("(direct %d %d %d)", md->pkt_len, md->data_len,
#ifdef TREX_SIM
                           (int)md->refcnt_reserved);
#else
                           (int)md->refcnt_atomic.cnt);
#endif
                }
                m = m->next;
            }
            printf("\n");
}
