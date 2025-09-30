/*
 * Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef IBDM_PRTL_REC_H
#define IBDM_PRTL_REC_H

#include <string>

using namespace std;

class PrtlRecord {
public:
    PrtlRecord();
    ~PrtlRecord() {}

    bool IsSupported() const { return (rtt_support ? true : false); }
    float CalculateLength(const PrtlRecord &remote, string &message) const;
    string ToString() const;

    u_int8_t lp_msb;
    u_int8_t local_port;
    u_int8_t rtt_support;
    u_int8_t latency_accuracy;
    u_int8_t latency_res;
    u_int16_t local_phy_latency;
    u_int16_t local_mod_dp_latency;
    u_int32_t round_trip_latency;

private:
    float CalculateLength(const PrtlRecord &remote) const;

};

#endif
