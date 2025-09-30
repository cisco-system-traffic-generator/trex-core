/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#pragma once

#include <string>
#include <ibis/ibis.h>
#include <infiniband/ibdiag/ibdiag_types.h>


//
// Section NODES
//  - SMP_NodeInfo
//
class NodeRecord {
    public:
        std::string             m_description;
        struct SMP_NodeInfo     m_data;

    public:
        NodeRecord() : m_data({}) {}

        static int Init(vector < ParseFieldInfo <class NodeRecord> > &parse_section_info);
};

//
// Section PORTS
//  - SMP_PortInfo
//
class PortRecord {
    public:
        u_int64_t               m_node_guid;
        u_int64_t               m_port_guid;
        u_int8_t                m_port_num;

        std::string             m_fec_actv;
        std::string             m_retrans_actv;
        bool                    m_has_na;

        struct SMP_PortInfo     m_data;

    public:
        PortRecord() : m_node_guid(0), m_port_guid(0), m_port_num(0), m_has_na(false), m_data({}) {}

        static int Init(vector < ParseFieldInfo <class PortRecord> > &parse_section_info);
};

//
// Section EXTENDED_NODE_INFO
//  - ib_extended_node_info
//
class ExtendedNodeInfoRecord {
    public:
        u_int64_t                        m_node_guid;
        struct ib_extended_node_info     m_data;
        bool                             m_has_na;

    public:
        ExtendedNodeInfoRecord() : m_node_guid(0), m_data({}), m_has_na(false) {}

        static int Init(vector < ParseFieldInfo <class ExtendedNodeInfoRecord> > &parse_section_info);
};

//
// Section EXTENDED_PORT_INFO
//  - SMP_MlnxExtPortInfo
//
class ExtendedPortInfoRecord {
    public:
        u_int64_t                     m_node_guid;
        u_int64_t                     m_port_guid;
        u_int8_t                      m_port_num;
        bool                          m_has_na;
        struct SMP_MlnxExtPortInfo    m_data;

    public:
        ExtendedPortInfoRecord() : m_node_guid(0), m_port_guid(0), m_port_num(0), m_has_na(false), m_data({}) {}

        static int Init(vector < ParseFieldInfo <class ExtendedPortInfoRecord> > &parse_section_info);
};

//
// Section PORT_INFO_EXTENDED
//  - SMP_PortInfoExtended
//
class PortInfoExtendedRecord {
    public:
        u_int64_t                   m_node_guid;
        u_int64_t                   m_port_guid;
        u_int8_t                    m_port_num;
        struct SMP_PortInfoExtended m_data;

    public:
        PortInfoExtendedRecord() : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({}) {}

        static int Init(vector < ParseFieldInfo <class PortInfoExtendedRecord> > &parse_section_info);
};

//
// Section SWITCHES
//  - SMP_SwitchInfo
//
class SwitchRecord {
    public:
        u_int64_t                 m_node_guid;
        struct SMP_SwitchInfo     m_data;

    public:
        SwitchRecord() : m_node_guid(0), m_data({}) {}

        static int Init(vector < ParseFieldInfo <class SwitchRecord> > &parse_section_info);
};

//
// Section PORT_HIERARCHY_INFO
//
class PortHierarchyInfoRecord
{
    public:
        u_int64_t m_node_guid;
        u_int64_t m_port_guid;
        u_int64_t m_template_guid;
        u_int8_t  m_port_num;

    public:
        int32_t  m_bus;
        int32_t  m_device;
        int32_t  m_function;
        int32_t  m_type;
        int32_t  m_slot_type;
        int32_t  m_slot_value;
        int32_t  m_asic;
        int32_t  m_cage;
        int32_t  m_port;
        int32_t  m_split;
        int32_t  m_ibport;
        int32_t  m_port_type;
        int32_t  m_asic_name;
        int32_t  m_is_cage_manager;
        int32_t  m_number_on_base_board;

        // DeviceRecord
        int32_t  m_device_num_on_cpu_node;
        int32_t  m_cpu_node_number;

        // BoardRecord
        int32_t m_board_type;
        int32_t m_chassis_slot_index;
        int32_t m_tray_index;

        int32_t m_topology_id;


        // APort
        int32_t  m_aport;
        int32_t  m_plane;
        int32_t  m_num_of_planes;

        bool     m_has_na;

    public:
        PortHierarchyInfoRecord()
            : m_node_guid(0), m_port_guid(0), m_template_guid(0),
              m_port_num(0), m_bus(0), m_device(0), m_function(0),
              m_type(0), m_slot_type(0), m_slot_value(0),
              m_asic(0), m_cage(0), m_port(0), m_split(0),
              m_ibport(0), m_port_type(0), m_asic_name(0),
              m_is_cage_manager(0), m_number_on_base_board(0),
              m_device_num_on_cpu_node(0),
              m_cpu_node_number(0),
              m_board_type(0),
              m_chassis_slot_index(0),
              m_tray_index(0),
              m_topology_id(0),
              m_aport(0), m_plane(0), m_num_of_planes(0), m_has_na(false)

        {}

        static int Init(vector < ParseFieldInfo <class PortHierarchyInfoRecord> > &parse_section_info);
};

//
// Section PHYSICAL_HIERARCHY_INFO
//
class PhysicalHierarchyInfoRecord
{
    public:
        u_int64_t m_node_guid;

    public:
        int32_t  m_campus_serial_num;
        int32_t  m_room_serial_num;
        int32_t  m_rack_serial_num;
        int32_t  m_system_type;
        int32_t  m_system_topu_num;
        int32_t  m_board_type;
        int32_t  m_board_slot_num;
        int32_t  m_device_serial_num;
        // template 6
        int32_t m_device_num_on_cpu_node;
        int32_t m_cpu_node_number;
        int32_t m_chassis_slot_index;
        int32_t m_tray_index;
        int32_t m_topology_id;
        bool     m_has_na;

    public:
        PhysicalHierarchyInfoRecord()
            :m_node_guid(0), m_campus_serial_num(0), m_room_serial_num(0),
             m_rack_serial_num(0), m_system_type(0), m_system_topu_num(0),
             m_board_type(0), m_board_slot_num(0), m_device_serial_num(0),
             m_device_num_on_cpu_node(0), m_cpu_node_number(0), m_chassis_slot_index(0),
             m_tray_index(0), m_topology_id(0), m_has_na(false)
        {}

    public:
        static int Init(vector < ParseFieldInfo <class PhysicalHierarchyInfoRecord> > &parse_section_info);
};

//
// Section AR_INFO
// - adaptive_routing_info
//
class ARInfoRecord {
    public:
        u_int64_t               m_node_guid;
        adaptive_routing_info   m_data;

    public:
        ARInfoRecord() : m_node_guid(0), m_data({}) {}

        static int Init(vector < ParseFieldInfo <class ARInfoRecord> > &parse_section_info);
};

//
// Section LINKS
//
class LinkRecord {
    public:
        u_int64_t   node_guid1;
        u_int8_t    port_num1;
        u_int64_t   node_guid2;
        u_int8_t    port_num2;

    public:
        LinkRecord(): node_guid1(0), port_num1(0), node_guid2(0), port_num2(0) {};

        static int Init(vector < ParseFieldInfo <class LinkRecord> > &parse_section_info);
};

//
// Section START_GENERAL_INFO_SMP
//
class GeneralInfoSMPRecord {
    public:
        u_int64_t     m_node_guid;
        std::string   m_fw_info_extended_major;
        std::string   m_fw_info_extended_minor;
        std::string   m_fw_info_extended_sub_minor;
        string        m_capability_mask_fields[NUM_CAPABILITY_FIELDS];

    public:
        GeneralInfoSMPRecord() : m_node_guid(0) {}

        static int Init(vector < ParseFieldInfo <class GeneralInfoSMPRecord> > &parse_section_info);
};

//
// Section START_EXTENDED_SWITCH_INFO
//  - SMP_ExtendedSwitchInfo
//
class ExtendedSwitchInfoRecord {
    public:
        u_int64_t                         m_node_guid;
        struct SMP_ExtendedSwitchInfo     m_data;
        bool                              m_has_na;

    public:
        ExtendedSwitchInfoRecord() : m_node_guid(0), m_data({}), m_has_na(false) {}

        static int Init(vector < ParseFieldInfo <class ExtendedSwitchInfoRecord> > &parse_section_info);
};

//
// Section NODES_INFO
//  - VendorSpec_GeneralInfo
//
class GeneralInfoGMPRecord {
    public:
        u_int64_t                     m_node_guid;
        struct VendorSpec_GeneralInfo m_data;
        bool                          m_has_na;
        bool                          m_cap_has_na;
        GeneralInfoGMPRecord() : m_node_guid(0), m_data({}), m_has_na(false), m_cap_has_na(false) {}

        static int Init(vector < ParseFieldInfo <class GeneralInfoGMPRecord> > &parse_section_info);
};

//
// Section PM_INFO
//  - PM_PortCounters
//  - PM_PortCountersExtended
//  - PM_PortRcvErrorDetails
//  - PM_PortXmitDiscardDetails
//
class PMInfoRecord {
    public:
        u_int64_t                           m_node_guid;
        u_int64_t                           m_port_guid;
        u_int8_t                            m_port_num;
        struct PM_PortCounters              m_pm_port_counters;
        struct PM_PortCountersExtended      m_pm_port_counters_ext;
        struct PM_PortRcvErrorDetails       m_pm_port_rcv_error_details;
        struct PM_PortXmitDiscardDetails    m_pm_port_xmit_discard_details;
    
        PMInfoRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0),
              m_pm_port_counters({}), m_pm_port_counters_ext({}),
              m_pm_port_rcv_error_details({}), m_pm_port_xmit_discard_details({})
        {}
    
        static int Init(vector < ParseFieldInfo <class PMInfoRecord> > &parse_section_info);
};

//
// Section HBF_PORT_COUNTERS
//  - port_routing_decision_counters
//
class HBFPortCountersRecord {
    public:
        u_int64_t                               m_node_guid;
        u_int64_t                               m_port_guid;
        u_int8_t                                m_port_num;
        struct port_routing_decision_counters   m_data;
    
        HBFPortCountersRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({})
        {}
    
        static int Init(vector < ParseFieldInfo <class HBFPortCountersRecord> > &parse_section_info);
};

//
// Section FAST_RECOVERY_COUNTERS
//  - VS_FastRecoveryCounters
//
class FastRecoveryCountersRecord {
    public:
        u_int64_t                           m_node_guid;
        u_int64_t                           m_port_guid;
        u_int8_t                            m_port_num;
        struct VS_FastRecoveryCounters      m_data;
    
        FastRecoveryCountersRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({})
        {}
    
        static int Init(vector < ParseFieldInfo <class FastRecoveryCountersRecord> > &parse_section_info);
};

//
// Section CREDIT_WATCHDOG_TIMEOUT_COUNTERS
//  - VS_CreditWatchdogTimeoutCounters
//
class CreditWatchdogTimeoutCountersRecord {
    public:
        u_int64_t                               m_node_guid;
        u_int64_t                               m_port_guid;
        u_int8_t                                m_port_num;
        struct VS_CreditWatchdogTimeoutCounters m_data;
    
        CreditWatchdogTimeoutCountersRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({})
        {}
    
        static int Init(vector < ParseFieldInfo <class CreditWatchdogTimeoutCountersRecord> > &parse_section_info);
};

//
// Section RN_COUNTERS
//  - port_rn_counters
//
class RNCountersRecord {
    public:
        u_int64_t                           m_node_guid;
        u_int64_t                           m_port_guid;
        u_int8_t                            m_port_num;
        struct port_rn_counters             m_data;

        RNCountersRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({})
        {}

        static int Init(vector < ParseFieldInfo <class RNCountersRecord> > &parse_section_info);
};

//
// Section PM_PORT_SAMPLES_CONTROL
//  - PM_PortSamplesControl
//
class PMPortSamplesControlRecord {
 public:
        u_int64_t                               m_node_guid;
        u_int64_t                               m_port_guid;
        u_int8_t                                m_port_num;
        struct PM_PortSamplesControl            m_data;

        PMPortSamplesControlRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0), m_data({})
        {}

        static int Init(vector < ParseFieldInfo <class PMPortSamplesControlRecord> > &parse_section_info);
};

//
// Section CHASSIS_INFO
//  - SMP_ChassisInfo
//
class ChassisInfoRecord {
 public:
        u_int64_t                               m_node_guid;
        struct SMP_ChassisInfo                  m_data;

        ChassisInfoRecord()
            : m_node_guid(0), m_data({})
        {}

        static int Init(vector < ParseFieldInfo <class ChassisInfoRecord> > &parse_section_info);
};

class CableInfoRecord {
 public:
    struct CombinedCableInfo {
        std::string Source;
        std::string Vendor;
        u_int32_t OUI;
        std::string PN;
        std::string SN;
        std::string Rev;
        std::string LengthSMFiber;
        std::string LengthOM5;
        std::string LengthOM4;
        std::string LengthOM3;
        std::string LengthOM2;
        std::string LengthOM1;
        std::string LengthCopperOrActive;
        u_int8_t Identifier;
        std::string IdentifierStr;
        u_int8_t Connector;
        u_int32_t Type;
        u_int16_t SupportedSpeed;
        std::string LengthDesc;
        std::string LengthDescByPRTL;
        std::string LengthDescByReg;
        std::string TypeDesc;
        std::string SupportedSpeedDesc;
        std::string Temperature;
        u_int8_t PowerClass;
        u_int64_t NominalBitrate;
        std::string CDREnableTxRx;
        std::string CDREnableTx;
        std::string CDREnableRx;
        u_int8_t InputEq;
        u_int8_t OutputAmp;
        std::string OutputEmp;
        u_int8_t OutputPreEmp;
        u_int8_t OutputPostEmp;
        std::string FWVersion;
        std::string Attenuation2_5G;
        u_int8_t Attenuation5G;
        u_int8_t Attenuation7G;
        u_int8_t Attenuation12G;
        u_int8_t Attenuation25G;
        std::string RXPowerType;
        u_int16_t RX1Power;
        u_int16_t RX2Power;
        u_int16_t RX3Power;
        u_int16_t RX4Power;
        u_int16_t RX5Power;
        u_int16_t RX6Power;
        u_int16_t RX7Power;
        u_int16_t RX8Power;
        float TX1Bias;
        float TX2Bias;
        float TX3Bias;
        float TX4Bias;
        float TX5Bias;
        float TX6Bias;
        float TX7Bias;
        float TX8Bias;
        u_int16_t TX1Power;
        u_int16_t TX2Power;
        u_int16_t TX3Power;
        u_int16_t TX4Power;
        u_int16_t TX5Power;
        u_int16_t TX6Power;
        u_int16_t TX7Power;
        u_int16_t TX8Power;
        u_int8_t RX1LatchedLossIndicator;
        u_int8_t RX2LatchedLossIndicator;
        u_int8_t RX3LatchedLossIndicator;
        u_int8_t RX4LatchedLossIndicator;
        u_int8_t TX1LatchedLossIndicator;
        u_int8_t TX2LatchedLossIndicator;
        u_int8_t TX3LatchedLossIndicator;
        u_int8_t TX4LatchedLossIndicator;
        u_int8_t TX1AdaptiveEqualizationFaultIndicator;
        u_int8_t TX2AdaptiveEqualizationFaultIndicator;
        u_int8_t TX3AdaptiveEqualizationFaultIndicator;
        u_int8_t TX4AdaptiveEqualizationFaultIndicator;
        u_int8_t RX1CDRLOL;
        u_int8_t RX2CDRLOL;
        u_int8_t RX3CDRLOL;
        u_int8_t RX4CDRLOL;
        u_int8_t TX1CDRLOL;
        u_int8_t TX2CDRLOL;
        u_int8_t TX3CDRLOL;
        u_int8_t TX4CDRLOL;
        u_int8_t HighTemperatureAlarm;
        u_int8_t LowTemperatureAlarm;
        u_int8_t HighTemperatureWarning;
        u_int8_t LowTemperatureWarning;
        std::string InitializationFlagComplete;
        u_int8_t HighSupplyVoltageAlarm;
        u_int8_t LowSupplyVoltageAlarm;
        u_int8_t HighSupplyVoltageWarning;
        u_int8_t LowSupplyVoltageWarning;
        u_int8_t HighRX1PowerAlarm;
        u_int8_t LowRX1PowerAlarm;
        u_int8_t HighRX1PowerWarning;
        u_int8_t LowRX1PowerWarning;
        u_int8_t HighRX2PowerAlarm;
        u_int8_t LowRX2PowerAlarm;
        u_int8_t HighRX2PowerWarning;
        u_int8_t LowRX2PowerWarning;
        u_int8_t HighRX3PowerAlarm;
        u_int8_t LowRX3PowerAlarm;
        u_int8_t HighRX3PowerWarning;
        u_int8_t LowRX3PowerWarning;
        u_int8_t HighRX4PowerAlarm;
        u_int8_t LowRX4PowerAlarm;
        u_int8_t HighRX4PowerWarning;
        u_int8_t LowRX4PowerWarning;
        u_int8_t HighTX1BiasAlarm;
        u_int8_t LowTX1BiasAlarm;
        u_int8_t HighTX1BiasWarning;
        u_int8_t LowTX1BiasWarning;
        u_int8_t HighTX2BiasAlarm;
        u_int8_t LowTX2BiasAlarm;
        u_int8_t HighTX2BiasWarning;
        u_int8_t LowTX2BiasWarning;
        u_int8_t HighTX3BiasAlarm;
        u_int8_t LowTX3BiasAlarm;
        u_int8_t HighTX3BiasWarning;
        u_int8_t LowTX3BiasWarning;
        u_int8_t HighTX4BiasAlarm;
        u_int8_t LowTX4BiasAlarm;
        u_int8_t HighTX4BiasWarning;
        u_int8_t LowTX4BiasWarning;
        u_int8_t HighTX1PowerAlarm;
        u_int8_t LowTX1PowerAlarm;
        u_int8_t HighTX1PowerWarning;
        u_int8_t LowTX1PowerWarning;
        u_int8_t HighTX2PowerAlarm;
        u_int8_t LowTX2PowerAlarm;
        u_int8_t HighTX2PowerWarning;
        u_int8_t LowTX2PowerWarning;
        u_int8_t HighTX3PowerAlarm;
        u_int8_t LowTX3PowerAlarm;
        u_int8_t HighTX3PowerWarning;
        u_int8_t LowTX3PowerWarning;
        u_int8_t HighTX4PowerAlarm;
        u_int8_t LowTX4PowerAlarm;
        u_int8_t HighTX4PowerWarning;
        u_int8_t LowTX4PowerWarning;
        std::string SupplyVoltageReporting;
        u_int8_t TransmitterTechnology;
        std::string ActiveWavelengthControl;
        std::string CooledTransmitterDevice;
        std::string ActivePinDetector;
        std::string TunableTransmitter;
        u_int8_t ExtendedSpecificationComplianceCodes;
        std::string AlarmTemperatureHighThresh;
        std::string AlarmTemperatureLowThresh;
        std::string WarnTemperatureHighThresh;
        std::string WarnTemperatureLowThresh;
        std::string AlarmVoltageHighThresh;
        std::string AlarmVoltageLowThresh;
        std::string WarnVoltageHighThresh;
        std::string WarnVoltageLowThresh;
        u_int16_t RXPowerHighThresh;
        u_int16_t RXPowerLowThresh;
        u_int16_t TXPowerHighThresh;
        u_int16_t TXPowerLowThresh;
        float TXBiasHighThresh;
        float TXBiasLowThresh;
        std::string DateCode;
        u_int16_t Lot;
        std::string TX1AdaptiveEqualizationFreeze;
        std::string TX2AdaptiveEqualizationFreeze;
        std::string TX3AdaptiveEqualizationFreeze;
        std::string TX4AdaptiveEqualizationFreeze;
        std::string RXOutputDisable;
        std::string TXAdaptiveEqualizationEnable;

        std::string MaxPower;
        u_int8_t CdrVendor;
        u_int16_t MaxFiberLength;
    } CombinedCableInfo;

    public:
        u_int64_t                               m_node_guid;
        u_int64_t                               m_port_guid;
        u_int8_t                                m_port_num;
        struct CombinedCableInfo                m_data;

        CableInfoRecord()
            : m_node_guid(0), m_port_guid(0), m_port_num(0){}

        static int Init(vector < ParseFieldInfo <class CableInfoRecord> > &parse_section_info);
};
