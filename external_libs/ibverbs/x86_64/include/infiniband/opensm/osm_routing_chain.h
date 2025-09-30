/*
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
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
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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
 *      Routing chain manager
 */
#include <opensm/osm_port_group.h>

#ifndef OSM_ROUTING_ENGINE_H
#define OSM_ROUTING_ENGINE_H
#define MAX_ENGINES_NUMBER 50
#define MAX_ENGINE_ID (2 * MAX_ENGINES_NUMBER)

struct osm_subn;
struct osm_ucast_mgr;
struct osm_sm;
/*
 * per switch to switch link info
 */
typedef struct osm_routing_chain {
	cl_list_item_t list_item;
	size_t id;
	char *use;
	char *engine;
	char *config_file;
	struct osm_routing_chain *fallback_engine;
	struct osm_subn *subn;
	uint64_t topo_id;
	uint8_t min_path_bit;
	uint8_t max_path_bit;
	boolean_t used_fallback;
} osm_routing_chain_t;

typedef struct osm_routing_chain_mgr {
	cl_qlist_t engine_list;
	boolean_t used_ids[MAX_ENGINE_ID + 1];
	boolean_t fallback_ids[MAX_ENGINE_ID + 1];
	boolean_t is_itr;
	cl_qmap_t *p_rch_rboxes;
	struct osm_opensm *p_osm;
	struct osm_routing_engine *main_topo_routing_engine_list;
	struct osm_routing_engine *main_topo_last_routing_engine;
	struct osm_routing_engine *main_topo_routing_engine_used;
	struct osm_routing_engine *main_topo_default_routing_engine;
} osm_routing_chain_mgr_t;

/****f* OpenSM: Routing Chain/osm_rch_mgr_create
* NAME
*	osm_rch_mgr_create
*
* DESCRIPTION
*	Create the routing chain manager (parse configuration file)
*
* SYNOPSIS
*/
boolean_t osm_rch_mgr_create(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm in which to create the port grp manager.
*
* RETURN VALUE
*	True - Success
*	False - Failure
*/

/****f* OpenSM: Routing Chain/osm_rch_mgr_construct
* NAME
*	osm_rch_mgr_construct
*
* DESCRIPTION
*	Construct the routing chain manager
*
* SYNOPSIS
*/
void osm_rch_mgr_construct(IN osm_routing_chain_mgr_t *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to a routing chain manager object
*
* RETURN VALUE
*	Does not return value
*/

/****f* OpenSM: Routing Chain/osm_rch_mgr_init
 * NAME
 *       osm_rch_mgr_init
 *
 * DESCRIPTION
 *       Initialize routing chain manager object.
 *
 * SYNOPSIS
 */
ib_api_status_t osm_rch_mgr_init(IN osm_routing_chain_mgr_t * p_mgr,
				 IN struct osm_sm * sm);
/*
 * PARAMETERS
 *       p_mgr
 *               Pointer to a routing chain manager object.
 *
 *       sm
 *               Pointer to the SM object.
 *
 * RETURN VALUE
 *       IB_SUCCESS if the routing manager object was initialized successfully.
 */


/****f* OpenSM: Routing Chain/osm_rch_mgr_destroy
* NAME
*	osm_rch_mgr_destroy
*
* DESCRIPTION
*	Destroy the routing chain manager.
*
* SYNOPSIS
*/
void osm_rch_mgr_destroy(IN osm_routing_chain_mgr_t *p_mgr);
/*
* PARAMETERS
*	p_mgr
*		Pointer to routing chain manager.
*/

/* API toward parser */

/****f* OpenSM: Routing Chain/osm_rch_add_engine
* NAME
*	osm_rch_add_engine
*
* DESCRIPTION
*	Add engine to the routing chain
*
* SYNOPSIS
*/
boolean_t osm_rch_add_engine(IN struct osm_opensm *p_osm, IN size_t id,
			     IN char *use, IN char *engine, IN uint64_t topo_id,
			     IN struct osm_subn *p_subn, IN uint8_t min_path_bit,
			     IN uint8_t max_path_bit, IN char *config_file);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm.
*	id
*		id for new engine
*	use
*		use comment for new engine
*	engine
*		routing engine/algorithm to use in new engine
*	topo_id
*		id of the topology for this engine
*	p_subn
*		pointer to subn object for this engine
*	min_path_bit
*		Min offset from base lid to consider for this engine (All by default)
*	max_path_bit
*		Max offset from base lid to consider for this engine (All by default)
*	config_file
*		opensm config file for new engine
*
* RETURN VALUE
*	Boolean success indication
*/

/****f* OpenSM: Routing Chain/osm_rch_set_fallback_engine
* NAME
*	osm_rch_set_fallback_engine
*
* DESCRIPTION
*	Set fallback engine in the routing chain
*
* SYNOPSIS
*/
boolean_t osm_rch_set_fallback_engine(IN struct osm_opensm *p_osm,
				      IN size_t target_engine_id, IN size_t id,
				      IN char *use, IN char *engine,
				      IN uint64_t topo_id,
				      IN struct osm_subn *p_subn,
				      IN uint8_t min_path_bit,
				      IN uint8_t max_path_bit,
				      IN char *config_file);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm in which to create the port grp manager.
*	target_engine_id
*		id of engine to be fallback for.
*	id
*		id for fallback engine
*	use
*		use comment for fallback engine
*	engine
*		routing engine/algorithm to use in fallback engine
*	topo_id
*		id of the topology for this engine
*	p_subn
*		pointer to subn object for the fallback engine
*	min_path_bit
*		Min offset from base lid to consider for fallback engine (All by default)
*	max_path_bit
*		Max offset from base lid to consider for fallback engine (All by default)
*	config_file
*		opensm config file for fallback engine
*
* RETURN VALUE
*	Boolean success indication
*/

/****f* OpenSM: Routing Chain/osm_rch_engines_count
* NAME
*	osm_rch_engines_count
*
* DESCRIPTION
*	Return the number of engines in the routing chain
*
* SYNOPSIS
*/
uint32_t osm_rch_engines_count(IN struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		Pointer to a open_sm structure.
*/

/****f* OpenSM: Routing Chain/osm_rch_get_engine
* NAME
*	osm_rch_get_engine
*
* DESCRIPTION
*	Return routing engine according to required id
*
* SYNOPSIS
*/
osm_routing_chain_t *osm_rch_get_engine(struct osm_opensm *p_osm, size_t id);
/*
* PARAMETERS
*	p_osm
*		pointer to a open_sm structure.
*
*	id
*		id of required enigne
*
* RETURN VALUE
*	Pointer to routing engine (NULL if not found)
*/

/****f* OpenSM: Routing Chain/osm_rch_rbox_map_destroy
* NAME
*	osm_rch_rbox_map_destroy
*
* DESCRIPTION
* 	Destroys all rch_box objects associated with a given MLID
*
* SYNOPSIS
*/
void osm_rch_rbox_map_destroy(IN osm_routing_chain_mgr_t *p_mgr,
			      IN uint16_t mlid);
/*
* PARAMETERS
*	p_mgr
*		pointer to routing chain manager.
*
*	mlid
*		MLID of the rch_rbox map.
*
* RETURN VALUES
*       None.
*/


#endif
