/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_GENERIC_FLOW_H_
#define _ICE_GENERIC_FLOW_H_

#include <rte_flow_driver.h>

/* protocol */

#define ICE_PROT_MAC_INNER         (1ULL << 1)
#define ICE_PROT_MAC_OUTER         (1ULL << 2)
#define ICE_PROT_VLAN_INNER        (1ULL << 3)
#define ICE_PROT_VLAN_OUTER        (1ULL << 4)
#define ICE_PROT_IPV4_INNER        (1ULL << 5)
#define ICE_PROT_IPV4_OUTER        (1ULL << 6)
#define ICE_PROT_IPV6_INNER        (1ULL << 7)
#define ICE_PROT_IPV6_OUTER        (1ULL << 8)
#define ICE_PROT_TCP_INNER         (1ULL << 9)
#define ICE_PROT_TCP_OUTER         (1ULL << 10)
#define ICE_PROT_UDP_INNER         (1ULL << 11)
#define ICE_PROT_UDP_OUTER         (1ULL << 12)
#define ICE_PROT_SCTP_INNER        (1ULL << 13)
#define ICE_PROT_SCTP_OUTER        (1ULL << 14)
#define ICE_PROT_ICMP4_INNER       (1ULL << 15)
#define ICE_PROT_ICMP4_OUTER       (1ULL << 16)
#define ICE_PROT_ICMP6_INNER       (1ULL << 17)
#define ICE_PROT_ICMP6_OUTER       (1ULL << 18)
#define ICE_PROT_VXLAN             (1ULL << 19)
#define ICE_PROT_NVGRE             (1ULL << 20)
#define ICE_PROT_GTPU              (1ULL << 21)

/* field */

#define ICE_SMAC                   (1ULL << 63)
#define ICE_DMAC                   (1ULL << 62)
#define ICE_ETHERTYPE              (1ULL << 61)
#define ICE_IP_SRC                 (1ULL << 60)
#define ICE_IP_DST                 (1ULL << 59)
#define ICE_IP_PROTO               (1ULL << 58)
#define ICE_IP_TTL                 (1ULL << 57)
#define ICE_IP_TOS                 (1ULL << 56)
#define ICE_SPORT                  (1ULL << 55)
#define ICE_DPORT                  (1ULL << 54)
#define ICE_ICMP_TYPE              (1ULL << 53)
#define ICE_ICMP_CODE              (1ULL << 52)
#define ICE_VXLAN_VNI              (1ULL << 51)
#define ICE_NVGRE_TNI              (1ULL << 50)
#define ICE_GTPU_TEID              (1ULL << 49)
#define ICE_GTPU_QFI               (1ULL << 48)

/* input set */

#define ICE_INSET_NONE             0ULL

/* non-tunnel */

#define ICE_INSET_SMAC         (ICE_PROT_MAC_OUTER | ICE_SMAC)
#define ICE_INSET_DMAC         (ICE_PROT_MAC_OUTER | ICE_DMAC)
#define ICE_INSET_VLAN_INNER   (ICE_PROT_VLAN_INNER)
#define ICE_INSET_VLAN_OUTER   (ICE_PROT_VLAN_OUTER)
#define ICE_INSET_ETHERTYPE    (ICE_ETHERTYPE)

#define ICE_INSET_IPV4_SRC \
	(ICE_PROT_IPV4_OUTER | ICE_IP_SRC)
#define ICE_INSET_IPV4_DST \
	(ICE_PROT_IPV4_OUTER | ICE_IP_DST)
#define ICE_INSET_IPV4_TOS \
	(ICE_PROT_IPV4_OUTER | ICE_IP_TOS)
#define ICE_INSET_IPV4_PROTO \
	(ICE_PROT_IPV4_OUTER | ICE_IP_PROTO)
#define ICE_INSET_IPV4_TTL \
	(ICE_PROT_IPV4_OUTER | ICE_IP_TTL)
#define ICE_INSET_IPV6_SRC \
	(ICE_PROT_IPV6_OUTER | ICE_IP_SRC)
#define ICE_INSET_IPV6_DST \
	(ICE_PROT_IPV6_OUTER | ICE_IP_DST)
#define ICE_INSET_IPV6_NEXT_HDR \
	(ICE_PROT_IPV6_OUTER | ICE_IP_PROTO)
#define ICE_INSET_IPV6_HOP_LIMIT \
	(ICE_PROT_IPV6_OUTER | ICE_IP_TTL)
#define ICE_INSET_IPV6_TC \
	(ICE_PROT_IPV6_OUTER | ICE_IP_TOS)

#define ICE_INSET_TCP_SRC_PORT \
	(ICE_PROT_TCP_OUTER | ICE_SPORT)
#define ICE_INSET_TCP_DST_PORT \
	(ICE_PROT_TCP_OUTER | ICE_DPORT)
#define ICE_INSET_UDP_SRC_PORT \
	(ICE_PROT_UDP_OUTER | ICE_SPORT)
#define ICE_INSET_UDP_DST_PORT \
	(ICE_PROT_UDP_OUTER | ICE_DPORT)
#define ICE_INSET_SCTP_SRC_PORT \
	(ICE_PROT_SCTP_OUTER | ICE_SPORT)
#define ICE_INSET_SCTP_DST_PORT \
	(ICE_PROT_SCTP_OUTER | ICE_DPORT)
#define ICE_INSET_ICMP4_SRC_PORT \
	(ICE_PROT_ICMP4_OUTER | ICE_SPORT)
#define ICE_INSET_ICMP4_DST_PORT \
	(ICE_PROT_ICMP4_OUTER | ICE_DPORT)
#define ICE_INSET_ICMP6_SRC_PORT \
	(ICE_PROT_ICMP6_OUTER | ICE_SPORT)
#define ICE_INSET_ICMP6_DST_PORT \
	(ICE_PROT_ICMP6_OUTER | ICE_DPORT)
#define ICE_INSET_ICMP4_TYPE \
	(ICE_PROT_ICMP4_OUTER | ICE_ICMP_TYPE)
#define ICE_INSET_ICMP4_CODE \
	(ICE_PROT_ICMP4_OUTER | ICE_ICMP_CODE)
#define ICE_INSET_ICMP6_TYPE \
	(ICE_PROT_ICMP6_OUTER | ICE_ICMP_TYPE)
#define ICE_INSET_ICMP6_CODE \
	(ICE_PROT_ICMP6_OUTER | ICE_ICMP_CODE)

/* tunnel */

#define ICE_INSET_TUN_SMAC \
	(ICE_PROT_MAC_INNER | ICE_SMAC)
#define ICE_INSET_TUN_DMAC \
	(ICE_PROT_MAC_INNER | ICE_DMAC)

#define ICE_INSET_TUN_IPV4_SRC \
	(ICE_PROT_IPV4_INNER | ICE_IP_SRC)
#define ICE_INSET_TUN_IPV4_DST \
	(ICE_PROT_IPV4_INNER | ICE_IP_DST)
#define ICE_INSET_TUN_IPV4_TTL \
	(ICE_PROT_IPV4_INNER | ICE_IP_TTL)
#define ICE_INSET_TUN_IPV4_PROTO \
	(ICE_PROT_IPV4_INNER | ICE_IP_PROTO)
#define ICE_INSET_TUN_IPV4_TOS \
	(ICE_PROT_IPV4_INNER | ICE_IP_TOS)
#define ICE_INSET_TUN_IPV6_SRC \
	(ICE_PROT_IPV6_INNER | ICE_IP_SRC)
#define ICE_INSET_TUN_IPV6_DST \
	(ICE_PROT_IPV6_INNER | ICE_IP_DST)
#define ICE_INSET_TUN_IPV6_HOP_LIMIT \
	(ICE_PROT_IPV6_INNER | ICE_IP_TTL)
#define ICE_INSET_TUN_IPV6_NEXT_HDR \
	(ICE_PROT_IPV6_INNER | ICE_IP_PROTO)
#define ICE_INSET_TUN_IPV6_TC \
	(ICE_PROT_IPV6_INNER | ICE_IP_TOS)

#define ICE_INSET_TUN_TCP_SRC_PORT \
	(ICE_PROT_TCP_INNER | ICE_SPORT)
#define ICE_INSET_TUN_TCP_DST_PORT \
	(ICE_PROT_TCP_INNER | ICE_DPORT)
#define ICE_INSET_TUN_UDP_SRC_PORT \
	(ICE_PROT_UDP_INNER | ICE_SPORT)
#define ICE_INSET_TUN_UDP_DST_PORT \
	(ICE_PROT_UDP_INNER | ICE_DPORT)
#define ICE_INSET_TUN_SCTP_SRC_PORT \
	(ICE_PROT_SCTP_INNER | ICE_SPORT)
#define ICE_INSET_TUN_SCTP_DST_PORT \
	(ICE_PROT_SCTP_INNER | ICE_DPORT)
#define ICE_INSET_TUN_ICMP4_SRC_PORT \
	(ICE_PROT_ICMP4_INNER | ICE_SPORT)
#define ICE_INSET_TUN_ICMP4_DST_PORT \
	(ICE_PROT_ICMP4_INNER | ICE_DPORT)
#define ICE_INSET_TUN_ICMP6_SRC_PORT \
	(ICE_PROT_ICMP6_INNER | ICE_SPORT)
#define ICE_INSET_TUN_ICMP6_DST_PORT \
	(ICE_PROT_ICMP6_INNER | ICE_DPORT)
#define ICE_INSET_TUN_ICMP4_TYPE \
	(ICE_PROT_ICMP4_INNER | ICE_ICMP_TYPE)
#define ICE_INSET_TUN_ICMP4_CODE \
	(ICE_PROT_ICMP4_INNER | ICE_ICMP_CODE)
#define ICE_INSET_TUN_ICMP6_TYPE \
	(ICE_PROT_ICMP6_INNER | ICE_ICMP_TYPE)
#define ICE_INSET_TUN_ICMP6_CODE \
	(ICE_PROT_ICMP6_INNER | ICE_ICMP_CODE)

#define ICE_INSET_TUN_VXLAN_VNI \
	(ICE_PROT_VXLAN | ICE_VXLAN_VNI)
#define ICE_INSET_TUN_NVGRE_TNI \
	(ICE_PROT_NVGRE | ICE_NVGRE_TNI)
#define ICE_INSET_GTPU_TEID \
	(ICE_PROT_GTPU | ICE_GTPU_TEID)
#define ICE_INSET_GTPU_QFI \
	(ICE_PROT_GTPU | ICE_GTPU_QFI)

/* empty pattern */
extern enum rte_flow_item_type pattern_empty[];

/* L2 */
extern enum rte_flow_item_type pattern_ethertype[];
extern enum rte_flow_item_type pattern_ethertype_vlan[];
extern enum rte_flow_item_type pattern_ethertype_qinq[];

/* ARP */
extern enum rte_flow_item_type pattern_eth_arp[];

/* non-tunnel IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv4[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv4[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_icmp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv4_icmp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv4_icmp[];

/* non-tunnel IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv6[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv6[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_icmp6[];
extern enum rte_flow_item_type pattern_eth_vlan_ipv6_icmp6[];
extern enum rte_flow_item_type pattern_eth_qinq_ipv6_icmp6[];

/* IPv4 VXLAN IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv4_icmp[];

/* IPv4 VXLAN MAC IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv4_icmp[];

/* IPv6 VXLAN IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv4_icmp[];

/* IPv6 VXLAN MAC IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv4_icmp[];

/* IPv4 VXLAN IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_ipv6_icmp6[];

/* IPv4 VXLAN MAC IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_udp_vxlan_eth_ipv6_icmp6[];

/* IPv6 VXLAN IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_ipv6_icmp6[];

/* IPv6 VXLAN MAC IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_udp_vxlan_eth_ipv6_icmp6[];

/* IPv4 NVGRE IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv4_icmp[];

/* IPv4 NVGRE MAC IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv4_icmp[];

/* IPv6 NVGRE IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv4_icmp[];

/* IPv6 NVGRE MAC IPv4 */
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv4_icmp[];

/* IPv4 NVGRE IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_ipv6_icmp6[];

/* IPv4 NVGRE MAC IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv4_nvgre_eth_ipv6_icmp6[];

/* IPv6 NVGRE IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_ipv6_icmp6[];

/* IPv6 NVGRE MAC IPv6 */
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_ipv6_nvgre_eth_ipv6_icmp6[];

/* GTPU */
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_ipv4_gtpu_eh_ipv4_icmp[];

/* PPPoE */
extern enum rte_flow_item_type pattern_eth_pppoed[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoed[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoed[];
extern enum rte_flow_item_type pattern_eth_pppoes[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv4[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_udp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_tcp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_sctp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv4_icmp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv4_icmp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv4_icmp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv6[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_udp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_tcp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_sctp[];
extern enum rte_flow_item_type pattern_eth_pppoes_ipv6_icmp6[];
extern enum rte_flow_item_type pattern_eth_vlan_pppoes_ipv6_icmp6[];
extern enum rte_flow_item_type pattern_eth_qinq_pppoes_ipv6_icmp6[];

struct ice_adapter;

extern const struct rte_flow_ops ice_flow_ops;

/* engine types. */
enum ice_flow_engine_type {
	ICE_FLOW_ENGINE_NONE = 0,
	ICE_FLOW_ENGINE_FDIR,
	ICE_FLOW_ENGINE_SWITCH,
	ICE_FLOW_ENGINE_HASH,
	ICE_FLOW_ENGINE_ACL,
	ICE_FLOW_ENGINE_MAX,
};

/**
 * classification stages.
 * for non-pipeline mode, we have two classification stages: Distributor/RSS
 * for pipeline-mode we have three classification stages:
 * Permission/Distributor/RSS
 */
enum ice_flow_classification_stage {
	ICE_FLOW_STAGE_NONE = 0,
	ICE_FLOW_STAGE_RSS,
	ICE_FLOW_STAGE_PERMISSION,
	ICE_FLOW_STAGE_DISTRIBUTOR,
	ICE_FLOW_STAGE_MAX,
};
/* pattern structure */
struct ice_pattern_match_item {
	enum rte_flow_item_type *pattern_list;
	/* pattern_list must end with RTE_FLOW_ITEM_TYPE_END */
	uint64_t input_set_mask;
	void *meta;
};

typedef int (*engine_init_t)(struct ice_adapter *ad);
typedef void (*engine_uninit_t)(struct ice_adapter *ad);
typedef int (*engine_create_t)(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error);
typedef int (*engine_destroy_t)(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error);
typedef int (*engine_query_t)(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_query_count *count,
		struct rte_flow_error *error);
typedef void (*engine_free_t) (struct rte_flow *flow);
typedef int (*parse_pattern_action_t)(struct ice_adapter *ad,
		struct ice_pattern_match_item *array,
		uint32_t array_len,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error);

/* Struct to store engine created. */
struct ice_flow_engine {
	TAILQ_ENTRY(ice_flow_engine) node;
	engine_init_t init;
	engine_uninit_t uninit;
	engine_create_t create;
	engine_destroy_t destroy;
	engine_query_t query_count;
	engine_free_t free;
	enum ice_flow_engine_type type;
};
TAILQ_HEAD(ice_engine_list, ice_flow_engine);

/* Struct to store flow created. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) node;
	struct ice_flow_engine *engine;
	void *rule;
};

struct ice_flow_parser {
	struct ice_flow_engine *engine;
	struct ice_pattern_match_item *array;
	uint32_t array_len;
	parse_pattern_action_t parse_pattern_action;
	enum ice_flow_classification_stage stage;
};

/* Struct to store parser created. */
struct ice_flow_parser_node {
	TAILQ_ENTRY(ice_flow_parser_node) node;
	struct ice_flow_parser *parser;
};

void ice_register_flow_engine(struct ice_flow_engine *engine);
int ice_flow_init(struct ice_adapter *ad);
void ice_flow_uninit(struct ice_adapter *ad);
int ice_register_parser(struct ice_flow_parser *parser,
		struct ice_adapter *ad);
void ice_unregister_parser(struct ice_flow_parser *parser,
		struct ice_adapter *ad);
struct ice_pattern_match_item *
ice_search_pattern_match_item(const struct rte_flow_item pattern[],
		struct ice_pattern_match_item *array,
		uint32_t array_len,
		struct rte_flow_error *error);
#endif
