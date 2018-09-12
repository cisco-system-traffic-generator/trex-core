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


#include "trex_rx_defs.h"
#include "publisher/trex_publisher.h"

#include "trex_stl.h"
#include "trex_stl_port.h"
#include "trex_stl_streams_compiler.h"
#include "trex_stl_messaging.h"
#include "trex_stl_fs.h"

#include <common/basic_utils.h>
#include <common/captureFile.h>

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
        TrexStatelessPort *port = dynamic_cast<TrexStatelessPort *>(get_port());
        
        port->change_state(TrexPort::PORT_STATE_STREAMS);

        port->common_port_stop_actions(true);

        assert(port->m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID);
        port->m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
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
        
        m_port = port;
    }
    
    void feed() {
        
        /* fetch the original streams */
        m_port->get_object_list(m_in_streams);

        for (const TrexStream *in_stream : m_in_streams) {
            TrexStream *out_stream = in_stream->clone(true);

            m_out_streams.push_back(out_stream);
            
            CFlowStatRuleMgr::instance()->start_stream(out_stream);
            
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
                CFlowStatRuleMgr::instance()->copy_state(out_stream, in_stream);
            } else {
                /* fail path */
                CFlowStatRuleMgr::instance()->reset_stream(out_stream);
            }
            delete out_stream;
        }
    }

private:
    vector<TrexStream *>  m_in_streams;
    vector<TrexStream *>  m_out_streams;
    bool                  m_success;
    
    TrexStatelessPort    *m_port;
};


/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id) : TrexPort(port_id) {
    m_is_service_mode_on  = false;
    m_graph_obj = NULL;
}

TrexStatelessPort::~TrexStatelessPort() {

    stop_traffic();
    remove_and_delete_all_streams();
}


/**
 * starts the traffic on the port
 * 
 */
void
TrexStatelessPort::start_traffic(const TrexPortMultiplier &mul, double duration, bool force, uint64_t core_mask, double start_at_ts) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_STREAMS, "start");

    /* just making sure no leftovers... */
    delete_streams_graph();

    /* on start - we can only provide absolute values */
    assert(mul.m_op == TrexPortMultiplier::OP_ABS);

    if ( !force ) {
        /* check link state */
        if (!get_platform_api().getPortAttrObj(m_port_id)->is_link_up() ) {
            throw TrexException("Link state is DOWN.");
        }

        /* verify either valid dest MAC on port or explicit dest MAC on ALL streams */
        bool has_non_explicit_dst_mac_stream = false;
        for (auto &stream : m_stream_table) {
            if ( stream.second->get_override_dst_mac_mode() != TrexStream::stPKT ) {
                has_non_explicit_dst_mac_stream = true;
                break;
            }
        }

        if ( has_non_explicit_dst_mac_stream ) {
            bool port_dst_mac_valid = is_dst_mac_valid();
            if ( !port_dst_mac_valid ) {
                std::string err;
                err = "Port " + std::to_string(m_port_id) + " dest MAC is invalid and there are streams without explicit dest MAC.";
                throw TrexException(err);
            }
        }
    }

    /* we now need the graph - generate it if we don't have it (happens once) */
    if (!m_graph_obj) {
        generate_streams_graph();
    }

    /* caclulate the effective factor for DP */
    double factor = calculate_effective_factor(mul, force, *m_graph_obj);

    StreamsFeeder feeder(this);
    feeder.feed();

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

    // verify start_at_ts is sane (at least now_sec + 100msec so that DP will awake and run at given time)
    // TODO: check if we can replace 0.1 with variable/define relative to LONG_DELAY_MS
    if ( start_at_ts && ( start_at_ts < now_sec() + 0.1 ) ) {
        feeder.set_status(false);
        throw TrexException("start_at_ts, if defined, should be at least 100ms from now_sec()");
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
            TrexCpToDpMsgBase *start_msg = new TrexStatelessDpStart(m_port_id,
                                                                    m_pending_async_stop_event,
                                                                    compiled_objs[index],
                                                                    duration,
                                                                    start_at_ts);
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
    TrexCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);
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
        CFlowStatRuleMgr::instance()->stop_stream(entry.second);
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
    TrexCpToDpMsgBase *pause_msg = new TrexStatelessDpPause(m_port_id);

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
TrexStatelessPort::pause_streams(stream_ids_t &stream_ids) {

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE, "pause");

    if (m_last_all_streams_continues == false) {
        throw TrexException(" pause is supported when all streams are in continues mode ");
    }

    if ( m_last_duration>0.0 ) {
        throw TrexException(" pause is supported when duration is not enable is start command ");
    }

    /* send a pause message */
    TrexCpToDpMsgBase *pause_msg = new TrexStatelessDpPauseStreams(m_port_id, stream_ids);

    /* send message to all cores */
    send_message_to_all_dp(pause_msg, true);

    /* make sure all DP cores paused */
    m_dp_events.barrier();

}

void
TrexStatelessPort::resume_traffic(void) {

    verify_state(PORT_STATE_PAUSE, "resume");

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexCpToDpMsgBase *resume_msg = new TrexStatelessDpResume(m_port_id);

    send_message_to_all_dp(resume_msg, true);
    change_state(PORT_STATE_TX);

    Json::Value data;
    data["port_id"] = m_port_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RESUMED, data);
}

void
TrexStatelessPort::resume_streams(stream_ids_t &stream_ids) {

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE, "resume");

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexCpToDpMsgBase *resume_msg = new TrexStatelessDpResumeStreams(m_port_id, stream_ids);

    send_message_to_all_dp(resume_msg, true);
}


void
TrexStatelessPort::update_traffic(const TrexPortMultiplier &mul, bool force) {

    double factor;

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE, "update");

    /* generate a message to all the relevant DP cores to start transmitting */
    double new_factor = calculate_effective_factor(mul, force, *m_graph_obj);

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

    TrexCpToDpMsgBase *update_msg = new TrexStatelessDpUpdate(m_port_id, factor);
    send_message_to_all_dp(update_msg, true);

    m_factor *= factor;

}


void
TrexStatelessPort::update_streams(const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams) {

    verify_state(PORT_STATE_TX | PORT_STATE_PAUSE, "update");

    /* on update stream - we can only provide absolute values */
    if (mul.m_op != TrexPortMultiplier::OP_ABS) {
        throw TrexException("Must use absolute multiplier for updating stream");
    }

    stream_ipgs_map_t ipg_per_stream;
    std::vector<TrexStream *> stream_list;
    TrexStreamsGraph graph;
    double new_factor;
    uint8_t active_core_count = get_active_cores_count();

    for (TrexStream* stream:streams) {
        stream_list.clear();
        stream_list.push_back(stream);
        const TrexStreamsGraphObj *graph_obj = graph.generate(stream_list);

        try {
            new_factor = calculate_effective_factor(mul, force, *graph_obj);
            delete graph_obj;
        } catch (const TrexException &ex) {
            delete graph_obj;
            throw(ex);
        }

        ipg_per_stream[stream->m_stream_id] = (double) active_core_count / (new_factor * stream->get_pps());
    }

    TrexCpToDpMsgBase *update_msg = new TrexStatelessDpUpdateStreams(m_port_id, ipg_per_stream);
    send_message_to_all_dp(update_msg, true);
}


void
TrexStatelessPort::push_remote(const std::string &pcap_filename,
                               double ipg_usec,
                               double min_ipg_sec,
                               double speedup,
                               uint32_t count,
                               double duration,
                               bool is_dual) {

    /* command allowed only on state stream */
    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "push_remote");

    /* check that file exists */
    std::stringstream ss;

/* fast workaround for trex-403 until we will fix it correctly. this case can block for very long time crash the python with disconnect  */
#if 0
    CCapReaderBase *reader = CCapReaderFactory::CreateReader((char *)pcap_filename.c_str(), 0, ss);
    if (!reader) {
        throw TrexException(ss.str());
    }

    if ( (is_dual) && (reader->get_type() != ERF) ) {
        throw TrexException("dual mode is only supported on ERF format");
    }
    delete reader;
#endif

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
    TrexCpToDpMsgBase *push_msg = new TrexStatelessDpPushPCAP(m_port_id,
                                                              m_pending_async_stop_event,
                                                              pcap_filename,
                                                              ipg_usec,
                                                              min_ipg_sec,
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




int
TrexStatelessPort::get_max_stream_id() const {
    return m_stream_table.get_max_stream_id();
}


static inline double
bps_to_gbps(double bps) {
    return (bps / (1000.0 * 1000 * 1000));
}


double
TrexStatelessPort::calculate_effective_factor(const TrexPortMultiplier &mul, bool force, const TrexStreamsGraphObj &graph_obj) {

    double factor = calculate_effective_factor_internal(mul, graph_obj);

    /* did we exceeded the max L1 line rate ? */
    double expected_l1_rate = graph_obj.get_max_bps_l1(factor);


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

    /* if force simply return the value */
    if (force) {
        return factor;
    } else {
        
        /* due to float calculations we allow 0.1% roundup */
        if ( (expected_l1_rate / get_port_speed_bps()) > 1.0001 )  {
            stringstream ss;
            ss << "Expected L1 B/W: '" << bps_to_gbps(expected_l1_rate) << " Gbps' exceeds port line rate: '" << bps_to_gbps(get_port_speed_bps()) << " Gbps'";
            throw TrexException(ss.str());
        }
        
        /* in any case, without force, do not return any value higher than the max factor */
        double max_factor = graph_obj.get_factor_bps_l1(get_port_speed_bps());
        return std::min(max_factor, factor);
    }
    
}

double
TrexStatelessPort::calculate_effective_factor_internal(const TrexPortMultiplier &mul, const TrexStreamsGraphObj &graph_obj) {

    switch (mul.m_type) {

    case TrexPortMultiplier::MUL_FACTOR:
        return (mul.m_value);

    case TrexPortMultiplier::MUL_BPS:
        return graph_obj.get_factor_bps_l2(mul.m_value);

    case TrexPortMultiplier::MUL_BPSL1:
        return graph_obj.get_factor_bps_l1(mul.m_value);

    case TrexPortMultiplier::MUL_PPS:
        return graph_obj.get_factor_pps(mul.m_value);

    case TrexPortMultiplier::MUL_PERCENTAGE:
        /* if abs percentage is from the line speed - otherwise its from the current speed */

        if (mul.m_op == TrexPortMultiplier::OP_ABS) {
            double required = (mul.m_value / 100.0) * get_port_speed_bps();
            return graph_obj.get_factor_bps_l1(required);
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
TrexStatelessPort::add_stream(TrexStream *stream) {

    verify_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "add_stream");

    if (m_stream_table.size() >= MAX_STREAMS) {
        throw TrexException("Reached limit of " + std::to_string(MAX_STREAMS) + " streams at the port.");
    }
    CFlowStatRuleMgr::instance()->add_stream(stream);

    m_stream_table.add_stream(stream);
    delete_streams_graph();

    change_state(PORT_STATE_STREAMS);
}

void
TrexStatelessPort::remove_stream(TrexStream *stream) {

    verify_state(PORT_STATE_STREAMS, "remove_stream");

    CFlowStatRuleMgr::instance()->del_stream(stream);

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

/**
 * enable/disable service mode 
 * sends a query to the RX core 
 * 
 */
void 
TrexStatelessPort::set_service_mode(bool enabled) {
    static MsgReply<TrexStatelessRxQuery::query_rc_e> reply;
    reply.reset();
    
    TrexStatelessRxQuery::query_type_e query_type = (enabled ? TrexStatelessRxQuery::SERVICE_MODE_ON : TrexStatelessRxQuery::SERVICE_MODE_OFF);
    
    TrexCpToRxMsgBase *msg = new TrexStatelessRxQuery(m_port_id, query_type, reply);
    send_message_to_rx(msg);
    
    TrexStatelessRxQuery::query_rc_e rc = reply.wait_for_reply();
    
    switch (rc) {
    case TrexStatelessRxQuery::RC_OK:
        if (enabled) {
            getPortAttrObj()->set_rx_filter_mode(RX_FILTER_MODE_ALL);
        } else {
            getPortAttrObj()->set_rx_filter_mode(RX_FILTER_MODE_HW);
        }
        m_is_service_mode_on = enabled;
        break;
        
    case TrexStatelessRxQuery::RC_FAIL_RX_QUEUE_ACTIVE:
        throw TrexException("unable to disable service mode - please remove RX queue");
        
    case TrexStatelessRxQuery::RC_FAIL_CAPTURE_ACTIVE:
        throw TrexException("unable to disable service mode - an active capture on port " + std::to_string(m_port_id) + " exists");

    case TrexStatelessRxQuery::RC_FAIL_CAPWAP_PROXY_ACTIVE:
        throw TrexException("unable to disable service mode - please remove CAPWAP proxy");

    case TrexStatelessRxQuery::RC_FAIL_CAPTURE_PORT_ACTIVE:
        throw TrexException("unable to disable service mode - port " + std::to_string(m_port_id) + " is configured as capture port"); 

    default:
        assert(0);
    }
    
    /* update the all the relevant dp cores to move to service mode */
    TrexStatelessDpServiceMode *dp_msg = new TrexStatelessDpServiceMode(m_port_id, enabled);
    send_message_to_all_dp(dp_msg);
}


