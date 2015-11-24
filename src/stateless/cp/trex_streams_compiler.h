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

class TrexStreamsCompiler;
class TrexStream;
class GraphNodeMap;

/**
 * compiled object for a table of streams
 * 
 * @author imarom (28-Oct-15)
 */
class TrexStreamsCompiledObj {
    friend class TrexStreamsCompiler;
public:

    TrexStreamsCompiledObj(uint8_t port_id, double m_mul);
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

    /**
     * clone the compiled object
     * 
     */
    TrexStreamsCompiledObj * clone();

    double get_multiplier(){
        return (m_mul);
    }

    void Dump(FILE *fd);

private:
    void add_compiled_stream(TrexStream * stream,
                             uint32_t my_dp_id, int next_dp_id);
    void add_compiled_stream(TrexStream * stream);

    std::vector<obj_st> m_objs;

    uint8_t m_port_id;
    double  m_mul;
};

class TrexStreamsCompiler {
public:

    /**
     * compiles a vector of streams to an object passable to the DP
     * 
     * @author imarom (28-Oct-15)
     * 
     */
    bool compile(const std::vector<TrexStream *> &streams, TrexStreamsCompiledObj &obj, std::string *fail_msg = NULL);

    /**
     * 
     * returns a reference pointer to the last compile warnings
     * if no warnings were produced - the vector is empty
     */
    const std::vector<std::string> & get_last_compile_warnings() {
        return m_warnings;
    }

private:

    void pre_compile_check(const std::vector<TrexStream *> &streams,
                           GraphNodeMap & nodes);
    void allocate_pass(const std::vector<TrexStream *> &streams, GraphNodeMap *nodes);
    void direct_pass(GraphNodeMap *nodes);
    void check_for_unreachable_streams(GraphNodeMap *nodes);
    void check_stream(const TrexStream *stream);
    void add_warning(const std::string &warning);
    void err(const std::string &err);

    std::vector<std::string> m_warnings;
    
};

class TrexStreamsGraph;

/**************************************
 * streams graph object 
 *  
 * holds the step graph for bandwidth 
 *************************************/
class TrexStreamsGraphObj {
    friend class TrexStreamsGraph;

public:

    /**
     * rate event is defined by those: 
     * time - the time of the event on the timeline 
     * diff - what is the nature of the change ? 
     * 
     * @author imarom (23-Nov-15)
     */
    struct rate_event_st {
        double time;
        double diff_pps;
        double diff_bps;
        uint32_t stream_id;
    };

    double get_max_pps() const {
        return m_max_pps;
    }

    double get_max_bps() const {
        return m_max_bps;
    }

    const std::list<rate_event_st> & get_events() const {
        return m_rate_events;
    }

private:

    void add_rate_event(const rate_event_st &ev) {
        m_rate_events.push_back(ev);
    }

    void generate();
    void find_max_rate();

    double m_max_pps;
    double m_max_bps;

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

    /**
     * generate a sequence graph for streams
     * 
     */
    const TrexStreamsGraphObj & generate(const std::vector<TrexStream *> &streams);

private:

    void generate_graph_for_one_root(uint32_t root_stream_id);

    void add_rate_events_for_stream(double &offset, const TrexStream *stream);
    void add_rate_events_for_stream_cont(double &offset_usec, const TrexStream *stream);
    void add_rate_events_for_stream_single_burst(double &offset_usec, const TrexStream *stream);
    void add_rate_events_for_stream_multi_burst(double &offset_usec, const TrexStream *stream);

    /* for fast processing of streams */
    std::unordered_map<uint32_t, const TrexStream *> m_streams_hash;

    /* main object to hold the graph - returned to the user */
    TrexStreamsGraphObj m_graph_obj;
};

#endif /* __TREX_STREAMS_COMPILER_H__ */
