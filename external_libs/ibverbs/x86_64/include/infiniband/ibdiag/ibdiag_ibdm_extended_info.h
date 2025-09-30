/*
 * Copyright (c) 2004-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
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


#ifndef IBDIAG_IBDM_EXTENDED_INFO_H
#define IBDIAG_IBDM_EXTENDED_INFO_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <vector>
using namespace std;

#include <infiniband/ibdm/Fabric.h>
#include <ibis/packets/packets_layouts.h>

#include "ibdiag_types.h"



/*****************************************************/

//Go over speed's bits and add speed name
//regular speeds section at offset 0, first byte
//extended speeds section at offset 8, second byte
//mlnx speeds section at offset 16, third byte
//extended speeds 2 section at offset 24, fourth byte
static inline string supspeed2char(const u_int32_t speed)
{
    string speeds_str = "", section_speed_str = "";

    //check by speeds sections, as many bits are 0
    vector<u_int32_t> check_offset{0, 8, 16, 24};
    u_int32_t offset = 0, mask = 0, speed_bit = 0;
    u_int8_t section = 0;
    bool is_unknown = false;

    //look at different speeds sections
    for (unsigned int i = 0; i < check_offset.size(); i++) {
        speed_bit = 0;
        offset = check_offset[i];
        mask = 0xff << offset;
        section = (u_int8_t)((speed & mask) >> offset);
        while (section) {
            if (section & 0x1) {
                section_speed_str =
                    speed2char((IBLinkSpeed)(0x1 << (speed_bit + offset)));
                if (section_speed_str != IB_UNKNOWN_LINK_SPEED_STR)
                    speeds_str += (section_speed_str + " or ");
                else
                    is_unknown = true;
            }
            section = (u_int8_t)(section >> 1);
            speed_bit ++;
        }
    }

    if (is_unknown)
        speeds_str += "UNKNOWN";
    else
        //remove last " or "
        if (speeds_str.size() > 4)
            speeds_str.replace(speeds_str.size() - 4, string::npos, "");

    return speeds_str;
}

static inline const char * supwidth2char(const u_int8_t w)
{
    switch (w) {
    case 1:     return("1x");
    case 2:     return("4x");
    case 3:     return("1x or 4x");
    case 4:     return("8x");
    case 5:     return("1x or 8x");
    case 6:     return("4x or 8x");
    case 7:     return("1x or 4x or 8x");
    case 8:     return("12x");
    case 9:     return("1x or 12x");
    case 10:    return("4x or 12x");
    case 11:    return("1x or 4x or 12x");
    case 12:    return("8x or 12x");
    case 13:    return("1x or 8x or 12x");
    case 14:    return("4x or 8x or 12x");
    case 15:    return("1x or 4x or 8x or 12x");
    case 16:    return("2x");
    case 17:    return("1x or 2x");
    case 18:    return("2x or 4x");
    case 19:    return("1x or 2x or 4x");
    case 20:    return("2x or 8x");
    case 21:    return("1x or 2x or 8x");
    case 22:    return("2x or 4x or 8x");
    case 23:    return("1x or 2x or 4x or 8x");
    case 24:    return("2x or 12x");
    case 25:    return("1x or 2x or 12x");
    case 26:    return("2x or 4x or 12x");
    case 27:    return("1x or 2x or 4x or 12x");
    case 28:    return("2x or 8x or 12x");
    case 29:    return("1x or 2x or 8x or 12x");
    case 30:    return("2x or 4x or 8x or 12x");
    case 31:    return("1x or 2x or 4x or 8x or 12x");
    default:    return("UNKNOWN");
    }
};


IBLinkSpeed CalcFinalSpeed(u_int32_t speed1, u_int32_t speed2);
IBLinkWidth CalcFinalWidth(u_int8_t width1, u_int8_t width2);
u_int64_t CalcLinkRate(IBLinkWidth link_width, IBLinkSpeed link_speed);

// get the 'extended speeds 2 | mlnx extended speeds | extended speeds | regular speeds'
// for enabled supported and active
void GetTotalSpeeds(IN SMP_PortInfo *p_port_info,
                    IN SMP_MlnxExtPortInfo *p_mlnx_ext_port_info,
                    IN u_int32_t cap_mask,
                    IN u_int16_t cap_mask2,
                    OUT u_int32_t *sup_speed,
                    OUT u_int32_t *en_speed,
                    OUT u_int32_t *actv_speed);

static inline bool IsValidEnableSpeed(u_int32_t link_speed_en, u_int32_t link_speed_sup)
{
    return (((u_int32_t)link_speed_en | (u_int32_t)link_speed_sup) == (u_int32_t)link_speed_sup);
}
static inline bool IsValidEnableWidth(u_int8_t link_width_en, u_int8_t link_width_sup)
{
    return (((u_int8_t)link_width_en | (u_int8_t)link_width_sup) == (u_int8_t)link_width_sup);
}

static inline u_int8_t LinkWidthToLane(IBLinkWidth link_width)
{
    IBDIAG_ENTER;

    switch (link_width) {
    case IB_LINK_WIDTH_1X:  IBDIAG_RETURN(1); break;
    case IB_LINK_WIDTH_4X:  IBDIAG_RETURN(4); break;
    case IB_LINK_WIDTH_8X:  IBDIAG_RETURN(8); break;
    case IB_LINK_WIDTH_12X: IBDIAG_RETURN(12); break;
    case IB_LINK_WIDTH_2X:  IBDIAG_RETURN(2); break;
    default: IBDIAG_RETURN(0); break;
    }
}

/*****************************************************/
class IBDMExtendedInfo {
private:
    //members
    vector_p_node   nodes_vector;
    vector_p_port   ports_vector;
    vector_p_vport  vports_vector;
    vector_p_vnode  vnodes_vector;

    list_p_sm_info_obj                  sm_info_obj_list;

    vector_p_smp_node_info              smp_node_info_vector;           // by node index
    vector_p_smp_switch_info            smp_switch_info_vector;         // by node index
    vector_p_smp_port_info              smp_port_info_vector;           // by port index
    vector_p_smp_port_info_ext          smp_port_info_ext_vector;       // by port index
    vector_p_smp_mlnx_ext_node_info     smp_mlnx_ext_node_info_vector;  // by node index
    vector_p_smp_mlnx_ext_port_info     smp_mlnx_ext_port_info_vector;  // by port index
    vector_p_vs_general_info            vs_general_info_vector;         // by node index
    vector_p_vs_switch_network_info     vs_switch_network_info_vector;  // by node index
    vector_p_class_port_info            pm_class_port_info_vector;      // by node index
    vector_p_pm_info_obj                pm_info_obj_vector;             // by port index
    vector_p_pm_port_samples_control    pm_port_samples_control_vector; // by port index
    vector_p_pm_option_mask             pm_option_mask_vector;          // by node index
    vector_p_vs_mlnx_cntrs_obj          vs_mlnx_cntrs_obj_vector;       // by port index
    vector_p_smp_virtual_info           smp_virtual_info_vector;        // by port index
    vector_p_smp_vport_info             smp_vport_info_vector;          // by port index
    vector_p_smp_vnode_info             smp_vnode_info_vector;          // by vport index
    vector_p_vs_ar_info                 vs_ar_info_vector;              // by node index
    vector_p_vs_plft_info               vs_plft_info_vector;            // by node index
    vector_p_vs_rn_counters             vs_rn_counters_vector;          // by port index
    vector_p_vs_routing_decision_counters  vs_routing_decision_vector;  // by port index
    vector_p_hbf_config                 hbf_config_vector;              // by node index
    vector_p_pfrn_config                pfrn_config_vector;             // by node index
    vector_p_class_port_info            n2n_class_port_info_vector;     // by node index
    vector_vec_p_neighbor_record        neighbor_record_vector;         // by node index ,block index
    vector_p_n2n_key_info               n2n_key_info_vector;            // by node index

    vector_p_smp_qos_config_sl          smp_qos_config_sl_vector;       // by port index
    vector_p_smp_qos_config_vl          smp_qos_config_vl_vector;       // by port index
    vector_p_smp_qos_config_sl          smp_vport_qos_config_sl_vector; // by vport index
    vector_p_smp_temp_sensing           smp_temp_sensing_vector;        // by node index
    vector_v_smp_vport_state            smp_vport_state_vector;         // by port index ,block index
    vector_v_smp_pkey_tbl               smp_vport_pkey_tbl_v_vector;    // by vport index, block index
    vector_v_smp_pkey_tbl               smp_pkey_tbl_v_vector;          // by port index, block index
    vector_vec_p_vl_arbitration_tbl     smp_vl_arbitration_tbl_v_vector;   // by port index, block index
    vector_v_smp_vport_guid_tbl         smp_vport_guid_tbl_v_vector;    // by port index, block index
    vector_v_smp_guid_tbl               smp_guid_tbl_v_vector;          // by port index, block index

    vector_p_smp_router_info            smp_router_info_vector;         //by node index
    vector_v_smp_adj_router_tbl         smp_adj_router_tbl_v_vector;    //by node index, block index
    vector_v_smp_next_hop_router_tbl    smp_nh_router_tbl_v_vector;     //by node index, block index
    vector_v_smp_adj_subnet_router_lid_info_tbl  smp_adj_subnet_router_lid_info_tbl_v_vector; //by node index, block index
    vector_v_smp_router_lid_tbl         smp_router_lid_tbl_v_vector;    //by node index, block index
    vector_v_ar_group_router_lid_tbl    smp_ar_group_router_lid_tb_v_vector; //by node index, block index
    vector_v_port_recovery_policy_config smp_port_recoverypolicy_config_v_vector; //by port index, policy_profile

    vec_enhanced_cc_info                cc_enhanced_info_vec;           // by node index
    vec_cc_sw_settings                  cc_sw_settings_vec;             // by node index
    vec_vec_cc_port_settings            cc_port_settings_vec_vec;       // by port index, per VL
    vec_cc_sl_mapping_settings          cc_sl_mapping_settings_vec;     // by port index
    vec_cc_hca_settings                 cc_hca_settings_vec;            // by port index
    vec_cc_hca_rp_parameters            cc_hca_rp_parameters_vec;       // by port index
    vec_cc_hca_np_parameters            cc_hca_np_parameters_vec;       // by port index
    vec_cc_hca_statistics_query         cc_hca_statistics_query_vec;    // by port index

    vec_cc_hca_algo_config              cc_hca_algo_config_sup_vec;       // by port index
    vec_vec_cc_hca_algo_config          cc_hca_algo_config_v_vec;         // by port index, per algo slot
    vec_vec_cc_hca_algo_config_params   cc_hca_algo_config_params_v_vec;  // by port index, per algo slot
    vec_vec_cc_hca_algo_counters        cc_hca_algo_counters_v_vec;       // by port index, per algo slot

    vector_p_vs_credit_wd_timeout_counters  vs_credit_wd_timeout_vector;   // by port index
    vec_vec_p_vs_fast_recovery_counters     vs_fast_recovery_cntrs_vector; // by port index per trigger
    vec_p_vs_port_general_counters          vs_port_general_counters_vector;     //   by port index
    vec_p_vs_port_recovery_policy_counters  vs_port_recovery_policy_counters_vector;  //   by port index
    vec_vec_vec_p_smp_profiles_config       smp_profiles_config_vector;    // by node index, per feature, per block
    vec_vec_p_credit_watchdog_config        credit_watchdog_config_vector; // by node index per profile
    vec_vec_p_ber_config                    ber_config_vector; // by node index per profile per ber_type

    vec_p_nvl_fec_mode                      nvl_fec_mode;                           // by port index
    vec_v_nvl_anycast_lid_info              nvl_anycast_lid_info;                   // by node index, block index
    vec_p_nvl_hbf_config                    nvl_hbf_config;                         // by port index
    vec_v_nvl_contain_and_drain_info        nvl_contain_and_drain_info;             // by node index, block index
    vec_v_nvl_contain_and_drain_port_state  nvl_contain_and_drain_port_state;       // by node index, block index

    vec_p_nvl_class_port_info               nvl_class_port_info;                    // by node index
    vec_p_nvl_reduction_info                nvl_reduction_info;                     // by node index
    vec_p_nvl_reduction_port_info           nvl_reduction_port_info;                // by port index
    vec_p_nvl_reduction_rounding_mode       nvl_reduction_rounding_mode;            // by node index
    vec_v_nvl_reduction_forwarding_table    nvl_reduction_forwarding_table;         // by node index, block index
    vec_v_nvl_penalty_box_config            nvl_penalty_box_config;                 // by node index, block index
    vec_p_nvl_reduction_configure_mlid_monitors
                                            nvl_reduction_configure_mlid_monitors;  // by port index

    vec_v_nvl_reduction_counters			nvl_reduction_counters; // by port index, block index
    vec_vec_performance_histogram_buffer_data vector_performance_histogram_buffer_data; //by port, vl and direction

    vec_p_smp_lft_split                     smp_lft_split_vec;                      // by node index

    vec_p_performance_histogram_info        performance_histogram_info; // by node index
    vec_vec_performance_histogram_buffer_control vector_performance_histogram_buffer_control; //by port index and vl_direction identifier ((vl<<1)|(dir))
    vec_v_performance_histogram_ports_control performance_histogram_ports_control_vector;     //   by port index, hist_id
    vec_v_vs_performance_histogram_ports_data performance_histogram_ports_data_vector;     //   by port index, hist_id

    vec_p_smp_extended_switch_info          smp_extended_switch_info_vector;        // by node index
    vec_p_pm_port_samples_result            pm_port_samples_result_vector;          // by port index
    vec_p_smp_chassis_info                  smp_chassis_info_vector;                // by node index

    //methods
    template <typename T>
    void addPtrToVec(vector<T*> & vector_obj, T *p_obj);

    int addPMObjectInfo(IBPort *p_port);
    int addMlnxCntrsObject(IBPort *p_port);

    template <typename T>
    T * getPtrFromVec(std::vector<T*> & vector_obj, u_int32_t create_index);

    template<typename T, typename V>
    int addDataToVec(vector<T*> & vector_obj,
            T *p_obj,
            vector<V*> & vector_data,
            const V & data);

    template <typename T, typename V>
    int addDataToVecInVec(vector<T*>& vector_obj,
            T *p_obj,
            vector< vector<V*> >& vec_of_vectors,
            u_int32_t data_idx,
            const V& data);

    template <typename T, typename V>
    int addDataToVecOfVecInVec(vector<T*>& vector_obj,
            T *p_obj,
            vector< vector< vector<V*> > >& vec_of_vec_of_vectors,
            u_int32_t data_idx1,
            u_int32_t data_idx2,
            const V& data);

    template <typename T>
    T * getPtrFromVecInVec(std::vector< std::vector<T*> > & vec_of_vectors,
            u_int32_t idx1,
            u_int32_t idx2);

    template <typename T>
    T * getPtrFromVecOfVecInVec(std::vector< std::vector< std::vector<T*> > > & vec_of_vec_of_vectors,
            u_int32_t idx1,
            u_int32_t idx2,
            u_int32_t idx3);

public:
    //methods
    void CleanUpInternalDB();
    void CleanVPortDB();
    void CleanVNodeDB();
    void CleanNVLDB();

    IBDMExtendedInfo();
    ~IBDMExtendedInfo();

    inline u_int32_t getNodesVectorSize() { return (u_int32_t)this->nodes_vector.size(); }
    inline u_int32_t getPortsVectorSize() { return (u_int32_t)this->ports_vector.size(); }
    inline u_int32_t getVPortsVectorSize() { return (u_int32_t)this->vports_vector.size(); }
    inline u_int32_t getVNodesVectorSize() { return (u_int32_t)this->vnodes_vector.size(); }

    IBNode * getNodePtr(u_int32_t node_index);
    IBPort * getPortPtr(u_int32_t port_index);
    IBVNode * getVNodePtr(u_int32_t vnode_index);
    IBVPort * getVPortPtr(u_int32_t vport_index);

    //Returns: SUCCESS_CODE / ERR_CODE_INCORRECT_ARGS / ERR_CODE_NO_MEM
    int addSMPSMInfoObj(IBPort *p_port, struct SMP_SMInfo &smpSMInfo);
    int addSMPNodeInfo(IBNode *p_node, const struct SMP_NodeInfo &smpNodeInfo);
    int addHBFConfig(IBNode *p_node, struct hbf_config &hbf);

    // pFRN (N2N, Class C)
    int addpFRNConfig(IBNode *p_node, struct SMP_pFRNConfig &pfrn);
    int addN2NClassPortInfo(IBNode *p_node, IB_ClassPortInfo &class_port_info);
    int addNeighborsRecord(IBNode *p_node, struct neighbor_record &neighbor, u_int32_t record_idx);
    int addN2NKeyInfo(IBNode *p_node, struct Class_C_KeyInfo &key_info);

    int addARInfo(IBNode *p_node, const struct adaptive_routing_info &ar_info);
    int addPLFTInfo(IBNode *p_node, const struct ib_private_lft_info &plft_info);
    int addRNCounters(IBPort *p_port, struct port_rn_counters &rn_counters);
    int addRoutingDecisionCounters(IBPort *p_port, struct port_routing_decision_counters &counters);
    int addSMPSwitchInfo(IBNode *p_node, const struct SMP_SwitchInfo &smpSwitchInfo);
    int addSMPPortInfo(IBPort *p_port, const struct SMP_PortInfo &smpPortInfo);
    int addSMPPortInfoExtended(IBPort *p_port, const struct SMP_PortInfoExtended &smpPortInfoExt);
    int addSMPExtNodeInfo(IBNode *p_node, const struct ib_extended_node_info &extNodeInfo);
    int addSMPMlnxExtPortInfo(IBPort *p_port, const struct SMP_MlnxExtPortInfo &smpMlnxExtPortInfo);
    int addVSGeneralInfo(IBNode *p_node, const struct VendorSpec_GeneralInfo &vsGeneralInfo);
    int addVSSwitchNetworkInfo(IBNode *p_node, struct VS_SwitchNetworkInfo &switchNetworkInf);
    int addPMClassPortInfo(IBNode *p_node, IB_ClassPortInfo &pm_class_port_info);
    int addPMPortCounters(IBPort *p_port, struct PM_PortCounters &pmPortCounters);
    int addPMPortCountersExtended(IBPort *p_port, struct PM_PortCountersExtended &pmPortCountersExtended);
    int addPMPortExtSpeedsCounters(IBPort *p_port, struct PM_PortExtendedSpeedsCounters &pmPortExtendedSpeedsCounters);
    int addPMPortExtSpeedsRSFECCounters(IBPort *p_port,
    struct PM_PortExtendedSpeedsRSFECCounters &pmPortExtSpeedsRSFECCounters);
    int addVSPortLLRStatistics(IBPort *p_port, struct VendorSpec_PortLLRStatistics &vsPortLLRCounters);
    int addPMPortCalculatedCounters(IBPort *p_port, struct PM_PortCalcCounters &pmPortCalcCounters);
    int addPMPortSamplesControl(IBPort* p_port, struct PM_PortSamplesControl &pm_port_samples_control);
    int addSMPPKeyTable(IBPort *p_port, struct SMP_PKeyTable &smpPKeyTable, u_int32_t block_idx);
    int addSMPVLArbitrationTable(IBPort *p_port,
                                 struct SMP_VLArbitrationTable &smpVLArbitrationTable,
                                 u_int32_t block_idx);
    int addSMPGUIDInfo(IBPort *p_port, struct SMP_GUIDInfo &smpGUIDInfo, u_int32_t block_idx);
    int addSMPVPortGUIDInfo(IBVPort *p_vport, struct SMP_VPortGUIDInfo &smpVPortGUIDInfo,
                            u_int32_t block_idx);
    int addVSDiagnosticCountersPage0(IBPort *p_port, struct VS_DiagnosticData &vs_mlnx_cntrs_p0);
    int addVSDiagnosticCountersPage1(IBPort *p_port, struct VS_DiagnosticData &vs_mlnx_cntrs_p1);
    int addVSDiagnosticCountersPage255(IBPort *p_port, struct VS_DiagnosticData &vs_mlnx_cntrs_p255);
    int applySubCluster();
    int addSMPVirtualizationInfo(IBPort *p_port,
                                 struct SMP_VirtualizationInfo &virtual_info);
    int addSMPVPortState(IBPort *p_port, struct SMP_VPortState &smpVPortState, u_int32_t block_idx);
    int addSMPVPortInfo(IBVPort *p_vport, struct SMP_VPortInfo &smpVPortInfo);
    int addSMPVNodeInfo(IBVNode *p_vnode, struct SMP_VNodeInfo &smpVNodeInfo);
    int addSMPVNodeDescription(IBVNode *p_vnode,
                               struct SMP_NodeDesc &smpVNodeDescription);
    int addSMPVPortPKeyTable(IBVPort *p_vport, struct SMP_PKeyTable &smpVPortPKeyTable, u_int16_t block_idx);
    int addSMPRouterInfo(IBNode *p_node, struct SMP_RouterInfo &smpRouterInfo);
    int addSMPAdjSiteLocalSubnTbl(IBNode *p_node, struct SMP_AdjSiteLocalSubnTbl &smpRouterTbl, u_int8_t block_idx);
    int addSMPNextHopTbl(IBNode *p_node, struct SMP_NextHopTbl &smpRouterTbl, u_int32_t block_idx);
    int addSMPAdjSubnetRouterLIDInfoTbl(IBNode *p_node, struct SMP_AdjSubnetsRouterLIDInfoTable &subnetTbl, u_int8_t block_idx);
    int addSMPRouterLIDITbl(IBNode *p_node, struct SMP_RouterLIDTable &routerLidTbl, u_int8_t block_idx);
    int addARGroupToRouterLIDTbl(IBNode *p_node,
                                 struct SMP_ARGroupToRouterLIDTable &arGroupToRouterLidTbl,
                                 u_int8_t block_idx);
    int addSMPPortRecoveryPolicyConfig(IBPort *p_node,
                                 struct SMP_PortRecoveryPolicyConfig &portPolicyConfig,
                                 u_int8_t profile);

    // PortRcvErrorDetails and PortXmitDiscardDetails
    int addPMPortRcvErrorDetails(IBPort *p_port,
                                 struct PM_PortRcvErrorDetails &pmPortRcvErrorDetails);
    int addPMPortXmitDiscardDetails(IBPort *p_port,
                                    struct PM_PortXmitDiscardDetails &pmPortXmitDiscardDetails);

    // NVLink
    int addNVLFecMode(IBPort *p_port, struct SMP_NVLinkFecMode &data);
    int addNVLAnycastLIDInfo(IBNode* p_node, u_int32_t block_idx, struct SMP_AnycastLIDInfo &data);
    int addNVLHBFConfig(IBPort *p_port, struct SMP_NVLHBFConfig &data);
    int addContainAndDrainInfo(IBNode* p_node, u_int32_t block_idx, struct SMP_ContainAndDrainInfo &data);
    int addContainAndDrainPortState(IBNode* p_node, u_int32_t block_idx, struct SMP_ContainAndDrainPortState &data);

    struct SMP_NVLinkFecMode *              getNVLFecMode(u_int32_t port_index);
    struct SMP_AnycastLIDInfo *             getNVLAnycastLIDInfo(u_int32_t node_index, u_int32_t block_idx);
    struct SMP_NVLHBFConfig *               getNVLHBFConfig(u_int32_t port_index);
    struct SMP_ContainAndDrainInfo *        getContainAndDrainInfo(u_int32_t node_index, u_int32_t block_idx);
    struct SMP_ContainAndDrainPortState *   getContainAndDrainPortState(u_int32_t node_index, u_int32_t block_idx);

    // NVLink - Reduction
    int addNVLClassPortInfo(IBNode *p_node, struct IB_ClassPortInfo &data);
    int addNVLReductionInfo(IBNode* p_node, struct NVLReductionInfo &data);
    int addNVLReductionPortInfo(IBPort* p_port, struct NVLReductionPortInfo &data);
    int addNVLReductionRoundingMode(IBNode* p_node, struct NVLReductionRoundingMode &data);
    int addNVLReductionForwardingTable(IBNode* p_node, u_int32_t block_idx, struct NVLReductionForwardingTable &data);
    int addNVLPenaltyBoxConfig(IBNode* p_node, u_int32_t block_idx, struct NVLPenaltyBoxConfig &data);
    int addNVLReductionConfigureMLIDMonitors(IBPort* p_port, struct NVLReductionConfigureMLIDMonitors &data);
    int addNVLReductionCounters(IBPort* p_port, u_int32_t block_idx, struct NVLReductionCounters &data);

    struct IB_ClassPortInfo *           getNVLClassPortInfo(u_int32_t node_index);
    struct NVLReductionInfo *           getNVLReductionInfo(u_int32_t node_index);
    struct NVLReductionPortInfo *       getNVLReductionPortInfo(u_int32_t port_index);
    struct NVLReductionRoundingMode *   getNVLReductionRoundingMode(u_int32_t node_index);
    struct NVLReductionForwardingTable* getNVLReductionForwardingTable(u_int32_t node_index, u_int32_t block_idx);
    struct NVLPenaltyBoxConfig *        getNVLPenaltyBoxConfig(u_int32_t node_index, u_int32_t block_idx);
    struct NVLReductionCounters *       getNVLReductionCounters(u_int32_t port_index, u_int32_t block_idx);
    struct NVLReductionConfigureMLIDMonitors * getNVLReductionConfigureMLIDMonitors(u_int32_t port_index);

    // LFT Split
    int addSMP_LFTSplit(IBNode* p_node, struct SMP_LinearForwardingTableSplit &data);
    struct SMP_LinearForwardingTableSplit* getSMP_LFTSplit(u_int32_t node_index);

    port_rn_counters*           getRNCounters(u_int32_t node_index);
    port_routing_decision_counters* getRoutingDecisionCounters(u_int32_t port_index);
    adaptive_routing_info*      getARInfo(u_int32_t node_index);
    ib_private_lft_info*        getPLFTInfo(u_int32_t node_index);
    hbf_config*                 getHBFConfig(u_int32_t node_index);

    // pFRN (N2N, Class C)
    SMP_pFRNConfig*             getpFRNConfig(u_int32_t node_index);
    IB_ClassPortInfo*           getN2NClassPortInfo(u_int32_t node_index);
    neighbor_record*            getNeighborRecord(u_int32_t node_index, u_int32_t record_idx);
    Class_C_KeyInfo*            getN2NKeyInfo(u_int32_t node_index);

    SMP_NodeInfo*               getSMPNodeInfo(u_int32_t node_index);
    SMP_SwitchInfo*             getSMPSwitchInfo(u_int32_t node_index);
    SMP_PortInfo*               getSMPPortInfo(u_int32_t port_index);
    SMP_PortInfoExtended *      getSMPPortInfoExtended(u_int32_t port_index);
    ib_extended_node_info*      getSMPExtNodeInfo(u_int32_t node_index);
    SMP_MlnxExtPortInfo*        getSMPMlnxExtPortInfo(u_int32_t port_index);
    VendorSpec_GeneralInfo*     getVSGeneralInfo(u_int32_t node_index);
    VS_SwitchNetworkInfo*       getVSSwitchNetworkInfo(u_int32_t node_index);
    IB_ClassPortInfo*           getPMClassPortInfo(u_int32_t node_index);
    PM_PortCounters*            getPMPortCounters(u_int32_t port_index);
    PM_PortCountersExtended*    getPMPortCountersExtended(u_int32_t port_index);
    PM_PortExtendedSpeedsCounters * getPMPortExtSpeedsCounters(u_int32_t port_index);
    PM_PortExtendedSpeedsRSFECCounters *
        getPMPortExtSpeedsRSFECCounters(u_int32_t port_index);
    VendorSpec_PortLLRStatistics * getVSPortLLRStatistics(u_int32_t port_index);
    // PortRcvErrorDetails and PortXmitDiscardDetails
    PM_PortRcvErrorDetails* getPMPortRcvErrorDetails(u_int32_t port_index);
    PM_PortXmitDiscardDetails* getPMPortXmitDiscardDetails(u_int32_t port_index);
    PM_PortCalcCounters *       getPMPortCalcCounters(u_int32_t port_index);
    struct PM_PortSamplesControl* getPMPortSamplesControl(u_int32_t port_index);
    SMP_PKeyTable *             getSMPPKeyTable(u_int32_t port_index, u_int32_t block_idx);
    SMP_VLArbitrationTable*     getSMPVLArbitrationTable(u_int32_t port_index, u_int32_t block_idx);
    SMP_GUIDInfo *              getSMPGUIDInfo(u_int32_t port_index, u_int32_t block_idx);
    struct SMP_VPortGUIDInfo*   getSMPVPortGUIDInfo(u_int32_t vport_index, u_int32_t block_idx);
    VS_DiagnosticData *         getVSDiagnosticCountersPage0(u_int32_t port_index);
    VS_DiagnosticData *         getVSDiagnosticCountersPage1(u_int32_t port_index);
    VS_DiagnosticData *         getVSDiagnosticCountersPage255(u_int32_t port_index);
    SMP_VirtualizationInfo *    getSMPVirtualizationInfo(u_int32_t port_index);
    SMP_VPortState * getSMPVPortState(u_int32_t port_index, u_int32_t block_idx);
    SMP_VPortInfo * getSMPVPortInfo(u_int32_t port_index);
    SMP_VNodeInfo * getSMPVNodeInfo(u_int32_t vnode_index);
    SMP_RouterInfo *            getSMPRouterInfo(u_int32_t node_index);
    SMP_AdjSiteLocalSubnTbl *   getSMPAdjSiteLocalSubnTbl(u_int32_t node_index,
                                                          u_int8_t block_idx);
    SMP_NextHopTbl *            getSMPNextHopTbl(u_int32_t node_index,
                                                 u_int32_t block_idx);
    SMP_ARGroupToRouterLIDTable * getSMPARGroupToRouterLIDTbl(u_int32_t node_index, u_int8_t block_idx);
    SMP_AdjSubnetsRouterLIDInfoTable * getSMPAdjSubnteRouterLIDInfoTbl(u_int32_t node_index, u_int8_t block_idx);
    struct SMP_RouterLIDTable * getSMPRouterLIDTbl(u_int32_t node_index, u_int8_t block_idx);
    struct SMP_NodeDesc * getSMPVNodeDescription(u_int32_t vnode_index);
    SMP_PKeyTable * getSMPVPortPKeyTable(u_int32_t vport_index, u_int32_t block_idx);
    SMP_PortRecoveryPolicyConfig * getSMPPortRecoveryPolicyConfig(u_int32_t port_index, u_int8_t profile);


    int addSMPQosConfigSL(IBPort *p_port, SMP_QosConfigSL &qos_config_sl);
    struct SMP_QosConfigSL* getSMPQosConfigSL(u_int32_t port_index);
    int addSMPVPortQosConfigSL(IBVPort *p_vport, SMP_QosConfigSL &qos_config_sl);
    struct SMP_QosConfigSL* getSMPVPortQosConfigSL(u_int32_t port_index);

    int addSMPQosConfigVL(IBPort *p_port, SMP_QosConfigVL &qos_config_vl);
    struct SMP_QosConfigVL* getSMPQosConfigVL(u_int32_t port_index);

    int addSMPTempSensing(IBNode *p_node, SMP_TempSensing &p_temp_sense);
    struct SMP_TempSensing * getSMPTempSensing(u_int32_t node_index);

    uint8_t getSMPVPortStateVectorSize(u_int32_t port_index);

    // CC
    int addCCEnhancedCongestionInfo(IBNode *p_node, CC_EnhancedCongestionInfo &cc_enhanced_info);
    struct CC_EnhancedCongestionInfo* getCCEnhancedCongestionInfo(u_int32_t node_index);

    // CC switch
    int addCCSwitchGeneralSettings(IBNode *p_node, CC_CongestionSwitchGeneralSettings &cc_switch_general_settings);
    struct CC_CongestionSwitchGeneralSettings* getCCSwitchGeneralSettings(u_int32_t node_index);

    int addCCPortProfileSettings(IBPort *p_port, u_int8_t vl, CC_CongestionPortProfileSettings &cc_port_profile_settings);
    struct CC_CongestionPortProfileSettings* getCCPortProfileSettings(u_int32_t port_index, u_int8_t vl);

    int addCCSLMappingSettings(IBPort *p_port, CC_CongestionSLMappingSettings &cc_sl_mapping_settings);
    struct CC_CongestionSLMappingSettings* getCCSLMappingSettings(u_int32_t port_index);

    // CC HCA
    int addCCHCAGeneralSettings(IBPort *p_port, CC_CongestionHCAGeneralSettings &cc_hca_general_settings);
    struct CC_CongestionHCAGeneralSettings* getCCHCAGeneralSettings(u_int32_t port_index);

    int addCCHCARPParameters(IBPort *p_port, CC_CongestionHCARPParameters &cc_hca_rp_parameters);
    struct CC_CongestionHCARPParameters* getCCHCARPParameters(u_int32_t port_index);

    int addCCHCANPParameters(IBPort *p_port, CC_CongestionHCANPParameters &cc_hca_np_parameters);
    struct CC_CongestionHCANPParameters* getCCHCANPParameters(u_int32_t port_index);

    int addCCHCAStatisticsQuery(IBPort *p_port, CC_CongestionHCAStatisticsQuery &cc_hca_statistics_query);
    struct CC_CongestionHCAStatisticsQuery* getCCHCAStatisticsQuery(u_int32_t port_index);

    // CC Algo HCA
    int addCC_HCA_AlgoConfigSup(IBPort *p_port, CC_CongestionHCAAlgoConfig &cc_hca_algo_config);
    struct CC_CongestionHCAAlgoConfig* getCC_HCA_AlgoConfigSup(u_int32_t port_index);

    int addCC_HCA_AlgoConfig(IBPort *p_port, CC_CongestionHCAAlgoConfig &cc_hca_algo_config, u_int32_t algo_slot);
    struct CC_CongestionHCAAlgoConfig* getCC_HCA_AlgoConfig(u_int32_t port_index, u_int32_t algo_slot);

    int addCC_HCA_AlgoConfigParams(IBPort *p_port, CC_CongestionHCAAlgoConfigParams &cc_hca_algo_config_params, u_int32_t algo_slot);
    struct CC_CongestionHCAAlgoConfigParams* getCC_HCA_AlgoConfigParams(u_int32_t port_index, u_int32_t algo_slot);

    int addCC_HCA_AlgoCounters(IBPort *p_port, CC_CongestionHCAAlgoCounters &cc_hca_algo_counters, u_int32_t algo_slot);
    struct CC_CongestionHCAAlgoCounters* getCC_HCA_AlgoCounters(u_int32_t port_index, u_int32_t algo_slot);

    // Fast Recovery
    int addCreditWatchdogTimeoutCounters(IBPort *p_port,
                                         struct VS_CreditWatchdogTimeoutCounters &credit_wd_counters);
    VS_CreditWatchdogTimeoutCounters* getCreditWatchdogTimeoutCounters(u_int32_t port_index);

    int addPerformanceHistogramBufferData(IBPort *p_port, struct VS_PerformanceHistogramBufferData &perf_hist_buff_data, u_int8_t vl, u_int8_t direction);
    VS_PerformanceHistogramBufferData* getPerformanceHistogramBufferData(u_int32_t port_index, u_int8_t vl, u_int8_t direction);

    int addPerformanceHistogramPortsControl(IBPort *p_port, u_int8_t hist_id, struct VS_PerformanceHistogramPortsControl &performance_histogram_ports_control);
    VS_PerformanceHistogramPortsControl* getPerformanceHistogramPortsControl(u_int32_t port_index, u_int8_t hist_id);

    int addFastRecoveryCounters(IBPort *p_port, struct VS_FastRecoveryCounters &fast_recovery_cntrs, u_int32_t trigger);
    VS_FastRecoveryCounters* getFastRecoveryCounters(u_int32_t port_index, u_int32_t trigger);

    int addVSPortGeneralCounters(IBPort *p_port, struct VS_PortGeneralCounters &vs_port_general_counters);
    VS_PortGeneralCounters* getVSPortGeneralCounters(u_int32_t node_index);

    int addPortRecoveryPolicyCounters(IBPort *p_port, struct VS_PortRecoveryPolicyCounters &port_recovery_policy_counters);
    VS_PortRecoveryPolicyCounters* getPortRecoveryPolicyCounters(u_int32_t port_index);

    int addPerformanceHistogramInfo(IBNode *p_node, struct VS_PerformanceHistogramInfo &perf_his_info);
    VS_PerformanceHistogramInfo* getPerformanceHistogramInfo(u_int32_t node_index);

    int addPerformanceHistogramBufferControl(IBPort *p_port, struct VS_PerformanceHistogramBufferControl &perf_hist_buff_control, u_int8_t vl, u_int8_t direction);
    VS_PerformanceHistogramBufferControl* getPerformanceHistogramBufferControl(u_int32_t port_index, u_int8_t vl, u_int8_t direction);

    int addPerformanceHistogramPortsData(IBPort *p_port, struct VS_PerformanceHistogramPortsData &vs_performance_histogram_ports_data, u_int8_t hist_id);
    VS_PerformanceHistogramPortsData* getPerformanceHistogramPortsData(u_int32_t port_index, u_int8_t hist_id);

    int addProfilesConfig(IBNode *p_node, struct SMP_ProfilesConfig &profiles_config, u_int32_t block,
                          u_int8_t profile_feature);
    SMP_ProfilesConfig* getProfilesConfig(u_int32_t node_index, u_int32_t block, u_int8_t profile_feature);

    int addCreditWatchdogConfig(IBNode *p_node, struct SMP_CreditWatchdogConfig &credit_wd_config, u_int32_t profile);
    SMP_CreditWatchdogConfig* getCreditWatchdogConfig(u_int32_t node_index, u_int32_t profile);

    int addBERConfig(IBNode *p_node, struct SMP_BERConfig &ber_config, u_int32_t profile, u_int32_t ber);
    SMP_BERConfig* getBERConfig(u_int32_t node_index, u_int32_t profile, u_int32_t ber);

    int addSMP_ExtendedSwitchInfo(IBNode *p_node, const struct SMP_ExtendedSwitchInfo &smp__extended_switch_info);
    SMP_ExtendedSwitchInfo* getSMP_ExtendedSwitchInfo(u_int32_t node_index);

    int addSMPChassisInfo(IBNode *p_node, const struct SMP_ChassisInfo& smp_chassis_info);
    SMP_ChassisInfo* getSMPChassisInfo(u_int32_t node_index);

    int addPMPortSamplesResult(IBPort *p_port, struct PM_PortSamplesResult &pm_port_samples_result);
    PM_PortSamplesResult* getPMPortSamplesResult(u_int32_t node_index);

    inline list_p_sm_info_obj& getSMPSMInfoListRef() { return this->sm_info_obj_list; }
    inline vector_p_smp_node_info& getSMPNodeInfoVectorRef() { return this->smp_node_info_vector; }
    inline vector_p_smp_switch_info& getSMPSwitchInfoVectorRef() { return this->smp_switch_info_vector; }
    inline vector_p_smp_port_info& getSMPPortInfoVectorRef() { return this->smp_port_info_vector; }
    inline vector_p_vs_general_info& getVSGeneralInfoVectorRef() { return this->vs_general_info_vector; }
    inline vector_p_class_port_info& getPMClassPortInfoVectorRef() { return this->pm_class_port_info_vector; }
    inline vector_p_pm_info_obj& getPMInfoObjVectorRef() { return this->pm_info_obj_vector; }

    void CleanPMInfoObjVector(vector_p_pm_info_obj& curr_pm_obj_info_vector);

    IBLinkSpeed getCorrectSpeed(const SMP_PortInfo& curr_port_info,
                                u_int32_t cap_mask,
                                u_int16_t cap_mask2);

    uint8_t getPortMTUCap(const IBPort* p_port);
    uint8_t getPortVLCap(const IBPort* p_port);
    u_int32_t getPortQoSRateLimitBySL(const IBPort *p_port, u_int32_t sl);
    u_int16_t getPortQoSBandwidthBySL(const IBPort *p_port, u_int32_t sl);
};

typedef SMP_PKeyTable* (IBDMExtendedInfo::*get_pkey_table_func_t)(u_int32_t, u_int32_t);

#endif          /* IBDIAG_IBDM_EXTENDED_INFO_H */

