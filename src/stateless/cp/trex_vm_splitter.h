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
#ifndef __TREX_VM_SPLITTER_H__
#define __TREX_VM_SPLITTER_H__

#include <trex_stream.h>


class TrexVmSplitter {

public:

    TrexVmSplitter() {
        m_dp_core_count = 0;
    }

    /**
     * split a stream's VM to per core streams
     */
    void split(TrexStream *stream, std::vector<TrexStream *> core_streams);


private:
    bool split_internal();
    bool split_by_flow_var(const StreamVmInstructionFlowMan *instr);
    bool split_by_flow_client_var(const StreamVmInstructionFlowClient *instr);

    void duplicate_vm();

    TrexStream                 *m_stream;
    std::vector<TrexStream *>  *m_core_streams;
    uint8_t                     m_dp_core_count;
};


#endif /* __TREX_VM_SPLITTER_H__ */
