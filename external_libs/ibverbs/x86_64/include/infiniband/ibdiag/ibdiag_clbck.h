/*
 * Copyright (c) 2004-2020 Mellanox Technologies LTD. All rights reserved
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
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


#ifndef IBDIAG__CLBCK_H
#define IBDIAG__CLBCK_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <map>
using namespace std;

#include "infiniband/ibdiag/ibdiag.h"

//#include <infiniband/ibdm/Fabric.h>
//#include <infiniband/ibis/ibis.h>

//#include "ibdiag_ibdm_extended_info.h"
//#include "ibdiag_fabric_errs.h"
#include "ibdiag_types.h"
#include "sharp_mngr.h"

typedef enum {RETRIEVE_STAGE_SEND = 0,
              RETRIEVE_STAGE_REC_WAIT = 1,
              RETRIEVE_STAGE_REC_DONE = 2
} RetrieveStage;


class IBDiagClbck
{
    list_p_fabric_general_err* m_pErrors;
    IBDiag *                   m_pIBDiag;
    IBDMExtendedInfo*          m_pFabricExtendedInfo;
    int                        m_ErrorState;
    string                     m_LastError;
    ofstream *                 m_p_sout;
    CapabilityModule *         m_p_capability_module;
    SharpMngr *                m_p_sharp_mngr;
    u_int32_t                  m_num_warnings;
    u_int32_t                  m_num_errors;

    void getPortsList(u_int64_t pgSubBlockElement,
                                   phys_port_t portOffset,
                                   list_phys_ports &portsList);
    void getPortsList(ib_portgroup_block_element &pgBlockElement,
                                   list_phys_ports &portsList);

    void SetLastError(const char *fmt, ...);

    void ParsePhysicalHierarchyInfo(SMP_HierarchyInfo *p_hierarchy_info, IBNode *p_node);

    void ParsePhysicalHierarchyInfoTemplate6(SMP_HierarchyInfo *p_hierarchy_info, IBNode *p_node);

    void ParsePortHierarchyInfo(SMP_HierarchyInfo *p_hierarchy_info, IBPort *p_port);

    void ParseXDRPortHierarchyInfo(SMP_HierarchyInfo *p_hierarchy_info, IBPort *p_port,
                          uint64_t template_guid);

    template<class T>
    bool VerifyObject(const T *p_object, int line)
    {
        if (p_object)
            return true;

        if (m_pErrors)
            m_pErrors->push_back(new NullPtrError(line));

        return false;
    }

public:
    IBDiagClbck():
        m_pErrors(NULL), m_pIBDiag(NULL),
        m_pFabricExtendedInfo(NULL), m_ErrorState(IBDIAG_SUCCESS_CODE),
        m_p_sout(NULL), m_p_capability_module(NULL), m_p_sharp_mngr(NULL), m_num_warnings(0), m_num_errors(0){}

    int GetState() {return m_ErrorState;}
    void ResetState()
    {
        m_LastError.clear();
        m_ErrorState = IBDIAG_SUCCESS_CODE;
        m_num_warnings = 0;
        m_num_errors = 0;
    }

    u_int32_t GetNumWarnings() {return m_num_warnings;}
    u_int32_t GetNumErrors() {return m_num_errors;}

    const char* GetLastError();

    void Set(IBDiag * p_IBDiag, IBDMExtendedInfo* p_fabric_extended_info,
             list_p_fabric_general_err* p_errors, ofstream * p_sout = NULL,
             CapabilityModule *p_capability_module = NULL)
    {
        m_pIBDiag = p_IBDiag;
        m_pFabricExtendedInfo = p_fabric_extended_info;
        m_pErrors = p_errors;
        m_ErrorState = IBDIAG_SUCCESS_CODE;
        m_LastError.clear();
        m_p_sout = p_sout;
        m_p_capability_module = p_capability_module;
        m_num_warnings = 0;
        m_num_errors = 0;
    }

    void SetOutStream(ofstream * p_sout) {m_p_sout = p_sout;}
    void SetSharpMngr(SharpMngr * p_sharp_mngr) {m_p_sharp_mngr = p_sharp_mngr;}

    void Reset()
    {
        m_pIBDiag = NULL;
        m_pErrors = NULL;
        m_pFabricExtendedInfo = NULL;
        m_p_sout = NULL;
    }

    // PM
    void PMPortCountersGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortCountersClearClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortCountersExtendedGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortCountersExtendedClearClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortExtendedSpeedsGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortExtendedSpeedsClearClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void PMPortExtendedSpeedsRSFECGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);
    void PMPortExtendedSpeedsRSFECClearClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);
    void PMClassPortInfoClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    // NVLink
    void NVLFecModeGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLAnycastLIDInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLHBFConfigGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLContainAndDrainInfoGetClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLContainAndDrainPortStateGetClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    // Reduction
    void NVLClassPortInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionPortInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionRoundingModeGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionForwardingTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLPenaltyBoxConfigClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionConfigureMLIDMonitorsGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void NVLReductionCountersGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    // VS
    void VSPortLLRStatisticsGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSPortLLRStatisticsClearClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSGeneralInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSSwitchNetworkInfoClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSDiagnosticCountersClearClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSDiagnosticCountersPage0GetClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSDiagnosticCountersPage1GetClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void VSDiagnosticCountersPage255GetClbck(
                        const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    // SMP
    void SMPGUIDInfoTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPPkeyTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPSMInfoMadGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPVLArbitrationGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPLinearForwardingTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPMulticastForwardingTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPSLToVLMappingTableGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPVSGeneralInfoFwInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPVSGeneralInfoCapabilityMaskGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPVSExtendedPortInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPExtendedNodeInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPPLFTInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPPortSLToPrivateLFTMapGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPPrivateLFTTopGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPHBFConfigGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);


    // pFRN (N2N, Class C)
    void SMPpFRNConfigGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void N2NClassPortInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void NeighborsInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void N2NKeyInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);


    void SMPARInfoGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPARInfoGetForSimulatorClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPWeightsHBFConfigGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPARGroupTableGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPARLinearForwardingTableGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPRNSubGroupDirectionTableGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPRNGenStringTableGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPRNGenBySubGroupPriorityGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPRNRcvStringGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPRNXmitPortMaskGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void VSPortRoutingDecisionCountersGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void VSPortRoutingDecisionCountersClearClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void VSPortRNCountersClearClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void VSPortRNCountersGetClbck(const clbck_data_t &clbck_data,
                       int rec_status,
                       void *p_attribute_data);

    void SMPHierarchyInfoGetClbck(const clbck_data_t &clbck_data,
                                  int rec_status,
                                  void *p_attribute_data);

    void SMPPortInfoExtendedGetClbck(const clbck_data_t &clbck_data,
                                     int rec_status,
                                     void *p_attribute_data);

    void SMPSwitchInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void SMPPortInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

   void SMPNodeDescGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

   void SMPNodeInfoGetClbck(const clbck_data_t &clbck_data,
                        int rec_status,
                        void *p_attribute_data);

    void IBDiagSMPVirtualizationInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVPortStateGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void IBDiagSMPVNodeInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVPortInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVPortGUIDInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVNodeDescriptionGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVPortPKeyGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPQoSConfigVLGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPQoSConfigSLGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPVPortQoSConfigSLGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void IBDiagSMPTempSensingGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SMPRouterInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void SMPAdjRouterTableGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SMPNextHopRouterTableGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SMPAdjRouterLIDInfoTableGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SMPARGroupToRouterLidTableGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SMPRouterLIDTableGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrClassPortInfoClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrTreeConfigClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrQPCConfigClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrPerfCountersClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrHBAPerfCountersClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrResetPerfCountersClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrANInfoClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void SharpMngrANActiveJobsClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void GSIPerSLVLGetClbck(const clbck_data_t &clbck_data,
                            int rec_status,
                            void *p_attribute_data);

    void PMPortSamplesControlGetClbck(const clbck_data_t &clbck_data,
                                      int rec_status,
                                      void *p_attribute_data);

    void PMPortRcvErrorDetailsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void PMPortRcvErrorDetailsClearClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void PMPortXmitDiscardDetailsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void PMPortXmitDiscardDetailsClearClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    /*** CC ***/
    void CCEnhancedCongestionInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    // CC switch
    void CCSwitchGeneralSettingsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CCPortProfileSettingsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    void CCSLMappingSettingsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);
    // CC HCA
    void CCHCAGeneralSettingsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CCHCARPParametersGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CCHCANPParametersGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CCHCAStatisticsQueryGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CCPerSLVLGetClbck(const clbck_data_t &clbck_data,
                           int rec_status,
                           void *p_attribute_data);
    // CC Algo
    void CC_HCA_AlgoConfigSupGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CC_HCA_AlgoConfigGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CC_HCA_AlgoConfigParamsGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void CC_HCA_AlgoCountersGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    // Fast Recovery
    void VSCreditWatchdogTimeoutCountersGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void VSCreditWatchdogTimeoutCountersClearClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void VSFastRecoveryCountersGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void VSFastRecoveryCountersClearClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void VSPortGeneralCountersClbck(
        const clbck_data_t &clbck_data,
        int rec_status,
        void *p_attribute_data);

    void VSPortRecoveryPolicyCountersClbck(
        const clbck_data_t &clbck_data,
        int rec_status,
        void *p_attribute_data);

    void VSPerformanceHistogramInfoGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void VSPerformanceHistogramBufferControlClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data
    );

    void VS_PerformanceHistogramBufferDataClbck(
        const clbck_data_t &clbck_data,
        int rec_status,
        void *p_attribute_data);

    void VSPerformanceHistogramPortsControlClbck(
        const clbck_data_t &clbck_data,
        int rec_status,
        void *p_attribute_data);

    void VSPerformanceHistogramPortsDataClbck(
        const clbck_data_t &clbck_data,
        int rec_status,
        void *p_attribute_data);

    void SMP_ProfilesConfigGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void SMP_CreditWatchdogConfigGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    void SMP_BERConfigGetClbck(
            const clbck_data_t &clbck_data,
            int rec_status,
            void *p_attribute_data);

    // Entry Plane Filter
    void SMPEntryPlaneFilterGetClbck(const clbck_data_t &clbck_data,
                                     int rec_status,
                                     void *p_attribute_data);

    // Rail Filter
    void SMPRailFilterGetClbck(const clbck_data_t &clbck_data,
                               int rec_status,
                               void *p_attribute_data);

    // LFT Split
    void SMP_LFTSplitGetClbck(const clbck_data_t &clbck_data,
                              int rec_status,
                              void *p_attribute_data);

    // End Port Plane Filter
    void SMPEndPortPlaneFilterGetClbck(const clbck_data_t &clbck_data,
                                       int rec_status,
                                       void *p_attribute_data);

    //SMP Extended Switch Info
    void SMP_ExtendedSwitchInfoClbck(const clbck_data_t &clbck_data,
                                     int rec_status,
                                     void *p_attribute_data);

    // SMP Chassis Info
    void SMPChassisInfoClbck(const clbck_data_t &clbck_data,
                             int rec_status,
                             void *p_attribute_data);

    void SMPPortRecoveryPolicyConfigClbck(const clbck_data_t &clbck_data,
                                          int rec_status,
                                          void *p_attribute_data);

    //PM Port Samples Result
    void PMPortSamplesResultClbck(const clbck_data_t &clbck_data,
                                  int rec_status,
                                  void *p_attribute_data);

};

#endif          /* IBDIAG__CLBCK_H */
