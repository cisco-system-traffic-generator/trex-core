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

#include "trex_stl_messaging.h"
#include "trex_rx_core.h"

#include "trex_stl_dp_core.h"
#include "trex_stl_streams_compiler.h"
#include "trex_stl.h"
#include "trex_stl_port.h"

#include "bp_sim.h"

/*************************
  start traffic message
 ************************/
TrexStatelessDpStart::TrexStatelessDpStart(uint8_t port_id, uint32_t profile_id, int event_id, TrexStreamsCompiledObj *obj, double duration, double start_at_ts) {
    m_port_id = port_id;
    m_profile_id = profile_id;
    m_event_id = event_id;
    m_obj = obj;
    m_duration = duration;
    m_start_at_ts = start_at_ts;
}


/**
 * clone for DP start message
 *
 */
TrexCpToDpMsgBase *
TrexStatelessDpStart::clone() {

    TrexStreamsCompiledObj *new_obj = m_obj->clone();

    TrexCpToDpMsgBase *new_msg = new TrexStatelessDpStart(m_port_id, m_profile_id, m_event_id, new_obj, m_duration, m_start_at_ts);

    return new_msg;
}

TrexStatelessDpStart::~TrexStatelessDpStart() {
    if (m_obj) {
        delete m_obj;
    }
}

bool
TrexStatelessDpStart::handle(TrexDpCore *dp_core) {
    
    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    /* start traffic */
    stl_core->start_traffic(m_obj, m_profile_id, m_duration, m_event_id, m_start_at_ts);

    return true;
}

/*************************
  stop traffic message
 ************************/
bool
TrexStatelessDpStop::handle(TrexDpCore *dp_core) {

    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    stl_core->stop_traffic(m_port_id, m_profile_id, m_stop_only_for_event_id, m_event_id);
    return true;
}


void TrexStatelessDpStop::on_node_remove(){
    if ( m_core ) {
        assert(m_core->m_non_active_nodes>0);
        m_core->m_non_active_nodes--;
    }
}


TrexCpToDpMsgBase * TrexStatelessDpPause::clone(){

    TrexStatelessDpPause *new_msg = new TrexStatelessDpPause(m_port_id, m_profile_id);
    return new_msg;
}


bool TrexStatelessDpPause::handle(TrexDpCore *dp_core){
    
    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    stl_core->pause_traffic(m_port_id, m_profile_id);
    return (true);
}


TrexCpToDpMsgBase * TrexStatelessDpPauseStreams::clone(){

    TrexStatelessDpPauseStreams *new_msg = new TrexStatelessDpPauseStreams(m_port_id, m_stream_ids);
    return new_msg;
}

bool TrexStatelessDpPauseStreams::handle(TrexDpCore *dp_core){

    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);

    stl_core->pause_streams(m_port_id, m_stream_ids);
    return (true);
}


TrexCpToDpMsgBase * TrexStatelessDpResume::clone(){
    TrexStatelessDpResume *new_msg = new TrexStatelessDpResume(m_port_id, m_profile_id);
    return new_msg;
}

bool TrexStatelessDpResume::handle(TrexDpCore *dp_core){

    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    stl_core->resume_traffic(m_port_id, m_profile_id);
    return (true);
}


TrexCpToDpMsgBase * TrexStatelessDpResumeStreams::clone(){
    TrexStatelessDpResumeStreams *new_msg = new TrexStatelessDpResumeStreams(m_port_id, m_stream_ids);
    return new_msg;
}

bool TrexStatelessDpResumeStreams::handle(TrexDpCore *dp_core){

    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);

    stl_core->resume_streams(m_port_id, m_stream_ids);
    return (true);
}


/**
 * clone for DP stop message
 *
 */
TrexCpToDpMsgBase *
TrexStatelessDpStop::clone() {
    TrexStatelessDpStop *new_msg = new TrexStatelessDpStop(m_port_id, m_profile_id);

    new_msg->set_event_id(m_event_id);
    new_msg->set_wait_for_event_id(m_stop_only_for_event_id);
    /* set back pointer to master */
    new_msg->set_core_ptr(m_core);

    return new_msg;
}


/*************************
  update traffic message
 ************************/
bool
TrexStatelessDpUpdate::handle(TrexDpCore *dp_core) {
    
    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    stl_core->update_traffic(m_port_id, m_profile_id, m_factor);

    return true;
}

TrexCpToDpMsgBase *
TrexStatelessDpUpdate::clone() {
    TrexCpToDpMsgBase *new_msg = new TrexStatelessDpUpdate(m_port_id, m_profile_id, m_factor);

    return new_msg;
}


bool
TrexStatelessDpUpdateStreams::handle(TrexDpCore *dp_core) {

    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);

    stl_core->update_streams(m_port_id, m_ipg_per_stream);

    return true;
}

TrexCpToDpMsgBase *
TrexStatelessDpUpdateStreams::clone() {
    TrexCpToDpMsgBase *new_msg = new TrexStatelessDpUpdateStreams(m_port_id, m_ipg_per_stream);

    return new_msg;
}

/*************************
  push PCAP message
 ************************/
bool
TrexStatelessDpPushPCAP::handle(TrexDpCore *dp_core) {
    
    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);
    
    stl_core->push_pcap(m_port_id,
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

TrexCpToDpMsgBase *
TrexStatelessDpPushPCAP::clone() {
    TrexCpToDpMsgBase *new_msg = new TrexStatelessDpPushPCAP(m_port_id,
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
  service mode message
 ************************/

bool
TrexStatelessDpServiceMode::handle(TrexDpCore *dp_core) {
    
    TrexStatelessDpCore *stl_core = dynamic_cast<TrexStatelessDpCore *>(dp_core);

    stl_core->set_service_mode(m_port_id, m_enabled);
    return true;
}

TrexCpToDpMsgBase *
TrexStatelessDpServiceMode::clone() {

    TrexCpToDpMsgBase *new_msg = new TrexStatelessDpServiceMode(m_port_id, m_enabled);

    return new_msg;
}



bool
TrexStatelessRxQuery::handle(CRxCore *rx_core) {

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

        /* cannot leave service mode if CAPWAP proxy is active */
        if (rx_core->get_rx_port_mngr(m_port_id).is_feature_set(RXPortManager::CAPWAP_PROXY)) {
            rc = RC_FAIL_CAPWAP_PROXY_ACTIVE;
            break;
        }
        
        /* cannot leave service mode if PCAP capturing is active */
        if (TrexCaptureMngr::getInstance().is_active(m_port_id)) {
            rc = RC_FAIL_CAPTURE_ACTIVE;
            break;
        }

        /* cannot leave service mode if capture port is active */
        if (rx_core->get_rx_port_mngr(m_port_id).is_feature_set(RXPortManager::CAPTURE_PORT)) {
            rc = RC_FAIL_CAPTURE_PORT_ACTIVE;
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


bool TrexStatelessRxEnableLatency::handle (CRxCore *rx_core) {
    rx_core->enable_latency();
    m_reply.set_reply(true);
    
    return true;
}

bool TrexStatelessRxDisableLatency::handle (CRxCore *rx_core) {
    rx_core->disable_latency();
    return true;
}

