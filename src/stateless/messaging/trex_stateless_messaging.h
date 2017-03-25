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
#include "trex_exception.h"
#include "trex_stateless_rx_defs.h"
#include "os_time.h"
#include "utl_ip.h"
#include "trex_stateless_capture.h"

class TrexStatelessDpCore;
class CRxCoreStateless;
class TrexStreamsCompiledObj;
class CFlowGenListPerThread;
class TrexPktBuffer;

/**
 * Generic message reply object
 * 
 * @author imarom (11/27/2016)
 */
template<typename T> class MsgReply {

public:
    
    MsgReply() {
        reset();
    }

    void reset() {
        m_pending = true;
    }
    
    bool is_pending() const {
        return m_pending;
    }

    void set_reply(const T &reply) {
        m_reply = reply;

        /* before marking as done make sure all stores are committed */
        asm volatile("mfence" ::: "memory");
        m_pending = false;
    }

    T wait_for_reply(int timeout_ms = 500, int backoff_ms = 1) {
        int guard = timeout_ms;

        while (is_pending()) {
            guard -= backoff_ms;
            if (guard < 0) {
                throw TrexException("timeout: failed to get reply from core");
            }

            delay(backoff_ms);
        }
        
        return m_reply;

    }
    
protected:
    volatile bool  m_pending;
    T              m_reply;
};


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

    TrexStatelessDpStart(uint8_t port_id, int event_id, TrexStreamsCompiledObj *obj, double duration);

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

    TrexStatelessDpPushPCAP(uint8_t port_id,
                            int event_id,
                            const std::string &pcap_filename,
                            double ipg_usec,
                            double min_ipg_sec,
                            double speedup,
                            uint32_t count,
                            double duration,
                            bool is_dual) : m_pcap_filename(pcap_filename)  {

        m_port_id  = port_id;
        m_event_id = event_id;
        m_ipg_usec = ipg_usec;
        m_min_ipg_sec  = min_ipg_sec;
        m_speedup  = speedup;
        m_count    = count;
        m_duration = duration;
        m_is_dual  = is_dual;
    }

    virtual bool handle(TrexStatelessDpCore *dp_core);

    virtual TrexStatelessCpToDpMsgBase * clone();

private:
    std::string  m_pcap_filename;
    int          m_event_id;
    double       m_ipg_usec;
    double       m_min_ipg_sec;
    double       m_speedup;
    double       m_duration;
    uint32_t     m_count;
    uint8_t      m_port_id;
    bool         m_is_dual;
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


/**
 * move a DP core in/out of service mode (slower as it might do
 * capturing and etc.) 
 *
 */
class TrexStatelessDpServiceMode : public TrexStatelessCpToDpMsgBase {
public:

    TrexStatelessDpServiceMode(uint8_t port_id, bool enabled) {
        m_port_id = port_id;
        m_enabled = enabled;
    }

    virtual TrexStatelessCpToDpMsgBase * clone();

    virtual bool handle(TrexStatelessDpCore *dp_core);

private:

    uint8_t   m_port_id;
    bool      m_enabled;
};

/************************* messages from DP to CP **********************/

/**
 * defines the base class for DP to CP messages
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


class TrexStatelessRxEnableLatency : public TrexStatelessCpToRxMsgBase {
public:
    TrexStatelessRxEnableLatency(MsgReply<bool> &reply) : m_reply(reply) {
    }
    
    bool handle (CRxCoreStateless *rx_core);
    
private:
    MsgReply<bool>    &m_reply;
};

class TrexStatelessRxDisableLatency : public TrexStatelessCpToRxMsgBase {
    bool handle (CRxCoreStateless *rx_core);
};

class TrexStatelessRxQuit : public TrexStatelessCpToRxMsgBase {
    bool handle (CRxCoreStateless *rx_core);
};


class TrexStatelessRxCapture : public TrexStatelessCpToRxMsgBase {
public:
    virtual bool handle (CRxCoreStateless *rx_core) = 0;
};

class TrexStatelessRxCaptureStart : public TrexStatelessRxCapture {
public:
    TrexStatelessRxCaptureStart(const CaptureFilter& filter,
                                uint64_t limit,
                                TrexPktBuffer::mode_e mode,
                                MsgReply<TrexCaptureRCStart> &reply) : m_reply(reply) {
        
        m_limit  = limit;
        m_filter = filter;
        m_mode   = mode;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t                          m_port_id;
    uint64_t                         m_limit;
    CaptureFilter                    m_filter;
    TrexPktBuffer::mode_e            m_mode;
    MsgReply<TrexCaptureRCStart>    &m_reply;
};


class TrexStatelessRxCaptureStop : public TrexStatelessRxCapture {
public:
    TrexStatelessRxCaptureStop(capture_id_t capture_id, MsgReply<TrexCaptureRCStop> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    capture_id_t                   m_capture_id;
    MsgReply<TrexCaptureRCStop>   &m_reply;
};


class TrexStatelessRxCaptureFetch : public TrexStatelessRxCapture {
public:
    TrexStatelessRxCaptureFetch(capture_id_t capture_id, uint32_t pkt_limit, MsgReply<TrexCaptureRCFetch> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
        m_pkt_limit  = pkt_limit;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    capture_id_t                   m_capture_id;
    uint32_t                       m_pkt_limit;
    MsgReply<TrexCaptureRCFetch>  &m_reply;
};


class TrexStatelessRxCaptureStatus : public TrexStatelessRxCapture {
public:
    TrexStatelessRxCaptureStatus(MsgReply<TrexCaptureRCStatus> &reply) : m_reply(reply) {
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    MsgReply<TrexCaptureRCStatus>   &m_reply;
};



class TrexStatelessRxCaptureRemove : public TrexStatelessRxCapture {
public:
    TrexStatelessRxCaptureRemove(capture_id_t capture_id, MsgReply<TrexCaptureRCRemove> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    capture_id_t                   m_capture_id;
    MsgReply<TrexCaptureRCRemove> &m_reply;
};


class TrexStatelessRxStartQueue : public TrexStatelessCpToRxMsgBase {
public:
    TrexStatelessRxStartQueue(uint8_t port_id,
                              uint64_t size,
                              MsgReply<bool> &reply) : m_reply(reply) {
        
        m_port_id = port_id;
        m_size    = size;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t           m_port_id;
    uint64_t          m_size;
    MsgReply<bool>   &m_reply;
};


class TrexStatelessRxStopQueue : public TrexStatelessCpToRxMsgBase {
public:
    TrexStatelessRxStopQueue(uint8_t port_id) {
        m_port_id = port_id;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t m_port_id;
};



class TrexStatelessRxQueueGetPkts : public TrexStatelessCpToRxMsgBase {
public:

    TrexStatelessRxQueueGetPkts(uint8_t port_id, MsgReply<const TrexPktBuffer *> &reply) : m_reply(reply) {
        m_port_id = port_id;
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t                              m_port_id;
    MsgReply<const TrexPktBuffer *>     &m_reply;
    
};

/**
 * updates the RX core that we are in L2 mode
 */
class TrexStatelessRxSetL2Mode : public TrexStatelessCpToRxMsgBase {
public:
    TrexStatelessRxSetL2Mode(uint8_t port_id) {
        m_port_id = port_id;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t           m_port_id;
};

/**
 * updates the RX core that we are in a L3 mode
 */
class TrexStatelessRxSetL3Mode : public TrexStatelessCpToRxMsgBase {
public:
    TrexStatelessRxSetL3Mode(uint8_t port_id,
                             const CManyIPInfo &src_addr,
                             bool is_grat_arp_needed) {
        
        m_port_id              = port_id;
        m_src_addr             = src_addr;
        m_is_grat_arp_needed   = is_grat_arp_needed;
    }

    virtual bool handle(CRxCoreStateless *rx_core);

private:
    uint8_t           m_port_id;
    CManyIPInfo       m_src_addr;
    bool              m_is_grat_arp_needed;
};

/**
 * a request from RX core to dump to Json the RX features
 */
class TrexStatelessRxFeaturesToJson : public TrexStatelessCpToRxMsgBase {
public:
    
    TrexStatelessRxFeaturesToJson(uint8_t port_id, MsgReply<Json::Value> &reply) : m_reply(reply) {
        m_port_id = port_id;
    }
     
    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCoreStateless *rx_core);
    
private:
    uint8_t                  m_port_id;
    MsgReply<Json::Value>   &m_reply;
};


class TrexStatelessRxQuery : public TrexStatelessCpToRxMsgBase {
public:

    /**
     * query type to request
     */
    enum query_type_e {
        SERVICE_MODE_ON,
        SERVICE_MODE_OFF,
    };
    
    /**
     * RC types for queries
     */
    enum query_rc_e {
        RC_OK,
        RC_FAIL_RX_QUEUE_ACTIVE,
        RC_FAIL_CAPTURE_ACTIVE,
    };
    
    TrexStatelessRxQuery(uint8_t port_id, query_type_e query_type, MsgReply<query_rc_e> &reply) : m_reply(reply) {
        m_port_id    = port_id;
        m_query_type = query_type;
    }
     
    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCoreStateless *rx_core);
    
private:
    uint8_t                m_port_id;
    query_type_e           m_query_type;
    MsgReply<query_rc_e>  &m_reply;
};

#endif /* __TREX_STATELESS_MESSAGING_H__ */
