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

#include "trex_publisher.h"
#include "trex_rpc_zip.h"
#include <zmq.h>
#include <assert.h>
#include <sstream>
#include <iostream>

/**
 * create the publisher
 * 
 */
bool 
TrexPublisher::Create(uint16_t port, bool disable){

    char thread_name[256];

    if (disable) {
        return (true);
    }

    m_context = zmq_ctx_new();
    if ( m_context == 0 ) {
        show_zmq_last_error("can't connect to ZMQ library");
    }

    /* change the pthread name temporarly for the socket creation */
    pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
    pthread_setname_np(pthread_self(), "Trex Publisher");

    m_publisher = zmq_socket (m_context, ZMQ_PUB);

    /* restore it */
    pthread_setname_np(pthread_self(), thread_name);

    if ( m_context == 0 ) {
        show_zmq_last_error("can't create ZMQ socket");
    }

    std::stringstream ss;
    ss << "tcp://*:" << port;

    int rc = zmq_bind (m_publisher, ss.str().c_str());
    if (rc != 0 ) {
        show_zmq_last_error("can't bind to ZMQ socket at " + ss.str());
    }

    std::cout << "zmq publisher at: " << ss.str() << "\n";
    return (true);
}


void 
TrexPublisher::Delete(){
    if (m_publisher) {
        
        /* before calling zmq_close set the linger property to zero
           (othersie zmq_ctx_destroy might hang forever)
         */
        int val = 0;
        zmq_setsockopt(m_publisher, ZMQ_LINGER, &val, sizeof(val));
        
        zmq_close (m_publisher);
        m_publisher = NULL;
    }

/* Deadlock inside ZMQ better to have leakage in termination - see 
  https://trex-tgn.cisco.com/youtrack/issue/trex-361 */
#if 0
    if (m_context) {
        zmq_ctx_destroy (m_context);
        m_context = NULL;
    }
#endif
}


void 
TrexPublisher::publish_json(const std::string &s, uint32_t zip_threshold){

    if (m_publisher) {
        if ( (zip_threshold != 0) && (s.size() > zip_threshold) ) {
            publish_zipped_json(s);
        } else {
            publish_raw_json(s);
        }
    }
}

void 
TrexPublisher::publish_zipped_json(const std::string &s) {
    std::string compressed_msg;

    TrexRpcZip::compress(s, compressed_msg);
    int size = zmq_send (m_publisher, compressed_msg.c_str(), compressed_msg.length(), 0);
    assert(size == compressed_msg.length());
}

void 
TrexPublisher::publish_raw_json(const std::string &s) {
     int size = zmq_send (m_publisher, s.c_str(), s.length(), 0);
     assert(size == s.length());
}

void
TrexPublisher::publish_event(event_type_e type, const Json::Value &data) {
    Json::FastWriter writer;
    Json::Value value;
    std::string s;
    
    value["name"] = "trex-event";
    value["type"] = type;
    value["data"] = data;

    s = writer.write(value);
    publish_json(s);
}

void
TrexPublisher::publish_barrier(uint32_t key) {
    Json::FastWriter writer;
    Json::Value value;
    std::string s;
    
    value["name"] = "trex-barrier";
    value["type"] = key;
    value["data"] = Json::objectValue;

    s = writer.write(value);
    publish_json(s);
}


/**
 * error handling
 * 
 */
void 
TrexPublisher::show_zmq_last_error(const std::string &err){
    std::cout << " ERROR " << err << "\n";
    std::cout << " ZMQ: " << zmq_strerror (zmq_errno ());
    exit(-1);
}

