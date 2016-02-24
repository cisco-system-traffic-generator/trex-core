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
#include <internal_api/trex_platform_api.h>

class TrexStatelessCpToDpMsgBase;
class TrexStreamsGraphObj;
class TrexPortMultiplier;

/** 
 * TRex port owner can perform
 * write commands
 * while port is owned - others can
 * do read only commands
 * 
 */
class TrexPortOwner {
public:

    TrexPortOwner();

    /**
     * is port free to acquire
     */
    bool is_free() {
        return m_is_free;
    }

    void release() {
        m_is_free = true;
        m_owner_name = "";
        m_handler = "";
    }

    bool is_owned_by(const std::string &user) {
        return ( !m_is_free && (m_owner_name == user) );
    }

    void own(const std::string &owner_name) {

        /* save user data */
        m_owner_name = owner_name;

        /* internal data */
        m_handler = generate_handler();
        m_is_free = false;
    }

    bool verify(const std::string &handler) {
        return ( (!m_is_free) && (m_handler == handler) );
    }

    const std::string &get_name() {
        return (!m_is_free ? m_owner_name : g_unowned_name);
    }

    const std::string &get_handler() {
        return (!m_is_free ? m_handler : g_unowned_handler);
    }


private:
    std::string  generate_handler();

    /* is this port owned by someone ? */
    bool         m_is_free;

    /* user provided info */
    std::string  m_owner_name;

    /* handler genereated internally */
    std::string  m_handler;
    
    /* seed for generating random values */
    unsigned int m_seed;

    /* just references defaults... */
    static const std::string g_unowned_name;
    static const std::string g_unowned_handler;
};


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
    
    ~TrexStatelessPort();

    /**
     * acquire port
     * throws TrexException in case of an error
     */
    void acquire(const std::string &user, uint32_t session_id, bool force = false);

    /**
     * release the port from the current user
     * throws TrexException in case of an error
     */
    void release(void);

    /**
     * validate the state of the port before start 
     * it will return a stream graph 
     * containing information about the streams 
     * configured on this port 
     *  
     * on error it throws TrexException
     */
    const TrexStreamsGraphObj *validate(void);

    /**
     * start traffic
     * throws TrexException in case of an error
     */
    void start_traffic(const TrexPortMultiplier &mul, double duration, bool force = false);

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
    void update_traffic(const TrexPortMultiplier &mul, bool force);

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
     * the the max stream id currently assigned
     * 
     */
    int get_max_stream_id() const;

    /**
     * fill up properties of the port
     * 
     * @author imarom (16-Sep-15)
     * 
     * @param driver 
     * @param speed 
     */
    void get_properties(std::string &driver, TrexPlatformApi::driver_speed_e &speed);



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
        delete_streams_graph();

        change_state(PORT_STATE_STREAMS);
    }

    void remove_stream(TrexStream *stream) {
        verify_state(PORT_STATE_STREAMS);

        m_stream_table.remove_stream(stream);
        delete_streams_graph();

        if (m_stream_table.size() == 0) {
            change_state(PORT_STATE_IDLE);
        }
    }

    void remove_and_delete_all_streams() {
        verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS);

        m_stream_table.remove_and_delete_all_streams();
        delete_streams_graph();

        change_state(PORT_STATE_IDLE);
    }

    TrexStream * get_stream_by_id(uint32_t stream_id) {
        return m_stream_table.get_stream_by_id(stream_id);
    }

    int get_stream_count() {
        return m_stream_table.size();
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


    /**
     * returns the number of DP cores linked to this port
     * 
     */
    uint8_t get_dp_core_count() {
        return m_cores_id_list.size();
    }

    /**
     * returns the traffic multiplier currently being used by the DP
     * 
     */
    double get_multiplier() {
        return (m_factor);
    }

    /**
     * get port speed in bits per second
     * 
     */
    uint64_t get_port_speed_bps() const;

    /**
     * return true if port adds CRC to a packet (not occurs for 
     * VNICs) 
     * 
     * @author imarom (24-Feb-16)
     * 
     * @return bool 
     */
    bool has_crc_added() const {
        return m_has_crc;
    }

    TrexPortOwner & get_owner() {
        return m_owner;
    }


    /**
     * get the port effective rate (on a started / paused port)
     * 
     * @author imarom (07-Jan-16)
     * 
     */
    void get_port_effective_rate(double &pps,
                                 double &bps_L1,
                                 double &bps_L2,
                                 double &percentage);

private:


    const std::vector<int> get_core_id_list () {
        return m_cores_id_list;
    }

    bool verify_state(int state, bool should_throw = true) const;

    void change_state(port_state_e new_state);

    std::string generate_handler();

    /**
     * send message to all cores using duplicate
     * 
     */
    void send_message_to_all_dp(TrexStatelessCpToDpMsgBase *msg);

    /**
     * send message to specific DP core
     * 
     */
    void send_message_to_dp(uint8_t core_id, TrexStatelessCpToDpMsgBase *msg);

    /**
     * triggered when event occurs
     * 
     */
    void on_dp_event_occured(TrexDpPortEvent::event_e event_type);


    /**
     * calculate effective M per core
     * 
     */
    double calculate_effective_factor(const TrexPortMultiplier &mul, bool force = false);
    double calculate_effective_factor_internal(const TrexPortMultiplier &mul);
  

    /**
     * generates a graph of streams graph
     * 
     */
    void generate_streams_graph();

    /**
     * dispose of it
     * 
     * @author imarom (26-Nov-15)
     */
    void delete_streams_graph();


    TrexStreamTable    m_stream_table;
    uint8_t            m_port_id;
    port_state_e       m_port_state;
    std::string        m_driver_name;
    bool               m_has_crc;

    TrexPlatformApi::driver_speed_e m_speed;

    /* holds the DP cores associated with this port */
    std::vector<int>   m_cores_id_list;

    bool               m_last_all_streams_continues;
    double             m_last_duration;
    double             m_factor;

    TrexDpPortEvents   m_dp_events;

    /* holds a graph of streams rate*/
    const TrexStreamsGraphObj  *m_graph_obj;

    /* owner information */
    TrexPortOwner       m_owner;
};


/**
 * port multiplier object
 * 
 */
class TrexPortMultiplier {
public:


    /**
     * defines the type of multipler passed to start
     */
    enum mul_type_e {
        MUL_FACTOR,
        MUL_BPS,
        MUL_PPS,
        MUL_PERCENTAGE
    };

    /**
     * multiplier can be absolute value 
     * increment value or subtract value 
     */
    enum mul_op_e {
        OP_ABS,
        OP_ADD,
        OP_SUB
    };


    TrexPortMultiplier(mul_type_e type, mul_op_e op, double value) {
        m_type   = type;
        m_op     = op;
        m_value  = value;
    }

    TrexPortMultiplier(const std::string &type_str, const std::string &op_str, double value);


public:
    static const std::initializer_list<std::string> g_types;
    static const std::initializer_list<std::string> g_ops;

    mul_type_e   m_type;
    mul_op_e     m_op;
    double       m_value;
};

#endif /* __TREX_STATELESS_PORT_H__ */
