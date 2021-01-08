/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2019
 */

#include "ice_common.h"
#include "ice_fdir.h"

/* These are training packet headers used to program flow director filters. */
static const u8 ice_fdir_tcpv4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x28, 0x00, 0x01, 0x00, 0x00, 0x40, 0x06,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 ice_fdir_udpv4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x1C, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_sctpv4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x20, 0x00, 0x00, 0x40, 0x00, 0x40, 0x84,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_ipv4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x14, 0x00, 0x00, 0x40, 0x00, 0x40, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static const u8 ice_fdir_udp4_gtpu4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x4c, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x68, 0x08, 0x68, 0x00, 0x00,
	0x00, 0x00, 0x34, 0xff, 0x00, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00,
	0x00, 0x1c, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_tcp4_gtpu4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x58, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x68, 0x08, 0x68, 0x00, 0x00,
	0x00, 0x00, 0x34, 0xff, 0x00, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00,
	0x00, 0x28, 0x00, 0x00, 0x40, 0x00, 0x40, 0x06,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_icmp4_gtpu4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x4c, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x68, 0x08, 0x68, 0x00, 0x00,
	0x00, 0x00, 0x34, 0xff, 0x00, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00,
	0x00, 0x1c, 0x00, 0x00, 0x40, 0x00, 0x40, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_ipv4_gtpu4_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x44, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x68, 0x08, 0x68, 0x00, 0x00,
	0x00, 0x00, 0x34, 0xff, 0x00, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00,
	0x00, 0x14, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_tcpv6_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xDD, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x06, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x50, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_udpv6_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xDD, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x11, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
};

static const u8 ice_fdir_sctpv6_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xDD, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x0C, 0x84, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static const u8 ice_fdir_ipv6_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xDD, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3B, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_tcp4_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x5a, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x45, 0x00, 0x00, 0x28, 0x00, 0x00, 0x40, 0x00,
	0x40, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x50, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_udp4_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x4e, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x45, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x40, 0x00,
	0x40, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_sctp4_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x52, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x45, 0x00, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00,
	0x40, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_ip4_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x46, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x45, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_tcp6_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x6e, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xdd,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x14, 0x06, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_udp6_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x62, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xdd,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x08, 0x11, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_sctp6_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x66, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xdd,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x84, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const u8 ice_fdir_ip6_tun_pkt[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x5a, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xdd,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Flow Director no-op training packet table */
static const struct ice_fdir_base_pkt ice_fdir_pkt[] = {
	{
		ICE_FLTR_PTYPE_NONF_IPV4_TCP,
		sizeof(ice_fdir_tcpv4_pkt), ice_fdir_tcpv4_pkt,
		sizeof(ice_fdir_tcp4_tun_pkt), ice_fdir_tcp4_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_UDP,
		sizeof(ice_fdir_udpv4_pkt), ice_fdir_udpv4_pkt,
		sizeof(ice_fdir_udp4_tun_pkt), ice_fdir_udp4_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_SCTP,
		sizeof(ice_fdir_sctpv4_pkt), ice_fdir_sctpv4_pkt,
		sizeof(ice_fdir_sctp4_tun_pkt), ice_fdir_sctp4_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_OTHER,
		sizeof(ice_fdir_ipv4_pkt), ice_fdir_ipv4_pkt,
		sizeof(ice_fdir_ip4_tun_pkt), ice_fdir_ip4_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP,
		sizeof(ice_fdir_udp4_gtpu4_pkt),
		ice_fdir_udp4_gtpu4_pkt,
		sizeof(ice_fdir_udp4_gtpu4_pkt),
		ice_fdir_udp4_gtpu4_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP,
		sizeof(ice_fdir_tcp4_gtpu4_pkt),
		ice_fdir_tcp4_gtpu4_pkt,
		sizeof(ice_fdir_tcp4_gtpu4_pkt),
		ice_fdir_tcp4_gtpu4_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP,
		sizeof(ice_fdir_icmp4_gtpu4_pkt),
		ice_fdir_icmp4_gtpu4_pkt,
		sizeof(ice_fdir_icmp4_gtpu4_pkt),
		ice_fdir_icmp4_gtpu4_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER,
		sizeof(ice_fdir_ipv4_gtpu4_pkt),
		ice_fdir_ipv4_gtpu4_pkt,
		sizeof(ice_fdir_ipv4_gtpu4_pkt),
		ice_fdir_ipv4_gtpu4_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV6_TCP,
		sizeof(ice_fdir_tcpv6_pkt), ice_fdir_tcpv6_pkt,
		sizeof(ice_fdir_tcp6_tun_pkt), ice_fdir_tcp6_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV6_UDP,
		sizeof(ice_fdir_udpv6_pkt), ice_fdir_udpv6_pkt,
		sizeof(ice_fdir_udp6_tun_pkt), ice_fdir_udp6_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV6_SCTP,
		sizeof(ice_fdir_sctpv6_pkt), ice_fdir_sctpv6_pkt,
		sizeof(ice_fdir_sctp6_tun_pkt), ice_fdir_sctp6_tun_pkt,
	},
	{
		ICE_FLTR_PTYPE_NONF_IPV6_OTHER,
		sizeof(ice_fdir_ipv6_pkt), ice_fdir_ipv6_pkt,
		sizeof(ice_fdir_ip6_tun_pkt), ice_fdir_ip6_tun_pkt,
	},
};

#define ICE_FDIR_NUM_PKT ARRAY_SIZE(ice_fdir_pkt)

/**
 * ice_set_dflt_val_fd_desc
 * @fd_fltr_ctx: pointer to fd filter descriptor
 */
void
ice_set_dflt_val_fd_desc(struct ice_fd_fltr_desc_ctx *fd_fltr_ctx)
{
	fd_fltr_ctx->comp_q = ICE_FXD_FLTR_QW0_COMP_Q_ZERO;
	fd_fltr_ctx->comp_report = ICE_FXD_FLTR_QW0_COMP_REPORT_SW_FAIL;
	fd_fltr_ctx->fd_space = ICE_FXD_FLTR_QW0_FD_SPACE_GUAR_BEST;
	fd_fltr_ctx->cnt_ena = ICE_FXD_FLTR_QW0_STAT_ENA_PKTS;
	fd_fltr_ctx->evict_ena = ICE_FXD_FLTR_QW0_EVICT_ENA_TRUE;
	fd_fltr_ctx->toq = ICE_FXD_FLTR_QW0_TO_Q_EQUALS_QINDEX;
	fd_fltr_ctx->toq_prio = ICE_FXD_FLTR_QW0_TO_Q_PRIO1;
	fd_fltr_ctx->dpu_recipe = ICE_FXD_FLTR_QW0_DPU_RECIPE_DFLT;
	fd_fltr_ctx->drop = ICE_FXD_FLTR_QW0_DROP_NO;
	fd_fltr_ctx->flex_prio = ICE_FXD_FLTR_QW0_FLEX_PRI_NONE;
	fd_fltr_ctx->flex_mdid = ICE_FXD_FLTR_QW0_FLEX_MDID0;
	fd_fltr_ctx->flex_val = ICE_FXD_FLTR_QW0_FLEX_VAL0;
	fd_fltr_ctx->dtype = ICE_TX_DESC_DTYPE_FLTR_PROG;
	fd_fltr_ctx->desc_prof_prio = ICE_FXD_FLTR_QW1_PROF_PRIO_ZERO;
	fd_fltr_ctx->desc_prof = ICE_FXD_FLTR_QW1_PROF_ZERO;
	fd_fltr_ctx->swap = ICE_FXD_FLTR_QW1_SWAP_SET;
	fd_fltr_ctx->fdid_prio = ICE_FXD_FLTR_QW1_FDID_PRI_ONE;
	fd_fltr_ctx->fdid_mdid = ICE_FXD_FLTR_QW1_FDID_MDID_FD;
	fd_fltr_ctx->fdid = ICE_FXD_FLTR_QW1_FDID_ZERO;
}

/**
 * ice_fdir_get_prgm_desc - set a fdir descriptor from a fdir filter struct
 * @hw: pointer to the hardware structure
 * @input: filter
 * @fdesc: filter descriptor
 * @add: if add is true, this is an add operation, false implies delete
 */
void
ice_fdir_get_prgm_desc(struct ice_hw *hw, struct ice_fdir_fltr *input,
		       struct ice_fltr_desc *fdesc, bool add)
{
	struct ice_fd_fltr_desc_ctx fdir_fltr_ctx = { 0 };

	/* set default context info */
	ice_set_dflt_val_fd_desc(&fdir_fltr_ctx);

	/* change sideband filtering values */
	fdir_fltr_ctx.fdid = input->fltr_id;
	if (input->dest_ctl == ICE_FLTR_PRGM_DESC_DEST_DROP_PKT) {
		fdir_fltr_ctx.drop = ICE_FXD_FLTR_QW0_DROP_YES;
		fdir_fltr_ctx.qindex = 0;
	} else if (input->dest_ctl ==
			ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER) {
		fdir_fltr_ctx.drop = ICE_FXD_FLTR_QW0_DROP_NO;
		fdir_fltr_ctx.qindex = 0;
	} else {
		if (input->dest_ctl ==
		    ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QGROUP)
			fdir_fltr_ctx.toq = input->q_region;
		fdir_fltr_ctx.drop = ICE_FXD_FLTR_QW0_DROP_NO;
		fdir_fltr_ctx.qindex = input->q_index;
	}
	fdir_fltr_ctx.cnt_ena = input->cnt_ena;
	fdir_fltr_ctx.cnt_index = input->cnt_index;
	fdir_fltr_ctx.fd_vsi = ice_get_hw_vsi_num(hw, input->dest_vsi);
	fdir_fltr_ctx.evict_ena = ICE_FXD_FLTR_QW0_EVICT_ENA_FALSE;
	if (input->dest_ctl == ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER)
		fdir_fltr_ctx.toq_prio = 0;
	else
		fdir_fltr_ctx.toq_prio = 3;
	fdir_fltr_ctx.pcmd = (add) ? ICE_FXD_FLTR_QW1_PCMD_ADD :
		ICE_FXD_FLTR_QW1_PCMD_REMOVE;
	fdir_fltr_ctx.swap = ICE_FXD_FLTR_QW1_SWAP_NOT_SET;
	fdir_fltr_ctx.comp_q = ICE_FXD_FLTR_QW0_COMP_Q_ZERO;
	fdir_fltr_ctx.comp_report = ICE_FXD_FLTR_QW0_COMP_REPORT_SW;
	fdir_fltr_ctx.fdid_prio = input->fdid_prio;
	fdir_fltr_ctx.desc_prof = 1;
	fdir_fltr_ctx.desc_prof_prio = 3;
	ice_set_fd_desc_val(&fdir_fltr_ctx, fdesc);
}

/**
 * ice_set_fd_desc_val
 * @ctx: pointer to fd filter descriptor context
 * @fdir_desc: populated with fd filter descriptor values
 */
void
ice_set_fd_desc_val(struct ice_fd_fltr_desc_ctx *ctx,
		    struct ice_fltr_desc *fdir_desc)
{
	u64 qword;

	/* prep QW0 of FD filter programming desc */
	qword = ((u64)ctx->qindex << ICE_FXD_FLTR_QW0_QINDEX_S) &
		ICE_FXD_FLTR_QW0_QINDEX_M;
	qword |= ((u64)ctx->comp_q << ICE_FXD_FLTR_QW0_COMP_Q_S) &
		 ICE_FXD_FLTR_QW0_COMP_Q_M;
	qword |= ((u64)ctx->comp_report << ICE_FXD_FLTR_QW0_COMP_REPORT_S) &
		 ICE_FXD_FLTR_QW0_COMP_REPORT_M;
	qword |= ((u64)ctx->fd_space << ICE_FXD_FLTR_QW0_FD_SPACE_S) &
		 ICE_FXD_FLTR_QW0_FD_SPACE_M;
	qword |= ((u64)ctx->cnt_index << ICE_FXD_FLTR_QW0_STAT_CNT_S) &
		 ICE_FXD_FLTR_QW0_STAT_CNT_M;
	qword |= ((u64)ctx->cnt_ena << ICE_FXD_FLTR_QW0_STAT_ENA_S) &
		 ICE_FXD_FLTR_QW0_STAT_ENA_M;
	qword |= ((u64)ctx->evict_ena << ICE_FXD_FLTR_QW0_EVICT_ENA_S) &
		 ICE_FXD_FLTR_QW0_EVICT_ENA_M;
	qword |= ((u64)ctx->toq << ICE_FXD_FLTR_QW0_TO_Q_S) &
		 ICE_FXD_FLTR_QW0_TO_Q_M;
	qword |= ((u64)ctx->toq_prio << ICE_FXD_FLTR_QW0_TO_Q_PRI_S) &
		 ICE_FXD_FLTR_QW0_TO_Q_PRI_M;
	qword |= ((u64)ctx->dpu_recipe << ICE_FXD_FLTR_QW0_DPU_RECIPE_S) &
		 ICE_FXD_FLTR_QW0_DPU_RECIPE_M;
	qword |= ((u64)ctx->drop << ICE_FXD_FLTR_QW0_DROP_S) &
		 ICE_FXD_FLTR_QW0_DROP_M;
	qword |= ((u64)ctx->flex_prio << ICE_FXD_FLTR_QW0_FLEX_PRI_S) &
		 ICE_FXD_FLTR_QW0_FLEX_PRI_M;
	qword |= ((u64)ctx->flex_mdid << ICE_FXD_FLTR_QW0_FLEX_MDID_S) &
		 ICE_FXD_FLTR_QW0_FLEX_MDID_M;
	qword |= ((u64)ctx->flex_val << ICE_FXD_FLTR_QW0_FLEX_VAL_S) &
		 ICE_FXD_FLTR_QW0_FLEX_VAL_M;
	fdir_desc->qidx_compq_space_stat = CPU_TO_LE64(qword);

	/* prep QW1 of FD filter programming desc */
	qword = ((u64)ctx->dtype << ICE_FXD_FLTR_QW1_DTYPE_S) &
		ICE_FXD_FLTR_QW1_DTYPE_M;
	qword |= ((u64)ctx->pcmd << ICE_FXD_FLTR_QW1_PCMD_S) &
		 ICE_FXD_FLTR_QW1_PCMD_M;
	qword |= ((u64)ctx->desc_prof_prio << ICE_FXD_FLTR_QW1_PROF_PRI_S) &
		 ICE_FXD_FLTR_QW1_PROF_PRI_M;
	qword |= ((u64)ctx->desc_prof << ICE_FXD_FLTR_QW1_PROF_S) &
		 ICE_FXD_FLTR_QW1_PROF_M;
	qword |= ((u64)ctx->fd_vsi << ICE_FXD_FLTR_QW1_FD_VSI_S) &
		 ICE_FXD_FLTR_QW1_FD_VSI_M;
	qword |= ((u64)ctx->swap << ICE_FXD_FLTR_QW1_SWAP_S) &
		 ICE_FXD_FLTR_QW1_SWAP_M;
	qword |= ((u64)ctx->fdid_prio << ICE_FXD_FLTR_QW1_FDID_PRI_S) &
		 ICE_FXD_FLTR_QW1_FDID_PRI_M;
	qword |= ((u64)ctx->fdid_mdid << ICE_FXD_FLTR_QW1_FDID_MDID_S) &
		 ICE_FXD_FLTR_QW1_FDID_MDID_M;
	qword |= ((u64)ctx->fdid << ICE_FXD_FLTR_QW1_FDID_S) &
		 ICE_FXD_FLTR_QW1_FDID_M;
	fdir_desc->dtype_cmd_vsi_fdid = CPU_TO_LE64(qword);
}

/**
 * ice_alloc_fd_res_cntr - obtain counter resource for FD type
 * @hw: pointer to the hardware structure
 * @cntr_id: returns counter index
 */
enum ice_status ice_alloc_fd_res_cntr(struct ice_hw *hw, u16 *cntr_id)
{
	return ice_alloc_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_COUNTER_BLOCK,
				  ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1, cntr_id);
}

/**
 * ice_free_fd_res_cntr - Free counter resource for FD type
 * @hw: pointer to the hardware structure
 * @cntr_id: counter index to be freed
 */
enum ice_status ice_free_fd_res_cntr(struct ice_hw *hw, u16 cntr_id)
{
	return ice_free_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_COUNTER_BLOCK,
				 ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1, cntr_id);
}

/**
 * ice_alloc_fd_guar_item - allocate resource for FD guaranteed entries
 * @hw: pointer to the hardware structure
 * @cntr_id: returns counter index
 * @num_fltr: number of filter entries to be allocated
 */
enum ice_status
ice_alloc_fd_guar_item(struct ice_hw *hw, u16 *cntr_id, u16 num_fltr)
{
	return ice_alloc_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_GUARANTEED_ENTRIES,
				  ICE_AQC_RES_TYPE_FLAG_DEDICATED, num_fltr,
				  cntr_id);
}

/**
 * ice_free_fd_guar_item - Free flow director guaranteed entries
 * @hw: pointer to the hardware structure
 * @cntr_id: counter index that needs to be freed
 * @num_fltr: number of filters to be freed
 */
enum ice_status
ice_free_fd_guar_item(struct ice_hw *hw, u16 cntr_id, u16 num_fltr)
{
	return ice_free_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_GUARANTEED_ENTRIES,
				 ICE_AQC_RES_TYPE_FLAG_DEDICATED, num_fltr,
				 cntr_id);
}

/**
 * ice_alloc_fd_shrd_item - allocate resource for flow director shared entries
 * @hw: pointer to the hardware structure
 * @cntr_id: returns counter index
 * @num_fltr: number of filter entries to be allocated
 */
enum ice_status
ice_alloc_fd_shrd_item(struct ice_hw *hw, u16 *cntr_id, u16 num_fltr)
{
	return ice_alloc_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_SHARED_ENTRIES,
				  ICE_AQC_RES_TYPE_FLAG_DEDICATED, num_fltr,
				  cntr_id);
}

/**
 * ice_free_fd_shrd_item - Free flow director shared entries
 * @hw: pointer to the hardware structure
 * @cntr_id: counter index that needs to be freed
 * @num_fltr: number of filters to be freed
 */
enum ice_status
ice_free_fd_shrd_item(struct ice_hw *hw, u16 cntr_id, u16 num_fltr)
{
	return ice_free_res_cntr(hw, ICE_AQC_RES_TYPE_FDIR_SHARED_ENTRIES,
				 ICE_AQC_RES_TYPE_FLAG_DEDICATED, num_fltr,
				 cntr_id);
}

/**
 * ice_get_fdir_cnt_all - get the number of Flow Director filters
 * @hw: hardware data structure
 *
 * Returns the number of filters available on device
 */
int ice_get_fdir_cnt_all(struct ice_hw *hw)
{
	return hw->func_caps.fd_fltr_guar +
	       hw->func_caps.fd_fltr_best_effort;
}

/**
 * ice_pkt_insert_ipv6_addr - insert a be32 IPv6 address into a memory buffer.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @addr: IPv6 address to convert and insert into pkt at offset
 */
static void ice_pkt_insert_ipv6_addr(u8 *pkt, int offset, __be32 *addr)
{
	int idx;

	for (idx = 0; idx < ICE_IPV6_ADDR_LEN_AS_U32; idx++)
		ice_memcpy(pkt + offset + idx * sizeof(*addr), &addr[idx],
			   sizeof(*addr), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_u6_qfi - insert a u6 value qfi into a memory buffer for gtpu
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @data: 8 bit value to convert and insert into pkt at offset
 *
 * This function is designed for inserting qfi (6 bits) for gtpu.
 */
static void ice_pkt_insert_u6_qfi(u8 *pkt, int offset, u8 data)
{
	u8 ret;

	ret = (data & 0x3F) + (*(pkt + offset) & 0xC0);
	ice_memcpy(pkt + offset, &ret, sizeof(ret), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_u8 - insert a u8 value into a memory buffer.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @data: 8 bit value to convert and insert into pkt at offset
 */
static void ice_pkt_insert_u8(u8 *pkt, int offset, u8 data)
{
	ice_memcpy(pkt + offset, &data, sizeof(data), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_u8_tc - insert a u8 value into a memory buffer for tc ipv6.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @data: 8 bit value to convert and insert into pkt at offset
 *
 * This function is designed for inserting Traffic Class (tc) for IPv6,
 * since that tc is not aligned in number of bytes. Here we split it out
 * into two part and fill each byte with data copy from pkt, then insert
 * the two bytes data one by one.
 */
static void ice_pkt_insert_u8_tc(u8 *pkt, int offset, u8 data)
{
	u8 high, low;

	high = (data >> 4) + (*(pkt + offset) & 0xF0);
	ice_memcpy(pkt + offset, &high, sizeof(high), ICE_NONDMA_TO_NONDMA);

	low = (*(pkt + offset + 1) & 0x0F) + ((data & 0x0F) << 4);
	ice_memcpy(pkt + offset + 1, &low, sizeof(low), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_u16 - insert a be16 value into a memory buffer.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @data: 16 bit value to convert and insert into pkt at offset
 */
static void ice_pkt_insert_u16(u8 *pkt, int offset, __be16 data)
{
	ice_memcpy(pkt + offset, &data, sizeof(data), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_u32 - insert a be32 value into a memory buffer.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @data: 32 bit value to convert and insert into pkt at offset
 */
static void ice_pkt_insert_u32(u8 *pkt, int offset, __be32 data)
{
	ice_memcpy(pkt + offset, &data, sizeof(data), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_pkt_insert_mac_addr - insert a MAC addr into a memory buffer.
 * @pkt: packet buffer
 * @offset: offset into buffer
 * @addr: MAC address to convert and insert into pkt at offset
 */
static void ice_pkt_insert_mac_addr(u8 *pkt, u8 *addr)
{
	ice_memcpy(pkt, addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_fdir_get_gen_prgm_pkt - generate a training packet
 * @hw: pointer to the hardware structure
 * @input: flow director filter data structure
 * @pkt: pointer to return filter packet
 * @frag: generate a fragment packet
 * @tun: true implies generate a tunnel packet
 */
enum ice_status
ice_fdir_get_gen_prgm_pkt(struct ice_hw *hw, struct ice_fdir_fltr *input,
			  u8 *pkt, bool frag, bool tun)
{
	enum ice_fltr_ptype flow;
	u16 tnl_port;
	u8 *loc;
	u16 idx;

	if (input->flow_type == ICE_FLTR_PTYPE_NONF_IPV4_OTHER) {
		switch (input->ip.v4.proto) {
		case ICE_IP_PROTO_TCP:
			flow = ICE_FLTR_PTYPE_NONF_IPV4_TCP;
			break;
		case ICE_IP_PROTO_UDP:
			flow = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
			break;
		case ICE_IP_PROTO_SCTP:
			flow = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
			break;
		default:
			flow = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
			break;
		}
	} else if (input->flow_type == ICE_FLTR_PTYPE_NONF_IPV6_OTHER) {
		switch (input->ip.v6.proto) {
		case ICE_IP_PROTO_TCP:
			flow = ICE_FLTR_PTYPE_NONF_IPV6_TCP;
			break;
		case ICE_IP_PROTO_UDP:
			flow = ICE_FLTR_PTYPE_NONF_IPV6_UDP;
			break;
		case ICE_IP_PROTO_SCTP:
			flow = ICE_FLTR_PTYPE_NONF_IPV6_SCTP;
			break;
		default:
			flow = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
			break;
		}
	} else {
		flow = input->flow_type;
	}

	for (idx = 0; idx < ICE_FDIR_NUM_PKT; idx++)
		if (ice_fdir_pkt[idx].flow == flow)
			break;
	if (idx == ICE_FDIR_NUM_PKT)
		return ICE_ERR_PARAM;
	if (!tun) {
		ice_memcpy(pkt, ice_fdir_pkt[idx].pkt,
			   ice_fdir_pkt[idx].pkt_len, ICE_NONDMA_TO_NONDMA);
		loc = pkt;
	} else {
		if (!ice_get_open_tunnel_port(hw, TNL_ALL, &tnl_port))
			return ICE_ERR_DOES_NOT_EXIST;
		if (!ice_fdir_pkt[idx].tun_pkt)
			return ICE_ERR_PARAM;
		ice_memcpy(pkt, ice_fdir_pkt[idx].tun_pkt,
			   ice_fdir_pkt[idx].tun_pkt_len, ICE_NONDMA_TO_NONDMA);
		ice_pkt_insert_u16(pkt, ICE_IPV4_UDP_DST_PORT_OFFSET,
				   HTONS(tnl_port));
		loc = &pkt[ICE_FDIR_TUN_PKT_OFF];
	}

	switch (flow) {
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		ice_pkt_insert_u32(loc, ICE_IPV4_DST_ADDR_OFFSET,
				   input->ip.v4.dst_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_TCP_DST_PORT_OFFSET,
				   input->ip.v4.dst_port);
		ice_pkt_insert_u32(loc, ICE_IPV4_SRC_ADDR_OFFSET,
				   input->ip.v4.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_TCP_SRC_PORT_OFFSET,
				   input->ip.v4.src_port);
		ice_pkt_insert_u8(loc, ICE_IPV4_TOS_OFFSET, input->ip.v4.tos);
		ice_pkt_insert_u8(loc, ICE_IPV4_TTL_OFFSET, input->ip.v4.ttl);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		if (frag)
			loc[20] = ICE_FDIR_IPV4_PKT_FLAG_DF;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		ice_pkt_insert_u32(loc, ICE_IPV4_DST_ADDR_OFFSET,
				   input->ip.v4.dst_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_UDP_DST_PORT_OFFSET,
				   input->ip.v4.dst_port);
		ice_pkt_insert_u32(loc, ICE_IPV4_SRC_ADDR_OFFSET,
				   input->ip.v4.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_UDP_SRC_PORT_OFFSET,
				   input->ip.v4.src_port);
		ice_pkt_insert_u8(loc, ICE_IPV4_TOS_OFFSET, input->ip.v4.tos);
		ice_pkt_insert_u8(loc, ICE_IPV4_TTL_OFFSET, input->ip.v4.ttl);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		ice_pkt_insert_u32(loc, ICE_IPV4_DST_ADDR_OFFSET,
				   input->ip.v4.dst_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_SCTP_DST_PORT_OFFSET,
				   input->ip.v4.dst_port);
		ice_pkt_insert_u32(loc, ICE_IPV4_SRC_ADDR_OFFSET,
				   input->ip.v4.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV4_SCTP_SRC_PORT_OFFSET,
				   input->ip.v4.src_port);
		ice_pkt_insert_u8(loc, ICE_IPV4_TOS_OFFSET, input->ip.v4.tos);
		ice_pkt_insert_u8(loc, ICE_IPV4_TTL_OFFSET, input->ip.v4.ttl);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		ice_pkt_insert_u32(loc, ICE_IPV4_DST_ADDR_OFFSET,
				   input->ip.v4.dst_ip);
		ice_pkt_insert_u32(loc, ICE_IPV4_SRC_ADDR_OFFSET,
				   input->ip.v4.src_ip);
		ice_pkt_insert_u8(loc, ICE_IPV4_TOS_OFFSET, input->ip.v4.tos);
		ice_pkt_insert_u8(loc, ICE_IPV4_TTL_OFFSET, input->ip.v4.ttl);
		ice_pkt_insert_u8(loc, ICE_IPV4_PROTO_OFFSET,
				  input->ip.v4.proto);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER:
		ice_pkt_insert_u32(loc, ICE_IPV4_SRC_ADDR_OFFSET,
					input->ip.v4.src_ip);
		ice_pkt_insert_u32(loc, ICE_IPV4_DST_ADDR_OFFSET,
					input->ip.v4.dst_ip);
		ice_pkt_insert_u32(loc, ICE_IPV4_GTPU_TEID_OFFSET,
				   input->gtpu_data.teid);
		ice_pkt_insert_u6_qfi(loc, ICE_IPV4_GTPU_QFI_OFFSET,
				      input->gtpu_data.qfi);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_DST_ADDR_OFFSET,
					 input->ip.v6.dst_ip);
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_SRC_ADDR_OFFSET,
					 input->ip.v6.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV6_TCP_DST_PORT_OFFSET,
				   input->ip.v6.dst_port);
		ice_pkt_insert_u16(loc, ICE_IPV6_TCP_SRC_PORT_OFFSET,
				   input->ip.v6.src_port);
		ice_pkt_insert_u8_tc(loc, ICE_IPV6_TC_OFFSET, input->ip.v6.tc);
		ice_pkt_insert_u8(loc, ICE_IPV6_HLIM_OFFSET, input->ip.v6.hlim);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_DST_ADDR_OFFSET,
					 input->ip.v6.dst_ip);
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_SRC_ADDR_OFFSET,
					 input->ip.v6.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV6_UDP_DST_PORT_OFFSET,
				   input->ip.v6.dst_port);
		ice_pkt_insert_u16(loc, ICE_IPV6_UDP_SRC_PORT_OFFSET,
				   input->ip.v6.src_port);
		ice_pkt_insert_u8_tc(loc, ICE_IPV6_TC_OFFSET, input->ip.v6.tc);
		ice_pkt_insert_u8(loc, ICE_IPV6_HLIM_OFFSET, input->ip.v6.hlim);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_DST_ADDR_OFFSET,
					 input->ip.v6.dst_ip);
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_SRC_ADDR_OFFSET,
					 input->ip.v6.src_ip);
		ice_pkt_insert_u16(loc, ICE_IPV6_SCTP_DST_PORT_OFFSET,
				   input->ip.v6.dst_port);
		ice_pkt_insert_u16(loc, ICE_IPV6_SCTP_SRC_PORT_OFFSET,
				   input->ip.v6.src_port);
		ice_pkt_insert_u8_tc(loc, ICE_IPV6_TC_OFFSET, input->ip.v6.tc);
		ice_pkt_insert_u8(loc, ICE_IPV6_HLIM_OFFSET, input->ip.v6.hlim);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_DST_ADDR_OFFSET,
					 input->ip.v6.dst_ip);
		ice_pkt_insert_ipv6_addr(loc, ICE_IPV6_SRC_ADDR_OFFSET,
					 input->ip.v6.src_ip);
		ice_pkt_insert_u8_tc(loc, ICE_IPV6_TC_OFFSET, input->ip.v6.tc);
		ice_pkt_insert_u8(loc, ICE_IPV6_HLIM_OFFSET, input->ip.v6.hlim);
		ice_pkt_insert_u8(loc, ICE_IPV6_PROTO_OFFSET,
				  input->ip.v6.proto);
		ice_pkt_insert_mac_addr(loc, input->ext_data.dst_mac);
		break;
	default:
		return ICE_ERR_PARAM;
	}

	if (input->flex_fltr)
		ice_pkt_insert_u16(loc, input->flex_offset, input->flex_word);

	return ICE_SUCCESS;
}

/**
 * ice_fdir_get_prgm_pkt - generate a training packet
 * @input: flow director filter data structure
 * @pkt: pointer to return filter packet
 * @frag: generate a fragment packet
 */
enum ice_status
ice_fdir_get_prgm_pkt(struct ice_fdir_fltr *input, u8 *pkt, bool frag)
{
	return ice_fdir_get_gen_prgm_pkt(NULL, input, pkt, frag, false);
}

/**
 * ice_fdir_has_frag - does flow type have 2 ptypes
 * @flow: flow ptype
 *
 * returns true is there is a fragment packet for this ptype
 */
bool ice_fdir_has_frag(enum ice_fltr_ptype flow)
{
	if (flow == ICE_FLTR_PTYPE_NONF_IPV4_OTHER)
		return true;
	else
		return false;
}

/**
 * ice_fdir_find_by_idx - find filter with idx
 * @hw: pointer to hardware structure
 * @fltr_idx: index to find.
 *
 * Returns pointer to filter if found or null
 */
struct ice_fdir_fltr *
ice_fdir_find_fltr_by_idx(struct ice_hw *hw, u32 fltr_idx)
{
	struct ice_fdir_fltr *rule = NULL;

	LIST_FOR_EACH_ENTRY(rule, &hw->fdir_list_head, ice_fdir_fltr,
			    fltr_node) {
		/* rule ID found in the list */
		if (fltr_idx == rule->fltr_id)
			return rule;
		if (fltr_idx < rule->fltr_id)
			break;
	}
	return NULL;
}

/**
 * ice_fdir_list_add_fltr - add a new node to the flow director filter list
 * @hw: hardware structure
 * @fltr: filter node to add to structure
 */
void ice_fdir_list_add_fltr(struct ice_hw *hw, struct ice_fdir_fltr *fltr)
{
	struct ice_fdir_fltr *rule, *parent = NULL;

	LIST_FOR_EACH_ENTRY(rule, &hw->fdir_list_head, ice_fdir_fltr,
			    fltr_node) {
		/* rule ID found or pass its spot in the list */
		if (rule->fltr_id >= fltr->fltr_id)
			break;
		parent = rule;
	}

	if (parent)
		LIST_ADD_AFTER(&fltr->fltr_node, &parent->fltr_node);
	else
		LIST_ADD(&fltr->fltr_node, &hw->fdir_list_head);
}

/**
 * ice_fdir_update_cntrs - increment / decrement filter counter
 * @hw: pointer to hardware structure
 * @flow: filter flow type
 * @add: true implies filters added
 */
void
ice_fdir_update_cntrs(struct ice_hw *hw, enum ice_fltr_ptype flow, bool add)
{
	int incr;

	incr = (add) ? 1 : -1;
	hw->fdir_active_fltr += incr;
	if (flow == ICE_FLTR_PTYPE_NONF_NONE || flow >= ICE_FLTR_PTYPE_MAX)
		ice_debug(hw, ICE_DBG_SW, "Unknown filter type %d\n", flow);
	else
		hw->fdir_fltr_cnt[flow] += incr;
}

/**
 * ice_cmp_ipv6_addr - compare 2 IP v6 addresses
 * @a: IP v6 address
 * @b: IP v6 address
 *
 * Returns 0 on equal, returns non-0 if different
 */
static int ice_cmp_ipv6_addr(__be32 *a, __be32 *b)
{
	return memcmp(a, b, 4 * sizeof(__be32));
}

/**
 * ice_fdir_comp_ipv6_rules - compare 2 filters
 * @a: a Flow Director filter data structure
 * @b: a Flow Director filter data structure
 * @v6: bool true if v6 filter
 *
 * Returns true if the filters match
 */
static bool
ice_fdir_comp_rules(struct ice_fdir_fltr *a,  struct ice_fdir_fltr *b, bool v6)
{
	enum ice_fltr_ptype flow_type = a->flow_type;

	/* The calling function already checks that the two filters have the
	 * same flow_type.
	 */
	if (!v6) {
		if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_TCP ||
		    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP ||
		    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_SCTP) {
			if (a->ip.v4.dst_ip == b->ip.v4.dst_ip &&
			    a->ip.v4.src_ip == b->ip.v4.src_ip &&
			    a->ip.v4.dst_port == b->ip.v4.dst_port &&
			    a->ip.v4.src_port == b->ip.v4.src_port)
				return true;
		} else if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_OTHER) {
			if (a->ip.v4.dst_ip == b->ip.v4.dst_ip &&
			    a->ip.v4.src_ip == b->ip.v4.src_ip &&
			    a->ip.v4.l4_header == b->ip.v4.l4_header &&
			    a->ip.v4.proto == b->ip.v4.proto &&
			    a->ip.v4.ip_ver == b->ip.v4.ip_ver &&
			    a->ip.v4.tos == b->ip.v4.tos)
				return true;
		}
	} else {
		if (flow_type == ICE_FLTR_PTYPE_NONF_IPV6_UDP ||
		    flow_type == ICE_FLTR_PTYPE_NONF_IPV6_TCP ||
		    flow_type == ICE_FLTR_PTYPE_NONF_IPV6_SCTP) {
			if (a->ip.v6.dst_port == b->ip.v6.dst_port &&
			    a->ip.v6.src_port == b->ip.v6.src_port &&
			    !ice_cmp_ipv6_addr(a->ip.v6.dst_ip,
					       b->ip.v6.dst_ip) &&
			    !ice_cmp_ipv6_addr(a->ip.v6.src_ip,
					       b->ip.v6.src_ip))
				return true;
		} else if (flow_type == ICE_FLTR_PTYPE_NONF_IPV6_OTHER) {
			if (a->ip.v6.dst_port == b->ip.v6.dst_port &&
			    a->ip.v6.src_port == b->ip.v6.src_port)
				return true;
		}
	}

	return false;
}

/**
 * ice_fdir_is_dup_fltr - test if filter is already in list for PF
 * @hw: hardware data structure
 * @input: Flow Director filter data structure
 *
 * Returns true if the filter is found in the list
 */
bool ice_fdir_is_dup_fltr(struct ice_hw *hw, struct ice_fdir_fltr *input)
{
	enum ice_fltr_ptype flow_type;
	struct ice_fdir_fltr *rule;
	bool ret = false;

	rule = NULL;

	LIST_FOR_EACH_ENTRY(rule, &hw->fdir_list_head, ice_fdir_fltr,
			    fltr_node) {
		if (rule->flow_type == input->flow_type) {
			flow_type = input->flow_type;
			if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_TCP ||
			    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP ||
			    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_SCTP ||
			    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_OTHER)
				ret = ice_fdir_comp_rules(rule, input, false);
			else
				ret = ice_fdir_comp_rules(rule, input, true);
			if (ret) {
				if (rule->fltr_id == input->fltr_id &&
				    rule->q_index != input->q_index)
					ret = false;
				else
					break;
			}
		}
	}

	return ret;
}

/**
 * ice_clear_vsi_fd_table - admin command to clear FD table for a VSI
 * @hw: hardware data structure
 * @vsi_num: vsi_num (HW VSI num)
 *
 * Clears FD table entries by issuing admin command (direct, 0x0B06)
 * Must to pass valid vsi_num as returned by "AddVSI".
 */
enum ice_status ice_clear_vsi_fd_table(struct ice_hw *hw, u16 vsi_num)
{
	struct ice_aqc_clear_fd_table *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.clear_fd_table;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_fd_table);
	cmd->clear_type = CL_FD_VM_VF_TYPE_VSI_IDX;

	cmd->vsi_index = CPU_TO_LE16(vsi_num);
	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_clear_pf_fd_table - admin command to clear FD table for PF
 * @hw: hardware data structure
 *
 * Clears FD table entries for a PF by issuing admin command (direct, 0x0B06)
 */
enum ice_status ice_clear_pf_fd_table(struct ice_hw *hw)
{
	struct ice_aqc_clear_fd_table *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.clear_fd_table;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_fd_table);
	cmd->clear_type = CL_FD_VM_VF_TYPE_PF_IDX;
	/* vsi_index must be 0 to clear FD table for a PF */
	cmd->vsi_index = CPU_TO_LE16(0);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}
