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
#ifndef __TREX_STREAM_API_H__
#define __TREX_STREAM_API_H__

#include <unordered_map>
#include <stdint.h>

class TrexRpcCmdAddStream;

/**
 * Stateless Stream
 * 
 */
class TrexStream {
    /* provide the RPC parser a way to access private fields */
    friend class TrexRpcCmdAddStream;
    friend class TrexStreamTable;

public:
    TrexStream();
    virtual ~TrexStream() = 0;

    static const uint32_t MIN_PKT_SIZE_BYTES = 64;
    static const uint32_t MAX_PKT_SIZE_BYTES = 9000;

private:
    /* config */
    uint32_t      stream_id;
    uint8_t       port_id;
    double        isg_usec;
    uint32_t      next_stream_id;
    uint32_t      loop_count;

    /* indicators */
    bool          enable;
    bool          start;
    
    /* pkt */
    uint8_t      *pkt;
    uint16_t      pkt_len;

    /* VM */

    /* RX check */
    struct {
        bool      enable;
        bool      seq_enable;
        bool      latency;
        uint32_t  stream_id;

    } rx_check;

};

/**
 * continuous stream
 * 
 */
class TrexStreamContinuous : public TrexStream {
protected:
    uint32_t pps;
};

/**
 * single burst
 * 
 */
class TrexStreamSingleBurst : public TrexStream {
protected:
    uint32_t packets;
    uint32_t pps;
};

/**
 * multi burst
 * 
 */
class TrexStreamMultiBurst : public TrexStream {
protected:
    uint32_t pps;
    double   ibg_usec;
    uint32_t number_of_bursts;
    uint32_t pkts_per_burst;
};

/**
 * holds all the streams 
 *  
 */
class TrexStreamTable {
public:

    TrexStreamTable();
    ~TrexStreamTable();

    /**
     * add a stream 
     * if a previous one exists, the old one  will be deleted 
     */
    void add_stream(TrexStream *stream);

    /**
     * remove a stream
     * 
     */
    void remove_stream(TrexStream *stream);

    /**
     * fetch a stream if exists 
     * o.w NULL 
     *  
     */
    TrexStream * get_stream_by_id(uint32_t stream_id);

private:
    /**
     * holds all the stream in a hash table by stream id
     * 
     */
    std::unordered_map<int, TrexStream *> m_stream_table;
};

#endif /* __TREX_STREAM_API_H__ */

