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

#ifndef __FILTER_NTACC_H__
#define __FILTER_NTACC_H__

#define NTPL_BSIZE 1024

enum {
  GTPC2_TUNNEL_TYPE,
  GTPC1_TUNNEL_TYPE,
  GTPC1_2_TUNNEL_TYPE,
  GTPU0_TUNNEL_TYPE,
  GTPU1_TUNNEL_TYPE,
  GREV0_TUNNEL_TYPE,
  GREV1_TUNNEL_TYPE,
  VXLAN_TUNNEL_TYPE,
  NVGRE_TUNNEL_TYPE,
  IP_IN_IP_TUNNEL_TYPE,
};

struct color_s {
  uint32_t color;
  uint32_t colorMask;
  enum {
    NO_COLOR,
    ONE_COLOR,
    COLOR_MASK
  } type;
};

enum {
  RX_FILTER            = (1ULL << 0),
  DROP_FILTER          = (1ULL << 1),
  RETRANSMIT_FILTER    = (1ULL << 2),
  DUMMY_FILTER         = (1ULL << 3),
  // Protokoller
  ETHER_ADDR_DST       = (1ULL << 4),
  ETHER_ADDR_SRC       = (1ULL << 5),
  ETHER_TYPE           = (1ULL << 6),
  IPV4_VERSION_IHL     = (1ULL << 7),
  IPV4_TYPE_OF_SERVICE = (1ULL << 9),
  IPV4_TOTAL_LENGTH    = (1ULL << 10),
  IPV4_PACKET_ID       = (1ULL << 11),
  IPV4_FRAGMENT_OFFSET = (1ULL << 12),
  IPV4_TIME_TO_LIVE    = (1ULL << 13),
  IPV4_NEXT_PROTO_ID   = (1ULL << 14),
  IPV4_HDR_CHECKSUM    = (1ULL << 15),
  IPV4_SRC_ADDR        = (1ULL << 16),
  IPV4_DST_ADDR        = (1ULL << 17),
  IPV6_VTC_FLOW        = (1ULL << 18),
  IPV6_PAYLOAD_LEN     = (1ULL << 19),
  IPV6_PROTO           = (1ULL << 20),
  IPV6_HOP_LIMITS      = (1ULL << 21),
  IPV6_SRC_ADDR        = (1ULL << 22),
  IPV6_DST_ADDR        = (1ULL << 23),
  TCP_SRC_PORT         = (1ULL << 24),
  TCP_DST_PORT         = (1ULL << 25),
  TCP_SENT_SEQ         = (1ULL << 26),
  TCP_RECV_ACK         = (1ULL << 27),
  TCP_DATA_OFF         = (1ULL << 28),
  TCP_FLAGS            = (1ULL << 29),
  TCP_RX_WIN           = (1ULL << 30),
  TCP_CKSUM            = (1ULL << 31),
  TCP_URP              = (1ULL << 32),
  UDP_SRC_PORT         = (1ULL << 33),
  UDP_DST_PORT         = (1ULL << 34),
  UDP_DGRAM_LEN        = (1ULL << 35),
  UDP_DGRAM_CKSUM      = (1ULL << 36),
  SCTP_SRC_PORT        = (1ULL << 37),
  SCTP_DST_PORT        = (1ULL << 38),
  SCTP_TAG             = (1ULL << 39),
  SCTP_CKSUM           = (1ULL << 40),
  ICMP_TYPE            = (1ULL << 41),
  ICMP_CODE            = (1ULL << 42),
  ICMP_CKSUM           = (1ULL << 43),
  ICMP_IDENT           = (1ULL << 44),
  ICMP_SEQ_NB          = (1ULL << 45),
  VLAN_TCI             = (1ULL << 46),
  GTPU0_TUNNEL         = (1ULL << 47),
  GTPU1_TUNNEL         = (1ULL << 48),
  GREV0_TUNNEL         = (1ULL << 49),
  VXLAN_TUNNEL         = (1ULL << 50),
  NVGRE_TUNNEL         = (1ULL << 52),
  IP_IN_IP_TUNNEL      = (1ULL << 52),
  GTPC2_TUNNEL         = (1ULL << 53),
  GTPC1_TUNNEL         = (1ULL << 54),
  GTPC1_2_TUNNEL       = (1ULL << 55),
  GREV1_TUNNEL         = (1ULL << 56),
  MPLS_LABEL           = (1ULL << 57),
};

/******************* Function Prototypes ********************/

void CreateHash(char *ntpl_buf, const struct rte_flow_action_rss *rss, struct pmd_internals *internals);
int CreateHashModeHash(uint64_t rss_hf, struct pmd_internals *internals, struct rte_flow *flow, int priority);
void CreateStreamid(char *ntpl_buf, struct pmd_internals *internals, uint32_t nb_queues, uint8_t *list_queues);
int ReturnKeysetValue(struct pmd_internals *internals, int value);
void pushNtplID(struct rte_flow *flow, uint32_t ntplId);

int SetEthernetFilter(const struct rte_flow_item *item,
                      bool tunnel,
                      uint64_t *typeMask,
                      struct pmd_internals *internals,
                      struct rte_flow_error *error);
int SetIPV4Filter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnel, uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetIPV6Filter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnel,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetUDPFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 bool tunnnel,
                 uint64_t *typeMask,
                 struct pmd_internals *internals,
                 struct rte_flow_error *error);
int SetSCTPFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnnel,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetTCPFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 bool tunnnel,
                 uint64_t *typeMask,
                 struct pmd_internals *internals,
                 struct rte_flow_error *error);
int SetICMPFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetVlanFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetMplsFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetGreFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 uint64_t *typeMask);
int SetGtpFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 uint64_t *typeMask,
                 int protocol);
int SetTunnelFilter(char *ntpl_buf,
                    bool *fc,
                    int type,
                    uint64_t *typeMask);

int CreateOptimizedFilter(char *ntpl_buf,
                          struct pmd_internals *internals,
                          struct rte_flow *flow,
                          bool *fc,
                          uint64_t typeMask,
                          uint8_t *plist_queues,
                          uint8_t nb_queues,
                          int key,
                          struct color_s *pColor,
                          struct rte_flow_error *error);

void DeleteKeyset(int key, struct pmd_internals *internals, struct rte_flow_error *error);
//void DeleteHash(uint64_t rss_hf, uint8_t port, int priority, struct pmd_internals *internals);
void FlushHash(struct pmd_internals *internals);
bool IsFilterReuse(struct pmd_internals *internals, uint64_t typeMask, uint8_t *plist_queues, uint8_t nb_queues, int *key);
int GetKeysetValue(struct pmd_internals *internals);

#endif

