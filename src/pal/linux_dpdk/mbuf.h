#ifndef MBUF_H
#define MBUF_H
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


#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_random.h>
#include <rte_ip.h>

typedef struct rte_mbuf  rte_mbuf_t;
inline void utl_rte_pktmbuf_check(struct rte_mbuf *m) {}
typedef struct rte_mempool rte_mempool_t;

#include "common_mbuf.h"

inline void utl_rte_mempool_delete(rte_mempool_t * & pool){
}

#endif
