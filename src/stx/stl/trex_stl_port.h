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
#ifndef __TREX_STL_PORT_H__
#define __TREX_STL_PORT_H__

#include "common/basic_utils.h"

/* common files */
#include "trex_port.h"
#include "trex_rx_defs.h"
#include "trex_exception.h"
#include "trex_capture.h"

/* STL files */
#include "trex_stl_stream.h"

class TrexCpToDpMsgBase;
class TrexCpToRxMsgBase;
class TrexStreamsGraphObj;
class TrexPortMultiplier;
class TrexPktBuffer;



class AsyncStopEvent;

/**
 * describes a stateless port
 *
 * @author imarom (31-Aug-15)
 */
class TrexStatelessPort : public TrexPort {
    
    friend TrexDpPortEvents;
    friend TrexDpPortEvent;
    friend AsyncStopEvent;

public:

    TrexStatelessPort(uint8_t port_id);

    ~TrexStatelessPort();

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
    void start_traffic(const TrexPortMultiplier &mul, double duration, bool force = false, uint64_t core_mask = UINT64_MAX, double start_at_ts = 0);

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
    void pause_streams(stream_ids_t &stream_ids);

    /**
     * resume traffic
     * throws TrexException in case of an error
     */
    void resume_traffic(void);
    void resume_streams(stream_ids_t &stream_ids);

    /**
     * update current traffic on port
     *
     */
    void update_traffic(const TrexPortMultiplier &mul, bool force);
    void update_streams(const TrexPortMultiplier &mul, bool force, std::vector<TrexStream *> &streams);

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
     * sets service mode
     * 
     * @author imarom (1/22/2017)
     * 
     * @param enabled 
     */
    void set_service_mode(bool enabled);
    
    bool is_service_mode_on() const {
        return m_is_service_mode_on;
    }

    bool is_running_flow_stats();
    bool has_flow_stats();

    /**
     * the the max stream id currently assigned
     *
     */
    int get_max_stream_id() const;

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

    
    /**
     * returns the traffic multiplier currently being used by the DP
     *
     */
    double get_multiplier() {
        return (m_factor);
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
    void set_service_mode_on();
    void set_service_mode_off();

    
    /**
     * when a port stops, perform various actions
     *
     */
    void common_port_stop_actions(bool async);

    /**
     * calculate effective M per core
     *
     */
    double calculate_effective_factor(const TrexPortMultiplier &mul, bool force, const TrexStreamsGraphObj &graph_obj);
    double calculate_effective_factor_internal(const TrexPortMultiplier &mul, const TrexStreamsGraphObj &graph_obj);


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

    bool               m_last_all_streams_continues;
    double             m_last_duration;
    double             m_factor;
    uint32_t           m_non_explicit_dst_macs_count;
    uint32_t           m_flow_stats_count;

    /* holds a graph of streams rate*/
    const TrexStreamsGraphObj  *m_graph_obj;

    bool  m_is_service_mode_on;
    
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

#endif /* __TREX_STL_PORT_H__ */

