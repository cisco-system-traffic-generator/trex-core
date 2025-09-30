/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#include "Fabric.h"


// Return 0 if fabrics match 1 otherwise.
// fill in the messages char array with diagnostics..
int
TopoMatchFabrics(
        IBFabric   *p_sFabric,          // The specification fabric
        IBFabric   *p_dFabric,          // The discovered fabric
        const char *anchorNodeName,     // The name of the node to be the anchor point - in the spec fabric
        phys_port_t anchorPortNum,      // The port number of the anchor port - in the spec fabric
        uint64_t    anchorPortGuid,     // Guid of the anchor port - in the discovered fabric
        char **messages,
		int compareNodesNames);			// Check that node's name match


// Build a merged fabric from a matched discovered and spec fabrics:
// * Every node from the discovered fabric must appear
// * We sued matched nodes and system names.
int
TopoMergeDiscAndSpecFabrics(
        IBFabric  *p_sFabric,
        IBFabric  *p_dFabric,
        IBFabric  *p_mFabric);

class TopoMatchOptions {
 public:
  string mapFileName; // user given mapping
  string csvFileName; // user requests CSV file to be generated
  string outMapFileName; // file containing mapped spec to disc node names and mapping reason
  bool matchCAsByName; // match non-SW by name
  bool matchSWsByName; // match SW by name
  bool smartSWsName; // use search switches by description if by name failed
  unsigned depth; // limit the depth of matching for CenterMode algorithm

  TopoMatchOptions() :
      mapFileName(""),
      csvFileName(""),
      outMapFileName(""),
      matchCAsByName(false),
      matchSWsByName(false),
      smartSWsName(false),
      depth(0) { }

  TopoMatchOptions(const string & mapFN, const string & csvFN, const string & outMapFN, bool CAsByName,
                   bool SWsByName, bool smart_sw_name, unsigned d) :
                       mapFileName(mapFN),
                       csvFileName(csvFN),
                       outMapFileName(outMapFN),
                       matchCAsByName(CAsByName),
                       matchSWsByName(SWsByName),
                       smartSWsName(smart_sw_name),
                       depth(d) { }
};

enum TopoMatchedBy { MATCH_BY_USER, MATCH_BY_NAME, MATCH_BY_CONN };
static inline const char * MatchByToStr(const TopoMatchedBy mb)
{
    switch (mb)
    {
    case MATCH_BY_USER:    return("User-Given");
    case MATCH_BY_NAME:    return("Name-Match");
    case MATCH_BY_CONN:    return("Connection");
    default:               return("UNKNOWN");
    }
};


// Return 0 if fabrics match, number of warnings and errors otherwise.
// The detailed compare messages are returned in messages
int
TopoMatchFabricsFromEdge(
        IBFabric *p_sFabric,    // The specification fabric
        IBFabric *p_dFabric,    // The discovered fabric
        const TopoMatchOptions &opts,  // options
        char **messages);

// Return 0 if fabrics match 1 otherwise.
// fill in the messages char array with diagnostics..
// this will only check that the discovered topology matches the spec around the anchor node
int
TopoMatchFabricsFromNode(
        IBFabric   *p_sFabric,          // The specification fabric
        IBFabric   *p_dFabric,          // The discovered fabric
        const char *anchorNodeName,     // The name of the node to be the anchor point - in the spec fabric
        phys_port_t anchorPortNum,      // The port number of the anchor port - in the spec fabric (0 if a switch)
        uint64_t    anchorPortGuid,     // Guid of the anchor port - in the discovered fabric (not mandatory - has precedence over name)
        const TopoMatchOptions &opts,   // options
        char **messages);

