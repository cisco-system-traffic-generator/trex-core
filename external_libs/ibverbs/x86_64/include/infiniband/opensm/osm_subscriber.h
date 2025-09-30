/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
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

/****h* OpenSM subscriber interface
* DESCRIPTION
*       Event subscriber interface.
*
* AUTHOR
*	Jeff Young, NVIDIA
*
*********/

typedef enum {
	osm_subscriber_type_start = 0,			// must be first
	osm_subscriber_type_event,
	osm_subscriber_type_routing,
	osm_subscriber_type_partitions,
	osm_subscriber_type_end,			// must be last
} osm_subscriber_type_t;

typedef struct {
	void			(*report)(void *, osm_epi_event_id_t, void *);
} osm_event_callbacks_t;

typedef struct {
	void			*p_void;		// fill in later
} osm_routing_callbacks_t;

typedef struct {
	void			(*update_partitions)(void *);
} osm_partitions_callbacks_t;

typedef struct {
	cl_list_item_t		list_item;
	cl_qlist_t		*qlist;
	char			*name;
	osm_subscriber_type_t	type;
	void			*context;
	void			*callbacks;
} osm_subscriber_t;

/*
 * API functions.
 */
osm_subscriber_t *osm_subscriber_create(osm_opensm_t *, const char *, osm_subscriber_type_t, void *, void *);
void osm_subscriber_destroy(osm_opensm_t *, osm_subscriber_t *);


/*
 * Convenience macros.
 */
#define	foreach_subscriber(P_SUBN, P_LIST, P_SUBSCRIBER)							\
	for (P_SUBSCRIBER  = (osm_subscriber_t *)cl_qlist_head(&((P_SUBN)->P_LIST));				\
	     P_SUBSCRIBER != (osm_subscriber_t *)cl_qlist_end (&((P_SUBN)->P_LIST));				\
	     P_SUBSCRIBER  = (osm_subscriber_t *)cl_qlist_next(&((P_SUBSCRIBER)->list_item)))

#define	foreach_event_subscriber(P_SUBN, P_SUBSCRIBER)								\
	foreach_subscriber(P_SUBN, event_subscriber_list, P_SUBSCRIBER)

#define	foreach_routing_subscriber(P_SUBN, P_SUBSCRIBER)							\
	foreach_subscriber(P_SUBN, routing_subscriber_list, P_SUBSCRIBER)

#define	foreach_partitions_subscriber(P_SUBN, P_SUBSCRIBER)                                                     \
	foreach_subscriber(P_SUBN, partitions_subscriber_list, P_SUBSCRIBER)

END_C_DECLS

