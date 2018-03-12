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

#include "trex_stx.h"
#include "msg_manager.h"
#include "trex_messaging.h"

/***********************************************************
 * Trex STX base object
 * 
 **********************************************************/

TrexSTX::TrexSTX(const TrexSTXCfg &cfg) : m_rpc_server(cfg.m_rpc_req_resp_cfg), m_cfg(cfg) {
    m_dp_core_count = get_platform_api().get_dp_core_count();
}


/** 
 * release all memory 
 * 
 * @author imarom (08-Oct-15)
 */
TrexSTX::~TrexSTX() {
}


/**
 * fetch a port by ID
 * 
 */
TrexPort *
TrexSTX::get_port_by_id(uint8_t port_id) {
    if (m_ports.find(port_id) == m_ports.end()) {
        throw TrexException("Port index out of range");
    }

    return m_ports[port_id];

}



/**
 * returns the port count
 */
uint8_t 
TrexSTX::get_port_count() const {
    return m_ports.size();
}


/**
 * sends a message to all the DP cores 
 * (regardless of ports) 
 */
void
TrexSTX::send_msg_to_all_dp(TrexCpToDpMsgBase *msg) {

    int max_threads=(int)CMsgIns::Ins()->getCpDp()->get_num_threads();
    
    for (int i = 0; i < max_threads; i++) {
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp((uint8_t)i);
        ring->Enqueue((CGenNode*)msg->clone());
    }
    
    delete msg;
}


/**
 * send message to RX core
 */
void
TrexSTX::send_msg_to_rx(TrexCpToRxMsgBase *msg) const {

    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->Enqueue((CGenNode *)msg);
}


/**
 * check for a message from DP core
 * 
 */
void
TrexSTX::check_for_dp_message_from_core(int thread_id) {

    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(thread_id);

    /* fast path check */
    if ( likely ( ring->isEmpty() ) ) {
        return;
    }

    while ( true ) {
        CGenNode * node = NULL;
        if (ring->Dequeue(node) != 0) {
            break;
        }
        assert(node);

        TrexDpToCpMsgBase * msg = (TrexDpToCpMsgBase *)node;
        msg->handle();
        delete msg;
    }

}

/**
 * check for messages that arrived from DP to CP
 *
 */
void
TrexSTX::check_for_dp_messages() {

    /* for all the cores - check for a new message */
    for (int i = 0; i < m_dp_core_count; i++) {
        check_for_dp_message_from_core(i);
    }
}

