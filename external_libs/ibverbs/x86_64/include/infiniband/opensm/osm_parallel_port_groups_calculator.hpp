/*
 * Copyright (c) 2019-2020 Mellanox Technologies LTD. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#pragma once

#include "osm_thread_pool.hpp"
#include "opensm/osm_ar_algorithm.hpp"
#include "opensm/osm_armgr_types.hpp"

class OSMAdaptiveRoutingManager;

namespace OSM {

/*
 * Invoke method on all switches in parallel using multi threading
 */
class OSMParallelPortGroupsCalculator : public OSMThreadPoolTasksCollection {

public:
	enum SubTask {
		SUB_TASK_CALCULATE_PORT_GROUPS,
		SUB_TASK_CALCULATE_STATIC_ROUTE_INFO,
		SUB_TASK_RECALCULATE_STATIC_ROUTE_INFO,
		SUB_TASK_LAST,
	};
private:
	OSMAdaptiveRoutingManager *m_ar_mgr_;
	OSMThreadPool             *m_thread_pool_;
	ARSWDataBase              *m_sw_db_;
	SubTask                    m_sub_task_;

	TreeAlgorithmContext      *m_tree_algorithm_context_;
	LidMapping                m_lid_mapping_;

public:

	OSMParallelPortGroupsCalculator(OSMAdaptiveRoutingManager *p_ar_mgr,
					OSMThreadPool *p_thread_pool,
					osm_log_t *p_osm_log,
					ARSWDataBase *p_sw_db) :
	OSMThreadPoolTasksCollection(p_osm_log),
	m_ar_mgr_(p_ar_mgr),
	m_thread_pool_(p_thread_pool),
	m_sw_db_(p_sw_db),
	m_sub_task_(SUB_TASK_LAST),
	m_tree_algorithm_context_(NULL) {}

	~OSMParallelPortGroupsCalculator();

	void CalculatePortGroupsTree(TreeAlgorithmContext *context);

	int CalculatePortGroups(AdaptiveRoutingAlgorithm *ar_algorithm);

	int CalculateStaticRouteInfo(AdaptiveRoutingAlgorithm *ar_algorithm,
				     bool rebuild_route_info);

	OSMAdaptiveRoutingManager *GetAdaptiveRoutingManager() { return m_ar_mgr_;}
	TreeAlgorithmContext *GetTreeAlgorithmContext() { return m_tree_algorithm_context_;}
	LidMapping *GetLidMapping() { return &m_lid_mapping_;}
	SubTask GetSubTask() { return m_sub_task_;}
};
} /* end of namespace OSM */

