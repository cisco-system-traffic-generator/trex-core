/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_H_
#define RTE_PMD_MLX5_COMMON_H_

#include <stdio.h>

#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>

#include "mlx5_prm.h"


/*
 * Compilation workaround for PPC64 when AltiVec is fully enabled, e.g. std=c11.
 * Otherwise there would be a type conflict between stdbool and altivec.
 */
#if defined(__PPC64__) && !defined(__APPLE_ALTIVEC__)
#undef bool
/* redefine as in stdbool.h */
#define bool _Bool
#endif

/* Bit-field manipulation. */
#define BITFIELD_DECLARE(bf, type, size) \
	type bf[(((size_t)(size) / (sizeof(type) * CHAR_BIT)) + \
		!!((size_t)(size) % (sizeof(type) * CHAR_BIT)))]
#define BITFIELD_DEFINE(bf, type, size) \
	BITFIELD_DECLARE((bf), type, (size)) = { 0 }
#define BITFIELD_SET(bf, b) \
	(void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] |= \
		((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT))))
#define BITFIELD_RESET(bf, b) \
	(void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &= \
		~((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT))))
#define BITFIELD_ISSET(bf, b) \
	!!(((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] & \
		((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))

/*
 * Helper macros to work around __VA_ARGS__ limitations in a C99 compliant
 * manner.
 */
#define PMD_DRV_LOG_STRIP(a, b) a
#define PMD_DRV_LOG_OPAREN (
#define PMD_DRV_LOG_CPAREN )
#define PMD_DRV_LOG_COMMA ,

/* Return the file name part of a path. */
static inline const char *
pmd_drv_log_basename(const char *s)
{
	const char *n = s;

	while (*n)
		if (*(n++) == '/')
			s = n;
	return s;
}

#define PMD_DRV_LOG___(level, type, name, ...) \
	rte_log(RTE_LOG_ ## level, \
		type, \
		RTE_FMT(name ": " \
			RTE_FMT_HEAD(__VA_ARGS__,), \
		RTE_FMT_TAIL(__VA_ARGS__,)))

/*
 * When debugging is enabled (MLX5_DEBUG not defined), file, line and function
 * information replace the driver name (MLX5_DRIVER_NAME) in log messages.
 */
#ifdef RTE_LIBRTE_MLX5_DEBUG

#define PMD_DRV_LOG__(level, type, name, ...) \
	PMD_DRV_LOG___(level, type, name, "%s:%u: %s(): " __VA_ARGS__)
#define PMD_DRV_LOG_(level, type, name, s, ...) \
	PMD_DRV_LOG__(level, type, name,\
		s "\n" PMD_DRV_LOG_COMMA \
		pmd_drv_log_basename(__FILE__) PMD_DRV_LOG_COMMA \
		__LINE__ PMD_DRV_LOG_COMMA \
		__func__, \
		__VA_ARGS__)

#else /* RTE_LIBRTE_MLX5_DEBUG */
#define PMD_DRV_LOG__(level, type, name, ...) \
	PMD_DRV_LOG___(level, type, name, __VA_ARGS__)
#define PMD_DRV_LOG_(level, type, name, s, ...) \
	PMD_DRV_LOG__(level, type, name, s "\n", __VA_ARGS__)

#endif /* RTE_LIBRTE_MLX5_DEBUG */

/* claim_zero() does not perform any check when debugging is disabled. */
#ifdef RTE_LIBRTE_MLX5_DEBUG

#define DEBUG(...) DRV_LOG(DEBUG, __VA_ARGS__)
#define MLX5_ASSERT(exp) RTE_VERIFY(exp)
#define claim_zero(...) MLX5_ASSERT((__VA_ARGS__) == 0)
#define claim_nonzero(...) MLX5_ASSERT((__VA_ARGS__) != 0)

#else /* RTE_LIBRTE_MLX5_DEBUG */

#define DEBUG(...) (void)0
#define MLX5_ASSERT(exp) RTE_ASSERT(exp)
#define claim_zero(...) (__VA_ARGS__)
#define claim_nonzero(...) (__VA_ARGS__)

#endif /* RTE_LIBRTE_MLX5_DEBUG */

/* Allocate a buffer on the stack and fill it with a printf format string. */
#define MKSTR(name, ...) \
	int mkstr_size_##name = snprintf(NULL, 0, "" __VA_ARGS__); \
	char name[mkstr_size_##name + 1]; \
	\
	snprintf(name, sizeof(name), "" __VA_ARGS__)

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX4 = 0x1013,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4VF = 0x1014,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4LX = 0x1015,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF = 0x1016,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5 = 0x1017,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5VF = 0x1018,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5EX = 0x1019,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF = 0x101a,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5BF = 0xa2d2,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF = 0xa2d3,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6 = 0x101b,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6VF = 0x101c,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6DX = 0x101d,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6DXVF = 0x101e,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF = 0xa2d6,
};

/* Maximum number of simultaneous unicast MAC addresses. */
#define MLX5_MAX_UC_MAC_ADDRESSES 128
/* Maximum number of simultaneous Multicast MAC addresses. */
#define MLX5_MAX_MC_MAC_ADDRESSES 128
/* Maximum number of simultaneous MAC addresses. */
#define MLX5_MAX_MAC_ADDRESSES \
	(MLX5_MAX_UC_MAC_ADDRESSES + MLX5_MAX_MC_MAC_ADDRESSES)

/* Recognized Infiniband device physical port name types. */
enum mlx5_nl_phys_port_name_type {
	MLX5_PHYS_PORT_NAME_TYPE_NOTSET = 0, /* Not set. */
	MLX5_PHYS_PORT_NAME_TYPE_LEGACY, /* before kernel ver < 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UPLINK, /* p0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_PFVF, /* pf0vf0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN, /* Unrecognized. */
};

/** Switch information returned by mlx5_nl_switch_info(). */
struct mlx5_switch_info {
	uint32_t master:1; /**< Master device. */
	uint32_t representor:1; /**< Representor device. */
	enum mlx5_nl_phys_port_name_type name_type; /** < Port name type. */
	int32_t pf_num; /**< PF number (valid for pfxvfx format only). */
	int32_t port_name; /**< Representor port name. */
	uint64_t switch_id; /**< Switch identifier. */
};

/* CQE status. */
enum mlx5_cqe_status {
	MLX5_CQE_STATUS_SW_OWN = -1,
	MLX5_CQE_STATUS_HW_OWN = -2,
	MLX5_CQE_STATUS_ERR = -3,
};

/**
 * Check whether CQE is valid.
 *
 * @param cqe
 *   Pointer to CQE.
 * @param cqes_n
 *   Size of completion queue.
 * @param ci
 *   Consumer index.
 *
 * @return
 *   The CQE status.
 */
static __rte_always_inline enum mlx5_cqe_status
check_cqe(volatile struct mlx5_cqe *cqe, const uint16_t cqes_n,
	  const uint16_t ci)
{
	const uint16_t idx = ci & cqes_n;
	const uint8_t op_own = cqe->op_own;
	const uint8_t op_owner = MLX5_CQE_OWNER(op_own);
	const uint8_t op_code = MLX5_CQE_OPCODE(op_own);

	if (unlikely((op_owner != (!!(idx))) || (op_code == MLX5_CQE_INVALID)))
		return MLX5_CQE_STATUS_HW_OWN;
	rte_cio_rmb();
	if (unlikely(op_code == MLX5_CQE_RESP_ERR ||
		     op_code == MLX5_CQE_REQ_ERR))
		return MLX5_CQE_STATUS_ERR;
	return MLX5_CQE_STATUS_SW_OWN;
}

int mlx5_dev_to_pci_addr(const char *dev_path, struct rte_pci_addr *pci_addr);

#define MLX5_CLASS_ARG_NAME "class"

enum mlx5_class {
	MLX5_CLASS_NET,
	MLX5_CLASS_VDPA,
	MLX5_CLASS_INVALID,
};

enum mlx5_class mlx5_class_get(struct rte_devargs *devargs);
void mlx5_translate_port_name(const char *port_name_in,
			      struct mlx5_switch_info *port_info_out);

#endif /* RTE_PMD_MLX5_COMMON_H_ */
