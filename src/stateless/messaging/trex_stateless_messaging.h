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
#ifndef __TREX_STATELESS_MESSAGING_H__
#define __TREX_STATELESS_MESSAGING_H__

#include <msg_manager.h>
#include <trex_dp_port_events.h>

class TrexStatelessDpCore;
class TrexStreamsCompiledObj;

/**
 * defines the base class for CP to DP messages
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessCpToDpMsgBase() {
    }

    virtual ~TrexStatelessCpToDpMsgBase() {
    }


    virtual bool handle(TrexStatelessDpCore *dp_core) = 0;

    /**
     * clone the current message
     * 
     */
    virtual TrexStatelessCpToDpMsgBase * clone() = 0;

    /* no copy constructor */
    TrexStatelessCpToDpMsgBase(TrexStatelessCpToDpMsgBase &) = delete;

protected:
    int m_event_id;
};

/**
 * a message to start traffic
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStart : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStart(int m_event_id, TrexStreamsCompiledObj *obj, double duration);

    ~TrexStatelessDpStart();

    virtual TrexStatelessCpToDpMsgBase * clone();

    virtual bool handle(TrexStatelessDpCore *dp_core);

private:

    int m_event_id;
    TrexStreamsCompiledObj *m_obj;
    double m_duration;
};

/**
 * a message to stop traffic
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStop : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStop(uint8_t port_id) : m_port_id(port_id) {
    }

    virtual TrexStatelessCpToDpMsgBase * clone();

    virtual bool handle(TrexStatelessDpCore *dp_core);

private:
    uint8_t m_port_id;
};

/**
 * a message to Quit the datapath traffic. support only stateless for now 
 * 
 * @author hhaim 
 */
class TrexStatelessDpQuit : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpQuit()  {
    }


    virtual TrexStatelessCpToDpMsgBase * clone();

    virtual bool handle(TrexStatelessDpCore *dp_core);

};

/************************* messages from DP to CP **********************/

/**
 * defines the base class for CP to DP messages
 * 
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpToCpMsgBase {
public:

    TrexStatelessDpToCpMsgBase() {
    }

    virtual ~TrexStatelessDpToCpMsgBase() {
    }

    /**
     * virtual function to handle a message
     * 
     */
    virtual bool handle() = 0;

    /* no copy constructor */
    TrexStatelessDpToCpMsgBase(TrexStatelessDpToCpMsgBase &) = delete;

};

/**
 * a message indicating an event has happened on a port at the 
 * DP 
 * 
 */
class TrexDpPortEventMsg : public TrexStatelessDpToCpMsgBase {
public:

    TrexDpPortEventMsg(int thread_id, uint8_t port_id, TrexDpPortEvent::event_e type, int event_id) {
        m_thread_id  = thread_id;
        m_port_id    = port_id;
        m_event_type = type;
        m_event_id   = event_id;
    }

    virtual bool handle();

private:
    int                         m_thread_id;
    uint8_t                     m_port_id;
    TrexDpPortEvent::event_e    m_event_type;
    int                         m_event_id;
    
};

#endif /* __TREX_STATELESS_MESSAGING_H__ */

