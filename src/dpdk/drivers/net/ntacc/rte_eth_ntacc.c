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
#include <time.h>
#include <linux/limits.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <dlfcn.h>
#include <rte_mbuf.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_version.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <net/if.h>
#include <nt.h>

#include "rte_eth_ntacc.h"
#include "filter_ntacc.h"
#include "nt_compat.h"

int ntacc_logtype;

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#define ETH_NTACC_MASK_ARG "mask"
#define ETH_NTACC_NTPL_ARG "ntpl"

#define HW_MAX_PKT_LEN  10000
#define HW_MTU    (HW_MAX_PKT_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN) /**< MTU */

#define MAX_NTACC_PORTS 32
#define STREAMIDS_PER_PORT  (256 / internals->nbPortsInSystem)

#define MAX_NTPL_NAME 512

static struct {
   int32_t major;
   int32_t minor;
   int32_t patch;
} supportedDriver = {3, 7, 2};

#define PCI_VENDOR_ID_NAPATECH 0x18F4
#define PCI_DEVICE_ID_NT200A01 0x01A5
#define PCI_DEVICE_ID_NT80E3   0x0165
#define PCI_DEVICE_ID_NT20E3   0x0175
#define PCI_DEVICE_ID_NT40E3   0x0145
#define PCI_DEVICE_ID_NT40A01  0x0185
#define PCI_DEVICE_ID_NT100E3  0x0155

#define NB_SUPPORTED_FPGAS 10
struct {
  uint32_t item:12;
  uint32_t product:16;
  uint32_t ver:8;
  uint32_t rev:8;
  uint32_t build:10;
} supportedAdapters[NB_SUPPORTED_FPGAS] =
{
  { 200, 9500, 9, 8, 0 },
  { 200, 9501, 9, 8, 0 },
  { 200, 9502, 9, 8, 0 },
  { 200, 9503, 9, 8, 0 },
  { 200, 9505, 9, 8, 0 },
  { 200, 9512, 9, 8, 0 },
  { 200, 9515, 9, 8, 0 },
  { 200, 9516, 9, 8, 0 },
  { 200, 9517, 9, 8, 0 },
  { 200, 9519, 10, 7, 0 },
};

static void *_libnt;

/* NTAPI library functions */
int (*_NT_Init)(uint32_t);
int (*_NT_NetRxOpen)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t, int);
int (*_NT_NetRxGet)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxRelease)(NtNetStreamRx_t, NtNetBuf_t);
int (*_NT_NetRxClose)(NtNetStreamRx_t);
char *(*_NT_ExplainError)(int, char *, uint32_t);
int (*_NT_NetTxOpen)(NtNetStreamTx_t *, const char *, uint64_t, uint32_t, uint32_t);
int (*_NT_NetTxClose)(NtNetStreamTx_t);
int (*_NT_InfoOpen)(NtInfoStream_t *, const char *);
int (*_NT_InfoRead)(NtInfoStream_t, NtInfo_t *);
int (*_NT_InfoClose)(NtInfoStream_t);
int (*_NT_ConfigOpen)(NtConfigStream_t *, const char *);
int (*_NT_ConfigClose)(NtConfigStream_t);
int (*_NT_NTPL)(NtConfigStream_t, const char *, NtNtplInfo_t *, uint32_t);
int (*_NT_NetRxGetNextPacket)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxOpenMulti)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t *, unsigned int, int);
int (*_NT_NetTxRelease)(NtNetStreamTx_t, NtNetBuf_t);
int (*_NT_NetTxGet)(NtNetStreamTx_t, NtNetBuf_t *, uint32_t, size_t, enum NtNetTxPacketOption_e, int);
int (*_NT_NetTxAddPacket)(NtNetStreamTx_t hStream, uint32_t port, NtNetTxFragment_t *fragments, uint32_t fragmentCount, int timeout );
int (*_NT_NetTxRead)(NtNetStreamTx_t hStream, NtNetTx_t *cmd);
int (*_NT_StatClose)(NtStatStream_t);
int (*_NT_StatOpen)(NtStatStream_t *, const char *);
int (*_NT_StatRead)(NtStatStream_t, NtStatistics_t *);

static int _dev_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error __rte_unused);
static int eth_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
static int eth_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
static int _dev_flow_isolate(struct rte_eth_dev *dev, int set, struct rte_flow_error *error);

static char errorBuffer[1024];

static int first = 0;
static int deviceCount = 0;

static volatile uint16_t port_locks[MAX_NTACC_PORTS];

static const char *valid_arguments[] = {
  ETH_NTACC_MASK_ARG,
  ETH_NTACC_NTPL_ARG,
  NULL
};

static struct ether_addr eth_addr[MAX_NTACC_PORTS];

static uint32_t _log_nt_errors(const uint32_t status, const char *msg, const char *func)
{
  char *pErrBuf;

  printf("ERR: %s\n", msg);

  pErrBuf = (char *)rte_malloc(NULL, NT_ERRBUF_SIZE+1, 0);
  if (!pErrBuf) {
    PMD_NTACC_LOG(ERR, "Error %s: Out of memory\n", func);
    return status;
  }

  (*_NT_ExplainError)(status, pErrBuf, NT_ERRBUF_SIZE);
  printf("ERR1: %s\n", pErrBuf);
  PMD_NTACC_LOG(ERR, "Error: %s. Code 0x%x = %s\n", msg, status, pErrBuf);

  rte_free(pErrBuf);
  return status;
}

static uint32_t _log_out_of_memory_errors(const char *func)
{
  PMD_NTACC_LOG(ERR, "Error %s: Out of memory\n", func);
  return ENOMEM;
}

static void _seg_release_cb(struct rte_mbuf *mbuf)
{
	struct batch_ctrl *batchCtl = (struct batch_ctrl *)((u_char *)mbuf->userdata);
	struct ntacc_rx_queue *rx_q = batchCtl->queue;

  (*_NT_NetRxRelease)(rx_q->pNetRx, batchCtl->pSeg);

	/* swap poniter back */
	mbuf->buf_addr = batchCtl->orig_buf_addr;
}


static void _write_to_file(int fd, const char *buffer)
{
  if (write(fd, buffer, strlen(buffer)) < 0) {
    PMD_NTACC_LOG(ERR, "NTPL dump failed: Unable to write to file. Error %d\n", errno);
  }
}

int DoNtpl(const char *ntplStr, uint32_t *pNtplID, struct pmd_internals *internals)
{
  NtConfigStream_t hCfgStream;      // Config stream
  NtNtplInfo_t *pNtplInfo;
  int status;
  int fd;

  if((status = (*_NT_ConfigOpen)(&hCfgStream, "capture")) != NT_SUCCESS) {
    return _log_nt_errors(status, "NT_ConfigOpen() failed", __func__);
  }

  pNtplInfo = (NtNtplInfo_t *)rte_malloc(internals->name, sizeof(NtNtplInfo_t), 0);
  if (!pNtplInfo) {
    return _log_out_of_memory_errors(__func__);
  }

  if (internals->ntpl_file) {
    fd = open(internals->ntpl_file, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
      PMD_NTACC_LOG(ERR, "NTPL dump failed: Unable to open file %s. Error %d\n", internals->ntpl_file, errno);
    }
    _write_to_file(fd, ntplStr); _write_to_file(fd, "\n");
    close(fd);
  }

  PMD_NTACC_LOG(DEBUG, "NTPL : %s\n", ntplStr);
  if((status = (*_NT_NTPL)(hCfgStream, ntplStr, pNtplInfo, NT_NTPL_PARSER_VALIDATE_NORMAL)) != NT_SUCCESS) {
    // Get the status code as text
    (*_NT_ExplainError)(status, errorBuffer, sizeof(errorBuffer)-1);
    PMD_NTACC_LOG(ERR, "NT_NTPL() failed: %s\n", errorBuffer);
    PMD_NTACC_LOG(ERR, ">>> NTPL errorcode: %X\n", pNtplInfo->u.errorData.errCode);
    PMD_NTACC_LOG(ERR, ">>> %s\n", pNtplInfo->u.errorData.errBuffer[0]);
    PMD_NTACC_LOG(ERR, ">>> %s\n", pNtplInfo->u.errorData.errBuffer[1]);
    PMD_NTACC_LOG(ERR, ">>> %s\n", pNtplInfo->u.errorData.errBuffer[2]);
    (*_NT_ConfigClose)(hCfgStream);

    if (internals->ntpl_file) {
      fd = open(internals->ntpl_file, O_WRONLY | O_APPEND | O_CREAT, 0666);
      if (fd == -1) {
        PMD_NTACC_LOG(ERR, "NT_NTPL() dump failed: Unable to open file %s. Error %d\n", internals->ntpl_file, errno);
      }
      _write_to_file(fd, pNtplInfo->u.errorData.errBuffer[0]); _write_to_file(fd, "\n");
      _write_to_file(fd, pNtplInfo->u.errorData.errBuffer[1]); _write_to_file(fd, "\n");
      _write_to_file(fd, pNtplInfo->u.errorData.errBuffer[2]); _write_to_file(fd, "\n");
      close(fd);
    }
    rte_free(pNtplInfo);
    return -1;
  }
  // Return ntpl ID
  if (pNtplID) {
    *pNtplID = pNtplInfo->ntplId;
  }
  PMD_NTACC_LOG(DEBUG, "NTPL : %d\n", pNtplInfo->ntplId);

  (*_NT_ConfigClose)(hCfgStream);
  rte_free(pNtplInfo);
  return 0;
}

static uint16_t eth_ntacc_rx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  struct rte_mbuf *mbuf;
  struct ntacc_rx_queue *rx_q = queue;
#ifdef USE_SW_STAT
  uint32_t bytes = 0;
#endif
  uint16_t num_rx = 0;

  if (unlikely(rx_q->pNetRx == NULL || nb_pkts == 0))
    return 0;

  // Do we have any segment
  if (rx_q->pSeg == NULL) {
    int status = (*_NT_NetRxGet)(rx_q->pNetRx, &rx_q->pSeg, 0);
    if (status != NT_SUCCESS) {
      if (rx_q->pSeg != NULL) {
        (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
        rx_q->pSeg = NULL;
      }
      return 0;
    }

    if (likely(NT_NET_GET_SEGMENT_LENGTH(rx_q->pSeg))) {
      _nt_net_build_pkt_netbuf(rx_q->pSeg, &rx_q->pkt);
    }
    else {
      (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
      rx_q->pSeg = NULL;
      return 0;
    }
  }

#if 0
  if (rx_q->cmbatch) {
    struct batch_ctrl *batchCtl;
    uint64_t countPackets;

    if (unlikely(rte_mempool_get_bulk(rx_q->mb_pool, (void **)bufs, 1) != 0)) {
      return 0;
    }

    mbuf = bufs[0];

    rte_mbuf_refcnt_set(mbuf, 1);
    rte_pktmbuf_reset(mbuf);

    batchCtl = (struct batch_ctrl *)((u_char *)mbuf->buf_addr + RTE_PKTMBUF_HEADROOM);
    batchCtl->queue = queue;
    batchCtl->pSeg = rx_q->pSeg;

    /* Hand over release responsibility and ownership */
    rx_q->pSeg = NULL;

    mbuf->port = rx_q->in_port;
    mbuf->ol_flags |= PKT_BATCH | CTRL_MBUF_FLAG;
    mbuf->cmbatch_release_cb = _seg_release_cb;

    /* let userdata point to original mbuf address where batchCtl is placed */
    mbuf->userdata = (void *)batchCtl;

    /* save buf_addr */
    batchCtl->orig_buf_addr = mbuf->buf_addr;

    NtDyn3Descr_t *hdr = (NtDyn3Descr_t*)batchCtl->pSeg->hHdr;
    mbuf->buf_addr = (uint8_t *)batchCtl->pSeg->hHdr;
    mbuf->data_off = 0;

    mbuf->data_len = hdr->capLength;
    mbuf->pkt_len = (uint32_t)batchCtl->pSeg->length;
    num_rx++;

    /* do packet count */
    mbuf->batch_nb_packet = 0;
    countPackets = 0;
    do {
      countPackets++;
    } while (_nt_net_get_next_packet(batchCtl->pSeg, NT_NET_GET_SEGMENT_LENGTH(batchCtl->pSeg), &rx_q->pkt)>0);
    mbuf->batch_nb_packet = countPackets;

#ifdef USE_SW_STAT
    rx_q->rx_pkts += mbuf->batch_nb_packet;
    rx_q->rx_bytes += batchCtl->pSeg->length - mbuf->batch_nb_packet * hdr->descrLength;
#endif
    return num_rx;
  }
  else
#endif
  {
    NtDyn3Descr_t *dyn3;
    uint16_t i;
    uint16_t mbuf_len;
    uint16_t data_len;

    if (rte_mempool_get_bulk(rx_q->mb_pool, (void **)bufs, nb_pkts) != 0)
      return 0;

    for (i = 0; i < nb_pkts; i++) {
      mbuf = bufs[i];
      rte_mbuf_refcnt_set(mbuf, 1);
      rte_pktmbuf_reset(mbuf);

      dyn3 = _NT_NET_GET_PKT_DESCR_PTR_DYN3(&rx_q->pkt);

      if (dyn3->descrLength == 20) {
        // We do have a hash value defined
        mbuf->hash.rss = dyn3->color_hi;
        mbuf->ol_flags |= PKT_RX_RSS_HASH;
      }
      else {
        // We do have a color value defined
        mbuf->hash.fdir.hi = ((dyn3->color_hi << 14) & 0xFFFFC000) | dyn3->color_lo;
        mbuf->ol_flags |= PKT_RX_FDIR_ID | PKT_RX_FDIR;
      }

      mbuf->timestamp = dyn3->timestamp;
      mbuf->ol_flags |= PKT_RX_TIMESTAMP;
      mbuf->port = rx_q->in_port + (dyn3->rxPort - rx_q->local_port);

      data_len = (uint16_t)(dyn3->capLength - dyn3->descrLength - 4);
      mbuf_len = rte_pktmbuf_tailroom(mbuf);
#ifdef USE_SW_STAT
      bytes += data_len+4;
#endif
      if (data_len <= mbuf_len) {
        /* Packet will fit in the mbuf, go ahead and copy */
        mbuf->pkt_len = mbuf->data_len = data_len;
				rte_memcpy((u_char *)mbuf->buf_addr + mbuf->data_off, (uint8_t *)dyn3 + dyn3->descrLength, mbuf->data_len);
#ifdef COPY_OFFSET0
        mbuf->data_off += dyn3->offset0;
#endif
      } else {
        /* Try read jumbo frame into multi mbufs. */
        struct rte_mbuf *m;
        const u_char *data;
        uint16_t total_len = data_len;

        mbuf->pkt_len = total_len;
        mbuf->data_len = mbuf_len;
        data = (u_char *)dyn3 + dyn3->descrLength;
        rte_memcpy((u_char *)mbuf->buf_addr + mbuf->data_off, data, mbuf_len);
        data_len -= mbuf_len;
        data += mbuf_len;

        m = mbuf;
        while (data_len > 0) {
          /* Allocate next mbuf and point to that. */
          m->next = rte_pktmbuf_alloc(rx_q->mb_pool);
          if (unlikely(!m->next))
            return 0;

          m = m->next;
          /* Copy next segment. */
          mbuf_len = RTE_MIN(rte_pktmbuf_tailroom(m), data_len);
          rte_memcpy((u_char *)m->buf_addr + m->data_off, data, mbuf_len);

          m->pkt_len = total_len;
          m->data_len = mbuf_len;

          mbuf->nb_segs++;
          data_len -= mbuf_len;
          data += mbuf_len;
        }
      }
      num_rx++;

      /* Get the next packet if any */
      if (_nt_net_get_next_packet(rx_q->pSeg, NT_NET_GET_SEGMENT_LENGTH(rx_q->pSeg), &rx_q->pkt) == 0 ) {
        (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
        rx_q->pSeg = NULL;
        break;
      }
    }
#ifdef USE_SW_STAT
    rx_q->rx_pkts+=num_rx;
    rx_q->rx_bytes+=bytes;
#endif
    if (num_rx < nb_pkts) {
      rte_mempool_put_bulk(rx_q->mb_pool, (void * const *)(bufs + num_rx), nb_pkts-num_rx);
    }
    return num_rx;
  }
}

/*
 * Callback to handle sending packets through a real NIC.
 */
#if 0
static uint16_t eth_ntacc_tx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  unsigned i;
  int ret;
  struct ntacc_tx_queue *tx_q = queue;
  NtNetBuf_t hNetBufTx;

  if (unlikely(tx_q == NULL || tx_q->pNetTx == NULL || nb_pkts == 0)) {
    return 0;
  }

  for (i = 0; i < nb_pkts; i++) {
    struct rte_mbuf *mbuf = bufs[i];
    // Get a TX buffer for this packet
    ret = (*_NT_NetTxGet)(tx_q->pNetTx, &hNetBufTx, tx_q->port, MAX(mbuf->data_len + 4, 64), NT_NETTX_PACKET_OPTION_L2, -1);
    if(unlikely(ret != NT_SUCCESS)) {
      _log_nt_errors(ret, "NT_NetTxGet() failed", __func__);
#ifdef USE_SW_STAT
      tx_q->err_pkts += (nb_pkts - i);
#endif
      return i;
    }
    NT_NET_SET_PKT_TXNOW(hNetBufTx, 1);
    rte_memcpy(NT_NET_GET_PKT_L2_PTR(hNetBufTx), rte_pktmbuf_mtod(mbuf, u_char *), mbuf->data_len);

    // Release the TX buffer and the packet will be transmitted
    ret = (*_NT_NetTxRelease)(tx_q->pNetTx, hNetBufTx);
    if(unlikely(ret != NT_SUCCESS)) {
      _log_nt_errors(ret, "NT_NetTxRelease() failed", __func__);
#ifdef USE_SW_STAT
      tx_q->err_pkts += (nb_pkts - i);
#endif
      return i;
    }

    rte_pktmbuf_free(bufs[i]);
  }
#ifdef USE_SW_STAT
      tx_q->tx_pkts += i;
#endif
  return i;
}
#elif 0
static uint16_t eth_ntacc_tx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  int status;
  struct ntacc_tx_queue *tx_q = queue;
  struct NtNetBuf_s *hNetBufTx;
  struct NtNetBuf_s pktNetBuf;
  uint32_t spaceLeftInSegment;
  uint32_t tx_pkts;
  uint32_t packetsInSegment;
  struct rte_mbuf *mbuf;

  if (unlikely(tx_q == NULL || tx_q->pNetTx == NULL || nb_pkts == 0)) {
    return 0;
  }

  spaceLeftInSegment = 0;
  packetsInSegment = 0;
  tx_pkts = 0;
  mbuf = bufs[tx_pkts];
  while (tx_pkts < nb_pkts) {
    for (;;) {
      if (spaceLeftInSegment == 0) {
        do {
          // Get a TX segment of size SEGMENT_LENGTH
          if((status = (*_NT_NetTxGet)(tx_q->pNetTx, &hNetBufTx, tx_q->port, SEGMENT_LENGTH, NT_NETTX_SEGMENT_OPTION_RAW, 1000)) != NT_SUCCESS) {
            if(status != NT_STATUS_TIMEOUT) {
              char errorBuffer[NT_ERRBUF_SIZE]; // Error buffer
              NT_ExplainError(status, errorBuffer, NT_ERRBUF_SIZE);
              PMD_NTACC_LOG(ERR, "Failed to get a tx segment: %s\n", errorBuffer);
#ifdef USE_SW_STAT
              tx_q->err_pkts += (nb_pkts - tx_pkts);
#endif
              return tx_pkts;
            }
          }
        }
        while (status == NT_STATUS_TIMEOUT);
        spaceLeftInSegment = SEGMENT_LENGTH;
        packetsInSegment = 0;
        // Get a packet buffer pointer from the segment.
        _nt_net_build_pkt_netbuf(hNetBufTx, &pktNetBuf);
      }

      // Build the packet
      NT_NET_SET_PKT_CLEAR_DESCR_NT((&pktNetBuf));
      NT_NET_SET_PKT_DESCR_TYPE_NT((&pktNetBuf));
      NT_NET_SET_PKT_RECALC_L2_CRC((&pktNetBuf), 1);
      NT_NET_SET_PKT_TXPORT((&pktNetBuf), tx_q->port);
      NT_NET_UPDATE_PKT_L2_PTR((&pktNetBuf));
      NT_NET_SET_PKT_CAP_LENGTH((&pktNetBuf), (uint16_t)mbuf->data_len + 4);
      NT_NET_SET_PKT_WIRE_LENGTH((&pktNetBuf), (uint16_t)mbuf->data_len + 4);
      NT_NET_SET_PKT_TXNOW((&pktNetBuf), 1);

      rte_memcpy(NT_NET_GET_PKT_L2_PTR((&pktNetBuf)), rte_pktmbuf_mtod(mbuf, u_char *), mbuf->data_len);

      // Release buffer
      rte_pktmbuf_free(bufs[tx_pkts]);

      // Packets in the segment
      tx_pkts++;
      packetsInSegment++;

      // Move the pointer to next packet and get the length of the remining space in the segment
      spaceLeftInSegment = _nt_net_get_next_packet(hNetBufTx, NT_NET_GET_SEGMENT_LENGTH(hNetBufTx), &pktNetBuf);

      if (tx_pkts >= nb_pkts) {
        // No more packet. Transmit now
        break;
      }

      // Get the next packet size to next transmit.
      mbuf = bufs[tx_pkts];

      // Check if we have space for more packets including a 64 byte dummy packet.
      if (spaceLeftInSegment < (mbuf->data_len + 4 + 2 * NT_DESCR_NT_LENGTH + 64)) {
        // Check if we have space for excately one packet.
        if(spaceLeftInSegment != (mbuf->data_len + 4 + NT_DESCR_NT_LENGTH)) {
          // No more space in segment. Transmit now
          break;
        }
      }
    }

    // Fill the segment with dummy packets
    while (spaceLeftInSegment > 0) {
      if (spaceLeftInSegment > 9000) {
        // Create an 8K dummy packet
        NT_NET_SET_PKT_CLEAR_DESCR_NT((&pktNetBuf));
        NT_NET_SET_PKT_DESCR_TYPE_NT((&pktNetBuf));
        NT_NET_SET_PKT_CAP_LENGTH((&pktNetBuf), 8196);
        NT_NET_SET_PKT_WIRE_LENGTH((&pktNetBuf), 8196);
        NT_NET_SET_PKT_TXIGNORE((&pktNetBuf), 1);
        spaceLeftInSegment = _nt_net_get_next_packet(hNetBufTx, NT_NET_GET_SEGMENT_LENGTH(hNetBufTx), &pktNetBuf);
      }
      else {
        // Create a dummy packet
        NT_NET_SET_PKT_CLEAR_DESCR_NT((&pktNetBuf));
        NT_NET_SET_PKT_DESCR_TYPE_NT((&pktNetBuf));
        NT_NET_SET_PKT_CAP_LENGTH((&pktNetBuf), spaceLeftInSegment - NT_DESCR_NT_LENGTH);
        NT_NET_SET_PKT_WIRE_LENGTH((&pktNetBuf), spaceLeftInSegment - NT_DESCR_NT_LENGTH);
        NT_NET_SET_PKT_TXIGNORE((&pktNetBuf), 1);
        spaceLeftInSegment = 0;
      }
    }

    // Release the TX buffer and the segment will be transmitted
    if((status = (*_NT_NetTxRelease)(tx_q->pNetTx, hNetBufTx)) != NT_SUCCESS) {
      char errorBuffer[NT_ERRBUF_SIZE]; // Error buffer
      NT_ExplainError(status, errorBuffer, NT_ERRBUF_SIZE);
      PMD_NTACC_LOG(ERR, "Failed to get a tx segment: %s\n", errorBuffer);
#ifdef USE_SW_STAT
      tx_q->err_pkts += (nb_pkts - tx_pkts - packetsInSegment);
#endif
      return tx_pkts - packetsInSegment;
    }
    spaceLeftInSegment = 0;
    packetsInSegment = 0;
  }
#ifdef USE_SW_STAT
      tx_q->tx_pkts += tx_pkts;
#endif
  return tx_pkts;
}
#else
static uint16_t eth_ntacc_tx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  unsigned i;
  int ret;
  struct ntacc_tx_queue *tx_q = queue;
#ifdef USE_SW_STAT
  uint32_t bytes=0;
#endif

  if (unlikely(tx_q == NULL || tx_q->pNetTx == NULL || nb_pkts == 0)) {
    return 0;
  }

  for (i = 0; i < nb_pkts; i++) {
    uint16_t wLen;
    struct rte_mbuf *mbuf = bufs[i];
    struct NtNetTxFragment_s frag[10]; // Need fragments enough for a jumbo packet */
    uint8_t fragCnt = 0;
    frag[fragCnt].data = rte_pktmbuf_mtod(mbuf, u_char *);
    frag[fragCnt++].size = mbuf->data_len;
    wLen = mbuf->data_len + 4;
    if (unlikely(mbuf->nb_segs > 1)) {
      while (mbuf->next) {
        mbuf = mbuf->next;
        frag[fragCnt].data = rte_pktmbuf_mtod(mbuf, u_char *);
        frag[fragCnt++].size = mbuf->data_len;
        wLen += mbuf->data_len;
      }
    }
    /* Check if packet needs padding or is too big to transmit */
    if (unlikely(wLen < tx_q->minTxPktSize)) {
      frag[fragCnt].data = rte_pktmbuf_mtod(mbuf, u_char *);
      frag[fragCnt++].size = tx_q->minTxPktSize - wLen;
    }
    if (unlikely(wLen > tx_q->maxTxPktSize)) {
      /* Packet is too big. Drop it as an error and continue */
#ifdef USE_SW_STAT
      tx_q->err_pkts++;
#endif
      rte_pktmbuf_free(bufs[i]);
      continue;
    }
    ret = (*_NT_NetTxAddPacket)(tx_q->pNetTx, tx_q->port, frag, fragCnt, 0);
    if (unlikely(ret != NT_SUCCESS)) {
      /* unsent packets is not expected to be freed */
#ifdef USE_SW_STAT
      tx_q->err_pkts++;
#endif
      break;
    }
#ifdef USE_SW_STAT
    bytes += wLen;
#endif
    rte_pktmbuf_free(bufs[i]);
  }
#ifdef USE_SW_STAT
  tx_q->tx_pkts += i;
  tx_q->tx_bytes += bytes;
#endif

  return i;
}
#endif

static int eth_dev_start(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
#ifndef DO_NOT_CREATE_DEFAULT_FILTER
  struct rte_flow_error error;
#endif
  struct ntacc_rx_queue *rx_q = internals->rxq;
  struct ntacc_tx_queue *tx_q = internals->txq;
  uint queue;
  int status;
  char *shm;

  // Open or create shared memory
  internals->key = 135546;
  if ((internals->shmid = shmget(internals->key, sizeof(struct pmd_shared_mem_s), 0666)) < 0) {
    if ((internals->shmid = shmget(internals->key, sizeof(struct pmd_shared_mem_s), IPC_CREAT | 0666)) < 0) {
      PMD_NTACC_LOG(ERR, "Unable to create shared mem size %u in eth_dev_start. Error = %d \"%s\"\n", (unsigned int)sizeof(struct pmd_shared_mem_s), errno, strerror(errno));
      goto StartError;
    }
    if ((shm = shmat(internals->shmid, NULL, 0)) == (char *) -1) {
      PMD_NTACC_LOG(ERR, "Unable to attach to shared mem in eth_dev_start. Error = %d \"%s\"\n", errno, strerror(errno));
      goto StartError;
    }
    memset(shm, 0, sizeof(struct pmd_shared_mem_s));
    internals->shm = (struct pmd_shared_mem_s *)shm;
  }
  else {
    bool clearMem = false;
    struct shmid_ds shmid_ds;
    if(shmctl(internals->shmid, IPC_STAT, &shmid_ds) != -1) {
      if(shmid_ds.shm_nattch == 0) {
        clearMem = true;
      }
    }
    if ((shm = shmat(internals->shmid, NULL, 0)) == (char *) -1) {
      PMD_NTACC_LOG(ERR, "Unable to attach to shared mem in eth_dev_start. Error = %d\n", errno);
      goto StartError;
    }
    if (clearMem) {
      memset(shm, 0, sizeof(struct pmd_shared_mem_s));
    }
    internals->shm = (struct pmd_shared_mem_s *)shm;
  }

  // Create interprocess mutex
  status = pthread_mutexattr_init(&internals->psharedm);
  if (status) {
    PMD_NTACC_LOG(ERR, "Unable to create mutex 1. Error = %d \"%s\"\n", status, strerror(status));
    goto StartError;
  }
  status = pthread_mutexattr_setpshared(&internals->psharedm, PTHREAD_PROCESS_SHARED);
  if (status) {
    PMD_NTACC_LOG(ERR, "Unable to create mutex 2. Error = %d \"%s\"\n", status, strerror(status));
    goto StartError;
  }
  status = pthread_mutex_init(&internals->shm->mutex, &internals->psharedm);
  if (status) {
    PMD_NTACC_LOG(ERR, "Unable to create mutex 3. Error = %d \"%s\"\n", status, strerror(status));
    goto StartError;
  }

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      uint32_t ntplID;
      char ntpl_buf[21];
      /* Delete all NTPL */
      snprintf(ntpl_buf, 20, "Delete=tag==%s", internals->tagName);
      if (DoNtpl(ntpl_buf, &ntplID, internals) != 0) {
        PMD_NTACC_LOG(ERR, "Failed to create delete filters in eth_dev_start\n");
        goto StartError;
      }
      break;
    }
  }

#ifndef DO_NOT_CREATE_DEFAULT_FILTER
  _dev_flow_isolate(dev, 0, &error);
#endif

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      if ((status = (*_NT_NetRxOpen)(&rx_q[queue].pNetRx, "DPDK", NT_NET_INTERFACE_SEGMENT, rx_q[queue].stream_id, -1)) != NT_SUCCESS) {
        _log_nt_errors(status, "NT_NetRxOpen() failed", __func__);
        goto StartError;
      }
      eth_rx_queue_start(dev, queue);
    }
  }

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (tx_q[queue].enabled) {
      if ((status = (*_NT_NetTxOpen)(&tx_q[queue].pNetTx, "DPDK", 1 << tx_q[queue].port, -1, 0)) != NT_SUCCESS) {
        if ((status = (*_NT_NetTxOpen)(&tx_q[queue].pNetTx, "DPDK", 1 << tx_q[queue].port, -2, 0)) != NT_SUCCESS) {
          _log_nt_errors(status, "NT_NetTxOpen() with -2 failed", __func__);
          goto StartError;
        }
        PMD_NTACC_LOG(DEBUG, "NT_NetTxOpen() Not optimal hostbuffer found on a neighbour numa node\n");
      }
    }
    tx_q[queue].plock = &port_locks[tx_q[queue].port];
  }

  dev->data->dev_link.link_status = 1;
  return 0;

StartError:
  return -1;
}

/*
 * This function gets called when the current port gets stopped.
 * Is the only place for us to close all the tx streams dumpers.
 * If not called the dumpers will be flushed within each tx burst.
 */
static void eth_dev_stop(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  struct ntacc_rx_queue *rx_q = internals->rxq;
  struct ntacc_tx_queue *tx_q = internals->txq;
  struct rte_flow_error error;
  uint queue;

  PMD_NTACC_LOG(DEBUG, "Stopping port %u (%u) on adapter %u\n", internals->port, deviceCount, internals->adapterNo);
  _dev_flow_isolate(dev, 1, &error);
  _dev_flow_flush(dev, &error);
  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      if (rx_q[queue].pSeg) {
        (*_NT_NetRxRelease)(rx_q[queue].pNetRx, rx_q[queue].pSeg);
        rx_q[queue].pSeg = NULL;
      }
      if (rx_q[queue].pNetRx) {
          (void)(*_NT_NetRxClose)(rx_q[queue].pNetRx);
      }
      eth_rx_queue_stop(dev, queue);
    }
  }
  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (tx_q[queue].enabled) {
      if (tx_q[queue].pNetTx) {
        (void)(*_NT_NetTxClose)(tx_q[queue].pNetTx);
      }
    }
  }

#ifndef USE_SW_STAT
  rte_spinlock_lock(&internals->statlock);
  if (internals->hStat) {
    (void)(*_NT_StatClose)(internals->hStat);
  }
  rte_spinlock_unlock(&internals->statlock);
#endif
  dev->data->dev_link.link_status = 0;

  // Detach shared memory
  shmdt(internals->shm);
  shmctl(internals->shmid, IPC_RMID, NULL);
}

static int eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
    internals->rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
  }
  else {
    internals->rss_hf = 0;
  }
  return 0;
}

static void eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
  struct pmd_internals *internals = dev->data->dev_private;
  NtInfoStream_t hInfo;
  NtInfo_t *pInfo;
  uint status;

  dev_info->if_index = internals->if_index;
  dev_info->driver_name = internals->driverName;
  dev_info->max_mac_addrs = 1;
  dev_info->max_rx_pktlen = HW_MTU;
  dev_info->max_rx_queues = STREAMIDS_PER_PORT > RTE_ETHDEV_QUEUE_STAT_CNTRS ? RTE_ETHDEV_QUEUE_STAT_CNTRS : STREAMIDS_PER_PORT;
  dev_info->max_tx_queues = STREAMIDS_PER_PORT > RTE_ETHDEV_QUEUE_STAT_CNTRS ? RTE_ETHDEV_QUEUE_STAT_CNTRS : STREAMIDS_PER_PORT;
  dev_info->min_rx_bufsize = 0;
  dev_info->default_txconf.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS;
  dev_info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);

  pInfo = (NtInfo_t *)rte_malloc(internals->name, sizeof(NtInfo_t), 0);
  if (!pInfo) {
    _log_out_of_memory_errors(__func__);
    return;
  }

  // Read speed capabilities for the port
  if ((status = (*_NT_InfoOpen)(&hInfo, "DPDK Info stream")) != NT_SUCCESS) {
    _log_nt_errors(status, "NT_InfoOpen failed", __func__);
    rte_free(pInfo);
    return;
  }

  pInfo->cmd = NT_INFO_CMD_READ_PORT_V8;
  pInfo->u.port_v8.portNo = (uint8_t)(internals->txq[0].port);
  if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
    _log_nt_errors(status, "NT_InfoRead failed", __func__);
    rte_free(pInfo);
    return;
  }
  (void)(*_NT_InfoClose)(hInfo);

  // Update speed capabilities for the port
  dev_info->speed_capa = 0;
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_10M) {
    dev_info->speed_capa |= ETH_LINK_SPEED_10M;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_100M) {
    dev_info->speed_capa |= ETH_LINK_SPEED_100M;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_1G) {
    dev_info->speed_capa |= ETH_LINK_SPEED_1G;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_10G) {
    dev_info->speed_capa |= ETH_LINK_SPEED_10G;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_40G) {
    dev_info->speed_capa |= ETH_LINK_SPEED_40G;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_100G) {
    dev_info->speed_capa |= ETH_LINK_SPEED_100G;
  }
  if (pInfo->u.port_v7.data.capabilities.speed & NT_LINK_SPEED_50G) {
    dev_info->speed_capa |= ETH_LINK_SPEED_50G;
  }
  rte_free(pInfo);
}

#ifdef USE_SW_STAT
static int eth_stats_get(struct rte_eth_dev *dev,
                          struct rte_eth_stats *igb_stats)
{
  unsigned i;
  uint64_t rx_total = 0;
  uint64_t tx_total = 0;
  uint64_t tx_err_total = 0;
  uint64_t rx_total_bytes = 0;
  uint64_t tx_total_bytes = 0;
  const struct pmd_internals *internal = dev->data->dev_private;

  memset(igb_stats, 0, sizeof(*igb_stats));
  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    if (internal->rxq[i].enabled) {
      igb_stats->q_ipackets[i] = internal->rxq[i].rx_pkts;
      igb_stats->q_ibytes[i] = internal->rxq[i].rx_bytes;
      rx_total += igb_stats->q_ipackets[i];
      rx_total_bytes += igb_stats->q_ibytes[i];
    }
  }

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    if (internal->txq[i].enabled) {
      igb_stats->q_opackets[i] = internal->txq[i].tx_pkts;
      igb_stats->q_obytes[i] = internal->txq[i].tx_bytes;
      igb_stats->q_errors[i] = internal->txq[i].err_pkts;
      tx_total += igb_stats->q_opackets[i];
      tx_total_bytes += igb_stats->q_obytes[i];
      tx_err_total += igb_stats->q_errors[i];
    }
  }

  igb_stats->ipackets = rx_total;
  igb_stats->opackets = tx_total;
  igb_stats->ibytes = rx_total_bytes;
  igb_stats->obytes = tx_total_bytes;
  igb_stats->oerrors = tx_err_total;
  return 0;
}
#else
static int eth_stats_get(struct rte_eth_dev *dev,
                         struct rte_eth_stats *igb_stats)
{
  struct pmd_internals *internals = dev->data->dev_private;
  uint queue;
  int status;
  NtStatistics_t *pStatData;
  uint8_t port;

  pStatData = (NtStatistics_t *)rte_malloc(internals->name, sizeof(NtStatistics_t), 0);
  if (!pStatData) {
    return _log_out_of_memory_errors(__func__);
  }

  memset(igb_stats, 0, sizeof(*igb_stats));

  /* port used */
  port = (uint8_t)internals->txq[0].port;

  /* Get stat data */
  pStatData->cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
  pStatData->u.query_v2.poll=0;
  pStatData->u.query_v2.clear=0;
  rte_spinlock_lock(&internals->statlock);
  if ((status = (*_NT_StatRead)(internals->hStat, pStatData)) != 0) {
    rte_spinlock_unlock(&internals->statlock);
    _log_nt_errors(status, "NT_StatRead failed", __func__);
    rte_free(pStatData);
    return -EIO;
  }
  rte_spinlock_unlock(&internals->statlock);
  igb_stats->ipackets = pStatData->u.query_v2.data.port.aPorts[port].rx.RMON1.pkts;
  igb_stats->ibytes = pStatData->u.query_v2.data.port.aPorts[port].rx.RMON1.octets;
  igb_stats->opackets = pStatData->u.query_v2.data.port.aPorts[port].tx.RMON1.pkts;
  igb_stats->obytes = pStatData->u.query_v2.data.port.aPorts[port].tx.RMON1.octets;
  igb_stats->imissed = pStatData->u.query_v2.data.port.aPorts[port].rx.extDrop.pktsOverflow;
  igb_stats->ierrors = pStatData->u.query_v2.data.port.aPorts[port].rx.RMON1.crcAlignErrors;
  igb_stats->oerrors = pStatData->u.query_v2.data.port.aPorts[port].tx.RMON1.crcAlignErrors;

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    igb_stats->q_ipackets[queue] = pStatData->u.query_v2.data.stream.streamid[internals->rxq[queue].stream_id].forward.pkts;
    igb_stats->q_ibytes[queue] =  pStatData->u.query_v2.data.stream.streamid[internals->rxq[queue].stream_id].forward.octets;
    igb_stats->q_errors[queue] = internals->txq[queue].err_pkts;

  }
  rte_free(pStatData);
  return 0;
}
#endif

#ifdef USE_SW_STAT
static void eth_stats_reset(struct rte_eth_dev *dev)
{
  unsigned i;
  struct pmd_internals *internal = dev->data->dev_private;

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internal->rxq[i].rx_pkts = 0;
    internal->rxq[i].rx_bytes = 0;
  }
  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internal->txq[i].tx_pkts = 0;
    internal->txq[i].tx_bytes = 0;
    internal->txq[i].err_pkts = 0;
  }
}
#else
static void eth_stats_reset(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  int status;
  NtStatistics_t *pStatData;

  pStatData = (NtStatistics_t *)rte_malloc(internals->name, sizeof(NtStatistics_t), 0);
  if (!pStatData) {
    _log_out_of_memory_errors(__func__);
    return;
  }

  pStatData->cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
  pStatData->u.query_v2.poll=0;
  pStatData->u.query_v2.clear=1;
  rte_spinlock_lock(&internals->statlock);
  if ((status = (*_NT_StatRead)(internals->hStat, pStatData)) != 0) {
    rte_spinlock_unlock(&internals->statlock);
    _log_nt_errors(status, "NT_StatRead failed", __func__);
    rte_free(pStatData);
    return;
  }
  rte_spinlock_unlock(&internals->statlock);
  rte_free(pStatData);
}
#endif

static void eth_dev_close(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  PMD_NTACC_LOG(DEBUG, "Closing port %u (%u) on adapter %u\n", internals->port, deviceCount, internals->adapterNo);

  if (internals->ntpl_file) {
    rte_free(internals->ntpl_file);
  }
  rte_free(dev->data->dev_private);
  rte_eth_dev_release_port(dev);

  deviceCount--;
  if (deviceCount == 0 && _libnt != NULL) {
    PMD_NTACC_LOG(DEBUG, "Closing dyn lib\n");
    dlclose(_libnt);
  }
}

static void eth_queue_release(void *q __rte_unused)
{
}

static int eth_link_update(struct rte_eth_dev *dev,
                           int wait_to_complete  __rte_unused)
{
  NtInfoStream_t hInfo;
  NtInfo_t *pInfo;
  uint status;
  struct pmd_internals *internals = dev->data->dev_private;

  pInfo = (NtInfo_t *)rte_malloc(internals->name, sizeof(NtInfo_t), 0);
  if (!pInfo) {
    return _log_out_of_memory_errors(__func__);
  }

  if ((status = (*_NT_InfoOpen)(&hInfo, "DPDK Info stream")) != NT_SUCCESS) {
    _log_nt_errors(status, "NT_InfoOpen failed", __func__);
    rte_free(pInfo);
    return status;
  }
  pInfo->cmd = NT_INFO_CMD_READ_PORT_V8;
  pInfo->u.port_v8.portNo = (uint8_t)(internals->txq[0].port);
  if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
    _log_nt_errors(status, "NT_InfoRead failed", __func__);
    rte_free(pInfo);
    return status;
  }
  (void)(*_NT_InfoClose)(hInfo);

  dev->data->dev_link.link_status = pInfo->u.port_v8.data.state == NT_LINK_STATE_UP ? 1 : 0;
  switch (pInfo->u.port_v8.data.speed) {
  case NT_LINK_SPEED_UNKNOWN:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10M:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_10M;
    break;
  case NT_LINK_SPEED_100M:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_100M;
    break;
  case NT_LINK_SPEED_1G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_10G;
    break;
  case NT_LINK_SPEED_40G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_40G;
    break;
  case NT_LINK_SPEED_50G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_50G;
    break;
  case NT_LINK_SPEED_100G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_100G;
    break;
  }
  rte_free(pInfo);
  return 0;
}

static int eth_rx_queue_setup(struct rte_eth_dev *dev,
                              uint16_t rx_queue_id,
                              uint16_t nb_rx_desc __rte_unused,
                              unsigned int socket_id __rte_unused,
                              const struct rte_eth_rxconf *rx_conf,
                              struct rte_mempool *mb_pool)
{
  struct rte_pktmbuf_pool_private *mbp_priv;
  struct pmd_internals *internals = dev->data->dev_private;
  struct ntacc_rx_queue *rx_q = &internals->rxq[rx_queue_id];

  rx_q->mb_pool = mb_pool;
  dev->data->rx_queues[rx_queue_id] = rx_q;
  rx_q->in_port = dev->data->port_id;
  rx_q->local_port = internals->local_port;

#if 0
  // Enable contiguous memory batching for this queue
  if (rx_conf->rxq_flags & ETH_RXQ_FLAGS_CMBATCH) {
    rx_q->cmbatch = 1;
  }
#endif

  mbp_priv =  rte_mempool_get_priv(rx_q->mb_pool);
  rx_q->buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);
  rx_q->enabled = 1;
  return 0;
}

static int eth_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
  struct pmd_internals *internals = dev->data->dev_private;
  char ntpl_buf[50];
  snprintf(ntpl_buf, sizeof(ntpl_buf),
    "Setup[State=Active] = StreamId == %d",
    internals->rxq[rx_queue_id].stream_id);
  DoNtpl(ntpl_buf, NULL, internals);
  dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
  return 0;
}

static int eth_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
  struct pmd_internals *internals = dev->data->dev_private;
  char ntpl_buf[50];
  snprintf(ntpl_buf, sizeof(ntpl_buf),
    "Setup[State=InActive] = StreamId == %d",
    internals->rxq[rx_queue_id].stream_id);
  DoNtpl(ntpl_buf, NULL, internals);
  dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
  return 0;
}

static int eth_tx_queue_setup(struct rte_eth_dev *dev,
                              uint16_t tx_queue_id,
                              uint16_t nb_tx_desc __rte_unused,
                              unsigned int socket_id __rte_unused,
                              const struct rte_eth_txconf *tx_conf __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  dev->data->tx_queues[tx_queue_id] = &internals->txq[tx_queue_id];
  internals->txq[tx_queue_id].enabled = 1;
  return 0;
}

static int _dev_set_mtu(struct rte_eth_dev *dev __rte_unused, uint16_t mtu)
{
  if (mtu < 46 || mtu > HW_MTU)
    return -EINVAL;

  return 0;
}

static int _dev_get_flow_ctrl(struct rte_eth_dev *dev __rte_unused, struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}

static int _dev_set_flow_ctrl(struct rte_eth_dev *dev __rte_unused, struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}

static const char *ActionErrorString(enum rte_flow_action_type type)
{
  switch (type) {
  case RTE_FLOW_ACTION_TYPE_PASSTHRU: return "Action PASSTHRU is not supported";
  case RTE_FLOW_ACTION_TYPE_MARK:     return "Action MARK is not supported";
  case RTE_FLOW_ACTION_TYPE_FLAG:     return "Action FLAG is not supported";
  case RTE_FLOW_ACTION_TYPE_DROP:     return "Action DROP is not supported";
  case RTE_FLOW_ACTION_TYPE_COUNT:    return "Action COUNT is not supported";
  case RTE_FLOW_ACTION_TYPE_DUP:      return "Action DUP is not supported";
  case RTE_FLOW_ACTION_TYPE_PF:       return "Action PF is not supported";
  case RTE_FLOW_ACTION_TYPE_VF:       return "Action VF is not supported";
  default:                            return "Action is UNKNOWN";
  }
}

// Do only release the keyset if it is not in used anymore.
// This means that is it not referenced in any other flow.
// No lock in this code
static void _cleanUpKeySet(int key, struct pmd_internals *internals)
{
  struct rte_flow *pTmp;
  LIST_FOREACH(pTmp, &internals->flows, next) {
    if (pTmp->key == key) {
      // Key set is still in use
      return;
    }
  }
  // Key set is not in use anymore. delete it.
  PMD_NTACC_LOG(DEBUG, "Returning keyset %u: %d\n", internals->adapterNo, key);
  DeleteKeyset(key, internals);
  ReturnKeysetValue(internals, key);
}

// Do only delete the assign command if it is not in used anymore.
// This means that is it not referenced in any other flow.
// No lock in this code
static void _cleanUpAssignNtplId(uint32_t assignNtplID, struct pmd_internals *internals)
{
  char ntpl_buf[21];
  struct rte_flow *pFlow;
  LIST_FOREACH(pFlow, &internals->flows, next) {
    if (pFlow->assign_ntpl_id == assignNtplID) {
      // NTPL ID still in use
      return;
    }
  }
  // NTPL ID not in use
  PMD_NTACC_LOG(DEBUG, "Deleting assign filter: %u\n", assignNtplID);
  snprintf(ntpl_buf, 20, "delete=%d", assignNtplID);
  DoNtpl(ntpl_buf, NULL, internals);
}

// Delete a flow by deleting the NTPL command assigned
// with the flow. Check if some of the shared components
// like keyset and assign filter is still in use.
// No lock in this code
static void _cleanUpFlow(struct rte_flow *flow, struct pmd_internals *internals)
{
  char ntpl_buf[21];
  PMD_NTACC_LOG(DEBUG, "Remove flow %p\n", flow);
  LIST_REMOVE(flow, next);
  while (!LIST_EMPTY(&flow->ntpl_id)) {
    struct filter_flow *id;
    id = LIST_FIRST(&flow->ntpl_id);
    snprintf(ntpl_buf, 20, "delete=%d", id->ntpl_id);
    DoNtpl(ntpl_buf, NULL, internals);
    PMD_NTACC_LOG(DEBUG, "Deleting Item filter: %s\n", ntpl_buf);
    LIST_REMOVE(id, next);
    rte_free(id);
  }
  _cleanUpAssignNtplId(flow->assign_ntpl_id, internals);
  _cleanUpKeySet(flow->key, internals);
  rte_free(flow);
}

static struct rte_flow *_dev_flow_create(struct rte_eth_dev *dev,
                                  const struct rte_flow_attr *attr,
                                  const struct rte_flow_item items[],
                                  const struct rte_flow_action actions[],
                                  struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  uint32_t ntplID;
  uint8_t nb_queues = 0;
  uint8_t list_queues[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  bool filterContinue = false;
  bool queueDefined = false;
  const struct rte_flow_action_rss *rss = NULL;
  uint32_t i;
  bool tunnel;
  int tunneltype;
  struct color_s color = {0, false};
  uint8_t nb_ports = 0;
  uint8_t list_ports[MAX_NTACC_PORTS];
  uint64_t rss_hf = 0;


  uint64_t typeMask = 0;
  bool reuse = false;

  char *ntpl_buf = NULL;
  char *filter_buf1 = NULL;
  struct rte_flow *flow = NULL;
  const char *ntpl_str = NULL;

  ntpl_buf = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
  if (!ntpl_buf) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }

  filter_buf1 = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
  if (!ntpl_buf) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }

  flow = rte_malloc(internals->name, sizeof(struct rte_flow), 0);
  if (!flow) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }
  memset(flow, 0, sizeof(struct rte_flow));

  if (attr->group) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL, "Attribute groups are not supported");
    goto FlowError;
  }
  if (attr->egress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL, "Attribute egress is not supported");
    goto FlowError;
  }
  if (!attr->ingress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, NULL, "Attribute ingress must be set");
    goto FlowError;
  }

  for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
    switch (actions->type)
    {
    default:
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, ActionErrorString(actions->type));
      goto FlowError;
    case RTE_FLOW_ACTION_TYPE_VOID:
      continue;
    case RTE_FLOW_ACTION_TYPE_MARK:
      color.color = ((const struct rte_flow_action_mark *)actions->conf)->id;
      color.valid = true;
      break;
    case RTE_FLOW_ACTION_TYPE_RSS:
      rss = (const struct rte_flow_action_rss *)actions->conf;
      if (queueDefined) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Queue or RSS already defined");
        goto FlowError;
      }
      queueDefined = true;
      if (rss->num > RTE_ETHDEV_QUEUE_STAT_CNTRS) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Number of RSS queues out of range");
        goto FlowError;
      }
      for (i = 0; i < rss->num; i++) {
        if (rss->queue[i] >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "RSS queue out of range");
          goto FlowError;
        }
        list_queues[nb_queues++] = rss->queue[i];

        // Set hash mode function
        if (rss->rss_conf) {
          rss_hf = rss->rss_conf->rss_hf;
        }
      }
      break;
    case RTE_FLOW_ACTION_TYPE_QUEUE:
      if (queueDefined) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Queue or RSS already defined");
        goto FlowError;
      }
      queueDefined = true;
      if (((const struct rte_flow_action_queue *)actions->conf)->index >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "queue out of range");
        goto FlowError;
      }
      list_queues[nb_queues++] = ((const struct rte_flow_action_queue *)actions->conf)->index;
      break;
    }
  }

  if (nb_queues == 0) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "A queue must be defined");
    goto FlowError;
  }

  // Check the queues
  for (i = 0; i < nb_queues; i++) {
    if (!internals->rxq[list_queues[i]].enabled) {
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "All defined queues must be enabled");
      goto FlowError;
    }
  }

  // Set the priority

  if (color.valid) {
#ifdef COPY_OFFSET0
    snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=22,colorbits=32,Offset0=%s[0];", attr->priority, STRINGIZE_VALUE_OF(COPY_OFFSET0));
#else
    snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=22,colorbits=32;", attr->priority);
#endif
  }
  else {
#ifdef COPY_OFFSET0
    snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=20,colorbits=14,Offset0=%s[0];", attr->priority, STRINGIZE_VALUE_OF(COPY_OFFSET0));
#else
    snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=20,colorbits=14;", attr->priority);
#endif
  }

  // Set the stream IDs
  CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, nb_queues, list_queues);

  // Set the filter expression
  tunnel = false;
  filter_buf1[0] = 0;
  for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
    switch (items->type) {
      case RTE_FLOW_ITEM_TYPE_NTPL:
        ntpl_str = ((const struct rte_flow_item_ntpl*)items->spec)->ntpl_str;
        if (((const struct rte_flow_item_ntpl*)items->spec)->tunnel == RTE_FLOW_NTPL_TUNNEL) {
          tunnel = true;
        }
        break;
    case RTE_FLOW_ITEM_TYPE_VOID:
      continue;
    case RTE_FLOW_ITEM_TYPE_PORT:
      if (nb_ports < MAX_NTACC_PORTS) {
        const struct rte_flow_item_port *spec = (const struct rte_flow_item_port *)items->spec;
        if (spec->index > internals->nbPortsOnAdapter) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Illegal port number in port flow. All port numbers must be from the same adapter");
          goto FlowError;
        }
        list_ports[nb_ports++] = spec->index + internals->local_port_offset;
      }
      break;
    case RTE_FLOW_ITEM_TYPE_ETH:
      if (SetEthernetFilter(items,
                            tunnel,
                            &typeMask,
                            internals) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up Ether filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_IPV4:
        if (SetIPV4Filter(&filter_buf1[strlen(filter_buf1)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask,
                          internals) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up IPV4 filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_IPV6:
      if (SetIPV6Filter(&filter_buf1[strlen(filter_buf1)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask,
                        internals) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up IPV6 filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_SCTP:
        if (SetSCTPFilter(&filter_buf1[strlen(filter_buf1)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask,
                          internals) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up TCP filter");
          goto FlowError;
        }
        break;

      case RTE_FLOW_ITEM_TYPE_TCP:
        if (SetTCPFilter(&filter_buf1[strlen(filter_buf1)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask,
                          internals) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up TCP filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_UDP:
      if (SetUDPFilter(&filter_buf1[strlen(filter_buf1)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask,
                        internals) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up UDP filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_ICMP:
        if (SetICMPFilter(&filter_buf1[strlen(filter_buf1)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask,
                          internals) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up ICMP filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_VLAN:
      if (SetVlanFilter(&filter_buf1[strlen(filter_buf1)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask,
                        internals) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up VLAN filter");
        goto FlowError;
      }
      break;

    case RTE_FLOW_ITEM_TYPE_MPLS:
      if (SetMplsFilter(&filter_buf1[strlen(filter_buf1)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask,
                        internals) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up MPLS filter");
        goto FlowError;
      }
      break;

    case  RTE_FLOW_ITEM_TYPE_GRE:
      if (SetGreFilter(&filter_buf1[strlen(filter_buf1)],
                       &filterContinue,
                       items,
                       &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up GRE filter");
        goto FlowError;
      }
      tunnel = true;
      break;
    case RTE_FLOW_ITEM_TYPE_GTPU:
      if (SetGtpFilter(&filter_buf1[strlen(filter_buf1)],
                       &filterContinue,
                       items,
                       &typeMask,
                       'U') != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up GTP-U filter");
        goto FlowError;
      }
      tunnel = true;
      break;
    case 	RTE_FLOW_ITEM_TYPE_GTPC:
      if (SetGtpFilter(&filter_buf1[strlen(filter_buf1)],
                       &filterContinue,
                       items,
                       &typeMask,
                       'C') != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up GTP-C filter");
        goto FlowError;
      }
      tunnel = true;
      break;


    case RTE_FLOW_ITEM_TYPE_VXLAN:
    case RTE_FLOW_ITEM_TYPE_NVGRE:
    case RTE_FLOW_ITEM_TYPE_IPinIP:
      switch (items->type) {
      case RTE_FLOW_ITEM_TYPE_VXLAN:
        tunneltype = VXLAN_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_NVGRE:
        tunneltype = NVGRE_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_IPinIP:
        tunneltype = IP_IN_IP_TUNNEL_TYPE;
        break;
      default:
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up tunnel filter");
        goto FlowError;
      }

      if (SetTunnelFilter(&filter_buf1[strlen(filter_buf1)],
                        &filterContinue,
                        tunneltype,
                        &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up tunnel filter");
        goto FlowError;
      }
      tunnel = true;
      break;

    default:
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Item is not supported");
      goto FlowError;

    }
  }

  // Create HASH
  if (rss && rss->rss_conf) {
    // If RSS is used, then set the Hash mode
    if (CreateHash(&ntpl_buf[strlen(ntpl_buf)], rss_hf, internals, tunnel) != 0) {
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Failed setting up hash mode");
      goto FlowError;
    }
  }

  // Set the color
  if (typeMask == 0 && color.valid) {
    // No values are used for any filter. Then we need to add color to the assign
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";color=%u", color.color);
  }

  // Set the tag name
  snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";tag=%s]=", internals->tagName);

  // Add the filter created info to the NTPL buffer
  strncpy(&ntpl_buf[strlen(ntpl_buf)], filter_buf1, NTPL_BSIZE - strlen(ntpl_buf) - 1);

  if (ntpl_str) {
    if (filterContinue) {
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " and");
    }
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " %s", ntpl_str);
    filterContinue = true;
  }

  if (filterContinue) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " and");
  }
  if (nb_ports == 0) {
  // Set the ports
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " port==%u", internals->port);
    filterContinue = true;
  }
  else {
    int ports;
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " port==");
    for (ports = 0; ports < nb_ports;ports++) {
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "%u", list_ports[ports]);
      if (ports < (nb_ports - 1)) {
        snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ",");
      }
    }
    filterContinue = true;
  }

  if (CreateOptimizedFilter(ntpl_buf, internals, flow, &filterContinue, typeMask, list_queues, nb_queues, &reuse, &color) != 0) {
    rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Creating filter error");
    goto FlowError;
  }

  if (!reuse) {
    if (DoNtpl(ntpl_buf, &ntplID, internals) != 0) {
      rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Filter error");
      goto FlowError;
    }
    rte_spinlock_lock(&internals->lock);
    flow->assign_ntpl_id = ntplID;
    rte_spinlock_unlock(&internals->lock);
  }

  if (ntpl_buf) {
    rte_free(ntpl_buf);
  }
  if (filter_buf1) {
    rte_free(filter_buf1);
  }
  rte_spinlock_lock(&internals->lock);
  LIST_INSERT_HEAD(&internals->flows, flow, next);
  rte_spinlock_unlock(&internals->lock);
  return flow;

FlowError:
    if (flow) {
      rte_free(flow);
    }
    if (ntpl_buf) {
      rte_free(ntpl_buf);
    }
    if (filter_buf1) {
      rte_free(filter_buf1);
    }
    return NULL;
}

static int _dev_flow_destroy(struct rte_eth_dev *dev,
                             struct rte_flow *flow,
                             struct rte_flow_error *error __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;

  rte_spinlock_lock(&internals->lock);
  _cleanUpFlow(flow, internals);
  rte_spinlock_unlock(&internals->lock);
  return 0;
}

static int _dev_flow_flush(struct rte_eth_dev *dev,
                           struct rte_flow_error *error __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;

  rte_spinlock_lock(&internals->lock);
  while (!LIST_EMPTY(&internals->flows)) {
    struct rte_flow *flow;
    flow = LIST_FIRST(&internals->flows);
    _cleanUpFlow(flow, internals);
  }
  rte_spinlock_unlock(&internals->lock);
  return 0;
}

static int _hash_filter_ctrl(struct rte_eth_dev *dev,
                             enum rte_filter_op filter_op,
                             void *arg)
{
  struct pmd_internals *internals = dev->data->dev_private;
  struct rte_eth_hash_filter_info *info = (struct rte_eth_hash_filter_info *)arg;
  int ret = 0;

  switch (filter_op) {
  case RTE_ETH_FILTER_NOP:
    break;
  case RTE_ETH_FILTER_SET:
    if (info->info_type == RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT) {
      if (info->info.enable) {
        if (internals->symHashMode != SYM_HASH_ENA_PER_PORT) {
          internals->symHashMode = SYM_HASH_ENA_PER_PORT;
        }
      }
      else {
        if (internals->symHashMode != SYM_HASH_DIS_PER_PORT) {
          internals->symHashMode = SYM_HASH_DIS_PER_PORT;
        }
      }
    }
    else {
      PMD_NTACC_LOG(WARNING, ">>> Warning: Filter Hash - info_type (%d) not supported", info->info_type);
      ret = -ENOTSUP;
    }
    break;
  default:
    PMD_NTACC_LOG(WARNING, ">>> Warning:  Filter Hash - Filter operation (%d) not supported", filter_op);
    ret = -ENOTSUP;
    break;
  }
  return ret;
}

static int _dev_flow_isolate(struct rte_eth_dev *dev,
                             int set,
                             struct rte_flow_error *error __rte_unused)
{
  uint32_t ntplID;
  struct pmd_internals *internals = dev->data->dev_private;
  int status;
  int i;

  if (set == 1 && internals->defaultFlow) {
    char ntpl_buf[21];
    rte_spinlock_lock(&internals->lock);
    while (!LIST_EMPTY(&internals->defaultFlow->ntpl_id)) {
      struct filter_flow *id;
      id = LIST_FIRST(&internals->defaultFlow->ntpl_id);
      snprintf(ntpl_buf, 20, "delete=%d", id->ntpl_id);
      DoNtpl(ntpl_buf, NULL, internals);
      PMD_NTACC_LOG(DEBUG, "Deleting Item filter 0: %s\n", ntpl_buf);
      LIST_REMOVE(id, next);
      rte_free(id);
    }
    rte_spinlock_unlock(&internals->lock);

    // Call RxGet in order to activate the delete of the default filter
    for (i = 0; i < internals->defaultFlow->nb_queues; i++) {
      NtNetBuf_t pSeg;
      struct ntacc_rx_queue *rx_q;
      rx_q = &internals->rxq[internals->defaultFlow->list_queues[i]];
      PMD_NTACC_LOG(DEBUG, "Get dummy segment: Queue %u streamID %u\n", internals->defaultFlow->list_queues[i], rx_q->stream_id);
      status = (*_NT_NetRxGet)(rx_q->pNetRx, &pSeg, 0);
      if (status == NT_SUCCESS) {
        PMD_NTACC_LOG(DEBUG, "Discard dummy segment: Queue %u streamID %u\n", internals->defaultFlow->list_queues[i], rx_q->stream_id);
        // We got a segment of data. Discard it and release the segment again
        (*_NT_NetRxRelease)(rx_q->pNetRx, pSeg);
      }
    }
    rte_spinlock_lock(&internals->lock);
    rte_free(internals->defaultFlow);
    internals->defaultFlow = NULL;
    rte_spinlock_unlock(&internals->lock);
  }
  else if (set == 0 && !internals->defaultFlow) {
    struct ntacc_rx_queue *rx_q = internals->rxq;
    uint queue;
    uint8_t nb_queues = 0;
    char *ntpl_buf = NULL;
    uint8_t list_queues[NUM_FLOW_QUEUES];

    // Build default flow
    for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS && queue < NUM_FLOW_QUEUES; queue++) {
      if (rx_q[queue].enabled) {
        list_queues[nb_queues++] = queue;
      }
    }

    ntpl_buf = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
    if (!ntpl_buf) {
      _log_out_of_memory_errors(__func__);
      goto IsolateError;
    }

    if (nb_queues > 0 && rx_q[0].enabled) {
      struct rte_flow *defFlow = rte_malloc(internals->name, sizeof(struct rte_flow), 0);
      if (!defFlow) {
        _log_out_of_memory_errors(__func__);
        goto IsolateError;
      }
      memset(defFlow, 0, sizeof(struct rte_flow));

      // Set the priority
#ifdef COPY_OFFSET0
      snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=62;Descriptor=DYN3,length=20,colorbits=14,Offset0=%s[0];", STRINGIZE_VALUE_OF(COPY_OFFSET0));
#else
      snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=62;Descriptor=DYN3,length=20,colorbits=14;");
#endif
      if (internals->rss_hf != 0) {
        // Set the stream IDs
        CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, nb_queues, list_queues);
        // If RSS is used, then set the Hash mode
        if (CreateHash(&ntpl_buf[strlen(ntpl_buf)], internals->rss_hf, internals, false) != 0) {
          PMD_NTACC_LOG(ERR, "Failed to create hash function eth_dev_start\n");
          goto IsolateError;
        }
      }
      else {
        // Set the stream IDs
        CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, 1, list_queues);
      }

      // Set the port number
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
               ";tag=%s]=port==%u", internals->tagName, internals->port);

      if (DoNtpl(ntpl_buf, &ntplID, internals) != 0) {
        PMD_NTACC_LOG(ERR, "Failed to create default filter in flow_isolate\n");
        goto IsolateError;
      }

      // Store the used queues for the default flow
      for (i = 0; i < nb_queues && i < NUM_FLOW_QUEUES; i++) {
        defFlow->list_queues[i] = list_queues[i];
        defFlow->nb_queues++;
      }

      rte_spinlock_lock(&internals->lock);
      defFlow->priority = 62;
      pushNtplID(defFlow, ntplID);
      internals->defaultFlow = defFlow;
      rte_spinlock_unlock(&internals->lock);
    }

    IsolateError:

    if (ntpl_buf) {
      rte_free(ntpl_buf);
      ntpl_buf = NULL;
    }
  }
  return 0;
}

static const struct rte_flow_ops _dev_flow_ops = {
  .validate = NULL,
  .create = _dev_flow_create,
  .destroy = _dev_flow_destroy,
  .flush = _dev_flow_flush,
  .query = NULL,
  .isolate = _dev_flow_isolate,
};

static int _dev_filter_ctrl(struct rte_eth_dev *dev __rte_unused,
                            enum rte_filter_type filter_type,
                            enum rte_filter_op filter_op,
                            void *arg)
{
  int ret = EINVAL;

  switch (filter_type) {
  case RTE_ETH_FILTER_HASH:
    ret = _hash_filter_ctrl(dev, filter_op, arg);
    return ret;
  case RTE_ETH_FILTER_GENERIC:
    switch (filter_op) {
    case RTE_ETH_FILTER_NOP:
      return 0;
    default:
      *(const void **)arg = &_dev_flow_ops;
      return 0;
    }
  default:
    PMD_NTACC_LOG(ERR, "NTACC: %s: filter type (%d) not supported\n", __func__, filter_type);
    break;
  }

  return -ret;
}

static int eth_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
  char buf[51];
  struct pmd_internals *internals = dev->data->dev_private;

  snprintf(buf, 50, "%d.%d.%d - %03d-%04d-%02d-%02d-%02d", internals->version.major,
                                                           internals->version.minor,
                                                           internals->version.patch,
                                                           internals->fpgaid.s.item,
                                                           internals->fpgaid.s.product,
                                                           internals->fpgaid.s.ver,
                                                           internals->fpgaid.s.rev,
                                                           internals->fpgaid.s.build);
  size_t size = strlen(buf);
  strncpy(fw_version, buf, MIN(size+1, fw_size));
  if (fw_size > size) {
    // We have space for the version string
    return 0;
  }
  else {
    // We do not have space for the version string
    // return the needed space
    return size;
  }
}

static int eth_rss_hash_update(struct rte_eth_dev *dev,
                               struct rte_eth_rss_conf *rss_conf)
{
  int ret = 0;
  struct pmd_internals *internals = dev->data->dev_private;

  if (rss_conf->rss_hf == 0) {
    // Flush all hash settings
    rte_spinlock_lock(&internals->lock);
    FlushHash(internals);
    rte_spinlock_unlock(&internals->lock);
  }
  else {
    struct rte_flow dummyFlow;
    memset(&dummyFlow, 0, sizeof(struct rte_flow));

    // Flush all other hash settings first
    rte_spinlock_lock(&internals->lock);
    FlushHash(internals);
    rte_spinlock_unlock(&internals->lock);
    if (CreateHashModeHash(rss_conf->rss_hf, internals, &dummyFlow, 61) != 0) {
      PMD_NTACC_LOG(ERR, "Failed to create hash function eth_rss_hash_update\n");
      ret = 1;
      goto UpdateError;
    }
  }
UpdateError:
  return ret;
}

static struct eth_dev_ops ops = {
    .dev_start = eth_dev_start,
    .dev_stop = eth_dev_stop,
    .dev_close = eth_dev_close,
    .mtu_set = _dev_set_mtu,
    .dev_configure = eth_dev_configure,
    .dev_infos_get = eth_dev_info,
    .rx_queue_setup = eth_rx_queue_setup,
    .tx_queue_setup = eth_tx_queue_setup,
    .rx_queue_release = eth_queue_release,
    .tx_queue_release = eth_queue_release,
    .rx_queue_start = eth_rx_queue_start,
    .rx_queue_stop = eth_rx_queue_stop,
    .link_update = eth_link_update,
    .stats_get = eth_stats_get,
    .stats_reset = eth_stats_reset,
    .flow_ctrl_get = _dev_get_flow_ctrl,
    .flow_ctrl_set = _dev_set_flow_ctrl,
    .filter_ctrl = _dev_filter_ctrl,
    .fw_version_get = eth_fw_version_get,
    .rss_hash_update = eth_rss_hash_update,
};

static int rte_pmd_init_internals(struct rte_pci_device *dev,
                                  const uint32_t mask,
                                  const char     *ntpl_file)
{
  int iRet = 0;
  NtInfoStream_t hInfo = NULL;
  struct pmd_internals *internals = NULL;
  struct rte_eth_dev *eth_dev = NULL;
  uint i, status;
  NtInfo_t *pInfo = NULL;
  struct rte_eth_link pmd_link;
  char name[NTACC_NAME_LEN];
  uint8_t nbPortsOnAdapter = 0;
  uint8_t nbPortsInSystem = 0;
  uint8_t nbAdapters = 0;
  uint8_t adapterNo = 0;
  uint8_t offset = 0;
  uint8_t localPort = 0;
  struct version_s version;

  pInfo = (NtInfo_t *)rte_malloc(internals->name, sizeof(NtInfo_t), 0);
  if (!pInfo) {
    iRet = _log_out_of_memory_errors(__func__);
    goto error;
  }

  /* Open the information stream */
  if ((status = (*_NT_InfoOpen)(&hInfo, "DPDK Info stream")) != NT_SUCCESS) {
    _log_nt_errors(status, "NT_InfoOpen failed", __func__);
    iRet = status;
    goto error;
  }

  /* Find driver version */
  pInfo->cmd = NT_INFO_CMD_READ_SYSTEM;
  if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
    _log_nt_errors(status, "NT_InfoRead failed", __func__);
    iRet = status;
    goto error;
  }

  nbAdapters = pInfo->u.system.data.numAdapters;
  nbPortsInSystem = pInfo->u.system.data.numPorts;
  version.major = pInfo->u.system.data.version.major;
  version.minor = pInfo->u.system.data.version.minor;
  version.patch = pInfo->u.system.data.version.patch;

  // Check that the driver is supported
  if (((supportedDriver.major * 100) + supportedDriver.minor)  >
      ((version.major * 100) + version.minor)) {
    PMD_NTACC_LOG(ERR, "ERROR: NT Driver version %d.%d.%d is not supported. The version must be %d.%d.%d or newer.\n",
            version.major, version.minor, version.patch,
            supportedDriver.major, supportedDriver.minor, supportedDriver.patch);
    iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
    goto error;
  }

  for (i = 0; i < nbAdapters; i++) {
    // Find adapter matching bus ID
    pInfo->cmd = NT_INFO_CMD_READ_ADAPTER_V6;
    pInfo->u.adapter_v6.adapterNo = i;
    if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
      _log_nt_errors(status, "NT_InfoRead failed", __func__);
      iRet = status;
      goto error;
    }

    PMD_NTACC_LOG(INFO, "Checking: "PCI_PRI_FMT"\n", dev->addr.domain, dev->addr.bus, dev->addr.devid, dev->addr.function);
    if (dev->addr.bus == pInfo->u.adapter_v6.data.busid.s.bus &&
        dev->addr.devid == pInfo->u.adapter_v6.data.busid.s.device &&
        dev->addr.domain == pInfo->u.adapter_v6.data.busid.s.domain &&
        dev->addr.function == pInfo->u.adapter_v6.data.busid.s.function) {
      nbPortsOnAdapter = pInfo->u.adapter_v6.data.numPorts;
      offset = pInfo->u.adapter_v6.data.portOffset;
      adapterNo = i;
      break;
    }
  }

  if (i == nbAdapters) {
    // No adapters found
    PMD_NTACC_LOG(INFO, "Adapter not found\n");
    return 1;
  }
  PMD_NTACC_LOG(INFO, "Found: "PCI_PRI_FMT": Ports %u, Offset %u, Adapter %u\n", dev->addr.domain, dev->addr.bus, dev->addr.devid, dev->addr.function, nbPortsOnAdapter, offset, adapterNo);

  for (localPort = 0; localPort < nbPortsOnAdapter; localPort++) {
    pInfo->cmd = NT_INFO_CMD_READ_PORT_V7;
    pInfo->u.port_v7.portNo = (uint8_t)localPort + offset;
    if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
      _log_nt_errors(status, "NT_InfoRead failed", __func__);
      iRet = status;
      goto error;
    }

    if (!((1<<localPort)&mask)) {
      continue;
    }

    snprintf(name, NTACC_NAME_LEN, PCI_PRI_FMT " Port %u", dev->addr.domain, dev->addr.bus, dev->addr.devid, dev->addr.function, localPort);
    PMD_NTACC_LOG(INFO, "Port: %u - %s\n", offset + localPort, name);

    // Check if FPGA is supported
    for (i = 0; i < NB_SUPPORTED_FPGAS; i++) {
      if (supportedAdapters[i].item == pInfo->u.port_v7.data.adapterInfo.fpgaid.s.item &&
          supportedAdapters[i].product == pInfo->u.port_v7.data.adapterInfo.fpgaid.s.product) {
        if (((supportedAdapters[i].ver * 100) + supportedAdapters[i].rev) >
            ((pInfo->u.port_v7.data.adapterInfo.fpgaid.s.ver * 100) + pInfo->u.port_v7.data.adapterInfo.fpgaid.s.rev)) {
          PMD_NTACC_LOG(ERR, "ERROR: NT adapter firmware %03d-%04d-%02d-%02d-%02d is not supported. The firmware must be %03d-%04d-%02d-%02d-%02d or newer.\n",
                  pInfo->u.port_v7.data.adapterInfo.fpgaid.s.item,
                  pInfo->u.port_v7.data.adapterInfo.fpgaid.s.product,
                  pInfo->u.port_v7.data.adapterInfo.fpgaid.s.ver,
                  pInfo->u.port_v7.data.adapterInfo.fpgaid.s.rev,
                  pInfo->u.port_v7.data.adapterInfo.fpgaid.s.build,
                  supportedAdapters[i].item,
                  supportedAdapters[i].product,
                  supportedAdapters[i].ver,
                  supportedAdapters[i].rev,
                  supportedAdapters[i].build);
          iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
          goto error;
        }
        break;
      }
    }

    if (i == NB_SUPPORTED_FPGAS) {
      // No matching adapter is found
      PMD_NTACC_LOG(ERR, ">>> ERROR: Not supported NT adapter is found. Following adapters are supported:\n");
      for (i = 0; i < NB_SUPPORTED_FPGAS; i++) {
        PMD_NTACC_LOG(ERR, "           %03d-%04d-%02d-%02d-%02d\n",
                supportedAdapters[i].item,
                supportedAdapters[i].product,
                supportedAdapters[i].ver,
                supportedAdapters[i].rev,
                supportedAdapters[i].build);
      }
      iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
      goto error;
    }
    if (RTE_ETHDEV_QUEUE_STAT_CNTRS > (256 / nbPortsInSystem)) {
      PMD_NTACC_LOG(ERR, ">>> Error: This adapter can only support %u queues\n", STREAMIDS_PER_PORT);
      PMD_NTACC_LOG(ERR, "           Set RTE_ETHDEV_QUEUE_STAT_CNTRS to %u or less\n", STREAMIDS_PER_PORT);
      iRet = NT_ERROR_STREAMID_OUT_OF_RANGE;
      goto error;
    }

    /* reserve an ethdev entry */
    eth_dev = rte_eth_dev_allocate(name);
    if (eth_dev == NULL) {
      PMD_NTACC_LOG(ERR, "ERROR: Failed to allocate ethernet device\n");
      iRet = -ENOMEM;
      goto error;
    }

    internals = rte_zmalloc_socket(name, sizeof(struct pmd_internals), RTE_CACHE_LINE_SIZE, dev->device.numa_node);
    if (internals == NULL) {
      iRet = _log_out_of_memory_errors(__func__);
      goto error;
    }

    if (strlen(ntpl_file) > 0) {
      internals->ntpl_file  = rte_zmalloc(name, strlen(ntpl_file) + 1, 0);
      if (internals->ntpl_file == NULL) {
        iRet = _log_out_of_memory_errors(__func__);
        goto error;
      }
      strcpy(internals->ntpl_file, ntpl_file);
    }
    deviceCount++;

    internals->version.major = version.major;
    internals->version.minor = version.minor;
    internals->version.patch = version.patch;
    internals->nbPortsOnAdapter = nbPortsOnAdapter;
    internals->nbPortsInSystem = nbPortsInSystem;
    strcpy(internals->name, name);
    strcpy(internals->driverName, "net_ntacc");

    snprintf(internals->tagName, 9, "port%d", localPort + offset);
    PMD_NTACC_LOG(INFO, "Tagname: %s - %u\n", internals->tagName, localPort + offset);

    internals->adapterNo = pInfo->u.port_v7.data.adapterNo;
    internals->port = offset + localPort;
    internals->local_port = localPort;
    internals->local_port_offset = offset;
    internals->symHashMode = SYM_HASH_ENA_PER_PORT;
    internals->fpgaid.value = pInfo->u.port_v7.data.adapterInfo.fpgaid.value;

    for (i=0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
      internals->rxq[i].stream_id = STREAMIDS_PER_PORT * internals->port + i;
      internals->rxq[i].pSeg = NULL;
      internals->rxq[i].enabled = 0;
    }

    for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
      internals->txq[i].port = internals->port;
      internals->txq[i].local_port = localPort;
      internals->txq[i].enabled = 0;
      internals->txq[i].minTxPktSize = pInfo->u.port_v7.data.capabilities.minTxPktSize;
      internals->txq[i].maxTxPktSize = pInfo->u.port_v7.data.capabilities.maxTxPktSize;
    }

    switch (pInfo->u.port_v7.data.speed) {
    case NT_LINK_SPEED_UNKNOWN:
      pmd_link.link_speed = ETH_SPEED_NUM_1G;
      break;
    case NT_LINK_SPEED_10M:
      pmd_link.link_speed = ETH_SPEED_NUM_10M;
      break;
    case NT_LINK_SPEED_100M:
      pmd_link.link_speed = ETH_SPEED_NUM_100M;
      break;
    case NT_LINK_SPEED_1G:
      pmd_link.link_speed = ETH_SPEED_NUM_1G;
      break;
    case NT_LINK_SPEED_10G:
      pmd_link.link_speed = ETH_SPEED_NUM_10G;
      break;
    case NT_LINK_SPEED_40G:
      pmd_link.link_speed = ETH_SPEED_NUM_40G;
      break;
    case NT_LINK_SPEED_50G:
      pmd_link.link_speed = ETH_SPEED_NUM_50G;
      break;
    case NT_LINK_SPEED_100G:
      pmd_link.link_speed = ETH_SPEED_NUM_100G;
      break;
    }

    memcpy(&eth_addr[internals->port].addr_bytes, &pInfo->u.port_v7.data.macAddress, sizeof(eth_addr[internals->port].addr_bytes));

    pmd_link.link_duplex = ETH_LINK_FULL_DUPLEX;
    pmd_link.link_status = 0;

    internals->if_index = internals->port;

    eth_dev->device = &dev->device;
    eth_dev->data->dev_private = internals;
    eth_dev->data->port_id = eth_dev->data->port_id;
    eth_dev->data->dev_link = pmd_link;
    eth_dev->data->mac_addrs = &eth_addr[internals->port];
    eth_dev->data->numa_node = dev->device.numa_node;

    eth_dev->dev_ops = &ops;
    eth_dev->rx_pkt_burst = eth_ntacc_rx;
    eth_dev->tx_pkt_burst = eth_ntacc_tx;

  #ifndef USE_SW_STAT
    /* Open the stat stream */
    if ((status = (*_NT_StatOpen)(&internals->hStat, "DPDK Stat stream")) != NT_SUCCESS) {
      _log_nt_errors(status, "NT_StatOpen failed", __func__);
      iRet = status;
      goto error;
    }
		rte_spinlock_init(&internals->statlock);
  #endif
  rte_spinlock_init(&internals->lock);
  }
  (void)(*_NT_InfoClose)(hInfo);

  rte_free(pInfo);
  return iRet;

error:
  if (pInfo) {
    rte_free(pInfo);
  }
  if (hInfo)
    (void)(*_NT_InfoClose)(hInfo);
  if (internals)
    rte_free(internals);
  return iRet;
}

/*
 * convert ascii to int
 */
static inline int ascii_to_u32(const char *key __rte_unused, const char *value, void *extra_args)
{
  *(uint32_t*)extra_args = atoi(value);
  return 0;
}

static inline int ascii_to_ascii(const char *key __rte_unused, const char *value, void *extra_args)
{
  strncpy((char *)extra_args, value, MAX_NTPL_NAME);
  return 0;
}

static int _nt_lib_open(void)
{
  char path[128];
  strcpy(path, NAPATECH3_LIB_PATH);
  strcat(path, "/libntapi.so");

  /* Load the library */
  _libnt = dlopen(path, RTLD_NOW);
  if (_libnt == NULL) {
    /* Library does not exist. */
    fprintf(stderr, "Failed to find needed library : %s\n", path);
    return -1;
  }
  _NT_Init = dlsym(_libnt, "NT_Init");
  if (_NT_Init == NULL) {
    fprintf(stderr, "Failed to find \"NT_Init\" in %s\n", path);
    return -1;
  }

  _NT_ConfigOpen = dlsym(_libnt, "NT_ConfigOpen");
  if (_NT_ConfigOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigOpen\" in %s\n", path);
    return -1;
  }
  _NT_ConfigClose = dlsym(_libnt, "NT_ConfigClose");
  if (_NT_ConfigClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigClose\" in %s\n", path);
    return -1;
  }
  _NT_NTPL = dlsym(_libnt, "NT_NTPL");
  if (_NT_NTPL == NULL) {
    fprintf(stderr, "Failed to find \"NT_NTPL\" in %s\n", path);
    return -1;
  }

  _NT_InfoOpen = dlsym(_libnt, "NT_InfoOpen");
  if (_NT_InfoOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoOpen\" in %s\n", path);
    return -1;
  }
  _NT_InfoRead = dlsym(_libnt, "NT_InfoRead");
  if (_NT_InfoRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoRead\" in %s\n", path);
    return -1;
  }
  _NT_InfoClose = dlsym(_libnt, "NT_InfoClose");
  if (_NT_InfoClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoClose\" in %s\n", path);
    return -1;
  }
  _NT_StatClose = dlsym(_libnt, "NT_StatClose");
  if (_NT_StatClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatClose\" in %s\n", path);
    return -1;
  }
  _NT_StatOpen = dlsym(_libnt, "NT_StatOpen");
  if (_NT_StatOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatOpen\" in %s\n", path);
    return -1;
  }
  _NT_StatRead = dlsym(_libnt, "NT_StatRead");
  if (_NT_StatRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatRead\" in %s\n", path);
    return -1;
  }
  _NT_ExplainError = dlsym(_libnt, "NT_ExplainError");
  if (_NT_ExplainError == NULL) {
    fprintf(stderr, "Failed to find \"NT_ExplainError\" in %s\n", path);
    return -1;
  }
  _NT_NetTxOpen = dlsym(_libnt, "NT_NetTxOpen");
  if (_NT_NetTxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetTxClose = dlsym(_libnt, "NT_NetTxClose");
  if (_NT_NetTxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpen = dlsym(_libnt, "NT_NetRxOpen");
  if (_NT_NetRxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetRxGet = dlsym(_libnt, "NT_NetRxGet");
  if (_NT_NetRxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGet\" in %s\n", path);
    return -1;
  }
  _NT_NetRxRelease = dlsym(_libnt, "NT_NetRxRelease");
  if (_NT_NetRxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxRelease\" in %s\n", path);
    return -1;
  }
  _NT_NetRxClose = dlsym(_libnt, "NT_NetRxClose");
  if (_NT_NetRxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxGetNextPacket = dlsym(_libnt, "NT_NetRxGetNextPacket");
  if (_NT_NetRxGetNextPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGetNextPacket\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpenMulti = dlsym(_libnt, "NT_NetRxOpenMulti");
  if (_NT_NetRxOpenMulti == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpenMulti\" in %s\n", path);
    return -1;
  }

  _NT_NetTxRelease = dlsym(_libnt, "NT_NetTxRelease");
  if (_NT_NetTxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxRelease\" in %s\n", path);
    return -1;
  }

  _NT_NetTxAddPacket = dlsym(_libnt, "NT_NetTxAddPacket");
  if (_NT_NetTxAddPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxAddPacket\" in %s\n", path);
    return -1;
  }
  _NT_NetTxRead = dlsym(_libnt, "NT_NetTxRead");
  if (_NT_NetTxRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxRead\" in %s\n", path);
    return -1;
  }
  _NT_NetTxGet = dlsym(_libnt, "NT_NetTxGet");
  if (_NT_NetTxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxGet\" in %s\n", path);
    return -1;
  }

  return 0;
}

static int rte_pmd_ntacc_dev_probe(struct rte_pci_driver *drv __rte_unused, struct rte_pci_device *dev)
{
  int ret = 0;
  struct rte_kvargs *kvlist;
  unsigned int i;
  uint32_t mask=0xF;

  char ntplStr[MAX_NTPL_NAME] = { 0 };

  switch (dev->id.device_id) {
  case PCI_DEVICE_ID_NT20E3:
  case PCI_DEVICE_ID_NT40E3:
  case PCI_DEVICE_ID_NT40A01:
    break;
  case PCI_DEVICE_ID_NT200A01:
  case PCI_DEVICE_ID_NT80E3:
  case PCI_DEVICE_ID_NT100E3:
    if (dev->id.subsystem_device_id != 1) {
      // Subdevice of the adapter. Ignore it
      return 1;
    }
    break;
  }

  PMD_NTACC_LOG(DEBUG, "Initializing net_ntacc %s for %s on numa %d\n", rte_version(),
                                                                       dev->device.name,
                                                                       dev->device.numa_node);

  PMD_NTACC_LOG(DEBUG, "PCI ID :    0x%04X:0x%04X\n", dev->id.vendor_id, dev->id.device_id);
  PMD_NTACC_LOG(DEBUG, "PCI device: "PCI_PRI_FMT"\n", dev->addr.domain,
                                                     dev->addr.bus,
                                                     dev->addr.devid,
                                                     dev->addr.function);

#ifdef USE_SW_STAT
  if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
    PMD_NTACC_LOG(ERR, "pmd_ntacc %s must run as a primary process, when using SW statistics\n", dev->device.name);
    return -1;
  }
#endif

  if (dev->device.devargs && dev->device.devargs->args) {
    kvlist = rte_kvargs_parse(dev->device.devargs->args, valid_arguments);
    if (kvlist == NULL) {
      return -1;
    }

    // Get port to use for Rx/Tx
    if ((i = rte_kvargs_count(kvlist, ETH_NTACC_MASK_ARG))) {
      assert (i == 1);
      ret = rte_kvargs_process(kvlist, ETH_NTACC_MASK_ARG, &ascii_to_u32, &mask);
    }

    // Get filename to store ntpl
    if ((i = rte_kvargs_count(kvlist, ETH_NTACC_NTPL_ARG))) {
      assert (i == 1);
      ret = rte_kvargs_process(kvlist, ETH_NTACC_NTPL_ARG, &ascii_to_ascii, ntplStr);
    }

    rte_kvargs_free(kvlist);
  }

  if (ret < 0)
    return -1;

  if (first == 0) {
    ret = _nt_lib_open();
    if (ret < 0)
      return -1;

    (*_NT_Init)(NTAPI_VERSION);
    first++;
  }


  if (rte_pmd_init_internals(dev, mask, ntplStr) < 0)
    return -1;

  return 0;
}

static const struct rte_pci_id ntacc_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT200A01)
	},
  {
    RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT80E3)
  },
  {
    RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT20E3)
  },
  {
    RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT40E3)
  },
  {
    RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT40A01)
  },
  {
    RTE_PCI_DEVICE(PCI_VENDOR_ID_NAPATECH,PCI_DEVICE_ID_NT100E3)
  },
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver ntacc_driver = {
	.driver = {
		.name = "net_ntacc",
	},
	.id_table = ntacc_pci_id_map,
  .probe = rte_pmd_ntacc_dev_probe,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_ntacc_pmd_init);
static void rte_ntacc_pmd_init(void)
{
	rte_pci_register(&ntacc_driver);
	ntacc_logtype = rte_log_register("pmd.net.ntacc");
	if (ntacc_logtype >= 0)
		rte_log_set_level(ntacc_logtype, RTE_LOG_NOTICE);
}

RTE_PMD_EXPORT_NAME(net_ntacc, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_ntacc, ntacc_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ntacc, "* nt3gd");

