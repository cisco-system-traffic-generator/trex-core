/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef _ibdiag_log_
#define _ibdiag_log_

#include <stdlib.h>

#include <infiniband/ibdiag/ibdiag.h>
/******************************************************/
/* log functions */
int contruct_log_file(const char *file_name);
void dump_to_log_file(const char *fmt, ...);
void destroy_log_file();

/* prints macros */
#define LOG_PRINT(fmt, ...) dump_to_log_file(fmt, ## __VA_ARGS__)
#define LOG_WARN_PRINT(fmt, ...) LOG_PRINT("-W- " fmt, ## __VA_ARGS__)
#define LOG_INFO_PRINT(fmt, ...) LOG_PRINT("-I- " fmt, ## __VA_ARGS__)
#define LOG_ERR_PRINT(fmt, ...) LOG_PRINT("-E- " fmt, ## __VA_ARGS__)
#define LOG_HDR_PRINT(header, ...) do { \
        LOG_PRINT("---------------------------------------------\n");   \
        LOG_PRINT(header, ## __VA_ARGS__); \
    } while (0);

#define SCREEN_WARN_PRINT(fmt, ...) SCREEN_PRINT("-W- " fmt, ## __VA_ARGS__)
#define SCREEN_INFO_PRINT(fmt, ...) SCREEN_PRINT("-I- " fmt, ## __VA_ARGS__)
#define SCREEN_ERR_PRINT(fmt, ...)  SCREEN_PRINT("-E- " fmt, ## __VA_ARGS__)
#define SCREEN_PRINT(fmt, ...)      printf(fmt, ## __VA_ARGS__)

#define PRINT(fmt, ...) do {    \
        LOG_PRINT(fmt, ## __VA_ARGS__); \
        SCREEN_PRINT(fmt, ## __VA_ARGS__);    \
    } while (0)
#define WARN_PRINT(fmt, ...)        PRINT("-W- " fmt, ## __VA_ARGS__)
#define INFO_PRINT(fmt, ...)        PRINT("-I- " fmt, ## __VA_ARGS__)
#define ERR_PRINT(fmt, ...)         PRINT("-E- " fmt, ## __VA_ARGS__)
#define HDR_PRINT(header, ...) do { \
        PRINT("---------------------------------------------\n"); \
        PRINT(header, ## __VA_ARGS__); \
    } while (0);
#define ERR_PRINT_ARGS(fmt, ...)    PRINT("-E- Illegal argument: " fmt, ## __VA_ARGS__)

#endif          /* not defined _IBDIAGNET_PLUGINS_INTERFACE_H_ */
