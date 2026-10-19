/* Copyright (c) 2026 Cisco Systems, Inc.
 * Licensed under the Apache License, Version 2.0. See LICENSE.
 */
#ifndef __TREX_STL_ROCEV2_H__
#define __TREX_STL_ROCEV2_H__

#include <stdint.h>
#include <json/json.h>

/* Shared with the ICRC VM; calculates over a complete frame without FCS. */
uint32_t trex_roce_v2_calc_icrc_bytes(const uint8_t *packet, uint32_t length,
                                    uint16_t l2_len, uint16_t l3_len, uint16_t bth_offset);

namespace TrexRoceV2 {
/* Pure native packet/stream generation. Throws std::invalid_argument for an
 * invalid request. Does not install streams or perform network resolution. */
Json::Value build_streams(const Json::Value &config, uint32_t first_stream_id = 1);
}

#endif
