/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBDIAG_H
#define IBDIAG_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <list>
#include <map>
#include <limits>
using namespace std;

#include <infiniband/ibutils/release_containers.h>
#include <infiniband/ibdm/Fabric.h>
#include <ibis/ibis.h>
#include <infiniband/ibis/ibis_common.h>
#include <infiniband/ibdiag/key_updater.h>
#include <infiniband/misc/ini_file/ini_file.h>

#include "ibdiag_progress_bar.h"
#include "ibdiag_ibdm_extended_info.h"
#include "ibdiag_fabric_errs.h"
#include "ibdiag_types.h"
#include "ibdiag_csv_out.h"
#include "ibdiag_fabric.h"
#include "ibdiag_smdb.h"
#include "ibdiag_ppcc.h"
#include "ibdiag_iblinkinfo.h"

#define CHECK_EXT_SPEEDS_COUNTERS_ON_SW  0x01
#define CHECK_EXT_SPEEDS_COUNTERS_ON_ALL 0x02
#define UNSET_EXT_SPEEDS_COUNTERS (~(CHECK_EXT_SPEEDS_COUNTERS_ON_SW | \
                                     CHECK_EXT_SPEEDS_COUNTERS_ON_ALL))
#define PRINT_LLR_COUNTERS               0x04

#define VS_MLNX_CNTRS_PAGE0        0
#define VS_MLNX_CNTRS_PAGE1        1
#define VS_MLNX_CNTRS_PAGE255      255

#define PAGE255_NUM_FIELDS 17

#define PAGE0_LATEST_VER 2
#define PAGE1_LATEST_VER 5
#define PAGE255_LATEST_VER 3

#define MLID_START    0xC000

//Num of port states in block of Switch Port State Table MAD
#define NUM_OF_PORT_STATES_IN_BLOCK 128

//K_FCODE - this is a coefficient to raw ber when RS-FEC is active
#define K_FCODE              2

#define VPORT_STATE_BLOCK_SIZE     128

#define DEFAULT_BER_THRESHOLD                       0xe8d4a51000ULL     /* pow(10,12) */
#define DEFAULT_BER_THRESHOLD_STR                   "10^-12"             /* pow(10,12) */
#define DEAFULT_NODES_INFO_MADS_IN_PACK             300

/****************PM PER SL/VL CNTRS******************/
#define PORT_RCV_DATA_VL_HEADER             "PortRcvDataVL"
#define PORT_RCV_DATA_VL_HEADER_CSV         "PORT_RCV_DATA_VL"

#define PORT_XMIT_DATA_VL_HEADER            "PortXmitDataVL"
#define PORT_XMIT_DATA_VL_HEADER_CSV        "PORT_XMIT_DATA_VL"

#define PORT_RCV_DATA_VL_EXT_HEADER         "PortRcvDataVLExt"
#define PORT_RCV_DATA_VL_EXT_HEADER_CSV     "PORT_RCV_DATA_VL_EXT"

#define PORT_XMIT_DATA_VL_EXT_HEADER        "PortXmitDataVLExt"
#define PORT_XMIT_DATA_VL_EXT_HEADER_CSV    "PORT_XMIT_DATA_VL_EXT"

#define PORT_RCV_PKT_VL_HEADER              "PortRcvPktVL"
#define PORT_RCV_PKT_VL_HEADER_CSV          "PORT_RCV_PKT_VL"

#define PORT_XMIT_PKT_VL_HEADER             "PortXmitPktVL"
#define PORT_XMIT_PKT_VL_HEADER_CSV         "PORT_XMIT_PKT_VL"

#define PORT_RCV_PKT_VL_EXT_HEADER          "PortRcvPktVLExt"
#define PORT_RCV_PKT_VL_EXT_HEADER_CSV      "PORT_RCV_PKT_VL_EXT"

#define PORT_XMIT_PKT_VL_EXT_HEADER         "PortXmitPktVLExt"
#define PORT_XMIT_PKT_VL_EXT_HEADER_CSV     "PORT_XMIT_PKT_VL_EXT"

#define PORT_XMIT_WAIT_VL_EXT_HEADER         "PortXmitWaitVLExt"
#define PORT_XMIT_WAIT_VL_EXT_HEADER_CSV     "PORT_XMIT_WAIT_VL_EXT"

#define PORT_XMIT_DATA_SL_HEADER            "PortXmitDataSL"
#define PORT_XMIT_DATA_SL_HEADER_CSV        "PORT_XMIT_DATA_SL"

#define PORT_RCV_DATA_SL_HEADER             "PortRcvDataSL"
#define PORT_RCV_DATA_SL_HEADER_CSV         "PORT_RCV_DATA_SL"

#define PORT_XMIT_DATA_SL_EXT_HEADER        "PortXmitDataSLExt"
#define PORT_XMIT_DATA_SL_EXT_HEADER_CSV    "PORT_XMIT_DATA_SL_EXT"

#define PORT_RCV_DATA_SL_EXT_HEADER         "PortRcvDataSLExt"
#define PORT_RCV_DATA_SL_EXT_HEADER_CSV     "PORT_RCV_DATA_SL_EXT"

#define PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS_HEADER         "PortVLXmitFlowCtlUpdateErrors"
#define PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS_HEADER_CSV     "PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS"

#define PORT_XMIT_WAIT_VL_HEADER             "PortXmitWaitVL"
#define PORT_XMIT_WAIT_VL_HEADER_CSV         "PORT_XMIT_WAIT_VL"

#define PORT_XMIT_CON_CTRL_HEADER            "PortXmitConCtrl"
#define PORT_XMIT_CON_CTRL_HEADER_CSV        "PORT_XMIT_CON_CTRL"

#define PORT_VL_XMIT_TIME_CONG_HEADER        "PortVLXmitTimeCong"
#define PORT_VL_XMIT_TIME_CONG_HEADER_CSV    "PORT_VL_XMIT_TIME_CONG"

#define PORT_VL_XMIT_TIME_CONG_EXT_HEADER        "PortVLXmitTimeCongExt"
#define PORT_VL_XMIT_TIME_CONG_EXT_HEADER_CSV    "PORT_VL_XMIT_TIME_CONG_EXT"

//Capability check bit
#define NOT_SUPPORT_XMIT_WAIT                               0x1
#define NOT_SUPPORT_EXT_PORT_COUNTERS                       0x2
#define NOT_SUPPORT_EXT_SPEEDS_COUNTERS                     0x4
#define NOT_SUPPORT_LLR_COUNTERS                            0x8
#define NOT_SUPPORT_EXT_SPEEDS_RSFEC_COUNTERS               0x10
#define NOT_SUPPORT_PORT_INFO_EXTENDED                      0x20
#define PM_PER_SLVL_VS_CLASS_PORT_RCV_DATA_VL               0x40
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_DATA_VL              0x80
#define PM_PER_SLVL_VS_CLASS_PORT_RCV_PKTS_VL               0x100
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_PKTS_VL              0x200
#define PM_PER_SLVL_VS_CLASS_PORT_RCV_DATA_VL_EXT           0x400
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_DATA_VL_EXT          0x800
#define PM_PER_SLVL_VS_CLASS_PORT_RCV_PKTS_VL_EXT           0x1000
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_PKTS_VL_EXT          0x2000
#define PM_PER_SLVL_PM_CLASS_PORT_XMIT_DATA_SL              0x4000
#define PM_PER_SLVL_PM_CLASS_PORT_RCV_DATA_SL               0x8000
#define PM_PER_SLVL_PM_CLASS_PORT_XMIT_DATA_SL_EXT          0x10000
#define PM_PER_SLVL_PM_CLASS_PORT_RCV_DATA_SL_EXT           0x20000
#define NOT_SUPPORT_PORT_RCV_ERROR_DETAILS                  0x40000
#define NOT_SUPPORT_PORT_XMIT_DISCARD_DETAILS               0x80000
#define NOT_SUPPORT_SPECIAL_PORTS_MARKING_CHECKED           0x100000
#define NOT_SUPPORT_PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS     0x200000
#define NOT_SUPPORT_HIERARCHY_INFO                          0x400000
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_WAIT_VL              0x800000
#define PM_PER_SLVL_VS_CLASS_PORT_XMIT_WAIT_VL_EXT          0x1000000
// Fast Recovery
#define NOT_SUPPORT_FR_COUNTERS                             0x2000000
#define NOT_SUPPORT_FR_PROFILES_CONFIG                      0x4000000
#define NOT_SUPPORT_FR_CREDIT_WATCHDOG_CONFIG               0x8000000
#define NOT_SUPPORT_FR_BER_CONFIG                           0x10000000
// Entry Plane Filter
#define NOT_SUPPORT_ENTRY_PLANE_FILTER                      0x20000000
// CC counters
#define NOT_SUPPORT_PORT_XMIT_CON_CTRL                      0x40000000
#define NOT_SUPPORT_PORT_VL_XMIT_TIME_CONG                  0x80000000
// Rail Filter
#define NOT_SUPPORT_RAIL_FILTER                             0x100000000
#define PM_PER_SLVL_VS_CLASS_PORT_VL_XMIT_TIME_CONG_EXT     0x200000000

#define NOT_SUPPORT_PORT_GENERAL_COUNTERS                   0x400000000

#define NOT_SUPPORT_PORT_RECOVERY_POLICY_COUNTERS           0x800000000


#define IS_SUPPORT_EXT_PORT_FAILED(appData) \
    (appData & NOT_SUPPORT_EXT_PORT_COUNTERS)
#define IS_SUPPORT_EXT_SPEEDS_FAILED(appData) \
    (appData & NOT_SUPPORT_EXT_SPEEDS_COUNTERS)
#define IS_SUPPORT_EXT_SPEEDS_RSFEC_FAILED(appData)\
    (appData & NOT_SUPPORT_EXT_SPEEDS_RSFEC_COUNTERS)
#define IS_SUPPORT_LLR_FAILED(appData1) \
    (appData1 & NOT_SUPPORT_LLR_COUNTERS)
#define IS_SUPPORT_PORT_INFO_EXTENDED_FAILED(appData)\
    (appData & NOT_SUPPORT_PORT_INFO_EXTENDED)
#define IS_SUPPORT_PORT_RCV_ERROR_DETAILS(appData) \
    (appData & NOT_SUPPORT_PORT_RCV_ERROR_DETAILS)
#define IS_SUPPORT_PORT_XMIT_DISCARD_DETAILS(appData) \
    (appData & NOT_SUPPORT_PORT_XMIT_DISCARD_DETAILS)
#define IS_SUPPORT_HIERARCHY_INFO_FAILED(appData) \
    (appData & NOT_SUPPORT_HIERARCHY_INFO)
// Fast Recovery
#define IS_SUPPORT_FR_COUNTERS_FAILED(appData) \
    (appData & NOT_SUPPORT_FR_COUNTERS)
#define IS_SUPPORT_FR_PROFILES_CONFIG_FAILED(appData) \
    (appData & NOT_SUPPORT_FR_PROFILES_CONFIG)
#define IS_SUPPORT_FR_CREDIT_WATCHDOG_CONFIG_FAILED(appData) \
    (appData & NOT_SUPPORT_FR_CREDIT_WATCHDOG_CONFIG)
#define IS_SUPPORT_FR_BER_CONFIG_FAILED(appData) \
    (appData & NOT_SUPPORT_FR_BER_CONFIG)
#define IS_SUPPORT_PORT_GENERAL_COUNTERS_FAILED(appData) \
    (appData & NOT_SUPPORT_PORT_GENERAL_COUNTERS)
#define IS_SUPPORT_PORT_RECOVERY_POLICY_COUNTERS_FAILED(appData) \
    (appData & NOT_SUPPORT_PORT_RECOVERY_POLICY_COUNTERS)



// Entry Plane Filter
#define IS_SUPPORT_ENTRY_PLANE_FILTER_FAILED(appData) \
    (appData & NOT_SUPPORT_ENTRY_PLANE_FILTER)

// Rail Filter
#define IS_SUPPORT_RAIL_FILTER_FAILED(appData) \
    (appData & NOT_SUPPORT_RAIL_FILTER)

//data1 set bit if not support cap
#define PM_PER_SLVL_ATTR_IS_NOT_SUPPORT_CAPABILTY(p_curr_node, attr_id_cap_bit) \
    IS_APP_DATA_BIT_SET(p_curr_node->appData1.val, attr_id_cap_bit)

//data2 set bit if support check
#define PM_PER_SLVL_ATTR_IS_SUPPORT_NOT_CHECKED_CAPABILTY(p_curr_node, attr_id_cap_bit) \
    !IS_APP_DATA_BIT_SET(p_curr_node->appData2.val, attr_id_cap_bit)

#define IS_SUPPORT_CC_SLVL_CNTR_FAILED(p_curr_node, attr_id_cap_bit) \
    IS_APP_DATA_BIT_SET(p_curr_node->appData1.val, attr_id_cap_bit)

#define IS_APP_DATA_BIT_SET(appdata, attr_id_cap_bit) \
    ((appdata & attr_id_cap_bit) ? true : false)

#define PORT_INFO_EXT_FEC_MODE_SUPPORT(capability) \
    ((capability) & 0x1)

#define EXT_FEC_MODE_CONTROL_AND_REPORT_SUPPORT(capability)\
    ((capability) & 0x10)

/****************************************************/
#define PER_SLVL_CNTR_SIZE_2    2
#define PER_SLVL_CNTR_SIZE_16   16
#define PER_SLVL_CNTR_SIZE_32   32
#define PER_SLVL_CNTR_SIZE_64   64

struct slvl_data_sort {
        inline bool operator () (const pair_ibport_slvl_cntr_data_t &lsvd,
                                 const pair_ibport_slvl_cntr_data_t &rsvd) const {
            return (lsvd.first->createIndex < rsvd.first->createIndex);
        }
};

union IBDiagSLVLCntrsData {
       u_int32_t data32[16];
       struct uint64bit data64[16];
};

class CountersPerSLVL {
protected:
    u_int32_t m_attr_id;
    bool m_is_vs_class;
    bool m_is_per_vl;
    bool m_is_ext_cntrs;
    u_int64_t m_cap_bit;
    string m_header;
    string m_csv_header;
    u_int32_t m_cntr_size;
    uint32_t m_num_fields;

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) = 0;

    void Unpack2(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                 const uint8_t *buff) {

        struct RawData_PM_PortRcvXmitCntrsSlVl32 data32;
        CLEAR_STRUCT(data32);

        uint32_t buff32 = *((const uint32_t*)buff);

        for (uint8_t i = 0; i < 16; ++i) {
            data32.DataVLSL32[i] = buff32 & 0x3;
            buff32 >>= 2;

            /* todo - check if this is the right one:
            data32.DataVLSL32[i] = buff32 & 0xc000;
            buff32 <<= 2;
            */
        }

        memcpy(ibdiag_slvl_cntrs_data.data32,
               data32.DataVLSL32,
               sizeof(ibdiag_slvl_cntrs_data.data32));
    }

    void Unpack16(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                  const uint8_t *buff) {
        struct RawData_PM_PortRcvXmitCntrsSlVl16 data;
        CLEAR_STRUCT(data);
        RawData_PM_PortRcvXmitCntrsSlVl16_unpack(&data, buff);

        for(int i=0; i < 16; ++i)
            ibdiag_slvl_cntrs_data.data32[i] = data.DataVLSL16[i];
    }

    void Unpack32(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                  const uint8_t *buff) {
        struct RawData_PM_PortRcvXmitCntrsSlVl32 data32;
        CLEAR_STRUCT(data32);
        RawData_PM_PortRcvXmitCntrsSlVl32_unpack(&data32, buff);

        memcpy(ibdiag_slvl_cntrs_data.data32,
               data32.DataVLSL32,
               sizeof(ibdiag_slvl_cntrs_data.data32));
    }

    void Unpack64SL(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                    const uint8_t *buff) {
        struct RawData_PM_PortRcvXmitCntrsSl64 data64;
        CLEAR_STRUCT(data64);
        RawData_PM_PortRcvXmitCntrsSl64_unpack(&data64, buff);

        memcpy(ibdiag_slvl_cntrs_data.data64,
               data64.DataVLSL64,
               sizeof(ibdiag_slvl_cntrs_data.data64));
    }

    void Unpack64VL(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                    const uint8_t *buff) {
        struct RawData_PM_PortRcvXmitCntrsVl64 data64;
	CLEAR_STRUCT(data64);
        RawData_PM_PortRcvXmitCntrsVl64_unpack(&data64, buff);

        memcpy(ibdiag_slvl_cntrs_data.data64,
               data64.DataVLSL64,
               sizeof(ibdiag_slvl_cntrs_data.data64));
    }

    void Dump(u_int32_t data[], size_t arrsize, u_int8_t operationalVLs, stringstream &sstream);
    void Dump(struct uint64bit data[], size_t arrsize, u_int8_t operationalVLs, stringstream &sstream);

public:
    set_port_data_update_t m_set_port_data_update;

    CountersPerSLVL(u_int32_t attr_id, bool is_vs_class, bool is_per_vl,
                    bool is_ext_cntrs, u_int64_t cap_bit, string header, string csv_header,
                    u_int32_t cntr_size, uint32_t num_fields = IB_NUM_SL) :
                        m_attr_id(attr_id), m_is_vs_class(is_vs_class), m_is_per_vl(is_per_vl),
                        m_is_ext_cntrs(is_ext_cntrs), m_cap_bit(cap_bit), m_header(header),
                        m_csv_header(csv_header), m_cntr_size(cntr_size), m_num_fields(num_fields) {}

    virtual ~CountersPerSLVL() { release_container_data(m_set_port_data_update); }

    inline u_int32_t GetAttrId() const { return m_attr_id; }
    inline bool IsVSClass() const { return m_is_vs_class; }
    inline bool IsExtCntrs() const {return m_is_ext_cntrs; }
    inline u_int64_t GetAttrCapBit() const { return m_cap_bit; }
    inline string GetCntrHeader() const { return m_header; }
    inline string GetCSVSectionHeader() const { return m_csv_header; }
    inline uint32_t GetNumFields() const { return m_num_fields; }
    void DumpSLVLCountersHeader(CSVOut &csv_out);
    void DumpSLVLCountersData(CSVOut &csv_out, IBDMExtendedInfo &fabric_extended_info);
};

//VS CLASS
class PortRcvDataVL: public CountersPerSLVL {
public:
    PortRcvDataVL(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_RCV_DATA_VL,
                                     true/*VS Class*/, true/*Per VL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_RCV_DATA_VL,
                                     PORT_RCV_DATA_VL_HEADER,
                                     PORT_RCV_DATA_VL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortRcvDataVL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvDataVL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};

class PortXmitDataVL: public CountersPerSLVL {
public:
    PortXmitDataVL(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_XMIT_DATA_VL,
                                     true/*VS Class*/, true/*Per VL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_XMIT_DATA_VL,
                                     PORT_XMIT_DATA_VL_HEADER,
                                     PORT_XMIT_DATA_VL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortXmitDataVL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitDataVL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};

class PortRcvDataVLExt: public CountersPerSLVL {
public:
    PortRcvDataVLExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_RCV_DATA_VL_EXT,
                                     true/*VS Class*/, true/*Per VL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_RCV_DATA_VL_EXT,
                                     PORT_RCV_DATA_VL_EXT_HEADER,
                                     PORT_RCV_DATA_VL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortRcvDataVLExt() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvDataVLExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff); }

};

class PortXmitDataVLExt: public CountersPerSLVL {
public:
    PortXmitDataVLExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_XMIT_DATA_VL_EXT,
                                     true/*VS Class*/, true/*Per VL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_XMIT_DATA_VL_EXT,
                                     PORT_XMIT_DATA_VL_EXT_HEADER,
                                     PORT_XMIT_DATA_VL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortXmitDataVLExt() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitDataVLExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff); }

};

class PortRcvPktVL: public CountersPerSLVL {
public:
    PortRcvPktVL(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_RCV_PKT_VL,
                                     true/*VS Class*/, true/*Per VL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_RCV_PKTS_VL,
                                     PORT_RCV_PKT_VL_HEADER,
                                     PORT_RCV_PKT_VL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortRcvPktVL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvPktVL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};

class PortXmitPktVL: public CountersPerSLVL {
public:
    PortXmitPktVL(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_XMIT_PKT_VL,
                                     true/*VS Class*/, true/*Per VL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_XMIT_PKTS_VL,
                                     PORT_XMIT_PKT_VL_HEADER,
                                     PORT_XMIT_PKT_VL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortXmitPktVL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitPktVL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};

class PortRcvPktVLExt: public CountersPerSLVL {
public:
    PortRcvPktVLExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_RCV_PKT_VL_EXT,
                                     true/*VS Class*/, true/*Per VL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_RCV_PKTS_VL_EXT,
                                     PORT_RCV_PKT_VL_EXT_HEADER,
                                     PORT_RCV_PKT_VL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortRcvPktVLExt() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvPktVLExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff); }

};

class PortXmitPktVLExt: public CountersPerSLVL {
public:
    PortXmitPktVLExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_XMIT_PKT_VL_EXT,
                                     true/*VS Class*/, true/*Per VL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_XMIT_PKTS_VL_EXT,
                                     PORT_XMIT_PKT_VL_EXT_HEADER,
                                     PORT_XMIT_PKT_VL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortXmitPktVLExt() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitPktVLExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff); }

};

class PortXmitWaitVLExt: public CountersPerSLVL {
public:
        PortXmitWaitVLExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_XMIT_WAIT_VL_EXT,
                                     true/*VS Class*/, true/*Per VL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_VS_CLASS_PORT_XMIT_WAIT_VL_EXT,
                                     PORT_XMIT_WAIT_VL_EXT_HEADER,
                                     PORT_XMIT_WAIT_VL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortXmitWaitVLExt() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitWaitVLExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff); }

};

//PM CLASS
class PortXmitDataSL: public CountersPerSLVL {
public:
    PortXmitDataSL(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_XMIT_DATA_SL,
                                     false/*PM Class*/, false/*Per SL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_PM_CLASS_PORT_XMIT_DATA_SL,
                                     PORT_XMIT_DATA_SL_HEADER,
                                     PORT_XMIT_DATA_SL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortXmitDataSL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitDataSL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};

class PortRcvDataSL: public CountersPerSLVL {
public:
    PortRcvDataSL(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_RCV_DATA_SL,
                                     false/*PM Class*/, false/*Per SL*/, false/*Not ext cntrs*/,
                                     PM_PER_SLVL_PM_CLASS_PORT_RCV_DATA_SL,
                                     PORT_RCV_DATA_SL_HEADER,
                                     PORT_RCV_DATA_SL_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_32) {}
    virtual ~PortRcvDataSL() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvDataSL Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff); }
};


class PortXmitDataSLExt: public CountersPerSLVL {
public:
    PortXmitDataSLExt(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_XMIT_DATA_SL_EXT,
                                     false/*PM Class*/, false/*Per SL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_PM_CLASS_PORT_XMIT_DATA_SL_EXT,
                                     PORT_XMIT_DATA_SL_EXT_HEADER,
                                     PORT_XMIT_DATA_SL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortXmitDataSLExt() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitDataSLExt Counter\n");
        Unpack64SL(ibdiag_slvl_cntrs_data, buff); }

};

class PortRcvDataSLExt: public CountersPerSLVL {
public:
    PortRcvDataSLExt(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_RCV_DATA_SL_EXT,
                                     false/*PM Class*/, false/*Per SL*/, true/*Ext cntrs*/,
                                     PM_PER_SLVL_PM_CLASS_PORT_RCV_DATA_SL_EXT,
                                     PORT_RCV_DATA_SL_EXT_HEADER,
                                     PORT_RCV_DATA_SL_EXT_HEADER_CSV,
                                     PER_SLVL_CNTR_SIZE_64) {}
    virtual ~PortRcvDataSLExt() {}
    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortRcvDataSLExt Counter\n");
        Unpack64SL(ibdiag_slvl_cntrs_data, buff); }

};

// PortVLXmitFlowCtlUpdateErrors
class PortVLXmitFlowCtlUpdateErrors: public CountersPerSLVL {
public:
	PortVLXmitFlowCtlUpdateErrors(): CountersPerSLVL(
	                                     IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS,
	                                     false /*PM (not VS) Class*/,
	                                     true  /*Per VL*/,
	                                     false /*not Ext cntrs*/,
	                                     NOT_SUPPORT_PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS,
	                                     PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS_HEADER,
	                                     PORT_VL_XMIT_FLOW_CTL_UPDATE_ERRORS_HEADER_CSV,
	                                     PER_SLVL_CNTR_SIZE_2) {}

    virtual ~PortVLXmitFlowCtlUpdateErrors() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortVLXmitFlowCtlUpdateErrors Counter\n");
        Unpack2(ibdiag_slvl_cntrs_data, buff);
    }
};

class PortXmitWaitVL: public CountersPerSLVL {
public:
        PortXmitWaitVL(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_VL_XMIT_WAIT,
                                          false/*PM (not VS) Class*/, true/*Per VL*/, false/*Not ext cntrs*/,
                                          PM_PER_SLVL_VS_CLASS_PORT_XMIT_WAIT_VL,
                                          PORT_XMIT_WAIT_VL_HEADER,
                                          PORT_XMIT_WAIT_VL_HEADER_CSV,
                                          PER_SLVL_CNTR_SIZE_16) {}
    virtual ~PortXmitWaitVL() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff) {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortXmitWaitVL Counter\n");
        Unpack16(ibdiag_slvl_cntrs_data, buff); }
};

// CC counters
class PortXmitConCtrl: public CountersPerSLVL {
public:
    PortXmitConCtrl(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_XMIT_CON_CTRL,
                                       false, // PM Class
                                       true,  // Per VL
                                       false, // Not ext cntrs
                                       NOT_SUPPORT_PORT_XMIT_CON_CTRL,
                                       PORT_XMIT_CON_CTRL_HEADER,
                                       PORT_XMIT_CON_CTRL_HEADER_CSV,
                                       PER_SLVL_CNTR_SIZE_32,
                                       1 /* only 1 counter */ ) {}
    virtual ~PortXmitConCtrl() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data, const uint8_t *buff) {

        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack CC PortXmitConCtrl Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff);
    }
};

class PortVLXmitTimeCong: public CountersPerSLVL {
public:
    PortVLXmitTimeCong(): CountersPerSLVL(IBIS_IB_ATTR_PERF_MANAGEMENT_PORT_VL_XMIT_TIME_CONG,
                                          false, // PM Class
                                          true,  // Per VL
                                          false, // Not ext cntrs
                                          NOT_SUPPORT_PORT_VL_XMIT_TIME_CONG,
                                          PORT_VL_XMIT_TIME_CONG_HEADER,
                                          PORT_VL_XMIT_TIME_CONG_HEADER_CSV,
                                          PER_SLVL_CNTR_SIZE_32,
                                          15) {}
    virtual ~PortVLXmitTimeCong() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data, const uint8_t *buff) {

        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack CC PortVLXmitTimeCong Counter\n");
        Unpack32(ibdiag_slvl_cntrs_data, buff);
    }
};

class PortVLXmitTimeCongExt: public CountersPerSLVL {
public:

    PortVLXmitTimeCongExt(): CountersPerSLVL(IBIS_IB_ATTR_VENDOR_SPEC_MELLANOX_PORT_VL_XMIT_TIME_CONG_EXT,
                                             true, // VS Class
                                             true, // Per VL
                                             true, // Ext cntrs
                                             PM_PER_SLVL_VS_CLASS_PORT_VL_XMIT_TIME_CONG_EXT,
                                             PORT_VL_XMIT_TIME_CONG_EXT_HEADER,
                                             PORT_VL_XMIT_TIME_CONG_EXT_HEADER_CSV,
                                             PER_SLVL_CNTR_SIZE_64) {}

    virtual ~PortVLXmitTimeCongExt() {}

    virtual void Unpack(union IBDiagSLVLCntrsData &ibdiag_slvl_cntrs_data,
                        const uint8_t *buff)
    {
        IBDIAG_LOG(TT_LOG_LEVEL_INFO, "Unpack PM PortVLXmitTimeCongExt Counter\n");
        Unpack64VL(ibdiag_slvl_cntrs_data, buff);
    }
};


/****************************************************/
string get_ibdiag_version();
u_int8_t get_operational_vl_num(u_int8_t opvl_code);
/****************************************************/

typedef vector<rn_gen_string_tbl > vec_rn_gen_string;
typedef vector<vec_rn_gen_string > vec_vec_rn_gen_string;


struct AdditionalRoutingData {

    IBNode *p_node;
    direct_route_t *p_direct_route;

    adaptive_routing_info ar_info;
    bool isRNSupported(){
        return (ar_info.is_arn_sup || ar_info.is_frn_sup);
    }

    static u_int16_t sw_supports_rn_count;

    //for each MAD vector
    //top is the max index for the current node
    //max is the max index in the fabric

    //// RNSubGroupDirectionTable
    u_int16_t top_sub_group_direction_block;
    static u_int16_t max_sub_group_direction_block;
    // index sub_group_direction_block
    vector <rn_sub_group_direction_tbl> sub_group_direction_table_vec;

    //// RNGenStringTable
    u_int8_t top_direction_block;
    static u_int8_t max_direction_block;
    static u_int8_t max_plft;
    //index: [pLFT][direction_block]
    vec_vec_rn_gen_string gen_string_table_vec;

    //// RNGenBySubGroupPriority
    rn_gen_by_sub_group_prio gen_by_sub_group_priority;

    //// RNRcvString
    u_int16_t top_string_block;
    static u_int16_t max_string_block;
    //index: string block
    vector<rn_rcv_string> rcv_string_vec;

    //// RNXmitPortMask
    u_int8_t top_ports_block;
    static u_int8_t max_ports_block;
    //index: ports block
    vector <rn_xmit_port_mask> xmit_port_mask_vec;

    //index: group table block
    vector <ib_ar_grp_table> group_table_vec;
    u_int16_t top_group_table_block;
    vector <ib_ar_linear_forwarding_table_sx> ar_lft_table_vec[MAX_PLFT_NUM];
    u_int16_t top_ar_lft_table_block;

    AdditionalRoutingData()
        : p_node(NULL), p_direct_route(NULL), ar_info({0}), top_sub_group_direction_block(0), top_direction_block(0),
          gen_by_sub_group_priority({0}), top_string_block(0), top_ports_block(0), top_group_table_block(0), top_ar_lft_table_block(0)
    {}

    void AddGroupTable(u_int16_t group_table_block,
                       ib_ar_grp_table *p_group_table){
        if (group_table_vec.size() <= group_table_block) {
            group_table_vec.resize(group_table_block + 100);
        }

        top_group_table_block = max(top_group_table_block, group_table_block);
        memcpy(&group_table_vec[group_table_block], p_group_table,
               sizeof (ib_ar_grp_table));
    }
    void AddARLFT(u_int8_t pLFT,
                  u_int16_t block,
                  ib_ar_linear_forwarding_table_sx *p_ar_lft){
        if (ar_lft_table_vec[pLFT].size() <= block) {
            ar_lft_table_vec[pLFT].resize(block + 100);
        }

        top_ar_lft_table_block = max(top_ar_lft_table_block, block);
        memcpy(&ar_lft_table_vec[pLFT][block], p_ar_lft, sizeof (*p_ar_lft));
    }

    //WHBF -- groups weights
    class weights {
	public:
        weights(): sg_weights(3, -1) {}
        void set(const group_weights& in) {
            sg_weights[2] = in.sg2_weight;
            sg_weights[1] = in.sg1_weight;
            sg_weights[0] = in.sg0_weight;
        }

        int get(u_int16_t index) const {
            if (index >= sg_weights.size())
                return -1;

            return sg_weights[index];
        }

    private:
	    vector<int> sg_weights;
    };
    vector<weights> group_weights_vec;
    void AddSubGroupWeights(u_int8_t block_index, const whbf_config& weights_config);
    int GetSubGroupWeight(u_int16_t group, u_int16_t subgroup);
};

struct AdditionalRoutingDataCompare {
    bool operator()(const IBNode *lhs, const IBNode *rhs) const {
        return lhs->guid_get() < rhs->guid_get();
    }
};

typedef map <IBNode *, AdditionalRoutingData, AdditionalRoutingDataCompare> AdditionalRoutingDataMap;
typedef AdditionalRoutingDataMap::iterator AdditionalRoutingDataMapIter;
typedef pair <AdditionalRoutingDataMapIter, bool> AdditionalRoutingDataMapInsertRes;

typedef vector < PCI_LeafSwitchInfo > PCI_LeafSwitchesInfoVec;

typedef list<string> warnings_list;
typedef map < APort *, vec_p_fabric_err > map_aport_vec_p_fabric_err;

typedef map < IBPort *, u_int8_t > map_p_port_membership;
typedef map < u_int16_t, map_p_port_membership > map_pkey_p_port_membership;

/*****************************************************/
class IBDiag {
public:
    struct DirectRouteAndNodeInfo{
        DirectRouteAndNodeInfo();

        direct_route_t *p_direct_route;
        bool is_filled;
        SMP_NodeInfo node_info;
    };

    typedef list < DirectRouteAndNodeInfo > list_route_and_node_info;
    typedef void (IBDiag::*virtual_data_request_func_t)(IBPort *, ProgressBar *);

    class NodeInfoSendData {
    public:
        NodeInfoSendData(list_route_and_node_info &in_list);

        list_route_and_node_info::iterator position_itr;
        const list_route_and_node_info::iterator end_itr;
    };

    u_int64_t curr_iteration;

    bool retrieved_ext_node_info_ok;
    int  ext_node_info_check_results;

private:

    typedef std::set < std::pair < const IBPort *, const IBPort * > > set_links;
    typedef std::map < int, set_links > map_links_by_depth;

    struct RNMaxData {
        u_int64_t port_rcv_rn_pkt;
        u_int64_t port_xmit_rn_pkt;
        u_int64_t port_rcv_rn_error;
        u_int64_t sw_relay_rn_error;

        bool      is_pfrn_supported_in_fabric;
        u_int32_t pfrn_received_packet;
        u_int32_t pfrn_received_error;
        u_int32_t pfrn_xmit_packet;
        u_int32_t pfrn_start_packet;
        bool      is_port_ar_trials_supported_in_fabric;
        u_int64_t port_ar_trials;

        RNMaxData() : port_rcv_rn_pkt(0), port_xmit_rn_pkt(0),
                      port_rcv_rn_error(0), sw_relay_rn_error(0),
                      is_pfrn_supported_in_fabric(false),
                      pfrn_received_packet(0), pfrn_received_error(0),
                      pfrn_xmit_packet(0), pfrn_start_packet(0),
                      is_port_ar_trials_supported_in_fabric(false),
                      port_ar_trials(0) {}
    };

private:
    ////////////////////
    //members
    ////////////////////

    // dev config file
    string dev_config_filename;
    string dev_config_name;
    IniFile dev_config;

    // Mads timeout and retries
    int mads_timeout;
    int mads_retries;

    IBFabric discovered_fabric;
    Ibis ibis_obj;
    IBDMExtendedInfo fabric_extended_info;

    enum {NOT_INITILIAZED, NOT_SET_PORT, READY} ibdiag_status;
    enum {DISCOVERY_SUCCESS, DISCOVERY_NOT_DONE, DISCOVERY_DUPLICATED_GUIDS} ibdiag_discovery_status;
    string last_error;
    string generated_files_list;
    bool check_duplicated_guids;
    bool check_switch_duplicated_guids;

    map_aport_vec_p_fabric_err errors_by_aport;

    ////////////////////
    //discovery members
    ////////////////////
    list_p_direct_route bfs_list;               //this list supposed to be empty at the end of discovery
    list_p_direct_route good_direct_routes;
    list_p_bad_direct_route bad_direct_routes;
    list_p_direct_route loop_direct_routes;
    list_string duplicated_guids_detection_errs;
    list_p_fabric_general_err errors; //MAYDO replace separete error per RetrieceInfo

    map_guid_list_p_direct_route bfs_known_node_guids;
    map_guid_list_p_direct_route bfs_known_port_guids;
    //topology should not contain duplicated port guids
    map_port_p_direct_route port_routes_map;

    //this map is filled in alias guids stage with port guids and their alias guids.
    //If one wishes to use db for only aguids please use the PortByAGuids in
    //ibdm.
    map_guid_pport port_aguids;

    IBNode * root_node;
    u_int8_t root_port_num;

    u_int8_t llr_active_cell_size; //active LLR given by user

    int64_t ber_threshold;
    //capability mask module, responsible for what mad capabilities are supported
    CapabilityModule capability_module;

    //it enables sending NodeDescription MAD.
    //May be used:
    //   1. in checking duplication GUIDS
    //   2. printing readable node's names for IbdiagPath if errors occur during discovery
    bool send_node_desc;

    //It enables sending PortInfo MADs for HCA/Routers.
    //The IbDiagPath works with lids. We need PortInfo's data in order to check HCA/Routesr lids
    bool send_port_info;

    //todo read in from input
    int max_node_info_mads_in_pack;

    //initiate checking that tpology is rail_optimized
    bool rail_on;
    //set if rails data is collected
    bool rail_data_collected;

    string smdb_path;
    IBDiagSMDB ibdiag_smdb;

    bool simulator_on;

    bool cable_exported;

    bool is_nvlink;
    KeyUpdater                  key_updater;
    ////////////////////
    //methods
    ////////////////////

    void CleanUpInternalDB();

    int RecalculatePortsSpeed();

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int GetReverseDirectRoute(INOUT direct_route_t *p_reverse_direct_route, IN direct_route_t *p_direct_route,
            IN bool startWithZero = false);

    // Returns: SUCCESS_CODE / ERR_CODE_EXCEEDS_MAX_HOPS
    int ConcatDirectRoutes(IN direct_route_t *p_direct_route1,
            IN direct_route_t *p_direct_route2,
            OUT direct_route_t *p_direct_route_result);

    ////////////////////
    //duplicated guids  methods
    ////////////////////
    void AddDupGUIDDetectError(IN direct_route_t *p_direct_route_checked_node,
            IN u_int64_t checked_node_guid,
            IN u_int8_t checked_node_type,
            IN direct_route_t *p_direct_route_got_err,
            IN bool no_response_err,
            IN bool max_hops_err,
            IN string err_desc);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_DUPLICATED_GUID / ERR_CODE_FABRIC_ERROR
    int CheckIfSameCADevice(IN direct_route_t *p_new_direct_route,
            IN direct_route_t *p_old_direct_route,
            IN struct SMP_NodeInfo *p_new_node_info,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_DUPLICATED_GUID / ERR_CODE_FABRIC_ERROR / ERR_CODE_EXCEEDS_MAX_HOPS
    int CheckIfSameSWDevice(IN direct_route_t *p_new_direct_route,
            IN direct_route_t *p_old_direct_route,
            IN struct SMP_NodeInfo *p_new_node_info,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int IsDuplicatedGuids(IN direct_route_t *p_new_direct_route,
            IN struct SMP_NodeInfo *p_new_node_info,
            OUT bool *duplicated_node_guid,
            OUT bool *duplicated_port_guid,
            OUT bool *is_visited_node_already,
            OUT bool *is_visited_port_already,
            OUT direct_route_t **p_old_direct_route,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info);

    ////////////////////
    //discovery methods
    ////////////////////
    inline bool IsBFSKnownPortGuid(IN u_int64_t guid);
    inline bool IsBFSKnownNodeGuid(IN u_int64_t guid);
    inline void MarkNodeGuidAsBFSKnown(IN u_int64_t guid, IN direct_route_t *p_direct_route) {
        this->bfs_known_node_guids[guid].push_back(p_direct_route);
    }
    inline void MarkPortGuidAsBFSKnown(IN u_int64_t guid, IN direct_route_t *p_direct_route) {
        this->bfs_known_port_guids[guid].push_back(p_direct_route);
    }

    inline list_p_direct_route& GetDirectRoutesByNodeGuid(IN u_int64_t guid) { return this->bfs_known_node_guids[guid]; }
    inline list_p_direct_route& GetDirectRoutesByPortGuid(IN u_int64_t guid) { return this->bfs_known_port_guids[guid]; }

    inline void BFSPushPath(IN direct_route_t *p_direct_route);
    inline direct_route_t * BFSPopPath();

    inline void AddGoodPath(IN direct_route_t *p_direct_route);
    inline int  AddBadPath(IN IbdiagBadDirectRoute_t *bad_direct_route, IN direct_route_t *direct_route);
    inline void AddLoopPath(IN direct_route_t *p_direct_route);

    void PostDiscoverFabricProcess();

    // Returns: SUCCESS_CODE / ERR_CODE_FABRIC_ERROR / ERR_CODE_IBDM_ERR / ERR_CODE_INCORRECT_ARGS / ERR_CODE_NO_MEM
    int DiscoverFabricOpenCAPorts(IN IBNode *p_node,
            IN direct_route_t *p_direct_route,
            IN SMP_NodeInfo *p_node_info,
            IN bool is_root,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info,
            IN bool push_new_direct_route);
    int DiscoverFabricOpenSWPorts(IN IBNode *p_node,
            IN direct_route_t *p_direct_route,
            IN SMP_NodeInfo *p_node_info,
            IN bool is_root,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info,
            IN bool push_new_direct_route);
    int DiscoverFabricBFSOpenPorts(IN direct_route_t * p_direct_route,
            IN IBNode *p_node,
            IN struct SMP_NodeInfo *p_node_info,
            IN bool is_visited_node,
            IN bool is_root,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info,
            IN bool push_new_direct_route = true);

    int ApplySubCluster(IN vector_p_node &sub_nodes,
                        IN vector_p_port &sub_ports);
    int ApplySubCluster(IN set_pnode &sub_nodes,
                        IN set_p_port &sub_ports);

    u_int16_t GetPathNextNode(IN IBNode **p_node,
            IN lid_t dest_lid,
            INOUT direct_route_t **p_direct_route,
            IN direct_route_t *dr_path,
            IN struct SMP_NodeInfo &curr_node_info);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_IBDM_ERR / ERR_CODE_TRY_TO_DISCONNECT_CONNECTED_PORT
    int DiscoverFabricBFSCreateLink(IN direct_route_t * p_direct_route,
            IN IBPort * p_port);

    // Returns: SUCCESS_CODE / ERR_CODE_FABRIC_ERROR / ERR_CODE_IBDM_ERR / ERR_CODE_INCORRECT_ARGS / ERR_CODE_NO_MEM
    int DiscoverFabricBFSOpenNode(IN direct_route_t *p_direct_route,
            IN bool is_root,
            OUT IBNode **p_pnode,
            OUT struct SMP_NodeInfo *p_node_info,
            OUT bool *is_visited_node,
            IN  ProgressBar *p_progress_bar,
            OUT IbdiagBadDirectRoute_t *p_bad_direct_route_info,
            IN  bool send_node_info = true);

    bool IsValidNodeInfoData(IN struct SMP_NodeInfo *p_node_info,
                             OUT string &additional_info);


    int GetAndValidateLevelRoutes(OUT list_route_and_node_info &level_routes_and_node_info_list,
                                  IN u_int8_t max_hops);
    void BuildNodeInfo(IN OUT list_route_and_node_info &level_routes_and_node_info_list);

    ////////////////////
    //db file methods
    ////////////////////
    void DumpNodeInfoToCSV(CSVOut &csv_out);
    void DumpSwitchInfoToCSV(CSVOut &csv_out);
    void DumpPortHierarchyInfoStream(ostream & stream, const char * header_preffix);
    void DumpPortHierarchyInfoToCSV(CSVOut &csv_out);
    void DumpPhysicalHierarchyInfoToCSV(CSVOut &csv_out);
    void DumpARInfoToCSV(CSVOut &csv_out);
    void DumpPortDRToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpPortInfoToCSV(CSVOut &csv_out, bool show_ports_data_extra);
    int DumpLinksToCSV(CSVOut &csv_out);
    void DumpExtendedNodeInfoToCSV(CSVOut &csv_out);
    void DumpExtendedPortInfoToCSV(CSVOut &csv_out);
    void DumpPortInfoExtendedToCSV(CSVOut &csv_out);
    void DumpFECModeToCSV(CSVOut &csv_out);

    /////////////////////////////
    //network dump file methods
    /////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpNetwork(ofstream &sout);
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpNetworkAggregated(ofstream &sout);
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpNetworkNodeHeader(ostream &sout, IBNode *p_node);
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpNetworkSystemHeader(ostream &sout, IBSystem *p_system);
    //Returns: SUCCESS_CODE / IBDIAG_ERR_CODE_FABRIC_ERROR
    int DumpNetworkPort(ostream &sout, IBPort *p_port);
    //Returns: SUCCESS_CODE / IBDIAG_ERR_CODE_FABRIC_ERROR
    int DumpNetworkPortBySys(ostream &sout, IBPort *p_port);

    ////////////////////
    //pm methods
    ////////////////////
    void DumpPortExtendedSpeedsCounters(ostream &sout,
                                        bool en_per_lane_cnts,
                                        IBPort *p_curr_port,
                                        u_int32_t port_idx);

    void DumpAPortCounters(APort* p_aport,
                           ofstream &sout,
                           u_int32_t check_counters_bitset,
                           bool en_per_lane_cnts);
    void DumpPortCounters(IBPort* p_port,
                          ostream &sout,
                          u_int32_t check_counters_bitset,
                          bool en_per_lane_cnts,
                          bool print_header);
    void DumpAllPortsCounters(ofstream &sout,
                              u_int32_t check_counters_bitset,
                              bool en_per_lane_cnts);
    void DumpAllAPortsCounters(ofstream &sout,
                               u_int32_t check_counters_bitset,
                               bool en_per_lane_cnts);
    bool DumpPerformanceHistogramBufferControlByVLAndDir(IBPort* p_port, 
                                                         ostream& sout, 
                                                         u_int8_t vl, 
                                                         u_int8_t dir);
    bool DumpPerformanceHistogramBufferDataByVLAndDir(IBPort* p_port, 
                                                      ostream& sout, 
                                                      u_int8_t vl, 
                                                      u_int8_t dir);
    int ReadCapMask(IBNode *p_node, IBPort *p_port,
                    u_int16_t &cap_mask, u_int32_t &port_info_cap_mask);
    int ReadPortInfoCapMask(IBNode *p_node,
                            IBPort *p_port,
                            u_int32_t &port_info_cap_mask,
                            u_int16_t *p_port_info_cap_mask2 = NULL);

    u_int8_t PMIsOptionalAttrSupported(IBNode * p_node, int attr_id);

    int BuildPMPortSamplesControl(list_p_fabric_general_err& pm_errors);
    int BuildPMPortSamplesResult(list_p_fabric_general_err& retrieve_errors);

    ////////////////////
    //sm methods
    ////////////////////
    void DumpSMInfo(ofstream &sout);

    ////////////////////
    // vs methods
    ////////////////////
    void DumpNodesInfo(ofstream &sout);

    //Returns: SUCCESS_CODE / IBDIAG_ERR_CODE_FABRIC_ERROR
    int CheckVSGeneralInfo(IBNode *p_curr_node, struct VendorSpec_GeneralInfo *p_curr_general_info);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsCapSmpFwInfo(list_p_fabric_general_err &vs_cap_smp_errors);
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsCapSmpCapabilityMask(list_p_fabric_general_err &vs_cap_smp_errors);
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsCapGmpInfo(list_p_fabric_general_err &vs_cap_gmp_errors);

    ////////////////////
    //routing methods
    ////////////////////
    int ReportNonUpDownCa2CaPaths(IBFabric *p_fabric, vec_pnode& rootNodes, string& output);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int GetSwitchesDirectRouteList(direct_route_list &ar_routes,
                                   AdditionalRoutingDataMap *p_routing_data_map = NULL);
    int GetSwitchesDirectRouteListEntry(IBNode *p_node,
                                        direct_route *p_route,
                                        direct_route_list &routes,
                                        AdditionalRoutingDataMap *p_routing_data_map);

    int GetSwitchesDirectRouteList(direct_route_list &from_routes,
                                   direct_route_list &ar_routes,
                                   AdditionalRoutingDataMap *p_routing_data_map);

    /// PLFT
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildPLFTInfo(list_p_fabric_general_err& retrieve_errors,
                         direct_route_list &directRouteList, bool is_ibdiagpath = false);
    int BuildPLFTMapping(list_p_fabric_general_err& retrieve_errors,
                         direct_route_list &directRouteList, bool is_ibdiagpath = false);
    int BuildPLFTTop(list_p_fabric_general_err& retrieve_errors,
                        direct_route_list &directRouteList, bool is_ibdiagpath = false);
    // AR
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    // removes sw entries from directRouteList if enable bit is false
    int BuildARGroupTable(list_p_fabric_general_err& retrieve_errors,
                       direct_route_list & directRouteList, bool is_ibdiagpath = false);
    int BuildARLinearForwardingTable(
                        list_p_fabric_general_err& retrieve_errors,
                        direct_route_list & directRouteList, bool is_ibdiagpath = false,
                        const set_lid &lids = set_lid());
    bool ShouldSendARLinearForwardingTableMAD(u_int16_t block, const set_lid &lids) const;

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int DumpUCFDBSInfo(ofstream &sout);
    int DumpMCFDBSInfo(ofstream &sout);
    int DumpSLVLFile(ofstream &sout,
                     list_p_fabric_general_err& retrieve_errors);
    int DumpPLFTInfo(ofstream &sout);
    int DumpARInfo(ofstream &sout);
    int DumpFARInfo(AdditionalRoutingDataMap *p_routing_data_map,
                    ofstream &sout, bool isFLID);
    int DumpRNInfo(list_p_fabric_general_err& errors,
                   AdditionalRoutingDataMap *p_routing_data_map,
                   ofstream &sout);

    int DumpEPFInfo(ofstream &sout);
    int DumpRailFilterInfo(ofstream &sout);
    int DumpEndPortPlaneFilterInfo(ofstream &sout);

    int DumpVL2VLInfo(ofstream &sout);

    //helper for DumpFARInfo
    void DumpPortsBitset(u_int64_t pgSubBlockElement,
                         phys_port_t portOffset,
                         ostream &sout);
    void DumpPortgroup(ib_portgroup_block_element &pgBlockElement,
                       ostream &sout);

    //helper for DumpRNInfo
    const char *RNDecisionToStr(u_int8_t decision);

    //helper for DumpSLVLFile
    int ReadCASLVL(ofstream &sout,
                   clbck_data_t &clbck_data,
                   SMP_SLToVLMappingTable &slvl_mapping,
                   IBNode *p_node);
    bool HandleUnsupportedSLMapping(ofstream &sout,
                                    IBNode *p_node,
                                    phys_port_t port);

    ////////////////////
    ////pkey methods
    ////////////////////
    void DumpPartitionKeys(ofstream &sout,
                           const map_pkey_p_port_membership& pkey_to_p_port_mem);

    ////////////////////
    ////aguid methods
    ////////////////////
    void DumpAliasGUID(ofstream &sout);

    int CheckCapabilityForQoSConfigSL(IBNode *p_curr_node, bool is_vports,
                                      list_p_fabric_general_err& qos_config_sl_errors,
                                      bool& has_capability);

    int PrintSwitchesToIBNetDiscoverFile(ostream &sout, warnings_list &warnings);
    int PrintHCAToIBNetDiscoverFile(ostream &sout, warnings_list &warnings);
    int PrintHCANodePorts(IBNode *p_node, ostream &sout, warnings_list &warnings);
    int PrintHCAVirtualPorts(IBNode *p_node, ostream &sout, warnings_list &warnings);
    void PrintVirtPortLidName(IBPort *p_curr_port, IBVPort *p_vport, ostream &sout);
    int PrintSwitchNodePorts(IBNode *p_node, ostream &sout, warnings_list &warnings);
    int PrintRemoteNodeAndPortForHCA(IBPort *p_port, ostream &sout);
    int PrintRemoteNodeAndPortForSwitch(IBPort *p_remote_port, ostream &sout);
    int PrintNodeInfo(IBNode *p_node, ostream &sout, warnings_list &warnings);

    int BuildHBFConfig(list_p_fabric_general_err& retrieve_errors, unsigned int &supportedDev);

    int BuildpFRNConfig(list_p_fabric_general_err& retrieve_errors);

    void DumpRNCounters_2_Info(ostream &sout,
            const port_rn_counters &rn_counters, const adaptive_routing_info &ar_info,
            RNMaxData &rn_max_data) const;
    void DumpHBFCounters_2_Info(ostream &sout,
            const port_routing_decision_counters &routing_decision_counters) const;

    // CC Algo Data
    void DumpCC_AlgoData(ofstream &sout, PPCCAlgoDatabase &ppcc_algo_db);

    ////////////////////
    // ibis_stat methods
    ////////////////////
    void DumpIbisStat(ofstream &sout);

    int DumpIBLinkInfo(ofstream& sout);

    // Entry Plane Filter
    bool isAvailableByEPF(IBPort *in_port, IBPort *out_port);


    typedef  std::map < IBNode*, set_lid > direction_map;

    int BuildScope(const set_pnode &dest_nodes, set_pnode &scope_nodes, set_p_port &scope_ports,
                      list_p_fabric_general_err &scope_errors);
    int BuildScope_AddSearchPaths(direct_route_list &rotes_to_continue,
                                  set_p_port &scope_ports, direction_map &search_queue,
                                  list_p_fabric_general_err &scope_errors);
    int BuildScope_GetRoutesToContinueSearch(const direction_map &seacrh_queue,
                                             const set_pnode &dest_nodes,
                                             direct_route_list &rotes_to_continue,
                                             set_pnode &scope_nodes,
                                             direction_map &tried_paths,
                                             list_p_fabric_general_err &scope_errors);
    int BuildScope_GetDestinationLids(const set_pnode &dest_nodes, set_lid &dest_lids);
    int BuildScope_InitSearchQueue(const set_pnode &dest_nodes, const set_lid &dest_lids,
                                   direction_map &search_queue);
public:
    ////////////////////
    //methods
    ////////////////////
    IBDiag();
    ~IBDiag();

    void LoadDevConfigFile();

    // dev config file
    void SetDevConfigFileName(const string &filename) { dev_config_filename = filename; }
    const string& GetDevConfigFileName() const { return dev_config_filename; }

    void SetDevConfigName(const string &name) { dev_config_name = name; }
    const string& GetDevConfigName() const { return dev_config_name; }
  
    inline const IniFile & GetDevConfig() { return this->dev_config; }

    // Mads timeout and retries
    void SetMadsTimeout(int timeout) { mads_timeout = timeout; }
    int GetMadsTimeout() const { return mads_timeout; }

    void SetMadsRetries(int retries) { mads_retries = retries; }
    int GetMadsRetries() const { return mads_retries; }

    // simulator generation
    bool IsSimulatorOn() const { return simulator_on; }
    void SetSimulatorOn(bool val) { simulator_on = val; }

    int BuildHBFData(list_p_fabric_general_err& retrieve_errors, unsigned int &supportedDev);

    // pFRN (N2N, Class C)
    int BuildpFRNData(list_p_fabric_general_err& retrieve_errors);
    int BuildN2NClassPortInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildNeighborsInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildN2NKeyInfo(list_p_fabric_general_err& retrieve_errors);
    // pFRN (N2N, Class C) - validations
    int pFRNSupportAndTrapsValidation(list_p_fabric_general_err &pfrn_errors);
    int pFRNNeighborsValidation(list_p_fabric_general_err &pfrn_errors);
    int ARGroupsUniformValidation(list_p_fabric_general_err &pfrn_errors);

    int CheckRailOptimizedTopology(PCI_LeafSwitchesInfoVec &leafSwitchInfoVec, vec_pport &excludedCards);

    int BuildNodeInfo(IN OUT NodeInfoSendData &node_info_send_data);

    inline bool IsNodeDescriptionSent() { return this->send_node_desc; }
    int FillInNodeDescription(list_p_fabric_general_err& retrieve_errors,
                              set_pnode *p_alreadySent = NULL);
    int BuildPortInfo(list_p_fabric_general_err& retrieve_errors);

    int CreateIBNetDiscoverFile(const std::string& file_name, warnings_list& warnings);

    int PostReportsSMValidations(list_p_fabric_general_err& errors, u_int32_t& num_of_diff_val);
    void PostReportsSMValidations_CC_RP_NP_Params(list_p_fabric_general_err& errors);
    void PostReportsSMValidations_CC_algo(list_p_fabric_general_err& errors);

    int CreateIBLinkInfoFile(const string& file_name);

    int CreateIBCPPSimulatorInfo(const string &class_name, const set< u_int64_t > &simulator_guids);

    void DumpNodeCapsForSimulator(ofstream& sout);

    void ResetAppData(bool force = false);

    static void PrintFileTimestamp(const string &file_path, const string &file_type);

    //Returns: SUCCESS_CODE / ERR_CODE_IO_ERR
    int OpenFile(const string & name, const OutputControl::Identity& identity, ofstream& sout,
                 bool to_append = false, const char * header_line_prefix = NULL);
    void CloseFile(ofstream & sout, const char* header_line_prefix = "#");

    void AddGeneratedFile(const string &name, const string &file_name);
    const string& GetGeneratedFilesListString() const { return generated_files_list; }

    inline bool IsInit() { return (this->ibdiag_status != NOT_INITILIAZED); };
    inline bool IsReady() { return (this->ibdiag_status == READY); };
    inline bool IsDiscoveryDone() { return (this->ibdiag_discovery_status == DISCOVERY_SUCCESS ||
                                            this->ibdiag_discovery_status == DISCOVERY_DUPLICATED_GUIDS); }
    inline bool IsDuplicatedGuidsFound() { return this->ibdiag_discovery_status == DISCOVERY_DUPLICATED_GUIDS; }
    bool IsInvalidGUID(uint64_t guid) const { return (guid == 0 || guid == numeric_limits<uint64_t>::max()); }

    list_p_fabric_general_err &GetErrors() {return errors;}

    inline IBFabric * GetDiscoverFabricPtr() { return &(this->discovered_fabric); }
    inline Ibis * GetIbisPtr() { return &(this->ibis_obj); }
    inline CapabilityModule* GetCapabilityModulePtr() { return &(this->capability_module); }
    inline IBDMExtendedInfo* GetIBDMExtendedInfoPtr() { return &(this->fabric_extended_info); }
    inline KeyUpdater* GetKeyUpdaterPtr() { return &(this->key_updater); }

    inline list_string& GetRefToDupGuidsDetectionErrors() {return this->duplicated_guids_detection_errs; }
    inline bool& GetRefToCheckDupGuids() { return this->check_duplicated_guids; }

    vec_p_fabric_err& GetErrorsByAPort(APort* p_aport);
    void SetLastError(const char *fmt, ...);
    const char* GetLastError();
    inline bool IsLastErrorEmpty() { return last_error.empty();}

    // Returns: SUCCESS_CODE / ERR_CODE_INIT_FAILED
    int Init();
    int SetPort(const char* device_name, phys_port_t port_num);
    int SetPort(u_int64_t port_guid);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int GetLocalPortState(OUT u_int8_t& state);

    IBNode * GetNodeByDirectRoute(IN const direct_route_t *p_direct_route);
    direct_route_t * GetDR(const IBNode *p_node);
    direct_route_t *GetDR(const IBPort *p_port);
    int SetDR(const IBPort *p_port, direct_route_t *p_direct_route);
    void SetDRPort(u_int64_t guid, u_int8_t port_num, direct_route_t *p_direct_route);

    lid_t GetLid(const IBNode *p_node);
    lid_t GetLid(const IBPort *p_port);

    // the last output port in a direct route (nullptr on error)
    IBPort* GetLastOutPortByDirectRoute(IN const direct_route_t *p_direct_route);
    APort* GetLastOutAPortByDirectRoute(IN const direct_route_t *p_direct_route);
    // the destination port the direct route leads to (nullptr on error)
    IBPort* GetDestPortByDirectRoute(IN const direct_route_t *p_direct_route);
    APort* GetDestAPortByDirectRoute(IN const direct_route_t *p_direct_route);
    bool isRoutesToSameAPort(IN const list_p_direct_route& p_routes);
    IBPort * GetRootPort();
    int GetAllLocalPortGUIDs(OUT local_port_t local_ports_array[IBIS_MAX_LOCAL_PORTS],
            OUT u_int32_t *p_local_ports_num);

    int getLatestSupportedVersion(int page_number,
                                  unsigned int &latest_version);

    inline void SetBERThreshold(int64_t ber) {
        if (ber == 0) {   /* ZERO is considered as overflow value ==> print all calculations */
            this->ber_threshold = OVERFLOW_VAL_64_BIT;
            return;
        }
        this->ber_threshold = ber;
    }

    inline int64_t GetBERThreshold() {return ber_threshold;}

    ////////////////////
    //duplicated guids methods
    ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_CHECK_FAILED / ERR_CODE_NO_MEM / ERR_CODE_DB_ERR
    int CheckDuplicatedGUIDs(list_p_fabric_general_err& guids_errors);

    // Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int PrintNodesDuplicatedGuids();
    int PrintPortsDuplicatedGuids();

    void PrintDupGuidsDetectionErrors();

    ////////////////////
    //discovery methods
    ////////////////////
    void GetGoodDirectRoutes(OUT list_string& good_direct_routes);
    void GetLoopDirectRoutes(OUT list_string& loop_direct_routes);
    void GetBadDirectRoutes(OUT list_string& bad_direct_routes);
    void PrintAllRoutes();
    void PrintAllDirectRoutes();


    int ApplyPathScope(const set_pnode &dest_nodes,
                       set_pnode &scope_nodes, set_p_port  &scope_ports,
                       list_p_fabric_general_err &errors);

    // Returns: SUCCESS_CODE / ERR_CODE_FABRIC_ERROR / ERR_CODE_IBDM_ERR / ERR_CODE_INCORRECT_ARGS / ERR_CODE_NO_MEM / ERR_CODE_DB_ERR / ERR_CODE_TRY_TO_DISCONNECT_CONNECTED_PORT / ERR_CODE_INCORRECT_ARGS
    int DiscoverPath(IN u_int8_t max_hops, IN lid_t src_lid, IN lid_t dest_lid,
                     direct_route_t &dr_path, OUT stringstream *pss);
    int NodeDescriptionEntry(ProgressBarNodes &progress_bar, clbck_data_t &clbck_data,
                             uint64_t guid, IBNode *p_curr_node);
    int CreateScopeFile(const set_pnode &nodes, const string &file_name);

    bool IsVirtualLidForNode(IBNode *p_node,
                            lid_t lid,
                            stringstream *pss);
    int DiscoverFabric(IN u_int8_t max_hops = IBDIAG_MAX_HOPS);

    // for 'load from file' mode
    int DiscoverFabricFromFile(IN const string &csv_file, IN bool build_direct_routes = true);
    int BuildDirectRoutesDB();

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildSwitchInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildExtendedSwitchInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildSwitchInfoEntry(ProgressBarNodes &progress_bar, clbck_data_t &clbck_data,
                             IBNode *p_curr_node, direct_route *p_p_curr_direct_route);

    // Chassis Info
    int BuildChassisInfo(list_p_fabric_general_err& errors);
    int DumpChassisInfoToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildHierarchyInfo(list_p_fabric_general_err& retrieve_errors);
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR
    // ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildPortInfoExtended(list_p_fabric_general_err& retrieve_errors);

    int ParsePSLFile(const string & file_name, string& output);
    int ParseSADumpFile(const string & file_name, string& output);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_IBDM_ERR
    int DumpCapabilityMaskFile(const OutputControl::Identity& identity, string& output);
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_IBDM_ERR
    int DumpFullCapabilityMaskFile(const OutputControl::Identity& identity, string& output);
    //for debug purposes
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_IBDM_ERR
    int DumpGuid2Mask(const string &file_name, string& output);

    void DumpGeneralInfoSMPToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int ParseCapabilityMaskFile(const char* file_name, string& output);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int ParseNodeNameMapFile(const char* file_name, string& output);

    inline void SetCheckSwitchDuplicatedGuids(bool val) {
        this->check_switch_duplicated_guids = val;

        if (this->check_switch_duplicated_guids){
           this->send_node_desc = true;
        }
    }

    inline void SetLLRActiveCell(const string &active_cell_size) {
        unsigned long int cell_size = strtoul(active_cell_size.c_str(), NULL, 10);

        switch (cell_size) {
        case 128:
            this->llr_active_cell_size = RETRANS_LLR_ACTIVE_CELL_128;
            break;
        case 64:
            this->llr_active_cell_size = RETRANS_LLR_ACTIVE_CELL_64;
            break;
        case 0:
        default:
            this->llr_active_cell_size = RETRANS_NO_RETRANS;
        }
    }

    inline u_int8_t GetLLRActiveCellSize() {
        return this->llr_active_cell_size;
    }

    inline void SetRailDataCollected(bool value) {  this->rail_data_collected = value; }
    inline bool IsRailDataCollected () const { return this->rail_data_collected; }

    inline void SetRailValidation(bool val) { this->rail_on = val; }
    inline int IsRailValidation() const { return this->rail_on; }

    inline void SetCableExported(bool value) {  this->cable_exported = value; }
    inline bool IsCableExported () const { return this->cable_exported; }


    ////////////////////
    //checks methods
    ////////////////////
    //Return: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_INCORRECT_ARGS / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED / ERR_CODE_DB_ERR
    int CheckLinkWidth(list_p_fabric_general_err& width_errors, string expected_link_width_str = "");
    int CheckLinkSpeed(list_p_fabric_general_err& speed_errors, string expected_link_speed = "");
    int CheckPortHierarchyInfo(list_p_fabric_general_err &hierarchy_errors);

    //Return: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED
    int CheckDuplicatedNodeDescription(list_p_fabric_general_err& nodes_errors);
    int CheckLinks(list_p_fabric_general_err& links_errors, IBLinksInfo *p_ib_links_info = NULL);
    int CheckLegacyLinks(list_p_fabric_general_err& links_errors, IBLinksInfo *p_ib_links_info);
    int CheckAPortLinks(list_p_fabric_general_err& links_errors, IBLinksInfo *p_ib_links_info);
    int CheckLids(list_p_fabric_general_err& lids_errors);
    int CheckAndSetVPortLid(list_p_fabric_general_err &vport_errors);

    ////////////////////
    //db file methods
    ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR / ERR_CODE_DB_ERR
    int DumpInternalToCSV(CSVOut &csv_out, bool show_ports_data_extra);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IBDM_ERR
    int WriteLSTFile(const string &file_path, bool write_with_lmc);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IBDM_ERR
    int WritePortHierarchyInfoFile(const std::string & file_name);

    int WriteNetDumpFile(const string &file_path);
    int WriteNetDumpAggregatedFile(const string &file_path);

    int DumpHBFConfigToCSV(CSVOut &csv_out);


    ////////////////////
    //pm methods
    ////////////////////
    void CopyPMInfoObjVector(OUT vector_p_pm_info_obj& new_pm_obj_info_vector);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildClassPortInfo(list_p_fabric_general_err& pm_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildPortCounters(list_p_fabric_general_err& pm_errors,
                            u_int32_t check_counters_bitset, bool send_port_samples_result);
    int ResetPortCounters(list_p_fabric_general_err& ports_errors,
                          u_int32_t check_counters_bitset);

    // Build and Clear RN Counters
    int BuildRNCounters(list_p_fabric_general_err& retrieve_errors);
    int ClearRNCounters(list_p_fabric_general_err& retrieve_errors);

    int BuildHBFCounters(list_p_fabric_general_err& retrieve_errors);
    int ClearHBFCounters(list_p_fabric_general_err& retrieve_errors);

    // Dump RN Counters
    int DumpRNCountersToCSV(CSVOut &csv_out, list_p_fabric_general_err& pfrn_errors);
    int WriteRNCounters_2_File(const string &file_name);
    int DumpRNCounters_2_Info(ostream &sout);
    int WriteRNCountersFile(const string &file_name);
    int DumpRNCountersInfo(ofstream &sout);

    int DumpHBFCountersToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpPortCountersToCSV(CSVOut &csv_out, u_int32_t check_counters_bitset);

    static u_int64_t PMOptionMaskTouint64(struct PortSampleControlOptionMask &pm_option_mask);
    static struct PortSampleControlOptionMask & uint64ToPMOptionMask(
                       struct PortSampleControlOptionMask &pm_option_mask, u_int64_t value);

    int DumpPortSamplesControlToCSV(CSVOut &csv_out);
    int DumpPortSamplesResultToCSVTable(CSVOut &csv_out);

    int DumpPortCountersDeltaToCSV(CSVOut &csv_out, const vector_p_pm_info_obj &prev_pm_info_obj_vec,
                                     u_int32_t check_counters_bitset,
                                     list_p_fabric_general_err& ports_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpDiagnosticCountersToCSV(CSVOut &csv_out);

    void DumpDiagnosticCounters(ofstream &sout);

    void DumpDiagnosticCountersDescriptionP1(ofstream &sout);
    void DumpDiagnosticCountersDescriptionP0(ofstream &sout);

    void DumpDiagnosticCountersP1(ofstream &sout,
                                    struct VS_DiagnosticData *p_p1);

    void DumpDiagnosticCountersP0(ofstream &sout,
                                    struct VS_DiagnosticData *p_p0);

    void DumpDiagnosticCountersP255(ofstream &sout, struct VS_DiagnosticData *p_p255);

    //Returns: SUCCESS_CODE / IBDIAG_ERR_CODE_NO_MEM / ERR_CODE_INCORRECT_ARGS / IBDIAG_ERR_CODE_CHECK_FAILED
    int CalcCounters(vector_p_pm_info_obj& prev_pm_info_obj_vec,
                     double diff_time_between_samples,
                     list_p_fabric_general_err& pm_errors);

    //Returns: SUCCESS_CODE / IBDIAG_ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED / ERR_CODE_INCORRECT_ARGS
    int CheckAllPMValues(list_p_fabric_general_err& pm_errors,
                         list_p_fabric_general_err &pm_threshold_overflow_warnings,
                         map_str_uint64& counters_to_threshold_map,
                         u_int32_t check_counters_bitset);

    //Returns: SUCCESS_CODE / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED / ERR_CODE_INCORRECT_ARGS
    int CheckCountersDiff(vector_p_pm_info_obj& prev_pm_info_obj_vec,
            list_p_fabric_general_err& pm_errors);

    int CalcBER(IBPort *p_curr_port,
                double time,
                u_int64_t symbol_error,
                long double &ber_value_reciprocal_val);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_CHECK_FAILED / ERR_CODE_FABRIC_ERROR
    int CalcBERErrors(vector_p_pm_info_obj& prev_pm_info_obj_vec,
            u_int64_t ber_threshold_opposite_val,
            double sec_between_samples,
            list_p_fabric_general_err& ber_errors,
            CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR
    int CalcPhyTest(vector_p_pm_info_obj& prev_pm_info_obj_vec,
                    double sec_between_samples,
                    CSVOut &csv_out);

    list_string GetListOFPMNames();

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WritePMFile(const string &file_name,
                    u_int32_t check_counters_bitset,
                    bool en_per_lane_cnts);
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WritePMAggregatedFile(const string &file_name,
                              u_int32_t check_counters_bitset,
                              bool en_per_lane_cnts);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteMlnxCntrsFile(const string &file_name);

    int BuildCapabilityCache(string& output);

    ////////////////////
    // Fast Recovery methods
    ////////////////////
    int BuildCreditWatchdogTimeoutCounters(list_p_fabric_general_err& retrieve_errors);
    int ClearCreditWatchdogTimeoutCounters(list_p_fabric_general_err& retrieve_errors);
    int DumpCreditWatchdogTimeoutToCSV(CSVOut &csv_out);

    int BuildFastRecoveryCounters(list_p_fabric_general_err& retrieve_errors);
    int ClearFastRecoveryCounters(list_p_fabric_general_err& retrieve_errors);
    int DumpFastRecoveryCountersToCSV(CSVOut &csv_out);

    int BuildVSPortGeneralCounters(list_p_fabric_general_err& retrieve_errors, bool clear);
    int DumpPortGeneralCountersToCSV(CSVOut &csv_out);

    int BuildVSPortPolicyRecoveryCounters(list_p_fabric_general_err& retrieve_errors, bool clear);
    int DumpPortPolicyRecoveryCountersToCSV(CSVOut &csv_out);

    int BuildProfilesConfig(list_p_fabric_general_err& retrieve_errors,
                            ProfilesConfigFeature profile_feature, bool with_HCAs);
    int DumpProfilesConfigToCSV(CSVOut &csv_out);

    int BuildCreditWatchdogConfig(list_p_fabric_general_err& retrieve_errors);
    int DumpCreditWatchdogConfigToCSV(CSVOut &csv_out);

    int BuildBERConfig(list_p_fabric_general_err& retrieve_errors);
    int DumpBERConfigToCSV(CSVOut &csv_out);

    ////////////////////
    // Performance Histogram methods
    ////////////////////
    int BuildPerformanceHistogramInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildPerformanceHistogramBufferControl(list_p_fabric_general_err& retrieve_errors);
    int BuildPerformanceHistogramBufferData(list_p_fabric_general_err& retrieve_errors, bool clear);
    int BuildPerformanceHistogramPortsControl(list_p_fabric_general_err& retrieve_errors);
    int BuildPerformanceHistogramPortsData(list_p_fabric_general_err& retrieve_errors, bool clear);
    
    int DumpPerformanceHistogramInfoToCSV(CSVOut &csv_out);
    int DumpPerformanceHistogramBufferControlToCSV(CSVOut &csv_out);
    int DumpPerformanceHistogramBufferDataToCSV(CSVOut &csv_out);
    int DumpPerformanceHistogramPortsControlToCSV(CSVOut &csv_out);
    int DumpPerformanceHistogramPortsDataToCSV(CSVOut &csv_out);

    ////////////////////
    //NVLink methods
    ////////////////////
    int BuildNVLAnycastLIDInfo(list_p_fabric_general_err& errors);
    int BuildNVLHBFConfig(list_p_fabric_general_err& errors);
    int BuildNVLContainAndDrainInfo(list_p_fabric_general_err& errors);
    int BuildNVLContainAndDrainPortState(list_p_fabric_general_err& errors);

    // Reduction
    int BuildNVLFecMode(list_p_fabric_general_err& retrieve_errors);
    int BuildNVLClassPortInfo(list_p_fabric_general_err& errors);
    int BuildNVLReductionInfo(list_p_fabric_general_err& errors);
    int BuildNVLReductionPortInfo(list_p_fabric_general_err& errors);
    int BuildNVLReductionForwardingTable(list_p_fabric_general_err& errors);
    int BuildNVLPenaltyBoxConfig(list_p_fabric_general_err& errors);
    int BuildNVLReductionConfigureMLIDMonitors(list_p_fabric_general_err& errors);
    int BuildNVLReductionCounters(list_p_fabric_general_err& errors, bool is_reset);
    int BuildNVLReductionRoundingMode(list_p_fabric_general_err& errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpNVLFecModeToCSV(CSVOut &csv_out);
    int DumpAnycastLIDInfoToCSV(CSVOut &csv_out);
    int DumpNVLHBFConfigToCSV(CSVOut &csv_out);
    int DumpNVLContainAndDrainInfoToCSV(CSVOut &csv_out);
    int DumpNVLContainAndDrainPortStateToCSV(CSVOut &csv_out);

    // Reduction
    int DumpNVLClassPortInfoToCSV(CSVOut &csv_out);
    int DumpNVLReductionInfoToCSV(CSVOut &csv_out);
    int DumpNVLReductionPortInfoToCSV(CSVOut &csv_out);
    int DumpNVLReductionForwardingTableToCSV(CSVOut &csv_out);
    int DumpNVLPenaltyBoxConfigToCSV(CSVOut &csv_out);
    int DumpNVLReductionConfigureMLIDMonitorsToCSV(CSVOut &csv_out);
    int DumpNVLReductionCountersToCSV(CSVOut &csv_out);
    int DumpNVLReductionRoundingModeToCSV(CSVOut &csv_out);


    ////////////////////
    //sm methods
    ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildSMInfo(list_p_fabric_general_err& ports_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED
    int CheckSMInfo(list_p_fabric_general_err& sm_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteSMFile(const string &file_name);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpSMInfoToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpVNodeInfoToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpVPortInfoToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpVPortsGUIDInfoToCSV(CSVOut &csv_out);
    ////////////////////
    //vs methods
    ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteNodesInfoFile(const string &file_name);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpVSNodeInfoToCSV(CSVOut &csv_out);

    int DumpExtendedSwitchInfoToCSV(CSVOut &csv_out);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_CHECK_FAILED
    int CheckFWVersion(list_p_fabric_general_err& fw_errors);

    ////////////////////
    //routing methods
    ////////////////////
    void SetDefaultSL(u_int8_t sl) {
        discovered_fabric.SetDefaultSL(sl);
    }

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildUCFDBSEntry(IBNode *p_curr_node, direct_route_t *p_curr_route,
                            list_p_fabric_general_err& retrieve_errors,
                            ProgressBarNodes &progress_bar, clbck_data_t &clbck_data, int &ret,
                            const set_lid &lids = set_lid());
    void MarkBlocksToSend(vector<bool> &blocks, const set_lid &lids, size_t block_size) const;
    int BuildUCFDBSInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildUCFDBSInfo(list_p_fabric_general_err& retrieve_errors,
                           direct_route_list &from_routes, const set_lid &lids = set_lid());
    int BuildMCFDBSInfo(list_p_fabric_general_err& retrieve_errors);

    int BuildVLArbitrationTable(list_p_fabric_general_err& retrieve_errors);
    int DumpVLArbitrationToCSV(CSVOut &csv_out);

    int BuildPLFTData(list_p_fabric_general_err& retrieve_errors, unsigned int &supportedDev);
    int BuildPLFTData(list_p_fabric_general_err& retrieve_errors, direct_route_list &from_routes, bool is_ibdiagpath);

    int BuildARData(list_p_fabric_general_err& retrieve_errors,
                       unsigned int &supportedDev,
                       AdditionalRoutingDataMap *p_routing_data_map,
                       bool skip_lfts);
    int BuildARData(list_p_fabric_general_err& retrieve_errors,
                       direct_route_list &from_routes,
                       AdditionalRoutingDataMap *p_routing_data_map,
                       bool is_ibdiagpath, const set_lid  &lids);
    int BuildWeightsHBFConfig(list_p_fabric_general_err& retrieve_errors);

    // LFT Split
    int BuildLFTSplit(list_p_fabric_general_err& retrieve_errors);
    int DumpLFTSplitToCSV(CSVOut &csv_out);

    int BuildRNData(list_p_fabric_general_err& retrieve_errors,
                       AdditionalRoutingDataMap *p_routing_data_map);

    int BuildPortRecoveryPolicyConfig(list_p_fabric_general_err& retrieve_errors);
    int DumpPortRecoveryPolicyConfigToCSV(CSVOut &csv_out);

    int BuildARInfo(list_p_fabric_general_err& retrieve_errors);
    int BuildARInfoEntry(ProgressBarNodes &progress_bar, clbck_data_t &clbck_data,
                           IBNode *p_node, direct_route_t *p_direct_route);
    int GetAREnabledNum(u_int64_t& ar_enabled_num,
                        u_int64_t& hbf_enabled_num,
                        u_int64_t& hbf_sup_num);

    int AddRNDataMapEntry(AdditionalRoutingDataMap *p_routing_data_map,
                          IBNode *p_node,
                          direct_route_t *p_direct_route,
                          adaptive_routing_info *p_ar_info);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR / ERR_CODE_DB_ERR
    int WriteUCFDBSFile(const string &file_name);
    int WriteMCFDBSFile(const string &file_name);
    int WriteSLVLFile(const string &file_name,
                      list_p_fabric_general_err& retrieve_errors);
    int WritePLFTFile(const string &file_name);
    int WriteARFile(const string &file_name);
    int WriteFARFile(AdditionalRoutingDataMap *p_routing_data_map,
                         const string &file_name, bool isFLID);
    int WriteRNFile(list_p_fabric_general_err& errors,
                    AdditionalRoutingDataMap *p_routing_data_map,
                    const string &file_name);

    // pFRN (N2N, Class C)
    int Dump_pFRNConfigToCSV(CSVOut &csv_out);
    int Dump_N2NClassPortInfoToCSV(CSVOut &csv_out);
    int Dump_NeighborsInfoToCSV(CSVOut &csv_out);
    int Dump_N2NKeyInfoToCSV(CSVOut &csv_out);

    int WriteVL2VLFile(const string &file_name);

    int ParseSLVLFile(const string & file_name, string& output);

    int CountSkipRoutingChecksNodes(string& output);

    //Returns: SUCCESS_CODE / ERR_CODE_IBDM_ERR / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int ReportFabricQualities(string& output, const char *outDir, bool ar_enabled, bool static_ca2ca);
    int ReportCreditLoops(string& output, bool is_fat_tree, bool checkAR = false);
    //AR Tests
    int ReportFabricARConnectivity(string& output);
    int ReportFabricAREmptyGroups(string& output);
    int CheckSL2VLTables(string& output);

    int ReportFabricARValidation(string& output);

    // Entry Plane Filter
    int BuildEntryPlaneFilter(list_p_fabric_general_err& retrieve_errors, bool& is_entry_plane_filter_sup);
    int EntryPlaneFilterValidation(list_p_fabric_general_err& validation_errors);
    int WriteEPFFile(const string &file_name);

    // Rail Filter
    int BuildRailFilter(list_p_fabric_general_err& retrieve_errors, bool& is_rail_filter_sup);
    int WriteRailFilterFile(const string &file_name);

    // End Port Plane Filter
    int BuildEndPortPlaneFilter(list_p_fabric_general_err& retrieve_errors, bool& is_end_port_plane_filter_sup);
    int WriteEndPortPlaneFilterFile(const string &file_name);
    int EndPortPlaneFilterValidation(list_p_fabric_general_err& validation_errors);

    int StaticRoutingSymmetricLinkValidation(list_p_fabric_general_err& errors);
    int AdaptiveRoutingSymmetricLinkValidation(list_p_fabric_general_err& errors);

        ////////////////////
        ////pkey methods
        ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildPartitionKeys(list_p_fabric_general_err &pkey_errors);

    void BuildPKeyMap(map_pkey_p_port_membership& pkey_to_p_port_mem);

    //Returns: SUCCESS_CODE / ERR_CODE_CHECK_FAILED
    int CheckAPortsPKeys(list_p_fabric_general_err& pkey_errors, bool skip_aport_membership);
    /**
     * @brief Check that all ports given as argument have the same PKeys configurations
     *        and memberships.
     * @param ports - A vector of ports to be compared. Can be planes of an APort, or
     *               Port0 for multiple different nodes. index 0 of the vector must be null,
     *               as is the case in planes vector.
     * @return int - SUCCESS_CODE / ERR_CODE_CHECK_FAILED
     */
    int CheckPortsPKeys(list_p_fabric_general_err& pkey_errors, const vec_pport& ports,
                        const string& ports_name, bool skip_aport_membership);

    // Check that all Black Mamba switches in the same system have the same PKey
    // membership on port 0.
    int CheckPlanarizedSystemPort0Pkeys(list_p_fabric_general_err& pkey_errors,
                        uint64_t system_guid, bool skip_aport_membership);

    //Returns: SUCCESS_CODE / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED
    int CheckPartitionKeys(list_p_fabric_general_err& pkey_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WritePKeyFile(const string &file_name,
                      const map_pkey_p_port_membership &pkey_to_p_port_mem);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpPartitionKeysToCSV(CSVOut &csv_out);

        ////////////////////
        ////aguid methods
        ////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildAliasGuids(list_p_fabric_general_err &aguid_errors, NodeTypesFilter mask = NodeTypesFilter_ALL);

    //Returns: SUCCESS_CODE / ERR_CODE_NO_MEM / ERR_CODE_CHECK_FAILED
    int CheckDuplicatedAliasGuids(list_p_fabric_general_err& aguid_errors);
    int CheckVPortDuplicatedGuids(list_p_fabric_general_err& vguid_errors);


    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteAliasGUIDFile(const string &file_name);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS
    int DumpAliasGUIDToCSV(CSVOut &csv_out);

    ////////////////////////////////
    ////VS Capability SMP methods
    ////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsCapSmp(list_p_fabric_general_err &vs_cap_smp_errors);

    ////////////////////////////////
    ////VS Capability GMP methods
    ////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsCapGmp(list_p_fabric_general_err &vs_cap_smp_errors);

    // Collect VS Extended Node Info MAD
    int BuildExtendedNodeInfo(list_p_fabric_general_err &vs_ext_pi_errors);
    int ValidateExtendedNodeInfoForSwitches();

    ////////////////////////////////////////
    ////Collect VS ExtendedPortInfo MADs
    ////////////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildExtendedPortInfo(list_p_fabric_general_err &vs_ext_pi_errors);
    ////////////////////////////////////////
    ////Mellanox Diagnostic Counters methods
    ////////////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVsDiagnosticCounters(list_p_fabric_general_err &mlnx_cntrs_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int ResetDiagnosticCounters(list_p_fabric_general_err& mlnx_cntrs_errors);

    ////////////////////////////////////////
    ///Scope methods
    ////////////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / IBDIAG_ERR_CODE_IBDM_ERR
    //Parses file and applies the scope on the fabric
    //if include_in_scope is true, applies data in file as scope on the fabric
    //else applies as excluded scope.
    int ParseScopePortGuidsFile(const string & file_name,
                                string& output,
                                bool include_in_scope,
                                int &num_of_lines);
    //Unhealthy ports
    int ReadUnhealthyPortsPolicy(string &output, map_guid_to_ports &exclude_ports,
                                const string &file_name,
                                bool switch_action, bool ca_action);
    int MarkOutUnhealthyPorts(string &output, int &unhealthy_ports,
                              const map_guid_to_ports &exclude_ports, const string &file_name);


    ////Virtualization Ports methods
    ////////////////////////////////////////

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVirtualizationBlock(virtual_data_request_func_t data_request_func,
                                 map_str_pnode &nodes_map, bool is_check_cap_mask,
                                 bool with_progress_bar = true);

    void BuildVirtualizationInfoDB(IBPort *p_port, ProgressBar *p_progress_bar);

    void BuildVPortState(IBPort *p_port, ProgressBar *p_progress_bar);

    void BuildVPortInfo(IBPort *p_port, ProgressBar *p_progress_bar);

    void BuildVPortGUIDInfo(IBPort *p_port, ProgressBar *p_progress_bar);

    void BuildVNodeInfo(IBPort *p_port, ProgressBar *p_progress_bar);

    void BuildVPortPKeyTable(IBPort *p_port, ProgressBar *p_progress_bar);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVirtualization(list_p_fabric_general_err &vport_errors);

    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / ERR_CODE_NO_MEM / ERR_CODE_FABRIC_ERROR
    int BuildVNodeDescription(IBNode *p_node, bool with_progress_bar = true);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteVPortsFile(const string &file_name);
    void DumpVPorts(ofstream &sout);

    //Returns: SUCCESS_CODE / ERR_CODE_DISCOVERY_NOT_SUCCESS / ERR_CODE_IO_ERR
    int WriteVPortsPKeyTblFile(const string &file_name);
    void DumpVPortsPKeyTbl(ofstream &sout);

    ////////////////////////////////////////
    ///Temperature Sensing methods
    ////////////////////////////////////////
    //Returns: SUCCESS_CODE / ERR_CODE_DB_ERR / IBDIAG_ERR_CODE_IBDM_ERR
    int BuildSMPTempSensing(
            list_p_fabric_general_err& temp_sensing_errors);

    int DumpTempSensingToCSV(CSVOut &csv_out);

    int BuildRouterInfo(list_p_fabric_general_err& errors);
    int BuildAdjSiteLocalSubnetsTable(list_p_fabric_general_err& errors);
    int BuildAdjSubnetsRouterLIDInfoTable(list_p_fabric_general_err& errors);
    int BuildRouterLIDTable(list_p_fabric_general_err& errors);
    int BuildARGroupToRouterLIDTable(list_p_fabric_general_err& errors);
    int DumpRouterInfoToCSV(CSVOut &csv_out);
    int DumpRouterAdjSiteLocalSubnetTableToCSV(CSVOut &csv_out);
    int DumpAdjSubnetsRouterLIDInfoTableToCSV(CSVOut &csv_out);
    int WriteARGroupToRouterFLIDData(const string& file_name);
    int DumpRouterNextHopToCSV(CSVOut &csv_out);

    bool HandleSpecialPorts(CountersPerSLVL * cntrs_per_slvl,
                            SMP_MlnxExtPortInfo *p_curr_mepi,
                            IBPort *p_curr_port,
                            int &rc,
                            list_p_fabric_general_err &cntrs_per_slvl_errors);

    IBSpecialPortType GetSpecialCAPortType(IBNode *p_node);
    IBSpecialPortType GetSpecialPortType(IBPort *p_port);

    int BuildDBOrResetSLVLCntrs(
            list_p_fabric_general_err &vs_ext_pi_errors,
            bool is_reset_cntr,
            bool to_dump_sup_warn,
            CountersPerSLVL * cntrs_per_slvl);
    int DumpPerSLVLPortCountersToCSV(CSVOut &csv_out, vec_slvl_cntrs  &slvl_cntrs_vec);

    // QoS config SL
    int BuildSMPQoSConfigSL(list_p_fabric_general_err& qos_config_sl_errors,
                            bool is_vports);
    int DumpQoSConfigSLToCSV(CSVOut &csv_out);

    // QoS config VL
    int BuildSMPQoSConfigVL(list_p_fabric_general_err& errors);
    int DumpQoSConfigVLToCSV(CSVOut &csv_out);

    int DumpVPortQoSConfigSLToCSV(CSVOut &csv_out);
    int CheckAPortsQosSymmetry(list_p_fabric_general_err& aport_errors);
    int CheckAPortQosSymmetry(list_p_fabric_general_err& aport_errors,
            const APort *p_aport, bool rate_limit_flag, bool bw_alloc_flag);

    // Congestion Info
    int BuildCCEnhancedCongestionInfo(list_p_fabric_general_err& qos_config_sl_errors);

    int BuildCCSwithGeneralSettings(list_p_fabric_general_err& qos_config_sl_errors);

    int BuildCCSwithConfig(list_p_fabric_general_err& qos_config_sl_errors, u_int64_t& enabled_SWs);

    int BuildCCHCAGeneralSettings(list_p_fabric_general_err& qos_config_sl_errors);

    int BuildCCHCAConfig(list_p_fabric_general_err& qos_config_sl_errors, u_int64_t& enabled_CAs);

    int BuildCCHCAStatisticsQuery(list_p_fabric_general_err& congestion_control_errors,
                                  bool to_clear_congestion_counters);

    int BuildCCSLVLCounters(list_p_fabric_general_err &cc_per_slvl_errors,
                           bool is_reset,
                           CountersPerSLVL* cntrs_per_slvl);

    // CC Algo
    int BuildCCHCAAlgoConfigSup(list_p_fabric_general_err& cc_errors);
    int BuildCCHCAAlgoConfig(list_p_fabric_general_err& cc_errors);
    int BuildCCHCAAlgoConfigParams(list_p_fabric_general_err& cc_errors);
    int BuildCCHCAAlgoCounters(list_p_fabric_general_err& cc_errors,
                                  bool to_clear_congestion_counters);

    void DumpCCEnhancedInfoToCSV(CSVOut &csv_out);
    void DumpCCSwitchGeneralSettingsToCSV(CSVOut &csv_out);
    void DumpCCPortProfileSettingsToCSV(CSVOut &csv_out);
    void DumpCCSLMappingSettingsToCSV(CSVOut &csv_out);
    void DumpCCHCAGeneralSettingsToCSV(CSVOut &csv_out);
    void DumpCCHCARPParametersToCSV(CSVOut &csv_out);
    void DumpCCHCANPParametersToCSV(CSVOut &csv_out);
    void DumpCCHCAStatisticsQueryToCSV(CSVOut &csv_out);
    // CC Algo
    int DumpCCHCAAlgoConfigSupToCSV(CSVOut &csv_out, list_p_fabric_general_err& cc_errors);
    void DumpCCHCAAlgoConfigToCSV(CSVOut &csv_out,
                                       u_int64_t& enabled_algo_CAs,
                                       u_int64_t& enabled_algo_CA_ports,
                                       u_int64_t& disabled_algo_CA_ports);
    int DumpCCHCAAlgoConfigParamsToCSV(CSVOut &csv_out, list_p_fabric_general_err& cc_errors);
    int DumpCCHCAAlgoCountersToCSV(CSVOut &csv_out, list_p_fabric_general_err& cc_errors);

    int WriteCCHCAAlgoDataToFile(const string& file_name,
                                   PPCCAlgoDatabase &ppcc_algo_database);

    int CC_HCA_AlgoValidations(list_p_fabric_general_err& cc_errors,
                               PPCCAlgoDatabase &ppcc_algo_database);

    bool IsSupportedCCCapability(u_int64_t cc_capability_mask, EnCCCapabilityMaskBit bit);

    inline void SetSMDBPath(const string &smdb_path) {
        this->smdb_path = smdb_path;
    }
    inline const string& GetSMDBPath() const { return this->smdb_path; }
    inline IBDiagSMDB& GetIBDiagSMDB() { return ibdiag_smdb; }
    int ParseSMDBFile();
    void SetIsNvLink(bool is_nvlink) {
        this->is_nvlink = is_nvlink;
    }
    bool GetIsNvLink() {
        return this->is_nvlink;
    }

    ////////////////////////////////////////
    // ibis_stat methods
    ////////////////////////////////////////
    int WriteIbisStatFile(const std::string file_name);

    ////////////////////////////////////////
    // Export Data methods
    ////////////////////////////////////////
    int LoadSymbol(void *p_lib_handle, const char *name, void **symbol,
                   list_p_fabric_general_err &export_data_errors);

    int CollectAPortsData(list_p_fabric_general_err& errors_list);
};


#endif          /* IBDIAG_H */

