/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_SPINLOCK_H_
#define _RTE_SPINLOCK_H_

#include <assert.h>

/* a stub for spin locks, API was taken fro m DPDK  */

/**
 * The rte_spinlock_t type.
 */
typedef struct {
	volatile int locked; /**< lock status 0 = unlocked, 1 = locked */
} rte_spinlock_t;

#define RTE_SPINLOCK_INITIALIZER { 0 }

static inline void
rte_spinlock_init(rte_spinlock_t *sl)
{
	sl->locked = 0;
}

static inline void
rte_spinlock_lock(rte_spinlock_t *sl){
    sl->locked = 1;
}


static inline void
rte_spinlock_unlock (rte_spinlock_t *sl){
    sl->locked = 0;
}
static inline int
rte_spinlock_trylock (rte_spinlock_t *sl){
    assert(sl->locked ==0);
    rte_spinlock_lock(sl);
    return (1);
}


static inline int rte_spinlock_is_locked (rte_spinlock_t *sl){
	return sl->locked;
}


#endif /* _RTE_SPINLOCK_H_ */
