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

#include <queue>

#include "opensm/osm_armgr_types.hpp"
#include "opensm/osm_ar_plft_based_algorithm.hpp"

#define MAX_KDOR_PLFT_NUMBER 4
#define MIN_KDOR_VL_CAP 3


/*
 * GHC coordinate is 64 BITs (4*16) representing up to 16 dimensions
 * Each dimension has 4 bits
 * The first dimension is 4 LSB bits
 */
#define BITS_PER_DIM 4
#define FIRST_DIM_BITS ((uint64_t)0xF)
/* dim is integer starting from 1 */
#define KDOR_DIM_MASK(dim) (FIRST_DIM_BITS << ((dim - 1) * BITS_PER_DIM))

#define KDOR_MAX_DIM 16

typedef uint8_t port_index_t;

namespace OSM {

struct CoordinateData;
struct KdorGroupData;

typedef set <u_int64_t> GuidSet;
typedef GuidSet::iterator GuidSetIter;
typedef pair <GuidSetIter, bool> GuidSetInsertRes;

typedef uint8_t dim_coord_t[KDOR_MAX_DIM+1];
typedef list <CoordinateData *> CoordinateDataList;

typedef map <u_int16_t, KdorGroupData> LidToKdorGroupDataMap;
typedef LidToKdorGroupDataMap::iterator LidToKdorGroupDataMapIter;
typedef LidToKdorGroupDataMap::const_iterator LidToKdorGroupDataMapConstIter;
typedef pair <LidToKdorGroupDataMapIter, bool> LidToKdorGroupDataMapInsertRes;

enum coordinate_state_t {
	COORDINATE_STATE_INIT,
	COORDINATE_STATE_SET
};

struct KdorGroupData {
	GroupData* group_data_array[MAX_KDOR_PLFT_NUMBER - 1];
};

struct KdorAlgorithmData : public SingleSubGroupGroupsData {

	LidToKdorGroupDataMap m_lid_to_kdor_group_map;

	/*
	 * Add lid to non empty group_data in ginen kdor_group_data
	 * And update lid_to_kdor_group_map
	 */
	void AddLidToKdorGroupData(uint16_t lid_num,
				   uint16_t sw_lid_num,
				   KdorGroupData &kdor_group_data,
				   bool is_new_group,
				   osm_log_t *p_osm_log);
private:

	void AddLidToKdorGroupData(uint16_t lid_num,
				   KdorGroupData &kdor_group_data);
};

struct CoordinateData {
	static uint16_t			sm_total_set_coordinates;

	ARSWDataBaseEntry 		&m_db_entry;
	GuidSet             		m_neighbors;
	CoordinateDataList		m_neighbors_data;
	coordinate_state_t		m_coordinate_state;
	uint64_t			m_coordinate;
	list <uint64_t>			m_neighbors_coordinate;
	uint8_t				m_neighbors_dim; /* set incase neighbors are on same dim*/

	CoordinateData(ARSWDataBaseEntry &db_entry) :
		m_db_entry(db_entry), m_coordinate_state(COORDINATE_STATE_INIT),
		m_coordinate(0), m_neighbors_dim(0) {}

	void Clear() {
		m_neighbors.clear();
		m_neighbors_data.clear();
		m_coordinate_state = COORDINATE_STATE_INIT;
		m_coordinate = 0;
		m_neighbors_coordinate.clear();
		m_neighbors_dim = 0;
	}

	bool IsCoordinateSet() {
		return m_coordinate_state == COORDINATE_STATE_SET;
	}

	int CountDimensions(uint64_t coordinate);

	uint8_t CountDimensions(uint64_t coordinate,
				uint8_t &first_dim);


	/* if yes returns dimension number else returns zero*/
	static uint8_t IsOneDimension(uint64_t coordinate);

	static uint64_t CoordinateIntersection(uint64_t dim_coordinate,
					       uint8_t dim,
					       uint64_t coordinate);

	void SetCoordinate(uint64_t coordinate);
	void DeleteCoordinate();

	int AddNeighborCoordinate(uint64_t coordinate);
	void RemoveNeighborCoordinate(uint64_t coordinate);
};

class KdorSwData : public ArAlgorithmSwData {
	/* Meta-data for coordinate calculation */
	CoordinateData      m_coordinate_data_;

public:
	KdorSwData(ARSWDataBaseEntry &db_entry,
		   OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
		ArAlgorithmSwData(db_entry, p_port_groups_calculator),
		m_coordinate_data_(db_entry) {}

	CoordinateData &GetCoordinateData() { return m_coordinate_data_; }
};

class ArKdorAlgorithm : public PlftBasedArAlgorithm {

protected:
	KdorSwData     *m_sw_lid_to_kdor_data_[IB_LID_UCAST_END_HO + 1];

	sl2vl_t     m_turn_1_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t     m_turn_2_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];

	ib_port_sl_to_plft_map_t m_plft_map_;

public:
	ArKdorAlgorithm(osm_log_t *p_osm_log,
			GuidToSWDataBaseEntry &sw_map,
			OSMAdaptiveRoutingManager &ar_mgr,
			supported_feature_t algorithm_feature,
			uint8_t planes_number,
			uint8_t min_vl_number) :
	PlftBasedArAlgorithm(p_osm_log, sw_map, ar_mgr,
			     algorithm_feature, planes_number, min_vl_number) {
		memset(m_sw_lid_to_kdor_data_, 0, sizeof(m_sw_lid_to_kdor_data_));
		BuildKdorVl2VlPerOpVl();
		BuildKdorPlftMap();
	}

	virtual ~ArKdorAlgorithm(){}

	virtual int Preprocess();
	virtual int RunCycle();

	virtual void Clear() {}
	virtual int Init();

	virtual int SetCoordinates() { return 0;}

	virtual void CalculateSwitchPortGroups(
		ARSWDataBaseEntry &sw_db_entry,
		LidMapping *p_lid_mapping,
		LidToConnectionArray &lid_to_connections,
		LidToPortArray &base_lid_to_port);

	virtual void CalculateRouteInfo(SwitchConnection &connection,
					RouteInfo &remote_route_info,
					RouteInfo &route_info);

	virtual int SetDirection(osm_switch_t* src,
				 SwitchConnection &connection) = 0;

	virtual bool IsValidCoordinate(ARSWDataBaseEntry &sw_db_entry);


protected:
	virtual ArAlgorithmSwData *GetArAlgorithmSwData(
		ARSWDataBaseEntry &db_entry) const { return db_entry.m_kdor_data; }

	virtual uint8_t GetMinimumVlCap() const { return MIN_KDOR_VL_CAP; }

	/*
	 * If flip_from_connection is true, the real from_connection is opposite
	 * to the given one and therefor has an opposite from dim_sign
	 */
	virtual ar_algorithm_turn_t GetTurnType(
		const SwitchConnection &from_connection,
		const SwitchConnection &to_connection,
		bool flip_from_connection) const;

private:
	bool SetCapable();

	/* Builds static unicast route info */
	int BuildStaticRouteInfo();

	/* Calculate Groups and pLFTs */
	void CalculateArGroups(ARSWDataBaseEntry &sw_db_entry,
			       u_int16_t *lid_to_sw_lid_mapping,
			       KdorAlgorithmData &algorithm_data);

	void CalculateArPlfts(ARSWDataBaseEntry &sw_db_entry,
			      LidMapping *p_lid_mapping,
			      KdorAlgorithmData &algorithm_data,
			      LidToConnectionArray &lid_to_connections,
			      LidToPortArray &base_lid_to_port);

	void CalculateArPlft(ARSWDataBaseEntry &sw_db_entry,
			     KdorAlgorithmData &algorithm_data,
			     u_int8_t ucast_lft_port,
			     unsigned dest_lid,
			     int plft_id,
			     LidToKdorGroupDataMapConstIter &ar_kdor_groups_iter,
			     OUT uint8_t &lid_state,
			     OUT uint16_t &ar_group_id);

	uint8_t GetStaticUcastLftPort(ARSWDataBaseEntry &sw_db_entry,
				      LidMapping *p_lid_mapping,
				      SwitchConnection *dst_sw_lid_to_connection[],
				      uint8_t base_lid_to_port[],
				      uint16_t dest_lid);

	/* Get static routing from the given switch */
	void BuildDstSwLidToConnection(ARSWDataBaseEntry &sw_db_entry,
				       SwitchConnection *dst_sw_lid_to_connection[]);

	void BuildKdorVl2VlPerOpVl();
	void BuildKdorPlftMap();

	void SetPlftMap();
	void SetPlftMap(ARSWDataBaseEntry &sw_db_entry);
};

#define HC_PLANES_NUMBER 4

class ArHcAlgorithm : public ArKdorAlgorithm {

public:

	ArHcAlgorithm(osm_log_t *p_osm_log,
		      GuidToSWDataBaseEntry &sw_map,
		      OSMAdaptiveRoutingManager &ar_mgr) :
	ArKdorAlgorithm(p_osm_log, sw_map, ar_mgr,
			SUPPORT_HC,
			HC_PLANES_NUMBER, HC_PLANES_NUMBER) {}

	virtual ~ArHcAlgorithm(){}

	virtual int SetDirection(osm_switch_t* src,
				 SwitchConnection &connection);

	virtual ar_algorithm_t GetAlgorithm() { return AR_ALGORITHM_KDOR_HC;}

};

class ArGhcAlgorithm : public ArKdorAlgorithm {

protected:
	dim_coord_t		m_max_dim_coord_;
	ARSWDataBaseEntry  	*m_origin_node_;

public:

	ArGhcAlgorithm(osm_log_t *p_osm_log,
		       GuidToSWDataBaseEntry &sw_map,
		       OSMAdaptiveRoutingManager &ar_mgr) :
	ArKdorAlgorithm(p_osm_log, sw_map, ar_mgr,
			SUPPORT_HC,
			HC_PLANES_NUMBER, HC_PLANES_NUMBER),
		m_origin_node_(NULL) {}

	virtual ~ArGhcAlgorithm(){}

	virtual int SetDirection(osm_switch_t* src,
				 SwitchConnection &connection);

	virtual bool IsValidCoordinate(ARSWDataBaseEntry &sw_db_entry);

	virtual ar_algorithm_t GetAlgorithm() { return AR_ALGORITHM_KDOR_GHC;}

	virtual int SetCoordinates();

private:

	void CoordinatesInit();

	int CoordinatesSetMainAxes(CoordinateDataList &main_planes);

	/*
	 * Set coordinates on main axis
	 * main_planes - OUT list of nodes on the main plane
	 * dim - IN/OUT the dimension of the main axis (dim >= 1)
	 *       The method can return a different dim if merge occurred
	 * coordinate_data - a node on the main axis
	 */
	int CoordinatesSetMainAxis(CoordinateDataList &main_planes,
				   uint8_t &dim,
				   CoordinateData &coordinate_data);

	/*
	 * Set coordinates on main axis
	 * Expand main axis by assigning main axis coordinate to neighbor of given nodes
	 * main_planes - OUT list of nodes on the main plane
	 * axis_queue - queue of CoordinateData of nodes with coordinates on the
	 *              main axis
	 * dim - the dimension of the main axis
	 */
	int CoordinatesSetMainAxis(CoordinateDataList &main_planes,
				   queue <CoordinateData *> &axis_queue,
				   uint8_t &dim);

	int CoordinatesSet(CoordinateDataList &main_planes);
	int SetNeighborsCoordinate(list <CoordinateData *> &new_coord_queue);

	/*
	 * Get the next free coordinate on main axis with dimension dim
	 */
	uint64_t GetNextCoordinate(uint8_t dim) {
		m_max_dim_coord_[dim]++;
		return ((uint64_t)m_max_dim_coord_[dim]) <<
			((dim - 1) * BITS_PER_DIM);
	}

	int AddNeighborMainAxisCoordinate(uint8_t axis_dim,
					  CoordinateData &coordinate_data,
					  uint64_t neighbor_coordinate);

	int MergeMainAxes(queue <CoordinateData *> &axis_queue,
			  CoordinateData *p_coordinate_data,
			  uint8_t &dim);

	int MergeMainAxesToDim(queue <CoordinateData *> &axis_queue,
			       CoordinateData *p_coordinate_data,
			       uint8_t to_dim);

	int MergeMainPlaneCoordinate(CoordinateDataList &main_planes,
				     CoordinateData *p_coordinate_data,
				     CoordinateData *p_conflict_neighbor);

	/*
	 * Validate node coordinate with respect to its neighbors
	 * If conflict found, return the conflicting neighbor
	 */
	CoordinateData *FindConflictingNeighbor(CoordinateData *p_coordinate_data);

	int ExpandDimension();
};

} /* end of namespace OSM */

