/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
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


#ifndef IBDM_FABRIC_TRACE_ROUTE_H
#define IBDM_FABRIC_TRACE_ROUTE_H

#include "Fabric.h"

// Trace a direct route from the given SM node port
int TraceDRPathRoute(IBPort *p_smNodePort, list_phys_ports drPathPortNums);

// Trace a route from slid to dlid by Min Hop
int TraceRouteByMinHops(IBFabric *p_fabric,
        lid_t slid , lid_t dlid);

// Trace a route from slid to dlid by LFT
int TraceRouteByLFT(IBFabric *p_fabric,
        lid_t slid , lid_t dlid,
        unsigned int *hops,
        vec_pnode *p_nodesList,
        vec_phys_ports *p_outPortsList,
        bool useVL15);

#endif          /* IBDM_FABRIC_TRACE_ROUTE_H */

