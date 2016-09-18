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

#include <trex_vm_splitter.h>
#include <trex_stateless.h>

/**
 * split a specific stream's VM to multiple cores
 * number of cores is implied by the size of the vector
 * 
 */
void
TrexVmSplitter::split(TrexStream *stream, std::vector<TrexStream *> core_streams) {

    /* nothing to do if no VM */
    if (stream->m_vm.is_vm_empty()) {
        return;
    }

    /* prepare some vars */
    m_dp_core_count = core_streams.size();
    m_core_streams  = &core_streams;
    m_stream        = stream;

    uint16_t cache_size=m_stream->m_cache_size;
    /* split the cache_size  */
    if (cache_size>0) {

        /* TBD need to check if we need to it is not too big from pool */
        if (cache_size > 10000) {
            throw TrexException("Cache is too big try to reduce it ");
        }

        /* split like variable splitters with leftovers */
        uint16_t cache_per_core = cache_size/m_dp_core_count;
        uint16_t leftover   = cache_size % m_dp_core_count;

        if (cache_per_core<1) {
            cache_per_core=1;
            leftover=0;
        }

        for (TrexStream *core_stream : *m_core_streams) {
            core_stream->m_cache_size = cache_per_core;
            if (leftover) {
                core_stream->m_cache_size+=1; 
                leftover-=1;
            }
        }
    }


    /* if we cannot split - compile the main and duplicate */
    bool rc = split_internal();
    if (!rc) {

        /* compile the stream and simply clone it to all streams */
        m_stream->vm_compile();

        /* for every core - simply clone the DP object */
        for (TrexStream *core_stream : *m_core_streams) {
            core_stream->m_vm_dp = m_stream->m_vm_dp->clone();
        }

        /* no need for the reference stream DP object */
        delete m_stream->m_vm_dp;
        m_stream->m_vm_dp = NULL;
    }
}

bool
TrexVmSplitter::split_internal() {

    /* no split needed ? fall back */
    if (!m_stream->m_vm.need_split()) {
        return false;
    }

    duplicate_vm();

    /* search for splitable instructions */
    for (StreamVmInstruction *instr : m_stream->m_vm.get_instruction_list()) {
        if (!instr->is_var_instruction()) {
            continue;
        }

        split_flow_var( (const StreamVmInstructionVar *)instr );

    }


    /* done - now compile for all cores */
    compile_vm();

    return true;

}

/**
 * split a flow var instruction
 * 
 * @author imarom (20-Dec-15)
 * 
 * @param instr 
 * 
 * @return bool 
 */
void
TrexVmSplitter::split_flow_var(const StreamVmInstructionVar *src) {
    /* a var might not need split (random) */
    if (!src->need_split()) {
        return;
    }

    /* do work */
    int core_id = 0;
    for (TrexStream *core_stream : *m_core_streams) {

        StreamVmInstructionVar *dst = core_stream->m_vm.lookup_var_by_name(src->get_var_name());
        assert(dst);

        /* for each core we need to give a phase and multiply the step frequency */
        dst->update(core_id, m_dp_core_count);

        core_id++;
    }

}



/**
 * duplicate the VM instructions
 * to all the cores
 */
void
TrexVmSplitter::duplicate_vm() {
    /* for each core - duplicate the instructions */
    for (TrexStream *core_stream : *m_core_streams) {
        m_stream->m_vm.clone(core_stream->m_vm);
    }
}

/**
 * now compile the updated VM
 */
void
TrexVmSplitter::compile_vm() {
    /* for each core - duplicate the instructions */
    for (TrexStream *core_stream : *m_core_streams) {
        core_stream->vm_compile();
    }
}

