/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
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


#ifndef IBDM_CREDLOOPS_H
#define IBDM_CREDLOOPS_H


#include "Fabric.h"


// Analyze the fabric for credit loops.
int
CrdLoopAnalyze(IBFabric *p_fabric, bool checkAR);

int
CredLoopMode(int include_switch_to_switch_paths, int include_multicast);

class DelayedRouteInfo {
public:
    sl_vl_t m_slvl;
    IBPort *m_pPort;
    int m_hopCnt;
    DelayedRouteInfo(const sl_vl_t &slvl, IBPort *p_port, int hopCnt):
        m_pPort(p_port), m_hopCnt(hopCnt) {
        m_slvl.SL = slvl.SL;
        m_slvl.VL = slvl.VL;
    }
};

typedef list <DelayedRouteInfo> list_delayed_route_t;

struct CrdLoopCacheEntry {
    lid_t m_dlid;
    //in case the in port is part of the out ARPortGroup
    //we save the in port and use it if we arrive to this cache
    //entry from different port.
    phys_port_t m_delayedOutPort;
    CrdLoopCacheEntry():m_dlid(0),m_delayedOutPort(0){}
};

typedef vector<CrdLoopCacheEntry > vec_cache_entry;

class CrdLoopNodeInfo {

    //m_routeInfoCollection[VL][SL][isLidsGroup][pLFT][sl2vlPortGroup]
    vec_cache_entry m_nodeInfo[IB_NUM_VL][IB_NUM_SL][2][MAX_PLFT_NUM];
    IBNode *m_pNode;

public:

    CrdLoopNodeInfo() : m_pNode(NULL) {}

    static int prepare(IBFabric *p_fabric);
    static void cleanup(IBFabric *p_fabric);


    //returns 0 if not in cache
    //returns delayedOutPort if already in cache

    static phys_port_t updateCache(
            IBNode *p_node, const sl_vl_t &slvl, u_int8_t isLidsGroup,
            u_int8_t pLFT, phys_port_t sl2vlPortGroup, lid_t dLid) {
        return ((CrdLoopNodeInfo*)p_node->appData1.ptr)->updateCache(
            slvl, isLidsGroup, pLFT, sl2vlPortGroup, dLid);
    }

    static void updateDelayedOutPort(
            IBNode *p_node, const sl_vl_t &slvl, u_int8_t isLidsGroup,
            u_int8_t pLFT, phys_port_t sl2vlPortGroup,
            phys_port_t delayedOutPort) {
        ((CrdLoopNodeInfo*)p_node->appData1.ptr)->updateCache(
            slvl, isLidsGroup, pLFT, sl2vlPortGroup, delayedOutPort);
    }

    phys_port_t updateCache(const sl_vl_t &slvl, u_int8_t isLidsGroup,
            u_int8_t pLFT, phys_port_t sl2vlPortGroup, lid_t dLid);
    void updateDelayedOutPort(const sl_vl_t &slvl, u_int8_t isLidsGroup,
            u_int8_t pLFT, phys_port_t sl2vlPortGroup, phys_port_t delayedOutPort)
    {
            m_nodeInfo[slvl.VL][slvl.SL][isLidsGroup][pLFT][sl2vlPortGroup].
        m_delayedOutPort = delayedOutPort;
    }
};

#endif          /* IBDM_CREDLOOPS_H */

