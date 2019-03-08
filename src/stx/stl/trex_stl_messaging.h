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
#ifndef __TREX_STL_MESSAGING_H__
#define __TREX_STL_MESSAGING_H__

#include "trex_messaging.h"
#include "trex_defs.h"

class TrexDpCore;
class TrexStreamsCompiledObj;
class CFlowGenListPerThread;


/**
 * a message to start traffic
 *
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStart : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpStart(uint8_t port_id, uint32_t profile_id, int event_id, TrexStreamsCompiledObj *obj, double duration, double start_at_ts = 0);
    TrexStatelessDpStart(uint8_t port_id, int event_id, TrexStreamsCompiledObj *obj, double duration, double start_at_ts = 0) : TrexStatelessDpStart(port_id, 0, event_id, obj, duration, start_at_ts) {
    }

    ~TrexStatelessDpStart();

    virtual TrexCpToDpMsgBase * clone();

    virtual bool handle(TrexDpCore *dp_core);

private:

    uint8_t                 m_port_id;
    uint32_t                m_profile_id;
    int                     m_event_id;
    TrexStreamsCompiledObj *m_obj;
    double                  m_duration;
    double                  m_start_at_ts;

};



class TrexStatelessDpPause : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpPause(uint8_t port_id, uint32_t profile_id) : m_port_id(port_id), m_profile_id(profile_id) {
    }

    TrexStatelessDpPause(uint8_t port_id) : TrexStatelessDpPause(port_id, 0) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    uint32_t m_profile_id;
};

class TrexStatelessDpPauseStreams : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpPauseStreams(uint8_t port_id, stream_ids_t &stream_ids) : m_port_id(port_id),
                                                                             m_stream_ids(stream_ids) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    stream_ids_t m_stream_ids;
};


class TrexStatelessDpResume : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpResume(uint8_t port_id, uint32_t profile_id) : m_port_id(port_id), m_profile_id(profile_id) {
    }

    TrexStatelessDpResume(uint8_t port_id) : TrexStatelessDpResume(port_id, 0) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    uint32_t m_profile_id;
};

class TrexStatelessDpResumeStreams : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpResumeStreams(uint8_t port_id, stream_ids_t &stream_ids) : m_port_id(port_id),
                                                                              m_stream_ids(stream_ids) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    stream_ids_t m_stream_ids;
};

/**
 * a message to stop traffic
 *
 * @author imarom (27-Oct-15)
 */
class TrexStatelessDpStop : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpStop(uint8_t port_id, uint32_t profile_id) : m_port_id(port_id), m_profile_id(profile_id) {
        m_stop_only_for_event_id=false;
        m_event_id = 0;
        m_core = NULL;
    }

    TrexStatelessDpStop(uint8_t port_id) : TrexStatelessDpStop(port_id, 0) {
    }

    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);

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
    uint8_t                   m_port_id;
    uint32_t                  m_profile_id;
    bool                      m_stop_only_for_event_id;
    int                       m_event_id;
    CFlowGenListPerThread   * m_core ;

};



/**
 * update message
 */
class TrexStatelessDpUpdate : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpUpdate(uint8_t port_id, uint32_t profile_id, double factor)  {
        m_port_id = port_id;
        m_profile_id = profile_id;
        m_factor  = factor;
    }

    TrexStatelessDpUpdate(uint8_t port_id, double factor): TrexStatelessDpUpdate(port_id, 0, factor) {
    }

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();

private:
    uint8_t  m_port_id;
    uint32_t m_profile_id;
    double   m_factor;
};

class TrexStatelessDpUpdateStreams : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpUpdateStreams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream) {
        m_port_id = port_id;
        m_ipg_per_stream = ipg_per_stream;
    }

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();

private:
    uint8_t  m_port_id;
    stream_ipgs_map_t m_ipg_per_stream;
};


/**
 * psuh a PCAP message
 */
class TrexStatelessDpPushPCAP : public TrexCpToDpMsgBase {
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

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();

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
 * move a DP core in/out of service mode (slower as it might do
 * capturing and etc.) 
 *
 */
class TrexStatelessDpServiceMode : public TrexCpToDpMsgBase {
public:

    TrexStatelessDpServiceMode(uint8_t port_id, bool enabled) {
        m_port_id = port_id;
        m_enabled = enabled;
    }

    virtual TrexCpToDpMsgBase * clone();

    virtual bool handle(TrexDpCore *dp_core);

private:

    uint8_t   m_port_id;
    bool      m_enabled;
};



class TrexStatelessRxQuery : public TrexCpToRxMsgBase {
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
        RC_FAIL_CAPTURE_PORT_ACTIVE,
        RC_FAIL_CAPWAP_PROXY_ACTIVE,
        RC_FAIL_CAPWAP_PROXY_INACTIVE,
    };
    
    TrexStatelessRxQuery(uint8_t port_id, query_type_e query_type, MsgReply<query_rc_e> &reply) : m_reply(reply) {
        m_port_id    = port_id;
        m_query_type = query_type;
    }
     
    /**
     * virtual function to handle a message
     *
     */
    virtual bool handle(CRxCore *rx_core);
    
private:
    uint8_t                m_port_id;
    query_type_e           m_query_type;
    MsgReply<query_rc_e>  &m_reply;
};


class TrexStatelessRxEnableLatency : public TrexCpToRxMsgBase {
public:
    TrexStatelessRxEnableLatency(MsgReply<bool> &reply) : m_reply(reply) {
    }
    
    bool handle (CRxCore *rx_core);
    
private:
    MsgReply<bool>    &m_reply;
};


class TrexStatelessRxDisableLatency : public TrexCpToRxMsgBase {
public:
    bool handle (CRxCore *rx_core);
};



#endif /* __TREX_STL_MESSAGING_H__ */

