/*
 * Copyright (c) 2023-2023 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
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

/**
* @brief a15 (Hierarchy Info) simulation.
*  This static class is used to simulate a discovered Hierarchy Info for
*  ports in a system that was created from a .topo file.
*/

#include "infiniband/ibis/ibis_common.h"
#include <infiniband/ibdm/Fabric.h>

#define QUANTUM3_BM "SW_BLACK_MAMBA"
#define QUANTUM3_CR "SW_CROCODILE"

class SimulateA15 {
public:
    /**
     * @brief This function simulates hierarchy info for a given system.
     *        It should only be called for systems that were created based on a .topo file,
     *        and therfore lack heirarchy info.
     * 
     * @param p_system - pointer to a system.
     * @return
     */
    static int SimulateSystemHeirarchyInfo(IBSystem* p_system);

private:
    typedef enum ConnectionTypes {
            LEGACY_4X, // Non splitted legacy port
            LEGACY_2X, // Splitted legacy port
            PLNR_4X,   // Non Splitted planarized port
            PLNR_2X,   // Splitted planarized port
            INACTIVE
    } ConnectionTypes;

    static int SimulateBMHeirarchyInfo(IBSystem* p_system);
    static int SimulateCRHeirarchyInfo(IBSystem* p_system);
    static int SimulateCX8HeirarchyInfo(IBSystem* p_system);

    /**
     * @brief Get XDR asic number from a node's description using regex
     * 
     * @return node's asic number on success, -1 on failure.
     */
    static int GetAsicNumberFromNodeDescription(IBNode* p_node);

    /**
     * @brief Crocodile simulated Hierarchy info changes based on the nodes
     *        connections - IBPorts do not have constant APort numbers,
     *        APort number can change based on splitted or legacy connections.
     *        This function returns a given node's connection types.
     *
     * @param connections - OUTPUT: index i of this vector will contain
     *                              the connection type for port number i.
     * @return 0 if all connection types were found successfully, -1 otherwise.
     */
    static int GetConnectionTypes(IBNode* p_node, vector<ConnectionTypes>& connections);

    /**
     * @brief checks if a range of ports in a node have the following properties:
     *        1. They are all connected to a remote port
     *        2. They all have 1x width connections
     *        3. They are all connected to the same remote port guid
     *
     * @param p_node - node containing the port range
     * @param begin  - first index of the checked port group
     * @param end - last index of the checked port group
     * @return true if the port range have the properties described above,
     *         false otherwise.
     */
    static bool IsPlanarizedPortList(IBNode* p_node, phys_port_t begin, phys_port_t end);
};
