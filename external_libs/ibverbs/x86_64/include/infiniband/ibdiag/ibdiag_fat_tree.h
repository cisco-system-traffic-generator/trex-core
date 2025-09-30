/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved
 * Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved
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


#ifndef IBDIAG_FT_H
#define IBDIAG_FT_H

#include <map>
#include <list>
#include <vector>
#include <sstream>
#include <bitset>

#include "ibdiag_types.h"
#include "ibdiag_fabric_errs.h"
#include "infiniband/ibdm/Fabric.h"

class regExp;
class FTClassification;
typedef vector<FTClassification*> classifications_vec;

class FTTopology;
class FTClassification {
public:
    typedef set <const IBNode*> nodes_set;
    typedef vector<nodes_set> nodes_by_rank_vec;
    typedef list<const IBNode*> nodes_list;

    struct NodeData {
        uint32_t caNum;
        uint32_t linksNum;

        NodeData(): caNum(0), linksNum(0) {}
        bool operator<(const NodeData& other) const {
            if (this == &other)   // Short?cut self?comparison
                return false;

            //compare first members
            if (caNum < other.caNum)
                return true;
            else if (caNum > other.caNum)
                return false;

            //first members are equal; compare second ones
            if (linksNum < other.linksNum)
                return true;
            else if (linksNum > other.linksNum)
                  return false;

            // All members are equal, so return false
            return false;
        }
    };
private:
    typedef map<int, nodes_list> distance_to_nodes_map;
    typedef map<const IBNode*, int> nodes_by_distance_map;
    typedef map<NodeData, nodes_list> nodedata_to_nodes_map;
    const IBNode* GetLeafToClassify(const classifications_vec &classifications,
                                      const nodes_list &nodes);
    int GetMaxThresholdDistance() const;
    void ClassifyByDistance(const IBNode &leaf);
    int SetNodesRanks();
    int Set2L_FTRanks();
    int Set3L_FTRanks();
    int Set4L_FTRanks();
    void SetRankToNodes(const nodes_list &inList,   nodes_set &outSet) const;
    int Set4L_DistanceToRanks(int distance, int compareDistance);
    bool EqualRanks(const nodes_set &one, const nodes_set &other);
    bool EqualsTo(const FTClassification &other) const;
    int CalculateThreshold() const;
    string ToString () const;

    struct SearchData {
       const IBNode* p_node;
       int distance;

       SearchData(const IBNode* p_inNode = NULL, int inDist = 0):
            p_node(p_inNode), distance(inDist) {}
    };

    int maxDistance;
    int maxThresholdDistance;
    distance_to_nodes_map distanceToNodesMap;
    nodes_by_distance_map nodesByDistanceMap;
    nodes_by_rank_vec   nodesByRank;
    stringstream lastError;
    const FTTopology &topology;

public:
    FTClassification(const FTTopology &inT);

    int Classify(const IBNode &fromLeaf);
    int CheckDistanceTo(const IBNode &node, bool &inDistance) const;
    const IBNode* GetLeafToClassify(const classifications_vec &classifications);
    int CountEquals(const classifications_vec &classifications) const;
    void SwapRanks(nodes_by_rank_vec &ranks);
    string GetLastError() const { return lastError.str(); }
      //leaves
};

class FTClassificationHandler {
private:
    classifications_vec classifications;

public:
    ~FTClassificationHandler();

    FTClassification* GetNewClassification(const FTTopology &inT);
    const classifications_vec& GetClassifications() const { return classifications; }
};

typedef std::pair<int, int> LinksData;

class FTTopology;
class FTNeighborhood {
public:
    typedef std::map<int, std::vector<uint64_t> >  link_to_nodes_map;
    typedef std::map<lid_t,
                    std::set < size_t >> lid_to_neighborhood_map;
    //flid to neighborhood's ID; allows printing neighborhood's ID in the ascending order
    
public:
    FTNeighborhood(FTTopology &inT, size_t inId = -1, size_t inRank = -1):
                topology(inT), id(inId), rank(inRank), totalUpLinks(0),  totalInternalLinks(0) {}
    ~FTNeighborhood() {}
    string GetLastError() const { return lastErrorStream.str(); }
    size_t GetId() const { return id; }
    void AddToUpNodes(const FTClassification::nodes_list &nodes) { AddNodes(nodes, true); }
    void AddToDownNodes(const FTClassification::nodes_list &nodes){ AddNodes(nodes, false); }
    int CheckUpDownLinks(list_p_fabric_general_err &errors, ostream &outFile);
    int CheckAsymmetricAPorts(ostream &outFile);
    int DumpToStream(ostream &stream) const;
    bool Contains(const IBNode *p_node) const {
            return (up.find(p_node) != up.end() ||
                    down.find(p_node) != down.end());
    }
    int MissingLinksReport(list_p_fabric_general_err &errors,
                          const PairsContainer<const IBNode*> &connectedNodes);
    int CollectFLIDs(lid_to_neighborhood_map &flidsToNeighborhoods);

private:
    int DumpNodesToStream(ostream &stream,
                        const FTClassification::nodes_set &nodes, const char *p_name) const;
    int CheckSetLinks(const FTClassification::nodes_set &nodes, size_t nodesRank, bool uplinks,
                    list_p_fabric_general_err &errors, ostream &stream);
    void SetLinksReport(ostream &outFile, const link_to_nodes_map &linksToNodesMap,
                        size_t nodesRank, bool uplinks) const;
    int CheckBlockingConfiguration(list_p_fabric_general_err &errors, ostream &outFile);
    int CheckInternalAPorts(ostream &outFile);
    int CheckExternalAPorts(ostream & outFile);

    //may be used in a future
    int CalculateUpDownLinks(LinksData &linksData);
    int CalculateUpDownLinks(const FTClassification::nodes_set &nodes, int &linksCount, bool isUp);

    void AddNodes(const FTClassification::nodes_list &nodes, bool isUp);
    bool IsWarning(size_t nodesRank, bool uplinks) const;
    void ReportToStream(ostream &stream, const link_to_nodes_map &map,
                        size_t maxInLine, const string &linkType) const;

    FTClassification::nodes_set up;
    FTClassification::nodes_set down;

    FTTopology &topology;
    size_t id;
    size_t rank;
    size_t totalUpLinks;
    size_t totalInternalLinks;
    stringstream lastErrorStream;
};

struct FTLinkIssue {
   const IBNode *p_node1;
   phys_port_t port1;
   size_t rank1;
   const IBNode *p_node2;
   phys_port_t port2;
   size_t rank2;

   FTLinkIssue(const IBNode *inNode1, phys_port_t inPort1, size_t inRank1,
            const IBNode*inNode2, phys_port_t inPort2, size_t inRank2):
                            p_node1(inNode1), port1(inPort1), rank1(inRank1),
                            p_node2(inNode2), port2(inPort2), rank2(inRank2) {}

   FTLinkIssue(const IBNode *inNode1, phys_port_t inPort1,
            const IBNode*inNode2, phys_port_t inPort2,
            size_t rank): p_node1(inNode1), port1(inPort1), rank1(rank),
                        p_node2(inNode2), port2(inPort2), rank2(rank) {}

   FTLinkIssue(const IBNode *inNode1, const IBNode*inNode2):
            p_node1(inNode1), port1(0), rank1(-1),
            p_node2(inNode2), port2(0), rank2(-1) {}

   FTLinkIssue():p_node1(NULL), port1(0), rank1(-1),
                p_node2(NULL), port2(0), rank2(-1) {}
};

#define FT_UP_HOP_SET_SIZE 2048
struct FTUpHopSet {
    typedef std::bitset<FT_UP_HOP_SET_SIZE> bit_set;

    FTUpHopSet():encountered(0) { upNodesBitSet.reset(); }

    bool IsSubset(const FTUpHopSet &other) const {
        return ((this->upNodesBitSet | other.upNodesBitSet) == other.upNodesBitSet);
    }
    bit_set Complement(const FTUpHopSet &other) const {
        return (~other.upNodesBitSet & this->upNodesBitSet);
    }
    //todo implement with bool functions
    bit_set Delta(const FTUpHopSet &other, size_t maxSize) const;
    bit_set Delta(const FTUpHopSet &other) const;
    bit_set Intersect(const FTUpHopSet &other) const {
        return (other.upNodesBitSet & this->upNodesBitSet);
    }
    void Merge(const FTUpHopSet &other, size_t maxSize);//todo take care of missing links
    int TakeOutUpNode(size_t index);

    //todo delete it
    void AddDownNodes(const FTUpHopSet &other);
    void AddDownNodes(const FTClassification::nodes_list &nodesToAdd);

    int GetNodeEncountered(size_t index, int &encountered) const;
    void InitEncounteredMap(size_t maxSize);

    int encountered;
    bit_set upNodesBitSet;
    FTClassification::nodes_list downNodes;

    typedef std::map<size_t, int> index_to_encounter_map;
    index_to_encounter_map encountered_map;
}; //todo Consider move to class

typedef std::vector <FTNeighborhood*> neighborhoods_vec;
class FTUpHopHistogram {
private:
      typedef std::map<std::string, FTUpHopSet> up_hop_sets_map;
      typedef std::map<size_t, const IBNode*> index_to_node_map;
      typedef std::map<const IBNode*, size_t> node_to_index_map;
      typedef std::vector <FTLinkIssue> link_issue_vec;

public:
    FTUpHopHistogram(FTTopology &inT, size_t inR): topology(inT), rank(inR),
                                            bitSetMaxSize(0), encounteredTresHold(0) {}
    int Init();
    int CreateNeighborhoods(list_p_fabric_general_err &errors);
    string GetLastError() const { return lastErrorStream.str(); }

private:
    std::string GetHashCode(const FTUpHopSet::bit_set &bitSet) const;
    void InitNodeToIndexConverters(const FTClassification::nodes_set &nodes);
    int NodeToIndex(size_t &index, const IBNode &p_node);
    const IBNode *IndexToNode(size_t index);
    int TryMergeSet_2(const FTUpHopSet &currentSet, bool &isMerged);
    int TryMergeTwoSets(const FTUpHopSet &currentSet, FTUpHopSet &other, bool &isMerged);
    int CheckCrossLinks(FTUpHopSet &currentSet);
    int SetsToNeighborhoods(list_p_fabric_general_err &errors);
    int BitSetToNodes(const FTUpHopSet::bit_set &bitSet, FTClassification::nodes_list &nodes);
    int InvalidLinksReport(list_p_fabric_general_err &errors,
                        const neighborhoods_vec &neighborhoods);
    const FTNeighborhood *FindNeighborhood(const neighborhoods_vec &neighborhoods,
                                        const IBNode *p_node);
    void AddIllegalLinkIssue(const FTLinkIssue &issue);
    int AddIllegalLinkIssues(size_t index, const FTClassification::nodes_list &downNodes);
    std::string UpHopSetToString(const FTUpHopSet &upHopSet);
    void CheckRootSwitchConnections(const IBNode &node);

private:
    index_to_node_map indexToNodeMap;
    node_to_index_map nodeToIndexMap;
    PairsContainer<const IBNode*> connectedNodes;

    stringstream lastErrorStream;
    up_hop_sets_map upHopSetsMap;
    FTTopology &topology;
    const size_t rank;
    size_t bitSetMaxSize;
    int encounteredTresHold;
    link_issue_vec invalidLinkIssuesVec;
};

class IBDiag;
class FTTopology {
public:
    typedef std::map<LinksData,
                    FTClassification::nodes_list > links_to_nodes_map;

public:
    FTTopology (IBFabric &fabRef, ostream &inStream):
                            fabric(fabRef), minimalRatio(0), outFile(inStream),
                            upHopSetFitInPercents(85), internalErrors(0){}
    ~FTTopology();

    int Build(list_p_fabric_general_err &errors, string &lastError);
    int Build(list_p_fabric_general_err &errors, string &lastError, regExp &rootsRegEx);
    int Build(list_p_fabric_general_err &errors, string &lastError, int retries, int equalResults);
    int Validate(list_p_fabric_general_err &errors, string &lastError);
    int Dump();
    size_t GetLevels() const { return nodesByRank.size(); }
    std::string LevelsReport() const;
    bool IsReportedLinkIssue(const IBNode* p_node1, const IBNode* p_node2) const;
    void AddNewLinkIssue(const IBNode* p_node1, const IBNode* p_node2);
    double GetMinimalRatio() const { return minimalRatio * GetPlanesNumber(); };
    LinksData GetSwitchLinksData(size_t rank, const IBNode &node);
    int SetNeighborhoodsOnRank(neighborhoods_vec &neighborhoods, size_t rank);
    const FTClassification::nodes_set *GetNodesOnRank(size_t rank);
    size_t GetNodeRank(const IBNode* p_node) const;
    bool IsLastRankNeighborhood(size_t rank) const;
    const FTClassification::NodeData *GetClassificationNodeData(const IBNode *p_node) const;
    int GetUpHopFitPercents() const { return upHopSetFitInPercents; };
    void SetUpHopFitPercents(int inT) { upHopSetFitInPercents = inT; }
    size_t GetInternalErrors() const { return internalErrors; }
    void IncrementInternalErrors(size_t in = 1) { internalErrors += in; }

    IBFabric& fabric;
    static int Show_GUID;

private:
    typedef std::map<const IBNode*, LinksData> node_to_links_map;
    typedef std::map<const IBNode*, FTClassification::NodeData> node_to_classification_data_map;

    const IBNode* GetFirstLeaf();
    int CreateNeighborhoods(list_p_fabric_general_err &errors);
    LinksData CalculateSwitchUpDownLinks(size_t rank, const IBNode& node);
    int CheckNeighborhoodsLinksAndAPorts(list_p_fabric_general_err &errors);
    int DumpNodesToStream();
    int DumpNeighborhoodsToStream();
    int CreateNeighborhoodsOnRank(list_p_fabric_general_err &errors, size_t rank);
    int CheckUpDownLinksAndAPorts(list_p_fabric_general_err &errors);
    int CheckFLIDs();
    int CalculateUpDownLinksMinRatio();
    int GetNodes(FTClassification::nodes_set &nodes, regExp &regExp);
    int FillRanksFromRoots(FTClassification::nodes_set &roots);
    int GetRootsBySMDB(FTClassification::nodes_set &roots);
    int GetPlanesNumber() const;

    std::vector< std::vector <FTNeighborhood*> > neighborhoodsByRank;
    node_to_links_map nodeToLinksMap;
    FTClassification::nodes_by_rank_vec nodesByRank;
    PairsContainer<const IBNode*> reportedLinksIssues;
    double minimalRatio;
    ostream &outFile;
    node_to_classification_data_map nodeDataMap;
    int upHopSetFitInPercents;
    size_t internalErrors;
    stringstream lastErrorStream;
};

class FTInvalidLinkError : public FabricErrGeneral {
public:
    FTInvalidLinkError(size_t id_1, size_t id_2,
                    const FTLinkIssue &issue, bool isNeighborhood);
    ~FTInvalidLinkError() {}

    string GetCSVErrorLine() override;
    string GetErrorLine() override;
};

class FTMissingLinkError : public FabricErrGeneral {
public:
    FTMissingLinkError(size_t id, const FTLinkIssue &issue, bool isNeighborhood);
    ~FTMissingLinkError() {}

    string GetCSVErrorLine() override;
    string GetErrorLine() override;
};

#endif          /* IBDIAG_FT_H */
