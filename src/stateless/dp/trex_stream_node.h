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
#ifndef __TREX_STREAM_NODE_H__
#define __TREX_STREAM_NODE_H__

#include <bp_sim.h>
#include <stdio.h>

class TrexStatelessDpCore;
#include <trex_stream.h>

class TrexStatelessCpToDpMsgBase;
class CFlowGenListPerThread;

struct CGenNodeCommand : public CGenNodeBase  {

friend class TrexStatelessDpCore;

public:
    TrexStatelessCpToDpMsgBase * m_cmd;

    uint8_t             m_pad_end[104];

public:
    void free_command();

} __rte_cache_aligned;;


static_assert(sizeof(CGenNodeCommand) == sizeof(CGenNode), "sizeof(CGenNodeCommand) != sizeof(CGenNode)" );


/* this is a event for stateless */
struct CGenNodeStateless : public CGenNodeBase  {
friend class TrexStatelessDpCore;

public:

    /* flags MASKS*/
    enum {
        SL_NODE_FLAGS_DIR                  =1, //USED by master
        SL_NODE_FLAGS_MBUF_CACHE           =2, //USED by master

        SL_NODE_CONST_MBUF                =4

    };

    enum {                                          
            ss_FREE_RESUSE =1, /* should be free by scheduler */
            ss_INACTIVE    =2, /* will be active by other stream or stopped */
            ss_ACTIVE      =3  /* the stream is active */ 
         };
    typedef uint8_t stream_state_t ;

    static std::string get_stream_state_str(stream_state_t stream_state);

private:
    /* cache line 0 */
    /* important stuff here */
    void *              m_cache_mbuf;

    double              m_next_time_offset; /* in sec */
    double              m_ibg_sec; /* inter burst time in sec */


    stream_state_t      m_state;
    uint8_t             m_port_id;
    uint8_t             m_stream_type; /* see TrexStream::STREAM_TYPE ,stream_type_t */
    uint8_t             m_pause;

    uint32_t            m_single_burst; /* the number of bursts in case of burst */
    uint32_t            m_single_burst_refill; 

    uint32_t            m_multi_bursts; /* in case of multi_burst how many bursts */

    /* cache line 1 */
    TrexStream *        m_ref_stream_info; /* the stream info */
    CGenNodeStateless  * m_next_stream;

    uint8_t *            m_original_packet_data_prefix; /* pointer to the original first pointer 64/128/512 */

    /* pad to match the size of CGenNode */
    uint8_t             m_pad_end[48];




public:

    uint8_t             get_port_id(){
        return (m_port_id);
    }


    /**
     * calculate the time offset based 
     * on the PPS and multiplier 
     * 
     */
    void update_rate(double factor) {
        /* update the inter packet gap */
        m_next_time_offset         =  m_next_time_offset / factor;
    }

    /* we restart the stream, schedule it using stream isg */
    inline void update_refresh_time(double cur_time){
        m_time = cur_time + usec_to_sec(m_ref_stream_info->m_isg_usec);
    }

    inline bool is_mask_for_free(){
        return (get_state() == CGenNodeStateless::ss_FREE_RESUSE ?true:false);

    }
    inline void mark_for_free(){
        set_state(CGenNodeStateless::ss_FREE_RESUSE);
        /* only to be safe */
        m_ref_stream_info= NULL;
        m_next_stream= NULL;
    }

    bool is_pause(){
        return (m_pause==1?true:false);
    }

    void set_pause(bool enable){
        if ( enable ){
            m_pause=1;
        }else{
            m_pause=0;
        }
    }

    inline uint8_t  get_stream_type(){
        return (m_stream_type);
    }

    inline uint32_t   get_single_burst_cnt(){
        return (m_single_burst);
    }

    inline double   get_multi_ibg_sec(){
        return (m_ibg_sec);
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

        if (unlikely (is_pause()==false)) {
            thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);
        }

        /* in case of continues */
        m_time += m_next_time_offset;

        /* insert a new event */
        thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
    }

    inline void handle_multi_burst(CFlowGenListPerThread *thread) {
        thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);

        m_single_burst--;
        if (m_single_burst > 0 ) {
            /* in case of continues */
            m_time += m_next_time_offset;

            thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
        }else{
            m_multi_bursts--;
            if ( m_multi_bursts == 0 ) {
                set_state(CGenNodeStateless::ss_INACTIVE);
                if ( thread->set_stateless_next_node(this,m_next_stream) ){
                    /* update the next stream time using isg */
                    m_next_stream->update_refresh_time(m_time);

                    thread->m_node_gen.m_p_queue.push( (CGenNode *)m_next_stream);
                }else{
                    // in case of zero we will schedule a command to stop 
                    // will be called from set_stateless_next_node
                }

            }else{
                m_time += m_ibg_sec;
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

    /* prefix header exits only in non cache mode size is 64/128/512  other are not possible right now */
    inline void alloc_prefix_header(uint8_t size){
         set_prefix_header_size(size);
         m_original_packet_data_prefix = (uint8_t *)malloc(size);
         assert(m_original_packet_data_prefix);
    }

    inline void free_prefix_header(){
         if (m_original_packet_data_prefix) {
             free(m_original_packet_data_prefix);
         }
    }

    /* prefix headr could be 64/128/512 */
    inline void set_prefix_header_size(uint16_t size){
        m_src_port=size;
    }

    inline uint16_t prefix_header_size(){
        return (m_src_port);
    }


    rte_mbuf_t   * alloc_node_with_vm();

    void free_stl_node();

public:
    /* debug functions */

    int get_stream_id();

    static void DumpHeader(FILE *fd);

    void Dump(FILE *fd);

} __rte_cache_aligned;

static_assert(sizeof(CGenNodeStateless) == sizeof(CGenNode), "sizeof(CGenNodeStateless) != sizeof(CGenNode)" );




#endif /* __TREX_STREAM_NODE_H__ */
