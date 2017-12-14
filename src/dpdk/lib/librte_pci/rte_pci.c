/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright 2013-2014 6WIND S.A.
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

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>

#include "rte_pci.h"

static inline const char *
get_u8_pciaddr_field(const char *in, void *_u8, char dlm)
{
	unsigned long val;
	uint8_t *u8 = _u8;
	char *end;

	errno = 0;
	val = strtoul(in, &end, 16);
	if (errno != 0 || end[0] != dlm || val > UINT8_MAX) {
		errno = errno ? errno : EINVAL;
		return NULL;
	}
	*u8 = (uint8_t)val;
	return end + 1;
}

static int
pci_bdf_parse(const char *input, struct rte_pci_addr *dev_addr)
{
	const char *in = input;

	dev_addr->domain = 0;
	in = get_u8_pciaddr_field(in, &dev_addr->bus, ':');
	if (in == NULL)
		return -EINVAL;
	in = get_u8_pciaddr_field(in, &dev_addr->devid, '.');
	if (in == NULL)
		return -EINVAL;
	in = get_u8_pciaddr_field(in, &dev_addr->function, '\0');
	if (in == NULL)
		return -EINVAL;
	return 0;
}

static int
pci_dbdf_parse(const char *input, struct rte_pci_addr *dev_addr)
{
	const char *in = input;
	unsigned long val;
	char *end;

	errno = 0;
	val = strtoul(in, &end, 16);
	if (errno != 0 || end[0] != ':' || val > UINT16_MAX)
		return -EINVAL;
	dev_addr->domain = (uint16_t)val;
	in = end + 1;
	in = get_u8_pciaddr_field(in, &dev_addr->bus, ':');
	if (in == NULL)
		return -EINVAL;
	in = get_u8_pciaddr_field(in, &dev_addr->devid, '.');
	if (in == NULL)
		return -EINVAL;
	in = get_u8_pciaddr_field(in, &dev_addr->function, '\0');
	if (in == NULL)
		return -EINVAL;
	return 0;
}

int
eal_parse_pci_BDF(const char *input, struct rte_pci_addr *dev_addr)
{
	return pci_bdf_parse(input, dev_addr);
}

int
eal_parse_pci_DomBDF(const char *input, struct rte_pci_addr *dev_addr)
{
	return pci_dbdf_parse(input, dev_addr);
}

void
rte_pci_device_name(const struct rte_pci_addr *addr,
		char *output, size_t size)
{
	RTE_VERIFY(size >= PCI_PRI_STR_SIZE);
	RTE_VERIFY(snprintf(output, size, PCI_PRI_FMT,
			    addr->domain, addr->bus,
			    addr->devid, addr->function) >= 0);
}

int
rte_eal_compare_pci_addr(const struct rte_pci_addr *addr,
			 const struct rte_pci_addr *addr2)
{
	return rte_pci_addr_cmp(addr, addr2);
}

int
rte_pci_addr_cmp(const struct rte_pci_addr *addr,
	     const struct rte_pci_addr *addr2)
{
	uint64_t dev_addr, dev_addr2;

	if ((addr == NULL) || (addr2 == NULL))
		return -1;

	dev_addr = ((uint64_t)addr->domain << 24) |
		(addr->bus << 16) | (addr->devid << 8) | addr->function;
	dev_addr2 = ((uint64_t)addr2->domain << 24) |
		(addr2->bus << 16) | (addr2->devid << 8) | addr2->function;

	if (dev_addr > dev_addr2)
		return 1;
	else if (dev_addr < dev_addr2)
		return -1;
	else
		return 0;
}

int
rte_pci_addr_parse(const char *str, struct rte_pci_addr *addr)
{
	if (pci_bdf_parse(str, addr) == 0 ||
	    pci_dbdf_parse(str, addr) == 0)
		return 0;
	return -1;
}


/* map a particular resource from a file */
void *
pci_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		 int additional_flags)
{
	void *mapaddr;

	/* Map the PCI memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap(%d, %p, 0x%lx, 0x%lx): %s (%p)\n",
			__func__, fd, requested_addr,
			(unsigned long)size, (unsigned long)offset,
			strerror(errno), mapaddr);
	} else
		RTE_LOG(DEBUG, EAL, "  PCI memory mapped at %p\n", mapaddr);

	return mapaddr;
}

/* unmap a particular resource */
void
pci_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (munmap(requested_addr, size)) {
		RTE_LOG(ERR, EAL, "%s(): cannot munmap(%p, 0x%lx): %s\n",
			__func__, requested_addr, (unsigned long)size,
			strerror(errno));
	} else
		RTE_LOG(DEBUG, EAL, "  PCI memory unmapped at %p\n",
				requested_addr);
}
