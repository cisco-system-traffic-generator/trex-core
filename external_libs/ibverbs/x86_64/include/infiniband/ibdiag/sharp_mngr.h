/*
 * Copyright (c) 2004-2020 Mellanox Technologies LTD. All rights reserved.
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


#ifndef SHARP_MNGR_H
#define SHARP_MNGR_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <map>
#include <unordered_map>
using namespace std;

#include <infiniband/ibdm/Fabric.h>
#include <ibis/ibis.h>

#include <infiniband/ibdiag/ibdiag_ibdm_extended_info.h>
#include <infiniband/ibdiag/ibdiag_fabric_errs.h>
#include <infiniband/ibdiag/ibdiag_types.h>

#define DEFAULT_AM_KEY DEFAULT_KEY
#define DEFAULT_AM_CLASS_VERSION  1
#define MAX_CHILD_IDX_IN_TREE_CONFIG_MAD    44
#define ALL_COUNTERS_SELECT 0xffff
#define AN_ACTIVE_JOBS_DWORDS_NUM 48

// Bit mask for relevant counters for old devices (first 9 CLU counters for backward compatibility)
#define NONE_MODE_PERF_CNTR_MASK 0x000001ff

typedef enum {
    QP_STATE_DISABLED = 0,
    QP_STATE_ACTIVE,
    QP_STATE_ERROR
} qp_state;

enum tree_mode {
    TREE_MODE_LLT = 0,
    TREE_MODE_SAT
};

enum counters_mode {
    CLU = 0,
    HBA,
    AGGREGATED,
    NONE_MODE
};

#define CLU_MODE_STR            "CLU"
#define HBA_MODE_STR            "HBA"
#define AGGREGATED_MODE_STR     "Aggregated"
#define NONE_MODE_STR           "None-Mode"

// Used as initial value of class version for SHARP aggregation node (INVALID by default)
// Version can be provided by user or auto detected by ANInfo
// in case if ANInfo will failure node will be filtered because default value is INVALID
#define CLASS_VERSION_UNKNOWN   0xFF

static inline const char* sharp_counters_mode2char(const counters_mode mode)
{
    switch (mode) {
        case CLU:             return (CLU_MODE_STR);
        case HBA:             return (HBA_MODE_STR);
        case AGGREGATED:      return (AGGREGATED_MODE_STR);
        case NONE_MODE:
        default:              return (NONE_MODE_STR);
    }
};

static inline counters_mode char2sharp_counters_mode(const char *mode)
{
    if (!strcmp(mode,CLU_MODE_STR))              return CLU;
    if (!strcmp(mode,HBA_MODE_STR))              return HBA;
    if (!strcmp(mode,AGGREGATED_MODE_STR))       return AGGREGATED;

    return(NONE_MODE);
};

enum sharp_pm_counters_bit {
    PACKET_SENT_BIT = 0,
    ACK_PACKET_SENT_BIT,
    RETRY_PACKET_SENT_BIT,
    RNR_EVENT_BIT,
    TIMEOUT_EVENT_BIT,
    OOS_NACK_RCV_BIT,
    RNR_NACK_RCV_BIT,
    PACKET_DISCARD_TRANSPORT_BIT,
    PACKET_DISCARD_SHARP_BIT,
    AETH_SYNDROME_ACK_PACKET_BIT,
    HBA_SHARP_LOOKUP_BIT,
    HBA_RECEIVED_PKTS_BIT,
    HBA_RECEIVED_BYTES_BIT,
    HBA_SENT_ACK_PACKETS_BIT,
    RCDS_SENT_PACKETS_BIT,
    HBA_SENT_ACK_BYTES_BIT,
    RCDS_SENT_BYTES_BIT,
    HBA_MULTI_PACKET_MESSAGE_DROPPED_PKTS_BIT,
    HBA_MULTI_PACKET_MESSAGE_DROPPED_BYTES_BIT
};

struct AggNodePerformanceCounters {
    AM_PerformanceCounters *perf_cntr;
    counters_mode mode;

    AggNodePerformanceCounters(): perf_cntr(NULL), mode(NONE_MODE) { }
    ~AggNodePerformanceCounters() { delete this->perf_cntr; }
};

typedef list < IBNode * > vector_ibnodes;
typedef list < class SharpAggNode * > list_sharp_an;
typedef vector < class SharpTreeEdge * > vector_p_sharp_tree_edge;
typedef vector < class SharpTreeNode * > vector_p_sharp_tree_node;
typedef map < lid_t, class SharpAggNode * > map_lid_to_sharpagg_node;
typedef vector < class SharpTree * > vector_sharp_root_node;
typedef map < u_int32_t, struct AM_QPCConfig * > map_qpn_to_qpc_config;
typedef map < u_int32_t, u_int16_t > map_qpn_to_treeid;
typedef map < lid_t, struct IB_ClassPortInfo* > map_lid_to_class_ports_info;
typedef map < phys_port_t, struct AM_PerformanceCounters > map_port_number_to_hba_pm_counters;
typedef unordered_map<lid_t, lid_t> map_lid_to_lid;

class IBDiag;

class SharpMngr {
private:
    u_int16_t                       m_fabric_max_trees_idx;
    IBDiag *                        m_ibdiag;
    u_int8_t                        version;

    map_lid_to_sharpagg_node        m_lid_to_sharp_agg_node;
    map_lid_to_lid                  m_logical_lid_to_base_lid; // map AN logical lid to AN base lid, only if logical lids exist
    list_sharp_an                   m_sharp_an;
    vector_ibnodes                  m_sharp_supported_nodes; //all IBNodes that supports Sharp
    map_lid_to_class_ports_info     m_lid_to_class_port_info;

    int DiscoverSharpAggNodes(list_p_fabric_general_err& sharp_discovery_errors);
    int BuildTreeConfig(list_p_fabric_general_err& sharp_discovery_errors);
    int BuildQPCConfig(list_p_fabric_general_err& sharp_discovery_errors);
    int BuildANInfo(list_p_fabric_general_err& sharp_discovery_errors);

    void RemoveANsNotInVersion();

    int BuildANActiveJobs(list_p_fabric_general_err& sharp_discovery_errors);

    void ReportRemoteNodeDoesntExistErr(list_p_fabric_general_err &sharp_discovery_errors, int &rc,
                                        SharpAggNode *p_sharp_agg_node);

    void ReportQPCPortNotZeroErr(list_p_fabric_general_err &sharp_discovery_errors, int &rc,
                                 SharpAggNode *p_sharp_agg_node, u_int8_t qpc_port,
                                 u_int8_t an_port_sel_supported,
                                 SharpAggNode *p_remote_sharp_agg_node, u_int8_t remote_qpc_port,
                                 u_int8_t remote_an_port_sel_supported);

    void CheckQPCPortsAreValid(list_p_fabric_general_err &sharp_discovery_errors, int &rc,
                               SharpAggNode *p_sharp_agg_node, SharpTreeEdge * p_sharp_tree_edge);

public:
    SharpMngr(IBDiag * ibdiag, u_int8_t ver);
    ~SharpMngr();

    inline const list_sharp_an &GetSharpANList() const { return m_sharp_an; };

    inline void UpdateFabricMaxTreeIdx(u_int16_t tree_index) {
        if (tree_index > this->m_fabric_max_trees_idx)
            m_fabric_max_trees_idx = tree_index;
    }
    void AddSharpSupportedNodes(IBNode * p_ibnode) {
        m_sharp_supported_nodes.push_back(p_ibnode);
    }

    void AddClassPortInfo(lid_t lid, IB_ClassPortInfo *p_class_pi) {
        m_lid_to_class_port_info[lid] = new IB_ClassPortInfo(*p_class_pi);
    }

    int BuildSharpConfiguration(
        list_p_fabric_general_err &sharp_discovery_errors, bool check_trees);

    int VerifyTrapsLids(list_p_fabric_general_err& sharp_discovery_errors);
    int VerifyVersions(list_p_fabric_general_err& sharp_discovery_errors);

    int BuildPerformanceCounters(list_p_fabric_general_err& sharp_discovery_errors,
                                   bool per_port);
    int ResetPerformanceCounters(list_p_fabric_general_err &sharp_discovery_errors);

    int ConnectTreeEdges(list_p_fabric_general_err &sharp_discovery_errors);
    int CheckSharpTrees(list_p_fabric_general_err &sharp_discovery_errors);

    int WriteSharpFile(const string &file_name);

    int DumpSharpANInfoToCSV(CSVOut &sout);
    int WriteSharpANInfoFile(const string &file_name);

    int DumpSharpPMCountersToCSV(CSVOut &csv_out);
    int DumpSharpPMHBAPortCountersToCSV(CSVOut &csv_out);
    void SharpMngrDumpAllTrees(ofstream &sout);

    int SharpMngrDumpAllQPs(ofstream &sout);

    void DumpQPC(ofstream &sout, struct AM_QPCConfig *qpconfig);

    void UpdateMaxRadixOnRoots();

    SharpAggNode* GetAggNodeByLid(lid_t lid) const;
};

class SharpAggNode {
private:
    IBPort *                                m_port;
    AM_ANInfo                               m_an_info;
    AggNodePerformanceCounters              m_agg_node_perf_cntr;
    vector_p_sharp_tree_node                m_trees;
    u_int8_t                                class_version;
    AM_ANActiveJobs                         m_an_act_jobs;
    map_port_number_to_hba_pm_counters      m_map_hba_pm_counters;
    vector_sharp_root_node                  m_roots;

public:
    SharpAggNode(IBPort * port);
    ~SharpAggNode();

    inline u_int16_t GetMaxNumQps() const { return m_an_info.max_num_qps; }
    inline u_int16_t GetTreesSize() { return (u_int16_t)this->m_trees.size(); };
    inline IBPort * GetIBPort() const { return m_port; }

    inline u_int16_t GetRootsSize() { return (u_int16_t)this->m_roots.size(); };
    inline const vector_sharp_root_node& GetSharpRootNodeVec() const { return m_roots; };


    inline void SetPerfCounters(AM_PerformanceCounters  *perf_cntr, counters_mode mode) {
        if (!this->m_agg_node_perf_cntr.perf_cntr)
            this->m_agg_node_perf_cntr.perf_cntr = new struct AM_PerformanceCounters;
        *this->m_agg_node_perf_cntr.perf_cntr = *perf_cntr;
        this->m_agg_node_perf_cntr.mode = mode;
    }
    inline const AggNodePerformanceCounters& GetPerfCounters() const {
        return m_agg_node_perf_cntr;
    }

    inline void SetHBAPerfCounters(AM_PerformanceCounters* p_hba_perf_cntr, IBPort *p_port) {
        memcpy(&m_map_hba_pm_counters[p_port->num], p_hba_perf_cntr,
               sizeof(AM_PerformanceCounters));
    }
    inline const map_port_number_to_hba_pm_counters& GetHBAPerfCounters() const {
        return this->m_map_hba_pm_counters;
    }
    bool IsPerfCounterSupported(counters_mode mode, sharp_pm_counters_bit bit);

    void SetANInfo(AM_ANInfo *an_info);
    inline const AM_ANInfo & GetANInfo() const { return m_an_info; }
    void SetANActiveJobs(AM_ANActiveJobs *an_jobs) { m_an_act_jobs = *an_jobs; }
    inline const AM_ANActiveJobs& GetANActiveJobs() const { return m_an_act_jobs; }

    int AddSharpTreeNode(SharpTreeNode *p_sharp_tree_node,
                         u_int16_t tree_index);
    SharpTreeNode * GetSharpTreeNode(u_int16_t tree_index);

    u_int8_t getClassVersion() { return class_version; }

    int AddTreeRoot(u_int16_t tree_id, SharpTreeNode *p_sharp_tree_node);
    SharpTree* GetTreeRoot(u_int16_t tree_id);
    bool OwnsLid(lid_t lid) const;
};

class SharpTree {
private:
    SharpTreeNode *         m_root;
    uint32_t                m_max_radix;

public:
    SharpTree(SharpTreeNode *root);
    ~SharpTree(){}

    inline uint32_t GetMaxRadix() const { return m_max_radix; }
    inline void SetMaxRadix(uint32_t max_radix) { m_max_radix = max_radix; }
    inline SharpTreeNode * GetRoot() const { return m_root; }
};

class SharpTreeNode {
private:
    u_int16_t                   m_tree_id;
    int                         m_child_idx;
    AM_TreeConfig               m_tree_config;
    SharpAggNode *              m_agg_node;
    SharpTreeEdge *             m_parent;
    vector_p_sharp_tree_edge    m_children;

public:
    SharpTreeNode(SharpAggNode *aggNode,
                  u_int16_t treeid,
                  AM_TreeConfig &tree_config);
    ~SharpTreeNode(){}

    inline u_int16_t GetTreeId() const { return m_tree_id; }

    inline void SetChildIdx(int child_idx) { m_child_idx = child_idx; }

    inline SharpTreeEdge * GetSharpParentTreeEdge() const { return m_parent; }
    inline void SetSharpParentTreeEdge(SharpTreeEdge * parent) { m_parent = parent; }

    inline u_int8_t GetChildrenSize() const { return (u_int8_t)this->m_children.size(); };

    int AddSharpTreeEdge(SharpTreeEdge *p_sharp_tree_eage,
                         u_int8_t db_index);
    SharpTreeEdge * GetSharpTreeEdge(u_int8_t db_index);

    void DumpTree(int indent_level, ofstream &sout);

    AM_TreeConfig GetTreeConfig() { return m_tree_config; }

    SharpAggNode * GetSharpAggNode() const { return m_agg_node; }

    SharpTree* GetRoot();
};

class SharpTreeEdge {
private:
    SharpTreeNode * m_remote_tree_node;
    u_int32_t m_qpn;
    u_int8_t m_child_idx;
    struct AM_QPCConfig  m_qpc_config;

public:
    SharpTreeEdge(u_int32_t qp_num, u_int8_t qp_idx);
    ~SharpTreeEdge(){}

   inline SharpTreeNode * GetRemoteTreeNode() const { return m_remote_tree_node; }
   //TODO remove this
   inline void SetRemoteTreeNode(SharpTreeNode * remote_tree_node) {
       m_remote_tree_node = remote_tree_node;
   }

   inline u_int32_t GetQpn() const { return m_qpn; }
   inline u_int8_t GetChildIdx() const { return m_child_idx; }

   inline struct AM_QPCConfig GetQPCConfig() const { return m_qpc_config; }
   inline void SetQPCConfig(struct AM_QPCConfig  qpc_config) { m_qpc_config = qpc_config; }

   void AddToQpcMap(map_qpn_to_qpc_config &qpc_map) {
       qpc_map.insert(pair<u_int32_t, struct AM_QPCConfig *> (m_qpn, &m_qpc_config));
   }
};


#endif  /* SHARP_MNGR_H */
