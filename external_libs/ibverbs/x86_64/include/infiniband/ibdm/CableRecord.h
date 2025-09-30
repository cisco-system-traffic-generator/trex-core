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


#ifndef IBDM_CABLE_H
#define IBDM_CABLE_H

#include <string>
#include <ostream>

#include "infiniband/ibdm/cable/CableRecordData.h"
#include "infiniband/ibdm/cable/CableFW.h"

class PrtlRecord;

using namespace std;


class CableRecord
{
public:
    CableRecord():
        cable_return_status(0xff),
        identifier(CABLE_INFO_IDENTIFIER_NONE), connector(0), supported_speed(0),cable_type(0xff),
        lengthsmfiber(0), lengthom3(0), lengthom2(0), lengthom1(0),
        lengthcopper(0), temperature(0),
        nominal_br_100(0), nominal_br(0), power_class(0), cdr_control(0), cdr_present(0),
        mlnx_vendor_byte(0), attenuation_2_5g(0), attenuation_5g(0), attenuation_7g(0),
        attenuation_12g(0), eth_com_codes_10g_40g(0), eth_com_codes_ext(0), mlnx_revision(0),
        CDR_TX_RX_loss_indicator(0), adaptive_equalization_fault(0), TX_RX_LOL_indicator(0),
        temperature_alarm_and_Warning(0), voltage_alarm_and_warning(0),
        transmitter_technology(0), TX_adaptive_equalization_freeze(0), adaptive_eq_control(0),
        input_eq(0), output_amp(0), output_emp(0), mellanox_cap(0),
        diag_supply_voltage(0), RX_power_alarm_and_warning(0), TX_bias_alarm_and_warning(0),
        TX_power_alarm_and_warning(0), lot(0),
        high_temp_alarm_th(0), high_temp_warning_th(0), low_temp_alarm_th(0), low_temp_warning_th(0),
        high_vcc_alarm_th(0), high_vcc_warning_th(0), low_vcc_alarm_th(0), low_vcc_warning_th(0),
        RX1Power(0), RX2Power(0), RX3Power(0), RX4Power(0),
        TX1Bias(0), TX2Bias(0), TX3Bias(0), TX4Bias(0),
        TX1Power(0), TX2Power(0), TX3Power(0), TX4Power(0), RXpower_type(0),
        qsfp_options(0),
        vendor("NA"), oui("NA"), pn("NA"), sn("NA"), rev("NA"),
        length_str("NA"), mlnx_sfg_sn("NA"), fw_version("NA"), date_code("N/A") {}
    virtual ~CableRecord(){};

    bool IsModule() const;
    bool IsActiveCable() const;
    bool IsMlnxMmf() const;
    bool IsMlnxPsm() const;
    void ToCSVStream(ostream &stream) const;
    void ToFileStream(ostream &stream) const;
    string c_str(bool isCombined = false) const;
    int GetTemperatureAlarms() const;
    int GetTemperatureErrorsByTreshold() const;
    string GetTemperatureStr() const;
    string GetHighTemperatureThresholdStr() const;
    string GetLowTemperatureThresholdStr() const;
    cable_id_t GetIdObj() const;
    cable_fw_t GetFWObj() const;
    void SetPrtlLength(const string & length) { prtl_length = length; }
    bool IsOpticCable() const;
    string GetPNVendor() const;

protected:
    string ConvertLengthToStr() const; //used for exporting
    string ConvertPrtlLengthToStr() const;
    string ConvertTemperatureToStr(u_int16_t temp, bool is_csv) const;
    string ConvertVoltageToStr(u_int16_t vcc) const;
    string ConvertSupportedSpeedToStr() const;
    string ConvertCableIdentifierToStr() const;
    string ConvertCableTypeToStr() const;
    string ConvertCDREnableTxRxToStr(bool is_csv) const;
    string ConvertInputEqToStr(bool is_csv) const;
    string ConvertOutputAmpToStr(bool is_csv) const;
    string ConvertOutputEmpToStr(bool is_csv) const;
    string ConvertFwVersionToStr(bool is_csv) const;
    string ConvertAttenuationToStr(bool is_csv) const;
    string ConvertDateCodeToStr() const;
    string ConvertRXOutputDisableToStr() const;
    string ConvertTXAdaptiveEqualizationEnableToStr() const;
    bool IsPassiveCable() const;
    bool isValidTemperature(int8_t temp, u_int8_t cableType) const;
    double mW_to_dBm(double mW) const;

protected:
    u_int8_t cable_return_status;

    u_int8_t identifier;
    u_int8_t connector;
    u_int8_t supported_speed;
    u_int8_t cable_type;
    u_int8_t lengthsmfiber;
    u_int8_t lengthom3;
    u_int8_t lengthom2;
    u_int8_t lengthom1;
    u_int8_t lengthcopper;
    u_int16_t temperature;
    u_int8_t nominal_br_100;
    u_int8_t nominal_br;
    u_int8_t power_class;
    u_int8_t cdr_control;
    u_int8_t cdr_present;
    u_int8_t mlnx_vendor_byte;
    u_int8_t attenuation_2_5g;
    u_int8_t attenuation_5g;
    u_int8_t attenuation_7g;
    u_int8_t attenuation_12g;
    u_int8_t eth_com_codes_10g_40g;
    u_int8_t eth_com_codes_ext;
    u_int8_t mlnx_revision;
    u_int8_t CDR_TX_RX_loss_indicator;
    u_int8_t adaptive_equalization_fault;
    u_int8_t TX_RX_LOL_indicator;
    u_int8_t temperature_alarm_and_Warning;
    u_int8_t voltage_alarm_and_warning;
    u_int8_t transmitter_technology;
    u_int8_t TX_adaptive_equalization_freeze;
    u_int8_t adaptive_eq_control;

    u_int16_t input_eq;
    u_int16_t output_amp;
    u_int16_t output_emp;
    u_int16_t mellanox_cap;
    u_int16_t diag_supply_voltage;
    u_int16_t RX_power_alarm_and_warning;
    u_int16_t TX_bias_alarm_and_warning;
    u_int16_t TX_power_alarm_and_warning;
    u_int16_t lot;

    u_int16_t high_temp_alarm_th;
    u_int16_t high_temp_warning_th;
    u_int16_t low_temp_alarm_th;
    u_int16_t low_temp_warning_th;

    u_int16_t high_vcc_alarm_th;
    u_int16_t high_vcc_warning_th;
    u_int16_t low_vcc_alarm_th;
    u_int16_t low_vcc_warning_th;

    float RX1Power;
    float RX2Power;
    float RX3Power;
    float RX4Power;
    float TX1Bias;
    float TX2Bias;
    float TX3Bias;
    float TX4Bias;
    float TX1Power;
    float TX2Power;
    float TX3Power;
    float TX4Power;
    bool RXpower_type; // '0'-OMA, '1'-AVP

    u_int32_t qsfp_options;

    string vendor;
    string oui;
    string pn;
    string sn;
    string rev;
    string length_str;
    string mlnx_sfg_sn;
    string fw_version;
    string date_code;

    string prtl_length;

    // CMIS cable Identifiers
    enum CableInfoIdentifier {
        CABLE_INFO_IDENTIFIER_NONE    = 0,
        QSFP_ID_VALUE                 = 0x0c,
        QSFP_PLUS_ID_VALUE            = 0x0d,
        QSFP28_ID_VALUE               = 0x11,
        QSFP_DD_ID_VALUE              = 0x18,
        OSFP_ID_VALUE                 = 0x19,
        SFP_DD_ID_VALUE               = 0x1a,
        DSFP_ID_VALUE                 = 0x1b,
        QSFP_CMIS_ID_VALUE            = 0x1e
    };
};

#endif
