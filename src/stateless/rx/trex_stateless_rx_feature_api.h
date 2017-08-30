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

#ifndef __TREX_STATELESS_RX_FEATURE_API_H__
#define __TREX_STATELESS_RX_FEATURE_API_H__

#include <stdint.h>
#include "mbuf.h"

class RXPortManager;
class VLANConfig;
class CManyIPInfo;

/**
 * a general API class to provide common functions to 
 * all RX features 
 *  
 * it simply exposes part of the RX port mngr instead of 
 * a full back pointer 
 *  
 */
class RXFeatureAPI {
public:

    RXFeatureAPI(RXPortManager *port_mngr) {
        m_port_mngr = port_mngr;
    }
    
    /**
     * sends a packet through the TX queue
     * 
     */
    bool tx_pkt(const std::string &pkt);
    bool tx_pkt(rte_mbuf_t *m);
    
    /**
     * returns the port id associated with the port manager
     * 
     */
    uint8_t get_port_id();
    
    /**
     * returns the port VLAN configuration
     */
    const VLANConfig *get_vlan_cfg();
    
    /**
     * source addresses associated with the port
     * 
     */
    CManyIPInfo *get_src_addr();
    
    /**
     * returns the *first* IPv4 address associated with the port
     */
    uint32_t get_src_ipv4();
    
private:
    
    RXPortManager *m_port_mngr;
};

#endif /* __TREX_STATELESS_RX_FEATURE_API_H__ */
