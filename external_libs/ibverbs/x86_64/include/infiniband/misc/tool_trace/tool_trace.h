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


#ifndef _TT_LOG_H_
#define _TT_LOG_H_

#include <stdio.h>
#include "tool_trace_verbosity.h"


/******************************************************/
#if defined(_WIN32) || defined(_WIN64)
    #ifdef TT_EXPORTS
        #define TT_API __declspec(dllexport)
    #else
        #define TT_API __declspec(dllimport)
	#endif		/* def TT_EXPORTS */
#else   /* Windows */
	#define TT_API
#endif  /* Linux */

#ifdef __GNUC__
#define STRICT_TOOL_TRACE_LOG_FORMAT __attribute__((format(printf, 3, 4)))
#else
#define STRICT_TOOL_TRACE_LOG_FORMAT
#endif


BEGIN_C_DECLS

/******************************************************/
#define IN
#define OUT
#define LOG_ENTRY_SIZE_MAX		4096

int construct_secure_file(const char *file_name, FILE ** file_handle);

#define TT_LOG(module, level, fmt, ...) do {	\
		if (tt_is_module_verbosity_active(module) &&	\
		    tt_is_level_verbosity_active(level))	\
			tt_log(module, level, "(%s,%d,%s): " fmt, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__);	\
	} while (0)

#define TT_ENTER( module )	\
	TT_LOG(module, TT_LOG_LEVEL_FUNCS, "%s: [\n", __FUNCTION__);

#define TT_EXIT( module )	\
	TT_LOG(module, TT_LOG_LEVEL_FUNCS, "%s: ]\n", __FUNCTION__);

#define TT_OUT_PORT tt_get_log_file_port()


/******************************************************/
/* Function: tt_log_construct
* Arguments:
*	flush
*		[in] Set to TRUE directs the log to flush all log messages
*		immediately.  This severely degrades log performance,
*		and is normally used for debugging only.
*	module_verbosity
*		[in] The log module verbosity to be used.
*	level_verbosity
*		[in] The log level verbosity to be used.
*	log_file
*		[in] if not NULL defines the name of the log file. Otherwise
*		it is stdout.
*	max_size
*		[in] The maximum bytes size to be used for this log - MB. If value 0 than ignore.
*	accum_log_file
*		[in] Set to TRUE directs the log to append the previous log file.
*
* Returns:
*	0 on success or nonzero value otherwise.
* Description:
*	This function constructs a log object for use.
*	Calling tt_log_construct is a prerequisite to calling any other method.
* NOTES:
*	Allows calling other log methods
* SEE ALSO
*	tt_log_construct_v2
*	tt_log_destroy
* SYNOPSIS:
*/
TT_API int tt_log_construct(IN int flush,
			       IN tt_log_module_t module_verbosity,
			       IN tt_log_level_t level_verbosity,
			       IN const char *log_file,
			       IN unsigned long max_size,
			       IN int accum_log_file);


/******************************************************/
/* Function: tt_log_construct_v2
* Arguments:
*	Same as tt_log_construct() but without moudle / level verbosites.
*	The verbosites will be parsed from env variable TT_LOG_LEVEL.
*	To retrieve info regarding the eay to define it, set its value to '?'
* Returns:
*	0 on success or nonzero value otherwise.
* Description:
*	This function constructs a log object for use.
*	Calling tt_log_construct_v2 is a prerequisite to calling any other method.
*	It is a wrapper for tt_log_construct().
* NOTES:
*	Allows calling other log methods
* SEE ALSO
*	tt_log_construct
*	tt_log_destroy
* SYNOPSIS:
*/
TT_API int tt_log_construct_v2(IN int flush,
        IN const char *log_file,
        IN unsigned long max_size,
        IN int accum_log_file);


/******************************************************/
/* Function: tt_log_destroy
* Arguments:
*	None
* Returns:
*	None
* Description:
*	The function destroys the log object, releasing
*	all resources.
* NOTES:
*	Performs any necessary cleanup of the specified
*	log object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to tt_log_construct
* SEE ALSO
*	tt_log_construct
*	tt_log_init
* 	tt_log_init_v2,
* SYNOPSIS:
*/
TT_API void tt_log_destroy(void);


/******************************************************/
/* Function: tt_log
* Arguments:
*	module
*		[in] module verbosity
*	level
*		[in] level verbosity
*	p_str
*		[in] string format
*	...
*		[in] all args
* Returns:
*	None
* Description:
*	The function write a string into the log according to the module verbosity
*	and level verbosity
* NOTES:
*	This function should be called only after a call to tt_log_init / tt_log_init_v2.
*	The expected string as the same as printf format.
* SEE ALSO
*	tt_log_init
* 	tt_log_init_v2,
* SYNOPSIS:
*/
TT_API void tt_log(IN tt_log_module_t module, IN tt_log_level_t verbosity,
		   IN const char *p_str, ...) STRICT_TOOL_TRACE_LOG_FORMAT;


/******************************************************/
/* Function: tt_vlog
* Arguments:
*	module
*		[in] module verbosity
*	level
*		[in] level verbosity
*	p_str
*		[in] string format
*	args
*		[in] arguments list
* Returns:
*	None
* Description:
*	The function write a string into the log according to the module verbosity
*	and level verbosity
* NOTES:
*	This function should be called only after a call to tt_log_init / tt_log_init_v2.
*	The expected string as the same as printf format.
* SEE ALSO
*	tt_log_init
* 	tt_log_init_v2,
* SYNOPSIS:
*/
TT_API void tt_vlog(IN tt_log_module_t module, IN tt_log_level_t verbosity,
		    IN const char *p_str, IN va_list args);


/******************************************************/
/* Function: tt_log_reopen_file
* Arguments:
*	None
* Returns:
*	The port the log is using - file or stdout for example
* Description:
*	The function returns the out port the log is using
* NOTES:
*	This function should be called only after a call to tt_log_init / tt_log_init_v2.
* SEE ALSO
*	tt_log_init
* 	tt_log_init_v2,
* SYNOPSIS:
*/
TT_API FILE* tt_get_log_file_port(void);


/******************************************************/
/* Function: tt_log_get_module_verbosity
* Arguments:
*	None
* Returns:
*	module verbosity
* Description:
*	The function return the active module verbosity
* SEE ALSO
*	tt_log_get_level_verbosity
*	tt_log_set_module_verbosity
*	tt_log_set_level_verbosity
* SYNOPSIS:
*/
TT_API tt_log_module_t tt_log_get_module_verbosity(void);


/******************************************************/
/* Function: tt_log_get_level_verbosity
* Arguments:
*	None
* Returns:
*	level verbosity
* Description:
*	The function return the active level verbosity
* SEE ALSO
*	tt_log_get_module_verbosity
*	tt_log_set_module_verbosity
*	tt_log_set_level_verbosity
* SYNOPSIS:
*/
TT_API tt_log_level_t tt_log_get_level_verbosity(void);


/******************************************************/
/* Function: tt_log_set_module_verbosity
* Arguments:
*	The new module verbosity
* Returns:
*	None
* Description:
*	The function sets a new module verbosity
* SEE ALSO
*	tt_log_get_module_verbosity
*	tt_log_get_level_verbosity
*	tt_log_set_level_verbosity
* SYNOPSIS:
*/
TT_API void tt_log_set_module_verbosity(tt_log_module_t module);


/******************************************************/
/* Function: tt_log_set_level_verbosity
* Arguments:
*	The new level verbosity
* Returns:
*	None
* Description:
*	The function sets a new level verbosity
* SEE ALSO
*	tt_log_get_module_verbosity
*	tt_log_get_level_verbosity
*	tt_log_set_module_verbosity
* SYNOPSIS:
*/
TT_API void tt_log_set_level_verbosity(tt_log_level_t level);


/******************************************************/
/* Function: tt_is_module_verbosity_active
* Arguments:
*	The module verbosity to be checked
* Returns:
*	Returns TRUE if the specified log module would be logged.
*	FALSE otherwise.
* Description:
*
* SEE ALSO
*	tt_is_level_verbosity_active
* SYNOPSIS:
*/
TT_API int tt_is_module_verbosity_active(tt_log_module_t module);


/******************************************************/
/* Function: tt_is_level_verbosity_active
* Arguments:
*	The level verbosity to be checked
* Returns:
*	Returns TRUE if the specified log level would be logged.
*	FALSE otherwise.
* Description:
*
* SEE ALSO
*	tt_is_module_verbosity_active
* SYNOPSIS:
*/
TT_API int tt_is_level_verbosity_active(tt_log_level_t level);


END_C_DECLS

#endif		/* _TT_LOG_H_ */
