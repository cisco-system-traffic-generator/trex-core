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

#define NUM_FLOW_QUEUES 256
struct rte_flow {
	LIST_ENTRY(rte_flow) next;
  LIST_HEAD(_filter_flows, filter_flow) ntpl_id;
  uint8_t port;
  uint8_t  key;
  uint64_t typeMask;
  uint64_t rss_hf;
  int priority;
  uint8_t nb_queues;
  uint8_t list_queues[NUM_FLOW_QUEUES];
};

enum {
  SYM_HASH_DIS_PER_PORT,
  SYM_HASH_ENA_PER_PORT,
};

struct ntacc_rx_queue {
  NtNetBuf_t             pSeg;    /* The current segment we are working with */
  NtNetStreamRx_t        pNetRx;
  struct rte_mempool    *mb_pool;
  uint32_t               cmbatch;
  uint32_t               in_port;
  struct NtNetBuf_s      pkt;     /* The current packet */
#ifdef USE_SW_STAT
  volatile uint64_t      rx_pkts;
  volatile uint64_t      rx_bytes;
  volatile uint64_t      err_pkts;
#endif

  uint16_t               buf_size;
  uint32_t               stream_id;
  uint8_t                local_port;
  const char             *name;
  const char             *type;
  int                    enabled;
} __rte_cache_aligned;

struct ntacc_tx_queue {
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

#define NTACC_NAME_LEN (PCI_PRI_STR_SIZE + 10)

struct pmd_internals {
  struct ntacc_rx_queue rxq[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  struct ntacc_tx_queue txq[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  uint32_t              nbStreamIDs;
  uint32_t              streamIDOffset;
  uint64_t              rss_hf;
  struct rte_flow       *defaultFlow;
#ifndef USE_SW_STAT
  NtStatStream_t        hStat;
#endif
  int                   if_index;
  LIST_HEAD(_flows, rte_flow) flows;
  LIST_HEAD(filter_values_t, filter_values_s) filter_values;
  LIST_HEAD(filter_hash_t, filter_hash_s) filter_hash;
  LIST_HEAD(filter_keyset_t, filter_keyset_s) filter_keyset;
  rte_spinlock_t        lock;
  rte_spinlock_t        statlock;
  uint8_t               port;
  uint8_t               local_port;
  uint8_t               local_port_offset;
  uint8_t               adapterNo;
  uint8_t               nbPortsOnAdapter;
  uint8_t               nbPortsInSystem;
  uint8_t               symHashMode;
  char                  driverName[128];
  char                  tagName[10];
  char                  name[NTACC_NAME_LEN];
  union Ntfpgaid_u      fpgaid;
  struct version_s      version;
  char                  *ntpl_file;
  int                   shmid;
  key_t                 key;
  pthread_mutexattr_t   psharedm;
  struct pmd_shared_mem_s *shm;
};

struct batch_ctrl {
	void      *orig_buf_addr;
	void      *queue;
	NtNetBuf_t pSeg;
};

int DoNtpl(const char *ntplStr, NtNtplInfo_t *ntplInfo, struct pmd_internals *internals);

	/**
	 * Insert a NTPL string into the NTPL filtercode.
	 *
	 * See struct rte_flow_item_ntpl.
	 */
#define	RTE_FLOW_ITEM_TYPE_NTPL 255

/**
 * RTE_FLOW_ITEM_TYPE_NTPL
 *
 * Insert a NTPL string into the NTPL filtercode.
 *
 * tunnel defines how following commands are treated.
 * Setting tunnel=RTE_FLOW_NTPL_TUNNEL makes the following commands to be treated
 * as inner tunnel commands
 *
 * 1. The string is inserted into the filter string with tunnel=RTE_FLOW_NTPL_NO_TUNNEL:
 *
 *    assign[xxxxxxxx]=(Layer3Protocol==IPV4) and and "Here is the ntpl item" and port==0 and Key(KDEF4)==4
 *
 * 2. The string is inserted into the filter string with tunnel=RTE_FLOW_NTPL_TUNNEL:
 *
 *    assign[xxxxxxxx]=(InnerLayer3Protocol==IPV4) and and "Here is the ntpl item" and port==0 and Key(KDEF4)==4
 *
 * Note: When setting tunnel=RTE_FLOW_NTPL_TUNNEL the Layer3Protocol command is changed to InnerLayer3Protocol,
 * 			 now matching the inner layers in stead of the outer layers.
 *
 * Note: When setting tunnel=RTE_FLOW_NTPL_TUNNEL, the commands used before the RTE_FLOW_ITEM_TYPE_NTPL will
 *       match the outer layers and commands used after will match the inner layers.
 *
 * The ntpl item string must have the right syntax in order to prevent a
 * syntax error and it must break the rest of the ntpl string in order
 * to prevent a ntpl error.
 */
struct rte_flow_item_ntpl {
	const char *ntpl_str;
	int tunnel;
};

enum {
	RTE_FLOW_NTPL_NO_TUNNEL,
	RTE_FLOW_NTPL_TUNNEL,
};

#ifndef __cplusplus
static const struct rte_flow_item_ntpl rte_flow_item_ntpl_mask = {
	.ntpl_str = NULL,
	.tunnel   = RTE_FLOW_NTPL_NO_TUNNEL,
};
#endif


#endif


