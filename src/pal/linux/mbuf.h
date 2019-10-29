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
#include <assert.h>

typedef struct rte_mbuf  rte_mbuf_t;

#define MAGIC0 0xAABBCCDD
#define MAGIC2 0x11223344

#define IND_ATTACHED_MBUF    (1ULL << 62) /**< Indirect attached mbuf */
#define PKT_TX_VLAN_PKT      (1ULL << 57) /**< TX packet is a 802.1q VLAN packet. */
#define RTE_MBUF_CLONED(mb)   ((mb)->ol_flags & IND_ATTACHED_MBUF)
#define RTE_MBUF_TO_BADDR(mb)       (((struct rte_mbuf *)(mb)) + 1)
#define RTE_MBUF_FROM_BADDR(ba)     (((struct rte_mbuf *)(ba)) - 1)


struct rte_mempool {
    uint32_t  magic;
    uint32_t  elt_size;
    uint32_t  priv_size;
    uint32_t  magic2;
    uint32_t  _id;
    int size;
};

#ifdef TREX_MBUF_SIM_LOCAL
  #define TREX_MBUF_SIM_LOCAL
#else
  #undef TREX_MBUF_SIM_LOCAL
#endif


#ifdef TREX_MBUF_SIM_LOCAL

typedef enum {
    RTE_MBUF_CORE_LOCALITY_MULTI = 0,
    RTE_MBUF_CORE_LOCALITY_LOCAL = 1,
    RTE_MBUF_CORE_LOCALITY_CONST = 2,
} mbuf_type_e;

#endif


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
    uint16_t refcnt_reserved;
    uint64_t ol_flags;        /**< Offload features. */
    uint16_t tso_segsz;
    uint16_t l2_len;
    uint16_t l3_len;
    uint16_t l4_len;
    uint16_t vlan_tci;
    uint8_t  m_core_locality;
    union {
        uint32_t rss;     /**< RSS hash result if RSS enabled */
    } hash;                   /**< hash information */

} ;

#ifdef TREX_MBUF_SIM_LOCAL

static inline void 
rte_mbuf_set_as_core_local(struct rte_mbuf *m) {
    m->m_core_locality = RTE_MBUF_CORE_LOCALITY_LOCAL;
}

static inline void
rte_mbuf_set_as_core_const(struct rte_mbuf *m) {
    m->m_core_locality = RTE_MBUF_CORE_LOCALITY_CONST;
}

static inline void
rte_mbuf_set_as_core_multi(struct rte_mbuf *m) {
    m->m_core_locality = RTE_MBUF_CORE_LOCALITY_MULTI;
}

#else
static inline void 
rte_mbuf_set_as_core_local(struct rte_mbuf *m) {
    
}

static inline void
rte_mbuf_set_as_core_const(struct rte_mbuf *m) {
    
}

static inline void
rte_mbuf_set_as_core_multi(struct rte_mbuf *m) {
}

#endif

typedef struct rte_mempool rte_mempool_t;

#define RTE_PKTMBUF_HEADROOM  0

void utl_rte_mempool_delete(rte_mempool_t  * &pool);

inline unsigned rte_mempool_count(rte_mempool_t  *mp){
    return (10);
}

uint16_t rte_ipv4_phdr_cksum(const struct ipv4_hdr *ipv4_hdr, uint64_t ol_flags);
uint16_t rte_ipv6_phdr_cksum(const struct ipv6_hdr *ipv6_hdr, uint64_t ol_flags);


#define PKT_TX_L4_NO_CKSUM   (0ULL << 52) /**< Disable L4 cksum of TX pkt. */
#define PKT_TX_TCP_CKSUM     (1ULL << 52) /**< TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    (2ULL << 52) /**< SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     (3ULL << 52) /**< UDP cksum of TX pkt. computed by NIC. */
#define PKT_TX_L4_MASK       (3ULL << 52) /**< Mask for L4 cksum offload request. */

/**
 * Offload the IP checksum in the hardware. The flag PKT_TX_IPV4 should
 * also be set by the application, although a PMD will only check
 * PKT_TX_IP_CKSUM.
 *  - set the IP checksum field in the packet to 0
 *  - fill the mbuf offload information: l2_len, l3_len
 */
#define PKT_TX_IP_CKSUM      (1ULL << 54)

/**
 * Packet is IPv4. This flag must be set when using any offload feature
 * (TSO, L3 or L4 checksum) to tell the NIC that the packet is an IPv4
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV4          (1ULL << 55)

/**
 * Packet is IPv6. This flag must be set when using an offload feature
 * (TSO or L4 checksum) to tell the NIC that the packet is an IPv6
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV6          (1ULL << 56)


#define PKT_RX_IP_CKSUM_MASK ((1ULL << 4) | (1ULL << 7))
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)

#define PKT_RX_IP_CKSUM_BAD  (1ULL << 4)
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)


#define PKT_TX_TCP_SEG       (1ULL << 50)



void rte_pktmbuf_free(rte_mbuf_t *m);

rte_mbuf_t *rte_pktmbuf_alloc(rte_mempool_t *mp);

char *rte_pktmbuf_append(rte_mbuf_t *m, uint16_t len);

char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len);

int rte_pktmbuf_trim(rte_mbuf_t *m, uint16_t len);
int rte_pktmbuf_is_contiguous(const struct rte_mbuf *m);
void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md);




void rte_pktmbuf_free_seg(rte_mbuf_t *m);

uint16_t rte_mbuf_refcnt_update(rte_mbuf_t *m, int16_t value);

void rte_pktmbuf_dump(const struct rte_mbuf *m, unsigned dump_len);


int _rte_mempool_sc_get(struct rte_mempool *mp, void **obj_p);

void _rte_mempool_sp_put(struct rte_mempool *mp, void *obj);

inline int rte_mempool_get(struct rte_mempool *mp, void **obj_p){
    return (_rte_mempool_sc_get(mp, obj_p));
}

inline void rte_mempool_put(struct rte_mempool *mp, void *obj){
    _rte_mempool_sp_put(mp, obj);
}


static inline void *
rte_memcpy(void *dst, const void *src, size_t n)
{
    return (memcpy(dst, src, n));
}


static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return m->refcnt_reserved;
}


extern "C" void rte_exit(int exit_code, const char *format, ...);

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



/**
 * Get the headroom in a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the headroom.
 */
static inline uint16_t rte_pktmbuf_headroom(const struct rte_mbuf *m)
{
	return m->data_off;
}

/**
 * Get the tailroom of a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the tailroom.
 */
static inline uint16_t rte_pktmbuf_tailroom(const struct rte_mbuf *m)
{
	return (uint16_t)(m->buf_len - rte_pktmbuf_headroom(m) -
			  m->data_len);
}




uint64_t rte_rand(void);

static inline void rte_pktmbuf_refcnt_update(struct rte_mbuf *m, int16_t v)
{
	do {
		rte_mbuf_refcnt_update(m, v);
	} while ((m = m->next) != NULL);
}

static inline void utl_rte_check(rte_mempool_t * mp){
    assert(mp->magic  == MAGIC0);
    assert(mp->magic2 == MAGIC2);
}

static inline void utl_rte_pktmbuf_check(struct rte_mbuf *m){
    utl_rte_check(m->pool);
    assert(m->magic == MAGIC0);
    assert(m->magic2== MAGIC2);
}



#define __rte_cache_aligned 

#ifdef __PPC64__
#define CACHE_LINE_SIZE 128
#define RTE_CACHE_LINE_SIZE 128
#else
#define CACHE_LINE_SIZE 64
#define RTE_CACHE_LINE_SIZE 64
#endif

#define SOCKET_ID_ANY 0

// has to be after the definition of rte_mbuf and other utility functions
#include "common_mbuf.h"

void hw_checksum_sim(struct rte_mbuf *m);

#endif
