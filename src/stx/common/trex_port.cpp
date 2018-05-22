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

#include "trex_port.h"
#include "trex_stx.h"
#include "trex_exception.h"
#include "trex_messaging.h"
#include "msg_manager.h"
#include "publisher/trex_publisher.h"


TrexPort::TrexPort(uint8_t port_id) : m_dp_events(this) {
    std::vector<std::pair<uint8_t, uint8_t>> core_pair_list;

    m_port_id             = port_id;
    m_port_state          = PORT_STATE_IDLE;
    m_synced_stack_caps   = false;
    
    /* query RX info from driver */
    get_platform_api().get_port_stat_info(port_id, m_rx_count_num, m_rx_caps, m_ip_id_base);
        
    /* get the DP cores belonging to this port */
    get_platform_api().port_id_to_cores(m_port_id, core_pair_list);

    for (auto core_pair : core_pair_list) {

        /* send the core id */
        m_cores_id_list.push_back(core_pair.first);
    }

    m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
}


/**
 * acquire the port
 * 
 * @author imarom (09-Nov-15)
 * 
 * @param user 
 * @param force 
 */
void 
TrexPort::acquire(const std::string &user, uint32_t session_id, bool force) {

    bool used_force = !get_owner().is_free() && force;

    if (get_owner().is_free() || force) {
        get_owner().own(user, session_id);
        cancel_rx_cfg_tasks();

    } else {
        /* not same user or session id and not force - report error */
        if (get_owner().get_name() == user) {
            throw TrexException("port is already owned by another session of '" + user + "'");
        } else {
            throw TrexException("port is already taken by '" + get_owner().get_name() + "'");
        }
    }

    Json::Value data;

    data["port_id"]    = m_port_id;
    data["who"]        = user;
    data["session_id"] = session_id;
    data["force"]      = used_force;

    get_stx()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_ACQUIRED, data);

}

void
TrexPort::release(void) {
    

    Json::Value data;

    data["port_id"]    = m_port_id;
    data["who"]        = get_owner().get_name();
    data["session_id"] = get_owner().get_session_id();

    get_owner().release();
    cancel_rx_cfg_tasks();

    get_stx()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RELEASED, data);
}


void 
TrexPort::start_rx_queue(uint64_t size) {
    static MsgReply<bool> reply;
    
    reply.reset();
    
    TrexRxStartQueue *msg = new TrexRxStartQueue(m_port_id, size, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    
    /* we cannot return ACK to the user until the RX core has approved
       this might cause the user to lose some packets from the queue
     */
    reply.wait_for_reply();
}


void
TrexPort::stop_rx_queue() {
    TrexCpToRxMsgBase *msg = new TrexRxStopQueue(m_port_id);
    send_message_to_rx(msg);
}


const TrexPktBuffer *
TrexPort::get_rx_queue_pkts() {
    static MsgReply<const TrexPktBuffer *> reply;
    
    reply.reset();

    TrexRxQueueGetPkts *msg = new TrexRxQueueGetPkts(m_port_id, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );

    return reply.wait_for_reply();
}



bool TrexPort::is_active() const {
    return   (  (m_port_state == PORT_STATE_TX) 
             || (m_port_state == PORT_STATE_PAUSE)
             || (m_port_state == PORT_STATE_PCAP_TX)
             );
}


bool
TrexPort::verify_state(int state, const char *cmd_name, bool should_throw) const {
    if ( (state & m_port_state) == 0 ) {
        if (should_throw) {
            std::stringstream ss;
            ss << "command '" << cmd_name << "' cannot be executed on current state: '" << get_state_as_string() << "'";
            throw TrexException(ss.str());
        } else {
            return false;
        }
    }

    return true;
}

void
TrexPort::change_state(port_state_e new_state) {

    m_port_state = new_state;
}


/**
 * core is considered active if it has a pending for async stop
 * 
 */
bool
TrexPort::is_core_active(int core_id) {
    return ( (m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID) &&
             (m_dp_events.is_core_pending_on_event(m_pending_async_stop_event, core_id))
           );
}

/**
 * returns port state formatted as string
 * 
 */
std::string 
TrexPort::get_state_as_string() const {

    switch (get_state()) {
    case PORT_STATE_DOWN:
        return "DOWN";

    case PORT_STATE_IDLE:
        return "IDLE";

    case PORT_STATE_STREAMS:
        return "STREAMS";

    case PORT_STATE_TX:
        return "TX";

    case PORT_STATE_PAUSE:
        return "PAUSE";

    case PORT_STATE_PCAP_TX:
        return "PCAP_TX";
    }

    return "UNKNOWN";
}



void
TrexPort::encode_stats(Json::Value &port) {

    TrexPlatformInterfaceStats stats;
    get_platform_api().get_port_stats(m_port_id, stats);

    port["tx_bps"]          = stats.m_stats.m_tx_bps;
    port["rx_bps"]          = stats.m_stats.m_rx_bps;

    port["tx_pps"]          = stats.m_stats.m_tx_pps;
    port["rx_pps"]          = stats.m_stats.m_rx_pps;

    port["total_tx_pkts"]   = Json::Value::UInt64(stats.m_stats.m_total_tx_pkts);
    port["total_rx_pkts"]   = Json::Value::UInt64(stats.m_stats.m_total_rx_pkts);

    port["total_tx_bytes"]  = Json::Value::UInt64(stats.m_stats.m_total_tx_bytes);
    port["total_rx_bytes"]  = Json::Value::UInt64(stats.m_stats.m_total_rx_bytes);
    
    port["tx_rx_errors"]    = Json::Value::UInt64(stats.m_stats.m_tx_rx_errors);
}


void 
TrexPort::send_message_to_all_dp(TrexCpToDpMsgBase *msg, bool send_to_active_only) {

    for (auto core_id : m_cores_id_list) {

        /* skip non active cores if requested */
        if ( (send_to_active_only) && (!is_core_active(core_id)) ) {
            continue;
        }

        send_message_to_dp(core_id, msg->clone());
    }

    /* original was not sent - delete it */
    delete msg;
}

uint8_t
TrexPort::get_active_cores_count(void) {

    uint8_t active_count = 0;
    for (uint8_t &core_id : m_cores_id_list) {
        if ( is_core_active(core_id) ) {
            active_count++;
        }
    }
    return active_count;
}


void 
TrexPort::send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg) {

    /* send the message to the core */
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    ring->Enqueue((CGenNode *)msg);
}


void
TrexPort::send_message_to_rx(TrexCpToRxMsgBase *msg) {

    /* send the message to the core */
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->Enqueue((CGenNode *)msg);
}


uint64_t
TrexPort::get_port_speed_bps() const {
    return (uint64_t) get_platform_api().getPortAttrObj(m_port_id)->get_link_speed() * 1000 * 1000;
}

uint16_t TrexPort::get_stack_caps(void) {
    if ( !m_synced_stack_caps ) {
        static MsgReply<uint16_t> reply;
        reply.reset();
        TrexRxGetStackCaps *msg = new TrexRxGetStackCaps(m_port_id, reply);
        send_message_to_rx( (TrexCpToRxMsgBase *)msg );
        m_stack_caps = reply.wait_for_reply();
    }
    return m_stack_caps;
}

bool TrexPort::has_fast_stack(void) {
    return get_stack_caps() & CStackBase::FAST_OPS;
}

std::string& TrexPort::get_stack_name(void) {
    return CGlobalInfo::m_options.m_stack_type;
}

void TrexPort::get_port_node(CNodeBase &node) {
    static MsgReply<bool> reply;
    reply.reset();
    std::string err;
    TrexRxGetPortNode *msg = new TrexRxGetPortNode(m_port_id, node, err, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    reply.wait_for_reply();
    if ( err.size() ) {
        throw TrexException("Could not get interface information, error: " + err);
    }
}

/**
 * configures port in L2 mode
 * 
 */
void TrexPort::set_l2_mode_async(const std::string &dst_mac) {
    /* not valid under traffic */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_l2_mode");

    uint8_t *dp_dst_mac = CGlobalInfo::m_options.m_mac_addr[m_port_id].u.m_mac.dest;
    memcpy(dp_dst_mac, dst_mac.c_str(), 6);

    /* update RX core */
    TrexRxSetL2Mode *msg = new TrexRxSetL2Mode(m_port_id, dst_mac);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

/**
 * configures port in L3 mode
 * 
 */
void TrexPort::set_l3_mode_async(const std::string &src_ipv4, const std::string &dst_ipv4, const std::string *dst_mac) {
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_l3_mode");

    if ( dst_mac != nullptr ) {
        uint8_t *dp_dst_mac = CGlobalInfo::m_options.m_mac_addr[m_port_id].u.m_mac.dest;
        memcpy(dp_dst_mac, dst_mac->c_str(), 6);
    }

    TrexRxSetL3Mode *msg = new TrexRxSetL3Mode(m_port_id, src_ipv4, dst_ipv4, dst_mac);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

/**
 * configures IPv6 of port
 * 
 */
void TrexPort::conf_ipv6_async(bool enabled, const std::string &src_ipv6) {
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "conf_ipv6");
    TrexRxConfIPv6 *msg = new TrexRxConfIPv6(m_port_id, enabled, src_ipv6);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

void TrexPort::invalidate_dst_mac(void) {
    TrexRxInvalidateDstMac *msg = new TrexRxInvalidateDstMac(m_port_id);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

/**
 * configures VLAN tagging
 * 
 */
void TrexPort::set_vlan_cfg_async(const vlan_list_t &vlan_list) {
    /* not valid under traffic */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_vlan_cfg");

    TrexRxSetVLAN *msg = new TrexRxSetVLAN(m_port_id, vlan_list);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

void TrexPort::run_rx_cfg_tasks_initial_async(void) {
    /* valid at startup of server */
    verify_state(PORT_STATE_IDLE, "run_rx_cfg_tasks_initial");
    TrexPort::run_rx_cfg_tasks_internal_async(0);
}

uint64_t TrexPort::run_rx_cfg_tasks_async(void) {
    /* valid at startup of server */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "run_rx_cfg_tasks");

    uint64_t ticket_id = get_stx()->get_ticket();
    TrexPort::run_rx_cfg_tasks_internal_async(ticket_id);
    return ticket_id;
}

void TrexPort::run_rx_cfg_tasks_internal_async(uint64_t ticket_id) {
    TrexRxRunCfgTasks *msg = new TrexRxRunCfgTasks(m_port_id, ticket_id);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

bool TrexPort::is_rx_running_cfg_tasks(void) {
    if ( !get_stx()->get_rx()->is_active() ) {
        return false;
    }
    if ( has_fast_stack() ) {
        return false;
    }
    static MsgReply<bool> reply;
    reply.reset();
    TrexRxIsRunningTasks *msg = new TrexRxIsRunningTasks(m_port_id, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    return reply.wait_for_reply();
}

bool TrexPort::get_rx_cfg_tasks_results(uint64_t ticket_id, stack_result_t &results) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexRxGetTasksResults *msg = new TrexRxGetTasksResults(m_port_id, ticket_id, results, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    return reply.wait_for_reply();
}

void TrexPort::cancel_rx_cfg_tasks(void) {
    TrexRxCancelCfgTasks *msg = new TrexRxCancelCfgTasks(m_port_id);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

void TrexPort::port_attr_to_json(Json::Value &attr_res) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexPortAttrToJson *msg = new TrexPortAttrToJson(m_port_id, attr_res, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    reply.wait_for_reply();
}

void TrexPort::rx_features_to_json(Json::Value &feat_res) {
    static MsgReply<bool> reply;
    reply.reset();
    TrexRxFeaturesToJson *msg = new TrexRxFeaturesToJson(m_port_id, feat_res, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
    reply.wait_for_reply();
}



/************* Trex Port Owner **************/

TrexPortOwner::TrexPortOwner() {
    m_is_free = true;
    m_session_id = 0;

    /* for handlers random generation */
    m_seed = time(NULL);
}

const std::string TrexPortOwner::g_unowned_name = "<FREE>";
const std::string TrexPortOwner::g_unowned_handler = "";

