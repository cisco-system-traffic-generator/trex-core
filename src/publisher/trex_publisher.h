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

class TrexPublisher {

public:

    TrexPublisher() {
        m_context = NULL;
        m_publisher = NULL;
    }

    virtual ~TrexPublisher() {}

    virtual bool Create(uint16_t port, bool disable);
    virtual void Delete();
    virtual void publish_json(const std::string &s);

    enum event_type_e {
        EVENT_PORT_STARTED          = 0,
        EVENT_PORT_STOPPED          = 1,
        EVENT_PORT_PAUSED           = 2,
        EVENT_PORT_RESUMED          = 3,
        EVENT_PORT_FINISHED_TX      = 4,
        EVENT_PORT_FORCE_ACQUIRED   = 5,

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

private:
    void show_zmq_last_error(const std::string &err);
private:
    void * m_context;
    void * m_publisher;
};

#endif /* __TREX_PUBLISHER_H__ */
