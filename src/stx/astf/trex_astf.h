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
#ifndef __TREX_ASTF_H__
#define __TREX_ASTF_H__

#include <stdint.h>

#include "common/trex_stx.h"


/**
 * TRex adavanced stateful interactive object
 * 
 */
class TrexAstf : public TrexSTX {
public:

    /** 
     * create ASTF object 
     */
    TrexAstf(const TrexSTXCfg &cfg);
    virtual ~TrexAstf();

    
    /**
     * ASTF control plane
     * 
     */
    void launch_control_plane();
    
    
    /**
     * shutdown ASTF
     * 
     */
    void shutdown();
    
    
    /**
     * async data sent for ASTF
     * 
     */
    void publish_async_data();
    
    
    /**
     * create a ASTF DP core
     * 
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);
  
    
protected:
};


/**
 * returns the ASTF object 
 * if mode is not ASTF, will return NULL 
 */
static inline TrexAstf * get_astf_object() {
    return dynamic_cast<TrexAstf *>(get_stx());
}

#endif /* __TREX_STL_H__ */

