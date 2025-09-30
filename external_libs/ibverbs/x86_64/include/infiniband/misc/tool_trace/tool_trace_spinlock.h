/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
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


#ifndef _TT_LOG_SPINLOCK_H_
#define _TT_LOG_SPINLOCK_H_

#include <stdio.h>


#ifndef WIN32
#include <pthread.h>
#else
#include <windows.h>
#endif		/* ndef WIN32 */


/******************************************************/
/* new types */
#ifndef WIN32
typedef struct tt_spinlock {
    pthread_mutex_t mutex;
} tt_spinlock_t;
#else
typedef struct tt_spinlock {
    HANDLE mutex;
} tt_spinlock_t;
#endif		/* ndef WIN32 */


/******************************************************/
int tt_spinlock_init(tt_spinlock_t * const p_spinlock);
void tt_spinlock_destroy(tt_spinlock_t * const p_spinlock);
int tt_spinlock_acquire(tt_spinlock_t * const p_spinlock);
int tt_spinlock_release(tt_spinlock_t * const p_spinlock);


#endif          /* _TT_LOG_SPINLOCK_H_ */

