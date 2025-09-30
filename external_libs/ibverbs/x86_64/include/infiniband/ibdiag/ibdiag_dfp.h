#ifndef IBDIAG_DFP_H
#define IBDIAG_DFP_H

#include <list>
#include <map>
#include <vector>

#include "ibdiag_types.h"
#include "infiniband/ibdm/Fabric.h"


class DFPIsland;

class DFPTopology {
private:
    typedef std::list<int> id_list;
    typedef std::map<int, id_list> global_link_id_map;
    typedef std::map<double, id_list> bandwidth_id_map;
    typedef std::vector<DFPIsland*> dfp_islands_vec;
    typedef std::map<size_t, dfp_islands_vec> islands_size_map;

    dfp_islands_vec  islands;
    DFPIsland *p_nonComputeIsland;
    PairsContainer<const IBNode*> edges;

    int CheckTopologyConnectivity(u_int32_t &warnings, u_int32_t &errors,
                                bool &allIslandsConnected);
    int CheckTopologySymmetric(u_int32_t &warnings, u_int32_t &errors,
                                bool &isSymmetric) const;
    int CheckMediumTopology(u_int32_t &warnings, u_int32_t &errors,
                        bool &isMedium, bool &topoToMedium);
    int BandwidthReport(u_int32_t &errors) const;
    int ResilientReport() const;
    double CalculateNetworkBandwidth(double bandwidth) const;
    int FindNonComputeIsland(u_int32_t &errors);
    void ExternalLinksReport(const global_link_id_map& linksToIdMap) const;
    int IslandRootsReport(u_int32_t &errors) const;
    int FillIslandsSizeMap(islands_size_map &islandsSizeMap, u_int32_t &errors) const;
    void IslandsToStream(ostream& stream, const dfp_islands_vec &vec) const;

public:
    DFPTopology (IBFabric& ref): p_nonComputeIsland(NULL), fabric(ref){}
    ~DFPTopology();

    IBFabric& fabric;

    int Build(map_guid_pnode &dfp_roots);
    int Validate(u_int32_t &warnings, u_int32_t &errors);
    int DumpToStream(ostream& stream) const;
    bool IsConnected(const IBNode *n1, const IBNode *n2) const;
};

class DFPIsland {
private:
    typedef std::list< const IBNode * > nodes_list;

    struct RemoteIslandConnectivityData {
        int globalLinks;
        bool resilient;

        RemoteIslandConnectivityData(): globalLinks(0), resilient(false) {}
    };
    typedef std::map<int, RemoteIslandConnectivityData> remote_islands_map;

    struct NodeData {
        int globalLinksToAll;
        bool resilientToAll;
        int freePorts;

        remote_islands_map remoteIslandsMap;

        NodeData(): globalLinksToAll(0),
                resilientToAll(true), freePorts(-1) {}
    };
    typedef std::map<const IBNode*, NodeData> connectivity_map;

    DFPTopology&   topology;
    int id;

    map_guid_pnode  nodes;
    map_guid_pnode  roots;
    map_guid_pnode  leaves;
    connectivity_map rootsConnectivityMap;
    double bandwidth;

    int CheckNotConnectedNodes(int rank, const map_guid_pnode& nodesMap,
                    u_int32_t &warnings, u_int32_t &errors) const;
    int CheckFullyCoonnetedLeavesAndRoots(u_int32_t &warnings, u_int32_t &errors) const;
    int DumpNodesToStream(ostream& stream,
                    int rank, const map_guid_pnode&  map) const;
public:

    DFPIsland(DFPTopology&  ref, int index): topology(ref),
            id(index), bandwidth(0){}

    const map_guid_pnode& GetRoots() const { return roots; };
    void AddRoot(IBNode *p_root);
    void AddLeaf(IBNode *p_leaf);
    int Validate(u_int32_t &warnings, u_int32_t &errors) const;
    int GetId() const { return id; }
    double GetBandwidth() const { return bandwidth; }
    int CountGlobalLinks(const DFPIsland *p_nonComputeIsland, u_int32_t &warnings) const;
    int CheckMedium(const DFPIsland *p_nonComputeIsland,
                    size_t islandsCount, bool &isMedium, bool &toMedium) const;
    int FillConnectivityData(const DFPIsland &other, bool &isConnected);
    int FillConnectivityData(const DFPIsland &other);
    void UpdateResilient();
    int DumpToStream(ostream& stream) const;
    int ConnectivityDetailsToStream(ostream& stream) const;
    int CheckResilient(const DFPIsland* p_nonComputeIsland,
                    bool &islResilient, bool &islPartialResilient) const;
};



#endif          /* IBDIAG_DFP_H */