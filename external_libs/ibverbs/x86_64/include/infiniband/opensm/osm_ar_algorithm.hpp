/*
 * Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2019-2020 Mellanox Technologies LTD. All rights reserved.
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
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
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

#include "opensm/osm_armgr_types.hpp"
#include "osm_calculate_port_groups_task.hpp"

#define MAX_OP_VL_CODE 5
#define INVALID_DIM_INDEX 0xFF

class OSMAdaptiveRoutingManager;

namespace OSM {
struct SwitchConnection;
struct RouteInfo;

typedef int8_t dim_sign_t;
typedef uint8_t dim_index_t;
typedef uint16_t dim_size_t;

typedef vector <RouteInfo> RouteInfoVec;
typedef vector <SwitchConnection> ConnectionVec;
typedef vector <SwitchConnection*> ConnectionPtrVec;

typedef map <osm_switch_t*, SwitchConnection&> SwToConnectionMap;
typedef pair <osm_switch_t*, SwitchConnection&> SwToConnectionMapEntry;
typedef SwToConnectionMap::iterator SwToConnectionMapIter;
typedef pair <SwToConnectionMapIter, bool> SwToConnectionMapInsertRes;

typedef list <osm_physp_t*> PhysPortsList;
typedef PhysPortsList::iterator PhysPortsListIter;

typedef vector <DfSwData *> DfSwDataPtrVec;

typedef vector < LidToGroupNumberMap > LidToGroupNumberVecMap;

struct LidMapping {
	u_int16_t m_lid_to_sw_lid_mapping[IB_LID_UCAST_END_HO + 1];
	u_int16_t m_lid_to_base_lid_mapping[IB_LID_UCAST_END_HO + 1];
	vector<u_int16_t> m_sw_lids;

	LidMapping() { Clear(); }

	void Clear() {
		memset(m_lid_to_sw_lid_mapping, 0, sizeof(m_lid_to_sw_lid_mapping));
		memset(m_lid_to_base_lid_mapping, 0, sizeof(m_lid_to_base_lid_mapping));
		m_sw_lids.clear();
	}
};

struct PlftDefInfo {
	uint8_t lid_offset;
	uint8_t lid_space;
	uint8_t table_idx;
};

class ArAlgorithmSwData {

protected:
	OSMCalculatePortGroupsTask  m_calculate_task_;

	ARSWDataBaseEntry  &m_db_entry_;
	uint32_t            m_sw_idx_;

	RouteInfoVec        m_route_info_; 	/* sw_idx_ to route info; */
	ConnectionPtrVec    m_port_num_to_connection_tbl_;  /* Will be used for fast lookup in VL2VL */
	ConnectionVec       m_connection_pool_; /* gather connections together to increase performance */
	uint32_t            m_connections_number_;

public:

	ArAlgorithmSwData(ARSWDataBaseEntry &db_entry,
			  OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
		m_calculate_task_(p_port_groups_calculator),
		m_db_entry_(db_entry), m_sw_idx_(0),
		m_connections_number_(0) {}

	void Resize(uint32_t ports_num);

	RouteInfo &GetRouteInfo(ArAlgorithmSwData *p_dst_switch_data) {
		return m_route_info_[p_dst_switch_data->m_sw_idx_];
	}

	ARSWDataBaseEntry &GetSwDataBaseEntry() {
		return m_db_entry_;
	}

	int Init(AdaptiveRoutingAlgorithm &algorithm,
		 uint32_t sw_idx,
		 uint32_t switches_number);

	OSMThreadPoolTask *GetCalculateTask() {
		return &m_calculate_task_;
	}

	int SetConnectionsDirection(AdaptiveRoutingAlgorithm &algorithm);

	int GetSwIdx() const { return m_sw_idx_; }

	ConnectionVec &GetConnectionsVec() { return m_connection_pool_;}
	int GetConnectionsNumber() { return m_connections_number_;}

	const SwitchConnection *GetPortConnection(uint8_t port_num) const;
};

class ArAlgorithmBfsEntry {
public:
	enum bfs_status_t {
		BFS_INIT,
		BFS_QUEUED,
		BFS_DONE
	};
private:
	/* Meta-data for BFS traversing during routing */
	bfs_status_t        m_route_status_;
	uint16_t            m_route_distance_;
public:
	ArAlgorithmBfsEntry() :
		m_route_status_(BFS_INIT), m_route_distance_(0) {}

	void InitRouteInfo() {
		m_route_status_ = BFS_INIT;
		m_route_distance_ = 0;
	}

	void SetRouteStatus(bfs_status_t status) { m_route_status_ = status;}
	bfs_status_t GetRouteStatus() const { return m_route_status_;}

	void SetRouteDistance(uint16_t dist) { m_route_distance_ = dist;}
	uint16_t GetRouteDistance() const { return m_route_distance_;}
};

class ArAlgorithmBfsData {
	SwDbEntryPrtList			m_queue_;
	vector <ArAlgorithmBfsEntry> 		m_queue_data_;
	uint32_t				m_head_;
	uint32_t			 	m_tail_;
	uint8_t 				m_max_vlinc_to_switch_;

public:
	ArAlgorithmBfsData(uint32_t size) :
		m_queue_data_(size),
		m_head_(0),m_tail_(0), m_max_vlinc_to_switch_(0) {
	}

	SwDbEntryPrtList& GetQueue() { return m_queue_;}
	uint8_t &GetMaxVlIncreaseToSwitch() { return m_max_vlinc_to_switch_; }

	void InitRouteInfo() {
		uint32_t size = (uint32_t)m_queue_data_.size();
		for (uint32_t i = 0; i < size; ++i) {
			m_queue_data_[i].InitRouteInfo();
		}
	}

	ArAlgorithmBfsEntry &GetData(
		ArAlgorithmSwData &sw_data) {
		return m_queue_data_[sw_data.GetSwIdx()];
	}
};

struct SwitchConnection {
	/* If m_remote_switch == NULL this is a connection to all non sw ports */
	osm_switch_t   *m_remote_switch;
	PhysPortsList   m_ports; /* Elements of type, osm_physp_t* */
	PortsBitset     m_ports_bitset;
	PhysPortsListIter  m_next_port_for_routing;	/* Will be used for load-balancing DLIDs among ports */
	dim_index_t     m_dim_idx;
	dim_sign_t      m_dim_sign;

	SwitchConnection() :
		m_remote_switch(NULL), m_next_port_for_routing(m_ports.end()),
		m_dim_idx(0), m_dim_sign(1){}

	void Clear() {
		m_remote_switch = NULL;
		m_ports.clear();
		m_ports_bitset.reset();
		m_next_port_for_routing = m_ports.end();
		m_dim_idx = 0;
		m_dim_sign =1;
	}

	ARSWDataBaseEntry *GetRemoteSwitchDbEntry() {
		return(m_remote_switch ?
		       (ARSWDataBaseEntry *)(m_remote_switch->priv) :
		       NULL);
	}

	string ToString() const;

};

/*
 * Turn types description by algorithm
 *
 * KDOR:
 * 0 :  +-Low   -> +-High
 * 1 :  +-High  ->  +Low
 * 2 :  +-High  ->  -Low
 *
 * DFP:
 * 0: Regular, non VL inc required e.g. Up -> Down, Up -> Up etc.
 * 1: VL inc required. Down -> Up, Up to rank 0 -> Up to rank 0.
 */

enum ar_algorithm_turn_t {
	TURN_TYPE_0 = 0,
	TURN_TYPE_1,
	TURN_TYPE_2,
	TURN_TYPE_LAST
};

struct RouteInfo {
	SwitchConnection   *m_connection;
	uint8_t             m_vl_inc;    /* plane number */
	ar_algorithm_turn_t m_turn_type; /* First turn type that is used in the path */
	uint8_t             m_hops_count; /* Distance to destination */
	uint8_t             m_global_hops_count; /* DFP inter groups hops */

	/*
	 * Restricted route can be used only under specific conditions.
	 * Setting restricted and using it is algorithm specific.
	 *
	 * For example, on DFP:
	 * Restricted route is a route that requires VL increase and has
	 * down connection on the local group.
	 * Only root can use restricted route without any condition,
	 * others can use it only if it is the only route available.
	 */
	bool 		    m_restricted;
	/*
	 * alternative_connection indicates existence of same RouteInfo
	 * with connection to different switch but on the same direction
	 * and it used to avoid loopback
	 */
	bool		    m_alternative_connection;

private:
	static uint8_t turn_type_wait[TURN_TYPE_LAST];
public:

	RouteInfo() : m_connection(NULL), m_vl_inc(0),
		m_turn_type(TURN_TYPE_0),
		m_hops_count(0),
		m_global_hops_count(0),
		m_restricted(false),
		m_alternative_connection(false) {}

	void Init() {
		m_connection = NULL;
		m_vl_inc = 0;
		m_turn_type = TURN_TYPE_0;
		m_hops_count = 0;
		m_global_hops_count = 0;
		m_restricted = false;
		m_alternative_connection = false;
	}

	bool operator<(const RouteInfo& __rhs) const;
	bool operator==(const RouteInfo& __rhs) const;
};

struct DfSwSetup {
	ar_sw_t        m_sw_type;
	PortsBitset    m_up_ports;
	PortsBitset    m_down_ports;
	PortsBitset    m_end_ports; /*CAs, GW etc */

	DfSwSetup():m_sw_type(SW_TYPE_UNKNOWN){}

	void Clear() {
		m_sw_type = SW_TYPE_UNKNOWN;
		m_up_ports.reset();
		m_down_ports.reset();
		m_end_ports.reset();
	}
};

/*
 * TODO: only used by old dfp version implemented in OSMAdaptiveRoutingManager class
 * remove if old implementation is removed
 */
struct PLFTMads {
	set <u_int16_t> m_no_df_valid_route;
};

typedef vector < uint16_t > IslandGroupsVec;
/* [per_sw_group_index][island id][sequential index ]*/
typedef vector < vector < IslandGroupsVec > > IslandToGroupVecVecVec;

struct ArGroupPerIslandData {

	uint16_t 		m_max_island_number;
	uint8_t			m_max_groups_per_leaf;
	uint16_t 		m_max_ar_group_number;
	uint16_t		m_total_used_groups;
	uint16_t		m_total_leaves_groups;
	uint16_t		m_max_groups_per_island;
	/*
	 * m_island_to_group_id is used in order to preserve AR group number
	 * given to a specific group.
	 * Each <per_sw_group_index, island id> may be given different group numbers
	 * whenever link fails and different leaves in the group may be reached by
	 * different AR group ports.
	 * m_island_to_group_id[per_sw_group_index][island_id] is a vector of all
	 * different group numbers previously given to <per_sw_group_index, island id>
	 * m_island_to_group_id[per_sw_group_index][island_id][0] holds the last
	 * used index in the vector.
	 *
	 * dimension sizes: [per_sw_group_index][island id][sequential index]
	 * [1..groups_per_leaf+1][1..max_island_number+1][0.. m_max_groups_per_island]
	 */
	IslandToGroupVecVecVec	m_island_to_group_id;
	/*
	 * Ar Group number for local leaf switch
	 * The vector entry is the current group index out of m_groups_per_leaf
	 * m_leaf_sw_id_to_ar_group[per_sw_group_index]map<lid, group_id>
	 */
	LidToGroupNumberVecMap	m_leaf_sw_id_to_ar_group;

	vector < bool >		m_used_groups_id;
	vector < bool >		m_used_leaf_groups_id;

	ArGroupPerIslandData() :
		m_max_island_number(0),
		m_max_groups_per_leaf(0),
		m_max_ar_group_number(1), /* Reserve empty group 0 */
		m_total_used_groups(0), m_total_leaves_groups(0),
		m_max_groups_per_island(0) {}

	void Clear() {
		m_max_island_number = 0;
		m_max_groups_per_leaf = 0;
		m_max_ar_group_number = 1;
		m_total_used_groups = 0;
		m_total_leaves_groups = 0;
		m_max_groups_per_island = 0;
		m_island_to_group_id.clear();
		m_leaf_sw_id_to_ar_group.clear();
		m_used_groups_id.clear();
		m_used_leaf_groups_id.clear();
	}

	void Init(uint8_t groups_per_leaf,
		  uint16_t max_island_number,
		  uint16_t group_cap);

	void UseArGroup(uint16_t ar_group) {
		m_used_groups_id[ar_group] = true;
		m_total_used_groups++;
	}

	void UseLeafArGroup(uint16_t ar_group,
				uint16_t leaf_lid,
				uint8_t per_sw_group_index) {
		m_used_groups_id[ar_group] = true;
		m_used_leaf_groups_id[ar_group] = true;
		m_leaf_sw_id_to_ar_group[per_sw_group_index].
			insert(make_pair(leaf_lid, ar_group));
		m_total_used_groups++;
		m_total_leaves_groups++;
	}
};

struct DfSwData : public ArAlgorithmSwData {
	u_int16_t  m_df_group_number;
	u_int16_t  m_df_prev_group_number;
	PLFTMads   m_plft[MAX_PLFT_NUMBER];
	u_int8_t   plft_number;
	DfSwSetup  m_df_sw_setup[DF_DATA_LAST];
	uint8_t    m_rank;

	RouteInfoVec	m_alternative_route_info;  /* sw_idx_ to route info */

	/* Data used to calculate groups and LFTs */
	PSGroupsData m_algorithm_data;

	DfSwData(ARSWDataBaseEntry &db_entr,
		 OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
		ArAlgorithmSwData(db_entr, p_port_groups_calculator),
		m_df_group_number(INVALID_DFP_GROUP_NUMBER),
		m_df_prev_group_number(INVALID_DFP_GROUP_NUMBER),
		plft_number(0),
		m_rank(OSM_SW_NO_RANK) {}

	void ResizeAlternativeRouteInfo() {
		if (m_route_info_.size() > m_alternative_route_info.size()) {
			m_alternative_route_info.resize(m_route_info_.size());
		}
	}

	RouteInfo *GetAlternativeRouteInfo(ArAlgorithmSwData *p_dst_switch_data)
	{
		size_t sw_idx = p_dst_switch_data->GetSwIdx();
		if (m_alternative_route_info.size() <= sw_idx) {
			return NULL;
		}

		return &m_alternative_route_info[sw_idx];
	}

	void InitAlternativeRouteInfo()
	{
		RouteInfoVec::iterator iter = m_alternative_route_info.begin();
		for (; iter != m_alternative_route_info.end(); ++iter) {
			iter->Init();
		}
	}
};

class AdaptiveRoutingAlgorithm {
protected:
	osm_log_t                   *m_p_osm_log; /* Pointer to osm log */
	GuidToSWDataBaseEntry       &m_sw_map_;
	OSMAdaptiveRoutingManager   &m_ar_mgr_;

	supported_feature_t 	    m_algorithm_feature_;

	/* Temporary algorithm data */
	LidMapping 			m_lid_mapping_;

public:
	AdaptiveRoutingAlgorithm(osm_log_t *p_osm_log,
				 GuidToSWDataBaseEntry &sw_map,
				 OSMAdaptiveRoutingManager &ar_mgr,
				 supported_feature_t algorithm_feature) :
	m_p_osm_log(p_osm_log), m_sw_map_(sw_map),
	m_ar_mgr_(ar_mgr), m_algorithm_feature_(algorithm_feature) {}

	virtual ~AdaptiveRoutingAlgorithm() {}

	virtual ar_algorithm_t GetAlgorithm() = 0;
	virtual int Preprocess() = 0;
	virtual int RunCycle() = 0;

	virtual int Init() = 0;
	virtual void Clear() = 0;

	osm_log_t *GetLog() { return m_p_osm_log;}

	void AllocatePersistentARGroupIDs();
	int BuildLidMapping(LidMapping &lid_mapping);

	OSMThreadPoolTask *GetCalculateTask(ARSWDataBaseEntry &db_entry) {
		return GetArAlgorithmSwData(db_entry)->GetCalculateTask();
	}

	virtual void ClearAlgorithmConfiguration() = 0;

	virtual void CalculateSwitchPortGroups(ARSWDataBaseEntry &sw_db_entry,
					       LidMapping *p_lid_mapping,
					       LidToConnectionArray &lid_to_connections,
					       LidToPortArray &base_lid_to_port) = 0;

	virtual int GetSL2VL(osm_physp_t *osm_phys_port,
			     uint8_t iport_num, uint8_t oport_num,
			     uint8_t (*sl2vl)[IB_NUMBER_OF_SLS])
	{
		return -1;
	}

	virtual int SetDirection(osm_switch_t* src,
				 SwitchConnection &connection) = 0;

	virtual int BuildStaticRouteInfoToSwitch(
		ARSWDataBaseEntry &dst_db_entry,
		bool rebuild_route_info) = 0;

protected:

	bool IsFeatureActive(ARSWDataBaseEntry &sw_db_entry,
			     supported_feature_t m_feature)
	{
		return((sw_db_entry.m_support[SUPPORT_AR] == SUPPORTED) &&
		       (sw_db_entry.m_support[m_feature] == SUPPORTED) &&
		       (sw_db_entry.m_option_on));
	}

	virtual ArAlgorithmSwData *GetArAlgorithmSwData(ARSWDataBaseEntry &db_entry) const = 0;

	/* Calculate Groups and pLFTs */
	int CalculatePortGroups();

	void PrintRoute(osm_log_level_t level,
			ARSWDataBaseEntry &src_sw_db_entry,
			ARSWDataBaseEntry &dst_sw_db_entry,
			RouteInfo &route_info);

	virtual uint16_t GetGroupCap(ARSWDataBaseEntry &db_entry) {
		return db_entry.GetGroupCap();
	}

private:
	int SetHcaLidMapping(osm_physp_t *p_hca_physp,
			     osm_node_t *p_remote_sw_node,
			     LidMapping &lid_mapping);
};

} /* end of namespace OSM */

