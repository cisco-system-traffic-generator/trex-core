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

class TrexStreamsCompiler;
class TrexStream;

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
        double   m_pps;
        uint8_t *m_pkt;
        uint16_t m_pkt_len;
        uint8_t  m_port_id;
    };

    const std::vector<obj_st> & get_objects() {
        return m_objs;
    }

private:
    void add_compiled_stream(double pps, uint8_t *pkt, uint16_t pkt_len);
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
    bool compile(const std::vector<TrexStream *> &streams, TrexStreamsCompiledObj &obj);
};

#endif /* __TREX_STREAMS_COMPILER_H__ */
