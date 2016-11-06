/*
  Itay Marom
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __TREX_STATELESS_RX_DEFS_H__
#define __TREX_STATELESS_RX_DEFS_H__

#include "trex_defs.h"
#include <json/json.h>

class CPortLatencyHWBase;

/**
 * general SL cfg
 * 
 */
class CRxSlCfg {
 public:
    CRxSlCfg (){
        m_max_ports = 0;
        m_cps = 0.0;
    }

 public:
    uint32_t             m_max_ports;
    double               m_cps;
    CPortLatencyHWBase * m_ports[TREX_MAX_PORTS];
};

/**
 * describes the filter type applied to the RX 
 *  RX_FILTER_MODE_HW - only hardware filtered traffic will
 *  reach the RX core
 *  
 */
typedef enum rx_filter_mode_ {
	RX_FILTER_MODE_HW,
	RX_FILTER_MODE_ALL
} rx_filter_mode_e;

/**
 * holds RX capture info
 * 
 */
struct RXCaptureInfo {
    RXCaptureInfo() {
        m_is_active = false;
        m_limit = 0;
        m_shared_counter = 0;
    }

    void to_json(Json::Value &output) const {
        output["is_active"] = m_is_active;
        if (m_is_active) {
            output["pcap_filename"] = m_pcap_filename;
            output["limit"]         = Json::UInt64(m_limit);
            output["count"]         = Json::UInt64(m_shared_counter);
        }
    }

    bool             m_is_active;
    std::string      m_pcap_filename;
    uint64_t         m_limit;
    uint64_t         m_shared_counter;
};

#endif /* __TREX_STATELESS_RX_DEFS_H__ */
