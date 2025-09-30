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


#ifndef IBDM_FABRIC_AR_TRACE_ROUTE_H
#define IBDM_FABRIC_AR_TRACE_ROUTE_H

#include <map>

#include "Fabric.h"

class ARTraceRouteInfo;
class ARTraceRouteNodeInfo;

typedef list <ARTraceRouteInfo *> ARTraceRoutePath;

enum ARTraceRouteCount_t {
    AR_TRACE_ROUTE_GOOD_PATH = 0,
    AR_TRACE_ROUTE_DEAD_END,
    AR_TRACE_ROUTE_LOOP,
    AR_TRACE_ROUTE_END
};

int ARTraceRouteByLFT(IBFabric *p_fabric,
                      lid_t sLid , lid_t dLid,
                      ARTraceRouteInfo *&pArRouteInfo, int plane);

class ARTraceRouteInfo {
    static ARTraceRoutePath               sm_ARTraceRoutePath;

    //good path count and errors count
    u_int64_t m_routeStatistics[AR_TRACE_ROUTE_END];
    bool m_errorInPath;
    unsigned int m_minHops;
    unsigned int m_maxHops;

    ARTraceRouteNodeInfo  *m_pNodeInfo;

	//The in port used to generate this TraceRouteInfo.
	//might change if SLVLPortGroup used
    u_int8_t  m_currInPort;
    u_int8_t  m_currOutPort;
	//for cpu efficiency group together ports with same slvl table
    u_int8_t  m_inSLVLPortGroup;
	//if SLVLPortGroup used will skip outPort==m_currInPort
	u_int8_t  m_skippedOutPort;


    sl_vl_t   m_inSLVL;
    u_int8_t  m_pLFT;
    lid_t     m_dLid;
    bool      m_useAR;

    list_phys_ports m_portsList;
    list_phys_ports::iterator m_portsListIter;
    bool m_incIter; //if true move iterator before returning next element.

    //AR LFT info used for cache validation
    u_int16_t m_arLFTPortGroup;
    phys_port_t m_outStaticPort;

    set_pnode m_reachedFLIDs;

    bool isLoopInRoute(ARTraceRouteInfo *p_routeInfo);

public:

    static bool isPathEmpty() {
        return sm_ARTraceRoutePath.empty();
    }
    static void clearPath() {
        sm_ARTraceRoutePath.clear();
    }
    static ARTraceRouteInfo * pathFront() {
        return sm_ARTraceRoutePath.front();
    }

    ARTraceRouteNodeInfo* getARTraceRouteNodeInfo() { return m_pNodeInfo; }

    static void pathPushFront(ARTraceRouteInfo *p_arInfo);
    static void pathPopFront();

    ARTraceRouteInfo():
        m_errorInPath(false), m_minHops(0xFFFF), m_maxHops(0), m_pNodeInfo(NULL),
        m_currInPort(IB_LFT_UNASSIGNED), m_currOutPort(IB_LFT_UNASSIGNED), 
        m_inSLVLPortGroup(0), m_skippedOutPort(IB_LFT_UNASSIGNED),
		m_pLFT(0), m_dLid(0), m_useAR(false), m_incIter(false),
        m_arLFTPortGroup(IB_AR_LFT_UNASSIGNED),
        m_outStaticPort(IB_LFT_UNASSIGNED)
    {
        m_inSLVL.SL = 0;
        m_inSLVL.VL = 0;
        clearRouteStatistics();
    }

    bool isErrorInPath() const {return m_errorInPath;}
    u_int64_t getGoodPathCount() const {
        return m_routeStatistics[AR_TRACE_ROUTE_GOOD_PATH];}

    void set(sl_vl_t inSLVL, u_int8_t inPort, u_int8_t inSLVLPortGroup,
             u_int8_t pLFT, lid_t dLid,
             ARTraceRouteNodeInfo *pNodeInfo, int plane);

    bool isSet() const {return m_dLid != 0;}
    void clear() {m_dLid = 0;}
    bool convertDestLid(lid_t dLid);
    bool isDestinationLIDReachable(phys_port_t oldPort, phys_port_t newPort, lid_t dLid) const;

    u_int8_t getPLFT() const {return m_pLFT;}
    const sl_vl_t &getInSLVL() const {return m_inSLVL;}
    void setCurrentAttributes(phys_port_t inPort) {
        m_currInPort = inPort;
    }
    lid_t getDLID() const {return m_dLid;}

    phys_port_t getCurrentPort() const { return m_currOutPort;}
    phys_port_t getNextPort();
	phys_port_t getSkippedPort() const {
		return (phys_port_t)(m_currInPort == m_skippedOutPort ?
				IB_LFT_UNASSIGNED : m_skippedOutPort);
	}

    ARTraceRouteInfo* findNextARTraceRouteInfo(
        phys_port_t out_port, lid_t dLid, bool &reachedDest) const;
    ARTraceRouteInfo* getNextARTraceRouteInfo(phys_port_t out_port, int plane);

    unsigned int getMinHops() const {return m_minHops;}
    void updateRouteStatistics(ARTraceRouteInfo *p_childInfo);
    void addGoodPath(unsigned int hops, IBNode *nodeToTrack = nullptr);
    void markReachedFLIDs();
    void dumpRouteStatistics();
    void clearRouteStatistics(){
        memset(m_routeStatistics, 0, sizeof(m_routeStatistics));
    }

    void addCount(ARTraceRouteCount_t count_type){
        m_routeStatistics[count_type]++;
        if (count_type != AR_TRACE_ROUTE_GOOD_PATH)
            m_errorInPath = true;
    }
};

typedef vector<ARTraceRouteInfo > RouteInfoVec;
typedef vector<RouteInfoVec > RouteInfoVecVec;
typedef vector<RouteInfoVecVec > RouteInfoVecVecVec;

typedef list<ARTraceRouteInfo*> UsedRouteInfo;

class ARTraceRouteNodeInfo {

    //cache of created RouteInfo in last cycle
    UsedRouteInfo m_usedRouteInfo;

    IBNode *m_pNode;
    //m_routeInfoCollection[VL][SL][sl2vlPortGroup][pLFT]
    RouteInfoVecVecVec m_routeInfoCollection[IB_NUM_SL];

    //We can visit same node without entering a loop if we would like
    //to notify about this we should use visitCount
    int visitCount;

public:
    ARTraceRouteNodeInfo(IBNode *pNode):m_pNode(pNode), visitCount(0){}

    static int prepare(IBFabric *p_fabric);
    static void cleanup(IBFabric *p_fabric);
    static void clearDB(IBFabric *p_fabric);
    static void checkDB(IBFabric *p_fabric, lid_t dlid);

    static ARTraceRouteInfo* getRouteInfo(
        IBPort *p_port, sl_vl_t inSLVL, lid_t dLid, int plane){
        return ((ARTraceRouteNodeInfo *)(p_port->p_node->appData1.ptr))->
            getInfo(p_port, inSLVL, dLid, plane);
    }

    static ARTraceRouteInfo* findRouteInfo(
        IBPort *p_port, sl_vl_t inSLVL){
        return ((ARTraceRouteNodeInfo *)(p_port->p_node->appData1.ptr))->
            findInfo(p_port, inSLVL);
    }

    IBNode *getNode() {return m_pNode;}

    ARTraceRouteInfo* getInfo(
        IBPort *p_port, sl_vl_t inSLVL, lid_t dLid, int plane);

    ARTraceRouteInfo* findInfo(
        IBPort *p_port, sl_vl_t inSLVL);

    void visit(){
        visitCount++;
    }

    void leave(){
        visitCount--;
    }

};

#endif          /* IBDM_FABRIC_AR_TRACE_ROUTE_H */
