/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Cisco Systems, Inc.
 */

#ifndef TREX_AF_PACKET_RX_H
#define TREX_AF_PACKET_RX_H

#include <linux/if_packet.h>
#include <stdbool.h>
#include <stdint.h>

/* Linux reflects locally emitted frames to SOCK_RAW listeners as
 * PACKET_OUTGOING.  Such a reflection is not physical ingress. */
static inline bool
trex_af_packet_rx_accept(uint8_t packet_type)
{
	switch (packet_type) {
	case PACKET_HOST:
	case PACKET_BROADCAST:
	case PACKET_MULTICAST:
	case PACKET_OTHERHOST:
		return true;
	default:
		/* PACKET_OUTGOING, PACKET_LOOPBACK, and any future/internal
		 * classification are not proven physical ingress. */
		return false;
	}
}

struct trex_af_packet_rx_batch {
	uint16_t packets;
	unsigned long bytes;
};

static inline uint16_t
trex_af_packet_rx_output_slot(const struct trex_af_packet_rx_batch *batch)
{
	return batch->packets;
}

static inline void
trex_af_packet_rx_account_accepted(struct trex_af_packet_rx_batch *batch,
				   uint32_t frame_length)
{
	batch->packets++;
	batch->bytes += frame_length;
}

#endif /* TREX_AF_PACKET_RX_H */
