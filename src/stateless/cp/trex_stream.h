/*
 Itay Marom
 Hanoch Haim
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
#ifndef __TREX_STREAM_H__
#define __TREX_STREAM_H__

#include <unordered_map>
#include <vector>
#include <stdint.h>
#include <string>

#include <json/json.h>

#include <trex_stream_vm.h>
#include <stdio.h>
#include <string.h>

class TrexRpcCmdAddStream;


static inline uint16_t get_log2_size(uint16_t size){

    uint16_t _sizes[]={64,128,256,512,1024,2048};
    int i;
    for (i=0; i<sizeof(_sizes)/sizeof(_sizes[0]); i++) {
        if (size<=_sizes[i]) {
            return (_sizes[i]);
        }
    }
    assert(0);
    return (0);
}

/**
 *  calculate the size of writable mbuf in bytes. maximum size if packet size 
 * 
 * @param max_offset_writable
 *                 the last byte that we don't write too. for example when 63 it means that bytes [62] in the array is written (zero base)
 * @param pkt_size packet size in bytes
 * 
 * @return the writable size of the first mbuf . the idea is to give at least 64 bytes const mbuf else all packet will be writeable 
 * 
 * examples:
 *       max_offset_writable =63
 *       pkt_size =62
 *        ==>62
 * 
 */
static inline uint16_t calc_writable_mbuf_size(uint16_t max_offset_writable,
                                               uint16_t pkt_size){

    if ( pkt_size<=64 ){
        return (pkt_size);
    }
    if (pkt_size<=128) {
        return (pkt_size);
    }

    //pkt_size> 128
    uint16_t non_writable = pkt_size - (max_offset_writable -1) ;
    if ( non_writable<64 ) {
        return (pkt_size);
    }
    return(max_offset_writable-1);
}



struct CStreamPktData {
        uint8_t      *binary;
        uint16_t      len;

        std::string   meta;

public:
        inline void clone(uint8_t  * in_binary,
                          uint32_t in_pkt_size){
            binary = new uint8_t[in_pkt_size];
            len    = in_pkt_size;
            memcpy(binary,in_binary,in_pkt_size);
        }
}; 


/**
 * Stateless Stream
 * 
 */
class TrexStream {

public:
    enum STREAM_TYPE {
        stNONE         = 0,
        stCONTINUOUS   = 4,
        stSINGLE_BURST = 5,
        stMULTI_BURST  = 6
    };

    typedef uint8_t stream_type_t ;

    static std::string get_stream_type_str(stream_type_t stream_type);

public:
    TrexStream(uint8_t type,uint8_t port_id, uint32_t stream_id);
    virtual ~TrexStream();

    /* defines the min max per packet supported */
    static const uint32_t MIN_PKT_SIZE_BYTES = 1;
    static const uint32_t MAX_PKT_SIZE_BYTES = 9000;

    /* provides storage for the stream json*/
    void store_stream_json(const Json::Value &stream_json);

    /* access the stream json */
    const Json::Value & get_stream_json();

    /* compress the stream id to be zero based */
    void fix_dp_stream_id(uint32_t my_stream_id,int next_stream_id){
        m_stream_id      = my_stream_id;
        m_next_stream_id = next_stream_id;
    }

    double get_pps() const {
        return m_pps;
    }

    void set_pps(double pps){
        m_pps = pps;
    }

    void set_type(uint8_t type){
        m_type = type;
    }

    uint8_t get_type(void) const {
        return ( m_type );
    }

    bool is_dp_next_stream(){
        if (m_next_stream_id<0) {
            return (false);
        }else{
            return (true);
        }
    }



    void set_multi_burst(uint32_t   burst_total_pkts, 
                         uint32_t   num_bursts,
                         double     ibg_usec) {
        m_burst_total_pkts = burst_total_pkts;
        m_num_bursts       = num_bursts;
        m_ibg_usec         = ibg_usec;
    }

    void set_single_burst(uint32_t   burst_total_pkts){
        set_multi_burst(burst_total_pkts,1,0.0); 
    }

    /* create new stream */
    TrexStream * clone() const {

        /* not all fields will be cloned */

        TrexStream *dp = new TrexStream(m_type,m_port_id,m_stream_id);
        if (m_vm_dp) {
            dp->m_vm_dp = m_vm_dp->clone();
        } else {
            dp->m_vm_dp = NULL;
        }

        dp->m_isg_usec      = m_isg_usec;
        dp->m_next_stream_id = m_next_stream_id;

        dp->m_enabled    = m_enabled;
        dp->m_self_start = m_self_start;

        /* deep copy */
        dp->m_pkt.clone(m_pkt.binary,m_pkt.len);

        dp->m_rx_check              =   m_rx_check;
        dp->m_pps                   =   m_pps;
        dp->m_burst_total_pkts      =   m_burst_total_pkts;
        dp->m_num_bursts            =   m_num_bursts;
        dp->m_ibg_usec              =   m_ibg_usec;

        return(dp);
    }


    double get_burst_length_usec() const {
        return ( (m_burst_total_pkts / m_pps) * 1000 * 1000);
    }

    double get_bps() const {
        /* packet length + 4 CRC bytes to bits and multiplied by PPS */
        return (m_pps * (m_pkt.len + 4) * 8);
    }

    void Dump(FILE *fd);

    StreamVmDp * getDpVm(){
        return (m_vm_dp);
    }

    /**
     * internal compilation of stream (for DP)
     * 
     */
    void vm_compile();

public:
    /* basic */
    uint8_t       m_type;
    uint8_t       m_port_id;
    uint32_t      m_stream_id;              /* id from RPC can be anything */
    

    /* config fields */
    double        m_isg_usec;
    int           m_next_stream_id;

    /* indicators */
    bool          m_enabled;
    bool          m_self_start;


    /* VM CP and DP */
    StreamVm      m_vm;
    StreamVmDp   *m_vm_dp;

    CStreamPktData   m_pkt;
    /* pkt */


    /* RX check */
    struct {
        bool      m_enable;
        bool      m_seq_enabled;
        bool      m_latency;
        uint32_t  m_stream_id;

    } m_rx_check;

    double m_pps;

    uint32_t   m_burst_total_pkts; /* valid in case of burst stSINGLE_BURST,stMULTI_BURST*/

    uint32_t   m_num_bursts; /* valid in case of stMULTI_BURST */

    double     m_ibg_usec;  /* valid in case of stMULTI_BURST */

    /* original template provided by requester */
    Json::Value m_stream_json;

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
     */
    void remove_stream(TrexStream *stream);

    /**
     * remove all streams on the table
     * memory will be deleted
     */
    void remove_and_delete_all_streams();

    /**
     * fetch a stream if exists 
     * o.w NULL 
     *  
     */
    TrexStream * get_stream_by_id(uint32_t stream_id);

    /**
     * populate a list with all the stream IDs
     * 
     * @author imarom (06-Sep-15)
     * 
     * @param stream_list 
     */
    void get_id_list(std::vector<uint32_t> &id_list);

    /**
     * populate a list with all the stream objects
     * 
     */
    void get_object_list(std::vector<TrexStream *> &object_list);

    /**
     * get the table size
     * 
     */
    int size();

    std::unordered_map<int, TrexStream *>::iterator begin() {return m_stream_table.begin();}
    std::unordered_map<int, TrexStream *>::iterator end() {return m_stream_table.end();}

private:
    /**
     * holds all the stream in a hash table by stream id
     * 
     */
    std::unordered_map<int, TrexStream *> m_stream_table;
};

#endif /* __TREX_STREAM_H__ */

