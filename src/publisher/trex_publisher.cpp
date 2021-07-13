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
#include "rpc-server/trex_rpc_zip.h"
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
    
    m_is_connected = true;
    
    return (true);
}


void 
TrexPublisher::Delete(int timeout_sec) {

    m_is_connected = false;
    
    if (m_publisher) {

        /* before calling zmq_close set the linger property to the specific timeout in seconds
           by the default the value is zero (do not wait)
           (othersie zmq_ctx_destroy might hang forever)
         */
        int val = timeout_sec;
        zmq_setsockopt(m_publisher, ZMQ_LINGER, &val, sizeof(val));

        zmq_close (m_publisher);
        m_publisher = NULL;
    }

    if (m_context) {
        zmq_ctx_destroy (m_context);
        m_context = NULL;
    }

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
    if (size > 0) {
      assert(size == compressed_msg.length());
    }else{
        printf(" publish_zipped_json zmq_send error %d \n",size);
    }
}

void 
TrexPublisher::publish_raw_json(const std::string &s) {
     int size = zmq_send (m_publisher, s.c_str(), s.length(), 0);
     if (size > 0) {
        assert(size == s.length());
     }else{
        printf(" publish_raw_json zmq_send error %d \n",size);
     }
}

bool  TrexPublisher::add_session_id(uint32_t session_id, bool is_reader){
    // check if it not already exists 
    for (int i = 0; i < m_ctxs.size(); i++ ) {
        if (m_ctxs[i]->m_session_id ==session_id) {
            if (!is_reader) {
                m_ctxs[i]->m_reader_mode = is_reader;
            }
            return false;
        }
    }
    TrexPublisherCtx * ctx = new TrexPublisherCtx();
    ctx->m_session_id = session_id;
    ctx->m_seq =0;
    ctx->m_qevents = Json::arrayValue;
    ctx->m_last_query = now_sec();
    ctx->m_reader_mode = is_reader;

    m_ctxs.push_back(ctx);
    return (true);
}

bool  TrexPublisher::remove_session_id(uint32_t session_id){
    for (int i = 0; i < m_ctxs.size(); i++ ) {
        if (m_ctxs[i]->m_session_id == session_id) {
            TrexPublisherCtx* p=m_ctxs[i];
            p->m_reader_mode = true; // changed to read-only session
            return (true);
        }
    }
    return (false);
}

bool  TrexPublisher::get_queue_events(Json::Value & val,
                                      uint32_t session_id){
    for (int i = 0; i < m_ctxs.size(); i++ ) {
        if (m_ctxs[i]->m_session_id == session_id) {
            val = m_ctxs[i]->m_qevents;
            m_ctxs[i]->m_qevents = Json::arrayValue;
            m_ctxs[i]->m_last_query = now_sec();
            return true;
        }
    }
    return false;
    //assert(0);
}

void
TrexPublisher::publish_event(event_type_e type, const Json::Value &data) {
    Json::Value value;
    
    value["name"] = "trex-event";
    value["type"] = type;
    value["data"] = data;

    if (m_is_interactive){
        for (auto it = m_ctxs.begin(); it != m_ctxs.end(); ) {
            TrexPublisherCtx* ctx = *it;
            // don't add to stall context -- disconnect without removing the session_id
            if (ctx->m_qevents.empty() || ((now_sec() - ctx->m_last_query) < 10.0)) {
                Json::Value valctx;
                valctx =value;
                valctx["seq"] = ctx->m_seq++;
                if (ctx->m_qevents.empty()) {
                    ctx->m_last_query = now_sec();
                }
                ctx->m_qevents.append(valctx);
            }
            else if (ctx->m_reader_mode) {
                delete *it;
                it = m_ctxs.erase(it);
                continue;
            }
            it++;
        }
        publish_json("{}");
    }else{
        Json::FastWriter writer;
        std::string s;
        s = writer.write(value);
        publish_json(s);
    }
}

void
TrexPublisher::publish_barrier(uint32_t key) {

    if (!m_is_interactive){
        Json::FastWriter writer;
        Json::Value value;
        std::string s;
        
        value["name"] = "trex-barrier";
        value["type"] = key;
        value["data"] = Json::objectValue;

        s = writer.write(value);
        publish_json(s);
    }else{
        publish_json("{}");
    }
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

