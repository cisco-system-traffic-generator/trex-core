/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
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

#ifndef IBDM_IBDIAGNET_FILE_NAMES_H
#define IBDM_IBDIAGNET_FILE_NAMES_H

#define LST_FILE                 "ibdiagnet2.lst"
#define SM_FILE                  "ibdiagnet2.sm"
#define PM_FILE                  "ibdiagnet2.pm"
#define PM_AGG_FILE              "ibdiagnet2.pm_agg"
#define NODES_INFO_FILE          "ibdiagnet2.nodes_info"
#define UC_FDBS_FILE             "ibdiagnet2.fdbs"
#define MC_FDBS_FILE             "ibdiagnet2.mcfdbs"
#define DEBUG_FILE               "ibdiagnet2.debug"
#define PKEY_FILE                "ibdiagnet2.pkey"
#define AGUID_FILE               "ibdiagnet2.aguid"
#define SLVL_FILE                "ibdiagnet2.slvl"
#define VL2VL_FILE               "ibdiagnet2.vl2vl"
#define PLFT_FILE                "ibdiagnet2.plft"
#define AR_FILE                  "ibdiagnet2.ar"
#define FAR_FILE                 "ibdiagnet2.far"
#define FAR_FLID_FILE            "ibdiagnet2.far_flid"
#define EPF_FILE                 "ibdiagnet2.epf"
#define RAIL_FILTER_FILE         "ibdiagnet2.rail_filter"
#define END_PORT_PF_FILE         "ibdiagnet2.end_port_pf"
#define RN_FILE                  "ibdiagnet2.rn"
#define RNC_FILE_2               "ibdiagnet2.rnc2"
#define RNC_FILE                 "ibdiagnet2.rnc"
#define MLNX_CNTRS_FILE          "ibdiagnet2.mlnx_cntrs"
#define NET_DUMP_FILE            "ibdiagnet2.net_dump"
#define NET_DUMP_AGG_FILE        "ibdiagnet2.net_dump_agg"
#define VPORTS_FILE              "ibdiagnet2.vports"
#define VPORTS_PKEY_FILE         "ibdiagnet2.vports_pkey"
#define SHARP_FILE               "ibdiagnet2.sharp"
#define SHARP_AN_FILE            "ibdiagnet2.sharp_an_info"
#define SHARP_PM_FILE            "ibdiagnet2.sharp_pm"
#define IBNET_DISCOVER_FILE      "ibdiagnet2.ibnetdiscover"
#define RAILS_DUMP_FILE          "ibdiagnet2.rails"
#define APORT_FILE               "ibdiagnet2.aports"
#define DFP_DUMP_FILE            "ibdiagnet2.dfp"
#define FAT_TREE_DUMP_FILE       "ibdiagnet2.fat_tree"
#define FLID_DUMP_FILE           "ibdiagnet2.flid"
#define AR_GROUP_TO_FLID_DUMP_FILE  "ibdiagnet2.arg2flid"
#define COMBINED_CABLE_DUMP_FILE "ibdiagnet2.cables"
#define IBIS_STAT_FILE           "ibdiagnet2.ibis"
#define PCAP_FILE                "ibdiagnet2.pcap"
#define PPCC_FILE                "ibdiagnet2.ppcc"
#define IBLINKINFO_FILE          "ibdiagnet2.iblinkinfo"
#define PORT_HI_INFO_FILE        "ibdiagnet2.port_hi"

// SM mkey files
#define GUID2LID_FILE_NAME      "guid2lid"
#define GUID2MKEY_FILE_NAME     "guid2mkey"
#define NEIGHBORS_FILE_NAME     "neighbors"

// VS Key file
#define GUID2VSKEY_FILE_NAME    "guid2vskey"

// CC Key file
#define GUID2CCKEY_FILE_NAME    "guid2cckey"

// Class 0xC  M2N Key file
#define GUID2_M2N_KEY_FILE_NAME    "guid2_m2n_key"

// Class 0x4 PM Key
#define GUID2_PM_KEY_FILE_NAME    "guid2pmkey"

// default config file
#ifndef DEFAULT_CONFIG_FILE
#define DEFAULT_CONFIG_FILE     "/etc/ibutils2/ibdiag.conf"
#endif

#define LOG_FILE                "ibdiagnet2.log"
#if !defined(_WIN32) && !defined(_WIN64)    /* Linux */
    #define HTML_REPORT_DST_DIR     "html_report/"
    #define IBNL_OUTPUT_DIR         "ibdiag_ibnl/"
#else   /* Windows */
    #define HTML_REPORT_DST_DIR     "html_report\\"
    #define IBNL_OUTPUT_DIR         "ibdiag_ibnl\\"
#endif

/* environment variable names */
#define MAP_FILE_ENV        "IBUTILS_NODE_NAME_MAP_FILE_PATH"
#define CAPABILITY_FILE_ENV "IBDIAG_CAPABILITY_MASK_FILE_PATH"

#endif /* IBDM_IBDIAGNET_FILE_NAMES_H */
