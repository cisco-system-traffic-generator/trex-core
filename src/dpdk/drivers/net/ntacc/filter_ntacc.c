/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <nt.h>

#include "rte_eth_ntacc.h"
#include "filter_ntacc.h"

#define NON_ZERO2(a)  (*a != 0 || *(a + 1) != 0)
#define NON_ZERO4(a)  (*a != 0 || *(a + 1) != 0 || *(a + 2) != 0 || *(a + 3) != 0)
#define NON_ZERO6(a)  (a[0] != 0  || a[1] != 0  || a[2] != 0  || a[3] != 0 || a[4] != 0  || a[5] != 0)
#define NON_ZERO16(a) (a[0] != 0  || a[1] != 0  || a[2] != 0  || a[3] != 0 ||  \
                       a[4] != 0  || a[5] != 0  || a[6] != 0  || a[7] != 0 ||  \
                       a[8] != 0  || a[9] != 0  || a[10] != 0 || a[11] != 0 || \
                       a[12] != 0 || a[13] != 0 || a[14] != 0 || a[15] != 0)

#define IPV4_ADDRESS(a) ((const char *)&a)[3] & 0xFF, ((const char *)&a)[2] & 0xFF, \
                        ((const char *)&a)[1] & 0xFF, ((const char *)&a)[0] & 0xFF

#define IPV6_ADDRESS(a) a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF,    \
                        a[4] & 0xFF, a[5] & 0xFF, a[6] & 0xFF, a[7] & 0xFF,    \
                        a[8] & 0xFF, a[9] & 0xFF, a[10] & 0xFF, a[11] & 0xFF,  \
                        a[12] & 0xFF, a[13] & 0xFF, a[14] & 0xFF, a[15] & 0xFF

#define MAC_ADDRESS2(a) a[5] & 0xFF, a[4] & 0xFF, a[3] & 0xFF, a[2] & 0xFF, a[1] & 0xFF, a[0] & 0xFF,    \
                        a[11] & 0xFF, a[10] & 0xFF, a[9] & 0xFF, a[8] & 0xFF, a[7] & 0xFF, a[6] & 0xFF,  \
                        a[12] & 0xFF, a[13] & 0xFF, a[14] & 0xFF, a[15] & 0xFF

#define MAC_ADDRESS(a)  a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF, a[4] & 0xFF, a[5] & 0xFF

#define MAC_ADDRESS_SWAP(a,b)  {b[5]=a[0];b[4]=a[1];b[3]=a[2];b[2]=a[3];b[1]=a[4];b[0]=a[5];}

#if 0
#define PRINT_IPV4(a, b) { uint32_t c = b; printf("%s: %d.%d.%d.%d\n", a, IPV4_ADDRESS(c)); }
#else
#define PRINT_IPV4(a, b)
#endif

static LIST_HEAD(filter_values_t, filter_values_s) filter_values;
static LIST_HEAD(filter_hash_t, filter_hash_s) filter_hash;
static LIST_HEAD(filter_keyset_t, filter_keyset_s) filter_keyset;

#define CHECK8(a, b)   (a != NULL && (a->b != 0 && a->b != 0xFF))
#define CHECK16(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFF))
#define CHECK32(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFFFFFF))
#define CHECK64(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFFFFFFFFFFFFFF))
#define CHECKIPV6(a)   _CheckArray(a, 16)
#define CHECKETHER(a)  _CheckArray(a, 6)

static inline int _CheckArray(const uint8_t *addr, uint8_t len)
{
  int i;
  for (i = 0; i < len; i++) {
    if (addr[i] != 0 && addr[i] != 0xFF) {
      return 1;
    }
  }
  return 0;
}

void pushNtplID(struct rte_flow *flow, uint32_t ntplId)
{
  struct filter_flow *filter = malloc(sizeof(struct filter_flow));
  if (!filter) {
    RTE_LOG(ERR, PMD, "Memory allocation failed. Filter clean up is not possible\n");
  }
  else {
    memset(filter, 0, sizeof(struct filter_flow));
    filter->ntpl_id = ntplId;
    LIST_INSERT_HEAD(&flow->ntpl_id, filter, next);
  }
}

void DeleteHash(uint64_t rss_hf, uint8_t port, int priority, struct pmd_internals *internals) {
  NtNtplInfo_t ntplInfo;
  char ntpl_buf[21];
  struct filter_hash_s *pHash;

  LIST_FOREACH(pHash, &filter_hash, next) {
    if (pHash->rss_hf == rss_hf && pHash->port == port && pHash->priority == priority) {
      LIST_REMOVE(pHash, next);
      snprintf(ntpl_buf, 20, "delete=%d", pHash->ntpl_id);
      DoNtpl(ntpl_buf, &ntplInfo, internals);
      RTE_LOG(DEBUG, PMD, "Deleting Hash filter: %s\n", ntpl_buf);
      free(pHash);
    }
  }
}

static void pushHash(uint32_t ntpl_id, uint64_t rss_hf, uint8_t port, int priority)
{
  struct filter_hash_s *pHash = malloc(sizeof(struct filter_hash_s));
  if (!pHash) {
    RTE_LOG(ERR, PMD, "Memory allocation failed. Filter clean up is not possible\n");
  }
  else {
    pHash->ntpl_id = ntpl_id;
    pHash->port = port;
    pHash->priority = priority;
    pHash->rss_hf = rss_hf;
    LIST_INSERT_HEAD(&filter_hash, pHash, next);
  }
}

static int FindHash(uint64_t rss_hf, uint8_t port, int priority) {
  struct filter_hash_s *pHash;
  LIST_FOREACH(pHash, &filter_hash, next) {
    if (pHash->rss_hf == rss_hf && pHash->port == port && pHash->priority == priority) {
      return 1;
    }
  }
  return 0;
}

#define TMP_BSIZE 200
#define PRINT_HASH(a,b) { if (PrintHash(a, priority, internals, rss_hf, b) != 0)  return -1; }
static int PrintHash(const char *str, int priority, struct pmd_internals *internals, uint64_t rss_hf, uint8_t tuple)
{
  NtNtplInfo_t ntplInfo;
  char tmpBuf[TMP_BSIZE + 1];

  const char *ptrTuple = "hashroundrobin";

  switch (internals->symHashMode) {
  case SYM_HASH_DIS_PER_PORT:
    switch (tuple) {
    case 0x02:
      ptrTuple = "hash2Tuple";
      break;
    case 0x05:
      ptrTuple = "hash5Tuple";
      break;
    case 0x06:
      ptrTuple = "hash5TupleSCTP";
      break;
    case 0x12:
      ptrTuple = "hashInner2Tuple";
      break;
    case 0x15:
      ptrTuple = "hashInner5Tuple";
      break;
    }
    break;
  default:
  case SYM_HASH_ENA_PER_PORT:
    switch (tuple) {
    case 0x02:
      ptrTuple = "hash2TupleSorted";
      break;
    case 0x05:
      ptrTuple = "hash5TupleSorted";
      break;
    case 0x06:
      ptrTuple = "hash5TupleSCTPSorted";
      break;
    case 0x12:
      ptrTuple = "hashInner2TupleSorted";
      break;
    case 0x15:
      ptrTuple = "hashInner5TupleSorted";
      break;
    }
    break;
  }

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  snprintf(tmpBuf, TMP_BSIZE, str, priority, internals->port, internals->tagName, ptrTuple);
#pragma GCC diagnostic pop
  if (DoNtpl(tmpBuf, &ntplInfo, internals) != 0) {
    return -1;
  }
  pushHash(ntplInfo.ntplId, rss_hf, internals->port, priority);
  return 0;
}

/**
 * Create the hash filter from the DPDK hash function.
 */
int CreateHash(uint64_t rss_hf, struct pmd_internals *internals, struct rte_flow *flow, int priority)
{
  if (rss_hf == 0) {
    RTE_LOG(ERR, PMD, "No HASH function is selected. Ignoring hash.\n");
    return 0;
  }

  // These hash functions is not supported and will cause an error
  if ((rss_hf & ETH_RSS_L2_PAYLOAD) ||
      (rss_hf & ETH_RSS_PORT)       ||
      (rss_hf & ETH_RSS_VXLAN)      ||
      (rss_hf & ETH_RSS_GENEVE)     ||
      (rss_hf & ETH_RSS_NVGRE)) {
    RTE_LOG(ERR, PMD, "One of the selected HASH functions is not supported\n");
    return -1;
  }

  flow->port = internals->port;
  flow->rss_hf = rss_hf;
  flow->priority = priority;

  if (FindHash(rss_hf, internals->port, priority)) {
    // Hash is already programmed
    return 0;
  }

  /*****************************/
  /* Inner UDP hash mode setup */
  /*****************************/
  if ((rss_hf & ETH_RSS_INNER_IPV4_UDP) || (rss_hf & ETH_RSS_INNER_IPV6_UDP)) {
    if ((rss_hf & ETH_RSS_INNER_IPV4_UDP) && (rss_hf & ETH_RSS_INNER_IPV6_UDP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IP;InnerLayer4Type=UDP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV4_UDP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV4;InnerLayer4Type=UDP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV6_UDP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV6;InnerLayer4Type=UDP;tag=%s]=%s", 0x15);
    }
  }
  /*****************************/
  /* Inner TCP hash mode setup */
  /*****************************/
  if ((rss_hf & ETH_RSS_INNER_IPV4_TCP) || (rss_hf & ETH_RSS_INNER_IPV6_TCP)) {
    if ((rss_hf & ETH_RSS_INNER_IPV4_TCP) && (rss_hf & ETH_RSS_INNER_IPV6_TCP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IP;InnerLayer4Type=TCP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV4_TCP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV4;InnerLayer4Type=TCP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV6_TCP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV6;InnerLayer4Type=TCP;tag=%s]=%s", 0x15);
    }
  }
  /******************************/
  /* Inner SCTP hash mode setup */
  /******************************/
  if ((rss_hf & ETH_RSS_INNER_IPV4_SCTP) || (rss_hf & ETH_RSS_INNER_IPV6_SCTP)) {
    if ((rss_hf & ETH_RSS_INNER_IPV4_SCTP) && (rss_hf & ETH_RSS_INNER_IPV6_SCTP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IP;InnerLayer4Type=SCTP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV4_SCTP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV4;InnerLayer4Type=SCTP;tag=%s]=%s", 0x15);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV6_SCTP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV6;InnerLayer4Type=SCTP;tag=%s]=%s", 0x15);
    }
  }
  /*****************************/
  /* Outer UDP hash mode setup */
  /*****************************/
  if ((rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) || (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)) {
    if ((rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) && (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IP;Layer4Type=UDP;tag=%s]=%s", 0x05);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV4;Layer4Type=UDP;tag=%s]=%s", 0x05);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV6;Layer4Type=UDP;tag=%s]=%s", 0x05);
    }
  }
  /*****************************/
  /* Outer TCP hash mode setup */
  /*****************************/
  if ((rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) || (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)) {
    if ((rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) && (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IP;Layer4Type=TCP;tag=%s]=%s", 0x05);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV4;Layer4Type=TCP;tag=%s]=%s", 0x05);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV6;Layer4Type=TCP;tag=%s]=%s", 0x05);
    }
  }
  /******************************/
  /* Outer SCTP hash mode setup */
  /******************************/
  if ((rss_hf & ETH_RSS_NONFRAG_IPV4_SCTP) || (rss_hf & ETH_RSS_NONFRAG_IPV6_SCTP)) {
    if ((rss_hf & ETH_RSS_NONFRAG_IPV4_SCTP) && (rss_hf & ETH_RSS_NONFRAG_IPV6_SCTP)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IP;Layer4Type=SCTP;tag=%s]==%s", 0x06);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV4_SCTP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV4;Layer4Type=SCTP;tag=%s]=%s", 0x06);
    }
    else if (rss_hf & ETH_RSS_NONFRAG_IPV6_SCTP) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV6;Layer4Type=SCTP;tag=%s]=%s", 0x06);
    }
  }
  /****************************/
  /* Inner IP hash mode setup */
  /****************************/
  if (((rss_hf & ETH_RSS_INNER_IPV4) || (rss_hf & ETH_RSS_INNER_IPV6)) || (rss_hf & ETH_RSS_INNER_IPV4_OTHER) || (rss_hf & ETH_RSS_INNER_IPV6_OTHER)) {
    if (((rss_hf & ETH_RSS_INNER_IPV4) && (rss_hf & ETH_RSS_INNER_IPV6)) ||
        ((rss_hf & ETH_RSS_INNER_IPV4_OTHER) && (rss_hf & ETH_RSS_INNER_IPV6)) ||
        ((rss_hf & ETH_RSS_INNER_IPV4_OTHER) && (rss_hf & ETH_RSS_INNER_IPV6_OTHER)) ||
        ((rss_hf & ETH_RSS_INNER_IPV4) && (rss_hf & ETH_RSS_INNER_IPV6_OTHER))) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IP;tag=%s]=%s", 0x12);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV4) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV4;tag=%s]=%s", 0x12);
    }
    else if (rss_hf & ETH_RSS_INNER_IPV6) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;InnerLayer3Type=IPV6;tag=%s]=%s", 0x12);
    }
  }
  /****************************/
  /* Outer IP hash mode setup */
  /****************************/
  if ((rss_hf & ETH_RSS_IPV4) || (rss_hf & ETH_RSS_IPV6) || (rss_hf & ETH_RSS_NONFRAG_IPV4_OTHER) || (rss_hf & ETH_RSS_NONFRAG_IPV6_OTHER)) {
    if (((rss_hf & ETH_RSS_IPV4) && (rss_hf & ETH_RSS_IPV6)) ||
        ((rss_hf & ETH_RSS_NONFRAG_IPV4_OTHER) && (rss_hf & ETH_RSS_NONFRAG_IPV6_OTHER)) ||
        ((rss_hf & ETH_RSS_NONFRAG_IPV4_OTHER) && (rss_hf & ETH_RSS_IPV6)) ||
        ((rss_hf & ETH_RSS_IPV6) && (rss_hf & ETH_RSS_NONFRAG_IPV6_OTHER))) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IP;tag=%s]=%s", 0x02);
    }
    else if ((rss_hf & ETH_RSS_IPV4) || (rss_hf & ETH_RSS_NONFRAG_IPV4_OTHER)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV4;tag=%s]=%s", 0x02);
    }
    else if ((rss_hf & ETH_RSS_IPV6)  || (rss_hf & ETH_RSS_NONFRAG_IPV6_OTHER)) {
      PRINT_HASH("Hashmode[priority=%u;port=%u;Layer3Type=IPV6;tag=%s]=%s", 0x02);
    }
  }
  return 0;
}

#if 0
/**
 * Get a keyset value from the keyset pool.
 * Used by the keymatcher command
 */
static int GetKeysetValue(struct pmd_internals *internals)
{
  int i;

  if (internals->adapterNo >= 8) {
    return -1;
  }
  pthread_mutex_lock(&internals->shm->mutex);
  for (i = 0; i < 12; i++) {
    printf(">>>>>>>>>>>>>>>>>> Get key set adapter: %u - Key %u=%u\n", internals->adapterNo, i, internals->shm->keyset[internals->adapterNo][i]);
    if (internals->shm->keyset[internals->adapterNo][i] == 0) {
      internals->shm->keyset[internals->adapterNo][i] = 1;
      pthread_mutex_unlock(&internals->shm->mutex);
      return (i + 3);
    }
  }
  pthread_mutex_unlock(&internals->shm->mutex);
  return -1;
}

/**
 * Return a keyset value to the keyset pool.
 */
int ReturnKeysetValue(struct pmd_internals *internals, int value)
{
  if (internals->adapterNo >= 8 || value < 3 || value > 15) {
    return -1;
  }
  pthread_mutex_lock(&internals->shm->mutex);
  internals->shm->keyset[internals->adapterNo][value - 3] = 0;
  pthread_mutex_unlock(&internals->shm->mutex);
  return 0;
}
#else
/**
 * Get a keyset value from the keyset pool.
 * Used by the keymatcher command
 */
static int GetKeysetValue(struct pmd_internals *internals)
{
  int i;

  if (internals->adapterNo >= 8) {
    return -1;
  }
  pthread_mutex_lock(&internals->shm->mutex);
  for (i = 0; i < 12; i++) {
    if (internals->shm->keyset[0][i] == 0) {
      internals->shm->keyset[0][i] = 1;
      pthread_mutex_unlock(&internals->shm->mutex);
      return (i + 3);
    }
  }
  pthread_mutex_unlock(&internals->shm->mutex);
  return -1;
}

/**
 * Return a keyset value to the keyset pool.
 */
int ReturnKeysetValue(struct pmd_internals *internals, int value)
{
  if (internals->adapterNo >= 8 || value < 3 || value > 15) {
    return -1;
  }
  pthread_mutex_lock(&internals->shm->mutex);
  internals->shm->keyset[0][value - 3] = 0;
  pthread_mutex_unlock(&internals->shm->mutex);
  return 0;
}
#endif

/**
 * Create the stream ID part of the NTPL assign command.
 *
 * The stream ID is created form the number of queues as a range
 * or if only one queue as a single value
 */
void CreateStreamid(char *ntpl_buf, struct pmd_internals *internals, uint32_t nb_queues, uint8_t *list_queues)
{
  bool range = true;
  char buf[21];
  uint32_t i;

  if (nb_queues > 1) {
    // We need to check whether we can set it up as a range or list
    for (i = 0; i < nb_queues - 1; i++) {
      if (list_queues[i] != (list_queues[i + 1] - 1)) {
        // The queues are not contigous, so we need to use a list
        range = false;
        break;
      }
    }
  }
  else {
    range = false;
  }

  strcat(ntpl_buf, "streamid=");
  if (range) {
    snprintf(buf, 20, "(%u..%u)", internals->rxq[0].stream_id, internals->rxq[nb_queues - 1].stream_id);
    strcat(ntpl_buf, buf);
  }
  else {
    for (i = 0; i < nb_queues; i++) {
      snprintf(buf, 20, "%u", internals->rxq[list_queues[i]].stream_id);
      strcat(ntpl_buf, buf);
      if (i < nb_queues - 1) {
        strcat(ntpl_buf, ",");
      }
    }
  }
}

enum layer_e {
  LAYER2,
  LAYER3,
  LAYER4,
  VLAN,
  IP,
};

/**
 * Get the layer NTPL keyword. Either an outer or an inner
 * version.
 */
static const char *GetLayer(enum layer_e layer, bool tunnel)
{
  switch (layer) {
  case LAYER2:
    if (tunnel)
      return "InnerLayer2Header";
    else
      return "Layer2Header";
  case LAYER3:
  case IP:
    if (tunnel)
      return "InnerLayer3Header";
    else
      return "Layer3Header";
  case LAYER4:
    if (tunnel)
      return "InnerLayer4Header";
    else
      return "Layer4Header";
  case VLAN:
    if (tunnel)
      return "InnerFirstVLAN";
    else
      return "FirstVLAN";
  }
  return "UNKNOWN";
}

static void InsertFilterValues(struct filter_values_s *pInsertFilterValues)
{
  struct filter_values_s *pFilter_values;
  struct filter_values_s *pLast_value;
  uint32_t insertVal;
  uint32_t sortVal;

  if (LIST_EMPTY(&filter_values)) {
    LIST_INSERT_HEAD(&filter_values, pInsertFilterValues, next);
    return;
  }

  insertVal = pInsertFilterValues->layer +
              (((uint32_t)pInsertFilterValues->offset << 16) & 0x00FF0000) +
              (((uint32_t)pInsertFilterValues->size << 8) & 0x0000FF00);
  LIST_FOREACH(pFilter_values, &filter_values, next) {
    sortVal = pFilter_values->layer +
              (((uint32_t)pFilter_values->offset << 16) & 0x00FF0000) +
              (((uint32_t)pFilter_values->size << 8) & 0x0000FF00);
    if (sortVal > insertVal) {
      LIST_INSERT_BEFORE(pFilter_values, pInsertFilterValues, next);
      return;
    }
    pLast_value = pFilter_values;
  }
  LIST_INSERT_AFTER(pLast_value, pInsertFilterValues, next);
}

/**
 * Create the NTPL filter expression part.
 */
static int SetFilter(int size,
                     uint64_t mask,
                     int offset,
                     bool tunnel,
                     enum layer_e layer,
                     const void *specVal,
                     const void *maskVal,
                     const void *lastVal)
{
  struct filter_values_s *pFilter_values = malloc(sizeof(struct filter_values_s));
  if (!pFilter_values) {
    return -1;
  }
  memset(pFilter_values, 0, sizeof(struct filter_values_s));

  // Store the filter values;
  pFilter_values->offset = offset;
  pFilter_values->size = size;
  pFilter_values->layerString = GetLayer(layer, tunnel);
  pFilter_values->layer = layer;
  pFilter_values->mask = mask;

  // Create the keylist depending on the size of the search value
  switch (size)
  {
  case 16:
    if (lastVal) {
      pFilter_values->value.v16.lastVal = *((const uint16_t *)lastVal);
      pFilter_values->value.v16.specVal = *((const uint16_t *)specVal);
    }
    else if (maskVal) {
      pFilter_values->value.v16.maskVal = *((const uint16_t *)maskVal);
      pFilter_values->value.v16.specVal = *((const uint16_t *)specVal);
    }
    else {
      pFilter_values->value.v16.specVal = *((const uint16_t *)specVal);
    }
    break;
  case 32:
    if (lastVal) {
      pFilter_values->value.v32.lastVal = *((const uint32_t *)lastVal);
      pFilter_values->value.v32.specVal = *((const uint32_t *)specVal);
    }
    else if (maskVal) {
      pFilter_values->value.v32.maskVal = *((const uint32_t *)maskVal);
      pFilter_values->value.v32.specVal = *((const uint32_t *)specVal);
    }
    else {
      pFilter_values->value.v32.specVal = *((const uint32_t *)specVal);
    }
    break;
  case 64:
    if (lastVal) {
      pFilter_values->value.v64.lastVal = *((const uint64_t *)lastVal);
      pFilter_values->value.v64.specVal = *((const uint64_t *)specVal);
    }
    else if (maskVal) {
      pFilter_values->value.v64.maskVal = *((const uint64_t *)maskVal);
      pFilter_values->value.v64.specVal = *((const uint64_t *)specVal);
    }
    else {
      pFilter_values->value.v64.specVal = *((const uint64_t *)specVal);
    }
    break;
  case 128:
    if (lastVal) {
      const char *vLast = (const char *)lastVal;
      const char *vSpec = (const char *)specVal;
      memcpy(pFilter_values->value.v128.lastVal, vLast, 16);
      memcpy(pFilter_values->value.v128.specVal, vSpec, 16);
    }
    else if (maskVal) {
      const char *vMask = (const char *)maskVal;
      const char *vSpec = (const char *)specVal;
      memcpy(pFilter_values->value.v128.maskVal, vMask, 16);
      memcpy(pFilter_values->value.v128.specVal, vSpec, 16);
    }
    else {
      const char *vSpec = (const char *)specVal;
      memcpy(pFilter_values->value.v128.specVal, vSpec, 16);
    }
    break;
  }
  InsertFilterValues(pFilter_values);
  return 0;
}

void DeleteKeyset(int key, struct pmd_internals *internals) {
  NtNtplInfo_t ntplInfo;
  char ntpl_buf[21];
  struct filter_keyset_s *key_set;

  LIST_FOREACH(key_set, &filter_keyset, next) {
    if (key_set->key == key) {
      LIST_REMOVE(key_set, next);
      snprintf(ntpl_buf, 20, "delete=%d", key_set->ntpl_id2);
      DoNtpl(ntpl_buf, &ntplInfo, internals);
      snprintf(ntpl_buf, 20, "delete=%d", key_set->ntpl_id1);
      DoNtpl(ntpl_buf, &ntplInfo, internals);
      free(key_set);
      return;
    }
  }
}

static int FindKeyset(uint64_t typeMask, uint8_t *plist_queues, uint8_t nb_queues, uint8_t port)
{
  struct filter_keyset_s *key_set;
  int match = 0;
  int i;

  LIST_FOREACH(key_set, &filter_keyset, next) {
    if (key_set->typeMask == typeMask && nb_queues == key_set->nb_queues && key_set->port == port) {
      match = 0;
      for (i = 0; i < nb_queues; i++) {
        if (plist_queues[i] == key_set->list_queues[i]) {
          match++;
        }
      }
      if (match == nb_queues) {
        return key_set->key;
      }
    }
  }
  return 0;
}

int CreateOptimizedFilter(char *ntpl_buf,
                          struct pmd_internals *internals,
                          struct rte_flow *flow,
                          bool *fc,
                          uint64_t typeMask,
                          uint8_t *plist_queues,
                          uint8_t nb_queues,
                          bool *reuse,
                          struct color_s *pColor)
{
  NtNtplInfo_t *pNtplInfo = NULL;
  struct filter_values_s *pFilter_values;
  int key;
  int iRet = 0;
  bool first = true;
  char *filter_buffer1 = NULL;
  char *filter_buffer2 = NULL;
  char *filter_buffer3 = NULL;
  int i;

  if (LIST_EMPTY(&filter_values)) {
    return 0;
  }
  pNtplInfo = malloc(sizeof(NtNtplInfo_t));
  if (!pNtplInfo) {
    iRet = -1;
    RTE_LOG(ERR, PMD, "Allocating memory failed\n");
    goto Errors;
  }

  filter_buffer1 = malloc(NTPL_BSIZE + 1);
  if (!filter_buffer1) {
    iRet = -1;
    RTE_LOG(ERR, PMD, "Allocating memory failed\n");
    goto Errors;
  }

  filter_buffer2 = malloc(NTPL_BSIZE + 1);
  if (!filter_buffer2) {
    iRet = -1;
    RTE_LOG(ERR, PMD, "Allocating memory failed\n");
    goto Errors;
  }

  filter_buffer3 = malloc(NTPL_BSIZE + 1);
  if (!filter_buffer3) {
    iRet = -1;
    RTE_LOG(ERR, PMD, "Allocating memory failed\n");
    goto Errors;
  }

  first = true;
  if (LIST_EMPTY(&filter_keyset) || ((key = FindKeyset(typeMask, plist_queues, nb_queues, internals->port)) == 0)) {
    struct filter_keyset_s *key_set = malloc(sizeof(struct filter_keyset_s));
    if (!key_set) {
      iRet = -1;
      RTE_LOG(ERR, PMD, "Allocating memory failed\n");
      goto Errors;
    }
    key = GetKeysetValue(internals);
    if (key < 0) {
      free(key_set);
      iRet = -1;
      RTE_LOG(ERR, PMD, "Internal error: Illegal key set value returned\n");
      goto Errors;
    }

    key_set->key = key;
    key_set->typeMask = typeMask;

    LIST_FOREACH(pFilter_values, &filter_values, next) {
      if (first) {
        if (pColor->valid) {
          snprintf(filter_buffer3, NTPL_BSIZE,
                   "KeyType[name=KT%u;Access=partial;Bank=0;colorinfo=true;tag=%s]={", key, internals->tagName);
        }
        else {
          snprintf(filter_buffer3, NTPL_BSIZE,
                  "KeyType[name=KT%u;Access=partial;Bank=0;tag=%s]={", key, internals->tagName);
        }
        snprintf(filter_buffer2, NTPL_BSIZE,
                 "KeyDef[name=KDEF%u;KeyType=KT%u;tag=%s]=(", key, key, internals->tagName);
        first=false;
      }
      else {
        snprintf(&filter_buffer3[strlen(filter_buffer3)], NTPL_BSIZE - strlen(filter_buffer3) - 1, ",");
        snprintf(&filter_buffer2[strlen(filter_buffer2)], NTPL_BSIZE - strlen(filter_buffer2) - 1, ",");
      }

      snprintf(&filter_buffer3[strlen(filter_buffer3)], NTPL_BSIZE - strlen(filter_buffer3) - 1, "%u", pFilter_values->size);

      if (pFilter_values->size == 128 && pFilter_values->layer == LAYER2) {
        // This is an ethernet address
        snprintf(&filter_buffer2[strlen(filter_buffer2)], NTPL_BSIZE - strlen(filter_buffer2) - 1,
                "{0xFFFFFFFFFFFFFFFFFFFFFFFF00000000:%s[%u]/%u}", pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
      else {
        if (pFilter_values->mask != 0) {
          snprintf(&filter_buffer2[strlen(filter_buffer2)],  NTPL_BSIZE - strlen(filter_buffer2) - 1,
                   "{0x%llX:%s[%u]/%u}", (const long long unsigned int)pFilter_values->mask, pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
        }
        else {
          snprintf(&filter_buffer2[strlen(filter_buffer2)],  NTPL_BSIZE - strlen(filter_buffer2) - 1,
                  "%s[%u]/%u", pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
        }
      }
    }
    snprintf(&filter_buffer3[strlen(filter_buffer3)],  NTPL_BSIZE - strlen(filter_buffer3) - 1, "}");
    snprintf(&filter_buffer2[strlen(filter_buffer2)],  NTPL_BSIZE - strlen(filter_buffer2) - 1, ")");

    if (DoNtpl(filter_buffer3, pNtplInfo, internals)) {
      free(key_set);
      iRet = -1;
      goto Errors;
    }
    key_set->ntpl_id1 = pNtplInfo->ntplId;

    if (DoNtpl(filter_buffer2, pNtplInfo, internals)) {
      free(key_set);
      iRet = -1;
      goto Errors;
    }
    key_set->ntpl_id2 = pNtplInfo->ntplId;

    for (i = 0; i < nb_queues; i++) {
      key_set->list_queues[i] = plist_queues[i];
    }
    key_set->nb_queues = nb_queues;
    key_set->port = internals->port;
    LIST_INSERT_HEAD(&filter_keyset, key_set, next);
    *reuse = false;
  }
  else {
    *reuse = true;
  }

  flow->key = key;
  first = true;
  LIST_FOREACH(pFilter_values, &filter_values, next) {
    if (first) {
      if (pColor->valid) {
        snprintf(filter_buffer1, NTPL_BSIZE,
                 "KeyList[KeySet=%u;KeyType=KT%u;color=%u;tag=%s]=(", key, key, pColor->color, internals->tagName);
      }
      else {
        snprintf(filter_buffer1, NTPL_BSIZE,
                "KeyList[KeySet=%u;KeyType=KT%u;tag=%s]=(", key, key, internals->tagName);
      }
      first=false;
    }
    else {
      snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1, ",");
    }

    switch (pFilter_values->size)
    {
    case 16:
      if (pFilter_values->value.v16.lastVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "(0x%04X..0x%04X)", pFilter_values->value.v16.specVal, pFilter_values->value.v16.lastVal);
      }
      else if (pFilter_values->value.v16.maskVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "{0x%04X:0x%04X}", pFilter_values->value.v16.maskVal, pFilter_values->value.v16.specVal);
      }
      else {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "0x%04X", pFilter_values->value.v16.specVal);
      }
      break;
    case 32:
      if (pFilter_values->value.v32.lastVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "(0x%08X..0x%08X)", pFilter_values->value.v32.specVal, pFilter_values->value.v32.lastVal);
      }
      else if (pFilter_values->value.v32.maskVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "{0x%08X:0x%08X}", pFilter_values->value.v32.maskVal, pFilter_values->value.v32.specVal);
      }
      else {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "0x%08X", pFilter_values->value.v32.specVal);
      }
      break;
    case 64:
      if (pFilter_values->value.v64.lastVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "(0x%016llX..0x%016llX)", (long long unsigned int)pFilter_values->value.v64.specVal,
                                           (long long unsigned int)pFilter_values->value.v64.lastVal);
      }
      else if (pFilter_values->value.v64.maskVal) {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                 "{0x%016llX:0x%016llX}", (long long unsigned int)pFilter_values->value.v64.maskVal,
                                          (long long unsigned int)pFilter_values->value.v64.specVal);
      }
      else {
        snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                "0x%016llX", (long long unsigned int)pFilter_values->value.v64.specVal);
      }
      break;
    case 128:
      if (pFilter_values->layer == LAYER2) {
        if (NON_ZERO16(pFilter_values->value.v128.lastVal)) {
          snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                    "(0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X.."
                    "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X)",
                    MAC_ADDRESS2(pFilter_values->value.v128.specVal), MAC_ADDRESS2(pFilter_values->value.v128.lastVal));
        }
        else if (NON_ZERO16(pFilter_values->value.v128.maskVal)) {
          snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                   "{0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X:"
                   "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X}",
                   MAC_ADDRESS2(pFilter_values->value.v128.maskVal), MAC_ADDRESS2(pFilter_values->value.v128.specVal));
        }
        else {
          snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                   "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                   MAC_ADDRESS2(pFilter_values->value.v128.specVal));
        }
      }
      else {
        if (NON_ZERO16(pFilter_values->value.v128.lastVal)) {
          snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                   "([%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X].."
                   "[%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X])",
                   IPV6_ADDRESS(pFilter_values->value.v128.specVal), IPV6_ADDRESS(pFilter_values->value.v128.lastVal));
        }
        else if (NON_ZERO16(pFilter_values->value.v128.maskVal)) {
          snprintf(&filter_buffer1[strlen(filter_buffer1)],  NTPL_BSIZE - strlen(filter_buffer1) - 1,
                   "{[%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X]:"
                   "[%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X]}",
                   IPV6_ADDRESS(pFilter_values->value.v128.maskVal), IPV6_ADDRESS(pFilter_values->value.v128.specVal));
        }
        else {
          snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1,
                   "[%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X]",
                   IPV6_ADDRESS(pFilter_values->value.v128.specVal));
        }
      }
      break;
    }
  }
  snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1, ")");

  // Cleanup filter values
  while (!LIST_EMPTY(&filter_values)) {
    pFilter_values = LIST_FIRST(&filter_values);
    LIST_REMOVE(pFilter_values, next);
    if (pFilter_values) {
      free(pFilter_values);
    }
  }

  // Set keylist filter
  if (DoNtpl(filter_buffer1, pNtplInfo, internals)) {
    iRet = -1;
    goto Errors;
  }
  pushNtplID(flow, pNtplInfo->ntplId);

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;

  snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "Key(KDEF%u)==%u", key, key);

Errors:
  if (pNtplInfo) {
    free(pNtplInfo);
  }
  if (filter_buffer1) {
    free(filter_buffer1);
  }
  if (filter_buffer2) {
    free(filter_buffer2);
  }
  if (filter_buffer3) {
    free(filter_buffer3);
  }

  return iRet;
}

/**
 * Setup an ethernet filter.
 */
int SetEthernetFilter(const struct rte_flow_item *item,
                      bool tnl,
                      uint64_t *typeMask)
{
  const struct rte_flow_item_eth *spec = (const struct rte_flow_item_eth *)item->spec;
  const struct rte_flow_item_eth *mask = (const struct rte_flow_item_eth *)item->mask;
  const struct rte_flow_item_eth *last = (const struct rte_flow_item_eth *)item->last;

  bool singleSetup = true;
  if (spec && NON_ZERO6(spec->src.addr_bytes) && NON_ZERO6(spec->dst.addr_bytes)) {
    if (last && (NON_ZERO6(last->src.addr_bytes) || NON_ZERO6(last->dst.addr_bytes))) {
        singleSetup = true;
    }
    else if (mask && CHECKETHER(mask->src.addr_bytes) && CHECKETHER(mask->dst.addr_bytes)) {
      uint8_t addr0[16];
      uint8_t addr1[16];
      memset(addr0, 0, 16 * sizeof(uint8_t));
      memset(addr1, 0, 16 * sizeof(uint8_t));
      MAC_ADDRESS_SWAP(spec->dst.addr_bytes,addr0);
      MAC_ADDRESS_SWAP(spec->src.addr_bytes,(&addr0[6]));
      MAC_ADDRESS_SWAP(mask->dst.addr_bytes,addr1);
      MAC_ADDRESS_SWAP(mask->src.addr_bytes,(&addr1[6]));
      if (SetFilter(128, 0, 0, tnl, LAYER2, (const void *)addr0, (const void *)addr1, NULL) != 0) {
        return -1;
      }
      *typeMask |= (ETHER_ADDR_DST | ETHER_ADDR_SRC);
      singleSetup = false;
    }
    else if (!mask || (mask && !CHECKETHER(mask->src.addr_bytes) && !CHECKETHER(mask->dst.addr_bytes))) {
      /* Setup source and destination simpel filter */
      uint8_t addr[16];
      memset(addr, 0, 16 * sizeof(uint8_t));
      MAC_ADDRESS_SWAP(spec->dst.addr_bytes,addr);
      MAC_ADDRESS_SWAP(spec->src.addr_bytes,(&addr[6]));
      if (SetFilter(128, 0, 0, tnl, LAYER2, (const void *)addr, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= (ETHER_ADDR_DST | ETHER_ADDR_SRC);
      singleSetup = false;
    }
  }
  if (singleSetup) {
    if (spec && NON_ZERO6(spec->src.addr_bytes)) {
      if (last && NON_ZERO6(last->src.addr_bytes)) {
        /* Setup source range filter */
        uint8_t addr0[ETHER_ADDR_LEN];
        uint8_t addr1[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->src.addr_bytes,addr0);
        MAC_ADDRESS_SWAP(last->src.addr_bytes,addr1);
        const uint64_t *tmp0 = (const uint64_t *)addr0;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        const uint64_t *tmp1 = (const uint64_t *)addr1;
        uint64_t vLast = ((*tmp1) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 6, tnl, LAYER2, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_SRC);
      }
      else if (mask && CHECKETHER(mask->src.addr_bytes)) {
        /* Setup source mask filter */
        uint8_t addr0[ETHER_ADDR_LEN];
        uint8_t addr1[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->src.addr_bytes,addr0);
        MAC_ADDRESS_SWAP(mask->src.addr_bytes,addr1);
        const uint64_t *tmp0 = (const uint64_t *)addr0;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        const uint64_t *tmp1 = (const uint64_t *)addr1;
        uint64_t vMask = ((*tmp1) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 6, tnl, LAYER2, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_SRC);
      }
      else {
        /* Setup source simpel filter */
        uint8_t addr[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->src.addr_bytes,addr);
        const uint64_t *tmp0 = (const uint64_t *)addr;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 6, tnl, LAYER2, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_SRC);
      }
    }
    if (spec && NON_ZERO6(spec->dst.addr_bytes)) {
      if (last && NON_ZERO6(last->dst.addr_bytes)) {
        /* Setup destination range filter */
        uint8_t addr0[ETHER_ADDR_LEN];
        uint8_t addr1[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->dst.addr_bytes,addr0);
        MAC_ADDRESS_SWAP(last->dst.addr_bytes,addr1);
        const uint64_t *tmp0 = (const uint64_t *)addr0;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        const uint64_t *tmp1 = (const uint64_t *)addr1;
        uint64_t vLast = ((*tmp1) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 0, tnl, LAYER2, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_DST);
      }
      else if (mask && CHECKETHER(mask->dst.addr_bytes)) {
        /* Setup destination mask filter */
        uint8_t addr0[ETHER_ADDR_LEN];
        uint8_t addr1[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->dst.addr_bytes,addr0);
        MAC_ADDRESS_SWAP(mask->dst.addr_bytes,addr1);
        const uint64_t *tmp0 = (const uint64_t *)addr0;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        const uint64_t *tmp1 = (const uint64_t *)addr1;
        uint64_t vMask = ((*tmp1) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 0, tnl, LAYER2, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_DST);
      }
      else {
        /* Setup destination simpel filter */
        uint8_t addr[ETHER_ADDR_LEN];
        MAC_ADDRESS_SWAP(spec->dst.addr_bytes,addr);
        const uint64_t *tmp0 = (const uint64_t *)addr;
        uint64_t vSpec = ((*tmp0) << 16) & 0xFFFFFFFFFFFF0000;
        if (SetFilter(64, 0xFFFFFFFFFFFF0000, 0, tnl, LAYER2, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= (ETHER_ADDR_DST);
      }
    }
  }

  if (spec && spec->type) {
    if (last && last->type) {
      /* Setup type range filter */
      uint16_t vSpec = rte_bswap16(spec->type);
      uint16_t vLast = rte_bswap16(last->type);
      if (SetFilter(16, 0, 12, tnl, LAYER2, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= (ETHER_TYPE);
    }
    else if (CHECK16(mask, type)) {
      /* Setup type mask filter */
      uint16_t vSpec = rte_bswap16(spec->type);
      uint16_t vMask = rte_bswap16(mask->type);
      if (SetFilter(16, 0, 12, tnl, LAYER2, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= (ETHER_TYPE);
    }
    else {
      /* Setup type simpel filter */
      uint16_t vSpec = rte_bswap16(spec->type);
      if (SetFilter(16, 0, 12, tnl, LAYER2, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= (ETHER_TYPE);
    }
  }
  return 0;
}

/**
 * Setup an IPv4 filter.
 */

int SetIPV4Filter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_ipv4 *spec = (const struct rte_flow_item_ipv4 *)item->spec;
  const struct rte_flow_item_ipv4 *mask = (const struct rte_flow_item_ipv4 *)item->mask;
  const struct rte_flow_item_ipv4 *last = (const struct rte_flow_item_ipv4 *)item->last;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer3Protocol==IPV4)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer3Protocol==IPV4)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && (spec->hdr.version_ihl || spec->hdr.type_of_service)) {
    if (last && (last->hdr.version_ihl || spec->hdr.type_of_service)) {
      RTE_LOG(ERR, PMD, "Range is not supported for version_ihl and type_of_service\n");
    }

    if (CHECK8(mask, hdr.version_ihl) || CHECK8(mask, hdr.type_of_service)) {
      uint16_t vSpec;
      uint16_t vMask;
      vSpec = ((spec->hdr.version_ihl << 8) & 0xFF00) | spec->hdr.type_of_service;
      vMask = ((mask->hdr.version_ihl << 8) & 0xFF00) | mask->hdr.type_of_service;
      if (SetFilter(16, 0, 0, tnl, LAYER3, (void *)&vSpec, (void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_VERSION_IHL | IPV4_TYPE_OF_SERVICE;
    }
    else {
      uint16_t vSpec;
      vSpec = ((spec->hdr.version_ihl << 8) & 0xFF00) | spec->hdr.type_of_service;
      if (SetFilter(16, 0, 0, tnl, LAYER3, (void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_VERSION_IHL | IPV4_TYPE_OF_SERVICE;
    }
  }

  if (spec && spec->hdr.total_length) {
    if (last && last->hdr.total_length) {
      uint16_t vSpec = rte_bswap16(spec->hdr.total_length);
      uint16_t vLast = rte_bswap16(last->hdr.total_length);
      if (SetFilter(16, 0, 2, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV4_TOTAL_LENGTH;
    }
    else if (CHECK16(mask, hdr.total_length)) {
      uint16_t vSpec = rte_bswap16(spec->hdr.total_length);
      uint16_t vMask = rte_bswap16(mask->hdr.total_length);
      if (SetFilter(16, 0, 2, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_TOTAL_LENGTH;
    }
    else {
      uint16_t vSpec = rte_bswap16(spec->hdr.total_length);
      if (SetFilter(16, 0, 2, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_TOTAL_LENGTH;
    }
  }

  if (spec && spec->hdr.packet_id) {
    if (last && last->hdr.packet_id) {
      uint16_t vSpec = rte_bswap16(spec->hdr.packet_id);
      uint16_t vLast = rte_bswap16(last->hdr.packet_id);
      if (SetFilter(16, 0, 4, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV4_PACKET_ID;
    }
    else if (CHECK16(mask, hdr.packet_id)) {
      uint16_t vSpec = rte_bswap16(spec->hdr.packet_id);
      uint16_t vMask = rte_bswap16(mask->hdr.packet_id);
      if (SetFilter(16, 0, 4, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_PACKET_ID;
    }
    else {
      uint16_t vSpec = rte_bswap16(spec->hdr.packet_id);
      if (SetFilter(16, 0, 4, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_PACKET_ID;
    }
  }

  if (spec && spec->hdr.fragment_offset) {
    if (last && last->hdr.fragment_offset) {
      uint16_t vSpec = rte_bswap16(spec->hdr.fragment_offset);
      uint16_t vLast = rte_bswap16(last->hdr.fragment_offset);
      if (SetFilter(16, 0, 6, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV4_FRAGMENT_OFFSET;
    }
    else if (CHECK16(mask, hdr.fragment_offset)) {
      uint16_t vSpec = rte_bswap16(spec->hdr.fragment_offset);
      uint16_t vMask = rte_bswap16(mask->hdr.fragment_offset);
      if (SetFilter(16, 0, 6, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_FRAGMENT_OFFSET;
    }
    else {
      uint16_t vSpec = rte_bswap16(spec->hdr.fragment_offset);
      if (SetFilter(16, 0, 6, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_FRAGMENT_OFFSET;
    }
  }

  if (spec && spec->hdr.time_to_live) {
    uint16_t vSpec;
    uint16_t vMask;
    uint16_t vLast;
    if (last && last->hdr.time_to_live) {
      vSpec = spec->hdr.time_to_live << 8;
      vLast = last->hdr.time_to_live << 8;
      if (SetFilter(16, 0xFF00, 8, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV4_TIME_TO_LIVE;
    }
    else if (CHECK8(mask, hdr.time_to_live)) {
      vSpec = spec->hdr.time_to_live << 8;
      vMask = mask->hdr.time_to_live << 8;
      if (SetFilter(16, 0xFF00, 8, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
    }
    else {
      vSpec = spec->hdr.time_to_live << 8;
      if (SetFilter(16, 0xFF00, 8, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
    }
  }

  if (spec && spec->hdr.next_proto_id) {
    uint16_t vSpec;
    uint16_t vMask;
    uint16_t vLast;
    if (last && last->hdr.next_proto_id) {
      vSpec = spec->hdr.next_proto_id;
      vLast = last->hdr.next_proto_id;
      if (SetFilter(16, 0x00FF, 8, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV4_NEXT_PROTO_ID;
    }
    else if (CHECK8(mask, hdr.next_proto_id)) {
      vSpec = spec->hdr.next_proto_id;
      vMask = mask->hdr.next_proto_id;
      if (SetFilter(16, 0x00FF, 8, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_NEXT_PROTO_ID;
    }
    else {
      vSpec = spec->hdr.next_proto_id;
      if (SetFilter(16, 0x00FF, 8, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV4_NEXT_PROTO_ID;
    }
  }

  bool singleSetup = true;
  if (spec && spec->hdr.src_addr && spec->hdr.dst_addr) {
    if (last && (last->hdr.src_addr || last->hdr.dst_addr)) {
      singleSetup = true;
    }
    else if (CHECK32(mask, hdr.src_addr) || CHECK32(mask, hdr.dst_addr)) {
      if (mask->hdr.src_addr && mask->hdr.dst_addr) {
        uint64_t vSpec = (((uint64_t)rte_bswap32(spec->hdr.src_addr) << 32ULL) & 0xFFFFFFFF00000000ULL) | (uint64_t)rte_bswap32(spec->hdr.dst_addr);
        uint64_t vMask = (((uint64_t)rte_bswap32(mask->hdr.src_addr) << 32ULL) & 0xFFFFFFFF00000000ULL) | (uint64_t)rte_bswap32(mask->hdr.dst_addr);
        if (SetFilter(64, 0, 12, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= IPV4_SRC_ADDR | IPV4_DST_ADDR;
        singleSetup = false;
        PRINT_IPV4("SRC", rte_bswap32(spec->hdr.src_addr));
        PRINT_IPV4("DST", rte_bswap32(spec->hdr.dst_addr));
      }
    }
    else {
      uint64_t vSpec = (((uint64_t)rte_bswap32(spec->hdr.src_addr) << 32ULL) & 0xFFFFFFFF00000000ULL) | (uint64_t)rte_bswap32(spec->hdr.dst_addr);
      if (SetFilter(64, 0, 12, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      singleSetup = false;
      PRINT_IPV4("SRC", rte_bswap32(spec->hdr.src_addr));
      PRINT_IPV4("DST", rte_bswap32(spec->hdr.dst_addr));
    }
  }
  if (singleSetup) {
    if (spec && spec->hdr.src_addr) {
      if (last && last->hdr.src_addr) {
        uint32_t vSpec = rte_bswap32(spec->hdr.src_addr);
        uint32_t vLast = rte_bswap32(last->hdr.src_addr);
        if (SetFilter(32, 0, 12, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= IPV4_SRC_ADDR;
        PRINT_IPV4("SRC", rte_bswap32(spec->hdr.src_addr));
      }
      else if (CHECK32(mask, hdr.src_addr)) {
        uint32_t vSpec = rte_bswap32(spec->hdr.src_addr);
        uint32_t vMask = rte_bswap32(mask->hdr.src_addr);
        if (SetFilter(32, 0, 12, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= IPV4_SRC_ADDR;
        PRINT_IPV4("SRC", rte_bswap32(spec->hdr.src_addr));
      }
      else {
        uint32_t vSpec = rte_bswap32(spec->hdr.src_addr);
        if (SetFilter(32, 0, 12, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= IPV4_SRC_ADDR;
        PRINT_IPV4("SRC", rte_bswap32(spec->hdr.src_addr));
      }
    }
    if (spec && spec->hdr.dst_addr) {
      if (last && last->hdr.dst_addr) {
        uint32_t vSpec = rte_bswap32(spec->hdr.dst_addr);
        uint32_t vLast = rte_bswap32(last->hdr.dst_addr);
        if (SetFilter(32, 0, 16, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= IPV4_DST_ADDR;
        PRINT_IPV4("DST", rte_bswap32(spec->hdr.dst_addr));
      }
      else if (CHECK32(mask, hdr.dst_addr)) {
        uint32_t vSpec = rte_bswap32(spec->hdr.dst_addr);
        uint32_t vMask = rte_bswap32(mask->hdr.dst_addr);
        if (SetFilter(32, 0, 16, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= IPV4_DST_ADDR;
        PRINT_IPV4("DST", rte_bswap32(spec->hdr.dst_addr));
      }
      else {
        uint32_t vSpec = rte_bswap32(spec->hdr.dst_addr);
        if (SetFilter(32, 0, 16, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= IPV4_DST_ADDR;
        PRINT_IPV4("DST", rte_bswap32(spec->hdr.dst_addr));
      }
    }
  }

  return 0;
}

int SetIPV6Filter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_ipv6 *spec = (const struct rte_flow_item_ipv6 *)item->spec;
  const struct rte_flow_item_ipv6 *mask = (const struct rte_flow_item_ipv6 *)item->mask;
  const struct rte_flow_item_ipv6 *last = (const struct rte_flow_item_ipv6 *)item->last;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer3Protocol==IPV6)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer3Protocol==IPV6)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && (spec->hdr.vtc_flow)) {
    if (last && (last->hdr.vtc_flow)) {
      uint32_t vSpec = rte_bswap32(spec->hdr.vtc_flow);
      uint32_t vLast = rte_bswap32(last->hdr.vtc_flow);
      if (SetFilter(32, 0, 0, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV6_VTC_FLOW;
    }
    else if (CHECK32(mask, hdr.vtc_flow)) {
      uint32_t vSpec = rte_bswap32(spec->hdr.vtc_flow);
      uint32_t vMask = rte_bswap32(mask->hdr.vtc_flow);
      if (SetFilter(32, 0, 0, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_VTC_FLOW;
    }
    else {
      uint32_t vSpec = rte_bswap32(spec->hdr.vtc_flow);
      if (SetFilter(32, 0, 0, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_VTC_FLOW;
    }
  }

  if (spec && (spec->hdr.proto)) {
    if (last && (last->hdr.proto)) {
      uint16_t vSpec = (spec->hdr.proto << 8) & 0xFF00;
      uint16_t vLast = (last->hdr.proto << 8) & 0xFF00;
      if (SetFilter(16, 0xFF00, 6, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV6_PROTO;
    }
    else if (CHECK8(mask, hdr.proto)) {
      uint16_t vSpec = (spec->hdr.proto << 8) & 0xFF00;
      uint16_t vMask = (mask->hdr.proto << 8) & 0xFF00;
      if (SetFilter(16, 0xFF00, 6, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_PROTO;
    }
    else {
      uint16_t vSpec = (spec->hdr.proto << 8) & 0xFF00;
      if (SetFilter(16, 0xFF00, 6, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_PROTO;
    }
  }

  if (spec && (spec->hdr.hop_limits)) {
    if (last && (last->hdr.hop_limits)) {
      uint16_t vSpec = (spec->hdr.hop_limits);
      uint16_t vLast = (last->hdr.hop_limits);
      if (SetFilter(16, 0xFF, 6, tnl, LAYER3, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= IPV6_HOP_LIMITS;
    }
    else if (CHECK8(mask, hdr.hop_limits)) {
      uint16_t vSpec = (spec->hdr.hop_limits);
      uint16_t vMask = (mask->hdr.hop_limits);
      if (SetFilter(16, 0xFF, 6, tnl, LAYER3, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_HOP_LIMITS;
    }
    else {
      uint16_t vSpec = (spec->hdr.hop_limits);
      if (SetFilter(16, 0xFF, 6, tnl, LAYER3, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_HOP_LIMITS;
    }
  }

  if (spec && NON_ZERO16(spec->hdr.src_addr)) {
    if (last && NON_ZERO16(last->hdr.src_addr)) {
      if (SetFilter(128, 0, 8, tnl, LAYER3, (const void *)&spec->hdr.src_addr, NULL, (const void *)&last->hdr.src_addr) != 0) {
        return -1;
      }
      *typeMask |= IPV6_SRC_ADDR;
    }
    else if (mask && CHECKIPV6(mask->hdr.src_addr)) {
      if (SetFilter(128, 0, 8, tnl, LAYER3, (const void *)&spec->hdr.src_addr, (const void *)&mask->hdr.src_addr, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_SRC_ADDR;
    }
    else {
      if (SetFilter(128, 0, 8, tnl, LAYER3, (const void *)&spec->hdr.src_addr, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_SRC_ADDR;
    }
  }

  if (spec && NON_ZERO16(spec->hdr.dst_addr)) {
    if (last && NON_ZERO16(last->hdr.dst_addr)) {
      if (SetFilter(128, 0, 24, tnl, LAYER3, (const void *)&spec->hdr.dst_addr, NULL, (const void *)&last->hdr.dst_addr) != 0) {
        return -1;
      }
      *typeMask |= IPV6_DST_ADDR;
    }
    else if (mask && CHECKIPV6(mask->hdr.dst_addr)) {
      if (SetFilter(128, 0, 24, tnl, LAYER3, (const void *)&spec->hdr.dst_addr, (const void *)&mask->hdr.dst_addr, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_DST_ADDR;
    }
    else {
      if (SetFilter(128, 0, 24, tnl, LAYER3, (const void *)&spec->hdr.dst_addr, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= IPV6_DST_ADDR;
    }
  }
  return 0;
}

int SetTCPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_tcp *spec = (const struct rte_flow_item_tcp *)item->spec;
  const struct rte_flow_item_tcp *mask = (const struct rte_flow_item_tcp *)item->mask;
  const struct rte_flow_item_tcp *last = (const struct rte_flow_item_tcp *)item->last;
  bool singleSetup = true;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer4Protocol==TCP)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer4Protocol==TCP)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && spec->hdr.src_port && spec && spec->hdr.dst_port)  {
    if (last && (last->hdr.src_port || last->hdr.dst_port)) {
      singleSetup = true;
    }
    else if (CHECK16(mask, hdr.src_port) || CHECK16(mask, hdr.dst_port)) {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      uint32_t vMask = ((rte_bswap16(mask->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(mask->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= TCP_SRC_PORT | TCP_DST_PORT;
      singleSetup = false;
    }
    else {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= TCP_SRC_PORT | TCP_DST_PORT;
      singleSetup = false;
    }
  }
  if (singleSetup) {
    if (spec && spec->hdr.src_port) {
      if (last && last->hdr.src_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vLast = rte_bswap16(last->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= TCP_SRC_PORT;
      }
      else if (CHECK16(mask, hdr.src_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vMask = rte_bswap16(mask->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= TCP_SRC_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= TCP_SRC_PORT;
      }
    }
    if (spec && spec->hdr.dst_port) {
      if (last && last->hdr.dst_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vLast = rte_bswap16(last->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= TCP_DST_PORT;
      }
      else if (CHECK16(mask, hdr.dst_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vMask = rte_bswap16(mask->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= TCP_DST_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= TCP_DST_PORT;
      }
    }
  }

  if (spec && (spec->hdr.data_off || spec->hdr.tcp_flags)) {
    if (last && (last->hdr.data_off || last->hdr.tcp_flags)) {
      RTE_LOG(ERR, PMD, "Range is not supported for data_off and tcp_flags\n");
      return -1;
    }

    if (CHECK8(mask, hdr.data_off) || CHECK8(mask, hdr.tcp_flags)) {
      uint16_t vSpec = ((spec->hdr.data_off << 8) & 0xFF00) | spec->hdr.tcp_flags;
      uint16_t vMask = ((mask->hdr.data_off << 8) & 0xFF00) | mask->hdr.tcp_flags;
      if (SetFilter(16, 0, 12, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= TCP_DATA_OFF | TCP_FLAGS;

    }
    else {
      uint16_t vSpec = ((spec->hdr.data_off << 8) & 0xFF00) | spec->hdr.tcp_flags;
      if (SetFilter(16, 0, 12, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= TCP_DATA_OFF | TCP_FLAGS;
    }
  }
  return 0;
}

int SetUDPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_udp *spec = (const struct rte_flow_item_udp *)item->spec;
  const struct rte_flow_item_udp *mask = (const struct rte_flow_item_udp *)item->mask;
  const struct rte_flow_item_udp *last = (const struct rte_flow_item_udp *)item->last;
  bool singleSetup = true;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer4Protocol==UDP)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer4Protocol==UDP)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && spec->hdr.src_port && spec->hdr.dst_port) {
    if (last && (last->hdr.src_port || last->hdr.dst_port)) {
      singleSetup = true;
    }
    else if (CHECK16(mask, hdr.src_port) || CHECK16(mask, hdr.dst_port)) {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      uint32_t vMask = ((rte_bswap16(mask->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(mask->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= UDP_SRC_PORT | UDP_DST_PORT;
      singleSetup = false;
    }
    else {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= UDP_SRC_PORT | UDP_DST_PORT;
      singleSetup = false;
    }
  }
  if (singleSetup) {
    if (spec && spec->hdr.src_port) {
      if (last && last->hdr.src_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vLast = rte_bswap16(last->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= UDP_SRC_PORT;
      }
      else if (CHECK16(mask, hdr.src_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vMask = rte_bswap16(mask->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= UDP_SRC_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= UDP_SRC_PORT;
      }
    }
    if (spec && spec->hdr.dst_port) {
      if (last && last->hdr.dst_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vLast = rte_bswap16(last->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= UDP_DST_PORT;
      }
      else if (CHECK16(mask, hdr.dst_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vMask = rte_bswap16(mask->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= UDP_DST_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= UDP_DST_PORT;
      }
    }
  }

  return 0;
}

int SetSCTPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_sctp *spec = (const struct rte_flow_item_sctp *)item->spec;
  const struct rte_flow_item_sctp *mask = (const struct rte_flow_item_sctp *)item->mask;
  const struct rte_flow_item_sctp *last = (const struct rte_flow_item_sctp *)item->last;
  bool singleSetup = true;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer4Protocol==SCTP)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer4Protocol==SCTP)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && spec->hdr.src_port && spec->hdr.dst_port) {
    if (last && (last->hdr.src_port || last->hdr.dst_port)) {
      singleSetup = true;
    }
    else if (CHECK16(mask, hdr.src_port) || CHECK16(mask, hdr.dst_port)) {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      uint32_t vMask = ((rte_bswap16(mask->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(mask->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= SCTP_SRC_PORT | SCTP_DST_PORT;
      singleSetup = false;
    }
    else {
      uint32_t vSpec = ((rte_bswap16(spec->hdr.src_port) << 16) & 0xFFFF0000) | rte_bswap16(spec->hdr.dst_port);
      if (SetFilter(32, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= SCTP_SRC_PORT | SCTP_DST_PORT;
      singleSetup = false;
    }
  }
  if (singleSetup) {
    if (spec && spec->hdr.src_port) {
      if (last && last->hdr.src_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vLast = rte_bswap16(last->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= SCTP_SRC_PORT;
      }
      else if (CHECK16(mask, hdr.src_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        uint16_t vMask = rte_bswap16(mask->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= SCTP_SRC_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.src_port);
        if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= SCTP_SRC_PORT;
      }
    }

    if (spec && spec->hdr.dst_port) {
      if (last && last->hdr.dst_port) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vLast = rte_bswap16(last->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= SCTP_DST_PORT;
      }
      else if (CHECK16(mask, hdr.dst_port)) {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        uint16_t vMask = rte_bswap16(mask->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= SCTP_DST_PORT;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->hdr.dst_port);
        if (SetFilter(16, 0, 2, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= SCTP_DST_PORT;
      }
    }
  }

  return 0;
}

int SetICMPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_icmp *spec = (const struct rte_flow_item_icmp *)item->spec;
  const struct rte_flow_item_icmp *mask = (const struct rte_flow_item_icmp *)item->mask;
  const struct rte_flow_item_icmp *last = (const struct rte_flow_item_icmp *)item->last;
  bool singleSetup = true;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerLayer4Protocol==ICMP)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Layer4Protocol==ICMP)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && spec->hdr.icmp_type && spec->hdr.icmp_code) {
    if (last && (last->hdr.icmp_type || last->hdr.icmp_code)) {
      singleSetup = true;
    }
    else if (mask && CHECK8(mask, hdr.icmp_type) && CHECK8(mask, hdr.icmp_code)) {
      uint16_t vSpec = ((spec->hdr.icmp_type << 8) & 0xFF00) | spec->hdr.icmp_code;
      uint16_t vMask = ((mask->hdr.icmp_type << 8) & 0xFF00) | mask->hdr.icmp_code;
      if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= ICMP_TYPE | ICMP_CODE;
      singleSetup = false;
    }
    else if (!mask || (mask && !CHECK8(mask, hdr.icmp_type) && !CHECK8(mask, hdr.icmp_code))) {
      uint16_t vSpec = ((spec->hdr.icmp_type << 8) & 0xFF00) | spec->hdr.icmp_code;
      if (SetFilter(16, 0, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= ICMP_TYPE | ICMP_CODE;
      singleSetup = false;
    }
  }

  if (singleSetup) {
    if (spec && spec->hdr.icmp_type) {
      if (last && last->hdr.icmp_type) {
        uint16_t vSpec = ((spec->hdr.icmp_type << 8) & 0xFF00);
        uint16_t vLast = ((last->hdr.icmp_type << 8) & 0xFF00);
        if (SetFilter(16, 0xFF00, 0, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= ICMP_TYPE;
      }
      else if (mask && CHECK8(mask, hdr.icmp_type)) {
        uint16_t vSpec = ((spec->hdr.icmp_type << 8) & 0xFF00);
        uint16_t vMask = ((mask->hdr.icmp_type << 8) & 0xFF00);
        if (SetFilter(16, 0xFF00, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= ICMP_TYPE;
      }
      else {
        uint16_t vSpec = ((spec->hdr.icmp_type << 8) & 0xFF00);
        if (SetFilter(16, 0xFF00, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= ICMP_TYPE;
      }
    }

    if (spec && spec->hdr.icmp_code) {
      if (last && last->hdr.icmp_code) {
        uint16_t vSpec = spec->hdr.icmp_code;
        uint16_t vLast = last->hdr.icmp_code;
        if (SetFilter(16, 0xFF, 0, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
          return -1;
        }
        *typeMask |= ICMP_CODE;
      }
      else if (mask && CHECK8(mask, hdr.icmp_code)) {
        uint16_t vSpec = spec->hdr.icmp_code;
        uint16_t vMask = mask->hdr.icmp_code;
        if (SetFilter(16, 0xFF, 0, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= ICMP_CODE;
      }
      else {
        uint16_t vSpec = spec->hdr.icmp_code;
        if (SetFilter(16, 0xFF, 0, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= ICMP_CODE;
      }
    }
  }

  if (spec && spec->hdr.icmp_ident) {
    if (last && last->hdr.icmp_ident) {
      uint16_t vSpec = rte_bswap16(spec->hdr.icmp_ident);
      uint16_t vLast = rte_bswap16(last->hdr.icmp_ident);
      if (SetFilter(16, 0, 4, tnl, LAYER4, (const void *)&vSpec, NULL, (const void *)&vLast) != 0) {
        return -1;
      }
      *typeMask |= ICMP_IDENT;
    }
    else if (mask && CHECK16(mask, hdr.icmp_ident)) {
      uint16_t vSpec = rte_bswap16(spec->hdr.icmp_ident);
      uint16_t vMask = rte_bswap16(mask->hdr.icmp_ident);
      if (SetFilter(16, 0, 4, tnl, LAYER4, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= ICMP_IDENT;
    }
    else {
      uint16_t vSpec = rte_bswap16(spec->hdr.icmp_ident);
      if (SetFilter(16, 0, 4, tnl, LAYER4, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= ICMP_IDENT;
    }
  }

  return 0;
}

int SetVlanFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl, uint64_t *typeMask)
{
  const struct rte_flow_item_vlan *spec = (const struct rte_flow_item_vlan *)item->spec;
  const struct rte_flow_item_vlan *mask = (const struct rte_flow_item_vlan *)item->mask;
  const struct rte_flow_item_vlan *last = (const struct rte_flow_item_vlan *)item->last;
  bool singleSetup = true;

  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  if (tnl) {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(InnerEncapsulation==VLAN)");
  }
  else {
    strcat(&ntpl_buf[strlen(ntpl_buf)], "(Encapsulation==VLAN)");
  }

  if (!last && !mask && !spec) {
    return 0;
  }

  if (spec && spec->tpid && spec->tci) {
    if (last && (last->tpid || last->tci)) {
      singleSetup = true;
    }
    else if (mask && CHECK16(mask, tpid) && CHECK16(mask, tci)) {
      uint32_t vSpec = ((rte_bswap16(spec->tpid) << 16) & 0xFFFF0000) | rte_bswap16(spec->tci);
      uint32_t vMask = ((rte_bswap16(mask->tpid) << 16) & 0xFFFF0000) | rte_bswap16(mask->tci);
      if (SetFilter(32, 0, 0, tnl, VLAN, (const void *)&vSpec, (const void *)&vMask, NULL) != 0) {
        return -1;
      }
      *typeMask |= VLAN_TPID | VLAN_TCI;
      singleSetup = false;
    }
    else if (!mask || (mask && !CHECK16(mask, tpid) && !CHECK16(mask, tci))) {
      uint32_t vSpec = ((rte_bswap16(spec->tpid) << 16) & 0xFFFF0000) | rte_bswap16(spec->tci);
      if (SetFilter(32, 0, 0, tnl, VLAN, (const void *)&vSpec, NULL, NULL) != 0) {
        return -1;
      }
      *typeMask |= VLAN_TPID | VLAN_TCI;
      singleSetup = false;
    }
  }
  if (singleSetup) {
    if (spec && spec->tpid) {
      if (last && last->tpid) {
        uint16_t vSpec = rte_bswap16(spec->tpid);
        uint16_t vLast = rte_bswap16(last->tpid);
        if (SetFilter(16, 0, 0, tnl, VLAN, (const void *)&vSpec, NULL, &vLast) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TPID;
      }
      else if (mask && CHECK16(mask, tpid)) {
        uint16_t vSpec = rte_bswap16(spec->tpid);
        uint16_t vMask = rte_bswap16(mask->tpid);
        if (SetFilter(16, 0, 0, tnl, VLAN, (const void *)&vSpec, &vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TPID;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->tpid);
        if (SetFilter(16, 0, 0, tnl, VLAN, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TPID;
      }
    }

    if (spec && spec->tci) {
      if (last && last->tci) {
        uint16_t vSpec = rte_bswap16(spec->tci);
        uint16_t vLast = rte_bswap16(last->tci);
        if (SetFilter(16, 0, 2, tnl, VLAN, (const void *)&vSpec, NULL, &vLast) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TCI;
      }
      else if (mask && CHECK16(mask, tci)) {
        uint16_t vSpec = rte_bswap16(spec->tci);
        uint16_t vMask = rte_bswap16(mask->tci);
        if (SetFilter(16, 0, 2, tnl, VLAN, (const void *)&vSpec, &vMask, NULL) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TCI;
      }
      else {
        uint16_t vSpec = rte_bswap16(spec->tci);
        if (SetFilter(16, 0, 2, tnl, VLAN, (const void *)&vSpec, NULL, NULL) != 0) {
          return -1;
        }
        *typeMask |= VLAN_TCI;
      }
    }
  }
  return 0;
}

int SetTunnelFilter(char *ntpl_buf,
                    bool *fc,
                    int version,
                    int type,
                    uint64_t *typeMask)
{
  if (*fc) strcat(ntpl_buf," and ");
  *fc = true;
  switch (type) {
  case GTP_TUNNEL_TYPE:
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "(TunnelType==GTPv%d-U)", version);
    *typeMask |= GTP_TUNNEL;
    break;
  case GRE_TUNNEL_TYPE:
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "(TunnelType==GREv%d)", version);
    *typeMask |= GRE_TUNNEL;
    break;
  case VXLAN_TUNNEL_TYPE:
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "(TunnelType==VXLAN)");
    *typeMask |= VXLAN_TUNNEL;
    break;
  case NVGRE_TUNNEL_TYPE:
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
            "((TunnelType == GREv0 OR TunnelType == GREv1) AND InnerLayer2Protocol != other)");
    *typeMask |= NVGRE_TUNNEL;
    break;
  case IP_IN_IP_TUNNEL_TYPE:
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "(TunnelType == IPinIP)");
    *typeMask |= IP_IN_IP_TUNNEL;
    break;
  }
  return 0;
}

int SetGreFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, uint64_t *typeMask)
{
  const struct rte_flow_item_gre *spec = (const struct rte_flow_item_gre *)item->spec;
  int version;

  version = spec->c_rsvd0_ver & 0x7;
  return SetTunnelFilter(ntpl_buf, fc, version, GRE_TUNNEL_TYPE, typeMask);
}

void *ntacc_add_rules(uint8_t port_id, uint16_t type,
                      uint8_t l4_proto, int queue, char *ntpl_str)
 {
  struct rte_flow *rte_flow = NULL;
  // Simple L4PROTO and TOS/TC bit 1 filter.
  // The most annoying part is that a id <-> struct rte_flow* must be maintained
  struct rte_flow_attr attr = { .ingress=1, };
  struct rte_flow_action_queue rte_flow_action_queue = { .index = queue, };
  struct rte_flow_action actions[2] = {
      {
          .type = RTE_FLOW_ACTION_TYPE_QUEUE,
          .conf = &rte_flow_action_queue,
      },
      {
          .type = RTE_FLOW_ACTION_TYPE_END,
          .conf = NULL,
      }
  };
  struct rte_flow_item pattern[2] =
  {
      {
          .type = 0,
          .spec = NULL,
          .last = NULL,
          .mask = NULL,
      },
      {
          .type = RTE_FLOW_ITEM_TYPE_END,
          .spec = NULL,
          .last = NULL,
          .mask = NULL,
      },
  };
  struct rte_flow_error rte_flow_error;
  switch (type) {
    case RTE_ETH_FLOW_NTPL:
    {
      struct rte_flow_item_ntpl ntpl = {
        .ntpl_str = ntpl_str,
      };
      pattern[0].type = RTE_FLOW_ITEM_TYPE_NTPL;
      pattern[0].spec = &ntpl;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
    }
    break;
    case RTE_ETH_FLOW_RAW:
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern[0].spec = NULL;
    pattern[0].last = NULL;
    pattern[0].mask = NULL;
    rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
    break;
    case RTE_ETH_FLOW_IPV4:
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
      pattern[0].spec = NULL;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
    case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
    {
      struct rte_flow_item_ipv4 ipv4 = {
        .hdr = {
            .next_proto_id = l4_proto,
            .type_of_service = 0x1,
        },
      };
      struct rte_flow_item_ipv4 ipv4_mask = {
        .hdr = {
            .next_proto_id = 0xFF,
            .type_of_service = 0x01,
        },
      };
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
      pattern[0].spec = &ipv4;
      pattern[0].last = NULL;
      pattern[0].mask = &ipv4_mask;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
    case RTE_ETH_FLOW_IPV6:
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[0].spec = NULL;
      pattern[0].last = NULL;
      pattern[0].mask = NULL;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
    case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
    {
      struct rte_flow_item_ipv6 ipv6 = {
          .hdr = {
              .vtc_flow = 0x6 | (1 << 4),
              .proto = l4_proto,
          },
      };
      struct rte_flow_item_ipv6 ipv6_mask = {
          .hdr = {
              .vtc_flow = 0x010,
              .proto = 0xFF,
          },
      };
      pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
      pattern[0].spec = &ipv6;
      pattern[0].last = NULL;
      pattern[0].mask = &ipv6_mask;
      rte_flow = rte_flow_create(port_id, &attr, pattern, actions, &rte_flow_error);
      break;
    }
  }
  if (rte_flow == NULL) {
    printf("Programming port %d failed: %d %s\n",
            port_id, rte_flow_error.type, rte_flow_error.message);
  }
  return rte_flow;
}

void ntacc_del_rules(uint8_t port_id, void *rte_flow)
{
  rte_flow_destroy(port_id, (struct rte_flow*)rte_flow, NULL);
}
