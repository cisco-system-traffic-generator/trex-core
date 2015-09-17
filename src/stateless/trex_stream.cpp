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
TrexStream::TrexStream(uint8_t port_id, uint32_t stream_id) : m_port_id(port_id), m_stream_id(stream_id) {

    /* default values */
    m_isg_usec        = 0;
    m_next_stream_id  = -1;
    m_enabled    = false;
    m_self_start = false;

    m_pkt.binary = NULL;
    m_pkt.len    = 0;

    m_rx_check.m_enable = false;

}

TrexStream::~TrexStream() {
    if (m_pkt.binary) {
        delete [] m_pkt.binary;
    }
}

void
TrexStream::store_stream_json(const Json::Value &stream_json) {
    /* deep copy */
    m_stream_json = stream_json;
}

const Json::Value &
TrexStream::get_stream_json() {
    return m_stream_json;
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
    TrexStream *old_stream = get_stream_by_id(stream->m_stream_id);
    if (old_stream) {
        remove_stream(old_stream);
        delete old_stream;
    }

    m_stream_table[stream->m_stream_id] = stream;
}                                           

void TrexStreamTable::remove_stream(TrexStream *stream) {
    m_stream_table.erase(stream->m_stream_id);
}


void TrexStreamTable::remove_and_delete_all_streams() {

    for (auto stream : m_stream_table) {
        delete stream.second;
    }

    m_stream_table.clear();
}

TrexStream * TrexStreamTable::get_stream_by_id(uint32_t stream_id) {
    auto search = m_stream_table.find(stream_id);

    if (search != m_stream_table.end()) {
        return search->second;
    } else {
        return NULL;
    }
}

void TrexStreamTable::get_stream_list(std::vector<uint32_t> &stream_list) {
    stream_list.clear();

    for (auto stream : m_stream_table) {
        stream_list.push_back(stream.first);
    }
}

int TrexStreamTable::size() {
    return m_stream_table.size();
}
