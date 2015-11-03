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

#include <string.h>
#include <trex_streams_compiler.h>
#include <trex_stream.h>

/**************************************
 * stream compiled object
 *************************************/
TrexStreamsCompiledObj::TrexStreamsCompiledObj(uint8_t port_id, double mul) : m_port_id(port_id), m_mul(mul) {
}

TrexStreamsCompiledObj::~TrexStreamsCompiledObj() {
    for (auto &obj : m_objs) {
        delete obj.m_pkt;
    }
    m_objs.clear();
}

void 
TrexStreamsCompiledObj::add_compiled_stream(double isg_usec, double pps, uint8_t *pkt, uint16_t pkt_len) {
    obj_st obj;

    obj.m_isg_usec  = isg_usec;
    obj.m_port_id   = m_port_id;
    obj.m_pps       = pps * m_mul;
    obj.m_pkt_len   = pkt_len;

    obj.m_pkt = new uint8_t[pkt_len];
    memcpy(obj.m_pkt, pkt, pkt_len);

    m_objs.push_back(obj);
}

TrexStreamsCompiledObj *
TrexStreamsCompiledObj::clone() {

    TrexStreamsCompiledObj *new_compiled_obj = new TrexStreamsCompiledObj(m_port_id, m_mul);

    /**
     * clone each element
     */
    for (auto obj : m_objs) {
        new_compiled_obj->add_compiled_stream(obj.m_isg_usec,
                                              obj.m_pps,
                                              obj.m_pkt,
                                              obj.m_pkt_len);
    }

    return new_compiled_obj;
}

/**************************************
 * stream compiler
 *************************************/
bool 
TrexStreamsCompiler::compile(const std::vector<TrexStream *> &streams, TrexStreamsCompiledObj &obj) {
    /* for now we do something trivial, */
    for (auto stream : streams) {

        /* skip non-enabled streams */
        if (!stream->m_enabled) {
            continue;
        }

        /* for now skip also non self started streams */
        if (!stream->m_self_start) {
            continue;
        }

        /* for now support only continous ... */
        TrexStreamContinuous *cont_stream = dynamic_cast<TrexStreamContinuous *>(stream);
        if (!cont_stream) {
            continue;
        }

        /* add it */
        obj.add_compiled_stream(cont_stream->m_isg_usec,
                                cont_stream->get_pps(),
                                cont_stream->m_pkt.binary,
                                cont_stream->m_pkt.len);
    }

    return true;
}


