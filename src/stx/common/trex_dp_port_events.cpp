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

#include "trex_dp_port_events.h"
#include "trex_exception.h"
#include "trex_messaging.h"
#include "trex_port.h"
#include "trex_stx.h"

#include <sstream>
#include <os_time.h>


/**
 * port events
 */
TrexDpPortEvents::TrexDpPortEvents(TrexPort *port) {
    m_port = port;
    m_event_id_counter = EVENT_ID_INVALID;
}

TrexDpPortEvent *
TrexDpPortEvents::lookup(int event_id) {
    auto search = m_events.find(event_id);

    if (search != m_events.end()) {
        return search->second;
    } else {
        return NULL;
    }
}

/**
 * mark the next allowed event 
 * all other events will be disabled 
 * 
 */
int
TrexDpPortEvents::create_event(TrexDpPortEvent *event, int timeout_ms) {
    /* allocate ID for event */
    int event_id = ++m_event_id_counter;

    /* init and add */
    event->init(m_port, event_id, timeout_ms);
    m_events[event_id] = event;

    return event_id;
}

void 
TrexDpPortEvents::destroy_event(int event_id) {
    TrexDpPortEvent *event = lookup(event_id);
    if (!event) {
        /* cannot find event */
        throw TrexException("internal error - cannot find event");
    }

    m_events.erase(event_id);
    delete event;
}

class DPBarrier : public TrexDpPortEvent {
protected:
    virtual void on_event() {
        /* do nothing */
    }
    virtual void on_error(int thread_id) {
        /* do nothing */
    }
};


void
TrexDpPortEvents::barrier(uint32_t profile_id) {

    /* simulator will be stuck here forever */
    #ifdef TREX_SIM
    return;
    #endif
     
    int barrier_id = create_event(new DPBarrier());

    TrexCpToDpMsgBase *barrier_msg = new TrexDpBarrier(m_port->m_port_id, profile_id, barrier_id);
    m_port->send_message_to_all_dp(barrier_msg);

    get_stx()->check_for_dp_messages();
    while (lookup(barrier_id) != NULL) {
        delay(1);
        get_stx()->check_for_dp_messages();
    }
}
/**
 * handle an event
 * 
 */
void 
TrexDpPortEvents::on_core_reporting_in(int event_id, int thread_id, bool status) {
    TrexDpPortEvent *event = lookup(event_id);
    /* event might have been deleted */
    if (!event) {
        return;
    }

    bool done = event->on_core_reporting_in(thread_id, status);

    if (done) {
        destroy_event(event_id);
    }
}


/**
 * return true if a core is still pending on an event 
 */
bool
TrexDpPortEvents::is_core_pending_on_event(int event_id, int thread_id) {
    TrexDpPortEvent *event = lookup(event_id);
    /* event might have been deleted */
    if (!event) {
        return false;
    }

    return event->is_core_pending_on_event(thread_id);
}

/***************************
 * event
 * 
 **************************/
TrexDpPortEvent::TrexDpPortEvent() {
    m_port = NULL;
    m_event_id = -1;
}

void 
TrexDpPortEvent::init(TrexPort *port, int event_id, int timeout_ms) {
    m_port = port;
    m_event_id = event_id;

    /* do we have a timeout ? */
    if (timeout_ms > 0) {
        m_expire_limit_ms = os_get_time_msec() + timeout_ms;
    } else {
        m_expire_limit_ms = -1;
    }

    /* prepare the signal array */
    m_pending_cnt = 0;
    for (int core_id : m_port->get_core_id_list()) {
        m_signal[core_id] = false;
        m_pending_cnt++;
    }
}

bool
TrexDpPortEvent::on_core_reporting_in(int thread_id, bool status) {
    /* mark sure no double signal */
    if (m_signal.at(thread_id)) {
        std::stringstream err;
        err << "double signal detected on event id: " << m_event_id;
        throw TrexException(err.str());

    }

    /* mark */
    m_signal.at(thread_id) = true;
    m_pending_cnt--;

    /* if any core reported an error - mark as a failure */
    if (!status) {
        on_error(thread_id);
    }

    /* event occured */
    if (m_pending_cnt == 0) {
        on_event();
        return true;
    } else {
        return false;
    }
}

bool
TrexDpPortEvent::is_core_pending_on_event(int thread_id) {
    /* if the core has yet to mark its 'done' bit - it is still pending */
    return !m_signal.at(thread_id);
}

