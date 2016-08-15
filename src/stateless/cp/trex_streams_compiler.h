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
#ifndef __TREX_STREAMS_COMPILER_H__
#define __TREX_STREAMS_COMPILER_H__

#include <stdint.h>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include "trex_exception.h"

class TrexStreamsCompiler;
class TrexStream;
class GraphNodeMap;

/**
 * a mask object passed to compilation
 */
class TrexDPCoreMask {

public:


    TrexDPCoreMask(uint8_t dp_core_count, uint64_t dp_core_mask = UINT64_MAX) {
        assert(is_valid_mask(dp_core_count, dp_core_mask));

        m_dp_core_count = dp_core_count;
        m_dp_core_mask  = dp_core_mask;

        /* create a vector of all the active cores */
        for (int i = 0; i < m_dp_core_count; i++) {
            if (is_core_active(i)) {
                m_active_cores.push_back(i);
            }
        }
    }

   
    uint8_t get_total_count() const {
        return m_dp_core_count;
    }

    uint8_t get_active_count() const {
        return m_active_cores.size();
    }

    bool is_core_active(uint8_t core_id) const {
        assert(core_id < m_dp_core_count);
        return ( (1 << core_id) & m_dp_core_mask );
    }

    bool is_core_disabled(uint8_t core_id) const {
        return (!is_core_active(core_id));
    }

    const std::vector<uint8_t> & get_active_cores() const {
        return m_active_cores;
    }

    static bool is_valid_mask(uint8_t dp_core_count, uint64_t dp_core_mask) {
        if ( (dp_core_count < 1) || (dp_core_count > 64) ) {
            return false;
        }
        /* highest bit pushed to left and then -1 will give all the other bits on */
        return ( (dp_core_mask & ( (1 << dp_core_count) - 1 ) ) != 0);
    }

private:

    uint8_t   m_dp_core_count;
    uint64_t  m_dp_core_mask;

    std::vector<uint8_t> m_active_cores;

public:
    static const uint64_t MASK_ALL = UINT64_MAX;

};


/**
 * compiled object for a table of streams
 * 
 * @author imarom (28-Oct-15)
 */
class TrexStreamsCompiledObj {
    friend class TrexStreamsCompiler;

public:

    TrexStreamsCompiledObj(uint8_t port_id);
    ~TrexStreamsCompiledObj();

    struct obj_st {

        TrexStream * m_stream;
    };

    const std::vector<obj_st> & get_objects() {
        return m_objs;
    }

    uint8_t get_port_id(){
        return (m_port_id);
    }

    bool get_all_streams_continues(){
        return (m_all_continues);
    }

    void Dump(FILE *fd);

    TrexStreamsCompiledObj* clone();

    bool is_empty() {
        return (m_objs.size() == 0);
    }

private:
    void add_compiled_stream(TrexStream *stream);


    std::vector<obj_st> m_objs;

    bool    m_all_continues;
    uint8_t m_port_id;
};

class TrexStreamsCompiler {
public:

    /**
     * compiles a vector of streams to an object passable to the DP
     * 
     * @author imarom (28-Oct-15)
     * 
     */
    bool compile(uint8_t                                port_id,
                 const std::vector<TrexStream *>        &streams,
                 std::vector<TrexStreamsCompiledObj *>  &objs,
                 const TrexDPCoreMask                   &core_mask = 1,
                 double                                 factor = 1.0,
                 std::string                            *fail_msg = NULL);


    /**
     * 
     * returns a reference pointer to the last compile warnings
     * if no warnings were produced - the vector is empty
     */
    const std::vector<std::string> & get_last_compile_warnings() {
        return m_warnings;
    }

private:

    bool compile_internal(uint8_t                                port_id,
                          const std::vector<TrexStream *>        &streams,
                          std::vector<TrexStreamsCompiledObj *>  &objs,
                          uint8_t                                dp_core_count,
                          double                                 factor,
                          std::string                            *fail_msg);

    void pre_compile_check(const std::vector<TrexStream *> &streams,
                           GraphNodeMap & nodes);
    void allocate_pass(const std::vector<TrexStream *> &streams, GraphNodeMap *nodes);
    void direct_pass(GraphNodeMap *nodes);
    void check_for_unreachable_streams(GraphNodeMap *nodes);
    void check_stream(const TrexStream *stream);
    void add_warning(const std::string &warning);
    void err(const std::string &err);

    void compile_on_single_core(uint8_t                                port_id,
                                const std::vector<TrexStream *>        &streams,
                                std::vector<TrexStreamsCompiledObj *>  &objs,
                                uint8_t                                dp_core_count,
                                double                                 factor,
                                GraphNodeMap                           &nodes,
                                bool                                   all_continues);

    void compile_on_all_cores(uint8_t                                port_id,
                              const std::vector<TrexStream *>        &streams,
                              std::vector<TrexStreamsCompiledObj *>  &objs,
                              uint8_t                                dp_core_count,
                              double                                 factor,
                              GraphNodeMap                           &nodes,
                              bool                                   all_continues);


    void compile_stream(const TrexStream *stream,
                        double factor,
                        uint8_t dp_core_count,
                        std::vector<TrexStreamsCompiledObj *> &objs,
                        GraphNodeMap &nodes);

    void compile_stream_on_single_core(TrexStream *stream,
                                       uint8_t dp_core_count,
                                       std::vector<TrexStreamsCompiledObj *> &objs,
                                       int new_id,
                                       int new_next_id);

    void compile_stream_on_all_cores(TrexStream *stream,
                                     uint8_t dp_core_count,
                                     std::vector<TrexStreamsCompiledObj *> &objs,
                                     int new_id,
                                     int new_next_id);

    std::vector<std::string> m_warnings;
};

class TrexStreamsGraph;

/* describes a bandwidth point */
class BW {
public:

    BW() {
        m_pps    = 0;
        m_bps_l2 = 0;
        m_bps_l1 = 0;
    }

    BW(double pps, double bps_l2, double bps_l1) {
        m_pps    = pps;
        m_bps_l2 = bps_l2;
        m_bps_l1 = bps_l1;

    }

    BW& operator+= (const BW &other) {
        m_pps    += other.m_pps;
        m_bps_l1 += other.m_bps_l1;
        m_bps_l2 += other.m_bps_l2;

        return *this;
    }
    
  
    double m_pps;
    double m_bps_l1;
    double m_bps_l2;

};

/* there are two temp copies here - it is known... */
static inline BW operator+ (BW lhs, const BW &rhs) {
    lhs += rhs;
    return lhs;
}

/**************************************
 * streams graph object 
 *  
 * holds the step graph for bandwidth 
 *************************************/
class TrexStreamsGraphObj {
    friend class TrexStreamsGraph;

public:

    TrexStreamsGraphObj() {
        m_expected_duration = 0;
    }

    /**
     * rate event is defined by those: 
     * time - the time of the event on the timeline 
     * diff - what is the nature of the change ? 
     * 
     * @author imarom (23-Nov-15)
     */
    struct rate_event_st {
        double   time;
        double   diff_pps;
        double   diff_bps_l1;
        double   diff_bps_l2;
        uint32_t stream_id;
    };

    double get_max_pps(double factor = 1) const {
        return (m_var.m_pps * factor + m_fixed.m_pps);
    }

    double get_max_bps_l1(double factor = 1) const {
        return (m_var.m_bps_l1 * factor + m_fixed.m_bps_l1);
    }

    double get_max_bps_l2(double factor = 1) const {
        return (m_var.m_bps_l2 * factor + m_fixed.m_bps_l2);
    }

    double get_factor_pps(double req_pps) const {
        if ( (req_pps - m_fixed.m_pps) <= 0 )  {
            std::stringstream ss;
            ss << "current stream configuration enforces a minimum rate of '" << m_fixed.m_pps << "' pps";
            throw TrexException(ss.str());
        }

        return ( (req_pps - m_fixed.m_pps) / m_var.m_pps );
    }

    double get_factor_bps_l1(double req_bps_l1) const {
        if ( (req_bps_l1 - m_fixed.m_bps_l1) <= 0 )  {
            std::stringstream ss;
            ss << "current stream configuration enforces a minimum rate of '" << m_fixed.m_bps_l1 << "' BPS L1";
            throw TrexException(ss.str());
        }

        return ( (req_bps_l1 - m_fixed.m_bps_l1) / m_var.m_bps_l1 );
    }

    double get_factor_bps_l2(double req_bps_l2) const {
        if ( (req_bps_l2 - m_fixed.m_bps_l2) <= 0 )  {
            std::stringstream ss;
            ss << "current stream configuration enforces a minimum rate of '" << m_fixed.m_bps_l2 << "' BPS L2";
            throw TrexException(ss.str());
        }

        return ( (req_bps_l2 - m_fixed.m_bps_l2) / m_var.m_bps_l2 );
    }

    int get_duration() const {
        return m_expected_duration;
    }

    const std::list<rate_event_st> & get_events() const {
        return m_rate_events;
    }


private:

  
    void on_loop_detection() {
        m_expected_duration = -1;
    }

    void add_rate_event(const rate_event_st &ev) {
        m_rate_events.push_back(ev);
    }

    void add_fixed_rate(const BW &bw) {
        m_fixed += bw;
    }

    void generate();
    void find_max_rate();

    /* max variable BW in the graph */
    BW m_var;

    /* graph might contain fixed rate traffic (DC traffic such as latency) */
    BW m_fixed;

    /* total consists of fixed rate + variable rate*/
    BW m_total;

    int     m_expected_duration;

    /* list of rate events */
    std::list<rate_event_st> m_rate_events;

};

/**
 * graph creator 
 * 
 * @author imarom (23-Nov-15)
 */
class TrexStreamsGraph {
public:

    TrexStreamsGraph() {
        m_graph_obj = NULL;
    }

    /**
     * generate a sequence graph for streams
     * 
     */
    const TrexStreamsGraphObj * generate(const std::vector<TrexStream *> &streams);

private:

    void generate_graph_for_one_root(uint32_t root_stream_id);

    void add_rate_events_for_stream(double &offset, TrexStream *stream);
    void add_rate_events_for_stream_cont(double &offset_usec, TrexStream *stream);
    void add_rate_events_for_stream_single_burst(double &offset_usec, TrexStream *stream);
    void add_rate_events_for_stream_multi_burst(double &offset_usec, TrexStream *stream);

    /* for fast processing of streams */
    std::unordered_map<uint32_t, TrexStream *> m_streams_hash;

    /* main object to hold the graph - returned to the user */
    TrexStreamsGraphObj *m_graph_obj;
};

#endif /* __TREX_STREAMS_COMPILER_H__ */
