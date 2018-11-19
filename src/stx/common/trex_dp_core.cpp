/*
 Itay Marom
 Hanoch Haim
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

#include "trex_dp_core.h"
#include "trex_messaging.h"
#include "bp_sim.h"

TrexDpCore::TrexDpCore(uint32_t thread_id, CFlowGenListPerThread *core, state_e init_state) {
    m_thread_id  = thread_id;
    m_core       = core;
    
    CMessagingManager *cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(thread_id);
    m_ring_to_cp   = cp_dp->getRingDpToCp(thread_id);
    
    m_state  = init_state;
}



void
TrexDpCore::barrier(uint8_t port_id, int event_id) {

    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(m_core->m_thread_id);
    TrexDpToCpMsgBase *event_msg = new TrexDpPortEventMsg(m_core->m_thread_id,
                                                          port_id,
                                                          event_id);
    ring->Enqueue((CGenNode *)event_msg);
}



/**
 * in idle state loop, the processor most of the time sleeps
 * and periodically checks for messages
 *
 * @author imarom (01-Nov-15)
 */
void
TrexDpCore::idle_state_loop() {

    const int SHORT_DELAY_MS    = 2;
    const int LONG_DELAY_MS     = 50;
    const int DEEP_SLEEP_LIMIT  = 2000;

    int counter = 0;

    while (m_state == STATE_IDLE) {
        m_core->tickle();
        
        bool had_msg = m_core->check_msgs();
        if (had_msg) {
            counter = 0;
            continue;
        }

        bool had_rx = rx_for_idle();
        if (had_rx) {
            counter = 0;
            continue;
        }

        /* enter deep sleep only if enough time had passed */
        if (counter < DEEP_SLEEP_LIMIT) {
            delay(SHORT_DELAY_MS);
            counter++;
        } else {
            delay(LONG_DELAY_MS);
        }

    }
}

bool
TrexDpCore::rx_for_idle(void) {
    return false;
}

void
TrexDpCore::start_once() {
    /* for simulation */
    
    if ( (m_state == STATE_IDLE) && (are_any_pending_cp_messages()) ) {
        idle_state_loop();
    }
    
    start_scheduler();
}


void
TrexDpCore::start() {

    while (true) {
        
        switch (m_state) {
        case STATE_IDLE:
            idle_state_loop();
            break;
            
        case STATE_TRANSMITTING:
        case STATE_PCAP_TX:
            start_scheduler();
            break;
            
        case STATE_TERMINATE:
            return;
        }
        
    }
    
}


void TrexDpCore::stop() {
    m_core->set_terminate_mode(true); /* mark it as terminated */
    m_state = STATE_TERMINATE;
    add_global_duration(0.0001);
}


void
TrexDpCore::add_global_duration(double duration) {
    if (duration > 0.0) {
        CGenNode *node = m_core->create_node() ;

        node->m_type = CGenNode::EXIT_SCHED;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration;

        m_core->m_node_gen.add_node(node);
    }
}


/**
 * handle a message from CP to DP
 *
 */
void
TrexDpCore::handle_cp_msg(TrexCpToDpMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

