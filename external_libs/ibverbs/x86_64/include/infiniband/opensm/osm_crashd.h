/*
 * Copyright (c) 2010 Gilad Ben-Yossef https://github.com/gby/libcrash.
 * Copyright (c) 2012 Mellanox Technologies LTD. All rights reserved.
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

#pragma once

#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <complib/cl_types.h>
#include <opensm/osm_log.h>

struct crash_message_struct;

#define CRASHD_MAX_FILENAME         (1024)
#define CRASHD_MAX_BACKTRACE_DEPTH  (25)
#define CRASHD_MAX_MSG_SIZE         (1024*16)
#define CRASHD_MSG_MAGIC            (0xdeadbeefUL)
#define CRASHD_AUXILIARY_DATA_SIZE  (CRASHD_MAX_MSG_SIZE - \
				     sizeof(struct crash_message_struct))

#define CRASHD_READ_TIMEOUT (5)

#define CRASHD_NAME "osm_crashd"

#define COPY_MEMBER(member) \
	__typeof(((mcontext_t *)NULL)->member) member

typedef struct registers {
#if defined(__x86_64__)
	COPY_MEMBER(gregs);
#elif defined(__aarch64__)
	COPY_MEMBER(regs);
	COPY_MEMBER(sp);
	COPY_MEMBER(pc);
	COPY_MEMBER(pstate);
#else
	/* Unsupported architecture */
	int dummy;
#endif
} registers_t;

/****s* OpenSM: osm_crash_msg_t
* NAME
*	osm_crash_msg_t
*
* DESCRIPTION
*	Crash info that is passed from OpenSM to crash daemon.
*
* SYNOPSIS
*/
typedef struct osm_crash_msg {
	unsigned long magic;
	pid_t         process_id;
	pid_t         thread_id;
	unsigned int  signal_number;
	unsigned int  signal_code;
	void         *fault_address;
	unsigned int  signal_errno;
	unsigned int  handler_errno;
	size_t        num_backtrace_frames;
	void         *backtrace[CRASHD_MAX_BACKTRACE_DEPTH];
	registers_t   registers;
	char          symbols[0];
} osm_crash_msg_t;
/*
* FIELDS
*	magic
*		Number that indicated whether the message was received
*
*	process_id
*		PID of the crashing process
*
*	thread_id
*		Kernel thread id. Note: struct task->pid, NOT pthread_self().
*
*	signal_number
*		received signal number.
*
*	signal_code
*		Signal code from siginfo_t. Might provide reason for signal.
*
*	fault_address
*		Fault address, if relevant.
*
*	signal_errno
*		The last error as reported via siginfo_t (usually 0).
*
*	handler_errno
*		The last error in errno when the exception handler got called.
*
*	num_backtrace_frames
*		Number of stack frames we got.
*
*	backtrace
*		OpenSM process backtrace
*
*	symbols
*		Place holder for backtrace symbols.
*********/

/*
 * Main registration function - call this once for each process (not thread).
 */
int register_crash_handler(int argc, char *argv[], const char *log_file_name);

/*
 * OSM Crash Daemon main function.
 */
void crashd_main(int argc, char *argv[], const char *log_file_name, int r_fd);

