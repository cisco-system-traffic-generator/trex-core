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

#include "trex_messaging.h"
#include "trex_stx.h"
#include "trex_dp_core.h"
#include "trex_port.h"
#include "trex_rx_core.h"

/*************************
  DP quit
 ************************/

bool TrexDpQuit::handle(TrexDpCore *dp_core){

    /* quit  */
    dp_core->stop();
    return (true);
}


TrexCpToDpMsgBase *
TrexDpQuit::clone(){

    TrexCpToDpMsgBase *new_msg = new TrexDpQuit();

    return new_msg;
}



/*************************
  DP can quit
 ************************/

bool TrexDpCanQuit::handle(TrexDpCore *dp_core){

    if ( dp_core->are_all_ports_idle() ){
        /* if all ports are idle quit now */
        set_quit(true);
    }
    return (true);
}


TrexCpToDpMsgBase *
TrexDpCanQuit::clone(){

    TrexCpToDpMsgBase *new_msg = new TrexDpCanQuit();

    return new_msg;
}

/*************************
  barrier message
 ************************/

bool
TrexDpBarrier::handle(TrexDpCore *dp_core) {
    dp_core->barrier(m_port_id, m_event_id);
    return true;
}

TrexCpToDpMsgBase *
TrexDpBarrier::clone() {

    TrexCpToDpMsgBase *new_msg = new TrexDpBarrier(m_port_id, m_event_id);

    return new_msg;
}


/************************* messages from DP to CP **********************/

bool
TrexDpPortEventMsg::handle() {
    TrexPort *port = get_stx()->get_port_by_id(m_port_id);
    port->get_dp_events().on_core_reporting_in(m_event_id, m_thread_id, get_status());

    return (true);
}


/************************* messages from CP to RX **********************/

#if 0
bool TrexRxEnableLatency::handle (CRxCore *rx_core) {
    rx_core->enable_latency();
    m_reply.set_reply(true);
    
    return true;
}


bool TrexRxDisableLatency::handle (CRxCore *rx_core) {
    rx_core->disable_latency();
    return true;
}

#endif


bool TrexRxQuit::handle (CRxCore *rx_core) {
    rx_core->quit();
    return true;
}


bool
TrexRxCaptureStart::handle(CRxCore *rx_core) {
    
    TrexCaptureRCStart start_rc;
    
    TrexCaptureMngr::getInstance().start(m_filter, m_limit, m_mode, start_rc);
    
    /* mark as done */
    m_reply.set_reply(start_rc);
    
    return true;
}

bool
TrexRxCaptureStop::handle(CRxCore *rx_core) {
    
    TrexCaptureRCStop stop_rc;
    
    TrexCaptureMngr::getInstance().stop(m_capture_id, stop_rc);
    
    /* mark as done */
    m_reply.set_reply(stop_rc);
    
    return true;
}

bool
TrexRxCaptureFetch::handle(CRxCore *rx_core) {
    
    TrexCaptureRCFetch fetch_rc;
    
    TrexCaptureMngr::getInstance().fetch(m_capture_id, m_pkt_limit, fetch_rc);
    
    /* mark as done */
    m_reply.set_reply(fetch_rc);
    
    return true;
}

bool
TrexRxCaptureStatus::handle(CRxCore *rx_core) {
    
    TrexCaptureRCStatus status_rc;
    
    status_rc.set_rc(TrexCaptureMngr::getInstance().to_json()); 
    
    /* mark as done */
    m_reply.set_reply(status_rc);
    
    return true;
}

bool
TrexRxCaptureRemove::handle(CRxCore *rx_core) {
    
    TrexCaptureRCRemove remove_rc;
    
    TrexCaptureMngr::getInstance().remove(m_capture_id, remove_rc);
    
    /* mark as done */
    m_reply.set_reply(remove_rc);
    
    return true;
}


bool
TrexRxStartQueue::handle(CRxCore *rx_core) {
    rx_core->start_queue(m_port_id, m_size);
    
    /* mark as done */
    m_reply.set_reply(true);
    
    return true;
}

bool
TrexRxStopQueue::handle(CRxCore *rx_core) {
    rx_core->stop_queue(m_port_id);

    return true;
}



bool
TrexRxQueueGetPkts::handle(CRxCore *rx_core) {
    const TrexPktBuffer *pkt_buffer = rx_core->get_rx_queue_pkts(m_port_id);
    
    /* set the reply */
    m_reply.set_reply(pkt_buffer);

    return true;
}


bool
TrexRxFeaturesToJson::handle(CRxCore *rx_core) {
    Json::Value output = rx_core->get_rx_port_mngr(m_port_id).to_json();
    
    /* set the reply */
    m_reply.set_reply(output);

    return true;
}

bool
TrexRxSetL2Mode::handle(CRxCore *rx_core) {
    rx_core->get_rx_port_mngr(m_port_id).set_l2_mode();
    
    return true;
}

bool
TrexRxSetL3Mode::handle(CRxCore *rx_core) {
    rx_core->get_rx_port_mngr(m_port_id).set_l3_mode(m_src_addr, m_is_grat_arp_needed);
    
    return true;
}


bool
TrexRxSetVLAN::handle(CRxCore *rx_core) {
    RXPortManager &port_mngr = rx_core->get_rx_port_mngr(m_port_id);
    port_mngr.set_vlan_cfg(m_vlan_cfg);
    
    return true;
}


bool
TrexRxTXPkts::handle(CRxCore *rx_core) {
    uint32_t sent = rx_core->tx_pkts(m_port_id, m_pkts, m_ipg_usec);
    
    m_reply.set_reply(sent);
    
    return true;
}

