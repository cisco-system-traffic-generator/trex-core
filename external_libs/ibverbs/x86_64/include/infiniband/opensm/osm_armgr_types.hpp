/*                  - Mellanox Confidential and Proprietary -
 *
 *  Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *  Copyright (C) 2010-2020, Mellanox Technologies Ltd.  ALL RIGHTS RESERVED.
 *
 *  Except as specifically permitted herein, no portion of the information,
 *  including but not limited to object code and source code, may be reproduced,
 *  modified, distributed, republished or otherwise exploited in any form or by
 *  any means for any purpose without the prior written permission of Mellanox
 *  Technologies Ltd. Use of software subject to the terms and conditions
 *  detailed in the file "COPYING".
 *
 */


#pragma once


#include <stdlib.h>
#include <list>
#include <map>
#include <set>
#include <vector>
//#include <bitset>
using namespace std;

#include <opensm/osm_node.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_max_flow_algorithm.h>
#include <opensm/osm_advanced_routing.h>
#include <opensm/osm_ucast_mgr.h>

#include <opensm/osm_armgr_bitset.hpp>

#ifndef CLEAR_STRUCT
    #define CLEAR_STRUCT(n) memset(&(n), 0, sizeof(n))
#endif

#include <opensm/osm_calculate_port_groups_task.hpp>

//============================================================================//
// We have some HACKs because known bugs in fw.
#ifndef __AR_PORT_GROUP_BIT0_PORT1
    #define __AR_PORT_GROUP_BIT0_PORT1
#endif

#ifndef __AR_REVERSED_LFT_MAD
    #define __AR_REVERSED_LFT_MAD
#endif

#ifndef __AR_NEED_TO_ABORT_BY_MYSELF
    #define __AR_NEED_TO_ABORT_BY_MYSELF
#endif


//============================================================================//
// Some hacks for debug
//#define _DEBUG_AR_ONLY_ONE_CYCLE
//#define _DEBUG_AR_UPDATE_DB
//#define _DEBUG_AR_OSM_INTEGRATION
//#define _DEBUG_AR_OSM_INTEGRATION_BAD_FLOW
//#define _DEBUG_AR_OSM_INTEGRATION_BAD_FLOW2
//#define _DEBUG_AR_OSM_INTEGRATION_BAD_FLOW3


//============================================================================//
/* log defines */
#define OSM_AR_LOG( p_log, level, fmt, ...) {   \
        osm_log( p_log, level, "AR_MGR - " fmt, ## __VA_ARGS__); }
#define OSM_AR_LOG_ENTER( p_log )   \
        osm_log( p_log, OSM_LOG_FUNCS, "AR_MGR - %s: [\n", __func__);
#define OSM_AR_LOG_RETURN( p_log, rc ) {  \
        osm_log( p_log, OSM_LOG_FUNCS, "AR_MGR - %s: ]\n", __func__);   \
        return(rc); }
#define OSM_AR_LOG_RETURN_VOID( p_log ) {   \
        osm_log( p_log, OSM_LOG_FUNCS, "AR_MGR - %s: ]\n", __func__);   \
        return; }

#ifdef DEBUG
    #define AR_MGR_LOG(level, fmt, ...) TT_LOG(TT_LOG_MODULE_ARMGR, level, fmt, ## __VA_ARGS__);
    #define AR_MGR_ENTER TT_ENTER( TT_LOG_MODULE_ARMGR );
    #define AR_MGR_RETURN(rc) { TT_EXIT( TT_LOG_MODULE_ARMGR );    \
        return (rc); }
    #define AR_MGR_RETURN_VOID { TT_EXIT( TT_LOG_MODULE_ARMGR );   \
        return; }
#else           /* def DEBUG */
    #define AR_MGR_LOG(level, fmt, ...)
    #define AR_MGR_ENTER
    #define AR_MGR_RETURN(rc) return (rc);
    #define AR_MGR_RETURN_VOID return;
#endif          /* def DEBUG */


//============================================================================//
/* AR general defines */

/* FIT hack */
#ifdef PROD_TREE_ROOT
    #include <md_mod_reg.h>
#endif

//#define ARMGR_MAX_PORTS_SUPPORTED       63
#define ARMGR_MAX_GROUP_TABLES          8
#define ARMGR_SW_NOT_UPDATED            0
#define ARMGR_SW_UPDATED                1

#define AR_LFT_TABLE_BLOCK_SIZE         32
#define AR_LFT_TABLE_BLOCK_SIZE_SX      16
#define AR_LFT_TABLE_NUM_BLOCKS         1536
#define AR_LFT_TABLE_NUM_BLOCKS_SX      3072

#define AR_GROUP_TABLE_BLOCK_SIZE       2
#define AR_GROUP_TABLE_NUM_BLOCKS       1024
#define AR_GROUP_UNASSIGNED             UCAST_MGR_INVALID_GROUP_ID
#define AR_COPY_GROUP_BLOCK_SIZE        16

#define RN_SUB_GROUP_DIRECTION_BLOCK_SIZE 64
#define RN_SUB_GROUP_DIRECTION_NUM_BLOCKS 64
#define RN_SUB_GROUP_DIRECTION_UNASSIGNED 0xFF

#define RN_GEN_STRING_TABLE_BLOCK_SIZE 32
#define RN_RCV_STRING_BLOCK_SIZE 16
#define RN_XMIT_PORT_MASK_BLOCK_SIZE 128

#define LEAF_PLFT_NUMBER 2
#define SPINE_PLFT_NUMBER 2 //might need 3 for pelican
#define MIN_PLFT_NUMBER 2
#define MAX_PLFT_NUMBER 2 //might need 3 for pelican
#define MIN_PLFT_TABLE_SIZE 48
#define DLID_LOCATION_NUMBER 4
#define PATHS_NUMBER 3
#define DF_MAX_HOPS 6 //Max hops in DF path (Leaf SW to Leaf SW)
#define PLFT_LID_SPACE 96
#define MAX_SUPPORTED_RANK 16
#define INVALID_DFP_GROUP_NUMBER 0
#define MAX_DFP_RANK             1


#define DEFAULT_SMPS_WINDOW                         8                   /* maximum smp on wire */
#define DEFAULT_GMPS_WINDOW                         128                 /* maximum smp on wire */

#define AR_ENABLE_BY_VL_MASK 0x0055   //enable AR on VLs: 0,2,4,6

#define AR_VERSION_CAP_GROUP_TOP 0x02

#define RN_XMIT_PORT_MASK_GENERATE_ARN 0X1
#define RN_XMIT_PORT_MASK_GENERATE_FRN 0X2
#define RN_XMIT_PORT_MASK_PASS_ON_RN   0X4

/* VlCap for DFP is 2: VL0, VL1 */
#define DF_MIN_VL_CAP 2

//============================================================================//
/* new types - typedef */

typedef list < u_int16_t > LidsList;
typedef vector < u_int16_t > LidsVec;
typedef vector < pair < u_int16_t, LidsVec > > LidsToLidsVecVec;



//============================================================================//
/* new types - enums */
enum SMP_AR_ATTR_ID {
    AR_IB_ATTR_SMP_PLFT_INFO = 0xff10,                  /* PrivateLFTInfo attr */
    AR_IB_ATTR_SMP_PLFT_DEF = 0xff11,                   /* PrivateLFTDef attr */
    AR_IB_ATTR_SMP_PLFT_MAP = 0xff12,                   /* PrivateLFTMap attr */
    AR_IB_ATTR_SMP_PORT_SL_TO_PLFT_MAP = 0xff14,        /* PortSLToPrivateLFTMapT attr */
    AR_IB_ATTR_SMP_AR_INFO = 0xff20,                    /* AdaptiveRoutingInfo attr */
    AR_IB_ATTR_SMP_AR_GROUP_TABLE = 0xff21,             /* AdaptiveRoutingGroupTable attr */
    AR_IB_ATTR_SMP_AR_LINEAR_FORWARDING_TABLE = 0xff22, /* AdaptiveRoutingLinearForwardingTable attr */
    AR_IB_ATTR_SMP_AR_LINEAR_FORWARDING_TABLE_SX = 0xff23, /* AdaptiveRoutingLinearForwardingTableSX attr */
    AR_IB_ATTR_SMP_EXTENDED_SWITCH_INFO = 0xff91,       /* ExtendedSwitchInfo attr */
    AR_IB_ATTR_SMP_RN_GEN_STRING_TABLE  = 0xffb8,       /* RNGenStringTable attr */
    AR_IB_ATTR_SMP_RN_RCV_STRING = 0xffb9,              /* RNRcvString attr */
    AR_IB_ATTR_SMP_RN_SUB_GROUP_DIRECTION_TABLE = 0xffba, /* RNSubGroupDirectionTable attr */
    AR_IB_ATTR_SMP_RN_XMIT_PORT_MASK = 0xffbc,          /* RNXmitPortMask attr */
    AR_IB_ATTR_SMP_AR_GROUP_TABLE_COPY = 0xffbd,        /* AdaptiveRoutingGroupTableCopy attr */
    AR_IB_ATTR_SMP_RN_GEN_BY_SUB_GROUP_PRIORITY = 0xffbe, /* RNGenBySubGroupPriority attr */
    AR_IB_ATTR_SMP_LAST
};

//#define AR_IB_ATTR_VS_PORT_RN_COUNTERS 0x0082

enum supported_feature_t {
    SUPPORT_AR = 0, //AR or FR
    SUPPORT_DF,
    SUPPORT_RN,
    SUPPORT_HC,
    SUPPORT_LAST
};

enum support_status_t {
    SUPPORT_UNKNOWN,
    NOT_SUPPORTED,
    SUPPORTED
};

enum support_errs_t {
    NO_ERR,
    FABRIC_ERR,
    DB_ERR,
    AR_DEVICE_ID_NOT_SUPPORTED,
    AR_MAX_PORTS_EXCEEDED,
    AR_GROUP_LESS_THAN_PORTS,
    AR_GROUP_CAP_ZERO,
    DF_NO_AR_BY_SL_CAP,
    DF_GLOBAL_AR_GROUP,
    DF_NO_VL2VL_CAP,
    DF_NO_SL_MAP_CAP,
    DF_PLFT_CAP_ZERO,
    DF_PLFT_CAP_LESS_THAN_MIN,
    DF_INVALID_PLFT_TOP,
    DF_VL_CAP_LESS_THAN_MIN
};

/*
enum df_support_status_t {
    DF_SUPPORT_UNKNOWN,
    DF_NOT_SUPPORTED,
    DF_SUPPORTED
};

enum df_support_errs_t {
    DF_NO_ERR,

    DF_FABRIC_ERR
};

enum rn_support_status_t {
    RN_SUPPORT_UNKNOWN,
    RN_NOT_SUPPORTED,
    RN_SUPPORTED
};

enum rn_support_errs_t {
    RN_NO_ERR,
    RN_FABRIC_ERR
};
*/

enum ar_algorithm_t {
    AR_ALGORITHM_LAG,
    AR_ALGORITHM_TREE,
    AR_ALGORITHM_DF_PLUS,
    AR_ALGORITHM_DFP2,
    AR_ALGORITHM_KDOR_HC,
    AR_ALGORITHM_KDOR_GHC,
    AR_ALGORITHM_MINHOP,
    AR_ALGORITHM_UNKNOWN
};


enum ar_sw_t {
    SW_TYPE_UNKNOWN,
    SW_TYPE_LEAF,
    SW_TYPE_SPINE
};

enum ar_dlid_location_t {
    LEAF_SAME_GROUP,
    LEAF_REMOTE_GROUP,
    SPINE_SAME_GROUP,
    SPINE_REMOTE_GROUP
};

enum path_validation_t {
    PATH_VALIDATION_NON,
    //only if there is valid route on remote
    PATH_VALIDATION_REMOTE,
    //only if next remote supports DF
    PATH_VALIDATION_REMOTE_DF,
    //only up ports plus validate route on remote
    PATH_VALIDATION_REMOTE_UP,
    //only down ports
    PATH_VALIDATION_DOWN,
    //only down ports plus validate tru hops number (not using same link)
    PATH_VALIDATION_HOPS_DOWN
};

enum ar_to_ar_entry_t {
    AR_TO_AR_ENTRY_NA = 0,
    AR_TO_AR_ENTRY_SAME,
    AR_TO_AR_ENTRY_INC
};

enum df_data_new_old_t {
    NEW_DATA = 0,
    OLD_DATA,
    DF_DATA_LAST
};

enum AR_OPERATION_MODE {
    AR_OP_MODE_ENABLE_SPINE_TO_SPINE_NONE_DF_ROUTE = 0x1,
    AR_OP_LAST
};

enum RN_RCV_STRING_DECISION {
    RN_RCV_STRING_DISCARD = 0,
    RN_RCV_STRING_CONSUME_ARN = 1,
    RN_RCV_STRING_CONSUME_ALL = 2,
    RN_RCV_STRING_PASS_ON = 3
};


//============================================================================//
/* AR configuration defines */
#define AR_CFG_DEFAULT_LOG_FILE_NAME    "/var/log/armgr.log"
#define AR_CFG_DEFAULT_LOG_FILE_SIZE    5
#define AR_CFG_DEFAULT_ENABLE           true
#define AR_CFG_DEFAULT_AR_ENABLE        true
#define AR_CFG_DEFAULT_ARN_ENABLE       false
#define AR_CFG_DEFAULT_FLFR_ENABLE      false //fast link fault recovery
#define AR_CFG_DEFAULT_FLFR_REMOTE_DISABLE false
#define AR_CFG_DEFAULT_PFRN_ENABLE	false
#define AR_CFG_DEFAULT_MAX_ERRORS       5
#define AR_CFG_DEFAULT_ERROR_WINDOW     5
#define AR_CFG_DEFAULT_ALGORITHM_STR	"TREE"
#define AR_CFG_DEFAULT_MAX_TIMEOUT_NUM  10
#define AR_CFG_DEFAULT_EN_SL_MASK       0x0 //(no SL supports AR)
#define AR_CFG_DEFAULT_DFP_EN_VL_MASK   0xFFFC //(VL0 and VL1 o not support AR)

#define AR_CFG_DEFAULT_SW_AR_MODE       OSM_AR_IB_LID_STATE_FREE
#define AR_CFG_DEFAULT_SW_AGEING_TIME   0
#define AR_CFG_DEFAULT_SW_ENABLE        true

#define AR_CFG_INPUT_FILE_NAME          "/etc/opensm/ar_mgr.conf"
#define AR_CFG_INPUT_FILE_FLAG          "armgr --conf_file"

#define AR_CFG_SW_OPT_STR_ENABLE        "option_enable"
#define AR_CFG_SW_OPT_STR_AGEING        "option_ageing"


//============================================================================//
namespace OSM
{
typedef map<u_int64_t, ARSWDataBaseEntry> GuidToSWDataBaseEntry;
typedef GuidToSWDataBaseEntry::iterator GuidToSWDataBaseEntryIter;
typedef std::pair< GuidToSWDataBaseEntryIter, bool > GuidToSWDataBaseEntryInsertRes;

inline static string ConvertARIBLidStateToStr(IN u_int8_t lid_state)
{
    switch (lid_state) {
    case OSM_AR_IB_LID_STATE_BOUNDED:
        return "Bounded";
    case OSM_AR_IB_LID_STATE_FREE:
        return "Free";
    case OSM_AR_IB_LID_STATE_STATIC:
        return "Static";
    default:
        return "Unknown";
    }
}

inline static string ConvertBitMaskToStr(IN u_int64_t mask)
{
    char buff[1024];
    string str = "";
    for (unsigned port_num = 1; port_num < 64; ++port_num) {
        if (mask & (1ull << port_num)) {
            sprintf(buff, "%u,", port_num);
            str += buff;
        }
    }
    if (str != "")
        str.erase(str.end() - 1, str.end());
    return (str);
}

inline static u_int64_t FixGroupBitmask(u_int64_t bit_mask)
{
    u_int64_t res = bit_mask;
#ifdef __AR_PORT_GROUP_BIT0_PORT1
    res = res >> 1;
#endif
    return res;
}

inline static u_int8_t FixLFTEntryNum(u_int8_t entry_num)
{
    u_int8_t res = entry_num;
#ifdef __AR_REVERSED_LFT_MAD
    res ^= 1;
#endif
    return res;
}


inline uint8_t OsmSwitchGetPortByLidUseNewLft(IN const osm_switch_t * p_sw,
        IN uint16_t lid_ho)
{
    if (lid_ho == 0 || lid_ho > p_sw->max_lid_ho)
        return OSM_NO_PATH;
    return p_sw->new_lft[lid_ho];
}

typedef map <uint16_t, uint16_t> LidToGroupNumberMap;
typedef LidToGroupNumberMap::iterator LidToGroupNumberMapIter;
typedef LidToGroupNumberMap::const_iterator LidToGroupNumberMapConstIter;
//typedef pair <LidToGroupNumberMapIter, bool> LidToGroupNumberMapInsertRes;

class KdorSwData;

struct BaseGroupData {
    BaseGroupData():
	    m_group_size(0), m_group_number(AR_GROUP_UNASSIGNED),
	    m_group_table_number(ARMGR_MAX_GROUP_TABLES),
	    m_group_numbers(UCAST_MGR_MAX_PLFT, vector<uint16_t>(0)),
	    m_lid_per_plft(UCAST_MGR_MAX_PLFT, vector<uint16_t>(0)) {}

    BaseGroupData(uint8_t group_size):
	    m_group_size(group_size), m_group_number(AR_GROUP_UNASSIGNED),
	    m_group_table_number(ARMGR_MAX_GROUP_TABLES),
	    m_group_numbers(UCAST_MGR_MAX_PLFT, vector<uint16_t>(0)),
	    m_lid_per_plft(UCAST_MGR_MAX_PLFT, vector<uint16_t>(0)) {}

    ~BaseGroupData(){}

    virtual void Clear() {
        m_lids_list.clear();
        m_sw_lids_list.clear();
        m_leaf_switches.clear();
	ClearVector(m_group_numbers);
	ClearVector(m_lid_per_plft);
        m_group_size = 0;
        m_group_number = AR_GROUP_UNASSIGNED;
        m_group_table_number = ARMGR_MAX_GROUP_TABLES;
    }

    /* Update all GroupElements in the switch group table */
    virtual void UpdateSWGroupElement(osm_log_t *p_osm_log,
				      ARSWDataBaseEntry &sw_db_entry,
				      uint16_t group_number,
				      osm_ar_group_set_type_t ar_group_set) = 0;

    virtual std::ostream &BitsetToOstream(std::ostream& __os) const = 0;

    virtual bool operator<(const BaseGroupData& __rhs) const = 0;

    vector<uint16_t>  m_lids_list;
    vector<uint16_t>  m_sw_lids_list;
    uint8_t         m_group_size;
    uint16_t        m_group_number;
    uint8_t         m_group_table_number;
    //groups per leaf switch
    vector<uint16_t>   m_leaf_switches;
    /*
     * Group numbers associated with this group (used for routing notifications)
     * Note that for DFP2, group IDs are per <LID, PLFT>
     */
    vector<vector<uint16_t> >   m_group_numbers;
    /*
     * Destination LIDs that this group (set of exit ports) serve.
     * Vector size UCAST_MGR_MAX_PLFT. Each entry holds a vector of LIDs used by that PLFT
     * for exit ports of this groups. (used by DFP2 in order to set group ID for this group)
     *
     * Note: we can have different set of LIDs for different PLFTs, since the set of ports
     * varies between different parts (PLFTs) of path towards the same destination.
     */
    vector<vector<uint16_t> >   m_lid_per_plft;

    void inline ClearVector(vector<vector<uint16_t> > vec)
    {
	for (size_t i = 0; i < vec.size(); i++)
	    vec[i].clear();
    }

};

struct SingleSubGroupPerGroupData : public BaseGroupData {

	PortsBitset	m_group_bitmask;

	SingleSubGroupPerGroupData() : BaseGroupData(), m_group_bitmask() {}

	SingleSubGroupPerGroupData(const PortsBitset &group_bitmask,
				   uint8_t group_size) :
	    BaseGroupData(group_size), m_group_bitmask(group_bitmask) {}

	~SingleSubGroupPerGroupData() {}

	virtual void Clear() {
		BaseGroupData::Clear();
		m_group_bitmask.reset();
	}

	virtual void UpdateSWGroupElement(osm_log_t *p_osm_log,
					  ARSWDataBaseEntry &sw_db_entry,
					  uint16_t group_number,
					  osm_ar_group_set_type_t ar_group_set);

	virtual std::ostream &BitsetToOstream(std::ostream& __os) const {
		return m_group_bitmask.to_ostream(__os);
	}

	virtual bool operator<(const BaseGroupData& __rhs) const {
		return m_group_bitmask <
			((SingleSubGroupPerGroupData &)__rhs).m_group_bitmask;
	}
};

typedef SingleSubGroupPerGroupData GroupData;

struct GroupTableData {
    GroupTableData():
        m_next_group_number(0){}

    void Clear() {
        m_assign_bitmask.reset();
        m_next_group_number = 0;
    }

    PortsBitset m_assign_bitmask;
    uint16_t    m_next_group_number;
};

//TODO use mem pool for GroupData *
typedef map <const PortsBitset, GroupData, PortsBitsetLstr> GroupBitmaskToDataMap;
typedef GroupBitmaskToDataMap::iterator GroupBitmaskToDataMapIter;
typedef GroupBitmaskToDataMap::const_iterator GroupBitmaskToDataMapConstIter;
typedef pair <GroupBitmaskToDataMapIter, bool> GroupBitmaskToDataMapInsertRes;

/*
 * Map from LID to GroupData
 * Data structure does not support deleting mappings
 */
class LidToGroupDataMap {
    vector<BaseGroupData*>  map;
    bool is_empty;

public:
    LidToGroupDataMap(): map(IB_LID_UCAST_END_HO+1,NULL), is_empty(true) {}

    void clear() {
        map.clear();
        map.resize(IB_LID_UCAST_END_HO+1);
        is_empty = true;
    }

    bool empty() {
        return is_empty;
    }

    void set(uint16_t lid, BaseGroupData *value) {
        if (is_empty)
            is_empty = false;

        map[lid] = value;
    }

    BaseGroupData *get(uint16_t lid) {
        return map[lid];
    }
};

typedef list <BaseGroupData*> GroupsList;
typedef GroupsList::iterator GroupsListIter;
typedef GroupsList::const_iterator GroupsListConstIter;
typedef vector <BaseGroupData*> GroupsPtrVec;

typedef list <class ARSWDataBaseEntry*> SwDbEntryPrtList;

/*
 * Context for OSMAdaptiveRoutingManager::ARCalculateSwitchPortGroupsTree
 * Contains precalculated information that used by AR tree port group
 * calculation algorithm.
 */
struct TreeAlgorithmContext {
    uint16_t m_hca_to_sw_lid_mapping[IB_MAX_NUM_OF_PLANES_XDR + 1][IB_LID_UCAST_END_HO + 1];
    uint8_t  m_sw_lid_to_rank_mapping[IB_LID_UCAST_END_HO + 1];
    uint8_t  m_max_rank;
    flow_graph_t *p_graph;
    vector< vector<uint16_t> > m_sw_to_lids;
    vector<uint16_t> m_sw_lids;

    TreeAlgorithmContext() {
        m_max_rank = 0;
        memset(m_hca_to_sw_lid_mapping, 0, sizeof(m_hca_to_sw_lid_mapping));
        memset(m_sw_lid_to_rank_mapping, 0xFF, sizeof(m_sw_lid_to_rank_mapping));
        m_sw_to_lids.resize(IB_LID_UCAST_END_HO + 1);
        p_graph = NULL;
    }
};

struct GroupsData {

    GroupsPtrVec          m_groups;
    LidToGroupDataMap     m_lid_to_group_map;
    GroupTableData        m_assign_group_table[ARMGR_MAX_GROUP_TABLES];
    GroupsList            m_assign_groups; /* if config_rn use m_groups */
    vector<uint16_t>      m_sw_lid_to_group_number;
    unsigned              m_next_group_number;
    /*
     * m_next_group_number initial value is 1 if FR enabled, else it is 0
     * Setting m_next_group_number to its initial value is done before allocating
     * groups number, if  m_reset_next_group_number is true.
     */
    bool                  m_reset_next_group_number;
    /* Number of required groups per leaf, if group per leaf is applied */
    uint8_t               m_groups_per_leaf;
    /* The current group index to be allocated out of m_groups_per_leaf */
    uint8_t               m_per_sw_group_index;


    GroupsData() : m_sw_lid_to_group_number(IB_LID_UCAST_END_HO + 1),
	    m_next_group_number(0),
	    m_reset_next_group_number(true),
	    m_groups_per_leaf(1),
	    m_per_sw_group_index(1) {
        Clear();
    }

    virtual ~GroupsData() {}

    virtual void Clear() {
        size_t capacity = m_groups.capacity();
        m_groups.clear();
        m_groups.reserve(capacity);
        m_lid_to_group_map.clear();
        m_assign_groups.clear();
        for (int i = 0 ; i < ARMGR_MAX_GROUP_TABLES ; i++)
            m_assign_group_table[i].Clear();
        fill(m_sw_lid_to_group_number.begin(), m_sw_lid_to_group_number.end(),
	     AR_GROUP_UNASSIGNED);
        m_per_sw_group_index = 1;
        m_next_group_number = 0;
        m_reset_next_group_number = true;
    }

    /*
     * SetCalculateNextGroupIndex should be called after m_per_sw_group_index
     * or per island was calculated for all switches.
     * All lft tables using those groups
     * were updated and before starting calculate groups for the next group index
     */
    virtual void SetCalculateNextGroupIndex() {
	    m_lid_to_group_map.clear();
	    m_per_sw_group_index++;
    }
};

struct SingleSubGroupGroupsData : public GroupsData {

	GroupBitmaskToDataMap	m_groups_map;

	SingleSubGroupGroupsData() : GroupsData() {}
	virtual ~SingleSubGroupGroupsData() {}

	virtual void Clear() {
		GroupsData::Clear();
		m_groups_map.clear();
	}
};

struct PSGroupData : public BaseGroupData {

	PSPortsBitset   m_ps_group_bitmask;
	u_int8_t        m_port_num; /* if m_group_size == 1 use m_port_num */

	PSGroupData() : BaseGroupData(),
		m_ps_group_bitmask(), m_port_num(0){}

	~PSGroupData() {}

	virtual void Clear() {
		BaseGroupData::Clear();
		m_ps_group_bitmask.reset();
		m_port_num = 0;
	}

	virtual void UpdateSWGroupElement(osm_log_t *p_osm_log,
					  ARSWDataBaseEntry &sw_db_entry,
					  uint16_t group_number,
					  osm_ar_group_set_type_t ar_group_set);

	virtual std::ostream &BitsetToOstream(std::ostream& __os) const {
		return m_ps_group_bitmask.to_ostream(__os);
	}

	virtual bool operator<(const BaseGroupData& __rhs) const {
		return m_ps_group_bitmask <
			((PSGroupData &)__rhs).m_ps_group_bitmask;
	}

};

typedef map <const PSPortsBitset, PSGroupData, PSPortsBitsetLstr> PSGroupBitmaskToDataMap;
typedef PSGroupBitmaskToDataMap::iterator PSGroupBitmaskToDataMapIter;
typedef PSGroupBitmaskToDataMap::const_iterator PSGroupBitmaskToDataMapConstIter;
typedef pair <PSGroupBitmaskToDataMapIter, bool> PSGroupBitmaskToDataMapInsertRes;

typedef map <u_int16_t, PSGroupData*> LidToPSGroupDataMap;
typedef LidToPSGroupDataMap::iterator LidToPSGroupDataMapIter;
typedef LidToPSGroupDataMap::const_iterator LidToPSGroupDataMapConstIter;
typedef pair <LidToPSGroupDataMapIter, bool> LidToPSGroupDataMapInsertRes;

typedef list <PSGroupData*> PSGroupsList;
typedef PSGroupsList::iterator PSGroupsListIter;
typedef PSGroupsList::const_iterator PSGroupsListConstIter;


struct PSGroupsData : public GroupsData {

	PSGroupBitmaskToDataMap m_ps_groups_map;

	PSGroupsData() : GroupsData() {}

	virtual ~PSGroupsData() {}

	virtual void Clear() {
		GroupsData::Clear();
		m_ps_groups_map.clear();
	}
};

//============================================================================//
/* new types - structs */
// AR Node Info
struct ARGeneralSWInfo {
	u_int64_t m_guid;
	u_int16_t m_lid;
	u_int8_t  m_num_ports; //num of phys ports, ports number not including port 0
	osm_switch_t  *m_p_osm_sw;
	ARGeneralSWInfo(u_int64_t guid, u_int16_t lid,
			unsigned num_ports,
			osm_switch_t *p_osm_sw = NULL/*JL osm_dr_path_t *p_osm_path = NULL*/) : m_guid(guid), m_lid(lid),
	m_num_ports(num_ports),
	m_p_osm_sw(p_osm_sw) {
	}
};

class LidPortMappingPtrBase {
public:
	virtual ~LidPortMappingPtrBase() {}
	virtual void SetData(u_int8_t lid_state, u_int8_t table_number,
			u_int16_t group_number, u_int8_t ucast_lft_port) = 0;
	virtual u_int16_t GetGroupNumber() = 0;
	virtual u_int8_t GetTableNumber() = 0;
	virtual u_int8_t GetLidState() = 0;
};

class LidPortMappingPtrSX : public LidPortMappingPtrBase {
public:
	virtual ~LidPortMappingPtrSX(){}
	void SetPtr(ib_ar_lft_entry_t *lid_entry) {m_lid_entry = lid_entry;}
	void SetData(u_int8_t lid_state, u_int8_t table_number,
			u_int16_t group_number, u_int8_t ucast_lft_port) {
		OSM_ASSERT(table_number == 0);
		m_lid_entry->state_table = (lid_state << 6) | (table_number & 0x0F);
		m_lid_entry->group_number = cl_ntoh16(group_number);
		m_lid_entry->legacy_port = ucast_lft_port;
	}
	virtual u_int16_t GetGroupNumber() {
		return cl_hton16(m_lid_entry->group_number);
	}
	virtual u_int8_t GetTableNumber() {
		return m_lid_entry->state_table & 0x0F;
	}
	virtual u_int8_t GetLidState() {
		return m_lid_entry->state_table >> 6;
	}

private:
	ib_ar_lft_entry_t *m_lid_entry;
};

class OSMCalculatePortGroupsTreeTask;
struct DfSwData;

struct CopyFromToGroups {
	u_int16_t m_copy_from_group;
	bool      m_copy_direction;
	int init() {
		//m_copy_from_group = 0;
		//memset(&m_copy_to, 0, sizeof(CopyToGroups));
		return 0; //for MemoryPool usage
	}
};

// SW AR Settings Data Base
struct ARSWDataBaseEntry {
	ARGeneralSWInfo m_general_sw_info;
	support_status_t m_support[SUPPORT_LAST];
	support_errs_t   m_error[SUPPORT_LAST];
	bool in_temporary_error;
	u_int16_t temporary_error_num;
	bool m_osm_update_needed;
	u_int8_t m_app_data;

	bool m_option_on;
	u_int32_t m_ageing_time_value; //configured AgeingTimeValue
	ib_ar_info_t m_ar_info; /* TODO remove and use only p_sw->ar_data.ar_info */
	ib_ar_info_t m_required_ar_info; /* TODO remove and use only p_sw->ar_data.ar_info */

	u_int16_t m_max_lid;
	u_int16_t m_group_top;
	bool m_to_set_group_table[AR_GROUP_TABLE_NUM_BLOCKS];
	//since we do not read the group_table configuration from switches
	//the block become valid only after we set it on the first time
	bool m_is_group_table_valid[AR_GROUP_TABLE_NUM_BLOCKS];
	bool m_to_set_lft_table[AR_LFT_TABLE_NUM_BLOCKS_SX];

	//DragonFly+
	DfSwData * m_p_df_data;
	KdorSwData  *m_kdor_data;

	// Tree
	// Indicator that the SW is a leaf (all the neighbors has lower rank)
	bool m_is_leaf;

	// Number of CAs connected to the SW (Not counting Aggregation Nodes)
	int m_num_cas;

	/*
	 * For trees, TRUE when ARN/FRN are enabled
	 * However for DFP, TRUE when ARN/FRN/pFRN are enabled
	 */
	bool m_config_rn;

	u_int16_t                       m_rn_gen_string;
	bool                            m_to_set_rn_rcv_string;
	bool                            m_to_set_rn_xmit_port_mask;
	list<CopyFromToGroups*>         m_copy_from_to_group_list;

	OSMCalculatePortGroupsTreeTask  m_calculate_port_groups_tree_task;

	void SetARGeneralSWInfo(const ARGeneralSWInfo& general_sw_info) {
		this->m_general_sw_info = general_sw_info;
	}

	ARSWDataBaseEntry(const ARGeneralSWInfo& general_sw_info,
			OSMParallelPortGroupsCalculator *p_port_groups_calculator) :
		m_general_sw_info(general_sw_info),
		in_temporary_error(false), temporary_error_num(0),
		m_osm_update_needed(true), m_app_data(0),
		m_option_on(AR_CFG_DEFAULT_SW_ENABLE),
		m_ageing_time_value(AR_CFG_DEFAULT_SW_AGEING_TIME),
		m_max_lid(0), m_group_top(0),
		m_p_df_data(NULL), m_kdor_data(NULL),
		m_is_leaf(false), m_num_cas(0),
		m_config_rn(false), m_rn_gen_string(0xFF),
		m_to_set_rn_rcv_string(false), m_to_set_rn_xmit_port_mask(false),
		m_calculate_port_groups_tree_task(p_port_groups_calculator) {

			CLEAR_STRUCT(m_ar_info);
			CLEAR_STRUCT(m_required_ar_info);
			ClearARData();
			for (int i = 0; i < SUPPORT_LAST; i++) {
				m_support[i] = SUPPORT_UNKNOWN;
				m_error[i] = NO_ERR;
			}
		}

	void Init() {
		m_calculate_port_groups_tree_task.Init(this);
	}

	~ARSWDataBaseEntry();

	void ClearARGroupsData() {
		memset(this->m_to_set_group_table, false, sizeof(bool) * AR_GROUP_TABLE_NUM_BLOCKS);
		memset(this->m_is_group_table_valid, false, sizeof(bool) * AR_GROUP_TABLE_NUM_BLOCKS);
	}

	void ClearARLftData() {
		memset(this->m_to_set_lft_table, false, sizeof(bool) * AR_LFT_TABLE_NUM_BLOCKS_SX);
	}

	void ClearARData() {
		this->m_ar_info.enable_features &= ~IB_AR_INFO_FEATURE_MASK_AR_ENABLED;

		ClearARGroupsData();
		ClearARLftData();
	}

	void MarkSWSupport(supported_feature_t feature) {
		m_support[feature] = SUPPORTED;
	}

	void MarkSWNotSupport(supported_feature_t feature,
			support_errs_t type) {
		m_support[feature] = NOT_SUPPORTED;
		m_error[feature] = type;
	}

	inline uint16_t GetGroupCap() {
		return ib_ar_info_get_group_cap_ho(&m_ar_info);
	}

	osm_ar_subgroup_t *GetSubGroupPtr(u_int16_t group, u_int8_t sub_group) {
		return osm_ar_subgroup_get(&m_general_sw_info.m_p_osm_sw->ar_data,
				group, sub_group);
	}
};

struct ARSWDataBase {
	pthread_mutex_t       m_db_lock;
	bool                  m_is_init;

	GuidToSWDataBaseEntry m_sw_map;

	ARSWDataBase() {
		m_is_init = false;
	}

	~ARSWDataBase() {
		if (m_is_init)
			pthread_mutex_destroy(&m_db_lock);
	}

	int Init() {
		int rc = pthread_mutex_init(&m_db_lock, NULL);
		if (!rc)
			m_is_init = true;

		return rc;
	}
};

// AR Master Data Base
struct MasterDataBase {
	bool m_enable;
	bool m_flfr_enable;         //user cfg param
	bool m_flfr_remote_disable; //user cfg param
	bool m_fr_enable;
	bool m_frn_enable; //FR notification
	bool m_pfrn_enable; //pFR notification
	bool m_ar_enable;
	bool m_arn_enable; //AR notification
	unsigned int m_max_errors;
	unsigned int m_error_window;
	string       m_ar_log_file_name;
	unsigned int m_ar_log_file_size;
	enum OSM_AR_LID_STATE m_ar_mode;
	u_int16_t m_ageing_time;
	string       m_ar_algorithm_str;
	u_int16_t  m_en_sl_mask;
	u_int16_t  m_hbf_sl_mask;
	u_int16_t  m_dfp_en_vl_mask;
	u_int8_t   m_en_ca_sl2vl[IB_NUMBER_OF_SLS];
	u_int8_t   m_en_tr_mask;
	u_int16_t  m_op_mode;
	u_int16_t  m_max_timeout_num;
	ar_algorithm_t m_ar_algorithm;
	u_int8_t   m_max_cas_on_spine;
	u_int8_t   ar_tree_asymmetric_flow;
};

struct PathDescription {
	uint8_t             m_max_hops_number;
	uint8_t             m_min_hops_number;
	path_validation_t   m_path_validation_t;
	uint8_t             m_is_intermediate_down_path;
};

struct PathDescriptionEntry {
	PathDescription     *m_p_desk;
	int                 m_path_index;
};

} // End of namespace

