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
        return  "IDLE";

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



/**
 * configures port in L2 mode
 * 
 */
void
TrexPort::set_l2_mode(const uint8_t *dest_mac) {
    
    /* not valid under traffic */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_l2_mode");
    
    /* configure port attributes for L2 */
    getPortAttrObj()->set_l2_mode(dest_mac);

    /* update RX core */
    TrexRxSetL2Mode *msg = new TrexRxSetL2Mode(m_port_id);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

/**
 * configures port in L3 mode - unresolved
 */
void
TrexPort::set_l3_mode(uint32_t src_ipv4, uint32_t dest_ipv4) {
    
    /* not valid under traffic */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_l3_mode");
    
    /* configure port attributes with L3 */
    getPortAttrObj()->set_l3_mode(src_ipv4, dest_ipv4);

    /* send RX core the relevant info */
    CManyIPInfo ip_info;
    ip_info.insert(COneIPv4Info(src_ipv4, 0, getPortAttrObj()->get_layer_cfg().get_ether().get_src()));
    
    TrexRxSetL3Mode *msg = new TrexRxSetL3Mode(m_port_id, ip_info, false);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}

/**
 * configures port in L3 mode - resolved
 * 
 */
void
TrexPort::set_l3_mode(uint32_t src_ipv4, uint32_t dest_ipv4, const uint8_t *resolved_mac) {
    
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_l3_mode");
    
    /* configure port attributes with L3 */
    getPortAttrObj()->set_l3_mode(src_ipv4, dest_ipv4, resolved_mac);
    
    /* send RX core the relevant info */
    CManyIPInfo ip_info;
    ip_info.insert(COneIPv4Info(src_ipv4, 0, getPortAttrObj()->get_layer_cfg().get_ether().get_src()));
    
    bool is_grat_arp_needed = !getPortAttrObj()->is_loopback();
    
    TrexRxSetL3Mode *msg = new TrexRxSetL3Mode(m_port_id, ip_info, is_grat_arp_needed);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}


/**
 * configures VLAN tagging
 * 
 */
void
TrexPort::set_vlan_cfg(const VLANConfig &vlan_cfg) {
    
    /* not valid under traffic */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "set_vlan_cfg");
    
    /* configure VLAN on port attribute object */
    getPortAttrObj()->set_vlan_cfg(vlan_cfg);
    
    TrexRxSetVLAN *msg = new TrexRxSetVLAN(m_port_id, vlan_cfg);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );
}



Json::Value
TrexPort::rx_features_to_json() {
    static MsgReply<Json::Value> reply;
    
    reply.reset();

    TrexRxFeaturesToJson *msg = new TrexRxFeaturesToJson(m_port_id, reply);
    send_message_to_rx( (TrexCpToRxMsgBase *)msg );

    return reply.wait_for_reply();
}


const uint8_t *
TrexPort::get_src_mac() const {
    return getPortAttrObj()->get_layer_cfg().get_ether().get_src();
}

const uint8_t *
TrexPort::get_dst_mac() const {
    return getPortAttrObj()->get_layer_cfg().get_ether().get_dst();
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

