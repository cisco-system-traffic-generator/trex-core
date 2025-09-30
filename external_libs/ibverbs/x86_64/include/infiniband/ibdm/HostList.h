/*
 * Copyright (c) 2024-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _COMMON_HOSTLIST_H_
#define _COMMON_HOSTLIST_H_


/* The hostlist opaque data type
 *
 * A hostlist is a sequence of host names.
 */
typedef struct hostlist* hostlist_t;

/*
 * hostlist_create():
 *
 * Create a new hostlist from a string representation.
 * The string representation (str) may contain one or more hostnames or
 * bracketed hostlists separated by following delimiters  ',' '\t' ' '.
 * A bracketed hostlist is denoted by a common prefix followed by a list of numeric
 * ranges contained within brackets: e.g. "nodename[0-5,12,20-25]"
 *
 * Empty hostlist is created and returned in case str is NULL.
 * If the create fails, hostlist_create() returns NULL.
 *
 * Note: The returned hostlist must be freed with hostlist_destroy()
 */
hostlist_t hostlist_create(const char* hostlist);

/* hostlist_destroy():
 *
 * Destroy a hostlist object.
 * Frees all memory allocated to the hostlist.
 */
void hostlist_destroy(hostlist_t hl);

/* hostlist_count():
 *
 * Return the number of hosts in hostlist hl.
 */
int hostlist_count(hostlist_t hl);

/* hostlist_n2host():
 *
 * Returns the string representation of the host in the hostlist
 * by ordering number.
 * or NULL if the hostlist is empty or passed invalid number.
 * The host is removed from the hostlist.
 *
 * Note: Caller is responsible for freeing the returned memory.
 */
char* hostlist_n2host(hostlist_t hl, size_t n);

/* hostlist_shift():
 *
 * Returns the string representation of the first host in the hostlist
 * or NULL if the hostlist is empty or there was an error allocating memory.
 * The host is removed from the hostlist.
 *
 * Note: Caller is responsible for freeing the returned memory.
 */
char* hostlist_shift(hostlist_t hl);

/* hostlist_uniq():
 *
 * Sort the hostlist hl and remove duplicate entries.
 *
 */
void hostlist_uniq(hostlist_t hl);

/* hostlist_proc():
 *
 * Process hostlist from a string representation calling
 * user defined callback function for every host.
 *
 * Return the number of processed hosts or negative value as a failure.
 */
int hostlist_proc(const char* buf, void* arg, int uniq, int (*callback)(const char* hostname, void* arg));

#endif /* _COMMON_HOSTLIST_H_ */
