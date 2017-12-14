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

#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_common.h>
#include "rte_string_fns.h"
#include "eal_internal_cfg.h"
#include "eal_hugepages.h"
#include "eal_filesystem.h"

static const char sys_dir_path[] = "/sys/kernel/mm/hugepages";

/* this function is only called from eal_hugepage_info_init which itself
 * is only called from a primary process */
static uint32_t
get_num_hugepages(const char *subdir)
{
	char path[PATH_MAX];
	long unsigned resv_pages, num_pages = 0;
	const char *nr_hp_file = "free_hugepages";
	const char *nr_rsvd_file = "resv_hugepages";

	/* first, check how many reserved pages kernel reports */
	snprintf(path, sizeof(path), "%s/%s/%s",
			sys_dir_path, subdir, nr_rsvd_file);
	if (eal_parse_sysfs_value(path, &resv_pages) < 0)
		return 0;

	snprintf(path, sizeof(path), "%s/%s/%s",
			sys_dir_path, subdir, nr_hp_file);
	if (eal_parse_sysfs_value(path, &num_pages) < 0)
		return 0;

	if (num_pages == 0)
		RTE_LOG(WARNING, EAL, "No free hugepages reported in %s\n",
				subdir);

	/* adjust num_pages */
	if (num_pages >= resv_pages)
		num_pages -= resv_pages;
	else if (resv_pages)
		num_pages = 0;

	/* we want to return a uint32_t and more than this looks suspicious
	 * anyway ... */
	if (num_pages > UINT32_MAX)
		num_pages = UINT32_MAX;

	return num_pages;
}

static uint64_t
get_default_hp_size(void)
{
	const char proc_meminfo[] = "/proc/meminfo";
	const char str_hugepagesz[] = "Hugepagesize:";
	unsigned hugepagesz_len = sizeof(str_hugepagesz) - 1;
	char buffer[256];
	unsigned long long size = 0;

	FILE *fd = fopen(proc_meminfo, "r");
	if (fd == NULL)
		rte_panic("Cannot open %s\n", proc_meminfo);
	while(fgets(buffer, sizeof(buffer), fd)){
		if (strncmp(buffer, str_hugepagesz, hugepagesz_len) == 0){
			size = rte_str_to_size(&buffer[hugepagesz_len]);
			break;
		}
	}
	fclose(fd);
	if (size == 0)
		rte_panic("Cannot get default hugepage size from %s\n", proc_meminfo);
	return size;
}

static const char *
get_hugepage_dir(uint64_t hugepage_sz)
{
	enum proc_mount_fieldnames {
		DEVICE = 0,
		MOUNTPT,
		FSTYPE,
		OPTIONS,
		_FIELDNAME_MAX
	};
	static uint64_t default_size = 0;
	const char proc_mounts[] = "/proc/mounts";
	const char hugetlbfs_str[] = "hugetlbfs";
	const size_t htlbfs_str_len = sizeof(hugetlbfs_str) - 1;
	const char pagesize_opt[] = "pagesize=";
	const size_t pagesize_opt_len = sizeof(pagesize_opt) - 1;
	const char split_tok = ' ';
	char *splitstr[_FIELDNAME_MAX];
	char buf[BUFSIZ];
	char *retval = NULL;

	FILE *fd = fopen(proc_mounts, "r");
	if (fd == NULL)
		rte_panic("Cannot open %s\n", proc_mounts);

	if (default_size == 0)
		default_size = get_default_hp_size();

	while (fgets(buf, sizeof(buf), fd)){
		if (rte_strsplit(buf, sizeof(buf), splitstr, _FIELDNAME_MAX,
				split_tok) != _FIELDNAME_MAX) {
			RTE_LOG(ERR, EAL, "Error parsing %s\n", proc_mounts);
			break; /* return NULL */
		}

		/* we have a specified --huge-dir option, only examine that dir */
		if (internal_config.hugepage_dir != NULL &&
				strcmp(splitstr[MOUNTPT], internal_config.hugepage_dir) != 0)
			continue;

		if (strncmp(splitstr[FSTYPE], hugetlbfs_str, htlbfs_str_len) == 0){
			const char *pagesz_str = strstr(splitstr[OPTIONS], pagesize_opt);

			/* if no explicit page size, the default page size is compared */
			if (pagesz_str == NULL){
				if (hugepage_sz == default_size){
					retval = strdup(splitstr[MOUNTPT]);
					break;
				}
			}
			/* there is an explicit page size, so check it */
			else {
				uint64_t pagesz = rte_str_to_size(&pagesz_str[pagesize_opt_len]);
				if (pagesz == hugepage_sz) {
					retval = strdup(splitstr[MOUNTPT]);
					break;
				}
			}
		} /* end if strncmp hugetlbfs */
	} /* end while fgets */

	fclose(fd);
	return retval;
}

/*
 * Clear the hugepage directory of whatever hugepage files
 * there are. Checks if the file is locked (i.e.
 * if it's in use by another DPDK process).
 */
static int
clear_hugedir(const char * hugedir)
{
	DIR *dir;
	struct dirent *dirent;
	int dir_fd, fd, lck_result;
	const char filter[] = "*map_*"; /* matches hugepage files */

	/* open directory */
	dir = opendir(hugedir);
	if (!dir) {
		RTE_LOG(ERR, EAL, "Unable to open hugepage directory %s\n",
				hugedir);
		goto error;
	}
	dir_fd = dirfd(dir);

	dirent = readdir(dir);
	if (!dirent) {
		RTE_LOG(ERR, EAL, "Unable to read hugepage directory %s\n",
				hugedir);
		goto error;
	}

	while(dirent != NULL){
		/* skip files that don't match the hugepage pattern */
		if (fnmatch(filter, dirent->d_name, 0) > 0) {
			dirent = readdir(dir);
			continue;
		}

		/* try and lock the file */
		fd = openat(dir_fd, dirent->d_name, O_RDONLY);

		/* skip to next file */
		if (fd == -1) {
			dirent = readdir(dir);
			continue;
		}

		/* non-blocking lock */
		lck_result = flock(fd, LOCK_EX | LOCK_NB);

		/* if lock succeeds, unlock and remove the file */
		if (lck_result != -1) {
			flock(fd, LOCK_UN);
			unlinkat(dir_fd, dirent->d_name, 0);
		}
		close (fd);
		dirent = readdir(dir);
	}

	closedir(dir);
	return 0;

error:
	if (dir)
		closedir(dir);

	RTE_LOG(ERR, EAL, "Error while clearing hugepage dir: %s\n",
		strerror(errno));

	return -1;
}

static int
compare_hpi(const void *a, const void *b)
{
	const struct hugepage_info *hpi_a = a;
	const struct hugepage_info *hpi_b = b;

	return hpi_b->hugepage_sz - hpi_a->hugepage_sz;
}

/*
 * when we initialize the hugepage info, everything goes
 * to socket 0 by default. it will later get sorted by memory
 * initialization procedure.
 */
int
eal_hugepage_info_init(void)
{
	const char dirent_start_text[] = "hugepages-";
	const size_t dirent_start_len = sizeof(dirent_start_text) - 1;
	unsigned i, num_sizes = 0;
	DIR *dir;
	struct dirent *dirent;

	dir = opendir(sys_dir_path);
	if (dir == NULL) {
		RTE_LOG(ERR, EAL,
			"Cannot open directory %s to read system hugepage info\n",
			sys_dir_path);
		return -1;
	}

	for (dirent = readdir(dir); dirent != NULL; dirent = readdir(dir)) {
		struct hugepage_info *hpi;

		if (strncmp(dirent->d_name, dirent_start_text,
			    dirent_start_len) != 0)
			continue;

		if (num_sizes >= MAX_HUGEPAGE_SIZES)
			break;

		hpi = &internal_config.hugepage_info[num_sizes];
		hpi->hugepage_sz =
			rte_str_to_size(&dirent->d_name[dirent_start_len]);
		hpi->hugedir = get_hugepage_dir(hpi->hugepage_sz);

		/* first, check if we have a mountpoint */
		if (hpi->hugedir == NULL) {
			uint32_t num_pages;

			num_pages = get_num_hugepages(dirent->d_name);
			if (num_pages > 0)
				RTE_LOG(NOTICE, EAL,
					"%" PRIu32 " hugepages of size "
					"%" PRIu64 " reserved, but no mounted "
					"hugetlbfs found for that size\n",
					num_pages, hpi->hugepage_sz);
			continue;
		}

		/* try to obtain a writelock */
		hpi->lock_descriptor = open(hpi->hugedir, O_RDONLY);

		/* if blocking lock failed */
		if (flock(hpi->lock_descriptor, LOCK_EX) == -1) {
			RTE_LOG(CRIT, EAL,
				"Failed to lock hugepage directory!\n");
			break;
		}
		/* clear out the hugepages dir from unused pages */
		if (clear_hugedir(hpi->hugedir) == -1)
			break;

		/* for now, put all pages into socket 0,
		 * later they will be sorted */
		hpi->num_pages[0] = get_num_hugepages(dirent->d_name);

#ifndef RTE_ARCH_64
		/* for 32-bit systems, limit number of hugepages to
		 * 1GB per page size */
		hpi->num_pages[0] = RTE_MIN(hpi->num_pages[0],
					    RTE_PGSIZE_1G / hpi->hugepage_sz);
#endif

		num_sizes++;
	}
	closedir(dir);

	/* something went wrong, and we broke from the for loop above */
	if (dirent != NULL)
		return -1;

	internal_config.num_hugepage_sizes = num_sizes;

	/* sort the page directory entries by size, largest to smallest */
	qsort(&internal_config.hugepage_info[0], num_sizes,
	      sizeof(internal_config.hugepage_info[0]), compare_hpi);

	/* now we have all info, check we have at least one valid size */
	for (i = 0; i < num_sizes; i++)
		if (internal_config.hugepage_info[i].hugedir != NULL &&
		    internal_config.hugepage_info[i].num_pages[0] > 0)
			return 0;

	/* no valid hugepage mounts available, return error */
	return -1;
}
