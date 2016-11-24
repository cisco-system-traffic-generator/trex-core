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
#include <common/basic_utils.h>
#include <common/captureFile.h>
#include "trex_stateless_rx_defs.h"

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

#include <os_time.h>

void
port_id_to_cores(uint8_t port_id, std::vector<std::pair<uint8_t, uint8_t>> &cores_id_list);

using namespace std;



/***************************
 * trex DP events handlers
 * 
 **************************/
class AsyncStopEvent : public TrexDpPortEvent {

protected:
    /**
     * when an async event occurs (all cores have reported in)
     * 
     * @author imarom (29-Feb-16)
     */
    virtual void on_event() {
        get_port()->change_state(TrexStatelessPort::PORT_STATE_STREAMS);

        get_port()->common_port_stop_actions(true);

        assert(get_port()->m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID);
        get_port()->m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
    }

    /**
     * when a DP core encountered an error
     * 
     * @author imarom (20-Apr-16)
     */
    virtual void on_error(int thread_id) {
        Json::Value data;

        data["port_id"]   = get_port()->get_port_id();
        data["thread_id"] = thread_id;

        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_ERROR, data);
    }
};

/*************************************
 * Streams Feeder 
 * A class that holds a temporary 
 * clone of streams that can be 
 * manipulated 
 *  
 * this is a RAII object meant for 
 * graceful cleanup 
 ************************************/
class StreamsFeeder {
public:
    StreamsFeeder(TrexStatelessPort *port) {

        /* start pesimistic */
        m_success = false;

        /* fetch the original streams */
        port->get_object_list(m_in_streams);

        for (const TrexStream *in_stream : m_in_streams) {
            TrexStream *out_stream = in_stream->clone(true);

            get_stateless_obj()->m_rx_flow_stat.start_stream(out_stream);

            m_out_streams.push_back(out_stream);
        }
    }

    void set_status(bool status) {
        m_success = status;
    }

    vector<TrexStream *> &get_streams() {
        return m_out_streams;
    }

    /**
     * RAII
     */
    ~StreamsFeeder() {
        for (int i = 0; i < m_out_streams.size(); i++) {
            TrexStream *out_stream = m_out_streams[i];
            TrexStream *in_stream  = m_in_streams[i];

            if (m_success) {
                /* success path */
                get_stateless_obj()->m_rx_flow_stat.copy_state(out_stream, in_stream);
            } else {
                /* fail path */
                get_stateless_obj()->m_rx_flow_stat.stop_stream(out_stream);
            }
            delete out_stream;
        }
    }

private:
    vector<TrexStream *>  m_in_streams;
    vector<TrexStream *>  m_out_streams;
    bool                  m_success;
};


/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id, const TrexPlatformApi *api) : m_dp_events(this) {
    std::vector<std::pair<uint8_t, uint8_t>> core_pair_list;

    m_port_id            = port_id;
    m_port_state         = PORT_STATE_IDLE;
    m_platform_api       = api;

    /* get the platform specific data */
    api->get_interface_info(port_id, m_api_info);

    /* get RX caps */
    api->get_interface_stat_info(port_id, m_rx_count_num, m_rx_caps);

    /* get the DP cores belonging to this port */
    api->port_id_to_cores(m_port_id, core_pair_list);

    for (auto core_pair : core_pair_list) {

        /* send the core id */
        m_cores_id_list.push_back(core_pair.first);
    }

    m_graph_obj = NULL;

    m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
}

TrexStatelessPort::~TrexStatelessPort() {

    stop_traffic();
    remove_and_delete_all_streams();
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

    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_ACQUIRED, data);

}

void
TrexStatelessPort::release(void) {
    

    Json::Value data;

    data["port_id"]    = m_port_id;
    data["who"]        = get_owner().get_name();
    data["session_id"] = get_owner().get_session_id();

    get_owner().release();

    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RELEASED, data);
}

/**
 * starts the traffic on the port
 * 
 */
void
TrexStatelessPort::start_traffic(const TrexPortMultiplier &mul, double duration, bool force, uint64_t core_mask) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_STREAMS, "start");

    /* just making sure no leftovers... */
    delete_streams_graph();

    /* on start - we can only provide absolute values */
    assert(mul.m_op == TrexPortMultiplier::OP_ABS);

    /* check link state */
    if ( !m_platform_api->getPortAttrObj(m_port_id)->is_link_up() && !force ) {
        throw TrexException("Link state is DOWN.");
    }

    /* caclulate the effective factor for DP */
    double factor = calculate_effective_factor(mul, force);

    StreamsFeeder feeder(this);

    /* compiler it */
    std::vector<TrexStreamsCompiledObj *> compiled_objs;
    std::string fail_msg;

    TrexStreamsCompiler compiler;
    TrexDPCoreMask mask(get_dp_core_count(), core_mask);

    bool rc = compiler.compile(m_port_id,
                               feeder.get_streams(),
                               compiled_objs,
                               mask,
                               factor,
                               &fail_msg);

    if (!rc) {
        feeder.set_status(false);
        throw TrexException(fail_msg);
    }

    feeder.set_status(true);

    /* generate a message to all the relevant DP cores to stop transmitting */
    assert(m_pending_async_stop_event == TrexDpPortEvents::INVALID_ID);
    m_pending_async_stop_event = m_dp_events.create_event(new AsyncStopEvent());

    /* update object status */
    m_factor = factor;
    m_last_all_streams_continues = compiled_objs[mask.get_active_cores()[0]]->get_all_streams_continues();
    m_last_duration = duration;

    change_state(PORT_STATE_TX);

    /* update the DP - messages will be freed by the DP */
    int index = 0;
    for (auto core_id : m_cores_id_list) {

        /* was the core assigned a compiled object ? */
        if (compiled_objs[index]) {
            TrexStatelessCpToDpMsgBase *start_msg = new TrexStatelessDpStart(m_port_id,
                                                                             m_pending_async_stop_event,
                                                                             compiled_objs[index],
                                                                             duration);
            send_message_to_dp(core_id, start_msg);
        } else {

            /* mimic an end event */
            m_dp_events.on_core_reporting_in(m_pending_async_stop_event, core_id);
        }

        index++;
    }
    
    /* for debug - this can be turn on */
    //m_dp_events.barrier();

    /* update subscribers */    
    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STARTED, data);
    
}


bool TrexStatelessPort::is_active() const {
    return   (  (m_port_state == PORT_STATE_TX) 
             || (m_port_state == PORT_STATE_PAUSE)
             || (m_port_state == PORT_STATE_PCAP_TX)
             );
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
    if (!is_active()) {
        return;
    }

    /* delete any previous graphs */
    delete_streams_graph();

    /* to avoid race, first destroy any previous stop/pause events */
    if (m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID) {
        m_dp_events.destroy_event(m_pending_async_stop_event);
        m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;

    }
    
    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);
    send_message_to_all_dp(stop_msg);

    /* a barrier - make sure all the DP cores stopped */
    m_dp_events.barrier();

    change_state(PORT_STATE_STREAMS);

    common_port_stop_actions(false);
}

/**
 * remove all RX filters from port
 * 
 * @author imarom (28-Mar-16)
 */
void
TrexStatelessPort::remove_rx_filters(void) {
    /* only valid when IDLE or with streams and not TXing */
    verify_state(PORT_STATE_STREAMS, "remove_rx_filters");

    for (auto entry : m_stream_table) {
        get_stateless_obj()->m_rx_flow_stat.stop_stream(entry.second);
    }

}

/**
 * when a port stops, perform various actions
 * 
 */
void
TrexStatelessPort::common_port_stop_actions(bool async) {

    Json::Value data;
    data["port_id"] = m_port_id;

    if (async) {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_FINISHED_TX, data);
    } else {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STOPPED, data);
    }

}

/**
 * core is considered active if it has a pending for async stop
 * 
 */
bool
TrexStatelessPort::is_core_active(int core_id) {
    return ( (m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID) &&
             (m_dp_events.is_core_pending_on_event(m_pending_async_stop_event, core_id))
           );
}

void
TrexStatelessPort::pause_traffic(void) {

    verify_state(PORT_STATE_TX, "pause");

    if (m_last_all_streams_continues == false) {
        throw TrexException(" pause is supported when all streams are in continues mode ");
    }

    if ( m_last_duration>0.0 ) {
        throw TrexException(" pause is supported when duration is not enable is start command ");
    }

    /* send a pause message */
    TrexStatelessCpToDpMsgBase *pause_msg = new TrexStatelessDpPause(m_port_id);

    /* send message to all cores */
    send_message_to_all_dp(pause_msg, true);

    /* make sure all DP cores paused */
    m_dp_events.barrier();

    /* change state */
    change_state(PORT_STATE_PAUSE);

    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_PAUSED, data);
}


void
TrexStatelessPort::resume_traffic(void) {

    verify_state(PORT_STATE_PAUSE, "resume");

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexStatelessCpToDpMsgBase *resume_msg = new TrexStatelessDpResume(m_port_id);

    send_message_to_all_dp(resume_msg, true);
    change_state(PORT_STATE_TX);

    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RESUMED, data);
}

void
TrexStatelessPort::update_traffic(const TrexPortMultiplier &mul, bool force) {

    double factor;

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE, "update");

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
    send_message_to_all_dp(update_msg, true);

    m_factor *= factor;

}

void
TrexStatelessPort::push_remote(const std::string &pcap_filename,
                               double ipg_usec,
                               double speedup,
                               uint32_t count,
                               double duration,
                               bool is_dual) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "push_remote");

    /* check that file exists */
    std::stringstream ss;
    CCapReaderBase *reader = CCapReaderFactory::CreateReader((char *)pcap_filename.c_str(), 0, ss);
    if (!reader) {
        throw TrexException(ss.str());
    }

    if ( (is_dual) && (reader->get_type() != ERF) ) {
        throw TrexException("dual mode is only supported on ERF format");
    }
    delete reader;

    /* only one core gets to play */
    int tx_core = m_cores_id_list[0];

    /* create async event */
    assert(m_pending_async_stop_event == TrexDpPortEvents::INVALID_ID);
    m_pending_async_stop_event = m_dp_events.create_event(new AsyncStopEvent());

    /* mark all other cores as done */
    for (int index = 1; index < m_cores_id_list.size(); index++) {
        /* mimic an end event */
        m_dp_events.on_core_reporting_in(m_pending_async_stop_event, m_cores_id_list[index]);
    }

    /* send a message to core */
    change_state(PORT_STATE_PCAP_TX);
    TrexStatelessCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(m_port_id,
                                                                       m_pending_async_stop_event,
                                                                       pcap_filename,
                                                                       ipg_usec,
                                                                       speedup,
                                                                       count,
                                                                       duration,
                                                                       is_dual);
    send_message_to_dp(tx_core, push_msg);

    /* update subscribers */    
    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STARTED, data);
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

    case PORT_STATE_PCAP_TX:
        return "PCAP_TX";
    }

    return "UNKNOWN";
}

int
TrexStatelessPort::get_max_stream_id() const {
    return m_stream_table.get_max_stream_id();
}

void
TrexStatelessPort::get_properties(std::string &driver) {

    driver = m_api_info.driver_name;
}

bool
TrexStatelessPort::verify_state(int state, const char *cmd_name, bool should_throw) const {
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
TrexStatelessPort::change_state(port_state_e new_state) {

    m_port_state = new_state;
}


void
TrexStatelessPort::encode_stats(Json::Value &port) {

    TrexPlatformInterfaceStats stats;
    m_platform_api->get_interface_stats(m_port_id, stats);

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
TrexStatelessPort::send_message_to_all_dp(TrexStatelessCpToDpMsgBase *msg, bool send_to_active_only) {

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
TrexStatelessPort::send_message_to_dp(uint8_t core_id, TrexStatelessCpToDpMsgBase *msg) {

    /* send the message to the core */
    CNodeRing *ring = CMsgIns::Ins()->getCpDp()->getRingCpToDp(core_id);
    ring->Enqueue((CGenNode *)msg);
}

void
TrexStatelessPort::send_message_to_rx(TrexStatelessCpToRxMsgBase *msg) {

    /* send the message to the core */
    CNodeRing *ring = CMsgIns::Ins()->getCpRx()->getRingCpToDp(0);
    ring->Enqueue((CGenNode *)msg);
}

uint64_t
TrexStatelessPort::get_port_speed_bps() const {
    return (uint64_t) m_platform_api->getPortAttrObj(m_port_id)->get_link_speed() * 1000 * 1000;
}

static inline double
bps_to_gbps(double bps) {
    return (bps / (1000.0 * 1000 * 1000));
}

double
TrexStatelessPort::calculate_effective_factor(const TrexPortMultiplier &mul, bool force) {

    double factor = calculate_effective_factor_internal(mul);

    /* did we exceeded the max L1 line rate ? */
    double expected_l1_rate = m_graph_obj->get_max_bps_l1(factor);

    /* if not force and exceeded - throw exception */
    if ( (!force) && (expected_l1_rate > get_port_speed_bps()) ) {
        stringstream ss;
        ss << "Expected L1 B/W: '" << bps_to_gbps(expected_l1_rate) << " Gbps' exceeds port line rate: '" << bps_to_gbps(get_port_speed_bps()) << " Gbps'";
        throw TrexException(ss.str());
    }

    /* L1 BW must be positive */
    if (expected_l1_rate <= 0){
        stringstream ss;
        ss << "Effective bandwidth must be positive, got: " << expected_l1_rate;
        throw TrexException(ss.str());
    }

    /* factor must be positive */
    if (factor <= 0) {
        stringstream ss;
        ss << "Factor must be positive, got: " << factor;
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
        return m_graph_obj->get_factor_bps_l2(mul.m_value);

    case TrexPortMultiplier::MUL_BPSL1:
        return m_graph_obj->get_factor_bps_l1(mul.m_value);

    case TrexPortMultiplier::MUL_PPS:
        return m_graph_obj->get_factor_pps(mul.m_value);

    case TrexPortMultiplier::MUL_PERCENTAGE:
        /* if abs percentage is from the line speed - otherwise its from the current speed */

        if (mul.m_op == TrexPortMultiplier::OP_ABS) {
            double required = (mul.m_value / 100.0) * get_port_speed_bps();
            return m_graph_obj->get_factor_bps_l1(required);
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
const std::initializer_list<std::string> TrexPortMultiplier::g_types = {"raw", "bps", "bpsl1", "pps", "percentage"};
const std::initializer_list<std::string> TrexPortMultiplier::g_ops   = {"abs", "add", "sub"};

TrexPortMultiplier::
TrexPortMultiplier(const std::string &type_str, const std::string &op_str, double value) {
    mul_type_e type;
    mul_op_e   op;

    if (type_str == "raw") {
        type = MUL_FACTOR; 

    } else if (type_str == "bps") {
        type = MUL_BPS;

    } else if (type_str == "bpsl1") {
        type = MUL_BPSL1;

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

    /* TODO: think of this mask...*/
    TrexDPCoreMask core_mask(get_dp_core_count(), TrexDPCoreMask::MASK_ALL);

    std::vector<TrexStreamsCompiledObj *> compiled_objs;

    std::string fail_msg;
    bool rc = compiler.compile(m_port_id,
                               streams,
                               compiled_objs,
                               core_mask,
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
TrexStatelessPort::get_port_effective_rate(double &pps,
                                           double &bps_L1,
                                           double &bps_L2,
                                           double &percentage) {

    if (get_stream_count() == 0) {
        return;
    }

    if (!m_graph_obj) {
        generate_streams_graph();
    }

    pps        = m_graph_obj->get_max_pps(m_factor);
    bps_L1     = m_graph_obj->get_max_bps_l1(m_factor);
    bps_L2     = m_graph_obj->get_max_bps_l2(m_factor);
    percentage = (bps_L1 / get_port_speed_bps()) * 100.0;
    
}

void
TrexStatelessPort::get_pci_info(std::string &pci_addr, int &numa_node) {
    pci_addr  = m_api_info.pci_addr;
    numa_node = m_api_info.numa_node;
}

void
TrexStatelessPort::add_stream(TrexStream *stream) {

    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "add_stream");

    get_stateless_obj()->m_rx_flow_stat.add_stream(stream);

    m_stream_table.add_stream(stream);
    delete_streams_graph();

    change_state(PORT_STATE_STREAMS);
}

void
TrexStatelessPort::remove_stream(TrexStream *stream) {

    verify_state(PORT_STATE_STREAMS, "remove_stream");

    get_stateless_obj()->m_rx_flow_stat.del_stream(stream);

    m_stream_table.remove_stream(stream);
    delete_streams_graph();

    if (m_stream_table.size() == 0) {
        change_state(PORT_STATE_IDLE);
    }
}

void
TrexStatelessPort::remove_and_delete_all_streams() {
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "remove_and_delete_all_streams");

    vector<TrexStream *> streams;
    get_object_list(streams);

    for (auto stream : streams) {
        remove_stream(stream);
        delete stream;
    }
}

void 
TrexStatelessPort::start_rx_capture(const std::string &pcap_filename, uint64_t limit) {

    m_rx_features_info.m_rx_capture_info.enable(pcap_filename, limit);

    TrexStatelessCpToRxMsgBase *msg = new TrexStatelessRxStartCapture(m_port_id, m_rx_features_info.m_rx_capture_info);
    send_message_to_rx(msg);
}

void
TrexStatelessPort::stop_rx_capture() {
    TrexStatelessCpToRxMsgBase *msg = new TrexStatelessRxStopCapture(m_port_id);
    send_message_to_rx(msg);
    m_rx_features_info.m_rx_capture_info.disable();
}

void 
TrexStatelessPort::start_rx_queue(uint64_t size) {

    m_rx_features_info.m_rx_queue_info.enable(size);

    TrexStatelessCpToRxMsgBase *msg = new TrexStatelessRxStartQueue(m_port_id, m_rx_features_info.m_rx_queue_info);
    send_message_to_rx(msg);
}

void
TrexStatelessPort::stop_rx_queue() {
    TrexStatelessCpToRxMsgBase *msg = new TrexStatelessRxStopQueue(m_port_id);
    send_message_to_rx(msg);
    m_rx_features_info.m_rx_queue_info.disable();
}


RXPacketBuffer *
TrexStatelessPort::get_rx_queue_pkts() {

    if (m_rx_features_info.m_rx_queue_info.is_empty()) {
        return NULL;
    }

    /* ask RX core for the pkt queue */
    TrexStatelessMsgReply<RXPacketBuffer *> msg_reply;

    TrexStatelessCpToRxMsgBase *msg = new TrexStatelessRxQueueGetPkts(m_port_id, msg_reply);
    send_message_to_rx(msg);

    RXPacketBuffer *pkt_buffer = msg_reply.wait_for_reply();
    return pkt_buffer;
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
