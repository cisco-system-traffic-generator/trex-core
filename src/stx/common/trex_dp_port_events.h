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

class TrexPort;
class TrexDpPortEvents;

/**
 * interface class for DP events
 * 
 * @author imarom (29-Feb-16)
 */
class TrexDpPortEvent {
    friend TrexDpPortEvents;

public:
    TrexDpPortEvent();
    virtual ~TrexDpPortEvent() {}

protected:
    /**
     * what to do when an event has been completed (all cores 
     * reported in 
     * 
     * @author imarom (29-Feb-16)
     */
    virtual void on_event() = 0;

    /**
     * when a thread ID encounter an error
     * 
     * @author imarom (20-Apr-16)
     * 
     * @param thread_id 
     */
    virtual void on_error(int thread_id) = 0;

    TrexPort *get_port() {
        return m_port;
    }

private:
    void init(TrexPort *port, int event_id, int timeout_ms);
    bool on_core_reporting_in(int thread_id, bool status = true);
    bool is_core_pending_on_event(int thread_id);

    std::unordered_map<int, bool>  m_signal;
    int                            m_pending_cnt;

    TrexPort                      *m_port;
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

    static const int INVALID_ID = -1;

    TrexDpPortEvents(TrexPort *port);

    /**
     * wait a new DP event on the port 
     * returns a key which will be used to identify 
     * the event happened 
     * 
     */
    int create_event(TrexDpPortEvent *event, int timeout_ms = -1);

    /**
     * destroy an event
     * 
     */
    void destroy_event(int event_id);

    /**
     * return when all DP cores have responsed on a barrier
     */
    void barrier();

    /**
     * a core has reached the event 
     */
    void on_core_reporting_in(int event_id, int thread_id, bool status = true);

    /**
     * return true if core has yet to respond 
     * to the event 
     * 
     */
    bool is_core_pending_on_event(int event_id, int thread_id);

private:
    TrexDpPortEvent *lookup(int event_id);

    static const int EVENT_ID_INVALID = -1;
    std::unordered_map<int, TrexDpPortEvent *> m_events;
    int m_event_id_counter;

    TrexPort *m_port;
    
};

#endif /* __TREX_DP_PORT_EVENTS_H__ */
