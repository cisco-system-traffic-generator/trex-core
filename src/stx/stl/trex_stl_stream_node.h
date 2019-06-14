/*
 Itay Marom
 Hanoh Haim
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
#ifndef __TREX_STL_STREAM_NODE_H__
#define __TREX_STL_STREAM_NODE_H__


#include <stdio.h>

#include "bp_sim.h"
#include "trex_stl_stream.h"

class TrexStatelessDpCore;
class TrexStatelessDpPerPort;



class TrexCpToDpMsgBase;
class CFlowGenListPerThread;

struct CGenNodeCommand : public CGenNodeBase  {

friend class TrexStatelessDpCore;

public:
    TrexCpToDpMsgBase  *m_cmd;

    uint8_t             m_pad_end[104];

    /* CACHE_LINE */
    uint64_t            m_pad3[8];


public:
    void free_command();

} __rte_cache_aligned;;


static_assert(sizeof(CGenNodeCommand) == sizeof(CGenNode), "sizeof(CGenNodeCommand) != sizeof(CGenNode)" );


struct CGenNodeCacheMbuf {
    rte_mbuf_t *  m_mbuf_const;
    rte_mbuf_t *  m_array[0];
public:
    static uint32_t get_object_size(uint32_t size){
        return ( sizeof(CGenNodeCacheMbuf) + sizeof(rte_mbuf_t *) * size );
    }
};

/* this is a event for stateless */
struct CGenNodeStateless : public CGenNodeBase  {
friend class TrexStatelessDpCore;

public:

    /* flags MASKS*/
    enum {
        SL_NODE_FLAGS_DIR                  =1, //USED by master
        SL_NODE_FLAGS_MBUF_CACHE           =2, //USED by master

        SL_NODE_CONST_MBUF                 =4,
                                             
        SL_NODE_VAR_PKT_SIZE               = 8,
        SL_NODE_STATS_NEEDED               = 0x10,
        SL_NODE_CONST_MBUF_CACHE_ARRAY     = 0x20  /* array of mbuf - cache */
    };

    enum {                                          
            ss_FREE_RESUSE =1, /* should be free by scheduler */
            ss_INACTIVE    =2, /* will be active by other stream or stopped */
            ss_ACTIVE      =3  /* the stream is active */ 
         };
    typedef uint8_t stream_state_t ;

    static std::string get_stream_state_str(stream_state_t stream_state);

private:
    /******************************/
    /* cache line 0               */
    /* important stuff here  R/W  */
    /******************************/
    void *              m_cache_mbuf; /* could be an array or a one mbuf */

    double              m_next_time_offset; /* in sec */
    uint32_t            m_action_counter;
    uint16_t            m_stat_hw_id; // hw id used to count rx and tx stats
    uint16_t            m_cache_array_cnt;

    uint8_t             m_null_stream;
    stream_state_t      m_state;
    uint8_t             m_stream_type; /* see TrexStream::STREAM_TYPE ,stream_type_t */
    uint8_t             m_pause;

    uint32_t            m_single_burst; /* the number of bursts in case of burst */
    uint32_t            m_single_burst_refill; 

    uint32_t            m_multi_bursts; /* in case of multi_burst how many bursts */
    double              m_next_time_offset_backup; /* paused nodes will be given slower ipg, backup value */

    /******************************/
    /* cache line 1  
      this cache line should be READONLY ! you can write only at init time */ 
    /******************************/
    TrexStream *         m_ref_stream_info; /* the stream info */
    CGenNodeStateless  * m_next_stream;

    uint8_t *            m_original_packet_data_prefix; /* pointer to the original first pointer 64/128/512 */

    /* Fast Field VM section */
    uint8_t *            m_vm_flow_var; /* pointer to the vm flow var */
    uint8_t *            m_vm_program;  /* pointer to the program */
    uint16_t             m_vm_program_size; /* up to 64K op codes */
    uint16_t             m_cache_size;   /*RO*/ /* the size of the mbuf array */
    uint8_t              m_batch_size;   /*RO*/ /* the batch size */
    uint8_t              m_port_id;
    uint32_t             m_profile_id;

    uint16_t             m_pad5; 

    /* End Fast Field VM Section */

    /* pad to match the size of CGenNode */
    uint8_t             m_pad_end[8];

    /* CACHE_LINE */
    uint64_t            m_pad3[8];


public:

    uint8_t             get_port_id(){
        return (m_port_id);
    }

    uint32_t            get_profile_id() {
        return (m_profile_id);
    }

    /**
     * calculate the time offset based 
     * on the PPS and multiplier 
     * 
     */
    void update_rate(double factor) {
        /* update the inter packet gap */
        m_next_time_offset_backup /= factor;
        if ( likely(!m_pause) ) {
            m_next_time_offset = m_next_time_offset_backup;
        }
    }

    void set_ipg(double ipg) {
        /* set inter packet gap */
        m_next_time_offset_backup = ipg;
        if ( likely(!m_pause) ) {
            m_next_time_offset = m_next_time_offset_backup;
        }
    }

    /* we restart the stream, schedule it using stream isg */
    inline void update_refresh_time(double cur_time){
        m_time = cur_time + usec_to_sec(m_ref_stream_info->m_isg_usec) + m_ref_stream_info->m_mc_phase_pre_sec;
    }

    inline bool is_mask_for_free(){
        return (get_state() == CGenNodeStateless::ss_FREE_RESUSE ?true:false);

    }
    inline void mark_for_free(){
        if (m_state != CGenNodeStateless::ss_FREE_RESUSE) {
            /* must be first */
            free_stl_node();
            set_state(CGenNodeStateless::ss_FREE_RESUSE);
            /* only to be safe */
            m_ref_stream_info= NULL;
            m_next_stream= NULL;
        }
    }

    bool is_pause(){
        return (m_pause==1?true:false);
    }

    void set_pause(bool enable){
        if ( enable ){
            m_next_time_offset = 0.1; // we don't want paused nodes to interfere too much in scheduler with non-paused
            m_pause=1;
        }else{
            m_next_time_offset = m_next_time_offset_backup;
            m_pause=0;
        }
    }

    bool is_node_active() {
        /* bitwise or - faster instead of two IFs */
        return ((m_pause | m_null_stream) == 0);
    }

    inline uint8_t  get_stream_type(){
        return (m_stream_type);
    }

    inline uint32_t   get_single_burst_cnt(){
        return (m_single_burst);
    }

    inline double   get_multi_ibg_sec(){
        return (usec_to_sec(m_ref_stream_info->m_ibg_usec));
    }

    inline uint32_t   get_multi_burst_cnt(){
        return (m_multi_bursts);
    }

    inline  void set_state(stream_state_t new_state){
        m_state=new_state;
    }


    inline stream_state_t get_state() {
        return m_state;
    }

    void refresh();

    inline void handle_continues(CFlowGenListPerThread *thread) {

        if (likely (is_node_active())) {
            thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);
        }

        /* in case of continues */
        m_time += m_next_time_offset;

        /* insert a new event */
        thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
    }

    inline void handle_multi_burst(CFlowGenListPerThread *thread) {
        if (likely (is_node_active())) {
            thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);
        }

        m_single_burst--;
        if (m_single_burst > 0 ) {
            /* in case of continues */
            m_time += m_next_time_offset;

            thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
        }else{
            m_multi_bursts--;
            if ( m_multi_bursts == 0 ) {
                set_state(CGenNodeStateless::ss_INACTIVE);
                
                TrexStatelessDpCore *stl_dp_core = (TrexStatelessDpCore *)thread->get_dp_core();
                if ( stl_dp_core->set_stateless_next_node(this, m_next_stream) ) {
                    /* update the next stream time using isg and post phase */
                    m_next_stream->update_refresh_time(m_time + m_ref_stream_info->get_next_stream_delay_sec());

                    thread->m_node_gen.m_p_queue.push( (CGenNode *)m_next_stream);
                }else{
                    // in case of zero we will schedule a command to stop 
                    // will be called from set_stateless_next_node
                }

            }else{
                /* next burst is like starting a new stream - add pre and post phase */
                m_time +=  m_ref_stream_info->get_next_burst_delay_sec();
                m_single_burst = m_single_burst_refill;
                thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
            }
        }
    }

        
    /**
     * main function to handle an event of a packet tx
     * 
     * 
     * 
     */

    inline void handle(CFlowGenListPerThread *thread) {

        if (m_stream_type == TrexStream::stCONTINUOUS ) {
            handle_continues(thread) ;
        }else{
            if (m_stream_type == TrexStream::stMULTI_BURST) {
                handle_multi_burst(thread);
            }else{
                assert(0);
            }
        }

    }

    void set_socket_id(socket_id_t socket){
        m_socket_id=socket;
    }

    socket_id_t get_socket_id(){
        return ( m_socket_id );
    }

    void set_stat_hw_id(uint16_t hw_id) {
        m_stat_hw_id = hw_id;
    }

    uint16_t get_stat_hw_id() {
        return ( m_stat_hw_id );
    }

    inline void set_stat_needed() {
        m_flags |= SL_NODE_STATS_NEEDED;
    }

    inline bool is_stat_needed() {
        return ((m_flags & SL_NODE_STATS_NEEDED) != 0);
    }

    inline bool is_latency_stream() {
        return m_ref_stream_info->is_latency_stream();
    }

    inline void set_mbuf_cache_dir(pkt_dir_t  dir){
        if (dir) {
            m_flags |=NODE_FLAGS_DIR;
        }else{
            m_flags &=~NODE_FLAGS_DIR;
        }
    }

    inline pkt_dir_t get_mbuf_cache_dir(){
        return ((pkt_dir_t)( m_flags &1));
    }

    inline void set_cache_mbuf(rte_mbuf_t * m){
        m_cache_mbuf=(void *)m;
        m_flags |= NODE_FLAGS_MBUF_CACHE;
    }

    inline rte_mbuf_t * get_cache_mbuf(){
        if ( m_flags & NODE_FLAGS_MBUF_CACHE ) {
            return ((rte_mbuf_t *)m_cache_mbuf);
        }else{
            return ((rte_mbuf_t *)0);
        }
    }

    inline void set_var_pkt_size(){
        m_flags |= SL_NODE_VAR_PKT_SIZE;
    }

    inline bool is_var_pkt_size(){
        return ( ( m_flags &SL_NODE_VAR_PKT_SIZE )?true:false);
    }

    inline void set_const_mbuf(rte_mbuf_t * m){
        m_cache_mbuf=(void *)m;
        m_flags |= SL_NODE_CONST_MBUF;
    }

    inline rte_mbuf_t * get_const_mbuf(){
        if ( m_flags &SL_NODE_CONST_MBUF ) {
            return ((rte_mbuf_t *)m_cache_mbuf);
        }else{
            return ((rte_mbuf_t *)0);
        }
    }

    void clear_const_mbuf(){
        m_flags= ( m_flags & ~SL_NODE_CONST_MBUF );
    }

    /* prefix header exits only in non cache mode size is 64/128/512  other are not possible right now */
    inline void alloc_prefix_header(uint16_t size){
         set_prefix_header_size(size);
         m_original_packet_data_prefix = (uint8_t *)malloc(size);
         assert(m_original_packet_data_prefix);
    }

    inline void free_prefix_header(){
         if (m_original_packet_data_prefix) {
             free(m_original_packet_data_prefix);
             m_original_packet_data_prefix=0;
         }
    }

    /* prefix headr could be 64/128/512 */
    inline void set_prefix_header_size(uint16_t size){
        m_src_port=size;
    }

    inline uint16_t prefix_header_size(){
        return (m_src_port);
    }

    rte_mbuf_t   * alloc_flow_stat_mbuf(rte_mbuf_t *m, struct flow_stat_payload_header * &fsp_head
                                        , bool is_const);
    bool alloc_flow_stat_mbuf_test_const();
    rte_mbuf_t   * alloc_node_with_vm();

    void free_stl_node();

protected:

    void free_stl_vm_buf();

public:
    void cache_mbuf_array_init();

    inline bool is_cache_mbuf_array(){
        return  ( m_flags & SL_NODE_CONST_MBUF_CACHE_ARRAY ? true:false );
    }

    void cache_mbuf_array_copy(CGenNodeCacheMbuf *obj,uint16_t size);

     rte_mbuf_t ** cache_mbuf_array_alloc(uint16_t size);

     void cache_mbuf_array_free();

     void cache_mbuf_array_set(uint16_t index,rte_mbuf_t * m);

     void cache_mbuf_array_set_const_mbuf(rte_mbuf_t * m);

     rte_mbuf_t * cache_mbuf_array_get_const_mbuf();

     rte_mbuf_t * cache_mbuf_array_get(uint16_t index);

     rte_mbuf_t * cache_mbuf_array_get_cur(void){
            CGenNodeCacheMbuf *p =(CGenNodeCacheMbuf *) m_cache_mbuf;
            rte_mbuf_t * m=p->m_array[m_cache_array_cnt];
            assert(m);
            m_cache_array_cnt++;
            if (m_cache_array_cnt == m_cache_size) {
                m_cache_array_cnt=0;
            }
            return m;
     }

    inline uint32_t get_user_stream_id(void) {
        return m_ref_stream_info->m_user_stream_id;
    }

public:
    /* debug functions */

    int get_stream_id();

    static void DumpHeader(FILE *fd);

    void Dump(FILE *fd);

private:

    void generate_random_seed();
    void refresh_vm_bss();


    void set_random_seed(uint32_t seed){
        uint32_t *p=get_random_bss_seed_memory();
        *p=seed;
    }

    uint32_t* get_random_bss_seed_memory(){
        return (uint32_t*)m_vm_flow_var;/* always the first 4 bytes */
    }


} __rte_cache_aligned;

static_assert(sizeof(CGenNodeStateless) == sizeof(CGenNode), "sizeof(CGenNodeStateless) != sizeof(CGenNode)" );


/* this is a event for PCAP transmitting */
struct CGenNodePCAP : public CGenNodeBase  {
friend class TrexStatelessDpPerPort;

public:

    /**
     * creates a node from a PCAP file 
     */
    bool create(uint8_t port_id,
                pkt_dir_t dir,
                socket_id_t socket_id,
                const uint8_t *mac_addr,
                const uint8_t *slave_mac_addr,
                const std::string &pcap_filename,
                double ipg_usec,
                double min_ipg_sec,
                double speedup,
                uint32_t count,
                bool is_dual);

    /**
     * destroy the node cleaning up any data
     * 
     */
    void destroy();
 
    bool is_dual() const {
        return m_is_dual;
    }

    /**
     * advance - will read the next packet
     * 
     * @author imarom (03-May-16)
     */
    void next() {
        assert(is_active());

        /* save the previous packet time */
        m_last_pkt_time = m_raw_packet->get_time();

        /* advance */
        if ( m_reader->ReadPacket(m_raw_packet) == false ){
            m_count--;

            /* if its the end - go home... */
            if (m_count == 0) {
                m_state = PCAP_INACTIVE;
                return;
            } 

            /* rewind and load the first packet */
            m_reader->Rewind();
            if (!m_reader->ReadPacket(m_raw_packet)) {
                m_state = PCAP_INACTIVE;
                return;
            }
        }

        /* update the packet dir if needed */
        update_pkt_dir();
      
    }


    inline void update_pkt_dir() {
        /* if dual mode and the interface is odd - swap the dir */
        if (is_dual()) {
            pkt_dir_t dir = (m_raw_packet->getInterface() & 0x1) ? (m_dir ^ 0x1) : m_dir;
            set_mbuf_dir(dir);
        }
    }

    /**
     * return the time for the next scheduling for a packet
     * 
     */
    inline double get_ipg() {
        assert(m_state != PCAP_INVALID);

        /* fixed IPG */
        if (m_ipg_sec != -1) {
            return m_ipg_sec;
        } else {
            return (std::max(m_min_ipg_sec, (m_raw_packet->get_time() - m_last_pkt_time) / m_speedup));
        }
    }

    /**
     * get the current packet as MBUF
     * 
     */
    inline rte_mbuf_t *get_pkt() {
        assert(m_state != PCAP_INVALID);

        rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc_local( get_socket_id(), m_raw_packet->getTotalLen());
        assert(m);

        char *p = rte_pktmbuf_append(m, m_raw_packet->getTotalLen());
        assert(p);

        /* copy the packet */
        memcpy(p, m_raw_packet->raw, m_raw_packet->getTotalLen());

        /* fix the MAC */
        if (get_mbuf_dir() == m_dir) {
            memcpy(p, m_mac_addr, 12);
        } else {
            memcpy(p, m_slave_mac_addr, 12);
        }
        

        return (m);
    }


    inline void handle(CFlowGenListPerThread *thread) {
        assert(m_state != PCAP_INVALID);
        thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);

        // read the next packet
        next();

        if (is_active()) {
            m_time += get_ipg();
            thread->m_node_gen.m_p_queue.push((CGenNode *)this);  
                            
        } else {
            TrexStatelessDpCore *stl_dp_core = (TrexStatelessDpCore *)thread->get_dp_core();
            stl_dp_core->stop_traffic(get_port_id(), 0, false, 0);
        }
    }

    void set_mbuf_dir(pkt_dir_t dir) {
        if (dir) {
            m_flags |=NODE_FLAGS_DIR;
        }else{
            m_flags &=~NODE_FLAGS_DIR;
        }
    }

    inline pkt_dir_t get_mbuf_dir(){
        return ((pkt_dir_t)( m_flags &1));
    }

    uint8_t get_port_id() {
        return m_port_id;
    }

    void mark_for_free() {
        m_state = PCAP_MARKED_FOR_FREE;
    }

    bool is_active() {
        return (m_state == PCAP_ACTIVE);
    }

    bool is_marked_for_free() {
        return (m_state == PCAP_MARKED_FOR_FREE);
    }

private:

    enum {
        PCAP_INVALID = 0,
        PCAP_ACTIVE,
        PCAP_INACTIVE,
        PCAP_MARKED_FOR_FREE
    };

    /* cache line 0 */
    /* important stuff here */
    uint8_t             m_mac_addr[12];
    uint8_t             m_slave_mac_addr[12];
    uint8_t             m_state;

    pkt_dir_t           m_dir;

    double              m_last_pkt_time;
    double              m_speedup;
    double              m_ipg_sec;
    double              m_min_ipg_sec;
    uint32_t            m_count;

    double              m_next_time_offset; /* in sec */

    CCapReaderBase      *m_reader;
    CCapPktRaw          *m_raw_packet;
    
    uint8_t             m_port_id;

    bool                m_is_dual;

    /* pad to match the size of CGenNode */
    uint8_t             m_pad_end[11];

    /* CACHE_LINE */
    uint64_t            m_pad3[8];

} __rte_cache_aligned;


static_assert(sizeof(CGenNodePCAP) == sizeof(CGenNode), "sizeof(CGenNodePCAP) != sizeof(CGenNode)" );

#endif /* __TREX_STL_STREAM_NODE_H__ */

