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
#ifndef __TREX_STATELESS_MESSAGING_H__
#define __TREX_STATELESS_MESSAGING_H__

#include "msg_manager.h"
#include "trex_dp_port_events.h"

class TrexStatelessDpCore;
class CRxCoreStateless;
class TrexStreamsCompiledObj;
class CFlowGenListPerThread;

/**
 * defines the base class for CP to DP messages
 *
 * @author imarom (27-Oct-15)
 */
class TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessCpToDpMsgBase() {
        m_quit_scheduler=false;
    }

    virtual ~TrexStatelessCpToDpMsgBase() {
    }


    virtual bool handle(TrexStatelessDpCore *dp_core) = 0;

    /**
     * clone the current message
     *
     */
    virtual TrexStatelessCpToDpMsgBase * clone() = 0;

    /* do we want to quit scheduler, can be set by handle function  */
    void set_quit(bool enable){
        m_quit_scheduler=enable;
    }

    bool is_quit(){
        return ( m_quit_scheduler);
    }

    /* this node is called from scheduler in case the node is free */
    virtual void on_node_remove(){
    }

    /* no copy constructor */
    TrexStatelessCpToDpMsgBase(TrexStatelessCpToDpMsgBase &) = delete;

protected:
    int     m_event_id;
    bool    m_quit_scheduler;
};

/**
 * a message to start traffic
 *
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStart : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStart(uint8_t m_port_id, int m_event_id, TrexStreamsCompiledObj *obj, double duration);

    ~TrexStatelessDpStart();

    virtual TrexStatelessCpToDpMsgBase * clone();

    virtual bool handle(TrexStatelessDpCore *dp_core);

private:

    uint8_t                 m_port_id;
    int                     m_event_id;
    TrexStreamsCompiledObj *m_obj;
    double                  m_duration;

};

class TrexStatelessDpPause : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpPause(uint8_t port_id) : m_port_id(port_id) {
    }


    virtual TrexStatelessCpToDpMsgBase * clone();


    virtual bool handle(TrexStatelessDpCore *dp_core);


private:
    uint8_t m_port_id;
};


class TrexStatelessDpResume : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpResume(uint8_t port_id) : m_port_id(port_id) {
    }


    virtual TrexStatelessCpToDpMsgBase * clone();


    virtual bool handle(TrexStatelessDpCore *dp_core);


private:
    uint8_t m_port_id;
};


/**
 * a message to stop traffic
 *
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStop : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpStop(uint8_t port_id) : m_port_id(port_id) {
        m_stop_only_for_event_id=false;
        m_event_id = 0;
        m_core = NULL;
    }

    virtual TrexStatelessCpToDpMsgBase * clone();


    virtual bool handle(TrexStatelessDpCore *dp_core);

    void set_core_ptr(CFlowGenListPerThread   *  core){
            m_core = core;
    }

    CFlowGenListPerThread   * get_core_ptr(){
        return ( m_core);
    }


    void set_event_id(int event_id){
        m_event_id                =  event_id;
    }

    void set_wait_for_event_id(bool wait){
        m_stop_only_for_event_id  =  wait;
    }

    virtual void on_node_remove();


    bool get_is_stop_by_event_id(){
        return (m_stop_only_for_event_id);
    }

    int get_event_id(){
        return (m_event_id);
    }

private:
    uint8_t m_port_id;
    bool    m_stop_only_for_event_id;
    int     m_event_id;
    CFlowGenListPerThread   *  m_core ;

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

/**
 * a message to check if both port are idel and exit
 *
 * @author hhaim
 */
class TrexStatelessDpCanQuit : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpCanQuit()  {
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();
};


/**
 * update message
 */
class TrexStatelessDpUpdate : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpUpdate(uint8_t port_id, double factor)  {
        m_port_id = port_id;
        m_factor = factor;
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    uint8_t  m_port_id;
    double   m_factor;
};


/**
 * psuh a PCAP message
 */
class TrexStatelessDpPushPCAP : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpPushPCAP(uint8_t port_id, int event_id, const std::string &pcap_filename) : m_pcap_filename(pcap_filename)  {
        m_port_id  = port_id;
        m_event_id = event_id;
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    std::string  m_pcap_filename;
    uint8_t      m_port_id;
    int          m_event_id;
};


/**
 * barrier message for DP core
 *
 */
class TrexStatelessDpBarrier : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpBarrier(uint8_t port_id, int event_id) {
        m_port_id  = port_id;
        m_event_id = event_id;
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    uint8_t m_port_id;
    int     m_event_id;
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

    TrexDpPortEventMsg(int thread_id, uint8_t port_id, int event_id, bool status = true) {
        m_thread_id     = thread_id;
        m_port_id       = port_id;
        m_event_id      = event_id;
        m_status        = status;
    }

    virtual bool handle();

    int get_thread_id() {
        return m_thread_id;
    }

    uint8_t get_port_id() {
        return m_port_id;
    }

    int get_event_id() {
        return m_event_id;
    }

    bool get_status() {
        return m_status;
    }

private:
    int                         m_thread_id;
    uint8_t                     m_port_id;
    int                         m_event_id;
    bool                        m_status;

};

/************************* messages from CP to RX **********************/

/**
 * defines the base class for CP to RX messages
 *
 */
class TrexStatelessCpToRxMsgBase {
public:

    TrexStatelessCpToRxMsgBase() {
    }

    virtual ~TrexStatelessCpToRxMsgBase() {
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle (CRxCoreStateless *rx_core) = 0;

    /* no copy constructor */
    TrexStatelessCpToRxMsgBase(TrexStatelessCpToRxMsgBase &) = delete;

};

class TrexStatelessRxStartMsg : public TrexStatelessCpToRxMsgBase {
    bool handle (CRxCoreStateless *rx_core);
};

class TrexStatelessRxStopMsg : public TrexStatelessCpToRxMsgBase {
    bool handle (CRxCoreStateless *rx_core);
};

class TrexStatelessRxQuit : public TrexStatelessCpToRxMsgBase {
    bool handle (CRxCoreStateless *rx_core);
};

#endif /* __TREX_STATELESS_MESSAGING_H__ */
