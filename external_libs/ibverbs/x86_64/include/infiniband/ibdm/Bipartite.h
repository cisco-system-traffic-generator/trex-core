/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 * Bipartite Graph Header file
 *
 * Author: Vladimir Zdornov, Mellanox Technologies
 *
 */

#ifndef IBDM_BIPARTITE_H_
#define IBDM_BIPARTITE_H_

#include <list>
#include "RouteSys.h"

using namespace std;

typedef list<void*>::iterator peIter;
typedef list<void*> peList;

typedef enum side_ {LEFT=0, RIGHT} side;

class edge
{
 public:
  // Vertices
  void* v1;
  void* v2;
  // Connection indices
  int idx1;
  int idx2;

  // Edge list iterator
  peIter it;

  // Request data
  inputData reqDat;

  // C'tor
  edge():v1(NULL),v2(NULL),idx1(-1),idx2(-1){};

  // Get the vertex on the other side of the edge
  void* otherSide(const void* v) {
    if (v == v1)
      return v2;
    if (v == v2)
      return v1;
    return NULL;
  }

  // Check whether the edge is matched
  bool isMatched();
};

class vertex
{
  // ID
  int id;
  // Side (0 - left, 1 - right)
  side s;
  // Array of connected edges
  edge** connections;
  // Initial number of neighbors
  int radix;
  // Current number of neighbors
  int maxUsed;

  // Matching fields

  // Edge leading to the partner (NULL if none)
  edge* partner;
  // Array of layers predecessors
  edge** pred;
  // Number of predecessors
  int predCount;
  // Array of layers successors
  edge** succ;
  // Number of successors
  int succCount;
  // Denotes whether vertex is currently in layers
  bool inLayers;

 public:
  // C'tor
  vertex(int n, side sd, int rad);
  // D'tor
  ~vertex();
  // Getters + Setters
  int getID() const;
  side getSide() const;
  edge* getPartner() const;
  vertex* getPredecessor() const;
  bool getInLayers() const;
  void setInLayers(bool b);
  // Reset matching info
  void resetLayersInfo();
  // Adding partner to match layers
  void addPartnerLayers(list<vertex*>& l);
  // Adding non-partners to match layers
  // Return true if one of the neighbors was free
  bool addNonPartnersLayers(list<vertex*>& l);
  // Add new connection (this vertex only)
  void pushConnection(edge* e);
  // Remove given connection (vertices on both sides)
  void delConnection(edge* e);
  // Flip predecessor edge status
  // idx represents the layer index % 2 in the augmenting backward traversal (starting from 0)
  void flipPredEdge(int idx);
  // Remove vertex from layers, update predecessors and successors
  void unLink(list<vertex*>& l);
  // Get SOME connection and remove it (from both sides)
  // If no connections present NULL is returned
  edge* popConnection();
  // Match vertex to SOME unmatched neighbor
  // In case of success true is returned, false in case of failure
  bool match();
};

class Bipartite
{
  // Number of vertices on each side
  int size;
  // Number of edges per vertex
  int radix;
  // Vertices arrays
  vertex** leftSide;
  vertex** rightSide;

  peIter it;
  // Edges list
  peList List;

  // Apply augmenting paths
  void augment(list<vertex*>& l);

 public:
  // C'tor
  Bipartite(int s, int r);
  // D'tor
  ~Bipartite();
  int getRadix() const;
  // Set iterator to first edge (returns false if list is empty)
  bool setIterFirst();
  // Set iterator to next edge (return false if list end is reached)
  bool setIterNext();
  // Get inputData pointed by iterator
  inputData getReqDat();
  // Add connection between two nodes
  void connectNodes(int n1, int n2, const inputData & reqDat);
  // Find maximal matching on current graph
  void maximalMatch();
  // Find maximum matching and remove it from the graph
  // We use a variant of Hopcroft-Karp algorithm
  Bipartite* maximumMatch();
  // Decompose bipartite into to edge disjoint radix/2-regular bps
  // We use a variant of Euler Decomposition
  void decompose(Bipartite** bp1, Bipartite** bp2);
};

#endif
