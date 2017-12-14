/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 6WIND S.A. All rights reserved.
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
 *     * Neither the name of 6WIND nor the names of its
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

#ifndef _RTE_VFIO_H_
#define _RTE_VFIO_H_

/*
 * determine if VFIO is present on the system
 */
#if !defined(VFIO_PRESENT) && defined(RTE_EAL_VFIO)
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#define VFIO_PRESENT
#endif /* kernel version >= 3.6.0 */
#endif /* RTE_EAL_VFIO */

#ifdef VFIO_PRESENT

#include <linux/vfio.h>

#define VFIO_DIR "/dev/vfio"
#define VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define VFIO_GROUP_FMT "/dev/vfio/%u"
#define VFIO_NOIOMMU_GROUP_FMT "/dev/vfio/noiommu-%u"
#define VFIO_GET_REGION_ADDR(x) ((uint64_t) x << 40ULL)
#define VFIO_GET_REGION_IDX(x) (x >> 40)
#define VFIO_NOIOMMU_MODE      \
	"/sys/module/vfio/parameters/enable_unsafe_noiommu_mode"

/**
 * Setup vfio_cfg for the device identified by its address.
 * It discovers the configured I/O MMU groups or sets a new one for the device.
 * If a new groups is assigned, the DMA mapping is performed.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param vfio_dev_fd
 *   VFIO fd.
 *
 * @param device_info
 *   Device information.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 *   >1 if the device cannot be managed this way.
 */
int rte_vfio_setup_device(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * Release a device mapped to a VFIO-managed I/O MMU group.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param fd
 *   VFIO fd.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
int rte_vfio_release_device(const char *sysfs_base, const char *dev_addr, int fd);

/**
 * Enable a VFIO-related kmod.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param modname
 *   kernel module name.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
int rte_vfio_enable(const char *modname);

/**
 * Check whether a VFIO-related kmod is enabled.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param modname
 *   kernel module name.
 *
 * @return
 *   !0 if true.
 *   0 otherwise.
 */
int rte_vfio_is_enabled(const char *modname);

/**
 * Whether VFIO NOIOMMU mode is enabled.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @return
 *   !0 if true.
 *   0 otherwise.
 */
int rte_vfio_noiommu_is_enabled(void);

#endif /* VFIO_PRESENT */

#endif /* _RTE_VFIO_H_ */
