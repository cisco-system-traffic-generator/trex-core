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
    m_objs.clear();
    m_duration_sim=-1.0;
}

void 
TrexStreamsCompiledObj::add_compiled_stream(TrexStream * stream) {
    obj_st obj;

    obj.m_stream = stream;

    m_objs.push_back(obj);
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

    new_compiled_obj->m_duration_sim = m_duration_sim;

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

        /* add it */
        obj.add_compiled_stream(stream);
    }

    return true;
}


