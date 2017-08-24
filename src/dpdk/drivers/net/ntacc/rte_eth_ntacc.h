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

#define SEGMENT_LENGTH  (1024*1024)

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
  uint8_t list_queues[RTE_ETHDEV_QUEUE_STAT_CNTRS];
};

struct rte_flow {
	LIST_ENTRY(rte_flow) next;
  LIST_HEAD(_filter_flows, filter_flow) ntpl_id;
  uint8_t port;
  uint8_t  key;
  uint64_t typeMask;
  uint64_t rss_hf;
  int priority;
};

enum {
  SYM_HASH_DIS_PER_PORT,
  SYM_HASH_ENA_PER_PORT,
};

//#define USE_SW_STAT
struct ntacc_rx_queue {
  NtNetStreamRx_t        pNetRx;
  struct rte_mempool    *mb_pool;
  uint16_t               buf_size;
  NtNetBuf_t             pSeg;    /* The current segment we are working with */
  struct NtNetBuf_s      pkt;     /* The current packet */
#ifdef USE_SW_STAT
  volatile unsigned long rx_pkts;
  volatile unsigned long rx_bytes;
#endif

  uint32_t               stream_id;
  uint8_t                in_port;
  const char             *name;
  const char             *type;
  int                    enabled;
};

//#define DIRECT_TX_RING_CONTROL
struct ntacc_tx_queue {
#ifdef DIRECT_TX_RING_CONTROL
  struct NtNetTxHbRing_s ringControl;
#endif
  NtNetStreamTx_t        pNetTx;
  volatile unsigned long err_pkts;
#ifdef USE_SW_STAT
  volatile unsigned long tx_pkts;
  volatile unsigned long tx_bytes;
#endif
  volatile uint16_t     *plock;
  uint32_t               port;
  uint8_t                local_port;
  int                    enabled;
};

struct pmd_shared_mem_s {
  pthread_mutex_t mutex;
  int keyset[8][12];
};

struct version_s {
    int32_t major;
    int32_t minor;
    int32_t patch;
};


struct pmd_internals {
  struct ntacc_rx_queue rxq[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  struct ntacc_tx_queue txq[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint32_t              nbStreamIDs;
  uint32_t              streamIDOffset;
  uint64_t              rss_hf;
#ifndef USE_SW_STAT
  NtStatStream_t        hStat;
#endif
  int                   if_index;
  LIST_HEAD(_flows, rte_flow) flows;
  rte_spinlock_t        lock;
  uint8_t               port;
  uint8_t               local_port;
  uint8_t               adapterNo;
  uint8_t               nbPortsOnAdapter;
  uint8_t               nbPortsInSystem;
  uint8_t               symHashMode;
  char                  driverName[128];
  char                  tagName[10];
  char                  name[PCI_PRI_STR_SIZE];
  union Ntfpgaid_u      fpgaid;
  struct version_s      version;
  char                  *ntpl_file;
  int                   shmid;
  key_t                 key;
  pthread_mutexattr_t   psharedm;
  struct pmd_shared_mem_s *shm;
};



int DoNtpl(const char *ntplStr, NtNtplInfo_t *ntplInfo, struct pmd_internals *internals);

#endif


