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

class CAstfDB;

// create tcp batch per DP core
class TrexAstfDpCreateTcp : public TrexCpToDpMsgBase {
public:
    TrexAstfDpCreateTcp(profile_id_t profile_id, double factor, CAstfDB* astf_db);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    double m_factor;
    CAstfDB* m_astf_db;
};

// delete tcp batch per DP core
class TrexAstfDpDeleteTcp : public TrexCpToDpMsgBase {
public:
    TrexAstfDpDeleteTcp(profile_id_t profile_id, bool do_remove, CAstfDB* astf_db);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    bool m_do_remove;   // to remove profile context explicitly
    CAstfDB* m_astf_db;
};

/**
 * a message to start traffic
 *
 */
class TrexAstfDpStart : public TrexCpToDpMsgBase {
public:
    TrexAstfDpStart(profile_id_t profile_id, double duration, bool nc);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    double m_duration;
    bool m_nc_flow_close;
};

/**
 * a message to stop traffic
 *
 */
class TrexAstfDpStop : public TrexCpToDpMsgBase {
public:
    TrexAstfDpStop(profile_id_t profile_id, uint32_t stop_id);
    TrexAstfDpStop(profile_id_t profile_id) : TrexAstfDpStop(profile_id, 0) {}
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
    virtual void on_node_remove();
    void set_core_ptr(CFlowGenListPerThread* core) { m_core = core; }
private:
    CFlowGenListPerThread* m_core;
    profile_id_t m_profile_id;
    uint32_t m_stop_id;
};

/**
 * a message to stop DP scheduler
 *
 */
class TrexAstfDpScheduler : public TrexCpToDpMsgBase {
public:
    TrexAstfDpScheduler(bool activate);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    bool m_activate;    // DP scheduler activate(true) or deactivate(false)
};

/**
 * a message to update traffic rate
 *
 */
class TrexAstfDpUpdate : public TrexCpToDpMsgBase {
public:
    TrexAstfDpUpdate(profile_id_t profile_id, double old_new_ratio);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    double m_old_new_ratio;
};

/**
 * a message to load DB
 *
 */
class TrexAstfLoadDB : public TrexCpToDpMsgBase {
public:
    TrexAstfLoadDB(profile_id_t profile_id, std::string *profile_buffer, std::string *topo_buffer, CAstfDB* astf_db);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    std::string *m_profile_buffer;
    std::string *m_topo_buffer;
    CAstfDB* m_astf_db;
};

/**
 * a message to remove DB
 *
 */
class TrexAstfDeleteDB : public TrexCpToDpMsgBase {
public:
    TrexAstfDeleteDB(profile_id_t profile_id, CAstfDB* astf_db);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    CAstfDB* m_astf_db;
};

/**
* a message to set service mode on / filtered with mask
*/
class TrexAstfDpServiceMode : public TrexCpToDpMsgBase {
public:
    TrexAstfDpServiceMode(bool enabled, bool filtered = false, uint8_t mask = 0) {
        m_enabled  = enabled;
        m_filtered = filtered;
        m_mask     = mask;
    }
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    bool      m_enabled;
    bool      m_filtered;
    uint8_t   m_mask;
};

/**
+ * a message to Activate/Deactivate Client
+ *
+ */

class TrexAstfDpActivateClient : public TrexCpToDpMsgBase {
public:
    TrexAstfDpActivateClient(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool activate, bool is_range);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    CAstfDB* m_astf_db;
    std::vector<uint32_t> m_msg_data;
    bool     m_activate;
    bool     m_is_range;
};

/**
+ * a message to get clients information
+ *
+ */

class TrexAstfDpGetClientStats : public TrexCpToDpMsgBase {
public:
    TrexAstfDpGetClientStats(CAstfDB* astf_db, std::vector<uint32_t> msg_data, bool is_range, MsgReply<Json::Value> &reply) : m_reply(reply) {
        m_astf_db = astf_db;
        m_msg_data =  msg_data;
        m_is_range = is_range;
    }
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    CAstfDB* m_astf_db;
    std::vector<uint32_t> m_msg_data;
    bool     m_is_range;
    MsgReply<Json::Value> &m_reply;
};

/**
 * a message to Add tunnel information for a client
 *
 */

class TrexAstfDpUpdateTunnelClient : public TrexCpToDpMsgBase {
public:
    TrexAstfDpUpdateTunnelClient(CAstfDB* astf_db, std::vector<client_tunnel_data_t> msg_data, uint8_t tunnel_type);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    CAstfDB* m_astf_db;
    std::vector<client_tunnel_data_t> m_msg_data;
    uint8_t m_tunnel_type;
};


#endif /* __TREX_STL_MESSAGING_H__ */

