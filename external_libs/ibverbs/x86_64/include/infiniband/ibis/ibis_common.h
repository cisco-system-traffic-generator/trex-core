/*
 * Copyright (c) 2021 Mellanox Technologies LTD. All rights reserved.
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

/* Holds common definitions used by all ibutils modules */

#ifndef IBIS_COMMON_H_
#define IBIS_COMMON_H_

#include <infiniband/misc/tool_trace/tool_trace.h>

typedef u_int16_t device_id_t;
#define DEVICE_ID_FORMAT "%hu"
typedef u_int8_t  hops_t;
typedef u_int8_t  phys_port_t;
typedef u_int16_t virtual_port_t;
typedef u_int8_t  rank_t;
typedef u_int16_t lid_t;

struct sl_vl_t {
    u_int8_t  SL;
    u_int8_t  VL;
};

// #define IBUTILS_LOG(level, fmt, ...) TT_LOG(TT_LOG_MODULE_IBDIAG, level, fmt, ## __VA_ARGS__);
#define IBUTILS_LOG(level, fmt, ...)

#ifndef WIN32
    #define SEPERATOR               "/"
#else
    #define SEPERATOR               "\\"
#endif

#endif  //IBIS_COMMON_H_
