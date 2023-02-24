/*
 Itay Marom
 ystems, Inc.
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
#ifndef __TREX_STL_STREAM_H__
#define __TREX_STL_STREAM_H__

#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <stdint.h>
#include <string>

#include <json/json.h>

#include "os_time.h"
#include "common/captureFile.h"
#include "common/bitMan.h"

#include "internal_api/trex_platform_api.h"
#include "trex_exception.h"

#include "trex_stl_stream_vm.h"

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

    if (pkt_size<=128) {
        return (pkt_size);
    }

    //pkt_size> 128
    // if reside is less than 64 keep it as a single packet
    uint16_t non_writable = pkt_size - (max_offset_writable +1) ;
    if ( non_writable<64 ) {
        return (pkt_size);
    }

    // keep the r/w at least 60 byte
    if ((max_offset_writable+1)<=60) {
        return 60;
    }
    return max_offset_writable+1;
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

class TrexStream;

/**
 * describes a stream rate
 * 
 * @author imarom (18-Feb-16)
 */
class TrexStreamRate {


public:

    enum rate_type_e {
        RATE_INVALID,
        RATE_PPS,
        RATE_BPS_L1,
        RATE_BPS_L2,
        RATE_PERCENTAGE
    };

    TrexStreamRate(TrexStream &stream) : m_stream(stream) {
        m_pps        = 0;
        m_bps_L1     = 0;
        m_bps_L2     = 0;
        m_percentage = 0;
    }


    TrexStreamRate& operator=(const TrexStreamRate& other) {
        m_pps        = other.m_pps;
        m_bps_L1     = other.m_bps_L1;
        m_bps_L2     = other.m_bps_L2;
        m_percentage = other.m_percentage;

        return (*this);
    }

    /**
     * set the base rate 
     * other values will be dervied from this value 
     * 
     */
    void set_base_rate(rate_type_e type, double value) {
        m_pps        = 0;
        m_bps_L1     = 0;
        m_bps_L2     = 0;
        m_percentage = 0;

        if ( value <= 0 ) {
            throw TrexException("Rate value should be positive.");
        }

        switch (type) {
        case RATE_PPS:
            m_pps = value;
            break;
        case RATE_BPS_L1:
            m_bps_L1 = value;
            break;
        case RATE_BPS_L2:
            m_bps_L2 = value;
            break;
        case RATE_PERCENTAGE:
            m_percentage = value;
            break;

        default:
            throw TrexException("Invalid rate type.");
        
        }
    }

    double get_pps() {
        if (m_pps == 0) {
            calculate();
        }
        return (m_pps);
    }
    
    double get_bps_L1() {
        if (m_bps_L1 == 0) {
            calculate();
        }
        return (m_bps_L1);
    }

    double get_bps_L2() {
        if (m_bps_L2 == 0) {
            calculate();
        }
        return m_bps_L2;
    }

    double get_percentage() {
        if (m_percentage == 0) {
            calculate();
        }
        return m_percentage;
    }

  

    /* update the rate by a factor */
    void update_factor(double factor) {
        /* if all are non zero - it works, if only one (base) is also works */
        m_pps        *= factor;
        m_bps_L1     *= factor;
        m_bps_L2     *= factor;
        m_percentage *= factor;
    }

   

private:

    /**
     * calculates all the rates from the base rate
     * 
     */
    void calculate() {

        if (m_pps != 0) {
            calculate_from_pps();
        } else if (m_bps_L1 != 0) {
            calculate_from_bps_L1();
        } else if (m_bps_L2 != 0) {
            calculate_from_bps_L2();
        } else if (m_percentage != 0) {
            calculate_from_percentage();
        } else {
            throw TrexException("Rate value shound be positive.");
       }
    }


    uint64_t get_line_speed_bps();
    double get_pkt_size();

    void calculate_from_pps() {
        m_bps_L1     = m_pps * (get_pkt_size() + 20) * 8;
        m_bps_L2     = m_pps * get_pkt_size() * 8;
        m_percentage = (m_bps_L1 / get_line_speed_bps()) * 100.0;
    }


    void calculate_from_bps_L1() {
        m_bps_L2     = m_bps_L1 * ( get_pkt_size() / (get_pkt_size() + 20.0) );
        m_pps        = m_bps_L2 / (8 * get_pkt_size());
        m_percentage = (m_bps_L1 / get_line_speed_bps()) * 100.0;
    }


    void calculate_from_bps_L2() {
        m_bps_L1     = m_bps_L2 * ( (get_pkt_size() + 20.0) / get_pkt_size());
        m_pps        = m_bps_L2 / (8 * get_pkt_size());
        m_percentage = (m_bps_L1 / get_line_speed_bps()) * 100.0;
    }

    void calculate_from_percentage() {
        m_bps_L1     = (m_percentage / 100.0) * get_line_speed_bps();
        m_bps_L2     = m_bps_L1 * ( get_pkt_size() / (get_pkt_size() + 20.0) );
        m_pps        = m_bps_L2 / (8 * get_pkt_size());

    }

    double       m_pps;
    double       m_bps_L1;
    double       m_bps_L2;
    double       m_percentage;

    /* reference to the owner class */
    TrexStream  &m_stream;
};

/**
 * Stateless Stream
 * 
 */
class TrexStream {
friend class TrexStreamRate;

public:
    enum STREAM_TYPE {
        stNONE         = 0,
        stCONTINUOUS   = 4,
        stSINGLE_BURST = 5,
        stMULTI_BURST  = 6
    };

    typedef uint8_t stream_type_t ;

    enum DST_MAC_TYPE {
        stCFG_FILE     = 0,
        stPKT          = 1,
        stARP          = 2
    };

    typedef uint8_t stream_dst_mac_t ;


    static std::string get_stream_type_str(stream_type_t stream_type);

public:
    TrexStream(uint8_t type,uint8_t port_id, uint32_t stream_id) : TrexStream(type, port_id, stream_id, stream_id) {}
    virtual ~TrexStream();

    /* provides storage for the stream json*/
    void store_stream_json(const Json::Value &stream_json);

    /* access the stream json */
    const Json::Value & get_stream_json();

    /* compress the stream id to be zero based */
    void fix_dp_stream_id(uint32_t my_stream_id,int next_stream_id){
        m_stream_id      = my_stream_id;
        m_next_stream_id = next_stream_id;
    }


    double get_pps() {
        return m_rate.get_pps();
    }

    double get_bps_L1() {
        return m_rate.get_bps_L1();
    }

    double get_bps_L2() {
        return m_rate.get_bps_L2();
    }

    double get_bw_percentage() {
        return m_rate.get_percentage();
    }

    void set_rate(TrexStreamRate::rate_type_e type, double value) {
        m_rate.set_base_rate(type, value);
    }

    void update_rate_factor(double factor) {
        /* fixed rate streams cannot be updated in rate */
        if (!is_fixed_rate_stream()) {
            m_rate.update_factor(factor);
        }
    }

    void set_type(uint8_t type){
        m_type = type;
    }

    void set_null_stream(bool enable) {
        m_null_stream = enable;
    }

    bool is_null_stream() const {
        return m_null_stream;
    }

    uint8_t get_type(void) const {
        return ( m_type );
    }

    bool is_dp_next_stream() {
        if (m_next_stream_id<0) {
            return (false);
        }else{
            return (true);
        }
    }

    inline bool is_latency_stream() const {
        return (m_rx_check.m_enabled && (m_rx_check.m_rule_type == TrexPlatformApi::IF_STAT_PAYLOAD));
    }

    /**
     * Indicate if a stream is a Tagged Packet Group stream.
     *
     * @return bool
     *   True iff the stream is a TPG stream.
     **/
    inline bool is_tpg_stream() const {
        return (m_rx_check.m_enabled && (m_rx_check.m_rule_type) == TrexPlatformApi::IF_STAT_TPG_PAYLOAD);
    }

    bool need_flow_stats() const {
        return m_rx_check.m_enabled && !m_null_stream;
    }

    bool has_explicit_dst_mac() {
        return get_override_dst_mac_mode() == TrexStream::stPKT;
    }

    /**
     * Indicate if the stream has a fixed rate. For example Latency streams have a fixed rate.
     *
     * @return bool
     *   True iff the stream has fixed rate.
     **/
    inline bool is_fixed_rate_stream() const {
        return is_latency_stream();
    }

    /**
     * Does the stream need sequencing? For example, latency and Tagged Packet Group streams
     * have sequence numbers in the packet payload.
     *
     * @return bool
     *   True iff the stream needs sequencing.
     **/
    inline bool is_sequenced_stream() const {
        return is_latency_stream() || is_tpg_stream();
    }

    /* can this stream be split ? */
    bool is_splitable(uint8_t dp_core_count) const {

        if (is_sequenced_stream() || m_core_id_specified) {
            // because of sequence number, can't split streams with payload rule to different cores
            return false;
        }

        /* cont stream is splittable unless otherwise specified */
        if (m_type == stCONTINUOUS) {
            return true;
        }

        return (m_burst_total_pkts >= dp_core_count);

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
    TrexStream * clone(bool full = false) const {

        /* not all fields will be cloned */

        TrexStream *dp = new TrexStream(m_type, m_port_id, m_stream_id, m_user_stream_id);

        /* on full clone we copy also VM */
        if (full) {
            m_vm.clone(dp->m_vm);
            
        }

        /* copy VM DP product */
        if (m_vm_dp) {
            dp->m_vm_dp = m_vm_dp->clone();
        } else {
            dp->m_vm_dp = NULL;
        }

        dp->m_isg_usec                = m_isg_usec;

        /* multi core phase parameters */
        dp->m_mc_phase_pre_sec            = m_mc_phase_pre_sec;
        dp->m_mc_phase_post_sec           = m_mc_phase_post_sec;

        dp->m_next_stream_id          = m_next_stream_id;

        dp->m_enabled    = m_enabled;
        dp->m_self_start = m_self_start;
        dp->m_start_paused = m_start_paused;

        /* deep copy */
        dp->m_pkt.clone(m_pkt.binary,m_pkt.len);

        dp->m_pkt_len_data          =   m_pkt_len_data;
        dp->m_rx_check              =   m_rx_check;
        dp->m_burst_total_pkts      =   m_burst_total_pkts;
        dp->m_num_bursts            =   m_num_bursts;
        dp->m_ibg_usec              =   m_ibg_usec;
        dp->m_flags                 =   m_flags;
        dp->m_cache_size            =   m_cache_size;
        dp->m_action_count          =   m_action_count;
        dp->m_random_seed           =   m_random_seed;
        dp->m_null_stream           =   m_null_stream;

        dp->m_rate                  =   m_rate;

        dp->m_core_id_specified     = m_core_id_specified;
        dp->m_core_id               = m_core_id;

        return(dp);
    }

    /* release the DP object */
    void release_dp_object() {
        if (m_vm_dp) {
            delete m_vm_dp;
            m_vm_dp = NULL;
        }
    }

    double get_burst_length_usec()  {
        return ( ( (m_burst_total_pkts - 1) / get_pps()) * 1000 * 1000);
    }

    double get_ipg_sec() {
        return (1.0 / get_pps());
    }
   
    /* return the delay before starting a stream */
    inline double get_start_delay_sec() {
        return usec_to_sec(m_isg_usec) + m_mc_phase_pre_sec;
    }

    /* return the delay before starting the next stream */
    inline double get_next_stream_delay_sec() {
        return m_mc_phase_post_sec;
    }

    /* return the delay between scheduling a new burst in a multi burst stream */
    inline double get_next_burst_delay_sec() {
        return usec_to_sec(m_ibg_usec) + m_mc_phase_post_sec + m_mc_phase_pre_sec;
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

    void set_override_src_mac_by_pkt_data(bool enable){
        btSetMaskBit16(m_flags,0,0,enable?1:0);
    }

    bool get_override_src_mac_by_pkt_data(){
        return (btGetMaskBit16(m_flags,0,0) ?true:false);
    }

    void set_override_dst_mac_mode(stream_dst_mac_t  val){
        btSetMaskBit16(m_flags,2,1,val&0x3);
    }

    stream_dst_mac_t get_override_dst_mac_mode(){
        return ((stream_dst_mac_t)btGetMaskBit16(m_flags,2,1));
    }

public:
    /* basic */
    uint8_t       m_type;
    uint8_t       m_port_id;
    uint16_t      m_flags;

    uint32_t      m_stream_id;              /* id from RPC can be anything, will be overriden by DP ID */
    const uint32_t m_user_stream_id;        /* backup of stream_id coming from user/CP */
    uint32_t      m_action_count;
    uint16_t      m_cache_size;
    uint32_t      m_random_seed;
    

    /* config fields */
    double        m_mc_phase_pre_sec;
    double        m_mc_phase_post_sec;

    double        m_isg_usec;
    int           m_next_stream_id;
    uint8_t       m_core_id;

    /* indicators */
    bool          m_enabled;
    bool          m_self_start;
    bool          m_start_paused;
    bool          m_core_id_specified;

    /* null stream (a dummy stream) */
    bool          m_null_stream;

    /* VM CP and DP */
    StreamVm      m_vm;
    StreamVmDp   *m_vm_dp;

    CStreamPktData   m_pkt;

    TrexStreamPktLenData m_pkt_len_data;

    /* pkt */


    /* RX check */
    struct {
        bool      m_enabled;
        bool      m_seq_enabled;
        bool      m_latency;
        bool      m_vxlan_skip;
        uint16_t  m_rule_type;
        uint32_t  m_pg_id;
        uint16_t  m_hw_id;
        bool      m_ieee_1588;
    } m_rx_check;

    uint32_t   m_burst_total_pkts; /* valid in case of burst stSINGLE_BURST,stMULTI_BURST*/

    uint32_t   m_num_bursts; /* valid in case of stMULTI_BURST */

    double     m_ibg_usec;  /* valid in case of stMULTI_BURST */

    /* original template provided by requester */
    Json::Value m_stream_json;

    TrexStreamPktLenData *get_pkt_size() {
        /* lazy calculate the expected packet length */
        if (m_pkt_len_data.m_expected_pkt_len == 0) {
            /* if we have a VM - it might have changed the packet (even random) */
            if (m_vm.is_vm_empty()) {
                m_pkt_len_data.m_expected_pkt_len = m_pkt.len;
                m_pkt_len_data.m_min_pkt_len = m_pkt.len;
                m_pkt_len_data.m_max_pkt_len = m_pkt.len;
            } else {
                m_vm.calc_pkt_len_data(m_pkt.len, m_pkt_len_data);
            }
        }

        return &m_pkt_len_data;
    }

private:
    // additional constructor for clone() to save const m_user_stream_id
    TrexStream(uint8_t type, uint8_t port_id, uint32_t stream_id, uint32_t user_stream_id);

    /* no access to this without a lazy build method */
    TrexStreamRate m_rate;
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
     * fetch a stream if exists 
     * o.w NULL 
     *  
     */
    TrexStream * get_stream_by_id(uint32_t stream_id);

    /**
     * get max stream ID assigned
     * 
     * 
     * @return int
     */
    int get_max_stream_id() const;

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

#endif /* __TREX_STL_STREAM_H__ */

