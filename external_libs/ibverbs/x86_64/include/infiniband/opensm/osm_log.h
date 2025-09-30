/*
 * Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2012 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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

/*
 * Abstract:
 * 	Declaration of osm_log_t.
 *	This object represents the log file.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#ifndef __WIN__
#include <syslog.h>
#endif
#include <complib/cl_spinlock.h>
#include <complib/cl_qmap.h>
#include <opensm/osm_base.h>
#include <iba/ib_types.h>
#include <stdio.h>

#ifdef __GNUC__
#define STRICT_OSM_LOG_FORMAT __attribute__((format(printf, 3, 4)))
#define STRICT_OSM_LOG_V2_FORMAT __attribute__((format(printf, 4, 5)))
#define STRICT_OSM_ASSERT_FORMAT __attribute__((format(printf, 1, 2)))
#else
#define STRICT_OSM_LOG_FORMAT
#define STRICT_OSM_LOG_V2_FORMAT
#define STRICT_OSM_ASSERT_FORMAT
#endif

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
#define LOG_ENTRY_SIZE_MAX		4096
#define LOG_LARGE_BUF_SIZE_MAX		(LOG_ENTRY_SIZE_MAX*32)
#define BUF_SIZE			LOG_ENTRY_SIZE_MAX
#define __func__ __FUNCTION__
#ifdef FILE_ID
#define OSM_LOG_ENTER( OSM_LOG_PTR ) \
	osm_log_v2( OSM_LOG_PTR, OSM_LOG_FUNCS, FILE_ID, \
		    "%s: [\n", __func__);
#define OSM_LOG_EXIT( OSM_LOG_PTR ) \
	osm_log_v2( OSM_LOG_PTR, OSM_LOG_FUNCS, FILE_ID, \
		    "%s: ]\n", __func__);
#define OSM_LOG_IS_ACTIVE_V2( OSM_LOG_PTR, OSM_LOG_FUNCS ) \
	osm_log_is_active_v2( OSM_LOG_PTR, OSM_LOG_FUNCS, FILE_ID)
#else
#define OSM_LOG_ENTER( OSM_LOG_PTR ) \
	osm_log( OSM_LOG_PTR, OSM_LOG_FUNCS, \
		 "%s: [\n", __func__);
#define OSM_LOG_EXIT( OSM_LOG_PTR ) \
	osm_log( OSM_LOG_PTR, OSM_LOG_FUNCS, \
		 "%s: ]\n", __func__);
#endif

/****h* OpenSM/Log
* NAME
*	Log
*
* DESCRIPTION
*
* AUTHOR
*
*********/
typedef uint8_t osm_log_level_t;

#define OSM_LOG_NONE	0x00
#define OSM_LOG_ERROR	0x01
#define OSM_LOG_INFO	0x02
#define OSM_LOG_VERBOSE	0x04
#define OSM_LOG_DEBUG	0x08
#define OSM_LOG_FUNCS	0x10
#define OSM_LOG_FRAMES	0x20
#define OSM_LOG_ROUTING	0x40
#define OSM_LOG_ALL	0x7f
#define OSM_LOG_SYS	0x80

/*
	DEFAULT - turn on ERROR and INFO only
*/
#define OSM_LOG_DEFAULT_LEVEL		(OSM_LOG_ERROR | OSM_LOG_INFO)
#define OSM_SYSLOG_LOG_DEFAULT_LEVEL	OSM_LOG_SYS

/****s* OpenSM: Log/osm_log_t
* NAME
*	osm_log_t
*
* DESCRIPTION
*
* SYNOPSIS
*/
typedef struct osm_log {
	osm_log_level_t level;
	osm_log_level_t syslog_level;
	cl_spinlock_t lock;
	unsigned long count;
	unsigned long max_size;
	boolean_t flush;
	FILE *out_port;
	boolean_t accum_log_file;
	boolean_t daemon;
	char *log_file_name;
	char *log_prefix;
	osm_log_level_t per_mod_log_tbl[256];
	cl_qmap_t admin_pid;
	boolean_t bypass_policy_parsed;
	int num_backlogs;
} osm_log_t;
/*********/

/************ Thread Log *****************/

void osm_log_register_admin_pid(osm_log_t *p_log);
void osm_log_unregister_admin_pid(osm_log_t *p_log);
int osm_log_is_admin_pid(const osm_log_t *p_log);

/************ Thread Log *****************/

#define OSM_LOG_MOD_NAME_MAX	32

/****f* OpenSM: Log/osm_get_log_per_module
 * NAME
 *	osm_get_log_per_module
 *
 * DESCRIPTION
 *	This looks up the given file ID in the per module log table.
 *	NOTE: this code is not thread safe. Need to grab the lock before
 *	calling it.
 *
 * SYNOPSIS
 */
osm_log_level_t osm_get_log_per_module(IN osm_log_t * p_log,
				       IN const int file_id);
/*
 * PARAMETERS
 *	p_log
 *		[in] Pointer to a Log object to construct.
 *
 *	file_id
 *		[in] File ID for module
 *
 * RETURN VALUES
 *	The log level from the per module logging structure for this file ID.
 *********/

/****f* OpenSM: Log/osm_set_log_per_module
 * NAME
 *	osm_set_log_per_module
 *
 * DESCRIPTION
 *	This sets log level for the given file ID in the per module log table.
 *	NOTE: this code is not thread safe. Need to grab the lock before
 *	calling it.
 *
 * SYNOPSIS
 */
void osm_set_log_per_module(IN osm_log_t * p_log, IN const int file_id,
			    IN osm_log_level_t level);
/*
 * PARAMETERS
 *	p_log
 *		[in] Pointer to a Log object to construct.
 *
 *	file_id
 *		[in] File ID for module
 *
 *	level
 *		[in] Log level of the module
 *
 * RETURN VALUES
 *	This function does not return a value.
 *********/

/****f* OpenSM: Log/osm_reset_log_per_module
 * NAME
 *	osm_reset_log_per_module
 *
 * DESCRIPTION
 *	This resets log level for the entire per module log table.
 *	NOTE: this code is not thread safe. Need to grab the lock before
 *	calling it.
 *
 * SYNOPSIS
 */
void osm_reset_log_per_module(IN osm_log_t * p_log);
/*
 * PARAMETERS
 *	p_log
 *		[in] Pointer to a Log object to construct.
 *
 * RETURN VALUES
 *	This function does not return a value.
 *********/

/****f* OpenSM: Log/osm_log_construct
* NAME
*	osm_log_construct
*
* DESCRIPTION
*	This function constructs a Log object.
*
* SYNOPSIS
*/
static inline void osm_log_construct(IN osm_log_t * p_log)
{
	cl_spinlock_construct(&p_log->lock);
	cl_qmap_init(&p_log->admin_pid);
}

/*
* PARAMETERS
*	p_log
*		[in] Pointer to a Log object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_log_init, osm_log_init_v2, osm_log_destroy
*
*	Calling osm_log_construct is a prerequisite to calling any other
*	method except osm_log_init or osm_log_init_v2.
*
* SEE ALSO
*	Log object, osm_log_init, osm_log_init_v2,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_destroy
* NAME
*	osm_log_destroy
*
* DESCRIPTION
*	The osm_log_destroy function destroys the object, releasing
*	all resources.
*
* SYNOPSIS
*/
static inline void osm_log_destroy(IN osm_log_t * p_log)
{
	cl_spinlock_destroy(&p_log->lock);
	if (p_log->out_port != stdout) {
		fclose(p_log->out_port);
		p_log->out_port = stdout;
	}
	closelog();
}

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified
*	Log object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to
*	osm_log_construct, osm_log_init, or osm_log_init_v2.
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_init, osm_log_init_v2
*********/

/****f* OpenSM: Log/osm_log_init_v2
* NAME
*	osm_log_init_v2
*
* DESCRIPTION
*	The osm_log_init_v2 function initializes a
*	Log object for use.
*
* SYNOPSIS
*/
ib_api_status_t
osm_log_init_v2(IN osm_log_t * const p_log,
		IN const boolean_t flush,
		IN const uint8_t log_flags,
		IN const char *log_file,
		IN const unsigned long max_size,
		IN const boolean_t accum_log_file);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	flush
*		[in] Set to TRUE directs the log to flush all log messages
*		immediately.  This severely degrades log performance,
*		and is normally used for debugging only.
*
*	log_flags
*		[in] The log verbosity level to be used.
*
*	log_file
*		[in] if not NULL defines the name of the log file. Otherwise
*		it is stdout.
*
* RETURN VALUES
*	CL_SUCCESS if the Log object was initialized
*	successfully.
*
* NOTES
*	Allows calling other Log methods.
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_init_v3
* NAME
*	osm_log_init_v3
*
* DESCRIPTION
*	The osm_log_init_v3 function initializes a
*	Log object for use.
*
* SYNOPSIS
*/
ib_api_status_t
osm_log_init_v3(IN osm_log_t * const p_log,
		IN const boolean_t flush,
		IN const uint8_t log_flags,
		IN const uint8_t syslog_log_flags,
		IN const char *log_file,
		IN const unsigned long max_size,
		IN const boolean_t accum_log_file);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	flush
*		[in] Set to TRUE directs the log to flush all log messages
*		immediately.  This severely degrades log performance,
*		and is normally used for debugging only.
*
*	log_flags
*		[in] The log verbosity level to be used.
*
*	syslog_log_flags
*		[in] The log verbosity level to be used when logging to syslog.
*
*	log_file
*		[in] if not NULL defines the name of the log file. Otherwise
*		it is stdout.
*
* RETURN VALUES
*	CL_SUCCESS if the Log object was initialized
*	successfully.
*
* NOTES
*	Allows calling other Log methods.
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_reopen_file
* NAME
*	osm_log_reopen_file
*
* DESCRIPTION
*	The osm_log_reopen_file function reopens the log file
*
* SYNOPSIS
*/
int osm_log_reopen_file(osm_log_t * p_log);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
* RETURN VALUES
*	0 on success or nonzero value otherwise.
*********/

/****f* OpenSM: Log/osm_log_init
* NAME
*	osm_log_init
*
* DESCRIPTION
*	The osm_log_init function initializes a
*	Log object for use. It is a wrapper for osm_log_init_v2().
*
* SYNOPSIS
*/
ib_api_status_t osm_log_init(IN osm_log_t * p_log, IN boolean_t flush,
			     IN uint8_t log_flags, IN const char *log_file,
			     IN boolean_t accum_log_file);
/*
 * Same as osm_log_init_v2() but without max_size parameter
 */

void osm_log(IN osm_log_t * p_log, IN osm_log_level_t verbosity,
	     IN const char *p_str, ...) STRICT_OSM_LOG_FORMAT;

void osm_log_v2(IN osm_log_t * p_log, IN osm_log_level_t verbosity,
		IN const int file_id, IN const char *p_str, ...) STRICT_OSM_LOG_V2_FORMAT;

/****f* OpenSM: Log/osm_log_get_level
* NAME
*	osm_log_get_level
*
* DESCRIPTION
*	Returns the current log level.
*
* SYNOPSIS
*/
static inline osm_log_level_t osm_log_get_level(IN const osm_log_t * p_log)
{
	return p_log->level;
}

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
* RETURN VALUES
*	Returns the current log level.
*
* NOTES
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_set_level
* NAME
*	osm_log_set_level
*
* DESCRIPTION
*	Sets the current log level.
*
* SYNOPSIS
*/
static inline void osm_log_set_level(IN osm_log_t * p_log,
				     IN osm_log_level_t level)
{
	p_log->level = level;
	osm_log(p_log, OSM_LOG_ALL, "Setting log level to: 0x%02x\n", level);
}

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	level
*		[in] New level to set.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_set_num_backlogs
* NAME
*	osm_log_set_num_backlogs
*
* DESCRIPTION
*	Sets number of back logs.
*
* SYNOPSIS
*/
void osm_log_set_num_backlogs(IN osm_log_t * p_log, IN int num_backlogs);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	num_backlogs
*		[in] Number of back logs.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_set_max_size
* NAME
*	osm_log_set_max_size
*
* DESCRIPTION
*	Sets log file maximal size.
*
* SYNOPSIS
*/
void osm_log_set_max_size(IN osm_log_t * const p_log, IN const unsigned long max_size);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	max size
*		[in] Maximal size in MB.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

/****f* OpenSM: Log/osm_log_is_active
* NAME
*	osm_log_is_active
*
* DESCRIPTION
*	Returns TRUE if the specified log level would be logged.
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t osm_log_is_active(IN const osm_log_t * p_log,
					  IN osm_log_level_t level)
{
	return ((p_log->level & level) != 0);
}

/*
* PARAMETERS
*	p_log
*		[in] Pointer to the log object.
*
*	level
*		[in] Level to check.
*
* RETURN VALUES
*	Returns TRUE if the specified log level would be logged.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Log object, osm_log_construct,
*	osm_log_destroy
*********/

static inline boolean_t osm_log_is_active_v2(IN const osm_log_t * p_log,
					     IN osm_log_level_t level,
					     IN const int file_id)
{
	if ((p_log->level & level) != 0)
		return 1;
	if ((level & p_log->per_mod_log_tbl[file_id]))
		return 1;
	if (osm_log_is_admin_pid(p_log))
		return 1;
	return 0;
}

extern void osm_log_msg_box(osm_log_t *log, osm_log_level_t level,
			    const char *func_name, const char *msg);
extern void osm_log_msg_box_v2(osm_log_t *log, osm_log_level_t level,
			       const int file_id, const char *func_name,
			       const char *msg);
extern void osm_log_raw(IN osm_log_t * p_log, IN osm_log_level_t verbosity,
			IN const char *p_buf);

#ifdef FILE_ID
#define OSM_LOG(log, level, fmt, ...) do { \
		if (osm_log_is_active_v2(log, (level), FILE_ID)) \
			osm_log_v2(log, level, FILE_ID, "%s: " fmt, __func__, ## __VA_ARGS__); \
	} while (0)

#define OSM_LOG_MSG_BOX(log, level, msg) \
		osm_log_msg_box_v2(log, level, FILE_ID, __func__, msg)
#else
#define OSM_LOG(log, level, fmt, ...) do { \
		if (osm_log_is_active(log, (level))) \
			osm_log(log, level, "%s: " fmt, __func__, ## __VA_ARGS__); \
	} while (0)

#define OSM_LOG_MSG_BOX(log, level, msg) \
		osm_log_msg_box(log, level, __func__, msg)
#endif

/*
 * The macro OSM_LOG_VALIDATE_CONF supports two modes:
 *   OSM_LOG_VALIDATE_CONF_DEFAULT:
 *     If log file exists, write message to log file.
 *     Otherwise to stderr.
 *
 *   OSM_LOG_VALIDATE_CONF_BOTH:
 *     If log file exists, write message to both log file and stderr.
 *     Otherwise, stderr only.
 *
 * When running OpenSM in configuration file validate mode,
 * write log messages only to stderr.
 */
#define OSM_LOG_VALIDATE_CONF_DEFAULT	0
#define OSM_LOG_VALIDATE_CONF_BOTH	1

#define OSM_LOG_VALIDATE_CONF(log, level, log_both, fmt, ...) do { \
		if (log) { \
			OSM_LOG(log, level, fmt, ## __VA_ARGS__); \
			if ((log_both)) \
				fprintf(stderr, fmt, ## __VA_ARGS__); \
		} else { \
			fprintf(stderr, "%s: " fmt, __func__, ## __VA_ARGS__); \
		} \
	} while (0)


/****f* OpenSM: Helper/osm_is_debug
* NAME
*	osm_is_debug
*
* DESCRIPTION
*	The osm_is_debug function returns TRUE if the opensm was compiled
*	in debug mode, and FALSE otherwise.
*
* SYNOPSIS
*/
boolean_t osm_is_debug(void);
/*
* PARAMETERS
*	None
*
* RETURN VALUE
*	TRUE if compiled in debug version. FALSE otherwise.
*
* NOTES
*
*********/

/****f* OpenSM: Log/osm_log_assert
* NAME
*	osm_log_assert
*
* DESCRIPTION
*	Print formatted assertion message.
*
* SYNOPSIS
*/
void osm_log_assert(IN const char *p_str, ...) STRICT_OSM_ASSERT_FORMAT;
/*
* PARAMETERS
*	p_str
*		[in] Output format string.
*	...
*		[in] Variable arguments.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	osm_log_to_stream
*********/

/****f* OpenSM: Log/osm_log_to_stream
* NAME
*	osm_log_to_stream
*
* DESCRIPTION
*	Print formatted message to the given stream
*
* SYNOPSIS
*/
void osm_log_to_stream(IN FILE * out_stream, IN const char *p_str, ...);
/*
* PARAMETERS
*	out_stream
*		[in] Stream that the message should be printed to.
*	p_str
*		[in] Output format string.
*	...
*		[in] Variable arguments.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	osm_log_assert
*********/

#ifdef	OSM_DEBUG
#define	OSM_ASSERT_DEBUG	1
#else
#define	OSM_ASSERT_DEBUG	0
#endif

#define VA_ARGS_SIZE(...)       sizeof((char[]){# __VA_ARGS__})
#define HAS_VA_ARGS(...)        (VA_ARGS_SIZE(__VA_ARGS__) != 1)

#define LOG_ASSERT_MESSAGE	"ASSERT(file %s, line %d, function %s) - assertion failed\n"

#define OSM_LOG_ASSERT()							\
    fprintf(stderr, LOG_ASSERT_MESSAGE, __FILE__, __LINE__, __func__);		\
    osm_log_assert(LOG_ASSERT_MESSAGE, __FILE__, __LINE__, __func__);

#define OSM_ASSERT(expr, ...)							\
	do {									\
		if (PF(!(expr))) {						\
			OSM_LOG_ASSERT();					\
										\
			if (OSM_ASSERT_DEBUG) {					\
				CL_ASSERT(expr);				\
			} else if (HAS_VA_ARGS(__VA_ARGS__)) {			\
				__VA_ARGS__ ;					\
			}							\
		}								\
	} while (0)

END_C_DECLS
