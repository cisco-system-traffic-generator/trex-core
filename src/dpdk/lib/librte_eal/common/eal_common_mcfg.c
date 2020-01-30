/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_config.h>
#include <rte_eal_memconfig.h>

void
rte_mcfg_mem_read_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_read_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_write_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_lock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_write_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_unlock(&mcfg->memory_hotplug_lock);
}

bool
rte_mcfg_get_single_file_segments(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	return (bool)mcfg->single_file_segments;
}
