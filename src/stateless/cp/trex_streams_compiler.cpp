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

#include <string>
#include <sstream>
#include <trex_streams_compiler.h>
#include <trex_stream.h>
#include <assert.h>
#include <trex_stateless.h>
#include <iostream>

/**
 * describes a graph node in the pre compile check
 * 
 * @author imarom (16-Nov-15)
 */
class GraphNode {
public:
    GraphNode(const TrexStream *stream, GraphNode *next) : m_stream(stream), m_next(next) {
        m_marked   = false;
        m_compressed_stream_id=-1;

    }

    uint32_t get_stream_id() const {
        return m_stream->m_stream_id;
    }

    uint32_t get_next_stream_id() const {
        return m_stream->m_next_stream_id;

    }

    const TrexStream *m_stream;
    GraphNode *m_next;
    std::vector<const GraphNode *> m_parents;
    bool m_marked;
    int m_compressed_stream_id;
};

/**
 * node map
 * 
 */
class GraphNodeMap {
public:

    GraphNodeMap() : m_dead_end(NULL, NULL) {

    }

    bool add(GraphNode *node) {
        if (has(node->get_stream_id())) {
            return false;
        }

        m_nodes[node->get_stream_id()] = node;

        if (node->m_stream->m_self_start) {
            m_roots.push_back(node);
        }

        return true;
    }

    bool has(uint32_t stream_id) {

        return (get(stream_id) != NULL);
    }

    GraphNode * get(uint32_t stream_id) {

        if (stream_id == -1) {
            return &m_dead_end;
        }

        auto search = m_nodes.find(stream_id);

        if (search != m_nodes.end()) {
            return search->second;
        } else {
            return NULL;
        }
    }

    void clear_marks() {
        for (auto node : m_nodes) {
            node.second->m_marked = false;
        }
    }

    void get_unmarked(std::vector <GraphNode *> &unmarked) {
        for (auto node : m_nodes) {
            if (!node.second->m_marked) {
                unmarked.push_back(node.second);
            }
        }
    }


    ~GraphNodeMap() {
        for (auto node : m_nodes) {
            delete node.second;
        }
        m_nodes.clear();
    }

    std::vector <GraphNode *> & get_roots() {
        return m_roots;
    }


    std::unordered_map<uint32_t, GraphNode *> get_nodes() {
        return m_nodes;
    }

private:
    std::unordered_map<uint32_t, GraphNode *> m_nodes;
    std::vector <GraphNode *> m_roots;
    GraphNode m_dead_end;
};


/**************************************
 * stream compiled object
 *************************************/
TrexStreamsCompiledObj::TrexStreamsCompiledObj(uint8_t port_id, double mul) : m_port_id(port_id), m_mul(mul) {
      m_all_continues=false;
}

TrexStreamsCompiledObj::~TrexStreamsCompiledObj() {
    for (auto obj : m_objs) {
        delete obj.m_stream;
    }
    m_objs.clear();
}


void 
TrexStreamsCompiledObj::add_compiled_stream(TrexStream * stream){

    obj_st obj;

    obj.m_stream = stream->clone_as_dp();

    m_objs.push_back(obj);
}

void 
TrexStreamsCompiledObj::add_compiled_stream(TrexStream * stream,
                                            uint32_t my_dp_id, int next_dp_id) {
    obj_st obj;

    obj.m_stream = stream->clone_as_dp();
    /* compress the id's*/
    obj.m_stream->fix_dp_stream_id(my_dp_id,next_dp_id);

    m_objs.push_back(obj);
}

void TrexStreamsCompiledObj::Dump(FILE *fd){
    for (auto obj : m_objs) {
        obj.m_stream->Dump(fd);
    }
}



TrexStreamsCompiledObj *
TrexStreamsCompiledObj::clone() {

    /* use multiplier of 1 to avoid double mult */
    TrexStreamsCompiledObj *new_compiled_obj = new TrexStreamsCompiledObj(m_port_id, 1);

    /**
     * clone each element
     */
    for (auto obj : m_objs) {
        new_compiled_obj->add_compiled_stream(obj.m_stream);
    }

    new_compiled_obj->m_mul = m_mul;

    return new_compiled_obj;
}

void
TrexStreamsCompiler::add_warning(const std::string &warning) {
    m_warnings.push_back("*** warning: " + warning);
}

void
TrexStreamsCompiler::err(const std::string &err) {
    throw TrexException("*** error: " + err);
}

void
TrexStreamsCompiler::check_stream(const TrexStream *stream) {
    std::stringstream ss;

    /* cont. stream can point only on itself */
    if (stream->get_type() == TrexStream::stCONTINUOUS) {
        if (stream->m_next_stream_id != -1) {
            ss << "continous stream '" << stream->m_stream_id << "' cannot point on another stream";
            err(ss.str());
        }
    }
}

void
TrexStreamsCompiler::allocate_pass(const std::vector<TrexStream *> &streams,
                                   GraphNodeMap *nodes) {
    std::stringstream ss;
    uint32_t compressed_stream_id=0;


    /* first pass - allocate all nodes and check for duplicates */
    for (auto stream : streams) {

        /* skip non enabled streams */
        if (!stream->m_enabled) {
            continue;
        }

        /* sanity check on the stream itself */
        check_stream(stream);

        /* duplicate stream id ? */
        if (nodes->has(stream->m_stream_id)) {
            ss << "duplicate instance of stream id " << stream->m_stream_id;
            err(ss.str());
        }

        GraphNode *node = new GraphNode(stream, NULL);
        /* allocate new compressed id */
        node->m_compressed_stream_id = compressed_stream_id;

        compressed_stream_id++;

        /* add to the map */
        assert(nodes->add(node));
    }

}

/**
 * on this pass we direct the graph to point to the right nodes
 * 
 */
void
TrexStreamsCompiler::direct_pass(GraphNodeMap *nodes) {
   
    /* second pass - direct the graph */
    for (auto p : nodes->get_nodes()) {

        GraphNode *node = p.second;
        const TrexStream *stream = node->m_stream;

        /* check the stream points on an existing stream */
        GraphNode *next_node = nodes->get(stream->m_next_stream_id);
        if (!next_node) {
            std::stringstream ss;
            ss << "stream " << node->get_stream_id() << " is pointing on non existent stream " << stream->m_next_stream_id;
            err(ss.str());
        }

        node->m_next = next_node;

        /* do we have more than one parent ? */
        next_node->m_parents.push_back(node);
    }


    /* check for multiple parents */
    for (auto p : nodes->get_nodes()) {
        GraphNode *node = p.second;

        if (node->m_parents.size() > 0 ) {
            std::stringstream ss;

            ss << "stream " << node->get_stream_id() << " is triggered by multiple streams: ";
            for (auto x : node->m_parents) {
                ss << x->get_stream_id() << " ";
            }

            add_warning(ss.str());
        }
    }
}

/**
 * mark sure all the streams are reachable
 * 
 */
void
TrexStreamsCompiler::check_for_unreachable_streams(GraphNodeMap *nodes) {
    /* start with the roots */
    std::vector <GraphNode *> next_nodes = nodes->get_roots();


    nodes->clear_marks();

    /* run BFS from all the roots */
    while (!next_nodes.empty()) {

        /* pull one */
        GraphNode *node = next_nodes.back();
        next_nodes.pop_back();
        if (node->m_marked) {
            continue;
        }

        node->m_marked = true;

        if (node->m_next != NULL) {
            next_nodes.push_back(node->m_next);
        }

    }

    std::vector <GraphNode *> unmarked;
    nodes->get_unmarked(unmarked);

    if (!unmarked.empty()) {
        std::stringstream ss;
        for (auto node : unmarked) {
            ss << "stream " << node->get_stream_id() << " is unreachable from any other stream\n";
        }
        err(ss.str());
    }


}

/**
 * check validation of streams for compile
 * 
 * @author imarom (16-Nov-15)
 * 
 * @param streams 
 * @param fail_msg 
 * 
 * @return bool 
 */
void
TrexStreamsCompiler::pre_compile_check(const std::vector<TrexStream *> &streams,
                                       GraphNodeMap & nodes) {

    m_warnings.clear();

    /* allocate nodes */
    allocate_pass(streams, &nodes);

    /* direct the graph */
    direct_pass(&nodes);

    /* check for non reachable streams inside the graph */
    check_for_unreachable_streams(&nodes);

}

/**************************************
 * stream compiler
 *************************************/
bool 
TrexStreamsCompiler::compile(const std::vector<TrexStream *> &streams,
                             TrexStreamsCompiledObj &obj,
                             std::string *fail_msg) {

#if 0
    fprintf(stdout,"------------pre compile \n");
    for (auto stream : streams) {
        stream->Dump(stdout);
    }
    fprintf(stdout,"------------pre compile \n");
#endif

    GraphNodeMap nodes;


    /* compile checks */
    try {
        pre_compile_check(streams,nodes);
    } catch (const TrexException &ex) {
        if (fail_msg) {
            *fail_msg = ex.what();
        } else {
            std::cout << ex.what();
        }
        return false;
    }


    bool all_continues=true;
    /* for now we do something trivial, */
    for (auto stream : streams) {

        /* skip non-enabled streams */
        if (!stream->m_enabled) {
            continue;
        }
        if (stream->get_type() != TrexStream::stCONTINUOUS ) {
            all_continues=false;
        }

        int new_id= nodes.get(stream->m_stream_id)->m_compressed_stream_id;
        assert(new_id>=0);
        uint32_t my_stream_id = (uint32_t)new_id;
        int my_next_stream_id=-1;
        if (stream->m_next_stream_id>=0) {
            my_next_stream_id=nodes.get(stream->m_next_stream_id)->m_compressed_stream_id;
        }

        /* add it */
        obj.add_compiled_stream(stream,
                                my_stream_id,
                                my_next_stream_id
                                );
        
    }
    obj.m_all_continues =all_continues;
    return true;
}

/**************************************
 * streams graph
 *************************************/

/**
 * for each stream we create the right rate events (up/down)
 * 
 * @author imarom (24-Nov-15)
 * 
 * @param offset_usec 
 * @param stream 
 */
void
TrexStreamsGraph::add_rate_events_for_stream(double &offset_usec, const TrexStream *stream) {

    switch (stream->get_type()) {
   
    case TrexStream::stCONTINUOUS:
        add_rate_events_for_stream_cont(offset_usec, stream);
        return;
        
    case TrexStream::stSINGLE_BURST:
        add_rate_events_for_stream_single_burst(offset_usec, stream);
        return;

    case TrexStream::stMULTI_BURST:
        add_rate_events_for_stream_multi_burst(offset_usec, stream);
        return;
    }
}

/**
 * continous stream
 * 
 */
void
TrexStreamsGraph::add_rate_events_for_stream_cont(double &offset_usec, const TrexStream *stream) {

    TrexStreamsGraphObj::rate_event_st start_event;

    /* for debug purposes */
    start_event.stream_id = stream->m_stream_id;

    start_event.time = offset_usec + stream->m_isg_usec;
    start_event.diff_pps = stream->get_pps();
    start_event.diff_bps = stream->get_bps();
    m_graph_obj.add_rate_event(start_event);

    /* no more events after this stream */
    offset_usec = -1;
}

/**
 * single burst stream
 * 
 */
void
TrexStreamsGraph::add_rate_events_for_stream_single_burst(double &offset_usec, const TrexStream *stream) {
    TrexStreamsGraphObj::rate_event_st start_event;
    TrexStreamsGraphObj::rate_event_st stop_event;


    /* for debug purposes */
    start_event.stream_id = stream->m_stream_id;
    stop_event.stream_id   = stream->m_stream_id;

     /* start event */
    start_event.time = offset_usec + stream->m_isg_usec;
    start_event.diff_pps = stream->get_pps();
    start_event.diff_bps = stream->get_bps();
    m_graph_obj.add_rate_event(start_event);

    /* stop event */
    stop_event.time = start_event.time + stream->get_burst_length_usec();
    stop_event.diff_pps = -(start_event.diff_pps);
    stop_event.diff_bps = -(start_event.diff_bps);
    m_graph_obj.add_rate_event(stop_event);

    /* next stream starts from here */
    offset_usec = stop_event.time;

}

/**
 * multi burst stream
 * 
 */
void
TrexStreamsGraph::add_rate_events_for_stream_multi_burst(double &offset_usec, const TrexStream *stream) {
    TrexStreamsGraphObj::rate_event_st start_event;
    TrexStreamsGraphObj::rate_event_st stop_event;

    /* first the delay is the inter stream gap */
    double delay = stream->m_isg_usec;

    /* for debug purposes */
    
    start_event.diff_pps   = stream->get_pps();
    start_event.diff_bps   = stream->get_bps();
    start_event.stream_id  = stream->m_stream_id;

    stop_event.diff_pps    = -(start_event.diff_pps);
    stop_event.diff_bps    = -(start_event.diff_bps);
    stop_event.stream_id   = stream->m_stream_id;

    /* for each burst create up/down events */
    for (int i = 0; i < stream->m_num_bursts; i++) {

        start_event.time = offset_usec + delay;
        m_graph_obj.add_rate_event(start_event);

        stop_event.time = start_event.time + stream->get_burst_length_usec();
        m_graph_obj.add_rate_event(stop_event);

        /* after the first burst, the delay is inter burst gap */
        delay = stream->m_ibg_usec;

        offset_usec = stop_event.time;
    }
}

/**
 * for a single root we can until done or a loop detected
 * 
 * @author imarom (24-Nov-15)
 * 
 * @param root_stream_id 
 */
void
TrexStreamsGraph::generate_graph_for_one_root(uint32_t root_stream_id) {

    std::unordered_map<uint32_t, bool> loop_hash;
    std::stringstream ss;
    
    uint32_t stream_id = root_stream_id;
    double offset = 0;

    while (true) {
        const TrexStream *stream;
        
        /* fetch the stream from the hash - if it is not present, report an error */
        try {
            stream = m_streams_hash.at(stream_id);
        } catch (const std::out_of_range &e) {
            ss << "stream id " << stream_id << " does not exists";
            throw TrexException(ss.str());
        }

        /* add the node to the hash for loop detection */
        loop_hash[stream_id] = true;

        /* create the right rate events for the stream */
        add_rate_events_for_stream(offset, stream);

        /* do we have a next stream ? */
        if (stream->m_next_stream_id == -1) {
            break;
        }

        /* loop detection */
        auto search = loop_hash.find(stream->m_next_stream_id);
        if (search != loop_hash.end()) {
            break;
        }

        /* handle the next one */
        stream_id = stream->m_next_stream_id;
    }
}

/**
 * for a vector of streams generate a graph of BW 
 * see graph object for more details 
 * 
 */
const TrexStreamsGraphObj &
TrexStreamsGraph::generate(const std::vector<TrexStream *> &streams) {
    std::vector <uint32_t> root_streams;

    /* before anything we create a hash streams ID
       and grab the root nodes
     */
    for (TrexStream *stream : streams) {

        /* skip non enabled streams */
        if (!stream->m_enabled) {
            continue;
        }

        /* for fast search we populate all the streams in a hash */        
        m_streams_hash[stream->m_stream_id] = stream;

        /* hold all the self start nodes in a vector */
        if (stream->m_self_start) {
            root_streams.push_back(stream->m_stream_id);
        }
    }

    /* for each node - scan until done or loop */
    for (uint32_t root_id : root_streams) {
        generate_graph_for_one_root(root_id);
    }


    m_graph_obj.generate();

    return m_graph_obj;
}

/**************************************
 * streams graph object
 *************************************/
void
TrexStreamsGraphObj::find_max_rate() {
    double max_rate_pps = 0;
    double current_rate_pps = 0;

    double max_rate_bps = 0;
    double current_rate_bps = 0;

    /* now we simply walk the list and hold the max */
    for (auto &ev : m_rate_events) {

        current_rate_pps += ev.diff_pps;
        current_rate_bps += ev.diff_bps;

        max_rate_pps = std::max(max_rate_pps, current_rate_pps);
        max_rate_bps = std::max(max_rate_bps, current_rate_bps);
    }

    m_max_pps = max_rate_pps;
    m_max_bps = max_rate_bps;
}

static 
bool event_compare (const TrexStreamsGraphObj::rate_event_st &first, const TrexStreamsGraphObj::rate_event_st &second) {
    return (first.time < second.time);
}

void
TrexStreamsGraphObj::generate() {
    m_rate_events.sort(event_compare);
    find_max_rate();
}

