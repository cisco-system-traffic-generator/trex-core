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
#ifndef __TREX_STATELESS_DP_CORE_H__
#define __TREX_STATELESS_DP_CORE_H__

#include <vector>

#include <msg_manager.h>
#include <pal_utl.h>

class TrexStatelessCpToDpMsgBase;
class TrexStatelessDpStart;
class CFlowGenListPerThread;
class CGenNodeStateless;
class TrexStreamsCompiledObj;

class TrexStatelessDpCore {

public:

    /* states */
    enum state_e {
        STATE_IDLE,
        STATE_TRANSMITTING
    };

    TrexStatelessDpCore(uint8_t thread_id, CFlowGenListPerThread *core);

    /**
     * launch the stateless DP core code
     * 
     */
    void start();

    /**
     * dummy traffic creator
     * 
     * @author imarom (27-Oct-15)
     * 
     * @param pkt 
     * @param pkt_len 
     */
    void start_traffic(TrexStreamsCompiledObj *obj);

    /**
     * stop all traffic for this core
     * 
     */
    void stop_traffic(uint8_t port_id);

    /**
     * check for and handle messages from CP
     * 
     * @author imarom (27-Oct-15)
     */
    void periodic_check_for_cp_messages() {
        // doing this inline for performance reasons

        /* fast path */
        if ( likely ( m_ring_from_cp->isEmpty() ) ) {
            return;
        }

        while ( true ) {
            CGenNode * node = NULL;
            if (m_ring_from_cp->Dequeue(node) != 0) {
                break;
            }
            assert(node);

            TrexStatelessCpToDpMsgBase * msg = (TrexStatelessCpToDpMsgBase *)node;
            handle_cp_msg(msg);
        }

    }

private:
    /**
     * handles a CP to DP message
     * 
     * @author imarom (27-Oct-15)
     * 
     * @param msg 
     */
    void handle_cp_msg(TrexStatelessCpToDpMsgBase *msg);

    void add_cont_stream(double pps, const uint8_t *pkt, uint16_t pkt_len);

    uint8_t              m_thread_id;
    state_e              m_state;
    CNodeRing           *m_ring_from_cp;
    CNodeRing           *m_ring_to_cp;

    /* holds the current active nodes */
    std::vector<CGenNodeStateless *> m_active_nodes;

    /* pointer to the main object */
    CFlowGenListPerThread *m_core;
};

#endif /* __TREX_STATELESS_DP_CORE_H__ */

