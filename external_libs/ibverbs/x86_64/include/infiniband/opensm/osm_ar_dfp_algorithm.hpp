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
#include "opensm/osm_armgr.hpp"
#include "opensm/osm_ar_plft_based_algorithm.hpp"


namespace OSM {

#define DFP_DIM_INDEX_LOCAL	0
#define DFP_DIM_INDEX_REMOTE	1
#define DFP_DIM_SIGN_UP		1
#define DFP_DIM_SIGN_DOWN	-1

#define DFP_PLANES_NUMBER 2
/* VlCap 2 is : VL0, VL1 */
#define MIN_DFP_VL_CAP 2

#define PLFT_ID_FROM_UP_DIRECTION 0
#define PLFT_ID_FROM_DOWN_DIRECTION 1
#define PLFT_ID_FROM_DOWN_DIRECTION_VL1 2

class ArDfpAlgorithm : public PlftBasedArAlgorithm {

protected:

	/*TODO move from osm_armgr.hpp to this file*/
	AnalyzeDFSetupData	m_setup_data_;

	bool		m_df_configured_;
	bool		m_sw_info_configured_;
	uint16_t	m_max_df_group_number_;
	GuidToGroupMap	m_guid_to_dfp_group_;
	osm_dfp_intermediate_down_path	m_dfp_down_up_turns_mode_;

public:
	ArDfpAlgorithm(osm_log_t *p_osm_log,
		       GuidToSWDataBaseEntry &sw_map,
		       OSMAdaptiveRoutingManager &ar_mgr) :
		PlftBasedArAlgorithm(p_osm_log, sw_map, ar_mgr,
			     SUPPORT_DF,
			     DFP_PLANES_NUMBER, DFP_PLANES_NUMBER),
		m_df_configured_(false), m_sw_info_configured_(false),
		m_max_df_group_number_(0),
		m_dfp_down_up_turns_mode_(OSM_DFP_DOWN_PATH_ENABLED) {

		m_support_non_minimal_hops_ = true;

		m_turn_type_to_vl2vl_per_op_vls_[TURN_TYPE_0] = m_vl2vl_per_op_vls_;
		m_turn_type_to_vl2vl_per_op_vls_[TURN_TYPE_1] = m_inc_vl2vl_per_op_vls_;
	}

	virtual ~ArDfpAlgorithm(){}

	virtual int Preprocess();
	virtual int RunCycle();

	virtual void Clear() {}
	virtual int Init();
	virtual void ClearAlgorithmConfiguration();

	virtual ar_algorithm_t GetAlgorithm() { return AR_ALGORITHM_DFP2;}

	virtual void CalculateSwitchPortGroups(
		ARSWDataBaseEntry &sw_db_entry,
		LidMapping *p_lid_mapping,
		LidToConnectionArray &lid_to_connections,
		LidToPortArray &base_lid_to_port);

	virtual void CalculateRouteInfo(SwitchConnection &connection,
					RouteInfo &remote_route_info,
					RouteInfo &route_info);

	virtual int SetDirection(osm_switch_t* src,
				 SwitchConnection &connection);

protected:
	virtual ArAlgorithmSwData *GetArAlgorithmSwData(
		ARSWDataBaseEntry &db_entry) const { return db_entry.m_p_df_data; }

	virtual uint8_t GetMinimumVlCap() const { return MIN_DFP_VL_CAP; }

	/* Return false if build non minimal static routes not supported */
	virtual bool BuildNonMinimalStaticRouteInfo();

	virtual void CalculateNonMinimalRouteInfo(
		SwitchConnection &connection,
		ARSWDataBaseEntry &src_sw_db_entry,
		ARSWDataBaseEntry &dst_sw_db_entry,
		RouteInfo &route_info);

	/*
	 * If flip_from_connection is true, the real from_connection is opposite
	 * to the given one and there or has an opposite from dim_sign
	 */
	virtual ar_algorithm_turn_t GetTurnType(
		const SwitchConnection &from_connection,
		const SwitchConnection &to_connection,
		bool flip_from_connection) const;

	void CalculateRouteInfo(SwitchConnection &connection,
				ARSWDataBaseEntry &src_sw_db_entry,
				ARSWDataBaseEntry &dst_sw_db_entry,
				SwitchConnection *p_ingress_connection,
				RouteInfo &route_info);

	void BuildNonMinimalStaticRouteInfo(ARSWDataBaseEntry &sw_db_entry,
					    uint16_t &restricted_routes_number);
	void HandleRestrictedRouteInfo(ARSWDataBaseEntry &sw_db_entry);

	virtual uint8_t GetPlftsNumberForSwitch(ARSWDataBaseEntry &db_entry);

	virtual uint16_t GetGroupCap(ARSWDataBaseEntry &db_entry) {
		uint16_t groups_per_leaf;
		/*
		 * Divide GroupCap by groups per leaf.
		 * Each PLFT requires it's own group id.
		 */
		groups_per_leaf = GetPlftsNumberForSwitch(db_entry);
		return db_entry.GetGroupCap() / groups_per_leaf;
	}

private:
	bool SetCapable();

	int RootDetectionAnalyzeSetup();
	int AnalyzeSetup();
	int MarkLeafsByCasNumber(SwDbEntryPrtList &leafs_list);
	int DiscoverGroups(SwDbEntryPrtList &leafs_list,
			   BoolVec &used_group_numbers,
			   int cycle = 1);
	int MarkLeafsByGroupsNumber(SwDbEntryPrtList &leafs_list);
	int DiscoverLeaf(ARSWDataBaseEntry* p_db_entry);
	int DiscoverSpine(ARSWDataBaseEntry* p_db_entry);
	int SetLeaf(SwDbEntryPrtList &leafs_list,
		    osm_node_t *p_node,
		    bool add_to_list = true);
	int SetSpine(osm_node_t *p_node);
	void SetRanks();
	int SetPortsDirection();
	void SetGroupNumber(ARSWDataBaseEntry* p_sw_entry,
			    u_int16_t group_number);
	int SetPrevGroupNumber(ARSWDataBaseEntry* p_sw_entry,
			       BoolVec &used_group_numbers);
	int AnalyzeSetup(const char* root_guid_file);
	int MarkRanks(SwDbEntryPrtList &root_switches);
	int MarkIslands();

	void DumpAnalyzedSetup();

	void UpdateSmDbSwInfo();

	u_int8_t GetNextStaticPort(
		uint16_t *ports_load,
		PSPortsBitset &ps_group_bitmask,
		bool isHCA,
		u_int8_t num_ports);
	void PrintPSGroupData(const char * str,PSGroupData &group_data);

	int ARMapPLFTsAndSetSL2VLAct();

	void ARMapPLFTs(ARSWDataBaseEntry &sw_db_entry,
			u_int8_t port_block);

	u_int8_t GetMaxHops(DfSwData *local_sw_df_data,
			    DfSwData *dst_sw_df_data) const;

	u_int8_t GetMinHops(DfSwData *local_sw_df_data,
			    DfSwData *dst_sw_df_data,
			    uint8_t least_hops) const;


	/* Calculate Groups and pLFTs */
	int CalculateArGroups(ARSWDataBaseEntry &sw_db_entry,
			      vector<u_int16_t> &sw_lids,
			      int plft_id);

	/*
	 * Helper method used by CalculateArGroups
	 *
	 * Prints log message for missing route to destination
	 * Returns non zero value if the missing route is from leaf down port
	 */
	int HandleNoValidRouteToLid(ARSWDataBaseEntry &sw_db_entry,
				    int plft_id,
				    DfSwData *p_dst_switch_data,
				    uint16_t dst_sw_lid_num);
	/*
	 * Helper method used by CalculateArGroups
	 *
	 * Return true if the given connection can be used for valid route to
	 * destination.
	 * Returned out parameters:
	 * port_least_hops - Minimal hops to destination using this connection
	 * restricted - True if returned route is restricted
	 * ingress_connection - out parameter to the calculated
	 * 			ingress connection to the given sw_db_entry
	 * ingress_route_info - Route to destination using the calculated
	 *			ingress_connection and the given connection
	 *			from sw_db_entry
	 */
	bool GetValidRouteToDestination(ARSWDataBaseEntry &sw_db_entry,
					int plft_id,
					uint16_t destination_sw_lid_num,
					DfSwData *p_dst_switch_data,
					bool local_destination,
					SwitchConnection &connection,
					uint16_t max_hops,
					u_int8_t &port_least_hops,
					bool &restricted,
					SwitchConnection &ingress_connection,
					RouteInfo &ingress_route_info);

	void CalculateArPlfts(ARSWDataBaseEntry &sw_db_entry,
			      u_int16_t *lid_to_sw_lid_mapping,
			      uint8_t plft_id);

	/* Squash p_group_bitmask array of size 3 to PSGroupData */
	int UpdateGroupData(ARSWDataBaseEntry &sw_db_entry,
			    uint16_t lid_number,
			    uint8_t ports_number,
			    PortsBitset *p_group_bitmask,
			    bool use_only_minhop_path,
			    PSGroupData &ps_group_data,
			    u_int8_t *p_last_port_num);

};
} /* end of namespace OSM */

