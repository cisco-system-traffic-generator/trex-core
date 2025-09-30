/*
 * Copyright (c) 2017-2020 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef IBDIAG_FABRIC_H
#define IBDIAG_FABRIC_H

#include <stdlib.h>
#include <infiniband/ibdm/Fabric.h>
#include "ibdiag_ibdm_extended_info.h"
#include <ibis/ibis.h>

class NodeRecord;
class PortRecord;
class SwitchRecord;
class ExtendedSwitchInfoRecord;
class LinkRecord;
class GeneralInfoSMPRecord;
class GeneralInfoGMPRecord;
class ExtendedNodeInfoRecord;
class ExtendedPortInfoRecord;
class PortInfoExtendedRecord;
class PortHierarchyInfoRecord;
class PhysicalHierarchyInfoRecord;
class ARInfoRecord;
class ChassisInfoRecord;

class IBDiagFabric {
private:

    CsvParser m_csv_parser;

    IBFabric &discovered_fabric;
    IBDMExtendedInfo &fabric_extended_info;
    CapabilityModule &capability_module;;

    u_int32_t nodes_found;
    u_int32_t sw_found;
    u_int32_t ca_found;
    u_int64_t ports_found;

    string    last_error;

    int CreateDummyPorts();

public:

    const string& GetLastError() const { return last_error; }

    u_int32_t getNodesFound() { return nodes_found;}
    u_int32_t getSWFound()    { return sw_found;}
    u_int32_t getCAFound()    { return ca_found;}
    u_int64_t getPortsFound() { return ports_found;}

    int UpdateFabric(const string &csv_file, bool ignore_missing_section = false);

    int CreateNode(const NodeRecord &nodeRecord);
    int CreatePort(const PortRecord &portRecord);
    int CreateSwitch(const SwitchRecord &switchRecord);
    int CreateLink(const LinkRecord &linkRecord);
    int CreateVSGeneralInfoSMP(const GeneralInfoSMPRecord &generalInfoSMPRecord);
    int CreateVSGeneralInfoGMP(const GeneralInfoGMPRecord &generalInfoGMPRecord);
    int CreateExtendedNodeInfo(const ExtendedNodeInfoRecord &extendedNodeInfoRecord);
    int CreateExtendedPortInfo(const ExtendedPortInfoRecord &extendedPortInfoRecord);
    int CreateExtendedSwitchInfo(const ExtendedSwitchInfoRecord &extendedSwitchInfoRecord);
    int CreatePortInfoExtended(const PortInfoExtendedRecord &portInfoExtendedRecord);
    int CreatePortHierarchyInfo(const PortHierarchyInfoRecord &portHierarchyInfoRecord);
    int CreatePhysicalHierarchyInfo(const PhysicalHierarchyInfoRecord &physicalHierarchyInfoRecord);
    int CreateARInfo(const ARInfoRecord &arInfoRecord);
    int CreateChassisInfo(const ChassisInfoRecord& chassisInfoRecord);

    IBDiagFabric(IBFabric &discovered_fabric, IBDMExtendedInfo &fabric_extended_info,
                 CapabilityModule &capability_module) :
        discovered_fabric(discovered_fabric), fabric_extended_info(fabric_extended_info),
        capability_module(capability_module), nodes_found(0), sw_found(0), ca_found(0),
        ports_found(0) { }
};
#endif   /* IBDIAG_FABRIC_H */
