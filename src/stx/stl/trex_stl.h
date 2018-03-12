/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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
#ifndef __TREX_STL_H__
#define __TREX_STL_H__

#include <stdint.h>
#include <vector>

#include "common/trex_stx.h"

class TrexStatelessPort;

typedef std::unordered_map<uint8_t, TrexStatelessPort*> stl_port_map_t;

/**
 * TRex stateless interactive object
 * 
 */
class TrexStateless : public TrexSTX {
public:

    /** 
     * create stateless object 
     */
    TrexStateless(const TrexSTXCfg &cfg);
    virtual ~TrexStateless();

    
    /**
     * launch control plane
     * 
     */
    void launch_control_plane();
    
    
    /**
     * shutdown
     * 
     */
    void shutdown();
    
    
    /**
     * create a STL DP core
     * 
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);
    
    
    /**
     * fetch stateless port object by port ID
     * 
     */
    TrexStatelessPort * get_port_by_id(uint8_t port_id);


    /**
     * publish stateless async data
     * 
     */
    void publish_async_data();


    /**
     * returns the port list as stateless port objects
     */
    const stl_port_map_t &get_port_map() const {
        return *(stl_port_map_t *)&m_ports;
    }


    CRxCore *get_stl_rx(){
        return static_cast<CRxCore *>(m_rx);
    }
    
protected:
    
    void _shutdown();
    
};


/**
 * returns the stateless object 
 * if mode is not stateless, will return NULL 
 */
static inline TrexStateless * get_stateless_obj() {
    return dynamic_cast<TrexStateless *>(get_stx());
}

#endif /* __TREX_STL_H__ */

