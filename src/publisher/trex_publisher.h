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
#ifndef __TREX_PUBLISHER_H__
#define __TREX_PUBLISHER_H__

#include <stdint.h>
#include <string>
#include <json/json.h>
#include <vector>
#include "os_time.h"



class TrexPublisherCtx {
    public:
    uint32_t        m_session_id;  
    Json::Value     m_qevents;
    dsec_t          m_last_query;

    uint32_t        m_seq;
    bool            m_reader_mode;
};

class TrexPublisher {

public:

    TrexPublisher() {
        m_context      = NULL;
        m_publisher    = NULL;
        m_is_connected = false;
        m_is_interactive = false;
    }

    virtual ~TrexPublisher() {}

    virtual void set_interactive_mode(bool  is_interactive){
        m_is_interactive = is_interactive;
    }

    virtual bool Create(uint16_t port, bool disable);
    virtual void Delete(int timeout_sec = 0);
    virtual void publish_json(const std::string &s, uint32_t zip_threshold = MSG_COMPRESS_THRESHOLD);

    enum event_type_e {
        EVENT_PORT_STARTED          = 0,
        EVENT_PORT_STOPPED          = 1,
        EVENT_PORT_PAUSED           = 2,
        EVENT_PORT_RESUMED          = 3,
        EVENT_PORT_FINISHED_TX      = 4,
        EVENT_PORT_ACQUIRED         = 5,
        EVENT_PORT_RELEASED         = 6,
        EVENT_PORT_ERROR            = 7,
        EVENT_PORT_ATTR_CHANGED     = 8,

        EVENT_PROFILE_STARTED       = 10,
        EVENT_PROFILE_STOPPED       = 11,
        EVENT_PROFILE_PAUSED        = 12,
        EVENT_PROFILE_RESUMED       = 13,
        EVENT_PROFILE_FINISHED_TX   = 14,
        EVENT_PROFILE_ERROR         = 17,

        EVENT_ASTF_STATE_CHG        = 50,
        /*
        EVENT_ASTF_IDLE             = 50,
        EVENT_ASTF_LOADED           = 51,
        EVENT_ASTF_PARSE            = 52,
        EVENT_ASTF_BUILD            = 53,
        EVENT_ASTF_TX               = 54,
        EVENT_ASTF_CLEANUP          = 55,
        */

        EVENT_ASTF_PROFILE_STATE_CHG = 60,
        EVENT_ASTF_PROFILE_CLEARED   = 61,

        EVENT_SERVER_STOPPED        = 100,
        
        
    };

    /**
     * publishes an async event
     * 
     */
    virtual void publish_event(event_type_e type, const Json::Value &data = Json::nullValue);

    /**
     * publishes a barrier requested by the user
     * 
     * @author imarom (17-Jan-16)
     * 
     */
    virtual void publish_barrier(uint32_t key);

    virtual bool  get_queue_events(Json::Value & val,uint32_t session_id);

    virtual bool  add_session_id(uint32_t session_id, bool is_reader);

    virtual bool  remove_session_id(uint32_t session_id);


    /**
     * return true if the publisher socket is currently connected
     * 
     */
    bool is_connected() const {
        return (m_is_connected);
    }
    
private:
    void show_zmq_last_error(const std::string &err);
    void publish_zipped_json(const std::string &s);
    void publish_raw_json(const std::string &s);

private:
    void * m_context;
    void * m_publisher;
    bool   m_is_connected;
    bool   m_is_interactive;

    std::vector<TrexPublisherCtx*> m_ctxs;
    
    static const int MSG_COMPRESS_THRESHOLD = 256;
};

#endif /* __TREX_PUBLISHER_H__ */
