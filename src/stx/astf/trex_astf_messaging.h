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


// create tcp batch per DP core
class TrexAstfDpCreateTcp : public TrexCpToDpMsgBase {
public:
    TrexAstfDpCreateTcp(profile_id_t profile_id, double factor);
    TrexAstfDpCreateTcp() : TrexAstfDpCreateTcp(0, -1) {}
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    double m_factor;
};

// delete tcp batch per DP core
class TrexAstfDpDeleteTcp : public TrexCpToDpMsgBase {
public:
    TrexAstfDpDeleteTcp(profile_id_t profile_id, bool do_remove);
    TrexAstfDpDeleteTcp() : TrexAstfDpDeleteTcp(0, false) {}
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    bool m_do_remove;   // to remove profile context explicitly
};

/**
 * a message to start traffic
 *
 */
class TrexAstfDpStart : public TrexCpToDpMsgBase {
public:
    TrexAstfDpStart(profile_id_t profile_id, double duration, bool nc);
    TrexAstfDpStart() : TrexAstfDpStart(0, -1, false) {}
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
    TrexAstfDpStop() : TrexAstfDpStop(0, 0) {}
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
    TrexAstfDpUpdate(double old_new_ratio) : TrexAstfDpUpdate(0, old_new_ratio) {}
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
    TrexAstfLoadDB(profile_id_t profile_id, std::string *profile_buffer, std::string *topo_buffer);
    TrexAstfLoadDB(std::string *profile_buffer, std::string *topo_buffer) : TrexAstfLoadDB(0, profile_buffer, topo_buffer) {}
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
    std::string *m_profile_buffer;
    std::string *m_topo_buffer;
};

/**
 * a message to remove DB
 *
 */
class TrexAstfDeleteDB : public TrexCpToDpMsgBase {
public:
    TrexAstfDeleteDB(profile_id_t profile_id);
    virtual TrexCpToDpMsgBase* clone();
    virtual bool handle(TrexDpCore *dp_core);
private:
    profile_id_t m_profile_id;
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


#endif /* __TREX_STL_MESSAGING_H__ */

