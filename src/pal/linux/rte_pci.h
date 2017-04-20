/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
/*   BSD LICENSE
 *
 *   Copyright 2013-2014 6WIND S.A.
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

#ifndef _RTE_PCI_H_
#define _RTE_PCI_H_

/**
 * @file
 *
 * RTE PCI Interface
 */

#ifdef __cplusplus
extern "C" {
#endif


#define	RTE_VERIFY(exp)	assert(exp)

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/** Formatting string for PCI device identifier: Ex: 0000:00:01.0 */
#define PCI_PRI_FMT "%.4" PRIx16 ":%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8
#define PCI_PRI_STR_SIZE sizeof("XXXX:XX:XX.X")

/** Short formatting string, without domain, for PCI device: Ex: 00:01.0 */
#define PCI_SHORT_PRI_FMT "%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8

/** Nb. of values in PCI device identifier format string. */
#define PCI_FMT_NVAL 4

/** Nb. of values in PCI resource format. */
#define PCI_RESOURCE_FMT_NVAL 3

/** Maximum number of PCI resources. */
#define PCI_MAX_RESOURCE 6

/**
 * A structure describing an ID for a PCI driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_pci_id {
	uint32_t class_id;            /**< Class ID (class, subclass, pi) or RTE_CLASS_ANY_ID. */
	uint16_t vendor_id;           /**< Vendor ID or PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or PCI_ANY_ID. */
	uint16_t subsystem_vendor_id; /**< Subsystem vendor ID or PCI_ANY_ID. */
	uint16_t subsystem_device_id; /**< Subsystem device ID or PCI_ANY_ID. */
};

/**
 * A structure describing the location of a PCI device.
 */
struct rte_pci_addr {
	uint16_t domain;                /**< Device domain */
	uint8_t bus;                    /**< Device bus */
	uint8_t devid;                  /**< Device ID */
	uint8_t function;               /**< Device function. */
};

struct rte_devargs;

/**< Internal use only - Macro used by pci addr parsing functions **/
#define GET_PCIADDR_FIELD(in, fd, lim, dlm)                   \
do {                                                               \
	unsigned long val;                                      \
	char *end;                                              \
	errno = 0;                                              \
	val = strtoul((in), &end, 16);                          \
	if (errno != 0 || end[0] != (dlm) || val > (lim))       \
		return -EINVAL;                                 \
	(fd) = (typeof (fd))val;                                \
	(in) = end + 1;                                         \
} while(0)

/**
 * Utility function to produce a PCI Bus-Device-Function value
 * given a string representation. Assumes that the BDF is provided without
 * a domain prefix (i.e. domain returned is always 0)
 *
 * @param input
 *	The input string to be parsed. Should have the format XX:XX.X
 * @param dev_addr
 *	The PCI Bus-Device-Function address to be returned. Domain will always be
 *	returned as 0
 * @return
 *  0 on success, negative on error.
 */
static inline int
eal_parse_pci_BDF(const char *input, struct rte_pci_addr *dev_addr)
{
	dev_addr->domain = 0;
	GET_PCIADDR_FIELD(input, dev_addr->bus, UINT8_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->devid, UINT8_MAX, '.');
	GET_PCIADDR_FIELD(input, dev_addr->function, UINT8_MAX, 0);
	return 0;
}

/**
 * Utility function to produce a PCI Bus-Device-Function value
 * given a string representation. Assumes that the BDF is provided including
 * a domain prefix.
 *
 * @param input
 *	The input string to be parsed. Should have the format XXXX:XX:XX.X
 * @param dev_addr
 *	The PCI Bus-Device-Function address to be returned
 * @return
 *  0 on success, negative on error.
 */
static inline int
eal_parse_pci_DomBDF(const char *input, struct rte_pci_addr *dev_addr)
{
	GET_PCIADDR_FIELD(input, dev_addr->domain, UINT16_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->bus, UINT8_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->devid, UINT8_MAX, '.');
	GET_PCIADDR_FIELD(input, dev_addr->function, UINT8_MAX, 0);
	return 0;
}
#undef GET_PCIADDR_FIELD

/**
 * Utility function to write a pci device name, this device name can later be
 * used to retrieve the corresponding rte_pci_addr using eal_parse_pci_*
 * BDF helpers.
 *
 * @param addr
 *	The PCI Bus-Device-Function address
 * @param output
 *	The output buffer string
 * @param size
 *	The output buffer size
 */
static inline void
rte_eal_pci_device_name(const struct rte_pci_addr *addr,
		    char *output, size_t size)
{
	RTE_VERIFY(size >= PCI_PRI_STR_SIZE);
	snprintf(output, size, PCI_PRI_FMT,
			    addr->domain, addr->bus,
			    addr->devid, addr->function);
}

/* Compare two PCI device addresses. */
/**
 * Utility function to compare two PCI device addresses.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to compare
 * @param addr2
 *	The PCI Bus-Device-Function address to compare
 * @return
 *	0 on equal PCI address.
 *	Positive on addr is greater than addr2.
 *	Negative on addr is less than addr2, or error.
 */
static inline int
rte_eal_compare_pci_addr(const struct rte_pci_addr *addr,
			 const struct rte_pci_addr *addr2)
{
	uint64_t dev_addr, dev_addr2;

	if ((addr == NULL) || (addr2 == NULL))
		return -1;

	dev_addr = (addr->domain << 24) | (addr->bus << 16) |
				(addr->devid << 8) | addr->function;
	dev_addr2 = (addr2->domain << 24) | (addr2->bus << 16) |
				(addr2->devid << 8) | addr2->function;

	if (dev_addr > dev_addr2)
		return 1;
	else if (dev_addr < dev_addr2)
		return -1;
	else
		return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_PCI_H_ */
