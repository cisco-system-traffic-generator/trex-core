/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <time.h>
#include <limits.h>
#include <iba/ib_types.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_config.h>
#include <opensm/osm_switch.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****h* OpenSM plugin interface
* DESCRIPTION
*       Loadable plugin interface.
*
* AUTHOR
*	Jeff Young, NVIDIA
*
*********/

typedef struct osm_plugin osm_plugin_t;

#define	OSM_PLUGIN_VERSION	"1"

#define	osm_join(X,Y)		X ## _ ## Y

/*
 * System functions.  These are not to be called except via macros below.
 */
void osm_plugin_init(IN osm_opensm_t *p_osm);
osm_plugin_t *osm_plugin_first(void);
osm_plugin_t *osm_plugin_next(IN osm_plugin_t *p_plugin);

/*
 * API functions.
 */
void *osm_plugin_load(IN osm_opensm_t *p_osm,
		      IN const char *plugin_name,
		      IN void *context);
void osm_plugin_unload(IN osm_opensm_t *p_osm,
		       IN const char *plugin_name,
		       IN void *context);
void *osm_plugin_resolve(IN osm_opensm_t *p_osm,
			 IN const char *plugin_name,
			 IN const char *symbol_name);
void *osm_plugin_find(IN char *name);
void *osm_plugin_data(IN void *p_plugin);
char *osm_plugin_name(IN void *p_plugin);

/*
 * Return plugin_args array in argc, argv format.
 * argv[0] = plugin_name, argv[argc] = NULL;
 * Need to call osm_plugin_release_args_array function to release the array.
 */
char **osm_plugin_get_args_array(IN osm_opensm_t *p_osm,
                                 IN const char *plugin_name, OUT int *argc);

void osm_plugin_release_args_array(IN char **plugin_args);

/*
 * Convenience macro.
 */
#define osm_declare_plugin(NAME)						\
const char *osm_join(NAME,version) = OSM_PLUGIN_VERSION;

#define foreach_plugin(P_PLUGIN)						\
	for (P_PLUGIN  = osm_plugin_first();					\
	     P_PLUGIN != NULL;							\
	     P_PLUGIN  = osm_plugin_next(P_PLUGIN))

END_C_DECLS

