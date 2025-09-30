/*
 * Copyright (c) 2022-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef IBDM_CABLE_RECORD_DATA_H
#define IBDM_CABLE_RECORD_DATA_H

typedef struct
{
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
    u_int8_t RXpower_type; // '0'-OMA, '1'-AVP

    u_int32_t qsfp_options;

    char vendor[33];
    char oui[33];
    char pn[33];
    char sn[33];
    char rev[33];
    char length_str[33];
    char mlnx_sfg_sn[33];
    char fw_version[41];
    char date_code[17];
} cable_record_data_t;

#endif
