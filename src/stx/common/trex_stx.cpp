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
#include "publisher/trex_publisher.h"


/***********************************************************
 * Trex STX base object
 * 
 **********************************************************/

TrexSTX::TrexSTX(const TrexSTXCfg &cfg) : m_rpc_server(cfg.m_rpc_req_resp_cfg), m_cfg(cfg) {
    m_dp_core_count = get_platform_api().get_dp_core_count();
    m_ticket_id = 1; // 0 is reserved for initial configs
    for (int i=0; i<TREX_MAX_PORTS; i++){
        m_session_id_ports[i]=0;
    }
}

void TrexSTX::add_session_id(uint32_t session_id){
    get_publisher()->add_session_id(session_id, true);  // reader mode
}

void TrexSTX::add_session_id(uint8_t port_id, 
                             uint32_t session_id){
    assert(session_id!=0);
    int new_ref= get_ref_session_id(session_id);

    uint32_t old_id = m_session_id_ports[port_id];
    int old_ref= get_ref_session_id(old_id);

    m_session_id_ports[port_id] = session_id;
    if ((get_ref_session_id(session_id)== 1) && (new_ref==0)){
        get_publisher()->add_session_id(session_id, false); // port owner mode
    }   
    if ((old_id>0) && (old_ref==1) && (get_ref_session_id(old_id)==0)){
        get_publisher()->remove_session_id(old_id);
    }
}

int TrexSTX::get_ref_session_id(uint32_t session_id){
    if (session_id==0){
        return(0);
    }
   int cnt=0;
   for (int i=0; i<TREX_MAX_PORTS; i++){
        if ( m_session_id_ports[i] == session_id ){
            cnt++;
        }
    }
    return cnt;
}


void TrexSTX::remove_session_id(uint8_t port_id){
    uint32_t old_id = m_session_id_ports[port_id];
    m_session_id_ports[port_id]=0;
    if ((old_id>0) && (get_ref_session_id(old_id) == 0)){
        get_publisher()->remove_session_id(old_id);
    }
}

/** 
 * release all memory 
 * 
 * @author imarom (08-Oct-15)
 */
TrexSTX::~TrexSTX() {
}

uint64_t TrexSTX::get_ticket(void) {
    return m_ticket_id++;
}

#define MAX_STX_TICKETS 50

void clean_old_tickets(async_ticket_map_t &ticket_map) {
    while ( ticket_map.size() > MAX_STX_TICKETS ) {
        ticket_map.erase(ticket_map.begin());
    }
}

void TrexSTX::add_task_by_ticket(uint64_t ticket_id, async_ticket_task_t &task) {
    clean_old_tickets(m_async_task_by_ticket);
    m_async_task_by_ticket[ticket_id] = task;
}

bool TrexSTX::get_task_by_ticket(uint64_t ticket_id, async_ticket_task_t &task) {
    auto it = m_async_task_by_ticket.find(ticket_id);
    if ( it == m_async_task_by_ticket.end() ) {
        return false;
    }
    task = it->second;
    m_async_task_by_ticket.erase(it);
    return true;
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
        ring->SecureEnqueue((CGenNode*)msg->clone(),true);
    }
    
    delete msg;
}

/**
 * sends a message to a DP core
 */
void
TrexSTX::send_msg_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg) {
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    ring->SecureEnqueue((CGenNode*)msg->clone(),true);
    delete msg;
}

/**
 * send message to RX core
 */
void
TrexSTX::send_msg_to_rx(TrexCpToRxMsgBase *msg) const {

    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->SecureEnqueue((CGenNode *)msg,true);
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

    /* limit the number of handling messages at once to avoid watchdog timeout,
     * and give a chance to handle a waiting message by RPC server thread.
     */
    for ( int handle_cnt = 0; handle_cnt < 100; handle_cnt++ ) {
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

void
TrexSTX::dp_core_finished(int thread_id, uint32_t profile_id) {
}

void
TrexSTX::dp_core_finished_partial(int thread_id, uint32_t profile_id) {
}

void
TrexSTX::add_dp_profile_ctx(uint32_t profile_id, void* client, void* server) {
}

void
TrexSTX::dp_core_error(int thread_id, uint32_t profile_id, const std::string &err) {
}

void
TrexSTX::dp_core_state(int thread_id, int state) {
}

/**
 * check for messages that arrived from DP to CP
 *
 */
void
TrexSTX::check_for_dp_messages() {

    /* for all the cores - check for a new message */
    for (int i = 0; i < m_dp_core_count; i++) {
        CNodeRing *ring_to_dp = CMsgIns::Ins()->getCpDp()->getRingCpToDp(i);
        ring_to_dp->Reschedule();

        check_for_dp_message_from_core(i);
    }
}

bool
TrexSTX::has_dp_messages() {
    for (int i = 0; i < m_dp_core_count; i++) {
        CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingDpToCp(i);
        if ( ! ring->isEmpty() ) {
            return true;
        }
    }
    return false;
}
