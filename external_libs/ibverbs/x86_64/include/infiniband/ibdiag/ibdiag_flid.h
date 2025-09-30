/*
 * Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBDIAG_FLID_H
#define IBDIAG_FLID_H


#include <map>
#include <vector>
#include <iostream>

#include "ibdiag_types.h"
#include "ibdiag_fabric_errs.h"
#include "infiniband/ibdm/Fabric.h"

struct SMP_RouterInfo;
class IBDiag;
class FLIDsManager {
private:

    struct Range {
        u_int32_t start;
        u_int32_t end;
        Range(u_int32_t inS = 0, u_int32_t inE = 0): start(inS), end(inE) {}

        bool operator=(const Range &other) const {
            return start == other.start && end == other.end;
        }

        bool operator<(const Range &other) const {
            return start < other.start || (start == other.start && end < other.end);
        };
    };

    typedef Range range_t;
    typedef std::vector< const IBNode * > vec_const_pnode;
    typedef std::map< range_t, vec_const_pnode > ranges_map;
    typedef std::map< u_int16_t, ranges_map > adj_subnets_ranges_map;
    typedef std::map< lid_t, vec_const_pnode > lid_to_nodes_map;
    typedef std::vector< const IBPort * > vec_const_p_port;
    typedef std::map< lid_t, vec_const_p_port > lid_to_ports_map;
    typedef std::map< const IBNode *, lid_to_ports_map > node_to_lids_map;
    typedef std::set< const IBNode* > set_const_p_node;

    class SubnetData {
        public:
           SubnetData() {}

           int pfrn_enabled = -1;
           int max_ar_group_id = -1;
           int64_t start_range = -1;
           int64_t end_range = -1;
    };
    typedef std::map < u_int16_t, SubnetData> subnet_by_prefix_map;
    typedef std::map< const IBNode*, subnet_by_prefix_map > router_subnets_map;

public:
    FLIDsManager(IBDiag *p_in): p_ibdiag(p_in), localSubnetPrefix(0) {}
    ~FLIDsManager() {}

    int Init(const std::string& name);
    int CheckLocalSubnet(list_p_fabric_general_err &errorsList);
    bool IsApplicable() const;
    int CheckAdjacentSubnets(list_p_fabric_general_err &errorsList);
    int CheckHCAsAndSwitches(list_p_fabric_general_err &errorsList);
    int CheckPFRNSettings(list_p_fabric_general_err &errorsList);
    int CheckRemoteEnabledFLIDs(list_p_fabric_general_err &errorsList);
    const char* GetLastError() const { return error.c_str(); }
    int Dump(const std::string &name);

private:
    int Dump(std::ostream &outStream);
    int CheckLocaLSubnetPFRN(list_p_fabric_general_err &errorsList);
    int CheckLocalAndAdjacentSubnetsPFRN(list_p_fabric_general_err &errorsList);
    int CheckRoutersRanges(list_p_fabric_general_err &errorsList);
    int CheckLocalAndGlobalRangesCorrectness(list_p_fabric_general_err &errorsList);
    bool IsConfiguredFLID(const IBNode &routerNode, const SMP_RouterInfo &routerInfo) const;
    int RangesToStream(const ranges_map &ranges, ostream &stream, int inLineNum);
    int CheckRanges(const ranges_map &ranges,
                     list_p_fabric_general_err &errorsList,
                     bool isGlobal);
    bool FindIntersection(lid_t min1, lid_t max1, lid_t min2, lid_t max2,
                         std::pair<lid_t, lid_t> &common) const;
    bool FindIntersection(const ranges_map &ranges_1, const ranges_map &ranges_2,
                          std::pair<lid_t, lid_t> &common) const;
    int DumpRanges(const string &message, const ranges_map &ranges, ostream &oustStream);
    int CollectAdjSubnetsRanges(list_p_fabric_general_err &errorsList);
    int CheckAdjSubnetsRanges(list_p_fabric_general_err &errorsList);
    int CheckAdjSubnetsSameRanges(list_p_fabric_general_err &errorsList);
    int CheckHCAs(list_p_fabric_general_err &errorsList);
    void CheckRouterLIDEnablementBitOnHCA(set_const_p_node &reportedNodes,
                                          const IBNode &remoteNode,
                                          const IBPort &remotePort,
                                          list_p_fabric_general_err &errorsList) const;
    int CheckSwitches(list_p_fabric_general_err &errorsList);
    range_t MakeRange(const AdjSubnetRouterLIDRecord &record) const;
    int FindLocalSubnetPrefix();
    bool GetFLID(const IBPort &port, lid_t &flid) const;
    int DumpAdjacentSubnetsAggregatedData(ostream &outStream);
    int DumpRouters(ostream &outStream);
    void DumpRouterAdjacentSubnets(const IBNode &router,
                                   ostream &outStream) const;
    void LocalEnabledFLIDsToStream(const IBNode &router,
                                   const SMP_RouterInfo &routerInfo,
                                   ostream &outStream) const;
    void NonLocalEnabledFLIDsToStream(const IBNode &router, ostream &outStream) const;
    int DumpFLIDsPerSwitches(ostream &outStream);
    int DumpSwitchesPerFLIDsHistogram(ostream &outStream);
    int FLIDsToStream(const lid_to_ports_map &hcaPortsByFLID, ostream &stream, int inLineNum);
    template <class T>
    int GUIDsToStream(const std::vector<const T*> &vec, ostream &stream, int inLineNum);
    void CollectRemoteEnabledFLIDs(u_int32_t from, u_int32_t to, IBNode &router,
                                   list_p_fabric_general_err &errorsList) const;
    void FindCommonLids();
    void LidsToStream(const vec_lids &lids, ostream &stream, int inLineNum) const;
    void DumpCommonLids(ostream &stream) const;

    IBDiag                 *p_ibdiag;
    std::string            error;
    ranges_map             globalRanges;
    ranges_map             localRanges;
    adj_subnets_ranges_map adjSubnetsRanges;
    //provides a nice output -- switches are ordered from the smallest to the largest FLID.
    lid_to_nodes_map       switchesByFLID;
    node_to_lids_map       switchesWithMultipleFLIDs;
    vec_lids               common;
    uint16_t               localSubnetPrefix;
    router_subnets_map     router_subnets;
};


template <class T>
int FLIDsManager::GUIDsToStream(const std::vector<const T*> &vec, ostream &stream, int inLineNum) {
    IBDIAG_ENTER

    //just to be on the safe side.
    if (vec.empty()) {
       stream << "[]";
       IBDIAG_RETURN(IBDIAG_SUCCESS_CODE);
    }

    stream << '[';
    typename std::vector<const T*>::const_iterator itrLast = --vec.end();
    if (!(*itrLast)) {
        this->error = "DB error: Null pointer found in the provided list.";
        IBDIAG_RETURN(IBDIAG_ERR_CODE_DB_ERR);
    }

    int j = 0;
    int inLine = (inLineNum > 0) ? inLineNum : static_cast<int>(vec.size());
    for (typename std::vector<const T*>::const_iterator itr = vec.begin();
            itr != itrLast && j < inLine; itr++) {
        if (!(*itr)) {
            this->error = "DB error: Null pointer found in the provided list.";
            IBDIAG_RETURN(IBDIAG_ERR_CODE_DB_ERR);
        }

        stream << PTR((*itr)->guid_get()) << ", ";
        j++;
    }

    if (j + 1 == static_cast<int>(vec.size()) && j < inLine)
        stream << PTR((*itrLast)->guid_get());
    else
        stream << "...";

    stream << ']';

    IBDIAG_RETURN(IBDIAG_SUCCESS_CODE);
}


#endif          /* IBDIAG_FLID_H */
