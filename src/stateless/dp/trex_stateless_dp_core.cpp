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
#include <trex_stateless_dp_core.h>
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>
#include <trex_stream_node.h>
#include <trex_stream.h>

#include <bp_sim.h>

static inline double
usec_to_sec(double usec) {
    return (usec / (1000 * 1000));
}



void CGenNodeStateless::free_stl_node(){
    /* if we have cache mbuf free it */
    rte_mbuf_t * m=get_cache_mbuf();
    if (m) {
        rte_pktmbuf_free(m);
        m_cache_mbuf=0;
    }
}



void
TrexStatelessDpCore::create(uint8_t thread_id, CFlowGenListPerThread *core) {
    m_thread_id = thread_id;
    m_core = core;

    CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

    m_ring_from_cp = cp_dp->getRingCpToDp(thread_id);
    m_ring_to_cp   = cp_dp->getRingDpToCp(thread_id);

    m_state = STATE_IDLE;
}

/**
 * in idle state loop, the processor most of the time sleeps 
 * and periodically checks for messages 
 * 
 * @author imarom (01-Nov-15)
 */
void 
TrexStatelessDpCore::idle_state_loop() {

    while (m_state == STATE_IDLE) {
        periodic_check_for_cp_messages();
        delay(200);
    }
}

/**
 * scehduler runs when traffic exists 
 * it will return when no more transmitting is done on this 
 * core 
 * 
 * @author imarom (01-Nov-15)
 */
void
TrexStatelessDpCore::start_scheduler() {
    /* creates a maintenace job using the scheduler */
    CGenNode * node_sync = m_core->create_node() ;
    node_sync->m_type = CGenNode::FLOW_SYNC;
    node_sync->m_time = m_core->m_cur_time_sec + SYNC_TIME_OUT;
    m_core->m_node_gen.add_node(node_sync);

    double old_offset = 0.0;
    m_core->m_node_gen.flush_file(-1, 0.0, false, m_core, old_offset);
    m_core->m_node_gen.close_file(m_core);
}


void 
TrexStatelessDpCore::run_once(){

    idle_state_loop();
    start_scheduler();
}


void
TrexStatelessDpCore::start() {

    while (true) {
        run_once();
    }
}

void
TrexStatelessDpCore::add_duration(double duration){
    if (duration > 0.0) {
        CGenNode *node = m_core->create_node() ;

        node->m_type = CGenNode::EXIT_SCHED;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + duration ;

        m_core->m_node_gen.add_node(node);
    }
}


void
TrexStatelessDpCore::add_cont_stream(TrexStream * stream,
                                     TrexStreamsCompiledObj *comp) {

    CGenNodeStateless *node = m_core->create_node_sl();

    /* add periodic */
    node->m_type = CGenNode::STATELESS_PKT;

    node->m_time = m_core->m_cur_time_sec + usec_to_sec(stream->m_isg_usec);

    pkt_dir_t dir = m_core->m_node_gen.m_v_if->port_id_to_dir(stream->m_port_id);
    node->m_flags = 0; 

    /* set socket id */
    node->set_socket_id(m_core->m_node_gen.m_socket_id);

    /* build a mbuf from a packet */
    
    uint16_t pkt_size = stream->m_pkt.len;
    const uint8_t *stream_pkt = stream->m_pkt.binary;

    node->m_stream_type = stream->m_type;
    node->m_next_time_offset =  1.0 / (stream->get_pps() * comp->get_multiplier()) ;


    /* stateless specific fields */
    switch ( stream->m_type ) {

    case TrexStream::stCONTINUOUS :
        break;

    case TrexStream::stSINGLE_BURST :
        node->m_stream_type             = TrexStream::stMULTI_BURST;
        node->m_single_burst            = stream->m_burst_total_pkts;
        node->m_single_burst_refill     = stream->m_burst_total_pkts;
        node->m_multi_bursts            = 1;  /* single burst in multi burst of 1 */
        node->m_ibg_sec                 = 0.0;
        break;

    case TrexStream::stMULTI_BURST :
        node->m_single_burst        = stream->m_burst_total_pkts;
        node->m_single_burst_refill = stream->m_burst_total_pkts;
        node->m_multi_bursts        = stream->m_num_bursts;
        node->m_ibg_sec             = usec_to_sec( stream->m_ibg_usec );
        break;
    default:

        assert(0);
    };

    node->m_is_stream_active = 1;
    node->m_port_id = stream->m_port_id;

    /* allocate const mbuf */
    rte_mbuf_t *m = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), pkt_size);
    assert(m);

    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* copy the packet */
    memcpy(p,stream_pkt,pkt_size);

    /* set dir 0 or 1 client or server */
    node->set_mbuf_cache_dir(dir);

    /* TBD repace the mac if req we should add flag  */
    m_core->m_node_gen.m_v_if->update_mac_addr_from_global_cfg(dir, m);

    /* set the packet as a readonly */
    node->set_cache_mbuf(m);

    /* keep track */
    m_active_nodes.push_back(node);

    /* schedule */
    m_core->m_node_gen.add_node((CGenNode *)node);

    m_state = TrexStatelessDpCore::STATE_TRANSMITTING;

}

void
TrexStatelessDpCore::start_traffic(TrexStreamsCompiledObj *obj, double duration) {
    for (auto single_stream : obj->get_objects()) {
        add_cont_stream(single_stream.m_stream,obj);
    }

    if ( duration > 0.0 ){
        add_duration( duration );
    }
}

void
TrexStatelessDpCore::stop_traffic(uint8_t port_id) {
    /* we cannot remove nodes not from the top of the queue so
       for every active node - make sure next time
       the scheduler invokes it, it will be free */
    for (auto node : m_active_nodes) {
        if (node->m_port_id == port_id) {
            node->m_is_stream_active = 0;
        }
    }

    /* remove all the non active nodes */
    auto pred = std::remove_if(m_active_nodes.begin(),
                               m_active_nodes.end(), 
                               [](CGenNodeStateless *node) { return (!node->m_is_stream_active); });

    m_active_nodes.erase(pred, m_active_nodes.end());

    if (m_active_nodes.size() == 0) {
        m_state = STATE_IDLE;
        /* stop the scheduler */

        CGenNode *node = m_core->create_node() ;

        node->m_type = CGenNode::EXIT_SCHED;

        /* make sure it will be scheduled after the current node */
        node->m_time = m_core->m_cur_time_sec + 0.0001;

        m_core->m_node_gen.add_node(node);
    }
    
}

/**
 * handle a message from CP to DP
 * 
 */
void 
TrexStatelessDpCore::handle_cp_msg(TrexStatelessCpToDpMsgBase *msg) {
    msg->handle(this);
    delete msg;
}

