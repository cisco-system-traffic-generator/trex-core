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
#ifndef __TREX_MESSAGING_H__
#define __TREX_MESSAGING_H__

class TrexDpCore;
class CRxCore;

#include "os_time.h"
#include "trex_capture.h"
#include "utl_ip.h"
#include "trex_exception.h"
#include <rte_atomic.h>
#include "trex_stack_base.h"

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
        rte_mb();
        m_pending = false;
    }

    T wait_for_reply(int timeout_ms = 500, int backoff_ms = 1) {
        int guard = timeout_ms;

        while (is_pending()) {
            guard -= backoff_ms;
            if (guard < 0) {
                throw TrexException("internal error: failed to get response from remote core for more than '" + std::to_string(timeout_ms) + "' ms");
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
class TrexCpToDpMsgBase {
public:

    TrexCpToDpMsgBase() {
        m_quit_scheduler = false;
        m_event_id = 0;
    }

    virtual ~TrexCpToDpMsgBase() {
    }


    virtual bool handle(TrexDpCore *dp_core) = 0;

    /**
     * clone the current message
     *
     */
    virtual TrexCpToDpMsgBase * clone() = 0;

    /* do we want to quit scheduler, can be set by handle function  */
    void set_quit(bool enable){
        m_quit_scheduler = enable;
    }

    bool is_quit(){
        return ( m_quit_scheduler);
    }

    /* this node is called from scheduler in case the node is free */
    virtual void on_node_remove(){
    }

    /* no copy constructor */
    TrexCpToDpMsgBase(TrexCpToDpMsgBase &) = delete;

protected:
    int     m_event_id;
    bool    m_quit_scheduler;
};



/**
 * a message to Quit the datapath traffic
 *
 * @author hhaim
 */
class TrexDpQuit : public TrexCpToDpMsgBase {
public:

    TrexDpQuit()  {
    }


    virtual TrexCpToDpMsgBase * clone();

    virtual bool handle(TrexDpCore *dp_core);

};



/**
 * a message to check if both port are idel and exit
 *
 * @author hhaim
 */
class TrexDpCanQuit : public TrexCpToDpMsgBase {
public:

    TrexDpCanQuit()  {
    }

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();
};


/**
 * barrier message for DP core
 *
 */
class TrexDpBarrier : public TrexCpToDpMsgBase {
public:

    TrexDpBarrier(uint8_t port_id, int event_id) {
        m_port_id  = port_id;
        m_event_id = event_id;
    }

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();

private:
    uint8_t   m_port_id;
    int       m_event_id;
};


/************************* messages from DP to CP **********************/

/**
 * defines the base class for DP to CP messages
 *
 * @author imarom (27-Oct-15)
 */
class TrexDpToCpMsgBase {
public:

    TrexDpToCpMsgBase() {
    }

    virtual ~TrexDpToCpMsgBase() {
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle() = 0;

    /* no copy constructor */
    TrexDpToCpMsgBase(TrexDpToCpMsgBase &) = delete;

};


/**
 * a message indicating an event has happened on a port at the
 * DP
 *
 */
class TrexDpPortEventMsg : public TrexDpToCpMsgBase {
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
class TrexCpToRxMsgBase {
public:

    TrexCpToRxMsgBase() {
    }

    virtual ~TrexCpToRxMsgBase() {
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle (CRxCore *rx_core) = 0;

    /* no copy constructor */
    TrexCpToRxMsgBase(TrexCpToRxMsgBase &) = delete;

};


class TrexRxQuit : public TrexCpToRxMsgBase {
    bool handle (CRxCore *rx_core);
};


class TrexRxCapture : public TrexCpToRxMsgBase {
public:
    virtual bool handle (CRxCore *rx_core) = 0;
};


class TrexRxCaptureStart : public TrexRxCapture {
public:
    TrexRxCaptureStart(const CaptureFilter& filter,
                                uint64_t limit,
                                TrexPktBuffer::mode_e mode,
                                MsgReply<TrexCaptureRCStart> &reply) : m_reply(reply) {
        
        m_limit  = limit;
        m_filter = filter;
        m_mode   = mode;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t                          m_port_id;
    uint64_t                         m_limit;
    CaptureFilter                    m_filter;
    TrexPktBuffer::mode_e            m_mode;
    MsgReply<TrexCaptureRCStart>    &m_reply;
};


class TrexRxCaptureStop : public TrexRxCapture {
public:
    TrexRxCaptureStop(capture_id_t capture_id, MsgReply<TrexCaptureRCStop> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    capture_id_t                   m_capture_id;
    MsgReply<TrexCaptureRCStop>   &m_reply;
};


class TrexRxCaptureFetch : public TrexRxCapture {
public:
    TrexRxCaptureFetch(capture_id_t capture_id, uint32_t pkt_limit, MsgReply<TrexCaptureRCFetch> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
        m_pkt_limit  = pkt_limit;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    capture_id_t                   m_capture_id;
    uint32_t                       m_pkt_limit;
    MsgReply<TrexCaptureRCFetch>  &m_reply;
};


class TrexRxCaptureStatus : public TrexRxCapture {
public:
    TrexRxCaptureStatus(MsgReply<TrexCaptureRCStatus> &reply) : m_reply(reply) {
    }

    virtual bool handle(CRxCore *rx_core);

private:
    MsgReply<TrexCaptureRCStatus>   &m_reply;
};



class TrexRxCaptureRemove : public TrexRxCapture {
public:
    TrexRxCaptureRemove(capture_id_t capture_id, MsgReply<TrexCaptureRCRemove> &reply) : m_reply(reply) {
        m_capture_id = capture_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    capture_id_t                   m_capture_id;
    MsgReply<TrexCaptureRCRemove> &m_reply;
};


class TrexRxStartQueue : public TrexCpToRxMsgBase {
public:
    TrexRxStartQueue(uint8_t port_id,
                              uint64_t size,
                              MsgReply<bool> &reply) : m_reply(reply) {
        
        m_port_id = port_id;
        m_size    = size;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t           m_port_id;
    uint64_t          m_size;
    MsgReply<bool>   &m_reply;
};


class TrexRxStopQueue : public TrexCpToRxMsgBase {
public:
    TrexRxStopQueue(uint8_t port_id) {
        m_port_id = port_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t m_port_id;
};



class TrexRxQueueGetPkts : public TrexCpToRxMsgBase {
public:

    TrexRxQueueGetPkts(uint8_t port_id, MsgReply<const TrexPktBuffer *> &reply) : m_reply(reply) {
        m_port_id = port_id;
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t                              m_port_id;
    MsgReply<const TrexPktBuffer *>     &m_reply;
    
};

/**
 * get capabilities of stack
 */
class TrexRxGetStackCaps : public TrexCpToRxMsgBase {
public:
    TrexRxGetStackCaps(uint8_t port_id, MsgReply<uint16_t> &reply) : m_reply(reply) {
        m_port_id       = port_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    MsgReply<uint16_t>  &m_reply;
};

/**
 * updates the RX core that we are in L2 mode
 */
class TrexRxSetL2Mode : public TrexCpToRxMsgBase {
public:
    TrexRxSetL2Mode(uint8_t port_id, const std::string &dst_mac) {
        m_port_id       = port_id;
        m_dst_mac       = dst_mac;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    std::string         m_dst_mac;
};


/**
 * updates the RX core that we are in a L3 mode
 */
class TrexRxSetL3Mode : public TrexCpToRxMsgBase {
public:
    TrexRxSetL3Mode(uint8_t port_id, const std::string &src_ipv4, const std::string &dst_ipv4, const std::string *dst_mac) {
        m_port_id       = port_id;
        m_src_ipv4      = src_ipv4;
        m_dst_ipv4      = dst_ipv4;
        m_dst_mac       = (dst_mac == nullptr) ? "" : *dst_mac;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    std::string         m_src_ipv4;
    std::string         m_dst_ipv4;
    std::string         m_dst_mac;
};

/**
 * Configure IPv6 of port
 */
class TrexRxConfIPv6 : public TrexCpToRxMsgBase {
public:
    TrexRxConfIPv6(uint8_t port_id, bool enabled, const std::string &src_ipv6) {
        m_port_id       = port_id;
        m_enabled       = enabled;
        m_src_ipv6      = src_ipv6;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    bool                m_enabled;
    std::string         m_src_ipv6;
};

/**
 * Get port node for data
 */
class TrexRxGetPortNode : public TrexCpToRxMsgBase {
public:
    TrexRxGetPortNode(uint8_t port_id, CNodeBase &node, std::string &err, MsgReply<bool> &reply) :
                    m_node(node), m_err(err), m_reply(reply) {
        m_port_id       = port_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    CNodeBase          &m_node;
    std::string        &m_err;
    MsgReply<bool>     &m_reply;
};

/**
 * Is RX in the middle of config
 */
class TrexRxIsRunningTasks : public TrexCpToRxMsgBase {
public:
    TrexRxIsRunningTasks(uint8_t port_id, MsgReply<bool> &reply) : m_reply(reply) {
        m_port_id       = port_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    MsgReply<bool>     &m_reply;
};

/**
 * Get results of running config tasks of RX
 */
class TrexRxGetTasksResults : public TrexCpToRxMsgBase {
public:
    TrexRxGetTasksResults(uint8_t port_id, uint64_t ticket_id, stack_result_t &results, MsgReply<bool> &reply) :
                    m_results(results), m_reply(reply) {
        m_port_id       = port_id;
        m_ticket_id     = ticket_id;
    }

    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t             m_port_id;
    uint64_t            m_ticket_id;
    stack_result_t     &m_results;
    MsgReply<bool>     &m_reply;
};

class TrexRxRunCfgTasks : public TrexCpToRxMsgBase {
public:
    TrexRxRunCfgTasks(uint8_t port_id, uint64_t ticket_id) {
        m_port_id = port_id;
        m_ticket_id = ticket_id;
    }
    virtual bool handle(CRxCore *rx_core);
private:
    uint8_t                  m_port_id;
    uint64_t                 m_ticket_id;
};

class TrexRxCancelCfgTasks : public TrexCpToRxMsgBase {
public:
    TrexRxCancelCfgTasks(uint8_t port_id) {
        m_port_id = port_id;
    }
    virtual bool handle(CRxCore *rx_core);
private:
    uint8_t                 m_port_id;
};

/**
 * a request from RX core to dump to Json with MAC/IP/VLAN etc. of port
 */
class TrexPortAttrToJson : public TrexCpToRxMsgBase {
public:
    
    TrexPortAttrToJson(uint8_t port_id, Json::Value &attr_res, MsgReply<bool> &reply) :
                    m_attr_res(attr_res), m_reply(reply) {
        m_port_id = port_id;
    }
     
    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCore *rx_core);
    
private:
    uint8_t                  m_port_id;
    Json::Value             &m_attr_res;
    MsgReply<bool>          &m_reply;
};


/**
 * a request from RX core to dump to Json the RX features
 */
class TrexRxFeaturesToJson : public TrexCpToRxMsgBase {
public:
    
    TrexRxFeaturesToJson(uint8_t port_id, Json::Value &feat_res, MsgReply<bool> &reply) :
                    m_feat_res(feat_res), m_reply(reply) {
        m_port_id = port_id;
    }
     
    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCore *rx_core);
    
private:
    uint8_t                  m_port_id;
    Json::Value             &m_feat_res;
    MsgReply<bool>          &m_reply;
};


/**
 * VLAN config message to RX core
 */
class TrexRxSetVLAN : public TrexCpToRxMsgBase {
public:

    TrexRxSetVLAN(uint8_t port_id, const vlan_list_t &vlan_list) : m_vlan_list(vlan_list) {
        m_port_id    = port_id;
    }
    
    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t           m_port_id;
    vlan_list_t       m_vlan_list;
};

/**
 * Invalidate dst MAC
 */
class TrexRxInvalidateDstMac : public TrexCpToRxMsgBase {
public:

    TrexRxInvalidateDstMac(uint8_t port_id) {
        m_port_id    = port_id;
    }
    
    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t           m_port_id;
};

/**
 * Is dst MAC valid?
 */
class TrexRxIsDstMacValid : public TrexCpToRxMsgBase {
public:

    TrexRxIsDstMacValid(uint8_t port_id, MsgReply<bool> &reply) : m_reply(reply){
        m_port_id    = port_id;
    }
    
    virtual bool handle(CRxCore *rx_core);

private:
    uint8_t          m_port_id;
    MsgReply<bool>  &m_reply;
};

/**
 * sends binary pkts through the RX core
 */
class TrexRxTXPkts : public TrexCpToRxMsgBase {
public:
    
    TrexRxTXPkts(uint8_t port_id, const std::vector<std::string> &pkts, uint32_t ipg_usec, MsgReply<uint32_t> &reply) :
                    m_pkts(pkts), m_reply(reply) {
        m_port_id  = port_id;
        m_ipg_usec = ipg_usec;
    }

    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCore *rx_core);

private:
    std::vector<std::string>      m_pkts;
    uint32_t                      m_ipg_usec;
    MsgReply<uint32_t>           &m_reply;
    uint8_t                       m_port_id;
};


#endif /* __TREX_MESSAGING_H__ */

