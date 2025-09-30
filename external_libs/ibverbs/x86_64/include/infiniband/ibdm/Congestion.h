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


#ifndef IBDM_CONGESTION_H
#define IBDM_CONGESTION_H

#include "Fabric.h"
#include <sstream>

// initializing data structures per fabric
int
CongInit(IBFabric *p_fabric, bool ccMode);

// tracks a single flow path through the network
int
CongTrackPath(IBFabric *p_fabric, lid_t srcLid, lid_t dstLid, double ratio);

// perform BW calculation for each flow after tracing flows is done and
// before CongZero is called
int
CongCalcBW(IBFabric *p_fabric, bool dump, ostream &dumpF);

// dump the state of the links into a file. 
// need to be called before CongZero
int
CongDumpStage(ostream &f,
	      IBFabric *p_fabric,
	      unsigned int stage,
	      map< lid_t, unsigned int> &RankByLID);

// dump less information about a stage.
// need to be called before CongZero
int
CongDump(IBFabric *p_fabric, ostream &out);

// accumulate the host-spot-degree statistics and clears the stage data
int
CongZero(IBFabric *p_fabric);

// report multiple stages accumulated data about the entire run
int
CongReport(IBFabric *p_fabric, ostream &out);

// cleanup of the data structures - free the memory
int
CongCleanup(IBFabric *p_fabric);

#endif /* IBDM_CONGESTION_H */
