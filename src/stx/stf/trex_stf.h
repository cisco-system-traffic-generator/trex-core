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
#ifndef __TREX_STF_H__
#define __TREX_STF_H__

#include "trex_stx.h"
#include "trex_messaging.h"
#include "publisher/trex_publisher.h"

/**
 * a degerenated object for old stateful
 */
class TrexDpCoreStf : public TrexDpCore {
public:

    TrexDpCoreStf(uint32_t thread_id, CFlowGenListPerThread *core) : TrexDpCore(thread_id, core, STATE_TRANSMITTING) {
    }

    /* stateful always on */
    virtual bool are_all_ports_idle() override {
        return false;
    }

    /* stateful always on */
    virtual bool is_port_active(uint8_t port_id) override {
        return true;
    }

protected:

    void start_stf();


    virtual void start_scheduler() override {
        start_stf();
        /* in batch - no more iterations */
        m_state = STATE_TERMINATE;
    }
};


/**
 * a degenerated object for old stateful
 *
 * @author imarom (9/13/2017)
 */
class TrexStateful : public TrexSTX {
public:

    TrexStateful(const TrexSTXCfg &cfg, CLatencyManager *mg) : TrexSTX(cfg) {
        /* no RPC or ports */

        /* RX core */
        m_rx = mg;
    }


    /* nothing on the control plane side */
    void launch_control_plane() override {}


    void shutdown() override;


    void publish_async_data() override;


    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core) override {
        return new TrexDpCoreStf(thread_id, core);
    }



private:

    /* dump the template info */
    void dump_template_info(std::string & json);


    CLatencyManager *get_mg() {
        return static_cast<CLatencyManager *>(m_rx);
    }


    volatile bool          m_is_rx_active;
};

#endif /* __TREX_STF_H__ */
