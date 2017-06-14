/*
 Itay Marom
 Hanoch Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
#include <string.h>

#include "trex_stateless_messaging.h"
#include "trex_stateless_dp_core.h"
#include "trex_stateless_rx_core.h"
#include "trex_streams_compiler.h"
#include "trex_stateless.h"
#include "bp_sim.h"

/*************************
  start traffic message
 ************************/
TrexStatelessDpStart::TrexStatelessDpStart(uint8_t port_id, int event_id, TrexStreamsCompiledObj *obj, double duration, double start_at_ts) {
    m_port_id = port_id;
    m_event_id = event_id;
    m_obj = obj;
    m_duration = duration;
    m_start_at_ts = start_at_ts;
}


/**
 * clone for DP start message
 *
 */
TrexStatelessCpToDpMsgBase *
TrexStatelessDpStart::clone() {

    TrexStreamsCompiledObj *new_obj = m_obj->clone();

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpStart(m_port_id, m_event_id, new_obj, m_duration, m_start_at_ts);

    return new_msg;
}

TrexStatelessDpStart::~TrexStatelessDpStart() {
    if (m_obj) {
        delete m_obj;
    }
}

bool
TrexStatelessDpStart::handle(TrexStatelessDpCore *dp_core) {

    /* start traffic */
    dp_core->start_traffic(m_obj, m_duration,m_event_id,m_start_at_ts);

    return true;
}

/*************************
  stop traffic message
 ************************/
bool
TrexStatelessDpStop::handle(TrexStatelessDpCore *dp_core) {


    dp_core->stop_traffic(m_port_id,m_stop_only_for_event_id,m_event_id);
    return true;
}


void TrexStatelessDpStop::on_node_remove(){
    if ( m_core ) {
        assert(m_core->m_non_active_nodes>0);
        m_core->m_non_active_nodes--;
    }
}


TrexStatelessCpToDpMsgBase * TrexStatelessDpPause::clone(){

    TrexStatelessDpPause *new_msg = new TrexStatelessDpPause(m_port_id);
    return new_msg;
}


bool TrexStatelessDpPause::handle(TrexStatelessDpCore *dp_core){
    dp_core->pause_traffic(m_port_id);
    return (true);
}



TrexStatelessCpToDpMsgBase * TrexStatelessDpResume::clone(){
    TrexStatelessDpResume *new_msg = new TrexStatelessDpResume(m_port_id);
    return new_msg;
}

bool TrexStatelessDpResume::handle(TrexStatelessDpCore *dp_core){
    dp_core->resume_traffic(m_port_id);
    return (true);
}


/**
 * clone for DP stop message
 *
 */
TrexStatelessCpToDpMsgBase *
TrexStatelessDpStop::clone() {
    TrexStatelessDpStop *new_msg = new TrexStatelessDpStop(m_port_id);

    new_msg->set_event_id(m_event_id);
    new_msg->set_wait_for_event_id(m_stop_only_for_event_id);
    /* set back pointer to master */
    new_msg->set_core_ptr(m_core);

    return new_msg;
}



TrexStatelessCpToDpMsgBase *
TrexStatelessDpQuit::clone(){

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpQuit();

    return new_msg;
}


bool TrexStatelessDpQuit::handle(TrexStatelessDpCore *dp_core){

    /* quit  */
    dp_core->quit_main_loop();
    return (true);
}

bool TrexStatelessDpCanQuit::handle(TrexStatelessDpCore *dp_core){

    if ( dp_core->are_all_ports_idle() ){
        /* if all ports are idle quit now */
        set_quit(true);
    }
    return (true);
}

TrexStatelessCpToDpMsgBase *
TrexStatelessDpCanQuit::clone(){

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpCanQuit();

    return new_msg;
}

/*************************
  update traffic message
 ************************/
bool
TrexStatelessDpUpdate::handle(TrexStatelessDpCore *dp_core) {
    dp_core->update_traffic(m_port_id, m_factor);

    return true;
}

TrexStatelessCpToDpMsgBase *
TrexStatelessDpUpdate::clone() {
    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpUpdate(m_port_id, m_factor);

    return new_msg;
}


/*************************
  push PCAP message
 ************************/
bool
TrexStatelessDpPushPCAP::handle(TrexStatelessDpCore *dp_core) {
    dp_core->push_pcap(m_port_id,
                       m_event_id,
                       m_pcap_filename,
                       m_ipg_usec,
                       m_min_ipg_sec,
                       m_speedup,
                       m_count,
                       m_duration,
                       m_is_dual);
    return true;
}

TrexStatelessCpToDpMsgBase *
TrexStatelessDpPushPCAP::clone() {
    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpPushPCAP(m_port_id,
                                                                      m_event_id,
                                                                      m_pcap_filename,
                                                                      m_ipg_usec,
                                                                      m_min_ipg_sec,
                                                                      m_speedup,
                                                                      m_count,
                                                                      m_duration,
                                                                      m_is_dual);

    return new_msg;
}


/*************************
  barrier message
 ************************/

bool
TrexStatelessDpBarrier::handle(TrexStatelessDpCore *dp_core) {
    dp_core->barrier(m_port_id, m_event_id);
    return true;
}

TrexStatelessCpToDpMsgBase *
TrexStatelessDpBarrier::clone() {

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpBarrier(m_port_id, m_event_id);

    return new_msg;
}

/*************************
  service mode message
 ************************/

bool
TrexStatelessDpServiceMode::handle(TrexStatelessDpCore *dp_core) {
    dp_core->set_service_mode(m_port_id, m_enabled);
    return true;
}

TrexStatelessCpToDpMsgBase *
TrexStatelessDpServiceMode::clone() {

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpServiceMode(m_port_id, m_enabled);

    return new_msg;
}

/************************* messages from DP to CP **********************/
bool
TrexDpPortEventMsg::handle() {
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(m_port_id);
    port->get_dp_events().on_core_reporting_in(m_event_id, m_thread_id, get_status());

    return (true);
}

/************************* messages from CP to RX **********************/
bool TrexStatelessRxEnableLatency::handle (CRxCoreStateless *rx_core) {
    rx_core->enable_latency();
    m_reply.set_reply(true);
    
    return true;
}

bool TrexStatelessRxDisableLatency::handle (CRxCoreStateless *rx_core) {
    rx_core->disable_latency();
    return true;
}

bool TrexStatelessRxQuit::handle (CRxCoreStateless *rx_core) {
    rx_core->quit();
    return true;
}


bool
TrexStatelessRxCaptureStart::handle(CRxCoreStateless *rx_core) {
    
    TrexCaptureRCStart start_rc;
    
    TrexStatelessCaptureMngr::getInstance().start(m_filter, m_limit, m_mode, start_rc);
    
    /* mark as done */
    m_reply.set_reply(start_rc);
    
    return true;
}

bool
TrexStatelessRxCaptureStop::handle(CRxCoreStateless *rx_core) {
    
    TrexCaptureRCStop stop_rc;
    
    TrexStatelessCaptureMngr::getInstance().stop(m_capture_id, stop_rc);
    
    /* mark as done */
    m_reply.set_reply(stop_rc);
    
    return true;
}

bool
TrexStatelessRxCaptureFetch::handle(CRxCoreStateless *rx_core) {
    
    TrexCaptureRCFetch fetch_rc;
    
    TrexStatelessCaptureMngr::getInstance().fetch(m_capture_id, m_pkt_limit, fetch_rc);
    
    /* mark as done */
    m_reply.set_reply(fetch_rc);
    
    return true;
}

bool
TrexStatelessRxCaptureStatus::handle(CRxCoreStateless *rx_core) {
    
    TrexCaptureRCStatus status_rc;
    
    status_rc.set_rc(TrexStatelessCaptureMngr::getInstance().to_json()); 
    
    /* mark as done */
    m_reply.set_reply(status_rc);
    
    return true;
}

bool
TrexStatelessRxCaptureRemove::handle(CRxCoreStateless *rx_core) {
    
    TrexCaptureRCRemove remove_rc;
    
    TrexStatelessCaptureMngr::getInstance().remove(m_capture_id, remove_rc);
    
    /* mark as done */
    m_reply.set_reply(remove_rc);
    
    return true;
}


bool
TrexStatelessRxStartQueue::handle(CRxCoreStateless *rx_core) {
    rx_core->start_queue(m_port_id, m_size);
    
    /* mark as done */
    m_reply.set_reply(true);
    
    return true;
}

bool
TrexStatelessRxStopQueue::handle(CRxCoreStateless *rx_core) {
    rx_core->stop_queue(m_port_id);

    return true;
}



bool
TrexStatelessRxQueueGetPkts::handle(CRxCoreStateless *rx_core) {
    const TrexPktBuffer *pkt_buffer = rx_core->get_rx_queue_pkts(m_port_id);
    
    /* set the reply */
    m_reply.set_reply(pkt_buffer);

    return true;
}


bool
TrexStatelessRxFeaturesToJson::handle(CRxCoreStateless *rx_core) {
    Json::Value output = rx_core->get_rx_port_mngr(m_port_id).to_json();
    
    /* set the reply */
    m_reply.set_reply(output);

    return true;
}

bool
TrexStatelessRxSetL2Mode::handle(CRxCoreStateless *rx_core) {
    rx_core->get_rx_port_mngr(m_port_id).set_l2_mode();
    
    return true;
}

bool
TrexStatelessRxSetL3Mode::handle(CRxCoreStateless *rx_core) {
    rx_core->get_rx_port_mngr(m_port_id).set_l3_mode(m_src_addr, m_is_grat_arp_needed);
    
    return true;
}

bool
TrexStatelessRxQuery::handle(CRxCoreStateless *rx_core) {

    query_rc_e rc = RC_OK;
    
    switch (m_query_type) {
   
    case SERVICE_MODE_ON:
        /* for service mode on - always allow this */
        rc = RC_OK;
        break;
        
    case SERVICE_MODE_OFF:
        /* cannot leave service mode when RX queue is active */
        if (rx_core->get_rx_port_mngr(m_port_id).is_feature_set(RXPortManager::QUEUE)) {
            rc = RC_FAIL_RX_QUEUE_ACTIVE;
            break;
        }
        
        /* cannot leave service mode if PCAP capturing is active */
        if (TrexStatelessCaptureMngr::getInstance().is_active(m_port_id)) {
            rc = RC_FAIL_CAPTURE_ACTIVE;
            break;
        }
        
        break;
    
    default:
        assert(0);
        break;
        
    }
    
    m_reply.set_reply(rc);
    
    return true;
}

bool
TrexStatelessRxSetVLAN::handle(CRxCoreStateless *rx_core) {
    RXPortManager &port_mngr = rx_core->get_rx_port_mngr(m_port_id);
    port_mngr.set_vlan_cfg(m_vlan_cfg);
    
    return true;
}
