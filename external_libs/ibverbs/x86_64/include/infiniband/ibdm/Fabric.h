/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
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


/*
 * Fabric Utilities Project
 *
 * Data Model Header file
 *
 */

#ifndef IBDM_FABRIC_H
#define IBDM_FABRIC_H

#include <cstdint>
#include <unordered_set>

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <functional>
#include <map>
#include <set>
#include <list>
#include <bitset>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <glob.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <inttypes.h>

using namespace std;

#include <infiniband/ibis/ibis_common.h>
#include <infiniband/ibutils/ports_bitset.h>
#include <infiniband/ibdm/OutputControl.h>

///////////////////////////////////////////////////////////////////////////////
const char * get_ibdm_version();

// string vectorization (Vectorize.cpp)
std::string compressNames (std::list<std::string> &words);

///////////////////////////////////////////////////////////////////////////////
#define IB_LID_UNASSIGNED 0
#define IB_LFT_UNASSIGNED 255
#define IB_AR_LFT_UNASSIGNED 0xFFFF
#define IB_SLT_UNASSIGNED 255
#define IB_HOP_UNASSIGNED 255
#define IB_NUM_SL 16
#define IB_NUM_VL 16
#define IB_DROP_VL 15

#define SPLIT_PORTS_QUANTOM1_NUM_PORTS      80
#define SPLIT_PORTS_QUANTOM1_AN_NUM_PORTS   SPLIT_PORTS_QUANTOM1_NUM_PORTS + 1
#define SPLIT_PORTS_QUANTOM2_NUM_PORTS      128
#define SPLIT_PORTS_QUANTOM2_AN_NUM_PORTS   SPLIT_PORTS_QUANTOM2_NUM_PORTS + 1


//maximum number of virtual ports that can be returned in vnode info (not counting port 0)
//the max number of the vport can be 64000
#define IB_MAX_VIRT_NUM_PORTS  64000
//maximum number of physical ports that can be returned in node info (not counting port 0)
#define IB_MAX_PHYS_NUM_PORTS  254
#define IB_MIN_PHYS_NUM_PORTS  1
#define IB_MAX_UCAST_LID       0xBFFF

#define IB_MCAST_STATE_SENDER_ONLY_BIT 0x4
#define IB_MCAST_STATE_SENDER_ONLY_FULL_MEMBER_BIT 0x8

#ifndef PRIx64
#if __WORDSIZE == 64
#define PRIx64 "lx"
#else
#define PRIx64 "llx"
#endif
#endif

#define EPF_FILE_VERSION     1
#define RAIL_FILTER_FILE_VERSION     1
#define END_PORT_PLANE_FILTER_FILE_VERSION     1

#define GET_LID_STEP(lmc) ((lid_t)(1 << lmc))
#define LID_INC(lid,inc) (lid = (lid_t)(lid + inc))

#define NOT_AVAILABLE             "N/A"

#define DATA_WORTHY_GMP_MAD false
#define DATA_WORTHY_SMP_MAD true

#define DEFAULT_KEY  0x00
#define DEFAULT_KEY_STR  TO_STR(DEFAULT_KEY)

// CC_ALGO Macros
#define MAX_CC_ALGO_SLOT          16
#define CC_ALGO_ENCAP_TYPE_1      1
#define CC_ALGO_ENCAP_TYPE_2      2
#define MAX_CC_ALGO_ARRAY_LAYOUT  44

// When system has one mlx, node description will present mlx_0.
// As the max number of mlx in a system is from parsing the node description,
// we need to initialize the number of mlx with the value -1 and not 0, because 0 says
// we have one mlx
#define MLX_NA  -1
///////////////////////////////////////////////////////////////////////////////
//
// STL TYPEDEFS
//
// This section defines typedefs for all the STL containers used by the package

typedef vector<int > vec_int;
typedef vector<vec_int > vec_vec_int;
typedef vector<uint8_t > vec_byte;
typedef vector<uint16_t > vec_uint16;
typedef vector<vec_uint16 > vec_vec_uint16;
typedef vector<u_int32_t > vec_uint32;
typedef vector<vec_uint32 > vec_vec_uint32;
typedef vector<uint64_t > vec_uint64;
typedef vector<vec_byte > vec_vec_byte;
typedef vector<vec_vec_byte > vec3_byte;
typedef vector< PortsBitset > vec_port_bitset;

typedef vector<bool > vec_bool;
typedef vector<vec_bool > vec_vec_bool;
typedef vector<vec_vec_bool > vec3_bool;

typedef vector<phys_port_t > vec_phys_ports;
typedef vector<lid_t > vec_lids;
typedef vector<class IBPort * > vec_pport;
typedef vector<class IBVPort * > vec_pvport;
typedef map< uint16_t, class IBVPort * > map_vportnum_vport;
typedef vector<class IBNode * > vec_pnode;
typedef set<class IBNode * > set_pnode;
typedef vector<class VChannel * > vec_pvch;
typedef vector<class CrdRoute > vec_crd_route;
typedef map< string, class IBSysPort * > map_str_psysport;
typedef map< string, class IBNode * > map_str_pnode;
typedef map< string, class IBSystem * > map_str_psys;
typedef map< uint64_t, class IBPort * > map_guid_pport;
typedef map< uint64_t, class IBVPort * > map_guid_pvport;
typedef map< uint64_t, class IBVNode * > map_guid_pvnode;
typedef map< uint64_t, class IBNode * > map_guid_pnode;
typedef map< uint64_t, class IBSystem * > map_guid_psys;
typedef map< uint64_t, string > map_guid_str;
typedef map< string, int > map_str_int;
typedef map< string, string > map_str_str;

typedef vector < class APort * > vec_p_aport;
typedef map < uint64_t, vec_p_aport > map_guid_p_aports;

typedef list<lid_t > list_lid;
typedef set< lid_t > set_lid;
typedef map< lid_t, vec_pnode > map_nodes_per_lid;

//to be sure port appears only once.
typedef std::set < phys_port_t > set_ports;
typedef std::map< uint64_t, set_ports > map_guid_to_ports;

typedef list<class IBPort * > list_p_port;

typedef list<class APort* > list_p_aport;
typedef map < uint64_t, list_p_aport > map_guid_list_p_aport;

typedef list<class IBNode * > list_pnode;
typedef list<class IBSystem * > list_psystem;
typedef list<class CrdRoute > list_crd_route;
typedef list<string > list_str;
typedef list<phys_port_t > list_phys_ports;
typedef vector<list_phys_ports > vec_list_phys_ports;
typedef map< IBNode *, int > map_pnode_int;
typedef map< IBPort *, int > map_pport_int;
typedef map< IBNode *, vec_int > map_pnode_vec_int;
typedef map< IBSystem *, int > map_psystem_int;
typedef map< string, list_pnode > map_str_list_pnode;
typedef map< string, vector<string> > map_str_vec_str;
typedef set< uint8_t > set_uint8;
typedef set< uint16_t > set_uint16;
typedef set < class APort* > set_p_aport;
typedef set < class IBPort* > set_p_port;

typedef map< IBNode *, IBNode * > map_template_pnode;
typedef set< string > set_boards;
typedef set< IBNode * > set_nodes_ptr;
typedef map< IBNode *, PortsBitset >
        map_pnode_ports_bitset;

typedef struct direct_route direct_route_t;
typedef list< pair <IBNode *,direct_route_t *> > direct_route_list;
typedef direct_route_list::iterator direct_route_list_iter;
typedef direct_route_list::const_iterator direct_route_list_const_iter;

typedef pair < u_int16_t, IBVPort * > pair_vportnum_vport;
///////////////////////////////////////////////////////////////////////////////
//
// CONSTANTS
//
#define FABU_LOG_NONE 0x0
#define FABU_LOG_ERROR 0x1
#define FABU_LOG_INFO 0x2
#define FABU_LOG_VERBOSE 0x4
#define IBNODE_UNASSIGNED_RANK 0xFF

#define MAX_PLFT_NUM 8
// DFS constants type
typedef enum {Untouched,Open,Closed} dfs_t;

//
// GLOBALS
//
// Log level should be part of the "main" module
extern uint8_t FabricUtilsVerboseLevel;

void ibdmUseInternalLog();
void ibdmUseCoutLog();
void ibdmClearInternalLog();
char *ibdmGetAndClearInternalLog();


///////////////////////////////////////////////////////////////////////////////
enum NodeTypesFilter {
    NodeTypesFilter_NONE                               = 0,
    NodeTypesFilter_SW                                 = 0x1,
    NodeTypesFilter_HCA                                = 0x2,
    NodeTypesFilter_AN                                 = 0x4,
    NodeTypesFilter_RTR                                = 0x8,
    NodeTypesFilter_GW                                 = 0x10,
    NodeTypesFilter_ALL_HCA                            = NodeTypesFilter_HCA | NodeTypesFilter_AN |
                                                         NodeTypesFilter_RTR | NodeTypesFilter_GW,
    NodeTypesFilter_ALL_EXLUDE_EXCLUDE_SPECIAL_NODE    = NodeTypesFilter_SW | NodeTypesFilter_HCA,
    NodeTypesFilter_ALL                                = 0xffff
};

typedef enum {
    IB_UNKNOWN_NODE_TYPE,
    IB_CA_NODE,
    IB_SW_NODE,
    IB_RTR_NODE
} IBNodeType;

#define UNDEFINED_PLANE -1000

#define HIERARCHY_INFO_PORT_TYPE_FNM1    1
#define HIERARCHY_INFO_PORT_TYPE_FNM     2
#define HIERARCHY_INFO_PORT_TYPE_SW      3
#define HIERARCHY_INFO_PORT_TYPE_HCA     4
#define HIERARCHY_INFO_PORT_TYPE_ACCESS  5
#define HIERARCHY_INFO_PORT_TYPE_GPU     6

#define FNM_FORWARD_PORT   145
#define FNM_BACKWARD_PORT  146

static inline const char * hierPortTypeInt2char(const int t)
{
    switch (t) {
        case HIERARCHY_INFO_PORT_TYPE_FNM1:    return("FNM1");
        case HIERARCHY_INFO_PORT_TYPE_FNM:     return("FNM");
        case HIERARCHY_INFO_PORT_TYPE_SW:      return("sw");
        case HIERARCHY_INFO_PORT_TYPE_HCA:     return("HCA");
        case HIERARCHY_INFO_PORT_TYPE_ACCESS:  return("Access");
        case HIERARCHY_INFO_PORT_TYPE_GPU:     return("GPU");
        default:  return("N/A");
    }
};

static inline const char * hierAsicNameInt2char(const int n)
{
    switch (n) {
        case 1:    return("A1");
        case 2:    return("A2");
        case 3:    return("A3");
        case 4:    return("A4");
        case 256:  return("A");
        case 257:  return("B");
        default:   return("N/A");
    }
};

#define MIN_MULTINODE_ASIC_NAME 1
#define MAX_MULTINODE_ASIC_NAME 4

#define HIERARCHY_TEMPLATE_GUID_PHYSICAL      0x01
#define HIERARCHY_TEMPLATE_GUID_PORT_0x03     0x03
#define HIERARCHY_TEMPLATE_GUID_PORT_0x04     0x04
#define HIERARCHY_TEMPLATE_GUID_PORT_0x05     0x05
#define HIERARCHY_TEMPLATE_GUID_PORT_0x06     0x06

#define IB_SLOT_TYPE_SLOT 0
#define IB_SLOT_TYPE_PHYS 1

#define DEV_ID_SW           0xD2F4
#define DEV_ID_HCA          0x1023

typedef enum {
    IB_UNKNOWN_EXT_NODE_TYPE,
    IB_GPU_NODE,
    IB_MULTI_PLANE_HCA_NODE
} IBExtNodeType;

enum SerDesVersion {
    SerDes_NA = 0,
    SerDes_40nm,
    SerDes_28nm,
    SerDes_16nm,
    SerDes_7nm,
    SerDes_5nm
};

#define SerDes_NA_STR     "0"
#define SerDes_40nm_STR   "40"
#define SerDes_28nm_STR   "28"
#define SerDes_16nm_STR   "16"
#define SerDes_7nm_STR    "7"
#define SerDes_5nm_STR    "5"

static inline const char * serdesversion2char(SerDesVersion ver)
{
    switch (ver) {
    case SerDes_NA:   return SerDes_NA_STR;
    case SerDes_40nm: return SerDes_40nm_STR;
    case SerDes_28nm: return SerDes_28nm_STR;
    case SerDes_16nm: return SerDes_16nm_STR;
    case SerDes_7nm:  return SerDes_7nm_STR;
    case SerDes_5nm:  return SerDes_5nm_STR;
    default:          return SerDes_NA_STR;
    }
}

static inline SerDesVersion char2serdesversion(const char *str)
{
    if (!str || !*str)
        return SerDes_NA;

    if (!strcmp(str, SerDes_NA_STR))   return SerDes_NA;
    if (!strcmp(str, SerDes_40nm_STR)) return SerDes_40nm;
    if (!strcmp(str, SerDes_28nm_STR)) return SerDes_28nm;
    if (!strcmp(str, SerDes_16nm_STR)) return SerDes_16nm;
    if (!strcmp(str, SerDes_7nm_STR))  return SerDes_7nm;
    if (!strcmp(str, SerDes_5nm_STR))  return SerDes_5nm;

    return SerDes_NA;
}

class PluginData
{
public:
    virtual ~PluginData() {;}
};

static inline IBNodeType char2nodetype(const char *w)
{
    if (!w || (*w == '\0')) return IB_UNKNOWN_NODE_TYPE;
    if (!strcmp(w,"SW"))    return IB_SW_NODE;
    if (!strcmp(w,"CA"))    return IB_CA_NODE;
    if (!strcmp(w,"RTR"))   return IB_RTR_NODE;
    if (!strcmp(w,"Rt"))    return IB_RTR_NODE; // SM use for LST file 'Rt' ibdiagnet 'RTR'
    return IB_UNKNOWN_NODE_TYPE;
};

static inline const char * nodetype2char(const IBNodeType w)
{
    switch (w) {
    case IB_SW_NODE:    return("SW");
    case IB_CA_NODE:    return("CA");
    case IB_RTR_NODE:   return("RTR");
    default:            return("UNKNOWN");
    }
};

#define IB_DISABLE_LINK_SPEED_STR       "0"
#define IB_DISABLE_LINK_WIDTH_STR       "0"

typedef enum {IB_UNKNOWN_LINK_WIDTH = 0,
              IB_LINK_WIDTH_1X = 1,
              IB_LINK_WIDTH_4X = 2,
              IB_LINK_WIDTH_8X = 4,
              IB_LINK_WIDTH_12X = 8,
              IB_LINK_WIDTH_2X = 0x10,
} IBLinkWidth;

typedef enum {
              IB_UNKNOWN_LINK_WIDTH_INDEX = 0,
              IB_LINK_WIDTH_1X_INDEX,
              IB_LINK_WIDTH_4X_INDEX,
              IB_LINK_WIDTH_8X_INDEX,
              IB_LINK_WIDTH_12X_INDEX,
              IB_LINK_WIDTH_2X_INDEX,
              IB_LINK_WIDTH_END_INDEX
} IBLinkWidthIndex;

static inline IBLinkWidth char2width(const char *w)
{
    if (!w || (*w == '\0')) return IB_UNKNOWN_LINK_WIDTH;
    if (!strcmp(w,"1x"))    return IB_LINK_WIDTH_1X;
    if (!strcmp(w,"4x"))    return IB_LINK_WIDTH_4X;
    if (!strcmp(w,"8x"))    return IB_LINK_WIDTH_8X;
    if (!strcmp(w,"12x"))   return IB_LINK_WIDTH_12X;
    if (!strcmp(w,"2x"))    return IB_LINK_WIDTH_2X;
    return IB_UNKNOWN_LINK_WIDTH;
};

static inline const char * width2char(const IBLinkWidth w, const char* out_name = "UNKNOWN")
{
    switch (w) {
    case IB_LINK_WIDTH_1X:  return("1x");
    case IB_LINK_WIDTH_4X:  return("4x");
    case IB_LINK_WIDTH_8X:  return("8x");
    case IB_LINK_WIDTH_12X: return("12x");
    case IB_LINK_WIDTH_2X:  return("2x");
    default:                return(out_name);
    }
};


static inline string width_to_str(const IBLinkWidth w) {
    return string(width2char(w));
}

static inline IBLinkWidthIndex width2width_index(const IBLinkWidth width)
{
    switch (width) {
    case IB_LINK_WIDTH_1X:  return IB_LINK_WIDTH_1X_INDEX;
    case IB_LINK_WIDTH_4X:  return IB_LINK_WIDTH_4X_INDEX;
    case IB_LINK_WIDTH_8X:  return IB_LINK_WIDTH_8X_INDEX;
    case IB_LINK_WIDTH_12X: return IB_LINK_WIDTH_12X_INDEX;
    case IB_LINK_WIDTH_2X:  return IB_LINK_WIDTH_2X_INDEX;
    default:                return IB_UNKNOWN_LINK_WIDTH_INDEX;
    }
};


static inline IBLinkWidth width_index2width(const IBLinkWidthIndex width_index)
{
    switch (width_index) {
    case IB_LINK_WIDTH_1X_INDEX:  return IB_LINK_WIDTH_1X;
    case IB_LINK_WIDTH_4X_INDEX:  return IB_LINK_WIDTH_4X;
    case IB_LINK_WIDTH_8X_INDEX:  return IB_LINK_WIDTH_8X;
    case IB_LINK_WIDTH_12X_INDEX: return IB_LINK_WIDTH_12X;
    case IB_LINK_WIDTH_2X_INDEX:  return IB_LINK_WIDTH_2X;
    default:                return IB_UNKNOWN_LINK_WIDTH;
    }
};

static inline unsigned int width2uint(const IBLinkWidth width)
{
    switch (width) {
    case IB_LINK_WIDTH_1X:  return 1;
    case IB_LINK_WIDTH_4X:  return 4;
    case IB_LINK_WIDTH_8X:  return 8;
    case IB_LINK_WIDTH_12X: return 12;
    case IB_LINK_WIDTH_2X:  return 2;
    default:                return 0;
    }
};

static inline IBLinkWidth uint2width(unsigned int width)
{
    switch (width) {
    case 1:  return IB_LINK_WIDTH_1X;
    case 4:  return IB_LINK_WIDTH_4X;
    case 8:  return IB_LINK_WIDTH_8X;
    case 12: return IB_LINK_WIDTH_12X;
    case 2:  return IB_LINK_WIDTH_2X;
    default: return IB_UNKNOWN_LINK_WIDTH;
    }
};

typedef enum {IB_UNKNOWN_LINK_SPEED = 0,
              IB_LINK_SPEED_2_5     = 1,
              IB_LINK_SPEED_5       = 2,
              IB_LINK_SPEED_10      = 4,
              IB_LINK_SPEED_14      = 1 << 8,   /* second byte is for extended ones */
              IB_LINK_SPEED_25      = 2 << 8,   /* second byte is for extended ones */
              IB_LINK_SPEED_50      = 4 << 8,   /* second byte is for extended ones */
              IB_LINK_SPEED_100     = 8 << 8,   /* second byte is for extended ones */
              IB_LINK_SPEED_FDR_10  = 1 << 16,  /* third byte is for vendor specific ones */
              IB_LINK_SPEED_EDR_20  = 2 << 16,  /* third byte is for vendor specific ones */
              IB_LINK_SPEED_200     = 1 << 24   /* fourth byte is for extended 2 ones */

} IBLinkSpeed;


typedef enum {
              IB_UNKNOWN_LINK_SPEED_INDEX = 0,
              IB_LINK_SPEED_2_5_INDEX,
              IB_LINK_SPEED_5_INDEX,
              IB_LINK_SPEED_10_INDEX,
              IB_LINK_SPEED_14_INDEX,
              IB_LINK_SPEED_25_INDEX,
              IB_LINK_SPEED_50_INDEX,
              IB_LINK_SPEED_100_INDEX,
              IB_LINK_SPEED_FDR_10_INDEX,
              IB_LINK_SPEED_EDR_20_INDEX,
              IB_LINK_SPEED_200_INDEX,
              IB_LINK_SPEED_END_INDEX
} IBLinkSpeedIndex;


static inline double speed2double(const IBLinkSpeed s)
{
    switch (s) {
    case IB_LINK_SPEED_2_5:     return 2.5;
    case IB_LINK_SPEED_5:       return 5;
    case IB_LINK_SPEED_10:      return 10;
    case IB_LINK_SPEED_14:      return 14;
    case IB_LINK_SPEED_25:      return 25;
    case IB_LINK_SPEED_50:      return 50;
    case IB_LINK_SPEED_100:     return 100;
    case IB_LINK_SPEED_FDR_10:  return 14; //actual value 14.06
    case IB_LINK_SPEED_EDR_20:  return 25; //actual value 25.78;
    case IB_LINK_SPEED_200:     return 200;
    default:                    return 0;
    }
};


#define IB_UNKNOWN_LINK_SPEED_STR    "UNKNOWN"
#define IB_LINK_SPEED_2_5_STR        "2.5"
#define IB_LINK_SPEED_5_STR          "5"
#define IB_LINK_SPEED_10_STR         "10"
#define IB_LINK_SPEED_14_STR         "14"
#define IB_LINK_SPEED_25_STR         "25"
#define IB_LINK_SPEED_50_STR         "50"
#define IB_LINK_SPEED_100_STR        "100"
#define IB_LINK_SPEED_FDR_10_STR     "FDR10"
#define IB_LINK_SPEED_EDR_20_STR     "EDR20"
#define IB_LINK_SPEED_200_STR        "200"


static inline const char * speed2char(const IBLinkSpeed s)
{
    switch (s) {
    case IB_LINK_SPEED_2_5:     return(IB_LINK_SPEED_2_5_STR);
    case IB_LINK_SPEED_5:       return(IB_LINK_SPEED_5_STR);
    case IB_LINK_SPEED_10:      return(IB_LINK_SPEED_10_STR);
    case IB_LINK_SPEED_14:      return(IB_LINK_SPEED_14_STR);
    case IB_LINK_SPEED_25:      return(IB_LINK_SPEED_25_STR);
    case IB_LINK_SPEED_50:      return(IB_LINK_SPEED_50_STR);
    case IB_LINK_SPEED_100:     return(IB_LINK_SPEED_100_STR);
    case IB_LINK_SPEED_FDR_10:  return(IB_LINK_SPEED_FDR_10_STR);
    case IB_LINK_SPEED_EDR_20:  return(IB_LINK_SPEED_EDR_20_STR);
    case IB_LINK_SPEED_200:     return(IB_LINK_SPEED_200_STR);
    default:                    return(IB_UNKNOWN_LINK_SPEED_STR);
    }
};

static inline IBLinkSpeed char2speed(const char *s)
{
    if (!s || (*s == '\0'))                   return IB_UNKNOWN_LINK_SPEED;
    if (!strcmp(s, IB_LINK_SPEED_2_5_STR))    return IB_LINK_SPEED_2_5;
    if (!strcmp(s, IB_LINK_SPEED_5_STR))      return IB_LINK_SPEED_5;
    if (!strcmp(s, IB_LINK_SPEED_10_STR))     return IB_LINK_SPEED_10;
    if (!strcmp(s, IB_LINK_SPEED_14_STR))     return IB_LINK_SPEED_14;
    if (!strcmp(s, IB_LINK_SPEED_25_STR))     return IB_LINK_SPEED_25;
    if (!strcmp(s, IB_LINK_SPEED_50_STR))     return IB_LINK_SPEED_50;
    if (!strcmp(s, IB_LINK_SPEED_100_STR))    return IB_LINK_SPEED_100;
    if (!strcmp(s, IB_LINK_SPEED_FDR_10_STR)) return IB_LINK_SPEED_FDR_10;
    if (!strcmp(s, IB_LINK_SPEED_EDR_20_STR)) return IB_LINK_SPEED_EDR_20;
    if (!strcmp(s, IB_LINK_SPEED_200_STR))    return IB_LINK_SPEED_200;
    return IB_UNKNOWN_LINK_SPEED;
};

static inline string speed_to_str(const IBLinkSpeed w) {
    return string(speed2char(w));
}

#define IB_LINK_SPEED_2_5_NAME_STR     "SDR"
#define IB_LINK_SPEED_5_NAME_STR       "DDR"
#define IB_LINK_SPEED_10_NAME_STR      "QDR"
#define IB_LINK_SPEED_14_NAME_STR      "FDR"
#define IB_LINK_SPEED_25_NAME_STR      "EDR"
#define IB_LINK_SPEED_50_NAME_STR      "HDR"
#define IB_LINK_SPEED_100_NAME_STR     "NDR"
#define IB_LINK_SPEED_FDR_10_NAME_STR  "FDR_10"
#define IB_LINK_SPEED_EDR_20_NAME_STR  "EDR_20"
#define IB_LINK_SPEED_200_NAME_STR     "XDR"


static inline IBLinkSpeedIndex speed2speed_index(const IBLinkSpeed speed)
{
    switch (speed) {
    case IB_LINK_SPEED_2_5:     return IB_LINK_SPEED_2_5_INDEX;
    case IB_LINK_SPEED_5:       return IB_LINK_SPEED_5_INDEX;
    case IB_LINK_SPEED_10:      return IB_LINK_SPEED_10_INDEX;
    case IB_LINK_SPEED_14:      return IB_LINK_SPEED_14_INDEX;
    case IB_LINK_SPEED_25:      return IB_LINK_SPEED_25_INDEX;
    case IB_LINK_SPEED_50:      return IB_LINK_SPEED_50_INDEX;
    case IB_LINK_SPEED_100:     return IB_LINK_SPEED_100_INDEX;
    case IB_LINK_SPEED_FDR_10:  return IB_LINK_SPEED_FDR_10_INDEX;
    case IB_LINK_SPEED_EDR_20:  return IB_LINK_SPEED_EDR_20_INDEX;
    case IB_LINK_SPEED_200:     return IB_LINK_SPEED_200_INDEX;
    default:                    return IB_UNKNOWN_LINK_SPEED_INDEX;
    }
};

static inline IBLinkSpeed speed_index2speed(const IBLinkSpeedIndex speed_index)
{
    switch (speed_index) {
    case IB_LINK_SPEED_2_5_INDEX:     return IB_LINK_SPEED_2_5;
    case IB_LINK_SPEED_5_INDEX:       return IB_LINK_SPEED_5;
    case IB_LINK_SPEED_10_INDEX:      return IB_LINK_SPEED_10;
    case IB_LINK_SPEED_14_INDEX:      return IB_LINK_SPEED_14;
    case IB_LINK_SPEED_25_INDEX:      return IB_LINK_SPEED_25;
    case IB_LINK_SPEED_50_INDEX:      return IB_LINK_SPEED_50;
    case IB_LINK_SPEED_100_INDEX:     return IB_LINK_SPEED_100;
    case IB_LINK_SPEED_FDR_10_INDEX:  return IB_LINK_SPEED_FDR_10;
    case IB_LINK_SPEED_EDR_20_INDEX:  return IB_LINK_SPEED_EDR_20;
    case IB_LINK_SPEED_200_INDEX:     return IB_LINK_SPEED_200;
    default:                          return IB_UNKNOWN_LINK_SPEED;
    }
};

static inline IBLinkSpeed extspeed2speed(uint8_t ext_speed)
{
    switch (ext_speed) {
    case 1 /* IB_LINK_SPEED_14  >> 8 */: return IB_LINK_SPEED_14;
    case 2 /* IB_LINK_SPEED_25  >> 8 */: return IB_LINK_SPEED_25;
    case 4 /* IB_LINK_SPEED_50  >> 8 */: return IB_LINK_SPEED_50;
    case 8 /* IB_LINK_SPEED_100 >> 8 */: return IB_LINK_SPEED_100;
    default:                             return IB_UNKNOWN_LINK_SPEED;
    }
};

static inline IBLinkSpeed extspeed2_2_speed(uint8_t ext_speed)
{
    switch (ext_speed) {
    case 1 /* IB_LINK_SPEED_200  >> 24 */: return IB_LINK_SPEED_200;
    default:                               return IB_UNKNOWN_LINK_SPEED;
    }
};

static inline IBLinkSpeed mlnxspeed2speed(uint8_t mlnx_speed)
{
    switch (mlnx_speed) {
    case 1 /* IB_LINK_SPEED_FDR_10 >> 16 */:    return IB_LINK_SPEED_FDR_10;
    case 2 /* IB_LINK_SPEED_EDR_20 >> 16 */:    return IB_LINK_SPEED_EDR_20;
    default:                                    return IB_UNKNOWN_LINK_SPEED;
    }
};

//enum for retransmission mode
typedef enum {
    RETRANS_NO_RETRANS          = 0,
    RETRANS_LLR_ACTIVE_CELL_64  = 1,
    RETRANS_LLR_ACTIVE_CELL_128 = 2,
    RETRANS_PLR                 = 3
} EnRetransmissionMode;

static inline unsigned int llr_active_cell2size(u_int8_t active_cell)
{
    switch (active_cell) {
    case RETRANS_LLR_ACTIVE_CELL_64:
        return 64;
    case RETRANS_LLR_ACTIVE_CELL_128:
        return 128;
    case RETRANS_NO_RETRANS: default:
        return 0;
    }
};

#define RETRANS_MODE_NO_RETRANS_STR "NO-RTR"
#define RETRANS_MODE_LLR64_STR      "LLR64"
#define RETRANS_MODE_LLR128_STR     "LLR128"
#define RETRANS_MODE_PLR_STR        "PLR"
#define RETRANS_MODE_NA             "N/A"

static inline const char * retransmission2char(const EnRetransmissionMode r)
{
    switch (r) {
    case RETRANS_NO_RETRANS:
        return (RETRANS_MODE_NO_RETRANS_STR);
    case RETRANS_LLR_ACTIVE_CELL_64:
        return (RETRANS_MODE_LLR64_STR);
    case RETRANS_LLR_ACTIVE_CELL_128:
        return(RETRANS_MODE_LLR128_STR);
    case RETRANS_PLR:
        return(RETRANS_MODE_PLR_STR);
    default:
        return(RETRANS_MODE_NA);
    }
};

typedef enum {IB_UNKNOWN_PORT_STATE = 0,
              IB_PORT_STATE_DOWN = 1,
              IB_PORT_STATE_INIT = 2,
              IB_PORT_STATE_ARM = 3,
              IB_PORT_STATE_ACTIVE = 4
} IBPortState;

enum IBSpecialPortType {
    IB_SPECIAL_PORT_AN               =  1,
    IB_SPECIAL_PORT_ROUTER           =  2,
    IB_SPECIAL_PORT_ETH_GATEWAY      =  3,
    IB_NOT_SPECIAL_PORT              =  0xff
};

static inline IBPortState char2portstate(const char *w)
{
    if (!w || (*w == '\0')) return IB_UNKNOWN_PORT_STATE;
    if (!strcmp(w,"DOWN"))  return IB_PORT_STATE_DOWN;
    if (!strcmp(w,"INI"))   return IB_PORT_STATE_INIT;
    if (!strcmp(w,"ARM"))   return IB_PORT_STATE_ARM;
    if (!strcmp(w,"ACT"))   return IB_PORT_STATE_ACTIVE;
    return IB_UNKNOWN_PORT_STATE;
};

static inline const char * portstate2char(const IBPortState w)
{
    switch (w)
    {
    case IB_PORT_STATE_DOWN:    return("DOWN");
    case IB_PORT_STATE_INIT:    return("INI");
    case IB_PORT_STATE_ARM:     return("ARM");
    case IB_PORT_STATE_ACTIVE:  return("ACT");
    default:                    return("UNKNOWN");
    }
};

static inline string port_state_to_str(const IBPortState s) {
    return string(portstate2char(s));
}

typedef enum {
    IB_PORT_PHYS_STATE_UNKNOWN           = 0,
    /* this state same as UNKNOWN, added for compatability with IBMGTSim */
    IB_PORT_PHYS_STATE_NO_CHANGE         = IB_PORT_PHYS_STATE_UNKNOWN,
    IB_PORT_PHYS_STATE_SLEEP             = 1,
    IB_PORT_PHYS_STATE_POLLING           = 2,
    IB_PORT_PHYS_STATE_DISABLED          = 3,
    IB_PORT_PHYS_STATE_PORT_CONF_TRAIN   = 4,
    IB_PORT_PHYS_STATE_LINK_UP           = 5,
    IB_PORT_PHYS_STATE_LINK_ERR_RECOVERY = 6,
    IB_PORT_PHYS_STATE_PHY_TEST          = 7
    /* 8-15 reserved */
} IBPortPhysState;

static inline const char * portphysstate2char(const IBPortPhysState w)
{
    switch (w)
    {
    case IB_PORT_PHYS_STATE_SLEEP:             return("SLEEP");
    case IB_PORT_PHYS_STATE_POLLING:           return("POLL");
    case IB_PORT_PHYS_STATE_DISABLED:          return("DISABLE");
    case IB_PORT_PHYS_STATE_PORT_CONF_TRAIN:   return("PORT CONF TRAIN");
    case IB_PORT_PHYS_STATE_LINK_UP:           return("LINK UP");
    case IB_PORT_PHYS_STATE_LINK_ERR_RECOVERY: return("LINK ERR RECOVER");
    case IB_PORT_PHYS_STATE_PHY_TEST:          return("PHY TEST");
    case IB_PORT_PHYS_STATE_UNKNOWN: default:  return("UNKNOWN");
    }
};

static inline string guid2str(uint64_t guid) {
    char buffer[19];
    snprintf(buffer, sizeof(buffer), "0x%016" PRIx64 , guid);
    return buffer;
};


const char* speed2char_name(IBLinkSpeed s, const char* out_name = "UNKNOWN");
IBLinkSpeed char_name2speed(const char *str);
const char* nodetype2char_capital(const IBNodeType type);
const char* nodetype2char_low(const IBNodeType type);
const char* nodetype2char_short(const IBNodeType type);

//Represents the available FEC Modes, that are presented
//in Mlnx Extended Port Info FECModeActive field
typedef enum {
    IB_FEC_NO_FEC               = 0,
    IB_FEC_FIRECODE_FEC         = 1,
    IB_FEC_RS_FEC               = 2,
    IB_FEC_LL_RS_FEC            = 3,
    IB_FEC_RS_FEC_544_514       = 4,
    //5-7 Reserved
    IB_FEC_MLNX_STRONG_RS_FEC   = 8,
    IB_FEC_MLNX_LL_RS_FEC       = 9,
    IB_FEC_MLNX_ADAPTIVE_RS_FEC = 10,
    IB_FEC_MLNX_COD_FEC         = 11,
    IB_FEC_MLNX_ZL_FEC          = 12,
    IB_FEC_MLNX_RS_544_514_PLR  = 13,
    IB_FEC_MLNX_RS_271_257_PLR  = 14,

    //15 Reserved
    IB_FEC_NA                   = 0xff
} IBFECMode;

#define IB_FEC_NO_FEC_STR                "NO-FEC"
#define IB_FEC_FIRECODE_FEC_STR          "FIRECODE"
#define IB_FEC_RS_FEC_STR                "STD-RS"
#define IB_FEC_LL_RS_FEC_STR             "STD-LL-RS"
#define IB_FEC_RS_FEC_544_514_STR        "RS_FEC_544_514"

#define IB_FEC_MLNX_STRONG_RS_FEC_STR    "MLNX-STRONG-RS"
#define IB_FEC_MLNX_LL_RS_FEC_STR        "MLNX-LL-RS"
#define IB_FEC_MLNX_ADAPTIVE_RS_FEC_STR  "MLNX-ADAPT-RS"
#define IB_FEC_MLNX_COD_FEC_STR          "MLNX-COD-FEC"
#define IB_FEC_MLNX_ZL_FEC_STR           "MLNX-ZL-FEC"
#define IB_FEC_MLNX_RS_544_514_PLR_STR   "MLNX_RS_544_514_PLR"
#define IB_FEC_MLNX_RS_271_257_PLR_STR   "MLNX_RS_271_257_PLR"

#define IB_FEC_NA_STR                    "N/A"

static inline const char * fec2char(const IBFECMode f)
{
    switch (f) {
    case IB_FEC_NO_FEC:               return(IB_FEC_NO_FEC_STR);
    case IB_FEC_FIRECODE_FEC:         return(IB_FEC_FIRECODE_FEC_STR);
    case IB_FEC_RS_FEC:               return(IB_FEC_RS_FEC_STR);
    case IB_FEC_LL_RS_FEC:            return(IB_FEC_LL_RS_FEC_STR);
    case IB_FEC_RS_FEC_544_514:       return(IB_FEC_RS_FEC_544_514_STR);

    case IB_FEC_MLNX_STRONG_RS_FEC:   return(IB_FEC_MLNX_STRONG_RS_FEC_STR);
    case IB_FEC_MLNX_LL_RS_FEC:       return(IB_FEC_MLNX_LL_RS_FEC_STR);
    case IB_FEC_MLNX_ADAPTIVE_RS_FEC: return(IB_FEC_MLNX_ADAPTIVE_RS_FEC_STR);
    case IB_FEC_MLNX_COD_FEC:         return(IB_FEC_MLNX_COD_FEC_STR);
    case IB_FEC_MLNX_ZL_FEC:          return(IB_FEC_MLNX_ZL_FEC_STR);
    case IB_FEC_MLNX_RS_544_514_PLR:  return(IB_FEC_MLNX_RS_544_514_PLR_STR);
    case IB_FEC_MLNX_RS_271_257_PLR:  return(IB_FEC_MLNX_RS_271_257_PLR_STR);

    case IB_FEC_NA: default:          return(IB_FEC_NA_STR);
    }
};

static inline IBFECMode char2fec(const char *str)
{
    if (!str || !*str)
        return IB_FEC_NA;

    char tail[128] = {0};
    int val = 0;
    if (sscanf(str ,"%d %s", &val, tail) != 1)
        return IB_FEC_NA;

    if ((val >= IB_FEC_NO_FEC && val <= IB_FEC_RS_FEC_544_514) ||
         (val >= IB_FEC_MLNX_STRONG_RS_FEC && val <= IB_FEC_MLNX_RS_271_257_PLR))
        return static_cast<IBFECMode>(val);

    return IB_FEC_NA;
}

static inline IBFECMode fec_mask2value(u_int16_t mask)
{
    switch (mask) {
    case 0:     return(IB_FEC_NO_FEC);
    case 1:     return(IB_FEC_FIRECODE_FEC);
    case 2:     return(IB_FEC_RS_FEC);
    case 3:     return(IB_FEC_LL_RS_FEC);
    default:    return(IB_FEC_NA);
    }
}

//return true for any type of RS-FEC fec mode
static inline bool isRSFEC(const IBFECMode f)
{
    switch (f) {
    case IB_FEC_RS_FEC:
    case IB_FEC_LL_RS_FEC:
    case IB_FEC_RS_FEC_544_514:

    case IB_FEC_MLNX_STRONG_RS_FEC:
    case IB_FEC_MLNX_LL_RS_FEC:
    case IB_FEC_MLNX_ADAPTIVE_RS_FEC:
    case IB_FEC_MLNX_COD_FEC:
    case IB_FEC_MLNX_RS_544_514_PLR:
    case IB_FEC_MLNX_RS_271_257_PLR:
        return (true);

    case IB_FEC_NO_FEC:
    case IB_FEC_FIRECODE_FEC:
    case IB_FEC_MLNX_ZL_FEC:
    case IB_FEC_NA:
    default:
        return (false);
    }
}

enum IBBERType {
    IB_RAW_BER = 0,
    IB_EFFECTIVE_BER,
    IB_SYMBOL_BER,
    NUM_OF_BER_TYPES
};

#define IB_RAW_BER_STR                "Raw BER"
#define IB_EFFECTIVE_BER_STR          "Effective BER"
#define IB_SYMBOL_BER_STR             "Symbol BER"

#define IB_RAW_BER_SHORT_STR          "RAW"
#define IB_EFFECTIVE_BER_SHORT_STR    "EFF"
#define IB_SYMBOL_BER_SHORT_STR       "SYM"

#define IB_BER_NA_STR                 "N/A"

static inline const char* ber_type2char(const IBBERType ber_type)
{
    switch (ber_type) {
    case IB_RAW_BER:               return(IB_RAW_BER_STR);
    case IB_EFFECTIVE_BER:         return(IB_EFFECTIVE_BER_STR);
    case IB_SYMBOL_BER:            return(IB_SYMBOL_BER_STR);
    default:                       return(IB_FEC_NA_STR);
    }
};

static inline const char* ber_type2short_char(const IBBERType ber_type)
{
    switch (ber_type) {
    case IB_RAW_BER:               return(IB_RAW_BER_SHORT_STR);
    case IB_EFFECTIVE_BER:         return(IB_EFFECTIVE_BER_SHORT_STR);
    case IB_SYMBOL_BER:            return(IB_SYMBOL_BER_SHORT_STR);
    default:                       return(IB_FEC_NA_STR);
    }
};

static inline IBBERType short_char2ber_type(const char *ber_type)
{
    if (!strcmp(ber_type,IB_RAW_BER_SHORT_STR))          return(IB_RAW_BER);
    if (!strcmp(ber_type,IB_EFFECTIVE_BER_SHORT_STR))    return(IB_EFFECTIVE_BER);
    if (!strcmp(ber_type,IB_SYMBOL_BER_SHORT_STR))       return(IB_SYMBOL_BER);

    return(NUM_OF_BER_TYPES);
};

enum IBRoutingEngine {
    IB_ROUTING_MINHOP,
    IB_ROUTING_UPDN,
    IB_ROUTING_DNUP,
    IB_ROUTING_FILE,
    IB_ROUTING_FTREE,
    IB_ROUTING_PQFT,
    IB_ROUTING_LASH,
    IB_ROUTING_DOR,
    IB_ROUTING_TORUS_2QOS,
    IB_ROUTING_DFSSSP,
    IB_ROUTING_SSSP,
    IB_ROUTING_CHAIN,
    IB_ROUTING_DFP,
    IB_ROUTING_AR_DOR,
    IB_ROUTING_AR_MINHOP,
    IB_ROUTING_AR_UPDN,
    IB_ROUTING_AR_FTREE,
    IB_ROUTING_AR_TORUS,
    IB_ROUTING_KDOR_HC,
    IB_ROUTING_UNKNOWN
};

#define IB_ROUTING_MINHOP_STR             "minhop"
#define IB_ROUTING_UPDN_STR               "updn"
#define IB_ROUTING_DNUP_STR               "dnup"
#define IB_ROUTING_FILE_STR               "file"
#define IB_ROUTING_FTREE_STR              "ftree"
#define IB_ROUTING_PQFT_STR               "pqft"
#define IB_ROUTING_LASH_STR               "lash"
#define IB_ROUTING_DOR_STR                "dor"
#define IB_ROUTING_TORUS_2QOS_STR         "torus-2QoS"
#define IB_ROUTING_DFSSSP_STR             "dfsssp"
#define IB_ROUTING_SSSP_STR               "sssp"
#define IB_ROUTING_CHAIN_STR              "chain"
#define IB_ROUTING_DFP_STR                "dfp"
#define IB_ROUTING_AR_DOR_STR             "ar_dor"
#define IB_ROUTING_AR_MINHOP_STR          "ar_minhop"
#define IB_ROUTING_AR_UPDN_STR            "ar_updn"
#define IB_ROUTING_AR_FTREE_STR           "ar_ftree"
#define IB_ROUTING_AR_TORUS_STR           "ar_torus"
#define IB_ROUTING_KDOR_HC_STR            "kdor-hc"
#define IB_ROUTING_UNKNOWN_STR            "Unknown"

static inline const char * routing_engine2char(const IBRoutingEngine routing_engine) {
    switch (routing_engine) {
        case IB_ROUTING_MINHOP:            return IB_ROUTING_MINHOP_STR;
        case IB_ROUTING_UPDN:              return IB_ROUTING_UPDN_STR;
        case IB_ROUTING_DNUP:              return IB_ROUTING_DNUP_STR;
        case IB_ROUTING_FILE:              return IB_ROUTING_FILE_STR;
        case IB_ROUTING_FTREE:             return IB_ROUTING_FTREE_STR;
        case IB_ROUTING_PQFT:              return IB_ROUTING_PQFT_STR;
        case IB_ROUTING_LASH:              return IB_ROUTING_LASH_STR;
        case IB_ROUTING_DOR:               return IB_ROUTING_DOR_STR;
        case IB_ROUTING_TORUS_2QOS:        return IB_ROUTING_TORUS_2QOS_STR;
        case IB_ROUTING_DFSSSP:            return IB_ROUTING_DFSSSP_STR;
        case IB_ROUTING_SSSP:              return IB_ROUTING_SSSP_STR;
        case IB_ROUTING_CHAIN:             return IB_ROUTING_CHAIN_STR;
        case IB_ROUTING_DFP:               return IB_ROUTING_DFP_STR;
        case IB_ROUTING_AR_DOR:            return IB_ROUTING_AR_DOR_STR;
        case IB_ROUTING_AR_UPDN:           return IB_ROUTING_AR_UPDN_STR;
        case IB_ROUTING_AR_FTREE:          return IB_ROUTING_AR_FTREE_STR;
        case IB_ROUTING_AR_TORUS:          return IB_ROUTING_AR_TORUS_STR;
        case IB_ROUTING_KDOR_HC:           return IB_ROUTING_KDOR_HC_STR;
        case IB_ROUTING_UNKNOWN:
        default: return IB_ROUTING_UNKNOWN_STR;
    }
}

static inline IBRoutingEngine char2routing_engine(const char *routing_engine) {
    if (!strcmp(routing_engine, IB_ROUTING_MINHOP_STR))          return IB_ROUTING_MINHOP;
    if (!strcmp(routing_engine, IB_ROUTING_UPDN_STR))            return IB_ROUTING_UPDN;
    if (!strcmp(routing_engine, IB_ROUTING_DNUP_STR))            return IB_ROUTING_DNUP;
    if (!strcmp(routing_engine, IB_ROUTING_FILE_STR))            return IB_ROUTING_FILE;
    if (!strcmp(routing_engine, IB_ROUTING_FTREE_STR))           return IB_ROUTING_FTREE;
    if (!strcmp(routing_engine, IB_ROUTING_PQFT_STR))            return IB_ROUTING_PQFT;
    if (!strcmp(routing_engine, IB_ROUTING_LASH_STR))            return IB_ROUTING_LASH;
    if (!strcmp(routing_engine, IB_ROUTING_DOR_STR))             return IB_ROUTING_DOR;
    if (!strcmp(routing_engine, IB_ROUTING_TORUS_2QOS_STR))      return IB_ROUTING_TORUS_2QOS;
    if (!strcmp(routing_engine, IB_ROUTING_DFSSSP_STR))          return IB_ROUTING_DFSSSP;
    if (!strcmp(routing_engine, IB_ROUTING_SSSP_STR))            return IB_ROUTING_SSSP;
    if (!strcmp(routing_engine, IB_ROUTING_CHAIN_STR))           return IB_ROUTING_CHAIN;
    if (!strcmp(routing_engine, IB_ROUTING_DFP_STR))             return IB_ROUTING_DFP;
    if (!strcmp(routing_engine, IB_ROUTING_AR_DOR_STR))          return IB_ROUTING_AR_DOR;
    if (!strcmp(routing_engine, IB_ROUTING_AR_MINHOP_STR))       return IB_ROUTING_AR_MINHOP;
    if (!strcmp(routing_engine, IB_ROUTING_AR_UPDN_STR))         return IB_ROUTING_AR_UPDN;
    if (!strcmp(routing_engine, IB_ROUTING_AR_FTREE_STR))        return IB_ROUTING_AR_FTREE;
    if (!strcmp(routing_engine, IB_ROUTING_AR_TORUS_STR))        return IB_ROUTING_AR_TORUS;
    if (!strcmp(routing_engine, IB_ROUTING_KDOR_HC_STR))         return IB_ROUTING_KDOR_HC;
    return IB_ROUTING_UNKNOWN;
}

static inline bool IsPortStateDown(IBPortState s) {
    switch (s) {
        case IB_PORT_STATE_INIT:
        case IB_PORT_STATE_ARM:
        case IB_PORT_STATE_ACTIVE:
            return false;
        case IB_PORT_STATE_DOWN:
        // unknown stage is consider as down
        case IB_UNKNOWN_PORT_STATE:
        default:
            return true;
    }
}

static inline bool IsValidUcastLid(lid_t lid)
{
    return (lid && lid <= IB_MAX_UCAST_LID);
}

///////////////////////////////////////////////////////////////////////////////
//
// Multicast SA Info
//
class McastGroupInfo;
class McastGroupMemberInfo;
typedef map< lid_t, class McastGroupInfo > map_mcast_groups;
typedef pair< map_mcast_groups::iterator, bool > McastGroupMapInsertRes;
typedef map< IBPort *, class McastGroupMemberInfo > map_mcast_members;
typedef pair< map_mcast_members::iterator, bool > McastMemberMapInsertRes;

class McastGroupInfo {
public:
    map_mcast_members m_members;
};

// We may have several entries(reflected in the opensm-sa.dump file)
// of the same port, for the same mlid, with different is_sender_only
// flag and sl.
// We save a global is_sender_only flag (not per sl) which is the
// sum of all is_sender_only flags
class McastGroupMemberInfo {
public:
    McastGroupMemberInfo() : is_sender_only(false) {}
    set_uint8 m_sls;
    bool is_sender_only;
};

class PCI_Index {
public:
    u_int8_t pcie_index;
    u_int8_t depth;
    u_int8_t node;

    PCI_Index(u_int8_t inIndex = -1, u_int8_t inDepth = -1, u_int8_t inNode = -1):
        pcie_index(inIndex), depth(inDepth), node(inNode) {}

    bool operator< (const PCI_Index &other) const {
        if (this == &other)
            return false;

        // Compare first members
        //
        if (pcie_index < other.pcie_index)
            return true;
        else if (other.pcie_index < pcie_index)
            return false;

        // First members are equal, compare second members
        //
        if (depth < other.depth)
            return true;
        else if (other.depth < depth)
            return false;

        // Second members are equal, compare third members
        //
        if (node < other.node)
            return true;
        else if (other.node < node)
            return false;

        // All members are equal, so return false
        return false;
    }
};

class PCI_Address {
public:
    u_int8_t bus;
    u_int8_t function;
    u_int8_t device;
    bool     isValid;

    PCI_Address(): bus(-1), function(-1), device(-1), isValid(false) {}
    PCI_Address (u_int8_t inBus, u_int8_t inPort, u_int8_t inDevice):
       bus(inBus), function(inPort), device(inDevice), isValid(true){}

    bool IsValid() const { return isValid; }

    bool operator< (const PCI_Address& other) const {
        if (this == &other)   // Short?cut self?comparison
        return false;

    // Compare first members
    //
    if (bus < other.bus)
        return true;
    else if (other.bus < bus)
        return false;

    // First members are equal, compare second members
    //
    if (function < other.function)
        return true;
    else if (other.function < function)
        return false;

    // Second members are equal, compare third members
    //
    if (device < other.device)
        return true;
    else if (other.device < device)
        return false;

    // All members are equal, so return false
    return false;
    }
};

typedef map < PCI_Address, vector<IBNode*> > PCI_AddressToNodesMap;
typedef map < PCI_Index, PCI_Address > PCI_AddressMap;

enum PCI_BDF_SOURCE {
    PCI_BDF_SOURCE_AUTO,
    PCI_BDF_SOURCE_HI,
    PCI_BDF_SOURCE_PHY
};

class PCI_LeafSwitchInfo {
public:
    IBNode*                 p_switch;
    PCI_AddressToNodesMap   pciAddressMap;
    PCI_BDF_SOURCE          source;

    PCI_LeafSwitchInfo(): p_switch(NULL), source(PCI_BDF_SOURCE_AUTO) {}
};

///////////////////////////////////////////////////////////////////////////////
//
// Virtual Channel class
// Used for credit loops verification
//
class VChannel;

class CrdRoute {
public:
    VChannel *m_pvch;
    lid_t m_slid;
    lid_t m_dlid;
    //algorithm optimization
    //if dlid is same as last dlid and sl is same as last sl
    //no need to continue checking this flow
    lid_t m_lastDlid;
    u_int16_t m_lastSLs;

    // Constructor
    CrdRoute():m_pvch(NULL), m_slid(0), m_dlid(0),
        m_lastDlid(0),m_lastSLs(0) {}
    CrdRoute(VChannel *pvch, lid_t slid, lid_t dlid):
        m_pvch(pvch), m_slid(slid), m_dlid(dlid),
        m_lastDlid(0), m_lastSLs(0){}
    CrdRoute(VChannel *pvch, lid_t slid, lid_t dlid, u_int8_t sl):
        m_pvch(pvch), m_slid(slid), m_dlid(dlid),
        m_lastDlid(dlid), m_lastSLs((u_int16_t)(1 << sl)){}

};

class VChannel {
    //vec_pvch depend;    // Vector of dependencies
    vec_crd_route depend;    // Vector of dependencies
    dfs_t    flag;      // DFS state
public:
    IBPort  *pPort;
    int vl;

    // Constructor
    VChannel(IBPort * p, int v) {
        flag = Untouched;
        pPort = p;
        vl = v;
    };

    //Getters/Setters
    inline void setDependSize(int numDepend) {
        if (depend.size() != (unsigned)numDepend) {
            depend.resize(numDepend);
            /*
            for (int i=0;i<numDepend;i++) {
                depend[i] = NULL;
            }
            */
        }
    };
    inline int getDependSize() const {
        return (int)depend.size();
    };

    inline dfs_t getFlag() const {
        return flag;
    };
    inline void setFlag(dfs_t f) {
        flag = f;
    };

    inline void setDependency(int i,VChannel* p, lid_t slid, lid_t dlid) {
        if(depend[i].m_pvch == NULL)
            depend[i]=CrdRoute(p,slid,dlid);
    };

    //return 0 if add new Dependency
    //return 1 if set new dlid or new sl
    //return 2 if nothing was changed
    inline int setDependency(int i,VChannel* p, lid_t slid, lid_t dlid,
                              u_int8_t sl) {
        if(depend[i].m_pvch == NULL){
            depend[i]=CrdRoute(p,slid,dlid, sl);
            return 0;
        }

        u_int16_t slMask = (u_int16_t)(1 << sl);

        if (depend[i].m_lastDlid != dlid) {
            depend[i].m_lastDlid = dlid;
            depend[i].m_lastSLs = slMask;
            return 1;
        }

        if (!(depend[i].m_lastSLs | slMask)) {
            depend[i].m_lastSLs =
                (u_int16_t)(depend[i].m_lastSLs | slMask);
            return 1;
        }

        return 2;
    };
    inline CrdRoute & getDependency(int i) {
        return depend[i];
    };
    inline VChannel* getDependencyPvch(int i) {
        return depend[i].m_pvch;
    };
};


enum IBKeyType {
    IB_AM_KEY
};

template <int key_type>
class IBKey {
private:
    u_int64_t key;
    u_int64_t *p_key;

public:
    IBKey(u_int64_t key = *(IBKey::GetDefaultKey())) {
        this->SetKey(key);
    };

    ~IBKey() {};

    static u_int64_t SetDefaultKey(u_int64_t default_key) {
        return *(IBKey::GetDefaultKey()) = default_key;
    };

    static u_int64_t* GetDefaultKey() {
        static u_int64_t default_key = DEFAULT_KEY;
        return &default_key;
    };

    inline u_int64_t SetKey(u_int64_t key) {
        this->p_key = &this->key;
        return this->key = key;
    };

    inline u_int64_t GetKey() const {
        return *this->p_key;
    };

    inline u_int64_t UnSetKey() {
        this->p_key = IBKey::GetDefaultKey();
        return *this->p_key;
    };
};

class CombinedCableInfo;
class PrtlRecord;

///////////////////////////////////////////////////////////////////////////////
//
// Hierarchy Info classes
//
class PortHierarchyInfo
{
    public:
        union bdf_t {
            struct {
                unsigned int function : 3;
                unsigned int device : 5;
                unsigned int bus : 8;
                unsigned int : 16;
            };
            int value;

            bdf_t(int _value) {
                value = _value;
            }

            bdf_t(int _bus, int _device, int _function) {
                value = -1; // Fill reserved field with 1s
                bus = _bus & 0xFF;
                device = _device & 0x1F;
                function = _function & 0x7;
            }
        };

        uint64_t m_template_guid;

        int m_port_type = -1;
        int m_asic_name = -1;
        int m_ibport = -1;
        int m_type = -1;
        int m_slot_type = -1;
        int m_slot_value = -1;
        int m_asic = -1;
        int m_cage = -1;
        int m_port = -1;
        int m_split = -1;
        int m_is_cage_manager = -1;
        int m_number_on_base_board = -1;

        // DeviceRecord
        int m_device_num_on_cpu_node = -1;
        int m_cpu_node_number = -1;

        // BoardRecord
        int m_board_type = -1;
        int m_chassis_slot_index = -1;
        int m_tray_index = -1;

        int m_topology_id = -1;

        // APort
        int m_aport = -1;
        int m_plane = -1;
        int m_num_of_planes = -1;

        bdf_t m_bdf = -1;

    private:
        std::string m_label;
        std::string m_extended_label;

        void set_slot(int unparsed_value) {
            if (unparsed_value == -1) {
                m_slot_type = -1;
                m_slot_value = -1;
            } else {
                m_slot_type = (unparsed_value) & 0x03;
                m_slot_value = ((unparsed_value) >> 8) & 0xFFFF;
            }
        }

    public:
        int bdf() const {
            return m_bdf.value;
        }

        int bus() const {
            return m_bdf.value != -1 ? m_bdf.bus : -1;
        }

        int device() const {
            return m_bdf.value != -1 ? m_bdf.device : -1;
        }

        int function() const {
            return m_bdf.value != -1 ? m_bdf.function : -1;
        }

        const std::string& label() const {
            return m_label;
        }

        const std::string& set_label(const std::string& label) {
            return m_label = label;
        }

        const std::string& extendedLabel() const {
            return m_extended_label;
        }

        const std::string& set_extendedLabel(const std::string& label) {
            return m_extended_label = label;
        }
    public:
        PortHierarchyInfo(uint64_t template_guid ) : m_template_guid(template_guid) {};
        PortHierarchyInfo(IBNodeType node_type, uint64_t template_guid,
                          int port_type, int asic_name, int ibport, int is_cage_manager,
                          int m_number_on_base_board, int aport, int plane, int num_of_planes,
                          bdf_t bdf, int type, int slot_type, int slot_value, 
                          int asic, int cage, int port, int split,
                          int device_num_on_cpu_node, int cpu_node_number,
                          int board_type, int chassis_slot_index, int tray_index,
                          int topology_id)
            : m_template_guid(template_guid),
              m_port_type(port_type),
              m_asic_name(asic_name),
              m_ibport(ibport),
              m_type(type),
              m_slot_type(slot_type),
              m_slot_value(slot_value),
              m_asic(asic),
              m_cage(cage),
              m_port(port),
              m_split(split),
              m_is_cage_manager(is_cage_manager),
              m_device_num_on_cpu_node(device_num_on_cpu_node),
              m_cpu_node_number(cpu_node_number),
              m_board_type(board_type),
              m_chassis_slot_index(chassis_slot_index),
              m_tray_index(tray_index),
              m_topology_id(topology_id),
              m_aport(aport),
              m_plane(plane),
              m_num_of_planes(num_of_planes),
              m_bdf(bdf)
        {
            createLabel(node_type);
        }

        PortHierarchyInfo(IBNodeType node_type, const vector<int>& hierarchy_data,
                          uint64_t template_guid) {

            m_template_guid = template_guid;
            switch (template_guid) {
                case HIERARCHY_TEMPLATE_GUID_PORT_0x03:
                    SetHierarchyInfoFrom0x03TemplateGUID(hierarchy_data);
                    break;
                case HIERARCHY_TEMPLATE_GUID_PORT_0x04:
                    SetHierarchyInfoFrom0x04TemplateGUID(hierarchy_data);
                    break;
                case HIERARCHY_TEMPLATE_GUID_PORT_0x05:
                    SetHierarchyInfoFrom0x05TemplateGUID(hierarchy_data);
                    break;
                default:
                    m_template_guid = 0;
                    cout << "Error Creating PortHierarchyInfo with Template GUID: "
                     << template_guid << endl;
            }

            if (m_template_guid != 0)
                createLabel(node_type);
        }

        bool IsFNMPort() const
        {
            return  (m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x04 &&
                     m_port_type == HIERARCHY_INFO_PORT_TYPE_FNM);
        }

        bool IsFNM1Port() const
        {
            return  (m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x04 &&
                     m_port_type == HIERARCHY_INFO_PORT_TYPE_FNM1);
        }

    private:
        void SetHierarchyInfoFrom0x03TemplateGUID(const vector<int>& hierarchy_data) {
            // 0 - Split; 1 - Port; 2 - Cage; 3 - ASIC; 4 - Slot; 5 - Type; 6 - BDF; 
            m_split     = hierarchy_data[0];
            m_port      = hierarchy_data[1];
            m_cage      = hierarchy_data[2];
            m_asic      = hierarchy_data[3];
            set_slot(hierarchy_data[4]);
            m_type      = hierarchy_data[5];
            m_bdf       = bdf_t(hierarchy_data[6]);
        }

        void SetHierarchyInfoFrom0x04TemplateGUID(const vector<int>& hierarchy_data) {
            // 0 - PortType; 1 - AsicName; 2 - IBport; 3 - SwitchCage; 4 - Port (IPIL);
            // 5 - Split; 6 - Asic; 7 - RESERVED; 8 - Type; 9 - IsCageManager; 10 - Plane;
            // 11 - NumoPlanes; 12 - APort
            m_port_type         = hierarchy_data[0];
            m_asic_name         = hierarchy_data[1];
            m_ibport            = hierarchy_data[2];
            m_cage              = hierarchy_data[3];
            m_port              = hierarchy_data[4];
            m_split             = hierarchy_data[5];
            m_asic              = hierarchy_data[6];
            m_type              = hierarchy_data[8];
            m_is_cage_manager   = hierarchy_data[9];
            m_plane             = hierarchy_data[10];
            m_num_of_planes     = hierarchy_data[11];
            m_aport             = hierarchy_data[12];
        }

        void SetHierarchyInfoFrom0x05TemplateGUID(const vector<int>& hierarchy_data) {
            // 0 - PortType; 1 - NumberOnBaseBoard; 2 - IBport; 3 - SwitchCage; 4 - Port (IPIL);
            // 5 - Split; 6 - RESERVED; 7 - RESERVED; 8 - RESERVED;
            // 9 - BDF; 10 - Plane; 11 - NumoPlanes; 12 - APort
            // 13 - DeviceRecord; 14 - BoardRecord; 15 - SystemRecord
            m_port_type              = hierarchy_data[0];
            m_ibport                 = hierarchy_data[2];
            m_cage                   = hierarchy_data[3];
            m_port                   = hierarchy_data[4];
            m_split                  = hierarchy_data[5];
            m_bdf                    = hierarchy_data[9];
            m_plane                  = hierarchy_data[10];
            m_num_of_planes          = hierarchy_data[11];
            m_aport                  = hierarchy_data[12];

            // DeviceRecord
            m_device_num_on_cpu_node = hierarchy_data[13] & 0xFF;
            m_cpu_node_number        = (hierarchy_data[13] >> 8) & 0xFF;

            // BoardRecord
            m_board_type             = (hierarchy_data[14]) & 0x3;
            m_chassis_slot_index     = (hierarchy_data[14] >> 8) & 0xFF;
            m_tray_index             = (hierarchy_data[14] >> 16) & 0xFF;

            // SystemRecord
            m_topology_id            = hierarchy_data[15];

        }

public:
        void createLabel(IBNodeType node_type) {
            std::stringstream ss;

            if (m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x04) {
                ss << hierPortTypeInt2char(m_port_type);

                if (m_asic_name != -1)
                    ss << hierAsicNameInt2char(m_asic_name);

                if (m_ibport != -1)
                    ss << 'P' << m_ibport;

                if (m_cage != -1)
                    ss << m_cage;

                if (m_port != -1)
                    ss << 'p' << m_port;

                if (m_split != -1)
                    ss << 's' << m_split;

            } else if (m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x03) {
                if (node_type == IB_SW_NODE) {
                    ss << m_asic << '/' << m_cage << '/' << m_port;
                    if (m_split != -1)
                        ss << '/' << m_split;
                } else if (node_type == IB_CA_NODE) {
                    if (m_slot_type == IB_SLOT_TYPE_PHYS) {
                        ss << "Physical" << m_slot_value << '/' << m_cage << '/' << m_port;
                    } else if (bus() != 0 || device() != 0 || function() != 0) {
                            ss << 'B' << bus() << 'D' << device() << 'F' << function() << '/' << m_cage
                                    << '/' << m_port;
                    }
                    if (m_split != -1)
                        ss << '/' << m_split;
                }
            } else if (m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x05) {
                if(m_port_type == HIERARCHY_INFO_PORT_TYPE_GPU) {
                    ss << "GPU" << "P" << m_ibport;
                }
                else {
                    ss << "ib";
                    if (bdf() != -1)
                        ss << 'B' << bus() << 'D' << device() << 'F' << function();

                    ss << hierPortTypeInt2char(m_port_type);

                    if (m_cage != -1)
                        ss << m_cage;

                    if (m_port != -1)
                        ss << "p" << m_port;

                    if (m_split != -1)
                        ss << "s" << m_split;
                }
            }

            if (ss.tellp() > 0)  {
                m_label = ss.str();
                if ((m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x04 ||
                        m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x05) && 
                        m_plane != -1) {
                    ss << "pl" << m_plane;
                }
                m_extended_label = ss.str();
            } else {
                m_label = "N/A";
                m_extended_label = "N/A";
            }

    }

    /**
     * @brief rerturn true if a node that holds the given port hierarchy info
     *        is a part of a multi-node switch
     */
    static bool IsMultiNodeSwitch(const PortHierarchyInfo *p_hier) {
        if (!p_hier)
            return false;

        return (p_hier->m_template_guid == HIERARCHY_TEMPLATE_GUID_PORT_0x04 &&
                    p_hier->m_asic_name >= MIN_MULTINODE_ASIC_NAME &&
                    p_hier->m_asic_name <= MAX_MULTINODE_ASIC_NAME);
    }

};

class PhysicalHierarchyInfo {
public:
    const int m_device_serial_num;
    const int m_board_type;
    const int m_board_slot_num;
    const int m_system_type;
    const int m_system_topu_num;
    const int m_rack_serial_num;
    const int m_room_serial_num;
    const int m_campus_serial_num;

    // DeviceRecord
    const int m_device_num_on_cpu_node;
    const int m_cpu_node_number;
    // BoardRecord
    // m_board_type already exist
    const int m_chassis_slot_index;
    const int m_tray_index;
    // SystemRecord
    const int m_topology_id;

    PhysicalHierarchyInfo(int campus_serial_num, int room_serial_num, int rack_serial_num,
                          int system_type, int system_topu_num, int board_type, int board_slot_num,
                          int device_serial_num, int device_num_on_cpu_node, int cpu_node_number,
                          int chassis_slot_index, int tray_index, int topology_id)
        : m_device_serial_num(device_serial_num),
          m_board_type(board_type),
          m_board_slot_num(board_slot_num),
          m_system_type(system_type),
          m_system_topu_num(system_topu_num),
          m_rack_serial_num(rack_serial_num),
          m_room_serial_num(room_serial_num),
          m_campus_serial_num(campus_serial_num),
          // template 6
          m_device_num_on_cpu_node(device_num_on_cpu_node),
          m_cpu_node_number(cpu_node_number),
          m_chassis_slot_index(chassis_slot_index),
          m_tray_index(tray_index),
          m_topology_id(topology_id) {}
};

///////////////////////////////////////////////////////////////////////////////
//
// IB Port class.
// This is not the "End Node" but the physical port of
// a node.
//
class IBPort {
    uint64_t        guid;           // The port GUID (on SW only on Port0)
    IBLinkWidth     width;          // The link width of the port
    IBLinkSpeed     speed;          // The link speed of the port
    IBPortState     port_state;     // The state of the port
    bool            in_sub_fabric;
    IBFECMode       fec_mode;       // The FEC mode of the port
    IBSpecialPortType special_port_type; // The type of special port
    u_int32_t       cap_mask;       // The capability mask; relevant for Switch's Port 0 only;
    uint16_t        cap_mask2;      // The capability mask 2; relevant for Switch's Port 0 only;
    bool            port_info_mad_was_sent; // indiactes if the PortInfo Mad was sent during discovery
public:
    class IBPort    *p_remotePort;  // Port connected on the other side of link
    class IBSysPort *p_sysPort;     // The system port (if any) connected to
    class IBNode    *p_node;        // The node the port is part of.
    class APort     *p_aport;       // The aport the port is part of.
    vec_pvch        channels;       // Virtual channels associated with the port
    phys_port_t     num;            // Physical ports are identified by number.
    lid_t           base_lid;       // The base lid assigned to the port.
    uint8_t         lmc;            // The LMC assigned to the port (0 <= lmc < 8)
    unsigned int    counter1;       // a generic value to be used by various algorithms
    unsigned int    counter2;       // a generic value to be used by various algorithms
    u_int32_t       createIndex;    // Port index, we will use it to create vectors of extended
                                    // info regarding this node and access the info in O(1)
    map_vportnum_vport  VPorts;     // map vport number to vports ,we use map because only not down vports are in the map
    IBKey<IB_AM_KEY> am_key;        // AM Key per port for AN device
    CombinedCableInfo *p_combined_cable;
    PrtlRecord        *p_prtl; // cable's PRTL register data. Used for cable's length calculations

    const PortHierarchyInfo *p_port_hierarchy_info;

    PluginData*     p_phy_data;     // pointer to object that hold all phy data.

    IBLinkWidth     expected_width; // The expected link width (in case of an error)
    IBLinkSpeed     expected_speed; // The expected link speed (in case of an error)

    struct {

        bool is_initialized = false;
        uint16_t VLMask = 0;
        bool MCEnable = false;
        bool UCEnable = false;
        vec_bool outPorts;

    } railFilterData;

    // constructor
    IBPort(IBNode *p_nodePtr, phys_port_t number);

    // destructor:
    ~IBPort();

    void CleanVPorts();

    // get the port name
    string getName() const;
    // get label from hierarchy info
    string getLabel() const;
    // get extend label from hierarchy info
    string getExtendedLabel() const;

    string numAsString() const;
    bool isValid();

    bool IsSplitted() const;

    bool is_lid_in_lmc_range(lid_t lid) const;

    // if the node isn't in sub cluster then the port is also not in sub cluster
    bool getInSubFabric() const;
    void setInSubFabric(bool in_sub_fabric);

    // connect the port to another node port
    void connect(IBPort *p_otherPort);

    // disconnect the port. Return 0 if successful
    int disconnect(int duringSysPortDisconnect = 0);

    inline uint64_t guid_get() const {return guid;};
    void guid_set(uint64_t g);

    IBNode* get_remote_node() const;

    bool isFNMPort() const;
    bool isFNM1Port() const;
    bool isFNMFamilyPort() const { return isFNMPort() || isFNM1Port(); }
    bool isSymmetricLink() const;

    //get_internal and get_common wraper functions
    //It can occur in the abric that the ports on the link
    //report different speed/width/state. This is illegal situation in the fabric.
    //It means that one of the port's fw is lying, but we don't know which.
    //This is why everywhere that access to speed/width/state is needed,
    //use get_common_speed/width/state (that return the common data on the link)
    //Except when we want to check and report the inconsistency on the link in that data,
    //then use get_internal functions.

    //get internal port state
    IBPortState get_internal_state() const { return port_state; }
    //set internal port_state
    void set_internal_state(IBPortState s) { port_state = s; }
    //get common port_state on the port and its peer
    IBPortState get_common_state();

    //get internal port width
    IBLinkWidth get_internal_width() const { return width; }
    //set internal port width
    void set_internal_width(IBLinkWidth w) { width = w; }
    //get common port width on the port and its peer
    IBLinkWidth get_common_width();

    //get internal speed
    IBLinkSpeed get_internal_speed() const { return speed; }
    //set internal speed
    void set_internal_speed(IBLinkSpeed sp) { speed = sp; }
    //get common port speed on the port and its peer
    IBLinkSpeed get_common_speed() const;

    void set_internal_data(IBLinkSpeed sp, IBLinkWidth w, IBPortState s) {
        speed = sp;
        width = w;
        port_state = s;
    }

    //Port that is worth reading/checking data, port that is not down,
    //not in undefined state,
    //and is in the scope of the fabric, we currently look at.
    bool is_data_worthy(bool is_direct_route) const {
        if (is_direct_route)
            return ((port_state > IB_PORT_STATE_DOWN) && getInSubFabric());
        return ((port_state > IB_PORT_STATE_INIT) && getInSubFabric());
    }
    IBFECMode get_fec_mode() const { return fec_mode; }
    void set_fec_mode(IBFECMode fec) { fec_mode = fec; }
    inline bool IsPLR() {
        if ((this->get_fec_mode() == IB_FEC_MLNX_RS_544_514_PLR) ||
            (this->get_fec_mode() == IB_FEC_MLNX_RS_271_257_PLR))
             return true;

        return false;
    }
    IBVPort *getVPort(virtual_port_t num) {
        map_vportnum_vport::iterator VPorts_iter = VPorts.find(num);
        return ((VPorts_iter == VPorts.end()) ? NULL : VPorts_iter->second);
    }

    bool isSpecialPort() const;
    // Set the type of special port
    void setSpecialPortType(IBSpecialPortType special_port_type);
    // Get the type of special port
    IBSpecialPortType getSpecialPortType() const;

    // Get the extended name of IBPort name
    string getExtendedName() const;

    // Set Port's capability mask; relevant for Port 0
    void setCapMask(u_int32_t  cap_mask);
    u_int32_t getCapMask() const;

    // Set Port's capability mask 2; relevant for Port 0
    void setCapMask2(uint16_t cap_mask);
    uint16_t getCapMask2() const;

    void setPortInfoMadWasSent(bool val);
    //indicates if PortInfo MAD was sent for this port.
    bool getPortInfoMadWasSent() const;

    PCI_Address getPCIaddress() const;

    int get_plane_number() const;

    void addRailFilterEntry(vec_uint16& VLList,
                            bool MCEnable,
                            bool UCEnable,
                            list_phys_ports& portsList);

    bool isPassingRailFilter(phys_port_t out_port) const;
};

///////////////////////////////////////////////////////////////////////////////
//
// IB Links info class
//
class IBLinksInfo {
private:
    u_int32_t num_of_ib_links;
    vec_vec_uint32 ib_link_width_speed_matrix;

    uint32_t num_of_fnm_links;
    vec_vec_uint32 fnm_link_width_speed_matrix;

    uint32_t num_of_fnm1_links;
    vec_vec_uint32 fnm1_link_width_speed_matrix;

public:

    // Constructor
    IBLinksInfo();

    // Destructor
    ~IBLinksInfo() { }

    // Get total numbers of IB links in fabric
    inline u_int32_t GetNumOfIBLinks() const { return this->num_of_ib_links; }
    inline uint32_t GetNumOfFNMLinks() const { return this->num_of_fnm_links; }
    inline uint32_t GetNumOfFNM1Links() const { return this->num_of_fnm1_links; }

    // Get IB links width-speed matrix
    inline vec_vec_uint32 & GetIBLinkWidthSpeedMatrix() {
        return this->ib_link_width_speed_matrix;
    }

    // Get FNM links width-speed matrix
    inline vec_vec_uint32& GetFNMLinkWidthSpeedMatrix() {
        return this->fnm_link_width_speed_matrix;
    }

    // Get FNM1 links width-speed matrix
    inline vec_vec_uint32& GetFNM1LinkWidthSpeedMatrix() {
        return this->fnm1_link_width_speed_matrix;
    }

    // Fill IB links width-speed matrix of specific port and count the total number of IB links
    void FillIBLinkWidthSpeedIndex(IBPort * p_port);
    void FillFNMLinkWidthSpeedIndex(IBPort* p_port);

    void FillIBAPortLinkWidthSpeedIndex(APort * p_aport);
    void FillFNM1LinkWidthSpeedIndex(APort* p_aport);

    void FillAsymmetricalLinks(APort * p_aport);
};

///////////////////////////////////////////////////////////////////////////////
//
// IB VPort class.
// This is the virtual port part of physical port.
// vports are only relevant for HCAs
class IBVPort {
    uint64_t        m_guid;           // The port GUID
    IBPortState     m_vport_state;    // The state of the vport
    class IBFabric *      m_p_fabric;
    // If lid_required is set to 1, the  vport lid assigned by SM
    lid_t           m_vlid;
    class IBPort    *m_p_phys_port;   // The phys port the vport is part of.
    virtual_port_t  m_num;            // The index number in the physical port's vports.
    u_int8_t        m_vlocal_port_num; // The number of the VHCA port which received this SMP.
    virtual_port_t  m_lid_by_vport_index;

public:


    u_int32_t       createIndex;    // VPort index, we will use it to create vectors of extended
                                    // info regarding this vport and access the info in O(1)
    class IBVNode   *m_p_vnode;       // A pointer to Vnode which this VPort is part of
    // constructor
    IBVPort(IBPort *p_phys_portPtr, virtual_port_t number,
            uint64_t guid, IBPortState vport_state, IBFabric *p_fab);
    // destructor:
    ~IBVPort();

    // get the port name
    string getName();

    inline lid_t get_vlid() const { return m_vlid; }
    void set_vlid(lid_t l) { m_vlid = l; }

    inline uint64_t guid_get() const { return m_guid; };
    void guid_set(uint64_t g);

    //get port state
    IBPortState get_state() const { return m_vport_state; }
    //set port_state
    void set_state(IBPortState s) { m_vport_state = s; }

    // when creating the VPort we don't know it's VNode yes.
    // so the VNode will be set later
    void setVNodePtr(IBVNode *p_vnode);

    inline u_int8_t get_vlocal_port_num() const { return m_vlocal_port_num; }
    void set_vlocal_port_num(u_int8_t val) { m_vlocal_port_num = val; }

    inline virtual_port_t get_lid_by_vport_index() const { return m_lid_by_vport_index; }
    void set_lid_by_vport_index(virtual_port_t val) { m_lid_by_vport_index = val; }

    IBVNode *getVNodePtr();

    inline IBPort *getIBPortPtr() { return m_p_phys_port; }
    inline APort *getAPortPtr() {
        if (m_p_phys_port)
            return m_p_phys_port->p_aport;
        return nullptr;
    }
    inline IBFabric *getIBFabricPtr() { return m_p_fabric; }
    inline virtual_port_t getVPortNum() const { return m_num; }
};


///////////////////////////////////////////////////////////////////////////////
//
// IB VNode class.
// This is the virtual node part of physical node.
// May have many Virtual HCAs on physical HCA.
class IBVNode {
    uint64_t        guid;           // The port GUID
    class IBFabric  *p_fabric;
    virtual_port_t  numVPorts;
    string          description;           //Description of the vnode

public:
    u_int32_t       createIndex;    // VNode index, we will use it to create vectors of extended
                                    // info regarding this vnode and access the info in O(1)
    map_vportnum_vport  VPorts;     // map vport number to vports ,we use map
                                    // because only not down vports are in the map

    // constructor
    IBVNode(uint64_t guid, IBFabric *p_fab, virtual_port_t np, u_int32_t createIndex);
    // destructor:
    ~IBVNode();

    inline uint64_t guid_get() const { return guid; };

    // get the vnode description
    string getDescription() const;

    // set the vnode description
    void setDescription(const string & n);

    //Get the VNode name. Currently returns the vnode description.
    string getName() const;


    // add vport into VPort
    int addVPort(virtual_port_t num, IBVPort *p_vport);
};

///////////////////////////////////////////////////////////////////////////////
//
// Private App Data
//
typedef union _PrivateAppData {
    void        *ptr;
    uint64_t    val;
} PrivateAppData;

enum SMP_AR_LID_STATE {
    AR_IB_LID_STATE_BOUNDED = 0x0,
    AR_IB_LID_STATE_FREE = 0x1,
    AR_IB_LID_STATE_STATIC = 0x2,
    AR_IB_LID_STATE_HBF = 0x3,
    AR_IB_LID_STATE_LAST
};

//
// AR Group class
//
class ARgrp {
    int subGrpsNum;
    vec_list_phys_ports subGrps;

public:
    ARgrp(int subGrpsNum = 2) :
            subGrpsNum(subGrpsNum),
            subGrps(vec_list_phys_ports(subGrpsNum)) {
    }
    virtual ~ARgrp() {
    }

    void setSubGrp(const list_phys_ports& subGrpToSet, int idx) {
        subGrps[idx] = subGrpToSet;
    }
    const list_phys_ports& getSubGrp(int idx) const {
        return subGrps[idx];
    }
    list_phys_ports& operator[](int idx) {
        return subGrps[idx];
    }
    const list_phys_ports& operator[](int idx) const {
        return subGrps[idx];
    }

    bool operator==(const ARgrp& grp2) const {
        return (this->subGrps == grp2.subGrps
                && this->subGrpsNum == grp2.subGrpsNum);
    }
    int getSubGrpsNum() {
        return this->subGrpsNum;
    }

};

enum IBSplittedType {
    IB_NOT_SPLITTED = 0,
    IB_SPLITTED_QUANTOM1,
    IB_SPLITTED_QUANTOM2
};

//
// IB Node class
//
class IBNode {
    uint64_t        guid;
    uint64_t        sys_guid;       // guid of the system that node is part of
    vec_pport       Ports;          // Vector of all the ports (in index 0 we will put port0 if exist)
    static bool     usePSL;         // true if should use data from PSL
    static u_int8_t maxSL;          //maxSL used in the fabric if usePSL
    static bool     useSLVL;        // true if should use data from SLVL
    vector<bool>    replaceSLsByInVL;

    //PLFT
    bool            pLFTEnabled;
    vec_vec_byte    portSLToPLFTMap;
    vec_uint16      pLFTTop;
    u_int8_t        maxPLFT; //max pLFT ID
    //AR - refers also to FR
    u_int16_t       arGroupTop;
    bool            isArGroupTopSupported;
    //arBySLEn is kept for having an entry in the FAR file only
    //arEnableBySLMask is enough to support AR functionality
    bool            arBySLEn;
    u_int16_t       arEnableBySLMask;
    u_int8_t        arSubGrpsActive;
    vec_list_phys_ports   arPortGroups;
    u_int16_t       arMaxGroupNumber;
    vec_vec_uint16  arLFT; //arLFT[plft][lid] = portGroup
    bool            RNSupported;

    //ARmodes + subgroups
    vector< vector<SMP_AR_LID_STATE> > arState; // arState[plft][lid] = lid's state
    map<uint16_t , ARgrp> arGroups; //Adaptive Routing Groups. ( arGroups[ARgroupIdx] = ArGroup )
                                    //Differently from arPortGroups supports division to sub groups.
                                    //Updated by parsing ".far" file

    bool            frEnabled;
    bool            arRnXmitEnabled;
    u_int8_t        by_transport_disable;

    bool            in_sub_fabric;

    PCI_AddressMap  pciAddresses;
    bool            sdm;//socket direct mode indicator

    bool      hbfSupported;
    bool      weightsHbfEnabled;
    u_int16_t hbfEnableBySlMask;

    bool      is_pfrn_supported;
    bool      pfrn_enabled;

 public:
    const IBNodeType type;          // Either a CA or SW or RTR
    IBExtNodeType   ext_type;       // Either a GPU or MULTI_PLANE_HCA
    string          name;           // Name of the node (instance name of the chip)
    device_id_t     devId;          // The device ID of the node
    uint32_t        revId;          // The device revision Id.
    uint32_t        vendId;         // The device Vendor ID.
    uint8_t         rank;           // The tree level the node is at 0 is root.
    class IBSystem  *p_system;      // What system we belong to
    class IBFabric  *p_fabric;      // What fabric we belong to.
    phys_port_t     numPorts;       // Number of physical ports
    string          attributes;     // Comma-sep string of arbitrary attributes k=v
    string          description;    // Description of the node
    string          orig_description;// Original description,
                                     //when usr map file is given
    vec_vec_byte    MinHopsTable;   // Table describing minimal hop count through
                                    // each port to each target lid
    vec_vec_byte    LFT;            // The LFT of this node (for switches only)
    vec_byte        PSL;            // PSL table (CAxCA->SL mapping) of this node (for CAs only)
    vec3_byte       SLVL;           // SL2VL table of this node (for switches only)
    vec_phys_ports  slvlPortsGroups; //group ports with same optPort-vl-vl value
    vec_port_bitset MFT;            // The Multicast forwarding table
    PrivateAppData  appData1;       // Application Private Data #1
    PrivateAppData  appData2;       // Application Private Data #2
    PrivateAppData  appData3;       // Application Private Data #3
    u_int32_t       createIndex;    // Node index, we will use it to create vectors of extended
                                    // info regarding this node and access the info in O(1)
    bool            toIgnore;
    phys_port_t     numVirtPorts;   // number of ports out of the numPorts that are virtual
                                    // (for these ports there is no checking in topodiff)
    SerDesVersion   serdes_version; // SerDes Version of the node (28/16/7 nm)
    PluginData*     p_phy_data;     // pointer to object that hold all phy data.

    set_lid remote_enabled_flids;   // for Routers - set of enabled FLIDs
    set_lid flids;                  // keeps FLIDs associated with a swcith

    bool            should_support_port_hierarchy_info;

    const PhysicalHierarchyInfo *p_physical_hierarchy_info;

    bool            skipRoutingChecks;

    set<uint8_t>    fast_recovery_profiles; // for Fast Recovery feature, save valid profiles

    vec3_bool EPF;               // Entry Plane Filter; EPF[in_port][plane][out_port] = 0/1
    vec_lids EndPortPlaneFilter; // EndPortPlaneFilter[plane] = LID

    struct {
      // to track the actual paths going through each switch port
      // we need to have a map from switch node to a vector of count
      // per port.
      vec_int paths;


      // tracks if a path to current dlid was found per switch out port
      vec_int any_paths;

      // track the number of dlids actually routed through each switch port.
      // to avoid memory scalability we do the path scanning with dest port
      // in the external loop. So we only need to look on the aggregated
      // vector per port at the end of all sources and sum up to the final results
      vec_int lids;

      // group_check_data - struct to save results in credit loop check for AR.
      // while checking if a dlids list is broken (meaning: not all dlids
      // have same ARLFTPortGroupon switch).
      struct group_check_data {

          // the current dlid
          lid_t    dlid;

          // false - group was checked and found OK (didn't break).
          // true  - group was checked and found broken.
          bool     failed;

          group_check_data() : dlid(0), failed(false) { }
      };
      // We save this cache per PLFT per sup/notSup AR.
      group_check_data group_check[2][MAX_PLFT_NUM];

      // representative CA as slid for switches
      bool      representative; // true if the first CA can be used as a representative
      lid_t     CA_slid;        // lid of first CA
      set_lid   CA_lids;

    } routing_cache;

    // Constructor
    IBNode(const string & n, class IBFabric *p_fab, class IBSystem *p_sys,
            IBNodeType t, phys_port_t np);

    // destructor:
    ~IBNode();

    const string &getAlternativeName() const;

    int getPlanesNumber() const;
    bool isPrismaSwitch() const;

    bool isPassingEPF(phys_port_t in_port, int plane, phys_port_t out_port) const;
    bool checkPortByEPFandPlane(phys_port_t in_port, int plane, phys_port_t out_port) const;
    list_phys_ports filterPortsByEPFAndPlane(phys_port_t in_ports,
                                             const list_phys_ports &out_ports, int plane) const;

    /**
     * @brief Checks if the current node is a Black Mamba switch.
     *        If the all ports in the switch have the same plane number,
     *        and all APorts have 4 planes, it's a BM switch.
     * 
     * @return true 
     * @return false 
     */
    bool isMultiNode() const;
    string getPrismaSwitchASIC() const;
    bool isOnSamePlane(int plane) const;
    int getSuitablePlane() const;

    bool hasFNMPort() const;
    IBPort *getFNMPort(phys_port_t num) const;
    size_t getAllFNMPorts(vec_pport& ports) const;

    bool getInSubFabric() const {return this->in_sub_fabric; }
    void setInSubFabric(bool in_sub_fabric) {
        this->in_sub_fabric = in_sub_fabric;
    }

    // get the node name
    inline const string &getName() const { return name; }

    //get original description
    inline const string &getOrigNodeDescription() const {
        return orig_description.empty() ? description : orig_description;
    }

    // create a new port by name if required to
    IBPort *makePort(phys_port_t num);

    // get a port by number num = 1..N:
    inline IBPort *getPort(phys_port_t num) const {
        if ((type == IB_SW_NODE) && (num == 0))
            return Ports[0];
        if ((num < 1) || (Ports.size() <= num))
            return NULL;
        return Ports[num];
    }

    // Set the min hop for the given port (* is all) lid pair
    void setHops(IBPort *p_port, lid_t lid, uint8_t hops);

    // Get the min number of hops defined for the given port or all
    uint8_t getHops(IBPort *p_port, lid_t lid);

    // Report Hop Table of the given node
    void repHopTable();

    // Scan the node ports and find the first port
    // with min hop to the lid
    IBPort *getFirstMinHopPort(lid_t lid);

    // Set the Linear Forwarding Table:
    void setLFTPortForLid(lid_t lid, phys_port_t portNum, u_int8_t pLFT=0);

    // Set the Switch AR State Table (for the given lid)
    void setARstateForLid(lid_t lid, SMP_AR_LID_STATE state, u_int8_t pLFT = 0);

    //get the AR state for the given lid
    SMP_AR_LID_STATE getARstateForLid(lid_t lid, u_int8_t pLFT = 0) const;

    // Get the LFT for a given lid
    phys_port_t getLFTPortForLid(lid_t lid, phys_port_t port, sl_vl_t slvl) const {
        return getLFTPortForLid(lid, getPLFTMapping(port, slvl));}

    phys_port_t getLFTPortForLid(lid_t lid, u_int8_t pLFT=0) const;
    //unsecure version of getLFTPortForLid
    phys_port_t getLFTEntry(lid_t lid, u_int8_t pLFT=0) const {
        return ( LFT[pLFT][lid] );}

    // Resize the Linear Forwarding Table (can be resize with linear top value):
    void resizeLFT(uint16_t newSize, u_int8_t pLFT=0);

    void clearLFT(u_int8_t pLFT) {LFT[pLFT].clear();}

    // Set the PSL table
    void setPSLForLid(lid_t lid, lid_t maxLid, uint8_t sl);

    //set replaceSLsByInVL
    void setSL2VLAct(u_int8_t slvlAct);
    void setVL2VL(u_int8_t sl);

    void getSL2VLCfg(char *line) const;

    //get SL or VL according to replaceSLsByInVL
    u_int8_t getUsedSLOrVL(sl_vl_t slvl) const;

    // Add entry to SL2VL table
    void setSLVL(phys_port_t iport,phys_port_t oport,uint8_t sl, uint8_t vl);

    // Get the PSL table for a given lid
    uint8_t getPSLForLid(lid_t lid);

    // Check if node contains PSL Info:
    bool isValidPSLInfo() const { return !(PSL.empty() && usePSL); }
    static bool getUsePSL() { return usePSL; }

    static uint8_t getMaxSL() { return maxSL; }
    static void setMaxSL(uint8_t sl) { maxSL = max(maxSL, sl); }

    uint8_t getVL(phys_port_t iport, phys_port_t oport, sl_vl_t slvl) const;
    // Get the SL2VL table entry
    void getSLVL(phys_port_t iport, phys_port_t oport,
                                const sl_vl_t &iSLVL, sl_vl_t &oSLVL) const;

    //check that all SLs leads to valid VLs
    int checkSL2VLTable() const;

    phys_port_t getSLVLPortGroup(phys_port_t port);
    void buildSLVLPortsGroups();

    // Set the Multicast FDB table
    void setMFTPortForMLid(lid_t lid, phys_port_t portNum);
    void setMFTPortForMLid(lid_t lid, uint16_t portMask, uint8_t portGroup);

    // Get the list of ports for the givan MLID from the MFT
    list_phys_ports getMFTPortsForMLid(lid_t lid);

    int getLidAndLMC(phys_port_t portNum, lid_t &lid, uint8_t &lmc) const;
    lid_t getFirstLid() const;
    IBPort* getFirstPort(bool include_init_state);
    IBPort* getFirstPortNotDown();
    IBPort* getFirstPortNotInit();
    IBPort* getPortWithAsicName() const;

    inline uint64_t guid_get() const {return guid;};
    void guid_set(uint64_t g);
    inline uint64_t system_guid_get() const {return sys_guid;};
    void system_guid_set(uint64_t g);

    //PLFT
    void setPLFTEnabled();
    bool isPLFTEnabled() const {return pLFTEnabled;}
    void setPLFTMapping(phys_port_t port, u_int8_t nSLVL, u_int8_t pLFT) {
        portSLToPLFTMap[port][nSLVL] = pLFT;
        maxPLFT = max(maxPLFT, pLFT);
    }

    void setMaxPLFT(u_int8_t pLFT) { maxPLFT = pLFT; }
    u_int8_t getMaxPLFT() const {return maxPLFT;}

    u_int8_t getPLFTMapping(phys_port_t port, sl_vl_t slvl) const;
    void getPLFTMapping(phys_port_t port, char *plft_line) const;

    void setLFDBTop(u_int8_t pLFT, u_int16_t lfdbTop){pLFTTop[pLFT] = lfdbTop;}
    u_int16_t getLFDBTop(u_int8_t pLFT = 0) const {return pLFTTop[pLFT];}


    //HBF
    void setHBFEnabled(u_int8_t is_hbf_supported, u_int8_t is_weights_hbf_enabled,
                    u_int16_t enableBySLMask) {
        hbfSupported = (bool) is_hbf_supported;
        weightsHbfEnabled = (bool) is_weights_hbf_enabled;
        hbfEnableBySlMask = enableBySLMask;
    }

    void setHBFEnabledSLMask(u_int16_t slMask) { hbfEnableBySlMask = slMask; }
    u_int16_t getHBFEnabledSLMask() const { return hbfEnableBySlMask; }

    bool isHBFEnable() const { return hbfSupported && (hbfEnableBySlMask != 0); }

    bool isWeightsHBFEnabled() const { return weightsHbfEnabled; }

    bool isHBFActive(sl_vl_t slvl) const;

    bool isHBFSupported() const { return hbfSupported; }

    //AR
    void setAREnabled(bool by_sl_en, u_int16_t enableBySLMask, u_int8_t subGrpsActive,
                      u_int8_t fr_enabled, u_int8_t by_transp_cap, u_int8_t by_transp_disable) {
        arBySLEn = by_sl_en;
        arEnableBySLMask = enableBySLMask;
        arSubGrpsActive = subGrpsActive;
        frEnabled = (bool)fr_enabled;
        by_transport_disable = by_transp_cap ? by_transp_disable : (u_int8_t)0;
    }
    void setAREnabled(u_int8_t sl) {
        arEnableBySLMask =
            (u_int16_t)(arEnableBySLMask | (u_int16_t)(1<< sl));
    }

    void setAREnabledSLMask(u_int16_t slMask) { arEnableBySLMask = slMask; }
    u_int16_t getAREnabledSLMask() const { return arEnableBySLMask; }

    void setRNXmitEnabled(bool rn_xmit_enabled) { arRnXmitEnabled = rn_xmit_enabled; }
    bool isRnXmitEnabled() const { return arRnXmitEnabled; }

    void setRNSupported(bool rn_supported) { RNSupported = rn_supported; }
    bool isRNSupported() const { return RNSupported; }

    bool isBySLEn () const { return arBySLEn; }

    bool isFREnabled() const {
        return frEnabled;
    }
    u_int8_t getTranspDisable() const {
        return by_transport_disable;
    }
    void setFREnabled(){
        frEnabled =  true;
    }

    // pFRN
    bool ispFRNSupported() const { return is_pfrn_supported; }
    void setpFRNSupported(bool val = true) { is_pfrn_supported = val; }
    bool ispFRNEnabled() const { return pfrn_enabled; }
    void setpFRNEnabled(bool val = true) { pfrn_enabled = val; }

    void setARGroupTop(u_int16_t group_top) {
        arGroupTop = group_top;
        if(arPortGroups.size() <= arGroupTop)
            arPortGroups.resize(arGroupTop + 1);
    }
    bool isARGroupTopSupported() const {
        return isArGroupTopSupported;
    }
    void setARGroupTopSopported(bool val = true) {
        isArGroupTopSupported = val;
    }
    //isAREnable also refer to FR
    bool isAREnable() const {return (frEnabled || arEnableBySLMask != 0);}
    bool isARActive(sl_vl_t slvl) const;
    void getARActiveCfg(char *line) const;
    void getARGroupCfg(u_int16_t groupNumber, char *line) const;
    ostream& getARActiveCfg(ostream & stream) const;
    ostream& getARGroupCfg(u_int16_t groupNumber, ostream & stream) const;

    void setARPortGroup(u_int16_t groupNum,
                        list_phys_ports portsList);

    u_int16_t getARSubGrpsActive() const {
        return arSubGrpsActive;
    }

    u_int16_t getARGroupTop() const {
        return arGroupTop;
    }

    u_int16_t getNonEmptyARGroupNumber() const {
        u_int16_t arGroupNum = 0;
        for (u_int16_t grp = 0; grp <= arGroupTop; ++grp) {
            if (!arPortGroups[grp].empty())
                arGroupNum++;
        }
        return arGroupNum;
    }

    u_int16_t getMaxARGroupNumber() const {return arMaxGroupNumber;}
    //void IBNode::resizeARPortGroup(size_t size) {arPortGroups.resize(size);}

    void setARLFTPortGroupForLid(lid_t lid, u_int16_t portGroup,
                                 u_int8_t pLFT);
    u_int16_t getARLFTPortGroupForLid(lid_t lid, u_int8_t pLFT) const;
    void getLFTPortListForLid(lid_t lid, u_int8_t pLFT,
                              bool useAR, list_phys_ports &portsList) const ;
    void getLFTPortListForLid(lid_t lid, phys_port_t inPort,
                              sl_vl_t slvl, list_phys_ports &portsList) const;

    void getLFTPortListForLid(phys_port_t staticOutPortNum,
                              u_int16_t portGroup, list_phys_ports &portsList) const;


    bool isARPortGroupEmpty(u_int16_t portGroup) const;

    void resizeARLFT(uint16_t newSize, u_int8_t pLFT);

    inline phys_port_t numRealPorts() const {
        return (this->numPorts >= this->numVirtPorts) ?
                (phys_port_t)(this->numPorts - this->numVirtPorts) :
                (phys_port_t)0;
    }

    inline bool isRealPort(phys_port_t pn) const {
        return pn <= this->numRealPorts();
    }

    void resizeARstate(uint16_t newSize, u_int8_t pLFT);

    //AR+ sub Groups
    void setARSubGrp(u_int16_t group, u_int16_t subGroup,
            const list_phys_ports& portsList);

    list_phys_ports& getARSubGrp(u_int16_t group, u_int16_t subGroup) {
        return this->arGroups[group][subGroup];
    }
    ARgrp& getARgroup(u_int16_t grpIdx) {
        return this->arGroups[grpIdx];
    }

    // Check if the device is in splited mode
    IBSplittedType GetSplitType() const;
    bool isSplitted();

    bool isSpecialNode() const;
    IBSpecialPortType getSpecialNodeType() const;

    bool IsFiltered(NodeTypesFilter mask);

    const PCI_AddressMap& getPCIaddresses() const {
        return this->pciAddresses;
    }

    PCI_Address getPCIaddress() const;
    void addPCIAddress (const PCI_Index &index, const PCI_Address &address, uint8_t sdm) {
        this->pciAddresses[index] = address;

        if (sdm == 1)
            this->sdm = true;
    }

    bool isSDM () const { return this->sdm == true; }

    void addEPFEntry(uint8_t in_port, uint8_t plane, const list_phys_ports& out_ports);
    // return OR of all planes up to max_planes. if max_planes is 0, find the maximum.
    // returns true if inputs exceed vector size.
    bool getEPFFromAllPlanes(uint8_t in_p, uint8_t out_p, u_int8_t max_planes = 0) const;
    // whether EPF data is large enough
    bool CheckEPFSize(u_int8_t max_planes) const;
};
// Class IBNode

///////////////////////////////////////////////////////////////////////////////
//
// System Port Class
// The System Port is a front pannel entity.
//
class IBSysPort {
public:
    string          name;               // The front pannel name of the port
    class IBSysPort *p_remoteSysPort;   // If connected the other side sys port
    class IBSystem  *p_system;          // System it benongs to
    class IBPort    *p_nodePort;        // The node port it connects to.

    // Constructor
    IBSysPort(const string & n, class IBSystem *p_sys);

    // destructor:
    ~IBSysPort();

    // connect two SysPorts
    void connectPorts(IBSysPort *p_otherSysPort);

    // connect two SysPorts and two node ports
    void connect(IBSysPort *p_otherSysPort,
            IBLinkWidth width = IB_LINK_WIDTH_4X,
            IBLinkSpeed speed = IB_LINK_SPEED_2_5,
            IBPortState state = IB_PORT_STATE_ACTIVE);

    // disconnect the SysPort (and ports). Return 0 if successful
    int disconnect(int duringPortDisconnect = 0);
};

///////////////////////////////////////////////////////////////////////////////

//typedef enum {IB_SYS_OK, IB_SYS_MOD, IB_SYS_NEW} IBSysTypeDesc;

///////////////////////////////////////////////////////////////////////////////
//
// IB System Class
// This is normally derived into a system specific class
//
class IBSystem {
public:
    string              name;       // the "host" name of the system
    string              type;       // what is the type i.e. Cougar, Buffalo etc
    string              cfg;        // configuration string modifier
    class IBFabric      *p_fabric;  // fabric belongs to
    map_str_psysport    PortByName; // A map provising pointer to the SysPort by name
    map_str_pnode       NodeByName; // All the nodes belonging to this system.
    map_str_vec_str     APorts;     // A map that contains matching sysports for a given APort
    bool                APortsSupportNonPlanarized;   // the APORTs can connect to non Planarized ports
    bool                newDef;     // system without SysDef
    bool                sys_mlx_nd_format; //system with node description of format "hostname mlx_card_name"
    //TBD, if more card types are added or changed, consider holding
    //the max index of each card type in a map by type
    int                 max_mlx4; // if the format is with NDD this value holds the max mlx4 HCAs
    int                 max_mlx5; // if the format is with NDD this value holds the max mlx5 HCAs

    // Default Constractor - empty need to be overwritten
    IBSystem()
        : p_fabric(NULL), APortsSupportNonPlanarized(false),
          newDef(false), sys_mlx_nd_format(false),
          max_mlx4(0), max_mlx5(0)
        {} ;

    // Destructor must be virtual
    virtual ~IBSystem();

    // Constractor
    IBSystem(const string &n, class IBFabric *p_fab, const string &t,
             bool new_nd_format = false);

    // Get a string with all the System Port Names (even if not connected)
    virtual list_str getAllSysPortNames();

    // Get a Sys Port by name
    IBSysPort *getSysPort(string name);

    inline IBNode *getNode(string nName) {
        map_str_pnode::iterator nI = NodeByName.find(nName);
        if (nI != NodeByName.end()) {
            return (*nI).second;
        } else {
            return NULL;
        }
    };

    // make sure we got the port defined (so define it if not)
    virtual IBSysPort *makeSysPort(string pName);

    // get the node port for the given sys port by name
    virtual IBPort *getSysPortNodePortByName(string sysPortName);

    // Remove a sub module of the system
    int removeBoard(const string & boardName);

    // parse configuration string into a vector
    void cfg2Vector(const string& cfg,
            vector<string>& boardCfgs,
            int numBoards) const;

    int CreateMissingSystemMlxNodes(map_str_pnode *pIBNLFabricNodes);

    // Write system IBNL into the given directory and return IBNL name
    int dumpIBNL(string &sysType);

    void generateSysPortName(char *buf, IBNode *p_node, unsigned int  pn);

    /**
     * @brief returns true if all the system's switches are multinode
     */
    bool isMultiNodeSystem() const;
};

//forward declaration of the scope class
class IBScope;
class rexMatch;

///////////////////////////////////////////////////////////////////////////////
//
// IB Fabric Class
// The entire fabric
//
class IBFabric {
private:
    u_int32_t       numOfNodesCreated;
    u_int32_t       numOfPortsCreated;
    u_int32_t       numOfVPortsCreated;
    u_int32_t       numOfVNodesCreated;

    u_int8_t        defaultSL;      //default SL used for routing check
    phys_port_t     maxNodePorts;   // Track max ports for a node in the fabric.
    IBRoutingEngine routing_engine; // Routing engine/algorithm in the fabric.
    bool            is_smdb_applied;// Check if data from SMDB file applied.
    static string   timestamp;
    bool            is_epf_valid;      // true if EPF is valid or disabled.
public:
    map_str_pnode           NodeByName;     // Provide the node pointer by its name
    map_str_pnode           FullNodeByName; // Provide the node pointer by its name
    map_guid_pnode          NodeByGuid;     // Provides the node by guid
    map_str_psys            SystemByName;   // Provide the system pointer by its name
    map_guid_pnode          NodeBySystemGuid; // Provides the node by system guid
    set_pnode               Switches;       // Provides the switches
    set_pnode               HCAs;           // Provides the hcas
    set_pnode               Routers;        // Provides the routers
    map_guid_list_p_aport   APortsBySysGuid;   // Provides APorts associated to node's System GUID
    map_guid_pport          PortByGuid;     // Provides the port by guid
    map_guid_pvnode         VNodeByGuid;    // Provides VNode by guid
    map_guid_pvport         VPortByGuid;    // Provides VPort by guid
    map_guid_pport          PortByAGuid;    // Provides the port by aguid
    map_str_list_pnode      NodeByDesc;     // Provides nodes by node description - valid
                                            // only for discovery process, after that we will
                                            // not update this field if delete a node
    map_guid_str            NGuid2Name;     // Maps node guid to user given name
    map_mcast_groups        McastGroups;    // Multicast group information
                                            // valid only after parsing sa_dump
    map_nodes_per_lid       RoutersByFLID;  // map of vecs of routers per flid
    vec_pport               PortByLid;      // Pointer to the Port by its lid
    vec_pvport              VPortByLid;     // Pointer to the VPort by its lid
    lid_t                   minLid;         // Track min lid used.
    lid_t                   maxLid;         // Track max lid used.
    uint8_t                 caLmc;          // Default LMC value used for all CAs and Routers
    uint8_t                 swLmc;          // Default LMC value used for all Switches
    uint8_t                 defAllPorts;    // If not zero all ports (unconn) are declared
    uint8_t                 subnCANames;    // CA nodes are marked by name
    uint8_t                 numSLs;         // Number of used SLs
    uint8_t                 numVLs;         // Number of used VLs
    set_uint16              mcGroups;       // A set of all active multicast groups
    bool                    pLFTEnabled;    //Private LFT is enabled on at least one switch

    static string           running_version; // Runnig user command line
    static string           running_command; // Running App version

    lid_t                   globalMinFLID;
    lid_t                   globalMaxFLID;

    // Constructor
    IBFabric() {
        Init();
    };

    // Destructor
    ~IBFabric() {
        CleanUpInternalDB();
    }

    void Init();
    void CleanUpInternalDB();
    void CleanVNodes();
    void unvisitAllAPorts();

    inline u_int32_t getNodeIndex() { return numOfNodesCreated++; }
    inline u_int32_t getPortIndex() { return numOfPortsCreated++; }
    inline u_int32_t getVPortIndex() { return numOfVPortsCreated++; }
    inline u_int32_t getVNodeIndex() { return numOfVNodesCreated++; }

    inline void ResetVPortIndex() { this->numOfVPortsCreated = 0; }
    inline void ResetVNodeIndex() { this->numOfVNodesCreated = 0; }

    bool isFLID(lid_t lid) const { return (RoutersByFLID.find(lid) != RoutersByFLID.end()); }

    IBNode* createNode(string name,
                       IBSystem* p_sys,
                       IBNodeType type,
                       phys_port_t numPorts);

    // get the node by its name (create one if does not exist)
    IBNode *makeNode(string n,
                IBSystem *p_sys,
                IBNodeType type,
                phys_port_t numPorts,
                uint64_t sysGuid = 0,
                uint64_t nodeGuid = 0,
                bool should_be_new = false);

    // get port by guid:
    IBPort *getPortByGuid(uint64_t guid);
    IBPort *getPortByGuid(uint64_t guid, bool get_vguid, bool get_aguid);

    // get the node by its name
    IBNode *getNode(string name);
    IBNode *getNodeByGuid(uint64_t guid);

    // get vport by guid:
    IBVPort *getVPortByGuid(uint64_t guid);

    // crate a new generic system - basically an empty contaner for nodes...
    IBSystem *makeGenericSystem(const string &name, const string &sysType,
                                bool new_nd_format);

    // crate a new system - the type must have a pre-red SysDef
    IBSystem *makeSystem(const string & name, const string & type, const string & cfg = "");

    // get a system by name
    IBSystem *getSystem(const string & name);

    // get a system by guid
    IBSystem *getSystemByGuid(uint64_t guid) const;

    // Add a cable connection
    int addCable(const string & t1, const string & n1, const string & p1,
            const string & t2, const string & n2, const string & p2,
            IBLinkWidth width, IBLinkSpeed speed);

    // Parse the cables file and build the fabric
    int parseCables(const string &fn);

    // Parse Topology File according to file name extension
    // Supports ibnetdiscover (ibnd), subnet links (lst), and topo file (topo)
    int parseTopology(const string &fn, bool is_topo_valid = true);

    // Add a link into the fabric - this will create nodes / ports and link between them
    // by calling the forward methods makeNode + setNodePortsystem + makeLinkBetweenPorts
    int addLink(const string & type1, phys_port_t numPorts1,
            uint64_t sysGuid1, uint64_t nodeGuid1,  uint64_t portGuid1,
            int vend1, device_id_t devId1, int rev1, const string & desc1,
            lid_t lid1, uint8_t lmc1, phys_port_t portNum1,
            string type2, phys_port_t numPorts2,
            uint64_t sysGuid2, uint64_t nodeGuid2,  uint64_t portGuid2,
            int vend2, device_id_t devId2, int rev2, const string & desc2,
            lid_t lid2, uint8_t lmc2, phys_port_t portNum2,
            IBLinkWidth width, IBLinkSpeed speed, IBPortState portState);


    int renameNode(IBNode *p_node, string desc, string& error);

    // create a new node in fabric (don't check if exists already), also create system if required
    IBNode * makeNode(IBNodeType type, phys_port_t numPorts,
                    uint64_t sysGuid, uint64_t nodeGuid,
                    int vend, device_id_t devId, int rev, string desc, bool should_be_new = false);

    // set the node's port given data (create one of does not exist).
    IBPort * setNodePort(IBNode * p_node, uint64_t portGuid,
			 lid_t lid, uint8_t limc, phys_port_t portNum,
			 IBLinkWidth width, IBLinkSpeed speed, IBPortState port_state);

    // Add a link between the given ports.
    // not creating sys ports for now.
    int makeLinkBetweenPorts(IBPort *p_port1, IBPort *p_port2);

    int getFileVersion(ifstream &f, u_int16_t &fileVersion);

    // Parse the OpenSM subnet.lst file and build the fabric from it.
    int parseSubnetLinks (string fn, const char * ignore_desc_pattern = NULL);

    // Parse ibnetdiscover file and build the fabric from it.
    int parseIBNetDiscover (string fn);

    // Parse topo file and build the fabric from it.
    int parseTopoFile(const string &fn);

    // Parse OpenSM FDB dump file
    int parseFdbFile(string fn);

    // Parse PSL mapping
    int parsePSLFile(const string& fn);

    // Parse SLVL mapping
    int parseSLVLFile(const string& fn);

    // Parse an OpenSM MCFDBs file and set the MFT table accordingly
    int parseMCFdbFile(string fn);

    int parseVL2VLFile(string fn);

    int parseARFile(string fn);
    int parseFARFile(string fn);

    int parseEPFFile(string fn);
    int parseRailFilterFile(string fn);

    int parsePortHierarchyInfoFile(const string & fn);

    int collectAportData();

    int parseFLIDFile(const string& fn);

    int parsePLFTFile(string fn);

    int parseSADumpFile(string fn);
    //parse file that maps node name to guid
    //int parseApplyNodeNameMapFile(string fn);
    int parseNodeNameMapFile(string fn);

    // set a lid port
    void setLidPort(lid_t lid, IBPort *p_port);
    void setLidVPort(lid_t lid, IBVPort *p_vport);
    void UnSetLidVPort(lid_t lid);

    int getMaxPlanesNumber() const;

    // get a port by lid and prefered plane
    IBPort *getPortByLid(lid_t lid, int preferedPlane);
    // get a port by lid
    IBPort *getPortByLid(lid_t lid);

    // get a port by vlid
    inline IBVPort *getVPortByLid(lid_t lid) {
        if ( VPortByLid.empty() || (VPortByLid.size() < (unsigned)lid+1))
            return NULL;
        return (VPortByLid[lid]);
    };

    //set number of SLs
    inline void setNumSLs(uint8_t nSL) {
        numSLs=nSL;
    };

    //get number of SLs
    inline uint8_t getNumSLs() const {
        return numSLs;
    };

    //set number of VLs
    inline void setNumVLs(uint8_t nVL) {
        numVLs=nVL;
    };

    //get number of VLs
    inline uint8_t getNumVLs() const {
        return numVLs;
    };

    // dump out the contents of the entire fabric
    void dump(ostream &sout) const;

    // write out a topology file
    int dumpTopology(const OutputControl::Identity &identity);

    // write out a LST file
    int dumpLSTFile(ostream &sout, bool write_with_lmc);

    // Write out the name to guid and LID map
    int dumpNameMap(const char *fileName);

    //finish systems construction, create IBSystem ports and check sys type
    void constructSystems();

    IBSystem* getSystemTemplate(map_str_psys &systemTemplates,
                                IBSystem *p_system);

    inline static const string& GetTimestamp() { return IBFabric::timestamp; }

    static void GetSwitchLabelPortNumExplanation(ostream& stream, const string& prefix = "# ");
    static void WriteFileHeader(ostream& stream, const char * header_line_prefix);
    static vector<string> getFilesByPattern(const string& pattern);

    static string GetNowTimestamp();

    //Returns: SUCCESS_CODE / ERR_CODE_IO_ERR
    static int GetFileTimestamp(char *timestamp_buf, size_t size, const string &file_path);

    //Returns: SUCCESS_CODE / ERR_CODE_IO_ERR
    static int OpenFile(const char *file_name,
                        ofstream& sout,
                        bool to_append,
                        string & err_message,
                        const char * header_line_prefix = NULL,
                        ios_base::openmode mode = ios_base::out);

    static int OpenFile(const OutputControl::Identity & identity,
                        ofstream& sout,
					    string & out_filename,
                        bool to_append,
                        string & err_message,
                        const char * header_line_prefix = NULL,
                        ios_base::openmode mode = ios_base::out);

    void setPLFTEnabled() { pLFTEnabled = true; }
    bool isPLFTEnabled() const { return pLFTEnabled; }

    int parseHealthyPortsPolicyFile(map_guid_to_ports &exclude_ports,
                                    const string &file_name,
                                    bool switch_action, bool ca_action);
    int markOutUnhealthyPorts(int &unhealthy_ports,
                              const map_guid_to_ports &exclude_ports,
                              const string &file_name);

    int parseScopePortGuidsFile(const string& fn,
                                bool include_in_scope,
                                int &num_of_lines);
    int markInScopeNodes(IBScope &in_scope);
    int markOutScopeNodes(IBScope &out_scope);
    int applySubCluster();

    IBVNode * makeVNode(uint64_t guid,
                        virtual_port_t num_vports,
                        IBVPort *p_vport,
                        virtual_port_t local_vport_num);

    IBVPort *makeVPort(IBPort *p_port, virtual_port_t vport_num,
                       uint64_t guid, IBPortState vport_state);

    void SetDefaultSL(u_int8_t sl) {
        defaultSL = sl;
        IBNode::setMaxSL(sl);
    }
    u_int8_t GetDefaultSL() const {
        return defaultSL;
    }

    inline phys_port_t GetMaxNodePorts() const {
        return maxNodePorts;
    }

    inline void SetRoutingEngine(IBRoutingEngine routing_engine) {
        this->routing_engine = routing_engine;
    }
    inline IBRoutingEngine GetRoutingEngine() const { return this->routing_engine; }

    inline void SetIsSMDBApplied(bool is_smdb_applied) {
        this->is_smdb_applied = is_smdb_applied;
    }
    inline bool IsSMDBApplied() const { return this->is_smdb_applied; }

    void markNodesAsSpecialByDescriptions();

    /**
     * @brief creates system and node GUID. intended for use only
     *        for fabric created from .topo file.
     *
     * @param output - pointer to an output stream. null means no output stream.
     * @return int - success code.
     */
    int AllocateFabricNodeGuids();

    inline void SetIsEPFValid(bool is_epf_valid) { 
        this->is_epf_valid = is_epf_valid; 
    }
    inline bool IsEPFValid() const { return this->is_epf_valid; }

private:
    bool setRemoteFLIDs(const string &range, IBNode *p_node);
    void removeWhiteSpaces(string& desc);
    int remapNode(IBNode* p_Node, const string& newNodeName);
    int remapSystem(IBNode* p_Node, const string& newNodeName,
                    const  string& newSystemName, const string& newSystemType, bool mlx_nd_format);
    int  removeOldDescription(IBNode* p_Node);

    int parseSubnetLine(char *line);
    int constructGeneralSystem(IBSystem *p_system);
    int constructGeneralSystemNode(IBSystem *p_system, IBNode *p_node);
    int constructSystem(IBSystem *p_system,
                        IBSystem *p_systemTemplate, bool isHCA);
    int constructSystemNode(IBNode *p_node, IBNode *p_nodeT,
                            set_nodes_ptr &missingNodesPtr,
                            map_template_pnode &systemToTemplateMap,
                            bool &isModSys, bool &isNewSys);

    int parseCommaSeperatedValues(
        const string &line, vector<u_int32_t> &vecRes);
    void parseFARSwitchOld(rexMatch *p_rexRes, int &anyErr, IBNode *p_node);
    /*
     * This function assumes sys1, p1 is and APORT.
     */
    int addAPortCable(IBSystem* sys1, string p1,
                      IBSystem* sys2, string p2,
                      IBLinkWidth width, IBLinkSpeed speed);
    int addSysPortCable(IBSystem* sys1, string p1,
                        IBSystem* sys2, string p2,
                        IBLinkWidth width, IBLinkSpeed speed);
    bool parseFARSwitchNew(rexMatch *p_rexRes, int &anyErr, ifstream &f, IBNode *p_node);
    int  parseUnhealthyPortsDumpFile(map_pnode_ports_bitset &node_2_ports,
                                     int &unhealthy_ports,
                                     const map_guid_to_ports &exclude_ports,
                                     const string &file_name);

    static void SetTimestamp();

public:
    bool IsHaveAPorts() const;
};

///////////////////////////////////////////////////////////////////////////////
//
// IB Scope Class
// The scope of nodes to be left or excluded in/from fabric
//
class IBScope {
public:
    //node guid with number ports to be in scope
    map_pnode_ports_bitset    node_ports;
    bool                      is_all_sw;
    bool                      is_all_ca;
    bool                      is_all_routers;

    IBScope(const map_pnode_ports_bitset &nodes, bool all_sw, bool all_ca, bool all_rtr) :
        node_ports(nodes),
        is_all_sw(all_sw),
        is_all_ca(all_ca),
        is_all_routers(all_rtr) {
    }
    ~IBScope() {};
};

// an IBPort accessor is a function that takes an IBPort as an argument,
// and returns some attribute (type T) of that port.
template<typename T>
using ibport_accessor_t = std::function<T(const IBPort *)>;

APort* findAPort(list_p_aport& aports_list, int aport);
APort* findAPort(IBNode* p_node, int aport);

enum APortSymmetryViolations {
    APORT_MISSING_PLANES = 0,
    APORT_NO_AGG_LABEL,
    APORT_UNEQUAL_ATTRIBUTE,
    APORT_NO_VALID_ATTRIBUTE,
    APORT_INVALID_PORT_GUIDS,
    APORT_INVALID_CONNECTION,
    APORT_INVALID_REMOTE_PLANE,
    APORT_INVALID_NUM_PLANES,

    APORT_NUM_VIOLATIONS
};

class APort {
public:
    APort(int num_of_planes, int aport_index);
    const int aport_index;
    vec_pport  ports;
    bitset<APORT_NUM_VIOLATIONS> asymmetry_mask;
    // can be set/unset by programs that iterate the fabric
    bool visited;

    string getAggregatedLabel() const;
    string getName() const;
    string getNodeName() const;
    vec_lids getLids() const;
    bool isFNM1() const;
    uint64_t getSystemGUID() const;
    IBLinkSpeed get_internal_speed() const;
    IBLinkWidth get_internal_width() const {
        return uint2width((unsigned int)(ports.size() - 1));
    }
    IBPortState get_internal_state() const;
    uint64_t guid_get() const;
    APort* get_remote_aport() const;
    // if all planes are on the same node, return it. otherwise, return null.
    IBNode* get_common_node() const;
    // attempts to return a node 
    IBNode* get_any_node() const;

    /**
     * @brief if one or more planes are connected
     */
    bool any_plane_connected() const;
    bool allPlanesConnected() const;
    bool isSymmetric() const { return asymmetry_mask.none(); };

    /**
     * @brief checks if all IBPorts in a list belong to the same
     *        APort. NULL ports are ignored (all NULL list will return true).
     *        If one or more ports in the list do not belong to any APort, return false.
     *        return true only if all non-NULL ports belong to the same APort.
     */
    static bool isSameAPort(const list_p_port& ports);

    /**
     * @brief same as previous function, with node and port numbers as arguments.
     *        This function can only work for APorts with all planes on the same node,
     *        i.e. Crocodile and not BlackMamba.
     */
    static bool isSameAPort(IBNode *p_node, const list_phys_ports& port_nums);

    /**
     * @brief This function takes a list of IBPorts as input,
     *        and returns as output one list of APorts that contain one or more
     *        of the IBPorts in the list,
     *        and another list of the IBPorts from the input list without any
     *        containitng APorts. 
     * 
     * @param input_ports - input list of IBPorts
     * @param containing_aports - output list with every APort that contains one 
     *                            or more of the IBPorts in the input list. elements
     *                            is unique, arranged by the order of contained IBPorts
     *                            in the input list.
     * @param legacy_ports - output list with all the IBPorts in the input list that
     *                       do not belong to any APort. elements are uniqe, ordered
     *                       like the original input list.
     */
    static void splitIBPortListToAPorts(const list_p_port& input_ports,
                                        list_p_aport& containing_aports,
                                        list_p_port& legacy_ports);
    /**
     * @brief Return the number of Aports and legacy ports in a list
     *        (all planes of the same APorts are conted as one)
     */
    static size_t countPortsAggregated(const list_p_port& input_ports);

    /**
     * @brief returns the index of the first NULL IBPort (not including index 0)
     *        or -1 if all IBPorts not NULL.
     */
    int findMissingIBPort() const { 
        auto it = std::find(ports.begin() + 1, ports.end(), nullptr);
        if (it == ports.end()) {
            return -1;
        }

        return static_cast<int>(it - (ports.begin() + 1));
    }

     /**
     * @brief check if at least one IBPort in 'ports' is data worthy
     * 
     * @return true if one or more IBPorts are data worthy,
     *         false if none of them are (or all of them are NULL).
     */
    bool is_data_worthy(bool is_direct_route) const {
        return (std::find_if(ports.begin() + 1, ports.end(),
                [is_direct_route](IBPort* p) {
                    return p && p->is_data_worthy(is_direct_route);
                }) != ports.end());
    }


    /**
     * @brief Check if all defined IBPorts in this aport share
     *        the same return value from the accessor.
     * 
     * @tparam T - The attribute's type
     * @param accessor - function, returns an attribute of type T given an IBPort
     * @return true if the attribute's value is equal for all IBPorts
     *         or if no ports are defined.
     */
    template<class T>
    bool isEqualAttribute(const ibport_accessor_t<T>& accessor) const {
        size_t p, first_defined = 0;

        // skip all NULL ports
        for (p = 1; p < ports.size(); p++) {
            if (ports[p]) {
                break;
            }
        }

        if (p == ports.size()) {
            // No ports are defined, nothing to check
            return true;
        }

        first_defined = p;
        // check if all other defined ibports have the same width
        for (; p < ports.size(); p++) {
            if (!ports[p])
                continue;

            if (accessor(ports[p]) != accessor(ports[first_defined])) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief isEqualAttribute wrapper for getters
     */
    template<class T>
    bool isEqualAttribute(T (IBPort::*getter)() const) const {
        ibport_accessor_t<T> accessor = std::mem_fn(getter);
        return isEqualAttribute(accessor);
    }

    /**
     * @brief return the accessor on the first non-null plane in this aport.
     * 
     * @tparam T - getter return type
     * @param out - output reference
     * @return true - a non null port was found an 'out' was set,
     *         false otherwie.
     */
    template<class T>
    bool getFromFirstPlane(const ibport_accessor_t<T>& accessor, T& out) const {
        size_t p = 0;
        for (p = 1; p < ports.size(); p++)
            if (ports[p])
                break;

        if (p == ports.size())
            // No ports are defined 
            return false;

        out = accessor(ports[p]);
        return true;
    }

    /**
     * @brief return the result of an IBPort accessor if all planes have the same result
     * 
     * @param accessor - the accessor that will be called on each IBPort
     * @param out - output reference
     * @return true  - no plane was missing and all planes return the same accessor result,
     *                 output is set
     */
    template<class T>
    bool getFromAPort(const ibport_accessor_t<T>& accessor, T& out) const {
        if (this->findMissingIBPort() != -1 || !this->isEqualAttribute(accessor))
            return false;
        if (!this->getFromFirstPlane(accessor, out))
            return false;

        return true;
    }

    /**
     * @brief Check if at least one IBPort is defined, and known (it's accessor function
     *        returns a different value than the unknown value). 
     * 
     * @tparam T - Attribute type 
     * @param accessor - function, returns an attribute of type T given an IBPort
     * @param unknown_value - the attribute's default/unknown value
     * @return true if at least one defined IBPort return a known value
     */
    template<class T>
    bool isKnownAttribute(const ibport_accessor_t<T>& accessor, T unknown_value) const {
        for (size_t p = 1; p < ports.size(); p++) {
            if (!ports[p])
                continue;

            if (accessor(ports[p]) != unknown_value) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Get a string with all IBPort attributes given by the accessors
     * 
     * @tparam T - Attribute type 
     * @param accessor - function, returns an attribute of type T given an IBPort
     * @param t_to_str - a pointer to a function that converts T to a string.
     *                   if this pointer is null, directly add the getter's return 
     *                   value to a stringstream.
     * @return a string with the attribute from all IBPorts, stylized as an array 
     */
    template<class T>
    string getAttributeArrayStr(const ibport_accessor_t<T>& accessor, string (*t_to_str)(const T)) const {
        stringstream ss;
        ss << "[";
        for (size_t p = 1; p < ports.size(); p++) {
            if (!ports[p])
                ss << "N/A";
            else
                if (t_to_str != nullptr)
                    ss << t_to_str(accessor(ports[p]));
                else
                    ss << accessor(ports[p]);

            if (p != ports.size() - 1) {
                ss << ", ";
            }
        }

        ss << "]";

        return ss.str();
    }

    /*
     * Wrapper for previous function for getter use.
     */
    template<class T>
    string getAttributeArrayStr(T (IBPort::*getter)() const, string (*t_to_str)(const T)) const {
        // transform the getter to an accessor using std::mem_fn
        ibport_accessor_t<T> accessor = std::mem_fn(getter);
        return getAttributeArrayStr(accessor, t_to_str);
    }

    /**
     * @brief check that all IBPorts in this aport have a
     *        unique getter return value. ignores null ibports.
     * 
     * @param accessor - function, returns an attribute of type T given an IBPort
     * @return true if all defined IBPorts have a unique getter return value,
     *         false if 2 or more IBPorts share the same getter return value
     */
    template<class T>
    bool isUniqueAttribute(const ibport_accessor_t<T>& accessor) const {
        std::unordered_set<T> values;
        for (size_t i = 1; i < ports.size(); ++i) {
                if (!ports[i])
                    continue;

                // value is not unique
                if (values.count(accessor(ports[i])))
                    return false;

                values.insert(accessor(ports[i]));
        }

       return true;
    }

    /*
     * Wrapper for previous function for getter use.
     */
    template<class T>
    bool isUniqueAttribute(T (IBPort::*getter)() const) const {
        // transform the getter to an accessor using std::mem_fn
        ibport_accessor_t<T> accessor = std::mem_fn(getter);
        return isUniqueAttribute(accessor);
    }

private:
    mutable string aggregated_label;
    void createAggregatedLabel() const;
};

#endif /* IBDM_FABRIC_H */

