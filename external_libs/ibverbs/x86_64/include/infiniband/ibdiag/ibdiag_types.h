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


#ifndef IBDIAG_TYPES_H_
#define IBDIAG_TYPES_H_

#include <stdlib.h>
#include <vector>
#include <list>
#include <map>
#include <queue>

#include <infiniband/ibdm/Fabric.h>
#include <ibis/ibis_types.h>
#include <ibis/packets/packets_layouts.h>
#include <infiniband/misc/tool_trace/tool_trace.h>
#include "infiniband/ibutils/forward_callback.h"


#define DEFAULT_SL  0

// N2N Class NeighborsInfo MAD
#define NEIGHBORS_IN_BLOCK  14

/****************************************************/
/* new types */
typedef struct sm_info_obj {
    struct SMP_SMInfo smp_sm_info;
    IBPort *p_port;
} sm_info_obj_t;

// struct that holds different calculated counters based on pm counters
struct PM_PortCalcCounters {
    u_int64_t RetransmissionPerSec;
};

typedef struct vs_mlnx_cntrs_obj {
    struct VS_DiagnosticData *p_mlnx_cntrs_p0;
    struct VS_DiagnosticData *p_mlnx_cntrs_p1;
    struct VS_DiagnosticData *p_mlnx_cntrs_p255;
} vs_mlnx_cntrs_obj_t;

typedef struct index_line {
    string name;
    size_t offset;
    size_t size;
    size_t line;
    size_t rows;
} index_line_t;

typedef struct pm_info_obj {
    struct PM_PortCounters *p_port_counters;
    struct PM_PortCountersExtended *p_extended_port_counters;
    struct PM_PortExtendedSpeedsCounters *p_port_ext_speeds_counters;
    struct PM_PortExtendedSpeedsRSFECCounters *p_port_ext_speeds_rsfec_counters;
    struct VendorSpec_PortLLRStatistics *p_port_llr_statistics; //May need to move to vs_info_obj
    struct PM_PortCalcCounters *p_port_calc_counters;
    struct PM_PortRcvErrorDetails *p_port_rcv_error_details;
    struct PM_PortXmitDiscardDetails *p_port_xmit_discard_details;

    pm_info_obj() : p_port_counters(NULL), p_extended_port_counters(NULL),
                    p_port_ext_speeds_counters(NULL), p_port_ext_speeds_rsfec_counters(NULL),
                    p_port_llr_statistics(NULL), p_port_calc_counters(NULL),
                    p_port_rcv_error_details(NULL), p_port_xmit_discard_details(NULL) {
    }

    ~pm_info_obj() {
        delete p_port_counters;
        delete p_extended_port_counters;
        delete p_port_ext_speeds_counters;
        delete p_port_ext_speeds_rsfec_counters;
        delete p_port_llr_statistics;
        delete p_port_calc_counters;
        delete p_port_rcv_error_details;
        delete p_port_xmit_discard_details;
    }
} pm_info_obj_t;

typedef struct fw_version_obj {
    u_int32_t major;
    u_int32_t minor;
    u_int32_t sub_minor;

    fw_version_obj& operator=(const struct VendorSpec_GeneralInfo& other) {
        //extended fields != 0, then take them
        if (other.FWInfo.Extended_Major ||
            other.FWInfo.Extended_Minor ||
            other.FWInfo.Extended_SubMinor) {
            major = other.FWInfo.Extended_Major;
            minor = other.FWInfo.Extended_Minor;
            sub_minor = other.FWInfo.Extended_SubMinor;
        } else {
            major = other.FWInfo.Major;
            minor = other.FWInfo.Minor;
            sub_minor = other.FWInfo.SubMinor;
        }
        return *this;
    }
    int set(const char* fw_str) {
        if (!fw_str)
            return 1;
        unsigned int n = sscanf(fw_str, "%u.%u.%u", &major, &minor, &sub_minor);
        if (n == 3)
            return 0;
        return 1;
    }
    bool operator==(const struct fw_version_obj& other) const {
        return (major == other.major && minor == other.minor && sub_minor == other.sub_minor);
    }
    bool operator!=(const struct fw_version_obj& other) {
        return (!(*this == other));
    }
    bool operator>(const struct fw_version_obj& other) const {
        u_int32_t x[3] = {major, minor, sub_minor};
        const u_int32_t y[3] = {other.major, other.minor, other.sub_minor};
        for (int i = 0; i < 3; i++) {
            if (x[i] > y[i])
                return true;
            if (x[i] < y[i])
                return false;
        }
        return false;
    }
    bool operator<(const struct fw_version_obj& other) const {
        return (!((*this > other) || (*this == other)));
    }
} fw_version_obj_t;

struct GreaterFwVerObjComparer{
    bool operator()(fw_version_obj_t const& x, fw_version_obj_t const& y) const{
        return x > y;
    }
};

//check that PortInfo.CapabilityMask's 'IsExtendedSpeedsSupported' is set
#define IS_SUPPORT_PORT_INFO_EXT_SPEEDS(cap_mask) ((cap_mask) & 0x4000)

//check that PortInfo.CapabilityMask's 'IsCapabilityMask2Supported' is set
#define IS_SUPPORT_PORT_INFO_CAP_MASK2(cap_mask) ((cap_mask & 0x8000))

//check that PortInfo.CapabilityMask2's IsPortInfoExtendedSupported is set
#define IS_SUPPORT_PORT_INFO_EXTENDED(cap_mask) ((cap_mask) & 0x2)

//check that PortInfo.CapabilityMask2's IsSwitchPortStateTableSupported is set
#define IS_SUPPORT_SWITCH_PORT_STATE_TABLE(cap_mask) ((cap_mask) & 0x0008)

//check that PortInfo.CapabilityMask's 'IsHierarchyInfoSupported' is set
#define IS_SUPPORT_HIERARCHY_INFO(cap_mask) ((cap_mask & 0x80000000))

//check that PortInfo.CapabilityMsk's IsLinkRoundTripLatencySupported is set
#define IS_SUPPORT_LINK_ROUND_TRIP_LATENCY(cap_mask) ((cap_mask) & 0x1000000)

//check that PortInfo.CapabilityMask2's 'IsExtendedSpeeds2Supported' is set
#define IS_SUPPORT_PORT_INFO_EXT_SPEEDS2(cap_mask2) ((cap_mask2) & 0x800)

enum VLArbitrationBlockIndex {
    LOWER_LOW_PRIORITY_VL_ARBITRATION_TABLE    = 1,
    UPPER_LOW_PRIORITY_VL_ARBITRATION_TABLE    = 2,
    LOWER_HIGH_PRIORITY_VL_ARBITRATION_TABLE   = 3,
    UPPER_HIGH_PRIORITY_VL_ARBITRATION_TABLE   = 4
};

enum FastRecoveryCountersTrigger {
    FR_TRIGGER_CREDIT_WD_TIMEOUT = 2,
    FR_TRIGGER_FIRST = FR_TRIGGER_CREDIT_WD_TIMEOUT,
    FR_TRIGGER_RESERVED = 3,
    FR_TRIGGER_RAW_BER = 4,
    FR_TRIGGER_EFFECTIVE_BER = 5,
    FR_TRIGGER_SYMBOL_BER = 6,
    FR_TRIGGER_LAST
};

enum ProfilesConfigFeature {
    FAST_RECOVERY_PROFILE = 0,
    QOS_VL_CONFIG_PROFILE,
    DATA_TYPE_TO_VL_PROFILE,
    NVLHBF_PROFILE,
    PORT_RECOVERY_POLICY_PROFILE,
    NUM_PROFILES_CONFIG_FEATURES
};

#define ENTRY_PLANE_FILTER_EGRESS_BLOCK_SIZE 256
#define RAIL_FILTER_EGRESS_BLOCK_SIZE        256
#define CROCODILE_MAX_PLANES    4
#define BM_MAX_PLANES           1


// reasons for bad direct route
typedef enum {
    IBDIAG_BAD_DR_NONE = 0,
    IBDIAG_BAD_DR_NODE_FIRST,
    IBDIAG_BAD_DR_NODE_NODE_INFO = IBDIAG_BAD_DR_NODE_FIRST,
    IBDIAG_BAD_DR_NODE_GUID_INVALID,
    IBDIAG_BAD_DR_NODE_DUPLICATE_CHECK,
    IBDIAG_BAD_DR_NODE_NODE_DESCRIPTION,
    IBDIAG_BAD_DR_NODE_INCONSISTENT, //data db inconsistency, no mem
    IBDIAG_BAD_DR_NODE_BAD_NODE_INFO, //received bad node info data
    IBDIAG_BAD_DR_NODE_LAST = IBDIAG_BAD_DR_NODE_BAD_NODE_INFO,
    IBDIAG_BAD_DR_PORT_FIRST,
    IBDIAG_BAD_DR_PORT_PORT_INFO = IBDIAG_BAD_DR_PORT_FIRST,
    IBDIAG_BAD_DR_PORT_INVALID_LID,
    IBDIAG_BAD_DR_PORT_INCONSISTENT,
    IBDIAG_BAD_DR_PORT_LAST = IBDIAG_BAD_DR_PORT_INCONSISTENT,
    IBDIAG_BAD_DR_MAX
} IbdiagBadDirectRouteReason_t;

extern const char* bad_direct_route_reasons [IBDIAG_BAD_DR_MAX];

// struct for more info on bad direct route
typedef struct IbdiagBadDirectRoute {
    direct_route_t              *direct_route;
    // more details on why it's bad
    IbdiagBadDirectRouteReason_t reason;
    int                          port_num;
    string                       message;
    void clear(void) {
        direct_route = NULL;
        reason = IBDIAG_BAD_DR_NONE;
        port_num = 0;
        message = "";
    };
    IbdiagBadDirectRoute() { clear(); }
} IbdiagBadDirectRoute_t;

#define NUM_CAPABILITY_FIELDS 4
//mask consists number of indexes. each filed size in bits
#define CAPABILITY_MASK_FIELD_SIZE (sizeof(u_int32_t)*8)

typedef struct capability_mask {
    u_int32_t mask[NUM_CAPABILITY_FIELDS];

    void clear() { memset(mask, 0, sizeof(mask)); }
    capability_mask() { clear(); }

    capability_mask& operator=(const struct GeneralInfoCapabilityMask& other) {
        mask[0] = other.capability0;
        mask[1] = other.capability1;
        mask[2] = other.capability2;
        mask[3] = other.capability3;
        return *this;
    }
    //bit is a number between 0 and 127
    //to set bit k in mask[w] -> bit=w*32+k
    int set(u_int8_t bit);
    void ntoh();
    void hton();
    friend std::ostream& operator<< (std::ostream& stream,
                                     const struct capability_mask& mask);
} capability_mask_t;

typedef struct query_or_mask {
    bool to_query; //if this true, then the 'mask' is irrelevant
    capability_mask_t mask;
    friend std::ostream& operator<< (std::ostream& stream,
                                     const struct query_or_mask& qmask);
} query_or_mask_t;

typedef struct prefix_guid_data {
    u_int64_t        original_guid;
    query_or_mask_t  qmask;
    prefix_guid_data() { original_guid = 0; qmask.to_query = false; }
    friend std::ostream& operator<< (std::ostream& stream,
                                     const struct prefix_guid_data& guid_data);
} prefix_guid_data_t;

typedef vector < IBNode * >                                     vector_p_node;
typedef vector < IBPort * >                                     vector_p_port;
typedef vector < IBVPort * >                                    vector_p_vport;
typedef vector < IBVNode * >                                    vector_p_vnode;
typedef vector < struct SMP_NodeInfo * >                        vector_p_smp_node_info;
typedef vector < struct SMP_SwitchInfo * >                      vector_p_smp_switch_info;
typedef vector < struct whbf_config * >                         vector_p_whbf_config;
typedef vector < struct adaptive_routing_info * >               vector_p_vs_ar_info;
typedef vector < struct ib_private_lft_info * >                 vector_p_vs_plft_info;
typedef vector < struct port_rn_counters* >                     vector_p_vs_rn_counters;
typedef vector < struct port_routing_decision_counters* >       vector_p_vs_routing_decision_counters;
typedef vector < struct hbf_config * >                          vector_p_hbf_config;
typedef vector < struct VS_CreditWatchdogTimeoutCounters * >    vector_p_vs_credit_wd_timeout_counters;
typedef vector < struct VS_FastRecoveryCounters * >             vector_p_vs_fast_recovery_counters;
typedef vector < vector_p_vs_fast_recovery_counters >           vec_vec_p_vs_fast_recovery_counters;
typedef vector < struct VS_PortGeneralCounters * >              vec_p_vs_port_general_counters;
typedef vector < struct VS_PortRecoveryPolicyCounters * >       vec_p_vs_port_recovery_policy_counters;
typedef vector < struct SMP_ProfilesConfig * >                  vector_p_smp_profiles_config;
typedef vector < vector_p_smp_profiles_config >                 vec_vec_p_smp_profiles_config;
typedef vector < vec_vec_p_smp_profiles_config >                vec_vec_vec_p_smp_profiles_config;
typedef vector < struct SMP_CreditWatchdogConfig * >            vector_p_credit_watchdog_config;
typedef vector < vector_p_credit_watchdog_config >              vec_vec_p_credit_watchdog_config;
typedef vector < struct SMP_BERConfig * >                       vector_p_ber_config;
typedef vector < vector_p_ber_config >                          vec_vec_p_ber_config;
typedef vector < struct VS_PerformanceHistogramBufferControl *> vec_p_performance_histogram_buffer_control;
typedef vector < vec_p_performance_histogram_buffer_control >   vec_vec_performance_histogram_buffer_control;
typedef vector < struct VS_PerformanceHistogramInfo * >         vec_p_performance_histogram_info;
typedef vector < struct VS_PerformanceHistogramBufferData * >   vec_p_performance_histogram_buffer_data;
typedef vector < vec_p_performance_histogram_buffer_data >      vec_vec_performance_histogram_buffer_data;
typedef vector < struct VS_PerformanceHistogramPortsControl * > vec_p_performance_histogram_ports_control;
typedef vector < vec_p_performance_histogram_ports_control >    vec_v_performance_histogram_ports_control;

typedef vector < struct VS_PerformanceHistogramPortsData * >    vec_p_vs_performance_histogram_ports_data;
typedef vector < vec_p_vs_performance_histogram_ports_data >    vec_v_vs_performance_histogram_ports_data;

// pFRN
typedef vector < struct SMP_pFRNConfig * >                      vector_p_pfrn_config;
typedef vector < struct neighbor_record * >                     vector_p_neighbor_record;
typedef vector < vector_p_neighbor_record >                     vector_vec_p_neighbor_record;
typedef vector < struct Class_C_KeyInfo * >                     vector_p_n2n_key_info;

typedef vector < struct SMP_PortInfo * >                        vector_p_smp_port_info;
typedef vector < struct SMP_PortInfoExtended * >                vector_p_smp_port_info_ext;
typedef vector < struct ib_extended_node_info* >                vector_p_smp_mlnx_ext_node_info;
typedef vector < struct SMP_MlnxExtPortInfo * >                 vector_p_smp_mlnx_ext_port_info;
typedef vector < struct VendorSpec_GeneralInfo * >              vector_p_vs_general_info;
typedef vector < struct VS_SwitchNetworkInfo * >                vector_p_vs_switch_network_info;
typedef vector < struct SMP_SwitchPortStateTable >              vector_smp_switch_port_state_table;
typedef vector < u_int16_t * >                                  vector_p_pm_cap_mask;
typedef vector < IB_ClassPortInfo * >                           vector_p_class_port_info;
typedef vector < pm_info_obj_t * >                              vector_p_pm_info_obj;
typedef vector < struct PM_PortSamplesControl* >                vector_p_pm_port_samples_control;
typedef vector < struct PortSampleControlOptionMask * >         vector_p_pm_option_mask;
typedef vector < vs_mlnx_cntrs_obj_t * >                        vector_p_vs_mlnx_cntrs_obj;
typedef vector < struct SMP_VirtualizationInfo * >              vector_p_smp_virtual_info;
typedef vector < struct SMP_VPortInfo * >                       vector_p_smp_vport_info;
typedef vector < struct SMP_VNodeInfo * >                       vector_p_smp_vnode_info;
typedef vector < struct SMP_TempSensing * >                     vector_p_smp_temp_sensing;
typedef vector < struct SMP_QosConfigSL * >                     vector_p_smp_qos_config_sl;
typedef vector < struct SMP_QosConfigVL * >                     vector_p_smp_qos_config_vl;
typedef vector < struct SMP_RouterInfo * >                      vector_p_smp_router_info;
typedef vector < struct SMP_AdjSiteLocalSubnTbl * >             vector_p_smp_adj_router_tbl;
typedef vector < vector_p_smp_adj_router_tbl >                  vector_v_smp_adj_router_tbl;
typedef vector < struct SMP_NextHopTbl * >                      vector_p_smp_next_hop_router_tbl;
typedef vector < vector_p_smp_next_hop_router_tbl >             vector_v_smp_next_hop_router_tbl;
typedef vector < struct SMP_AdjSubnetsRouterLIDInfoTable * >    vector_p_smp_adj_subnet_router_lid_info_tbl;
typedef vector < vector_p_smp_adj_subnet_router_lid_info_tbl >  vector_v_smp_adj_subnet_router_lid_info_tbl;
typedef vector < struct SMP_RouterLIDTable * >                  vector_p_smp_router_lid_tbl;
typedef vector < vector_p_smp_router_lid_tbl >                  vector_v_smp_router_lid_tbl;
typedef vector < struct SMP_ARGroupToRouterLIDTable * >         vector_p_smp_ar_group_router_lid_tbl;
typedef vector < vector_p_smp_ar_group_router_lid_tbl >         vector_v_ar_group_router_lid_tbl;
typedef vector < struct SMP_PortRecoveryPolicyConfig * >        vector_port_recovery_policy_config;
typedef vector < vector_port_recovery_policy_config >           vector_v_port_recovery_policy_config;


typedef vector < u_int64_t >                                    vector_uint64;
typedef list < IBNode * >                                       list_p_node;
typedef list < sm_info_obj_t * >                                list_p_sm_info_obj;
typedef list < direct_route_t  * >                              list_p_direct_route;
typedef vector < direct_route_t  * >                            vector_p_direct_route;
typedef list < IbdiagBadDirectRoute * >                         list_p_bad_direct_route;
typedef list < string >                                         list_string;
typedef list < u_int8_t >                                       list_uint8;
typedef list < index_line_t >                                   list_index_line;
typedef map < u_int64_t, list_p_direct_route >                  map_guid_list_p_direct_route;
typedef map < pair < u_int64_t, u_int8_t >, direct_route_t * >  map_port_p_direct_route;
typedef map < u_int16_t, list_p_port >                          map_lid_list_p_port;
typedef map < string,  u_int64_t >                              map_str_uint64;
typedef map < u_int8_t,  u_int64_t >                            map_uint8_uint64;
typedef map < uint64_t, uint64_t >                              map_guid_2_key_t;
typedef vector < struct SMP_VPortState * >                      vector_p_smp_vport_state;
typedef vector < vector_p_smp_vport_state >                     vector_v_smp_vport_state;
typedef vector < struct SMP_PKeyTable * >                       vector_p_smp_pkey_tbl;
typedef vector < vector_p_smp_pkey_tbl >                        vector_v_smp_pkey_tbl;
typedef vector < struct SMP_VLArbitrationTable * >              vector_p_vl_arbitration_tbl;
typedef vector < vector_p_vl_arbitration_tbl >                  vector_vec_p_vl_arbitration_tbl;
typedef vector < struct SMP_GUIDInfo * >                        vector_p_smp_guid_tbl;
typedef vector < vector_p_smp_guid_tbl >                        vector_v_smp_guid_tbl;
typedef vector < struct SMP_VPortGUIDInfo * >                   vector_p_smp_vport_guid_tbl;
typedef vector < vector_p_smp_vport_guid_tbl >                  vector_v_smp_vport_guid_tbl;
typedef vector < u_int64_t >                                    vec_guids;
typedef map<u_int64_t, fw_version_obj_t>                        map_uint64_fw_version_obj_t;
typedef map<u_int64_t, capability_mask_t>                       map_uint64_capability_mask_t;
typedef pair<uint32_t, device_id_t>                             pair_uint32_device_id_t;
typedef pair<device_id_t, string>                               pair_device_id_t_string;
typedef map<pair_uint32_device_id_t, capability_mask_t>         map_ven_dev_to_capability_mask_t;
typedef map<pair_device_id_t_string, struct VendorSpec_GeneralInfo * >     map_devid_psid_to_fw_version_obj_t;
typedef map<fw_version_obj_t, query_or_mask_t, GreaterFwVerObjComparer>    map_fw_to_query_or_mask_t;
typedef map<pair_uint32_device_id_t, map_fw_to_query_or_mask_t> map_ven_id_dev_id_2_fw_data_t;
typedef map<u_int64_t, prefix_guid_data_t>                      map_guid_to_prefix_data_t;
typedef map<u_int8_t, map_guid_to_prefix_data_t>                map_prefix_2_guids_data_t;
typedef queue < IBNode * >                                      queue_p_node;
typedef set < string >                                          set_string_t;
typedef vector < string >                                       vector_string_t;
typedef map < string, set_string_t >                            map_string_2_set_string_t;

typedef vector < class CountersPerSLVL * >                      vec_slvl_cntrs;

typedef pair< IBPort *, struct PM_PortRcvXmitCntrsSlVl >             pair_ibport_slvl_cntr_data_t;
typedef set < pair_ibport_slvl_cntr_data_t, struct slvl_data_sort >  set_port_data_update_t;

typedef vector < struct CC_EnhancedCongestionInfo * >           vec_enhanced_cc_info;
typedef vector < struct CC_CongestionSwitchGeneralSettings * >  vec_cc_sw_settings;
typedef vector < struct CC_CongestionPortProfileSettings * >    vec_cc_port_settings;
typedef vector < vec_cc_port_settings >                         vec_vec_cc_port_settings;
typedef vector < struct CC_CongestionSLMappingSettings * >      vec_cc_sl_mapping_settings;
typedef vector < struct CC_CongestionHCAGeneralSettings * >     vec_cc_hca_settings;
typedef vector < struct CC_CongestionHCARPParameters * >        vec_cc_hca_rp_parameters;
typedef vector < struct CC_CongestionHCANPParameters * >        vec_cc_hca_np_parameters;
typedef vector < struct CC_CongestionHCAStatisticsQuery * >     vec_cc_hca_statistics_query;

// CC algo MADs
typedef vector < struct CC_CongestionHCAAlgoConfig * >          vec_cc_hca_algo_config;
typedef vector < vec_cc_hca_algo_config>                        vec_vec_cc_hca_algo_config;
typedef vector < struct CC_CongestionHCAAlgoConfigParams * >    vec_cc_hca_algo_config_params;
typedef vector < vec_cc_hca_algo_config_params >                vec_vec_cc_hca_algo_config_params;
typedef vector < struct CC_CongestionHCAAlgoCounters * >        vec_cc_hca_algo_counters;
typedef vector < vec_cc_hca_algo_counters>                      vec_vec_cc_hca_algo_counters;

// NVL MADs
typedef vector < struct SMP_NVLinkFecMode * >                   vec_p_nvl_fec_mode;
typedef vector < struct SMP_AnycastLIDInfo * >                  vec_p_nvl_anycast_lid_info;
typedef vector < vec_p_nvl_anycast_lid_info >                   vec_v_nvl_anycast_lid_info;
typedef vector < struct SMP_NVLHBFConfig * >                    vec_p_nvl_hbf_config;
typedef vector < struct SMP_ContainAndDrainInfo * >             vec_p_nvl_contain_and_drain_info;
typedef vector < vec_p_nvl_contain_and_drain_info >             vec_v_nvl_contain_and_drain_info;
typedef vector < struct SMP_ContainAndDrainPortState * >        vec_p_nvl_contain_and_drain_port_state;
typedef vector < vec_p_nvl_contain_and_drain_port_state >       vec_v_nvl_contain_and_drain_port_state;


// NVL - Reduction
typedef vector < struct IB_ClassPortInfo*>                      vec_p_nvl_class_port_info;
typedef vector < struct NVLReductionInfo*>                      vec_p_nvl_reduction_info;
typedef vector < struct NVLReductionPortInfo*>                  vec_p_nvl_reduction_port_info;
typedef vector < struct NVLReductionRoundingMode*>              vec_p_nvl_reduction_rounding_mode;
typedef vector < struct NVLReductionForwardingTable * >            vec_p_nvl_reduction_forwarding_table;
typedef vector < vec_p_nvl_reduction_forwarding_table >         vec_v_nvl_reduction_forwarding_table;
typedef vector < struct NVLPenaltyBoxConfig * >                    vec_p_nvl_penalty_box_config;
typedef vector < vec_p_nvl_penalty_box_config >                 vec_v_nvl_penalty_box_config;
typedef vector < struct NVLReductionConfigureMLIDMonitors * >   vec_p_nvl_reduction_configure_mlid_monitors;
typedef vector < struct NVLReductionCounters * >                vec_p_nvl_reduction_counters;
typedef vector < vec_p_nvl_reduction_counters >                 vec_v_nvl_reduction_counters;

typedef vector < struct SMP_LinearForwardingTableSplit * >      vec_p_smp_lft_split;

typedef vector < struct SMP_ExtendedSwitchInfo * >              vec_p_smp_extended_switch_info;
typedef vector < struct PM_PortSamplesResult * >                vec_p_pm_port_samples_result;
typedef vector < struct SMP_ChassisInfo* >                      vec_p_smp_chassis_info;

//SMP Capability Mask bits
//note: when adding new values, update the 'first' and the 'last'
typedef enum {
    EnSMPCapIsPrivateLinearForwardingSupported = 0,    //0
    EnSMPCapFirst = EnSMPCapIsPrivateLinearForwardingSupported,
    EnSMPCapIsAdaptiveRoutingSupported,                //1
    EnSMPCapIsAdaptiveRoutingRev1Supported,            //2
    EnSMPCapIsRemotePortMirroringSupported,            //3
    EnSMPCapIsTemperatureSensingSupported,             //4
    EnSMPCapIsConfigSpaceAccessSupported,              //5
    EnSMPCapIsCableInfoSupported,                      //6
    EnSMPCapIsSMPEyeOpenSupported,                     //7
    EnSMPCapIsLossyVLConfigSupported,                  //8
    EnSMPCapIsExtendedPortInfoSupported,               //9
    EnSMPCapIsAccessRegisterSupported,                 //10
    EnSMPCapIsInterProcessCommunicationSupported,      //11
    EnSMPCapIsPortSLToPrivateLFTMapSupported,          //12
    EnSMPCapIsExtendedNodeInfoSupported,               //13
    EnSMPCapIsASlaveVirtualSwitch,                     //14
    EnSMPCapIsVirtualizationSupported,                 //15
    EnSMPCapIsAdvancedRetransAndFECModesSupported,     //16
    EnSMPCapIsSemaphoreLockSupported,                  //17
    EnSMPCapIsSHArPAggrNodeSupported,                  //18
    EnSMPCapIsCableInfoPasswordSupported,              //19
    EnSMPCapIsVL2VLPerSWIDSupported,                   //20
    EnSMPCapIsSpecialPortsMarkingSupported,            //21
    EnSMPCapIsVPortGUIDInfoSupported,                  //22
    EnSMPCapIsForcePLFTSubgroup0Supported,             //23
    EnSMPCapIsQoSConfigSLRateLimitSupported,           //24
    EnSMPCapIsQoSConfigSLVPortRateLimitSupported,      //25
    EnSMPCapIsQoSConfigSLAllocBWSupported,             //26
    EnSMPCapIsQoSConfigSLVPortAllocBWSupported,        //27
    EnSMPCapIsEnhancedTrap128Supported,                //28
    EnSMPCapIsDevInfoSupported,                        //29
    EnSMPCapIsGlobalOOOSupported,                      //30
    EnSMPCapIsMADAddlStatusSupported,                  //31
    EnSMPCapIsQoSConfigSLRateLimitPortSupported,       //32
    EnSMPCapIsQoSConfigSLRateLimitVPortSupported,      //33
    EnSMPCapIsIsQoSConfigSLAllocBWPortSupported,       //34
    EnSMPCapIsQoSConfigSLAllocBWVPortSupported,        //35
    EnSMPCapIsQoSConfigHighPriorityVLGroupRateLimitSupported,  //36
    EnSMPCapIsQoSConfigHighPriorityVLGroupAllocBWSupported,    //37
    EnSMPCapsIsQoSConfigLowPriorityVLGroupRateLimitSupported,  //38
    EnSMPCapIsQoSConfigLowPriorityVLGroupAllocBWSupported,     //39
    EnSMPCapIsQoSConfigVLRateLimitSupported,           //40
    EnSMPCapIsQoSConfigVLAllocBWSupported,             //41
    //the bit is RESERVED                              //42
    EnSMPCapIsCableInfoBankNumberSupported = 43,       //43
    EnSMPCapIsVSCCSupported,                           //44
    EnSMPCapIsAdaptiveTimeoutSupported,                //45
    EnSMPCapIsClassEnableSupported,                    //46
    EnSMPCapIsExtendedAddressModeSupported,            //47
    EnSMPCapIsRouterLIDSupported,                      //48
    EnSMPCapIsLinearForwardingTableSplitSupported,     //49
    EnSMPCapIsDataTypetoSLMappingSupported,            //50
    EnSMPCapIsQoSConfigVLSupported,                    //51
    EnSMPCapIsFastRecoverySupported,                   //52
    EnSMPCapIsCreditWatchdogSupported,                 //53
    EnSMPCapIsNVLFabricManagementSupported,            //54
    EnSMPCapIsNVLReductionManagementSupported,         //55
    EnSMPCapIsRailFilterSupported,                     //56
    EnSMPCapIsNVLHBFConfigSupported,                   //57
    EnSMPCapIsDataTypetoVLMappingSupported,            //58 should be reserved ?
    EnSMPCapIsContainAndDrainSupported,                //59
    EnSMPCapIsProfilesConfigSupported,                 //60
    EnSMPCapIsBERConfigSupported,                      //61
    EnSMPCapIsGlobalOOOV2Supported,                    //62
    EnSMPCapIsVLBufferWeightSupported,                 //63
    EnSMPCapIsEndPortPlaneFilterConfigSupported,       //64
    EnSMPCapIsEntryPlaneFilterSupported,               //65
    EnSMPCapIsBwUtiliztionSupported,                   //66
    EnSMPCapIsExtendedSwitchIfoSupported,              //67
    EnSMPCapIsFabricManagerInfoSupported,              //68
    EnSMPCapIsExtPortInfoUnhealthyReasonSupported,     //69
    EnSMPCapIsMulticastPrivateLinearForwardingSupported,  //70
    EnSMPCapIsPortSLToMulticastPrivateLFTMapSupported, //71
    EnSMPCapIsChassisInfoSupported,                    //72
    EnSMPCapIsPLRTriggerSupported,                     //73
    EnSMPCapIsFwISSUSupported,                         //74
    EnSMPCapIsNVLinkFecModeSupported,                  //75
    EnSMPCapIsPortRecoveryPolicyConfigSupported,       //76
    EnSMPCapIsNMXAdminstateSupported,                  //77
    EnSMPCapIsMultiplaneAsymmetrySupported,            //78
    EnSMPCapIsUtcTimestampSupported,                   //79
    EnSMPCapIsCpoIndicationSupported,                  //80
    EnSMPCapIsLinkAutoRetrainSupported,                //81
    EnSMPCapLast = EnSMPCapIsLinkAutoRetrainSupported,
    EnSMPCapNone = 0xffff
} EnSMPCapabilityMaskBit_t;

//GMP Capability Mask bits
//note: when adding new values, update the 'first' and the 'last'
typedef enum {
    EnGMPCapIsPortPowerStateSupported = 0,             //   vs-spec::IsPortPowerStateSupported
    EnGMPCapFirst = EnGMPCapIsPortPowerStateSupported,
    EnGMPCapIsDeviceSoftResetSupported,                //1  vs-spec::IsDeviceSoftResetSupported
    EnGMPCapIsExtPortAccessSupported,                  //2  vs-spec::IsExtGPIOPortAccessSupported
    EnGMPCapIsPhyConfigSupported,                      //3  vs-spec::IsPhyConfigSupported
    EnGMPCapIsVendSpecLedControlSupported,             //4  vs-spec::IsVendSpecLedControlSupported
    EnGMPCapIsConfigSpaceAccessSupported,              //5  vs-spec::IsConfigSpaceAccessSupported
    EnGMPCapIsPerVLCountersSupported,                  //6  vs-spec::IsPerVLCountersSupported
    EnGMPCapIsPerVLExtendedSupported,                  //7  vs-spec::IsPerVLExtendedSupported
    EnGMPCapIsPortLLRStatisticsSupported,              //8  vs-spec::IsPortLLRStatisticsSupported
    EnGMPCapIsCounterGroupsSupported,                  //9  vs-spec::IsCounterGroupsSupported
    EnGMPCapIsEnhancedConfigSpaceAccessSupported,      //10 vs-spec::IsEnhancedConfigSpaceAccess-Supported
    EnGMPCapIsFlowMonitoringSupported,                 //11 vs-spec::IsFlowMonitoringSupported
    EnGMPCapIsCongestionNotificationSupported,         //12 vs-spec::IsCongestionNotificationSupported
    EnGMPCapIsVirtualSwitchSupported,                  //13 vs-spec::IsVirtualSwitchSupported
    EnGMPCapIsPortXmitQLengthSupported,                //14 vs-spec::IsPortXmitQLengthSupported
    EnGMPCapIsMulticastForwardingTableSupported,       //15 vs-spec::IsMulticastForwardingTableSupported
    EnGMPCapIsPortXmitWaitVLExtendedSupported,         //16 vs-spec::IsPortXmitWaitVLExtendedSupported
    EnGMPCapIsExtI2CPortAccessSupported,               //17 vs-spec::IsExtI2CPortAccessSupported
    EnGMPCapIsDiagnosticDataSupported,                 //18 vs-spec::IsDiagnosticDataSupported
    EnGMPCapIsMaxRetransmissionRateSupported,          //19 vs-spec::IsMaxRetransmissionRateSupported
    EnGMPCapIsAccessRegisterSupported,                 //20 vs-spec::IsAccessRegisterGMPSupported
    EnGMPCapIsPortHistogramsSupported,                 //21 vs-spec::IsPortHistogramsSupported
    EnGMPCAPIsPoolHistogramsSupported,                 //22 vs-spec::IsPoolHistogramsSupported
    EnGMPCAPIsVPortCountersSupported,                  //23 vs-spec::IsVPortCountersSupported
    EnGMPCAPIsDiagnosticDataIdentificationSupported,   //24 vs-spec::IsDiagnosticDataIdentification-Supported
    EnGMPCAPIsEnhPort0DeviceResetSupported,            //25 vs-spec::IsEnhPort0DeviceResetSupported
    EnGMPCAPIsDevInfoSupported,                        //26 vs-spec::IsDevInfoSupported
    EnGMPCAPIsMADAddlStatusSupported,                  //27 vs-spec::IsMADAddlStatusSupported
    EnGMPCAPIsCableBurnSupported,                      //28 vs-spec::IsCableBurnSupported
    EnGMPCAPIsMirroringEventsSupported,                //29 vs-spec::IsMirroringEventsSupported
    EnGMPCAPIsBERMonitoringSupported,                  //30 vs-spec::IsBERMonitoringSupported
    EnGMPCAPIsBERMonitoringEventsSupported,            //31 vs-specIsBERMonitoringEventsSupported
    EnGMPIsDCQCNSupported,                             //32 vs-spec::IsDCQCNSupported
    EnGMPCAPIsPLRMaxRetransmissionRateSupported,       //33 vs-spec::IsPLRMaxRetransmissionRate-Supported
    EnGMPCAPIsEffectiveCounterSupported,               //34 vs-spec::IsEffectiveCounterSupported
    EnGMPCAPIsRawBerPerLaneSupported,                  //35 vs-spec::IsRawBerPerLaneSupported
    EnGMPCAPIsGMPAccessRegisterSupported,              //36 vs-spec::IsAccessRegisterSupported
    EnGMPCAPIsMirroringInfoSupported,                  //37 vs-spec::IsMirroringInfoSupported
    EnGMPCAPIsExtendedAddressModeSupported,            //38 vs-spec::IsExtendedAddressModeSupported
    EnGMPCAPIsSwitchNetworkInfoSupported,              //39 vs-spec::IsSwitchNetworkInfoSupported
    EnGMPCAPIsCreditWatchdogTimeoutCountersSupported,  //40 vs-spec::IsCreditWatchdogTimeoutCountersSupported
    EnGMPCAPIsFastRecoveryCountersSupported,           //41 vs-spec::IsFastRecoveryCountersSupported
    EnGMPCAPIsGeneralResetSupported,                   //42 vs-spec::IsGeneralResetSupported
    EnGMPCAPIsBufferHistogramSupported,                //43 vs-spec::IsBufferHistogramSupported
    EnGMPCAPIsPortsHistogramSupported,                 //44 vs-spec::IsPortsHistogramSupported
    EnGMPCAPIsPacketBufferAllocControlSupported,       //45 vs-spec::IsPacketBufferAllocControlSupported
    EnGMPCAPIsPortGeneralCountersSupported,            //46 vs-spec::IsPortGeneralCountersSupported
    EnGMPCAPIsPortRecoveryPolicyCountersSupported,     //47 vs-spec::IsPortRecoveryPolicyCountersSupported
    EnGMPCAPIsDualVSKeySupported,                      //48 vs-spec::IsDualVSKeySupported
    EnGMPCapLast = EnGMPCAPIsDualVSKeySupported,
    EnGMPCapNone = 0xffff
} EnGMPCapabilityMaskBit_t;

typedef enum {
    EnSPCapSL2VL_VL2VL_Capability,
    EnSPCapAR_supported,
    EnSPCapIsPerVLCountersSupported,
    EnSPCapCC_supported,
    EnSPCapIsPerVLExtendedSupported,
    EnSPCapIsPortXmitWaitVLExtendedSupported,
    EnSPCapIsDiagnosticDataSupported
} EnSpecialPortsCapabilityMaskBit_t;

enum EnCCCapabilityMaskBit {
    EnCCHCAStatisticsQuerySupported = 1,
    EnCCHCA_AlgoConfigParamCounetrsSupported = 2,
    EnCCHCA_PortProfileSettingsGranularitySupported = 3,
    EnCCHCA_NPParametersRttSlModeSupported = 4,
    EnCCHCA_DualCCKeyForAttrAuthSupported = 5,
    EnCCHCA_GeneralSettingsEnCCPerPlaneSupported = 6,
};

enum PMClassPortInfoCapabilityMask {
    AllPortSelect = 8,
    IsExtendedWidthSupported,
    IsExtendedWidthSupportedNoIETF,
    SamplesOnlySupported,
    PortCountersXmitWaitSupported,
    IsInhibitLimitedPkeyMulticastConstraintErrors,
    IsRSFECCountersSupported,
    IsQP1DroppedSupported
};

enum PMClassPortInfoCapabilityMask2 {
    IsPMKeySupported = 0,
    IsAdditionalPortCountersExtendedSupported,
    IsVirtualizationSupported
};

enum NVLClassPortInfoCapabilityMask {
    NVL_CPI_CapMas_IsVendKeySupported = 8,
    NVL_CPI_CapMas_IsMultipleManagersSupported,
    NVL_CPI_CapMas_IsPenaltyBoxSupported
};



class CapabilityModule;
class Ibis;

class CapabilityMaskConfig {
    friend class CapabilityModule;
protected:
    u_int8_t                         m_mask_first_bit; //first bit number
    u_int8_t                         m_mask_last_bit; //last bit number

    //devices that don't support the capability mask mad
    map_ven_dev_to_capability_mask_t m_unsupported_mad_devices;
    //devices that have different mask per fw
    map_ven_id_dev_id_2_fw_data_t    m_fw_devices;
    //guids with prefixes to query or mask map
    map_prefix_2_guids_data_t        m_prefix_guids_2_mask;

    ////////////////////////////////////////
    //auxiliary data
    ////////////////////////////////////////

    //node guid to fw map
    map_uint64_fw_version_obj_t      m_guid_2_fw;
    //node guid to capability mask. guids that we already checked, and cached their mask
    map_uint64_capability_mask_t     m_guid_2_mask;

    string                           m_what_mask;
    string                           m_section_header;
    string                           m_section_footer;
    string                           m_unsupported_mad_devs_header;
    string                           m_fw_devs_header;
    string                           m_prefix_guids_2_mask_header;
public:
    CapabilityMaskConfig(u_int8_t mask_first_bit, u_int8_t mask_last_bit):
        m_mask_first_bit(mask_first_bit), m_mask_last_bit(mask_last_bit) {};
    virtual ~CapabilityMaskConfig() {};

    virtual int Init();
    //Init mask for SwitchX, ConnectX3, Golan.
    //These devices have old fw that doesn't support capability mask,
    //so store the mask for them, that tells what capabilities are supported
    virtual void InitMask(capability_mask_t &mask) = 0;

    //Init FW version, from which connectX3 supports capability mask
    virtual void InitFWConnectX3(fw_version_obj_t &fw) = 0;
    //Init FW version, from which connectIB supports capability mask
    virtual void InitFWConnectIB(fw_version_obj_t &fw) = 0;

    int AddFw(u_int64_t guid, fw_version_obj_t &fw);
    int GetFw(u_int64_t guid, fw_version_obj_t &fw);
    int AddCapabilityMask(u_int64_t guid, capability_mask_t &mask);
    void AddFwDevice(u_int32_t ven_id, device_id_t dev_id,
                     fw_version_obj_t &fw, query_or_mask_t &query_or_mask);
    void RemoveFwDevice(u_int32_t ven_id, device_id_t dev_id);
    int GetFwConfiguredMask(uint32_t ven_id, device_id_t dev_id,
                            fw_version_obj_t &fw, capability_mask_t &mask,
                            bool *is_only_fw = NULL);
    void AddUnsupportMadDevice(u_int32_t ven_id, device_id_t dev_id,
                               capability_mask_t &mask);
    void RemoveUnsupportMadDevice(u_int32_t ven_id, device_id_t dev_id);
    int AddPrefixGuid(u_int8_t prefix_len, u_int64_t guid,
                      query_or_mask_t &qmask, string &output);

    bool IsUnsupportedMadDevice(uint32_t ven_id, device_id_t dev_id,
                                capability_mask_t &mask);

    bool IsMaskKnown(u_int64_t guid);

    bool IsSupportedCapability(const IBNode *node, u_int8_t cap_bit) const;
    bool IsLongestPrefixMatch(u_int64_t guid, u_int8_t &prefix_len,
                              u_int64_t &matched_guid, query_or_mask &qmask);
    int DumpCapabilityMaskFile(ostream& sout);

    int DumpGuid2Mask(ostream& sout, IBFabric *p_discovered_fabric);

    int GetCapability(IBNode *node, capability_mask_t& mask);
};

class SmpMask: public CapabilityMaskConfig {
    friend class CapabilityModule;
public:
    SmpMask();
    virtual ~SmpMask() {}
    virtual void InitMask(capability_mask_t &mask);
    //Init FW version, from which connectX3 supports SMP capability mask
    virtual void InitFWConnectX3(fw_version_obj_t &fw);
    //Init FW version, from which connectIB supports SMP capability mask
    virtual void InitFWConnectIB(fw_version_obj_t &fw);
    virtual int Init();
    void DumpCSVVSGeneralInfo(stringstream &sout);
};

class GmpMask: public CapabilityMaskConfig {
    friend class CapabilityModule;
public:
    GmpMask();
    virtual ~GmpMask() {}
    virtual void InitMask(capability_mask_t &mask);
    //Init FW version, from which connectX3 supports GMP capability mask
    virtual void InitFWConnectX3(fw_version_obj_t &fw);
    //Init FW version, from which connectIB supports GMP capability mask
    virtual void InitFWConnectIB(fw_version_obj_t &fw);
    virtual int Init();
};

class CapabilityModule {
    SmpMask smp_mask;
    GmpMask gmp_mask;
public:
    CapabilityModule() {}
    ~CapabilityModule() {}
    int Init();

    inline SmpMask &GetSmpMask() { return smp_mask; };
    inline GmpMask &GetGmpMask() { return gmp_mask; };
    int DumpCapabilityMaskFile(ostream &sout);
    //for debug purposes
    int DumpGuid2Mask(ostream &sout, IBFabric *p_discovered_fabric);
    int ParseCapabilityMaskFile(const char *file_name);

    int AddSMPFw(u_int64_t guid, fw_version_obj_t &fw);
    int GetSMPFw(u_int64_t guid, fw_version_obj_t &fw);
    int AddSMPCapabilityMask(u_int64_t guid, capability_mask_t &mask);
    void AddSMPFwDevice(u_int32_t ven_id, device_id_t dev_id, fw_version_obj_t &fw, query_or_mask_t &query_or_mask);

    bool IsSMPUnsupportedMadDevice(uint32_t ven_id, device_id_t dev_id,
                                   capability_mask_t &mask);
    bool IsSMPMaskKnown(u_int64_t guid);
    bool IsSupportedSMPCapability(const IBNode *node, u_int8_t cap_bit) const;

    bool IsLongestSMPPrefixMatch(u_int64_t guid, u_int8_t &prefix_len,
                                 u_int64_t &matched_guid,
                                 query_or_mask &qmask);
    int  GetSMPFwConfiguredMask(uint32_t ven_id, device_id_t dev_id,
                                fw_version_obj_t &fw, capability_mask_t &mask,
                                bool *is_only_fw = NULL);

    int AddGMPFw(u_int64_t guid, fw_version_obj_t &fw);
    int GetGMPFw(u_int64_t guid, fw_version_obj_t &fw);
    int AddGMPCapabilityMask(u_int64_t guid, capability_mask_t &mask);
    bool IsGMPUnsupportedMadDevice(uint32_t ven_id, device_id_t dev_id,
                                   capability_mask_t &mask);
    bool IsGMPMaskKnown(u_int64_t guid);
    bool IsLongestGMPPrefixMatch(u_int64_t guid, u_int8_t &prefix_len,
                                 u_int64_t &matched_guid,
                                 query_or_mask &qmask);
    int GetGMPFwConfiguredMask(uint32_t ven_id, device_id_t dev_id,
                               fw_version_obj_t &fw, capability_mask_t &mask,
                               bool *is_only_fw = NULL);
    bool IsSupportedGMPCapability(const IBNode *node, u_int8_t cap_bit) const;

    int GetFw(u_int64_t guid, fw_version_obj_t &fw);

    int GetCapability(IBNode *node, bool is_gmp, capability_mask_t& mask);
};

#define BOOL_TO_YES_NO_STR(val) (val ? "Yes" : "No")

template <class T> class PairsContainer {
private:
    std::set< std::pair<T, T> > pairs;

    const std::pair<T, T> Make(const T& t1, const T& t2) const
    {
        if (t1 > t2)
            return (std::make_pair(t1, t2));

        return (std::make_pair(t2, t1));
    }

public:
    bool Contains(const T &t1, const T &t2) const
    {
        return(this->pairs.find(this->Make(t1, t2)) != this->pairs.end());
    }

    void Add(const T &t1, const T &t2)
    {
        this->pairs.insert(this->Make(t1, t2));
    }
};


/****************************************************/
/* sections defines */
#define SECTION_NODES                "NODES"
#define SECTION_PORTS                "PORTS"
#define SECTION_SWITCHES             "SWITCHES"
#define SECTION_PORT_HIERARCHY_INFO  "PORT_HIERARCHY_INFO"
#define SECTION_PHYSICAL_HIERARCHY_INFO  "PHYSICAL_HIERARCHY_INFO"
#define SECTION_FEC_MODE             "FEC_MODE"
#define SECTION_EXTENDED_NODE_INFO   "EXTENDED_NODE_INFO"
#define SECTION_EXTENDED_PORT_INFO   "EXTENDED_PORT_INFO"
#define SECTION_EXTENDED_SWITCH_INFO "EXTENDED_SWITCH_INFO"
#define SECTION_CHASSIS_INFO         "CHASSIS_INFO"
#define SECTION_PORT_RECOVERY_POLICY_CONFIG "PORT_RECOVERY_POLICY_CONFIG"
#define SECTION_PORT_INFO_EXTENDED   "PORT_INFO_EXTENDED"
#define SECTION_LINKS                "LINKS"
#define SECTION_PM_INFO              "PM_INFO"
#define SECTION_PM_PORT_SAMPLES_CONTROL  "PM_PORT_SAMPLES_CONTROL"
#define SECTION_PM_PORT_SAMPLES_RESULT   "PM_PORT_SAMPLES_RESULT"
#define SECTION_PM_DELTA             "PM_DELTA"
#define SECTION_SM_INFO              "SM_INFO"
#define SECTION_NODES_INFO           "NODES_INFO"
#define SECTION_AGUID                "AGUID"
#define SECTION_PKEY                 "PKEY"
#define SECTION_MLNX_CNTRS_INFO      "MLNX_CNTRS_INFO"
#define SECTION_VNODES               "VNODES"
#define SECTION_VPORTS               "VPORTS"
#define SECTION_VPORTS_GUID_INFO     "VPORTS_GUID_INFO"
#define SECTION_QOS_CONFIG_SL        "QOS_CONFIG_SL"
#define SECTION_QOS_CONFIG_VL        "QOS_CONFIG_VL"
#define SECTION_VPORTS_QOS_CONFIG_SL "VPORTS_QOS_CONFIG_SL"
#define SECTION_TEMP_SENSING         "TEMP_SENSING"
#define SECTION_BER_TEST             "BER_TEST"
#define SECTION_PHY_TEST             "PHY_TEST"
#define SECTION_ROUTERS_INFO         "ROUTERS_INFO"
#define SECTION_ROUTERS_ADJ_TBL      "ROUTERS_ADJ_SITE_LOCAL_SUBNETS_TABLE"
#define SECTION_ROUTERS_ADJ_LID_TBL  "ROUTERS_ADJ_SUBNETS_LID_INFO_TABLE"
#define SECTION_ROUTERS_NEXT_HOP_TBL "ROUTERS_NEXT_HOP_TABLE"
#define SECTION_SHARP_AN_INFO        "SHARP_AN_INFO"
#define SECTION_SHARP_PM_COUNTERS    "SHARP_PM_COUNTERS"
#define SECTION_HBA_PORT_COUNTERS    "HBA_PORT_COUNTERS"
#define SECTION_GENERAL_INFO_SMP     "GENERAL_INFO_SMP"
#define SECTION_AR_INFO              "AR_INFO"
#define SECTION_PORT_DRS             "PORT_DRS"
#define SECTION_HBF_CONFIG           "HBF_CONFIG"
#define SECTION_VL_ARBITRATION_TBL   "VL_ARBITRATION_TABLE"
#define SECTION_WHBF_CONFIG          "WHBF_CONFIG"
#define SECTION_RN_COUNTERS          "RN_COUNTERS"
#define SECTION_HBF_PORT_COUNTRERS   "HBF_PORT_COUNTERS"
#define SECTION_STAGE_PERF_INFO      "STAGE_PERF_INFO"
#define SECTION_CSV_PERF_INFO        "CSV_PERF_INFO"
#define SECTION_SWITCH_NETWORK_INFO  "SWITCH_NETWORK_INFO"
#define SECTION_IBIS_MAD_INFO        "IBIS_MAD_INFO"
#define SECTION_IBIS_PERF_INFO       "IBIS_PERF_INFO"

// pFRN headers
#define SECTION_PFRN_CONFIG          "PFRN_CONFIG"
#define SECTION_N2N_CLASS_PORT_INFO  "N2N_CLASS_PORT_INFO"
#define SECTION_NEIGHBORS_INFO       "NEIGHBORS_INFO"
#define SECTION_N2N_KEY_INFO         "N2N_KEY_INFO"

// CC config headers
#define SECTION_CC_ENHANCED_INFO              "CC_ENHANCED_INFO"
#define SECTION_CC_SWITCH_GENERAL_SETTINGS    "CC_SWITCH_GENERAL_SETTINGS"
#define SECTION_CC_PORT_PROFILE_SETTINGS      "CC_PORT_PROFILE_SETTINGS"
#define SECTION_CC_SL_MAPPING_SETTINGS        "CC_SL_MAPPING_SETTINGS"
#define SECTION_CC_HCA_GENERAL_SETTINGS       "CC_HCA_GENERAL_SETTINGS"
#define SECTION_CC_HCA_RP_PARAMETERS          "CC_HCA_RP_PARAMETERS"
#define SECTION_CC_HCA_NP_PARAMETERS          "CC_HCA_NP_PARAMETERS"
#define SECTION_CC_HCA_STATISTICS_QUERY       "CC_HCA_STATISTICS_QUERY"
// CC algo
#define SECTION_CC_HCA_ALGO_CONFIG_SUP        "CC_HCA_ALGO_CONFIG_SUPPORT"
#define SECTION_CC_HCA_ALGO_CONFIG            "CC_HCA_ALGO_CONFIG"
#define SECTION_CC_HCA_ALGO_CONFIG_PARAMS     "CC_HCA_ALGO_CONFIG_PARAMS"
#define SECTION_CC_HCA_ALGO_COUNTERS          "CC_HCA_ALGO_COUNTERS"

// fast recovery
#define SECTION_CREDIT_WATCHDOG_TIMEOUT_COUNTERS  "CREDIT_WATCHDOG_TIMEOUT_COUNTERS"
#define SECTION_FAST_RECOVERY_COUNTERS            "FAST_RECOVERY_COUNTERS"
#define SECTION_PROFILES_CONFIG                   "PROFILES_CONFIG"
#define SECTION_CREDIT_WATCHDOG_CONFIG            "CREDIT_WATCHDOG_CONFIG"
#define SECTION_BER_CONFIG                        "BER_CONFIG"
#define SECTION_PORT_GENERAL_COUNTERS             "PORT_GENERAL_COUNTERS"

#define SECTION_PORT_POLICY_RECOVERY_COUNTERS     "PORT_POLICY_RECOVERY_COUNTERS"

//Histogram Info
#define SECTION_PERFORMANCE_HISTOGRAM_INFO           "PERFORMANCE_HISTOGRAM_INFO"
#define SECTION_PERFORMANCE_HISTOGRAM_BUFFER_CONTROL "PERFORMANCE_HISTOGRAM_BUFFER_CONTROL"
#define SECTION_PERFORMANCE_HISTOGRAM_BUFFER_DATA    "PERFORMANCE_HISTOGRAM_BUFFER_DATA"
#define SECTION_PERFORMANCE_HISTOGRAM_PORTS_CONTROL  "PERFORMANCE_HISTOGRAM_PORTS_CONTROL"
#define SECTION_PERFORMANCE_HISTOGRAM_PORTS_DATA     "PERFORMANCE_HISTOGRAM_PORTS_DATA"

// NVLink
#define SECTION_NVL_FEC_MODE                            "NVL_FEC_MODE"
#define SECTION_NVL_ANYCAST_LID_INFO                    "NVL_ANYCAST_LID_INFO"
#define SECTION_NVL_HBF_CONFIG                          "NVL_HBF_CONFIG"
#define SECTION_NVL_CONTAIN_AND_DRAIN_INFO              "NVL_CONTAIN_AND_DRAIN_INFO"
#define SECTION_NVL_CONTAIN_AND_DRAIN_PORT_STATE        "NVL_CONTAIN_AND_DRAIN_PORT_STATE"
#define SECTION_NVL_CLASS_PORT_INFO                     "NVL_CLASS_PORT_INFO"
#define SECTION_NVL_REDUCTION_INFO                      "NVL_REDUCTION_INFO"
#define SECTION_NVL_REDUCTION_FORWARDING_TABLE          "NVL_REDUCTION_FORWARDING_TABLE"
#define SECTION_NVL_PENALTY_BOX_CONFIG                  "NVL_PENALTY_BOX_CONFIG"
#define SECTION_NVL_REDUCTION_PORT_INFO                 "NVL_REDUCTION_PORT_INFO"
#define SECTION_NVL_REDUCTION_CONFIGURE_MLID_MONITORS   "NVL_REDUCTION_CONFIGURE_MLID_MONITORS"
#define SECTION_NVL_REDUCTION_COUNTERS                  "NVL_REDUCTION_COUNTERS"
#define SECTION_NVL_REDUCTION_ROUNDING_MODE             "NVL_REDUCTION_ROUNDING_MODE"
#define SECTION_LINEAR_FORWARDING_TABLE_SPLIT           "LINEAR_FORWARDING_TABLE_SPLIT"

#define SECTION_COMBINED_CABLE_INFO "CABLE_INFO"
/****************************************************/
/* defines */

/*
 * As the ttlog buffer size is 4096 and if 'ibdiag.last_error' buffer will
 * be copied to ttlog buffer it will cause to overflow.
 * So the last_IBDIAG_ERR_SIZE should be less than the ttlog buffer because
 * the ttlog function add to the last_error buffer some overhead of info of
 * the function/error_level and then copy it to the ttlog buffer.
 * */
#define IBDIAG_ERR_SIZE                             3840            /* 3840 bytes */


#define IBDIAG_MAX_HOPS                             64              /* maximum hops for discovery BFS */
#define IBDIAG_MAX_SUPPORTED_NODE_PORTS             254             /* for lft and mft retrieve */
#define IBDIAG_BER_THRESHOLD_OPPOSITE_VAL           0xe8d4a51000    /* 10^12 */

#define OPTION_DEF_VAL_NULL "(null)"
#define OPTION_DEF_VAL_TRUE "TRUE"
#define OPTION_DEF_VAL_FALSE "FALSE"

#define PORT_PROFILES_IN_BLOCK 128

#define PORT_RECOVERY_POLICY_NUM_OF_PROFILES 8

enum OVERFLOW_VALUES {
    OVERFLOW_VAL_NONE   = 0x0,
    OVERFLOW_VAL_4_BIT  = 0xf,
    OVERFLOW_VAL_8_BIT  = 0xff,
    OVERFLOW_VAL_16_BIT = 0xffff,
    OVERFLOW_VAL_32_BIT = 0xffffffff,
    OVERFLOW_VAL_64_BIT = 0xffffffffffffffffULL
};

enum IBDIAG_RETURN_CODES {
    IBDIAG_SUCCESS_CODE = 0x0,
    IBDIAG_ERR_CODE_FABRIC_ERROR = 0x1,
    IBDIAG_ERR_CODE_NO_MEM = 0x3,
    IBDIAG_ERR_CODE_DB_ERR = 0x4,
    IBDIAG_ERR_CODE_IBDM_ERR = 0x5,
    IBDIAG_ERR_CODE_INIT_FAILED = 0x6,
    IBDIAG_ERR_CODE_NOT_READY = 0x7,
    IBDIAG_ERR_CODE_IO_ERR = 0x8,
    IBDIAG_ERR_CODE_CHECK_FAILED = 0x9,
    IBDIAG_ERR_CODE_PARSE_FILE_FAILED = 0xA,
    IBDIAG_ERR_CODE_EXCEEDS_MAX_HOPS = 0x10,
    IBDIAG_ERR_CODE_DUPLICATED_GUID = 0x11,
    IBDIAG_ERR_CODE_INCORRECT_ARGS = 0x12,
    IBDIAG_ERR_CODE_DISCOVERY_NOT_SUCCESS = 0x13,
    IBDIAG_ERR_CODE_TRY_TO_DISCONNECT_CONNECTED_PORT = 0x14,
    IBDIAG_ERR_CODE_ILLEGAL_FUNCTION_CALL = 0x15,
    IBDIAG_ERR_CODE_FILE_NOT_OPENED =0x16,
    IBDIAG_ERR_CODE_FILE_NOT_EXIST =0x17,
    IBDIAG_ERR_CODE_DISABLED = 0x18,
    IBDIAG_ERR_CODE_PORT_GUID_NOT_FOUND = 0x19,
    IBDIAG_ERR_CODE_NODE_GUID_NOT_FOUND = 0x20,
    IBDIAG_ERR_CODE_NOT_SUPPORTED = 0x21,
    IBDIAG_ERR_CODE_CAP_MODULE_NULL = 0x22,
    IBDIAG_ERR_CODE_DR_NULL = 0x23,
    IBDIAG_ERR_CODE_LID_ZERO = 0x24,
    IBDIAG_ERR_CODE_LAST
};

#define PATH_INFO_PREFIX_SEFIX "-I- --------------------------------------"\
                               "--------\n"
#define  TRAVERSE_SRC_TO_DEST PATH_INFO_PREFIX_SEFIX  \
    "-I- Traversing the path from source to destination\n" \
    PATH_INFO_PREFIX_SEFIX

#define  TRAVERSE_LOCAL_TO_SRC PATH_INFO_PREFIX_SEFIX \
    "-I- Traversing the path from local to source\n"   \
        PATH_INFO_PREFIX_SEFIX

/****************************************************/
/* general defines */
#ifndef CLEAR_STRUCT
    #define CLEAR_STRUCT(n) memset(&(n), 0, sizeof(n))
#endif
#define OFFSET_OF(type, field) ((unsigned long) &(((type *) 0)->field))

#ifndef IN
    #define IN
#endif
#ifndef OUT
    #define OUT
#endif
#ifndef INOUT
    #define INOUT
#endif

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

/****************************************************/
/* log Macros */
#ifdef DEBUG
    #define IBDIAG_LOG(level, fmt, ...) TT_LOG(TT_LOG_MODULE_IBDIAG, level, fmt, ## __VA_ARGS__)
    #define IBDIAG_ENTER TT_ENTER( TT_LOG_MODULE_IBDIAG )
    #define IBDIAG_RETURN(rc) do { TT_EXIT( TT_LOG_MODULE_IBDIAG ); return (rc); } while (0)
    #define IBDIAG_RETURN_VOID do { TT_EXIT( TT_LOG_MODULE_IBDIAG ); return; } while (0)
#else           /*def DEBUG */
    #define IBDIAG_LOG(level, fmt, ...)
    #define IBDIAG_ENTER
    #define IBDIAG_RETURN(rc) return (rc)
    #define IBDIAG_RETURN_VOID return
#endif          /*def DEBUG */

#ifndef WIN32
    #define IBDIAG_LOG_NAME "/tmp/ibutils2_ibdiag.log"
#else
    #define IBDIAG_LOG_NAME "\\tmp\\ibutils2_ibdiag.log"
#endif      /* ndef WIN32 */

#endif          /* not defined IBDIAG_TYPES_H_ */
