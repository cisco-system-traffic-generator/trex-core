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
#include <string>

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

#endif /* __TREX_STREAMS_COMPILER_H__ */
