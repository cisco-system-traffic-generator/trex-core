/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_UTILS_H_
#define RTE_PMD_MLX5_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#include <mlx5_common.h>

#include "mlx5_defs.h"


/*
 * Compilation workaround for PPC64 when AltiVec is fully enabled, e.g. std=c11.
 * Otherwise there would be a type conflict between stdbool and altivec.
 */
#if defined(__PPC64__) && !defined(__APPLE_ALTIVEC__)
#undef bool
/* redefine as in stdbool.h */
#define bool _Bool
#endif

/* Convert a bit number to the corresponding 64-bit mask */
#define MLX5_BITSHIFT(v) (UINT64_C(1) << (v))

/* Save and restore errno around argument evaluation. */
#define ERRNO_SAFE(x) ((errno = (int []){ errno, ((x), 0) }[0]))

extern int mlx5_logtype;

/* Generic printf()-like logging macro with automatic line feed. */
#define DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, mlx5_logtype, MLX5_DRIVER_NAME, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

#define INFO(...) DRV_LOG(INFO, __VA_ARGS__)
#define WARN(...) DRV_LOG(WARNING, __VA_ARGS__)
#define ERROR(...) DRV_LOG(ERR, __VA_ARGS__)

/* Convenience macros for accessing mbuf fields. */
#define NEXT(m) ((m)->next)
#define DATA_LEN(m) ((m)->data_len)
#define PKT_LEN(m) ((m)->pkt_len)
#define DATA_OFF(m) ((m)->data_off)
#define SET_DATA_OFF(m, o) ((m)->data_off = (o))
#define NB_SEGS(m) ((m)->nb_segs)
#define PORT(m) ((m)->port)

/* Transpose flags. Useful to convert IBV to DPDK flags. */
#define TRANSPOSE(val, from, to) \
	(((from) >= (to)) ? \
	 (((val) & (from)) / ((from) / (to))) : \
	 (((val) & (from)) * ((to) / (from))))

/**
 * Return logarithm of the nearest power of two above input value.
 *
 * @param v
 *   Input value.
 *
 * @return
 *   Logarithm of the nearest power of two above input value.
 */
static inline unsigned int
log2above(unsigned int v)
{
	unsigned int l;
	unsigned int r;

	for (l = 0, r = 0; (v >> 1); ++l, v >>= 1)
		r |= (v & 1);
	return l + r;
}

/** Maximum size of string for naming the hlist table. */
#define MLX5_HLIST_NAMESIZE			32

/**
 * Structure of the entry in the hash list, user should define its own struct
 * that contains this in order to store the data. The 'key' is 64-bits right
 * now and its user's responsibility to guarantee there is no collision.
 */
struct mlx5_hlist_entry {
	LIST_ENTRY(mlx5_hlist_entry) next; /* entry pointers in the list. */
	uint64_t key; /* user defined 'key', could be the hash signature. */
};

/** Structure for hash head. */
LIST_HEAD(mlx5_hlist_head, mlx5_hlist_entry);

/** Type of function that is used to handle the data before freeing. */
typedef void (*mlx5_hlist_destroy_callback_fn)(void *p, void *ctx);

/** hash list table structure */
struct mlx5_hlist {
	char name[MLX5_HLIST_NAMESIZE]; /**< Name of the hash list. */
	/**< number of heads, need to be power of 2. */
	uint32_t table_sz;
	/**< mask to get the index of the list heads. */
	uint32_t mask;
	struct mlx5_hlist_head heads[];	/**< list head arrays. */
};

/**
 * Create a hash list table, the user can specify the list heads array size
 * of the table, now the size should be a power of 2 in order to get better
 * distribution for the entries. Each entry is a part of the whole data element
 * and the caller should be responsible for the data element's allocation and
 * cleanup / free. Key of each entry will be calculated with CRC in order to
 * generate a little fairer distribution.
 *
 * @param name
 *   Name of the hash list(optional).
 * @param size
 *   Heads array size of the hash list.
 *
 * @return
 *   Pointer of the hash list table created, NULL on failure.
 */
struct mlx5_hlist *mlx5_hlist_create(const char *name, uint32_t size);

/**
 * Search an entry matching the key.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param key
 *   Key for the searching entry.
 *
 * @return
 *   Pointer of the hlist entry if found, NULL otherwise.
 */
struct mlx5_hlist_entry *mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key);

/**
 * Insert an entry to the hash list table, the entry is only part of whole data
 * element and a 64B key is used for matching. User should construct the key or
 * give a calculated hash signature and guarantee there is no collision.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param entry
 *   Entry to be inserted into the hash list table.
 *
 * @return
 *   - zero for success.
 *   - -EEXIST if the entry is already inserted.
 */
int mlx5_hlist_insert(struct mlx5_hlist *h, struct mlx5_hlist_entry *entry);

/**
 * Remove an entry from the hash list table. User should guarantee the validity
 * of the entry.
 *
 * @param h
 *   Pointer to the hast list table. (not used)
 * @param entry
 *   Entry to be removed from the hash list table.
 */
void mlx5_hlist_remove(struct mlx5_hlist *h __rte_unused,
		       struct mlx5_hlist_entry *entry);

/**
 * Destroy the hash list table, all the entries already inserted into the lists
 * will be handled by the callback function provided by the user (including
 * free if needed) before the table is freed.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param cb
 *   Callback function for each inserted entry when destroying the hash list.
 * @param ctx
 *   Common context parameter used by callback function for each entry.
 */
void mlx5_hlist_destroy(struct mlx5_hlist *h,
			mlx5_hlist_destroy_callback_fn cb, void *ctx);

#endif /* RTE_PMD_MLX5_UTILS_H_ */
