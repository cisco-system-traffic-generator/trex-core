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


#ifndef _TT_LOG_VERBOSITY_H_
#define _TT_LOG_VERBOSITY_H_

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
#   define BEGIN_C_DECLS extern "C" {
#   define END_C_DECLS  }
#else           /* !__cplusplus */
#   define BEGIN_C_DECLS
#   define END_C_DECLS
#endif          /* __cplusplus */


BEGIN_C_DECLS

/******************************************************/
#if !defined(_WIN32) && !defined(_WIN64)
    typedef u_int8_t tt_log_module_t;
    typedef u_int8_t tt_log_level_t;
#else   /* Windows */
    typedef unsigned __int8 tt_log_module_t;
    typedef unsigned __int8 tt_log_level_t;
#endif  /* Linux */

typedef struct module_map {
    char *module_name;
    tt_log_module_t module;
} tt_log_module_map_t;


/******************************************************/
#define TT_LOG_LEVEL_ENV_VARIABLE_NAME  "TT_LOG_LEVEL"


/******************************************************/
#define TT_LOG_MODULE_NONE          0x00
#define TT_LOG_MODULE_IBIS          0x01
#define TT_LOG_MODULE_IBDIAG        0x02
#define TT_LOG_MODULE_IBDM          0x04
#define TT_LOG_MODULE_IBMGTSIM      0x08
#define TT_LOG_MODULE_IBDIAGNET     0x10
#define TT_LOG_MODULE_CCMGR         0x20
#define TT_LOG_MODULE_ARMGR         0x40
#define TT_LOG_MODULE_SYS           0x80
#define TT_LOG_MODULE_ALL           0xff

/* DEFAULT - turn on all */
#define TT_LOG_DEFAULT_MODULE       TT_LOG_MODULE_ALL
#define TT_LOG_NUM_OF_MODULES       9

/******************************************************/
#define TT_LOG_LEVEL_NONE           0x00
#define TT_LOG_LEVEL_ERROR          0x01
#define TT_LOG_LEVEL_INFO           0x02
#define TT_LOG_LEVEL_MAD            0x04
#define TT_LOG_LEVEL_DISCOVER       0x08
#define TT_LOG_LEVEL_DEBUG          0x10
#define TT_LOG_LEVEL_FUNCS          0x20
#define TT_LOG_LEVEL_SYS            0x80
#define TT_LOG_LEVEL_ALL            0xff

/* DEFAULT - turn on ERROR and INFO only */
#define TT_LOG_DEFAULT_LEVEL        TT_LOG_LEVEL_ERROR | TT_LOG_LEVEL_NONE


/******************************************************/

END_C_DECLS

#endif          /* _TT_LOG_VERBOSITY_H_ */
