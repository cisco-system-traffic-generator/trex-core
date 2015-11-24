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
#ifndef __TREX_STATELESS_PORT_H__
#define __TREX_STATELESS_PORT_H__

#include <trex_stream.h>
#include <trex_dp_port_events.h>

class TrexPlatformApi;
class TrexStatelessCpToDpMsgBase;

/**
 * describes a stateless port
 * 
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort {
    friend class TrexDpPortEvent;

public:

    /**
     * port state
     */
    enum port_state_e {
        PORT_STATE_DOWN = 0x1,
        PORT_STATE_IDLE = 0x2,
        PORT_STATE_STREAMS = 0x4,
        PORT_STATE_TX = 0x8,
        PORT_STATE_PAUSE = 0x10,
    };

    /**
     * describess different error codes for port operations
     */
    enum rc_e {
        RC_OK,
        RC_ERR_BAD_STATE_FOR_OP,
        RC_ERR_NO_STREAMS,
        RC_ERR_FAILED_TO_COMPILE_STREAMS
    };

    TrexStatelessPort(uint8_t port_id, const TrexPlatformApi *api);
    
    /**
     * acquire port
     * throws TrexException in case of an error
     */
    void acquire(const std::string &user, bool force = false);

    /**
     * release the port from the current user
     * throws TrexException in case of an error
     */
    void release(void);

    /**
     * start traffic
     * throws TrexException in case of an error
     */
    void start_traffic(double mul, double duration = -1);

    /**
     * stop traffic
     * throws TrexException in case of an error
     */
    void stop_traffic(void);

    /**
     * pause traffic
     * throws TrexException in case of an error
     */
    void pause_traffic(void);

    /**
     * resume traffic
     * throws TrexException in case of an error
     */
    void resume_traffic(void);

    /**
     * update current traffic on port
     * 
     */
    void update_traffic(double mul);

    /**
     * get the port state
     * 
     */
    port_state_e get_state() const {
        return m_port_state;
    }

    /**
     * port state as string
     * 
     */
    std::string get_state_as_string() const;

    /**
     * fill up properties of the port
     * 
     * @author imarom (16-Sep-15)
     * 
     * @param driver 
     * @param speed 
     */
    void get_properties(std::string &driver, std::string &speed);


    /**
    * query for ownership
    * 
    */
    const std::string &get_owner() {
        return m_owner;
    }

    /**
     * owner handler 
     * for the connection 
     * 
     */
    const std::string &get_owner_handler() {
        return m_owner_handler;
    }


    bool verify_owner_handler(const std::string &handler) {

        return ( (m_owner != "none") && (m_owner_handler == handler) );

    }

    /**
     * encode stats as JSON
     */
    void encode_stats(Json::Value &port);

    uint8_t get_port_id() {
        return m_port_id;
    }

    /**
     * delegators
     * 
     */

    void add_stream(TrexStream *stream) {
        verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS);

        m_stream_table.add_stream(stream);

        change_state(PORT_STATE_STREAMS);
    }

    void remove_stream(TrexStream *stream) {
        verify_state(PORT_STATE_STREAMS);

        m_stream_table.remove_stream(stream);

        if (m_stream_table.size() == 0) {
            change_state(PORT_STATE_IDLE);
        }
    }

    void remove_and_delete_all_streams() {
        verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS);

        m_stream_table.remove_and_delete_all_streams();

        change_state(PORT_STATE_IDLE);
    }

    TrexStream * get_stream_by_id(uint32_t stream_id) {
        return m_stream_table.get_stream_by_id(stream_id);
    }

    void get_id_list(std::vector<uint32_t> &id_list) {
        m_stream_table.get_id_list(id_list);
    }

    void get_object_list(std::vector<TrexStream *> &object_list) {
        m_stream_table.get_object_list(object_list);
    }

    TrexDpPortEvents & get_dp_events() {
        return m_dp_events;
    }



private:

  

    /**
    * take ownership of the server array 
    * this is static 
    * ownership is total 
    * 
    */
    void set_owner(const std::string &owner) {
        m_owner = owner;
        m_owner_handler = generate_handler();
    }

    void clear_owner() {
        m_owner = "none";
        m_owner_handler = "";
    }

    bool is_free_to_aquire() {
        return (m_owner == "none");
    }


    const std::vector<int> get_core_id_list () {
        return m_cores_id_list;
    }

    bool verify_state(int state, bool should_throw = true) const;

    void change_state(port_state_e new_state);

    std::string generate_handler();

    void send_message_to_dp(TrexStatelessCpToDpMsgBase *msg);

    /**
     * triggered when event occurs
     * 
     */
    void on_dp_event_occured(TrexDpPortEvent::event_e event_type);


    TrexStreamTable    m_stream_table;
    uint8_t            m_port_id;
    port_state_e       m_port_state;
    std::string        m_owner;
    std::string        m_owner_handler;

    /* holds the DP cores associated with this port */
    std::vector<int> m_cores_id_list;

    bool               m_last_all_streams_continues;
    double             m_last_duration;

    TrexDpPortEvents   m_dp_events;
};

#endif /* __TREX_STATELESS_PORT_H__ */
