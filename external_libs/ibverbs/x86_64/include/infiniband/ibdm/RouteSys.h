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


/*
 * Fabric Utilities Project
 *
 * Permutation Routing System abstraction Header file
 *
 * Author: Vladimir Zdornov, Mellanox Technologies
 *
 */

#ifndef IBDM_ROUTE_SYS_H_
#define IBDM_ROUTE_SYS_H_

#include <unistd.h>
#include "Fabric.h"

using namespace std;


/////////////////////////////////////////////////////////////////////////////
class inputData {
public:
    bool used;
    int src;
    int dst;
    int inputNum;
    int outNum;

    inputData() : used(false), src(0), dst(0), inputNum(0), outNum(0) {}
};


/////////////////////////////////////////////////////////////////////////////
// Routing system abstraction class
class RouteSys {
private:
    // Basic parameters
    int radix;
    int height;
    int step;
    int ports;

    // Ports data
    inputData* inPorts;
    bool* outPorts;

    // Sub-systems
    RouteSys** subSys;

    int myPow(int base, int pow);

public:
    // Constructor
    RouteSys(int rad, int hgth, int s=0);

    // Destructor
    ~RouteSys();

    // Add communication requests to the system
    // Format: i -> req[i]
    // Restriction: Requests must form a complete permutation
    int pushRequests(vec_int req);

    // Get input data for input port i
    inputData& getInput(int i);

    // Is output port i busy already?
    bool& getOutput(int i);

    // Invoke the system level routing
    int doRouting(vec_vec_int& out);
};


#endif
