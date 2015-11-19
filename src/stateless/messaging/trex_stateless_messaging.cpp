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
#include <trex_stateless_messaging.h>
#include <trex_stateless_dp_core.h>
#include <trex_streams_compiler.h>
#include <trex_stateless.h>

#include <string.h>

/*************************
  start traffic message
 ************************/ 
TrexStatelessDpStart::TrexStatelessDpStart(int event_id, TrexStreamsCompiledObj *obj, double duration) {
    m_event_id = event_id;
    m_obj = obj;
    m_duration = duration;
}


/**
 * clone for DP start message
 * 
 */
TrexStatelessCpToDpMsgBase *
TrexStatelessDpStart::clone() {

    TrexStreamsCompiledObj *new_obj = m_obj->clone();

    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpStart(m_event_id, new_obj, m_duration);

    return new_msg;
}

TrexStatelessDpStart::~TrexStatelessDpStart() {
    if (m_obj) {
        delete m_obj;
    }
}

bool
TrexStatelessDpStart::handle(TrexStatelessDpCore *dp_core) {

    /* mark the event id for DP response */
    dp_core->set_event_id(m_event_id);

    /* staet traffic */
    dp_core->start_traffic(m_obj, m_duration);

    return true;
}

/*************************
  stop traffic message
 ************************/ 
bool
TrexStatelessDpStop::handle(TrexStatelessDpCore *dp_core) {
    dp_core->stop_traffic(m_port_id);
    return true;
}

/**
 * clone for DP stop message
 * 
 */
TrexStatelessCpToDpMsgBase *
TrexStatelessDpStop::clone() {
    TrexStatelessCpToDpMsgBase *new_msg = new TrexStatelessDpStop(m_port_id);

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


/************************* messages from DP to CP **********************/
bool
TrexDpPortEventMsg::handle() {
    TrexStatelessPort *port = get_stateless_obj()->get_port_by_id(m_port_id);
    port->get_dp_events().handle_event(m_event_type, m_thread_id, m_event_id);

    return (true);
}

