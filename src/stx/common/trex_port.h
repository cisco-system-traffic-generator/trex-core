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

#ifndef __TREX_PORT_H__
#define __TREX_PORT_H__

#include "internal_api/trex_platform_api.h"
#include "common/basic_utils.h"
#include "trex_dp_port_events.h"
#include "trex_port_attr.h"
#include "trex_stack_base.h"


class TrexPktBuffer;
class TrexCpToDpMsgBase;
class TrexCpToRxMsgBase;

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
 * describes a TRex (interactive) port
 *
 */
class TrexPort {
    friend TrexDpPortEvents;
    friend TrexDpPortEvent;

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
    
    TrexPort(uint8_t port_id);
    virtual ~TrexPort() {}
    
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

    
    uint8_t get_port_id() {
        return m_port_id;
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
     * get port speed in bits per second
     *
     */
    uint64_t get_port_speed_bps() const;

  
    TrexPortOwner & get_owner() {
        return m_owner;
    }


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

    /* stack related funcs */

    // get stack capabilities
    uint16_t get_stack_caps(void);

    // stack operations will not anger watchdog
    bool has_fast_stack(void);

    // get name of stack to print in error message
    std::string& get_stack_name(void);

    // run pending tasks of stack, need to poll results with ticket
    uint64_t run_rx_cfg_tasks_async(void);

    // same as above with ticket 0
    void run_rx_cfg_tasks_initial_async(void);

    // check if stack is running tasks
    bool is_rx_running_cfg_tasks(void);

    // get results of stack tasks
    // return false if results are deleted
    bool get_rx_cfg_tasks_results(uint64_t ticket_id, stack_result_t &results);

    // get port node to query ip/mac/vlan etc. info
    void get_port_node(CNodeBase &node);

    /**
     * configures port for L2 mode
     * 
     */
    void set_l2_mode_async(const std::string &dst_mac);
    
    /**
     * configures port in L3 mode
     * 
     */
    //void set_l3_mode(uint32_t src_ipv4, uint32_t dest_ipv4);
    void set_l3_mode_async(const std::string &src_ipv4, const std::string &dst_ipv4, const std::string *dst_mac);
    
    /**
     * configure VLAN
     */
    void set_vlan_cfg_async(const vlan_list_t &vlan_cfg);

    /**
     * invalidate dst MAC of port
     */
    void invalidate_dst_mac(void);

    // cancel rx config tasks
    void cancel_rx_cfg_tasks(void);

    /**
     * generate a JSON with MAC/IP/VLAN etc. of port
     */
    void port_attr_to_json(Json::Value &attr_res);

    /**
     * generate a JSON describing the status 
     * of the RX features 
     */
    void rx_features_to_json(Json::Value &feat_res);
    
    /**
     * return the port attribute object (speed, prom etc.)
     */
    TRexPortAttr *getPortAttrObj() {
        return get_platform_api().getPortAttrObj(m_port_id);
    }
    
    /**
     * simply a const version
     */
    const TRexPortAttr *getPortAttrObj() const {
        return get_platform_api().getPortAttrObj(m_port_id);
    }
    
    /**
     * get port source MAC
     * 
     */
    const uint8_t * get_src_mac() const;
    
    /**
     * get port dst MAC
     * 
     */
    const uint8_t * get_dst_mac() const;
    
    
    /**
     * is port active
     * 
     */
    bool is_active() const;

    
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
     * RX caps
     */
    uint16_t get_rx_caps() const {
        return m_rx_caps;
    }
    
    /**
     * encode stats of the port 
     * to JSON 
     */
    void encode_stats(Json::Value &port);
    
    
    /**
     * implemented by dervied
     * 
     */
    virtual bool is_service_mode_on() const = 0;

protected:
    
    /**
     * verify the state of the port for a command
     *  
     * if should_throw is true will throw exception 
     * in case the state is not valid 
     *  
     * the state is bitfield 
     */
    bool verify_state(int state, const char *cmd_name, bool should_throw = true) const;

    /**
     * change the state 
     * to a new state 
     */
    void change_state(port_state_e new_state);
    
    
    /**
     * return true if specific core is active 
     * (has yet to respond as done) 
     * 
     * @author imarom (9/4/2017)
     * 
     * @param core_id 
     * 
     * @return bool 
     */
    bool is_core_active(int core_id);

    /**
     * counts active cores
     */
    uint8_t get_active_cores_count(void);

    
    const std::vector<uint8_t> get_core_id_list () {
        return m_cores_id_list;
    }

    
    /**
     * send message to all cores using duplicate
     *
     */
    void send_message_to_all_dp(TrexCpToDpMsgBase *msg, bool send_to_active_only = false);

    /**
     * send message to specific DP core
     *
     */
    void send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg);

    /**
     * send message to specific RX core
     *
     */
    void send_message_to_rx(TrexCpToRxMsgBase *msg);

    // run RX core config tasks with given ticket ID
    void run_rx_cfg_tasks_internal_async(uint64_t ticket_id);

    
    
    /* port id */
    uint8_t                m_port_id;
    
    /* port state */
    port_state_e           m_port_state;
    
    /* holds the DP cores associated with this port */
    std::vector<uint8_t>   m_cores_id_list;

    TrexDpPortEvents       m_dp_events;
    int                    m_pending_async_stop_event;
    
    /* owner information */
    TrexPortOwner          m_owner;

    
    /* caching some RX info */
    uint16_t               m_rx_caps;
    uint16_t               m_rx_count_num;
    uint16_t               m_ip_id_base;
    uint16_t               m_stack_caps;
    bool                   m_synced_stack_caps;
};


#endif /* __TREX_PORT_H__ */

