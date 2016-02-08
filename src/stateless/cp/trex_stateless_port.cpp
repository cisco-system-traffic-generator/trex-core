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

#include <trex_stateless.h>
#include <trex_stateless_port.h>
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>

#include <string>

#ifndef TREX_RPC_MOCK_SERVER

// DPDK c++ issue 
#ifndef UINT8_MAX
    #define UINT8_MAX 255
#endif

#ifndef UINT16_MAX
    #define UINT16_MAX 0xFFFF
#endif

// DPDK c++ issue 
#endif

#include <rte_ethdev.h>
#include <os_time.h>

void
port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list);

using namespace std;

/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id, const TrexPlatformApi *api) {
    std::vector<std::pair<uint8_t, uint8_t>> core_pair_list;

    m_port_id = port_id;
    m_port_state = PORT_STATE_IDLE;

    /* get the platform specific data */
    api->get_interface_info(port_id, m_driver_name, m_speed);

    /* get the DP cores belonging to this port */
    api->port_id_to_cores(m_port_id, core_pair_list);

    for (auto core_pair : core_pair_list) {

        /* send the core id */
        m_cores_id_list.push_back(core_pair.first);
    }

    /* init the events DP DB */
    m_dp_events.create(this);

    m_graph_obj = NULL;
}

TrexStatelessPort::~TrexStatelessPort() {
    if (m_graph_obj) {
        delete m_graph_obj;
    }
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
TrexStatelessPort::acquire(const std::string &user, uint32_t session_id, bool force) {

    /* if port is free - just take it */
    if (get_owner().is_free()) {
        get_owner().own(user);
        return;
    }

    if (force) {
        get_owner().own(user);

        /* inform the other client of the steal... */
        Json::Value data;

        data["port_id"] = m_port_id;
        data["who"] = user;
        data["session_id"] = session_id;

        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_FORCE_ACQUIRED, data);

    } else {
        /* not same user or session id and not force - report error */
        if (get_owner().get_name() == user) {
            throw TrexException("port is already owned by another session of '" + user + "'");
        } else {
            throw TrexException("port is already taken by '" + get_owner().get_name() + "'");
        }
    }

}

void
TrexStatelessPort::release(void) {
    get_owner().release();
}

/**
 * starts the traffic on the port
 * 
 */
void
TrexStatelessPort::start_traffic(const TrexPortMultiplier &mul, double duration, bool force) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_STREAMS);

    /* just making sure no leftovers... */
    delete_streams_graph();

    /* on start - we can only provide absolute values */
    assert(mul.m_op == TrexPortMultiplier::OP_ABS);

    /* caclulate the effective factor for DP */
    double factor = calculate_effective_factor(mul, force);

    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);


    /* compiler it */
    std::vector<TrexStreamsCompiledObj *> compiled_objs;
    std::string fail_msg;

    TrexStreamsCompiler compiler;
    bool rc = compiler.compile(m_port_id,
                               streams,
                               compiled_objs,
                               get_dp_core_count(),
                               factor,
                               &fail_msg);
    if (!rc) {
        throw TrexException(fail_msg);
    }

    /* generate a message to all the relevant DP cores to start transmitting */
    int event_id = m_dp_events.generate_event_id();

    /* mark that DP event of stoppped is possible */
    m_dp_events.wait_for_event(TrexDpPortEvent::EVENT_STOP, event_id);


    /* update object status */
    m_factor = factor;
    m_last_all_streams_continues = compiled_objs[0]->get_all_streams_continues();
    m_last_duration = duration;
    change_state(PORT_STATE_TX);


    /* update the DP - messages will be freed by the DP */
    int index = 0;
    for (auto core_id : m_cores_id_list) {

        /* was the core assigned a compiled object ? */
        if (compiled_objs[index]) {
            TrexStatelessCpToDpMsgBase *start_msg = new TrexStatelessDpStart(m_port_id, event_id, compiled_objs[index], duration);
            send_message_to_dp(core_id, start_msg);
        } else {

            /* mimic an end event */
            m_dp_events.handle_event(TrexDpPortEvent::EVENT_STOP, core_id, event_id);
        }

        index++;
    }
    
    /* update subscribers */    
    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STARTED, data);
    
}


/**
 * stop traffic on port
 * 
 * @author imarom (09-Nov-15)
 * 
 * @return TrexStatelessPort::rc_e 
 */
void
TrexStatelessPort::stop_traffic(void) {

    if (!( (m_port_state == PORT_STATE_TX) 
        || (m_port_state == PORT_STATE_PAUSE) )) {
        return;
    }

    /* delete any previous graphs */
    delete_streams_graph();

    /* mask out the DP stop event */
    m_dp_events.disable(TrexDpPortEvent::EVENT_STOP);

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);

    send_message_to_all_dp(stop_msg);

    change_state(PORT_STATE_STREAMS);
    
    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STOPPED, data);

}

void
TrexStatelessPort::pause_traffic(void) {

    verify_state(PORT_STATE_TX);

    if (m_last_all_streams_continues == false) {
        throw TrexException(" pause is supported when all streams are in continues mode ");
    }

    if ( m_last_duration>0.0 ) {
        throw TrexException(" pause is supported when duration is not enable is start command ");
    }

    TrexStatelessCpToDpMsgBase *pause_msg = new TrexStatelessDpPause(m_port_id);

    send_message_to_all_dp(pause_msg);

    change_state(PORT_STATE_PAUSE);

    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_PAUSED, data);
}

void
TrexStatelessPort::resume_traffic(void) {

    verify_state(PORT_STATE_PAUSE);

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *resume_msg = new TrexStatelessDpResume(m_port_id);

    send_message_to_all_dp(resume_msg);

    change_state(PORT_STATE_TX);


    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RESUMED, data);
}

void
TrexStatelessPort::update_traffic(const TrexPortMultiplier &mul, bool force) {

    double factor;

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE);

    /* generate a message to all the relevant DP cores to start transmitting */
    double new_factor = calculate_effective_factor(mul, force);

    switch (mul.m_op) {
    case TrexPortMultiplier::OP_ABS:
        factor = new_factor / m_factor;
        break;

    case TrexPortMultiplier::OP_ADD:
        factor = (m_factor + new_factor) / m_factor;
        break;

    case TrexPortMultiplier::OP_SUB:
        factor = (m_factor - new_factor) / m_factor;
        if (factor <= 0) {
            throw TrexException("Update request will lower traffic to less than zero");
        }
        break;

    default:
        assert(0);
        break;
    }

    TrexStatelessCpToDpMsgBase *update_msg = new TrexStatelessDpUpdate(m_port_id, factor);

    send_message_to_all_dp(update_msg);

    m_factor *= factor;

}

std::string 
TrexStatelessPort::get_state_as_string() const {

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
    }

    return "UNKNOWN";
}

int
TrexStatelessPort::get_max_stream_id() const {
    return m_stream_table.get_max_stream_id();
}

void
TrexStatelessPort::get_properties(std::string &driver, TrexPlatformApi::driver_speed_e &speed) {

    driver = m_driver_name;
    speed  = m_speed;
}

bool
TrexStatelessPort::verify_state(int state, bool should_throw) const {
    if ( (state & m_port_state) == 0 ) {
        if (should_throw) {
            throw TrexException("command cannot be executed on current state: '" + get_state_as_string() + "'");
        } else {
            return false;
        }
    }

    return true;
}

void
TrexStatelessPort::change_state(port_state_e new_state) {

    m_port_state = new_state;
}


void
TrexStatelessPort::encode_stats(Json::Value &port) {

    const TrexPlatformApi *api = get_stateless_obj()->get_platform_api();

    TrexPlatformInterfaceStats stats;
    api->get_interface_stats(m_port_id, stats);

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
TrexStatelessPort::send_message_to_all_dp(TrexStatelessCpToDpMsgBase *msg) {

    for (auto core_id : m_cores_id_list) {
        send_message_to_dp(core_id, msg->clone());
    }

    /* original was not sent - delete it */
    delete msg;
}

void 
TrexStatelessPort::send_message_to_dp(uint8_t core_id, TrexStatelessCpToDpMsgBase *msg) {

    /* send the message to the core */
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    ring->Enqueue((CGenNode *)msg);
}

/**
 * when a DP (async) event occurs - handle it
 * 
 */
void 
TrexStatelessPort::on_dp_event_occured(TrexDpPortEvent::event_e event_type) {
    Json::Value data;

    switch (event_type) {

    case TrexDpPortEvent::EVENT_STOP:
        /* set a stop event */
        change_state(PORT_STATE_STREAMS);
        /* send a ZMQ event */

        data["port_id"] = m_port_id;
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_FINISHED_TX, data);
        break;

    default:
        assert(0);

    }
}

uint64_t
TrexStatelessPort::get_port_speed_bps() const {
    switch (m_speed) {
    case TrexPlatformApi::SPEED_1G:
        return (1LLU * 1000 * 1000 * 1000);

    case TrexPlatformApi::SPEED_10G:
        return (10LLU * 1000 * 1000 * 1000);

    case TrexPlatformApi::SPEED_40G:
        return (40LLU * 1000 * 1000 * 1000);

    default:
        return 0;
    }
}

static inline double
bps_to_gbps(double bps) {
    return (bps / (1000.0 * 1000 * 1000));
}

double
TrexStatelessPort::calculate_effective_factor(const TrexPortMultiplier &mul, bool force) {

    double factor = calculate_effective_factor_internal(mul);

    /* did we exceeded the max L1 line rate ? */
    double expected_l1_rate = factor * m_graph_obj->get_max_bps_l1();

    /* if not force and exceeded - throw exception */
    if ( (!force) && (expected_l1_rate > get_port_speed_bps()) ) {
        stringstream ss;
        ss << "Expected L1 B/W: '" << bps_to_gbps(expected_l1_rate) << " Gbps' exceeds port line rate: '" << bps_to_gbps(get_port_speed_bps()) << " Gbps'";
        throw TrexException(ss.str());
    }

    return factor;
}

double
TrexStatelessPort::calculate_effective_factor_internal(const TrexPortMultiplier &mul) {

    /* we now need the graph - generate it if we don't have it (happens once) */
    if (!m_graph_obj) {
        generate_streams_graph();
    }

    switch (mul.m_type) {

    case TrexPortMultiplier::MUL_FACTOR:
        return (mul.m_value);

    case TrexPortMultiplier::MUL_BPS:
        return (mul.m_value / m_graph_obj->get_max_bps_l2());

    case TrexPortMultiplier::MUL_PPS:
         return (mul.m_value / m_graph_obj->get_max_pps());

    case TrexPortMultiplier::MUL_PERCENTAGE:
        /* if abs percentage is from the line speed - otherwise its from the current speed */

        if (mul.m_op == TrexPortMultiplier::OP_ABS) {
            double required = (mul.m_value / 100.0) * get_port_speed_bps();
            return (required / m_graph_obj->get_max_bps_l1());
        } else {
            return (m_factor * (mul.m_value / 100.0));
        }

    default:
        assert(0);
    }

}


void
TrexStatelessPort::generate_streams_graph() {

    /* dispose of the old one */
    if (m_graph_obj) {
        delete_streams_graph();
    }

    /* fetch all the streams from the table */
    vector<TrexStream *> streams;
    get_object_list(streams);

    TrexStreamsGraph graph;
    m_graph_obj = graph.generate(streams);
}

void
TrexStatelessPort::delete_streams_graph() {
    if (m_graph_obj) {
        delete m_graph_obj;
        m_graph_obj = NULL;
    }
}



/***************************
 * port multiplier
 * 
 **************************/
const std::initializer_list<std::string> TrexPortMultiplier::g_types = {"raw", "bps", "pps", "percentage"};
const std::initializer_list<std::string> TrexPortMultiplier::g_ops   = {"abs", "add", "sub"};

TrexPortMultiplier::
TrexPortMultiplier(const std::string &type_str, const std::string &op_str, double value) {
    mul_type_e type;
    mul_op_e   op;

    if (type_str == "raw") {
        type = MUL_FACTOR; 

    } else if (type_str == "bps") {
        type = MUL_BPS;

    } else if (type_str == "pps") {
        type = MUL_PPS;

    } else if (type_str == "percentage") {
        type = MUL_PERCENTAGE;
    } else {
        throw TrexException("bad type str: " + type_str);
    }

    if (op_str == "abs") {
        op = OP_ABS;

    } else if (op_str == "add") {
        op = OP_ADD;

    } else if (op_str == "sub") {
        op = OP_SUB;

    } else {
        throw TrexException("bad op str: " + op_str);
    }

    m_type  = type;
    m_op    = op;
    m_value = value;

}

const TrexStreamsGraphObj *
TrexStatelessPort::validate(void) {

    /* first compile the graph */

    vector<TrexStream *> streams;
    get_object_list(streams);

    if (streams.size() == 0) {
        throw TrexException("no streams attached to port");
    }

    TrexStreamsCompiler compiler;
    std::vector<TrexStreamsCompiledObj *> compiled_objs;

    std::string fail_msg;
    bool rc = compiler.compile(m_port_id,
                               streams,
                               compiled_objs,
                               get_dp_core_count(),
                               1.0,
                               &fail_msg);
    if (!rc) {
        throw TrexException(fail_msg);
    }

    for (auto obj : compiled_objs) {
        delete obj;
    }

    /* now create a stream graph */
    if (!m_graph_obj) {
        generate_streams_graph();
    }

    return m_graph_obj;
}



void
TrexStatelessPort::get_port_effective_rate(uint64_t &bps, uint64_t &pps) {

    if (get_stream_count() == 0) {
        return;
    }

    if (!m_graph_obj) {
        generate_streams_graph();
    }

    bps = m_graph_obj->get_max_bps_l2() * m_factor;
    pps = m_graph_obj->get_max_pps() * m_factor;
}

/************* Trex Port Owner **************/

TrexPortOwner::TrexPortOwner() {
    m_is_free = true;

    /* for handlers random generation */
    srand(time(NULL));
}

/**
 * generate a random connection handler
 * 
 */
std::string 
TrexPortOwner::generate_handler() {
    std::stringstream ss;

    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    /* generate 8 bytes of random handler */
    for (int i = 0; i < 8; ++i) {
        ss << alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return (ss.str());
}

const std::string TrexPortOwner::g_unowned_name = "<FREE>";
const std::string TrexPortOwner::g_unowned_handler = "";
