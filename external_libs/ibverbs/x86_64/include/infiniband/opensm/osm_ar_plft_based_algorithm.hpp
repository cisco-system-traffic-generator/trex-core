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

#include "opensm/osm_ar_algorithm.hpp"

typedef vector <vector <bool> > BoolVecVec;

namespace OSM {

typedef uint8_t sl2vl_t[IB_NUMBER_OF_SLS];

class PlftBasedArAlgorithm : public AdaptiveRoutingAlgorithm {
protected:

	/*
	 * Planes number is derived from max VL increase per QoS
	 * 
	 * Planes are used to prevent credit loops by increasing VL
	 * to break a loop in the current plane.
	 * Each VL increase moves to a higher plane.
	 */
	const uint8_t   m_planes_number_;
	const uint8_t   m_min_vl_number_;
	bool		m_support_non_minimal_hops_; /* true if non minimal hops supported */

	/* sl2vl */
	sl2vl_t         m_sl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t		m_compact_sl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	uint16_t        m_en_sl_mask_; /* used to calculate m_slvl_per_op_vl_ */
	bool            m_update_sl2vl_;
	/* vl2vl */
	sl2vl_t         m_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t         m_inc_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t		m_sw2end_point_compact_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t		m_end_point2sw_compact_vl2vl_per_op_vls_[MAX_OP_VL_CODE+1];
	sl2vl_t		*m_turn_type_to_vl2vl_per_op_vls_[TURN_TYPE_LAST];

	uint8_t         m_max_vlinc_; /* max static routing vl increase */

public:
	PlftBasedArAlgorithm(osm_log_t *p_osm_log,
			     GuidToSWDataBaseEntry &sw_map,
			     OSMAdaptiveRoutingManager &ar_mgr,
			     supported_feature_t algorithm_feature,
			     uint8_t planes_number,
			     uint8_t min_vl_number) :
	AdaptiveRoutingAlgorithm(p_osm_log, sw_map, ar_mgr, algorithm_feature),
	m_planes_number_(planes_number),
	m_min_vl_number_(min_vl_number),
	m_support_non_minimal_hops_(false),
	m_en_sl_mask_(0),
	m_update_sl2vl_(false),
	m_max_vlinc_(0)
	{
		memset(m_sl2vl_per_op_vls_, 0, sizeof(m_sl2vl_per_op_vls_));
		memset(m_turn_type_to_vl2vl_per_op_vls_, 0,
		       sizeof(m_turn_type_to_vl2vl_per_op_vls_));
		BuildVl2VlPerOpVl();
	}

	virtual ~PlftBasedArAlgorithm() {}

	virtual int Preprocess() = 0;
	virtual int RunCycle() = 0;

	virtual int Init() = 0;
	virtual void Clear() = 0;

	virtual void ClearAlgorithmConfiguration() {}
	virtual void CalculateSwitchPortGroups(
		ARSWDataBaseEntry &sw_db_entry,
		LidMapping *p_lid_mapping,
		LidToConnectionArray &lid_to_connections,
		LidToPortArray &base_lid_to_port) = 0;

	virtual int GetSL2VL(osm_physp_t *osm_phys_port,
			     uint8_t iport_num, uint8_t oport_num,
			     uint8_t (*sl2vl)[IB_NUMBER_OF_SLS]);

protected:
	int GetSwitchSL2VL(osm_physp_t *osm_phys_port,
			   uint8_t iport_num,
			   uint8_t oport_num,
			   osm_node_t *p_node,
			   uint8_t (*sl2vl)[IB_NUMBER_OF_SLS]) const;

	virtual ArAlgorithmSwData *GetArAlgorithmSwData(
		ARSWDataBaseEntry &db_entry) const = 0;

	virtual uint8_t GetMinimumVlCap() const = 0;

	/*
	 * If flip_from_connection is true, the real from_connection is opposite
	 * to the given one and therefor has an opposite from dim_sign
	 */
	virtual ar_algorithm_turn_t GetTurnType(
		const SwitchConnection &from_connection,
		const SwitchConnection &to_connection,
		bool flip_from_connection) const = 0;

	void SetPlftConfiguration();
	int SetRequiredPLFTInfo(ARSWDataBaseEntry &db_entry);

	/*
	 * Update sw PrivateLFTDef and to_set_plft_def according to plft info
	 * and required plft number and size
	 */
	int SetPlftDef(ARSWDataBaseEntry &db_entry,
		       uint8_t bank_size_k,
		       uint8_t banks_number,
		       uint8_t plft_size_k,
		       uint8_t plfts_number);

	void CycleEnd(int rc);

	void SetPlftInfoProcess();
	void SetPlftDefProcess();
	void MapPlftsProcess();
	int PlftProcess();

	string SLToVLMappingToStr(const sl2vl_t &sl2vl) const;

	void BuildSl2VlPerOpVl(uint16_t en_sl_mask);

	int BuildStaticRouteInfo();

	void CalculateRouteInfo(SwitchConnection &connection,
				ArAlgorithmSwData *p_remote_switch_data,
				ArAlgorithmSwData *p_dst_switch_data,
				RouteInfo &route_info) {
		CalculateRouteInfo(connection,
				   p_remote_switch_data->GetRouteInfo(p_dst_switch_data),
				   route_info);
	}

	virtual void CalculateRouteInfo(SwitchConnection &connection,
					RouteInfo &remote_route_info,
					RouteInfo &route_info) = 0;

	/* Return false if build non minimal static routes not supported */
	virtual bool BuildNonMinimalStaticRouteInfo() { return false; }

	virtual void CalculateNonMinimalRouteInfo(
		SwitchConnection &connection,
		ARSWDataBaseEntry &src_sw_db_entry,
		ARSWDataBaseEntry &dst_sw_db_entry,
		RouteInfo &route_info) {}

	virtual int BuildStaticRouteInfoToSwitch(
		ARSWDataBaseEntry &dst_db_entry,
		bool rebuild_route_info);

	/* By default planes_number == plfts_number */
	virtual uint8_t GetPlftsNumberForSwitch(ARSWDataBaseEntry &db_entry) {
		return m_planes_number_;
	}

	void SetRoutingNotification(ARSWDataBaseEntry &sw_db_entry,
				    uint8_t consume_plft_id);
	void UpdateRNRcvString(ARSWDataBaseEntry &sw_db_entry,
			       uint8_t consume_plft_id);
	void UpdateRNXmitPortMask(ARSWDataBaseEntry &sw_db_entry);

private:
	int SetHcaLidMapping(osm_physp_t *p_hca_physp,
			     osm_node_t *p_remote_sw_node,
			     LidMapping &lid_mapping);

	void BuildVl2VlPerOpVl();

	int ProcessNeighborsBfs(ArAlgorithmBfsData &bfs_data,
				ARSWDataBaseEntry *p_sw_db_entry,
				ARSWDataBaseEntry *p_dst_db_entry,
				bool rebuild_route_info);

	/* Helper method used by ProcessNeighborsBfs */
	void SetRouteInfoToSwitch(ArAlgorithmBfsData &bfs_data,
				  ARSWDataBaseEntry *p_sw_db_entry,
				  ArAlgorithmSwData *p_local_switch_data,
				  ARSWDataBaseEntry *p_dst_db_entry,
				  ArAlgorithmSwData *p_dst_switch_data,
				  bool rebuild_route_info,
				  bool set_route_info,
				  RouteInfo &minimal_route_info);
};
} /* End of namespace OSM */
