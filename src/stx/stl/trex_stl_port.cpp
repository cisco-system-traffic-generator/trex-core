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


class AsyncProfileStopEvent : public TrexDpPortEvent {

protected:
    /**
     * when an async event occurs (all cores have reported in)
     *
     * @author imarom (29-Feb-16)
     */
    virtual void on_event() {
        TrexStatelessProfile *mprofile = dynamic_cast<TrexStatelessProfile *>(get_port());

        mprofile->change_state(TrexPort::PORT_STATE_STREAMS);
        mprofile->common_profile_stop_actions(true);

        uint8_t port_id = get_port()->get_port_id();
        TrexStatelessPort *port = (TrexStatelessPort*) get_stx()->get_port_by_id(port_id);
        port->update_and_publish_port_state(true);

        assert(mprofile->m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID);
        mprofile->m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
    }

    /**
     * when a DP core encountered an error
     *
     * @author imarom (20-Apr-16)
     */
    virtual void on_error(int thread_id) {
        Json::Value data;

        TrexStatelessProfile *mprofile = dynamic_cast<TrexStatelessProfile *>(get_port());
        data["port_id"]   = get_port()->get_port_id();
        data["profile_id"]   = mprofile->m_profile_id;
        data["thread_id"] = thread_id;

        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_ERROR, data);
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

    StreamsFeeder(TrexStatelessProfile *mprofile) {

        /* start pesimistic */
        m_success = false;
        m_mprofile = mprofile;
    }

    void feed() {
 
        /* fetch the original streams */
        m_mprofile->get_object_list(m_in_streams);

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
 
    TrexStatelessProfile    *m_mprofile;
};



/***************************
 * trex stateless profile
 *
 **************************/
TrexStatelessProfile::TrexStatelessProfile(uint8_t port_id, string profile_id): TrexPort(port_id) {

    m_port_id = port_id;
    m_profile_id = profile_id;
    m_graph_obj = NULL;
    m_non_explicit_dst_macs_count = 0;
    m_flow_stats_count = 0;
    change_state(PORT_STATE_IDLE);
}

TrexStatelessProfile::~TrexStatelessProfile() {
    stop_traffic();
    remove_and_delete_all_streams();
}

void
TrexStatelessProfile::store_profile_json(const Json::Value &profile_json) {
    /* deep copy */
    m_profile_json = profile_json;
}

const Json::Value &
TrexStatelessProfile::get_profile_json() {
    return m_profile_json;
}


bool TrexStatelessProfile::is_running_flow_stats() {
    return ( m_port_state == PORT_STATE_TX || m_port_state == PORT_STATE_PAUSE ) && has_flow_stats();
}

bool TrexStatelessProfile::has_flow_stats() {
    return m_flow_stats_count;
}

/**
 * starts the traffic on the port
 *
 */
void
TrexStatelessProfile::start_traffic(const TrexPortMultiplier &mul, double duration, bool force, uint64_t core_mask, double start_at_ts) {

    /* command allowed only on state stream */
    verify_profile_state(PORT_STATE_STREAMS, "start");

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
        if ( m_non_explicit_dst_macs_count && !is_dst_mac_valid()) {
            throw TrexException("Port " + to_string(m_port_id) + " dest MAC is invalid and there are streams without explicit dest MAC.");
        }
    }

    /* we now need the graph - generate it (happens once) */
    generate_streams_graph();

    /* caclulate the effective factor for DP */
    double factor = calculate_effective_factor(mul, force, *m_graph_obj);

    StreamsFeeder feeder(this);
    feeder.feed();

    /* compiler it */
    std::vector<TrexStreamsCompiledObj *> compiled_objs;
    std::string fail_msg;

    TrexStreamsCompiler compiler(m_profile_id);

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
    m_pending_async_stop_event = m_dp_events.create_event(new AsyncProfileStopEvent());

    /* update object status */
    m_factor = factor;
    m_last_all_streams_continues = compiled_objs[mask.get_active_cores()[0]]->get_all_streams_continues();
    m_last_duration = duration;
    
    change_state(PORT_STATE_TX);

    /* update the DP - messages will be freed by the DP */
    int index = 0;
    for (auto core_id : m_cores_id_list) {

        if ( !compiled_objs[index] ) {
            m_dp_events.on_core_reporting_in(m_pending_async_stop_event, core_id);
            index++;
            continue;
        }

        /* was the core assigned a compiled object ? */
        if (!compiled_objs[index]->is_empty()) {
            TrexCpToDpMsgBase *start_msg = new TrexStatelessDpStart(m_port_id,
                                                                    m_dp_profile_id,
                                                                    m_pending_async_stop_event,
                                                                    compiled_objs[index],
                                                                    duration,
                                                                    start_at_ts);
            send_message_to_dp(core_id, start_msg);
        } else {
            delete compiled_objs[index];
            /* mimic an end event */
            m_dp_events.on_core_reporting_in(m_pending_async_stop_event, core_id);
        }

        index++;
    }
    
    /* for debug - this can be turn on */
    //m_dp_events.barrier(m_dp_profile_id);

    /* update subscribers */    
    Json::Value data;
    data["port_id"] = m_port_id;
    data["profile_id"] = m_profile_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_STARTED, data);

}


/**
 * stop traffic on port
 * 
 * @author imarom (09-Nov-15)
 * 
 * @return TrexStatelessProfile::rc_e 
 */
void
TrexStatelessProfile::stop_traffic() {

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
    TrexCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id, m_dp_profile_id);
    send_message_to_all_dp(stop_msg);

    /* a barrier - make sure all the DP cores stopped */
    m_dp_events.barrier(m_dp_profile_id);

    change_state(PORT_STATE_STREAMS);

    common_profile_stop_actions(false);
}

/**
 * remove all RX filters from port
 * 
 * @author imarom (28-Mar-16)
 */
void
TrexStatelessProfile::remove_rx_filters() {
    /* only valid when IDLE or with streams and not TXing */
    verify_profile_state(PORT_STATE_STREAMS, "remove_rx_filters");

    for (auto entry : m_stream_table) {
        CFlowStatRuleMgr::instance()->stop_stream(entry.second);
    }

}


void
TrexStatelessProfile::common_profile_stop_actions(bool async) {

    Json::Value data;
    data["port_id"] = m_port_id;
    data["profile_id"] = m_profile_id;

    if (async) {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_FINISHED_TX, data);
    } else {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_STOPPED, data);
    }

}


void
TrexStatelessProfile::pause_traffic(void) {

    verify_profile_state(PORT_STATE_TX, "pause");

    if (m_last_all_streams_continues == false) {
        throw TrexException(" pause is supported when all streams are in continues mode ");
    }

    if ( m_last_duration>0.0 ) {
        throw TrexException(" pause is supported when duration is not enable is start command ");
    }

    /* send a pause message */
    TrexCpToDpMsgBase *pause_msg = new TrexStatelessDpPause(m_port_id, m_dp_profile_id);

    /* send message to all cores */
    send_message_to_all_dp(pause_msg, true);

    /* make sure all DP cores paused */
    m_dp_events.barrier(m_dp_profile_id);

    /* change state */
    change_state(PORT_STATE_PAUSE);

    Json::Value data;
    data["port_id"] = m_port_id;
    data["profile_id"] = m_profile_id;
get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_PAUSED, data);
}

void
TrexStatelessProfile::pause_streams(stream_ids_t &stream_ids) {

    verify_profile_state(PORT_STATE_TX | PORT_STATE_PAUSE, "pause");

    if (m_last_all_streams_continues == false) {
        throw TrexException(" pause is supported when all streams are in continues mode ");
    }

    if ( m_last_duration>0.0 ) {
        throw TrexException(" pause is supported when duration is not enable is start command ");
    }

    /* send a pause message */
    TrexCpToDpMsgBase *pause_msg = new TrexStatelessDpPauseStreams(m_port_id, m_dp_profile_id, stream_ids);

    /* send message to all cores */
    send_message_to_all_dp(pause_msg, true);

    /*  comment-out for multi profiles */
    /* make sure all DP cores paused */
    m_dp_events.barrier(m_dp_profile_id);

}

void
TrexStatelessProfile::resume_traffic(void) {

    verify_profile_state(PORT_STATE_PAUSE, "resume");

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexCpToDpMsgBase *resume_msg = new TrexStatelessDpResume(m_port_id, m_dp_profile_id);

    send_message_to_all_dp(resume_msg, true);
    change_state(PORT_STATE_TX);


    Json::Value data;
    data["port_id"] = m_port_id;
    data["profile_id"] = m_profile_id;
    get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PROFILE_RESUMED, data);
}

void
TrexStatelessProfile::resume_streams(stream_ids_t &stream_ids) {

    verify_profile_state(PORT_STATE_TX | PORT_STATE_PAUSE, "resume");

    /* generate a message to all the relevant DP cores to start transmitting */
    TrexCpToDpMsgBase *resume_msg = new TrexStatelessDpResumeStreams(m_port_id, m_dp_profile_id, stream_ids);

    send_message_to_all_dp(resume_msg, true);
}


void
TrexStatelessProfile::update_traffic(const TrexPortMultiplier &mul, bool force) {

    double factor;

    verify_profile_state(PORT_STATE_TX | PORT_STATE_PAUSE, "update");

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

    TrexCpToDpMsgBase *update_msg = new TrexStatelessDpUpdate(m_port_id, m_dp_profile_id, factor);
    send_message_to_all_dp(update_msg, true);

    m_factor *= factor;
}


void
TrexStatelessProfile::update_streams(const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams) {

    verify_profile_state(PORT_STATE_TX | PORT_STATE_PAUSE, "update");

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

    TrexCpToDpMsgBase *update_msg = new TrexStatelessDpUpdateStreams(m_port_id, m_dp_profile_id, ipg_per_stream);
    send_message_to_all_dp(update_msg, true);
}



int
TrexStatelessProfile::get_max_stream_id() const {
    return m_stream_table.get_max_stream_id();
}

static inline double
bps_to_gbps(double bps) {
    return (bps / (1000.0 * 1000 * 1000));
}


double
TrexStatelessProfile::calculate_effective_factor(const TrexPortMultiplier &mul, bool force, const TrexStreamsGraphObj &graph_obj) {

    double factor = calculate_effective_factor_internal(mul, graph_obj);

    /* did we exceeded the max L1 line rate ? */
    double expected_l1_rate = graph_obj.get_max_bps_l1(factor);


    /* L1 BW must be positive */
    if (expected_l1_rate <= 0){
        stringstream ss;
        ss << "Effective bandwidth is zero, this means that your dpdk driver does not know the link throughput (virtual), please use absolute bandwidth  " << expected_l1_rate;
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
TrexStatelessProfile::calculate_effective_factor_internal(const TrexPortMultiplier &mul, const TrexStreamsGraphObj &graph_obj) {

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
TrexStatelessProfile::generate_streams_graph() {

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
TrexStatelessProfile::delete_streams_graph() {
    if (m_graph_obj) {
        delete m_graph_obj;
        m_graph_obj = NULL;
    }
}

/* verify state for profile */
bool
TrexStatelessProfile::verify_profile_state(int state, const char *cmd_name, bool should_throw) const {

    if ( (state & get_state()) == 0 ) {
        if (should_throw) {
            std::stringstream ss;
            ss << "command '" << cmd_name << "' cannot be executed on current state: '" << get_state_as_string() << " on profile_id(" << m_profile_id << ").";
            throw TrexException(ss.str());
        } else {
            return false;
        }
    }

    return true;
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
TrexStatelessProfile::validate(void) {

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
TrexStatelessProfile::get_port_effective_rate(double &pps,
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
TrexStatelessProfile::add_stream(TrexStream *stream) {

    verify_profile_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "add_stream");

    if (m_stream_table.size() >= MAX_STREAMS) {
        throw TrexException("Reached limit of " + std::to_string(MAX_STREAMS) + " streams at the port.");
    }
    CFlowStatRuleMgr::instance()->add_stream(stream);

    m_stream_table.add_stream(stream);
    delete_streams_graph();

    if ( !stream->is_null_stream() ) {
        if ( stream->m_rx_check.m_enabled ) {
            m_flow_stats_count++;
        }
        if ( !stream->has_explicit_dst_mac() ) {
            m_non_explicit_dst_macs_count++;
        }
    }
    change_state(PORT_STATE_STREAMS);
}

void
TrexStatelessProfile::remove_stream(TrexStream *stream) {

    verify_profile_state(PORT_STATE_STREAMS, "remove_stream");

    CFlowStatRuleMgr::instance()->del_stream(stream);

    m_stream_table.remove_stream(stream);
    delete_streams_graph();

    if ( !stream->is_null_stream() ) {
        if ( stream->m_rx_check.m_enabled ) {
            m_flow_stats_count--;
        }
        if ( !stream->has_explicit_dst_mac() ) {
            m_non_explicit_dst_macs_count--;
        }
    }

    if (m_stream_table.size() == 0) {
        change_state(PORT_STATE_IDLE);
    }
}

void
TrexStatelessProfile::remove_and_delete_all_streams() {

    verify_profile_state(PORT_STATE_IDLE | PORT_STATE_STREAMS, "remove_and_delete_all_streams");
    vector<TrexStream *> streams;
    get_object_list(streams);

    for (auto stream : streams) {
        remove_stream(stream);
        delete stream;
    }
    m_non_explicit_dst_macs_count = 0;
    m_flow_stats_count = 0;
}


/* end of profile */




/***************************
 * trex stateless port
 * 
 **************************/
TrexStatelessPort::TrexStatelessPort(uint8_t port_id) : TrexPort(port_id) {

    m_is_service_mode_on          = false;
    m_is_service_filtered_mode_on = false;
    TrexStatelessProfile *mprofile = new TrexStatelessProfile(m_port_id, default_profile);
    mprofile->m_dp_profile_id = ++m_dp_profile_id_inc;
    m_profile_table.add_profile(mprofile);
}

TrexStatelessPort::~TrexStatelessPort() {

    stop_traffic("*");
    remove_and_delete_all_streams("*");
}


bool
TrexStatelessPort::is_running_flow_stats() {

    std::vector<string> profile_list;
    get_profile_id_list(profile_list);

    bool is_running_flow_stats = false;
    for (auto &profile_id : profile_list) {
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        if ( mprofile->is_running_flow_stats()) {
            is_running_flow_stats = true;
        }
    }
    return is_running_flow_stats;
}


bool 
TrexStatelessPort::has_flow_stats(string profile_id) {

    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    return mprofile->has_flow_stats();
}

/**
 * starts the traffic on the port
 * 
 */
void
TrexStatelessPort::start_traffic(string profile_id, const TrexPortMultiplier &mul, double duration, bool force, uint64_t core_mask, double start_at_ts) {

    if (get_state()==(TrexPort::PORT_STATE_PCAP_TX)) {
        std::stringstream ss;
        ss << "start_traffici: port(" << m_port_id << ") can't execute on PORT_STATE_PCAP_TX";
        throw TrexException(ss.str());
    }

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->start_traffic(mul, duration, force, core_mask, start_at_ts);
    update_and_publish_port_state();
 
}


/**
 * stop traffic on port
 * 
 */
void
TrexStatelessPort::stop_traffic(string profile_id) {

    if ((get_state()==(TrexPort::PORT_STATE_PCAP_TX))) {
        stop_traffic_pcap();
    }

    if (profile_id == "*") {
        std::vector<string> profile_list;
        get_profile_id_list(profile_list);

        std::stringstream ss ;
        for (auto &profile_id : profile_list) {
            try {
                TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
                mprofile->stop_traffic();
            } catch (const TrexException &ex) {
                ss << ex.what() << std::endl;
            }
        }
        update_and_publish_port_state();

        if (ss.str()!="") {
            throw TrexException(ss.str());
        }

    } else {
        valide_profile(profile_id);
        TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
        mprofile->stop_traffic();
        update_and_publish_port_state();
    }
}



void
TrexStatelessPort::stop_traffic_pcap(void) {

    if (!is_active()) {
        return;
    }

    if (m_pending_async_stop_event != TrexDpPortEvents::INVALID_ID) {
        m_dp_events.destroy_event(m_pending_async_stop_event);
        m_pending_async_stop_event = TrexDpPortEvents::INVALID_ID;
    }

    TrexCpToDpMsgBase *stop_msg = new TrexStatelessDpStop(m_port_id);
    send_message_to_all_dp(stop_msg);

    m_dp_events.barrier();

    change_state(PORT_STATE_STREAMS);
    common_port_stop_actions(false);
}


/**
 * remove all RX filters from port
 * 
 */
void
TrexStatelessPort::remove_rx_filters(string profile_id) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->remove_rx_filters();
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
TrexStatelessPort::pause_traffic(string profile_id) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->pause_traffic();
    update_and_publish_port_state();
}


void
TrexStatelessPort::pause_streams(string profile_id, stream_ids_t &stream_ids) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->pause_streams(stream_ids);
}


void
TrexStatelessPort::resume_traffic(string profile_id) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->resume_traffic();
    update_and_publish_port_state();
}

void
TrexStatelessPort::resume_streams(string profile_id, stream_ids_t &stream_ids) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->resume_streams(stream_ids);
}


void
TrexStatelessPort::update_traffic(string profile_id, const TrexPortMultiplier &mul, bool force) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->update_traffic(mul, force);
}

void
TrexStatelessPort::update_streams(string profile_id, const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->update_streams(mul, force, streams);
}


void
TrexStatelessPort::push_remote(const std::string &pcap_filename,
                               double ipg_usec,
                               double min_ipg_sec,
                               double speedup,
                               uint32_t count,
                               double duration,
                               bool is_dual) {

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
TrexStatelessPort::get_max_stream_id(){

    int port_max_stream_id = 0;
    std::vector <TrexStatelessProfile *> profiles;
    get_profile_object_list(profiles);

    if(profiles.size() == 0)
        return 0;
    else {
        for (auto &mprofile : profiles) {
            if(port_max_stream_id < mprofile->get_max_stream_id()){
                port_max_stream_id = mprofile->get_max_stream_id();
            }
        }
        return port_max_stream_id;
    } 
}


const TrexStreamsGraphObj *
TrexStatelessPort::validate(string profile_id) {

    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    return mprofile->validate();
}


void
TrexStatelessPort::get_port_effective_rate(double &pps,
                                           double &bps_L1,
                                           double &bps_L2,
                                           double &percentage) {

    TrexStatelessProfile *mprofile = get_profile_by_id(default_profile);
    mprofile->get_port_effective_rate(pps, bps_L1, bps_L2, percentage);
}


void
TrexStatelessPort::add_stream(string profile_id, TrexStream *stream) {

    if (!(get_profile_by_id(profile_id)) ) {
        TrexStatelessProfile *mprofile = new TrexStatelessProfile(m_port_id, profile_id);
        mprofile->m_dp_profile_id = ++m_dp_profile_id_inc;
        m_profile_table.add_profile(mprofile);
    }
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->add_stream(stream);
    update_and_publish_port_state();
}


void
TrexStatelessPort::remove_stream(string profile_id, TrexStream *stream) {

    valide_profile(profile_id);
    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    mprofile->remove_stream(stream);
    update_and_publish_port_state();
}


void
TrexStatelessPort::remove_and_delete_all_streams(string profile_id) {

    if (profile_id == "*") {
        std::vector<string> profile_list;
        get_profile_id_list(profile_list);

        std::stringstream ss ;
        for (auto &profile_id : profile_list) {
            try {
                TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
                mprofile->remove_and_delete_all_streams();
            } catch (const TrexException &ex) {
                ss << ex.what() << std::endl;
            }
        }
        update_and_publish_port_state();

        if (ss.str()!="") {
            throw TrexException(ss.str());
        }

    } else {
        /* Don't valide_profile due to this command can be execute for no existence profile */
        if( get_profile_by_id(profile_id) ) {
            TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
            mprofile->remove_and_delete_all_streams();
            update_and_publish_port_state();
        }
    }
}


/**
 * enable/disable service mode 
 * sends a query to the RX core 
 * 
 */
void 
TrexStatelessPort::set_service_mode(bool enabled) {
    set_service_mode(enabled, false, 0);
}

void 
TrexStatelessPort::set_service_mode(bool enabled, bool filtered, uint8_t mask) {

    static MsgReply<TrexStatelessRxQuery::query_rc_e> reply;
    reply.reset();
    
    TrexStatelessRxQuery::query_type_e query_type = (enabled ? TrexStatelessRxQuery::SERVICE_MODE_ON : TrexStatelessRxQuery::SERVICE_MODE_OFF);
    query_type = filtered ? TrexStatelessRxQuery::SERVICE_MODE_FILTERED : query_type;

    TrexCpToRxMsgBase *msg = new TrexStatelessRxQuery(m_port_id, query_type, reply);
    send_message_to_rx(msg);

    TrexStatelessRxQuery::query_rc_e rc = reply.wait_for_reply();
    
    switch (rc) {
    case TrexStatelessRxQuery::RC_OK:
        if (enabled || filtered) {
            getPortAttrObj()->set_rx_filter_mode(RX_FILTER_MODE_ALL);
        } else {
            getPortAttrObj()->set_rx_filter_mode(RX_FILTER_MODE_HW);
        }
        m_is_service_mode_on = enabled;
        m_is_service_filtered_mode_on = filtered;
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
    TrexStatelessDpServiceMode *dp_msg = new TrexStatelessDpServiceMode(m_port_id, enabled, filtered , mask);
    send_message_to_all_dp(dp_msg);
}


/**
 * get profile state function 
 *
 */
TrexPort::port_state_e
TrexStatelessPort::get_profile_state(string profile_id) {

    TrexStatelessProfile *mprofile = get_profile_by_id(profile_id);
    return mprofile->get_state(); 
}


void
TrexStatelessPort::get_state_as_string(string profile_id, Json::Value &result) {

    if ( profile_id == "*" ) {
        get_profiles_status(result);
    } else {
        result = get_profile_state_as_string(profile_id);
    }
}


void
TrexStatelessPort::get_profiles_status(Json::Value &result) {

    std::vector<string> profile_list;
    get_profile_id_list(profile_list);
 
    Json::Value get_profiles_status_json = Json::objectValue;

    for (auto &profile_id : profile_list) {
        string j = get_profile_state_as_string(profile_id);
        std::stringstream ss;
        ss << profile_id;
        get_profiles_status_json[ss.str()] = j; 
    }
 
    result =  get_profiles_status_json;
}


/**
 * returns profile state formatted as string
 * 
 */
std::string
TrexStatelessPort::get_profile_state_as_string(string profile_id) {

    switch (get_profile_state(profile_id)) {
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
 
        case PORT_STATE_ASTF_LOADED:
            return "ASTF_LOADED";
 
        case PORT_STATE_ASTF_PARSE:
            return "ASTF_PARSE";
 
        case PORT_STATE_ASTF_BUILD:
            return "ASTF_BUILD";
 
        case PORT_STATE_ASTF_CLEANUP:
            return "ASTF_CLEANUP";
 
    }
    return "UNKNOWN";
}



/**
 * update port state with all profiles state 
 */
void
TrexStatelessPort::update_port_state() {

    
    if (get_state() & (PORT_STATE_TX | PORT_STATE_PAUSE | PORT_STATE_STREAMS | PORT_STATE_IDLE)) {

        int temp_state = 0;
        for (auto it : m_profile_table) {
            TrexStatelessProfile *mprofile = it.second;
            temp_state |= mprofile->get_state();
        }

        if (temp_state & PORT_STATE_TX) {
            change_state(TrexPort::PORT_STATE_TX);
        } else if (temp_state &  PORT_STATE_PAUSE) {
            change_state(TrexPort::PORT_STATE_PAUSE);
        } else if (temp_state & PORT_STATE_STREAMS) {
            change_state(TrexPort::PORT_STATE_STREAMS);
        } else if (temp_state & PORT_STATE_IDLE ) {
            change_state(TrexPort::PORT_STATE_IDLE);
        }
    }
}

/**
 * update and publish port state 
 */
void
TrexStatelessPort::update_and_publish_port_state(bool async) {

    int old_state = get_state();

    update_port_state();
    if (old_state == get_state()) {
        return;
    }

    Json::Value data;
    data["port_id"] = m_port_id;
    if ( (get_state() == PORT_STATE_TX) && (old_state == PORT_STATE_STREAMS)) {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_STARTED, data);
    } else if ( get_state() == PORT_STATE_PAUSE ) {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_PAUSED, data);
    } else if ((get_state() == PORT_STATE_TX) && (old_state == PORT_STATE_PAUSE)) {
        get_stateless_obj()->get_publisher()->publish_event(TrexPublisher::EVENT_PORT_RESUMED, data);
    } else if ((get_state() == PORT_STATE_STREAMS) && (old_state != PORT_STATE_IDLE)) {
        common_port_stop_actions(async);
    }
}


void
TrexStatelessPort::valide_profile(string profile_id) {

    if (!get_profile_by_id(profile_id)) {
        std::stringstream ss;
        ss << "profile_id " << profile_id << " does not exists";
        throw TrexException(ss.str());
    }
}


/* ********************************* 
 * profile table                      
 * ******************************* */
TrexProfileTable::TrexProfileTable() {

}

TrexProfileTable::~TrexProfileTable() {

    for (auto mprofile : m_profile_table) {
        delete mprofile.second;
    }   
}

void TrexProfileTable::add_profile(TrexStatelessProfile *mprofile) {

    TrexStatelessProfile *old_profile = get_profile_by_id(mprofile->m_profile_id);
    if (!old_profile) {
        m_profile_table[mprofile->m_profile_id] = mprofile;
    }
    delete old_profile;
}


TrexStatelessProfile * TrexProfileTable::get_profile_by_id(string profile_id) {

    auto search = m_profile_table.find(profile_id);

    if (search != m_profile_table.end()) {
        return search->second;
    } else {
        return NULL;
    }
}


void TrexProfileTable::get_profile_id_list(std::vector<string> &profile_id_list) {

    profile_id_list.clear();

    for (auto mprofile : m_profile_table) {
        //if(( mprofile.first  == "_") && (mprofile.second->get_state()==(TrexPort::PORT_STATE_IDLE)))
        //continue;
        profile_id_list.push_back(mprofile.first);
    }
}

void TrexProfileTable::get_profile_object_list(std::vector<TrexStatelessProfile *> &profile_object_list) {

    profile_object_list.clear();

    for (auto mprofile : m_profile_table) {
        profile_object_list.push_back(mprofile.second);
    }
}

int TrexProfileTable::size() {
    return m_profile_table.size();
}

// ** end of profile table ** //



