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
#ifndef __TREX_STREAM_NODE_H__
#define __TREX_STREAM_NODE_H__

#include <bp_sim.h>
#include <stdio.h>

class TrexStatelessDpCore;

/* this is a event for stateless */
struct CGenNodeStateless : public CGenNodeBase  {
friend class TrexStatelessDpCore;

private:
    void *              m_cache_mbuf;

    double              m_next_time_offset;
    uint8_t             m_is_stream_active;
    uint8_t             m_port_id;

    /* pad to match the size of CGenNode */
    uint8_t             m_pad_end[87];


public:


    inline bool is_active() {
        return m_is_stream_active;
    }

        
    /**
     * main function to handle an event of a packet tx
     * 
     */
    inline void handle(CFlowGenListPerThread *thread) {

        thread->m_node_gen.m_v_if->send_node( (CGenNode *)this);

        /* in case of continues */
        m_time += m_next_time_offset;

        /* insert a new event */
        thread->m_node_gen.m_p_queue.push( (CGenNode *)this);
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
        if ( m_flags &NODE_FLAGS_MBUF_CACHE ) {
            return ((rte_mbuf_t *)m_cache_mbuf);
        }else{
            return ((rte_mbuf_t *)0);
        }
    }

    void free_stl_node();


    void Dump(FILE *fd){
        fprintf(fd," %f, %lu, %lu \n",m_time,(ulong)m_port_id,(ulong)get_mbuf_cache_dir());
    }

} __rte_cache_aligned;

static_assert(sizeof(CGenNodeStateless) == sizeof(CGenNode), "sizeof(CGenNodeStateless) != sizeof(CGenNode)");

#endif /* __TREX_STREAM_NODE_H__ */
