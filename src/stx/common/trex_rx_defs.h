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
        m_tx_cores = 0;
    }

    void create(uint32_t tx_cores, const std::unordered_map<uint8_t,CPortLatencyHWBase *> &ports) {
        m_tx_cores  = tx_cores;
        m_ports = ports;
    }
    
 public:
    uint32_t                            m_tx_cores;
    std::unordered_map<uint8_t, CPortLatencyHWBase *>   m_ports;
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


class CRXCoreIgnoreStat;

/**
 * RX core interface - used by both stateful RX core and 
 * STL/ASTF RX core 
 * 
 */
class TrexRxCore {
public:

    virtual ~TrexRxCore() {}
    
    
    /**
     * start the RX core
     * should be called from the RX thread
     */
    virtual void start() = 0;
    
    
    /**
     * return true if RX core is active
     * 
     */
    virtual bool is_active() = 0;
    
    
    /**
     * updates the RX core CPU util. 
     * (called from master) 
     * 
     */
    virtual void update_cpu_util() = 0;
    
    
    /**
     * get current CPU util.
     * 
     */
    virtual double get_cpu_util() = 0;
    
    
    /**
     * RX core PPS rate
     * 
     */
    virtual float get_pps_rate() = 0;
    
    
    /**
     * get ignored stats
     */
    virtual void get_ignore_stats(int port_id, CRXCoreIgnoreStat &ign_stats, bool get_diff) = 0;
    
};

#endif /* __TREX_STATELESS_RX_DEFS_H__ */

