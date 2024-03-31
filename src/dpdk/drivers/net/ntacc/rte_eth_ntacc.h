/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __RTE_ETH_NTACC_H__
#define __RTE_ETH_NTACC_H__

#include "nt_compat.h"


#define SEGMENT_LENGTH  (1024*1024)

//#define NTACC_LOCK(a)    { printf(" Req Lock %s(%p) - %u\n", __FILE__, a, __LINE__); rte_spinlock_lock(a); printf(" Got Lock %s(%p) - %u\n", __FILE__, a, __LINE__); }
//#define NTACC_UNLOCK(a)  { rte_spinlock_unlock(a); printf("Unlocked %s(%p) - %u\n", __FILE__, a, __LINE__); }
#define NTACC_LOCK(a)    rte_spinlock_lock(a)
#define NTACC_UNLOCK(a)  rte_spinlock_unlock(a)

struct filter_flow {
  LIST_ENTRY(filter_flow) next;
  uint32_t ntpl_id;
};

struct filter_hash_s {
  LIST_ENTRY(filter_hash_s) next;
  uint64_t rss_hf;
  int priority;
  uint8_t port;
  uint32_t ntpl_id;
};

struct filter_keyset_s {
  LIST_ENTRY(filter_keyset_s) next;
  uint32_t ntpl_id1;
  uint32_t ntpl_id2;
  uint64_t typeMask;
  uint8_t  key;
  uint8_t  port;
  uint8_t nb_queues;
  uint8_t list_queues[256];
};

struct rte_flow {
	LIST_ENTRY(rte_flow) next;
  LIST_HEAD(_filter_flows, filter_flow) ntpl_id;
  uint32_t assign_ntpl_id;
  uint8_t port;
  uint8_t  key;
  uint64_t typeMask;
  uint64_t rss_hf;
  int priority;
  uint8_t nb_queues;
  uint8_t list_queues[256];
};

enum {
  SYM_HASH_DIS_PER_PORT,
  SYM_HASH_ENA_PER_PORT,
};

struct ntacc_rx_queue {
  uint64_t iCnt;
  uint64_t oCnt;
  uint64_t offW;
  uint64_t offR;
  struct NtNetRxHbRing_s ringControl;
  NtNetBuf_t             pSeg;    /* The current segment we are working with */
  NtNetStreamRx_t        pNetRx;
  struct rte_mempool    *mb_pool;
  uint32_t               in_port;
  struct NtNetBuf_s      pkt;     /* The current packet */
#ifdef USE_SW_STAT
  volatile uint64_t      rx_pkts;
  volatile uint64_t      rx_bytes;
  volatile uint64_t      err_pkts;
#endif

  uint32_t                stream_id;
  int                    stream_assigned;
  uint16_t               buf_size;
  int                     enabled;
  uint8_t                local_port;
  uint8_t                tsMultiplier;
  const char             *name;
  const char             *type;
} __rte_cache_aligned;

struct ntacc_tx_queue {
  struct NtNetTxHbRing_s ringControl;
  NtNetStreamTx_t        pNetTx;
#ifdef USE_SW_STAT
  volatile uint64_t      tx_pkts;
  volatile uint64_t      tx_bytes;
#endif
  volatile uint64_t      err_pkts;
  volatile uint16_t     *plock;
  uint32_t               port;
  uint16_t               minTxPktSize;
  uint16_t               maxTxPktSize;
  uint8_t                local_port;
  int                    enabled;
} __rte_cache_aligned;

struct pmd_shared_mem_s {
  pthread_mutex_t mutex;
  int keyset[8][12];
  int key_id;
};

struct version_s {
    int32_t major;
    int32_t minor;
    int32_t patch;
};

struct filter_values_s {
	LIST_ENTRY(filter_values_s) next;
  uint64_t mask;
  const char *layerString;
  uint8_t size;
  uint8_t layer;
  uint8_t offset;
  union {
    struct {
      uint16_t specVal;
      uint16_t maskVal;
      uint16_t lastVal;
    } v16;
    struct {
      uint32_t specVal;
      uint32_t maskVal;
      uint32_t lastVal;
    } v32;
    struct {
      uint64_t specVal;
      uint64_t maskVal;
      uint64_t lastVal;
    } v64;
    struct {
      uint8_t specVal[16];
      uint8_t maskVal[16];
      uint8_t lastVal[16];
    } v128;
  } value;
};

#define NTACC_NAME_LEN (PCI_PRI_STR_SIZE + 20)

struct pmd_internals {
  struct ntacc_rx_queue *rxq;
  struct ntacc_tx_queue *txq;
  uint32_t              nbStreamIDs;
  uint32_t              streamIDOffset;
  uint64_t              rss_hf;
  struct rte_flow       *defaultFlow;
#ifndef USE_SW_STAT
  NtStatStream_t        hStat;
#endif
  NtConfigStream_t      hCfgStream;
  int                   if_index;
  LIST_HEAD(_flows, rte_flow) flows;
  LIST_HEAD(filter_values_t, filter_values_s) filter_values;
  LIST_HEAD(filter_hash_t, filter_hash_s) filter_hash;
  LIST_HEAD(filter_keyset_t, filter_keyset_s) filter_keyset;
  rte_spinlock_t        lock;
  rte_spinlock_t        statlock;
  rte_spinlock_t        configlock;
  uint8_t               port;
  uint8_t               local_port;
  uint8_t               local_port_offset;
  uint8_t               adapterNo;
  uint8_t               nbPortsOnAdapter;
  uint8_t               nbPortsInSystem;
  uint8_t               symHashMode;
  uint8_t               tsMultiplier;
  char                  driverName[128];
  char                  tagName[10];
  char                  name[NTACC_NAME_LEN];
  union Ntfpgaid_u      fpgaid;
  struct version_s      version;
  char                  *ntpl_file;
  int                   shmid;
  key_t                 key;
  uint16_t              minTxPktSize;
  uint16_t              maxTxPktSize;
  pthread_mutexattr_t   psharedm;
  struct pmd_shared_mem_s *shm;
  uint32_t              dropId;
  uint32_t              keyMatcher:1;
  uint32_t              mode2Tx:1;
  uint32_t              mode2Rx:1;
};

enum {
  ACTION_RSS       = 1 << 0,
  ACTION_QUEUE     = 1 << 1,
  ACTION_DROP      = 1 << 2,
  ACTION_FORWARD   = 1 << 3,
  ACTION_HASH      = 1 << 4,
};


struct supportedDriver_s {
   int32_t major;
   int32_t minor;
   int32_t patch;
};

struct supportedAdapters_s {
  uint32_t item:12;
  uint32_t product:16;
  uint32_t ver:8;
  uint32_t rev:8;
  uint32_t build:10;
};

#ifdef USE_EXTERNAL_BUFFER
struct externalBufferInfo_s {
  struct ntacc_rx_queue           *rx_q;
  uint64_t                        offR;
  struct rte_mbuf_ext_shared_info shinfo;
  uint64_t cnt;
};
#endif

int DoNtpl(const char *ntplStr, uint32_t *pNtplID, struct pmd_internals *internals, struct rte_flow_error *error);

extern int ntacc_logtype;

#define PMD_NTACC_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, ntacc_logtype, "%s: " fmt , __func__, ##args)

#endif


