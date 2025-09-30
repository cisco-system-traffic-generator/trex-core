/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2018-2020 Mellanox Technologies LTD. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#pragma once

#include <stdlib.h>
#include <iba/ib_types.h>

namespace OSM {

struct SwitchConnection;
typedef SwitchConnection *LidToConnectionArray[IB_LID_UCAST_END_HO + 1];
typedef	uint8_t LidToPortArray[IB_LID_UCAST_END_HO + 1];

class OSMThreadPoolTask {

	bool    m_check_result_;

public:

	OSMThreadPoolTask() : m_check_result_(false) {}
	/*
	 * Run should not throw for error handling,
	 * It should change the tasks state instead.
	 */
	virtual void Run() = 0;
	virtual ~OSMThreadPoolTask() {}

	void SetCheckResult(bool is_check_result) {
		m_check_result_ = is_check_result;
	}

	bool IsCheckResult() {
		return m_check_result_;
	}
};

struct ARSWDataBaseEntry;
struct SwitchConnection;
class OSMParallelPortGroupsCalculator;
class AdaptiveRoutingAlgorithm;

class OSMCalculatePortGroupsTreeTask : public OSMThreadPoolTask {

	ARSWDataBaseEntry                *m_sw_db_entry_;
	OSMParallelPortGroupsCalculator  *m_port_groups_calculator_;

public:
	OSMCalculatePortGroupsTreeTask(
		OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
	m_sw_db_entry_(),
	m_port_groups_calculator_(p_port_groups_calculator) {}

	virtual ~OSMCalculatePortGroupsTreeTask() {}

	void Init(ARSWDataBaseEntry *p_db_entry) {
		m_sw_db_entry_ = p_db_entry;
	}

	virtual void Run();
};

class OSMCalculatePortGroupsTask : public OSMThreadPoolTask {

	ARSWDataBaseEntry                *m_sw_db_entry_;
	OSMParallelPortGroupsCalculator  *m_port_groups_calculator_;
	AdaptiveRoutingAlgorithm         *m_ar_algorithm_;

	/* Temporary memory for Run */
	LidToConnectionArray 		m_dst_sw_lid_to_connection_;
	LidToPortArray			m_base_lid_to_port_;

public:
	OSMCalculatePortGroupsTask(
		OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
	m_sw_db_entry_(),
	m_port_groups_calculator_(p_port_groups_calculator),
	m_ar_algorithm_(NULL) {
		memset(m_dst_sw_lid_to_connection_, 0, sizeof(m_dst_sw_lid_to_connection_));
		memset(m_base_lid_to_port_, 0, sizeof(m_base_lid_to_port_));
	}

	virtual ~OSMCalculatePortGroupsTask() {}

	void Init(ARSWDataBaseEntry *p_db_entry,
			  AdaptiveRoutingAlgorithm *ar_algorithm) {
		m_sw_db_entry_ = p_db_entry;
		m_ar_algorithm_ = ar_algorithm;
	}

	virtual void Run();
};
} /* End of namespace OSM */

