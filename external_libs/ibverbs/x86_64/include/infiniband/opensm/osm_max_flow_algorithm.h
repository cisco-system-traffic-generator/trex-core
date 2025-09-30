/*
 * Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <opensm/osm_subnet.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_log.h>
#include <complib/cl_mpthreadpool.h>

#define FLOW_MAX_SUPPORTED_PORTS 255

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/*
 * Convenience macros.
 */
#define foreach_vertex(P_F,P_V)                                         \
	for (P_V = &((P_F)->vertices[0]);                               \
	     P_V < &(P_F->vertices[P_F->vertices_num]);                 \
	     P_V++)

#define foreach_edge(P_F,P_E)                                           \
	for (P_E = &((P_F)->edges[0]);                                  \
	     P_E < &(P_F->edges[P_F->num_edges]);                       \
	     P_E++)

#define foreach_vertex_edge(P_F,P_V,P_E)                                \
	for (P_E = &(P_F->edges[P_V->start_edges_index]);               \
	     P_E < &(P_F->edges[P_V->end_edges_index]);                 \
	     P_E++)


struct flow_edge;
struct flow_vertex;

typedef struct flow_edge {
	int flow;
	int capacity;
	int index;
	int port_num;
	int adj_index;
	int adj_port_num;
	int residual_edge_index;
} flow_edge_t;
/*
* FIELDS
* 	flow
* 		The current flow on the edge
*
* 	capacity
* 		The capacity of the edge
*
* 	index
* 		The index of the vertex that the edge is attached to
*
*	port_num
*		The port number on the side of the vertex the edge is attached to
*
*	adj_index
* 		The index of the vertex on the other side of the edge
*
*	adj_port_num
* 		The port number on the adjacent vertex side of the edge
*
*	residual_edge_index
*		The residual edge index that is used to get
*		the residual edge of this edge from the edges array.
*		(the edge attached to the adjacent vertex with the opposite flow and direction).
*
*********/

typedef struct flow_vertex {
	osm_switch_t *p_sw;
	unsigned int index;
	unsigned int start_edges_index;
	unsigned int end_edges_index;
	unsigned int rank;
	boolean_t calculate_flow;
} flow_vertex_t;
/*
* FIELDS
* 	p_sw
* 		A pointer to the switch that this vertex represents
*
* 	index
* 		Used as the ID of the vertex when accessing the vertex data in flow_graph_t struct
* 		(The index of the vertex in vertices, dirty and visited arrays).
*
*	start_edges_index
*		The start index of the edges of this vertex in the edges array (located in the graph struct).
*
*	end_edges_index
*		The end index of the edges of this vertex in the edges array (located in the graph struct).
*
* 	degree
* 		The number of edges attached to this vertex.
*
* 	rank
* 		Used when calculating directions from a src vertex to all the other vertexes
*
* 	calculate_flow
* 		An indicator of if it's necessary to calculate flow for this vertex
*
*********/

typedef struct flow_port_group_ {
	uint8_t *ports;
	uint8_t num_ports;
} flow_port_group_t;

typedef struct flow_port_groups_ {
	unsigned int vertex_index_of_representative;
	unsigned int vertex_index_of_second_representative;
	uint8_t representative_port_map[FLOW_MAX_SUPPORTED_PORTS];
	uint8_t second_representative_port_map[FLOW_MAX_SUPPORTED_PORTS];
	flow_port_group_t *port_groups;
} flow_port_groups_t;

typedef struct flow_graph {
	flow_vertex_t *vertices;
	int vertices_num;
	flow_edge_t *edges;
	int num_edges;
	int *visited;
	boolean_t *dirty;
	void **prev_privs;
	unsigned int visited_token;
	flow_vertex_t *p_dst_vertex;
	osm_subn_t *p_subn;
	uint16_t dst_lid_ho;
	unsigned int sw_lid_to_vertex_mapping[IB_LID_UCAST_END_HO + 1];
	int count_sort_array_size;
	cl_list_t *count_sort;
	flow_port_groups_t *src_port_groups;
} flow_graph_t;
/*
* FIELDS
*	vertices
*		An array of all the vertexes in the graph.
*
*	vertices_num
*		Number of vertices in the graph.
*
*	edges
*		An array of all the edges in the graph.
*
*	num_edges
*		Number of edges in the graph.
*
*	visited
*		An array of visited tokens matching the vertices array.
*		Each cell in the array is a visited token for a vertex,
*		in the same index in the vertices array.
*
*	dirty
*		An array of dirty indicators matching the vertices array.
*		Each cell in the array is a dirty indicator for a vertex,
*		in the same index in the vertices array.
*		A vertex is dirty if it was a part of a flow on the previous run.
*
*	prev_privs
*		An array of previous privs of the switches.
*
*	visited_token
*		The current visited token.
*		Use the token to prevent the cleaning of the visited value after each DFS iteration.
*		The DFS algorithm compares the "visited" value of a vertex
*		to the "visited token" value to know if it's visited.
*
*	p_dst_vertex
*		The current destination vertex for the max flow calculation.
*
*	p_subn
*		Pointer to the Subnet object for this subnet.
*
*	dst_lid_ho
*		The current destination lid in host order.
*
*	sw_lid_to_vertex_mapping
*		A map from a switch lid to a matching vertex index.
*
*********/

/****f* OpenSM: Max Flow/osm_flow_build_graph
* NAME
*	osm_flow_build_graph
*
* DESCRIPTION
*	Process the subnet, and build flow_graph_t object.
*
* SYNOPSIS
*/
flow_graph_t *osm_flow_build_graph(osm_subn_t *p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object.
*
* RETURN VALUES
*	Returns flow_graph_t object on success and NULL on failure.
*
* NOTES
*	This function processes the subnet, and builds flow_graph_t object
*	based on the switches in the subnet.
*
*********/

/****f* OpenSM: Max Flow/osm_flow_copy_graph
* NAME
*	osm_flow_build_graph
*
* DESCRIPTION
*	Make a copy of a given flow_graph_t object and return it.
*
* SYNOPSIS
*/
flow_graph_t *osm_flow_copy_graph(flow_graph_t *p_graph);
/*
* PARAMETERS
*	p_graph
*		[in] Pointer to a flow_graph_t object.
*
* RETURN VALUES
*	Returns flow_graph_t object (copy of the original) on success and NULL on failure.
*
* NOTES
*	This function makes a copy of a given flow_graph_t object and returns it.
*
*********/

/****f* OpenSM: Max Flow/osm_flow_update_edge_directions
* NAME
*	update_edge_directions
*
* DESCRIPTION
* 	Update the direction of the edges to be from a given vertex.
*
* SYNOPSIS
*/
int osm_flow_update_edge_directions(flow_graph_t *p_graph,
				    flow_vertex_t *p_src_vertex);
/*
* PARAMETERS
*	p_graph
*		[in] Pointer to a flow_graph_t object.
*	p_src_vertex
*		[in] Pointer to a flow_vertex_t object.
*
* NOTES
* 	This function updates the direction of the edges to be
* 	from a given src to all other vertexes on the shortest paths.
*
*********/

/****f* OpenSM: Max Flow/osm_flow_init_flow_and_visited
* NAME
*	osm_flow_init_flow_and_visited
*
* DESCRIPTION
* 	Clear the flow and visited array.
*
* SYNOPSIS
*/
void osm_flow_init_flow_and_visited(flow_graph_t *p_graph);
/*
* PARAMETERS
*	p_graph
*		[in] Pointer to a flow_graph_t object.
*
* NOTES
* 	This function sets the flow to 0 on all the edges
* 	of vertexes changed in previous runs of max_flow,
* 	and init's the visited array to 0.
*
*********/

/****f* OpenSM: Max Flow/osm_flow_max_flow_solve
* NAME
*	osm_flow_max_flow_solve
*
* DESCRIPTION
*	Calculates the max flow from src to dst switches, on minimal hop paths.
*
* SYNOPSIS
*/
int osm_flow_max_flow_solve(flow_graph_t *p_graph, flow_vertex_t *p_src_vertex,
			    flow_vertex_t *p_dst_vertex, int *start_idx);

/****f* OpenSM: Max Flow/osm_flow_destroy_graph
* NAME
*	osm_flow_destroy_graph
*
* DESCRIPTION
*	Destroys the flow_graph_t object, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_flow_destroy_graph(flow_graph_t **pp_graph);
/*
* PARAMETERS
*	pp_graph
*		[in] Pointer to a pointer of flow_graph_t object.
*
* NOTES
*	This function destroys the flow_graph_t object, releasing
*	all resources.
*
*********/

void osm_flow_calculate_parallel_flow(flow_graph_t *p_graph);
END_C_DECLS
