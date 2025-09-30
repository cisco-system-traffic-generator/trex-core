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


#ifndef MKEY_MNGR_H_
#define MKEY_MNGR_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <fstream>      // std::ifstream
using namespace std;

#include "ibis_types.h"
#include <infiniband/ibis/memory_pool.h>
#include <infiniband/misc/regexp/regexp.h>

typedef pair < u_int64_t, phys_port_t > pair_pguid_port_num_t;
typedef map < pair_pguid_port_num_t, pair_pguid_port_num_t > pguid_portNum_to_peer_pguid_portNum_map_t;
typedef map < pair_pguid_port_num_t, class MkeyPort *> pguid_portNum_to_MkeyPort_map_t;

typedef map <u_int16_t, uint64_t> lid_to_guid_map_t;
typedef map <u_int16_t, uint64_t> lid_to_mkey_map_t;
typedef map < uint64_t, class MkeyNode *> nguid_to_MkeyNode_map_t;
typedef map < uint64_t, uint64_t > pguid_to_mkey_map_t;
typedef map < u_int64_t, phys_port_t > pguid_to_port_num_map_t;
typedef vector<class MkeyPort * > vec_pmkeyport;

class MkeyNode {
public:
    uint64_t m_guid;
    uint64_t m_mkey;
    phys_port_t m_numPorts;
    vec_pmkeyport   Ports;    // Vector of all the ports (in index 0 we will put port0 if exist)
    MkeyNode(uint64_t guid, uint64_t mkey,
                 phys_port_t numPorts);

    ~MkeyNode();
    MkeyPort * makePort(phys_port_t  portNum);
    MkeyNode * getPeerNodeByPortNum(phys_port_t portNum);
    inline uint64_t getMKey() {return m_mkey;}
};

class MkeyPort {
public:
    uint64_t m_guid;
    phys_port_t m_portNum;

    MkeyPort * m_pPeerPort;
    MkeyNode * m_pNode;
    MkeyPort(MkeyNode *pNodePtr, phys_port_t portNum);
    ~MkeyPort() {};
    int connect(MkeyPort *pOtherPort);
};


class MKeyManager {
public:
    virtual ~MKeyManager() {};
    virtual bool getMkeyFromDRPath(const direct_route_t *pCurrDirectRoute, uint64_t &mkey) const
    {
        mkey = 0;

        if (!pCurrDirectRoute)
            cout << "-E- MKeyManager invalid direct route"
                 << endl;

        return false;
    }//empty implementation
    virtual uint64_t getMkeyFromDRPath(const direct_route_t *pCurrDirectRoute) const = 0;
    virtual uint64_t getMkeyByLid(u_int16_t lid) const = 0;
    // Not pure virtual methods to keep compatibility with other implementations (e.g. ar_mgr)
    virtual uint64_t getMKeyByPortGuid(uint64_t) const { return 0; }
    virtual int setMkeyManagerFabricTreeRoot(u_int64_t) { return 0; }
};

class FilesBasedMKeyManager : public MKeyManager {
    friend class MkeyNode;
private:
    uint64_t m_constMkey;

    pguid_to_mkey_map_t             m_pguidToMkey;
    nguid_to_MkeyNode_map_t         m_nguidToMkeymngr;
    lid_to_mkey_map_t               m_lidToMkey;
    pguid_to_port_num_map_t         m_guidToNumOfPorts;

    pguid_portNum_to_peer_pguid_portNum_map_t m_pguidPnToPguidPn;
    MkeyNode * m_rootMkeyNode;

    int parseNeighbors(const string neighborsFilePath);
    int parseGuid2MKey(const string pguid2mkeyFilePath);

    int buildMkeyManagerFabricTree();
    int addLink(uint64_t nodeGuid1, phys_port_t portNum1,
                uint64_t nodeGuid2, phys_port_t portNum2);
    int makeLinkBetweenPorts(MkeyPort *pPort1,
                             MkeyPort *pPort2);

    MkeyNode * getMKeyNodeByNodeGuid(uint64_t guid);
    MkeyPort *setMKeyNodePort(MkeyNode *pMkeyNode,
                                  phys_port_t  portNum);
    MkeyNode * makeMKeyNode(uint64_t nodeGuid);
public:
    FilesBasedMKeyManager();
    ~FilesBasedMKeyManager();

    inline void setConstMKey(uint64_t mkey) { m_constMkey = mkey;};
    inline uint64_t getConstMKey() const { return m_constMkey;};
    int parseAndCreateMKeyFabric(string neighborsFilePath,
                                 string pguid2mkeyFilePath);

    uint64_t getMKeyByPortGuid(uint64_t guid) const;
    bool getMkeyFromDRPath(const direct_route_t *pCurrDirectRoute, uint64_t &mkey) const;
    uint64_t getMkeyFromDRPath(const direct_route_t *pCurrDirectRoute) const;
    uint64_t getMkeyByLid(u_int16_t lid) const;
    int setMkeyManagerFabricTreeRoot(u_int64_t rootGuid);
    void setLidToNodeGuidMap(const lid_to_guid_map_t &lid2guid);
};

/****************************************************/

#endif	/* MKEY_MNGR_H_ */
