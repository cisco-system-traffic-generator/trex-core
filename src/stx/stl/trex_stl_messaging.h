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
class TPGCpCtx;
class PacketGroupTagMgr;
class TPGDpMgrPerSide;


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

    TrexStatelessDpPauseStreams(uint8_t port_id, uint32_t profile_id, stream_ids_t &stream_ids) : m_port_id(port_id), m_profile_id(profile_id), m_stream_ids(stream_ids) {
    }

    TrexStatelessDpPauseStreams(uint8_t port_id, stream_ids_t &stream_ids) : TrexStatelessDpPauseStreams(port_id, 0, stream_ids) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    uint32_t m_profile_id;
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

    TrexStatelessDpResumeStreams(uint8_t port_id, uint32_t profile_id, stream_ids_t &stream_ids) : m_port_id(port_id), m_profile_id(profile_id), m_stream_ids(stream_ids) {
    }

    TrexStatelessDpResumeStreams(uint8_t port_id, stream_ids_t &stream_ids) : TrexStatelessDpResumeStreams(port_id, 0, stream_ids) {
    }


    virtual TrexCpToDpMsgBase * clone();


    virtual bool handle(TrexDpCore *dp_core);


private:
    uint8_t m_port_id;
    uint32_t m_profile_id;
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

    TrexStatelessDpUpdateStreams(uint8_t port_id, uint32_t profile_id, stream_ipgs_map_t &ipg_per_stream) {
        m_port_id = port_id;
        m_profile_id = profile_id;
        m_ipg_per_stream = ipg_per_stream;
    }

    TrexStatelessDpUpdateStreams(uint8_t port_id, stream_ipgs_map_t &ipg_per_stream): TrexStatelessDpUpdateStreams(port_id, 0, ipg_per_stream) {
    }

    virtual bool handle(TrexDpCore *dp_core);

    virtual TrexCpToDpMsgBase * clone();

private:
    uint8_t  m_port_id;
    uint32_t m_profile_id;
    stream_ipgs_map_t m_ipg_per_stream;
};


/**
 * push a PCAP message
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

    TrexStatelessDpServiceMode(uint8_t port_id, bool enabled, bool filtered = false, uint8_t mask = 0) {
        m_port_id  = port_id;
        m_enabled  = enabled;
        m_filtered = filtered;
        m_mask     = mask;
    }

    virtual TrexCpToDpMsgBase * clone();

    virtual bool handle(TrexDpCore *dp_core);

private:

    uint8_t   m_port_id;
    bool      m_enabled;
    bool      m_filtered;
    uint8_t   m_mask;
};



class TrexStatelessRxQuery : public TrexCpToRxMsgBase {
public:

    /**
     * query type to request
     */
    enum query_type_e {
        SERVICE_MODE_ON,
        SERVICE_MODE_OFF,
        SERVICE_MODE_FILTERED,
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

class TrexStatelessRxEnableTaggedPktGroup: public TrexCpToRxMsgBase {
public:
    TrexStatelessRxEnableTaggedPktGroup(TPGCpCtx* tpg_ctx)
                                        : m_tpg_ctx(tpg_ctx) {}

    bool handle(CRxCore *rx_core);

private:
    TPGCpCtx*                  m_tpg_ctx;          // Pointer to Control Plane TPG Context.
};

class TrexStatelessRxDisableTaggedPktGroup: public TrexCpToRxMsgBase {
public:
    TrexStatelessRxDisableTaggedPktGroup(const std::string& username)
                                         : m_username(username) {}

    bool handle(CRxCore *rx_core);

private:
    const std::string       m_username;     // User for whom we are disabling TPG

};

class TrexStatelessRxGetTPGState: public TrexCpToRxMsgBase {
public:

    TrexStatelessRxGetTPGState(const std::string& username, MsgReply<int>& reply)
                                         : m_username(username), m_reply(reply) {}

    bool handle(CRxCore *rx_core);

private:
    const std::string         m_username;     // User for whom we are checking if TPG is enabled.
    MsgReply<int>&            m_reply;        // Reply object
};

class TrexStatelessRxUpdateTPGTags: public TrexCpToRxMsgBase {
public:

    TrexStatelessRxUpdateTPGTags(MsgReply<bool>& reply, const std::string& username, PacketGroupTagMgr* tag_mgr)
                                         :  m_reply(reply), m_username(username), m_tag_mgr(tag_mgr) {}

    bool handle(CRxCore *rx_core);

private:

    MsgReply<bool>&           m_reply;        // Reply object
    const std::string         m_username;     // User for whom we are checking if TPG is enabled.
    PacketGroupTagMgr*        m_tag_mgr;      // Tag Manager to copy
};

class TrexStatelessRxClearTPGStatsTpgid : public TrexCpToRxMsgBase {
public:

    TrexStatelessRxClearTPGStatsTpgid(MsgReply<bool>& reply, uint8_t port_id, uint32_t min_tpgid, uint32_t max_tpgid, const std::vector<uint16_t>& tag_list)
                                            : m_reply(reply), m_port_id(port_id), m_min_tpgid(min_tpgid), m_max_tpgid(max_tpgid), m_tag_list(tag_list){}

    bool handle(CRxCore *rx_core);

private:
    MsgReply<bool>&                 m_reply;        // Reply object
    uint8_t                         m_port_id;      // Port Id
    uint32_t                        m_min_tpgid;    // Min Tpgid
    uint32_t                        m_max_tpgid;    // Max Tpgid
    const std::vector<uint16_t>     m_tag_list;     // List of Tags to clear
};

class TrexStatelessRxClearTPGStatsList : public TrexCpToRxMsgBase {
public:

    TrexStatelessRxClearTPGStatsList(MsgReply<bool>& reply, uint8_t port_id, uint32_t tpgid, const std::vector<uint16_t>& tag_list, bool unknown_tag, bool untagged)
                                            : m_reply(reply), m_port_id(port_id), m_tpgid(tpgid), m_tag_list(tag_list), m_unknown_tag(unknown_tag), m_untagged(untagged) {}

    bool handle(CRxCore *rx_core);

private:
    MsgReply<bool>&                 m_reply;        // Reply object
    uint8_t                         m_port_id;      // Port Id
    uint32_t                        m_tpgid;        // Tpgid
    const std::vector<uint16_t>     m_tag_list;     // List of Tags to clear
    bool                            m_unknown_tag;  // Should clear unknown tag stats?
    bool                            m_untagged;     // Should clear untagged stats?
};

class TrexStatelessRxClearTPGStats : public TrexCpToRxMsgBase {
public:

    TrexStatelessRxClearTPGStats(MsgReply<bool>& reply, uint8_t port_id, uint32_t tpgid, uint16_t min_tag, uint16_t max_tag, bool unknown_tag, bool untagged)
                                            : m_reply(reply), m_port_id(port_id), m_tpgid(tpgid), m_min_tag(min_tag), m_max_tag(max_tag), m_unknown_tag(unknown_tag), m_untagged(untagged) {}

    bool handle(CRxCore *rx_core);

private:
    MsgReply<bool>&      m_reply;        // Reply object
    uint8_t              m_port_id;      // Port Id
    uint32_t             m_tpgid;        // Tpgid
    uint16_t             m_min_tag;      // Min Tag to clear
    uint16_t             m_max_tag;      // Max Tag to clear
    bool                 m_unknown_tag;  // Should clear unknown tag stats?
    bool                 m_untagged;     // Should clear untagged stats?
};

class TrexStatelessRxGetTPGUnknownTags : public TrexCpToRxMsgBase {
public:

    TrexStatelessRxGetTPGUnknownTags(MsgReply<bool>& reply, Json::Value& result, uint8_t port_id)
                                            : m_reply(reply), m_result(result), m_port_id(port_id) {}

    bool handle(CRxCore *rx_core);

private:

    MsgReply<bool>&      m_reply;        // Reply object
    Json::Value&         m_result;       // Json to write result to
    uint8_t              m_port_id;      // Port Id
};

class TrexStatelessRxClearTPGUnknownTags : public TrexCpToRxMsgBase {
public:

    TrexStatelessRxClearTPGUnknownTags(MsgReply<bool>& reply, uint8_t port_id)
                                            : m_reply(reply), m_port_id(port_id) {}

    bool handle(CRxCore *rx_core);

private:
    MsgReply<bool>&      m_reply;        // Reply object
    uint8_t              m_port_id;      // Port Id
};

class TrexStatelessDpSetLatencyFeature : public TrexCpToDpMsgBase {
public:
    TrexStatelessDpSetLatencyFeature(MsgReply<bool>& reply) : m_reply(reply) {
    }
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>      &m_reply;
};

class TrexStatelessDpUnsetLatencyFeature: public TrexCpToDpMsgBase {
public:
    TrexStatelessDpUnsetLatencyFeature(MsgReply<bool>& reply) : m_reply(reply) {
    }
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>      &m_reply;
};

class TrexStatelessDpSetCaptureFeature : public TrexCpToDpMsgBase {
public:
    TrexStatelessDpSetCaptureFeature(MsgReply<bool>& reply) : m_reply(reply) {
    }
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>      &m_reply;
};

class TrexStatelessDpUnsetCaptureFeature: public TrexCpToDpMsgBase {
public:
    TrexStatelessDpUnsetCaptureFeature(MsgReply<bool>& reply) : m_reply(reply) {
    }
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>      &m_reply;
};

class TrexStatelessDpSetTPGFeature : public TrexCpToDpMsgBase {
public:
    TrexStatelessDpSetTPGFeature(MsgReply<int>& reply, uint32_t num_pgids, bool designated_core) :
                                m_reply(reply), m_num_pgids(num_pgids), m_designated_core(designated_core) {}
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<int>&      m_reply;
    uint32_t            m_num_pgids;                    // Number of Tagged Packet Group Identifiers
    bool                m_designated_core;              // This core actually sends TPG packets (sequenced packets are send from one core only)
};

class TrexStatelessDpUnsetTPGFeature: public TrexCpToDpMsgBase {
public:
    TrexStatelessDpUnsetTPGFeature(MsgReply<bool>& reply) : m_reply(reply) {}
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>&     m_reply;
};

class TrexStatelessDpGetTPGMgr: public TrexCpToDpMsgBase {
public:
    TrexStatelessDpGetTPGMgr(MsgReply<TPGDpMgrPerSide*>& reply, uint8_t port) : m_reply(reply), m_port(port) {}
    virtual TrexCpToDpMsgBase * clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<TPGDpMgrPerSide*>&     m_reply;
    uint8_t                         m_port;
};

class TrexStatelessDpClearTPGTxStats: public TrexCpToDpMsgBase {
public:
    TrexStatelessDpClearTPGTxStats(MsgReply<bool>& reply, uint8_t port, uint32_t tpgid) : m_reply(reply), m_port(port), m_tpgid(tpgid) {}
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore* dp_core);

private:
    MsgReply<bool>&     m_reply;
    uint8_t             m_port;
    uint32_t            m_tpgid;
};


#endif /* __TREX_STL_MESSAGING_H__ */

