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

#ifndef _RTE_RWLOCK_H_
#define _RTE_RWLOCK_H_

/**
 * @file
 *
 * RTE Read-Write Locks
 *
 * This file defines an API for read-write locks. The lock is used to
 * protect data that allows multiple readers in parallel, but only
 * one writer. All readers are blocked until the writer is finished
 * writing.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_atomic.h>
#include <rte_pause.h>

/**
 * The rte_rwlock_t type.
 *
 * cnt is -1 when write lock is held, and > 0 when read locks are held.
 */
typedef struct {
	volatile int32_t cnt; /**< -1 when W lock held, > 0 when R locks held. */
} rte_rwlock_t;

/**
 * A static rwlock initializer.
 */
#define RTE_RWLOCK_INITIALIZER { 0 }

/**
 * Initialize the rwlock to an unlocked state.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_init(rte_rwlock_t *rwl)
{
	rwl->cnt = 0;
}

/**
 * Take a read lock. Loop until the lock is held.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_read_lock(rte_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = rwl->cnt;
		/* write lock is held */
		if (x < 0) {
			rte_pause();
			continue;
		}
		success = rte_atomic32_cmpset((volatile uint32_t *)&rwl->cnt,
					      x, x + 1);
	}
}

/**
 * Release a read lock.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_read_unlock(rte_rwlock_t *rwl)
{
	rte_atomic32_dec((rte_atomic32_t *)(intptr_t)&rwl->cnt);
}

/**
 * Take a write lock. Loop until the lock is held.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_lock(rte_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = rwl->cnt;
		/* a lock is held */
		if (x != 0) {
			rte_pause();
			continue;
		}
		success = rte_atomic32_cmpset((volatile uint32_t *)&rwl->cnt,
					      0, -1);
	}
}

/**
 * Release a write lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_unlock(rte_rwlock_t *rwl)
{
	rte_atomic32_inc((rte_atomic32_t *)(intptr_t)&rwl->cnt);
}

/**
 * Try to execute critical section in a hardware memory transaction, if it
 * fails or not available take a read lock
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_read_lock_tm(rte_rwlock_t *rwl);

/**
 * Commit hardware memory transaction or release the read lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_read_unlock_tm(rte_rwlock_t *rwl);

/**
 * Try to execute critical section in a hardware memory transaction, if it
 * fails or not available take a write lock
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_lock_tm(rte_rwlock_t *rwl);

/**
 * Commit hardware memory transaction or release the write lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_unlock_tm(rte_rwlock_t *rwl);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RWLOCK_H_ */
