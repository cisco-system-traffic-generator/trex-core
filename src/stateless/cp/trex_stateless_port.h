/*
 Itay Marom
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
#ifndef __TREX_STATELESS_PORT_H__
#define __TREX_STATELESS_PORT_H__

#include "common/basic_utils.h"
#include "internal_api/trex_platform_api.h"
#include "trex_dp_port_events.h"
#include "trex_stateless_rx_defs.h"
#include "trex_stream.h"
#include "trex_exception.h"
#include "trex_stateless_capture.h"

class TrexStatelessCpToDpMsgBase;
class TrexStatelessCpToRxMsgBase;
class TrexStreamsGraphObj;
class TrexPortMultiplier;
class TrexPktBuffer;


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
        m_session_id = 0;
    }

    bool is_owned_by(const std::string &user) {
        return ( !m_is_free && (m_owner_name == user) );
    }

    void own(const std::string &owner_name, uint32_t session_id) {

        /* save user data */
        m_owner_name = owner_name;
        m_session_id = session_id;

        /* internal data */
        m_handler = utl_generate_random_str(m_seed, 8);
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

    const uint32_t get_session_id() {
        return m_session_id;
    }

private:

    /* is this port owned by someone ? */
    bool         m_is_free;

    /* user provided info */
    std::string  m_owner_name;

    /* which session of the user holds this port*/
    uint32_t     m_session_id;

    /* handler genereated internally */
    std::string  m_handler;

    /* seed for generating random values */
    unsigned int m_seed;

    /* just references defaults... */
    static const std::string g_unowned_name;
    static const std::string g_unowned_handler;
};

/**
 * enforces in/out from service mode
 * 
 * @author imarom (1/4/2017)
 */
class TrexServiceMode {
public:
    TrexServiceMode(uint8_t port_id, const TrexPlatformApi *api) {
        m_is_enabled   = false;
        m_has_rx_queue = false;
        m_port_id      = port_id;
        m_port_attr    = api->getPortAttrObj(port_id);
    }

    void enable() {
        m_port_attr->set_rx_filter_mode(RX_FILTER_MODE_ALL);
        m_is_enabled = true;
    }

    void disable() {
        if (m_has_rx_queue) {
            throw TrexException("unable to disable service mode - please remove RX queue");
        }

        if (TrexStatelessCaptureMngr::getInstance().is_active(m_port_id)) {
            throw TrexException("unable to disable service mode - an active capture on port " + std::to_string(m_port_id) + " exists");
        }
        
        m_port_attr->set_rx_filter_mode(RX_FILTER_MODE_HW);
        m_is_enabled = false;
    }

    bool is_enabled() const {
        return m_is_enabled;
    }

    void set_rx_queue() {
        m_has_rx_queue = true;
    }

    void unset_rx_queue() {
        m_has_rx_queue = false;
    }

private:
    bool            m_is_enabled;
    bool            m_has_rx_queue;
    TRexPortAttr   *m_port_attr;
    uint8_t         m_port_id;
};

class AsyncStopEvent;

/**
 * describes a stateless port
 *
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort {
    friend TrexDpPortEvents;
    friend TrexDpPortEvent;
    friend AsyncStopEvent;

public:

    /**
     * port state
     */
    enum port_state_e {
        PORT_STATE_DOWN     = 0x1,
        PORT_STATE_IDLE     = 0x2,
        PORT_STATE_STREAMS  = 0x4,
        PORT_STATE_TX       = 0x8,
        PORT_STATE_PAUSE    = 0x10,
        PORT_STATE_PCAP_TX  = 0x20,
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

    /**
     * port capture mode
     */
    enum capture_mode_e {
        PORT_CAPTURE_NONE = 0,
        PORT_CAPTURE_RX,
        PORT_CAPTURE_ALL
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
    void start_traffic(const TrexPortMultiplier &mul, double duration, bool force = false, uint64_t core_mask = UINT64_MAX);

    /**
     * stop traffic
     * throws TrexException in case of an error
     */
    void stop_traffic(void);

    /**
     * remove all RX filters 
     * valid only when port is stopped 
     * 
     * @author imarom (28-Mar-16)
     */
    void remove_rx_filters(void);

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
     * push a PCAP file onto the port
     * 
     */
    void push_remote(const std::string &pcap_filename,
                     double            ipg_usec,
                     double            min_ipg_sec,
                     double            speedup,
                     uint32_t          count,
                     double            duration,
                     bool              is_dual);

    /** 
     * moves port to / out service mode 
     */
    void set_service_mode(bool enabled) {
        if (enabled) {
            m_service_mode.enable();
        } else {
            m_service_mode.disable();
        }
    }
    bool is_service_mode_on() const {
        return m_service_mode.is_enabled();
    }
    
    /**
     * get the port state
     *
     */
    port_state_e get_state() const {
        return m_port_state;
    }

    /**
     * return true if the port is active
     * (paused is considered active)
     */
    bool is_active() const;

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
     */
    void get_properties(std::string &driver);

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

    void add_stream(TrexStream *stream);
    void remove_stream(TrexStream *stream);
    void remove_and_delete_all_streams();

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
     * return RX caps
     *
     */
    int get_rx_caps() const {
        return m_rx_caps;
    }

    uint16_t get_rx_count_num() const {
        return m_rx_count_num;
    }

    /**
     * return true if port adds CRC to a packet (not occurs for
     * VNICs)
     *
     * @author imarom (24-Feb-16)
     *
     * @return bool
     */
    bool has_crc_added() const {
        return m_api_info.has_crc;
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

    void get_pci_info(std::string &pci_addr, int &numa_node);

    /**
     * start RX queueing of packets
     * 
     * @author imarom (11/7/2016)
     * 
     * @param limit 
     */
    void start_rx_queue(uint64_t limit);

    /**
     * stop RX queueing
     * 
     * @author imarom (11/7/2016)
     */
    void stop_rx_queue();

    /**
     * fetch the RX queue packets from the queue
     * 
     */
    const TrexPktBuffer *get_rx_queue_pkts();

    /**
     * configures port for L2 mode
     * 
     */
    void set_l2_mode(const uint8_t *dest_mac);
    
    /**
     * configures port in L3 mode
     * 
     */
    void set_l3_mode(uint32_t src_ipv4, uint32_t dest_ipv4);
    void set_l3_mode(uint32_t src_ipv4, uint32_t dest_ipv4, const uint8_t *resolved_mac);
    
    /**
     * generate a JSON describing the status 
     * of the RX features 
     * 
     */
    Json::Value rx_features_to_json();
    
    /**
     * return the port attribute object
     * 
     */
    TRexPortAttr *getPortAttrObj() {
        return m_platform_api->getPortAttrObj(m_port_id);
    }
    
private:
    void set_service_mode_on();
    void set_service_mode_off();
    
    bool is_core_active(int core_id);

    const std::vector<uint8_t> get_core_id_list () {
        return m_cores_id_list;
    }

    bool verify_state(int state, const char *cmd_name, bool should_throw = true) const;

    void change_state(port_state_e new_state);

    std::string generate_handler();

    /**
     * send message to all cores using duplicate
     *
     */
    void send_message_to_all_dp(TrexStatelessCpToDpMsgBase *msg, bool send_to_active_only = false);

    /**
     * send message to specific DP core
     *
     */
    void send_message_to_dp(uint8_t core_id, TrexStatelessCpToDpMsgBase *msg);

    /**
     * send message to specific RX core
     *
     */
    void send_message_to_rx(TrexStatelessCpToRxMsgBase *msg);
    
    /**
     * when a port stops, perform various actions
     *
     */
    void common_port_stop_actions(bool async);

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

    TrexPlatformApi::intf_info_st m_api_info;
    const TrexPlatformApi *m_platform_api;

    uint16_t           m_rx_count_num;
    uint16_t           m_rx_caps;

    /* holds the DP cores associated with this port */
    std::vector<uint8_t>   m_cores_id_list;

    bool               m_last_all_streams_continues;
    double             m_last_duration;
    double             m_factor;

    TrexDpPortEvents   m_dp_events;

    /* holds a graph of streams rate*/
    const TrexStreamsGraphObj  *m_graph_obj;

    /* owner information */
    TrexPortOwner       m_owner;

    int m_pending_async_stop_event;
    
    TrexServiceMode m_service_mode;

    static const uint32_t MAX_STREAMS = 20000;

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
        MUL_BPSL1,
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

    mul_type_e             m_type;
    mul_op_e               m_op;
    double                 m_value;
};

#endif /* __TREX_STATELESS_PORT_H__ */
