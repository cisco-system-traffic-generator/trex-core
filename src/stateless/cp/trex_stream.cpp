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
#include <trex_stream.h>
#include <cstddef>
#include <string.h>
#include <assert.h>

/**************************************
 * stream
 *************************************/


std::string TrexStream::get_stream_type_str(stream_type_t stream_type){

    std::string res;


    switch (stream_type) {

    case        stCONTINUOUS :
         res="stCONTINUOUS  ";
        break;

    case stSINGLE_BURST :
        res="stSINGLE_BURST  ";
        break;

    case stMULTI_BURST :
        res="stMULTI_BURST  ";
        break;
    default:
        res="Unknow  ";
    };
    return(res);
}


void
TrexStream::compile() {

    /* in case there are no instructions - nothing to do */
    if (m_vm.is_vm_empty()) {
        m_has_vm = false;
        return;
    }

    m_has_vm = true;

    m_vm.set_packet_size(m_pkt.len);

    m_vm.compile();

    m_vm_dp = m_vm.cloneAsVmDp();


    /* calc m_vm_prefix_size which is the size of the writable packet */
    uint16_t max_pkt_offset = m_vm_dp->get_max_packet_update_offset();
    uint16_t pkt_size = m_pkt.len;

    /* calculate the mbuf size that we should allocate */
    m_vm_prefix_size = calc_writable_mbuf_size(max_pkt_offset, pkt_size);
}


void TrexStream::Dump(FILE *fd){

    fprintf(fd,"\n");
    fprintf(fd,"==> Stream_id    : %lu \n",(ulong)m_stream_id);
    fprintf(fd," Enabled      : %lu \n",(ulong)(m_enabled?1:0));
    fprintf(fd," Self_start   : %lu \n",(ulong)(m_self_start?1:0));

    if (m_next_stream_id>=0) {
        fprintf(fd," Nex_stream_id  : %lu \n",(ulong)m_next_stream_id);
    }else {
        fprintf(fd," Nex_stream_id  : %d \n",m_next_stream_id);
    }

    fprintf(fd," Port_id      : %lu \n",(ulong)m_port_id);

    if (m_isg_usec>0.0) {
        fprintf(fd," isg    : %6.2f \n",m_isg_usec);
    }
    fprintf(fd," type    : %s \n",get_stream_type_str(m_type).c_str());

    if ( m_type == TrexStream::stCONTINUOUS ) {
        fprintf(fd," pps        : %f \n",m_pps);
    }
    if (m_type == TrexStream::stSINGLE_BURST) {
        fprintf(fd," pps        : %f \n",m_pps);
        fprintf(fd," burst      : %lu \n",(ulong)m_burst_total_pkts);
    }
    if (m_type == TrexStream::stMULTI_BURST) {
        fprintf(fd," pps        : %f \n",m_pps);
        fprintf(fd," burst      : %lu \n",(ulong)m_burst_total_pkts);
        fprintf(fd," mburst     : %lu \n",(ulong)m_num_bursts);
        if (m_ibg_usec>0.0) {
            fprintf(fd," m_ibg_usec : %f \n",m_ibg_usec);
        }
    }
}

 
TrexStream::TrexStream(uint8_t type,
                       uint8_t port_id, uint32_t stream_id) : m_port_id(port_id), m_stream_id(stream_id) {

    /* default values */
    m_type            = type;
    m_isg_usec        = 0;
    m_next_stream_id  = -1;
    m_enabled    = false;
    m_self_start = false;

    m_pkt.binary = NULL;
    m_pkt.len    = 0;
    m_has_vm = false;
    m_vm_prefix_size = 0;

    m_rx_check.m_enable = false;


    m_pps=-1.0;
    m_burst_total_pkts=0; 
    m_num_bursts=1; 
    m_ibg_usec=0.0;  
    m_vm_dp = NULL;
}

TrexStream::~TrexStream() {
    if (m_pkt.binary) {
        delete [] m_pkt.binary;
    }
    if ( m_vm_dp ){
        delete  m_vm_dp;
        m_vm_dp=NULL;
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

void TrexStreamTable::get_id_list(std::vector<uint32_t> &id_list) {
    id_list.clear();

    for (auto stream : m_stream_table) {
        id_list.push_back(stream.first);
    }
}

void TrexStreamTable::get_object_list(std::vector<TrexStream *> &object_list) {
    object_list.clear();

    for (auto stream : m_stream_table) {
        object_list.push_back(stream.second);
    }

}

int TrexStreamTable::size() {
    return m_stream_table.size();
}


