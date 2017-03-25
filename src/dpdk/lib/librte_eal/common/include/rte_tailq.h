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

#ifndef _RTE_TAILQ_H_
#define _RTE_TAILQ_H_

/**
 * @file
 *  Here defines rte_tailq APIs for only internal use
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>
#include <stdio.h>
#include <rte_debug.h>

/** dummy structure type used by the rte_tailq APIs */
struct rte_tailq_entry {
	TAILQ_ENTRY(rte_tailq_entry) next; /**< Pointer entries for a tailq list */
	void *data; /**< Pointer to the data referenced by this tailq entry */
};
/** dummy */
TAILQ_HEAD(rte_tailq_entry_head, rte_tailq_entry);

#define RTE_TAILQ_NAMESIZE 32

/**
 * The structure defining a tailq header entry for storing
 * in the rte_config structure in shared memory. Each tailq
 * is identified by name.
 * Any library storing a set of objects e.g. rings, mempools, hash-tables,
 * is recommended to use an entry here, so as to make it easy for
 * a multi-process app to find already-created elements in shared memory.
 */
struct rte_tailq_head {
	struct rte_tailq_entry_head tailq_head; /**< NOTE: must be first element */
	char name[RTE_TAILQ_NAMESIZE];
};

struct rte_tailq_elem {
	/**
	 * Reference to head in shared mem, updated at init time by
	 * rte_eal_tailqs_init()
	 */
	struct rte_tailq_head *head;
	TAILQ_ENTRY(rte_tailq_elem) next;
	const char name[RTE_TAILQ_NAMESIZE];
};

/**
 * Return the first tailq entry casted to the right struct.
 */
#define RTE_TAILQ_CAST(tailq_entry, struct_name) \
	(struct struct_name *)&(tailq_entry)->tailq_head

/**
 * Utility macro to make looking up a tailqueue for a particular struct easier.
 *
 * @param name
 *   The name of tailq
 *
 * @param struct_name
 *   The name of the list type we are using. (Generally this is the same as the
 *   first parameter passed to TAILQ_HEAD macro)
 *
 * @return
 *   The return value from rte_eal_tailq_lookup, typecast to the appropriate
 *   structure pointer type.
 *   NULL on error, since the tailq_head is the first
 *   element in the rte_tailq_head structure.
 */
#define RTE_TAILQ_LOOKUP(name, struct_name) \
	RTE_TAILQ_CAST(rte_eal_tailq_lookup(name), struct_name)

/**
 * Dump tail queues to a file.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_dump_tailq(FILE *f);

/**
 * Lookup for a tail queue.
 *
 * Get a pointer to a tail queue header of a tail
 * queue identified by the name given as an argument.
 * Note: this function is not multi-thread safe, and should only be called from
 * a single thread at a time
 *
 * @param name
 *   The name of the queue.
 * @return
 *   A pointer to the tail queue head structure.
 */
struct rte_tailq_head *rte_eal_tailq_lookup(const char *name);

/**
 * Register a tail queue.
 *
 * Register a tail queue from shared memory.
 * This function is mainly used by EAL_REGISTER_TAILQ macro which is used to
 * register tailq from the different dpdk libraries. Since this macro is a
 * constructor, the function has no access to dpdk shared memory, so the
 * registered tailq can not be used before call to rte_eal_init() which calls
 * rte_eal_tailqs_init().
 *
 * @param t
 *   The tailq element which contains the name of the tailq you want to
 *   create (/retrieve when in secondary process).
 * @return
 *   0 on success or -1 in case of an error.
 */
int rte_eal_tailq_register(struct rte_tailq_elem *t);

#define EAL_REGISTER_TAILQ(t) \
RTE_INIT(tailqinitfn_ ##t); \
static void tailqinitfn_ ##t(void) \
{ \
	if (rte_eal_tailq_register(&t) < 0) \
		rte_panic("Cannot initialize tailq: %s\n", t.name); \
}

/* This macro permits both remove and free var within the loop safely.*/
#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)		\
	for ((var) = TAILQ_FIRST((head));			\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);	\
	    (var) = (tvar))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TAILQ_H_ */
