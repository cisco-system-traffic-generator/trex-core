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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_debug.h>
#include <rte_devargs.h>
#include <rte_log.h>

#include "eal_private.h"

/** Global list of device drivers. */
static struct rte_driver_list dev_driver_list =
	TAILQ_HEAD_INITIALIZER(dev_driver_list);
/** Global list of device drivers. */
static struct rte_device_list dev_device_list =
	TAILQ_HEAD_INITIALIZER(dev_device_list);

/* register a driver */
void
rte_eal_driver_register(struct rte_driver *driver)
{
	TAILQ_INSERT_TAIL(&dev_driver_list, driver, next);
}

/* unregister a driver */
void
rte_eal_driver_unregister(struct rte_driver *driver)
{
	TAILQ_REMOVE(&dev_driver_list, driver, next);
}

void rte_eal_device_insert(struct rte_device *dev)
{
	TAILQ_INSERT_TAIL(&dev_device_list, dev, next);
}

void rte_eal_device_remove(struct rte_device *dev)
{
	TAILQ_REMOVE(&dev_device_list, dev, next);
}

int
rte_eal_dev_init(void)
{
	struct rte_devargs *devargs;

	/*
	 * Note that the dev_driver_list is populated here
	 * from calls made to rte_eal_driver_register from constructor functions
	 * embedded into PMD modules via the RTE_PMD_REGISTER_VDEV macro
	 */

	/* call the init function for each virtual device */
	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->type != RTE_DEVTYPE_VIRTUAL)
			continue;

		if (rte_eal_vdev_init(devargs->virt.drv_name,
					devargs->args)) {
			RTE_LOG(ERR, EAL, "failed to initialize %s device\n",
					devargs->virt.drv_name);
			return -1;
		}
	}

	return 0;
}

int rte_eal_dev_attach(const char *name, const char *devargs)
{
	struct rte_pci_addr addr;

	if (name == NULL || devargs == NULL) {
		RTE_LOG(ERR, EAL, "Invalid device or arguments provided\n");
		return -EINVAL;
	}

	if (eal_parse_pci_DomBDF(name, &addr) == 0) {
		if (rte_eal_pci_probe_one(&addr) < 0)
			goto err;

	} else {
		if (rte_eal_vdev_init(name, devargs))
			goto err;
	}

	return 0;

err:
	RTE_LOG(ERR, EAL, "Driver cannot attach the device (%s)\n", name);
	return -EINVAL;
}

int rte_eal_dev_detach(const char *name)
{
	struct rte_pci_addr addr;

	if (name == NULL) {
		RTE_LOG(ERR, EAL, "Invalid device provided.\n");
		return -EINVAL;
	}

	if (eal_parse_pci_DomBDF(name, &addr) == 0) {
		if (rte_eal_pci_detach(&addr) < 0)
			goto err;
	} else {
		if (rte_eal_vdev_uninit(name))
			goto err;
	}
	return 0;

err:
	RTE_LOG(ERR, EAL, "Driver cannot detach the device (%s)\n", name);
	return -EINVAL;
}
