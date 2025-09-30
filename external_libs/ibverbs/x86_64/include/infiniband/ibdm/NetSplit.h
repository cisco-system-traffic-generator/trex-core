/*
 * Copyright (c) 2012 Mellanox Technologies LTD. All rights reserved.
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


#ifndef IBDM_NETSPLIT_H
#define IBDM_NETSPLIT_H


#include "Fabric.h"
#include <list>
#include <map>
#include <set>
#include <string>

#define map_port_set_ports std::map< IBPort *, std::set< IBPort* > >

// Given a fabric and a list of head ports split the hosts into groups
// by the minimal distance head port
int NetSplitGroupHostsByHeads(IBFabric &fabric, std::list<IBPort*> headPorts,
			      map_port_set_ports &portGroups);

int NetSplitGroupHostsByMinHop(IBFabric &fabric, unsigned int maxDist,
			       map_port_set_ports &portGroups);

// write port groups into a file
int NetSplitDumpGroupsFile(map_port_set_ports &portGroups,
			   std::string groupsFileName);

// parse groups
int NetSplitParseGroupsFile(IBFabric &fabric, string fileName,
			    map_port_set_ports &portGroups);

// mark which links are part of which partition
int NetSplitMarkMinHopLinks(IBPort *ctrlPort, std::set< IBPort* > &portGroup,
			    std::map<IBNode*, IBPort*> &switchToHead,
			    std::map<IBPort*, IBPort*> &portToHead);

// write out the join/split scripts
int NetSplitWriteSplitScripts(IBFabric &fabric, map_port_set_ports &portGroups,
			      std::map<IBNode*, IBPort*> &switchToHead,
			      std::map<IBPort*, IBPort*> &portToHead,
			      std::string outputDirName);

#endif          /* IBDM_NETSPLIT_H */
