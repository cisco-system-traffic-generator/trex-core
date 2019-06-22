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
#ifndef __TREX_ASTF_BATCH_H__
#define __TREX_ASTF_BATCH_H__

#include "trex_stx.h"
#include "trex_messaging.h"
#include "publisher/trex_publisher.h"
#include "stt_cp.h"

/**
 * ASTF batch core
 */
class TrexDpCoreAstfBatch : public TrexDpCore {
public:
    
    TrexDpCoreAstfBatch(uint32_t thread_id, CFlowGenListPerThread *core);
    ~TrexDpCoreAstfBatch(void);

    /* stateful always on */
    virtual bool are_all_ports_idle() override {
        return false;
    }
    
    /* stateful always on */
    virtual bool is_port_active(uint8_t port_id) override {
        return true;
    }
    
protected:
    
    void start_astf();
    
    virtual void start_scheduler() override {

        start_astf();
        
        /* in batch - no more iterations */
        m_state = STATE_TERMINATE;
    }
};


/**
 * ASTF batch
 * 
 * @author imarom (9/13/2017)
 */
class TrexAstfBatch : public TrexSTX {
public:

    TrexAstfBatch(const TrexSTXCfg &cfg, CLatencyManager *mg) : TrexSTX(cfg) {
        
        /* no RPC or ports */
        
        /* RX core */
        m_rx = mg;
    }
    
    
    /**
     * create and allocate a DP core object
     *
     */
    virtual TrexDpCore * create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) override {
        return new TrexDpCoreAstfBatch(thread_id, core);
    }
    
    
    
    /* nothing on the control plane side */
    void launch_control_plane() override {}

    
    /**
     * shutdown for ASTF batch mode
     * 
     */
    void shutdown() override;
    
  
    void publish_async_data() override;

    
private:

    CLatencyManager *get_mg() {
        return static_cast<CLatencyManager *>(m_rx);
    }
    
};

#endif /* __TREX_ASTF_BATCH_H__ */
