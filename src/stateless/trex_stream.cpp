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
#include <trex_stream_api.h>
#include <cstddef>

/**************************************
 * stream
 *************************************/
TrexStream::TrexStream() {
    pkt = NULL;
}

TrexStream::~TrexStream() {
    if (pkt) {
        delete [] pkt;
    }
}

/**************************************
 * stream table
 *************************************/
TrexStreamTable::TrexStreamTable() {

}

TrexStreamTable::~TrexStreamTable() {
    for (auto stream : m_stream_table) {
        delete stream.second;
    }
}

void TrexStreamTable::add_stream(TrexStream *stream) {
    TrexStream *old_stream = get_stream_by_id(stream->stream_id);
    if (old_stream) {
        remove_stream(old_stream);
        delete old_stream;
    }

    m_stream_table[stream->stream_id] = stream;
}                                           

void TrexStreamTable::remove_stream(TrexStream *stream) {
    m_stream_table.erase(stream->stream_id);
}

TrexStream * TrexStreamTable::get_stream_by_id(uint32_t stream_id) {
    auto search = m_stream_table.find(stream_id);

    if (search != m_stream_table.end()) {
        return search->second;
    } else {
        return NULL;
    }
}
