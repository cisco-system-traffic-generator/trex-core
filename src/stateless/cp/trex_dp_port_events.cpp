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

#include <trex_dp_port_events.h>
#include <sstream>
#include <os_time.h>
#include <trex_stateless.h>

/**
 * port events
 */
void 
TrexDpPortEvents::create(TrexStatelessPort *port) {
    m_port = port;

    for (int i = 0; i < TrexDpPortEvent::EVENT_MAX; i++) {
        m_events[i].create((TrexDpPortEvent::event_e) i, port);
    }

    m_event_id_counter = EVENT_ID_INVALID;
}

/**
 * generate a new event ID
 * 
 */
int
TrexDpPortEvents::generate_event_id() {
    return (++m_event_id_counter);
}

/**
 * mark the next allowed event 
 * all other events will be disabled 
 * 
 */
void
TrexDpPortEvents::wait_for_event(TrexDpPortEvent::event_e ev, int event_id, int timeout_ms) {

    /* first disable all events */
    for (TrexDpPortEvent & e : m_events) {
        e.disable();
    }

    /* mark this event as allowed */
    m_events[ev].wait_for_event(event_id, timeout_ms);
}

/**
 * handle an event
 * 
 */
void 
TrexDpPortEvents::handle_event(TrexDpPortEvent::event_e ev, int thread_id, int event_id) {
    m_events[ev].handle_event(thread_id, event_id);
}

/*********** 
 * single event object
 * 
 */

void
TrexDpPortEvent::create(event_e type, TrexStatelessPort *port) {
    m_event_type = type;
    m_port = port;

    /* add the core ids to the hash */
    m_signal.clear();
    for (int core_id : m_port->get_core_id_list()) {
        m_signal[core_id] = false;
    }

    /* event is disabled */
    disable();
}


/**
 * wait the event using event id and timeout
 * 
 */
void
TrexDpPortEvent::wait_for_event(int event_id, int timeout_ms) {

    /* set a new event id */
    m_event_id = event_id;

    /* do we have a timeout ? */
    if (timeout_ms > 0) {
        m_expire_limit_ms = os_get_time_msec() + timeout_ms;
    } else {
        m_expire_limit_ms = -1;
    }

    /* prepare the signal array */
    m_pending_cnt = 0;
    for (auto & core_pair : m_signal) {
        core_pair.second = false;
        m_pending_cnt++;
    }
}

void
TrexDpPortEvent::disable() {
    m_event_id = TrexDpPortEvents::EVENT_ID_INVALID;
}

/**
 * get the event status
 * 
 */

TrexDpPortEvent::event_status_e
TrexDpPortEvent::status() {

    /* is it even active ? */
    if (m_event_id == TrexDpPortEvents::EVENT_ID_INVALID) {
        return (EVENT_DISABLE);
    }

    /* did it occured ? */
    if (m_pending_cnt == 0) {
        return (EVENT_OCCURED);
    }

    /* so we are enabled and the event did not occur - maybe we timed out ? */
    if ( (m_expire_limit_ms > 0) && (os_get_time_msec() > m_expire_limit_ms) ) {
        return (EVENT_TIMED_OUT);
    }

    /* so we are still waiting... */
    return (EVENT_PENDING);

}

void
TrexDpPortEvent::err(int thread_id, int event_id, const std::string &err_msg)  {
    std::stringstream err;
    err << "DP event '" << event_name(m_event_type)  << "' on thread id '" << thread_id << "' with key '" << event_id <<"' - ";
}

/**
 * event occured
 * 
 */
void 
TrexDpPortEvent::handle_event(int thread_id, int event_id) {

    /* if the event is disabled - we don't care */
    if (!is_active()) {
        return;
    }

    /* check the event id is matching the required event */
    if (event_id != m_event_id) {
        err(thread_id, event_id, "event key mismatch");
    }

    /* mark sure no double signal */
    if (m_signal.at(thread_id)) {
        err(thread_id, event_id, "double signal");

    } else {
        /* mark */
        m_signal.at(thread_id) = true;
        m_pending_cnt--;
    }

    /* event occured */
    if (m_pending_cnt == 0) {
        m_port->on_dp_event_occured(m_event_type);
        m_event_id = TrexDpPortEvents::EVENT_ID_INVALID;
    }
}

bool
TrexDpPortEvent::is_active() {
    return (status() != EVENT_DISABLE);
}

bool 
TrexDpPortEvent::has_timeout_expired() {
    return (status() == EVENT_TIMED_OUT);
}

const char *
TrexDpPortEvent::event_name(event_e type) {
    switch (type) {
    case EVENT_STOP:
        return "DP STOP";

    default:
        throw TrexException("unknown event type");
    }

}
