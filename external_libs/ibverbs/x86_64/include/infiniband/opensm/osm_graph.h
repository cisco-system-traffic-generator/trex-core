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

#pragma once

#include <regex.h>

#include <opensm/osm_subnet.h>
#include <opensm/osm_switch.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else	/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif	/* __cplusplus */

BEGIN_C_DECLS

struct osm_ucast_mgr;

/*
 * Convenience macros.
 */
#define foreach_graph_vertex(P_GRAPH,P_VERTEX)									\
	for (P_VERTEX = (P_GRAPH)->vertices;									\
	     (P_GRAPH->num_vertices > 0) && (P_VERTEX <= &(P_GRAPH->vertices[(P_GRAPH->num_vertices - 1)]));	\
	     P_VERTEX++)

#define foreach_graph_edge(P_GRAPH,P_EDGE)									\
	for (P_EDGE = &((P_GRAPH)->edges[0]);									\
	     (P_GRAPH->num_edges > 0) && (P_EDGE <= &(P_GRAPH->edges[(P_GRAPH->num_edges - 1)]));		\
	     P_EDGE++)

#define foreach_graph_vertex_edge(P_VERTEX,P_EDGE)								\
	for (P_EDGE = P_VERTEX->edges;										\
	     (P_VERTEX->num_edges > 0) && (P_EDGE <= (P_VERTEX->edges + (P_VERTEX->num_edges - 1)));		\
	     P_EDGE++)

/*
 * Fundamental definitions.
 */
typedef struct osm_edge {
	uint32_t	index;

	uint32_t	vertex_index;
	uint32_t	port_num;

	uint32_t	remote_vertex_index;
	uint32_t	remote_port_num;

	uint32_t	weight;
} osm_edge_t;

typedef struct osm_vertex {
	uint32_t	index;

	uint8_t		num_hcas;

	size_t		num_edges;
	osm_edge_t	*edges;

	osm_switch_t	*p_sw;
} osm_vertex_t;

typedef struct osm_graph {
	uint32_t	plane;
	uint32_t	max_rank;
	uint32_t	max_coord;
	uint32_t	max_weight;
	uint32_t	max_vertex_edges;

	size_t		num_vertices;
	osm_vertex_t	*vertices;

	size_t		num_edges;
	osm_edge_t	*edges;

	osm_subn_t	*p_subn;
	osm_log_t	*p_log;
} osm_graph_t;

static inline boolean_t osm_graph_is_empty(IN osm_graph_t *p_graph)
{
	return (p_graph == NULL) || (p_graph->num_vertices == 0);
}

static inline osm_vertex_t *osm_graph_index_to_vertex(osm_graph_t *p_graph, int index)
{
	CL_ASSERT(p_graph != NULL);
	CL_ASSERT(0 <= index);
	CL_ASSERT(index < (int)p_graph->num_vertices);

	return &p_graph->vertices[index];
}

static inline osm_switch_t *osm_graph_index_to_switch(osm_graph_t *p_graph, int index)
{
	CL_ASSERT(p_graph != NULL);
	CL_ASSERT(0 <= index);
	CL_ASSERT(index < (int)p_graph->num_vertices);

	return p_graph->vertices[index].p_sw;
}

static inline osm_switch_t *osm_graph_vertex_to_switch(osm_vertex_t *p_vertex)
{
	CL_ASSERT(p_vertex != NULL);

	return p_vertex->p_sw;
}

static inline int osm_graph_switch_to_index(osm_graph_t *p_graph, osm_switch_t *p_sw)
{
	CL_ASSERT(p_graph != NULL);
	CL_ASSERT(p_sw != NULL);
	CL_ASSERT(p_graph->plane <= IB_MAX_NUM_OF_PLANES_XDR);

	int		sw_index = p_sw->graph_index[p_graph->plane];

	/*
	 * The switch is not in the graph in the following cases:
	 *	1) sw_index is not in the range [0, p_graph->num_vertices)
	 *	2) the vertex does not point to the switch.
	 */
	if (sw_index < 0) {
		return -1;
	} else if ((size_t)sw_index >= p_graph->num_vertices) {
		return -1;
	} else if (p_graph->vertices[sw_index].p_sw != p_sw) {
		return -1;
	} else {
		return sw_index;
	}
}

static inline osm_vertex_t *osm_graph_switch_to_vertex(osm_graph_t *p_graph, osm_switch_t *p_sw)
{
	int		sw_index = osm_graph_switch_to_index(p_graph, p_sw);

	if (sw_index < 0) {
		return NULL;
	} else {
		return osm_graph_index_to_vertex(p_graph, sw_index);
	}
}

static inline osm_edge_t *osm_graph_index_to_edge(osm_graph_t *p_graph, int index)
{
	CL_ASSERT(p_graph != NULL);
	CL_ASSERT(0 <= index);
	CL_ASSERT(index < (int)p_graph->num_edges);

	return &p_graph->edges[index];
}

static inline char *osm_vertex_name(IN osm_vertex_t * p_vertex)
{
	return osm_switch_name(p_vertex->p_sw);
}

/*
 * Prototypes.
 */
osm_graph_t		*osm_graph_create(struct osm_ucast_mgr *, uint8_t);
void			osm_graph_destroy(osm_graph_t *);

END_C_DECLS
