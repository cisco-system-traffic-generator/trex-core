/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
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


#ifndef IBDIAG_FABRIC_ERRS_H
#define IBDIAG_FABRIC_ERRS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <iostream>
using namespace std;

#include <infiniband/ibdm/Fabric.h>

#include "ibdiag_types.h"
#include "ibdiag_csv_out.h"

/*****************************************************/
/*
 * NOTE: All the following defines must not contain ','
 */
#define SCOPE_CLUSTER   "CLUSTER"
#define SCOPE_SYSTEM    "SYSTEM"
#define SCOPE_NODE      "NODE"
#define SCOPE_PORT      "PORT"
#define SCOPE_APORT     "APORT"

/* event names */
#define FER_DUPLICATED_NODE_GUID    "DUPLICATED_NODE_GUID"
#define FER_DUPLICATED_PORT_GUID    "DUPLICATED_PORT_GUID"
#define FER_DUPLICATED_APORT_GUID   "DUPLICATED_APORT_GUID"
#define FER_NOT_ALL_NODES_SUP_CAP   "NOT_ALL_NODES_SUP_CAP"

#define FER_DR_IS_NULL                  "DR_IS_NULL"
#define FER_LID_IS_ZERO                 "LID_IS_ZERO"
#define FER_DATA_IS_NULL                "DATA_IS_NULL"
#define FER_NODE_IS_NULL                "NODE_IS_NULL"
#define FER_PORT_IS_NULL                "PORT_IS_NULL"
#define FER_NODE_CAP_MODULE_IS_NULL     "NODE_CAP_MODULE_IS_NULL"
#define FER_NODE_EXT_NODE_INFO_IS_NULL  "NODE_EXT_NODE_INFO_IS_NULL"
#define FER_NODE_NOT_RESPOND            "NODE_NO_RESPONSE"
#define FER_NODE_WRONG_CONFIG           "NODE_CONFIG_WRONG"
#define FER_NODE_DUPLICATED_NODE_DESC   "NODE_DUPLICATED_NODE_DESC"
#define FER_NODE_WRONG_FW_VER           "NODE_WRONG_FW_VERSION"
#define FER_NODE_NOT_SUPPORT_CAP        "NODE_NOT_SUPPORT_CAPABILTY"
#define FER_NODE_INVALID_LID            "NODE_INVALID_LID"
#define FER_NODE_SMP_GMP_FW_MISMATCH    "NODE_SMP_GMP_FW_MISMATCH"
#define FER_NODE_SMP_GMP_CAP_MASK_EXIST "NODE_SMP_GMP_CAPABILITY_MASK_EXIST"
#define FER_NODE_NO_VALID_EXIT_FNM      "NODE_NO_VALID_EXIT_FNM"
#define FER_NODE_NON_FNM_CONNECTION     "NODE_NON_FNM_CONNECTION"
#define FER_INVALID_FNM_CONNECTIONS     "FER_INVALID_FNM_CONNECTIONS"
#define FER_FNM_SWITCH_NOT_VISITED      "FNM_SWITCH_NOT_VISITED"
#define FER_INVALID_FNM_SPEEDS          "FER_INVALID_FNM_SPEEDS"

#define FER_SM_NOT_FOUND        "SM_NOT_FOUND_MASTER"
#define FER_SM_UNKNOWN_STATE    "SM_UNKNOWN_STATE"
#define FER_SM_MANY_EXISTS      "SM_FOUND_MANY_MASTERS"
#define FER_SM_NOT_CORRECT      "SM_NOT_CORRECT_MASTER"

#define FER_PM_COUNTER_OVERFLOW         "PM_COUNTER_OVERFLOW"
#define FER_PM_BASE_COUNTER_OVERFLOW    "PM_BASE_COUNTER_OVERFLOW"
#define FER_PM_COUNTER_EXCEED_THRESH    "PM_COUNTER_EXCEEDS_THRESHOLD"
#define FER_PM_COUNTER_NOT_SUPPORTED    "PM_COUNTER_NOT_SUPPORTED"
#define FER_PM_COUNTER_INCREASED        "PM_COUNTER_INCREASED"
#define FER_PM_COUNTER_INVALID_SIZE     "PM_COUNTER_INVALID_SIZE"
#define FER_PM_NEGATIVE_DELTA_COUNTERS  "PM_NEGATIVE_DELTA_COUNTERS"

#define FER_BER_EXCEED_THRESH   "BER_EXCEEDS_THRESHOLD"
#define FER_BER_NO_RCV_DATA     "BER_NO_RCV_DATA"
#define FER_BER_IS_ZERO         "BER_VALUE_ZERO"
#define FER_BER_NO_THRESHOLD    "BER_NO_THRESHOLD_IS_SUPPORTED"
#define FER_BER_ERR_LESS_WARNING "BER_ERR_LESS_WARNING"

#define FER_PORT_ZERO_LID                    "PORT_LID_ZERO"
#define FER_PORT_DUPLICATED_LID              "PORT_DUPLICATED_LID"
#define FER_PORT_NOT_RESPOND                 "PORT_NO_RESPONSE"
#define FER_PORT_WRONG_CONFIG                "PORT_CONFIG_WRONG"
#define FER_PORT_NOT_SUPPORT_CAP             "PORT_NOT_SUPPORT_CAPABILTY"
#define FER_PORT_INVALID_VALUE               "PORT_INVALID_VALUE"
#define FER_PORT_INFO_FAILED                 "PORT_INFO_FAILED"
#define FER_PORT_INVALID_FNM_CONNECTION      "PORT_INVALID_FNM_CONNECTION"

#define FER_APORT_INFO_FAILED                "APORT_INFO_FAILED"

#define FER_DIRECT_ROUTE    "BAD_LINK"

#define FER_LINK_LOGICAL_STATE_WRONG        "LINK_WRONG_LOGICAL_STATE"
#define FER_LINK_LOGICAL_STATE_NOT_ACTIVE   "LINK_LOGICAL_STATE_NOT_ACTIVE"
#define FER_LINK_LOGICAL_DIFFERENT_SPEED    "LINK_DIFFERENT_SPEED"
#define FER_LINK_LOGICAL_UNEXPECTED_SPEED   "LINK_UNEXPECTED_SPEED"
#define FER_LINK_LOGICAL_DIFFERENT_WIDTH    "LINK_DIFFERENT_WIDTH"
#define FER_LINK_LOGICAL_UNEXPECTED_WIDTH   "LINK_UNEXPECTED_WIDTH"
#define FER_LINK_LOGICAL_AUTONEG_ERR        "LINK_AUTONEG_ERR"

#define FER_APORT_LINK_LOGICAL_STATE_NOT_ACTIVE   "APROT_LINK_LOGICAL_STATE_NOT_ACTIVE"
#define FER_APORT_LINK_LOGICAL_DIFFERENT_SPEED    "APORT_LINK_DIFFERENT_SPEED"
#define FER_APORT_LINK_LOGICAL_STATE_WRONG        "APORT_LINK_LOGICAL_STATE_WRONG"
#define FER_APORT_LINK_LOGICAL_AUTONEG_ERR        "APORT_LINK_LOGICAL_AUTONEG_ERR"
#define FER_APORT_LINK_LOGICAL_UNEXPECTED_SPEED   "APORT_LINK_LOGICAL_UNEXPECTED_SPEED"

#define FER_PKEY_MISMATCH                   "PKEY_MISMATCH"
#define FER_AGUID                           "ALIAS_GUID_ERROR"
#define FER_PLANES_MISSING_PKEY             "PLANES_MISSING_PKEY"
#define FER_PLANES_PKEY_WRONG_MEMSHP        "FER_PLANES_PKEY_WRONG_MEMSHP"
#define FER_PLANES_PKEY_WRONG_CONF          "FER_PLANES_PKEY_WRONG_CONF"

#define FER_MLNX_CNTRS_WRONG_PAGE_VER           "MLNX_CNTRS_WRONG_PAGE_VERSION"

#define FER_DISCOVERY_REACHED_MAX_HOP           "DISCOVERY_REACHED_MAX_HOP"
#define FER_SCOPE_REACHED_MAX_HOP               "SCOPE_BUILDER_REACHED_MAX_HOP"
#define FER_SCOPE_WRONG_DESTINATION             "SCOPE_BUILDER_WRONG_DESTINATION"
#define FER_SCOPE_DEAD_END                      "SCOPE_BUILDER_DEAD_END"

#define FER_VIRT_INFO_INVALID_TOP               "VIRT_INFO_INVALID_TOP"
#define FER_VIRT_INFO_INVALID_VLID              "VIRT_INFO_INVALID_VLID"

#define FER_CABLE_TEMPERATURE_ERROR             "CABLE_TEMPERATURE_ERROR"

#define FER_SHARP_CLASS_PORT_INFO               "AM_CLASS_PORT_INFO_TRAP_LID_ERR"
#define FER_SHARP_VERSIONING                    "SHARP_VERSIONING_ERR"
#define FER_SHARP_EDGE_NODE_NOT_FOUND           "EDGE_NODE_NOT_FOUND"
#define FER_SHARP_PARENT_TREE_EDGE_NOT_FOUND    "PARENT_TREE_EDGE_NOT_FOUND"
#define FER_SHARP_MISMATCH_CHILD_NODE_TO_PARENT_NODE    "MISMATCH_CHILD_NODE_TO_PARENT_NODE"
#define FER_SHARP_MISMATCH_CHILD_TO_PARENT_QPN  "MISMATCH_CHILD_TO_PARENT_QPN"
#define FER_SHARP_NODE_NOT_SUPPORT_SHARP        "NODE_NOT_SUPPORT_SHARP"
#define FER_SHARP_TREE_NODE_NOT_FOUND           "TREE_NODE_NOT_FOUND"
#define FER_SHARP_DISCONNECTED_TREE_NODE        "DISCONNECTED_TREE_NODE"
#define FER_SHARP_TREE_ID_NOT_MATCH             "TREE_ID_NOT_MATCH"
#define FER_SHARP_ROOT_ALREADY_EXISTS           "TREE_ROOT_ALREADY_EXISTS"
#define FER_SHARP_TREE_ID_DISABLE_STATE         "TREE_ID_DISABLE_STATE"
#define FER_SHARP_DUPLICATE_QPN_FOR_AN          "DUPLICATE_QPN_FOR_AN"
#define FER_SHARP_QP_NOT_ACTIVE                 "QP_NOT_ACTIVE"
#define FER_SHARP_RQP_NOT_VALID                 "RQP_NOT_VALID"
#define FER_SHARP_REMOTE_NODE_DOESNT_EXIST      "REMOTE_NODE_DOESNT_EXIST"
#define FER_SHARP_QPC_PORT_NOT_ZERO             "QPC_PORT_NOT_ZERO"
#define FER_SHARP_QPC_PORTS_NOT_CONNECTED       "QPC_PORTS_NOT_CONNECTED"

#define FER_PORT_HIERARCHY_MISSING              "PORT_HIERARCHY_MISSING"
#define FER_PORT_HIERARCHY_EXTRA_FIELDS         "PORT_HIERARCHY_EXTRA_FIELDS"
#define FER_PORT_HIERARCHY_MISSING_FIELDS       "PORT_HIERARCHY_MISSING_FIELDS"
#define FER_HIERARCHY_TEMPLATE_MISMATCH         "HIERARCHY_TEMPLATE_MISMATCH"
#define FER_WHBF_WRONG_CONFIGURATIONT           "WHBF_WRONG_CONFIGURATIONT"

#define FER_PFRN_PARTIALLY_SUPPORTED            "PFRN_PARTIALLY_SUPPORTED"
#define FER_PFRN_DIFFERENT_TRAP_LIDS            "PFRN_DIFFERENT_TRAP_LIDS"
#define FER_PFRN_TRAP_LID_NOT_TO_SM             "PFRN_TRAP_LID_NOT_TO_SM"
#define FER_PFRN_NEIGHBOR_NOT_EXIST             "PFRN_NEIGHBOR_NOT_EXIST"
#define FER_PFRN_NEIGHBOR_NOT_SWITCH            "PFRN_NEIGHBOR_NOT_SWITCH"
#define FER_PFRN_FR_NOT_ENABLED                 "PFRN_FR_NOT_ENABLED"
#define FER_DIFFERENT_AR_GROUPS_ID_FOR_DLID     "DIFFERENT_AR_GROUPS_ID_FOR_DLID"
#define FER_PFRN_RECEIVED_ERROR_NOT_ZERO        "PFRN_RECEIVED_ERROR_NOT_ZERO"
#define FER_FLID_VALIDATION                     "FLID_VALIDATION"
#define FER_LOCAL_SUBNET_PFRN_ON_ROUTERS        "LOCAL_SUBNET_PFRN_ON_ROUTERS"
#define FER_ADJACENT_SUBNET_PFRN_ON_ROUTERS     "ADJACENT_SUBNET_PFRN_ON_ROUTERS"
#define FER_CABLE_FW_VERSION                    "CABLE_FW_VERSION"
#define FER_TRANSCEIVER_FW_MISMATCH             "TRANSCEIVER_FW_VERSION_MISMATCH"
#define FER_INTERNAL_DB                         "INTERNAL_DB_ERROR"

// CC Algo validation
#define FER_CC_ALGO_COUNTER_EN_ERROR            "CC_ALGO_COUNTER_EN_ERROR"
#define FER_CC_ALGO_SL_EN_ERROR                 "CC_ALGO_SL_EN_ERROR"
#define FER_CC_ALGO_PARAMS_SL_EN_ERROR          "CC_ALGO_PARMAS_SL_EN_ERROR"
#define FER_CC_ALGO_PARAM_OUT_OF_RANGE          "CC_ALGO_PARAM_OUT_OF_RANGE"

#define FER_EXPORT_DATA                         "EXPORT_DATA"

#define CABLE_TYPE_MISMATCH                     "CABLE_TYPE_MISMATCH"
#define PRTL_REGISTER_MISMATCH                  "PRTL_REGISTER_MISMATCH"
#define PRTL_ROUND_TRIP_LATENCY                 "PRTL_ROUND_TRIP_LATENCY"

#define FER_APORT_PLANE_ALREADY_IN_USE           "APORT_PLANE_ALREADY_IN_USE"
#define FER_APORT_INVALID_PLANE                  "APORT_INVALID_PLANE"
#define FER_APORT_PLANE_IN_MULTIPLE_APORTS       "APORT_PLANE_ALREADY_IN_USE"

#define FER_APORT_MISSING_PLANES                "APORT_MISSING_PLANES"
#define FER_APORT_UNEQUAL_ATTRIBUTE             "APORT_UNEQUAL_ATTRIBUTE"
#define FER_APORT_NO_VALID_ATTRIBUTE            "APORT_NO_VALID_ATTRIBUTE"
#define FER_APORT_NO_AGGREGATED_LABEL           "APORT_NO_AGGREGATED_LABEL"
#define FER_APORT_INVALID_PORT_GUIDS            "APORT_INVALID_PORT_GUIDS"
#define FER_APORT_INVALID_CONNECTION            "APORT_INVALID_CONNECTION"
#define FER_APORT_INVALID_REMOTE_PLANE          "APORT_INVALID_REMOTE_PLANE"
#define FER_APORT_INVALID_NUM_PLANES            "APORT_INVALID_NUM_PLANES"
#define FER_APORT_ZERO_LID                      "APORT_ZERO_LID"
#define FER_APORT_DUPLICATED_LID                "APORT_DUPLICATED_LID"
#define FER_APORT_UNEQUAL_LID                   "APORT_UNEQUAL_LID"
#define FER_APORT_WRONG_CONFIG                  "APORT_WRONG_CONFIG"
#define FER_APORT_INVALID_CAGE_MANAGER          "APORT_INVALID_CAGE_MANAGER"
#define FER_APORT_INVALID_CAGE_MANAGER_SYMMETRY_IN_CAGE "APORT_INVALID_CAGE_MANAGER_SYMMETRY_IN_CAGE"
#define FER_APORT_UNEQUAL_QOS_BW                "FER_APORT_UNEQUAL_QOS_BW"
#define FER_APORT_UNEQUAL_QOS_RL                "FER_APORT_UNEQUAL_QOS_RL"

#define PATH_DISCOVERY_DEAD_END                 "PATH_DISCOVERY_DEAD_END"
#define PATH_DISCOVERY_WRONG_ROUTING            "PATH_DISCOVERY_WRONG_ROUTING"

#define RAILS_SDM_CARDS_EXCLUDE                 "SDM_CARDS_EXCLUDE"
#define RAILS_NO_PCI_ADDRESS_AVAILABLE          "NO_PCI_ADDRESS_AVAILABLE"

#define ENTRY_PLANE_FILTER_MISMATCH             "ENTRY_PLANE_FILTER_MISMATCH"
#define ENTRY_PLANE_FILTER_INVALID_SIZE         "ENTRY_PLANE_FILTER_INVALID_SIZE"
#define ENTRY_PLANE_FILTER_UNEXPECTED           "ENTRY_PLANE_FILTER_UNEXPECTED"

#define END_PORT_PLANE_FILTER_UNEXPECTED        "END_PORT_PLANE_FILTER_UNEXPECTED"
#define END_PORT_PLANE_FILTER_INVALID_LID       "END_PORT_PLANE_FILTER_INVALID_LID"
#define END_PORT_PLANE_FILTER_INVALID_NODE_TYPE "END_PORT_PLANE_FILTER_INVALID_NODE_TYPE"
#define END_PORT_PLANE_FILTER_WRONG_LID         "END_PORT_PLANE_FILTER_WRONG_LID"

#define STATIC_ROUTING_ASYMMETRIC_LINK          "STATIC_ROUTING_ASYMMETRIC_LINK"
#define ADAPTIVE_ROUTING_ASYMMETRIC_LINK        "ADAPTIVE_ROUTING_ASYMMETRIC_LINK"

#define SM_CONFIG_DIFF_VALUES                   "DIFFERENT_VALUE_BY_SM_CONFIGURATION"
#define CPLD_VERSION_MISMATCH                   "CPLD_VERSION_MISMATCH"

#define MAD_FETCHER_FAILED_ADD                  "MAD_FETCHER_FAILED_ADD"

/*****************************************************/
typedef vector < class FabricErrGeneral * >         vec_p_fabric_err;
typedef vec_p_fabric_err                            list_p_fabric_general_err;

typedef enum {
    EN_FABRIC_ERR_UNDEFINED = 0,
    EN_FABRIC_ERR_INFO,
    EN_FABRIC_ERR_WARNING,
    EN_FABRIC_ERR_ERROR
} EnFabricErrLevel_t;

/*****************************************************/
void DumpCSVFabricErrorListTable(list_p_fabric_general_err& list_errors, CSVOut &csv_out,
                                 string name, EnFabricErrLevel_t type);
void CleanFabricErrorsList(list_p_fabric_general_err& list_errors);
void ResetAccumulatedErrors(list_p_fabric_general_err &list_errors);
string DescToCsvDesc(const string & desc);

/*****************************************************/
/*****************************************************/
/*****************************************************/

class FabricErrGeneral {
protected:
    string             scope;
    string             description;
    string             err_desc;
    EnFabricErrLevel_t level;
    bool               dump_csv_only;
    int                line;
    int                count;

public:
    FabricErrGeneral(int inLine = -1, int inCount = 0) :
        scope("UNKNOWN"),
        description("UNKNOWN"),
        err_desc("UNKNOWN"),
        level(EN_FABRIC_ERR_ERROR),
        dump_csv_only(false),
        line(inLine), count(inCount) {}

    virtual ~FabricErrGeneral() {}

    virtual string GetCSVErrorLine() = 0;
    virtual string GetErrorLine() = 0;
    inline virtual EnFabricErrLevel_t GetLevel() { return level; }
    inline virtual void SetLevel(EnFabricErrLevel_t in_level) {
        level = in_level;
    }

    inline virtual bool GetDumpCSVOnly() { return this->dump_csv_only; }
    inline virtual void SetDumpCSVOnly(bool dump_csv_only) {
        this->dump_csv_only = dump_csv_only;
    }

    inline int GetLine() { return line; }
    inline string getDescription() { return description; }
    inline void IncCount() { count++; }
    inline virtual bool IsAccumulable() { return false; }
};


/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrGuid : public FabricErrGeneral {
protected:
    IBNode *p_node;
    u_int64_t duplicated_guid;
public:
    FabricErrGuid(IBNode *p_node, u_int64_t duplicated_guid) :
        FabricErrGeneral(), p_node(p_node), duplicated_guid(duplicated_guid) {}
    ~FabricErrGuid() {}

    virtual string GetCSVErrorLine() = 0;

    inline string GetErrorLine() {
        return (this->description);
    }
};

class FabricErrGuidDR : public FabricErrGuid {
protected:
    string direct_route_to_node_str;
public:
    FabricErrGuidDR(IBNode *p_node, u_int64_t duplicated_guid, string direct_route_to_node_str) :
        FabricErrGuid(p_node, duplicated_guid),  direct_route_to_node_str(direct_route_to_node_str) {}
    ~FabricErrGuidDR() {}

    virtual string GetCSVErrorLine() = 0;
};

class FabricErrDuplicatedNodeGuid : public FabricErrGuidDR {
public:
    FabricErrDuplicatedNodeGuid(IBNode *p_node, string direct_route_to_node_str, u_int64_t duplicated_guid);
    ~FabricErrDuplicatedNodeGuid() {}

    string GetCSVErrorLine();
};


class FabricErrDuplicatedPortGuid : public FabricErrGuidDR {
public:
    FabricErrDuplicatedPortGuid(IBNode *p_node, string direct_route_to_node_str, u_int64_t duplicated_guid);
    ~FabricErrDuplicatedPortGuid() {}

    string GetCSVErrorLine();
};

class FabricErrDuplicatedAPortGuid : public FabricErrGuid {
public:
    FabricErrDuplicatedAPortGuid(const APort *p_aport, u_int64_t duplicated_guid);
    ~FabricErrDuplicatedAPortGuid() {}

    string GetCSVErrorLine();
};

//********************************************************
//*******************************************************
//*********************************************************

class FabricInvalidGuid : public FabricErrGeneral {
protected:
    uint64_t guid;
    string direct_route;

public:
    FabricInvalidGuid(uint64_t in_guid, const string &in_route,
                const string &in_descr, const string &in_type);
    ~FabricInvalidGuid() {}

    virtual string GetCSVErrorLine();

    inline string GetErrorLine() {
        return (this->description);
    }
};


class FabricInvalidPortGuid : public FabricInvalidGuid {
public:
    FabricInvalidPortGuid(uint64_t in_guid, const string& in_route):
                FabricInvalidGuid(in_guid, in_route, string("INVALID_PORT_GUID"), string("Port")){}
    ~FabricInvalidPortGuid() {}
};


class FabricInvalidNodeGuid : public FabricInvalidGuid {
public:
    FabricInvalidNodeGuid(uint64_t in_guid, const string& in_route):
                FabricInvalidGuid(in_guid, in_route, string("INVALID_NODE_GUID"), string("Node")){}
    ~FabricInvalidNodeGuid() {}
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrNotAllDevicesSupCap : public FabricErrGeneral {
public:
    FabricErrNotAllDevicesSupCap(string desc);
    ~FabricErrNotAllDevicesSupCap() {}

    string GetErrorLine();
    string GetCSVErrorLine();
};

/*****************************************************/
/*****************************************************/
/*****************************************************/

class FabricErrNode : public FabricErrGeneral {
protected:
    const IBNode *p_node;
public:
    FabricErrNode(const IBNode *ptr) : FabricErrGeneral(), p_node(ptr) {}
    ~FabricErrNode() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


class FabricErrNodeNotRespond : public FabricErrNode {
public:
    FabricErrNodeNotRespond(IBNode *p_node, string desc);
    ~FabricErrNodeNotRespond() {}
};


class FabricErrNodeWrongConfig : public FabricErrNode {
public:
    FabricErrNodeWrongConfig(IBNode *p_node, string desc);
    ~FabricErrNodeWrongConfig() {}
};

class FabricErrNodeMlnxCountersPageVer : public FabricErrNode {
public:
    FabricErrNodeMlnxCountersPageVer(IBNode *p_node,
                                     unsigned int page,
                                     unsigned int curr_ver,
                                     unsigned int latest_ver);
    ~FabricErrNodeMlnxCountersPageVer() {}
};

class FabricErrNodeDuplicatedNodeDesc : public FabricErrNode {
public:
    FabricErrNodeDuplicatedNodeDesc(IBNode *p_node);
    ~FabricErrNodeDuplicatedNodeDesc() {}
};


class FabricErrNodeWrongFWVer : public FabricErrNode {
public:
    FabricErrNodeWrongFWVer(IBNode *p_node, string desc);
    ~FabricErrNodeWrongFWVer() {}
};


class FabricErrNodeNotSupportCap : public FabricErrNode {
public:
    FabricErrNodeNotSupportCap(IBNode *p_node, string desc);
    ~FabricErrNodeNotSupportCap() {}
};

class FabricErrNodeInvalidLid : public FabricErrNode {
public:
    FabricErrNodeInvalidLid(IBNode *p_node, phys_port_t port, lid_t lid, uint8_t lmc);
    ~FabricErrNodeInvalidLid() {}
};

class FabricErrSmpGmpFwMismatch : public FabricErrNode {
public:
    FabricErrSmpGmpFwMismatch(IBNode *p_node,
                              fw_version_obj_t &smp_fw,
                              fw_version_obj_t &gmp_fw);
    ~FabricErrSmpGmpFwMismatch() {}
};

class FabricErrSmpGmpCapMaskExist : public FabricErrNode {
public:
    FabricErrSmpGmpCapMaskExist(IBNode *p_node, bool is_smp, capability_mask_t &mask);
    ~FabricErrSmpGmpCapMaskExist() {}
};

class NoValidExitFNM : public FabricErrNode {
public:
    NoValidExitFNM(IBNode *p_node, const vec_pport& fnm_ports,
            const vector<IBPort *>& path);
    ~NoValidExitFNM() {}
};

class NonFNMConnection : public FabricErrNode {
public:
    NonFNMConnection(IBNode *p_node, IBPort *p_exit_port,
            const vector<IBPort *>& path);
    ~NonFNMConnection() {}
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrSystem : public FabricErrGeneral {
protected:
    const IBSystem *p_system;
    uint64_t system_guid;
public:
    FabricErrSystem(const IBSystem *ptr);
    ~FabricErrSystem() {}

    string GetCSVErrorLine();
    string GetErrorLine();
private:
    void InitializeSystemGuid();
};

class FNMLoopInsideRing : public FabricErrSystem {
public:
    FNMLoopInsideRing(const IBSystem* p_system,
        const vector<IBPort *>& path);
    ~FNMLoopInsideRing() {}
};

class FNMSwitchNotVisited : public FabricErrSystem {
public:
    FNMSwitchNotVisited(const IBSystem* p_system,
        IBNode *p_not_visited,
        const vector<IBPort *>& path);
    ~FNMSwitchNotVisited() {}
};

class FabricErrInvalidFNMSpeeds : public FabricErrSystem {
public:
    FabricErrInvalidFNMSpeeds(const IBSystem* p_system, string fnm_speeds_str);
    ~FabricErrInvalidFNMSpeeds() {}
};
/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrSM : public FabricErrGeneral {
protected:
    sm_info_obj_t *p_sm_obj;
public:
    FabricErrSM(sm_info_obj_t *p_sm_obj) : FabricErrGeneral(), p_sm_obj(p_sm_obj) {}
    ~FabricErrSM() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


class FabricErrSMNotFound : public FabricErrSM {
public:
    FabricErrSMNotFound(sm_info_obj_t *p_sm_obj);
    ~FabricErrSMNotFound() {}
};


class FabricErrSMUnknownState : public FabricErrSM {
public:
    FabricErrSMUnknownState(sm_info_obj_t *p_sm_obj);
    ~FabricErrSMUnknownState() {}
};


class FabricErrSMManyExists : public FabricErrSM {
public:
    FabricErrSMManyExists(sm_info_obj_t *p_sm_obj);
    ~FabricErrSMManyExists() {}
};


class FabricErrSMNotCorrect : public FabricErrSM {
public:
    FabricErrSMNotCorrect(sm_info_obj_t *p_sm_obj);
    ~FabricErrSMNotCorrect() {}
};


/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrPM : public FabricErrGeneral {
protected:
    IBPort *p_port;
public:
    FabricErrPM(IBPort *p_port) : FabricErrGeneral(), p_port(p_port) {}
    ~FabricErrPM() {}

    string GetCSVErrorLine();
    virtual string GetErrorLine();
};

class FabricErrPMInvalidDelta : public FabricErrPM {
public:
    FabricErrPMInvalidDelta(IBPort *p_port, const string &counters);
    ~FabricErrPMInvalidDelta() {}

    string GetErrorLine() { return (this->p_port->getExtendedName() + " - " + this->description); }
};


class FabricErrPMCounterInvalidSize : public FabricErrPM {
public:
    FabricErrPMCounterInvalidSize(IBPort *p_port, const string& counter_name, u_int8_t real_size);
    ~FabricErrPMCounterInvalidSize() {}

    string GetErrorLine() { return (this->description); }
};


class FabricErrPMCounterOverflow : public FabricErrPM {
public:
    FabricErrPMCounterOverflow(IBPort *p_port, string counter_name, u_int64_t overflow_value);
    ~FabricErrPMCounterOverflow() {}

    string GetErrorLine() { return (this->description); }
};


class FabricErrPMBaseCalcCounterOverflow : public FabricErrPM {
public:
    FabricErrPMBaseCalcCounterOverflow(IBPort *p_port, string counter_name);
    ~FabricErrPMBaseCalcCounterOverflow() {}

    string GetErrorLine() { return (this->description); }
};


class FabricErrPMCounterExceedThreshold : public FabricErrPM {
public:
    FabricErrPMCounterExceedThreshold(IBPort *p_port, string counter_name,
            u_int64_t expected_value, u_int64_t actual_value);
    ~FabricErrPMCounterExceedThreshold() {}

    string GetErrorLine() { return (this->description); }
};


class FabricErrPMCounterNotSupported : public FabricErrPM {
public:
    FabricErrPMCounterNotSupported(IBPort *p_port, string counter_name);
    ~FabricErrPMCounterNotSupported() {}

    string GetErrorLine() { return (this->description); }
};


class FabricErrPMErrCounterIncreased : public FabricErrPM {
public:
    FabricErrPMErrCounterIncreased(IBPort *p_port, string counter_name,
            u_int64_t expected_value, u_int64_t actual_value, bool is_warning);
    ~FabricErrPMErrCounterIncreased() {}
};


/*****************************************************/
/*****************************************************/
/*****************************************************/
typedef list < FabricErrPM * > list_p_pm_err;


class FabricErrPMCountersAll : public FabricErrGeneral {
private:
    IBPort *p_port;
    string err_line;
    string csv_err_line;
public:
    FabricErrPMCountersAll(IBPort *p_port, list_p_pm_err& pm_errors);
    ~FabricErrPMCountersAll() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrBER : public FabricErrGeneral {
protected:
    IBPort *p_port;
public:
    FabricErrBER(IBPort *p_port) : FabricErrGeneral(), p_port(p_port) {}
    ~FabricErrBER() {}

    virtual string GetCSVErrorLine();
    virtual string GetErrorLine();
};

class FabricErrBERThresholdValue : public FabricErrBER {
public:
      FabricErrBERThresholdValue(IBPort *p_port, const char *media_type,
                                 double error, double warning);
      ~FabricErrBERThresholdValue(){}
};


class FabricErrBERThresholdNotFound : public FabricErrBER {
public:
      FabricErrBERThresholdNotFound(IBPort *p_port, const char *media_type);
      ~FabricErrBERThresholdNotFound(){}
};

class FabricErrFwBERExceedThreshold : public FabricErrBER {
public:
    FabricErrFwBERExceedThreshold(IBPort *p_port, double thresh, double value, IBBERType ber_type,
                                  string desc = "");
    ~FabricErrFwBERExceedThreshold() {}
};

class FabricErrBERExceedThreshold : public FabricErrBER {
public:
    FabricErrBERExceedThreshold(IBPort *p_port, u_int64_t expected_value, long double actual_value);
    ~FabricErrBERExceedThreshold() {}
};

class FabricErrEffBERExceedThreshold : public FabricErrBER {
public:
    FabricErrEffBERExceedThreshold(IBPort *p_port, u_int64_t expected_value, long double actual_value);
    ~FabricErrEffBERExceedThreshold() {}
};

class FabricErrBERNoRcvData : public FabricErrBER {
public:
    FabricErrBERNoRcvData(IBPort *p_port);
    ~FabricErrBERNoRcvData() {}
};


class FabricErrBERIsZero : public FabricErrBER {
public:
    FabricErrBERIsZero(IBPort *p_port);
    ~FabricErrBERIsZero() {}
};

class FabricErrEffBERIsZero : public FabricErrBER {
public:
    FabricErrEffBERIsZero(IBPort *p_port);
    ~FabricErrEffBERIsZero() {}
};
/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricPCIDegradation : public FabricErrGeneral {
protected:
    const IBPort *p_port;
    uint8_t depth;
    uint8_t pci_idx;
    uint8_t pci_node;
    string pci_properties;

public:
    FabricPCIDegradation(
        const IBPort *port,
        uint8_t depth, uint8_t pci_idx, uint8_t pci_node);
    ~FabricPCIDegradation() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricPCIWidthDegradation : public FabricPCIDegradation {
public:
    FabricPCIWidthDegradation(
            const IBPort *port,
            uint8_t depth, uint8_t pci_idx, uint8_t pci_node,
            uint32_t enabled, uint32_t active);
    ~FabricPCIWidthDegradation() {}
};

class FabricPCISpeedDegradation : public FabricPCIDegradation {
public:
    FabricPCISpeedDegradation(
            const IBPort *port,
            uint8_t depth, uint8_t pci_idx, uint8_t pci_node,
            uint32_t enabled, uint32_t active);
    ~FabricPCISpeedDegradation() {}
};
/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrPort : public FabricErrGeneral {
protected:
    const IBPort *p_port;
public:
    FabricErrPort(const IBPort *p_port) : FabricErrGeneral(), p_port(p_port) {}
    ~FabricErrPort() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


class FabricErrPortZeroLid : public FabricErrPort {
public:
    FabricErrPortZeroLid(IBPort *p_port);
    ~FabricErrPortZeroLid() {}
};


class FabricErrPortDuplicatedLid : public FabricErrPort {
public:
    FabricErrPortDuplicatedLid(IBPort *p_port);
    ~FabricErrPortDuplicatedLid() {}
};

class FabricErrPortNotRespond : public FabricErrPort {
public:
    FabricErrPortNotRespond(IBPort *p_port, string desc);
    ~FabricErrPortNotRespond() {}
};

class FabricErrPortVLNotRespond : public FabricErrPortNotRespond {
public:
    FabricErrPortVLNotRespond(IBPort *p_port, u_int8_t vl, string desc);
    ~FabricErrPortVLNotRespond() {}
};

class FabricErrPortWrongConfig : public FabricErrPort {
public:
    FabricErrPortWrongConfig(IBPort *p_port, string desc);
    ~FabricErrPortWrongConfig() {}
};


class FabricErrPortNotSupportCap : public FabricErrPort {
public:
    FabricErrPortNotSupportCap(IBPort *p_port, string desc);
    ~FabricErrPortNotSupportCap() {}
};

class FabricErrPortInvalidValue : public FabricErrPort {
public:
    FabricErrPortInvalidValue(IBPort *p_port, string desc);
    ~FabricErrPortInvalidValue() {}
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrDR : public FabricErrGeneral {
protected:
    string direct_route_str;
public:
    FabricErrDR(string direct_route_str);
    ~FabricErrDR() {}

    string GetCSVErrorLine();

    inline string GetErrorLine() {
        return (this->description);
    }
};


/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrLink : public FabricErrGeneral {
protected:
    IBPort *p_port1;
    IBPort *p_port2;
public:
    FabricErrLink(IBPort *p_port1, IBPort *p_port2) :
        FabricErrGeneral(), p_port1(p_port1), p_port2(p_port2) {}
    ~FabricErrLink() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


class FabricErrLinkLogicalStateWrong : public FabricErrLink {
public:
    FabricErrLinkLogicalStateWrong(IBPort *p_port1, IBPort *p_port2);
    ~FabricErrLinkLogicalStateWrong() {}
};


class FabricErrLinkLogicalStateNotActive : public FabricErrLink {
public:
    FabricErrLinkLogicalStateNotActive(IBPort *p_port1, IBPort *p_port2);
    ~FabricErrLinkLogicalStateNotActive() {}
};


class FabricErrLinkDifferentSpeed : public FabricErrLink {
public:
    FabricErrLinkDifferentSpeed(IBPort *p_port1, IBPort *p_port2);
    ~FabricErrLinkDifferentSpeed() {}
};


class FabricErrLinkUnexpectedSpeed : public FabricErrLink {
public:
    FabricErrLinkUnexpectedSpeed(IBPort *p_port1, IBPort *p_port2, string desc);
    ~FabricErrLinkUnexpectedSpeed() {}
};


class FabricErrLinkDifferentWidth : public FabricErrLink {
public:
    FabricErrLinkDifferentWidth(IBPort *p_port1, IBPort *p_port2);
    ~FabricErrLinkDifferentWidth() {}
};


class FabricErrLinkUnexpectedWidth : public FabricErrLink {
public:
    FabricErrLinkUnexpectedWidth(IBPort *p_port1, IBPort *p_port2, string desc);
    ~FabricErrLinkUnexpectedWidth() {}
};


class FabricErrLinkAutonegError : public FabricErrLink {
public:
    FabricErrLinkAutonegError(IBPort *p_port1, IBPort *p_port2, string desc);
    ~FabricErrLinkAutonegError() {}
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrAPortLink : public FabricErrGeneral {
protected:
    APort *p_aport1;
    APort *p_aport2;
public:
    FabricErrAPortLink(APort *p_aport1, APort *p_aport2) :
        FabricErrGeneral(), p_aport1(p_aport1), p_aport2(p_aport2) {};
    ~FabricErrAPortLink() {};

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrAPortLinkLogicalStateWrong : public FabricErrAPortLink {
public:
    FabricErrAPortLinkLogicalStateWrong(APort *p_aport1, APort *p_aport2);
    ~FabricErrAPortLinkLogicalStateWrong() {}
};


class FabricErrAPortLinkLogicalStateNotActive : public FabricErrAPortLink {
public:
    FabricErrAPortLinkLogicalStateNotActive(APort *p_aport1, APort *p_aport2);
    ~FabricErrAPortLinkLogicalStateNotActive() {}
};

class FabricErrAPortLinkDifferentSpeed : public FabricErrAPortLink {
public:
    FabricErrAPortLinkDifferentSpeed(APort *p_aport1, APort *p_aport2);
    ~FabricErrAPortLinkDifferentSpeed() {}
};

class FabricErrAPortLinkAutonegError : public FabricErrAPortLink {
public:
    FabricErrAPortLinkAutonegError(APort *p_aport1, APort *p_aport2, string desc);
    ~FabricErrAPortLinkAutonegError() {}
};

class FabricErrAPortLinkUnexpectedSpeed : public FabricErrAPortLink {
public:
    FabricErrAPortLinkUnexpectedSpeed(APort *p_aport1, APort *p_aport2, string desc);
    ~FabricErrAPortLinkUnexpectedSpeed() {}
};

class FabricErrAPortLinkDifferentWidth : public FabricErrAPortLink {
public:
    FabricErrAPortLinkDifferentWidth(APort *p_aport1, APort *p_aport2);
    ~FabricErrAPortLinkDifferentWidth() {}
};

class FabricErrAPortLinkUnexpectedWidth : public FabricErrAPortLink {
public:
    FabricErrAPortLinkUnexpectedWidth(APort *p_aport1, APort *p_aport2, string desc);
    ~FabricErrAPortLinkUnexpectedWidth() {}
};

/****************************************************/
/****************************************************/
/****************************************************/
class FabricErrPKeyMismatch : public FabricErrGeneral {
protected:
    IBPort *p_port1;
    IBPort *p_port2;
public:
    FabricErrPKeyMismatch(IBPort *p_port1, IBPort *p_port2,
            string port1_pkey_str, string port2_pkey_str);
    ~FabricErrPKeyMismatch() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


/****************************************************/
/****************************************************/
/****************************************************/
class FabricErrAGUID : public FabricErrGeneral {
protected:
    IBPort *p_port;
    string guid_owner_name;
    u_int64_t duplicated_guid;
    string guid_type;
public:
    FabricErrAGUID(IBPort *p_port, string owner_name,
            u_int64_t guid, string guid_type);
    ~FabricErrAGUID() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrAGUIDPortGuidDuplicated : public FabricErrAGUID {
public:
    FabricErrAGUIDPortGuidDuplicated(IBPort *p_port1, IBPort *p_port2,
            u_int64_t guid):
                FabricErrAGUID(p_port1, p_port2->getName(), guid, "port GUID") {}
    ~FabricErrAGUIDPortGuidDuplicated() {}
};

class FabricErrAGUIDNodeGuidDuplicated : public FabricErrAGUID {
public:
    FabricErrAGUIDNodeGuidDuplicated(IBPort *p_port, IBNode *p_node,
            u_int64_t guid):
                FabricErrAGUID(p_port, p_node->getName(), guid, "node GUID") {}
    ~FabricErrAGUIDNodeGuidDuplicated() {}
};

class FabricErrAGUIDSysGuidDuplicated : public FabricErrAGUID {
public:
    FabricErrAGUIDSysGuidDuplicated(IBPort *p_port, IBSystem *p_system,
            u_int64_t guid):
                FabricErrAGUID(p_port, p_system->name, guid, "system GUID") {}
    ~FabricErrAGUIDSysGuidDuplicated() {}
};

class FabricErrAGUIDInvalidFirstEntry : public FabricErrGeneral {
protected:
    IBPort *p_port;
    u_int64_t guid_zero_index;
public:
    FabricErrAGUIDInvalidFirstEntry(IBPort *port, u_int64_t first_alias_guid);
    ~FabricErrAGUIDInvalidFirstEntry() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};


/****************************************************/
/****************************************************/
/****************************************************/
class FabricErrVPort : public FabricErrGeneral {
protected:
    IBVPort *p_port;
    string guid_owner_name;
    u_int64_t duplicated_guid;
    string guid_type;
public:
    FabricErrVPort(IBVPort *p_port, string owner_name,
            u_int64_t guid, string guid_type);
    ~FabricErrVPort() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrVPortGuidDuplicated : public FabricErrVPort {
public:
    FabricErrVPortGuidDuplicated(IBVPort *p_port1, IBVPort *p_port2,
            u_int64_t guid):
                FabricErrVPort(p_port1, p_port2->getName(), guid, "virtual port GUID") {}
    ~FabricErrVPortGuidDuplicated() {}
};

class FabricErrVPortGuidPGUIDDuplicated : public FabricErrVPort {
public:
    FabricErrVPortGuidPGUIDDuplicated(IBVPort *p_port1, IBPort *p_port2,
            u_int64_t guid):
        FabricErrVPort(p_port1, p_port2->getName(), guid, "port GUID") {}
   ~FabricErrVPortGuidPGUIDDuplicated() {}
};

class FabricErrVPortNodeGuidDuplicated : public FabricErrVPort {
public:
    FabricErrVPortNodeGuidDuplicated(IBVPort *p_port, IBNode *p_node,
            u_int64_t guid):
                FabricErrVPort(p_port, p_node->getName(), guid, "node GUID") {}
    ~FabricErrVPortNodeGuidDuplicated() {}
};

class FabricErrVPortSysGuidDuplicated : public FabricErrVPort {
public:
    FabricErrVPortSysGuidDuplicated(IBVPort *p_port, IBSystem *p_system,
            u_int64_t guid):
                FabricErrVPort(p_port, p_system->name, guid, "system GUID") {}
    ~FabricErrVPortSysGuidDuplicated() {}
};

class FabricErrVPortGUIDInvalidFirstEntry : public FabricErrGeneral {
protected:
    IBVPort *p_port;
    IBPort *p_phys_port;
    u_int64_t guid_at_zero_index;
public:
    FabricErrVPortGUIDInvalidFirstEntry(IBPort *phys_port,
                                        IBVPort *port,
                                        u_int64_t first_alias_guid);
    ~FabricErrVPortGUIDInvalidFirstEntry() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrVPortIvalidTopIndex : public FabricErrGeneral {
protected:
    IBPort *p_port;
    uint16_t cap_idx;
    uint16_t top_idx;
public:
    FabricErrVPortIvalidTopIndex(IBPort *port,
                                 uint16_t cap,
                                 uint16_t top);
    ~FabricErrVPortIvalidTopIndex() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrVPortInvalid : public FabricErrGeneral {
protected:
    IBPort *p_port;
public:
    FabricErrVPortInvalid(IBPort *port) : FabricErrGeneral(),
                                          p_port(port) {}
    ~FabricErrVPortInvalid() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrVLidZero : public FabricErrVPortInvalid {
public:
    FabricErrVLidZero(IBPort *port, IBVPort *vport);
    ~FabricErrVLidZero() {}
};

class FabricErrVPortInvalidLid: public FabricErrVPortInvalid {
public:
    FabricErrVPortInvalidLid(IBPort *port, IBVPort *vport, lid_t lid);
    ~FabricErrVPortInvalidLid() {}
};

class FabricErrInvalidIndexForVLid : public FabricErrVPortInvalid {
public:
    FabricErrInvalidIndexForVLid(IBPort *port,
                                 IBVPort *vport,
                                 u_int16_t lid_by_vport_idx);
    ~FabricErrInvalidIndexForVLid() {}
};

class FabricErrVlidForVlidByIndexIsZero : public FabricErrVPortInvalid {
public:
    FabricErrVlidForVlidByIndexIsZero(IBPort *port,
                                      IBVPort *vport,
                                      IBVPort *vport_by_index,
                                      u_int16_t lid_by_vport_idx);
    ~FabricErrVlidForVlidByIndexIsZero() {}
};
/****************************************************/
/****************************************************/
/****************************************************/
class FabricErrPortInfoFail : public FabricErrGeneral {
protected:
    IBNode *m_p_node;
    unsigned int    m_port_num;
    void init(const char *error_desc);
public:
    FabricErrPortInfoFail(IBNode *p_node,
                          unsigned int port_num,
                          const char *error_desc);
    FabricErrPortInfoFail(IBNode *p_node,
                          unsigned int port_num,
                          int rc);
    ~FabricErrPortInfoFail() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class FabricErrAPortInfoFail : public FabricErrGeneral {
public:
    FabricErrAPortInfoFail(APort *p_aport,
                           const char *error_desc);
    ~FabricErrAPortInfoFail() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

/****************************************************/
/****************************************************/
/****************************************************/
class FabricErrDiscovery : public FabricErrGeneral {
protected:
    IBNode      *p_node;
    uint8_t     max_hops;

public:
    FabricErrDiscovery(IBNode *p_node,
                       uint8_t max_hops);
    ~FabricErrDiscovery() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

/****************************************************/
/****************************************************/
/****************************************************/

class CableTemperatureErr : public FabricErrGeneral {
private:
    const IBPort      *p_port;

public:
    CableTemperatureErr(const IBPort *p_inPort, const string &message,
                        const string& temp, const string &treshold);
    ~CableTemperatureErr();

    string GetCSVErrorLine();
    string GetErrorLine();
};

/****************************************************/
/****************************************************/
/****************************************************/

class FabricErrCluster : public FabricErrGeneral {
public:
    FabricErrCluster(string err_desc, string desc);
    ~FabricErrCluster() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class SharpErrClassPortInfo : public FabricErrCluster {
public:
    SharpErrClassPortInfo(string desc);
    ~SharpErrClassPortInfo() {}
};

class SharpErrVersions : public FabricErrCluster {
public:
    SharpErrVersions(string desc);
    ~SharpErrVersions() {}
};

class SharpErrInvalidActiveVer : public FabricErrNode {
public:
    SharpErrInvalidActiveVer(IBNode *p_node);
    ~SharpErrInvalidActiveVer() {}
};

class SharpErrDiffVerMgmtAndSharp : public FabricErrNode {
public:
    SharpErrDiffVerMgmtAndSharp(IBNode *p_node, int class_ver, int sharp_ver);
    ~SharpErrDiffVerMgmtAndSharp() {}
};

class SharpErrEdgeNodeNotFound : public FabricErrNode {
public:
    SharpErrEdgeNodeNotFound(IBNode *p_node, lid_t rlid);
    ~SharpErrEdgeNodeNotFound() {}
};

class SharpErrParentTreeEdgeNotFound : public FabricErrNode {
public:
    SharpErrParentTreeEdgeNotFound(IBNode *p_node, lid_t rlid, u_int16_t tree_id);
    ~SharpErrParentTreeEdgeNotFound() {}
};

class SharpErrMismatchParentChildQPNumber : public FabricErrNode {
public:
    SharpErrMismatchParentChildQPNumber(IBNode *p_node, lid_t parent_lid, u_int32_t parent_qpn,
                                        u_int32_t parent_rqpn, lid_t lid_child, u_int32_t child_qpn,
                                        u_int32_t child_rqpn, u_int16_t tree_id);
    ~SharpErrMismatchParentChildQPNumber() {}
};

class SharpErrMismatchParentChildQPConfig : public FabricErrNode {
public:
    SharpErrMismatchParentChildQPConfig(IBNode *p_node, lid_t child_lid, lid_t parent_lid,
                                        lid_t mismatch_parent_lid, u_int16_t tree_id);
    ~SharpErrMismatchParentChildQPConfig() {}
};

class SharpErrTreeNodeNotFound : public FabricErrNode {
public:
    SharpErrTreeNodeNotFound(IBNode *p_node, u_int16_t tree_id);
    ~SharpErrTreeNodeNotFound() {}
};

class SharpErrDisconnectedTreeNode : public FabricErrNode {
public:
    SharpErrDisconnectedTreeNode(IBNode *p_node, u_int16_t tree_id,
                                 u_int32_t qpn, lid_t rlid);
    ~SharpErrDisconnectedTreeNode() {}
};

class SharpErrNodeTreeIDNotMatchGetRespondTreeID : public FabricErrNode {
public:
    SharpErrNodeTreeIDNotMatchGetRespondTreeID(IBNode *p_node, u_int16_t tree_id);
    ~SharpErrNodeTreeIDNotMatchGetRespondTreeID() {}
};

class SharpErrRootTreeNodeAlreadyExistsForTreeID : public FabricErrNode {
public:
    SharpErrRootTreeNodeAlreadyExistsForTreeID(IBNode *p_node, u_int16_t tree_id);
    ~SharpErrRootTreeNodeAlreadyExistsForTreeID() {}
};

class SharpErrDuplicatedQPNForAggNode : public FabricErrNode {
public:
    SharpErrDuplicatedQPNForAggNode(IBNode *p_node, u_int16_t tree_id,
                                     u_int16_t dup_tree_id, u_int32_t qpn);
    ~SharpErrDuplicatedQPNForAggNode() {}
};

class SharpErrQPNotActive : public FabricErrNode {
public:
    SharpErrQPNotActive(IBNode *p_node, u_int32_t qpn, u_int8_t qp_state);
    ~SharpErrQPNotActive() {}
};

class SharpErrRQPNotValid : public FabricErrNode {
public:
    SharpErrRQPNotValid(IBNode *p_node, u_int32_t qpn, u_int32_t rqpn);
    ~SharpErrRQPNotValid() {}
};

class SharpErrRemoteNodeDoesntExist : public FabricErrNode {
public:
    SharpErrRemoteNodeDoesntExist(IBNode *p_node);
    ~SharpErrRemoteNodeDoesntExist() {}
};

class SharpErrQPCPortNotZero : public FabricErrNode {
public:
    SharpErrQPCPortNotZero(IBNode *p_node, u_int8_t qpc_port, u_int8_t port_sel_supported,
                           IBNode *p_remote_node, u_int8_t remote_qpc_port,
                           u_int8_t remote_port_sel_supported);
    ~SharpErrQPCPortNotZero() {}
};

class SharpErrQPCPortsNotConnected : public FabricErrNode {
public:
    SharpErrQPCPortsNotConnected(IBNode *p_node, u_int8_t qpc_port, IBNode *p_remote_node,
                                 u_int8_t remote_qpc_port);
    ~SharpErrQPCPortsNotConnected() {}
};

/****************************************************/
/****************************************************/
/****************************************************/

class FabricErrPortHierarchyMissing : public FabricErrPort {
public:
    FabricErrPortHierarchyMissing(IBPort *p_port);
    ~FabricErrPortHierarchyMissing() {}
};

class FabricErrPortHierarchyExtraFields : public FabricErrPort {
public:
    FabricErrPortHierarchyExtraFields(IBPort *p_port, vector<string> fields);
    ~FabricErrPortHierarchyExtraFields() {}
};

class FabricErrPortHierarchyMissingFields : public FabricErrPort {
public:
    FabricErrPortHierarchyMissingFields(IBPort *p_port, vector<string> fields);
    ~FabricErrPortHierarchyMissingFields() {}
};

class FabricErrHierarchyTemplateMismatch : public FabricErrPort {
public:
    FabricErrHierarchyTemplateMismatch(IBPort *p_port, u_int64_t template_guid,
                                       u_int8_t hierarchy_index);
    ~FabricErrHierarchyTemplateMismatch() {}
};

/****************************************************/
/****************************************************/
/****************************************************/

class ExportDataErr : public FabricErrGeneral {
protected:
    IBNode *p_node;
    IBPort *p_port;

public:
    ExportDataErr(IBNode *p_node, IBPort *p_port, const char *fmt, ...);
    ~ExportDataErr() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

//******************************************************************************************
class FabricErrWHBFConfiguration : public FabricErrNode {
public:
    FabricErrWHBFConfiguration(IBNode *p_node);
    ~FabricErrWHBFConfiguration() {}
};

/****************************************************/
/***           PFRN (Class 0xC) Errors            ***/
/****************************************************/
class pFRNErrPartiallySupported: public FabricErrCluster {
public:
    pFRNErrPartiallySupported(string desc);
    ~pFRNErrPartiallySupported() {}
};

class pFRNErrDiffTrapLIDs: public FabricErrCluster {
public:
    pFRNErrDiffTrapLIDs(string desc);
    ~pFRNErrDiffTrapLIDs() {}
};

class pFRNErrTrapLIDNotSM : public FabricErrCluster {
public:
    pFRNErrTrapLIDNotSM(string desc);
    ~pFRNErrTrapLIDNotSM() {}
};

class pFRNErrNeighborNotExist : public FabricErrNode {
public:
    pFRNErrNeighborNotExist(IBNode *p_node, u_int32_t record);
    ~pFRNErrNeighborNotExist() {}
};

class pFRNErrNeighborNotSwitch : public FabricErrNode {
public:
    pFRNErrNeighborNotSwitch(IBNode *p_node, u_int32_t record);
    ~pFRNErrNeighborNotSwitch() {}
};

class DifferentARGroupsIDForDLIDErr : public FabricErrCluster {
public:
    DifferentARGroupsIDForDLIDErr(string desc);
    ~DifferentARGroupsIDForDLIDErr() {}
};

class pFRNReceivedErrorNotZeroErr : public FabricErrPort {
public:
    pFRNReceivedErrorNotZeroErr(IBPort *p_port, u_int32_t pfrn_received_error);
    ~pFRNReceivedErrorNotZeroErr() {}
};

class pFRNErrFRNotEnabled : public FabricErrNode {
public:
    pFRNErrFRNotEnabled(IBNode *p_node);
    ~pFRNErrFRNotEnabled() {}
};

/***********************************************************************
 *                   FLID verification errors
***********************************************************************/
class FLIDError : public FabricErrGeneral {
public:
    FLIDError(const std::string&  message): FabricErrGeneral(), error(message) {}
    ~FLIDError() {}
    string GetCSVErrorLine();
    string GetErrorLine();

private:
    std::string error;
};

class FLIDPortError : public FabricErrPort {
public:
    FLIDPortError(const IBPort *p_port, const string &message);
    ~FLIDPortError() {}
};

/***********************************************************************
 *                   PFRN on routers verification errors
***********************************************************************/

class LocalSubnetPFRNOnRoutersError : public FabricErrGeneral {
public:
    LocalSubnetPFRNOnRoutersError(const std::string&  message):
                FabricErrGeneral(), error(message) {}
    ~LocalSubnetPFRNOnRoutersError() {}
    string GetCSVErrorLine() override;
    string GetErrorLine() override;

private:
    std::string error;
};

class AdjacentSubnetsPFRNOConfigError : public FabricErrGeneral {
public:
    AdjacentSubnetsPFRNOConfigError(const std::string&  message):
                FabricErrGeneral(), error(message) {}
    ~AdjacentSubnetsPFRNOConfigError() {}
    string GetCSVErrorLine() override;
    string GetErrorLine() override;

private:
    std::string error;
};

/******************************************************************
* CABLE FW Version error
**********************************************************************/
class CableFWVersionError : public FabricErrPort {
public:
    CableFWVersionError(const IBPort *p_port, const string &message);
    ~CableFWVersionError() {}
};

class CableFWVersionMismatchError : public FabricErrGeneral {
public:
    ~CableFWVersionMismatchError() {}
    CableFWVersionMismatchError(const string &message);
    string GetCSVErrorLine();
    string GetErrorLine();
};

/**************************************************************************
*  NULL pointer accumulation error
**************************************************************************/
class NullPtrError: public FabricErrGeneral {
public:
    NullPtrError(int inLine): FabricErrGeneral(inLine, 1) {}
    ~NullPtrError() {}

    string GetCSVErrorLine() override;
    string GetErrorLine() override;
    inline  bool IsAccumulable() override{ return true; }
};

class FLIDNodeError : public FabricErrNode {
public:
    FLIDNodeError(const IBNode *p_node, const string &message);
    ~FLIDNodeError() {}
};

/***********************************************************************
 *                   CC Algo verification errors
 ***********************************************************************/
class CC_AlgoCounterEnErr : public FabricErrPort {
public:
    CC_AlgoCounterEnErr(IBPort *p_port, const vector <int> &counters_en_algos);
    ~CC_AlgoCounterEnErr() {}
};

class CC_AlgoSLEnErr : public FabricErrPort {
public:
    CC_AlgoSLEnErr(IBPort *p_port, u_int8_t sl, const vector <int> &algos);
    ~CC_AlgoSLEnErr () {}
};

class CC_AlgoParamsSLEnErr : public FabricErrPort {
public:
    CC_AlgoParamsSLEnErr(IBPort *p_port, u_int8_t sl, const vector <int> &algos);
    ~CC_AlgoParamsSLEnErr () {}
};

class CC_AlgoParamRangeErr : public FabricErrPort {
public:
    CC_AlgoParamRangeErr(IBPort *p_port, const string &desc);
    ~CC_AlgoParamRangeErr () {}
};

/***********************************************************************
 *                   PRTL cable length calculation errors
 ***********************************************************************/

class CableTypeMismatchError : public FabricErrPort {
public:
    CableTypeMismatchError(const IBPort *p_port);
    ~CableTypeMismatchError () {}
    string GetErrorLine() override;
};

class PrtlRegisterMismatchError : public FabricErrPort {
public:
    PrtlRegisterMismatchError(const IBPort *p_port);
    ~PrtlRegisterMismatchError () {}
    string GetErrorLine() override;
};

class PrtlRegisterInvalidError : public FabricErrPort {
public:
    PrtlRegisterInvalidError(const IBPort *p_port, const string &reason);
    ~PrtlRegisterInvalidError () {}
};

/***********************************************************************
 *                   APorts Errors
 ***********************************************************************/
class APortPlaneAlreadyInUseError : public FabricErrPort {
public:
    APortPlaneAlreadyInUseError(const IBPort *p_port);
    ~APortPlaneAlreadyInUseError() {}
};

class APortInvalidPlaneNumError : public FabricErrPort {
public:
    APortInvalidPlaneNumError(const IBPort *p_port, size_t planes_num);
    APortInvalidPlaneNumError(const IBPort *p_port);
    ~APortInvalidPlaneNumError() {}
};

class PlaneInMultipleAPorts : public FabricErrPort {
public:
    PlaneInMultipleAPorts(const IBPort *p_port);
    ~PlaneInMultipleAPorts() {}
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrAPort : public FabricErrGeneral {
protected:
    const APort    *p_aport;
    string         name;

   string getErrorPrefix() const;
public:
    FabricErrAPort(const APort *p_aport);
    // Constructor to allow producing APort errors for non-APorts, like BM port 0
    FabricErrAPort(const string& name);
    ~FabricErrAPort() {}

    string GetCSVErrorLine();
    string GetErrorLine();
};

class APortMissingPlanes : public FabricErrAPort {
public:
    APortMissingPlanes(const APort *p_aport);
    ~APortMissingPlanes () {}
};

class APortUnequalAttribute : public FabricErrAPort {
public:
    APortUnequalAttribute(const APort *p_aport,
                          const string& attr_name, const string& attr_values_str);
    ~APortUnequalAttribute () {}
};

class APortNoValidAttribute : public FabricErrAPort {
public:
    APortNoValidAttribute(const APort *p_aport,
                          const string& attr_name);
    ~APortNoValidAttribute () {}
};

class APortNoAggregatedLabel : public FabricErrAPort {
public:
    APortNoAggregatedLabel(const APort *p_aport);
    ~APortNoAggregatedLabel () {}
};

class APortInvalidPortGuids : public FabricErrAPort {
public:
    APortInvalidPortGuids(const APort *p_aport,
                          const string& attr_values_str);
    ~APortInvalidPortGuids () {}
};

class APortInvalidConnection : public FabricErrAPort {
public:
    APortInvalidConnection(const APort *p_aport);
    ~APortInvalidConnection () {}
};

class APortInvalidRemotePlane : public FabricErrAPort {
public:
    APortInvalidRemotePlane(const APort *p_aport,
                            int local_plane, int remote_plane);
    ~APortInvalidRemotePlane () {}
};

class APortInvalidNumOfPlanes : public FabricErrAPort {
public:
    APortInvalidNumOfPlanes(const APort *p_aport,
                            int local_num, int remote_num);
    ~APortInvalidNumOfPlanes () {}
};

class APortInvalidCageManager : public FabricErrAPort {
public:
    APortInvalidCageManager(const APort *p_aport);
    ~APortInvalidCageManager () {}
};

class APortInvalidCageManagerSymmetryInCage : public FabricErrAPort {
public:
    APortInvalidCageManagerSymmetryInCage(const APort *p_aport, int cage_num, int plane);
    ~APortInvalidCageManagerSymmetryInCage () {}
};

class FabricErrAPortZeroLid : public FabricErrAPort {
public:
    FabricErrAPortZeroLid(const APort *p_aport);
    ~FabricErrAPortZeroLid() {}
};

class FabricErrAPortDuplicatedLid : public FabricErrAPort {
public:
    FabricErrAPortDuplicatedLid(const APort *p_aport, lid_t lid);
    ~FabricErrAPortDuplicatedLid() {}
};

class APortPlanesMissingPkey : public FabricErrAPort {
public:
    APortPlanesMissingPkey(const APort *p_aport, u_int16_t pkey);
    APortPlanesMissingPkey(const string& aport_name, u_int16_t pkey);
    ~APortPlanesMissingPkey() {}
};

class APortWrongPKeyMembership : public FabricErrAPort {
public:
    APortWrongPKeyMembership(const APort *p_aport, u_int16_t pkey,
                             uint8_t memshp1, uint8_t memshp2);
    APortWrongPKeyMembership(const string& aport_name, u_int16_t pkey,
                             uint8_t memshp1, uint8_t memshp2);
    ~APortWrongPKeyMembership() {}
};

class APortWrongPKeyConf : public FabricErrAPort {
public:
    APortWrongPKeyConf(const APort *p_aport);
    APortWrongPKeyConf(const string& aport_name);
    ~APortWrongPKeyConf() {}
};

class FabricErrAPortUnequalLID : public FabricErrAPort {
public:
    FabricErrAPortUnequalLID(const APort *p_aport);
    ~FabricErrAPortUnequalLID() {}
};

class FabricErrAPortWrongConfig : public FabricErrAPort {
public:
    FabricErrAPortWrongConfig(APort *p_aport, string desc);
    ~FabricErrAPortWrongConfig() {}
};

class FabricErrAPortUnequalQoSBandwidth : public FabricErrAPort {
public:
    FabricErrAPortUnequalQoSBandwidth(const APort *p_aport, uint32_t sl, string arr);
    ~FabricErrAPortUnequalQoSBandwidth() {}
};

class FabricErrAPortUnequalQoSRateLimit : public FabricErrAPort {
public:
    FabricErrAPortUnequalQoSRateLimit(const APort *p_aport, uint32_t sl, string arr);
    ~FabricErrAPortUnequalQoSRateLimit() {}
};

//*******************************************************************************************
//*******************************************************************************************
class PathDiscoveryDeadEndError : public FabricErrNode {
public:
    PathDiscoveryDeadEndError(const IBNode *p_node, lid_t target_lid);
    ~PathDiscoveryDeadEndError() {}
};

class PathDiscoveryWrongRouting : public FabricErrPort {
public:
    PathDiscoveryWrongRouting(const IBPort *p_port, lid_t target_lid);
    ~PathDiscoveryWrongRouting() {}
};


//*******************************************************************************************
//**************************************************************************************************
class RailsSDMCardsError : public FabricErrGeneral {
public:
    RailsSDMCardsError(size_t count);
    ~RailsSDMCardsError() {}

    string GetCSVErrorLine() override;
    string GetErrorLine() override;
};

class RailsInvalidPCIAddress : public FabricErrPort {
public:
    RailsInvalidPCIAddress(const IBPort *p_port, PCI_BDF_SOURCE source);
    ~RailsInvalidPCIAddress() {}
};


// Entry Plane Filter
class EntryPlaneFilterMismatch : public FabricErrNode {
public:
    EntryPlaneFilterMismatch(IBNode *p_node, phys_port_t in_p, phys_port_t out_p,
                             bool expected, bool actual);
    ~EntryPlaneFilterMismatch() {}
};

class EntryPlaneFilterInvalidSize : public FabricErrNode {
public:
    EntryPlaneFilterInvalidSize(IBNode *p_node);
    ~EntryPlaneFilterInvalidSize() {}
};

class EntryPlaneFilterUnexpected : public FabricErrNode {
public:
    EntryPlaneFilterUnexpected(IBNode *p_node);
    ~EntryPlaneFilterUnexpected() {}
};

class EndPortPlaneFilterUnexpected : public FabricErrNode {
public:
    EndPortPlaneFilterUnexpected(IBNode *p_node);
    ~EndPortPlaneFilterUnexpected() {}
};

class EndPortPlaneFilterInvalidLID : public FabricErrNode {
public:
    EndPortPlaneFilterInvalidLID(IBNode *p_node, size_t index);
    ~EndPortPlaneFilterInvalidLID() {}
};

class EndPortPlaneFilterInvalidNodeType : public FabricErrNode {
public:
    EndPortPlaneFilterInvalidNodeType(IBNode *p_node, size_t index);
    ~EndPortPlaneFilterInvalidNodeType() {}
};

class EndPortPlaneFilterWrongLID : public FabricErrNode {
public:
    EndPortPlaneFilterWrongLID(IBNode *p_node, size_t index);
    ~EndPortPlaneFilterWrongLID() {}
};

// Symmetric Routing validation errors
class StaticRoutingAsymmetricLink : public FabricErrNode {
public:
    StaticRoutingAsymmetricLink(IBNode *p_node,
                                IBPort *p_port,
                                lid_t lid,
                                u_int8_t pLFT);

    ~StaticRoutingAsymmetricLink() {}
};

class AdaptiveRoutingAsymmetricLink : public FabricErrNode {
public:
    AdaptiveRoutingAsymmetricLink(IBNode *p_node,
                                  IBPort *p_port,
                                  lid_t lid,
                                  u_int8_t pLFT);

    ~AdaptiveRoutingAsymmetricLink() {}
};

class ScopeBuilderMaxHopError : public FabricErrGeneral {
public:
    ScopeBuilderMaxHopError(int max_hops);
    ~ScopeBuilderMaxHopError() {}

    string GetCSVErrorLine() override;
    string GetErrorLine() override;
};

class ScopeBuilderWrongDestinationError : public FabricErrNode {
public:
    ScopeBuilderWrongDestinationError(const IBNode *p_node);
    ~ScopeBuilderWrongDestinationError() {}
};

class ScopeBuilderDeadEndError : public FabricErrNode {
public:
    ScopeBuilderDeadEndError(const IBNode *p_node, u_int8_t plft, lid_t target_lid);
    ~ScopeBuilderDeadEndError() {}
};

template <typename T>
class FabricErrValueSet: public FabricErrCluster {
public:
    FabricErrValueSet(const std::set<T>& vals, uint8_t max_vals, string err_desc, string desc)
    :FabricErrCluster(err_desc, desc)
    {
        stringstream ss;
        ss << desc;
        ss << " [";
        for (auto val = vals.begin(); val != vals.end() && max_vals; ++val, --max_vals) {
            if (val != vals.begin())
                ss << "; ";

            if (max_vals == 1)
                ss << "...";
            else
                ss << +(*val);
        }
        ss << "]";
        this->description.assign(ss.str());
    }
    ~FabricErrValueSet() {};
};


// Post Reports SM Configuration Validations
template <typename T>
class SMConfigDiffValues: public FabricErrValueSet<T> {
public:
    SMConfigDiffValues(const std::set<T>& vals, uint8_t max_vals, string desc) :
    FabricErrValueSet<T>(vals, max_vals, SM_CONFIG_DIFF_VALUES, desc)   {}
    ~SMConfigDiffValues() {}
};

class CPLDVersionMismatch: public FabricErrValueSet<u_int32_t> {
public:
    CPLDVersionMismatch(const std::set<u_int32_t>& vals, uint8_t max_vals)
       : FabricErrValueSet<u_int32_t>(vals, max_vals, CPLD_VERSION_MISMATCH,
            string("CPLD version mismatch - currently there are ")
                .append(std::to_string(vals.size()))
                .append(" different versions in fabric:")) 
    {
        this->SetLevel(EN_FABRIC_ERR_WARNING);
    }
    ~CPLDVersionMismatch() {}
};

class CCPerPlaneInvalidEntry : public FabricErrPort {
public:
    CCPerPlaneInvalidEntry(IBPort* p_port, uint8_t en_cc_per_plane);
    ~CCPerPlaneInvalidEntry() {};
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
class FabricErrNodePort : public FabricErrGeneral {
    public:
        const IBNode *m_node;
        const IBPort *m_port;
        phys_port_t  m_port_num;

    public:
        FabricErrNodePort()
            : m_node(nullptr), m_port(nullptr), m_port_num(0x00) {};

        FabricErrNodePort(const IBNode *node)
            : m_node(node), m_port(nullptr), m_port_num(0x00) {};

        FabricErrNodePort(const IBPort *port)
            : m_node(nullptr), m_port(port), m_port_num(0x00) {};

        FabricErrNodePort(const IBNode *node, const IBPort *port)
            : m_node(node), m_port(port), m_port_num(0x00) {};

        FabricErrNodePort(const IBNode *node, phys_port_t port_num)
            : m_node(node), m_port(nullptr), m_port_num(port_num) {};

        FabricErrNodePort(const IBNode *node, const IBPort *port, phys_port_t port_num)
            : m_node(node), m_port(port), m_port_num(port_num) {};

        ~FabricErrNodePort() {}

        void GetCSVErrorHeader(string & output, const IBNode *node, const IBPort *port, phys_port_t port_num);

        string GetCSVErrorLine() override;
        string GetErrorLine() override;
    };

/*****************************************************/

class FabricErrNodeIsNull : public FabricErrNodePort {
    public:
        FabricErrNodeIsNull(const string &name);
        ~FabricErrNodeIsNull() {}
    };

/*****************************************************/

class FabricErrPortIsNull : public FabricErrNodePort {
    public:
        FabricErrPortIsNull(const IBNode *node, phys_port_t port_num);
        ~FabricErrPortIsNull() {}
};

/*****************************************************/

class FabricErrCapModuleIsNull : public FabricErrNodePort {
    public:
        FabricErrCapModuleIsNull(const IBNode *node);
        ~FabricErrCapModuleIsNull() {}
};

/*****************************************************/

class FabricErrExtNodeInfoIsNull : public FabricErrNodePort {
    public:
        FabricErrExtNodeInfoIsNull(const IBNode *node);
        ~FabricErrExtNodeInfoIsNull() {}
};

/*****************************************************/

class FabricErrClbckDataIsNull : public FabricErrNodePort {
    public:
        FabricErrClbckDataIsNull(const IBNode *node, const IBPort *port);
        ~FabricErrClbckDataIsNull() {}
};

/*****************************************************/

class FabricErrInvalidClbckNodePort : public FabricErrNodePort {
    public:
        FabricErrInvalidClbckNodePort(const IBNode *node, const IBPort *port, bool per_port);
        ~FabricErrInvalidClbckNodePort() {}
};

/*****************************************************/

class DirectRouteNodePortIsNull : public FabricErrNodePort {
    public:
        DirectRouteNodePortIsNull(const IBNode *node, const IBPort *port, bool per_port);
        ~DirectRouteNodePortIsNull() {}
};

/*****************************************************/

class LidNodePortIsZero : public FabricErrNodePort {
    public:
        LidNodePortIsZero(const IBNode *node, const IBPort *port, bool per_port);
        ~LidNodePortIsZero() {}
};

class FabricErrMadFetcherFailedAdd : public FabricErrNodePort {
public:
    FabricErrMadFetcherFailedAdd(int status, const std::string & name, const clbck_data_t & clbck_data, bool per_port);
    ~FabricErrMadFetcherFailedAdd() {}

};

/*****************************************************/
/*****************************************************/
/*****************************************************/
#endif          /* IBDIAG_FABRIC_ERRS_H */
