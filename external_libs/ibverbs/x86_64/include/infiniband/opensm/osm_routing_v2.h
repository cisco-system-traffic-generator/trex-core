/*
 * Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 *	Declaration of osm_subn_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#pragma once

#include <stdio.h>
#include <complib/cl_bitmap.h>
#include <complib/cl_u64_vector.h>
#include <complib/cl_fleximap.h>
#include <opensm/osm_graph.h>
#include <opensm/osm_log.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/*
 * Routing directions.
 */
#define	ROUTING_INVALID				0
#define	ROUTING_UP				1
#define	ROUTING_DOWN				2

/*
 * Routing algorithm types.
 */
#define	ROUTING_ALGORITHM_MINHOP		0
#define	ROUTING_ALGORITHM_BFS			1
#define	ROUTING_ALGORITHM_DIJKSTRA		2
#define	ROUTING_ALGORITHM_UPDN			3

#define	ROUTING_ALGORITHM_AR_MINHOP		10
#define	ROUTING_ALGORITHM_AR_BFS		11
#define	ROUTING_ALGORITHM_AR_DIJKSTRA		12
#define	ROUTING_ALGORITHM_AR_UPDN		13

#define	ROUTING_ALGORITHM_AR_ASYM1		21
#define	ROUTING_ALGORITHM_AR_ASYM2		22
#define	ROUTING_ALGORITHM_AR_ASYM3		23

#define routing_key(P_ROUTE, DISTANCE)	(((uint64_t)(DISTANCE) << 32) |				\
					 ((uint64_t)((P_ROUTE)->dst) << 16) |			\
					  (uint64_t)((P_ROUTE)->src))

/*
 * Define for uint64_t port masks.
 */
#define	ROUTING_PORT_MASK_SIZE			(IB_ENDP_MAX_NUMBER_PORTS/64)
#define	ROUTING_PORT_MASK_BYTES			(ROUTING_PORT_MASK_SIZE * sizeof(uint64_t))

typedef struct osm_ucast_mgr osm_ucast_mgr_t;
typedef struct osm_v2_context osm_v2_context_t;

/* ----------------------------------------------------------------------------------------------
 * Routing helper structs.
 */

typedef struct {
	char			*id;
	uint64_t		leader;
	cl_u64_vector_t		members;
} osm_class_vector_t;

typedef struct {
	char			*id;
	int			index;
} osm_class_map_t;

/* ----------------------------------------------------------------------------------------------
 * Routing data structs.
 */

typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	uint32_t		*edges;

	size_t			visited_size;
	size_t			distance_size;
	size_t			edges_size;
} osm_vde_t;


typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	uint32_t		*stack;

	size_t			visited_size;
	size_t			distance_size;
	size_t			stack_size;
} osm_vds_t;


typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	uint16_t		*queue;

	size_t			visited_size;
	size_t			distance_size;
	size_t			queue_size;
} osm_vdq_t;


typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	uint8_t			*direction;
	uint16_t		*queue;

	size_t			visited_size;
	size_t			distance_size;
	size_t			direction_size;
	size_t			queue_size;
} osm_vddq_t;


typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	uint8_t			*direction;
	uint8_t			*first;
	uint16_t		*queue;

	size_t			visited_size;
	size_t			distance_size;
	size_t			direction_size;
	size_t			first_size;
	size_t			queue_size;
} osm_vddfq_t;


typedef struct {
	uint8_t			*visited;
	uint8_t			*distance;
	void			*heap;

	size_t			visited_size;
	size_t			distance_size;
	size_t			heap_size;
} osm_vdh_t;


typedef struct {
	uint16_t		src;
	uint16_t		dst;
	uint32_t		heap_index;
	cl_bitmap_t		ports;
} osm_route_t;


typedef struct {
	uint8_t			**hops;
} osm_hops_t;


typedef struct osm_route_context {
	osm_routing_engine_t	*p_re;
	osm_opensm_t		*p_osm;
	osm_ucast_mgr_t		*p_mgr;
	osm_log_t		*p_log;

	uint8_t			plane;

	osm_graph_t		*p_graph;
	size_t			num_vertices;
	void			*re_context;

	uint8_t			*p_least_hops;
	cl_bitmap_t		least_dirs;
	osm_hops_t		*p_hops;
	osm_route_t		*p_routes;

	void			*task_context;
	void			*function;
	void			*sw_function;
	void			*sw_init_function;
	void			*sw_fini_function;

	osm_v2_context_t	*p_v2_context;
} osm_route_context_t;

typedef struct osm_v2_context {
	osm_routing_engine_t	*p_re;
	osm_opensm_t		*p_osm;
	osm_ucast_mgr_t		*p_mgr;
	osm_log_t		*p_log;

	osm_route_context_t	*plane_context[IB_MAX_NUM_OF_PLANES_XDR+1];
} osm_v2_context_t;

#define foreach_route(P_CONTEXT, P_ROUTE, ITER)						\
	for (ITER = 0, P_ROUTE = P_CONTEXT->p_routes;					\
	     ITER < P_CONTEXT->num_vertices * P_CONTEXT->num_vertices;			\
	     ITER++, P_ROUTE++)

/* ----------------------------------------------------------------------------------------------
 * Routing port functions.
 */

__attribute__((unused))
static inline void osm_route_ports_init(IN osm_route_t *p_route, IN int count)
{
	cl_bitmap_init(&p_route->ports, count);
}

__attribute__((unused))
static inline void osm_route_ports_destroy(IN osm_route_t *p_route)
{
	cl_bitmap_destroy(&p_route->ports);
}

__attribute__((unused))
static inline void osm_route_ports_set(IN osm_route_t *p_route, IN uint8_t port_num)
{
	cl_bitmap_set(&p_route->ports, port_num);
}

__attribute__((unused))
static inline void osm_route_ports_copy(IN osm_route_t *p_dst_route, IN osm_route_t *p_src_route)
{
	cl_bitmap_clone(&p_dst_route->ports, &p_src_route->ports);
}

__attribute__((unused))
static inline void osm_route_ports_merge(IN osm_route_t *p_dst_route, IN osm_route_t *p_src_route)
{
	cl_bitmap_or(&p_dst_route->ports, &p_src_route->ports);
}

__attribute__((unused))
static inline void osm_route_ports_clear(IN osm_route_t *p_route)
{
	cl_bitmap_reset(&p_route->ports);
}

__attribute__((unused))
static inline void osm_route_ports_to_array(IN osm_route_t *p_route, OUT uint8_t *ports, IN int count)
{
	cl_bitmap_to_array(&p_route->ports, ports, count);
}

__attribute__((unused))
static inline void osm_route_ports_from_array(IN osm_route_t *p_route, IN uint8_t *ports, IN int count)
{
	cl_bitmap_from_array(&p_route->ports, ports, count);
}

__attribute__((unused))
static inline int osm_route_ports_get_port_list(IN osm_route_t *p_route, OUT int *ports, IN int length)
{
	return cl_bitmap_get_set_positions(&p_route->ports, ports, (size_t)length);
}

__attribute__((unused))
static inline int osm_route_ports_count(IN osm_route_t *p_route)
{
	return cl_bitmap_count(&p_route->ports);
}

/* ----------------------------------------------------------------------------------------------
 * Helper functions.
 */

static inline osm_route_t *osm_index_to_index_route(IN osm_route_context_t *p_context, IN int src, IN int dst)
{
	if (((size_t)src < p_context->num_vertices) && ((size_t)dst < p_context->num_vertices)) {
		return &p_context->p_routes[(src * p_context->num_vertices) + dst];
	} else {
		return NULL;
	}
}

static inline osm_route_t *osm_switch_to_switch_route(IN osm_route_context_t *p_context,
						      IN osm_switch_t *p_src_sw,
						      IN osm_switch_t *p_dst_sw)
{
	int		src_index = osm_graph_switch_to_index(p_context->p_graph, p_src_sw);
	int		dst_index = osm_graph_switch_to_index(p_context->p_graph, p_dst_sw);

	if ((src_index < 0) || (dst_index < 0)) {
		return NULL;
	} else {
		return osm_index_to_index_route(p_context, src_index, dst_index);
	}
}

static inline osm_route_t *osm_vertex_to_vertex_route(IN osm_route_context_t *p_context,
						      IN osm_vertex_t *p_src_vertex,
						      IN osm_vertex_t *p_dst_vertex)
{
	return osm_switch_to_switch_route(p_context,
					  osm_graph_vertex_to_switch(p_src_vertex),
					  osm_graph_vertex_to_switch(p_dst_vertex));
}

/* ----------------------------------------------------------------------------------------------
 * Prototypes.
 */

void *osm_routing_vdq_init(osm_ucast_mgr_t *p_mgr, osm_graph_t *p_graph);
void osm_routing_vdq_free(osm_vdq_t *p_vdq);
void osm_routing_vdq_reset(osm_vdq_t *p_vdq);

void *osm_routing_vddq_init(osm_ucast_mgr_t *p_mgr, osm_graph_t *p_graph);
void osm_routing_vddq_free(osm_vddq_t *p_vddq);
void osm_routing_vddq_reset(osm_vddq_t *p_vddq);

void *osm_routing_vddfq_init(osm_ucast_mgr_t *p_mgr, osm_graph_t *p_graph);
void osm_routing_vddfq_free(osm_vddfq_t *p_vddfq);
void osm_routing_vddfq_reset(osm_vddfq_t *p_vddfq);

void *osm_routing_vdh_init(osm_ucast_mgr_t *p_mgr, osm_graph_t *p_graph);
void osm_routing_vdh_free(osm_vdh_t *p_vdh);
void osm_routing_vdh_reset(osm_vdh_t *p_vdh);

void osm_routing_index_update(const void * context, const size_t new_index);
int osm_routing_key_cmp(const void *p_lhs, const void *p_rhs);

osm_v2_context_t *osm_routing_v2_create(IN osm_opensm_t *p_osm, IN osm_routing_engine_t *p_re);
osm_route_context_t *osm_routing_v2_create_plane(IN osm_routing_engine_t *p_re, IN int plane);

void osm_routing_v2_destroy(IN osm_routing_engine_t *p_re);

int osm_routing_per_vertex(osm_route_context_t *p_context);
int osm_routing_execute_threaded(osm_route_context_t *, void *, void *);

uint8_t osm_routing_get_least_hops(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *);
void osm_routing_set_least_hops(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, uint8_t);

uint8_t osm_routing_get_least_dir(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *);
void osm_routing_set_least_dir(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, uint8_t);

uint8_t osm_routing_get_minhop_ports(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, uint64_t *);
uint8_t osm_routing_get_minhop_port_nums(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, uint8_t *);
uint8_t osm_routing_get_minhop_port_count(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *);

cl_status_t osm_routing_set_hops(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, osm_edge_t *, uint8_t);
cl_status_t osm_routing_set_hops_to_remote_switch(osm_route_context_t *, osm_vertex_t *, osm_vertex_t *, osm_edge_t *, uint8_t);

uint8_t osm_routing_v2_get_hops(osm_routing_engine_t *p_re,
			        osm_vertex_t *p_src_vertex,
			        osm_vertex_t *p_dst_vertex,
			        osm_edge_t *p_edge,
			        uint8_t plane);
uint8_t osm_routing_get_hops(osm_route_context_t *p_context,
			     osm_vertex_t *p_src_vertex,
			     osm_vertex_t *p_dst_vertex,
			     osm_edge_t *p_edge);
void osm_routing_clear_hops(osm_route_context_t *, osm_vertex_t *);

int osm_routing_set_default_hopcounts(osm_ucast_mgr_t *p_mgr, osm_graph_t *p_graph);
int osm_routing_get_ports(osm_routing_engine_t *p_re,
			  osm_switch_t *p_src_sw,
			  osm_switch_t *p_dst_sw,
			  uint8_t plane,
			  int plft,
			  int subgroup,
			  uint64_t *ports);
int osm_routing_v2(osm_routing_engine_t *p_re, uint8_t plane);

uint8_t osm_routing_get_reachable_switch_count(IN osm_routing_engine_t *p_re,
                                               IN osm_switch_t *p_src_sw,
                                               IN uint8_t plane);

void osm_routing_v2_print_all(IN osm_routing_engine_t *p_re, uint8_t plane);

END_C_DECLS
