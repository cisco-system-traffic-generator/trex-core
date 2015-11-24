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
#ifndef __TREX_DP_PORT_EVENTS_H__
#define __TREX_DP_PORT_EVENTS_H__

#include <unordered_map>
#include <string>

class TrexStatelessPort;

/**
 * describes a single DP event related to port
 * 
 * @author imarom (18-Nov-15)
 */
class TrexDpPortEvent {
public:

    enum event_e {
        EVENT_STOP   = 1,
        EVENT_MAX
    };

    /**
     * status of the event for the port
     */
    enum event_status_e {
        EVENT_DISABLE,
        EVENT_PENDING,
        EVENT_TIMED_OUT,
        EVENT_OCCURED
    };

    /**
     * init for the event
     * 
     */
    void create(event_e type, TrexStatelessPort *port);

    /**
     * create a new pending event
     * 
     */
    void wait_for_event(int event_id, int timeout_ms = -1);

    /**
     * mark event as not allowed to happen
     * 
     */
    void disable();

    /**
     * get the event status
     * 
     */
    event_status_e status();

    /**
     * event occured
     * 
     */
    void handle_event(int thread_id, int event_id);

    /**
     * returns true if event is active
     * 
     */
    bool is_active();

    /**
     * has timeout already expired ?
     * 
     */
    bool has_timeout_expired();

    /**
     * generate error
     * 
     */
    void err(int thread_id, int event_id, const std::string &err_msg);

    /**
     * event to name
     * 
     */
    static const char * event_name(event_e type);


private:

    event_e                        m_event_type;
    std::unordered_map<int, bool>  m_signal;
    int                            m_pending_cnt;

    TrexStatelessPort             *m_port;
    int                            m_event_id;
    int                            m_expire_limit_ms;
    
};

/**
 * all the events related to a port
 * 
 */
class TrexDpPortEvents {
public:
    friend class TrexDpPortEvent;

    void create(TrexStatelessPort *port);

    /**
     * generate a new event ID to be used with wait_for_event
     * 
     */
    int generate_event_id();

    /**
     * wait a new DP event on the port 
     * returns a key which will be used to identify 
     * the event happened 
     * 
     * @author imarom (18-Nov-15)
     * 
     * @param ev - type of event
     * @param event_id - a unique identifier for the event
     * @param timeout_ms - does it has a timeout ?
     * 
     */
    void wait_for_event(TrexDpPortEvent::event_e ev, int event_id, int timeout_ms = -1);

    /**
     * disable an event (don't care)
     * 
     */
    void disable(TrexDpPortEvent::event_e ev);

    /**
     * event has occured
     * 
     */
    void handle_event(TrexDpPortEvent::event_e ev, int thread_id, int event_id);

private:
    static const int EVENT_ID_INVALID = -1;

    TrexDpPortEvent m_events[TrexDpPortEvent::EVENT_MAX];
    int m_event_id_counter;

    TrexStatelessPort *m_port;
    
};

#endif /* __TREX_DP_PORT_EVENTS_H__ */
