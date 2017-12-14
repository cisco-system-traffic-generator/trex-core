/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef EAL_VFIO_H_
#define EAL_VFIO_H_

/*
 * determine if VFIO is present on the system
 */
#if !defined(VFIO_PRESENT) && defined(RTE_EAL_VFIO)
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#define VFIO_PRESENT
#else
#pragma message("VFIO configured but not supported by this kernel, disabling.")
#endif /* kernel version >= 3.6.0 */
#endif /* RTE_EAL_VFIO */

#ifdef VFIO_PRESENT

#include <linux/vfio.h>

#define RTE_VFIO_TYPE1 VFIO_TYPE1_IOMMU

#ifndef VFIO_SPAPR_TCE_v2_IOMMU
#define RTE_VFIO_SPAPR 7
#define VFIO_IOMMU_SPAPR_REGISTER_MEMORY _IO(VFIO_TYPE, VFIO_BASE + 17)
#define VFIO_IOMMU_SPAPR_TCE_CREATE _IO(VFIO_TYPE, VFIO_BASE + 19)
#define VFIO_IOMMU_SPAPR_TCE_REMOVE _IO(VFIO_TYPE, VFIO_BASE + 20)

struct vfio_iommu_spapr_register_memory {
	uint32_t argsz;
	uint32_t flags;
	uint64_t vaddr;
	uint64_t size;
};

struct vfio_iommu_spapr_tce_create {
	uint32_t argsz;
	uint32_t flags;
	/* in */
	uint32_t page_shift;
	uint32_t __resv1;
	uint64_t window_size;
	uint32_t levels;
	uint32_t __resv2;
	/* out */
	uint64_t start_addr;
};

struct vfio_iommu_spapr_tce_remove {
	uint32_t argsz;
	uint32_t flags;
	/* in */
	uint64_t start_addr;
};

struct vfio_iommu_spapr_tce_ddw_info {
	uint64_t pgsizes;
	uint32_t max_dynamic_windows_supported;
	uint32_t levels;
};

/* SPAPR_v2 is not present, but SPAPR might be */
#ifndef VFIO_SPAPR_TCE_IOMMU
#define VFIO_IOMMU_SPAPR_TCE_GET_INFO _IO(VFIO_TYPE, VFIO_BASE + 12)

struct vfio_iommu_spapr_tce_info {
	uint32_t argsz;
	uint32_t flags;
	uint32_t dma32_window_start;
	uint32_t dma32_window_size;
	struct vfio_iommu_spapr_tce_ddw_info ddw;
};
#endif /* VFIO_SPAPR_TCE_IOMMU */

#else /* VFIO_SPAPR_TCE_v2_IOMMU */
#define RTE_VFIO_SPAPR VFIO_SPAPR_TCE_v2_IOMMU
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define RTE_VFIO_NOIOMMU 8
#else
#define RTE_VFIO_NOIOMMU VFIO_NOIOMMU_IOMMU
#endif

#define VFIO_MAX_GROUPS 64

/*
 * Function prototypes for VFIO multiprocess sync functions
 */
int vfio_mp_sync_send_request(int socket, int req);
int vfio_mp_sync_receive_request(int socket);
int vfio_mp_sync_send_fd(int socket, int fd);
int vfio_mp_sync_receive_fd(int socket);
int vfio_mp_sync_connect_to_primary(void);

/*
 * we don't need to store device fd's anywhere since they can be obtained from
 * the group fd via an ioctl() call.
 */
struct vfio_group {
	int group_no;
	int fd;
	int devices;
};

struct vfio_config {
	int vfio_enabled;
	int vfio_container_fd;
	int vfio_active_groups;
	struct vfio_group vfio_groups[VFIO_MAX_GROUPS];
};

/* DMA mapping function prototype.
 * Takes VFIO container fd as a parameter.
 * Returns 0 on success, -1 on error.
 * */
typedef int (*vfio_dma_func_t)(int);

struct vfio_iommu_type {
	int type_id;
	const char *name;
	vfio_dma_func_t dma_map_func;
};

/* pick IOMMU type. returns a pointer to vfio_iommu_type or NULL for error */
const struct vfio_iommu_type *
vfio_set_iommu_type(int vfio_container_fd);

/* check if we have any supported extensions */
int
vfio_has_supported_extensions(int vfio_container_fd);

/* open container fd or get an existing one */
int
vfio_get_container_fd(void);

/* parse IOMMU group number for a device
 * returns 1 on success, -1 for errors, 0 for non-existent group
 */
int
vfio_get_group_no(const char *sysfs_base,
		const char *dev_addr, int *iommu_group_no);

/* open group fd or get an existing one */
int
vfio_get_group_fd(int iommu_group_no);

/* remove group fd from internal VFIO group fd array */
int
clear_group(int vfio_group_fd);

int vfio_mp_sync_setup(void);

#define SOCKET_REQ_CONTAINER 0x100
#define SOCKET_REQ_GROUP 0x200
#define SOCKET_CLR_GROUP 0x300
#define SOCKET_OK 0x0
#define SOCKET_NO_FD 0x1
#define SOCKET_ERR 0xFF

#endif /* VFIO_PRESENT */

#endif /* EAL_VFIO_H_ */
